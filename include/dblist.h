/*
 * stalist.h
 *
 * Header file for parse station list data.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * March, 2020
 *
 */
#pragma once
/* */
#include <mysql.h>
/* */
#include <dbinfo.h>

#define COL_STA_LIST_TABLE \
		X(COL_STA_SERIAL,   "serial"  ) \
		X(COL_STA_STATION,  "station" ) \
		X(COL_STA_NETWORK,  "network" ) \
		X(COL_STA_LOCATION, "location") \
		X(COL_STA_LIST_COUNT, "NULL"  )

#define X(a, b) a,
typedef enum {
	COL_STA_LIST_TABLE
} COL_STA_LIST;
#undef X

#define COL_CHAN_LIST_TABLE \
		X(COL_CHAN_STATION,  "station"          ) \
		X(COL_CHAN_NETWORK,  "network"          ) \
		X(COL_CHAN_LOCATION, "location"         ) \
		X(COL_CHAN_CHANNEL,  "channel"          ) \
		X(COL_CHAN_SEQ,      "sequence"         ) \
		X(COL_CHAN_RECORD,   "record_type"      ) \
		X(COL_CHAN_INST,     "instrument"       ) \
		X(COL_CHAN_SAMPRATE, "samprate"         ) \
		X(COL_CHAN_CFACTOR,  "conversion_factor") \
		X(COL_CHAN_LIST_COUNT, "NULL"           )

#define X(a, b) a,
typedef enum {
	COL_CHAN_LIST_TABLE
} COL_CHAN_LIST;
#undef X

/*
 *
 */
typedef union {
	COL_STA_LIST  col_sta;
	COL_CHAN_LIST col_chan;
} STALIST_COL_LIST __attribute__((__transparent_union__));

typedef char *(*GET_COLUMN_NAME)( const STALIST_COL_LIST );

/* Export functions' prototypes */
MYSQL_RES *dblist_sta_query_sql( const DBINFO *, const char *, const int, ... );
MYSQL_RES *dblist_sta_query_sql_historic( const DBINFO *, const char *, const time_t, const int, ... );
MYSQL_RES *dblist_chan_query_sql( const DBINFO *, const char *, const int, ... );
MYSQL_RES *dblist_chan_query_sql_historic( const DBINFO *, const char *, const time_t, const int, ... );
MYSQL_RES *dblist_chan_query_sql_snl(
	const DBINFO *, const char *, const char *, const char *, const char *, const int, ...
);
MYSQL_ROW      dblist_fetch_row_sql( MYSQL_RES * );
unsigned long *dblist_fetch_lengths_sql( MYSQL_RES * );
int            dblist_num_rows_sql( MYSQL_RES * );
unsigned int   dblist_num_fields_sql( MYSQL_RES * );
char          *dblist_field_extract_sql( char *, const unsigned int, const void *, const unsigned int );
void           dblist_free_result_sql( MYSQL_RES * );
MYSQL         *dblist_start_persistent_sql( const DBINFO * );
void           dblist_close_persistent_sql( void );
void           dblist_end_thread_sql( void );
