/*
 *
 */
#pragma once
/* Local header include */
#include <respectra.h>
/* Filter node structure */
typedef struct {
	double  delta;
	PMATRIX pmatrix;
} TRACE_PMATRIX;

/* Function prototype */
int      rsp_pmat_init( const double, const double );
PMATRIX *rsp_pmat_search( const _TRACEINFO * );
PMATRIX *rsp_pmat_find( const _TRACEINFO * );
void     rsp_pmat_end( void );
