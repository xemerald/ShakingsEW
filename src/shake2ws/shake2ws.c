#ifdef _OS2
#define INCL_DOSMEMMGR
#define INCL_DOSSEMAPHORES
#include <os2.h>
#endif
/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <libwebsockets.h>
/* Earthworm environment header include */
#include <earthworm.h>
#include <kom.h>
#include <transport.h>
#include <lockfile.h>
/* Local header include */
#include <dl_chain_list.h>
#include <recordtype.h>
#include <scnlfilter.h>
#include <shakeint.h>
#include <tracepeak.h>
#include <shake2ws.h>
#include <shake2ws_list.h>
#include <shake2ws_protocols.h>

/* Functions prototype in this source file */
static void shake2ws_config( char * );
static void shake2ws_lookup( void );
static void shake2ws_status( unsigned char, short, char * );
static void shake2ws_end( void );                /* Free all the local memory & close socket */
static thr_ret shake2ws_thread_lwsservice( void * );
/* */
static int    is_single_pvalue_sync( const STATION_PEAK *, const int );
static int    is_needed_pvalues_sync( const STATION_PEAK *, const int );
static void   update_related_intensities( STATION_PEAK *, const int );
static double update_single_pvalue( STATION_PEAK *, const int );
static void   check_station_latency( void *, const int, void * );
static void  *proc_com_sv_index( const char * );
static int    proc_com_input_pv( int, const int );

/* Ring messages things */
static SHM_INFO Region;      /* shared memory region to use for i/o    */
/* */
#define MAXLOGO 5
static MSG_LOGO Getlogo[MAXLOGO];       /* array for requesting module, type, instid */
static pid_t    MyPid;                  /* for restarts by startstop                 */

/* Thread things */
#define THREAD_STACK  8388608       /* 8388608 Byte = 8192 Kilobyte = 8 Megabyte */
#define THREAD_OFF    0				/* ComputePGA has not been started      */
#define THREAD_ALIVE  1				/* ComputePGA alive and well            */
#define THREAD_ERR   -1				/* ComputePGA encountered error quit    */
volatile int   LWSServiceStatus = THREAD_OFF;
volatile _Bool Finish = 0;

/* Things to read or derive from configuration file */
static char     RingName[MAX_RING_STR];     /* name of transport ring for i/o    */
static char     MyModName[MAX_MOD_STR];     /* speak as this module name/id      */
static uint8_t  LogSwitch;                  /* 0 if no logfile should be written */
static uint64_t HeartBeatInterval;          /* seconds between heartbeats        */
static int32_t  WSPort = DEFAULT_WS_PORT;   /* Port number of WS server          */
static uint16_t nLogo  = 0;
uint16_t        nPeakValue = 0;
uint16_t        nIntensity = 0;

/* Things to look up in the earthworm.h tables with getutil.c functions */
static int64_t RingKey;         /* key of transport ring for i/o     */
static uint8_t InstId;          /* local installation id             */
static uint8_t MyModId;         /* Module Id for this program        */
static uint8_t TypeHeartBeat;
static uint8_t TypeError;
static uint8_t TypeTracePeak = 0;

/* Error messages used by shake2ws */
#define  ERR_MISSMSG       0   /* message missed in transport ring       */
#define  ERR_TOOBIG        1   /* retreived msg too large for buffer     */
#define  ERR_NOTRACK       2   /* msg retreived; tracking limit exceeded */
#define  ERR_QUEUE         3   /* error queueing message for sending      */
static char Text[150];         /* string for log/error messages          */

/* */
static struct {
	uint16_t rectype;
	uint8_t  nrelated;
	uint8_t  related_int[MAX_TYPE_INTENSITY];
	uint8_t  inttype;
} SetPeakValue[MAX_TYPE_PEAKVALUE];
/* */
static struct {
	uint8_t  inttype;
	uint8_t  npvalue;
	uint8_t  pvindex[MAX_TYPE_PEAKVALUE];
} GenIntensity[MAX_TYPE_INTENSITY];
/* List of supported protocols and callbacks */
static struct lws_protocols Protocols[] = {
	SHAKE2WS_LWS_PROTOCOL_HTTP_DUMMY,
	SHAKE2WS_LWS_PROTOCOL_MAP_SHAKE,
	SHAKE2WS_LWS_PROTOCOL_STATION_SHAKE,
	SHAKE2WS_LWS_PROTOCOL_STATION_STATUS,
	{ NULL, NULL, 0, 0, 0, NULL, 0 } /* terminator */
};

