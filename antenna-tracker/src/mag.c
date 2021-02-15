/**
 * mag.c
 *
 * This file contains code for interfacing with the Honeywell HMC5883L
 * 3 axis magnetometer.
 *
 * @author Kalyan Sriram <kalyan@coderkalyan.com>
 */

#include <stdio.h>
#include <math.h>
#include <zephyr.h>
#include <device.h>
#include <drivers/sensor.h>
#include <logging/log.h>

#include "mag.h"

LOG_MODULE_REGISTER(mag, LOG_LEVEL_DBG);

struct mag_trigger_data {
	struct sensor_trigger *trigger;
	struct k_msgq *mag_msgq;
};
struct mag_trigger_data mag_trigger_data;

int sample_mag(const struct device *dev, struct mag_sample *mag_sample)
{
	int64_t time_now;
	struct sensor_value magn[3];

	time_now = k_uptime_get();

	int ret;
	ret = sensor_sample_fetch(dev);
	if (ret != 0) goto end;

	ret = sensor_channel_get(dev, SENSOR_CHAN_MAGN_XYZ, magn);
	if (ret != 0) goto end;

	mag_sample->timestamp = time_now;
	mag_sample->magn[0] = sensor_value_to_double(&magn[0]);
	mag_sample->magn[1] = sensor_value_to_double(&magn[1]);
	mag_sample->magn[2] = sensor_value_to_double(&magn[2]);

end:
	return ret;
}

#ifdef CONFIG_HMC5883L_TRIGGER
static void handle_hmc5883l_trigger(const struct device *dev,
		struct sensor_trigger *trig);

static struct sensor_trigger trigger;
int setup_hmc5883l_trigger(const struct device *dev, struct k_msgq *mag_msgq)
{
	trigger = (struct sensor_trigger) {
		.type = SENSOR_TRIG_DATA_READY,
		.chan = SENSOR_CHAN_ALL,
	};
	mag_trigger_data.trigger = &trigger;
	mag_trigger_data.mag_msgq = mag_msgq;

	if (sensor_trigger_set(dev, mag_trigger_data.trigger,
				handle_hmc5883l_trigger) < 0) {
		LOG_ERR("Failed to configure trigger");
		return -EIO;
	}

	LOG_DBG("Configured sensor for triggered sampling.");
	return 0;
}

static void handle_hmc5883l_trigger(const struct device *dev,
		struct sensor_trigger *trig)
{
	struct mag_sample mag_sample;
	int ret = sample_mag(dev, &mag_sample);
	if (ret != 0) {
		LOG_ERR("Error processing mag sample: %d", ret);
		(void) sensor_trigger_set(dev, trig, NULL);
		return;
	}

	while (k_msgq_put(mag_trigger_data.mag_msgq, &mag_sample, K_NO_WAIT) != 0) {
		LOG_ERR("Dropping mag samples");
		k_msgq_purge(mag_trigger_data.mag_msgq);
	}
}
#endif /* CONFIG_HMC5883L_TRIGGER */

#ifndef CONFIG_HMC5883L_TRIGGER
/* mag poll loop for when trigger is not enabled */
void mag_poll_thread_entry(void *arg1, void *arg2, void *unused3)
{
	LOG_DBG("Initializing mag poll thread");

	const struct device *dev = (struct device *) arg1;
	struct k_msgq *mag_msgq = (struct k_msgq *) arg2;

	struct mag_sample mag_sample;

	while (1) {
		int ret = sample_mag(dev, &mag_sample);
		if (ret != 0) {
			LOG_ERR("Error sampling mag: %d", ret);
		}

		while (k_msgq_put(mag_msgq, &mag_sample, K_NO_WAIT) != 0) {
			LOG_ERR("Dropping mag samples");
			k_msgq_purge(mag_msgq);
		}

		k_sleep(K_MSEC(100)); /* 10Hz poll rate */
	}
}
#endif /* !CONFIG_HMC5883L_TRIGGER */
