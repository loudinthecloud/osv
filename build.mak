
arch = x64
cmdline = java.so -jar /java/cli.jar
#cmdline = testrunner.so
#cmdline = java.so Hello
INCLUDES = -I. -I$(src)/arch/$(arch) -I$(src) -I$(src)/external/libunwind/include -I$(src)/include
INCLUDES += -I$(src)/external/acpica/source/include
COMMON = $(autodepend) -g -Wall -Wno-pointer-arith -Werror -Wformat=0 \
	-D __BSD_VISIBLE=1 -U _FORTIFY_SOURCE -fno-stack-protector $(INCLUDES) \
	$(arch-cflags) $(conf-opt) $(acpi-defines) $(tracing-flags) \
	$(configuration)

tracing-flags-0 =
tracing-flags-1 = -finstrument-functions -finstrument-functions-exclude-file-list=c++,trace.cc,trace.hh,align.hh
tracing-flags = $(tracing-flags-$(conf-tracing))

CXXFLAGS = -std=gnu++11 -lstdc++ $(do-sys-includes) $(COMMON)
CFLAGS = -std=gnu99 $(COMMON)

# should be limited to files under libc/ eventually
CFLAGS += -I $(src)/libc/internal -I  $(src)/libc/arch/$(arch) \
	-Wno-missing-braces -Wno-parentheses -Wno-unused-but-set-variable

ASFLAGS = -g $(autodepend)

fs/vfs/main.o: CXXFLAGS += -Wno-sign-compare -Wno-write-strings

configuration-defines = conf-preempt conf-debug_memory

configuration = $(foreach cf,$(configuration-defines), \
                      -D$(cf:conf-%=CONF_%)=$($(cf)))

include $(src)/conf/base.mak
include $(src)/conf/$(mode).mak

arch-cflags = -msse4.1


quiet = $(if $V, $1, @echo " $2"; $1)
very-quiet = $(if $V, $1, @$1)

makedir = $(call very-quiet, mkdir -p $(dir $@))
build-cxx = $(CXX) $(CXXFLAGS) -c -o $@ $<
q-build-cxx = $(call quiet, $(build-cxx), CXX $@)
build-c = $(CC) $(CFLAGS) -c -o $@ $<
q-build-c = $(call quiet, $(build-c), CC $@)
build-s = $(CXX) $(CXXFLAGS) $(ASFLAGS) -c -o $@ $<
q-build-s = $(call quiet, $(build-s), AS $@)
build-so = $(CC) $(CFLAGS) -o $@ $^
q-build-so = $(call quiet, $(build-so), CC $@)
	
%.o: %.cc
	$(makedir)
	$(q-build-cxx)

%.o: %.c
	$(makedir)
	$(q-build-c)

%.o: %.S
	$(makedir)
	$(q-build-s)

%.o: %.s
	$(makedir)
	$(q-build-s)

%.class: %.java
	$(makedir)
	$(call quiet, javac -d $(javabase) -cp $(src)/$(javabase) $^, JAVAC $@)

tests/%.o: COMMON += -fPIC

%.so: COMMON += -fPIC -shared
%.so: %.o
	$(makedir)
	$(q-build-so)

# Some .so's need to refer to libstdc++ so it will be linked at run time.
# The majority of our .so don't actually need libstdc++, so didn't add it
# by default.
tests/tst-queue-mpsc.so: CFLAGS+=-lstdc++
tests/tst-mutex.so: CFLAGS+=-lstdc++

sys-includes = $(jdkbase)/include $(jdkbase)/include/linux
sys-includes +=  $(gccbase)/usr/include $(glibcbase)/usr/include
autodepend = -MD -MT $@ -MP

do-sys-includes = $(foreach inc, $(sys-includes), -isystem $(inc))

tests := tests/tst-pthread.so tests/tst-ramdisk.so tests/hello/Hello.class
tests += tests/tst-vblk.so tests/tst-fat.so tests/tst-romfs.so tests/bench/bench.jar
tests += tests/tst-bsd-evh.so tests/tst-bsd-callout.so tests/tst-bsd-netisr.so \
         tests/tst-bsd-netdriver.so tests/tst-virtionet.so
tests += tests/tst-bsd-kthread.so
tests += tests/tst-bsd-taskqueue.so
tests += tests/tst-fpu.so
tests += tests/tst-preempt.so
tests += tests/tst-tracepoint.so
tests += tests/tst-hub.so
tests += tests/tst-leak.so tests/tst-mmap.so tests/tst-vfs.so
tests += tests/tst-mutex.so
tests += tests/tst-sockets.so
tests += tests/tst-bsd-tcp1.so
tests += tests/tst-ifconfig.so
tests += tests/tst-lsroute.so
tests += tests/tst-condvar.so
tests += tests/tst-queue-mpsc.so
tests += tests/tst-af-local.so

