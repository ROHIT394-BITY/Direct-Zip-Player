#ifndef DIRECTZIP_VIDEO_DECODER_H
#define DIRECTZIP_VIDEO_DECODER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "memory_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VideoDecoder VideoDecoder;

typedef struct {
    int width;              // Native stream width
    int height;             // Native stream height
    int native_width;       // Original stream width
    int native_height;      // Original stream height
    char resolution_label[32]; // e.g. "1080p FHD", "720p HD", "4K UHD"
    double duration_sec;
    double fps;
    int time_base_num;
    int time_base_den;
    int has_video;
    int has_audio;
    char video_codec_name[64];
    char audio_codec_name[64];
    int audio_sample_rate;
    int audio_channels;
} VideoInfo;

typedef struct {
    uint8_t *data;          // Backward-compatible pointer to primary plane buffer
    uint8_t *y_plane;       // Y plane pointer
    uint8_t *u_plane;       // U plane pointer
    uint8_t *v_plane;       // V plane pointer
    int y_pitch;            // Y plane linesize
    int u_pitch;            // U plane linesize
    int v_pitch;            // V plane linesize
    int width;              // Native width
    int height;             // Native height
    int linesize;           // Primary linesize
    double pts_sec;         // Exact Presentation Timestamp in seconds
    int is_yuv;             // 1 if YUV420P, 0 if RGB24
} DecodedVideoFrame;

typedef struct {
    uint8_t *data;          // 16-bit PCM interleaved stereo (44100 Hz, 2 channels)
    int nb_samples;
    int data_size;
    double pts_sec;         // Exact Presentation Timestamp in seconds
} DecodedAudioFrame;

// Initialize video decoder from an in-memory buffer (zero disk writes)
VideoDecoder* video_decoder_open_memory(const MemoryBuffer *buffer);

// Close and release all FFmpeg contexts and buffers
void video_decoder_close(VideoDecoder *decoder);

// Get video and stream information
VideoInfo video_decoder_get_info(const VideoDecoder *decoder);

// Read and decode the next video frame in presentation order.
// Returns true if a frame was decoded, false on EOF/error.
bool video_decoder_read_video_frame(VideoDecoder *decoder, DecodedVideoFrame *out_frame);

// Read and decode the next audio frame.
// Returns true if a frame was decoded, false on EOF/error.
bool video_decoder_read_audio_frame(VideoDecoder *decoder, DecodedAudioFrame *out_frame);

// Seek to a target timestamp in seconds
bool video_decoder_seek(VideoDecoder *decoder, double target_sec);

// Free frame data allocated during read
void video_decoder_free_video_frame(DecodedVideoFrame *frame);
void video_decoder_free_audio_frame(DecodedAudioFrame *frame);

#ifdef __cplusplus
}
#endif

#endif // DIRECTZIP_VIDEO_DECODER_H
