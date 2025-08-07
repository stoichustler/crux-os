/*
 * Copyright (C) 2005 Hewlett-Packard Co.
 * written by Aravind Menon & Jose Renato Santos
 *            (email: cruxoprof@groups.hp.com)
 *
 * arch generic cruxoprof and IA64 support.
 * dynamic map/unmap cruxoprof buffer support.
 * Copyright (c) 2006 Isaku Yamahata <yamahata at valinux co jp>
 *                    VA Linux Systems Japan K.K.
 */

#ifndef COMPAT
#include <crux/guest_access.h>
#include <crux/sched.h>
#include <crux/event.h>
#include <crux/cruxoprof.h>
#include <public/cruxoprof.h>
#include <crux/paging.h>
#include <xsm/xsm.h>
#include <crux/hypercall.h>

/* Override macros from asm/page.h to make them work with mfn_t */
#undef virt_to_mfn
#define virt_to_mfn(va) _mfn(__virt_to_mfn(va))

#define CRUXOPROF_DOMAIN_IGNORED    0
#define CRUXOPROF_DOMAIN_ACTIVE     1
#define CRUXOPROF_DOMAIN_PASSIVE    2

#define CRUXOPROF_IDLE              0
#define CRUXOPROF_INITIALIZED       1
#define CRUXOPROF_COUNTERS_RESERVED 2
#define CRUXOPROF_READY             3
#define CRUXOPROF_PROFILING         4

#ifndef CONFIG_COMPAT
#define CRUXOPROF_COMPAT(x) false
typedef struct cruxoprof_buf cruxoprof_buf_t;
#define cruxoprof_buf(d, b, field) ACCESS_ONCE((b)->field)
#else
#include <compat/cruxoprof.h>
#define CRUXOPROF_COMPAT(x) ((x)->is_compat)
typedef union {
    struct cruxoprof_buf native;
    struct compat_oprof_buf compat;
} cruxoprof_buf_t;
#define cruxoprof_buf(d, b, field) ACCESS_ONCE(*(!(d)->cruxoprof->is_compat \
                                                ? &(b)->native.field \
                                                : &(b)->compat.field))
#endif

/* Limit amount of pages used for shared buffer (per domain) */
#define MAX_OPROF_SHARED_PAGES 32

/* Lock protecting the following global state */
static DEFINE_SPINLOCK(cruxoprof_lock);

static DEFINE_SPINLOCK(pmu_owner_lock);
int pmu_owner = 0;
int pmu_hvm_refcount = 0;

struct cruxoprof_vcpu {
    int event_size;
    cruxoprof_buf_t *buffer;
};

struct cruxoprof {
    char *rawbuf;
    int npages;
    int nbuf;
    int bufsize;
    int domain_type;
#ifdef CONFIG_COMPAT
    bool is_compat;
#endif
    struct cruxoprof_vcpu *vcpu;
};

static struct domain *active_domains[MAX_OPROF_DOMAINS];
static int active_ready[MAX_OPROF_DOMAINS];
static unsigned int adomains;

static struct domain *passive_domains[MAX_OPROF_DOMAINS];
static unsigned int pdomains;

static unsigned int activated;
static struct domain *cruxoprof_primary_profiler;
static int cruxoprof_state = CRUXOPROF_IDLE;
static unsigned long backtrace_depth;

static u64 total_samples;
static u64 invalid_buffer_samples;
static u64 corrupted_buffer_samples;
static u64 lost_samples;
static u64 active_samples;
static u64 passive_samples;
static u64 idle_samples;
static u64 others_samples;

int acquire_pmu_ownership(int pmu_ownership)
{
    spin_lock(&pmu_owner_lock);
    if ( pmu_owner == PMU_OWNER_NONE )
    {
        pmu_owner = pmu_ownership;
        goto out;
    }

    if ( pmu_owner == pmu_ownership )
        goto out;

    spin_unlock(&pmu_owner_lock);
    return 0;
 out:
    if ( pmu_owner == PMU_OWNER_HVM )
        pmu_hvm_refcount++;
    spin_unlock(&pmu_owner_lock);
    return 1;
}

