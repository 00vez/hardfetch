#ifndef HF_THREAD_POSIX_H
#define HF_THREAD_POSIX_H

#include <pthread.h>

/* A thread that has to be waited for with a deadline.
   macOS has no pthread_timedjoin_np, so the worker marks itself done and
   wakes the waiter, and the waiter waits with a deadline on the condition
   variable. The flag is only set after the work function returned, so a
   successful wait means the thread really finished, not just that it is
   about to. */

typedef void* (*hf_thread_fn)(void*);

typedef struct {
    pthread_t        handle;
    pthread_mutex_t  lock;
    pthread_cond_t   cond;
    hf_thread_fn     fn;
    void*            arg;
    int              started;
    int              done;
} hf_thread_posix;

/* 1 when the thread was started. */
int hf_thread_posix_start(hf_thread_posix* th, hf_thread_fn fn, void* arg);

/* 1 when the thread finished within the timeout, 0 when it is still
   running. A finished thread is joined and released. */
int hf_thread_posix_wait(hf_thread_posix* th, unsigned int timeoutMs);

/* Releases what is left. A thread that did not finish keeps running, so
   its condition variable stays valid until the process ends. */
void hf_thread_posix_close(hf_thread_posix* th);

#endif
