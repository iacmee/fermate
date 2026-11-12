#pragma once

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h>
#include "xkb.h"

extern volatile int g_fd_keyboard;
extern volatile int g_max_block_time;
// colors
#define RES "\001\e[0m\002"
#define BLK "\001\e[1;90m\002"
#define RED "\001\e[1;91m\002"
#define GRN "\001\e[1;92m\002"
#define YEL "\001\e[1;93m\002"
#define BLU "\001\e[1;94m\002"
#define MAG "\001\e[1;95m\002"
#define CYN "\001\e[1;96m\002"
#define WHT "\001\e[1;97m\002"
#define BLD "\001\e[1;m\002"

#define DIR_TO_WATCH "/dev/input"
#define FDS_INITIAL_SIZE 32
#define EMERGENCY_EXIT "exit"
#define DEFAULT_BLOCK_TIME (5 * 60) // 5 minutes
#define USR_BUF_SIZE 1024

#define PERROR(x) printf("%s line: %d function: %s\n" RED "%s : %s\n" RES, __FILE__, __LINE__, __func__, x, strerror(errno))

typedef enum fd_action
{
    A_ADD,
    A_EXIT
} fd_action;

#define CH_ASCII_ALPHA 1
#define CH_ASCII_NUMBER 2
#define CH_ASCII_SYMBOL 4
#define CH_UTF8_ALPHA 8

void block_all_device_and_set_fd_keyboard(void); // set the global variable to the rigth fd
void handle_fds(fd_action action, int new_fd);
void block_device(const char *name);
void *continuing_blocking_device(void *arg);
int32_t launch_thread_for_continuing_blocking();
bool is_event(const char *name);
void get_max_block_time(int ac, char *av[]);






