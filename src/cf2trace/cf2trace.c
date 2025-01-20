/**
 * @file cf2trace.c
 * @author Benjamin Ming Yang (b98204032@gmail.com) @ Department of Geology, National Taiwan University
 * @brief
 * @date 2018-03-20
 *
 * @copyright Copyright (c) 2018
 *
 */
#ifdef _OS2
#define INCL_DOSMEMMGR
#define INCL_DOSSEMAPHORES
#include <os2.h>
#endif
/**
 * @name Standard C header include
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <threads.h>
#include <time.h>
/**
 * @name Earthworm environment header include
 *
 */
#include <earthworm.h>
#include <kom.h>
#include <transport.h>
#include <lockfile.h>
#include <trace_buf.h>
#include <swap.h>
/**
 * @name Local header include
 *
 */
#include <dbinfo.h>
#include <cf2trace.h>
#include <cf2trace_list.h>

/**
 * @name Functions prototype in this source file
 *
 */
static void cf2trace_config( const char * );
static void cf2trace_lookup( void );
static void cf2trace_status( unsigned char, short, char * );
static void cf2trace_end( void );                /* Free all the local memory & close socket */

static int thread_update_list( void * );
static int update_list_configfile( const char * );
static int apply_cf_tracedata_single( TracePacket *, const TracePacket *, const double );
static int apply_cf_tracedata_double( TracePacket *, const TracePacket *, const double );

/**
 * @name Ring messages things
 *
 */
static SHM_INFO InRegion;      /* shared memory region to use for i/o    */
static SHM_INFO OutRegion;     /* shared memory region to use for i/o    */
/* */
#define MAXLOGO  5
static MSG_LOGO Putlogo;           /* array for output module, type, instid     */
static MSG_LOGO Getlogo[MAXLOGO];  /* array for requesting module, type, instid */
static pid_t    MyPid;             /* for restarts by startstop                 */

/* */
#define MAXLIST  5
/**
 * @name Things to read or derive from configuration file
 *
 */
static char     InRingName[MAX_RING_STR];   /* name of transport ring for i/o    */
static char     OutRingName[MAX_RING_STR];  /* name of transport ring for i/o    */
static char     MyModName[MAX_MOD_STR];     /* speak as this module name/id      */
static uint8_t  LogSwitch;                  /* 0 if no logfile should be written */
static uint64_t HeartBeatInterval;          /* seconds between heartbeats        */
static uint64_t UpdateInterval = 0;         /* seconds between updating check    */
static uint8_t  FloatPrecision = CF2TRA_FLOAT_SINGLE;  /* */
static uint16_t nLogo = 0;
static DBINFO   DBInfo;
static char     SQLChannelTable[MAXLIST][MAX_TABLE_LEGTH];
static uint16_t nList = 0;

/**
 * @name Things to look up in the earthworm.h tables with getutil.c functions
 *
 */
static int64_t InRingKey;       /* key of transport ring for i/o     */
static int64_t OutRingKey;      /* key of transport ring for i/o     */
static uint8_t InstId;          /* local installation id             */
static uint8_t MyModId;         /* Module Id for this program        */
static uint8_t TypeHeartBeat;
static uint8_t TypeError;
static uint8_t TypeTracebuf2 = 0;

/**
 * @name Error messages used by cf2trace
 *
 */
#define ERR_MISSMSG       0   /* message missed in transport ring       */
#define ERR_TOOBIG        1   /* retreived msg too large for buffer     */
#define ERR_NOTRACK       2   /* msg retreived; tracking limit exceeded */
#define ERR_QUEUE         3   /* error queueing message for sending      */
static char Text[150];         /* string for log/error messages          */

/**
 * @name Update flag used by cf2trace
 *
 */
#define LIST_IS_UPDATED    0
#define LIST_NEED_UPDATED  1
#define LIST_UNDER_UPDATE  2

/**
 * @brief
 *
 */
static volatile uint8_t UpdateStatus = LIST_IS_UPDATED;

/**
 * @brief
 *
 */
