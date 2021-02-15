#include <zephyr.h>
#include <device.h>
#include <drivers/uart.h>
#include <sys/ring_buffer.h>
#include <logging/log.h>

#include "mavlink/common/mavlink.h"
#include "mavlink/mavlink_helpers.h"

#include "quaternion.h"
#include "mavlink.h"

LOG_MODULE_REGISTER(mavlink, LOG_LEVEL_DBG);

int chan = MAVLINK_COMM_0;
mavlink_status_t rstatus;
mavlink_message_t rmsg;

uint8_t aps_sys_id = 220;
uint8_t aps_comp_id = MAV_COMP_ID_GIMBAL;

uint8_t sysid_primary_control;
uint8_t compid_primary_control;
uint8_t sysid_secondary_control;
uint8_t compid_secondary_control;

#define MAVLINK_BUFFER_LEN 2041
uint8_t send_buffer[MAVLINK_BUFFER_LEN];

struct k_timer tim_heartbeat;
struct k_work heartbeat_work;
struct k_timer tim_gimbal_status;
struct k_work gimbal_status_work;
struct k_timer tim_attitude_status;
struct k_work attitude_status_work;
struct k_work gimbal_manager_info_work;

float gimbal_quat[4];
float gimbal_angular_vel_x;
float gimbal_angular_vel_y;
float gimbal_angular_vel_z;
uint32_t gimbal_failures = 0;

struct timer_callback_data {
	struct device *usb_dev;
	struct ring_buf *rx_ringbuf;
	struct ring_buf *tx_ringbuf;
};
struct timer_callback_data timer_callback_data;

struct command_ack_data {
	int16_t command;
	uint8_t result;
	uint8_t target_sys;
	uint8_t target_comp;
	struct k_work work;
};

void queue_message(mavlink_message_t *msg)
{
	struct ring_buf *tx_ringbuf = timer_callback_data.tx_ringbuf;
	struct device *usb_dev = timer_callback_data.usb_dev;

	uint16_t msg_len = mavlink_msg_to_send_buffer(send_buffer, msg);
	size_t rb_len = MIN(ring_buf_space_get(tx_ringbuf), msg_len);

	int put_len = ring_buf_put(tx_ringbuf, send_buffer, rb_len);
	if (put_len < msg_len) {
		LOG_ERR("Dropping %u bytes", msg_len - put_len);
	}

	uart_irq_tx_enable(usb_dev);
}

void send_heartbeat(struct k_work *item)
{
	mavlink_message_t msg;

	mavlink_msg_heartbeat_pack(
			aps_sys_id, aps_comp_id,
			&msg,
			MAV_TYPE_ANTENNA_TRACKER,
			MAV_AUTOPILOT_INVALID,
			MAV_MODE_FLAG_SAFETY_ARMED, 0, MAV_STATE_ACTIVE);
	queue_message(&msg);
}

void send_gimbal_manager_info(struct k_work *item) {
	mavlink_message_t msg;
	mavlink_msg_gimbal_manager_information_pack(
			aps_sys_id, aps_comp_id,
			&msg, k_uptime_get(),
			GIMBAL_MANAGER_CAP_FLAGS_HAS_PITCH_AXIS |
			GIMBAL_MANAGER_CAP_FLAGS_HAS_YAW_AXIS |
			GIMBAL_MANAGER_CAP_FLAGS_HAS_PITCH_LOCK |
			GIMBAL_MANAGER_CAP_FLAGS_HAS_YAW_LOCK |
			GIMBAL_MANAGER_CAP_FLAGS_SUPPORTS_INFINITE_YAW,
			aps_comp_id,
			0, 0,
			0, 1.57,
			0.0/0.0, 0.0/0.0);
	queue_message(&msg);
}

void send_gimbal_manager_status(struct k_work *item)
{
	mavlink_message_t msg;
	mavlink_msg_gimbal_manager_status_pack(
			aps_sys_id, aps_comp_id,
			&msg, k_uptime_get(),
			aps_comp_id,
			GIMBAL_MANAGER_FLAGS_YAW_LOCK |
			GIMBAL_MANAGER_FLAGS_PITCH_LOCK,
			0, 0, 0, 0);
	queue_message(&msg);
}

void send_gimbal_device_attitude_status(struct k_work *item)
{
	mavlink_message_t msg;
	mavlink_msg_gimbal_device_attitude_status_pack(
			aps_sys_id, aps_comp_id,
			&msg,
			0, 0,
			k_uptime_get(),
			GIMBAL_DEVICE_FLAGS_YAW_LOCK |
			GIMBAL_DEVICE_FLAGS_PITCH_LOCK,
			gimbal_quat,
			gimbal_angular_vel_x,
			gimbal_angular_vel_y,
			gimbal_angular_vel_z, gimbal_failures);
	queue_message(&msg);
}

void send_ack(struct k_work *item)
{
	struct command_ack_data *data = CONTAINER_OF(item, struct command_ack_data, work);

	mavlink_message_t msg;
	mavlink_msg_command_ack_pack(
			aps_sys_id, aps_comp_id,
			&msg, data->command, data->result, 0, 0, data->target_sys, data->target_comp);
	queue_message(&msg);

	k_free(data);
}

