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
#include <scnlfilter.h>
#include <dif2trace.h>
#include <dif2trace_list.h>
#include <dif2trace_filter.h>

/* Functions prototype in this source file */
static void dif2trace_config( char * );
static void dif2trace_lookup( void );
static void dif2trace_status( unsigned char, short, char * );
static void dif2trace_end( void );                /* Free all the local memory & close socket */

static void init_traceinfo( const TRACE2X_HEADER *, const uint8_t, _TRACEINFO * );
static void operation_rmavg( _TRACEINFO *, TracePacket * );
static void operation_diff( _TRACEINFO *, TracePacket * );
static void operation_ddiff( _TRACEINFO *, TracePacket * );
static void operation_int( _TRACEINFO *, TracePacket * );
static void operation_dint( _TRACEINFO *, TracePacket * );

/* Ring messages things */
static SHM_INFO InRegion;      /* shared memory region to use for i/o    */
static SHM_INFO OutRegion;     /* shared memory region to use for i/o    */
/* */
#define MAXLOGO 5
static MSG_LOGO Putlogo;             /* array for output module, type, instid     */
static MSG_LOGO Getlogo[MAXLOGO];    /* array for requesting module, type, instid */
static pid_t    MyPid;               /* for restarts by startstop                 */

#define OPERATION_DIFF  0
#define OPERATION_DDIFF 1
#define OPERATION_INT   2
#define OPERATION_DINT  3

/* Things to read or derive from configuration file */
static char     InRingName[MAX_RING_STR];   /* name of transport ring for i/o    */
static char     OutRingName[MAX_RING_STR];  /* name of transport ring for i/o    */
static char     MyModName[MAX_MOD_STR];     /* speak as this module name/id      */
static uint8_t  LogSwitch;                  /* 0 if no logfile should be written */
static uint64_t HeartBeatInterval;          /* seconds between heartbeats        */
static uint8_t  OperationFlag;              /* 0 for diff, 1 for int */
static uint16_t DriftCorrectThreshold;      /* seconds waiting for D.C. */
static uint16_t nLogo = 0;
static uint8_t  HighPassOrder  = 2;         /* Order for high pass filter */
static double   HighPassCorner = 0.075;     /* Corner frequency for high pass filter */
static uint8_t  SCNLFilterSwitch = 0;       /* 0 if no filter command in the file    */

/* Things to look up in the earthworm.h tables with getutil.c functions */
static int64_t InRingKey;       /* key of transport ring for i/o     */
static int64_t OutRingKey;      /* key of transport ring for i/o     */
static uint8_t InstId;          /* local installation id             */
static uint8_t MyModId;         /* Module Id for this program        */
static uint8_t TypeHeartBeat;
static uint8_t TypeError;
static uint8_t TypeTracebuf2 = 0;

/* Error messages used by dif2trace */
#define  ERR_MISSMSG       0   /* message missed in transport ring       */
#define  ERR_TOOBIG        1   /* retreived msg too large for buffer     */
#define  ERR_NOTRACK       2   /* msg retreived; tracking limit exceeded */
#define  ERR_QUEUE         3   /* error queueing message for sending      */
static char Text[150];         /* string for log/error messages          */

/*
 *
 */
