/*
 *
 */
#pragma once
/* */
#include <time.h>
/* */
#include <dbinfo.h>
#include <tracepeak.h>
#include <peak2trig.h>
/* */
#define PEAK2TRIG_LIST_INITIALIZING  0
#define PEAK2TRIG_LIST_UPDATING      1
/* */
int       peak2trig_list_db_fetch( const char *, const DBINFO *, const int );
int       peak2trig_list_sta_line_parse( const char *, const int );
void      peak2trig_list_end( void );
_STAINFO *peak2trig_list_find( const TRACE_PEAKVALUE * );
void      peak2trig_list_tree_activate( void );
void      peak2trig_list_tree_abandon( void );
int       peak2trig_list_total_station_get( void );
time_t    peak2trig_list_timestamp_get( void );
