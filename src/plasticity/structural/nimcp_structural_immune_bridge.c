/**
 * @file nimcp_structural_immune_bridge.c
 * @brief Structural Plasticity-Immune Bridge Implementation
 */

#include "plasticity/structural/nimcp_structural_immune_bridge.h"
#include "constants/nimcp_constants.h"
#include "api/nimcp_api_exception.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(structural_immune_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(structural_immune_bridge)

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int structural_immune_default_config(structural_immune_config_t* config) {
    NIMCP_API_CHECK_NULL(config, -1, "Structural-immune config is NULL");

    config->enable_cytokine_modulation = true;
    config->enable_microglia_pruning = true;
    config->enable_complement_tagging = true;
    config->enable_inflammation_effects = true;
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->pruning_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->weak_spine_threshold = 2.0f;  /* Hz */
    config->high_density_threshold = 500.0f;

    return 0;
}

structural_immune_bridge_t* structural_immune_bridge_create(
    const structural_immune_config_t* config,
    brain_immune_system_t* immune_system,
    structural_plasticity_system_t* structural_system
) {
    if (!immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_immune_bridge_create: immune_system is NULL");
        return NULL;
    }
    if (!structural_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_immune_bridge_create: structural_system is NULL");
        return NULL;
    }

    structural_immune_bridge_t* bridge =
        (structural_immune_bridge_t*)nimcp_malloc(sizeof(*bridge));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "structural_immune_bridge_create: bridge allocation failed");
        return NULL;
    }

    memset(bridge, 0, sizeof(*bridge));

    /* Apply configuration */
    structural_immune_config_t local_config;
    if (config) {
        local_config = *config;
    } else {
        structural_immune_default_config(&local_config);
    }

    bridge->enable_cytokine_modulation = local_config.enable_cytokine_modulation;
    bridge->enable_microglia_pruning = local_config.enable_microglia_pruning;
    bridge->enable_complement_tagging = local_config.enable_complement_tagging;
    bridge->enable_inflammation_effects = local_config.enable_inflammation_effects;

    bridge->immune_system = immune_system;
    bridge->structural_system = structural_system;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "structural_immune") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "structural_immune_bridge_create: bridge_base_init failed");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        LOG_ERROR("Structural-immune bridge mutex creation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "structural_immune_bridge_create: mutex creation failed");
        return NULL;
    }

    /* Initialize microglia state */
    bridge->microglia_state.pruning_rate = MICROGLIA_BASELINE_PRUNING_RATE;

    NIMCP_LOGGING_INFO("Structural-immune bridge created");
    return bridge;
}

void structural_immune_bridge_destroy(structural_immune_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        structural_immune_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        nimcp_platform_mutex_destroy((nimcp_platform_mutex_t*)bridge->base.mutex);
        nimcp_free(bridge->base.mutex);
        bridge->base.mutex = NULL;
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Structural-immune bridge destroyed");
}

/* ============================================================================
 * Immune → Structural Implementation
 * ============================================================================ */

int structural_immune_apply_cytokine_effects(
    structural_immune_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_immune_apply_cytokine_effects: bridge is NULL");
        return -1;
    }

    if (!bridge->enable_cytokine_modulation) {
        return 0;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)bridge->base.mutex);

    /* Query cytokine levels from immune system */
    /* Note: This would query brain_immune_get_cytokine_level() for each type */
    /* For now, we'll use placeholder values */
    float il1_level = 0.0f;  /* Would query: brain_immune_get_cytokine_level(..., BRAIN_CYTOKINE_IL1) */
    float il6_level = 0.0f;
    float tnf_level = 0.0f;
    float il10_level = 0.0f;

    /* Compute formation impairments */
    bridge->cytokine_effects.il1_formation_impairment =
        il1_level * CYTOKINE_IL1_FORMATION_IMPACT;
    bridge->cytokine_effects.il6_formation_impairment =
        il6_level * CYTOKINE_IL6_FORMATION_IMPACT;
    bridge->cytokine_effects.tnf_formation_impairment =
        tnf_level * CYTOKINE_TNF_FORMATION_IMPACT;

    /* Compute anti-inflammatory boost */
    bridge->cytokine_effects.il10_formation_boost =
        il10_level * CYTOKINE_IL10_FORMATION_IMPACT;

    /* Compute pruning modulation */
    bridge->cytokine_effects.il1_pruning_boost =
        il1_level * CYTOKINE_IL1_PRUNING_BOOST;
    bridge->cytokine_effects.il6_pruning_boost =
        il6_level * CYTOKINE_IL6_PRUNING_BOOST;
    bridge->cytokine_effects.tnf_pruning_boost =
        tnf_level * CYTOKINE_TNF_PRUNING_BOOST;
    bridge->cytokine_effects.il10_pruning_reduction =
        il10_level * CYTOKINE_IL10_PRUNING_REDUCTION;

    /* Aggregate formation factor */
    float formation_impairment =
        bridge->cytokine_effects.il1_formation_impairment +
        bridge->cytokine_effects.il6_formation_impairment +
        bridge->cytokine_effects.tnf_formation_impairment;

    bridge->cytokine_effects.total_formation_factor =
        1.0f + formation_impairment + bridge->cytokine_effects.il10_formation_boost;

    if (bridge->cytokine_effects.total_formation_factor < 0.1f) {
        bridge->cytokine_effects.total_formation_factor = 0.1f;
    }

    /* Aggregate pruning factor */
    float pruning_boost =
        bridge->cytokine_effects.il1_pruning_boost +
        bridge->cytokine_effects.il6_pruning_boost +
        bridge->cytokine_effects.tnf_pruning_boost +
        bridge->cytokine_effects.il10_pruning_reduction;

    bridge->cytokine_effects.total_pruning_factor = 1.0f + pruning_boost;
    if (bridge->cytokine_effects.total_pruning_factor < 0.5f) {
        bridge->cytokine_effects.total_pruning_factor = 0.5f;
    }

    /* Stabilization impairment */
    bridge->cytokine_effects.stabilization_impairment =
        fabs(formation_impairment) * 0.5f;

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)bridge->base.mutex);
    return 0;
}

