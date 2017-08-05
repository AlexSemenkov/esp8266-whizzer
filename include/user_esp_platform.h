/*
 *  user_esp_platform.h
 *
 *  Created on: 10 dec 2016
 *      Author: asemenkov
 */

#ifndef TEST_T_USER_ESP_PLATFORM_H_
#define TEST_T_USER_ESP_PLATFORM_H_

/* NOTICE! - this is for 512KB spi flash.
 * You can change to other sector if you use other size spi flash. */
#define ESP_PARAM_START_SEC		0x3D
#define ESP_DBG 				os_printf

#define PACKET_SIZE 			(2 * 1024)
#define TOKEN_SIZE				41

struct esp_platform_saved_param {
	uint8 devKey[40];
	uint8 token[40];
	uint8 activeFlag;
	uint8 pad[3];
};

// Device iot.espressif.cn cloud connection status
enum {
	DEVICE_CONNECTING = 40,
	DEVICE_ACTIVE_DONE,
	DEVICE_ACTIVE_FAIL,
	DEVICE_CONNECT_SERVER_FAIL
};

struct dhcp_client_info {
	ip_addr_t ip_addr;
	ip_addr_t netmask;
	ip_addr_t gw;
	uint8 flag;
	uint8 pad[3];
};

void user_esp_platform_check_ip(uint8 reset_flag);

#endif /* TEST_T_USER_ESP_PLATFORM_H_ */
