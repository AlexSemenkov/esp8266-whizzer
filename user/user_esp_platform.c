/*
 * user_esp_platform.c
 *
 *  Created on: 10 jul 2016
 *      Author: Espressif Systems (Shanghai) Pte., Ltd.
 */

#include <ctype.h>

#include "ets_sys.h"
#include "os_type.h"
#include "mem.h"
#include "osapi.h"
#include "user_interface.h"

#include "espconn.h"
#include "upgrade.h"

#include "user_esp_platform.h"
#include "user_whizzer.h"
#include "user_iot_version.h"
#include "user_config.h"

#define ACTIVE_FRAME    	"{\"nonce\": %d,\"path\": \"/v1/device/activate/\", \"method\": \"POST\", \"body\": {\"encrypt_method\": \"PLAIN\", \"token\": \"%s\", \"bssid\": \""MACSTR"\",\"rom_version\":\"%s\"}, \"meta\": {\"Authorization\": \"token %s\"}}\n"
#define FIRST_FRAME     	"{\"nonce\": %d, \"path\": \"/v1/device/identify\", \"method\": \"GET\",\"meta\": {\"Authorization\": \"token %s\"}}\n"
#define BEACON_FRAME    	"{\"path\": \"/v1/ping/\", \"method\": \"POST\",\"meta\": {\"Authorization\": \"token %s\"}}\n"

#define RESPONSE_FRAME  	"{\"status\": 200, \"emulating_steps\": {\"steps_num\": %d, \"pwm_cycle_decay_stop\": %d}, \"nonce\": %d, \"deliver_to_device\": true}\n"

#define ERROR_FRAME			"{\"status\": 200, \"error\": {\"code\": %d, \"message\": %s}, \"nonce\": %d, \"deliver_to_device\": true}\n"
#define ERROR_VALUE_FRAME	"{\"status\": 200, \"error\": {\"code\": %d, \"message\": %s%d\"}, \"nonce\": %d, \"deliver_to_device\": true}\n"

#define RPC_RESPONSE_FRAME  "{\"status\": 200, \"nonce\": %d, \"deliver_to_device\": true}\n"
#define UPGRADE_FRAME  		"{\"path\": \"/v1/messages/\", \"method\": \"POST\", \"meta\": {\"Authorization\": \"token %s\"},\"get\":{\"action\":\"%s\"},\"body\":{\"pre_rom_version\":\"%s\",\"rom_version\":\"%s\"}}\n"

