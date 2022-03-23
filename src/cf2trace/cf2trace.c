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
#include <time.h>

/* Earthworm environment header include */
#include <earthworm.h>
#include <kom.h>
#include <transport.h>
#include <lockfile.h>
#include <trace_buf.h>
#include <swap.h>

/* Local header include */
#include <stalist.h>
#include <cf2trace.h>
#include <cf2trace_list.h>


/* Functions prototype in this source file
 *******************************/
void  cf2trace_config ( char * );
void  cf2trace_lookup ( void );
void  cf2trace_status ( unsigned char, short, char * );

static thr_ret UpdateTraceList ( void * );

static void EndProc ( void );                /* Free all the local memory & close socket */

/* Ring messages things */
static  SHM_INFO  InRegion;      /* shared memory region to use for i/o    */
static  SHM_INFO  OutRegion;     /* shared memory region to use for i/o    */

#define MAXLOGO 5

MSG_LOGO  Putlogo;                /* array for output module, type, instid     */
MSG_LOGO  Getlogo[MAXLOGO];                /* array for requesting module, type, instid */
pid_t     myPid;                  /* for restarts by startstop                 */

/* Thread things */
#define THREAD_STACK 8388608         /* 8388608 Byte = 8192 Kilobyte = 8 Megabyte */
#define THREAD_OFF    0              /* Thread has not been started      */
#define THREAD_ALIVE  1              /* Thread alive and well            */
#define THREAD_ERR   -1              /* Thread encountered error quit    */

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

/* Things to look up in the earthworm.h tables with getutil.c functions
 **********************************************************************/
static int64_t InRingKey;       /* key of transport ring for i/o     */
static int64_t OutRingKey;      /* key of transport ring for i/o     */
static uint8_t InstId;          /* local installation id             */
static uint8_t MyModId;         /* Module Id for this program        */
static uint8_t TypeHeartBeat;
static uint8_t TypeError;
static uint8_t TypeTracebuf2 = 0;

/* Error messages used by cf2trace
 *********************************/
#define  ERR_MISSMSG       0   /* message missed in transport ring       */
#define  ERR_TOOBIG        1   /* retreived msg too large for buffer     */
#define  ERR_NOTRACK       2   /* msg retreived; tracking limit exceeded */
#define  ERR_QUEUE         3   /* error queueing message for sending      */
static char Text[150];         /* string for log/error messages          */

static volatile uint8_t UpdateStatus = 0;

/* struct timespec TT, TT2; */            /* Nanosecond Timer */


