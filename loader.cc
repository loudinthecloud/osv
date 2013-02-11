
#include "drivers/isa-serial.hh"
#include "fs/fs.hh"
#include <boost/format.hpp>
#include <cctype>
#include "elf.hh"
#include "tls.hh"
#include "msr.hh"
#include "exceptions.hh"
#include "debug.hh"
#include "drivers/pci.hh"
#include "smp.hh"

#include "drivers/driver.hh"
#include "drivers/virtio-net.hh"
#include "drivers/virtio-blk.hh"

#include "sched.hh"
#include "drivers/clock.hh"
#include "drivers/clockevent.hh"
#include "barrier.hh"
#include "tests/tst-hub.hh"

asm(".pushsection \".debug_gdb_scripts\", \"MS\",@progbits,1 \n"
    ".byte 1 \n"
    ".asciz \"scripts/loader.py\" \n"
    ".popsection \n");

namespace {

    void test_locale()
    {
	auto loc = std::locale();
	auto &fac = std::use_facet<std::ctype<char>>(loc);
	bool ok = fac.is(std::ctype_base::digit, '3')
	    && !fac.is(std::ctype_base::digit, 'x');
	debug(ok ? "locale works" : "locale fails");
	//asm volatile ("1: jmp 1b");
    }

}

elf::Elf64_Ehdr* elf_header;
elf::tls_data tls_data;

void setup_tls(elf::init_table inittab)
{
    tls_data = inittab.tls;
    extern char tcb0[]; // defined by linker script
    memcpy(tcb0, inittab.tls.start, inittab.tls.size);
    auto p = reinterpret_cast<thread_control_block*>(tcb0 + inittab.tls.size);
    p->self = p;
    processor::wrmsr(msr::IA32_FS_BASE, reinterpret_cast<uint64_t>(p));
}

extern "C" {
    void premain();
    void vfs_init(void);
    void ramdisk_init(void);
}


void premain()
{
    auto inittab = elf::get_init(elf_header);
    setup_tls(inittab);
    for (auto init = inittab.start; init < inittab.start + inittab.count; ++init) {
        (*init)();
    }
}

void disable_pic()
{
    outb(0xff, 0x21);
    outb(0xff, 0xa1);
}

using sched::thread;
// since it contains actual code it needs to be after the 'using' declaration
#include "tests/tst-malloc.hh"
#include "tests/tst-threads.hh"


elf::program* prog;

int main(int ac, char **av)
{
    debug("Loader Copyright 2013 Unnamed");

    test_locale();
    idt.load_on_cpu();
    smp_init();
    void main_cont(int ac, char** av);
    sched::init(tls_data, [=] { main_cont(ac, av); });
}

void main_cont(int ac, char** av)
{
    smp_launch();

    vfs_init();
    ramdisk_init();

    filesystem fs;

    disable_pic();
    processor::sti();

    prog = new elf::program(fs);
    void main_thread(int ac, char** av);
    main_thread(ac, av);
}

struct argblock {
    int ac;
    char** av;
};

void run_main(elf::program *prog, struct argblock *args)
{
    auto av = args->av;
    auto ac = args->ac;
    prog->add_object(av[0]);
    ++av, --ac;
    auto osv_main = prog->lookup_function<void (int, char**)>("osv_main");
    osv_main(ac, av);
}

void* do_main_thread(void *_args)
{
    auto args = static_cast<argblock*>(_args);

    //Tests malloc and free using threads.
    unit_tests::tests::instance().execute_tests();
    //test_alloc();
    //test_threads();

    // Enumerate PCI devices
    pci::pci_devices_print();
    pci::pci_device_enumeration();

    // List all devices
    hw::device_manager::instance()->list_devices();

    // Initialize all drivers
    hw::driver_manager* drvman = hw::driver_manager::instance();
    drvman->register_driver(new virtio::virtio_blk(0));
    drvman->register_driver(new virtio::virtio_blk(1));
    drvman->register_driver(new virtio::virtio_net());
    drvman->load_all();
    drvman->list_drivers();

    auto t1 = clock::get()->time();
    auto t2 = clock::get()->time();
    debug(fmt("clock@t1 %1%") % t1);
    debug(fmt("clock@t2 %1%") % t2);

    timespec ts = {};
    ts.tv_nsec = 100;
    t1 = clock::get()->time();
    nanosleep(&ts, nullptr);
    t2 = clock::get()->time();
    debug(fmt("nanosleep(100) -> %d") % (t2 - t1));
    ts.tv_nsec = 100000;
    t1 = clock::get()->time();
    nanosleep(&ts, nullptr);
    t2 = clock::get()->time();
    debug(fmt("nanosleep(100000) -> %d") % (t2 - t1));

    run_main(prog, args);

    while (true)
	;
    return nullptr;
}

void main_thread(int ac, char **av)
{
    pthread_t pthread;
    // run the payload in a pthread, so pthread_self() etc. work
    argblock args{ ac, av };
    pthread_create(&pthread, nullptr, do_main_thread, &args);
    sched::thread::wait_until([] { return false; });
}

int __argc;
char** __argv;
