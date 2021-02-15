#ifndef BOARD_H
#define BOARD_H

#include <zephyr.h>
#include <device.h>

#define IMU_NODE DT_ALIAS(imu)
#if DT_NODE_HAS_STATUS(IMU_NODE, okay)
#define IMU_LABEL DT_LABEL(IMU_NODE)
#else
#error "Unsupported board."
#endif

#define MAG_NODE DT_ALIAS(mag)
#if DT_NODE_HAS_STATUS(MAG_NODE, okay)
#define MAG_LABEL DT_LABEL(MAG_NODE)
#else
#error "Unsupported board."
#endif

#define ALT_NODE DT_ALIAS(alt)
#if DT_NODE_HAS_STATUS(ALT_NODE, okay)
#define ALT_LABEL DT_PWMS_LABEL(ALT_NODE)
#define ALT_CHANNEL DT_PWMS_CHANNEL(ALT_NODE)
#define ALT_PERIOD DT_PWMS_PERIOD(ALT_NODE)
#define ALT_FLAGS DT_PWMS_FLAGS(ALT_NODE)
#else
#error "Unsupported board."
#endif

#define AZM_NODE DT_ALIAS(azm)
#if DT_NODE_HAS_STATUS(AZM_NODE, okay)
#define AZM_LABEL DT_PWMS_LABEL(AZM_NODE)
#define AZM_CHANNEL DT_PWMS_CHANNEL(AZM_NODE)
#define AZM_PERIOD DT_PWMS_PERIOD(AZM_NODE)
#define AZM_FLAGS DT_PWMS_FLAGS(AZM_NODE)
#else
#error "Unsupported board."
#endif

#endif /* BOARD_H */
