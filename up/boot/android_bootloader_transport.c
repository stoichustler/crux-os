/*
 * Copyright (C) 2023 The Android Open Source Project
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <android_bootloader_transport.h>

#include <serial.h>

struct header {
	uint32_t cmd : 31;
	uint32_t is_response : 1;
	uint32_t payload_size;
} __attribute__((packed));

static bool verify_device_is_console(struct udevice* console)
{
	if (console->driver->id != UCLASS_SERIAL) {
		log_err("Passed device: %s isn't a serial. Uclass: %d\n",
			console->name, console->driver->id);
		return false;
	}
	struct dm_serial_ops *ops = serial_get_ops(console);
	if (ops->putc == NULL) {
		log_err("Passed device doesn't support putc\n");
		return false;
	}
	if (ops->getc == NULL) {
		log_err("Passed device doesn't support getc\n");
		return false;
	}
	return true;
}

static ssize_t console_write(struct udevice* console, const void* data, size_t size)
{
	const unsigned char* data_chars = data;
	struct dm_serial_ops *ops = serial_get_ops(console);
	ssize_t i;

	for (i = 0; i < size; i++) {
		int ret;
		if ((ret = ops->putc(console, data_chars[i])) != 0) {
			log_err("error writing to console: %d\n", ret);
			return ret;
		}
	}
	return i;
}

static ssize_t console_read(struct udevice* console, void* data, size_t size)
{
	unsigned char* data_chars = data;
	struct dm_serial_ops *ops = serial_get_ops(console);
	ssize_t i;

	for (i = 0; i < size; i++) {
		int c;
		while ((c = ops->getc(console)) == -EAGAIN) {}
		if (c < 0) {
			log_err("error reading from console: %d\n", c);
			return c;
		}
		data_chars[i] = c;
	}
	return i;
}

int android_bootloader_request_response(struct udevice *console, uint32_t command,
					const void *request, size_t request_size,
					void *response, size_t response_size)
{
	if (!verify_device_is_console(console)) {
		return -EINVAL;
	}
	struct header req_header = {
		.cmd = command,
		.is_response = 0,
		.payload_size = request_size,
	};
	int ret;
	printf("Writing %zu bytes to %s console\n", sizeof(req_header), console->name);
	ret = console_write(console, &req_header, sizeof(req_header));
	if (ret != sizeof(req_header)) {
		log_err("Failed to write android bootloader request header: %d\n", ret);
		return ret;
	}
	ret = console_write(console, request, request_size);
	if (ret != request_size) {
		log_err("Failed to write android bootloader request body: %d\n", ret);
		return ret;
	}
	struct header expected_response_header = {
		.cmd = command,
		.is_response = 1,
		.payload_size = response_size,
	};
	struct header resp_header;
	printf("Reading %zu bytes from %s console\n", sizeof(resp_header), console->name);
	ret = console_read(console, &resp_header, sizeof(resp_header));
	if (ret != sizeof(resp_header)) {
		log_err("Failed to read android bootloader response header: %d\n", ret);
		return ret;
	}
	int resp_comparison = memcmp(&resp_header, &expected_response_header,
				     sizeof(resp_header));
	if (resp_comparison != 0) {
		log_err("Received unexpected android bootloader response header.\n");
		log_err("Expected cmd = %d, received cmd = %d\n",
		      expected_response_header.cmd, resp_header.cmd);
		log_err("Expected is_response = %d, received is_response = %d\n",
		      expected_response_header.is_response,
		      resp_header.is_response);
		log_err("Expected payload_size = %d, received payload_size = %d\n",
		      expected_response_header.payload_size,
		      resp_header.payload_size);
		return -EINVAL;
	}
	ret = console_read(console, response, response_size);
	if (ret != response_size) {
		log_err("Failed to read android bootloader response body: %d\n", ret);
		return ret;
	}
	return 0;
}
