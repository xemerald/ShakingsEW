/**
 * @file dif2trace_list.c
 * @author Benjamin Ming Yang (b98204032@gmail.com) @ Department of Geology, National Taiwan University
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
#include <unistd.h>
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
#include <dif2trace.h>
#include <dif2trace_list.h>

/**
 * @name Functions prototype in this source file
 *
 */
static int  compare_scnl( const void *, const void * );
static void free_node( void * );

/**
 * @brief Root of binary tree
 *
 */
static void *Root = NULL;

/**
 * @brief Insert the new trace info to the tree.
 *
 * @param trh2x
 * @return _TRACEINFO*
 */
_TRACEINFO *dif2tra_list_search( const TRACE2X_HEADER *trh2x )
{
	_TRACEINFO *traceptr = NULL;

/* Test if already in the tree */
	if ( !(traceptr = dif2tra_list_find( trh2x )) ) {
		traceptr = (_TRACEINFO *)calloc(1, sizeof(_TRACEINFO));
		memcpy(traceptr->sta,  trh2x->sta, TRACE2_STA_LEN);
		memcpy(traceptr->net,  trh2x->net, TRACE2_NET_LEN);
		memcpy(traceptr->loc,  trh2x->loc, TRACE2_LOC_LEN);
		memcpy(traceptr->chan, trh2x->chan, TRACE2_CHAN_LEN);

		if ( !(traceptr = tsearch(traceptr, &Root, compare_scnl)) ) {
		/* Something error when insert into the tree */
			return NULL;
		}

		traceptr = *(_TRACEINFO **)traceptr;
		traceptr->firsttime = true;
		traceptr->filter    = NULL;
		traceptr->stage     = NULL;
		traceptr->match     = NULL;
	}

	return traceptr;
}

/**
 * @brief Output the trace info that match the SCNL.
 *
 * @param trh2x
 * @return _TRACEINFO*
 */
_TRACEINFO *dif2tra_list_find( const TRACE2X_HEADER *trh2x )
{
	_TRACEINFO  key;
	_TRACEINFO *traceptr = NULL;

	memcpy(key.sta,  trh2x->sta,  TRACE2_STA_LEN);
	memcpy(key.net,  trh2x->net,  TRACE2_NET_LEN);
	memcpy(key.loc,  trh2x->loc,  TRACE2_LOC_LEN);
	memcpy(key.chan, trh2x->chan, TRACE2_CHAN_LEN);

/* Find which trace */
	if ( !(traceptr = tfind(&key, &Root, compare_scnl)) ) {
	/* Not found in trace table */
		return NULL;
	}

	traceptr = *(_TRACEINFO **)traceptr;

	return traceptr;
}

/**
 * @brief End process of this list service.
 *
 */
void dif2tra_list_end( void )
{
	tdestroy(Root, free_node);
	return;
}

/**
 * @brief The SCNL compare function of binary tree search
 *
 * @param a
 * @param b
 * @return int
 */
static int compare_scnl( const void *a, const void *b )
{
	_TRACEINFO *tmpa = (_TRACEINFO *)a;
	_TRACEINFO *tmpb = (_TRACEINFO *)b;
	int         rc;

	if ( (rc = memcmp(tmpa->sta, tmpb->sta, TRACE2_STA_LEN)) != 0 )
		return rc;
	if ( (rc = memcmp(tmpa->chan, tmpb->chan, TRACE2_CHAN_LEN)) != 0 )
		return rc;
	if ( (rc = memcmp(tmpa->net, tmpb->net, TRACE2_NET_LEN)) != 0 )
		return rc;
	return memcmp(tmpa->loc, tmpb->loc, TRACE2_LOC_LEN);
}

/**
 * @brief Free node of binary tree search
 *
 * @param node
 */
static void free_node( void *node )
{
	_TRACEINFO *traceptr = (_TRACEINFO *)node;

	free(traceptr->stage);
	free(traceptr);

	return;
}
