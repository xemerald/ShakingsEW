/*
 *
 */
#pragma once
/* */
#include <time.h>
/* */
#include <trace_buf.h>
/* */
#include <dbinfo.h>
#include <cf2trace.h>

/* */
#define CF2TRA_LIST_INITIALIZING  0
#define CF2TRA_LIST_UPDATING      1
/* */
int         cf2tra_list_db_fetch( const char *, const DBINFO *, const int );
int         cf2tra_list_chan_line_parse( const char *, const int );
void        cf2tra_list_end( void );
_TRACEINFO *cf2tra_list_find( const TRACE2X_HEADER * );
void        cf2tra_list_tree_activate( void );
void        cf2tra_list_tree_abandon( void );
int         cf2tra_list_total_channel_get( void );
time_t      cf2tra_list_timestamp_get( void );
