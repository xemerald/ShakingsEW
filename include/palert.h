/*
 * palert.h
 *
 * Header file for Palert & PalertPlus data packet.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * May, 2018
 *
 */

#pragma once
/* */
#include <stdint.h>

/* */
#define PALERTMODE1_HEADER_LENGTH  200
#define PALERTMODE1_PACKET_LENGTH  1200
/* */
#define PALERTHEADER_LENGTH   PALERTMODE1_HEADER_LENGTH
#define PALERTPACKET_LENGTH   PALERTMODE1_PACKET_LENGTH
/*
 * Definition of Palert mode 1 header, total size is 200 bytes
 */
typedef struct {
/* Packet infomation */
	uint8_t  packet_type[2];
	uint8_t  event_flag[2];
/* System time infomation */
#ifdef _SPARC
	uint8_t sys_year[2];
	uint8_t sys_month[2];
	uint8_t sys_day[2];
	uint8_t sys_hour[2];
	uint8_t sys_minute[2];
#else
	uint16_t sys_year;
	uint16_t sys_month;
	uint16_t sys_day;
	uint16_t sys_hour;
	uint16_t sys_minute;
#endif
	uint8_t  sys_tenmsec;
	uint8_t  sys_second;
/* Event time infomation */
#ifdef _SPARC
	uint8_t ev_year[2];
	uint8_t ev_month[2];
	uint8_t ev_day[2];
	uint8_t ev_hour[2];
	uint8_t ev_minute[2];
#else
	uint16_t ev_year;
	uint16_t ev_month;
	uint16_t ev_day;
	uint16_t ev_hour;
	uint16_t ev_minute;
#endif
	uint8_t  ev_tenmsec;
	uint8_t  ev_second;
/* Hardware infomation */
	uint8_t  serial_no[2];
/* Warning & watch setting */
	uint8_t  dis_watch_threshold[2];
	uint8_t  pgv_1s[2];
	uint8_t  pgd_1s[2];
	uint8_t  pga_10s[2];
	uint8_t  pga_trig_axis[2];
	uint8_t  pd_warning_threshold[2];
	uint8_t  pga_warning_threshold[2];
	uint8_t  dis_warning_threshold[2];
	uint8_t  pd_flag[2];
	uint8_t  pd_watch_threshold[2];
	uint8_t  pga_watch_threshold[2];
/* Some value for EEW */
	uint8_t  intensity_now[2];
	uint8_t  intensity_max[2];
	uint8_t  pga_1s[2];
	uint8_t  pga_1s_axis[2];
	uint8_t  tauc[2];
	uint8_t  trig_mode[2];
	uint8_t  op_mode[2];
	uint8_t  dura_watch_warning[2];
/* Firmware version */
	uint8_t  firmware[2];
/* Network infomation */
	uint16_t palert_ip[4];
	uint8_t  tcp0_server[4];
	uint8_t  tcp1_server[4];
	uint16_t ntp_server[4];
	uint8_t  socket_remain[2];
	uint8_t  connection_flag[2];
/* EEW status */
	uint8_t  dio_status[2];
	uint8_t  eew_register[2];
/* Sensor summary */
	uint8_t  pd_vertical[2];
	uint8_t  pv_vertical[2];
	uint8_t  pa_vertical[2];
	uint8_t  vector_max[2];
	uint8_t  acc_a_max[2];
	uint8_t  acc_b_max[2];
	uint8_t  acc_c_max[2];
	uint8_t  accvec_a_max[2];
	uint8_t  accvec_b_max[2];
	uint8_t  accvec_c_max[2];
/* reserved byte */
	uint8_t  reserved_1[18];
/* synchronized character */
	uint8_t  sync_char[8];
/* Packet length */
	uint8_t  packet_len[2];
/* EEW DO intensity */
	uint8_t  eews_do0_intensity[2];
	uint8_t  eews_do1_intensity[2];
/* FTE-D04 information */
	uint8_t  fte_d04_server[4];
/* Operation mode X */
	uint8_t  op_mode_x[2];
/* White list information */
	uint8_t  whitelist_1[4];
	uint8_t  whitelist_2[4];
	uint8_t  whitelist_3[4];
/* Maximum vector velocity */
	uint8_t  vel_vector_max[2];
/* reserved byte */
	uint8_t  reserved_2[24];
/* Sampling rate */
	uint8_t  samprate[2];
} PALERTMODE1_HEADER;

/*
 *
 */
#define PALERT_SET_IP   0
#define PALERT_NTP_IP   1
#define PALERT_TCP0_IP  2
#define PALERT_TCP1_IP  3

/*
 *
 */
