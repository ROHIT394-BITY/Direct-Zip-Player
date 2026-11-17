#ifndef DIRECTZIP_THREADING_H
#define DIRECTZIP_THREADING_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>

typedef HANDLE dz_thread_t;
typedef CRITICAL_SECTION dz_mutex_t;
typedef CONDITION_VARIABLE dz_cond_t;
typedef unsigned (__stdcall *dz_thread_func_t)(void *);

#else
#include <pthread.h>
#include <unistd.h>

typedef pthread_t dz_thread_t;
typedef pthread_mutex_t dz_mutex_t;
typedef pthread_cond_t dz_cond_t;
typedef void *(*dz_thread_func_t)(void *);
#endif

// Mutex operations
void dz_mutex_init(dz_mutex_t *mutex);
void dz_mutex_lock(dz_mutex_t *mutex);
void dz_mutex_unlock(dz_mutex_t *mutex);
void dz_mutex_destroy(dz_mutex_t *mutex);

// Condition variable operations
void dz_cond_init(dz_cond_t *cond);
void dz_cond_wait(dz_cond_t *cond, dz_mutex_t *mutex);
bool dz_cond_wait_timeout(dz_cond_t *cond, dz_mutex_t *mutex, uint32_t timeout_ms);
void dz_cond_signal(dz_cond_t *cond);
void dz_cond_broadcast(dz_cond_t *cond);
void dz_cond_destroy(dz_cond_t *cond);

// Thread creation and lifecycle
bool dz_thread_create(dz_thread_t *thread, void (*func)(void *), void *arg);
void dz_thread_join(dz_thread_t thread);
void dz_thread_sleep(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif // DIRECTZIP_THREADING_H