void release_pmu_ownership(int pmu_ownership)
{
    spin_lock(&pmu_owner_lock);
    if ( pmu_ownership == PMU_OWNER_HVM )
        pmu_hvm_refcount--;
    if ( !pmu_hvm_refcount )
        pmu_owner = PMU_OWNER_NONE;
    spin_unlock(&pmu_owner_lock);
}

int is_active(struct domain *d)
{
    struct cruxoprof *x = d->cruxoprof;
    return ((x != NULL) && (x->domain_type == CRUXOPROF_DOMAIN_ACTIVE));
}

int is_passive(struct domain *d)
{
    struct cruxoprof *x = d->cruxoprof;
    return ((x != NULL) && (x->domain_type == CRUXOPROF_DOMAIN_PASSIVE));
}

static int is_profiled(struct domain *d)
{
    return (is_active(d) || is_passive(d));
}

static void cruxoprof_reset_stat(void)
{
    total_samples = 0;
    invalid_buffer_samples = 0;
    corrupted_buffer_samples = 0;
    lost_samples = 0;
    active_samples = 0;
    passive_samples = 0;
    idle_samples = 0;
    others_samples = 0;
}

static void cruxoprof_reset_buf(struct domain *d)
{
    int j;
    cruxoprof_buf_t *buf;

    if ( d->cruxoprof == NULL )
    {
        printk("cruxoprof_reset_buf: ERROR - Unexpected "
               "Xenoprof NULL pointer \n");
        return;
    }

    for ( j = 0; j < d->max_vcpus; j++ )
    {
        buf = d->cruxoprof->vcpu[j].buffer;
        if ( buf != NULL )
        {
            cruxoprof_buf(d, buf, event_head) = 0;
            cruxoprof_buf(d, buf, event_tail) = 0;
        }
    }
}

static int
share_cruxoprof_page_with_guest(struct domain *d, mfn_t mfn, int npages)
{
    int i;

    /* Check if previous page owner has released the page. */
    for ( i = 0; i < npages; i++ )
    {
        struct page_info *page = mfn_to_page(mfn_add(mfn, i));

        if ( (page->count_info & (PGC_allocated|PGC_count_mask)) != 0 )
        {
            printk(CRUXLOG_G_INFO "dom%d mfn %#lx page->count_info %#lx\n",
                   d->domain_id, mfn_x(mfn_add(mfn, i)), page->count_info);
            return -EBUSY;
        }
        page_set_owner(page, NULL);
    }

    for ( i = 0; i < npages; i++ )
        share_crux_page_with_guest(mfn_to_page(mfn_add(mfn, i)), d, SHARE_rw);

    return 0;
}

static void
unshare_cruxoprof_page_with_guest(struct cruxoprof *x)
{
    int i, npages = x->npages;
    mfn_t mfn = virt_to_mfn(x->rawbuf);

    for ( i = 0; i < npages; i++ )
    {
        struct page_info *page = mfn_to_page(mfn_add(mfn, i));

        BUG_ON(page_get_owner(page) != current->domain);
        put_page_alloc_ref(page);
    }
}

static void
cruxoprof_shared_gmfn_with_guest(
    struct domain *d, unsigned long maddr, unsigned long gmaddr, int npages)
{
    int i;

    for ( i = 0; i < npages; i++, maddr += PAGE_SIZE, gmaddr += PAGE_SIZE )
    {
        BUG_ON(page_get_owner(maddr_to_page(maddr)) != d);
        if ( i == 0 )
            gdprintk(CRUXLOG_WARNING,
                     "cruxoprof unsupported with autotranslated guests\n");

    }
}

static int alloc_cruxoprof_struct(
    struct domain *d, int max_samples, int is_passive)
{
    struct vcpu *v;
    int nvcpu, npages, bufsize, max_bufsize;
    unsigned max_max_samples;
    int i;

    nvcpu = 0;
    for_each_vcpu ( d, v )
        nvcpu++;

    if ( !nvcpu )
        return -EINVAL;

    d->cruxoprof = xzalloc(struct cruxoprof);
    if ( d->cruxoprof == NULL )
    {
        printk("alloc_cruxoprof_struct(): memory allocation failed\n");
        return -ENOMEM;
    }

