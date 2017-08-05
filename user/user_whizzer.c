/*
 *  user_stepper.c
 *
 *  Created on: 22 dec 2016
 *      Author: asemenkov
 */

#include "ets_sys.h"
#include "mem.h"
#include "osapi.h"
#include "os_type.h"
#include "pwm.h"
#include "gpio.h"

#include "user_whizzer.h"

/*
 * Phase 1 - Awake the motor and rotate slowly (MOTOR_STEP_IO_NUM PWM circle is large)
 * Phase 2 - Accelerate motor (MOTOR_STEP_IO_NUM PWM cycle is being decreased)
 * Phase 3 - Stop motor (sleep mode + brakes)
 */

LOCAL uint32 step_periods_us[MAX_PWM_CYCLE_DECAY_STOP] = { 4000, 3923, 3848, 3774, 3701, 3630, 3560, 3491, 3424, 3357,
		3292, 3228, 3166, 3104, 3043, 2984, 2925, 2868, 2812, 2756, 2702, 2649, 2596, 2545, 2494, 2445, 2396, 2348,
		2301, 2255, 2209, 2165, 2121, 2078, 2036, 1994, 1954, 1914, 1874, 1836, 1798, 1761, 1724, 1688, 1653, 1619,
		1585, 1551, 1518, 1486, 1455, 1424, 1393, 1363, 1334, 1305, 1276, 1249, 1221, 1194, 1168, 1142, 1117, 1092,
		1067, 1043, 1019, 996, 973, 951, 929, 907, 886, 865, 845, 824, 805, 785, 766, 747, 729, 711, 693, 676, 659, 642,
		625, 609, 593, 578, 562, 547, 532, 518, 504, 490, 476, 462, 449, 436, 423, 411, 398, 386, 374, 362, 351, 340,
		329, 318, 307, 296, 286, 276, 266, 256, 247, 237, 228, 219, 210, 201, 193, 184, 176, 168, 160, 152, 144, 137,
		129, 122, 115, 108, 101, 94, 87, 81, 74, 68, 62, 55, 49, 43, 38, 32, 26, 21, 16, 10 };

LOCAL os_timer_t step_timer;

LOCAL uint32 PWM_DUTY = 67;
LOCAL uint8 pwm_stopper;
LOCAL uint32 macro_steps;
LOCAL uint8 micro_counter;

/******************************************************************************
 * FunctionName : user_get_remaining_macro_steps
 * Description  : get the amount macro steps remaining
 * Parameters   : none
 * Returns      : macro_step
 ******************************************************************************/
uint16 ICACHE_FLASH_ATTR user_get_remaining_macro_steps(void) {
	return macro_steps;
}

