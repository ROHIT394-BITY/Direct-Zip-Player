#ifndef DIRECTZIP_PLAYLIST_H
#define DIRECTZIP_PLAYLIST_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "zip_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char filename[512];
    char display_name[256];
    uint64_t uncompressed_size;
    uint64_t compressed_size;
    double duration_sec;
    int index;
    const ZipEntry *zip_entry;
} PlaylistItem;

typedef struct Playlist Playlist;

Playlist* playlist_create(void);
void playlist_destroy(Playlist *pl);

void playlist_clear(Playlist *pl);
bool playlist_load_archive(Playlist *pl, const ZipArchive *archive);

int playlist_get_count(const Playlist *pl);
int playlist_get_selected_index(const Playlist *pl);

bool playlist_select_index(Playlist *pl, int index);
bool playlist_select_next(Playlist *pl);
bool playlist_select_prev(Playlist *pl);

const PlaylistItem* playlist_get_item(const Playlist *pl, int index);
const PlaylistItem* playlist_get_selected(const Playlist *pl);

void playlist_set_item_duration(Playlist *pl, int index, double duration_sec);

#ifdef __cplusplus
}
#endif

#endif // DIRECTZIP_PLAYLIST_H
