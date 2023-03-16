#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <datestring.h>

/*
	The format of the simple date string is like that "YYYYMMDDMMSS"
	and it is terminated by the null terminator '\0'.
*/

/*************************************************************
 *  date2string( )  According to the date generate a string  *
 *************************************************************/
char *date2spstring( struct tm *source, char *dest, size_t destlength )
{
	char yr[5], mo[3], dy[3], hr[3], mn[3], sc[3];

	if ( dest == NULL || source == NULL || destlength < MAX_DSTR_LENGTH )
		return NULL;

	sprintf(yr, "%04d", (1900 + source->tm_year));
	sprintf(mo, "%02d", (1 + source->tm_mon));
	sprintf(dy, "%02d", source->tm_mday);
	sprintf(hr, "%02d", source->tm_hour);
	sprintf(mn, "%02d", source->tm_min);
	sprintf(sc, "%02d", source->tm_sec);

	sprintf(dest, "%s%s%s%s%s%s", yr, mo, dy, hr, mn, sc);
	dest[destlength - 1] = '\0';

	return dest;
}

/**********************************************************************
 *  spstring2date( )  According to the string generate a tm structure *
 **********************************************************************/
struct tm *spstring2date( struct tm *dest, char *source )
{
	char tmpstr[5];

	strncpy(tmpstr, &source[0], 4);
	tmpstr[4] = '\0';
	dest->tm_year = atoi(tmpstr) - 1900;

	strncpy(tmpstr, &source[4], 2);
	tmpstr[2] = '\0';
	dest->tm_mon = atoi(tmpstr) - 1;

	strncpy(tmpstr, &source[6], 2);
	tmpstr[2] = '\0';
	dest->tm_mday = atoi(tmpstr);

	strncpy(tmpstr, &source[8], 2);
	tmpstr[2] = '\0';
	dest->tm_hour = atoi(tmpstr);

	strncpy(tmpstr, &source[10], 2);
	tmpstr[2] = '\0';
	dest->tm_min = atoi(tmpstr);

	strncpy(tmpstr, &source[12], 2);
	tmpstr[2] = '\0';
	dest->tm_sec = atoi(tmpstr);

	return dest;
}
