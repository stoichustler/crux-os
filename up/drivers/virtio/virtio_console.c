// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018, Tuomas Tynkkynen <tuomas.tynkkynen@iki.fi>
 * Copyright (C) 2018, Bin Meng <bmeng.cn@gmail.com>
 * Copyright (C) 2006, 2007, 2009 Rusty Russell, IBM Corporation
 * Copyright (C) 2009, 2010, 2011 Red Hat, Inc.
 * Copyright (C) 2009, 2010, 2011 Amit Shah <amit.shah@redhat.com>
 * Copyright (C) 2021 Ahmad Fatoum
 * Copyright (C) 2021, Google LLC, schuffelen@google.com (A. Cody Schuffelen)
 */

#include <blk.h>
#include <common.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <dm/devres.h>
#include <part.h>
#include <serial.h>
#include <virtio_types.h>
#include <virtio.h>
#include <virtio_ring.h>
#include "dm/device.h"
#include "linux/compat.h"
#include "virtio_console.h"

#define CONTROL_BUFFER_SIZE 64
#define CONTROL_QUEUE_SIZE  32

static const u32 features[] = {
	VIRTIO_CONSOLE_F_MULTIPORT,
};

struct virtio_console_priv;

// Both the top-level and every child port device contain one of these. Because
// a `struct virtio_console_port_priv` is the first member of `struct
// virtio_console_priv`, it is safe to use a `struct virtio_console_priv` as a
// `struct virtio_console_port_priv` in the `virtio_console_port` methods that
// are shared betewen the two devices to implement `dm_serial_ops`.
struct virtio_console_port_priv {
	struct virtio_console_priv *console_priv;
	struct virtqueue *receiveq;
	struct virtqueue *transmitq;
	int port_num;
	unsigned char char_inbuf[1] __aligned(sizeof(void *));
	bool buffer_queued;
};

// Private data struct for the top-level virtio-console udevice.
struct virtio_console_priv {
	struct virtio_console_port_priv port0;
	struct virtqueue *receiveq_control;
	struct virtqueue *transmitq_control;
	char control_buffers[CONTROL_QUEUE_SIZE][CONTROL_BUFFER_SIZE];
};

static int virtqueue_blocking_send(struct virtqueue *queue, void *data,
				   size_t size)
{
	struct virtio_sg sg = {
		.addr = data,
		.length = size,
	};

	struct virtio_sg *sgs[] = {&sg};
	int ret = virtqueue_add(queue, sgs, 1, 0);

	if (ret)
		return log_msg_ret("failed to add buffer", ret);

	virtqueue_kick(queue);

	while (!virtqueue_get_buf(queue, NULL))
		;

	return 0;
}

static int virtio_console_send_control_message(struct virtio_console_priv *priv,
					       u32 id, u16 event, u16 value)
{
	struct virtio_console_control message = {
		.id = id,
		.event = event,
		.value = value,
	};
	return virtqueue_blocking_send(priv->transmitq_control, &message, sizeof(message));
}

static int fill_control_inbuf(struct virtio_console_priv *priv)
{
	// The QEMU host implementation of this will drop control messages if
	// there's no space in the guest. Preemptively try to keep the control
	// queue well-provisioned with messages that can store either a control
	// message or a name (from VIRTIO_CONSOLE_PORT_NAME).
	struct virtio_sg sgs[ARRAY_SIZE(priv->control_buffers)];
	struct virtio_sg *sg_addrs[ARRAY_SIZE(sgs)];
	int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(sgs); i++) {
		sgs[i].addr = priv->control_buffers[i];
		sgs[i].length = CONTROL_BUFFER_SIZE;
		sg_addrs[i] = &sgs[i];
	}

	ret = virtqueue_add(priv->receiveq_control, sg_addrs, 0, ARRAY_SIZE(sgs));

	if (ret)
		return log_msg_ret("virtqueue_add failed", ret);

	virtqueue_kick(priv->receiveq_control);

	return 0;
}

static int return_control_buffer(struct virtio_console_priv *priv, void *data)
{
	struct virtio_sg sg = {
		.addr = data,
		.length = CONTROL_BUFFER_SIZE,
	};
	struct virtio_sg *sgs[1] = { &sg };
	int ret = virtqueue_add(priv->receiveq_control, sgs, 0, 1);

	if (ret)
		return log_msg_ret("Adding control receive buffer", ret);

	virtqueue_kick(priv->receiveq_control);

	return 0;
}

static int virtio_console_control_messsage_pending(struct virtio_console_priv *priv)
{
	return virtqueue_poll(priv->receiveq_control, priv->receiveq_control->last_used_idx);
}

