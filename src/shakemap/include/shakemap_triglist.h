/*
 *
 */
#pragma once
/* */
#include <time.h>
/* Local header include */
#include <tracepeak.h>
#include <shakemap.h>
/* */
typedef struct sta_node {
	_STAINFO        *staptr;
	STA_SHAKE        shakeinfo;
	struct sta_node *next;
} STA_NODE;

/* Function prototype */
STA_NODE *shakemap_tlist_insert( const _STAINFO * );
STA_NODE *shakemap_tlist_find( const _STAINFO * );
STA_NODE *shakemap_tlist_delete( const _STAINFO * );

int  shakemap_tlist_len_get( void );
int  shakemap_tlist_pack( void *, size_t );
void shakemap_tlist_pvalue_update( void );
void shakemap_tlist_walk( void (*)(const void *) );
void shakemap_tlist_time_sync( const time_t );
void shakemap_tlist_destroy( void );
