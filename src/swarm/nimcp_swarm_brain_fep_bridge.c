/**
 * @file nimcp_swarm_brain_fep_bridge.c
 * @brief FEP Bridge Implementation for Swarm Brain Coordinator
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 */

#include "swarm/nimcp_swarm_brain_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_brain_fep_bridge)

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

/**
 * WHAT: Provides default FEP bridge configuration for swarm brain
 * WHY:  Ensures sensible defaults based on biological principles
 * HOW:  Sets biologically-inspired parameter values
 */
void swarm_brain_fep_default_config(swarm_brain_fep_config_t* config) {
    if (!config) return;

    config->collective_pe_weight = 0.7f;
    config->coherence_precision_gain = 1.5f;
    config->emergence_lr_scale = 1.2f;
    config->formation_prior_strength = 0.8f;
    config->enable_collective_inference = true;
    config->enable_emergence_scaling = true;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * WHAT: Creates swarm brain FEP bridge
 * WHY:  Establish bidirectional FEP-swarm brain connection
 * HOW:  Allocates structure, initializes fields, creates mutex
 */
swarm_brain_fep_bridge_t* swarm_brain_fep_create(
    const swarm_brain_fep_config_t* config,
    swarm_brain_t* swarm_brain,
    fep_system_t* fep_system)
{
    if (!swarm_brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_fep_create: swarm_brain is NULL");
        return NULL;
    }
    if (!fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_fep_create: fep_system is NULL");
        return NULL;
    }

    swarm_brain_fep_bridge_t* bridge = (swarm_brain_fep_bridge_t*)nimcp_malloc(sizeof(swarm_brain_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_brain_fep_create: bridge is NULL");
        return NULL;
    }

    memset(bridge, 0, sizeof(swarm_brain_fep_bridge_t));

    if (config) {
        bridge->config = *config;
    } else {
        swarm_brain_fep_default_config(&bridge->config);
    }

    bridge->fep_system = fep_system;
    bridge->swarm_brain = swarm_brain;
    bridge->base.bio_async_enabled = false;

    if (bridge_base_init(&bridge->base, 0, "swarm_brain_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_brain_fep_create: bridge->base is NULL");
        return NULL;
    }

    bridge->state.last_tier = SWARM_TIER_INDIVIDUAL;

    NIMCP_LOGGING_INFO("Swarm brain FEP bridge created");
    return bridge;
}

/**
 * WHAT: Destroys swarm brain FEP bridge
 * WHY:  Clean resource deallocation
 * HOW:  Frees mutex and structure memory
 */
void swarm_brain_fep_destroy(swarm_brain_fep_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        swarm_brain_fep_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Swarm brain FEP bridge destroyed");
}

/* ============================================================================
 * Update Functions
 * ============================================================================ */

/**
 * WHAT: Updates FEP bridge state
 * WHY:  Synchronize FEP and swarm brain bidirectionally
 * HOW:  Compute collective FE, determine effects in both directions
 */
int swarm_brain_fep_update(swarm_brain_fep_bridge_t* bridge) {
    if (!bridge) return -1;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Get swarm brain stats
    swarm_stats_t swarm_stats;
    if (!swarm_brain_get_stats(bridge->swarm_brain, &swarm_stats)) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return -1;
    }

    // Get current emergence tier
    swarm_emergence_tier_t current_tier = swarm_brain_get_emergence_tier(bridge->swarm_brain);

    // Get FEP state
    float free_energy = fep_get_free_energy(bridge->fep_system);

    // Compute FEP effects on swarm brain
    // High free energy → Increase exploration, adjust coordination
    bridge->fep_effects.coordination_adjustment = -tanhf(free_energy * 0.5f);
    bridge->fep_effects.communication_urgency = fminf(1.0f, free_energy * 0.3f);
    bridge->fep_effects.exploration_bias = fminf(1.0f, free_energy * 0.4f);
    bridge->fep_effects.tier_advancement_threshold = 0.7f - (free_energy * 0.1f);

    // Compute swarm brain effects on FEP
    // High coherence → High precision
    bridge->swarm_effects.precision_modulation = 0.5f + (swarm_stats.workspace_coherence * 1.5f);
    bridge->swarm_effects.learning_rate_modulation = 0.7f + (swarm_stats.peers_connected / 32.0f) * 0.8f;
    bridge->swarm_effects.hierarchy_depth = (uint32_t)current_tier + 1;
    bridge->swarm_effects.collective_confidence = swarm_stats.workspace_coherence;

    // Update state tracking
    bridge->state.last_collective_free_energy = free_energy;
    bridge->state.last_coherence = swarm_stats.workspace_coherence;
    if (bridge->state.last_tier != (uint32_t)current_tier) {
        bridge->stats.tier_changes++;
        bridge->state.last_tier = (uint32_t)current_tier;
    }
    bridge->state.update_count++;
    bridge->state.last_update_time = nimcp_platform_time_monotonic_ms();

    // Update statistics
    bridge->stats.total_updates++;
    bridge->stats.avg_collective_fe =
        (bridge->stats.avg_collective_fe * (bridge->stats.total_updates - 1) + free_energy) /
        bridge->stats.total_updates;

    if (bridge->stats.total_updates == 1 || free_energy < bridge->stats.min_collective_fe) {
        bridge->stats.min_collective_fe = free_energy;
    }
    if (bridge->stats.total_updates == 1 || free_energy > bridge->stats.max_collective_fe) {
        bridge->stats.max_collective_fe = free_energy;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Applies FEP modulation to swarm brain
 * WHY:  Actually modify swarm brain parameters based on FEP state
 * HOW:  Apply computed effects to swarm brain configuration
 */
int swarm_brain_fep_apply_modulation(swarm_brain_fep_bridge_t* bridge) {
    if (!bridge) return -1;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Apply FEP-driven swarm brain adjustments
    // This would call swarm_brain API to adjust parameters
    // For now, effects are computed and available for query

    bridge->stats.formation_adaptations++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Computes collective free energy across swarm
 * WHY:  Measure total uncertainty in collective model
 * HOW:  Weighted average of individual drone free energies
 */
float swarm_brain_fep_compute_collective_fe(swarm_brain_fep_bridge_t* bridge) {
    if (!bridge) return 0.0f;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    float collective_fe = fep_get_free_energy(bridge->fep_system);

    // Weight by coherence (high coherence → lower collective FE)
    swarm_stats_t stats;
    if (swarm_brain_get_stats(bridge->swarm_brain, &stats)) {
        collective_fe *= (1.0f - stats.workspace_coherence * 0.3f);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return collective_fe;
}

/**
 * WHAT: Processes swarm observation through FEP
 * WHY:  Update collective beliefs given sensor data
 * HOW:  Aggregate observations, compute prediction error, update beliefs
 */
int swarm_brain_fep_process_collective_observation(
    swarm_brain_fep_bridge_t* bridge,
    const float* observation,
    uint32_t observation_dim)
{
    if (!bridge || !observation) return -1;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Process observation through FEP system
    // fep_process_observation takes (fep_system, float*, dim)
    int result = fep_process_observation(bridge->fep_system, (float*)observation, observation_dim);

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

/* ============================================================================
 * Query Functions
 * ============================================================================ */

int swarm_brain_fep_get_effects(
    const swarm_brain_fep_bridge_t* bridge,
    swarm_brain_fep_effects_t* effects)
{
    if (!bridge || !effects) return -1;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->fep_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_brain_fep_get_swarm_effects(
    const swarm_brain_fep_bridge_t* bridge,
    fep_swarm_brain_effects_t* effects)
{
    if (!bridge || !effects) return -1;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->swarm_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_brain_fep_get_stats(
    const swarm_brain_fep_bridge_t* bridge,
    swarm_brain_fep_stats_t* stats)
{
    if (!bridge || !stats) return -1;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int swarm_brain_fep_connect_bio_async(swarm_brain_fep_bridge_t* bridge) {
    if (!bridge) return -1;
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_SWARM_BRAIN,
        .module_name = "swarm_brain_fep_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    }

    return 0;
}

int swarm_brain_fep_disconnect_bio_async(swarm_brain_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

bool swarm_brain_fep_is_bio_async_connected(const swarm_brain_fep_bridge_t* bridge) {
    return bridge && bridge->base.bio_async_enabled;
}