    d->cruxoprof->vcpu = xzalloc_array(struct cruxoprof_vcpu, d->max_vcpus);
    if ( d->cruxoprof->vcpu == NULL )
    {
        xfree(d->cruxoprof);
        d->cruxoprof = NULL;
        printk("alloc_cruxoprof_struct(): vcpu array allocation failed\n");
        return -ENOMEM;
    }

    bufsize = sizeof(struct cruxoprof_buf);
    i = sizeof(struct event_log);
#ifdef CONFIG_COMPAT
    d->cruxoprof->is_compat = is_pv_32bit_domain(is_passive ? hardware_domain : d);
    if ( CRUXOPROF_COMPAT(d->cruxoprof) )
    {
        bufsize = sizeof(struct compat_oprof_buf);
        i = sizeof(struct compat_event_log);
    }
#endif

    /* reduce max_samples if necessary to limit pages allocated */
    max_bufsize = (MAX_OPROF_SHARED_PAGES * PAGE_SIZE) / nvcpu;
    max_max_samples = ( (max_bufsize - bufsize) / i ) + 1;
    if ( (unsigned)max_samples > max_max_samples )
        max_samples = max_max_samples;

    bufsize += (max_samples - 1) * i;
    npages = (nvcpu * bufsize - 1) / PAGE_SIZE + 1;

    d->cruxoprof->rawbuf = alloc_cruxheap_pages(get_order_from_pages(npages), 0);
    if ( d->cruxoprof->rawbuf == NULL )
    {
        xfree(d->cruxoprof->vcpu);
        xfree(d->cruxoprof);
        d->cruxoprof = NULL;
        return -ENOMEM;
    }

    for ( i = 0; i < npages; ++i )
        clear_page(d->cruxoprof->rawbuf + i * PAGE_SIZE);

    d->cruxoprof->npages = npages;
    d->cruxoprof->nbuf = nvcpu;
    d->cruxoprof->bufsize = bufsize;
    d->cruxoprof->domain_type = CRUXOPROF_DOMAIN_IGNORED;

    /* Update buffer pointers for active vcpus */
    i = 0;
    for_each_vcpu ( d, v )
    {
        cruxoprof_buf_t *buf = (cruxoprof_buf_t *)
            &d->cruxoprof->rawbuf[i * bufsize];

        d->cruxoprof->vcpu[v->vcpu_id].event_size = max_samples;
        d->cruxoprof->vcpu[v->vcpu_id].buffer = buf;
        cruxoprof_buf(d, buf, event_size) = max_samples;
        cruxoprof_buf(d, buf, vcpu_id) = v->vcpu_id;

        i++;
        /* in the unlikely case that the number of active vcpus changes */
        if ( i >= nvcpu )
            break;
    }
    
    return 0;
}

void free_cruxoprof_pages(struct domain *d)
{
    struct cruxoprof *x;
    int order;

    x = d->cruxoprof;
    if ( x == NULL )
        return;

    if ( x->rawbuf != NULL )
    {
        order = get_order_from_pages(x->npages);
        free_cruxheap_pages(x->rawbuf, order);
    }

    xfree(x->vcpu);
    xfree(x);
    d->cruxoprof = NULL;
}

static int active_index(struct domain *d)
{
    int i;

    for ( i = 0; i < adomains; i++ )
        if ( active_domains[i] == d )
            return i;

    return -1;
}

static int set_active(struct domain *d)
{
    int ind;
    struct cruxoprof *x;

    ind = active_index(d);
    if ( ind < 0 )
        return -EPERM;

    x = d->cruxoprof;
    if ( x == NULL )
        return -EPERM;

    x->domain_type = CRUXOPROF_DOMAIN_ACTIVE;
    active_ready[ind] = 1;
    activated++;

    return 0;
}

static int reset_active(struct domain *d)
{
    int ind;
    struct cruxoprof *x;

    ind = active_index(d);
    if ( ind < 0 )
        return -EPERM;

    x = d->cruxoprof;
    if ( x == NULL )
        return -EPERM;

    x->domain_type = CRUXOPROF_DOMAIN_IGNORED;
    active_ready[ind] = 0;
    active_domains[ind] = NULL;
    activated--;
    put_domain(d);

    if ( activated <= 0 )
        adomains = 0;

    return 0;
}

