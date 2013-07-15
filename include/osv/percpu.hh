#ifndef PERCPU_HH_
#define PERCPU_HH_

#include <sched.hh>
#include <type_traits>
#include <memory>

extern char _percpu_start[];

extern __thread void* percpu_base;

template <typename T>
class percpu {
public:
    constexpr percpu() {}
    T* operator->() {
        return addr();
    }
    T& operator*() {
        return *addr();
    }
    T* for_cpu(sched::cpu* cpu) {
        return addr(cpu->percpu_base);
    }
private:
    T *addr(void* base = percpu_base) {
        size_t offset = reinterpret_cast<size_t>(&_var);
        return reinterpret_cast<T*>(base + offset);
    }
private:
    T _var;
    friend size_t dynamic_percpu_base();
};

#define PERCPU(type, var) percpu<type> var __attribute__((section(".percpu")))

size_t dynamic_percpu_alloc(size_t size, size_t align);
void dynamic_percpu_free(size_t offset, size_t size);

template <typename T, size_t align = std::alignment_of<T>::value>
class dynamic_percpu {
public:
    dynamic_percpu()
        : _offset(dynamic_percpu_alloc(sizeof(T), align))
        , _notifier(new sched::cpu::notifier([=] { new (addr()) T(); }))
    {
        for (auto c : sched::cpus) {
            new (for_cpu(c)) T();
        }
    }
    ~dynamic_percpu() {
        for (auto c : sched::cpus) {
            for_cpu(c)->~T();
        }
        dynamic_percpu_free(_offset, sizeof(T));
    }
    T* operator->() { return addr(); }
    T& operator*() { return *addr(); }
    T* for_cpu(sched::cpu* cpu) { return addr(cpu->percpu_base); }
private:
    T* addr(void* base = percpu_base) {
        return static_cast<T*>(base + _offset);
    }
private:
    size_t _offset;
    std::unique_ptr<sched::cpu::notifier> _notifier;
};


void percpu_init(unsigned cpu);

#endif /* PERCPU_HH_ */
