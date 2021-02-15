/**
 * estimator.c
 *
 * This file contains a simple state estimator that uses a complimentary filter
 * to estimate the attitude of the antenna tracker, using IMU and magnetometer data.
 *
 * @author Kalyan Sriram <kalyan@coderkalyan.com>
 */

#include <stdio.h>
#include <math.h>

#include <zephyr.h>
#include <device.h>
#include <drivers/sensor.h>
#include <logging/log.h>

#include "estimator.h"
#include "imu.h"
#include "mag.h"

LOG_MODULE_REGISTER(estimator, LOG_LEVEL_DBG);

#define EST_STACK_SIZE 2000
#define EST_PRIORITY 0

static const float M_PI = 3.1415926;

extern void est_thread_entry(void *, void *, void *);

K_THREAD_STACK_DEFINE(est_stack_area, EST_STACK_SIZE);
struct k_thread est_thread_data;

void est_init(struct k_msgq *imu_msgq, struct k_msgq *mag_msgq,
		struct k_msgq *attitude_msgq)
{
	LOG_INF("Initializing estimator");

	k_tid_t est_tid = k_thread_create(&est_thread_data, est_stack_area,
									  K_THREAD_STACK_SIZEOF(est_stack_area),
									  est_thread_entry,
									  (void *) imu_msgq, (void *) mag_msgq, 
									  (void *) attitude_msgq,
									  EST_PRIORITY, 0, K_NO_WAIT);
}


