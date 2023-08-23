/*
 *
 */
#pragma once
/* */
#include <search.h>
/* */
#include <shake2ws.h>

/* */
int           sk2ws_list_init( void );
STATION_PEAK *sk2ws_list_search( const char *, const char *, const char * );
STATION_PEAK *sk2ws_list_find( const char *, const char *, const char * );
void          sk2ws_list_walk( void (*)( void *, const int, void * ), void * );
CHAN_PEAK    *sk2ws_list_chlist_search( STATION_PEAK *, const char *, const int );
CHAN_PEAK    *sk2ws_list_chlist_find( const STATION_PEAK *, const char * );
void          sk2ws_list_chlist_delete( STATION_PEAK *, const char *, const int );
int           sk2ws_list_total_sta_get( void );
time_t        sk2ws_list_timestamp_get( void );
int           sk2ws_list_list_map( void **, char *, void *, const size_t );
STATION_PEAK *sk2ws_list_station_map( const char *, const char *, const char * );
void          sk2ws_list_end( void );
