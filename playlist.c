#include "playlist.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct Playlist {
    PlaylistItem *items;
    int count;
    int capacity;
    int selected_index;
};

Playlist* playlist_create(void) {
    Playlist *pl = (Playlist *)calloc(1, sizeof(Playlist));
    if (!pl) return NULL;
    pl->selected_index = -1;
    return pl;
}

void playlist_destroy(Playlist *pl) {
    if (!pl) return;
    playlist_clear(pl);
    free(pl);
}

void playlist_clear(Playlist *pl) {
    if (!pl) return;
    if (pl->items) {
        free(pl->items);
        pl->items = NULL;
    }
    pl->count = 0;
    pl->capacity = 0;
    pl->selected_index = -1;
}

bool playlist_load_archive(Playlist *pl, const ZipArchive *archive) {
    if (!pl) return false;
    playlist_clear(pl);
    if (!archive) return false;

    int mp4_count = zip_engine_get_mp4_count(archive);
    if (mp4_count <= 0) return false;

    pl->items = (PlaylistItem *)calloc(mp4_count, sizeof(PlaylistItem));
    if (!pl->items) return false;

    pl->count = mp4_count;
    pl->capacity = mp4_count;

    for (int i = 0; i < mp4_count; i++) {
        const ZipEntry *entry = zip_engine_get_mp4_entry(archive, i);
        if (!entry) continue;

        PlaylistItem *item = &pl->items[i];
        item->index = i;
        item->zip_entry = entry;
        item->uncompressed_size = entry->uncompressed_size;
        item->compressed_size = entry->compressed_size;
        item->duration_sec = entry->duration_sec;
        strncpy(item->filename, entry->filename, sizeof(item->filename) - 1);
        strncpy(item->display_name, entry->display_name, sizeof(item->display_name) - 1);
    }

    if (pl->count > 0) {
        pl->selected_index = 0;
    }

    return true;
}

int playlist_get_count(const Playlist *pl) {
    return pl ? pl->count : 0;
}

int playlist_get_selected_index(const Playlist *pl) {
    return pl ? pl->selected_index : -1;
}

bool playlist_select_index(Playlist *pl, int index) {
    if (!pl || index < 0 || index >= pl->count) return false;
    pl->selected_index = index;
    return true;
}

bool playlist_select_next(Playlist *pl) {
    if (!pl || pl->count == 0) return false;
    if (pl->selected_index + 1 < pl->count) {
        pl->selected_index++;
        return true;
    }
    return false;
}

bool playlist_select_prev(Playlist *pl) {
    if (!pl || pl->count == 0) return false;
    if (pl->selected_index - 1 >= 0) {
        pl->selected_index--;
        return true;
    }
    return false;
}

const PlaylistItem* playlist_get_item(const Playlist *pl, int index) {
    if (!pl || index < 0 || index >= pl->count) return NULL;
    return &pl->items[index];
}

const PlaylistItem* playlist_get_selected(const Playlist *pl) {
    if (!pl || pl->selected_index < 0 || pl->selected_index >= pl->count) return NULL;
    return &pl->items[pl->selected_index];
}

void playlist_set_item_duration(Playlist *pl, int index, double duration_sec) {
    if (!pl || index < 0 || index >= pl->count) return;
    pl->items[index].duration_sec = duration_sec;
}
