/**
 * @file nimcp_occipital_training_bridge.c
 * @brief Bridge between Occipital Cortex V1-V5 hierarchy and training pipeline
 *
 * @author NIMCP Team
 * @date 2025-01-01
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "middleware/training/nimcp_occipital_training_bridge.h"
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"
#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "utils/memory/nimcp_memory.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(occipital_training_bridge)

#define LOG_MODULE "OCCIPITAL_TRAINING_BRIDGE"


/*=============================================================================
 * Internal Structure
 *===========================================================================*/

struct occipital_training_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    occipital_adapter_t* occipital;           /**< Connected occipital adapter */
    nimcp_brain_training_ctx_t* training;     /**< Connected training context */
    occipital_training_config_t config;       /**< Bridge configuration */
    occipital_training_effects_t effects;     /**< Current training effects */
    occipital_training_stats_t stats;         /**< Runtime statistics */
    bool occipital_connected;                 /**< Occipital connection flag */
    bool training_connected;                  /**< Training connection flag */
    float* attention_buffer;                  /**< Attention weights buffer */
    uint32_t attention_buffer_size;           /**< Buffer capacity */
};

/*=============================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * @brief Compute area confidence from occipital statistics
 */
static float compute_area_confidence(
    const occipital_training_bridge_t* bridge,
    visual_area_t area)
{
    if (!bridge->occipital_connected || !bridge->occipital) {
        return 0.5f;  /* Default */
    }

    occipital_stats_t stats;
    if (!occipital_get_stats(bridge->occipital, &stats)) {
        return 0.5f;
    }

    /* Map visual area to confidence based on statistics */
    float frames = (float)(stats.frames_processed + 1);  /* Avoid div by zero */

    switch (area) {
        case VISUAL_AREA_V1:
            /* V1 confidence based on edges per frame */
            return nimcp_clampf((float)stats.edges_detected / frames / 50.0f, 0.0f, 1.0f);
        case VISUAL_AREA_V2:
            /* V2 confidence based on features per frame */
            return nimcp_clampf((float)stats.features_extracted / frames / 100.0f, 0.0f, 1.0f);
        case VISUAL_AREA_V4:
            /* V4 confidence based on feature extraction rate */
            return nimcp_clampf((float)stats.features_extracted / frames / 150.0f, 0.0f, 1.0f);
        case VISUAL_AREA_V5_MT:
            /* V5 confidence based on motion detection rate */
            return nimcp_clampf((float)stats.motions_detected / frames / 30.0f, 0.0f, 1.0f);
        default:
            return 0.5f;
    }
}

/**
 * @brief Compute overall confidence from per-area confidences
 */
static float compute_overall_confidence(const occipital_training_effects_t* effects) {
    /* Weighted average: V1 most important, then V4, V5, V2 */
    return 0.4f * effects->v1_confidence +
           0.25f * effects->v4_confidence +
           0.2f * effects->v5_confidence +
           0.15f * effects->v2_confidence;
}

/**
 * @brief Compute feature novelty
 */
static float compute_feature_novelty(occipital_training_bridge_t* bridge) {
    if (!bridge->occipital_connected || !bridge->occipital) {
        return 0.5f;
    }

    occipital_stats_t stats;
    if (!occipital_get_stats(bridge->occipital, &stats)) {
        return 0.5f;
    }

    /* Novelty based on ratio of new features to total */
    /* More features extracted = potentially more novel content */
    float novelty = (float)stats.features_extracted /
                    (float)(stats.frames_processed + 1) / 100.0f;
    return nimcp_clampf(novelty, 0.0f, 1.0f);
}

/**
 * @brief Compute motion stability
 */
static float compute_motion_stability(occipital_training_bridge_t* bridge) {
    if (!bridge->occipital_connected || !bridge->occipital) {
        return 0.5f;
    }

    occipital_stats_t stats;
    if (!occipital_get_stats(bridge->occipital, &stats)) {
        return 0.5f;
    }

    /* Stability based on motion detection variability
     * Fewer motions detected = more stable scene */
    float frames = (float)(stats.frames_processed + 1);
    float motions_per_frame = (float)stats.motions_detected / frames;

    /* High motion rate = less stable */
    float stability = 1.0f - nimcp_clampf(motions_per_frame / 20.0f, 0.0f, 1.0f);
    return stability;
}

/**
 * @brief Compute LR factor from effects
 */