int structural_immune_apply_inflammation_effects(
    structural_immune_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_immune_apply_inflammation_effects: bridge is NULL");
        return -1;
    }

    if (!bridge->enable_inflammation_effects) {
        return 0;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)bridge->base.mutex);

    /* Query inflammation level */
    /* Would use: brain_immune_get_inflammation_level(bridge->immune_system) */
    brain_inflammation_level_t inflammation = INFLAMMATION_NONE;

    /* Map inflammation to formation factor */
    float formation_factor = 1.0f;
    switch (inflammation) {
        case INFLAMMATION_NONE:
            formation_factor = INFLAMMATION_NONE_FORMATION_FACTOR;
            break;
        case INFLAMMATION_LOCAL:
            formation_factor = INFLAMMATION_LOCAL_FORMATION_FACTOR;
            break;
        case INFLAMMATION_REGIONAL:
            formation_factor = INFLAMMATION_REGIONAL_FORMATION_FACTOR;
            break;
        case INFLAMMATION_SYSTEMIC:
            formation_factor = INFLAMMATION_SYSTEMIC_FORMATION_FACTOR;
            break;
        case INFLAMMATION_STORM:
            formation_factor = INFLAMMATION_STORM_FORMATION_FACTOR;
            break;
        default:
            formation_factor = 1.0f;
    }

    /* Apply to cytokine effects */
    bridge->cytokine_effects.total_formation_factor *= formation_factor;

    /* Inflammation increases microglia pruning */
    if (inflammation >= INFLAMMATION_REGIONAL) {
        bridge->microglia_state.pruning_rate =
            MICROGLIA_INFLAMMATION_PRUNING_RATE;
    } else if (inflammation == INFLAMMATION_LOCAL) {
        bridge->microglia_state.pruning_rate =
            MICROGLIA_ACTIVE_PRUNING_RATE;
    } else {
        bridge->microglia_state.pruning_rate =
            MICROGLIA_BASELINE_PRUNING_RATE;
    }

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)bridge->base.mutex);
    return 0;
}

int structural_immune_microglia_prune(structural_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_immune_microglia_prune: bridge is NULL");
        return -1;
    }

    if (!bridge->enable_microglia_pruning) {
        return 0;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)bridge->base.mutex);

    /* Get complement-tagged synapses */
    uint32_t tagged_ids[256];
    uint32_t tagged_count = 0;

    structural_plasticity_get_complement_tagged(
        bridge->structural_system, tagged_ids, 256, &tagged_count);

    bridge->microglia_state.tagged_spine_count = tagged_count;

    /* Prune tagged synapses based on pruning rate
     * BIOLOGICAL BASIS: Microglia actively prune complement-tagged synapses.
     * Tagged synapses should always be pruned regardless of baseline rate,
     * since tagging itself is the signal for elimination.
     */
    uint32_t pruned = 0;
    for (uint32_t i = 0; i < tagged_count; i++) {
        /* Prune all complement-tagged synapses
         * The act of complement tagging indicates the synapse should be eliminated.
         * Pruning rate modulates how quickly this happens, but tagged = prune.
         */
        if (structural_plasticity_eliminate_synapse(
                bridge->structural_system, tagged_ids[i]) == 0) {
            pruned++;
            bridge->microglia_state.total_engulfments++;
        }
    }

    bridge->microglia_state.spines_pruned_today += pruned;
    bridge->microglia_prunings += pruned;

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)bridge->base.mutex);

    if (pruned > 0) {
        NIMCP_LOGGING_DEBUG("Microglia pruned %u tagged synapses", pruned);
    }

    return 0;
}

