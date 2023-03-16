#ifdef _OS2
#define INCL_DOSMEMMGR
#define INCL_DOSSEMAPHORES
#include <os2.h>
#endif

/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
/* PGPLOT C library header */
#include <cpgplot.h>
/* Earthworm environment header include */
#include <earthworm.h>
#include <kom.h>
#include <transport.h>
#include <lockfile.h>
/* Local header include */
#include <datestring.h>
#include <griddata.h>
#include <recordtype.h>
#include <shakeint.h>
#include <postshake.h>
#include <postshake_msg_queue.h>
#include <postshake_plot.h>
#include <postshake_misc.h>

/* Functions in this source file
 *******************************/
static void postshake_config( char * );
static void postshake_lookup( void );
static void postshake_status( unsigned char, short, char * );
static void postshake_end( void );  /* Free all the local memory & close socket */

static thr_ret thread_proc_shake( void * );
static thr_ret thread_send_mail( void * );
static thr_ret thread_plot_shakemap( void * );

static int   write_mail_content( const PLOTSMAP *, const int );
static void  remove_shakemap( const char *, const char * );
static char *gen_script_command_args( char *, const PLOTSMAP * );

static SHM_INFO Region;      /* shared memory region to use for i/o    */

#define BUF_SIZE  150000     /* define maximum size for an event msg   */
#define MAXLOGO   MAX_IN_MSG

static MSG_LOGO Getlogo[MAXLOGO];   /* array for requesting module,type,instid */
static pid_t    MyPid;              /* for restarts by startstop               */

/* Thread things */
#define THREAD_STACK 8388608        /* 8388608 Byte = 8192 Kilobyte = 8 Megabyte */
#define THREAD_OFF    0				/* ComputePGA has not been started      */
#define THREAD_ALIVE  1				/* ComputePGA alive and well            */
#define THREAD_ERR   -1				/* ComputePGA encountered error quit    */
static sema_t  *SemaPtr;

#define MAX_PLOT_SHAKEMAPS      8
#define MAX_EMAIL_RECIPIENTS    20
#define MAX_POST_SCRIPTS        5
#define DEFAULT_SUBJECT_PREFIX "EEWS"

static volatile int ProcessShakeStatus = THREAD_OFF;
static volatile int MailProcessStatus  = THREAD_OFF;
static volatile int PlotMapStatus      = THREAD_OFF;
static volatile _Bool Finish = 0;

/* Things to read or derive from configuration file */
static char     RingName[MAX_RING_STR];		/* name of transport ring for i/o    */
static char     MyModName[MAX_MOD_STR];		/* speak as this module name/id      */
static uint8_t  LogSwitch;					/* 0 if no logfile should be written */
static uint64_t HeartBeatInterval;			/* seconds between heartbeats        */
static uint64_t QueueSize;					/* max messages in output circular buffer */
static uint8_t  RemoveSwitch = 0;
static uint64_t IssueInterval = 30;
static short    nLogo = 0;
static char     ReportPath[MAX_PATH_STR];
static char     SubjectPrefix[MAX_STR_SIZE];     /* defaults to EEWS, settable now */
static char     EmailProgram[MAX_PATH_STR];      /* Path to the email program */
static PLOTSMAP PlotShakeMaps[MAX_PLOT_SHAKEMAPS];
static uint8_t  NumPlotShakeMaps = 0;              /* Number of plotting shakemaps */
static char     EmailFromAddress[MAX_STR_SIZE] = { 0 };
static EMAILREC EmailRecipients[MAX_EMAIL_RECIPIENTS];
static uint8_t  NumEmailRecipients = 0;            /* Number of email recipients */
static POSCRIPT PostScripts[MAX_POST_SCRIPTS];
static uint8_t  NumPostScripts = 0;                /* Number of exec. scripts */
static char     LinkURLPrefix[MAX_PATH_STR];

/* Things to look up in the earthworm.h tables with getutil.c functions */
static long          RingKey;       /* key of transport ring for i/o     */
static unsigned char InstId;        /* local installation id             */
static unsigned char MyModId;       /* Module Id for this program        */
static unsigned char TypeHeartBeat;
static unsigned char TypeError;
static unsigned char TypeGridMap;

/* Error messages used by postshake */
#define ERR_MISSMSG       0   /* message missed in transport ring       */
#define ERR_TOOBIG        1   /* retreived msg too large for buffer     */
#define ERR_NOTRACK       2   /* msg retreived; tracking limit exceeded */
#define ERR_QUEUE         3   /* error queueing message for sending      */
static char  Text[150];       /* string for log/error messages          */

/*
 *
 */
