/*
 *     Revision history:
 *
 *     Revision 1.0  2020/02/20 17:57:20  Benjamin Yang
 *     Initial revision
 *
 */

/*
 * shakeint.h
 *
 * Header file for shaking intensity type.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * February, 2020
 *
 */


#pragma once

/**/
typedef enum {
/* PAlert system used Seismic Intensity Scale */
	SHAKE_PAPGA,
	SHAKE_PAPGV,
	SHAKE_PASA,
	SHAKE_PACD,
/* Central Weather Bureau Seismic Intensity Scale */
	SHAKE_CWBPGA,
	SHAKE_CWBPGV,
	SHAKE_CWB2020,
/* Japan Meteorological Agency Seismic Intensity Scale */
	SHAKE_JMA,
/* Modified Mercalli Intensity Scale */
	SHAKE_MMI,
/* China Seismic Intensity Scale, GB/T 17742-1999 */
	SHAKE_GBT1999,
/* European Macroseismic Scale, EMS-98 */
	SHAKE_EMS98,
/* Medvedev-Sponheuer-Karnik Scale, MSK-64 */
	SHAKE_MSK64,

/* Should always be the last */
	SHAKE_TYPE_COUNT
} SHAKE_TYPE;

/**/
typedef enum {
/* Shaking Level 1 */
	SHAKE_INT_I = 0,
/* Shaking Level 2 */
	SHAKE_INT_II,
/* Shaking Level 3 */
	SHAKE_INT_III,
/* Shaking Level 4 */
	SHAKE_INT_IV,
/* Shaking Level 5 */
	SHAKE_INT_V,
/* Shaking Level 6 */
	SHAKE_INT_VI,
/* Shaking Level 7 */
	SHAKE_INT_VII,
/* Shaking Level 8 */
	SHAKE_INT_VIII,
/* Shaking Level 9 */
	SHAKE_INT_IX,
/* Shaking Level 10 */
	SHAKE_INT_X,
/* Shaking Level 11 */
	SHAKE_INT_XI,
/* Shaking Level 12 */
	SHAKE_INT_XII,

/* Should always be the last */
	SHAKE_INT_COUNT
} SHAKE_INT;


#define SHAKE_STR_LEN 16

/* Functions prototype */
SHAKE_TYPE shakestr2num( const char * );
char *shakenum2str( const SHAKE_TYPE );

/* Main interface function */
SHAKE_INT shake_get_intensity( const double *, const int, const SHAKE_TYPE );
int shake_get_reqinputs( const SHAKE_TYPE );
int shake_get_maxintensity( const SHAKE_TYPE );
