/*
 *  This file contains the Flask hook function implementations for Xen.
 *
 *  Author:  George Coker, <gscoker@alpha.ncsc.mil>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License version 2,
 *      as published by the Free Software Foundation.
 */

#include <crux/init.h>
#include <crux/irq.h>
#include <crux/lib.h>
#include <crux/sched.h>
#include <crux/paging.h>
#include <crux/xmalloc.h>
#include <xsm/xsm.h>
#include <crux/spinlock.h>
#include <crux/cpumask.h>
#include <crux/errno.h>
#include <crux/guest_access.h>
#include <crux/cruxoprof.h>
#include <crux/iommu.h>
#ifdef CONFIG_HAS_PCI_MSI
#include <asm/msi.h>
#endif
#include <public/crux.h>
#include <public/physdev.h>
#include <public/platform.h>
#include <public/version.h>
#include <public/hvm/params.h>
#include <public/cruxoprof.h>
#include <public/xsm/flask_op.h>

#include <avc.h>
#include <avc_ss.h>
#include <objsec.h>
#include <conditional.h>
#include "private.h"

#ifdef CONFIG_X86
#include <asm/pv/shim.h>
#else
#define pv_shim false
#endif

static uint32_t domain_sid(const struct domain *dom)
{
    struct domain_security_struct *dsec = dom->ssid;
    return dsec->sid;
}

static uint32_t domain_target_sid(
    const struct domain *src, const struct domain *dst)
{
    struct domain_security_struct *ssec = src->ssid;
    struct domain_security_struct *dsec = dst->ssid;
    if ( src == dst )
        return ssec->self_sid;
    if ( src->target == dst )
        return ssec->target_sid;
    return dsec->sid;
}

static uint32_t evtchn_sid(const struct evtchn *chn)
{
    return chn->ssid.flask_sid;
}

static int domain_has_perm(
    const struct domain *dom1, const struct domain *dom2, uint16_t class,
    uint32_t perms)
{
    uint32_t ssid, tsid;
    struct avc_audit_data ad;
    AVC_AUDIT_DATA_INIT(&ad, NONE);
    ad.sdom = dom1;
    ad.tdom = dom2;

    ssid = domain_sid(dom1);
    tsid = domain_target_sid(dom1, dom2);

    return avc_has_perm(ssid, tsid, class, perms, &ad);
}

static int avc_current_has_perm(
    uint32_t tsid, uint16_t class, uint32_t perm, struct avc_audit_data *ad)
{
    uint32_t csid = domain_sid(current->domain);
    return avc_has_perm(csid, tsid, class, perm, ad);
}

static int current_has_perm(struct domain *d, uint16_t class, uint32_t perms)
{
    return domain_has_perm(current->domain, d, class, perms);
}

static int domain_has_evtchn(
    struct domain *d, struct evtchn *chn, uint32_t perms)
{
    uint32_t dsid = domain_sid(d);
    uint32_t esid = evtchn_sid(chn);

    return avc_has_perm(dsid, esid, SECCLASS_EVENT, perms, NULL);
}

static int domain_has_crux(struct domain *d, uint32_t perms)
{
    uint32_t dsid = domain_sid(d);

    return avc_has_perm(dsid, SECINITSID_CRUX, SECCLASS_CRUX, perms, NULL);
}

static int get_irq_sid(int irq, uint32_t *sid, struct avc_audit_data *ad)
{
    if ( irq >= nr_irqs || irq < 0 )
        return -EINVAL;
    if ( irq < nr_static_irqs ) {
        if (ad) {
            AVC_AUDIT_DATA_INIT(ad, IRQ);
            ad->irq = irq;
        }
        return security_irq_sid(irq, sid);
    }
#ifdef CONFIG_HAS_PCI_MSI
    {
        struct irq_desc *desc = irq_to_desc(irq);

        if ( desc->msi_desc && desc->msi_desc->dev )
        {
            struct pci_dev *dev = desc->msi_desc->dev;
            uint32_t sbdf = (dev->seg << 16) | (dev->bus << 8) | dev->devfn;
            if ( ad )
            {
                AVC_AUDIT_DATA_INIT(ad, DEV);
                ad->device = sbdf;
            }
            return security_device_sid(sbdf, sid);
        }
    }
#endif

    if ( ad )
    {
        AVC_AUDIT_DATA_INIT(ad, IRQ);
        ad->irq = irq;
    }
    /* HPET or IOMMU IRQ, should not be seen by domains */
    *sid = SECINITSID_UNLABELED;
    return 0;
}

static int avc_unknown_permission(const char *name, int id)
{
    int rc;

    if ( !flask_enforcing || security_get_allow_unknown() )
    {
        printk(CRUXLOG_G_WARNING "FLASK: Allowing unknown %s: %d.\n", name, id);
        rc = 0;
    }
    else
    {
        printk(CRUXLOG_G_ERR "FLASK: Denying unknown %s: %d.\n", name, id);
        rc = -EPERM;
    }

    return rc;
}

static int cf_check flask_domain_alloc_security(struct domain *d)
{
    struct domain_security_struct *dsec;

    dsec = xzalloc(struct domain_security_struct);
    if ( !dsec )
        return -ENOMEM;

    /* Set as unlabeled then change as appropriate. */
    dsec->sid = SECINITSID_UNLABELED;

    switch ( d->domain_id )
    {
    case DOMID_IDLE:
        dsec->sid = SECINITSID_CRUXBOOT;
        break;
    case DOMID_CRUX:
        dsec->sid = SECINITSID_DOMCRUX;
        break;
    case DOMID_IO:
        dsec->sid = SECINITSID_DOMIO;
        break;
    default:
        if ( domain_sid(current->domain) == SECINITSID_CRUXBOOT )
        {
            if ( d->is_privileged )
                dsec->sid = SECINITSID_DOM0;
            else if ( pv_shim )
                dsec->sid = SECINITSID_DOMU;
        }
        break;
    }

    dsec->self_sid = dsec->sid;
    d->ssid = dsec;

    return 0;
}

static int cf_check flask_set_system_active(void)
{
    struct domain_security_struct *dsec;
    struct domain *d = current->domain;

    dsec = d->ssid;

    ASSERT(d->is_privileged);
    ASSERT(dsec->sid == SECINITSID_CRUXBOOT);
    ASSERT(dsec->self_sid == SECINITSID_CRUXBOOT);

    if ( d->domain_id != DOMID_IDLE )
    {
        printk("%s: should only be called by idle domain\n", __func__);
        return -EPERM;
    }

    /*
     * While is_privileged has no significant meaning under flask, set to false
     * as is_privileged is not only used for a privilege check but also as a
     * type of domain check, specifically if the domain is the control domain.
     */
    d->is_privileged = false;

    dsec->self_sid = dsec->sid = SECINITSID_CRUX;

    return 0;
}

static void cf_check flask_domain_free_security(struct domain *d)
{
    struct domain_security_struct *dsec = d->ssid;

    if ( !dsec )
        return;

    d->ssid = NULL;
    xfree(dsec);
}

static int cf_check flask_evtchn_unbound(
    struct domain *d1, struct evtchn *chn, domid_t id2)
{
    uint32_t sid1, sid2, newsid;
    int rc;
    struct domain *d2;

    d2 = rcu_lock_domain_by_any_id(id2);
    if ( d2 == NULL )
        return -EPERM;

    sid1 = domain_sid(d1);
    sid2 = domain_target_sid(d1, d2);

    rc = security_transition_sid(sid1, sid2, SECCLASS_EVENT, &newsid);
    if ( rc )
        goto out;

    rc = avc_current_has_perm(newsid, SECCLASS_EVENT, EVENT__CREATE, NULL);
    if ( rc )
        goto out;

    rc = avc_has_perm(newsid, sid2, SECCLASS_EVENT, EVENT__BIND, NULL);
    if ( rc )
        goto out;

    chn->ssid.flask_sid = newsid;

 out:
    rcu_unlock_domain(d2);
    return rc;
}

static int cf_check flask_evtchn_interdomain(
    struct domain *d1, struct evtchn *chn1,
    struct domain *d2, struct evtchn *chn2)
{
    uint32_t sid1, sid2, newsid, reverse_sid;
    int rc;
    struct avc_audit_data ad;
    AVC_AUDIT_DATA_INIT(&ad, NONE);
    ad.sdom = d1;
    ad.tdom = d2;

    sid1 = domain_sid(d1);
    sid2 = domain_target_sid(d1, d2);

    rc = security_transition_sid(sid1, sid2, SECCLASS_EVENT, &newsid);
    if ( rc )
    {
        printk("security_transition_sid failed, rc=%d, %pd\n", -rc, d2);
        return rc;
    }

