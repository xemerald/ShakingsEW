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
#include <stalist.h>


/*--------------------------------------------------------*
 * Definition of trace peak value, total size is 40 bytes *
 *--------------------------------------------------------*/
#define TRACE_PEAKVALUE_SIZE  40

typedef struct {
	char sta[STA_CODE_LEN];   /* Site name (NULL-terminated) */
	char net[NET_CODE_LEN];   /* Network name (NULL-terminated) */
	char loc[LOC_CODE_LEN];   /* Location code (NULL-terminated) */
	char chan[CHAN_CODE_LEN]; /* Component/channel code (NULL-terminated) */

	uint16_t recordtype;      /* Flag for record type   */
	uint8_t  sourcemod;       /* Come from which module */
	uint8_t  padding;         /*  */
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
