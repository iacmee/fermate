#include "fermate.h"

void *continuing_blocking_device(void *arg)
{
	char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    const struct inotify_event *event;
	int fd = inotify_init();
	int wd = inotify_add_watch(fd, DIR_TO_WATCH, IN_CREATE | IN_DELETE);

    while (true)
    {
        ssize_t byte_read = read(fd, buf, sizeof(buf));
        if (byte_read == -1)
        {
            PERROR("read");
            exit(EXIT_FAILURE);
        }

        for (char *ptr = buf; ptr < buf + byte_read; ptr += sizeof(struct inotify_event) + event->len)
        {
            event = (struct inotify_event *)ptr;

            if (event->mask & IN_CREATE)
            {
                if (is_event(event->name))
                    printf(RED "New device detected, blocking...\n" RES);
                block_device(event->name);
            }
            if (event->mask & IN_DELETE)
            {
                printf(RED "something was erase : '%s'\n" RES, event->name);
            }
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);
    (void)arg;
}
