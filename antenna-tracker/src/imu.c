/**
 * imu.c
 *
 * This file contains code for interfacing with the Invensense MPU6050
 * inertial measurement unit.
 *
 * @author Kalyan Sriram <kalyan@coderkalyan.com>
 */

#include <stdio.h>
#include <math.h>

#include <zephyr.h>
#include <device.h>
#include <drivers/sensor.h>
#include <logging/log.h>

#include "threads.h"
#include "imu.h"

LOG_MODULE_REGISTER(imu, LOG_LEVEL_DBG);

int sample_imu(const struct device *dev, struct imu_sample *imu_sample)
{
	int64_t time_now;
	struct sensor_value gyro[3];
	struct sensor_value accel[3];
	struct sensor_value temperature;

	time_now = k_uptime_get();

	int ret;
	ret = sensor_sample_fetch(dev);
	if (ret != 0) goto end;

	ret = sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ, accel);
	if (ret != 0) goto end;
	ret = sensor_channel_get(dev, SENSOR_CHAN_GYRO_XYZ, gyro);
	if (ret != 0) goto end;
	ret = sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
	if (ret != 0) goto end;

	imu_sample->timestamp = time_now;
	imu_sample->gyro[0] = sensor_value_to_double(&gyro[0]);
	imu_sample->gyro[1] = sensor_value_to_double(&gyro[1]);
	imu_sample->gyro[2] = sensor_value_to_double(&gyro[2]);
	imu_sample->accel[0] = sensor_value_to_double(&accel[0]);
	imu_sample->accel[1] = sensor_value_to_double(&accel[1]);
	imu_sample->accel[2] = sensor_value_to_double(&accel[2]);
	imu_sample->temp = sensor_value_to_double(&temperature);

end:
	return ret;
}

#ifdef CONFIG_MPU6050_TRIGGER
static void handle_mpu6050_trigger(const struct device *dev,
		struct sensor_trigger *trig);

static struct sensor_trigger trigger;
int setup_mpu6050_trigger(const struct device *dev)
{
	trigger = (struct sensor_trigger) {
		.type = SENSOR_TRIG_DATA_READY,
		.chan = SENSOR_CHAN_ALL,
	};

	if (sensor_trigger_set(dev, &trigger,
				handle_mpu6050_trigger) < 0) {
		LOG_ERR("Failed to configure trigger");
		return -EIO;
	}

	LOG_DBG("Configured sensor for triggered sampling.");
	return 0;
}

static void handle_mpu6050_trigger(const struct device *dev,
		struct sensor_trigger *trig)
{
	struct imu_sample imu_sample;
	int ret = sample_imu(dev, &imu_sample);
	if (ret != 0) {
		LOG_ERR("Error processing IMU sample: %d", ret);
		(void) sensor_trigger_set(dev, trig, NULL);
		return;
	}

	while (k_msgq_put(&imu_msgq, &imu_sample, K_NO_WAIT) != 0) {
		LOG_ERR("Dropping IMU samples");
		k_msgq_purge(&imu_msgq);
	}
}
#endif /* CONFIG_MPU6050_TRIGGER */

#ifndef CONFIG_MPU6050_TRIGGER
#ifdef IMU_POLL_THREAD
/* IMU poll loop for when trigger is not enabled */
void imu_poll_thread_entry(void *arg1, void *arg2, void *unused3)
{
	LOG_DBG("Initializing IMU poll thread");

	const struct device *dev = (struct device *) arg1;
	struct k_msgq *imu_msgq = (struct k_msgq *) arg2;

	struct imu_sample imu_sample;

	while (1) {
		int ret = sample_imu(dev, &imu_sample);
		if (ret != 0) {
			LOG_ERR("Error sampling IMU: %d", ret);
		}

		while (k_msgq_put(imu_msgq, &imu_sample, K_NO_WAIT) != 0) {
			LOG_ERR("Dropping IMU samples");
			k_msgq_purge(imu_msgq);
		}

		k_sleep(K_MSEC(1)); /* 1Khz poll rate */
	}
}
#endif /* IMU_POLL_THREAD */
#endif /* !CONFIG_MPU6050_TRIGGER */
