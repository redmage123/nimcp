//=============================================================================
// nimcp_hemispheric_immune_bridge.c - Hemispheric Brain Immune Integration
//=============================================================================
/**
 * @file nimcp_hemispheric_immune_bridge.c
 * @brief Implementation of hemispheric-immune bidirectional integration
 *
 * WHAT: Connects hemispheric brain with brain immune system
 * WHY:  Model asymmetric inflammation effects on lateralized processing
 * HOW:  Cytokine-driven modulation of per-hemisphere learning and callosum
 */

#include "core/brain/hemispheric/nimcp_hemispheric_immune_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(hemispheric_immune_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Get learning rate factor for left hemisphere based on inflammation
 */
static float get_left_lr_factor(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return HEMI_IMMUNE_LR_LEFT_NONE;
        case INFLAMMATION_LOCAL:    return HEMI_IMMUNE_LR_LEFT_LOCAL;
        case INFLAMMATION_REGIONAL: return HEMI_IMMUNE_LR_LEFT_REGIONAL;
        case INFLAMMATION_SYSTEMIC: return HEMI_IMMUNE_LR_LEFT_SYSTEMIC;
        case INFLAMMATION_STORM:    return HEMI_IMMUNE_LR_LEFT_STORM;
        default:                    return HEMI_IMMUNE_LR_LEFT_NONE;
    }
}

/**
 * @brief Get learning rate factor for right hemisphere based on inflammation
 */
static float get_right_lr_factor(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return HEMI_IMMUNE_LR_RIGHT_NONE;
        case INFLAMMATION_LOCAL:    return HEMI_IMMUNE_LR_RIGHT_LOCAL;
        case INFLAMMATION_REGIONAL: return HEMI_IMMUNE_LR_RIGHT_REGIONAL;
        case INFLAMMATION_SYSTEMIC: return HEMI_IMMUNE_LR_RIGHT_SYSTEMIC;
        case INFLAMMATION_STORM:    return HEMI_IMMUNE_LR_RIGHT_STORM;
        default:                    return HEMI_IMMUNE_LR_RIGHT_NONE;
    }
}

/**
 * @brief Get callosum bandwidth factor based on inflammation
 */
static float get_callosum_factor(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return HEMI_IMMUNE_CALLOSUM_NONE;
        case INFLAMMATION_LOCAL:    return HEMI_IMMUNE_CALLOSUM_LOCAL;
        case INFLAMMATION_REGIONAL: return HEMI_IMMUNE_CALLOSUM_REGIONAL;
        case INFLAMMATION_SYSTEMIC: return HEMI_IMMUNE_CALLOSUM_SYSTEMIC;
        case INFLAMMATION_STORM:    return HEMI_IMMUNE_CALLOSUM_STORM;
        default:                    return HEMI_IMMUNE_CALLOSUM_NONE;
    }
}

/**
 * @brief Get lateralization plasticity factor based on inflammation
 */
static float get_plasticity_factor(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return HEMI_IMMUNE_PLASTICITY_NONE;
        case INFLAMMATION_LOCAL:    return HEMI_IMMUNE_PLASTICITY_LOCAL;
        case INFLAMMATION_REGIONAL: return HEMI_IMMUNE_PLASTICITY_REGIONAL;
        case INFLAMMATION_SYSTEMIC: return HEMI_IMMUNE_PLASTICITY_SYSTEMIC;
        case INFLAMMATION_STORM:    return HEMI_IMMUNE_PLASTICITY_STORM;
        default:                    return HEMI_IMMUNE_PLASTICITY_NONE;
    }
}

/**
 * @brief Compute cytokine-weighted modulation factor
 *
 * WHAT: Combines cytokine levels with their cognitive impact weights
 * WHY:  Different cytokines affect cognition differently
 * HOW:  Weighted sum: IL-1β(35%) + TNF-α(30%) + IL-6(25%) + IFN-γ(10%)
 */
