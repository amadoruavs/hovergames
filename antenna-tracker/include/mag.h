#ifndef __MAG_H
#define __MAG_H

static const float MAG_OFFSET_X = -0.282569;
static const float MAG_OFFSET_Y = -0.363303;
static const float MAG_OFFSET_Z = -0.325688;
static const float MAG_SCALE_X  =  1.125176;
static const float MAG_SCALE_Y  =  0.976801;
static const float MAG_SCALE_Z  =  0.919540;

struct mag_sample {
	int64_t timestamp;
	double magn[3];
};

void init_mag(struct k_msgq *imu_msgq, struct k_msgq *attitude_msgq);
int process_mag(const struct device *dev, struct mag_sample *mag_sample);

#ifndef CONFIG_HMC5883L_TRIGGER
int setup_hmc5883l_trigger(const struct device *dev, struct k_msgq *mag_msgq);
#endif

#ifndef CONFIG_HMC5883L_TRIGGER
extern void mag_poll_thread_entry(void *, void *, void *);
#endif

#endif /* __MAG_H */
