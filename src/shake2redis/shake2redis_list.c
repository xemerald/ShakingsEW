#define _GNU_SOURCE
/* Standard C header include */
#include <stdlib.h>
#include <string.h>
#include <search.h>
#include <time.h>
/* Earthworm environment header include */
#include <earthworm.h>
/* Local header include */
#include <dl_chain_list.h>
#include <shake2redis.h>
#include <shake2redis_list.h>

/* */
typedef struct {
	int     count;      /* Number of clients in the list */
	time_t  timestamp;  /* Time of the last time updated */
	void   *entry;      /* Pointer to first client       */
	void   *root;       /* Root of binary searching tree */
} StaList;

/* */
static StaList      *create_sta_list( void );
static void          destroy_sta_list( StaList * );
static int           compare_snl( const void *, const void * ); /* The compare function of binary tree search */
static int           compare_chan( const void *, const void * );
static STATION_PEAK *enrich_stapeak( STATION_PEAK *, const char *, const char *, const char * );
static CHAN_PEAK    *enrich_chapeak( CHAN_PEAK *, const char * );
static void          free_stapeak( void * );
static void          dummy_func( void * );

/* */
static StaList *SList = NULL;

/*
 *
 */
int sk2rd_list_init( void )
{
	if ( !SList ) {
		SList = create_sta_list();
		if ( !SList ) {
			logit("e", "shake2redis: Fatal! Station list memory initialized error!\n");
			return -1;
		}
	}

	return 0;
}

/*
 * sk2rd_list_search() - Insert the trace to the tree.
 * Arguments:
 *
 * Returns:
 *    NULL = Didn't find the station.
 *   !NULL = The Pointer to the station.
 */
STATION_PEAK *sk2rd_list_search( const char *sta, const char *net, const char *loc )
{
	STATION_PEAK *result = NULL;

/* Test if already in the tree */
	if ( (result = sk2rd_list_find( sta, net, loc )) == NULL ) {
		result = (STATION_PEAK *)calloc(1, sizeof(STATION_PEAK));
		enrich_stapeak( result, sta, net, loc );
	/* Insert the station information into binary tree */
		if ( dl_node_append( (DL_NODE **)&SList->entry, result ) == NULL ) {
			logit("e", "shake2redis: Error insert station into linked list!\n");
			goto except;
		}
		if ( tsearch(result, &SList->root, compare_snl) == NULL ) {
			logit("e", "shake2redis: Error insert station into binary tree!\n");
			goto except;
		}
	/* */
		SList->count++;
		time(&SList->timestamp);
	}

	return result;
/* Exception handle */
except:
	free(result);
	return NULL;
}

/*
 * sk2rd_list_find() - Output the Trace that match the SNL.
 * Arguments:
 *
 * Returns:
 *    NULL = Didn't find the trace.
 *   !NULL = The Pointer to the trace.
 */
STATION_PEAK *sk2rd_list_find( const char *sta, const char *net, const char *loc )
{
	STATION_PEAK  key;
	STATION_PEAK *result;

/* */
	memcpy(key.sta, sta, TRACE2_STA_LEN);
	memcpy(key.net, net, TRACE2_NET_LEN);
	memcpy(key.loc, loc, TRACE2_LOC_LEN);
/* Find which trace */
	if ( (result = tfind(&key, &SList->root, compare_snl)) == NULL ) {
	/* Not found in trace table */
		return NULL;
	}

	return *(STATION_PEAK **)result;
}

/*
 * sk2rd_list_walk() -
 *  Arguments:
 *
 *  Returns:
 *    None.
 */
void sk2rd_list_walk( void (*action)(const void *, const int, void *), void *arg )
{
	int      i;
	DL_NODE *current = NULL;

/* */
	for ( current = (DL_NODE *)SList->entry, i = 0; current != NULL; current = DL_NODE_GET_NEXT(current), i++ )
		action( DL_NODE_GET_DATA(current), i, arg );

	return;
}

/*
 *
 */
CHAN_PEAK *sk2rd_list_chlist_search( STATION_PEAK *stapeak, const char *chan, const int pvalue_i )
{
	CHAN_PEAK *result  = NULL;

/* Test if already in the list */
	if ( (result = sk2rd_list_chlist_find( stapeak, chan )) == NULL ) {
		result = (CHAN_PEAK *)calloc(1, sizeof(CHAN_PEAK));
		enrich_chapeak( result, chan );
	/* */
		if ( dl_node_append( (DL_NODE **)&stapeak->chlist[pvalue_i], result ) == NULL ) {
			logit("e", "shake2redis: Error insert channel into linked list!\n");
			goto except;
		}
		if ( tsearch(result, &stapeak->chlist_root, compare_chan) == NULL ) {
			logit("e", "shake2redis: Error insert channel into binary tree!\n");
			goto except;
		}
	}

	return result;

except:
	free(result);
	return NULL;
}

/*
 *
 */
