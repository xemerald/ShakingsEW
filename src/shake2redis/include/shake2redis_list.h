/*
 *
 */
#pragma once
/* */
#include <search.h>
/* */
#include <shake2redis.h>

/* */
int           sk2rd_list_init( void );
STATION_PEAK *sk2rd_list_search( const char *, const char *, const char * );
STATION_PEAK *sk2rd_list_find( const char *, const char *, const char * );
void          sk2rd_list_walk( void (*)( void *, const int, void * ), void * );
CHAN_PEAK    *sk2rd_list_chlist_search( STATION_PEAK *, const char *, const int );
CHAN_PEAK    *sk2rd_list_chlist_find( const STATION_PEAK *, const char * );
void          sk2rd_list_chlist_delete( STATION_PEAK *, const char *, const int );
void          sk2rd_list_end( void );
int           sk2rd_list_total_sta_get( void );
