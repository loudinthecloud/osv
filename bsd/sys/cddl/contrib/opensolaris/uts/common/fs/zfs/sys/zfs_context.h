/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_ZFS_CONTEXT_H
#define	_SYS_ZFS_CONTEXT_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <bsd/porting/netport.h>
#include <bsd/porting/synch.h>

#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>

#include <sys/param.h>
#include <sys/note.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/sysmacros.h>
#include <sys/bitmap.h>
#include <sys/cmn_err.h>
#include <sys/taskq.h>
#include <sys/taskqueue.h>
#include <sys/systm.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/kcondvar.h>
#include <sys/random.h>
#include <sys/byteorder.h>
#include <sys/systm.h>
#include <sys/list.h>
#include <sys/zfs_debug.h>
#include <sys/sysevent.h>
#include <sys/solaris_uio.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/limits.h>
#include <sys/string.h>
#include <sys/cred.h>
#include <sys/sdt.h>
#include <sys/sysctl.h>
#include <sys/sbuf.h>
#include <sys/priv.h>
#include <sys/refstr.h>
#include <sys/zone.h>
#include <sys/eventhandler.h>
#include <sys/misc.h>
#include <sys/sysevent/dev.h>
#include <sys/sysevent/eventdefs.h>
#include <sys/u8_textprep.h>
#include <sys/fm/util.h>
#include <sys/sunddi.h>
#include <sys/priority.h>

#define	CPU_SEQID	(curcpu)

#define	tsd_create(keyp, destructor)	do {				\
	*(keyp) = osd_thread_register((destructor));			\
	KASSERT(*(keyp) > 0, ("cannot register OSD"));			\
} while (0)
#define	tsd_destroy(keyp)		osd_thread_deregister(*(keyp))
#define	tsd_get(key)			osd_thread_get(curthread, (key))
#define	tsd_set(key, value)		osd_thread_set(curthread, (key), (value))

#ifdef	__cplusplus
}
#endif

extern int zfs_debug_level;
extern struct mtx zfs_debug_mtx;
#define	ZFS_LOG(lvl, ...)	do {					\
	if (((lvl) & 0xff) <= zfs_debug_level) {			\
		mtx_lock(&zfs_debug_mtx);				\
		printf("%s:%u[%d]: ", __func__, __LINE__, (lvl));	\
		printf(__VA_ARGS__);					\
		printf("\n");						\
		if ((lvl) & 0x100)					\
			kdb_backtrace();				\
		mtx_unlock(&zfs_debug_mtx);				\
	}								\
} while (0)

#define	sys_shutdown	rebooting

#endif	/* _SYS_ZFS_CONTEXT_H */
