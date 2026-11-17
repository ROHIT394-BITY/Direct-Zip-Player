#include "video_decoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t pos;
} MemoryBufferReader;

static int buffer_read_packet(void *opaque, uint8_t *buf, int buf_size) {
    MemoryBufferReader *br = (MemoryBufferReader *)opaque;
    if (!br || br->pos >= br->size) {
        return AVERROR_EOF;
    }
    size_t remaining = br->size - br->pos;
    size_t to_copy = (size_t)buf_size < remaining ? (size_t)buf_size : remaining;
    memcpy(buf, br->data + br->pos, to_copy);
    br->pos += to_copy;
    return (int)to_copy;
}

static int64_t buffer_seek_packet(void *opaque, int64_t offset, int whence) {
    MemoryBufferReader *br = (MemoryBufferReader *)opaque;
    if (!br) return -1;

    if (whence == AVSEEK_SIZE) {
        return (int64_t)br->size;
    }

    int64_t new_pos = -1;
    switch (whence & ~AVSEEK_FORCE) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = (int64_t)br->pos + offset;
            break;
        case SEEK_END:
            new_pos = (int64_t)br->size + offset;
            break;
        default:
            return -1;
    }

    if (new_pos < 0 || (size_t)new_pos > br->size) {
        return -1;
    }

    br->pos = (size_t)new_pos;
    return (int64_t)br->pos;
}

#define PKT_QUEUE_CAPACITY 512

typedef struct {
    AVPacket *packets[PKT_QUEUE_CAPACITY];
    int head;
    int tail;
    int count;
} PacketQueue;

static void pkt_queue_init(PacketQueue *q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    memset(q->packets, 0, sizeof(q->packets));
}

static bool pkt_queue_push(PacketQueue *q, const AVPacket *src) {
    if (q->count >= PKT_QUEUE_CAPACITY) return false;
    AVPacket *pkt = av_packet_alloc();
    if (!pkt) return false;
    av_packet_ref(pkt, src);
    q->packets[q->tail] = pkt;
    q->tail = (q->tail + 1) % PKT_QUEUE_CAPACITY;
    q->count++;
    return true;
}

static AVPacket* pkt_queue_peek(PacketQueue *q) {
    if (q->count == 0) return NULL;
    return q->packets[q->head];
}

static void pkt_queue_pop(PacketQueue *q) {
    if (q->count == 0) return;
    if (q->packets[q->head]) {
        av_packet_free(&q->packets[q->head]);
    }
    q->head = (q->head + 1) % PKT_QUEUE_CAPACITY;
    q->count--;
}

static void pkt_queue_flush(PacketQueue *q) {
    while (q->count > 0) {
        if (q->packets[q->head]) {
            av_packet_free(&q->packets[q->head]);
        }
        q->head = (q->head + 1) % PKT_QUEUE_CAPACITY;
        q->count--;
    }
    q->head = 0;
    q->tail = 0;
}

struct VideoDecoder {
    MemoryBufferReader mem_reader;
    uint8_t *avio_buffer;
    AVIOContext *avio_ctx;
    AVFormatContext *fmt_ctx;

    int video_stream_idx;
    int audio_stream_idx;

    AVCodecContext *video_codec_ctx;
    AVCodecContext *audio_codec_ctx;

    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;

    AVFrame *raw_frame;
    AVFrame *yuv_frame;
    uint8_t *yuv_buffer;
    int yuv_buffer_size;

    AVPacket *pkt;
    PacketQueue video_pkt_queue;
    PacketQueue audio_pkt_queue;
    VideoInfo info;
};

