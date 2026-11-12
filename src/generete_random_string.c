#include "fermate.h"


int32_t bit_one_count(int32_t n)
{
    int32_t count = 0;

    while (n)
    {
        count += n & 1;
        n >>= 1;
    }
    return (count);
}

// void get_random_chars(char utf8[4], int32_t ch_supported)
// {
//     int32_t range = bit_one_count(ch_supported);
//     int32_t rd = rand();


// }

char *generate_random_string(size_t len, int32_t ch_supported)
{
    srand(time(NULL));

    // max size if all are utf8 with 4 byte
    char *rdstr = (char *)malloc(sizeof(char) * (len * 4 + 1));


    (void)ch_supported;
    (void)len;
    (void)rdstr;
    return (NULL);
}
