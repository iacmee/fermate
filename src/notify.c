#include "fermate.h"
#include <errno.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>
int watch_devices(void)
{
    int fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (fd < 0)
        return (-1);
    if (inotify_add_watch(fd, DIR_TO_WATCH, IN_CREATE | IN_DELETE |
        IN_ATTRIB | IN_MOVED_TO | IN_MOVED_FROM | IN_DELETE_SELF |
        IN_MOVE_SELF) < 0)
    {
        close(fd);
        return (-1);
    }
    return (fd);
}
int refresh_devices(int watcher, struct devices *devices)
{
    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    ssize_t count;
    while ((count = read(watcher, buf, sizeof(buf))) > 0)
    {
        for (char *p = buf; p < buf + count;)
        {
            const struct inotify_event *event = (const struct inotify_event *)p;
            if (event->mask & (IN_IGNORED | IN_DELETE_SELF | IN_MOVE_SELF))
                return -1;
            p += sizeof(*event) + event->len;
        }
    }
    if (count < 0 && errno != EAGAIN && errno != EINTR)
        return -1;
    for (size_t i = 0; i < devices->size;)
    {
        struct stat held, current;
        if (fstat(devices->items[i].fd, &held) < 0 ||
            stat(devices->items[i].path, &current) < 0 ||
            held.st_ino != current.st_ino || held.st_dev != current.st_dev)
            remove_device(devices, i);
        else
            ++i;
    }
    return scan_devices(devices);
}
