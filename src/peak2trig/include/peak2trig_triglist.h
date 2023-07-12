/*
 *
 */
#pragma once
/* */
#include <stdint.h>
/* Local header include */
#include <trace_buf.h>
#include <tracepeak.h>
#include <peak2trig.h>
/* */
#define CLUSTER_NUM 2

typedef struct trig_sta {
	_STAINFO        *staptr;

	uint16_t         padding;
	uint16_t         recordtype;
	char             peakchan[TRACE2_CHAN_LEN];
	double           peakvalue;
	double           peaktime;

	struct trig_sta *cluster[CLUSTER_NUM];
} TRIG_STA;

/* Function prototype */
TRIG_STA *peak2trig_tlist_insert( const _STAINFO * );
TRIG_STA *peak2trig_tlist_find( const _STAINFO * );
TRIG_STA *peak2trig_tlist_delete( const _STAINFO * );
TRIG_STA *peak2trig_tlist_update( const TRACE_PEAKVALUE *, TRIG_STA * );

int  peak2trig_tlist_len_get( void );
int  peak2trig_tlist_cluster( const double, const double );
int  peak2trig_tlist_pack( void *, size_t, const uint8_t );
void peak2trig_tlist_walk( void (*)(const void *) );
void peak2trig_tlist_time_filter( const double );
void peak2trig_tlist_destroy( void );
