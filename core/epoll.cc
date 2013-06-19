// Implement the Linux epoll(7) functions in OSV

// NOTE: This is an inefficient implementation, in which epoll_wait() calls
// poll(), thereby negating all the performance benefits of using epoll and
// not poll. This is only a temporary implementation, for getting the
// functionality of epoll which Java needs - but not its performance.

#include <sys/epoll.h>
#include <memory>
#include <errno.h>


#include <osv/file.h>
#include <fs/fs.hh>
#include <fs/unsupported.h>
#include <drivers/clock.hh>

#include <debug.hh>
#include <unordered_map>

class epoll_obj {
    std::unordered_map<int, struct epoll_event> map;
public:
    int add(int fd, struct epoll_event *event)
    {
        if (map.count(fd)) {
            return EEXIST;
        }
        map[fd] = *event;
        return 0;
    }
    int mod(int fd, struct epoll_event *event)
    {
        try {
            map.at(fd) = *event;
            return 0;
        } catch (std::out_of_range &e) {
            return ENOENT;
        }
    }
    int del(int fd)
    {
        if (map.erase(fd)) {
            return 0;
        } else {
            return ENOENT;
        }
    }
    int wait(struct epoll_event *events, int maxevents, int timeout_ms)
    {
        std::unique_ptr<pollfd[]> pollfds(new pollfd[map.size()]);
        int n=0;
        for (auto &i : map) {
            pollfds[n].fd = i.first;
            int eevents = i.second.events;
            assert(!(eevents&(EPOLLET|EPOLLONESHOT)));
            // We (ab)use the fact that POLLIN==EPOLLIN, etc. TODO: static_assert.
            pollfds[n].revents = eevents;
            n++;
        }
        int r = poll(pollfds.get(), n, timeout_ms);
        if (r > 0) {
            r = std::min(r, maxevents);
            int remain = r;
            for (int i = 0; i < n && remain;  i++) {
                if (pollfds[i].revents) {
                    --remain;
                    events[remain].data = map[pollfds[i].fd].data;
                    events[remain].events = pollfds[i].revents;
                }
            }
        }
        if(r) debug("epoll_wait returning %d\n", r);
        return r;
    }
};

static int epoll_fop_init(file* f)
{
    return 0;
}

static int epoll_fop_close(file *f)
{
    delete static_cast<epoll_obj*>(f->f_data);
    f->f_data = nullptr;
    return 0;
}

static fileops epoll_ops = {
    epoll_fop_init,
    unsupported_read,
    unsupported_write,
    unsupported_truncate,
    unsupported_ioctl,
    unsupported_poll,
    unsupported_stat,
    epoll_fop_close,
    unsupported_chmod,
};

static inline bool is_epoll(struct file *f)
{
    return f->f_ops == &epoll_ops;
}


static inline epoll_obj *get_epoll_obj(fileref fr) {
    struct file *f = fr.get();
    if (is_epoll(f)) {
        return static_cast<epoll_obj*> (f->f_data);
    } else {
        return nullptr;
    }
}

int epoll_create(int size)
{
    debug("epoll_create\n");
    // Note we ignore the size parameter. There's no point in checking it's
    // positive, and on the other hand we can't trust it being meaningful
    // because Linux ignores it too.
    std::unique_ptr<epoll_obj> s{new epoll_obj()};
    try {
        fileref f{falloc_noinstall()};
        finit(f.get(), 0 , DTYPE_UNSPEC, s.release(), &epoll_ops);
        fdesc fd(f);
        return fd.release();
    } catch (int error) {
        errno = error;
        return -1;
    }
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
    debug("epoll_ctl %d %d %d\n", epfd, fd, op);
    fileref epfr(fileref_from_fd(epfd));
    if (!epfr) {
        errno = EBADF;
        return -1;
    }

    epoll_obj *epo = get_epoll_obj(epfr);
    if (!epo) {
        errno = EINVAL;
        return -1;
    }

    int error = 0;

    switch (op) {
    case EPOLL_CTL_ADD:
        error = epo->add(fd, event);
        break;
    case EPOLL_CTL_MOD:
        error = epo->mod(fd, event);
        break;
    case EPOLL_CTL_DEL:
        error = epo->del(fd);
        break;
    default:
        error = EINVAL;
    }

    debug("epoll_ctl done with %d\n",error);
    if (error) {
        errno = error;
        return -1;
    } else {
        return 0;
    }
}

int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout_ms)
{
    //debug("epoll_wait timeout=%d\n", timeout_ms);
    fileref epfr(fileref_from_fd(epfd));
    if (!epfr) {
        errno = EBADF;
        return -1;
    }

    epoll_obj *epo = get_epoll_obj(epfr);
    if (!epo || maxevents <= 0) {
        errno = EINVAL;
        return -1;
    }

    return epo->wait(events, maxevents, timeout_ms);
}