static float compute_cytokine_modulation(const float* cytokine_levels) {
    if (!cytokine_levels) return 0.0f;

    float modulation = 0.0f;

    // IL-1β - strongest cognitive impact (memory, attention)
    modulation += cytokine_levels[BRAIN_CYTOKINE_IL1] * HEMI_IMMUNE_IL1_IMPACT;

    // TNF-α - prefrontal cortex impact
    modulation += cytokine_levels[BRAIN_CYTOKINE_TNF] * HEMI_IMMUNE_TNF_IMPACT;

    // IL-6 - moderate general impact
    modulation += cytokine_levels[BRAIN_CYTOKINE_IL6] * HEMI_IMMUNE_IL6_IMPACT;

    // IFN-γ - quarantine signal, less direct cognitive effect
    modulation += cytokine_levels[BRAIN_CYTOKINE_IFN_GAMMA] * HEMI_IMMUNE_IFN_IMPACT;

    // Clamp to [0, 1]
    if (modulation < 0.0f) modulation = 0.0f;
    if (modulation > 1.0f) modulation = 1.0f;

    return modulation;
}

/**
 * @brief Update hemisphere effects based on inflammation and cytokines
 */
static void update_hemisphere_effects(
    hemisphere_immune_effects_t* effects,
    brain_inflammation_level_t level,
    const float* cytokine_levels,
    float vulnerability,
    bool is_left
) {
    if (!effects) return;

    // Base LR factor from inflammation level
    float base_lr = is_left ? get_left_lr_factor(level) : get_right_lr_factor(level);

    // Apply vulnerability coefficient
    float vuln_adjusted = 1.0f - ((1.0f - base_lr) * vulnerability);

    // Apply cytokine modulation (reduces LR further)
    float cytokine_mod = compute_cytokine_modulation(cytokine_levels);
    effects->learning_rate_factor = vuln_adjusted * (1.0f - cytokine_mod * 0.3f);

    // Attention factor follows similar pattern but less affected
    effects->attention_factor = vuln_adjusted * (1.0f - cytokine_mod * 0.2f);

    // Memory consolidation is sensitive to IL-1β
    float il1_impact = cytokine_levels ? cytokine_levels[BRAIN_CYTOKINE_IL1] : 0.0f;
    effects->memory_consolidation = vuln_adjusted * (1.0f - il1_impact * 0.4f);

    // Executive function affected by TNF-α (prefrontal)
    float tnf_impact = cytokine_levels ? cytokine_levels[BRAIN_CYTOKINE_TNF] : 0.0f;
    effects->executive_function = vuln_adjusted * (1.0f - tnf_impact * 0.35f);

    // Compensation flag set when one hemisphere is significantly impaired
    effects->is_compensating = false;

    // Clamp all factors to valid range
    if (effects->learning_rate_factor < 0.05f) effects->learning_rate_factor = 0.05f;
    if (effects->attention_factor < 0.1f) effects->attention_factor = 0.1f;
    if (effects->memory_consolidation < 0.1f) effects->memory_consolidation = 0.1f;
    if (effects->executive_function < 0.1f) effects->executive_function = 0.1f;
}

/**
 * @brief Update callosum effects based on inflammation
 */
