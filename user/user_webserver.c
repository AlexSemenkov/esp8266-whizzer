/*
 *  user_webserver.c
 *
 *  Created on: 10 dec 2016
 *      Author: Espressif Systems (Shanghai) Pte., Ltd.
 */

#include "user_interface.h"
#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "mem.h"
#include "espconn.h"

#include "user_config.h"
#include "user_json.h"
#include "user_iot_version.h"
#include "user_webserver.h"
#include "user_esp_platform.h"

extern u16 scannum;

LOCAL struct station_config *sta_conf;
LOCAL struct softap_config *ap_conf;

LOCAL scaninfo *scan_info;
struct bss_info *bss;
struct bss_info *bss_temp;
struct bss_info *bss_head;

// buffers for save_data() and check_data() functions
LOCAL char *recv_buffer;
static uint32 data_sum_length = 0;

// system restart parameters
LOCAL os_timer_t *restart_10ms;
LOCAL rst_parm *rstparm;

// upgrade parameters
uint8 upgrade_lock = 0;
LOCAL os_timer_t app_upgrade_10s;
LOCAL os_timer_t upgrade_check_timer;

/******************************************************************************
 * FunctionName : device_get
 * Description  : set up the device information parameter as a JSON format
 * Parameters   : jsonContext -- a pointer to a JSON set up
 * Returns      : result
 *******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR device_get(struct jsontree_context *jsonContext) {

	const char *path = jsontree_path_name(jsonContext, jsonContext->depth - 1);

	if (os_strncmp(path, "manufacture", 11) == 0) {
		jsontree_write_string(jsonContext, MANUFACTURER);
	} else if (os_strncmp(path, "product", 7) == 0) {
		jsontree_write_string(jsonContext, PRODUCT_NAME);
	}

	return 0;
}

LOCAL struct jsontree_callback device_callback = JSONTREE_CALLBACK(device_get, NULL);

/******************************************************************************
 * FunctionName : userbin_get
 * Description  : get up the user bin parameter as a JSON format
 * Parameters   : jsonContext -- a pointer to a JSON set up
 * Returns      : result
 ******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR userbin_get(struct jsontree_context *jsonContext) {

	const char *path = jsontree_path_name(jsonContext, jsonContext->depth - 1);
	char string[32];

	if (os_strncmp(path, "status", 8) == 0) {
		os_sprintf(string, "200");

	} else if (os_strncmp(path, "user_bin", 8) == 0) {
		if (system_upgrade_userbin_check() == 0x00) {
			os_sprintf(string, "user1.bin");
		} else if (system_upgrade_userbin_check() == 0x01) {
			os_sprintf(string, "user2.bin");
		} else {
			return 0;
		}
	}

	jsontree_write_string(jsonContext, string);
	return 0;
}

LOCAL struct jsontree_callback userbin_callback = JSONTREE_CALLBACK(userbin_get, NULL);

JSONTREE_OBJECT(userbin_tree, JSONTREE_PAIR("status", &userbin_callback), JSONTREE_PAIR("user_bin", &userbin_callback));

JSONTREE_OBJECT(userinfo_tree, JSONTREE_PAIR("user_info",&userbin_tree));

/******************************************************************************
 * FunctionName : version_get
 * Description  : set up the device version parameter as a JSON format
 * Parameters   : jsonContext -- a pointer to a JSON set up
 * Returns      : result
 *******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR version_get(struct jsontree_context *jsonContext) {

	const char *path = jsontree_path_name(jsonContext, jsonContext->depth - 1);
	char string[32];

	if (os_strncmp(path, "hardware", 8) == 0) {
		os_sprintf(string, WHIZZER_VERSION);

	} else if (os_strncmp(path, "sdk_version", 11) == 0) {
		os_sprintf(string, "%s", system_get_sdk_version());

	} else if (os_strncmp(path, "iot_version", 11) == 0) {
		os_sprintf(string, "%s%d.%d.%dt%d(%s)", VERSION_TYPE, IOT_VERSION_MAJOR,
		IOT_VERSION_MINOR, IOT_VERSION_REVISION, device_type, UPGRADE_FALG);
	}

	jsontree_write_string(jsonContext, string);
	return 0;
}

LOCAL struct jsontree_callback version_callback = JSONTREE_CALLBACK(version_get, NULL);

JSONTREE_OBJECT(device_tree, JSONTREE_PAIR("product", &device_callback),
		JSONTREE_PAIR("manufacturer", &device_callback));

JSONTREE_OBJECT(version_tree, JSONTREE_PAIR("hardware", &version_callback),
		JSONTREE_PAIR("sdk_version", &version_callback), JSONTREE_PAIR("iot_version", &version_callback));

JSONTREE_OBJECT(info_tree, JSONTREE_PAIR("Version", &version_tree), JSONTREE_PAIR("Device", &device_tree));

JSONTREE_OBJECT(INFOTree, JSONTREE_PAIR("info", &info_tree));

/******************************************************************************
 * FunctionName : connect_status_get
 * Description  : set up the device espconn status parameter as a JSON format
 * Parameters   : jsonContext -- a pointer to a JSON set up
 * Returns      : result
 *******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR connect_status_get(struct jsontree_context *jsonContext) {

	const char *path = jsontree_path_name(jsonContext, jsonContext->depth - 1);

	if (os_strncmp(path, "status", 8) == 0) {
		jsontree_write_int(jsonContext, user_esp_platform_get_connect_status());
	}

	return 0;
}

LOCAL struct jsontree_callback connect_status_callback = JSONTREE_CALLBACK(connect_status_get, NULL);

JSONTREE_OBJECT(status_sub_tree, JSONTREE_PAIR("status", &connect_status_callback));

JSONTREE_OBJECT(connect_status_tree, JSONTREE_PAIR("Status", &status_sub_tree));

JSONTREE_OBJECT(con_status_tree, JSONTREE_PAIR("info", &connect_status_tree));

/******************************************************************************
 * FunctionName : wifi_station_get
 * Description  : set up the station parameter as a JSON format
 * Parameters   : jsonContext -- a pointer to a JSON set up
 * Returns      : result
 *******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR wifi_station_get(struct jsontree_context *jsonContext) {

	const char *path = jsontree_path_name(jsonContext, jsonContext->depth - 1);

	struct ip_info ipConfig;
	uint8 buf[20];
	os_memset(buf, 0, sizeof(buf));

	wifi_station_get_config(sta_conf);
	wifi_get_ip_info(STATION_IF, &ipConfig);

	if (os_strncmp(path, "ssid", 4) == 0) {
		jsontree_write_string(jsonContext, sta_conf->ssid);

	} else if (os_strncmp(path, "password", 8) == 0) {
		jsontree_write_string(jsonContext, sta_conf->password);

	} else if (os_strncmp(path, "ip", 2) == 0) {
		os_sprintf(buf, IPSTR, IP2STR(&ipConfig.ip));
		jsontree_write_string(jsonContext, buf);

	} else if (os_strncmp(path, "mask", 4) == 0) {
		os_sprintf(buf, IPSTR, IP2STR(&ipConfig.netmask));
		jsontree_write_string(jsonContext, buf);

	} else if (os_strncmp(path, "gw", 2) == 0) {
		os_sprintf(buf, IPSTR, IP2STR(&ipConfig.gw));
		jsontree_write_string(jsonContext, buf);
	}

	return 0;
}

/******************************************************************************
 * FunctionName : wifi_station_set
 * Description  : parse the station parameter as a JSON format
 * Parameters   : jsonContext -- a pointer to a JSON set up
 *                parser -- a pointer to a JSON parser state
 * Returns      : result
 *******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR wifi_station_set(struct jsontree_context *jsonContext, struct jsonparse_state *parser) {

	int type;
	uint8 station_tree;

	while ((type = jsonparse_next(parser)) != 0) {

		if (type == JSON_TYPE_PAIR_NAME) {
			char buffer[64];
			os_memset(buffer, 0, sizeof(buffer));

			if (jsonparse_strcmp_value(parser, "Station") == 0) {
				station_tree = 1;
			} else if (jsonparse_strcmp_value(parser, "Softap") == 0) {
				station_tree = 0;
			}

			if (station_tree) {

				if (jsonparse_strcmp_value(parser, "ssid") == 0) {
					jsonparse_next(parser);
					jsonparse_next(parser);
					jsonparse_copy_value(parser, buffer, sizeof(buffer));
					os_memcpy(sta_conf->ssid, buffer, os_strlen(buffer));

				} else if (jsonparse_strcmp_value(parser, "password") == 0) {
					jsonparse_next(parser);
					jsonparse_next(parser);
					jsonparse_copy_value(parser, buffer, sizeof(buffer));
					os_memcpy(sta_conf->password, buffer, os_strlen(buffer));

				} else if (jsonparse_strcmp_value(parser, "token") == 0) {
					jsonparse_next(parser);
					jsonparse_next(parser);
					jsonparse_copy_value(parser, buffer, sizeof(buffer));
					user_esp_platform_set_token(buffer);
				}
			}
		}
	}

	return 0;
}

LOCAL struct jsontree_callback wifi_station_callback = JSONTREE_CALLBACK(wifi_station_get, wifi_station_set);

JSONTREE_OBJECT(get_station_config_tree, JSONTREE_PAIR("ssid", &wifi_station_callback),
		JSONTREE_PAIR("password", &wifi_station_callback));

JSONTREE_OBJECT(set_station_config_tree, JSONTREE_PAIR("ssid", &wifi_station_callback),
		JSONTREE_PAIR("password", &wifi_station_callback), JSONTREE_PAIR("token", &wifi_station_callback));

JSONTREE_OBJECT(ip_tree, JSONTREE_PAIR("ip", &wifi_station_callback), JSONTREE_PAIR("mask", &wifi_station_callback),
		JSONTREE_PAIR("gw", &wifi_station_callback));

JSONTREE_OBJECT(get_station_tree, JSONTREE_PAIR("Connect_Station", &get_station_config_tree),
		JSONTREE_PAIR("Ipinfo_Station", &ip_tree));

JSONTREE_OBJECT(set_station_tree, JSONTREE_PAIR("Connect_Station", &set_station_config_tree));

/******************************************************************************
 * FunctionName : wifi_softap_get
 * Description  : set up the soft-AP parameter as a JSON format
 * Parameters   : jsonContext -- a pointer to a JSON set up
 * Returns      : result
 *******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR wifi_softap_get(struct jsontree_context *jsonContext) {

	const char *path = jsontree_path_name(jsonContext, jsonContext->depth - 1);

	struct ip_info ipConfig;
	uint8 buf[20];
	os_memset(buf, 0, sizeof(buf));

	wifi_softap_get_config(ap_conf);
	wifi_get_ip_info(SOFTAP_IF, &ipConfig);

	if (os_strncmp(path, "ssid", 4) == 0) {
		jsontree_write_string(jsonContext, ap_conf->ssid);

	} else if (os_strncmp(path, "password", 8) == 0) {
		jsontree_write_string(jsonContext, ap_conf->password);

	} else if (os_strncmp(path, "channel", 7) == 0) {
		jsontree_write_int(jsonContext, ap_conf->channel);

	} else if (os_strncmp(path, "authmode", 8) == 0) {

		switch (ap_conf->authmode) {

		case AUTH_OPEN:
			jsontree_write_string(jsonContext, "OPEN");
			break;

		case AUTH_WEP:
			jsontree_write_string(jsonContext, "WEP");
			break;

		case AUTH_WPA_PSK:
			jsontree_write_string(jsonContext, "WPAPSK");
			break;

		case AUTH_WPA2_PSK:
			jsontree_write_string(jsonContext, "WPA2PSK");
			break;

		case AUTH_WPA_WPA2_PSK:
			jsontree_write_string(jsonContext, "WPAPSK/WPA2PSK");
			break;

		default:
			jsontree_write_int(jsonContext, ap_conf->authmode);
			break;
		}

	} else if (os_strncmp(path, "ip", 2) == 0) {
		os_sprintf(buf, IPSTR, IP2STR(&ipConfig.ip));
		jsontree_write_string(jsonContext, buf);

	} else if (os_strncmp(path, "mask", 4) == 0) {
		os_sprintf(buf, IPSTR, IP2STR(&ipConfig.netmask));
		jsontree_write_string(jsonContext, buf);

	} else if (os_strncmp(path, "gw", 2) == 0) {
		os_sprintf(buf, IPSTR, IP2STR(&ipConfig.gw));
		jsontree_write_string(jsonContext, buf);
	}

	return 0;
}

/******************************************************************************
 * FunctionName : wifi_softap_set
 * Description  : parse the soft-AP parameter as a JSON format
 * Parameters   : jsonContext -- a pointer to a JSON set up
 *                parser -- a pointer to a JSON parser state
 * Returns      : result
 *******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR wifi_softap_set(struct jsontree_context *jsonContext, struct jsonparse_state *parser) {

	int type;
	uint8 softap_tree;

	while ((type = jsonparse_next(parser)) != 0) {

		if (type == JSON_TYPE_PAIR_NAME) {
			char buffer[64];
			os_memset(buffer, 0, sizeof(buffer));

			if (jsonparse_strcmp_value(parser, "Station") == 0) {
				softap_tree = 0;
			} else if (jsonparse_strcmp_value(parser, "Softap") == 0) {
				softap_tree = 1;
			}

			if (softap_tree) {

				if (jsonparse_strcmp_value(parser, "authmode") == 0) {
					jsonparse_next(parser);
					jsonparse_next(parser);
					jsonparse_copy_value(parser, buffer, sizeof(buffer));

					// other modes are not supported for now
					if (os_strcmp(buffer, "OPEN") == 0) {
						ap_conf->authmode = AUTH_OPEN;

					} else if (os_strcmp(buffer, "WPAPSK") == 0) {
						ap_conf->authmode = AUTH_WPA_PSK;
						os_printf("%d %s\n", ap_conf->authmode, buffer);

					} else if (os_strcmp(buffer, "WPA2PSK") == 0) {
						ap_conf->authmode = AUTH_WPA2_PSK;

					} else if (os_strcmp(buffer, "WPAPSK/WPA2PSK") == 0) {
						ap_conf->authmode = AUTH_WPA_WPA2_PSK;

					} else {
						ap_conf->authmode = AUTH_OPEN;
						return 0;
					}
				}

				if (jsonparse_strcmp_value(parser, "channel") == 0) {
					jsonparse_next(parser);
					jsonparse_next(parser);
					ap_conf->channel = jsonparse_get_value_as_int(parser);

				} else if (jsonparse_strcmp_value(parser, "ssid") == 0) {
					jsonparse_next(parser);
					jsonparse_next(parser);
					jsonparse_copy_value(parser, buffer, sizeof(buffer));
					os_memcpy(ap_conf->ssid, buffer, os_strlen(buffer));

				} else if (jsonparse_strcmp_value(parser, "password") == 0) {
					jsonparse_next(parser);
					jsonparse_next(parser);
					jsonparse_copy_value(parser, buffer, sizeof(buffer));
					os_memcpy(ap_conf->password, buffer, os_strlen(buffer));
				}
			}
		}
	}

	return 0;
}

LOCAL struct jsontree_callback wifi_softap_callback = JSONTREE_CALLBACK(wifi_softap_get, wifi_softap_set);

JSONTREE_OBJECT(softap_config_tree, JSONTREE_PAIR("authmode", &wifi_softap_callback),
		JSONTREE_PAIR("channel", &wifi_softap_callback), JSONTREE_PAIR("ssid", &wifi_softap_callback),
		JSONTREE_PAIR("password", &wifi_softap_callback));

JSONTREE_OBJECT(softap_ip_tree, JSONTREE_PAIR("ip", &wifi_softap_callback),
		JSONTREE_PAIR("mask", &wifi_softap_callback), JSONTREE_PAIR("gw", &wifi_softap_callback));

JSONTREE_OBJECT(get_softap_tree, JSONTREE_PAIR("Connect_Softap", &softap_config_tree),
		JSONTREE_PAIR("Ipinfo_Softap", &softap_ip_tree));

JSONTREE_OBJECT(set_softap_tree, JSONTREE_PAIR("Ipinfo_Softap", &softap_config_tree));

JSONTREE_OBJECT(get_wifi_tree, JSONTREE_PAIR("Station", &get_station_tree), JSONTREE_PAIR("Softap", &get_softap_tree));

JSONTREE_OBJECT(set_wifi_tree, JSONTREE_PAIR("Station", &set_station_tree), JSONTREE_PAIR("Softap", &set_softap_tree));

JSONTREE_OBJECT(wifi_response_tree, JSONTREE_PAIR("Response", &get_wifi_tree));

JSONTREE_OBJECT(wifi_request_tree, JSONTREE_PAIR("Request", &set_wifi_tree));

JSONTREE_OBJECT(wifi_info_tree, JSONTREE_PAIR("wifi", &wifi_response_tree));

JSONTREE_OBJECT(wifi_req_tree, JSONTREE_PAIR("wifi", &wifi_request_tree));

/******************************************************************************
 * FunctionName : scan_get
 * Description  : set up the scan data as a JSON format
 * Parameters   : jsonContext -- a pointer to a JSON set up
 * Returns      : result
 *******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR scan_get(struct jsontree_context *jsonContext) {

	const char *path = jsontree_path_name(jsonContext, jsonContext->depth - 1);

	if (os_strncmp(path, "TotalPage", 9) == 0) {
		jsontree_write_int(jsonContext, scan_info->totalpage);

	} else if (os_strncmp(path, "PageNum", 7) == 0) {
		jsontree_write_int(jsonContext, scan_info->pagenum);

	} else if (os_strncmp(path, "bssid", 5) == 0) {

		if (bss == NULL) {
			bss = bss_head;
		}

		u8 buffer[32];
		os_memset(buffer, 0, sizeof(buffer));
		os_sprintf(buffer, MACSTR, MAC2STR(bss->bssid));
		jsontree_write_string(jsonContext, buffer);

	} else if (os_strncmp(path, "ssid", 4) == 0) {
		jsontree_write_string(jsonContext, bss->ssid);

	} else if (os_strncmp(path, "rssi", 4) == 0) {
		jsontree_write_int(jsonContext, -(bss->rssi));

	} else if (os_strncmp(path, "channel", 7) == 0) {
		jsontree_write_int(jsonContext, bss->channel);

	} else if (os_strncmp(path, "authmode", 8) == 0) {

		switch (bss->authmode) {

		case AUTH_OPEN:
			jsontree_write_string(jsonContext, "OPEN");
			break;

		case AUTH_WEP:
			jsontree_write_string(jsonContext, "WEP");
			break;

		case AUTH_WPA_PSK:
			jsontree_write_string(jsonContext, "WPAPSK");
			break;

		case AUTH_WPA2_PSK:
			jsontree_write_string(jsonContext, "WPA2PSK");
			break;

		case AUTH_WPA_WPA2_PSK:
			jsontree_write_string(jsonContext, "WPAPSK/WPA2PSK");
			break;

		default:
			jsontree_write_int(jsonContext, bss->authmode);
			break;
		}

		bss = STAILQ_NEXT(bss, next);
	}

	return 0;
}

LOCAL struct jsontree_callback scan_callback = JSONTREE_CALLBACK(scan_get, NULL);

JSONTREE_OBJECT(scaninfo_tree, JSONTREE_PAIR("bssid", &scan_callback), JSONTREE_PAIR("ssid", &scan_callback),
		JSONTREE_PAIR("rssi", &scan_callback), JSONTREE_PAIR("channel", &scan_callback),
		JSONTREE_PAIR("authmode", &scan_callback));

JSONTREE_ARRAY(scanrslt_tree, JSONTREE_PAIR_ARRAY(&scaninfo_tree), JSONTREE_PAIR_ARRAY(&scaninfo_tree),
		JSONTREE_PAIR_ARRAY(&scaninfo_tree), JSONTREE_PAIR_ARRAY(&scaninfo_tree), JSONTREE_PAIR_ARRAY(&scaninfo_tree),
		JSONTREE_PAIR_ARRAY(&scaninfo_tree), JSONTREE_PAIR_ARRAY(&scaninfo_tree), JSONTREE_PAIR_ARRAY(&scaninfo_tree));

JSONTREE_OBJECT(scantree, JSONTREE_PAIR("TotalPage", &scan_callback), JSONTREE_PAIR("PageNum", &scan_callback),
		JSONTREE_PAIR("ScanResult", &scanrslt_tree));

JSONTREE_OBJECT(scanres_tree, JSONTREE_PAIR("Response", &scantree));

JSONTREE_OBJECT(scan_tree, JSONTREE_PAIR("scan", &scanres_tree));

/******************************************************************************
 * FunctionName : parse_url
 * Description  : parse the received data from the server
 * Parameters   : recvPtr -- the received data pointer
 *                urlFramePtr -- the result of parsing the url
 * Returns      : none
 *******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR parse_url(char *recvPtr, URL_Frame *urlFramePtr) {

	char *str = NULL;
	char *buffer = NULL;
	char *buf = NULL;
	uint8 length = 0;

	if (urlFramePtr == NULL || recvPtr == NULL) {
		return;
	}

	buffer = (char *) os_strstr(recvPtr, "Host:");

	if (buffer == NULL) {
		return;

	} else {
		length = buffer - recvPtr;
		buf = (char *) os_zalloc(length + 1);
		buffer = buf;
		os_memcpy(buffer, recvPtr, length);

		os_memset(urlFramePtr->pSelect, 0, URL_SIZE);
		os_memset(urlFramePtr->pCommand, 0, URL_SIZE);
		os_memset(urlFramePtr->pFileName, 0, URL_SIZE);

		if (os_strncmp(buffer, "GET ", 4) == 0) {
			urlFramePtr->Type = GET;
			buffer += 4;

		} else if (os_strncmp(buffer, "POST ", 5) == 0) {
			urlFramePtr->Type = POST;
			buffer += 5;
		}

		buffer++;
		str = (char *) os_strstr(buffer, "?");

		if (str != NULL) {
			length = str - buffer;
			os_memcpy(urlFramePtr->pSelect, buffer, length);
			str++;
			buffer = (char *) os_strstr(str, "=");

			if (buffer != NULL) {
				length = buffer - str;
				os_memcpy(urlFramePtr->pCommand, str, length);
				buffer++;
				str = (char *) os_strstr(buffer, "&");

				if (str != NULL) {
					length = str - buffer;
					os_memcpy(urlFramePtr->pFileName, buffer, length);

				} else {
					str = (char *) os_strstr(buffer, " HTTP");

					if (str != NULL) {
						length = str - buffer;
						os_memcpy(urlFramePtr->pFileName, buffer, length);
					}
				}
			}
		}

		os_free(buf);
	}
}

/******************************************************************************
 * FunctionName : save_data
 * Description  : save data into recv_buffer
 * Parameters   : recvPtr -- the received data pointer
 * 				  length -- data length
 * Returns      : bool is data saved or not
 *******************************************************************************/