#define PALERTMODE1_HEADER_VEC_UNIT   0.1f          /* count to gal    */
#define PALERTMODE1_HEADER_ACC_UNIT   0.059814453f  /* count to gal    */
#define PALERTMODE1_HEADER_VEL_UNIT   0.01f         /* count to cm/sec */
#define PALERTMODE1_HEADER_DIS_UNIT   0.001f        /* count to cm     */

/*
 *
 */
#define PALERTMODE1_HEADER_OPX_WHITELIST_BIT        0x01
#define PALERTMODE1_HEADER_OPX_HORIZON_INSTALL_BIT  0x02
#define PALERTMODE1_HEADER_OPX_CWB2020_BIT          0x04

/*
 *
 */
#define PALERTMODE1_SAMPLE_NUMBER    100

/*
 * Palert default channel information
 */
#define PALERT_DEFAULT_SAMPRATE  100

#define PALERTMODE1_CHAN_TABLE \
		X(PALERTMODE1_CHAN_0,     "HLZ",  PALERTMODE1_HEADER_ACC_UNIT) \
		X(PALERTMODE1_CHAN_1,     "HLN",  PALERTMODE1_HEADER_ACC_UNIT) \
		X(PALERTMODE1_CHAN_2,     "HLE",  PALERTMODE1_HEADER_ACC_UNIT) \
		X(PALERTMODE1_CHAN_3,     "PD",   PALERTMODE1_HEADER_DIS_UNIT) \
		X(PALERTMODE1_CHAN_4,     "DIS",  PALERTMODE1_HEADER_DIS_UNIT) \
		X(PALERTMODE1_CHAN_COUNT, "NULL", 0                          )

#define X(a, b, c) a,
typedef enum {
	PALERTMODE1_CHAN_TABLE
} PALERTMODE1_CHANNEL;
#undef X

/*
 * Palert trigger mode information
 */
#define PALERT_TRIGMODE_VDIS_BIT     0x01
#define PALERT_TRIGMODE_PD_BIT       0x02
#define PALERT_TRIGMODE_PGA_BIT      0x04
#define PALERT_TRIGMODE_STA_LTA_BIT  0x08

#define PALERT_TRIGMODE_TABLE \
		X(PALERT_TRIGMODE_VDIS,    "vdisp",   0x01) \
		X(PALERT_TRIGMODE_PD,      "Pd",      0x02) \
		X(PALERT_TRIGMODE_PGA,     "PGA",     0x04) \
		X(PALERT_TRIGMODE_STA_LTA, "STA/LTA", 0x08) \
		X(PALERT_TRIGMODE_COUNT,   "NULL",    0xFF)

#define PALERT_TRIGMODE_STR_LENGTH  24

#define X(a, b, c) a,
typedef enum {
	PALERT_TRIGMODE_TABLE
} PALERT_TRIGMODES;
#undef X

/*
 * Definition of Palert data block structure, total size is 10 bytes
 */
typedef union {
#ifdef _SPARC
	struct {
		uint8_t acc[3][2];
		uint8_t pd[2];
		uint8_t dis[2];
	} norm_label;
	uint8_t cmp[PALERTMODE1_CHAN_COUNT][2];
#else
	struct {
		int16_t acc[3];
		int16_t pd;
		int16_t dis;
	} norm_label;
	int16_t cmp[PALERTMODE1_CHAN_COUNT];
#endif
} PALERTDATA;

/*
 * Definition of Palert generic packet structure, total size is 1200 bytes
 */
typedef struct {
	PALERTMODE1_HEADER pah;
	PALERTDATA         data[PALERTMODE1_SAMPLE_NUMBER];
} PalertPacket;

/*
 * Definition of Palert mode 4 header, total size is 64 bytes
 */
typedef struct {
/* packet infomation */
	uint8_t packet_type[2];
	uint8_t packet_len[2];
	uint8_t device_type;
	uint8_t channel_number;
	uint8_t crc16_byte[2];

/* hardware infomation */
	uint8_t firmware[2];
	uint8_t serial[2];
	uint8_t connection_flag[2];
	uint8_t trigger_flag[2];
	uint8_t op_mode[2];
	uint8_t dio_status[2];
	uint8_t filter_trigger_mode[2];

/* network infomation */
	uint8_t ntp_server[4];
	uint8_t tcp0_server[4];
	uint8_t tcp1_server[4];
	uint8_t tcp2_server[4];
	uint8_t admin0_server[4];
	uint8_t admin1_server[4];
	uint8_t palert_ip[4];
	uint8_t subnet_mask[4];
	uint8_t gateway_ip[4];

/* synchronized character */
	uint8_t sync_char[4];

/* reserved byte */
	uint8_t padding[2];
} PALERTMODE4_HEADER;

/*
 * Definition of Streamline mini-SEED data record structure
 */
