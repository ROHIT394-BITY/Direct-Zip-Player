#ifndef DIRECTZIP_ZIP_ENGINE_H
#define DIRECTZIP_ZIP_ENGINE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "memory_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char filename[512];
    char display_name[256];
    uint64_t uncompressed_size;
    uint64_t compressed_size;
    uint32_t file_index;
    uint16_t compression_method;
    bool is_mp4;
    double duration_sec;
} ZipEntry;

typedef struct ZipArchive ZipArchive;

// Progress callback: progress is 0.0f to 1.0f
typedef void (*ZipProgressCallback)(float progress, const char *status, void *user_data);

// Open a ZIP archive from disk, parse its central directory, and index all MP4 files
ZipArchive* zip_engine_open(const char *path);

// Close a ZIP archive and free its metadata
void zip_engine_close(ZipArchive *archive);

// Get total number of entries found in the archive
int zip_engine_get_total_entries(const ZipArchive *archive);

// Get number of MP4 video entries found
int zip_engine_get_mp4_count(const ZipArchive *archive);

// Get MP4 entry by playlist index (0 to mp4_count - 1)
const ZipEntry* zip_engine_get_mp4_entry(const ZipArchive *archive, int index);

// Get the path of the opened archive
const char* zip_engine_get_path(const ZipArchive *archive);

// Decompress an entry completely into RAM (Zero-Extraction)
// Returns a dynamically allocated MemoryBuffer containing the uncompressed MP4 data, or NULL on error
MemoryBuffer* zip_engine_decompress_entry(ZipArchive *archive, 
                                          const ZipEntry *entry,
                                          ZipProgressCallback progress_cb,
                                          void *user_data);

// Helper: check if a filename has an MP4 extension (.mp4, .m4v)
bool zip_engine_is_mp4_filename(const char *filename);

#ifdef __cplusplus
}
#endif

#endif // DIRECTZIP_ZIP_ENGINE_H
