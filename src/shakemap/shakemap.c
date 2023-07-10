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
#include <ctype.h>
#include <math.h>
#include <float.h>
#include <time.h>
/* Earthworm environment header include */
#include <earthworm.h>
#include <kom.h>
#include <transport.h>
#include <lockfile.h>
#include <trace_buf.h>
/* Local header include */
#include <recordtype.h>
#include <tracepeak.h>
#include <triglist.h>
#include <geogfunc.h>
#include <polyline.h>
#include <datestring.h>
#include <griddata.h>
#include <shakemap.h>
#include <shakemap_list.h>
#include <shakemap_triglist.h>
#include <shakemap_msg_queue.h>

/* Functions prototype in this source file */
static void shakemap_config( char * );
static void shakemap_lookup( void );
static void shakemap_status( unsigned char, short, char * );
static void shakemap_end( void );                /* Free all the local memory & close socket */

static thr_ret thread_shakingmap( void * );

static void update_list ( void * );
static int  update_list_configfile( char * );
static void cal_mag_values(
	GRIDMAP_HEADER *, const uint32_t, const uint32_t, const uint32_t, const uint32_t
);
static void output_result_file(
	const GRIDMAP_HEADER *, const uint32_t, const uint32_t, const uint32_t, const uint32_t, const uint32_t
);

/* Ring messages things */
static SHM_INFO PeakRegion;    /* shared memory region to use for i/o    */
static SHM_INFO TrigRegion;    /* shared memory region to use for i/o    */
static SHM_INFO OutRegion;     /* shared memory region to use for i/o    */

#define MAXLOGO      5
#define MAX_PATH_STR 256
static MSG_LOGO Putlogo;                /* array for output module, type, instid     */
static MSG_LOGO Getlogo[MAXLOGO];       /* array for requesting module, type, instid */
static pid_t    MyPid;                  /* for restarts by startstop                 */

/* Thread things */
#define THREAD_STACK 8388608         /* 8388608 Byte = 8192 Kilobyte = 8 Megabyte */
#define THREAD_OFF    0              /* Thread has not been started      */
#define THREAD_ALIVE  1              /* Thread alive and well            */
#define THREAD_ERR   -1              /* Thread encountered error quit    */
sema_t  *SemaPtr;

/* */
#define MAXLIST  5
/* Things to read or derive from configuration file */
static char     PeakRingName[MAX_RING_STR];   /* name of transport ring for i/o    */
static char     TrigRingName[MAX_RING_STR];   /* name of transport ring for i/o    */
static char     OutRingName[MAX_RING_STR];  /* name of transport ring for i/o    */
static char     MyModName[MAX_MOD_STR];     /* speak as this module name/id      */
static uint8_t  LogSwitch;                  /* 0 if no logfile should be written */
static uint64_t HeartBeatInterval;          /* seconds between heartbeats        */
static uint64_t QueueSize = 10;             /* seconds between heartbeats        */
static uint8_t  OutputMapToRingSwitch = 0;
static uint16_t nLogo = 0;
static uint16_t TriggerAlgType  = 0;
static uint16_t PeakValueType   = 0;
static uint64_t TriggerDuration = 30;
static char     ReportPath[MAX_PATH_STR];
static void    *BoundPolyLine = NULL;
static double   InterpolateDistance = 30.0;
static DBINFO   DBInfo;
static char     SQLStationTable[MAXLIST][MAX_TABLE_LEGTH];
static uint16_t nList = 0;

/* Things to look up in the earthworm.h tables with getutil.c functions */
static int64_t PeakRingKey;     /* key of transport ring for i/o     */
static int64_t TrigRingKey;     /* key of transport ring for i/o     */
static int64_t OutRingKey;      /* key of transport ring for i/o     */
static uint8_t InstId;          /* local installation id             */
static uint8_t MyModId;         /* Module Id for this program        */
static uint8_t TypeHeartBeat;
static uint8_t TypeError;
static uint8_t TypeTracePeak = 0;
static uint8_t TypeTrigList  = 0;
static uint8_t TypeGridMap   = 0;

/* Error messages used by shakemap */
#define  ERR_MISSMSG       0   /* message missed in transport ring       */
#define  ERR_TOOBIG        1   /* retreived msg too large for buffer     */
#define  ERR_NOTRACK       2   /* msg retreived; tracking limit exceeded */
#define  ERR_QUEUE         3   /* error queueing message for sending      */
static char Text[150];         /* string for log/error messages          */

static volatile int32_t  ShakingMapStatus = THREAD_OFF;
static volatile uint8_t  Finish        = 0;
static volatile uint64_t TotalStations = 0;

/*
 *
 */
