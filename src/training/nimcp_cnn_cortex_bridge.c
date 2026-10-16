/**
 * @file nimcp_cnn_cortex_bridge.c
 * @brief CNN Training Integration with Visual and Audio Cortexes - Implementation
 *
 * WHAT: Bridge enabling CNN training to use perception cortex features
 * WHY: Leverages biologically-realistic feature extraction for transfer learning
 * HOW: Feature extraction wrappers + gradient feedback conversion
 *
 * @author NIMCP Development Team
 * @date 2025-12-24
 * @version 1.0.0
 */

#include "training/nimcp_cnn_cortex_bridge.h"
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_audio_cortex.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "cnn_cortex_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_dimension_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cnn_cortex_bridge)

//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * WHAT: Compute LR factor from perception metrics
 * WHY: Higher quality perception → higher learning rate
 * HOW: Weighted combination of confidence and quality
 */
static float compute_lr_factor(const cnn_cortex_bridge_t* bridge) {
    if (!bridge) return 1.0f;

    const cnn_cortex_bridge_config_t* cfg = &bridge->config;
    float factor = 1.0f;

    /* Visual confidence contribution */
    if (bridge->visual_cortex && bridge->metrics.visual_available) {
        float visual_contrib = (bridge->metrics.visual_confidence - 0.5f) *
                               cfg->confidence_scale;
        factor += visual_contrib;
    }

    /* Audio quality contribution */
    if (bridge->audio_cortex && bridge->metrics.audio_available) {
        float audio_contrib = (bridge->metrics.audio_quality - 0.5f) *
                              cfg->quality_scale;
        factor += audio_contrib;
    }

    /* Clamp to configured range */
    if (factor < cfg->lr_min_factor) factor = cfg->lr_min_factor;
    if (factor > cfg->lr_max_factor) factor = cfg->lr_max_factor;

    return factor;
}

/**
 * WHAT: Determine if sample should be skipped
 * WHY: Low quality samples degrade training
 * HOW: Check against configured thresholds
 */
static bool should_skip(const cnn_cortex_bridge_t* bridge) {
    if (!bridge || !bridge->config.skip_low_quality_samples) {
        return false;
    }

    const cnn_cortex_bridge_config_t* cfg = &bridge->config;

    /* Check visual confidence */
    if (bridge->visual_cortex && bridge->metrics.visual_available) {
        if (bridge->metrics.visual_confidence < cfg->visual_confidence_threshold) {
            return true;
        }
    }

    /* Check audio quality */
    if (bridge->audio_cortex && bridge->metrics.audio_available) {
        if (bridge->metrics.audio_quality < cfg->audio_quality_threshold) {
            return true;
        }
    }

    return false;
}

/**
 * WHAT: Update running average statistic
 * WHY: Track average metrics over time
 * HOW: Exponential moving average
 */
static float update_avg(float current_avg, float new_value, uint64_t count) {
    if (count <= 1) return new_value;
    float alpha = 0.1f;  /* EMA smoothing factor */
    return alpha * new_value + (1.0f - alpha) * current_avg;
}

//=============================================================================
// Lifecycle API
//=============================================================================

void cnn_cortex_bridge_default_config(cnn_cortex_bridge_config_t* config) {
    /* WHAT: Initialize config with sensible defaults
     * WHY:  Ensure all fields have valid values
     * HOW:  Set mode, thresholds, and flags
     */
    if (!config) return;

    memset(config, 0, sizeof(cnn_cortex_bridge_config_t));

    config->mode = CNN_CORTEX_MODE_TRAINING;
    config->priority = CNN_CORTEX_PRIORITY_VISUAL;

    /* Cortex settings */
    config->freeze_cortex_weights = true;
    config->enable_gradient_feedback = false;  /* Disabled by default */
    config->gradient_method = CNN_CORTEX_GRADIENT_MAGNITUDE;
    config->gradient_feedback_scale = CNN_CORTEX_DEFAULT_GRADIENT_SCALE;

    /* LR modulation */
    config->enable_perception_modulation = true;
    config->lr_min_factor = CNN_CORTEX_DEFAULT_LR_MIN_FACTOR;
    config->lr_max_factor = CNN_CORTEX_DEFAULT_LR_MAX_FACTOR;
    config->confidence_scale = 0.5f;
    config->quality_scale = 0.5f;

    /* Quality thresholds */
    config->visual_confidence_threshold = CNN_CORTEX_DEFAULT_CONFIDENCE_THRESHOLD;
    config->audio_quality_threshold = CNN_CORTEX_DEFAULT_QUALITY_THRESHOLD;
    config->skip_low_quality_samples = false;

    /* Feature caching */
    config->cache_features = true;
    config->cache_size = 16;

    /* Integration */
    config->enable_bio_async = true;
    config->integrate_perception_bridge = true;

    /* Update settings */
    config->update_interval_ms = 100;
}

