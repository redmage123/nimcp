/**
 * @file nimcp_self_introspection_bridge.c
 * @brief Bridge between Self-Model and Introspection systems
 *
 * WHAT: Bidirectional integration where the self-model guides introspective queries
 *       and introspection results update the self-model.
 *
 * WHY: Self-awareness emerges from the interplay between a stable self-representation
 *      (self-model) and active self-examination (introspection). The self-model
 *      provides context for introspection; introspection refines the self-model.
 *
 * HOW: Self-model state influences what introspection examines and how results are
 *      interpreted. Introspection results trigger self-model updates and can
 *      initiate reflective processes when discrepancies are detected.
 *
 * BIOLOGICAL BASIS:
 * - Self-model relies on medial prefrontal cortex (mPFC) for self-referential processing
 * - Introspection engages anterior cingulate cortex (ACC) for monitoring
 * - Posterior cingulate cortex (PCC) integrates self-awareness
 * - Insula provides interoceptive self-knowledge
 *
 * @author NIMCP Development Team
 * @date 2025-01
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/integration/nimcp_self_introspection_bridge.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(self_introspection_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_self_introspection_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_self_introspection_bridge_mesh_registry = NULL;

nimcp_error_t self_introspection_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_self_introspection_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "self_introspection_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "self_introspection_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_self_introspection_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_self_introspection_bridge_mesh_registry = registry;
    return err;
}

void self_introspection_bridge_mesh_unregister(void) {
    if (g_self_introspection_bridge_mesh_registry && g_self_introspection_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_self_introspection_bridge_mesh_registry, g_self_introspection_bridge_mesh_id);
        g_self_introspection_bridge_mesh_id = 0;
        g_self_introspection_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from self_introspection_bridge module (instance-level) */
static inline void self_introspection_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_self_introspection_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_self_introspection_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_self_introspection_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "SELF_INTROSPECTION_BRIDGE"


/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal self-model state representation
 *
 * WHAT: Tracks core aspects of self-awareness
 * WHY: Self-model needs internal state to guide introspection
 * HOW: Maintained through introspection feedback loop
 */
typedef struct self_model_state {
    float self_awareness;       /**< Overall self-awareness level [0.0-1.0] */
    float agency;               /**< Sense of control/authorship [0.0-1.0] */
    float coherence;            /**< Identity coherence [0.0-1.0] */
    float continuity;           /**< Temporal continuity of self [0.0-1.0] */
    float emotional_state[4];   /**< Basic emotions: joy, sadness, fear, anger */
    uint64_t last_reflection;   /**< Timestamp of last reflection */
} self_model_state_t;

/**
 * @brief Internal introspection query tracking
 *
 * WHAT: Tracks pending and completed introspection queries
 * WHY: Enable asynchronous query/result pattern
 * HOW: Stores query metadata and result data
 */
typedef struct introspection_query {
    uint32_t query_id;          /**< Unique query identifier */
    uint32_t query_type;        /**< Type of introspective query */
    bool pending;               /**< Query is awaiting result */
    bool completed;             /**< Query has been completed */
    void* result_data;          /**< Pointer to result data */
    size_t result_size;         /**< Size of result data */
} introspection_query_t;

/**
 * @brief Full bridge structure definition
 *
 * WHAT: Complete Self-Introspection bridge state
 * WHY: Encapsulates all bridge data and synchronization
 * HOW: Contains config, state, queries array, stats, and mutex
 */
struct self_introspection_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    self_introspection_config_t config;     /**< Bridge configuration */
    self_model_state_t self_state;          /**< Internal self-model state */
    introspection_query_t* queries;         /**< Array of queries */
    size_t query_capacity;                  /**< Capacity of queries array */
    size_t query_count;                     /**< Current number of active queries */
    uint32_t next_query_id;                 /**< Next query ID to assign */
    self_introspection_stats_t stats;       /**< Bridge statistics */
    bool initialized;                       /**< Bridge initialization flag */
};

/* ============================================================================
 * Constants
 * ============================================================================ */

#define DEFAULT_QUERY_CAPACITY 32
#define DEFAULT_INTROSPECTION_DEPTH 3
#define DEFAULT_UPDATE_RATE 0.1f
#define DEFAULT_REFLECTION_THRESHOLD 0.5f
#define DEFAULT_MIN_UPDATE_CONFIDENCE 0.3f
#define DEFAULT_MAX_REFLECTION_DEPTH 5

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Find a query by ID (unlocked version)
 *
 * WHAT: Locate query in array by ID
 * WHY: Need to find queries for result processing
 * HOW: Linear search through queries array
 *
 * @param bridge Bridge instance (must hold lock)
 * @param query_id ID to search for
 * @return Pointer to query or NULL if not found
 */
