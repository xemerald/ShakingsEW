/**
 * @file respectra_pmat.c
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
#include <search.h>
#include <float.h>
#include <math.h>
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
#include <respectra.h>
#include <respectra_list.h>
#include <respectra_pmat.h>

/**
 * @name Pi-related definitions
 *
 */
#define PI  3.141592653589793238462643383279f
#define PI2 6.283185307179586476925286766559f

/**
 * @name Functions prototype in this source file
 *
 */
static PMATRIX design_pmatrix( const double, const double, const double );
static int     compare_delta( const void *, const void * ); /* The compare function of binary tree search */

/**
 * @brief Root of matrix binary tree
 *
 */
static void  *PMatrixRoot   = NULL;
static double _DampingRatio  = 0.0;

/**
 * @name Used parameters.
 *
 */
double AngularFreq;
double AFreqSquare;
double DAFreqDamping;

/**
 * @brief Initialize the parameters.
 *
 * @param damping
 * @param period
 * @return int
 */
int rsp_pmat_init( const double damping, const double period )
{
	_DampingRatio = damping;

	AngularFreq   = PI2 / period;
	AFreqSquare   = AngularFreq * AngularFreq;
	DAFreqDamping = 2.0 * AngularFreq * _DampingRatio;

	return 0;
}

/**
 * @brief Find the PMatrix that match the trace's delta.
 *        If not, insert a new PMatrix to the tree.
 *
 * @param traceptr
 * @return PMATRIX*
 */
PMATRIX *rsp_pmat_search( const _TRACEINFO *traceptr )
{
	TRACE_PMATRIX *trpmatptr = NULL;
	PMATRIX       *pmatptr = NULL;

/* Test if already in the tree */
	if ( !(pmatptr = rsp_pmat_find( traceptr )) ) {
		trpmatptr          = (TRACE_PMATRIX *)calloc(1, sizeof(TRACE_PMATRIX));
		trpmatptr->delta   = traceptr->delta / traceptr->intsteps;
		trpmatptr->pmatrix = design_pmatrix( _DampingRatio, AngularFreq, trpmatptr->delta );

		if ( !(trpmatptr = tsearch(trpmatptr, &PMatrixRoot, compare_delta)) ) {
		/* Something error when insert into the tree */
			return NULL;
		}

		trpmatptr = *(TRACE_PMATRIX **)trpmatptr;
		pmatptr   = &trpmatptr->pmatrix;
	}

	return pmatptr;
}

/**
 * @brief Output the PMatrix that match the trace's delta.
 *
 * @param traceptr
 * @return PMATRIX*
 */
PMATRIX *rsp_pmat_find( const _TRACEINFO *traceptr )
{
	TRACE_PMATRIX  key;
	TRACE_PMATRIX *trpmatptr = NULL;

	key.delta = traceptr->delta / traceptr->intsteps;
/* Find which trace */
	if ( !(trpmatptr = tfind(&key, &PMatrixRoot, compare_delta)) ) {
	/* Not found in trace table */
		return NULL;
	}

	trpmatptr = *(TRACE_PMATRIX **)trpmatptr;

	return &trpmatptr->pmatrix;
}

/**
 * @brief End process of this PMatrix list service.
 *
 */
void rsp_pmat_end( void )
{
	tdestroy(PMatrixRoot, free);

	return;
}

/**
 * @brief
 *
 * @param damping
 * @param afreq
 * @param delta
 * @return PMATRIX
 */
static PMATRIX design_pmatrix( const double damping, const double afreq, const double delta )
{
	PMATRIX result;
	double  tmp;

	const double dampsq  = damping * damping;
	const double afreqsq = afreq * afreq;
	const double dampaf  = damping * afreq;

	const double a0  = exp(-dampaf * delta);
	const double a1  = afreq * sqrt(1.0 - dampsq);
	const double a2  = sin(a1 * delta);
	const double a3  = cos(a1 * delta);
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
/* Special trick for reducing computation load */
	result.b[0] += result.b[1];
	result.b[2] += result.b[3];

	return result;
}

/**
 * @brief The delta compare function of binary tree search.
 *
 * @param a
 * @param b
 * @return int
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
