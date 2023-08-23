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

/* */
static void init_per_session_data( struct per_session_data * );
static void destroy_per_session_data( struct per_session_data * );

/* */
extern volatile uint16_t nPeakValue;
extern volatile uint16_t nIntensity;

/*
 *
 */
int shake2ws_protocols_map_shake(
	struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len
) {
	int    i;
	time_t timenow;
	char   tagbuf[16] = { 0 };
/* */
	static uint8_t *payload      = NULL;
	static size_t   payload_size = 0;
	uint8_t        *payload_ptr  = NULL;
	size_t          data_size;
	STATION_PEAK   *_stapeak = NULL;

	struct per_session_data *pss = (struct per_session_data *)user;

/* Get the time of present */
	time(&timenow);
/* Start to define the callback condition */
	switch ( reason ) {
	case LWS_CALLBACK_PROTOCOL_DESTROY:
		if ( payload != NULL && payload_size ) {
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
	case LWS_CALLBACK_SERVER_WRITEABLE:
		if ( pss->totalsta && payload != NULL ) {
			if ( (timenow - pss->lasttime) >= 1 ) {
				payload_ptr = payload + LWS_PRE;
				if ( sk2ws_list_timestamp_get() > pss->listtime ) {
				/* Re-map the station list */
					if ( pss->stapeak != NULL ) {
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
				for ( i = 0; i < pss->totalsta; i++ ) {
					_stapeak = ((STATION_PEAK **)pss->stapeak)[i];
					*(payload_ptr + i) = _stapeak->intensity[pss->peak_i];
				}
			/* */
				data_size = pss->totalsta + 1;
				if ( lws_write(wsi, payload_ptr, data_size, LWS_WRITE_BINARY) < (int)data_size ) {
					logit("e", "shake2ws: Client:%s websocket write error!\n", pss->ip);
					lws_close_reason(wsi, LWS_CLOSE_STATUS_UNEXPECTED_CONDITION, NULL, 0);
					return -1;
				}
				pss->lasttime = timenow;
			}
		}
		break;
	case LWS_CALLBACK_RECEIVE:
	/* */
		if ( pss->totalsta && pss->savemsg != NULL )
			destroy_per_session_data( pss );
	/* */
		if ( !pss->msg_size && pss->savemsg == NULL )
			pss->savemsg = (uint8_t *)malloc(MAX_EX_STATION_STRING);
		memcpy(pss->savemsg + pss->msg_size, in, len);
		pss->msg_size += len;
	/* */
		if ( lws_is_final_fragment(wsi) ) {
			pss->lasttime = timenow;
			pss->totalsta = sk2ws_list_list_map( &pss->stapeak, tagbuf, pss->savemsg, pss->msg_size );
			pss->peak_i   = atoi(tagbuf);
			pss->listtime = sk2ws_list_timestamp_get();
		/* */
			if ( pss->totalsta && pss->peak_i < nIntensity ) {
				if ( payload == NULL || payload_size < (size_t)(LWS_PRE + pss->totalsta + 1) ) {
					if ( payload != NULL ) {
						free(payload);
						payload = NULL;
					}
					payload_size = LWS_PRE + pss->totalsta + 1;
					payload      = (uint8_t *)malloc(payload_size);
				}
				if ( payload == NULL ) {
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
			logit("ot", "shake2ws: Client:%s request the #%d value in MAP_SHAKE.\n", pss->ip, pss->peak_i);
		}
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

/*
 *
 */
int shake2ws_protocols_station_shake(
	struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len
) {
	int    i;
	time_t timenow;
/* */
	static uint8_t *payload      = NULL;
	static size_t   payload_size = 0;
	static uint8_t *payload_ptr  = NULL;
	static size_t   data_size    = 0;

	struct per_session_data *pss = (struct per_session_data *)user;

/* Get the time of present */
	time(&timenow);
/* Start to define the callback condition */
	switch ( reason ) {
	case LWS_CALLBACK_PROTOCOL_INIT:
		if ( payload == NULL ) {
			payload_size = LWS_PRE + sizeof(double) * nPeakValue + 1;
			payload      = (uint8_t *)malloc(payload_size);
			payload_ptr  = payload + LWS_PRE;
			data_size    = sizeof(double) * nPeakValue;
		}
		break;
	case LWS_CALLBACK_PROTOCOL_DESTROY:
		if ( payload != NULL && payload_size ) {
			free(payload);
			payload      = NULL;
			payload_ptr  = NULL;
			payload_size = 0;
		}
		break;
	case LWS_CALLBACK_ESTABLISHED:
		init_per_session_data( pss );
		pss->savemsg  = (uint8_t *)malloc(TRACE2_STA_LEN);
		pss->msg_size = TRACE2_STA_LEN;
		lws_get_peer_simple(wsi, pss->ip, INET6_ADDRSTRLEN);
		logit("ot", "shake2ws: Client:%s is connected, protocol is STATION_SHAKE.\n", pss->ip);
		break;
	case LWS_CALLBACK_SERVER_WRITEABLE:
		if ( pss->totalsta ) {
			if ( (timenow - pss->lasttime) >= 1 ) {
				if ( sk2ws_list_timestamp_get() > pss->listtime ) {
				/* Re-map the station link */
					pss->stapeak  = sk2ws_list_station_map( (char *)pss->savemsg, "TW", "--" );
					pss->listtime = sk2ws_list_timestamp_get();
				}
				for ( i = 0; i < nPeakValue; i++ )
					*((double *)payload_ptr + i) = ((STATION_PEAK *)pss->stapeak)->pvalue[i];

				if ( lws_write(wsi, payload_ptr, data_size, LWS_WRITE_BINARY) < (int)data_size ) {
					logit("e", "shake2ws: Client:%s websocket write error!\n", pss->ip);
					return -1;
				}
			    pss->lasttime = timenow;
			}
		}
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
		break;
	case LWS_CALLBACK_CLOSED:
		logit("ot", "shake2ws: Client:%s connection close, protocol is STATION_SHAKE.\n", pss->ip);
		break;
	default:
		break;
	}

	return 0;
}

/*
 *
 */
int shake2ws_protocols_station_status(
	struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len
) {
	int    i;
	time_t timenow;
/* */
	static uint8_t *payload      = NULL;
	static size_t   payload_size = 0;
	uint32_t       *payload_ptr  = NULL;
	size_t          data_size;
	STATION_PEAK   *_stapeak = NULL;
	_Bool           _status;

	struct per_session_data *pss = (struct per_session_data *)user;

/* Get the time of present */
	time(&timenow);
/* Start to define the callback condition */
	switch ( reason ) {
	case LWS_CALLBACK_PROTOCOL_DESTROY:
		if ( payload != NULL && payload_size ) {
			free(payload);
			payload = NULL;
			payload_size = 0;
		}
		break;
	case LWS_CALLBACK_ESTABLISHED:
		init_per_session_data( pss );
		lws_get_peer_simple(wsi, pss->ip, INET6_ADDRSTRLEN);
		logit("ot", "shake2ws: Client:%s is connected, protocol is STATION_STATUS.\n", pss->ip);
		break;
	case LWS_CALLBACK_SERVER_WRITEABLE:
		if ( pss->totalsta && payload != NULL ) {
			if ( (timenow - pss->lasttime) >= 1 ) {
			/* */
				payload_ptr    = (uint32_t *)(payload + LWS_PRE);
				*payload_ptr++ = pss->seq;
			/* */
				if ( sk2ws_list_timestamp_get() > pss->listtime ) {
				/* Re-map the station list */
					if ( pss->stapeak != NULL ) {
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
				for ( i = 0; i < pss->totalsta; i++ ) {
					_stapeak = ((STATION_PEAK **)pss->stapeak)[i];
					_status  = _stapeak->pvalue[0] < 0.0 ? 0 : 1;
				/* After the first time, we just notify those stations with different status than last time */
					if ( pss->peak_i == 0 ) {
						if ( pss->status[i] != _status )
							*payload_ptr++ = i;
					}
				/* In the first time, we just notify the stations which are offline. */
					else {
						if ( !_status )
							*payload_ptr++ = i;
					}
				/* And save the status of every station... */
					pss->status[i] = _status;
				}
			/* */
				if ( (data_size = (uint8_t *)payload_ptr - (payload + LWS_PRE)) > 0 ) {
					payload_ptr = (uint32_t *)(payload + LWS_PRE);
				/* */
					if ( lws_write(wsi, (uint8_t *)payload_ptr, data_size, LWS_WRITE_BINARY) < (int)data_size ) {
						logit("e", "shake2ws: Client:%s websocket write error!\n", pss->ip);
						lws_close_reason(wsi, LWS_CLOSE_STATUS_UNEXPECTED_CONDITION, NULL, 0);
						return -1;
					}
					pss->seq++;
					pss->lasttime = timenow;
				/* We use the unused peak_i as first time flag */
					if ( pss->peak_i != 0 )
						pss->peak_i = 0;
				}
			}
		}
		break;
	case LWS_CALLBACK_RECEIVE:
	/* */
		if ( pss->totalsta && pss->savemsg != NULL )
			destroy_per_session_data( pss );
	/* */
		if ( !pss->msg_size && pss->savemsg == NULL )
			pss->savemsg = (uint8_t *)malloc(MAX_EX_STATION_STRING);
		memcpy(pss->savemsg + pss->msg_size, in, len);
		pss->msg_size += len;
	/* */
		if ( lws_is_final_fragment(wsi) ) {
			pss->peak_i   = nPeakValue;
			pss->totalsta = sk2ws_list_list_map( &pss->stapeak, NULL, pss->savemsg, pss->msg_size );
			pss->listtime = sk2ws_list_timestamp_get();
		/* */
			if ( pss->totalsta ) {
				if ( payload == NULL || payload_size < (size_t)(LWS_PRE + sizeof(uint32_t) * pss->totalsta + 1) ) {
					if ( payload != NULL ) {
						free(payload);
						payload = NULL;
					}
					payload_size = LWS_PRE + sizeof(uint32_t) * pss->totalsta + 1;
					payload      = (uint8_t *)malloc(payload_size);
				}
				pss->status = calloc(pss->totalsta, sizeof(uint8_t));
				if ( payload == NULL || pss->status == NULL ) {
					logit("e", "shake2ws: Can't allocate memory for Client:%s in STATION_STATUS.\n", pss->ip);
					lws_close_reason(wsi, LWS_CLOSE_STATUS_UNEXPECTED_CONDITION, NULL, 0);
					return -1;
				}
			}
			else {
				logit("e", "shake2ws: Can't map station list for Client:%s in STATION_STATUS.\n", pss->ip);
				lws_close_reason(wsi, LWS_CLOSE_STATUS_UNEXPECTED_CONDITION, NULL, 0);
				return -1;
			}
		/* */
			logit("ot", "shake2ws: Client:%s request the status of stations.\n", pss->ip);
		}
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

/*
 *
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

/*
 *
 */
static void destroy_per_session_data( struct per_session_data *pss )
{
/* */
	if ( pss->savemsg != NULL ) {
		free(pss->savemsg);
		pss->savemsg = NULL;
	}
	if ( pss->status != NULL ) {
		free(pss->status);
		pss->stapeak = NULL;
	}
	if ( pss->stapeak != NULL ) {
		free(pss->stapeak);
		pss->status = NULL;
	}

	return;
}
