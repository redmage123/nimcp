/**
 * @file nimcp_cortical_attention_gain.c
 * @brief Implementation of attention-modulated gain control for cortical columns
 * @version 1.0.0
 * @date 2025-12-15
 */

#include "core/cortical_columns/nimcp_cortical_attention_gain.h"
#include "api/nimcp_api_exception.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cortical_attention_gain)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Compute Gaussian weight
 *
 * WHAT: Calculate Gaussian function value
 * WHY:  Used for spatial and feature-based selectivity
 * HOW:  exp(-distance² / 2σ²)
 */
static float compute_gaussian(float distance, float sigma) {
    if (sigma <= 0.0f) return 0.0f;
    float normalized = distance / sigma;
    return expf(-0.5f * normalized * normalized);
}

/**
 * @brief Compute feature-based gain
 *
 * WHAT: Calculate gain boost for feature similarity
 * WHY:  Feature attention boosts similar features
 * HOW:  Gaussian tuning curve around target feature
 */
static float compute_feature_gain(
    float baseline,
    float boost,
    float target_feature,
    float column_feature,
    float selectivity
) {
    /* WHAT: Compute distance in feature space
     * WHY:  Determine similarity to target
     * HOW:  Handle circular features (e.g., orientation)
     */
    float distance = fabsf(target_feature - column_feature);

    /* Handle circular feature space (e.g., 0° = 180° for orientation) */
    if (distance > 180.0f) {
        distance = 360.0f - distance;
    }

    /* WHAT: Apply Gaussian tuning
     * WHY:  Similar features get higher gain
     * HOW:  Gaussian centered on target
     */
    float similarity = compute_gaussian(distance, selectivity);
    return baseline + boost * similarity;
}

/**
 * @brief Compute spatial gain from spotlight
 *
 * WHAT: Calculate gain boost for spatial location
 * WHY:  Spatial attention boosts specific locations
 * HOW:  Gaussian spotlight centered on attended location
 */
