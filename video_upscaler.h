#ifndef DIRECTZIP_VIDEO_UPSCALER_H
#define DIRECTZIP_VIDEO_UPSCALER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "video_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UPSCALING_MODE_AUTO_1080P = 0,  // Automatically upscales videos < 1080p to fit in 1920x1080 (Default)
    UPSCALING_MODE_ORIGINAL,        // Original native resolution (Bypass upscaling)
    UPSCALING_MODE_BEST_FIT         // Automatic best fit to target boundary
} UpscalingMode;

typedef enum {
    UPSCALING_QUALITY_PERFORMANCE = 0, // Fast bilinear scaling
    UPSCALING_QUALITY_BALANCED,        // Bicubic scaling
    UPSCALING_QUALITY_HIGH             // Lanczos scaling (Default, maximum sharpness)
} UpscalingQuality;

typedef struct {
    int original_width;
    int original_height;
    int output_width;
    int output_height;
    bool is_active;
    UpscalingMode mode;
    UpscalingQuality quality;
    char algorithm_name[32];   // "LANCZOS", "BICUBIC", "BILINEAR"
    char mode_name[32];        // "Automatic 1080p", "Original", "Best Fit"
    double scale_factor;       // e.g. 1.50x
} VideoUpscalerInfo;

typedef struct VideoUpscaler VideoUpscaler;

// Create and initialize VideoUpscaler with default settings (Auto 1080p + High Quality Lanczos)
VideoUpscaler* video_upscaler_create(void);

// Destroy VideoUpscaler and release all SwsContexts and buffers
void video_upscaler_destroy(VideoUpscaler *upscaler);

// Configuration setters & getters
void video_upscaler_set_mode(VideoUpscaler *upscaler, UpscalingMode mode);
UpscalingMode video_upscaler_get_mode(const VideoUpscaler *upscaler);

void video_upscaler_set_quality(VideoUpscaler *upscaler, UpscalingQuality quality);
UpscalingQuality video_upscaler_get_quality(const VideoUpscaler *upscaler);

// Target boundary for Best Fit mode (defaults to 1920x1080)
void video_upscaler_set_target_boundary(VideoUpscaler *upscaler, int max_w, int max_h);

// Calculate output dimensions preserving aspect ratio without distortion
void video_upscaler_calculate_target_size(
    UpscalingMode mode, int in_w, int in_h, int target_bound_w, int target_bound_h,
    int *out_w, int *out_h, bool *out_active
);

// Upscale a decoded video frame.
// If upscaling is inactive, produces a direct copy of src_frame.
// If upscaling is active, executes high-quality libswscale Lanczos/Bicubic/Bilinear scaling into out_frame.
bool video_upscaler_process(VideoUpscaler *upscaler, const DecodedVideoFrame *src_frame, DecodedVideoFrame *out_frame);

// Query real-time upscaler information
VideoUpscalerInfo video_upscaler_get_info(const VideoUpscaler *upscaler);

// String helpers
const char* video_upscaler_mode_to_string(UpscalingMode mode);
const char* video_upscaler_quality_to_string(UpscalingQuality quality);

// Optional AI Super Resolution Architecture Hook (e.g. ONNX Runtime backend)
typedef enum {
    AI_BACKEND_NONE = 0,
    AI_BACKEND_ONNX_RUNTIME,
    AI_BACKEND_CUSTOM
} AIBackendType;

typedef bool (*AIUpscaleCallback)(void *user_data, const DecodedVideoFrame *src, DecodedVideoFrame *dst);

void video_upscaler_register_ai_backend(
    VideoUpscaler *upscaler,
    AIBackendType backend_type,
    const char *model_path,
    AIUpscaleCallback callback,
    void *user_data
);

#ifdef __cplusplus
}
#endif

#endif // DIRECTZIP_VIDEO_UPSCALER_H
