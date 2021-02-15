#ifndef USB_H
#define USB_H

#include <zephyr.h>
#include <device.h>
#include <sys/ring_buffer.h>

void usb_interrupt_handler(const struct device *dev, void *user_data);

const struct device *usb_init(char *device_label,
		struct ring_buf *rx_ringbuf, struct ring_buf *tx_ringbuf);

#endif /* USB_H */
