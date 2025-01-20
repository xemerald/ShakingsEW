/**
 * @file trace2peak_list.h
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University (b98204032@gmail.com)
 * @brief
 * @date 2018-03-20
 *
 * @copyright Copyright (c) 2018-now
 *
 */
#pragma once
/**
 * @name Earthworm environment header include
 *
 */
#include <trace_buf.h>
/**
 * @name Local header include
 *
 */
#include <trace2peak.h>

/**
 * @name External functions prototypes
 *
 */
_TRACEPEAK *tra2peak_list_search( const TRACE2X_HEADER * );
_TRACEPEAK *tra2peak_list_find( const TRACE2X_HEADER * );
void        tra2peak_list_end( void );
