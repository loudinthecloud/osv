/*
 * Copyright (C) 2014 Huawei Technologies Duesseldorf GmbH
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <osv/debug.hh>
#include <osv/prio.hh>
#include <osv/sched.hh>

#include "exceptions.hh"
#include "gic.hh"

__thread exception_frame* current_interrupt_frame;
class interrupt_table idt __attribute__((init_priority((int)init_prio::idt)));

interrupt_table::interrupt_table() {
    debug_early_entry("interrupt_table::interrupt_table()");

    /* XXX hardcoded addresses */
    gic::gic = new gic::gic_driver(0x8001000, 0x8002000);
    gic::gic->init_cpu(0);
    gic::gic->init_dist(0);

    this->nr_irqs = gic::gic->nr_irqs;

    debug_early("interrupt table: gic driver created.\n");
}

void interrupt_table::enable_irqs()
{
    WITH_LOCK(osv::rcu_read_lock) {
        for (int i = 0; i < this->nr_irqs; i++) {
            struct interrupt_desc *desc = this->irq_desc[i].read();
            if (desc && desc->handler) {
                debug_early_u64("enabling InterruptID=", desc->id);
                gic::gic->set_irq_type(desc->id, desc->type);
                gic::gic->unmask_irq(desc->id);
            }
        }
    }
}

void interrupt_table::register_handler(int i, interrupt_handler h,
                                       gic::irq_type t)
{
    WITH_LOCK(_lock) {
        assert(i < this->nr_irqs);
        struct interrupt_desc *old = this->irq_desc[i].read_by_owner();

        if (old) {
            debug_early_u64("already registered IRQ id=", (u64)i);
            return;
        }

        struct interrupt_desc *desc = new interrupt_desc(i, h, t);
        this->irq_desc[i].assign(desc);
        osv::rcu_dispose(old);

        debug_early_u64("registered IRQ id=", (u64)i);
    }
}

int interrupt_table::invoke_interrupt(int id)
{
    WITH_LOCK(osv::rcu_read_lock) {
        assert(id < this->nr_irqs);
        struct interrupt_desc *desc = this->irq_desc[id].read();

        if (!desc || !desc->handler) {
            return 0;
        }

        desc->handler(desc);
        return 1;
    }
}

extern "C" { void interrupt(exception_frame* frame); }

void interrupt(exception_frame* frame)
{
    /* remember frame in a global, need to change if going to nested */
    current_interrupt_frame = frame;

    unsigned int iar = gic::gic->ack_irq();
    int irq = iar & 0x3ff;

    /* note that special values 1022 and 1023 are used for
       group 1 and spurious interrupts respectively. */
    if (irq >= gic::gic->nr_irqs) {
        printf("special InterruptID %x detected!\n");

    } else {
        if (!idt.invoke_interrupt(irq))
            printf("unhandled InterruptID %x!\n", irq);
        gic::gic->end_irq(iar);
    }

    current_interrupt_frame = nullptr;
    sched::preempt();
}
