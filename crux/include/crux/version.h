#ifndef __CRUX_VERSION_H__
#define __CRUX_VERSION_H__

#include <crux/types.h>
#include <crux/elfstructs.h>

const char *crux_compile_date(void);
const char *crux_compile_time(void);
const char *crux_compile_by(void);
const char *crux_compile_domain(void);
const char *crux_compile_host(void);
const char *crux_compiler(void);
unsigned int crux_major_version(void);
unsigned int crux_minor_version(void);
const char *crux_extra_version(void);
const char *crux_changeset(void);
const char *crux_banner(void);
const char *crux_deny(void);
const char *crux_build_info(void);

extern char crux_cap_info[128];

extern const void *crux_build_id;
extern unsigned int crux_build_id_len; /* 0 -> No build id. */

#ifdef BUILD_ID
void crux_build_init(void);
int crux_build_id_check(const Elf_Note *n, unsigned int n_sz,
                       const void **p, unsigned int *len);
#else
static inline void crux_build_init(void) {};
#endif

#endif /* __CRUX_VERSION_H__ */
