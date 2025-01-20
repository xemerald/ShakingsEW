/**
 * @file cf2trace_list.h
 * @author Benjamin Ming Yang (b98204032@gmail.com) @ Department of Geology, National Taiwan University
 * @brief
 * @date 2018-03-20
 *
 * @copyright Copyright (c) 2018
 *
 */
#pragma once
/**
 * @name Standard C header include
 *
 */
#include <time.h>
/**
 * @name Earthworm environment header include
 *
 */
#include <trace_buf.h>
/**
 * @name Local header include
 *
 */
#include <dbinfo.h>
#include <cf2trace.h>

/**
 * @name List updating flag used in argument of function.
 *
 */
#define CF2TRA_LIST_INITIALIZING  0
#define CF2TRA_LIST_UPDATING      1

/**
 * @name Externa function prototypes
 *
 */
int         cf2tra_list_db_fetch( const char *, const DBINFO *, const int );
int         cf2tra_list_chan_line_parse( const char *, const int );
void        cf2tra_list_end( void );
_TRACEINFO *cf2tra_list_find( const TRACE2X_HEADER * );
void        cf2tra_list_tree_activate( void );
void        cf2tra_list_tree_abandon( void );
int         cf2tra_list_total_channel_get( void );
time_t      cf2tra_list_timestamp_get( void );
