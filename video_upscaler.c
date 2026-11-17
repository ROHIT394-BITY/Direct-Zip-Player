#include "video_upscaler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>

struct VideoUpscaler {
    UpscalingMode mode;
    UpscalingQuality quality;
    int target_bound_w;
    int target_bound_h;

    // Persistent libswscale context
    struct SwsContext *sws_ctx;
    int cached_in_w;
    int cached_in_h;
    int cached_out_w;
    int cached_out_h;
    int cached_sws_flags;

    // Telemetry information
    VideoUpscalerInfo info;

    // Optional AI backend hook
    AIBackendType ai_backend;
    char ai_model_path[256];
    AIUpscaleCallback ai_callback;
    void *ai_user_data;
};

const char* video_upscaler_mode_to_string(UpscalingMode mode) {
    switch (mode) {
        case UPSCALING_MODE_AUTO_1080P: return "Automatic 1080p";
        case UPSCALING_MODE_ORIGINAL:   return "Original Resolution";
        case UPSCALING_MODE_BEST_FIT:   return "Automatic Best Fit";
        default:                        return "Unknown";
    }
}

const char* video_upscaler_quality_to_string(UpscalingQuality quality) {
    switch (quality) {
        case UPSCALING_QUALITY_PERFORMANCE: return "BILINEAR (Performance)";
        case UPSCALING_QUALITY_BALANCED:    return "BICUBIC (Balanced)";
        case UPSCALING_QUALITY_HIGH:        return "LANCZOS (High Quality)";
        default:                            return "LANCZOS";
    }
}

VideoUpscaler* video_upscaler_create(void) {
    VideoUpscaler *upscaler = (VideoUpscaler *)calloc(1, sizeof(VideoUpscaler));
    if (!upscaler) return NULL;

    upscaler->mode = UPSCALING_MODE_AUTO_1080P;
    upscaler->quality = UPSCALING_QUALITY_HIGH;
    upscaler->target_bound_w = 1920;
    upscaler->target_bound_h = 1080;

    upscaler->info.mode = upscaler->mode;
    upscaler->info.quality = upscaler->quality;
    upscaler->info.is_active = false;
    upscaler->info.scale_factor = 1.0;
    snprintf(upscaler->info.mode_name, sizeof(upscaler->info.mode_name), "%s",
             video_upscaler_mode_to_string(upscaler->mode));
    snprintf(upscaler->info.algorithm_name, sizeof(upscaler->info.algorithm_name), "%s",
             video_upscaler_quality_to_string(upscaler->quality));

    return upscaler;
}

void video_upscaler_destroy(VideoUpscaler *upscaler) {
    if (!upscaler) return;

    if (upscaler->sws_ctx) {
        sws_freeContext(upscaler->sws_ctx);
        upscaler->sws_ctx = NULL;
    }

    free(upscaler);
}

void video_upscaler_set_mode(VideoUpscaler *upscaler, UpscalingMode mode) {
    if (!upscaler) return;
    upscaler->mode = mode;
    upscaler->info.mode = mode;
    snprintf(upscaler->info.mode_name, sizeof(upscaler->info.mode_name), "%s",
             video_upscaler_mode_to_string(mode));
}

UpscalingMode video_upscaler_get_mode(const VideoUpscaler *upscaler) {
    return upscaler ? upscaler->mode : UPSCALING_MODE_AUTO_1080P;
}

void video_upscaler_set_quality(VideoUpscaler *upscaler, UpscalingQuality quality) {
    if (!upscaler) return;
    upscaler->quality = quality;
    upscaler->info.quality = quality;
    snprintf(upscaler->info.algorithm_name, sizeof(upscaler->info.algorithm_name), "%s",
             video_upscaler_quality_to_string(quality));
}

UpscalingQuality video_upscaler_get_quality(const VideoUpscaler *upscaler) {
    return upscaler ? upscaler->quality : UPSCALING_QUALITY_HIGH;
}

void video_upscaler_set_target_boundary(VideoUpscaler *upscaler, int max_w, int max_h) {
    if (!upscaler) return;
    upscaler->target_bound_w = max_w > 0 ? max_w : 1920;
    upscaler->target_bound_h = max_h > 0 ? max_h : 1080;
}