LOCAL bool ICACHE_FLASH_ATTR save_data(char *recvPtr, uint16 length) {

	char length_buf[10] = { 0 };
	char *temp = NULL;
	char *data = NULL;
	uint16 headLength = 0;
	static uint32 totalLength = 0;

	temp = (char *) os_strstr(recvPtr, "\r\n\r\n");

	if (temp != NULL) {
		length -= temp - recvPtr;
		length -= 4;
		totalLength += length;
		headLength = temp - recvPtr + 4;
		data = (char *) os_strstr(recvPtr, "Content-Length: ");

		if (data != NULL) {
			data += 16;
			recv_buffer = (char *) os_strstr(data, "\r\n");

			if (recv_buffer != NULL) {
				os_memcpy(length_buf, data, recv_buffer - data);
				data_sum_length = atoi(length_buf);
			}

		} else if (totalLength != 0x00) {
			totalLength = 0;
			data_sum_length = 0;
			return false;
		}

		if ((data_sum_length + headLength) >= 1024) {
			recv_buffer = (char *) os_zalloc(headLength + 1);
			os_memcpy(recv_buffer, recvPtr, headLength + 1);

		} else {
			recv_buffer = (char *) os_zalloc(data_sum_length + headLength + 1);
			os_memcpy(recv_buffer, recvPtr, os_strlen(recvPtr));
		}

	} else {
		if (recv_buffer != NULL) {
			totalLength += length;
			os_memcpy(recv_buffer + os_strlen(recv_buffer), recvPtr, length);

		} else {
			totalLength = 0;
			data_sum_length = 0;
			return false;
		}
	}

	if (totalLength == data_sum_length) {
		totalLength = 0;
		data_sum_length = 0;
		return true;

	} else {
		return false;
	}
}

