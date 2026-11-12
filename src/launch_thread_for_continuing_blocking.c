#include "fermate.h"

int32_t launch_thread_for_continuing_blocking()
{
    pthread_t thread;
    
    if (pthread_create(&thread, NULL, continuing_blocking_device, NULL))
    {
        PERROR("pthread_create");
        fprintf(stderr, "can't continuing blocking other device\n");
        return (EXIT_FAILURE);
    }

    return (EXIT_SUCCESS);
}
