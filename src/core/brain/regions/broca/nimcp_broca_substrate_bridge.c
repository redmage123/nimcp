/**
 * @file nimcp_broca_substrate_bridge.c
 * @brief Broca-Substrate Bridge Implementation
 *
 * Links language production to metabolic state for biologically
 * realistic speech production under fatigue/stress conditions.
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/broca/nimcp_broca_substrate_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(broca_substrate_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "BROCA_SUBSTRATE_BRIDGE"


//=============================================================================
// Internal Constants
//=============================================================================

#define LOW_ATP_THRESHOLD 0.3f
#define HIGH_FATIGUE_THRESHOLD 0.7f

//=============================================================================
// Internal Structure
//=============================================================================

struct broca_substrate_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* broca;                         /**< Broca adapter handle */
    neural_substrate_t* substrate;       /**< Neural substrate handle */
    broca_substrate_config_t config;     /**< Configuration */
    broca_substrate_effects_t effects;   /**< Current effects */
    broca_substrate_stats_t stats;       /**< Statistics */
    bio_router_t* router;                /**< Bio-async router */
    bool registered;                     /**< Registered with router */
};

//=============================================================================
// Configuration API
//=============================================================================

broca_substrate_config_t broca_substrate_default_config(void) {
    return (broca_substrate_config_t){
        .enable_atp_modulation = true,
        .enable_fatigue_modulation = true,
        .enable_bio_async = true,
        .atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .min_capacity = 0.1f,
        .fluency_atp_weight = 1.0f,
        .syntax_fatigue_weight = 1.0f
    };
}

//=============================================================================
// Lifecycle API
//=============================================================================

broca_substrate_bridge_t* broca_substrate_bridge_create(
    void* broca,
    neural_substrate_t* substrate,
    const broca_substrate_config_t* config
) {
    broca_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(broca_substrate_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "broca_substrate") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "broca_substrate_bridge_create: bridge_base_init failed");
        return NULL;
    }

    bridge->broca = broca;
    bridge->substrate = substrate;
    bridge->config = config ? *config : broca_substrate_default_config();

    /* Initialize effects to full capacity */
    bridge->effects.speech_fluency = 1.0f;
    bridge->effects.word_retrieval = 1.0f;
    bridge->effects.syntax_complexity = 1.0f;
    bridge->effects.articulation_precision = 1.0f;
    bridge->effects.phonological_accuracy = 1.0f;
    bridge->effects.overall_capacity = 1.0f;

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.min_observed_capacity = 1.0f;
    bridge->router = NULL;
    bridge->registered = false;

    return bridge;
}

void broca_substrate_bridge_destroy(broca_substrate_bridge_t* bridge) {
    if (bridge) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
    }
}

//=============================================================================
// Update API
//=============================================================================

