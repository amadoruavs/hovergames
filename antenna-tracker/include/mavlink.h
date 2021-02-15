#ifndef __APS_MAVLINK_H
#define __APS_MAVLINK_H

#include <zephyr.h>
#include <device.h>
#include <sys/ring_buffer.h>

struct parse_mavlink_data {
	struct ring_buf *rx_ringbuf;
	struct ring_buf *tx_ringbuf;
	struct k_work work;
};

void init_mavlink(const struct device *usb_dev,
		struct ring_buf *rx_ringbuf, struct ring_buf *tx_ringbuf);
void parse_mavlink(struct k_work *item);
void mavlink_timer_start();
void mavlink_timer_stop();

#endif /* __APS_MAVLINK_H */