static int virtio_console_process_control_message(struct virtio_console_priv *priv)
{
	unsigned int len = 0;
	struct virtio_console_control *control_ptr;
	struct virtio_console_control control;
	int ret;

	if (!virtio_console_control_messsage_pending(priv))
		return 0;  // Nothing to process

	control_ptr = virtqueue_get_buf(priv->receiveq_control, &len);

	if (!control_ptr)
		return log_msg_ret("No buffers", -EINVAL);
	else if (len != sizeof(*control_ptr))
		return log_msg_ret("Unexpected buffer size", -EINVAL);

	control = *control_ptr;
	ret = return_control_buffer(priv, control_ptr);

	if (ret)
		log_msg_ret("returning control buffer", ret);

	switch (control.event) {
	case VIRTIO_CONSOLE_PORT_ADD:
		ret = virtio_console_send_control_message(priv, control.id,
							  VIRTIO_CONSOLE_PORT_READY, 1);

		if (ret)
			return log_msg_ret("sending port ready message", ret);
		return 0;
	case VIRTIO_CONSOLE_CONSOLE_PORT:
		ret = virtio_console_send_control_message(priv, control.id,
							  VIRTIO_CONSOLE_PORT_OPEN, 1);

		if (ret)
			return log_msg_ret("sending port open message", ret);
		return 0;
	case VIRTIO_CONSOLE_PORT_REMOVE:
	case VIRTIO_CONSOLE_RESIZE:
	case VIRTIO_CONSOLE_PORT_OPEN:
		return 0;
	case VIRTIO_CONSOLE_PORT_NAME: {
		// Since this command is always followed by the name, we have to
		// read the name to avoid interpreting it as another control
		// command.
		while (!virtio_console_control_messsage_pending(priv))
			;
		unsigned int len = 0;
		void *buf = virtqueue_get_buf(priv->receiveq_control, &len);

		if (!buf)
			return log_msg_ret("expected port name string", -EINVAL);

		ret = return_control_buffer(priv, buf);
		if (ret)
			return log_msg_ret("returning name buffer", ret);
		return 0;
	}
	default:
		return log_msg_ret("unexpected control message event", -EINVAL);
	}
}

static int virtio_console_exhaust_control_queue(struct virtio_console_priv *priv)
{
	if (!priv->receiveq_control || !priv->transmitq_control)
		return 0;

	while (virtio_console_control_messsage_pending(priv)) {
		int ret = virtio_console_process_control_message(priv);

		if (ret)
			return log_ret(ret);
	}
	return 0;
}

/*
 * Create a scatter-gather list representing our input buffer and put
 * it in the queue.
 */
static int add_char_inbuf(struct virtio_console_port_priv *priv)
{
	struct virtio_sg sg = {
		.addr = priv->char_inbuf,
		.length = sizeof(priv->char_inbuf),
	};
	struct virtio_sg *sgs[] = {&sg};

	int ret = virtqueue_add(priv->receiveq, sgs, 0, 1);

	if (ret)
		return log_msg_ret("Failed to add to virtqueue", ret);

	virtqueue_kick(priv->receiveq);

	return 0;
}

static int virtio_console_port_probe(struct udevice *dev)
{
	return 0;
}

static int virtio_console_port_post_probe(struct virtio_console_port_priv *priv)
{
	int ret;

	// QEMU will accept output on ports at any time, but will not pass
	// through input until it receives a VIRTIO_CONSOLE_PORT_OPEN on that
	// port number. It doesn't seem to produce a VIRTIO_CONSOLE_DEVICE_ADD
	// for each port it already has on startup, so here we  pre-emptively
	// `OPEN` every port when we probe.
	ret = virtio_console_send_control_message(priv->console_priv, priv->port_num,
						  VIRTIO_CONSOLE_PORT_OPEN, 1);

	return log_msg_ret("failed to send port open message", ret);
}

static int virtio_console_serial_setbrg(struct udevice *dev, int baudrate)
{
	return 0;
}

static int virtio_console_serial_pending(struct udevice *dev, bool input)
{
	struct virtio_console_port_priv *priv = dev_get_priv(dev);

	return virtqueue_poll(priv->receiveq,
			      priv->receiveq->last_used_idx);
}

static int virtio_console_port_serial_getc(struct udevice *dev)
{
	struct virtio_console_port_priv *priv = dev_get_priv(dev);
	unsigned int len = 0;
	unsigned char *in;
	int ret = virtio_console_exhaust_control_queue(priv->console_priv);

	if (ret)
		return ret;

	if (!priv->buffer_queued) {
		ret = add_char_inbuf(priv);
		if (ret)
			return log_msg_ret("Failed to set up character buffer", ret);
		priv->buffer_queued = true;
	}

	in = virtqueue_get_buf(priv->receiveq, &len);

	if (!in)
		return -EAGAIN;
	else if (len != 1)
		log_err("%s: too much data: %d\n", __func__, len);

	priv->buffer_queued = false;

	int ch = *in;

	return ch;
}

static int virtio_console_port_serial_putc(struct udevice *dev, const char ch)
{
	struct virtio_console_port_priv *priv = dev_get_priv(dev);
	int ret = virtio_console_exhaust_control_queue(priv->console_priv);

	if (ret)
		return ret;

	return log_ret(virtqueue_blocking_send(priv->transmitq, (void *)&ch, 1));
}