static void reset_passive(struct domain *d)
{
    struct cruxoprof *x;

    if ( d == NULL )
        return;

    x = d->cruxoprof;
    if ( x == NULL )
        return;

    x->domain_type = CRUXOPROF_DOMAIN_IGNORED;
    unshare_cruxoprof_page_with_guest(x);
}

static void reset_active_list(void)
{
    int i;

    for ( i = 0; i < adomains; i++ )
        if ( active_ready[i] )
            reset_active(active_domains[i]);

    adomains = 0;
    activated = 0;
}

static void reset_passive_list(void)
{
    int i;

    for ( i = 0; i < pdomains; i++ )
    {
        reset_passive(passive_domains[i]);
        put_domain(passive_domains[i]);
        passive_domains[i] = NULL;
    }

    pdomains = 0;
}

static int add_active_list(domid_t domid)
{
    struct domain *d;

    if ( adomains >= MAX_OPROF_DOMAINS )
        return -E2BIG;

    d = get_domain_by_id(domid);
    if ( d == NULL )
        return -EINVAL;

    active_domains[adomains] = d;
    active_ready[adomains] = 0;
    adomains++;

    return 0;
}

static int add_passive_list(CRUX_GUEST_HANDLE_PARAM(void) arg)
{
    struct cruxoprof_passive passive;
    struct domain *d;
    int ret = 0;

    if ( pdomains >= MAX_OPROF_DOMAINS )
        return -E2BIG;

    if ( copy_from_guest(&passive, arg, 1) )
        return -EFAULT;

    d = get_domain_by_id(passive.domain_id);
    if ( d == NULL )
        return -EINVAL;

    if ( d->cruxoprof == NULL )
    {
        ret = alloc_cruxoprof_struct(d, passive.max_samples, 1);
        if ( ret < 0 )
        {
            put_domain(d);
            return -ENOMEM;
        }
    }

    ret = share_cruxoprof_page_with_guest(
        current->domain, virt_to_mfn(d->cruxoprof->rawbuf),
        d->cruxoprof->npages);
    if ( ret < 0 )
    {
        put_domain(d);
        return ret;
    }

    d->cruxoprof->domain_type = CRUXOPROF_DOMAIN_PASSIVE;
    passive.nbuf = d->cruxoprof->nbuf;
    passive.bufsize = d->cruxoprof->bufsize;
    if ( !paging_mode_translate(current->domain) )
        passive.buf_gmaddr = __pa(d->cruxoprof->rawbuf);
    else
        cruxoprof_shared_gmfn_with_guest(
            current->domain, __pa(d->cruxoprof->rawbuf),
            passive.buf_gmaddr, d->cruxoprof->npages);

    if ( __copy_to_guest(arg, &passive, 1) )
    {
        put_domain(d);
        return -EFAULT;
    }
    
    passive_domains[pdomains] = d;
    pdomains++;

    return ret;
}


/* Get space in the buffer */
static int cruxoprof_buf_space(int head, int tail, int size)
{
    return ((tail > head) ? 0 : size) + tail - head - 1;
}

/* Check for space and add a sample. Return 1 if successful, 0 otherwise. */
static int cruxoprof_add_sample(const struct domain *d,
                               const struct cruxoprof_vcpu *v,
                               uint64_t eip, int mode, int event)
{
    cruxoprof_buf_t *buf = v->buffer;
    int head, tail, size;

    head = cruxoprof_buf(d, buf, event_head);
    tail = cruxoprof_buf(d, buf, event_tail);
    size = v->event_size;
    
    /* make sure indexes in shared buffer are sane */
    if ( (head < 0) || (head >= size) || (tail < 0) || (tail >= size) )
    {
        corrupted_buffer_samples++;
        return 0;
    }

    if ( cruxoprof_buf_space(head, tail, size) > 0 )
    {
        cruxoprof_buf(d, buf, event_log[head].eip) = eip;
        cruxoprof_buf(d, buf, event_log[head].mode) = mode;
        cruxoprof_buf(d, buf, event_log[head].event) = event;
        head++;
        if ( head >= size )
            head = 0;
        
        cruxoprof_buf(d, buf, event_head) = head;
    }
    else
    {
        cruxoprof_buf(d, buf, lost_samples)++;
        lost_samples++;
        return 0;
    }

    return 1;
}

