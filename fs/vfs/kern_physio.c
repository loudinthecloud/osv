
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <osv/device.h>
#include <osv/bio.h>

struct bio *
alloc_bio(void)
{
	struct bio *bio = malloc(sizeof(*bio));
	if (!bio)
		return NULL;
	memset(bio, 0, sizeof(*bio));

	list_init(&bio->bio_list);
	pthread_mutex_init(&bio->bio_mutex, NULL);
	pthread_cond_init(&bio->bio_wait, NULL);
	return bio;
}

void
destroy_bio(struct bio *bio)
{
	pthread_cond_destroy(&bio->bio_wait);
//	pthread_mutex_destroy(&bio->bio_mutex);
	free(bio);
}

void
biodone(struct bio *bio)
{
	void (*bio_done)(struct bio *);

	pthread_mutex_lock(&bio->bio_mutex);
	bio->bio_flags |= BIO_DONE;
	bio_done = bio->bio_done;
	if (!bio_done) {
		pthread_cond_signal(&bio->bio_wait);
		pthread_mutex_unlock(&bio->bio_mutex);
	} else {
		pthread_mutex_unlock(&bio->bio_mutex);
		bio_done(bio);
	}
}

int
physio(struct device *dev, struct uio *uio, int ioflags)
{
	struct bio *bio;

	if (uio->uio_offset < 0)
		return EINVAL;
	if (uio->uio_resid == 0)
		return 0;
    
	while (uio->uio_resid > 0) {
		struct iovec *iov = uio->uio_iov;

		if (!iov->iov_len)
			continue;

		bio = alloc_bio();
		if (!bio)
			return ENOMEM;

		if (uio->uio_rw == UIO_READ)
			bio->bio_cmd = BIO_READ;
		else
			bio->bio_cmd = BIO_WRITE;

		bio->bio_dev = dev;
		bio->bio_data = iov->iov_base;
		bio->bio_offset = uio->uio_offset;
		bio->bio_bcount = uio->uio_resid;

		dev->driver->devops->strategy(bio);

		pthread_mutex_lock(&bio->bio_mutex);
		while (!(bio->bio_flags & BIO_DONE))
			pthread_cond_wait(&bio->bio_wait, &bio->bio_mutex);
		pthread_mutex_unlock(&bio->bio_mutex);

		destroy_bio(bio);

	        uio->uio_iov++;
        	uio->uio_iovcnt--;
        	uio->uio_resid -= iov->iov_len;
        	uio->uio_offset += iov->iov_len;
	}

	return 0;
}

