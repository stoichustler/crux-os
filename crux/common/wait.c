/******************************************************************************
 * wait.c
 * 
 * Sleep in hypervisor context for some event to occur.
 * 
 * Copyright (c) 2010, Keir Fraser <keir@crux.org>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; If not, see <http://www.gnu.org/licenses/>.
 */

#include <crux/sched.h>
#include <crux/softirq.h>
#include <crux/wait.h>
#include <crux/errno.h>

struct waitqueue_vcpu {
    struct list_head list;
    struct vcpu *vcpu;
};

int init_waitqueue_vcpu(struct vcpu *v)
{
    struct waitqueue_vcpu *wqv;

    wqv = xzalloc(struct waitqueue_vcpu);
    if ( wqv == NULL )
        return -ENOMEM;

    INIT_LIST_HEAD(&wqv->list);
    wqv->vcpu = v;

    v->waitqueue_vcpu = wqv;

    return 0;
}

void destroy_waitqueue_vcpu(struct vcpu *v)
{
    struct waitqueue_vcpu *wqv;

    wqv = v->waitqueue_vcpu;
    if ( wqv == NULL )
        return;

    BUG_ON(!list_empty(&wqv->list));

    xfree(wqv);

    v->waitqueue_vcpu = NULL;
}

void init_waitqueue_head(struct waitqueue_head *wq)
{
    spin_lock_init(&wq->lock);
    INIT_LIST_HEAD(&wq->list);
}

void destroy_waitqueue_head(struct waitqueue_head *wq)
{
    wake_up_all(wq);
}

void wake_up_nr(struct waitqueue_head *wq, unsigned int nr)
{
    struct waitqueue_vcpu *wqv;

    spin_lock(&wq->lock);

    while ( !list_empty(&wq->list) && nr-- )
    {
        wqv = list_entry(wq->list.next, struct waitqueue_vcpu, list);
        list_del_init(&wqv->list);
        vcpu_unpause(wqv->vcpu);
        put_domain(wqv->vcpu->domain);
    }

    spin_unlock(&wq->lock);
}

void wake_up_one(struct waitqueue_head *wq)
{
    wake_up_nr(wq, 1);
}

void wake_up_all(struct waitqueue_head *wq)
{
    wake_up_nr(wq, UINT_MAX);
}

#define __prepare_to_wait(wqv) ((void)0)
#define __finish_wait(wqv)     ((void)0)

void prepare_to_wait(struct waitqueue_head *wq)
{
    struct vcpu *curr = current;
    struct waitqueue_vcpu *wqv = curr->waitqueue_vcpu;

    ASSERT_NOT_IN_ATOMIC();
    __prepare_to_wait(wqv);

    ASSERT(list_empty(&wqv->list));
    spin_lock(&wq->lock);
    list_add_tail(&wqv->list, &wq->list);
    vcpu_pause_nosync(curr);
    get_knownalive_domain(curr->domain);
    spin_unlock(&wq->lock);
}

void finish_wait(struct waitqueue_head *wq)
{
    struct vcpu *curr = current;
    struct waitqueue_vcpu *wqv = curr->waitqueue_vcpu;

    __finish_wait(wqv);

    if ( list_empty(&wqv->list) )
        return;

    spin_lock(&wq->lock);
    if ( !list_empty(&wqv->list) )
    {
        list_del_init(&wqv->list);
        vcpu_unpause(curr);
        put_domain(curr->domain);
    }
    spin_unlock(&wq->lock);
}
