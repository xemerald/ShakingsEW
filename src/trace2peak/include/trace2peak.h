/**
 * @file trace2peak.h
 * @author Benjamin Ming Yang (b98204032@gmail.com) @ Department of Geology, National Taiwan University
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
/* Default filled in peak value type is acc. */
#define DEF_PEAK_VALUE_TYPE  2

/**
 * @brief Trace & peak info related struct
 *
 */
typedef struct {
	char sta[TRACE2_STA_LEN];    /* Site name (NULL-terminated) */
	char net[TRACE2_NET_LEN];    /* Network name (NULL-terminated) */
	char loc[TRACE2_LOC_LEN];    /* Location code (NULL-terminated) */
	char chan[TRACE2_CHAN_LEN];  /* Component/channel code (NULL-terminated) */

	uint8_t  padding;
	bool     firsttime;
	uint16_t readycount;
	double   delta;
	double   lasttime;
	double   average;

	const void *match;
} _TRACEPEAK;
