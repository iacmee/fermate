#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <linux/input.h>
#define DEFAULT_BLOCK_TIME 300
#define USR_BUF_SIZE 1024
#define EMERGENCY_EXIT "exit"
#ifndef DIR_TO_WATCH
#define DIR_TO_WATCH "/dev/input"
#endif
#define CH_ASCII_ALPHA 1
#define CH_ASCII_NUMBER 2
#define CH_ASCII_SYMBOL 4
#define CH_UTF8_ALPHA 8
struct device
{
    int fd;
    char *path;
    bool keyboard;
    struct xkb_state *state;
    struct xkb_compose_state *compose;
};
struct devices
{
    struct device *items;
    size_t size;
    size_t capacity;
};
bool parse_duration(const char *text, int *seconds);
bool update_user_buffer(char *buffer, size_t *length, size_t capacity, const char *text);
char *generate_random_string(size_t length, int32_t classes);
bool is_event(const char *name);
int block_device(struct devices *devices, const char *name);
void remove_device(struct devices *devices, size_t index);
void release_devices(struct devices *devices);
int scan_devices(struct devices *devices);
int watch_devices(void);
int refresh_devices(int watcher, struct devices *devices);
