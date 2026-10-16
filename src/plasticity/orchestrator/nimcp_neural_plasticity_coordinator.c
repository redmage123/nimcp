/**
 * @file nimcp_neural_plasticity_coordinator.c
 * @brief Neural Plasticity Coordinator - Unified Neural-Plasticity Integration
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Unified coordinator for neural computation and plasticity
 * WHY:  Single point of control for neurons, axons, dendrites, and plasticity
 * HOW:  Wraps all bridges with consistent simulation interface
 *
 * @author NIMCP Development Team
 */

#include "plasticity/orchestrator/nimcp_neural_plasticity_coordinator.h"
#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neural_plasticity_coordinator)

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/** Microseconds to milliseconds */
#define US_TO_MS 1000.0f

/** Performance timer (placeholder) */
#define GET_TIME_US() 0ULL

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

/**
 * @brief Internal coordinator structure
 */
struct neural_plasticity_coordinator {
    /* Configuration */
    neural_plasticity_config_t config;

    /* Core systems */
    plasticity_orchestrator_t* orchestrator;
    axon_network_t* axon_network;
    dendrite_network_t* dendrite_network;

    /* Bridges */
    axon_orchestrator_bridge_t* axon_bridge;
    neuron_orchestrator_bridge_t* neuron_bridge;
    dendrite_orchestrator_bridge_t* dendrite_bridge;

    /* Integration */
    struct brain_immune_system* immune_system;
    struct unified_mem_manager* umm;

    /* Bio-async */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    /* Timing */
    float time_since_orch_update_ms;
    float time_since_sync_ms;
    uint64_t last_step_time_us;

    /* Statistics */
    neural_plasticity_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t mutex;
    bool mutex_initialized;

    /* Ownership tracking */
    bool owns_orchestrator;

    /* Phase 5: Plasticity bridge for backprop gating */
    tpb_context_t* plasticity_bridge;
};

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int neural_plasticity_default_config(neural_plasticity_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    config->axon_config = NULL;
    config->neuron_config = NULL;
    config->dendrite_config = NULL;
    config->orchestrator_config = NULL;
    config->default_dt_ms = NEURAL_COORD_DEFAULT_DT_MS;
    config->orchestrator_interval_ms = NEURAL_COORD_DEFAULT_ORCH_INTERVAL_MS;
    config->sync_interval_ms = NEURAL_COORD_DEFAULT_SYNC_INTERVAL_MS;
    config->enable_bio_async = true;
    config->enable_immune_integration = true;
    config->enable_umm = true;

    return 0;
}

