#ifndef DIRECTZIP_MEMORY_MANAGER_H
#define DIRECTZIP_MEMORY_MANAGER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Default max memory limit: 2 GB
#define DIRECTZIP_DEFAULT_MAX_MEMORY (2ULL * 1024ULL * 1024ULL * 1024ULL)

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
    bool is_allocated;
} MemoryBuffer;

typedef struct {
    size_t max_allowed_bytes;
    size_t currently_allocated_bytes;
    size_t peak_allocated_bytes;
    size_t total_allocations;
} MemoryManagerStats;

// Initialize the memory manager subsystem
bool memory_manager_init(size_t max_allowed_bytes);

// Check if an allocation of the given size is permitted (safe against overflow)
bool memory_manager_can_allocate(size_t size);

// Allocate an in-memory buffer
MemoryBuffer* memory_buffer_alloc(size_t size);

// Free an allocated buffer and update accounting
void memory_buffer_free(MemoryBuffer *buffer);

// Get current memory statistics
MemoryManagerStats memory_manager_get_stats(void);

// Reset / clean up memory manager
void memory_manager_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // DIRECTZIP_MEMORY_MANAGER_H
