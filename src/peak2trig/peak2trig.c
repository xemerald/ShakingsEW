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
#include <time.h>
/* Earthworm environment header include */
#include <earthworm.h>
#include <kom.h>
#include <transport.h>
#include <lockfile.h>
/* Local header include */
#include <dbinfo.h>
#include <recordtype.h>
#include <tracepeak.h>
#include <triglist.h>
#include <peak2trig.h>
#include <peak2trig_list.h>
#include <peak2trig_triglist.h>

/* Functions prototype in this source file */
static void peak2trig_config( char * );
static void peak2trig_lookup( void );
static void peak2trig_status( unsigned char, short, char * );
static void peak2trig_end( void );                /* Free all the local memory & close socket */

static void update_list ( void * );
static int  update_list_configfile( char * );

/* Ring messages things */
static SHM_INFO InRegion;      /* shared memory region to use for i/o    */
static SHM_INFO OutRegion;     /* shared memory region to use for i/o    */

#define MAXLOGO  5
static MSG_LOGO Putlogo;           /* array for output module, type, instid     */
static MSG_LOGO Getlogo[MAXLOGO];  /* array for requesting module, type, instid */
static pid_t    MyPid;             /* for restarts by startstop                 */

/* */
#define MAXLIST  5
/* Things to read or derive from configuration file */
static char     InRingName[MAX_RING_STR];   /* name of transport ring for i/o    */
static char     OutRingName[MAX_RING_STR];  /* name of transport ring for i/o    */
static char     MyModName[MAX_MOD_STR];     /* speak as this module name/id      */
static uint8_t  LogSwitch;                  /* 0 if no logfile should be written */
static uint64_t HeartBeatInterval;          /* seconds between heartbeats        */
static uint64_t UpdateInterval = 0;         /* seconds between updating check    */
static uint16_t nLogo = 0;
static uint16_t TriggerTimeInterval = 0;
static uint16_t RecordTypeToTrig    = 0;
static uint16_t TriggerStations     = 3;
static double   PeakThreshold       = 1.5;
static double   PeakDuration        = 6.0;
static double   ClusterDistance     = 60.0;
static double   ClusterTimeGap      = 5.0;
static DBINFO   DBInfo;
static char     SQLStationTable[MAXLIST][MAX_TABLE_LEGTH];
static uint16_t nList = 0;

/* Things to look up in the earthworm.h tables with getutil.c functions */
static int64_t InRingKey;       /* key of transport ring for i/o     */
static int64_t OutRingKey;      /* key of transport ring for i/o     */
static uint8_t InstId;          /* local installation id             */
static uint8_t MyModId;         /* Module Id for this program        */
static uint8_t TypeHeartBeat;
static uint8_t TypeError;
static uint8_t TypeTracePeak = 0;
static uint8_t TypeTrigList = 0;

/* Error messages used by peak2trig */
#define  ERR_MISSMSG       0   /* message missed in transport ring       */
#define  ERR_TOOBIG        1   /* retreived msg too large for buffer     */
#define  ERR_NOTRACK       2   /* msg retreived; tracking limit exceeded */
#define  ERR_QUEUE         3   /* error queueing message for sending      */
static char Text[150];         /* string for log/error messages          */

/* Station list update status flag
 *********************************/
#define  LIST_IS_UPDATED      0
#define  LIST_NEED_UPDATED    1
#define  LIST_UNDER_UPDATE    2

static volatile uint8_t UpdateStatus = LIST_IS_UPDATED;

/*
 *
 */
