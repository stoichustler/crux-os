/*
 * Copyright (c) 2025 HUSTLER
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/shell/shell.h>

extern void coremark_run(void);

static int coremark_start(const struct shell *sh, int argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "kicking coremark ...");
	coremark_run();

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	subcmd_coremark,
	SHELL_CMD_ARG(start, NULL,
		      " start coremark\n",
		      coremark_start, 1, 0),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_ARG_REGISTER(coremark,
	&subcmd_coremark,
	"benchmarking via coremark",
	NULL, 2, 0);
