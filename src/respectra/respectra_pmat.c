#define _GNU_SOURCE
/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <search.h>
#include <float.h>
#include <math.h>
/* Earthworm environment header include */
#include <earthworm.h>
#include <trace_buf.h>
/* Local header include */
#include <respectra.h>
#include <respectra_list.h>
#include <respectra_pmat.h>

#define PI  3.141592653589793238462643383279f
#define PI2 6.283185307179586476925286766559f

static PMATRIX design_pmatrix( const double, const double, const double );
static int     compare_delta( const void *, const void * ); /* The compare function of binary tree search */

static void  *PMatrixRoot   = NULL;         /* Root of filter binary tree */
static double _DampingRatio  = 0.0;

/* */
double AngularFreq;
double AFreqSquare;
double DAFreqDamping;

/*
 * rsp_pmat_init( ) -- Initialization function of filter chain.
 * Arguments:
 * Returns:
 *    0 = Normal.
 */
int rsp_pmat_init( const double damping, const double period )
{
	_DampingRatio = damping;

	AngularFreq   = PI2 / period;
	AFreqSquare   = AngularFreq * AngularFreq;
	DAFreqDamping = 2.0 * AngularFreq * _DampingRatio;

	return 0;
}

/*
 * rsp_pmat_search( ) -- Insert the trace to the tree.
 * Arguments:
 *   trh2x = Pointer of the trace you want to find.
 * Returns:
 *    NULL = Didn't find the trace.
 *   !NULL = The Pointer to the trace.
 */
PMATRIX *rsp_pmat_search( const _TRACEINFO *traceptr )
{
	TRACE_PMATRIX *trpmatptr = NULL;
	PMATRIX       *pmatptr = NULL;

/* Test if already in the tree */
	if ( (pmatptr = rsp_pmat_find( traceptr )) == NULL ) {
		trpmatptr          = (TRACE_PMATRIX *)calloc(1, sizeof(TRACE_PMATRIX));
		trpmatptr->delta   = traceptr->delta / traceptr->intsteps;
		trpmatptr->pmatrix = design_pmatrix( _DampingRatio, AngularFreq, trpmatptr->delta );

		if ( (trpmatptr = tsearch(trpmatptr, &PMatrixRoot, compare_delta)) == NULL ) {
		/* Something error when insert into the tree */
			return NULL;
		}

		trpmatptr = *(TRACE_PMATRIX **)trpmatptr;
		pmatptr   = &trpmatptr->pmatrix;
	}

	return pmatptr;
}

/*
 * rsp_pmat_find( ) -- Output the Trace that match the SCNL.
 * Arguments:
 *   trh2x = Pointer of the trace you want to find.
 * Returns:
 *    NULL = Didn't find the trace.
 *   !NULL = The Pointer to the trace.
 */
PMATRIX *rsp_pmat_find( const _TRACEINFO *traceptr )
{
	TRACE_PMATRIX  key;
	TRACE_PMATRIX *trpmatptr = NULL;

	key.delta = traceptr->delta / traceptr->intsteps;
/* Find which trace */
	if ( (trpmatptr = tfind(&key, &PMatrixRoot, compare_delta)) == NULL ) {
	/* Not found in trace table */
		return NULL;
	}

	trpmatptr = *(TRACE_PMATRIX **)trpmatptr;

	return &trpmatptr->pmatrix;
}

/*
 * rsp_pmat_end( ) -- End process of PMatrix list.
 * Arguments:
 *   None.
 * Returns:
 *   None.
 */
void rsp_pmat_end( void )
{
	tdestroy(PMatrixRoot, free);

	return;
}

/*
 *
 */
static PMATRIX design_pmatrix( const double damping, const double afreq, const double delta )
{
	PMATRIX result;
	double  tmp = 0.0;

	const double dampsq  = damping * damping;
	const double afreqsq = afreq * afreq;
	const double dampaf  = damping * afreq;

	const double a0  = exp(-dampaf * delta);
	const double a1  = afreq * sqrt(1.0 - dampsq);
	tmp = a1 * delta;
	const double a2  = sin(tmp);
	const double a3  = cos(tmp);
	const double a4  = (2.0 * dampsq - 1.0) / afreqsq;
	const double a5  = damping / afreq;
	const double a6  = 2.0 * a5 / afreqsq;
	const double a7  = 1.0 / afreqsq;
	const double a8  = a0 * a3;
	const double a9  = a0 / a1 * a2;
	const double a10 = -(a0 * a1 * a2 + dampaf * a8);
	const double a11 = a8 - dampaf * a9;

	result.a[0] = a8 + a9 * dampaf;
	result.a[1] = a9;
	result.a[2] = a10 + a11 * dampaf;
	result.a[3] = a11;

	tmp = (a4 * a9 + a6 * a8 - a6) / delta;
	result.b[0] = -tmp - a5 * a9 - a7 * a8;
	result.b[1] = tmp + a7;
	tmp = (a4 * a11 + a6 * a10 + a7) / delta;
	result.b[2] = -tmp - a5 * a11 - a7 * a10;
	result.b[3] = tmp;

	return result;
}

/*
 * compare_delta() - the delta compare function of binary tree search
 */
static int compare_delta( const void *a, const void *b )
{
	TRACE_PMATRIX *tmpa = (TRACE_PMATRIX *)a;
	TRACE_PMATRIX *tmpb = (TRACE_PMATRIX *)b;

	if ( fabs(tmpa->delta - tmpb->delta) < FLT_EPSILON )
		return 0;
	else if ( tmpa->delta > tmpb->delta )
		return 1;
	else
		return -1;
}
