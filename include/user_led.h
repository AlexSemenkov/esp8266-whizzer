/*
 *  user_led.h
 *
 *  Created on: 07 dec 2016
 *      Author: asemenkov
 */

#define FAN_LED_IO_MUX     		PERIPHS_IO_MUX_MTDO_U
#define FAN_LED_IO_NUM     		BIT15
#define FAN_LED_IO_FUNC    		FUNC_GPIO15

#define DNS_LED_IO_MUX     		PERIPHS_IO_MUX_GPIO4_U
#define DNS_LED_IO_NUM     		BIT4
#define DNS_LED_IO_FUNC    		FUNC_GPIO4

#define CLOUD_LED_IO_MUX    	PERIPHS_IO_MUX_GPIO5_U
#define CLOUD_LED_IO_NUM    	BIT5
#define CLOUD_LED_IO_FUNC    	FUNC_GPIO5

typedef struct {
	bool fan_led_status;
	bool dns_led_status;
	bool cloud_led_status;
	uint8 pad[1];
} LedsStatus;

void user_set_dns_led(bool);
void user_set_cloud_led(bool);
void user_set_fan_led(bool);
void user_leds_init(void);
