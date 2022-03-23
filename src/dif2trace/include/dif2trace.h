/*
 *
 */
#pragma once
#include <stdint.h>
/* Local header include */
#include <trace_buf.h>
#include <iirfilter.h>

/* Trace info related struct */
typedef struct {
	char sta[TRACE2_STA_LEN];   /* Site name (NULL-terminated) */
	char net[TRACE2_NET_LEN];   /* Network name (NULL-terminated) */
	char loc[TRACE2_LOC_LEN];   /* Location code (NULL-terminated) */
	char chan[TRACE2_CHAN_LEN]; /* Component/channel code (NULL-terminated) */

	uint8_t     padding;
	uint8_t     firsttime;
	uint16_t    readycount;
	double      lasttime;
	double      lastsample;
	double      lastresult;
	double      average;
	double      delta;
	IIR_STAGE  *stage;
	IIR_FILTER *filter;
} _TRACEINFO;