#define RE_ARRANGE_TRACEDATA(_TRACE_PACKET_PTR, _DROP_NSAMP, _REST_NSAMP) \
		memmove( \
			&(_TRACE_PACKET_PTR)->trh2 + 1, \
			(uint8_t *)(&(_TRACE_PACKET_PTR)->trh2 + 1) + ((_TRACE_PACKET_PTR)->trh2.datatype[1] - '0') * (_DROP_NSAMP), \
			((_TRACE_PACKET_PTR)->trh2.datatype[1] - '0') * (_REST_NSAMP) \
		)

/**
 * @brief Main entry.
 *
 * @param argc
 * @param argv
 * @return int
 */
int main ( int argc, char **argv )
{
	int      i;
	int      res;
	int64_t  recsize = 0;
	MSG_LOGO reclogo;
	time_t   time_now;          /* current time                  */
	time_t   time_lastbeat;     /* time last heartbeat was sent  */
	time_t   time_lastupd;      /* time last updated             */
	char    *lockfile;
	int32_t  lockfile_fd;

	_TRACEINFO *traceptr;
	TracePacket tracebuffer_i;  /* message which is received from share ring    */
	TracePacket tracebuffer_o;  /* message which is sent to share ring    */
	thrd_t      tid_update;     /* Thread ID */

	int  (*apply_cf_func)( TracePacket *, const TracePacket *, const double );
	size_t odata_size  = 4;
	char   odata_type1 = '4';

/* Check command line arguments */
	if ( argc != 2 ) {
		fprintf(stderr, "Usage: cf2trace <configfile>\n");
		exit(0);
	}
/* Initialize name of log-file & open it */
	logit_init( argv[1], 0, 256, 1 );
/* Read the configuration file(s) */
	cf2trace_config( argv[1] );
	logit("" , "%s: Read command file <%s>\n", argv[0], argv[1]);
/* Read the channels list from remote database */
	for ( i = 0; i < nList; i++ ) {
		if ( cf2tra_list_db_fetch( SQLChannelTable[i], &DBInfo, CF2TRA_LIST_INITIALIZING ) < 0 ) {
			fprintf(stderr, "Something error when fetching channels list %s. Exiting!\n", SQLChannelTable[i]);
			exit(-1);
		}
	}
/* Checking total station number again */
	if ( !(i = cf2tra_list_total_channel_get()) ) {
		fprintf(stderr, "There is not any channel in the list after fetching. Exiting!\n");
		exit(-1);
	}
	else {
		logit("o", "cf2trace: There are total %d channel(s) in the list.\n", i);
		cf2tra_list_tree_activate();
	}
/* Look up important info from earthworm.h tables */
	cf2trace_lookup();
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
		logit("e","cf2trace: Cannot get pid. Exiting.\n");
		exit(-1);
	}
/* Build the message */
	Putlogo.instid = InstId;
	Putlogo.mod    = MyModId;
	Putlogo.type   = TypeTracebuf2;
/* Attach to Input/Output shared memory ring */
	tport_attach(&InRegion, InRingKey);
	logit("", "cf2trace: Attached to public memory region %s: %ld\n", InRingName, InRingKey);
/* Flush the transport ring */
	tport_flush(&InRegion, Getlogo, nLogo, &reclogo);
/* */
	tport_attach(&OutRegion, OutRingKey);
	logit("", "cf2trace: Attached to public memory region %s: %ld\n", OutRingName, OutRingKey);

/* Force a heartbeat to be issued in first pass thru main loop */
	time_lastupd = time_lastbeat = time(&time_now) - HeartBeatInterval - 1;
/* Assign the apply function pointer according to float precision */
	apply_cf_func = FloatPrecision == CF2TRA_FLOAT_DOUBLE ? apply_cf_tracedata_double : apply_cf_tracedata_single;
	odata_size    = FloatPrecision == CF2TRA_FLOAT_DOUBLE ? 8 : 4;
	odata_type1   = '0' + odata_size;
