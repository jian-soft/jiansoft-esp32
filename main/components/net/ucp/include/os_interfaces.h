#ifndef _OS_INTERFACES_H_
#define _OS_INTERFACES_H_

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#ifdef __ANDROID__
#include <android/log.h>
#endif

//memory
#define os_malloc   malloc
#define os_calloc   calloc
#define os_free     free
#define os_memcpy   memcpy
#define os_memset   memset

//posix prhread
int os_create_thread(void *(*start_routine)(void *), void *arg);

#define os_mutex_t     pthread_mutex_t
int os_mutex_init(os_mutex_t *mutex);
#define os_mutex_destroy pthread_mutex_destroy
#define os_mutex_lock   pthread_mutex_lock
#define os_mutex_unlock pthread_mutex_unlock

#define os_sem_t sem_t
int os_sem_init(os_sem_t *sem);
#define os_sem_wait sem_wait
#define os_sem_post sem_post

#include "esp_timer.h"
static inline uint32_t os_get_ms_ts()
{
    uint64_t ts_us;
    ts_us = esp_timer_get_time();

    return ts_us/1000;
}

static inline uint32_t os_get_us_ts()
{
    uint64_t ts_us;
    ts_us = esp_timer_get_time();

    return ts_us;
}

static inline uint64_t os_get_ns_ts()
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC_RAW, &now);
    return (now.tv_sec * 1000000000 + now.tv_nsec);
}


#ifdef __ANDROID__
#define os_log(...) ((void)__android_log_print(ANDROID_LOG_INFO, "*BotFpv*", __VA_ARGS__))
#else
#define os_log printf
#endif


#include <unistd.h>
//utils
#define os_sleep sleep
#define os_usleep usleep



#endif /* _OS_INTERFACES_H_ */

