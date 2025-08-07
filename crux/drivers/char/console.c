/******************************************************************************
 * console.c
 *
 * Emergency console I/O for crux and the domain-0 guest OS.
 *
 * Copyright (c) 2002-2004, K A Fraser.
 *
 * Added printf_ratelimit
 *     Taken from Linux - Author: Andi Kleen (net_ratelimit)
 *     Ported to crux - Steven Rostedt - Red Hat
 */

#include <crux/version.h>
#include <crux/lib.h>
#include <crux/init.h>
#include <crux/event.h>
#include <crux/console.h>
#include <crux/param.h>
#include <crux/serial.h>
#include <crux/softirq.h>
#include <crux/keyhandler.h>
#include <crux/guest_access.h>
#include <crux/watchdog.h>
#include <crux/shutdown.h>
#include <crux/video.h>
#include <crux/kexec.h>
#include <crux/warning.h>
#include <asm/div64.h>
#include <crux/hypercall.h> /* for do_console_io */
#include <crux/early_printk.h>
#include <crux/warning.h>
#include <crux/pv_console.h>
#include <asm/setup.h>
#include <crux/sections.h>
#include <crux/consoled.h>

#ifdef CONFIG_X86
#include <asm/guest.h>
#endif
#ifdef CONFIG_SBSA_VUART_CONSOLE
#include <asm/vpl011.h>
#endif

/* Internal console flags. */
enum {
    CONSOLE_SERIAL          = BIT(0, U),    /* Use serial device. */
    CONSOLE_PV              = BIT(1, U),    /* Use PV console. */
    CONSOLE_VIDEO           = BIT(2, U),    /* Use video device. */
    CONSOLE_DEBUG           = BIT(3, U),    /* Use debug device. */
    CONSOLE_RING            = BIT(4, U),    /* Use console ring. */
    CONSOLE_RING_VIRQ       = BIT(5, U),    /* Use console ring VIRQ. */

    /* Default console flags. */
    CONSOLE_DEFAULT         = CONSOLE_SERIAL |
                              CONSOLE_PV |
                              CONSOLE_VIDEO |
                              CONSOLE_RING_VIRQ |
                              CONSOLE_DEBUG,

    /* Use all known console devices. */
    CONSOLE_ALL             = CONSOLE_DEFAULT | CONSOLE_RING,
};

/* Prefix for hypervisor's diagnostic console messages. */
#define CONSOLE_PREFIX      "<crux> "

static void console_send(const char *str, size_t len, unsigned int flags);

/* console: comma-separated list of console outputs. */
static char __initdata opt_console[30] = OPT_CONSOLE_STR;
string_param("console", opt_console);

/* conswitch: a character pair controlling console switching. */
/* Char 1: CTRL+<char1> is used to switch console input between crux and DOM0 */
/* Char 2: If this character is 'x', then do not auto-switch to DOM0 when it */
/*         boots. Any other value, or omitting the char, enables auto-switch */
static char __read_mostly opt_conswitch[3] = "a";
string_runtime_param("conswitch", opt_conswitch);

/* sync_console: force synchronous console output (useful for debugging). */
static bool __initdata opt_sync_console;
boolean_param("sync_console", opt_sync_console);
static const char __initconst warning_sync_console[] =
    "WARNING: CONSOLE OUTPUT IS SYNCHRONOUS\n"
    "This option is intended to aid debugging of crux by ensuring\n"
    "that all output is synchronously delivered on the serial line.\n"
    "However it can introduce SIGNIFICANT latencies and affect\n"
    "timekeeping. It is NOT recommended for production use!\n";

/* console_to_ring: send guest (incl. dom 0) console data to console ring. */
static bool __read_mostly opt_console_to_ring;
boolean_param("console_to_ring", opt_console_to_ring);

/* console_timestamps: include a timestamp prefix on every crux console line. */
enum con_timestamp_mode
{
    TSM_NONE,          /* No timestamps */
    TSM_DATE,          /* [YYYY-MM-DD HH:MM:SS] */
    TSM_DATE_MS,       /* [YYYY-MM-DD HH:MM:SS.mmm] */
    TSM_BOOT,          /* [SSSSSS.uuuuuu] */
    TSM_RAW,           /* [XXXXXXXXXXXXXXXX] */
};

static enum con_timestamp_mode __read_mostly opt_con_timestamp_mode = TSM_NONE;

#ifdef CONFIG_HYPFS
static const char con_timestamp_mode_2_string[][7] = {
    [TSM_NONE] = "none",
    [TSM_DATE] = "date",
    [TSM_DATE_MS] = "datems",
    [TSM_BOOT] = "boot",
    [TSM_RAW] = "raw",
};

static void cf_check con_timestamp_mode_upd(struct param_hypfs *par)
{
    const char *val = con_timestamp_mode_2_string[opt_con_timestamp_mode];

    custom_runtime_set_var_sz(par, val, 7);
}
#else
#define con_timestamp_mode_upd(par)
#endif

static int cf_check parse_console_timestamps(const char *s);
custom_runtime_param("console_timestamps", parse_console_timestamps,
                     con_timestamp_mode_upd);

/* conring_size: allows a large console ring than default (16kB). */
static uint32_t __initdata opt_conring_size;
size_param("conring_size", opt_conring_size);