CHAN_PEAK *sk2rd_list_chlist_find( const STATION_PEAK *stapeak, const char *chan )
{
	CHAN_PEAK  key;
	CHAN_PEAK *result = NULL;

/* */
	memcpy(key.chan, chan, TRACE2_CHAN_LEN);
/* Find which trace */
	if ( (result = tfind(&key, &stapeak->chlist_root, compare_chan)) == NULL ) {
	/* Not found in trace table */
		return NULL;
	}

	return *(CHAN_PEAK **)result;
}

/*
 *
 */
void sk2rd_list_chlist_delete( STATION_PEAK *stapeak, const char *chan, const int pvalue_i )
{
	CHAN_PEAK *target  = sk2rd_list_chlist_find( stapeak, chan );
	CHAN_PEAK *chapeak = NULL;
	DL_NODE   *current = NULL;
	DL_NODE   *next    = NULL;

/* */
	if ( target ) {
	/* Delete it from the tree */
		tdelete(target, &stapeak->chlist_root, compare_chan);
	/* Then, delete it from the linked list */
		for ( current = stapeak->chlist[pvalue_i]; current != NULL; current = DL_NODE_GET_NEXT( current ) ) {
			chapeak = (CHAN_PEAK *)DL_NODE_GET_DATA( current );
		/* */
			if ( !strcmp( chapeak->chan, chan ) ) {
				next = dl_node_delete( current, NULL );
			/* If we deleted the first element of the list, we should reassign the next element to the head */
				if ( current == stapeak->chlist[pvalue_i] )
					stapeak->chlist[pvalue_i] = next;
			/* */
				break;
			}
		}
	/* Real free the memory space of the channel */
		free(target);
	}

	return;
}

/*
 * sk2rd_list_end() - End process of stations' list.
 * Arguments:
 *   None.
 * Returns:
 *   None.
 */
void sk2rd_list_end( void )
{
	destroy_sta_list( SList );
	SList = NULL;

	return;
}

/*
 * sk2rd_list_total_sta_get() -
 */
int sk2rd_list_total_sta_get( void )
{
	return SList->count;
}

/*
 * create_sta_list() -
 */
static StaList *create_sta_list( void )
{
	StaList *result = (StaList *)calloc(1, sizeof(StaList));

	if ( result ) {
		result->count     = 0;
		result->timestamp = time(NULL);
		result->entry     = NULL;
		result->root      = NULL;
	}

	return result;
}

/*
 * destroy_sta_list() -
 */
static void destroy_sta_list( StaList *list )
{
	if ( list != (StaList *)NULL ) {
	/* */
		tdestroy(list->root, dummy_func);
		dl_list_destroy( (DL_NODE **)&list->entry, free_stapeak );
		free(list);
	}

	return;
}

/*
 * compare_snl() - The SNL compare function of binary tree search
 */
static int compare_snl( const void *a, const void *b )
{
	int rc;
	STATION_PEAK *tmpa = (STATION_PEAK *)a;
	STATION_PEAK *tmpb = (STATION_PEAK *)b;

	if ( (rc = strcmp( tmpa->sta, tmpb->sta )) != 0 )
		return rc;
	if ( (rc = strcmp( tmpa->net, tmpb->net )) != 0 )
		return rc;
	return strcmp( tmpa->loc, tmpb->loc );
}

/*
 * compare_chan() - The channel code compare function of binary tree search
 */
static int compare_chan( const void *a, const void *b )
{
	CHAN_PEAK *tmpa = (CHAN_PEAK *)a;
	CHAN_PEAK *tmpb = (CHAN_PEAK *)b;

	return strcmp( tmpa->chan, tmpb->chan );
}

/*
 *
 */
static STATION_PEAK *enrich_stapeak( STATION_PEAK *stapeak, const char *sta, const char *net, const char *loc )
{
	int i;
/* */
	memcpy(stapeak->sta, sta, TRACE2_STA_LEN);
	memcpy(stapeak->net, net, TRACE2_NET_LEN);
	memcpy(stapeak->loc, loc, TRACE2_LOC_LEN);
/* */
	stapeak->chlist_root = NULL;
	for ( i = 0; i < MAX_TYPE_PEAKVALUE; i++ ) {
		stapeak->chlist[i] = NULL;
		stapeak->pvalue[i] = NULL_PEAKVALUE;
		stapeak->ptime[i]  = NULL_PEAKVALUE;
	}

	return stapeak;
}

/*
 *
 */
static CHAN_PEAK *enrich_chapeak( CHAN_PEAK *chapeak, const char *chan )
{
/* */
	memcpy(chapeak->chan, chan, TRACE2_CHAN_LEN);
	chapeak->pvalue = NULL_PEAKVALUE;
	chapeak->ptime  = NULL_PEAKVALUE;
	chapeak->match  = NULL;

	return chapeak;
}

/*
 * free_stapeak() -
 */
static void free_stapeak( void *node )
{
	int           i;
	STATION_PEAK *stapeak = (STATION_PEAK *)node;

	tdestroy(stapeak->chlist_root, dummy_func);
	for ( i = 0; i < MAX_TYPE_PEAKVALUE; i++ )
		dl_list_destroy( (DL_NODE **)&stapeak->chlist[i], free );

	free(stapeak);
	return;
}

/*
 * dummy_func() -
 */
static void dummy_func( void *node )
{
	return;
}
