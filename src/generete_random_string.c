#include "fermate.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
char *generate_random_string(size_t length, int32_t classes)
{
    char alphabet[128] = "";
    if (classes <= 0 || (classes & ~(CH_ASCII_ALPHA | CH_ASCII_NUMBER | CH_ASCII_SYMBOL)) || length == SIZE_MAX)
    {
        errno = EINVAL;
        return (NULL);
    }
    if (classes & CH_ASCII_ALPHA)
        strcat(alphabet, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
    if (classes & CH_ASCII_NUMBER)
        strcat(alphabet, "0123456789");
    if (classes & CH_ASCII_SYMBOL)
        strcat(alphabet, "!@#$%&*+-=?");
    char *result = malloc(length + 1);
    if (!result)
        return NULL;
    size_t n = strlen(alphabet);
    unsigned limit = 256 - 256 % n;
    for (size_t i = 0; i < length;)
    {
        unsigned char byte;
        ssize_t count = getrandom(&byte, 1, 0);
        if (count < 0 && errno == EINTR)
            continue;
        if (count != 1)
        {
            free(result);
            return NULL;
        }
        if (byte < limit)
            result[i++] = alphabet[byte % n];
    }
    result[length] = '\0';
    return result;
}
