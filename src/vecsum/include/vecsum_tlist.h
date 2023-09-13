/*
 *
 */
#pragma once
/* Earthworm environment header include */
#include <trace_buf.h>
/* Local header include */
#include <vecsum.h>

/* Function prototype */
_TRACEINFO *vecsum_tlist_search( const TRACE2X_HEADER * );
_TRACEINFO *vecsum_tlist_find( const TRACE2X_HEADER * );
void        vecsum_tlist_end( void );
