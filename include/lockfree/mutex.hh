#ifndef LOCKFREE_MUTEX_HH
#define LOCKFREE_MUTEX_HH
// A lock-free mutex implementation, based on the combination of two basic
// techniques:
// 1. Our lock-free multi-producer single-consumer queue technique
//    (see lockfree/queue-mpsc.hh)
// 2. The "responsibility hand-off" (RHO) protocol described in the 2007 paper
//    "Blocking without Locking or LFTHREADS: A lock-free thread library"
//    by Anders Gidenstam and Marina Papatriantafilou.
//
// The operation and correctness of the RHO protocol is discussed in the
// aforementioned G&P 2007 paper, so we will avoid lengthy comments about it
// below, except where we differ from G&P.
//
// One especially important issue that we do need to justify is:
// Our lockfree queue implementation assumes that there cannot be two
// concurrent pop()s. We claim that this is true in the RHO protocol because:
// 1. We have pop() calls at two places:
//    (A) In unlock(), after decrementing count and outside a handoff (=null)
//    (B) in lock(), after picking up a handoff.
// 2. We can't have two threads at (A) at the same time, because one thread
//    at (A) means another thread thread was just in lock() (because count>0),
//    but currently running lock()s cannot complete (and get to unlock and A)
//    until somebody will wake them it (and this is what we're trying to show
//    is impossible), and news lock()s will likewise wait because the waiting
//    lock() is keeping count>0.
// 3. While one lock() is at (B), we cannot have another thread at (A) or (B):
//    This is because in (B) we only pop() after picking a handoff, so other
//    lock()s cannot reach (B) (they did not pick the handoff, we did), and
//    unlock cannot be at (A) because it only reaches (A) before making the
//    handoff of after taking it back - and we know it didn't because we took
//    the handoff.
//
// Another difference from our implementation from G&P is the content of the
// handoff token. G&P use the processor ID, but remark that it is not enough
// because of the ABA problem (it is possible while a CPU running lock() is
// paused, another one finishes unlock(), and then succeeds in another lock()
// and then comes a different unlock() with its unrelated handoff) and suggest
// to add a per-processor sequence number. Instead, we just used a per-mutex
// sequence number. As long as one CPU does not pause for a long enough
// duration for our (currently 32-bit) sequence number to wrap, we won't have
// a problem. A per-mutex sequence number is slower than a per-cpu one, but
// I doubt this will make a practical difference.

#include <atomic>

#include <sched.hh>
#include <lockfree/queue-mpsc.hh>

namespace lockfree {

class mutex {
private:
    std::atomic<int> count;
    // "owner" and "depth" are need for implementing a recursive mutex
    unsigned int depth;
    std::atomic<sched::thread *> owner;
    queue_mpsc<sched::thread *> waitqueue;
    std::atomic<unsigned int> handoff;
    unsigned int sequence;
public:
    mutex() : count(0), depth(0), owner(nullptr), waitqueue(), handoff(0), sequence(0) { }
    ~mutex() { assert(count==0); }

