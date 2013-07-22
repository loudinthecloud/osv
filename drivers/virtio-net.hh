#ifndef VIRTIO_NET_DRIVER_H
#define VIRTIO_NET_DRIVER_H

/* This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of IBM nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL IBM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE. */

#include <bsd/porting/netport.h>
#include <bsd/sys/net/if_var.h>
#include <bsd/sys/net/if.h>
#define _KERNEL
#include <bsd/sys/sys/mbuf.h>

#include "drivers/virtio.hh"
#include "drivers/pci-device.hh"

namespace virtio {

    class virtio_net : public virtio_driver {
    public:

        // The feature bitmap for virtio net
        enum NetFeatures {
            VIRTIO_NET_F_CSUM = 0,       /* Host handles pkts w/ partial csum */
            VIRTIO_NET_F_GUEST_CSUM = 1,       /* Guest handles pkts w/ partial csum */
            VIRTIO_NET_F_MAC = 5,       /* Host has given MAC address. */
            VIRTIO_NET_F_GSO = 6,       /* Host handles pkts w/ any GSO type */
            VIRTIO_NET_F_GUEST_TSO4 = 7,       /* Guest can handle TSOv4 in. */
            VIRTIO_NET_F_GUEST_TSO6 = 8,       /* Guest can handle TSOv6 in. */
            VIRTIO_NET_F_GUEST_ECN = 9,       /* Guest can handle TSO[6] w/ ECN in. */
            VIRTIO_NET_F_GUEST_UFO = 10,      /* Guest can handle UFO in. */
            VIRTIO_NET_F_HOST_TSO4 = 11,      /* Host can handle TSOv4 in. */
            VIRTIO_NET_F_HOST_TSO6 = 12,      /* Host can handle TSOv6 in. */
            VIRTIO_NET_F_HOST_ECN = 13,      /* Host can handle TSO[6] w/ ECN in. */
            VIRTIO_NET_F_HOST_UFO = 14,      /* Host can handle UFO in. */
            VIRTIO_NET_F_MRG_RXBUF = 15,      /* Host can merge receive buffers. */
            VIRTIO_NET_F_STATUS = 16,      /* virtio_net_config.status available */
            VIRTIO_NET_F_CTRL_VQ  = 17,      /* Control channel available */
            VIRTIO_NET_F_CTRL_RX = 18,      /* Control channel RX mode support */
            VIRTIO_NET_F_CTRL_VLAN = 19,      /* Control channel VLAN filtering */
            VIRTIO_NET_F_CTRL_RX_EXTRA = 20,   /* Extra RX mode control support */
            VIRTIO_NET_F_GUEST_ANNOUNCE = 21,  /* Guest can announce device on the network */
            VIRTIO_NET_F_MQ = 22,      /* Device supports Receive Flow Steering */
            VIRTIO_NET_F_CTRL_MAC_ADDR = 23,   /* Set MAC address */
        };

        enum {
            VIRTIO_NET_DEVICE_ID=0x1000,
            VIRTIO_NET_S_LINK_UP = 1,       /* Link is up */
            VIRTIO_NET_S_ANNOUNCE = 2,       /* Announcement is needed */
            VIRTIO_NET_OK = 0,
            VIRTIO_NET_ERR = 1,
            /*
             * Control the RX mode, ie. promisucous, allmulti, etc...
             * All commands require an "out" sg entry containing a 1 byte
             * state value, zero = disable, non-zero = enable.  Commands
             * 0 and 1 are supported with the VIRTIO_NET_F_CTRL_RX feature.
             * Commands 2-5 are added with VIRTIO_NET_F_CTRL_RX_EXTRA.
             */
            VIRTIO_NET_CTRL_RX = 0,
            VIRTIO_NET_CTRL_RX_PROMISC = 0,
            VIRTIO_NET_CTRL_RX_ALLMULTI = 1,
            VIRTIO_NET_CTRL_RX_ALLUNI = 2,
            VIRTIO_NET_CTRL_RX_NOMULTI = 3,
            VIRTIO_NET_CTRL_RX_NOUNI = 4,
            VIRTIO_NET_CTRL_RX_NOBCAST = 5,

            VIRTIO_NET_CTRL_MAC = 1,
            VIRTIO_NET_CTRL_MAC_TABLE_SET = 0,
            VIRTIO_NET_CTRL_MAC_ADDR_SET = 1,

            /*
             * Control VLAN filtering
             *
             * The VLAN filter table is controlled via a simple ADD/DEL interface.
             * VLAN IDs not added may be filterd by the hypervisor.  Del is the
             * opposite of add.  Both commands expect an out entry containing a 2
             * byte VLAN ID.  VLAN filterting is available with the
             * VIRTIO_NET_F_CTRL_VLAN feature bit.
             */
            VIRTIO_NET_CTRL_VLAN = 2,
            VIRTIO_NET_CTRL_VLAN_ADD = 0,
            VIRTIO_NET_CTRL_VLAN_DEL = 1,

            /*
             * Control link announce acknowledgement
             *
             * The command VIRTIO_NET_CTRL_ANNOUNCE_ACK is used to indicate that
             * driver has recevied the notification; device would clear the
             * VIRTIO_NET_S_ANNOUNCE bit in the status field after it receives
             * this command.
             */
            VIRTIO_NET_CTRL_ANNOUNCE = 3,
            VIRTIO_NET_CTRL_ANNOUNCE_ACK = 0,