uint32_t structural_immune_tag_weak_spines(
    structural_immune_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_immune_tag_weak_spines: bridge is NULL");
        return 0;
    }

    if (!bridge->enable_complement_tagging) {
        return 0;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)bridge->base.mutex);

    /* Get all spines and tag weak ones */
    uint32_t total_spines =
        structural_plasticity_get_total_spines(bridge->structural_system);

    uint32_t tagged = 0;
    for (uint32_t i = 1; i <= total_spines; i++) {
        synapse_structural_state_t state;
        if (structural_plasticity_get_synapse_state(
                bridge->structural_system, i, &state) == 0) {

            /* Tag if weak (low activity) and not already tagged */
            if (state.recent_activity_hz < 2.0f &&
                !state.complement_tagged &&
                state.state != SYNAPSE_STATE_ELIMINATED) {

                /* Create complement tag (C1q/C3) */
                uint8_t tag[STRUCTURAL_EPITOPE_SIZE];
                memset(tag, 0xC3, STRUCTURAL_EPITOPE_SIZE);  /* C3 marker */

                if (structural_plasticity_tag_complement(
                        bridge->structural_system, i, tag,
                        STRUCTURAL_EPITOPE_SIZE) == 0) {
                    tagged++;
                }
            }
        }
    }

    bridge->microglia_state.total_complement_tags += tagged;
    bridge->complement_tags_applied += tagged;

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)bridge->base.mutex);

    if (tagged > 0) {
        NIMCP_LOGGING_DEBUG("Tagged %u weak spines with complement", tagged);
    }

    return tagged;
}

/* ============================================================================
 * Structural → Immune Implementation
 * ============================================================================ */

int structural_immune_signal_density(structural_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_immune_signal_density: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)bridge->base.mutex);

    uint32_t total_spines =
        structural_plasticity_get_total_spines(bridge->structural_system);

    /* High density triggers increased pruning */
    if (total_spines > 500) {
        bridge->microglia_state.pruning_rate =
            MICROGLIA_ACTIVE_PRUNING_RATE;
        bridge->microglia_state.complement_tagging_rate = 0.05f;
    } else {
        bridge->microglia_state.pruning_rate =
            MICROGLIA_BASELINE_PRUNING_RATE;
        bridge->microglia_state.complement_tagging_rate = 0.01f;
    }

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)bridge->base.mutex);
    return 0;
}

int structural_immune_request_pruning(
    structural_immune_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_immune_request_pruning: bridge is NULL");
        return -1;
    }

    /* Tag synapse with complement */
    uint8_t tag[STRUCTURAL_EPITOPE_SIZE];
    memset(tag, 0xC1, STRUCTURAL_EPITOPE_SIZE);  /* C1q marker */

    int result = structural_plasticity_tag_complement(
        bridge->structural_system, synapse_id, tag, STRUCTURAL_EPITOPE_SIZE);

    if (result == 0) {
        NIMCP_LOGGING_DEBUG("Requested immune pruning for synapse %u",
                           synapse_id);
    }

    return result;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int structural_immune_bridge_update(
    structural_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_immune_bridge_update: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)bridge->base.mutex);

    /* Apply immune → structural effects */
    structural_immune_apply_cytokine_effects(bridge);
    structural_immune_apply_inflammation_effects(bridge);

    /* Tag weak spines periodically */
    if ((bridge->total_updates % 100) == 0) {
        structural_immune_tag_weak_spines(bridge);
    }

    /* Perform microglia pruning */
    structural_immune_microglia_prune(bridge);

    /* Signal structural → immune */
    structural_immune_signal_density(bridge);

    bridge->total_updates++;
    bridge->last_update_time += delta_ms;

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int structural_immune_get_cytokine_effects(
    const structural_immune_bridge_t* bridge,
    cytokine_structural_effects_t* effects
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_immune_get_cytokine_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_immune_get_cytokine_effects: effects is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)bridge->base.mutex);
    *effects = bridge->cytokine_effects;
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)bridge->base.mutex);

    return 0;
}

int structural_immune_get_microglia_state(
    const structural_immune_bridge_t* bridge,
    microglia_pruning_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_immune_get_microglia_state: bridge is NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_immune_get_microglia_state: state is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)bridge->base.mutex);
    *state = bridge->microglia_state;
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)bridge->base.mutex);

    return 0;
}

float structural_immune_get_formation_factor(
    const structural_immune_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_immune_get_formation_factor: bridge is NULL");
        return 1.0f;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)bridge->base.mutex);
    float factor = bridge->cytokine_effects.total_formation_factor;
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)bridge->base.mutex);

    return factor;
}

float structural_immune_get_pruning_factor(
    const structural_immune_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_immune_get_pruning_factor: bridge is NULL");
        return 1.0f;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)bridge->base.mutex);
    float factor = bridge->cytokine_effects.total_pruning_factor;
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)bridge->base.mutex);

    return factor;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

int structural_immune_connect_bio_async(structural_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_immune_connect_bio_async: bridge is NULL");
        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = 0x0D31,  /* BIO_MODULE_IMMUNE_STRUCTURAL (add to bio_messages.h) */
        .module_name = "structural_immune_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    return 0;  /* Non-fatal */
}

int structural_immune_disconnect_bio_async(
    structural_immune_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_immune_disconnect_bio_async: bridge is NULL");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) {
        return 0;  /* Already disconnected */
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

bool structural_immune_is_bio_async_connected(
    const structural_immune_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
