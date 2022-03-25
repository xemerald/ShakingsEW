/* Standard C header include */
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <float.h>
/* Local header include */
#include <tracepeak.h>
#include <triglist.h>
#include <geogfunc.h>
#include <peak2trig.h>
#include <peak2trig_misc.h>
#include <peak2trig_triglist.h>
/* Local function prototype */
static void   reset_cluster( const void * );
static double get_station_dist( const void *, const void * );
/* */
static STA_NODE *Head = NULL;      /* First pointer of the linked list. */
static STA_NODE *Tail = NULL;      /* Last pointer of the linked list. */
static uint32_t  ListLength = 0;   /* Length of the linked list. */

/*
 * peak2trig_tlist_insert() - Search the key in the linked list, when there
 *                            is not, insert it.
 * Arguments:
 *   key   = Key to search.
 *   first = First pointer of the linked list.
 * Returns:
 *    NULL = Something error.
 *   !NULL = The pointer of the key.
 */
STA_NODE *peak2trig_tlist_insert( const _STAINFO *key )
{
	STA_NODE  *new;
	STA_NODE **current = &Head;

/* */
	if ( current == NULL )
		return NULL;
/* */
	while ( *current != NULL ) {
		if ( peak2trig_misc_snl_compare( (*current)->staptr, key ) == 0 )
			return *current;
		current = &(*current)->next;
	}
/* */
	new = calloc(1, sizeof(STA_NODE));
	if ( new != NULL ) {
		*current = new;
		new->staptr = (_STAINFO *)key;
		new->next   = NULL;
		ListLength++;
	}
/* */
	if ( new->next == NULL )
		Tail = new;

	return new;
}

/*
 * peak2trig_tlist_find() - Search the key in the linked list, when there
 *                          is not, insert it.
 * Arguments:
 *   key   = Key to find.
 *   first = First pointer of the linked list.
 * Returns:
 *    NULL = No result or something error.
 *   !NULL = The pointer of the key.
 */
STA_NODE *peak2trig_tlist_find( const _STAINFO *key )
{
	STA_NODE **current = &Head;

/* */
	if ( current == NULL )
		return NULL;
/* */
	while ( *current != NULL ) {
		if ( peak2trig_misc_snl_compare( (*current)->staptr, key ) == 0 )
			return *current;
		current = &(*current)->next;
	}

	return NULL;
}

/*
 * peak2trig_tlist_delete() - Search the key in the linked list, when there
 *                            is not, insert it.
 * Arguments:
 *   key   = Key to delete.
 *   first = First pointer of the linked list.
 * Returns:
 *   None.
 */
STA_NODE *peak2trig_tlist_delete( const _STAINFO *key )
{
	STA_NODE **current = &Head;
	STA_NODE *prev = (STA_NODE *)1;
	STA_NODE *next;

/* */
	if ( current == NULL && *current == NULL )
		return NULL;
/* */
	while ( peak2trig_misc_snl_compare( (*current)->staptr, key ) != 0 ) {
		prev    = *current;
		current = &(*current)->next;
		if ( *current == NULL )
			return NULL;
	}
/* */
	next = (*current)->next;
	free(*current);
	*current = next;
/* */
	ListLength--;

	return prev;
}

/*
 * peak2trig_tlist_update() - Search the key in the linked list, when there
 *                            is not, insert it.
 * Arguments:
 *   new    = Key to delete.
 *   target = First pointer of the linked list.
 * Returns:
 *   None.
 */
STA_NODE *peak2trig_tlist_update( const TRACE_PEAKVALUE *new, STA_NODE *target )
{
	if ( new->peaktime > target->peaktime ) {
		memcpy(target->peakchan, new->chan, TRACE2_CHAN_LEN);
		target->recordtype = new->recordtype;
		target->peakvalue  = new->peakvalue;
		target->peaktime   = new->peaktime;
	}

	return target;
}

/*
 * peak2trig_tlist_len_get() - Search the key in the linked list, when there
 *                              is not, insert it.
 * Arguments:
 *   None.
 * Returns:
 *   >= 0 = The length of the trigger list.
 */
