/*
 * datestring.h
 *
 * Header file for date string processing functions.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * January, 2019
 *
 */

#pragma once

#include <stdlib.h>
#include <time.h>

/*
 * The format of the simple date string is like that "YYYYMMDDMMSS"
 * and it is terminated by the null terminator '\0'.
 */

/* Define the maximum simple date string length */
#define MAX_DSTR_LENGTH  16

/* Function prototype */
char      *date2spstring( struct tm *, char *, size_t );
struct tm *spstring2date( struct tm *, char * );
