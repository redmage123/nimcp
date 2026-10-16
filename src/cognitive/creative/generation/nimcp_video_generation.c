//=============================================================================
// nimcp_video_generation.c - Creative Video Generation Implementation
//=============================================================================
/**
 * @file nimcp_video_generation.c
 * @brief Implements video/animation generation
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#include "cognitive/creative/generation/nimcp_video_generation.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#define LOG_MODULE "VIDEO_GEN"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(video_generation, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Configuration Defaults
//=============================================================================

void video_generator_config_defaults(video_generator_config_t* config)
{
    if (!config) return;

    memset(config, 0, sizeof(video_generator_config_t));

    /* Model settings */
    strncpy(config->video_model_path, "/models/video/svd",
            sizeof(config->video_model_path) - 1);
    strncpy(config->interpolation_model_path, "/models/video/rife",
            sizeof(config->interpolation_model_path) - 1);
    config->use_gpu = true;
    config->gpu_device_id = 0;

    /* Default settings */
    config->default_width = 1024;
    config->default_height = 576;
    config->default_fps = 24.0f;
    config->default_quality = VIDEO_QUALITY_STANDARD;
    config->default_method = VIDEO_METHOD_FRAME_BY_FRAME;

    /* Temporal coherence */
    config->temporal_consistency_weight = 0.8f;
    config->enable_frame_interpolation = true;

    /* Resource limits */
    config->max_vram_bytes = 12ULL * 1024 * 1024 * 1024;  /* 12GB */
    config->max_frames_in_memory = 300;

    /* Output settings */
    config->default_codec = VIDEO_CODEC_H264;
    config->default_bitrate_kbps = 8000;
}

//=============================================================================
// Lifecycle
//=============================================================================

video_generator_t* video_generator_create(const video_generator_config_t* config)
{
    video_generator_t* gen = nimcp_calloc(1, sizeof(video_generator_t));
    if (!gen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "video_generator_create: gen is NULL");
        return NULL;
    }

    /* Apply config */
    if (config) {
        gen->config = *config;
    } else {
        video_generator_config_defaults(&gen->config);
    }

    /* Initialize statistics */
    gen->videos_generated = 0;
    gen->avg_quality_score = 0.0f;
    gen->avg_generation_time_seconds = 0.0f;

    /* Models loaded on demand */
    gen->video_model = NULL;
    gen->frame_model = NULL;
    gen->interpolation_model = NULL;
    gen->animation_model = NULL;

    /* Integration set later */
    gen->visual_generator = NULL;
    gen->music_generator = NULL;
    gen->aesthetic_evaluator = NULL;
    gen->creative_bridge = NULL;
    gen->current_style = NULL;

    return gen;
}

void video_generator_destroy(video_generator_t* gen)
{
    if (!gen) return;

    /* Free current style */
    if (gen->current_style) {
        if (gen->current_style->embedding) {
            nimcp_free(gen->current_style->embedding);
        }
        nimcp_free(gen->current_style);
    }

    nimcp_free(gen);
    gen = NULL;
}

//=============================================================================
// Internal: Frame Generation
//=============================================================================

/**
 * @brief Generate a single frame
 */
static int generate_frame(video_generator_t* gen,
                          const char* prompt,
                          uint32_t width, uint32_t height,
                          uint64_t seed, uint32_t frame_num,
                          const visual_image_t* prev_frame,
                          float temporal_weight,
                          visual_image_t* out_frame)
{
    (void)gen;
    (void)prompt;
    (void)prev_frame;
    (void)temporal_weight;

    /* Allocate frame */
    out_frame->width = width;
    out_frame->height = height;
    out_frame->channels = 3;
    out_frame->pixels = nimcp_calloc((size_t)width * (size_t)height * 3, sizeof(uint8_t));
    if (!out_frame->pixels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "generate_frame: out_frame->pixels is NULL");
        return -1;
    }

    /* Generate frame content (placeholder) */
    uint64_t state = seed + frame_num * 12345;

    float time_factor = (float)frame_num * 0.05f;

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            size_t idx = (y * width + x) * 3;

            /* Create animated pattern */
            float fx = (float)x / (float)width;
            float fy = (float)y / (float)height;

            /* Flowing pattern that changes with frame number */
            float pattern = sinf((fx + time_factor) * 10.0f) *
                           cosf((fy + time_factor * 0.5f) * 10.0f);
            pattern = pattern * 0.5f + 0.5f;

            /* Add some variation */
            state = state * 6364136223846793005ULL + 1442695040888963407ULL;
            float noise = (float)(state >> 56) / 255.0f * 0.05f;

            /* Color gradient that shifts over time */
            float r = pattern * 0.7f + noise + 0.15f;
            float g = pattern * 0.6f + sinf(time_factor) * 0.1f + 0.2f;
            float b = pattern * 0.8f + cosf(time_factor * 0.7f) * 0.1f + 0.1f;

            out_frame->pixels[idx + 0] = (uint8_t)(fminf(255.0f, r * 255.0f));
            out_frame->pixels[idx + 1] = (uint8_t)(fminf(255.0f, g * 255.0f));
            out_frame->pixels[idx + 2] = (uint8_t)(fminf(255.0f, b * 255.0f));
        }
    }

    return 0;
}