int main ( int argc, char **argv )
{
	int res;

	int64_t  recsize = 0;
	MSG_LOGO reclogo;
	time_t   time_now;          /* current time                  */
	time_t   time_last_beat;     /* time last heartbeat was sent  */
	char    *lockfile;
	int32_t  lockfile_fd;

	_TRACEINFO *traceptr;
	TracePacket tracebuffer;  /* message which is sent to share ring    */
	const void *_match = NULL;

	double tmp_time;

	int8_t operationdirc = 0;
	void (*operationfunc)( _TRACEINFO *, TracePacket * ) = NULL;

/* Check command line arguments */
	if ( argc != 2 ) {
		fprintf( stderr, "Usage: dif2trace <configfile>\n" );
		exit( 0 );
	}
/* Initialize name of log-file & open it */
	logit_init( argv[1], 0, 256, 1 );
/* Read the configuration file(s) */
	dif2trace_config( argv[1] );
	logit( "" , "%s: Read command file <%s>\n", argv[0], argv[1] );
/* Look up important info from earthworm.h tables */
	dif2trace_lookup();
/* Reinitialize logit to desired logging level */
	logit_init( argv[1], 0, 256, LogSwitch );

	lockfile = ew_lockfile_path(argv[1]);
	if ( (lockfile_fd = ew_lockfile(lockfile) ) == -1 ) {
		fprintf(stderr, "one instance of %s is already running, exiting\n", argv[0]);
		exit (-1);
	}
/* Initialize the SCNL filter required arguments */
	if ( SCNLFilterSwitch )
		scnlfilter_init( "dif2trace" );
/* Get process ID for heartbeat messages */
	MyPid = getpid();
	if ( MyPid == -1 ) {
		logit("e","dif2trace: Cannot get pid. Exiting.\n");
		exit (-1);
	}

/* Build the message */
	Putlogo.instid = InstId;
	Putlogo.mod    = MyModId;
	Putlogo.type   = TypeTracebuf2;

/* Attach to Input/Output shared memory ring */
	tport_attach( &InRegion, InRingKey );
	logit("", "dif2trace: Attached to public memory region %s: %ld\n", InRingName, InRingKey);
/* Flush the transport ring */
	tport_flush( &InRegion, Getlogo, nLogo, &reclogo );
/* */
	tport_attach( &OutRegion, OutRingKey );
	logit("", "dif2trace: Attached to public memory region %s: %ld\n", OutRingName, OutRingKey);

/* Initialization of filter and operation function */
	switch ( OperationFlag ) {
	case OPERATION_DIFF:
		operationfunc = operation_diff;
		operationdirc = 1;
		break;
	case OPERATION_DDIFF:
		operationfunc = operation_ddiff;
		operationdirc = 2;
		break;
	case OPERATION_INT:
		operationfunc = operation_int;
		operationdirc = -1;
		break;
	case OPERATION_DINT:
		operationfunc = operation_dint;
		operationdirc = -2;
		break;
	default:
		logit("e", "dif2trace: Unknown operation type, exiting!\n");
		exit(-1);
	}
/* Initialize the filter parameters, use the butterworth high pass filter */
	if ( OperationFlag == OPERATION_INT || OperationFlag == OPERATION_DINT ) {
		if ( dif2tra_filter_init( HighPassOrder, HighPassCorner, 0.0, IIR_HIGHPASS_FILTER, IIR_BUTTERWORTH ) ) {
			logit("e", "dif2trace: Initialize the high pass filter error, exiting!\n");
			exit(-1);
		}
	}
/* Force a heartbeat to be issued in first pass thru main loop */
	time_last_beat = time(&time_now) - HeartBeatInterval - 1;
/*----------------------- setup done; start main loop -------------------------*/
	while ( 1 ) {
	/* Send dif2trace's heartbeat */
		if  ( time(&time_now) - time_last_beat >= (int64_t)HeartBeatInterval ) {
			time_last_beat = time_now;
			dif2trace_status( TypeHeartBeat, 0, "" );
		}

	/* Process all new messages */
		do {
		/* See if a termination has been requested */
			res = tport_getflag( &InRegion );
			if ( res == TERMINATE || res == MyPid ) {
			/* write a termination msg to log file */
				logit("t", "dif2trace: Termination requested; exiting!\n");
				fflush(stdout);
			/* */
				goto exit_procedure;
			}

		/* Get msg & check the return code from transport */
			res = tport_getmsg(&InRegion, Getlogo, nLogo, &reclogo, &recsize, tracebuffer.msg, MAX_TRACEBUF_SIZ);
		/* No more new messages     */
			if ( res == GET_NONE ) {
				break;
			}
		/* Next message was too big */
			else if ( res == GET_TOOBIG ) {
			/* Complain and try again   */
				sprintf(
					Text, "Retrieved msg[%ld] (i%u m%u t%u) too big for Buffer[%ld]",
					recsize, reclogo.instid, reclogo.mod, reclogo.type, sizeof(tracebuffer) - 1
				);
				dif2trace_status( TypeError, ERR_TOOBIG, Text );
				continue;
			}
		/* Got a msg, but missed some */
			else if ( res == GET_MISS ) {
				sprintf(
					Text, "Missed msg(s)  i%u m%u t%u  %s.", reclogo.instid, reclogo.mod, reclogo.type, InRingName
				);
				dif2trace_status( TypeError, ERR_MISSMSG, Text );
			}
		/* Got a msg, but can't tell */
			else if ( res == GET_NOTRACK ) {
			/* If any were missed        */
				sprintf(
					Text, "Msg received (i%u m%u t%u); transport.h NTRACK_GET exceeded",
					reclogo.instid, reclogo.mod, reclogo.type
				);
				dif2trace_status( TypeError, ERR_NOTRACK, Text );
			}

		/* Process the message */
			if ( reclogo.type == TypeTracebuf2 ) {
				if ( !TRACE2_HEADER_VERSION_IS_21(&(tracebuffer.trh2)) ) {
					printf(
						"dif2trace: SCNL %s.%s.%s.%s version is invalid, please check it!\n",
						tracebuffer.trh2.sta, tracebuffer.trh2.chan, tracebuffer.trh2.net, tracebuffer.trh2.loc
					);
					continue;
				}
			/* */
				if ( tracebuffer.trh2x.datatype[0] != 'f' && tracebuffer.trh2x.datatype[0] != 't' ) {
					printf(
						"dif2trace: SCNL %s.%s.%s.%s datatype[%s] is invalid, skip it!\n",
						tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc,
						tracebuffer.trh2x.datatype
					);
					continue;
				}
			/* Initialize for in-list checking */
				traceptr = NULL;
			/* If this trace is already inside the local list, it would skip the SCNL filter */
				if (
					SCNLFilterSwitch &&
					!(traceptr = dif2tra_list_find( &tracebuffer.trh2x )) &&
					!scnlfilter_trace_apply( tracebuffer.msg, reclogo.type, &_match )
				) {
				#ifdef _SEW_DEBUG
					printf("dif2trace: Found SCNL %s.%s.%s.%s but not in the filter, drop it!\n",
					tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc);
				#endif
					continue;
				}
			/* If we can't get the trace pointer to the local list, search it again */
				if ( !traceptr && !(traceptr = dif2tra_list_search( &tracebuffer.trh2x )) ) {
				/* Error when insert into the tree */
					logit(
						"e", "dif2trace: SCNL %s.%s.%s.%s insert into trace tree error, drop this trace.\n",
						tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc
					);
					continue;
				}

			/* First time initialization */
				if ( traceptr->firsttime || fabs(1.0 / tracebuffer.trh2x.samprate - traceptr->delta) > FLT_EPSILON ) {
					printf(
						"dif2trace: New SCNL %s.%s.%s.%s received, starting to trace!\n",
						traceptr->sta, traceptr->chan, traceptr->net, traceptr->loc
					);
					init_traceinfo( &tracebuffer.trh2x, OperationFlag, traceptr );
				}
			/* Remap the SCNL of this incoming trace, if we turned it on */
				if ( SCNLFilterSwitch ) {
					if ( traceptr->match ) {
						scnlfilter_trace_remap( tracebuffer.msg, reclogo.type, traceptr->match );
					}
					else {
						if ( scnlfilter_trace_remap( tracebuffer.msg, reclogo.type, _match ) ) {
							printf(
								"dif2trace: Remap received trace SCNL %s.%s.%s.%s to %s.%s.%s.%s!\n",
								traceptr->sta, traceptr->chan, traceptr->net, traceptr->loc,
								tracebuffer.trh2x.sta, tracebuffer.trh2x.chan,
								tracebuffer.trh2x.net, tracebuffer.trh2x.loc
							);
						}
						traceptr->match = _match;
					}
				}
			/* Start processing the gap in trace */
				if ( fabs(tmp_time = tracebuffer.trh2x.starttime - traceptr->lasttime) > traceptr->delta * 2.0 ) {
					if ( (time_t)tracebuffer.trh2x.starttime > (time(&time_now) + 3) ) {
					#ifdef _SEW_DEBUG
						printf( "dif2trace: %s.%s.%s.%s NTP sync error, drop it!\n",
						tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc );
					#endif
						continue;
					}
					else if ( tmp_time < 0.0 ) {
					#ifdef _SEW_DEBUG
						printf( "dif2trace: Overlapped in %s.%s.%s.%s, drop it!\n",
						tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc );
					#endif
						continue;
					}
					else if ( tmp_time > 0.0 && traceptr->lasttime > 0.0 )	{
						if ( tmp_time >= DriftCorrectThreshold ) {
							printf(
								"dif2trace: Found %ld sample gap in SCNL %s.%s.%s.%s, restart tracing!\n",
								(long)(tmp_time * tracebuffer.trh2x.samprate),
								tracebuffer.trh2x.sta, tracebuffer.trh2x.chan,
								tracebuffer.trh2x.net, tracebuffer.trh2x.loc
							);
						/* Due to large gap, try to restart this trace */
							init_traceinfo( &tracebuffer.trh2x, OperationFlag, traceptr );
						}
					#ifdef _SEW_DEBUG
						else {
							printf( "dif2trace: Found %ld sample gap in %s.%s.%s.%s!\n",
								(long)(tmp_time * tracebuffer.trh2x.samprate),
								tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc );
						}
					#endif
					}
				}

			/* Record the time that the trace last updated */
				traceptr->lasttime = tracebuffer.trh2x.endtime;
			/* Wait for the D.C. */
				if ( traceptr->readycount >= DriftCorrectThreshold ) {
				/* Do the main operaion */
					operationfunc( traceptr, &tracebuffer );
				/* Modify the trace header to indicate that the data already been processed */
					tracebuffer.trh2x.pinno += operationdirc;
				/*
				 * Dump the new trace into output message,
				 * and send the packed message to the output ring
				 */
					if ( tport_putmsg( &OutRegion, &Putlogo, recsize, tracebuffer.msg ) != PUT_OK )
						logit("e", "dif2trace: Error putting message in region %ld\n", OutRegion.key);
				}
				else {
					traceptr->readycount += (uint16_t)(tracebuffer.trh2x.nsamp / tracebuffer.trh2x.samprate + 0.5);
				/* Only do the average removing before D.C complete */
					operation_rmavg( traceptr, &tracebuffer );
				/* */
					if ( traceptr->readycount >= DriftCorrectThreshold ) {
						printf(
							"dif2trace: SCNL %s.%s.%s.%s initialization of D.C. complete!\n",
							tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc
						);
					}
				}
			}
		} while ( 1 );  /* end of message-processing-loop */
		sleep_ew(50);  /* no more messages; wait for new ones to arrive */
	}
/*-----------------------------end of main loop-------------------------------*/
exit_procedure:
/* detach from shared memory */
	dif2trace_end();

	ew_unlockfile(lockfile_fd);
	ew_unlink_lockfile(lockfile);

	return 0;
}

