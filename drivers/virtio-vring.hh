#ifndef VIRTIO_VRING_H
#define VIRTIO_VRING_H

#include <functional>
#include "mutex.hh"
#include "debug.hh"

class sglist;

#define virtio_tag "virtio"
#define virtio_d(fmt)   logger::instance()->log(virtio_tag, logger::logger_debug, (fmt))
#define virtio_i(fmt)   logger::instance()->log(virtio_tag, logger::logger_info, (fmt))
#define virtio_w(fmt)   logger::instance()->log(virtio_tag, logger::logger_warn, (fmt))
#define virtio_e(fmt)   logger::instance()->log(virtio_tag, logger::logger_error, (fmt))

namespace virtio {

class virtio_vring;
class virtio_device;

    // Buffer descriptors in the ring
    class vring_desc {
    public:
        enum {
            // This marks a buffer as continuing via the next field.
            VRING_DESC_F_NEXT=1,
            // This marks a buffer as write-only (otherwise read-only).
            VRING_DESC_F_WRITE=2,
            // This means the buffer contains a list of buffer descriptors.
            VRING_DESC_F_INDIRECT=4
        };

        u64 get_paddr(void);
        u32 get_len(void) { return (_len); }
        u16 next_idx(void) { return (_next); }

        // flags
        bool is_chained(void) { return ((_flags & VRING_DESC_F_NEXT) == VRING_DESC_F_NEXT); };
        bool is_write(void) { return ((_flags & VRING_DESC_F_WRITE) == VRING_DESC_F_WRITE); };
        bool is_indirect(void) { return ((_flags & VRING_DESC_F_INDIRECT) == VRING_DESC_F_INDIRECT); };
        
        u64 _paddr;
        u32 _len;
        u16 _flags;
        u16 _next;
    };

    // Guest to host
    class vring_avail {
    public:
        enum {
            // Mark that we do not need an interrupt for consuming a descriptor
            // from the ring. Unrelieable so it's simply an optimization
            VRING_AVAIL_F_NO_INTERRUPT=1
        };

        void disable_interrupt(void) { _flags |= VRING_AVAIL_F_NO_INTERRUPT; }
        void enable_interrupt(void) { _flags = 0;}
       
        u16 _flags;
        // Where we put the next descriptor
        u16 _idx;
        // There may be no more entries than the queue size read from device
        u16 _ring[];
        // used event index is an optimization in order to get an interrupt from the host
        // only when the value reaches this number
        // FIXME: broken; can't put a field after a variable legth array.
        // try mode=release
        //u16 used_event;
    };

    class vring_used_elem {
    public:
        // Index of start of used vring_desc chain. (u32 for padding reasons)
        u32 _id;
        // Total length of the descriptor chain which was used (written to)
        u32 _len;
    };

    // Host to guest
    class vring_used {
    public:

        enum {
            // The Host advise the Guest: don't kick me when
            // you add a buffer.  It's unreliable, so it's simply an 
            // optimization. Guest will still kick if it's out of buffers.
            VRING_USED_F_NO_NOTIFY=1
        };

        void disable_interrupt(void) { _flags |= VRING_USED_F_NO_NOTIFY; }
        void enable_interrupt(void) { _flags = 0; }
        
        u16 _flags;
        u16 _idx;
        vring_used_elem _used_elements[];
    };

    class vring {
    public:

        enum {

            /* We support indirect buffer descriptors */
            VIRTIO_RING_F_INDIRECT_DESC = 28,

            /* The Guest publishes the used index for which it expects an interrupt
             * at the end of the avail ring. Host should ignore the avail->flags field. */
            /* The Host publishes the avail index for which it expects a kick
             * at the end of the used ring. Guest should ignore the used->flags field. */
            VIRTIO_RING_F_EVENT_IDX = 29,
        };    
      
        vring(virtio_device* const dev, u16 num, u16 q_index);
        virtual ~vring();

        u64 get_paddr(void);
        static unsigned get_size(unsigned int num, unsigned long align);

        // Ring operations
        bool add_buf(sglist* sg, u16 out, u16 in, void* cookie);
        void* get_buf();
        bool used_ring_not_empty();
        bool avail_ring_not_empty();
        // when the available ring has x descriptors as room it means that
        // x descriptors can be allocated while _num-x are available for the host
        bool avail_ring_has_room(int n);
        bool kick();
        // Total number of descriptors in ring
        int size() {return _num;}

        // The following is used with USED_EVENT_IDX and AVAIL_EVENT_IDX
        // Assuming a given event_idx value from the other size, if
        // we have just incremented index from old to new_idx,
        // should we trigger an event?
        static int need_event(u16 event_idx, u16 new_idx, u16 old);

    private:

        // Up pointer
        virtio_device* _dev;
        u16 _q_index;
        // The physical of the physical address handed to the virtio device
        void* _vring_ptr;
        
        // Total number of descriptors in ring
        unsigned int _num;

        // Position of the next available descriptor
        u16 _avail_head;
        // Position of the used descriptor we've last seen
        u16 _used_guest_head;
        // The amount of avail descriptors we've added since last kick
        u16 _avail_added_since_kick;
        u16 _avail_count;

        // Flat list of chained descriptors
        vring_desc *_desc;
        // Available for host consumption
        vring_avail *_avail;
        // Available for guest consumption
        vring_used *_used;
        // cookies to store access to the upper layer pointers
        void** _cookie;
        //protects parallel get_bug /add_buf access, mainly the _avail_head variable
        mutex _lock;
    };


}

#endif // VIRTIO_VRING_H
