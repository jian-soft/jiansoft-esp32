#include "os_interfaces.h"


int os_create_thread(void *(*start_routine)(void *), void *arg)
{
    int ret;
    pthread_t thread;

    ret = pthread_create(&thread, NULL, start_routine, arg);
    if (ret < 0) {
        os_log("pthread_create fail:%s\n", strerror(errno));
        return ret;
    }

    ret = pthread_detach(thread);
    if (ret < 0) {
        os_log("pthread_detach fail:%s\n", strerror(errno));
        return ret;
    }

    return 0;
}

int os_mutex_init(os_mutex_t *mutex)
{
    return pthread_mutex_init(mutex, NULL);
}

int os_sem_init(os_sem_t *sem)
{
    return sem_init(sem, 0, 0);
}

#include <sys/time.h>


