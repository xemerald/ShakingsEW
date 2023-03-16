/*
 * iirfilter.h
 *
 * Header file for IIR filter related functions & data.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * January, 2019
 *
 */

#pragma once

#define  MAX_NUM_SECTIONS  10

/*----------------------------------------------------------------------*
 * Definition of IIR filters' type, total number of types is 4          *
 *----------------------------------------------------------------------*/
typedef enum {
	IIR_BANDPASS_FILTER,
	IIR_BANDREJECT_FILTER,
	IIR_LOWPASS_FILTER,
	IIR_HIGHPASS_FILTER,

/* Should always be the last */
	IIR_FILTER_TYPE_COUNT
} IIR_FILTER_TYPE;

/*----------------------------------------------------------------------*
 * Definition of IIR analog prototype, total number of types is 2       *
 *----------------------------------------------------------------------*/
typedef enum {
	IIR_BUTTERWORTH,
	IIR_BESSEL,

/* Should always be the last */
	IIR_ANALOG_PROTOTYPE_COUNT
} IIR_ANALOG_PROTOTYPE;

/*----------------------------------------------------------------------*
 * Definition of IIR second section type, total number of types is 3    *
 *----------------------------------------------------------------------*/
typedef enum {
	IIR_SINGLE_REALPOLE,
	IIR_COMPLEX_CONJ_POLEPAIR,
	IIR_COMPLEX_CONJ_PZPAIR,

/* Should always be the last */
	IIR_2ND_SECTIONTYPE_COUNT
} IIR_2ND_SECTIONTYPE;

/*----------------------------------------------------------------------*
 * Definition of IIR second section structure, total size is 48 bytes       *
 *----------------------------------------------------------------------*/
typedef struct {
	double numerator[3];
	double denominator[3];
} IIR_SECTION;

/*----------------------------------------------------------------------*
 * Definition of IIR filter structure, total size is 484 bytes          *
 *----------------------------------------------------------------------*/
typedef struct {
	int         nsects;
	IIR_SECTION sections[MAX_NUM_SECTIONS];
} IIR_FILTER;

/*----------------------------------------------------------------------*
 * Definition of IIR stage structure, total size is 32 bytes            *
 *----------------------------------------------------------------------*/
typedef struct {
	double x1;
	double x2;
	double y1;
	double y2;
} IIR_STAGE;

/* Functions prototype */
double applyfilter( const double, const IIR_FILTER *, IIR_STAGE * );
IIR_FILTER designfilter( const int, const int, const int, const double, const double, const double );