static float compute_spatial_gain(
    float baseline,
    float boost,
    float rf_x,
    float rf_y,
    const attention_spotlight_t* spotlight
) {
    if (!spotlight || spotlight->intensity <= 0.0f) {
        return baseline;
    }

    /* WHAT: Compute Euclidean distance from RF center to spotlight center
     * WHY:  Determine spatial proximity
     */
    float dx = rf_x - spotlight->center_x;
    float dy = rf_y - spotlight->center_y;
    float distance = sqrtf(dx * dx + dy * dy);

    /* WHAT: Apply Gaussian spatial profile
     * WHY:  Gradual falloff with distance
     * HOW:  Scale by spotlight intensity
     */
    float spatial_weight = compute_gaussian(distance, spotlight->radius);
    return baseline + boost * spotlight->intensity * spatial_weight;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int cortical_attention_default_config(cortical_attention_config_t* config) {
    /* WHAT: Validate input
     * WHY:  Guard clause
     */
    if (!config) {
        NIMCP_LOGGING_ERROR("cortical_attention_default_config: NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_attention_default_config: config is NULL");
        return -1;
    }

    /* WHAT: Set biologically-plausible defaults
     * WHY:  Based on Reynolds & Heeger (2009) normalization model
     * HOW:  Values from empirical cortical studies
     */
    config->mode = ATTENTION_NONE;
    config->baseline_gain = 1.0f;
    config->max_gain_boost = 2.0f;  /* 2x boost for attended */
    config->spatial_sigma = 2.0f;    /* ~2° visual angle */
    config->feature_selectivity = 30.0f; /* 30° tuning width */
    config->enable_suppression = true;
    config->suppression_factor = 0.6f; /* 40% suppression for unattended */
    config->layer_23_gain_factor = 1.8f; /* Layer 2/3 most affected */
    config->layer_56_gain_factor = 1.3f; /* Layer 5/6 moderately affected */
    config->enable_fep_coupling = false;
    config->precision_gain_slope = 0.5f;

    return 0;
}

cortical_attention_gain_t* cortical_attention_create(
    const cortical_attention_config_t* config,
    hypercolumn_t* hypercolumn
) {
    /* WHAT: Validate inputs
     * WHY:  Guard clause
     */
    if (!hypercolumn) {
        NIMCP_LOGGING_ERROR("cortical_attention_create: NULL hypercolumn");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypercolumn is NULL");

        return NULL;
    }

    /* WHAT: Allocate attention system
     * WHY:  Create system object
     * HOW:  Use nimcp_calloc for zero-initialization
     */
    cortical_attention_gain_t* attention =
        (cortical_attention_gain_t*)nimcp_calloc(1, sizeof(cortical_attention_gain_t));
    if (!attention) {
        NIMCP_LOGGING_ERROR("cortical_attention_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_attention_create: allocation failed");
        return NULL;
    }

    /* WHAT: Set configuration
     * WHY:  Initialize parameters
     * HOW:  Copy config or use defaults
     */
    if (config) {
        memcpy(&attention->config, config, sizeof(cortical_attention_config_t));
    } else {
        cortical_attention_default_config(&attention->config);
    }

    /* WHAT: Get hypercolumn info
     * WHY:  Need minicolumn count
     */
    cc_hypercolumn_stats_t hc_stats;
    hypercolumn_get_stats(hypercolumn, &hc_stats);
    attention->num_minicolumns = hc_stats.num_minicolumns;
    attention->hypercolumn = hypercolumn;

    /* WHAT: Allocate per-minicolumn gain states
     * WHY:  Track gain for each minicolumn
     */
    attention->minicolumn_gains = (minicolumn_gain_state_t*)nimcp_calloc(
        attention->num_minicolumns,
        sizeof(minicolumn_gain_state_t)
    );
    if (!attention->minicolumn_gains) {
        NIMCP_LOGGING_ERROR("cortical_attention_create: gain state allocation failed");
        nimcp_free(attention);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_attention_create: attention->minicolumn_gains is NULL");
        return NULL;
    }

    /* WHAT: Initialize gain states to baseline
     * WHY:  Start with no attention effects
     */
    for (uint32_t i = 0; i < attention->num_minicolumns; i++) {
        attention->minicolumn_gains[i].feature_gain = attention->config.baseline_gain;
        attention->minicolumn_gains[i].spatial_gain = attention->config.baseline_gain;
        attention->minicolumn_gains[i].layer_23_gain = attention->config.baseline_gain;
        attention->minicolumn_gains[i].layer_4_gain = attention->config.baseline_gain;
        attention->minicolumn_gains[i].layer_56_gain = attention->config.baseline_gain;
        attention->minicolumn_gains[i].total_gain = attention->config.baseline_gain;
        attention->minicolumn_gains[i].is_attended = false;
    }

    /* WHAT: Initialize spotlight
     * WHY:  Set default spatial attention
     */
    attention->spotlight.center_x = 0.0f;
    attention->spotlight.center_y = 0.0f;
    attention->spotlight.radius = attention->config.spatial_sigma;
    attention->spotlight.intensity = 0.0f;

    /* WHAT: Initialize divided attention
     * WHY:  Start with no additional spotlights
     */
    attention->spotlights = NULL;
    attention->num_spotlights = 0;

    /* WHAT: Initialize FEP coupling
     * WHY:  Start uncoupled
     */
    attention->fep_system = NULL;
    attention->current_precision = 1.0f;

    /* WHAT: Create mutex
     * WHY:  Thread safety
     */
    attention->mutex = nimcp_platform_mutex_create();
    if (!attention->mutex) {
        NIMCP_LOGGING_WARN("cortical_attention_create: mutex creation failed, continuing without thread safety");
    }

    /* WHAT: Initialize bio-async
     * WHY:  Prepare for messaging (not connected yet)
     */
    attention->bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Cortical attention system created: %u minicolumns",
                       attention->num_minicolumns);

    return attention;
}

