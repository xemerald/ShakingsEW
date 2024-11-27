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
#include <time.h>
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
#include <triglist.h>
#include <tracepeak.h>
#include <shakereport.h>
#include <shakereport_list.h>
#include <shakereport_msg_queue.h>

/* Functions prototype in this source file */
static void shakereport_config( char * );
static void shakereport_lookup( void );
static void shakereport_status( unsigned char, short, char * );
static void shakereport_end( void );                /* Free all the local memory & close socket */
static thr_ret shakereport_output_thread( void * );
static thr_ret shakereport_oneshot_thread( void * );
/* */
static int   output_hashtable_rdrec( redisContext *, REDIS_RECORDS * );
static int   append_shakerec2rdrec( REDIS_RECORDS *, const SHAKE_RECORD * );
static char *append_str2rdrec( REDIS_RECORDS *, const char * );
static char *terminate_rdrec( REDIS_RECORDS * );
/* */
static int    is_single_pvalue_sync( const STATION_PEAK *, const int );
static int    is_needed_pvalues_sync( const STATION_PEAK *, const int );
static void   update_related_intensities( STATION_PEAK *, const int );
static double update_single_pvalue( STATION_PEAK *, const int );
static void   check_station_latency( void *, const int, void * );
static double get_precise_timenow( void );
static void  *proc_com_sv_index( const char * );
static int    proc_com_input_pv( int, const int );
/* */
static SHAKE_RECORD *enrich_shake_record(
	SHAKE_RECORD *, const char *, const char *, const char *, const char *, const double, const double
);

/* Ring messages things */
static SHM_INFO PeakRegion;      /* shared memory region to use for i/o    */
static SHM_INFO TrigRegion;      /* shared memory region to use for i/o    */

#define MAXLOGO 5

static MSG_LOGO Getlogo[MAXLOGO];       /* array for requesting module, type, instid */
static pid_t    MyPid;                  /* for restarts by startstop                 */

/* Thread things */
#define THREAD_STACK  8388608       /* 8388608 Byte = 8192 Kilobyte = 8 Megabyte */
#define THREAD_OFF    0				/* ComputePGA has not been started      */
#define THREAD_ALIVE  1				/* ComputePGA alive and well            */
#define THREAD_ERR   -1				/* ComputePGA encountered error quit    */
volatile int   OutputThreadStatus  = THREAD_OFF;
volatile int   OneshotThreadStatus = THREAD_OFF;
volatile _Bool Finish = 0;

/* Things to read or derive from configuration file */
static char     PeakRingName[MAX_RING_STR];  /* name of transport ring for i/o    */
static char     TrigRingName[MAX_RING_STR];  /* name of transport ring for i/o    */
static char     MyModName[MAX_MOD_STR];      /* speak as this module name/id      */
static uint8_t  LogSwitch;                   /* 0 if no logfile should be written */
static uint64_t HeartBeatInterval;           /* seconds between heartbeats        */
static uint64_t QueueSize;                   /* max messages in output circular buffer */
static uint16_t nLogo  = 0;
static uint16_t nPeakValue = 0;
static uint16_t nIntensity = 0;
static uint64_t ReportInterval = DEFAULT_REPORT_INTERVAL;
static char     ReportPath[MAX_PATH_LENGTH];

/* Things to look up in the earthworm.h tables with getutil.c functions */
static int64_t PeakRingKey;     /* key of transport ring for i/o     */
static int64_t TrigRingKey;     /* key of transport ring for i/o     */
static uint8_t InstId;          /* local installation id             */
static uint8_t MyModId;         /* Module Id for this program        */
static uint8_t TypeHeartBeat;
static uint8_t TypeError;
static uint8_t TypeTracePeak = 0;
static uint8_t TypeTrigList  = 0;

/* Error messages used by shakereport */
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
	char     prefix[MAX_PREFIX_LENGTH];
} SetPeakValue[MAX_TYPE_PEAKVALUE];
/* */
static struct {
	uint8_t  inttype;
	uint8_t  npvalue;
	uint8_t  pvindex[MAX_TYPE_PEAKVALUE];
} GenIntensity[MAX_TYPE_INTENSITY];
/* */
static struct {
	mutex_t mutex;      /* */
	void   *entry;      /* */
} DelayShakeRecList;

/* */
#define INIT_REDIS_RECORDS(REDISREC) \
		__extension__({ \
			memset((REDISREC), 0, sizeof(REDIS_RECORDS)); \
			(REDISREC)->npos = (REDISREC)->records; \
		})

#define MARK_TIMESTAMP_REDISREC(REDISREC) \
		((REDISREC)->timestamp = get_precise_timenow())

#define REDIS_RECORDS_IS_EMPTY(REDISREC) \
		((REDISREC)->npos == (REDISREC)->records)

#define SHAKEREC_BELONGS_REDISREC(REDISREC, SHAKEREC) \
		(!strcmp((REDISREC)->records, (SHAKEREC)->table))

/*
 *
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

	TrigListPacket  tlist;
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
		fprintf(stderr, "Usage: shakereport <configfile>\n");
		exit(0);
	}
/* Initialize name of log-file & open it */
	logit_init(argv[1], 0, 256, 1);
