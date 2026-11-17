#include "memory_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static CRITICAL_SECTION g_mem_cs;
static bool g_cs_initialized = false;
static void mem_lock(void) {
    if (!g_cs_initialized) {
        InitializeCriticalSection(&g_mem_cs);
        g_cs_initialized = true;
    }
    EnterCriticalSection(&g_mem_cs);
}
static void mem_unlock(void) {
    LeaveCriticalSection(&g_mem_cs);
}
#else
#include <pthread.h>
static pthread_mutex_t g_mem_mutex = PTHREAD_MUTEX_INITIALIZER;
static void mem_lock(void) { pthread_mutex_lock(&g_mem_mutex); }
static void mem_unlock(void) { pthread_mutex_unlock(&g_mem_mutex); }
#endif

static MemoryManagerStats g_stats = {
    .max_allowed_bytes = DIRECTZIP_DEFAULT_MAX_MEMORY,
    .currently_allocated_bytes = 0,
    .peak_allocated_bytes = 0,
    .total_allocations = 0
};

bool memory_manager_init(size_t max_allowed_bytes) {
    mem_lock();
    if (max_allowed_bytes == 0) {
        max_allowed_bytes = DIRECTZIP_DEFAULT_MAX_MEMORY;
    }
    g_stats.max_allowed_bytes = max_allowed_bytes;
    g_stats.currently_allocated_bytes = 0;
    g_stats.peak_allocated_bytes = 0;
    g_stats.total_allocations = 0;
    mem_unlock();
    return true;
}

bool memory_manager_can_allocate(size_t size) {
    if (size == 0) return false;
    
    // Check against SIZE_MAX overflow
    if (size > (SIZE_MAX - sizeof(MemoryBuffer) - 64)) {
        return false;
    }

    mem_lock();
    bool can = false;
    if (g_stats.currently_allocated_bytes + size <= g_stats.max_allowed_bytes) {
        can = true;
    }
    mem_unlock();
    return can;
}

MemoryBuffer* memory_buffer_alloc(size_t size) {
    if (!memory_manager_can_allocate(size)) {
        fprintf(stderr, "[MemoryManager] Allocation rejected: %zu bytes (limit: %zu, current: %zu)\n",
                size, g_stats.max_allowed_bytes, g_stats.currently_allocated_bytes);
        return NULL;
    }

    MemoryBuffer *buf = (MemoryBuffer*)malloc(sizeof(MemoryBuffer));
    if (!buf) {
        return NULL;
    }

    // Allocate 64 extra bytes for safety / SIMD alignment / FFmpeg padding requirements
    // AV_INPUT_BUFFER_PADDING_SIZE is typically 64
    size_t padded_size = size + 64;
    buf->data = (uint8_t*)malloc(padded_size);
    if (!buf->data) {
        free(buf);
        return NULL;
    }

    // Zero-fill padding at the end
    memset(buf->data + size, 0, 64);

    buf->size = size;
    buf->capacity = padded_size;
    buf->is_allocated = true;

    mem_lock();
    g_stats.currently_allocated_bytes += size;
    if (g_stats.currently_allocated_bytes > g_stats.peak_allocated_bytes) {
        g_stats.peak_allocated_bytes = g_stats.currently_allocated_bytes;
    }
    g_stats.total_allocations++;
    mem_unlock();

    return buf;
}

void memory_buffer_free(MemoryBuffer *buffer) {
    if (!buffer) return;

    mem_lock();
    if (buffer->is_allocated && buffer->data) {
        if (g_stats.currently_allocated_bytes >= buffer->size) {
            g_stats.currently_allocated_bytes -= buffer->size;
        } else {
            g_stats.currently_allocated_bytes = 0;
        }
        free(buffer->data);
        buffer->data = NULL;
        buffer->size = 0;
        buffer->capacity = 0;
        buffer->is_allocated = false;
    }
    mem_unlock();

    free(buffer);
}

MemoryManagerStats memory_manager_get_stats(void) {
    mem_lock();
    MemoryManagerStats s = g_stats;
    mem_unlock();
    return s;
}

void memory_manager_shutdown(void) {
    mem_lock();
    g_stats.currently_allocated_bytes = 0;
    mem_unlock();
}
