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
#include <peak2trig.h>
#include <peak2trig_misc.h>
#include <peak2trig_list.h>
/* */
typedef struct {
	int     count;      /* Number of clients in the list */
	time_t  timestamp;  /* Time of the last time updated */
	void   *entry;      /* Pointer to first client       */
	void   *root;       /* Root of binary searching tree */
	void   *root_t;     /* Temporary root of binary searching tree */
} StaList;

/* */
static int       fetch_list_sql( StaList *, const char *, const DBINFO *, const int );
static StaList  *init_sta_list( void );
static void      destroy_sta_list( StaList * );
static void      dummy_func( void * );
static _STAINFO *update_stainfo( _STAINFO *, const _STAINFO * );
static _STAINFO *append_stainfo_list( StaList *, _STAINFO *, const int );
static _STAINFO *create_new_stainfo(
	const char *, const char *, const char *, const double, const double, const double
);
static _STAINFO *enrich_stainfo_raw(
	_STAINFO *, const char *, const char *, const char *, const double, const double, const double
);
/* */
#if defined( _USE_SQL )
static void extract_stainfo_mysql(
	char *, char *, char *, double *, double *, double *, const MYSQL_ROW, const unsigned long []
);
#endif
/* */
static StaList *SList = NULL;

/*
 * peak2trig_list_db_fetch() -
 */
int peak2trig_list_db_fetch( const char *table_sta, const DBINFO *dbinfo, const int update )
{
	if ( !SList ) {
		SList = init_sta_list();
		if ( !SList ) {
			logit("e", "peak2trig: Fatal! Trace list memory initialized error!\n");
			return -3;
		}
	}

	if ( strlen(dbinfo->host) > 0 && strlen(table_sta) > 0 )
		return fetch_list_sql( SList, table_sta, dbinfo, update );
	else
		return 0;
}

/*
 *
 */
int peak2trig_list_sta_line_parse( const char *line, const int update )
{
	int     result = 0;
	char    sta[TRACE2_STA_LEN]   = { 0 };
	char    net[TRACE2_NET_LEN]   = { 0 };
	char    loc[TRACE2_LOC_LEN]   = { 0 };
	double  lat = 0.0;
	double  lon = 0.0;
	double  elv = 0.0;

/* */
	if ( !SList ) {
		SList = init_sta_list();
		if ( !SList ) {
			logit("e", "peak2trig: Fatal! Trace list memory initialized error!\n");
			return -3;
		}
	}
/* */
	if ( sscanf(line, "%s %s %s %lf %lf %lf", sta, net, loc, &lat, &lon, &elv) >= 6 ) {
	/* */
		if ( append_stainfo_list( SList, create_new_stainfo( sta, net, loc, lat, lon, elv ), update ) == NULL )
			result = -2;
	}
	else {
		logit("e", "peak2trig: ERROR, lack of some trace information in local list!\n");
		result = -1;
	}

	return result;
}

/*
 * peak2trig_list_end() -
 */
void peak2trig_list_end( void )
{
	destroy_sta_list( SList );
	SList = NULL;

	return;
}

/*
 * peak2trig_list_find() -
 */
_STAINFO *peak2trig_list_find( const TRACE_PEAKVALUE *tpv )
{
	_STAINFO *result = NULL;
	_STAINFO  key;

/* */
	memcpy(key.sta, tpv->sta, TRACE2_STA_LEN);
	memcpy(key.net, tpv->net, TRACE2_NET_LEN);
	memcpy(key.loc, tpv->loc, TRACE2_LOC_LEN);
/* Find which station */
	if ( (result = tfind(&key, &SList->root, peak2trig_misc_snl_compare)) != NULL ) {
	/* Found in the main Palert table */
		result = *(_STAINFO **)result;
	}

	return result;
}

/*
 * peak2trig_list_tree_activate() -
 */
