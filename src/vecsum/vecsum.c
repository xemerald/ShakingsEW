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
#include <vecsum.h>
#include <vecsum_tlist.h>
#include <vecsum_vslist.h>

/* Functions prototype in this source file */
static void vecsum_config( char * );
static void vecsum_lookup( void );
static void vecsum_status( unsigned char, short, char * );
static void vecsum_end( void );                /* Free all the local memory & close socket */

static void operation_square( TracePacket * );

/* Ring messages things */
static SHM_INFO InRegion;      /* shared memory region to use for i/o    */
static SHM_INFO OutRegion;     /* shared memory region to use for i/o    */
/* */
#define MAXLOGO 5
static MSG_LOGO Putlogo;             /* array for output module, type, instid     */
static MSG_LOGO Getlogo[MAXLOGO];    /* array for requesting module, type, instid */
static pid_t    MyPid;               /* for restarts by startstop                 */

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

/* Error messages used by vecsum */
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

	_TRACEINFO  *traceptr;
	TracePacket  tracebuffer;  /* message which is sent to share ring    */
	VECSUM_INFO *vecsumptr;
	const void  *_match = NULL;

	double tmp_time;

/* Check command line arguments */
	if ( argc != 2 ) {
		fprintf( stderr, "Usage: vecsum <configfile>\n" );
		exit( 0 );
	}
/* Initialize name of log-file & open it */
	logit_init( argv[1], 0, 256, 1 );
/* Read the configuration file(s) */
	vecsum_config( argv[1] );
	logit( "" , "%s: Read command file <%s>\n", argv[0], argv[1] );
/* Look up important info from earthworm.h tables */
	vecsum_lookup();
/* Reinitialize logit to desired logging level */
	logit_init( argv[1], 0, 256, LogSwitch );

	lockfile = ew_lockfile_path(argv[1]);
	if ( (lockfile_fd = ew_lockfile(lockfile) ) == -1 ) {
		fprintf(stderr, "one instance of %s is already running, exiting\n", argv[0]);
		exit (-1);
	}
/* Initialize the SCNL filter required arguments */
	if ( SCNLFilterSwitch )
		scnlfilter_init( "vecsum" );
/* Get process ID for heartbeat messages */
	MyPid = getpid();
	if ( MyPid == -1 ) {
		logit("e","vecsum: Cannot get pid. Exiting.\n");
		exit (-1);
	}

/* Build the message */
	Putlogo.instid = InstId;
	Putlogo.mod    = MyModId;
	Putlogo.type   = TypeTracebuf2;

/* Attach to Input/Output shared memory ring */
	tport_attach( &InRegion, InRingKey );
	logit("", "vecsum: Attached to public memory region %s: %ld\n", InRingName, InRingKey);
/* Flush the transport ring */
	tport_flush( &InRegion, Getlogo, nLogo, &reclogo );
/* */
	tport_attach( &OutRegion, OutRingKey );
	logit("", "vecsum: Attached to public memory region %s: %ld\n", OutRingName, OutRingKey);

/* Force a heartbeat to be issued in first pass thru main loop */
	time_last_beat = time(&time_now) - HeartBeatInterval - 1;
