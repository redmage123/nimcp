/**
 * @file nimcp_dragonfly_cnn_bridge.c
 * @brief Implementation of Dragonfly-to-CNN Training Bridge
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "dragonfly/nimcp_dragonfly_cnn_bridge.h"
#include "utils/rng/nimcp_rand.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_constants.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_cnn_bridge)

#define LOG_MODULE "DRAGONFLY_CNN_BRIDGE"


//=============================================================================
// Internal Structure
//=============================================================================

struct dragonfly_cnn_bridge_s {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    dragonfly_cnn_config_t config;
    dragonfly_system_t* dragonfly;
    void* cnn_trainer;

    /* Motion history */
    dragonfly_cnn_motion_history_t motion_history;

    /* Feature buffer */
    float* feature_buffer;
    uint32_t feature_buffer_size;

    /* Training state */
    bool is_training;
    float current_loss;
    uint64_t step_count;

    /* Statistics */
    dragonfly_cnn_stats_t stats;
};

//=============================================================================
// Configuration
//=============================================================================

int dragonfly_cnn_bridge_default_config(dragonfly_cnn_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cnn_bridge_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    config->task = CNN_TASK_MOTION_DETECTION;
    config->data_source = CNN_DATA_SYNTHETIC;
    config->feature_mode = CNN_FEATURE_RAW_FRAMES;

    config->frame_width = 64;
    config->frame_height = 64;
    config->motion_history_frames = 8;
    config->frame_sample_rate_hz = 60.0f;

    config->batch_size = 32;
    config->learning_rate = 0.001f;
    config->reward_scale = 1.0f;
    config->use_reward_shaping = true;

    config->augment_flip = true;
    config->augment_rotate = true;
    config->augment_scale = false;
    config->augment_noise = true;
    config->augment_probability = 0.5f;

    config->enable_snn_conversion = false;
    config->target_firing_rate = 100.0f;

    return 0;
}

int dragonfly_cnn_bridge_validate_config(const dragonfly_cnn_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cnn_bridge_validate_config: config is NULL");
        return -1;
    }
    if (config->task >= CNN_TASK_TYPE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cnn_bridge_validate_config: invalid task type");
        return -1;
    }
    if (config->frame_width == 0 || config->frame_height == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cnn_bridge_validate_config: config->frame_width is zero");
        return -1;
    }
    if (config->motion_history_frames > DRAGONFLY_CNN_MAX_FRAMES) {
        return -1;
    }
    if (config->batch_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cnn_bridge_validate_config: config->batch_size is zero");
        return -1;
    }
    if (config->learning_rate <= 0.0f) {
        return -1;
    }
    return 0;
}

//=============================================================================
// Lifecycle
//=============================================================================

dragonfly_cnn_bridge_t* dragonfly_cnn_bridge_create(
    dragonfly_system_t* dragonfly,
    void* cnn_trainer,
    const dragonfly_cnn_config_t* config
) {
    dragonfly_cnn_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "dragonfly_cnn_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        if (dragonfly_cnn_bridge_validate_config(config) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "dragonfly_cnn_bridge_create: invalid configuration");
            nimcp_free(bridge);
            return NULL;
        }
        bridge->config = *config;
    } else {
        dragonfly_cnn_bridge_default_config(&bridge->config);
    }

    bridge->dragonfly = dragonfly;
    bridge->cnn_trainer = cnn_trainer;

    /* Initialize motion history */
    bridge->motion_history.max_frames = bridge->config.motion_history_frames;
    bridge->motion_history.num_frames = 0;

    /* Allocate feature buffer */
    bridge->feature_buffer_size = DRAGONFLY_CNN_FEATURE_DIM;
    bridge->feature_buffer = nimcp_calloc(bridge->feature_buffer_size, sizeof(float));
    if (!bridge->feature_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "dragonfly_cnn_bridge_create: failed to allocate feature buffer");
        nimcp_free(bridge);
        return NULL;
    }

    bridge->is_training = false;
    bridge->current_loss = 0.0f;
    bridge->step_count = 0;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Initialize bridge base infrastructure (mutex + metrics) */
    if (bridge_base_init(&bridge->base, 0, "dragonfly_cnn") != 0) {
        nimcp_free(bridge->feature_buffer);
        nimcp_free(bridge);
        return NULL;
    }

    return bridge;
}

