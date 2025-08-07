#ifndef __CRUX_STRING_H__
#define __CRUX_STRING_H__

#include <crux/types.h>	/* for size_t */

/*
 * These string functions are considered too dangerous for normal use.
 * Use safe_strcpy(), safe_strcat(), strlcpy(), strlcat() as appropriate.
 */
#define strcpy  __crux_has_no_strcpy__
#define strcat  __crux_has_no_strcat__
#define strncpy __crux_has_no_strncpy__
#define strncat __crux_has_no_strncat__

size_t strlcpy(char *dest, const char *src, size_t size);
size_t strlcat(char *dest, const char *src, size_t size);
int strcmp(const char *cs, const char *ct);
int strncmp(const char *cs, const char *ct, size_t count);
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t len);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *s1, const char *s2);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t count);
char *strpbrk(const char *cs,const char *ct);
char *strsep(char **s, const char *ct);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);

void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *cs, const void *ct, size_t count);
void *memchr(const void *s, int c, size_t n);
void *memchr_inv(const void *s, int c, size_t n);

#include <asm/string.h>

#ifndef __HAVE_ARCH_STRCMP
#define strcmp(s1, s2) __builtin_strcmp(s1, s2)
#endif

#ifndef __HAVE_ARCH_STRNCMP
#define strncmp(s1, s2, n) __builtin_strncmp(s1, s2, n)
#endif

#ifndef __HAVE_ARCH_STRCASECMP
#define strcasecmp(s1, s2) __builtin_strcasecmp(s1, s2)
#endif

#ifndef __HAVE_ARCH_STRCASECMP
#define strncasecmp(s1, s2, n) __builtin_strncasecmp(s1, s2, n)
#endif

#ifndef __HAVE_ARCH_STRCHR
#define strchr(s1, c) __builtin_strchr(s1, c)
#endif

#ifndef __HAVE_ARCH_STRRCHR
#define strrchr(s1, c) __builtin_strrchr(s1, c)
#endif

#ifndef __HAVE_ARCH_STRSTR
#define strstr(s1, s2) __builtin_strstr(s1, s2)
#endif

#ifndef __HAVE_ARCH_STRLEN
#define strlen(s1) __builtin_strlen(s1)
#endif

#ifndef __HAVE_ARCH_STRCSPN
#define strcspn(s, r) __builtin_strcspn(s, r)
#endif

#ifndef __HAVE_ARCH_MEMSET
#define memset(s, c, n) __builtin_memset(s, c, n)
#endif

#ifndef __HAVE_ARCH_MEMCPY
#define memcpy(d, s, n) __builtin_memcpy(d, s, n)
#endif

#ifndef __HAVE_ARCH_MEMMOVE
#define memmove(d, s, n) __builtin_memmove(d, s, n)
#endif

#ifndef __HAVE_ARCH_MEMCMP
#define memcmp(s1, s2, n) __builtin_memcmp(s1, s2, n)
#endif

#ifndef __HAVE_ARCH_MEMCHR
#define memchr(s, c, n) __builtin_memchr(s, c, n)
#endif

#define is_char_array(x) __builtin_types_compatible_p(typeof(x), char[])

/* safe_xxx always NUL-terminates and returns !=0 if result is truncated. */
#define safe_strcpy(d, s) ({                    \
    BUILD_BUG_ON(!is_char_array(d));            \
    (strlcpy(d, s, sizeof(d)) >= sizeof(d));    \
})
#define safe_strcat(d, s) ({                    \
    BUILD_BUG_ON(!is_char_array(d));            \
    (strlcat(d, s, sizeof(d)) >= sizeof(d));    \
})

#endif /* __CRUX_STRING_H__ */
/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
