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
#include <trace_buf.h>
/* Local header include */
#include <scnlfilter.h>
#include <recordtype.h>
#include <tracepeak.h>
#include <trace2peak.h>
#include <trace2peak_list.h>

/* Functions prototype in this source file */
static void trace2peak_config( char * );
static void trace2peak_lookup( void );
static void trace2peak_status( unsigned char, short, char * );
static void trace2peak_end( void );                /* Free all the local memory & close socket */

static void *proc_com_pv_type( const char * );

/* Ring messages things */
static SHM_INFO InRegion;      /* shared memory region to use for i/o    */
static SHM_INFO OutRegion;     /* shared memory region to use for i/o    */
/* */
#define MAXLOGO 5
static MSG_LOGO Putlogo;              /* array for output module, type, instid     */
static MSG_LOGO Getlogo[MAXLOGO];     /* array for requesting module, type, instid */
static pid_t    MyPid;                /* for restarts by startstop                 */

/* Things to read or derive from configuration file */
static char     InRingName[MAX_RING_STR];   /* name of transport ring for i/o    */
static char     OutRingName[MAX_RING_STR];  /* name of transport ring for i/o    */
static char     MyModName[MAX_MOD_STR];     /* speak as this module name/id      */
static uint8_t  LogSwitch;                  /* 0 if no logfile should be written */
static uint64_t HeartBeatInterval;          /* seconds between heartbeats        */
static uint16_t DriftCorrectThreshold;      /* seconds waiting for D.C. */
static uint16_t nLogo = 0;
static uint8_t  SCNLFilterSwitch = 0;       /* 0 if no filter command in the file    */

/* Things to look up in the earthworm.h tables with getutil.c functions */
static int64_t InRingKey;       /* key of transport ring for i/o     */
static int64_t OutRingKey;      /* key of transport ring for i/o     */
static uint8_t InstId;          /* local installation id             */
static uint8_t MyModId;         /* Module Id for this program        */
static uint8_t TypeHeartBeat;
static uint8_t TypeError;
static uint8_t TypeTracebuf2 = 0;
static uint8_t TypeTracePeak = 0;

/* Error messages used by trace2peak */
#define  ERR_MISSMSG       0   /* message missed in transport ring       */
#define  ERR_TOOBIG        1   /* retreived msg too large for buffer     */
#define  ERR_NOTRACK       2   /* msg retreived; tracking limit exceeded */
#define  ERR_QUEUE         3   /* error queueing message for sending      */
static char Text[150];         /* string for log/error messages          */

/* Main process macro */
#define FOR_EACH_TRACE_DATA_MAIN(_DATA_PTR, _DATA_PTR_END, _DATA_PTR_TYPE, _INPUT_TRACEPACK, _TRACE_PTR, _OUTPUT_TRACEPEAK) \
		__extension__({ \
			double _tmp_time = (_OUTPUT_TRACEPEAK).peaktime; \
			for ( (_DATA_PTR) = (_DATA_PTR_TYPE)(&(_INPUT_TRACEPACK).trh2x + 1), (_DATA_PTR_END) = (_DATA_PTR_TYPE)(_DATA_PTR) + (_INPUT_TRACEPACK).trh2x.nsamp; \
				(_DATA_PTR) < (_DATA_PTR_END); \
				(_DATA_PTR) = (_DATA_PTR_TYPE)(_DATA_PTR) + 1, _tmp_time += (_TRACE_PTR)->delta \
			) { \
				(_TRACE_PTR)->average += 0.001 * (*(_DATA_PTR_TYPE)(_DATA_PTR) - (_TRACE_PTR)->average); \
				*(_DATA_PTR_TYPE)(_DATA_PTR) -= (_TRACE_PTR)->average; \
				if ( fabs(*(_DATA_PTR_TYPE)(_DATA_PTR)) > fabs((_OUTPUT_TRACEPEAK).peakvalue) ) { \
					(_OUTPUT_TRACEPEAK).peakvalue = *(_DATA_PTR_TYPE)(_DATA_PTR); \
					(_OUTPUT_TRACEPEAK).peaktime = _tmp_time; \
				} \
			} \
		})

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

	_TRACEPEAK *traceptr;
	TracePacket tracebuffer;  /* message which is sent to share ring    */
	void       *dataptr;
	void       *dataptr_end;
	double      tmp_time;
	const void *_match = NULL;
	const void *_extra = NULL;

	TRACE_PEAKVALUE obuffer;

