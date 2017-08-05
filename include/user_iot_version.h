/*
 *  user_iot_version.h
 *
 *  Created on: 10 dec 2016
 *      Author: asemenkov
 */

#ifndef TEST_T_USER_IOT_VERSION_H_
#define TEST_T_USER_IOT_VERSION_H_

#include "user_config.h"

#define IOT_VERSION_MAJOR		0U
#define IOT_VERSION_MINOR		0U
#define IOT_VERSION_REVISION	1U
#define VERSION_TYPE   	  		"v"

#define device_type     		200

#define ONLINE_UPGRADE  		0
#define LOCAL_UPGRADE   		1
#define ALL_UPGRADE     		0
#define NONE_UPGRADE			0

#if	ONLINE_UPGRADE
#define UPGRADE_FALG			"o"
#elif  LOCAL_UPGRADE
#define UPGRADE_FALG			"l"
#elif  ALL_UPGRADE
#define UPGRADE_FALG			"a"
#elif NONE_UPGRADE
#define UPGRADE_FALG			"n"
#endif

#endif /* TEST_T_USER_IOT_VERSION_H_ */
