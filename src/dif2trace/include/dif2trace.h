/**
 * @file dif2trace.h
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University (b98204032@gmail.com)
 * @brief
 * @date 2018-03-20
 *
 * @copyright Copyright (c) 2018-now
 *
 */
#pragma once
/**
 * @name Standard C header include
 *
 */
#include <stdint.h>
#include <stdbool.h>
/**
 * @name Earthworm environment header include
 *
 */
#include <trace_buf.h>
/**
 * @name Local header include
 *
 */
#include <iirfilter.h>

/**
 * @brief Trace info related struct
 *
 */
typedef struct {
	char sta[TRACE2_STA_LEN];   /* Site name (NULL-terminated) */
	char net[TRACE2_NET_LEN];   /* Network name (NULL-terminated) */
	char loc[TRACE2_LOC_LEN];   /* Location code (NULL-terminated) */
	char chan[TRACE2_CHAN_LEN]; /* Component/channel code (NULL-terminated) */

	uint8_t     padding;
	bool        firsttime;
	uint16_t    readycount;
	double      lasttime;
	double      lastsample[3];
	double      average;
	double      delta;
	IIR_STAGE  *stage;
	IIR_FILTER *filter;

	const void *match;
} _TRACEINFO;
