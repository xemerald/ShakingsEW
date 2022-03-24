#define _GNU_SOURCE
/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <search.h>
#include <time.h>
#include <ctype.h>
/* Earthworm environment header include */
#include <earthworm.h>
/* Local header include */
#include <dblist.h>
#include <dl_chain_list.h>
#include <recordtype.h>
#include <cf2trace.h>
#include <cf2trace_list.h>
/* */
typedef struct {
	int     count;      /* Number of clients in the list */
	time_t  timestamp;  /* Time of the last time updated */
	void   *entry;      /* Pointer to first client       */
	void   *root;       /* Root of binary searching tree */
	void   *root_t;     /* Temporary root of binary searching tree */
} TraceList;

/* */
static int         fetch_list_sql( TraceList *, const char *, const DBINFO *, const int );
static TraceList  *init_trace_list( void );
static void        destroy_trace_list( TraceList * );
static int         compare_scnl( const void *, const void * );
static void        dummy_func( void * );
static _TRACEINFO *update_traceinfo( _TRACEINFO *, const _TRACEINFO * );
static _TRACEINFO *append_traceinfo_list( TraceList *, _TRACEINFO *, const int );
static _TRACEINFO *create_new_traceinfo(
	const char *, const char *, const char *, const char *, const uint8_t, const float
);
static _TRACEINFO *enrich_traceinfo_raw(
	_TRACEINFO *, const char *, const char *, const char *, const char *, const uint8_t, const float
);
/* */
#if defined( _USE_SQL )
static void extract_traceinfo_mysql(
	char *, char *, char *, char *, uint8_t *, float *, const MYSQL_ROW, const unsigned long []
);
#endif
/* */
static TraceList *TList = NULL;

/*
 * cf2tra_list_db_fetch() -
 */
int cf2tra_list_db_fetch( const char *table_chan, const DBINFO *dbinfo, const int update )
{
	if ( !TList ) {
		TList = init_trace_list();
		if ( !TList ) {
			logit("e", "cf2trace: Fatal! Trace list memory initialized error!\n");
			return -3;
		}
	}

	if ( strlen(dbinfo->host) > 0 && strlen(table_chan) > 0 )
		return fetch_list_sql( TList, table_chan, dbinfo, update );
	else
		return 0;
}

/*
 *
 */
int cf2tra_list_chan_line_parse( const char *line, const int update )
{
	int     result = 0;
	char    sta[TRACE2_STA_LEN]   = { 0 };
	char    net[TRACE2_NET_LEN]   = { 0 };
	char    loc[TRACE2_LOC_LEN]   = { 0 };
	char    chan[TRACE2_CHAN_LEN] = { 0 };
	char    typestr[TYPE_STR_LEN] = { 0 };
	uint8_t rtype = 0;
	float   cfactor;

/* */
	if ( !TList ) {
		TList = init_trace_list();
		if ( !TList ) {
			logit("e", "cf2trace: Fatal! Trace list memory initialized error!\n");
			return -3;
		}
	}
/* */
	if ( sscanf(line, "%s %s %s %s %s %f", sta, net, loc, chan, typestr, &cfactor) >= 6 ) {
	/* */
		rtype = typestr2num( typestr );
		if ( append_traceinfo_list( TList, create_new_traceinfo( sta, net, loc, chan, rtype, cfactor ), update ) == NULL )
			result = -2;
	}
	else {
		logit("e", "cf2trace: ERROR, lack of some trace information in local list!\n");
		result = -1;
	}

	return result;
}

/*
 * cf2tra_list_end() -
 */
void cf2tra_list_end( void )
{
	destroy_trace_list( TList );
	TList = NULL;

	return;
}

/*
 * cf2tra_list_find() -
 */
_TRACEINFO *cf2tra_list_find( const TRACE2X_HEADER *trh2x )
{
	_TRACEINFO *result = NULL;
	_TRACEINFO  key;

/* */
	memcpy(key.sta, trh2x->sta, TRACE2_STA_LEN);
	memcpy(key.net, trh2x->net, TRACE2_NET_LEN);
	memcpy(key.loc, trh2x->loc, TRACE2_LOC_LEN);
	memcpy(key.chan, trh2x->chan, TRACE2_LOC_LEN);
/* Find which station */
	if ( (result = tfind(&key, &TList->root, compare_scnl)) != NULL ) {
	/* Found in the main Palert table */
		result = *(_TRACEINFO **)result;
	}

	return result;
}

/*
 * cf2tra_list_tree_activate() -
 */
void cf2tra_list_tree_activate( void )
{
	void *_root = TList->root;

	TList->root      = TList->root_t;
	TList->root_t    = NULL;
	TList->timestamp = time(NULL);

	if ( _root ) {
		sleep_ew(1000);
		tdestroy(_root, dummy_func);
	}

	return;
}