VideoDecoder* video_decoder_open_memory(const MemoryBuffer *buffer) {
    if (!buffer || !buffer->data || buffer->size == 0) {
        fprintf(stderr, "[VideoDecoder] Invalid memory buffer\n");
        return NULL;
    }

    VideoDecoder *dec = (VideoDecoder *)calloc(1, sizeof(VideoDecoder));
    if (!dec) return NULL;

    dec->video_stream_idx = -1;
    dec->audio_stream_idx = -1;
    pkt_queue_init(&dec->video_pkt_queue);
    pkt_queue_init(&dec->audio_pkt_queue);
    dec->mem_reader.data = buffer->data;
    dec->mem_reader.size = buffer->size;
    dec->mem_reader.pos = 0;

    size_t avio_buf_size = 64 * 1024;
    dec->avio_buffer = (uint8_t *)av_malloc(avio_buf_size);
    if (!dec->avio_buffer) {
        free(dec);
        return NULL;
    }

    dec->avio_ctx = avio_alloc_context(
        dec->avio_buffer,
        (int)avio_buf_size,
        0, // read-only
        &dec->mem_reader,
        buffer_read_packet,
        NULL,
        buffer_seek_packet
    );

    if (!dec->avio_ctx) {
        av_free(dec->avio_buffer);
        free(dec);
        return NULL;
    }

    dec->fmt_ctx = avformat_alloc_context();
    if (!dec->fmt_ctx) {
        avio_context_free(&dec->avio_ctx);
        free(dec);
        return NULL;
    }

    dec->fmt_ctx->pb = dec->avio_ctx;
    dec->fmt_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;

    // Open stream directly from in-memory AVIO
    if (avformat_open_input(&dec->fmt_ctx, "memory_mp4", NULL, NULL) < 0) {
        fprintf(stderr, "[VideoDecoder] Failed to open in-memory MP4 stream\n");
        avio_context_free(&dec->avio_ctx);
        avformat_free_context(dec->fmt_ctx);
        free(dec);
        return NULL;
    }

    if (avformat_find_stream_info(dec->fmt_ctx, NULL) < 0) {
        fprintf(stderr, "[VideoDecoder] Failed to find stream info\n");
        video_decoder_close(dec);
        return NULL;
    }

    // Video stream discovery
    const AVCodec *v_codec = NULL;
    dec->video_stream_idx = av_find_best_stream(dec->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &v_codec, 0);
    if (dec->video_stream_idx >= 0 && v_codec) {
        AVStream *st = dec->fmt_ctx->streams[dec->video_stream_idx];
        dec->video_codec_ctx = avcodec_alloc_context3(v_codec);
        if (dec->video_codec_ctx) {
            avcodec_parameters_to_context(dec->video_codec_ctx, st->codecpar);
            if (avcodec_open2(dec->video_codec_ctx, v_codec, NULL) == 0) {
                dec->info.has_video = 1;
                // Preserve original native stream resolution
                dec->info.native_width = dec->video_codec_ctx->width;
                dec->info.native_height = dec->video_codec_ctx->height;
                dec->info.width = dec->info.native_width;
                dec->info.height = dec->info.native_height;

                // Time base
                dec->info.time_base_num = st->time_base.num;
                dec->info.time_base_den = st->time_base.den;

                // Human-readable resolution label
                if (dec->info.native_height >= 2160 || dec->info.native_width >= 3840) {
                    snprintf(dec->info.resolution_label, sizeof(dec->info.resolution_label), "4K UHD");
                } else if (dec->info.native_height >= 1080 || dec->info.native_width >= 1920) {
                    snprintf(dec->info.resolution_label, sizeof(dec->info.resolution_label), "1080p FHD");
                } else if (dec->info.native_height >= 720 || dec->info.native_width >= 1280) {
                    snprintf(dec->info.resolution_label, sizeof(dec->info.resolution_label), "720p HD");
                } else if (dec->info.native_height >= 480) {
                    snprintf(dec->info.resolution_label, sizeof(dec->info.resolution_label), "480p SD");
                } else {
                    snprintf(dec->info.resolution_label, sizeof(dec->info.resolution_label), "%dp", dec->info.native_height);
                }

                // Framerate
                dec->info.fps = av_q2d(st->avg_frame_rate);
                if (dec->info.fps <= 0.0) dec->info.fps = av_q2d(st->r_frame_rate);
                if (dec->info.fps <= 0.0) dec->info.fps = 25.0;

                strncpy(dec->info.video_codec_name, v_codec->name, sizeof(dec->info.video_codec_name) - 1);
            }
        }
    }

    // Audio stream discovery
    const AVCodec *a_codec = NULL;
    dec->audio_stream_idx = av_find_best_stream(dec->fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &a_codec, 0);
    if (dec->audio_stream_idx >= 0 && a_codec) {
        AVStream *st = dec->fmt_ctx->streams[dec->audio_stream_idx];
        dec->audio_codec_ctx = avcodec_alloc_context3(a_codec);
        if (dec->audio_codec_ctx) {
            avcodec_parameters_to_context(dec->audio_codec_ctx, st->codecpar);
            if (avcodec_open2(dec->audio_codec_ctx, a_codec, NULL) == 0) {
                dec->info.has_audio = 1;
                dec->info.audio_sample_rate = dec->audio_codec_ctx->sample_rate;
                dec->info.audio_channels = dec->audio_codec_ctx->ch_layout.nb_channels;
                strncpy(dec->info.audio_codec_name, a_codec->name, sizeof(dec->info.audio_codec_name) - 1);

                // Configure SwrContext for conversion to S16 stereo 44100Hz
                AVChannelLayout out_layout = AV_CHANNEL_LAYOUT_STEREO;
                swr_alloc_set_opts2(&dec->swr_ctx,
                                    &out_layout, AV_SAMPLE_FMT_S16, 44100,
                                    &dec->audio_codec_ctx->ch_layout,
                                    dec->audio_codec_ctx->sample_fmt,
                                    dec->audio_codec_ctx->sample_rate,
                                    0, NULL);
                if (dec->swr_ctx) {
                    swr_init(dec->swr_ctx);
                }
            }
        }
    }

    if (dec->fmt_ctx->duration > 0) {
        dec->info.duration_sec = (double)dec->fmt_ctx->duration / (double)AV_TIME_BASE;
    } else if (dec->video_stream_idx >= 0 && dec->fmt_ctx->streams[dec->video_stream_idx]->duration > 0) {
        AVStream *st = dec->fmt_ctx->streams[dec->video_stream_idx];
        dec->info.duration_sec = (double)st->duration * av_q2d(st->time_base);
    }

    dec->raw_frame = av_frame_alloc();
    dec->yuv_frame = av_frame_alloc();
    dec->pkt = av_packet_alloc();

    if (dec->info.has_video) {
        // Pre-allocate YUV420P buffer for non-YUV420P fallback conversions
        dec->yuv_buffer_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, dec->info.width, dec->info.height, 1);
        dec->yuv_buffer = (uint8_t *)av_malloc(dec->yuv_buffer_size);
        av_image_fill_arrays(dec->yuv_frame->data, dec->yuv_frame->linesize,
                             dec->yuv_buffer, AV_PIX_FMT_YUV420P,
                             dec->info.width, dec->info.height, 1);

        // SwsContext if source is not native YUV420P
        if (dec->video_codec_ctx->pix_fmt != AV_PIX_FMT_YUV420P &&
            dec->video_codec_ctx->pix_fmt != AV_PIX_FMT_YUVJ420P) {
            dec->sws_ctx = sws_getContext(
                dec->video_codec_ctx->width, dec->video_codec_ctx->height, dec->video_codec_ctx->pix_fmt,
                dec->info.width, dec->info.height, AV_PIX_FMT_YUV420P,
                SWS_BICUBIC | SWS_ACCURATE_RND, NULL, NULL, NULL
            );
        }
    }

    return dec;
}