/**
 * @brief Interpolate between two frames
 */
static int interpolate_between_frames(const visual_image_t* frame_a,
                                       const visual_image_t* frame_b,
                                       float t,
                                       visual_image_t* out_frame)
{
    if (frame_a->width != frame_b->width ||
        frame_a->height != frame_b->height) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "interpolate_between_frames: operation failed");
        return -1;
    }

    out_frame->width = frame_a->width;
    out_frame->height = frame_a->height;
    out_frame->channels = frame_a->channels;

    size_t pixel_count = (size_t)frame_a->width * frame_a->height * frame_a->channels;
    out_frame->pixels = nimcp_calloc(pixel_count, sizeof(uint8_t));
    if (!out_frame->pixels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "interpolate_between_frames: out_frame->pixels is NULL");
        return -1;
    }

    /* Linear interpolation (would use optical flow in production) */
    for (size_t i = 0; i < pixel_count; i++) {
        float a_val = (float)frame_a->pixels[i];
        float b_val = (float)frame_b->pixels[i];
        out_frame->pixels[i] = (uint8_t)(a_val * (1.0f - t) + b_val * t);
    }

    return 0;
}

/**
 * @brief Apply camera movement to frame
 */
static void apply_camera_to_frame(const visual_image_t* src,
                                  visual_image_t* dst,
                                  const camera_spec_t* camera,
                                  float progress)
{
    if (!src || !dst || !camera) return;

    /* Calculate current camera parameters */
    float curr_x = camera->start_x + (camera->end_x - camera->start_x) * progress;
    float curr_y = camera->start_y + (camera->end_y - camera->start_y) * progress;
    float curr_zoom = camera->zoom_start +
                      (camera->zoom_end - camera->zoom_start) * progress;

    /* Clamp values */
    curr_x = fmaxf(0.0f, fminf(1.0f, curr_x));
    curr_y = fmaxf(0.0f, fminf(1.0f, curr_y));
    curr_zoom = fmaxf(0.5f, fminf(4.0f, curr_zoom));

    /* Calculate crop region */
    float crop_w = 1.0f / curr_zoom;
    float crop_h = 1.0f / curr_zoom;
    float crop_x = curr_x - crop_w * 0.5f;
    float crop_y = curr_y - crop_h * 0.5f;

    /* Clamp crop region to bounds */
    crop_x = fmaxf(0.0f, fminf(1.0f - crop_w, crop_x));
    crop_y = fmaxf(0.0f, fminf(1.0f - crop_h, crop_y));

    /* Resample source to destination with crop/zoom */
    for (uint32_t y = 0; y < dst->height; y++) {
        for (uint32_t x = 0; x < dst->width; x++) {
            /* Map to source coordinates */
            float src_fx = crop_x + ((float)x / dst->width) * crop_w;
            float src_fy = crop_y + ((float)y / dst->height) * crop_h;

            uint32_t src_x = (uint32_t)(src_fx * src->width);
            uint32_t src_y = (uint32_t)(src_fy * src->height);

            src_x = fminf(src_x, src->width - 1);
            src_y = fminf(src_y, src->height - 1);

            size_t src_idx = (src_y * src->width + src_x) * src->channels;
            size_t dst_idx = (y * dst->width + x) * dst->channels;

            for (uint32_t c = 0; c < dst->channels; c++) {
                dst->pixels[dst_idx + c] = src->pixels[src_idx + c];
            }
        }
    }
}

//=============================================================================
// Generation API
//=============================================================================

