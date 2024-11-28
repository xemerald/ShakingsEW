/**
 * @file shake2ws_protocols.c
 * @author Bemjamin Ming Yang (b98204032@gmail.com)
 * @brief
 * @date 2024-11-27
 *
 * @copyright Copyright (c) 2024
 *
 */
/* Standard C header include */
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <netinet/in.h>
#include <libwebsockets.h>
/* Earthworm environment header include */
#include <earthworm.h>
#include <trace_buf.h>
/* Local header include */
#include <shake2ws.h>
#include <shake2ws_list.h>
#include <shake2ws_protocols.h>

/**
 * @brief
 *
 */
typedef struct {
	union {
		double  realvalue;
		uint8_t intensity;
	} peakvalue;
	uint32_t alive_sec;
} ShakeAlive;

/**
 * @
 *
 */
static void init_per_session_data( struct per_session_data * );
static void destroy_per_session_data( struct per_session_data * );

/**
 * @
 *
 */
extern volatile uint16_t nPeakValue;
extern volatile uint16_t nIntensity;

/**
 * @brief
 *
 * @param wsi
 * @param reason
 * @param user
 * @param in
 * @param len
 * @return int
 */
int sk2ws_protocols_map_shake(
	struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len
) {
	static uint8_t *payload      = NULL;
	static size_t   payload_size = 0;
/* */
	time_t          timenow;
	char            tagbuf[16] = { 0 };
	uint8_t        *payload_ptr = NULL;
	size_t          data_size;
	STATION_PEAK   *_stapeak = NULL;
/* */
	struct per_session_data *pss = (struct per_session_data *)user;
	ShakeAlive *salive_ptr = NULL;

/* Get the time of present */
	time(&timenow);
/* Start to define the callback condition */
	switch ( reason ) {
	case LWS_CALLBACK_PROTOCOL_DESTROY:
		if ( payload && payload_size ) {
			free(payload);
			payload = NULL;
			payload_size = 0;
		}
		break;
	case LWS_CALLBACK_ESTABLISHED:
		init_per_session_data( pss );
		lws_get_peer_simple(wsi, pss->ip, INET6_ADDRSTRLEN);
		logit("ot", "shake2ws: Client:%s is connected, protocol is MAP_SHAKE.\n", pss->ip);
		break;
	case LWS_CALLBACK_TIMER:
		if ( pss->totalsta && payload ) {
			if ( sk2ws_list_timestamp_get() > pss->listtime ) {
			/* Re-map the station list */
				if ( pss->stapeak ) {
					free(pss->stapeak);
					pss->stapeak = NULL;
				}
				if ( !sk2ws_list_list_map( &pss->stapeak, NULL, pss->savemsg, pss->msg_size ) ) {
					logit("e", "shake2ws: Can't map station list for Client:%s in MAP_SHAKE.\n", pss->ip);
					lws_close_reason(wsi, LWS_CLOSE_STATUS_UNEXPECTED_CONDITION, NULL, 0);
					return -1;
				}
				pss->listtime = sk2ws_list_timestamp_get();
			}
		/* */
			payload_ptr            = payload + LWS_PRE;
			*(double *)payload_ptr = (double)timenow;
			payload_ptr           += sizeof(double);
			salive_ptr             = (ShakeAlive *)pss->status;
		/* */
			for ( int i = 0; i < pss->totalsta; i++, salive_ptr++ ) {
				_stapeak = ((STATION_PEAK **)pss->stapeak)[i];
			/* */
				if ( _stapeak->intensity[pss->peak_i] >= salive_ptr->peakvalue.intensity || !salive_ptr->alive_sec ) {
					salive_ptr->peakvalue.intensity = _stapeak->intensity[pss->peak_i];
					salive_ptr->alive_sec = pss->seq;
				}
			/* */
				*payload_ptr++ = salive_ptr->peakvalue.intensity;
				salive_ptr->alive_sec--;
			}
		/* */
			data_size = sizeof(double) + pss->totalsta;
			if ( lws_write(wsi, payload + LWS_PRE, data_size, LWS_WRITE_BINARY) < (int)data_size ) {
				logit("e", "shake2ws: Client:%s websocket write error!\n", pss->ip);
				lws_close_reason(wsi, LWS_CLOSE_STATUS_UNEXPECTED_CONDITION, NULL, 0);
				return -1;
			}
			pss->lasttime = timenow;
		}
	/* */
		lws_set_timer_usecs(wsi, LWS_USEC_PER_SEC);
		break;
	case LWS_CALLBACK_RECEIVE:
	/* */
		if ( pss->totalsta && pss->savemsg )
			destroy_per_session_data( pss );
	/* */
		if ( !pss->msg_size && !pss->savemsg )
			pss->savemsg = (uint8_t *)malloc(MAX_EX_STATION_STRING);
		memcpy(pss->savemsg + pss->msg_size, in, len);
		pss->msg_size += len;
	/* */
		if ( lws_is_final_fragment(wsi) ) {
			pss->lasttime = timenow;
			pss->totalsta = sk2ws_list_list_map( &pss->stapeak, tagbuf, pss->savemsg, pss->msg_size );
		/* The tag format will be "<peak_type>:<time_window>", and we use the seq to save the time window value here */
			sscanf(tagbuf, "%hd:%d", &pss->peak_i, &pss->seq);
			pss->listtime = sk2ws_list_timestamp_get();
			pss->status   = calloc(pss->totalsta, sizeof(ShakeAlive));
		/* */
			if ( pss->totalsta && pss->peak_i < nIntensity ) {
				if ( !payload || payload_size < (size_t)(LWS_PRE + sizeof(double) + pss->totalsta) ) {
					if ( payload ) {
						free(payload);
						payload = NULL;
					}
					payload_size = LWS_PRE + sizeof(double) + pss->totalsta;
					payload      = (uint8_t *)malloc(payload_size);
				}
				if ( !payload ) {
					logit("t", "shake2ws: Can't allocate memory for Client:%s in MAP_SHAKE.\n", pss->ip);
					lws_close_reason(wsi, LWS_CLOSE_STATUS_UNEXPECTED_CONDITION, NULL, 0);
					return -1;
				}
			}
			else {
				logit("t", "shake2ws: Some errors when parsing JSON message from Client:%s in MAP_SHAKE.\n", pss->ip);
				lws_close_reason(wsi, LWS_CLOSE_STATUS_UNEXPECTED_CONDITION, NULL, 0);
				return -1;
			}
		/* */
			logit(
				"ot", "shake2ws: Client:%s request the #%hd value with time window %d sec. in MAP_SHAKE.\n",
				pss->ip, pss->peak_i, pss->seq
			);
		}
	/* */
		lws_set_timer_usecs(wsi, LWS_USEC_PER_SEC);
		break;
	case LWS_CALLBACK_CLOSED:
	/* */
		destroy_per_session_data( pss );
		logit("ot", "shake2ws: Client:%s connection close, protocol is MAP_SHAKE.\n", pss->ip);
		break;
	default:
		break;
	}

	return 0;
}

