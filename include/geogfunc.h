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
