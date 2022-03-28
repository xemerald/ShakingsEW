
/*
 * scnfilter.c contains a function to filter messages based
 *               on content of site-component-network-loc fields.
 *               Works on TYPE_TRACEBUF, TYPE_TRACE_COMP_UA,
 *               TYPE_PICK2K, TYPE_CODA2K, messages.
 *
 *               It can also rename channels in waveform data
 *               if desired.
 *
 *               Returns: 1 if this site-component-network-loc
 *                          is on the list of requested scnl's.
 *                        0 if it isn't.
 *                       -1 if it's an unknown message type.
 *
 * Origin from: 981112 Lynn Dietz
 *
 * Modified by: 20220328 Benjamin Yang @ National Taiwan University
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <time_ew.h>
#include <earthworm.h>
#include <trace_buf.h>
#include <kom.h>
#include <swap.h>
#include <priority_queue.h>
#include <rdpickcoda.h>

/* Globals for site-comp-net-loc filter */
static int   FilterInit = 0;     /* initialization flag                    */
static int   Max_SCNL   = 0;     /* working limit on # scnl's to ship      */
static int   nSCNL      = 0;     /* # of scnl's we're configured to ship   */
static int   nWild      = 0;     /* # of Wildcards used in config file     */
static char *Wild       = "*";   /* wildcard string for SCNL               */

#define INCREMENT_SCNL  10                   /* increment the limit of # scnl's    */
#define STA_STRLEN    (TRACE2_STA_LEN - 1)   /* max string-length of station code  */
#define CHAN_STRLEN   (TRACE2_CHAN_LEN - 1)  /* max string-length of component code*/
#define NET_STRLEN    (TRACE2_NET_LEN - 1)   /* max string-length of network code  */
#define LOC_STRLEN    (TRACE2_LOC_LEN - 1)   /* max string-length of location code */

typedef struct {
   char sta[TRACE2_STA_LEN];     /* original station name  */
   char chan[TRACE2_CHAN_LEN];   /* original channel name  */
   char net[TRACE2_NET_LEN];     /* original network name  */
   char loc[TRACE2_LOC_LEN];     /* original location code */

   char rsta[TRACE2_STA_LEN];    /* remapped station name  */
   char rchan[TRACE2_CHAN_LEN];  /* remapped channel name  */
   char rnet[TRACE2_NET_LEN];    /* remapped network name  */
   char rloc[TRACE2_LOC_LEN];    /* remapped location code */

   int  remap;                /* flag: 1=remap SCNL, 0=don't */
   int  block;                /* flag: 1=block SCNL, 0=don't */
   EW_PRIORITY pri;           /* 20020319 dbh added    */
} _SCNL;

_SCNL *Lists = NULL;       /* list of SCNL's to accept & rename   */

/* Local function prototypes & macros */
static char  *copytrim( char *, char *, int );
static double WaveMsgTime( TRACE2_HEADER * );
static _SCNL *realloc_scnl_list( const char * );
static int    compare_scnl( const void *s1, const void *s2 );  /* qsort & bsearch */

#define IS_WILD(x)     (!strcmp((x),Wild))
#define NotMatch(x,y)  strcmp((x),(y))

/*
 * scnlfilter_com() - processes config-file commands to
 *                      set up the filter & send-list.
 * Returns  1 if the command was recognized & processed
 *          0 if the command was not recognized
 * Note: this function may exit the process if it finds
 *       serious errors in any commands
 */