void peak2trig_list_tree_activate( void )
{
	void *_root = SList->root;

	SList->root      = SList->root_t;
	SList->root_t    = NULL;
	SList->timestamp = time(NULL);

	if ( _root ) {
		sleep_ew(1000);
		tdestroy(_root, dummy_func);
	}

	return;
}

/*
 * peak2trig_list_tree_abandon() -
 */
void peak2trig_list_tree_abandon( void )
{
	if ( SList->root_t )
		tdestroy(SList->root_t, dummy_func);

	SList->root_t = NULL;

	return;
}

/*
 * peak2trig_list_total_station_get() -
 */
int peak2trig_list_total_station_get( void )
{
	DL_NODE *node   = NULL;
	int      result = 0;

/* */
	DL_LIST_FOR_EACH( (DL_NODE *)SList->entry, node ) {
		result++;
	}
/* */
	SList->count = result;

	return result;
}

/*
 * peak2trig_list_timestamp_get() -
 */
time_t peak2trig_list_timestamp_get( void )
{
	return SList->timestamp;
}

/*
 *
 */
#if defined( _USE_SQL )
/*
 * fetch_list_sql() - Get stations list from MySQL server
 */
static int fetch_list_sql( StaList *list, const char *table_sta, const DBINFO *dbinfo, const int update )
{
	int     result = 0;
	char    sta[TRACE2_STA_LEN] = { 0 };
	char    net[TRACE2_NET_LEN] = { 0 };
	char    loc[TRACE2_LOC_LEN] = { 0 };
	double  lat;
	double  lon;
	double  elv;

	MYSQL_RES *sql_res = NULL;
	MYSQL_ROW  sql_row;

/* Connect to database */
	printf("peak2trig: Querying the stations information from MySQL server %s...\n", dbinfo->host);
	sql_res = dblist_sta_query_sql(
		dbinfo, table_sta, PEAK2TRIG_INFO_FROM_SQL,
		COL_STA_STATION, COL_STA_NETWORK, COL_STA_LOCATION,
		COL_STA_LATITUDE, COL_STA_LONGITUDE, COL_STA_ELEVATION
	);
	if ( sql_res == NULL )
		return -1;
	printf("peak2trig: Queried the stations information success!\n");

/* Start the SQL server connection for channel */
	dblist_start_persistent_sql( dbinfo );
/* Read station list from query result */
	while ( (sql_row = dblist_fetch_row_sql( sql_res )) != NULL ) {
	/* */
		extract_stainfo_mysql(
			sta, net, loc, &lat, &lon, &elv,
			sql_row, dblist_fetch_lengths_sql( sql_res )
		);
	/* */
		if (
			append_stainfo_list(
				list, create_new_stainfo( sta, net, loc, lat, lon, elv ), update
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
		logit("o", "peak2trig: Read %d stations information from MySQL server success!\n", result);
	else
		logit("e", "peak2trig: Some errors happened when fetching stations information from MySQL server!\n");

	return result;
}

/*
 * extract_stainfo_mysql() -
 */
static void extract_stainfo_mysql(
	char *sta, char *net, char *loc, double *lat, double *lon, double *elv,
	const MYSQL_ROW sql_row, const unsigned long row_lengths[]
) {
	char _str[32] = { 0 };

/* */
	dblist_field_extract_sql( sta, TRACE2_STA_LEN, sql_row[0], row_lengths[0] );
	dblist_field_extract_sql( net, TRACE2_NET_LEN, sql_row[1], row_lengths[1] );
	dblist_field_extract_sql( loc, TRACE2_LOC_LEN, sql_row[2], row_lengths[2] );
	*lat = atof(dblist_field_extract_sql( _str, sizeof(_str), sql_row[3], row_lengths[3] ));
	*lon = atof(dblist_field_extract_sql( _str, sizeof(_str), sql_row[4], row_lengths[4] ));
	*elv = atof(dblist_field_extract_sql( _str, sizeof(_str), sql_row[5], row_lengths[5] ));

	return;
}

#else
/*
 * fetch_list_sql() - Fake function
 */
static int fetch_list_sql( StaList *list, const char *table_sta, const DBINFO *dbinfo, const int update )
{
	printf(
		"peak2trig: Skip the process of fetching station list from remote database "
		"'cause you did not define the _USE_SQL tag when compiling.\n"
	);
	return 0;
}
#endif

/*
 * init_sta_list() -
 */
static StaList *init_sta_list( void )
{
	StaList *result = (StaList *)calloc(1, sizeof(StaList));

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
 * destroy_sta_list() -
 */
static void destroy_sta_list( StaList *list )
{
	if ( list != (StaList *)NULL ) {
	/* */
		tdestroy(list->root, dummy_func);
		dl_list_destroy( (DL_NODE **)&list->entry, free );
		free(list);
	}

	return;
}

/*
 *  append_stainfo_list() - Appending the new client to the client list.
 */
static _STAINFO *append_stainfo_list( StaList *list, _STAINFO *stainfo, const int update )
{
	_STAINFO *result = NULL;
	void    **_root  = update == PEAK2TRIG_LIST_UPDATING ? &list->root : &list->root_t;

/* */
	if ( list && stainfo ) {
		if ( (result = tfind(stainfo, _root, peak2trig_misc_snl_compare)) == NULL ) {
		/* Insert the station information into binary tree */
			if ( dl_node_append( (DL_NODE **)&list->entry, stainfo ) == NULL ) {
				logit("e", "peak2trig: Error insert channel into linked list!\n");
				goto except;
			}
			if ( (result = tsearch(stainfo, &list->root_t, peak2trig_misc_snl_compare)) == NULL ) {
				logit("e", "peak2trig: Error insert channel into binary tree!\n");
				goto except;
			}
		}
		else if ( update == PEAK2TRIG_LIST_UPDATING ) {
			update_stainfo( *(_STAINFO **)result, stainfo );
			if ( (result = tsearch(stainfo, &list->root_t, peak2trig_misc_snl_compare)) == NULL ) {
				logit("e", "peak2trig: Error insert channel into binary tree!\n");
				goto except;
			}
		}
		else {
			logit(
				"o", "peak2trig: SNL(%s.%s.%s) is already in the list, skip it!\n",
				stainfo->sta, stainfo->net, stainfo->loc
			);
			free(stainfo);
		}
	}

	return result ? *(_STAINFO **)result : NULL;
/* Exception handle */
except:
	free(stainfo);
	return NULL;
}

/*
 *  create_new_stainfo() - Creating new channel info memory space with the input value.
 */
static _STAINFO *create_new_stainfo(
	const char *sta, const char *net, const char *loc, const double lat, const double lon, const double elv
) {
	_STAINFO *result = (_STAINFO *)calloc(1, sizeof(_STAINFO));

/* */
	if ( result )
		enrich_stainfo_raw( result, sta, net, loc, lat, lon, elv );

	return result;
}

/*
 * enrich_stainfo_raw() -
 */
static _STAINFO *enrich_stainfo_raw(
	_STAINFO *stainfo, const char *sta, const char *net, const char *loc,
	const double lat, const double lon, const double elv
) {
/* */
	memcpy(stainfo->sta, sta, TRACE2_STA_LEN);
	memcpy(stainfo->net, net, TRACE2_NET_LEN);
	memcpy(stainfo->loc, loc, TRACE2_LOC_LEN);
	stainfo->latitude  = lat;
	stainfo->longitude = lon;
	stainfo->elevation = elv;

	return stainfo;
}

/*
 * update_stainfo() -
 */
static _STAINFO *update_stainfo( _STAINFO *dest, const _STAINFO *src )
{
/* */
	dest->latitude  = src->latitude;
	dest->longitude = src->longitude;
	dest->elevation = src->elevation;

	return dest;
}

/*
 * dummy_func() -
 */
static void dummy_func( void *node )
{
	return;
}
