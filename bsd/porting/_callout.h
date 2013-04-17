/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)callout.h	8.2 (Berkeley) 1/21/94
 * $FreeBSD$
 */

#ifndef _SYS__CALLOUT_H
#define	_SYS__CALLOUT_H

#define CALLOUT_LOCK(c)			(mutex_lock(&(c)->c_callout_mtx))
#define CALLOUT_UNLOCK(c)		(mutex_unlock(&(c)->c_callout_mtx))

#include <osv/mutex.h>

struct callout {
	/* OSv thread */
	void *thread;
	/* OSv waiter thread for drain (drain) */
	void *waiter_thread;
	/* State of this entry */
	int c_flags;
	/* OSv: Mark to stop waiting */
	volatile int c_stopped;
	/* OSv: Mark to reschedule currently running callout */
	volatile int c_reschedule;
	/* Time when callout will be dispatched, both in ticks and in ns */
	uint64_t c_time;
	uint64_t c_to_ns;
	/* MP lock to callout data */
	mutex_t c_callout_mtx;
	/* Callout Handler */
	void (*c_fn)(void *);
	void* c_arg;
	/* Mutex */
	struct mtx* c_mtx;
	/* Rwlock */
	struct rwlock *c_rwlock;
};

#endif