/* Check command line arguments */
	if ( argc != 2 ) {
		fprintf(stderr, "Usage: trace2peak <configfile>\n");
		exit(0);
	}
/* Initialize name of log-file & open it */
	logit_init(argv[1], 0, 256, 1);
/* Read the configuration file(s) */
	trace2peak_config( argv[1] );
	logit("" , "%s: Read command file <%s>\n", argv[0], argv[1]);
/* Look up important info from earthworm.h tables */
	trace2peak_lookup();
/* Reinitialize logit to desired logging level */
	logit_init(argv[1], 0, 256, LogSwitch);

	lockfile = ew_lockfile_path(argv[1]);
	if ( (lockfile_fd = ew_lockfile(lockfile) ) == -1 ) {
		fprintf(stderr, "one instance of %s is already running, exiting\n", argv[0]);
		exit(-1);
	}
/* Initialize the SCNL filter required arguments */
	if ( SCNLFilterSwitch )
		scnlfilter_init( "trace2peak" );
/* Get process ID for heartbeat messages */
	MyPid = getpid();
	if( MyPid == -1 ) {
		logit("e","trace2peak: Cannot get pid. Exiting.\n");
		exit(-1);
	}

/* Build the message */
	Putlogo.instid = InstId;
	Putlogo.mod    = MyModId;
	Putlogo.type   = TypeTracePeak;

/* Attach to Input/Output shared memory ring */
	tport_attach( &InRegion, InRingKey );
	logit("", "trace2peak: Attached to public memory region %s: %ld\n", InRingName, InRingKey);
/* Flush the transport ring */
	tport_flush( &InRegion, Getlogo, nLogo, &reclogo );
/* */
	tport_attach( &OutRegion, OutRingKey );
	logit("", "trace2peak: Attached to public memory region %s: %ld\n", OutRingName, OutRingKey);

/* Force a heartbeat to be issued in first pass thru main loop */
	timeLastBeat = time(&timeNow) - HeartBeatInterval - 1;