int main( int argc, char **argv )
{
	time_t timeNow;          /* current time                  */
	time_t time_lastbeat;     /* time last heartbeat was sent  */
	char  *lockfile;
	int    lockfile_fd;

	int      res;
	long     recsize;      /* size of retrieved message     */
	MSG_LOGO reclogo;      /* logo of retrieved message     */

#if defined( _V710 )
	ew_thread_t tid;            /* Thread ID */
#else
	unsigned    tid;            /* Thread ID */
#endif
	GRIDMAP_HEADER *gmbuffer = NULL;

/* Check command line arguments */
	if ( argc != 2 ) {
		fprintf( stderr, "Usage: postshake <configfile>\n" );
		exit( 0 );
	}
	Finish = 1;
/* Initialize name of log-file & open it */
	logit_init( argv[1], 0, 256, 1 );
/* Read the configuration file(s) */
	postshake_config( argv[1] );
	logit("", "%s: Read command file <%s>\n", argv[0], argv[1]);
/* Look up important info from earthworm.h tables */
	postshake_lookup();
/* Reinitialize logit to desired logging level */
	logit_init( argv[1], 0, 256, LogSwitch );

	lockfile = ew_lockfile_path(argv[1]);
	if ( (lockfile_fd = ew_lockfile(lockfile) ) == -1) {
		fprintf(stderr, "one instance of %s is already running, exiting\n", argv[0]);
		exit(-1);
	}
/* Get process ID for heartbeat messages */
	MyPid = getpid();

	if ( MyPid == -1 ) {
		logit("e","postshake: Cannot get pid. Exiting.\n");
		exit (-1);
	}

/* Create a Mutex to control access to queue & initialize the message queue */
	/* CreateSemaphore_ew(); */ /* Obsoleted by Earthworm */
	SemaPtr = CreateSpecificSemaphore_ew( 0 );
	psk_msgqueue_init( QueueSize, MAX_GRIDMAP_SIZ + 1 );

/* Initialization */
	gmbuffer = (GRIDMAP_HEADER *)malloc(MAX_GRIDMAP_SIZ);

/* Attach to Input/Output shared memory ring */
	tport_attach( &Region, RingKey );
	logit("", "postshake: Attached to public memory region %s: %ld\n", RingName, RingKey );
/* Flush the transport ring */
	tport_flush( &Region, Getlogo, nLogo, &reclogo );

/* Force a heartbeat to be issued in first pass thru main loop */
	time_lastbeat = time(&timeNow) - HeartBeatInterval - 1;
/*----------------------- setup done; start main loop -------------------------*/
	while ( 1 ) {
	/* Send postshake's heartbeat */
		if ( time(&timeNow) - time_lastbeat >= (int64_t)HeartBeatInterval ) {
			time_lastbeat = timeNow;
			postshake_status( TypeHeartBeat, 0, "" );
		}

		if ( ProcessShakeStatus != THREAD_ALIVE ) {
			if ( StartThread(thread_proc_shake, (unsigned)THREAD_STACK, &tid) == -1 ) {
				logit("e", "postshake: Error starting ProcessShake thread; exiting!\n");
				tport_detach( &Region );
				postshake_end();
				exit(-1);
			}
			ProcessShakeStatus = THREAD_ALIVE;
		}

		do {
		/* See if a termination has been requested */
			res = tport_getflag( &Region );
			if ( res == TERMINATE || res == MyPid ) {
			/* Write a termination msg to log file */
				logit( "t", "postshake: Termination requested; exiting!\n" );
				fflush( stdout );
			/* */
				goto exit_procedure;
			}

		/* Get msg & check the return code from transport */
			res = tport_getmsg(&Region, Getlogo, nLogo, &reclogo, &recsize, (char *)gmbuffer, MAX_GRIDMAP_SIZ);
		/* No more new messages     */
			if ( res == GET_NONE ) {
				/* PostSemaphore(); */ /* Obsoleted by Earthworm */
				PostSpecificSemaphore_ew( SemaPtr );
				break;
			}
		/* Next message was too big */
			else if ( res == GET_TOOBIG ) {
			/* Complain and try again   */
				sprintf(
					Text, "Retrieved msg[%ld] (i%u m%u t%u) too big for Buffer[%ld]",
					recsize, reclogo.instid, reclogo.mod, reclogo.type, (long)MAX_GRIDMAP_SIZ
				);
				postshake_status( TypeError, ERR_TOOBIG, Text );
				continue;
			}
		/* Got a msg, but missed some */
			else if ( res == GET_MISS ) {
				sprintf(
					Text, "Missed msg(s)  i%u m%u t%u  %s.", reclogo.instid, reclogo.mod, reclogo.type, RingName
				);
				postshake_status( TypeError, ERR_MISSMSG, Text );
			}
		/* Got a msg, but can't tell */
			else if ( res == GET_NOTRACK ) {
			/* If any were missed        */
				sprintf(
					Text, "Msg received (i%u m%u t%u); transport.h NTRACK_GET exceeded",
					reclogo.instid, reclogo.mod, reclogo.type
				);
				postshake_status( TypeError, ERR_NOTRACK, Text );
			}

		/* Process the message */
			if ( reclogo.type == TypeGridMap ) {
			/* Debug */
				/* logit( "", "%s", rec ); */
				/* printf( "Maptype %d, Valuetype: %d, From %d to %d!\n",
					gmbuffer->maptype, gmbuffer->valuetype, gmbuffer->starttime, gmbuffer->endtime ); */
			/* Generate every given seconds or at the coda of event */
				if ( ((gmbuffer->endtime - gmbuffer->starttime) % IssueInterval ) && !gmbuffer->codaflag )
					break;

				res = psk_msgqueue_enqueue( gmbuffer, recsize, reclogo );
				/* PostSemaphore(); */ /* Obsoleted by Earthworm */
				PostSpecificSemaphore_ew( SemaPtr );
				if ( res ) {
					if ( res == -2 ) {  /* Serious: quit */
					/* Currently, eneueue() in mem_circ_queue.c never returns this error. */
						sprintf(Text, "internal queue error. Terminating.");
						postshake_status( TypeError, ERR_QUEUE, Text );
						postshake_end();
						exit(-1);
					}
					else if ( res == -1 ) {
						sprintf(Text, "queue cannot allocate memory. Lost message.");
						postshake_status( TypeError, ERR_QUEUE, Text );
					}
					else if ( res == -3 ) {
					/*
					 * Queue is lapped too often to be logged to screen.
					 * Log circular queue laps to logfile.
					 * Maybe queue laps should not be logged at all.
					 */
						logit("et", "postshake: Circular queue lapped. Messages lost!\n");
					}
				}
			}
		} while ( 1 );
		sleep_ew(50);  /* no more messages; wait for new ones to arrive */
	}  /* while( 1 ) */
/*-----------------------------end of main loop-------------------------------*/
exit_procedure:
	Finish = 0;
	/* PostSemaphore(); */ /* Obsoleted by Earthworm */
	PostSpecificSemaphore_ew( SemaPtr );
/* detach from shared memory */
	sleep_ew(500);
	postshake_end();

	ew_unlockfile(lockfile_fd);
	ew_unlink_lockfile(lockfile);

	return 0;
}

