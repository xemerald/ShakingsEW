
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
/* */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* */
#include <earthworm.h>
#include <time_ew.h>
#include <trace_buf.h>
#include <kom.h>
#include <swap.h>

#define INCREMENT_SCNL  10                   /* increment the limit of # scnl's    */
#define STA_STRLEN    (TRACE2_STA_LEN - 1)   /* max string-length of station code  */
#define CHAN_STRLEN   (TRACE2_CHAN_LEN - 1)  /* max string-length of component code*/
#define NET_STRLEN    (TRACE2_NET_LEN - 1)   /* max string-length of network code  */
#define LOC_STRLEN    (TRACE2_LOC_LEN - 1)   /* max string-length of location code */
/* */
#define STA_FILTER_BIT   0x08
#define CHAN_FILTER_BIT  0x04
#define NET_FILTER_BIT   0x02
#define LOC_FILTER_BIT   0x01
/* */
typedef struct {
	char sta[TRACE2_STA_LEN];     /* original station name  */
	char chan[TRACE2_CHAN_LEN];   /* original channel name  */
	char net[TRACE2_NET_LEN];     /* original network name  */
	char loc[TRACE2_LOC_LEN];     /* original location code */

	char rsta[TRACE2_STA_LEN];    /* remapped station name  */
	char rchan[TRACE2_CHAN_LEN];  /* remapped channel name  */
	char rnet[TRACE2_NET_LEN];    /* remapped network name  */
	char rloc[TRACE2_LOC_LEN];    /* remapped location code */

	uint8_t remap;                /* flag: 1=remap SCNL, 0=don't */
	uint8_t block;                /* flag: 1=block SCNL, 0=don't */
	uint8_t owilds;               /* for original wildcard bits  */
	uint8_t rwilds;               /* for remapped wildcard bits  */
} SCNL_Filter;

/* Local function prototypes & macros */
static int          filter_wildcards( const SCNL_Filter *, const SCNL_Filter * );
static int          remap_tracebuf_scnl( const SCNL_Filter *, TRACE_HEADER * );
static int          remap_tracebuf2_scnl( const SCNL_Filter *, TRACE2_HEADER * );
static double       get_wavetime( TRACE2_HEADER * );
static char        *copytrim( char *, char *, int );
static SCNL_Filter *realloc_scnlf_list( const char * );
static int          compare_scnl( const void *s1, const void *s2 );  /* qsort & bsearch */

/* Globals for site-comp-net-loc filter */
static SCNL_Filter *Lists = NULL;       /* list of SCNL's to accept & rename   */
static int    FilterInit = 0;     /* initialization flag                    */
static int    Max_SCNL   = 0;     /* working limit on # scnl's to ship      */
static int    nSCNL      = 0;     /* # of scnl's we're configured to ship   */
static int    nWild      = 0;     /* # of Wildcards used in config file     */
static char  *Wild       = "*";   /* wildcard string for SCNL               */
/* */
static unsigned char TypeTraceBuf;
static unsigned char TypeTraceBuf2;   /* with Loc */
/* */
#define IS_WILD(SCNL)                 (!strcmp((SCNL), Wild))
#define IS_MATCH(SCNL_A, SCNL_B)      (!strcmp((SCNL_A), (SCNL_B)))
#define IS_NOT_MATCH(SCNL_A, SCNL_B)  (strcmp((SCNL_A), (SCNL_B)))

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
	char   *str;
	uint8_t fbit;
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
	Lists[nSCNL].owilds = 0;
	Lists[nSCNL].rwilds = 0;
/* */
	if ( nSCNL >= Max_SCNL )
		realloc_scnlf_list( prog );
/* Read original SCNL */
	for ( fbit = STA_FILTER_BIT; fbit > 0; fbit >>= 1 ) {
		if ( (str = k_str()) ) {
		/* */
			if ( IS_WILD(str) ) {
				nWild++;  /* count SCNL wildcards */
				Lists[nSCNL].owilds |= fbit;
			}
		/* */
			switch ( fbit ) {
			case STA_FILTER_BIT:
			/* original station code */
				if ( strlen(str) > (size_t)STA_STRLEN )
					goto except_com;
				strcpy(Lists[nSCNL].sta, str);
				break;
			case CHAN_FILTER_BIT:
			/* original component code */
				if ( strlen(str) > (size_t)CHAN_STRLEN )
					goto except_com;
				strcpy(Lists[nSCNL].chan,str);
				break;
			case NET_FILTER_BIT:
			/* original network code */
				if ( strlen(str) > (size_t)NET_STRLEN )
					goto except_com;
				strcpy(Lists[nSCNL].net,str);
				break;
			case LOC_FILTER_BIT:
			/* original location code */
				if ( strlen(str) > (size_t)LOC_STRLEN )
					goto except_com;
				strcpy(Lists[nSCNL].loc,str);
			default:
				break;
			}
		}
		else {
			goto except_com;
		}
	}