tests/hello/Hello.class: javabase=tests/hello

java/RunJar.class: javabase=java

all: loader.img loader.bin usr.img

boot.bin: arch/x64/boot16.ld arch/x64/boot16.o
	$(call quiet, $(LD) -o $@ -T $^, LD $@)

image-size = $(shell stat --printf %s loader.elf)

loader.img: boot.bin loader.elf
	$(call quiet, dd if=boot.bin of=$@ > /dev/null 2>&1, DD $@ boot.bin)
	$(call very-quiet, cp loader.elf loader-stripped.elf)
	$(call very-quiet, strip loader-stripped.elf)
	$(call quiet, dd if=loader-stripped.elf of=$@ conv=notrunc seek=128 > /dev/null 2>&1, \
		DD $@ loader.elf)
	$(call quiet, $(src)/scripts/imgedit.py setsize $@ $(image-size), IMGEDIT $@)
	$(call quiet, $(src)/scripts/imgedit.py setargs $@ $(cmdline), IMGEDIT $@)

loader.bin: arch/x64/boot32.o arch/x64/loader32.ld
	$(call quiet, $(LD) -nostartfiles -static -nodefaultlibs -o $@ \
	                $(filter-out %.bin, $(^:%.ld=-T %.ld)), LD $@)

arch/x64/boot32.o: loader.elf

bsd/sys/crypto/sha2/sha2.o: CFLAGS+=-Wno-strict-aliasing

bsd  = bsd/net.o  
bsd += bsd/machine/in_cksum.o
bsd += bsd/sys/crypto/sha2/sha2.o
bsd += bsd/sys/libkern/arc4random.o
bsd += bsd/sys/libkern/random.o
bsd += bsd/sys/libkern/inet_ntoa.o
bsd += bsd/sys/libkern/inet_aton.o
bsd += bsd/sys/kern/md5c.o
bsd += bsd/sys/kern/kern_mbuf.o
bsd += bsd/sys/kern/uipc_mbuf.o
bsd += bsd/sys/kern/uipc_mbuf2.o
bsd += bsd/sys/kern/uipc_domain.o
bsd += bsd/sys/kern/uipc_sockbuf.o
bsd += bsd/sys/kern/uipc_socket.o
bsd += bsd/sys/kern/uipc_syscalls.o
bsd += bsd/sys/kern/uipc_syscalls_wrap.o
bsd += bsd/sys/kern/subr_sbuf.o
bsd += bsd/sys/kern/subr_eventhandler.o
bsd += bsd/sys/kern/subr_hash.o
bsd += bsd/sys/kern/subr_taskqueue.o
bsd += bsd/sys/kern/sys_socket.o
bsd += bsd/porting/route.o
bsd += bsd/porting/networking.o
bsd += bsd/porting/netport.o
bsd += bsd/porting/netport1.o
bsd += bsd/porting/cpu.o
bsd += bsd/porting/uma_stub.o
bsd += bsd/porting/sync_stub.o
bsd += bsd/porting/rwlock.o
bsd += bsd/porting/callout.o
bsd += bsd/porting/synch.o
bsd += bsd/porting/kthread.o
bsd += bsd/sys/netinet/if_ether.o  
bsd += bsd/sys/compat/linux/linux_socket.o  
bsd += bsd/sys/compat/linux/linux_ioctl.o  
bsd += bsd/sys/net/if_ethersubr.o  
bsd += bsd/sys/net/if_llatbl.o  
bsd += bsd/sys/net/radix.o  
bsd += bsd/sys/net/route.o  
bsd += bsd/sys/net/raw_cb.o  
bsd += bsd/sys/net/raw_usrreq.o  
bsd += bsd/sys/net/rtsock.o  
bsd += bsd/sys/net/netisr.o  
bsd += bsd/sys/net/netisr1.o  
bsd += bsd/sys/net/if_dead.o  
bsd += bsd/sys/net/if_clone.o  
bsd += bsd/sys/net/if_loop.o  
bsd += bsd/sys/net/if.o  
bsd += bsd/sys/net/pfil.o  
bsd += bsd/sys/netinet/in.o
bsd += bsd/sys/netinet/in_pcb.o
bsd += bsd/sys/netinet/in_proto.o
bsd += bsd/sys/netinet/in_mcast.o
bsd += bsd/sys/netinet/in_rmx.o
bsd += bsd/sys/netinet/ip_id.o
bsd += bsd/sys/netinet/ip_icmp.o
bsd += bsd/sys/netinet/ip_input.o
bsd += bsd/sys/netinet/ip_output.o
bsd += bsd/sys/netinet/ip_options.o
bsd += bsd/sys/netinet/raw_ip.o
bsd += bsd/sys/netinet/igmp.o
bsd += bsd/sys/netinet/udp_usrreq.o
bsd += bsd/sys/netinet/tcp_debug.o
bsd += bsd/sys/netinet/tcp_hostcache.o
bsd += bsd/sys/netinet/tcp_input.o
bsd += bsd/sys/netinet/tcp_lro.o
bsd += bsd/sys/netinet/tcp_offload.o
bsd += bsd/sys/netinet/tcp_output.o
bsd += bsd/sys/netinet/tcp_reass.o
bsd += bsd/sys/netinet/tcp_sack.o
bsd += bsd/sys/netinet/tcp_subr.o
bsd += bsd/sys/netinet/tcp_syncache.o
bsd += bsd/sys/netinet/tcp_timer.o
bsd += bsd/sys/netinet/tcp_timewait.o
bsd += bsd/sys/netinet/tcp_usrreq.o
bsd += bsd/sys/netinet/cc/cc.o
bsd += bsd/sys/netinet/cc/cc_cubic.o
bsd += bsd/sys/netinet/cc/cc_htcp.o
bsd += bsd/sys/netinet/cc/cc_newreno.o
bsd += bsd/sys/xdr/xdr.o
bsd += bsd/sys/xdr/xdr_array.o
bsd += bsd/sys/xdr/xdr_mem.o

