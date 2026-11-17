#define SDL_MAIN_HANDLED
#include "player_controller.h"
#include "threading.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    PlayerController *pc;
    int track_index;
} DecompressTask;

struct PlayerController {
    ZipArchive *archive;
    Playlist *playlist;
    AudioPlayer *audio_player;

    MemoryBuffer *current_buffer;
    VideoDecoder *decoder;

    PlayerState state;
    char status_text[256];
    float decompression_progress;

    dz_mutex_t state_mutex;
    dz_thread_t worker_thread;
    bool worker_running;

    // Clocks
    double current_time_sec;
    double duration_sec;
    double audio_clock;
    double video_clock;
    float playback_speed;
    Uint64 last_perf_counter;
    Uint64 perf_frequency;

    // Video metadata
    int video_width;
    int video_height;
    char resolution_label[32];

    // Real-Time 1080p Video Upscaler
    VideoUpscaler *upscaler;

    // Frame Presentation Queue & Synchronization
    dz_mutex_t frame_mutex;
    DecodedVideoFrame current_frame;
    bool has_current_frame;
    bool has_new_frame;

    DecodedVideoFrame next_frame;
    bool has_next_frame;

    // FPS Measurement
    Uint64 fps_window_start;
    int fps_frame_count;
    double measured_fps;

    // Diagnostics
    PlayerDiagnostics diagnostics;

    // Seek request
    bool seek_requested;
    double seek_target_sec;
};

static void decompression_progress_cb(float progress, const char *status, void *user_data) {
    PlayerController *pc = (PlayerController *)user_data;
    if (!pc) return;

    dz_mutex_lock(&pc->state_mutex);
    pc->decompression_progress = progress;
    if (status) {
        snprintf(pc->status_text, sizeof(pc->status_text), "%s (%.0f%%)", status, progress * 100.0f);
    }
    dz_mutex_unlock(&pc->state_mutex);
}

static void decompress_worker_thread(void *arg) {
    DecompressTask *task = (DecompressTask *)arg;
    PlayerController *pc = task->pc;
    int track_idx = task->track_index;
    free(task);

    const ZipEntry *entry = zip_engine_get_mp4_entry(pc->archive, track_idx);
    if (!entry) {
        dz_mutex_lock(&pc->state_mutex);
        pc->state = PLAYER_STATE_ERROR;
        snprintf(pc->status_text, sizeof(pc->status_text), "Error: MP4 entry not found in archive");
        pc->worker_running = false;
        dz_mutex_unlock(&pc->state_mutex);
        return;
    }

    // In-memory decompression directly to RAM
    MemoryBuffer *new_buffer = zip_engine_decompress_entry(
        pc->archive, entry, decompression_progress_cb, pc
    );

    dz_mutex_lock(&pc->state_mutex);

    if (!new_buffer) {
        pc->state = PLAYER_STATE_ERROR;
        snprintf(pc->status_text, sizeof(pc->status_text), "Decompression failed (insufficient RAM or corrupt stream)");
        pc->worker_running = false;
        dz_mutex_unlock(&pc->state_mutex);
        return;
    }

    // Clean up previous decoder and frames
    if (pc->has_next_frame) {
        video_decoder_free_video_frame(&pc->next_frame);
        pc->has_next_frame = false;
    }

    dz_mutex_lock(&pc->frame_mutex);
    if (pc->has_current_frame) {
        video_decoder_free_video_frame(&pc->current_frame);
        pc->has_current_frame = false;
    }
    dz_mutex_unlock(&pc->frame_mutex);

    if (pc->decoder) {
        video_decoder_close(pc->decoder);
        pc->decoder = NULL;
    }
    if (pc->current_buffer) {
        memory_buffer_free(pc->current_buffer);
        pc->current_buffer = NULL;
    }

    pc->current_buffer = new_buffer;

    // Open video decoder with in-memory AVIOContext
    pc->decoder = video_decoder_open_memory(pc->current_buffer);
    if (!pc->decoder) {
        pc->state = PLAYER_STATE_ERROR;
        snprintf(pc->status_text, sizeof(pc->status_text), "FFmpeg failed to parse in-memory MP4 container");
        pc->worker_running = false;
        dz_mutex_unlock(&pc->state_mutex);
        return;
    }

    VideoInfo info = video_decoder_get_info(pc->decoder);
    pc->video_width = info.width;
    pc->video_height = info.height;
    strncpy(pc->resolution_label, info.resolution_label, sizeof(pc->resolution_label) - 1);
    pc->duration_sec = info.duration_sec;
    pc->current_time_sec = 0.0;
    pc->audio_clock = 0.0;
    pc->video_clock = 0.0;
    pc->last_perf_counter = SDL_GetPerformanceCounter();
    pc->fps_window_start = pc->last_perf_counter;
    pc->fps_frame_count = 0;
    pc->measured_fps = info.fps;

    // Initialize diagnostics
    pc->diagnostics.native_width = info.native_width;
    pc->diagnostics.native_height = info.native_height;
    pc->diagnostics.original_fps = info.fps;
    pc->diagnostics.current_fps = info.fps;
    pc->diagnostics.time_base_num = info.time_base_num;
    pc->diagnostics.time_base_den = info.time_base_den;
    pc->diagnostics.playback_speed = pc->playback_speed;
    pc->diagnostics.decoded_frames = 0;
    pc->diagnostics.dropped_frames = 0;

    playlist_set_item_duration(pc->playlist, track_idx, info.duration_sec);

    if (pc->audio_player) {
        audio_player_clear(pc->audio_player);
        audio_player_resume(pc->audio_player);
    }

    // Pre-decode the first video frame so it's ready immediately
    if (video_decoder_read_video_frame(pc->decoder, &pc->next_frame)) {
        pc->has_next_frame = true;
        pc->diagnostics.decoded_frames++;
    }

    pc->state = PLAYER_STATE_PLAYING;
    snprintf(pc->status_text, sizeof(pc->status_text), "Playing: %s", entry->display_name);
    pc->worker_running = false;

    dz_mutex_unlock(&pc->state_mutex);
}