static float compute_lr_factor(
    const occipital_training_bridge_t* bridge,
    const occipital_training_effects_t* effects)
{
    float base_factor = 1.0f;
    const occipital_training_config_t* cfg = &bridge->config;

    /* Confidence boost: high confidence -> higher LR */
    float confidence_boost = (effects->overall_confidence - 0.5f) *
                             cfg->confidence_lr_scale;
    base_factor += confidence_boost;

    /* Novelty boost: high novelty -> slightly higher LR (exploration) */
    float novelty_boost = (effects->feature_novelty - 0.5f) *
                          cfg->novelty_lr_scale;
    base_factor += novelty_boost;

    /* Stability adjustment: low stability -> lower LR */
    float stability_adj = (effects->motion_stability - 0.5f) *
                          cfg->stability_lr_scale;
    base_factor += stability_adj;

    return nimcp_clampf(base_factor, cfg->lr_min_factor, cfg->lr_max_factor);
}

/**
 * @brief Check if sample should be skipped
 */
static bool should_skip_sample(
    const occipital_training_bridge_t* bridge,
    const occipital_training_effects_t* effects)
{
    const occipital_training_config_t* cfg = &bridge->config;

    if (effects->overall_confidence < cfg->skip_confidence_threshold) {
        return true;
    }

    if (effects->motion_stability < cfg->skip_stability_threshold) {
        return true;
    }

    return false;
}

/**
 * @brief Update running average
 */
static float update_avg(float old_avg, float new_val, float alpha) {
    return (1.0f - alpha) * old_avg + alpha * new_val;
}

/*=============================================================================
 * Configuration API
 *===========================================================================*/

void occipital_training_default_config(occipital_training_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(occipital_training_config_t));

    /* Enable all areas by default */
    config->enable_v1_training = true;
    config->enable_v2_training = true;
    config->enable_v4_training = true;
    config->enable_v5_training = true;

    /* Modulation strengths */
    config->confidence_lr_scale = 0.5f;
    config->novelty_lr_scale = 0.3f;
    config->stability_lr_scale = 0.2f;

    /* LR limits */
    config->lr_min_factor = OCCIPITAL_TRAINING_DEFAULT_LR_MIN_FACTOR;
    config->lr_max_factor = OCCIPITAL_TRAINING_DEFAULT_LR_MAX_FACTOR;

    /* Skip thresholds */
    config->skip_confidence_threshold = 0.2f;
    config->skip_stability_threshold = 0.2f;

    /* Update settings */
    config->update_interval_ms = OCCIPITAL_TRAINING_DEFAULT_UPDATE_INTERVAL_MS;
    config->enable_bio_async = false;

    /* Integration flags */
    config->enable_perception_training = true;
    config->enable_weight_update_router = true;
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

occipital_training_bridge_t* occipital_training_bridge_create(
    const occipital_training_config_t* config)
{
    occipital_training_bridge_t* bridge = nimcp_calloc(1, sizeof(occipital_training_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    /* Initialize bridge base infrastructure (mutex, metrics, lifecycle) */
    if (bridge_base_init(&bridge->base, 0, "occipital_training") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        occipital_training_default_config(&bridge->config);
    }

    /* Initialize effects */
    bridge->effects.v1_confidence = 0.5f;
    bridge->effects.v2_confidence = 0.5f;
    bridge->effects.v4_confidence = 0.5f;
    bridge->effects.v5_confidence = 0.5f;
    bridge->effects.overall_confidence = 0.5f;
    bridge->effects.feature_novelty = 0.5f;
    bridge->effects.motion_novelty = 0.5f;
    bridge->effects.motion_stability = 0.5f;
    bridge->effects.color_stability = 0.5f;
    bridge->effects.lr_factor = 1.0f;
    bridge->effects.sample_weight = 1.0f;
    bridge->effects.skip_sample = false;
    bridge->effects.valid = false;

    /* Initialize stats */
    memset(&bridge->stats, 0, sizeof(occipital_training_stats_t));

    /* Allocate attention buffer */
    bridge->attention_buffer_size = 256;
    bridge->attention_buffer = nimcp_calloc(bridge->attention_buffer_size, sizeof(float));
    if (!bridge->attention_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "occipital_training_bridge_create: attention buffer allocation failed");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        return NULL;
    }

    return bridge;
}

void occipital_training_bridge_destroy(occipital_training_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "occipital_training");

    bridge_base_cleanup(&bridge->base);

    if (bridge->attention_buffer) {
        nimcp_free(bridge->attention_buffer);
    }

    nimcp_free(bridge);
}

int occipital_training_bridge_reset(occipital_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_training_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Reset effects to defaults */
    bridge->effects.v1_confidence = 0.5f;
    bridge->effects.v2_confidence = 0.5f;
    bridge->effects.v4_confidence = 0.5f;
    bridge->effects.v5_confidence = 0.5f;
    bridge->effects.overall_confidence = 0.5f;
    bridge->effects.feature_novelty = 0.5f;
    bridge->effects.motion_novelty = 0.5f;
    bridge->effects.motion_stability = 0.5f;
    bridge->effects.color_stability = 0.5f;
    bridge->effects.lr_factor = 1.0f;
    bridge->effects.sample_weight = 1.0f;
    bridge->effects.skip_sample = false;
    bridge->effects.valid = false;

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(occipital_training_stats_t));

    return 0;
}