int main ( int argc, char **argv )
{
	int res;

	int64_t  recsize = 0;

	MSG_LOGO   reclogo;
	time_t     timeNow;          /* current time                  */
	time_t     timeLastBeat;     /* time last heartbeat was sent  */

	char   *lockfile;
	int32_t lockfile_fd;

	_TRACEINFO   *traceptr;
	TracePacket   tracebuffer_i;  /* message which is received from share ring    */
	TracePacket   tracebuffer_o;  /* message which is sent to share ring    */
#if defined( _V710 )
	ew_thread_t   tid;            /* Thread ID */
#else
	unsigned      tid;            /* Thread ID */
#endif

/* Check command line arguments
 ******************************/
	if ( argc != 2 ) {
		fprintf( stderr, "Usage: cf2trace <configfile>\n" );
		exit( 0 );
	}

	UpdateStatus = 0;

/* Initialize name of log-file & open it
 ***************************************/
	logit_init( argv[1], 0, 256, 1 );

/* Read the configuration file(s)
 ********************************/
	cf2trace_config( argv[1] );
	logit( "" , "%s: Read command file <%s>\n", argv[0], argv[1] );

/* Look up important info from earthworm.h tables
 ************************************************/
	cf2trace_lookup();

/* Reinitialize logit to desired logging level
 **********************************************/
	logit_init( argv[1], 0, 256, LogSwitch );

	lockfile = ew_lockfile_path(argv[1]);
	if ( (lockfile_fd = ew_lockfile(lockfile) ) == -1 ) {
		fprintf(stderr, "one instance of %s is already running, exiting\n", argv[0]);
		exit (-1);
	}
/*
	fprintf(stderr, "DEBUG: for %s, fd=%d for %s, LOCKED\n", argv[0], lockfile_fd, lockfile);
*/

/* Get process ID for heartbeat messages */
	myPid = getpid();
	if( myPid == -1 ) {
		logit("e","cf2trace: Cannot get pid. Exiting.\n");
		exit (-1);
	}

/* Build the message
 *******************/
	Putlogo.instid = InstId;
	Putlogo.mod    = MyModId;
	Putlogo.type   = TypeTracebuf2;

/* Attach to Input/Output shared memory ring
 *******************************************/

	tport_attach( &InRegion, InRingKey );
	logit( "", "cf2trace: Attached to public memory region %s: %ld\n",
		InRingName, InRingKey );

/* Flush the transport ring */
	tport_flush( &InRegion, Getlogo, nLogo, &reclogo );

	tport_attach( &OutRegion, OutRingKey );
	logit( "", "cf2trace: Attached to public memory region %s: %ld\n",
		OutRingName, OutRingKey );


/* Force a heartbeat to be issued in first pass thru main loop
 *************************************************************/
	timeLastBeat = time(&timeNow) - HeartBeatInterval - 1;

/* Initialize traces list */
	TraListInit( ListRingName, MyModName, ListFromRingSwitch );
	if ( TraListFetch() <= 0 ) {
		logit("e", "cf2trace: Cannot get the list of traces. Exiting!\n");
		EndProc();
		exit(-1);
	}

/*----------------------- setup done; start main loop -------------------------*/
	while(1)
	{
	/* Send cf2trace's heartbeat
	***************************/
		if  ( time(&timeNow) - timeLastBeat >= (int64_t)HeartBeatInterval ) {
			timeLastBeat = timeNow;
			cf2trace_status( TypeHeartBeat, 0, "" );
		}

		if ( UpdateStatus == 1 ) {
			UpdateStatus = 2;
			if ( StartThread( UpdateTraceList, (uint32_t)THREAD_STACK, &tid ) == -1 ) {
				logit( "e", "cf2trace: Error starting the thread(UpdateTraceList)!\n" );
				UpdateStatus = 0;
			}
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
				logit( "t", "cf2trace: Termination requested; exiting!\n" );
				fflush( stdout );
			/* should check the return of these if we really care */
			/*
				fprintf(stderr, "DEBUG: %s, fd=%d for %s\n", argv[0], lockfile_fd, lockfile);
			*/
			/* detach from shared memory */
				sleep_ew(100);
				EndProc();

				ew_unlockfile(lockfile_fd);
				ew_unlink_lockfile(lockfile);
				exit( 0 );
			}

		/* Get msg & check the return code from transport
		 ************************************************/
			res = tport_getmsg( &InRegion, Getlogo, nLogo, &reclogo, &recsize, tracebuffer_i.msg, MAX_TRACEBUF_SIZ );

			if ( res == GET_NONE )			/* no more new messages     */
			{
				break;
			}
			else if ( res == GET_TOOBIG )	/* next message was too big */
			{								/* complain and try again   */
				sprintf( Text,
						"Retrieved msg[%ld] (i%u m%u t%u) too big for Buffer[%ld]",
						recsize, reclogo.instid, reclogo.mod, reclogo.type, sizeof(tracebuffer_i)-1 );
				cf2trace_status( TypeError, ERR_TOOBIG, Text );
				continue;
			}
			else if ( res == GET_MISS )		/* got a msg, but missed some */
			{
				sprintf( Text,
						"Missed msg(s)  i%u m%u t%u  %s.",
						reclogo.instid, reclogo.mod, reclogo.type, InRingName );
				cf2trace_status( TypeError, ERR_MISSMSG, Text );
			}
			else if ( res == GET_NOTRACK )	/* got a msg, but can't tell */
			{								/* if any were missed        */
				sprintf( Text,
						"Msg received (i%u m%u t%u); transport.h NTRACK_GET exceeded",
						reclogo.instid, reclogo.mod, reclogo.type );
				cf2trace_status( TypeError, ERR_NOTRACK, Text );
			}

		/* Process the message
		*********************/
			if ( reclogo.type == TypeTracebuf2 ) {
				if ( TRACE2_HEADER_VERSION_IS_20(&tracebuffer_i.trh2) ) {
				/* Swap the byte order to local order */
					if ( WaveMsg2MakeLocal( &tracebuffer_i.trh2 ) ) {
						logit("e", "cf2trace: %s.%s.%s.%s byte order swap error, please check it!\n",
							tracebuffer_i.trh2.sta, tracebuffer_i.trh2.chan, tracebuffer_i.trh2.net, tracebuffer_i.trh2.loc);
						continue;
					}
					tracebuffer_i.trh2x.version[1] = TRACE2_VERSION11;
				}
				else if ( TRACE2_HEADER_VERSION_IS_21(&tracebuffer_i.trh2) ) {
				/* Swap the byte order to local order */
					if ( WaveMsg2XMakeLocal( &tracebuffer_i.trh2x ) ) {
						logit("e", "cf2trace: %s.%s.%s.%s byte order swap error, please check it!\n",
							tracebuffer_i.trh2x.sta, tracebuffer_i.trh2x.chan, tracebuffer_i.trh2x.net, tracebuffer_i.trh2x.loc);
						continue;
					}
				}
				else {
					logit("e", "cf2trace: %s.%s.%s.%s version is invalid, please check it!\n",
						tracebuffer_i.trh2.sta, tracebuffer_i.trh2.chan, tracebuffer_i.trh2.net, tracebuffer_i.trh2.loc);
					continue;
				}

				traceptr = TraListFind( &tracebuffer_i.trh2x );

				if ( traceptr == NULL ) {
				/* Not found in trace table */
					//printf("cf2trace: %s.%s.%s.%s not found in trace table, maybe it's a new trace.\n",
						//tracebuffer_i.trh2.sta, tracebuffer_i.trh2.chan, tracebuffer_i.trh2.net, tracebuffer_i.trh2.loc);
				/* Force to update the table */
					if ( UpdateStatus == 0 ) UpdateStatus = 1;
					continue;
				}
			/* Copy all the header data into the output buffer */
				memcpy(tracebuffer_o.msg, tracebuffer_i.msg, sizeof(TRACE2X_HEADER));
			/* */
				tracebuffer_o.trh2x.pinno                   = traceptr->recordtype; /* Temporally use this space to store the record type*/
				tracebuffer_o.trh2x.x.v21.conversion_factor = traceptr->conversion_factor;
			/* */
#if defined( _SPARC )
				strcpy(tracebuffer_o.trh2x.datatype, "t4");      /* SUN IEEE single precision real */
#elif defined( _INTEL )
				strcpy(tracebuffer_o.trh2x.datatype, "f4");      /* VAX/Intel IEEE single precision real */
#else
				logit( "e", "cf2trace warning: _INTEL and _SPARC are both undefined." );
#endif

			/* */
				float *tracedata_f = (float *)(&tracebuffer_o.trh2x + 1);
			/* */
				if ( tracebuffer_i.trh2x.datatype[1] == '4' ) {
					int32_t *tracedata_i = (int32_t *)(&tracebuffer_i.trh2x + 1);
					int32_t *tracedata_iend = tracedata_i + tracebuffer_i.trh2x.nsamp;
				/* */
					for ( ; tracedata_i < tracedata_iend; tracedata_i++, tracedata_f++ )
						*tracedata_f = *tracedata_i * tracebuffer_o.trh2x.x.v21.conversion_factor;
				}
			/* */
				else if ( tracebuffer_i.trh2x.datatype[1] == '2' ) {
					int16_t *tracedata_s = (int16_t *)(&tracebuffer_i.trh2x + 1);
					int16_t *tracedata_send = tracedata_s + tracebuffer_i.trh2x.nsamp;
				/* */
					for ( ; tracedata_s < tracedata_send; tracedata_s++, tracedata_f++ )
						*tracedata_f = *tracedata_s * tracebuffer_o.trh2x.x.v21.conversion_factor;
				}
			/* */
				else {
					/* Do nothing */
				}
			/* */
				if ( tport_putmsg( &OutRegion, &Putlogo, recsize, tracebuffer_o.msg ) != PUT_OK ) {
					logit( "e", "cf2trace: Error putting message in region %ld\n", OutRegion.key );
				}
			}

			/* logit( "", "%s", res ); */   /*debug*/

		} while( 1 );  /* end of message-processing-loop */

		sleep_ew( 50 );  /* no more messages; wait for new ones to arrive */
	}