int video_generate(video_generator_t* gen,
                   const video_generation_request_t* request,
                   video_generation_result_t* result)
{
    if (!gen || !request || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "video_generate: required parameter is NULL (gen, request, result)");
        return -1;
    }

    memset(result, 0, sizeof(video_generation_result_t));

    clock_t start = clock();

    /* Determine parameters */
    uint32_t width = request->width > 0 ? request->width : gen->config.default_width;
    uint32_t height = request->height > 0 ? request->height : gen->config.default_height;
    float fps = request->fps > 0 ? request->fps : gen->config.default_fps;
    float duration = request->duration_seconds > 0 ?
                     request->duration_seconds : 5.0f;

    uint32_t num_frames = (uint32_t)(duration * fps);
    if (num_frames > gen->config.max_frames_in_memory) {
        num_frames = gen->config.max_frames_in_memory;
    }
    if (num_frames < 1) num_frames = 1;

    /* Allocate frames */
    result->frames = nimcp_calloc(num_frames, sizeof(video_frame_t));
    if (!result->frames) {
        result->success = false;
        strncpy(result->error_message, "Failed to allocate frames",
                sizeof(result->error_message) - 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "video_generate: result->frames is NULL");
        return -1;
    }

    result->num_frames = num_frames;
    result->width = width;
    result->height = height;
    result->fps = fps;
    result->duration_seconds = (float)num_frames / fps;
    result->codec = request->codec > 0 ? request->codec : gen->config.default_codec;

    /* Get seed */
    uint64_t seed = request->seed;
    if (seed == 0) {
        seed = (uint64_t)time(NULL);
    }

    /* Generate frames */
    visual_image_t prev_frame = {0};

    for (uint32_t i = 0; i < num_frames; i++) {
        video_frame_t* frame = &result->frames[i];
        frame->frame_number = i;
        frame->timestamp = (float)i / fps;

        int rc = generate_frame(gen, request->prompt, width, height,
                                seed, i,
                                i > 0 ? &prev_frame : NULL,
                                gen->config.temporal_consistency_weight,
                                &frame->image);
        if (rc != 0) {
            result->success = false;
            strncpy(result->error_message, "Frame generation failed",
                    sizeof(result->error_message) - 1);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "video_generate: validation failed");
            return -1;
        }

        /* Keep reference for temporal consistency */
        if (prev_frame.pixels) {
            nimcp_free(prev_frame.pixels);
        }
        prev_frame = frame->image;
        prev_frame.pixels = nimcp_calloc((size_t)width * (size_t)height * 3, sizeof(uint8_t));
        if (prev_frame.pixels) {
            memcpy(prev_frame.pixels, frame->image.pixels, (size_t)width * (size_t)height * 3);
        }
    }

    if (prev_frame.pixels) {
        nimcp_free(prev_frame.pixels);
    }

    /* Apply camera if specified */
    if (request->camera) {
        for (uint32_t i = 0; i < num_frames; i++) {
            float progress = (num_frames > 1) ? (float)i / (float)(num_frames - 1) : 0.0f;

            /* Allocate temp buffer */
            visual_image_t temp;
            temp.width = width;
            temp.height = height;
            temp.channels = 3;
            temp.pixels = nimcp_calloc((size_t)width * (size_t)height * 3, sizeof(uint8_t));

            if (temp.pixels) {
                apply_camera_to_frame(&result->frames[i].image, &temp,
                                     request->camera, progress);
                /* Copy back */
                memcpy(result->frames[i].image.pixels, temp.pixels,
                       (size_t)width * (size_t)height * 3);
                nimcp_free(temp.pixels);
            }
        }
    }

    /* Calculate temporal coherence */
    float coherence_sum = 0.0f;
    for (uint32_t i = 1; i < num_frames; i++) {
        /* Simple coherence: measure pixel difference between consecutive frames */
        float diff = 0.0f;
        size_t pixel_count = (size_t)width * height * 3;
        for (size_t j = 0; j < pixel_count; j++) {
            float d = (float)result->frames[i].image.pixels[j] -
                     (float)result->frames[i-1].image.pixels[j];
            diff += d * d;
        }
        diff = sqrtf(diff / (float)pixel_count) / 255.0f;
        coherence_sum += 1.0f - fminf(1.0f, diff * 10.0f);
    }
    result->temporal_coherence = num_frames > 1 ?
                                 coherence_sum / (num_frames - 1) : 1.0f;

    /* Quality evaluation */
    result->evaluation.overall_quality = 0.7f;  /* Placeholder */

    /* Timing */
    result->generation_time_seconds =
        (float)(clock() - start) / CLOCKS_PER_SEC;

    result->success = true;

    /* Update statistics */
    gen->videos_generated++;
    float n = (float)gen->videos_generated;
    gen->avg_quality_score = gen->avg_quality_score * ((n-1)/(fabsf(n) > 1e-7f ? n : 1e-7f)) +
                             result->evaluation.overall_quality / (fabsf(n) > 1e-7f ? n : 1e-7f);
    gen->avg_generation_time_seconds = gen->avg_generation_time_seconds * ((n-1)/(fabsf(n) > 1e-7f ? n : 1e-7f)) +
                                        result->generation_time_seconds / (fabsf(n) > 1e-7f ? n : 1e-7f);

    return 0;
}