    rc = avc_current_has_perm(newsid, SECCLASS_EVENT, EVENT__CREATE, &ad);
    if ( rc )
        return rc;

    rc = avc_has_perm(newsid, sid2, SECCLASS_EVENT, EVENT__BIND, &ad);
    if ( rc )
        return rc;

    /* It's possible the target domain has changed (relabel or destroy/create)
     * since the unbound part was created; re-validate this binding now.
     */
    reverse_sid = evtchn_sid(chn2);
    sid1 = domain_target_sid(d2, d1);
    rc = avc_has_perm(reverse_sid, sid1, SECCLASS_EVENT, EVENT__BIND, &ad);
    if ( rc )
        return rc;

    chn1->ssid.flask_sid = newsid;

    return rc;
}

static void cf_check flask_evtchn_close_post(struct evtchn *chn)
{
    chn->ssid.flask_sid = SECINITSID_UNLABELED;
}

static int cf_check flask_evtchn_send(struct domain *d, struct evtchn *chn)
{
    int rc;

    switch ( chn->state )
    {
    case ECS_INTERDOMAIN:
        rc = domain_has_evtchn(d, chn, EVENT__SEND);
        break;
    case ECS_IPI:
    case ECS_UNBOUND:
        rc = 0;
        break;
    default:
        rc = avc_unknown_permission("event channel state", chn->state);
        break;
    }

    return rc;
}

static int cf_check flask_evtchn_status(struct domain *d, struct evtchn *chn)
{
    return domain_has_evtchn(d, chn, EVENT__STATUS);
}

static int cf_check flask_evtchn_reset(struct domain *d1, struct domain *d2)
{
    return domain_has_perm(d1, d2, SECCLASS_EVENT, EVENT__RESET);
}

static int cf_check flask_alloc_security_evtchns(
    struct evtchn chn[], unsigned int nr)
{
    unsigned int i;

    for ( i = 0; i < nr; ++i )
        chn[i].ssid.flask_sid = SECINITSID_UNLABELED;

    return 0;
}

static void cf_check flask_free_security_evtchns(
    struct evtchn chn[], unsigned int nr)
{
    unsigned int i;

    if ( !chn )
        return;

    for ( i = 0; i < nr; ++i )
        chn[i].ssid.flask_sid = SECINITSID_UNLABELED;
}

static char *cf_check flask_show_security_evtchn(
    struct domain *d, const struct evtchn *chn)
{
    int irq;
    uint32_t sid = 0;
    char *ctx;
    uint32_t ctx_len;

    switch ( chn->state )
    {
    case ECS_UNBOUND:
    case ECS_INTERDOMAIN:
        sid = evtchn_sid(chn);
        break;
    case ECS_PIRQ:
        irq = domain_pirq_to_irq(d, chn->u.pirq.irq);
        if (irq && get_irq_sid(irq, &sid, NULL))
            return NULL;
        break;
    }
    if ( !sid )
        return NULL;
    if ( security_sid_to_context(sid, &ctx, &ctx_len) )
        return NULL;
    return ctx;
}

static int cf_check flask_init_hardware_domain(struct domain *d)
{
    return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__CREATE_HARDWARE_DOMAIN);
}

static int cf_check flask_grant_mapref(
    struct domain *d1, struct domain *d2, uint32_t flags)
{
    uint32_t perms = GRANT__MAP_READ;

    if ( !(flags & GNTMAP_readonly) )
        perms |= GRANT__MAP_WRITE;

    return domain_has_perm(d1, d2, SECCLASS_GRANT, perms);
}

static int cf_check flask_grant_unmapref(struct domain *d1, struct domain *d2)
{
    return domain_has_perm(d1, d2, SECCLASS_GRANT, GRANT__UNMAP);
}

static int cf_check flask_grant_setup(struct domain *d1, struct domain *d2)
{
    return domain_has_perm(d1, d2, SECCLASS_GRANT, GRANT__SETUP);
}

static int cf_check flask_grant_transfer(struct domain *d1, struct domain *d2)
{
    return domain_has_perm(d1, d2, SECCLASS_GRANT, GRANT__TRANSFER);
}

static int cf_check flask_grant_copy(struct domain *d1, struct domain *d2)
{
    return domain_has_perm(d1, d2, SECCLASS_GRANT, GRANT__COPY);
}

static int cf_check flask_grant_query_size(struct domain *d1, struct domain *d2)
{
    return domain_has_perm(d1, d2, SECCLASS_GRANT, GRANT__QUERY);
}

static int cf_check flask_get_pod_target(struct domain *d)
{
    return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__GETPODTARGET);
}

static int cf_check flask_set_pod_target(struct domain *d)
{
    return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__SETPODTARGET);
}

static int cf_check flask_memory_exchange(struct domain *d)
{
    return current_has_perm(d, SECCLASS_MMU, MMU__EXCHANGE);
}

static int cf_check flask_memory_adjust_reservation(
    struct domain *d1, struct domain *d2)
{
    return domain_has_perm(d1, d2, SECCLASS_MMU, MMU__ADJUST);
}

static int cf_check flask_memory_stat_reservation(
    struct domain *d1, struct domain *d2)
{
    return domain_has_perm(d1, d2, SECCLASS_MMU, MMU__STAT);
}

static int cf_check flask_memory_pin_page(
    struct domain *d1, struct domain *d2, struct page_info *page)
{
    return domain_has_perm(d1, d2, SECCLASS_MMU, MMU__PINPAGE);
}

static int cf_check flask_claim_pages(struct domain *d)
{
    return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__SETCLAIM);
}

static int cf_check flask_get_vnumainfo(struct domain *d)
{
    return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__GET_VNUMAINFO);
}

static int cf_check flask_console_io(struct domain *d, int cmd)
{
    uint32_t perm;

    switch ( cmd )
    {
    case CONSOLEIO_read:
        perm = CRUX__READCONSOLE;
        break;
    case CONSOLEIO_write:
        perm = CRUX__WRITECONSOLE;
        break;
    default:
        return avc_unknown_permission("console_io", cmd);
    }

    return domain_has_crux(d, perm);
}

static int cf_check flask_profile(struct domain *d, int op)
{
    uint32_t perm;

    switch ( op )
    {
    case CRUXOPROF_init:
    case CRUXOPROF_enable_virq:
    case CRUXOPROF_disable_virq:
    case CRUXOPROF_get_buffer:
        perm = CRUX__NONPRIVPROFILE;
        break;
    case CRUXOPROF_reset_active_list:
    case CRUXOPROF_reset_passive_list:
    case CRUXOPROF_set_active:
    case CRUXOPROF_set_passive:
    case CRUXOPROF_reserve_counters:
    case CRUXOPROF_counter:
    case CRUXOPROF_setup_events:
    case CRUXOPROF_start:
    case CRUXOPROF_stop:
    case CRUXOPROF_release_counters:
    case CRUXOPROF_shutdown:
        perm = CRUX__PRIVPROFILE;
        break;
    default:
        return avc_unknown_permission("cruxoprof op", op);
    }

    return domain_has_crux(d, perm);
}

static int cf_check flask_kexec(void)
{
    return domain_has_crux(current->domain, CRUX__KEXEC);
}

static int cf_check flask_schedop_shutdown(struct domain *d1, struct domain *d2)
{
    return domain_has_perm(d1, d2, SECCLASS_DOMAIN, DOMAIN__SHUTDOWN);
}

static void cf_check flask_security_domaininfo(
    struct domain *d, struct crux_domctl_getdomaininfo *info)
{
    info->ssidref = domain_sid(d);
}

static int cf_check flask_domain_create(struct domain *d, uint32_t ssidref)
{
    int rc;
    struct domain_security_struct *dsec = d->ssid;
    static int dom0_created = 0;

    /*
     * If the null label is passed, then use the label from security context
     * allocation. NB: if the label from the allocated security context is also
     * null, the security server will use unlabeled_t for the domain.
     */
    if ( ssidref == 0 )
        ssidref = dsec->sid;

    /*
     * First check if the current domain is allowed to create the target domain
     * type before making changes to the current state.
     */
    rc = avc_current_has_perm(ssidref, SECCLASS_DOMAIN, DOMAIN__CREATE, NULL);
    if ( rc )
        return rc;

    /*
     * The dom0_t label is expressed as a singleton label in the base policy.
     * This cannot be enforced by the security server, therefore it will be
     * enforced here.
     */
    if ( ssidref == SECINITSID_DOM0 )
    {
        if ( !dom0_created )
            dom0_created = 1;
        else
            return -EINVAL;
    }

    dsec->sid = ssidref;
    dsec->self_sid = dsec->sid;

    rc = security_transition_sid(dsec->sid, dsec->sid, SECCLASS_DOMAIN,
                                 &dsec->self_sid);

    return rc;
}

