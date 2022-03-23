#pragma once

#include <stalist.h>

#define  SHAKE_BUF_LEN    30
#define  TRIGGER_MIN_INT  1

/* Shaking information related struct */
typedef struct {
	uint16_t recordtype;
	uint8_t  padding[2];
	char     peakchan[CHAN_CODE_LEN];
	double   peakvalue;
	double   peaktime;
} STA_SHAKE;

/* Station info related struct */
typedef struct {
	char sta[STA_CODE_LEN];   /* Site name (NULL-terminated) */
	char net[NET_CODE_LEN];   /* Network name (NULL-terminated) */
	char loc[LOC_CODE_LEN];   /* Location code (NULL-terminated) */

	double latitude;      /* Latitude of station */
	double longitude;     /* Longitude of station */
	double elevation;     /* Elevation of station */

	int       shakelatest;
	STA_SHAKE shakeinfo[SHAKE_BUF_LEN];
} _STAINFO;

/* Shake list element */
typedef struct {
	_STAINFO  *staptr;
	STA_SHAKE  shakeinfo;
} SHAKE_LIST_ELEMENT;

/* Shake list header */
typedef struct {
	time_t   starttime;
	time_t   endtime;
	uint32_t totalstations;
	uint16_t trigtype;
	uint16_t codaflag;
} SHAKE_LIST_HEADER;