PlayerController* player_controller_create(void) {
    PlayerController *pc = (PlayerController *)calloc(1, sizeof(PlayerController));
    if (!pc) return NULL;

    dz_mutex_init(&pc->state_mutex);
    dz_mutex_init(&pc->frame_mutex);

    pc->playlist = playlist_create();
    pc->audio_player = audio_player_init();

    pc->state = PLAYER_STATE_IDLE;
    snprintf(pc->status_text, sizeof(pc->status_text), "Ready. Drag & drop or Open a ZIP archive.");

    pc->upscaler = video_upscaler_create();

    pc->playback_speed = 1.0f;
    pc->perf_frequency = SDL_GetPerformanceFrequency();
    pc->last_perf_counter = SDL_GetPerformanceCounter();
    pc->fps_window_start = pc->last_perf_counter;
    pc->diagnostics.playback_speed = 1.0f;

    return pc;
}

void player_controller_destroy(PlayerController *pc) {
    if (!pc) return;

    player_controller_close_zip(pc);

    if (pc->upscaler) {
        video_upscaler_destroy(pc->upscaler);
        pc->upscaler = NULL;
    }

    if (pc->audio_player) {
        audio_player_shutdown(pc->audio_player);
        pc->audio_player = NULL;
    }

    if (pc->playlist) {
        playlist_destroy(pc->playlist);
        pc->playlist = NULL;
    }

    dz_mutex_destroy(&pc->state_mutex);
    dz_mutex_destroy(&pc->frame_mutex);

    free(pc);
}

bool player_controller_open_zip(PlayerController *pc, const char *zip_path) {
    if (!pc || !zip_path) return false;

    player_controller_close_zip(pc);

    dz_mutex_lock(&pc->state_mutex);
    pc->archive = zip_engine_open(zip_path);
    if (!pc->archive) {
        pc->state = PLAYER_STATE_ERROR;
        snprintf(pc->status_text, sizeof(pc->status_text), "Failed to open or parse ZIP archive: %s", zip_path);
        dz_mutex_unlock(&pc->state_mutex);
        return false;
    }

    int mp4_count = zip_engine_get_mp4_count(pc->archive);
    if (mp4_count == 0) {
        pc->state = PLAYER_STATE_ERROR;
        snprintf(pc->status_text, sizeof(pc->status_text), "Archive opened, but 0 MP4 files were found");
        dz_mutex_unlock(&pc->state_mutex);
        return false;
    }

    playlist_load_archive(pc->playlist, pc->archive);
    snprintf(pc->status_text, sizeof(pc->status_text), "Archive loaded: %d MP4 videos identified", mp4_count);
    pc->state = PLAYER_STATE_IDLE;
    dz_mutex_unlock(&pc->state_mutex);

    // Auto-load first video
    player_controller_load_track(pc, 0);

    return true;
}

