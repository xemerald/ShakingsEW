/*
 *
 */
#pragma once
/* */
#include <trace_buf.h>

/*
 *
 */
typedef struct {
	double a[4];
	double b[4];
} PMATRIX;

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
	double      average;
	double      delta;
	double      intsteps;
	double      xmatrix[3];
	PMATRIX    *pmatrix;

	const void *match;
} _TRACEINFO;
