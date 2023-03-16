/* Standard C header include */
#include <string.h>
#include <ctype.h>

/* Local header include */
#include <recordtype.h>

/* Record data type string define */
static const char *TypeString[RECORD_TYPE_COUNT] = {
	[RECORD_DISPLACEMENT]          = "dis",
	[RECORD_VELOCITY]              = "vel",
	[RECORD_ACCELERATION]          = "acc",
	[RECORD_SPECTRAL_DISPLACEMENT] = "sd",
	[RECORD_SPECTRAL_VELOCITY]     = "sv",
	[RECORD_SPECTRAL_ACCELERATION] = "sa"
};

static const char *UndefType = "undef";

/* Transform the type string to type number */
RECORD_TYPE typestr2num( const char *_typestr ) {
	int  i;
	char ltypestr[TYPE_STR_LEN];

	if ( _typestr == NULL ) return RECORD_TYPE_COUNT;

	strcpy(ltypestr, _typestr);

	for( i = 0; ltypestr[i]; i++ ) ltypestr[i] = tolower(ltypestr[i]);

	for ( i = RECORD_DISPLACEMENT; i < RECORD_TYPE_COUNT; i++ )
		if ( strncmp(ltypestr, TypeString[i], strlen(TypeString[i])) == 0 ) break;

	return i;
}

/* Transform the type number to type string */
const char *typenum2str( const RECORD_TYPE _typenum ) {
	if ( _typenum >= RECORD_TYPE_COUNT || _typenum < RECORD_DISPLACEMENT )
		return UndefType;

	return TypeString[_typenum];
}