/******************************************************************************
 * FunctionName : check_data
 * Description  : compare saved data length and total length (?)
 * Parameters   : recvPtr -- the received data pointer
 * 				  length -- data length
 * Returns      : true or false
 *******************************************************************************/
LOCAL bool ICACHE_FLASH_ATTR check_data(char *recvPtr, uint16 length) {

	char length_buf[10] = { 0 };
	char *temp = NULL;
	char *data = NULL;
	char *tmpRecvBufferPtr;
	uint16 tmpLength = length;
	uint32 tmpTotalLength = 0;

	temp = (char *) os_strstr(recvPtr, "\r\n\r\n");

	if (temp != NULL) {
		tmpLength -= temp - recvPtr;
		tmpLength -= 4;
		tmpTotalLength += tmpLength;

		data = (char *) os_strstr(recvPtr, "Content-Length: ");

		if (data != NULL) {
			data += 16;
			tmpRecvBufferPtr = (char *) os_strstr(data, "\r\n");

			if (tmpRecvBufferPtr != NULL) {

				os_memcpy(length_buf, data, tmpRecvBufferPtr - data);
				data_sum_length = atoi(length_buf);
				os_printf("A_dat:%u,tot:%u,lenght:%u\n", data_sum_length, tmpTotalLength, tmpLength);

				if (data_sum_length != tmpTotalLength) {
					return false;
				}
			}
		}
	}
	return true;
}

