#include "fermate.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#define BITS (8 * sizeof(unsigned long))

static bool bit(const unsigned long *bits, unsigned code)
{
    return (bits[code / BITS] & (1UL << (code % BITS))) != 0;
}
bool is_event(const char *name)
{
    if (strncmp(name, "event", 5) || !name[5])
        return (false);
    for (const char *p = name + 5; *p; ++p)
        if (!isdigit((unsigned char)*p))
            return (false);
    return (true);
}
int block_device(struct devices *devices, const char *name)
{
    char path[4096];
    if (!is_event(name))
        return (0);
    int n = snprintf(path, sizeof(path), "%s/%s", DIR_TO_WATCH, name);
    if (n < 0 || (size_t)n >= sizeof(path))
        return (-1);
    for (size_t i = 0; i < devices->size; ++i)
        if (!strcmp(devices->items[i].path, path))
            return (0);
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
    {
        if (errno == ENOENT || errno == ENODEV)
            return (0);
        perror(path);
        return (-1);
    }
    if (ioctl(fd, EVIOCGRAB, 1) < 0)
    {
        perror(path);
        close(fd);
        return (-1);
    }
    unsigned long keys[KEY_MAX / BITS + 1] = {0};
    bool keyboard = ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) >= 0 && bit(keys, KEY_A) && bit(keys, KEY_Z) && bit(keys, KEY_ENTER);
    if (devices->size == devices->capacity)
    {
        size_t capacity = devices->capacity ? devices->capacity * 2 : 32;
        struct device *items = realloc(devices->items, capacity * sizeof(*items));
        if (!items)
        {
            close(fd);
            return (-1);
        }
        devices->items = items;
        devices->capacity = capacity;
    }
    char *copy = strdup(path);
    if (!copy)
    {
        close(fd);
        return (-1);
    }
    devices->items[devices->size++] = (struct device){.fd = fd, .path = copy, .keyboard = keyboard};
    return (0);
}
int scan_devices(struct devices *devices)
{
    DIR *dir = opendir(DIR_TO_WATCH);
    if (!dir)
    {
        perror(DIR_TO_WATCH);
        return (-1);
    }
    struct dirent *entry;
    int result = 0;
    while ((entry = readdir(dir)))
    {
        if (block_device(devices, entry->d_name) < 0)
        {
            result = -1;
            break;
        }
    }
    closedir(dir);
    return (result);
}
