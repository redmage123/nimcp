//=============================================================================
// nimcp_hemispheric_portia_bridge.c - Hemispheric Brain Portia Integration
//=============================================================================
/**
 * @file nimcp_hemispheric_portia_bridge.c
 * @brief Implementation of hemispheric-Portia bidirectional integration
 */

#include "core/brain/hemispheric/nimcp_hemispheric_portia_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(hemispheric_portia_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Get resource fraction for task type
 */
static float get_task_fraction(task_type_t task) {
    switch (task) {
        case TASK_TYPE_LANGUAGE:
        case TASK_TYPE_LOGIC:
            return HEMI_PORTIA_LEFT_DOMINANT_FRACTION;  // 70% left

        case TASK_TYPE_SPATIAL:
        case TASK_TYPE_CREATIVE:
        case TASK_TYPE_EMOTIONAL:
        case TASK_TYPE_ATTENTION:
            return HEMI_PORTIA_RIGHT_DOMINANT_FRACTION;  // 30% left (70% right)

        case TASK_TYPE_MOTOR:
        case TASK_TYPE_MEMORY:
        case TASK_TYPE_UNKNOWN:
        default:
            return HEMI_PORTIA_BALANCED_FRACTION;  // 50/50
    }
}

/**
 * @brief Convert fraction to tier pair
 */
static void fraction_to_tiers(
    float left_fraction,
    platform_tier_t global_tier,
    platform_tier_t* left_tier,
    platform_tier_t* right_tier
) {
    if (!left_tier || !right_tier) return;

    // Global tier sets the maximum
    platform_tier_t max_tier = global_tier;
    if (max_tier == PLATFORM_TIER_MINIMAL) {
        // Both at minimal
        *left_tier = PLATFORM_TIER_MINIMAL;
        *right_tier = PLATFORM_TIER_MINIMAL;
        return;
    }

    // Distribute based on fraction
    if (left_fraction > 0.6f) {
        // Left dominant
        *left_tier = max_tier;
        *right_tier = (max_tier > PLATFORM_TIER_CONSTRAINED)
            ? (platform_tier_t)(max_tier - 1)
            : PLATFORM_TIER_CONSTRAINED;
    } else if (left_fraction < 0.4f) {
        // Right dominant
        *right_tier = max_tier;
        *left_tier = (max_tier > PLATFORM_TIER_CONSTRAINED)
            ? (platform_tier_t)(max_tier - 1)
            : PLATFORM_TIER_CONSTRAINED;
    } else {
        // Balanced - both get same tier (one step down from max if needed)
        if (max_tier == PLATFORM_TIER_FULL) {
            *left_tier = PLATFORM_TIER_MEDIUM;
            *right_tier = PLATFORM_TIER_MEDIUM;
        } else {
            *left_tier = max_tier;
            *right_tier = max_tier;
        }
    }
}

/**
 * @brief Check if transition cooldown has passed
 */
static bool can_transition(const hemisphere_resource_state_t* state, uint32_t cooldown_ms) {
    if (!state) {
        return false;
    }

    // Simple time check (would use actual time in production)
    return !state->transition_in_progress;
}

/**
 * @brief Get current time in ms (simplified)
 */
static uint64_t get_time_ms(void) {
    // In production, this would use actual time
    static uint64_t fake_time = 0;
    return fake_time++;
}

//=============================================================================
// Lifecycle API
//=============================================================================

hemispheric_portia_config_t hemispheric_portia_default_config(void) {
    hemispheric_portia_config_t config = {
        .initial_strategy = ALLOCATION_BALANCED,
        .left_base_fraction = HEMI_PORTIA_BALANCED_FRACTION,

        .transition_cooldown_ms = HEMI_PORTIA_TRANSITION_COOLDOWN_MS,
        .enable_gradual_transition = true,

        .activity_threshold = 0.6f,
        .hysteresis_margin = 0.1f,

        .enable_bio_async = true,
        .subscribe_tier_events = true
    };
    return config;
}