/******************************************************************************
 * FunctionName : restart_10ms_cb
 * Description  : system restart or wifi reconnection after a certain time
 * Parameters   : arg -- additional argument to pass to the function
 * Returns      : none
 *******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR restart_10ms_cb(void *arg) {

	if (rstparm != NULL && rstparm->espconnPtr != NULL) {
		switch (rstparm->parmType) {

		case WIFI:
			if (sta_conf->ssid[0] != 0x00) {
				wifi_station_set_config(sta_conf);
				wifi_station_disconnect();
				wifi_station_connect();
				user_esp_platform_check_ip(1);
			}

			if (ap_conf->ssid[0] != 0x00) {
				wifi_softap_set_config(ap_conf);
				system_restart();
			}

			os_free(ap_conf);
			ap_conf = NULL;

			os_free(sta_conf);
			sta_conf = NULL;

			os_free(rstparm);
			rstparm = NULL;

			os_free(restart_10ms);
			restart_10ms = NULL;

			break;

		case DEEP_SLEEP:

		case REBOOT:
			if (rstparm->espconnPtr->state == ESPCONN_CLOSE) {
				wifi_set_opmode(STATION_MODE);
			} else {
				os_timer_arm(restart_10ms, 10, 0);
			}

			break;

		default:
			break;
		}
	}
}

/******************************************************************************
 * FunctionName : data_send
 * Description  : processing the data as http format and send to the client or server
 * Parameters   : arg -- argument to set for client or server
 *                responseOK -- true or false
 *                sendPtr -- the send data
 * Returns      : none
 *******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR data_send(void *arg, bool responseOK, char *sendData) {

	struct espconn *espconnPtr = arg;
	uint16 length = 0;
	char *buf = NULL;
	char httphead[256];
	os_memset(httphead, 0, 256);

	if (responseOK) {
		os_sprintf(httphead, "HTTP/1.0 200 OK\r\nContent-Length: %d\r\nServer: lwIP/1.4.0\r\n",
				sendData ? os_strlen(sendData) : 0);

		if (sendData) {
			os_sprintf(httphead + os_strlen(httphead),
					"Content-type: application/json\r\nExpires: Fri, 10 Apr 2008 14:00:00 GMT\r\nPragma: no-cache\r\n\r\n");
			length = os_strlen(httphead) + os_strlen(sendData);
			buf = (char *) os_zalloc(length + 1);
			os_memcpy(buf, httphead, os_strlen(httphead));
			os_memcpy(buf + os_strlen(httphead), sendData, os_strlen(sendData));

		} else {
			os_sprintf(httphead + os_strlen(httphead), "\n");
			length = os_strlen(httphead);
		}

	} else {
		os_sprintf(httphead, "HTTP/1.0 400 BadRequest\r\nContent-Length: 0\r\nServer: lwIP/1.4.0\r\n\n");
		length = os_strlen(httphead);
	}

	if (sendData) {
		espconn_sent(espconnPtr, buf, length);

	} else {
		espconn_sent(espconnPtr, httphead, length);
	}

	if (buf) {
		os_free(buf);
		buf = NULL;
	}
}

/******************************************************************************
 * FunctionName : json_send
 * Description  : processing the data as json format and send to the client or server
 * Parameters   : arg -- argument to set for client or server
 *                ParmType -- json format type
 * Returns      : none
 *******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR json_send(void *arg, ParmType ParmType) {

	struct espconn *espconnPtr = arg;
	char *buf = NULL;
	buf = (char *) os_zalloc(JSON_SIZE);

	switch (ParmType) {

	case INFOMATION:
		json_ws_send((struct jsontree_value *) &INFOTree, "info", buf);
		break;

	case WIFI:
		json_ws_send((struct jsontree_value *) &wifi_info_tree, "wifi", buf);
		break;

	case CONNECT_STATUS:
		json_ws_send((struct jsontree_value *) &con_status_tree, "info", buf);
		break;

	case USER_BIN:
		json_ws_send((struct jsontree_value *) &userinfo_tree, "user_info", buf);
		break;

	case SCAN: {
		u8 i = 0;
		u8 scanCount = 0;
		struct bss_info *bss = NULL;
		bss = bss_head;

		if (bss == NULL) {
			os_free(scan_info);
			scan_info = NULL;
			os_sprintf(buf, "{\n\"successful\": false,\n\"data\": null\n}");

		} else {
			do {
				if (scan_info->page_sn == scan_info->pagenum) {
					scan_info->page_sn = 0;
					os_sprintf(buf, "{\n\"successful\": false,\n\"meessage\": \"repeated page\"\n}");
					break;
				}

				scanCount = scannum - (scan_info->pagenum - 1) * 8;

				if (scanCount >= 8) {
					scan_info->data_cnt += 8;
					scan_info->page_sn = scan_info->pagenum;

					if (scan_info->data_cnt > scannum) {
						scan_info->data_cnt -= 8;
						os_sprintf(buf, "{\n\"successful\": false,\n\"meessage\": \"error page\"\n}");
						break;
					}

					json_ws_send((struct jsontree_value *) &scan_tree, "scan", buf);

				} else {
					scan_info->data_cnt += scanCount;
					scan_info->page_sn = scan_info->pagenum;

					if (scan_info->data_cnt > scannum) {
						scan_info->data_cnt -= scanCount;
						os_sprintf(buf, "{\n\"successful\": false,\n\"meessage\": \"error page\"\n}");
						break;
					}

					char *scanBufPtr = (char *) os_zalloc(JSON_SIZE);
					char *scanBufP = scanBufPtr;
					os_sprintf(scanBufP, ",\n\"ScanResult\": [\n");
					scanBufP += os_strlen(scanBufP);

					for (i = 0; i < scanCount; i++) {
						JSONTREE_OBJECT(page_tree, JSONTREE_PAIR("page", &scaninfo_tree));
						json_ws_send((struct jsontree_value *) &page_tree, "page", scanBufP);
						os_sprintf(scanBufP + os_strlen(scanBufP), ",\n");
						scanBufP += os_strlen(scanBufP);
					}

					os_sprintf(scanBufP - 2, "]\n");
					JSONTREE_OBJECT(scantree, JSONTREE_PAIR("TotalPage", &scan_callback),
							JSONTREE_PAIR("PageNum", &scan_callback));
					JSONTREE_OBJECT(scanres_tree, JSONTREE_PAIR("Response", &scantree));
					JSONTREE_OBJECT(scan_tree, JSONTREE_PAIR("scan", &scanres_tree));
					json_ws_send((struct jsontree_value *) &scan_tree, "scan", buf);
					os_memcpy(buf + os_strlen(buf) - 4, scanBufPtr, os_strlen(scanBufPtr));
					os_sprintf(buf + os_strlen(buf), "}\n}");
					os_free(scanBufPtr);
				}
			} while (0);
		}

		break;
	}

	default:
		break;
	}

	data_send(espconnPtr, true, buf);
	os_free(buf);
	buf = NULL;
}

/******************************************************************************
 * FunctionName : response_send
 * Description  : processing the send result
 * Parameters   : arg -- argument to set for client or server
 *                responseOK --  true or false
 * Returns      : none
 *******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR response_send(void *arg, bool responseOK) {

	struct espconn *espconnPtr = arg;
	data_send(espconnPtr, responseOK, NULL);
}

/******************************************************************************
 * FunctionName : json_scan_cb
 * Description  : processing the scan result
 * Parameters   : arg -- additional argument to pass to the callback function
 *                status -- scan status
 * Returns      : none
 *******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR json_scan_cb(void *arg, STATUS status) {

	scan_info->pbss = arg;

	if (scannum % 8 == 0) {
		scan_info->totalpage = scannum / 8;
	} else {
		scan_info->totalpage = scannum / 8 + 1;
	}

	JSONTREE_OBJECT(totaltree, JSONTREE_PAIR("TotalPage", &scan_callback));

	JSONTREE_OBJECT(totalres_tree, JSONTREE_PAIR("Response", &totaltree));

	JSONTREE_OBJECT(total_tree, JSONTREE_PAIR("total", &totalres_tree));

	bss_temp = bss_head;

	while (bss_temp != NULL) {
		bss_head = bss_temp->next.stqe_next;
		os_free(bss_temp);
		bss_temp = bss_head;
	}

	bss_head = NULL;
	bss_temp = NULL;
	bss = STAILQ_FIRST(scan_info->pbss);

	while (bss != NULL) {

		if (bss_temp == NULL) {
			bss_temp = (struct bss_info *) os_zalloc(sizeof(struct bss_info));
			bss_head = bss_temp;

		} else {
			bss_temp->next.stqe_next = (struct bss_info *) os_zalloc(sizeof(struct bss_info));
			bss_temp = bss_temp->next.stqe_next;
		}

		if (bss_temp == NULL) {
			os_printf("malloc scan info failed\n");
			break;

		} else {
			os_memcpy(bss_temp->bssid, bss->bssid, sizeof(bss->bssid));
			os_memcpy(bss_temp->ssid, bss->ssid, sizeof(bss->ssid));
			bss_temp->authmode = bss->authmode;
			bss_temp->rssi = bss->rssi;
			bss_temp->channel = bss->channel;
		}

		bss = STAILQ_NEXT(bss, next);
	}

	char *buf = NULL;
	buf = (char *) os_zalloc(JSON_SIZE);

	json_ws_send((struct jsontree_value *) &total_tree, "total", buf);

	data_send(scan_info->pespconn, true, buf);

	os_free(buf);
}

/******************************************************************************
 * FunctionName : upgrade_check_func
 * Description  : check upgrade result and send it to client or server
 * Parameters   : arg -- argument to set for client or server
 * Returns      : none
 *******************************************************************************/
