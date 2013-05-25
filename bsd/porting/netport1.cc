#include <osv/types.h>
#include "drivers/clock.hh"
#include "sched.hh"
#include "mempool.hh"

extern "C" {
    #include <time.h>
    #include <bsd/porting/netport.h>

    int tick = 1;
}

int osv_curtid(void)
{
    return (sched::thread::current()->id());
}

void ntm2tv(u64 ntm, struct timeval *tvp)
{
    u64 utm = ntm / 1000L;

    tvp->tv_sec = ntm/TSECOND;
    tvp->tv_usec = (utm - (tvp->tv_sec*TMILISECOND));
}

void getmicrotime(struct timeval *tvp)
{
    u64 ntm = clock::get()->time();
    ntm2tv(ntm, tvp);
}

void getmicrouptime(struct timeval *tvp)
{
    /* FIXME: OSv - initialize time_uptime */
    u64 ntm = clock::get()->time() - time_uptime;
    ntm2tv(ntm, tvp);
}

int get_ticks(void)
{
    u64 ntm = clock::get()->time();
    return (ns2ticks(ntm));
}

size_t get_physmem(void)
{
    return memory::phys_mem_size / memory::page_size;
}
