#ifndef PWMCTRL_H
#define PWMCTRL_H

#include <zephyr.h>

enum motor {
	MOTOR_ALTITUDE = 0,
	MOTOR_AZIMUTH = 1,
};

struct motor_setpoint {
	enum motor motor;
	int16_t pwm;
};

void pwmctrl_device_init(struct k_msgq *msgq);

static const double PWM_MIN = 1000;
static const double PWM_CENTER = 1500;
static const double PWM_MAX = 2000;

#endif /* PWMCTRL_H */