static int cf_check flask_getdomaininfo(struct domain *d)
{
    return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__GETDOMAININFO);
}

static int cf_check flask_domctl_scheduler_op(struct domain *d, int op)
{
    switch ( op )
    {
    case CRUX_DOMCTL_SCHEDOP_putinfo:
    case CRUX_DOMCTL_SCHEDOP_putvcpuinfo:
        return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__SETSCHEDULER);

    case CRUX_DOMCTL_SCHEDOP_getinfo:
    case CRUX_DOMCTL_SCHEDOP_getvcpuinfo:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__GETSCHEDULER);

    default:
        return avc_unknown_permission("domctl_scheduler_op", op);
    }
}

#ifdef CONFIG_SYSCTL
static int cf_check flask_sysctl_scheduler_op(int op)
{
    switch ( op )
    {
    case CRUX_SYSCTL_SCHEDOP_putinfo:
        return domain_has_crux(current->domain, CRUX__SETSCHEDULER);

    case CRUX_SYSCTL_SCHEDOP_getinfo:
        return domain_has_crux(current->domain, CRUX__GETSCHEDULER);

    default:
        return avc_unknown_permission("sysctl_scheduler_op", op);
    }
}
#endif /* CONFIG_SYSCTL */

static int cf_check flask_set_target(struct domain *d, struct domain *t)
{
    int rc;
    struct domain_security_struct *dsec, *tsec;
    dsec = d->ssid;
    tsec = t->ssid;

    rc = current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__MAKE_PRIV_FOR);
    if ( rc )
        return rc;
    rc = current_has_perm(t, SECCLASS_DOMAIN2, DOMAIN2__SET_AS_TARGET);
    if ( rc )
        return rc;
    /* Use avc_has_perm to avoid resolving target/current SID */
    rc = avc_has_perm(dsec->sid, tsec->sid, SECCLASS_DOMAIN, DOMAIN__SET_TARGET, NULL);
    if ( rc )
        return rc;

    /* (tsec, dsec) defaults the label to tsec, as it should here */
    rc = security_transition_sid(tsec->sid, dsec->sid, SECCLASS_DOMAIN,
                                 &dsec->target_sid);
    return rc;
}

static int cf_check flask_domctl(struct domain *d, unsigned int cmd,
                                 uint32_t ssidref)
{
    switch ( cmd )
    {
    case CRUX_DOMCTL_createdomain:
        /*
         * There is a later hook too, but at this early point simply check
         * that the calling domain is privileged enough to create a domain.
         *
         * Note that d is NULL because we haven't even allocated memory for it
         * this early in CRUX_DOMCTL_createdomain.
         */
        return avc_current_has_perm(ssidref, SECCLASS_DOMAIN, DOMAIN__CREATE, NULL);

    /* These have individual XSM hooks (common/domctl.c) */
    case CRUX_DOMCTL_getdomaininfo:
    case CRUX_DOMCTL_scheduler_op:
    case CRUX_DOMCTL_irq_permission:
    case CRUX_DOMCTL_iomem_permission:
    case CRUX_DOMCTL_memory_mapping:
    case CRUX_DOMCTL_set_target:
    case CRUX_DOMCTL_vm_event_op:
    case CRUX_DOMCTL_get_domain_state:

    /* These have individual XSM hooks (arch/../domctl.c) */
    case CRUX_DOMCTL_bind_pt_irq:
    case CRUX_DOMCTL_unbind_pt_irq:
#ifdef CONFIG_X86
    /* These have individual XSM hooks (arch/x86/domctl.c) */
    case CRUX_DOMCTL_shadow_op:
    case CRUX_DOMCTL_ioport_permission:
    case CRUX_DOMCTL_ioport_mapping:
    case CRUX_DOMCTL_gsi_permission:
#endif
#ifdef CONFIG_HAS_PASSTHROUGH
    /*
     * These have individual XSM hooks
     * (drivers/passthrough/{pci,device_tree.c)
     */
    case CRUX_DOMCTL_get_device_group:
    case CRUX_DOMCTL_test_assign_device:
    case CRUX_DOMCTL_assign_device:
    case CRUX_DOMCTL_deassign_device:
#endif
        return 0;

    case CRUX_DOMCTL_destroydomain:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__DESTROY);

    case CRUX_DOMCTL_pausedomain:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__PAUSE);

    case CRUX_DOMCTL_unpausedomain:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__UNPAUSE);

    case CRUX_DOMCTL_setvcpuaffinity:
    case CRUX_DOMCTL_setnodeaffinity:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__SETAFFINITY);

    case CRUX_DOMCTL_getvcpuaffinity:
    case CRUX_DOMCTL_getnodeaffinity:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__GETAFFINITY);

    case CRUX_DOMCTL_resumedomain:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__RESUME);

    case CRUX_DOMCTL_max_vcpus:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__MAX_VCPUS);

    case CRUX_DOMCTL_max_mem:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__SETDOMAINMAXMEM);

    case CRUX_DOMCTL_setdomainhandle:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__SETDOMAINHANDLE);

    case CRUX_DOMCTL_set_ext_vcpucontext:
    case CRUX_DOMCTL_set_vcpu_msrs:
    case CRUX_DOMCTL_setvcpucontext:
    case CRUX_DOMCTL_setvcpuextstate:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__SETVCPUCONTEXT);

    case CRUX_DOMCTL_get_ext_vcpucontext:
    case CRUX_DOMCTL_get_vcpu_msrs:
    case CRUX_DOMCTL_getvcpucontext:
    case CRUX_DOMCTL_getvcpuextstate:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__GETVCPUCONTEXT);

    case CRUX_DOMCTL_getvcpuinfo:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__GETVCPUINFO);

    case CRUX_DOMCTL_settimeoffset:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__SETTIME);

    case CRUX_DOMCTL_setdebugging:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__SETDEBUGGING);

    case CRUX_DOMCTL_getpageframeinfo3:
        return current_has_perm(d, SECCLASS_MMU, MMU__PAGEINFO);

    case CRUX_DOMCTL_hypercall_init:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__HYPERCALL);

    case CRUX_DOMCTL_sethvmcontext:
        return current_has_perm(d, SECCLASS_HVM, HVM__SETHVMC);

    case CRUX_DOMCTL_gethvmcontext:
    case CRUX_DOMCTL_gethvmcontext_partial:
        return current_has_perm(d, SECCLASS_HVM, HVM__GETHVMC);

    case CRUX_DOMCTL_set_address_size:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__SETADDRSIZE);

    case CRUX_DOMCTL_get_address_size:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__GETADDRSIZE);

    case CRUX_DOMCTL_mem_sharing_op:
        return current_has_perm(d, SECCLASS_HVM, HVM__MEM_SHARING);

    case CRUX_DOMCTL_sendtrigger:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__TRIGGER);

    case CRUX_DOMCTL_set_access_required:
        return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__VM_EVENT);

    case CRUX_DOMCTL_monitor_op:
        return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__VM_EVENT);

    case CRUX_DOMCTL_debug_op:
    case CRUX_DOMCTL_vmtrace_op:
    case CRUX_DOMCTL_gdbsx_guestmemio:
    case CRUX_DOMCTL_gdbsx_pausevcpu:
    case CRUX_DOMCTL_gdbsx_unpausevcpu:
    case CRUX_DOMCTL_gdbsx_domstatus:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__SETDEBUGGING);

    case CRUX_DOMCTL_subscribe:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__SET_MISC_INFO);

    case CRUX_DOMCTL_set_virq_handler:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__SET_VIRQ_HANDLER);

    case CRUX_DOMCTL_set_cpu_policy:
        return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__SET_CPU_POLICY);

    case CRUX_DOMCTL_gettscinfo:
        return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__GETTSC);

    case CRUX_DOMCTL_settscinfo:
        return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__SETTSC);

    case CRUX_DOMCTL_audit_p2m:
        return current_has_perm(d, SECCLASS_HVM, HVM__AUDIT_P2M);

    case CRUX_DOMCTL_cacheflush:
        return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__CACHEFLUSH);

    case CRUX_DOMCTL_setvnumainfo:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN2__SET_VNUMAINFO);
    case CRUX_DOMCTL_psr_cmt_op:
        return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__PSR_CMT_OP);

    case CRUX_DOMCTL_psr_alloc:
        return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__PSR_ALLOC);

    case CRUX_DOMCTL_soft_reset:
        return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__SOFT_RESET);

    case CRUX_DOMCTL_vuart_op:
        return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__VUART_OP);

    case CRUX_DOMCTL_get_cpu_policy:
        return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__GET_CPU_POLICY);

    case CRUX_DOMCTL_get_paging_mempool_size:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__GETPAGINGMEMPOOL);

    case CRUX_DOMCTL_set_paging_mempool_size:
        return current_has_perm(d, SECCLASS_DOMAIN, DOMAIN__SETPAGINGMEMPOOL);

    case CRUX_DOMCTL_dt_overlay:
        return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__DT_OVERLAY);

    case CRUX_DOMCTL_set_llc_colors:
        return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__SET_LLC_COLORS);

    default:
        return avc_unknown_permission("domctl", cmd);
    }
}