void cortical_attention_destroy(cortical_attention_gain_t* attention) {
    /* WHAT: Handle NULL input
     * WHY:  NULL-safe cleanup
     */
    if (!attention) return;

    /* WHAT: Disconnect bio-async
     * WHY:  Clean shutdown
     */
    if (attention->bio_async_enabled) {
        cortical_attention_disconnect_bio_async(attention);
    }

    /* WHAT: Free spotlights array
     * WHY:  Release divided attention memory
     */
    if (attention->spotlights) {
        nimcp_free(attention->spotlights);
    }

    /* WHAT: Free gain states
     * WHY:  Release per-minicolumn memory
     */
    if (attention->minicolumn_gains) {
        nimcp_free(attention->minicolumn_gains);
    }

    /* WHAT: Destroy mutex
     * WHY:  Release synchronization primitive
     */
    if (attention->mutex) {
        nimcp_platform_mutex_destroy(attention->mutex);
        nimcp_free(attention->mutex);
        attention->mutex = NULL;
    }

    /* WHAT: Free attention structure
     * WHY:  Final cleanup
     */
    nimcp_free(attention);

    NIMCP_LOGGING_INFO("Cortical attention system destroyed");
}

/* ============================================================================
 * Attention Control Implementation
 * ============================================================================ */

int cortical_attention_set_mode(
    cortical_attention_gain_t* attention,
    attention_mode_t mode
) {
    /* WHAT: Validate input
     * WHY:  Guard clause
     */
    if (!attention) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention is NULL");

        return -1;

    }

    /* WHAT: Lock mutex
     * WHY:  Thread safety
     */
    if (attention->mutex) {
        nimcp_mutex_lock(attention->mutex);
    }

    /* WHAT: Update mode
     * WHY:  Change attention strategy
     */
    attention->config.mode = mode;

    /* WHAT: Reset gain states
     * WHY:  Mode change requires recomputation
     */
    for (uint32_t i = 0; i < attention->num_minicolumns; i++) {
        attention->minicolumn_gains[i].total_gain = attention->config.baseline_gain;
        attention->minicolumn_gains[i].is_attended = false;
    }

    /* WHAT: Unlock mutex
     * WHY:  Release lock
     */
    if (attention->mutex) {
        nimcp_mutex_unlock(attention->mutex);
    }

    NIMCP_LOGGING_INFO("Attention mode set to: %d", mode);
    return 0;
}

int cortical_attention_set_spotlight(
    cortical_attention_gain_t* attention,
    float center_x,
    float center_y,
    float radius,
    float intensity
) {
    /* WHAT: Validate input
     * WHY:  Guard clause
     */
    if (!attention) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention is NULL");

        return -1;

    }
    if (radius <= 0.0f || intensity < 0.0f || intensity > 1.0f) {
        NIMCP_LOGGING_ERROR("Invalid spotlight parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_attention_set_spotlight: validation failed");
        return -1;
    }

    /* WHAT: Lock mutex
     * WHY:  Thread safety
     */
    if (attention->mutex) {
        nimcp_mutex_lock(attention->mutex);
    }

    /* WHAT: Update spotlight
     * WHY:  Set spatial attention target
     */
    attention->spotlight.center_x = center_x;
    attention->spotlight.center_y = center_y;
    attention->spotlight.radius = radius;
    attention->spotlight.intensity = intensity;

    /* WHAT: Unlock mutex
     * WHY:  Release lock
     */
    if (attention->mutex) {
        nimcp_mutex_unlock(attention->mutex);
    }

    return 0;
}

int cortical_attention_set_feature_target(
    cortical_attention_gain_t* attention,
    float target_feature
) {
    /* WHAT: Validate input
     * WHY:  Guard clause
     */
    if (!attention) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention is NULL");

        return -1;

    }

    /* WHAT: Lock mutex
     * WHY:  Thread safety
     */
    if (attention->mutex) {
        nimcp_mutex_lock(attention->mutex);
    }

    /* WHAT: Update target feature
     * WHY:  Set feature-based attention target
     */
    attention->target_feature = target_feature;

    /* WHAT: Unlock mutex
     * WHY:  Release lock
     */
    if (attention->mutex) {
        nimcp_mutex_unlock(attention->mutex);
    }

    return 0;
}

