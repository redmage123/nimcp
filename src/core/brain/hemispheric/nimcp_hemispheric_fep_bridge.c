//=============================================================================
// nimcp_hemispheric_fep_bridge.c - Hemispheric Brain FEP Integration
//=============================================================================
/**
 * @file nimcp_hemispheric_fep_bridge.c
 * @brief Implementation of hemispheric-FEP bidirectional integration
 */

#include "core/brain/hemispheric/nimcp_hemispheric_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hemispheric_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Compute free energy from prediction error and precision
 *
 * WHAT: F = precision * error^2 + complexity penalty
 * WHY:  Higher precision amplifies prediction errors
 */
static float compute_free_energy(float prediction_error, float precision) {
    float error_term = precision * prediction_error * prediction_error;
    float complexity = 0.1f * (1.0f - precision);  // Complexity penalty
    return error_term + complexity;
}

/**
 * @brief Compute learning rate from prediction error and confidence
 *
 * WHAT: High error + low confidence = high learning rate
 * WHY:  Learn more when predictions are wrong and uncertain
 */
static float compute_learning_rate_factor(
    float prediction_error,
    float confidence,
    float min_lr,
    float max_lr
) {
    // High error increases learning, high confidence decreases it
    float base = 0.5f + 0.5f * prediction_error - 0.3f * confidence;

    // Clamp to range
    if (base < min_lr) base = min_lr;
    if (base > max_lr) base = max_lr;

    return base;
}

/**
 * @brief Update hemisphere FEP effects based on state
 */
static void update_hemisphere_effects(
    hemisphere_fep_effects_t* effects,
    float base_precision,
    float prior_width,
    float min_lr,
    float max_lr
) {
    if (!effects) return;

    // Precision can be modulated by prediction error (reduces precision when uncertain)
    float modulated_precision = base_precision;
    if (effects->prediction_error > 0.5f) {
        modulated_precision *= (1.0f - 0.2f * effects->prediction_error);
    }
    effects->precision = fmaxf(0.1f, fminf(1.0f, modulated_precision));

    effects->prior_width = prior_width;

    // Compute free energy
    effects->free_energy = compute_free_energy(
        effects->prediction_error,
        effects->precision
    );

    // Compute confidence (inverse of uncertainty)
    effects->confidence = 1.0f - effects->prior_width * effects->prediction_error;
    if (effects->confidence < 0.1f) effects->confidence = 0.1f;
    if (effects->confidence > 1.0f) effects->confidence = 1.0f;

    // Compute learning rate factor
    effects->learning_rate_factor = compute_learning_rate_factor(
        effects->prediction_error,
        effects->confidence,
        min_lr,
        max_lr
    );
}

/**
 * @brief Update callosum FEP effects for prediction transfer
 */
static void update_callosum_effects(
    callosum_fep_effects_t* effects,
    const hemisphere_fep_effects_t* left,
    const hemisphere_fep_effects_t* right,
    float transfer_rate,
    bool transfer_enabled
) {
    if (!effects || !left || !right) return;

    effects->transfer_rate = transfer_rate;
    effects->transfer_active = transfer_enabled;

    if (!transfer_enabled) {
        effects->integration_free_energy = 0.0f;
        effects->consensus_strength = 0.0f;
        return;
    }

    // Integration free energy is the disagreement between hemispheres
    float prediction_diff = fabsf(left->prediction_error - right->prediction_error);
    float precision_diff = fabsf(left->precision - right->precision);

    effects->integration_free_energy = 0.5f * (prediction_diff + precision_diff);

    // Consensus strength inversely related to disagreement
    effects->consensus_strength = 1.0f - effects->integration_free_energy;
    if (effects->consensus_strength < 0.0f) effects->consensus_strength = 0.0f;
}

/**
 * @brief Update global FEP state
 */
