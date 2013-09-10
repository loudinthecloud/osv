#ifndef MMU_HH
#define MMU_HH

#include "fs/fs.hh"
#include <stdint.h>
#include <boost/intrusive/set.hpp>
#include <osv/types.h>
#include <functional>
#include <osv/error.h>

namespace mmu {

constexpr uintptr_t page_size = 4096;

typedef uint64_t f_offset;

static constexpr char* phys_mem = reinterpret_cast<char*>(0xffffc00000000000);
// area for debug allocations:
static constexpr char* debug_base = reinterpret_cast<char*>(0xffffe00000000000);

inline unsigned pt_index(void *virt, unsigned level)
{
    auto v = reinterpret_cast<ulong>(virt);
    return (v >> (12 + level * 9)) & 511;
}

enum {
    perm_read = 1,
    perm_write = 2,
    perm_exec = 4,
    perm_rx = perm_read | perm_exec,
    perm_rw = perm_read | perm_write,
    perm_rwx = perm_read | perm_write | perm_exec,
};

class vma {
public:
    vma(uintptr_t start, uintptr_t end);
    virtual ~vma();
    void set(uintptr_t start, uintptr_t end);
    uintptr_t start() const;
    uintptr_t end() const;
    void* addr() const;
    uintptr_t size() const;
    virtual void split(uintptr_t edge);
    virtual error sync(uintptr_t start, uintptr_t end);
protected:
    uintptr_t _start;
    uintptr_t _end;
public:
    boost::intrusive::set_member_hook<> _vma_list_hook;
};

class file_vma : public vma {
public:
    file_vma(uintptr_t start, uintptr_t end, fileref file, f_offset offset, bool shared);
    virtual void split(uintptr_t edge) override;
    virtual error sync(uintptr_t start, uintptr_t end) override;
private:
    f_offset offset(uintptr_t addr);
    fileref _file;
    f_offset _offset;
    bool _shared;
};

void* map_file(void* addr, size_t size, bool search, unsigned perm,
              fileref file, f_offset offset, bool shared);
void* map_anon(void* addr, size_t size, bool search, unsigned perm);
void unmap(void* addr, size_t size);
int protect(void *addr, size_t size, unsigned int perm);
error msync(void* addr, size_t length, int flags);
bool ismapped(void *addr, size_t size);
bool isreadable(void *addr, size_t size);

typedef uint64_t phys;
phys virt_to_phys(void *virt);
void* phys_to_virt(phys pa);

template <typename T>
T* phys_cast(phys pa)
{
    return static_cast<T*>(phys_to_virt(pa));
}

void linear_map(void* virt, phys addr, size_t size, size_t slop);
void free_initial_memory_range(uintptr_t addr, size_t size);
void switch_to_runtime_page_table();
void set_nr_page_sizes(unsigned nr);

void vpopulate(void* addr, size_t size);
void vdepopulate(void* addr, size_t size);

}

#endif
