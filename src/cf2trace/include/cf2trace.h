/*
 * cf2trace.h
 *
 * Header file for unified plateform station list data.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * March, 2018
 *
 */
#pragma once

#include <trace_buf.h>
/* */
#define CF2TRA_INFO_FROM_SQL  6
/* Trace info related struct */
typedef struct {
	uint16_t seq;
	uint16_t recordtype;

	char sta[TRACE2_STA_LEN];
	char net[TRACE2_NET_LEN];
	char loc[TRACE2_LOC_LEN];
	char chan[TRACE2_CHAN_LEN];

	float conversion_factor;
} _TRACEINFO;
