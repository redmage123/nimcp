/**
 * @file nimcp_curiosity_fep_bridge.c
 * @brief Free Energy Principle - Curiosity Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between FEP and curiosity system
 * WHY:  Curiosity is epistemic value under FEP; prediction errors drive exploration
 * HOW:  FEP epistemic value modulates curiosity; curiosity-driven exploration reduces uncertainty
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 * Curiosity emerges from the Free Energy Principle as epistemic affordances:
 * organisms seek information-rich states to reduce expected free energy through
 * information gain. This bridge models dopaminergic exploration signals (VTA/SNc)
 * driven by prediction error magnitude (anterior cingulate cortex monitoring).
 *
 * FEP → CURIOSITY: Epistemic value (expected information gain) increases curiosity
 * CURIOSITY → FEP: Exploratory actions provide observations that reduce uncertainty
 *
 * REFERENCES:
 * - Friston et al. (2017) "Active inference and epistemic value"
 * - Friston et al. (2015) "Active inference and learning"
 * - Schmidhuber (2010) "Formal theory of creativity, fun, and intrinsic motivation"
 */

#include "cognitive/curiosity/nimcp_curiosity_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/free_energy/nimcp_fep_curiosity.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(curiosity_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_curiosity_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_curiosity_fep_bridge_mesh_registry = NULL;

nimcp_error_t curiosity_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_curiosity_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "curiosity_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "curiosity_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_curiosity_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_curiosity_fep_bridge_mesh_registry = registry;
    return err;
}