void player_controller_close_zip(PlayerController *pc) {
    if (!pc) return;

    dz_mutex_lock(&pc->state_mutex);
    if (pc->worker_running && pc->worker_thread) {
        dz_thread_join(pc->worker_thread);
        pc->worker_running = false;
    }

    if (pc->audio_player) {
        audio_player_clear(pc->audio_player);
    }

    if (pc->has_next_frame) {
        video_decoder_free_video_frame(&pc->next_frame);
        pc->has_next_frame = false;
    }

    dz_mutex_lock(&pc->frame_mutex);
    if (pc->has_current_frame) {
        video_decoder_free_video_frame(&pc->current_frame);
        pc->has_current_frame = false;
    }
    dz_mutex_unlock(&pc->frame_mutex);

    if (pc->decoder) {
        video_decoder_close(pc->decoder);
        pc->decoder = NULL;
    }

    if (pc->current_buffer) {
        memory_buffer_free(pc->current_buffer);
        pc->current_buffer = NULL;
    }

    if (pc->archive) {
        zip_engine_close(pc->archive);
        pc->archive = NULL;
    }

    if (pc->playlist) {
        playlist_clear(pc->playlist);
    }

    pc->state = PLAYER_STATE_IDLE;
    pc->current_time_sec = 0.0;
    pc->duration_sec = 0.0;
    pc->audio_clock = 0.0;
    pc->video_clock = 0.0;
    snprintf(pc->status_text, sizeof(pc->status_text), "No archive loaded.");
    dz_mutex_unlock(&pc->state_mutex);
}

bool player_controller_load_track(PlayerController *pc, int playlist_index) {
    if (!pc || !pc->archive || !pc->playlist) return false;
    if (playlist_index < 0 || playlist_index >= playlist_get_count(pc->playlist)) return false;

    dz_mutex_lock(&pc->state_mutex);

    if (pc->worker_running) {
        dz_mutex_unlock(&pc->state_mutex);
        dz_thread_join(pc->worker_thread);
        dz_mutex_lock(&pc->state_mutex);
    }

    playlist_select_index(pc->playlist, playlist_index);
    const PlaylistItem *item = playlist_get_item(pc->playlist, playlist_index);

    pc->state = PLAYER_STATE_DECOMPRESSING;
    pc->decompression_progress = 0.0f;
    snprintf(pc->status_text, sizeof(pc->status_text), "Decompressing %s into RAM...", item ? item->display_name : "video");

    DecompressTask *task = (DecompressTask *)malloc(sizeof(DecompressTask));
    task->pc = pc;
    task->track_index = playlist_index;

    pc->worker_running = true;
    if (!dz_thread_create(&pc->worker_thread, decompress_worker_thread, task)) {
        free(task);
        pc->worker_running = false;
        pc->state = PLAYER_STATE_ERROR;
        snprintf(pc->status_text, sizeof(pc->status_text), "Failed to spawn decompression worker thread");
        dz_mutex_unlock(&pc->state_mutex);
        return false;
    }

    dz_mutex_unlock(&pc->state_mutex);
    return true;
}

void player_controller_play(PlayerController *pc) {
    if (!pc) return;
    dz_mutex_lock(&pc->state_mutex);
    if (pc->state == PLAYER_STATE_PAUSED || pc->state == PLAYER_STATE_STOPPED) {
        pc->state = PLAYER_STATE_PLAYING;
        pc->last_perf_counter = SDL_GetPerformanceCounter();
        if (pc->audio_player) audio_player_resume(pc->audio_player);
    }
    dz_mutex_unlock(&pc->state_mutex);
}

