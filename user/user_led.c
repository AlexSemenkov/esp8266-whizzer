/*
 * user_led.c
 *
 *  Created on: 07 dec 2016
 *      Author: asemenkov
 */

#include "ets_sys.h"
#include "gpio.h"
#include "user_led.h"

LOCAL LedsStatus leds_status = {0};

/******************************************************************************
 * FunctionName : user_set_fan_led
 * Description  : turns UART led on/off
 * Parameters   : status - status to be set
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR user_set_fan_led(bool status) {
	if (leds_status.fan_led_status != status) {
		if (status) {
			gpio_output_set(FAN_LED_IO_NUM, 0, FAN_LED_IO_NUM, 0);
		} else {
			gpio_output_set(0, FAN_LED_IO_NUM, FAN_LED_IO_NUM, 0);
		}
		leds_status.fan_led_status = status;
	}
}


/******************************************************************************
 * FunctionName : user_set_dns_led
 * Description  : turns DNS led on/off
 * Parameters   : status - status to be set
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR user_set_dns_led(bool status) {
	if (leds_status.dns_led_status != status) {
		if (status) {
			gpio_output_set(DNS_LED_IO_NUM, 0, DNS_LED_IO_NUM, 0);
		} else {
			user_set_cloud_led(0);
			gpio_output_set(0, DNS_LED_IO_NUM, DNS_LED_IO_NUM, 0);
		}
		leds_status.dns_led_status = status;
	}
}


/******************************************************************************
 * FunctionName : user_set_cloud_led
 * Description  : turns CLOUD led on/off
 * Parameters   : status - status to be set
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR user_set_cloud_led(bool status) {
	if (leds_status.cloud_led_status != status) {
		if (status) {
			gpio_output_set(CLOUD_LED_IO_NUM, 0, CLOUD_LED_IO_NUM, 0);
		} else {
			gpio_output_set(0, CLOUD_LED_IO_NUM, CLOUD_LED_IO_NUM, 0);
		}
		leds_status.cloud_led_status = status;
	}
}


/******************************************************************************
 * FunctionName : user_leds_init
 * Description  : UART, DNS and CLOUD leds initialization
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR user_leds_init(void) {
	PIN_FUNC_SELECT(FAN_LED_IO_MUX, FAN_LED_IO_FUNC);
	PIN_FUNC_SELECT(DNS_LED_IO_MUX, DNS_LED_IO_FUNC);
	PIN_FUNC_SELECT(CLOUD_LED_IO_MUX, CLOUD_LED_IO_FUNC);

	gpio_output_set(FAN_LED_IO_NUM, 0, FAN_LED_IO_NUM, 0);
	gpio_output_set(0, CLOUD_LED_IO_NUM, CLOUD_LED_IO_NUM, 0);
	gpio_output_set(0, DNS_LED_IO_NUM, DNS_LED_IO_NUM, 0);

	leds_status.fan_led_status = 1;
	leds_status.dns_led_status = 0;
	leds_status.cloud_led_status = 0;
}
