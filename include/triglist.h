/*
 * triglist.h
 *
 * Header file for trigger list data.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * November, 2018
 *
 */

#pragma once

#include <time.h>
#include <stdint.h>
/* */
#include <trace_buf.h>

/*
 * Definition of structure of TYPE_TRIG_LIST:
 *
 * -Trigger list header
 *     -Station trigger information block
 *     -Station trigger information block
 *     -Station trigger information block
 *              .
 *              .
 *              .
 * -End of TYPE_TRIG_LIST
 */

 /* For triggered type flag */
 typedef enum {
 	TRIG_BY_HYPO,
	TRIG_BY_PEAK_CLUSTER,
	/* Maybe else... */

/* Should always be the last */
	TRIG_TYPE_COUNT
} TRIG_TYPE;

/* For coda status flag */
typedef enum {
	NO_CODA,
	IS_CODA,
	/* Maybe else... */

/* Should always be the last */
	CODA_FLAG_COUNT
} CODA_FLAG;

/*
 * Definition of trigger list header, total size is 48 bytes
 */
typedef struct {
	uint16_t trigtype;
	uint16_t codaflag;
	uint32_t trigstations;
	double   origintime;
	double   trigtime;
	double   latitude;
	double   longitude;
	double   depth;
} TRIGLIST_HEADER;

/*
 * Definition of Station triggered information, total size is 48 bytes
 */
typedef struct {
	char     sta[TRACE2_STA_LEN];   /* Site name (NULL-terminated)     */
	char     net[TRACE2_NET_LEN];   /* Network name (NULL-terminated)  */
	char     loc[TRACE2_LOC_LEN];   /* Location code (NULL-terminated) */
	char     pchan[TRACE2_CHAN_LEN];

	uint8_t  padding[5];   /* Padding? */
	uint16_t seq;          /* Sequence number of station      */
	uint16_t datatype;     /* Flag for record type            */
	double   ptime;        /* Time of peak sample or pick time in epoch seconds               */
	double   pvalue;       /* Peak value in this triggered station or weighting for this pick */
} TRIG_STATION;

/*
 * Definition of a generic Stations List Packet
 */
#define MAX_TRIGLIST_SIZ  65536  /* define maximum size of trigger list message */

typedef union {
    uint8_t         msg[MAX_TRIGLIST_SIZ];
    TRIGLIST_HEADER tlh;
} TrigListPacket;

#define TRIGLIST_SIZE_GET(TLH) \
		(sizeof(TRIGLIST_HEADER) + (TLH)->trigstations * sizeof(TRIG_STATION))