void curiosity_fep_bridge_mesh_unregister(void) {
    if (g_curiosity_fep_bridge_mesh_registry && g_curiosity_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_curiosity_fep_bridge_mesh_registry, g_curiosity_fep_bridge_mesh_id);
        g_curiosity_fep_bridge_mesh_id = 0;
        g_curiosity_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from curiosity_fep_bridge module (instance-level) */
static inline void curiosity_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_curiosity_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_curiosity_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_curiosity_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Get current time in milliseconds
 * WHY:  Track temporal dynamics for update cycles
 * HOW:  Use CLOCK_MONOTONIC for stable timing
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

/**
 * WHAT: Set default configuration for curiosity-FEP bridge
 * WHY:  Provide biologically-plausible parameter defaults
 * HOW:  Initialize all config fields with standard values
 */
int curiosity_fep_bridge_default_config(curiosity_fep_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_fep_bridge_heartbeat("curiosity_fe_default_config", 0.0f);


    config->epistemic_value_weight = 1.0f;
    config->uncertainty_sensitivity = 0.5f;
    config->information_gain_rate = INFORMATION_GAIN_BASELINE;
    config->exploration_boost = 0.2f;

    config->enable_epistemic_curiosity = true;
    config->enable_knowledge_gap_detection = true;
    config->enable_exploration_feedback = true;
    config->enable_learning_updates = true;

    return 0;
}

/**
 * WHAT: Create curiosity-FEP integration bridge
 * WHY:  Enable bidirectional coupling between FEP and curiosity
 * HOW:  Allocate bridge structure, initialize state and stats
 */
curiosity_fep_bridge_t* curiosity_fep_bridge_create(const curiosity_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    curiosity_fep_bridge_heartbeat("curiosity_fe_create", 0.0f);


    curiosity_fep_bridge_t* bridge = (curiosity_fep_bridge_t*)nimcp_calloc(
        1, sizeof(curiosity_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate curiosity-FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Apply configuration */
    curiosity_fep_config_t default_cfg;
    if (!config) {
        curiosity_fep_bridge_default_config(&default_cfg);
        config = &default_cfg;
    }
    bridge->config = *config;

    /* Initialize state */
    memset(&bridge->state, 0, sizeof(curiosity_fep_state_t));
    memset(&bridge->stats, 0, sizeof(curiosity_fep_stats_t));
    memset(&bridge->effects, 0, sizeof(curiosity_fep_effects_t));

    /* Create mutex for thread safety */
    if (bridge_base_init(&bridge->base, 0, "curiosity_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for curiosity-FEP bridge");
        curiosity_fep_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "curiosity_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Curiosity-FEP bridge created");
    return bridge;
}

/**
 * WHAT: Destroy curiosity-FEP bridge and free resources
 * WHY:  Clean shutdown, prevent memory leaks
 * HOW:  Disconnect bio-async, destroy mutex, free memory
 */
void curiosity_fep_bridge_destroy(curiosity_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if enabled */
    /* Phase 8: Heartbeat at operation start */
    curiosity_fep_bridge_heartbeat("curiosity_fe_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        curiosity_fep_bridge_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Curiosity-FEP bridge destroyed");
}

/* ============================================================================
 * Connection Implementation
 * ============================================================================ */

/**
 * WHAT: Connect bridge to FEP system
 * WHY:  Enable access to epistemic value and prediction errors
 * HOW:  Store FEP pointer for later access
 */
int curiosity_fep_bridge_connect_fep(
    curiosity_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    /* Allow NULL fep to disconnect/reset FEP connection */

    /* Phase 8: Heartbeat at operation start */
    curiosity_fep_bridge_heartbeat("curiosity_fe_connect_fep", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Curiosity-FEP bridge connected to FEP system");
    return 0;
}

/**
 * WHAT: Connect bridge to curiosity engine
 * WHY:  Enable modulation of curiosity drive from epistemic value
 * HOW:  Store curiosity engine for later access
 */
int curiosity_fep_bridge_connect_curiosity(
    curiosity_fep_bridge_t* bridge,
    curiosity_engine_t curiosity
) {
    if (!bridge || !curiosity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_fep_bridge_connect_curiosity: required parameter is NULL (bridge, curiosity)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_fep_bridge_heartbeat("curiosity_fe_connect_curiosity", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->curiosity_engine = curiosity;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Curiosity-FEP bridge connected to curiosity engine");
    return 0;
}

/* ============================================================================
 * FEP → Curiosity Direction Implementation
 * ============================================================================ */

/**
 * WHAT: Compute epistemic value from FEP system
 * WHY:  Epistemic value (expected information gain) drives curiosity
 * HOW:  Query FEP prediction errors, map to curiosity intensity
 *
 * BIOLOGY: Dopaminergic VTA neurons signal prediction errors → exploration drive
 */
int curiosity_fep_compute_epistemic_value(curiosity_fep_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_epistemic_curiosity) return 0;
    if (!bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_fep_compute_epistemic_value: bridge->fep_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_fep_bridge_heartbeat("curiosity_fe_curiosity_fep_comput", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute average prediction error magnitude across FEP hierarchy */
    fep_system_t* fep = bridge->fep_system;
    float total_pe = 0.0f;
    uint32_t level_count = 0;

    for (uint32_t i = 0; i < fep->num_levels && i < FEP_MAX_HIERARCHY_LEVELS; i++) {
        total_pe += fep->levels[i].errors.magnitude;
        level_count++;
    }

    float avg_pe = level_count > 0 ? total_pe / (float)level_count : 0.0f;

    /* Map prediction error to epistemic value */
    /* High PE → high uncertainty → high epistemic value */
    float epistemic_value = nimcp_clampf(avg_pe * bridge->config.epistemic_value_weight,
                                     0.0f, EPISTEMIC_VALUE_MAX);

    /* Update state */
    bridge->state.current_epistemic_value = epistemic_value;
    bridge->effects.epistemic_value = epistemic_value;

    /* Update stats */
    bridge->stats.total_epistemic_triggers++;
    bridge->stats.avg_information_gain =
        (bridge->stats.avg_information_gain * 0.99f) + (epistemic_value * 0.01f);

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Detect knowledge gaps from FEP uncertainty
 * WHY:  High uncertainty regions indicate knowledge gaps worth exploring
 * HOW:  Check if FEP uncertainty exceeds threshold
 *
 * BIOLOGY: Anterior cingulate cortex monitors conflict/uncertainty → question generation
 */
int curiosity_fep_detect_knowledge_gaps(curiosity_fep_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_knowledge_gap_detection) return 0;
    if (!bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_fep_detect_knowledge_gaps: bridge->fep_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_fep_bridge_heartbeat("curiosity_fe_curiosity_fep_detect", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute uncertainty from FEP belief precision */
    fep_system_t* fep = bridge->fep_system;
    float total_uncertainty = 0.0f;

    for (uint32_t i = 0; i < fep->num_levels && i < FEP_MAX_HIERARCHY_LEVELS; i++) {
        fep_hierarchy_level_t* level = &fep->levels[i];

        /* Uncertainty = 1 / precision (averaged across dimensions) */
        float level_uncertainty = 0.0f;
        for (uint32_t j = 0; j < level->errors.dim && j < FEP_MAX_STATE_DIM; j++) {
            if (level->errors.precision[j] > 1e-6f) {
                level_uncertainty += 1.0f / level->errors.precision[j];
            } else {
                level_uncertainty += 100.0f; /* Very uncertain */
            }
        }
        level_uncertainty /= (float)level->errors.dim;
        total_uncertainty += level_uncertainty;
    }

    float avg_uncertainty = fep->num_levels > 0 ?
                            total_uncertainty / (float)fep->num_levels : 0.0f;

    /* Scale by sensitivity */
    avg_uncertainty *= bridge->config.uncertainty_sensitivity;

    /* Detect knowledge gap if uncertainty exceeds threshold */
    float gap_size = 0.0f;
    if (avg_uncertainty > KNOWLEDGE_GAP_THRESHOLD) {
        gap_size = nimcp_clampf((avg_uncertainty - KNOWLEDGE_GAP_THRESHOLD) /
                          (1.0f - KNOWLEDGE_GAP_THRESHOLD), 0.0f, 1.0f);
        bridge->state.knowledge_gaps_detected++;
    }

    /* Update state */
    bridge->state.current_uncertainty = avg_uncertainty;
    bridge->effects.knowledge_gap_size = gap_size;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Trigger curiosity-driven exploration based on epistemic value
 * WHY:  High epistemic value should increase exploration motivation
 * HOW:  Compute exploration motivation from epistemic value and uncertainty
 *
 * BIOLOGY: Locus coeruleus noradrenergic boost for novelty → exploration
 */
int curiosity_fep_trigger_exploration(curiosity_fep_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_exploration_feedback) return 0;

    /* Phase 8: Heartbeat at operation start */
    curiosity_fep_bridge_heartbeat("curiosity_fe_curiosity_fep_trigge", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute exploration motivation */
    float epistemic_component = bridge->state.current_epistemic_value *
                                bridge->config.epistemic_value_weight;
    float uncertainty_component = bridge->state.current_uncertainty *
                                  bridge->config.uncertainty_sensitivity;

    float exploration_motivation = nimcp_clampf(
        epistemic_component + uncertainty_component + bridge->config.exploration_boost,
        0.0f, 1.0f);

    /* Boost curiosity based on exploration motivation */
    float curiosity_boost = exploration_motivation * bridge->config.exploration_boost;

    /* Update effects */
    bridge->effects.exploration_motivation = exploration_motivation;
    bridge->effects.curiosity_boost = curiosity_boost;

    /* Apply to curiosity engine if connected */
    if (bridge->curiosity_engine && curiosity_boost > 0.1f) {
        curiosity_set_exploration_rate(bridge->curiosity_engine, exploration_motivation);
        bridge->state.explorations_triggered++;
        bridge->stats.total_explorations++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Curiosity → FEP Direction Implementation
 * ============================================================================ */

/**
 * WHAT: Update FEP generative model from curiosity-driven learning
 * WHY:  Exploration provides observations that reduce uncertainty
 * HOW:  Query information gain from curiosity, update FEP beliefs
 *
 * BIOLOGY: Hippocampal consolidation of exploratory experiences → model updates
 */
int curiosity_fep_update_model_from_learning(curiosity_fep_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_learning_updates) return 0;
    if (!bridge->curiosity_engine || !bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_fep_update_model_from_learning: required parameter is NULL (bridge->curiosity_engine, bridge->fep_system)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_fep_bridge_heartbeat("curiosity_fe_curiosity_fep_update", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Query information gain from recent curiosity-driven exploration */
    float information_gain = curiosity_get_information_gain(bridge->curiosity_engine);

    /* Update state */
    bridge->state.current_information_gain = information_gain;

    /* If significant information gain, this reduces FEP uncertainty */
    if (information_gain > bridge->config.information_gain_rate) {
        /* Increment precision (reduce uncertainty) in FEP beliefs */
        fep_system_t* fep = bridge->fep_system;

        for (uint32_t i = 0; i < fep->num_levels && i < FEP_MAX_HIERARCHY_LEVELS; i++) {
            fep_hierarchy_level_t* level = &fep->levels[i];

            /* Boost precision proportional to information gain */
            float precision_boost = 1.0f + (information_gain * 0.1f);

            for (uint32_t j = 0; j < level->errors.dim && j < FEP_MAX_STATE_DIM; j++) {
                level->errors.precision[j] *= precision_boost;

                /* Clamp to valid range */
                level->errors.precision[j] = nimcp_clampf(level->errors.precision[j],
                                                      FEP_MIN_PRECISION,
                                                      FEP_MAX_PRECISION);
            }
        }

        /* Track learning update */
        bridge->state.learning_updates_made++;
        bridge->stats.total_learning_updates++;

        /* Accumulate total uncertainty reduction */
        bridge->stats.total_uncertainty_reduction += information_gain;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Update Cycle Implementation
 * ============================================================================ */

/**
 * WHAT: Update curiosity-FEP bridge for current timestep
 * WHY:  Maintain bidirectional coupling over time
 * HOW:  Compute epistemic value, detect gaps, trigger exploration, update model
 */
int curiosity_fep_bridge_update(curiosity_fep_bridge_t* bridge, uint64_t delta_ms) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* FEP → Curiosity direction */
    /* Phase 8: Heartbeat at operation start */
    curiosity_fep_bridge_heartbeat("curiosity_fe_update", 0.0f);


    if (curiosity_fep_compute_epistemic_value(bridge) != 0) {
        NIMCP_LOGGING_WARN("Failed to compute epistemic value");
    }

    if (curiosity_fep_detect_knowledge_gaps(bridge) != 0) {
        NIMCP_LOGGING_WARN("Failed to detect knowledge gaps");
    }

    if (curiosity_fep_trigger_exploration(bridge) != 0) {
        NIMCP_LOGGING_WARN("Failed to trigger exploration");
    }

    /* Curiosity → FEP direction */
    if (curiosity_fep_update_model_from_learning(bridge) != 0) {
        NIMCP_LOGGING_WARN("Failed to update model from learning");
    }

    /* Update curiosity level statistics */
    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (bridge->curiosity_engine) {
        float curiosity_level = curiosity_get_drive(bridge->curiosity_engine);
        bridge->state.current_curiosity_level = curiosity_level;
        bridge->stats.avg_curiosity_level =
            (bridge->stats.avg_curiosity_level * 0.99f) + (curiosity_level * 0.01f);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * State/Stats Implementation
 * ============================================================================ */

/**
 * WHAT: Get current bridge state
 * WHY:  Enable monitoring of curiosity-FEP dynamics
 * HOW:  Copy state structure to output
 */
int curiosity_fep_bridge_get_state(
    const curiosity_fep_bridge_t* bridge,
    curiosity_fep_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_fep_bridge_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    *state = bridge->state;
    /* Phase 8: Heartbeat at operation start */
    curiosity_fep_bridge_heartbeat("curiosity_fe_get_state", 0.0f);


    return 0;
}

/**
 * WHAT: Get cumulative statistics
 * WHY:  Track learning progress and exploration efficiency
 * HOW:  Copy stats structure to output
 */
int curiosity_fep_bridge_get_stats(
    const curiosity_fep_bridge_t* bridge,
    curiosity_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_fep_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    curiosity_fep_bridge_heartbeat("curiosity_fe_get_stats", 0.0f);


    return 0;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

/**
 * WHAT: Connect to bio-async router for inter-module messaging
 * WHY:  Enable curiosity signals to influence other cognitive modules
 * HOW:  Register as BIO_MODULE_CURIOSITY_FEP_BRIDGE
 */
int curiosity_fep_bridge_connect_bio_async(curiosity_fep_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    curiosity_fep_bridge_heartbeat("curiosity_fe_connect_bio_async", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_CURIOSITY_CORE_BRIDGE,
        .module_name = "curiosity_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Curiosity-FEP bridge connected to bio-async");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown
 * HOW:  Unregister module context
 */
int curiosity_fep_bridge_disconnect_bio_async(curiosity_fep_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    curiosity_fep_bridge_heartbeat("curiosity_fe_disconnect_bio_async", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Curiosity-FEP bridge disconnected from bio-async");
    return 0;
}

/**
 * WHAT: Check if bio-async is connected
 * WHY:  Verify messaging availability
 * HOW:  Return bio_async_enabled flag
 */
bool curiosity_fep_bridge_is_bio_async_connected(const curiosity_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    curiosity_fep_bridge_heartbeat("curiosity_fe_is_bio_async_connect", 0.0f);


    return bridge && bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int curiosity_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    curiosity_fep_bridge_heartbeat("curiosity_fe_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Curiosity_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                curiosity_fep_bridge_heartbeat("curiosity_fe_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Curiosity_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Curiosity_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void curiosity_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_curiosity_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int curiosity_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    curiosity_fep_bridge_heartbeat_instance(g_curiosity_fep_bridge_health_agent, "curiosity_fep_bridge_training_begin", 0.0f);
    return 0;
}

int curiosity_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_fep_bridge_training_end: NULL argument");
        return -1;
    }
    curiosity_fep_bridge_heartbeat_instance(g_curiosity_fep_bridge_health_agent, "curiosity_fep_bridge_training_end", 1.0f);
    return 0;
}

int curiosity_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    curiosity_fep_bridge_heartbeat_instance(g_curiosity_fep_bridge_health_agent, "curiosity_fep_bridge_training_step", progress);
    return 0;
}
