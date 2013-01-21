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

#ifndef DRIVER_H
#define DRIVER_H

#include "arch/x64/processor.hh"
#include "drivers/pci.hh"
#include "drivers/pci-device.hh"
#include <ostream>
#include <unordered_map>

using namespace processor;
using namespace pci;

class Driver {
public:
    Driver(u16 vid, u16 id) : _id(id), _vid(vid), _dev(nullptr) {}
    virtual ~Driver() {}
    virtual bool Init(pci_device *d);

    virtual bool earlyInitChecks(void);
    virtual void dump_config(void);

    friend std::ostream& operator <<(std::ostream& out, const Driver &d);
    struct equal {
      bool operator()(const Driver* d1, const Driver* d2) const
      {
        return (d1->_id == d2->_id && d1->_vid == d2->_vid);
      }
    };

    struct hash : std::unary_function< const Driver*, std::size_t> {
        std::size_t operator() ( const Driver* const key ) const {
           return (size_t)(key->_id + key->_vid);
        }
    };

    class InitException {
        const char* what() const { return "uninitialized driver"; }
    };

protected:

    u16 _id;
    u16 _vid;

    pci_device *_dev;
};

#endif
