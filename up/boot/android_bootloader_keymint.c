/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <android_bootloader_keymint.h>
#include <android_bootloader_transport.h>

#include <dm/device.h>
#include <serial.h>
#include <../lib/libavb/libavb.h>

static int km_config(struct udevice* console, uint32_t version, uint32_t patchlevel)
{
	static const uint32_t cmd = 18; /* CONFIGURE */
	uint32_t request[] = { version, patchlevel };
	uint32_t response = 0;
	int ret = android_bootloader_request_response(console, cmd, &request, sizeof(request),
						      &response, sizeof(response));
	if (ret != 0) {
		log_err("Failed to handle keymint config message: %d\n", ret);
		return ret;
	}
	if (response != 0) {
		log_err("KM config response was not KM_ERROR_OK, got %d\n", ret);
	}
	return response;
}

static int km_vendor_patchlevel(struct udevice* console, uint32_t patchlevel)
{
	static const uint32_t cmd = 32; /* CONFIGURE_VENDOR_PATCHLEVEL */
	uint32_t request = patchlevel;
	uint32_t response = 0;
	int ret = android_bootloader_request_response(console, cmd, &request, sizeof(request),
						      &response, sizeof(response));
	if (ret != 0) {
		log_err("Failed to handle KM vendor message: %d\n", ret);
		return ret;
	}
	if (response != 0) {
		log_err("KM vendor response was not KM_ERROR_OK, got %d\n", ret);
	}
	return response;
}

static int km_boot_patchlevel(struct udevice* console, uint32_t patchlevel)
{
	static const uint32_t cmd = 33; /* CONFIGURE_BOOT_PATCHLEVEL */
	uint32_t request = patchlevel;
	uint32_t response = 0;
	int ret = android_bootloader_request_response(console, cmd, &request, sizeof(request),
						      &response, sizeof(response));
	if (ret != 0) {
		log_err("Failed to handle KM boot message: %d\n", ret);
		return ret;
	}
	if (response != 0) {
		log_err("KM boot response was not KM_ERROR_OK, got %d\n", ret);
	}
	return response;
}

static int parse_patchlevel(const char* patchlevel_str, uint32_t* result)
{
	bool patchlevel_valid = strlen(patchlevel_str) == strlen("YYYY-MM-DD");
	// If the string is the wrong length, `&&` will short-circuit.
	patchlevel_valid = patchlevel_valid && (patchlevel_str[4] == '-');
	patchlevel_valid = patchlevel_valid && (patchlevel_str[7] == '-');
	if (!patchlevel_valid) {
		log_err("Patchlevel (%s) date format was not YYYY-MM-DD\n",
			patchlevel_str);
		return 0;
	}
	char patchlevel_nodashes[sizeof("YYYYMMDD")];
	memcpy(patchlevel_nodashes, patchlevel_str, 4);
	memcpy(&patchlevel_nodashes[4], &patchlevel_str[5], 2);
	memcpy(&patchlevel_nodashes[6], &patchlevel_str[8], 2);
	patchlevel_nodashes[8] = '\0';
	unsigned long result_ul = 0; // Cover uint32_t / unsigned long mismatch
	int ret = strict_strtoul(patchlevel_nodashes, 10, &result_ul);
	if (ret != 0) {
		log_err("Patchlevel (%s) date format was not YYYY-MM-DD\n",
			patchlevel_str);
	} else {
		*result = result_ul;
	}
	return ret;
}

static struct keymint_relevant_avb {
	uint32_t system_version;
	uint32_t system_patchlevel;
	uint32_t vendor_patchlevel;
	uint32_t boot_patchlevel;
};

static int extract_keymint_relevant_data(AvbSlotVerifyData* avb_in,
					 struct keymint_relevant_avb* avb_out)
{
	static const char system_version_key[] =
		"com.android.build.system.os_version";
	const char* system_version = NULL;

	static const char system_patchlevel_key[] =
		"com.android.build.system.security_patch";
	const char* system_patchlevel = NULL;