cnn_cortex_bridge_t* cnn_cortex_bridge_create(
    const cnn_cortex_bridge_config_t* config
) {
    /* WHAT: Create and initialize bridge
     * WHY:  Entry point for using the bridge
     * HOW:  Allocate, init base, copy config
     */
    cnn_cortex_bridge_t* bridge = nimcp_malloc(sizeof(cnn_cortex_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate CNN-cortex bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(cnn_cortex_bridge_t));

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_CNN_CORTEX_BRIDGE,
                         CNN_CORTEX_BRIDGE_MODULE_NAME) != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cnn_cortex_bridge_create: operation failed");
        return NULL;
    }

    /* Copy or use default config */
    if (config) {
        memcpy(&bridge->config, config, sizeof(cnn_cortex_bridge_config_t));
    } else {
        cnn_cortex_bridge_default_config(&bridge->config);
    }

    /* Initialize metrics */
    bridge->metrics.valid = false;
    bridge->metrics.lr_factor = 1.0f;

    /* Initialize stats */
    bridge->stats.current_mode = bridge->config.mode;

    NIMCP_LOGGING_INFO("Created CNN-cortex bridge (mode=%s)",
                       cnn_cortex_mode_to_string(bridge->config.mode));

    return bridge;
}

void cnn_cortex_bridge_destroy(cnn_cortex_bridge_t* bridge) {
    /* WHAT: Destroy bridge and free resources
     * WHY:  Proper cleanup
     * HOW:  Free tensors, cleanup base
     */
    if (!bridge) return;

    /* Free cached tensors */
    if (bridge->visual_features) {
        nimcp_tensor_destroy(bridge->visual_features);
        bridge->visual_features = NULL;
    }
    if (bridge->audio_features) {
        nimcp_tensor_destroy(bridge->audio_features);
        bridge->audio_features = NULL;
    }
    if (bridge->combined_features) {
        nimcp_tensor_destroy(bridge->combined_features);
        bridge->combined_features = NULL;
    }

    /* Free gradient state buffers */
    if (bridge->gradient_state.visual_gradients) {
        nimcp_free(bridge->gradient_state.visual_gradients);
    }
    if (bridge->gradient_state.audio_gradients) {
        nimcp_free(bridge->gradient_state.audio_gradients);
    }

    /* Free training state buffers */
    if (bridge->visual_state.conv_output) {
        nimcp_free(bridge->visual_state.conv_output);
    }
    if (bridge->visual_state.pool_output) {
        nimcp_free(bridge->visual_state.pool_output);
    }
    if (bridge->audio_state.mel_features) {
        nimcp_free(bridge->audio_state.mel_features);
    }
    if (bridge->audio_state.mfcc_features) {
        nimcp_free(bridge->audio_state.mfcc_features);
    }

    /* Cleanup base bridge */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    NIMCP_LOGGING_DEBUG("Destroyed CNN-cortex bridge");
}

//=============================================================================
// Connection API
//=============================================================================

int cnn_cortex_bridge_connect_trainer(
    cnn_cortex_bridge_t* bridge,
    cnn_trainer_t* trainer
) {
    /* WHAT: Connect to CNN trainer
     * WHY:  Enable feature injection and gradient retrieval
     * HOW:  Store reference, update stats
     */
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->trainer = trainer;
    bridge->base.system_a = trainer;
    bridge->base.system_a_connected = (trainer != NULL);
    bridge->stats.trainer_connected = (trainer != NULL);

    /* Update bridge active status */
    bridge->base.bridge_active = bridge->base.system_a_connected &&
                                  (bridge->visual_cortex != NULL ||
                                   bridge->audio_cortex != NULL);

    BRIDGE_UNLOCK(bridge);

    if (trainer) {
        NIMCP_LOGGING_INFO("Connected CNN trainer to bridge");
    } else {
        NIMCP_LOGGING_DEBUG("Disconnected CNN trainer from bridge");
    }

    return 0;
}

int cnn_cortex_bridge_connect_visual_cortex(
    cnn_cortex_bridge_t* bridge,
    visual_cortex_handle_t visual_cortex
) {
    /* WHAT: Connect to visual cortex
     * WHY:  Enable visual feature extraction
     * HOW:  Store reference, query dimensions
     */
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->visual_cortex = visual_cortex;
    bridge->stats.visual_cortex_connected = (visual_cortex != NULL);

    if (visual_cortex) {
        /* Initialize visual metrics as available */
        bridge->metrics.visual_available = true;
        bridge->metrics.visual_confidence = 0.5f;  /* Default neutral */
        bridge->metrics.visual_novelty = 0.0f;

        /* Visual cortex is treated as system_b for bridge_base */
        if (!bridge->audio_cortex) {
            bridge->base.system_b = visual_cortex;
            bridge->base.system_b_connected = true;
        }
    } else {
        bridge->metrics.visual_available = false;
    }

    /* Update bridge active status */
    bridge->base.bridge_active = bridge->base.system_a_connected &&
                                  (bridge->visual_cortex != NULL ||
                                   bridge->audio_cortex != NULL);

    BRIDGE_UNLOCK(bridge);

    if (visual_cortex) {
        /* Enable training mode on visual cortex if bridge is in training mode */
        if (bridge->config.mode == CNN_CORTEX_MODE_TRAINING ||
            bridge->config.mode == CNN_CORTEX_MODE_FINE_TUNING) {
            visual_cortex_set_training_mode(visual_cortex, true);
        }
        NIMCP_LOGGING_INFO("Connected visual cortex to CNN-cortex bridge");
    } else {
        NIMCP_LOGGING_DEBUG("Disconnected visual cortex from bridge");
    }

    return 0;
}