int main ( int argc, char **argv )
{
	int      i;
	int      res;
	_Bool    evflag = FALSE;
	int64_t  msg_size = 0;
	MSG_LOGO reclogo;
	time_t   time_now;          /* current time                    */
	time_t   time_lastbeat;     /* time last heartbeat was sent    */
	time_t   time_lasttrig;     /* time last trigger list was sent */
	time_t   time_lastupd;   /* time last updated stations list */
	char    *lockfile;
	int32_t  lockfile_fd;

	_STAINFO       *staptr    = NULL;
	STA_NODE       *targetsta = NULL;
	TRACE_PEAKVALUE tracepv;
	TrigListPacket  obuffer;

/* Check command line arguments */
	if ( argc != 2 ) {
		fprintf( stderr, "Usage: peak2trig <configfile>\n" );
		exit( 0 );
	}
/* Initialize name of log-file & open it */
	logit_init( argv[1], 0, 256, 1 );
/* Read the configuration file(s) */
	peak2trig_config( argv[1] );
	logit("" , "%s: Read command file <%s>\n", argv[0], argv[1]);
/* Read the channels list from remote database */
	for ( i = 0; i < nList; i++ ) {
		if ( peak2trig_list_db_fetch( SQLStationTable[i], &DBInfo, PEAK2TRIG_LIST_INITIALIZING ) < 0 ) {
			fprintf(stderr, "Something error when fetching stations list %s. Exiting!\n", SQLStationTable[i]);
			exit(-1);
		}
	}
/* Checking total station number again */
	if ( !(i = peak2trig_list_total_station_get()) ) {
		fprintf(stderr, "There is not any station in the list after fetching. Exiting!\n");
		exit(-1);
	}
	else {
		logit("o", "peak2trig: There are total %d station(s) in the list.\n", i);
		peak2trig_list_tree_activate();
	}
/* Look up important info from earthworm.h tables */
	peak2trig_lookup();
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
		logit("e","peak2trig: Cannot get pid. Exiting.\n");
		exit(-1);
	}
/* Build the message */
	Putlogo.instid = InstId;
	Putlogo.mod    = MyModId;
	Putlogo.type   = TypeTrigList;
/* Attach to Input/Output shared memory ring */
	tport_attach( &InRegion, InRingKey );
	logit("", "peak2trig: Attached to public memory region %s: %ld\n", InRingName, InRingKey);
/* Flush the transport ring */
	tport_flush( &InRegion, Getlogo, nLogo, &reclogo );
/* */
	tport_attach( &OutRegion, OutRingKey );
	logit("", "peak2trig: Attached to public memory region %s: %ld\n", OutRingName, OutRingKey);

/* Initialize the triggered type */
	memset(obuffer.msg, 0, MAX_TRIGLIST_SIZ);
/* Force a heartbeat to be issued in first pass thru main loop */
	time_lastupd = time_lastbeat = time(&time_now) - HeartBeatInterval - 1;
/* Initialize the time stamp for trigger */
	time_lasttrig = time_now - TriggerTimeInterval - 1;
