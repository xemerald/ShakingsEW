/*
 *
 */
#pragma once
/* */
#include <stdint.h>
/* Local header include */
#include <trace_buf.h>
#include <triglist.h>
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
TRIG_STA *pk2trig_tlist_search( const TRACE_PEAKVALUE * );
TRIG_STA *pk2trig_tlist_find( const TRACE_PEAKVALUE * );
TRIG_STA *pk2trig_tlist_delete( const TRACE_PEAKVALUE * );
TRIG_STA *pk2trig_tlist_update( TRIG_STA * , const TRACE_PEAKVALUE * );

int  pk2trig_tlist_len_get( void );
int  pk2trig_tlist_cluster( const double, const double );
int  pk2trig_tlist_pack( TrigListPacket *, const int );
void pk2trig_tlist_walk( void (*)( TRIG_STA * ) );
void pk2trig_tlist_time_filter( const double );
void pk2trig_tlist_destroy( void );