static ssize_t virtio_console_port_serial_puts(struct udevice *dev, const char *s, size_t len)
{
	struct virtio_console_port_priv *priv = dev_get_priv(dev);
	int ret = virtio_console_exhaust_control_queue(priv->console_priv);

	if (ret)
		return ret;

	return log_ret(virtqueue_blocking_send(priv->transmitq, (void *)s, len));
}

static const struct dm_serial_ops virtio_console_port_serial_ops = {
	.putc = virtio_console_port_serial_putc,
	.puts = virtio_console_port_serial_puts,
	.pending = virtio_console_serial_pending,
	.getc = virtio_console_port_serial_getc,
	.setbrg = virtio_console_serial_setbrg,
};

static int virtio_console_bind(struct udevice *dev)
{
	struct virtio_dev_priv *uc_priv = dev_get_uclass_priv(dev->parent);

	/* Indicate what driver features we support */
	virtio_driver_features_init(uc_priv, features, ARRAY_SIZE(features),
				    NULL, 0);

	return 0;
}

U_BOOT_DRIVER(virtio_console_port) = {
	.name	= VIRTIO_CONSOLE_PORT_DRV_NAME,
	.id	= UCLASS_SERIAL,
	.ops	= &virtio_console_port_serial_ops,
	.priv_auto	= sizeof(struct virtio_console_port_priv),
	.probe	= virtio_console_port_probe,
	.flags	= DM_FLAG_ACTIVE_DMA,
};

static int virtio_console_create_port(struct udevice *dev,
				      struct virtqueue **queues,
				      int port_num)
{
	struct virtio_console_port_priv *priv;
	struct udevice *created_dev = NULL;
	int ret = 0;

	ret = device_bind(dev, DM_DRIVER_REF(virtio_console_port),
			  "virtio_console_port", NULL, ofnode_null(),
			  &created_dev);
	if (ret)
		return log_msg_ret("Can't create port device", ret);

	ret = device_probe(created_dev);
	if (ret)
		return log_msg_ret("Failed to probe device", ret);

	// `priv` is only allocated by `device_probe`
	priv = dev_get_priv(created_dev);
	*priv = (struct virtio_console_port_priv) {
		.console_priv = dev_get_priv(dev),
		.receiveq = queues[(port_num * 2) + 2],
		.transmitq = queues[(port_num * 2) + 3],
		.port_num = port_num,
		.buffer_queued = false,
	};

	return log_ret(virtio_console_port_post_probe(priv));
}

static int virtio_console_probe(struct udevice *dev)
{
	struct virtio_console_priv *priv = dev_get_priv(dev);
	int is_multiport = 0;
	u32 max_ports = 1;
	int num_queues = 2;
	int ret = 0;
	struct virtqueue *virtqueues[64]; // max size

	if (virtio_has_feature(dev, VIRTIO_CONSOLE_F_MULTIPORT)) {
		virtio_cread(dev, struct virtio_console_config, max_nr_ports,
			     &max_ports);
		is_multiport = 1;
		num_queues = (1 + max_ports) * 2; // In/out per port + control
	}
	if (num_queues > 64)
		return log_msg_ret("Too many queues", -ENOMEM);

	ret = virtio_find_vqs(dev, num_queues, virtqueues);

	if (ret)
		return log_msg_ret("Can't find virtqueues", ret);

	priv->port0 = (struct virtio_console_port_priv) {
		.console_priv = priv,
		.receiveq = virtqueues[0],
		.transmitq = virtqueues[1],
		.port_num = 0,
		.buffer_queued = false,
	};

	if (is_multiport == 0) {
		priv->receiveq_control = NULL;
		priv->transmitq_control = NULL;
		return 0;
	}

	priv->receiveq_control = virtqueues[2];
	priv->transmitq_control = virtqueues[3];

	ret = fill_control_inbuf(priv);
	if (ret)
		return log_ret(ret);

	ret = virtio_console_send_control_message(priv, 0, VIRTIO_CONSOLE_DEVICE_READY, 1);
	if (ret)
		return log_msg_ret("Failed to send ready message", ret);

	ret = virtio_console_exhaust_control_queue(priv);
	if (ret)
		return log_msg_ret("Failed to handle control message", ret);

	ret = virtio_console_send_control_message(priv, 0, VIRTIO_CONSOLE_PORT_OPEN, 1);
	if (ret)
		return log_msg_ret("Failed to send port open message", ret);

	for (int i = 1; i < max_ports; i++) {
		ret = virtio_console_create_port(dev, virtqueues, i);
		if (ret)
			return log_msg_ret("Failed to create port", ret);
	}

	return 0;
}

U_BOOT_DRIVER(virtio_console) = {
	.name	= VIRTIO_CONSOLE_DRV_NAME,
	.id	= UCLASS_SERIAL,
	.ops	= &virtio_console_port_serial_ops,
	.bind	= virtio_console_bind,
	.probe	= virtio_console_probe,
	.remove	= virtio_reset,
	.priv_auto	= sizeof(struct virtio_console_priv),
	.flags	= DM_FLAG_ACTIVE_DMA,
};