VideoInfo video_decoder_get_info(const VideoDecoder *decoder) {
    if (!decoder) {
        VideoInfo zero = {0};
        return zero;
    }
    return decoder->info;
}

static bool copy_frame_to_output(VideoDecoder *decoder, AVFrame *src_frame, DecodedVideoFrame *out_frame) {
    int w = decoder->info.width;
    int h = decoder->info.height;

    out_frame->width = w;
    out_frame->height = h;
    out_frame->is_yuv = 1;

    // Check if source frame is already standard YUV420P
    if (src_frame->format == AV_PIX_FMT_YUV420P || src_frame->format == AV_PIX_FMT_YUVJ420P) {
        // Allocate contiguous buffer for Y, U, V planes
        int y_size = w * h;
        int u_size = (w / 2) * (h / 2);
        int v_size = u_size;
        size_t total_size = (size_t)(y_size + u_size + v_size);

        uint8_t *buf = (uint8_t *)malloc(total_size);
        if (!buf) return false;

        out_frame->data = buf;
        out_frame->y_plane = buf;
        out_frame->u_plane = buf + y_size;
        out_frame->v_plane = buf + y_size + u_size;
        out_frame->y_pitch = w;
        out_frame->u_pitch = w / 2;
        out_frame->v_pitch = w / 2;
        out_frame->linesize = w;

        // Copy Y plane row-by-row to pack tightly
        for (int r = 0; r < h; r++) {
            memcpy(out_frame->y_plane + r * w, src_frame->data[0] + r * src_frame->linesize[0], w);
        }
        // Copy U plane
        for (int r = 0; r < h / 2; r++) {
            memcpy(out_frame->u_plane + r * (w / 2), src_frame->data[1] + r * src_frame->linesize[1], w / 2);
        }
        // Copy V plane
        for (int r = 0; r < h / 2; r++) {
            memcpy(out_frame->v_plane + r * (w / 2), src_frame->data[2] + r * src_frame->linesize[2], w / 2);
        }
    } else {
        // Source is not YUV420P: convert via sws_scale
        if (!decoder->sws_ctx) {
            decoder->sws_ctx = sws_getContext(
                decoder->video_codec_ctx->width, decoder->video_codec_ctx->height, src_frame->format,
                w, h, AV_PIX_FMT_YUV420P,
                SWS_BICUBIC | SWS_ACCURATE_RND, NULL, NULL, NULL
            );
        }

        sws_scale(decoder->sws_ctx,
                  (const uint8_t * const *)src_frame->data,
                  src_frame->linesize,
                  0, decoder->video_codec_ctx->height,
                  decoder->yuv_frame->data,
                  decoder->yuv_frame->linesize);

        int y_size = w * h;
        int u_size = (w / 2) * (h / 2);
        int v_size = u_size;
        size_t total_size = (size_t)(y_size + u_size + v_size);

        uint8_t *buf = (uint8_t *)malloc(total_size);
        if (!buf) return false;

        out_frame->data = buf;
        out_frame->y_plane = buf;
        out_frame->u_plane = buf + y_size;
        out_frame->v_plane = buf + y_size + u_size;
        out_frame->y_pitch = w;
        out_frame->u_pitch = w / 2;
        out_frame->v_pitch = w / 2;
        out_frame->linesize = w;

        for (int r = 0; r < h; r++) {
            memcpy(out_frame->y_plane + r * w, decoder->yuv_frame->data[0] + r * decoder->yuv_frame->linesize[0], w);
        }
        for (int r = 0; r < h / 2; r++) {
            memcpy(out_frame->u_plane + r * (w / 2), decoder->yuv_frame->data[1] + r * decoder->yuv_frame->linesize[1], w / 2);
        }
        for (int r = 0; r < h / 2; r++) {
            memcpy(out_frame->v_plane + r * (w / 2), decoder->yuv_frame->data[2] + r * decoder->yuv_frame->linesize[2], w / 2);
        }
    }

    AVStream *st = decoder->fmt_ctx->streams[decoder->video_stream_idx];
    if (src_frame->pts != AV_NOPTS_VALUE) {
        out_frame->pts_sec = (double)src_frame->pts * av_q2d(st->time_base);
    } else if (src_frame->pkt_dts != AV_NOPTS_VALUE) {
        out_frame->pts_sec = (double)src_frame->pkt_dts * av_q2d(st->time_base);
    } else {
        out_frame->pts_sec = 0.0;
    }

    return true;
}