int cortical_attention_add_spotlight(
    cortical_attention_gain_t* attention,
    float center_x,
    float center_y,
    float radius,
    float intensity
) {
    /* WHAT: Validate input
     * WHY:  Guard clause
     */
    if (!attention) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention is NULL");

        return -1;

    }
    if (radius <= 0.0f || intensity < 0.0f || intensity > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_attention_add_spotlight: validation failed");
        return -1;
    }

    /* WHAT: Lock mutex
     * WHY:  Thread safety
     */
    if (attention->mutex) {
        nimcp_mutex_lock(attention->mutex);
    }

    /* WHAT: Reallocate spotlights array
     * WHY:  Add new spotlight
     */
    uint32_t new_count = attention->num_spotlights + 1;
    attention_spotlight_t* new_spotlights = (attention_spotlight_t*)nimcp_realloc(
        attention->spotlights,
        new_count * sizeof(attention_spotlight_t)
    );

    if (!new_spotlights) {
        if (attention->mutex) {
            nimcp_mutex_unlock(attention->mutex);
        }
        NIMCP_LOGGING_ERROR("Failed to add spotlight");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "cortical_attention_add_spotlight: validation failed");
        return -1;
    }

    /* WHAT: Add new spotlight
     * WHY:  Store spotlight parameters
     */
    attention->spotlights = new_spotlights;
    attention->spotlights[attention->num_spotlights].center_x = center_x;
    attention->spotlights[attention->num_spotlights].center_y = center_y;
    attention->spotlights[attention->num_spotlights].radius = radius;
    attention->spotlights[attention->num_spotlights].intensity = intensity;
    attention->num_spotlights = new_count;

    /* WHAT: Unlock mutex
     * WHY:  Release lock
     */
    if (attention->mutex) {
        nimcp_mutex_unlock(attention->mutex);
    }

    return 0;
}

int cortical_attention_clear_spotlights(cortical_attention_gain_t* attention) {
    /* WHAT: Validate input
     * WHY:  Guard clause
     */
    if (!attention) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention is NULL");

        return -1;

    }

    /* WHAT: Lock mutex
     * WHY:  Thread safety
     */
    if (attention->mutex) {
        nimcp_mutex_lock(attention->mutex);
    }

    /* WHAT: Free spotlights
     * WHY:  Clear divided attention
     */
    if (attention->spotlights) {
        nimcp_free(attention->spotlights);
        attention->spotlights = NULL;
    }
    attention->num_spotlights = 0;

    /* WHAT: Unlock mutex
     * WHY:  Release lock
     */
    if (attention->mutex) {
        nimcp_mutex_unlock(attention->mutex);
    }

    return 0;
}

/* ============================================================================
 * Gain Application Implementation
 * ============================================================================ */

