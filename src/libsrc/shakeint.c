/* Standard C header include */
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Local header include */
#include <shakeint.h>

/* Internal functions' prototype */
static SHAKE_INT shake_papga_int( const double *, const int );
static SHAKE_INT shake_papgv_int( const double *, const int );
static SHAKE_INT shake_pasa_int( const double *, const int );
static SHAKE_INT shake_pacd_int( const double *, const int );
static SHAKE_INT shake_cwbpga_int( const double *, const int );
static SHAKE_INT shake_cwbpgv_int( const double *, const int );
static SHAKE_INT shake_cwb2020_int( const double *, const int );

/* Shaking scale type string define */
static const struct _TypeInfo {
/* */
	char *typestring;
/* */
	SHAKE_INT (*typescalefunc)( const double *, const int );
/* */
	int reqinputs;
/* */
	SHAKE_INT maxintensity;
} TypeInfo[SHAKE_TYPE_COUNT] = {
/* PAlert system used Seismic Intensity Scale */
	[SHAKE_PAPGA]   = { "papga", shake_papga_int, 1, SHAKE_INT_X },
	[SHAKE_PAPGV]   = { "papgv", shake_papgv_int, 1, SHAKE_INT_X },
	[SHAKE_PASA]    = { "pasa", shake_pasa_int, 1, SHAKE_INT_X },
	[SHAKE_PACD]    = { "pacd", shake_pacd_int, 1, SHAKE_INT_X },
/* Central Weather Bureau Seismic Intensity Scale */
	[SHAKE_CWBPGA]  = { "cwbpga", shake_cwbpga_int, 1, SHAKE_INT_VIII },
	[SHAKE_CWBPGV]  = { "cwbpgv", shake_cwbpgv_int, 1, SHAKE_INT_VIII },
	[SHAKE_CWB2020] = { "cwb2020", shake_cwb2020_int, 2, SHAKE_INT_X },
/* Japan Meteorological Agency Seismic Intensity Scale */
	[SHAKE_JMA]     = { "jma", NULL, 1, SHAKE_INT_XII },
/* Modified Mercalli Intensity Scale */
	[SHAKE_MMI]     = { "mmi", NULL, 1, SHAKE_INT_XII },
/* China Seismic Intensity Scale, GB/T 17742-1999 */
	[SHAKE_GBT1999] = { "gbt1999", NULL, 1, SHAKE_INT_XII },
/* European Macroseismic Scale, EMS-98 */
	[SHAKE_EMS98]   = { "ems98", NULL, 1, SHAKE_INT_XII },
/* Medvedev-Sponheuer-Karnik Scale, MSK-64 */
	[SHAKE_MSK64]   = { "msk64", NULL, 1, SHAKE_INT_XII }
};

/* Undefined type string */
static const char *UndefType = "undef";

/*
	Transform the type string to type number
*/
SHAKE_TYPE shakestr2num( const char *_typestr ) {
	int  i;
	char ltypestr[SHAKE_STR_LEN];

	if ( _typestr == NULL ) return SHAKE_TYPE_COUNT;

	strcpy(ltypestr, _typestr);

	for( i = 0; ltypestr[i]; i++ ) ltypestr[i] = tolower(ltypestr[i]);

	for ( i = SHAKE_PAPGA; i < SHAKE_TYPE_COUNT; i++ )
		if ( strncmp(ltypestr, TypeInfo[i].typestring, strlen(TypeInfo[i].typestring)) == 0 ) break;

	return i;
}

/*
	Transform the type number to type string
*/
char *shakenum2str( const SHAKE_TYPE shaketype ) {
	if ( shaketype >= SHAKE_TYPE_COUNT || shaketype < SHAKE_PAPGA )
		return (char *)UndefType;

	return TypeInfo[shaketype].typestring;
}

/*
	The shaking intensity interface function
*/
SHAKE_INT shake_get_intensity( const double *input, const int inlength, const SHAKE_TYPE shaketype )
{
	if ( shaketype >= SHAKE_TYPE_COUNT || shaketype < SHAKE_PAPGA )
		return SHAKE_INT_COUNT;

	return TypeInfo[shaketype].typescalefunc( input, inlength );
}

/*
	The number of required inputs of shaking type function
*/
int shake_get_reqinputs( const SHAKE_TYPE shaketype )
{
	if ( shaketype >= SHAKE_TYPE_COUNT || shaketype < SHAKE_PAPGA )
		return 0;

	return TypeInfo[shaketype].reqinputs;
}