int peak2trig_tlist_len_get( void )
{
	int       count   = 0;
	STA_NODE *current = Head;

/* */
	while ( current != NULL ) {
		count++;
		current = current->next;
	}

	return count;
}

/*
 * peak2trig_tlist_cluster() - Search the key in the linked list, when there
 *                             is not, insert it.
 * Arguments:
 *   dist = Those two stations' distance should be less than this
 *          distance(km).
 *   sec  = Those two stations peak occurrence time difference should
 *          be less than this seconds.
 * Returns:
 *    0   = There is not any clustered station.
 *   >0   = There are such number of clustered stations.
 */
int peak2trig_tlist_cluster( const double dist, const double sec )
{
	int       i, j;
	int       result = 0;
	STA_NODE *node1  = Head;
	STA_NODE *node2  = NULL;

/* */
	if ( node1 == NULL )
		return 0;
/* */
	peak2trig_tlist_walk( reset_cluster );
/* Go thru all the stations in the triggered list now */
	while ( node1 != NULL ) {
	/* Find out is there any empty space in node1 to store the clustered station */
		for ( i = 0; i < CLUSTER_NUM; i++ )
			if ( node1->cluster[i] == NULL )
				break;
	/* */
		if ( i < (int)CLUSTER_NUM ) {
		/* Go thru all the stations again except the previous part, for clustering */
			node2 = Head;
			while ( node2 != NULL ) {
			/* Skip the same station */
				if ( node2 != node1 ) {
				/* Find out is there any empty space in node2 to store the clustered station */
					for ( j = 0; j < CLUSTER_NUM; j++ ) {
						if ( node2->cluster[j] == NULL )
							break;
					/* Other, if there is already node1 in the cluster list of node2, should skip this node2 */
						else if ( node2->cluster[j] == node1 )
							j = CLUSTER_NUM;
					}
				/* Check if these two stations is fitting the criteria or not: */
					if (
						j <= CLUSTER_NUM &&
					/* These two stations peak occurrence time difference is less than assume seconds */
						fabs(node1->peaktime - node2->peaktime) <= sec &&
					/* These two stations' distance is less than assume distance(km) */
						get_station_dist( node1, node2 ) <= dist
					) {
						node1->cluster[i] = node2;
						if ( j < CLUSTER_NUM )
							node2->cluster[j] = node1;
					/*
					 * Once the station pair meet the criteria, add one to the cluster number
					 * and jump out the loop.
					 */
						if ( ++i >= (int)CLUSTER_NUM )
							break;
					}
				}
			/* Next node */
				node2 = node2->next;
			}
		}
	/* Once the station meet the threshold for cluster, add one to the triggered number */
		if ( i >= (int)CLUSTER_NUM )
			result++;
	/* Next node */
		node1 = node1->next;
	}

	return result;
}

/*
 * peak2trig_tlist_walk() - Search the key in the linked list, when there *
 *                          is not, insert it.
 * Arguments:
 *   first = First pointer of the linked list.
 *   action =
 * Returns:
 *   None.
 */
void peak2trig_tlist_walk( void (*action)( const void * ) )
{
	STA_NODE *current = Head;

/* */
	if ( action == NULL )
		return;
/* */
	while ( current != NULL ) {
		(*action)( current );
		current = current->next;
	}

	return;
}

/*
 * peak2trig_tlist_time_filter() - Search the key in the linked list, when there
 *                                 is not, insert it.
 * Arguments:
 *   key   = Key to delete.
 *   first = First pointer of the linked list.
 * Returns:
 *   None.
 */
void peak2trig_tlist_time_filter( const double sec )
{
	STA_NODE **current = &Head;
	STA_NODE  *next;
	time_t     timenow;

/* */
	if ( current == NULL )
		return;
/* */
	time(&timenow);
/* */
	while ( *current != NULL ) {
		if ( fabs((double)timenow - (*current)->peaktime) > sec ) {
			next = (*current)->next;
			free(*current);
			*current = next;

			ListLength--;
			if ( next == NULL )
				Tail = NULL;
		}
		else {
			current = &(*current)->next;
		}
	}

	return;
}

