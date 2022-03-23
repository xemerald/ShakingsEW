#define _GNU_SOURCE
/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <search.h>
/* Earthworm environment header include */
#include <earthworm.h>
#include <trace_buf.h>
/* Local header include */
#include <trace2peak.h>
#include <trace2peak_list.h>

static int  compare_scnl( const void *, const void * );	/* The compare function of binary tree search */

static void *Root = NULL;         /* Root of binary tree */

/*
 * tra2peak_list_search( ) -- Insert the trace to the tree.
 * Arguments:
 *   trh2x = Pointer of the trace you want to find.
 * Returns:
 *    NULL = Didn't find the trace.
 *   !NULL = The Pointer to the trace.
 */
_TRACEPEAK *tra2peak_list_search( const TRACE2X_HEADER *trh2x )
{
	_TRACEPEAK *traceptr = NULL;


/* Test if already in the tree */
	if ( (traceptr = tra2peak_list_find( trh2x )) == NULL ) {
		traceptr = (_TRACEPEAK *)calloc(1, sizeof(_TRACEPEAK));
		memcpy(traceptr->sta,  trh2x->sta, TRACE2_STA_LEN);
		memcpy(traceptr->net,  trh2x->net, TRACE2_NET_LEN);
		memcpy(traceptr->loc,  trh2x->loc, TRACE2_LOC_LEN);
		memcpy(traceptr->chan, trh2x->chan, TRACE2_CHAN_LEN);

		if ( (traceptr = tsearch(traceptr, &Root, compare_scnl)) == NULL ) {
		/* Something error when insert into the tree */
			return NULL;
		}

		traceptr = *(_TRACEPEAK **)traceptr;
		traceptr->firsttime = TRUE;
	}

	return traceptr;
}

/*
 * tra2peak_list_find( ) -- Output the Trace that match the SCNL.
 * Arguments:
 *   trh2x = Pointer of the trace you want to find.
 * Returns:
 *    NULL = Didn't find the trace.
 *   !NULL = The Pointer to the trace.
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
	if ( (traceptr = tfind(&key, &Root, compare_scnl)) == NULL ) {
	/* Not found in trace table */
		return NULL;
	}

	traceptr = *(_TRACEPEAK **)traceptr;

	return traceptr;
}

/*
 * tra2peak_list_end( ) -- End process of Trace list.
 * Arguments:
 *   None.
 * Returns:
 *   None.
 */
void tra2peak_list_end( void )
{
	tdestroy(Root, free);

	return;
}

/*
 * compare_scnl() - the SCNL compare function of binary tree search
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
