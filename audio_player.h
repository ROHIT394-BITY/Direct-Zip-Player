#ifndef DIRECTZIP_AUDIO_PLAYER_H
#define DIRECTZIP_AUDIO_PLAYER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AudioPlayer AudioPlayer;

// Initialize audio output with SDL2 (44100Hz, 16-bit stereo)
AudioPlayer* audio_player_init(void);

// Queue decoded 16-bit stereo PCM audio samples
bool audio_player_queue(AudioPlayer *player, const uint8_t *pcm_data, size_t size);

// Queue decoded audio with presentation timestamp for hardware clock synchronization
bool audio_player_queue_pts(AudioPlayer *player, const uint8_t *pcm_data, size_t size, double pts_sec, int nb_samples);

// Clear queued audio immediately (called during seek)
void audio_player_clear(AudioPlayer *player);

// Playback state control
void audio_player_pause(AudioPlayer *player);
void audio_player_resume(AudioPlayer *player);
bool audio_player_is_paused(const AudioPlayer *player);

// Volume control: 0.0f to 1.0f
void audio_player_set_volume(AudioPlayer *player, float volume);
float audio_player_get_volume(const AudioPlayer *player);

// Mute toggle
void audio_player_set_muted(AudioPlayer *player, bool muted);
bool audio_player_is_muted(const AudioPlayer *player);

// Get current amount of audio data queued
size_t audio_player_get_queued_bytes(const AudioPlayer *player);
double audio_player_get_queued_seconds(const AudioPlayer *player);

// Hardware audio playback clock
double audio_player_get_clock(const AudioPlayer *player);
bool audio_player_has_clock(const AudioPlayer *player);
void audio_player_set_clock(AudioPlayer *player, double pts_sec);

// Shutdown audio output
void audio_player_shutdown(AudioPlayer *player);

#ifdef __cplusplus
}
#endif

#endif // DIRECTZIP_AUDIO_PLAYER_H