/*
 * peak2trig_tlist_pack() - Search the key in the linked list, when there
 *                          is not, insert it.
 * Arguments:
 *   key   = Key to delete.
 *   first = First pointer of the linked list.
 * Returns:
 *   None.
 */
int peak2trig_tlist_pack( void *msg, size_t maxsize, const uint8_t trig_flag )
{
	int i;

	STA_NODE            *current = Head;
	STA_NODE            *posepic = NULL;   /* Pointer to the possible epicenter station */
	TRIGLIST_HEADER     *tlh     = (TRIGLIST_HEADER *)msg;
	TRIG_STATION        *tsta    = (TRIG_STATION *)(tlh + 1);
	TRIG_STATION * const msgend  = (TRIG_STATION *)((uint8_t *)msg + maxsize);

	uint32_t trigstations = 0;
	time_t   timenow;

/* */
	if ( current == NULL || tlh == NULL )
		return -1;
/* Mark trigger generated time */
	time(&timenow);
	tlh->trigtime = (double)timenow;
/* */
	while ( current != NULL ) {
		for ( i = 0; i < CLUSTER_NUM; i++ )
			if ( current->cluster[i] == NULL )
				break;
	/* */
		if ( i >= (int)CLUSTER_NUM ) {
			if ( (tsta + 1) >= msgend )
				return -2;
			if ( posepic == NULL )
				posepic = current;

			memcpy(tsta->sta, current->staptr->sta, TRACE2_STA_LEN);
			memcpy(tsta->net, current->staptr->net, TRACE2_NET_LEN);
			memcpy(tsta->loc, current->staptr->loc, TRACE2_LOC_LEN);
			memcpy(tsta->pchan, current->peakchan, TRACE2_CHAN_LEN);
			tsta->seq      = trigstations++;
			tsta->datatype = current->recordtype;
			tsta->ptime    = current->peaktime;
			tsta->pvalue   = current->peakvalue;
		/* Possible epicenter station */
			if ( current->peaktime < posepic->peaktime )
				posepic = current;
		/* Next triggered station storage pointer */
			tsta++;
		}
		else if ( posepic == NULL ) {
			posepic = current;
		}

		current = current->next;
	}

/* Check if the first time trigger */
	if ( trig_flag == PEAK2TRIG_FIRST_TRIG ) {
	/* Possible origin time */
		tlh->origintime = posepic->peaktime;
	/* Possible epicenter */
		tlh->latitude   = posepic->staptr->latitude;
		tlh->longitude  = posepic->staptr->longitude;
		tlh->depth      = 0.0;
	}
/* */
	tlh->trigstations = trigstations;

	return TRIGLIST_SIZE_GET( tlh );
}

/*
 * peak2trig_tlist_destroy() - Search the key in the linked list, when there
 *                             is not, insert it.
 * Arguments:
 *   first = First pointer of the linked list.
 * Returns:
 *   None.
 */
void peak2trig_tlist_destroy( void )
{
	STA_NODE *current = Head;
	STA_NODE *next;

/* */
	while ( current != NULL ) {
		next = current->next;
		free(current);
		current = next;
	}
/* */
	Head = Tail = NULL;
	ListLength = 0;

	return;
}

/*
 *
 */
static void reset_cluster( const void *node )
{
	int       i;
	STA_NODE *stanode = (STA_NODE *)node;

	for ( i = 0; i < CLUSTER_NUM; i++ )
		stanode->cluster[i] = NULL;

	return;
}

/*
 *
 */
static double get_station_dist( const void *node1, const void *node2 )
{
	const _STAINFO *sta1 = ((STA_NODE *)node1)->staptr;
	const _STAINFO *sta2 = ((STA_NODE *)node2)->staptr;

	return coor2distf( sta1->latitude, sta1->longitude, sta2->latitude, sta2->longitude );
}
