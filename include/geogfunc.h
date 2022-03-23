/*
 *     Revision history:
 *
 *     Revision 1.0  2018/11/16 13:23:20  Benjamin Yang
 *     Initial revision
 *
 */

/*
 * geogfunc.h
 *
 * Header file for geographical related functions.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * November, 2018
 *
 */

#pragma once

/* Structure for distance & azimuth */
typedef struct {
	double distance;
	double azimuth;
} DELAZ;

/* Functions prototype */
double coor2distf( const double, const double, const double, const double );

int locpt( const float, const float, const float *, const float *, int, int * );
