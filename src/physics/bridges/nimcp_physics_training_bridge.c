#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_physics_training_bridge.c - Physics Layer to Training Layer Bridge
//=============================================================================

#include "utils/bridge/nimcp_bridge_base.h"
#include "physics/bridges/nimcp_physics_training_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(physics_training_bridge)

#define LOG_MODULE "PHYSICS_TRAINING_BRIDGE"


//=============================================================================
// Internal Structure
//=============================================================================

struct physics_train_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    physics_train_config_t config;
    nimcp_thermodynamic_state_t* thermo;
    nimcp_hh_population_t* hh_pop;
    physics_train_modulation_t current_modulation;
    physics_train_stats_t stats;
    float lr_scale_sum;
    uint64_t sample_count;
    bool initialized;
};

//=============================================================================
// Configuration API
//=============================================================================

int physics_train_default_config(physics_train_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->enable_metabolic_gating = true;
    config->atp_threshold = PHYSICS_TRAIN_ATP_THRESHOLD;
    config->learning_cost = PHYSICS_TRAIN_LEARNING_COST;
    config->base_stdp_window_ms = PHYSICS_TRAIN_PLASTICITY_WINDOW;
    config->enable_temp_effects = true;

    return 0;
}

//=============================================================================
// Lifecycle API
//=============================================================================

physics_train_bridge_t* physics_train_bridge_create(
    const physics_train_config_t* config
) {
    physics_train_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate physics-training bridge");
    if (bridge_base_init(&bridge->base, 0, "physics_training") != 0) { nimcp_free(bridge); return NULL; }

    if (config) {
        bridge->config = *config;
    } else {
        physics_train_default_config(&bridge->config);
    }

    /* Initialize modulation to defaults */
    bridge->current_modulation.learning_rate_scale = 1.0f;
    bridge->current_modulation.plasticity_enabled = true;
    bridge->current_modulation.available_atp = 1.0f;
    bridge->current_modulation.metabolic_cost = bridge->config.learning_cost;
    bridge->current_modulation.stdp_window_ms = bridge->config.base_stdp_window_ms;

    bridge->initialized = true;

    NIMCP_LOG_INFO(PHYSICS_TRAIN_MODULE_NAME,
        "Physics-training bridge created: metabolic_gating=%d",
        bridge->config.enable_metabolic_gating);

    return bridge;
}

void physics_train_bridge_destroy(physics_train_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "physics_training");

    NIMCP_LOG_INFO(PHYSICS_TRAIN_MODULE_NAME,
        "Bridge destroyed - modulations: %lu, blocks: %lu, cost: %.3f",
        (unsigned long)bridge->stats.modulations_computed,
        (unsigned long)bridge->stats.plasticity_blocks,
        bridge->stats.total_learning_cost);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int physics_train_connect_physics(
    physics_train_bridge_t* bridge,
    nimcp_thermodynamic_state_t* thermo,
    nimcp_hh_population_t* hh_pop
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->thermo = thermo;
    bridge->hh_pop = hh_pop;

    NIMCP_LOG_DEBUG(PHYSICS_TRAIN_MODULE_NAME,
        "Connected physics: thermo=%p, hh=%p",
        (void*)thermo, (void*)hh_pop);

    return 0;
}

//=============================================================================
// Core API
//=============================================================================

int physics_train_get_modulation(
    physics_train_bridge_t* bridge,
    physics_train_modulation_t* modulation
) {
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_train_get_modulation: required parameter is NULL (bridge, modulation)");
        return -1;
    }

    float atp_level = 1.0f;

    /* Get ATP from thermodynamics */
    if (bridge->thermo) {
        double atp = nimcp_thermo_get_atp_ratio(bridge->thermo);
        atp_level = (float)atp;
    }

    bridge->current_modulation.available_atp = atp_level;

    /* Check metabolic gating */
    if (bridge->config.enable_metabolic_gating) {
        if (atp_level < bridge->config.atp_threshold) {
            bridge->current_modulation.plasticity_enabled = false;
            bridge->current_modulation.learning_rate_scale = 0.0f;
            bridge->stats.plasticity_blocks++;
        } else {
            bridge->current_modulation.plasticity_enabled = true;
            /* Scale learning rate by ATP availability */
            float excess = (atp_level - bridge->config.atp_threshold) /
                          (1.0f - bridge->config.atp_threshold);
            bridge->current_modulation.learning_rate_scale = 0.5f + 0.5f * excess;
        }
    } else {
        bridge->current_modulation.plasticity_enabled = true;
        bridge->current_modulation.learning_rate_scale = 1.0f;
    }

    /* Clamp learning rate scale */
    if (bridge->current_modulation.learning_rate_scale < 0.0f) {
        bridge->current_modulation.learning_rate_scale = 0.0f;
    }
    if (bridge->current_modulation.learning_rate_scale > 2.0f) {
        bridge->current_modulation.learning_rate_scale = 2.0f;
    }

    /* Update stats */
    bridge->stats.modulations_computed++;
    bridge->lr_scale_sum += bridge->current_modulation.learning_rate_scale;
    bridge->sample_count++;
    if (bridge->sample_count > 0) {
        bridge->stats.avg_lr_scale =
            bridge->lr_scale_sum / (float)bridge->sample_count;
    }

    *modulation = bridge->current_modulation;

    return 0;
}

int physics_train_report_feedback(
    physics_train_bridge_t* bridge,
    const physics_train_feedback_t* feedback
) {
    if (!bridge || !feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_train_report_feedback: required parameter is NULL (bridge, feedback)");
        return -1;
    }

    /* Compute metabolic cost */
    float cost = feedback->update_magnitude *
                 feedback->synapses_updated *
                 bridge->config.learning_cost;

    bridge->stats.total_learning_cost += cost;

    /* In full implementation, would debit ATP from thermodynamics */
    /* nimcp_thermo_consume_atp(bridge->thermo, cost); */

    NIMCP_LOG_DEBUG(PHYSICS_TRAIN_MODULE_NAME,
        "Training feedback: magnitude=%.3f, synapses=%u, cost=%.4f",
        feedback->update_magnitude, feedback->synapses_updated, cost);

    return 0;
}

bool physics_train_is_plasticity_enabled(const physics_train_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->current_modulation.plasticity_enabled;
}

int physics_train_update(physics_train_bridge_t* bridge, float dt) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    (void)dt;  /* Time-based updates not needed currently */

    /* Refresh modulation */
    physics_train_modulation_t mod;
    physics_train_get_modulation(bridge, &mod);

    bridge->stats.last_update_ms += dt;

    return 0;
}

int physics_train_get_stats(
    const physics_train_bridge_t* bridge,
    physics_train_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_train_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}
