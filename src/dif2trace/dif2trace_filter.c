#define _GNU_SOURCE
/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <search.h>
#include <float.h>
#include <math.h>
/* Earthworm environment header include */
#include <earthworm.h>
#include <trace_buf.h>
/* Local header include */
#include <iirfilter.h>
#include <dif2trace.h>
#include <dif2trace_list.h>
#include <dif2trace_filter.h>

static int compare_delta( const void *, const void * );	 /* The compare function of binary tree search */

static void  *FilterRoot  = NULL;         /* Root of filter binary tree */
static int    FilterOrder = 1;
static double FreqHigh    = 0.0;
static double FreqLow     = 0.0;

static IIR_FILTER_TYPE      FilterType  = IIR_FILTER_TYPE_COUNT;
static IIR_ANALOG_PROTOTYPE AnalogPtype = IIR_ANALOG_PROTOTYPE_COUNT;


/*
 * dif2tra_filter_init() - Initialization function of filter chain.
 * Arguments:
 * Returns:
 *    0 = Normal.
 */
int dif2tra_filter_init(
	const int order, const double freql, const double freqh, const int filtertype, const int analogptype
) {
	FilterOrder = order;
	FreqLow     = freql;
	FreqHigh    = freqh;

	FilterType  = filtertype;
	if ( FilterType < 0 || FilterType >= IIR_FILTER_TYPE_COUNT )
		return -1;

	AnalogPtype = analogptype;
	if ( AnalogPtype < 0 || AnalogPtype >= IIR_ANALOG_PROTOTYPE_COUNT )
		return -1;

	return 0;
}

/*
 * dif2tra_filter_search( ) -- Insert the trace to the tree.
 * Arguments:
 *   trh2x = Pointer of the trace you want to find.
 * Returns:
 *    NULL = Didn't find the trace.
 *   !NULL = The Pointer to the trace.
 */
IIR_FILTER *dif2tra_filter_search( const _TRACEINFO *traceptr )
{
	TRACE_FILTER *trfptr = NULL;
	IIR_FILTER   *filterptr = NULL;

/* Test if already in the tree */
	if ( (filterptr = dif2tra_filter_find( traceptr )) == NULL ) {
		trfptr = (TRACE_FILTER *)calloc(1, sizeof(TRACE_FILTER));
		trfptr->delta  = traceptr->delta;
		trfptr->filter = designfilter( FilterOrder, FilterType, AnalogPtype, FreqLow, FreqHigh, trfptr->delta );

		if ( (trfptr = tsearch(trfptr, &FilterRoot, compare_delta)) == NULL ) {
		/* Something error when insert into the tree */
			return NULL;
		}

		trfptr    = *(TRACE_FILTER **)trfptr;
		filterptr = &trfptr->filter;
	}

	return filterptr;
}

/*
 * dif2tra_filter_find( ) -- Output the Trace that match the SCNL.
 * Arguments:
 *   trh2x = Pointer of the trace you want to find.
 * Returns:
 *    NULL = Didn't find the trace.
 *   !NULL = The Pointer to the trace.
 */
IIR_FILTER *dif2tra_filter_find( const _TRACEINFO *traceptr )
{
	TRACE_FILTER  key;
	TRACE_FILTER *tfptr     = NULL;

	key.delta = traceptr->delta;

/* Find which trace */
	if ( (tfptr = tfind(&key, &FilterRoot, compare_delta)) == NULL ) {
	/* Not found in trace table */
		return NULL;
	}

	tfptr = *(TRACE_FILTER **)tfptr;

	return &tfptr->filter;
}

/*
 *
 */
IIR_STAGE *dif2tra_filter_stage_create( const _TRACEINFO *traceptr )
{
	IIR_FILTER *fptr   = traceptr->filter;
	IIR_STAGE  *nstage = NULL;

/* Find which trace */
	if ( fptr->nsects )
		nstage = (IIR_STAGE *)calloc(fptr->nsects, sizeof(IIR_STAGE));

	return nstage;
}

/*
 *
 */
double dif2tra_filter_apply( double sample, _TRACEINFO *traceptr )
{
	return applyfilter( sample, traceptr->filter, traceptr->stage );
}

/*
 * dif2tra_filter_end() - End process of Trace list.
 * Arguments:
 *   None.
 * Returns:
 *   None.
 */
void dif2tra_filter_end( void )
{
	tdestroy(FilterRoot, free);
	return;
}

/*
 * compare_delta() - the delta compare function of binary tree search
 */
static int compare_delta( const void *a, const void *b )
{
	TRACE_FILTER *tmpa, *tmpb;

	tmpa = (TRACE_FILTER *)a;
	tmpb = (TRACE_FILTER *)b;

	if ( fabs(tmpa->delta - tmpb->delta) < FLT_EPSILON )
		return 0;
	else if ( tmpa->delta > tmpb->delta )
		return 1;
	else
		return -1;
}
