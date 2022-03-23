#pragma once

#include <stdint.h>

#include <trace_buf.h>

#include <cf2trace.h>


int TraListInit( char *, char *, uint8_t );
int TraListReg( char *, char * );
int TraListFetch( void );

_TRACEINFO *TraListFind( TRACE2X_HEADER * );

void TraListEnd( void );
