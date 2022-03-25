#pragma once

#include <trace_buf.h>

#define NTRIG_INTERVAL_SEC       30
/* */
#define PEAK2TRIG_INFO_FROM_SQL  6
/* */
#define PEAK2TRIG_FIRST_TRIG   0
#define PEAK2TRIG_FOLLOW_TRIG  1
/* Station info related struct */
typedef struct {
	char sta[TRACE2_STA_LEN];   /* Site name (NULL-terminated) */
	char net[TRACE2_NET_LEN];   /* Network name (NULL-terminated) */
	char loc[TRACE2_LOC_LEN];   /* Location code (NULL-terminated) */

	double latitude;      /* Latitude of station */
	double longitude;     /* Longitude of station */
	double elevation;     /* Elevation of station */
} _STAINFO;
