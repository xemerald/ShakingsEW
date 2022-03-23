/*
 *     Revision history:
 *
 *     Revision 1.2  2018/09/21 12:14:20  Benjamin Yang
 *     Change the structure of TYPE_STA_LIST
 *
 *     Revision 1.1  2018/05/08 12:27:44  Benjamin Yang
 *     Add the channels list name to header
 *
 *     Revision 1.0  2018/03/19 16:17:44  Benjamin Yang
 *     Initial revision
 *
 */

/*
 * stalist.h
 *
 * Header file for unified plateform station list data.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * March, 2018
 *
 */

#pragma once

#include <stdint.h>

/*---------------------------------------------------------------------------*
 * Definition of structure of TYPE_STA_LIST:                                 *
 *                                                                           *
 * -Stations list header                                                     *
 *     -Station information block                                            *
 *     -Station information block                                            *
 *     -Station information block                                            *
 *              .                                                            *
 *              .                                                            *
 *              .                                                            *
 *              .                                                            *
 *              .                                                            *
 *              .                                                            *
 *     -Channel information block                                            *
 *     -Channel information block                                            *
 *     -Channel information block                                            *
 *              .                                                            *
 *              .                                                            *
 *              .                                                            *
 *              .                                                            *
 *              .                                                            *
 *              .                                                            *
 * -End of TYPE_STA_LIST                                                     *
 *---------------------------------------------------------------------------*/

#define LIST_NAME_LEN  24
#define INST_NAME_LEN  24

#define STA_CODE_LEN   8     /* 5 bytes plus 3 bytes padding */
#define NET_CODE_LEN   4     /* 3 bytes plus 1 byte padding */
#define LOC_CODE_LEN   4     /* 3 bytes plus 1 byte padding */
#define CHAN_CODE_LEN  4     /* 4 bytes */

/* Definitions of return values for reqstalist */
#define SL_REQ_OK        1   /* Put the request to share memory, no problems     */
#define SL_REQ_FAIL     -1   /* Failed to put the request to the shared memory   */

/* Definitions of semaphore operations */
#define SL_READ         0    /* No list of requested station list in memory    */
#define SL_WRITE        1    /* No list of requested station list in memory    */

#define SL_UNUSED       0    /* No list of requested station list in memory    */
#define SL_LOCKED       1    /* Got a requested station list                   */
#define SL_RELEASE     -1    /* Got a broken station list                      */

#define SL_LOCKTRIES    3

/*---------------------------------------------------------------------------*
 * Definition of Stations list header, total size is 64 bytes                *
 *---------------------------------------------------------------------------*/
typedef struct {
    uint32_t seq;                     /* Seq. number */
	uint32_t updatetime;              /* Time of updating this record */
    uint32_t totalsize;               /* Total record size */
	uint16_t totalstations;           /* Total number of stations */
	uint16_t totalchannels;           /* Total number of channels */
	char     stalist[LIST_NAME_LEN];  /* List or table name of stations (NULL-terminated) */
	char     chanlist[LIST_NAME_LEN]; /* List or table name of channels (NULL-terminated) */
} STALIST_HEADER;

/*---------------------------------------------------------------------------*
 * Definition of Station information block, total size is 48 bytes           *
 *---------------------------------------------------------------------------*/
typedef struct {
	uint16_t serial;             /* Sensor serial number of station */
	uint16_t nchannels;          /* Total number of channels */
	uint32_t offset_chaninfo;    /* Position of first channel infomation */
	char     sta[STA_CODE_LEN];  /* Site name (NULL-terminated) */
    char     net[NET_CODE_LEN];  /* Network name (NULL-terminated) */
	char     loc[LOC_CODE_LEN];  /* Location code (NULL-terminated) */
    double   latitude;           /* Latitude of station */
	double   longitude;          /* Longitude of station */
	double   elevation;          /* Elevation of station */
} STATION_BLOCK;

/*---------------------------------------------------------------------------*
 * Definition of Channel information block, total size is 48 bytes           *
 *---------------------------------------------------------------------------*/
typedef struct {
    uint16_t seq;                      /* Seq. number of component */
    uint16_t recordtype;               /* Flag for record type */
	char     chan[CHAN_CODE_LEN];      /* Component/channel code (NULL-terminated) */
	char     instname[INST_NAME_LEN];  /* Instrument name of this component (NULL-terminated) */
	double   samprate;                 /* Sample rate; nominal */
	double   conversion_factor;        /* Simple conversion factor, change counts to physical unit */
} CHAN_BLOCK;

/*----------------------------------------------*
 * Definition of a generic Stations List Packet *
 *----------------------------------------------*/
#define MAX_STALIST_SIZ 524288  /* define maximum size of stations list message */

typedef union {
    uint8_t         msg[MAX_STALIST_SIZ];
    STALIST_HEADER  slh;
} StaListPacket;

/*---------------------------------------------------*
 * Definition of Stations & Channel List info struct *
 *---------------------------------------------------*/
typedef struct {
	char stalist[LIST_NAME_LEN];   /* List or table name (NULL-terminated) */
	char chanlist[LIST_NAME_LEN];  /* List or table name (NULL-terminated) */

	STALIST_HEADER *slh;      /* pointer to beginning of shared memory */
	int64_t         shkey;    /* key to shared memory region           */
	int64_t         mid;      /* shared memory region identifier       */
	int64_t         sid;      /* associated semaphore identifier       */
} SSLIST_INFO;

typedef struct {
	char stalist[LIST_NAME_LEN];   /* List or table name (NULL-terminated) */
	char chanlist[LIST_NAME_LEN];  /* List or table name (NULL-terminated) */
} SLIST_INFO;

/*----------------------------------------------*
 * Definition of a generic Stations List info   *
 *----------------------------------------------*/
typedef union {
	SSLIST_INFO sslist;
	SLIST_INFO  slist;
} GETLISTS;

int  sslist_init( char *, char * );     /* Initialization for station list ring */
int  sslist_req( SSLIST_INFO * );       /* Put station list request to list ring */

void sslist_create( SSLIST_INFO * );
void sslist_destroy( SSLIST_INFO * );
void sslist_attach( SSLIST_INFO * );
void sslist_detach( SSLIST_INFO * );

int  sslist_wait( SSLIST_INFO *, int );
void sslist_release( SSLIST_INFO *, int );

void sslist_end( void );                            /* End process of station list */
