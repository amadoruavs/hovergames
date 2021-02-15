#ifndef IMU_H
#define IMU_H

#include <zephyr.h>
#include <device.h>

struct imu_sample {
	int64_t timestamp;
	double accel[3];
	double gyro[3];
	double temp;
};

void init_imu(struct k_msgq *imu_msgq, struct k_msgq *attitude_msgq);
int process_imu(const struct device *dev, struct imu_sample *imu_sample);

#ifdef CONFIG_MPU6050_TRIGGER
int setup_mpu6050_trigger(const struct device *dev);
#endif

#ifndef CONFIG_MPU6050_TRIGGER
extern void imu_poll_thread_entry(void *, void *, void *);
#endif

#endif /* IMU_H */