void player_controller_pause(PlayerController *pc) {
    if (!pc) return;
    dz_mutex_lock(&pc->state_mutex);
    if (pc->state == PLAYER_STATE_PLAYING) {
        pc->state = PLAYER_STATE_PAUSED;
        if (pc->audio_player) audio_player_pause(pc->audio_player);
    }
    dz_mutex_unlock(&pc->state_mutex);
}

void player_controller_toggle_play_pause(PlayerController *pc) {
    if (!pc) return;
    if (player_controller_get_state(pc) == PLAYER_STATE_PLAYING) {
        player_controller_pause(pc);
    } else {
        player_controller_play(pc);
    }
}

void player_controller_stop(PlayerController *pc) {
    if (!pc) return;
    dz_mutex_lock(&pc->state_mutex);
    if (pc->decoder) {
        video_decoder_seek(pc->decoder, 0.0);
    }
    if (pc->audio_player) {
        audio_player_clear(pc->audio_player);
        audio_player_pause(pc->audio_player);
    }
    pc->current_time_sec = 0.0;
    pc->audio_clock = 0.0;
    pc->video_clock = 0.0;
    pc->state = PLAYER_STATE_STOPPED;
    dz_mutex_unlock(&pc->state_mutex);
}

bool player_controller_next(PlayerController *pc) {
    if (!pc || !pc->playlist) return false;
    int next_idx = playlist_get_selected_index(pc->playlist) + 1;
    if (next_idx < playlist_get_count(pc->playlist)) {
        return player_controller_load_track(pc, next_idx);
    }
    return false;
}

bool player_controller_prev(PlayerController *pc) {
    if (!pc || !pc->playlist) return false;
    int prev_idx = playlist_get_selected_index(pc->playlist) - 1;
    if (prev_idx >= 0) {
        return player_controller_load_track(pc, prev_idx);
    }
    return false;
}

void player_controller_seek_to(PlayerController *pc, double target_sec) {
    if (!pc) return;
    dz_mutex_lock(&pc->state_mutex);
    pc->seek_requested = true;
    pc->seek_target_sec = target_sec;
    dz_mutex_unlock(&pc->state_mutex);
}

void player_controller_seek_relative(PlayerController *pc, double delta_sec) {
    if (!pc) return;
    double current = player_controller_get_current_time(pc);
    player_controller_seek_to(pc, current + delta_sec);
}

void player_controller_set_volume(PlayerController *pc, float volume) {
    if (!pc || !pc->audio_player) return;
    audio_player_set_volume(pc->audio_player, volume);
}

float player_controller_get_volume(const PlayerController *pc) {
    if (!pc || !pc->audio_player) return 0.8f;
    return audio_player_get_volume(pc->audio_player);
}

void player_controller_toggle_mute(PlayerController *pc) {
    if (!pc || !pc->audio_player) return;
    audio_player_set_muted(pc->audio_player, !audio_player_is_muted(pc->audio_player));
}

bool player_controller_is_muted(const PlayerController *pc) {
    if (!pc || !pc->audio_player) return false;
    return audio_player_is_muted(pc->audio_player);
}

PlayerState player_controller_get_state(const PlayerController *pc) {
    if (!pc) return PLAYER_STATE_IDLE;
    return pc->state;
}

const char* player_controller_get_status_text(const PlayerController *pc) {
    return pc ? pc->status_text : "";
}

float player_controller_get_decompression_progress(const PlayerController *pc) {
    return pc ? pc->decompression_progress : 0.0f;
}

double player_controller_get_current_time(const PlayerController *pc) {
    return pc ? pc->current_time_sec : 0.0;
}

double player_controller_get_duration(const PlayerController *pc) {
    return pc ? pc->duration_sec : 0.0;
}

void player_controller_get_video_size(const PlayerController *pc, int *width, int *height) {
    if (!pc) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }
    VideoUpscalerInfo u_info = video_upscaler_get_info(pc->upscaler);
    if (u_info.is_active && u_info.output_width > 0 && u_info.output_height > 0) {
        if (width) *width = u_info.output_width;
        if (height) *height = u_info.output_height;
    } else {
        if (width) *width = pc->video_width;
        if (height) *height = pc->video_height;
    }
}

