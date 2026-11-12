#include <unistd.h>

int main(void)
{
    char buf[] = {'p', 'r', 'o', 'v', 'a', '\0', 'd'};
    write (STDOUT_FILENO, buf, sizeof(buf));
}