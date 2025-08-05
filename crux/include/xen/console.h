/******************************************************************************
 * xen/console.h
 * 
 * xen header file concerning console access.
 */

#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <xen/inttypes.h>
#include <xen/ctype.h>
#include <public/xen.h>

struct xen_sysctl_readconsole;
long read_console_ring(struct xen_sysctl_readconsole *op);

void console_init_preirq(void);
void console_init_ring(void);
void console_init_irq(void);
void console_init_postirq(void);
void console_endboot(void);
int console_has(const char *device);

/* Not speculation safe - only used to prevent interleaving of output. */
unsigned long console_lock_recursive_irqsave(void);
void console_unlock_recursive_irqrestore(unsigned long flags);
void console_force_unlock(void);

void console_start_sync(void);
void console_end_sync(void);

void console_start_log_everything(void);
void console_end_log_everything(void);

struct domain *console_get_domain(void);
void console_put_domain(struct domain *d);

/*
 * Steal output from the console. Returns +ve identifier, else -ve error.
 * Takes the handle of the serial line to steal, and steal callback function.
 */
int console_steal(int handle, void (*fn)(const char *str, size_t nr));

/* Give back stolen console. Takes the identifier returned by console_steal. */
void console_giveback(int id);

#ifdef CONFIG_SYSTEM_SUSPEND
int console_suspend(void);
int console_resume(void);
#endif

/* Emit a string via the serial console. */
void console_serial_puts(const char *s, size_t nr);

extern int8_t opt_console_xen;

static inline bool is_console_printable(unsigned char c)
{
    return isprint(c) || c == '\n' || c == '\t';
}

#endif /* __CONSOLE_H__ */