const char* player_controller_get_resolution_label(const PlayerController *pc) {
    if (!pc) return "1080p FHD Ready";
    VideoUpscalerInfo u_info = video_upscaler_get_info(pc->upscaler);
    if (u_info.is_active) {
        if (u_info.output_width >= 1920 || u_info.output_height >= 1080) {
            return "1080p FHD Upscaled";
        }
        return "Upscaled HD";
    }
    return (pc->resolution_label[0]) ? pc->resolution_label : "1080p FHD";
}

void player_controller_set_upscaling_mode(PlayerController *pc, UpscalingMode mode) {
    if (!pc || !pc->upscaler) return;
    dz_mutex_lock(&pc->state_mutex);
    video_upscaler_set_mode(pc->upscaler, mode);
    dz_mutex_unlock(&pc->state_mutex);
}

UpscalingMode player_controller_get_upscaling_mode(const PlayerController *pc) {
    if (!pc || !pc->upscaler) return UPSCALING_MODE_AUTO_1080P;
    return video_upscaler_get_mode(pc->upscaler);
}

void player_controller_set_upscaling_quality(PlayerController *pc, UpscalingQuality quality) {
    if (!pc || !pc->upscaler) return;
    dz_mutex_lock(&pc->state_mutex);
    video_upscaler_set_quality(pc->upscaler, quality);
    dz_mutex_unlock(&pc->state_mutex);
}

UpscalingQuality player_controller_get_upscaling_quality(const PlayerController *pc) {
    if (!pc || !pc->upscaler) return UPSCALING_QUALITY_HIGH;
    return video_upscaler_get_quality(pc->upscaler);
}

VideoUpscalerInfo player_controller_get_upscaler_info(const PlayerController *pc) {
    if (!pc || !pc->upscaler) {
        VideoUpscalerInfo zero = {0};
        return zero;
    }
    return video_upscaler_get_info(pc->upscaler);
}

void player_controller_toggle_diagnostics(PlayerController *pc) {
    if (!pc) return;
    dz_mutex_lock(&pc->state_mutex);
    pc->diagnostics.diagnostics_enabled = !pc->diagnostics.diagnostics_enabled;
    dz_mutex_unlock(&pc->state_mutex);
}

bool player_controller_is_diagnostics_enabled(const PlayerController *pc) {
    return pc ? pc->diagnostics.diagnostics_enabled : false;
}

PlayerDiagnostics player_controller_get_diagnostics(const PlayerController *pc) {
    if (!pc) {
        PlayerDiagnostics zero = {0};
        zero.playback_speed = 1.0f;
        return zero;
    }
    dz_mutex_lock((dz_mutex_t *)&pc->state_mutex);
    PlayerDiagnostics diag = pc->diagnostics;
    dz_mutex_unlock((dz_mutex_t *)&pc->state_mutex);
    return diag;
}

bool player_controller_lock_current_frame(PlayerController *pc, const uint8_t **out_rgb_data, int *width, int *height, int *linesize) {
    if (!pc) return false;
    dz_mutex_lock(&pc->frame_mutex);
    if (!pc->has_current_frame || !pc->current_frame.data) {
        dz_mutex_unlock(&pc->frame_mutex);
        return false;
    }
    if (out_rgb_data) *out_rgb_data = pc->current_frame.data;
    if (width) *width = pc->current_frame.width;
    if (height) *height = pc->current_frame.height;
    if (linesize) *linesize = pc->current_frame.linesize;
    return true;
}

bool player_controller_lock_video_frame(PlayerController *pc, DecodedVideoFrame *out_frame) {
    if (!pc || !out_frame) return false;
    dz_mutex_lock(&pc->frame_mutex);
    if (!pc->has_current_frame || !pc->current_frame.data) {
        dz_mutex_unlock(&pc->frame_mutex);
        return false;
    }
    *out_frame = pc->current_frame;
    return true;
}

void player_controller_unlock_frame(PlayerController *pc) {
    if (!pc) return;
    dz_mutex_unlock(&pc->frame_mutex);
}

