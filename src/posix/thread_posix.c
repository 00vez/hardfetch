#include "thread_posix.h"

#include <errno.h>
#include <time.h>

static void* thread_trampoline(void* p)
{
    hf_thread_posix* th = (hf_thread_posix*)p;
    void* result = th->fn(th->arg);

    pthread_mutex_lock(&th->lock);
    th->done = 1;
    pthread_cond_signal(&th->cond);
    pthread_mutex_unlock(&th->lock);

    return result;
}

int hf_thread_posix_start(hf_thread_posix* th, hf_thread_fn fn, void* arg)
{
    th->fn = fn;
    th->arg = arg;
    th->started = 0;
    th->done = 0;

    if (pthread_mutex_init(&th->lock, NULL) != 0) return 0;
    if (pthread_cond_init(&th->cond, NULL) != 0) {
        pthread_mutex_destroy(&th->lock);
        return 0;
    }
    if (pthread_create(&th->handle, NULL, thread_trampoline, th) != 0) {
        pthread_cond_destroy(&th->cond);
        pthread_mutex_destroy(&th->lock);
        return 0;
    }
    th->started = 1;
    return 1;
}

int hf_thread_posix_wait(hf_thread_posix* th, unsigned int timeoutMs)
{
    struct timespec deadline;
    int finished = 0;

    if (!th->started) return 0;

    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += (time_t)(timeoutMs / 1000);
    deadline.tv_nsec += (long)(timeoutMs % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }

    pthread_mutex_lock(&th->lock);
    while (!th->done) {
        if (pthread_cond_timedwait(&th->cond, &th->lock, &deadline) != 0) break;
    }
    finished = th->done;
    pthread_mutex_unlock(&th->lock);

    if (!finished) return 0;

    pthread_join(th->handle, NULL);
    pthread_cond_destroy(&th->cond);
    pthread_mutex_destroy(&th->lock);
    th->started = 0;
    return 1;
}

void hf_thread_posix_close(hf_thread_posix* th)
{
    (void)th;
}
