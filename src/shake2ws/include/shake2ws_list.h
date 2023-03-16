/*
 *
 */
#pragma once
/* */
#include <search.h>
/* */
#include <shake2ws.h>

/* */
STATION_PEAK *shake2ws_list_search( const char *, const char *, const char * );
STATION_PEAK *shake2ws_list_find( const char *, const char *, const char * );
void          shake2ws_list_walk( void (*)(const void *, const VISIT, const int) );
CHAN_PEAK    *shake2ws_list_chlist_search( const STATION_PEAK *, const int, const char * );
CHAN_PEAK    *shake2ws_list_chlist_find( const STATION_PEAK *, const int, const char * );
int           shake2ws_list_list_map( void **, char *, void *, const size_t );
STATION_PEAK *shake2ws_list_station_map( const char *, const char *, const char * );
void          shake2ws_list_end( void );
