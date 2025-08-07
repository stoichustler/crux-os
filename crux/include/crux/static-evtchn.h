/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef CRUX_STATIC_EVTCHN_H
#define CRUX_STATIC_EVTCHN_H

#ifdef CONFIG_STATIC_EVTCHN

void alloc_static_evtchn(void);

#else /* !CONFIG_STATIC_EVTCHN */

static inline void alloc_static_evtchn(void) {};

#endif /* CONFIG_STATIC_EVTCHN */

#endif /* CRUX_STATIC_EVTCHN_H */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