#ifdef CONFIG_SYSCTL
static int cf_check flask_sysctl(int cmd)
{
    switch ( cmd )
    {
    /* These have individual XSM hooks */
    case CRUX_SYSCTL_readconsole:
    case CRUX_SYSCTL_getdomaininfolist:
    case CRUX_SYSCTL_page_offline_op:
    case CRUX_SYSCTL_scheduler_op:
#ifdef CONFIG_X86
    case CRUX_SYSCTL_cpu_hotplug:
#endif
        return 0;

    case CRUX_SYSCTL_tbuf_op:
        return domain_has_crux(current->domain, CRUX__TBUFCONTROL);

    case CRUX_SYSCTL_sched_id:
        return domain_has_crux(current->domain, CRUX__GETSCHEDULER);

    case CRUX_SYSCTL_perfc_op:
        return domain_has_crux(current->domain, CRUX__PERFCONTROL);

    case CRUX_SYSCTL_debug_keys:
        return domain_has_crux(current->domain, CRUX__DEBUG);

    case CRUX_SYSCTL_getcpuinfo:
        return domain_has_crux(current->domain, CRUX__GETCPUINFO);

    case CRUX_SYSCTL_availheap:
        return domain_has_crux(current->domain, CRUX__HEAP);

    case CRUX_SYSCTL_get_pmstat:
        return domain_has_crux(current->domain, CRUX__PM_OP);

    case CRUX_SYSCTL_pm_op:
        return domain_has_crux(current->domain, CRUX__PM_OP);

    case CRUX_SYSCTL_lockprof_op:
        return domain_has_crux(current->domain, CRUX__LOCKPROF);

    case CRUX_SYSCTL_cpupool_op:
        return domain_has_crux(current->domain, CRUX__CPUPOOL_OP);

    case CRUX_SYSCTL_physinfo:
    case CRUX_SYSCTL_cputopoinfo:
    case CRUX_SYSCTL_numainfo:
    case CRUX_SYSCTL_pcitopoinfo:
    case CRUX_SYSCTL_get_cpu_policy:
        return domain_has_crux(current->domain, CRUX__PHYSINFO);

    case CRUX_SYSCTL_psr_cmt_op:
        return avc_current_has_perm(SECINITSID_CRUX, SECCLASS_CRUX2,
                                    CRUX2__PSR_CMT_OP, NULL);
    case CRUX_SYSCTL_psr_alloc:
        return avc_current_has_perm(SECINITSID_CRUX, SECCLASS_CRUX2,
                                    CRUX2__PSR_ALLOC, NULL);

    case CRUX_SYSCTL_get_cpu_levelling_caps:
        return avc_current_has_perm(SECINITSID_CRUX, SECCLASS_CRUX2,
                                    CRUX2__GET_CPU_LEVELLING_CAPS, NULL);

    case CRUX_SYSCTL_get_cpu_featureset:
        return avc_current_has_perm(SECINITSID_CRUX, SECCLASS_CRUX2,
                                    CRUX2__GET_CPU_FEATURESET, NULL);

    case CRUX_SYSCTL_livepatch_op:
        return avc_current_has_perm(SECINITSID_CRUX, SECCLASS_CRUX2,
                                    CRUX2__LIVEPATCH_OP, NULL);
    case CRUX_SYSCTL_coverage_op:
        return avc_current_has_perm(SECINITSID_CRUX, SECCLASS_CRUX2,
                                    CRUX2__COVERAGE_OP, NULL);

    default:
        return avc_unknown_permission("sysctl", cmd);
    }
}

static int cf_check flask_readconsole(uint32_t clear)
{
    uint32_t perms = CRUX__READCONSOLE;

    if ( clear )
        perms |= CRUX__CLEARCONSOLE;

    return domain_has_crux(current->domain, perms);
}
#endif /* CONFIG_SYSCTL */

static inline uint32_t resource_to_perm(uint8_t access)
{
    if ( access )
        return RESOURCE__ADD;
    else
        return RESOURCE__REMOVE;
}

static char *cf_check flask_show_irq_sid(int irq)
{
    uint32_t sid, ctx_len;
    char *ctx;
    int rc = get_irq_sid(irq, &sid, NULL);
    if ( rc )
        return NULL;

    if ( security_sid_to_context(sid, &ctx, &ctx_len) )
        return NULL;

    return ctx;
}

static int cf_check flask_map_domain_pirq(struct domain *d)
{
    return current_has_perm(d, SECCLASS_RESOURCE, RESOURCE__ADD);
}

static int flask_map_domain_msi (
    struct domain *d, int irq, const void *data, uint32_t *sid,
    struct avc_audit_data *ad)
{
#ifdef CONFIG_HAS_PCI_MSI
    const struct msi_info *msi = data;
    uint32_t machine_bdf = msi->sbdf.sbdf;

    AVC_AUDIT_DATA_INIT(ad, DEV);
    ad->device = machine_bdf;

    return security_device_sid(machine_bdf, sid);
#else
    return -EINVAL;
#endif
}

static uint32_t flask_iommu_resource_use_perm(const struct domain *d)
{
    /* Obtain the permission level required for allowing a domain
     * to use an assigned device.
     *
     * An active IOMMU with interrupt remapping capability is essential
     * for ensuring strict isolation of devices, so provide a distinct
     * permission for that case and also enable optional support for
     * less capable hardware (no IOMMU or IOMMU missing intremap capability)
     * via other separate permissions.
     */
    uint32_t perm = RESOURCE__USE_NOIOMMU;

    if ( is_iommu_enabled(d) )
        perm = ( iommu_intremap ? RESOURCE__USE_IOMMU :
                                  RESOURCE__USE_IOMMU_NOINTREMAP );
    return perm;
}

static int cf_check flask_map_domain_irq(
    struct domain *d, int irq, const void *data)
{
    uint32_t sid, dsid;
    int rc = -EPERM;
    struct avc_audit_data ad;
    uint32_t dperm = flask_iommu_resource_use_perm(d);

    if ( irq >= nr_static_irqs && data )
        rc = flask_map_domain_msi(d, irq, data, &sid, &ad);
    else
        rc = get_irq_sid(irq, &sid, &ad);

    if ( rc )
        return rc;

    dsid = domain_sid(d);

    rc = avc_current_has_perm(sid, SECCLASS_RESOURCE, RESOURCE__ADD_IRQ, &ad);
    if ( rc )
        return rc;

    rc = avc_has_perm(dsid, sid, SECCLASS_RESOURCE, dperm, &ad);
    return rc;
}

static int cf_check flask_unmap_domain_pirq(struct domain *d)
{
    return current_has_perm(d, SECCLASS_RESOURCE, RESOURCE__REMOVE);
}

static int flask_unmap_domain_msi (
    struct domain *d, int irq, const void *data, uint32_t *sid,
    struct avc_audit_data *ad)
{
#ifdef CONFIG_HAS_PCI_MSI
    const struct pci_dev *pdev = data;
    uint32_t machine_bdf = (pdev->seg << 16) | (pdev->bus << 8) | pdev->devfn;

    AVC_AUDIT_DATA_INIT(ad, DEV);
    ad->device = machine_bdf;

    return security_device_sid(machine_bdf, sid);
#else
    return -EINVAL;
#endif
}

static int cf_check flask_unmap_domain_irq(
    struct domain *d, int irq, const void *data)
{
    uint32_t sid;
    int rc = -EPERM;
    struct avc_audit_data ad;

    if ( irq >= nr_static_irqs && data )
        rc = flask_unmap_domain_msi(d, irq, data, &sid, &ad);
    else
        rc = get_irq_sid(irq, &sid, &ad);

    if ( rc )
        return rc;

    rc = avc_current_has_perm(sid, SECCLASS_RESOURCE, RESOURCE__REMOVE_IRQ, &ad);
    return rc;
}

