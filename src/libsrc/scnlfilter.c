/**
 * @file scnlfilter.c
 * @author Origin from Lynn Dietz, 1998-11-12
 * @author Benjamin Ming Yang @ Department of Geoscience, National Taiwan University (b98204032@gmail.com)
 * @brief Source file that contains a series of function to filter messages based on content of site-component-network-loc fields.
 * @date 2022-03-28
 *
 * @copyright Copyright (c) 2022-now
 *
 */
/**
 * @name Standard C header include
 *
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/**
 * @name Earthworm environment header include
 *
 */
#include <earthworm.h>
#include <kom.h>
#include <trace_buf.h>

/**
 * @name
 *
 */
#define INCREMENT_SCNL  8                    /* increment the limit of # scnl's    */
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
#define WILDCARD_STR     "*"      /* wildcard string for SCNL */

/**
 * @brief
 *
 */
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

	void *extra;
} SCNL_Filter;

/**
 * @name Functions prototype in this source file
 *
 */
static int          filter_wildcards( const SCNL_Filter *, const SCNL_Filter * );
static char        *copytrim( char *, const char *, const int );
static SCNL_Filter *realloc_scnlf_list( const char * );
static int          compare_scnl( const void *, const void * );  /* qsort & bsearch */

/**
 * @name Globals for site-comp-net-loc filter
 *
 */
static SCNL_Filter *Lists      = NULL;       /* list of SCNL's to accept & rename   */
static int          FilterInit = 0;     /* initialization flag                    */
static int          Max_SCNL   = 0;     /* working limit on # scnl's to ship      */
static int          nSCNL      = 0;     /* # of scnl's we're configured to ship   */
static int          nWild      = 0;     /* # of Wildcards used in config file     */
/**
 * @name
 *
 */
#define IS_WILD(SCNL)                 (!strcmp((SCNL), WILDCARD_STR))
#define IS_MATCH(SCNL_A, SCNL_B)      (!strcmp((SCNL_A), (SCNL_B)))
#define IS_NOT_MATCH(SCNL_A, SCNL_B)  (strcmp((SCNL_A), (SCNL_B)))

/**
 * @brief processes config-file commands to set up the filter & send-list.
 *        this function may exit the process if it finds serious errors in any commands
 *
 * @param prog
 * @return int
 * @retval 1 if the command was recognized & processed
 * @retval 0 if the command was not recognized
 */
