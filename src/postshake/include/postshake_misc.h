/*
 *
 */
#pragma once
/* */
#include <griddata.h>
#include <postshake.h>
/* Function prototype */
char           *psk_misc_smfilename_gen( const PLOTSMAP *, char *, size_t );
int             psk_misc_trigstations_get( const PLOTSMAP * );
GRIDMAP_HEADER *psk_misc_refmap_get( const PLOTSMAP * );