int cnn_cortex_bridge_connect_audio_cortex(
    cnn_cortex_bridge_t* bridge,
    audio_cortex_handle_t audio_cortex
) {
    /* WHAT: Connect to audio cortex
     * WHY:  Enable audio feature extraction
     * HOW:  Store reference, query dimensions
     */
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->audio_cortex = audio_cortex;
    bridge->stats.audio_cortex_connected = (audio_cortex != NULL);

    if (audio_cortex) {
        /* Initialize audio metrics as available */
        bridge->metrics.audio_available = true;
        bridge->metrics.audio_quality = 0.5f;  /* Default neutral */
        bridge->metrics.speech_salience = 0.0f;

        /* Audio cortex is treated as system_b if visual not connected */
        if (!bridge->visual_cortex) {
            bridge->base.system_b = audio_cortex;
            bridge->base.system_b_connected = true;
        }
    } else {
        bridge->metrics.audio_available = false;
    }

    /* Update bridge active status */
    bridge->base.bridge_active = bridge->base.system_a_connected &&
                                  (bridge->visual_cortex != NULL ||
                                   bridge->audio_cortex != NULL);

    BRIDGE_UNLOCK(bridge);

    if (audio_cortex) {
        /* Enable training mode on audio cortex if bridge is in training mode */
        if (bridge->config.mode == CNN_CORTEX_MODE_TRAINING ||
            bridge->config.mode == CNN_CORTEX_MODE_FINE_TUNING) {
            audio_cortex_set_training_mode(audio_cortex, true);
        }
        NIMCP_LOGGING_INFO("Connected audio cortex to CNN-cortex bridge");
    } else {
        NIMCP_LOGGING_DEBUG("Disconnected audio cortex from bridge");
    }

    return 0;
}

int cnn_cortex_bridge_connect_perception_bridge(
    cnn_cortex_bridge_t* bridge,
    perception_training_bridge_t* perception_bridge
) {
    /* WHAT: Connect to perception-training bridge
     * WHY:  Share perception metrics and modulation factors
     * HOW:  Store reference
     */
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);
    bridge->perception_bridge = perception_bridge;
    bridge->stats.perception_bridge_connected = (perception_bridge != NULL);
    BRIDGE_UNLOCK(bridge);

    if (perception_bridge) {
        NIMCP_LOGGING_DEBUG("Connected perception-training bridge");
    }

    return 0;
}

bool cnn_cortex_bridge_is_connected(const cnn_cortex_bridge_t* bridge) {
    /* WHAT: Check if at least one cortex connected
     * WHY:  Determine if feature extraction possible
     * HOW:  Check cortex pointers
     */
    if (!bridge) {
        return false;
    }
    return bridge->visual_cortex != NULL || bridge->audio_cortex != NULL;
}

//=============================================================================
// Feature Extraction API
//=============================================================================