static void process_request_message(mavlink_command_long_t *command)
{
	if (command->param1 == MAVLINK_MSG_ID_GIMBAL_MANAGER_INFORMATION) {
		k_work_submit(&gimbal_manager_info_work);
	}
}

static int process_command(mavlink_command_long_t *command)
{
	switch (command->command) {
	case MAV_CMD_REQUEST_MESSAGE:
		process_request_message(command);
		return -1;
	case MAV_CMD_DO_GIMBAL_MANAGER_CONFIGURE:
		sysid_primary_control = command->param1;
		compid_primary_control = command->param2;
		sysid_secondary_control = command->param3;
		compid_secondary_control = command->param4;

		return MAV_RESULT_ACCEPTED;
	default:
		return MAV_RESULT_UNSUPPORTED;
	}
}

static void submit_ack(mavlink_message_t *msg, mavlink_command_long_t *command, 
		int result)
{
	struct command_ack_data *data = k_calloc(1, sizeof(struct command_ack_data));
	data->command = command->command;
	data->result = result;
	data->target_sys = msg->sysid;
	data->target_comp = msg->compid;
	k_work_init(&(data->work), send_ack);
	k_work_submit(&(data->work));
}

static void process_set_attitude(mavlink_message_t *msg)
{
	mavlink_gimbal_manager_set_attitude_t mavlink_setpoint;
	mavlink_msg_gimbal_manager_set_attitude_decode(msg, &mavlink_setpoint);

	printf("set_attitude quat (%f, %f, %f, %f)\n",
			mavlink_setpoint.q[0], mavlink_setpoint.q[1], 
			mavlink_setpoint.q[2], mavlink_setpoint.q[3]);
	float euler[3];
	quat_to_euler(mavlink_setpoint.q, euler);

	printf("set_attitude euler (%f %f %f)\n", euler[0], euler[1], euler[2]);
}

static void process_message(mavlink_message_t *msg)
{
	mavlink_command_long_t command;

	switch (msg->msgid) {
	case MAVLINK_MSG_ID_COMMAND_LONG:
		mavlink_msg_command_long_decode(msg, &command);
		int result = process_command(&command);
		if (result >= 0) {
			submit_ack(msg, &command, result);
		}
		break;
	case MAVLINK_MSG_ID_GIMBAL_MANAGER_SET_ATTITUDE:
		process_set_attitude(msg);

		break;
	}
}

void parse_mavlink(struct k_work *item)
{
	struct parse_mavlink_data *data =
		CONTAINER_OF(item, struct parse_mavlink_data, work);
	struct ring_buf *rx_ringbuf = data->rx_ringbuf;

	uint8_t buffer[64];
	int recv_len;

	recv_len = ring_buf_get(rx_ringbuf, buffer, sizeof(buffer));
	if (!recv_len) {
		return;
	}

	for (int i = 0;i < recv_len; i++) {
		if (mavlink_parse_char(chan, buffer[i], &rmsg, &rstatus)) {
			/* decode message payload here */
			process_message(&rmsg);
		}
	}
}

void tim_heartbeat_callback(struct k_timer *timer_id)
{
	k_work_submit(&heartbeat_work);
}

void tim_gimbal_status_callback(struct k_timer *timer_id)
{
	k_work_submit(&gimbal_status_work);
}

void tim_attitude_status_callback(struct k_timer *timer_id)
{
	k_work_submit(&attitude_status_work);
}

void init_mavlink(const struct device *usb_dev,
		struct ring_buf *rx_ringbuf, struct ring_buf *tx_ringbuf)
{
	timer_callback_data.usb_dev = usb_dev;
	timer_callback_data.rx_ringbuf = rx_ringbuf;
	timer_callback_data.tx_ringbuf = tx_ringbuf;

	k_timer_init(&tim_heartbeat, tim_heartbeat_callback, NULL);
	k_timer_init(&tim_gimbal_status, tim_gimbal_status_callback, NULL);
	k_timer_init(&tim_attitude_status, tim_attitude_status_callback, NULL);

	k_work_init(&heartbeat_work, send_heartbeat);
	k_work_init(&gimbal_status_work, send_gimbal_manager_status);
	k_work_init(&attitude_status_work, send_gimbal_device_attitude_status);
	k_work_init(&gimbal_manager_info_work, send_gimbal_manager_info);
}

void mavlink_timer_start()
{
	k_timer_start(&tim_heartbeat, K_MSEC(1000), K_MSEC(1000)); /* 1Hz */
	k_timer_start(&tim_gimbal_status, K_MSEC(200), K_MSEC(200)); /* 5Hz */
	k_timer_start(&tim_attitude_status, K_MSEC(100), K_MSEC(100)); /* 10Hz */
}

void mavlink_timer_stop()
{
	k_timer_stop(&tim_heartbeat);
	k_timer_stop(&tim_gimbal_status);
	k_timer_stop(&tim_attitude_status);
}
