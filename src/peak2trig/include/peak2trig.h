#pragma once

#include <stalist.h>

#define  NTRIG_INTERVAL_SEC  30

/* Station info related struct */
typedef struct {
	char sta[STA_CODE_LEN];   /* Site name (NULL-terminated) */
	char net[NET_CODE_LEN];   /* Network name (NULL-terminated) */
	char loc[LOC_CODE_LEN];   /* Location code (NULL-terminated) */

	double latitude;      /* Latitude of station */
	double longitude;     /* Longitude of station */
	double elevation;     /* Elevation of station */
} _STAINFO;
