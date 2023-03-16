/*
 *
 */
#pragma once
/* Earthworm environment header include */
#include <trace_buf.h>
/* Local header include */
#include <respectra.h>

/* Function prototype */
_TRACEINFO *rsp_list_search( const TRACE2X_HEADER * );
_TRACEINFO *rsp_list_find( const TRACE2X_HEADER * );
void        rsp_list_end( void );