hemispheric_portia_bridge_t* hemispheric_portia_create(
    const hemispheric_portia_config_t* config,
    hemispheric_brain_t* brain,
    portia_tier_switch_t portia
) {
    if (!brain) {
        NIMCP_LOGGING_ERROR("hemispheric_portia_create: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;
    }

    hemispheric_portia_bridge_t* bridge = nimcp_malloc(sizeof(hemispheric_portia_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("hemispheric_portia_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }
    memset(bridge, 0, sizeof(hemispheric_portia_bridge_t));

    // Connect systems
    bridge->brain = brain;
    bridge->portia_system = portia;

    // Apply configuration
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = hemispheric_portia_default_config();
    }

    // Initialize state
    bridge->current_strategy = bridge->config.initial_strategy;
    bridge->current_task = TASK_TYPE_UNKNOWN;
    bridge->global_tier = PLATFORM_TIER_MEDIUM;

    // Initialize left hemisphere state
    bridge->left_state.current_tier = PLATFORM_TIER_MEDIUM;
    bridge->left_state.target_tier = PLATFORM_TIER_MEDIUM;
    bridge->left_state.resource_fraction = bridge->config.left_base_fraction;
    bridge->left_state.transition_in_progress = false;
    bridge->left_state.last_transition_ms = 0;

    // Initialize right hemisphere state
    bridge->right_state.current_tier = PLATFORM_TIER_MEDIUM;
    bridge->right_state.target_tier = PLATFORM_TIER_MEDIUM;
    bridge->right_state.resource_fraction = 1.0f - bridge->config.left_base_fraction;
    bridge->right_state.transition_in_progress = false;
    bridge->right_state.last_transition_ms = 0;

    // Initialize bridge base (allocates mutex, sets module name)
    if (bridge_base_init(&bridge->base, 0, "hemispheric_portia") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    bridge->initialized = true;

    NIMCP_LOGGING_INFO("Created hemispheric Portia bridge");
    return bridge;
}

void hemispheric_portia_destroy(hemispheric_portia_bridge_t* bridge) {
    if (!bridge) return;

    /* Cleanup bridge base (disconnects bio-async, destroys+frees mutex) */
    bridge_base_cleanup(&bridge->base);

    bridge->initialized = false;
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Destroyed hemispheric Portia bridge");
}

//=============================================================================
// Update API
//=============================================================================

int hemispheric_portia_update(hemispheric_portia_bridge_t* bridge, float dt) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    nimcp_mutex_lock(bridge->base.mutex);

    // Compute resource fraction based on strategy
    float left_fraction = bridge->config.left_base_fraction;

    switch (bridge->current_strategy) {
        case ALLOCATION_BALANCED:
            left_fraction = HEMI_PORTIA_BALANCED_FRACTION;
            bridge->stats.time_in_balanced += dt;
            break;

        case ALLOCATION_LEFT_DOMINANT:
            left_fraction = HEMI_PORTIA_LEFT_DOMINANT_FRACTION;
            bridge->stats.time_in_left_dominant += dt;
            break;

        case ALLOCATION_RIGHT_DOMINANT:
            left_fraction = HEMI_PORTIA_RIGHT_DOMINANT_FRACTION;
            bridge->stats.time_in_right_dominant += dt;
            break;

        case ALLOCATION_TASK_DRIVEN:
            left_fraction = get_task_fraction(bridge->current_task);
            if (left_fraction > 0.55f) {
                bridge->stats.time_in_left_dominant += dt;
            } else if (left_fraction < 0.45f) {
                bridge->stats.time_in_right_dominant += dt;
            } else {
                bridge->stats.time_in_balanced += dt;
            }
            break;

        case ALLOCATION_ADAPTIVE:
        default:
            // Keep current fraction, would query activity levels
            left_fraction = bridge->left_state.resource_fraction;
            break;
    }

    // Update fractions
    bridge->left_state.resource_fraction = left_fraction;
    bridge->right_state.resource_fraction = 1.0f - left_fraction;

    // Compute target tiers based on fraction and global tier
    fraction_to_tiers(
        left_fraction,
        bridge->global_tier,
        &bridge->left_state.target_tier,
        &bridge->right_state.target_tier
    );

    // Check for tier transitions
    if (bridge->left_state.current_tier != bridge->left_state.target_tier) {
        if (can_transition(&bridge->left_state, bridge->config.transition_cooldown_ms)) {
            bridge->left_state.current_tier = bridge->left_state.target_tier;
            bridge->left_state.last_transition_ms = get_time_ms();
            bridge->stats.tier_transitions++;
        }
    }

    if (bridge->right_state.current_tier != bridge->right_state.target_tier) {
        if (can_transition(&bridge->right_state, bridge->config.transition_cooldown_ms)) {
            bridge->right_state.current_tier = bridge->right_state.target_tier;
            bridge->right_state.last_transition_ms = get_time_ms();
            bridge->stats.tier_transitions++;
        }
    }

    // Update average fraction statistic
    float n = (float)(bridge->stats.tier_transitions + bridge->stats.allocation_changes + 1);
    bridge->stats.avg_left_fraction =
        (bridge->stats.avg_left_fraction * (n - 1) + left_fraction) / n;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int hemispheric_portia_apply_allocation(hemispheric_portia_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized && bridge->brain, NIMCP_ERROR_NULL_POINTER, "bridge is NULL, not initialized, or brain is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    // Apply tiers to hemispheric brain
    hemispheric_brain_set_tier(
        bridge->brain,
        HEMISPHERE_LEFT,
        bridge->left_state.current_tier
    );

    hemispheric_brain_set_tier(
        bridge->brain,
        HEMISPHERE_RIGHT,
        bridge->right_state.current_tier
    );

    // Apply resource fraction
    hemispheric_brain_set_asymmetric_resources(
        bridge->brain,
        bridge->left_state.resource_fraction,
        false  // Already set tiers above
    );

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Control API
//=============================================================================

int hemispheric_portia_set_strategy(
    hemispheric_portia_bridge_t* bridge,
    allocation_strategy_t strategy
) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->current_strategy = strategy;
    bridge->stats.allocation_changes++;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Set allocation strategy to %d", (int)strategy);
    return NIMCP_SUCCESS;
}

int hemispheric_portia_set_task(
    hemispheric_portia_bridge_t* bridge,
    task_type_t task
) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->current_task = task;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int hemispheric_portia_set_fraction(
    hemispheric_portia_bridge_t* bridge,
    float left_fraction
) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");
    NIMCP_CHECK_THROW(left_fraction >= 0.0f && left_fraction <= 1.0f, NIMCP_ERROR_INVALID_PARAM, "left_fraction must be between 0.0 and 1.0");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->left_state.resource_fraction = left_fraction;
    bridge->right_state.resource_fraction = 1.0f - left_fraction;

    // Compute new target tiers
    fraction_to_tiers(
        left_fraction,
        bridge->global_tier,
        &bridge->left_state.target_tier,
        &bridge->right_state.target_tier
    );

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int hemispheric_portia_force_tier(
    hemispheric_portia_bridge_t* bridge,
    hemisphere_id_t hemisphere,
    platform_tier_t tier
) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    nimcp_mutex_lock(bridge->base.mutex);

    if (hemisphere == HEMISPHERE_LEFT) {
        bridge->left_state.current_tier = tier;
        bridge->left_state.target_tier = tier;
    } else {
        bridge->right_state.current_tier = tier;
        bridge->right_state.target_tier = tier;
    }

    bridge->stats.tier_transitions++;
    nimcp_mutex_unlock(bridge->base.mutex);

    // Apply immediately
    return hemispheric_portia_apply_allocation(bridge);
}

int hemispheric_portia_handle_tier_change(
    hemispheric_portia_bridge_t* bridge,
    platform_tier_t new_tier
) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->global_tier = new_tier;
    bridge->stats.portia_events_received++;

    // Recompute target tiers based on new global tier
    fraction_to_tiers(
        bridge->left_state.resource_fraction,
        new_tier,
        &bridge->left_state.target_tier,
        &bridge->right_state.target_tier
    );

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Received global tier change to %d", (int)new_tier);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Query API
//=============================================================================

allocation_strategy_t hemispheric_portia_get_strategy(
    const hemispheric_portia_bridge_t* bridge
) {
    if (!bridge || !bridge->initialized) {
        return ALLOCATION_BALANCED;
    }
    return bridge->current_strategy;
}

float hemispheric_portia_get_fraction(
    const hemispheric_portia_bridge_t* bridge,
    hemisphere_id_t hemisphere
) {
    if (!bridge || !bridge->initialized) {
        return 0.5f;
    }

    return (hemisphere == HEMISPHERE_LEFT)
        ? bridge->left_state.resource_fraction
        : bridge->right_state.resource_fraction;
}

platform_tier_t hemispheric_portia_get_tier(
    const hemispheric_portia_bridge_t* bridge,
    hemisphere_id_t hemisphere
) {
    if (!bridge || !bridge->initialized) {
        return PLATFORM_TIER_MEDIUM;
    }

    return (hemisphere == HEMISPHERE_LEFT)
        ? bridge->left_state.current_tier
        : bridge->right_state.current_tier;
}

hemispheric_portia_stats_t hemispheric_portia_get_stats(
    const hemispheric_portia_bridge_t* bridge
) {
    hemispheric_portia_stats_t stats = {0};
    if (!bridge || !bridge->initialized) {
        return stats;
    }
    return bridge->stats;
}

void hemispheric_portia_reset_stats(hemispheric_portia_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) return;

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(hemispheric_portia_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
}

//=============================================================================
// Bio-async API
//=============================================================================

int hemispheric_portia_connect_bio_async(hemispheric_portia_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    if (bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_HEMISPHERIC_PORTIA,
        .module_name = "hemispheric_portia_bridge",
        .inbox_capacity = 16,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Hemispheric Portia bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    return NIMCP_SUCCESS;
}

int hemispheric_portia_disconnect_bio_async(hemispheric_portia_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge && bridge->initialized, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or not initialized");

    if (!bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Hemispheric Portia bridge disconnected from bio-async router");

    return NIMCP_SUCCESS;
}
