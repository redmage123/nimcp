//=============================================================================
// nimcp_hemispheric_glial_bridge.c - Per-Hemisphere Glial Integration
//=============================================================================
/**
 * @file nimcp_hemispheric_glial_bridge.c
 * @brief Implementation of hemispheric glial system integration
 */

#include "core/brain/hemispheric/nimcp_hemispheric_glial_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(hemispheric_glial_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Update glial effects for a hemisphere
 */
static void update_hemisphere_effects(
    hemispheric_glial_bridge_t* bridge,
    hemisphere_id_t hemisphere
) {
    if (!bridge) return;

    hemisphere_glial_effects_t* effects = (hemisphere == HEMISPHERE_LEFT)
        ? &bridge->left_effects
        : &bridge->right_effects;

    glial_integration_t* glial = (hemisphere == HEMISPHERE_LEFT)
        ? bridge->left_glial
        : bridge->right_glial;

    if (!glial) {
        // No glial system, use defaults
        effects->astrocyte_modulation = 1.0f;
        effects->avg_calcium_level = 0.1f;  // Baseline
        effects->myelination_factor = 0.5f;
        effects->pruning_rate = 0.0f;
        effects->metabolic_support = 1.0f;
        effects->gliotransmitter_level = 0.0f;
        return;
    }

    // Get statistics from glial integration
    glial_integration_stats_t stats;
    if (glial_integration_get_stats(glial, &stats) == NIMCP_SUCCESS) {
        effects->astrocyte_modulation = stats.avg_synaptic_modulation;
        effects->myelination_factor = stats.avg_myelination_factor;
        effects->pruning_rate = stats.avg_pruning_rate;
    }

    // Get calcium from astrocyte network if available
    if (glial->astrocyte_network) {
        float avg_ca, max_ca, avg_glu;
        astrocyte_network_get_stats(
            glial->astrocyte_network,
            &avg_ca, &max_ca, &avg_glu
        );
        effects->avg_calcium_level = avg_ca;
        effects->gliotransmitter_level = avg_glu;
    }

    // Estimate metabolic support from ATP levels
    // Higher calcium often means higher activity demand
    float activity = fminf(1.0f, effects->avg_calcium_level / 5.0f);
    effects->metabolic_support = 1.0f - 0.3f * activity;  // Decreases with high activity
}

/**
 * @brief Process cross-hemisphere calcium wave
 */
static void process_cross_hemisphere_wave(
    hemispheric_glial_bridge_t* bridge,
    float dt
) {
    if (!bridge || !bridge->cross_state.wave_propagating) return;
    if (!bridge->config.enable_cross_hemisphere_waves) return;

    // Wave takes time to propagate through callosum
    // Simulate ~20ms crossing time
    uint64_t wave_duration_us = 20000;  // 20ms

    // Check if wave should arrive at target hemisphere
    // (simplified: would use actual timestamp in production)
    if (bridge->cross_state.wave_start_time > 0) {
        // Transfer calcium to target hemisphere
        float transfer = bridge->cross_state.calcium_transfer_rate *
                         bridge->config.calcium_transfer_coefficient * dt;

        hemisphere_glial_effects_t* target =
            (bridge->cross_state.wave_source == HEMISPHERE_LEFT)
            ? &bridge->right_effects
            : &bridge->left_effects;

        target->avg_calcium_level += transfer;

        // Clamp to reasonable max
        if (target->avg_calcium_level > 10.0f) {
            target->avg_calcium_level = 10.0f;
        }

        // Wave dissipates
        bridge->cross_state.calcium_transfer_rate *= 0.95f;
        if (bridge->cross_state.calcium_transfer_rate < 0.01f) {
            bridge->cross_state.wave_propagating = false;
            bridge->stats.cross_hemisphere_waves++;
        }
    }
}

/**
 * @brief Balance metabolic resources between hemispheres
 */
static void balance_metabolic_resources(
    hemispheric_glial_bridge_t* bridge,
    float dt
) {
    if (!bridge) return;
    if (bridge->config.metabolic_coupling_strength < 0.01f) return;

    // Compute metabolic differential
    float left_support = bridge->left_effects.metabolic_support;
    float right_support = bridge->right_effects.metabolic_support;
    float diff = left_support - right_support;

    // Transfer resources from higher to lower
    float transfer = diff * bridge->config.metabolic_coupling_strength * dt;

    bridge->left_effects.metabolic_support -= transfer * 0.5f;
    bridge->right_effects.metabolic_support += transfer * 0.5f;

    // Track flow direction
    bridge->cross_state.metabolic_flow = -transfer;  // Negative = L to R
    bridge->stats.metabolic_balance += transfer;
}

