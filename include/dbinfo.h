/*
 * dbinfo.h
 *
 * Header file for database information struct.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * August, 2020
 *
 */

#pragma once

/* Define the character length of parameters */
#define  MAX_HOST_LENGTH      256
#define  MAX_USER_LENGTH      16
#define  MAX_PASSWORD_LENGTH  32
#define  MAX_DATABASE_LENGTH  64
#define  MAX_TABLE_LEGTH      64

/* Database login information */
typedef struct {
	char host[MAX_HOST_LENGTH];
	char user[MAX_USER_LENGTH];
	char password[MAX_PASSWORD_LENGTH];
	char database[MAX_DATABASE_LENGTH];
	long port;
} DBINFO;

#define DBINFO_INIT(_DB_INFO_) \
		((_DB_INFO_) = (DBINFO){ { 0 }, { 0 }, { 0 }, { 0 }, 0 })
