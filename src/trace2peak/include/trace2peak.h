#pragma once
/* */
#include <trace_buf.h>
/* Default filled in peak value type is acc. */
#define DEF_PEAK_VALUE_TYPE   2

/* Trace & peak info related struct */
typedef struct {
	char sta[TRACE2_STA_LEN];    /* Site name (NULL-terminated) */
	char net[TRACE2_NET_LEN];    /* Network name (NULL-terminated) */
	char loc[TRACE2_LOC_LEN];    /* Location code (NULL-terminated) */
	char chan[TRACE2_CHAN_LEN];  /* Component/channel code (NULL-terminated) */

	uint8_t  padding;
	uint8_t  firsttime;
	uint16_t readycount;
	double   delta;
	double   lasttime;
	double   average;

	const void *match;
} _TRACEPEAK;
