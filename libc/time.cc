/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <api/utime.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <osv/stubbing.hh>
#include "libc.hh"
#include <osv/clock.hh>
#include "sched.hh"

u64 convert(const timespec& ts)
{
    return ts.tv_sec * 1000000000 + ts.tv_nsec;
}

extern "C"
int gettimeofday(struct timeval* tv, struct timezone* tz)
{
    if (!tv) {
        return 0;
    }
    u64 time = clock::get()->time();
    auto sec = time / 1000000000;
    auto nsec = time % 1000000000;
    tv->tv_sec = sec;
    tv->tv_usec = nsec / 1000;
    return 0;
}

int nanosleep(const struct timespec* req, struct timespec* rem)
{
    sched::thread::sleep_until(clock::get()->time() + convert(*req));
    return 0;
}

int usleep(useconds_t usec)
{
    sched::thread::sleep_until(clock::get()->time() + usec * 1_us);
    return 0;
}

// Temporary until all clock primitives functions on std::chrono
static void fill_ts(s64 time, struct timespec *ts)
{
    ts->tv_sec  =  time / 1000000000;
    ts->tv_nsec =  time % 1000000000;
}

static void fill_ts(std::chrono::time_point<osv::clock::uptime>::duration t,
                    struct timespec *ts)
{
    ts->tv_sec = std::chrono::duration_cast<std::chrono::seconds>(t).
                    count();
    ts->tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(t).
                    count() % 1000000000;
}

int clock_gettime(clockid_t clk_id, struct timespec* ts)
{
    switch (clk_id) {
    case CLOCK_MONOTONIC:
        fill_ts(osv::clock::uptime::now().time_since_epoch(), ts);
        break;
    case CLOCK_REALTIME:
    case CLOCK_REALTIME_COARSE:
        fill_ts(clock::get()->time(), ts);
        break;
    case CLOCK_PROCESS_CPUTIME_ID:
        // FIXME: discount idle time
        fill_ts(osv::clock::uptime::now().time_since_epoch() * sched::cpus.size(), ts);
        break;
    case CLOCK_THREAD_CPUTIME_ID:
        fill_ts(sched::thread::current()->thread_clock(), ts);
        break;

    default:
        if (clk_id < _OSV_CLOCK_SLOTS) {
            return libc_error(EINVAL);
        } else {
            auto thread = sched::thread::find_by_id(clk_id - _OSV_CLOCK_SLOTS);
            fill_ts(thread->thread_clock(), ts);
        }
    }

    return 0;
}

extern "C"
int __clock_gettime(clockid_t clk_id, struct timespec* ts) __attribute__((alias("clock_gettime")));

int clock_getres(clockid_t clk_id, struct timespec* ts)
{
    switch (clk_id) {
    case CLOCK_REALTIME:
    case CLOCK_REALTIME_COARSE:
    case CLOCK_PROCESS_CPUTIME_ID:
    case CLOCK_THREAD_CPUTIME_ID:
    case CLOCK_MONOTONIC:
        break;
    default:
        if (clk_id < _OSV_CLOCK_SLOTS) {
            return libc_error(EINVAL);
        }
    }

    if (ts) {
        ts->tv_sec = 0;
        ts->tv_nsec = 1;
    }
    return 0;
}

int clock_getcpuclockid(pid_t pid, clockid_t* clock_id)
{
    return CLOCK_PROCESS_CPUTIME_ID;
}

clock_t clock (void)
{
    WARN_STUBBED();
    return -1;
}

NO_SYS(int utime(const char *, const struct utimbuf *));
