#define _GNU_SOURCE
/* Standard C header include */
#include <stdlib.h>
#include <string.h>
#include <search.h>
#include <time.h>
#include <json-c/json.h>
/* Earthworm environment header include */
#include <earthworm.h>
/* Local header include */
#include <dl_chain_list.h>
#include <shake2ws.h>
#include <shake2ws_list.h>

static int  compare_snl( const void *, const void * ); /* The compare function of binary tree search */
static void free_stapeak( void * );

static void    *Root = NULL;         /* Root of binary tree */
time_t          TimeListChanged = 0;

/*
 * shake2ws_list_search() - Insert the trace to the tree.
 * Arguments:
 *
 * Returns:
 *    NULL = Didn't find the station.
 *   !NULL = The Pointer to the station.
 */
STATION_PEAK *shake2ws_list_search( const char *sta, const char *net, const char *loc )
{
	int           i;
	STATION_PEAK *result = NULL;

/* Test if already in the tree */
	if ( (result = shake2ws_list_find( sta, net, loc )) == NULL ) {
		result = (STATION_PEAK *)calloc(1, sizeof(STATION_PEAK));
		memcpy(result->sta, sta, TRACE2_STA_LEN);
		memcpy(result->net, net, TRACE2_NET_LEN);
		memcpy(result->loc, loc, TRACE2_LOC_LEN);
		for ( i = 0; i < MAX_TYPE_PEAKVALUE; i++ )
			result->pvalue[i] = NULL_PEAKVALUE;

		if ( (result = tsearch(result, &Root, compare_snl)) == NULL ) {
		/* Something error when insert into the tree */
			return NULL;
		}
		result = *(STATION_PEAK **)result;
		time(&TimeListChanged);
	}

	return result;
}

/*
 * shake2ws_list_find() - Output the Trace that match the SNL.
 * Arguments:
 *
 * Returns:
 *    NULL = Didn't find the trace.
 *   !NULL = The Pointer to the trace.
 */
STATION_PEAK *shake2ws_list_find( const char *sta, const char *net, const char *loc )
{
	STATION_PEAK  key;
	STATION_PEAK *result = NULL;

/* */
	memcpy(key.sta, sta, TRACE2_STA_LEN);
	memcpy(key.net, net, TRACE2_NET_LEN);
	memcpy(key.loc, loc, TRACE2_LOC_LEN);

/* Find which trace */
	if ( (result = tfind(&key, &Root, compare_snl)) == NULL ) {
	/* Not found in trace table */
		return NULL;
	}

	return *(STATION_PEAK **)result;
}

/*
 * shake2ws_list_walk() -
 *  Arguments:
 *
 *  Returns:
 *    None.
 */
void shake2ws_list_walk( void (*action)(const void *, const VISIT, const int) )
{
	twalk(Root, action);
	return;
}

/*
 *
 */
CHAN_PEAK *shake2ws_list_chlist_search( const STATION_PEAK *stapeak, const int pvalue_i, const char *chan )
{
	CHAN_PEAK *result  = NULL;
	DL_NODE   *current = NULL;

/* Test if already in the list */
	if ( (result = shake2ws_list_chlist_find( stapeak, pvalue_i, chan )) == NULL ) {
		result = (CHAN_PEAK *)calloc(1, sizeof(CHAN_PEAK));
		memcpy(result->chan, chan, TRACE2_CHAN_LEN);
	/* */
		current = dl_node_append( (DL_NODE **)&stapeak->chlist[pvalue_i], result );
		if ( current == NULL ) {
			free(result);
			result = NULL;
		}
	}

	return result;
}

/*
 *
 */
CHAN_PEAK *shake2ws_list_chlist_find( const STATION_PEAK *stapeak, const int pvalue_i, const char *chan )
{
	CHAN_PEAK *result  = NULL;
	DL_NODE   *current = (DL_NODE *)stapeak->chlist[pvalue_i];

	for ( ; current != NULL; current = DL_NODE_GET_NEXT( current ) ) {
		result = (CHAN_PEAK *)DL_NODE_GET_DATA( current );
		if ( !strcmp(result->chan, chan) )
			break;
		else
			result = NULL;
	}

	return result;
}