static void update_callosum_effects(
    callosum_immune_effects_t* effects,
    brain_inflammation_level_t level,
    const float* cytokine_levels,
    float sensitivity
) {
    if (!effects) return;

    // Base bandwidth from inflammation level
    float base_bandwidth = get_callosum_factor(level);

    // Apply sensitivity coefficient
    effects->bandwidth_factor = base_bandwidth * sensitivity + (1.0f - sensitivity);

    // Latency increases inversely with bandwidth
    effects->latency_multiplier = 1.0f / effects->bandwidth_factor;
    if (effects->latency_multiplier > 5.0f) {
        effects->latency_multiplier = 5.0f;  // Cap at 5x latency
    }

    // Reliability degrades with inflammation
    effects->reliability_factor = effects->bandwidth_factor;

    // Degraded flag for severe impairment
    effects->degraded = (effects->bandwidth_factor < 0.5f);

    // Cytokine modulation - chronic inflammation affects myelination
    float cytokine_mod = compute_cytokine_modulation(cytokine_levels);
    effects->bandwidth_factor *= (1.0f - cytokine_mod * 0.2f);
    effects->reliability_factor *= (1.0f - cytokine_mod * 0.15f);

    // Clamp
    if (effects->bandwidth_factor < 0.05f) effects->bandwidth_factor = 0.05f;
    if (effects->reliability_factor < 0.1f) effects->reliability_factor = 0.1f;
}

/**
 * @brief Update lateralization effects
 */
static void update_lateralization_effects(
    lateralization_immune_effects_t* effects,
    brain_inflammation_level_t level,
    const hemisphere_immune_effects_t* left,
    const hemisphere_immune_effects_t* right
) {
    if (!effects) return;

    // Plasticity factor from inflammation level
    effects->plasticity_factor = get_plasticity_factor(level);

    // Shift toward less affected hemisphere
    if (left && right) {
        float left_impairment = 1.0f - left->learning_rate_factor;
        float right_impairment = 1.0f - right->learning_rate_factor;
        float diff = left_impairment - right_impairment;

        // Positive diff = left more impaired = shift toward right
        effects->shift_toward_right = diff * 0.5f;  // Moderate shift
        if (effects->shift_toward_right < -1.0f) effects->shift_toward_right = -1.0f;
        if (effects->shift_toward_right > 1.0f) effects->shift_toward_right = 1.0f;
    } else {
        effects->shift_toward_right = 0.0f;
    }

    // Emergency bilateral for storm-level inflammation
    effects->emergency_bilateral = (level == INFLAMMATION_STORM);
}

/**
 * @brief Check and trigger compensation between hemispheres
 */
static void check_compensation(
    hemispheric_immune_bridge_t* bridge
) {
    if (!bridge || !bridge->config.enable_compensation) return;

    float left_lr = bridge->left_effects.learning_rate_factor;
    float right_lr = bridge->right_effects.learning_rate_factor;
    float threshold = bridge->config.compensation_threshold;

    // Check if left needs compensation from right
    if (left_lr < threshold && right_lr > threshold) {
        bridge->right_effects.is_compensating = true;
        bridge->stats.compensation_events++;
    }
    // Check if right needs compensation from left
    else if (right_lr < threshold && left_lr > threshold) {
        bridge->left_effects.is_compensating = true;
        bridge->stats.compensation_events++;
    }
    // Neither compensating
    else {
        bridge->left_effects.is_compensating = false;
        bridge->right_effects.is_compensating = false;
    }
}

//=============================================================================
// Lifecycle API
//=============================================================================

hemispheric_immune_config_t hemispheric_immune_default_config(void) {
    hemispheric_immune_config_t config = {
        .left_vulnerability = 0.7f,      // Left more vulnerable (language centers)
        .right_vulnerability = 0.5f,     // Right more resilient
        .callosum_sensitivity = 0.8f,    // Callosum sensitive to inflammation

        .enable_compensation = true,
        .compensation_threshold = 0.5f,  // Compensate when LR drops below 50%

        .enable_immune_plasticity = true,
        .plasticity_recovery_rate = 0.1f,

        .enable_bio_async = true
    };
    return config;
}

