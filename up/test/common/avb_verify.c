// SPDX-License-Identifier: GPL-2.0+
/*
 * Unit tests for AVB verification (avb_verify)
 *
 * Copyright 2023 Google LLC
 */

#include <avb_verify.h>
#include <common.h>
#include <test/common.h>
#include <test/test.h>
#include <test/ut.h>

#define TEST_INTERFACE "dev-iface"
#define TEST_DEV_NUM "dev-num"

static int test_avb_ops_alloc(struct unit_test_state *uts)
{
	AvbOps *ops = NULL;
	struct AvbOpsData *user_data = NULL;

	ops = avb_ops_alloc(TEST_INTERFACE, TEST_DEV_NUM);
	ut_assertnonnull(ops);
	ut_assertnonnull(ops->read_from_partition);
	ut_assertnonnull(ops->write_to_partition);
	ut_assertnonnull(ops->validate_vbmeta_public_key);
	ut_assertnonnull(ops->read_rollback_index);
	ut_assertnonnull(ops->write_rollback_index);
	ut_assertnonnull(ops->read_is_device_unlocked);
	ut_assertnonnull(ops->get_unique_guid_for_partition);
	ut_assertnonnull(ops->get_size_of_partition);
#ifdef CONFIG_OPTEE_TA_AVB
	ut_assertnonnull(ops->write_persistent_value);
	ut_assertnonnull(ops->read_persistent_value);
#endif
	user_data = (struct AvbOpsData*)ops->user_data;
	ut_asserteq_ptr(&user_data->ops, ops);
	ut_asserteq_str(user_data->iface, TEST_INTERFACE);
	ut_asserteq_str(user_data->devnum, TEST_DEV_NUM);

	avb_ops_free(ops);

	return 0;
}

static int test_avb_ops_free(struct unit_test_state *uts)
{
	AvbOps *ops = NULL;

	// Verify initial free of allocated AvbOps succeeds.
	ops = avb_ops_alloc(TEST_INTERFACE, TEST_DEV_NUM);
	ut_assertnonnull(ops);
	avb_ops_free(ops);

	// Verify free of NULL does not crash.
	avb_ops_free(NULL);

	return 0;
}

static int test_avb_ops_set_state(struct unit_test_state *uts)
{
	AvbOps *ops = NULL;
	struct AvbOpsData *user_data = NULL;

	ops = avb_ops_alloc(TEST_INTERFACE, TEST_DEV_NUM);
	user_data = (struct AvbOpsData *)ops->user_data;

	// Verify that the command line and state changes reflect the request.
	ut_asserteq_str(avb_set_state(ops, AVB_GREEN), AVB_VERIFIED_BOOT_STATE_GREEN);
	ut_asserteq(user_data->boot_state, AVB_GREEN);
	ut_asserteq_str(avb_set_state(ops, AVB_YELLOW), AVB_VERIFIED_BOOT_STATE_YELLOW);
	ut_asserteq(user_data->boot_state, AVB_YELLOW);
	ut_asserteq_str(avb_set_state(ops, AVB_ORANGE), AVB_VERIFIED_BOOT_STATE_ORANGE);
	ut_asserteq(user_data->boot_state, AVB_ORANGE);
	ut_assertnull(avb_set_state(ops, AVB_RED));
	ut_asserteq(user_data->boot_state, AVB_RED);

	// Verify that when setting an invalid AVB state, we default to RED.
	avb_set_state(ops, AVB_GREEN);
	ut_assertnull(avb_set_state(ops, -1 /* Invalid AVB State */));
	ut_asserteq(user_data->boot_state, AVB_RED);

	avb_ops_free(ops);
	return 0;
}

/*
 * Verifies that `cmdline` matches `replaced_cmdline` with
 * `expected_veritymode` appended at the end. This is a helper function for
 * |test_avb_set_enforce_option|.
 */
static int assert_expected_verity_flags(struct unit_test_state *uts,
					const char* cmdline,
					const char* replaced_cmdline,
					const char* expected_veritymode)
{
	ut_asserteq_mem(cmdline, replaced_cmdline, strlen(replaced_cmdline));
	ut_asserteq(cmdline[strlen(replaced_cmdline)], (int)' ');
	ut_asserteq_str(&cmdline[strlen(replaced_cmdline) + 1], expected_veritymode);
	return 0;
}

