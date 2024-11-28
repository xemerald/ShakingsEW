/**
 * @file shake2ws_protocols.h
 * @author Benjamin Ming Yang (b98204032@gmail.com)
 * @brief
 * @date 2024-11-27
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once
/* */
#include <stdint.h>
#include <time.h>
#include <netinet/in.h>
#include <libwebsockets.h>

/* */
struct per_session_data {
	char     ip[INET6_ADDRSTRLEN];
	time_t   lasttime;
/* */
	uint32_t seq;
	int32_t  totalsta;
	uint16_t peak_i;
	time_t   listtime;
	void    *stapeak;
	_Bool   *status;
/* */
	size_t   msg_size;
	uint8_t *savemsg;
};

/* */
enum shake_protocols {
	PROTOCOL_HTTP_DUMMY,
	PROTOCOL_MAP_SHAKE,
	PROTOCOL_STATION_SHAKE,
	PROTOCOL_STATION_STATUS,

/* always last */
	SHAKE2WS_PROTOCOL_COUNT
};

/* Dummy Http callback */
#define SHAKE2WS_LWS_PROTOCOL_HTTP_DUMMY \
		{ \
			"http-dummy-protocol", \
			lws_callback_http_dummy, \
			0, 0, 0, NULL, 0 \
		}

/* Map view shaking */
int sk2ws_protocols_map_shake( struct lws *, enum lws_callback_reasons, void *, void *, size_t );
/* */
#define SHAKE2WS_LWS_PROTOCOL_MAP_SHAKE \
		{ \
			"map-shake-protocol", \
			sk2ws_protocols_map_shake, \
			sizeof(struct per_session_data), \
			16384, \
			0, NULL, 0 \
		}

/* Individual station shaking */
int sk2ws_protocols_station_shake( struct lws *, enum lws_callback_reasons, void *, void *, size_t );
/* */
#define SHAKE2WS_LWS_PROTOCOL_STATION_SHAKE \
		{ \
			"station-shake-protocol", \
			sk2ws_protocols_station_shake, \
			sizeof(struct per_session_data), \
			32, \
			0, NULL, 0 \
		}

/* Status for alive or not */
int sk2ws_protocols_station_status( struct lws *, enum lws_callback_reasons, void *, void *, size_t );
/* */
#define SHAKE2WS_LWS_PROTOCOL_STATION_STATUS \
		{ \
			"station-status-protocol", \
			sk2ws_protocols_station_status, \
			sizeof(struct per_session_data), \
			16384, \
			0, NULL, 0 \
		}
