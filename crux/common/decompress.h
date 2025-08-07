#ifndef DECOMPRESS_H
#define DECOMPRESS_H

#ifdef __CRUX__

#include <crux/decompress.h>
#include <crux/init.h>
#include <crux/string.h>
#include <crux/types.h>
#include <crux/xmalloc.h>

#define malloc xmalloc_bytes
#define free xfree

#define large_malloc xmalloc_bytes
#define large_free xfree

#else

#undef __init /* tools/libs/guest/xg_private.h has its own one */
#define __init
#define __initdata

#define large_malloc malloc
#define large_free free

#endif

#endif /* DECOMPRESS_H */