static bool fetch_packet_for_stream(VideoDecoder *decoder, int target_stream_idx) {
    while (1) {
        if (target_stream_idx == decoder->video_stream_idx && decoder->video_pkt_queue.count > 0) {
            return true;
        }
        if (target_stream_idx == decoder->audio_stream_idx && decoder->audio_pkt_queue.count > 0) {
            return true;
        }

        int ret = av_read_frame(decoder->fmt_ctx, decoder->pkt);
        if (ret < 0) {
            return false;
        }

        if (decoder->pkt->stream_index == decoder->video_stream_idx) {
            pkt_queue_push(&decoder->video_pkt_queue, decoder->pkt);
        } else if (decoder->pkt->stream_index == decoder->audio_stream_idx) {
            pkt_queue_push(&decoder->audio_pkt_queue, decoder->pkt);
        }
        av_packet_unref(decoder->pkt);

        if (target_stream_idx == decoder->video_stream_idx && decoder->video_pkt_queue.count > 0) {
            return true;
        }
        if (target_stream_idx == decoder->audio_stream_idx && decoder->audio_pkt_queue.count > 0) {
            return true;
        }
    }
}

bool video_decoder_read_video_frame(VideoDecoder *decoder, DecodedVideoFrame *out_frame) {
    if (!decoder || !decoder->info.has_video || !decoder->video_codec_ctx || !out_frame) return false;

    while (1) {
        int ret = avcodec_receive_frame(decoder->video_codec_ctx, decoder->raw_frame);
        if (ret == 0) {
            return copy_frame_to_output(decoder, decoder->raw_frame, out_frame);
        }
        if (ret == AVERROR_EOF) {
            return false;
        }

        if (decoder->video_pkt_queue.count == 0) {
            if (!fetch_packet_for_stream(decoder, decoder->video_stream_idx)) {
                // Drain video decoder
                avcodec_send_packet(decoder->video_codec_ctx, NULL);
                ret = avcodec_receive_frame(decoder->video_codec_ctx, decoder->raw_frame);
                if (ret == 0) {
                    return copy_frame_to_output(decoder, decoder->raw_frame, out_frame);
                }
                return false;
            }
        }

        AVPacket *pkt = pkt_queue_peek(&decoder->video_pkt_queue);
        if (pkt) {
            int send_ret = avcodec_send_packet(decoder->video_codec_ctx, pkt);
            if (send_ret == 0) {
                pkt_queue_pop(&decoder->video_pkt_queue);
            } else if (send_ret == AVERROR(EAGAIN)) {
                continue;
            } else {
                pkt_queue_pop(&decoder->video_pkt_queue);
            }
        }
    }
}

