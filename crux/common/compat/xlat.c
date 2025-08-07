/******************************************************************************
 * xlat.c
 */

#include <crux/compat.h>
#include <crux/lib.h>
#include <compat/crux.h>
#include <compat/event_channel.h>
#include <compat/vcpu.h>

/* In-place translation functons: */
void xlat_start_info(struct start_info *native,
                     enum XLAT_start_info_console console)
{
    struct compat_start_info *compat = (void *)native;

    BUILD_BUG_ON(sizeof(*native) < sizeof(*compat));
    XLAT_start_info(compat, native);
}

void xlat_vcpu_runstate_info(struct vcpu_runstate_info *native)
{
    struct compat_vcpu_runstate_info *compat = (void *)native;

    BUILD_BUG_ON(sizeof(*native) < sizeof(*compat));
    XLAT_vcpu_runstate_info(compat, native);
}

#define crux_dom0_vga_console_info dom0_vga_console_info
CHECK_dom0_vga_console_info;
#undef dom0_vga_console_info

#define crux_evtchn_alloc_unbound evtchn_alloc_unbound
#define crux_evtchn_bind_interdomain evtchn_bind_interdomain
#define crux_evtchn_bind_ipi evtchn_bind_ipi
#define crux_evtchn_bind_pirq evtchn_bind_pirq
#define crux_evtchn_bind_vcpu evtchn_bind_vcpu
#define crux_evtchn_bind_virq evtchn_bind_virq
#define crux_evtchn_close evtchn_close
#define crux_evtchn_op evtchn_op
#define crux_evtchn_send evtchn_send
#define crux_evtchn_status evtchn_status
#define crux_evtchn_unmask evtchn_unmask
CHECK_evtchn_op;
#undef crux_evtchn_alloc_unbound
#undef crux_evtchn_bind_interdomain
#undef crux_evtchn_bind_ipi
#undef crux_evtchn_bind_pirq
#undef crux_evtchn_bind_vcpu
#undef crux_evtchn_bind_virq
#undef crux_evtchn_close
#undef crux_evtchn_op
#undef crux_evtchn_send
#undef crux_evtchn_status
#undef crux_evtchn_unmask

#define crux_evtchn_expand_array evtchn_expand_array
CHECK_evtchn_expand_array;
#undef crux_evtchn_expand_array

#define crux_evtchn_init_control evtchn_init_control
CHECK_evtchn_init_control;
#undef crux_evtchn_init_control

#define crux_evtchn_reset evtchn_reset
CHECK_evtchn_reset;
#undef crux_evtchn_reset

#define crux_evtchn_set_priority evtchn_set_priority
CHECK_evtchn_set_priority;
#undef crux_evtchn_set_priority

#define crux_mmu_update mmu_update
CHECK_mmu_update;
#undef crux_mmu_update

#define crux_vcpu_time_info vcpu_time_info
CHECK_vcpu_time_info;
#undef crux_vcpu_time_info

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
