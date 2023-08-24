/* Standard C header include */
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
/* */
#include <earthworm.h>
/* Local header include */
#include <dl_chain_list.h>
#include <tracepeak.h>
#include <triglist.h>
#include <geogfunc.h>
#include <peak2trig.h>
#include <peak2trig_misc.h>
#include <peak2trig_list.h>
#include <peak2trig_triglist.h>

/* */
typedef struct {
	int      count;      /* Number of clients in the list */
	DL_NODE *entry;      /* Pointer to first client       */
	DL_NODE *tail;       /* Pointer to last client        */
} _TrigList;

/* Local function prototype */
static int    is_station_clustered( const TRIG_STA * );
static void   reset_station_cluster( TRIG_STA * );
static double get_station_dist( const void *, const void * );
/* */
static _TrigList PeakPool = { 0, NULL, NULL };

/*
 * pk2trig_tlist_search() - Search the key in the linked list, when there
 *                            is not, insert it.
 * Arguments:
 *   key   = Key to search.
 * Returns:
 *    NULL = Something error.
 *   !NULL = The pointer of the key.
 */
TRIG_STA *pk2trig_tlist_search( const TRACE_PEAKVALUE *key )
{
	TRIG_STA *tsta = NULL;
	_STAINFO *staptr = NULL;

/* */
	if ( (staptr = peak2trig_list_find( key )) != NULL && (tsta = pk2trig_tlist_find( key )) == NULL ) {
	/* */
		if ( (tsta = calloc(1, sizeof(TRIG_STA))) != NULL ) {
			tsta->staptr = staptr;
			PeakPool.tail = dl_node_append( &PeakPool.entry, tsta );
			PeakPool.count++;
		}
		else {
			logit("e", "peak2trig: Error insert new %s.%s.%s into trigger list.\n", staptr->sta, staptr->net, staptr->loc);
		}
	}

	return tsta != NULL ? pk2trig_tlist_update( tsta, key ) : NULL;
}

/*
 * pk2trig_tlist_find() - Search the key in the linked list, when there
 *                          is not, insert it.
 * Arguments:
 *   key   = Key to find.
 * Returns:
 *    NULL = No result or something error.
 *   !NULL = The pointer of the key.
 */
TRIG_STA *pk2trig_tlist_find( const TRACE_PEAKVALUE *tpv )
{
	DL_NODE  *current = NULL;
	TRIG_STA *tsta = NULL;
	_STAINFO  key;

/* */
	memcpy(key.sta, tpv->sta, TRACE2_STA_LEN);
	memcpy(key.net, tpv->net, TRACE2_NET_LEN);
	memcpy(key.loc, tpv->loc, TRACE2_LOC_LEN);
/* */
	DL_LIST_FOR_EACH_DATA(PeakPool.entry, current, tsta) {
		if ( pk2trig_misc_snl_compare( tsta->staptr, &key ) == 0 )
			return tsta;
	}

	return NULL;
}

/*
 * pk2trig_tlist_delete() - Search the key in the linked list, when there
 *                            is not, insert it.
 * Arguments:
 *   key   = Key to delete.
 *   first = First pointer of the linked list.
 * Returns:
 *   None.
 */
TRIG_STA *pk2trig_tlist_delete( const TRACE_PEAKVALUE *tpv )
{
	DL_NODE  *current = NULL;
	TRIG_STA *tsta = NULL;
	_STAINFO  key;

/* */
	memcpy(key.sta, tpv->sta, TRACE2_STA_LEN);
	memcpy(key.net, tpv->net, TRACE2_NET_LEN);
	memcpy(key.loc, tpv->loc, TRACE2_LOC_LEN);
/* */
	DL_LIST_FOR_EACH_DATA(PeakPool.entry, current, tsta) {
		if ( pk2trig_misc_snl_compare( tsta->staptr, &key ) == 0 ) {
			current = dl_node_delete( current, free );
			if ( current->prev == NULL )
				PeakPool.entry = current;
			if ( current->next == NULL )
				PeakPool.tail = current;
			PeakPool.count--;
			break;
		}
	}

/* */
	return tsta;
}

/*
 * peak2trig_tlist_update() - Search the key in the linked list, when there
 *                            is not, insert it.
 * Arguments:
 *   target = Target pointer of the triggered station.
 *   new    = New peak value.
 * Returns:
 *   None.
 */
