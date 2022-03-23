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

/* Local header include */
#include <stalist.h>
#include <peak2trig.h>
#include <peak2trig_list.h>


static int  _FetchStaListRing( GETLISTS *, const unsigned short );
static int  _FetchStaListFile( GETLISTS *, const unsigned short );
static int  SNLCompare( const void *, const void * );	/* The compare function of binary tree search */

#define  MAXLIST  5

static void          *Root = NULL;         /* Root of binary tree */
static volatile _Bool RingSwitch = 0;      /* The switch for getting list from ring. */
static GETLISTS       GetLists[MAXLIST];   /* Array for requesting list name */
static uint16_t       nLists = 0;
static uint8_t        Historical = 0;

/*********************************************************************
 *  StaListInit( ) -- Initialization function of Station list.       *
 *  Arguments:                                                       *
 *    ringName   = Name of list ring.                                *
 *    modId      = The module ID of calling process.                 *
 *    ringSwitch = The switch for getting list from ring.            *
 *  Returns:                                                         *
 *     0 = Normal.                                                   *
 *********************************************************************/
int StaListInit( char *ringName, char *modName, uint8_t ringSwitch )
{
	if ( ringSwitch ) {
		sslist_init( ringName, modName );
		RingSwitch = 1;
	}
	else RingSwitch = 0;

	return 0;
}


/*********************************************************************
 *  StaListReg( ) -- Register the station list to GetList array.     *
 *  Arguments:                                                       *
 *    staListName   = Name of station list.                          *
 *    chanListName  = Name of channel list.                          *
 *  Returns:                                                         *
 *     0 = Normal.                                                   *
 *********************************************************************/
int StaListReg( char *staListName, char *chanListName )
{
	if ( nLists+1 >= (int)MAXLIST ) {
		logit( "e",	"peak2trig: Too many station list, maximum is %d!\n", (int)MAXLIST );
		return -1;
	}

	if ( strlen(staListName) < LIST_NAME_LEN )
		strcpy( GetLists[nLists].sslist.stalist, staListName );
	else {
		logit ( "e", "peak2trig: Station list %s name too long, maximum is %d!\n", staListName, (int)LIST_NAME_LEN );
		return -2;
	}

	if ( strlen(chanListName) < LIST_NAME_LEN )
		strcpy( GetLists[nLists].sslist.chanlist, chanListName );
	else {
		logit ( "e", "peak2trig: Station list %s name too long, maximum is %d!\n", chanListName, (int)LIST_NAME_LEN );
		return -2;
	}

	nLists++;

	return 0;
}


/*********************************************************************
 *  StaListFetch( ) -- Initialization function of Station list.      *
 *  Arguments:                                                       *
 *    None.                                                          *
 *  Returns:                                                         *
 *     0 = Normal.                                                   *
 *********************************************************************/
int StaListFetch( void )
{
	if ( RingSwitch )
		return _FetchStaListRing( GetLists, nLists );
	else
		return _FetchStaListFile( GetLists, nLists );
}


/*********************************************************************
 *  _FetchStaListRing( ) -- Get Stations list from list ring.        *
 *  Arguments:                                                       *
 *    _getLists  = Name of list ring.                                *
 *    nLists     = The module ID of calling process.                 *
 *  Returns:                                                         *
 *     0 = Normal.                                                   *
 *********************************************************************/
static int _FetchStaListRing( GETLISTS *_getLists, const unsigned short _nLists )
{
	int      ret = 0;
	uint32_t i;
	uint32_t count = 0;

	_STAINFO *stainfo = NULL;

	STALIST_HEADER *slptr = NULL;
	STATION_BLOCK  *station_blk = NULL;
	STATION_BLOCK  *station_end = NULL;

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
			logit ( "e", "peak2trig: Request the shared station list for %s error!\n", _getLists[i].sslist.stalist );
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
			/* Allocate the station information memory */
				stainfo = (_STAINFO *)calloc(1, sizeof(_STAINFO));

				if ( stainfo == NULL ) {
					logit( "e", "peak2trig: Error allocate the memory for station information!\n" );
					ret = -2;
					goto except;
				}

				strncpy(stainfo->sta, station_blk->sta, STA_CODE_LEN);
				strncpy(stainfo->net, station_blk->net, NET_CODE_LEN);
				strncpy(stainfo->loc, station_blk->loc, LOC_CODE_LEN);

				stainfo->latitude    = station_blk->latitude;
				stainfo->longitude   = station_blk->longitude;
				stainfo->elevation   = station_blk->elevation;

			/* Insert the station information into binary tree */
				if ( tsearch(stainfo, &root, SNLCompare) == NULL ) {
					logit( "e", "peak2trig: Error insert station into binary tree!\n" );
					ret = -2;
					goto except;
				}
				stainfo = NULL;

			/* Counting the total number of traces */
				count++;
			}

			if ( !_getLists[i].sslist.slh->seq ) Historical = 1;
			sslist_release( &_getLists[i].sslist, SL_READ );
		}
	}

	if ( Historical )
		logit( "o", "peak2trig: Change from Real-Time Mode to the Historical Event Mode!\n" );

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

		logit("o", "peak2trig: Reading %d traces information from shared station list success!\n", count);
		return count;
	}