bool video_decoder_read_audio_frame(VideoDecoder *decoder, DecodedAudioFrame *out_frame) {
    if (!decoder || !decoder->info.has_audio || !decoder->audio_codec_ctx || !decoder->swr_ctx || !out_frame) {
        return false;
    }

    while (1) {
        int ret = avcodec_receive_frame(decoder->audio_codec_ctx, decoder->raw_frame);
        if (ret == 0) {
            int out_samples = (int)av_rescale_rnd(
                swr_get_delay(decoder->swr_ctx, decoder->audio_codec_ctx->sample_rate) + decoder->raw_frame->nb_samples,
                44100, decoder->audio_codec_ctx->sample_rate, AV_ROUND_UP
            );

            // 16-bit stereo = 4 bytes per sample
            int out_buf_size = out_samples * 2 * 2;
            uint8_t *out_samples_buf = (uint8_t *)malloc(out_buf_size);
            if (!out_samples_buf) return false;

            uint8_t *out_ptrs[1] = { out_samples_buf };
            int converted = swr_convert(
                decoder->swr_ctx,
                out_ptrs, out_samples,
                (const uint8_t **)decoder->raw_frame->extended_data, decoder->raw_frame->nb_samples
            );

            if (converted <= 0) {
                free(out_samples_buf);
                continue;
            }

            out_frame->data = out_samples_buf;
            out_frame->nb_samples = converted;
            out_frame->data_size = converted * 2 * 2;

            AVStream *st = decoder->fmt_ctx->streams[decoder->audio_stream_idx];
            if (decoder->raw_frame->pts != AV_NOPTS_VALUE) {
                out_frame->pts_sec = (double)decoder->raw_frame->pts * av_q2d(st->time_base);
            } else if (decoder->raw_frame->pkt_dts != AV_NOPTS_VALUE) {
                out_frame->pts_sec = (double)decoder->raw_frame->pkt_dts * av_q2d(st->time_base);
            } else {
                out_frame->pts_sec = 0.0;
            }
            return true;
        }

        if (ret == AVERROR_EOF) {
            return false;
        }

        if (decoder->audio_pkt_queue.count == 0) {
            if (!fetch_packet_for_stream(decoder, decoder->audio_stream_idx)) {
                // Drain audio decoder
                avcodec_send_packet(decoder->audio_codec_ctx, NULL);
                ret = avcodec_receive_frame(decoder->audio_codec_ctx, decoder->raw_frame);
                if (ret != 0) {
                    return false;
                }
                continue;
            }
        }

        AVPacket *pkt = pkt_queue_peek(&decoder->audio_pkt_queue);
        if (pkt) {
            int send_ret = avcodec_send_packet(decoder->audio_codec_ctx, pkt);
            if (send_ret == 0) {
                pkt_queue_pop(&decoder->audio_pkt_queue);
            } else if (send_ret == AVERROR(EAGAIN)) {
                continue;
            } else {
                pkt_queue_pop(&decoder->audio_pkt_queue);
            }
        }
    }
}