#define _CONRING_SIZE 16384
#define CONRING_IDX_MASK(i) ((i)&(conring_size-1))
static char __initdata _conring[_CONRING_SIZE];
static char *__read_mostly conring = _conring;
static uint32_t __read_mostly conring_size = _CONRING_SIZE;
static uint32_t conringc, conringp;

static int __read_mostly sercon_handle = -1;

#ifdef CONFIG_X86
/* Tristate: 0 disabled, 1 user enabled, -1 default enabled */
int8_t __read_mostly opt_console_crux; /* console=crux */
#endif

static DEFINE_RSPINLOCK(console_lock);

/*
 * To control the amount of printing, thresholds are added.
 * These thresholds correspond to the CRUXLOG logging levels.
 * There's an upper and lower threshold for non-guest messages and for
 * guest-provoked messages.  This works as follows, for a given log level L:
 *
 * L < lower_threshold                     : always logged
 * lower_threshold <= L < upper_threshold  : rate-limited logging
 * upper_threshold <= L                    : never logged
 *
 * Note, in the above algorithm, to disable rate limiting simply make
 * the lower threshold equal to the upper.
 */
#ifdef NDEBUG
#define CRUXLOG_UPPER_THRESHOLD       3 /* Do not print DEBUG  */
#define CRUXLOG_LOWER_THRESHOLD       3 /* Always print INFO, ERR and WARNING */
#define CRUXLOG_GUEST_UPPER_THRESHOLD 2 /* Do not print INFO and DEBUG  */
#define CRUXLOG_GUEST_LOWER_THRESHOLD 0 /* Rate-limit ERR and WARNING   */
#else
#define CRUXLOG_UPPER_THRESHOLD       4 /* Do not discard anything      */
#define CRUXLOG_LOWER_THRESHOLD       4 /* Print everything             */
#define CRUXLOG_GUEST_UPPER_THRESHOLD 4 /* Do not discard anything      */
#define CRUXLOG_GUEST_LOWER_THRESHOLD 4 /* Print everything             */
#endif
/*
 * The CRUXLOG_DEFAULT is the default given to printks that
 * do not have any print level associated with them.
 */
#define CRUXLOG_DEFAULT       2 /* CRUXLOG_INFO */
#define CRUXLOG_GUEST_DEFAULT 1 /* CRUXLOG_WARNING */

static int __read_mostly cruxlog_upper_thresh = CRUXLOG_UPPER_THRESHOLD;
static int __read_mostly cruxlog_lower_thresh = CRUXLOG_LOWER_THRESHOLD;
static int __read_mostly cruxlog_guest_upper_thresh =
    CRUXLOG_GUEST_UPPER_THRESHOLD;
static int __read_mostly cruxlog_guest_lower_thresh =
    CRUXLOG_GUEST_LOWER_THRESHOLD;

static int cf_check parse_loglvl(const char *s);
static int cf_check parse_guest_loglvl(const char *s);

#ifdef CONFIG_HYPFS
#define LOGLVL_VAL_SZ 16
static char cruxlog_val[LOGLVL_VAL_SZ];
static char cruxlog_guest_val[LOGLVL_VAL_SZ];

static void cruxlog_update_val(int lower, int upper, char *val)
{
    static const char * const lvl2opt[] =
        { "none", "error", "warning", "info", "all" };

    snprintf(val, LOGLVL_VAL_SZ, "%s/%s", lvl2opt[lower], lvl2opt[upper]);
}

static void __init cf_check cruxlog_init(struct param_hypfs *par)
{
    cruxlog_update_val(cruxlog_lower_thresh, cruxlog_upper_thresh, cruxlog_val);
    custom_runtime_set_var(par, cruxlog_val);
}

static void __init cf_check cruxlog_guest_init(struct param_hypfs *par)
{
    cruxlog_update_val(cruxlog_guest_lower_thresh, cruxlog_guest_upper_thresh,
                      cruxlog_guest_val);
    custom_runtime_set_var(par, cruxlog_guest_val);
}
#else
#define cruxlog_val       NULL
#define cruxlog_guest_val NULL

static void cruxlog_update_val(int lower, int upper, char *val)
{
}
#endif

/*
 * <lvl> := none|error|warning|info|debug|all
 * loglvl=<lvl_print_always>[/<lvl_print_ratelimit>]
 *  <lvl_print_always>: log level which is always printed
 *  <lvl_print_rlimit>: log level which is rate-limit printed
 * Similar definitions for guest_loglvl, but applies to guest tracing.
 * Defaults: loglvl=warning ; guest_loglvl=none/warning
 */
custom_runtime_param("loglvl", parse_loglvl, cruxlog_init);
custom_runtime_param("guest_loglvl", parse_guest_loglvl, cruxlog_guest_init);

static atomic_t print_everything = ATOMIC_INIT(0);

#define ___parse_loglvl(s, ps, lvlstr, lvlnum)          \
    if ( !strncmp((s), (lvlstr), strlen(lvlstr)) ) {    \
        *(ps) = (s) + strlen(lvlstr);                   \
        return (lvlnum);                                \
    }

static int __parse_loglvl(const char *s, const char **ps)
{
    ___parse_loglvl(s, ps, "none",    0);
    ___parse_loglvl(s, ps, "error",   1);
    ___parse_loglvl(s, ps, "warning", 2);
    ___parse_loglvl(s, ps, "info",    3);
    ___parse_loglvl(s, ps, "debug",   4);
    ___parse_loglvl(s, ps, "all",     4);
    return 2; /* sane fallback */
}