/*
 * cf2tra_list_tree_activate() -
 */
void cf2tra_list_tree_abandon( void )
{
	if ( TList->root_t )
		tdestroy(TList->root_t, dummy_func);

	TList->root_t = NULL;

	return;
}

/*
 * cf2tra_list_total_channel_get() -
 */
int cf2tra_list_total_channel_get( void )
{
	DL_NODE *current = NULL;
	int      result  = 0;

/* */
	for ( current = (DL_NODE *)TList->entry; current != NULL; current = DL_NODE_GET_NEXT(current) )
		result++;
/* */
	TList->count = result;

	return result;
}

/*
 * cf2tra_list_timestamp_get() -
 */
time_t cf2tra_list_timestamp_get( void )
{
	return TList->timestamp;
}

/*
 *
 */
#if defined( _USE_SQL )
/*
 * fetch_list_sql() - Get stations list from MySQL server
 */
static int fetch_list_sql( TraceList *list, const char *table_chan, const DBINFO *dbinfo, const int update )
{
	int     result = 0;
	char    sta[TRACE2_STA_LEN] = { 0 };
	char    net[TRACE2_NET_LEN] = { 0 };
	char    loc[TRACE2_LOC_LEN] = { 0 };
	char    chan[TRACE2_CHAN_LEN] = { 0 };
	uint8_t rtype;
	float   cfactor;

	MYSQL_RES *sql_res = NULL;
	MYSQL_ROW  sql_row;

/* Connect to database */
	printf("cf2trace: Querying the channels information from MySQL server %s...\n", dbinfo->host);
	sql_res = dblist_chan_query_sql(
		dbinfo, table_chan, CF2TRA_INFO_FROM_SQL,
		COL_CHAN_STATION, COL_CHAN_NETWORK, COL_CHAN_LOCATION,
		COL_CHAN_CHANNEL, COL_CHAN_RECORD, COL_CHAN_CFACTOR
	);
	if ( sql_res == NULL )
		return -1;
	printf("cf2trace: Queried the channels information success!\n");

/* Start the SQL server connection for channel */
	dblist_start_persistent_sql( dbinfo );
/* Read station list from query result */
	while ( (sql_row = dblist_fetch_row_sql( sql_res )) != NULL ) {
	/* */
		extract_traceinfo_mysql(
			sta, net, loc, chan, &rtype, &cfactor,
			sql_row, dblist_fetch_lengths_sql( sql_res )
		);
	/* */
		if (
			append_traceinfo_list(
				list, create_new_traceinfo( sta, net, loc, chan, rtype, cfactor ), update
			) != NULL
		) {
			result++;
		}
		else {
			result = -2;
			break;
		}
	}
/* Close the connection for channel */
	dblist_close_persistent_sql();
	dblist_free_result_sql( sql_res );
	dblist_end_thread_sql();

	if ( result > 0 )
		logit("o", "cf2trace: Read %d channels information from MySQL server success!\n", result);
	else
		logit("e", "cf2trace: Some errors happened when fetching channels information from MySQL server!\n");

	return result;
}

/*
 * extract_traceinfo_mysql() -
 */
static void extract_traceinfo_mysql(
	char *sta, char *net, char *loc, char *chan, uint16_t *rtype, float *cfactor,
	const MYSQL_ROW sql_row, const unsigned long row_lengths[]
) {
	char _str[32] = { 0 };

/* */
	dblist_field_extract_sql( sta, TRACE2_STA_LEN, sql_row[0], row_lengths[0] );
	dblist_field_extract_sql( net, TRACE2_NET_LEN, sql_row[1], row_lengths[1] );
	dblist_field_extract_sql( loc, TRACE2_LOC_LEN, sql_row[2], row_lengths[2] );
	dblist_field_extract_sql( chan, TRACE2_CHAN_LEN, sql_row[3], row_lengths[3] );
	*rtype   = typestr2num( dblist_field_extract_sql( _str, sizeof(_str), sql_row[4], row_lengths[4] ) );
	*cfactor = atof(dblist_field_extract_sql( _str, sizeof(_str), sql_row[5], row_lengths[5] ));

	return;
}

#else
/*
 * fetch_list_sql() - Fake function
 */
static int fetch_list_sql( TraceList *list, const char *table_chan, const DBINFO *dbinfo, const int update )
{
	printf(
		"cf2trace: Skip the process of fetching station list from remote database "
		"'cause you did not define the _USE_SQL tag when compiling.\n"
	);
	return 0;
}
#endif