/*
 * dif2trace_config() - processes command file(s) using kom.c functions;
 *                      exits if any errors are encountered.
 */
static void dif2trace_config( char *configfile )
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
	for( i = 0; i < ncommand; i++ )
		init[i] = 0;
/* Open the main configuration file */
	nfiles = k_open( configfile );
	if ( nfiles == 0 ) {
		logit("e", "dif2trace: Error opening command file <%s>; exiting!\n", configfile);
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
					logit("e", "dif2trace: Error opening command file <%s>; exiting!\n", &com[1]);
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
			else if ( k_its("InWaveRing") ) {
				str = k_str();
				if ( str )
					strcpy(InRingName, str);
				init[2] = 1;
			}
		/* 3 */
			else if ( k_its("OutWaveRing") ) {
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
		/* 5 */
			else if ( k_its("OperationType") ) {
				str = k_str();
				if ( str ) {
					char typestr[8];
				/* */
					strncpy(typestr, str, 8);
					for ( i = 0; i < 8; i++ )
						typestr[i] = tolower(typestr[i]);

					if ( !strncmp(typestr, "diff", 4) ) {
						OperationFlag = OPERATION_DIFF;
					}
					else if ( !strncmp(typestr, "ddiff", 5) ) {
						OperationFlag = OPERATION_DDIFF;
					}
					else if ( !strncmp(typestr, "int", 3) ) {
						OperationFlag = OPERATION_INT;
					}
					else if ( !strncmp(typestr, "dint", 4) ) {
						OperationFlag = OPERATION_DINT;
					}
					else {
						logit("e", "dif2trace: Invalid operation type <%s>", str);
						logit("e", " in <OperationType> cmd; exiting!\n");
						exit(-1);
					}
				}
				init[5] = 1;
			}
		/* 6 */
			else if ( k_its("DriftCorrectThreshold") ) {
				DriftCorrectThreshold = k_long();
				init[6] = 1;
			}
			else if ( k_its("HighPassOrder") ) {
				HighPassOrder = k_int();
				logit("o", "dif2trace: Order of high pass filter change to %d\n", HighPassOrder);
			}
			else if ( k_its("HighPassCorner") ) {
				HighPassCorner = k_val();
				logit("o", "dif2trace: Corner frequency of high pass filter change to %lf\n", HighPassCorner);
			}
		/* Enter installation & module to get event messages from */
		/* 7 */
			else if ( k_its("GetEventsFrom") ) {
				if ( (nLogo + 1) >= MAXLOGO ) {
					logit("e", "dif2trace: Too many <GetEventsFrom> commands in <%s>", configfile);
					logit("e", "; max=%d; exiting!\n", (int)MAXLOGO);
					exit(-1);
				}
				if ( (str = k_str()) ) {
					if ( GetInst(str, &Getlogo[nLogo].instid) != 0 ) {
						logit("e", "dif2trace: Invalid installation name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				if ( (str = k_str()) ) {
					if( GetModId(str, &Getlogo[nLogo].mod) != 0 ) {
						logit("e", "dif2trace: Invalid module name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				if ( (str = k_str()) ) {
					if( GetType( str, &Getlogo[nLogo].type ) != 0 ) {
						logit("e", "dif2trace: Invalid message type name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				nLogo++;
				init[7] = 1;
			}
			else if ( scnlfilter_com( "dif2trace" ) ) {
			/* */
				SCNLFilterSwitch = 1;
			}
		/* Unknown command */
			else {
				logit("e", "dif2trace: <%s> Unknown command in <%s>.\n", com, configfile);
				continue;
			}

		/* See if there were any errors processing the command */
			if ( k_err() ) {
				logit("e", "dif2trace: Bad <%s> command in <%s>; exiting!\n", com, configfile);
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
		logit( "e", "dif2trace: ERROR, no " );
		if ( !init[0] ) logit("e", "<LogFile> "              );
		if ( !init[1] ) logit("e", "<MyModuleId> "           );
		if ( !init[2] ) logit("e", "<InputRing> "            );
		if ( !init[3] ) logit("e", "<OutputRing> "           );
		if ( !init[4] ) logit("e", "<HeartBeatInterval> "    );
		if ( !init[5] ) logit("e", "<OperationType> "        );
		if ( !init[6] ) logit("e", "<DriftCorrectThreshold> ");
		if ( !init[7] ) logit("e", "any <GetEventsFrom> "    );

		logit("e", "command(s) in <%s>; exiting!\n", configfile);
		exit(-1);
	}

	return;
}

/*
 * dif2trace_lookup() - Look up important info from earthworm.h tables
 */
static void dif2trace_lookup( void )
{
/* Look up keys to shared memory regions */
	if ( ( InRingKey = GetKey(InRingName) ) == -1 ) {
		fprintf(stderr, "dif2trace:  Invalid ring name <%s>; exiting!\n", InRingName);
		exit(-1);
	}
	if ( ( OutRingKey = GetKey(OutRingName) ) == -1 ) {
		fprintf(stderr, "dif2trace:  Invalid ring name <%s>; exiting!\n", OutRingName);
		exit(-1);
	}
/* Look up installations of interest */
	if ( GetLocalInst(&InstId) != 0 ) {
		fprintf(stderr, "dif2trace: error getting local installation id; exiting!\n");
		exit(-1);
	}
/* Look up modules of interest */
	if ( GetModId(MyModName, &MyModId) != 0 ) {
		fprintf(stderr, "dif2trace: Invalid module name <%s>; exiting!\n", MyModName);
		exit(-1);
	}
/* Look up message types of interest */
	if ( GetType("TYPE_HEARTBEAT", &TypeHeartBeat) != 0 ) {
		fprintf(stderr, "dif2trace: Invalid message type <TYPE_HEARTBEAT>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_ERROR", &TypeError) != 0 ) {
		fprintf(stderr, "dif2trace: Invalid message type <TYPE_ERROR>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_TRACEBUF2", &TypeTracebuf2) != 0 ) {
		fprintf(stderr, "dif2trace: Invalid message type <TYPE_TRACEBUF2>; exiting!\n");
		exit(-1);
	}

	return;
}

/*
 * dif2trace_status() - builds a heartbeat or error message & puts it into
 *                      shared memory.  Writes errors to log file & screen.
 */
static void dif2trace_status( unsigned char type, short ierr, char *note )
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
		logit("et", "dif2trace: %s\n", note);
	}

	size = strlen(msg);  /* don't include the null byte in the message */

/* Write the message to shared memory */
	if ( tport_putmsg(&InRegion, &logo, size, msg) != PUT_OK ) {
		if ( type == TypeHeartBeat ) {
			logit("et", "dif2trace:  Error sending heartbeat.\n");
		}
		else if ( type == TypeError ) {
			logit("et", "dif2trace:  Error sending error:%d.\n", ierr);
		}
	}

	return;
}

/*
 * dif2trace_end() - free all the local memory & close socket
 */
static void dif2trace_end( void )
{
	tport_detach( &InRegion );
	tport_detach( &OutRegion );
	scnlfilter_end( NULL );
	dif2tra_filter_end();
	dif2tra_list_end();

	return;
}

/*
 *
 */
static void init_traceinfo( const TRACE2X_HEADER *trh2x, const uint8_t opflag, _TRACEINFO *traceptr )
{
	traceptr->firsttime     = FALSE;
	traceptr->readycount    = 0;
	traceptr->lasttime      = 0.0;
	traceptr->lastsample[0] = 0.0;
	traceptr->lastsample[1] = 0.0;
	traceptr->lastsample[2] = 0.0;
	traceptr->average       = 0.0;
	traceptr->delta         = 1.0 / trh2x->samprate;
/* Prepare the filter & stage space for integration */
	if ( opflag == OPERATION_INT || opflag == OPERATION_DINT ) {
		if ( traceptr->stage != (IIR_STAGE *)NULL )
			free(traceptr->stage);
		traceptr->filter = dif2tra_filter_search( traceptr );
		traceptr->stage  = dif2tra_filter_stage_create( traceptr );
	}

	return;
}

/**
 * @brief
 *
 */
#define OPERATION_RMAVG_MACRO(_DATA_PTR_TYPE, _TRACE_PTR, _TBUFF_PTR) \
		__extension__({ \
			_DATA_PTR_TYPE _dataptr = (_DATA_PTR_TYPE)(&(_TBUFF_PTR)->trh2x + 1); \
			_DATA_PTR_TYPE _dataptr_end = _dataptr + (_TBUFF_PTR)->trh2x.nsamp; \
			for ( ; _dataptr < _dataptr_end; _dataptr++ ) { \
				(_TRACE_PTR)->average += 0.001 * (*_dataptr - (_TRACE_PTR)->average); \
				*_dataptr -= (_TRACE_PTR)->average; \
			} \
		})
/**
 * @brief
 *
 * @param traceptr
 * @param tbufferptr
 */
static void operation_rmavg( _TRACEINFO *traceptr, TracePacket *tbufferptr )
{
/* Go through all the double precision data */
	if ( tbufferptr->trh2x.datatype[1] == '8' )
		OPERATION_RMAVG_MACRO( double *, traceptr, tbufferptr );
/* Go through all the single precision data */
	else
		OPERATION_RMAVG_MACRO( float *, traceptr, tbufferptr );

	return;
}

/**
 * @brief
 *
 */
#define OPERATION_DIFF_MACRO(_DATA_PTR_TYPE, _TRACE_PTR, _TBUFF_PTR) \
		__extension__({ \
			double _result; \
			_DATA_PTR_TYPE _dataptr = (_DATA_PTR_TYPE)(&(_TBUFF_PTR)->trh2x + 1); \
			_DATA_PTR_TYPE _dataptr_end = _dataptr + (_TBUFF_PTR)->trh2x.nsamp; \
			for ( ; _dataptr < _dataptr_end; _dataptr++ ) { \
				(_TRACE_PTR)->average += 0.001 * (*_dataptr - (_TRACE_PTR)->average); \
				*_dataptr -= (_TRACE_PTR)->average; \
				_result = (*_dataptr - (_TRACE_PTR)->lastsample[0]) / (_TRACE_PTR)->delta; \
				(_TRACE_PTR)->lastsample[0] = *_dataptr; \
				(_TRACE_PTR)->lastsample[1] = _result; \
				*_dataptr = _result; \
			} \
		})
/**
 * @brief
 *
 * @param traceptr
 * @param tbufferptr
 */
static void operation_diff( _TRACEINFO *traceptr, TracePacket *tbufferptr )
{
/* Go through all the double precision data */
	if ( tbufferptr->trh2x.datatype[1] == '8' )
		OPERATION_DIFF_MACRO( double *, traceptr, tbufferptr );
/* Go through all the single precision data */
	else
		OPERATION_DIFF_MACRO( float *, traceptr, tbufferptr );

	return;
}

/**
 * @brief
 *
 */
#define OPERATION_DDIFF_MACRO(_DATA_PTR_TYPE, _TRACE_PTR, _TBUFF_PTR) \
		__extension__({ \
			double _result; \
			double _tmp; \
			_DATA_PTR_TYPE _dataptr = (_DATA_PTR_TYPE)(&(_TBUFF_PTR)->trh2x + 1); \
			_DATA_PTR_TYPE _dataptr_end = _dataptr + (_TBUFF_PTR)->trh2x.nsamp; \
			for ( ; _dataptr < _dataptr_end; _dataptr++ ) { \
				(_TRACE_PTR)->average += 0.001 * (*_dataptr - (_TRACE_PTR)->average); \
				*_dataptr -= (_TRACE_PTR)->average; \
				_tmp = (*_dataptr - (_TRACE_PTR)->lastsample[0]) / (_TRACE_PTR)->delta; \
				_result = (_tmp - (_TRACE_PTR)->lastsample[1]) / (_TRACE_PTR)->delta; \
				(_TRACE_PTR)->lastsample[0] = *_dataptr; \
				(_TRACE_PTR)->lastsample[1] = _tmp; \
				(_TRACE_PTR)->lastsample[2] = _result; \
				*_dataptr = _result; \
			} \
		})
/**
 * @brief
 *
 * @param traceptr
 * @param tbufferptr
 */
static void operation_ddiff( _TRACEINFO *traceptr, TracePacket *tbufferptr )
{
/* Go through all the double precision data */
	if ( tbufferptr->trh2x.datatype[1] == '8' )
		OPERATION_DDIFF_MACRO( double *, traceptr, tbufferptr );
/* Go through all the single precision data */
	else
		OPERATION_DDIFF_MACRO( float *, traceptr, tbufferptr );

	return;
}

/**
 * @brief
 *
 */
#define OPERATION_INT_MACRO(_DATA_PTR_TYPE, _TRACE_PTR, _TBUFF_PTR) \
		__extension__({ \
			const double _halfdelta = (_TRACE_PTR)->delta * 0.5; \
			double _result; \
			_DATA_PTR_TYPE _dataptr = (_DATA_PTR_TYPE)(&(_TBUFF_PTR)->trh2x + 1); \
			_DATA_PTR_TYPE _dataptr_end = _dataptr + (_TBUFF_PTR)->trh2x.nsamp; \
			for ( ; _dataptr < _dataptr_end; _dataptr++ ) { \
				(_TRACE_PTR)->average += 0.001 * (*_dataptr - (_TRACE_PTR)->average); \
				*_dataptr -= (_TRACE_PTR)->average; \
				_result = ((_TRACE_PTR)->lastsample[0] + *_dataptr) * _halfdelta + (_TRACE_PTR)->lastsample[1]; \
				(_TRACE_PTR)->lastsample[0] = *_dataptr; \
				(_TRACE_PTR)->lastsample[1] = _result; \
				*_dataptr = dif2tra_filter_apply( _result, (_TRACE_PTR) ); \
			} \
		})
/**
 * @brief
 *
 * @param traceptr
 * @param tbufferptr
 */
static void operation_int( _TRACEINFO *traceptr, TracePacket *tbufferptr )
{
/* Go through all the double precision data */
	if ( tbufferptr->trh2x.datatype[1] == '8' )
		OPERATION_INT_MACRO( double *, traceptr, tbufferptr );
/* Go through all the single precision data */
	else
		OPERATION_INT_MACRO( float *, traceptr, tbufferptr );

	return;
}

/**
 * @brief
 *
 */
#define OPERATION_DINT_MACRO(_DATA_PTR_TYPE, _TRACE_PTR, _TBUFF_PTR) \
		__extension__({ \
			const double _halfdelta = (_TRACE_PTR)->delta * 0.5; \
			double _result; \
			double _tmp; \
			_DATA_PTR_TYPE _dataptr = (_DATA_PTR_TYPE)(&(_TBUFF_PTR)->trh2x + 1); \
			_DATA_PTR_TYPE _dataptr_end = _dataptr + (_TBUFF_PTR)->trh2x.nsamp; \
			for ( ; _dataptr < _dataptr_end; _dataptr++ ) { \
				(_TRACE_PTR)->average += 0.001 * (*_dataptr - (_TRACE_PTR)->average); \
				*_dataptr -= (_TRACE_PTR)->average; \
				_tmp = ((_TRACE_PTR)->lastsample[0] + *_dataptr) * _halfdelta + (_TRACE_PTR)->lastsample[1]; \
				_result = ((_TRACE_PTR)->lastsample[1] + _tmp) * _halfdelta + (_TRACE_PTR)->lastsample[2]; \
				(_TRACE_PTR)->lastsample[0] = *_dataptr ; \
				(_TRACE_PTR)->lastsample[1] = _tmp; \
				(_TRACE_PTR)->lastsample[2] = _result; \
				*_dataptr = dif2tra_filter_apply( _result, (_TRACE_PTR) ); \
			} \
		})
/**
 * @brief
 *
 * @param traceptr
 * @param tbufferptr
 */
static void operation_dint( _TRACEINFO *traceptr, TracePacket *tbufferptr )
{
/* Go through all the double precision data */
	if ( tbufferptr->trh2x.datatype[1] == '8' )
		OPERATION_DINT_MACRO( double *, traceptr, tbufferptr );
/* Go through all the single precision data */
	else
		OPERATION_DINT_MACRO( float *, traceptr, tbufferptr );

	return;
}
