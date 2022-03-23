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

/* Earthworm environment header include */
#include <earthworm.h>
#include <kom.h>
#include <transport.h>
#include <lockfile.h>
#include <trace_buf.h>

/* Local header include */
#include <stalist.h>
#include <recordtype.h>
#include <tracepeak.h>
#include <triglist.h>
#include <geogfunc.h>
#include <datestring.h>
#include <griddata.h>
#include <shakemap.h>
#include <shakemap_list.h>
#include <shakemap_triglist.h>
#include <shakemap_msg_queue.h>

/* Functions prototype in this source file
 *******************************/
void  shakemap_config ( char * );
void  shakemap_lookup ( void );
void  shakemap_status ( unsigned char, short, char * );

static thr_ret ShakingMap ( void * );

static void UpdateStaList ( void );
static void EndProc( void );                /* Free all the local memory & close socket */

/* Ring messages things */
static  SHM_INFO  PeakRegion;    /* shared memory region to use for i/o    */
static  SHM_INFO  TrigRegion;    /* shared memory region to use for i/o    */
static  SHM_INFO  OutRegion;     /* shared memory region to use for i/o    */

#define MAXLOGO      5
#define MAX_PATH_STR 256

MSG_LOGO  Putlogo;                /* array for output module, type, instid     */
MSG_LOGO  Getlogo[MAXLOGO];       /* array for requesting module, type, instid */
pid_t     myPid;                  /* for restarts by startstop                 */

/* Thread things */
#define THREAD_STACK 8388608         /* 8388608 Byte = 8192 Kilobyte = 8 Megabyte */
#define THREAD_OFF    0              /* Thread has not been started      */
#define THREAD_ALIVE  1              /* Thread alive and well            */
#define THREAD_ERR   -1              /* Thread encountered error quit    */
sema_t  *SemaPtr;

/* Things to read or derive from configuration file
 **************************************************/
static char     PeakRingName[MAX_RING_STR];   /* name of transport ring for i/o    */
static char     TrigRingName[MAX_RING_STR];   /* name of transport ring for i/o    */
static char     OutRingName[MAX_RING_STR];  /* name of transport ring for i/o    */
static char     ListRingName[MAX_RING_STR]; /* name of transport ring for i/o    */
static char     MyModName[MAX_MOD_STR];     /* speak as this module name/id      */
static uint8_t  LogSwitch;                  /* 0 if no logfile should be written */
static uint64_t HeartBeatInterval;          /* seconds between heartbeats        */
static uint64_t QueueSize = 10;             /* seconds between heartbeats        */
static uint8_t  OutputMapToRingSwitch = 0;
static uint8_t  ListFromRingSwitch    = 0;
static uint16_t nLogo = 0;
static uint16_t TriggerAlgType  = 0;
static uint16_t PeakValueType   = 0;
static uint64_t TriggerDuration = 30;
static char     ReportPath[MAX_PATH_STR];
static float    *X, *Y;
static uint32_t BoundPointCount;
static double   InterpolateDistance = 30.0;

/* Things to look up in the earthworm.h tables with getutil.c functions
 **********************************************************************/
static int64_t PeakRingKey;       /* key of transport ring for i/o     */
static int64_t TrigRingKey;       /* key of transport ring for i/o     */
static int64_t OutRingKey;      /* key of transport ring for i/o     */
static uint8_t InstId;          /* local installation id             */
static uint8_t MyModId;         /* Module Id for this program        */
static uint8_t TypeHeartBeat;
static uint8_t TypeError;
static uint8_t TypeTracePeak = 0;
static uint8_t TypeTrigList  = 0;
static uint8_t TypeGridMap   = 0;

/* Error messages used by shakemap
 *********************************/
#define  ERR_MISSMSG       0   /* message missed in transport ring       */
#define  ERR_TOOBIG        1   /* retreived msg too large for buffer     */
#define  ERR_NOTRACK       2   /* msg retreived; tracking limit exceeded */
#define  ERR_QUEUE         3   /* error queueing message for sending      */
static char Text[150];         /* string for log/error messages          */

/* Station list update status flag
 *********************************/
#define  UPDATE_UNLOCK     0   /* It is open to do station list updating */
#define  UPDATE_NEEDED     1   /* The station list need to update */
#define  UPDATE_PROCING    2   /* The updating is processing */
#define  UPDATE_LOCK       3   /* It is locked to do station list updating */

#define  UPDATE_TIME_INTERVAL 30

static volatile uint8_t  UpdateStatus     = UPDATE_UNLOCK;
static volatile int32_t  ShakingMapStatus = THREAD_OFF;
static volatile uint8_t  Finish        = 0;
static volatile uint64_t TotalStations = 0;

