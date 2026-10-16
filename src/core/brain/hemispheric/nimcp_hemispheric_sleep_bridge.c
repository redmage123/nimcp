//=============================================================================
// nimcp_hemispheric_sleep_bridge.c - Hemispheric Brain Sleep Integration
//=============================================================================
/**
 * @file nimcp_hemispheric_sleep_bridge.c
 * @brief Implementation of hemispheric-sleep bidirectional integration
 */

#include "core/brain/hemispheric/nimcp_hemispheric_sleep_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(hemispheric_sleep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Get activity factor for left hemisphere based on sleep stage
 */
static float get_left_activity_factor(fep_sleep_stage_t stage) {
    switch (stage) {
        case SLEEP_STAGE_AWAKE: return HEMI_SLEEP_ACTIVITY_LEFT_AWAKE;
        case SLEEP_STAGE_NREM1: return HEMI_SLEEP_ACTIVITY_LEFT_NREM1;
        case SLEEP_STAGE_NREM2: return HEMI_SLEEP_ACTIVITY_LEFT_NREM2;
        case SLEEP_STAGE_NREM3: return HEMI_SLEEP_ACTIVITY_LEFT_NREM3;
        case SLEEP_STAGE_REM:   return HEMI_SLEEP_ACTIVITY_LEFT_REM;
        default:                return HEMI_SLEEP_ACTIVITY_LEFT_AWAKE;
    }
}

/**
 * @brief Get activity factor for right hemisphere based on sleep stage
 */
static float get_right_activity_factor(fep_sleep_stage_t stage) {
    switch (stage) {
        case SLEEP_STAGE_AWAKE: return HEMI_SLEEP_ACTIVITY_RIGHT_AWAKE;
        case SLEEP_STAGE_NREM1: return HEMI_SLEEP_ACTIVITY_RIGHT_NREM1;
        case SLEEP_STAGE_NREM2: return HEMI_SLEEP_ACTIVITY_RIGHT_NREM2;
        case SLEEP_STAGE_NREM3: return HEMI_SLEEP_ACTIVITY_RIGHT_NREM3;
        case SLEEP_STAGE_REM:   return HEMI_SLEEP_ACTIVITY_RIGHT_REM;
        default:                return HEMI_SLEEP_ACTIVITY_RIGHT_AWAKE;
    }
}

/**
 * @brief Get callosum recovery rate based on sleep stage
 */
static float get_callosum_recovery_rate(fep_sleep_stage_t stage) {
    switch (stage) {
        case SLEEP_STAGE_AWAKE: return HEMI_SLEEP_CALLOSUM_AWAKE_RATE;
        case SLEEP_STAGE_NREM1: return HEMI_SLEEP_CALLOSUM_NREM1_RATE;
        case SLEEP_STAGE_NREM2: return HEMI_SLEEP_CALLOSUM_NREM2_RATE;
        case SLEEP_STAGE_NREM3: return HEMI_SLEEP_CALLOSUM_NREM3_RATE;
        case SLEEP_STAGE_REM:   return HEMI_SLEEP_CALLOSUM_REM_RATE;
        default:                return HEMI_SLEEP_CALLOSUM_AWAKE_RATE;
    }
}

/**
 * @brief Get learning rate factor based on sleep stage
 */
static float get_lr_factor(fep_sleep_stage_t stage) {
    switch (stage) {
        case SLEEP_STAGE_AWAKE: return HEMI_SLEEP_LR_AWAKE;
        case SLEEP_STAGE_NREM1: return HEMI_SLEEP_LR_NREM1;
        case SLEEP_STAGE_NREM2: return HEMI_SLEEP_LR_NREM2;
        case SLEEP_STAGE_NREM3: return HEMI_SLEEP_LR_NREM3;
        case SLEEP_STAGE_REM:   return HEMI_SLEEP_LR_REM;
        default:                return HEMI_SLEEP_LR_AWAKE;
    }
}

/**
 * @brief Get plasticity factor based on sleep stage
 */
static float get_plasticity_factor(fep_sleep_stage_t stage) {
    switch (stage) {
        case SLEEP_STAGE_AWAKE: return HEMI_SLEEP_PLASTICITY_AWAKE;
        case SLEEP_STAGE_NREM1: return HEMI_SLEEP_PLASTICITY_NREM1;
        case SLEEP_STAGE_NREM2: return HEMI_SLEEP_PLASTICITY_NREM2;
        case SLEEP_STAGE_NREM3: return HEMI_SLEEP_PLASTICITY_NREM3;
        case SLEEP_STAGE_REM:   return HEMI_SLEEP_PLASTICITY_REM;
        default:                return HEMI_SLEEP_PLASTICITY_AWAKE;
    }
}

/**
 * @brief Get consolidation mode based on sleep stage
 */
