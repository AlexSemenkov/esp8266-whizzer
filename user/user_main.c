/*
 *  user_main.c
 *
 *  Created on: 10 dec 2016
 *      Author: asemenkov
 */

#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "user_interface.h"
#include "driver/uart.h"
#include "espconn.h"
#include "mem.h"
#include "gpio.h"

#include "user_config.h"
#include "user_webserver.h"
#include "user_whizzer.h"
#include "user_led.h"

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : don't remove as it may cause error with some SDK versions
 * Parameters   : none
 * Returns      : rf_cal_sec
 ******************************************************************************/
uint32 ICACHE_FLASH_ATTR user_rf_cal_sector_set(void) {
	enum flash_size_map size_map = system_get_flash_size_map();
	uint32 rf_cal_sec = 0;

	switch (size_map) {
	case FLASH_SIZE_4M_MAP_256_256:
		rf_cal_sec = 128 - 8;
		break;

	case FLASH_SIZE_8M_MAP_512_512:
		rf_cal_sec = 256 - 5;
		break;

	case FLASH_SIZE_16M_MAP_512_512:
	case FLASH_SIZE_16M_MAP_1024_1024:
		rf_cal_sec = 512 - 5;
		break;

	case FLASH_SIZE_32M_MAP_512_512:
	case FLASH_SIZE_32M_MAP_1024_1024:
		rf_cal_sec = 1024 - 5;
		break;

	default:
		rf_cal_sec = 0;
		break;
	}

	return rf_cal_sec;
}

/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
 *******************************************************************************/
void user_init(void) {
	uart0_sendStr("\r\nuser_init\r\n");
	os_printf("SDK version: %s\n", system_get_sdk_version());

	// Initialize LEDs pins
	user_leds_init();

	// Check whether IP is assigned by the router. If so, connect to ESP-server
	user_esp_platform_init();

	// Establish a TCP server for http POST or GET command to communicate with the device
	user_webserver_init(SERVER_PORT);

	// Initialize Whizzer pins
	user_whizzer_init();
}
