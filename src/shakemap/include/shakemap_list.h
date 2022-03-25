/*
 *
 */
#pragma once
/* */
#include <time.h>
/* */
#include <dbinfo.h>
#include <shakemap.h>
/* */
#define SHAKEMAP_LIST_INITIALIZING  0
#define SHAKEMAP_LIST_UPDATING      1
/* */
int       shakemap_list_db_fetch( const char *, const DBINFO *, const int );
int       shakemap_list_sta_line_parse( const char *, const int );
void      shakemap_list_end( void );
_STAINFO *shakemap_list_find( const char *, const char *, const char * );
void      shakemap_list_tree_activate( void );
void      shakemap_list_tree_abandon( void );
int       shakemap_list_total_station_get( void );
time_t    shakemap_list_timestamp_get( void );
