#include <stdio.h>
#include <zephyr.h>
#include <device.h>
#include <logging/log.h>

#include "util.h"
#include "pwmctrl.h"
#include "estimator.h"
#include "attctrl.h"

LOG_MODULE_REGISTER(attctrl, LOG_LEVEL_DBG);

#define ATTCTRL_STACK_SIZE 2000
#define ATTCTRL_PRIORITY 0

extern void attctrl_thread_entry(void *, void *, void *);

K_THREAD_STACK_DEFINE(attctrl_stack_area, ATTCTRL_STACK_SIZE);
struct k_thread attctrl_thread_data;

static struct pid_state altitude_pid = {
	.kp = .025,
	.ki = 0.00001,
	.kd = 0.00001,

	.integral = 0,
	.prev_error = 0,
	.cur_error = 0,
	.prev_time = 0,
	.cur_time = 0,
};

static struct pid_state azimuth_pid = {
	.kp = .025,
	.ki = 0.00001,
	.kd = 0.00001,

	.integral = 0,
	.prev_error = 0,
	.cur_error = 0,
	.prev_time = 0,
	.cur_time = 0,
};

void attctrl_init(struct k_msgq *attitude_msgq, struct k_msgq *pwmctrl_msgq,
		struct k_msgq *command_msgq)
{
	LOG_INF("Initializing attctrl interface");

	k_tid_t attctrl_tid = k_thread_create(&attctrl_thread_data, attctrl_stack_area,
										  K_THREAD_STACK_SIZEOF(attctrl_stack_area),
										  attctrl_thread_entry,
										  (void *) attitude_msgq, (void *) pwmctrl_msgq, 
										  (void *) command_msgq,
										  ATTCTRL_PRIORITY, 0, K_NO_WAIT);
}

/* 
 * Processes the latest frame through a pid controller, and returns
 * the next setpoint to apply. This function is generic and is used for both
 * the altitude and azimuth controllers.
 *
 * @param state the state information for the pid controller
 * @param input the latest sensor value (IMU angle, compass heading, etc)
 * 				normalized from -1..1
 * @return setpoint normalized from -1..1
 */
float pid_process(struct pid_state *state, float input, float target, int64_t cur_time)
{
	float setpoint;
	
	state->prev_error = state->cur_error;
	state->prev_time = state->cur_time;
	state->cur_time = cur_time;
	state->cur_error = (target - input);
	state->integral += state->cur_error;
	float dtime = ((double) (state->cur_time - state->prev_time)) / 1000.0;

	float p_term = state->kp * state->cur_error;
	float i_term = state->ki * state->integral;
	float d_term = state->kd * ((state->cur_error - state->prev_error) / (dtime));

	setpoint = p_term + i_term + d_term;

	/*printf("%f %f %f %f\n", p_term, i_term, d_term, setpoint);*/
	/*
	 * we *should* do better normalization, but for now just
	 * constrain the setpoint to [-1, 1] and make sure the gains
	 * are scaled correctly
	 */

	setpoint = constrain(setpoint, -1.0, 1.0);

	return setpoint;
}

void attctrl_thread_entry(void *arg1, void *arg2, void *arg3)
{
	LOG_INF("Starting attctrl thread");

	struct k_msgq *attitude_msgq = (struct k_msgq *) arg1;
	struct k_msgq *pwmctrl_msgq = (struct k_msgq *) arg2;
	struct k_msgq *command_msgq = (struct k_msgq *) arg3;

	struct motor_setpoint altitude_setpoint = {
		.motor = MOTOR_ALTITUDE,
	};
	struct motor_setpoint azimuth_setpoint = {
		.motor = MOTOR_AZIMUTH,
	};

	struct attitude_frame att_frame;
	k_msgq_get(attitude_msgq, &att_frame, K_FOREVER);
	altitude_pid.prev_time = att_frame.timestamp;

	float target_alt = 0, target_azm = 0;

	struct command_setpoint command_setpoint;

	while (1) {
		/* wait for an attitude frame */
		k_msgq_get(attitude_msgq, &att_frame, K_FOREVER);
		/* check if there's a new command setpoint (without waiting)
		 * but use the existing one if not */
		k_msgq_get(command_msgq, &command_setpoint, K_NO_WAIT);

		target_alt = command_setpoint.data.euler[0];
		target_azm = 0;//command_setpoint.data.euler[2];

		printf("Angle: %03.1f %03.1f %03.1f\n", att_frame.angle[0], att_frame.angle[1], att_frame.angle[2]);
		float alt_pid_setpoint = pid_process(&altitude_pid, att_frame.angle[0],
										 target_alt, att_frame.timestamp);

		/* scale the normalized setpoint value [-1, 1] to [PWM_MIN, PWM_MAX] */
		if (alt_pid_setpoint >= 0) {
			altitude_setpoint.pwm = (alt_pid_setpoint * (PWM_MAX - PWM_CENTER)) + PWM_CENTER;
		} else {
			altitude_setpoint.pwm = (alt_pid_setpoint * (PWM_CENTER - PWM_MIN)) + PWM_CENTER;
		}

		float azm_pid_setpoint = pid_process(&azimuth_pid, att_frame.angle[2],
				target_azm, att_frame.timestamp);

		/* scale the normalized setpoint value [-1, 1] to [PWM_MIN, PWM_MAX] */
		if (azm_pid_setpoint >= 0) {
			azimuth_setpoint.pwm = (azm_pid_setpoint * (PWM_MAX - PWM_CENTER)) + PWM_CENTER;
		} else {
			azimuth_setpoint.pwm = (azm_pid_setpoint * (PWM_CENTER - PWM_MIN)) + PWM_CENTER;
		}
		/*printf("(%f - %f) = %f; %f %d\n",target, att_frame.angle[0],*/
				/*(target - att_frame.angle[0]), pid_setpoint, altitude_setpoint.pwm);*/

		while (k_msgq_put(pwmctrl_msgq, &altitude_setpoint, K_NO_WAIT) != 0) {
			LOG_ERR("Dropping setpoint frames");
			k_msgq_purge(pwmctrl_msgq);
		}
		while (k_msgq_put(pwmctrl_msgq, &azimuth_setpoint, K_NO_WAIT) != 0) {
			LOG_ERR("Dropping setpoint frames");
			k_msgq_purge(pwmctrl_msgq);
		}
	}
}