neural_plasticity_coordinator_t* neural_plasticity_coordinator_create(
    const neural_plasticity_config_t* config,
    axon_network_t* axon_network,
    dendrite_network_t* dendrite_network
) {
    /* Allocate coordinator */
    neural_plasticity_coordinator_t* coord = (neural_plasticity_coordinator_t*)nimcp_calloc(
        1, sizeof(neural_plasticity_coordinator_t)
    );
    if (!coord) {
        NIMCP_LOGGING_ERROR("neural_plasticity_coordinator_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neural_plasticity_coordinator_create: coord is NULL");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        coord->config = *config;
    } else {
        neural_plasticity_default_config(&coord->config);
    }

    /* Store network handles */
    coord->axon_network = axon_network;
    coord->dendrite_network = dendrite_network;

    /* Create plasticity orchestrator */
    coord->orchestrator = plasticity_orchestrator_create(coord->config.orchestrator_config);
    if (!coord->orchestrator) {
        NIMCP_LOGGING_ERROR("neural_plasticity_coordinator_create: orchestrator creation failed");
        nimcp_free(coord);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neural_plasticity_coordinator_create: coord->orchestrator is NULL");
        return NULL;
    }
    coord->owns_orchestrator = true;

    /* Create axon bridge (if axon network provided) */
    if (axon_network) {
        coord->axon_bridge = axon_orchestrator_bridge_create(
            coord->config.axon_config,
            coord->orchestrator,
            axon_network
        );
        if (!coord->axon_bridge) {
            NIMCP_LOGGING_WARN("neural_plasticity_coordinator_create: axon bridge creation failed");
        }
    }

    /* Create neuron bridge */
    coord->neuron_bridge = neuron_orchestrator_bridge_create(
        coord->config.neuron_config,
        coord->orchestrator,
        axon_network,
        dendrite_network
    );
    if (!coord->neuron_bridge) {
        NIMCP_LOGGING_ERROR("neural_plasticity_coordinator_create: neuron bridge creation failed");
        if (coord->axon_bridge) {
            axon_orchestrator_bridge_destroy(coord->axon_bridge);
        }
        plasticity_orchestrator_destroy(coord->orchestrator);
        nimcp_free(coord);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_plasticity_coordinator_create: validation failed");
        return NULL;
    }

    /* Create dendrite bridge (if dendrite network provided) */
    if (dendrite_network) {
        coord->dendrite_bridge = dendrite_orchestrator_bridge_create(
            coord->config.dendrite_config,
            coord->orchestrator,
            dendrite_network
        );
        if (!coord->dendrite_bridge) {
            NIMCP_LOGGING_WARN("neural_plasticity_coordinator_create: dendrite bridge creation failed");
        }
    }

    /* Create mutex */
    if (nimcp_mutex_init(&coord->mutex, NULL) == 0) {
        coord->mutex_initialized = true;
    } else {
        NIMCP_LOGGING_WARN("neural_plasticity_coordinator_create: mutex creation failed");
        coord->mutex_initialized = false;
    }

    /* Initialize timing */
    coord->time_since_orch_update_ms = 0.0f;
    coord->time_since_sync_ms = 0.0f;
    coord->last_step_time_us = 0;

    /* Initialize statistics */
    memset(&coord->stats, 0, sizeof(neural_plasticity_stats_t));

    NIMCP_LOGGING_INFO("neural_plasticity_coordinator: created successfully");

    return coord;
}

void neural_plasticity_coordinator_destroy(neural_plasticity_coordinator_t* coordinator) {
    if (!coordinator) {
        return;
    }

    /* Disconnect bio-async */
    if (coordinator->bio_async_enabled && coordinator->bio_ctx) {
        bio_router_unregister_module(coordinator->bio_ctx);
    }

    /* Destroy bridges */
    if (coordinator->dendrite_bridge) {
        dendrite_orchestrator_bridge_destroy(coordinator->dendrite_bridge);
    }
    if (coordinator->neuron_bridge) {
        neuron_orchestrator_bridge_destroy(coordinator->neuron_bridge);
    }
    if (coordinator->axon_bridge) {
        axon_orchestrator_bridge_destroy(coordinator->axon_bridge);
    }

    /* Destroy orchestrator (if we own it) */
    if (coordinator->owns_orchestrator && coordinator->orchestrator) {
        plasticity_orchestrator_destroy(coordinator->orchestrator);
    }

    /* Free mutex */
    if (coordinator->mutex_initialized) {
        nimcp_mutex_destroy(&coordinator->mutex);
    }

    /* Free coordinator */
    nimcp_free(coordinator);

    NIMCP_LOGGING_INFO("neural_plasticity_coordinator: destroyed");
}

/* ============================================================================
 * Integration Implementation
 * ============================================================================ */

int neural_plasticity_connect_immune(
    neural_plasticity_coordinator_t* coordinator,
    struct brain_immune_system* immune_system
) {
    if (!coordinator || !immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_plasticity_connect_immune: required parameter is NULL (coordinator, immune_system)");
        return -1;
    }

    coordinator->immune_system = immune_system;

    /* Connect orchestrator to immune */
    if (coordinator->orchestrator) {
        plasticity_orchestrator_connect_immune(coordinator->orchestrator, immune_system);
    }

    NIMCP_LOGGING_INFO("neural_plasticity_coordinator: connected to immune system");

    return 0;
}