static int _parse_loglvl(const char *s, int *lower, int *upper, char *val)
{
    *lower = *upper = __parse_loglvl(s, &s);
    if ( *s == '/' )
        *upper = __parse_loglvl(s+1, &s);
    if ( *upper < *lower )
        *upper = *lower;

    cruxlog_update_val(*lower, *upper, val);

    return *s ? -EINVAL : 0;
}

static int cf_check parse_loglvl(const char *s)
{
    int ret;

    ret = _parse_loglvl(s, &cruxlog_lower_thresh, &cruxlog_upper_thresh,
                        cruxlog_val);
    custom_runtime_set_var(param_2_parfs(parse_loglvl), cruxlog_val);

    return ret;
}

static int cf_check parse_guest_loglvl(const char *s)
{
    int ret;

    ret = _parse_loglvl(s, &cruxlog_guest_lower_thresh,
                        &cruxlog_guest_upper_thresh, cruxlog_guest_val);
    custom_runtime_set_var(param_2_parfs(parse_guest_loglvl),
                           cruxlog_guest_val);

    return ret;
}

static const char *loglvl_str(int lvl)
{
    switch ( lvl )
    {
    case 0: return "Nothing";
    case 1: return "Errors";
    case 2: return "Errors and warnings";
    case 3: return "Errors, warnings and info";
    case 4: return "All";
    }
    return "???";
}

static int *__read_mostly upper_thresh_adj = &cruxlog_upper_thresh;
static int *__read_mostly lower_thresh_adj = &cruxlog_lower_thresh;
static const char *__read_mostly thresh_adj = "standard";

static void cf_check do_toggle_guest(unsigned char key, bool unused)
{
    if ( upper_thresh_adj == &cruxlog_upper_thresh )
    {
        upper_thresh_adj = &cruxlog_guest_upper_thresh;
        lower_thresh_adj = &cruxlog_guest_lower_thresh;
        thresh_adj = "guest";
    }
    else
    {
        upper_thresh_adj = &cruxlog_upper_thresh;
        lower_thresh_adj = &cruxlog_lower_thresh;
        thresh_adj = "standard";
    }
    printk("'%c' pressed -> %s log level adjustments enabled\n",
           key, thresh_adj);
}

static void do_adj_thresh(unsigned char key)
{
    if ( *upper_thresh_adj < *lower_thresh_adj )
        *upper_thresh_adj = *lower_thresh_adj;
    printk("'%c' pressed -> %s log level: %s (rate limited %s)\n",
           key, thresh_adj, loglvl_str(*lower_thresh_adj),
           loglvl_str(*upper_thresh_adj));
}

static void cf_check do_inc_thresh(unsigned char key, bool unused)
{
    ++*lower_thresh_adj;
    do_adj_thresh(key);
}

static void cf_check do_dec_thresh(unsigned char key, bool unused)
{
    if ( *lower_thresh_adj )
        --*lower_thresh_adj;
    do_adj_thresh(key);
}

/*
 * ********************************************************
 * *************** ACCESS TO CONSOLE RING *****************
 * ********************************************************
 */

static void cf_check conring_notify(void *unused)
{
    send_global_virq(VIRQ_CON_RING);
}

static DECLARE_SOFTIRQ_TASKLET(conring_tasklet, conring_notify, NULL);

/* NB: Do not send conring VIRQs during panic. */
static bool conring_no_notify;

static void conring_puts(const char *str, size_t len)
{
    ASSERT(rspin_is_locked(&console_lock));

    while ( len-- )
        conring[CONRING_IDX_MASK(conringp++)] = *str++;

    if ( conringp - conringc > conring_size )
        conringc = conringp - conring_size;
}

#ifdef CONFIG_SYSCTL
long read_console_ring(struct crux_sysctl_readconsole *op)
{
    CRUX_GUEST_HANDLE_PARAM(char) str;
    uint32_t idx, len, max, sofar, c, p;

    str   = guest_handle_cast(op->buffer, char),
    max   = op->count;
    sofar = 0;

    c = read_atomic(&conringc);
    p = read_atomic(&conringp);
    if ( op->incremental &&
         (c <= p ? c < op->index && op->index <= p
                 : c < op->index || op->index <= p) )
        c = op->index;

    while ( (c != p) && (sofar < max) )
    {
        idx = CONRING_IDX_MASK(c);
        len = p - c;
        if ( (idx + len) > conring_size )
            len = conring_size - idx;
        if ( (sofar + len) > max )
            len = max - sofar;
        if ( copy_to_guest_offset(str, sofar, &conring[idx], len) )
            return -EFAULT;
        sofar += len;
        c += len;
    }

    if ( op->clear )
    {
        nrspin_lock_irq(&console_lock);
        conringc = p - c > conring_size ? p - conring_size : c;
        nrspin_unlock_irq(&console_lock);
    }

    op->count = sofar;
    op->index = c;

    return 0;
}
#endif /* CONFIG_SYSCTL */


/*
 * *******************************************************
 * *************** ACCESS TO SERIAL LINE *****************
 * *******************************************************
 */

/* Characters received over the serial line are buffered for domain 0. */
#define SERIAL_RX_SIZE 128
#define SERIAL_RX_MASK(_i) ((_i)&(SERIAL_RX_SIZE-1))
static char serial_rx_ring[SERIAL_RX_SIZE];
static unsigned int serial_rx_cons, serial_rx_prod;