/* Read remap SCNL  */
	if ( Lists[nSCNL].remap ) {
		for ( fbit = STA_FILTER_BIT; fbit > 0; fbit >>= 1 ) {
			if ( (str = k_str()) ) {
			/* */
				if ( IS_WILD(str) )
					Lists[nSCNL].rwilds |= fbit;
			/* */
				switch ( fbit ) {
				case STA_FILTER_BIT:
				/* remap station code */
					if ( strlen(str) > (size_t)STA_STRLEN )
						goto except_com;
					strcpy(Lists[nSCNL].rsta, str);
					break;
				case CHAN_FILTER_BIT:
				/* remap component code */
					if ( strlen(str) > (size_t)CHAN_STRLEN )
						goto except_com;
					strcpy(Lists[nSCNL].rchan,str);
					break;
				case NET_FILTER_BIT:
				/* remap network code */
					if ( strlen(str) > (size_t)NET_STRLEN )
						goto except_com;
					strcpy(Lists[nSCNL].rnet,str);
					break;
				case LOC_FILTER_BIT:
				/* remap location code */
					if ( strlen(str) > (size_t)LOC_STRLEN )
						goto except_com;
					strcpy(Lists[nSCNL].rloc,str);
				default:
					break;
				}
			}
			else {
				goto except_com;
			}
		}
	}
/* */
	nSCNL++;

	return 1;