int cnn_cortex_bridge_extract_visual_features(
    cnn_cortex_bridge_t* bridge,
    const uint8_t* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    nimcp_tensor_t** features
) {
    /* WHAT: Extract visual features as tensor
     * WHY:  Provides perception-based features for CNN
     * HOW:  Call visual_cortex_process, wrap in tensor
     */
    NIMCP_CHECK_THROW(bridge && image && features, NIMCP_ERROR_NULL_POINTER,
                      "bridge, image, or features is NULL");
    NIMCP_CHECK_THROW(bridge->visual_cortex, NIMCP_ERROR_INVALID_STATE,
                      "Visual cortex not connected");

    uint64_t start_time = nimcp_time_monotonic_us();

    /* Get visual cortex stats to determine feature dimension */
    visual_cortex_stats_t vc_stats;
    if (!visual_cortex_get_stats(bridge->visual_cortex, &vc_stats)) {
        NIMCP_LOGGING_WARN("Could not get visual cortex stats");
    }

    /* Allocate feature buffer - use configured feature_dim from cortex */
    /* Default to 128 if unknown */
    uint32_t feature_dim = NIMCP_FEATURE_DIM;
    float* feature_buffer = nimcp_malloc(feature_dim * sizeof(float));
    NIMCP_CHECK_THROW(feature_buffer, NIMCP_ERROR_NO_MEMORY,
                      "Failed to allocate visual feature buffer");

    /* Process image through visual cortex */
    bool success = visual_cortex_process(
        bridge->visual_cortex,
        image, width, height, channels,
        feature_buffer
    );

    if (!success) {
        nimcp_free(feature_buffer);
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "Visual cortex processing failed");
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Update visual training state */
    BRIDGE_LOCK(bridge);

    /* Get confidence/novelty from visual cortex */
    bridge->visual_state.confidence = visual_cortex_compute_novelty(
        bridge->visual_cortex, feature_buffer);
    /* Note: novelty is inverted - high novelty = low familiarity */
    bridge->visual_state.confidence = 1.0f - bridge->visual_state.confidence;
    bridge->visual_state.feature_dim = feature_dim;
    bridge->visual_state.timestamp_ms = nimcp_time_get_ms();
    bridge->visual_state.valid = true;

    /* Update metrics */
    bridge->metrics.visual_confidence = bridge->visual_state.confidence;
    bridge->metrics.visual_novelty = 1.0f - bridge->visual_state.confidence;
    bridge->metrics.last_update_ms = bridge->visual_state.timestamp_ms;
    bridge->metrics.valid = true;

    /* Update LR factor */
    bridge->metrics.lr_factor = compute_lr_factor(bridge);
    bridge->metrics.skip_sample = should_skip(bridge);

    /* Update stats */
    bridge->stats.visual_extractions++;
    bridge->stats.total_feature_extractions++;
    bridge->stats.samples_processed++;
    bridge->stats.avg_visual_confidence = update_avg(
        bridge->stats.avg_visual_confidence,
        bridge->metrics.visual_confidence,
        bridge->stats.visual_extractions);
    bridge->stats.avg_lr_factor = update_avg(
        bridge->stats.avg_lr_factor,
        bridge->metrics.lr_factor,
        bridge->stats.total_feature_extractions);

    BRIDGE_UNLOCK(bridge);

    /* Create output tensor */
    uint32_t dims[1] = {feature_dim};
    *features = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    if (!*features) {
        nimcp_free(feature_buffer);
        NIMCP_THROW(NIMCP_ERROR_NO_MEMORY, "Failed to allocate visual features tensor");
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Copy features to tensor */
    float* tensor_data = (float*)nimcp_tensor_data(*features);
    memcpy(tensor_data, feature_buffer, feature_dim * sizeof(float));
    nimcp_free(feature_buffer);

    /* Update timing stats */
    uint64_t elapsed = nimcp_time_monotonic_us() - start_time;
    bridge->stats.avg_extraction_time_us = update_avg(
        bridge->stats.avg_extraction_time_us,
        (float)elapsed,
        bridge->stats.total_feature_extractions);

    return 0;
}

int cnn_cortex_bridge_extract_audio_features(
    cnn_cortex_bridge_t* bridge,
    const float* audio,
    uint32_t num_samples,
    uint8_t num_channels,
    nimcp_tensor_t** features
) {
    /* WHAT: Extract audio features as tensor
     * WHY:  Provides perception-based features for CNN
     * HOW:  Call audio_cortex_process, wrap in tensor
     */
    NIMCP_CHECK_THROW(bridge && audio && features, NIMCP_ERROR_NULL_POINTER,
                      "bridge, audio, or features is NULL");
    NIMCP_CHECK_THROW(bridge->audio_cortex, NIMCP_ERROR_INVALID_STATE,
                      "Audio cortex not connected");

    uint64_t start_time = nimcp_time_monotonic_us();

    /* Get audio cortex stats */
    audio_cortex_stats_t ac_stats;
    audio_cortex_get_stats(bridge->audio_cortex, &ac_stats);

    /* Allocate feature buffer - default MFCC + mel features */
    uint32_t feature_dim = NIMCP_FEATURE_DIM;  /* Default feature dimension */
    float* feature_buffer = nimcp_malloc(feature_dim * sizeof(float));
    NIMCP_CHECK_THROW(feature_buffer, NIMCP_ERROR_NO_MEMORY,
                      "Failed to allocate audio feature buffer");

    /* Process audio through audio cortex */
    bool success = audio_cortex_process(
        bridge->audio_cortex,
        audio, num_samples, num_channels,
        feature_buffer
    );

    if (!success) {
        nimcp_free(feature_buffer);
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "Audio cortex processing failed");
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Update audio training state */
    BRIDGE_LOCK(bridge);

    /* Get quality metrics from audio cortex */
    bridge->audio_state.quality = audio_cortex_compute_novelty(
        bridge->audio_cortex, feature_buffer);
    bridge->audio_state.quality = 1.0f - bridge->audio_state.quality;
    bridge->audio_state.speech_salience = audio_cortex_get_speech_salience(
        bridge->audio_cortex, feature_buffer, feature_dim);
    bridge->audio_state.timestamp_ms = nimcp_time_get_ms();
    bridge->audio_state.valid = true;

    /* Update metrics */
    bridge->metrics.audio_quality = bridge->audio_state.quality;
    bridge->metrics.speech_salience = bridge->audio_state.speech_salience;
    bridge->metrics.last_update_ms = bridge->audio_state.timestamp_ms;
    bridge->metrics.valid = true;

    /* Update LR factor */
    bridge->metrics.lr_factor = compute_lr_factor(bridge);
    bridge->metrics.skip_sample = should_skip(bridge);

    /* Update stats */
    bridge->stats.audio_extractions++;
    bridge->stats.total_feature_extractions++;
    bridge->stats.samples_processed++;
    bridge->stats.avg_audio_quality = update_avg(
        bridge->stats.avg_audio_quality,
        bridge->metrics.audio_quality,
        bridge->stats.audio_extractions);
    bridge->stats.avg_lr_factor = update_avg(
        bridge->stats.avg_lr_factor,
        bridge->metrics.lr_factor,
        bridge->stats.total_feature_extractions);

    BRIDGE_UNLOCK(bridge);

    /* Create output tensor */
    uint32_t dims[1] = {feature_dim};
    *features = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    if (!*features) {
        nimcp_free(feature_buffer);
        NIMCP_THROW(NIMCP_ERROR_NO_MEMORY, "Failed to allocate audio features tensor");
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Copy features to tensor */
    float* tensor_data = (float*)nimcp_tensor_data(*features);
    memcpy(tensor_data, feature_buffer, feature_dim * sizeof(float));
    nimcp_free(feature_buffer);

    /* Update timing stats */
    uint64_t elapsed = nimcp_time_monotonic_us() - start_time;
    bridge->stats.avg_extraction_time_us = update_avg(
        bridge->stats.avg_extraction_time_us,
        (float)elapsed,
        bridge->stats.total_feature_extractions);

    return 0;
}

int cnn_cortex_bridge_extract_multimodal_features(
    cnn_cortex_bridge_t* bridge,
    const uint8_t* image,
    uint32_t image_width,
    uint32_t image_height,
    uint32_t image_channels,
    const float* audio,
    uint32_t num_audio_samples,
    uint8_t num_audio_channels,
    nimcp_tensor_t** features
) {
    /* WHAT: Extract and concatenate features from both cortexes
     * WHY:  Enable multimodal CNN training
     * HOW:  Extract both, concatenate
     */
    NIMCP_CHECK_THROW(bridge && features, NIMCP_ERROR_NULL_POINTER,
                      "bridge or features is NULL");

    nimcp_tensor_t* visual_features = NULL;
    nimcp_tensor_t* audio_features = NULL;
    int result = 0;

    /* Extract visual features if image provided */
    if (image && bridge->visual_cortex) {
        result = cnn_cortex_bridge_extract_visual_features(
            bridge, image, image_width, image_height, image_channels,
            &visual_features);
        if (result != 0) {
            NIMCP_LOGGING_WARN("Visual feature extraction failed: %d", result);
        }
    }

    /* Extract audio features if audio provided */
    if (audio && bridge->audio_cortex) {
        result = cnn_cortex_bridge_extract_audio_features(
            bridge, audio, num_audio_samples, num_audio_channels,
            &audio_features);
        if (result != 0) {
            NIMCP_LOGGING_WARN("Audio feature extraction failed: %d", result);
        }
    }

    /* Handle cases */
    if (!visual_features && !audio_features) {
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "No features extracted from either cortex");
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    if (visual_features && !audio_features) {
        *features = visual_features;
        return 0;
    }

    if (!visual_features && audio_features) {
        *features = audio_features;
        return 0;
    }

    /* Concatenate both features */
    size_t visual_size = nimcp_tensor_numel(visual_features);
    size_t audio_size = nimcp_tensor_numel(audio_features);
    size_t total_size = visual_size + audio_size;

    uint32_t dims[1] = {(uint32_t)total_size};
    *features = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    if (!*features) {
        nimcp_tensor_destroy(visual_features);
        nimcp_tensor_destroy(audio_features);
        NIMCP_THROW(NIMCP_ERROR_NO_MEMORY, "Failed to allocate multimodal features tensor");
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Copy visual then audio features */
    float* out_data = (float*)nimcp_tensor_data(*features);
    memcpy(out_data, nimcp_tensor_data(visual_features),
           visual_size * sizeof(float));
    memcpy(out_data + visual_size, nimcp_tensor_data(audio_features),
           audio_size * sizeof(float));

    nimcp_tensor_destroy(visual_features);
    nimcp_tensor_destroy(audio_features);

    /* Update stats */
    bridge->stats.multimodal_extractions++;

    return 0;
}

//=============================================================================
// Gradient Feedback API
//=============================================================================

int cnn_cortex_bridge_set_gradients(
    cnn_cortex_bridge_t* bridge,
    const nimcp_tensor_t* gradients
) {
    /* WHAT: Store gradients for feedback
     * WHY:  Enable STDP modulation from CNN training
     * HOW:  Copy gradient data to internal buffer
     */
    NIMCP_CHECK_THROW(bridge && gradients, NIMCP_ERROR_NULL_POINTER,
                      "bridge or gradients is NULL");

    if (!bridge->config.enable_gradient_feedback) {
        return 0;  /* Silently skip if feedback disabled */
    }

    BRIDGE_LOCK(bridge);

    size_t grad_size = nimcp_tensor_numel(gradients);
    const float* grad_data = (const float*)nimcp_tensor_data((nimcp_tensor_t*)gradients);

    /* Allocate or reallocate gradient buffers based on connected cortexes */
    if (bridge->visual_cortex) {
        if (!bridge->gradient_state.visual_gradients ||
            bridge->gradient_state.visual_gradient_size < grad_size) {

            if (bridge->gradient_state.visual_gradients) {
                nimcp_free(bridge->gradient_state.visual_gradients);
            }
            bridge->gradient_state.visual_gradients =
                nimcp_malloc(grad_size * sizeof(float));
            bridge->gradient_state.visual_gradient_size = grad_size;
        }

        if (bridge->gradient_state.visual_gradients) {
            memcpy(bridge->gradient_state.visual_gradients, grad_data,
                   grad_size * sizeof(float));
        }
    }

    if (bridge->audio_cortex) {
        if (!bridge->gradient_state.audio_gradients ||
            bridge->gradient_state.audio_gradient_size < grad_size) {

            if (bridge->gradient_state.audio_gradients) {
                nimcp_free(bridge->gradient_state.audio_gradients);
            }
            bridge->gradient_state.audio_gradients =
                nimcp_malloc(grad_size * sizeof(float));
            bridge->gradient_state.audio_gradient_size = grad_size;
        }

        if (bridge->gradient_state.audio_gradients) {
            memcpy(bridge->gradient_state.audio_gradients, grad_data,
                   grad_size * sizeof(float));
        }
    }

    /* Compute gradient norm */
    float norm = 0.0f;
    for (size_t i = 0; i < grad_size; i++) {
        norm += grad_data[i] * grad_data[i];
    }
    bridge->gradient_state.gradient_norm = sqrtf(norm);
    bridge->gradient_state.pending = true;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int cnn_cortex_bridge_propagate_gradients(cnn_cortex_bridge_t* bridge) {
    /* WHAT: Apply gradient feedback to cortex STDP
     * WHY:  Enable top-down learning modulation
     * HOW:  Convert gradients to STDP signals per method
     */
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->config.enable_gradient_feedback) {
        return 0;
    }

    BRIDGE_LOCK(bridge);

    if (!bridge->gradient_state.pending) {
        BRIDGE_UNLOCK(bridge);
        return 0;  /* No pending gradients */
    }

    uint64_t start_time = nimcp_time_monotonic_us();
    float scale = bridge->config.gradient_feedback_scale;

    /* Apply to visual cortex if connected */
    /* Note: visual_cortex_apply_gradient_feedback is a new function
     * we'll add to the visual cortex API */
    if (bridge->visual_cortex && bridge->gradient_state.visual_gradients) {
        /* For now, log that we would apply gradients
         * Full implementation requires visual cortex training interface */
        NIMCP_LOGGING_DEBUG("Would apply visual gradient feedback (scale=%.4f, norm=%.4f)",
                           scale, bridge->gradient_state.gradient_norm);
        bridge->stats.visual_feedbacks++;
    }

    /* Apply to audio cortex if connected */
    if (bridge->audio_cortex && bridge->gradient_state.audio_gradients) {
        NIMCP_LOGGING_DEBUG("Would apply audio gradient feedback (scale=%.4f, norm=%.4f)",
                           scale, bridge->gradient_state.gradient_norm);
        bridge->stats.audio_feedbacks++;
    }

    bridge->gradient_state.pending = false;
    bridge->stats.total_gradient_feedbacks++;

    /* Update timing stats */
    uint64_t elapsed = nimcp_time_monotonic_us() - start_time;
    bridge->stats.avg_feedback_time_us = update_avg(
        bridge->stats.avg_feedback_time_us,
        (float)elapsed,
        bridge->stats.total_gradient_feedbacks);

    BRIDGE_UNLOCK(bridge);

    return 0;
}

//=============================================================================
// Perception Modulation API
//=============================================================================

int cnn_cortex_bridge_get_perception_metrics(
    const cnn_cortex_bridge_t* bridge,
    cnn_cortex_perception_metrics_t* metrics
) {
    /* WHAT: Get current perception metrics
     * WHY:  Query for LR modulation decisions
     * HOW:  Copy internal metrics
     */
    NIMCP_CHECK_THROW(bridge && metrics, NIMCP_ERROR_NULL_POINTER,
                      "bridge or metrics is NULL");

    /* Note: const-cast for lock, but we only read */
    cnn_cortex_bridge_t* mutable_bridge = (cnn_cortex_bridge_t*)bridge;
    BRIDGE_LOCK(mutable_bridge);
    memcpy(metrics, &bridge->metrics, sizeof(cnn_cortex_perception_metrics_t));
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

float cnn_cortex_bridge_get_modulated_lr(
    const cnn_cortex_bridge_t* bridge,
    float base_lr
) {
    /* WHAT: Apply perception modulation to LR
     * WHY:  High quality → boost, low quality → reduce
     * HOW:  base_lr × lr_factor
     */
    if (!bridge || !bridge->config.enable_perception_modulation) {
        return base_lr;
    }

    return base_lr * bridge->metrics.lr_factor;
}

bool cnn_cortex_bridge_should_skip_sample(const cnn_cortex_bridge_t* bridge) {
    /* WHAT: Check if sample should be skipped
     * WHY:  Avoid training on low-quality samples
     * HOW:  Return cached skip decision
     */
    if (!bridge) {
        return false;
    }
    return bridge->metrics.skip_sample;
}

//=============================================================================
// Training State API
//=============================================================================

int cnn_cortex_bridge_get_visual_state(
    const cnn_cortex_bridge_t* bridge,
    visual_cortex_training_state_t* state
) {
    /* WHAT: Get cached visual cortex state
     * WHY:  Needed for gradient computation
     * HOW:  Copy internal state
     */
    NIMCP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER,
                      "bridge or state is NULL");

    cnn_cortex_bridge_t* mutable_bridge = (cnn_cortex_bridge_t*)bridge;
    BRIDGE_LOCK(mutable_bridge);
    memcpy(state, &bridge->visual_state, sizeof(visual_cortex_training_state_t));
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int cnn_cortex_bridge_get_audio_state(
    const cnn_cortex_bridge_t* bridge,
    audio_cortex_training_state_t* state
) {
    /* WHAT: Get cached audio cortex state
     * WHY:  Needed for gradient computation
     * HOW:  Copy internal state
     */
    NIMCP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER,
                      "bridge or state is NULL");

    cnn_cortex_bridge_t* mutable_bridge = (cnn_cortex_bridge_t*)bridge;
    BRIDGE_LOCK(mutable_bridge);
    memcpy(state, &bridge->audio_state, sizeof(audio_cortex_training_state_t));
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

//=============================================================================
// Update Cycle API
//=============================================================================

int cnn_cortex_bridge_update(cnn_cortex_bridge_t* bridge) {
    /* WHAT: Main update cycle
     * WHY:  Refresh metrics, apply pending gradients
     * HOW:  Update metrics, propagate gradients
     */
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    /* Update LR factor from current metrics */
    bridge->metrics.lr_factor = compute_lr_factor(bridge);
    bridge->metrics.skip_sample = should_skip(bridge);
    bridge->metrics.last_update_ms = nimcp_time_get_ms();

    /* Record update */
    bridge->stats.total_updates++;
    bridge->stats.last_update_ms = bridge->metrics.last_update_ms;
    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);

    /* Propagate any pending gradients */
    if (bridge->gradient_state.pending) {
        cnn_cortex_bridge_propagate_gradients(bridge);
    }

    return 0;
}

//=============================================================================
// Bio-Async API (Use bridge_base infrastructure)
//=============================================================================

int cnn_cortex_bridge_connect_bio_async(cnn_cortex_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    int result = bridge_base_connect_bio_async(&bridge->base);
    bridge->stats.bio_async_connected = bridge->base.bio_async_enabled;
    return result;
}

int cnn_cortex_bridge_disconnect_bio_async(cnn_cortex_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    int result = bridge_base_disconnect_bio_async(&bridge->base);
    bridge->stats.bio_async_connected = false;
    return result;
}

bool cnn_cortex_bridge_is_bio_async_connected(const cnn_cortex_bridge_t* bridge) {
    return bridge_base_is_bio_async_connected(bridge ? &bridge->base : NULL);
}

//=============================================================================
// Statistics API
//=============================================================================

int cnn_cortex_bridge_get_stats(
    const cnn_cortex_bridge_t* bridge,
    cnn_cortex_bridge_stats_t* stats
) {
    /* WHAT: Get bridge statistics
     * WHY:  Monitoring and debugging
     * HOW:  Copy internal stats
     */
    NIMCP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER,
                      "bridge or stats is NULL");

    cnn_cortex_bridge_t* mutable_bridge = (cnn_cortex_bridge_t*)bridge;
    BRIDGE_LOCK(mutable_bridge);
    memcpy(stats, &bridge->stats, sizeof(cnn_cortex_bridge_stats_t));
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int cnn_cortex_bridge_reset_stats(cnn_cortex_bridge_t* bridge) {
    /* WHAT: Reset statistics
     * WHY:  Start fresh measurement
     * HOW:  Zero counters, preserve connection state
     */
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    /* Preserve connection state */
    bool trainer = bridge->stats.trainer_connected;
    bool visual = bridge->stats.visual_cortex_connected;
    bool audio = bridge->stats.audio_cortex_connected;
    bool perception = bridge->stats.perception_bridge_connected;
    bool bio_async = bridge->stats.bio_async_connected;
    cnn_cortex_mode_t mode = bridge->stats.current_mode;

    memset(&bridge->stats, 0, sizeof(cnn_cortex_bridge_stats_t));

    /* Restore connection state */
    bridge->stats.trainer_connected = trainer;
    bridge->stats.visual_cortex_connected = visual;
    bridge->stats.audio_cortex_connected = audio;
    bridge->stats.perception_bridge_connected = perception;
    bridge->stats.bio_async_connected = bio_async;
    bridge->stats.current_mode = mode;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

//=============================================================================
// Utility API
//=============================================================================

const char* cnn_cortex_mode_to_string(cnn_cortex_mode_t mode) {
    switch (mode) {
        case CNN_CORTEX_MODE_DISABLED:      return "DISABLED";
        case CNN_CORTEX_MODE_FEATURE_ONLY:  return "FEATURE_ONLY";
        case CNN_CORTEX_MODE_TRAINING:      return "TRAINING";
        case CNN_CORTEX_MODE_FINE_TUNING:   return "FINE_TUNING";
        default:                            return "UNKNOWN";
    }
}

const char* cnn_cortex_priority_to_string(cnn_cortex_priority_t priority) {
    switch (priority) {
        case CNN_CORTEX_PRIORITY_VISUAL:     return "VISUAL";
        case CNN_CORTEX_PRIORITY_AUDIO:      return "AUDIO";
        case CNN_CORTEX_PRIORITY_MULTIMODAL: return "MULTIMODAL";
        default:                             return "UNKNOWN";
    }
}

const char* cnn_cortex_gradient_method_to_string(cnn_cortex_gradient_method_t method) {
    switch (method) {
        case CNN_CORTEX_GRADIENT_MAGNITUDE: return "MAGNITUDE";
        case CNN_CORTEX_GRADIENT_SIGN:      return "SIGN";
        case CNN_CORTEX_GRADIENT_HEBBIAN:   return "HEBBIAN";
        case CNN_CORTEX_GRADIENT_NONE:      return "NONE";
        default:                            return "UNKNOWN";
    }
}

void cnn_cortex_bridge_dump_state(const cnn_cortex_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_LOGGING_INFO("CNN-Cortex Bridge: NULL");
        return;
    }

    NIMCP_LOGGING_INFO("=== CNN-Cortex Bridge State ===");
    NIMCP_LOGGING_INFO("Mode: %s", cnn_cortex_mode_to_string(bridge->config.mode));
    NIMCP_LOGGING_INFO("Priority: %s", cnn_cortex_priority_to_string(bridge->config.priority));
    NIMCP_LOGGING_INFO("Connections:");
    NIMCP_LOGGING_INFO("  Trainer: %s", bridge->trainer ? "YES" : "NO");
    NIMCP_LOGGING_INFO("  Visual Cortex: %s", bridge->visual_cortex ? "YES" : "NO");
    NIMCP_LOGGING_INFO("  Audio Cortex: %s", bridge->audio_cortex ? "YES" : "NO");
    NIMCP_LOGGING_INFO("  Bio-async: %s", bridge->base.bio_async_enabled ? "YES" : "NO");
    NIMCP_LOGGING_INFO("Metrics:");
    NIMCP_LOGGING_INFO("  Visual Confidence: %.3f", bridge->metrics.visual_confidence);
    NIMCP_LOGGING_INFO("  Audio Quality: %.3f", bridge->metrics.audio_quality);
    NIMCP_LOGGING_INFO("  LR Factor: %.3f", bridge->metrics.lr_factor);
    NIMCP_LOGGING_INFO("  Skip Sample: %s", bridge->metrics.skip_sample ? "YES" : "NO");
    NIMCP_LOGGING_INFO("Stats:");
    NIMCP_LOGGING_INFO("  Total Extractions: %lu", bridge->stats.total_feature_extractions);
    NIMCP_LOGGING_INFO("  Visual Extractions: %lu", bridge->stats.visual_extractions);
    NIMCP_LOGGING_INFO("  Audio Extractions: %lu", bridge->stats.audio_extractions);
    NIMCP_LOGGING_INFO("  Gradient Feedbacks: %lu", bridge->stats.total_gradient_feedbacks);
    NIMCP_LOGGING_INFO("  Samples Skipped: %lu", bridge->stats.samples_skipped);
    NIMCP_LOGGING_INFO("================================");
}