void video_upscaler_calculate_target_size(
    UpscalingMode mode, int in_w, int in_h, int target_bound_w, int target_bound_h,
    int *out_w, int *out_h, bool *out_active
) {
    if (!out_w || !out_h || !out_active) return;
    if (in_w <= 0 || in_h <= 0) {
        *out_w = in_w;
        *out_h = in_h;
        *out_active = false;
        return;
    }

    if (target_bound_w <= 0) target_bound_w = 1920;
    if (target_bound_h <= 0) target_bound_h = 1080;

    switch (mode) {
        case UPSCALING_MODE_ORIGINAL:
            *out_w = in_w;
            *out_h = in_h;
            *out_active = false;
            break;

        case UPSCALING_MODE_AUTO_1080P:
            // Check if already 1080p, 1440p, or 4K (2160p)
            if (in_w >= 1920 || in_h >= 1080) {
                *out_w = in_w;
                *out_h = in_h;
                *out_active = false;
            } else {
                // Upscale to fit within 1920x1080 preserving original aspect ratio
                double scale_x = 1920.0 / (double)in_w;
                double scale_y = 1080.0 / (double)in_h;
                double scale = scale_x < scale_y ? scale_x : scale_y;

                int w = (int)round((double)in_w * scale);
                int h = (int)round((double)in_h * scale);

                // Ensure even dimensions for YUV420P Chroma subsampling
                w = (w + 1) & ~1;
                h = (h + 1) & ~1;

                if (w > 1920) w = 1920;
                if (h > 1080) h = 1080;

                *out_w = w;
                *out_h = h;
                *out_active = (w != in_w || h != in_h);
            }
            break;

        case UPSCALING_MODE_BEST_FIT:
            {
                double scale_x = (double)target_bound_w / (double)in_w;
                double scale_y = (double)target_bound_h / (double)in_h;
                double scale = scale_x < scale_y ? scale_x : scale_y;

                int w = (int)round((double)in_w * scale);
                int h = (int)round((double)in_h * scale);

                w = (w + 1) & ~1;
                h = (h + 1) & ~1;

                if (w > target_bound_w) w = target_bound_w;
                if (h > target_bound_h) h = target_bound_h;

                *out_w = w;
                *out_h = h;
                *out_active = (w != in_w || h != in_h);
            }
            break;
    }
}

static bool copy_frame_clone(const DecodedVideoFrame *src, DecodedVideoFrame *dst) {
    if (!src || !dst) return false;
    int w = src->width;
    int h = src->height;
    int y_size = w * h;
    int u_size = (w / 2) * (h / 2);
    int v_size = u_size;
    size_t total = (size_t)(y_size + u_size + v_size);

    uint8_t *buf = (uint8_t *)malloc(total);
    if (!buf) return false;

    dst->data = buf;
    dst->y_plane = buf;
    dst->u_plane = buf + y_size;
    dst->v_plane = buf + y_size + u_size;
    dst->y_pitch = w;
    dst->u_pitch = w / 2;
    dst->v_pitch = w / 2;
    dst->width = w;
    dst->height = h;
    dst->linesize = w;
    dst->is_yuv = src->is_yuv;
    dst->pts_sec = src->pts_sec;

    if (src->y_plane && src->u_plane && src->v_plane) {
        for (int r = 0; r < h; r++) {
            memcpy(dst->y_plane + r * w, src->y_plane + r * src->y_pitch, w);
        }
        for (int r = 0; r < h / 2; r++) {
            memcpy(dst->u_plane + r * (w / 2), src->u_plane + r * src->u_pitch, w / 2);
        }
        for (int r = 0; r < h / 2; r++) {
            memcpy(dst->v_plane + r * (w / 2), src->v_plane + r * src->v_pitch, w / 2);
        }
    } else if (src->data) {
        memcpy(dst->data, src->data, total);
    }
    return true;
}

