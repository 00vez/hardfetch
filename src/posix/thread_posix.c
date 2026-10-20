#include "thread_posix.h"

#include <errno.h>
#include <time.h>

static void* thread_trampoline(void* p)
{
    hf_thread_posix* th = (hf_thread_posix*)p;
    void* result = th->fn(th->arg);

    sem_post(&th->done);
    return result;
}

int hf_thread_posix_start(hf_thread_posix* th, hf_thread_fn fn, void* arg)
{
    th->fn = fn;
    th->arg = arg;
    th->started = 0;

    if (sem_init(&th->done, 0, 0) != 0) return 0;
    if (pthread_create(&th->handle, NULL, thread_trampoline, th) != 0) {
        sem_destroy(&th->done);
        return 0;
    }
    th->started = 1;
    return 1;
}

int hf_thread_posix_wait(hf_thread_posix* th, unsigned int timeoutMs)
{
    struct timespec deadline;

    if (!th->started) return 0;

    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += (time_t)(timeoutMs / 1000);
    deadline.tv_nsec += (long)(timeoutMs % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }

    /* A signal can cut the wait short without the timeout being reached,
       so the wait continues until the deadline is really past. */
    for (;;) {
        if (sem_timedwait(&th->done, &deadline) == 0) break;
        if (errno != EINTR) return 0;
    }

    pthread_join(th->handle, NULL);
    sem_destroy(&th->done);
    th->started = 0;
    return 1;
}

void hf_thread_posix_close(hf_thread_posix* th)
{
    (void)th;
}