int scnlfilter_com( const char *prog )
{
	char *str;

/* */
	if ( nSCNL >= Max_SCNL )
		realloc_scnlf_list( prog );
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
	Lists[nSCNL].extra  = NULL;

/* Read original SCNL */
	for ( register uint8_t fbit = STA_FILTER_BIT; fbit > 0; fbit >>= 1 ) {
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
				strcpy(Lists[nSCNL].chan, str);
				break;
			case NET_FILTER_BIT:
			/* original network code */
				if ( strlen(str) > (size_t)NET_STRLEN )
					goto except_com;
				strcpy(Lists[nSCNL].net, str);
				break;
			case LOC_FILTER_BIT:
			/* original location code */
				if ( strlen(str) > (size_t)LOC_STRLEN )
					goto except_com;
				strcpy(Lists[nSCNL].loc, str);
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
		for ( register uint8_t fbit = STA_FILTER_BIT; fbit > 0; fbit >>= 1 ) {
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
					strcpy(Lists[nSCNL].rchan, str);
					break;
				case NET_FILTER_BIT:
				/* remap network code */
					if ( strlen(str) > (size_t)NET_STRLEN )
						goto except_com;
					strcpy(Lists[nSCNL].rnet, str);
					break;
				case LOC_FILTER_BIT:
				/* remap location code */
					if ( strlen(str) > (size_t)LOC_STRLEN )
						goto except_com;
					strcpy(Lists[nSCNL].rloc, str);
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

/**
 * @brief
 *
 * @param extra_proc
 * @return int
 */
int scnlfilter_extra_com( void *(*extra_proc)( const char * ) )
{
	const int prev = nSCNL - 1;
	char     *str;

/* */
	if ( (str = k_str()) )
		Lists[prev].extra = extra_proc( str );
/* */
	if ( !str || !Lists[prev].extra )
		return -1;

	return 1;
}

/**
 * @brief Make sure all the required commands were found in the config file,
 *        do any other startup things necessary for filter to work properly
 *
 * @param prog
 * @return int
 */
int scnlfilter_init( const char *prog )
{
/* Check to see if required config-file commands were processed */
	if ( nSCNL == 0 ) {
		logit(
			"e", "%s: No any <Allow_SCNL> or <Allow_SCNL_Remap> commands in config file;"
			" no data will be processed!\n", prog
		);
		return -1;
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
			" will use linear search for SCNL matching.\n", prog
		);
	}

	logit("o", "%s: configured to handle %d channels:", prog, nSCNL);
	for ( register int i = 0; i < nSCNL; i++ ) {
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
/* */
	FilterInit = 1;

	return 0;
}

/**
 * @brief Looks at the candidate message.
 *
 * @param sta
 * @param chan
 * @param net
 * @param loc
 * @param outmatch
 * @return int
 */
int scnlfilter_apply( const char *sta, const char *chan, const char *net, const char *loc, const void **outmatch )
{
	register int i;
	SCNL_Filter  key;          /* Key for binary search        */
	SCNL_Filter *match;        /* Pointer into Lists-list      */
	SCNL_Filter *current;
/* */
	if ( !FilterInit )
		scnlfilter_init( "scnlfilter_apply" );
/* Read SCNL from input arguments */
	copytrim( key.sta,  sta ? sta : WILDCARD_STR,    STA_STRLEN  );
	copytrim( key.chan, chan ? chan : WILDCARD_STR,  CHAN_STRLEN );
	copytrim( key.net,  net ? net : WILDCARD_STR,    NET_STRLEN  );
	copytrim( key.loc,  loc ? loc : LOC_NULL_STRING, LOC_STRLEN  );

/* Look for the message's SCNL in the Lists-list. */
	match = NULL;
	if ( nWild ) {
	/* Gotta do it the linear way if wildcards were used! */
		for ( i = 0, current = Lists; i < nSCNL; i++, current++ ) {
			if ( filter_wildcards( current, &key ) ) {
			/* found a match! */
				if ( !current->block )
					match = current;
				break;
			}
		}
	}
	else {
	/*
	 * Use the more efficient binary search if there
	 * were no wildcards in the original SCNL request list.
	 */
		match = (SCNL_Filter *)bsearch( &key, Lists, nSCNL, sizeof(SCNL_Filter), compare_scnl );
		if ( match && match->block )
			match = (SCNL_Filter *)NULL; /* SCNL is marked to be blocked */
	}

	if ( !match ) {
		/* logit(
			"e","scnlfilter: rejecting msgtype:%d from %s %s %s %s\n",
			intype, key.sta, key.chan, key.net, key.loc
		); */
		/* DEBUG */
		return 0;  /* SCNL not in Lists list */
	}

/*
 * Found a message we want to ship!
 * Do some extra checks for trace data only
 */
/* Match pointer output */
	if ( outmatch )
		*outmatch = match;
/*
	logit(
		"e","scnlfilter: accepting msgtype:%d from %s %s %s %s\n",
		intype, key.sta, key.chan, key.net, key.loc
	);
*/
/* DEBUG */

	return 1;
}

/**
 * @brief
 *
 * @param sta
 * @param chan
 * @param net
 * @param loc
 * @param match
 * @return int
 */
int scnlfilter_remap( char *sta, char *chan, char *net, char *loc, const void *match )
{
	const SCNL_Filter *_match = (const SCNL_Filter *)match;

/* */
	if ( _match && _match->remap ) {
		for ( register uint8_t fbit = STA_FILTER_BIT; fbit > 0; fbit >>= 1 ) {
			if ( !(_match->rwilds & fbit) ) {
				switch ( fbit ) {
				case STA_FILTER_BIT:
					if ( sta )
						strncpy(sta, _match->rsta, TRACE2_STA_LEN);
					break;
				case CHAN_FILTER_BIT:
					if ( chan )
						strncpy(chan, _match->rchan, TRACE2_CHAN_LEN);
					break;
				case NET_FILTER_BIT:
					if ( net )
						strncpy(net, _match->rnet, TRACE2_NET_LEN);
					break;
				case LOC_FILTER_BIT:
					if ( loc )
						strncpy(loc, _match->rloc, TRACE2_LOC_LEN);
				default:
					break;
				}
			}
		}
	}

	return 1;
}

/**
 * @brief
 *
 * @param match
 * @return void*
 */
void *scnlfilter_extra_get( const void *match )
{
	const SCNL_Filter *_match = (const SCNL_Filter *)match;

/* Extra argument output */
	if ( _match && _match->extra )
		return _match->extra;
	else
		return NULL;
}

/**
 * @brief Frees allocated memory and does any other cleanup stuff
 *
 * @param free_extra
 */
void scnlfilter_end( void (*free_extra)( void * ) )
{
	register int i;
	SCNL_Filter *current;

/* */
	if ( Lists ) {
		if ( free_extra ) {
			for ( i = 0, current = Lists; i < nSCNL; i++, current++ ) {
				if ( current->extra )
					free_extra( current->extra );
			}
		}
		free(Lists);
	}
	return;
}

/**
 * @brief
 *
 * @param filter
 * @param key
 * @return int
 */
static int filter_wildcards( const SCNL_Filter *filter, const SCNL_Filter *key )
{
/* */
	for ( register uint8_t fbit = STA_FILTER_BIT; fbit > 0; fbit >>= 1 ) {
		if ( !(filter->owilds & fbit) ) {
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

/**
 * @brief Copies n bytes from one string to another, trimming off any leading and trailing blank spaces
 *
 * @param dest
 * @param src
 * @param n
 * @return char*
 */
static char *copytrim( char *dest, const char *src, const int n )
{
	register int i, len;

/* DEBUG */
/* printf( "copy %3d bytes of src: \"%s\"\n", n, src ); */

/* find first non-blank char in src (trim off leading blanks) */
	for ( i = 0; i < n; i++ )
		if ( src[i] != ' ' )
			break;
/* copy the remaining number of bytes to dest */
	len = n - i;
	memcpy(dest, src + i, len);
	dest[len] = '\0';
/* DEBUG */
/* printf( "  leading-trimmed dest: \"%s\"\n", dest ); */

/* find last non-blank char in dest (trim off trailing blanks) */
	for ( i = len - 1; i >= 0; i-- )
		if ( dest[i] != ' ' )
			break;
	dest[i + 1] = '\0';
/* DEBUG */
/* printf( " trailing-trimmed dest: \"%s\"\n\n", dest ); */

	return dest;
}

/**
 * @brief
 *
 * @param prog
 * @return SCNL_Filter*
 */
static SCNL_Filter *realloc_scnlf_list( const char *prog )
{
	size_t _size;

/* */
	Max_SCNL += INCREMENT_SCNL;
	_size     = Max_SCNL * sizeof(SCNL_Filter);
/* */
	if ( (Lists = (SCNL_Filter *)realloc(Lists, _size)) == NULL ) {
		logit("e", "%s: Error allocating %ld bytes for SCNL list; exiting!\n", prog, _size);
		exit(-1);
	}

	return Lists;
}

/**
 * @brief This function is passed to qsort() and bsearch().
 *        We use qsort() to sort the station list by SCNL numbers,
 *        and we use bsearch to look up an SCNL in the list if no
 *        wildcards were used in the requested channel list
 *
 * @param s1
 * @param s2
 * @return int
 */
static int compare_scnl( const void *s1, const void *s2 )
{
	SCNL_Filter *t1 = (SCNL_Filter *)s1;
	SCNL_Filter *t2 = (SCNL_Filter *)s2;
	int          rc;

	if ( (rc = memcmp(t1->sta, t2->sta, TRACE2_STA_LEN)) )
		return rc;
	if ( (rc = memcmp(t1->chan, t2->chan, TRACE2_CHAN_LEN)) )
		return rc;
	if ( (rc = memcmp(t1->net, t2->net, TRACE2_NET_LEN)) )
		return rc;
	return (rc = memcmp(t1->loc, t2->loc, TRACE2_LOC_LEN));
}
