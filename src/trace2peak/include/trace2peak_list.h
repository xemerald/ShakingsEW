/*
 *
 */
#pragma once
/*
 *
 */
#include <trace_buf.h>
#include <trace2peak.h>

_TRACEPEAK *tra2peak_list_search( const TRACE2X_HEADER * );
_TRACEPEAK *tra2peak_list_find( const TRACE2X_HEADER * );
void        tra2peak_list_end( void );
