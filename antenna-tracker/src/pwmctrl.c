#include <zephyr.h>
#include <device.h>
#include <drivers/pwm.h>
#include <logging/log.h>

#include "board.h"
#include "pwmctrl.h"

LOG_MODULE_REGISTER(pwmctrl, LOG_LEVEL_DBG);

#define PWMCTRL_STACK_SIZE 2000
#define PWMCTRL_PRIORITY 0

extern void pwmctrl_thread_entry(void *, void *, void *);

K_THREAD_STACK_DEFINE(pwmctrl_stack_area, PWMCTRL_STACK_SIZE);
struct k_thread pwmctrl_thread_data;

void pwmctrl_device_init(struct k_msgq *msgq)
{
	LOG_INF("Initializing pwmctrl interface");

	/* altitude motor */
	const struct device *alt_dev = device_get_binding(ALT_LABEL);
	if (alt_dev == NULL) {
		LOG_ERR("Unable to find device %s", ALT_LABEL);
		return;
	}

	/* azimuth motor */
	const struct device *azm_dev = device_get_binding(AZM_LABEL);
	if (azm_dev == NULL) {
		LOG_ERR("Unable to find evice %s", AZM_LABEL);
		return;
	}

	k_tid_t pwmctrl_tid = k_thread_create(&pwmctrl_thread_data, pwmctrl_stack_area,
										  K_THREAD_STACK_SIZEOF(pwmctrl_stack_area),
										  pwmctrl_thread_entry,
										  (void *) msgq, NULL, NULL,
										  PWMCTRL_PRIORITY, 0, K_NO_WAIT);
}

void pwmctrl_thread_entry(void *arg1, void *unused2, void *unused3)
{
	LOG_INF("Initializing pwmctrl");

	struct k_msgq *msgq = (struct k_msgq *) arg1;

	/* altitude motor */
	const struct device *alt_dev = device_get_binding(ALT_LABEL);
	if (alt_dev == NULL) {
		LOG_ERR("Unable to find device %s", ALT_LABEL);
		return;
	}

	/* azimuth motor */
	const struct device *azm_dev = device_get_binding(AZM_LABEL);
	if (azm_dev == NULL) {
		LOG_ERR("Unable to find evice %s", AZM_LABEL);
		return;
	}

	struct motor_setpoint setpoint;

	while (1) {
		k_msgq_get(msgq, &setpoint, K_FOREVER);

		switch (setpoint.motor) {
		case MOTOR_ALTITUDE:
			pwm_pin_set_usec(alt_dev, ALT_CHANNEL,
					ALT_PERIOD, setpoint.pwm, ALT_FLAGS);
			break;
		case MOTOR_AZIMUTH:
			pwm_pin_set_usec(azm_dev, AZM_CHANNEL,
					AZM_PERIOD, setpoint.pwm, AZM_FLAGS);
			break;
		}
	}
}