/**
 * @brief Get current time in microseconds (simplified)
 */
static uint64_t get_time_us(void) {
    static uint64_t fake_time = 0;
    return fake_time++;
}

//=============================================================================
// Lifecycle API
//=============================================================================

hemispheric_glial_config_t hemispheric_glial_default_config(void) {
    hemispheric_glial_config_t config = {
        .left_specialization = GLIAL_SPEC_LANGUAGE_DOMINANT,
        .right_specialization = GLIAL_SPEC_SPATIAL_DOMINANT,

        .left_astrocyte_density_factor = HEMI_GLIAL_ASTRO_DENSITY_LEFT,
        .right_astrocyte_density_factor = HEMI_GLIAL_ASTRO_DENSITY_RIGHT,
        .calcium_wave_threshold = ASTROCYTE_CALCIUM_WAVE_THRESHOLD_UM,

        .left_myelin_factor = 1.0f,
        .right_myelin_factor = 1.0f,

        .calcium_transfer_coefficient = HEMI_GLIAL_CALCIUM_TRANSFER_COEFF,
        .metabolic_coupling_strength = HEMI_GLIAL_METABOLIC_COUPLING,
        .enable_cross_hemisphere_waves = true,

        .enable_bio_async = true
    };
    return config;
}

hemispheric_glial_bridge_t* hemispheric_glial_create(
    const hemispheric_glial_config_t* config,
    hemispheric_brain_t* brain
) {
    if (!brain) {
        NIMCP_LOGGING_ERROR("hemispheric_glial_create: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;
    }

    hemispheric_glial_bridge_t* bridge = nimcp_malloc(sizeof(hemispheric_glial_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("hemispheric_glial_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }
    memset(bridge, 0, sizeof(hemispheric_glial_bridge_t));

    // Connect to brain
    bridge->brain = brain;

    // Apply configuration
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = hemispheric_glial_default_config();
    }

    // Initialize effects with defaults
    bridge->left_effects.astrocyte_modulation = 1.0f;
    bridge->left_effects.avg_calcium_level = ASTROCYTE_BASELINE_CALCIUM_UM;
    bridge->left_effects.myelination_factor = 0.5f;
    bridge->left_effects.metabolic_support = 1.0f;

    bridge->right_effects.astrocyte_modulation = 1.0f;
    bridge->right_effects.avg_calcium_level = ASTROCYTE_BASELINE_CALCIUM_UM;
    bridge->right_effects.myelination_factor = 0.5f;
    bridge->right_effects.metabolic_support = 1.0f;

    // Initialize cross-hemisphere state
    bridge->cross_state.calcium_transfer_rate = 0.0f;
    bridge->cross_state.metabolic_flow = 0.0f;
    bridge->cross_state.wave_propagating = false;

    // Initialize bridge base (allocates mutex, sets module name)
    if (bridge_base_init(&bridge->base, 0, "hemispheric_glial") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    bridge->initialized = true;

    NIMCP_LOGGING_INFO("Created hemispheric glial bridge");
    return bridge;
}

void hemispheric_glial_destroy(hemispheric_glial_bridge_t* bridge) {
    if (!bridge) return;

    /* Cleanup bridge base (disconnects bio-async, destroys+frees mutex) */
    bridge_base_cleanup(&bridge->base);

    // Note: We don't destroy left_glial/right_glial - caller owns them

    bridge->initialized = false;
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Destroyed hemispheric glial bridge");
}

//=============================================================================
// Update API
//=============================================================================

int hemispheric_glial_update(hemispheric_glial_bridge_t* bridge, float dt) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_glial_update: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    // Update per-hemisphere glial systems
    uint64_t timestamp = get_time_us();

    if (bridge->left_glial) {
        glial_integration_step(bridge->left_glial, timestamp);
    }
    if (bridge->right_glial) {
        glial_integration_step(bridge->right_glial, timestamp);
    }

    // Update computed effects
    update_hemisphere_effects(bridge, HEMISPHERE_LEFT);
    update_hemisphere_effects(bridge, HEMISPHERE_RIGHT);

    // Process cross-hemisphere calcium waves
    process_cross_hemisphere_wave(bridge, dt);

    // Balance metabolic resources
    balance_metabolic_resources(bridge, dt);

    // Update statistics
    bridge->stats.glial_updates++;
    bridge->stats.avg_left_calcium = bridge->left_effects.avg_calcium_level;
    bridge->stats.avg_right_calcium = bridge->right_effects.avg_calcium_level;
    bridge->stats.avg_left_myelination = bridge->left_effects.myelination_factor;
    bridge->stats.avg_right_myelination = bridge->right_effects.myelination_factor;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int hemispheric_glial_apply_modulation(hemispheric_glial_bridge_t* bridge) {
    if (!bridge || !bridge->initialized || !bridge->brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_glial_apply_modulation: required parameter is NULL (bridge, bridge->initialized, bridge->brain)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    // The glial effects are computed during update()
    // Here we would apply them to the hemispheric brain's processing

    // Apply myelin factors if brain has per-hemisphere tier control
    // (The actual effect is in the glial_integration_get_myelination_factor calls
    //  during neural network stepping)

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Astrocyte API
//=============================================================================

int hemispheric_glial_stimulate_astrocyte(
    hemispheric_glial_bridge_t* bridge,
    hemisphere_id_t hemisphere,
    uint32_t astrocyte_id,
    float intensity
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_glial_stimulate_astrocyte: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    glial_integration_t* glial = (hemisphere == HEMISPHERE_LEFT)
        ? bridge->left_glial
        : bridge->right_glial;

    if (!glial || !glial->astrocyte_network ||
        !glial->astrocyte_network->calcium_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hemispheric_glial_stimulate_astrocyte: operation failed");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    astrocyte_calcium_system_stimulate(
        glial->astrocyte_network->calcium_system,
        astrocyte_id,
        intensity
    );

    // Check if wave should cross hemispheres
    if (intensity >= bridge->config.calcium_wave_threshold &&
        bridge->config.enable_cross_hemisphere_waves) {
        bridge->cross_state.wave_propagating = true;
        bridge->cross_state.wave_source = hemisphere;
        bridge->cross_state.calcium_transfer_rate = intensity * 0.3f;
        bridge->cross_state.wave_start_time = get_time_us();
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

float hemispheric_glial_get_astrocyte_calcium(
    const hemispheric_glial_bridge_t* bridge,
    hemisphere_id_t hemisphere,
    uint32_t astrocyte_id
) {
    if (!bridge || !bridge->initialized) {
        return 0.0f;
    }

    glial_integration_t* glial = (hemisphere == HEMISPHERE_LEFT)
        ? bridge->left_glial
        : bridge->right_glial;

    if (!glial || !glial->astrocyte_network ||
        !glial->astrocyte_network->calcium_system) {
        return ASTROCYTE_BASELINE_CALCIUM_UM;
    }

    return astrocyte_calcium_system_get_calcium(
        glial->astrocyte_network->calcium_system,
        astrocyte_id
    );
}

int hemispheric_glial_trigger_cross_wave(
    hemispheric_glial_bridge_t* bridge,
    hemisphere_id_t source,
    float intensity
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_glial_trigger_cross_wave: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    if (!bridge->config.enable_cross_hemisphere_waves) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_glial_trigger_cross_wave: bridge->config is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->cross_state.wave_propagating = true;
    bridge->cross_state.wave_source = source;
    bridge->cross_state.calcium_transfer_rate = intensity;
    bridge->cross_state.wave_start_time = get_time_us();

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Triggered cross-hemisphere wave from %s, intensity %.2f",
        source == HEMISPHERE_LEFT ? "left" : "right", intensity);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Myelination API
//=============================================================================

float hemispheric_glial_get_myelination(
    const hemispheric_glial_bridge_t* bridge,
    hemisphere_id_t hemisphere,
    uint32_t neuron_id
) {
    if (!bridge || !bridge->initialized) {
        return 0.0f;
    }

    glial_integration_t* glial = (hemisphere == HEMISPHERE_LEFT)
        ? bridge->left_glial
        : bridge->right_glial;

    if (!glial) {
        return 0.0f;
    }

    float base_factor = glial_integration_get_myelination_factor(glial, neuron_id);

    // Apply hemisphere-specific bias
    float bias = (hemisphere == HEMISPHERE_LEFT)
        ? bridge->config.left_myelin_factor
        : bridge->config.right_myelin_factor;

    return fminf(1.0f, base_factor * bias);
}

int hemispheric_glial_set_myelin_factor(
    hemispheric_glial_bridge_t* bridge,
    hemisphere_id_t hemisphere,
    float factor
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_glial_set_myelin_factor: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    if (factor < 0.0f || factor > 2.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hemispheric_glial_set_myelin_factor: validation failed");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (hemisphere == HEMISPHERE_LEFT) {
        bridge->config.left_myelin_factor = factor;
    } else {
        bridge->config.right_myelin_factor = factor;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Metabolic API
//=============================================================================

float hemispheric_glial_get_metabolic_support(
    const hemispheric_glial_bridge_t* bridge,
    hemisphere_id_t hemisphere
) {
    if (!bridge || !bridge->initialized) {
        return 1.0f;
    }

    return (hemisphere == HEMISPHERE_LEFT)
        ? bridge->left_effects.metabolic_support
        : bridge->right_effects.metabolic_support;
}

int hemispheric_glial_transfer_metabolic(
    hemispheric_glial_bridge_t* bridge,
    float amount
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_glial_transfer_metabolic: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    // Positive amount = left to right
    bridge->left_effects.metabolic_support -= amount;
    bridge->right_effects.metabolic_support += amount;

    // Clamp to valid range
    bridge->left_effects.metabolic_support = fmaxf(0.0f, fminf(1.0f,
        bridge->left_effects.metabolic_support));
    bridge->right_effects.metabolic_support = fmaxf(0.0f, fminf(1.0f,
        bridge->right_effects.metabolic_support));

    bridge->cross_state.metabolic_flow = amount;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Pruning API
//=============================================================================

bool hemispheric_glial_should_prune(
    const hemispheric_glial_bridge_t* bridge,
    hemisphere_id_t hemisphere,
    uint32_t synapse_id
) {
    if (!bridge || !bridge->initialized) {
        return false;
    }

    glial_integration_t* glial = (hemisphere == HEMISPHERE_LEFT)
        ? bridge->left_glial
        : bridge->right_glial;

    if (!glial) {
        return false;
    }

    // Decode synapse_id to pre/post neurons (synapse_id = pre * 10000 + post)
    uint32_t pre_neuron = synapse_id / 10000;
    uint32_t post_neuron = synapse_id % 10000;

    return glial_integration_should_prune_synapse(glial, pre_neuron, post_neuron);
}

float hemispheric_glial_get_pruning_rate(
    const hemispheric_glial_bridge_t* bridge,
    hemisphere_id_t hemisphere
) {
    if (!bridge || !bridge->initialized) {
        return 0.0f;
    }

    return (hemisphere == HEMISPHERE_LEFT)
        ? bridge->left_effects.pruning_rate
        : bridge->right_effects.pruning_rate;
}

//=============================================================================
// Query API
//=============================================================================

hemisphere_glial_effects_t hemispheric_glial_get_effects(
    const hemispheric_glial_bridge_t* bridge,
    hemisphere_id_t hemisphere
) {
    hemisphere_glial_effects_t effects = {0};
    if (!bridge || !bridge->initialized) {
        return effects;
    }

    return (hemisphere == HEMISPHERE_LEFT)
        ? bridge->left_effects
        : bridge->right_effects;
}

hemispheric_glial_stats_t hemispheric_glial_get_stats(
    const hemispheric_glial_bridge_t* bridge
) {
    hemispheric_glial_stats_t stats = {0};
    if (!bridge || !bridge->initialized) {
        return stats;
    }
    return bridge->stats;
}

void hemispheric_glial_reset_stats(hemispheric_glial_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) return;

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(hemispheric_glial_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
}

cross_hemisphere_glial_t hemispheric_glial_get_cross_state(
    const hemispheric_glial_bridge_t* bridge
) {
    cross_hemisphere_glial_t state = {0};
    if (!bridge || !bridge->initialized) {
        return state;
    }
    return bridge->cross_state;
}

//=============================================================================
// Connection API
//=============================================================================

int hemispheric_glial_connect_glial(
    hemispheric_glial_bridge_t* bridge,
    hemisphere_id_t hemisphere,
    glial_integration_t* glial
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_glial_connect_glial: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (hemisphere == HEMISPHERE_LEFT) {
        bridge->left_glial = glial;
    } else {
        bridge->right_glial = glial;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected glial system to %s hemisphere",
        hemisphere == HEMISPHERE_LEFT ? "left" : "right");

    return NIMCP_SUCCESS;
}

//=============================================================================
// Bio-async API
//=============================================================================

int hemispheric_glial_connect_bio_async(hemispheric_glial_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_glial_connect_bio_async: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_HEMISPHERIC_GLIAL,
        .module_name = "hemispheric_glial_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Hemispheric glial bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    return NIMCP_SUCCESS;
}

int hemispheric_glial_disconnect_bio_async(hemispheric_glial_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_glial_disconnect_bio_async: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    if (!bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Hemispheric glial bridge disconnected from bio-async router");

    return NIMCP_SUCCESS;
}
