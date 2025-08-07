#ifndef __CRUX_ERRNO_H__
#define __CRUX_ERRNO_H__

#ifndef __ASSEMBLY__

#define CRUX_ERRNO(name, value) name = (value),
enum {
#include <public/errno.h>
};

#else /* !__ASSEMBLY__ */

#define CRUX_ERRNO(name, value) .equ name, value
#include <public/errno.h>

#endif /* __ASSEMBLY__ */

#endif /*  __CRUX_ERRNO_H__ */