/* struct timespec TT, TT2; */            /* Nanosecond Timer */


int main ( int argc, char **argv )
{
	int res;

	int64_t    msgSize = 0;
	uint64_t   stanumber_local = 0;
	uint64_t   trigseccount = 0;
	MSG_LOGO   reclogo;
	time_t     timeNow;           /* current time                  */
	time_t     timeLastBeat;      /* time last heartbeat was sent  */
	time_t     timeLastTrigger;   /* time last updated stations list */
	time_t     timeLastUpdate;    /* time last updated stations list */

	char       *lockfile;
	int32_t     lockfile_fd;
#if defined( _V710 )
	ew_thread_t   tid;            /* Thread ID */
#else
	unsigned      tid;            /* Thread ID */
#endif

/* Internal data exchange pointer and buffer */
	_STAINFO            key;
	_STAINFO           *staptr   = NULL;
	STA_SHAKE          *shakeptr = NULL;
	SHAKE_LIST_HEADER  *slbuffer = NULL;

/* Input data type and its buffer */
	TRACE_PEAKVALUE tracepv;
	TrigListPacket  tlist;


/* Check command line arguments
 ******************************/
	if ( argc != 2 ) {
		fprintf( stderr, "Usage: shakemap <configfile>\n" );
		exit( 0 );
	}

	Finish = 1;
/* Initialize name of log-file & open it
 ***************************************/
	logit_init( argv[1], 0, 256, 1 );

/* Read the configuration file(s)
 ********************************/
	shakemap_config( argv[1] );
	logit( "" , "%s: Read command file <%s>\n", argv[0], argv[1] );

/* Look up important info from earthworm.h tables
 ************************************************/
	shakemap_lookup();

/* Reinitialize logit to desired logging level
 **********************************************/
	logit_init( argv[1], 0, 256, LogSwitch );

	lockfile = ew_lockfile_path(argv[1]);
	if ( (lockfile_fd = ew_lockfile(lockfile) ) == -1 ) {
		fprintf(stderr, "one instance of %s is already running, exiting\n", argv[0]);
		exit(-1);
	}
/*
	fprintf(stderr, "DEBUG: for %s, fd=%d for %s, LOCKED\n", argv[0], lockfile_fd, lockfile);
*/

/* Get process ID for heartbeat messages */
	myPid = getpid();
	if( myPid == -1 ) {
		logit("e","shakemap: Cannot get pid. Exiting.\n");
		exit(-1);
	}

/* Build the message
 *******************/
	Putlogo.instid = InstId;
	Putlogo.mod    = MyModId;
	Putlogo.type   = TypeGridMap;

/* Attach to Input/Output shared memory ring
 *******************************************/
	tport_attach( &PeakRegion, PeakRingKey );
	logit( "", "shakemap: Attached to public memory region %s: %ld\n",
		PeakRingName, PeakRingKey );

	tport_attach( &TrigRegion, TrigRingKey );
	logit( "", "shakemap: Attached to public memory region %s: %ld\n",
		TrigRingName, TrigRingKey );

/* Flush the transport ring */
	tport_flush( &PeakRegion, Getlogo, nLogo, &reclogo );
	tport_flush( &TrigRegion, Getlogo, nLogo, &reclogo );

	if ( OutputMapToRingSwitch ) {
		tport_attach( &OutRegion, OutRingKey );
		logit( "", "shakemap: Attached to public memory region %s: %ld\n",
			OutRingName, OutRingKey );
	}

/* Force a heartbeat to be issued in first pass thru main loop
 *************************************************************/
	timeLastBeat = time(&timeNow) - HeartBeatInterval - 1;

/* Initialize stations list */
	StaListInit( ListRingName, MyModName, ListFromRingSwitch );
	if ( (TotalStations = StaListFetch()) <= 0 ) {
		logit("e", "shakemap: Cannot get any list of stations, exiting!\n");
		EndProc();
		exit(-1);
	}
	timeLastUpdate  = time(&timeNow);
	timeLastTrigger = timeNow;

	/* CreateSemaphore_ew(); */ /* Obsoleted by Earthworm */
	SemaPtr = CreateSpecificSemaphore_ew( 0 );
	MsgQueueInit( QueueSize, sizeof(SHAKE_LIST_ELEMENT)*TotalStations + sizeof(SHAKE_LIST_HEADER) );

/*----------------------- setup done; start main loop -------------------------*/
	while(1)
	{
	/* Send shakemap's heartbeat
	***************************/
		if  ( time(&timeNow) - timeLastBeat >= (int64_t)HeartBeatInterval ) {
			timeLastBeat = timeNow;
			shakemap_status( TypeHeartBeat, 0, "" );
		}

		if ( UpdateStatus == UPDATE_NEEDED
			&& timeNow - timeLastUpdate >= UPDATE_TIME_INTERVAL
			&& TrigListLength() == 0 )
		{
			UpdateStaList();
		}

		if ( stanumber_local < TotalStations ) {
			if ( stanumber_local ) free(slbuffer);
			stanumber_local = TotalStations;
			slbuffer = (SHAKE_LIST_HEADER *)malloc(sizeof(SHAKE_LIST_ELEMENT)*stanumber_local + sizeof(SHAKE_LIST_HEADER));
			MsgQueueReInit( QueueSize, sizeof(SHAKE_LIST_ELEMENT)*stanumber_local + sizeof(SHAKE_LIST_HEADER) );
		}

		if ( ShakingMapStatus != THREAD_ALIVE && Finish ) {
			if ( StartThread( ShakingMap, (unsigned)THREAD_STACK, &tid ) == -1 ) {
				logit( "e", "shakemap: Error starting ShakingMap thread; exiting!\n");
				EndProc();
				exit(-1);
			}
			ShakingMapStatus = THREAD_ALIVE;
		}

	/* Process all new messages
	**************************/
		do
		{
		/* See if a termination has been requested
		*****************************************/
			if ( tport_getflag( &PeakRegion ) == TERMINATE ||
				tport_getflag( &PeakRegion ) == myPid ) {
			/* write a termination msg to log file */
				logit( "t", "shakemap: Termination requested; exiting!\n" );
				fflush( stdout );
			/* should check the return of these if we really care */
			/*
				fprintf(stderr, "DEBUG: %s, fd=%d for %s\n", argv[0], lockfile_fd, lockfile);
			*/
				Finish = 0;
				PostSpecificSemaphore_ew( SemaPtr );
			/* detach from shared memory */
				sleep_ew(500);
				EndProc();
				KillThread(tid);

				ew_unlockfile(lockfile_fd);
				ew_unlink_lockfile(lockfile);
				exit( 0 );
			}

		/* Get msg & check the return code from transport
		 ************************************************/
			res = tport_getmsg( &TrigRegion, Getlogo, nLogo, &reclogo, &msgSize, (char *)&tlist.msg, MAX_TRIGLIST_SIZ );

			if ( res == GET_OK ) {
			/* Check the received message is the trigger information */
				if ( reclogo.type == TypeTrigList ) {
				/* Check the received message is what we set in the configfile */
					if ( tlist.tlh.trigtype == TriggerAlgType ) {
						TRIG_STATION *tsta = (TRIG_STATION *)(&tlist.tlh + 1);
						TRIG_STATION *tstaend = tsta + tlist.tlh.trigstations;

						for ( ; tsta < tstaend; tsta++ ) {
							strcpy(key.sta, tsta->sta);
							strcpy(key.net, tsta->net);
							strcpy(key.loc, tsta->loc);

							staptr = StaListFind( &key );

							if ( staptr == NULL ) {
							/* Not found in trace table */
								/* printf("shakemap: %s.%s.%s.%s not found in station table, maybe it's a new trace.\n",
									tracepv.sta, tracepv.chan, tracepv.net, tracepv.loc); */
							/* Force to update the table */
								if ( UpdateStatus == UPDATE_UNLOCK ) UpdateStatus = UPDATE_NEEDED;
								continue;
							}

						/* Insert the triggered station pointer to the trigger list */
							if ( TrigListInsert( staptr ) == NULL ) {
								logit( "e","shakemap: Error insert station %s into trigger list.\n", staptr->sta );
								continue;
							}
						}
					/* */
						if ( !trigseccount ) timeLastTrigger = (time_t)tlist.tlh.origintime;
					/* */
						trigseccount = TriggerDuration;
					} /* tlist.tlh.trigtype == TriggerAlgType */
				} /* reclogo.type == TypeTrigList */
			} /* res == GET_OK */

		/* */
			if ( trigseccount > 0 && (time(&timeNow) - timeLastTrigger) >= (int)TRIGGER_MIN_INT ) {
			/* */
				TrigListTimeSync( timeLastTrigger );
				TrigListPeakValUpd();
			/* */
				if ( (msgSize = TrigListPack(slbuffer, sizeof(SHAKE_LIST_ELEMENT)*stanumber_local + sizeof(SHAKE_LIST_HEADER))) > 0 ) {
					slbuffer->trigtype = TriggerAlgType;
					slbuffer->endtime  = timeLastTrigger;
					timeLastTrigger   += (int)TRIGGER_MIN_INT;

					if ( slbuffer->starttime == 0 ) slbuffer->starttime = slbuffer->endtime;
					if ( (trigseccount -= (int)TRIGGER_MIN_INT) == 0 ) slbuffer->codaflag = IS_CODA;
					else slbuffer->codaflag = NO_CODA;

					res = MsgEnqueue( slbuffer, msgSize );

					/* PostSemaphore(); */ /* Obsoleted by Earthworm */
					PostSpecificSemaphore_ew( SemaPtr );

					if ( res ) {
						if ( res == -2 ) {  /* Serious: quit */
						/* Currently, eneueue() in mem_circ_queue.c never returns this error. */
							sprintf(Text, "internal queue error. Terminating.");
							shakemap_status( TypeError, ERR_QUEUE, Text );
							EndProc();
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
						TrigListDestroy();
						memset(slbuffer, 0, sizeof(SHAKE_LIST_ELEMENT)*stanumber_local + sizeof(SHAKE_LIST_HEADER));
					}
				}
				else logit( "e","shakemap: Error packing the trigger list to shake list.\n" );
			}

		/* Get msg & check the return code from transport
		 ************************************************/
			res = tport_getmsg( &PeakRegion, Getlogo, nLogo, &reclogo, &msgSize, (char *)&tracepv, TRACE_PEAKVALUE_SIZE );

			if ( res == GET_NONE )			/* no more new messages     */
			{
				PostSpecificSemaphore_ew( SemaPtr );
				break;
			}
			else if ( res == GET_TOOBIG )	/* next message was too big */
			{								/* complain and try again   */
				sprintf( Text,
						"Retrieved msg[%ld] (i%u m%u t%u) too big for Buffer[%ld]",
						msgSize, reclogo.instid, reclogo.mod, reclogo.type, (long)TRACE_PEAKVALUE_SIZE );
				shakemap_status( TypeError, ERR_TOOBIG, Text );
				continue;
			}
			else if ( res == GET_MISS )		/* got a msg, but missed some */
			{
				sprintf( Text,
						"Missed msg(s)  i%u m%u t%u  %s.",
						reclogo.instid, reclogo.mod, reclogo.type, PeakRingName );
				shakemap_status( TypeError, ERR_MISSMSG, Text );
			}
			else if ( res == GET_NOTRACK )	/* got a msg, but can't tell */
			{								/* if any were missed        */
				sprintf( Text,
						"Msg received (i%u m%u t%u); transport.h NTRACK_GET exceeded",
						reclogo.instid, reclogo.mod, reclogo.type );
				shakemap_status( TypeError, ERR_NOTRACK, Text );
			}

		/* Process the message
		*********************/
			if ( reclogo.type == TypeTracePeak ) {
				if ( tracepv.recordtype == PeakValueType ) {
				/* Debug */
					/* printf("shakemap: Got a new trace peak from %s.%s.%s.%s!\n",
						tracepv.sta, tracepv.chan, tracepv.net, tracepv.loc); */
					strcpy(key.sta,  tracepv.sta);
					strcpy(key.net,  tracepv.net);
					strcpy(key.loc,  tracepv.loc);

					staptr = StaListFind( &key );

					if ( staptr == NULL ) {
					/* Not found in trace table */
						/* printf("shakemap: %s.%s.%s.%s not found in station table, maybe it's a new station.\n",
							tracepv.sta, tracepv.chan, tracepv.net, tracepv.loc); */
					/* Force to update the table */
						if ( UpdateStatus == UPDATE_UNLOCK ) UpdateStatus = UPDATE_NEEDED;
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

					shakeptr->recordtype = tracepv.recordtype;
					shakeptr->peakvalue  = tracepv.peakvalue;
					shakeptr->peaktime   = tracepv.peaktime;
					strcpy(shakeptr->peakchan, tracepv.chan);
				}
			}

			/* logit( "", "%s", res ); */   /* debug */

		} while( 1 );  /* end of message-processing-loop */

		sleep_ew( 50 );  /* no more messages; wait for new ones to arrive */
	}
/*-----------------------------end of main loop-------------------------------*/
	Finish = 0;
	sleep_ew(500);
	EndProc();
	return 0;
}

/******************************************************************************
 *  shakemap_config() processes command file(s) using kom.c functions;       *
 *                    exits if any errors are encountered.                    *
 ******************************************************************************/
void shakemap_config ( char *configfile )
{
	char  init[11];     /* init flags, one byte for each required command */
	char *com;
	char *str;

	uint32_t ncommand;     /* # of required commands you expect to process   */
	uint32_t nmiss;        /* number of required commands that were missed   */
	uint32_t nfiles;
	uint32_t success;
	uint32_t i;

/* Set to zero one init flag for each required command
 *****************************************************/
   ncommand = 11;
   for( i=0; i<ncommand; i++ )  init[i] = 0;

/* Open the main configuration file
 **********************************/
   nfiles = k_open( configfile );
   if ( nfiles == 0 ) {
		logit( "e",
				"shakemap: Error opening command file <%s>; exiting!\n",
				 configfile );
		exit( -1 );
   }

/* Process all command files
 ***************************/
   while(nfiles > 0)   /* While there are command files open */
   {
		while(k_rd())        /* Read next line from active file  */
		{
			com = k_str();         /* Get the first token from line */

		/* Ignore blank lines & comments
		 *******************************/
			if( !com )           continue;
			if( com[0] == '#' )  continue;

		/* Open a nested configuration file
		 **********************************/
			if( com[0] == '@' ) {
			   success = nfiles+1;
			   nfiles  = k_open(&com[1]);
			   if ( nfiles != success ) {
				  logit( "e",
						  "shakemap: Error opening command file <%s>; exiting!\n",
						   &com[1] );
				  exit( -1 );
			   }
			   continue;
			}

		/* Process anything else as a command
		 ************************************/
	/*0*/   if( k_its("LogFile") ) {
				LogSwitch = k_int();
				init[0] = 1;
			}
	/*1*/   else if( k_its("MyModuleId") ) {
				str = k_str();
				if ( str ) strcpy( MyModName, str );
				init[1] = 1;
			}
	/*2*/   else if( k_its("PeakRing") ) {
				str = k_str();
				if ( str ) strcpy( PeakRingName, str );
				init[2] = 1;
			}
	/*3*/   else if( k_its("TrigRing") ) {
				str = k_str();
				if ( str ) strcpy( TrigRingName, str );
				init[3] = 1;
			}
			else if( k_its("OutputRing") ) {
				str = k_str();
				if ( str ) strcpy( OutRingName, str );
				OutputMapToRingSwitch = 1;
			}
			else if( k_its("ListRing") ) {
				str = k_str();
				if ( str ) strcpy( ListRingName, str );
				ListFromRingSwitch = 1;
			}
	/*4*/   else if( k_its("HeartBeatInterval") ) {
				HeartBeatInterval = k_long();
				init[4] = 1;
			}
			else if( k_its("QueueSize") ) {
				QueueSize = k_long();
				logit("o", "shakemap: Queue size change to %ld (default is 10)!\n", QueueSize);
			}
	/*5*/   else if( k_its("TriggerAlgType") ) {
				TriggerAlgType = k_int();
				logit("o", "shakemap: Trigger datatype set to %d!\n", TriggerAlgType);
				init[5] = 1;
			}
	/*6*/   else if( k_its("PeakValueType") ) {
				str = k_str();
				PeakValueType = typestr2num( str );
				logit("o", "shakemap: Peak value type set to %s:%d!\n", str, PeakValueType);
				init[6] = 1;
			}
			else if( k_its("TriggerDuration") ) {
				TriggerDuration = k_int();
				logit("o", "shakemap: Trigger duration change to %ld sec (default is 30 sec)!\n", TriggerDuration);
			}
			else if( k_its("InterpolateDistance") ) {
				InterpolateDistance = k_val();
				logit("o", "shakemap: Cluster max distance change to %.1lf km (default is 30.0 km)!\n", InterpolateDistance);
			}
	/*7*/   else if( k_its("GetListFrom") ) {
				char tmpstr[256] = { 0 };

				str = k_str();
				if ( str ) {
					strcpy( tmpstr, str );
					str = k_str();

					if ( str ) {
						if ( StaListReg( tmpstr, str ) ) {
							logit("e", "shakemap: Error occur when register station list, exiting!\n");
							exit(-1);
						}
					}
				}
				init[7] = 1;
			}
	/*8*/   else if( k_its("ReportPath") ) {
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
	/*9*/   else if( k_its("MapBoundFile") ) {
				str = k_str();
				if ( str ) {
					FILE *filebound;

					if ( (filebound = fopen(str, "r")) == NULL ) {
						logit("e", "shakemap: Error opening map boundary file %s!\n", str);
						exit(-1);
					}
					else {
						int   count = 0;
						float tmpx, tmpy;

						while( fscanf(filebound, "%f %f", &tmpx, &tmpy) == 2 ) count++;

						if ( count > 2 ) {
							X = (float *)calloc(count, sizeof(float));
							Y = (float *)calloc(count, sizeof(float));

							BoundPointCount = count;
							count = 0;

							rewind(filebound);
							while( fscanf(filebound,"%f %f", &X[count], &Y[count]) == 2 ) count++;
						}
						else {
							logit("e", "shakemap: Map boundary file contains not enough point, exiting!\n");
							exit(-1);
						}

						fclose(filebound);
						logit("o", "shakemap: Reading map boundary file finish. Total %d points\n", BoundPointCount);
					}
				}
				init[9] = 1;
			}
		/* Enter installation & module to get event messages from
		********************************************************/
	/*10*/  else if( k_its("GetEventsFrom") ) {
				if ( nLogo+1 >= MAXLOGO ) {
					logit( "e",
							"shakemap: Too many <GetEventsFrom> commands in <%s>",
							configfile );
					logit( "e", "; max=%d; exiting!\n", (int) MAXLOGO );
					exit( -1 );
				}
				if( ( str=k_str() ) ) {
					if( GetInst( str, &Getlogo[nLogo].instid ) != 0 ) {
						logit( "e",
								"shakemap: Invalid installation name <%s>", str );
						logit( "e", " in <GetEventsFrom> cmd; exiting!\n" );
						exit( -1 );
					}
				}
				if( ( str=k_str() ) ) {
					if( GetModId( str, &Getlogo[nLogo].mod ) != 0 ) {
						logit( "e",
								"shakemap: Invalid module name <%s>", str );
						logit( "e", " in <GetEventsFrom> cmd; exiting!\n" );
						exit( -1 );
					}
				}
				if( ( str=k_str() ) ) {
					if( GetType( str, &Getlogo[nLogo].type ) != 0 ) {
						logit( "e",
								"shakemap: Invalid message type name <%s>", str );
						logit( "e", " in <GetEventsFrom> cmd; exiting!\n" );
						exit( -1 );
					}
				}
				nLogo++;
				init[10] = 1;

				/*printf("Getlogo[%d] inst:%d module:%d type:%d\n", nLogo,
					(int)Getlogo[nLogo].instid,
					(int)Getlogo[nLogo].mod,
					(int)Getlogo[nLogo].type );*/   /*DEBUG*/
			}

		 /* Unknown command
		  *****************/
			else {
				logit( "e", "shakemap: <%s> Unknown command in <%s>.\n",
						 com, configfile );
				continue;
			}

		/* See if there were any errors processing the command
		 *****************************************************/
			if( k_err() ) {
			   logit( "e",
					   "shakemap: Bad <%s> command in <%s>; exiting!\n",
						com, configfile );
			   exit( -1 );
			}
		}
		nfiles = k_close();
   }

/* After all files are closed, check init flags for missed commands
 ******************************************************************/
	nmiss = 0;
	for ( i=0; i<ncommand; i++ )  if( !init[i] ) nmiss++;
	if ( nmiss ) {
		logit( "e", "shakemap: ERROR, no " );
		if ( !init[0] )  logit( "e", "<LogFile> "           );
		if ( !init[1] )  logit( "e", "<MyModuleId> "        );
		if ( !init[2] )  logit( "e", "<PeakRing> "          );
		if ( !init[3] )  logit( "e", "<TrigRing> "          );
		if ( !init[4] )  logit( "e", "<HeartBeatInterval> " );
		if ( !init[5] )  logit( "e", "<TriggerAlgType> "    );
		if ( !init[6] )  logit( "e", "<PeakValueType> "     );
		if ( !init[7] )  logit( "e", "any <GetListFrom> "   );
		if ( !init[8] )  logit( "e", "<ReportPath> "        );
		if ( !init[9] )  logit( "e", "<MapBoundFile> "      );
		if ( !init[10] ) logit( "e", "any <GetEventsFrom> " );

		logit( "e", "command(s) in <%s>; exiting!\n", configfile );
		exit( -1 );
	}

	return;
}

/******************************************************************************
 *  shakemap_lookup( )   Look up important info from earthworm.h tables       *
 ******************************************************************************/
void shakemap_lookup ( void )
{
/* Look up keys to shared memory regions
*************************************/
	if( ( PeakRingKey = GetKey(PeakRingName) ) == -1 ) {
		fprintf( stderr,
				"shakemap:  Invalid ring name <%s>; exiting!\n", PeakRingName);
		exit( -1 );
	}

	if( ( TrigRingKey = GetKey(TrigRingName) ) == -1 ) {
		fprintf( stderr,
				"shakemap:  Invalid ring name <%s>; exiting!\n", TrigRingName);
		exit( -1 );
	}

	if ( OutputMapToRingSwitch ) {
		if( ( OutRingKey = GetKey(OutRingName) ) == -1 ) {
			fprintf( stderr,
					"shakemap:  Invalid ring name <%s>; exiting!\n", OutRingName);
			exit( -1 );
		}
	}

/* Look up installations of interest
*********************************/
	if ( GetLocalInst( &InstId ) != 0 ) {
		fprintf( stderr,
				"shakemap: error getting local installation id; exiting!\n" );
		exit( -1 );
	}

/* Look up modules of interest
***************************/
	if ( GetModId( MyModName, &MyModId ) != 0 ) {
		fprintf( stderr,
				"shakemap: Invalid module name <%s>; exiting!\n", MyModName );
		exit( -1 );
	}

/* Look up message types of interest
*********************************/
	if ( GetType( "TYPE_HEARTBEAT", &TypeHeartBeat ) != 0 ) {
		fprintf( stderr,
				"shakemap: Invalid message type <TYPE_HEARTBEAT>; exiting!\n" );
		exit( -1 );
	}
	if ( GetType( "TYPE_ERROR", &TypeError ) != 0 ) {
		fprintf( stderr,
				"shakemap: Invalid message type <TYPE_ERROR>; exiting!\n" );
		exit( -1 );
	}
	if ( GetType( "TYPE_TRACEPEAK", &TypeTracePeak ) != 0 ) {
		fprintf( stderr,
				"shakemap: Invalid message type <TYPE_TRACEPEAK>; exiting!\n" );
		exit( -1 );
	}
	if ( GetType( "TYPE_TRIGLIST", &TypeTrigList ) != 0 ) {
		fprintf( stderr,
				"shakemap: Invalid message type <TYPE_TRIGLIST>; exiting!\n" );
		exit( -1 );
	}
	if ( GetType( "TYPE_GRIDMAP", &TypeGridMap ) != 0 ) {
		fprintf( stderr,
				"shakemap: Invalid message type <TYPE_GRIDMAP>; exiting!\n" );
		exit( -1 );
	}

	return;
}

/******************************************************************************
 * shakemap_status() builds a heartbeat or error message & puts it into      *
 *                   shared memory.  Writes errors to log file & screen.      *
 ******************************************************************************/
void shakemap_status ( unsigned char type, short ierr, char *note )
{
   MSG_LOGO    logo;
   char        msg[256];
   uint64_t    size;
   time_t      t;

/* Build the message
 *******************/
   logo.instid = InstId;
   logo.mod    = MyModId;
   logo.type   = type;

   time( &t );

   if( type == TypeHeartBeat ) {
		sprintf( msg, "%ld %ld\n", (long)t, (long)myPid);
   }
   else if( type == TypeError ) {
		sprintf( msg, "%ld %hd %s\n", (long)t, ierr, note);
		logit( "et", "shakemap: %s\n", note );
   }

   size = strlen( msg );   /* don't include the null byte in the message */

/* Write the message to shared memory
 ************************************/
   if( tport_putmsg( &PeakRegion, &logo, size, msg ) != PUT_OK ) {
		if( type == TypeHeartBeat ) {
		   logit("et","shakemap:  Error sending heartbeat.\n" );
		}
		else if( type == TypeError ) {
		   logit("et","shakemap:  Error sending error:%d.\n", ierr );
		}
   }

   return;
}

/******************************************************************************
 * UpdateStaList() ...                      *
 ******************************************************************************/
static void UpdateStaList ( void )
{
	UpdateStatus = UPDATE_PROCING;

	logit("o", "shakemap: Start to updating the list of stations.\n");

	if ( (TotalStations = StaListFetch()) <= 0 )
		logit("e", "shakemap: Cannot update the list of stations.\n");

/* Tell other threads that update is finshed */
	UpdateStatus = UPDATE_UNLOCK;

	return;
}

/******************************************************************************
 * ShakingMap() Read the station info from the array <stainfo>. And creates   *
 *              the table of shakemap of Taiwan.                              *
 ******************************************************************************/
static thr_ret ShakingMap ( void *dummy )
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

		res = MsgDequeue( slbuffer, &recsize );

		if ( res == 0 ) {
			int   mm;

			_STAINFO  *staptr = NULL;
			FILE      *evfptr;
			struct tm *tp = gmtime( &(slbuffer->endtime) );

			char eventfile[MAX_PATH_STR] = { 0 };
			char reportdate[MAX_DSTR_LENGTH] = { 0 };
			char rsec[3] = { 0 };

			uint32_t putsize;
			uint32_t count_25 = 0, count_80 = 0;
			uint32_t count_250 = 0, count_400 = 0;

			double lonmax = 0.0, lonmin = 180.0;
			double latmax = 0.0, latmin = 90.0;
			double tmplat = 0.0, tmplon = 0.0;
			double dissum = 0.0, valsum = 0.0, voverd = 0.0;
			double dists = 0.0;

		/* Generate the report file name by the present time */
			date2spstring( tp, reportdate, sizeof(reportdate) );
			memset(&reportdate[12], 0, 3);

		/* Generate the second string */
			sprintf(rsec, "%02d", tp->tm_sec);

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
								count_25++;
								if ( voverd >= 110.0 ) {
									count_80++;
									if ( voverd >= 370.0 ) {
										count_250++;
										if ( voverd >= 400.0 ) count_400++;
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

		/* Calculate the estimated magnitude */
			if ( count_25 ) {
				gmbuffer->magnitude[0] = (0.002248 * 80 + 0.279229) * log10(count_25*25) + 4.236343;
				/* mag25 = (0.001949 * 80 + 0.271711) * log10(count_25*25) + 4.455552; */
				if ( count_80 ) {
					gmbuffer->magnitude[1] = (0.002248 * 110 + 0.279229) * log10(count_80*25) + 4.236343;
					/* mag80 = (0.001949 * 110 + 0.271711) * log10(count_80*25) + 4.455552; */
					if ( count_250 ) {
						gmbuffer->magnitude[2] = (0.002248 * 370 + 0.279229) * log10(count_250*25) + 4.236343;
						/* mag250 = (0.001949 * 370 + 0.271711) * log10(count_250*25) + 4.455552; */
						if ( count_400 ) {
							gmbuffer->magnitude[3] = (0.002248 * 400 + 0.279229) * log10(count_400*25) + 4.236343;
							/* mag400 = (0.001949 * 400 + 0.271711) * log10(count_400*25) + 4.455552; */
						}
					}
				}
			}

		/* Output the the whole shakemap data to share ring */
			if ( OutputMapToRingSwitch ) {
				putsize = sizeof(GRIDMAP_HEADER) + sizeof(GRID_REC) * gmbuffer->totalgrids;
				if ( tport_putmsg( &OutRegion, &Putlogo, putsize, (char *)gmbuffer ) != PUT_OK ) {
					logit( "e", "shakemap: Error putting message in region %ld\n", OutRegion.key );
				}
			}

			if ( gmbuffer->starttime == gmbuffer->endtime )
				logit( "ot", "New event detected, start generating grid map!\n" );

		/* Output the triggered stations table */
			sprintf( eventfile, "%s%s_%s_sta.txt", ReportPath, reportdate, typenum2str( gmbuffer->valuetype ) );
			evfptr  = fopen( eventfile, "a" );

			gridrec = (GRID_REC *)(gmbuffer + 1);
			gridend = gridrec + gmbuffer->totalgrids;
			for ( ; gridrec->gridtype == GRID_STATION && gridrec < gridend; gridrec++ ) {
				fprintf( evfptr, "%s %6.2f %6.2f %6.2f %s\n",
					gridrec->gridname, gridrec->longitude, gridrec->latitude, gridrec->gridvalue, rsec );
			}

			fclose( evfptr );

		/* Check if the grid record type is GRID_MAPGRID */
			for ( ; gridrec->gridtype != GRID_MAPGRID && gridrec < gridend; gridrec++ );
			if ( gridrec >= gridend ) {
				logit( "t", "shakemap: There is not any map grid data!\n" );
				continue;
			}

		/* Output the peak value data of triggered grid */
			sprintf( eventfile, "%s%s_%s.txt", ReportPath, reportdate, typenum2str( gmbuffer->valuetype ) );
			evfptr = fopen( eventfile, "a" );

			for ( ; gridrec->gridtype == GRID_MAPGRID && gridrec < gridend; gridrec++ ) {
				fprintf( evfptr, "%6.2f %6.2f %8.3f %s\n",
					gridrec->longitude, gridrec->latitude, gridrec->gridvalue, rsec );
			}

			fclose( evfptr );

		/* Output the estimated magnitude record */
			if ( count_25 ) {
				sprintf( eventfile, "%s%s_mag.txt", ReportPath, reportdate );
				evfptr = fopen( eventfile, "a" );

				fprintf( evfptr, "%4.1f %4.1f %4.1f %4.1f %4d %4d %4d %4d %3d %6.2f %5.2f %6.2f %s\n",
					gmbuffer->magnitude[0], gmbuffer->magnitude[1], gmbuffer->magnitude[2], gmbuffer->magnitude[3],
					count_25*25, count_80*25, count_250*25, count_400*25, slbuffer->totalstations,
					gmbuffer->centerlon, gmbuffer->centerlat, gmbuffer->centervalue, rsec );

				fclose( evfptr );
			}

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

/*************************************************************
 *  EndProc( )  free all the local memory & close queue      *
 *************************************************************/
static void EndProc ( void )
{
	Finish = 0;
	sleep_ew( 100 );

	tport_detach( &PeakRegion );
	tport_detach( &TrigRegion );
	if ( OutputMapToRingSwitch )
		tport_detach( &OutRegion );

	TrigListDestroy();
	StaListEnd();
	DestroySpecificSemaphore_ew( SemaPtr );

	return;
}
