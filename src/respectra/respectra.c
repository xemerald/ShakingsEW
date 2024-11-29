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
#include <scnlfilter.h>
#include <respectra.h>
#include <respectra_list.h>
#include <respectra_pmat.h>

/* Functions prototype in this source file */
static void respectra_config( char * );
static void respectra_lookup( void );
static void respectra_status( unsigned char, short, char * );
static void respectra_end( void );  /* Free all the local memory & close socket */

static void init_traceinfo( const TRACE2X_HEADER *, _TRACEINFO * );
static void operation_rsp( _TRACEINFO *, TracePacket *, int );
static void operation_rmavg( _TRACEINFO *, TracePacket * );

/* Ring messages things */
static SHM_INFO InRegion;      /* shared memory region to use for i/o    */
static SHM_INFO OutRegion;     /* shared memory region to use for i/o    */
/* */
#define MAXLOGO 5
static MSG_LOGO Putlogo;           /* array for output module, type, instid     */
static MSG_LOGO Getlogo[MAXLOGO];  /* array for requesting module, type, instid */
static pid_t    MyPid;             /* for restarts by startstop                 */

#define OUTPUT_TYPE_SD   0
#define OUTPUT_TYPE_SV   1
#define OUTPUT_TYPE_SA   2

/* Things to read or derive from configuration file */
static char     InRingName[MAX_RING_STR];   /* name of transport ring for i/o    */
static char     OutRingName[MAX_RING_STR];  /* name of transport ring for i/o    */
static char     MyModName[MAX_MOD_STR];     /* speak as this module name/id      */
static uint8_t  LogSwitch;                  /* 0 if no logfile should be written */
static uint64_t HeartBeatInterval;          /* seconds between heartbeats        */
static uint8_t  OutputTypeFlag;             /* 0 for Sd, 1 for Sv and 2 for Sa */
static uint16_t DriftCorrectThreshold;      /* seconds waiting for D.C. */
static uint16_t nLogo = 0;
static double   DampingRatio  = 0.05;       /* The damping ratio for calculating, default is 5% */
static double   NaturalPeriod = 1.0;        /* The period for calculating, default is 1 second. Normally 0.3 & 3 seconds are other choice */
static double   GainFactor    = 1.0;        /* The gain factor or amplify factor for the raw data, default is 1.0 */
static uint8_t  SCNLFilterSwitch = 0;       /* 0 if no filter command in the file */

/* Things to look up in the earthworm.h tables with getutil.c functions */
static int64_t InRingKey;       /* key of transport ring for i/o     */
static int64_t OutRingKey;      /* key of transport ring for i/o     */
static uint8_t InstId;          /* local installation id             */
static uint8_t MyModId;         /* Module Id for this program        */
static uint8_t TypeHeartBeat;
static uint8_t TypeError;
static uint8_t TypeTracebuf2;

/* Error messages used by respectra */
#define  ERR_MISSMSG       0   /* message missed in transport ring       */
#define  ERR_TOOBIG        1   /* retreived msg too large for buffer     */
#define  ERR_NOTRACK       2   /* msg retreived; tracking limit exceeded */
#define  ERR_QUEUE         3   /* error queueing message for sending      */
static char Text[150];         /* string for log/error messages          */

/* Extern variables initialized by PMatrixInit function */
extern double  AngularFreq;
extern double  AFreqSquare;
extern double  DAFreqDamping;

/*
 *
 */