int cortical_attention_update_gains(cortical_attention_gain_t* attention) {
    /* WHAT: Validate input
     * WHY:  Guard clause
     */
    if (!attention) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention is NULL");

        return -1;

    }

    /* WHAT: Lock mutex
     * WHY:  Thread safety during update
     */
    if (attention->mutex) {
        nimcp_mutex_lock(attention->mutex);
    }

    /* WHAT: Reset statistics
     * WHY:  Compute fresh stats
     */
    attention->stats.attended_columns = 0;
    attention->stats.avg_attended_gain = 0.0f;
    attention->stats.avg_unattended_gain = 0.0f;
    attention->stats.max_gain_applied = 0.0f;

    /* WHAT: Iterate all minicolumns
     * WHY:  Update gain for each
     */
    for (uint32_t i = 0; i < attention->num_minicolumns; i++) {
        minicolumn_gain_state_t* gain = &attention->minicolumn_gains[i];

        /* WHAT: Get minicolumn stats
         * WHY:  Need tuning preference and receptive field
         * NOTE: Placeholder - actual minicolumn access would require hypercolumn API
         */
        minicolumn_stats_t mc_stats;
        /* TODO: Get actual minicolumn from hypercolumn->minicolumns[i] when API available */
        (void)mc_stats; /* Unused in simplified version */

        /* WHAT: Compute feature-based gain
         * WHY:  Feature attention boosts similar features
         */
        if (attention->config.mode == ATTENTION_FEATURE_BASED) {
            gain->feature_gain = compute_feature_gain(
                attention->config.baseline_gain,
                attention->config.max_gain_boost,
                attention->target_feature,
                mc_stats.tuning_preference,
                attention->config.feature_selectivity
            );
        } else {
            gain->feature_gain = attention->config.baseline_gain;
        }

        /* WHAT: Compute spatial gain
         * WHY:  Spatial attention boosts specific locations
         */
        if (attention->config.mode == ATTENTION_SPATIAL) {
            /* Use hypercolumn topographic position as approximation */
            gain->spatial_gain = compute_spatial_gain(
                attention->config.baseline_gain,
                attention->config.max_gain_boost,
                0.0f, /* Would need actual RF position */
                0.0f,
                &attention->spotlight
            );
        } else if (attention->config.mode == ATTENTION_DIVIDED) {
            /* Sum contributions from all spotlights */
            float max_spatial_gain = attention->config.baseline_gain;
            for (uint32_t s = 0; s < attention->num_spotlights; s++) {
                float sg = compute_spatial_gain(
                    attention->config.baseline_gain,
                    attention->config.max_gain_boost,
                    0.0f,
                    0.0f,
                    &attention->spotlights[s]
                );
                if (sg > max_spatial_gain) {
                    max_spatial_gain = sg;
                }
            }
            gain->spatial_gain = max_spatial_gain;
        } else {
            gain->spatial_gain = attention->config.baseline_gain;
        }

        /* WHAT: Combine feature and spatial gains
         * WHY:  Multiplicative combination
         */
        float combined_gain = gain->feature_gain * gain->spatial_gain;

        /* WHAT: Apply FEP precision modulation
         * WHY:  Couple attention to inference precision
         */
        if (attention->config.enable_fep_coupling && attention->fep_system) {
            float precision_factor = 1.0f +
                attention->config.precision_gain_slope *
                (attention->current_precision - 1.0f);
            combined_gain *= precision_factor;
        }

        /* WHAT: Compute layer-specific gains
         * WHY:  Different layers have different susceptibility
         */
        gain->layer_23_gain = combined_gain * attention->config.layer_23_gain_factor;
        gain->layer_4_gain = combined_gain; /* Layer 4 baseline */
        gain->layer_56_gain = combined_gain * attention->config.layer_56_gain_factor;

        /* WHAT: Set total gain (use Layer 2/3 as representative)
         * WHY:  Layer 2/3 most affected by attention
         */
        gain->total_gain = gain->layer_23_gain;

        /* WHAT: Apply suppression to unattended
         * WHY:  Winner-take-all competition
         */
        float attend_threshold = attention->config.baseline_gain +
                                 0.2f * attention->config.max_gain_boost;
        if (gain->total_gain < attend_threshold && attention->config.enable_suppression) {
            gain->total_gain *= attention->config.suppression_factor;
            gain->is_attended = false;
        } else {
            gain->is_attended = true;
        }

        /* WHAT: Update statistics
         * WHY:  Track attention effects
         */
        if (gain->is_attended) {
            attention->stats.attended_columns++;
            attention->stats.avg_attended_gain += gain->total_gain;
        } else {
            attention->stats.avg_unattended_gain += gain->total_gain;
        }

        if (gain->total_gain > attention->stats.max_gain_applied) {
            attention->stats.max_gain_applied = gain->total_gain;
        }
    }

    /* WHAT: Finalize statistics
     * WHY:  Compute averages
     */
    if (attention->stats.attended_columns > 0) {
        attention->stats.avg_attended_gain /= attention->stats.attended_columns;
    }
    uint32_t unattended = attention->num_minicolumns - attention->stats.attended_columns;
    if (unattended > 0) {
        attention->stats.avg_unattended_gain /= unattended;
    }
    attention->stats.total_updates++;

    /* WHAT: Unlock mutex
     * WHY:  Release lock
     */
    if (attention->mutex) {
        nimcp_mutex_unlock(attention->mutex);
    }

    return 0;
}

