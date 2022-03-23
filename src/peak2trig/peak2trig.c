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
#include <stalist.h>
#include <recordtype.h>
#include <tracepeak.h>
#include <triglist.h>
#include <peak2trig.h>
#include <peak2trig_list.h>
#include <peak2trig_triglist.h>


/* Functions prototype in this source file
 *******************************/
void  peak2trig_config ( char * );
void  peak2trig_lookup ( void );
void  peak2trig_status ( unsigned char, short, char * );

static void UpdateStaList ( void );
static void EndProc ( void );                /* Free all the local memory & close socket */

/* Ring messages things */
static  SHM_INFO  InRegion;      /* shared memory region to use for i/o    */
static  SHM_INFO  OutRegion;     /* shared memory region to use for i/o    */

#define MAXLOGO 5

MSG_LOGO  Putlogo;                /* array for output module, type, instid     */
MSG_LOGO  Getlogo[MAXLOGO];                /* array for requesting module, type, instid */
pid_t     myPid;                  /* for restarts by startstop                 */

/* Things to read or derive from configuration file
 **************************************************/
static char     InRingName[MAX_RING_STR];   /* name of transport ring for i/o    */
static char     OutRingName[MAX_RING_STR];  /* name of transport ring for i/o    */
static char     ListRingName[MAX_RING_STR]; /* name of transport ring for i/o    */
static char     MyModName[MAX_MOD_STR];     /* speak as this module name/id      */
static uint8_t  LogSwitch;                  /* 0 if no logfile should be written */
static uint64_t HeartBeatInterval;          /* seconds between heartbeats        */
static uint8_t  ListFromRingSwitch = 0;
static uint16_t nLogo = 0;
static uint16_t TriggerTimeInterval = 0;
static uint16_t RecordTypeToTrig = 0;
static uint16_t TriggerStations = 3;
static double   PeakThreshold   = 1.5;
static double   PeakDuration    = 6.0;
static double   ClusterDistance = 60.0;
static double   ClusterTimeGap  = 5.0;

/* Things to look up in the earthworm.h tables with getutil.c functions
 **********************************************************************/
static int64_t InRingKey;       /* key of transport ring for i/o     */
static int64_t OutRingKey;      /* key of transport ring for i/o     */
static uint8_t InstId;          /* local installation id             */
static uint8_t MyModId;         /* Module Id for this program        */
static uint8_t TypeHeartBeat;
static uint8_t TypeError;
static uint8_t TypeTracePeak = 0;
static uint8_t TypeTrigList = 0;

/* Error messages used by peak2trig
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

static volatile uint8_t UpdateStatus = UPDATE_UNLOCK;
/* struct timespec TT, TT2; */            /* Nanosecond Timer */


int main ( int argc, char **argv )
{
	int res;

	_Bool      evflag = FALSE;
	int64_t    msgSize = 0;
	MSG_LOGO   reclogo;
	time_t     timeNow;          /* current time                    */
	time_t     timeLastBeat;     /* time last heartbeat was sent    */
	time_t     timeLastTrig;     /* time last trigger list was sent */
	time_t     timeLastUpdate;   /* time last updated stations list */

	char   *lockfile;
	int32_t lockfile_fd;

	_STAINFO         key;
	_STAINFO        *staptr    = NULL;
	STA_NODE        *targetsta = NULL;
	TRACE_PEAKVALUE  tracepv;
	TrigListPacket   obuffer;


/* Check command line arguments
 ******************************/
	if ( argc != 2 ) {
		fprintf( stderr, "Usage: peak2trig <configfile>\n" );
		exit( 0 );
	}

/* Initialize name of log-file & open it
 ***************************************/
	logit_init( argv[1], 0, 256, 1 );

/* Read the configuration file(s)
 ********************************/
	peak2trig_config( argv[1] );
	logit( "" , "%s: Read command file <%s>\n", argv[0], argv[1] );

/* Look up important info from earthworm.h tables
 ************************************************/
	peak2trig_lookup();

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
		logit("e","peak2trig: Cannot get pid. Exiting.\n");
		exit(-1);
	}