hemispheric_immune_bridge_t* hemispheric_immune_create(
    const hemispheric_immune_config_t* config,
    hemispheric_brain_t* brain,
    brain_immune_system_t* immune
) {
    // Validate inputs
    if (!brain || !immune) {
        NIMCP_LOGGING_ERROR("hemispheric_immune_create: NULL brain or immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hemispheric_immune_create: required parameter is NULL (brain, immune)");
        return NULL;
    }

    // Allocate bridge
    hemispheric_immune_bridge_t* bridge = nimcp_malloc(sizeof(hemispheric_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("hemispheric_immune_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }
    memset(bridge, 0, sizeof(hemispheric_immune_bridge_t));

    // Connect systems
    bridge->brain = brain;
    bridge->immune_system = immune;

    // Apply configuration
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = hemispheric_immune_default_config();
    }

    // Initialize effects to default (no inflammation)
    bridge->left_effects.learning_rate_factor = 1.0f;
    bridge->left_effects.attention_factor = 1.0f;
    bridge->left_effects.memory_consolidation = 1.0f;
    bridge->left_effects.executive_function = 1.0f;
    bridge->left_effects.is_compensating = false;

    bridge->right_effects = bridge->left_effects;

    bridge->callosum_effects.bandwidth_factor = 1.0f;
    bridge->callosum_effects.latency_multiplier = 1.0f;
    bridge->callosum_effects.reliability_factor = 1.0f;
    bridge->callosum_effects.degraded = false;

    bridge->lateralization_effects.plasticity_factor = 1.0f;
    bridge->lateralization_effects.shift_toward_right = 0.0f;
    bridge->lateralization_effects.emergency_bilateral = false;

    bridge->current_inflammation = INFLAMMATION_NONE;

    // Initialize bridge base (allocates mutex, sets module name)
    if (bridge_base_init(&bridge->base, 0, "hemispheric_immune") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    bridge->initialized = true;

    NIMCP_LOGGING_INFO("Created hemispheric immune bridge");
    return bridge;
}

void hemispheric_immune_destroy(hemispheric_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Cleanup bridge base (disconnects bio-async, destroys+frees mutex) */
    bridge_base_cleanup(&bridge->base);

    bridge->initialized = false;
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Destroyed hemispheric immune bridge");
}

//=============================================================================
// Update API
//=============================================================================

int hemispheric_immune_update(hemispheric_immune_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    nimcp_mutex_lock(bridge->base.mutex);

    // Get current inflammation from immune system (unless manually overridden)
    if (bridge->immune_system && !bridge->inflammation_override) {
        bridge->current_inflammation = brain_immune_get_inflammation_level(bridge->immune_system);

        // Get cytokine levels
        for (int i = 0; i < BRAIN_CYTOKINE_COUNT; i++) {
            bridge->cytokine_levels[i] = brain_immune_get_cytokine_level(bridge->immune_system, i);
        }
    }

    // Update statistics
    if (bridge->cytokine_levels[BRAIN_CYTOKINE_IL1] +
        bridge->cytokine_levels[BRAIN_CYTOKINE_IL6] +
        bridge->cytokine_levels[BRAIN_CYTOKINE_TNF] > bridge->stats.max_inflammation_seen) {
        bridge->stats.max_inflammation_seen =
            bridge->cytokine_levels[BRAIN_CYTOKINE_IL1] +
            bridge->cytokine_levels[BRAIN_CYTOKINE_IL6] +
            bridge->cytokine_levels[BRAIN_CYTOKINE_TNF];
    }

    // Update left hemisphere effects
    update_hemisphere_effects(
        &bridge->left_effects,
        bridge->current_inflammation,
        bridge->cytokine_levels,
        bridge->config.left_vulnerability,
        true
    );

    // Update right hemisphere effects
    update_hemisphere_effects(
        &bridge->right_effects,
        bridge->current_inflammation,
        bridge->cytokine_levels,
        bridge->config.right_vulnerability,
        false
    );

    // Update callosum effects
    update_callosum_effects(
        &bridge->callosum_effects,
        bridge->current_inflammation,
        bridge->cytokine_levels,
        bridge->config.callosum_sensitivity
    );

    // Update lateralization effects
    update_lateralization_effects(
        &bridge->lateralization_effects,
        bridge->current_inflammation,
        &bridge->left_effects,
        &bridge->right_effects
    );

    // Check compensation
    check_compensation(bridge);

    // Track callosum degradation events
    if (bridge->callosum_effects.degraded) {
        bridge->stats.callosum_degradations++;
    }

    // Update running averages
    float n = (float)(bridge->stats.updates + 1);
    bridge->stats.avg_left_lr_factor =
        (bridge->stats.avg_left_lr_factor * (n - 1) + bridge->left_effects.learning_rate_factor) / n;
    bridge->stats.avg_right_lr_factor =
        (bridge->stats.avg_right_lr_factor * (n - 1) + bridge->right_effects.learning_rate_factor) / n;
    bridge->stats.avg_callosum_bandwidth =
        (bridge->stats.avg_callosum_bandwidth * (n - 1) + bridge->callosum_effects.bandwidth_factor) / n;

    bridge->stats.updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int hemispheric_immune_apply_modulation(hemispheric_immune_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized && bridge->brain, NIMCP_ERROR_NULL_POINTER, "bridge is NULL, not initialized, or brain is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    // Apply learning rate modulation to each hemisphere
    brain_hemisphere_t* left = hemispheric_brain_get_left(bridge->brain);
    brain_hemisphere_t* right = hemispheric_brain_get_right(bridge->brain);

    if (left) {
        // Get current LR and modulate
        float current_lr = brain_hemisphere_get_learning_rate(left);
        float modulated_lr = current_lr * bridge->left_effects.learning_rate_factor;
        brain_hemisphere_set_learning_rate(left, modulated_lr);
    }

    if (right) {
        float current_lr = brain_hemisphere_get_learning_rate(right);
        float modulated_lr = current_lr * bridge->right_effects.learning_rate_factor;
        brain_hemisphere_set_learning_rate(right, modulated_lr);
    }

    // Apply callosum bandwidth modulation
    corpus_callosum_t* callosum = hemispheric_brain_get_callosum(bridge->brain);
    if (callosum) {
        uint32_t base_bw = corpus_callosum_get_base_bandwidth(callosum);
        uint32_t modulated_bw = (uint32_t)(base_bw * bridge->callosum_effects.bandwidth_factor);
        callosum_set_bandwidth_limit(callosum, modulated_bw);
    }

    // Apply lateralization shift if plasticity enabled
    if (bridge->config.enable_immune_plasticity) {
        float shift = bridge->lateralization_effects.shift_toward_right;
        if (fabsf(shift) > 0.1f) {
            hemispheric_brain_apply_lateralization_shift(bridge->brain, shift);
        }
    }

    // Trigger emergency bilateral if needed
    if (bridge->lateralization_effects.emergency_bilateral) {
        hemispheric_brain_set_bilateral_mode(bridge->brain, true);
    }

    bridge->stats.modulations_applied++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Query API
//=============================================================================

hemisphere_immune_effects_t hemispheric_immune_get_left_effects(
    const hemispheric_immune_bridge_t* bridge
) {
    hemisphere_immune_effects_t effects = {0};
    if (!bridge || !bridge->initialized) {
        effects.learning_rate_factor = 1.0f;
        effects.attention_factor = 1.0f;
        effects.memory_consolidation = 1.0f;
        effects.executive_function = 1.0f;
        return effects;
    }
    return bridge->left_effects;
}

hemisphere_immune_effects_t hemispheric_immune_get_right_effects(
    const hemispheric_immune_bridge_t* bridge
) {
    hemisphere_immune_effects_t effects = {0};
    if (!bridge || !bridge->initialized) {
        effects.learning_rate_factor = 1.0f;
        effects.attention_factor = 1.0f;
        effects.memory_consolidation = 1.0f;
        effects.executive_function = 1.0f;
        return effects;
    }
    return bridge->right_effects;
}

callosum_immune_effects_t hemispheric_immune_get_callosum_effects(
    const hemispheric_immune_bridge_t* bridge
) {
    callosum_immune_effects_t effects = {0};
    if (!bridge || !bridge->initialized) {
        effects.bandwidth_factor = 1.0f;
        effects.latency_multiplier = 1.0f;
        effects.reliability_factor = 1.0f;
        return effects;
    }
    return bridge->callosum_effects;
}

float hemispheric_immune_get_effective_lr(
    const hemispheric_immune_bridge_t* bridge,
    hemisphere_id_t hemisphere,
    float base_lr
) {
    if (!bridge || !bridge->initialized) {
        return base_lr;
    }

    float factor = (hemisphere == HEMISPHERE_LEFT)
        ? bridge->left_effects.learning_rate_factor
        : bridge->right_effects.learning_rate_factor;

    return base_lr * factor;
}

uint32_t hemispheric_immune_get_effective_bandwidth(
    const hemispheric_immune_bridge_t* bridge,
    uint32_t base_bandwidth
) {
    if (!bridge || !bridge->initialized) {
        return base_bandwidth;
    }

    return (uint32_t)(base_bandwidth * bridge->callosum_effects.bandwidth_factor);
}

bool hemispheric_immune_is_compensating(
    const hemispheric_immune_bridge_t* bridge,
    hemisphere_id_t* compensating_hemisphere
) {
    if (!bridge || !bridge->initialized) {
        return false;
    }

    if (bridge->left_effects.is_compensating) {
        if (compensating_hemisphere) *compensating_hemisphere = HEMISPHERE_LEFT;
        return true;
    }
    if (bridge->right_effects.is_compensating) {
        if (compensating_hemisphere) *compensating_hemisphere = HEMISPHERE_RIGHT;
        return true;
    }

    return false;
}

//=============================================================================
// Control API
//=============================================================================

int hemispheric_immune_set_inflammation(
    hemispheric_immune_bridge_t* bridge,
    brain_inflammation_level_t level
) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->current_inflammation = level;
    bridge->inflammation_override = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    // Trigger update to recompute effects
    return hemispheric_immune_update(bridge);
}

int hemispheric_immune_trigger_emergency_bilateral(
    hemispheric_immune_bridge_t* bridge
) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->lateralization_effects.emergency_bilateral = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    // Apply immediately
    if (bridge->brain) {
        hemispheric_brain_set_bilateral_mode(bridge->brain, true);
    }

    NIMCP_LOGGING_WARN("Emergency bilateral processing triggered");
    return NIMCP_SUCCESS;
}

int hemispheric_immune_clear_emergency(hemispheric_immune_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->lateralization_effects.emergency_bilateral = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    // Restore normal mode
    if (bridge->brain) {
        hemispheric_brain_set_bilateral_mode(bridge->brain, false);
    }

    NIMCP_LOGGING_INFO("Emergency bilateral processing cleared");
    return NIMCP_SUCCESS;
}

//=============================================================================
// Statistics API
//=============================================================================

hemispheric_immune_stats_t hemispheric_immune_get_stats(
    const hemispheric_immune_bridge_t* bridge
) {
    hemispheric_immune_stats_t stats = {0};
    if (!bridge || !bridge->initialized) {
        return stats;
    }
    return bridge->stats;
}

void hemispheric_immune_reset_stats(hemispheric_immune_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) return;

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(hemispheric_immune_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
}

//=============================================================================
// Bio-async API
//=============================================================================

int hemispheric_immune_connect_bio_async(hemispheric_immune_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    if (bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;  // Already connected
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_HEMISPHERIC_IMMUNE,
        .module_name = "hemispheric_immune_bridge",
        .inbox_capacity = 16,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Hemispheric immune bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    return NIMCP_SUCCESS;
}

int hemispheric_immune_disconnect_bio_async(hemispheric_immune_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    if (!bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;  // Already disconnected
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Hemispheric immune bridge disconnected from bio-async router");

    return NIMCP_SUCCESS;
}
