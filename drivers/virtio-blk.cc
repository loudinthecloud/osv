
#include <sys/cdefs.h>

#include "drivers/virtio.hh"
#include "drivers/virtio-blk.hh"
#include "drivers/pci-device.hh"
#include "interrupt.hh"

#include "mempool.hh"
#include "mmu.hh"
#include "sglist.hh"

#include <sstream>
#include <string>
#include <string.h>
#include <map>
#include <errno.h>
#include "debug.hh"

#include "sched.hh"
#include "drivers/clock.hh"
#include "drivers/clockevent.hh"

#include <osv/device.h>
#include <osv/bio.h>

using namespace memory;
using sched::thread;


namespace virtio {

int virtio_blk::_instance = 0;


struct virtio_blk_priv {
    virtio_blk* drv;
};

static void
virtio_blk_strategy(struct bio *bio)
{
    struct virtio_blk_priv *prv = reinterpret_cast<struct virtio_blk_priv*>(bio->bio_dev->private_data);

    prv->drv->make_virtio_request(bio);
}

static int
virtio_blk_read(struct device *dev, struct uio *uio, int ioflags)
{
    struct virtio_blk_priv *prv =
        reinterpret_cast<struct virtio_blk_priv*>(dev->private_data);

    if (uio->uio_offset + uio->uio_resid > prv->drv->size())
        return EIO;

    return bdev_read(dev, uio, ioflags);
}

static int
virtio_blk_write(struct device *dev, struct uio *uio, int ioflags)
{
    struct virtio_blk_priv *prv =
        reinterpret_cast<struct virtio_blk_priv*>(dev->private_data);

    if (uio->uio_offset + uio->uio_resid > prv->drv->size())
        return EIO;

    return bdev_write(dev, uio, ioflags);
}

static struct devops virtio_blk_devops = {
    .open       = no_open,
    .close      = no_close,
    .read       = virtio_blk_read,
    .write      = virtio_blk_write,
    .ioctl      = no_ioctl,
    .devctl     = no_devctl,
    .strategy   = virtio_blk_strategy,
};

struct driver virtio_blk_driver = {
    .name       = "virtio_blk",
    .devops     = &virtio_blk_devops,
    .devsz      = sizeof(struct virtio_blk_priv),
};

    virtio_blk::virtio_blk(unsigned dev_idx)
        : virtio_driver(VIRTIO_BLK_DEVICE_ID, dev_idx)
    {
        std::stringstream ss;
        ss << "virtio-blk" << dev_idx;

        _driver_name = ss.str();
        virtio_i(fmt("VIRTIO BLK INSTANCE %d") % dev_idx);
        _id = _instance++;
    }

    virtio_blk::~virtio_blk()
    {
        //TODO: In theory maintain the list of free instances and gc it
        // including the thread objects and their stack
    }

    bool virtio_blk::load(void)
    {
        virtio_driver::load();
        
        read_config();

        _dev->add_dev_status(VIRTIO_CONFIG_S_DRIVER_OK);

        _dev->register_callback([this] { this->response_worker();});


        struct virtio_blk_priv* prv;
        struct device *dev;
        std::string dev_name("vblk");
        dev_name += std::to_string(_id);

        dev = device_create(&virtio_blk_driver, dev_name.c_str(), D_BLK);
        prv = reinterpret_cast<struct virtio_blk_priv*>(dev->private_data);
        prv->drv = this;

        // Perform test if this isn't the boot image (test is destructive
        if (_id > 0) {
            virtio_d(fmt("virtio blk: testing instance %d") % _id);

            for (int i=0;i<6;i++) {
                virtio_d(fmt("Running test %d") % i);
                test();
                timespec ts = {};
                ts.tv_sec = 1;
                nanosleep(&ts, nullptr);

                sched::thread::current()->yield();
            }
        }

        return true;
    }

    bool virtio_blk::unload(void)
    {
        return (true);
    }

