/*
 * Copyright (c) 2025 HUSTLER
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/xen/generic.h>
#include <zephyr/xen/dom0/domctl.h>
#include <zephyr/xen/dom0/sysctl.h>
#include <zephyr/xen/dom0/version.h>
#include <zephyr/xen/public/xen.h>

#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(crux_cmds);

#define MAX_DOMAINS         (3)

static int crux_stats(const struct shell *sh, int argc, char **argv)
{
	uint32_t nr_dom, i, domid;
	int rc;
	xen_domctl_getdomaininfo_t doms_info[MAX_DOMAINS];
	struct xen_sysctl_physinfo phys_info;
	char extra_version[XEN_EXTRAVERSION_LEN];

	if ((rc = xen_sysctl_physinfo(&phys_info))) {
		shell_error(sh, "get crux physical infos failed: %d", rc);
		return -EINVAL;
	}

	nr_dom = xen_sysctl_getdomaininfo(doms_info, 0, MAX_DOMAINS);
	if (nr_dom > MAX_DOMAINS) {
		shell_error(sh, "get invalid vm number: %u > max(%u)",
			nr_dom, MAX_DOMAINS);
		return -EINVAL;
	}

	rc = xen_version_extraversion(extra_version, XEN_EXTRAVERSION_LEN);
	if (rc) {
		shell_error(sh, "get crux extra version failed\n");
		return -EINVAL;
	}

	/* get the version of crux */
	rc = xen_version();

	shell_print(sh,
	     "zeus as a vm-0 on crux\n\n"
	     "[<hyp>]\n"
	     "  version:                  %u.%u%s\n"
	     "  threads per core:         %u\n"
	     "  cores per socket:         %u\n"
	     "  nr cpus:                  %u\n"
	     "  nr nodes:                 %u\n"
	     "  cpu khz:                  %u\n"
	     "  capabilities:             0x%08x\n"
	     "  arch capabilities:        0x%08x\n"
	     "  total pages:              %llu\n"
	     "  free pages:               %llu\n"
	     "  scrub pages:              %llu\n"
	     "  outstanding pages:        %llu\n"
	     "  nr vms:                   %u",
	     (rc >> 16) & 0xFFFF, rc & 0xFFFF, extra_version,
	     phys_info.threads_per_core,
	     phys_info.cores_per_socket,
	     phys_info.nr_cpus,
	     phys_info.nr_nodes,
	     phys_info.cpu_khz,
	     phys_info.capabilities,
	     phys_info.arch_capabilities,
	     phys_info.total_pages,
	     phys_info.free_pages,
	     phys_info.scrub_pages,
	     phys_info.outstanding_pages,
	     nr_dom);

	for (i = 0; i < nr_dom; i++) {
		domid = i;

		shell_print(sh,
			"[<vm%u>]\n"
			"  flags:                    0x%08x\n"
			"  total pages:              %llu\n"
			"  maximum pages:            %llu\n"
			"  outstanding pages:        %llu\n"
			"  shared pages:             %llu\n"
			"  paged pages:              %llu\n"
			"  shared frames:            %llu\n"
			"  cpu time:                 %llu\n"
			"  online vcpus:             %u\n"
			"  guest address bits:       %u",
			domid,
			doms_info[domid].flags,
			doms_info[domid].tot_pages,
			doms_info[domid].max_pages,
			doms_info[domid].outstanding_pages,
			doms_info[domid].shr_pages,
			doms_info[domid].paged_pages,
			doms_info[domid].shared_info_frame,
			doms_info[domid].cpu_time,
			doms_info[domid].nr_online_vcpus,
			doms_info[domid].gpaddr_bits);
	}

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	subcmd_box,
	SHELL_CMD_ARG(stats, NULL,
		      " Display crux vms stats\n",
		      crux_stats, 1, 0),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_ARG_REGISTER(box,
	&subcmd_box,
	"crux hypervisor svm commands",
	NULL, 2, 0);