solaris :=
solaris += bsd/sys/cddl/compat/opensolaris/kern/opensolaris.o
solaris += bsd/sys/cddl/compat/opensolaris/kern/opensolaris_atomic.o
solaris += bsd/sys/cddl/compat/opensolaris/kern/opensolaris_cmn_err.o
solaris += bsd/sys/cddl/compat/opensolaris/kern/opensolaris_kmem.o
solaris += bsd/sys/cddl/compat/opensolaris/kern/opensolaris_kobj.o
solaris += bsd/sys/cddl/compat/opensolaris/kern/opensolaris_kstat.o
solaris += bsd/sys/cddl/compat/opensolaris/kern/opensolaris_sunddi.o
solaris += bsd/sys/cddl/compat/opensolaris/kern/opensolaris_string.o
solaris += bsd/sys/cddl/compat/opensolaris/kern/opensolaris_taskq.o
solaris += bsd/sys/cddl/compat/opensolaris/kern/opensolaris_sysevent.o
solaris += bsd/sys/cddl/contrib/opensolaris/common/avl/avl.o
solaris += bsd/sys/cddl/contrib/opensolaris/common/nvpair/fnvpair.o
solaris += bsd/sys/cddl/contrib/opensolaris/common/nvpair/nvpair.o
solaris += bsd/sys/cddl/contrib/opensolaris/common/nvpair/nvpair_alloc_fixed.o
solaris += bsd/sys/cddl/contrib/opensolaris/common/unicode/u8_textprep.o
solaris += bsd/sys/cddl/contrib/opensolaris/uts/common/os/callb.o
solaris += bsd/sys/cddl/contrib/opensolaris/uts/common/os/list.o
solaris += bsd/sys/cddl/contrib/opensolaris/uts/common/os/nvpair_alloc_system.o

solaris-tests += tests/tst-solaris-taskq.so

$(solaris) $(solaris-tests): CFLAGS+= \
	-Wno-strict-aliasing \
	-Wno-unknown-pragmas \
	-Wno-unused-variable \
	-Wno-switch \
	-Wno-maybe-uninitialized \
	-D_KERNEL \
	-I$(src)/bsd/sys/cddl/compat/opensolaris \
	-I$(src)/bsd/sys/cddl/contrib/opensolaris/uts/common \
	-I$(src)/bsd/sys

tests += $(solaris-tests)

drivers :=
drivers += drivers/console.o drivers/vga.o drivers/isa-serial.o
drivers += drivers/debug-console.o
drivers += drivers/ramdisk.o
drivers += $(bsd) $(solaris)
drivers += core/mmu.o
drivers += core/elf.o
drivers += core/interrupt.o
drivers += drivers/device.o
drivers += drivers/pci-device.o drivers/pci-function.o drivers/pci-bridge.o 
drivers += drivers/driver.o
drivers += drivers/virtio.o
drivers += drivers/virtio-vring.o
drivers += drivers/virtio-net.o
drivers += drivers/virtio-blk.o
drivers += drivers/clock.o drivers/kvmclock.o
drivers += drivers/clockevent.o
drivers += drivers/acpi.o