float cortical_attention_apply_gain(
    const cortical_attention_gain_t* attention,
    uint32_t minicolumn_idx,
    float activation
) {
    /* WHAT: Validate input
     * WHY:  Guard clause
     */
    if (!attention || minicolumn_idx >= attention->num_minicolumns) {
        return activation;
    }

    /* WHAT: Get gain for minicolumn
     * WHY:  Look up precomputed gain
     */
    float gain = attention->minicolumn_gains[minicolumn_idx].total_gain;

    /* WHAT: Apply multiplicative gain
     * WHY:  Attention modulates response
     */
    float modulated = activation * gain;

    /* WHAT: Clamp to [0, 1]
     * WHY:  Keep activation in valid range
     */
    if (modulated > 1.0f) modulated = 1.0f;
    if (modulated < 0.0f) modulated = 0.0f;

    return modulated;
}

float cortical_attention_compute_layer_gain(
    const cortical_attention_gain_t* attention,
    uint32_t minicolumn_idx,
    uint32_t layer
) {
    /* WHAT: Validate input
     * WHY:  Guard clause
     */
    if (!attention || minicolumn_idx >= attention->num_minicolumns) {
        return 1.0f;
    }

    /* WHAT: Get layer-specific gain
     * WHY:  Return appropriate layer gain
     */
    const minicolumn_gain_state_t* gain = &attention->minicolumn_gains[minicolumn_idx];

    switch (layer) {
        case 0: return gain->layer_23_gain;  /* Layer 2/3 */
        case 1: return gain->layer_4_gain;   /* Layer 4 */
        case 2: return gain->layer_56_gain;  /* Layer 5/6 */
        default: return gain->total_gain;
    }
}

int cortical_attention_apply_gain_to_hypercolumn(
    cortical_attention_gain_t* attention
) {
    /* WHAT: Validate input
     * WHY:  Guard clause
     */
    if (!attention || !attention->hypercolumn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_attention_apply_gain_to_hypercolumn: required parameter is NULL (attention, attention->hypercolumn)");
        return -1;
    }

    /* WHAT: Update gains first
     * WHY:  Ensure gains are current
     */
    cortical_attention_update_gains(attention);

    /* WHAT: Apply gains to all minicolumns
     * WHY:  Batch application for efficiency
     * NOTE: This would require hypercolumn API support for gain modulation
     */

    return 0;
}

/* ============================================================================
 * FEP Integration Implementation
 * ============================================================================ */

int cortical_attention_connect_fep(
    cortical_attention_gain_t* attention,
    fep_system_t* fep_system
) {
    /* WHAT: Validate input
     * WHY:  Guard clause
     */
    if (!attention || !fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_attention_connect_fep: required parameter is NULL (attention, fep_system)");
        return -1;
    }

    /* WHAT: Store FEP system
     * WHY:  Enable precision coupling
     */
    attention->fep_system = fep_system;
    attention->config.enable_fep_coupling = true;

    NIMCP_LOGGING_INFO("Connected to FEP system");
    return 0;
}

int cortical_attention_disconnect_fep(cortical_attention_gain_t* attention) {
    /* WHAT: Validate input
     * WHY:  Guard clause
     */
    if (!attention) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention is NULL");

        return -1;

    }

    /* WHAT: Clear FEP system
     * WHY:  Disable coupling
     */
    attention->fep_system = NULL;
    attention->config.enable_fep_coupling = false;

    return 0;
}

float cortical_attention_modulate_precision(
    const cortical_attention_gain_t* attention,
    float base_precision
) {
    /* WHAT: Validate input
     * WHY:  Guard clause
     */
    if (!attention) return base_precision;

    /* WHAT: Map attention to precision
     * WHY:  High attention → high precision (confident inference)
     * HOW:  Linear scaling by average attended gain
     */
    float avg_gain = attention->stats.avg_attended_gain;
    float precision_factor = 1.0f +
        attention->config.precision_gain_slope * (avg_gain - 1.0f);

    return base_precision * precision_factor;
}

