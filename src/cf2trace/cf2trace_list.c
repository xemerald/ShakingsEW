/**
 * @file cf2trace_list.c
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University (b98204032@gmail.com)
 * @brief
 * @date 2018-03-20
 *
 * @copyright Copyright (c) 2018-now
 *
 */
#define _GNU_SOURCE
/**
 * @name Standard C header include
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <search.h>
#include <time.h>
#include <ctype.h>
/**
 * @name Earthworm environment header include
 *
 */
#include <earthworm.h>
/**
 * @name Local header include
 *
 */
#include <dblist.h>
#include <dbinfo.h>
#include <dl_chain_list.h>
#include <recordtype.h>
#include <cf2trace.h>
#include <cf2trace_list.h>

/**
 * @brief
 *
 */
typedef struct {
	int     count;      /* Number of clients in the list */
	time_t  timestamp;  /* Time of the last time updated */
	void   *entry;      /* Pointer to first client       */
	void   *root;       /* Root of binary searching tree */
	void   *root_t;     /* Temporary root of binary searching tree */
} TraceList;

/**
 * @name Functions prototype in this source file
 *
 */
static int         fetch_list_sql( TraceList *, const char *, const DBINFO *, const int );
static TraceList  *create_trace_list( void );
static void        destroy_trace_list( TraceList * );
static int         compare_scnl( const void *, const void * );
static void        dummy_func( void * );
static _TRACEINFO *update_traceinfo( _TRACEINFO *, const _TRACEINFO * );
static _TRACEINFO *append_traceinfo_list( TraceList *, _TRACEINFO *, const int );
static _TRACEINFO *create_new_traceinfo(
	const char *, const char *, const char *, const char *, const uint8_t, const double
);
static _TRACEINFO *enrich_traceinfo_raw(
	_TRACEINFO *, const char *, const char *, const char *, const char *, const uint8_t, const double
);
/**
 * @name
 *
 */
#if defined( _USE_SQL )
static void extract_traceinfo_mysql(
	char *, char *, char *, char *, uint8_t *, double *, const MYSQL_ROW, const unsigned long []
);
#endif
/**
 * @brief
 *
 */
static TraceList *TList = NULL;

/**
 * @brief
 *
 * @param table_chan
 * @param dbinfo
 * @param update
 * @return int
 */