/*
typedef struct {
	struct fsdh_s fsdh;
	uint16_t blkt_type;
	uint16_t next_blkt;
	struct blkt_1000_s blkt1000;
	uint8_t smsrlength[2];
	uint8_t reserved[6];
} SMSRECORD;
*/

/*
 * PALERT_IS_MODE1_HEADER()
 */
#define PALERT_IS_MODE1_HEADER(PAH) \
		(((PALERTMODE1_HEADER *)(PAH))->packet_type[0] & 0x01)

/*
 * PALERT_IS_MODE2_HEADER()
 */
#define PALERT_IS_MODE2_HEADER(PAH) \
		(((PALERTMODE1_HEADER *)(PAH))->packet_type[0] & 0x02)

/*
 * PALERT_IS_MODE4_HEADER()
 */
#define PALERT_IS_MODE4_HEADER(PAH) \
		(((PALERTMODE1_HEADER *)(PAH))->packet_type[0] & 0x03)

/*
 * PALERTMODE1_HEADER_GET_WORD()
 */
#define PALERTMODE1_HEADER_GET_WORD(PAM1H_WORD) \
		(((PAM1H_WORD)[1] << 8) + (PAM1H_WORD)[0])

/*
 * PALERTMODE1_HEADER_CHECK_NTP() -
 */
#define PALERTMODE1_HEADER_CHECK_NTP(PAM1H) \
		((PAM1H)->connection_flag[0] & 0x01)

/*
 * PALERTMODE1_HEADER_CHECK_SYNC() - Check the palert packet synchronization
 */
#define PALERTMODE1_HEADER_CHECK_SYNC(PAM1H) \
		((PAM1H)->sync_char[0] == 0x30 && (PAM1H)->sync_char[1] == 0x33	&& \
		(PAM1H)->sync_char[2] == 0x30 && (PAM1H)->sync_char[3] == 0x35 && \
		(PAM1H)->sync_char[4] == 0x31 && (PAM1H)->sync_char[5] == 0x35 && \
		(PAM1H)->sync_char[6] == 0x30 && (PAM1H)->sync_char[7] == 0x31)

/*
 * PALERTMODE1_HEADER_GET_PACKETLEN() - Parse the palert packet length
 */
#define PALERTMODE1_HEADER_GET_PACKETLEN(PAM1H) \
		PALERTMODE1_HEADER_GET_WORD((PAM1H)->packet_len)

/*
 * PALERTMODE1_HEADER_GET_SERIAL() - Parse the palert serial number
 */
#define PALERTMODE1_HEADER_GET_SERIAL(PAM1H) \
		PALERTMODE1_HEADER_GET_WORD((PAM1H)->serial_no)

/*
 * PALERTMODE1_HEADER_GET_FIRMWARE() - Parse the palert firmware version
 */
#define PALERTMODE1_HEADER_GET_FIRMWARE(PAM1H) \
		PALERTMODE1_HEADER_GET_WORD((PAM1H)->firmware)

/*
 * PALERTMODE1_HEADER_GET_DIO_STATUS() -
 */
#define PALERTMODE1_HEADER_GET_DIO_STATUS(PAM1H, DIO_NUMBER) \
		((DIO_NUMBER) < 8 ? \
		((PAM1H)->dio_status[0] & (0x01 << (DIO_NUMBER))) : \
		((PAM1H)->dio_status[1] & (0x01 << ((DIO_NUMBER) - 8))))

/*
 * PALERTMODE1_HEADER_GET_SAMPRATE() - Parse the palert sampling rate
 */
#define PALERTMODE1_HEADER_GET_SAMPRATE(PAM1H) \
		(PALERTMODE1_HEADER_GET_WORD((PAM1H)->samprate) ? \
		PALERTMODE1_HEADER_GET_WORD((PAM1H)->samprate) : PALERT_DEFAULT_SAMPRATE)

/*
 * PALERTMODE1_HEADER_GET_FIRMWARE() - Parse the palert firmware version
 */
#define PALERTMODE1_HEADER_IS_CWB2020(PAM1H) \
		((PAM1H)->op_mode_x[0] & PALERTMODE1_HEADER_OPX_CWB2020_BIT)


/* Export functions's prototypes */
double   palert_get_systime( const PALERTMODE1_HEADER *, long );
double   palert_get_evtime( const PALERTMODE1_HEADER *, long );
char    *palert_get_trigmode_str( const PALERTMODE1_HEADER * );
char    *palert_get_chan_code( const PALERTMODE1_CHANNEL );
double   palert_get_chan_unit( const PALERTMODE1_CHANNEL );
char    *palert_get_ip( const PALERTMODE1_HEADER *, const int, char * );
int32_t *palert_get_data( const PalertPacket *, const int, int32_t * );
int      palert_translate_cwb2020_int( const int );
