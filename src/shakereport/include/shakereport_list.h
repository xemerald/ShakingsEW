/*
 *
 */
#pragma once
/* */
#include <search.h>
/* */
#include <shakereport.h>

/* */
int           skrep_list_init( void );
STATION_PEAK *skrep_list_search( const char *, const char *, const char * );
STATION_PEAK *skrep_list_find( const char *, const char *, const char * );
void          skrep_list_walk( void (*)( void *, const int, void * ), void * );
CHAN_PEAK    *skrep_list_chlist_search( STATION_PEAK *, const char *, const int );
CHAN_PEAK    *skrep_list_chlist_find( const STATION_PEAK *, const char * );
void          skrep_list_chlist_delete( STATION_PEAK *, const char *, const int );
void          skrep_list_end( void );
int           skrep_list_total_sta_get( void );
