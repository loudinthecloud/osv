#ifndef DEBUG_CONSOLE_HH_
#define DEBUG_CONSOLE_HH_

#include "console.hh"
#include <osv/mutex.h>

// Wrap a Console with a spinlock, used for debugging
// (we can't use a mutex, since we might want to debug the scheduler)

class debug_console : public Console {
public:
    void set_impl(Console* impl);
    virtual void write(const char *str, size_t len);
    virtual void newline();
    virtual bool input_ready() override;
    virtual char readch();
private:
    Console* _impl;
    spinlock _lock;
};


#endif /* DEBUG_CONSOLE_HH_ */
