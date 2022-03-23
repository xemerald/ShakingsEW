/*
 *     Revision history:
 *
 *     Revision 1.1  2020/03/13 18:56:10  Benjamin Yang
 *     Add two record type, RECORD_SPECTRAL_DISPLACEMENT &
 *     RECORD_SPECTRAL_VELOCITY.
 *
 *     Revision 1.0  2018/11/15 18:32:20  Benjamin Yang
 *     Initial revision
 *
 */

/*
 * recordtype.h
 *
 * Header file for record data type.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * November, 2018
 *
 */

#pragma once

/* For record type flag */
typedef enum {
	RECORD_DISPLACEMENT,
	RECORD_VELOCITY,
	RECORD_ACCELERATION,
	RECORD_SPECTRAL_DISPLACEMENT,
	RECORD_SPECTRAL_VELOCITY,
	RECORD_SPECTRAL_ACCELERATION,
	/* Maybe else... */

/* Should always be the last */
	RECORD_TYPE_COUNT
} RECORD_TYPE;

#define TYPE_STR_LEN 16

/* Functions prototype */
RECORD_TYPE typestr2num( const char * );
const char *typenum2str( const RECORD_TYPE );