/*----------------------- setup done; start main loop -------------------------*/
	while ( 1 ) {
	/* Send trace2peak's heartbeat */
		if ( time(&timeNow) - timeLastBeat >= (int64_t)HeartBeatInterval ) {
			timeLastBeat = timeNow;
			trace2peak_status( TypeHeartBeat, 0, "" );
		}

	/* Process all new messages */
		do {
		/* See if a termination has been requested */
			res = tport_getflag( &InRegion );
			if ( res == TERMINATE || res == MyPid ) {
			/* write a termination msg to log file */
				logit("t", "trace2peak: Termination requested; exiting!\n");
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
				trace2peak_status( TypeError, ERR_TOOBIG, Text );
				continue;
			}
		/* Got a msg, but missed some */
			else if ( res == GET_MISS ) {
				sprintf(
					Text, "Missed msg(s)  i%u m%u t%u  %s.", reclogo.instid, reclogo.mod, reclogo.type, InRingName
				);
				trace2peak_status( TypeError, ERR_MISSMSG, Text );
			}
		/* Got a msg, but can't tell */
			else if ( res == GET_NOTRACK ) {
			/* If any were missed        */
				sprintf(
					Text, "Msg received (i%u m%u t%u); transport.h NTRACK_GET exceeded",
					reclogo.instid, reclogo.mod, reclogo.type
				);
				trace2peak_status( TypeError, ERR_NOTRACK, Text );
			}

		/* Process the message */
			if ( reclogo.type == TypeTracebuf2 ) {
				if ( !TRACE2_HEADER_VERSION_IS_21(&(tracebuffer.trh2)) ) {
					printf(
						"trace2peak: %s.%s.%s.%s version is invalid, please check it!\n",
						tracebuffer.trh2.sta, tracebuffer.trh2.chan, tracebuffer.trh2.net, tracebuffer.trh2.loc
					);
					continue;
				}
			/* */
				if ( tracebuffer.trh2x.datatype[0] != 'f' && tracebuffer.trh2x.datatype[0] != 't' ) {
					printf(
						"trace2peak: %s.%s.%s.%s datatype[%s] is invalid, skip it!\n",
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
					!(traceptr = tra2peak_list_find( &tracebuffer.trh2x )) &&
					!scnlfilter_trace_apply( tracebuffer.msg, reclogo.type, &_match )
				) {
				#ifdef _DEBUG
					printf("trace2peak: Found SCNL %s.%s.%s.%s but not in the filter, drop it!\n",
					tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc);
				#endif
					continue;
				}
			/* */
				if ( !traceptr && !(traceptr = tra2peak_list_search( &tracebuffer.trh2x )) ) {
				/* Error when insert into the tree */
					logit(
						"e", "trace2peak: %s.%s.%s.%s insert into trace tree error, drop this trace.\n",
						tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc
					);
					continue;
				}
			/* First time initialization */
				if ( traceptr->firsttime ) {
					printf(
						"trace2peak: New SCNL(%s.%s.%s.%s) received, starting to trace!\n",
						traceptr->sta, traceptr->chan, traceptr->net, traceptr->loc
					);
					traceptr->firsttime  = FALSE;
					traceptr->readycount = 0;
					traceptr->delta      = 1.0/tracebuffer.trh2x.samprate;
					traceptr->lasttime   = 0.0;
					traceptr->average    = 0.0;
				}
			/* Remap the SCNL of this incoming trace */
				if ( SCNLFilterSwitch ) {
					if ( traceptr->match ) {
						scnlfilter_trace_remap( tracebuffer.msg, reclogo.type, traceptr->match );
					}
					else {
						if ( scnlfilter_trace_remap( tracebuffer.msg, reclogo.type, _match ) ) {
							printf(
								"trace2peak: Remap received trace SCNL %s.%s.%s.%s to %s.%s.%s.%s!\n",
								traceptr->sta, traceptr->chan, traceptr->net, traceptr->loc,
								tracebuffer.trh2x.sta, tracebuffer.trh2x.chan,
								tracebuffer.trh2x.net, tracebuffer.trh2x.loc
							);
						}
						traceptr->match = _match;
					}
				/* Fetch the extra argument */
					_extra = scnlfilter_extra_get( traceptr->match );
				}
				else {
					_extra = NULL;
				}

			/* Start processing the gap in trace */
				if ( fabs(tmp_time = tracebuffer.trh2x.starttime - traceptr->lasttime) > traceptr->delta * 2.0 ) {
					if ( (long)tracebuffer.trh2x.starttime > (time(&timeNow) + 3) ) {
					#ifdef _DEBUG
						printf( "trace2peak: %s.%s.%s.%s NTP sync error, drop it!\n",
						tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc );
					#endif
						continue;
					}
					else if ( tmp_time < 0.0 ) {
					#ifdef _DEBUG
						printf( "trace2peak: Overlapped in %s.%s.%s.%s, drop it!\n",
						tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc );
					#endif
						continue;
					}
					else if ( tmp_time > 0.0 && traceptr->lasttime > 0.0 )	{
						if ( tmp_time >= DriftCorrectThreshold ) {
							printf(
								"trace2peak: Found %ld sample gap in %s.%s.%s.%s, restart tracing!\n",
								(long)(tmp_time * tracebuffer.trh2x.samprate),
								tracebuffer.trh2x.sta, tracebuffer.trh2x.chan,
								tracebuffer.trh2x.net, tracebuffer.trh2x.loc
							);
						/* Due to large gap, try to restart this trace */
							traceptr->firsttime = TRUE;
							continue;
						}
					#ifdef _DEBUG
						else {
							printf( "trace2peak: Found %ld sample gap in %s.%s.%s.%s!\n",
								(long)(tmp_time * tracebuffer.trh2x.samprate),
								tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc );
						}
					#endif
					}
				}

			/* Record the time that the trace last updated */
				traceptr->lasttime = tracebuffer.trh2x.endtime;
			/* Reset the peak value & its time */
				obuffer.peakvalue = 0.0;
				obuffer.peaktime  = tracebuffer.trh2x.starttime;
			/* Go through all the data for 'double' precision type */
				if ( tracebuffer.trh2x.datatype[1] == '8' )
					FOR_EACH_TRACE_DATA_MAIN( dataptr, dataptr_end, double *, tracebuffer, traceptr, obuffer );
			/* Go through all the data for 'single' precision type */
				else
					FOR_EACH_TRACE_DATA_MAIN( dataptr, dataptr_end, float *, tracebuffer, traceptr, obuffer );

			/* Wait for the D.C. */
				if ( traceptr->readycount < DriftCorrectThreshold ) {
					traceptr->readycount += (uint16_t)(tracebuffer.trh2x.nsamp / tracebuffer.trh2x.samprate + 0.5);
					if ( traceptr->readycount >= DriftCorrectThreshold ) {
						printf(
							"trace2peak: %s.%s.%s.%s initialization of D.C. complete!\n",
							tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc
						);
					}
					continue;
				}

			/* Packing the output message */
				memcpy(obuffer.sta, traceptr->sta, TRACE2_STA_LEN);
				memcpy(obuffer.net, traceptr->net, TRACE2_NET_LEN);
				memcpy(obuffer.loc, traceptr->loc, TRACE2_LOC_LEN);
				memcpy(obuffer.chan, traceptr->chan, TRACE2_CHAN_LEN);
			/* */
				obuffer.recordtype = _extra ? *(RECORD_TYPE *)_extra : DEF_PEAK_VALUE_TYPE;
				obuffer.sourcemod  = reclogo.mod;
			/* Send the packed message to the output ring */
				if ( tport_putmsg(&OutRegion, &Putlogo, TRACE_PEAKVALUE_SIZE, (char *)&obuffer) != PUT_OK )
					logit("e", "trace2peak: Error putting message in region %ld\n", OutRegion.key);
			}
		} while ( 1 );  /* end of message-processing-loop */
		sleep_ew(50);  /* no more messages; wait for new ones to arrive */
	}
/*-----------------------------end of main loop-------------------------------*/
exit_procedure:
/* detach from shared memory */
	trace2peak_end();

	ew_unlockfile(lockfile_fd);
	ew_unlink_lockfile(lockfile);

	return 0;
}

