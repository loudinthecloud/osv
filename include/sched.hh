#ifndef SCHED_HH_
#define SCHED_HH_

#include "arch-thread-state.hh"
#include "arch-cpu.hh"
#include <functional>
#include "tls.hh"
#include "elf.hh"
#include "drivers/clockevent.hh"
#include <boost/intrusive/set.hpp>
#include <boost/intrusive/list.hpp>
#include "mutex.hh"
#include <atomic>
#include "osv/lockless-queue.hh"

extern "C" {
void smp_main();
};
void smp_launch();

namespace sched {

class thread;
class cpu;
class timer;
class timer_list;

void schedule(bool yield = false);

extern "C" {
    void thread_main_c(thread* t);
};

namespace bi = boost::intrusive;

class timer : public bi::set_base_hook<>, public bi::list_base_hook<> {
public:
    explicit timer(thread& t);
    ~timer();
    void set(u64 time);
    bool expired() const;
    void cancel();
    friend bool operator<(const timer& t1, const timer& t2);
private:
    void expire();
private:
    thread& _t;
    bool _expired;
    u64 _time;
    friend class timer_list;
};

class thread {
public:
    struct stack_info {
        stack_info();
        stack_info(void* begin, size_t size);
        void* begin;
        size_t size;
        bool owned; // by thread
    };
    struct attr {
        stack_info stack;
        bool pinned;
    };

public:
    explicit thread(std::function<void ()> func, attr attributes = attr(),
            bool main = false);
    ~thread();
    template <class Pred>
    static void wait_until(Pred pred);
    void wake();
    static void sleep_until(u64 abstime);
    static void yield();
    static thread* current();
    stack_info get_stack_info();
    cpu* tcpu();
    void join();
private:
    void main();
    void switch_to();
    void switch_to_first();
    void prepare_wait();
    void wait();
    void stop_wait();
    void init_stack();
    void setup_tcb();
    void complete();
    static void on_thread_stack(thread* t);
private:
    std::function<void ()> _func;
    thread_state _state;
    thread_control_block* _tcb;
    bool _on_runqueue;
    std::atomic_bool _waiting;
    attr _attr;
    cpu* _cpu;
    bool _terminated;
    bi::list<timer> _active_timers;
    friend void thread_main_c(thread* t);
    friend class wait_guard;
    friend class cpu;
    friend class timer;
    friend void ::smp_main();
    friend void ::smp_launch();
    friend void init(elf::tls_data tls, std::function<void ()> cont);
public:
    thread* _joiner;
    bi::list_member_hook<> _runqueue_link;
    // see cpu class
    lockless_queue_link<thread> _wakeup_link;
    // for the debugger
    bi::list_member_hook<> _thread_list_link;
};

class timer_list {
public:
    void fired();
private:
    friend class timer;
    bi::set<timer, bi::base_hook<bi::set_base_hook<>>> _list;
    class callback_dispatch : private clock_event_callback {
    public:
        callback_dispatch();
        virtual void fired();
    };
    static callback_dispatch _dispatch;
};

typedef bi::list<thread,
                 bi::member_hook<thread,
                                 bi::list_member_hook<>,
                                 &thread::_runqueue_link>
                > runqueue_type;

struct cpu {
    unsigned id;
    struct arch_cpu arch;
    thread* bringup_thread;
    runqueue_type runqueue;
    timer_list timers;
    // for each cpu, a list of threads that are migrating into this cpu:
    typedef lockless_queue<thread, &thread::_wakeup_link> incoming_wakeup_queue;
    incoming_wakeup_queue* incoming_wakeups;
    static cpu* current();
    void init_on_cpu();
    void schedule(bool yield = false);
    void handle_incoming_wakeups();
};

thread* current();

class wait_guard {
public:
    wait_guard(thread* t) : _t(t) { t->prepare_wait(); }
    ~wait_guard() { _t->stop_wait(); }
private:
    thread* _t;
};

// does not return - continues to @cont instead
void init(elf::tls_data tls_data, std::function<void ()> cont);

template <class Pred>
void thread::wait_until(Pred pred)
{
    thread* me = current();
    wait_guard waiter(me);
    while (!pred()) {
        me->wait();
    }
}

extern cpu __thread* current_cpu;

inline cpu* thread::tcpu()
{
    return _cpu;
}

inline cpu* cpu::current()
{
    return thread::current()->tcpu();
}

extern std::vector<cpu*> cpus;

}

#endif /* SCHED_HH_ */
