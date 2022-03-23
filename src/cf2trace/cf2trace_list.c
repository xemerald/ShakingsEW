#define _GNU_SOURCE
/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <search.h>
#include <unistd.h>
#include <errno.h>

/* Earthworm environment header include */
#include <earthworm.h>
#include <transport.h>
#include <trace_buf.h>

/* Local header include */
#include <stalist.h>
#include <recordtype.h>
#include <cf2trace.h>
#include <cf2trace_list.h>


static int  _FetchTraListRing( GETLISTS *, const unsigned short );
static int  _FetchTraListFile( GETLISTS *, const unsigned short );
static int  SCNLCompare( const void *, const void * );	/* The compare function of binary tree search */

#define  MAXLIST  5

static void          *Root = NULL;         /* Root of binary tree */
static volatile _Bool RingSwitch = 0;  /* The switch for getting list from ring. */
static GETLISTS       GetLists[MAXLIST];   /* array for requesting list name */
static uint16_t       nLists = 0;
static uint8_t        Historical = 0;

/*********************************************************************
 *  TraListInit( ) -- Initialization function of Station list.       *
 *  Arguments:                                                       *
 *    ringName   = Name of list ring.                                *
 *    modId      = The module ID of calling process.                 *
 *    ringSwitch = The switch for getting list from ring.            *
 *  Returns:                                                         *
 *     0 = Normal.                                                   *
 *********************************************************************/
int TraListInit( char *ringName, char *modName, uint8_t ringSwitch )
{
	if ( ringSwitch ) {
		sslist_init( ringName, modName );
		RingSwitch = 1;
	}
	else RingSwitch = 0;

	return 0;
}


/*********************************************************************
 *  TraListReg( ) -- Register the station list to GetList array.     *
 *  Arguments:                                                       *
 *    staListName   = Name of station list.                          *
 *    chanListName  = Name of channel list.                          *
 *  Returns:                                                         *
 *     0 = Normal.                                                   *
 *********************************************************************/
int TraListReg( char *staListName, char *chanListName )
{
	if ( nLists+1 >= (int)MAXLIST ) {
		logit( "e",	"cf2trace: Too many station list, maximum is %d!\n", (int)MAXLIST );
		return -1;
	}

	if ( strlen(staListName) < LIST_NAME_LEN )
		strcpy( GetLists[nLists].sslist.stalist, staListName );
	else {
		logit ( "e", "cf2trace: Station list %s name too long, maximum is %d!\n", staListName, (int)LIST_NAME_LEN );
		return -2;
	}

	if ( strlen(chanListName) < LIST_NAME_LEN )
		strcpy( GetLists[nLists].sslist.chanlist, chanListName );
	else {
		logit ( "e", "cf2trace: Station list %s name too long, maximum is %d!\n", chanListName, (int)LIST_NAME_LEN );
		return -2;
	}

	nLists++;

	return 0;
}


/*********************************************************************
 *  TraListFetch( ) -- Initialization function of Station list.      *
 *  Arguments:                                                       *
 *    None.                                                          *
 *  Returns:                                                         *
 *     0 = Normal.                                                   *
 *********************************************************************/
int TraListFetch( void )
{
	if ( RingSwitch )
		return _FetchTraListRing( GetLists, nLists );
	else
		return _FetchTraListFile( GetLists, nLists );
}


/*********************************************************************
 *  _FetchTraListRing( ) -- Get Stations list from list ring.        *
 *  Arguments:                                                       *
 *    _getLists   = Name of list ring.                               *
 *    nLists     = The module ID of calling process.                 *
 *  Returns:                                                         *
 *     0 = Normal.                                                   *
 *********************************************************************/