void dragonfly_cnn_bridge_destroy(dragonfly_cnn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "dragonfly_cnn");

    bridge_base_cleanup(&bridge->base);

    /* Free frame data in motion history */
    for (uint32_t i = 0; i < bridge->motion_history.num_frames; i++) {
        nimcp_free(bridge->motion_history.frames[i].data);
    }

    nimcp_free(bridge->feature_buffer);
    nimcp_free(bridge);
}

int dragonfly_cnn_bridge_reset(dragonfly_cnn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cnn_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Free frame data */
    for (uint32_t i = 0; i < bridge->motion_history.num_frames; i++) {
        nimcp_free(bridge->motion_history.frames[i].data);
        bridge->motion_history.frames[i].data = NULL;
    }
    bridge->motion_history.num_frames = 0;

    bridge->current_loss = 0.0f;
    bridge->step_count = 0;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return 0;
}

//=============================================================================
// Data Collection
//=============================================================================

int dragonfly_cnn_add_frame(
    dragonfly_cnn_bridge_t* bridge,
    const dragonfly_cnn_frame_t* frame
) {
    if (!bridge || !frame || !frame->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cnn_add_frame: required parameter is NULL (bridge, frame, frame->data)");
        return -1;
    }

    /* Shift frames if at capacity */
    if (bridge->motion_history.num_frames >= bridge->motion_history.max_frames) {
        nimcp_free(bridge->motion_history.frames[0].data);
        memmove(&bridge->motion_history.frames[0],
                &bridge->motion_history.frames[1],
                (bridge->motion_history.max_frames - 1) * sizeof(dragonfly_cnn_frame_t));
        bridge->motion_history.num_frames--;
    }

    /* Add new frame */
    uint32_t idx = bridge->motion_history.num_frames;
    uint32_t size = frame->width * frame->height * frame->channels;

    bridge->motion_history.frames[idx].data = nimcp_malloc(size * sizeof(float));
    if (!bridge->motion_history.frames[idx].data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dragonfly_cnn_add_frame: failed to allocate frame data");
        return -1;
    }

    memcpy(bridge->motion_history.frames[idx].data, frame->data, size * sizeof(float));
    bridge->motion_history.frames[idx].width = frame->width;
    bridge->motion_history.frames[idx].height = frame->height;
    bridge->motion_history.frames[idx].channels = frame->channels;
    bridge->motion_history.frames[idx].timestamp_ms = frame->timestamp_ms;

    bridge->motion_history.num_frames++;

    return 0;
}

int dragonfly_cnn_record_episode(
    dragonfly_cnn_bridge_t* bridge,
    bool success
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cnn_record_episode: bridge is NULL");
        return -1;
    }

    bridge->stats.avg_interception_reward =
        (bridge->stats.avg_interception_reward * bridge->stats.samples_processed +
         (success ? 1.0f : 0.0f)) /
        (bridge->stats.samples_processed + 1);

    bridge->stats.samples_processed++;

    return 0;
}

int dragonfly_cnn_extract_features(
    dragonfly_cnn_bridge_t* bridge,
    float* features,
    uint32_t feature_dim
) {
    if (!bridge || !features || feature_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cnn_extract_features: required parameter is NULL (bridge, features)");
        return -1;
    }

    uint32_t extracted = 0;
    uint32_t max_extract = feature_dim < DRAGONFLY_CNN_FEATURE_DIM ?
                           feature_dim : DRAGONFLY_CNN_FEATURE_DIM;

    switch (bridge->config.feature_mode) {
        case CNN_FEATURE_RAW_FRAMES:
            /* Extract from most recent frame */
            if (bridge->motion_history.num_frames > 0) {
                uint32_t idx = bridge->motion_history.num_frames - 1;
                dragonfly_cnn_frame_t* frame = &bridge->motion_history.frames[idx];
                uint32_t frame_size = frame->width * frame->height * frame->channels;
                extracted = max_extract < frame_size ? max_extract : frame_size;
                memcpy(features, frame->data, extracted * sizeof(float));
            }
            break;

        case CNN_FEATURE_MOTION_VECTORS:
            /* Compute motion from frame differences */
            if (bridge->motion_history.num_frames >= 2) {
                /* Simplified: compute mean difference */
                uint32_t prev = bridge->motion_history.num_frames - 2;
                uint32_t curr = bridge->motion_history.num_frames - 1;
                dragonfly_cnn_frame_t* prev_frame = &bridge->motion_history.frames[prev];
                dragonfly_cnn_frame_t* curr_frame = &bridge->motion_history.frames[curr];
                uint32_t size = curr_frame->width * curr_frame->height;
                extracted = max_extract < size ? max_extract : size;
                for (uint32_t i = 0; i < extracted; i++) {
                    features[i] = curr_frame->data[i] - prev_frame->data[i];
                }
            }
            break;

        case CNN_FEATURE_TSDN_ACTIVATIONS:
        case CNN_FEATURE_TRACKING_STATE:
        case CNN_FEATURE_HYBRID:
            /* Placeholder: would extract from dragonfly system */
            memset(features, 0, max_extract * sizeof(float));
            extracted = max_extract;
            break;
    }

    return (int)extracted;
}

