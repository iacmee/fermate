#include "fermate.h"

volatile int g_fd_keyboard = -1;
volatile int g_max_block_time = 0;

static void read_from_event(struct input_event *ev)
{
    ssize_t byte_read = read(g_fd_keyboard, ev, sizeof(struct input_event));
    if (byte_read == -1)
    {
        PERROR("read");
        exit(EXIT_FAILURE);
    }
    if (byte_read != sizeof(struct input_event))
    {
        printf("ho letto una size sbaglaita\n");
        exit(EXIT_FAILURE);
    }
}

void hexdump(const void *data, size_t size) {
    const unsigned char *p = (const unsigned char *)data;
    size_t i, j;

    for (i = 0; i < size; i += 16) {
        // stampa offset
        printf("%08zx  ", i);

        // stampa i byte in esadecimale
        for (j = 0; j < 16; j++) {
            if (i + j < size)
                printf("%02x ", p[i + j]);
            else
                printf("   ");
            if (j == 7) printf(" "); // spaziatura extra a metà riga
        }

        printf(" |");

        // stampa in ASCII i caratteri stampabili
        for (j = 0; j < 16 && i + j < size; j++) {
            unsigned char c = p[i + j];
            printf("%c", isprint(c) ? c : '.');
        }

        printf("|\n");
    }
}

bool get_next_character(XkbEnv *env, char utf8[4])
{
    struct input_event ev;

    read_from_event(&ev);

    if (ev.type == EV_KEY) 
    {
        xkb_keycode_t kc = xkb_update_from_evdev(env, ev.code, ev.value);
        xkb_keysym_t sym;
        if (xkb_ev_to_utf8(env, kc, ev.value, utf8, 4, &sym))
            return (true);
    }
    utf8[0] = '\0'; // if is not an key_event or is key_realese
    return (false);
}

static inline void init(XkbEnv *env)
{
	setlocale(LC_CTYPE, "");
	xkb_env_init_from_rmlvo(env, "pc105", "it", NULL, NULL);
	xkb_env_enable_compose(env, NULL);
}


void update_user_buffer(char user_buf[USR_BUF_SIZE], size_t *buf_len, char utf8[4])
{
    if (*utf8 == '\b')
    {
        if (*buf_len == 0)
            return ;
        for (int32_t i = 0; i < 4; ++i)
        {
            bool exit = !(user_buf[*buf_len - 1] & 0x80);
            user_buf[*buf_len - 1] = '\0';
            *buf_len -= 1;
            if (exit)
                return ;
        }
        write(STDOUT_FILENO, "\b \b", 3);
    }
    strcat(user_buf + *buf_len, utf8);
    *buf_len += strlen(utf8);
}

void loop()
{
    XkbEnv env;
    char *target = EMERGENCY_EXIT;
    char user_buf[USR_BUF_SIZE];
    char utf8[4];
    size_t buf_len = 0;

    *user_buf = '\0';
    init(&env);
    while (true)
    {
        if (get_next_character(&env, utf8))
        {
            // hexdump(utf8, 4);
            if (*utf8 == '\r')
            {
                if (strcmp(user_buf, target) == 0)
                    return ;
                else
                {
                    buf_len = 0;
                    *user_buf = '\0';
                }
            }
            else
                update_user_buffer(user_buf, &buf_len, utf8);
        }
        if (*utf8 == 'm')
            printf("BUFFER : '%s'\n", user_buf);
    }
}


int	main(int ac, char *av[])
{
    // sleep to not bug the terminal
    usleep(100000);
    get_max_block_time(ac, av);
    block_all_device_and_set_fd_keyboard();
    launch_thread_for_continuing_blocking();

    loop();
    printf(CYN "complimenti\n" RES);
    handle_fds(A_EXIT, 0);

    return (0);
}