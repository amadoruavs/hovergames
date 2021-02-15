#ifndef ATTCTRL_H
#define ATTCTRL_H

#include <zephyr.h>

struct pid_state {
	const float kp;
	const float ki;
	const float kd;

	float integral;
	float prev_error, cur_error;
	int64_t prev_time, cur_time;
};

enum command_setpoint_type {
	COMMAND_SETPOINT_TYPE_EULER = 0,
	COMMAND_SETPOINT_TYPE_QUATERNION = 1,
};

struct command_setpoint {
	enum command_setpoint_type type;

	union {
		float euler[3];
		float quaternion[4];
	} data;
};

void attctrl_init(struct k_msgq *imu_msgq, struct k_msgq *pwmctrl_msgq,
		struct k_msgq *command_msgq);

#endif /* ATTCTRL_H */