int neural_plasticity_connect_umm(
    neural_plasticity_coordinator_t* coordinator,
    struct unified_mem_manager* umm
) {
    if (!coordinator || !umm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_plasticity_connect_umm: required parameter is NULL (coordinator, umm)");
        return -1;
    }

    coordinator->umm = umm;

    NIMCP_LOGGING_INFO("neural_plasticity_coordinator: connected to UMM");

    return 0;
}

int neural_plasticity_connect_bio_async(neural_plasticity_coordinator_t* coordinator) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "coordinator is NULL");

        return -1;
    }

    if (!bio_router_is_initialized()) {
        NIMCP_LOGGING_WARN("neural_plasticity_coordinator: bio-async not available");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "neural_plasticity_connect_bio_async: bio_router_is_initialized is NULL");
        return -1;
    }

    /* Register coordinator itself */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_NEURAL_PLASTICITY_COORDINATOR,
        .module_name = "neural_plasticity_coordinator",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_LARGE,
        .user_data = coordinator
    };

    coordinator->bio_ctx = bio_router_register_module(&info);
    if (!coordinator->bio_ctx) {
        NIMCP_LOGGING_ERROR("neural_plasticity_coordinator: failed to register with bio-async");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_plasticity_connect_bio_async: coordinator->bio_ctx is NULL");
        return -1;
    }

    coordinator->bio_async_enabled = true;

    /* Connect all bridges */
    if (coordinator->axon_bridge) {
        axon_orchestrator_connect_bio_async(coordinator->axon_bridge);
    }
    if (coordinator->neuron_bridge) {
        neuron_orchestrator_connect_bio_async(coordinator->neuron_bridge);
    }
    if (coordinator->dendrite_bridge) {
        dendrite_orchestrator_connect_bio_async(coordinator->dendrite_bridge);
    }

    /* Connect orchestrator */
    if (coordinator->orchestrator) {
        plasticity_orchestrator_connect_bio_async(coordinator->orchestrator);
    }

    NIMCP_LOGGING_INFO("neural_plasticity_coordinator: connected all components to bio-async");

    return 0;
}

/* ============================================================================
 * Registration Implementation
 * ============================================================================ */

int neural_plasticity_register_neuron(
    neural_plasticity_coordinator_t* coordinator,
    uint32_t neuron_id,
    neuron_model_state_t model_state,
    const neuron_model_vtable_t* vtable
) {
    if (!coordinator || !coordinator->neuron_bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_plasticity_register_neuron: required parameter is NULL (coordinator, coordinator->neuron_bridge)");
        return -1;
    }

    return neuron_orchestrator_register_neuron(
        coordinator->neuron_bridge,
        neuron_id,
        model_state,
        vtable
    );
}

int neural_plasticity_add_neuron_axon(
    neural_plasticity_coordinator_t* coordinator,
    uint32_t neuron_id,
    uint32_t axon_id
) {
    if (!coordinator || !coordinator->neuron_bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_plasticity_add_neuron_axon: required parameter is NULL (coordinator, coordinator->neuron_bridge)");
        return -1;
    }

    return neuron_orchestrator_add_axon(
        coordinator->neuron_bridge,
        neuron_id,
        axon_id
    );
}

int neural_plasticity_add_neuron_dendrite(
    neural_plasticity_coordinator_t* coordinator,
    uint32_t neuron_id,
    uint32_t dendrite_id
) {
    if (!coordinator || !coordinator->neuron_bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_plasticity_add_neuron_dendrite: required parameter is NULL (coordinator, coordinator->neuron_bridge)");
        return -1;
    }

    return neuron_orchestrator_add_dendrite(
        coordinator->neuron_bridge,
        neuron_id,
        dendrite_id
    );
}