static consolidation_mode_t get_consolidation_mode(fep_sleep_stage_t stage) {
    switch (stage) {
        case SLEEP_STAGE_AWAKE: return CONSOLIDATION_NONE;
        case SLEEP_STAGE_NREM1: return CONSOLIDATION_HEMISPHERE_LEFT;
        case SLEEP_STAGE_NREM2: return CONSOLIDATION_HEMISPHERE_RIGHT;
        case SLEEP_STAGE_NREM3: return CONSOLIDATION_BILATERAL;
        case SLEEP_STAGE_REM:   return CONSOLIDATION_INTERHEMISPHERIC;
        default:                return CONSOLIDATION_NONE;
    }
}

/**
 * @brief Update hemisphere effects based on sleep state
 */
static void update_hemisphere_effects(
    hemisphere_sleep_effects_t* effects,
    fep_sleep_stage_t stage,
    float depth_bias,
    bool is_left
) {
    if (!effects) return;

    // Base activity from sleep stage
    float base_activity = is_left
        ? get_left_activity_factor(stage)
        : get_right_activity_factor(stage);

    // Apply depth bias (left sleeps deeper, right more vigilant)
    if (is_left) {
        effects->activity_factor = base_activity * (1.0f - depth_bias * 0.2f);
    } else {
        effects->activity_factor = base_activity * (1.0f + depth_bias * 0.1f);
    }

    // Learning rate from sleep stage
    effects->learning_rate_factor = get_lr_factor(stage);

    // Consolidation strength increases in deep sleep
    effects->consolidation_strength = 1.0f - effects->activity_factor;
    if (effects->consolidation_strength < 0.0f) effects->consolidation_strength = 0.0f;

    // REM dreaming
    effects->is_dreaming = (stage == SLEEP_STAGE_REM);

    // Clamp values
    if (effects->activity_factor < 0.05f) effects->activity_factor = 0.05f;
    if (effects->activity_factor > 1.0f) effects->activity_factor = 1.0f;
}

/**
 * @brief Update callosum effects based on sleep state
 */
static void update_callosum_effects(
    callosum_sleep_effects_t* effects,
    fep_sleep_stage_t stage,
    float recovery_rate_mult,
    float max_efficiency
) {
    if (!effects) return;

    // Recovery rate from sleep stage
    float base_rate = get_callosum_recovery_rate(stage);
    effects->bandwidth_recovery = base_rate * recovery_rate_mult;

    // Apply recovery to efficiency
    effects->current_efficiency += effects->bandwidth_recovery * 0.01f;
    if (effects->current_efficiency > max_efficiency) {
        effects->current_efficiency = max_efficiency;
    }
    if (effects->current_efficiency < 0.1f) {
        effects->current_efficiency = 0.1f;
    }

    // Inter-hemispheric transfer highest in REM
    if (stage == SLEEP_STAGE_REM) {
        effects->interhemispheric_transfer = 0.8f;
    } else if (stage == SLEEP_STAGE_NREM3) {
        effects->interhemispheric_transfer = 0.3f;
    } else {
        effects->interhemispheric_transfer = 0.1f;
    }

    // Recovery active if not awake
    effects->recovery_active = (stage != SLEEP_STAGE_AWAKE);
}

/**
 * @brief Update lateralization effects
 */
static void update_lateralization_effects(
    lateralization_sleep_effects_t* effects,
    fep_sleep_stage_t stage
) {
    if (!effects) return;

    // Plasticity factor from sleep stage
    effects->plasticity_factor = get_plasticity_factor(stage);

    // Stabilization active during deep sleep
    effects->stabilization_active = (stage == SLEEP_STAGE_NREM2 ||
                                      stage == SLEEP_STAGE_NREM3);

    // Consolidation mode
    effects->consolidation_mode = get_consolidation_mode(stage);
}

//=============================================================================
// Lifecycle API
//=============================================================================

hemispheric_sleep_config_t hemispheric_sleep_default_config(void) {
    hemispheric_sleep_config_t config = {
        .left_sleep_depth_bias = 0.3f,    // Left tends to sleep 30% deeper
        .right_vigilance_bias = 0.2f,     // Right maintains 20% more vigilance

        .callosum_recovery_rate = 1.0f,   // Base recovery rate
        .max_callosum_efficiency = 1.0f,  // Full recovery possible

        .enable_consolidation = true,
        .consolidation_threshold = 0.3f,  // Need 30% sleep depth

        .enable_bio_async = true
    };
    return config;
}