static introspection_query_t* find_query_unlocked(
    self_introspection_bridge_t* bridge,
    uint32_t query_id
) {
    for (size_t i = 0; i < bridge->query_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->query_count > 256) {
            self_introspection_bridge_heartbeat("self_introsp_loop",
                             (float)(i + 1) / (float)bridge->query_count);
        }

        if (bridge->queries[i].query_id == query_id) {
            return &bridge->queries[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_query_unlocked: validation failed");
    return NULL;
}

/**
 * @brief Calculate stability based on self-state
 *
 * WHAT: Compute overall self-model stability
 * WHY: Stability indicates when reflection is needed
 * HOW: Weighted average of coherence, continuity, agency
 *
 * @param state Self-model state
 * @return Stability score [0.0-1.0]
 */
static float calculate_stability(const self_model_state_t* state) {
    return (state->coherence * 0.4f +
            state->continuity * 0.3f +
            state->agency * 0.3f);
}

/**
 * @brief Calculate confidence based on self-state
 *
 * WHAT: Compute self-knowledge confidence
 * WHY: Confidence guides update rates
 * HOW: Based on awareness and stability
 *
 * @param state Self-model state
 * @return Confidence score [0.0-1.0]
 */
static float calculate_confidence(const self_model_state_t* state) {
    float stability = calculate_stability(state);
    return (state->self_awareness * 0.5f + stability * 0.5f);
}

/**
 * @brief Determine suggested focus based on current state
 *
 * WHAT: Identify weakest aspect of self-model
 * WHY: Guide introspection to most needed areas
 * HOW: Find lowest state component
 *
 * @param state Self-model state
 * @return Suggested query type to focus on
 */
static self_introspection_query_type_t get_suggested_focus(
    const self_model_state_t* state
) {
    /* Find the weakest aspect */
    float min_val = state->self_awareness;
    self_introspection_query_type_t focus = SELF_INTROSPECTION_QUERY_STATE;

    if (state->agency < min_val) {
        min_val = state->agency;
        focus = SELF_INTROSPECTION_QUERY_INTENTION;
    }
    if (state->coherence < min_val) {
        min_val = state->coherence;
        focus = SELF_INTROSPECTION_QUERY_BELIEF;
    }

    /* Check emotional awareness */
    float emotional_avg = 0.0f;
    for (int i = 0; i < 4; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 4 > 256) {
            self_introspection_bridge_heartbeat("self_introsp_loop",
                             (float)(i + 1) / (float)4);
        }

        emotional_avg += state->emotional_state[i];
    }
    emotional_avg /= 4.0f;

    if (emotional_avg < min_val) {
        focus = SELF_INTROSPECTION_QUERY_EMOTION;
    }

    return focus;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int self_introspection_default_config(self_introspection_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_introspection_bridge_heartbeat("self_introsp_self_introspection_d", 0.0f);


    memset(config, 0, sizeof(self_introspection_config_t));

    config->introspection_depth = DEFAULT_INTROSPECTION_DEPTH;
    config->self_model_update_rate = DEFAULT_UPDATE_RATE;
    config->reflection_threshold = DEFAULT_REFLECTION_THRESHOLD;
    config->enable_auto_reflection = true;
    config->enable_guided_introspection = true;
    config->min_update_confidence = DEFAULT_MIN_UPDATE_CONFIDENCE;
    config->max_reflection_depth = DEFAULT_MAX_REFLECTION_DEPTH;

    return 0;
}

self_introspection_bridge_t* self_introspection_bridge_create(
    const self_introspection_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    self_introspection_bridge_heartbeat("self_introsp_create", 0.0f);


    self_introspection_bridge_t* bridge =
        (self_introspection_bridge_t*)nimcp_malloc(sizeof(self_introspection_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(self_introspection_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        self_introspection_default_config(&bridge->config);
    }

    /* Allocate queries array */
    bridge->query_capacity = DEFAULT_QUERY_CAPACITY;
    bridge->queries = (introspection_query_t*)nimcp_calloc(
        bridge->query_capacity, sizeof(introspection_query_t));
    if (!bridge->queries) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "self_introspection_bridge_create: bridge->queries is NULL");
        return NULL;
    }

    /* Initialize base bridge infrastructure (includes mutex) */
    if (bridge_base_init(&bridge->base, 0, "self_introspection") != 0) {
        nimcp_free(bridge->queries);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "self_introspection_bridge_create: bridge_base_init failed");
        return NULL;
    }

    /* Initialize self-model state with reasonable defaults */
    bridge->self_state.self_awareness = 0.5f;
    bridge->self_state.agency = 0.5f;
    bridge->self_state.coherence = 0.5f;
    bridge->self_state.continuity = 0.5f;
    for (int i = 0; i < 4; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 4 > 256) {
            self_introspection_bridge_heartbeat("self_introsp_loop",
                             (float)(i + 1) / (float)4);
        }

        bridge->self_state.emotional_state[i] = 0.0f;
    }
    bridge->self_state.last_reflection = 0;

    /* Initialize query tracking */
    bridge->query_count = 0;
    bridge->next_query_id = 1;

    /* Clear statistics */
    memset(&bridge->stats, 0, sizeof(self_introspection_stats_t));

    bridge->initialized = true;

    return bridge;
}

