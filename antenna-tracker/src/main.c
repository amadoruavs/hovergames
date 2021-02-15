#include <stdio.h>
#include <zephyr.h>
#include <logging/log.h>

#include "board.h"
#include "imu.h"
#include "mag.h"
#include "estimator.h"
#include "pwmctrl.h"
#include "attctrl.h"
#include "usb.h"
#include "mavlink.h"

LOG_MODULE_REGISTER(antenna_tracker, LOG_LEVEL_DBG);

#ifndef CONFIG_MPU6050_TRIGGER
#define IMU_POLL_STACK_SIZE 500
#define IMU_POLL_PRIORITY 0
K_THREAD_STACK_DEFINE(imu_poll_stack_area, IMU_POLL_STACK_SIZE);
struct k_thread imu_poll_thread_data;
k_tid_t imu_poll_tid;
#endif /* !CONFIG_MPU6050_TRIGGER */

#ifndef CONFIG_HMC5883L_TRIGGER
#define MAG_POLL_STACK_SIZE 500
#define MAG_POLL_PRIORITY 0
K_THREAD_STACK_DEFINE(mag_poll_stack_area, MAG_POLL_STACK_SIZE);
struct k_thread mag_poll_thread_data;
k_tid_t mag_poll_tid;
#endif /* !CONFIG_HMC5883L_TRIGGER */

K_MSGQ_DEFINE(pwmctrl_msgq, sizeof(struct motor_setpoint), 4, 16);
K_MSGQ_DEFINE(imu_msgq, sizeof(struct imu_sample), 4, 16);
K_MSGQ_DEFINE(mag_msgq, sizeof(struct mag_sample), 4, 16);
K_MSGQ_DEFINE(attitude_msgq, sizeof(struct attitude_frame), 4, 16);
K_MSGQ_DEFINE(command_msgq, sizeof(struct command_setpoint), 2, 32);

#define RING_BUF_SIZE 2048
uint8_t rx_ring_buffer[RING_BUF_SIZE];
struct ring_buf rx_ringbuf;
uint8_t tx_ring_buffer[RING_BUF_SIZE];
struct ring_buf tx_ringbuf;

void main(void)
{
	/* IMU setup */
	const struct device *mpu6050 = device_get_binding(IMU_LABEL);
	if (!mpu6050) {
		LOG_ERR("Failed to initialize device %s\n", IMU_LABEL);
		return;
	}

#ifdef CONFIG_MPU6050_TRIGGER
	int ret;
	ret = setup_mpu6050_trigger(mpu6050);
	if (ret != 0) {
		LOG_ERR("Unable to configure MPU6050 trigger: %d", ret);
		return;
	}
#endif

#ifndef CONFIG_MPU6050_TRIGGER
	imu_poll_tid = k_thread_create(&imu_poll_thread_data, imu_poll_stack_area,
			K_THREAD_STACK_SIZEOF(imu_poll_stack_area),
			imu_poll_thread_entry,
			(void *) mpu6050, (void *) &imu_msgq, NULL,
			IMU_POLL_PRIORITY, 0, K_NO_WAIT);
#endif

	/* mag setup */
	const struct device *hmc5883l = device_get_binding(MAG_LABEL);
	if (!hmc5883l) {
		LOG_ERR("Failed to initialize device %s\n", MAG_LABEL);
		return;
	}

#ifdef CONFIG_HMC5883L_TRIGGER
	int ret;
	ret = setup_hmc5883l_trigger(hmc5883l, &mag_msgq);
	if (ret != 0) {
		LOG_ERR("Unable to configure HMC5883L trigger: %d", ret);
		return;
	}
#endif

#ifndef CONFIG_HMC5883L_TRIGGER
	mag_poll_tid = k_thread_create(&mag_poll_thread_data, mag_poll_stack_area,
			K_THREAD_STACK_SIZEOF(mag_poll_stack_area),
			mag_poll_thread_entry,
			(void *) hmc5883l, (void *) &mag_msgq, NULL,
			MAG_POLL_PRIORITY, 0, K_NO_WAIT);
#endif

	/* initialize the attitude estimator */
	est_init(&imu_msgq, &mag_msgq, &attitude_msgq);

	pwmctrl_device_init(&pwmctrl_msgq);
	attctrl_init(&attitude_msgq, &pwmctrl_msgq, &command_msgq);

	const struct device *usb_dev = usb_init("CDC_ACM_0", &rx_ringbuf, &tx_ringbuf);
	if (!usb_dev) {
		LOG_ERR("Failed to initialize device %s", "CDC_ACM_0");
		return;
	}

	ring_buf_init(&rx_ringbuf, sizeof(rx_ring_buffer), rx_ring_buffer);
	ring_buf_init(&tx_ringbuf, sizeof(tx_ring_buffer), tx_ring_buffer);

	init_mavlink(usb_dev, &rx_ringbuf, &tx_ringbuf);

	struct command_setpoint setpoint = {
		.type = COMMAND_SETPOINT_TYPE_EULER,
		.data.euler[0] = 0,
		.data.euler[1] = 0,
		.data.euler[2] = 0,
	};

	while (1) {
		setpoint.data.euler[0] += 15;
		if (setpoint.data.euler[0] > 60) {
			setpoint.data.euler[0] = 0;
		}

		while (k_msgq_put(&command_msgq, &setpoint, K_NO_WAIT) != 0) {
			LOG_ERR("Dropping setpoint frames");
			k_msgq_purge(&command_msgq);
		}
		
		k_sleep(K_MSEC(2000));
	}
}