            VIRTIO_NET_CTRL_MQ = 4,
            VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET = 0,
            VIRTIO_NET_CTRL_MQ_VQ_PAIRS_MIN = 1,
            VIRTIO_NET_CTRL_MQ_VQ_PAIRS_MAX = 0x8000,

            ETH_ALEN = 14,
        };

        struct virtio_net_config {
            /* The config defining mac address (if VIRTIO_NET_F_MAC) */
            u8 mac[6];
            /* See VIRTIO_NET_F_STATUS and VIRTIO_NET_S_* above */
            u16 status;
            /* Maximum number of each of transmit and receive queues;
             * see VIRTIO_NET_F_MQ and VIRTIO_NET_CTRL_MQ.
             * Legal values are between 1 and 0x8000
             */
            u16 max_virtqueue_pairs;
        } __attribute__((packed));

        /* This is the first element of the scatter-gather list.  If you don't
         * specify GSO or CSUM features, you can simply ignore the header. */
        struct virtio_net_hdr {
            enum {
                VIRTIO_NET_HDR_F_NEEDS_CSUM  = 1,       // Use csum_start, csum_offset
                VIRTIO_NET_HDR_F_DATA_VALID = 2,       // Csum is valid
            };
            u8 flags;
            enum {
                VIRTIO_NET_HDR_GSO_NONE = 0,       // Not a GSO frame
                VIRTIO_NET_HDR_GSO_TCPV4 = 1,       // GSO frame, IPv4 TCP (TSO)
                VIRTIO_NET_HDR_GSO_UDP = 3,       // GSO frame, IPv4 UDP (UFO)
                VIRTIO_NET_HDR_GSO_TCPV6 = 4,       // GSO frame, IPv6 TCP
                VIRTIO_NET_HDR_GSO_ECN = 0x80,    // TCP has ECN set
            };
            u8 gso_type;
            u16 hdr_len;          /* Ethernet + IP + tcp/udp hdrs */
            u16 gso_size;         /* Bytes to append to hdr_len per frame */
            u16 csum_start;       /* Position to start checksumming from */
            u16 csum_offset;      /* Offset after that to place checksum */
        };

        /* This is the version of the header to use when the MRG_RXBUF
         * feature has been negotiated. */
        struct virtio_net_hdr_mrg_rxbuf {
            struct virtio_net_hdr hdr;
            u16 num_buffers;      /* Number of merged rx buffers */
        };

        /*
         * Control virtqueue data structures
         *
         * The control virtqueue expects a header in the first sg entry
         * and an ack/status response in the last entry.  Data for the
         * command goes in between.
         */
        struct virtio_net_ctrl_hdr {
                u8 class_t;
                u8 cmd;
        } __attribute__((packed));

        typedef u8 virtio_net_ctrl_ack;

        /*
         * Control the MAC
         *
         * The MAC filter table is managed by the hypervisor, the guest should
         * assume the size is infinite.  Filtering should be considered
         * non-perfect, ie. based on hypervisor resources, the guest may
         * received packets from sources not specified in the filter list.
         *
         * In addition to the class/cmd header, the TABLE_SET command requires
         * two out scatterlists.  Each contains a 4 byte count of entries followed
         * by a concatenated byte stream of the ETH_ALEN MAC addresses.  The
         * first sg list contains unicast addresses, the second is for multicast.
         * This functionality is present if the VIRTIO_NET_F_CTRL_RX feature
         * is available.
         *
         * The ADDR_SET command requests one out scatterlist, it contains a
         * 6 bytes MAC address. This functionality is present if the
         * VIRTIO_NET_F_CTRL_MAC_ADDR feature is available.
         */
        struct virtio_net_ctrl_mac {
                u32 entries;
                u8 macs[][ETH_ALEN];
        } __attribute__((packed));

        /*
         * Control Receive Flow Steering
         *
         * The command VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET
         * enables Receive Flow Steering, specifying the number of the transmit and
         * receive queues that will be used. After the command is consumed and acked by
         * the device, the device will not steer new packets on receive virtqueues
         * other than specified nor read from transmit virtqueues other than specified.
         * Accordingly, driver should not transmit new packets  on virtqueues other than
         * specified.
         */
        struct virtio_net_ctrl_mq {
                u16 virtqueue_pairs;
        };

        explicit virtio_net(pci::device& dev);
        virtual ~virtio_net();

        virtual const std::string get_name(void) { return _driver_name; }
        bool read_config();

        virtual u32 get_driver_features(void);

        void wait_for_queue(vring* queue);
        void receiver();
        void fill_rx_ring();
        bool tx(struct mbuf* m_head, bool flush = false);
        void kick(int queue) {_queues[queue]->kick();}
        void tx_gc_thread();
        void tx_gc();
        static hw_driver* probe(hw_device* dev);

    private:

        struct virtio_net_req {
            struct virtio_net::virtio_net_hdr_mrg_rxbuf mhdr;
            struct free_deleter {
                void operator()(struct mbuf *m) {m_freem(m);}
            };

            std::unique_ptr<struct mbuf, free_deleter> um;

            virtio_net_req() {memset(&mhdr,0,sizeof(mhdr));};
        };

        std::string _driver_name;
        virtio_net_config _config;
        bool _mergeable_bufs;
        u32 _hdr_size;

        vring* _rx_queue;
        vring* _tx_queue;

        //maintains the virtio instance number for multiple drives
        static int _instance;
        int _id;
        struct ifnet* _ifn;

        // tx gc lock that can be called by the gc thread or the tx xmitter
        mutex _tx_gc_lock;
    };
}

#endif