int video_generate_from_keyframes(video_generator_t* gen,
                                   const video_keyframe_t* keyframes,
                                   uint32_t num_keyframes,
                                   float fps,
                                   const style_embedding_t* style,
                                   video_generation_result_t* result)
{
    if (!gen || !keyframes || num_keyframes < 2 || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "video_generate_from_keyframes: required parameter is NULL (gen, keyframes, result)");
        return -1;
    }

    memset(result, 0, sizeof(video_generation_result_t));

    (void)style;  /* Used in production for style-consistent generation */

    /* Calculate total duration */
    float total_duration = keyframes[num_keyframes - 1].timestamp;
    uint32_t total_frames = (uint32_t)(total_duration * fps);

    if (total_frames < 2) total_frames = 2;
    if (total_frames > gen->config.max_frames_in_memory) {
        total_frames = gen->config.max_frames_in_memory;
    }

    /* Allocate frames */
    result->frames = nimcp_calloc(total_frames, sizeof(video_frame_t));
    if (!result->frames) {
        result->success = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "video_generate_from_keyframes: result->frames is NULL");
        return -1;
    }

    result->num_frames = total_frames;
    result->fps = fps;
    result->duration_seconds = total_duration;

    /* Get dimensions from first keyframe */
    if (keyframes[0].image) {
        result->width = keyframes[0].image->width;
        result->height = keyframes[0].image->height;
    } else {
        result->width = gen->config.default_width;
        result->height = gen->config.default_height;
    }

    /* Generate keyframe images if not provided */
    visual_image_t* keyframe_images = nimcp_calloc(num_keyframes, sizeof(visual_image_t));
    if (!keyframe_images) {
        nimcp_free(result->frames);
        result->success = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "video_generate_from_keyframes: keyframe_images is NULL");
        return -1;
    }

    for (uint32_t k = 0; k < num_keyframes; k++) {
        if (keyframes[k].image) {
            /* Copy provided image */
            keyframe_images[k] = *keyframes[k].image;
            keyframe_images[k].pixels = nimcp_calloc(
                keyframes[k].image->width * keyframes[k].image->height *
                keyframes[k].image->channels, sizeof(uint8_t));
            if (keyframe_images[k].pixels) {
                memcpy(keyframe_images[k].pixels, keyframes[k].image->pixels,
                       keyframes[k].image->width * keyframes[k].image->height *
                       keyframes[k].image->channels);
            }
        } else {
            /* Generate from prompt */
            generate_frame(gen, keyframes[k].prompt,
                          result->width, result->height,
                          (uint64_t)time(NULL) + k, k,
                          NULL, 0.0f, &keyframe_images[k]);
        }
    }

    /* Interpolate between keyframes */
    uint32_t frame_idx = 0;
    for (uint32_t k = 0; k < num_keyframes - 1 && frame_idx < total_frames; k++) {
        float start_time = keyframes[k].timestamp;
        float end_time = keyframes[k + 1].timestamp;
        float segment_duration = end_time - start_time;

        uint32_t segment_frames = (uint32_t)(segment_duration * fps);
        if (segment_frames < 1) segment_frames = 1;

        for (uint32_t f = 0; f < segment_frames && frame_idx < total_frames; f++) {
            float t = (float)f / (float)segment_frames;

            result->frames[frame_idx].frame_number = frame_idx;
            result->frames[frame_idx].timestamp = start_time + segment_duration * t;

            interpolate_between_frames(&keyframe_images[k], &keyframe_images[k + 1],
                                       t, &result->frames[frame_idx].image);
            frame_idx++;
        }
    }

    /* Cleanup keyframe images */
    for (uint32_t k = 0; k < num_keyframes; k++) {
        if (keyframe_images[k].pixels) {
            nimcp_free(keyframe_images[k].pixels);
        }
    }
    nimcp_free(keyframe_images);
    keyframe_images = NULL;

    result->temporal_coherence = 0.85f;  /* Keyframe-based typically coherent */
    result->evaluation.overall_quality = 0.75f;
    result->success = true;

    return 0;
}

