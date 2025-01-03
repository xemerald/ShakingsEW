/*
 *
 */
#pragma once

#include <trace_buf.h>

/* */
#define LATENCY_THRESHOLD       5
#define MAX_PATH_LENGTH         512
#define MAX_HOST_LENGTH         256
#define MAX_FIELD_LENGTH        32
#define MAX_PREFIX_LENGTH       16
#define MAX_TYPE_PEAKVALUE      8
#define MAX_TYPE_INTENSITY      8
#define NULL_PEAKVALUE          -1.0f
#define NULL_PEAKVALUE_ARRAY    { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f }
/* */
#define DEFAULT_REPORT_INTERVAL    30
#define SKREP_TABLE_NAME_FORMAT    "%s:%ld"
#define SKREP_FIELD_NAME_FORMAT    "%s_%s_%s"
/* */
#define MAX_RECORDS_STR_LEN    4096
#define MAX_RECORDS_PER_RDREC  128
#define MIN_RECORDS_PER_RDREC  8

/* Station peak value related struct */
typedef struct {
	char    sta[TRACE2_STA_LEN];   /* Site name (NULL-terminated) */
	char    net[TRACE2_NET_LEN];   /* Network name (NULL-terminated) */
	char    loc[TRACE2_LOC_LEN];   /* Location code (NULL-terminated) */
/* */
	void   *chlist_root;
	void   *chlist[MAX_TYPE_PEAKVALUE];
/* Peak value */
	double  ptime[MAX_TYPE_PEAKVALUE];     /* Time of peak sample in epoch seconds  */
	double  pvalue[MAX_TYPE_PEAKVALUE];    /* Realtime peak value of each station   */
	uint8_t intensity[MAX_TYPE_INTENSITY]; /* Different intensities of each station */
} STATION_PEAK;

/* Channel peak value related struct */
typedef struct {
	char chan[TRACE2_CHAN_LEN];   /* Channel name (NULL-terminated) */
/* Peak value */
	double ptime;               /* Time of peak sample in epoch seconds  */
	double pvalue;              /* Realtime peak value of each station   */
/* */
	const void *match;
} CHAN_PEAK;

/* */
typedef struct {
	char   table[MAX_FIELD_LENGTH];
	char   field[MAX_FIELD_LENGTH];
	double value;
} SHAKE_RECORD;
/* */
typedef struct {
	char   records[MAX_RECORDS_STR_LEN];
	char  *npos;
	int    record_num;
	double timestamp;
} REDIS_RECORDS;
