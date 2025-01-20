/**
 * @file dif2trace_list.h
 * @author Benjamin Ming Yang (b98204032@gmail.com) @ Department of Geology, National Taiwan University
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
#include <dif2trace.h>

/**
 * @name External functions prototypes
 *
 */
_TRACEINFO *dif2tra_list_search( const TRACE2X_HEADER * );
_TRACEINFO *dif2tra_list_find( const TRACE2X_HEADER * );
void        dif2tra_list_end( void );
