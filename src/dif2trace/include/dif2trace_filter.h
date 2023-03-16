/*
 *
 */
#pragma once
/* Local header include */
#include <iirfilter.h>
#include <dif2trace.h>

/* Filter node structure */
typedef struct {
	double     delta;
	IIR_FILTER filter;
} TRACE_FILTER;

/* Function prototype */
int         dif2tra_filter_init( const int, const double, const double, const int, const int );
IIR_FILTER *dif2tra_filter_search( const _TRACEINFO * );
IIR_FILTER *dif2tra_filter_find( const _TRACEINFO * );
IIR_STAGE  *dif2tra_filter_stage_create( const _TRACEINFO * );
double      dif2tra_filter_apply( double, _TRACEINFO * );
void        dif2tra_filter_end( void );
