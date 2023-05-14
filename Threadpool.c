#include "threadpool.h"
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

#define SHUTDOWN 1

threadpool* create_threadpool(int num_threads_in_pool) {
    if (num_threads_in_pool <= 0 || num_threads_in_pool > MAXT_IN_POOL)
        return NULL;

    threadpool* t_pool = malloc(sizeof(threadpool));
    if (t_pool == NULL)
        return NULL;

    t_pool->num_threads = num_threads_in_pool;
    t_pool->qsize = 0;
    t_pool->threads = malloc(sizeof(pthread_t) * num_threads_in_pool);
    if (t_pool->threads == NULL) {
        free(t_pool);
        return NULL;
    }

    t_pool->qhead = NULL;
    t_pool->qtail = NULL;

    if (pthread_mutex_init(&(t_pool->qlock), NULL) != 0 ||
        pthread_cond_init(&(t_pool->q_not_empty), NULL) != 0 ||
        pthread_cond_init(&(t_pool->q_empty), NULL) != 0) {
        free(t_pool->threads);
        free(t_pool);
        return NULL;
    }

    t_pool->shutdown = 0;
    t_pool->dont_accept = 0;

    int i;
    for (i = 0; i < t_pool->num_threads; i++) {
        if (pthread_create(&((t_pool->threads)[i]), NULL, do_work, t_pool) != 0) {
            free(t_pool->threads);
            free(t_pool);
            return NULL;
        }
    }

    return t_pool;
}

void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void* arg) {
    if (from_me == NULL || dispatch_to_here == NULL) {
        puts("No variables as required (threadpool/dispatch_fn)");
        return;
    }

    pthread_mutex_lock(&(from_me->qlock));

    if (!(from_me->dont_accept)) {
        work_t* work = malloc(sizeof(work_t));
        if (work == NULL) {
            pthread_mutex_unlock(&(from_me->qlock));
            return;
        }

        work->routine = dispatch_to_here;
        work->arg = arg;
        work->next = NULL;

        if (from_me->qhead == NULL) {
            from_me->qhead = work;
            from_me->qtail = work;
        } else {
            from_me->qtail->next = work;
            from_me->qtail = work;
        }

        from_me->qsize++;
        pthread_cond_signal(&from_me->q_not_empty);
    } else {
        puts("In else");
    }

    pthread_mutex_unlock(&(from_me->qlock));
}

void* do_work(void* p) {
    if (p == NULL)
        return NULL;

    threadpool* tp = (threadpool*)p;

    while (1) {
        pthread_mutex_lock(&(tp->qlock));

        while (tp->qsize == 0 && tp->shutdown != SHUTDOWN) {
            pthread_cond_wait(&(tp->q_not_empty), &(tp->qlock));
        }

        if (tp->shutdown == SHUTDOWN) {
            pthread_mutex_unlock(&(tp->qlock));
            return NULL;
        }

        work_t* work = tp->qhead;
        if (!work) {
            pthread_mutex_unlock(&(tp->qlock));
            continue;
        }

        tp->qsize--;

        if (tp->qsize == 0) {
            tp->qhead = NULL;
            tp->qtail = NULL;

