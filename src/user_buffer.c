#include "fermate.h"
#include <string.h>
bool update_user_buffer(char *buffer, size_t *length, size_t capacity, const char *text)
{
    if (*text == '\b' || *text == 127)
    {
        if (*length)
        {
            do
            {
                --(*length);
            } while (*length && ((unsigned char)buffer[*length] & 0xc0) == 0x80);
            buffer[*length] = '\0';
        }
        return (true);
    }
    size_t n = strlen(text);
    if (*length >= capacity || n >= capacity - *length)
        return (false);
    memcpy(buffer + *length, text, n + 1);
    *length += n;
    return (true);
}
