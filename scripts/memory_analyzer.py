#!/usr/bin/env python
from collections import Counter
from osv import tree
import re

print("-=[ Mem0ry Allocations Analyz3r ]=-")

def is_malloc(x):
    return x.find("malloc ") > 0

def is_mempool(x):
    return x.find("malloc_mempool ") > 0

def is_large(x):
    return x.find("malloc_large ") > 0

def is_free(x):
    return x.find("memory_free ") > 0

def get_len(x):
    return int(re.findall("len=(\d*)", x)[0])

def get_buf(x):
    return re.findall("buf=(0x................)", x)[0]

def get_align(x):
    return int(re.findall("align=(\d*)", x)[0])

def get_type(x):
    return re.findall("malloc_(\S*) ", x)[0]

def is_page_alloc(x):
    return x.find("memory_page_alloc ") > 0

def is_page_free(x):
    return x.find("memory_page_free ") > 0

def get_page(x):
    return re.findall("page=(0x................)", x)[0]

class Buffer(object):
    def __init__(self, buf, alloc_type, alloc_len):
        self.buf = buf
        self.alloc_type = alloc_type
        self.alloc_len = alloc_len
        self.req_len = 0
        self.align = 0
        self.is_freed = False

    def set_freed(self):
        if self.is_freed:
            print("Buffer %s has already been freed." % self.buf)
        self.is_freed = True

def process_records(mallocs, trace_records):
    for l in trace_records:
        try:
            #
            # Maintain a hash dictionary that maps between buffer address and
            # a list of object, each describing the buffer: its requested and
            # actual size, alignment, allocator and whether it had been freed.
            #
            if is_malloc(l):
                buf = get_buf(l)
                if buf in mallocs and not mallocs[buf][-1].is_freed:
                    mallocs[buf][-1].req_len = get_len(l)
                    mallocs[buf][-1].align = get_align(l)
                else:
                    print("Buffer %s allocated by unknown allocator." % buf)
                    t = Buffer(buf, '<unknown>', 0)
                    if buf in mallocs:
                        mallocs[buf] += [t]
                    else:
                        mallocs[buf] = [t]

            elif is_mempool(l) or is_large(l):
                buf = get_buf(l)
                t = Buffer(buf, get_type(l), get_len(l))
                if buf in mallocs:
                    if not mallocs[buf][-1].is_freed:
                        print("Buffer %s was already allocated." % buf)
                    mallocs[buf] += [t]
                else:
                    mallocs[buf] = [t]

            elif is_free(l):
                buf = get_buf(l)
                # check if buffer had been allocated
                if not buf in mallocs:
                    # print "Buffer %s never been allocated." % buf
                    pass
                else:
                    mallocs[buf][-1].set_freed()

        #
        # TODO: It is possible to alloc_huge_page() and then free_page()
        # it in 512 pieces, and vice versa. Add support for that.
        #
            elif is_page_alloc(l):
                page = get_page(l)
                t = Buffer(page, 'page', 4096)
                t.req_len = 4096
                t.align = 4096
                if page in mallocs:
                    if not mallocs[page][-1].is_freed:
                        print("Page %s was already allocated." % page)
                    mallocs[page] += [t]
                else:
                    mallocs[page] = [t]

            elif is_page_free(l):
                page = get_page(l)
                # check if page had been allocated
                if not page in mallocs:
                    # print "Page %s never been allocated." % page
                    pass
                else:
                    # check if page already freed
                    mallocs[page][-1].set_freed()

        except:
            print("Problem parsing line: '%s'" % l)
            raise

class TreeKey(object):
    def __init__(self, this, desc):
        self.this = this
        self.desc = desc
        self.alloc = 0
        self.unfreed_count = 0
        self.unfreed_bytes = 0
        self.lost_bytes = 0

    def __str__(self):
        if not self.desc == None:
            name = "%s %s" % (self.this, self.desc)
        else:
            name = "%s" % self.this
        name += "\ncount: %d" % self.alloc
        if self.unfreed_count > 0:
            name += "\nunfreed: %d (%d bytes" % (
                self.unfreed_count,
                self.unfreed_bytes)
            if self.lost_bytes > 0:
                name += ", unused: %d %d%%" % (
                    self.lost_bytes,
                    self.lost_bytes * 100 / self.unfreed_bytes)
            name += ")"
        return name

    def __eq__(self, other):
        return self.this == other.this

    def __lt__(self, other):
        return self.this < other.this

    def __hash__(self):
        return self.this.__hash__()


def show_results(mallocs):
    root = tree.TreeNode(TreeKey('All', None))

    lost = 0
    unfreed = 0

    for buf in mallocs:
        for desc in mallocs[buf]:
            alloc_type = root.get_or_add(TreeKey(desc.alloc_type, None))
            align = alloc_type.get_or_add(TreeKey(desc.align, 'alignment'))
            alloc_size = align.get_or_add(TreeKey(desc.alloc_len, 'allocated'))
            req_size = alloc_size.get_or_add(TreeKey(desc.req_len, 'requested'))

            req_size.key.alloc += 1
            if not desc.is_freed:
                req_size.key.unfreed_count += 1
                req_size.key.unfreed_bytes += desc.alloc_len
                req_size.key.lost_bytes += desc.alloc_len - desc.req_len

    def propagate(parent, node):
        for child in node.children:
            propagate(node, child)
        if parent:
            parent.key.alloc += node.key.alloc
            parent.key.unfreed_count += node.key.unfreed_count
            parent.key.unfreed_bytes += node.key.unfreed_bytes
            parent.key.lost_bytes += node.key.lost_bytes

    propagate(None, root)

    def formatter(key):
        return key.key.__str__()

    tree.print_tree(root, formatter)


if __name__ == "__main__":
    mallocs = {}
    trace_records = open("trace.txt", "rt").readlines()
    process_records(mallocs, trace_records)
    show_results(mallocs)