/*----------------------- setup done; start main loop -------------------------*/
	while ( 1 ) {
	/* Send vecsum's heartbeat */
		if  ( time(&time_now) - time_last_beat >= (int64_t)HeartBeatInterval ) {
			time_last_beat = time_now;
			vecsum_status( TypeHeartBeat, 0, "" );
		}

	/* Process all new messages */
		do {
		/* See if a termination has been requested */
			res = tport_getflag( &InRegion );
			if ( res == TERMINATE || res == MyPid ) {
			/* write a termination msg to log file */
				logit("t", "vecsum: Termination requested; exiting!\n");
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
				vecsum_status( TypeError, ERR_TOOBIG, Text );
				continue;
			}
		/* Got a msg, but missed some */
			else if ( res == GET_MISS ) {
				sprintf(
					Text, "Missed msg(s)  i%u m%u t%u  %s.", reclogo.instid, reclogo.mod, reclogo.type, InRingName
				);
				vecsum_status( TypeError, ERR_MISSMSG, Text );
			}
		/* Got a msg, but can't tell */
			else if ( res == GET_NOTRACK ) {
			/* If any were missed        */
				sprintf(
					Text, "Msg received (i%u m%u t%u); transport.h NTRACK_GET exceeded",
					reclogo.instid, reclogo.mod, reclogo.type
				);
				vecsum_status( TypeError, ERR_NOTRACK, Text );
			}

		/* Process the message */
			if ( reclogo.type == TypeTracebuf2 ) {
				if ( !TRACE2_HEADER_VERSION_IS_21(&(tracebuffer.trh2)) ) {
					printf(
						"vecsum: SCNL %s.%s.%s.%s version is invalid, please check it!\n",
						tracebuffer.trh2.sta, tracebuffer.trh2.chan, tracebuffer.trh2.net, tracebuffer.trh2.loc
					);
					continue;
				}
			/* */
				if ( tracebuffer.trh2x.datatype[0] != 'f' && tracebuffer.trh2x.datatype[0] != 't' ) {
					printf(
						"vecsum: SCNL %s.%s.%s.%s datatype[%s] is invalid, skip it!\n",
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
					!(traceptr = vecsum_tlist_find( &tracebuffer.trh2x )) &&
					!scnlfilter_trace_apply( tracebuffer.msg, reclogo.type, &_match )
				) {
				#ifdef _DEBUG
					printf("vecsum: Found SCNL %s.%s.%s.%s but not in the filter, drop it!\n",
					tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc);
				#endif
					continue;
				}
			/* If we can't get the trace pointer to the local list, search it again */
				if ( !traceptr && !(traceptr = vecsum_tlist_search( &tracebuffer.trh2x )) ) {
				/* Error when insert into the tree */
					logit(
						"e", "vecsum: SCNL %s.%s.%s.%s insert into trace tree error, drop this trace.\n",
						tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc
					);
					continue;
				}
			/* First time initialization */
				if ( fabs(tracebuffer.trh2x.samprate - traceptr->samprate) > FLT_EPSILON ) {
					printf(
						"vecsum: New SCNL %s.%s.%s.%s received, starting to trace!\n",
						traceptr->sta, traceptr->chan, traceptr->net, traceptr->loc
					);
					init_traceinfo( &tracebuffer.trh2x, traceptr );
				}
			/* Remap the SCNL of this incoming trace, if we turned it on */
				if ( SCNLFilterSwitch ) {
					if ( traceptr->match ) {
						scnlfilter_trace_remap( tracebuffer.msg, reclogo.type, traceptr->match );
					}
					else {
						if ( scnlfilter_trace_remap( tracebuffer.msg, reclogo.type, _match ) ) {
							printf(
								"vecsum: Remap received trace SCNL %s.%s.%s.%s to %s.%s.%s.%s!\n",
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
					#ifdef _DEBUG
						printf( "vecsum: %s.%s.%s.%s NTP sync error, drop it!\n",
						tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc );
					#endif
						continue;
					}
					else if ( tmp_time < 0.0 ) {
					#ifdef _DEBUG
						printf( "vecsum: Overlapped in %s.%s.%s.%s, drop it!\n",
						tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc );
					#endif
						continue;
					}
					else if ( tmp_time > 0.0 && traceptr->lasttime > 0.0 )	{
					#ifdef _DEBUG
						printf( "vecsum: Found %ld sample gap in %s.%s.%s.%s!\n",
							(long)(tmp_time * tracebuffer.trh2x.samprate),
							tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc );
					#endif
					}
				}

			/* Record the time that the trace last updated */
				traceptr->lasttime = tracebuffer.trh2x.endtime;
			/* Do the main operaion */
				operation_square( traceptr, &tracebuffer );
			/* If we didn't got the vs pointer to the local list, search it again */
				if ( !(vecsumptr = traceptr->vsi) && !(vecsumptr = vecsum_vslist_search( &tracebuffer.trh2x )) ) {
				/* Error when insert into the tree */
					logit(
						"e", "vecsum: Create new vector sum for %s.%s.%s.%s error, drop this trace.\n",
						tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc
					);
					continue;
				}
			/* */
				insert_composition( vecsumptr, &tracebuffer );
			/*
			 * Dump the new trace into output message,
			 * and send the packed message to the output ring
			 */
				if ( tport_putmsg( &OutRegion, &Putlogo, recsize, tracebuffer.msg ) != PUT_OK )
					logit("e", "vecsum: Error putting message in region %ld\n", OutRegion.key);
			}
		} while ( 1 );  /* end of message-processing-loop */
		sleep_ew(50);  /* no more messages; wait for new ones to arrive */
	}
/*-----------------------------end of main loop-------------------------------*/
exit_procedure:
/* detach from shared memory */
	vecsum_end();

	ew_unlockfile(lockfile_fd);
	ew_unlink_lockfile(lockfile);

	return 0;
}

/*
 * vecsum_config() - processes command file(s) using kom.c functions;
 *                      exits if any errors are encountered.
 */
static void vecsum_config( char *configfile )
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
		logit("e", "vecsum: Error opening command file <%s>; exiting!\n", configfile);
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
					logit("e", "vecsum: Error opening command file <%s>; exiting!\n", &com[1]);
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
						logit("e", "vecsum: Invalid operation type <%s>", str);
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
				logit("o", "vecsum: Order of high pass filter change to %d\n", HighPassOrder);
			}
			else if ( k_its("HighPassCorner") ) {
				HighPassCorner = k_val();
				logit("o", "vecsum: Corner frequency of high pass filter change to %lf\n", HighPassCorner);
			}
		/* Enter installation & module to get event messages from */
		/* 7 */
			else if ( k_its("GetEventsFrom") ) {
				if ( (nLogo + 1) >= MAXLOGO ) {
					logit("e", "vecsum: Too many <GetEventsFrom> commands in <%s>", configfile);
					logit("e", "; max=%d; exiting!\n", (int)MAXLOGO);
					exit(-1);
				}
				if ( (str = k_str()) ) {
					if ( GetInst(str, &Getlogo[nLogo].instid) != 0 ) {
						logit("e", "vecsum: Invalid installation name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				if ( (str = k_str()) ) {
					if( GetModId(str, &Getlogo[nLogo].mod) != 0 ) {
						logit("e", "vecsum: Invalid module name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				if ( (str = k_str()) ) {
					if( GetType( str, &Getlogo[nLogo].type ) != 0 ) {
						logit("e", "vecsum: Invalid message type name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				nLogo++;
				init[7] = 1;
			}
			else if ( scnlfilter_com( "vecsum" ) ) {
			/* */
				SCNLFilterSwitch = 1;
			}
		/* Unknown command */
			else {
				logit("e", "vecsum: <%s> Unknown command in <%s>.\n", com, configfile);
				continue;
			}

		/* See if there were any errors processing the command */
			if ( k_err() ) {
				logit("e", "vecsum: Bad <%s> command in <%s>; exiting!\n", com, configfile);
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
		logit( "e", "vecsum: ERROR, no " );
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
 * vecsum_lookup() - Look up important info from earthworm.h tables
 */
static void vecsum_lookup( void )
{
/* Look up keys to shared memory regions */
	if ( ( InRingKey = GetKey(InRingName) ) == -1 ) {
		fprintf(stderr, "vecsum:  Invalid ring name <%s>; exiting!\n", InRingName);
		exit(-1);
	}
	if ( ( OutRingKey = GetKey(OutRingName) ) == -1 ) {
		fprintf(stderr, "vecsum:  Invalid ring name <%s>; exiting!\n", OutRingName);
		exit(-1);
	}
/* Look up installations of interest */
	if ( GetLocalInst(&InstId) != 0 ) {
		fprintf(stderr, "vecsum: error getting local installation id; exiting!\n");
		exit(-1);
	}
/* Look up modules of interest */
	if ( GetModId(MyModName, &MyModId) != 0 ) {
		fprintf(stderr, "vecsum: Invalid module name <%s>; exiting!\n", MyModName);
		exit(-1);
	}
/* Look up message types of interest */
	if ( GetType("TYPE_HEARTBEAT", &TypeHeartBeat) != 0 ) {
		fprintf(stderr, "vecsum: Invalid message type <TYPE_HEARTBEAT>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_ERROR", &TypeError) != 0 ) {
		fprintf(stderr, "vecsum: Invalid message type <TYPE_ERROR>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_TRACEBUF2", &TypeTracebuf2) != 0 ) {
		fprintf(stderr, "vecsum: Invalid message type <TYPE_TRACEBUF2>; exiting!\n");
		exit(-1);
	}

	return;
}

/*
 * vecsum_status() - builds a heartbeat or error message & puts it into
 *                      shared memory.  Writes errors to log file & screen.
 */
static void vecsum_status( unsigned char type, short ierr, char *note )
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
		logit("et", "vecsum: %s\n", note);
	}

	size = strlen(msg);  /* don't include the null byte in the message */

/* Write the message to shared memory */
	if ( tport_putmsg(&InRegion, &logo, size, msg) != PUT_OK ) {
		if ( type == TypeHeartBeat ) {
			logit("et", "vecsum:  Error sending heartbeat.\n");
		}
		else if ( type == TypeError ) {
			logit("et", "vecsum:  Error sending error:%d.\n", ierr);
		}
	}

	return;
}

/*
 * vecsum_end() - free all the local memory & close socket
 */
static void vecsum_end( void )
{
	tport_detach( &InRegion );
	tport_detach( &OutRegion );
	scnlfilter_end( NULL );
	vecsum_filter_end();
	vecsum_tlist_end();

	return;
}

/**
 * @brief
 *
 */
#define OPERATION_SQUARE_MACRO(_DATA_PTR_TYPE, _TBUFF_PTR) \
		__extension__({ \
			_DATA_PTR_TYPE _dataptr = (_DATA_PTR_TYPE)(&(_TBUFF_PTR)->trh2x + 1); \
			_DATA_PTR_TYPE _dataptr_end = _dataptr + (_TBUFF_PTR)->trh2x.nsamp; \
			for ( ; _dataptr < _dataptr_end; _dataptr++ ) { \
				*_dataptr *= *_dataptr; \
			} \
		})
/**
 * @brief
 *
 * @param traceptr
 * @param tbufferptr
 */
static void operation_square( TracePacket *tbufferptr )
{
/* Go through all the double precision data */
	if ( tbufferptr->trh2x.datatype[1] == '8' )
		OPERATION_SQUARE_MACRO( double *, tbufferptr );
/* Go through all the single precision data */
	else
		OPERATION_SQUARE_MACRO( float *, tbufferptr );

	return;
}


/**
 * @brief
 *
 */
#define OPERATION_SUM_MACRO(_OUT_VS_PTR, _IN_DATA_PTR_TYPE, _IN_TBUFF_PTR) \
		__extension__({ \
			double _out_dataptr = (_OUT_VS_PTR)->buffer; \
			_IN_DATA_PTR_TYPE _in_dataptr = (_IN_DATA_PTR_TYPE)(&(_IN_TBUFF_PTR)->trh2x + 1); \
			_IN_DATA_PTR_TYPE _in_dataptr_end = _in_dataptr + (_IN_TBUFF_PTR)->trh2x.nsamp; \
			for ( ; _in_dataptr < _in_dataptr_end; _in_dataptr++, _out_dataptr++ ) { \
				*_out_dataptr += *_in_dataptr; \
			} \
		})

/**
 * @brief
 *
 */
#define OPERATION_SUM_MACRO(_OUT_VS_PTR, _IN_DATA_PTR_TYPE, _IN_TBUFF_PTR) \
		__extension__({ \
			double _out_dataptr = (_OUT_VS_PTR)->buffer; \
			_IN_DATA_PTR_TYPE _in_dataptr = (_IN_DATA_PTR_TYPE)(&(_IN_TBUFF_PTR)->trh2x + 1); \
			_IN_DATA_PTR_TYPE _in_dataptr_end = _in_dataptr + (_IN_TBUFF_PTR)->trh2x.nsamp; \
			for ( ; _in_dataptr < _in_dataptr_end; _in_dataptr++, _out_dataptr++ ) { \
				*_out_dataptr += *_in_dataptr; \
			} \
		})
static int insert_composition( VECSUM_INFO *vecsumptr, _TRACEINFO *traceptr, TracePacket *tracebuf )
{
	TRACE2X_HEADER *in_trh2x = &tracebuf->trh2x;

	if ( fabs(in_trh2x->samprate - vecsumptr->samprate) > FLT_EPSILON )
		return -1;

	if ( nsamp ) {
		int start_pos = (int)((in_trh2x->starttime - vecsumptr->endtime[traceptr->comp_seq]) / vecsumptr->delta);
		int end_pos   = (in_trh2x->endtime - vs_trh2x->starttime) / vecsumptr->delta;

		if ( start_pos < 0 ) {
			start_pos = abs(start_pos);
			memmove(in_trh2x + 1, (double *)(in_trh2x + 1) + start_pos, in_trh2x->nsamp * sizeof(double));
		}
		else if ( start_pos > 0 ) {

		}
			start_pos = 0;
		for ( int i = start_pos; i < end_pos; i++ ) {

		}
	}
	else {
		vecsumptr->starttime = in_trh2x->starttime;
		vecsumptr->endtime[traceptr->comp_seq] = in_trh2x->endtime;
	}
	OPERATION_SUM_MACRO(vecsumptr, double *, tracebuf);
}