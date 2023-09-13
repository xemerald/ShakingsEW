/*
 *
 */
#pragma once
#include <stdint.h>
/* Local header include */
#include <trace_buf.h>


#define COMPOSITION_CHANNELS  3
#define MAX_VSBUF_SAMPLES     4096

/* Trace info related struct */
typedef struct {
	char sta[TRACE2_STA_LEN];   /* Site name (NULL-terminated) */
	char net[TRACE2_NET_LEN];   /* Network name (NULL-terminated) */
	char loc[TRACE2_LOC_LEN];   /* Location code (NULL-terminated) */
	char chan[TRACE2_CHAN_LEN]; /* Component/channel code (NULL-terminated) */

	int         comp_seq;
	double      lasttime;
	void       *vsi;
	const void *match;
} _TRACEINFO;

typedef struct {
	char sta[TRACE2_STA_LEN];   /* Site name (NULL-terminated) */
	char net[TRACE2_NET_LEN];   /* Network name (NULL-terminated) */
	char loc[TRACE2_LOC_LEN];   /* Location code (NULL-terminated) */
	char chan[TRACE2_CHAN_LEN]; /* Component/channel code (NULL-terminated) */
	
	int    nsamp[COMPOSITION_CHANNELS];
	double delta;
	double samprate;
	double lasttime;
	double starttime;
	double endtime[COMPOSITION_CHANNELS];
	double buffer[MAX_VSBUF_SAMPLES];
} VECSUM_INFO;