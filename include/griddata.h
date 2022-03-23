/*
 *     Revision history:
 *
 *     Revision 1.1  2020/03/13 18:53:15  Benjamin Yang
 *     Change MAP_TYPE to EVALUATE_TYPE.
 *
 *     Revision 1.0  2018/12/25 17:36:50  Benjamin Yang
 *     Initial revision.
 *
 */

/*
 * griddata.h
 *
 * Header file for grid map data.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * December, 2018
 *
 */

#pragma once

#include <time.h>
#include <stdint.h>

/*-----------------------------------------------------------*
 * Definition of structure of TYPE_GRID_MAP:                 *
 *                                                           *
 * -Grid map header                                          *
 *     -Grid record block                                    *
 *     -Grid record block                                    *
 *     -Grid record block                                    *
 *              .                                            *
 *              .                                            *
 *              .                                            *
 * -End of TYPE_GRID_MAP                                     *
 *-----------------------------------------------------------*/

typedef enum {
/* Using only real value & inverse distance weighting method */
	EVALUATE_REALSHAKE,
/* Using Shakealarm algorithm & inverse distance weighting method */
	EVALUATE_ALERTSHAKE,
/* Using GMPE value */
	EVALUATE_GMPE,

/* Should always be the last */
	EVALUATE_TYPE_COUNT
} EVALUATE_TYPE;


typedef enum {
/* */
	GRID_MAPGRID,
/* */
	GRID_STATION,
/* */
	GRID_BUILDING,
/* */
	GRID_OTHERS,

/* Should always be the last */
	GRID_TYPE_COUNT
} GRID_TYPE;

/*----------------------------------------------------------------------*
 * Definition of grid map header, total size is 104 bytes               *
 *----------------------------------------------------------------------*/
typedef struct {
/* */
	time_t   starttime;
/* */
	time_t   endtime;
/* */
	uint8_t  evaltype;
/* */
	uint8_t  valuetype;
/* */
	uint8_t  codaflag;
/* */
	uint8_t  padding;
/* */
	uint32_t totalgrids;

/* */
	double centervalue;
/* */
	double centerlon;
/* */
	double centerlat;
/* */
	double magnitude[8];
} GRIDMAP_HEADER; /* For message type: TYPE_EEW_SMAP */

/*---------------------------------------------------------*
 * Definition of grid record, total size is 40 bytes       *
 *---------------------------------------------------------*/
typedef struct {
/* */
	char    gridname[15];
/* */
	uint8_t gridtype;
/* */
	double  longitude;
/* */
	double  latitude;
/* */
	double  gridvalue;
} GRID_REC;

/*-----------------------------------------*
 * Definition of a generic Grid map Packet *
 *-----------------------------------------*/
#define MAX_GRID_NUM     4096    /* define maximum number of grids in map */
#define MAX_GRIDMAP_SIZ  163944  /* define maximum size of grid map message */
