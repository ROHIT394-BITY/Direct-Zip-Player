#ifndef DIRECTZIP_PLAYER_CONTROLLER_H
#define DIRECTZIP_PLAYER_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "zip_engine.h"
#include "memory_manager.h"
#include "video_decoder.h"
#include "video_upscaler.h"
#include "audio_player.h"
#include "playlist.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PLAYER_STATE_IDLE = 0,
    PLAYER_STATE_DECOMPRESSING,
    PLAYER_STATE_PLAYING,
    PLAYER_STATE_PAUSED,
    PLAYER_STATE_STOPPED,
    PLAYER_STATE_ERROR
} PlayerState;

typedef struct {
    int native_width;
    int native_height;
    int output_width;
    int output_height;
    bool upscaling_active;
    char upscaling_algorithm[32];
    char upscaling_mode_name[32];
    double scale_factor;
    double original_fps;
    double current_fps;
    int time_base_num;
    int time_base_den;
    double current_timestamp;
    double audio_clock;
    double video_clock;
    double av_sync_diff_ms;
    uint64_t decoded_frames;
    uint64_t dropped_frames;
    float playback_speed;
    bool is_audio_master;
    bool diagnostics_enabled;
} PlayerDiagnostics;

typedef struct PlayerController PlayerController;

// Create and initialize player controller
PlayerController* player_controller_create(void);

// Destroy player controller and free all resources
void player_controller_destroy(PlayerController *pc);

// Open a ZIP archive from disk
bool player_controller_open_zip(PlayerController *pc, const char *zip_path);

// Close current archive and release memory
void player_controller_close_zip(PlayerController *pc);

// Playback controls
bool player_controller_load_track(PlayerController *pc, int playlist_index);
void player_controller_play(PlayerController *pc);
void player_controller_pause(PlayerController *pc);
void player_controller_toggle_play_pause(PlayerController *pc);
void player_controller_stop(PlayerController *pc);

bool player_controller_next(PlayerController *pc);
bool player_controller_prev(PlayerController *pc);

// Seeking: seek to absolute seconds or relative (+5s, -5s)
void player_controller_seek_to(PlayerController *pc, double target_sec);
void player_controller_seek_relative(PlayerController *pc, double delta_sec);

// Volume and audio
void player_controller_set_volume(PlayerController *pc, float volume);
float player_controller_get_volume(const PlayerController *pc);
void player_controller_toggle_mute(PlayerController *pc);
bool player_controller_is_muted(const PlayerController *pc);

// State queries
PlayerState player_controller_get_state(const PlayerController *pc);
const char* player_controller_get_status_text(const PlayerController *pc);
float player_controller_get_decompression_progress(const PlayerController *pc);

double player_controller_get_current_time(const PlayerController *pc);
double player_controller_get_duration(const PlayerController *pc);

// Get current video dimensions and resolution label
void player_controller_get_video_size(const PlayerController *pc, int *width, int *height);
const char* player_controller_get_resolution_label(const PlayerController *pc);

// Video Upscaling Configuration
void player_controller_set_upscaling_mode(PlayerController *pc, UpscalingMode mode);
UpscalingMode player_controller_get_upscaling_mode(const PlayerController *pc);
void player_controller_set_upscaling_quality(PlayerController *pc, UpscalingQuality quality);
UpscalingQuality player_controller_get_upscaling_quality(const PlayerController *pc);
VideoUpscalerInfo player_controller_get_upscaler_info(const PlayerController *pc);

// Diagnostics overlay
void player_controller_toggle_diagnostics(PlayerController *pc);
bool player_controller_is_diagnostics_enabled(const PlayerController *pc);
PlayerDiagnostics player_controller_get_diagnostics(const PlayerController *pc);

// Video frame access for UI rendering
bool player_controller_lock_current_frame(PlayerController *pc, const uint8_t **out_rgb_data, int *width, int *height, int *linesize);
bool player_controller_lock_video_frame(PlayerController *pc, DecodedVideoFrame *out_frame);
void player_controller_unlock_frame(PlayerController *pc);

// Update routine to be called once per frame from the UI loop
void player_controller_update(PlayerController *pc);

// Subsystem references
Playlist* player_controller_get_playlist(PlayerController *pc);
const ZipArchive* player_controller_get_archive(const PlayerController *pc);

#ifdef __cplusplus
}
#endif

#endif // DIRECTZIP_PLAYER_CONTROLLER_H