objects = bootfs.o
objects += arch/x64/exceptions.o
objects += arch/x64/entry.o
objects += arch/x64/ioapic.o
objects += arch/x64/math.o
objects += arch/x64/apic.o
objects += arch/x64/apic-clock.o
objects += arch/x64/arch-setup.o
objects += arch/x64/smp.o
objects += arch/x64/signal.o
objects += arch/x64/cpuid.o
objects += core/mutex.o
objects += core/condvar.o
objects += core/eventlist.o
objects += core/debug.o
objects += drivers/pci.o
objects += core/mempool.o
objects += core/alloctracker.o
objects += arch/x64/elf-dl.o
objects += linux.o
objects += core/sched.o
objects += core/mmio.o
objects += core/sglist.o
objects += core/kprintf.o
objects += core/trace.o
objects += core/poll.o

include $(src)/fs/build.mak
include $(src)/libc/build.mak

objects += $(addprefix fs/, $(fs))
objects += $(addprefix libc/, $(libc))
objects += $(acpi)

acpi-defines = -DACPI_MACHINE_WIDTH=64 -DACPI_USE_LOCAL_CACHE

acpi-source := $(shell find $(src)/external/acpica/source/components -type f -name '*.c')
acpi = $(patsubst $(src)/%.c, %.o, $(acpi-source))

$(acpi): CFLAGS += -fno-strict-aliasing -Wno-strict-aliasing

libstdc++.a = $(shell find $(gccbase) -name libstdc++.a)
libsupc++.a = $(shell find $(gccbase) -name libsupc++.a)
libgcc_s.a = $(shell find $(gccbase) -name libgcc.a |  grep -v /32/)
libgcc_eh.a = $(shell find $(gccbase) -name libgcc_eh.a |  grep -v /32/)

loader.elf: arch/x64/boot.o arch/x64/loader.ld loader.o runtime.o $(drivers) \
        $(objects) dummy-shlib.so \
		bootfs.bin
	$(call quiet, $(LD) -o $@ \
		-Bdynamic --export-dynamic --eh-frame-hdr --enable-new-dtags \
	    $(filter-out %.bin, $(^:%.ld=-T %.ld)) \
	    $(boost-libs) \
	    $(libstdc++.a) $(libsupc++.a) $(libgcc_s.a) $(libgcc_eh.a) $(src)/libunwind.a, \
		LD $@)

dummy-shlib.so: dummy-shlib.o
	$(call quiet, $(CXX) -nodefaultlibs -shared -o $@ $^, LD $@)

jdkbase := $(shell find $(src)/external/openjdk.bin/usr/lib/jvm \
                         -maxdepth 1 -type d -name 'java*')
glibcbase = $(src)/external/glibc.bin
gccbase = $(src)/external/gcc.bin
miscbase = $(src)/external/misc.bin
boost-libs := $(lastword $(sort $(wildcard /usr/lib*/libboost_program_options-mt.a)))

bsd/%.o: COMMON += -D _KERNEL

usr.img: usr.manifest
	$(call quiet, \
		JDKBASE=$(jdkbase) \
		GCCBASE=$(gccbase) \
		MISCBASE=$(miscbase) \
		BUILDDIR="${@}.tmp" \
		IMAGE="$@" \
		sh $(src)/scripts/mkromfs.sh, MKROMFS $@)

jni = java/jni/balloon.so java/jni/elf-loader.so java/jni/networking.so
$(jni): INCLUDES += -I /usr/lib/jvm/java/include -I /usr/lib/jvm/java/include/linux/

bootfs.bin: scripts/mkbootfs.py bootfs.manifest $(tests) $(jni) \
		tests/testrunner.so java/java.so java/RunJar.class
	$(call quiet, $(src)/scripts/mkbootfs.py -o $@ -d $@.d -m $(src)/bootfs.manifest \
		-D jdkbase=$(jdkbase) -D gccbase=$(gccbase) -D \
		glibcbase=$(glibcbase) -D miscbase=$(miscbase), MKBOOTFS $@)

bootfs.o: bootfs.bin

runtime.o: ctype-data.h

ctype-data.h: gen-ctype-data
	$(call quiet, ./gen-ctype-data > $@, GEN $@)

gen-ctype-data: gen-ctype-data.o
	$(call quiet, $(CXX) -o $@ $^, LD $@)

-include $(shell find -name '*.d')

.DELETE_ON_ERROR:

.SECONDARY:
