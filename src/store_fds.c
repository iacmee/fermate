#include "fermate.h"
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>
void remove_device(struct devices *devices, size_t index)
{
    struct device *device = &devices->items[index];
    ioctl(device->fd, EVIOCGRAB, 0);
    close(device->fd);
    if (device->state) xkb_state_unref(device->state);
    if (device->compose) xkb_compose_state_unref(device->compose);
    free(device->path);
    devices->items[index] = devices->items[--devices->size];
}
void release_devices(struct devices *devices)
{
    while (devices->size) remove_device(devices, devices->size - 1);
    free(devices->items);
    *devices = (struct devices){0};
}