/* Build the message
 *******************/
	Putlogo.instid = InstId;
	Putlogo.mod    = MyModId;
	Putlogo.type   = TypeTrigList;

/* Attach to Input/Output shared memory ring
 *******************************************/
	tport_attach( &InRegion, InRingKey );
	logit( "", "peak2trig: Attached to public memory region %s: %ld\n",
		InRingName, InRingKey );

/* Flush the transport ring */
	tport_flush( &InRegion, Getlogo, nLogo, &reclogo );

	tport_attach( &OutRegion, OutRingKey );
	logit( "", "peak2trig: Attached to public memory region %s: %ld\n",
		OutRingName, OutRingKey );

/* Force a heartbeat to be issued in first pass thru main loop
 *************************************************************/
	timeLastBeat = time(&timeNow) - HeartBeatInterval - 1;

/* Initialize the time stamp for trigger */
	timeLastTrig = timeNow - TriggerTimeInterval - 1;

/* Initialize traces list */
	StaListInit( ListRingName, MyModName, ListFromRingSwitch );
	if ( StaListFetch() <= 0 ) {
		logit("e", "peak2trig: Cannot get the list of stations. Exiting!\n");
		EndProc();
		exit(-1);
	}
	timeLastUpdate = time(&timeNow);

/* Initialize the triggered type */
	memset(obuffer.msg, 0, MAX_TRIGLIST_SIZ);