/**
 * @brief
 *
 * @param wsi
 * @param reason
 * @param user
 * @param in
 * @param len
 * @return int
 */
int sk2ws_protocols_station_shake(
	struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len
) {
/* */
	static uint8_t *payload       = NULL;
	static size_t   payload_size  = 0;
	static double  *timestamp_ptr = NULL;
	static double  *pvalue_ptr    = NULL;
	static uint8_t *plevel_ptr    = NULL;
	static size_t   data_size    = 0;
/* */
	time_t          timenow;

	struct per_session_data *pss = (struct per_session_data *)user;

/* Get the time of present */
	time(&timenow);
/* Start to define the callback condition */
	switch ( reason ) {
	case LWS_CALLBACK_PROTOCOL_INIT:
		if ( !payload ) {
			payload_size  = LWS_PRE + sizeof(double) * (nPeakValue + 1) + sizeof(uint8_t) * nPeakValue;
			payload       = (uint8_t *)malloc(payload_size);
			timestamp_ptr = (double *)(payload + LWS_PRE);
			pvalue_ptr    = timestamp_ptr + 1;
			plevel_ptr    = (uint8_t *)(pvalue_ptr + nPeakValue);
			data_size     = sizeof(double) * (nPeakValue + 1) + sizeof(uint8_t) * nPeakValue;
		}
		break;
	case LWS_CALLBACK_PROTOCOL_DESTROY:
		if ( payload && payload_size ) {
			free(payload);
			payload       = NULL;
			timestamp_ptr = NULL;
			pvalue_ptr    = NULL;
			plevel_ptr    = NULL;
			payload_size  = 0;
		}
		break;
	case LWS_CALLBACK_ESTABLISHED:
		init_per_session_data( pss );
		pss->savemsg  = (uint8_t *)malloc(TRACE2_STA_LEN);
		pss->msg_size = TRACE2_STA_LEN;
		lws_get_peer_simple(wsi, pss->ip, INET6_ADDRSTRLEN);
		logit("ot", "shake2ws: Client:%s is connected, protocol is STATION_SHAKE.\n", pss->ip);
		break;
	case LWS_CALLBACK_TIMER:
		if ( pss->totalsta ) {
		/* */
			if ( sk2ws_list_timestamp_get() > pss->listtime ) {
			/* Re-map the station link */
				pss->stapeak  = sk2ws_list_station_map( (char *)pss->savemsg, "TW", "--" );
				pss->listtime = sk2ws_list_timestamp_get();
			}
		/* */
			*timestamp_ptr = (double)timenow;
			for ( int i = 0; i < nPeakValue; i++ ) {
				pvalue_ptr[i] = ((STATION_PEAK *)pss->stapeak)->pvalue[i];
				plevel_ptr[i] = ((STATION_PEAK *)pss->stapeak)->plevel[i];
			}
		/* */
			if ( lws_write(wsi, payload + LWS_PRE, data_size, LWS_WRITE_BINARY) < (int)data_size ) {
				logit("e", "shake2ws: Client:%s websocket write error!\n", pss->ip);
				return -1;
			}
			pss->lasttime = timenow;
		}
	/* */
		lws_set_timer_usecs(wsi, LWS_USEC_PER_SEC);
		break;
	case LWS_CALLBACK_RECEIVE:
		if ( len < 1 )
			break;
	/* */
		memcpy(pss->savemsg, in, len);
		((char *)pss->savemsg)[len] = '\0';
		pss->stapeak  = sk2ws_list_station_map( (char *)pss->savemsg, "TW", "--" );
		pss->listtime = sk2ws_list_timestamp_get();
		pss->peak_i   = nPeakValue;
		pss->totalsta = 1;
		pss->lasttime = timenow;
	/* */
		logit(
			"ot", "shake2ws: Client:%s request the peak value of %s.\n",
			pss->ip, ((STATION_PEAK *)pss->stapeak)->sta
		);
	/* */
		lws_set_timer_usecs(wsi, LWS_USEC_PER_SEC);
		break;
	case LWS_CALLBACK_CLOSED:
		logit("ot", "shake2ws: Client:%s connection close, protocol is STATION_SHAKE.\n", pss->ip);
		break;
	default:
		break;
	}

	return 0;
}