int dragonfly_cnn_generate_sample(
    dragonfly_cnn_bridge_t* bridge,
    dragonfly_cnn_sample_t* sample
) {
    if (!bridge || !sample) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cnn_generate_sample: required parameter is NULL (bridge, sample)");
        return -1;
    }

    /* Generate synthetic sample */
    memset(sample, 0, sizeof(*sample));

    sample->weight = 1.0f;
    sample->class_label = 0;
    sample->interception_success = 0.0f;

    return 0;
}

//=============================================================================
// Training
//=============================================================================

float dragonfly_cnn_train_step(dragonfly_cnn_bridge_t* bridge) {
    if (!bridge || !bridge->is_training) return -1.0f;

    bridge->step_count++;
    bridge->stats.batches_processed++;

    /* Simulated training step */
    float loss = bridge->current_loss * NIMCP_EMA_DECAY_DEFAULT + NIMCP_LEARNING_RATE_DEFAULT * nimcp_rand_uniform();
    bridge->current_loss = loss;

    bridge->stats.current_loss = loss;
    bridge->stats.average_loss =
        (bridge->stats.average_loss * (bridge->stats.batches_processed - 1) + loss) /
        bridge->stats.batches_processed;

    if (loss < bridge->stats.min_loss || bridge->stats.min_loss == 0.0f) {
        bridge->stats.min_loss = loss;
    }

    return loss;
}

float dragonfly_cnn_train_batch(
    dragonfly_cnn_bridge_t* bridge,
    const dragonfly_cnn_sample_t* samples,
    uint32_t num_samples
) {
    if (!bridge || !samples || num_samples == 0) return -1.0f;

    float total_loss = 0.0f;
    for (uint32_t i = 0; i < num_samples; i++) {
        total_loss += dragonfly_cnn_train_step(bridge);
    }

    return total_loss / num_samples;
}

float dragonfly_cnn_evaluate(
    dragonfly_cnn_bridge_t* bridge,
    const dragonfly_cnn_sample_t* samples,
    uint32_t num_samples
) {
    if (!bridge || !samples || num_samples == 0) return -1.0f;

    /* Simulated evaluation */
    return bridge->current_loss * 1.1f;
}

int dragonfly_cnn_set_learning_rate(dragonfly_cnn_bridge_t* bridge, float lr) {
    if (!bridge || lr <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cnn_set_learning_rate: bridge is NULL or lr <= 0");
        return -1;
    }
    bridge->config.learning_rate = lr;
    return 0;
}

//=============================================================================
// Inference
//=============================================================================

int dragonfly_cnn_infer(
    dragonfly_cnn_bridge_t* bridge,
    float* output,
    uint32_t output_size
) {
    if (!bridge || !output || output_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cnn_infer: required parameter is NULL (bridge, output)");
        return -1;
    }

    /* Placeholder inference */
    memset(output, 0, output_size * sizeof(float));
    return (int)output_size;
}

float dragonfly_cnn_detect_motion(dragonfly_cnn_bridge_t* bridge) {
    if (!bridge) return -1.0f;
    if (bridge->motion_history.num_frames < 2) return 0.0f;

    /* Compute motion magnitude from frame difference */
    uint32_t prev = bridge->motion_history.num_frames - 2;
    uint32_t curr = bridge->motion_history.num_frames - 1;
    dragonfly_cnn_frame_t* prev_frame = &bridge->motion_history.frames[prev];
    dragonfly_cnn_frame_t* curr_frame = &bridge->motion_history.frames[curr];

    float motion = 0.0f;
    uint32_t size = curr_frame->width * curr_frame->height;
    for (uint32_t i = 0; i < size && i < 1024; i++) {
        float diff = curr_frame->data[i] - prev_frame->data[i];
        motion += diff * diff;
    }

    /* Normalize to [0,1] */
    motion = sqrtf(motion / (size > 1024 ? 1024 : size));
    return motion > 1.0f ? 1.0f : motion;
}