/*----------------------- setup done; start main loop -------------------------*/
	while(1)
	{
	/* Send peak2trig's heartbeat
	***************************/
		if  ( time(&timeNow) - timeLastBeat >= (int64_t)HeartBeatInterval ) {
			timeLastBeat = timeNow;
			peak2trig_status( TypeHeartBeat, 0, "" );
		}

		if ( UpdateStatus == UPDATE_NEEDED &&
			timeNow - timeLastUpdate >= UPDATE_TIME_INTERVAL &&
			TrigListLength() == 0 ) {
		/* Enter the update station list function */
			UpdateStaList();
		}

	/* Process all new messages
	**************************/
		do
		{
		/* See if a termination has been requested
		*****************************************/
			if ( tport_getflag( &InRegion ) == TERMINATE ||
				tport_getflag( &InRegion ) == myPid ) {
			/* write a termination msg to log file */
				logit( "t", "peak2trig: Termination requested; exiting!\n" );
				fflush( stdout );
			/* should check the return of these if we really care */
			/*
				fprintf(stderr, "DEBUG: %s, fd=%d for %s\n", argv[0], lockfile_fd, lockfile);
			*/
				sleep_ew(10);
			/* detach from shared memory and those lists */
				EndProc();

				ew_unlockfile(lockfile_fd);
				ew_unlink_lockfile(lockfile);
				exit( 0 );
			}

		/* Get msg & check the return code from transport
		 ************************************************/
			res = tport_getmsg( &InRegion, Getlogo, nLogo, &reclogo, &msgSize, (char *)&tracepv, TRACE_PEAKVALUE_SIZE );

			if ( res == GET_NONE )			/* no more new messages     */
			{
				break;
			}
			else if ( res == GET_TOOBIG )	/* next message was too big */
			{								/* complain and try again   */
				sprintf( Text,
						"Retrieved msg[%ld] (i%u m%u t%u) too big for Buffer[%ld]",
						msgSize, reclogo.instid, reclogo.mod, reclogo.type, (long)TRACE_PEAKVALUE_SIZE );
				peak2trig_status( TypeError, ERR_TOOBIG, Text );
				continue;
			}
			else if ( res == GET_MISS )		/* got a msg, but missed some */
			{
				sprintf( Text,
						"Missed msg(s)  i%u m%u t%u  %s.",
						reclogo.instid, reclogo.mod, reclogo.type, InRingName );
				peak2trig_status( TypeError, ERR_MISSMSG, Text );
			}
			else if ( res == GET_NOTRACK )	/* got a msg, but can't tell */
			{								/* if any were missed        */
				sprintf( Text,
						"Msg received (i%u m%u t%u); transport.h NTRACK_GET exceeded",
						reclogo.instid, reclogo.mod, reclogo.type );
				peak2trig_status( TypeError, ERR_NOTRACK, Text );
			}

		/* Process the message
		*********************/
			if ( reclogo.type == TypeTracePeak ) {
				if ( tracepv.recordtype == RecordTypeToTrig ) {
					if ( (tracepv.peakvalue = fabs(tracepv.peakvalue)) >= PeakThreshold ) {
					/* Debug */
					/*
						printf("peak2trig: Got a new trace peak from %s.%s.%s.%s!\n",
							tracepv.sta, tracepv.chan, tracepv.net, tracepv.loc);
					*/
						strcpy(key.sta,  tracepv.sta);
						strcpy(key.net,  tracepv.net);
						strcpy(key.loc,  tracepv.loc);

						staptr = StaListFind( &key );

						if ( staptr == NULL ) {
						/* Not found in trace table */
						/*
							printf("peak2trig: %s.%s.%s.%s not found in station table, maybe it's a new station.\n",
								tracepv.sta, tracepv.chan, tracepv.net, tracepv.loc);
						*/
						/* Force to update the table */
							if ( UpdateStatus == UPDATE_UNLOCK ) UpdateStatus = UPDATE_NEEDED;
							continue;
						}

					/* Insert the triggered station pointer to the trigger list */
						if ( (targetsta = TrigListInsert( staptr )) == NULL ) {
							logit( "e","peak2trig: Error insert station %s into trigger list.\n", staptr->sta );
							continue;
						}

						TrigListUpdate( &tracepv, targetsta );

						if ( (time(&timeNow) - timeLastTrig) >= TriggerTimeInterval ) {
						/* Check if the length of trigger list is larger than cluster threshold */
							if ( TrigListLength() > (int)CLUSTER_NUM ) {
							/* Do the cluster with the input condition(ex: distance & time gap) */
								if ( TrigListCluster( ClusterDistance, ClusterTimeGap ) >= TriggerStations ) {
								/* The triggered stations is over the threshold and issue the triggered message */
									if ( (msgSize = TrigListPack( obuffer.msg, MAX_TRIGLIST_SIZ )) > 0 ) {
									/* Since there is still new station detecting peak value higher than threshold, the event should be undergoing */
										obuffer.tlh.codaflag = NO_CODA;
										obuffer.tlh.trigtype = TRIG_BY_PEAK_CLUSTER;

									/* */
										if ( evflag ) {
										/* Following detection */
											logit("ot", "peak2trig: During detected possible event last for %.1lfs, total triggered stations for now are %d!\n",
												obuffer.tlh.trigtime - obuffer.tlh.origintime,
												obuffer.tlh.trigstations);
										}
										else {
										/* First detection */
											logit("ot", "peak2trig: First detected possible event!! Total triggered stations are %d!\n", obuffer.tlh.trigstations);
											evflag = TRUE;
										}

										if ( tport_putmsg( &OutRegion, &Putlogo, msgSize, (char *)obuffer.msg ) != PUT_OK ) {
											logit( "e", "peak2trig: Error putting message in region %ld\n", OutRegion.key );
										}
									}
									timeLastTrig = timeNow;
								}
							}
						}
					}  /* tracepv.peakvalue >= PeakThreshold */
				}  /* tracepv.recordtype == RecordTypeToTrig */
			}  /* reclogo.type == TypeTracePeak */

			if ( evflag && (time(&timeNow) - timeLastTrig) > PeakDuration ) {
			/* Coda? */
				logit("ot", "peak2trig: End of possible event! Event duration might be %.1lfs!\n", obuffer.tlh.trigtime - obuffer.tlh.origintime);
				evflag = FALSE;
			/*
			 * Since there is still new station detecting peak value higher than threshold,
			 * the event should be undergoing
			 */
				obuffer.tlh.codaflag = IS_CODA;
				obuffer.tlh.trigtype = TRIG_BY_PEAK_CLUSTER;
				msgSize = TRIGLIST_SIZE_GET( &obuffer.tlh );
				if ( tport_putmsg( &OutRegion, &Putlogo, msgSize, (char *)obuffer.msg ) != PUT_OK ) {
					logit( "e", "peak2trig: Error putting message in region %ld\n", OutRegion.key );
				}
			/* Reset the output buffer for the next event */
				memset(obuffer.msg, 0, MAX_TRIGLIST_SIZ);
			}
			/* logit( "", "%s", res ); */   /* debug */

		} while( 1 );  /* end of message-processing-loop */

	/* Do the filter to remove the stations which are obsolete */
		TrigListTimeFilter( PeakDuration );
		sleep_ew( 50 );  /* no more messages; wait for new ones to arrive */
	}
