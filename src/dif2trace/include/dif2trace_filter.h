/**
 * @file dif2trace_filter.h
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University (b98204032@gmail.com)
 * @brief
 * @date 2018-03-20
 *
 * @copyright Copyright (c) 2018-now
 *
 */
#pragma once
/**
 * @name Local header include
 *
 */
#include <iirfilter.h>
#include <dif2trace.h>

/**
 * @brief Filter node structure
 *
 */
typedef struct {
	double     delta;
	IIR_FILTER filter;
} TRACE_FILTER;

/**
 * @name External functions prototypes
 *
 */
int         dif2tra_filter_init( const int, const double, const double, const int, const int );
IIR_FILTER *dif2tra_filter_search( const _TRACEINFO * );
IIR_FILTER *dif2tra_filter_find( const _TRACEINFO * );
IIR_STAGE  *dif2tra_filter_stage_create( const _TRACEINFO * );
double      dif2tra_filter_apply( const double, _TRACEINFO * );
void        dif2tra_filter_end( void );
