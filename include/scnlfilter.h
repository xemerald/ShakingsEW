/**
 * @file scnlfilter.h
 * @author Origin from Lynn Dietz, 1998-11-12
 * @author Benjamin Ming Yang @ Department of Geoscience, National Taiwan University (b98204032@gmail.com)
 * @brief Source header file that contains a series of function to filter messages based on content of site-component-network-loc fields.
 * @date 2022-03-28
 *
 * @copyright Copyright (c) 2022-now
 *
 */
#pragma once
/**
 * @name
 *
 */
int   scnlfilter_com( const char * );
int   scnlfilter_extra_com( void *(*)( const char * ) );
int   scnlfilter_init( const char * );
int   scnlfilter_apply( const char *, const char *, const char *, const char *, const void ** );
int   scnlfilter_remap( const char *, const char *, const char *, const char *, const void * );
void *scnlfilter_extra_get( const void * );
void  scnlfilter_end( void (*)( void * ) );
