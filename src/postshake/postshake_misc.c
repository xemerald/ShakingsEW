#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <datestring.h>
#include <griddata.h>
#include <shakeint.h>
#include <postshake.h>

/*
 *
 */
GRIDMAP_HEADER *psk_misc_refmap_get( const PLOTSMAP *psm )
{
	int i;
	GRIDMAP_HEADER *result = NULL;

/* Checking for input pointer */
	if ( psm == NULL ) return result;

	for ( i = 0; i < MAX_IN_MSG; i++ ) {
		if ( psm->gmflag[i] ) {
			result = (GRIDMAP_HEADER *)(psm->gmptr[i]);
			break;
		}
	}

	return result;
}

/*
 *
 */
int psk_misc_trigstations_get( const PLOTSMAP *psm )
{
	int result = 0;

	GRIDMAP_HEADER *gmh = psk_misc_refmap_get( psm );

	if ( gmh == NULL ) return result;

	GRID_REC       *gdr = (GRID_REC *)(gmh + 1);
	GRID_REC const *gde = gdr + gmh->totalgrids;

	/* Get the triggered stations */
	for ( ; gdr < gde; gdr++ )
		if ( gdr->gridtype == GRID_STATION ) result++;

	return result;
}

/*
 * psk_misc_smfilename_gen() - According to the griddata generate a string for result
 *                             file name
 */
char *psk_misc_smfilename_gen( const PLOTSMAP *psm, char *dest, size_t strlength )
{
	GRIDMAP_HEADER *gmh = psk_misc_refmap_get( psm );
	struct tm      *tp = NULL;

	char _rstring[MAX_STR_SIZE];
	char etimestring[MAX_DSTR_LENGTH];
	char typepostfix[16];

	if ( gmh == NULL || strlength < MAX_STR_SIZE ) return NULL;

/* Generate the time string for file name */
	tp = gmtime( &gmh->endtime );
	date2spstring( tp, etimestring, MAX_DSTR_LENGTH );

/* Generate the postfix string for file name */
	sprintf( typepostfix, "%s", shakenum2str( psm->smaptype ) );
	if ( gmh->evaltype == EVALUATE_ALERTSHAKE )
		strcat( typepostfix, "_a" );

/* Generate the full file name and copy into output string */
	sprintf(_rstring, "%s_%s.png", etimestring, typepostfix);
	strcpy(dest, _rstring);

	return dest;
}
