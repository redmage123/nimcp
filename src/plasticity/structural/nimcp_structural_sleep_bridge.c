/**
 * @file nimcp_structural_sleep_bridge.c
 * @brief Sleep-Structural Plasticity Bridge Implementation
 */

#include "plasticity/structural/nimcp_structural_sleep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(structural_sleep_bridge)

/* Security integration */
/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct structural_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    structural_sleep_config_t config;

    /* System handles */
    sleep_system_t sleep_system;
    structural_plasticity_system_t* structural_system;

    /* Current state */
    structural_sleep_effects_t effects;

    /* Statistics */
    uint64_t total_consolidations;
    uint64_t total_rem_prunings;
    uint64_t total_updates;

    /* Thread safety */
    nimcp_platform_mutex_t* mutex;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(structural_sleep_bridge, struct structural_sleep_bridge_struct)

/* ============================================================================
 * Helper Functions Implementation
 * ============================================================================ */

float structural_sleep_get_formation_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:
            return STRUCTURAL_SLEEP_FORMATION_AWAKE;
        case SLEEP_STATE_DROWSY:
            return STRUCTURAL_SLEEP_FORMATION_DROWSY;
        case SLEEP_STATE_LIGHT_NREM:
            return STRUCTURAL_SLEEP_FORMATION_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:
            return STRUCTURAL_SLEEP_FORMATION_DEEP_NREM;
        case SLEEP_STATE_REM:
            return STRUCTURAL_SLEEP_FORMATION_REM;
        default:
            return 1.0f;
    }
}

float structural_sleep_get_consolidation_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:
            return STRUCTURAL_SLEEP_CONSOLIDATION_AWAKE;
        case SLEEP_STATE_DROWSY:
            return STRUCTURAL_SLEEP_CONSOLIDATION_DROWSY;
        case SLEEP_STATE_LIGHT_NREM:
            return STRUCTURAL_SLEEP_CONSOLIDATION_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:
            return STRUCTURAL_SLEEP_CONSOLIDATION_DEEP_NREM;
        case SLEEP_STATE_REM:
            return STRUCTURAL_SLEEP_CONSOLIDATION_REM;
        default:
            return 1.0f;
    }
}

float structural_sleep_get_pruning_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:
            return STRUCTURAL_SLEEP_PRUNING_AWAKE;
        case SLEEP_STATE_DROWSY:
            return STRUCTURAL_SLEEP_PRUNING_DROWSY;
        case SLEEP_STATE_LIGHT_NREM:
            return STRUCTURAL_SLEEP_PRUNING_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:
            return STRUCTURAL_SLEEP_PRUNING_DEEP_NREM;
        case SLEEP_STATE_REM:
            return STRUCTURAL_SLEEP_PRUNING_REM;
        default:
            return 1.0f;
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int structural_sleep_default_config(structural_sleep_config_t* config) {
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_sleep_default_config: config is NULL");
        return -1;
    }

    config->enable_sleep_consolidation = true;
    config->enable_rem_pruning = true;
    config->enable_formation_modulation = true;
    config->consolidation_strength = 1.0f;
    config->pruning_strength = 1.0f;

    return 0;
}

structural_sleep_bridge_t structural_sleep_bridge_create(
    const structural_sleep_config_t* config,
    sleep_system_t sleep_system,
    structural_plasticity_system_t* structural_system
) {
    if (!sleep_system) {
        NIMCP_LOGGING_ERROR("NULL sleep_system parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_sleep_bridge_create: sleep_system is NULL");
        return NULL;
    }
    if (!structural_system) {
        NIMCP_LOGGING_ERROR("NULL structural_system parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_sleep_bridge_create: structural_system is NULL");
        return NULL;
    }

    struct structural_sleep_bridge_struct* bridge =
        (struct structural_sleep_bridge_struct*)nimcp_malloc(sizeof(*bridge));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate sleep bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "structural_sleep_bridge_create: bridge allocation failed");
        return NULL;
    }

    memset(bridge, 0, sizeof(*bridge));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        structural_sleep_default_config(&bridge->config);
    }

    bridge->sleep_system = sleep_system;
    bridge->structural_system = structural_system;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "structural_sleep") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "structural_sleep_bridge_create: bridge_base_init failed");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "structural_sleep_bridge_create: mutex creation failed");
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize effects */
    bridge->effects.formation_rate_factor = 1.0f;
    bridge->effects.consolidation_boost = 1.0f;
    bridge->effects.pruning_rate_factor = 1.0f;
    bridge->effects.current_state = SLEEP_STATE_AWAKE;

    NIMCP_LOGGING_INFO("Structural-sleep bridge created");
    return bridge;
}

