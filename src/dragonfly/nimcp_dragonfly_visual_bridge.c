/**
 * @file nimcp_dragonfly_visual_bridge.c
 * @brief Visual Cortex Bridge Implementation
 *
 * Connects dragonfly interception system to visual cortex perception.
 */

#include "dragonfly/nimcp_dragonfly_visual_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_visual_bridge)

#define LOG_MODULE "DRAGONFLY_VISUAL_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Previous frame blob for motion tracking
 */
typedef struct {
    float x;
    float y;
    float size;
    bool valid;
    uint32_t track_id;
} prev_blob_t;

/**
 * @brief Visual bridge state
 */
struct dragonfly_visual_bridge_s {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    visual_bridge_config_t config;

    /* Connected systems */
    dragonfly_system_t* dragonfly;
    visual_cortex_t* visual_cortex;

    /* State */
    visual_motion_result_t current_result;
    uint32_t frame_count;
    uint32_t next_track_id;

    /* Previous frame for motion detection */
    prev_blob_t prev_blobs[VISUAL_BRIDGE_MAX_DETECTIONS];
    uint32_t num_prev_blobs;

    /* Statistics */
    visual_bridge_stats_t stats;
};

//=============================================================================
// Helper Functions
//=============================================================================

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static float distance_2d(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return sqrtf(dx*dx + dy*dy);
}

//=============================================================================
// Visual Cortex Integration Functions
//=============================================================================

int visual_cortex_extract_features(
    visual_cortex_t* cortex,
    const uint8_t* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    visual_features_t* features
) {
    if (!features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_extract_features: features is NULL");
        return -1;
    }

    memset(features, 0, sizeof(*features));
    features->timestamp_us = get_time_us();

    /* If no visual cortex, return empty features */
    if (!cortex || !image) {
        return 0;
    }

    /* Basic feature extraction from raw image data.
     * When full visual cortex integration is available, this should call
     * visual_cortex_extract_features_tensor() instead. */

    if (width == 0 || height == 0) return 0;

    uint32_t stride = width * channels;

    /* Compute average luminance */
    uint64_t total_lum = 0;
    uint32_t pixel_count = width * height;
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t idx = y * stride + x * channels;
            if (channels >= 3) {
                /* RGB luminance approximation */
                total_lum += (uint64_t)(image[idx] * 77 + image[idx+1] * 150 + image[idx+2] * 29) >> 8;
            } else {
                total_lum += image[idx];
            }
        }
    }
    features->avg_luminance = (float)total_lum / (float)pixel_count / 255.0f;

    /* Simple block-based contrast detection for motion regions.
     * Divide image into a grid and find high-contrast blocks. */
    uint32_t block_w = width / 8;
    uint32_t block_h = height / 8;
    if (block_w < 1) block_w = 1;
    if (block_h < 1) block_h = 1;

    features->num_motion_regions = 0;
    float avg_lum_byte = features->avg_luminance * 255.0f;

    for (uint32_t by = 0; by < 8 && by * block_h < height; by++) {
        for (uint32_t bx = 0; bx < 8 && bx * block_w < width; bx++) {
            if (features->num_motion_regions >= MAX_MOTION_REGIONS) break;

            /* Compute block statistics */
            float block_sum = 0.0f;
            float block_var = 0.0f;
            uint32_t count = 0;

            for (uint32_t y = by * block_h; y < (by + 1) * block_h && y < height; y++) {
                for (uint32_t x = bx * block_w; x < (bx + 1) * block_w && x < width; x++) {
                    uint32_t idx = y * stride + x * channels;
                    float val = (float)image[idx];
                    block_sum += val;
                    count++;
                }
            }

            if (count == 0) continue;
            float block_mean = block_sum / (float)count;

            /* Compute variance */
            for (uint32_t y = by * block_h; y < (by + 1) * block_h && y < height; y++) {
                for (uint32_t x = bx * block_w; x < (bx + 1) * block_w && x < width; x++) {
                    uint32_t idx = y * stride + x * channels;
                    float diff = (float)image[idx] - block_mean;
                    block_var += diff * diff;
                }
            }
            block_var /= (float)count;

            /* High contrast blocks become motion regions */
            float contrast = fabsf(block_mean - avg_lum_byte) / 255.0f;
            if (contrast > 0.15f && block_var > 100.0f) {
                visual_motion_region_t* reg = &features->motion_regions[features->num_motion_regions];
                reg->center_x = ((float)bx + 0.5f) / 8.0f;
                reg->center_y = ((float)by + 0.5f) / 8.0f;
                reg->size = (float)(block_w * block_h);
                reg->contrast = contrast;
                reg->salience = contrast * sqrtf(block_var) / 128.0f;
                if (reg->salience > 1.0f) reg->salience = 1.0f;
                reg->velocity_x = 0.0f;  /* No temporal info for single frame */
                reg->velocity_y = 0.0f;
                features->num_motion_regions++;
            }
        }
    }

    return 0;
}

