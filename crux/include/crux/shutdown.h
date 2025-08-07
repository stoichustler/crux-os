#ifndef __CRUX_SHUTDOWN_H__
#define __CRUX_SHUTDOWN_H__

#include <crux/compiler.h>
#include <crux/types.h>

/* opt_noreboot: If true, machine will need manual reset on error. */
extern bool opt_noreboot;

void noreturn hwdom_shutdown(unsigned char reason);

void noreturn machine_restart(unsigned int delay_millisecs);
void noreturn machine_halt(void);

#endif /* __CRUX_SHUTDOWN_H__ */
