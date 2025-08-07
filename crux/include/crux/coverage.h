#ifndef _CRUX_COV_H
#define _CRUX_COV_H

#ifdef CONFIG_COVERAGE
#include <public/sysctl.h>
int sysctl_cov_op(struct crux_sysctl_coverage_op *op);
#else
static inline int sysctl_cov_op(void *unused)
{
    return -EOPNOTSUPP;
}
#endif

#endif	/* _CRUX_GCOV_H */