void self_introspection_bridge_destroy(self_introspection_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Free any result data in queries */
    /* Phase 8: Heartbeat at operation start */
    self_introspection_bridge_heartbeat("self_introsp_destroy", 0.0f);


    if (bridge->queries) {
        for (size_t i = 0; i < bridge->query_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->query_count > 256) {
                self_introspection_bridge_heartbeat("self_introsp_loop",
                                 (float)(i + 1) / (float)bridge->query_count);
            }

            if (bridge->queries[i].result_data) {
                nimcp_free(bridge->queries[i].result_data);
            }
        }
        nimcp_free(bridge->queries);
    }

    /* Cleanup base bridge infrastructure (includes mutex) */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
        bridge->base.mutex = NULL;
    }

    bridge->initialized = false;

    nimcp_free(bridge);
    bridge = NULL;
}

/* ============================================================================
 * Core API - Self-Model -> Introspection
 * ============================================================================ */

int self_introspection_guide_query(
    self_introspection_bridge_t* bridge,
    self_introspection_query_type_t query_type,
    self_introspection_guidance_t* guidance_out
) {
    if (!bridge || !guidance_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_introspection_guide_query: required parameter is NULL (bridge, guidance_out)");
        return -1;
    }
    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_introspection_guide_query: bridge->initialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_introspection_bridge_heartbeat("self_introsp_self_introspection_g", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Determine suggested focus based on current state */
    guidance_out->suggested_focus = bridge->config.enable_guided_introspection
        ? get_suggested_focus(&bridge->self_state)
        : query_type;

    /* Calculate expected value based on query type */
    switch (query_type) {
        case SELF_INTROSPECTION_QUERY_STATE:
            guidance_out->expected_value = bridge->self_state.self_awareness;
            break;
        case SELF_INTROSPECTION_QUERY_CAPABILITY:
            guidance_out->expected_value = bridge->self_state.agency;
            break;
        case SELF_INTROSPECTION_QUERY_CONFIDENCE:
            guidance_out->expected_value = calculate_confidence(&bridge->self_state);
            break;
        case SELF_INTROSPECTION_QUERY_EMOTION:
            /* Average emotional state */
            guidance_out->expected_value = 0.0f;
            for (int i = 0; i < 4; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && 4 > 256) {
                    self_introspection_bridge_heartbeat("self_introsp_loop",
                                     (float)(i + 1) / (float)4);
                }

                guidance_out->expected_value += bridge->self_state.emotional_state[i];
            }
            guidance_out->expected_value /= 4.0f;
            break;
        case SELF_INTROSPECTION_QUERY_BELIEF:
            guidance_out->expected_value = bridge->self_state.coherence;
            break;
        case SELF_INTROSPECTION_QUERY_INTENTION:
            guidance_out->expected_value = bridge->self_state.agency;
            break;
        case SELF_INTROSPECTION_QUERY_KNOWLEDGE:
        case SELF_INTROSPECTION_QUERY_MEMORY:
        default:
            guidance_out->expected_value = bridge->self_state.self_awareness;
            break;
    }

    /* Calculate confidence in expectation */
    guidance_out->expectation_confidence = calculate_confidence(&bridge->self_state);

    /* Calculate self-model stability */
    guidance_out->self_model_stability = calculate_stability(&bridge->self_state);

    /* Priority based on how much this area needs attention */
    float state_value = guidance_out->expected_value;
    guidance_out->priority = 1.0f - state_value; /* Lower values = higher priority */

    /* Assign context ID */
    guidance_out->context_id = bridge->next_query_id;

    /* Update statistics */
    bridge->stats.queries_guided++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Core API - Introspection -> Self-Model
 * ============================================================================ */

int self_introspection_on_result(
    self_introspection_bridge_t* bridge,
    uint32_t query_id,
    const self_introspection_result_t* result_data
) {
    if (!bridge || !result_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_introspection_on_result: required parameter is NULL (bridge, result_data)");
        return -1;
    }
    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_introspection_on_result: bridge->initialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_introspection_bridge_heartbeat("self_introsp_self_introspection_o", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Find the query */
    introspection_query_t* query = find_query_unlocked(bridge, query_id);

    /* Update self-state based on result */
    float update_rate = bridge->config.self_model_update_rate;

    /* Only update if confidence is above threshold */
    if (result_data->confidence >= bridge->config.min_update_confidence) {
        switch (result_data->query_type) {
            case SELF_INTROSPECTION_QUERY_STATE:
                bridge->self_state.self_awareness =
                    bridge->self_state.self_awareness * (1.0f - update_rate) +
                    result_data->result_value * update_rate;
                break;

            case SELF_INTROSPECTION_QUERY_CAPABILITY:
            case SELF_INTROSPECTION_QUERY_INTENTION:
                bridge->self_state.agency =
                    bridge->self_state.agency * (1.0f - update_rate) +
                    result_data->result_value * update_rate;
                break;

            case SELF_INTROSPECTION_QUERY_CONFIDENCE:
                /* Confidence affects coherence */
                bridge->self_state.coherence =
                    bridge->self_state.coherence * (1.0f - update_rate) +
                    result_data->result_value * update_rate;
                break;

            case SELF_INTROSPECTION_QUERY_EMOTION:
                /* Update emotional state (simplified - uniform distribution) */
                for (int i = 0; i < 4; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && 4 > 256) {
                        self_introspection_bridge_heartbeat("self_introsp_loop",
                                         (float)(i + 1) / (float)4);
                    }

                    bridge->self_state.emotional_state[i] =
                        bridge->self_state.emotional_state[i] * (1.0f - update_rate) +
                        (result_data->result_value / 4.0f) * update_rate;
                }
                break;

            case SELF_INTROSPECTION_QUERY_BELIEF:
                bridge->self_state.coherence =
                    bridge->self_state.coherence * (1.0f - update_rate) +
                    result_data->result_value * update_rate;
                break;

            case SELF_INTROSPECTION_QUERY_KNOWLEDGE:
            case SELF_INTROSPECTION_QUERY_MEMORY:
            default:
                /* General awareness update */
                bridge->self_state.self_awareness =
                    bridge->self_state.self_awareness * (1.0f - update_rate) +
                    result_data->result_value * update_rate;
                break;
        }

        bridge->stats.self_model_updates++;
    }

    /* Track discrepancy */
    if (result_data->discrepancy > 0.0f) {
        bridge->stats.discrepancies_detected++;

        /* Update average discrepancy (exponential moving average) */
        bridge->stats.avg_discrepancy =
            bridge->stats.avg_discrepancy * 0.9f +
            result_data->discrepancy * 0.1f;
    }

    /* Update average confidence */
    bridge->stats.avg_introspection_confidence =
        bridge->stats.avg_introspection_confidence * 0.9f +
        result_data->confidence * 0.1f;

    /* Mark query as completed if found */
    if (query) {
        query->completed = true;
        query->pending = false;
    }

    /* Update statistics */
    bridge->stats.results_integrated++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Core API - Reflection Triggering
 * ============================================================================ */

int self_introspection_trigger_reflection(
    self_introspection_bridge_t* bridge,
    self_introspection_trigger_type_t trigger_type
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }
    if (!bridge->initialized) {
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_introspection_bridge_heartbeat("self_introsp_self_introspection_t", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    int result = 0;
    bool should_trigger = false;

    /* Determine if reflection is needed based on trigger type */
    switch (trigger_type) {
        case SELF_INTROSPECTION_TRIGGER_DISCREPANCY:
            /* Trigger if discrepancy rate is high */
            should_trigger = (bridge->stats.avg_discrepancy > 0.3f);
            break;

        case SELF_INTROSPECTION_TRIGGER_UNCERTAINTY:
            /* Trigger if confidence is low */
            should_trigger = (calculate_confidence(&bridge->self_state) <
                             bridge->config.reflection_threshold);
            break;

        case SELF_INTROSPECTION_TRIGGER_ERROR:
            /* Always trigger on error */
            should_trigger = true;
            break;

        case SELF_INTROSPECTION_TRIGGER_NOVELTY:
            /* Trigger if self-model is stable (can handle novelty) */
            should_trigger = (calculate_stability(&bridge->self_state) >
                             bridge->config.reflection_threshold);
            break;

        case SELF_INTROSPECTION_TRIGGER_SCHEDULED:
        case SELF_INTROSPECTION_TRIGGER_EXTERNAL:
            /* Always trigger for scheduled or external requests */
            should_trigger = true;
            break;

        default:
            should_trigger = false;
            break;
    }

    /* Also check coherence threshold */
    if (bridge->self_state.coherence < bridge->config.reflection_threshold) {
        should_trigger = true;
    }

    if (should_trigger) {
        /* Check capacity */
        if (bridge->query_count >= bridge->query_capacity) {
            /* Cannot add more queries - reflection fails */
            bridge->stats.reflection_failures++;
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "self_introspection_trigger_reflection: capacity exceeded");
            return -1;
        }

        /* Create new introspection query */
        introspection_query_t* new_query = &bridge->queries[bridge->query_count];
        new_query->query_id = bridge->next_query_id++;
        new_query->query_type = (uint32_t)get_suggested_focus(&bridge->self_state);
        new_query->pending = true;
        new_query->completed = false;
        new_query->result_data = NULL;
        new_query->result_size = 0;

        bridge->query_count++;
        bridge->stats.reflections_triggered++;

        /* Update last reflection time (using query ID as proxy for time) */
        bridge->self_state.last_reflection = bridge->next_query_id;

        result = (int)new_query->query_id;
    } else {
        /* No reflection needed */
        result = 0;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

/* ============================================================================
 * State Query API
 * ============================================================================ */

int self_introspection_get_self_state(
    self_introspection_bridge_t* bridge,
    self_introspection_self_state_t* state_out
) {
    if (!bridge || !state_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_introspection_get_self_state: required parameter is NULL (bridge, state_out)");
        return -1;
    }
    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_introspection_get_self_state: bridge->initialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_introspection_bridge_heartbeat("self_introsp_self_introspection_g", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Map internal state to output structure */
    state_out->coherence = bridge->self_state.coherence;
    state_out->stability = calculate_stability(&bridge->self_state);
    state_out->confidence = calculate_confidence(&bridge->self_state);

    /* Count beliefs (simplified - based on initialized state aspects) */
    state_out->belief_count = 0;
    if (bridge->self_state.self_awareness > 0.0f) state_out->belief_count++;
    if (bridge->self_state.agency > 0.0f) state_out->belief_count++;
    if (bridge->self_state.coherence > 0.0f) state_out->belief_count++;
    if (bridge->self_state.continuity > 0.0f) state_out->belief_count++;

    /* Count pending queries */
    state_out->pending_updates = 0;
    for (size_t i = 0; i < bridge->query_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->query_count > 256) {
            self_introspection_bridge_heartbeat("self_introsp_loop",
                             (float)(i + 1) / (float)bridge->query_count);
        }

        if (bridge->queries[i].pending) {
            state_out->pending_updates++;
        }
    }

    /* Time since revision (using query ID difference as proxy) */
    state_out->time_since_revision =
        bridge->next_query_id - (uint32_t)bridge->self_state.last_reflection;

    /* Stability check */
    state_out->is_stable = (state_out->stability > bridge->config.reflection_threshold);

    /* Check if reflection is active (any pending queries) */
    state_out->reflection_active = (state_out->pending_updates > 0);

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int self_introspection_get_stats(
    const self_introspection_bridge_t* bridge,
    self_introspection_stats_t* stats_out
) {
    if (!bridge || !stats_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_introspection_get_stats: required parameter is NULL (bridge, stats_out)");
        return -1;
    }
    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_introspection_get_stats: bridge->initialized is NULL");
        return -1;
    }

    /* Cast away const for mutex lock (safe - read-only operation) */
    /* Phase 8: Heartbeat at operation start */
    self_introspection_bridge_heartbeat("self_introsp_self_introspection_g", 0.0f);


    self_introspection_bridge_t* mutable_bridge =
        (self_introspection_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);

    *stats_out = bridge->stats;

    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void self_introspection_bridge_set_instance_health_agent(self_introspection_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "self_introspection_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int self_introspection_bridge_training_begin(self_introspection_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_introspection_bridge_training_begin: NULL argument");
        return -1;
    }
    self_introspection_bridge_heartbeat_instance(bridge->health_agent, "self_introspection_bridge_training_begin", 0.0f);
    return 0;
}

int self_introspection_bridge_training_end(self_introspection_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_introspection_bridge_training_end: NULL argument");
        return -1;
    }
    self_introspection_bridge_heartbeat_instance(bridge->health_agent, "self_introspection_bridge_training_end", 1.0f);
    return 0;
}

int self_introspection_bridge_training_step(self_introspection_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_introspection_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    self_introspection_bridge_heartbeat_instance(bridge->health_agent, "self_introspection_bridge_training_step", progress);
    return 0;
}
