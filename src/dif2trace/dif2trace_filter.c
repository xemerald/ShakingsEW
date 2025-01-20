/**
 * @file dif2trace_filter.c
 * @author Benjamin Ming Yang (b98204032@gmail.com) @ Department of Geology, National Taiwan University
 * @brief
 * @date 2018-03-20
 *
 * @copyright Copyright (c) 2018-now
 *
 */
#define _GNU_SOURCE
/**
 * @name Standard C header include
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <search.h>
#include <float.h>
#include <math.h>
/**
 * @name Earthworm environment header include
 *
 */
#include <earthworm.h>
#include <trace_buf.h>
/**
 * @name Local header include
 *
 */
#include <iirfilter.h>
#include <dif2trace.h>
#include <dif2trace_list.h>
#include <dif2trace_filter.h>

/**
 * @name Functions prototype in this source file
 *
 */
static int compare_delta( const void *, const void * );  /* The compare function of binary tree search */

/**
 * @name
 *
 */
static void  *FilterRoot  = NULL;         /* Root of filter binary tree */
static int    FilterOrder = 1;
static double FreqHigh    = 0.0;
static double FreqLow     = 0.0;

static IIR_FILTER_TYPE      FilterType  = IIR_FILTER_TYPE_COUNT;
static IIR_ANALOG_PROTOTYPE AnalogPtype = IIR_ANALOG_PROTOTYPE_COUNT;

/**
 * @brief Initialization function of filter chain.
 *
 * @param order
 * @param freql
 * @param freqh
 * @param filtertype
 * @param analogptype
 * @return int
 */
int dif2tra_filter_init(
	const int order, const double freql, const double freqh, const int filtertype, const int analogptype
) {
	FilterOrder = order;
	FreqLow     = freql;
	FreqHigh    = freqh;
	FilterType  = filtertype;
	AnalogPtype = analogptype;
/* */
	if (
		FilterType < 0 || FilterType >= IIR_FILTER_TYPE_COUNT ||
		AnalogPtype < 0 || AnalogPtype >= IIR_ANALOG_PROTOTYPE_COUNT
	) {
		return -1;
	}

	return 0;
}

/**
 * @brief Try to find out is there the filter that match the trace's delta.
 *        If not, create a new one & insert to the tree.
 *
 * @param traceptr
 * @return IIR_FILTER*
 */
IIR_FILTER *dif2tra_filter_search( const _TRACEINFO *traceptr )
{
	TRACE_FILTER *trfptr = NULL;
	IIR_FILTER   *filterptr = NULL;

/* Test if already in the tree */
	if ( !(filterptr = dif2tra_filter_find( traceptr )) ) {
		trfptr = (TRACE_FILTER *)calloc(1, sizeof(TRACE_FILTER));
		trfptr->delta  = traceptr->delta;
		trfptr->filter = iirfilter_design( FilterOrder, FilterType, AnalogPtype, FreqLow, FreqHigh, trfptr->delta );

		if ( !(trfptr = tsearch(trfptr, &FilterRoot, compare_delta)) ) {
		/* Something error when insert into the tree */
			return NULL;
		}

		trfptr    = *(TRACE_FILTER **)trfptr;
		filterptr = &trfptr->filter;
	}

	return filterptr;
}

/**
 * @brief Output the filter that match the trace's delta.
 *
 * @param traceptr Pointer of the trace you want to find.
 * @return IIR_FILTER*
 */
IIR_FILTER *dif2tra_filter_find( const _TRACEINFO *traceptr )
{
	TRACE_FILTER  key;
	TRACE_FILTER *tfptr = NULL;

	key.delta = traceptr->delta;

/* Find which trace */
	if ( !(tfptr = tfind(&key, &FilterRoot, compare_delta)) ) {
	/* Not found in trace table */
		return NULL;
	}

	tfptr = *(TRACE_FILTER **)tfptr;

	return &tfptr->filter;
}

/**
 * @brief
 *
 * @param traceptr
 * @return IIR_STAGE*
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

/**
 * @brief
 *
 * @param sample
 * @param traceptr
 * @return double
 */
double dif2tra_filter_apply( const double sample, _TRACEINFO *traceptr )
{
	return iirfilter_apply( sample, traceptr->filter, traceptr->stage );
}

/**
 * @brief End process of Trace list.
 *
 */
void dif2tra_filter_end( void )
{
	tdestroy(FilterRoot, free);
	return;
}

/**
 * @brief The delta compare function of binary tree search.
 *
 * @param a
 * @param b
 * @return int
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
