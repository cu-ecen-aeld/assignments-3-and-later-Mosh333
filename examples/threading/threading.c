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

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    //Multiply by 1000 to convert from micro to mill second since usleep operates on micro
    usleep(thread_func_args->wait_to_obtain_ms * 1000);

    // Attempt to lock mutex
    if (pthread_mutex_lock(thread_func_args->thread_mutex) != 0) {
        // Mark the thread as unsuccessful since locking failed
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }

    usleep(thread_func_args->wait_to_release_ms * 1000);

    // Unlock mutex
    if (pthread_mutex_unlock(thread_func_args->thread_mutex) != 0) {
        // Mark the thread as unsuccessful since unlocking failed
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }
    
    // Since we got here, mark as successful
    thread_func_args->thread_complete_success = true;

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    struct thread_data *my_thread_data = (struct thread_data *)malloc(sizeof(struct thread_data));

    if(my_thread_data!=NULL){
        // Setup thread data
        my_thread_data->thread_complete_success = false;
        my_thread_data->thread_mutex = mutex;
        my_thread_data->wait_to_obtain_ms = wait_to_obtain_ms;
        my_thread_data->wait_to_release_ms = wait_to_release_ms;

        
        if(pthread_create(thread,NULL,threadfunc,(void*)my_thread_data)!=0){
            //Thread creation failed, free data and exist with false
            free(my_thread_data);
            return false;
        }
        
        return true;
    }

    return false;
}