/**
 * @brief
 *
 * @param wsi
 * @param reason
 * @param user
 * @param in
 * @param len
 * @return int
 */
int sk2ws_protocols_station_status(
	struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len
) {
/* */
	static uint8_t *payload = NULL;
	static size_t   payload_size = 0;
/* */
	time_t          timenow;
	size_t          data_size;
	uint32_t       *payload_ptr = NULL;
	STATION_PEAK   *_stapeak = NULL;
	_Bool           _status;
/* */
	struct per_session_data *pss = (struct per_session_data *)user;

/* Get the time of present */
	time(&timenow);
/* Start to define the callback condition */
	switch ( reason ) {
	case LWS_CALLBACK_PROTOCOL_DESTROY:
		if ( payload && payload_size ) {
			free(payload);
			payload = NULL;
			payload_size = 0;
		}
		break;
	case LWS_CALLBACK_ESTABLISHED:
		init_per_session_data( pss );
	/* It seems like that the library can only provide the IP in IPV6 format */
		lws_get_peer_simple(wsi, pss->ip, INET6_ADDRSTRLEN);
		logit("ot", "shake2ws: Client:%s is connected, protocol is STATION_STATUS.\n", pss->ip);
		break;
	case LWS_CALLBACK_TIMER:
		if ( pss->totalsta && payload ) {
		/* */
			*(double *)(payload + LWS_PRE) = (double)timenow;
			payload_ptr                    = (uint32_t *)(payload + LWS_PRE + sizeof(double));
			*payload_ptr++                 = pss->seq;
		/* We got a new version of station list */
			if ( sk2ws_list_timestamp_get() > pss->listtime ) {
			/* Re-map the station list */
				if ( pss->stapeak ) {
					free(pss->stapeak);
					pss->stapeak = NULL;
				}
				if ( !sk2ws_list_list_map( &pss->stapeak, NULL, pss->savemsg, pss->msg_size ) ) {
					logit("e", "shake2ws: Can't map station list for Client:%s in STATION_STATUS.\n", pss->ip);
					lws_close_reason(wsi, LWS_CLOSE_STATUS_UNEXPECTED_CONDITION, NULL, 0);
					return -1;
				}
				pss->listtime = sk2ws_list_timestamp_get();
			}
		/* Go thru all stations */
			for ( int i = 0; i < pss->totalsta; i++ ) {
				_stapeak = ((STATION_PEAK **)pss->stapeak)[i];
				_status  = _stapeak->pvalue[0] < 0.0 ? 0 : 1;
			/* We just notify those stations with different status than the last time, include the first time (all online) */
				if ( pss->status[i] != _status ) {
					*payload_ptr++ = i;
				/* And save the status of every station... */
					pss->status[i] = _status;
				}
			}
		/* Calculate the payload size and send it */
			if ( (data_size = (uint8_t *)payload_ptr - (payload + LWS_PRE)) > 0 ) {
			/* */
				if ( lws_write(wsi, payload + LWS_PRE, data_size, LWS_WRITE_BINARY) < (int)data_size ) {
					logit("e", "shake2ws: Client:%s websocket write error!\n", pss->ip);
					lws_close_reason(wsi, LWS_CLOSE_STATUS_UNEXPECTED_CONDITION, NULL, 0);
					return -1;
				}
			/* */
				pss->seq++;
				pss->lasttime = timenow;
			}
		}
	/* */
		lws_set_timer_usecs(wsi, LWS_USEC_PER_SEC);
		break;
	case LWS_CALLBACK_RECEIVE:
	/* */
		if ( pss->totalsta && pss->savemsg )
			destroy_per_session_data( pss );
	/* */
		if ( !pss->msg_size && !pss->savemsg )
			pss->savemsg = (uint8_t *)malloc(MAX_EX_STATION_STRING);
	/* */
		memcpy(pss->savemsg + pss->msg_size, in, len);
		pss->msg_size += len;
	/* */
		if ( lws_is_final_fragment(wsi) ) {
			pss->peak_i   = nPeakValue;
			pss->totalsta = sk2ws_list_list_map( &pss->stapeak, NULL, pss->savemsg, pss->msg_size );
			pss->listtime = sk2ws_list_timestamp_get();
		/* */
			if ( pss->totalsta ) {
				if ( !payload || payload_size < (size_t)(LWS_PRE + sizeof(double) + sizeof(uint32_t) * (pss->totalsta + 1)) ) {
					if ( payload )
						free(payload);
				/* */
					payload_size = LWS_PRE + sizeof(double) + sizeof(uint32_t) * (pss->totalsta + 1);
					if ( !(payload = (uint8_t *)malloc(payload_size)) ) {
						logit("e", "shake2ws: Can't allocate memory for Client:%s in STATION_STATUS.\n", pss->ip);
						lws_close_reason(wsi, LWS_CLOSE_STATUS_UNEXPECTED_CONDITION, NULL, 0);
						return -1;
					}
				}
			/* */
				if ( !(pss->status = calloc(pss->totalsta, sizeof(uint8_t))) ) {
					logit("e", "shake2ws: Can't allocate memory for Client:%s in STATION_STATUS.\n", pss->ip);
					lws_close_reason(wsi, LWS_CLOSE_STATUS_UNEXPECTED_CONDITION, NULL, 0);
					return -1;
				}
			/* Go thru all stations and set the status to online */
				for ( int i = 0; i < pss->totalsta; i++ )
					pss->status[i] = 1;
			}
			else {
				logit("e", "shake2ws: Can't map station list for Client:%s in STATION_STATUS.\n", pss->ip);
				lws_close_reason(wsi, LWS_CLOSE_STATUS_UNEXPECTED_CONDITION, NULL, 0);
				return -1;
			}
		/* */
			logit("ot", "shake2ws: Client:%s request the status of stations.\n", pss->ip);
		}
	/* */
		lws_set_timer_usecs(wsi, LWS_USEC_PER_SEC);
		break;
	case LWS_CALLBACK_CLOSED:
	/* */
		destroy_per_session_data( pss );
		logit("ot", "shake2ws: Client:%s connection close, protocol is STATION_STATUS.\n", pss->ip);
		break;
	default:
		break;
	}

	return 0;
}

/**
 * @brief
 *
 * @param pss
 */
static void init_per_session_data( struct per_session_data *pss )
{
	pss->seq      = 0;
	pss->lasttime = 0;
	pss->totalsta = 0;
	pss->peak_i   = 0;
	pss->msg_size = 0;
	pss->savemsg  = NULL;
	pss->stapeak  = NULL;
	pss->status   = NULL;

	return;
}

/**
 * @brief
 *
 * @param pss
 */
static void destroy_per_session_data( struct per_session_data *pss )
{
/* */
	if ( pss->savemsg ) {
		free(pss->savemsg);
		pss->savemsg = NULL;
	}
	if ( pss->status ) {
		free(pss->status);
		pss->status = NULL;
	}
	if ( pss->stapeak ) {
		free(pss->stapeak);
		pss->stapeak = NULL;
	}

	return;
}
