/*
 *
 */
#pragma once
/* Earthworm environment header include */
#include <trace_buf.h>
/* Local header include */
#include <dif2trace.h>

/* Function prototype */
_TRACEINFO *dif2tra_list_search( const TRACE2X_HEADER * );
_TRACEINFO *dif2tra_list_find( const TRACE2X_HEADER * );
void        dif2tra_list_end( void );
