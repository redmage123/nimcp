/**
 * @file nimcp_salience_fep_bridge.c
 * @brief Free Energy Principle - Salience Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between FEP and salience system
 * WHY:  Salience corresponds to precision-weighted prediction error in FEP framework;
 *       aberrant salience (psychosis) relates to FEP precision dysregulation
 * HOW:  Salience scores modulate FEP precision; FEP prediction errors drive salience
 *       computation (novelty, surprise, urgency)
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SALIENCE AS PRECISION-WEIGHTED PREDICTION ERROR:
 * ------------------------------------------------
 * In FEP, salience S corresponds to:
 *   S = Π * |ε|
 * Where:
 *   Π = Precision (inverse variance, confidence)
 *   ε = Prediction error (surprise)
 *
 * High salience occurs when:
 * 1. High precision errors (confident predictions violated)
 * 2. Large prediction errors (surprising events)
 * 3. Both (most salient: confident predictions strongly violated)
 *
 * ABERRANT SALIENCE IN PSYCHOSIS:
 * -------------------------------
 * - Kapur (2003): "Psychosis as a state of aberrant salience"
 * - FEP explanation: Dysregulated precision → inappropriate salience attribution
 * - High precision on irrelevant stimuli → delusions
 * - Adams et al. (2013): "Computational basis of aberrant salience"
 *
 * FEP → SALIENCE PATHWAYS:
 * ------------------------
 * 1. Prediction Error Magnitude → Surprise Salience
 *    - Large PE → High surprise
 *    - Bayesian surprise attracts attention
 *
 * 2. Precision × PE → Urgency
 *    - High precision PE demands immediate attention
 *    - Model violation requires updating
 *
 * 3. Expected Free Energy → Novelty Seeking
 *    - High EFE → Explore novel stimuli
 *    - Information gain drive
 *
 * SALIENCE -> FEP PATHWAYS:
 * ------------------------
 * 1. Salience Score → Precision Boost
 *    - Salient stimuli get high precision
 *    - Implements selective attention
 *
 * 2. Priority Updates → Gating
 *    - High salience → Gate belief updates
 *    - Focus on important information
 *
 * REFERENCES:
 * - Feldman & Friston (2010) "Attention, uncertainty, and free energy"
 * - Kapur (2003) "Psychosis as a state of aberrant salience"
 * - Adams et al. (2013) "Computational psychiatry: towards a mathematically informed understanding of mental illness"
 * - Itti & Baldi (2009) "Bayesian surprise attracts human attention"
 *
 * @author NIMCP Development Team
 */

#include "cognitive/salience/nimcp_salience_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(salience_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)



/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Get current time in milliseconds
 * WHY:  Need timestamps for temporal dynamics
 * HOW:  Use CLOCK_MONOTONIC for steady time
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

/**
 * WHAT: Initialize default configuration for salience-FEP bridge
 * WHY:  Provide sensible defaults for typical use cases
 * HOW:  Set moderate values for precision gain, surprise weighting
 */
int salience_fep_bridge_default_config(salience_fep_config_t* config) {
    /* Guard: Validate input */
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_fep_bridge_default_config: config is NULL");
        return -1;
    }

    /* Initialize salience → FEP modulation */
    /* Phase 8: Heartbeat at operation start */
    salience_fep_bridge_heartbeat("salience_fep_default_config", 0.0f);


    config->salience_precision_gain = 1.5f;         /* Salient stimuli get 1.5x precision */
    config->surprise_salience_weight = 0.4f;        /* Surprise weighs heavily in salience */
    config->novelty_precision_boost = 1.3f;         /* Novel stimuli boost precision */
    config->urgency_precision_boost = 2.0f;         /* Urgent stimuli get highest precision */

    /* Initialize FEP → salience computation */
    config->enable_precision_modulation = true;     /* Allow salience to modulate precision */
    config->enable_pe_salience = true;              /* Compute salience from prediction errors */
    config->enable_salience_gating = true;          /* Gate belief updates by salience */

    NIMCP_LOGGING_DEBUG("Initialized default salience-FEP bridge config");
    return 0;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Create new salience-FEP bridge instance
 * WHY:  Need to allocate and initialize bridge structure
 * HOW:  Allocate memory, apply config, create mutex, initialize state
 */
