#ifndef CRUX_LIB_X86_PRIVATE_H
#define CRUX_LIB_X86_PRIVATE_H

#ifdef __CRUX__

#include <crux/bitops.h>
#include <crux/guest_access.h>
#include <crux/kernel.h>
#include <crux/lib.h>
#include <crux/nospec.h>
#include <crux/types.h>

#include <asm/msr-index.h>

#define copy_to_buffer_offset copy_to_guest_offset
#define copy_from_buffer_offset copy_from_guest_offset

#else

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <crux/asm/msr-index.h>
#include <crux/asm/x86-vendors.h>

#include <crux-tools/common-macros.h>

static inline bool test_bit(unsigned int bit, const void *vaddr)
{
    const char *addr = vaddr;

    return addr[bit / 8] & (1u << (bit % 8));
}

#define array_access_nospec(a, i) (a)[(i)]

/* memcpy(), but with copy_to_guest_offset()'s API. */
#define copy_to_buffer_offset(dst, index, src, nr)      \
({                                                      \
    const typeof(*(src)) *src_ = (src);                 \
    typeof(*(dst)) *dst_ = (dst);                       \
    typeof(index) index_ = (index);                     \
    typeof(nr) nr_ = (nr), i_;                          \
                                                        \
    for ( i_ = 0; i_ < nr_; i_++ )                      \
        dst_[index_ + i_] = src_[i_];                   \
    0;                                                  \
})

/* memcpy(), but with copy_from_guest_offset()'s API. */
#define copy_from_buffer_offset(dst, src, index, nr)    \
({                                                      \
    const typeof(*(src)) *src_ = (src);                 \
    typeof(*(dst)) *dst_ = (dst);                       \
    typeof(index) index_ = (index);                     \
    typeof(nr) nr_ = (nr), i_;                          \
                                                        \
    for ( i_ = 0; i_ < nr_; i_++ )                      \
        dst_[i_] = src_[index_ + i_];                   \
    0;                                                  \
})

#endif /* __CRUX__ */

#endif /* CRUX_LIB_X86_PRIVATE_H */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
