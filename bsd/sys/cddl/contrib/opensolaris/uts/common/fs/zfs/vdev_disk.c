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
 * Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2012 by Delphix. All rights reserved.
 * Copyright (c) 2013, Cloudius Systems . All rights reserved.
 */


#include <sys/types.h>
#include <osv/bio.h>
#include <osv/device.h>

#include <sys/zfs_context.h>
#include <sys/spa_impl.h>
#include <sys/vdev_impl.h>
#include <sys/fs/zfs.h>
#include <sys/zio.h>


struct vdev_disk {
	struct device	*device;
};

static void
vdev_disk_hold(vdev_t *vd)
{
}

static void
vdev_disk_rele(vdev_t *vd)
{
}

static int
vdev_disk_open(vdev_t *vd, uint64_t *psize, uint64_t *max_psize,
    uint64_t *ashift)
{
	struct vdev_disk *dvd;
	int error;

	/*
	 * We must have a pathname, and it must be absolute.
	 */
	if (vd->vdev_path == NULL || vd->vdev_path[0] != '/') {
		vd->vdev_stat.vs_aux = VDEV_AUX_BAD_LABEL;
		return (EINVAL);
	}

	/*
	 * Open the device if it's not currently open, otherwise just update
	 * the physical size of the device.
	 */
	if (vd->vdev_tsd == NULL) {
		dvd = vd->vdev_tsd = kmem_zalloc(sizeof(struct vdev_disk), KM_SLEEP);

		error = device_open(vd->vdev_path + 5, DO_RDWR, &dvd->device);
		if (error) {
			vd->vdev_stat.vs_aux = VDEV_AUX_OPEN_FAILED;
			return error;
		}
	} else {
		ASSERT(vd->vdev_reopening);
		dvd = vd->vdev_tsd;
	}

	/*
	 * Determine the actual size of the device.
	 */
	*max_psize = *psize = dvd->device->size;
	*ashift = highbit(MAX(DEV_BSIZE, SPA_MINBLOCKSIZE)) - 1;
	return 0;
}

static void
vdev_disk_close(vdev_t *vd)
{
	struct vdev_disk *dvd = vd->vdev_tsd;

	if (vd->vdev_reopening || dvd == NULL)
		return;

	if (dvd->device)
		device_close(dvd->device);

	vd->vdev_delayed_close = B_FALSE;
	kmem_free(dvd, sizeof(struct vdev_disk));
	vd->vdev_tsd = NULL;
}

static int
vdev_disk_start_bio(zio_t *zio)
{
	vdev_t *vd = zio->io_vd;
	struct vdev_disk *dvd = vd->vdev_tsd;
	struct bio *bio;
	int ret;

	bio = alloc_bio();
	if (zio->io_type == ZIO_TYPE_READ)
		bio->bio_cmd = BIO_READ;
	else
		bio->bio_cmd = BIO_WRITE;

	bio->bio_dev = dvd->device;
	bio->bio_data = zio->io_data;
	bio->bio_offset = zio->io_offset;
	bio->bio_bcount = zio->io_size;

	bio->bio_dev->driver->devops->strategy(bio);

	ret = bio_wait(bio);
	destroy_bio(bio);

	if (ret) {
		zio->io_error = ret;
		return ZIO_PIPELINE_CONTINUE;
	}

	zio_interrupt(zio);
	return ZIO_PIPELINE_STOP;
}

static int
vdev_disk_start_ioctl(zio_t *zio)
{
	vdev_t *vd = zio->io_vd;

	switch (zio->io_cmd) {
	case DKIOCFLUSHWRITECACHE:
		if (zfs_nocacheflush)
			break;
		if (vd->vdev_nowritecache) {
			zio->io_error = ENOTSUP;
			break;
		}

		kprintf("DKIOCFLUSHWRITECACHE used\n");
		abort();
		break;

	default:
		zio->io_error = ENOTSUP;
		break;
	}

	return ZIO_PIPELINE_CONTINUE;
}

static int
vdev_disk_io_start(zio_t *zio)
{
	if (zio->io_type == ZIO_TYPE_IOCTL)
		return vdev_disk_start_ioctl(zio);
	else
		return vdev_disk_start_bio(zio);
}

static void
vdev_disk_io_done(zio_t *zio)
{
}

vdev_ops_t vdev_disk_ops = {
	vdev_disk_open,
	vdev_disk_close,
	vdev_default_asize,
	vdev_disk_io_start,
	vdev_disk_io_done,
	NULL,
	vdev_disk_hold,
	vdev_disk_rele,
	VDEV_TYPE_DISK,		/* name of this vdev type */
	B_TRUE			/* leaf vdev */
};
