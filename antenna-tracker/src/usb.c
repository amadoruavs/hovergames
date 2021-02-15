/** usb.c
 *
 * This file contains code for interfacing with the ground computer
 * via a USB CDC ACM connection.
 *
 * @author Vincent Wang, Kalyan Sriram
 */

#include <stdio.h>
#include <zephyr.h>
#include <device.h>
#include <logging/log.h>
#include <drivers/uart.h>
#include <usb/usb_device.h>
#include <sys/ring_buffer.h>

#include "mavlink.h"
#include "usb.h"

LOG_MODULE_REGISTER(cdcacm, LOG_LEVEL_DBG);

#define USB_STACK_SIZE 2000
#define USB_PRIORITY 1

extern void usb_thread_entry(void *, void *, void *);

K_THREAD_STACK_DEFINE(usb_stack_area, USB_STACK_SIZE);
struct k_thread usb_thread_data;

struct usb_callback_data {
	struct device *usb_dev;
	struct ring_buf *rx_ringbuf;
	struct ring_buf *tx_ringbuf;
};

struct usb_callback_data usb_callback_data;

struct parse_mavlink_data parse_mavlink_data;

const struct device *usb_init(char *device_label, 
		struct ring_buf *rx_ringbuf, struct ring_buf *tx_ringbuf)
{
	LOG_DBG("Initializing USB");

	const struct device *dev;
	int ret;

	dev = device_get_binding(device_label);
	if (!dev) {
		LOG_ERR("CDC ACM device not found");
		return NULL;
	}

	ret = usb_enable(NULL);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return NULL;
	}

	LOG_DBG("Initialized USB");

	k_work_init(&(parse_mavlink_data.work), parse_mavlink);
	parse_mavlink_data.rx_ringbuf = rx_ringbuf;
	parse_mavlink_data.tx_ringbuf = tx_ringbuf;

	k_tid_t usb_tid = k_thread_create(&usb_thread_data, usb_stack_area,
									  K_THREAD_STACK_SIZEOF(usb_stack_area),
									  usb_thread_entry,
									  (void *) dev, (void *) rx_ringbuf, (void *) tx_ringbuf,
									  USB_PRIORITY, 0, K_NO_WAIT);

	return dev;
}

void usb_interrupt_handler(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	struct ring_buf *rx_ringbuf = usb_callback_data.rx_ringbuf;
	struct ring_buf *tx_ringbuf = usb_callback_data.tx_ringbuf;

	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (uart_irq_rx_ready(dev)) {
			int recv_len, rb_len;
			uint8_t buffer[64];
			size_t len = MIN(ring_buf_space_get(rx_ringbuf),
					sizeof(buffer));

			recv_len = uart_fifo_read(dev, buffer, len);

			rb_len = ring_buf_put(rx_ringbuf, buffer, recv_len);
			if (rb_len < recv_len) {
				LOG_ERR("Dropping %u bytes", recv_len - rb_len);
			}

			k_work_submit(&(parse_mavlink_data.work));
		}

		if (uart_irq_tx_ready(dev)) {
			uint8_t buffer[64];
			int rb_len, send_len;

			rb_len = ring_buf_get(tx_ringbuf, buffer, sizeof(buffer));
			if (!rb_len) {
				uart_irq_tx_disable(dev);
				continue;
			}

			send_len = uart_fifo_fill(dev, buffer, rb_len);
			if (send_len < rb_len) {
				LOG_ERR("Dropping %d bytes", rb_len - send_len);
			}
		}
	}
}

void usb_connect(struct device *usb_dev)
{
	int ret;
	int baudrate;

	/* test the interrupt endpoint */
	ret = uart_line_ctrl_set(usb_dev, UART_LINE_CTRL_DCD, 1);
	if (ret) {
		LOG_WRN("Failed to set DCD: %d", ret);
	}

	ret = uart_line_ctrl_set(usb_dev, UART_LINE_CTRL_DSR, 1);
	if (ret) {
		LOG_WRN("Failed to set DSR: %d", ret);
	}

	/* wait 1 sec for the host to setup */
	k_sleep(K_MSEC(1000));

	ret = uart_line_ctrl_get(usb_dev, UART_LINE_CTRL_BAUD_RATE, &baudrate);
	if (ret) {
		LOG_WRN("Failed to get baudrate: %d", ret);
	} else {
		LOG_INF("Baudrate detected: %d", baudrate);
	}

	/* setup rx interrupts */
	uart_irq_callback_set(usb_dev, usb_interrupt_handler);
	uart_irq_rx_enable(usb_dev);

	mavlink_timer_start();
}

void usb_disconnect(struct device *usb_dev)
{
	/* disable interrupts */
	uart_irq_rx_disable(usb_dev);
	uart_irq_tx_disable(usb_dev);
	mavlink_timer_stop();
}

/* monitors the USB connection */
void usb_thread_entry(void *arg1, void *arg2, void *arg3)
{
	LOG_DBG("Initializing USB monitoring thread");

	struct device *usb_dev = (struct device *) arg1;
	struct ring_buf *rx_ringbuf = (struct ring_buf *) arg2;
	struct ring_buf *tx_ringbuf = (struct ring_buf *) arg3;

	usb_callback_data.usb_dev = usb_dev;
	usb_callback_data.rx_ringbuf = rx_ringbuf;
	usb_callback_data.tx_ringbuf = tx_ringbuf;

	bool connected = false;
	int dtr = 0U;

	while (1) {
		uart_line_ctrl_get(usb_dev, UART_LINE_CTRL_DTR, &dtr);
		if (dtr && !connected) {
			LOG_INF("USB connected");
			connected = true;
			usb_connect(usb_dev);
		} else if (!dtr && connected) {
			LOG_INF("USB disconnected");
			connected = false;
			usb_disconnect(usb_dev);
		}

		k_sleep(K_MSEC(100)); /* 10Hz poll */
	}
}
