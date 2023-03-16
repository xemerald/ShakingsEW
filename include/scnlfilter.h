/*
 *
 */
#pragma once
/**/
int scnlfilter_com( const char * );
int scnlfilter_extra_com( void *(*)( const char * ) );
int scnlfilter_init( const char * );
int scnlfilter_apply( const char *, const char *, const char *, const char *, const void ** );
int scnlfilter_trace_apply( const void *, const unsigned char, const void ** );
int scnlfilter_trace_remap( void *, const unsigned char, const void * );
void *scnlfilter_extra_get( const void * );
void scnlfilter_logmsg( char *, const int, const unsigned char, const char * );
void scnlfilter_end( void (*)( void * ) );