/*
 * postshake_config() - processes command file(s) using kom.c functions;
 *                      exits if any errors are encountered.
 */
static void postshake_config( char *configfile )
{
	int   ncommand;     /* # of required commands you expect to process   */
	char  init[10];     /* init flags, one byte for each required command */
	int   nmiss;        /* number of required commands that were missed   */
	char *com;
	char *str;
	int   nfiles;
	int   success;
	int   i;
	char  plfilename[MAX_PATH_STR];

/* Set to zero one init flag for each required command */
	ncommand = 9;
	for ( i = 0; i < ncommand; i++ )
		init[i] = 0;

	strcpy(EmailProgram, "\0");
	strcpy(SubjectPrefix, DEFAULT_SUBJECT_PREFIX);
	strcpy(LinkURLPrefix, "/");

/* Open the main configuration file */
	nfiles = k_open( configfile );
	if ( nfiles == 0 ) {
		logit("e", "postshake: Error opening command file <%s>; exiting!\n", configfile);
		exit(-1);
	}

/* Process all command files */
/* While there are command files open */
	while ( nfiles > 0 ) {
	/* Read next line from active file  */
		while ( k_rd() ) {
			com = k_str();  /* Get the first token from line */
		/* Ignore blank lines & comments */
			if( !com )
				continue;
			if( com[0] == '#' )
				continue;

		/* Open a nested configuration file */
			if ( com[0] == '@' ) {
				success = nfiles+1;
				nfiles  = k_open(&com[1]);
				if ( nfiles != success ) {
					logit("e", "postshake: Error opening command file <%s>; exiting!\n", &com[1]);
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
					strcpy( MyModName, str );
				init[1] = 1;
			}
		/* 2 */
			else if ( k_its("RingName") ) {
				str = k_str();
				if ( str )
					strcpy( RingName, str );
				init[2] = 1;
			}
		/* 3 */
			else if ( k_its("HeartBeatInterval") ) {
				HeartBeatInterval = k_long();
				init[3] = 1;
			}
		/* 4 */
			else if ( k_its("QueueSize") ) {
				QueueSize = k_long();
				init[4] = 1;
			}
			else if ( k_its("RemoveShakeMap") ) {
				RemoveSwitch = k_int();
				logit(
					"o", "postshake: This process will %s the files after posted.\n",
					RemoveSwitch ? "remove" : "keep"
				);
			}
		/* 5 */
			else if ( k_its("ReportPath") ) {
				str = k_str();
				if ( str )
					strcpy(ReportPath, str);
				if ( ReportPath[strlen(ReportPath) - 1] != '/' )
					strncat(ReportPath, "/", 1);

				logit("o", "postshake: Report path %s\n", ReportPath);
				init[5] = 1;
			}
		/* 6 */
			else if ( k_its("PlotShakeMap") ) {
				if ( NumPlotShakeMaps < MAX_PLOT_SHAKEMAPS ) {
				/* Processing of shaking type */
					str = k_str();
					if ( str )
						PlotShakeMaps[NumPlotShakeMaps].smaptype = shakestr2num( str );
					if ( PlotShakeMaps[NumPlotShakeMaps].smaptype == SHAKE_TYPE_COUNT ) {
						logit("e", "postshake: Unknown type of shakemap. Exiting!\n");
						exit(-1);
					}
				/* Processing of map title */
					str = k_str();
					if ( str ) {
						strcpy(PlotShakeMaps[NumPlotShakeMaps].title, str);
						PlotShakeMaps[NumPlotShakeMaps].title[MAX_STR_SIZE - 1] = '\0';
					}
					else {
						PlotShakeMaps[NumPlotShakeMaps].title[0] = '\0';
					}
				/* Processing of input message list */
					int _inputmsg = k_long();
					if ( _inputmsg > ((1 << MAX_IN_MSG) - 1) || _inputmsg < 1 ) {
						logit("e", "postshake: Excessive value of input messages(range is 1~255). Exiting!\n");
						exit(-1);
					}
					int _msgcount = 0;
					for ( i = 0; i < MAX_IN_MSG; i++, _inputmsg >>= 1 ) {
						if ( _inputmsg & 0x01 ) {
							PlotShakeMaps[NumPlotShakeMaps].gmflag[i] = 1;
							_msgcount++;
						/* Debug */
						/*
							printf("%s: %d\n", PlotShakeMaps[NumPlotShakeMaps].title, i);
						*/
						}
						else {
							PlotShakeMaps[NumPlotShakeMaps].gmflag[i] = 0;
						}
						PlotShakeMaps[NumPlotShakeMaps].gmptr[i] = NULL;
					}
					if ( _msgcount != shake_get_reqinputs(PlotShakeMaps[NumPlotShakeMaps].smaptype) ) {
						logit(
							"e", "postshake: The number of inputs is not correct(it should be %d for %s). Exiting!\n",
							shake_get_reqinputs( PlotShakeMaps[NumPlotShakeMaps].smaptype ),
							shakenum2str( PlotShakeMaps[NumPlotShakeMaps].smaptype )
						);
						exit(-1);
					}
				/* Processing of the map caption */
					str = k_str();
					if ( str ) {
						strcpy(PlotShakeMaps[NumPlotShakeMaps].caption, str);
						PlotShakeMaps[NumPlotShakeMaps].caption[MAX_STR_SIZE - 1] = '\0';
					}
					else {
						PlotShakeMaps[NumPlotShakeMaps].caption[0] = '\0';
					}
				/* */
					PlotShakeMaps[NumPlotShakeMaps].min_magnitude = DUMMY_MAG;
					logit("o", strlen(PlotShakeMaps[NumPlotShakeMaps].title) ? "postshake: Got #%d plotting shakemap type: %s, title: '%s'," :
						"postshake: Got #%d plotting shakemap type: %s,  no title%s,",
						NumPlotShakeMaps,
						shakenum2str( PlotShakeMaps[NumPlotShakeMaps].smaptype ),
						PlotShakeMaps[NumPlotShakeMaps].title);
					logit("o", strlen(PlotShakeMaps[NumPlotShakeMaps].caption) ? " caption: '%s', needs %d messages.\n" :
						" no caption%s, needs %d messages.\n",
						PlotShakeMaps[NumPlotShakeMaps].caption,
						_msgcount);
					NumPlotShakeMaps++;
				}
				else {
					logit("e", "postshake: Excessive number of plotting shakemaps. Exiting!\n");
					exit(-1);
				}
				init[6] = 1;
			}
		/* 7 */
			else if ( k_its("MapRange") ) {
				float range[4];
				for ( i = 0; i < 4; i++ )
					range[i] = k_val();

				psk_plot_init( range[0], range[1], range[2], range[3] );
				init[7] = 1;
			}
			else if ( k_its("IssueInterval") ) {
				IssueInterval = k_long();
				logit("o", "postshake: Real shakemap alarm interval change to %ld\n", IssueInterval);
			}
			else if ( k_its("NormalPolyLineFile") ) {
				str = k_str();
				if ( str )
					strcpy(plfilename, str);
				logit("o", "postshake: Normal polygon line file: %s\n", plfilename);

				if ( psk_plot_polyline_read( plfilename, PLOT_NORMAL_POLY ) ) {
					logit("e", "postshake: Error reading normal polygon line file; exiting!\n");
					exit(-1);
				}
				else {
					logit("o", "postshake: Reading normal polygon line file finish!\n");
				}
			}
			else if ( k_its("StrongPolyLineFile") ) {
				str = k_str();
				if ( str )
					strcpy(plfilename, str);
				logit("o", "postshake: Strong polygon line file: %s\n", plfilename);

				if ( psk_plot_polyline_read( plfilename, PLOT_STRONG_POLY ) ) {
					logit("e", "postshake: Error reading strong polygon line file; exiting!\n");
					exit(-1);
				}
				else {
					logit("o", "postshake: Reading strong polygon line file finish!\n");
				}
			}
			else if ( k_its("EmailProgram") ) {
				str = k_str();
				if ( str )
					strcpy(EmailProgram, str);
				logit("o", "postshake: Email program %s\n", EmailProgram);
			}
			else if ( k_its("SubjectPrefix") ) {
				str = k_str();
				if ( str )
					strcpy(SubjectPrefix, str);
				logit("o", "postshake: Email subject prefix change to \"%s\"\n", SubjectPrefix);
			}
			else if ( k_its("EmailRecipientWithMinMag") ) {
				if ( NumEmailRecipients < MAX_EMAIL_RECIPIENTS ) {
					str = k_str();
					if ( str )
						strcpy(EmailRecipients[NumEmailRecipients].address, str);
					EmailRecipients[NumEmailRecipients].min_magnitude = k_val();
					NumEmailRecipients++;
				}
				else {
					logit("e", "postshake: Excessive number of email recipients. Exiting!\n");
					exit(-1);
				}
			}
			else if ( k_its("EmailRecipient") ) {
				if ( NumEmailRecipients < MAX_EMAIL_RECIPIENTS ) {
					str = k_str();
					if ( str )
						strcpy(EmailRecipients[NumEmailRecipients].address, str);
					EmailRecipients[NumEmailRecipients].min_magnitude = DUMMY_MAG;
					NumEmailRecipients++;
				}
				else {
					logit("e", "postshake: Excessive number of email recipients. Exiting!\n");
					exit(-1);
				}
			}
			else if ( k_its("EmailFromAddress") ) {
				str = k_str();
				if ( str )
					strcpy(EmailFromAddress, str);
				logit("o", "postshake: Email from address setted to \"%s\"\n", EmailFromAddress);
			}
			else if ( k_its("LinkURLPrefix") ) {
				str = k_str();
				if ( str )
					strcpy(LinkURLPrefix, str);

				if ( LinkURLPrefix[strlen(LinkURLPrefix) - 1] != '/' )
					strncat(LinkURLPrefix, "/", 1);
			}
			else if ( k_its("PostScriptWithMinMag") ) {
				if ( NumPostScripts < MAX_POST_SCRIPTS ) {
					str = k_str();
					if ( str )
						strcpy(PostScripts[NumPostScripts].script, str);
					PostScripts[NumPostScripts].min_magnitude = k_val();
					logit(
						"o", "postshake: Using script(%d): %s!\n",
						NumPostScripts, PostScripts[NumPostScripts].script
					);
					NumPostScripts++;
				}
				else {
					logit("e", "postshake: Excessive number of post scripts. Exiting!\n");
					exit(-1);
				}
			}
			else if ( k_its("PostScript") ) {
				if ( NumPostScripts < MAX_POST_SCRIPTS ) {
					str = k_str();
					if ( str )
						strcpy(PostScripts[NumPostScripts].script, str);
					PostScripts[NumPostScripts].min_magnitude = DUMMY_MAG;
					logit(
						"o", "postshake: Using script(%d): %s!\n", NumPostScripts, PostScripts[NumPostScripts].script
					);
					NumPostScripts++;
				}
				else {
					logit("e", "postshake: Excessive number of post scripts. Exiting!\n");
					exit(-1);
				}
			}
		/* Enter installation & module to get event messages from */
		/* 8 */
			else if( k_its("GetEventsFrom") ) {
				if ( nLogo >= MAXLOGO ) {
					logit("e", "postshake: Too many <GetEventsFrom> commands in <%s>", configfile);
					logit("e", "; max=%d; exiting!\n", (int) MAXLOGO);
					exit(-1);
				}
				if ( (str = k_str()) ) {
					if ( GetInst(str, &Getlogo[nLogo].instid) != 0 ) {
						logit("e", "trace2peak: Invalid installation name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				if ( (str = k_str()) ) {
					if ( strcmp(str, "MOD_WILDCARD") == 0 ) {
						logit("e", "postshake: This module do not accept MOD_WILDCARD; exiting!\n");
						exit(-1);
					}
					if ( GetModId(str, &Getlogo[nLogo].mod) != 0 ) {
						logit("e", "trace2peak: Invalid module name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				if ( (str = k_str()) ) {
					if ( GetType(str, &Getlogo[nLogo].type) != 0 ) {
						logit("e", "trace2peak: Invalid message type name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				nLogo++;
				init[8] = 1;
			}
		/* Unknown command */
			else {
				logit("e", "postshake: <%s> Unknown command in <%s>.\n", com, configfile);
				continue;
			}

		/* See if there were any errors processing the command */
			if ( k_err() ) {
				logit("e", "postshake: Bad <%s> command in <%s>; exiting!\n", com, configfile);
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
		logit( "e", "postshake: ERROR, no " );
		if ( !init[0] ) logit("e", "<LogFile> "           );
		if ( !init[1] ) logit("e", "<MyModuleId> "        );
		if ( !init[2] ) logit("e", "<RingName> "          );
		if ( !init[3] ) logit("e", "<HeartBeatInterval> " );
		if ( !init[4] ) logit("e", "<QueueSize> "         );
		if ( !init[5] ) logit("e", "<ReportPath> "        );
		if ( !init[6] ) logit("e", "any <PlotShakeMap> "  );
		if ( !init[7] ) logit("e", "<MapRange> "          );
		if ( !init[8] ) logit("e", "any <GetEventsFrom> " );
		logit("e", "command(s) in <%s>; exiting!\n", configfile);
		exit(-1);
	}

	return;
}

/*
 * postshake_lookup() - Look up important info from earthworm.h tables
 */
static void postshake_lookup( void )
{
/* Look up keys to shared memory regions */
   if( ( RingKey = GetKey(RingName) ) == -1 ) {
	   fprintf(stderr, "postshake:  Invalid ring name <%s>; exiting!\n", RingName);
	   exit(-1);
   }
   /* Look up installations of interest */
   	if ( GetLocalInst(&InstId) != 0 ) {
   		fprintf(stderr, "postshake: error getting local installation id; exiting!\n");
   		exit(-1);
   	}
/* Look up modules of interest */
	if ( GetModId(MyModName, &MyModId) != 0 ) {
		fprintf(stderr, "postshake: Invalid module name <%s>; exiting!\n", MyModName);
		exit(-1);
	}
/* Look up message types of interest */
	if ( GetType("TYPE_HEARTBEAT", &TypeHeartBeat) != 0 ) {
		fprintf(stderr, "postshake: Invalid message type <TYPE_HEARTBEAT>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_ERROR", &TypeError) != 0 ) {
		fprintf(stderr, "postshake: Invalid message type <TYPE_ERROR>; exiting!\n");
		exit(-1);
	}
	if ( GetType( "TYPE_GRIDMAP", &TypeGridMap ) != 0 ) {
		fprintf(stderr, "postshake: Invalid message type <TYPE_GRIDMAP>; exiting!\n");
		exit(-1);
	}

	return;
}

/*
 * postshake_status() - builds a heartbeat or error message & puts it into
 *                      shared memory.  Writes errors to log file & screen.
 */
static void postshake_status( unsigned char type, short ierr, char *note )
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
	else if( type == TypeError ) {
		sprintf(msg, "%ld %hd %s\n", (long)t, ierr, note);
		logit("et", "postshake: %s\n", note);
	}

	size = strlen(msg);  /* don't include the null byte in the message */

/* Write the message to shared memory */
	if ( tport_putmsg(&Region, &logo, size, msg) != PUT_OK ) {
		if ( type == TypeHeartBeat ) {
			logit("et", "postshake:  Error sending heartbeat.\n");
		}
		else if ( type == TypeError ) {
			logit("et", "postshake:  Error sending error:%d.\n", ierr);
		}
	}

	return;
}

/*
 * postshake_end() - free all the local memory & close queue
 */
static void postshake_end( void )
{
	tport_detach( &Region );
	psk_plot_end();
	psk_msgqueue_end();
	/* DestroySemaphore(); */ /* Obsoleted by Earthworm */
	DestroySpecificSemaphore_ew( SemaPtr );

	return;
}

/*
 * thread_proc_shake() - Read the station info from the array <stainfo>. And creates
 *                       the table of postshake of Taiwan.
 */
static thr_ret thread_proc_shake( void *dummy )
{
	int      i, j;
	int      nready;
	int      res;
	long     recsize;                        /* Size of retrieved message from queue */
	MSG_LOGO reclogo;                        /* logo of retrieved message     */
	char     command[MAX_PATH_STR * 4];
	char     script_args[MAX_PATH_STR * 2];

	double          max_mag = DUMMY_MAG;
	GRIDMAP_HEADER *gmbuffer[nLogo];
	GRIDMAP_HEADER *gmtmp, *gmref;;
#if defined( _V710 )
	ew_thread_t     tid[2];  /* Thread id used to control */
#else
	unsigned        tid[2];  /* Thread id used to control */
#endif

/* Tell the main thread we're ok */
	ProcessShakeStatus = THREAD_ALIVE;

/* Initialization */
	for ( i = 0; i < nLogo; i++ ) {
		gmbuffer[i] = (GRIDMAP_HEADER *)malloc(MAX_GRIDMAP_SIZ);
		memset(gmbuffer[i], 0, MAX_GRIDMAP_SIZ);
		for ( j = 0; j < NumPlotShakeMaps; j++ ) {
			if ( PlotShakeMaps[j].gmflag[i] )
				PlotShakeMaps[j].gmptr[i] = gmbuffer[i];
		}
	}
	gmref = gmtmp = (GRIDMAP_HEADER *)malloc(MAX_GRIDMAP_SIZ);
	nready = 0;

	do {
		/* WaitSemPost(); */ /* Obsoleted by Earthworm */
		WaitSpecificSemaphore_ew(SemaPtr);

		res = psk_msgqueue_dequeue( gmtmp, &recsize, &reclogo );

		if ( res == 0 ) {
			for ( i = 0; i < nLogo; i++ ) {
				if ( reclogo.mod == Getlogo[i].mod ) {
					if ( gmbuffer[i]->endtime == 0 && abs(gmref->endtime - gmtmp->endtime) < 2 ) {
						memcpy(gmbuffer[i], gmtmp, recsize);
						if ( nready++ == 0 )
							gmref = gmbuffer[i];
					}
					else {
						if ( (res = psk_msgqueue_enqueue( gmtmp, recsize, reclogo )) ) {
							if ( res == -3 )
								logit("et", "postshake: ProcessShake's circular queue lapped. Messages lost!\n");
						}
					}
					break;
				}
			}
		}

		if ( nready == nLogo ) {
		/* Basically, we use the first ref. value of the first one plotting shakemap as the base */
			if ( (gmref = psk_misc_refmap_get( PlotShakeMaps )) == NULL ) {
				logit("e", "postshake: Can't find the grid reference in the first plotting shakemap!\n");
				exit(-1);
			}
		/* And get the maximum magnitude */
			for ( i = 0; i < 4; i++ )
				if ( gmref->magnitude[i] > max_mag )
					max_mag = gmref->magnitude[i];

		/* Generate the plotting function's threads */
			if ( StartThread( thread_plot_shakemap, (unsigned)THREAD_STACK, &tid[0] ) == -1 ) {
				logit("e", "postshake: Error starting PlotShakemap_thr thread!\n");
			}
			PlotMapStatus = THREAD_ALIVE;

		/* Processing the alert Email */
			if ( strlen( EmailProgram ) > 0 && NumEmailRecipients ) {
			/* Write the content of Email */
				if ( write_mail_content( PlotShakeMaps, NumPlotShakeMaps ) < 0 ) {
					logit("e", "postshake: Error writing the content of alert mail!\n");
				}
			/* Send the alert Email */
				if ( StartThreadWithArg( thread_send_mail, gmref, (unsigned)THREAD_STACK, &tid[1] ) == -1 ) {
					logit("e", "postshake: Error starting SendShakeMail thread!\n");
				}
				MailProcessStatus = THREAD_ALIVE;
			}

			while ( PlotMapStatus != THREAD_OFF ) {
				printf("postshake: Waiting for the shakemap plotting...\n");
				sleep_ew(200);
			}

		/* Post to the Facebook or other place by external script */
		 	gen_script_command_args( script_args, PlotShakeMaps );
			for ( i = 0; i < NumPostScripts; i++ ) {
				if ( PostScripts[i].min_magnitude <= max_mag ) {
					logit("o", "postshake: Executing the script: '%s'\n", PostScripts[i].script);
					sprintf(command, "%s %s", PostScripts[i].script, script_args);
				/* Execute system command to post shakemap */
					if ( system(command) )
						logit("e", "postshake: Execute the script: '%s' error, please check it!\n", PostScripts[i].script);
					else
						logit("t", "postshake: Execute the script: '%s' success!\n", PostScripts[i].script);
				}
			}

			while ( MailProcessStatus != THREAD_OFF ) {
				printf("postshake: Waiting for the mail sending...\n");
				sleep_ew(250);
			}

		/* Remove the plotted shakemaps */
			if ( RemoveSwitch ) {
				for ( i = 0; i < NumPlotShakeMaps; i++ ) {
					psk_misc_smfilename_gen( PlotShakeMaps + i, script_args, MAX_STR_SIZE );
					remove_shakemap( ReportPath, script_args );
				}
			}

			max_mag = DUMMY_MAG;
			nready  = 0;
			gmref   = gmtmp;

			for ( i = 0; i < nLogo; i++ )
				memset(gmbuffer[i], 0, MAX_GRIDMAP_SIZ);
		}
	} while ( Finish );

	for ( i = 0; i < nLogo; i++ )
		free(gmbuffer[i]);
	free(gmtmp);

/* we're quitting */
	ProcessShakeStatus = THREAD_ERR;   /* file a complaint to the main thread */
	KillSelfThread();                  /* main thread will restart us */

	return NULL;
}

/*
*/
static int write_mail_content( const PLOTSMAP *psm, const int numofpsm )
{
	int    i;
	int    trigstations = -1;
	struct tm *tp = NULL;
	char   resfilename[MAX_STR_SIZE];
	char   stimestring[MAX_DSTR_LENGTH];
	char   etimestring[MAX_DSTR_LENGTH];

	FILE            *htmlfile;                      /* HTML file */
	GRIDMAP_HEADER  *gmref = psk_misc_refmap_get( psm );

/* Open the temporary header file */
	if( (htmlfile = fopen( "postshake_mail_html.tmp", "w" )) != NULL ) {
	/* Generate start & end timestamp */
		tp = gmtime( &gmref->starttime );
		date2spstring( tp, stimestring, MAX_DSTR_LENGTH );
		tp = gmtime( &gmref->endtime );
		date2spstring( tp, etimestring, MAX_DSTR_LENGTH );

	/* Get the triggered stations */
		trigstations = psk_misc_trigstations_get( psm );

		fprintf( htmlfile, "<!DOCTYPE html>\n" );
		fprintf( htmlfile, "<html>\n<body>\n" );
		fprintf( htmlfile, "<p><b>All the information has not been confirmed yet. For reference only!</b></p>\n" );
		fprintf( htmlfile, "<p><table><tr><td>From:</td><td>%s</td></tr>\n", stimestring );
		fprintf( htmlfile, "<tr><td>To:</td><td>%s</td></tr>\n", etimestring );
		fprintf( htmlfile, "<tr><td>Trig Sta:</td><td>%d</td></tr>\n", trigstations );
		fprintf( htmlfile, "<tr><td>Mag25:</td><td>%4.1lf</td></tr>\n", gmref->magnitude[0] );
		fprintf( htmlfile, "<tr><td>Mag80:</td><td>%4.1lf</td></tr>\n", gmref->magnitude[1] );
		fprintf( htmlfile, "<tr><td>Mag250:</td><td>%4.1lf</td></tr>\n", gmref->magnitude[2] );
		fprintf( htmlfile, "<tr><td>Mag400:</td><td>%4.1lf</td></tr></table></p>\n", gmref->magnitude[3] );

	/* Extra content about ftp */
		for ( i = 0; i < numofpsm; i++ ) {
		/* Generate file name */
			psk_misc_smfilename_gen( psm + i, resfilename, MAX_STR_SIZE );
			fprintf(htmlfile, "<p><a href=\'%sshakemap/img/%s\'><b>Download \"%s\"</b></a></p>\n",
				LinkURLPrefix, resfilename, (psm + i)->title);
		}

		fprintf( htmlfile, "<p><a href=\'%sevents/\'><b>Download Waveform</b></a></p>\n", LinkURLPrefix );
		fprintf( htmlfile, "<p><b>This mail is automatically generated by our EEWS. Do not reply!</b></p>\n" );
		fprintf( htmlfile, "</body>\n</html>\n" );
		fclose( htmlfile );
	}
	else {
		logit( "e", "postshake: Email content file create error!\n" );
	}

	return trigstations;
}

/*
*/
static thr_ret thread_send_mail( void *arg )
{
	int i;

	char   etimestring[MAX_DSTR_LENGTH];
	char   command[MAX_STR_SIZE];  /* System command to call email prog */
	char   _emailfrom[MAX_STR_SIZE] = { 0 };
	double max_mag = DUMMY_MAG;

	struct tm      *tp  = NULL;
	GRIDMAP_HEADER *gmh = (GRIDMAP_HEADER *)arg;
	FILE           *headerfile;              /* Email header file */

	tp = gmtime( &gmh->endtime );
	date2spstring( tp, etimestring, MAX_DSTR_LENGTH );

	MailProcessStatus = THREAD_ALIVE;

	logit( "o", "postshake: Processing email alerts...\n" );

	strcpy(_emailfrom, strlen(EmailFromAddress) ? EmailFromAddress : "EEW System <eews@eews.com>");

	for ( i = 0; i < 4; i++ )
		if ( gmh->magnitude[i] > max_mag ) max_mag = gmh->magnitude[i];

/* One email for each recipient */
	for ( i = 0; i < NumEmailRecipients; i++ ) {
		if ( EmailRecipients[i].min_magnitude <= max_mag ) {
		/* Create email header file
		 **************************/
			if ( (headerfile = fopen( "postshake_mail_header.tmp", "w" )) != NULL ) {
				fprintf( headerfile, "From: %s\n", _emailfrom );
				fprintf( headerfile, "To: %s\n", EmailRecipients[i].address );
				fprintf( headerfile, "Subject: %s shakemap report. Time ID: %s\n", SubjectPrefix, etimestring );
				fprintf( headerfile, "Message-ID: %s\n", etimestring );
				fprintf( headerfile, "Content-Type: text/html\n" );
				fprintf( headerfile, "MIME-Version: 1.0\n\n" );
				fclose( headerfile );

			/* System command for sendmail without attachments */
				sprintf( command,"cat postshake_mail_header.tmp postshake_mail_html.tmp | %s -t ",
						 EmailProgram );
			}
			else {
				logit("e", "postshake: Email header file create error!\n");
				continue;
			}

		/* Execute system command to send email
		 **************************************/
			if ( system(command) )
				logit( "e", "postshake: Email sent error, please check it!\n" );
			else
				logit( "t", "postshake: Email sent to <%s>!\n", EmailRecipients[i].address );
		}
	}

/* we're quitting
 *****************/
	MailProcessStatus = THREAD_OFF;			/* fire a complaint to the main thread */
	KillSelfThread();
	return NULL;
}

/*
*/
static thr_ret thread_plot_shakemap( void *dummy )
{
	int  i;
	char resfilename[MAX_STR_SIZE];

	PlotMapStatus = THREAD_ALIVE;

	for ( i = 0; i < NumPlotShakeMaps; i++ ) {
	/* Generate the result file name */
		psk_misc_smfilename_gen( PlotShakeMaps + i, resfilename, MAX_STR_SIZE );
	/* Plot the shakemap by the plotting functions like PGPLOT or GMT etc. */
		if ( psk_plot_sm_plot( PlotShakeMaps + i, ReportPath, resfilename ) < 0 ) {
			logit( "e", "postshake: Plotting shakemap error; skip it!\n" );
		}
	}

/* we're quitting
 *****************/
	PlotMapStatus = THREAD_OFF;  /* fire a complaint to the main thread */
	KillSelfThread();
	return NULL;
}

/*
*/
static void remove_shakemap( const char *reportpath, const char *resfilename )
{
	char fullfilepath[MAX_PATH_STR*2];

	sprintf(fullfilepath, "%s%s", reportpath, resfilename);
	remove(fullfilepath);

	return;
}

/*
 *
 */
static char *gen_script_command_args( char *buffer, const PLOTSMAP *psm )
{
	int             i;
	GRIDMAP_HEADER *gmref   = psk_misc_refmap_get( psm );
	double          max_mag = DUMMY_MAG;
	struct tm      *tp      = NULL;
	char            filename[MAX_PATH_STR];
	char            starttime[MAX_DSTR_LENGTH];
	char            endtime[MAX_DSTR_LENGTH];
	time_t          reptime;

/* Generate the timestamp */
	tp = localtime(&gmref->starttime);
	date2spstring( tp, starttime, MAX_DSTR_LENGTH );
	tp = localtime(&gmref->endtime);
	date2spstring( tp, endtime, MAX_DSTR_LENGTH );
	reptime = gmref->codaflag ? -1 : gmref->endtime - gmref->starttime;
/* And get the maximum magnitude */
	for ( i = 0; i < 4; i++ )
		if ( gmref->magnitude[i] > max_mag )
			max_mag = gmref->magnitude[i];

/* Command arguments for executing script */
	sprintf(
		buffer, "%s %s %ld %f %d ", starttime, endtime, reptime, max_mag, psk_misc_trigstations_get( psm )
	);
/* Generate the result file names */
	for ( i = 0; i < NumPlotShakeMaps; i++ ) {
		psk_misc_smfilename_gen( psm + i, filename, MAX_STR_SIZE );
		strcat(buffer, ReportPath);
		strcat(buffer, filename);
		strcat(buffer, " ");
	}

	return buffer;
}