static void (*serial_steal_fn)(const char *str, size_t nr) = early_puts;

int console_steal(int handle, void (*fn)(const char *str, size_t nr))
{
    if ( (handle == -1) || (handle != sercon_handle) )
        return 0;

    if ( serial_steal_fn != NULL )
        return -EBUSY;

    serial_steal_fn = fn;
    return 1;
}

void console_giveback(int id)
{
    if ( id == 1 )
        serial_steal_fn = NULL;
}

void console_serial_puts(const char *s, size_t nr)
{
    if ( serial_steal_fn != NULL )
        serial_steal_fn(s, nr);
    else
        serial_puts(sercon_handle, s, nr);
}

/*
 * Flush contents of the conring to the selected console devices.
 */
static int conring_flush(unsigned int flags)
{
    uint32_t idx, len, sofar, c;
    unsigned int order;
    char *buf;

    order = get_order_from_bytes(conring_size + 1);
    buf = alloc_cruxheap_pages(order, 0);
    if ( buf == NULL )
        return -ENOMEM;

    c = conringc;
    sofar = 0;
    while ( (c != conringp) )
    {
        idx = CONRING_IDX_MASK(c);
        len = conringp - c;
        if ( (idx + len) > conring_size )
            len = conring_size - idx;
        memcpy(buf + sofar, &conring[idx], len);
        sofar += len;
        c += len;
    }

    console_send(buf, sofar, flags);

    free_cruxheap_pages(buf, order);

    return 0;
}

static void cf_check conring_dump_keyhandler(unsigned char key)
{
    int rc;

    printk("'%c' pressed -> dumping console ring buffer (dmesg)\n", key);
    rc = conring_flush(CONSOLE_SERIAL | CONSOLE_VIDEO | CONSOLE_PV);
    if ( rc )
        printk("failed to dump console ring buffer: %d\n", rc);
}

/*
 * CTRL-<switch_char> changes input direction, rotating among crux, Dom0,
 * and the DomUs started from crux at boot.
 */
#define switch_code (opt_conswitch[0]-'a'+1)
/*
 * console_rx=0 => input to crux
 * console_rx=1 => input to dom0 (or the sole shim domain)
 * console_rx=N => input to dom(N-1)
 */
static unsigned int __read_mostly console_rx = 0;

#define max_console_rx (max_init_domid + 1)

struct domain *console_get_domain(void)
{
    struct domain *d;

    if ( console_rx == 0 )
            return NULL;

    d = rcu_lock_domain_by_id(console_rx - 1);
    if ( !d )
        return NULL;

    if ( d->console.input_allowed )
        return d;

    rcu_unlock_domain(d);

    return NULL;
}

void console_put_domain(struct domain *d)
{
    if ( d )
        rcu_unlock_domain(d);
}

static void console_switch_input(void)
{
    unsigned int next_rx = console_rx;

    /*
     * Rotate among crux, dom0 and boot-time created domUs while skipping
     * switching serial input to non existing domains.
     */
    for ( ; ; )
    {
        domid_t domid;
        struct domain *d;

        if ( next_rx++ >= max_console_rx )
        {
            console_rx = 0;
            printk("### Serial input to crux");
            break;
        }

        if ( consoled_is_enabled() && next_rx == 1 )
            domid = get_initial_domain_id();
        else
            domid = next_rx - 1;
        d = rcu_lock_domain_by_id(domid);
        if ( d )
        {
            rcu_unlock_domain(d);

            if ( !d->console.input_allowed )
                continue;

            console_rx = next_rx;
            printk("### Serial input to dom%u", domid);
            break;
        }
    }

    if ( switch_code )
        printk(" (type 'CTRL-%c' three times to switch input)",
               opt_conswitch[0]);
    printk("\n");
}

static void __serial_rx(char c)
{
    struct domain *d;
    int rc = 0;

    if ( console_rx == 0 )
        return handle_keypress(c, false);

    d = console_get_domain();
    if ( !d )
        return;

    if ( is_hardware_domain(d) )
    {
        /*
         * Deliver input to the hardware domain buffer, unless it is
         * already full.
         */
        if ( (serial_rx_prod - serial_rx_cons) != SERIAL_RX_SIZE )
            serial_rx_ring[SERIAL_RX_MASK(serial_rx_prod++)] = c;

        /*
         * Always notify the hardware domain: prevents receive path from
         * getting stuck.
         */
        send_global_virq(VIRQ_CONSOLE);
    }
#ifdef CONFIG_SBSA_VUART_CONSOLE
    else
        /* Deliver input to the emulated UART. */
        rc = vpl011_rx_char_crux(d, c);
#endif

    if ( consoled_is_enabled() )
        /* Deliver input to the PV shim console. */
        rc = consoled_guest_tx(c);

    if ( rc )
        guest_printk(d,
                     CRUXLOG_WARNING "failed to process console input: %d\n",
                     rc);

    console_put_domain(d);
}

static void cf_check serial_rx(char c)
{
    static int switch_code_count = 0;

    if ( switch_code && (c == switch_code) )
    {
        /* We eat CTRL-<switch_char> in groups of 3 to switch console input. */
        if ( ++switch_code_count == 3 )
        {
            console_switch_input();
            switch_code_count = 0;
        }
        return;
    }

    for ( ; switch_code_count != 0; switch_code_count-- )
        __serial_rx(switch_code);

    /* Finally process the just-received character. */
    __serial_rx(c);
}