static int _FetchTraListRing( GETLISTS *_getLists, const unsigned short _nLists )
{
	int ret = 0;
	uint32_t i, j;
	uint32_t count = 0;

	_TRACEINFO *traceinfo = NULL;

	STALIST_HEADER *slptr = NULL;
	STATION_BLOCK  *station_blk = NULL;
	STATION_BLOCK  *station_end = NULL;
	CHAN_BLOCK     *chan_blk = NULL;

	void *root = NULL;
	void *tmproot = Root;

	if ( Historical ) return 0;

/* Initialize */
	for ( i=0; i<_nLists; i++ ) {
	/* Force to update shared station list */
		_getLists[i].sslist.shkey = 0;

		if ( sslist_req( &_getLists[i].sslist ) == SL_REQ_OK ) {
			sslist_attach( &_getLists[i].sslist );
		}
		else {
			logit ( "e", "cf2trace: Request the shared station list for %s error!\n", _getLists[i].sslist.stalist );
			return -1;
		}
	}

	for ( i=0; i<_nLists; i++ ) {
		if ( (slptr = _getLists[i].sslist.slh) != (STALIST_HEADER *)NULL ) {
		/* Wait for the building process */
			sslist_wait( &_getLists[i].sslist, SL_READ );

		/* Move the pointer to begin of each block */
			station_blk = (STATION_BLOCK *)(slptr + 1);
			station_end = (STATION_BLOCK *)((uint8_t *)slptr + station_blk->offset_chaninfo);

		/* Start to parse the information */
			for ( ; station_blk<station_end; station_blk++ ) {
			/* Move the pointer to begin of each block */
				chan_blk = (CHAN_BLOCK *)((uint8_t *)slptr + station_blk->offset_chaninfo);

			/* Deal with the exception when list has some mistake */
				if ( station_blk->nchannels == 0 ) {
					logit ( "e", "cf2trace: There isn't any channels information of %s, please check the database!\n", station_blk->sta );
				}
				else {
					for ( j=0; j<station_blk->nchannels; j++, chan_blk++, traceinfo++ ) {
					/* Allocate the trace information memory */
						traceinfo = (_TRACEINFO *)calloc(1, sizeof(_TRACEINFO));

						if ( traceinfo == NULL ) {
							logit( "e", "cf2trace: Error allocate the memory for trace information!\n" );
							ret = -2;
							goto except;
						}

						traceinfo->seq               = chan_blk->seq;
						traceinfo->recordtype        = chan_blk->recordtype;
						traceinfo->samprate          = (float)chan_blk->samprate;
						traceinfo->conversion_factor = (float)chan_blk->conversion_factor;

						strncpy(traceinfo->sta, station_blk->sta, STA_CODE_LEN);
						strncpy(traceinfo->net, station_blk->net, NET_CODE_LEN);
						strncpy(traceinfo->loc, station_blk->loc, LOC_CODE_LEN);
						strncpy(traceinfo->chan, chan_blk->chan, CHAN_CODE_LEN);

					/* Insert the station information into binary tree */
						if ( tsearch(traceinfo, &root, SCNLCompare) == NULL ) {
							logit( "e", "cf2trace: Error insert trace into binary tree!\n" );
							ret = -2;
							goto except;
						}
						traceinfo = NULL;

					/* Counting the total number of traces */
						count++;
					}
				}
			}

			if ( !_getLists[i].sslist.slh->seq ) Historical = 1;
			sslist_release( &_getLists[i].sslist, SL_READ );
		}
	}

	if ( Historical )
		logit( "o", "cf2trace: Change from Real-Time Mode to the Historical Event Mode!\n" );

	for ( i=0; i<_nLists; i++ )
		if ( _getLists[i].sslist.slh != (STALIST_HEADER *)NULL )
			sslist_detach( &_getLists[i].sslist );

	if ( count ) {
	/* Switch the tree's root */
		Root = root;
	/* Waiting for 500 ms */
		sleep_ew(500);
	/* Free the old one */
		if ( tmproot != (void *)NULL ) tdestroy(tmproot, free);

		logit("o", "cf2trace: Reading %d traces information from shared station list success!\n", count);
		return count;
	}

except:
	if ( traceinfo != (_TRACEINFO *)NULL ) free(traceinfo);
	tdestroy(root, free);

	return ret;
}

/*********************************************************************
 *  _FetchTraListFile( ) -- Get Station list from local file.        *
 *  Arguments:                                                       *
 *    _getLists   = Name of list ring.                               *
 *    _nLists     = The module ID of calling process.                *
 *  Returns:                                                         *
 *     0 = Normal.                                                   *
 *********************************************************************/