/**
 * @brief
 *
 * @param argc
 * @param argv
 * @return int
 */
int main ( int argc, char **argv )
{
	int      res;
	MSG_LOGO reclogo;
	int64_t  recsize = 0;
	int32_t  lockfile_fd;
	char    *lockfile;
	time_t   timenow;           /* current time                  */
	time_t   timelastbeat;      /* time last heartbeat was sent  */
	time_t   timelastcheck;     /* */

	TracePeakPacket buffer;
	STATION_PEAK   *stapeak;
	CHAN_PEAK      *chapeak;
	const void     *_match = NULL;
	const void     *_extra = NULL;
/* */
#if defined( _V710 )
	ew_thread_t   tid;            /* Thread ID */
#else
	unsigned      tid;            /* Thread ID */
#endif

/* Check command line arguments */
	if ( argc != 2 ) {
		fprintf(stderr, "Usage: shake2ws <configfile>\n");
		exit(0);
	}
/* Initialize name of log-file & open it */
	logit_init(argv[1], 0, 256, 1);
/* Read the configuration file(s) */
	shake2ws_config( argv[1] );
	logit("" , "%s: Read command file <%s>\n", argv[0], argv[1]);
/* Look up important info from earthworm.h tables */
	shake2ws_lookup();
/* Reinitialize logit to desired logging level */
	logit_init(argv[1], 0, 256, LogSwitch);
	lockfile = ew_lockfile_path(argv[1]);
	if ( (lockfile_fd = ew_lockfile(lockfile) ) == -1 ) {
		fprintf(stderr, "one instance of %s is already running, exiting\n", argv[0]);
		exit (-1);
	}
/* */
	scnlfilter_init( "shake2ws" );
/* */
	Finish = 1;
/* Get process ID for heartbeat messages */
	MyPid = getpid();
	if ( MyPid == -1 ) {
		logit("e","shake2ws: Cannot get pid. Exiting.\n");
		exit (-1);
	}
/* Attach to Input/Output shared memory ring */
	tport_attach(&Region, RingKey);
	logit("", "shake2ws: Attached to public memory region %s: %ld\n", RingName, RingKey);
/* Flush the transport ring */
	tport_flush(&Region, Getlogo, nLogo, &reclogo);
/* */
	sk2ws_list_init();
/* Force a heartbeat to be issued in first pass thru main loop */
	timelastbeat  = time(&timenow) - HeartBeatInterval - 1;
	timelastcheck = timenow + LATENCY_THRESHOLD;
/*----------------------- setup done; start main loop -------------------------*/
	while ( 1 ) {
	/* Send shake2ws's heartbeat */
		if ( (time(&timenow) - timelastbeat) >= (int64_t)HeartBeatInterval ) {
			timelastbeat = timenow;
			shake2ws_status( TypeHeartBeat, 0, "" );
		}
	/* */
		if ( (timenow - timelastcheck) >= 1 ) {
			timelastcheck = timenow;
			sk2ws_list_walk( check_station_latency, &timelastcheck );
		}
	/* */
		if ( LWSServiceStatus != THREAD_ALIVE ) {
			if ( StartThread( shake2ws_thread_lwsservice, (unsigned)THREAD_STACK, &tid ) == -1 ) {
				logit("e", "shake2ws: Error starting thread(output_pvalue); exiting!\n");
				shake2ws_end();
				exit(-1);
			}
			LWSServiceStatus = THREAD_ALIVE;
		}
	/* Process all new messages */
		do {
		/* See if a termination has been requested */
			res = tport_getflag( &Region );
			if ( res == TERMINATE || res == MyPid ) {
			/* write a termination msg to log file */
				logit("t", "shake2ws: Termination requested; exiting!\n");
				fflush(stdout);
			/* */
				goto exit_procedure;
			}

		/* Get msg & check the return code from transport */
			res = tport_getmsg(&Region, Getlogo, nLogo, &reclogo, &recsize, (char *)buffer.msg, TRACE_PEAKVALUE_SIZE);
		/* no more new messages */
			if ( res == GET_NONE ) {
				break;
			}
		/* next message was too big */
			else if ( res == GET_TOOBIG ) {
			/* complain and try again   */
				sprintf(
					Text, "Retrieved msg[%ld] (i%u m%u t%u) too big for Buffer[%ld]",
					recsize, reclogo.instid, reclogo.mod, reclogo.type, sizeof(buffer) - 1
				);
				shake2ws_status( TypeError, ERR_TOOBIG, Text );
				continue;
			}
		/* got a msg, but missed some */
			else if ( res == GET_MISS ) {
				sprintf(
					Text, "Missed msg(s)  i%u m%u t%u  %s.", reclogo.instid, reclogo.mod, reclogo.type, RingName
				);
				shake2ws_status( TypeError, ERR_MISSMSG, Text );
			}
		/* got a msg, but can't tell */
			else if ( res == GET_NOTRACK ) {
			/* if any were missed */
				sprintf(
					Text, "Msg received (i%u m%u t%u); transport.h NTRACK_GET exceeded",
					reclogo.instid, reclogo.mod, reclogo.type
				);
				shake2ws_status( TypeError, ERR_NOTRACK, Text );
			}

		/* Process the message */
			if ( reclogo.type == TypeTracePeak ) {
			/* Initialize for in-list checking */
				stapeak = NULL;
				chapeak = NULL;
			/* If this trace is already inside the local list, it would skip the SCNL filter */
				if (
					!((stapeak = sk2ws_list_find( buffer.tpv.sta, buffer.tpv.net, buffer.tpv.loc )) &&
					(chapeak = sk2ws_list_chlist_find( stapeak, buffer.tpv.chan ))) &&
					!scnlfilter_apply( buffer.tpv.sta, buffer.tpv.chan, buffer.tpv.net, buffer.tpv.loc, &_match )
				) {
				#ifdef _SEW_DEBUG
					printf("shake2ws: Found SCNL %s.%s.%s.%s but not in the filter, drop it!\n",
					buffer.tpv.sta, buffer.tpv.chan, buffer.tpv.net, buffer.tpv.loc);
				#endif
					continue;
				}
			/* If this is the first time recv. this station, try to insert into the binary tree */
				if ( !stapeak && !(stapeak = sk2ws_list_search( buffer.tpv.sta, buffer.tpv.net, buffer.tpv.loc )) ) {
				/* Error when insert into the tree */
					logit(
						"e", "shake2ws: %s.%s.%s insert into station tree error, drop this trace.\n",
						buffer.tpv.sta, buffer.tpv.net, buffer.tpv.loc
					);
					continue;
				}
			/* And then the same as the channel */
				if ( !chapeak ) {
					_extra = scnlfilter_extra_get( _match );
					if ( !(chapeak = sk2ws_list_chlist_search( stapeak, buffer.tpv.chan, *(int *)_extra )) ) {
					/* Error when insert into the tree */
						logit(
							"e", "shake2ws: %s.%s.%s.%s insert into channel list error, drop this trace.\n",
							buffer.tpv.sta, buffer.tpv.chan, buffer.tpv.net, buffer.tpv.loc
						);
						continue;
					}
					if ( scnlfilter_remap( buffer.tpv.sta, buffer.tpv.chan, buffer.tpv.net, buffer.tpv.loc, _match ) ) {
					/* First time remap the trace's SCNL */
						printf(
							"shake2ws: Remap received trace SCNL %s.%s.%s.%s to %s.%s.%s.%s!\n",
							stapeak->sta, chapeak->chan, stapeak->net, stapeak->loc,
							buffer.tpv.sta, buffer.tpv.chan, buffer.tpv.net, buffer.tpv.loc
						);
					}
					chapeak->match = _match;
				}
				else {
					scnlfilter_remap( buffer.tpv.sta, buffer.tpv.chan, buffer.tpv.net, buffer.tpv.loc, chapeak->match );
				/* Fetch the extra argument */
					_extra = scnlfilter_extra_get( chapeak->match );
				}

			/* Check if the peak value is newer than the record */
				if ( buffer.tpv.peaktime > chapeak->ptime ) {
					chapeak->ptime  = buffer.tpv.peaktime;
					chapeak->pvalue = fabs(buffer.tpv.peakvalue);
				}
			/* Fetching the peak value index from SCNL filter extra argument */
				res = *(int *)_extra;
			/* Checking for synchronization of the single type of peak value... */
				if ( is_single_pvalue_sync( stapeak, res ) ) {
				/* If sync. then check all of the peak values... */
					update_single_pvalue( stapeak, res );
					update_related_intensities( stapeak, res );
				}
			}
		} while( 1 );  /* end of message-processing-loop */
	/* No more messages; wait for new ones to arrive */
		sleep_ew(10);
	}
/*-----------------------------end of main loop-------------------------------*/
exit_procedure:
/* detach from shared memory */
	Finish = 0;
	shake2ws_end();
	ew_unlockfile(lockfile_fd);
	ew_unlink_lockfile(lockfile);

	return 0;
}

