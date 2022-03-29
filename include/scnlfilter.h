/*
 *
 */
#pragma once
/**/
int scnlfilter_com( const char * );
int scnlfilter_init( const char * );
int scnlfilter_apply( void *, size_t, unsigned char, void *, size_t *, unsigned char * );
void scnlfilter_logmsg( char *, int, unsigned char, char * );
void scnlfilter_end( void );