int video_animate_image(video_generator_t* gen,
                        const visual_image_t* image,
                        const char* motion_prompt,
                        float duration_seconds,
                        video_generation_result_t* result)
{
    if (!gen || !image || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "video_animate_image: required parameter is NULL (gen, image, result)");
        return -1;
    }

    memset(result, 0, sizeof(video_generation_result_t));

    (void)motion_prompt;  /* Used in production for motion guidance */

    float fps = gen->config.default_fps;
    uint32_t num_frames = (uint32_t)(duration_seconds * fps);
    if (num_frames < 2) num_frames = 2;
    if (num_frames > gen->config.max_frames_in_memory) {
        num_frames = gen->config.max_frames_in_memory;
    }

    result->frames = nimcp_calloc(num_frames, sizeof(video_frame_t));
    if (!result->frames) {
        result->success = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "video_animate_image: result->frames is NULL");
        return -1;
    }

    result->num_frames = num_frames;
    result->width = image->width;
    result->height = image->height;
    result->fps = fps;
    result->duration_seconds = (float)num_frames / fps;

    /* Animate the static image with subtle motion */
    for (uint32_t i = 0; i < num_frames; i++) {
        result->frames[i].frame_number = i;
        result->frames[i].timestamp = (float)i / fps;

        visual_image_t* frame = &result->frames[i].image;
        frame->width = image->width;
        frame->height = image->height;
        frame->channels = image->channels;

        size_t pixel_count = (size_t)image->width * image->height * image->channels;
        frame->pixels = nimcp_calloc(pixel_count, sizeof(uint8_t));
        if (!frame->pixels) continue;

        /* Apply subtle motion effect */
        float time = (float)i / fps;
        float wobble_x = sinf(time * 2.0f) * 2.0f;
        float wobble_y = cosf(time * 1.5f) * 2.0f;

        for (uint32_t y = 0; y < image->height; y++) {
            for (uint32_t x = 0; x < image->width; x++) {
                /* Source with motion offset */
                int src_x = (int)x + (int)wobble_x;
                int src_y = (int)y + (int)wobble_y;

                /* Clamp to bounds */
                src_x = src_x < 0 ? 0 :
                       (src_x >= (int)image->width ? (int)image->width - 1 : src_x);
                src_y = src_y < 0 ? 0 :
                       (src_y >= (int)image->height ? (int)image->height - 1 : src_y);

                size_t src_idx = ((uint32_t)src_y * image->width + (uint32_t)src_x) *
                                 image->channels;
                size_t dst_idx = (y * image->width + x) * image->channels;

                for (uint32_t c = 0; c < image->channels; c++) {
                    frame->pixels[dst_idx + c] = image->pixels[src_idx + c];
                }
            }
        }
    }

    result->temporal_coherence = 0.9f;  /* Animation from single image is coherent */
    result->evaluation.overall_quality = 0.7f;
    result->success = true;

    return 0;
}

//=============================================================================
// Frame Operations API
//=============================================================================

int video_interpolate_frames(video_generator_t* gen,
                              const visual_image_t* frame_a,
                              const visual_image_t* frame_b,
                              uint32_t num_frames,
                              visual_image_t* frames)
{
    if (!gen || !frame_a || !frame_b || !frames) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "video_interpolate_frames: required parameter is NULL (gen, frame_a, frame_b, frames)");
        return -1;
    }
    if (frame_a->width != frame_b->width ||
        frame_a->height != frame_b->height) return -1;

    for (uint32_t i = 0; i < num_frames; i++) {
        float t = (float)(i + 1) / (float)(num_frames + 1);
        int rc = interpolate_between_frames(frame_a, frame_b, t, &frames[i]);
        if (rc != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "video_interpolate_frames: validation failed");
            return -1;
        }
    }

    return 0;
}

