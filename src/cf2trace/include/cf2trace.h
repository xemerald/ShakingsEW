/**
 * @file cf2trace.h
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University (b98204032@gmail.com)
 * @brief
 * @date 2018-03-20
 *
 * @copyright Copyright (c) 2018-now
 *
 */
#pragma once

/**
 * @name For trace2 header information
 *
 */
#include <trace_buf.h>
/**
 * @name For information request from SQL server, used by dblist library.
 *
 */
#define CF2TRA_INFO_FROM_SQL  6
/**
 * @name For 'FloatPrecision' setting
 *
 */
#define CF2TRA_FLOAT_PRECISION_TABLE \
		X(CF2TRA_FLOAT_SINGLE, "single" ) \
		X(CF2TRA_FLOAT_DOUBLE, "double" ) \
		X(CF2TRA_FLOAT_COUNT,  "NULL"   )

#define X(a, b) a,
typedef enum {
	CF2TRA_FLOAT_PRECISION_TABLE
} CF2TRA_FLOAT_PRECISION_LIST;
#undef X

/**
 * @name Trace info related struct
 *
 */
typedef struct {
	uint16_t seq;
	uint16_t recordtype;

	char sta[TRACE2_STA_LEN];
	char net[TRACE2_NET_LEN];
	char loc[TRACE2_LOC_LEN];
	char chan[TRACE2_CHAN_LEN];

	double conversion_factor;
} _TRACEINFO;