int scnlfilter_com( const char *prog )
{
	char *str;

/* Add a block entry to the send-list without remapping */
	if ( k_its("Block_SCNL") ) {
	/* This SCNL should be blocked */
		Lists[nSCNL].block = 1;
		Lists[nSCNL].remap = 0;
	}
/* Add an entry to the send-list without remapping */
	else if ( k_its("Allow_SCNL") ) {
	/* This SCNL should NOT be blocked */
		Lists[nSCNL].block = 0;
		Lists[nSCNL].remap = 0;
	}
/* Add an entry to the send-list with remapping */
	else if ( k_its("Allow_SCNL_Remap") ) {
	/* This SCNL should NOT be blocked */
		Lists[nSCNL].block = 0;
		Lists[nSCNL].remap = 1;
	}
	else {
		return 0;
	}

/* */
	if ( nSCNL >= Max_SCNL )
		realloc_scnl_list( prog );
/* Read original SCNL */
/* original station code */
	if ( (str = k_str()) ) {
		if ( strlen(str) > (size_t)STA_STRLEN )
			goto except_com;
		if ( IS_WILD(str) )
			nWild++;  /* count SCNL wildcards */
		strcpy(Lists[nSCNL].sta, str);
	}
/* original component code */
	if ( (str = k_str()) ) {
		if ( strlen(str) > (size_t)CHAN_STRLEN )
			goto except_com;
		if ( IS_WILD(str) )
			nWild++;  /* count SCNL wildcards */
		strcpy(Lists[nSCNL].chan,str);
	}
/* original network code */
	if ( (str = k_str()) ) {
		if ( strlen(str) > (size_t)NET_STRLEN )
			goto except_com;
		if ( IS_WILD(str) )
			nWild++;  /* count SCNL wildcards */
		strcpy(Lists[nSCNL].net,str);
	}
/* original location code */
	if ( (str = k_str()) ) {
		if ( strlen(str) > (size_t)LOC_STRLEN )
			goto except_com;
		if ( IS_WILD(str) )
			nWild++;  /* count SCNL wildcards */
		strcpy(Lists[nSCNL].loc,str);
	}
/* Read remap SCNL  */
	if ( Lists[nSCNL].remap ) {
	/* remap station code */
		if ( (str = k_str()) ) {
			if ( strlen(str) > (size_t)STA_STRLEN )
				goto except_com;
			strcpy(Lists[nSCNL].rsta, str);
		}
	/* remap component code */
		if ( (str = k_str()) ) {
			if ( strlen(str) > (size_t)CHAN_STRLEN )
				goto except_com;
			strcpy(Lists[nSCNL].rchan, str);
		}
	/* remap network code */
		if ( (str = k_str()) ) {
			if ( strlen(str) > (size_t)NET_STRLEN )
				goto except_com;
			strcpy(Lists[nSCNL].rnet, str);
		}
	/* remap location code */
		if ( (str = k_str()) ) {
			if ( strlen(str) > (size_t)LOC_STRLEN )
				goto except_com;
			strcpy(Lists[nSCNL].rloc, str);
		}
	}
/* */
	nSCNL++;

	return 0;

except_com:
	logit(
		"e", "%s: Error in command(s) (argument either missing, too long, or invalid): "
		"\"%s\"\n", prog, k_com()
	);
	logit("e", "%s: exiting!\n", prog);
	exit(-1);
}


/*
 * scnlfilter_init() - Make sure all the required commands were found in the config file,
 *                     do any other startup things necessary for filter to work properly
 */
int scnlfilter_init( const char *prog )
{
	int i;

/* Check to see if required config-file commands were processed */
	if ( nSCNL == 0 ) {
		logit(
			"e", "%s: No <Allow_SCNL> or <Allow_SCNL_Remap> commands in config file;"
			" no data will be processed!\n", prog
		);
		return -1;
	}
/* Look up message types of we can deal with */
	if ( GetType("TYPE_TRACEBUF", &TypeTraceBuf) != 0 ) {
		logit("e", "%s: Invalid message type <TYPE_TRACEBUF>\n", prog);
		return -1;
	}
	if ( GetType("TYPE_TRACEBUF2", &TypeTraceBuf2) != 0 ) {
		logit("e", "%s: Invalid message type <TYPE_TRACEBUF2>\n", prog);
		return( -1 );
	}

/* Sort and Log list of SCNL's that we're handling */
	if ( nWild == 0 ) {
		qsort(Lists, nSCNL, sizeof(_SCNL), compare_scnl);
		logit(
			"o", "%s: no wildcards in requested channel list;"
			" will use binary search for SCNL matching.\n", prog
		);
	}
	else {
		logit(
			"o", "%s: wildcards present in requested channel list;"
			" must use linear search for SCNL matching.\n", prog
		);
	}

	logit("o", "%s: configured to handle %d channels:", prog, nSCNL);
	for ( i = 0; i < nSCNL; i++ ) {
		if ( Lists[i].block ) {
			logit(
				"o","\n   Blocking channel[%d]: %5s %3s %2s %2s",
				i, Lists[i].sta, Lists[i].chan, Lists[i].net, Lists[i].loc
			);
		}
		else {
			logit(
				"o","\n   Listsing channel[%d]: %5s %3s %2s %2s",
				i, Lists[i].sta, Lists[i].chan, Lists[i].net, Lists[i].loc
			);
		}
		if ( Lists[i].remap ) {
			logit(
				"o","   mapped to %5s %3s %2s %2s",
				Lists[i].rsta, Lists[i].rchan, Lists[i].rnet, Lists[i].rloc
			);
		}
	}
	logit("o","\n");
	FilterInit = 1;

	return 0;
}

/*
 * scnlfilter_apply() - looks at the candidate message.
 *   returns: the priority if SCNL is in "send" list
 *            otherwise EW_PRIORITY_NONE (0) if not found
 *
 *   20020319 dbh Changed the return code handling
 */
