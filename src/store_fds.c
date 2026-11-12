#include "fermate.h"

static void add(int32_t **fds, int32_t *size, int32_t *capacity, int32_t new_fd);
static void close_all(int32_t *fds, int32_t size);

void handle_fds(fd_action action, int new_fd)
{
    static int32_t size;
    static int32_t capacity = FDS_INITIAL_SIZE;
    static int32_t *fds;

    if (fds == NULL)
    {
        fds = (int32_t *)malloc(sizeof(int) * FDS_INITIAL_SIZE);
        if (fds == NULL)
        {
            PERROR("malloc");
            exit(EXIT_FAILURE);
        }
    }
    if (action == A_ADD)
        add(&fds, &size, &capacity, new_fd);
    if (action == A_EXIT)
        close_all(fds, size);
}

static void add(int32_t **fds, int32_t *size, int32_t *capacity, int32_t new_fd)
{
    if (*size == *capacity)
    {
        *fds = realloc(*fds, *capacity + FDS_INITIAL_SIZE);
        if (*fds == NULL)
        {
            PERROR("malloc");
            exit(EXIT_FAILURE);
        }
    }
    (*fds)[*size] = new_fd;
    *size += 1;
}

static void close_all(int32_t *fds, int32_t size)
{
    for (int32_t i = size; i < size; ++i)
    {
        ioctl(fds[i], EVIOCGRAB, 0); // i didn't check the return cause isn't realy necessery realease the grab, close does it.
        if (close(fds[i]) == -1)
            PERROR("close");
    }
    printf(CYN "All event realesed\n" RES);
}