int neural_plasticity_register_synapse(
    neural_plasticity_coordinator_t* coordinator,
    uint32_t synapse_id,
    uint32_t axon_id,
    uint32_t dendrite_id,
    uint32_t spine_index,
    float initial_weight
) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "coordinator is NULL");

        return -1;
    }

    /* Map synapse to axon */
    if (coordinator->axon_bridge) {
        int result = axon_orchestrator_map_synapse(
            coordinator->axon_bridge,
            synapse_id,
            axon_id
        );
        if (result != 0) {
            NIMCP_LOGGING_WARN("neural_plasticity: failed to map synapse %u to axon %u",
                              synapse_id, axon_id);
        }
    }

    /* Map synapse to spine */
    if (coordinator->dendrite_bridge) {
        int result = dendrite_orchestrator_spine_formed(
            coordinator->dendrite_bridge,
            dendrite_id,
            spine_index,
            synapse_id,
            initial_weight
        );
        if (result != 0) {
            NIMCP_LOGGING_WARN("neural_plasticity: failed to map synapse %u to spine",
                              synapse_id);
        }
    }

    /* Initialize weight in orchestrator */
    if (coordinator->orchestrator) {
        plasticity_orchestrator_set_weight(
            coordinator->orchestrator,
            synapse_id,
            initial_weight
        );
    }

    return 0;
}

int neural_plasticity_unregister_synapse(
    neural_plasticity_coordinator_t* coordinator,
    uint32_t synapse_id
) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "coordinator is NULL");

        return -1;
    }

    /* Unmap from axon bridge */
    if (coordinator->axon_bridge) {
        axon_orchestrator_unmap_synapse(coordinator->axon_bridge, synapse_id);
    }

    /* Unmap from dendrite bridge */
    if (coordinator->dendrite_bridge) {
        dendrite_orchestrator_spine_eliminated(coordinator->dendrite_bridge, synapse_id);
    }

    return 0;
}

/* ============================================================================
 * Simulation Implementation
 * ============================================================================ */

int neural_plasticity_step(
    neural_plasticity_coordinator_t* coordinator,
    float dt_ms,
    const float* neuron_inputs,
    uint64_t current_time_us
) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "coordinator is NULL");

        return -1;
    }

    /* Phase 5: Skip plasticity during backprop to prevent interference */
    if (coordinator->plasticity_bridge &&
        nimcp_tpb_is_backprop_active(coordinator->plasticity_bridge)) {
        return 0;
    }

    /* Use default dt if not specified */
    if (dt_ms <= 0.0f) {
        dt_ms = coordinator->config.default_dt_ms;
    }

    uint64_t step_start = GET_TIME_US();

    /* Step 1: Step all neurons (detect spikes, trigger cascades) */
    int spikes = 0;
    if (coordinator->neuron_bridge) {
        spikes = neuron_orchestrator_step_all(
            coordinator->neuron_bridge,
            dt_ms,
            neuron_inputs,
            current_time_us
        );
        if (spikes > 0) {
            coordinator->stats.total_spikes += spikes;
        }
    }

    /* Step 2: Process axon arrivals */
    if (coordinator->axon_bridge) {
        int arrivals = axon_orchestrator_bridge_update(
            coordinator->axon_bridge,
            current_time_us
        );
        if (arrivals > 0) {
            coordinator->stats.axon_spikes_arrived += arrivals;
        }
    }

    /* Step 3: Update dendrite bridge */
    if (coordinator->dendrite_bridge) {
        dendrite_orchestrator_bridge_update(
            coordinator->dendrite_bridge,
            current_time_us
        );
    }

    /* Step 4: Run orchestrator update (if interval reached) */
    coordinator->time_since_orch_update_ms += dt_ms;
    if (coordinator->time_since_orch_update_ms >= coordinator->config.orchestrator_interval_ms) {
        if (coordinator->orchestrator) {
            plasticity_orchestrator_update(
                coordinator->orchestrator,
                (uint64_t)coordinator->time_since_orch_update_ms
            );
            coordinator->stats.orchestrator_updates++;
        }
        coordinator->time_since_orch_update_ms = 0.0f;
    }

    /* Step 5: Sync spine↔weight (if interval reached) */
    coordinator->time_since_sync_ms += dt_ms;
    if (coordinator->time_since_sync_ms >= coordinator->config.sync_interval_ms) {
        if (coordinator->dendrite_bridge) {
            dendrite_orchestrator_sync_all(
                coordinator->dendrite_bridge,
                SYNC_ORCHESTRATOR_TO_SPINE
            );
        }
        coordinator->time_since_sync_ms = 0.0f;
    }

    /* Update timing statistics */
    uint64_t step_end = GET_TIME_US();
    float step_time_us = (float)(step_end - step_start);

    if (coordinator->mutex_initialized) {
        nimcp_mutex_lock(&coordinator->mutex);
    }

    coordinator->stats.total_steps++;
    coordinator->stats.last_step_time_us = step_time_us;

    /* Update mean (exponential moving average) */
    float alpha = 0.01f;
    coordinator->stats.mean_step_time_us =
        (1.0f - alpha) * coordinator->stats.mean_step_time_us + alpha * step_time_us;

    if (coordinator->mutex_initialized) {
        nimcp_mutex_unlock(&coordinator->mutex);
    }

    coordinator->last_step_time_us = current_time_us;

    return spikes;
}

