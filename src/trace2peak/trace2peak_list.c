/**
 * @file trace2peak_list.c
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University (b98204032@gmail.com)
 * @brief
 * @date 2018-03-20
 *
 * @copyright Copyright (c) 2018-now
 *
 */
#define _GNU_SOURCE
/**
 * @name Standard C header include
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <search.h>
/**
 * @name Earthworm environment header include
 *
 */
#include <earthworm.h>
#include <trace_buf.h>
/**
 * @name Local header include
 *
 */
#include <trace2peak.h>
#include <trace2peak_list.h>

/**
 * @name Functions prototype in this source file
 *
 */
static int  compare_scnl( const void *, const void * );

/**
 * @brief Root of binary tree
 *
 */
static void *Root = NULL;

/**
 * @brief Insert the new trace info to the tree.
 *
 * @param trh2x
 * @return _TRACEPEAK*
 */
_TRACEPEAK *tra2peak_list_search( const TRACE2X_HEADER *trh2x )
{
	_TRACEPEAK *traceptr = NULL;

/* Test if already in the tree */
	if ( !(traceptr = tra2peak_list_find( trh2x )) ) {
		traceptr = (_TRACEPEAK *)calloc(1, sizeof(_TRACEPEAK));
		memcpy(traceptr->sta,  trh2x->sta, TRACE2_STA_LEN);
		memcpy(traceptr->net,  trh2x->net, TRACE2_NET_LEN);
		memcpy(traceptr->loc,  trh2x->loc, TRACE2_LOC_LEN);
		memcpy(traceptr->chan, trh2x->chan, TRACE2_CHAN_LEN);

		if ( !(traceptr = tsearch(traceptr, &Root, compare_scnl)) ) {
		/* Something error when insert into the tree */
			return NULL;
		}

		traceptr = *(_TRACEPEAK **)traceptr;
		traceptr->firsttime = true;
		traceptr->match     = NULL;
	}

	return traceptr;
}

/**
 * @brief Output the trace info that match the SCNL.
 *
 * @param trh2x
 * @return _TRACEPEAK*
 */
_TRACEPEAK *tra2peak_list_find( const TRACE2X_HEADER *trh2x )
{
	_TRACEPEAK  key;
	_TRACEPEAK *traceptr = NULL;

	memcpy(key.sta,  trh2x->sta, TRACE2_STA_LEN);
	memcpy(key.net,  trh2x->net, TRACE2_NET_LEN);
	memcpy(key.loc,  trh2x->loc, TRACE2_LOC_LEN);
	memcpy(key.chan, trh2x->chan, TRACE2_CHAN_LEN);

/* Find which trace */
	if ( !(traceptr = tfind(&key, &Root, compare_scnl)) ) {
	/* Not found in trace table */
		return NULL;
	}

	traceptr = *(_TRACEPEAK **)traceptr;

	return traceptr;
}

/**
 * @brief End process of this trace list service.
 *
 */
void tra2peak_list_end( void )
{
	tdestroy(Root, free);

	return;
}

/**
 * @brief The SCNL compare function of binary tree search.
 *
 * @param a
 * @param b
 * @return int
 */
static int compare_scnl( const void *a, const void *b )
{
	_TRACEPEAK *tmpa = (_TRACEPEAK *)a;
	_TRACEPEAK *tmpb = (_TRACEPEAK *)b;
	int         rc;

	if ( (rc = strcmp(tmpa->sta, tmpb->sta)) != 0 )
		return rc;
	if ( (rc = strcmp(tmpa->chan, tmpb->chan)) != 0 )
		return rc;
	if ( (rc = strcmp(tmpa->net, tmpb->net)) != 0 )
		return rc;
	return strcmp(tmpa->loc, tmpb->loc);
}