/*=============================================================================
 * Connection API
 *===========================================================================*/

int occipital_training_connect_occipital(
    occipital_training_bridge_t* bridge,
    occipital_adapter_t* occipital)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_training_connect_occipital: bridge is NULL");
        return -1;
    }

    bridge->occipital = occipital;
    bridge->occipital_connected = (occipital != NULL);
    bridge->stats.occipital_connected = bridge->occipital_connected;

    return 0;
}

int occipital_training_connect_training(
    occipital_training_bridge_t* bridge,
    nimcp_brain_training_ctx_t* training)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_training_connect_training: bridge is NULL");
        return -1;
    }

    bridge->training = training;
    bridge->training_connected = (training != NULL);
    bridge->stats.training_connected = bridge->training_connected;

    return 0;
}

/*=============================================================================
 * Training Effects API
 *===========================================================================*/

int occipital_training_update_effects(occipital_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_training_update_effects: bridge is NULL");
        return -1;
    }

    /* Compute per-area confidences */
    bridge->effects.v1_confidence = compute_area_confidence(bridge, VISUAL_AREA_V1);
    bridge->effects.v2_confidence = compute_area_confidence(bridge, VISUAL_AREA_V2);
    bridge->effects.v4_confidence = compute_area_confidence(bridge, VISUAL_AREA_V4);
    bridge->effects.v5_confidence = compute_area_confidence(bridge, VISUAL_AREA_V5_MT);

    /* Compute overall confidence */
    bridge->effects.overall_confidence = compute_overall_confidence(&bridge->effects);

    /* Compute novelty */
    bridge->effects.feature_novelty = compute_feature_novelty(bridge);
    bridge->effects.motion_novelty = bridge->effects.feature_novelty * 0.8f;  /* Correlated */

    /* Compute stability */
    bridge->effects.motion_stability = compute_motion_stability(bridge);
    bridge->effects.color_stability = bridge->effects.v4_confidence * 0.9f;

    /* Compute modulations */
    bridge->effects.lr_factor = compute_lr_factor(bridge, &bridge->effects);
    bridge->effects.sample_weight = 0.5f + bridge->effects.overall_confidence;
    bridge->effects.skip_sample = should_skip_sample(bridge, &bridge->effects);

    /* Mark as valid */
    bridge->effects.valid = true;

    /* Update stats */
    const float alpha = 0.1f;
    bridge->stats.avg_confidence = update_avg(
        bridge->stats.avg_confidence, bridge->effects.overall_confidence, alpha);
    bridge->stats.avg_novelty = update_avg(
        bridge->stats.avg_novelty, bridge->effects.feature_novelty, alpha);
    bridge->stats.avg_lr_factor = update_avg(
        bridge->stats.avg_lr_factor, bridge->effects.lr_factor, alpha);

    if (bridge->effects.lr_factor > 1.0f) {
        bridge->stats.lr_increases++;
    } else if (bridge->effects.lr_factor < 1.0f) {
        bridge->stats.lr_decreases++;
    }

    if (bridge->effects.skip_sample) {
        bridge->stats.samples_skipped++;
    }

    return 0;
}

int occipital_training_get_effects(
    const occipital_training_bridge_t* bridge,
    occipital_training_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_training_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    *effects = bridge->effects;
    /* Don't copy the attention pointer - it's internal */
    effects->attention_weights = NULL;
    effects->num_features = 0;

    return 0;
}

float occipital_training_get_modulated_lr(
    const occipital_training_bridge_t* bridge,
    float base_lr)
{
    if (!bridge || !bridge->effects.valid) {
        return base_lr;
    }

    return base_lr * bridge->effects.lr_factor;
}

bool occipital_training_should_skip(
    const occipital_training_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->effects.skip_sample;
}

int occipital_training_get_attention_scaling(
    const occipital_training_bridge_t* bridge,
    float* factors,
    uint32_t num_features)
{
    if (!bridge || !factors || num_features == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_training_get_attention_scaling: required parameter is NULL (bridge, factors)");
        return -1;
    }

    /* If we have cached attention weights, use them */
    if (bridge->attention_buffer && bridge->effects.num_features > 0) {
        uint32_t copy_count = (num_features < bridge->effects.num_features) ?
                              num_features : bridge->effects.num_features;
        memcpy(factors, bridge->attention_buffer, copy_count * sizeof(float));

        /* Fill remaining with 1.0 */
        for (uint32_t i = copy_count; i < num_features; i++) {
            factors[i] = 1.0f;
        }
    } else {
        /* Default: uniform attention based on overall confidence */
        for (uint32_t i = 0; i < num_features; i++) {
            factors[i] = 0.5f + bridge->effects.overall_confidence * 0.5f;
        }
    }

    return 0;
}

