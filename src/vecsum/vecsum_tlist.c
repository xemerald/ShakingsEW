#define _GNU_SOURCE
/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <search.h>
#include <unistd.h>
/* Earthworm environment header include */
#include <earthworm.h>
#include <trace_buf.h>
/* Local header include */
#include <vecsum.h>
#include <vecsum_tlist.h>

static int  compare_scnl( const void *, const void * );	/* The compare function of binary tree search */
static void free_node( void * );

static void *Root = NULL;         /* Root of binary tree */

/*
 * vecsum_tlist_search() - Insert the trace to the tree.
 * Arguments:
 *   trh2x = Pointer of the trace you want to find.
 * Returns:
 *    NULL = Didn't find the trace.
 *   !NULL = The Pointer to the trace.
 */
_TRACEINFO *vecsum_tlist_search( const TRACE2X_HEADER *trh2x )
{
	_TRACEINFO *traceptr = NULL;

/* Test if already in the tree */
	if ( (traceptr = vecsum_tlist_find( trh2x )) == NULL ) {
		traceptr = (_TRACEINFO *)calloc(1, sizeof(_TRACEINFO));
		memcpy(traceptr->sta,  trh2x->sta, TRACE2_STA_LEN);
		memcpy(traceptr->net,  trh2x->net, TRACE2_NET_LEN);
		memcpy(traceptr->loc,  trh2x->loc, TRACE2_LOC_LEN);
		memcpy(traceptr->chan, trh2x->chan, TRACE2_CHAN_LEN);

		if ( tsearch(traceptr, &Root, compare_scnl) == NULL ) {
		/* Something error when insert into the tree */
			free(traceptr);
			return NULL;
		}

		traceptr->vsi   = NULL;
		traceptr->match = NULL;
	}

	return traceptr;
}

/*
 * vecsum_tlist_find( ) -- Output the Trace that match the SCNL.
 * Arguments:
 *   trh2x = Pointer of the trace you want to find.
 * Returns:
 *    NULL = Didn't find the trace.
 *   !NULL = The Pointer to the trace.
 */
_TRACEINFO *vecsum_tlist_find( const TRACE2X_HEADER *trh2x )
{
	_TRACEINFO  key;
	_TRACEINFO *traceptr = NULL;

	memcpy(key.sta,  trh2x->sta, TRACE2_STA_LEN);
	memcpy(key.net,  trh2x->net, TRACE2_NET_LEN);
	memcpy(key.loc,  trh2x->loc, TRACE2_LOC_LEN);
	memcpy(key.chan, trh2x->chan, TRACE2_CHAN_LEN);

/* Find which trace */
	if ( (traceptr = tfind(&key, &Root, compare_scnl)) == NULL ) {
	/* Not found in trace table */
		return NULL;
	}

	traceptr = *(_TRACEINFO **)traceptr;

	return traceptr;
}

/*
 * vecsum_tlist_end() - End process of Trace list.
 * Arguments:
 *   None.
 * Returns:
 *   None.
 */
void vecsum_tlist_end( void )
{
	tdestroy(Root, free_node);
	return;
}

/*
 * compare_scnl() - the SCNL compare function of binary tree search
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

/*
 * free_node() - free node of binary tree search
 */
static void free_node( void *node )
{
	_TRACEINFO *traceptr = (_TRACEINFO *)node;

	free(traceptr);

	return;
}
