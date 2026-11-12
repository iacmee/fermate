#include "fermate.h"

void get_max_block_time(int ac, char *av[])
{
    g_max_block_time = DEFAULT_BLOCK_TIME;
    if (ac <= 1)
        return ;
    int block_time = atoi(av[1]);
    if (block_time == 0)
    {
        fprintf(stderr, RED "invalid argment, not a number. Default time set\n" RES);
        return ;
    }
    g_max_block_time = block_time;
}