#ifdef CONFIG_X86
static inline void crux_console_write_debug_port(const char *buf, size_t len)
{
    unsigned long tmp;
    asm volatile ( "rep outsb;"
                   : "=&S" (tmp), "=&c" (tmp)
                   : "0" (buf), "1" (len), "d" (CRUX_HVM_DEBUGCONS_IOPORT) );
}
#endif

static inline void console_debug_puts(const char *str, size_t len)
{
#ifdef CONFIG_X86
    if ( opt_console_crux )
    {
        if ( crux_guest )
            crux_hypercall_console_write(str, len);
        else
            crux_console_write_debug_port(str, len);
    }
#endif
}

/*
 * Send a message on console device(s).
 *
 * That will handle all possible scenarios working w/ console
 * - physical console (serial console, VGA console (x86 only));
 * - PV console;
 * - debug console (x86 only): debug I/O port or __HYPERVISOR_console_io
 *   hypercall;
 * - console ring.
 */
static void console_send(const char *str, size_t len, unsigned int flags)
{
    if ( flags & CONSOLE_SERIAL )
        console_serial_puts(str, len);

    if ( flags & CONSOLE_PV )
        pv_console_puts(str, len);

    if ( flags & CONSOLE_VIDEO )
        video_puts(str, len);

    if ( flags & CONSOLE_DEBUG )
        console_debug_puts(str, len);

    if ( flags & CONSOLE_RING )
    {
        conring_puts(str, len);

        if ( flags & CONSOLE_RING_VIRQ )
            tasklet_schedule(&conring_tasklet);
    }
}

static inline void __putstr(const char *str)
{
    unsigned int flags = CONSOLE_ALL;

    ASSERT(rspin_is_locked(&console_lock));

    if ( conring_no_notify )
        flags &= ~CONSOLE_RING_VIRQ;

    console_send(str, strlen(str), flags);
}

static long guest_console_write(CRUX_GUEST_HANDLE_PARAM(char) buffer,
                                unsigned int count)
{
    char kbuf[128];
    unsigned int kcount = 0;
    unsigned int flags = opt_console_to_ring
                         ? CONSOLE_ALL : CONSOLE_DEFAULT;
    struct domain *cd = current->domain;

    while ( count > 0 )
    {
        if ( kcount && hypercall_preempt_check() )
            return hypercall_create_continuation(
                __HYPERVISOR_console_io, "iih",
                CONSOLEIO_write, count, buffer);

        kcount = min((size_t)count, sizeof(kbuf) - 1);
        if ( copy_from_guest(kbuf, buffer, kcount) )
            return -EFAULT;

        if ( is_hardware_domain(cd) )
        {
            /* Use direct console output as it could be interactive */
            nrspin_lock_irq(&console_lock);
            console_send(kbuf, kcount, flags);
            nrspin_unlock_irq(&console_lock);
        }
        else
        {
            char *kin = kbuf, *kout = kbuf, c;

            /* Strip non-printable characters */
            do
            {
                c = *kin++;
                if ( c == '\n' )
                    break;
                if ( is_console_printable(c) )
                    *kout++ = c;
            } while ( --kcount > 0 );

            *kout = '\0';
            spin_lock(&cd->pbuf_lock);
            kcount = kin - kbuf;
            if ( c != '\n' &&
                 (cd->pbuf_idx + (kout - kbuf) < (DOMAIN_PBUF_SIZE - 1)) )
            {
                /* buffer the output until a newline */
                memcpy(cd->pbuf + cd->pbuf_idx, kbuf, kout - kbuf);
                cd->pbuf_idx += (kout - kbuf);
            }
            else
            {
                cd->pbuf[cd->pbuf_idx] = '\0';
                guest_printk(cd, CRUXLOG_G_DEBUG "%s%s\n", cd->pbuf, kbuf);
                cd->pbuf_idx = 0;
            }
            spin_unlock(&cd->pbuf_lock);
        }

        guest_handle_add_offset(buffer, kcount);
        count -= kcount;
    }

    return 0;
}

long do_console_io(
    unsigned int cmd, unsigned int count, CRUX_GUEST_HANDLE_PARAM(char) buffer)
{
    long rc;
    unsigned int idx, len;

    rc = xsm_console_io(XSM_OTHER, current->domain, cmd);
    if ( rc )
        return rc;

    switch ( cmd )
    {
    case CONSOLEIO_write:
        rc = guest_console_write(buffer, count);
        break;
    case CONSOLEIO_read:
        /*
         * The return value is either the number of characters read or
         * a negative value in case of error. So we need to prevent
         * overlap between the two sets.
         */
        rc = -E2BIG;
        if ( count > INT_MAX )
            break;

        rc = 0;
        while ( (serial_rx_cons != serial_rx_prod) && (rc < count) )
        {
            idx = SERIAL_RX_MASK(serial_rx_cons);
            len = serial_rx_prod - serial_rx_cons;
            if ( (idx + len) > SERIAL_RX_SIZE )
                len = SERIAL_RX_SIZE - idx;
            if ( (rc + len) > count )
                len = count - rc;
            if ( copy_to_guest_offset(buffer, rc, &serial_rx_ring[idx], len) )
            {
                rc = -EFAULT;
                break;
            }
            rc += len;
            serial_rx_cons += len;
        }
        break;
    default:
        rc = -ENOSYS;
        break;
    }

    return rc;
}


