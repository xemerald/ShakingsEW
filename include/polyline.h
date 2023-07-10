/**
 * @file polyline.h
 * @author Benjamin Ming Yang (b98204032@gmail.com
 * @author Department of Geology in National Taiwan University
 * @brief
 * @version 0.1
 * @date 2023-07-06
 *
 * @copyright Copyright (c) 2023
 *
 */
#pragma once

#define MAX_LINE_LENGTH  256

/* */
typedef struct {
	int    points;
	float *x;
	float *y;
} POLY_LINE_ENTITY;

/* */
int polyline_read( void **, const char * );
int polyline_locpt_one( const float, const float, const POLY_LINE_ENTITY *, int * );
int polyline_locpt_all( const float, const float, const void *, int * );
int polyline_walk_all( const void *, void (*)( POLY_LINE_ENTITY * ) );
void polyline_free_all( void * );