void player_controller_update(PlayerController *pc) {
    if (!pc) return;

    dz_mutex_lock(&pc->state_mutex);

    if (pc->state != PLAYER_STATE_PLAYING && !pc->seek_requested) {
        pc->last_perf_counter = SDL_GetPerformanceCounter();
        dz_mutex_unlock(&pc->state_mutex);
        return;
    }

    if (!pc->decoder) {
        dz_mutex_unlock(&pc->state_mutex);
        return;
    }

    // Handle pending seek request
    if (pc->seek_requested) {
        double target = pc->seek_target_sec;
        pc->seek_requested = false;

        video_decoder_seek(pc->decoder, target);
        if (pc->audio_player) {
            audio_player_clear(pc->audio_player);
            audio_player_set_clock(pc->audio_player, target);
        }

        if (pc->has_next_frame) {
            video_decoder_free_video_frame(&pc->next_frame);
            pc->has_next_frame = false;
        }

        pc->current_time_sec = target;
        pc->video_clock = target;
        pc->audio_clock = target;
        pc->last_perf_counter = SDL_GetPerformanceCounter();
        pc->fps_window_start = pc->last_perf_counter;
        pc->fps_frame_count = 0;

        // Pre-read frame after seek
        if (video_decoder_read_video_frame(pc->decoder, &pc->next_frame)) {
            pc->has_next_frame = true;
            pc->diagnostics.decoded_frames++;

            // Upscale and immediately display the seeked frame
            DecodedVideoFrame display_frame = {0};
            video_upscaler_process(pc->upscaler, &pc->next_frame, &display_frame);

            dz_mutex_lock(&pc->frame_mutex);
            if (pc->has_current_frame) {
                video_decoder_free_video_frame(&pc->current_frame);
            }
            pc->current_frame = display_frame;
            pc->has_current_frame = true;
            pc->has_new_frame = true;
            dz_mutex_unlock(&pc->frame_mutex);

            pc->video_clock = pc->next_frame.pts_sec;
            video_decoder_free_video_frame(&pc->next_frame);
            pc->has_next_frame = false;

            // Fetch following frame
            if (video_decoder_read_video_frame(pc->decoder, &pc->next_frame)) {
                pc->has_next_frame = true;
                pc->diagnostics.decoded_frames++;
            }
        }
        dz_mutex_unlock(&pc->state_mutex);
        return;
    }

    // ==========================================
    // 1. Audio Feeding & Audio Master Clock
    // ==========================================
    VideoInfo v_info = video_decoder_get_info(pc->decoder);
    bool has_audio_stream = (v_info.has_audio == 1);

    if (has_audio_stream && pc->audio_player) {
        // Buffer up to 0.35 seconds of audio to avoid underrun
        while (audio_player_get_queued_seconds(pc->audio_player) < 0.35) {
            DecodedAudioFrame a_frame = {0};
            if (video_decoder_read_audio_frame(pc->decoder, &a_frame)) {
                audio_player_queue_pts(
                    pc->audio_player,
                    a_frame.data,
                    (size_t)a_frame.data_size,
                    a_frame.pts_sec,
                    a_frame.nb_samples
                );
                video_decoder_free_audio_frame(&a_frame);
            } else {
                break;
            }
        }
    }

    // ==========================================
    // 2. Master Clock Calculation
    // ==========================================
    Uint64 now = SDL_GetPerformanceCounter();
    double master_clock = 0.0;

    if (has_audio_stream && pc->audio_player && audio_player_has_clock(pc->audio_player)) {
        // Use audio as master clock
        master_clock = audio_player_get_clock(pc->audio_player);
        pc->audio_clock = master_clock;
        pc->diagnostics.is_audio_master = true;
        pc->last_perf_counter = now;
    } else {
        // Use high-precision video clock when audio is absent
        double dt = (double)(now - pc->last_perf_counter) / (double)pc->perf_frequency;
        pc->last_perf_counter = now;
        if (dt > 0.05) dt = 0.05; // Guard against stall

        pc->video_clock += dt * (double)pc->playback_speed;
        master_clock = pc->video_clock;
        pc->audio_clock = 0.0;
        pc->diagnostics.is_audio_master = false;
    }

    pc->current_time_sec = master_clock;

    // Check end of video duration
    if (pc->duration_sec > 0.0 && pc->current_time_sec >= pc->duration_sec) {
        if (!player_controller_next(pc)) {
            pc->current_time_sec = pc->duration_sec;
            pc->state = PLAYER_STATE_PAUSED;
        }
        dz_mutex_unlock(&pc->state_mutex);
        return;
    }

    // ==========================================
    // 3. Pre-Fetch Next Video Frame
    // ==========================================
    if (!pc->has_next_frame) {
        if (video_decoder_read_video_frame(pc->decoder, &pc->next_frame)) {
            pc->has_next_frame = true;
            pc->diagnostics.decoded_frames++;
        }
    }

    // ==========================================
    // 4. Timestamp-Based Frame Presentation
    // ==========================================
    if (pc->has_next_frame) {
        double frame_pts = pc->next_frame.pts_sec;
        double diff = master_clock - frame_pts;

        if (diff >= -0.005) {
            // It is time to display this frame!
            // Check if frame is severely lagging (>120ms behind master clock)
            if (diff > 0.120) {
                // Video is falling behind: drop this stale frame and advance
                video_decoder_free_video_frame(&pc->next_frame);
                pc->has_next_frame = false;
                pc->diagnostics.dropped_frames++;

                // Pre-fetch following frame
                if (video_decoder_read_video_frame(pc->decoder, &pc->next_frame)) {
                    pc->has_next_frame = true;
                    pc->diagnostics.decoded_frames++;
                }
            } else {
                // Present frame with high-quality upscaling
                DecodedVideoFrame display_frame = {0};
                video_upscaler_process(pc->upscaler, &pc->next_frame, &display_frame);

                dz_mutex_lock(&pc->frame_mutex);
                if (pc->has_current_frame) {
                    video_decoder_free_video_frame(&pc->current_frame);
                }
                pc->current_frame = display_frame;
                pc->has_current_frame = true;
                pc->has_new_frame = true;
                dz_mutex_unlock(&pc->frame_mutex);

                pc->video_clock = frame_pts;
                video_decoder_free_video_frame(&pc->next_frame);
                pc->has_next_frame = false;
                pc->fps_frame_count++;

                // Measure real-time playback FPS over a 1-second rolling window
                if ((now - pc->fps_window_start) >= pc->perf_frequency) {
                    double elapsed = (double)(now - pc->fps_window_start) / (double)pc->perf_frequency;
                    pc->measured_fps = (double)pc->fps_frame_count / elapsed;
                    pc->fps_frame_count = 0;
                    pc->fps_window_start = now;
                }

                // Immediately buffer the following frame for next presentation tick
                if (video_decoder_read_video_frame(pc->decoder, &pc->next_frame)) {
                    pc->has_next_frame = true;
                    pc->diagnostics.decoded_frames++;
                }
            }
        }
        // Else: frame_pts > master_clock.
        // It is not time yet. Keep displaying current frame.
    }

    // ==========================================
    // 5. Update Diagnostics Snapshot
    // ==========================================
    pc->diagnostics.current_timestamp = master_clock;
    pc->diagnostics.audio_clock = pc->audio_clock;
    pc->diagnostics.video_clock = pc->video_clock;
    if (has_audio_stream && pc->audio_clock > 0.0) {
        pc->diagnostics.av_sync_diff_ms = (pc->video_clock - pc->audio_clock) * 1000.0;
    } else {
        pc->diagnostics.av_sync_diff_ms = 0.0;
    }
    pc->diagnostics.current_fps = pc->measured_fps;
    pc->diagnostics.playback_speed = pc->playback_speed;

    // Update real-time upscaler telemetry
    VideoUpscalerInfo u_info = video_upscaler_get_info(pc->upscaler);
    pc->diagnostics.output_width = u_info.output_width;
    pc->diagnostics.output_height = u_info.output_height;
    pc->diagnostics.upscaling_active = u_info.is_active;
    pc->diagnostics.scale_factor = u_info.scale_factor;
    snprintf(pc->diagnostics.upscaling_algorithm, sizeof(pc->diagnostics.upscaling_algorithm), "%s", u_info.algorithm_name);
    snprintf(pc->diagnostics.upscaling_mode_name, sizeof(pc->diagnostics.upscaling_mode_name), "%s", u_info.mode_name);

    dz_mutex_unlock(&pc->state_mutex);
}

Playlist* player_controller_get_playlist(PlayerController *pc) {
    return pc ? pc->playlist : NULL;
}

const ZipArchive* player_controller_get_archive(const PlayerController *pc) {
    return pc ? pc->archive : NULL;
}