static int test_avb_set_enforce_option(struct unit_test_state *uts)
{
	const char *CMDLINES_RESTART_ON_CORRUPTION[] = {
		/*
		 * Sample command lines where the target argument appears
		 * at the front, middle and end of the argument list.
		 */
                VERITY_TABLE_OPT_RESTART " . .",
                ". " VERITY_TABLE_OPT_RESTART " .",
                ". . " VERITY_TABLE_OPT_RESTART,
	};
	const char *CMDLINES_IGNORE_CORRUPTION[] = {
		/*
		 * Both variants of the verity table flags are supplied
		 * to ensure we leave the flag as-is if already correct.
		 */
                VERITY_TABLE_OPT_LOGGING " . .",
                ". " VERITY_TABLE_OPT_LOGGING " .",
                ". . " VERITY_TABLE_OPT_LOGGING,
	};
	size_t cmdlines_size = sizeof(CMDLINES_IGNORE_CORRUPTION)
			/ sizeof(CMDLINES_IGNORE_CORRUPTION[0]);

	char *cmdline_with_option = NULL;

	for (size_t i = 0; i < cmdlines_size; i++) {
		/*
		 * When enforcing verity on a command line that contains the
		 * 'restart_on_corruption' veritytable flag, verify that the
		 * flag is left unchanged and the enforcing veritymode flag is
		 * enabled.
		 */
		cmdline_with_option = avb_set_enforce_verity(
				CMDLINES_RESTART_ON_CORRUPTION[i]);
		assert_expected_verity_flags(uts,
					     cmdline_with_option,
					     CMDLINES_RESTART_ON_CORRUPTION[i],
					     AVB_VERITY_MODE_ENFORCING);
		avb_free(cmdline_with_option);

		/*
		 * When the command line contains 'ignore_corruption', confirm
		 * that the flag is replaced with 'restart_on_corruption'.
		 */
		cmdline_with_option = avb_set_enforce_verity(
				CMDLINES_IGNORE_CORRUPTION[i]);
		assert_expected_verity_flags(uts,
					     cmdline_with_option,
					     CMDLINES_RESTART_ON_CORRUPTION[i],
					     AVB_VERITY_MODE_ENFORCING);
		avb_free(cmdline_with_option);

		/*
		 * When ignoring corruption on a command line that contains the
		 * 'ignore_corruption' veritytable flag, verify that the flag is
		 * left unchanged and the logging veritymode flag is enabled.
		 */
		cmdline_with_option = avb_set_ignore_corruption(
				CMDLINES_IGNORE_CORRUPTION[i]);
		assert_expected_verity_flags(uts,
					     cmdline_with_option,
					     CMDLINES_IGNORE_CORRUPTION[i],
					     AVB_VERITY_MODE_IGNORE_CORRUPTION);
		avb_free(cmdline_with_option);

		/*
		 * When the command line contains 'restart_on_corruption',
		 * confirm that the flag is replaced with 'ignore_corruption'.
		 */
		cmdline_with_option = avb_set_ignore_corruption(
				CMDLINES_RESTART_ON_CORRUPTION[i]);
		assert_expected_verity_flags(uts,
					     cmdline_with_option,
					     CMDLINES_IGNORE_CORRUPTION[i],
					     AVB_VERITY_MODE_IGNORE_CORRUPTION);
		avb_free(cmdline_with_option);
	}

	// When the command line contains neither veritytable flag, return NULL.
	ut_assertnull(avb_set_ignore_corruption(". . ."));

	return 0;
}

static int test_avb_pubkey_is_trusted(struct unit_test_state *uts)
{
	extern const char avb_pubkey[];
	extern const size_t avb_pubkey_size;

	ut_asserteq(avb_pubkey_is_trusted(avb_pubkey, 0), CMD_RET_FAILURE);
	ut_asserteq(avb_pubkey_is_trusted("", avb_pubkey_size), CMD_RET_FAILURE);
	ut_asserteq(avb_pubkey_is_trusted("", 0), CMD_RET_FAILURE);
	ut_asserteq(avb_pubkey_is_trusted(avb_pubkey, avb_pubkey_size), CMD_RET_SUCCESS);

	return 0;
}

COMMON_TEST(test_avb_ops_alloc, 0);
COMMON_TEST(test_avb_ops_free, 0);
COMMON_TEST(test_avb_ops_set_state, 0);
COMMON_TEST(test_avb_set_enforce_option, 0);
COMMON_TEST(test_avb_pubkey_is_trusted, 0);
