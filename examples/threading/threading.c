#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    thread_data_s *thread_params = (thread_data_s *)thread_param;

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    
    usleep(thread_params->wait_to_obtain_us);
    pthread_mutex_lock(thread_params->mutex);
    usleep(thread_params->wait_to_release_us);
    pthread_mutex_unlock(thread_params->mutex);

    thread_params->thread_complete_success = true;
    
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */

    int retVal = 0;

    thread_data_s *thread_params = (thread_data_s *)malloc(sizeof(thread_data_s));

    thread_params->wait_to_obtain_us = wait_to_obtain_ms * 1000;
    thread_params->wait_to_release_us = wait_to_release_ms * 1000;
    thread_params->mutex = mutex;

    retVal = pthread_create(thread, NULL, threadfunc, thread_params);

    return (bool)!retVal;
}