int cruxoprof_add_trace(struct vcpu *vcpu, uint64_t pc, int mode)
{
    struct domain *d = vcpu->domain;

    /* Do not accidentally write an escape code due to a broken frame. */
    if ( pc == CRUXOPROF_ESCAPE_CODE )
    {
        invalid_buffer_samples++;
        return 0;
    }

    return cruxoprof_add_sample(d, &d->cruxoprof->vcpu[vcpu->vcpu_id],
                               pc, mode, 0);
}

void cruxoprof_log_event(struct vcpu *vcpu, const struct cpu_user_regs *regs,
                        uint64_t pc, int mode, int event)
{
    struct domain *d = vcpu->domain;
    struct cruxoprof_vcpu *v;
    cruxoprof_buf_t *buf;

    total_samples++;

    /* Ignore samples of un-monitored domains. */
    if ( !is_profiled(d) )
    {
        others_samples++;
        return;
    }

    v = &d->cruxoprof->vcpu[vcpu->vcpu_id];
    if ( v->buffer == NULL )
    {
        invalid_buffer_samples++;
        return;
    }
    
    buf = v->buffer;

    /* Provide backtrace if requested. */
    if ( backtrace_depth > 0 )
    {
        if ( cruxoprof_buf_space(cruxoprof_buf(d, buf, event_head),
                                cruxoprof_buf(d, buf, event_tail),
                                v->event_size) < 2 )
        {
            cruxoprof_buf(d, buf, lost_samples)++;
            lost_samples++;
            return;
        }

        /* cruxoprof_add_sample() will increment lost_samples on failure */
        if ( !cruxoprof_add_sample(d, v, CRUXOPROF_ESCAPE_CODE, mode,
                                  CRUXOPROF_TRACE_BEGIN) )
            return;
    }

    if ( cruxoprof_add_sample(d, v, pc, mode, event) )
    {
        if ( is_active(vcpu->domain) )
            active_samples++;
        else
            passive_samples++;
        if ( mode == 0 )
            cruxoprof_buf(d, buf, user_samples)++;
        else if ( mode == 1 )
            cruxoprof_buf(d, buf, kernel_samples)++;
        else
            cruxoprof_buf(d, buf, crux_samples)++;
    
    }

    if ( backtrace_depth > 0 )
        cruxoprof_backtrace(vcpu, regs, backtrace_depth, mode);
}



static int cruxoprof_op_init(CRUX_GUEST_HANDLE_PARAM(void) arg)
{
    struct domain *d = current->domain;
    struct cruxoprof_init cruxoprof_init;
    int ret;

    if ( copy_from_guest(&cruxoprof_init, arg, 1) )
        return -EFAULT;

    if ( (ret = cruxoprof_arch_init(&cruxoprof_init.num_events,
                                   cruxoprof_init.cpu_type)) )
        return ret;

    /* Only the hardware domain may become the primary profiler here because
     * there is currently no cleanup of cruxoprof_primary_profiler or associated
     * profiling state when the primary profiling domain is shut down or
     * crashes.  Once a better cleanup method is present, it will be possible to
     * allow another domain to be the primary profiler.
     */
    cruxoprof_init.is_primary = 
        ((cruxoprof_primary_profiler == d) ||
         ((cruxoprof_primary_profiler == NULL) && is_hardware_domain(d)));
    if ( cruxoprof_init.is_primary )
        cruxoprof_primary_profiler = current->domain;

    return __copy_to_guest(arg, &cruxoprof_init, 1) ? -EFAULT : 0;
}

#define ret_t long

#endif /* !COMPAT */

