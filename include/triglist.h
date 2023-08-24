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
 *       -Extra station trigger block (Optional)
 *     -Station trigger information block
 *       -Extra station trigger block (Optional)
 *     -Station trigger information block
 *       -Extra station trigger block (Optional)
 *              .
 *              .
 *              .
 * -End of TYPE_TRIG_LIST
 */

 /* For triggered type flag */
typedef enum {
	TRIG_BY_PEAK_CLUSTER,
	TRIG_BY_PICK_CLUSTER,
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

#define MAX_TRIG_PHASE_LEN  9

/*
 * Definition of trigger list header, total size is 48 bytes
 */
typedef struct {
	uint32_t tid;
	uint32_t trigseq;
	uint16_t trigtype;
	uint16_t codaflag;
	uint32_t ntrigsta;
	uint32_t tsize;
	uint32_t blockseq;
	uint32_t totalblock;
	uint8_t  padding[4];
	double   firsttime;
	double   trigtime;
} TRIGLIST_HEADER;

/*
 * Definition of Station triggered information, total size is 80 bytes
 */
typedef struct {
	char     sta[TRACE2_STA_LEN];        /* Site name (NULL-terminated)     */
	char     net[TRACE2_NET_LEN];        /* Network name (NULL-terminated)  */
	char     loc[TRACE2_LOC_LEN];        /* Location code (NULL-terminated) */
	char     chan[TRACE2_CHAN_LEN];      /* Trigger channel code (NULL-terminated) */
	char     phase[MAX_TRIG_PHASE_LEN];  /* Trigger phase name (NULL-terminated)   */

	double   latitude;    /* Latitude of station */
	double   longitude;   /* Longitude of station */
	double   elevation;   /* Elevation of station */
	double   ptime;       /* Time of peak sample or pick time in epoch seconds      */
	double   pvalue;      /* Peak value in this triggered station of data weighting */

	uint8_t  flag[2];     /* Simple flags for optional usage */
	uint8_t  update_seq;  /* Updating sequence number                               */
	uint8_t  datatype;    /* Flag for peak value record type                        */
	uint16_t extra_size;  /* */
	uint16_t next_pos;    /* */
} TRIGLIST_STATION;

/*
 * Definition of a generic Stations List Packet
 */
#define MAX_TRIGLIST_SIZ     65536  /* define maximum size of trigger list message */
#define MAX_TRIGSTATION_NUM  818    /* define maximum stations inside trigger list message */

typedef union {
    uint8_t         msg[MAX_TRIGLIST_SIZ];
    TRIGLIST_HEADER tlh;
} TrigListPacket;