/* Read the configuration file(s) */
	shakereport_config( argv[1] );
	logit("" , "%s: Read command file <%s>\n", argv[0], argv[1]);
/* Look up important info from earthworm.h tables */
	shakereport_lookup();
/* Reinitialize logit to desired logging level */
	logit_init(argv[1], 0, 256, LogSwitch);
	lockfile = ew_lockfile_path(argv[1]);
	if ( (lockfile_fd = ew_lockfile(lockfile) ) == -1 ) {
		fprintf(stderr, "one instance of %s is already running, exiting\n", argv[0]);
		exit (-1);
	}
/* */
	scnlfilter_init( "shakereport" );
/* */
	Finish = 1;
/* Get process ID for heartbeat messages */
	MyPid = getpid();
	if ( MyPid == -1 ) {
		logit("e","shakereport: Cannot get pid. Exiting.\n");
		exit (-1);
	}
/* Attach to Input/Output shared memory ring */
	tport_attach(&PeakRegion, PeakRingKey);
	logit("", "shakereport: Attached to public memory region %s: %ld\n", PeakRingName, PeakRingKey);
	tport_attach(&TrigRegion, TrigRingKey);
	logit("", "shakereport: Attached to public memory region %s: %ld\n", TrigRingName, TrigRingKey);
/* Flush the transport ring */
	tport_flush(&PeakRegion, Getlogo, nLogo, &reclogo);
	tport_flush(&TrigRegion, Getlogo, nLogo, &reclogo);
/* */
	skrep_list_init();
	skrep_msgqueue_init( QueueSize, sizeof(SHAKE_RECORD) + 1 );
/* Force a heartbeat to be issued in first pass thru main loop */
	timelastbeat  = time(&timenow) - HeartBeatInterval - 1;
	timelastcheck = timenow + 1;