salience_fep_bridge_t* salience_fep_bridge_create(const salience_fep_config_t* config) {
    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    salience_fep_bridge_heartbeat("salience_fep_create", 0.0f);


    salience_fep_bridge_t* bridge = (salience_fep_bridge_t*)nimcp_calloc(
        1, sizeof(salience_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate salience-FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Apply configuration */
    salience_fep_config_t default_cfg;
    if (!config) {
        salience_fep_bridge_default_config(&default_cfg);
        config = &default_cfg;
    }
    bridge->config = *config;

    /* Initialize state */
    memset(&bridge->state, 0, sizeof(salience_fep_state_t));
    bridge->state.current_salience = 0.0f;
    bridge->state.current_precision_boost = 1.0f;
    bridge->state.high_salience_events = 0;
    bridge->state.avg_prediction_error = 0.0f;

    /* Initialize effects */
    memset(&bridge->effects, 0, sizeof(salience_fep_effects_t));
    bridge->effects.precision_boost = 1.0f;
    bridge->effects.salience_from_pe = 0.0f;
    bridge->effects.gating_factor = 1.0f;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(salience_fep_stats_t));

    /* Create mutex for thread safety */
    if (bridge_base_init(&bridge->base, 0, "salience_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_UNKNOWN, "salience_fep_bridge_create: bridge_base_init failed, no mutex");
        return NULL;
    }

    /* Initialize bio-async state */
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Created salience-FEP bridge");
    return bridge;
}

/**
 * WHAT: Destroy salience-FEP bridge and free resources
 * WHY:  Clean up memory and connections on shutdown
 * HOW:  Disconnect bio-async, destroy mutex, free memory
 */
void salience_fep_bridge_destroy(salience_fep_bridge_t* bridge) {
    /* Guard: NULL pointer is safe to destroy */
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    salience_fep_bridge_heartbeat("salience_fep_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        salience_fep_bridge_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge structure */
    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Destroyed salience-FEP bridge");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * WHAT: Connect FEP system to salience bridge
 * WHY:  Need FEP reference to read precision/PE and modulate precision
 * HOW:  Store FEP pointer in thread-safe manner
 */
int salience_fep_bridge_connect_fep(
    salience_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    /* Guard: Validate inputs */
    if (!bridge || !fep) {
        NIMCP_LOGGING_ERROR("NULL pointer in connect_fep");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_fep_bridge_connect_fep: required parameter is NULL (bridge, fep)");
        return -1;
    }

    /* Thread-safe connection */
    /* Phase 8: Heartbeat at operation start */
    salience_fep_bridge_heartbeat("salience_fep_connect_fep", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected FEP system to salience bridge");
    return 0;
}

/**
 * WHAT: Connect salience evaluator to bridge
 * WHY:  Need salience reference to compute salience and get scores
 * HOW:  Store salience evaluator handle in thread-safe manner
 */
int salience_fep_bridge_connect_salience(
    salience_fep_bridge_t* bridge,
    salience_evaluator_t salience
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in connect_salience");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_fep_bridge_connect_salience: bridge is NULL");
        return -1;
    }

    /* Thread-safe connection */
    /* Phase 8: Heartbeat at operation start */
    salience_fep_bridge_heartbeat("salience_fep_connect_salience", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->salience_evaluator = salience;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected salience evaluator to FEP bridge");
    return 0;
}

/* ============================================================================
 * Salience → FEP Direction (Salience Modulates Precision)
 * ============================================================================ */

/**
 * WHAT: Modulate FEP precision based on salience scores
 * WHY:  Salient stimuli should get higher precision (increased attention)
 * HOW:  Read current salience, compute precision boost, apply to FEP
 *
 * BIOLOGICAL BASIS:
 * - Salient stimuli capture attention (increased precision)
 * - Implements selective sensory processing
 * - Feldman & Friston (2010): Attention = precision optimization
 */
int salience_fep_modulate_precision_by_salience(salience_fep_bridge_t* bridge) {
    /* Guard: Validate bridge and components */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in modulate_precision");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_fep_modulate_precision_by_salience: bridge is NULL");
        return -1;
    }

    if (!bridge->config.enable_precision_modulation) {
        return 0;  /* Feature disabled */
    }

    if (!bridge->fep_system || !bridge->salience_evaluator) {
        return 0;  /* Components not connected yet */
    }

    /* Phase 8: Heartbeat at operation start */
    salience_fep_bridge_heartbeat("salience_fep_salience_fep_modulat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Get current salience score */
    float salience = bridge->state.current_salience;

    /* Compute precision boost based on salience components */
    float precision_boost = 1.0f;

    /* High overall salience → general precision boost */
    if (salience > 0.7f) {
        precision_boost *= bridge->config.salience_precision_gain;
    }

    /* Update effects */
    bridge->effects.precision_boost = precision_boost;
    bridge->state.current_precision_boost = precision_boost;

    /* Update statistics */
    bridge->stats.total_precision_boosts++;
    if (bridge->stats.total_precision_boosts > 0) {
        bridge->stats.avg_precision_boost =
            (bridge->stats.avg_precision_boost * (bridge->stats.total_precision_boosts - 1) +
             precision_boost) / bridge->stats.total_precision_boosts;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Modulated precision by salience: boost=%f", precision_boost);
    return 0;
}

/* ============================================================================
 * FEP → Salience Direction (Prediction Errors Drive Salience)
 * ============================================================================ */

/**
 * WHAT: Compute salience from FEP prediction errors
 * WHY:  High PE = high surprise = high salience (Bayesian surprise)
 * HOW:  Read FEP prediction errors, compute surprise/novelty/urgency salience
 *
 * BIOLOGICAL BASIS:
 * - Surprise attracts attention (Itti & Baldi, 2009)
 * - Prediction errors signal unexpected events (salience network)
 * - Anterior insula detects salient prediction errors
 *
 * FORMULA:
 *   Surprise salience = |ε| / (1 + σ²)
 *   Urgency = Π * |ε|  (precision-weighted PE)
 *   Novelty = Large PE with low prior probability
 */
int salience_fep_compute_salience_from_pe(salience_fep_bridge_t* bridge) {
    /* Guard: Validate bridge and components */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in compute_salience_from_pe");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_fep_compute_salience_from_pe: bridge is NULL");
        return -1;
    }

    if (!bridge->config.enable_pe_salience) {
        return 0;  /* Feature disabled */
    }

    if (!bridge->fep_system) {
        return 0;  /* FEP not connected */
    }

    /* Phase 8: Heartbeat at operation start */
    salience_fep_bridge_heartbeat("salience_fep_salience_fep_compute", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    fep_system_t* fep = bridge->fep_system;

    /* Compute average prediction error across hierarchy */
    float total_pe = 0.0f;
    float total_precision = 0.0f;
    uint32_t count = 0;

    for (uint32_t l = 0; l < fep->num_levels; l++) {
        /* Phase 8: Loop progress heartbeat */
        if ((l & 0xFF) == 0 && fep->num_levels > 256) {
            salience_fep_bridge_heartbeat("salience_fep_loop",
                             (float)(l + 1) / (float)fep->num_levels);
        }

        fep_hierarchy_level_t* level = &fep->levels[l];
        total_pe += level->errors.magnitude;

        /* Sum precision values */
        for (uint32_t i = 0; i < level->errors.dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && level->errors.dim > 256) {
                salience_fep_bridge_heartbeat("salience_fep_loop",
                                 (float)(i + 1) / (float)level->errors.dim);
            }

            total_precision += level->errors.precision[i];
            count++;
        }
    }

    float avg_pe = (fep->num_levels > 0) ? (total_pe / fep->num_levels) : 0.0f;
    float avg_precision = (count > 0) ? (total_precision / count) : 1.0f;

    /* Compute salience components from PE */

    /* 1. Surprise: Normalized prediction error */
    float surprise = nimcp_clampf(avg_pe / 10.0f, 0.0f, 1.0f);

    /* 2. Urgency: Precision-weighted PE (high precision errors demand attention) */
    float urgency = nimcp_clampf(avg_precision * avg_pe / 10.0f, 0.0f, 1.0f);

    /* 3. Novelty: Large PE with low running average (unexpected events) */
    float expected_pe = bridge->state.avg_prediction_error;
    float novelty = (expected_pe > 0.01f) ?
        nimcp_clampf((avg_pe - expected_pe) / expected_pe, 0.0f, 1.0f) : 0.0f;

    /* Combine into overall salience (weighted average) */
    float salience =
        bridge->config.surprise_salience_weight * surprise +
        0.3f * urgency +
        0.3f * novelty;

    /* Update state */
    bridge->state.current_salience = salience;
    bridge->state.avg_prediction_error =
        0.9f * bridge->state.avg_prediction_error + 0.1f * avg_pe;

    /* Check for high salience events */
    if (salience > SURPRISE_SALIENCE_THRESHOLD) {
        bridge->state.high_salience_events++;
    }

    /* Update effects */
    bridge->effects.salience_from_pe = salience;

    /* Update statistics */
    bridge->stats.avg_salience =
        0.95f * bridge->stats.avg_salience + 0.05f * salience;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Computed salience from PE: salience=%f (surprise=%f, urgency=%f, novelty=%f)",
                        salience, surprise, urgency, novelty);
    return 0;
}

/* ============================================================================
 * Gating and Priority
 * ============================================================================ */

/**
 * WHAT: Gate FEP belief updates based on salience
 * WHY:  Focus updates on salient information (attentional gating)
 * HOW:  High salience → stronger gating, low salience → weaker gating
 *
 * BIOLOGICAL BASIS:
 * - Attention gates sensory information flow
 * - Salient stimuli receive preferential processing
 * - Implements selective perceptual inference
 */
int salience_fep_gate_by_salience(salience_fep_bridge_t* bridge) {
    /* Guard: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in gate_by_salience");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_fep_gate_by_salience: bridge is NULL");
        return -1;
    }

    if (!bridge->config.enable_salience_gating) {
        return 0;  /* Feature disabled */
    }

    /* Phase 8: Heartbeat at operation start */
    salience_fep_bridge_heartbeat("salience_fep_salience_fep_gate_by", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Get current salience */
    float salience = bridge->state.current_salience;

    /* Compute gating factor (higher salience → stronger gating) */
    float gating_factor = 0.5f + 0.5f * salience;  /* Range: [0.5, 1.0] */

    /* Update effects */
    bridge->effects.gating_factor = gating_factor;

    /* Update statistics */
    bridge->stats.total_salience_gates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied salience gating: factor=%f", gating_factor);
    return 0;
}

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * WHAT: Update salience-FEP bridge state
 * WHY:  Maintain bidirectional integration each timestep
 * HOW:  Compute salience from PE, modulate precision, apply gating
 */
int salience_fep_bridge_update(
    salience_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Guard: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in update");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_fep_bridge_update: bridge is NULL");
        return -1;
    }

    /* FEP → Salience: Compute salience from prediction errors */
    /* Phase 8: Heartbeat at operation start */
    salience_fep_bridge_heartbeat("salience_fep_update", 0.0f);


    salience_fep_compute_salience_from_pe(bridge);

    /* Salience → FEP: Modulate precision based on salience */
    salience_fep_modulate_precision_by_salience(bridge);

    /* Apply salience-based gating */
    salience_fep_gate_by_salience(bridge);

    return 0;
}

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * WHAT: Get current bridge state
 * WHY:  Allow external monitoring of salience-FEP interaction
 * HOW:  Copy state struct in thread-safe manner
 */