TRIG_STA *pk2trig_tlist_update( TRIG_STA *target, const TRACE_PEAKVALUE *new )
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
int pk2trig_tlist_len_get( void )
{
	return PeakPool.count;
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
int pk2trig_tlist_cluster( const double dist, const double sec )
{
	int       i, j;
	int       result = 0;
	DL_NODE  *current1, *current2;
	TRIG_STA *node1, *node2;

/* */
	if ( PeakPool.entry == NULL )
		return 0;
/* */
	pk2trig_tlist_walk( reset_station_cluster );
/* Go thru all the stations in the triggered list now */
	DL_LIST_FOR_EACH_DATA( PeakPool.entry, current1, node1 ) {
	/* Find out is there any empty space in node1 to store the clustered station */
		for ( i = 0; i < CLUSTER_NUM; i++ )
			if ( node1->cluster[i] == NULL )
				break;
	/* */
		if ( i < (int)CLUSTER_NUM ) {
		/* Go thru all the stations again except the previous part, for clustering */
			DL_LIST_FOR_EACH_DATA( PeakPool.entry, current2, node2 ) {
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
					/* These two stations peak occurrence time difference is less than assuming seconds */
						fabs(node1->peaktime - node2->peaktime) <= sec &&
					/* These two stations' distance is less than assuming distance(km) */
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
			}
		}
	/* Once the station meet the threshold for cluster, add one to the triggered number */
		if ( i >= (int)CLUSTER_NUM )
			result++;
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
void pk2trig_tlist_walk( void (*action)( TRIG_STA * ) )
{
	DL_NODE  *current = NULL;
	TRIG_STA *tsta = NULL;

/* */
	if ( action == NULL )
		return;
/* */
	DL_LIST_FOR_EACH_DATA( PeakPool.entry, current, tsta ) {
		action( tsta );
	}

	return;
}

/*
 * peak2trig_tlist_time_filter() - Search the key in the linked list, when there
 *                                 is not, insert it.
 * Arguments:
 *   sec = Key to delete.
 * Returns:
 *   None.
 */
void pk2trig_tlist_time_filter( const double sec )
{
	time_t    timenow;
	DL_NODE  *current = NULL;
	DL_NODE  *safe = NULL;
	TRIG_STA *tsta = NULL;

/* */
	time(&timenow);
/* */
	DL_LIST_FOR_EACH_DATA_SAFE( PeakPool.entry, current, tsta, safe ) {
		if ( fabs((double)timenow - tsta->peaktime) > sec ) {
			current = dl_node_delete( current, free );
			if ( current->prev == NULL )
				PeakPool.entry = current;
			if ( current->next == NULL )
				PeakPool.tail = current;
			PeakPool.count--;
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
int pk2trig_tlist_pack( TrigListPacket *tlbuffer, const int max_tl_sta )
{
	TRIGLIST_STATION *tl_sta  = (TRIGLIST_STATION *)(&tlbuffer->tlh + 1);
	DL_NODE      *current = NULL;
	TRIG_STA     *_tsta   = NULL;
	int           ntsta   = 0;

/* */
	if ( PeakPool.entry == NULL )
		return -1;
/* */
	DL_LIST_FOR_EACH_DATA( PeakPool.entry, current, _tsta ) {
	/* */
		if ( is_station_clustered( _tsta ) ) {
		/* */
			memcpy(tl_sta->sta, _tsta->staptr->sta, TRACE2_STA_LEN);
			memcpy(tl_sta->net, _tsta->staptr->net, TRACE2_NET_LEN);
			memcpy(tl_sta->loc, _tsta->staptr->loc, TRACE2_LOC_LEN);
			memcpy(tl_sta->chan, _tsta->peakchan, TRACE2_CHAN_LEN);
			tl_sta->latitude   = _tsta->staptr->latitude;
			tl_sta->longitude  = _tsta->staptr->longitude;
			tl_sta->elevation  = _tsta->staptr->elevation;
			tl_sta->ptime      = _tsta->peaktime;
			tl_sta->pvalue     = _tsta->peakvalue;
			tl_sta->flag[0]    = 0;
			tl_sta->flag[1]    = 0;
			tl_sta->update_seq = 0;
			tl_sta->datatype   = _tsta->recordtype;
			tl_sta->extra_size = 0;
			tl_sta->next_pos   = (uint8_t *)(tl_sta + 1) - tlbuffer->msg;
		/* */
			reset_station_cluster( _tsta );
		/* Next triggered station storage pointer */
			tl_sta++;
		/* */
			if ( ++ntsta >= max_tl_sta )
				return max_tl_sta;
		}
	}

	return ntsta;
}


/*
 * peak2trig_tlist_destroy() - Search the key in the linked list, when there
 *                             is not, insert it.
 * Arguments:
 *   first = First pointer of the linked list.
 * Returns:
 *   None.
 */
void pk2trig_tlist_destroy( void )
{
/* */
	dl_list_destroy( &PeakPool.entry, free );
/* */
	PeakPool.entry = PeakPool.tail = NULL;
	PeakPool.count = 0;

	return;
}

/*
 *
 */
static int is_station_clustered( const TRIG_STA *tsta )
{
	for ( int i = 0; i < CLUSTER_NUM; i++ )
		if ( tsta->cluster[i] == NULL )
			return 0;

	return 1;
}

/*
 *
 */
static void reset_station_cluster( TRIG_STA *tsta )
{
	for ( int i = 0; i < CLUSTER_NUM; i++ )
		tsta->cluster[i] = NULL;

	return;
}

/*
 *
 */
static double get_station_dist( const void *node1, const void *node2 )
{
	const _STAINFO *sta1 = ((TRIG_STA *)node1)->staptr;
	const _STAINFO *sta2 = ((TRIG_STA *)node2)->staptr;

	return coor2distf( sta1->latitude, sta1->longitude, sta2->latitude, sta2->longitude );
}