/*-----------------------------end of main loop-------------------------------*/
	EndProc();
	return 0;
}

/******************************************************************************
 *  cf2trace_config() processes command file(s) using kom.c functions;       *
 *                    exits if any errors are encountered.                    *
 ******************************************************************************/
void cf2trace_config ( char *configfile )
{
	char  init[7];     /* init flags, one byte for each required command */
	char *com;
	char *str;

	uint32_t ncommand;     /* # of required commands you expect to process   */
	uint32_t nmiss;        /* number of required commands that were missed   */
	uint32_t nfiles;
	uint32_t success;
	uint32_t i;

/* Set to zero one init flag for each required command
 *****************************************************/
   ncommand = 7;
   for( i=0; i<ncommand; i++ )  init[i] = 0;

/* Open the main configuration file
 **********************************/
   nfiles = k_open( configfile );
   if ( nfiles == 0 ) {
		logit( "e",
				"cf2trace: Error opening command file <%s>; exiting!\n",
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
						  "cf2trace: Error opening command file <%s>; exiting!\n",
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
	/*2*/   else if( k_its("InWaveRing") ) {
				str = k_str();
				if(str) strcpy( InRingName, str );
				init[2] = 1;
			}
	/*3*/   else if( k_its("OutWaveRing") ) {
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
	/*5*/   else if( k_its("GetListFrom") ) {
				char tmpstr[256] = { 0 };

				str = k_str();
				if ( str ) {
					strcpy( tmpstr, str );
					str = k_str();

					if ( str ) {
						if ( TraListReg( tmpstr, str ) ) {
							logit( "e", "cf2trace: Error occur when register station list, exiting!\n" );
						}
					}
				}
				init[5] = 1;
			}
		/* Enter installation & module to get event messages from
		********************************************************/
	/*6*/   else if( k_its("GetEventsFrom") ) {
				if ( nLogo+1 >= MAXLOGO ) {
					logit( "e",
							"cf2trace: Too many <GetEventsFrom> commands in <%s>",
							configfile );
					logit( "e", "; max=%d; exiting!\n", (int) MAXLOGO );
					exit( -1 );
				}
				if( ( str=k_str() ) ) {
					if( GetInst( str, &Getlogo[nLogo].instid ) != 0 ) {
						logit( "e",
								"cf2trace: Invalid installation name <%s>", str );
						logit( "e", " in <GetEventsFrom> cmd; exiting!\n" );
						exit( -1 );
					}
				}
				if( ( str=k_str() ) ) {
					if( GetModId( str, &Getlogo[nLogo].mod ) != 0 ) {
						logit( "e",
								"cf2trace: Invalid module name <%s>", str );
						logit( "e", " in <GetEventsFrom> cmd; exiting!\n" );
						exit( -1 );
					}
				}
				if( ( str=k_str() ) ) {
					if( GetType( str, &Getlogo[nLogo].type ) != 0 ) {
						logit( "e",
								"cf2trace: Invalid message type name <%s>", str );
						logit( "e", " in <GetEventsFrom> cmd; exiting!\n" );
						exit( -1 );
					}
				}
				nLogo++;
				init[6] = 1;

				/*printf("Getlogo[%d] inst:%d module:%d type:%d\n", nLogo,
					(int)Getlogo[nLogo].instid,
					(int)Getlogo[nLogo].mod,
					(int)Getlogo[nLogo].type );*/   /*DEBUG*/
			}

		 /* Unknown command
		  *****************/
			else {
				logit( "e", "cf2trace: <%s> Unknown command in <%s>.\n",
						 com, configfile );
				continue;
			}

		/* See if there were any errors processing the command
		 *****************************************************/
			if( k_err() ) {
			   logit( "e",
					   "cf2trace: Bad <%s> command in <%s>; exiting!\n",
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
		logit( "e", "cf2trace: ERROR, no " );
		if ( !init[0] )  logit( "e", "<LogFile> "           );
		if ( !init[1] )  logit( "e", "<MyModuleId> "        );
		if ( !init[2] )  logit( "e", "<InWaveRing> "        );
		if ( !init[3] )  logit( "e", "<OutWaveRing> "       );
		if ( !init[4] )  logit( "e", "<HeartBeatInterval> " );
		if ( !init[5] )  logit( "e", "any <GetListFrom> "   );
		if ( !init[6] )  logit( "e", "any <GetEventsFrom> " );

		logit( "e", "command(s) in <%s>; exiting!\n", configfile );
		exit( -1 );
	}

	return;
}

/******************************************************************************
 *  cf2trace_lookup( )   Look up important info from earthworm.h tables       *
 ******************************************************************************/
void cf2trace_lookup ( void )
{
/* Look up keys to shared memory regions
*************************************/
	if( ( InRingKey = GetKey(InRingName) ) == -1 ) {
		fprintf( stderr,
				"cf2trace:  Invalid ring name <%s>; exiting!\n", InRingName);
		exit( -1 );
	}

	if( ( OutRingKey = GetKey(OutRingName) ) == -1 ) {
		fprintf( stderr,
				"cf2trace:  Invalid ring name <%s>; exiting!\n", OutRingName);
		exit( -1 );
	}

/* Look up installations of interest
*********************************/
	if ( GetLocalInst( &InstId ) != 0 ) {
		fprintf( stderr,
				"cf2trace: error getting local installation id; exiting!\n" );
		exit( -1 );
	}

/* Look up modules of interest
***************************/
	if ( GetModId( MyModName, &MyModId ) != 0 ) {
		fprintf( stderr,
				"cf2trace: Invalid module name <%s>; exiting!\n", MyModName );
		exit( -1 );
	}

/* Look up message types of interest
*********************************/
	if ( GetType( "TYPE_HEARTBEAT", &TypeHeartBeat ) != 0 ) {
		fprintf( stderr,
				"cf2trace: Invalid message type <TYPE_HEARTBEAT>; exiting!\n" );
		exit( -1 );
	}
	if ( GetType( "TYPE_ERROR", &TypeError ) != 0 ) {
		fprintf( stderr,
				"cf2trace: Invalid message type <TYPE_ERROR>; exiting!\n" );
		exit( -1 );
	}
	if ( GetType( "TYPE_TRACEBUF2", &TypeTracebuf2 ) != 0 ) {
		fprintf( stderr,
				"cf2trace: Invalid message type <TYPE_TRACEBUF2>; exiting!\n" );
		exit( -1 );
	}

	return;
}

/******************************************************************************
 * cf2trace_status() builds a heartbeat or error message & puts it into      *
 *                   shared memory.  Writes errors to log file & screen.      *
 ******************************************************************************/
void cf2trace_status ( unsigned char type, short ierr, char *note )
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
		logit( "et", "cf2trace: %s\n", note );
   }

   size = strlen( msg );   /* don't include the null byte in the message */

/* Write the message to shared memory
 ************************************/
   if( tport_putmsg( &InRegion, &logo, size, msg ) != PUT_OK )
   {
		if( type == TypeHeartBeat ) {
		   logit("et","cf2trace:  Error sending heartbeat.\n" );
		}
		else if( type == TypeError ) {
		   logit("et","cf2trace:  Error sending error:%d.\n", ierr );
		}
   }

   return;
}

/******************************************************************************
 * UpdateTraceList() ...                      *
 ******************************************************************************/
static thr_ret
UpdateTraceList ( void *dummy )
{
	logit("o", "cf2trace: Start to updating the list of traces.\n");

	if ( TraListFetch() <= 0 ) {
		logit("e", "cf2trace: Cannot update the list of traces.\n");
	}

/* Just wait for 30 seconds */
	sleep_ew(30000);

/* Tell other threads that update is finshed */
	UpdateStatus = 0;

	KillSelfThread(); /* Just exit this thread */

	return NULL;
}


/*************************************************************
 *  EndProc( )  free all the local memory & close socket   *
 *************************************************************/
static void EndProc ( void )
{
	tport_detach( &InRegion );
	tport_detach( &OutRegion );

	TraListEnd();

	return;
}
