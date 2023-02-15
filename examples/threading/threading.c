#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    int ret;
    
    usleep(thread_func_args->wait_to_obtain_ms*1000);
    
    ret = pthread_mutex_lock(thread_func_args->mutex);

    if (!ret) {
      thread_func_args->thread_complete_success = true ;
    }
    else {
      printf("in threadfunc, CAN NOT obtained lock\n");
    }

    thread_func_args->cnt+=100;
    
    usleep(thread_func_args->wait_to_release_ms*1000);
    ret = pthread_mutex_unlock(thread_func_args->mutex);
    
    if (!ret) {
      thread_func_args->thread_complete_success = true ;
      printf("unlocked \n");
    }
    else {
      thread_func_args->thread_complete_success = false;
    }

    return (void *)thread_func_args;
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

  // allocate memory for thread_data
  struct thread_data * thread_param = (struct thread_data *) malloc(sizeof(struct thread_data));
  int ret;

  printf("thread_param->mutex = %p\n", thread_param->mutex);
  printf("param mutex = %p\n", mutex);

  
  thread_param->mutex = mutex;
  thread_param->cnt = 0;
  thread_param->wait_to_obtain_ms = wait_to_obtain_ms;
  thread_param->wait_to_release_ms = wait_to_release_ms;
  thread_param->thread_complete_success = false;
  
  printf("thread_param->mutex = %p\n", thread_param->mutex);
  printf("%d, %d\n", thread_param->wait_to_obtain_ms, thread_param->wait_to_release_ms);
  printf("calling pthread_create\n");
  ret = pthread_create(thread, NULL, threadfunc, (void *)thread_param);

  if (ret){ //on error, pthread_create returns non zero error code
    errno = ret;
    perror("pthread_create");
    return false;
  }

  printf("thread created\n");
  return true;
}
