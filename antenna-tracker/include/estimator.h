#ifndef __ESTIMATOR_H
#define __ESTIMATOR_H

#include <zephyr.h>

struct attitude_frame {
	int64_t timestamp;
	float angle[3];
};

void est_init(struct k_msgq *imu_msgq, struct k_msgq * mag_msgq,
		struct k_msgq *attitude_msgq);

#endif /* __ESTIMATOR_H */
