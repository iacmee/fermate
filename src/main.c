#include "fermate.h"
#include "xkb.h"
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <sys/timerfd.h>
#include <unistd.h>
static volatile sig_atomic_t g_stopped;
static const char *color(FILE *stream, const char *code)
{
    return isatty(fileno(stream)) && !getenv("NO_COLOR") ? code : "";
}
#define CYAN "\033[1;36m"
#define GREEN "\033[1;32m"
#define YELLOW "\033[1;33m"
#define RED "\033[1;31m"
#define RESET "\033[0m"
static void show_prompt(const char *buffer)
{
    if (isatty(STDOUT_FILENO))
        printf("\r\033[2K");
    printf("%s> %s%s", color(stdout, CYAN), color(stdout, RESET), buffer);
    fflush(stdout);
}
static void stop(int signo)
{
    (void)signo;
    g_stopped = 1;
}
static int handle_key(struct device *device, XkbEnv *env, const struct input_event *event,
                      char *buffer, size_t *length, const char *target, int timer)
{
    if (event->type == EV_SYN && event->code == SYN_DROPPED)
        return (-1);
    if (event->type != EV_KEY || event->value < 0 || event->value > 2)
        return (0);
    if (!device->state)
    {
        device->state = xkb_state_new(env->keymap);
        if (!device->state)
            return (-1);
        if (env->compose_table)
        {
            device->compose = xkb_compose_state_new(env->compose_table, XKB_COMPOSE_STATE_NO_FLAGS);
            if (!device->compose)
                return (-1);
        }
    }
    XkbEnv local = *env;
    local.state = device->state;
    local.compose_state = device->compose;
    xkb_keycode_t code = event->code + 8;
    if (event->value != 2)
        xkb_state_update_key(local.state, code, event->value ? XKB_KEY_DOWN : XKB_KEY_UP);
    char text[64];
    xkb_keysym_t symbol;
    if (!xkb_ev_to_utf8(&local, code, event->value, text, sizeof(text), &symbol))
        return (0);
    if (*text == '\r' || *text == '\n')
    {
        putchar('\n');
        bool unlock = target && !strcmp(buffer, target);
#ifdef ENABLE_EMERGENCY_EXIT
        unlock = unlock || !strcmp(buffer, EMERGENCY_EXIT);
#endif
        if (unlock)
        {
            fflush(stdout);
            return (1);
        }
        if (!strcmp(buffer, "status"))
        {
            struct itimerspec remaining;
            if (timerfd_gettime(timer, &remaining) < 0)
                return (-1);
            long long seconds = (long long)remaining.it_value.tv_sec +
                                (remaining.it_value.tv_nsec > 0);
            printf("%sTime remaining: %lld seconds.%s\n",
                   color(stdout, CYAN), seconds, color(stdout, RESET));
        }
        else
            printf("%sIncorrect input. Try again, or type status and press Enter.%s\n",
                   color(stdout, RED), color(stdout, RESET));
        *length = 0;
        buffer[0] = '\0';
    }
    else
    {
        /* Ignore terminal control characters except Backspace. */
        if ((unsigned char)*text < 32 && *text != '\b')
            return (0);
        if (!update_user_buffer(buffer, length, USR_BUF_SIZE, text))
        {
            *length = 0;
            buffer[0] = '\0';
            fprintf(stderr, "\n%sInput too long. Line cleared.%s\n",
                    color(stderr, RED), color(stderr, RESET));
        }
    }
    show_prompt(buffer);
    return (0);
}
int main(int argc, char **argv)
{
    sleep(1);
    int duration = DEFAULT_BLOCK_TIME;
    if (argc == 2 && !strcmp(argv[1], "--help"))
    {
        puts("Usage: fermate [seconds] [--challenge]\nDefault: 300 seconds. Type status and press Enter to check the remaining time.\n--challenge enables a random string for early unlock.\nLayout: XKB_DEFAULT_LAYOUT (default: it).");
#ifdef ENABLE_EMERGENCY_EXIT
        puts("Test build: type exit and press Enter to unlock early.");
#endif
        return (EXIT_SUCCESS);
    }
    bool challenge = argc == 3 && !strcmp(argv[2], "--challenge");
    if (argc > 3 || (argc == 3 && !challenge) || (argc > 1 && !parse_duration(argv[1], &duration)))
    {
        fprintf(stderr, "%sUsage: fermate [positive seconds] [--challenge]%s\n",
                color(stderr, RED), color(stderr, RESET));
        return (EXIT_FAILURE);
    }
    struct sigaction action = {.sa_handler = stop};
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) || sigaction(SIGTERM, &action, NULL) || sigaction(SIGHUP, &action, NULL))
    {
        perror("sigaction");
        return (EXIT_FAILURE);
    }
    XkbEnv env = {0};
    struct devices devices = {0};
    int watcher = -1;
    int timer = -1;
    int result = EXIT_FAILURE;
    char *random = NULL;
    struct pollfd *fds = NULL;
    setlocale(LC_CTYPE, "");
    const char *layout = getenv("XKB_DEFAULT_LAYOUT");
    if (!xkb_env_init_from_rmlvo(&env, "pc105", layout ? layout : "it", NULL, NULL))
        goto cleanup;
    if (!xkb_env_enable_compose(&env, NULL))
        goto cleanup;
    if (challenge && !(random = generate_random_string(DEFAULT_CHALLENG_LEN, CH_ASCII_ALPHA | CH_ASCII_NUMBER)))
        goto cleanup;
    const char *target = random;
    watcher = watch_devices();
    if (watcher < 0)
    {
        perror("inotify");
        goto cleanup;
    }
    timer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer < 0)
    {
        perror("timerfd_create");
        goto cleanup;
    }
    struct itimerspec timeout = {.it_value = {.tv_sec = duration}};
    if (timerfd_settime(timer, 0, &timeout, NULL))
    {
        perror("timerfd_settime");
        goto cleanup;
    }
    printf("%sLocked for %d seconds.%s\n", color(stdout, CYAN), duration, color(stdout, RESET));
    printf("%sFERMATE%s  |  Take a break.\n", color(stdout, CYAN), color(stdout, RESET));
    puts("Type status and press Enter to check the remaining time.");
    if (target)
        printf("Emergency unlock: %s%s%s + Enter.\n",
               color(stdout, YELLOW), target, color(stdout, RESET));