/*
 * init_trace_list() -
 */
static TraceList *init_trace_list( void )
{
	TraceList *result = (TraceList *)calloc(1, sizeof(TraceList));

	if ( result ) {
		result->count     = 0;
		result->timestamp = time(NULL);
		result->entry     = NULL;
		result->root      = NULL;
		result->root_t    = NULL;
	}

	return result;
}

/*
 * destroy_trace_list() -
 */
static void destroy_trace_list( TraceList *list )
{
	if ( list != (TraceList *)NULL ) {
	/* */
		tdestroy(list->root, dummy_func);
		dl_list_destroy( (DL_NODE **)&list->entry, free );
		free(list);
	}

	return;
}

/*
 *  append_traceinfo_list() - Appending the new client to the client list.
 */
static _TRACEINFO *append_traceinfo_list( TraceList *list, _TRACEINFO *traceinfo, const int update )
{
	_TRACEINFO *result = NULL;
	void      **_root  = update == CF2TRA_LIST_UPDATING ? &list->root : &list->root_t;

/* */
	if ( list && traceinfo ) {
		if ( (result = tfind(traceinfo, _root, compare_scnl)) == NULL ) {
		/* Insert the station information into binary tree */
			if ( dl_node_append( (DL_NODE **)&list->entry, traceinfo ) == NULL ) {
				logit("e", "cf2trace: Error insert channel into linked list!\n");
				goto except;
			}
			if ( (result = tsearch(traceinfo, &list->root_t, compare_scnl)) == NULL ) {
				logit("e", "cf2trace: Error insert channel into binary tree!\n");
				goto except;
			}
		}
		else if ( update == CF2TRA_LIST_UPDATING ) {
			update_traceinfo( *(_TRACEINFO **)result, traceinfo );
			if ( (result = tsearch(traceinfo, &list->root_t, compare_scnl)) == NULL ) {
				logit("e", "cf2trace: Error insert channel into binary tree!\n");
				goto except;
			}
		}
		else {
			logit(
				"o", "cf2trace: SCNL(%s.%s.%s.%s) is already in the list, skip it!\n",
				traceinfo->sta, traceinfo->chan, traceinfo->net, traceinfo->loc
			);
			free( traceinfo );
		}
	}

	return result ? *(_TRACEINFO **)result : NULL;
/* Exception handle */
except:
	free(traceinfo);
	return NULL;
}

/*
 *  create_new_traceinfo() - Creating new channel info memory space with the input value.
 */
static _TRACEINFO *create_new_traceinfo(
	const char *sta, const char *net, const char *loc, const char *chan, const uint8_t rtype, const float cfactor
) {
	_TRACEINFO *result = (_TRACEINFO *)calloc(1, sizeof(_TRACEINFO));

/* */
	if ( result )
		enrich_traceinfo_raw( result, sta, net, loc, chan, rtype, cfactor );

	return result;
}

/*
 *
 */
static _TRACEINFO *enrich_traceinfo_raw(
	_TRACEINFO *traceinfo, const char *sta, const char *net, const char *loc, const char *chan,
	const uint8_t rtype, const float cfactor
) {
/* */
	memcpy(traceinfo->sta, sta, TRACE2_STA_LEN);
	memcpy(traceinfo->net, net, TRACE2_NET_LEN);
	memcpy(traceinfo->loc, loc, TRACE2_LOC_LEN);
	memcpy(traceinfo->chan, chan, TRACE2_CHAN_LEN);
	traceinfo->seq = 0;
	traceinfo->recordtype = rtype;
	traceinfo->conversion_factor = cfactor;

	return traceinfo;
}

/*
 * update_traceinfo()
 */
static _TRACEINFO *update_traceinfo( _TRACEINFO *dest, const _TRACEINFO *src )
{
/* */
	dest->seq = src->seq;
	dest->recordtype = src->recordtype;
	dest->conversion_factor = src->conversion_factor;

	return dest;
}

/*
 * compare_scnl() - the SCNL compare function of binary tree search
 */
static int compare_scnl( const void *a, const void *b )
{
	_TRACEINFO *tmpa = (_TRACEINFO *)a;
	_TRACEINFO *tmpb = (_TRACEINFO *)b;
	int         rc;

	if ( (rc = strcmp(tmpa->sta, tmpb->sta)) != 0 )
		return rc;
	if ( (rc = strcmp(tmpa->chan, tmpb->chan)) != 0 )
		return rc;
	if ( (rc = strcmp(tmpa->net, tmpb->net)) != 0 )
		return rc;
	return strcmp(tmpa->loc, tmpb->loc);
}

/*
 * dummy_func() -
 */
static void dummy_func( void *node )
{
	return;
}