static int cf_check flask_bind_pt_irq(
    struct domain *d, struct crux_domctl_bind_pt_irq *bind)
{
    uint32_t dsid, rsid;
    int rc = -EPERM;
    int irq;
    struct avc_audit_data ad;
    uint32_t dperm = flask_iommu_resource_use_perm(d);

    rc = current_has_perm(d, SECCLASS_RESOURCE, RESOURCE__ADD);
    if ( rc )
        return rc;

    irq = domain_pirq_to_irq(d, bind->machine_irq);

    rc = get_irq_sid(irq, &rsid, &ad);
    if ( rc )
        return rc;

    rc = avc_current_has_perm(rsid, SECCLASS_HVM, HVM__BIND_IRQ, &ad);
    if ( rc )
        return rc;

    dsid = domain_sid(d);
    return avc_has_perm(dsid, rsid, SECCLASS_RESOURCE, dperm, &ad);
}

static int cf_check flask_unbind_pt_irq(
    struct domain *d, struct crux_domctl_bind_pt_irq *bind)
{
    return current_has_perm(d, SECCLASS_RESOURCE, RESOURCE__REMOVE);
}

static int cf_check flask_irq_permission(
    struct domain *d, int pirq, uint8_t access)
{
    /* the PIRQ number is not useful; real IRQ is checked during mapping */
    return current_has_perm(d, SECCLASS_RESOURCE, resource_to_perm(access));
}

struct iomem_has_perm_data {
    uint32_t ssid;
    uint32_t dsid;
    uint32_t perm;
    uint32_t use_perm;
};

static int cf_check _iomem_has_perm(
    void *v, uint32_t sid, unsigned long start, unsigned long end)
{
    struct iomem_has_perm_data *data = v;
    struct avc_audit_data ad;
    int rc = -EPERM;

    AVC_AUDIT_DATA_INIT(&ad, RANGE);
    ad.range.start = start;
    ad.range.end = end;

    rc = avc_has_perm(data->ssid, sid, SECCLASS_RESOURCE, data->perm, &ad);

    if ( rc )
        return rc;

    return avc_has_perm(data->dsid, sid, SECCLASS_RESOURCE, data->use_perm, &ad);
}

static int cf_check flask_iomem_permission(
    struct domain *d, uint64_t start, uint64_t end, uint8_t access)
{
    struct iomem_has_perm_data data;
    int rc;

    rc = current_has_perm(d, SECCLASS_RESOURCE,
                         resource_to_perm(access));
    if ( rc )
        return rc;

    if ( access )
        data.perm = RESOURCE__ADD_IOMEM;
    else
        data.perm = RESOURCE__REMOVE_IOMEM;

    data.ssid = domain_sid(current->domain);
    data.dsid = domain_sid(d);
    data.use_perm = flask_iommu_resource_use_perm(d);

    return security_iterate_iomem_sids(start, end, _iomem_has_perm, &data);
}

static int cf_check flask_iomem_mapping(struct domain *d, uint64_t start, uint64_t end, uint8_t access)
{
    return flask_iomem_permission(d, start, end, access);
}

static int cf_check flask_pci_config_permission(
    struct domain *d, uint32_t machine_bdf, uint16_t start, uint16_t end,
    uint8_t access)
{
    uint32_t dsid, rsid;
    int rc = -EPERM;
    struct avc_audit_data ad;
    uint32_t perm;

    rc = security_device_sid(machine_bdf, &rsid);
    if ( rc )
        return rc;

    /* Writes to the BARs count as setup */
    if ( access && (end >= 0x10 && start < 0x28) )
        perm = RESOURCE__SETUP;
    else
        perm = flask_iommu_resource_use_perm(d);

    AVC_AUDIT_DATA_INIT(&ad, DEV);
    ad.device = (unsigned long) machine_bdf;
    dsid = domain_sid(d);
    return avc_has_perm(dsid, rsid, SECCLASS_RESOURCE, perm, &ad);

}

static int cf_check flask_resource_plug_core(void)
{
    return avc_current_has_perm(SECINITSID_DOMCRUX, SECCLASS_RESOURCE, RESOURCE__PLUG, NULL);
}

static int cf_check flask_resource_unplug_core(void)
{
    return avc_current_has_perm(SECINITSID_DOMCRUX, SECCLASS_RESOURCE, RESOURCE__UNPLUG, NULL);
}

#ifdef CONFIG_SYSCTL
static int flask_resource_use_core(void)
{
    return avc_current_has_perm(SECINITSID_DOMCRUX, SECCLASS_RESOURCE, RESOURCE__USE, NULL);
}
#endif /* CONFIG_SYSCTL */

static int cf_check flask_resource_plug_pci(uint32_t machine_bdf)
{
    uint32_t rsid;
    int rc = -EPERM;
    struct avc_audit_data ad;

    rc = security_device_sid(machine_bdf, &rsid);
    if ( rc )
        return rc;

    AVC_AUDIT_DATA_INIT(&ad, DEV);
    ad.device = (unsigned long) machine_bdf;
    return avc_current_has_perm(rsid, SECCLASS_RESOURCE, RESOURCE__PLUG, &ad);
}

static int cf_check flask_resource_unplug_pci(uint32_t machine_bdf)
{
    uint32_t rsid;
    int rc = -EPERM;
    struct avc_audit_data ad;

    rc = security_device_sid(machine_bdf, &rsid);
    if ( rc )
        return rc;

    AVC_AUDIT_DATA_INIT(&ad, DEV);
    ad.device = (unsigned long) machine_bdf;
    return avc_current_has_perm(rsid, SECCLASS_RESOURCE, RESOURCE__UNPLUG, &ad);
}

static int cf_check flask_resource_setup_pci(uint32_t machine_bdf)
{
    uint32_t rsid;
    int rc = -EPERM;
    struct avc_audit_data ad;

    rc = security_device_sid(machine_bdf, &rsid);
    if ( rc )
        return rc;

    AVC_AUDIT_DATA_INIT(&ad, DEV);
    ad.device = (unsigned long) machine_bdf;
    return avc_current_has_perm(rsid, SECCLASS_RESOURCE, RESOURCE__SETUP, &ad);
}

static int cf_check flask_resource_setup_gsi(int gsi)
{
    uint32_t rsid;
    int rc = -EPERM;
    struct avc_audit_data ad;

    rc = get_irq_sid(gsi, &rsid, &ad);
    if ( rc )
        return rc;

    return avc_current_has_perm(rsid, SECCLASS_RESOURCE, RESOURCE__SETUP, &ad);
}

static int cf_check flask_resource_setup_misc(void)
{
    return avc_current_has_perm(SECINITSID_CRUX, SECCLASS_RESOURCE, RESOURCE__SETUP, NULL);
}

#ifdef CONFIG_SYSCTL
static inline int cf_check flask_page_offline(uint32_t cmd)
{
    switch ( cmd )
    {
    case sysctl_page_offline:
        return flask_resource_unplug_core();
    case sysctl_page_online:
        return flask_resource_plug_core();
    case sysctl_query_page_offline:
        return flask_resource_use_core();
    default:
        return avc_unknown_permission("page_offline", cmd);
    }
}
#endif /* CONFIG_SYSCTL */

static inline int cf_check flask_hypfs_op(void)
{
    return domain_has_crux(current->domain, CRUX__HYPFS_OP);
}

static int cf_check flask_add_to_physmap(struct domain *d1, struct domain *d2)
{
    return domain_has_perm(d1, d2, SECCLASS_MMU, MMU__PHYSMAP);
}

static int cf_check flask_remove_from_physmap(
    struct domain *d1, struct domain *d2)
{
    return domain_has_perm(d1, d2, SECCLASS_MMU, MMU__PHYSMAP);
}

static int cf_check flask_map_gmfn_foreign(struct domain *d, struct domain *t)
{
    return domain_has_perm(d, t, SECCLASS_MMU, MMU__MAP_READ | MMU__MAP_WRITE);
}

static int cf_check flask_hvm_param(struct domain *d, unsigned long op)
{
    uint32_t perm;

    switch ( op )
    {
    case HVMOP_set_param:
        perm = HVM__SETPARAM;
        break;
    case HVMOP_get_param:
        perm = HVM__GETPARAM;
        break;
    default:
        perm = HVM__HVMCTL;
        break;
    }

    return current_has_perm(d, SECCLASS_HVM, perm);
}

static int cf_check flask_hvm_param_altp2mhvm(struct domain *d)
{
    return current_has_perm(d, SECCLASS_HVM, HVM__ALTP2MHVM);
}