int video_apply_camera(video_generator_t* gen,
                        const video_frame_t* frames,
                        uint32_t num_frames,
                        const camera_spec_t* camera,
                        video_generation_result_t* result)
{
    if (!gen || !frames || !camera || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "video_apply_camera: required parameter is NULL (gen, frames, camera, result)");
        return -1;
    }

    memset(result, 0, sizeof(video_generation_result_t));

    result->frames = nimcp_calloc(num_frames, sizeof(video_frame_t));
    if (!result->frames) {
        result->success = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "video_apply_camera: result->frames is NULL");
        return -1;
    }

    result->num_frames = num_frames;
    result->width = frames[0].image.width;
    result->height = frames[0].image.height;

    for (uint32_t i = 0; i < num_frames; i++) {
        float progress = (num_frames > 1) ? (float)i / (float)(num_frames - 1) : 0.0f;

        result->frames[i].frame_number = i;
        result->frames[i].timestamp = frames[i].timestamp;
        result->frames[i].image.width = frames[i].image.width;
        result->frames[i].image.height = frames[i].image.height;
        result->frames[i].image.channels = frames[i].image.channels;

        size_t pixel_count = (size_t)frames[i].image.width *
                             frames[i].image.height * frames[i].image.channels;
        result->frames[i].image.pixels = nimcp_calloc(pixel_count, sizeof(uint8_t));
        if (!result->frames[i].image.pixels) continue;

        apply_camera_to_frame(&frames[i].image, &result->frames[i].image,
                             camera, progress);
    }

    result->success = true;
    return 0;
}

//=============================================================================
// Video Editing API
//=============================================================================

int video_extend(video_generator_t* gen,
                 const video_generation_result_t* existing,
                 float additional_seconds,
                 const char* continuation_prompt,
                 video_generation_result_t* result)
{
    if (!gen || !existing || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "video_extend: required parameter is NULL (gen, existing, result)");
        return -1;
    }

    /* Create request for continuation */
    video_generation_request_t request;
    memset(&request, 0, sizeof(request));

    request.prompt = continuation_prompt;
    request.width = existing->width;
    request.height = existing->height;
    request.fps = existing->fps;
    request.duration_seconds = additional_seconds;

    /* Generate continuation */
    video_generation_result_t continuation;
    int rc = video_generate(gen, &request, &continuation);
    if (rc != 0) {
        result->success = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "video_extend: validation failed");
        return -1;
    }

    /* Concatenate - build array of structs */
    video_generation_result_t videos[2];
    memcpy(&videos[0], existing, sizeof(video_generation_result_t));
    memcpy(&videos[1], &continuation, sizeof(video_generation_result_t));
    rc = video_concatenate(videos, 2, "dissolve", 0.5f, result);

    video_generation_result_free(&continuation);

    return rc;
}

