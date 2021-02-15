/* quaternion.c: a couple of quaternion utilities */

#include <math.h>

#include "quaternion.h"

static const float M_PI = 3.1415926;

void quat_to_euler(float *q, float *e)
{
	float roll, pitch, yaw;

	/* roll  (y-axis rotation) */
	float sinr_cosp = 2 * (q[0] * q[1] + q[2] * q[3]);
	float cosr_cosp = 1 - 2 * (q[1] * q[1] + q[2] * q[2]);
	roll = atan2f(sinr_cosp, cosr_cosp);

	/* pitch (x-axis rotation) */
	float sinp = 2 * (q[0] * q[2] - q[3] * q[1]);
	if (fabsf(sinp) >= 1) {
		pitch = (sinp >= 0) ? M_PI / 2 : -M_PI / 2;
	} else {
		pitch = asinf(sinp);
	}

	/* yaw (z-axis rotation) */
	float siny_cosp = 2 * (q[0] * q[3] + q[1] * q[2]);
	float cosy_cosp = 1 - 2 * (q[2] * q[2] + q[3] * q[3]);
	yaw = atan2f(siny_cosp, cosy_cosp);

	e[0] = pitch * 180 / M_PI;
	e[1] = roll * 180 / M_PI;
	e[2] = yaw * 180 / M_PI;
}