/*
 * *****************************************************
 * *************** GENERIC CONSOLE I/O *****************
 * *****************************************************
 */

static int printk_prefix_check(char *p, char **pp)
{
    int loglvl = -1;
    int upper_thresh = ACCESS_ONCE(cruxlog_upper_thresh);
    int lower_thresh = ACCESS_ONCE(cruxlog_lower_thresh);

    while ( (p[0] == '<') && (p[1] != '\0') && (p[2] == '>') )
    {
        switch ( p[1] )
        {
        case 'G':
            upper_thresh = ACCESS_ONCE(cruxlog_guest_upper_thresh);
            lower_thresh = ACCESS_ONCE(cruxlog_guest_lower_thresh);
            if ( loglvl == -1 )
                loglvl = CRUXLOG_GUEST_DEFAULT;
            break;
        case '0' ... '3':
            loglvl = p[1] - '0';
            break;
        }
        p += 3;
    }

    if ( loglvl == -1 )
        loglvl = CRUXLOG_DEFAULT;

    *pp = p;

    return ((atomic_read(&print_everything) != 0) ||
            (loglvl < lower_thresh) ||
            ((loglvl < upper_thresh) && printk_ratelimit()));
}

static int cf_check parse_console_timestamps(const char *s)
{
    switch ( parse_bool(s, NULL) )
    {
    case 0:
        opt_con_timestamp_mode = TSM_NONE;
        con_timestamp_mode_upd(param_2_parfs(parse_console_timestamps));
        return 0;
    case 1:
        opt_con_timestamp_mode = TSM_DATE;
        con_timestamp_mode_upd(param_2_parfs(parse_console_timestamps));
        return 0;
    }
    if ( *s == '\0' || /* Compat for old booleanparam() */
         !strcmp(s, "date") )
        opt_con_timestamp_mode = TSM_DATE;
    else if ( !strcmp(s, "datems") )
        opt_con_timestamp_mode = TSM_DATE_MS;
    else if ( !strcmp(s, "boot") )
        opt_con_timestamp_mode = TSM_BOOT;
    else if ( !strcmp(s, "raw") )
        opt_con_timestamp_mode = TSM_RAW;
    else if ( !strcmp(s, "none") )
        opt_con_timestamp_mode = TSM_NONE;
    else
        return -EINVAL;

    con_timestamp_mode_upd(param_2_parfs(parse_console_timestamps));

    return 0;
}

static void printk_start_of_line(const char *prefix)
{
    enum con_timestamp_mode mode = ACCESS_ONCE(opt_con_timestamp_mode);
    struct tm tm;
    char tstr[32];
    uint64_t sec, nsec;

    __putstr(prefix);

    switch ( mode )
    {
    case TSM_DATE:
    case TSM_DATE_MS:
        tm = wallclock_time(&nsec);

        if ( tm.tm_mday == 0 )
            /* nothing */;
        else if ( mode == TSM_DATE )
        {
            snprintf(tstr, sizeof(tstr), "[%04u-%02u-%02u %02u:%02u:%02u] ",
                     1900 + tm.tm_year, tm.tm_mon + 1, tm.tm_mday,
                     tm.tm_hour, tm.tm_min, tm.tm_sec);
            break;
        }
        else
        {
            snprintf(tstr, sizeof(tstr),
                     "[%04u-%02u-%02u %02u:%02u:%02u.%03"PRIu64"] ",
                     1900 + tm.tm_year, tm.tm_mon + 1, tm.tm_mday,
                     tm.tm_hour, tm.tm_min, tm.tm_sec, nsec / 1000000);
            break;
        }
        /* fall through */
    case TSM_BOOT:
        sec = NOW();
        nsec = do_div(sec, 1000000000);

        if ( sec | nsec )
        {
            snprintf(tstr, sizeof(tstr), "[%5"PRIu64".%06"PRIu64"] ",
                     sec, nsec / 1000);
            break;
        }
        /* fall through */
    case TSM_RAW:
        snprintf(tstr, sizeof(tstr), "[%016"PRIx64"] ", get_cycles());
        break;

    case TSM_NONE:
    default:
        return;
    }

    __putstr(tstr);
}

static void vprintk_common(const char *fmt, va_list args, const char *prefix)
{
    struct vps {
        bool continued, do_print;
    }            *state;
    static DEFINE_PER_CPU(struct vps, state);
    static char   buf[1024];
    char         *p, *q;
    unsigned long flags;

    /* console_lock can be acquired recursively from __printk_ratelimit(). */
    local_irq_save(flags);
    rspin_lock(&console_lock);
    state = &this_cpu(state);

    (void)vsnprintf(buf, sizeof(buf), fmt, args);

    p = buf;

    while ( (q = strchr(p, '\n')) != NULL )
    {
        *q = '\0';
        if ( !state->continued )
            state->do_print = printk_prefix_check(p, &p);
        if ( state->do_print )
        {
            if ( !state->continued )
                printk_start_of_line(prefix);
            __putstr(p);
            __putstr("\n");
        }
        state->continued = 0;
        p = q + 1;
    }

    if ( *p != '\0' )
    {
        if ( !state->continued )
            state->do_print = printk_prefix_check(p, &p);
        if ( state->do_print )
        {
            if ( !state->continued && (console_rx < 2) )
                printk_start_of_line(prefix);
            __putstr(p);
        }
        state->continued = 1;
    }

    rspin_unlock(&console_lock);
    local_irq_restore(flags);
}