	static const char vendor_patchlevel_key[] =
		"com.android.build.vendor.security_patch";
	const char* vendor_patchlevel = NULL;

	static const char boot_patchlevel_key[] =
		"com.android.build.boot.security_patch";
	const char* boot_patchlevel = NULL;

	for (int i = 0; i < avb_in->num_vbmeta_images; i++) {
		AvbVBMetaData *p = &avb_in->vbmeta_images[i];
		if (strcmp("vbmeta_system", p->partition_name) == 0) {
			system_version = avb_property_lookup(
				p->vbmeta_data, p->vbmeta_size,
				system_version_key, 0, NULL);
			system_patchlevel = avb_property_lookup(
				p->vbmeta_data, p->vbmeta_size,
				system_patchlevel_key, 0, NULL);
		}
		if (strcmp("vbmeta", p->partition_name) == 0) {
			vendor_patchlevel = avb_property_lookup(
				p->vbmeta_data, p->vbmeta_size,
				vendor_patchlevel_key, 0, NULL);
		}
		if (strcmp("boot", p->partition_name) == 0) {
			boot_patchlevel = avb_property_lookup(
				p->vbmeta_data, p->vbmeta_size,
				boot_patchlevel_key, 0, NULL);
		}
	}

	if (system_version == NULL) {
		log_err("AVB was missing %s\n", system_version_key);
		return -EINVAL;
	}
	unsigned long system_version_ul = 0;
	if (strict_strtoul(system_version, 10, &system_version_ul)) {
		log_err("%s had incorrect format, got %s\n", system_version_key,
			system_version);
		return -EINVAL;
	}
        avb_out->system_version = system_version_ul;
	if (system_patchlevel == NULL) {
		log_err("AVB was missing %s\n", system_patchlevel_key);
		return -EINVAL;
	}
	if (parse_patchlevel(system_patchlevel, &avb_out->system_patchlevel)) {
		log_err("%s had incorrect format, got \"%s\"\n",
			system_patchlevel_key, system_patchlevel);
		return -EINVAL;
	}
	if (vendor_patchlevel == NULL) {
		log_err("AVB was missing %s\n", vendor_patchlevel_key);
		return -EINVAL;
	}
	if (parse_patchlevel(vendor_patchlevel, &avb_out->vendor_patchlevel)) {
		log_err("%s had incorrect format, got \"%s\"\n",
			vendor_patchlevel_key, vendor_patchlevel);
		return -EINVAL;
	}
	if (vendor_patchlevel == NULL) {
		log_err("AVB was missing %s\n", vendor_patchlevel_key);
		return -EINVAL;
	}
	if (parse_patchlevel(boot_patchlevel, &avb_out->boot_patchlevel)) {
		log_err("%s had incorrect format, got \"%s\"\n",
			boot_patchlevel_key, boot_patchlevel);
		return -EINVAL;
	}
	return 0;
}

int write_avb_to_keymint_console(AvbSlotVerifyData *avb_data, struct udevice* km_console)
{
	if (avb_data == NULL) {
		log_err("Received null avb_data.\n");
		return -EINVAL;
	}

	struct keymint_relevant_avb km_avb;
	int ret = extract_keymint_relevant_data(avb_data, &km_avb);
	if (ret != 0) {
		log_err("Failed to extract km-related properties: %d\n", ret);
		return ret;
	}

	if (km_console == NULL) {
		log_err("Received null km_console.\n");
		return -EINVAL;
	}

	ret = km_config(km_console, km_avb.system_version,
			km_avb.system_patchlevel);
	if (ret != 0) {
		log_err("Failed to negotiate keymint config : %d\n", ret);
		return ret;
	}
	ret = km_vendor_patchlevel(km_console, km_avb.vendor_patchlevel);
	if (ret != 0) {
		log_err("Failed to negotiate keymint vendor patch: %d\n", ret);
		return ret;
	}
	ret = km_boot_patchlevel(km_console, km_avb.boot_patchlevel);
	if (ret != 0) {
		log_err("Failed to negotiate keymint boot patch: %d\n", ret);
		return ret;
	}
	return 0;
}
