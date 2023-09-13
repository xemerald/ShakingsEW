/*
 *
 */
#pragma once
/* Earthworm environment header include */
#include <trace_buf.h>
/* Local header include */
#include <vecsum.h>

/* Function prototype */
int vecsum_vslist_vs_reg( const char *, const char *, const char *, const char * );
VECSUM_INFO *vecsum_vslist_search( const TRACE2X_HEADER * );
VECSUM_INFO *vecsum_vslist_find( const TRACE2X_HEADER * );
void        vecsum_vslist_end( void );