#ifdef ENABLE_EMERGENCY_EXIT
    printf("%sTest build: type exit and press Enter to unlock early.%s\n",
           color(stdout, YELLOW), color(stdout, RESET));
#endif
    show_prompt("");
    if (scan_devices(&devices) < 0)
        goto cleanup;
    char buffer[USR_BUF_SIZE] = {0};
    size_t length = 0;
    while (!g_stopped)
    {
        size_t count = devices.size;
        struct pollfd *next = (struct pollfd *)realloc(fds, (count + 2) * sizeof(*fds));
        if (!next)
            goto cleanup;
        fds = next;
        fds[0] = (struct pollfd){.fd = timer, .events = POLLIN};
        fds[1] = (struct pollfd){.fd = watcher, .events = POLLIN};
        for (size_t i = 0; i < count; ++i)
            fds[i + 2] = (struct pollfd){.fd = devices.items[i].fd, .events = POLLIN};
        int ready = poll(fds, count + 2, -1);
        if (ready < 0)
        {
            if (errno == EINTR)
                continue;
            perror("poll");
            goto cleanup;
        }
        if (fds[0].revents & POLLIN)
            break;
        if (fds[0].revents & (POLLERR | POLLNVAL) || fds[1].revents & (POLLERR | POLLHUP | POLLNVAL))
            goto cleanup;
        /* Reverse order keeps indexes stable when removal swaps the last entry. */
        for (size_t i = count; i-- > 0;)
        {
            if (g_stopped)
                break;
            short events = fds[i + 2].revents;
            if (events & (POLLHUP | POLLERR | POLLNVAL))
            {
                remove_device(&devices, i);
                continue;
            }
            if (!(events & POLLIN))
                continue;
            struct input_event input;
            ssize_t bytes = read(devices.items[i].fd, &input, sizeof(input));
            if (bytes < 0 && (errno == EAGAIN || errno == EINTR))
                continue;
            if (bytes < 0 && errno == ENODEV)
            {
                remove_device(&devices, i);
                continue;
            }
            if (bytes != sizeof(input))
                goto cleanup;
            if (devices.items[i].keyboard)
            {
                int outcome = handle_key(&devices.items[i], &env, &input, buffer, &length, target, timer);
                if (outcome < 0)
                    goto cleanup;
                if (outcome > 0)
                    g_stopped = 1;
            }
        }
        if (!g_stopped && fds[1].revents & POLLIN)
            if (refresh_devices(watcher, &devices) < 0)
                goto cleanup;
    }
    result = EXIT_SUCCESS;
cleanup:
    release_devices(&devices);
    if (watcher >= 0)
        close(watcher);
    if (timer >= 0)
        close(timer);
    free(fds);
    free(random);
    xkb_env_destroy(&env);
    fprintf(stderr, "\n%s%s%s\n",
            color(stderr, result == EXIT_SUCCESS ? GREEN : RED),
            result == EXIT_SUCCESS ? "Devices unlocked. You're all set." : "Error: devices released.",
            color(stderr, RESET));
    return (result);
}
