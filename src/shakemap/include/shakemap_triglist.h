#pragma once

#include <stdint.h>

/* Local header include */
#include <stalist.h>
#include <tracepeak.h>
#include <shakemap.h>

typedef struct sta_node {
	_STAINFO        *staptr;
	STA_SHAKE        shakeinfo;
	struct sta_node *next;
} STA_NODE;

/* Function prototype */
STA_NODE *TrigListInsert( const _STAINFO * );
STA_NODE *TrigListFind( const _STAINFO * );
STA_NODE *TrigListDelete( const _STAINFO * );

int TrigListLength( void );

void TrigListPeakValUpd( void );
void TrigListWalk( void (*)(const void *) );
void TrigListTimeSync( const time_t );
void TrigListDestroy( void );

int TrigListPack( void *, size_t );