/*
	The maximum intensity of shaking type function
*/
int shake_get_maxintensity( const SHAKE_TYPE shaketype )
{
	if ( shaketype >= SHAKE_TYPE_COUNT || shaketype < SHAKE_PAPGA )
		return SHAKE_INT_COUNT;

	return TypeInfo[shaketype].maxintensity;
}

/* The scale rule's function of PAlert system PGA intensity */
static SHAKE_INT shake_papga_int( const double *input, const int inlength )
{
	SHAKE_INT result = SHAKE_INT_COUNT;

	if ( inlength != 1 ) {
		fprintf(stderr, "shake_papga_int: Error, the input length(%d) is not correct, it should be 1 (PGA).\n", inlength);
		return result;
	}

	const double pga = input[0];

	if ( pga < 2.5 ) result = SHAKE_INT_I;
	else if ( pga < 8.0 ) result = SHAKE_INT_II;
	else if ( pga < 15.0 ) result = SHAKE_INT_III;
	else if ( pga < 25.0 ) result = SHAKE_INT_IV;
	else if ( pga < 40.0 ) result = SHAKE_INT_V;
	else if ( pga < 80.0 ) result = SHAKE_INT_VI;
	else if ( pga < 150.0 ) result = SHAKE_INT_VII;
	else if ( pga < 250.0 ) result = SHAKE_INT_VIII;
	else if ( pga < 400.0 ) result = SHAKE_INT_IX;
	else result = SHAKE_INT_X;

	return result;
}

/*
	The scale rule's function of PAlert system PGV intensity
*/
static SHAKE_INT shake_papgv_int( const double *input, const int inlength )
{
	SHAKE_INT result = SHAKE_INT_COUNT;

	if ( inlength != 1 ) {
		fprintf(stderr, "shake_papgv_int: Error, the input length(%d) is not correct, it should be 1 (PGV).\n", inlength);
		return result;
	}

	const double pgv = input[0];

	if ( pgv < 0.65 ) result = SHAKE_INT_I;
	else if ( pgv < 1.90 ) result = SHAKE_INT_II;
	else if ( pgv < 3.45 ) result = SHAKE_INT_III;
	else if ( pgv < 5.70 ) result = SHAKE_INT_IV;
	else if ( pgv < 8.75 ) result = SHAKE_INT_V;
	else if ( pgv < 17.0 ) result = SHAKE_INT_VI;
	else if ( pgv < 31.0 ) result = SHAKE_INT_VII;
	else if ( pgv < 49.0 ) result = SHAKE_INT_VIII;
	else if ( pgv < 75.0 ) result = SHAKE_INT_IX;
	else result = SHAKE_INT_X;

	return result;
}

/*
	The scale rule's function of PAlert system Sa intensity
	Not ready!!!!
*/
static SHAKE_INT shake_pasa_int( const double *input, const int inlength )
{
	SHAKE_INT result = SHAKE_INT_COUNT;

	if ( inlength != 1 ) {
		fprintf(stderr, "shake_pasa_int: Error, the input length(%d) is not correct, it should be 1 (Sa).\n", inlength);
		return result;
	}

	const double sa = input[0];

	if ( sa < 2.5 ) result = SHAKE_INT_I;
	else if ( sa < 8.0 ) result = SHAKE_INT_II;
	else if ( sa < 15.0 ) result = SHAKE_INT_III;
	else if ( sa < 25.0 ) result = SHAKE_INT_IV;
	else if ( sa < 40.0 ) result = SHAKE_INT_V;
	else if ( sa < 80.0 ) result = SHAKE_INT_VI;
	else if ( sa < 150.0 ) result = SHAKE_INT_VII;
	else if ( sa < 250.0) result = SHAKE_INT_VIII;
	else if ( sa < 400.0 ) result = SHAKE_INT_IX;
	else result = SHAKE_INT_X;

	return result;
}

/*
	The scale rule's function of PAlert system Cd intensity
	Not ready!!!
*/
static SHAKE_INT shake_pacd_int( const double *input, const int inlength )
{
	SHAKE_INT result = SHAKE_INT_COUNT;

	if ( inlength != 1 ) {
		fprintf(stderr, "shake_pacd_int: Error, the input length(%d) is not correct, it should be 1 (Cd).\n", inlength);
		return result;
	}

	const double cd = input[0];

	if ( cd < 0.065 ) result = SHAKE_INT_I;
	else if ( cd < 0.19 ) result = SHAKE_INT_II;
	else if ( cd < 0.34 ) result = SHAKE_INT_III;
	else if ( cd < 0.57 ) result = SHAKE_INT_IV;
	else if ( cd < 0.87 ) result = SHAKE_INT_V;
	else if ( cd < 1.70 ) result = SHAKE_INT_VI;
	else if ( cd < 3.10 ) result = SHAKE_INT_VII;
	else if ( cd < 4.90 ) result = SHAKE_INT_VIII;
	else if ( cd < 7.50 ) result = SHAKE_INT_IX;
	else result = SHAKE_INT_X;

	return result;
}