void ICACHE_FLASH_ATTR upgrade_check_func(void *arg) {

	struct espconn *espconnPtr = arg;
	os_timer_disarm(&upgrade_check_timer);

	if (system_upgrade_flag_check() == UPGRADE_FLAG_START) {
		response_send(espconnPtr, false);
		system_upgrade_deinit();
		system_upgrade_flag_set(UPGRADE_FLAG_IDLE);
		upgrade_lock = 0;
		os_printf("local upgrade failed\n");

	} else if (system_upgrade_flag_check() == UPGRADE_FLAG_FINISH) {
		os_printf("local upgrade success\n");
		response_send(espconnPtr, true);
		upgrade_lock = 0;
	}
}

/******************************************************************************
 * FunctionName : upgrade_deinit
 * Description  : disconnect the connection with the host
 * Parameters   : bin -- server number
 * Returns      : none
 *******************************************************************************/
void ICACHE_FLASH_ATTR LOCAL local_upgrade_deinit(void) {

	if (system_upgrade_flag_check() != UPGRADE_FLAG_START) {
		os_printf("system upgrade deinit\n");
		system_upgrade_deinit();
	}
}

/******************************************************************************
 * FunctionName : upgrade_download
 * Description  : Processing the upgrade data from the host
 * Parameters   : bin -- server number
 *                userDataPtr -- The upgrade data (or NULL when the connection has been closed!)
 *                length -- The length of upgrade data
 * Returns      : none
 *******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR local_upgrade_download(void * arg, char *userDataPtr, unsigned short length) {

	struct espconn *espconnPtr = arg;

	char *ptr = NULL;
	char *tmp = NULL;
	char lengthBuffer[32];

	static uint32 totalLength = 0;
	static uint32 sumLength = 0;
	static uint32 eraseLength = 0;

	char a_buf[2] = { 0xE9, 0x03 };
	char b_buf[2] = { 0xEA, 0x04 };

	if (totalLength == 0&& (ptr = (char *)os_strstr(userDataPtr, "\r\n\r\n")) != NULL &&
	(ptr = (char *)os_strstr(userDataPtr, "Content-Length")) != NULL) {
		ptr = (char *) os_strstr(userDataPtr, "Content-Length: ");

		if (ptr != NULL) {
			ptr += 16;
			tmp = (char *) os_strstr(ptr, "\r\n");

			if (tmp != NULL) {
				os_memset(lengthBuffer, 0, sizeof(lengthBuffer));
				os_memcpy(lengthBuffer, ptr, tmp - ptr);
				sumLength = atoi(lengthBuffer);

				if (sumLength == 0) {
					os_timer_disarm(&upgrade_check_timer);
					os_timer_setfn(&upgrade_check_timer, (os_timer_func_t *) upgrade_check_func, espconnPtr);
					os_timer_arm(&upgrade_check_timer, 10, 0);
					return;
				}

			} else {
				os_printf("sumLength failed\n");
			}

		} else {
			os_printf("Content-Length: failed\n");
		}

		if (sumLength != 0) {

			if (sumLength >= LIMIT_ERASE_SIZE) {
				system_upgrade_erase_flash(0xFFFF);
				eraseLength = sumLength - LIMIT_ERASE_SIZE;

			} else {
				system_upgrade_erase_flash(sumLength);
				eraseLength = 0;
			}
		}

		ptr = (char *) os_strstr(userDataPtr, "\r\n\r\n");
		length -= ptr - userDataPtr;
		length -= 4;
		userDataPtr += length;
		os_printf("upgrade file download start.\n");
		system_upgrade(ptr + 4, length);

	} else {
		totalLength += length;

		if (eraseLength >= LIMIT_ERASE_SIZE) {
			system_upgrade_erase_flash(0xFFFF);
			eraseLength -= LIMIT_ERASE_SIZE;

		} else {
			system_upgrade_erase_flash(eraseLength);
			eraseLength = 0;
		}

		system_upgrade(userDataPtr, length);
	}

	if (totalLength == sumLength) {
		os_printf("upgrade file download finished.\n");
		system_upgrade_flag_set(UPGRADE_FLAG_FINISH);
		totalLength = 0;
		sumLength = 0;

		upgrade_check_func(espconnPtr);

		os_timer_disarm(&app_upgrade_10s);
		os_timer_setfn(&app_upgrade_10s, (os_timer_func_t *) local_upgrade_deinit, NULL);
		os_timer_arm(&app_upgrade_10s, 10, 0);
	}
}

/******************************************************************************
 * FunctionName : webserver_recv
 * Description  : Processing the received data from the server
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                userDataPtr -- The received data (or NULL when the connection has been closed!)
 *                length -- The length of received data
 * Returns      : none
 *******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR webserver_recv(void *arg, char *userDataPtr, unsigned short length) {

	struct espconn *espconnPtr = arg;
	URL_Frame *urlFramePtr = NULL;
	char *parseBufferPtr = NULL;
	bool parseFlag = false;

	if (upgrade_lock == 0) {
		os_printf("len:%u\n", length);

		if (check_data(userDataPtr, length) == false) {
			os_printf("goto\n");
			goto _temp_exit;
		}

		parseFlag = save_data(userDataPtr, length);

		if (parseFlag == false) {
			response_send(espconnPtr, false);
		}

		urlFramePtr = (URL_Frame *) os_zalloc(sizeof(URL_Frame));
		parse_url(recv_buffer, urlFramePtr);

		switch (urlFramePtr->Type) {

		case GET:

			os_printf("We have a GET request.\n");

			if (os_strcmp(urlFramePtr->pSelect, "client") == 0 &&
			os_strcmp(urlFramePtr->pCommand, "command") == 0) {

				if (os_strcmp(urlFramePtr->pFileName, "info") == 0) {
					json_send(espconnPtr, INFOMATION);
				}

				if (os_strcmp(urlFramePtr->pFileName, "status") == 0) {
					json_send(espconnPtr, CONNECT_STATUS);

				} else if (os_strcmp(urlFramePtr->pFileName, "scan") == 0) {
					char *strstr = NULL;
					strstr = (char *) os_strstr(userDataPtr, "&");

					if (strstr == NULL) {

						if (scan_info == NULL) {
							scan_info = (scaninfo *) os_zalloc(sizeof(scaninfo));
						}

						scan_info->pespconn = espconnPtr;
						scan_info->pagenum = 0;
						scan_info->page_sn = 0;
						scan_info->data_cnt = 0;
						wifi_station_scan(NULL, json_scan_cb);

					} else {
						strstr++;

						if (os_strncmp(strstr, "page", 4) == 0) {

							if (scan_info != NULL) {
								scan_info->pagenum = *(strstr + 5);
								scan_info->pagenum -= 0x30;

								if (scan_info->pagenum > scan_info->totalpage || scan_info->pagenum == 0) {
									response_send(espconnPtr, false);

								} else {
									json_send(espconnPtr, SCAN);
								}

							} else {
								response_send(espconnPtr, false);
							}

						} else if (os_strncmp(strstr, "finish", 6) == 0) {
							bss_temp = bss_head;

							while (bss_temp != NULL) {
								bss_head = bss_temp->next.stqe_next;
								os_free(bss_temp);
								bss_temp = bss_head;
							}

							bss_head = NULL;
							bss_temp = NULL;
							response_send(espconnPtr, true);
						} else {
							response_send(espconnPtr, false);
						}
					}

				} else {
					response_send(espconnPtr, false);
				}

			} else if (os_strcmp(urlFramePtr->pSelect, "config") == 0 &&
			os_strcmp(urlFramePtr->pCommand, "command") == 0) {

				if (os_strcmp(urlFramePtr->pFileName, "wifi") == 0) {
					ap_conf = (struct softap_config *) os_zalloc(sizeof(struct softap_config));
					sta_conf = (struct station_config *) os_zalloc(sizeof(struct station_config));
					json_send(espconnPtr, WIFI);
					os_free(sta_conf);
					os_free(ap_conf);
					sta_conf = NULL;
					ap_conf = NULL;

				} else if (os_strcmp(urlFramePtr->pFileName, "reboot") == 0) {
					json_send(espconnPtr, REBOOT);

				} else {
					response_send(espconnPtr, false);
				}

			} else if (os_strcmp(urlFramePtr->pSelect, "upgrade") == 0 &&
			os_strcmp(urlFramePtr->pCommand, "command") == 0) {

				if (os_strcmp(urlFramePtr->pFileName, "getuser") == 0) {
					json_send(espconnPtr, USER_BIN);
				}

			} else {
				response_send(espconnPtr, false);
			}

			break;

		case POST:

			os_printf("We have a POST request.\n");

			parseBufferPtr = (char *) os_strstr(recv_buffer, "\r\n\r\n");

			if (parseBufferPtr == NULL) {
				break;
			}

			parseBufferPtr += 4;

			if (os_strcmp(urlFramePtr->pSelect, "config") == 0 &&
			os_strcmp(urlFramePtr->pCommand, "command") == 0) {

				if (os_strcmp(urlFramePtr->pFileName, "reboot") == 0) {

					if (parseBufferPtr != NULL) {

						if (restart_10ms != NULL) {
							os_timer_disarm(restart_10ms);
						}

						if (rstparm == NULL) {
							rstparm = (rst_parm *) os_zalloc(sizeof(rst_parm));
						}

						rstparm->espconnPtr = espconnPtr;
						rstparm->parmType = REBOOT;

						if (restart_10ms == NULL) {
							restart_10ms = (os_timer_t *) os_malloc(sizeof(os_timer_t));
						}

						os_timer_setfn(restart_10ms, (os_timer_func_t *) restart_10ms_cb, NULL);
						os_timer_arm(restart_10ms, 10, 0);  // delay 10ms, then do

						response_send(espconnPtr, true);

					} else {
						response_send(espconnPtr, false);
					}

				} else if (os_strcmp(urlFramePtr->pFileName, "wifi") == 0) {

					if (parseBufferPtr != NULL) {
						struct jsontree_context js;
						user_esp_platform_set_connect_status(DEVICE_CONNECTING);

						if (restart_10ms != NULL) {
							os_timer_disarm(restart_10ms);
						}

						if (ap_conf == NULL) {
							ap_conf = (struct softap_config *) os_zalloc(sizeof(struct softap_config));
						}

						if (sta_conf == NULL) {
							sta_conf = (struct station_config *) os_zalloc(sizeof(struct station_config));
						}

						jsontree_setup(&js, (struct jsontree_value *) &wifi_req_tree, json_putchar);
						json_parse(&js, parseBufferPtr);

						if (rstparm == NULL) {
							rstparm = (rst_parm *) os_zalloc(sizeof(rst_parm));
						}

						rstparm->espconnPtr = espconnPtr;
						rstparm->parmType = WIFI;

						if (sta_conf->ssid[0] != 0x00 || ap_conf->ssid[0] != 0x00) {
							ap_conf->ssid_hidden = 0;
							ap_conf->max_connection = 4;

							if (restart_10ms == NULL) {
								restart_10ms = (os_timer_t *) os_malloc(sizeof(os_timer_t));
							}

							os_timer_disarm(restart_10ms);
							os_timer_setfn(restart_10ms, (os_timer_func_t *) restart_10ms_cb, NULL);
							os_timer_arm(restart_10ms, 10, 0);  // delay 10ms, then do

						} else {
							os_free(ap_conf);
							os_free(sta_conf);
							os_free(rstparm);
							sta_conf = NULL;
							ap_conf = NULL;
							rstparm = NULL;
						}

						response_send(espconnPtr, true);

					} else {
						response_send(espconnPtr, false);
					}

				} else {
					response_send(espconnPtr, false);
				}
			} else if (os_strcmp(urlFramePtr->pSelect, "upgrade") == 0 &&
			os_strcmp(urlFramePtr->pCommand, "command") == 0) {

				if (os_strcmp(urlFramePtr->pFileName, "start") == 0) {

					response_send(espconnPtr, true);
					os_printf("local upgrade start\n");
					upgrade_lock = 1;

					system_upgrade_init();
					system_upgrade_flag_set(UPGRADE_FLAG_START);

					os_timer_disarm(&upgrade_check_timer);
					os_timer_setfn(&upgrade_check_timer, (os_timer_func_t *) upgrade_check_func, NULL);
					os_timer_arm(&upgrade_check_timer, 120000, 0);

				} else if (os_strcmp(urlFramePtr->pFileName, "reset") == 0) {
					response_send(espconnPtr, true);
					os_printf("local upgrade restart\n");
					system_upgrade_reboot();

				} else {
					response_send(espconnPtr, false);
				}

			} else {
				response_send(espconnPtr, false);
			}

			break;
		}

		if (recv_buffer != NULL) {
			os_free(recv_buffer);
			recv_buffer = NULL;
		}

		os_free(urlFramePtr);
		urlFramePtr = NULL;
		_temp_exit: ;

	} else if (upgrade_lock == 1) {
		local_upgrade_download(espconnPtr, userDataPtr, length);

		if (recv_buffer != NULL) {
			os_free(recv_buffer);
			recv_buffer = NULL;
		}

		os_free(urlFramePtr);
		urlFramePtr = NULL;
	}
}

/******************************************************************************
 * FunctionName : webserver_recon
 * Description  : the connection has an err, reconnection
 * Parameters   : arg -- additional argument to pass to the callback function
 * 				  err -- error number
 * Returns      : none
 *******************************************************************************/
