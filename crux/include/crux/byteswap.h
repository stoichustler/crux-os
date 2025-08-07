/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef CRUX_BYTESWAP_H
#define CRUX_BYTESWAP_H

#define bswap16(x) __builtin_bswap16(x)
#define bswap32(x) __builtin_bswap32(x)
#define bswap64(x) __builtin_bswap64(x)

#if BITS_PER_LONG == 64
# define bswapl(x) bswap64(x)
#elif BITS_PER_LONG == 32
# define bswapl(x) bswap32(x)
#endif

#endif /* CRUX_BYTESWAP_H */