static void update_global_state(
    global_fep_state_t* state,
    const hemisphere_fep_effects_t* left,
    const hemisphere_fep_effects_t* right,
    const callosum_fep_effects_t* callosum
) {
    if (!state || !left || !right || !callosum) return;

    // Total free energy is sum of all components
    state->total_free_energy = left->free_energy + right->free_energy +
                               callosum->integration_free_energy;

    // Compute contributions as fractions
    if (state->total_free_energy > 0.001f) {
        state->left_contribution = left->free_energy / state->total_free_energy;
        state->right_contribution = right->free_energy / state->total_free_energy;
        state->integration_contribution = callosum->integration_free_energy /
                                           state->total_free_energy;
    } else {
        state->left_contribution = 0.33f;
        state->right_contribution = 0.33f;
        state->integration_contribution = 0.34f;
    }

    state->is_minimizing = true;
}

//=============================================================================
// Lifecycle API
//=============================================================================

hemispheric_fep_config_t hemispheric_fep_default_config(void) {
    hemispheric_fep_config_t config = {
        .left_base_precision = HEMI_FEP_LEFT_BASE_PRECISION,
        .right_base_precision = HEMI_FEP_RIGHT_BASE_PRECISION,

        .left_prior_width = HEMI_FEP_LEFT_PRIOR_WIDTH,
        .right_prior_width = HEMI_FEP_RIGHT_PRIOR_WIDTH,

        .min_learning_rate = HEMI_FEP_MIN_LEARNING_RATE,
        .max_learning_rate = HEMI_FEP_MAX_LEARNING_RATE,

        .callosum_transfer_rate = HEMI_FEP_CALLOSUM_TRANSFER_RATE,
        .callosum_latency_ms = HEMI_FEP_CALLOSUM_LATENCY_MS,

        .enable_precision_modulation = true,
        .enable_learning_modulation = true,
        .enable_callosum_transfer = true,

        .enable_bio_async = true
    };
    return config;
}

