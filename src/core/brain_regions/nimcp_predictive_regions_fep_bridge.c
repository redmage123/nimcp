/**
 * @file nimcp_predictive_regions_fep_bridge.c
 * @brief Free Energy Principle - Predictive Regions Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Implementation of bidirectional FEP-predictive regions integration
 * WHY:  Enable hierarchical belief propagation between FEP and cortical regions
 * HOW:  Synchronize beliefs, propagate errors, adapt precision across hierarchy
 */

#include "core/brain_regions/nimcp_predictive_regions_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(predictive_regions_fep_bridge)

static inline float safe_divide(float num, float denom, float default_val) {
    return (fabsf(denom) > 1e-10f) ? (num / denom) : default_val;
}

/* Free level mapping */
static void free_level_mapping(predictive_fep_level_mapping_t* mapping) {
    if (!mapping) return;
    nimcp_free(mapping->belief_buffer);
    nimcp_free(mapping->prediction_buffer);
    nimcp_free(mapping->error_buffer);
    memset(mapping, 0, sizeof(predictive_fep_level_mapping_t));
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int predictive_regions_fep_bridge_default_config(
    predictive_regions_fep_config_t* config
) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->num_hierarchy_levels = 3;
    config->enable_precision_adaptation = true;
    config->enable_belief_sync = true;
    config->enable_error_propagation = true;

    config->precision_learning_rate = PREDICTIVE_FEP_PRECISION_ADAPTATION_RATE;
    config->belief_learning_rate = 0.1f;
    config->prediction_learning_rate = 0.05f;

    config->convergence_threshold = PREDICTIVE_FEP_CONVERGENCE_THRESHOLD;
    config->error_threshold = PREDICTIVE_FEP_ERROR_THRESHOLD;

    config->enable_active_inference = false;
    config->exploration_rate = 0.1f;

    return 0;
}