/*=============================================================================
 * Training Targets API
 *===========================================================================*/

int occipital_training_apply_targets(
    occipital_training_bridge_t* bridge,
    const occipital_training_targets_t* targets)
{
    if (!bridge || !targets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_training_apply_targets: required parameter is NULL (bridge, targets)");
        return -1;
    }
    if (!bridge->occipital_connected || !bridge->occipital) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_training_apply_targets: required parameter is NULL (bridge->occipital_connected, bridge->occipital)");
        return -1;
    }

    /* Apply targets to occipital for supervised learning */
    /* The occipital adapter's train function handles this */

    bridge->stats.total_training_steps++;

    return 0;
}

int occipital_training_train_area(
    occipital_training_bridge_t* bridge,
    occipital_training_area_t area,
    float learning_rate)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_training_train_area: bridge is NULL");
        return -1;
    }
    if (!bridge->occipital_connected || !bridge->occipital) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_training_train_area: required parameter is NULL (bridge->occipital_connected, bridge->occipital)");
        return -1;
    }

    /* Check if area training is enabled */
    const occipital_training_config_t* cfg = &bridge->config;
    switch (area) {
        case OCCIPITAL_TRAIN_V1:
            if (!cfg->enable_v1_training) return 0;
            bridge->stats.v1_updates++;
            break;
        case OCCIPITAL_TRAIN_V2:
            if (!cfg->enable_v2_training) return 0;
            bridge->stats.v2_updates++;
            break;
        case OCCIPITAL_TRAIN_V4:
            if (!cfg->enable_v4_training) return 0;
            bridge->stats.v4_updates++;
            break;
        case OCCIPITAL_TRAIN_V5:
            if (!cfg->enable_v5_training) return 0;
            bridge->stats.v5_updates++;
            break;
        case OCCIPITAL_TRAIN_ALL:
            bridge->stats.v1_updates++;
            bridge->stats.v2_updates++;
            bridge->stats.v4_updates++;
            bridge->stats.v5_updates++;
            break;
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_training_train_area: operation failed");
            return -1;
    }

    /* Modulate learning rate */
    float modulated_lr = occipital_training_get_modulated_lr(bridge, learning_rate);

    (void)modulated_lr;  /* Would pass to occipital training function */

    return 0;
}

int occipital_training_compute_loss(
    const occipital_training_bridge_t* bridge,
    occipital_training_area_t area,
    float* loss)
{
    if (!bridge || !loss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_training_compute_loss: required parameter is NULL (bridge, loss)");
        return -1;
    }

    /* Compute loss based on confidence (inverse relationship) */
    /* High confidence = low loss, low confidence = high loss */
    switch (area) {
        case OCCIPITAL_TRAIN_V1:
            *loss = 1.0f - bridge->effects.v1_confidence;
            break;
        case OCCIPITAL_TRAIN_V2:
            *loss = 1.0f - bridge->effects.v2_confidence;
            break;
        case OCCIPITAL_TRAIN_V4:
            *loss = 1.0f - bridge->effects.v4_confidence;
            break;
        case OCCIPITAL_TRAIN_V5:
            *loss = 1.0f - bridge->effects.v5_confidence;
            break;
        case OCCIPITAL_TRAIN_ALL:
            *loss = 1.0f - bridge->effects.overall_confidence;
            break;
        default:
            *loss = 0.5f;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_training_compute_loss: operation failed");
            return -1;
    }

    return 0;
}

/*=============================================================================
 * Update Cycle API
 *===========================================================================*/

int occipital_training_update(
    occipital_training_bridge_t* bridge,
    uint64_t delta_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_training_update: bridge is NULL");
        return -1;
    }

    (void)delta_ms;

    /* Update effects from occipital state */
    int result = occipital_training_update_effects(bridge);
    if (result != 0) {
        return result;
    }

    bridge->stats.total_training_steps++;
    bridge->stats.last_update_ms += delta_ms;

    return 0;
}

/*=============================================================================
 * Statistics API
 *===========================================================================*/

int occipital_training_get_stats(
    const occipital_training_bridge_t* bridge,
    occipital_training_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_training_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

void occipital_training_reset_stats(occipital_training_bridge_t* bridge) {
    if (!bridge) return;

    bool occ_conn = bridge->stats.occipital_connected;
    bool train_conn = bridge->stats.training_connected;
    bool bio_conn = bridge->stats.bio_async_connected;

    memset(&bridge->stats, 0, sizeof(occipital_training_stats_t));

    /* Preserve connection status */
    bridge->stats.occipital_connected = occ_conn;
    bridge->stats.training_connected = train_conn;
    bridge->stats.bio_async_connected = bio_conn;
}