static int cf_check flask_hvm_altp2mhvm_op(struct domain *d, uint64_t mode, uint32_t op)
{
    /*
     * Require both mode and XSM to allow the operation. Assume XSM rules
     * are written with the XSM_TARGET policy in mind, so add restrictions
     * on the domain acting on itself when forbidden by the mode.
     */
    switch ( mode )
    {
    case CRUX_ALTP2M_mixed:
        break;
    case CRUX_ALTP2M_limited:
        if ( HVMOP_altp2m_vcpu_enable_notify == op )
            break;
        /* fall-through */
    case CRUX_ALTP2M_external:
        if ( d == current->domain )
            return -EPERM;
        break;
    };

    return current_has_perm(d, SECCLASS_HVM, HVM__ALTP2MHVM_OP);
}

static int cf_check flask_vm_event_control(struct domain *d, int mode, int op)
{
    return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__VM_EVENT);
}

#ifdef CONFIG_VM_EVENT
static int cf_check flask_mem_access(struct domain *d)
{
    return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__MEM_ACCESS);
}
#endif

#ifdef CONFIG_MEM_PAGING
static int cf_check flask_mem_paging(struct domain *d)
{
    return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__MEM_PAGING);
}
#endif

#ifdef CONFIG_MEM_SHARING
static int cf_check flask_mem_sharing(struct domain *d)
{
    return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__MEM_SHARING);
}
#endif

#if defined(CONFIG_HAS_PASSTHROUGH) && defined(CONFIG_HAS_PCI)
static int cf_check flask_get_device_group(uint32_t machine_bdf)
{
    uint32_t rsid;
    int rc = -EPERM;

    rc = security_device_sid(machine_bdf, &rsid);
    if ( rc )
        return rc;

    return avc_current_has_perm(rsid, SECCLASS_RESOURCE, RESOURCE__STAT_DEVICE, NULL);
}

static int flask_test_assign_device(uint32_t machine_bdf)
{
    uint32_t rsid;
    int rc = -EPERM;

    rc = security_device_sid(machine_bdf, &rsid);
    if ( rc )
        return rc;

    return avc_current_has_perm(rsid, SECCLASS_RESOURCE, RESOURCE__STAT_DEVICE, NULL);
}

static int cf_check flask_assign_device(struct domain *d, uint32_t machine_bdf)
{
    uint32_t dsid, rsid;
    int rc = -EPERM;
    struct avc_audit_data ad;
    uint32_t dperm;

    if ( !d )
        return flask_test_assign_device(machine_bdf);

    dperm = flask_iommu_resource_use_perm(d);

    rc = current_has_perm(d, SECCLASS_RESOURCE, RESOURCE__ADD);
    if ( rc )
        return rc;

    rc = security_device_sid(machine_bdf, &rsid);
    if ( rc )
        return rc;

    AVC_AUDIT_DATA_INIT(&ad, DEV);
    ad.device = (unsigned long) machine_bdf;
    rc = avc_current_has_perm(rsid, SECCLASS_RESOURCE, RESOURCE__ADD_DEVICE, &ad);
    if ( rc )
        return rc;

    dsid = domain_sid(d);
    return avc_has_perm(dsid, rsid, SECCLASS_RESOURCE, dperm, &ad);
}

static int cf_check flask_deassign_device(
    struct domain *d, uint32_t machine_bdf)
{
    uint32_t rsid;
    int rc = -EPERM;

    rc = current_has_perm(d, SECCLASS_RESOURCE, RESOURCE__REMOVE);
    if ( rc )
        return rc;

    rc = security_device_sid(machine_bdf, &rsid);
    if ( rc )
        return rc;

    return avc_current_has_perm(rsid, SECCLASS_RESOURCE, RESOURCE__REMOVE_DEVICE, NULL);
}
#endif /* HAS_PASSTHROUGH && HAS_PCI */

#if defined(CONFIG_HAS_PASSTHROUGH) && defined(CONFIG_HAS_DEVICE_TREE_DISCOVERY)
static int flask_test_assign_dtdevice(const char *dtpath)
{
    uint32_t rsid;
    int rc = -EPERM;

    rc = security_devicetree_sid(dtpath, &rsid);
    if ( rc )
        return rc;

    return avc_current_has_perm(rsid, SECCLASS_RESOURCE, RESOURCE__STAT_DEVICE,
                                NULL);
}

static int cf_check flask_assign_dtdevice(struct domain *d, const char *dtpath)
{
    uint32_t dsid, rsid;
    int rc = -EPERM;
    struct avc_audit_data ad;
    uint32_t dperm;

    if ( !d )
        return flask_test_assign_dtdevice(dtpath);

    dperm = flask_iommu_resource_use_perm(d);

    rc = current_has_perm(d, SECCLASS_RESOURCE, RESOURCE__ADD);
    if ( rc )
        return rc;

    rc = security_devicetree_sid(dtpath, &rsid);
    if ( rc )
        return rc;

    AVC_AUDIT_DATA_INIT(&ad, DTDEV);
    ad.dtdev = dtpath;
    rc = avc_current_has_perm(rsid, SECCLASS_RESOURCE, RESOURCE__ADD_DEVICE, &ad);
    if ( rc )
        return rc;

    dsid = domain_sid(d);
    return avc_has_perm(dsid, rsid, SECCLASS_RESOURCE, dperm, &ad);
}

static int cf_check flask_deassign_dtdevice(
    struct domain *d, const char *dtpath)
{
    uint32_t rsid;
    int rc = -EPERM;

    rc = current_has_perm(d, SECCLASS_RESOURCE, RESOURCE__REMOVE);
    if ( rc )
        return rc;

    rc = security_devicetree_sid(dtpath, &rsid);
    if ( rc )
        return rc;

    return avc_current_has_perm(rsid, SECCLASS_RESOURCE, RESOURCE__REMOVE_DEVICE,
                                NULL);
}
#endif /* HAS_PASSTHROUGH && HAS_DEVICE_TREE_DISCOVERY */

static int cf_check flask_platform_op(uint32_t op)
{
    switch ( op )
    {
#ifdef CONFIG_X86
    /* These operations have their own XSM hooks */
    case CRUXPF_cpu_online:
    case CRUXPF_cpu_offline:
    case CRUXPF_cpu_hotadd:
    case CRUXPF_mem_hotadd:
        return 0;
#endif

    case CRUXPF_settime32:
    case CRUXPF_settime64:
        return domain_has_crux(current->domain, CRUX__SETTIME);

    case CRUXPF_add_memtype:
        return domain_has_crux(current->domain, CRUX__MTRR_ADD);

    case CRUXPF_del_memtype:
        return domain_has_crux(current->domain, CRUX__MTRR_DEL);

    case CRUXPF_read_memtype:
        return domain_has_crux(current->domain, CRUX__MTRR_READ);

    case CRUXPF_microcode_update:
        return domain_has_crux(current->domain, CRUX__MICROCODE);

    case CRUXPF_platform_quirk:
        return domain_has_crux(current->domain, CRUX__QUIRK);

    case CRUXPF_firmware_info:
        return domain_has_crux(current->domain, CRUX__FIRMWARE);

    case CRUXPF_efi_runtime_call:
        return domain_has_crux(current->domain, CRUX__FIRMWARE);

    case CRUXPF_enter_acpi_sleep:
        return domain_has_crux(current->domain, CRUX__SLEEP);

    case CRUXPF_change_freq:
        return domain_has_crux(current->domain, CRUX__FREQUENCY);

    case CRUXPF_getidletime:
        return domain_has_crux(current->domain, CRUX__GETIDLE);

    case CRUXPF_set_processor_pminfo:
    case CRUXPF_core_parking:
        return domain_has_crux(current->domain, CRUX__PM_OP);

    case CRUXPF_get_cpu_version:
    case CRUXPF_get_cpuinfo:
        return domain_has_crux(current->domain, CRUX__GETCPUINFO);

    case CRUXPF_resource_op:
        return avc_current_has_perm(SECINITSID_CRUX, SECCLASS_CRUX2,
                                    CRUX2__RESOURCE_OP, NULL);

    case CRUXPF_get_symbol:
        return avc_has_perm(domain_sid(current->domain), SECINITSID_CRUX,
                            SECCLASS_CRUX2, CRUX2__GET_SYMBOL, NULL);

    case CRUXPF_get_dom0_console:
        return avc_has_perm(domain_sid(current->domain), SECINITSID_CRUX,
                            SECCLASS_CRUX2, CRUX2__GET_DOM0_CONSOLE, NULL);

    default:
        return avc_unknown_permission("platform_op", op);
    }
}

#ifdef CONFIG_X86
static int cf_check flask_do_mca(void)
{
    return domain_has_crux(current->domain, CRUX__MCA_OP);
}