bool video_decoder_seek(VideoDecoder *decoder, double target_sec) {
    if (!decoder || !decoder->fmt_ctx) return false;

    if (target_sec < 0.0) target_sec = 0.0;
    if (decoder->info.duration_sec > 0.0 && target_sec > decoder->info.duration_sec) {
        target_sec = decoder->info.duration_sec;
    }

    int64_t target_ts = (int64_t)(target_sec * AV_TIME_BASE);
    int stream_idx = decoder->video_stream_idx >= 0 ? decoder->video_stream_idx : decoder->audio_stream_idx;
    
    int64_t stream_ts = target_ts;
    if (stream_idx >= 0) {
        AVStream *st = decoder->fmt_ctx->streams[stream_idx];
        stream_ts = av_rescale_q(target_ts, AV_TIME_BASE_Q, st->time_base);
    }

    int flags = AVSEEK_FLAG_BACKWARD;
    if (av_seek_frame(decoder->fmt_ctx, stream_idx, stream_ts, flags) < 0) {
        // Fallback with any flag
        if (avformat_seek_file(decoder->fmt_ctx, -1, INT64_MIN, target_ts, INT64_MAX, 0) < 0) {
            return false;
        }
    }

    // Flush packet queues
    pkt_queue_flush(&decoder->video_pkt_queue);
    pkt_queue_flush(&decoder->audio_pkt_queue);

    // Flush decoder buffers to avoid artifacts after seeking
    if (decoder->video_codec_ctx) {
        avcodec_flush_buffers(decoder->video_codec_ctx);
    }
    if (decoder->audio_codec_ctx) {
        avcodec_flush_buffers(decoder->audio_codec_ctx);
    }

    return true;
}

void video_decoder_free_video_frame(DecodedVideoFrame *frame) {
    if (frame && frame->data) {
        free(frame->data);
        frame->data = NULL;
        frame->y_plane = NULL;
        frame->u_plane = NULL;
        frame->v_plane = NULL;
    }
}

void video_decoder_free_audio_frame(DecodedAudioFrame *frame) {
    if (frame && frame->data) {
        free(frame->data);
        frame->data = NULL;
    }
}

void video_decoder_close(VideoDecoder *decoder) {
    if (!decoder) return;

    if (decoder->sws_ctx) {
        sws_freeContext(decoder->sws_ctx);
        decoder->sws_ctx = NULL;
    }

    if (decoder->swr_ctx) {
        swr_free(&decoder->swr_ctx);
        decoder->swr_ctx = NULL;
    }

    if (decoder->yuv_buffer) {
        av_free(decoder->yuv_buffer);
        decoder->yuv_buffer = NULL;
    }

    if (decoder->raw_frame) {
        av_frame_free(&decoder->raw_frame);
    }
    if (decoder->yuv_frame) {
        av_frame_free(&decoder->yuv_frame);
    }
    if (decoder->pkt) {
        av_packet_free(&decoder->pkt);
    }

    pkt_queue_flush(&decoder->video_pkt_queue);
    pkt_queue_flush(&decoder->audio_pkt_queue);

    if (decoder->video_codec_ctx) {
        avcodec_free_context(&decoder->video_codec_ctx);
    }
    if (decoder->audio_codec_ctx) {
        avcodec_free_context(&decoder->audio_codec_ctx);
    }

    if (decoder->fmt_ctx) {
        avformat_close_input(&decoder->fmt_ctx);
    }

    if (decoder->avio_ctx) {
        av_freep(&decoder->avio_ctx->buffer);
        avio_context_free(&decoder->avio_ctx);
    }

    free(decoder);
}