#define pheadbuffer "Connection: keep-alive\r\n\
Cache-Control: no-cache\r\n\
User-Agent: Mozilla/5.0 (Windows NT 5.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/30.0.1599.101 Safari/537.36 \r\n\
Accept: */*\r\n\
Authorization: token %s\r\n\
Accept-Encoding: gzip,deflate,sdch\r\n\
Accept-Language: zh-CN,zh;q=0.8\r\n\r\n"

struct esp_platform_saved_param esp_param;
LOCAL struct espconn user_conn;
LOCAL struct _esp_tcp user_tcp;

LOCAL os_timer_t beacon_timer;
LOCAL os_timer_t client_timer;

LOCAL uint8 device_recon_count = 0;
ip_addr_t esp_server_ip;

LOCAL uint8  device_status;
LOCAL uint32 active_nonce = 0;
LOCAL uint8  iot_version[20] = {0};
LOCAL uint8  ping_status;


/******************************************************************************
 * FunctionName : user_esp_platform_set_token
 * Description  : save the token for the espressif's device
 * Parameters   : token -- the parameter pointer which is written to the flash
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR user_esp_platform_set_token(uint8_t *token) {
	if (token == NULL)
        return;

    os_memcpy(esp_param.token, token, os_strlen(token));
    system_param_save_with_protect(ESP_PARAM_START_SEC, &esp_param, sizeof(esp_param));
}


/******************************************************************************
 * FunctionName : user_esp_platform_set_connect_status
 * Description  : set device_status value
 * Parameters   : status
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR user_esp_platform_set_connect_status(uint8 status) {
	device_status = status;
}


/******************************************************************************
 * FunctionName : user_esp_platform_get_connect_status
 * Description  : get each connection step's status
 * Parameters   : none
 * Returns      : status
*******************************************************************************/
uint8 ICACHE_FLASH_ATTR user_esp_platform_get_connect_status(void) {
    uint8 status = wifi_station_get_connect_status();

    if (status == STATION_GOT_IP)
        status = (device_status == 0) ? DEVICE_CONNECTING : device_status;

    ESP_DBG("status %d\n", status);
    return status;
}


/******************************************************************************
 * FunctionName : user_esp_platform_parse_nonce
 * Description  : parse the device nonce (just a number)
 * Parameters   : bufferPtr -- the received data pointer
 * Returns      : the nonce
*******************************************************************************/
int ICACHE_FLASH_ATTR user_esp_platform_parse_nonce(char *bufferPtr) {
	uint32 nonce = 0;
	char *strPtr = NULL;

	strPtr = (char *)os_strstr(bufferPtr, "\"nonce\": ");

	if (strPtr != NULL) {
		strPtr += 9;
		if(isdigit(*strPtr)){
			nonce = atoi(strPtr);
		}
	}
    return nonce;
}


/******************************************************************************
 * FunctionName : user_esp_platform_emulate_steps
 * Description  : get steps_num and duration_sec from the request
 * 				: if values are acceptable, begin step emulation process
 * 				: send response to server
 * Parameters   : espconnPtr -- the espconn used to connect with host
 *                bufferPtr -- the processing data pointer
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR user_esp_platform_emulate_steps(struct espconn *espconnPtr, uint8 *bufferPtr) {
	char *strPtr = NULL;
	char *bufPtr = NULL;
	uint8 pwmCycleDecayStop = 0;
	uint32 stepsNum = 0;
	uint32 nonce = user_esp_platform_parse_nonce(bufferPtr);

	// Parse number of steps
	strPtr = (char *)os_strstr(bufferPtr, "\"steps_num\": ");
	if (strPtr != NULL) {
		strPtr += 14;
		if(isdigit(*strPtr)) {
			stepsNum = atoi(strPtr);
		}
	}

	// Parse edge of pwm cycle decay
	strPtr = (char *)os_strstr(bufferPtr, "\"pwm_cycle_decay_stop\": ");
	if (strPtr != NULL) {
		strPtr += 25;
		if(isdigit(*strPtr)) {
			pwmCycleDecayStop = atoi(strPtr);
		}
	}

	bufPtr = (char *)os_zalloc(PACKET_SIZE);

	// Set up response frame and emulate steps (if values are OK)
	if (bufPtr != NULL) {
		if (stepsNum == 0) {
			os_sprintf(bufPtr, ERROR_FRAME, 400, "\"invalid value of steps number\"", nonce);
		} else if (pwmCycleDecayStop == 0) {
			os_sprintf(bufPtr, ERROR_FRAME, 400, "\"invalid value of PWM cycle decay stop\"", nonce);
		} else if (user_get_remaining_macro_steps()) {
			os_sprintf(bufPtr, ERROR_VALUE_FRAME, 423, "\"locked, whizzer is busy - steps left: ", user_get_remaining_macro_steps(), nonce);
		} else if (stepsNum > MAX_STEPS) {
			os_sprintf(bufPtr, ERROR_VALUE_FRAME, 422, "\"steps threshold exceeded: greater than ", MAX_STEPS, nonce);
		} else if (stepsNum < MIN_STEPS) {
			os_sprintf(bufPtr, ERROR_VALUE_FRAME, 422, "\"short of steps value: less than ", MIN_STEPS, nonce);
		} else if (pwmCycleDecayStop > MAX_PWM_CYCLE_DECAY_STOP) {
			os_sprintf(bufPtr, ERROR_VALUE_FRAME, 422, "\"PWM cycle decay threshold exceeded: greater than ", MAX_PWM_CYCLE_DECAY_STOP, nonce);
		} else if (pwmCycleDecayStop < MIN_PWM_CYCLE_DECAY_STOP) {
			os_sprintf(bufPtr, ERROR_VALUE_FRAME, 422, "\"short of PWM cycle decay value: less than ", MIN_PWM_CYCLE_DECAY_STOP, nonce);
		} else {
			os_sprintf(bufPtr, RESPONSE_FRAME, stepsNum, pwmCycleDecayStop, nonce);
			user_emulate_macro_steps(stepsNum, pwmCycleDecayStop);
		}

		ESP_DBG("%s\n", bufPtr);
		espconn_send(espconnPtr, bufPtr, os_strlen(bufPtr));
		os_free(bufPtr);
		bufPtr = NULL;
	}
}


/******************************************************************************
 * FunctionName : user_esp_platform_reconnect
 * Description  : reconnect with host after get ip
 * Parameters   : espconnPtr -- the espconn used to reconnect with host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_esp_platform_reconnect(struct espconn *espconnPtr) {
	ESP_DBG("user_esp_platform_reconnect\n");
    user_esp_platform_check_ip(0);
}


/******************************************************************************
 * FunctionName : user_esp_platform_discon_cb
 * Description  : disconnect successfully with the host
 * Parameters   : arg -- additional argument to pass to the callback function
 * Returns      : nones
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_esp_platform_discon_cb(void *arg) {
    struct espconn *espconnPtr = arg;
    struct ip_info ipConfig;

    ESP_DBG("user_esp_platform_discon_cb\n");
    os_timer_disarm(&beacon_timer);

    if (espconnPtr == NULL)
        return;

    espconnPtr->proto.tcp->local_port = espconn_port();
    user_esp_platform_reconnect(espconnPtr);
}


/******************************************************************************
 * FunctionName : user_esp_platform_discon
 * Description  : A new incoming connection has been disconnected.
 * Parameters   : espconnPtr -- the espconn used to disconnect with host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_esp_platform_discon(struct espconn *espconnPtr) {
	ESP_DBG("user_esp_platform_discon\n");
    espconn_disconnect(espconnPtr);
}


/******************************************************************************
 * FunctionName : user_esp_platform_sent_cb
 * Description  : Data has been sent successfully and acknowledged by the remote host.
 * Parameters   : arg -- additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_esp_platform_sent_cb(void *arg) {
    ESP_DBG("user_esp_platform_sent_cb\n");
}


/******************************************************************************
 * FunctionName : user_esp_platform_send
 * Description  : Processing the application data and sending it to the host
 * Parameters   : espconnPtr -- the espconn used to connetion with the host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_esp_platform_send(struct espconn *espconnPtr) {

	uint32 nonce;
    char *bufPtr = (char *)os_zalloc(PACKET_SIZE);

    if (bufPtr != NULL) {
        if (ACTIVE_FLAG == 0){
        	uint8 token[TOKEN_SIZE] = {0};
        	uint8 bSSID[6];

        	active_nonce = os_random() & 0x7FFFFFFF;
        	//os_memcpy(token, esp_param.token, 40);
        	wifi_get_macaddr(STATION_IF, bSSID);

        	os_sprintf(bufPtr, ACTIVE_FRAME, active_nonce, TOKEN, MAC2STR(bSSID), iot_version, DEV_KEY);

        } else {
            nonce = os_random() & 0x7FFFFFFF;
            os_sprintf(bufPtr, FIRST_FRAME, nonce, DEV_KEY);
        }

        ESP_DBG("%s\n", bufPtr);
        espconn_send(espconnPtr, bufPtr, os_strlen(bufPtr));
        os_free(bufPtr);
    }
}


/******************************************************************************
 * FunctionName : user_esp_platform_sent_beacon
 * Description  : sent beacon frame for connection with the host is activate
 * Parameters   : espconnPtr -- the espconn used to connect with the host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_esp_platform_sent_beacon(struct espconn *espconnPtr) {

	user_set_cloud_led(0);

	if (espconnPtr == NULL) {
        return;
    }

    if (espconnPtr->state == ESPCONN_CONNECT) {
    	if (ACTIVE_FLAG == 0) {
            ESP_DBG("please check device is activated\n");

    	} else {
            ESP_DBG("user_esp_platform_sent_beacon %u\n", system_get_time());

            if (ping_status == 0) {
                ESP_DBG("user_esp_platform_sent_beacon sent fail!\n");
                user_esp_platform_discon(espconnPtr);

            } else {
                char *bufPtr = (char *)os_zalloc(PACKET_SIZE);
                if (bufPtr != NULL) {
                    os_sprintf(bufPtr, BEACON_FRAME, DEV_KEY);
                    espconn_send(espconnPtr, bufPtr, os_strlen(bufPtr));

                    ping_status = 0;
                    os_timer_arm(&beacon_timer, BEACON_TIME, 0);
                    os_free(bufPtr);
                }
            }
        }

    } else {
        ESP_DBG("user_esp_platform_sent_beacon sent fail!\n");
        user_esp_platform_discon(espconnPtr);
    }
}


/******************************************************************************
 * FunctionName : user_platform_rpc_set_rsp
 * Description  : response the message to server to show setting info is received
 * Parameters   : espconnPtr -- the espconn used to connect with the host
 *                nonce -- mark the message received from server
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_platform_rpc_set_rsp(struct espconn *espconnPtr, int nonce) {

	char *bufPtr = (char *)os_zalloc(PACKET_SIZE);

    if (espconnPtr == NULL) {
        return;
    }

    os_sprintf(bufPtr, RPC_RESPONSE_FRAME, nonce);
    ESP_DBG("%s\n", bufPtr);
    espconn_send(espconnPtr, bufPtr, os_strlen(bufPtr));
    os_free(bufPtr);
}


/******************************************************************************
 * FunctionName : user_esp_platform_upgrade_cb
 * Description  : Processing the download data from the server
 * Parameters   : espconnPtr -- the espconn used to connection with the host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_esp_platform_upgrade_rsp(void *arg) {

    struct upgrade_server_info *serverPtr = arg;
    struct espconn *espconnPtr = serverPtr->pespconn;

    char *action = NULL;
    char *bufPtr = (char *)os_zalloc(PACKET_SIZE);

    if (serverPtr->upgrade_flag == true) {
        ESP_DBG("user_esp_platform_upgarde_successfully\n");
        action = "device_upgrade_success";

        os_sprintf(bufPtr, UPGRADE_FRAME, DEV_KEY, action, serverPtr->pre_version, serverPtr->upgrade_version);
        ESP_DBG("%s\n",bufPtr);
        espconn_send(espconnPtr, bufPtr, os_strlen(bufPtr));
        os_free(bufPtr);

    } else {
        ESP_DBG("user_esp_platform_upgrade_failed\n");
        action = "device_upgrade_failed";

        os_sprintf(bufPtr, UPGRADE_FRAME, DEV_KEY, action, serverPtr->pre_version, serverPtr->upgrade_version);
        ESP_DBG("%s\n",bufPtr);
        espconn_send(espconnPtr, bufPtr, os_strlen(bufPtr));
        os_free(bufPtr);
    }

    os_free(serverPtr->url);
    serverPtr->url = NULL;
    os_free(serverPtr);
    serverPtr = NULL;
}


/******************************************************************************
 * FunctionName : user_esp_platform_upgrade_begin
 * Description  : Processing the received data from the server
 * Parameters   : espconnPtr -- the espconn used to connect with the host
 *                serverPtr -- upgrade parameters
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_esp_platform_upgrade_begin(struct espconn *espconnPtr, struct upgrade_server_info *serverPtr) {

	uint8 userBin[9] = {0};

    serverPtr->pespconn = espconnPtr;
    os_memcpy(serverPtr->ip, espconnPtr->proto.tcp->remote_ip, 4);
    serverPtr->port = 80;
    serverPtr->check_cb = user_esp_platform_upgrade_rsp;
    serverPtr->check_times = 120000;

    if (serverPtr->url == NULL) {
    	serverPtr->url = (uint8 *)os_zalloc(512);
    }

    if (system_upgrade_userbin_check() == UPGRADE_FW_BIN1) {
        os_memcpy(userBin, "user2.bin", 10);
    } else if (system_upgrade_userbin_check() == UPGRADE_FW_BIN2) {
        os_memcpy(userBin, "user1.bin", 10);
    }

    os_sprintf(serverPtr->url, "GET /v1/device/rom/?action=download_rom&version=%s&filename=%s HTTP/1.0\r\nHost: "IPSTR":%d\r\n"pheadbuffer"", serverPtr->upgrade_version, userBin, IP2STR(serverPtr->ip), serverPtr->port, DEV_KEY);
    ESP_DBG("%s\n", serverPtr->url);

    if (system_upgrade_start(serverPtr) == false) {
        ESP_DBG("upgrade has already started\n");
    }
}


/******************************************************************************
 * FunctionName : user_esp_platform_recv_cb
 * Description  : Processing the received data from the server
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                userDataPtr -- The received data (or NULL when the connection has been closed!)
 *                length -- The length of received data
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_esp_platform_recv_cb(void *arg, char *userDataPtr, unsigned short length) {

	char *strPtr = NULL;
    LOCAL char bufferPtr[PACKET_SIZE] = {0};
    struct espconn *espconnPtr = arg;

    ESP_DBG("user_esp_platform_recv_cb %s\n", userDataPtr);

    user_set_cloud_led(1);
    os_timer_disarm(&beacon_timer);

    if (length == 1460) {
        os_memcpy(bufferPtr, userDataPtr, length);

    } else {
        struct espconn *espconnPtr = (struct espconn *)arg;
        os_memcpy(bufferPtr + os_strlen(bufferPtr), userDataPtr, length);

        // Give a response to the host after firmware update
        if ((strPtr = (char *)os_strstr(bufferPtr, "\"action\": \"sys_upgrade\"")) != NULL) {
        	if ((strPtr = (char *)os_strstr(bufferPtr, "\"version\":")) != NULL) {

        		struct upgrade_server_info *serverPtr = NULL;
                int nonce = user_esp_platform_parse_nonce(bufferPtr);
                user_platform_rpc_set_rsp(espconnPtr, nonce);

                serverPtr = (struct upgrade_server_info *)os_zalloc(sizeof(struct upgrade_server_info));
                os_memcpy(serverPtr->upgrade_version, strPtr + 12, 16);
                serverPtr->upgrade_version[15] = '\0';
                os_sprintf(serverPtr->pre_version,"%s%d.%d.%dt%d(%s)",VERSION_TYPE, IOT_VERSION_MAJOR,\
                    	IOT_VERSION_MINOR, IOT_VERSION_REVISION, device_type, UPGRADE_FALG);
                user_esp_platform_upgrade_begin(espconnPtr, serverPtr);
            }

        // Reboot device after upgrade
        } else if ((strPtr = (char *)os_strstr(bufferPtr, "\"action\": \"sys_reboot\"")) != NULL) {
            os_timer_disarm(&client_timer);
            os_timer_setfn(&client_timer, (os_timer_func_t *)system_upgrade_reboot, NULL);
            os_timer_arm(&client_timer, 1000, 0);

        // Start steps emulation process
        } else if ((strPtr = (char *)os_strstr(bufferPtr, "\"action\": \"emulate_steps\"")) != NULL) {
        	user_esp_platform_emulate_steps(espconnPtr, bufferPtr);

        // Change ping_status after successful ping
        } else if ((strPtr = (char *)os_strstr(bufferPtr, "ping success")) != NULL) {
            ESP_DBG("ping success\n");
            ping_status = 1;
        }

        // Memset() is used because of bufferPtr is an array, not dynamic memory
        // Seems it doesn't update when incoming call length == 1460
        os_memset(bufferPtr, 0, sizeof(bufferPtr));
    }

    os_timer_arm(&beacon_timer, BEACON_TIME, 0);
}


/******************************************************************************
 * FunctionName : user_esp_platform_ap_change
 * Description  : add the user interface for changing to next Access Point
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_esp_platform_ap_change(void) {

    uint8 current_id;
    uint8 i = 0;

    ESP_DBG("user_esp_platform_ap_is_changing\n");

    current_id = wifi_station_get_current_ap_id();
    ESP_DBG("current ap id =%d\n", current_id);

    // Next AP ID
    (current_id == AP_CACHE_NUMBER - 1) ? (i = 0) : (i = current_id + 1);

    // Try to connect to APs one by one
    while (wifi_station_ap_change(i) != true) {
       i++;
       if (i == AP_CACHE_NUMBER - 1) {
    	   i = 0;
       }
    }

    // Just need to re-check ip while change AP
    device_recon_count = 0;
    os_timer_disarm(&client_timer);
    os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_check_ip, NULL);
    os_timer_arm(&client_timer, 100, 0);
}


/******************************************************************************
 * FunctionName : user_esp_platform_reset_mode
 * Description  : change device mode to STATION + AP and change AP in 5 sec
 * Parameters   : none
 * Returns      : none
*******************************************************************************/

LOCAL bool ICACHE_FLASH_ATTR user_esp_platform_reset_mode(void) {

	if (wifi_get_opmode() == STATION_MODE) {
        wifi_set_opmode(STATIONAP_MODE);
    }

    // Delay 5s to change AP
    os_timer_disarm(&client_timer);
    os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_ap_change, NULL);
    os_timer_arm(&client_timer, 5000, 0);

    return true;
}


