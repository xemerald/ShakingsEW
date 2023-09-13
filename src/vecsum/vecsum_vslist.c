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
#include <vecsum_vslist.h>

typedef struct {
	char ochan[TRACE2_STA_LEN];
	char vschan[TRACE2_STA_LEN];
} VS_CHAN_PAIR;

static int   compare_scnl( const void *, const void * );	/* The compare function of binary tree search */
static int   compare_ochan( const void *, const void * );	/* The compare function of binary tree search */
static char *remap_vs_channel( const char * );	/*  */
static void  free_node( void * );

static void *VSCP_Root = NULL;         /* Root for vector sum channels */
static void *VS_Root = NULL;         /* Root of binary tree */


/*
 * vecsum_vslist_search() - Insert the trace to the tree.
 * Arguments:
 *   trh2x = Pointer of the trace you want to find.
 * Returns:
 *    NULL = Didn't find the trace.
 *   !NULL = The Pointer to the trace.
 */
int vecsum_vslist_vs_reg( const char *vschan, const char *comp1, const char *comp2, const char *comp3 )
{
	VS_CHAN_PAIR *vscp;
	const char   *_comp[COMPOSITION_CHANNELS] = { comp1, comp2, comp3 };

/* Find which trace */
	for ( int i = 0; i < COMPOSITION_CHANNELS; i++ ) {
		vscp = (VS_CHAN_PAIR *)calloc(1, sizeof(VS_CHAN_PAIR));
		memcpy(vscp->vschan, vschan, TRACE2_CHAN_LEN);
		memcpy(vscp->ochan, _comp[i], TRACE2_CHAN_LEN);

		if ( tfind(vscp, &VSCP_Root, compare_ochan) == NULL ) {
			if ( tsearch(vscp, &VSCP_Root, compare_ochan) == NULL ) {
				goto exception;
			}
		}
		else {
			goto exception;
		}
	}

	return 0;

exception:
	if ( vscp ) 
		free(vscp);
	return -1;
}

/*
 * vecsum_vslist_search() - Insert the trace to the tree.
 * Arguments:
 *   trh2x = Pointer of the trace you want to find.
 * Returns:
 *    NULL = Didn't find the trace.
 *   !NULL = The Pointer to the trace.
 */
VECSUM_INFO *vecsum_vslist_search( const TRACE2X_HEADER *trh2x )
{
	VECSUM_INFO *vsptr = NULL;
	const char *vschan = remap_vs_channel( trh2x->chan );

/* Test if already in the tree & has its vector sum channel code */
	if ( (vsptr = vecsum_vslist_find( trh2x )) == NULL && vschan ) {
		vsptr = (VECSUM_INFO *)calloc(1, sizeof(VECSUM_INFO));
		memcpy(vsptr->sta, trh2x->sta, TRACE2_STA_LEN);
		memcpy(vsptr->net, trh2x->net, TRACE2_NET_LEN);
		memcpy(vsptr->loc, trh2x->loc, TRACE2_LOC_LEN);
		memcpy(vsptr->chan, vschan, TRACE2_CHAN_LEN);

		if ( tsearch(vsptr, &VS_Root, compare_scnl) == NULL ) {
		/* Something error when insert into the tree */
			free(vsptr);
			return NULL;
		}

		vsptr->delta = 1.0 / trh2x->samprate;
		vsptr->samprate = trh2x->samprate;
		vsptr->lasttime = trh2x->endtime;
	}

	return vsptr;
}

/*
 * vecsum_vslist_find( ) -- Output the Trace that match the SCNL.
 * Arguments:
 *   trh2x = Pointer of the trace you want to find.
 * Returns:
 *    NULL = Didn't find the trace.
 *   !NULL = The Pointer to the trace.
 */
VECSUM_INFO *vecsum_vslist_find( const TRACE2X_HEADER *trh2x )
{
	VECSUM_INFO  key;
	VECSUM_INFO *vsptr = NULL;
	const char  *vschan = remap_vs_channel( trh2x->chan );

	if ( vschan ) {
		memcpy(key.sta, trh2x->sta, TRACE2_STA_LEN);
		memcpy(key.net, trh2x->net, TRACE2_NET_LEN);
		memcpy(key.loc, trh2x->loc, TRACE2_LOC_LEN);
		memcpy(key.chan, vschan, TRACE2_CHAN_LEN);
	/* Find which trace */
		if ( (vsptr = tfind(&key, &VS_Root, compare_scnl)) == NULL ) {
		/* Not found in trace table */
			return NULL;
		}
		vsptr = *(VECSUM_INFO **)vsptr;
	}

	return vsptr;
}

/*
 * vecsum_tlist_end() - End process of Trace list.
 * Arguments:
 *   None.
 * Returns:
 *   None.
 */
void vecsum_vslist_end( void )
{
	tdestroy(VS_Root, free_node);
	tdestroy(VSCP_Root, free);

	return;
}

/**
 * @brief 
 * 
 * @param ochan 
 * @return const char* 
 */
static const char *remap_vs_channel( const char *ochan )
{	
	VS_CHAN_PAIR  key;
	VS_CHAN_PAIR *vscp;
	
	memcpy(key.ochan, ochan, TRACE2_CHAN_LEN);

	if ( (vscp = tfind(&key, &VSCP_Root, compare_ochan)) == NULL )
		return NULL;

	return (*(VS_CHAN_PAIR **)vscp)->vschan;	
}

/*
 * compare_scnl() - the SCNL compare function of binary tree search
 */
static int compare_scnl( const void *a, const void *b )
{
	VECSUM_INFO *tmpa = (VECSUM_INFO *)a;
	VECSUM_INFO *tmpb = (VECSUM_INFO *)b;
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
 * compare_scnl() - the SCNL compare function of binary tree search
 */
static int compare_ochan( const void *a, const void *b )
{
	VS_CHAN_PAIR *tmpa = (VS_CHAN_PAIR *)a;
	VS_CHAN_PAIR *tmpb = (VS_CHAN_PAIR *)b;
	int         rc;

	return memcmp(tmpa->ochan, tmpb->ochan, TRACE2_LOC_LEN);
}

/*
 * free_node() - free node of binary tree search
 */
static void free_node( void *node )
{
	VECSUM_INFO *traceptr = (VECSUM_INFO *)node;

	free(traceptr);

	return;
}
