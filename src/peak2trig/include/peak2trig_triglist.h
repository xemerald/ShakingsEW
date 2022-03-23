#pragma once

#include <stdint.h>

/* Local header include */
#include <stalist.h>
#include <tracepeak.h>
#include <peak2trig.h>

#define CLUSTER_NUM 2

typedef struct sta_node {
	_STAINFO        *staptr;

	uint16_t         padding;
	uint16_t         recordtype;
	char             peakchan[CHAN_CODE_LEN];
	double           peakvalue;
	double           peaktime;

	struct sta_node *cluster[CLUSTER_NUM];
	struct sta_node *next;
} STA_NODE;

/* Function prototype */
STA_NODE *TrigListInsert( const _STAINFO * );
STA_NODE *TrigListFind( const _STAINFO * );
STA_NODE *TrigListDelete( const _STAINFO * );
STA_NODE *TrigListUpdate( const TRACE_PEAKVALUE *, STA_NODE * );

int TrigListLength( void );
int TrigListCluster( const double, const double );

void TrigListWalk( void (*)(const void *) );
void TrigListTimeFilter( const double );
void TrigListDestroy ( void );

int TrigListPack( void *, size_t );