static int cruxoprof_op_get_buffer(CRUX_GUEST_HANDLE_PARAM(void) arg)
{
    struct cruxoprof_get_buffer cruxoprof_get_buffer;
    struct domain *d = current->domain;
    int ret;

    if ( copy_from_guest(&cruxoprof_get_buffer, arg, 1) )
        return -EFAULT;

    /*
     * We allocate cruxoprof struct and buffers only at first time
     * get_buffer is called. Memory is then kept until domain is destroyed.
     */
    if ( d->cruxoprof == NULL )
    {
        ret = alloc_cruxoprof_struct(d, cruxoprof_get_buffer.max_samples, 0);
        if ( ret < 0 )
            return ret;
    }
    else
        d->cruxoprof->domain_type = CRUXOPROF_DOMAIN_IGNORED;

    ret = share_cruxoprof_page_with_guest(
        d, virt_to_mfn(d->cruxoprof->rawbuf), d->cruxoprof->npages);
    if ( ret < 0 )
        return ret;

    cruxoprof_reset_buf(d);

    cruxoprof_get_buffer.nbuf = d->cruxoprof->nbuf;
    cruxoprof_get_buffer.bufsize = d->cruxoprof->bufsize;
    if ( !paging_mode_translate(d) )
        cruxoprof_get_buffer.buf_gmaddr = __pa(d->cruxoprof->rawbuf);
    else
        cruxoprof_shared_gmfn_with_guest(
            d, __pa(d->cruxoprof->rawbuf), cruxoprof_get_buffer.buf_gmaddr,
            d->cruxoprof->npages);

    return __copy_to_guest(arg, &cruxoprof_get_buffer, 1) ? -EFAULT : 0;
}

#define NONPRIV_OP(op) ( (op == CRUXOPROF_init)          \
                      || (op == CRUXOPROF_enable_virq)   \
                      || (op == CRUXOPROF_disable_virq)  \
                      || (op == CRUXOPROF_get_buffer))
 