    bool virtio_blk::read_config()
    {
        //read all of the block config (including size, mce, topology,..) in one shot
        _dev->virtio_conf_read(_dev->virtio_pci_config_offset(), &_config, sizeof(_config));

        virtio_i(fmt("The capacity of the device is %d") % (u64)_config.capacity);
        if (_dev->get_guest_feature_bit(VIRTIO_BLK_F_SIZE_MAX))
            virtio_i(fmt("The size_max of the device is %d") % (u32)_config.size_max);
        if (_dev->get_guest_feature_bit(VIRTIO_BLK_F_SEG_MAX))
            virtio_i(fmt("The seg_size of the device is %d") % (u32)_config.seg_max);
        if (_dev->get_guest_feature_bit(VIRTIO_BLK_F_GEOMETRY)) {
            virtio_i(fmt("The cylinders count of the device is %d") % (u16)_config.geometry.cylinders);
            virtio_i(fmt("The heads count of the device is %d") % (u32)_config.geometry.heads);
            virtio_i(fmt("The sector count of the device is %d") % (u32)_config.geometry.sectors);
        }
        if (_dev->get_guest_feature_bit(VIRTIO_BLK_F_BLK_SIZE))
            virtio_i(fmt("The block size of the device is %d") % (u32)_config.blk_size);
        if (_dev->get_guest_feature_bit(VIRTIO_BLK_F_TOPOLOGY)) {
            virtio_i(fmt("The physical_block_exp of the device is %d") % (u32)_config.physical_block_exp);
            virtio_i(fmt("The alignment_offset of the device is %d") % (u32)_config.alignment_offset);
            virtio_i(fmt("The min_io_size of the device is %d") % (u16)_config.min_io_size);
            virtio_i(fmt("The opt_io_size of the device is %d") % (u32)_config.opt_io_size);
        }
        if (_dev->get_guest_feature_bit(VIRTIO_BLK_F_CONFIG_WCE))
            virtio_i(fmt("The write cache enable of the device is %d") % (u32)_config.wce);

        return true;
    }

    //temporal hack for the local version of virtio tests
    extern "C" {
    static void blk_bio_done(struct bio*bio) {
        free(bio->bio_data);
        destroy_bio(bio);
    };
    }

    // to be removed soon once we move the test from here to the vfs layer
    virtio_blk::virtio_blk_req* virtio_blk::make_virtio_req(u64 sector, virtio_blk_request_type type, int val)
    {
        sglist* sg = new sglist();
        void* buf = malloc(page_size);
        memset(buf, val, page_size);
        sg->add(mmu::virt_to_phys(buf), page_size);

        struct bio* bio = alloc_bio();
        if (!bio) {
            virtio_e(fmt("bio_alloc failed"));
            return nullptr;
        }
        bio->bio_data = buf;
        bio->bio_done = blk_bio_done;

        virtio_blk_outhdr* hdr = new virtio_blk_outhdr;
        hdr->type = type;
        hdr->ioprio = 0;
        hdr->sector = sector;

        //push 'output' buffers to the beginning of the sg list
        sg->add(mmu::virt_to_phys(hdr), sizeof(struct virtio_blk_outhdr), true);

        virtio_blk_res* res = new virtio_blk_res;
        res->status = 0;
        sg->add(mmu::virt_to_phys(res), sizeof (struct virtio_blk_res));

        virtio_blk_req* req = new virtio_blk_req(hdr, sg, res, bio);
        return req;
    }

    void virtio_blk::test() {
        int i;
        static bool is_write = true; // keep changing the type every call

        virtio_d(fmt("test virtio blk"));
        vring* queue = _dev->get_virt_queue(0);
        virtio_blk_req* req;
        const int iterations = 100;

        if (is_write) {
            is_write = false;
            virtio_d(fmt(" write several requests"));
            for (i=0;i<iterations;i++) {
                req = make_virtio_req(i*8, VIRTIO_BLK_T_OUT,i);
                if (!queue->add_buf(req->payload,2,1,req)) {
                    break;
                }
            }

            virtio_d(fmt(" Let the host know about the %d requests") % i);
            queue->kick();
        } else {
            is_write = true;
            virtio_d(fmt(" read several requests"));
            for (i=0;i<iterations;i++) {
                req = make_virtio_req(i*8, VIRTIO_BLK_T_IN,0);
                if (!queue->add_buf(req->payload,1,2,req)) {
                    break;
                }
                if (i%2) queue->kick(); // should be out of the loop but I like plenty of irqs for the test

            }

            virtio_d(fmt(" Let the host know about the %d requests") % i);
            queue->kick();
        }

        //sched::thread::current()->yield();

        virtio_d(fmt("test virtio blk end"));
    }

