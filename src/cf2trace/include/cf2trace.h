/*
 *     Revision history:
 *
 *     Revision 1.1  2020/03/06 19:29:20  Benjamin Yang
 *     Modify the type of samprate & conversion_factor to float
 *
 *     Revision 1.0  2018/03/19 16:17:44  Benjamin Yang
 *     Initial revision
 *
 */

/*
 * cf2trace.h
 *
 * Header file for unified plateform station list data.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * March, 2018
 *
 */
#pragma once

#include <stalist.h>

/* Trace info related struct */
typedef struct {
	uint16_t seq;
	uint16_t recordtype;

	char sta[STA_CODE_LEN];
	char net[NET_CODE_LEN];
	char loc[LOC_CODE_LEN];
	char chan[CHAN_CODE_LEN];

	float samprate;
	float conversion_factor;
} _TRACEINFO;