/*
 * shake2ws_list_list_map() -
 */
int shake2ws_list_list_map( void **output, char *tagbuf, void *input_list, const size_t input_len )
{
	STATION_PEAK **result           = NULL;
	char          *json_string      = (char *)input_list;
	int            i, total_station = 0;

	struct json_tokener *tok     = NULL;
	struct json_object  *root    = NULL;
	struct json_object  *total   = NULL;
	struct json_object  *station = NULL;
	struct json_object  *_string = NULL;

/* */
	json_string[input_len] = '\0';
	tok  = json_tokener_new();
	root = json_tokener_parse_ex(tok, json_string, input_len);
	if ( root == NULL ) {
		logit("e", "shake2ws: Parsing the JSON list from remote client failed: %s.\n",
			json_tokener_error_desc(json_tokener_get_error(tok))
		);
		return 0;
	}
	json_tokener_free(tok);
/* */
	if ( !json_object_object_get_ex(root, "total", &total) || !json_object_object_get_ex(root, "station", &station) ) {
		logit("e", "shake2ws: Parsing the information of JSON list from remote client failed.\n");
		json_object_put(root);
		return 0;
	}
	total_station = json_object_get_int(total);

	result = (STATION_PEAK **)calloc(total_station, sizeof(STATION_PEAK *));
	if ( result == NULL ) {
		logit("e", "shake2ws: Allocate the memory for mapping station list failed.\n");
		json_object_put(root);
		return 0;
	}
/* */
	for ( i = 0; i < total_station; i++ ) {
		_string   = json_object_array_get_idx(station, i);
		result[i] = shake2ws_list_station_map( json_object_get_string(_string), "TW", "--" );
	}
/* */
	if ( tagbuf != NULL ) {
		if ( json_object_object_get_ex(root, "tag", &_string) )
			strcpy(tagbuf, json_object_get_string(_string));
	}
	json_object_put(root);
	*output = result;

	return total_station;
}

/*
 * shake2ws_list_station_map() -
 */
STATION_PEAK *shake2ws_list_station_map( const char *sta, const char *net, const char *loc )
{
	STATION_PEAK       *result   = NULL;
	static STATION_PEAK null_sta = {
		{ 0 }, { 0 }, { 0 }, { 0 }, { 0.0 }, NULL_PEAKVALUE_ARRAY, { 0 }
	};

/* */
	if ( (result = shake2ws_list_find( sta, net, loc )) == NULL ) {
		/* printf("shake2ws: Can't find the %s.%s.%s; assigned the null information!\n", sta, net, loc); */
		result = &null_sta;
	}

	return result;
}

/*
 * shake2ws_list_end() - End process of stations' list.
 * Arguments:
 *   None.
 * Returns:
 *   None.
 */
void shake2ws_list_end( void )
{
	tdestroy(Root, free_stapeak);
	return;
}


/*
 * compare_snl() - The SNL compare function of binary tree search
 */
static int compare_snl( const void *a, const void *b )
{
	STATION_PEAK *tmpa = (STATION_PEAK *)a;
	STATION_PEAK *tmpb = (STATION_PEAK *)b;
	int rc;

	if ( (rc = strcmp( tmpa->sta, tmpb->sta )) != 0 )
		return rc;
	if ( (rc = strcmp( tmpa->net, tmpb->net )) != 0 )
		return rc;
	return strcmp( tmpa->loc, tmpb->loc );
}

/*
 * free_stapeak() -
 */
static void free_stapeak( void *node )
{
	int           i;
	STATION_PEAK *stapeak = (STATION_PEAK *)node;

	for ( i = 0; i < MAX_TYPE_PEAKVALUE; i++ )
		dl_list_destroy( (DL_NODE **)&stapeak->chlist[i], free );

	free(stapeak);
	return;
}