/******************************************************************************
 * FunctionName : user_esp_platform_recon_cb
 * Description  : the connection had an error and is already deallocated
 * Parameters   : arg -- additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_esp_platform_recon_cb(void *arg, sint8 err) {

    struct espconn *espconnPtr = (struct espconn *)arg;

    ESP_DBG("user_esp_platform_recon_cb\n");
    os_timer_disarm(&beacon_timer);

    // After 5 tries device stop trying to reconnect and reset its WiFi mode
    if (++device_recon_count == 5) {
        device_status = DEVICE_CONNECT_SERVER_FAIL;
        if (user_esp_platform_reset_mode()) {
            return;
        }
    }

    // On tries 1-4 device try to get ip and reconnect with the host
    os_timer_disarm(&client_timer);
    os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_reconnect, espconnPtr);
    os_timer_arm(&client_timer, 1000, 0);
}


/******************************************************************************
 * FunctionName : user_esp_platform_connect_cb
 * Description  : a new incoming connection has been connected
 * Parameters   : arg -- additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_esp_platform_connect_cb(void *arg) {

    struct espconn *espconnPtr = arg;

    ESP_DBG("user_esp_platform_connect_cb\n");

    if (wifi_get_opmode() ==  STATIONAP_MODE ) {
        wifi_set_opmode(STATION_MODE);
    }

    device_recon_count = 0;
    espconn_regist_recvcb(espconnPtr, user_esp_platform_recv_cb);
    espconn_regist_sentcb(espconnPtr, user_esp_platform_sent_cb);
    user_esp_platform_send(espconnPtr);
}


/******************************************************************************
 * FunctionName : user_esp_platform_connect
 * Description  : The function given as the connect with the host
 * Parameters   : espconnPtr -- the espconn used to connect the connection
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_esp_platform_connect(struct espconn *espconnPtr) {
    ESP_DBG("user_esp_platform_connect\n");
    espconn_connect(espconnPtr);
}


/******************************************************************************
 * FunctionName : user_esp_platform_dns_found
 * Description  : dns found callback
 * Parameters   : name -- pointer to the name that was looked up
 *                ipAddr -- pointer to an ip_addr_t containing the IP address of
 *                the hostname, or NULL if the name could not be found (or on any
 *                other error)
 *                callback_arg -- a user-specified callback argument passed to
 *                dns_gethostbyname
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_esp_platform_dns_found(const char *name, ip_addr_t *ipAddr, void *arg) {

    struct espconn *espconnPtr = (struct espconn *)arg;

    if (ipAddr == NULL) {
        ESP_DBG("user_esp_platform_dns_found NULL\n");

        if (++device_recon_count == 5) {
            device_status = DEVICE_CONNECT_SERVER_FAIL;
            user_esp_platform_reset_mode();
        }

        return;
    }

    ESP_DBG("user_esp_platform_dns_found %d.%d.%d.%d\n",
            *((uint8 *)&ipAddr->addr), *((uint8 *)&ipAddr->addr + 1),
            *((uint8 *)&ipAddr->addr + 2), *((uint8 *)&ipAddr->addr + 3));

    if (esp_server_ip.addr == 0 && ipAddr->addr != 0) {

    	// Register IP address
    	os_timer_disarm(&client_timer);
        esp_server_ip.addr = ipAddr->addr;
        os_memcpy(espconnPtr->proto.tcp->remote_ip, &ipAddr->addr, 4);

        // Register ports
        espconnPtr->proto.tcp->local_port = espconn_port();
        espconnPtr->proto.tcp->remote_port = 8000;

        ping_status = 1;

        // Register callback functions to connect, disconnect, connection fails
        espconn_regist_connectcb(espconnPtr, user_esp_platform_connect_cb);
        espconn_regist_disconcb(espconnPtr, user_esp_platform_discon_cb);
        espconn_regist_reconcb(espconnPtr, user_esp_platform_recon_cb);
        user_esp_platform_connect(espconnPtr);
    }
}


/******************************************************************************
 * FunctionName : user_esp_platform_dns_check_cb
 * Description  : first time callback to check DNS found
 * Parameters   : arg -- additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_esp_platform_dns_check_cb(void *arg) {

    struct espconn *espconnPtr = arg;

    ESP_DBG("user_esp_platform_dns_check_cb\n");

    espconn_gethostbyname(espconnPtr, ESP_DOMAIN, &esp_server_ip, user_esp_platform_dns_found);
    os_timer_arm(&client_timer, 1000, 0);
}


/******************************************************************************
 * FunctionName : user_esp_platform_start_dns
 * Description  : start DNS connection process
 * Parameters   : espconnPtr -- the espconn used to connect the connection
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_esp_platform_start_dns(struct espconn *espconnPtr) {

    esp_server_ip.addr = 0;
    espconn_gethostbyname(espconnPtr, ESP_DOMAIN, &esp_server_ip, user_esp_platform_dns_found);

    os_timer_disarm(&client_timer);
    os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_dns_check_cb, espconnPtr);
    os_timer_arm(&client_timer, 1000, 0);
}


/******************************************************************************
 * FunctionName : user_esp_platform_check_ip
 * Description  : initialize espconn struct parameters when get ip address
 * Parameters   : reset_flag -- reset reconnect counter to 0
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR user_esp_platform_check_ip(uint8 reset_flag) {

    struct ip_info ipConfig;

    os_timer_disarm(&client_timer);
    wifi_get_ip_info(STATION_IF, &ipConfig);

    // If device has already got ip -- set up espconn and start connecting to the host
    if (wifi_station_get_connect_status() == STATION_GOT_IP && ipConfig.ip.addr != 0) {

        user_conn.proto.tcp = &user_tcp;
        user_conn.type = ESPCONN_TCP;
        user_conn.state = ESPCONN_NONE;

        device_status = DEVICE_CONNECTING;
        user_set_dns_led(1);

        if (reset_flag) {
            device_recon_count = 0;
        }

        os_timer_disarm(&beacon_timer);
        os_timer_setfn(&beacon_timer, (os_timer_func_t *)user_esp_platform_sent_beacon, &user_conn);
        user_esp_platform_start_dns(&user_conn);

    } else {
    	user_set_dns_led(0);

    	// If there are wrong while connecting to some AP, then reset mode
        if ((wifi_station_get_connect_status() == STATION_WRONG_PASSWORD ||
                wifi_station_get_connect_status() == STATION_NO_AP_FOUND ||
                wifi_station_get_connect_status() == STATION_CONNECT_FAIL)) {
            user_esp_platform_reset_mode();

        // Otherwise run user_esp_platform_check_ip again
        } else {
            os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_check_ip, NULL);
            os_timer_arm(&client_timer, 100, 0);
        }
    }
}


/******************************************************************************
 * FunctionName : user_esp_platform_init
 * Description  : device parameter init based on espressif platform
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR user_esp_platform_init(void) {

	os_sprintf(iot_version,"%s%d.%d.%dt%d(%s)", VERSION_TYPE, IOT_VERSION_MAJOR,\
			IOT_VERSION_MINOR, IOT_VERSION_REVISION, device_type, UPGRADE_FALG);
	os_printf("IOT VERSION = %s\n", iot_version);

	system_param_load(ESP_PARAM_START_SEC, 0, &esp_param, sizeof(esp_param));

	struct rst_info *resetInfo = system_get_rst_info();
	os_printf("reset reason: %x\n", resetInfo->reason);

	if (resetInfo->reason == REASON_WDT_RST ||
			resetInfo->reason == REASON_EXCEPTION_RST ||
			resetInfo->reason == REASON_SOFT_WDT_RST) {
		if (resetInfo->reason == REASON_EXCEPTION_RST) {
			os_printf("Fatal exception (%d):\n", resetInfo->exccause);
		}
		os_printf("epc1=0x%08x, epc2=0x%08x, epc3=0x%08x, excvaddr=0x%08x, depc=0x%08x\n",
				resetInfo->epc1, resetInfo->epc2, resetInfo->epc3, resetInfo->excvaddr, resetInfo->depc);
	}

	/***add by tzx for saving ip_info to avoid dhcp_client start****/
    struct dhcp_client_info dhcpInfo;
    struct ip_info staInfo;

    system_rtc_mem_read(64, &dhcpInfo, sizeof(struct dhcp_client_info));

    if(dhcpInfo.flag == 0x01 ) {
		if (true == wifi_station_dhcpc_status()) {
			wifi_station_dhcpc_stop();
		}

		staInfo.ip = dhcpInfo.ip_addr;
		staInfo.gw = dhcpInfo.gw;
		staInfo.netmask = dhcpInfo.netmask;
		if ( true != wifi_set_ip_info(STATION_IF, &staInfo)) {
			os_printf("set default ip wrong\n");
		}
	}

    os_memset(&dhcpInfo, 0, sizeof(struct dhcp_client_info));
    system_rtc_mem_write(64, &dhcpInfo, sizeof(struct rst_info));

    wifi_station_ap_number_set(AP_CACHE_NUMBER);

    if (ACTIVE_FLAG != 1) {
        wifi_set_opmode(STATIONAP_MODE);
    }

    if (wifi_get_opmode() != SOFTAP_MODE) {
        os_timer_disarm(&client_timer);
        os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_check_ip, 1);
        os_timer_arm(&client_timer, 100, 0);
    }
}
