/*
 *
 */
#pragma once
/* */
#include <trace_buf.h>

/* */
#define  LATENCY_THRESHOLD       5
#define  DEFAULT_WS_PORT         9999
#define  MAX_EX_STATION_STRING   32768
#define  MAX_TYPE_PEAKVALUE      8
#define  MAX_TYPE_INTENSITY      8
#define  NULL_PEAKVALUE          -1.0f
#define  NULL_PEAKVALUE_ARRAY    { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f }

/* Station peak value related struct */
typedef struct {
	char    sta[TRACE2_STA_LEN];   /* Site name (NULL-terminated) */
	char    net[TRACE2_NET_LEN];   /* Network name (NULL-terminated) */
	char    loc[TRACE2_LOC_LEN];   /* Location code (NULL-terminated) */
	void   *chlist[MAX_TYPE_PEAKVALUE];
/* Peak value */
	double  ptime[MAX_TYPE_PEAKVALUE];     /* Time of peak sample in epoch seconds  */
	double  pvalue[MAX_TYPE_PEAKVALUE];    /* Realtime peak value of each station   */
	uint8_t intensity[MAX_TYPE_INTENSITY]; /* Different intensities of each station */
} STATION_PEAK;

/* Channel peak value related struct */
typedef struct {
	char    chan[TRACE2_CHAN_LEN];   /* Channel name (NULL-terminated) */
/* Peak value */
	double  ptime;               /* Time of peak sample in epoch seconds  */
	double  pvalue;              /* Realtime peak value of each station   */
} CHAN_PEAK;
