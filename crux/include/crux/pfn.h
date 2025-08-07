#ifndef __CRUX_PFN_H__
#define __CRUX_PFN_H__

#include <crux/page-size.h>

#define PFN_DOWN(x)   ((x) >> PAGE_SHIFT)
#define PFN_UP(x)     (((x) + PAGE_SIZE-1) >> PAGE_SHIFT)

#define round_pgup(p)    (((p) + (PAGE_SIZE - 1)) & PAGE_MASK)
#define round_pgdown(p)  ((p) & PAGE_MASK)

#endif /* __CRUX_PFN_H__ */