void vprintk(const char *fmt, va_list args)
{
    vprintk_common(fmt, args, CONSOLE_PREFIX);
}

void printk(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
}

void guest_printk(const struct domain *d, const char *fmt, ...)
{
    va_list args;
    char prefix[16];

    snprintf(prefix, sizeof(prefix), "(d%d) ", d->domain_id);

    va_start(args, fmt);
    vprintk_common(fmt, args, prefix);
    va_end(args);
}

void __init console_init_preirq(void)
{
    char *p;
    int sh;
    unsigned int flags = CONSOLE_SERIAL | CONSOLE_VIDEO | CONSOLE_PV;

    serial_init_preirq();

    /* Where should console output go? */
    for ( p = opt_console; p != NULL; p = strchr(p, ',') )
    {
        if ( *p == ',' )
            p++;
        if ( !strncmp(p, "vga", 3) )
            video_init();
        else if ( !strncmp(p, "pv", 2) )
            pv_console_init();
#ifdef CONFIG_X86
        else if ( !strncmp(p, "crux", 3) )
            opt_console_crux = 1;
#endif
        else if ( !strncmp(p, "none", 4) )
            continue;
        else if ( (sh = serial_parse_handle(p)) >= 0 )
        {
            sercon_handle = sh;
            serial_steal_fn = NULL;
        }
        else
        {
            char *q = strchr(p, ',');
            if ( q != NULL )
                *q = '\0';
            printk("Bad console= option '%s'\n", p);
            if ( q != NULL )
                *q = ',';
        }
    }

#ifdef CONFIG_X86
    if ( opt_console_crux == -1 )
        opt_console_crux = 0;
#endif

    serial_set_rx_handler(sercon_handle, serial_rx);
    pv_console_set_rx_handler(serial_rx);

    /*
     * NB: send conring contents to all enabled physical consoles, if any.
     * Skip serial if CONFIG_EARLY_PRINTK is enabled, which means the early
     * messages have already been sent to serial.
     */
    if ( IS_ENABLED(CONFIG_EARLY_PRINTK) )
        flags &= ~CONSOLE_SERIAL;

    conring_flush(flags);

    /* HELLO WORLD --- start-of-day banner text. */
    nrspin_lock(&console_lock);
    __putstr(crux_banner());
    nrspin_unlock(&console_lock);

    /* Locate the buildid, if possible. */
    crux_build_init();

    print_version();

    if ( opt_sync_console )
    {
        serial_start_sync(sercon_handle);
        add_taint(TAINT_SYNC_CONSOLE);
        printk("Console output is synchronous.\n");
        warning_add(warning_sync_console);
    }
}

void __init console_init_ring(void)
{
    char *ring;
    unsigned int i, order, memflags;
    unsigned long flags;

    if ( !opt_conring_size )
        return;

    order = get_order_from_bytes(max(opt_conring_size, conring_size));
    memflags = MEMF_bits(crashinfo_maxaddr_bits);
    while ( (ring = alloc_cruxheap_pages(order, memflags)) == NULL )
    {
        BUG_ON(order == 0);
        order--;
    }
    opt_conring_size = PAGE_SIZE << order;

    nrspin_lock_irqsave(&console_lock, flags);
    for ( i = conringc ; i != conringp; i++ )
        ring[i & (opt_conring_size - 1)] = conring[i & (conring_size - 1)];
    conring = ring;
    smp_wmb(); /* Allow users of console_force_unlock() to see larger buffer. */
    conring_size = opt_conring_size;
    nrspin_unlock_irqrestore(&console_lock, flags);

    printk("Allocated console ring of %u KiB.\n", opt_conring_size >> 10);
}

void __init console_init_irq(void)
{
    serial_init_irq();
}

void __init console_init_postirq(void)
{
    serial_init_postirq();
    pv_console_init_postirq();

    if ( conring != _conring )
        return;

    if ( !opt_conring_size )
        opt_conring_size = num_present_cpus() << (9 + cruxlog_lower_thresh);

    console_init_ring();
}

void __init console_endboot(void)
{
    printk("Std. Loglevel: %s", loglvl_str(cruxlog_lower_thresh));
    if ( cruxlog_upper_thresh != cruxlog_lower_thresh )
        printk(" (Rate-limited: %s)", loglvl_str(cruxlog_upper_thresh));
    printk("\nGuest Loglevel: %s", loglvl_str(cruxlog_guest_lower_thresh));
    if ( cruxlog_guest_upper_thresh != cruxlog_guest_lower_thresh )
        printk(" (Rate-limited: %s)", loglvl_str(cruxlog_guest_upper_thresh));
    printk("\n");

    warning_print();

    video_endboot();

    /*
     * If user specifies so, we fool the switch routine to redirect input
     * straight back to crux. I use this convoluted method so we still print
     * a useful 'how to switch' message.
     */
    if ( opt_conswitch[1] == 'x' )
        console_rx = max_console_rx;

    register_keyhandler('w', conring_dump_keyhandler,
                        "synchronously dump console ring buffer (dmesg)", 0);
    register_irq_keyhandler('+', &do_inc_thresh,
                            "increase log level threshold", 0);
    register_irq_keyhandler('-', &do_dec_thresh,
                            "decrease log level threshold", 0);
    register_irq_keyhandler('G', &do_toggle_guest,
                            "toggle host/guest log level adjustment", 0);

    /* Serial input is directed to DOM0 by default. */
    console_switch_input();
}