static int cf_check flask_shadow_control(struct domain *d, uint32_t op)
{
    uint32_t perm;

    switch ( op )
    {
    case CRUX_DOMCTL_SHADOW_OP_OFF:
        perm = SHADOW__DISABLE;
        break;
    case CRUX_DOMCTL_SHADOW_OP_ENABLE:
    case CRUX_DOMCTL_SHADOW_OP_ENABLE_TEST:
    case CRUX_DOMCTL_SHADOW_OP_GET_ALLOCATION:
    case CRUX_DOMCTL_SHADOW_OP_SET_ALLOCATION:
        perm = SHADOW__ENABLE;
        break;
    case CRUX_DOMCTL_SHADOW_OP_ENABLE_LOGDIRTY:
    case CRUX_DOMCTL_SHADOW_OP_PEEK:
    case CRUX_DOMCTL_SHADOW_OP_CLEAN:
        perm = SHADOW__LOGDIRTY;
        break;
    default:
        return avc_unknown_permission("shadow_control", op);
    }

    return current_has_perm(d, SECCLASS_SHADOW, perm);
}

struct ioport_has_perm_data {
    uint32_t ssid;
    uint32_t dsid;
    uint32_t perm;
    uint32_t use_perm;
};

static int cf_check _ioport_has_perm(
    void *v, uint32_t sid, unsigned long start, unsigned long end)
{
    struct ioport_has_perm_data *data = v;
    struct avc_audit_data ad;
    int rc;

    AVC_AUDIT_DATA_INIT(&ad, RANGE);
    ad.range.start = start;
    ad.range.end = end;

    rc = avc_has_perm(data->ssid, sid, SECCLASS_RESOURCE, data->perm, &ad);

    if ( rc )
        return rc;

    return avc_has_perm(data->dsid, sid, SECCLASS_RESOURCE, data->use_perm, &ad);
}

static int cf_check flask_ioport_permission(
    struct domain *d, uint32_t start, uint32_t end, uint8_t access)
{
    int rc;
    struct ioport_has_perm_data data;

    rc = current_has_perm(d, SECCLASS_RESOURCE,
                         resource_to_perm(access));

    if ( rc )
        return rc;

    if ( access )
        data.perm = RESOURCE__ADD_IOPORT;
    else
        data.perm = RESOURCE__REMOVE_IOPORT;

    data.ssid = domain_sid(current->domain);
    data.dsid = domain_sid(d);
    data.use_perm = flask_iommu_resource_use_perm(d);

    return security_iterate_ioport_sids(start, end, _ioport_has_perm, &data);
}

static int cf_check flask_ioport_mapping(
    struct domain *d, uint32_t start, uint32_t end, uint8_t access)
{
    return flask_ioport_permission(d, start, end, access);
}

static int cf_check flask_mem_sharing_op(
    struct domain *d, struct domain *cd, int op)
{
    int rc = current_has_perm(cd, SECCLASS_HVM, HVM__MEM_SHARING);
    if ( rc )
        return rc;
    return domain_has_perm(d, cd, SECCLASS_HVM, HVM__SHARE_MEM);
}

static int cf_check flask_apic(struct domain *d, int cmd)
{
    uint32_t perm;

    switch ( cmd )
    {
    case PHYSDEVOP_apic_read:
    case PHYSDEVOP_alloc_irq_vector:
        perm = CRUX__READAPIC;
        break;
    case PHYSDEVOP_apic_write:
        perm = CRUX__WRITEAPIC;
        break;
    default:
        return avc_unknown_permission("apic", cmd);
    }

    return domain_has_crux(d, perm);
}

static int cf_check flask_machine_memory_map(void)
{
    return avc_current_has_perm(SECINITSID_CRUX, SECCLASS_MMU, MMU__MEMORYMAP, NULL);
}

static int cf_check flask_domain_memory_map(struct domain *d)
{
    return current_has_perm(d, SECCLASS_MMU, MMU__MEMORYMAP);
}

static int cf_check flask_mmu_update(
    struct domain *d, struct domain *t, struct domain *f, uint32_t flags)
{
    int rc = 0;
    uint32_t map_perms = 0;

    if ( t && d != t )
        rc = domain_has_perm(d, t, SECCLASS_MMU, MMU__REMOTE_REMAP);
    if ( rc )
        return rc;

    if ( flags & XSM_MMU_UPDATE_READ )
        map_perms |= MMU__MAP_READ;
    if ( flags & XSM_MMU_UPDATE_WRITE )
        map_perms |= MMU__MAP_WRITE;
    if ( flags & XSM_MMU_MACHPHYS_UPDATE )
        map_perms |= MMU__UPDATEMP;

    if ( map_perms )
        rc = domain_has_perm(d, f, SECCLASS_MMU, map_perms);
    return rc;
}

static int cf_check flask_mmuext_op(struct domain *d, struct domain *f)
{
    return domain_has_perm(d, f, SECCLASS_MMU, MMU__MMUEXT_OP);
}

static int cf_check flask_update_va_mapping(
    struct domain *d, struct domain *f, l1_pgentry_t pte)
{
    uint32_t map_perms = MMU__MAP_READ;
    if ( !(l1e_get_flags(pte) & _PAGE_PRESENT) )
        return 0;
    if ( l1e_get_flags(pte) & _PAGE_RW )
        map_perms |= MMU__MAP_WRITE;

    return domain_has_perm(d, f, SECCLASS_MMU, map_perms);
}

static int cf_check flask_priv_mapping(struct domain *d, struct domain *t)
{
    return domain_has_perm(d, t, SECCLASS_MMU, MMU__TARGET_HACK);
}

static int cf_check flask_pmu_op(struct domain *d, unsigned int op)
{
    uint32_t dsid = domain_sid(d);

    switch ( op )
    {
    case CRUXPMU_mode_set:
    case CRUXPMU_mode_get:
    case CRUXPMU_feature_set:
    case CRUXPMU_feature_get:
        return avc_has_perm(dsid, SECINITSID_CRUX, SECCLASS_CRUX2,
                            CRUX2__PMU_CTRL, NULL);
    case CRUXPMU_init:
    case CRUXPMU_finish:
    case CRUXPMU_lvtpc_set:
    case CRUXPMU_flush:
        return avc_has_perm(dsid, SECINITSID_CRUX, SECCLASS_CRUX2,
                            CRUX2__PMU_USE, NULL);
    default:
        return -EPERM;
    }
}
#endif /* CONFIG_X86 */

static int cf_check flask_dm_op(struct domain *d)
{
    return current_has_perm(d, SECCLASS_HVM, HVM__DM);
}

static int cf_check flask_crux_version(uint32_t op)
{
    uint32_t dsid = domain_sid(current->domain);

    switch ( op )
    {
    case CRUXVER_version:
    case CRUXVER_platform_parameters:
    case CRUXVER_get_features:
        /* These sub-ops ignore the permission checks and return data. */
        return 0;
    case CRUXVER_extraversion:
    case CRUXVER_extraversion2:
        return avc_has_perm(dsid, SECINITSID_CRUX, SECCLASS_VERSION,
                            VERSION__CRUX_EXTRAVERSION, NULL);
    case CRUXVER_compile_info:
        return avc_has_perm(dsid, SECINITSID_CRUX, SECCLASS_VERSION,
                            VERSION__CRUX_COMPILE_INFO, NULL);
    case CRUXVER_capabilities:
    case CRUXVER_capabilities2:
        return avc_has_perm(dsid, SECINITSID_CRUX, SECCLASS_VERSION,
                            VERSION__CRUX_CAPABILITIES, NULL);
    case CRUXVER_changeset:
    case CRUXVER_changeset2:
        return avc_has_perm(dsid, SECINITSID_CRUX, SECCLASS_VERSION,
                            VERSION__CRUX_CHANGESET, NULL);
    case CRUXVER_pagesize:
        return avc_has_perm(dsid, SECINITSID_CRUX, SECCLASS_VERSION,
                            VERSION__CRUX_PAGESIZE, NULL);
    case CRUXVER_guest_handle:
        return avc_has_perm(dsid, SECINITSID_CRUX, SECCLASS_VERSION,
                            VERSION__CRUX_GUEST_HANDLE, NULL);
    case CRUXVER_commandline:
    case CRUXVER_commandline2:
        return avc_has_perm(dsid, SECINITSID_CRUX, SECCLASS_VERSION,
                            VERSION__CRUX_COMMANDLINE, NULL);
    case CRUXVER_build_id:
        return avc_has_perm(dsid, SECINITSID_CRUX, SECCLASS_VERSION,
                            VERSION__CRUX_BUILD_ID, NULL);
    default:
        return -EPERM;
    }
}