int cortical_attention_update_from_precision(
    cortical_attention_gain_t* attention,
    float precision
) {
    /* WHAT: Validate input
     * WHY:  Guard clause
     */
    if (!attention) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention is NULL");

        return -1;

    }

    /* WHAT: Update current precision
     * WHY:  Store for gain computation
     */
    attention->current_precision = precision;

    /* WHAT: Trigger gain update
     * WHY:  Apply precision modulation
     */
    return cortical_attention_update_gains(attention);
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int cortical_attention_get_minicolumn_gain(
    const cortical_attention_gain_t* attention,
    uint32_t minicolumn_idx,
    minicolumn_gain_state_t* gain_state
) {
    /* WHAT: Validate input
     * WHY:  Guard clause
     */
    if (!attention || !gain_state || minicolumn_idx >= attention->num_minicolumns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_attention_get_minicolumn_gain: required parameter is NULL (attention, gain_state)");
        return -1;
    }

    /* WHAT: Copy gain state
     * WHY:  Return precomputed gain
     */
    memcpy(gain_state, &attention->minicolumn_gains[minicolumn_idx],
           sizeof(minicolumn_gain_state_t));

    return 0;
}

bool cortical_attention_is_attended(
    const cortical_attention_gain_t* attention,
    uint32_t minicolumn_idx
) {
    /* WHAT: Validate input
     * WHY:  Guard clause
     */
    if (!attention || minicolumn_idx >= attention->num_minicolumns) {
        return false;
    }

    /* WHAT: Return attended flag
     * WHY:  Binary attended/unattended
     */
    return attention->minicolumn_gains[minicolumn_idx].is_attended;
}

int cortical_attention_get_stats(
    const cortical_attention_gain_t* attention,
    attention_gain_stats_t* stats
) {
    /* WHAT: Validate input
     * WHY:  Guard clause
     */
    if (!attention || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_attention_get_stats: required parameter is NULL (attention, stats)");
        return -1;
    }

    /* WHAT: Copy statistics
     * WHY:  Return accumulated stats
     */
    memcpy(stats, &attention->stats, sizeof(attention_gain_stats_t));

    return 0;
}

void cortical_attention_reset_stats(cortical_attention_gain_t* attention) {
    /* WHAT: Validate input
     * WHY:  Guard clause
     */
    if (!attention) return;

    /* WHAT: Zero statistics
     * WHY:  Reset counters
     */
    memset(&attention->stats, 0, sizeof(attention_gain_stats_t));
}

/* ============================================================================
 * Bio-async Implementation
 * ============================================================================ */

int cortical_attention_connect_bio_async(cortical_attention_gain_t* attention) {
    /* WHAT: Validate input
     * WHY:  Guard clause
     */
    if (!attention) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention is NULL");

        return -1;

    }

    /* WHAT: Check if already connected
     * WHY:  Avoid double registration
     */
    if (attention->bio_async_enabled) {
        return 0;
    }

    /* WHAT: Register with bio-async router
     * WHY:  Enable inter-module messaging
     * HOW:  Use BIO_MODULE_CORTICAL_ATTENTION_GAIN module ID
     */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_CORTICAL_ATTENTION_GAIN,
        .module_name = "cortical_attention_gain",
        .inbox_capacity = 32,
        .user_data = attention
    };

    attention->bio_ctx = bio_router_register_module(&info);
    if (attention->bio_ctx) {
        attention->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
        return 0;
    }
}

int cortical_attention_disconnect_bio_async(cortical_attention_gain_t* attention) {
    /* WHAT: Validate input
     * WHY:  Guard clause
     */
    if (!attention) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention is NULL");

        return -1;

    }

    /* WHAT: Check if connected
     * WHY:  Only disconnect if connected
     */
    if (!attention->bio_async_enabled) {
        return 0;
    }

    /* WHAT: Unregister from router
     * WHY:  Clean shutdown
     */
    if (attention->bio_ctx) {
        bio_router_unregister_module(attention->bio_ctx);
        attention->bio_ctx = NULL;
    }

    attention->bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    return 0;
}

bool cortical_attention_is_bio_async_connected(
    const cortical_attention_gain_t* attention
) {
    /* WHAT: Validate input
     * WHY:  Guard clause
     */
    if (!attention) {
        return false;
    }

    /* WHAT: Return connection status
     * WHY:  Query bio-async state
     */
    return attention->bio_async_enabled;
}
