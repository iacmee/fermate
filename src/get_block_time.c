#include "fermate.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>
bool parse_duration(const char *text, int *seconds)
{
    char *end;
    if (!text || !*text)
        return (false);
    for (const char *p = text; *p; ++p)
        if (!isdigit((unsigned char)*p))
            return (false);
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno || *end || value <= 0 || value > INT_MAX)
        return (false);
    *seconds = (int)value;
    return (true);
}