static int cf_check flask_domain_resource_map(struct domain *d)
{
    return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__RESOURCE_MAP);
}

#ifdef CONFIG_ARGO
static int cf_check flask_argo_enable(const struct domain *d)
{
    return avc_has_perm(domain_sid(d), SECINITSID_CRUX, SECCLASS_ARGO,
                        ARGO__ENABLE, NULL);
}

static int cf_check flask_argo_register_single_source(
    const struct domain *d, const struct domain *t)
{
    return domain_has_perm(d, t, SECCLASS_ARGO,
                           ARGO__REGISTER_SINGLE_SOURCE);
}

static int cf_check flask_argo_register_any_source(const struct domain *d)
{
    return avc_has_perm(domain_sid(d), SECINITSID_CRUX, SECCLASS_ARGO,
                        ARGO__REGISTER_ANY_SOURCE, NULL);
}

static int cf_check flask_argo_send(
    const struct domain *d, const struct domain *t)
{
    return domain_has_perm(d, t, SECCLASS_ARGO, ARGO__SEND);
}

#endif

static int cf_check flask_get_domain_state(struct domain *d)
{
    return current_has_perm(d, SECCLASS_DOMAIN2, DOMAIN2__GET_DOMAIN_STATE);
}

static const struct xsm_ops __initconst_cf_clobber flask_ops = {
    .set_system_active = flask_set_system_active,
    .security_domaininfo = flask_security_domaininfo,
    .domain_create = flask_domain_create,
    .getdomaininfo = flask_getdomaininfo,
    .domctl_scheduler_op = flask_domctl_scheduler_op,
#ifdef CONFIG_SYSCTL
    .sysctl_scheduler_op = flask_sysctl_scheduler_op,
#endif
    .set_target = flask_set_target,
    .domctl = flask_domctl,
#ifdef CONFIG_SYSCTL
    .sysctl = flask_sysctl,
    .readconsole = flask_readconsole,
#endif

    .evtchn_unbound = flask_evtchn_unbound,
    .evtchn_interdomain = flask_evtchn_interdomain,
    .evtchn_close_post = flask_evtchn_close_post,
    .evtchn_send = flask_evtchn_send,
    .evtchn_status = flask_evtchn_status,
    .evtchn_reset = flask_evtchn_reset,

    .grant_mapref = flask_grant_mapref,
    .grant_unmapref = flask_grant_unmapref,
    .grant_setup = flask_grant_setup,
    .grant_transfer = flask_grant_transfer,
    .grant_copy = flask_grant_copy,
    .grant_query_size = flask_grant_query_size,

    .alloc_security_domain = flask_domain_alloc_security,
    .free_security_domain = flask_domain_free_security,
    .alloc_security_evtchns = flask_alloc_security_evtchns,
    .free_security_evtchns = flask_free_security_evtchns,
    .show_security_evtchn = flask_show_security_evtchn,
    .init_hardware_domain = flask_init_hardware_domain,

    .get_pod_target = flask_get_pod_target,
    .set_pod_target = flask_set_pod_target,
    .memory_exchange = flask_memory_exchange,
    .memory_adjust_reservation = flask_memory_adjust_reservation,
    .memory_stat_reservation = flask_memory_stat_reservation,
    .memory_pin_page = flask_memory_pin_page,
    .claim_pages = flask_claim_pages,

    .console_io = flask_console_io,

    .profile = flask_profile,

    .kexec = flask_kexec,
    .schedop_shutdown = flask_schedop_shutdown,

    .show_irq_sid = flask_show_irq_sid,

    .map_domain_pirq = flask_map_domain_pirq,
    .map_domain_irq = flask_map_domain_irq,
    .unmap_domain_pirq = flask_unmap_domain_pirq,
    .unmap_domain_irq = flask_unmap_domain_irq,
    .bind_pt_irq = flask_bind_pt_irq,
    .unbind_pt_irq = flask_unbind_pt_irq,
    .irq_permission = flask_irq_permission,
    .iomem_permission = flask_iomem_permission,
    .iomem_mapping = flask_iomem_mapping,
    .pci_config_permission = flask_pci_config_permission,

    .resource_plug_core = flask_resource_plug_core,
    .resource_unplug_core = flask_resource_unplug_core,
    .resource_plug_pci = flask_resource_plug_pci,
    .resource_unplug_pci = flask_resource_unplug_pci,
    .resource_setup_pci = flask_resource_setup_pci,
    .resource_setup_gsi = flask_resource_setup_gsi,
    .resource_setup_misc = flask_resource_setup_misc,

#ifdef CONFIG_SYSCTL
    .page_offline = flask_page_offline,
#endif
    .hypfs_op = flask_hypfs_op,
    .hvm_param = flask_hvm_param,
    .hvm_param_altp2mhvm = flask_hvm_param_altp2mhvm,
    .hvm_altp2mhvm_op = flask_hvm_altp2mhvm_op,

    .do_xsm_op = do_flask_op,
    .get_vnumainfo = flask_get_vnumainfo,

    .vm_event_control = flask_vm_event_control,

#ifdef CONFIG_VM_EVENT
    .mem_access = flask_mem_access,
#endif

#ifdef CONFIG_MEM_PAGING
    .mem_paging = flask_mem_paging,
#endif

#ifdef CONFIG_MEM_SHARING
    .mem_sharing = flask_mem_sharing,
#endif

#ifdef CONFIG_COMPAT
    .do_compat_op = compat_flask_op,
#endif

    .add_to_physmap = flask_add_to_physmap,
    .remove_from_physmap = flask_remove_from_physmap,
    .map_gmfn_foreign = flask_map_gmfn_foreign,

#if defined(CONFIG_HAS_PASSTHROUGH) && defined(CONFIG_HAS_PCI)
    .get_device_group = flask_get_device_group,
    .assign_device = flask_assign_device,
    .deassign_device = flask_deassign_device,
#endif

#if defined(CONFIG_HAS_PASSTHROUGH) && defined(CONFIG_HAS_DEVICE_TREE_DISCOVERY)
    .assign_dtdevice = flask_assign_dtdevice,
    .deassign_dtdevice = flask_deassign_dtdevice,
#endif

    .platform_op = flask_platform_op,
#ifdef CONFIG_X86
    .do_mca = flask_do_mca,
    .shadow_control = flask_shadow_control,
    .mem_sharing_op = flask_mem_sharing_op,
    .apic = flask_apic,
    .machine_memory_map = flask_machine_memory_map,
    .domain_memory_map = flask_domain_memory_map,
    .mmu_update = flask_mmu_update,
    .mmuext_op = flask_mmuext_op,
    .update_va_mapping = flask_update_va_mapping,
    .priv_mapping = flask_priv_mapping,
    .ioport_permission = flask_ioport_permission,
    .ioport_mapping = flask_ioport_mapping,
    .pmu_op = flask_pmu_op,
#endif
    .dm_op = flask_dm_op,
    .crux_version = flask_crux_version,
    .domain_resource_map = flask_domain_resource_map,
#ifdef CONFIG_ARGO
    .argo_enable = flask_argo_enable,
    .argo_register_single_source = flask_argo_register_single_source,
    .argo_register_any_source = flask_argo_register_any_source,
    .argo_send = flask_argo_send,
#endif
    .get_domain_state = flask_get_domain_state,
};

const struct xsm_ops *__init flask_init(
    const void *policy_buffer, size_t policy_size)
{
    int ret = -ENOENT;

    switch ( flask_bootparam )
    {
    case FLASK_BOOTPARAM_DISABLED:
        printk(CRUXLOG_INFO "Flask: Disabled at boot.\n");
        return NULL;

    case FLASK_BOOTPARAM_PERMISSIVE:
        flask_enforcing = 0;
        break;

    case FLASK_BOOTPARAM_ENFORCING:
    case FLASK_BOOTPARAM_LATELOAD:
        break;

    case FLASK_BOOTPARAM_INVALID:
    default:
        panic("Flask: Invalid value for flask= boot parameter.\n");
        break;
    }

    avc_init();

    if ( policy_size && flask_bootparam != FLASK_BOOTPARAM_LATELOAD )
        ret = security_load_policy(policy_buffer, policy_size);

    if ( ret && flask_bootparam == FLASK_BOOTPARAM_ENFORCING )
        panic("Unable to load FLASK policy\n");

    if ( ret )
        printk(CRUXLOG_INFO "Flask:  Access controls disabled until policy is loaded.\n");
    else if ( flask_enforcing )
        printk(CRUXLOG_INFO "Flask:  Starting in enforcing mode.\n");
    else
        printk(CRUXLOG_INFO "Flask:  Starting in permissive mode.\n");

    return &flask_ops;
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