int main ( int argc, char **argv )
{
	int res;

	int64_t  recsize = 0;
	MSG_LOGO reclogo;
	time_t   timeNow;          /* current time                  */
	time_t   timeLastBeat;     /* time last heartbeat was sent  */
	char    *lockfile;
	int32_t  lockfile_fd;

	_TRACEINFO *traceptr;
	TracePacket tracebuffer;  /* message which is sent to share ring    */
	const void *_match = NULL;

	double tmp_time;

/* Check command line arguments */
	if ( argc != 2 ) {
		fprintf( stderr, "Usage: respectra <configfile>\n" );
		exit( 0 );
	}
/* Initialize name of log-file & open it */
	logit_init( argv[1], 0, 256, 1 );
/* Read the configuration file(s) */
	respectra_config( argv[1] );
	logit( "" , "%s: Read command file <%s>\n", argv[0], argv[1] );
/* Look up important info from earthworm.h tables */
	respectra_lookup();
/* Reinitialize logit to desired logging level */
	logit_init( argv[1], 0, 256, LogSwitch );

	lockfile = ew_lockfile_path(argv[1]);
	if ( (lockfile_fd = ew_lockfile(lockfile) ) == -1 ) {
		fprintf(stderr, "one instance of %s is already running, exiting\n", argv[0]);
		exit (-1);
	}
/* Initialize the SCNL filter required arguments */
	if ( SCNLFilterSwitch )
		scnlfilter_init( "respectra" );
/* Get process ID for heartbeat messages */
	MyPid = getpid();
	if ( MyPid == -1 ) {
		logit("e","respectra: Cannot get pid. Exiting.\n");
		exit (-1);
	}

/* Build the message */
	Putlogo.instid = InstId;
	Putlogo.mod    = MyModId;
	Putlogo.type   = TypeTracebuf2;

/* Attach to Input/Output shared memory ring */
	tport_attach( &InRegion, InRingKey );
	logit("", "respectra: Attached to public memory region %s: %ld\n", InRingName, InRingKey);
/* Flush the transport ring */
	tport_flush( &InRegion, Getlogo, nLogo, &reclogo );
/* */
	tport_attach( &OutRegion, OutRingKey );
	logit("", "respectra: Attached to public memory region %s: %ld\n", OutRingName, OutRingKey);

/* Initialize the filter parameters, use the butterworth high pass filter */
	if ( rsp_pmat_init( DampingRatio, NaturalPeriod ) ) {
		logit("e", "respectra: Initialize the pmatrix error, exiting!\n");
		exit(-1);
	}
/* Force a heartbeat to be issued in first pass thru main loop */
	timeLastBeat = time(&timeNow) - HeartBeatInterval - 1;
/*----------------------- setup done; start main loop -------------------------*/
	while ( 1 ) {
	/* Send respectra's heartbeat */
		if ( time(&timeNow) - timeLastBeat >= (int64_t)HeartBeatInterval ) {
			timeLastBeat = timeNow;
			respectra_status( TypeHeartBeat, 0, "" );
		}

	/* Process all new messages */
		do {
		/* See if a termination has been requested */
			res = tport_getflag( &InRegion );
			if ( res == TERMINATE || res == MyPid ) {
			/* write a termination msg to log file */
				logit("t", "respectra: Termination requested; exiting!\n");
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
				respectra_status( TypeError, ERR_TOOBIG, Text );
				continue;
			}
		/* Got a msg, but missed some */
			else if ( res == GET_MISS ) {
				sprintf(
					Text, "Missed msg(s)  i%u m%u t%u  %s.", reclogo.instid, reclogo.mod, reclogo.type, InRingName
				);
				respectra_status( TypeError, ERR_MISSMSG, Text );
			}
		/* Got a msg, but can't tell */
			else if ( res == GET_NOTRACK ) {
			/* If any were missed        */
				sprintf(
					Text, "Msg received (i%u m%u t%u); transport.h NTRACK_GET exceeded",
					reclogo.instid, reclogo.mod, reclogo.type
				);
				respectra_status( TypeError, ERR_NOTRACK, Text );
			}

		/* Process the message */
			if ( reclogo.type == TypeTracebuf2 ) {
				if ( !TRACE2_HEADER_VERSION_IS_21(&(tracebuffer.trh2)) ) {
					printf(
						"respectra: SCNL %s.%s.%s.%s version is invalid, please check it!\n",
						tracebuffer.trh2.sta, tracebuffer.trh2.chan, tracebuffer.trh2.net, tracebuffer.trh2.loc
					);
					continue;
				}
			/* */
				if ( tracebuffer.trh2x.datatype[0] != 'f' && tracebuffer.trh2x.datatype[0] != 't' ) {
					printf(
						"respectra: SCNL %s.%s.%s.%s datatype[%s] is invalid, skip it!\n",
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
					!(traceptr = rsp_list_find( &tracebuffer.trh2x )) &&
					!scnlfilter_trace_apply( tracebuffer.msg, reclogo.type, &_match )
				) {
				#ifdef _SEW_DEBUG
					printf("respectra: Found SCNL %s.%s.%s.%s but not in the filter, drop it!\n",
					tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc);
				#endif
					continue;
				}
			/* If we can't get the trace pointer to the local list, search it again */
				if ( !traceptr && !(traceptr = rsp_list_search( &tracebuffer.trh2x )) ) {
				/* Error when insert into the tree */
					logit(
						"e", "respectra: SCNL %s.%s.%s.%s insert into trace tree error, drop this trace.\n",
						tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc
					);
					continue;
				}

			/* First time initialization */
				if ( traceptr->firsttime || fabs(1.0 / tracebuffer.trh2x.samprate - traceptr->delta) > FLT_EPSILON ) {
					printf(
						"respectra: New SCNL %s.%s.%s.%s received, starting to trace!\n",
						traceptr->sta, traceptr->chan, traceptr->net, traceptr->loc
					);
					init_traceinfo( &tracebuffer.trh2x, traceptr );
				}
			/* Remap the SCNL of this incoming trace */
				if ( SCNLFilterSwitch ) {
					if ( traceptr->match ) {
						scnlfilter_trace_remap( tracebuffer.msg, reclogo.type, traceptr->match );
					}
					else {
						if ( scnlfilter_trace_remap( tracebuffer.msg, reclogo.type, _match ) ) {
							printf(
								"respectra: Remap received trace SCNL %s.%s.%s.%s to %s.%s.%s.%s!\n",
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
					if ( (long)tracebuffer.trh2x.starttime > (time(&timeNow) + 3) ) {
					#ifdef _SEW_DEBUG
						printf( "respectra: %s.%s.%s.%s NTP sync error, drop it!\n",
						tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc );
					#endif
						continue;
					}
					else if ( tmp_time < 0.0 ) {
					#ifdef _SEW_DEBUG
						printf( "respectra: Overlapped in %s.%s.%s.%s, drop it!\n",
						tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc );
					#endif
						continue;
					}
					else if ( tmp_time > 0.0 && traceptr->lasttime > 0.0 ) {
						if ( tmp_time >= DriftCorrectThreshold ) {
							printf(
								"respectra: Found %ld sample gap in SCNL %s.%s.%s.%s, restart tracing!\n",
								(long)(tmp_time * tracebuffer.trh2x.samprate),
								tracebuffer.trh2x.sta, tracebuffer.trh2x.chan,
								tracebuffer.trh2x.net, tracebuffer.trh2x.loc
							);
						/* Due to large gap, try to restart this trace */
							init_traceinfo( &tracebuffer.trh2x, traceptr );
						}
					#ifdef _SEW_DEBUG
						else {
							printf( "respectra: Found %ld sample gap in %s.%s.%s.%s!\n",
								(long)(tmp_time * tracebuffer.trh2x.samprate),
								tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc );
						}
					#endif
					}
				}

			/* Record the time that the trace last updated */
				traceptr->lasttime = tracebuffer.trh2x.endtime;
			/* Update the conversion factor with the gain factor */
				tracebuffer.trh2x.x.v21.conversion_factor *= GainFactor;
			/* Wait for the D.C. */
				if ( traceptr->readycount >= DriftCorrectThreshold ) {
				/* Do the main operation */
					operation_rsp( traceptr, &tracebuffer, OutputTypeFlag );
				/* Modify the trace header to indicate that the data already been processed */
					tracebuffer.trh2x.pinno = OutputTypeFlag;
				/*
				 * Dump the new trace into output message,
				 * and send the packed message to the output ring
				 */
					if ( tport_putmsg( &OutRegion, &Putlogo, recsize, tracebuffer.msg ) != PUT_OK )
						logit("e", "respectra: Error putting message in region %ld\n", OutRegion.key);
				}
				else {
					traceptr->readycount += (uint16_t)(tracebuffer.trh2x.nsamp/tracebuffer.trh2x.samprate + 0.5);
				/* Only do the average removing before D.C complete */
					operation_rmavg( traceptr, &tracebuffer );
				/* */
					if ( traceptr->readycount >= DriftCorrectThreshold ) {
						printf(
							"respectra: SCNL %s.%s.%s.%s initialization of D.C. complete!\n",
							tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc
						);
					}
				}
			}
		} while ( 1 );  /* end of message-processing-loop */
		sleep_ew(50);   /* no more messages; wait for new ones to arrive */
	}
/*-----------------------------end of main loop-------------------------------*/
exit_procedure:
/* detach from shared memory */
	respectra_end();

	ew_unlockfile(lockfile_fd);
	ew_unlink_lockfile(lockfile);

	return 0;
}

/*
 * respectra_config() - processes command file(s) using kom.c functions;
 *                      exits if any errors are encountered.
 */
static void respectra_config( char *configfile )
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
		logit("e", "respectra: Error opening command file <%s>; exiting!\n", configfile);
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
					logit("e", "respectra: Error opening command file <%s>; exiting!\n", &com[1]);
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
			else if ( k_its("OutputType") ) {
				str = k_str();
				if ( str ) {
					char typestr[8];
					memcpy(typestr, str, 8);
					for ( i = 0; i < 8; i++ )
						typestr[i] = tolower(typestr[i]);
				/* Compare the type string */
					if ( !strncmp(typestr, "sa", 2) ) {
						OutputTypeFlag = RECORD_SPECTRAL_ACCELERATION;
					}
					else if ( !strncmp(typestr, "sv", 2) ) {
						OutputTypeFlag = RECORD_SPECTRAL_VELOCITY;
					}
					else if ( !strncmp(typestr, "sd", 2) ) {
						OutputTypeFlag = RECORD_SPECTRAL_DISPLACEMENT;
					}
					else {
						logit("e", "respectra: Invalid output value type <%s>", str);
						logit("e", " in <OutputType> cmd; exiting!\n");
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
			else if ( k_its("DampingRatio") ) {
				DampingRatio = k_val();
				logit("o", "respectra: Ratio of critical damping change to %lf\n", DampingRatio);
			}
			else if ( k_its("NaturalPeriod") ) {
				NaturalPeriod = k_val();
				logit("o", "respectra: The natural period change to %lf\n", NaturalPeriod);
			}
			else if ( k_its("GainFactor") ) {
				GainFactor = k_val();
				logit("o", "respectra: The gain factor change to %lf\n", GainFactor);
			}
		/* Enter installation & module to get event messages from */
		/* 7 */
			else if ( k_its("GetEventsFrom") ) {
				if ( (nLogo + 1) >= MAXLOGO ) {
					logit("e", "respectra: Too many <GetEventsFrom> commands in <%s>", configfile);
					logit("e", "; max=%d; exiting!\n", (int)MAXLOGO);
					exit(-1);
				}
				if ( (str = k_str()) ) {
					if ( GetInst(str, &Getlogo[nLogo].instid) != 0 ) {
						logit("e", "respectra: Invalid installation name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				if ( (str = k_str()) ) {
					if( GetModId(str, &Getlogo[nLogo].mod) != 0 ) {
						logit("e", "respectra: Invalid module name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				if ( (str = k_str()) ) {
					if( GetType( str, &Getlogo[nLogo].type ) != 0 ) {
						logit("e", "respectra: Invalid message type name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				nLogo++;
				init[7] = 1;
			}
			else if ( scnlfilter_com( "respectra" ) ) {
			/* */
				SCNLFilterSwitch = 1;
			}
		/* Unknown command */
			else {
				logit("e", "respectra: <%s> Unknown command in <%s>.\n", com, configfile);
				continue;
			}

		/* See if there were any errors processing the command */
			if ( k_err() ) {
				logit("e", "respectra: Bad <%s> command in <%s>; exiting!\n", com, configfile);
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
		logit( "e", "respectra: ERROR, no " );
		if ( !init[0] ) logit("e", "<LogFile> "              );
		if ( !init[1] ) logit("e", "<MyModuleId> "           );
		if ( !init[2] ) logit("e", "<InputRing> "            );
		if ( !init[3] ) logit("e", "<OutputRing> "           );
		if ( !init[4] ) logit("e", "<HeartBeatInterval> "    );
		if ( !init[5] ) logit("e", "<OutputType> "           );
		if ( !init[6] ) logit("e", "<DriftCorrectThreshold> ");
		if ( !init[7] ) logit("e", "any <GetEventsFrom> "    );

		logit("e", "command(s) in <%s>; exiting!\n", configfile);
		exit(-1);
	}

	return;
}

/*
 * respectra_lookup() - Look up important info from earthworm.h tables
 */
static void respectra_lookup( void )
{
/* Look up keys to shared memory regions */
	if ( ( InRingKey = GetKey(InRingName) ) == -1 ) {
		fprintf(stderr, "respectra:  Invalid ring name <%s>; exiting!\n", InRingName);
		exit(-1);
	}
	if ( ( OutRingKey = GetKey(OutRingName) ) == -1 ) {
		fprintf(stderr, "respectra:  Invalid ring name <%s>; exiting!\n", OutRingName);
		exit(-1);
	}
/* Look up installations of interest */
	if ( GetLocalInst(&InstId) != 0 ) {
		fprintf(stderr, "respectra: error getting local installation id; exiting!\n");
		exit(-1);
	}
/* Look up modules of interest */
	if ( GetModId(MyModName, &MyModId) != 0 ) {
		fprintf(stderr, "respectra: Invalid module name <%s>; exiting!\n", MyModName);
		exit(-1);
	}
/* Look up message types of interest */
	if ( GetType("TYPE_HEARTBEAT", &TypeHeartBeat) != 0 ) {
		fprintf(stderr, "respectra: Invalid message type <TYPE_HEARTBEAT>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_ERROR", &TypeError) != 0 ) {
		fprintf(stderr, "respectra: Invalid message type <TYPE_ERROR>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_TRACEBUF2", &TypeTracebuf2) != 0 ) {
		fprintf(stderr, "respectra: Invalid message type <TYPE_TRACEBUF2>; exiting!\n");
		exit(-1);
	}

	return;
}

/*
 * respectra_status() - builds a heartbeat or error message & puts it into
 *                      shared memory.  Writes errors to log file & screen.
 */
static void respectra_status( unsigned char type, short ierr, char *note )
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
		logit("et", "respectra: %s\n", note);
	}

	size = strlen(msg);  /* don't include the null byte in the message */

/* Write the message to shared memory */
	if ( tport_putmsg(&InRegion, &logo, size, msg) != PUT_OK ) {
		if ( type == TypeHeartBeat ) {
			logit("et", "respectra:  Error sending heartbeat.\n");
		}
		else if ( type == TypeError ) {
			logit("et", "respectra:  Error sending error:%d.\n", ierr);
		}
	}

	return;
}

/*
 * respectra_end() - free all the local memory & close socket
 */
static void respectra_end( void )
{
	tport_detach( &InRegion );
	tport_detach( &OutRegion );
	scnlfilter_end( NULL );
	rsp_pmat_end();
	rsp_list_end();

	return;
}

/*
 *
 */
static void init_traceinfo( const TRACE2X_HEADER *trh2x, _TRACEINFO *traceptr )
{
	traceptr->firsttime     = FALSE;
	traceptr->readycount    = 0;
	traceptr->lasttime      = 0.0;
	traceptr->lastsample    = 0.0;
	traceptr->average       = 0.0;
	traceptr->delta         = 1.0 / trh2x->samprate;
	traceptr->intsteps      = traceptr->delta * 20.0 / NaturalPeriod + 1.0 - 1.e-5;
	traceptr->pmatrix       = rsp_pmat_search( traceptr );
	memset(traceptr->xmatrix, 0, sizeof(traceptr->xmatrix));

	return;
}

/**
 * @brief
 *
 */
#define OPERATION_RSP_MACRO(_DATA_PTR_TYPE, _TRACE_PTR, _TBUFF_PTR, _OUTPUT_TYPE) \
		__extension__({ \
			double _dsample, _slope; \
			double _resp_d, _resp_v, _resp_a; \
			double _result; \
			const double *const _a = (_TRACE_PTR)->pmatrix->a; \
			const double *const _b = (_TRACE_PTR)->pmatrix->b; \
			double *const _x = (_TRACE_PTR)->xmatrix; \
			_DATA_PTR_TYPE _dataptr = (_DATA_PTR_TYPE)(&(_TBUFF_PTR)->trh2x + 1); \
			_DATA_PTR_TYPE _dataptr_end = _dataptr + (_TBUFF_PTR)->trh2x.nsamp; \
			for ( ; _dataptr < _dataptr_end; _dataptr++ ) { \
				_dsample = *_dataptr * GainFactor; \
				(_TRACE_PTR)->average += 0.001 * (_dsample - (_TRACE_PTR)->average); \
				_dsample -= (_TRACE_PTR)->average; \
				_slope = (_dsample - (_TRACE_PTR)->lastsample) / (_TRACE_PTR)->intsteps; \
				_result = 0.0; \
				for ( int _i = 0; _i < (int)(_TRACE_PTR)->intsteps; _i++ ) { \
					_resp_a = (_TRACE_PTR)->lastsample + _slope * _i; \
					_resp_d = _a[0] * _x[0] + _a[1] * _x[1] - _b[0] * _resp_a - _b[1] * _slope; \
					_resp_v = _a[2] * _x[0] + _a[3] * _x[1] - _b[2] * _resp_a - _b[3] * _slope; \
					_resp_a = -(DAFreqDamping * _resp_v + AFreqSquare * _resp_d); \
					_x[0] = _resp_d; \
					_x[1] = _resp_v; \
					_x[2] = _resp_a; \
					if ( fabs(_x[(_OUTPUT_TYPE)]) > fabs(_result) ) \
						_result = _x[(_OUTPUT_TYPE)]; \
				} \
				(_TRACE_PTR)->lastsample = _dsample; \
				*_dataptr = _result; \
			} \
		})
/**
 * @brief
 *
 * @param traceptr
 * @param tbufferptr
 * @param type
 */
static void operation_rsp( _TRACEINFO *traceptr, TracePacket *tbufferptr, int type )
{
	type -= RECORD_SPECTRAL_DISPLACEMENT;
/* Go through all the double precision data */
	if ( tbufferptr->trh2x.datatype[1] == '8' )
		OPERATION_RSP_MACRO( double *, traceptr, tbufferptr, type );
/* Go through all the single precision data */
	else
		OPERATION_RSP_MACRO( float *, traceptr, tbufferptr, type );

	return;
}

/**
 * @brief
 *
 */
#define OPERATION_RMAVG_MACRO(_DATA_PTR_TYPE, _TRACE_PTR, _TBUFF_PTR) \
		__extension__({ \
			double _dsample; \
			_DATA_PTR_TYPE _dataptr = (_DATA_PTR_TYPE)(&(_TBUFF_PTR)->trh2x + 1); \
			_DATA_PTR_TYPE _dataptr_end = _dataptr + (_TBUFF_PTR)->trh2x.nsamp; \
			for ( ; _dataptr < _dataptr_end; _dataptr++ ) { \
				_dsample = *_dataptr * GainFactor; \
				(_TRACE_PTR)->average += 0.001 * (_dsample - (_TRACE_PTR)->average); \
				*_dataptr = _dsample - (_TRACE_PTR)->average; \
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