ret_t do_cruxoprof_op(int op, CRUX_GUEST_HANDLE_PARAM(void) arg)
{
    int ret = 0;
    
    if ( (op < 0) || (op > CRUXOPROF_last_op) )
    {
        gdprintk(CRUXLOG_DEBUG, "invalid operation %d\n", op);
        return -EINVAL;
    }

    if ( !NONPRIV_OP(op) && (current->domain != cruxoprof_primary_profiler) )
    {
        gdprintk(CRUXLOG_DEBUG, "denied privileged operation %d\n", op);
        return -EPERM;
    }

    ret = xsm_profile(XSM_HOOK, current->domain, op);
    if ( ret )
        return ret;

    spin_lock(&cruxoprof_lock);
    
    switch ( op )
    {
    case CRUXOPROF_init:
        ret = cruxoprof_op_init(arg);
        if ( (ret == 0) &&
             (current->domain == cruxoprof_primary_profiler) )
            cruxoprof_state = CRUXOPROF_INITIALIZED;
        break;

    case CRUXOPROF_get_buffer:
        if ( !acquire_pmu_ownership(PMU_OWNER_CRUXOPROF) )
        {
            ret = -EBUSY;
            break;
        }
        ret = cruxoprof_op_get_buffer(arg);
        break;

    case CRUXOPROF_reset_active_list:
        reset_active_list();
        ret = 0;
        break;

    case CRUXOPROF_reset_passive_list:
        reset_passive_list();
        ret = 0;
        break;

    case CRUXOPROF_set_active:
    {
        domid_t domid;
        if ( cruxoprof_state != CRUXOPROF_INITIALIZED )
        {
            ret = -EPERM;
            break;
        }
        if ( copy_from_guest(&domid, arg, 1) )
        {
            ret = -EFAULT;
            break;
        }
        ret = add_active_list(domid);
        break;
    }

    case CRUXOPROF_set_passive:
        if ( cruxoprof_state != CRUXOPROF_INITIALIZED )
        {
            ret = -EPERM;
            break;
        }
        ret = add_passive_list(arg);
        break;

    case CRUXOPROF_reserve_counters:
        if ( cruxoprof_state != CRUXOPROF_INITIALIZED )
        {
            ret = -EPERM;
            break;
        }
        ret = cruxoprof_arch_reserve_counters();
        if ( !ret )
            cruxoprof_state = CRUXOPROF_COUNTERS_RESERVED;
        break;

    case CRUXOPROF_counter:
        if ( (cruxoprof_state != CRUXOPROF_COUNTERS_RESERVED) ||
             (adomains == 0) )
        {
            ret = -EPERM;
            break;
        }
        ret = cruxoprof_arch_counter(arg);
        break;

    case CRUXOPROF_setup_events:
        if ( cruxoprof_state != CRUXOPROF_COUNTERS_RESERVED )
        {
            ret = -EPERM;
            break;
        }
        ret = cruxoprof_arch_setup_events();
        if ( !ret )
            cruxoprof_state = CRUXOPROF_READY;
        break;

    case CRUXOPROF_enable_virq:
    {
        int i;

        if ( current->domain == cruxoprof_primary_profiler )
        {
            if ( cruxoprof_state != CRUXOPROF_READY )
            {
                ret = -EPERM;
                break;
            }
            cruxoprof_arch_enable_virq();
            cruxoprof_reset_stat();
            for ( i = 0; i < pdomains; i++ )
                cruxoprof_reset_buf(passive_domains[i]);
        }
        cruxoprof_reset_buf(current->domain);
        ret = set_active(current->domain);
        break;
    }

    case CRUXOPROF_start:
        ret = -EPERM;
        if ( (cruxoprof_state == CRUXOPROF_READY) &&
             (activated == adomains) )
            ret = cruxoprof_arch_start();
        if ( ret == 0 )
            cruxoprof_state = CRUXOPROF_PROFILING;
        break;

    case CRUXOPROF_stop:
    {
        struct domain *d;
        struct vcpu *v;
        int i;

        if ( cruxoprof_state != CRUXOPROF_PROFILING )
        {
            ret = -EPERM;
            break;
        }
        cruxoprof_arch_stop();

        /* Flush remaining samples. */
        for ( i = 0; i < adomains; i++ )
        {
            if ( !active_ready[i] )
                continue;
            d = active_domains[i];
            for_each_vcpu(d, v)
                send_guest_vcpu_virq(v, VIRQ_CRUXOPROF);
        }
        cruxoprof_state = CRUXOPROF_READY;
        break;
    }

    case CRUXOPROF_disable_virq:
    {
        struct cruxoprof *x;
        if ( (cruxoprof_state == CRUXOPROF_PROFILING) && 
             (is_active(current->domain)) )
        {
            ret = -EPERM;
            break;
        }
        if ( (ret = reset_active(current->domain)) != 0 )
            break;
        x = current->domain->cruxoprof;
        unshare_cruxoprof_page_with_guest(x);
        release_pmu_ownership(PMU_OWNER_CRUXOPROF);
        break;
    }

    case CRUXOPROF_release_counters:
        ret = -EPERM;
        if ( (cruxoprof_state == CRUXOPROF_COUNTERS_RESERVED) ||
             (cruxoprof_state == CRUXOPROF_READY) )
        {
            cruxoprof_state = CRUXOPROF_INITIALIZED;
            cruxoprof_arch_release_counters();
            cruxoprof_arch_disable_virq();
            reset_passive_list();
            ret = 0;
        }
        break;

    case CRUXOPROF_shutdown:
        ret = -EPERM;
        if ( cruxoprof_state == CRUXOPROF_INITIALIZED )
        {
            activated = 0;
            adomains=0;
            cruxoprof_primary_profiler = NULL;
            backtrace_depth=0;
            ret = 0;
        }
        break;
                
    case CRUXOPROF_set_backtrace:
        ret = 0;
        if ( !cruxoprof_backtrace_supported() )
            ret = -EINVAL;
        else if ( copy_from_guest(&backtrace_depth, arg, 1) )
            ret = -EFAULT;
        break;

    case CRUXOPROF_ibs_counter:
        if ( (cruxoprof_state != CRUXOPROF_COUNTERS_RESERVED) ||
             (adomains == 0) )
        {
            ret = -EPERM;
            break;
        }
        ret = cruxoprof_arch_ibs_counter(arg);
        break;

    case CRUXOPROF_get_ibs_caps:
        ret = ibs_caps;
        break;

    default:
        ret = -ENOSYS;
    }

    spin_unlock(&cruxoprof_lock);

    if ( ret < 0 )
        gdprintk(CRUXLOG_DEBUG, "operation %d failed: %d\n", op, ret);

    return ret;
}

#if defined(CONFIG_COMPAT) && !defined(COMPAT)
#undef ret_t
#include "compat/cruxoprof.c"
#endif

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