int scnlfilter_apply(
	void  *inmsg,  size_t inlen,   unsigned char  intype,
	void **outmsg, size_t *outlen, unsigned char *outtype
) {
	TRACE_HEADER  *thd;
	TRACE2_HEADER *thd2;
	_SCNL          key;          /* Key for binary search       */
	_SCNL         *match;        /* Pointer into Lists-list      */
	double         packettime;   /* starttime of data in packet */

	if ( !FilterInit )
		scnlfilter_init();

/* Read SCNL of a TYPE_TRACEBUF2 or TYPE_TRACE2_COMP_UA message */
	if ( intype == TypeTraceBuf2 ) {
		thd2 = (TRACE2_HEADER *)inmsg;
		copytrim( key.sta,  thd2->sta,  STATION_LEN );
		copytrim( key.chan, thd2->chan, CHAN_LEN    );
		copytrim( key.net,  thd2->net,  NETWORK_LEN );
		copytrim( key.loc,  thd2->loc,  LOC_LEN     );
	}
/* Read SCN of a TYPE_TRACEBUF or TYPE_TRACE_COMP_UA message */
	else if ( intype == TypeTraceBuf ) {
		thd = (TRACE_HEADER *)inmsg;
		copytrim( key.sta,  thd->sta,        STATION_LEN );
		copytrim( key.chan, thd->chan,       CHAN_LEN    );
		copytrim( key.net,  thd->net,        NETWORK_LEN );
		copytrim( key.loc,  LOC_NULL_STRING, LOC_LEN     );
	}
/* Or else we don't know how to read this type of message */
	else {
		return 0;
	}

/* Look for the message's SCNL in the Lists-list. */
   match = (_SCNL *) NULL;

/*
 * Use the more efficient binary search if there
 * were no wildcards in the original SCNL request list.
 */
	if ( nWild == 0 ) {
		match = (_SCNL *)bsearch( &key, Lists, nSCNL, sizeof(_SCNL), compare_scnl );
		if ( match != NULL && match->block == 1 )
			match = (_SCNL *)NULL; /* SCNL is marked to be blocked */
	}
/* Gotta do it the linear way if wildcards were used! */
	else {
		int i;
		for ( i = 0; i < nSCNL; i++ ) {
			_SCNL *next = &(Lists[i]);
			if ( !IS_WILD(next->sta)  && NotMatch(next->sta, key.sta)  ) continue;
			if ( !IS_WILD(next->chan) && NotMatch(next->chan,key.chan) ) continue;
			if ( !IS_WILD(next->net)  && NotMatch(next->net, key.net)  ) continue;
			if ( !IS_WILD(next->loc)  && NotMatch(next->loc, key.loc)  ) continue;
			if ( next->block != 1 )
			match = next;  /* found a match! */
			break;
		}
	}

	if ( match == NULL ) {
		/* logit(
			"e","scnlfilter: rejecting msgtype:%d from %s %s %s %s\n",
			intype, key.sta, key.chan, key.net, key.loc
		); */   /*DEBUG*/
		return 0;   /* SCNL not in Lists list */
	}

/*
 * Found a message we want to ship!
 * Do some extra checks for trace data only
 */
	if ( intype == TypeTraceBuf2 || intype == TypeTraceBuf ) {
	/* Rename its SCNL if appropriate */
		if ( match->remap ) {
			if ( !IS_WILD( match->rsta ) )
				strcpy(thd2->sta, match->rsta);
			if ( !IS_WILD( match->rchan ) )
				strcpy(thd2->chan, match->rchan);
			if ( !IS_WILD( match->rnet ) )
				strcpy(thd2->net, match->rnet);
			if ( !IS_WILD( match->rloc ) && intype == TypeTraceBuf2 )
				strcpy(thd2->loc, match->rloc);
		}
	}

/* Copy message to output buffer */
	memcpy(*outmsg, inmsg, inlen);
	*outlen  = inlen;
	*outtype = intype;

/*
	logit(
		"e","scnlfilter: accepting msgtype:%d from %s %s %s %s\n",
		intype, key.sta, key.chan, key.net, key.loc
	);*/   /*DEBUG*/

   return 1;
}

/**********************************************************
 * scnlfilter_logmsg()  simple logging of message       *
 **********************************************************/