hemispheric_fep_bridge_t* hemispheric_fep_create(
    const hemispheric_fep_config_t* config,
    hemispheric_brain_t* brain,
    fep_orchestrator_t* fep
) {
    // Validate inputs
    if (!brain) {
        NIMCP_LOGGING_ERROR("hemispheric_fep_create: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;
    }

    // Allocate bridge
    hemispheric_fep_bridge_t* bridge = nimcp_malloc(sizeof(hemispheric_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("hemispheric_fep_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }
    memset(bridge, 0, sizeof(hemispheric_fep_bridge_t));

    // Connect systems
    bridge->brain = brain;
    bridge->fep_system = fep;

    // Apply configuration
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = hemispheric_fep_default_config();
    }

    // Initialize left hemisphere effects
    bridge->left_effects.precision = bridge->config.left_base_precision;
    bridge->left_effects.prior_width = bridge->config.left_prior_width;
    bridge->left_effects.free_energy = 0.1f;
    bridge->left_effects.prediction_error = 0.0f;
    bridge->left_effects.learning_rate_factor = 0.5f;
    bridge->left_effects.confidence = 0.8f;

    // Initialize right hemisphere effects
    bridge->right_effects.precision = bridge->config.right_base_precision;
    bridge->right_effects.prior_width = bridge->config.right_prior_width;
    bridge->right_effects.free_energy = 0.1f;
    bridge->right_effects.prediction_error = 0.0f;
    bridge->right_effects.learning_rate_factor = 0.5f;
    bridge->right_effects.confidence = 0.7f;

    // Initialize callosum effects
    bridge->callosum_effects.transfer_rate = bridge->config.callosum_transfer_rate;
    bridge->callosum_effects.integration_free_energy = 0.0f;
    bridge->callosum_effects.consensus_strength = 1.0f;
    bridge->callosum_effects.transfer_active = bridge->config.enable_callosum_transfer;

    // Initialize global state
    bridge->global_state.total_free_energy = 0.2f;
    bridge->global_state.left_contribution = 0.4f;
    bridge->global_state.right_contribution = 0.4f;
    bridge->global_state.integration_contribution = 0.2f;
    bridge->global_state.is_minimizing = false;

    // Initialize statistics
    bridge->stats.min_free_energy = 1000.0f;  // Will be updated

    // Initialize bridge base (allocates mutex, sets module name)
    if (bridge_base_init(&bridge->base, 0, "hemispheric_fep") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    bridge->initialized = true;

    NIMCP_LOGGING_INFO("Created hemispheric FEP bridge");
    return bridge;
}

void hemispheric_fep_destroy(hemispheric_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Cleanup bridge base (disconnects bio-async, destroys+frees mutex) */
    bridge_base_cleanup(&bridge->base);

    bridge->initialized = false;
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Destroyed hemispheric FEP bridge");
}

//=============================================================================
// Update API
//=============================================================================

int hemispheric_fep_update(hemispheric_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    nimcp_mutex_lock(bridge->base.mutex);

    // Update left hemisphere effects
    update_hemisphere_effects(
        &bridge->left_effects,
        bridge->config.left_base_precision,
        bridge->config.left_prior_width,
        bridge->config.min_learning_rate,
        bridge->config.max_learning_rate
    );

    // Update right hemisphere effects
    update_hemisphere_effects(
        &bridge->right_effects,
        bridge->config.right_base_precision,
        bridge->config.right_prior_width,
        bridge->config.min_learning_rate,
        bridge->config.max_learning_rate
    );

    // Update callosum effects
    update_callosum_effects(
        &bridge->callosum_effects,
        &bridge->left_effects,
        &bridge->right_effects,
        bridge->config.callosum_transfer_rate,
        bridge->config.enable_callosum_transfer
    );

    // Update global state
    update_global_state(
        &bridge->global_state,
        &bridge->left_effects,
        &bridge->right_effects,
        &bridge->callosum_effects
    );

    // Track statistics
    float n = (float)(bridge->stats.updates + 1);
    bridge->stats.avg_free_energy =
        (bridge->stats.avg_free_energy * (n - 1) + bridge->global_state.total_free_energy) / n;

    if (bridge->global_state.total_free_energy > bridge->stats.peak_free_energy) {
        bridge->stats.peak_free_energy = bridge->global_state.total_free_energy;
    }

    if (bridge->global_state.total_free_energy < bridge->stats.min_free_energy) {
        bridge->stats.min_free_energy = bridge->global_state.total_free_energy;
    }

    bridge->stats.avg_left_precision =
        (bridge->stats.avg_left_precision * (n - 1) + bridge->left_effects.precision) / n;
    bridge->stats.avg_right_precision =
        (bridge->stats.avg_right_precision * (n - 1) + bridge->right_effects.precision) / n;

    bridge->stats.updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int hemispheric_fep_apply_modulation(hemispheric_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->initialized && bridge->brain, NIMCP_ERROR_NULL_POINTER, "bridge is NULL, not initialized, or brain is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    // Apply learning rate modulation to hemispheres
    if (bridge->config.enable_learning_modulation) {
        brain_hemisphere_t* left = hemispheric_brain_get_left(bridge->brain);
        brain_hemisphere_t* right = hemispheric_brain_get_right(bridge->brain);

        if (left) {
            float current_lr = brain_hemisphere_get_learning_rate(left);
            float modulated_lr = current_lr * bridge->left_effects.learning_rate_factor;
            brain_hemisphere_set_learning_rate(left, modulated_lr);
        }

        if (right) {
            float current_lr = brain_hemisphere_get_learning_rate(right);
            float modulated_lr = current_lr * bridge->right_effects.learning_rate_factor;
            brain_hemisphere_set_learning_rate(right, modulated_lr);
        }
    }

    // Apply callosum modulation based on consensus
    if (bridge->config.enable_callosum_transfer) {
        corpus_callosum_t* callosum = hemispheric_brain_get_callosum(bridge->brain);
        if (callosum) {
            // Higher consensus = higher bandwidth efficiency
            uint32_t base_bw = corpus_callosum_get_base_bandwidth(callosum);
            float efficiency = 0.7f + 0.3f * bridge->callosum_effects.consensus_strength;
            uint32_t modulated_bw = (uint32_t)(base_bw * efficiency);
            callosum_set_bandwidth_limit(callosum, modulated_bw);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int hemispheric_fep_minimize_step(hemispheric_fep_bridge_t* bridge, float dt) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    nimcp_mutex_lock(bridge->base.mutex);

    // Gradient descent on prediction error (simplified)
    float left_gradient = bridge->left_effects.precision * bridge->left_effects.prediction_error;
    float right_gradient = bridge->right_effects.precision * bridge->right_effects.prediction_error;

    // Update prediction errors (decay towards zero = successful prediction)
    float decay_rate = 0.1f * dt;
    bridge->left_effects.prediction_error *= (1.0f - decay_rate - 0.05f * left_gradient);
    bridge->right_effects.prediction_error *= (1.0f - decay_rate - 0.05f * right_gradient);

    // Clamp to valid range
    if (bridge->left_effects.prediction_error < 0.0f) bridge->left_effects.prediction_error = 0.0f;
    if (bridge->right_effects.prediction_error < 0.0f) bridge->right_effects.prediction_error = 0.0f;

    bridge->stats.fe_minimization_steps++;

    nimcp_mutex_unlock(bridge->base.mutex);

    // Update effects based on new errors
    return hemispheric_fep_update(bridge);
}

//=============================================================================
// Query API
//=============================================================================

hemisphere_fep_effects_t hemispheric_fep_get_left_effects(
    const hemispheric_fep_bridge_t* bridge
) {
    hemisphere_fep_effects_t effects = {0};
    if (!bridge || !bridge->initialized) {
        effects.precision = HEMI_FEP_LEFT_BASE_PRECISION;
        effects.learning_rate_factor = 0.5f;
        effects.confidence = 0.5f;
        return effects;
    }
    return bridge->left_effects;
}

hemisphere_fep_effects_t hemispheric_fep_get_right_effects(
    const hemispheric_fep_bridge_t* bridge
) {
    hemisphere_fep_effects_t effects = {0};
    if (!bridge || !bridge->initialized) {
        effects.precision = HEMI_FEP_RIGHT_BASE_PRECISION;
        effects.learning_rate_factor = 0.5f;
        effects.confidence = 0.5f;
        return effects;
    }
    return bridge->right_effects;
}

callosum_fep_effects_t hemispheric_fep_get_callosum_effects(
    const hemispheric_fep_bridge_t* bridge
) {
    callosum_fep_effects_t effects = {0};
    if (!bridge || !bridge->initialized) {
        effects.transfer_rate = HEMI_FEP_CALLOSUM_TRANSFER_RATE;
        effects.consensus_strength = 1.0f;
        return effects;
    }
    return bridge->callosum_effects;
}

global_fep_state_t hemispheric_fep_get_global_state(
    const hemispheric_fep_bridge_t* bridge
) {
    global_fep_state_t state = {0};
    if (!bridge || !bridge->initialized) {
        state.left_contribution = 0.33f;
        state.right_contribution = 0.33f;
        state.integration_contribution = 0.34f;
        return state;
    }
    return bridge->global_state;
}

float hemispheric_fep_get_precision(
    const hemispheric_fep_bridge_t* bridge,
    hemisphere_id_t hemisphere
) {
    if (!bridge || !bridge->initialized) {
        return (hemisphere == HEMISPHERE_LEFT)
            ? HEMI_FEP_LEFT_BASE_PRECISION
            : HEMI_FEP_RIGHT_BASE_PRECISION;
    }

    return (hemisphere == HEMISPHERE_LEFT)
        ? bridge->left_effects.precision
        : bridge->right_effects.precision;
}

float hemispheric_fep_get_free_energy(
    const hemispheric_fep_bridge_t* bridge,
    hemisphere_id_t hemisphere
) {
    if (!bridge || !bridge->initialized) {
        return 0.0f;
    }

    return (hemisphere == HEMISPHERE_LEFT)
        ? bridge->left_effects.free_energy
        : bridge->right_effects.free_energy;
}

float hemispheric_fep_get_total_free_energy(
    const hemispheric_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->initialized) {
        return 0.0f;
    }
    return bridge->global_state.total_free_energy;
}

//=============================================================================
// Control API
//=============================================================================

int hemispheric_fep_set_precision(
    hemispheric_fep_bridge_t* bridge,
    hemisphere_id_t hemisphere,
    float precision
) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");
    NIMCP_FEP_CHECK_THROW(precision >= 0.0f && precision <= 1.0f, NIMCP_ERROR_INVALID_PARAM, "precision must be between 0.0 and 1.0");

    nimcp_mutex_lock(bridge->base.mutex);

    if (hemisphere == HEMISPHERE_LEFT) {
        bridge->left_effects.precision = precision;
    } else {
        bridge->right_effects.precision = precision;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int hemispheric_fep_inject_prediction_error(
    hemispheric_fep_bridge_t* bridge,
    hemisphere_id_t hemisphere,
    float error_magnitude
) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    if (error_magnitude < 0.0f) {
        error_magnitude = 0.0f;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (hemisphere == HEMISPHERE_LEFT) {
        bridge->left_effects.prediction_error = error_magnitude;
    } else {
        bridge->right_effects.prediction_error = error_magnitude;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    // Update effects
    return hemispheric_fep_update(bridge);
}

int hemispheric_fep_trigger_transfer(hemispheric_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    nimcp_mutex_lock(bridge->base.mutex);

    // Force prediction sharing
    bridge->callosum_effects.transfer_active = true;
    bridge->callosum_effects.transfer_rate = 1.0f;  // Maximum transfer

    // Average out prediction errors (consensus)
    float avg_error = (bridge->left_effects.prediction_error +
                       bridge->right_effects.prediction_error) / 2.0f;
    bridge->left_effects.prediction_error = avg_error;
    bridge->right_effects.prediction_error = avg_error;

    bridge->stats.prediction_transfers++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Triggered cross-hemisphere prediction transfer");
    return 0;
}

int hemispheric_fep_reset_free_energy(hemispheric_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->left_effects.prediction_error = 0.0f;
    bridge->left_effects.free_energy = 0.1f;

    bridge->right_effects.prediction_error = 0.0f;
    bridge->right_effects.free_energy = 0.1f;

    bridge->callosum_effects.integration_free_energy = 0.0f;

    bridge->global_state.total_free_energy = 0.2f;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Reset free energy to baseline");
    return 0;
}

//=============================================================================
// Statistics API
//=============================================================================

hemispheric_fep_stats_t hemispheric_fep_get_stats(
    const hemispheric_fep_bridge_t* bridge
) {
    hemispheric_fep_stats_t stats = {0};
    if (!bridge || !bridge->initialized) {
        return stats;
    }
    return bridge->stats;
}

void hemispheric_fep_reset_stats(hemispheric_fep_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) return;

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(hemispheric_fep_stats_t));
    bridge->stats.min_free_energy = 1000.0f;
    nimcp_mutex_unlock(bridge->base.mutex);
}

//=============================================================================
// Bio-async API
//=============================================================================

int hemispheric_fep_connect_bio_async(hemispheric_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    if (bridge->base.bio_async_enabled) {
        return 0;  // Already connected
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_HEMISPHERIC_FEP,
        .module_name = "hemispheric_fep_bridge",
        .inbox_capacity = 16,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Hemispheric FEP bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    return 0;
}

int hemispheric_fep_disconnect_bio_async(hemispheric_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    if (!bridge->base.bio_async_enabled) {
        return 0;  // Already disconnected
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Hemispheric FEP bridge disconnected from bio-async router");

    return 0;
}
