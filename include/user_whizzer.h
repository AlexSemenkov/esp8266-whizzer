/*
 *  user_whizzer.h
 *
 *  Created on: 22 dec 2016
 *      Author: asemenkov
 */

#define ROTATE_TIME_MS				1000
#define BRAKE_TIME_MS				1000
#define PWM_UPDATE_INTERVAL_MS		20

#define MAX_STEPS					1000
#define MIN_STEPS					1

#define MAX_PWM_CYCLE_DECAY_STOP	150
#define MIN_PWM_CYCLE_DECAY_STOP	10

#define MOTOR_DIR_IO_MUX     		PERIPHS_IO_MUX_GPIO2_U
#define MOTOR_DIR_IO_NUM     		BIT2
#define MOTOR_DIR_IO_FUNC    		FUNC_GPIO2

#define MOTOR_SLEEP_IO_MUX     		PERIPHS_IO_MUX_MTDI_U
#define MOTOR_SLEEP_IO_NUM     		BIT12
#define MOTOR_SLEEP_IO_FUNC    		FUNC_GPIO12

#define MOTOR_STEP_IO_MUX     		PERIPHS_IO_MUX_MTCK_U
#define MOTOR_STEP_IO_NUM     		13
#define MOTOR_STEP_IO_FUNC    		FUNC_GPIO13

#define MOTOR_BRAKE_IO_MUX     		PERIPHS_IO_MUX_MTMS_U
#define MOTOR_BRAKE_IO_NUM     		BIT14
#define MOTOR_BRAKE_IO_FUNC    		FUNC_GPIO14

LOCAL void user_set_motor_brake(bool);
LOCAL void user_set_direction(bool);
LOCAL void user_set_motor_sleep(bool);

LOCAL void user_rotate_motor_slowly(void);
LOCAL void user_accelerate_motor(void);
LOCAL void user_set_pwm_period(void);
LOCAL void user_brake_motor(void);

uint16 user_get_remaining_macro_steps(void);
void user_emulate_macro_steps(uint32, uint8);
void user_whizzer_init(void);