int neural_plasticity_reward(
    neural_plasticity_coordinator_t* coordinator,
    float reward,
    uint64_t timestamp_us
) {
    if (!coordinator || !coordinator->orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_plasticity_reward: required parameter is NULL (coordinator, coordinator->orchestrator)");
        return -1;
    }

    return plasticity_orchestrator_reward(
        coordinator->orchestrator,
        reward,
        timestamp_us / US_TO_MS
    );
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

float neural_plasticity_get_weight(
    const neural_plasticity_coordinator_t* coordinator,
    uint32_t synapse_id
) {
    if (!coordinator || !coordinator->orchestrator) {
        return NAN;
    }

    return plasticity_orchestrator_get_weight(coordinator->orchestrator, synapse_id);
}

int neural_plasticity_set_weight(
    neural_plasticity_coordinator_t* coordinator,
    uint32_t synapse_id,
    float weight
) {
    if (!coordinator || !coordinator->orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_plasticity_set_weight: required parameter is NULL (coordinator, coordinator->orchestrator)");
        return -1;
    }

    int result = plasticity_orchestrator_set_weight(
        coordinator->orchestrator,
        synapse_id,
        weight
    );

    /* Also sync to spine */
    if (result == 0 && coordinator->dendrite_bridge) {
        dendrite_orchestrator_sync_weight_to_spine(
            coordinator->dendrite_bridge,
            synapse_id
        );
    }

    return result;
}

float neural_plasticity_get_firing_rate(
    const neural_plasticity_coordinator_t* coordinator,
    uint32_t neuron_id
) {
    if (!coordinator || !coordinator->neuron_bridge) {
        return -1.0f;
    }

    return neuron_orchestrator_get_firing_rate(coordinator->neuron_bridge, neuron_id);
}

int neural_plasticity_get_stats(
    const neural_plasticity_coordinator_t* coordinator,
    neural_plasticity_stats_t* stats
) {
    if (!coordinator || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_plasticity_get_stats: required parameter is NULL (coordinator, stats)");
        return -1;
    }

    /* Start with our stats */
    *stats = coordinator->stats;

    /* Aggregate from bridges */
    if (coordinator->neuron_bridge) {
        neuron_orchestrator_stats_t ns;
        if (neuron_orchestrator_get_stats(coordinator->neuron_bridge, &ns) == 0) {
            stats->neurons_registered = ns.neurons_registered;
            stats->total_neuron_steps = ns.step_calls;
            stats->axon_spikes_initiated = ns.axon_spikes_initiated;
        }
    }

    if (coordinator->axon_bridge) {
        axon_orchestrator_stats_t as;
        if (axon_orchestrator_get_stats(coordinator->axon_bridge, &as) == 0) {
            stats->axon_mappings = (uint32_t)axon_orchestrator_get_mapping_count(
                coordinator->axon_bridge
            );
        }
    }

    if (coordinator->dendrite_bridge) {
        dendrite_orchestrator_stats_t ds;
        if (dendrite_orchestrator_get_stats(coordinator->dendrite_bridge, &ds) == 0) {
            stats->pre_spikes_forwarded = ds.pre_spikes_forwarded;
            stats->spines_registered = ds.spines_registered;
            stats->spines_eliminated = ds.spines_eliminated;
            stats->dendrite_mappings = (uint32_t)dendrite_orchestrator_get_mapping_count(
                coordinator->dendrite_bridge
            );
        }
    }

    if (coordinator->orchestrator) {
        plasticity_stats_t ps;
        if (plasticity_orchestrator_get_stats(coordinator->orchestrator, &ps) == 0) {
            stats->ltp_events = ps.ltp_count;
            stats->ltd_events = ps.ltd_count;
        }
    }

    return 0;
}

int neural_plasticity_reset_stats(neural_plasticity_coordinator_t* coordinator) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "coordinator is NULL");

        return -1;
    }

    if (coordinator->mutex_initialized) {
        nimcp_mutex_lock(&coordinator->mutex);
    }

    memset(&coordinator->stats, 0, sizeof(neural_plasticity_stats_t));

    if (coordinator->mutex_initialized) {
        nimcp_mutex_unlock(&coordinator->mutex);
    }

    /* Reset sub-component stats */
    if (coordinator->axon_bridge) {
        axon_orchestrator_reset_stats(coordinator->axon_bridge);
    }
    if (coordinator->neuron_bridge) {
        neuron_orchestrator_reset_stats(coordinator->neuron_bridge);
    }
    if (coordinator->dendrite_bridge) {
        dendrite_orchestrator_reset_stats(coordinator->dendrite_bridge);
    }
    if (coordinator->orchestrator) {
        plasticity_orchestrator_reset_stats(coordinator->orchestrator);
    }

    return 0;
}

