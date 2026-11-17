#define SDL_MAIN_HANDLED
#include "audio_player.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct AudioPlayer {
    SDL_AudioDeviceID dev_id;
    SDL_AudioSpec spec;
    float volume;
    bool is_muted;
    bool is_paused;
    double latest_queued_pts;
    double latest_frame_duration;
    bool has_clock;
};

AudioPlayer* audio_player_init(void) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "[AudioPlayer] Failed to init SDL audio subsystem: %s\n", SDL_GetError());
        return NULL;
    }

    AudioPlayer *player = (AudioPlayer *)calloc(1, sizeof(AudioPlayer));
    if (!player) return NULL;

    SDL_AudioSpec desired;
    SDL_zero(desired);
    desired.freq = 44100;
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples = 1024;
    desired.callback = NULL; // Using SDL_QueueAudio

    player->dev_id = SDL_OpenAudioDevice(NULL, 0, &desired, &player->spec, 0);
    if (player->dev_id == 0) {
        fprintf(stderr, "[AudioPlayer] Failed to open audio device: %s\n", SDL_GetError());
        free(player);
        return NULL;
    }

    player->volume = 0.8f;
    player->is_muted = false;
    player->is_paused = false;
    player->latest_queued_pts = 0.0;
    player->latest_frame_duration = 0.0;
    player->has_clock = false;

    // Start audio device in playing state (it waits for queued data)
    SDL_PauseAudioDevice(player->dev_id, 0);

    return player;
}

bool audio_player_queue(AudioPlayer *player, const uint8_t *pcm_data, size_t size) {
    // Default fallback when PTS is omitted
    return audio_player_queue_pts(player, pcm_data, size, player ? player->latest_queued_pts : 0.0, (int)(size / 4));
}

bool audio_player_queue_pts(AudioPlayer *player, const uint8_t *pcm_data, size_t size, double pts_sec, int nb_samples) {
    if (!player || player->dev_id == 0 || !pcm_data || size == 0) return false;

    float effective_vol = player->is_muted ? 0.0f : player->volume;
    
    uint8_t *temp = (uint8_t *)malloc(size);
    if (!temp) return false;
    memcpy(temp, pcm_data, size);

    if (effective_vol < 0.999f) {
        int16_t *samples = (int16_t *)temp;
        size_t count = size / sizeof(int16_t);
        for (size_t i = 0; i < count; i++) {
            int32_t v = (int32_t)(samples[i] * effective_vol);
            if (v > 32767) v = 32767;
            else if (v < -32768) v = -32768;
            samples[i] = (int16_t)v;
        }
    }

    int res = SDL_QueueAudio(player->dev_id, temp, (Uint32)size);
    free(temp);

    if (res == 0) {
        player->latest_queued_pts = pts_sec;
        player->latest_frame_duration = (double)nb_samples / (double)player->spec.freq;
        player->has_clock = true;
        return true;
    }
    return false;
}

void audio_player_clear(AudioPlayer *player) {
    if (!player || player->dev_id == 0) return;
    SDL_ClearQueuedAudio(player->dev_id);
    player->has_clock = false;
    player->latest_queued_pts = 0.0;
    player->latest_frame_duration = 0.0;
}

void audio_player_pause(AudioPlayer *player) {
    if (!player || player->dev_id == 0) return;
    player->is_paused = true;
    SDL_PauseAudioDevice(player->dev_id, 1);
}

void audio_player_resume(AudioPlayer *player) {
    if (!player || player->dev_id == 0) return;
    player->is_paused = false;
    SDL_PauseAudioDevice(player->dev_id, 0);
}

bool audio_player_is_paused(const AudioPlayer *player) {
    return player ? player->is_paused : true;
}

void audio_player_set_volume(AudioPlayer *player, float volume) {
    if (!player) return;
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    player->volume = volume;
}

float audio_player_get_volume(const AudioPlayer *player) {
    return player ? player->volume : 0.0f;
}

void audio_player_set_muted(AudioPlayer *player, bool muted) {
    if (!player) return;
    player->is_muted = muted;
}

bool audio_player_is_muted(const AudioPlayer *player) {
    return player ? player->is_muted : false;
}

size_t audio_player_get_queued_bytes(const AudioPlayer *player) {
    if (!player || player->dev_id == 0) return 0;
    return (size_t)SDL_GetQueuedAudioSize(player->dev_id);
}

double audio_player_get_queued_seconds(const AudioPlayer *player) {
    if (!player || player->dev_id == 0) return 0.0;
    // 44100 Hz, 2 channels, 2 bytes per sample = 176400 bytes/sec
    Uint32 bytes = SDL_GetQueuedAudioSize(player->dev_id);
    return (double)bytes / (44100.0 * 2.0 * 2.0);
}

double audio_player_get_clock(const AudioPlayer *player) {
    if (!player || !player->has_clock || player->dev_id == 0) return 0.0;
    double queued_sec = audio_player_get_queued_seconds(player);
    double clk = (player->latest_queued_pts + player->latest_frame_duration) - queued_sec;
    return clk > 0.0 ? clk : 0.0;
}

bool audio_player_has_clock(const AudioPlayer *player) {
    return (player && player->has_clock && player->dev_id != 0);
}

void audio_player_set_clock(AudioPlayer *player, double pts_sec) {
    if (!player) return;
    player->latest_queued_pts = pts_sec;
    player->latest_frame_duration = 0.0;
    player->has_clock = true;
}

void audio_player_shutdown(AudioPlayer *player) {
    if (!player) return;
    if (player->dev_id != 0) {
        SDL_CloseAudioDevice(player->dev_id);
        player->dev_id = 0;
    }
    free(player);
}