/*----------------------- setup done; start main loop -------------------------*/
	while ( 1 ) {
	/* Send peak2trig's heartbeat */
		if  ( time(&time_now) - time_lastbeat >= (int64_t)HeartBeatInterval ) {
			time_lastbeat = time_now;
			peak2trig_status( TypeHeartBeat, 0, "" );
		}
	/* */
		if (
			UpdateInterval &&
			UpdateStatus == LIST_NEED_UPDATED &&
			(time_now - time_lastupd) >= (int64_t)UpdateInterval
		) {
			time_lastupd = time_now;
			update_list( argv[1] );
		}

	/* Process all new messages */
		do {
		/* See if a termination has been requested */
			res = tport_getflag(&InRegion);
			if ( res == TERMINATE || res == MyPid ) {
			/* write a termination msg to log file */
				logit("t", "peak2trig: Termination requested; exiting!\n");
				fflush(stdout);
			/* */
				goto exit_procedure;
			}
		/* Get msg & check the return code from transport */
			res = tport_getmsg(&InRegion, Getlogo, nLogo, &reclogo, &msg_size, (char *)&tracepv, TRACE_PEAKVALUE_SIZE);
		/* No more new messages     */
			if ( res == GET_NONE ) {
				break;
			}
		/* Next message was too big */
			else if ( res == GET_TOOBIG ) {
			/* Complain and try again   */
				sprintf(
					Text, "Retrieved msg[%ld] (i%u m%u t%u) too big for Buffer[%ld]",
					msg_size, reclogo.instid, reclogo.mod, reclogo.type, (long)TRACE_PEAKVALUE_SIZE
				);
				peak2trig_status( TypeError, ERR_TOOBIG, Text );
				continue;
			}
		/* Got a msg, but missed some */
			else if ( res == GET_MISS ) {
				sprintf(
					Text, "Missed msg(s)  i%u m%u t%u  %s.", reclogo.instid, reclogo.mod, reclogo.type, InRingName
				);
				peak2trig_status( TypeError, ERR_MISSMSG, Text );
			}
		/* Got a msg, but can't tell */
			else if ( res == GET_NOTRACK ) {
			/* If any were missed        */
				sprintf(
					Text, "Msg received (i%u m%u t%u); transport.h NTRACK_GET exceeded",
					reclogo.instid, reclogo.mod, reclogo.type
				);
				peak2trig_status( TypeError, ERR_NOTRACK, Text );
			}

		/* Process the message */
			if ( reclogo.type == TypeTracePeak ) {
				if ( tracepv.recordtype == RecordTypeToTrig ) {
					if ( (tracepv.peakvalue = fabs(tracepv.peakvalue)) >= PeakThreshold ) {
					/* Debug */
						//printf("peak2trig: Got a new trace peak from %s.%s.%s.%s!\n",
						//tracepv.sta, tracepv.chan, tracepv.net, tracepv.loc);

						if ( (staptr = peak2trig_list_find( &tracepv )) == NULL ) {
						/* Not found in trace table */
							//printf("peak2trig: %s.%s.%s.%s not found in station table, maybe it's a new station.\n",
							//tracepv.sta, tracepv.chan, tracepv.net, tracepv.loc);
						/* Force to update the table */
							if ( UpdateStatus == LIST_IS_UPDATED )
								UpdateStatus = LIST_NEED_UPDATED;
							continue;
						}
					/* Insert the triggered station pointer to the trigger list */
						if ( (targetsta = peak2trig_tlist_insert( staptr )) == NULL ) {
							logit( "e","peak2trig: Error insert station %s into trigger list.\n", staptr->sta );
							continue;
						}

						peak2trig_tlist_update( &tracepv, targetsta );

						if ( (time(&time_now) - time_lasttrig) >= TriggerTimeInterval ) {
						/* Check if the length of trigger list is larger than cluster threshold */
							if ( peak2trig_tlist_len_get() > (int)CLUSTER_NUM ) {
							/* Do the cluster with the input condition(ex: distance & time gap) */
								if ( peak2trig_tlist_cluster( ClusterDistance, ClusterTimeGap ) >= TriggerStations ) {
								/* The triggered stations is over the threshold and issue the triggered message */
									msg_size =
										peak2trig_tlist_pack(
											obuffer.msg, MAX_TRIGLIST_SIZ,
											evflag ?  PEAK2TRIG_FOLLOW_TRIG : PEAK2TRIG_FIRST_TRIG
										);
									if ( msg_size > 0 ) {
									/* Since there is still new station detecting peak value higher than threshold, the event should be undergoing */
										obuffer.tlh.codaflag = NO_CODA;
										obuffer.tlh.trigtype = TRIG_BY_PEAK_CLUSTER;
									/* */
										if ( evflag ) {
										/* Following detection */
											logit(
												"ot", "peak2trig: During detected possible event last for %.1lfs, total triggered stations for now are %d!\n",
												obuffer.tlh.trigtime - obuffer.tlh.origintime, obuffer.tlh.trigstations
											);
										}
										else {
										/* First detection */
											logit("ot", "peak2trig: First detected possible event!! Total triggered stations are %d!\n", obuffer.tlh.trigstations);
											evflag = TRUE;
										}
									/* */
										if ( tport_putmsg(&OutRegion, &Putlogo, msg_size, (char *)obuffer.msg) != PUT_OK )
											logit("e", "peak2trig: Error putting message in region %ld\n", OutRegion.key);
									}
									time_lasttrig = time_now;
								}
							}
						}
					}  /* tracepv.peakvalue >= PeakThreshold */
				}  /* tracepv.recordtype == RecordTypeToTrig */
			}  /* reclogo.type == TypeTracePeak */

			if ( evflag && (time(&time_now) - time_lasttrig) > PeakDuration ) {
			/* Coda? */
				logit(
					"ot", "peak2trig: End of possible event! Event duration might be %.1lfs!\n",
					obuffer.tlh.trigtime - obuffer.tlh.origintime
				);
				evflag = FALSE;
			/*
			 * Otherwise there is still new station detecting peak value higher than threshold,
			 * the event should be undergoing
			 */
				obuffer.tlh.codaflag = IS_CODA;
				obuffer.tlh.trigtype = TRIG_BY_PEAK_CLUSTER;
				msg_size = TRIGLIST_SIZE_GET( &obuffer.tlh );
				if ( tport_putmsg(&OutRegion, &Putlogo, msg_size, (char *)obuffer.msg) != PUT_OK )
					logit("e", "peak2trig: Error putting message in region %ld\n", OutRegion.key);
			/* Reset the output buffer for the next event */
				memset(obuffer.msg, 0, MAX_TRIGLIST_SIZ);
			}
		} while ( 1 );  /* end of message-processing-loop */
	/* Do the filter to remove the stations which are obsolete */
		peak2trig_tlist_time_filter( PeakDuration );
		sleep_ew( 50 );  /* no more messages; wait for new ones to arrive */
	}
