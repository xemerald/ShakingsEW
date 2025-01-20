/**
 * @file respectra_pmat.h
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University (b98204032@gmail.com)
 * @brief
 * @date 2018-03-20
 *
 * @copyright Copyright (c) 2018-now
 *
 */
#pragma once
/**
 * @name Local header include
 *
 */
#include <respectra.h>

/**
 * @brief
 *
 */
typedef struct {
	double  delta;
	PMATRIX pmatrix;
} TRACE_PMATRIX;
/**
 * @name External functions prototypes
 *
 */
int      rsp_pmat_init( const double, const double );
PMATRIX *rsp_pmat_search( const _TRACEINFO * );
PMATRIX *rsp_pmat_find( const _TRACEINFO * );
void     rsp_pmat_end( void );