/* ============================================================================
 * Accessor Implementation
 * ============================================================================ */

plasticity_orchestrator_t* neural_plasticity_get_orchestrator(
    neural_plasticity_coordinator_t* coordinator
) {
    return coordinator ? coordinator->orchestrator : NULL;
}

axon_orchestrator_bridge_t* neural_plasticity_get_axon_bridge(
    neural_plasticity_coordinator_t* coordinator
) {
    return coordinator ? coordinator->axon_bridge : NULL;
}

neuron_orchestrator_bridge_t* neural_plasticity_get_neuron_bridge(
    neural_plasticity_coordinator_t* coordinator
) {
    return coordinator ? coordinator->neuron_bridge : NULL;
}

dendrite_orchestrator_bridge_t* neural_plasticity_get_dendrite_bridge(
    neural_plasticity_coordinator_t* coordinator
) {
    return coordinator ? coordinator->dendrite_bridge : NULL;
}

axon_network_t* neural_plasticity_get_axon_network(
    neural_plasticity_coordinator_t* coordinator
) {
    return coordinator ? coordinator->axon_network : NULL;
}

dendrite_network_t* neural_plasticity_get_dendrite_network(
    neural_plasticity_coordinator_t* coordinator
) {
    return coordinator ? coordinator->dendrite_network : NULL;
}

void neural_plasticity_set_plasticity_bridge(
    neural_plasticity_coordinator_t* coordinator,
    tpb_context_t* tpb
) {
    if (!coordinator) return;
    coordinator->plasticity_bridge = tpb;
}