except:
	if ( stainfo != (_STAINFO *)NULL ) free(stainfo);
	tdestroy(root, free);

	return ret;
}

/*********************************************************************
 *  _FetchStaListFile( ) -- Get Station list from local file.       *
 *  Arguments:                                                       *
 *    _getLists   = Name of list ring.                               *
 *    _nLists     = The module ID of calling process.                *
 *  Returns:                                                         *
 *     0 = Normal.                                                   *
 *********************************************************************/
static int _FetchStaListFile( GETLISTS *_getLists, const unsigned short _nLists )
{
	int      ret = 0;
	uint32_t i;
	uint32_t count = 0;
	uint16_t readingstatus = 0;

	char line[128];

	FILE     *fp = NULL;
	_STAINFO *stainfo = NULL;

	void *root = NULL;
	void *tmproot = Root;


	for ( i=0; i<_nLists; i++ ) {
	/* Open list file */
		if ( (fp = fopen(_getLists[i].slist.stalist, "r")) == NULL ) {
			logit("e", "peak2trig: Read stations list from local file error!\n");
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
					/* Allocate the trace information memory */
						stainfo = (_STAINFO *)calloc(1, sizeof(_STAINFO));

						if ( stainfo == NULL ) {
							logit( "e", "peak2trig: Error allocate the memory for station information!\n" );
							ret = -2;
							goto except;
						}

						if ( sscanf( line, "%*d %hd %s %s %s %lf %lf %lf",
									&readingstatus,
									stainfo->sta,
									stainfo->net,
									stainfo->loc,
									&stainfo->latitude,
									&stainfo->longitude,
									&stainfo->elevation ) != 7 )
						{
							logit( "e", "peak2trig: Read stations information error!\n" );
							ret = -1;
							goto except;
						}

						count++;

					/* Check the channel number */
						if ( readingstatus == 0 ) {
							logit ( "e", "peak2trig: There is not any channels information of %s, please check the list file!\n", stainfo->sta );
						}
					}
				/* Channel line */
					else {
						if ( sscanf( line, "%*d %*s %*s %*s %*f %*f\n" ) != 0 ) {
							logit( "e", "peak2trig: Read %s channels information error!\n", stainfo->sta );
							ret = -1;
							goto except;
						}

						if ( --readingstatus == 0 ) {
						/* Insert the station information into binary tree */
							if ( tsearch(stainfo, &root, SNLCompare) == NULL ) {
								logit( "e", "peak2trig: Error insert station into binary tree!\n" );
								ret = -2;
								goto except;
							}
							stainfo = NULL;
						}
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

		logit( "o", "peak2trig: Reading %d Palerts information success!\n", count );
		return count;
	}

except:
	if ( stainfo != (_STAINFO *)NULL ) free(stainfo);
	tdestroy(root, free);

	return ret;
}

/*************************************************************
 *  StaListFind( ) -- Output the Station that match the SNL. *
 *  Arguments:                                               *
 *    tpv = Pointer of the trace you want to find.           *
 *  Returns:                                                 *
 *     NULL = Didn't find the trace.                         *
 *    !NULL = The Pointer to the trace.                      *
 *************************************************************/
_STAINFO *StaListFind( _STAINFO *key )
{
	_STAINFO *staptr = NULL;

/* Find which trace */
	if ( (staptr = tfind(key, &Root, SNLCompare)) == NULL ) {
	/* Not found in trace table */
		return NULL;
	}

	return *(_STAINFO **)staptr;
}

/*********************************************************************
 *  SNLCompare( )  the SNL compare function of binary tree search    *
 *********************************************************************/
static int SNLCompare( const void *a, const void *b )
{
	int rc;
	_STAINFO *tmpa, *tmpb;

	tmpa = (_STAINFO *)a;
	tmpb = (_STAINFO *)b;

	rc = strcmp( tmpa->sta, tmpb->sta );
	if ( rc != 0 ) return rc;
	rc = strcmp( tmpa->net, tmpb->net );
	if ( rc != 0 ) return rc;
	rc = strcmp( tmpa->loc, tmpb->loc );
	return rc;
}

/*************************************************
 *  EndStaList( ) -- End process of Trace list.  *
 *  Arguments:                                   *
 *    None.                                      *
 *  Returns:                                     *
 *    None.                                      *
 *************************************************/
void StaListEnd( void )
{
	tdestroy(Root, free);
	if ( RingSwitch ) sslist_end();

	return;
}