int __init console_has(const char *device)
{
    char *p;

    for ( p = opt_console; p != NULL; p = strchr(p, ',') )
    {
        if ( *p == ',' )
            p++;
        if ( strncmp(p, device, strlen(device)) == 0 )
            return 1;
    }

    return 0;
}

void console_start_log_everything(void)
{
    serial_start_log_everything(sercon_handle);
    atomic_inc(&print_everything);
}

void console_end_log_everything(void)
{
    serial_end_log_everything(sercon_handle);
    atomic_dec(&print_everything);
}

unsigned long console_lock_recursive_irqsave(void)
{
    return rspin_lock_irqsave(&console_lock);
}

void console_unlock_recursive_irqrestore(unsigned long flags)
{
    rspin_unlock_irqrestore(&console_lock, flags);
}

void console_force_unlock(void)
{
    watchdog_disable();
    spin_debug_disable();
    rspin_lock_init(&console_lock);
    serial_force_unlock(sercon_handle);
    conring_no_notify = true;
    console_start_sync();
}

void console_start_sync(void)
{
    atomic_inc(&print_everything);
    serial_start_sync(sercon_handle);
}

void console_end_sync(void)
{
    serial_end_sync(sercon_handle);
    atomic_dec(&print_everything);
}

/*
 * printk rate limiting, lifted from Linux.
 *
 * This enforces a rate limit: not more than one kernel message
 * every printk_ratelimit_ms (millisecs).
 */
int __printk_ratelimit(unsigned int ratelimit_ms, unsigned int ratelimit_burst)
{
    static DEFINE_SPINLOCK(ratelimit_lock);
    static unsigned long toks = 10 * 5 * 1000;
    static unsigned long last_msg;
    static unsigned int missed;
    unsigned long flags;
    unsigned long long now = NOW(); /* ns */
    unsigned long ms;

    do_div(now, 1000000);
    ms = (unsigned long)now;

    spin_lock_irqsave(&ratelimit_lock, flags);
    toks += ms - last_msg;
    last_msg = ms;
    if ( toks > (ratelimit_burst * ratelimit_ms))
        toks = ratelimit_burst * ratelimit_ms;
    if ( toks >= ratelimit_ms )
    {
        unsigned int lost = missed;

        missed = 0;
        toks -= ratelimit_ms;
        spin_unlock(&ratelimit_lock);
        if ( lost )
        {
            char lost_str[10];

            snprintf(lost_str, sizeof(lost_str), "%u", lost);
            /* console_lock may already be acquired by printk(). */
            rspin_lock(&console_lock);
            printk_start_of_line(CONSOLE_PREFIX);
            __putstr("printk: ");
            __putstr(lost_str);
            __putstr(" messages suppressed.\n");
            rspin_unlock(&console_lock);
        }
        local_irq_restore(flags);
        return 1;
    }
    missed++;
    spin_unlock_irqrestore(&ratelimit_lock, flags);
    return 0;
}

/* Minimum time in ms between messages */
static const unsigned int printk_ratelimit_ms = 5 * 1000;

/* Number of messages we send before ratelimiting */
static const unsigned int printk_ratelimit_burst = 10;

int printk_ratelimit(void)
{
    return __printk_ratelimit(printk_ratelimit_ms, printk_ratelimit_burst);
}

/*
 * **************************************************************
 * ********************** Error-report **************************
 * **************************************************************
 */

void panic(const char *fmt, ...)
{
    va_list args;
    unsigned long flags;
    static DEFINE_SPINLOCK(lock);

    spin_debug_disable();
    spinlock_profile_printall('\0');
    debugtrace_dump();

    /* Ensure multi-line message prints atomically. */
    spin_lock_irqsave(&lock, flags);

    console_start_sync();
    printk("\n****************************************\n");
    printk("Panic on CPU %d:\n", smp_processor_id());

    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);

    printk("****************************************\n\n");
    if ( opt_noreboot )
        printk("Manual reset required ('noreboot' specified)\n");
    else
#ifdef CONFIG_X86
        printk("%s in five seconds...\n", pv_shim ? "Crash" : "Reboot");
#else
        printk("Reboot in five seconds...\n");
#endif

    spin_unlock_irqrestore(&lock, flags);

    kexec_crash(CRASHREASON_PANIC);

    if ( opt_noreboot )
        machine_halt();
    else
        machine_restart(5000);
}

#ifdef CONFIG_SYSTEM_SUSPEND

/*
 * **************************************************************
 * ****************** Console suspend/resume ********************
 * **************************************************************
 */

static void cf_check suspend_steal_fn(const char *str, size_t nr) { }
static int suspend_steal_id;

int console_suspend(void)
{
    suspend_steal_id = console_steal(sercon_handle, suspend_steal_fn);
    serial_suspend();
    return 0;
}

int console_resume(void)
{
    serial_resume();
    console_giveback(suspend_steal_id);
    return 0;
}

#endif /* CONFIG_SYSTEM_SUSPEND */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */

