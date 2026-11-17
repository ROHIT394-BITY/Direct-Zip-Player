#include "threading.h"
#include <stdlib.h>

#if defined(_WIN32)

typedef struct {
    void (*func)(void *);
    void *arg;
} dz_win32_thread_wrapper_t;

static unsigned __stdcall win32_thread_entry(void *param) {
    dz_win32_thread_wrapper_t *wrapper = (dz_win32_thread_wrapper_t *)param;
    if (wrapper) {
        void (*func)(void *) = wrapper->func;
        void *arg = wrapper->arg;
        free(wrapper);
        if (func) {
            func(arg);
        }
    }
    return 0;
}

void dz_mutex_init(dz_mutex_t *mutex) {
    InitializeCriticalSection(mutex);
}

void dz_mutex_lock(dz_mutex_t *mutex) {
    EnterCriticalSection(mutex);
}

void dz_mutex_unlock(dz_mutex_t *mutex) {
    LeaveCriticalSection(mutex);
}

void dz_mutex_destroy(dz_mutex_t *mutex) {
    DeleteCriticalSection(mutex);
}

void dz_cond_init(dz_cond_t *cond) {
    InitializeConditionVariable(cond);
}

void dz_cond_wait(dz_cond_t *cond, dz_mutex_t *mutex) {
    SleepConditionVariableCS(cond, mutex, INFINITE);
}

bool dz_cond_wait_timeout(dz_cond_t *cond, dz_mutex_t *mutex, uint32_t timeout_ms) {
    return SleepConditionVariableCS(cond, mutex, timeout_ms) != 0;
}

void dz_cond_signal(dz_cond_t *cond) {
    WakeConditionVariable(cond);
}

void dz_cond_broadcast(dz_cond_t *cond) {
    WakeAllConditionVariable(cond);
}

void dz_cond_destroy(dz_cond_t *cond) {
    (void)cond;
}

bool dz_thread_create(dz_thread_t *thread, void (*func)(void *), void *arg) {
    dz_win32_thread_wrapper_t *wrapper = (dz_win32_thread_wrapper_t *)malloc(sizeof(dz_win32_thread_wrapper_t));
    if (!wrapper) return false;
    wrapper->func = func;
    wrapper->arg = arg;

    uintptr_t handle = _beginthreadex(NULL, 0, win32_thread_entry, wrapper, 0, NULL);
    if (handle == 0) {
        free(wrapper);
        *thread = NULL;
        return false;
    }
    *thread = (HANDLE)handle;
    return true;
}

void dz_thread_join(dz_thread_t thread) {
    if (thread && thread != INVALID_HANDLE_VALUE) {
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
    }
}

void dz_thread_sleep(uint32_t ms) {
    Sleep(ms);
}

#else

typedef struct {
    void (*func)(void *);
    void *arg;
} dz_posix_thread_wrapper_t;

static void *posix_thread_entry(void *param) {
    dz_posix_thread_wrapper_t *wrapper = (dz_posix_thread_wrapper_t *)param;
    if (wrapper) {
        void (*func)(void *) = wrapper->func;
        void *arg = wrapper->arg;
        free(wrapper);
        if (func) {
            func(arg);
        }
    }
    return NULL;
}

void dz_mutex_init(dz_mutex_t *mutex) {
    pthread_mutex_init(mutex, NULL);
}

void dz_mutex_lock(dz_mutex_t *mutex) {
    pthread_mutex_lock(mutex);
}

void dz_mutex_unlock(dz_mutex_t *mutex) {
    pthread_mutex_unlock(mutex);
}

void dz_mutex_destroy(dz_mutex_t *mutex) {
    pthread_mutex_destroy(mutex);
}

void dz_cond_init(dz_cond_t *cond) {
    pthread_cond_init(cond, NULL);
}

void dz_cond_wait(dz_cond_t *cond, dz_mutex_t *mutex) {
    pthread_cond_wait(cond, mutex);
}

bool dz_cond_wait_timeout(dz_cond_t *cond, dz_mutex_t *mutex, uint32_t timeout_ms) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }
    return pthread_cond_timedwait(cond, mutex, &ts) == 0;
}

void dz_cond_signal(dz_cond_t *cond) {
    pthread_cond_signal(cond);
}

void dz_cond_broadcast(dz_cond_t *cond) {
    pthread_cond_broadcast(cond);
}

void dz_cond_destroy(dz_cond_t *cond) {
    pthread_cond_destroy(cond);
}

bool dz_thread_create(dz_thread_t *thread, void (*func)(void *), void *arg) {
    dz_posix_thread_wrapper_t *wrapper = (dz_posix_thread_wrapper_t *)malloc(sizeof(dz_posix_thread_wrapper_t));
    if (!wrapper) return false;
    wrapper->func = func;
    wrapper->arg = arg;

    if (pthread_create(thread, NULL, posix_thread_entry, wrapper) != 0) {
        free(wrapper);
        return false;
    }
    return true;
}

void dz_thread_join(dz_thread_t thread) {
    pthread_join(thread, NULL);
}

void dz_thread_sleep(uint32_t ms) {
    usleep((useconds_t)ms * 1000);
}

#endif