const attention_map_t* visual_cortex_get_attention_map(visual_cortex_t* cortex) {
    /* TODO: Return attention map from visual cortex when available */
    (void)cortex;
    return NULL;  /* Not yet implemented */
}

//=============================================================================
// Configuration Functions
//=============================================================================

visual_bridge_config_t visual_bridge_default_config(void) {
    visual_bridge_config_t config;
    memset(&config, 0, sizeof(config));

    /* Motion detection */
    config.min_motion_speed = 2.0f;       /* 2 pixels/frame */
    config.min_blob_size = 4.0f;          /* 4 pixel radius */
    config.max_blob_size = 200.0f;        /* 200 pixel radius */
    config.contrast_threshold = 0.1f;

    /* Depth estimation */
    config.estimate_depth = true;
    config.assumed_target_size_m = 0.05f; /* 5cm target */

    /* Attention */
    config.use_attention_filter = true;
    config.attention_threshold = 0.2f;

    /* Default calibration (640x480 camera) */
    config.calibration.focal_length = 500.0f;
    config.calibration.principal_x = 320.0f;
    config.calibration.principal_y = 240.0f;
    config.calibration.image_width = 640;
    config.calibration.image_height = 480;
    config.calibration.baseline_distance = 0.0f;

    /* Update rate */
    config.frame_dt_s = 1.0f / 30.0f;  /* 30 FPS */

    return config;
}

bool visual_bridge_validate_config(const visual_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_bridge_validate_config: config is NULL");
        return false;
    }

    if (config->min_motion_speed < 0.0f) return false;
    if (config->min_blob_size <= 0.0f) return false;
    if (config->max_blob_size <= config->min_blob_size) {
        return false;
    }
    if (config->contrast_threshold < 0.0f || config->contrast_threshold > 1.0f) {
        return false;
    }
    if (config->assumed_target_size_m <= 0.0f) {
        return false;
    }
    if (config->attention_threshold < 0.0f || config->attention_threshold > 1.0f) {
        return false;
    }
    if (config->calibration.focal_length <= 0.0f) {
        return false;
    }
    if (config->calibration.image_width == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_bridge_validate_config: config->calibration.image_width is zero");
        return false;
    }
    if (config->calibration.image_height == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_bridge_validate_config: config->calibration.image_height is zero");
        return false;
    }
    if (config->frame_dt_s <= 0.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dragonfly_visual_bridge_t* dragonfly_visual_bridge_create(
    dragonfly_system_t* dragonfly,
    visual_cortex_t* visual_cortex,
    const visual_bridge_config_t* config
) {
    visual_bridge_config_t default_config;
    if (!config) {
        default_config = visual_bridge_default_config();
        config = &default_config;
    }

    if (!visual_bridge_validate_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_visual_bridge_create: invalid config");
        return NULL;
    }

    dragonfly_visual_bridge_t* bridge =
        (dragonfly_visual_bridge_t*)nimcp_calloc(1, sizeof(dragonfly_visual_bridge_t));
    NIMCP_API_CHECK_ALLOC(bridge, "dragonfly_visual_bridge_create: failed to allocate bridge");

    bridge->config = *config;
    bridge->dragonfly = dragonfly;
    bridge->visual_cortex = visual_cortex;
    bridge->next_track_id = 1;

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "dragonfly_visual") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "dragonfly_visual_bridge_create: failed to init base");
        nimcp_free(bridge);
        return NULL;
    }

    return bridge;
}

