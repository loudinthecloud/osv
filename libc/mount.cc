#include "libc.hh"
#include <osv/mount.h>
#include <sys/mount.h>
#include <fs/vfs/vfs.h>

int umount(const char *path)
{
    auto r = sys_umount(path);
    if (r == 0) {
        return 0;
    } else {
        return libc_error(r);
    }
}

int umount2(const char *path, int flags)
{
    auto r = sys_umount2(path, flags);
    if (r == 0) {
        return 0;
    } else {
        return libc_error(r);
    }
}

