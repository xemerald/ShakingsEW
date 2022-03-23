#pragma once

#include <stdint.h>

#include <peak2trig.h>


int StaListInit( char *, char *, uint8_t );
int StaListReg( char *, char * );
int StaListFetch( void );

_STAINFO *StaListFind( _STAINFO * );

void StaListEnd( void );