int video_concatenate(const video_generation_result_t* videos,
                      uint32_t num_videos,
                      const char* transition,
                      float transition_duration,
                      video_generation_result_t* result)
{
    if (!videos || num_videos == 0 || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "video_concatenate: required parameter is NULL (videos, result)");
        return -1;
    }

    memset(result, 0, sizeof(video_generation_result_t));

    /* Calculate total frames */
    uint32_t total_frames = 0;
    for (uint32_t v = 0; v < num_videos; v++) {
        total_frames += videos[v].num_frames;
    }

    /* Subtract transition overlap frames */
    float fps = videos[0].fps > 0 ? videos[0].fps : 24.0f;
    uint32_t trans_frames = (uint32_t)(transition_duration * fps);
    if (num_videos > 1) {
        uint32_t overlap = trans_frames * (num_videos - 1);
        if (overlap >= total_frames) {
            total_frames = num_videos;
        } else {
            total_frames -= overlap;
        }
    }

    result->frames = nimcp_calloc(total_frames, sizeof(video_frame_t));
    if (!result->frames) {
        result->success = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "video_concatenate: result->frames is NULL");
        return -1;
    }

    result->num_frames = total_frames;
    result->width = videos[0].width;
    result->height = videos[0].height;
    result->fps = fps;

    /* Copy frames with transitions */
    uint32_t out_idx = 0;
    float current_time = 0.0f;

    for (uint32_t v = 0; v < num_videos; v++) {
        uint32_t start_frame = 0;
        uint32_t end_frame = videos[v].num_frames;

        /* Skip overlapping frames for transition */
        if (v > 0 && trans_frames > 0) {
            start_frame = trans_frames;
        }

        for (uint32_t f = start_frame; f < end_frame && out_idx < total_frames; f++) {
            result->frames[out_idx].frame_number = out_idx;
            result->frames[out_idx].timestamp = current_time;

            /* Copy frame */
            const visual_image_t* src = &videos[v].frames[f].image;
            visual_image_t* dst = &result->frames[out_idx].image;

            dst->width = src->width;
            dst->height = src->height;
            dst->channels = src->channels;

            size_t pixel_count = (size_t)src->width * src->height * src->channels;
            dst->pixels = nimcp_calloc(pixel_count, sizeof(uint8_t));
            if (dst->pixels) {
                /* Apply transition blending at boundaries */
                bool in_transition = (f < start_frame + trans_frames && v > 0);
                if (in_transition && strcmp(transition, "dissolve") == 0) {
                    /* Blend with previous video's end frame */
                    float t = (float)(f - start_frame) / (float)trans_frames;
                    const visual_image_t* prev =
                        &videos[v-1].frames[videos[v-1].num_frames - trans_frames + f - start_frame].image;

                    for (size_t i = 0; i < pixel_count; i++) {
                        dst->pixels[i] = (uint8_t)(
                            prev->pixels[i] * (1.0f - t) + src->pixels[i] * t);
                    }
                } else {
                    memcpy(dst->pixels, src->pixels, pixel_count);
                }
            }

            current_time += 1.0f / fps;
            out_idx++;
        }
    }

    result->duration_seconds = current_time;
    result->success = true;

    return 0;
}

//=============================================================================
// Export API
//=============================================================================

int video_export(const video_generation_result_t* result,
                 const char* path,
                 const char* format)
{
    if (!result || !path || !format) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "video_export: required parameter is NULL (result, path, format)");
        return -1;
    }
    if (!result->frames || result->num_frames == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "video_export: result->frames is NULL");
        return -1;
    }

    /* Placeholder: would use ffmpeg or similar in production */
    /* For now, export as image sequence */
    char dir_path[NIMCP_METRICS_PATH_SIZE];
    snprintf(dir_path, sizeof(dir_path), "%s_frames", path);

    return video_export_frames(result, dir_path, "ppm", "frame_%04d");
}

int video_export_frames(const video_generation_result_t* result,
                        const char* dir,
                        const char* format,
                        const char* name_pattern)
{
    if (!result || !dir || !format || !name_pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "video_export_frames: required parameter is NULL (result, dir, format, name_pattern)");
        return -1;
    }

    /* Would create directory in production */
    (void)dir;

    for (uint32_t i = 0; i < result->num_frames; i++) {
        char path[NIMCP_METRICS_PATH_SIZE];
        char filename[NIMCP_SHORT_PATH_SIZE];
        snprintf(filename, sizeof(filename), name_pattern, i);
        snprintf(path, sizeof(path), "%s/%s.%s", dir, filename, format);

        /* Export frame */
        const visual_image_t* frame = &result->frames[i].image;
        FILE* fp = fopen(path, "wb");
        if (!fp) continue;

        fprintf(fp, "P6\n%u %u\n255\n", frame->width, frame->height);
        fwrite(frame->pixels, 1, frame->width * frame->height * 3, fp);
        fclose(fp);
    }

    return 0;
}

//=============================================================================
// Cleanup
//=============================================================================

void video_generation_result_free(video_generation_result_t* result)
{
    if (!result) return;

    if (result->frames) {
        for (uint32_t i = 0; i < result->num_frames; i++) {
            if (result->frames[i].image.pixels) {
                nimcp_free(result->frames[i].image.pixels);
            }
        }
        nimcp_free(result->frames);
        result->frames = NULL;
    }

    if (result->encoded_data) {
        nimcp_free(result->encoded_data);
        result->encoded_data = NULL;
    }

    if (result->audio_data) {
        nimcp_free(result->audio_data);
        result->audio_data = NULL;
    }

    result->num_frames = 0;
}

void video_keyframe_free(video_keyframe_t* keyframe)
{
    if (!keyframe) return;

    if (keyframe->image) {
        if (keyframe->image->pixels) {
            nimcp_free(keyframe->image->pixels);
        }
        nimcp_free(keyframe->image);
        keyframe->image = NULL;
    }
}