void structural_sleep_bridge_destroy(structural_sleep_bridge_t bridge) {
    if (!bridge) return;

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Structural-sleep bridge destroyed");
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int structural_sleep_update(structural_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_sleep_update: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Query sleep system state */
    sleep_state_t current_state = sleep_get_current_state(bridge->sleep_system);
    float sleep_pressure = sleep_get_pressure(bridge->sleep_system);

    /* Update effects */
    bridge->effects.current_state = current_state;
    bridge->effects.sleep_pressure = sleep_pressure;

    /* Compute modulation factors */
    bridge->effects.formation_rate_factor =
        structural_sleep_get_formation_factor(current_state);

    bridge->effects.consolidation_boost =
        structural_sleep_get_consolidation_factor(current_state) *
        bridge->config.consolidation_strength;

    bridge->effects.pruning_rate_factor =
        structural_sleep_get_pruning_factor(current_state) *
        bridge->config.pruning_strength;

    /* Set activity flags */
    bridge->effects.active_consolidation =
        (current_state == SLEEP_STATE_LIGHT_NREM ||
         current_state == SLEEP_STATE_DEEP_NREM) &&
        bridge->config.enable_sleep_consolidation;

    bridge->effects.active_pruning =
        (current_state == SLEEP_STATE_REM) &&
        bridge->config.enable_rem_pruning;

    bridge->total_updates++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int structural_sleep_get_effects(
    const structural_sleep_bridge_t bridge,
    structural_sleep_effects_t* effects
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_sleep_get_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_sleep_get_effects: effects is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int structural_sleep_consolidate_tagged(structural_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_sleep_consolidate_tagged: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Only consolidate during NREM sleep */
    if (!bridge->effects.active_consolidation) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Get all spines and consolidate tagged nascent ones */
    uint32_t total_spines =
        structural_plasticity_get_total_spines(bridge->structural_system);

    uint32_t consolidated = 0;
    for (uint32_t i = 1; i <= total_spines; i++) {
        synapse_structural_state_t state;
        if (structural_plasticity_get_synapse_state(
                bridge->structural_system, i, &state) == 0) {

            /* Consolidate nascent spines that are tagged
             * NOTE: During sleep consolidation, we stabilize tagged spines regardless
             * of maturation progress, as NREM sleep accelerates stabilization.
             * This is biologically accurate: sleep consolidation can fast-track
             * newly formed synapses that have been tagged for consolidation.
             */
            if (state.state == SYNAPSE_STATE_NASCENT &&
                state.consolidation_tagged) {

                if (structural_plasticity_stabilize_synapse(
                        bridge->structural_system, i) == 0) {
                    consolidated++;
                }
            }

            /* Potentiate stable spines that are tagged */
            else if (state.state == SYNAPSE_STATE_STABLE &&
                     state.consolidation_tagged &&
                     state.ltp_accumulator > 5.0f) {

                structural_plasticity_potentiate_synapse(
                    bridge->structural_system, i);
            }
        }
    }

    bridge->total_consolidations += consolidated;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    if (consolidated > 0) {
        NIMCP_LOGGING_DEBUG("Consolidated %u tagged spines during NREM",
                           consolidated);
    }

    return 0;
}

int structural_sleep_prune_weak(structural_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_sleep_prune_weak: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Only prune during REM sleep */
    if (!bridge->effects.active_pruning) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Get all spines and mark weak ones for pruning */
    uint32_t total_spines =
        structural_plasticity_get_total_spines(bridge->structural_system);

    uint32_t pruned = 0;
    for (uint32_t i = 1; i <= total_spines; i++) {
        synapse_structural_state_t state;
        if (structural_plasticity_get_synapse_state(
                bridge->structural_system, i, &state) == 0) {

            /* Prune nascent spines that failed to consolidate */
            if (state.state == SYNAPSE_STATE_NASCENT &&
                !state.consolidation_tagged &&
                state.maturation_progress > 0.8f) {

                if (structural_plasticity_eliminate_synapse(
                        bridge->structural_system, i) == 0) {
                    pruned++;
                }
            }

            /* Prune stable spines with very low activity */
            else if (state.state == SYNAPSE_STATE_STABLE &&
                     state.recent_activity_hz < 0.5f &&
                     state.ltd_accumulator > 10.0f) {

                if (structural_plasticity_eliminate_synapse(
                        bridge->structural_system, i) == 0) {
                    pruned++;
                }
            }
        }
    }

    bridge->total_rem_prunings += pruned;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    if (pruned > 0) {
        NIMCP_LOGGING_DEBUG("Pruned %u weak spines during REM", pruned);
    }

    return 0;
}