int dragonfly_cnn_estimate_velocity(
    dragonfly_cnn_bridge_t* bridge,
    float* vx,
    float* vy
) {
    if (!bridge || !vx || !vy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cnn_estimate_velocity: required parameter is NULL (bridge, vx, vy)");
        return -1;
    }

    *vx = 0.0f;
    *vy = 0.0f;

    if (bridge->motion_history.num_frames < 2) return 0;

    /* Simplified velocity estimation from motion flow */
    /* Would use optical flow in real implementation */
    *vx = 1.0f;
    *vy = 0.5f;

    return 0;
}

int dragonfly_cnn_predict_trajectory(
    dragonfly_cnn_bridge_t* bridge,
    float* predictions,
    uint32_t num_steps
) {
    if (!bridge || !predictions || num_steps == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cnn_predict_trajectory: required parameter is NULL (bridge, predictions)");
        return -1;
    }

    /* Placeholder: linear extrapolation */
    for (uint32_t i = 0; i < num_steps * 2; i += 2) {
        predictions[i] = (float)i * 0.1f;       /* x */
        predictions[i + 1] = (float)i * 0.05f;  /* y */
    }

    return 0;
}

//=============================================================================
// Integration
//=============================================================================

int dragonfly_cnn_connect_dragonfly(
    dragonfly_cnn_bridge_t* bridge,
    dragonfly_system_t* dragonfly
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cnn_connect_dragonfly: bridge is NULL");
        return -1;
    }
    bridge->dragonfly = dragonfly;
    return 0;
}

int dragonfly_cnn_connect_trainer(
    dragonfly_cnn_bridge_t* bridge,
    void* trainer
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cnn_connect_trainer: bridge is NULL");
        return -1;
    }
    bridge->cnn_trainer = trainer;
    return 0;
}

bool dragonfly_cnn_is_training(const dragonfly_cnn_bridge_t* bridge) {
    return bridge ? bridge->is_training : false;
}

int dragonfly_cnn_set_training(dragonfly_cnn_bridge_t* bridge, bool training) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cnn_set_training: bridge is NULL");
        return -1;
    }
    bridge->is_training = training;
    return 0;
}

//=============================================================================
// SNN Conversion
//=============================================================================

int dragonfly_cnn_prepare_snn_conversion(dragonfly_cnn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cnn_prepare_snn_conversion: bridge is NULL");
        return -1;
    }
    bridge->config.enable_snn_conversion = true;
    return 0;
}

int dragonfly_cnn_get_activation_stats(
    dragonfly_cnn_bridge_t* bridge,
    uint32_t layer_idx,
    float* mean,
    float* std
) {
    if (!bridge || !mean || !std) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cnn_get_activation_stats: required parameter is NULL (bridge, mean, std)");
        return -1;
    }

    /* Placeholder stats */
    *mean = 0.5f;
    *std = 0.2f;

    return 0;
}

//=============================================================================
// Statistics
//=============================================================================

int dragonfly_cnn_bridge_get_stats(
    const dragonfly_cnn_bridge_t* bridge,
    dragonfly_cnn_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cnn_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int dragonfly_cnn_bridge_reset_stats(dragonfly_cnn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cnn_bridge_reset_stats: bridge is NULL");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return 0;
}

//=============================================================================
// Utility
//=============================================================================

const char* dragonfly_cnn_task_name(dragonfly_cnn_task_t task) {
    switch (task) {
        case CNN_TASK_MOTION_DETECTION:     return "motion_detection";
        case CNN_TASK_VELOCITY_ESTIMATION:  return "velocity_estimation";
        case CNN_TASK_TARGET_CLASSIFICATION: return "target_classification";
        case CNN_TASK_SALIENCY_PREDICTION:  return "saliency_prediction";
        case CNN_TASK_TRAJECTORY_PREDICTION: return "trajectory_prediction";
        case CNN_TASK_EVASION_DETECTION:    return "evasion_detection";
        default:                             return "unknown";
    }
}

const char* dragonfly_cnn_feature_mode_name(dragonfly_cnn_feature_mode_t mode) {
    switch (mode) {
        case CNN_FEATURE_RAW_FRAMES:        return "raw_frames";
        case CNN_FEATURE_MOTION_VECTORS:    return "motion_vectors";
        case CNN_FEATURE_TSDN_ACTIVATIONS:  return "tsdn_activations";
        case CNN_FEATURE_TRACKING_STATE:    return "tracking_state";
        case CNN_FEATURE_HYBRID:            return "hybrid";
        default:                             return "unknown";
    }
}
