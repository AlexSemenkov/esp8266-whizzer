/*
 *  user_webserver.h
 *
 *  Created on: 10 dec 2016
 *      Author: asemenkov
 */

#ifndef TEST_T_USER_WEBSERVER_H_
#define TEST_T_USER_WEBSERVER_H_

#define SERVER_PORT 			80
#define URL_SIZE 				10

#define UPGRADE_FLAG_IDLE       0x00
#define UPGRADE_FLAG_START      0x01
#define UPGRADE_FLAG_FINISH     0x02

#define LIMIT_ERASE_SIZE		0x10000

typedef enum ProtocolType {
	GET = 0,
	POST,
} ProtocolType;

typedef struct URL_Frame {
	enum ProtocolType Type;
	char pSelect[URL_SIZE];
	char pCommand[URL_SIZE];
	char pFileName[URL_SIZE];
} URL_Frame;

typedef enum _ParmType {
	SWITCH_STATUS = 0,
	INFOMATION,
	WIFI,
	SCAN,
	REBOOT,
	DEEP_SLEEP,
	LIGHT_STATUS,
	CONNECT_STATUS,
	USER_BIN
} ParmType;

typedef struct _rst_parm {
	ParmType parmType;
	struct espconn *espconnPtr;
} rst_parm;

#endif /* TEST_T_USER_WEBSERVER_H_ */