    void virtio_blk::response_worker() {
        vring* queue = _dev->get_virt_queue(0);
        virtio_blk_req* req;

        while (1) {

            thread::wait_until([this] {
                vring* queue = this->_dev->get_virt_queue(0);
                return queue->used_ring_not_empty();
            });

            virtio_d(fmt("\t ----> IRQ: virtio_d - blk thread awaken"));

            int i = 0;

            while((req = reinterpret_cast<virtio_blk_req*>(queue->get_buf())) != nullptr) {
                virtio_d(fmt("\t got response:%d = %d ") % i++ % (int)req->status->status);

                virtio_blk_outhdr* header = reinterpret_cast<virtio_blk_outhdr*>(req->req_header);
                //  This is debug code to verify the read content, to be remove later on
                if (header->type == VIRTIO_BLK_T_IN) {
                    virtio_d(fmt("\t verify that sector %d contains data %d") % (int)header->sector % (int)(header->sector/8));
                    auto ii = req->payload->_nodes.begin();
                    ii++;
                    char*buf = reinterpret_cast<char*>(mmu::phys_to_virt(ii->_paddr));
                    virtio_d(fmt("\t value = %d len=%d") % (int)(*buf) % ii->_len);

                }
                if (req->bio != nullptr) {
                    biodone(req->bio);
                    req->bio = nullptr;
                }

                delete req;
            }

        }

    }

    virtio_blk::virtio_blk_req::~virtio_blk_req()
    {
        if (req_header) delete reinterpret_cast<virtio_blk_outhdr*>(req_header);
        if (payload) delete payload;
        if (status) delete status;
        if (bio) delete bio;
    }


    //todo: get it from the host
    int virtio_blk::size() {
        return 1024 * 1024 * 1024;
    }

    static const int page_size = 4096;
    static const int sector_size = 512;

    int virtio_blk::make_virtio_request(struct bio* bio)
    {
        if (!bio) return EIO;

        int in = 1, out = 1, *buf_count;
        virtio_blk_request_type type;

        switch (bio->bio_cmd) {
        case BIO_READ:
            type = VIRTIO_BLK_T_IN;
            buf_count = &in;
            break;
        case BIO_WRITE:
            type = VIRTIO_BLK_T_OUT;
            buf_count = &out;
            break;
        default:
            return ENOTBLK;
        }

        sglist* sg = new sglist();

        // need to break a contiguous buffers that > 4k into several physical page mapping
        // even if the virtual space is contiguous.
        int len = 0;
        int offset = bio->bio_offset;
        //todo fix hack that works around the zero offset issue
        offset = 0xfff & reinterpret_cast<long>(bio->bio_data);
        void *base = bio->bio_data;
        while (len != bio->bio_bcount) {
            int size = std::min((int)bio->bio_bcount - len, page_size);
            if (offset + size > page_size)
                size = page_size - offset;
            len += size;
            sg->add(mmu::virt_to_phys(base), size);
            base += size;
            offset = 0;
            (*buf_count)++;
        }

        virtio_blk_outhdr* hdr = new virtio_blk_outhdr;
        hdr->type = type;
        hdr->ioprio = 0;
        // TODO - fix offset source
        hdr->sector = (int)bio->bio_offset/ sector_size; //wait, isn't offset starts on page addr??

        //push 'output' buffers to the beginning of the sg list
        sg->add(mmu::virt_to_phys(hdr), sizeof(struct virtio_blk_outhdr), true);

        virtio_blk_res* res = new virtio_blk_res;
        res->status = 0;
        sg->add(mmu::virt_to_phys(res), sizeof (struct virtio_blk_res));

        virtio_blk_req* req = new virtio_blk_req(hdr, sg, res, bio);
        vring* queue = _dev->get_virt_queue(0);

        if (!queue->add_buf(req->payload,out,in,req)) {
            // TODO need to clea the bio
            delete req;
            return EBUSY;
        }

        queue->kick(); // should be out of the loop but I like plenty of irqs for the test

        return 0;
    }

    u32 virtio_blk::get_driver_features(void)
    {
        u32 base = virtio_driver::get_driver_features();
        return (base | ( 1 << VIRTIO_BLK_F_SIZE_MAX)
                     | ( 1 << VIRTIO_BLK_F_SEG_MAX)
                     | ( 1 << VIRTIO_BLK_F_GEOMETRY)
                     | ( 1 << VIRTIO_BLK_F_RO)
                     | ( 1 << VIRTIO_BLK_F_BLK_SIZE));
    }

}
