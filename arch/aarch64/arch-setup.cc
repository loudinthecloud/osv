/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * Copyright (C) 2014 Huawei Technologies Duesseldorf GmbH
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "arch-setup.hh"
#include <osv/sched.hh>
#include <osv/mempool.hh>
#include <osv/elf.hh>
#include <osv/types.h>
#include <string.h>
#include <osv/boot.hh>
#include <osv/debug.hh>
#include <osv/commands.hh>

#include "arch-mmu.hh"

extern elf::Elf64_Ehdr* elf_header;
extern size_t elf_size;
extern void* elf_start;
extern boot_time_chart boot_time;

char *cmdline;

void parse_cmdline(char* cmdline)
{
    osv::parse_cmdline(cmdline);
}

void setup_temporary_phys_map()
{
    // duplicate 1:1 mapping into phys_mem
    u64 *pt_ttbr0 = reinterpret_cast<u64*>(processor::read_ttbr0());
    u64 *pt_ttbr1 = reinterpret_cast<u64*>(processor::read_ttbr1());
    for (auto&& area : mmu::identity_mapped_areas) {
        auto base = reinterpret_cast<void*>(get_mem_area_base(area));
        pt_ttbr1[mmu::pt_index(base, 3)] = pt_ttbr0[0];
    }
    mmu::flush_tlb_all();
}

void arch_setup_free_memory()
{
    setup_temporary_phys_map();

    register u64 edata;
    asm ("adrp %0, .edata" : "=r"(edata));

    elf_start = reinterpret_cast<void*>(elf_header);
    elf_size = (u64)edata - (u64)elf_start;

    mmu::phys addr = (mmu::phys)elf_start + elf_size + 0x200000;
    addr = addr & ~0x1fffffull;

    /* set in stone for now, 512MB */
    memory::phys_mem_size = 0x20000000;
    mmu::free_initial_memory_range(addr, memory::phys_mem_size);

    /* linear_map [TTBR1] */
    for (auto&& area : mmu::identity_mapped_areas) {
        auto base = reinterpret_cast<void*>(get_mem_area_base(area));
        mmu::linear_map(base + addr, addr, memory::phys_mem_size);
    }

    /* linear_map [TTBR0 - ELF] */
    mmu::linear_map((void*)0x40000000, (mmu::phys)0x40000000, addr - 0x40000000);
    /* linear_map [TTBR0 - UART] */
    mmu::linear_map((void *)0x9000000, (mmu::phys)0x9000000, 0x1000);
    /* linear_map [TTBR0 - GIC DIST] */
    mmu::linear_map((void *)0x8000000, (mmu::phys)0x8000000, 0x10000);
    /* linear_map [TTBR0 - GIC CPU interface] */
    mmu::linear_map((void *)0x8010000, (mmu::phys)0x8010000, 0x10000);

    mmu::switch_to_runtime_page_tables();

    parse_cmdline(cmdline);
}

void arch_setup_tls(void *tls, void *start, size_t size)
{
    struct thread_control_block *tcb;
    memset(tls, 0, size + 1024);

    tcb = (thread_control_block *)tls;
    tcb[0].tls_base = &tcb[1];

    memcpy(&tcb[1], start, size);
    asm volatile ("msr tpidr_el0, %0; isb; " :: "r"(tcb) : "memory");

    /* check that the tls variable preempt_counter is correct */
    assert(sched::get_preempt_counter() == 1);
}

void arch_init_premain()
{
}

void arch_init_drivers()
{
}

#include "drivers/console.hh"
#include "drivers/pl011.hh"

bool arch_setup_console(std::string opt_console)
{
    if (opt_console.compare("pl011") == 0) {
        console::console_driver_add(new console::PL011_Console());
    } else if (opt_console.compare("all") == 0) {
        console::console_driver_add(new console::PL011_Console());
    } else {
        return false;
    }
    return true;
}