int broca_substrate_bridge_update(broca_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    float atp_level = 1.0f;
    float fatigue_level = 0.0f;

    /* Get metabolic state from substrate if available */
    if (bridge->substrate) {
        substrate_metabolic_state_t metabolic_state;
        if (substrate_get_metabolic_state(bridge->substrate, &metabolic_state) == 0) {
            atp_level = metabolic_state.atp_level;
            fatigue_level = 1.0f - metabolic_state.metabolic_capacity;
        }
    }

    /* Compute ATP-modulated effects */
    float atp_factor = 1.0f;
    if (bridge->config.enable_atp_modulation) {
        /* ATP affects word retrieval and fluency most */
        atp_factor = atp_level * bridge->config.atp_sensitivity;
        if (atp_factor < bridge->config.min_capacity) {
            atp_factor = bridge->config.min_capacity;
        }
        if (atp_factor > 1.0f) atp_factor = 1.0f;

        if (atp_level < LOW_ATP_THRESHOLD) {
            bridge->stats.low_atp_events++;
        }
    }

    /* Compute fatigue-modulated effects */
    float fatigue_factor = 1.0f;
    if (bridge->config.enable_fatigue_modulation) {
        /* Fatigue affects syntax complexity and articulation precision */
        fatigue_factor = 1.0f - (fatigue_level * bridge->config.fatigue_sensitivity);
        if (fatigue_factor < bridge->config.min_capacity) {
            fatigue_factor = bridge->config.min_capacity;
        }
        if (fatigue_factor > 1.0f) fatigue_factor = 1.0f;

        if (fatigue_level > HIGH_FATIGUE_THRESHOLD) {
            bridge->stats.high_fatigue_events++;
        }
    }

    /* Apply effects with appropriate weights */
    bridge->effects.speech_fluency = atp_factor * bridge->config.fluency_atp_weight;
    if (bridge->effects.speech_fluency > 1.0f) bridge->effects.speech_fluency = 1.0f;

    bridge->effects.word_retrieval = atp_factor * 1.05f;  /* ATP-dependent */
    if (bridge->effects.word_retrieval > 1.0f) bridge->effects.word_retrieval = 1.0f;

    bridge->effects.syntax_complexity = fatigue_factor * bridge->config.syntax_fatigue_weight;
    if (bridge->effects.syntax_complexity > 1.0f) bridge->effects.syntax_complexity = 1.0f;

    bridge->effects.articulation_precision = fatigue_factor * 0.95f;  /* Fatigue-sensitive */
    if (bridge->effects.articulation_precision > 1.0f) bridge->effects.articulation_precision = 1.0f;

    bridge->effects.phonological_accuracy = (atp_factor + fatigue_factor) * 0.5f;  /* Both affect */
    if (bridge->effects.phonological_accuracy > 1.0f) bridge->effects.phonological_accuracy = 1.0f;

    /* Compute overall capacity */
    bridge->effects.overall_capacity = (
        bridge->effects.speech_fluency +
        bridge->effects.word_retrieval +
        bridge->effects.syntax_complexity +
        bridge->effects.articulation_precision +
        bridge->effects.phonological_accuracy
    ) / 5.0f;

    /* Update statistics */
    bridge->stats.updates_processed++;

    float prev_avg_fluency = bridge->stats.avg_speech_fluency;
    float new_avg_fluency = (prev_avg_fluency * (bridge->stats.updates_processed - 1) +
                             bridge->effects.speech_fluency) / bridge->stats.updates_processed;
    if (isfinite(new_avg_fluency)) bridge->stats.avg_speech_fluency = new_avg_fluency;

    float prev_avg_syntax = bridge->stats.avg_syntax_complexity;
    float new_avg_syntax = (prev_avg_syntax * (bridge->stats.updates_processed - 1) +
                            bridge->effects.syntax_complexity) / bridge->stats.updates_processed;
    if (isfinite(new_avg_syntax)) bridge->stats.avg_syntax_complexity = new_avg_syntax;

    if (bridge->effects.overall_capacity < bridge->stats.min_observed_capacity) {
        bridge->stats.min_observed_capacity = bridge->effects.overall_capacity;
    }

    return 0;
}

int broca_substrate_bridge_get_effects(
    const broca_substrate_bridge_t* bridge,
    broca_substrate_effects_t* effects
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "effects is NULL");
        return -1;
    }
    *effects = bridge->effects;
    return 0;
}

int broca_substrate_bridge_apply_effects(broca_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    /* Update effects from substrate first */
    broca_substrate_bridge_update(bridge);

    /*
     * TODO: Apply effects to Broca adapter
     * Would call something like:
     * broca_adapter_set_fluency_factor(bridge->broca, bridge->effects.speech_fluency);
     * broca_adapter_set_syntax_capacity(bridge->broca, bridge->effects.syntax_complexity);
     */

    return 0;
}

//=============================================================================
// Bio-Async API
//=============================================================================

int broca_substrate_bridge_register_bio_async(
    broca_substrate_bridge_t* bridge,
    bio_router_t* router
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) return 0;

    bridge->router = router;
    bridge->registered = true;

    /* Register as module with bio router */
    /* bio_router_register_module(router, BIO_MODULE_SUBSTRATE_BROCA, bridge); */

    return 0;
}

//=============================================================================
// Statistics API
//=============================================================================

int broca_substrate_bridge_get_stats(
    const broca_substrate_bridge_t* bridge,
    broca_substrate_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stats is NULL");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void broca_substrate_bridge_reset_stats(broca_substrate_bridge_t* bridge) {
    if (bridge) {
        memset(&bridge->stats, 0, sizeof(bridge->stats));
        bridge->stats.min_observed_capacity = 1.0f;
    }
}