/*-----------------------------end of main loop-------------------------------*/
exit_procedure:
	peak2trig_end();
	ew_unlockfile(lockfile_fd);
	ew_unlink_lockfile(lockfile);

	return 0;
}

/*
 * peak2trig_config() - processes command file(s) using kom.c functions;
 *                      exits if any errors are encountered.
 */
static void peak2trig_config( char *configfile )
{
	char  init[13];     /* init flags, one byte for each required command */
	char *com;
	char *str;

	uint32_t ncommand;     /* # of required commands you expect to process   */
	uint32_t nmiss;        /* number of required commands that were missed   */
	uint32_t nfiles;
	uint32_t success;
	uint32_t i;

/* Set to zero one init flag for each required command */
	ncommand = 13;
	for( i = 0; i < ncommand; i++ ) {
		if ( i < 8 )
			init[i] = 0;
		else
			init[i] = 1;
	}
/* Open the main configuration file */
	nfiles = k_open(configfile);
	if ( nfiles == 0 ) {
		logit("e", "peak2trig: Error opening command file <%s>; exiting!\n", configfile);
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
   					logit("e", "peak2trig: Error opening command file <%s>; exiting!\n", &com[1]);
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
					strcpy(InRingName, str);
				init[2] = 1;
			}
		/* 3 */
			else if ( k_its("OutputRing") ) {
				str = k_str();
				if ( str )
					strcpy( OutRingName, str );
				init[3] = 1;
			}
		/* 4 */
			else if ( k_its("HeartBeatInterval") ) {
				HeartBeatInterval = k_long();
				init[4] = 1;
			}
			else if ( k_its("UpdateInterval") ) {
				UpdateInterval = k_long();
				if ( UpdateInterval )
					logit(
						"o", "peak2trig: Change to auto updating mode, the updating interval is %ld seconds!\n",
						UpdateInterval
					);
			}
			else if ( k_its("TriggerTimeInterval") ) {
				TriggerTimeInterval = k_long();
				logit("o", "peak2trig: Trigger time interval change to %d sec, default is 0 (Real-time mode)!\n", TriggerTimeInterval);
			}
		/* 5 */
			else if ( k_its("RecordTypeToTrig") ) {
				str = k_str();
				RecordTypeToTrig = typestr2num( str );
				logit("o", "peak2trig: Trigger datatype set to %s:%d!\n", str, RecordTypeToTrig);
				init[5] = 1;
			}
			else if ( k_its("TriggerStations") ) {
				TriggerStations = k_int();
				logit("o", "peak2trig: Trigger threshold of stations change to %d, default is 3!\n", TriggerStations);
			}
		/* 6 */
			else if ( k_its("PeakThreshold") ) {
				PeakThreshold = k_val();
				logit("o", "peak2trig: Trigger threshold of peak value set to %.2lf, unit depends on datatype!\n", PeakThreshold);
				init[6] = 1;
			}
			else if ( k_its("PeakDuration") ) {
				PeakDuration = k_val();
				logit("o", "peak2trig: Peak value duration change to %.2lf sec, default is 6.0 sec!\n", PeakDuration);
			}
			else if ( k_its("ClusterDistance") ) {
				ClusterDistance = k_val();
				logit("o", "peak2trig: Cluster max distance change to %.1lf km, default is 60.0 km!\n", ClusterDistance);
			}
			else if ( k_its("ClusterTimeGap") ) {
				ClusterTimeGap = k_val();
				logit("o", "peak2trig: Cluster time gap between each station change to %.2lf sec, default is 5.0 sec!\n", ClusterTimeGap);
			}
			else if ( k_its("SQLHost") ) {
				str = k_str();
				if ( str )
					strcpy(DBInfo.host, str);
#if defined( _USE_SQL )
				for ( i = 8; i < ncommand; i++ )
					init[i] = 0;
#endif
			}
		/* 8 */
			else if ( k_its("SQLPort") ) {
				DBInfo.port = k_long();
				init[8] = 1;
			}
		/* 9 */
			else if ( k_its("SQLUser") ) {
				str = k_str();
				if ( str )
					strcpy(DBInfo.user, str);
				init[9] = 1;
			}
		/* 10 */
			else if ( k_its("SQLPassword") ) {
				str = k_str();
				if ( str )
					strcpy(DBInfo.password, str);
				init[10] = 1;
			}
		/* 11 */
			else if ( k_its("SQLDatabase") ) {
				str = k_str();
				if ( str )
					strcpy(DBInfo.database, str);
				init[11] = 1;
			}
		/* 12 */
			else if ( k_its("SQLStationTable") ) {
				if ( nList >= MAXLIST ) {
					logit("e", "peak2trig: Too many <SQLStationTable> commands in <%s>", configfile);
					logit("e", "; max=%d; exiting!\n", (int)MAXLIST);
					exit(-1);
				}
				if ( (str = k_str()) )
					strcpy(SQLStationTable[nList], str);
				nList++;
				init[12] = 1;
			}
			else if ( k_its("Station") ) {
				str = k_get();
				for ( str += strlen(str) + 1; isspace(*str); str++ );
				if ( peak2trig_list_sta_line_parse( str, PEAK2TRIG_LIST_INITIALIZING ) ) {
					logit(
						"e", "peak2trig: ERROR, lack of some stations information for in <%s>. Exiting!\n",
						configfile
					);
					exit(-1);
				}
			}
		/* Enter installation & module to get event messages from */
		/* 7 */
			else if ( k_its("GetEventsFrom") ) {
				if ( (nLogo + 1) >= MAXLOGO ) {
					logit("e", "peak2trig: Too many <GetEventsFrom> commands in <%s>", configfile);
					logit("e", "; max=%d; exiting!\n", (int)MAXLOGO);
					exit(-1);
				}
				if ( (str = k_str()) ) {
					if ( GetInst(str, &Getlogo[nLogo].instid) != 0 ) {
						logit("e", "peak2trig: Invalid installation name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				if ( (str = k_str()) ) {
					if ( GetModId(str, &Getlogo[nLogo].mod) != 0 ) {
						logit("e", "peak2trig: Invalid module name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				if ( (str = k_str()) ) {
					if ( GetType(str, &Getlogo[nLogo].type) != 0 ) {
						logit("e", "peak2trig: Invalid message type name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				nLogo++;
				init[7] = 1;
			}
		 /* Unknown command */
			else {
				logit("e", "peak2trig: <%s> Unknown command in <%s>.\n", com, configfile);
				continue;
			}

		/* See if there were any errors processing the command */
			if ( k_err() ) {
			   logit("e", "peak2trig: Bad <%s> command in <%s>; exiting!\n", com, configfile);
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
		logit("e", "peak2trig: ERROR, no ");
		if ( !init[0] )  logit("e", "<LogFile> "            );
		if ( !init[1] )  logit("e", "<MyModuleId> "         );
		if ( !init[2] )  logit("e", "<InputRing> "          );
		if ( !init[3] )  logit("e", "<OutputRing> "         );
		if ( !init[4] )  logit("e", "<HeartBeatInterval> "  );
		if ( !init[5] )  logit("e", "<RecordTypeToTrig> "   );
		if ( !init[6] )  logit("e", "<PeakThreshold> "      );
		if ( !init[7] )  logit("e", "any <GetEventsFrom> "  );
		if ( !init[8] )  logit("e", "<SQLPort> "            );
		if ( !init[9] )  logit("e", "<SQLUser> "            );
		if ( !init[10] ) logit("e", "<SQLPassword> "        );
		if ( !init[11] ) logit("e", "<SQLDatabase> "        );
		if ( !init[12] ) logit("e", "any <SQLStationTable> ");

		logit("e", "command(s) in <%s>; exiting!\n", configfile);
		exit(-1);
	}

	return;
}

/*
 * peak2trig_lookup() - Look up important info from earthworm.h tables
 */
static void peak2trig_lookup( void )
{
/* Look up keys to shared memory regions */
	if ( ( InRingKey = GetKey(InRingName) ) == -1 ) {
		fprintf(stderr, "peak2trig:  Invalid ring name <%s>; exiting!\n", InRingName);
		exit(-1);
	}
	if ( ( OutRingKey = GetKey(OutRingName) ) == -1 ) {
		fprintf(stderr, "peak2trig:  Invalid ring name <%s>; exiting!\n", OutRingName);
		exit(-1);
	}
/* Look up installations of interest */
	if ( GetLocalInst(&InstId) != 0 ) {
		fprintf(stderr, "peak2trig: error getting local installation id; exiting!\n");
		exit(-1);
	}
/* Look up modules of interest */
	if ( GetModId(MyModName, &MyModId) != 0 ) {
		fprintf(stderr, "peak2trig: Invalid module name <%s>; exiting!\n", MyModName);
		exit(-1);
	}
/* Look up message types of interest */
	if ( GetType("TYPE_HEARTBEAT", &TypeHeartBeat) != 0 ) {
		fprintf(stderr, "peak2trig: Invalid message type <TYPE_HEARTBEAT>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_ERROR", &TypeError) != 0 ) {
		fprintf(stderr, "peak2trig: Invalid message type <TYPE_ERROR>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_TRACEPEAK", &TypeTracePeak) != 0 ) {
		fprintf(stderr, "peak2trig: Invalid message type <TYPE_TRACEPEAK>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_TRIGLIST", &TypeTrigList) != 0 ) {
		fprintf(stderr, "peak2trig: Invalid message type <TYPE_TRIGLIST>; exiting!\n");
		exit(-1);
	}

	return;
}

/*
 * peak2trig_status() - builds a heartbeat or error message & puts it into
 *                      shared memory.  Writes errors to log file & screen.
 */
static void peak2trig_status( unsigned char type, short ierr, char *note )
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
		logit("et", "peak2trig: %s\n", note);
	}

	size = strlen(msg);  /* don't include the null byte in the message */

/* Write the message to shared memory */
	if ( tport_putmsg(&InRegion, &logo, size, msg) != PUT_OK ) {
		if ( type == TypeHeartBeat ) {
			logit("et", "peak2trig:  Error sending heartbeat.\n");
		}
		else if ( type == TypeError ) {
			logit("et", "peak2trig:  Error sending error:%d.\n", ierr);
		}
	}

	return;
}

/*
 * peak2trig_end() - free all the local memory & close socket
 */
static void peak2trig_end( void )
{
	tport_detach( &InRegion );
	tport_detach( &OutRegion );
	peak2trig_tlist_destroy();
	peak2trig_list_end();

	return;
}

/*
 * update_list() -
 */
static void update_list( void *arg )
{
	int i;
	int update_flag = 0;

	logit("ot", "peak2trig: Updating the channels list...\n");
	UpdateStatus = LIST_UNDER_UPDATE;
/* */
	for ( i = 0; i < nList; i++ ) {
		if ( peak2trig_list_db_fetch( SQLStationTable[i], &DBInfo, PEAK2TRIG_LIST_UPDATING ) < 0 ) {
			logit("e", "peak2trig: Fetching channels list(%s) from remote database error!\n", SQLStationTable[i]);
			update_flag = 1;
		}
	}
/* */
	if ( update_list_configfile( (char *)arg ) ) {
		logit("e", "peak2trig: Fetching channels list from local file error!\n");
		update_flag = 1;
	}
/* */
	if ( update_flag ) {
		peak2trig_list_tree_abandon();
		logit("e", "peak2trig: Failed to update the channels list!\n");
		logit("ot", "peak2trig: Keep using the previous channels list(%ld)!\n", peak2trig_list_timestamp_get());
	}
	else {
		peak2trig_list_tree_activate();
		logit("ot", "peak2trig: Successfully updated the channels list(%ld)!\n", peak2trig_list_timestamp_get());
		logit(
			"ot", "peak2trig: There are total %d channels in the new channels list.\n", peak2trig_list_total_station_get()
		);
	}

/* */
	UpdateStatus = LIST_IS_UPDATED;

	return;
}

/*
 *
 */
static int update_list_configfile( char *configfile )
{
	char *com;
	char *str;
	int   nfiles;
	int   success;

/* Open the main configuration file */
	nfiles = k_open(configfile);
	if ( nfiles == 0 ) {
		logit("e","peak2trig: Error opening command file <%s> when updating!\n", configfile);
		return -1;
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
					logit("e", "peak2trig: Error opening command file <%s> when updating!\n", &com[1]);
					return -1;
   				}
   				continue;
   			}

		/* Process only "Station" command */
			if ( k_its("Station") ) {
				str = k_get();
				for ( str += strlen(str) + 1; isspace(*str); str++ );
				if ( peak2trig_list_sta_line_parse( str, PEAK2TRIG_LIST_UPDATING ) ) {
					logit(
						"e", "peak2trig: Some errors occured in <%s> when updating!\n",
						configfile
					);
					return -1;
				}
			}
		/* See if there were any errors processing the command */
			if ( k_err() ) {
			   logit("e", "peak2trig: Bad <%s> command in <%s> when updating!\n", com, configfile);
			   return -1;
			}
		}
		nfiles = k_close();
	}

	return 0;
}