/******************************************************************************
 * FunctionName : user_set_motor_brake
 * Description  : turns motor brake on/off
 * Parameters   : brake - brake on/off
 * Returns      : none
 ******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_set_motor_brake(bool brake) {
	if (brake) {
		gpio_output_set(0, MOTOR_BRAKE_IO_NUM, MOTOR_BRAKE_IO_NUM, 0);
	} else {
		gpio_output_set(MOTOR_BRAKE_IO_NUM, 0, MOTOR_BRAKE_IO_NUM, 0);

	}
}

/******************************************************************************
 * FunctionName : user_set_direction
 * Description  : set direction of motor rotation
 * Parameters   : dir - clockwise/counterclockwise
 * Returns      : none
 ******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_set_direction(bool dir) {
	if (dir) {
		gpio_output_set(MOTOR_DIR_IO_NUM, 0, MOTOR_DIR_IO_NUM, 0);
	} else {
		gpio_output_set(0, MOTOR_DIR_IO_NUM, MOTOR_DIR_IO_NUM, 0);
	}
}

/******************************************************************************
 * FunctionName : user_set_motor_sleep
 * Description  : set motor into/outa sleep mode
 * Parameters   : sleep - sleep mode on/off
 * Returns      : none
 ******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_set_motor_sleep(bool sleep) {
	if (sleep) {
		gpio_output_set(0, MOTOR_SLEEP_IO_NUM, MOTOR_SLEEP_IO_NUM, 0);
	} else {
		gpio_output_set(MOTOR_SLEEP_IO_NUM, 0, MOTOR_SLEEP_IO_NUM, 0);
	}
}

/******************************************************************************
 * FunctionName : user_rotate_motor_slowly
 * Description  : slowly rotate the motor
 * Parameters   : none
 * Returns      : none
 ******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_rotate_motor_slowly(void) {
	// Pull the brakes
	user_set_motor_brake(0);

	// Don't proceed emulation, if macro steps counter == 0
	if (macro_steps < 1) {
		GPIO_OUTPUT_SET(MOTOR_STEP_IO_NUM, 0);
		user_set_motor_brake(1);
		os_printf("step emulation complete\n");
		return;
	}

	os_printf("%d steps left\n", user_get_remaining_macro_steps());
	macro_steps -= 1;

	// Start PWM and awake the motor
	pwm_set_period(12000);
	pwm_start();
	user_set_motor_sleep(0);

	// Set acceleration function -> Phase 2
	os_timer_disarm(&step_timer);
	os_timer_setfn(&step_timer, (os_timer_func_t *) user_accelerate_motor);
	os_timer_arm(&step_timer, ROTATE_TIME_MS, 0);
}

/******************************************************************************
 * FunctionName : user_accelerate_motor
 * Description  : start the motor acceleration
 * Parameters   : none
 * Returns      : none
 ******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_accelerate_motor(void) {
	// Acceleration via PWM circle reduction
	os_timer_disarm(&step_timer);
	os_timer_setfn(&step_timer, (os_timer_func_t *) user_set_pwm_period);
	os_timer_arm(&step_timer, PWM_UPDATE_INTERVAL_MS, 1);
}

/******************************************************************************
 * FunctionName : user_set_pwm_period
 * Description  : emulation of a single micro step
 * Parameters   : none
 * Returns      : none
 ******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_set_pwm_period(void) {
	// Check whether micro_counter == size of step_periods_us
	if (micro_counter == pwm_stopper) {
		micro_counter = 0;
		user_brake_motor(); // Phase 3: use brakes to stop the motor
	} else {
		pwm_set_period(step_periods_us[micro_counter++]);
		pwm_start();
	}
}

/******************************************************************************
 * FunctionName : user_brake_motor
 * Description  : stop the motor rotation using brakes
 * Parameters   : none
 * Returns      : none
 ******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_brake_motor(void) {
	// Set the motor into sleep mode and push the brakes
	user_set_motor_sleep(1);
	user_set_motor_brake(1);

	// Set hold function -> Phase 1
	os_timer_disarm(&step_timer);
	os_timer_setfn(&step_timer, (os_timer_func_t *) user_rotate_motor_slowly);
	os_timer_arm(&step_timer, BRAKE_TIME_MS, 0);
}

/******************************************************************************
 * FunctionName : user_emulate_macro_steps
 * Description  : set emulation parameters
 * Parameters   : steps_number - the amount of macro steps to be emulated
 * Returns      : none
 ******************************************************************************/
void ICACHE_FLASH_ATTR user_emulate_macro_steps(uint32 steps_number, uint8 pwm_cycle_decay_stop) {
	// Initialize macro_steps counter
	os_printf("starting step emulation\n");
	pwm_stopper = pwm_cycle_decay_stop - 1;
	macro_steps = steps_number;

	// Begin the Phase 1
	user_rotate_motor_slowly();
}

/******************************************************************************
 * FunctionName : user_whizzer_init
 * Description  : whizzer pins initialization
 * Parameters   : none
 * Returns      : none
 ******************************************************************************/
void ICACHE_FLASH_ATTR user_whizzer_init(void) {
	PIN_FUNC_SELECT(MOTOR_DIR_IO_MUX, MOTOR_DIR_IO_FUNC);
	PIN_FUNC_SELECT(MOTOR_SLEEP_IO_MUX, MOTOR_SLEEP_IO_FUNC);
	PIN_FUNC_SELECT(MOTOR_BRAKE_IO_MUX, MOTOR_BRAKE_IO_FUNC);
	PIN_FUNC_SELECT(MOTOR_STEP_IO_MUX, MOTOR_STEP_IO_FUNC);

	// Initialize 'dir' and send the motor to sleep
	user_set_direction(1);
	user_set_motor_sleep(1);

	// Initialize 'step' pin
	uint32 io_info[][3] = { { MOTOR_STEP_IO_MUX, MOTOR_STEP_IO_FUNC, MOTOR_STEP_IO_NUM } };
	pwm_init(1000000, &PWM_DUTY, 1, io_info);
	set_pwm_debug_en(0); //disable debug print in pwm driver
}
