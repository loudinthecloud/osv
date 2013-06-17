#include <string.h>
#include "mempool.hh"
#include "mmu.hh"
#include "sglist.hh"
#include "barrier.hh"

#include "virtio.hh"
#include "drivers/virtio-vring.hh"
#include "debug.hh"

#include "sched.hh"
#include "interrupt.hh"
#include "osv/trace.hh"

using namespace memory;
using sched::thread;

TRACEPOINT(trace_virtio_enable_interrupts, "vring=%p", void *);
TRACEPOINT(trace_virtio_disable_interrupts, "vring=%p", void *);

namespace virtio {

    vring::vring(virtio_driver* const dev, u16 num, u16 q_index)
    {
        _dev = dev;
        _q_index = q_index;
        // Alloc enough pages for the vring...
        unsigned sz = VIRTIO_ALIGN(vring::get_size(num, VIRTIO_PCI_VRING_ALIGN));
        _vring_ptr = malloc(sz);
        memset(_vring_ptr, 0, sz);
        
        // Set up pointers        
        _num = num;
        _desc = (vring_desc *)_vring_ptr;
        _avail = (vring_avail *)(_vring_ptr + num*sizeof(vring_desc));
        _used = (vring_used *)(((unsigned long)&_avail->_ring[num] + 
                sizeof(u16) + VIRTIO_PCI_VRING_ALIGN-1) & ~(VIRTIO_PCI_VRING_ALIGN-1));

        // initialize the next pointer within the available ring
        for (int i=0;i<num;i++) _desc[i]._next = i+1;
        _desc[num-1]._next = 0;

        _cookie = new void*[num];

        _avail_head = 0;
        _used_guest_head = 0;
        _avail_added_since_kick = 0;
        _avail_count = num;
    }

    vring::~vring()
    {
        free(_vring_ptr);
        delete [] _cookie;
    }

    u64 vring::get_paddr(void)
    {
        return mmu::virt_to_phys(_vring_ptr);
    }

    unsigned vring::get_size(unsigned int num, unsigned long align)
    {
        return (((sizeof(vring_desc) * num + sizeof(u16) * (3 + num)
                 + align - 1) & ~(align - 1))
                + sizeof(u16) * 3 + sizeof(vring_used_elem) * num);
    }

    int vring::need_event(u16 event_idx, u16 new_idx, u16 old)
    {
        // Note: Xen has similar logic for notification hold-off
        // in include/xen/interface/io/ring.h with req_event and req_prod
        // corresponding to event_idx + 1 and new_idx respectively.
        // Note also that req_event and req_prod in Xen start at 1,
        // event indexes in virtio start at 0.
        return ( (u16)(new_idx - event_idx - 1) < (u16)(new_idx - old) );
    }

    void vring::disable_interrupts()
    {
        trace_virtio_disable_interrupts(this);
        _avail->disable_interrupt();
    }

    void vring::enable_interrupts()
    {
        trace_virtio_enable_interrupts(this);
        _avail->enable_interrupt();
    }

    // The convention is that out descriptors are at the beginning of the sg list
    // TODO: add barriers
    bool
    vring::add_buf(sglist* sg, u16 out, u16 in, void* cookie) {
        return with_lock(_lock, [=] {
            int desc_needed = in + out;
            if (_dev->get_indirect_buf_cap())
                desc_needed = 1;

            if (_avail_count < desc_needed) {
                //make sure the interrupts get there
                //it probably should force an exit to the host
                kick();
                return false;
            }

            int i = 0, idx, prev_idx = -1;
            idx = _avail_head;

            virtio_d("\t%s: avail_head=%d, in=%d, out=%d", __FUNCTION__, _avail_head, in, out);
            _cookie[idx] = cookie;
            vring_desc* descp = _desc;

            if (_dev->get_indirect_buf_cap()) {
                vring_desc* indirect = reinterpret_cast<vring_desc*>(malloc((in+out)*sizeof(vring_desc)));
                if (!indirect)
                    return false;
                _desc[idx]._flags = vring_desc::VRING_DESC_F_INDIRECT;
                _desc[idx]._paddr = mmu::virt_to_phys(indirect);
                _desc[idx]._len = (in+out)*sizeof(vring_desc);

                descp = indirect;
                //initialize the next pointers
                for (int j=0;j<in+out;j++) descp[j]._next = j+1;
                //hack to make the logic below the for loop below act
                //just as before
                descp[in+out-1]._next = _desc[idx]._next;
                idx = 0;
            }

            for (auto ii = sg->_nodes.begin(); i < in + out; ii++, i++) {
                virtio_d("\t%s: idx=%d, len=%d, paddr=%x", __FUNCTION__, idx, (*ii)._len, (*ii)._paddr);
                descp[idx]._flags = vring_desc::VRING_DESC_F_NEXT;
                descp[idx]._flags |= (i>=out)? vring_desc::VRING_DESC_F_WRITE:0;
                descp[idx]._paddr = (*ii)._paddr;
                descp[idx]._len = (*ii)._len;
                prev_idx = idx;
                idx = descp[idx]._next;
            }
            descp[prev_idx]._flags &= ~vring_desc::VRING_DESC_F_NEXT;

            _avail_added_since_kick++;
            _avail_count -= desc_needed;

            _avail->_ring[(_avail->_idx) % _num] = _avail_head;
            barrier();
            _avail->_idx++;
            _avail_head = idx;

            virtio_d("\t%s: _avail->_idx=%d, added=%d,", __FUNCTION__, _avail->_idx, _avail_added_since_kick);

            return true;
        });
    }

    void*
    vring::get_buf(u32 *len)
    {
        return with_lock(_lock, [=] {
            vring_used_elem elem;
            void* cookie = reinterpret_cast<void*>(0);
            int i = 1;

            // need to trim the free running counter w/ the array size
            int used_ptr = _used_guest_head % _num;

            barrier(); // Normalize the used fields of these descriptors
            if (_used_guest_head == _used->_idx) {
                virtio_d("get_used_desc: no avail buffers ptr=%d", _used_guest_head);
                return reinterpret_cast<void*>(0);
            }

            virtio_d("get used: guest head=%d use_elem[head].id=%d", used_ptr, _used->_used_elements[used_ptr]._id);
            elem = _used->_used_elements[used_ptr];
            int idx = elem._id;
            *len = elem._len;

            if (_desc[idx]._flags & vring_desc::VRING_DESC_F_INDIRECT) {
                free(mmu::phys_to_virt(_desc[idx]._paddr));
            } else
                while (_desc[idx]._flags & vring_desc::VRING_DESC_F_NEXT) {
                    idx = _desc[idx]._next;
                    i++;
                }

            cookie = _cookie[elem._id];
            _cookie[elem._id] = reinterpret_cast<void*>(0);

            _used_guest_head++;
            _avail_count += i;
            barrier();
            _desc[idx]._next = _avail_head;
            _avail_head = elem._id;

            return cookie;
        });
    }

    bool vring::avail_ring_not_empty()
    {
        return (_avail_count > 0);
    }

    bool vring::avail_ring_has_room(int descriptors)
    {
        if (_dev->get_indirect_buf_cap())
            descriptors--;
        return (_avail_count >= descriptors);
    }

    bool vring::used_ring_not_empty()
    {
        return (_used_guest_head != _used->_idx);
    }

    bool
    vring::kick() {
        _dev->kick(_q_index);
        _avail_added_since_kick = 0;
        return true;
    }

};
