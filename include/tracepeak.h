/*
 *     Revision history:
 *
 *     Revision 1.0  2018/10/26 16:08:50  Benjamin Yang
 *     Initial revision
 *
 */

/*
 * trace_peak.h
 *
 * Header file for trace peak value data.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * October, 2018
 *
 */

#pragma once

#include <stdint.h>
#include <trace_buf.h>


/*--------------------------------------------------------*
 * Definition of trace peak value, total size is 40 bytes *
 *--------------------------------------------------------*/
#define TRACE_PEAKVALUE_SIZE  48

typedef struct {
	char sta[TRACE2_STA_LEN];   /* Site name (NULL-terminated) */
	char net[TRACE2_NET_LEN];   /* Network name (NULL-terminated) */
	char loc[TRACE2_LOC_LEN];   /* Location code (NULL-terminated) */
	char chan[TRACE2_CHAN_LEN]; /* Component/channel code (NULL-terminated) */

	uint8_t  padding[6];      /*  */
	uint8_t  sourcemod;       /* Come from which module */
	uint16_t recordtype;      /* Flag for record type   */
	double   peakvalue;       /* Peak value in this trace */
	double   peaktime;        /* Time of peak sample in epoch seconds */
} TRACE_PEAKVALUE;

/*----------------------------------------------*
 * Definition of a generic Stations List Packet *
 *----------------------------------------------*/
typedef union {
    uint8_t         msg[TRACE_PEAKVALUE_SIZE];
    TRACE_PEAKVALUE tpv;
} TracePeakPacket;
