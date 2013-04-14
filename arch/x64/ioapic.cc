#include "interrupt.hh"
#include "exceptions.hh"
#include "mutex.hh"
#include "mmu.hh"

namespace ioapic {

constexpr u64 base_phys = 0xfec00000;
volatile void* const base = mmu::phys_cast<void>(0xfec00000);
constexpr unsigned index_reg_offset = 0;
constexpr unsigned data_reg_offset = 0x10;

mutex mtx;

volatile u32* index_reg()
{
    return static_cast<volatile u32*>(base + index_reg_offset);
}

volatile u32* data_reg()
{
    return static_cast<volatile u32*>(base + data_reg_offset);
}

u32 read(unsigned reg)
{
    *index_reg() = reg;
    return *data_reg();
}

void write(unsigned reg, u32 data)
{
    *index_reg() = reg;
    *data_reg() = data;
}

void init()
{
    mmu::linear_map(reinterpret_cast<uintptr_t>(base), base_phys, 4096, 4096);
}

}

using namespace ioapic;

gsi_edge_interrupt::gsi_edge_interrupt(unsigned gsi, std::function<void ()> handler)
    : _gsi(gsi)
    , _vector(idt.register_handler(handler))
{
    with_lock(mtx, [=] {
        write(0x10 + gsi * 2 + 1, sched::cpus[0]->arch.apic_id << 24);
        write(0x10 + gsi * 2, _vector);
    });
}

gsi_edge_interrupt::~gsi_edge_interrupt()
{
    with_lock(mtx, [=] {
        write(0x10 + _gsi * 2, 1 << 16);  // mask
    });
    idt.unregister_handler(_vector);
}