/*-----------------------------end of main loop-------------------------------*/
	EndProc();
	return 0;
}

/******************************************************************************
 *  peak2trig_config() processes command file(s) using kom.c functions;       *
 *                    exits if any errors are encountered.                    *
 ******************************************************************************/
void peak2trig_config ( char *configfile )
{
	char  init[9];     /* init flags, one byte for each required command */
	char *com;
	char *str;

	uint32_t ncommand;     /* # of required commands you expect to process   */
	uint32_t nmiss;        /* number of required commands that were missed   */
	uint32_t nfiles;
	uint32_t success;
	uint32_t i;

/* Set to zero one init flag for each required command
 *****************************************************/
   ncommand = 9;
   for( i=0; i<ncommand; i++ )  init[i] = 0;

/* Open the main configuration file
 **********************************/
   nfiles = k_open( configfile );
   if ( nfiles == 0 ) {
		logit( "e",
				"peak2trig: Error opening command file <%s>; exiting!\n",
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
						  "peak2trig: Error opening command file <%s>; exiting!\n",
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
				if(str) strcpy( MyModName, str );
				init[1] = 1;
			}
	/*2*/   else if( k_its("InputRing") ) {
				str = k_str();
				if(str) strcpy( InRingName, str );
				init[2] = 1;
			}
	/*3*/   else if( k_its("OutputRing") ) {
				str = k_str();
				if(str) strcpy( OutRingName, str );
				init[3] = 1;
			}
			else if( k_its("ListRing") ) {
				str = k_str();
				if(str) strcpy( ListRingName, str );
				ListFromRingSwitch = 1;
			}
	/*4*/   else if( k_its("HeartBeatInterval") ) {
				HeartBeatInterval = k_long();
				init[4] = 1;
			}
			else if( k_its("TriggerTimeInterval") ) {
				TriggerTimeInterval = k_long();
				logit("o", "peak2trig: Trigger time interval change to %d sec, default is 0 (Real-time mode)!\n", TriggerTimeInterval);
			}
	/*5*/   else if( k_its("RecordTypeToTrig") ) {
				str = k_str();
				RecordTypeToTrig = typestr2num( str );
				logit("o", "peak2trig: Trigger datatype set to %s:%d!\n", str, RecordTypeToTrig);
				init[5] = 1;
			}
			else if( k_its("TriggerStations") ) {
				TriggerStations = k_int();
				logit("o", "peak2trig: Trigger threshold of stations change to %d, default is 3!\n", TriggerStations);
			}
	/*6*/   else if( k_its("PeakThreshold") ) {
				PeakThreshold = k_val();
				logit("o", "peak2trig: Trigger threshold of peak value set to %.2lf, unit depends on datatype!\n", PeakThreshold);
				init[6] = 1;
			}
			else if( k_its("PeakDuration") ) {
				PeakDuration = k_val();
				logit("o", "peak2trig: Peak value duration change to %.2lf sec, default is 6.0 sec!\n", PeakDuration);
			}
			else if( k_its("ClusterDistance") ) {
				ClusterDistance = k_val();
				logit("o", "peak2trig: Cluster max distance change to %.1lf km, default is 60.0 km!\n", ClusterDistance);
			}
			else if( k_its("ClusterTimeGap") ) {
				ClusterTimeGap = k_val();
				logit("o", "peak2trig: Cluster time gap between each station change to %.2lf sec, default is 5.0 sec!\n", ClusterTimeGap);
			}
	/*7*/   else if( k_its("GetListFrom") ) {
				char tmpstr[256] = { 0 };

				str = k_str();
				if ( str ) {
					strcpy( tmpstr, str );
					str = k_str();

					if ( str ) {
						if ( StaListReg( tmpstr, str ) ) {
							logit( "e", "peak2trig: Error occur when register station list, exiting!\n" );
						}
					}
				}
				init[7] = 1;
			}
		/* Enter installation & module to get event messages from
		********************************************************/
	/*8*/   else if( k_its("GetEventsFrom") ) {
				if ( nLogo+1 >= MAXLOGO ) {
					logit( "e",
							"peak2trig: Too many <GetEventsFrom> commands in <%s>",
							configfile );
					logit( "e", "; max=%d; exiting!\n", (int) MAXLOGO );
					exit( -1 );
				}
				if( ( str = k_str() ) ) {
					if( GetInst( str, &Getlogo[nLogo].instid ) != 0 ) {
						logit( "e",
								"peak2trig: Invalid installation name <%s>", str );
						logit( "e", " in <GetEventsFrom> cmd; exiting!\n" );
						exit( -1 );
					}
				}
				if( ( str = k_str() ) ) {
					if( GetModId( str, &Getlogo[nLogo].mod ) != 0 ) {
						logit( "e",
								"peak2trig: Invalid module name <%s>", str );
						logit( "e", " in <GetEventsFrom> cmd; exiting!\n" );
						exit( -1 );
					}
				}
				if( ( str = k_str() ) ) {
					if( GetType( str, &Getlogo[nLogo].type ) != 0 ) {
						logit( "e",
								"peak2trig: Invalid message type name <%s>", str );
						logit( "e", " in <GetEventsFrom> cmd; exiting!\n" );
						exit( -1 );
					}
				}
				nLogo++;
				init[8] = 1;

				/*printf("Getlogo[%d] inst:%d module:%d type:%d\n", nLogo,
					(int)Getlogo[nLogo].instid,
					(int)Getlogo[nLogo].mod,
					(int)Getlogo[nLogo].type );*/   /*DEBUG*/
			}

		 /* Unknown command
		  *****************/
			else {
				logit( "e", "peak2trig: <%s> Unknown command in <%s>.\n",
						 com, configfile );
				continue;
			}

		/* See if there were any errors processing the command
		 *****************************************************/
			if( k_err() ) {
			   logit( "e",
					   "peak2trig: Bad <%s> command in <%s>; exiting!\n",
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
		logit( "e", "peak2trig: ERROR, no " );
		if ( !init[0] )  logit( "e", "<LogFile> "           );
		if ( !init[1] )  logit( "e", "<MyModuleId> "        );
		if ( !init[2] )  logit( "e", "<InputRing> "         );
		if ( !init[3] )  logit( "e", "<OutputRing> "        );
		if ( !init[4] )  logit( "e", "<HeartBeatInterval> " );
		if ( !init[5] )  logit( "e", "<RecordTypeToTrig> "  );
		if ( !init[6] )  logit( "e", "<PeakThreshold> "     );
		if ( !init[7] )  logit( "e", "any <GetListFrom> "   );
		if ( !init[8] )  logit( "e", "any <GetEventsFrom> " );

		logit( "e", "command(s) in <%s>; exiting!\n", configfile );
		exit( -1 );
	}

	return;
}

/******************************************************************************
 *  peak2trig_lookup( )   Look up important info from earthworm.h tables       *
 ******************************************************************************/
void peak2trig_lookup ( void )
{
/* Look up keys to shared memory regions
*************************************/
	if( ( InRingKey = GetKey(InRingName) ) == -1 ) {
		fprintf( stderr,
				"peak2trig:  Invalid ring name <%s>; exiting!\n", InRingName);
		exit( -1 );
	}

	if( ( OutRingKey = GetKey(OutRingName) ) == -1 ) {
		fprintf( stderr,
				"peak2trig:  Invalid ring name <%s>; exiting!\n", OutRingName);
		exit( -1 );
	}

/* Look up installations of interest
*********************************/
	if ( GetLocalInst( &InstId ) != 0 ) {
		fprintf( stderr,
				"peak2trig: error getting local installation id; exiting!\n" );
		exit( -1 );
	}

/* Look up modules of interest
***************************/
	if ( GetModId( MyModName, &MyModId ) != 0 ) {
		fprintf( stderr,
				"peak2trig: Invalid module name <%s>; exiting!\n", MyModName );
		exit( -1 );
	}

/* Look up message types of interest
*********************************/
	if ( GetType( "TYPE_HEARTBEAT", &TypeHeartBeat ) != 0 ) {
		fprintf( stderr,
				"peak2trig: Invalid message type <TYPE_HEARTBEAT>; exiting!\n" );
		exit( -1 );
	}
	if ( GetType( "TYPE_ERROR", &TypeError ) != 0 ) {
		fprintf( stderr,
				"peak2trig: Invalid message type <TYPE_ERROR>; exiting!\n" );
		exit( -1 );
	}
	if ( GetType( "TYPE_TRACEPEAK", &TypeTracePeak ) != 0 ) {
		fprintf( stderr,
				"peak2trig: Invalid message type <TYPE_TRACEPEAK>; exiting!\n" );
		exit( -1 );
	}
	if ( GetType( "TYPE_TRIGLIST", &TypeTrigList ) != 0 ) {
		fprintf( stderr,
				"peak2trig: Invalid message type <TYPE_TRIGLIST>; exiting!\n" );
		exit( -1 );
	}

	return;
}

/******************************************************************************
 * peak2trig_status() builds a heartbeat or error message & puts it into      *
 *                   shared memory.  Writes errors to log file & screen.      *
 ******************************************************************************/
void peak2trig_status ( unsigned char type, short ierr, char *note )
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

   if( type == TypeHeartBeat )
   {
		sprintf( msg, "%ld %ld\n", (long)t, (long)myPid);
   }
   else if( type == TypeError )
   {
		sprintf( msg, "%ld %hd %s\n", (long)t, ierr, note);
		logit( "et", "peak2trig: %s\n", note );
   }

   size = strlen( msg );   /* don't include the null byte in the message */

/* Write the message to shared memory
 ************************************/
   if( tport_putmsg( &InRegion, &logo, size, msg ) != PUT_OK )
   {
		if( type == TypeHeartBeat ) {
		   logit("et","peak2trig:  Error sending heartbeat.\n" );
		}
		else if( type == TypeError ) {
		   logit("et","peak2trig:  Error sending error:%d.\n", ierr );
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

	logit("o", "peak2trig: Start to updating the list of stations.\n");

	if ( StaListFetch() <= 0 )
		logit("e", "peak2trig: Cannot update the list of stations.\n");

/* Tell other threads that update is finshed */
	UpdateStatus = UPDATE_UNLOCK;

	return;
}

/*************************************************************
 *  EndProc( )  free all the local memory & close socket   *
 *************************************************************/
static void EndProc ( void )
{
	tport_detach( &InRegion );
	tport_detach( &OutRegion );

	TrigListDestroy();
	StaListEnd();

	return;
}