/*
 * trace2peak_config() - processes command file(s) using kom.c functions;
 *                      exits if any errors are encountered.
 */
static void trace2peak_config( char *configfile )
{
	char  init[7];     /* init flags, one byte for each required command */
	char *com;
	char *str;

	int ncommand;     /* # of required commands you expect to process   */
	int nmiss;        /* number of required commands that were missed   */
	int nfiles;
	int success;
	int i;

/* Set to zero one init flag for each required command */
	ncommand = 7;
	for ( i = 0; i < ncommand; i++ )
		init[i] = 0;
/* Open the main configuration file */
	nfiles = k_open( configfile );
	if ( nfiles == 0 ) {
		logit("e", "trace2peak: Error opening command file <%s>; exiting!\n", configfile);
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
					logit("e", "trace2peak: Error opening command file <%s>; exiting!\n", &com[1]);
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
					strcpy( InRingName, str );
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
		/* 5 */
			else if ( k_its("DriftCorrectThreshold") ) {
				DriftCorrectThreshold = k_long();
				init[5] = 1;
			}
		/* Enter installation & module to get event messages from */
		/* 6 */
			else if ( k_its("GetEventsFrom") ) {
				if ( (nLogo + 1) >= MAXLOGO ) {
					logit("e", "trace2peak: Too many <GetEventsFrom> commands in <%s>", configfile);
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
				init[6] = 1;
			}
			else if ( scnlfilter_com( "trace2peak" ) ) {
			/* */
				SCNLFilterSwitch = 1;
				for ( str = k_com(), i = 0; *str == ' ' && i < (int)strlen(str); str++, i++ );
				if ( strncmp(str, "Block_SCNL", 10) ) {
				/* Maybe we need much more checking for this command */
					if ( scnlfilter_extra_com( proc_com_pv_type ) < 0 ) {
						logit("o", "trace2peak: No peak value type define in command: \"%s\" ", k_com());
						logit("o", ", %d(%s) will be filled!\n", DEF_PEAK_VALUE_TYPE, typenum2str(DEF_PEAK_VALUE_TYPE));
					/* Reset the error code for this command */
						k_err();
					}
				}
			}
		/* Unknown command */
			else {
				logit("e", "trace2peak: <%s> Unknown command in <%s>.\n", com, configfile);
				continue;
			}

		/* See if there were any errors processing the command */
			if ( k_err() ) {
				logit("e", "trace2peak: Bad <%s> command in <%s>; exiting!\n", com, configfile);
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
		logit( "e", "trace2peak: ERROR, no " );
		if ( !init[0] ) logit("e", "<LogFile> "              );
		if ( !init[1] ) logit("e", "<MyModuleId> "           );
		if ( !init[2] ) logit("e", "<InputRing> "            );
		if ( !init[3] ) logit("e", "<OutputRing> "           );
		if ( !init[4] ) logit("e", "<HeartBeatInterval> "    );
		if ( !init[5] ) logit("e", "<DriftCorrectThreshold> ");
		if ( !init[6] ) logit("e", "any <GetEventsFrom> "    );

		logit("e", "command(s) in <%s>; exiting!\n", configfile);
		exit(-1);
	}

	return;
}

/*
 * trace2peak_lookup() - Look up important info from earthworm.h tables
 */
static void trace2peak_lookup( void )
{
/* Look up keys to shared memory regions */
	if ( ( InRingKey = GetKey(InRingName) ) == -1 ) {
		fprintf(stderr, "trace2peak:  Invalid ring name <%s>; exiting!\n", InRingName);
		exit(-1);
	}
	if ( ( OutRingKey = GetKey(OutRingName) ) == -1 ) {
		fprintf(stderr, "trace2peak:  Invalid ring name <%s>; exiting!\n", OutRingName);
		exit(-1);
	}
/* Look up installations of interest */
	if ( GetLocalInst(&InstId) != 0 ) {
		fprintf(stderr, "trace2peak: error getting local installation id; exiting!\n");
		exit(-1);
	}
/* Look up modules of interest */
	if ( GetModId(MyModName, &MyModId) != 0 ) {
		fprintf(stderr, "trace2peak: Invalid module name <%s>; exiting!\n", MyModName);
		exit(-1);
	}
/* Look up message types of interest */
	if ( GetType("TYPE_HEARTBEAT", &TypeHeartBeat) != 0 ) {
		fprintf(stderr, "trace2peak: Invalid message type <TYPE_HEARTBEAT>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_ERROR", &TypeError) != 0 ) {
		fprintf(stderr, "trace2peak: Invalid message type <TYPE_ERROR>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_TRACEBUF2", &TypeTracebuf2) != 0 ) {
		fprintf(stderr, "trace2peak: Invalid message type <TYPE_TRACEBUF2>; exiting!\n");
		exit(-1);
	}
	if ( GetType( "TYPE_TRACEPEAK", &TypeTracePeak ) != 0 ) {
		fprintf(stderr, "trace2peak: Invalid message type <TYPE_TRACEPEAK>; exiting!\n");
		exit(-1);
	}

	return;
}

/*
 * trace2peak_status() - builds a heartbeat or error message & puts it into
 *                       shared memory.  Writes errors to log file & screen.
 */
static void trace2peak_status( unsigned char type, short ierr, char *note )
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
		logit("et", "trace2peak: %s\n", note);
	}

	size = strlen(msg);  /* don't include the null byte in the message */

/* Write the message to shared memory */
	if ( tport_putmsg(&InRegion, &logo, size, msg) != PUT_OK ) {
		if ( type == TypeHeartBeat ) {
			logit("et", "trace2peak:  Error sending heartbeat.\n");
		}
		else if ( type == TypeError ) {
			logit("et", "trace2peak:  Error sending error:%d.\n", ierr);
		}
	}

	return;
}

/*
 * trace2peak_end() - free all the local memory & close socket
 */
static void trace2peak_end( void )
{
	tport_detach( &InRegion );
	tport_detach( &OutRegion );
	scnlfilter_end( free );
	tra2peak_list_end();

	return;
}

/*
 *
 */
static void *proc_com_pv_type( const char *command )
{
	RECORD_TYPE *result = (RECORD_TYPE *)calloc(1, sizeof(RECORD_TYPE));

/* */
	*result = typestr2num( command );

	return result;
}
