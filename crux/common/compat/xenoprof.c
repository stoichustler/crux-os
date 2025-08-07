/*
 * compat/cruxoprof.c
 */

#include <compat/cruxoprof.h>

#define COMPAT
#define ret_t int

#define do_cruxoprof_op compat_cruxoprof_op

#define crux_oprof_init cruxoprof_init
CHECK_oprof_init;
#undef crux_oprof_init

#define cruxoprof_get_buffer compat_oprof_get_buffer
#define cruxoprof_op_get_buffer compat_oprof_op_get_buffer
#define cruxoprof_arch_counter compat_oprof_arch_counter

#define crux_domid_t domid_t
#define compat_domid_t domid_compat_t
CHECK_TYPE(domid);
#undef compat_domid_t
#undef crux_domid_t

#define crux_oprof_passive cruxoprof_passive
CHECK_oprof_passive;
#undef crux_oprof_passive

#define cruxoprof_counter compat_oprof_counter

#include "../cruxoprof.c"

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