LOCAL ICACHE_FLASH_ATTR void webserver_recon(void *arg, sint8 err) {

	struct espconn *espconnPtr = arg;

	os_printf("webserver's client %d.%d.%d.%d:%d err %d reconnect\n", espconnPtr->proto.tcp->remote_ip[0],
			espconnPtr->proto.tcp->remote_ip[1], espconnPtr->proto.tcp->remote_ip[2],
			espconnPtr->proto.tcp->remote_ip[3], espconnPtr->proto.tcp->remote_port, err);
}

/******************************************************************************
 * FunctionName : webserver_discon
 * Description  : the connection has an err, disconnection
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
LOCAL ICACHE_FLASH_ATTR void webserver_discon(void *arg) {

	struct espconn *espconnPtr = arg;

	os_printf("webserver's client %d.%d.%d.%d:%d disconnected\n", espconnPtr->proto.tcp->remote_ip[0],
			espconnPtr->proto.tcp->remote_ip[1], espconnPtr->proto.tcp->remote_ip[2],
			espconnPtr->proto.tcp->remote_ip[3], espconnPtr->proto.tcp->remote_port);
}

/******************************************************************************
 * FunctionName : webserver_listen
 * Description  : server listened a connection successfully
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR webserver_listen(void *arg) {

	struct espconn *espconnPtr = arg;

	espconn_regist_recvcb(espconnPtr, webserver_recv);
	espconn_regist_reconcb(espconnPtr, webserver_recon);
	espconn_regist_disconcb(espconnPtr, webserver_discon);
}

/******************************************************************************
 * FunctionName : user_webserver_init
 * Description  : parameter initialize as a server
 * Parameters   : port -- server port
 * Returns      : none
 *******************************************************************************/
void ICACHE_FLASH_ATTR user_webserver_init(uint32 port) {

	LOCAL struct espconn espconnPtr;
	LOCAL esp_tcp espTcp;

	espconnPtr.type = ESPCONN_TCP;
	espconnPtr.state = ESPCONN_NONE;
	espconnPtr.proto.tcp = &espTcp;
	espconnPtr.proto.tcp->local_port = port;

	espconn_regist_connectcb(&espconnPtr, webserver_listen);
	espconn_accept(&espconnPtr);
}