/*----------------------- setup done; start main loop -------------------------*/
	while ( 1 ) {
	/* Send shakereport's heartbeat */
		if ( (time(&timenow) - timelastbeat) >= (int64_t)HeartBeatInterval ) {
			timelastbeat = timenow;
			shakereport_status( TypeHeartBeat, 0, "" );
		}
	/* */
		if ( (timenow - timelastcheck) >= 1 ) {
			timelastcheck = timenow;
			skrep_list_walk( check_station_latency, &timelastcheck );
		}
	/* */
		if ( OutputThreadStatus != THREAD_ALIVE ) {
			if ( StartThread( shakereport_output_thread, (unsigned)THREAD_STACK, &tid ) == -1 ) {
				logit("e", "shakereport: Error starting thread(output_thread); exiting!\n");
				shakereport_end();
				exit(-1);
			}
		/* Just wait for the connection & preparation */
			sleep_ew(1000);
			OutputThreadStatus = THREAD_ALIVE;
		}

	/* Process all new messages */
		do {
		/* See if a termination has been requested */
			res = tport_getflag( &PeakRegion );
			if ( res == TERMINATE || res == MyPid ) {
			/* write a termination msg to log file */
				logit("t", "shakereport: Termination requested; exiting!\n");
				fflush(stdout);
				goto exit_procedure;
			}

		/* Get msg & check the return code from transport */
			res = tport_getmsg(&PeakRegion, Getlogo, nLogo, &reclogo, &recsize, (char *)buffer.msg, TRACE_PEAKVALUE_SIZE);
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
				shakereport_status( TypeError, ERR_TOOBIG, Text );
				continue;
			}
		/* got a msg, but missed some */
			else if ( res == GET_MISS ) {
				sprintf(
					Text, "Missed msg(s)  i%u m%u t%u  %s.", reclogo.instid, reclogo.mod, reclogo.type, PeakRingName
				);
				shakereport_status( TypeError, ERR_MISSMSG, Text );
			}
		/* got a msg, but can't tell */
			else if ( res == GET_NOTRACK ) {
			/* if any were missed */
				sprintf(
					Text, "Msg received (i%u m%u t%u); transport.h NTRACK_GET exceeded",
					reclogo.instid, reclogo.mod, reclogo.type
				);
				shakereport_status( TypeError, ERR_NOTRACK, Text );
			}

		/* Process the message */
			if ( reclogo.type == TypeTracePeak ) {
			/* Initialize for in-list checking */
				stapeak = NULL;
				chapeak = NULL;
			/* If this trace is already inside the local list, it would skip the SCNL filter */
				if (
					!((stapeak = skrep_list_find( buffer.tpv.sta, buffer.tpv.net, buffer.tpv.loc )) &&
					(chapeak = skrep_list_chlist_find( stapeak, buffer.tpv.chan ))) &&
					!scnlfilter_trace_apply( buffer.msg, reclogo.type, &_match )
				) {
				#ifdef _DEBUG
					printf("shakereport: Found SCNL %s.%s.%s.%s but not in the filter, drop it!\n",
					buffer.tpv.sta, buffer.tpv.chan, buffer.tpv.net, buffer.tpv.loc);
				#endif
					continue;
				}
			/* If this is the first time recv. this station, try to insert into the binary tree */
				if ( !stapeak && !(stapeak = skrep_list_search( buffer.tpv.sta, buffer.tpv.net, buffer.tpv.loc )) ) {
				/* Error when insert into the tree */
					logit(
						"e", "shakereport: %s.%s.%s insert into station tree error, drop this trace.\n",
						buffer.tpv.sta, buffer.tpv.net, buffer.tpv.loc
					);
					continue;
				}
			/* And then the same as the channel */
				if ( !chapeak ) {
					_extra = scnlfilter_extra_get( _match );
					if ( !(chapeak = skrep_list_chlist_search( stapeak, buffer.tpv.chan, *(int *)_extra )) ) {
					/* Error when insert into the tree */
						logit(
							"e", "shakereport: %s.%s.%s.%s insert into channel list error, drop this trace.\n",
							buffer.tpv.sta, buffer.tpv.chan, buffer.tpv.net, buffer.tpv.loc
						);
						continue;
					}
					if ( scnlfilter_trace_remap( buffer.msg, reclogo.type, _match ) ) {
					/* First time remap the trace's SCNL */
						printf(
							"shakereport: Remap received trace SCNL %s.%s.%s.%s to %s.%s.%s.%s!\n",
							stapeak->sta, chapeak->chan, stapeak->net, stapeak->loc,
							buffer.tpv.sta, buffer.tpv.chan, buffer.tpv.net, buffer.tpv.loc
						);
					}
					chapeak->match = _match;
				}
				else {
					scnlfilter_trace_remap( buffer.msg, reclogo.type, chapeak->match );
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

		/* Get msg & check the return code from transport */
			res = tport_getmsg(&TrigRegion, Getlogo, nLogo, &reclogo, &recsize, (char *)tlist.msg, MAX_TRIGLIST_SIZ);
		/* no more new messages */
			if ( res == GET_NONE ) {
				break;
			}
		/* next message was too big */
			else if ( res == GET_TOOBIG ) {
			/* complain and try again   */
				sprintf(
					Text, "Retrieved msg[%ld] (i%u m%u t%u) too big for Buffer[%ld]",
					recsize, reclogo.instid, reclogo.mod, reclogo.type, MAX_TRIGLIST_SIZ
				);
				shakereport_status( TypeError, ERR_TOOBIG, Text );
				continue;
			}
		/* got a msg, but missed some */
			else if ( res == GET_MISS ) {
				sprintf(
					Text, "Missed msg(s)  i%u m%u t%u  %s.", reclogo.instid, reclogo.mod, reclogo.type, TrigRingName
				);
				shakereport_status( TypeError, ERR_MISSMSG, Text );
			}
		/* got a msg, but can't tell */
			else if ( res == GET_NOTRACK ) {
			/* if any were missed */
				sprintf(
					Text, "Msg received (i%u m%u t%u); transport.h NTRACK_GET exceeded",
					reclogo.instid, reclogo.mod, reclogo.type
				);
				shakereport_status( TypeError, ERR_NOTRACK, Text );
			}

		/* Process the message */
			if ( reclogo.type == TypeTrigList ) {
			/* Check the received message is what we set in the configfile */
				if ( tlist.tlh.trigtype == TriggerAlgType ) {
					TRIGLIST_STATION *tsta = (TRIGLIST_STATION *)(&tlist.tlh + 1);
					TRIGLIST_STATION *tstaend = tsta + tlist.tlh.trigstations;

					for ( ; tsta < tstaend; tsta++ ) {
						if ( (staptr = shakemap_list_find( tsta->sta, tsta->net, tsta->loc )) == NULL ) {
						/* Not found in trace table */
							//printf("shakemap: %s.%s.%s.%s not found in station table, maybe it's a new trace.\n",
							//tracepv.sta, tracepv.chan, tracepv.net, tracepv.loc); */
							continue;
						}

					/* Insert the triggered station pointer to the trigger list */
						if ( shakemap_tlist_insert( staptr ) == NULL ) {
							logit( "e","shakemap: Error insert station %s into trigger list.\n", staptr->sta );
							continue;
						}
					}
				/* */
					if ( !trigseccount )
						timeLastTrigger = (time_t)tlist.tlh.origintime;
				/* */
					trigseccount = TriggerDuration;
				} /* tlist.tlh.trigtype == TriggerAlgType */
			}
		} while ( 1 );  /* end of message-processing-loop */
	/* No more messages; wait for new ones to arrive */
		sleep_ew(40);
	}
/*-----------------------------end of main loop-------------------------------*/
exit_procedure:
	Finish = 0;
	shakereport_end();
	ew_unlockfile(lockfile_fd);
	ew_unlink_lockfile(lockfile);

	return 0;
}

/*
 * shakereport_config() - Processes command file(s) using kom.c functions;
 *                     exits if any errors are encountered.
 */