void dragonfly_visual_bridge_destroy(dragonfly_visual_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "dragonfly_visual");

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int dragonfly_visual_bridge_reset(dragonfly_visual_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_visual_bridge_reset: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    memset(&bridge->current_result, 0, sizeof(bridge->current_result));
    memset(bridge->prev_blobs, 0, sizeof(bridge->prev_blobs));
    bridge->num_prev_blobs = 0;
    bridge->frame_count = 0;
    bridge->next_track_id = 1;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Processing Functions
//=============================================================================

int dragonfly_visual_bridge_process_frame(
    dragonfly_visual_bridge_t* bridge,
    const float* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_visual_bridge_process_frame: bridge is NULL");
        return -1;
    }
    if (!image) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_visual_bridge_process_frame: image is NULL");
        return -1;
    }
    if (width == 0 || height == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_visual_bridge_process_frame: invalid dimensions");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t start_time = get_time_us();
    bridge->frame_count++;

    /* Clear current result */
    memset(&bridge->current_result, 0, sizeof(bridge->current_result));
    bridge->current_result.timestamp_us = start_time;
    bridge->current_result.frame_number = bridge->frame_count;

    /* Use visual cortex for feature extraction if available */
    if (bridge->visual_cortex) {
        /* Get visual cortex feature extraction */
        visual_features_t features;
        /* NOTE: visual_cortex_extract_features expects uint8_t* image data;
         * we receive float*. Cast is a raw bit reinterpretation (not a conversion). */
        if (visual_cortex_extract_features(bridge->visual_cortex, (const uint8_t*)image, width, height,
                                           channels, &features) == 0) {
            /* Extract motion blobs from V1/V2/MT hierarchy */
            for (uint32_t i = 0; i < features.num_motion_regions &&
                 bridge->current_result.num_blobs < VISUAL_BRIDGE_MAX_DETECTIONS; i++) {
                motion_blob_t blob;
                memset(&blob, 0, sizeof(blob));

                blob.center_x = features.motion_regions[i].center_x;
                blob.center_y = features.motion_regions[i].center_y;
                blob.size_pixels = features.motion_regions[i].size;
                blob.velocity_x = features.motion_regions[i].velocity_x;
                blob.velocity_y = features.motion_regions[i].velocity_y;
                blob.contrast = features.motion_regions[i].contrast;
                blob.salience = features.motion_regions[i].salience;
                blob.track_id = bridge->next_track_id++;

                /* Filter by configuration thresholds */
                float speed = sqrtf(blob.velocity_x * blob.velocity_x +
                                    blob.velocity_y * blob.velocity_y);
                if (speed >= bridge->config.min_motion_speed &&
                    blob.size_pixels >= bridge->config.min_blob_size &&
                    blob.size_pixels <= bridge->config.max_blob_size &&
                    blob.contrast >= bridge->config.contrast_threshold) {

                    bridge->current_result.blobs[bridge->current_result.num_blobs++] = blob;
                    bridge->stats.blobs_detected++;

                    /* Update attention peak */
                    if (blob.salience > bridge->current_result.peak_salience) {
                        bridge->current_result.attention_peak_x = blob.center_x;
                        bridge->current_result.attention_peak_y = blob.center_y;
                        bridge->current_result.peak_salience = blob.salience;
                    }
                }
            }
        }
    } else {
        /* Fallback: simple intensity-based blob detection */
        /* Scan image for high-contrast moving regions */
        float prev_intensity = 0.0f;
        for (uint32_t y = 1; y < height - 1 &&
             bridge->current_result.num_blobs < VISUAL_BRIDGE_MAX_DETECTIONS; y++) {
            for (uint32_t x = 1; x < width - 1; x++) {
                uint32_t idx = (y * width + x) * channels;
                float intensity = image[idx];
                if (channels > 1) {
                    intensity = 0.299f * image[idx] + 0.587f * image[idx+1] + 0.114f * image[idx+2];
                }

                /* Detect local maxima above contrast threshold */
                float local_contrast = fabsf(intensity - prev_intensity);
                if (local_contrast > bridge->config.contrast_threshold * 2.0f) {
                    motion_blob_t blob;
                    memset(&blob, 0, sizeof(blob));
                    blob.center_x = (float)x;
                    blob.center_y = (float)y;
                    blob.size_pixels = bridge->config.min_blob_size;
                    blob.contrast = local_contrast;
                    blob.salience = local_contrast;
                    blob.track_id = bridge->next_track_id++;

                    bridge->current_result.blobs[bridge->current_result.num_blobs++] = blob;
                    bridge->stats.blobs_detected++;
                }
                prev_intensity = intensity;
            }
        }
    }

    /* Update statistics */
    uint64_t elapsed = get_time_us() - start_time;
    bridge->stats.frames_processed++;
    {
        float new_avg =
            (bridge->stats.avg_process_time_us * (bridge->stats.frames_processed - 1) + (float)elapsed)
            / (float)bridge->stats.frames_processed;
        if (isfinite(new_avg)) {
            bridge->stats.avg_process_time_us = new_avg;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int dragonfly_visual_bridge_process_features(
    dragonfly_visual_bridge_t* bridge,
    const float* features,
    uint32_t feature_dim,
    const attention_map_t* attention
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_visual_bridge_process_features: bridge is NULL");
        return -1;
    }
    if (!features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_visual_bridge_process_features: features is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Extract motion information from visual cortex features */
    /* Feature vector layout (assuming MT-style features):
     * [0-31]: Direction-selective responses (32 directions)
     * [32-63]: Speed-selective responses (32 speeds)
     * [64-95]: Size-selective responses (32 sizes)
     * [96-127]: Contrast responses
     */

    if (feature_dim >= 128) {
        /* Find peak direction */
        float max_dir_response = 0.0f;
        uint32_t peak_dir_idx = 0;
        for (uint32_t i = 0; i < 32 && i < feature_dim; i++) {
            if (features[i] > max_dir_response) {
                max_dir_response = features[i];
                peak_dir_idx = i;
            }
        }

        /* Find peak speed */
        float max_speed_response = 0.0f;
        uint32_t peak_speed_idx = 0;
        for (uint32_t i = 32; i < 64 && i < feature_dim; i++) {
            if (features[i] > max_speed_response) {
                max_speed_response = features[i];
                peak_speed_idx = i - 32;
            }
        }

        /* Convert to motion blob if responses are strong enough */
        if (max_dir_response > 0.3f && max_speed_response > 0.2f) {
            motion_blob_t blob;
            memset(&blob, 0, sizeof(blob));

            /* Direction: 0-31 maps to 0-2*PI */
            float direction = (float)peak_dir_idx * (NIMCP_TWO_PI_F / 32.0f);
            /* Speed: 0-31 maps to min_motion_speed to 100 pixels/frame */
            float speed = bridge->config.min_motion_speed +
                         (float)peak_speed_idx * (100.0f / 32.0f);

            blob.velocity_x = speed * cosf(direction);
            blob.velocity_y = speed * sinf(direction);
            blob.salience = max_dir_response * max_speed_response;
            blob.contrast = (feature_dim > 96) ? features[96] : 0.5f;

            /* Apply attention filter if enabled and attention map provided */
            if (bridge->config.use_attention_filter && attention) {
                /* Find attention-weighted centroid */
                float weighted_x = 0.0f, weighted_y = 0.0f, total_weight = 0.0f;
                for (uint32_t i = 0; i < attention->num_peaks; i++) {
                    if (attention->peaks[i].salience > bridge->config.attention_threshold) {
                        weighted_x += attention->peaks[i].x * attention->peaks[i].salience;
                        weighted_y += attention->peaks[i].y * attention->peaks[i].salience;
                        total_weight += attention->peaks[i].salience;
                    }
                }
                if (total_weight > 0.0f) {
                    blob.center_x = weighted_x / total_weight;
                    blob.center_y = weighted_y / total_weight;
                }
            }

            blob.track_id = bridge->next_track_id++;
            blob.size_pixels = bridge->config.min_blob_size * 2.0f;  /* Default size */

            /* Inject this blob into the pipeline */
            if (bridge->current_result.num_blobs < VISUAL_BRIDGE_MAX_DETECTIONS) {
                bridge->current_result.blobs[bridge->current_result.num_blobs++] = blob;
                bridge->stats.blobs_detected++;
            }
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int dragonfly_visual_bridge_inject_blob(
    dragonfly_visual_bridge_t* bridge,
    const motion_blob_t* blob
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_visual_bridge_inject_blob: bridge is NULL");
        return -1;
    }
    if (!blob) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_visual_bridge_inject_blob: blob is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now = get_time_us();

    /* Add to current result */
    if (bridge->current_result.num_blobs < VISUAL_BRIDGE_MAX_DETECTIONS) {
        uint32_t idx = bridge->current_result.num_blobs;
        bridge->current_result.blobs[idx] = *blob;

        /* Assign track ID if not set */
        if (bridge->current_result.blobs[idx].track_id == 0) {
            bridge->current_result.blobs[idx].track_id = bridge->next_track_id++;
        }

        bridge->current_result.num_blobs++;
        bridge->stats.blobs_detected++;

        /* Update attention peak if this is most salient */
        if (blob->salience > bridge->current_result.peak_salience) {
            bridge->current_result.attention_peak_x = blob->center_x;
            bridge->current_result.attention_peak_y = blob->center_y;
            bridge->current_result.peak_salience = blob->salience;
        }
    }

    /* Prepare detection outside of lock if dragonfly is connected */
    dragonfly_detection_t det;
    bool send_detection = false;
    dragonfly_system_t* df = bridge->dragonfly;

    if (df) {
        memset(&det, 0, sizeof(det));

        det.id = blob->track_id;

        /* Convert pixel to 3D position (stateless, uses config snapshot) */
        float depth = (bridge->config.assumed_target_size_m *
                       bridge->config.calibration.focal_length) /
                      (blob->size_pixels > 0.0f ? blob->size_pixels : 1.0f);
        float nx = (blob->center_x - bridge->config.calibration.principal_x) /
                   bridge->config.calibration.focal_length;
        float ny = (blob->center_y - bridge->config.calibration.principal_y) /
                   bridge->config.calibration.focal_length;
        det.position[0] = nx * depth;
        det.position[1] = ny * depth;
        det.position[2] = depth;

        /* Compute motion direction and speed */
        float motion_speed = sqrtf(blob->velocity_x * blob->velocity_x +
                                   blob->velocity_y * blob->velocity_y);
        det.motion_direction_rad = atan2f(blob->velocity_y, blob->velocity_x);
        float radians_per_pixel = 1.0f / bridge->config.calibration.focal_length;
        float radians_per_frame = motion_speed * radians_per_pixel;
        det.motion_speed = radians_per_frame / bridge->config.frame_dt_s;

        det.size = blob->size_pixels / bridge->config.calibration.focal_length;
        det.contrast = blob->contrast;
        det.timestamp_us = now;

        bridge->stats.detections_sent++;
        send_detection = true;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Send detection OUTSIDE of lock to avoid deadlock */
    if (send_detection) {
        dragonfly_process_detection(df, &det);
    }

    return 0;
}

int dragonfly_visual_bridge_get_result(
    const dragonfly_visual_bridge_t* bridge,
    visual_motion_result_t* result
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_visual_bridge_get_result: bridge is NULL");
        return -1;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_visual_bridge_get_result: result is NULL");
        return -1;
    }

    nimcp_mutex_lock(((dragonfly_visual_bridge_t*)bridge)->base.mutex);
    *result = bridge->current_result;
    nimcp_mutex_unlock(((dragonfly_visual_bridge_t*)bridge)->base.mutex);

    return 0;
}

//=============================================================================
// Utility Functions
//=============================================================================

int dragonfly_visual_bridge_pixel_to_3d(
    const dragonfly_visual_bridge_t* bridge,
    float pixel_x,
    float pixel_y,
    float depth_m,
    float position[3]
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_visual_bridge_pixel_to_3d: bridge is NULL");
        return -1;
    }
    if (!position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_visual_bridge_pixel_to_3d: position is NULL");
        return -1;
    }
    if (depth_m <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_visual_bridge_pixel_to_3d: invalid depth");
        return -1;
    }

    const visual_calibration_t* cal = &bridge->config.calibration;

    /* Convert from pixel to normalized image coordinates */
    float nx = (pixel_x - cal->principal_x) / cal->focal_length;
    float ny = (pixel_y - cal->principal_y) / cal->focal_length;

    /* Project to 3D at given depth */
    position[0] = nx * depth_m;
    position[1] = ny * depth_m;
    position[2] = depth_m;

    return 0;
}

float dragonfly_visual_bridge_estimate_depth(
    const dragonfly_visual_bridge_t* bridge,
    float blob_size_pixels
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_visual_bridge_estimate_depth: bridge is NULL");
        return -1.0f;
    }
    if (blob_size_pixels <= 0.0f) return -1.0f;

    /* Estimate depth from angular size:
     * angular_size = actual_size / depth
     * pixel_size = angular_size * focal_length
     * depth = actual_size * focal_length / pixel_size
     */
    float depth = (bridge->config.assumed_target_size_m *
                   bridge->config.calibration.focal_length) / blob_size_pixels;

    return depth;
}

float dragonfly_visual_bridge_pixel_velocity_to_angular(
    const dragonfly_visual_bridge_t* bridge,
    float vel_pixels_per_frame
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_visual_bridge_pixel_velocity_to_angular: bridge is NULL");
        return 0.0f;
    }

    /* Convert pixels/frame to radians/second */
    float radians_per_pixel = 1.0f / bridge->config.calibration.focal_length;
    float radians_per_frame = vel_pixels_per_frame * radians_per_pixel;
    float radians_per_second = radians_per_frame / bridge->config.frame_dt_s;

    return radians_per_second;
}

//=============================================================================
// Statistics Functions
//=============================================================================

int dragonfly_visual_bridge_get_stats(
    const dragonfly_visual_bridge_t* bridge,
    visual_bridge_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_visual_bridge_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_visual_bridge_get_stats: stats is NULL");
        return -1;
    }

    nimcp_mutex_lock(((dragonfly_visual_bridge_t*)bridge)->base.mutex);

    *stats = bridge->stats;

    /* Calculate derived stats */
    if (stats->frames_processed > 0) {
        stats->avg_blobs_per_frame =
            (float)stats->blobs_detected / (float)stats->frames_processed;
    }

    nimcp_mutex_unlock(((dragonfly_visual_bridge_t*)bridge)->base.mutex);
    return 0;
}

int dragonfly_visual_bridge_reset_stats(dragonfly_visual_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_visual_bridge_reset_stats: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Configuration Functions
//=============================================================================

int dragonfly_visual_bridge_set_config(
    dragonfly_visual_bridge_t* bridge,
    const visual_bridge_config_t* config
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_visual_bridge_set_config: bridge is NULL");
        return -1;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_visual_bridge_set_config: config is NULL");
        return -1;
    }
    if (!visual_bridge_validate_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_visual_bridge_set_config: invalid config");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config = *config;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_visual_bridge_get_config(
    const dragonfly_visual_bridge_t* bridge,
    visual_bridge_config_t* config
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_visual_bridge_get_config: bridge is NULL");
        return -1;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_visual_bridge_get_config: config is NULL");
        return -1;
    }

    nimcp_mutex_lock(((dragonfly_visual_bridge_t*)bridge)->base.mutex);
    *config = bridge->config;
    nimcp_mutex_unlock(((dragonfly_visual_bridge_t*)bridge)->base.mutex);

    return 0;
}

int dragonfly_visual_bridge_set_calibration(
    dragonfly_visual_bridge_t* bridge,
    const visual_calibration_t* calibration
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_visual_bridge_set_calibration: bridge is NULL");
        return -1;
    }
    if (!calibration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_visual_bridge_set_calibration: calibration is NULL");
        return -1;
    }
    if (calibration->focal_length <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_visual_bridge_set_calibration: invalid focal_length");
        return -1;
    }
    if (calibration->image_width == 0 || calibration->image_height == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_visual_bridge_set_calibration: invalid image dimensions");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config.calibration = *calibration;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}