/*
	The scale rule's function of CWB simple PGA intensity
*/
static SHAKE_INT shake_cwbpga_int( const double *input, const int inlength )
{
	SHAKE_INT result = SHAKE_INT_COUNT;

	if ( inlength != 1 ) {
		fprintf(stderr, "shake_cwbpga_int: Error, the input length(%d) is not correct, it should be 1 (PGA).\n", inlength);
		return result;
	}

	const double pga = input[0];

	if ( pga < 0.8 ) result = SHAKE_INT_I;          /* Level 0 */
	else if ( pga < 2.5 ) result = SHAKE_INT_II;    /* Level 1 */
	else if ( pga < 8.0 ) result = SHAKE_INT_III;   /* Level 2 */
	else if ( pga < 25.0 ) result = SHAKE_INT_IV;   /* Level 3 */
	else if ( pga < 80.0 ) result = SHAKE_INT_V;    /* Level 4 */
	else if ( pga < 250.0 ) result = SHAKE_INT_VI;  /* Level 5 */
	else if ( pga < 400.0 ) result = SHAKE_INT_VII; /* Level 6 */
	else result = SHAKE_INT_VIII;                   /* Level 7 */

	return result;
}

/*
	The scale rule's function of CWB simple PGV intensity
*/
static SHAKE_INT shake_cwbpgv_int( const double *input, const int inlength )
{
	SHAKE_INT result = SHAKE_INT_COUNT;

	if ( inlength != 1 ) {
		fprintf(stderr, "shake_cwbpgv_int: Error, the input length(%d) is not correct, it should be 1 (PGV).\n", inlength);
		return result;
	}

	const double pgv = input[0];

	if ( pgv < 0.22 ) result = SHAKE_INT_I;        /* Level 0 */
	else if ( pgv < 0.65 ) result = SHAKE_INT_II;  /* Level 1 */
	else if ( pgv < 1.90 ) result = SHAKE_INT_III; /* Level 2 */
	else if ( pgv < 5.70 ) result = SHAKE_INT_IV;  /* Level 3 */
	else if ( pgv < 17.0 ) result = SHAKE_INT_V;   /* Level 4 */
	else if ( pgv < 49.0 ) result = SHAKE_INT_VI;  /* Level 5 */
	else if ( pgv < 75.0 ) result = SHAKE_INT_VII; /* Level 6 */
	else result = SHAKE_INT_VIII;                  /* Level 7 */

	return result;
}

/*
	The scale rule's function of CWB new intensity started from 2020 (mix of PGA & PGV)
*/
static SHAKE_INT shake_cwb2020_int( const double *input, const int inlength )
{
	SHAKE_INT result = SHAKE_INT_COUNT;

	if ( inlength != 2 ) {
		fprintf(stderr, "shake_cwb2020_int: Error, the input length(%d) is not correct, it should be 2 (PGA, PGV).\n", inlength);
		return result;
	}

	const double pga = input[0];
	const double pgv = input[1];

	if ( pga < 0.8 ) result = SHAKE_INT_I;                     /* Level 0 */
	else if ( pga < 2.5 ) result = SHAKE_INT_II;               /* Level 1 */
	else if ( pga < 8.0 ) result = SHAKE_INT_III;              /* Level 2 */
	else if ( pga < 25.0 ) result = SHAKE_INT_IV;              /* Level 3 */
	else if ( pga < 80.0 || pgv < 15.0 ) result = SHAKE_INT_V; /* Level 4 */
	else if ( pgv < 30.0 ) result = SHAKE_INT_VI;              /* Level 5L */
	else if ( pgv < 50.0 ) result = SHAKE_INT_VII;             /* Level 5H */
	else if ( pgv < 80.0 ) result = SHAKE_INT_VIII;            /* Level 6L */
	else if ( pgv < 140.0 ) result = SHAKE_INT_IX;             /* Level 6H */
	else result = SHAKE_INT_X;                                 /* Level 7 */

	return result;
}