predictive_regions_fep_bridge_t* predictive_regions_fep_bridge_create(
    const predictive_regions_fep_config_t* config
) {
    predictive_regions_fep_bridge_t* bridge = (predictive_regions_fep_bridge_t*)
        nimcp_calloc(1, sizeof(predictive_regions_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate predictive regions-FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    /* Apply configuration */
    predictive_regions_fep_config_t default_cfg;
    if (!config) {
        predictive_regions_fep_bridge_default_config(&default_cfg);
        config = &default_cfg;
    }
    bridge->config = *config;

    /* Allocate level mappings */
    bridge->level_mappings = (predictive_fep_level_mapping_t*)nimcp_calloc(
        PREDICTIVE_FEP_MAX_REGIONS, sizeof(predictive_fep_level_mapping_t));
    if (!bridge->level_mappings) {
        predictive_regions_fep_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "predictive_regions_fep_bridge_create: bridge->level_mappings is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->state.converged = false;
    bridge->state.total_free_energy = 0.0f;
    bridge->state.mean_prediction_error = 0.0f;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "predictive_regions_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        predictive_regions_fep_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "predictive_regions_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Predictive regions-FEP bridge created");
    return bridge;
}

void predictive_regions_fep_bridge_destroy(
    predictive_regions_fep_bridge_t* bridge
) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        predictive_regions_fep_bridge_disconnect_bio_async(bridge);
    }

    if (bridge->level_mappings) {
        for (uint32_t i = 0; i < bridge->num_mappings; i++) {
            free_level_mapping(&bridge->level_mappings[i]);
        }
        nimcp_free(bridge->level_mappings);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Predictive regions-FEP bridge destroyed");
}

/* ============================================================================
 * Connection Implementation
 * ============================================================================ */

int predictive_regions_fep_bridge_connect_fep(
    predictive_regions_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    if (!bridge || !fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_regions_fep_bridge_connect_fep: required parameter is NULL (bridge, fep)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Predictive regions-FEP bridge connected to FEP system");
    return 0;
}

int predictive_regions_fep_bridge_map_region(
    predictive_regions_fep_bridge_t* bridge,
    brain_region_t* region,
    uint32_t fep_level
) {
    if (!bridge || !region) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_regions_fep_bridge_map_region: required parameter is NULL (bridge, region)");
        return -1;
    }
    if (bridge->num_mappings >= PREDICTIVE_FEP_MAX_REGIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "predictive_regions_fep_bridge_map_region: capacity exceeded");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get region neuron count (placeholder - actual API may differ) */
    uint32_t num_neurons = 100; /* TODO: Get from region */

    predictive_fep_level_mapping_t* mapping =
        &bridge->level_mappings[bridge->num_mappings];

    mapping->fep_level = fep_level;
    mapping->region = region;
    mapping->region_id = bridge->num_mappings;
    mapping->buffer_size = num_neurons;

    /* Allocate buffers */
    mapping->belief_buffer = (float*)nimcp_calloc(num_neurons, sizeof(float));
    mapping->prediction_buffer = (float*)nimcp_calloc(num_neurons, sizeof(float));
    mapping->error_buffer = (float*)nimcp_calloc(num_neurons, sizeof(float));

    if (!mapping->belief_buffer || !mapping->prediction_buffer ||
        !mapping->error_buffer) {
        free_level_mapping(mapping);
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "predictive_regions_fep_bridge_map_region: operation failed");
        return -1;
    }

    bridge->num_mappings++;
    bridge->state.num_mapped_levels = bridge->num_mappings;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int predictive_regions_fep_bridge_disconnect(
    predictive_regions_fep_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;

    for (uint32_t i = 0; i < bridge->num_mappings; i++) {
        free_level_mapping(&bridge->level_mappings[i]);
    }
    bridge->num_mappings = 0;
    bridge->state.num_mapped_levels = 0;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * FEP → Predictive Regions Implementation
 * ============================================================================ */

int predictive_regions_fep_sync_beliefs_to_regions(
    predictive_regions_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_regions_fep_sync_beliefs_to_regions: required parameter is NULL (bridge, bridge->fep_system)");
        return -1;
    }
    if (!bridge->config.enable_belief_sync) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    fep_system_t* fep = bridge->fep_system;

    for (uint32_t i = 0; i < bridge->num_mappings; i++) {
        predictive_fep_level_mapping_t* mapping = &bridge->level_mappings[i];

        if (mapping->fep_level >= fep->num_levels) continue;

        fep_hierarchy_level_t* level = &fep->levels[mapping->fep_level];

        /* Copy FEP beliefs to region buffer */
        uint32_t copy_size = (level->beliefs.dim < mapping->buffer_size) ?
            level->beliefs.dim : mapping->buffer_size;

        memcpy(mapping->belief_buffer, level->beliefs.mean,
               copy_size * sizeof(float));
    }

    bridge->stats.belief_syncs++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int predictive_regions_fep_apply_precision_modulation(
    predictive_regions_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_regions_fep_apply_precision_modulation: required parameter is NULL (bridge, bridge->fep_system)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    fep_system_t* fep = bridge->fep_system;

    for (uint32_t i = 0; i < bridge->num_mappings; i++) {
        predictive_fep_level_mapping_t* mapping = &bridge->level_mappings[i];

        if (mapping->fep_level >= fep->num_levels) continue;

        fep_hierarchy_level_t* level = &fep->levels[mapping->fep_level];

        /* Compute mean precision */
        float mean_precision = 0.0f;
        for (uint32_t j = 0; j < level->beliefs.dim; j++) {
            mean_precision += level->beliefs.precision[j];
        }
        mean_precision = safe_divide(mean_precision,
                                     (float)level->beliefs.dim, 1.0f);

        /* Apply to bridge effects */
        bridge->effects.precision_modulation = mean_precision;
        bridge->effects.attention_weight = mean_precision;
    }

    bridge->stats.precision_updates++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int predictive_regions_fep_generate_predictions(
    predictive_regions_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_regions_fep_generate_predictions: required parameter is NULL (bridge, bridge->fep_system)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    fep_system_t* fep = bridge->fep_system;

    for (uint32_t i = 0; i < bridge->num_mappings; i++) {
        predictive_fep_level_mapping_t* mapping = &bridge->level_mappings[i];

        if (mapping->fep_level >= fep->num_levels) continue;

        fep_hierarchy_level_t* level = &fep->levels[mapping->fep_level];

        /* Copy FEP predictions to region buffer */
        if (level->predictions) {
            uint32_t copy_size = (level->prediction_dim < mapping->buffer_size) ?
                level->prediction_dim : mapping->buffer_size;

            memcpy(mapping->prediction_buffer, level->predictions,
                   copy_size * sizeof(float));
        }
    }

    bridge->effects.prediction_strength = 1.0f;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Predictive Regions → FEP Implementation
 * ============================================================================ */

int predictive_regions_fep_propagate_errors_to_fep(
    predictive_regions_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_regions_fep_propagate_errors_to_fep: required parameter is NULL (bridge, bridge->fep_system)");
        return -1;
    }
    if (!bridge->config.enable_error_propagation) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Copy region errors to FEP error signals */
    /* Note: Actual implementation would extract errors from regions */
    /* This is a simplified placeholder */

    bridge->stats.error_propagations++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int predictive_regions_fep_compute_free_energy(
    predictive_regions_fep_bridge_t* bridge,
    float* free_energy
) {
    if (!bridge || !free_energy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_regions_fep_compute_free_energy: required parameter is NULL (bridge, free_energy)");
        return -1;
    }
    if (!bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_regions_fep_compute_free_energy: bridge->fep_system is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    float total_fe = bridge->fep_system->free_energy.total;
    *free_energy = total_fe;
    bridge->state.total_free_energy = total_fe;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int predictive_regions_fep_adapt_precision(
    predictive_regions_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_regions_fep_adapt_precision: required parameter is NULL (bridge, bridge->fep_system)");
        return -1;
    }
    if (!bridge->config.enable_precision_adaptation) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    fep_system_t* fep = bridge->fep_system;
    float lr = bridge->config.precision_learning_rate;

    for (uint32_t i = 0; i < bridge->num_mappings; i++) {
        predictive_fep_level_mapping_t* mapping = &bridge->level_mappings[i];

        if (mapping->fep_level >= fep->num_levels) continue;

        fep_hierarchy_level_t* level = &fep->levels[mapping->fep_level];

        /* Adapt precision based on error variance */
        /* Precision ∝ 1/error_variance */
        for (uint32_t j = 0; j < level->beliefs.dim; j++) {
            float error = level->errors.error[j];
            float error_sq = error * error;

            /* Running average precision adaptation */
            float target_precision = safe_divide(1.0f, error_sq + 1e-6f, 1.0f);
            target_precision = nimcp_clampf(target_precision,
                                      PREDICTIVE_FEP_MIN_PRECISION,
                                      PREDICTIVE_FEP_MAX_PRECISION);

            level->beliefs.precision[j] += lr * (target_precision -
                                                level->beliefs.precision[j]);
        }
    }

    bridge->stats.precision_updates++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Active Inference Implementation
 * ============================================================================ */

int predictive_regions_fep_active_inference_select(
    predictive_regions_fep_bridge_t* bridge,
    uint32_t* selected_region
) {
    if (!bridge || !selected_region) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_regions_fep_active_inference_select: required parameter is NULL (bridge, selected_region)");
        return -1;
    }
    if (!bridge->config.enable_active_inference) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_regions_fep_active_inference_select: bridge->config is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute EFE for each region */
    float min_efe = INFINITY;
    uint32_t best_region = 0;

    for (uint32_t i = 0; i < bridge->num_mappings; i++) {
        float efe;
        if (predictive_regions_fep_compute_efe(bridge, i, &efe) == 0) {
            if (efe < min_efe) {
                min_efe = efe;
                best_region = i;
            }
        }
    }

    *selected_region = best_region;
    bridge->state.active_region_index = best_region;
    bridge->effects.selected_region = best_region;
    bridge->stats.active_inference_actions++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int predictive_regions_fep_compute_efe(
    predictive_regions_fep_bridge_t* bridge,
    uint32_t region_index,
    float* efe
) {
    if (!bridge || !efe) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_regions_fep_compute_efe: required parameter is NULL (bridge, efe)");
        return -1;
    }
    if (region_index >= bridge->num_mappings) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "predictive_regions_fep_compute_efe: capacity exceeded");
        return -1;
    }

    /* Simplified EFE computation */
    /* EFE = Risk + Ambiguity */
    /* Risk: Expected prediction error */
    /* Ambiguity: Uncertainty about observations */

    predictive_fep_level_mapping_t* mapping =
        &bridge->level_mappings[region_index];

    float risk = 0.0f;
    float ambiguity = 0.0f;

    /* Compute risk from cached error buffer */
    for (uint32_t i = 0; i < mapping->buffer_size; i++) {
        risk += mapping->error_buffer[i] * mapping->error_buffer[i];
    }
    risk /= (float)mapping->buffer_size;

    /* Ambiguity from precision (inverse relationship) */
    if (bridge->fep_system && mapping->fep_level < bridge->fep_system->num_levels) {
        fep_hierarchy_level_t* level =
            &bridge->fep_system->levels[mapping->fep_level];

        for (uint32_t i = 0; i < level->beliefs.dim; i++) {
            ambiguity += safe_divide(1.0f, level->beliefs.precision[i], 1.0f);
        }
        ambiguity /= (float)level->beliefs.dim;
    }

    *efe = risk + ambiguity;
    bridge->effects.expected_free_energy = *efe;
    return 0;
}

/* ============================================================================
 * Update Cycle Implementation
 * ============================================================================ */

int predictive_regions_fep_bridge_update(
    predictive_regions_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    (void)delta_ms; /* Unused for now */

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* 1. Synchronize FEP beliefs to regions */
    if (bridge->config.enable_belief_sync) {
        predictive_regions_fep_sync_beliefs_to_regions(bridge);
    }

    /* 2. Generate top-down predictions */
    predictive_regions_fep_generate_predictions(bridge);

    /* 3. Apply precision modulation */
    predictive_regions_fep_apply_precision_modulation(bridge);

    /* 4. Propagate region errors to FEP */
    if (bridge->config.enable_error_propagation) {
        predictive_regions_fep_propagate_errors_to_fep(bridge);
    }

    /* 5. Adapt precision */
    if (bridge->config.enable_precision_adaptation) {
        predictive_regions_fep_adapt_precision(bridge);
    }

    /* 6. Compute free energy */
    float fe;
    if (predictive_regions_fep_compute_free_energy(bridge, &fe) == 0) {
        /* Check convergence */
        float delta_fe = fabsf(fe - bridge->state.total_free_energy);
        bridge->state.convergence_delta = delta_fe;
        bridge->state.converged =
            (delta_fe < bridge->config.convergence_threshold);

        if (bridge->state.converged) {
            bridge->state.convergence_iterations++;
        }
    }

    /* 7. Update statistics */
    bridge->stats.avg_free_energy =
        (bridge->stats.avg_free_energy * 0.99f) +
        (bridge->state.total_free_energy * 0.01f);

    if (bridge->fep_system && bridge->fep_system->num_levels > 0) {
        float pe = bridge->fep_system->levels[0].errors.magnitude;
        bridge->stats.avg_prediction_error =
            (bridge->stats.avg_prediction_error * 0.99f) + (pe * 0.01f);
        bridge->state.mean_prediction_error = pe;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * State/Stats Implementation
 * ============================================================================ */

int predictive_regions_fep_bridge_get_state(
    const predictive_regions_fep_bridge_t* bridge,
    predictive_regions_fep_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_regions_fep_bridge_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    *state = bridge->state;
    return 0;
}

int predictive_regions_fep_bridge_get_stats(
    const predictive_regions_fep_bridge_t* bridge,
    predictive_regions_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_regions_fep_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

bool predictive_regions_fep_is_converged(
    const predictive_regions_fep_bridge_t* bridge
) {
    return bridge && bridge->state.converged;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int predictive_regions_fep_bridge_connect_bio_async(
    predictive_regions_fep_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_PREDICTIVE_REGIONS_BRIDGE,
        .module_name = "predictive_regions_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Predictive regions-FEP bridge connected to bio-async");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }
    return 0;
}

int predictive_regions_fep_bridge_disconnect_bio_async(
    predictive_regions_fep_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool predictive_regions_fep_bridge_is_bio_async_connected(
    const predictive_regions_fep_bridge_t* bridge
) {
    return bridge && bridge->base.bio_async_enabled;
}