/*
 * shake2ws_config() - Processes command file(s) using kom.c functions;
 *                     exits if any errors are encountered.
 */
static void shake2ws_config( char *configfile )
{
	char  init[8];     /* init flags, one byte for each required command */
	char *com;
	char *str;

	int ncommand;     /* # of required commands you expect to process   */
	int nmiss;        /* number of required commands that were missed   */
	int nfiles;
	int success;
	int i;

/* Set to zero one init flag for each required command */
	ncommand = 8;
	for ( i = 0; i < ncommand; i++ )
		init[i] = 0;

/* Open the main configuration file */
	nfiles = k_open( configfile );
	if ( nfiles == 0 ) {
		logit("e", "shake2ws: Error opening command file <%s>; exiting!\n", configfile);
		exit(-1);
	}

/* Process all command files. While there are command files open */
	while ( nfiles > 0 ) {
	/* Read next line from active file  */
		while ( k_rd() ) {
			com = k_str();         /* Get the first token from line */
		/* Ignore blank lines & comments */
			if ( !com )
				continue;
			if ( com[0] == '#' )
				continue;
		/* Open a nested configuration file */
			if ( com[0] == '@' ) {
				success = nfiles + 1;
				nfiles  = k_open(&com[1]);
				if ( nfiles != success ) {
					logit("e", "shake2ws: Error opening command file <%s>; exiting!\n", &com[1]);
					exit(-1);
				}
				continue;
			}

		/* Process anything else as a command */
		/* 0 */
		 	if ( k_its("LogFile") ) {
				LogSwitch = k_int();
				init[0] = 1;
			}
		/* 1 */
			else if ( k_its("MyModuleId") ) {
				str = k_str();
				if ( str )
					strcpy(MyModName, str);
				init[1] = 1;
			}
		/* 2 */
			else if ( k_its("InputRing") ) {
				str = k_str();
				if ( str )
					strcpy(RingName, str);
				init[2] = 1;
			}
		/* 3 */
			else if ( k_its("HeartBeatInterval") ) {
				HeartBeatInterval = k_long();
				init[3] = 1;
			}
			else if ( k_its("WSPort") ) {
				WSPort = k_int();
				logit(
					"o", "shake2ws: Changed the port of Websocket to %d. (default is %d)\n",
					WSPort, DEFAULT_WS_PORT
				);
			}
		/* Enter installation & module to get event messages from */
		/* 4 */
			else if ( k_its("GetEventsFrom") ) {
				if ( (nLogo + 1) >= MAXLOGO ) {
					logit("e", "shake2ws: Too many <GetEventsFrom> commands in <%s>", configfile);
					logit("e", "; max=%d; exiting!\n", (int)MAXLOGO);
					exit(-1);
				}
				if ( (str = k_str()) ) {
					if ( GetInst(str, &Getlogo[nLogo].instid) != 0 ) {
						logit("e", "shake2ws: Invalid installation name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				if ( (str = k_str()) ) {
					if ( GetModId(str, &Getlogo[nLogo].mod) != 0 ) {
						logit("e", "shake2ws: Invalid module name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				if ( (str = k_str()) ) {
					if ( GetType(str, &Getlogo[nLogo].type) != 0 ) {
						logit("e", "shake2ws: Invalid message type name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				nLogo++;
				init[4] = 1;
			}
		/* 5 */
			else if ( k_its("SetPeakValueType") ) {
				if ( (nPeakValue + 1) >= MAX_TYPE_PEAKVALUE ) {
					logit("e", "shake2ws: Too many <SetPeakValueType> commands in <%s>", configfile);
					logit("e", "; max=%d; exiting!\n", (int)MAX_TYPE_PEAKVALUE);
					exit(-1);
				}
			/* Parse the index of SetPeakValue first */
				i = k_int();
				if ( i >= 0 && i < MAX_TYPE_PEAKVALUE ) {
					if ( (str = k_str()) ) {
						SetPeakValue[i].rectype = typestr2num( str );
						logit(
							"o", "shake2ws: No.%d Peak value type set to %s:%d ",
							i, str, SetPeakValue[i].rectype
						);
					}
					if ( (str = k_str()) ) {
						SetPeakValue[i].inttype = shakestr2num( str );
						if ( SetPeakValue[i].inttype == SHAKE_TYPE_COUNT ) {
							logit("e", "shake2ws: Unknown type of intensity, exiting!\n");
							exit(-1);
						}
						logit(
							"o", "shake2ws: No.%d Peak value intensity type set to %s:%d!\n",
							i, str, SetPeakValue[i].inttype
						);
					}
				/* */
					SetPeakValue[i].nrelated = 0;
					nPeakValue++;
					init[5] = 1;
				}
				else {
					logit("e", "shake2ws: Error when parsing index of <SetPeakValueType> ");
					logit("e", "; setting number might too large, max=%d; exiting!\n", (int)MAX_TYPE_PEAKVALUE);
					exit(-1);
				}
			}
		/* 6 */
			else if ( k_its("GenIntensityType") ) {
				if ( (nIntensity + 1) >= MAX_TYPE_INTENSITY ) {
					logit("e", "shake2ws: Too many <GenIntensityType> commands in <%s>", configfile);
					logit("e", "; max=%d; exiting!\n", (int)MAX_TYPE_INTENSITY);
					exit(-1);
				}
				if ( (str = k_str()) ) {
					GenIntensity[nIntensity].inttype = shakestr2num( str );
					if ( GenIntensity[nIntensity].inttype == SHAKE_TYPE_COUNT ) {
						logit("e", "shake2ws: Unknown type of intensity, exiting!\n");
						exit(-1);
					}
					logit(
						"o", "shake2ws: No.%d intensity type set to %s:%d!\n",
						nIntensity, str, GenIntensity[nIntensity].inttype
					);
				}
			/* Processing of input message list */
				if ( (i = proc_com_input_pv( k_int(), nIntensity )) < 0 ) {
					logit("e", "shake2ws: ERROR when parsing input peak value of <%s> command; exiting!\n", com);
					exit(-1);
				}
				GenIntensity[nIntensity].npvalue = i;
				nIntensity++;
				init[6] = 1;
			}
		/* 7 */
			else if ( scnlfilter_com( "shake2ws" ) ) {
			/* */
				for ( str = k_com(), i = 0; *str == ' ' && i < (int)strlen(str); str++, i++ );
				if ( strncmp(str, "Block_SCNL", 10) ) {
				/* Maybe we need much more checking for this command */
					if ( scnlfilter_extra_com( proc_com_sv_index ) < 0 ) {
						logit("o", "shake2ws: No set value index define in command: \"%s\" ", k_com());
						logit("o", ", first index(0) will be filled!\n");
					/* Reset the error code for this command */
						k_err();
					}
					init[7] = 1;
				}
			}
		 /* Unknown command*/
			else {
				logit("e", "shake2ws: <%s> Unknown command in <%s>.\n", com, configfile);
				continue;
			}

		/* See if there were any errors processing the command */
			if ( k_err() ) {
			   logit("e", "shake2ws: Bad <%s> command in <%s>; exiting!\n", com, configfile);
			   exit(-1);
			}
		}
		nfiles = k_close();
	}

/* After all files are closed, check init flags for missed commands */
	nmiss = 0;
	for ( i = 0; i < ncommand; i++ )
		if ( !init[i] )
			nmiss++;
/* */
	if ( nmiss ) {
		logit("e", "shake2ws: ERROR, no ");
		if ( !init[0] ) logit("e", "<LogFile> "              );
		if ( !init[1] ) logit("e", "<MyModuleId> "           );
		if ( !init[2] ) logit("e", "<InputRing> "            );
		if ( !init[3] ) logit("e", "<HeartBeatInterval> "    );
		if ( !init[4] ) logit("e", "any <GetEventsFrom> "    );
		if ( !init[5] ) logit("e", "any <GetPeakValueType> " );
		if ( !init[6] ) logit("e", "any <GenIntensityType> " );
		if ( !init[7] ) logit("e", "any <Allow_SCNL*> "      );

		logit("e", "command(s) in <%s>; exiting!\n", configfile);
		exit(-1);
	}

	return;
}

/*
 * shake2ws_lookup() - Look up important info from earthworm.h tables
 */
static void shake2ws_lookup( void )
{
/* Look up keys to shared memory regions */
	if ( (RingKey = GetKey(RingName)) == -1 ) {
		fprintf(stderr, "shake2ws: Invalid ring name <%s>; exiting!\n", RingName);
		exit(-1);
	}
/* Look up installations of interest */
	if ( GetLocalInst(&InstId) != 0 ) {
		fprintf(stderr, "shake2ws: Error getting local installation id; exiting!\n");
		exit(-1);
	}
/* Look up modules of interest */
	if ( GetModId(MyModName, &MyModId) != 0 ) {
		fprintf(stderr, "shake2ws: Invalid module name <%s>; exiting!\n", MyModName);
		exit(-1);
	}
/* Look up message types of interest */
	if ( GetType("TYPE_HEARTBEAT", &TypeHeartBeat) != 0 ) {
		fprintf(stderr, "shake2ws: Invalid message type <TYPE_HEARTBEAT>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_ERROR", &TypeError) != 0) {
		fprintf(stderr, "shake2ws: Invalid message type <TYPE_ERROR>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_TRACEPEAK", &TypeTracePeak) != 0 ) {
		fprintf(stderr, "shake2ws: Invalid message type <TYPE_TRACEPEAK>; exiting!\n");
		exit(-1);
	}

	return;
}

/*
 * shake2ws_status() - Builds a heartbeat or error message & puts it into
 *                     shared memory.  Writes errors to log file & screen.
 */
static void shake2ws_status( unsigned char type, short ierr, char *note )
{
	MSG_LOGO    logo;
	char        msg[512];
	uint64_t    size;
	time_t      t;

/* Build the message */
	logo.instid = InstId;
	logo.mod    = MyModId;
	logo.type   = type;

	time(&t);

	if( type == TypeHeartBeat ) {
		sprintf(msg, "%ld %ld\n", (long)t, (long)MyPid);
	}
	else if( type == TypeError ) {
		sprintf(msg, "%ld %hd %s\n", (long)t, ierr, note);
		logit("et", "shake2ws: %s\n", note);
	}

	size = strlen(msg);   /* don't include the null byte in the message */

/* Write the message to shared memory */
	if( tport_putmsg(&Region, &logo, size, msg) != PUT_OK ) {
		if( type == TypeHeartBeat ) {
			logit("et","shake2ws: Error sending heartbeat.\n");
		}
		else if( type == TypeError ) {
			logit("et","shake2ws: Error sending error:%d.\n", ierr);
		}
	}

	return;
}

/*
 * shake2ws_end() - free all the local memory & close socket
 */
static void shake2ws_end( void )
{
	tport_detach( &Region );
	sk2ws_list_end();

	return;
}

/*
 *
 */
static thr_ret shake2ws_thread_lwsservice( void *dummy )
{
	int    ret = 0;
/* Related definition for Websocket server */
	struct lws_context_creation_info lwsinfo;
	struct lws_context              *ctx = NULL;

/* Tell the main thread we're ok */
	LWSServiceStatus = THREAD_ALIVE;

/* Initialization for websocket server */
	memset(&lwsinfo, 0, sizeof(lwsinfo));
	lwsinfo.port                     = WSPort;
	lwsinfo.protocols                = Protocols;
	lwsinfo.ssl_cert_filepath        = NULL;
	lwsinfo.ssl_private_key_filepath = NULL;
	lwsinfo.extensions               = NULL;
	lwsinfo.ssl_cipher_list          = NULL;
	lwsinfo.gid                      = -1;
	lwsinfo.uid                      = -1;
	lwsinfo.options                  = LWS_SERVER_OPTION_VALIDATE_UTF8;
	lws_set_log_level(0, NULL);
/* */
	if ( !(ctx = lws_create_context(&lwsinfo)) ) {
		logit("e","shake2ws: Cannot initialize the websocket server, exiting!\n");
		exit(-1);
	}

/* Main loop, The lws library is SINGLE thread!! */
	while ( ret >= 0 && Finish )
		ret = lws_service(ctx, 0);

/* we're quitting */
	lws_context_destroy(ctx);
/* File a complaint to the main thread */
	if ( Finish )
		LWSServiceStatus = THREAD_ERR;
/* Main thread will restart us */
	KillSelfThread();

	return NULL;
}

/*
 *
 */
static int is_single_pvalue_sync( const STATION_PEAK *stapeak, const int pvalue_i )
{
	DL_NODE   *node    = NULL;
	CHAN_PEAK *chapeak = NULL;
	time_t     ptime   = 0;

/* */
	DL_LIST_FOR_EACH_DATA( (DL_NODE *)stapeak->chlist[pvalue_i], node, chapeak ) {
		if ( ptime == 0 )
			ptime = (time_t)chapeak->ptime;
		else if ( (time_t)chapeak->ptime != ptime )
			return 0;
	}


	return 1;
}

/*
 *
 */
static int is_needed_pvalues_sync( const STATION_PEAK *stapeak, const int intensity_i )
{
	time_t ptime;

/* */
	ptime = stapeak->ptime[GenIntensity[intensity_i].pvindex[0]];
/* */
	if ( labs(time(NULL) - ptime) > LATENCY_THRESHOLD )
		return 0;
/* */
	for ( int i = 1; i < GenIntensity[intensity_i].npvalue; i++ ) {
		if ( labs((time_t)stapeak->ptime[GenIntensity[intensity_i].pvindex[i]] - ptime) > 2 )
			return 0;
	}

	return 1;
}

/*
 *
 */
static double update_single_pvalue( STATION_PEAK *stapeak, const int pvalue_i )
{
	DL_NODE   *node    = NULL;
	CHAN_PEAK *chapeak = NULL;
	double     pvalue  = NULL_PEAKVALUE;
	double     ptime   = 0.0;

/* */
	DL_LIST_FOR_EACH_DATA( (DL_NODE *)stapeak->chlist[pvalue_i], node, chapeak ) {
		if ( chapeak->pvalue > pvalue ) {
			pvalue = chapeak->pvalue;
			ptime  = chapeak->ptime;
		}
	}
/* */
	stapeak->pvalue[pvalue_i] = pvalue;
	stapeak->ptime[pvalue_i]  = ptime;
	stapeak->plevel[pvalue_i] =
		pvalue > 0.0 ? shake_get_intensity( &pvalue, 1, SetPeakValue[pvalue_i].inttype ) + 1 : 0;

	return pvalue;
}

/*
 *
 */
static void update_related_intensities( STATION_PEAK *stapeak, const int pvalue_i )
{
	int    intensity_i;
	int    npvalue;
	double pvalues[MAX_TYPE_PEAKVALUE];

/* */
	for ( int i = 0; i < SetPeakValue[pvalue_i].nrelated; i++ ) {
	/* */
		intensity_i = SetPeakValue[pvalue_i].related_int[i];
		if ( is_needed_pvalues_sync( stapeak, intensity_i ) ) {
			npvalue = GenIntensity[intensity_i].npvalue;
			for ( int j = 0; j < npvalue; j++ )
				pvalues[j] = stapeak->pvalue[GenIntensity[intensity_i].pvindex[j]];
			/* */
			stapeak->intensity[intensity_i] =
				shake_get_intensity( pvalues, npvalue, GenIntensity[intensity_i].inttype ) + 1;
		}
		else {
			stapeak->intensity[intensity_i] = 0;
		}
	}

	return;
}

/*
 *
 */
static void check_station_latency( void *nodep, const int seq, void *arg )
{
	STATION_PEAK *stapeak = (STATION_PEAK *)nodep;
	CHAN_PEAK    *chapeak = NULL;
	DL_NODE      *node    = NULL;
	DL_NODE      *safe    = NULL;
	time_t        timenow = *(time_t *)arg;

	for ( int i = 0; i < nPeakValue; i++ ) {
		if ( (timenow - (time_t)stapeak->ptime[i]) > LATENCY_THRESHOLD ) {
			stapeak->pvalue[i] = NULL_PEAKVALUE;
			stapeak->ptime[i]  = NULL_PEAKVALUE;
			stapeak->plevel[i] = 0;
			update_related_intensities( stapeak, i );
		/* We should also drop the channels that have already stopped for over LATENCY_THRESHOLD */
			DL_LIST_FOR_EACH_DATA_SAFE( (DL_NODE *)stapeak->chlist[i], node, chapeak, safe ) {
			/* */
				if ( chapeak->ptime < 0.0 ) {
					sk2ws_list_chlist_delete( stapeak, chapeak->chan, i );
				}
				else if ( (timenow - (time_t)chapeak->ptime) > LATENCY_THRESHOLD ) {
					chapeak->pvalue = NULL_PEAKVALUE;
					chapeak->ptime  = NULL_PEAKVALUE;
				}
			}
		}
	}

	return;
}

/*
 *
 */
static void *proc_com_sv_index( const char *command )
{
	int *result = (int *)calloc(1, sizeof(int));

/* */
	*result = atoi(command);

	return result;
}

/*
 *
 */
static int proc_com_input_pv( int inputpv, const int gen_index )
{
	int pvcount = 0;

/* Processing of input message list */
	if ( inputpv > ((1 << MAX_TYPE_PEAKVALUE) - 1) || inputpv < 1 ) {
		logit("e", "shake2ws: ERROR! Excessive value of input messages(range is 1~255).\n");
		return -1;
	}
/* */
	memset(GenIntensity[gen_index].pvindex, 0, sizeof(uint8_t) * MAX_TYPE_PEAKVALUE);
	for ( int i = 0; i < MAX_TYPE_PEAKVALUE; i++, inputpv >>= 1 ) {
		if ( inputpv & 0x01 ) {
			GenIntensity[gen_index].pvindex[pvcount++] = i;
			SetPeakValue[i].related_int[SetPeakValue[i].nrelated] = gen_index;
			SetPeakValue[i].nrelated++;
		}
	}
/* */
	if ( pvcount != shake_get_reqinputs( GenIntensity[gen_index].inttype ) ) {
		logit(
			"e", "shake2ws: ERROR! The number of inputs is not correct(it should be %d for %s).\n",
			shake_get_reqinputs( GenIntensity[gen_index].inttype ),
			shakenum2str( GenIntensity[gen_index].inttype )
		);
		return -1;
	}

	return pvcount;
}
