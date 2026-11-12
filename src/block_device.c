#include "fermate.h"

static bool is_keyboard(int fd);

void block_all_device_and_set_fd_keyboard(void)
{
    DIR *dir = opendir(DIR_TO_WATCH);
    struct dirent *file;

    if (!dir) {
        PERROR("opendir");
        exit(EXIT_FAILURE);
    }

    while ((file = readdir(dir)) != NULL)
    {
        block_device(file->d_name);
    }

    closedir(dir);
}

void block_device(const char *name)
{
    char file[BUFSIZ];

    if (!is_event(name))
        return ;
    if (snprintf(file, sizeof(file), "%s/%s", DIR_TO_WATCH, name) >= (int)sizeof(file))
    {
        fprintf(stderr, "Percorso troppo lungo!\n");
        exit(EXIT_FAILURE);
    }

    int fd = open(file, O_RDWR | O_CLOEXEC);
    if (fd < 0)
    {
        PERROR("open");
        exit(EXIT_FAILURE);
    }

    char device_name[256] = "unknown";
    if (ioctl(fd, EVIOCGNAME(sizeof(device_name)), device_name) >= 0)
        printf(GRN "Aperto: %-20s  %-40s" RES, file, device_name);

    // Prova il grab esclusivo
    if (ioctl(fd, EVIOCGRAB, 1) < 0) {
        PERROR("\nEVIOCGRAB");
        close(fd);
        exit(EXIT_FAILURE);
    }
    printf(BLU "%20s" RES "\n", "[GRABBED]");

    if (is_keyboard(fd))
    {
        g_fd_keyboard = fd;
        printf(YEL "keyboard found!\n" RES);
    }

    handle_fds(A_ADD, fd);
}

static bool is_keyboard(int fd)
{
    unsigned long evbit[EV_MAX/sizeof(long) + 1];
    memset(evbit, 0, sizeof(evbit));

    // Verifica che supporti eventi EV_KEY
    if (ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit) < 0)
        return (false);

    
    if (!(evbit[EV_KEY / (8 * sizeof(long))] & (1 << (EV_KEY % (8 * sizeof(long))))))
        return (false);

    // Controlla se ha tasti caratteristici di tastiera (KEY_A, KEY_Z, KEY_ENTER)
    unsigned long keybit[KEY_MAX/sizeof(long) + 1];
    memset(keybit, 0, sizeof(keybit));
    ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit);

    bool has_keys = false;
    if (keybit[KEY_A / (8 * sizeof(long))] & (1 << (KEY_A % (8 * sizeof(long)))))
        has_keys = true;
    if (keybit[KEY_ENTER / (8 * sizeof(long))] & (1 << (KEY_ENTER % (8 * sizeof(long)))))
        has_keys = true;

    return (has_keys);
}


bool is_event(const char *name)
{
    struct stat st;
    char buf[BUFSIZ];

    // isn't a /event device
    if (strstr(name, "event") == NULL)
        return (false);

    // taking a full path
    if (snprintf(buf, sizeof(buf), "%s/%s", DIR_TO_WATCH, name) >= (int)sizeof(buf))
    {
        fprintf(stderr, "Percorso troppo lungo!\n");
        return (false);
    }

    if (stat(buf, &st) < 0)
    {
        PERROR("stat");
        return (false);
    }
    // make sure is a character device file
    return (S_ISCHR(st.st_mode));
}