int main ( int argc, char **argv )
{
	int      i;
	int      res;
	int64_t  msgSize = 0;
	uint64_t stanumber_local = 0;
	uint64_t trigseccount = 0;
	MSG_LOGO reclogo;
	time_t   time_now;           /* current time                  */
	time_t   time_lastbeat;      /* time last heartbeat was sent  */
	time_t   timeLastTrigger;   /* time last updated stations list */
	char    *lockfile;
	int32_t  lockfile_fd;
#if defined( _V710 )
	ew_thread_t tid;            /* Thread ID */
#else
	unsigned    tid;            /* Thread ID */
#endif
/* Internal data exchange pointer and buffer */
	_STAINFO          *staptr   = NULL;
	STA_SHAKE         *shakeptr = NULL;
	SHAKE_LIST_HEADER *slbuffer = NULL;
/* Input data type and its buffer */
	TRACE_PEAKVALUE    tracepv;
	TrigListPacket     tlist;

/* Check command line arguments */
	if ( argc != 2 ) {
		fprintf(stderr, "Usage: shakemap <configfile>\n");
		exit(0);
	}
	Finish = 1;
/* Initialize name of log-file & open it */
	logit_init(argv[1], 0, 256, 1);
/* Read the configuration file(s) */
	shakemap_config(argv[1]);
	logit("" , "%s: Read command file <%s>\n", argv[0], argv[1]);
/* Read the channels list from remote database */
	for ( i = 0; i < nList; i++ ) {
		if ( shakemap_list_db_fetch( SQLStationTable[i], &DBInfo, SHAKEMAP_LIST_INITIALIZING ) < 0 ) {
			fprintf(stderr, "Something error when fetching stations list %s. Exiting!\n", SQLStationTable[i]);
			exit(-1);
		}
	}
/* Checking total station number again */
	if ( !(i = shakemap_list_total_station_get()) ) {
		fprintf(stderr, "There is not any station in the list after fetching. Exiting!\n");
		exit(-1);
	}
	else {
		logit("o", "shakemap: There are total %d station(s) in the list.\n", i);
		shakemap_list_tree_activate();
	}
/* Look up important info from earthworm.h tables */
	shakemap_lookup();
/* Reinitialize logit to desired logging level */
	logit_init( argv[1], 0, 256, LogSwitch );

	lockfile = ew_lockfile_path(argv[1]);
	if ( (lockfile_fd = ew_lockfile(lockfile) ) == -1 ) {
		fprintf(stderr, "one instance of %s is already running, exiting\n", argv[0]);
		exit(-1);
	}
/* Get process ID for heartbeat messages */
	MyPid = getpid();
	if ( MyPid == -1 ) {
		logit("e","shakemap: Cannot get pid. Exiting.\n");
		exit(-1);
	}
/* Build the message */
	Putlogo.instid = InstId;
	Putlogo.mod    = MyModId;
	Putlogo.type   = TypeGridMap;
/* Attach to Input/Output shared memory ring */
	tport_attach( &PeakRegion, PeakRingKey );
	logit("", "shakemap: Attached to public memory region %s: %ld\n", PeakRingName, PeakRingKey);
	tport_attach( &TrigRegion, TrigRingKey );
	logit("", "shakemap: Attached to public memory region %s: %ld\n", TrigRingName, TrigRingKey);
/* Flush the transport ring */
	tport_flush(&PeakRegion, Getlogo, nLogo, &reclogo);
	tport_flush(&TrigRegion, Getlogo, nLogo, &reclogo);
/* */
	if ( OutputMapToRingSwitch ) {
		tport_attach(&OutRegion, OutRingKey);
		logit("", "shakemap: Attached to public memory region %s: %ld\n", OutRingName, OutRingKey);
	}

	/* CreateSemaphore_ew(); */ /* Obsoleted by Earthworm */
	SemaPtr = CreateSpecificSemaphore_ew( 0 );
	shakemap_msgqueue_init( QueueSize, sizeof(SHAKE_LIST_ELEMENT)*TotalStations + sizeof(SHAKE_LIST_HEADER) );

/* Force a heartbeat to be issued in first pass thru main loop */
	time_lastbeat   = time(&time_now) - HeartBeatInterval - 1;
	timeLastTrigger = time_now;
/*----------------------- setup done; start main loop -------------------------*/
	while ( 1 ) {
	/* Send shakemap's heartbeat */
		if  ( time(&time_now) - time_lastbeat >= (int64_t)HeartBeatInterval ) {
			time_lastbeat = time_now;
			shakemap_status( TypeHeartBeat, 0, "" );
		}
		if ( stanumber_local < TotalStations ) {
			if ( stanumber_local ) free(slbuffer);
			stanumber_local = TotalStations;
			slbuffer = (SHAKE_LIST_HEADER *)malloc(sizeof(SHAKE_LIST_ELEMENT)*stanumber_local + sizeof(SHAKE_LIST_HEADER));
			shakemap_msgqueue_reinit( QueueSize, sizeof(SHAKE_LIST_ELEMENT)*stanumber_local + sizeof(SHAKE_LIST_HEADER) );
		}

		if ( ShakingMapStatus != THREAD_ALIVE && Finish ) {
			if ( StartThread( thread_shakingmap, (unsigned)THREAD_STACK, &tid ) == -1 ) {
				logit( "e", "shakemap: Error starting thread(thread_shakingmap); exiting!\n");
				shakemap_end();
				exit(-1);
			}
			ShakingMapStatus = THREAD_ALIVE;
		}

	/* Process all new messages */
		do {
		/* See if a termination has been requested */
			res = tport_getflag(&PeakRegion);
			if ( res == TERMINATE || res == MyPid ) {
			/* write a termination msg to log file */
				logit("t", "shakemap: Termination requested; exiting!\n");
				fflush(stdout);
			/* */
				goto exit_procedure;
			}

		/* Get msg & check the return code from transport */
			res = tport_getmsg(&TrigRegion, Getlogo, nLogo, &reclogo, &msgSize, (char *)&tlist.msg, MAX_TRIGLIST_SIZ);
		/* */
			if ( res == GET_OK ) {
			/* Check the received message is the trigger information */
				if ( reclogo.type == TypeTrigList ) {
				/* Check the received message is what we set in the configfile */
					if ( tlist.tlh.trigtype == TriggerAlgType ) {
						TRIG_STATION *tsta = (TRIG_STATION *)(&tlist.tlh + 1);
						TRIG_STATION *tstaend = tsta + tlist.tlh.trigstations;

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
				} /* reclogo.type == TypeTrigList */
			} /* res == GET_OK */
		/* */
			if ( trigseccount > 0 && (time(&time_now) - timeLastTrigger) >= (int)TRIGGER_MIN_INT ) {
			/* */
				shakemap_tlist_time_sync( timeLastTrigger );
				shakemap_tlist_pvalue_update();
			/* */
				if ( (msgSize = shakemap_tlist_pack(slbuffer, sizeof(SHAKE_LIST_ELEMENT)*stanumber_local + sizeof(SHAKE_LIST_HEADER))) > 0 ) {
					slbuffer->trigtype = TriggerAlgType;
					slbuffer->endtime  = timeLastTrigger;
					timeLastTrigger   += (int)TRIGGER_MIN_INT;

					if ( slbuffer->starttime == 0 ) slbuffer->starttime = slbuffer->endtime;
					if ( (trigseccount -= (int)TRIGGER_MIN_INT) == 0 ) slbuffer->codaflag = IS_CODA;
					else slbuffer->codaflag = NO_CODA;

					res = shakemap_msgqueue_enqueue( slbuffer, msgSize );

					/* PostSemaphore(); */ /* Obsoleted by Earthworm */
					PostSpecificSemaphore_ew( SemaPtr );

					if ( res ) {
						if ( res == -2 ) {  /* Serious: quit */
						/* Currently, eneueue() in mem_circ_queue.c never returns this error. */
							sprintf(Text, "internal queue error. Terminating.");
							shakemap_status( TypeError, ERR_QUEUE, Text );
							shakemap_end();
							exit(-1);
						}
						else if ( res == -1 ) {
							sprintf(Text, "queue cannot allocate memory. Lost message.");
							shakemap_status( TypeError, ERR_QUEUE, Text );
						}
						else if ( res == -3 ) {
						/*
							Queue is lapped too often to be logged to screen.
							Log circular queue laps to logfile.
							Maybe queue laps should not be logged at all.
						*/
							logit("et", "shakemap: Circular queue lapped. Messages lost!\n");
						}
					}

					if ( slbuffer->codaflag == IS_CODA ) {
						shakemap_tlist_destroy();
						memset(slbuffer, 0, sizeof(SHAKE_LIST_ELEMENT)*stanumber_local + sizeof(SHAKE_LIST_HEADER));
					}
				}
				else {
					logit( "e","shakemap: Error packing the trigger list to shake list.\n" );
				}
			}

		/* Get msg & check the return code from transport */
			res = tport_getmsg(&PeakRegion, Getlogo, nLogo, &reclogo, &msgSize, (char *)&tracepv, TRACE_PEAKVALUE_SIZE);
		/* no more new messages     */
			if ( res == GET_NONE ) {
				PostSpecificSemaphore_ew(SemaPtr);
				break;
			}
		/* next message was too big */
			else if ( res == GET_TOOBIG ) {
			/* complain and try again   */
				sprintf(
					Text, "Retrieved msg[%ld] (i%u m%u t%u) too big for Buffer[%ld]",
					msgSize, reclogo.instid, reclogo.mod, reclogo.type, (long)TRACE_PEAKVALUE_SIZE
				);
				shakemap_status( TypeError, ERR_TOOBIG, Text );
				continue;
			}
		/* got a msg, but missed some */
			else if ( res == GET_MISS ) {
				sprintf(
					Text, "Missed msg(s)  i%u m%u t%u  %s.", reclogo.instid, reclogo.mod, reclogo.type, PeakRingName
				);
				shakemap_status( TypeError, ERR_MISSMSG, Text );
			}
		/* got a msg, but can't tell */
			else if ( res == GET_NOTRACK ) {
			/* if any were missed        */
				sprintf(
					Text, "Msg received (i%u m%u t%u); transport.h NTRACK_GET exceeded",
					reclogo.instid, reclogo.mod, reclogo.type
				);
				shakemap_status( TypeError, ERR_NOTRACK, Text );
			}

		/* Process the message */
			if ( reclogo.type == TypeTracePeak ) {
				if ( tracepv.recordtype == PeakValueType ) {
				/* Debug */
					//printf("shakemap: Got a new trace peak from %s.%s.%s.%s!\n",
					//tracepv.sta, tracepv.chan, tracepv.net, tracepv.loc); */
					if ( (staptr = shakemap_list_find( tracepv.sta, tracepv.net, tracepv.loc )) == NULL ) {
					/* Not found in trace table */
						//printf("shakemap: %s.%s.%s.%s not found in station table, maybe it's a new station.\n",
						//tracepv.sta, tracepv.chan, tracepv.net, tracepv.loc); */
						continue;
					}
				/* Insert the triggered station pointer to the trigger list */
					shakeptr = &staptr->shakeinfo[staptr->shakelatest];
					tracepv.peakvalue = fabs(tracepv.peakvalue);
				/* Checking the peak time is newer than the last one */
					if ( tracepv.peaktime > shakeptr->peaktime ) {
						staptr->shakelatest++;
						staptr->shakelatest %= (int)SHAKE_BUF_LEN;
						shakeptr = &staptr->shakeinfo[staptr->shakelatest];
					}
					else if ( tracepv.peakvalue < shakeptr->peakvalue ) {
						continue;
					}
				/* */
					shakeptr->recordtype = tracepv.recordtype;
					shakeptr->peakvalue  = tracepv.peakvalue;
					shakeptr->peaktime   = tracepv.peaktime;
					memcpy(shakeptr->peakchan, tracepv.chan, TRACE2_CHAN_LEN);
				}
			}
		} while ( 1 );  /* end of message-processing-loop */
		sleep_ew(50);  /* no more messages; wait for new ones to arrive */
	}
/*-----------------------------end of main loop-------------------------------*/
exit_procedure:
	Finish = 0;
	PostSpecificSemaphore_ew(SemaPtr);
/* detach from shared memory */
	sleep_ew(500);
	shakemap_end();
	KillThread(tid);

	ew_unlockfile(lockfile_fd);
	ew_unlink_lockfile(lockfile);

	return 0;
}

/*
 * shakemap_config() - processes command file(s) using kom.c functions;
 *                     exits if any errors are encountered.
 */
static void shakemap_config( char *configfile )
{
	char  init[16];     /* init flags, one byte for each required command */
	char *com;
	char *str;

	int ncommand;     /* # of required commands you expect to process   */
	int nmiss;        /* number of required commands that were missed   */
	int nfiles;
	int success;
	int i;

/* Set to zero one init flag for each required command */
	ncommand = 16;
	for( i = 0; i < ncommand; i++ ) {
		if ( i < 11 )
			init[i] = 0;
		else
			init[i] = 1;
	}
/* Open the main configuration file */
	nfiles = k_open(configfile);
	if ( nfiles == 0 ) {
		logit("e", "shakemap: Error opening command file <%s>; exiting!\n", configfile);
		exit(-1);
	}

/*
 * Process all command files
 * While there are command files open
 */
	while ( nfiles > 0 ) {
	/* Read next line from active file  */
		while ( k_rd() ) {
		/* Get the first token from line */
			com = k_str();
		/* Ignore blank lines & comments */
			if ( !com )
				continue;
			if ( com[0] == '#' )
				continue;

		/* Open a nested configuration file */
			if ( com[0] == '@' ) {
				success = nfiles+1;
				nfiles  = k_open(&com[1]);
				if ( nfiles != success ) {
					logit("e", "shakemap: Error opening command file <%s>; exiting!\n", &com[1]);
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
			else if( k_its("PeakRing") ) {
				str = k_str();
				if ( str )
					strcpy( PeakRingName, str );
				init[2] = 1;
			}
		/* 3 */
			else if( k_its("TrigRing") ) {
				str = k_str();
				if ( str )
					strcpy( TrigRingName, str );
				init[3] = 1;
			}
			else if ( k_its("OutputRing") ) {
				str = k_str();
				if ( str )
					strcpy( OutRingName, str );
				OutputMapToRingSwitch = 1;
			}
		/* 4 */
			else if ( k_its("HeartBeatInterval") ) {
				HeartBeatInterval = k_long();
				init[4] = 1;
			}
			else if ( k_its("QueueSize") ) {
				QueueSize = k_long();
				logit("o", "shakemap: Queue size change to %ld (default is 10)!\n", QueueSize);
			}
		/* 5 */
			else if ( k_its("TriggerAlgType") ) {
				TriggerAlgType = k_int();
				logit("o", "shakemap: Trigger datatype set to %d!\n", TriggerAlgType);
				init[5] = 1;
			}
		/* 6 */
			else if ( k_its("PeakValueType") ) {
				str = k_str();
				PeakValueType = typestr2num( str );
				logit("o", "shakemap: Peak value type set to %s:%d!\n", str, PeakValueType);
				init[6] = 1;
			}
			else if ( k_its("TriggerDuration") ) {
				TriggerDuration = k_int();
				logit("o", "shakemap: Trigger duration change to %ld sec (default is 30 sec)!\n", TriggerDuration);
			}
			else if ( k_its("InterpolateDistance") ) {
				InterpolateDistance = k_val();
				logit("o", "shakemap: Cluster max distance change to %.1lf km (default is 30.0 km)!\n", InterpolateDistance);
			}
		/* 8 */
			else if ( k_its("ReportPath") ) {
				str = k_str();
				if ( str ) {
					if ( strlen(str) < MAX_PATH_STR ) {
						strcpy(ReportPath, str);
						if ( ReportPath[strlen(ReportPath) - 1] != '/' ) {
							strcat(ReportPath, "/");
						}
						logit("o", "shakemap: Output reports to %s\n", ReportPath);
					}
					else {
						logit("e", "shakemap: The length of report path is over the limit(%d)!\n", MAX_PATH_STR);
						exit(-1);
					}
				}
				init[8] = 1;
			}
			else if ( k_its("MapBoundFile") ) {
				if ( (str = k_str()) ) {
					if ( polyline_read( &BoundPolyLine, str ) ) {
						logit("e", "shakemap: Reading boundary file(%s) error. Exiting!\n", str);
						exit(-1);
					}
				}
			}
			else if ( k_its("SQLHost") ) {
				str = k_str();
				if ( str )
					strcpy(DBInfo.host, str);
#if defined( _USE_SQL )
				for ( i = 11; i < ncommand; i++ )
					init[i] = 0;
#endif
			}
		/* 11 */
			else if ( k_its("SQLPort") ) {
				DBInfo.port = k_long();
				init[11] = 1;
			}
		/* 12 */
			else if ( k_its("SQLUser") ) {
				str = k_str();
				if ( str )
					strcpy(DBInfo.user, str);
				init[12] = 1;
			}
		/* 13 */
			else if ( k_its("SQLPassword") ) {
				str = k_str();
				if ( str )
					strcpy(DBInfo.password, str);
				init[13] = 1;
			}
		/* 14 */
			else if ( k_its("SQLDatabase") ) {
				str = k_str();
				if ( str )
					strcpy(DBInfo.database, str);
				init[14] = 1;
			}
		/* 15 */
			else if ( k_its("SQLStationTable") ) {
				if ( nList >= MAXLIST ) {
					logit("e", "shakemap: Too many <SQLStationTable> commands in <%s>", configfile);
					logit("e", "; max=%d; exiting!\n", (int)MAXLIST);
					exit(-1);
				}
				if ( (str = k_str()) )
					strcpy(SQLStationTable[nList], str);
				nList++;
				init[15] = 1;
			}
			else if ( k_its("Station") ) {
				str = k_get();
				for ( str += strlen(str) + 1; isspace(*str); str++ );
				if ( shakemap_list_sta_line_parse( str, SHAKEMAP_LIST_INITIALIZING ) ) {
					logit(
						"e", "shakemap: ERROR, lack of some stations information for in <%s>. Exiting!\n",
						configfile
					);
					exit(-1);
				}
			}
		/* Enter installation & module to get event messages from */
		/* 10 */
			else if ( k_its("GetEventsFrom") ) {
				if ( (nLogo + 1) >= MAXLOGO ) {
					logit("e", "shakemap: Too many <GetEventsFrom> commands in <%s>", configfile);
					logit("e", "; max=%d; exiting!\n", (int)MAXLOGO);
					exit(-1);
				}
				if ( (str = k_str()) ) {
					if ( GetInst(str, &Getlogo[nLogo].instid) != 0 ) {
						logit("e", "shakemap: Invalid installation name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				if ( (str = k_str()) ) {
					if ( GetModId(str, &Getlogo[nLogo].mod) != 0 ) {
						logit("e", "shakemap: Invalid module name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				if ( (str = k_str()) ) {
					if ( GetType(str, &Getlogo[nLogo].type) != 0 ) {
						logit("e", "shakemap: Invalid message type name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				nLogo++;
				init[10] = 1;
			}
		 /* Unknown command */
			else {
				logit("e", "shakemap: <%s> Unknown command in <%s>.\n", com, configfile);
				continue;
			}

		/* See if there were any errors processing the command */
			if ( k_err() ) {
			   logit("e", "shakemap: Bad <%s> command in <%s>; exiting!\n", com, configfile);
			   exit( -1 );
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
		logit("e", "shakemap: ERROR, no ");
		if ( !init[0] )  logit("e", "<LogFile> "            );
		if ( !init[1] )  logit("e", "<MyModuleId> "         );
		if ( !init[2] )  logit("e", "<PeakRing> "           );
		if ( !init[3] )  logit("e", "<TrigRing> "           );
		if ( !init[4] )  logit("e", "<HeartBeatInterval> "  );
		if ( !init[5] )  logit("e", "<TriggerAlgType> "     );
		if ( !init[6] )  logit("e", "<PeakValueType> "      );
		if ( !init[8] )  logit("e", "<ReportPath> "         );
		if ( !init[10] ) logit("e", "any <GetEventsFrom> "  );
		if ( !init[11] ) logit("e", "<SQLPort> "            );
		if ( !init[12] ) logit("e", "<SQLUser> "            );
		if ( !init[13] ) logit("e", "<SQLPassword> "        );
		if ( !init[14] ) logit("e", "<SQLDatabase> "        );
		if ( !init[15] ) logit("e", "any <SQLStationTable> ");

		logit("e", "command(s) in <%s>; exiting!\n", configfile);
		exit(-1);
	}

	return;
}

/*
 * shakemap_lookup() - Look up important info from earthworm.h tables
 */
static void shakemap_lookup( void )
{
/* Look up keys to shared memory regions */
	if ( (PeakRingKey = GetKey(PeakRingName)) == -1 ) {
		fprintf(stderr, "shakemap:  Invalid ring name <%s>; exiting!\n", PeakRingName);
		exit(-1);
	}
	if ( (TrigRingKey = GetKey(TrigRingName)) == -1 ) {
		fprintf(stderr, "shakemap:  Invalid ring name <%s>; exiting!\n", TrigRingName);
		exit(-1);
	}
	if ( OutputMapToRingSwitch && (OutRingKey = GetKey(OutRingName)) == -1 ) {
		fprintf(stderr, "shakemap:  Invalid ring name <%s>; exiting!\n", OutRingName);
		exit(-1);
	}
/* Look up installations of interest */
	if ( GetLocalInst(&InstId) != 0 ) {
		fprintf(stderr, "shakemap: error getting local installation id; exiting!\n");
		exit(-1);
	}
/* Look up modules of interest */
	if ( GetModId(MyModName, &MyModId) != 0 ) {
		fprintf(stderr, "shakemap: Invalid module name <%s>; exiting!\n", MyModName);
		exit(-1);
	}
/* Look up message types of interest */
	if ( GetType("TYPE_HEARTBEAT", &TypeHeartBeat) != 0 ) {
		fprintf(stderr, "shakemap: Invalid message type <TYPE_HEARTBEAT>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_ERROR", &TypeError) != 0 ) {
		fprintf(stderr, "shakemap: Invalid message type <TYPE_ERROR>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_TRACEPEAK", &TypeTracePeak) != 0 ) {
		fprintf(stderr, "shakemap: Invalid message type <TYPE_TRACEPEAK>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_TRIGLIST", &TypeTrigList) != 0 ) {
		fprintf(stderr, "shakemap: Invalid message type <TYPE_TRIGLIST>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_GRIDMAP", &TypeGridMap) != 0 ) {
		fprintf(stderr, "shakemap: Invalid message type <TYPE_GRIDMAP>; exiting!\n");
		exit(-1);
	}

	return;
}

/*
 * shakemap_status() - builds a heartbeat or error message & puts it into
 *                      shared memory.  Writes errors to log file & screen.
 */
static void shakemap_status( unsigned char type, short ierr, char *note )
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
		logit("et", "shakemap: %s\n", note);
	}

	size = strlen(msg);  /* don't include the null byte in the message */

/* Write the message to shared memory */
	if ( tport_putmsg(&PeakRegion, &logo, size, msg) != PUT_OK ) {
		if ( type == TypeHeartBeat ) {
			logit("et", "shakemap:  Error sending heartbeat.\n");
		}
		else if ( type == TypeError ) {
			logit("et", "shakemap:  Error sending error:%d.\n", ierr);
		}
	}

	return;
}

/*
 * shakemap_end() - free all the local memory & close socket
 */
static void shakemap_end( void )
{
	Finish = 0;
	sleep_ew( 100 );

	tport_detach( &PeakRegion );
	tport_detach( &TrigRegion );
	if ( OutputMapToRingSwitch )
		tport_detach( &OutRegion );

	shakemap_tlist_destroy();
	shakemap_list_end();
	shakemap_msgqueue_end();
	DestroySpecificSemaphore_ew( SemaPtr );

	return;
}

/******************************************************************************
 * thread_shakingmap() Read the station info from the array <stainfo>. And creates   *
 *              the table of shakemap of Taiwan.                              *
 ******************************************************************************/
static thr_ret thread_shakingmap ( void *dummy )
{
	int      res;
	long     recsize;            /* Size of retrieved message from queue */
	uint64_t stanumber_local = 0;

	SHAKE_LIST_HEADER  *slbuffer  = NULL;
	SHAKE_LIST_ELEMENT *slelement = NULL;
	SHAKE_LIST_ELEMENT *slend     = NULL;
	GRIDMAP_HEADER     *gmbuffer  = NULL;
	GRID_REC           *gridrec   = NULL;
	GRID_REC           *gridend   = NULL;

/* Tell the main thread we're ok
 ********************************/
	ShakingMapStatus = THREAD_ALIVE;

/* Initialization
 *****************/
	gmbuffer = (GRIDMAP_HEADER *)malloc(MAX_GRIDMAP_SIZ);

	do
	{
		/* WaitSemPost(); */ /* Obsoleted by Earthworm */
		WaitSpecificSemaphore_ew( SemaPtr );

	/* Initialization */
		if ( stanumber_local < TotalStations ) {
			if ( stanumber_local ) free(slbuffer);
			stanumber_local = TotalStations;
			slbuffer = (SHAKE_LIST_HEADER *)malloc(sizeof(SHAKE_LIST_ELEMENT)*stanumber_local + sizeof(SHAKE_LIST_HEADER));
		}

		res = shakemap_msgqueue_dequeue( slbuffer, &recsize );

		if ( res == 0 ) {
			int   mm;

			_STAINFO *staptr = NULL;

			uint32_t putsize;
			uint32_t area_25 = 0, area_80 = 0;
			uint32_t area_250 = 0, area_400 = 0;

			double lonmax = 0.0, lonmin = 180.0;
			double latmax = 0.0, latmin = 90.0;
			double tmplat = 0.0, tmplon = 0.0;
			double dissum = 0.0, valsum = 0.0, voverd = 0.0;
			double dists = 0.0;

		/* Fill in the information for grid map message header */
			gmbuffer->evaltype  = EVALUATE_REALSHAKE;
			gmbuffer->valuetype = PeakValueType;
			gmbuffer->starttime = slbuffer->starttime;
			gmbuffer->endtime   = slbuffer->endtime;
			gmbuffer->codaflag  = slbuffer->codaflag;

		/* Find out the range of triggered stations */
			slelement = (SHAKE_LIST_ELEMENT *)(slbuffer + 1);
			slend     = slelement + slbuffer->totalstations;
			gridrec   = (GRID_REC *)(gmbuffer + 1);
			gridend   = gridrec + MAX_GRID_NUM;

			for ( ; slelement < slend; slelement++, gridrec++ ) {
				staptr = slelement->staptr;

				if ( staptr->latitude <= latmin ) latmin = staptr->latitude;
				else if ( staptr->latitude >= latmax ) latmax = staptr->latitude;

				if ( staptr->longitude <= lonmin ) lonmin = staptr->longitude;
				else if ( staptr->longitude >= lonmax ) lonmax = staptr->longitude;

			/* Fill the stations information into grid map message */
				sprintf(gridrec->gridname, "%s.%s.%s", staptr->sta, staptr->net, staptr->loc);
				gridrec->gridtype  = GRID_STATION;
				gridrec->longitude = staptr->longitude;
				gridrec->latitude  = staptr->latitude;
				gridrec->gridvalue = slelement->shakeinfo.peakvalue;
			}

		/* Now, the total grids in grid map message are equal to the total stations in the trigger list */
			gmbuffer->totalgrids = slbuffer->totalstations;

		/* Setup the grid range */
			latmin -= 0.05;
			lonmin -= 0.05;
			latmax += 0.1;
			lonmax += 0.1;

		/* Calculate the grid data */
		/* Longitude */
			for ( tmplon = lonmin; tmplon <= lonmax; tmplon += 0.05 ) {
			/* Latitude */
				for ( tmplat = latmin; tmplat <= latmax; tmplat += 0.05 ) {
				/* Check if inside the given boundary */
					if ( !BoundPolyLine || polyline_locpt_all((float)tmplon, (float)tmplat, BoundPolyLine, &mm) > -1 ) {
					/* Using the inverse distance weighting method to calculate the value of each grid */
						dissum = 0.0;
						valsum = 0.0;
						voverd = 0.0;

						slelement = (SHAKE_LIST_ELEMENT *)(slbuffer + 1);

						for ( ; slelement<slend; slelement++ ) {
							staptr = slelement->staptr;

							if ( (dists = coor2distf(tmplat, tmplon, staptr->latitude, staptr->longitude)) < InterpolateDistance ) {
							/* If the distance between station and grid is neglectable, just add epsilon (about 10^-6) to the distance */
								if ( dists < DBL_EPSILON ) dists += FLT_EPSILON;

							/* Otherwise, just follow the IDW method */
								dists *= dists;
								valsum += slelement->shakeinfo.peakvalue / dists;
								dissum += 1.0 / dists;
							}
						}

						voverd = valsum/dissum;

					/* Find out the grid with maximum value */
						if( voverd > gmbuffer->centervalue ) {
							gmbuffer->centervalue = voverd;
							gmbuffer->centerlat   = tmplat;
							gmbuffer->centerlon   = tmplon;
						}

					/* If the grid value is PGA, we can also derive the magnitude */
						if ( PeakValueType == RECORD_ACCELERATION ) {
							if ( voverd >= 80.0 ) {
								area_25++;
								if ( voverd >= 110.0 ) {
									area_80++;
									if ( voverd >= 370.0 ) {
										area_250++;
										if ( voverd >= 400.0 ) area_400++;
									}
								}
							}
						}

					/* If the grid value is not zero, save to the grid map message */
						if ( voverd > DBL_EPSILON ) {
							gridrec->gridname[0] = '\0';
							gridrec->gridtype    = GRID_MAPGRID;
							gridrec->longitude   = tmplon;
							gridrec->latitude    = tmplat;
							gridrec->gridvalue   = voverd;

							if ( ++gridrec >= gridend ) {
								logit( "e", "shakemap: Grid number over the maximum, maximum is %d!\n", MAX_GRID_NUM );
								break;
							}

							gmbuffer->totalgrids++;
						}
					}
				} /* Latitude */
			} /* Longitude */

		/* Calculate the estimated magnitude by cover areas */
			area_25  *= 25;
			area_80  *= 25;
			area_250 *= 25;
			area_400 *= 25;
			cal_mag_values( gmbuffer, area_25, area_80, area_250, area_400 );

		/* Output the the whole shakemap data to share ring */
			if ( OutputMapToRingSwitch ) {
				putsize = sizeof(GRIDMAP_HEADER) + sizeof(GRID_REC) * gmbuffer->totalgrids;
				if ( tport_putmsg(&OutRegion, &Putlogo, putsize, (char *)gmbuffer ) != PUT_OK)
					logit("e", "shakemap: Error putting message in region %ld\n", OutRegion.key);
			}

			if ( gmbuffer->starttime == gmbuffer->endtime )
				logit("ot", "New event detected, start generating grid map!\n");
		/* Output this result to the local files in text format */
			output_result_file(
				gmbuffer, slbuffer->totalstations, area_25, area_80, area_250, area_400
			);
		/* */
			memset(gmbuffer, 0, MAX_GRIDMAP_SIZ);
		}
	} while( Finish );

/* we're quitting
 *****************/
	free(gmbuffer);
	free(slbuffer);

	ShakingMapStatus = THREAD_ERR;			/* file a complaint to the main thread */
	KillSelfThread();						/* main thread will restart us */
	return NULL;
}

/*
 *
 */
static void cal_map_grids_data()
{

/* Calculate the grid data */
/* Longitude */
	for ( tmplon = lonmin; tmplon <= lonmax; tmplon += 0.05 ) {
	/* Latitude */
		for ( tmplat = latmin; tmplat <= latmax; tmplat += 0.05 ) {
		/* Check if inside the given boundary */
			if ( locpt((float)tmplon, (float)tmplat, X, Y, BoundPointCount, &mm) > -1 ) {
			/* Using the inverse distance weighting method to calculate the value of each grid */
				dissum = 0.0;
				valsum = 0.0;
				voverd = 0.0;

				slelement = (SHAKE_LIST_ELEMENT *)(slbuffer + 1);

				for ( ; slelement<slend; slelement++ ) {
					staptr = slelement->staptr;

					if ( (dists = coor2distf(tmplat, tmplon, staptr->latitude, staptr->longitude)) < InterpolateDistance ) {
					/* If the distance between station and grid is neglectable, just add epsilon (about 10^-6) to the distance */
						if ( dists < DBL_EPSILON )
							dists += FLT_EPSILON;
					/* Otherwise, just follow the IDW method */
						dists *= dists;
						valsum += slelement->shakeinfo.peakvalue / dists;
						dissum += 1.0 / dists;
					}
				}

				voverd = valsum/dissum;

			/* Find out the grid with maximum value */
				if( voverd > gmbuffer->centervalue ) {
					gmbuffer->centervalue = voverd;
					gmbuffer->centerlat   = tmplat;
					gmbuffer->centerlon   = tmplon;
				}

			/* If the grid value is PGA, we can also derive the magnitude */
				if ( PeakValueType == RECORD_ACCELERATION ) {
					if ( voverd >= 80.0 ) {
						area_25++;
						if ( voverd >= 110.0 ) {
							area_80++;
							if ( voverd >= 370.0 ) {
								area_250++;
								if ( voverd >= 400.0 ) area_400++;
							}
						}
					}
				}

			/* If the grid value is not zero, save to the grid map message */
				if ( voverd > DBL_EPSILON ) {
					gridrec->gridname[0] = '\0';
					gridrec->gridtype    = GRID_MAPGRID;
					gridrec->longitude   = tmplon;
					gridrec->latitude    = tmplat;
					gridrec->gridvalue   = voverd;

					if ( ++gridrec >= gridend ) {
						logit( "e", "shakemap: Grid number over the maximum, maximum is %d!\n", MAX_GRID_NUM );
						break;
					}

					gmbuffer->totalgrids++;
				}
			}
		} /* Latitude */
	} /* Longitude */

	return;
}

/*
 *
 */
static void cal_mag_values(
	GRIDMAP_HEADER *gmh, const uint32_t area_25, const uint32_t area_80,
	const uint32_t area_250, const uint32_t area_400
) {
/* Calculate the estimated magnitude */
	if ( area_25 )
		gmh->magnitude[0] = (0.002248 * 80 + 0.279229) * log10(area_25) + 4.236343;
		/* gmh->magnitude[0] = (0.001949 * 80 + 0.271711) * log10(area_25*25) + 4.455552; */
	if ( area_80 )
		gmh->magnitude[1] = (0.002248 * 110 + 0.279229) * log10(area_80) + 4.236343;
		/* gmh->magnitude[1] = (0.001949 * 110 + 0.271711) * log10(area_80*25) + 4.455552; */
	if ( area_250 )
		gmh->magnitude[2] = (0.002248 * 370 + 0.279229) * log10(area_250) + 4.236343;
		/* gmh->magnitude[2] = (0.001949 * 370 + 0.271711) * log10(area_250*25) + 4.455552; */
	if ( area_400 )
		gmh->magnitude[3] = (0.002248 * 400 + 0.279229) * log10(area_400) + 4.236343;
		/* gmh->magnitude[3] = (0.001949 * 400 + 0.271711) * log10(area_400*25) + 4.455552; */

	return;
}

/*
 *
 */
static void output_result_file(
	const GRIDMAP_HEADER *gmh, const uint32_t totalstations, const uint32_t area_25,
	const uint32_t area_80, const uint32_t area_250, const uint32_t area_400
) {
	FILE      *fd;
	struct tm *tp = gmtime(&gmh->endtime);

	char      filename[MAX_PATH_STR] = { 0 };
	char      datetime[MAX_DSTR_LENGTH] = { 0 };
	char      rsec[3] = { 0 };
	GRID_REC *gridrec = NULL;
	GRID_REC *gridend = NULL;

/* Generate the report file name by the present time */
	date2spstring( tp, datetime, sizeof(datetime) );
/* We only use the datetime string like YYYYMMDDHHMM, therefore drop the second part */
	datetime[12] = '\0';
/* Generate the second string */
	sprintf(rsec, "%02d", tp->tm_sec);

/* Output the triggered stations table */
	sprintf(filename, "%s%s_%s_sta.txt", ReportPath, datetime, typenum2str( gmh->valuetype ));
	fd  = fopen(filename, "a");
	gridrec = (GRID_REC *)(gmh + 1);
	gridend = gridrec + gmh->totalgrids;
	for ( ; gridrec->gridtype == GRID_STATION && gridrec < gridend; gridrec++ ) {
		fprintf(
			fd, "%s %6.2f %6.2f %6.2f %s\n",
			gridrec->gridname, gridrec->longitude, gridrec->latitude, gridrec->gridvalue, rsec
		);
	}
	fclose(fd);

/* Check if the grid record type is GRID_MAPGRID */
	for ( ; gridrec->gridtype != GRID_MAPGRID && gridrec < gridend; gridrec++ );
	if ( gridrec >= gridend ) {
		logit( "t", "shakemap: There is not any map grid data!\n" );
	}
	else {
	/* Output the peak value data of triggered grid */
		sprintf(filename, "%s%s_%s.txt", ReportPath, datetime, typenum2str( gmh->valuetype ));
		fd = fopen(filename, "a");
		for ( ; gridrec->gridtype == GRID_MAPGRID && gridrec < gridend; gridrec++ ) {
			fprintf(
				fd, "%6.2f %6.2f %8.3f %s\n",
				gridrec->longitude, gridrec->latitude, gridrec->gridvalue, rsec
			);
		}
		fclose(fd);
	}

/* Output the estimated magnitude record */
	if ( area_25 ) {
		sprintf(filename, "%s%s_mag.txt", ReportPath, datetime);
		fd = fopen(filename, "a");
		fprintf(
			fd, "%4.1f %4.1f %4.1f %4.1f %4d %4d %4d %4d %3d %6.2f %5.2f %6.2f %s\n",
			gmh->magnitude[0], gmh->magnitude[1], gmh->magnitude[2], gmh->magnitude[3],
			area_25, area_80, area_250, area_400, totalstations,
			gmh->centerlon, gmh->centerlat, gmh->centervalue, rsec
		);
		fclose(fd);
	}

	return;
}