void scnlfilter_logmsg( char *msg, int msglen,
                          unsigned char msgtype, char *note )
{
   TRACE2_HEADER *thd2 = (TRACE2_HEADER *)msg;
   char tmpstr[100];
   double tstart;

   if( msgtype == TypeTraceBuf2  ||
       msgtype == TypeTraceComp2    )
   {
      tstart = WaveMsgTime(thd2);
      datestr23( tstart, tmpstr, sizeof(tmpstr) );
      logit("t","%s t%d %s.%s.%s.%s %s\n",
            note, (int)msgtype,
            thd2->sta, thd2->chan, thd2->net, thd2->loc, tmpstr );
   }
   else if( msgtype == TypeTraceBuf  ||
            msgtype == TypeTraceComp    )
   {
      tstart = WaveMsgTime(thd2);
      datestr23( tstart, tmpstr, sizeof(tmpstr) );
      logit("t","%s t%d %s.%s.%s %s\n",
            note, (int)msgtype,
            thd2->sta, thd2->chan, thd2->net, tmpstr );
   }
   else
   {
      int endstr = (int)sizeof(tmpstr)-1;
      if( msglen < endstr ) endstr = msglen;
      strncpy( tmpstr, msg, endstr );
      tmpstr[endstr] = 0;
      logit("t","%s t%d %s\n",
            note, (int)msgtype, tmpstr );
   }
   return;
}


/**********************************************************
 * scnlfilter_end()  frees allocated memory and    *
 *         does any other cleanup stuff                   *
 **********************************************************/
void scnlfilter_end(void)
{
   free( Lists );
   return;
}


/**********************************************************
 * copytrim()  copies n bytes from one string to another, *
 *   trimming off any leading and trailing blank spaces   *
 **********************************************************/
static char *copytrim( char *str2, char *str1, int n )
{
   int i, len;

 /*printf( "copy %3d bytes of str1: \"%s\"\n", n, str1 );*/ /*DEBUG*/

/* find first non-blank char in str1 (trim off leading blanks)
 *************************************************************/
   for( i=0; i<n; i++ ) if( str1[i] != ' ' ) break;

/* copy the remaining number of bytes to str2
 ********************************************/
   len = n-i;
   strncpy( str2, str1+i, len );
   str2[len] = '\0';
 /*printf( "  leading-trimmed str2: \"%s\"\n", str2 );*/ /*DEBUG*/

/* find last non-blank char in str2 (trim off trailing blanks)
 *************************************************************/
   for( i=len-1; i>=0; i-- ) if( str2[i] != ' ' ) break;
   str2[i+1] = '\0';
 /*printf( " trailing-trimmed str2: \"%s\"\n\n", str2 );*/ /*DEBUG*/

   return( str2 );
}

/********************************************************************
*  WaveMsgTime  Return value of starttime from a tracebuf header    *
*               Returns -1. if unknown data type,                   *
*********************************************************************/

double WaveMsgTime( TRACE2_HEADER* wvmsg )
{
   char   byteOrder;
   char   dataSize;
   double packettime;  /* time from trace_buf header */

/* See what sort of data it carries
 **********************************/
   byteOrder = wvmsg->datatype[0];
   dataSize  = wvmsg->datatype[1];

/* Return now if we don't know this message type
 ***********************************************/
   if( byteOrder != 'i'  &&  byteOrder != 's' ) return(-1.);
   if( dataSize  != '2'  &&  dataSize  != '4' ) return(-1.);

   packettime = wvmsg->starttime;
#if defined( _SPARC )
   if( byteOrder == 'i' ) SwapDouble( &packettime );

#elif defined( _INTEL )
   if( byteOrder == 's' ) SwapDouble( &packettime );

#else
   logit("", "WaveMsgTime warning: _INTEL and _SPARC are both undefined." );
   return( -1. );
#endif

   return packettime;
}

/*
 *
 */
static _SCNL *realloc_scnl_list( const char *prog )
{
	size_t _size;

/* */
	Max_SCNL += INCREMENT_SCNL;
	_size     = Max_SCNL * sizeof(_SCNL);
/* */
	if ( (Lists = (_SCNL *)realloc(Lists, _size)) == NULL ) {
		logit("e", "%s: Error allocating %d bytes for SCNL list; exiting!\n", prog, _size);
		exit(-1);
	}

	return Lists;
}

/*************************************************************
 *                       compare_scnl()                      *
 *                                                           *
 *  This function is passed to qsort() and bsearch().        *
 *  We use qsort() to sort the station list by SCNL numbers, *
 *  and we use bsearch to look up an SCNL in the list if no  *
 *  wildcards were used in the requested channel list        *
 *************************************************************/

int compare_scnl( const void *s1, const void *s2 )
{
   int rc;
   _SCNL *t1 = (_SCNL *) s1;
   _SCNL *t2 = (_SCNL *) s2;

   rc = strcmp( t1->sta, t2->sta );
   if ( rc != 0 ) return rc;

   rc = strcmp( t1->chan, t2->chan );
   if ( rc != 0 ) return rc;

   rc = strcmp( t1->net,  t2->net );
   if ( rc != 0 ) return rc;

   rc = strcmp( t1->loc,  t2->loc );
   return rc;
}