/*----------------------- setup done; start main loop -------------------------*/
	while ( 1 ) {
	/* Send cf2trace's heartbeat */
		if ( time(&time_now) - time_lastbeat >= (int64_t)HeartBeatInterval ) {
			time_lastbeat = time_now;
			cf2trace_status( TypeHeartBeat, 0, "" );
		}
	/* */
		if (
			UpdateInterval &&
			UpdateStatus == LIST_NEED_UPDATED &&
			(time_now - time_lastupd) >= (int64_t)UpdateInterval
		) {
			time_lastupd = time_now;
			if ( thrd_create(&tid_update, thread_update_list, argv[1]) != thrd_success ) {
				logit("e", "cf2trace: Error starting the thread(thread_update_list)!\n");
				UpdateStatus = LIST_IS_UPDATED;
			}
		}

	/* Process all new messages */
		do {
		/* See if a termination has been requested */
			res = tport_getflag( &InRegion );
			if ( res == TERMINATE || res == MyPid ) {
			/* write a termination msg to log file */
				logit("t", "cf2trace: Termination requested; exiting!\n");
				fflush(stdout);
			/* */
				goto exit_procedure;
			}

		/* Get msg & check the return code from transport */
			res = tport_getmsg(&InRegion, Getlogo, nLogo, &reclogo, &recsize, tracebuffer_i.msg, MAX_TRACEBUF_SIZ);
		/* No more new messages     */
			if ( res == GET_NONE ) {
				break;
			}
		/* Next message was too big */
			else if ( res == GET_TOOBIG ) {
			/* Complain and try again   */
				sprintf(
					Text, "Retrieved msg[%ld] (i%u m%u t%u) too big for Buffer[%ld]",
					recsize, reclogo.instid, reclogo.mod, reclogo.type, sizeof(tracebuffer_i) - 1
				);
				cf2trace_status( TypeError, ERR_TOOBIG, Text );
				continue;
			}
		/* Got a msg, but missed some */
			else if ( res == GET_MISS ) {
				sprintf(
					Text, "Missed msg(s)  i%u m%u t%u  %s.", reclogo.instid, reclogo.mod, reclogo.type, InRingName
				);
				cf2trace_status( TypeError, ERR_MISSMSG, Text );
			}
		/* Got a msg, but can't tell */
			else if ( res == GET_NOTRACK ) {
			/* If any were missed        */
				sprintf(
					Text, "Msg received (i%u m%u t%u); transport.h NTRACK_GET exceeded",
					reclogo.instid, reclogo.mod, reclogo.type
				);
				cf2trace_status( TypeError, ERR_NOTRACK, Text );
			}

		/* Process the message */
			if ( reclogo.type == TypeTracebuf2 ) {
				if ( TRACE2_HEADER_VERSION_IS_20(&tracebuffer_i.trh2) ) {
				/* Swap the byte order to local order */
					if ( WaveMsg2MakeLocal(&tracebuffer_i.trh2) ) {
						logit(
							"e", "cf2trace: SCNL %s.%s.%s.%s byte order swap error, please check it!\n",
							tracebuffer_i.trh2.sta, tracebuffer_i.trh2.chan, tracebuffer_i.trh2.net,
							tracebuffer_i.trh2.loc
						);
						continue;
					}
					tracebuffer_i.trh2x.version[1] = TRACE2_VERSION11;
				}
				else if ( TRACE2_HEADER_VERSION_IS_21(&tracebuffer_i.trh2) ) {
				/* Swap the byte order to local order */
					if ( WaveMsg2XMakeLocal(&tracebuffer_i.trh2x) ) {
						logit(
							"e", "cf2trace: SCNL %s.%s.%s.%s byte order swap error, please check it!\n",
							tracebuffer_i.trh2x.sta, tracebuffer_i.trh2x.chan, tracebuffer_i.trh2x.net,
							tracebuffer_i.trh2x.loc
						);
						continue;
					}
				}
				else {
					logit(
						"e", "cf2trace: SCNL %s.%s.%s.%s version is invalid, please check it!\n",
						tracebuffer_i.trh2.sta, tracebuffer_i.trh2.chan, tracebuffer_i.trh2.net, tracebuffer_i.trh2.loc
					);
					continue;
				}

				if ( (traceptr = cf2tra_list_find( &tracebuffer_i.trh2x )) == NULL ) {
				/* Not found in trace table */
				#ifdef _SEW_DEBUG
					printf("cf2trace: %s.%s.%s.%s not found in trace table, maybe it's a new trace.\n",
					tracebuffer_i.trh2.sta, tracebuffer_i.trh2.chan, tracebuffer_i.trh2.net, tracebuffer_i.trh2.loc);
				#endif
				/* Force to update the table */
					if ( UpdateStatus == LIST_IS_UPDATED )
						UpdateStatus = LIST_NEED_UPDATED;
					continue;
				}
			/* Copy all the header data into the output buffer */
				memcpy(tracebuffer_o.msg, tracebuffer_i.msg, sizeof(TRACE2X_HEADER));
			/* Temporally use this space to store the record type*/
				tracebuffer_o.trh2x.pinno                   = traceptr->recordtype;
				tracebuffer_o.trh2x.x.v21.conversion_factor = traceptr->conversion_factor;
			/* */
				tracebuffer_o.trh2x.datatype[1] = odata_type1;
				tracebuffer_o.trh2x.datatype[2] = '\0';
			#if defined( _SPARC )
				tracebuffer_o.trh2x.datatype[0] = 't';      /* SUN IEEE floating number */
			#elif defined( _INTEL )
				tracebuffer_o.trh2x.datatype[0] = 'f';      /* VAX/Intel IEEE floating number */
			#else
				printf("cf2trace: WARNING! _INTEL and _SPARC are both undefined!\n");
			#endif
			/* */
				do {
				/* Output buffer part */
					tracebuffer_o.trh2x.nsamp     = apply_cf_func( &tracebuffer_o, &tracebuffer_i, traceptr->conversion_factor );
					tracebuffer_o.trh2x.starttime = tracebuffer_i.trh2.starttime;
					tracebuffer_o.trh2x.endtime   = tracebuffer_o.trh2x.starttime + (tracebuffer_o.trh2x.nsamp - 1) / tracebuffer_o.trh2x.samprate;
				/* Output the package trace data to ring */
					recsize = sizeof(TRACE2X_HEADER) + tracebuffer_o.trh2x.nsamp * odata_size;
					if ( tport_putmsg( &OutRegion, &Putlogo, recsize, tracebuffer_o.msg ) != PUT_OK )
						logit("e", "cf2trace: Error putting message in region %ld\n", OutRegion.key);
				/* We still have some samples inside source buffer need to be processed next turn */
					if ( (tracebuffer_i.trh2.nsamp -= tracebuffer_o.trh2x.nsamp) > 0 ) {
					/* Source buffer part */
						tracebuffer_i.trh2.starttime = tracebuffer_o.trh2x.endtime + 1.0 / tracebuffer_i.trh2.samprate;
						RE_ARRANGE_TRACEDATA( &tracebuffer_i, tracebuffer_o.trh2x.nsamp, tracebuffer_i.trh2.nsamp );
					}
				} while ( tracebuffer_i.trh2.nsamp > 0 );
			}
		} while ( 1 );  /* end of message-processing-loop */
		sleep_ew(50);  /* no more messages; wait for new ones to arrive */
	}