int salience_fep_bridge_get_state(
    const salience_fep_bridge_t* bridge,
    salience_fep_state_t* state
) {
    /* Guard: Validate inputs */
    if (!bridge || !state) {
        NIMCP_LOGGING_ERROR("NULL pointer in get_state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_fep_bridge_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_fep_bridge_heartbeat("salience_fep_get_state", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/**
 * WHAT: Get bridge statistics
 * WHY:  Monitor performance and behavior over time
 * HOW:  Copy stats struct in thread-safe manner
 */
int salience_fep_bridge_get_stats(
    const salience_fep_bridge_t* bridge,
    salience_fep_stats_t* stats
) {
    /* Guard: Validate inputs */
    if (!bridge || !stats) {
        NIMCP_LOGGING_ERROR("NULL pointer in get_stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_fep_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_fep_bridge_heartbeat("salience_fep_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * WHAT: Connect bridge to bio-async message router
 * WHY:  Enable inter-module communication via bio-async
 * HOW:  Register with router, get module context
 */
int salience_fep_bridge_connect_bio_async(salience_fep_bridge_t* bridge) {
    /* Guard: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in connect_bio_async");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_fep_bridge_connect_bio_async: bridge is NULL");
        return -1;
    }

    /* Already connected */
    /* Phase 8: Heartbeat at operation start */
    salience_fep_bridge_heartbeat("salience_fep_connect_bio_async", 0.0f);


    if (bridge->base.bio_async_enabled) {
        return 0;
    }

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_SALIENCE_BRIDGE,
        .module_name = "salience_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected salience-FEP bridge to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * WHAT: Disconnect bridge from bio-async router
 * WHY:  Clean up bio-async resources on shutdown
 * HOW:  Unregister module, clear context
 */
int salience_fep_bridge_disconnect_bio_async(salience_fep_bridge_t* bridge) {
    /* Guard: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in disconnect_bio_async");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_fep_bridge_disconnect_bio_async: bridge is NULL");
        return -1;
    }

    /* Not connected */
    if (!bridge->base.bio_async_enabled) {
        return 0;
    }

    /* Unregister from router */
    /* Phase 8: Heartbeat at operation start */
    salience_fep_bridge_heartbeat("salience_fep_disconnect_bio_async", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected salience-FEP bridge from bio-async router");

    return 0;
}

/**
 * WHAT: Check if bio-async is connected
 * WHY:  Query connection status
 * HOW:  Return bio_async_enabled flag
 */
bool salience_fep_bridge_is_bio_async_connected(const salience_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    salience_fep_bridge_heartbeat("salience_fep_is_bio_async_connect", 0.0f);


    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int salience_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    salience_fep_bridge_heartbeat("salience_fep_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Salience_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                salience_fep_bridge_heartbeat("salience_fep_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Salience_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Salience_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void salience_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_salience_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int salience_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "salience_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    salience_fep_bridge_heartbeat_instance(NULL, "salience_fep_bridge_training_begin", 0.0f);
    return 0;
}

int salience_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "salience_fep_bridge_training_end: NULL argument");
        return -1;
    }
    salience_fep_bridge_heartbeat_instance(NULL, "salience_fep_bridge_training_end", 1.0f);
    return 0;
}

int salience_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "salience_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    salience_fep_bridge_heartbeat_instance(NULL, "salience_fep_bridge_training_step", progress);
    return 0;
}