static void shakereport_config( char *configfile )
{
	char  init[11];     /* init flags, one byte for each required command */
	char *com;
	char *str;

	int ncommand;     /* # of required commands you expect to process   */
	int nmiss;        /* number of required commands that were missed   */
	int nfiles;
	int success;
	int i;

/* Set to zero one init flag for each required command */
	ncommand = 11;
	for ( i = 0; i < ncommand; i++ )
		init[i] = 0;

/* Open the main configuration file */
	nfiles = k_open( configfile );
	if ( nfiles == 0 ) {
		logit("e", "shakereport: Error opening command file <%s>; exiting!\n", configfile);
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
					logit("e", "shakereport: Error opening command file <%s>; exiting!\n", &com[1]);
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
			else if ( k_its("PeakRing") ) {
				str = k_str();
				if ( str )
					strcpy(PeakRingName, str);
				init[2] = 1;
			}
		/* 3 */
			else if ( k_its("TrigRing") ) {
				str = k_str();
				if ( str )
					strcpy(TrigRingName, str);
				init[3] = 1;
			}
		/* 4 */
			else if ( k_its("HeartBeatInterval") ) {
				HeartBeatInterval = k_long();
				init[4] = 1;
			}
		/* 5 */
			else if ( k_its("QueueSize") ) {
				QueueSize = k_long();
				init[5] = 1;
			}
			else if( k_its("ReportInterval") ) {
				ReportInterval = k_long();
				if ( ReportInterval )
					logit(
						"o", "shakereport: Change to reporting interval, the new reporting interval is %ld seconds!\n",
						ReportInterval
					);
			}
		/* 6 */
			else if( k_its("ReportPath") ) {
				str = k_str();
				if ( str )
					strcpy(ReportPath,str);
				if ( ReportPath[strlen(ReportPath) - 1] != '/' )
					strncat(ReportPath, "/", 1);

				logit("o", "shakereport: Reporting output path is <%s>\n", ReportPath);
				init[6] = 1;
			}
		/* Enter installation & module to get event messages from */
		/* 7 */
			else if ( k_its("GetEventsFrom") ) {
				if ( (nLogo + 1) >= MAXLOGO ) {
					logit("e", "shakereport: Too many <GetEventsFrom> commands in <%s>", configfile);
					logit("e", "; max=%d; exiting!\n", (int)MAXLOGO);
					exit(-1);
				}
				if ( (str = k_str()) ) {
					if ( GetInst(str, &Getlogo[nLogo].instid) != 0 ) {
						logit("e", "shakereport: Invalid installation name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				if ( (str = k_str()) ) {
					if ( GetModId(str, &Getlogo[nLogo].mod) != 0 ) {
						logit("e", "shakereport: Invalid module name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				if ( (str = k_str()) ) {
					if ( GetType(str, &Getlogo[nLogo].type) != 0 ) {
						logit("e", "shakereport: Invalid message type name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				nLogo++;
				init[7] = 1;
			}
		/* 8 */
			else if ( k_its("SetPeakValueType") ) {
				if ( (nPeakValue + 1) >= MAX_TYPE_PEAKVALUE ) {
					logit("e", "shakereport: Too many <SetPeakValueType> commands in <%s>", configfile);
					logit("e", "; max=%d; exiting!\n", (int)MAX_TYPE_PEAKVALUE);
					exit(-1);
				}
			/* Parse the index of SetPeakValue first */
				i = k_int();
				if ( i >= 0 && i < MAX_TYPE_PEAKVALUE ) {
					if ( (str = k_str()) ) {
						SetPeakValue[i].rectype = typestr2num( str );
						logit(
							"o", "shakereport: No.%d Peak value type set to %s:%d ",
							i, str, SetPeakValue[i].rectype
						);
					}
					if ( (str = k_str()) ) {
						strcpy(SetPeakValue[i].prefix, str);
						logit("o", "and the setting prefix is %s\n", SetPeakValue[i].prefix);
					}
				/* */
					SetPeakValue[i].nrelated = 0;
					nPeakValue++;
					init[8] = 1;
				}
				else {
					logit("e", "shakereport: Error when parsing index of <SetPeakValueType> ");
					logit("e", "; setting number might too large, max=%d; exiting!\n", (int)MAX_TYPE_PEAKVALUE);
					exit(-1);
				}
			}
		/* 9 */
			else if ( k_its("GenIntensityType") ) {
				if ( (nIntensity + 1) >= MAX_TYPE_INTENSITY ) {
					logit("e", "shakereport: Too many <GenIntensityType> commands in <%s>", configfile);
					logit("e", "; max=%d; exiting!\n", (int)MAX_TYPE_INTENSITY);
					exit(-1);
				}
				if ( (str = k_str()) ) {
					GenIntensity[nIntensity].inttype = shakestr2num( str );
					if ( GenIntensity[nIntensity].inttype == SHAKE_TYPE_COUNT ) {
						logit("e", "shakereport: Unknown type of intensity, exiting!\n");
						exit(-1);
					}
					logit(
						"o", "shakereport: No.%d intensity type set to %s:%d!\n",
						nIntensity, str, GenIntensity[nIntensity].inttype
					);
				}
			/* Processing of input message list */
				if ( (i = proc_com_input_pv( k_int(), nIntensity )) < 0 ) {
					logit("e", "shakereport: ERROR when parsing input peak value of <%s> command; exiting!\n", com);
					exit(-1);
				}
				GenIntensity[nIntensity].npvalue = i;
				nIntensity++;
				init[9] = 1;
			}
		/* 10 */
			else if ( scnlfilter_com( "shakereport" ) ) {
			/* */
				for ( str = k_com(), i = 0; *str == ' ' && i < (int)strlen(str); str++, i++ );
				if ( strncmp(str, "Block_SCNL", 10) ) {
				/* Maybe we need much more checking for this command */
					if ( scnlfilter_extra_com( proc_com_sv_index ) < 0 ) {
						logit("o", "shakereport: No set value index define in command: \"%s\" ", k_com());
						logit("o", ", first index(0) will be filled!\n");
					/* Reset the error code for this command */
						k_err();
					}
					init[10] = 1;
				}
			}
		/* Unknown command*/
			else {
				logit("e", "shakereport: <%s> Unknown command in <%s>.\n", com, configfile);
				continue;
			}

		/* See if there were any errors processing the command */
			if ( k_err() ) {
				logit("e", "shakereport: Bad <%s> command in <%s>; exiting!\n", com, configfile);
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
	if ( nmiss ) {
		logit("e", "shakereport: ERROR, no ");
		if ( !init[0] )  logit("e", "<LogFile> "              );
		if ( !init[1] )  logit("e", "<MyModuleId> "           );
		if ( !init[2] )  logit("e", "<PeakRing> "             );
		if ( !init[3] )  logit("e", "<TrigRing> "             );
		if ( !init[4] )  logit("e", "<HeartBeatInterval> "    );
		if ( !init[5] )  logit("e", "<QueueSize> "            );
		if ( !init[6] )  logit("e", "<ReportPath> "           );
		if ( !init[7] )  logit("e", "any <GetEventsFrom> "    );
		if ( !init[8] )  logit("e", "any <SetPeakValueType> " );
		if ( !init[9] )  logit("e", "any <GenIntensityType> " );
		if ( !init[10] ) logit("e", "any <Allow_SCNL*> "      );

		logit("e", "command(s) in <%s>; exiting!\n", configfile);
		exit(-1);
	}

	return;
}

/*
 * shakereport_lookup() - Look up important info from earthworm.h tables
 */
static void shakereport_lookup( void )
{
/* Look up keys to shared memory regions */
	if ( (PeakRingKey = GetKey(PeakRingName)) == -1 ) {
		fprintf(stderr, "shakereport: Invalid ring name <%s>; exiting!\n", PeakRingName);
		exit(-1);
	}
	if ( (TrigRingKey = GetKey(TrigRingName)) == -1 ) {
		fprintf(stderr, "shakereport: Invalid ring name <%s>; exiting!\n", TrigRingName);
		exit(-1);
	}
/* Look up installations of interest */
	if ( GetLocalInst(&InstId) != 0 ) {
		fprintf(stderr, "shakereport: Error getting local installation id; exiting!\n");
		exit(-1);
	}
/* Look up modules of interest */
	if ( GetModId(MyModName, &MyModId) != 0 ) {
		fprintf(stderr, "shakereport: Invalid module name <%s>; exiting!\n", MyModName);
		exit(-1);
	}
/* Look up message types of interest */
	if ( GetType("TYPE_HEARTBEAT", &TypeHeartBeat) != 0 ) {
		fprintf(stderr, "shakereport: Invalid message type <TYPE_HEARTBEAT>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_ERROR", &TypeError) != 0) {
		fprintf(stderr, "shakereport: Invalid message type <TYPE_ERROR>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_TRACEPEAK", &TypeTracePeak) != 0 ) {
		fprintf(stderr, "shakereport: Invalid message type <TYPE_TRACEPEAK>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_TRIGLIST", &TypeTrigList) != 0 ) {
		fprintf(stderr, "shakereport: Invalid message type <TYPE_TRIGLIST>; exiting!\n");
		exit(-1);
	}

	return;
}

/*
 * shakereport_status() - Builds a heartbeat or error message & puts it into
 *                     shared memory.  Writes errors to log file & screen.
 */
static void shakereport_status( unsigned char type, short ierr, char *note )
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

	if ( type == TypeHeartBeat ) {
		sprintf(msg, "%ld %ld\n", (long)t, (long)MyPid);
	}
	else if ( type == TypeError ) {
		sprintf(msg, "%ld %hd %s\n", (long)t, ierr, note);
		logit("et", "shakereport: %s\n", note);
	}

	size = strlen(msg);   /* don't include the null byte in the message */

/* Write the message to shared memory */
	if ( tport_putmsg(&PeakRegion, &logo, size, msg) != PUT_OK ) {
		if ( type == TypeHeartBeat ) {
			logit("et","shakereport: Error sending heartbeat.\n");
		}
		else if ( type == TypeError ) {
			logit("et","shakereport: Error sending error:%d.\n", ierr);
		}
	}

	return;
}

/*
 * shakereport_end() - free all the local memory & close socket
 */
static void shakereport_end( void )
{
	tport_detach(&PeakRegion);
	tport_detach(&TrigRegion);
	skrep_list_end();
	skrep_msgqueue_end();

	return;
}

/*
 *
 */
static thr_ret shakereport_output_thread( void *dummy )
{
	int     i;
	int     max_rec = MIN_RECORDS_PER_RDREC;
	uint8_t full_flag = 0;
	double  timenow;
/* */
	int            rdrec_num = MAX_TYPE_PEAKVALUE * LATENCY_THRESHOLD;
	REDIS_RECORDS *rdrec_main  = NULL;
	REDIS_RECORDS *rdrec_ptr   = NULL;
	REDIS_RECORDS *rdrec_empty = NULL;
/* */
	SHAKE_RECORD   shakerec;
	SHAKE_RECORD  *_shakerec = NULL;
	size_t         rec_size;
	MSG_LOGO       rec_logo;
/* */
#if defined( _V710 )
	ew_thread_t   tid;            /* Thread ID */
#else
	unsigned      tid;            /* Thread ID */
#endif

/* Tell the main thread we're ok */
	OutputThreadStatus = THREAD_ALIVE;
/* */
	rdrec_main = (REDIS_RECORDS *)calloc(rdrec_num, sizeof(REDIS_RECORDS));
	if ( rdrec_main ) {
		for ( i = 0, rdrec_ptr = rdrec_main; i < rdrec_num; i++, rdrec_ptr++ )
			INIT_REDIS_RECORDS( rdrec_ptr );
	}
	else {
		logit("e", "shakereport: Cannot allocate the memory for Redis buffer, exiting!\n");
		goto disconnect;
	}
/* Initialization for the delay list */
	CreateSpecificMutex(&DelayShakeRecList.mutex);
	DelayShakeRecList.entry = NULL;

/* Processing loop */
	do {
	/* */
		if ( full_flag || skrep_msgqueue_dequeue( &shakerec, &rec_size, &rec_logo ) < 0 ) {
		/* */
			full_flag = 0;
		/* */
			max_rec = skrep_list_total_sta_get() * 0.5;
			if ( max_rec < MIN_RECORDS_PER_RDREC )
				max_rec = MIN_RECORDS_PER_RDREC;
			else if ( max_rec > MAX_RECORDS_PER_RDREC )
				max_rec = MAX_RECORDS_PER_RDREC;
		/* */
			timenow = get_precise_timenow();
			for ( i = 0, rdrec_ptr = rdrec_main; i < rdrec_num; i++, rdrec_ptr++ ) {
				if ( !REDIS_RECORDS_IS_EMPTY( rdrec_ptr ) && (timenow - rdrec_ptr->timestamp) > 0.1 ) {
					if ( output_hashtable_rdrec( redis, rdrec_ptr ) )
						goto disconnect;
					INIT_REDIS_RECORDS( rdrec_ptr );
				}
			}
		/* */
			if ( OneshotThreadStatus != THREAD_ALIVE ) {
				if ( StartThread( shakereport_oneshot_thread, (unsigned)THREAD_STACK, &tid ) == -1 )
					logit("e", "shakereport: Error starting thread(oneshot_thread), try it next time!\n");
				else
					OneshotThreadStatus = THREAD_ALIVE;
			}
		/* Wait for next message */
			sleep_ew(10);
		}
		else {
		/* Find a redis record buffer to append this shake record */
			for ( i = 0, rdrec_ptr = rdrec_main, rdrec_empty = NULL; i < rdrec_num; i++, rdrec_ptr++ ) {
				if ( !REDIS_RECORDS_IS_EMPTY( rdrec_ptr ) ) {
				/* This shake record belongs to the redis record */
					if ( SHAKEREC_BELONGS_REDISREC( rdrec_ptr, &shakerec ) )
						break;
				}
				else if ( !rdrec_empty ) {
					rdrec_empty = rdrec_ptr;
				}
			}
		/* */
			if ( i >= rdrec_num ) {
				if ( rdrec_empty ) {
					rdrec_ptr = rdrec_empty;
				/* Fill the table name */
					append_str2rdrec( rdrec_ptr, shakerec.table );
				}
				else {
				/* Since the buffer is already full, we use the oneshot buffer for immediately output */
					if ( (_shakerec = (SHAKE_RECORD *)calloc(1, sizeof(SHAKE_RECORD))) ) {
						*_shakerec = shakerec;
						RequestSpecificMutex(&DelayShakeRecList.mutex);
						if ( dl_node_append( (DL_NODE **)&DelayShakeRecList.entry, _shakerec ) == NULL ) {
							logit("e", "shakereport: Error insert a shake record into delay linked list!\n");
						}
						ReleaseSpecificMutex(&DelayShakeRecList.mutex);
					}
				/* */
					full_flag = 1;
					continue;
				}
			}
		/* */
			MARK_TIMESTAMP_REDISREC( rdrec_ptr );
			if ( append_shakerec2rdrec( rdrec_ptr, &shakerec ) >= max_rec ) {
				if ( output_hashtable_rdrec( redis, rdrec_ptr ) )
					goto disconnect;
				INIT_REDIS_RECORDS( rdrec_ptr );
			}
		}
	} while ( Finish );
/* */
disconnect:
	redisFree(redis);
/* */
	RequestSpecificMutex(&DelayShakeRecList.mutex);
	dl_list_destroy( (DL_NODE **)&DelayShakeRecList.entry, free );
	ReleaseSpecificMutex(&DelayShakeRecList.mutex);
	CloseSpecificMutex(&DelayShakeRecList.mutex);
/* */
	if ( rdrec_main )
		free(rdrec_main);
/* File a complaint to the main thread */
	if ( Finish )
		OutputThreadStatus = THREAD_ERR;

	KillSelfThread();

	return NULL;
}

/*
 *
 */
static thr_ret shakereport_oneshot_thread( void *dummy )
{
/* */
	redisContext *redis = redisConnect(RedisHost, RedisPort);
/* */
	DL_NODE       *node     = NULL;
	SHAKE_RECORD  *shakerec = NULL;
	REDIS_RECORDS  rdrec;

/* Tell the main thread we're ok */
	OneshotThreadStatus = THREAD_ALIVE;
/* */
	if ( redis == NULL || redis->err ) {
		if ( redis )
			logit("e", "shakereport: Connection error %s, exiting!\n", redis->errstr);
		else
			logit("e", "shakereport: Can't allocate redis context, exiting!\n");
	/* */
		exit(-1);
	}
	else if ( strlen(RedisPass) ) {
		if ( auth_redis_server( redis, RedisPass ) )
			goto disconnect;
	}

/* Processing loop */
	do {
	/* */
		RequestSpecificMutex(&DelayShakeRecList.mutex);
		node = dl_node_pop( (DL_NODE **)&DelayShakeRecList.entry );
		ReleaseSpecificMutex(&DelayShakeRecList.mutex);
	/* */
		if ( !node ) {
		/* Wait for next message */
			sleep_ew(200);
		}
		else {
			shakerec = (SHAKE_RECORD *)dl_node_data_extract( node );
			printf(
				"shakereport: Delay record \"%s %s %.6lf\", refilling...\n",
				shakerec->table, shakerec->field, shakerec->value
			);
		/* */
			INIT_REDIS_RECORDS( &rdrec );
			//MARK_TIMESTAMP_REDISREC( &rdrec );
			append_str2rdrec( &rdrec, shakerec->table );
			append_shakerec2rdrec( &rdrec, shakerec );
			if ( output_hashtable_rdrec( redis, &rdrec ) )
				goto disconnect;
		/* */
			free(shakerec);
			shakerec = NULL;
			node = NULL;
		}
	} while ( Finish );
/* */
disconnect:
	redisFree(redis);
/* */
	if ( shakerec )
		free(shakerec);
/* File a complaint to the main thread */
	if ( Finish )
		OneshotThreadStatus = THREAD_ERR;

	KillSelfThread();

	return NULL;
}

/*
 *
 */
static int output_hashtable_rdrec( redisContext *redis, REDIS_RECORDS *rdrec )
{
	redisReply *reply = NULL;
	char        _table[MAX_FIELD_LENGTH];
	char        command[MAX_RECORDS_STR_LEN];

/* */
	strcpy(_table, rdrec->records);
	sprintf(command, "HSET %s", terminate_rdrec( rdrec ));
	if ( !(reply = redisCommand(redis, command)) ) {
		logit("et", "shakereport: Redis command(HSET) error!\n");
		return -1;
	}
	freeReplyObject(reply);
/* */
	sprintf(command, "EXPIRE %s %ld", _table, ExpireTime);
	if ( !(reply = redisCommand(redis, command)) ) {
		logit("et", "shakereport: Redis command(EXPIRE) error!\n");
		return -1;
	}
	freeReplyObject(reply);

	return 0;
}

/*
 *
 */
static int append_shakerec2rdrec( REDIS_RECORDS *rdrec, const SHAKE_RECORD *shakerec )
{
	char value[MAX_FIELD_LENGTH] = { 0 };

/* */
	append_str2rdrec( rdrec, shakerec->field );
/* */
	sprintf(value, "%.6lf", shakerec->value);
	append_str2rdrec( rdrec, value );

	return ++rdrec->record_num;
}

/*
 *
 */
static char *append_str2rdrec( REDIS_RECORDS *rdrec, const char *input )
{
	const int  len = strlen(input);
	const char termination = rdrec->npos == rdrec->records ? '\0' : ' ';

/* */
	memcpy(rdrec->npos, input, len);
	rdrec->npos += len;
	*rdrec->npos = termination;
	rdrec->npos++;

	return rdrec->records;
}

/*
 *
 */
static char *terminate_rdrec( REDIS_RECORDS *rdrec )
{
	const int len = strlen(rdrec->records);

/* */
	rdrec->records[len] = ' ';
	*rdrec->npos = '\0';

	return rdrec->records;
}

/*
 *
 */
static int is_single_pvalue_sync( const STATION_PEAK *stapeak, const int pvalue_i )
{
	DL_NODE   *node    = NULL;
	CHAN_PEAK *chapeak = NULL;
	time_t     _ptime  = 0;

/* */
	DL_LIST_FOR_EACH_DATA( (DL_NODE *)stapeak->chlist[pvalue_i], node, chapeak ) {
		if ( _ptime == 0 )
			_ptime = (time_t)chapeak->ptime;
		else if ( (time_t)chapeak->ptime != _ptime )
			return 0;
	}

	return 1;
}

/*
 *
 */
static int is_needed_pvalues_sync( const STATION_PEAK *stapeak, const int intensity_i )
{
	int    i;
	time_t _ptime;

/* */
	_ptime = stapeak->ptime[GenIntensity[intensity_i].pvindex[0]];
/* */
	if ( labs(time(NULL) - _ptime) > LATENCY_THRESHOLD )
		return 0;
/* */
	for ( i = 1; i < GenIntensity[intensity_i].npvalue; i++ ) {
		if ( labs((time_t)stapeak->ptime[GenIntensity[intensity_i].pvindex[i]] - _ptime) > 2 )
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
	double     _pvalue = NULL_PEAKVALUE;
	double     _ptime  = NULL_PEAKVALUE;
/* */
	SHAKE_RECORD shakerec;

/* */
	DL_LIST_FOR_EACH_DATA( (DL_NODE *)stapeak->chlist[pvalue_i], node, chapeak ) {
		if ( chapeak->pvalue > _pvalue ) {
			_pvalue = chapeak->pvalue;
			_ptime  = chapeak->ptime;
		}
	}
/* */
	stapeak->pvalue[pvalue_i] = _pvalue;
	stapeak->ptime[pvalue_i]  = _ptime;
/* */
	enrich_shake_record(
		&shakerec, stapeak->sta, stapeak->net, stapeak->loc, SetPeakValue[pvalue_i].prefix, _ptime, _pvalue
	);
	skrep_msgqueue_enqueue( &shakerec, sizeof(SHAKE_RECORD), (MSG_LOGO){ 0 } );

	return _pvalue;
}

/*
 *
 */
static void update_related_intensities( STATION_PEAK *stapeak, const int pvalue_i )
{
	int    i, j;
	int    _intensity_i;
	int    _npvalue;
	double _pvalues[MAX_TYPE_PEAKVALUE];
/* */
	//SHAKE_RECORD shakerec;

/* */
	for ( i = 0; i < SetPeakValue[pvalue_i].nrelated; i++ ) {
	/* */
		_intensity_i = SetPeakValue[pvalue_i].related_int[i];
		if ( is_needed_pvalues_sync( stapeak, _intensity_i ) ) {
			_npvalue = GenIntensity[_intensity_i].npvalue;
			for ( j = 0; j < _npvalue; j++ )
				_pvalues[j] = stapeak->pvalue[GenIntensity[_intensity_i].pvindex[j]];
			/* */
			stapeak->intensity[_intensity_i] =
				shake_get_intensity( _pvalues, _npvalue, GenIntensity[_intensity_i].inttype ) + 1;
		}
		else {
			stapeak->intensity[_intensity_i] = 0;
		}
	}

	return;
}

/*
 *
 */
static void check_station_latency( void *nodep, const int seq, void *arg )
{
	int           i;
	STATION_PEAK *stapeak = (STATION_PEAK *)nodep;
	CHAN_PEAK    *chapeak = NULL;
	DL_NODE      *node    = NULL;
	DL_NODE      *safe    = NULL;
	time_t        timenow = *(time_t *)arg;

	for ( i = 0; i < nPeakValue; i++ ) {
		if ( (timenow - (time_t)stapeak->ptime[i]) > LATENCY_THRESHOLD ) {
			stapeak->pvalue[i] = NULL_PEAKVALUE;
			stapeak->ptime[i]  = NULL_PEAKVALUE;
			update_related_intensities( stapeak, i );
		/* We should also drop the channels that have already stopped for over LATENCY_THRESHOLD */
			DL_LIST_FOR_EACH_DATA_SAFE( (DL_NODE *)stapeak->chlist[i], node, chapeak, safe ) {
			/* */
				if ( chapeak->ptime < 0.0 ) {
					skrep_list_chlist_delete( stapeak, chapeak->chan, i );
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
static SHAKE_RECORD *enrich_shake_record(
	SHAKE_RECORD *shakerec, const char *sta, const char *net, const char *loc,
	const char *prefix, const double ptime, const double pvalue
) {
/* */
	sprintf(shakerec->table, SKREP_TABLE_NAME_FORMAT, prefix, (time_t)ptime);
	sprintf(shakerec->field, SKREP_FIELD_NAME_FORMAT, sta, net, loc);
	shakerec->value = pvalue;

	return shakerec;
}

/*
 *
 */
static double get_precise_timenow( void )
{
	struct timespec time_sp;

/* */
	clock_gettime(CLOCK_REALTIME_COARSE, &time_sp);

	return (double)time_sp.tv_sec + (double)time_sp.tv_nsec * 1.0e-9;
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
	int i;
	int pvcount = 0;

/* Processing of input message list */
	if ( inputpv > ((1 << MAX_TYPE_PEAKVALUE) - 1) || inputpv < 1 ) {
		logit("e", "shakereport: ERROR! Excessive value of input messages(range is 1~255).\n");
		return -1;
	}
/* */
	memset(GenIntensity[gen_index].pvindex, 0, sizeof(uint8_t) * MAX_TYPE_PEAKVALUE);
	for ( i = 0; i < MAX_TYPE_PEAKVALUE; i++, inputpv >>= 1 ) {
		if ( inputpv & 0x01 ) {
			GenIntensity[gen_index].pvindex[pvcount++] = i;
			SetPeakValue[i].related_int[SetPeakValue[i].nrelated] = gen_index;
			SetPeakValue[i].nrelated++;
		}
	}
/* */
	if ( pvcount != shake_get_reqinputs( GenIntensity[gen_index].inttype ) ) {
		logit(
			"e", "shakereport: ERROR! The number of inputs is not correct(it should be %d for %s).\n",
			shake_get_reqinputs( GenIntensity[gen_index].inttype ),
			shakenum2str( GenIntensity[gen_index].inttype )
		);
		return -1;
	}

	return pvcount;
}
