/****************************************************************************
 * apps/testing/testsuites/kernel/mm/cmocka_mm_test.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <roux/config.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdio.h>
#include "MmTest.h"
#include <cmocka.h>

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: cmocka_sched_test_main
 ****************************************************************************/

int main(int argc, char *argv[])
{
	/* Add Test Cases */

	const struct CMUnitTest roux_mm_test_suites[] = {
		cmocka_unit_test_setup_teardown(test_roux_mm01, test_roux_mm_setup, test_roux_mm_teardown),
		cmocka_unit_test_setup_teardown(test_roux_mm02, test_roux_mm_setup, test_roux_mm_teardown),
		cmocka_unit_test_setup_teardown(test_roux_mm03, test_roux_mm_setup, test_roux_mm_teardown),
		cmocka_unit_test_setup_teardown(test_roux_mm04, test_roux_mm_setup, test_roux_mm_teardown),
		cmocka_unit_test_setup_teardown(test_roux_mm05, test_roux_mm_setup, test_roux_mm_teardown),
		cmocka_unit_test_setup_teardown(test_roux_mm06, test_roux_mm_setup, test_roux_mm_teardown),
		cmocka_unit_test_setup_teardown(test_roux_mm07, test_roux_mm_setup, test_roux_mm_teardown),
		cmocka_unit_test_setup_teardown(test_roux_mm08, test_roux_mm_setup, test_roux_mm_teardown),
	};

	/* Run Test cases */

	cmocka_run_group_tests(roux_mm_test_suites, NULL, NULL);

	return 0;
}
