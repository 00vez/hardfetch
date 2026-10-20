#ifndef HF_THREAD_POSIX_H
#define HF_THREAD_POSIX_H

#include <pthread.h>
#include <semaphore.h>

/* A thread that has to be waited for with a deadline.
   macOS has no pthread_timedjoin_np, so the worker posts a semaphore when
   it is done and the waiter counts that down instead of joining on a
   timer. The semaphore is posted after the work function returns, so a
   successful wait means the thread really finished, not just that it is
   about to. */

typedef void* (*hf_thread_fn)(void*);

typedef struct {
    pthread_t    handle;
    sem_t        done;
    hf_thread_fn fn;
    void*        arg;
    int          started;
} hf_thread_posix;

/* 1 when the thread was started. */
int hf_thread_posix_start(hf_thread_posix* th, hf_thread_fn fn, void* arg);

/* 1 when the thread finished within the timeout, 0 when it is still
   running. A finished thread is joined and released. */
int hf_thread_posix_wait(hf_thread_posix* th, unsigned int timeoutMs);

/* Releases what is left. A thread that did not finish keeps running, so
   its semaphore stays valid until the process ends. */
void hf_thread_posix_close(hf_thread_posix* th);

#endif