hemispheric_sleep_bridge_t* hemispheric_sleep_create(
    const hemispheric_sleep_config_t* config,
    hemispheric_brain_t* brain,
    fep_sleep_system_t* sleep
) {
    // Validate inputs
    if (!brain) {
        NIMCP_LOGGING_ERROR("hemispheric_sleep_create: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;
    }

    // Allocate bridge
    hemispheric_sleep_bridge_t* bridge = nimcp_malloc(sizeof(hemispheric_sleep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("hemispheric_sleep_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }
    memset(bridge, 0, sizeof(hemispheric_sleep_bridge_t));

    // Connect systems
    bridge->brain = brain;
    bridge->sleep_system = sleep;

    // Apply configuration
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = hemispheric_sleep_default_config();
    }

    // Initialize effects to awake state
    bridge->left_effects.activity_factor = 1.0f;
    bridge->left_effects.learning_rate_factor = 1.0f;
    bridge->left_effects.consolidation_strength = 0.0f;
    bridge->left_effects.is_dreaming = false;

    bridge->right_effects = bridge->left_effects;

    bridge->callosum_effects.bandwidth_recovery = 0.0f;
    bridge->callosum_effects.current_efficiency = 1.0f;
    bridge->callosum_effects.interhemispheric_transfer = 0.1f;
    bridge->callosum_effects.recovery_active = false;

    bridge->lateralization_effects.plasticity_factor = 1.0f;
    bridge->lateralization_effects.stabilization_active = false;
    bridge->lateralization_effects.consolidation_mode = CONSOLIDATION_NONE;

    bridge->current_stage = SLEEP_STAGE_AWAKE;
    bridge->sleep_depth = 0.0f;

    // Initialize bridge base (allocates mutex, sets module name)
    if (bridge_base_init(&bridge->base, 0, "hemispheric_sleep") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    bridge->initialized = true;

    NIMCP_LOGGING_INFO("Created hemispheric sleep bridge");
    return bridge;
}

void hemispheric_sleep_destroy(hemispheric_sleep_bridge_t* bridge) {
    if (!bridge) return;

    /* Cleanup bridge base (disconnects bio-async, destroys+frees mutex) */
    bridge_base_cleanup(&bridge->base);

    bridge->initialized = false;
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Destroyed hemispheric sleep bridge");
}

//=============================================================================
// Update API
//=============================================================================

int hemispheric_sleep_update(hemispheric_sleep_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    nimcp_mutex_lock(bridge->base.mutex);

    // Get current sleep state from sleep system
    if (bridge->sleep_system) {
        bridge->current_stage = fep_sleep_get_stage(bridge->sleep_system);
        // Compute sleep depth from stage (0=wake, higher=deeper)
        switch (bridge->current_stage) {
            case SLEEP_STAGE_AWAKE: bridge->sleep_depth = 0.0f; break;
            case SLEEP_STAGE_NREM1: bridge->sleep_depth = 0.25f; break;
            case SLEEP_STAGE_NREM2: bridge->sleep_depth = 0.5f; break;
            case SLEEP_STAGE_NREM3: bridge->sleep_depth = 0.9f; break;
            case SLEEP_STAGE_REM:   bridge->sleep_depth = 0.4f; break;
            default:                bridge->sleep_depth = 0.0f; break;
        }
    }

    // Update left hemisphere effects
    update_hemisphere_effects(
        &bridge->left_effects,
        bridge->current_stage,
        bridge->config.left_sleep_depth_bias,
        true
    );

    // Update right hemisphere effects
    update_hemisphere_effects(
        &bridge->right_effects,
        bridge->current_stage,
        bridge->config.right_vigilance_bias,
        false
    );

    // Update callosum effects
    update_callosum_effects(
        &bridge->callosum_effects,
        bridge->current_stage,
        bridge->config.callosum_recovery_rate,
        bridge->config.max_callosum_efficiency
    );

    // Update lateralization effects
    update_lateralization_effects(
        &bridge->lateralization_effects,
        bridge->current_stage
    );

    // Track statistics
    if (bridge->callosum_effects.recovery_active) {
        if (bridge->callosum_effects.bandwidth_recovery > bridge->stats.peak_recovery_rate) {
            bridge->stats.peak_recovery_rate = bridge->callosum_effects.bandwidth_recovery;
        }
    }

    if (bridge->lateralization_effects.consolidation_mode == CONSOLIDATION_INTERHEMISPHERIC) {
        bridge->stats.interhemispheric_transfers++;
    }

    // Update running averages
    float n = (float)(bridge->stats.updates + 1);
    bridge->stats.avg_callosum_efficiency =
        (bridge->stats.avg_callosum_efficiency * (n - 1) + bridge->callosum_effects.current_efficiency) / n;

    bridge->stats.updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int hemispheric_sleep_apply_modulation(hemispheric_sleep_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized && bridge->brain, NIMCP_ERROR_NULL_POINTER, "bridge is NULL, not initialized, or brain is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    // Apply activity modulation to hemispheres
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

    // Apply callosum efficiency
    corpus_callosum_t* callosum = hemispheric_brain_get_callosum(bridge->brain);
    if (callosum) {
        uint32_t base_bw = corpus_callosum_get_base_bandwidth(callosum);
        uint32_t modulated_bw = (uint32_t)(base_bw * bridge->callosum_effects.current_efficiency);
        callosum_set_bandwidth_limit(callosum, modulated_bw);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Query API
//=============================================================================

hemisphere_sleep_effects_t hemispheric_sleep_get_left_effects(
    const hemispheric_sleep_bridge_t* bridge
) {
    hemisphere_sleep_effects_t effects = {0};
    if (!bridge || !bridge->initialized) {
        effects.activity_factor = 1.0f;
        effects.learning_rate_factor = 1.0f;
        return effects;
    }
    return bridge->left_effects;
}

hemisphere_sleep_effects_t hemispheric_sleep_get_right_effects(
    const hemispheric_sleep_bridge_t* bridge
) {
    hemisphere_sleep_effects_t effects = {0};
    if (!bridge || !bridge->initialized) {
        effects.activity_factor = 1.0f;
        effects.learning_rate_factor = 1.0f;
        return effects;
    }
    return bridge->right_effects;
}

callosum_sleep_effects_t hemispheric_sleep_get_callosum_effects(
    const hemispheric_sleep_bridge_t* bridge
) {
    callosum_sleep_effects_t effects = {0};
    if (!bridge || !bridge->initialized) {
        effects.current_efficiency = 1.0f;
        return effects;
    }
    return bridge->callosum_effects;
}

float hemispheric_sleep_get_activity_level(
    const hemispheric_sleep_bridge_t* bridge,
    hemisphere_id_t hemisphere
) {
    if (!bridge || !bridge->initialized) {
        return 1.0f;
    }

    return (hemisphere == HEMISPHERE_LEFT)
        ? bridge->left_effects.activity_factor
        : bridge->right_effects.activity_factor;
}

consolidation_mode_t hemispheric_sleep_get_consolidation_mode(
    const hemispheric_sleep_bridge_t* bridge
) {
    if (!bridge || !bridge->initialized) {
        return CONSOLIDATION_NONE;
    }
    return bridge->lateralization_effects.consolidation_mode;
}

bool hemispheric_sleep_is_dreaming(
    const hemispheric_sleep_bridge_t* bridge,
    hemisphere_id_t hemisphere
) {
    if (!bridge || !bridge->initialized) {
        return false;
    }

    return (hemisphere == HEMISPHERE_LEFT)
        ? bridge->left_effects.is_dreaming
        : bridge->right_effects.is_dreaming;
}

//=============================================================================
// Control API
//=============================================================================

int hemispheric_sleep_set_stage(
    hemispheric_sleep_bridge_t* bridge,
    fep_sleep_stage_t stage
) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->current_stage = stage;
    nimcp_mutex_unlock(bridge->base.mutex);

    // Trigger update to recompute effects
    return hemispheric_sleep_update(bridge);
}

int hemispheric_sleep_trigger_transfer(hemispheric_sleep_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    nimcp_mutex_lock(bridge->base.mutex);

    // Force inter-hemispheric transfer mode
    bridge->lateralization_effects.consolidation_mode = CONSOLIDATION_INTERHEMISPHERIC;
    bridge->callosum_effects.interhemispheric_transfer = 1.0f;
    bridge->stats.interhemispheric_transfers++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Triggered inter-hemispheric consolidation transfer");
    return NIMCP_SUCCESS;
}

int hemispheric_sleep_reset_callosum(hemispheric_sleep_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->callosum_effects.current_efficiency = 1.0f;
    bridge->callosum_effects.bandwidth_recovery = 0.0f;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Reset callosum efficiency to baseline");
    return NIMCP_SUCCESS;
}

//=============================================================================
// Statistics API
//=============================================================================

hemispheric_sleep_stats_t hemispheric_sleep_get_stats(
    const hemispheric_sleep_bridge_t* bridge
) {
    hemispheric_sleep_stats_t stats = {0};
    if (!bridge || !bridge->initialized) {
        return stats;
    }
    return bridge->stats;
}

void hemispheric_sleep_reset_stats(hemispheric_sleep_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) return;

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(hemispheric_sleep_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
}

//=============================================================================
// Bio-async API
//=============================================================================

int hemispheric_sleep_connect_bio_async(hemispheric_sleep_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    if (bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;  // Already connected
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_HEMISPHERIC_SLEEP,
        .module_name = "hemispheric_sleep_bridge",
        .inbox_capacity = 16,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Hemispheric sleep bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    return NIMCP_SUCCESS;
}

int hemispheric_sleep_disconnect_bio_async(hemispheric_sleep_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    if (!bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;  // Already disconnected
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Hemispheric sleep bridge disconnected from bio-async router");

    return NIMCP_SUCCESS;
}