except_com:
	logit(
		"e", "%s: Error in SCNL filter command(s) (argument either missing, too long, or invalid): "
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
			"e", "%s: No any <Allow_SCNL> or <Allow_SCNL_Remap> commands in config file;"
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
		qsort(Lists, nSCNL, sizeof(SCNL_Filter), compare_scnl);
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
	int            i;
	TRACE_HEADER  *thd;
	TRACE2_HEADER *thd2;
	SCNL_Filter    key;          /* Key for binary search       */
	SCNL_Filter   *match;        /* Pointer into Lists-list      */
	SCNL_Filter   *current;

/* */
	if ( !FilterInit )
		scnlfilter_init( "scnlfilter_apply" );
/* Read SCNL of a TYPE_TRACEBUF2 or TYPE_TRACE2_COMP_UA message */
	if ( intype == TypeTraceBuf2 ) {
		thd2 = (TRACE2_HEADER *)inmsg;
		copytrim( key.sta,  thd2->sta,  STA_STRLEN  );
		copytrim( key.chan, thd2->chan, CHAN_STRLEN );
		copytrim( key.net,  thd2->net,  NET_STRLEN  );
		copytrim( key.loc,  thd2->loc,  LOC_STRLEN  );
	}
/* Read SCN of a TYPE_TRACEBUF or TYPE_TRACE_COMP_UA message */
	else if ( intype == TypeTraceBuf ) {
		thd = (TRACE_HEADER *)inmsg;
		copytrim( key.sta,  thd->sta,        STA_STRLEN  );
		copytrim( key.chan, thd->chan,       CHAN_STRLEN );
		copytrim( key.net,  thd->net,        NET_STRLEN  );
		copytrim( key.loc,  LOC_NULL_STRING, LOC_STRLEN  );
	}
/* Or else we don't know how to read this type of message */
	else {
		return 0;
	}

/* Look for the message's SCNL in the Lists-list. */
   match = (SCNL_Filter *) NULL;
/*
 * Use the more efficient binary search if there
 * were no wildcards in the original SCNL request list.
 */
	if ( nWild == 0 ) {
		match = (SCNL_Filter *)bsearch( &key, Lists, nSCNL, sizeof(SCNL_Filter), compare_scnl );
		if ( match != NULL && match->block )
			match = (SCNL_Filter *)NULL; /* SCNL is marked to be blocked */
	}
/* Gotta do it the linear way if wildcards were used! */
	else {
		for ( i = 0, current = Lists; i < nSCNL; i++, current++ ) {
			if ( filter_wildcards( current, &key ) ) {
			/* found a match! */
				if ( !current->block )
					match = current;
				break;
			}
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
	if ( match->remap ) {
	/* Rename its SCNL if appropriate */
		if ( intype == TypeTraceBuf2 )
			remap_tracebuf2_scnl( match, thd2 );
		else if ( intype == TypeTraceBuf )
			remap_tracebuf_scnl( match, thd );
	}

/* Copy message to output buffer */
	if ( outmsg && *outmsg ) {
		memcpy(*outmsg, inmsg, inlen);
		*outlen  = inlen;
		*outtype = intype;
	}
/*
	logit(
		"e","scnlfilter: accepting msgtype:%d from %s %s %s %s\n",
		intype, key.sta, key.chan, key.net, key.loc
	);
*/  /* DEBUG */

   return 1;
}

/*
 * scnlfilter_logmsg() - simple logging of message
 */
void scnlfilter_logmsg( char *msg, int msglen, unsigned char msgtype, char *note )
{
	TRACE2_HEADER *thd2 = (TRACE2_HEADER *)msg;
	char           tmpstr[128];
	double         tstart;
	int            endstr = (int)sizeof(tmpstr) - 1;

	if ( msgtype == TypeTraceBuf2 ) {
		tstart = get_wavetime( thd2 );
		datestr23( tstart, tmpstr, sizeof(tmpstr) );
		logit(
			"t", "%s t%d %s.%s.%s.%s %s\n",
			note, (int)msgtype, thd2->sta, thd2->chan, thd2->net, thd2->loc, tmpstr
		);
	}
	else if ( msgtype == TypeTraceBuf ) {
		tstart = get_wavetime(thd2);
		datestr23( tstart, tmpstr, sizeof(tmpstr) );
		logit(
			"t", "%s t%d %s.%s.%s %s\n",
			note, (int)msgtype, thd2->sta, thd2->chan, thd2->net, tmpstr
		);
	}
	else {
		if ( msglen < endstr )
			endstr = msglen;
		memcpy(tmpstr, msg, endstr);
		tmpstr[endstr] = 0;
		logit("t", "%s t%d %s\n", note, (int)msgtype, tmpstr);
	}

	return;
}


/*
 * scnlfilter_end() - frees allocated memory and
 *                    does any other cleanup stuff
 */
void scnlfilter_end( void )
{
	if ( Lists )
		free( Lists );
	return;
}

/*
 *
 */
static int filter_wildcards( const SCNL_Filter *filter, const SCNL_Filter *key )
{
	uint8_t fbit;

/* */
	for ( fbit = STA_FILTER_BIT; fbit > 0; fbit >>= 1 ) {
		if ( filter->owilds & fbit ) {
			continue;
		}
		else {
			switch ( fbit ) {
			case STA_FILTER_BIT:
				if ( IS_NOT_MATCH( filter->sta, key->sta ) )
					return 0;
				break;
			case CHAN_FILTER_BIT:
				if ( IS_NOT_MATCH( filter->chan, key->chan ) )
					return 0;
				break;
			case NET_FILTER_BIT:
				if ( IS_NOT_MATCH( filter->net, key->net ) )
					return 0;
				break;
			case LOC_FILTER_BIT:
				if ( IS_NOT_MATCH( filter->loc, key->loc ) )
					return 0;
				break;
			default:
				return 0;
			}
		}
	}

	return 1;
}

/*
 *
 */
static int remap_tracebuf_scnl( const SCNL_Filter *filter, TRACE_HEADER *trh )
{
	uint8_t fbit;

/* */
	for ( fbit = STA_FILTER_BIT; fbit > 0; fbit >>= 1 ) {
		if ( filter->rwilds & fbit ) {
			continue;
		}
		else {
			switch ( fbit ) {
			case STA_FILTER_BIT:
				memcpy(trh->sta, filter->rsta, TRACE2_STA_LEN);
				break;
			case CHAN_FILTER_BIT:
				memcpy(trh->chan, filter->rchan, TRACE2_CHAN_LEN);
				break;
			case NET_FILTER_BIT:
				memcpy(trh->net, filter->rnet, TRACE2_NET_LEN);
				break;
			case LOC_FILTER_BIT:
			default:
				break;
			}
		}
	}

	return 1;
}

/*
 *
 */
static int remap_tracebuf2_scnl( const SCNL_Filter *filter, TRACE2_HEADER *trh2 )
{
	uint8_t fbit;

/* */
	for ( fbit = STA_FILTER_BIT; fbit > 0; fbit >>= 1 ) {
		if ( filter->rwilds & fbit ) {
			continue;
		}
		else {
			switch ( fbit ) {
			case STA_FILTER_BIT:
				memcpy(trh2->sta, filter->rsta, TRACE2_STA_LEN);
				break;
			case CHAN_FILTER_BIT:
				memcpy(trh2->chan, filter->rchan, TRACE2_CHAN_LEN);
				break;
			case NET_FILTER_BIT:
				memcpy(trh2->net, filter->rnet, TRACE2_NET_LEN);
				break;
			case LOC_FILTER_BIT:
				memcpy(trh2->loc, filter->rloc, TRACE2_LOC_LEN);
			default:
				break;
			}
		}
	}

	return 1;
}

/*
 * copytrim() - copies n bytes from one string to another,
 *              trimming off any leading and trailing blank spaces
 */
static char *copytrim( char *str2, char *str1, int n )
{
	int i, len;

/* DEBUG */
/* printf( "copy %3d bytes of str1: \"%s\"\n", n, str1 ); */

/* find first non-blank char in str1 (trim off leading blanks) */
	for ( i = 0; i < n; i++ )
		if ( str1[i] != ' ' )
			break;
/* copy the remaining number of bytes to str2 */
	len = n - i;
	memcpy( str2, str1 + i, len );
	str2[len] = '\0';
/* DEBUG */
/* printf( "  leading-trimmed str2: \"%s\"\n", str2 ); */

/* find last non-blank char in str2 (trim off trailing blanks) */
	for ( i = len - 1; i >= 0; i-- )
		if ( str2[i] != ' ' )
			break;
	str2[i + 1] = '\0';
/* DEBUG */
/* printf( " trailing-trimmed str2: \"%s\"\n\n", str2 ); */

	return str2;
}

/*
 *  get_wavetime() - Return value of starttime from a tracebuf header
 *                   Returns -1. if unknown data type,
 */
static double get_wavetime( TRACE2_HEADER* wvmsg )
{
	char   byte_order;
	char   data_size;
	double packettime;  /* time from trace_buf header */

/* See what sort of data it carries */
	byte_order = wvmsg->datatype[0];
	data_size  = wvmsg->datatype[1];

/* Return now if we don't know this message type */
	if ( byte_order != 'i' && byte_order != 's' )
		return -1.;
	if ( data_size != '2' && data_size  != '4' )
		return -1.;
/* */
	packettime = wvmsg->starttime;
#if defined( _SPARC )
	if ( byte_order == 'i' )
		SwapDouble( &packettime );
#elif defined( _INTEL )
	if ( byte_order == 's' )
		SwapDouble( &packettime );
#else
   logit("", "get_wavetime WARNING! _INTEL and _SPARC are both undefined.");
   return -1.;
#endif

   return packettime;
}

/*
 *
 */
static SCNL_Filter *realloc_scnlf_list( const char *prog )
{
	size_t _size;

/* */
	Max_SCNL += INCREMENT_SCNL;
	_size     = Max_SCNL * sizeof(SCNL_Filter);
/* */
	if ( (Lists = (SCNL_Filter *)realloc(Lists, _size)) == NULL ) {
		logit("e", "%s: Error allocating %d bytes for SCNL list; exiting!\n", prog, _size);
		exit(-1);
	}

	return Lists;
}

/*
 * compare_scnl() -
 *
 *  This function is passed to qsort() and bsearch().
 *  We use qsort() to sort the station list by SCNL numbers,
 *  and we use bsearch to look up an SCNL in the list if no
 *  wildcards were used in the requested channel list
 */
static int compare_scnl( const void *s1, const void *s2 )
{
   int rc;
   SCNL_Filter *t1 = (SCNL_Filter *) s1;
   SCNL_Filter *t2 = (SCNL_Filter *) s2;

   rc = strcmp( t1->sta, t2->sta );
   if ( rc != 0 ) return rc;

   rc = strcmp( t1->chan, t2->chan );
   if ( rc != 0 ) return rc;

   rc = strcmp( t1->net,  t2->net );
   if ( rc != 0 ) return rc;

   rc = strcmp( t1->loc,  t2->loc );
   return rc;
}