bool video_upscaler_process(VideoUpscaler *upscaler, const DecodedVideoFrame *src_frame, DecodedVideoFrame *out_frame) {
    if (!upscaler || !src_frame || !out_frame) return false;

    int in_w = src_frame->width;
    int in_h = src_frame->height;
    if (in_w <= 0 || in_h <= 0) return false;

    int out_w = in_w;
    int out_h = in_h;
    bool is_active = false;

    video_upscaler_calculate_target_size(
        upscaler->mode, in_w, in_h, upscaler->target_bound_w, upscaler->target_bound_h,
        &out_w, &out_h, &is_active
    );

    // Update telemetry state
    upscaler->info.original_width = in_w;
    upscaler->info.original_height = in_h;
    upscaler->info.output_width = out_w;
    upscaler->info.output_height = out_h;
    upscaler->info.is_active = is_active;
    upscaler->info.scale_factor = (double)out_w / (double)in_w;
    snprintf(upscaler->info.algorithm_name, sizeof(upscaler->info.algorithm_name), "%s",
             video_upscaler_quality_to_string(upscaler->quality));
    snprintf(upscaler->info.mode_name, sizeof(upscaler->info.mode_name), "%s",
             video_upscaler_mode_to_string(upscaler->mode));

    // If upscaling is not active: produce direct clone
    if (!is_active) {
        return copy_frame_clone(src_frame, out_frame);
    }

    // Optional AI Backend Hook (if registered)
    if (upscaler->ai_backend != AI_BACKEND_NONE && upscaler->ai_callback) {
        if (upscaler->ai_callback(upscaler->ai_user_data, src_frame, out_frame)) {
            return true;
        }
    }

    // High-Quality FFmpeg libswscale Upscaling
    int sws_flag = SWS_LANCZOS;
    if (upscaler->quality == UPSCALING_QUALITY_BALANCED) {
        sws_flag = SWS_BICUBIC;
    } else if (upscaler->quality == UPSCALING_QUALITY_PERFORMANCE) {
        sws_flag = SWS_FAST_BILINEAR;
    }
    sws_flag |= SWS_ACCURATE_RND;

    // Check if SwsContext needs creation or recreation
    if (!upscaler->sws_ctx ||
        upscaler->cached_in_w != in_w || upscaler->cached_in_h != in_h ||
        upscaler->cached_out_w != out_w || upscaler->cached_out_h != out_h ||
        upscaler->cached_sws_flags != sws_flag) {

        if (upscaler->sws_ctx) {
            sws_freeContext(upscaler->sws_ctx);
            upscaler->sws_ctx = NULL;
        }

        upscaler->sws_ctx = sws_getContext(
            in_w, in_h, AV_PIX_FMT_YUV420P,
            out_w, out_h, AV_PIX_FMT_YUV420P,
            sws_flag, NULL, NULL, NULL
        );

        if (!upscaler->sws_ctx) {
            fprintf(stderr, "[VideoUpscaler] Failed to initialize SwsContext %dx%d -> %dx%d\n",
                    in_w, in_h, out_w, out_h);
            return copy_frame_clone(src_frame, out_frame);
        }

        upscaler->cached_in_w = in_w;
        upscaler->cached_in_h = in_h;
        upscaler->cached_out_w = out_w;
        upscaler->cached_out_h = out_h;
        upscaler->cached_sws_flags = sws_flag;
    }

    // Allocate memory for the upscaled frame
    int y_size = out_w * out_h;
    int u_size = (out_w / 2) * (out_h / 2);
    int v_size = u_size;
    size_t total_size = (size_t)(y_size + u_size + v_size);

    uint8_t *dst_buf = (uint8_t *)malloc(total_size);
    if (!dst_buf) return false;

    out_frame->data = dst_buf;
    out_frame->y_plane = dst_buf;
    out_frame->u_plane = dst_buf + y_size;
    out_frame->v_plane = dst_buf + y_size + u_size;
    out_frame->y_pitch = out_w;
    out_frame->u_pitch = out_w / 2;
    out_frame->v_pitch = out_w / 2;
    out_frame->width = out_w;
    out_frame->height = out_h;
    out_frame->linesize = out_w;
    out_frame->is_yuv = 1;
    out_frame->pts_sec = src_frame->pts_sec; // Strictly preserve PTS for 1.0x playback speed!

    // Setup source slice pointers and strides
    const uint8_t *src_slices[3] = { src_frame->y_plane, src_frame->u_plane, src_frame->v_plane };
    int src_strides[3] = { src_frame->y_pitch, src_frame->u_pitch, src_frame->v_pitch };

    // Setup destination slice pointers and strides
    uint8_t *dst_slices[3] = { out_frame->y_plane, out_frame->u_plane, out_frame->v_plane };
    int dst_strides[3] = { out_frame->y_pitch, out_frame->u_pitch, out_frame->v_pitch };

    // Execute high-quality scaling via libswscale
    sws_scale(upscaler->sws_ctx, src_slices, src_strides, 0, in_h, dst_slices, dst_strides);

    return true;
}

VideoUpscalerInfo video_upscaler_get_info(const VideoUpscaler *upscaler) {
    if (!upscaler) {
        VideoUpscalerInfo zero = {0};
        return zero;
    }
    return upscaler->info;
}

void video_upscaler_register_ai_backend(
    VideoUpscaler *upscaler,
    AIBackendType backend_type,
    const char *model_path,
    AIUpscaleCallback callback,
    void *user_data
) {
    if (!upscaler) return;
    upscaler->ai_backend = backend_type;
    if (model_path) {
        snprintf(upscaler->ai_model_path, sizeof(upscaler->ai_model_path), "%s", model_path);
    } else {
        upscaler->ai_model_path[0] = '\0';
    }
    upscaler->ai_callback = callback;
    upscaler->ai_user_data = user_data;
}