/*-----------------------------end of main loop-------------------------------*/
exit_procedure:
/* detach from shared memory */
	cf2trace_end();

	ew_unlockfile(lockfile_fd);
	ew_unlink_lockfile(lockfile);

	return 0;
}

/**
 * @brief # of required commands you expect to process
 *
 */
#define CONDFIG_NUM_COMMAND  11

/**
 * @brief processes command file(s) using kom.c functions;
 *        exits if any errors are encountered.
 *
 * @param configfile
 */
static void cf2trace_config( const char *configfile )
{
	char  init[CONDFIG_NUM_COMMAND];  /* init flags, one byte for each required command */
	char *com;
	char *str;

	int nmiss;                        /* number of required commands that were missed   */
	int nfiles;
	int success;
	int i;

/* Set to zero one init flag for each required command */
	for( i = 0; i < CONDFIG_NUM_COMMAND; i++ )
		init[i] = i < 6 ? 0 : 1;
/* Open the main configuration file */
	nfiles = k_open(configfile);
	if ( nfiles == 0 ) {
		logit("e", "cf2trace: Error opening command file <%s>; exiting!\n", configfile);
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
				success = nfiles + 1;
				nfiles  = k_open(&com[1]);
				if ( nfiles != success ) {
					logit("e", "cf2trace: Error opening command file <%s>; exiting!\n", &com[1]);
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
			else if ( k_its("UpdateInterval") ) {
				UpdateInterval = k_long();
				if ( UpdateInterval )
					logit(
						"o", "cf2trace: Change to auto updating mode, the updating interval is %ld seconds!\n",
						UpdateInterval
					);
			}
			else if ( k_its("FloatPrecision") ) {
				str = k_str();
				if ( str ) {
					char typestr[8];
				#define X(a, b) b,
					char *_typestr[] = {
						CF2TRA_FLOAT_PRECISION_TABLE
					};
				#undef X
					memcpy(typestr, str, 8);
					for ( i = 0; i < 8; i++ )
						typestr[i] = tolower(typestr[i]);
				/* Compare the type string */
					for ( i = 0; i < CF2TRA_FLOAT_COUNT; i++ ) {
						if ( !memcmp(typestr, _typestr[i], strlen(_typestr[i])) ) {
							FloatPrecision = i;
							break;
						}
					}
					if ( i >= CF2TRA_FLOAT_COUNT ) {
						logit("e", "respectra: Invalid output float precision <%s>", str);
						logit("e", " in <FloatPrecision> cmd; use default setting!\n");
					}
				}
			}
			else if ( k_its("SQLHost") ) {
				str = k_str();
				if ( str )
					strcpy(DBInfo.host, str);
#if defined( _USE_SQL )
				for ( i = 6; i < CONDFIG_NUM_COMMAND; i++ )
					init[i] = 0;
#endif
			}
		/* 6 */
			else if ( k_its("SQLPort") ) {
				DBInfo.port = k_long();
				init[6] = 1;
			}
		/* 7 */
			else if ( k_its("SQLUser") ) {
				str = k_str();
				if ( str )
					strcpy(DBInfo.user, str);
				init[7] = 1;
			}
		/* 8 */
			else if ( k_its("SQLPassword") ) {
				str = k_str();
				if ( str )
					strcpy(DBInfo.password, str);
				init[8] = 1;
			}
		/* 9 */
			else if ( k_its("SQLDatabase") ) {
				str = k_str();
				if ( str )
					strcpy(DBInfo.database, str);
				init[9] = 1;
			}
		/* 10 */
			else if ( k_its("SQLChannelTable") ) {
				if ( nList >= MAXLIST ) {
					logit("e", "cf2trace: Too many <SQLChannelTable> commands in <%s>", configfile);
					logit("e", "; max=%d; exiting!\n", (int)MAXLIST);
					exit(-1);
				}
				if ( (str = k_str()) )
					strcpy(SQLChannelTable[nList], str);
				nList++;
				init[10] = 1;
			}
			else if ( k_its("SCNL") ) {
				str = k_get();
				for ( str += strlen(str) + 1; isspace(*str); str++ );
				if ( cf2tra_list_chan_line_parse( str, CF2TRA_LIST_INITIALIZING ) ) {
					logit(
						"e", "cf2trace: ERROR, lack of some channels information for in <%s>. Exiting!\n",
						configfile
					);
					exit(-1);
				}
			}
		/* Enter installation & module to get event messages from */
		/* 5 */
			else if ( k_its("GetEventsFrom") ) {
				if ( (nLogo + 1) >= MAXLOGO ) {
					logit("e", "cf2trace: Too many <GetEventsFrom> commands in <%s>", configfile);
					logit("e", "; max=%d; exiting!\n", (int)MAXLOGO);
					exit(-1);
				}
				if ( (str = k_str()) ) {
					if ( GetInst(str, &Getlogo[nLogo].instid) != 0 ) {
						logit("e", "cf2trace: Invalid installation name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				if ( (str = k_str()) ) {
					if ( GetModId(str, &Getlogo[nLogo].mod) != 0 ) {
						logit("e", "cf2trace: Invalid module name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				if ( (str = k_str()) ) {
					if ( GetType(str, &Getlogo[nLogo].type) != 0 ) {
						logit("e", "cf2trace: Invalid message type name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				nLogo++;
				init[5] = 1;
			}
		/* Unknown command */
			else {
				logit("e", "cf2trace: <%s> Unknown command in <%s>.\n", com, configfile);
				continue;
			}

		/* See if there were any errors processing the command */
			if ( k_err() ) {
				logit("e", "cf2trace: Bad <%s> command in <%s>; exiting!\n", com, configfile);
				exit( -1 );
			}
		}
		nfiles = k_close();
	}

/* After all files are closed, check init flags for missed commands */
	nmiss = 0;
	for ( i = 0; i < CONDFIG_NUM_COMMAND; i++ )
		if ( !init[i] )
			nmiss++;
/* */
	if ( nmiss ) {
		logit("e", "cf2trace: ERROR, no ");
		if ( !init[0] )  logit("e", "<LogFile> "            );
		if ( !init[1] )  logit("e", "<MyModuleId> "         );
		if ( !init[2] )  logit("e", "<InWaveRing> "         );
		if ( !init[3] )  logit("e", "<OutWaveRing> "        );
		if ( !init[4] )  logit("e", "<HeartBeatInterval> "  );
		if ( !init[5] )  logit("e", "any <GetEventsFrom> "  );
		if ( !init[6] )  logit("e", "<SQLPort> "            );
		if ( !init[7] )  logit("e", "<SQLUser> "            );
		if ( !init[8] )  logit("e", "<SQLPassword> "        );
		if ( !init[9] )  logit("e", "<SQLDatabase> "        );
		if ( !init[10] ) logit("e", "any <SQLChannelTable> ");

		logit("e", "command(s) in <%s>; exiting!\n", configfile);
		exit(-1);
	}

	return;
}

/**
 * @brief Look up important info from earthworm.h tables
 *
 */
static void cf2trace_lookup( void )
{
/* Look up keys to shared memory regions */
	if ( ( InRingKey = GetKey(InRingName) ) == -1 ) {
		fprintf(stderr, "cf2trace:  Invalid ring name <%s>; exiting!\n", InRingName);
		exit(-1);
	}
	if ( ( OutRingKey = GetKey(OutRingName) ) == -1 ) {
		fprintf(stderr, "cf2trace:  Invalid ring name <%s>; exiting!\n", OutRingName);
		exit(-1);
	}
/* Look up installations of interest */
	if ( GetLocalInst(&InstId) != 0 ) {
		fprintf(stderr, "cf2trace: error getting local installation id; exiting!\n");
		exit(-1);
	}
/* Look up modules of interest */
	if ( GetModId(MyModName, &MyModId) != 0 ) {
		fprintf(stderr, "cf2trace: Invalid module name <%s>; exiting!\n", MyModName);
		exit(-1);
	}
/* Look up message types of interest */
	if ( GetType("TYPE_HEARTBEAT", &TypeHeartBeat) != 0 ) {
		fprintf(stderr, "cf2trace: Invalid message type <TYPE_HEARTBEAT>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_ERROR", &TypeError) != 0 ) {
		fprintf(stderr, "cf2trace: Invalid message type <TYPE_ERROR>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_TRACEBUF2", &TypeTracebuf2) != 0 ) {
		fprintf(stderr, "cf2trace: Invalid message type <TYPE_TRACEBUF2>; exiting!\n");
		exit(-1);
	}

	return;
}

/**
 * @brief Builds a heartbeat or error message & puts it into shared memory.
 *        Writes errors to log file & screen.
 *
 * @param type
 * @param ierr
 * @param note
 */
static void cf2trace_status( unsigned char type, short ierr, char *note )
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
		logit("et", "cf2trace: %s\n", note);
	}

	size = strlen(msg);  /* don't include the null byte in the message */

/* Write the message to shared memory */
	if ( tport_putmsg(&InRegion, &logo, size, msg) != PUT_OK ) {
		if ( type == TypeHeartBeat ) {
			logit("et", "cf2trace: Error sending heartbeat.\n");
		}
		else if ( type == TypeError ) {
			logit("et", "cf2trace: Error sending error:%d.\n", ierr);
		}
	}

	return;
}

/**
 * @brief Free all the local memory & detach from the shared memory
 *
 */
static void cf2trace_end( void )
{
	tport_detach(&InRegion);
	tport_detach(&OutRegion);
	cf2tra_list_end();

	return;
}

/**
 * @brief
 *
 * @param arg
 * @return int
 */
static int thread_update_list( void *arg )
{
	int update_flag = 0;

	logit("ot", "cf2trace: Updating the channels list...\n");
	UpdateStatus = LIST_UNDER_UPDATE;
/* */
	for ( int i = 0; i < nList; i++ ) {
		if ( cf2tra_list_db_fetch( SQLChannelTable[i], &DBInfo, CF2TRA_LIST_UPDATING ) < 0 ) {
			logit("e", "cf2trace: Fetching channels list(%s) from remote database error!\n", SQLChannelTable[i]);
			update_flag = 1;
		}
	}
/* */
	if ( update_list_configfile( (char *)arg ) ) {
		logit("e", "cf2trace: Fetching channels list from local file error!\n");
		update_flag = 1;
	}
/* */
	if ( update_flag ) {
		cf2tra_list_tree_abandon();
		logit("e", "cf2trace: Failed to update the channels list!\n");
		logit("ot", "cf2trace: Keep using the previous channels list(%ld)!\n", cf2tra_list_timestamp_get());
	}
	else {
		cf2tra_list_tree_activate();
		logit("ot", "cf2trace: Successfully updated the channels list(%ld)!\n", cf2tra_list_timestamp_get());
		logit(
			"ot", "cf2trace: There are total %d channels in the new channels list.\n", cf2tra_list_total_channel_get()
		);
	}

/* */
	UpdateStatus = LIST_IS_UPDATED;
/* Just exit this thread */
	KillSelfThread();

	return 0;
}

/**
 * @brief
 *
 * @param configfile
 * @return int
 */
static int update_list_configfile( const char *configfile )
{
	char *com;
	char *str;
	int   nfiles;
	int   success;

/* Open the main configuration file */
	nfiles = k_open(configfile);
	if ( nfiles == 0 ) {
		logit("e","cf2trace: Error opening command file <%s> when updating!\n", configfile);
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
					logit("e", "cf2trace: Error opening command file <%s> when updating!\n", &com[1]);
					return -1;
				}
				continue;
			}

		/* Process only "SCNL" command */
			if ( k_its("SCNL") ) {
				str = k_get();
				for ( str += strlen(str) + 1; isspace(*str); str++ );
				if ( cf2tra_list_chan_line_parse( str, CF2TRA_LIST_UPDATING ) ) {
					logit(
						"e", "cf2trace: Some errors occured in <%s> when updating!\n",
						configfile
					);
					return -1;
				}
			}
		/* See if there were any errors processing the command */
			if ( k_err() ) {
				logit("e", "cf2trace: Bad <%s> command in <%s> when updating!\n", com, configfile);
				return -1;
			}
		}
		nfiles = k_close();
	}

	return 0;
}

/**
 * @brief
 *
 * @param dest
 * @param src
 * @param cfactor
 * @return int
 */
static int apply_cf_tracedata_single( TracePacket *dest, const TracePacket *src, const double cfactor )
{
	float       *destdata_f    = (float *)(&dest->trh2x + 1);
	float       *destdata_fend = (float *)(&dest->msg[MAX_TRACEBUF_SIZ]);
	register int dest_nsamp    = 0;

/* */
	if ( dest == src )
		return -1;
/* Source datatype is integer */
	if ( src->trh2.datatype[1] == '4' && (src->trh2.datatype[0] == 'i' || src->trh2.datatype[0] == 's') ) {
		const int32_t *srcdata_i    = (int32_t *)(&src->trh2 + 1);
		const int32_t *srcdata_iend = srcdata_i + src->trh2.nsamp;
	/* */
		for ( ; srcdata_i < srcdata_iend && destdata_f < destdata_fend; srcdata_i++, destdata_f++ ) {
			*destdata_f = *srcdata_i * cfactor;
			dest_nsamp++;
		}
	}
/* Source datatype is single precision floating number */
	else if ( src->trh2.datatype[1] == '4' && (src->trh2.datatype[0] == 'f' || src->trh2.datatype[0] == 't') ) {
		const float *srcdata_f    = (float *)(&src->trh2 + 1);
		const float *srcdata_fend = srcdata_f + src->trh2.nsamp;
	/* */
		for ( ; srcdata_f < srcdata_fend && destdata_f < destdata_fend; srcdata_f++, destdata_f++ ) {
			*destdata_f = *srcdata_f * cfactor;
			dest_nsamp++;
		}
	}
/* Source datatype is short integer */
	else if ( src->trh2.datatype[1] == '2' ) {
		const int16_t *srcdata_s    = (int16_t *)(&src->trh2 + 1);
		const int16_t *srcdata_send = srcdata_s + src->trh2.nsamp;
	/* */
		for ( ; srcdata_s < srcdata_send && destdata_f < destdata_fend; srcdata_s++, destdata_f++ ) {
			*destdata_f = *srcdata_s * cfactor;
			dest_nsamp++;
		}
	}
/* Source datatype is double precision floating number */
	else if ( src->trh2.datatype[1] == '8' ) {
		const double *srcdata_d    = (double *)(&src->trh2 + 1);
		const double *srcdata_dend = srcdata_d + src->trh2.nsamp;
	/* */
		for ( ; srcdata_d < srcdata_dend && destdata_f < destdata_fend; srcdata_d++, destdata_f++ ) {
			*destdata_f = *srcdata_d * cfactor;
			dest_nsamp++;
		}
	}

	return dest_nsamp;
}

/**
 * @brief
 *
 * @param dest
 * @param src
 * @param cfactor
 * @return int
 */
static int apply_cf_tracedata_double( TracePacket *dest, const TracePacket *src, const double cfactor )
{
	double      *destdata_d    = (double *)(&dest->trh2x + 1);
	double      *destdata_dend = (double *)(&dest->msg[MAX_TRACEBUF_SIZ]);
	register int dest_nsamp    = 0;

/* */
	if ( dest == src )
		return -1;
/* Source datatype is integer */
	if ( src->trh2.datatype[1] == '4' && (src->trh2.datatype[0] == 'i' || src->trh2.datatype[0] == 's') ) {
		const int32_t *srcdata_i    = (int32_t *)(&src->trh2 + 1);
		const int32_t *srcdata_iend = srcdata_i + src->trh2.nsamp;
	/* */
		for ( ; srcdata_i < srcdata_iend && destdata_d < destdata_dend; srcdata_i++, destdata_d++ ) {
			*destdata_d = *srcdata_i * cfactor;
			dest_nsamp++;
		}
	}
/* Source datatype is single precision floating number */
	else if ( src->trh2.datatype[1] == '4' && (src->trh2.datatype[0] == 'f' || src->trh2.datatype[0] == 't') ) {
		const float *srcdata_f    = (float *)(&src->trh2 + 1);
		const float *srcdata_fend = srcdata_f + src->trh2.nsamp;
	/* */
		for ( ; srcdata_f < srcdata_fend && destdata_d < destdata_dend; srcdata_f++, destdata_d++ ) {
			*destdata_d = *srcdata_f * cfactor;
			dest_nsamp++;
		}
	}
/* Source datatype is short integer */
	else if ( src->trh2.datatype[1] == '2' ) {
		const int16_t *srcdata_s    = (int16_t *)(&src->trh2 + 1);
		const int16_t *srcdata_send = srcdata_s + src->trh2.nsamp;
	/* */
		for ( ; srcdata_s < srcdata_send && destdata_d < destdata_dend; srcdata_s++, destdata_d++ ) {
			*destdata_d = *srcdata_s * cfactor;
			dest_nsamp++;
		}
	}
/* Source datatype is double precision floating number */
	else if ( src->trh2.datatype[1] == '8' ) {
		const double *srcdata_d    = (double *)(&src->trh2 + 1);
		const double *srcdata_dend = srcdata_d + src->trh2.nsamp;
	/* */
		for ( ; srcdata_d < srcdata_dend && destdata_d < destdata_dend; srcdata_d++, destdata_d++ ) {
			*destdata_d = *srcdata_d * cfactor;
			dest_nsamp++;
		}
	}

	return dest_nsamp;
}