int cf2tra_list_db_fetch( const char *table_chan, const DBINFO *dbinfo, const int update )
{
	if ( !TList ) {
		TList = create_trace_list();
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

/**
 * @brief Function for parsing the channel information line in list file.
 *
 * @param line
 * @param update
 * @return int
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
	double  cfactor;

/* */
	if ( !TList ) {
		TList = create_trace_list();
		if ( !TList ) {
			logit("e", "cf2trace: Fatal! Trace list memory initialized error!\n");
			return -3;
		}
	}
/* */
	if ( sscanf(line, "%s %s %s %s %s %lf", sta, net, loc, chan, typestr, &cfactor) >= 6 ) {
	/* */
		rtype = typestr2num( typestr );
		if ( !append_traceinfo_list( TList, create_new_traceinfo( sta, net, loc, chan, rtype, cfactor ), update ) )
			result = -2;
	}
	else {
		logit("e", "cf2trace: ERROR, lack of some trace information in local list!\n");
		result = -1;
	}

	return result;
}

/**
 * @brief End the whole trace list service.
 *
 */
void cf2tra_list_end( void )
{
	destroy_trace_list( TList );
	TList = NULL;

	return;
}

/**
 * @brief Find the trace info that match the input trace header.
 *
 * @param trh2x
 * @return _TRACEINFO*
 */
_TRACEINFO *cf2tra_list_find( const TRACE2X_HEADER *trh2x )
{
	_TRACEINFO *result = NULL;
	_TRACEINFO  key;

/* */
	memcpy(key.sta, trh2x->sta, TRACE2_STA_LEN);
	memcpy(key.net, trh2x->net, TRACE2_NET_LEN);
	memcpy(key.loc, trh2x->loc, TRACE2_LOC_LEN);
	memcpy(key.chan, trh2x->chan, TRACE2_CHAN_LEN);
/* Find which station */
	if ( (result = tfind(&key, &TList->root, compare_scnl)) ) {
	/* Found in the main Palert table */
		result = *(_TRACEINFO **)result;
	}

	return result;
}

/**
 * @brief
 *
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

/**
 * @brief
 *
 */
void cf2tra_list_tree_abandon( void )
{
	if ( TList->root_t )
		tdestroy(TList->root_t, dummy_func);

	TList->root_t = NULL;

	return;
}

/**
 * @brief Get the total channel number of the main trace list.
 *
 * @return int
 */
int cf2tra_list_total_channel_get( void )
{
	DL_NODE *node   = NULL;
	int      result = 0;

/* */
	DL_LIST_FOR_EACH( (DL_NODE *)TList->entry, node ) {
		result++;
	}
/* */
	TList->count = result;

	return result;
}

/**
 * @brief Get the timestamp of the main trace list.
 *
 * @return time_t
 */
time_t cf2tra_list_timestamp_get( void )
{
	return TList->timestamp;
}

/**
 * @name
 *
 */
#if defined( _USE_SQL )
/**
 * @brief Get stations list from MySQL server.
 *
 * @param list
 * @param table_chan
 * @param dbinfo
 * @param update
 * @return int
 */
static int fetch_list_sql( TraceList *list, const char *table_chan, const DBINFO *dbinfo, const int update )
{
	int     result = 0;
	char    sta[TRACE2_STA_LEN] = { 0 };
	char    net[TRACE2_NET_LEN] = { 0 };
	char    loc[TRACE2_LOC_LEN] = { 0 };
	char    chan[TRACE2_CHAN_LEN] = { 0 };
	uint8_t rtype;
	double  cfactor;

	MYSQL_RES *sql_res = NULL;
	MYSQL_ROW  sql_row;

/* Connect to database */
	printf("cf2trace: Querying the channels information from MySQL server %s...\n", dbinfo->host);
	sql_res = dblist_chan_query_sql(
		dbinfo, table_chan, CF2TRA_INFO_FROM_SQL,
		COL_CHAN_STATION, COL_CHAN_NETWORK, COL_CHAN_LOCATION,
		COL_CHAN_CHANNEL, COL_CHAN_RECORD, COL_CHAN_CFACTOR
	);
	if ( !sql_res )
		return -1;
	printf("cf2trace: Queried the channels information success!\n");

/* Start the SQL server connection for channel */
	dblist_start_persistent_sql( dbinfo );
/* Read station list from query result */
	while ( (sql_row = dblist_fetch_row_sql( sql_res )) ) {
	/* */
		extract_traceinfo_mysql(
			sta, net, loc, chan, &rtype, &cfactor,
			sql_row, dblist_fetch_lengths_sql( sql_res )
		);
	/* */
		if (
			append_traceinfo_list(
				list, create_new_traceinfo( sta, net, loc, chan, rtype, cfactor ), update
			)
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

/**
 * @brief
 *
 * @param sta
 * @param net
 * @param loc
 * @param chan
 * @param rtype
 * @param cfactor
 * @param sql_row
 * @param row_lengths
 */
static void extract_traceinfo_mysql(
	char *sta, char *net, char *loc, char *chan, uint8_t *rtype, double *cfactor,
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
/**
 * @brief Fake function.
 *
 * @param list
 * @param table_chan
 * @param dbinfo
 * @param update
 * @return int
 */
static int fetch_list_sql( TraceList *list, const char *table_chan, const DBINFO *dbinfo, const int update )
{
	printf(
		"cf2trace: Skip the process of fetching channel list from remote database "
		"'cause you did not define the _USE_SQL tag when compiling.\n"
	);
	return 0;
}
#endif

/**
 * @brief Create a whole new trace list.
 *
 * @return TraceList*
 */
static TraceList *create_trace_list( void )
{
	TraceList *result;

	if ( (result = (TraceList *)calloc(1, sizeof(TraceList))) ) {
		result->count     = 0;
		result->timestamp = time(NULL);
		result->entry     = NULL;
		result->root      = NULL;
		result->root_t    = NULL;
	}

	return result;
}

/**
 * @brief Destroy the whole trace list.
 *
 * @param list
 */
static void destroy_trace_list( TraceList *list )
{
	if ( list ) {
	/* */
		tdestroy(list->root, dummy_func);
		dl_list_destroy( (DL_NODE **)&list->entry, free );
		free(list);
	}

	return;
}

/**
 * @brief Append the new trace to the trace list.
 *
 * @param list
 * @param traceinfo
 * @param update
 * @return _TRACEINFO*
 */
static _TRACEINFO *append_traceinfo_list( TraceList *list, _TRACEINFO *traceinfo, const int update )
{
	_TRACEINFO *result = NULL;
	void      **_root  = update == CF2TRA_LIST_UPDATING ? &list->root : &list->root_t;

/* */
	if ( list && traceinfo ) {
		if ( !(result = tfind(traceinfo, _root, compare_scnl)) ) {
		/* Insert the station information into binary tree */
			if ( !dl_node_append( (DL_NODE **)&list->entry, traceinfo ) ) {
				logit("e", "cf2trace: Error insert channel into linked list!\n");
				goto except;
			}
			if ( !(result = tsearch(traceinfo, &list->root_t, compare_scnl)) ) {
				logit("e", "cf2trace: Error insert channel into binary tree!\n");
				goto except;
			}
		}
		else if ( update == CF2TRA_LIST_UPDATING ) {
			update_traceinfo( *(_TRACEINFO **)result, traceinfo );
			if ( !(result = tsearch(traceinfo, &list->root_t, compare_scnl)) ) {
				logit("e", "cf2trace: Error insert channel into binary tree!\n");
				goto except;
			}
		}
		else {
			logit(
				"o", "cf2trace: SCNL(%s.%s.%s.%s) is already in the list, skip it!\n",
				traceinfo->sta, traceinfo->chan, traceinfo->net, traceinfo->loc
			);
			free(traceinfo);
		}
	}

	return result ? *(_TRACEINFO **)result : NULL;
/* Exception handle */
except:
	free(traceinfo);
	return NULL;
}

/**
 * @brief Create new channel info memory space with the input value.
 *
 * @param sta
 * @param net
 * @param loc
 * @param chan
 * @param rtype
 * @param cfactor
 * @return _TRACEINFO*
 */
static _TRACEINFO *create_new_traceinfo(
	const char *sta, const char *net, const char *loc, const char *chan, const uint8_t rtype, const double cfactor
) {
	_TRACEINFO *result;

/* */
	if ( (result = (_TRACEINFO *)calloc(1, sizeof(_TRACEINFO))) )
		enrich_traceinfo_raw( result, sta, net, loc, chan, rtype, cfactor );

	return result;
}

/**
 * @brief Enrich channel info to memory space with the input value.
 *
 * @param dest
 * @param sta
 * @param net
 * @param loc
 * @param chan
 * @param rtype
 * @param cfactor
 * @return _TRACEINFO*
 */
static _TRACEINFO *enrich_traceinfo_raw(
	_TRACEINFO *dest, const char *sta, const char *net, const char *loc, const char *chan,
	const uint8_t rtype, const double cfactor
) {
/* */
	memcpy(dest->sta, sta, TRACE2_STA_LEN);
	memcpy(dest->net, net, TRACE2_NET_LEN);
	memcpy(dest->loc, loc, TRACE2_LOC_LEN);
	memcpy(dest->chan, chan, TRACE2_CHAN_LEN);
	dest->seq = 0;
	dest->recordtype = rtype;
	dest->conversion_factor = cfactor;

	return dest;
}

/**
 * @brief Function only update the traceinfo for the same trace.
 *
 * @param dest
 * @param src
 * @return _TRACEINFO*
 */
static _TRACEINFO *update_traceinfo( _TRACEINFO *dest, const _TRACEINFO *src )
{
/* */
	dest->seq = src->seq;
	dest->recordtype = src->recordtype;
	dest->conversion_factor = src->conversion_factor;

	return dest;
}

/**
 * @brief SCNL compare function of binary tree search
 *
 * @param a
 * @param b
 * @return int
 */
static int compare_scnl( const void *a, const void *b )
{
	_TRACEINFO *tmpa = (_TRACEINFO *)a;
	_TRACEINFO *tmpb = (_TRACEINFO *)b;
	int         rc;

	if ( (rc = memcmp(tmpa->sta, tmpb->sta, TRACE2_STA_LEN)) != 0 )
		return rc;
	if ( (rc = memcmp(tmpa->chan, tmpb->chan, TRACE2_CHAN_LEN)) != 0 )
		return rc;
	if ( (rc = memcmp(tmpa->net, tmpb->net, TRACE2_NET_LEN)) != 0 )
		return rc;
	return memcmp(tmpa->loc, tmpb->loc, TRACE2_LOC_LEN);
}

/**
 * @brief Just a dummy function.
 *
 * @param node
 */
static void dummy_func( void *node )
{
	return;
}