void est_thread_entry(void *arg1, void *arg2, void *arg3)
{
	LOG_DBG("Initializing estimator thread");

	struct k_msgq *imu_msgq = (struct k_msgq *) arg1;
	struct k_msgq *mag_msgq = (struct k_msgq *) arg2;
	struct k_msgq *attitude_msgq = (struct k_msgq *) arg3;

	int64_t time_now, time_prev;
	double time_delta; /* in seconds */

	/* imu data */
	struct imu_sample imu_sample;
	float gyro_error[3] = {0};
	float gyro_dangle[3] = {0}, gyro_danglep[3] = {0};
	float accel_error[3] = {0};
	float accel_rawp[3] = {0};
	float accel_angle[3] = {0};
	float angle[3] = {0};

	/* mag data */
	struct mag_sample mag_sample;
	float magn_scaled[3] = {0};
	float magn_rot[3] = {0};
	float heading = 0;

	/* imu rotation matrix */
	float irot[3][3] =
	{
		{0, 0, 1},
		{0, 1, 0},
		{-1, 0, 0}
	};
	/* mag rotation matrix */
	float mrot[3][3] =
	{
		{1, 0, 0},
		{0, 1, 0},
		{0, 0, 1},
	};

	const float calib_samples = 250;
	for (int i = 0;i < calib_samples;i++) {
		k_msgq_get(imu_msgq, &imu_sample, K_FOREVER);

		for (int j = 0;j < 3;j++) {
			gyro_error[j] += imu_sample.gyro[j];
		}
		accel_error[0] += atan2f(imu_sample.accel[1], imu_sample.accel[2]) * 180.0 / M_PI;
		accel_error[1] += atan2f(-imu_sample.accel[0], imu_sample.accel[2]) * 180.0 / M_PI;
		accel_error[2] += atan2f(imu_sample.accel[1], imu_sample.accel[0]) * 180.0 / M_PI;
	}
	for (int i = 0;i < 3;i++) {
		gyro_error[i] /= calib_samples;
		accel_error[i] /= calib_samples;
	}

	time_now = imu_sample.timestamp;

	struct attitude_frame att_frame;

	/* make sure we have at least something for both imu and mag */
	k_msgq_get(imu_msgq, &imu_sample, K_FOREVER);
	k_msgq_get(mag_msgq, &mag_sample, K_FOREVER);

	while (1) {
		/* IMU samples much faster than the mag, so synchronize to
		 * IMU sample rate and reuse mag samples until new ones arrive */
		k_msgq_get(imu_msgq, &imu_sample, K_FOREVER);
		k_msgq_get(mag_msgq, &mag_sample, K_NO_WAIT);

		time_prev = time_now;
		time_now = imu_sample.timestamp;
		time_delta = ((double) (time_now - time_prev)) / 1000.0;

		/* apply gyro offset, convert to degrees, and calculate
		 * the difference in angle since previous sample */
		for (int i = 0;i < 3;i++) {
			imu_sample.gyro[i] -= gyro_error[i];
			imu_sample.gyro[i] *= 180.0 / M_PI;
			gyro_dangle[i] = imu_sample.gyro[i] * time_delta;
		}

		/* rotate gyro angle delta to body frame */
		gyro_danglep[0] = (irot[0][0]*gyro_dangle[0]) + (irot[0][1]*gyro_dangle[1])
			+ (irot[0][2]*gyro_dangle[2]);
		gyro_danglep[1] = (irot[1][0]*gyro_dangle[0]) + (irot[1][1]*gyro_dangle[1])
			+ (irot[1][2]*gyro_dangle[2]);
		gyro_danglep[2] = (irot[2][0]*gyro_dangle[0]) + (irot[2][1]*gyro_dangle[1])
			+ (irot[2][2]*gyro_dangle[2]);

		/* rotate accelerometer raw data to body frame */
		accel_rawp[0] = (irot[0][0]*imu_sample.accel[0]) + (irot[0][1]*imu_sample.accel[1])
			+ (irot[0][2]*imu_sample.accel[2]);
		accel_rawp[1] = (irot[1][0]*imu_sample.accel[0]) + (irot[1][1]*imu_sample.accel[1])
			+ (irot[1][2]*imu_sample.accel[2]);
		accel_rawp[2] = (irot[2][0]*imu_sample.accel[0]) + (irot[2][1]*imu_sample.accel[1])
			+ (irot[2][2]*imu_sample.accel[2]);

		/* calculate accelerometer gravity vector angle in degrees */
		accel_angle[0] = atan2f(accel_rawp[1], accel_rawp[2]) * 180.0 / M_PI;
		accel_angle[1] = atan2f(-accel_rawp[0], accel_rawp[2]) * 180.0 / M_PI;
		accel_angle[2] = atan2f(-accel_rawp[1], -accel_rawp[0]) * 180.0 / M_PI;
		/* and apply offset */
		/*accel_angle[0] -= accel_error[0];*/
		/*accel_angle[1] -= accel_error[1];*/
		/*accel_angle[2] -= accel_error[2];*/

		/* apply mag calibration */
		magn_scaled[0] = (mag_sample.magn[0] - MAG_OFFSET_X) * MAG_SCALE_X;
		magn_scaled[1] = (mag_sample.magn[1] - MAG_OFFSET_Y) * MAG_SCALE_Y;
		magn_scaled[2] = (mag_sample.magn[2] - MAG_OFFSET_Z) * MAG_SCALE_Z;

		/* rotate mag data to body frame */
		magn_rot[0] = (mrot[0][0]*magn_scaled[0]) + (mrot[0][1]*magn_scaled[1])
			+ (mrot[0][2]*magn_scaled[2]);
		magn_rot[1] = (mrot[1][0]*magn_scaled[0]) + (mrot[1][1]*magn_scaled[1])
			+ (mrot[1][2]*magn_scaled[2]);
		magn_rot[2] = (mrot[2][0]*magn_scaled[0]) + (mrot[2][1]*magn_scaled[1])
			+ (mrot[2][2]*magn_scaled[2]);

		/* compute heading */
		heading = atan2f(magn_rot[1], magn_rot[0]) * 180 / M_PI;

		angle[0] = (0.98 * (angle[0] + gyro_danglep[0])) + (0.02 * accel_angle[0]);
		angle[1] = (0.98 * (angle[1] + gyro_danglep[1])) + (0.02 * accel_angle[1]);
		angle[2] = gyro_danglep[2];

		att_frame.timestamp = imu_sample.timestamp;
		att_frame.angle[0] = angle[0];
		att_frame.angle[1] = angle[1];
		att_frame.angle[2] = heading;
		/*att_frame.angle[2] = angle[2];*/

		while (k_msgq_put(attitude_msgq, &att_frame, K_NO_WAIT) != 0) {
			LOG_ERR("Dropping attitude frames");
			k_msgq_purge(attitude_msgq);
		}
	}
}