    void lock()
    {
        sched::thread *current = sched::thread::current();

        if (count.fetch_add(1, std::memory_order_acquire) == 0) {
            // Uncontended case (no other thread is holding the lock, and no
            // concurrent lock() attempts). We got the lock.
            owner.store(current, std::memory_order_relaxed);
            depth = 1;
            return;
        }

        // If we're here the mutex was already locked, but we're implementing
        // a recursive mutex so it's possible the lock holder is us - in which
        // case we don't need to increment depth instead of waiting.
        if (owner.load(std::memory_order_relaxed) == current) {
            count.fetch_add(-1, std::memory_order_relaxed);
            ++depth;
            return;
        }

        // If we're here still here the lock is owned by a different thread.
        // Put this thread in a waiting queue, so it will eventually be woken
        // when another thread releases the lock.

        // Mark the thread "waiting" now with wait_guard(). The code from
        // here until the schedule() below must be kept to a bare minimum:
        // It will never be preempted (if we preempt a "waiting" thread, it
        // will never come back), and it cannot run any code expecting a
        // "running" thread - such as another wait_guard() or anything
        // using one (mutexes, malloc, etc.). A good way to think about the
        // situation is that between the wait_guard() and schedule(), the
        // current thread is replaced by a new non-preemptable execution
        // context with the code below.
        sched::wait_guard wait_guard(current);

        linked_item<sched::thread *> waiter(current);
        waitqueue.push(&waiter);

        // The "Responsibility Hand-Off" protocol where a lock() picks from
        // a concurrent unlock() the responsibility of waking somebody up:
        auto old_handoff = handoff.load();
        if (old_handoff) {
             if (!waitqueue.empty()){
                if (handoff.compare_exchange_strong(old_handoff, 0U)) {
                    // Note the explanation above about no concurrent pop()s also
                    // explains why we can be sure waitqueue is still not empty.
                    sched::thread *thread;
                    assert(waitqueue.pop(&thread));
                    assert(depth==0);
                    depth = 1;
                    owner.store(thread);
                    if(thread!=current) {
                        thread->wake();
                        // Note that because of the wait_guard, preemption of
                        // this thread is disabled, so the above wake() will
                        // not reschedule current or change its state.
                    }  else
                        return; // got the lock ourselves
                }
            }
        }

        // Wait until another thread wakes us up. When somebody wakes us,
        // they will set us to be the owner first.
        // TODO: perhaps check if owner isn't already current, in which case no need to reschedule?
        current->tcpu()->schedule(true);
        assert(owner.load(std::memory_order_relaxed)==current);
    }

    bool try_lock(){
        sched::thread *current = sched::thread::current();
        int zero = 0;
        if (count.compare_exchange_strong(zero, 1, std::memory_order_acquire)) {
            // Uncontended case. We got the lock.
            owner.store(current, std::memory_order_relaxed);
            depth = 1;
            return true;
        }

        // We're implementing a recursive mutex -lock may still succeed if
        // this thread is the one holding it.
        if (owner.load(std::memory_order_relaxed) == current) {
            ++depth;
            return true;
        }

        // The lock is taken, and we're almost ready to give up (return
        // false), but the last chance is if we can accept a handoff - and if
        // we do, we got the lock.
        auto old_handoff = handoff.load();
        if(!old_handoff && handoff.compare_exchange_strong(old_handoff, 0U)) {
            count.fetch_add(1, std::memory_order_relaxed);
            owner.store(current);
            assert(depth==0);
            depth = 1;
            return true;
        }
        return false;
    }

    void unlock(){
        // We assume unlock() is only ever called when this thread is holding
        // the lock. For performance reasons, we do not verify that
        // owner.load()==current && depth!=0.
        if (--depth)
            return; // recursive mutex still locked.

        owner.store(nullptr, std::memory_order_relaxed);

        // If there is no waiting lock(), we're done. This is the easy case :-)
        if (count.fetch_add(-1, std::memory_order_release) == 1) {
            return;
        }

        // Otherwise there is at least one concurrent lock(). Awaken one if
        // it's waiting on the waitqueue, otherwise use the RHO protocol to
        // have the lock() responsible for waking someone up.
        // TODO: it's not completely clear to me why more than two
        // iterations can ever be needed. If the loop continues it means
        // another lock() queued itself - but if it did, wouldn't the next
        // iteration just pop and return?
        while(true) {
            sched::thread *thread;
            if (waitqueue.pop(&thread)) {
                depth = 1;
                owner.store(thread, std::memory_order_relaxed);
                //assert(thread!=current); // this thread isn't waiting, we know that :(
                thread->wake();
                return;
            }
            // Some concurrent lock() is in progress (we know this because of
            // count) but it hasn't yet put itself on the wait queue.
            if (++sequence == 0U) ++sequence;  // pick a number, but not 0
            auto ourhandoff = sequence;
            handoff.store(ourhandoff);
            // If the queue is empty, the concurrent lock() is before adding
            // itself, and therefore will definitely find our handoff later.
            if (waitqueue.empty())
                return;
            // A thread already appeared on the queue, let's try to take the
            // handoff ourselves and awaken it. If somebody else already took
            // the handoff, great, we're done - they are responsible now.
            if (!handoff.compare_exchange_strong(ourhandoff, 0U))
                return;
        }
    }
};

}
#endif
