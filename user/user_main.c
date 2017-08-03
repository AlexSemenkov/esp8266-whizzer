/*
 * user_main.c
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
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void user_init(void) {
	uart0_sendStr("\r\nuser_init\r\n");
	os_printf("user_init\n");
	os_printf("SDK version: %s\n", system_get_sdk_version());

//	wifi_set_opmode(STATIONAP_MODE); //Set softAP + station mode
//
//	char ssid[32] = "ZTE19";
//	char password[64] = "WSX256478911qsc";
//	struct station_config stationConf;
//	stationConf.bssid_set = 0; //need not check MAC address of
//	os_memcpy(&stationConf.ssid, ssid, 32);
//	os_memcpy(&stationConf.password, password, 64);
//	wifi_station_set_config(&stationConf);

	// Initialize LEDs pins
    os_printf("user_leds_init\n");
	user_leds_init();

    // Initialization of the peripheral drivers
    // Check whether assigned ip address by the router. If so, connect to ESP-server
    os_printf("user_esp_platform_init\n");
    user_esp_platform_init();

    // Establish a TCP server for http (with JSON) POST or GET command to communicate with the device
    os_printf("user_webserver_init\n");
    user_webserver_init(SERVER_PORT);

    // Initialize whizzer pins
    os_printf("user_whizzer_init\n");
    user_whizzer_init();


    os_printf("wifi_get_opmode %d\n", wifi_get_opmode());

    struct station_config stationConf2;

    wifi_station_get_config(&stationConf2);
    os_printf("bssid_set %d\n", stationConf2.bssid_set);
    os_printf("bssid %s\n", stationConf2.bssid);
    os_printf("ssid %s\n", stationConf2.ssid);
    os_printf("password %s\n", stationConf2.password);

    wifi_station_get_config_default(&stationConf2);
    os_printf("bssid_set %d\n", stationConf2.bssid_set);
    os_printf("bssid %s\n", stationConf2.bssid);
    os_printf("ssid %s\n", stationConf2.ssid);
    os_printf("password %s\n", stationConf2.password);
}