static int _FetchTraListFile( GETLISTS *_getLists, const unsigned short _nLists )
{
	int      ret = 0;
	uint32_t i;
	uint32_t count = 0;
	uint16_t readingstatus = 0;

	char line[128];

	char tmpsta[STA_CODE_LEN];
	char tmpnet[NET_CODE_LEN];
	char tmploc[LOC_CODE_LEN];
	char tmptypestr[TYPE_STR_LEN];

	FILE       *fp = NULL;
	_TRACEINFO *traceinfo = NULL;

	void *root = NULL;
	void *tmproot = Root;


	for ( i=0; i<_nLists; i++ ) {
	/* Open list file */
		if ( (fp = fopen(_getLists[i].slist.stalist, "r")) == NULL ) {
			logit("e", "cf2trace: Read traces list from local file error!\n");
			return -1;
		}

	/* Read each line with the stations & channels sequence */
		while ( fgets( line, sizeof(line) - 1, fp ) != NULL ) {
			if ( !strlen(line) ) continue;

			for ( i = 0; i < (int)sizeof(line); i++ ) {
				if ( line[i] == '#' || line[i] == '\n' ) break;
				else if ( line[i] == '\t' || line[i] == ' ' ) continue;
				else
				{
				/* Station line */
					if ( readingstatus == 0 ) {
						if ( sscanf( line, "%*d %hd %s %s %s %*f %*f %*f",
									&readingstatus,
									tmpsta,
									tmpnet,
									tmploc ) != 4 )
						{
							logit( "e", "cf2trace: Read stations information error!\n" );
							ret = -1;
							goto except;
						}

					/* Check the channel number */
						if ( readingstatus == 0 ) {
							logit ( "e", "cf2trace: There is not any channels information of %s, please check the list file!\n", tmpsta );
						}
					}
				/* Channel line */
					else {
					/* Allocate the trace information memory */
						traceinfo = (_TRACEINFO *)calloc(1, sizeof(_TRACEINFO));

						if ( traceinfo == NULL ) {
							logit( "e", "cf2trace: Error allocate the memory for trace information!\n" );
							ret = -2;
							goto except;
						}

						if ( sscanf( line, "%hd %s %s %*s %f %f\n",
									&traceinfo->seq,
									tmptypestr,
									traceinfo->chan,
									&traceinfo->samprate,
									&traceinfo->conversion_factor ) != 5 )
						{
							logit( "e", "cf2trace: Read %s channels information error!\n", tmpsta );
							ret = -1;
							goto except;
						}

						strncpy(traceinfo->sta, tmpsta, STA_CODE_LEN);
						strncpy(traceinfo->net, tmpnet, NET_CODE_LEN);
						strncpy(traceinfo->loc, tmploc, LOC_CODE_LEN);
						traceinfo->recordtype = typestr2num( tmptypestr );

						count++;
						readingstatus--;

					/* Insert the trace information into binary tree */
						if ( tsearch(traceinfo, &root, SCNLCompare) == NULL ) {
							logit( "e", "cf2trace: Error insert trace into binary tree!\n" );
							ret = -2;
							goto except;
						}
						traceinfo = NULL;
					}
					break;
				}
			}
		}
		fclose(fp);
	}

	if ( count ) {
	/* Switch the tree's root */
		Root = root;
	/* Waiting for 500 ms */
		sleep_ew(500);
	/* Free the old one */
		if ( tmproot != (void *)NULL ) tdestroy(tmproot, free);

		logit( "o", "cf2trace: Reading %d traces information success!\n", count );
		return count;
	}

except:
	if ( traceinfo != (_TRACEINFO *)NULL ) free(traceinfo);
	tdestroy(root, free);

	return ret;
}

/*************************************************************
 *  TraListFind( ) -- Output the Trace that match the SCNL.  *
 *  Arguments:                                               *
 *    trh2x = Pointer of the trace you want to find.          *
 *  Returns:                                                 *
 *     NULL = Didn't find the trace.                         *
 *    !NULL = The Pointer to the trace.                      *
 *************************************************************/
_TRACEINFO *TraListFind( TRACE2X_HEADER *trh2x )
{
	_TRACEINFO  key;
	_TRACEINFO *traceptr = NULL;

	strcpy(key.sta,  trh2x->sta);
	strcpy(key.chan, trh2x->chan);
	strcpy(key.net,  trh2x->net);
	strcpy(key.loc,  trh2x->loc);

/* Find which trace */
	if ( (traceptr = tfind(&key, &Root, SCNLCompare)) == NULL ) {
	/* Not found in trace table */
		return NULL;
	}

	traceptr = *(_TRACEINFO **)traceptr;

	return traceptr;
}

/**********************************************************************
 *  SCNLCompare( )  the SCNL compare function of binary tree search   *
 **********************************************************************/
static int SCNLCompare( const void *a, const void *b )
{
	int rc;
	_TRACEINFO *tmpa, *tmpb;

	tmpa = (_TRACEINFO *)a;
	tmpb = (_TRACEINFO *)b;

	rc = strcmp( tmpa->sta, tmpb->sta );
	if ( rc != 0 ) return rc;
	rc = strcmp( tmpa->chan, tmpb->chan );
	if ( rc != 0 ) return rc;
	rc = strcmp( tmpa->net, tmpb->net );
	if ( rc != 0 ) return rc;
	rc = strcmp( tmpa->loc, tmpb->loc );
	return rc;
}


/*************************************************
 *  EndTraList( ) -- End process of Trace list.  *
 *  Arguments:                                   *
 *    None.                                      *
 *  Returns:                                     *
 *    None.                                      *
 *************************************************/
void TraListEnd( void )
{
	tdestroy(Root, free);
	if ( RingSwitch ) sslist_end();

	return;
}
