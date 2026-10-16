/**
 * @file nimcp_omni_wm_tom_bridge.c
 * @brief World Model Theory of Mind Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-17
 *
 * WHAT: Bidirectional bridge connecting World Model (RSSM) with Theory of Mind systems
 * WHY:  Enable social world modeling by integrating mental state prediction with physical world prediction
 * HOW:  ToM informs WM about agent mental states; WM provides counterfactual simulations for ToM reasoning
 *
 * IMPLEMENTATION NOTES:
 * =====================
 * This implementation integrates several key concepts:
 *
 * 1. BELIEF-DESIRE-INTENTION (BDI) MODEL:
 *    - Mental states as vectors in world model state space
 *    - RSSM dynamics for mental state evolution
 *
 * 2. FALSE BELIEF REASONING:
 *    - Track divergence between reality (WM) and beliefs (ToM)
 *    - Detect and reason about false beliefs
 *
 * 3. COUNTERFACTUAL SOCIAL REASONING:
 *    - "What if they believed X?" simulations
 *    - Trajectory prediction under alternative beliefs
 *
 * 4. MIRROR NEURON INTEGRATION:
 *    - Action understanding for intention prediction
 *    - Empathetic perspective-taking
 */

#include "cognitive/omni/bridges/nimcp_omni_wm_tom_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>

/* ============================================================================
 * Module-level Constants
 * ============================================================================ */

#define LOG_MODULE "wm_tom_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(omni_wm_tom_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_omni_wm_tom_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_omni_wm_tom_bridge_mesh_registry = NULL;

nimcp_error_t omni_wm_tom_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_omni_wm_tom_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "omni_wm_tom_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "omni_wm_tom_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_omni_wm_tom_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_omni_wm_tom_bridge_mesh_registry = registry;
    return err;
}

void omni_wm_tom_bridge_mesh_unregister(void) {
    if (g_omni_wm_tom_bridge_mesh_registry && g_omni_wm_tom_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_omni_wm_tom_bridge_mesh_registry, g_omni_wm_tom_bridge_mesh_id);
        g_omni_wm_tom_bridge_mesh_id = 0;
        g_omni_wm_tom_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from omni_wm_tom_bridge module (instance-level) */
static inline void omni_wm_tom_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_omni_wm_tom_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_omni_wm_tom_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_omni_wm_tom_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

void omni_wm_tom_bridge_set_instance_health_agent(
    omni_wm_tom_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "omni_wm_tom_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int omni_wm_tom_bridge_training_begin(omni_wm_tom_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_tom_bridge_training_begin: NULL argument");
        return -1;
    }
    omni_wm_tom_bridge_heartbeat_instance(g_omni_wm_tom_bridge_health_agent, "training_begin", 0.0f);
    (void)bridge;
    return 0;
}

int omni_wm_tom_bridge_training_step(omni_wm_tom_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_tom_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    omni_wm_tom_bridge_heartbeat_instance(g_omni_wm_tom_bridge_health_agent, "training_step", progress);
    (void)bridge;
    return 0;
}

int omni_wm_tom_bridge_training_end(omni_wm_tom_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_tom_bridge_training_end: NULL argument");
        return -1;
    }
    omni_wm_tom_bridge_heartbeat_instance(g_omni_wm_tom_bridge_health_agent, "training_end", 1.0f);
    (void)bridge;
    return 0;
}

/** Default tracked agent capacity */
#define DEFAULT_TRACKED_AGENT_CAPACITY 16

/** Default belief gap capacity */
#define DEFAULT_BELIEF_GAP_CAPACITY 16

/** Default interaction buffer capacity */
#define DEFAULT_INTERACTION_BUFFER_CAPACITY 32

/** Default counterfactual cache capacity */
#define DEFAULT_CF_CACHE_CAPACITY 8

/** Maximum state dimension for internal buffers */
#define MAX_STATE_DIM 512

#include "constants/nimcp_constants.h"
/** Epsilon for floating point comparisons */
#define FLOAT_EPSILON NIMCP_EPSILON_NUMERICAL

/* ============================================================================
 * Internal Helper Forward Declarations
 * ============================================================================ */

static nimcp_error_t allocate_agent_tracking(omni_wm_tom_bridge_t* bridge);
static void free_agent_tracking(omni_wm_tom_bridge_t* bridge);
static nimcp_error_t allocate_belief_gap_tracking(omni_wm_tom_bridge_t* bridge);
static void free_belief_gap_tracking(omni_wm_tom_bridge_t* bridge);
static nimcp_error_t allocate_interaction_buffer(omni_wm_tom_bridge_t* bridge);
static void free_interaction_buffer(omni_wm_tom_bridge_t* bridge);
static nimcp_error_t allocate_cf_cache(omni_wm_tom_bridge_t* bridge);
static void free_cf_cache(omni_wm_tom_bridge_t* bridge);
static nimcp_error_t update_wm_to_tom_effects(omni_wm_tom_bridge_t* bridge);
static nimcp_error_t update_tom_to_wm_effects(omni_wm_tom_bridge_t* bridge);
static nimcp_error_t update_belief_reality_gaps(omni_wm_tom_bridge_t* bridge);
static int32_t find_tracked_agent(const omni_wm_tom_bridge_t* bridge, agent_id_t agent_id);
static int32_t find_belief_gap(const omni_wm_tom_bridge_t* bridge, agent_id_t agent_id);
static float compute_state_divergence(const float* state1, const float* state2, uint32_t dim);
static uint64_t get_current_time_us(void);

/* Bio-async handlers */
static nimcp_error_t handle_mental_state_pred(const void* msg, size_t msg_size,
                                               nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_trajectory_pred(const void* msg, size_t msg_size,
                                             nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_counterfactual_req(const void* msg, size_t msg_size,
                                                nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_false_belief_detect(const void* msg, size_t msg_size,
                                                 nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_social_interaction(const void* msg, size_t msg_size,
                                                nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_empathy_simulation(const void* msg, size_t msg_size,
                                                nimcp_bio_promise_t promise, void* user_data);

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_current_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

/**
 * @brief Allocate agent tracking array
 */
static nimcp_error_t allocate_agent_tracking(omni_wm_tom_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    uint32_t capacity = bridge->config.max_tracked_agents;
    if (capacity == 0) capacity = DEFAULT_TRACKED_AGENT_CAPACITY;

    bridge->tracked_agents = nimcp_calloc(capacity, sizeof(tom_agent_mental_state_t));
    if (!bridge->tracked_agents) return -1;
    NIMCP_CHECK_THROW(bridge->tracked_agents, NIMCP_ERROR_NO_MEMORY, "failed to allocate tracked_agents");

    bridge->tracked_agent_capacity = capacity;
    bridge->tracked_agent_count = 0;

    return NIMCP_SUCCESS;
}

/**
 * @brief Free agent tracking array
 */
static void free_agent_tracking(omni_wm_tom_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_free(bridge->tracked_agents);
    bridge->tracked_agents = NULL;
    bridge->tracked_agent_count = 0;
    bridge->tracked_agent_capacity = 0;
}

/**
 * @brief Allocate belief-reality gap tracking array
 */
static nimcp_error_t allocate_belief_gap_tracking(omni_wm_tom_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    uint32_t capacity = bridge->config.max_tracked_agents;
    if (capacity == 0) capacity = DEFAULT_BELIEF_GAP_CAPACITY;

    bridge->belief_gaps = nimcp_calloc(capacity, sizeof(tom_belief_reality_gap_t));
    if (!bridge->belief_gaps) return -1;
    NIMCP_CHECK_THROW(bridge->belief_gaps, NIMCP_ERROR_NO_MEMORY, "failed to allocate belief_gaps");

    bridge->belief_gap_capacity = capacity;
    bridge->belief_gap_count = 0;

    return NIMCP_SUCCESS;
}

/**
 * @brief Free belief gap tracking array
 */
static void free_belief_gap_tracking(omni_wm_tom_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_free(bridge->belief_gaps);
    bridge->belief_gaps = NULL;
    bridge->belief_gap_count = 0;
    bridge->belief_gap_capacity = 0;
}

/**
 * @brief Allocate interaction buffer
 */
static nimcp_error_t allocate_interaction_buffer(omni_wm_tom_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    uint32_t capacity = bridge->config.interaction_buffer_size;
    if (capacity == 0) capacity = DEFAULT_INTERACTION_BUFFER_CAPACITY;

    bridge->interaction_buffer = nimcp_calloc(capacity, sizeof(tom_social_interaction_t));
    if (!bridge->interaction_buffer) return -1;
    NIMCP_CHECK_THROW(bridge->interaction_buffer, NIMCP_ERROR_NO_MEMORY, "failed to allocate interaction_buffer");

    bridge->interaction_buffer_capacity = capacity;
    bridge->interaction_buffer_head = 0;
    bridge->interaction_buffer_tail = 0;

    return NIMCP_SUCCESS;
}

/**
 * @brief Free interaction buffer
 */
static void free_interaction_buffer(omni_wm_tom_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_free(bridge->interaction_buffer);
    bridge->interaction_buffer = NULL;
    bridge->interaction_buffer_head = 0;
    bridge->interaction_buffer_tail = 0;
    bridge->interaction_buffer_capacity = 0;
}

/**
 * @brief Allocate counterfactual cache
 */
static nimcp_error_t allocate_cf_cache(omni_wm_tom_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    uint32_t capacity = bridge->config.max_counterfactuals;
    if (capacity == 0) capacity = DEFAULT_CF_CACHE_CAPACITY;

    bridge->cf_cache = nimcp_calloc(capacity, sizeof(tom_social_trajectory_t));
    if (!bridge->cf_cache) return -1;
    NIMCP_CHECK_THROW(bridge->cf_cache, NIMCP_ERROR_NO_MEMORY, "failed to allocate cf_cache");

    bridge->cf_cache_capacity = capacity;
    bridge->cf_cache_size = 0;

    return NIMCP_SUCCESS;
}

/**
 * @brief Free counterfactual cache
 */
static void free_cf_cache(omni_wm_tom_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_free(bridge->cf_cache);
    bridge->cf_cache = NULL;
    bridge->cf_cache_size = 0;
    bridge->cf_cache_capacity = 0;
}

/**
 * @brief Find tracked agent by ID
 * @return Index if found, -1 if not found
 */
static int32_t find_tracked_agent(const omni_wm_tom_bridge_t* bridge, agent_id_t agent_id) {
    if (!bridge || !bridge->tracked_agents) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_tracked_agent: required parameter is NULL (bridge, bridge->tracked_agents)");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->tracked_agent_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->tracked_agent_count > 256) {
            omni_wm_tom_bridge_heartbeat("omni_wm_tom__loop",
                             (float)(i + 1) / (float)bridge->tracked_agent_count);
        }

        if (bridge->tracked_agents[i].agent_id == agent_id) {
            return (int32_t)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_tracked_agent: validation failed");
    return -1;
}

/**
 * @brief Find belief gap by agent ID
 * @return Index if found, -1 if not found
 */
static int32_t find_belief_gap(const omni_wm_tom_bridge_t* bridge, agent_id_t agent_id) {
    if (!bridge || !bridge->belief_gaps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_belief_gap: required parameter is NULL (bridge, bridge->belief_gaps)");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->belief_gap_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->belief_gap_count > 256) {
            omni_wm_tom_bridge_heartbeat("omni_wm_tom__loop",
                             (float)(i + 1) / (float)bridge->belief_gap_count);
        }

        if (bridge->belief_gaps[i].agent_id == agent_id) {
            return (int32_t)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_belief_gap: validation failed");
    return -1;
}

/**
 * @brief Compute divergence between two state vectors
 */
static float compute_state_divergence(const float* state1, const float* state2, uint32_t dim) {
    if (!state1 || !state2 || dim == 0) return 0.0f;

    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            omni_wm_tom_bridge_heartbeat("omni_wm_tom__loop",
                             (float)(i + 1) / (float)dim);
        }

        float diff = state1[i] - state2[i];
        sum_sq += diff * diff;
    }
    return sqrtf(sum_sq / (float)dim);
}

/**
 * @brief Update effects flowing from WM to ToM
 */
static nimcp_error_t update_wm_to_tom_effects(omni_wm_tom_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    omni_wm_to_tom_effects_t* effects = &bridge->wm_to_tom;

    /* If world model connected, extract prediction information */
    if (bridge->world_model) {
        /* Placeholder: would extract actual WM predictions */
        effects->prediction_confidence = 0.8f;
        effects->action_outcome_confidence = 0.75f;
        effects->predicted_reward = 0.0f;
        effects->predicted_emotional_impact = 0.0f;
        effects->perspective_confidence = 0.7f;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Update effects flowing from ToM to WM
 */
static nimcp_error_t update_tom_to_wm_effects(omni_wm_tom_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    tom_to_omni_wm_effects_t* effects = &bridge->tom_to_wm;

    /* Update agent count */
    effects->agent_count = bridge->tracked_agent_count;
    effects->agent_states = bridge->tracked_agents;

    /* Update belief gap count */
    effects->gap_count = bridge->belief_gap_count;
    effects->belief_gaps = bridge->belief_gaps;

    /* Update social context */
    effects->is_cooperative_context = bridge->cooperative_mode;
    effects->is_competitive_context = bridge->competitive_mode;

    /* If ToM connected, query for observations */
    if (bridge->tom) {
        /* Placeholder: would query ToM for observed actions */
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Update belief-reality gaps for all tracked agents
 */
static nimcp_error_t update_belief_reality_gaps(omni_wm_tom_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_false_belief_detection) return NIMCP_SUCCESS;

    for (uint32_t i = 0; i < bridge->tracked_agent_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->tracked_agent_count > 256) {
            omni_wm_tom_bridge_heartbeat("omni_wm_tom__loop",
                             (float)(i + 1) / (float)bridge->tracked_agent_count);
        }

        tom_agent_mental_state_t* agent = &bridge->tracked_agents[i];

        /* Find or create belief gap entry */
        int32_t gap_idx = find_belief_gap(bridge, agent->agent_id);
        if (gap_idx < 0) {
            /* Create new entry */
            if (bridge->belief_gap_count < bridge->belief_gap_capacity) {
                gap_idx = (int32_t)bridge->belief_gap_count;
                bridge->belief_gap_count++;
                bridge->belief_gaps[gap_idx].agent_id = agent->agent_id;
            } else {
                continue; /* No capacity */
            }
        }

        tom_belief_reality_gap_t* gap = &bridge->belief_gaps[gap_idx];

        /* Copy agent's belief state */
        memcpy(gap->believed_state, agent->belief_state, OMNI_WM_STATE_DIM * sizeof(float));

        /* Get reality state from WM (placeholder: using zeros) */
        if (bridge->world_model) {
            /* Would extract actual WM state */
            memset(gap->reality_state, 0, OMNI_WM_STATE_DIM * sizeof(float));
        }

        /* Compute divergence */
        gap->divergence_score = compute_state_divergence(
            gap->reality_state, gap->believed_state, OMNI_WM_STATE_DIM);

        /* Check for false belief */
        gap->has_false_belief = (gap->divergence_score > bridge->config.false_belief_threshold);

        /* Count divergent dimensions */
        gap->false_belief_dimensions = 0;
        gap->max_dimension_divergence = 0.0f;
        for (uint32_t d = 0; d < OMNI_WM_STATE_DIM; d++) {
            /* Phase 8: Loop progress heartbeat */
            if ((d & 0xFF) == 0 && OMNI_WM_STATE_DIM > 256) {
                omni_wm_tom_bridge_heartbeat("omni_wm_tom__loop",
                                 (float)(d + 1) / (float)OMNI_WM_STATE_DIM);
            }

            float diff = fabsf(gap->reality_state[d] - gap->believed_state[d]);
            if (diff > 0.1f) {
                gap->false_belief_dimensions++;
            }
            if (diff > gap->max_dimension_divergence) {
                gap->max_dimension_divergence = diff;
            }
        }

        gap->last_update_us = get_current_time_us();

        /* Update statistics */
        if (gap->has_false_belief) {
            bridge->stats.false_beliefs_detected++;
        }
        bridge->stats.belief_reality_gaps_tracked++;
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Message Handlers
 * ============================================================================ */

/**
 * @brief Handle mental state prediction request
 */
static nimcp_error_t handle_mental_state_pred(const void* msg, size_t msg_size,
                                               nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_tom_bridge_t* bridge = (omni_wm_tom_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.mental_state_predictions++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle trajectory prediction request
 */
static nimcp_error_t handle_trajectory_pred(const void* msg, size_t msg_size,
                                             nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_tom_bridge_t* bridge = (omni_wm_tom_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.trajectory_predictions++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle counterfactual request
 */
static nimcp_error_t handle_counterfactual_req(const void* msg, size_t msg_size,
                                                nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_tom_bridge_t* bridge = (omni_wm_tom_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.counterfactual_queries++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle false belief detection request
 */
static nimcp_error_t handle_false_belief_detect(const void* msg, size_t msg_size,
                                                 nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_tom_bridge_t* bridge = (omni_wm_tom_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    update_belief_reality_gaps(bridge);
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle social interaction message
 */
static nimcp_error_t handle_social_interaction(const void* msg, size_t msg_size,
                                                nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_tom_bridge_t* bridge = (omni_wm_tom_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.interactions_processed++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle empathy simulation request
 */
static nimcp_error_t handle_empathy_simulation(const void* msg, size_t msg_size,
                                                nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_tom_bridge_t* bridge = (omni_wm_tom_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.empathy_simulations++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_tom_bridge_default_config(
    omni_wm_tom_bridge_config_t* config) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__default_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(omni_wm_tom_bridge_config_t));

    /* General settings */
    config->enable_modulation = true;
    config->sensitivity = 1.0f;

    /* Mental state prediction settings */
    config->enable_mental_state_prediction = true;
    config->default_prediction_horizon = 10;
    config->mental_state_learning_rate = 0.001f;
    config->belief_update_rate = 0.1f;

    /* False belief detection settings */
    config->enable_false_belief_detection = true;
    config->false_belief_threshold = WM_TOM_DEFAULT_FALSE_BELIEF_THRESHOLD;
    config->false_belief_persistence = 0.9f;

    /* Counterfactual settings */
    config->enable_counterfactual_reasoning = true;
    config->max_counterfactuals = WM_TOM_MAX_COUNTERFACTUALS;
    config->counterfactual_noise = 0.1f;

    /* Social training settings */
    config->enable_social_training = true;
    config->social_training_learning_rate = 0.0005f;
    config->interaction_buffer_size = DEFAULT_INTERACTION_BUFFER_CAPACITY;
    config->interaction_priority_decay = 0.95f;

    /* Multi-agent settings */
    config->max_tracked_agents = DEFAULT_TRACKED_AGENT_CAPACITY;
    config->agent_attention_decay = 0.05f;
    config->enable_joint_prediction = true;

    /* Mirror neuron integration settings */
    config->enable_mirror_integration = true;
    config->mirror_action_threshold = 0.5f;

    /* Empathy settings */
    config->enable_empathy_simulation = true;
    config->empathy_strength = 1.0f;

    /* Bio-async settings */
    config->enable_bio_async = true;

    return NIMCP_SUCCESS;
}

omni_wm_tom_bridge_t* omni_wm_tom_bridge_create(
    const omni_wm_tom_bridge_config_t* config) {

    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__create", 0.0f);


    omni_wm_tom_bridge_t* bridge = nimcp_calloc(1, sizeof(omni_wm_tom_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate WM ToM bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: bridge is NULL");
        return NULL;
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_WM_TOM_BRIDGE,
                         "wm_tom_bridge") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to initialize bridge base");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: operation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        omni_wm_tom_bridge_default_config(&bridge->config);
    }

    /* Allocate agent tracking */
    nimcp_error_t err = allocate_agent_tracking(bridge);
    if (err != NIMCP_SUCCESS) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to allocate agent tracking");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: validation failed");
        return NULL;
    }

    /* Allocate belief gap tracking */
    err = allocate_belief_gap_tracking(bridge);
    if (err != NIMCP_SUCCESS) {
        free_agent_tracking(bridge);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to allocate belief gap tracking");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: validation failed");
        return NULL;
    }

    /* Allocate interaction buffer */
    err = allocate_interaction_buffer(bridge);
    if (err != NIMCP_SUCCESS) {
        free_belief_gap_tracking(bridge);
        free_agent_tracking(bridge);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to allocate interaction buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: validation failed");
        return NULL;
    }

    /* Allocate counterfactual cache */
    err = allocate_cf_cache(bridge);
    if (err != NIMCP_SUCCESS) {
        free_interaction_buffer(bridge);
        free_belief_gap_tracking(bridge);
        free_agent_tracking(bridge);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to allocate counterfactual cache");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: validation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->current_social_context = 0.0f;
    bridge->cooperative_mode = false;
    bridge->competitive_mode = false;
    bridge->focus_agent = 0;

    NIMCP_LOGGING_INFO("WM ToM Bridge created successfully");
    return bridge;
}

void omni_wm_tom_bridge_destroy(omni_wm_tom_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        omni_wm_tom_bridge_disconnect_bio_async(bridge);
    }

    /* Free WM to ToM effects dynamic arrays */
    nimcp_free(bridge->wm_to_tom.counterfactual_trajectories);

    /* Free ToM to WM effects dynamic arrays */
    /* Note: agent_states and belief_gaps point to bridge->tracked_agents
     * and bridge->belief_gaps, so don't free them here */
    nimcp_free(bridge->tom_to_wm.interactions);

    /* Free internal buffers */
    free_cf_cache(bridge);
    free_interaction_buffer(bridge);
    free_belief_gap_tracking(bridge);
    free_agent_tracking(bridge);

    /* Cleanup base and free */
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;

    NIMCP_LOGGING_INFO("WM ToM Bridge destroyed");
}

nimcp_error_t omni_wm_tom_bridge_reset(omni_wm_tom_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__reset", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset effects */
    memset(&bridge->wm_to_tom, 0, sizeof(omni_wm_to_tom_effects_t));
    memset(&bridge->tom_to_wm, 0, sizeof(tom_to_omni_wm_effects_t));

    /* Clear tracked agents */
    bridge->tracked_agent_count = 0;
    memset(bridge->tracked_agents, 0,
           bridge->tracked_agent_capacity * sizeof(tom_agent_mental_state_t));

    /* Clear belief gaps */
    bridge->belief_gap_count = 0;
    memset(bridge->belief_gaps, 0,
           bridge->belief_gap_capacity * sizeof(tom_belief_reality_gap_t));

    /* Clear interaction buffer */
    bridge->interaction_buffer_head = 0;
    bridge->interaction_buffer_tail = 0;
    memset(bridge->interaction_buffer, 0,
           bridge->interaction_buffer_capacity * sizeof(tom_social_interaction_t));

    /* Clear CF cache */
    bridge->cf_cache_size = 0;
    memset(bridge->cf_cache, 0,
           bridge->cf_cache_capacity * sizeof(tom_social_trajectory_t));

    /* Reset internal state */
    bridge->current_social_context = 0.0f;
    bridge->cooperative_mode = false;
    bridge->competitive_mode = false;
    bridge->focus_agent = 0;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(omni_wm_tom_bridge_stats_t));

    /* Reset base bridge (unlocked since we already hold the mutex) */
    bridge_base_reset_unlocked(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_tom_bridge_connect(
    omni_wm_tom_bridge_t* bridge,
    omni_world_model_t* world_model,
    theory_of_mind_t tom,
    mirror_neurons_t mirror) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__connect", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(world_model, NIMCP_ERROR_INVALID_PARAM, "world_model is NULL");
    NIMCP_CHECK_THROW(tom, NIMCP_ERROR_INVALID_PARAM, "tom is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->world_model = world_model;
    bridge->tom = tom;
    bridge->mirror = mirror;

    /* Update base connection state */
    bridge->base.system_a = world_model;
    bridge->base.system_a_connected = true;
    bridge->base.system_b = tom;
    bridge->base.system_b_connected = true;
    bridge->base.bridge_active = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("WM ToM Bridge connected: WM=%p, ToM=%p, Mirror=%p",
                       (void*)world_model, (void*)tom, (void*)mirror);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_tom_bridge_connect_world_model(
    omni_wm_tom_bridge_t* bridge,
    omni_world_model_t* world_model) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__connect_world_model", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(world_model, NIMCP_ERROR_NULL_POINTER, "world_model is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->world_model = world_model;
    bridge->base.system_a = world_model;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = (bridge->tom != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_tom_bridge_connect_tom(
    omni_wm_tom_bridge_t* bridge,
    theory_of_mind_t tom) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__connect_tom", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(tom, NIMCP_ERROR_NULL_POINTER, "tom is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->tom = tom;
    bridge->base.system_b = tom;
    bridge->base.system_b_connected = true;
    bridge->base.bridge_active = (bridge->world_model != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_tom_bridge_connect_mirror(
    omni_wm_tom_bridge_t* bridge,
    mirror_neurons_t mirror) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__connect_mirror", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(mirror, NIMCP_ERROR_NULL_POINTER, "mirror is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->mirror = mirror;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_tom_bridge_connect_social(
    omni_wm_tom_bridge_t* bridge,
    tom_social_bridge_t* social_bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__connect_social", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(social_bridge, NIMCP_ERROR_NULL_POINTER, "social_bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->social_bridge = social_bridge;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

bool omni_wm_tom_bridge_is_connected(const omni_wm_tom_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__is_connected", 0.0f);


    return (bridge->world_model != NULL && bridge->tom != NULL);
}

/* ============================================================================
 * Update API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_tom_bridge_update(
    omni_wm_tom_bridge_t* bridge,
    float dt) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__update", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_modulation) return NIMCP_SUCCESS;

    uint64_t start_time = get_current_time_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update effects in both directions */
    update_wm_to_tom_effects(bridge);
    update_tom_to_wm_effects(bridge);

    /* Update belief-reality gaps */
    if (bridge->config.enable_false_belief_detection) {
        update_belief_reality_gaps(bridge);
    }

    /* Decay agent attention weights */
    if (bridge->config.agent_attention_decay > 0.0f) {
        float decay = expf(-bridge->config.agent_attention_decay * dt);
        for (uint32_t i = 0; i < WM_TOM_MAX_AGENTS; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && WM_TOM_MAX_AGENTS > 256) {
                omni_wm_tom_bridge_heartbeat("omni_wm_tom__loop",
                                 (float)(i + 1) / (float)WM_TOM_MAX_AGENTS);
            }

            bridge->tom_to_wm.social_attention_weights[i] *= decay;
        }
    }

    /* Update timing statistics */
    bridge->stats.total_updates++;
    uint64_t elapsed = get_current_time_us() - start_time;
    bridge->stats.total_processing_time_ms += (double)elapsed / 1000.0;
    bridge->stats.mean_update_time_ms = bridge->stats.total_processing_time_ms /
                                         (double)bridge->stats.total_updates;
    bridge->stats.last_update_time_us = start_time;

    /* Update tracked agent count in stats */
    bridge->stats.current_tracked_agents = bridge->tracked_agent_count;

    /* Record base update */
    bridge_base_record_update(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Mental State Prediction API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_tom_bridge_predict_mental_state(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id,
    uint32_t horizon_steps,
    tom_agent_mental_state_t* out_predicted_state) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__predict_mental_state", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(out_predicted_state, NIMCP_ERROR_NULL_POINTER, "out_predicted_state is NULL");
    NIMCP_CHECK_THROW(horizon_steps > 0, NIMCP_ERROR_INVALID_PARAM, "horizon_steps must be greater than 0");
    NIMCP_CHECK_THROW(horizon_steps <= WM_TOM_MAX_HORIZON, NIMCP_ERROR_INVALID_PARAM, "horizon_steps exceeds WM_TOM_MAX_HORIZON");
    if (!bridge->config.enable_mental_state_prediction) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Find agent */
    int32_t idx = find_tracked_agent(bridge, agent_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    tom_agent_mental_state_t* current = &bridge->tracked_agents[idx];

    /* In full implementation, would:
     * 1. Extract RSSM state for mental state
     * 2. Roll out dynamics for horizon_steps
     * 3. Decode predicted mental state
     * For now, use simple decay/persistence model */

    /* Copy current state as base */
    *out_predicted_state = *current;
    out_predicted_state->last_update_us = get_current_time_us();

    /* Apply simple prediction (placeholder) */
    float persistence = bridge->config.belief_update_rate;
    float decay = powf(persistence, (float)horizon_steps);

    /* Emotional state tends toward neutral */
    out_predicted_state->emotional_intensity *= decay;
    if (out_predicted_state->emotional_intensity < 0.1f) {
        out_predicted_state->emotional_state = WM_TOM_EMOTION_NEUTRAL;
    }

    /* Confidence decreases with horizon */
    out_predicted_state->confidence = current->confidence * decay;

    /* Update statistics */
    bridge->stats.mental_state_predictions++;
    bridge->stats.mean_prediction_confidence =
        0.1f * out_predicted_state->confidence +
        0.9f * bridge->stats.mean_prediction_confidence;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Predicted mental state for agent %u, horizon=%u, confidence=%.2f",
                       agent_id, horizon_steps, out_predicted_state->confidence);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_tom_bridge_update_mental_state(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id,
    const tom_agent_mental_state_t* observed_state) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__update_mental_state", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(observed_state, NIMCP_ERROR_NULL_POINTER, "observed_state is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Find agent */
    int32_t idx = find_tracked_agent(bridge, agent_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    tom_agent_mental_state_t* current = &bridge->tracked_agents[idx];

    /* Integrate observation with current state (Bayesian-like update) */
    float alpha = bridge->config.belief_update_rate;
    float beta = 1.0f - alpha;

    /* Update belief state */
    for (uint32_t i = 0; i < OMNI_WM_STATE_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && OMNI_WM_STATE_DIM > 256) {
            omni_wm_tom_bridge_heartbeat("omni_wm_tom__loop",
                             (float)(i + 1) / (float)OMNI_WM_STATE_DIM);
        }

        current->belief_state[i] = beta * current->belief_state[i] +
                                   alpha * observed_state->belief_state[i];
    }

    /* Update desire state */
    for (uint32_t i = 0; i < OMNI_WM_STATE_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && OMNI_WM_STATE_DIM > 256) {
            omni_wm_tom_bridge_heartbeat("omni_wm_tom__loop",
                             (float)(i + 1) / (float)OMNI_WM_STATE_DIM);
        }

        current->desire_state[i] = beta * current->desire_state[i] +
                                   alpha * observed_state->desire_state[i];
    }

    /* Update intention vector */
    for (uint32_t i = 0; i < OMNI_WM_ACTION_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && OMNI_WM_ACTION_DIM > 256) {
            omni_wm_tom_bridge_heartbeat("omni_wm_tom__loop",
                             (float)(i + 1) / (float)OMNI_WM_ACTION_DIM);
        }

        current->intention_vector[i] = beta * current->intention_vector[i] +
                                       alpha * observed_state->intention_vector[i];
    }

    /* Update emotional state (take observed if confidence high) */
    if (observed_state->confidence > current->confidence) {
        current->emotional_state = observed_state->emotional_state;
        current->emotional_intensity = observed_state->emotional_intensity;
    }

    /* Update confidence */
    current->confidence = fmaxf(current->confidence, observed_state->confidence);

    /* Update timestamp */
    current->last_update_us = get_current_time_us();

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Social Trajectory Prediction API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_tom_bridge_predict_social_trajectory(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id,
    uint32_t horizon_steps,
    tom_social_trajectory_t* out_trajectory) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__predict_social_traje", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(out_trajectory, NIMCP_ERROR_NULL_POINTER, "out_trajectory is NULL");
    NIMCP_CHECK_THROW(horizon_steps > 0, NIMCP_ERROR_INVALID_PARAM, "horizon_steps must be greater than 0");
    NIMCP_CHECK_THROW(horizon_steps <= WM_TOM_MAX_HORIZON, NIMCP_ERROR_INVALID_PARAM, "horizon_steps exceeds WM_TOM_MAX_HORIZON");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Find agent */
    int32_t idx = find_tracked_agent(bridge, agent_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    tom_agent_mental_state_t* agent = &bridge->tracked_agents[idx];

    /* Initialize trajectory */
    memset(out_trajectory, 0, sizeof(tom_social_trajectory_t));
    out_trajectory->agent_id = agent_id;
    out_trajectory->horizon_steps = horizon_steps;
    out_trajectory->prediction_timestamp_us = get_current_time_us();

    /* In full implementation, would:
     * 1. Initialize RSSM with agent's mental state context
     * 2. Roll out forward dynamics
     * 3. Decode positions, actions, emotions at each step
     * For now, use simple linear extrapolation */

    float confidence_decay = NIMCP_ELIGIBILITY_DECAY_DEFAULT;
    float cumulative_confidence = agent->confidence;

    for (uint32_t t = 0; t < horizon_steps; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && horizon_steps > 256) {
            omni_wm_tom_bridge_heartbeat("omni_wm_tom__loop",
                             (float)(t + 1) / (float)horizon_steps);
        }

        /* Placeholder: constant position with slight noise */
        out_trajectory->predicted_positions[t][0] = (float)t * 0.1f;
        out_trajectory->predicted_positions[t][1] = 0.0f;
        out_trajectory->predicted_positions[t][2] = 0.0f;

        /* Copy intention as predicted action */
        memcpy(out_trajectory->predicted_actions[t], agent->intention_vector,
               OMNI_WM_ACTION_DIM * sizeof(float));

        /* Emotion tends toward neutral over time */
        if (t < horizon_steps / 2) {
            out_trajectory->predicted_emotions[t] = agent->emotional_state;
        } else {
            out_trajectory->predicted_emotions[t] = WM_TOM_EMOTION_NEUTRAL;
        }

        /* Update confidence */
        out_trajectory->step_confidences[t] = cumulative_confidence;
        cumulative_confidence *= confidence_decay;
    }

    out_trajectory->trajectory_confidence = agent->confidence *
                                            powf(confidence_decay, (float)horizon_steps / 2.0f);

    /* Update statistics */
    bridge->stats.trajectory_predictions++;
    bridge->stats.mean_trajectory_confidence =
        0.1f * out_trajectory->trajectory_confidence +
        0.9f * bridge->stats.mean_trajectory_confidence;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Predicted trajectory for agent %u, horizon=%u, confidence=%.2f",
                       agent_id, horizon_steps, out_trajectory->trajectory_confidence);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_tom_bridge_predict_joint_trajectory(
    omni_wm_tom_bridge_t* bridge,
    const agent_id_t* agent_ids,
    uint32_t agent_count,
    uint32_t horizon_steps,
    tom_social_trajectory_t* out_trajectories) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__predict_joint_trajec", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(agent_ids, NIMCP_ERROR_NULL_POINTER, "agent_ids is NULL");
    NIMCP_CHECK_THROW(out_trajectories, NIMCP_ERROR_NULL_POINTER, "out_trajectories is NULL");
    NIMCP_CHECK_THROW(agent_count > 0, NIMCP_ERROR_INVALID_PARAM, "agent_count must be greater than 0");
    NIMCP_CHECK_THROW(agent_count <= WM_TOM_MAX_AGENTS, NIMCP_ERROR_INVALID_PARAM, "agent_count exceeds WM_TOM_MAX_AGENTS");
    NIMCP_CHECK_THROW(horizon_steps > 0, NIMCP_ERROR_INVALID_PARAM, "horizon_steps must be greater than 0");
    NIMCP_CHECK_THROW(horizon_steps <= WM_TOM_MAX_HORIZON, NIMCP_ERROR_INVALID_PARAM, "horizon_steps exceeds WM_TOM_MAX_HORIZON");
    if (!bridge->config.enable_joint_prediction) return NIMCP_SUCCESS;

    /* Predict each agent's trajectory */
    for (uint32_t i = 0; i < agent_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && agent_count > 256) {
            omni_wm_tom_bridge_heartbeat("omni_wm_tom__loop",
                             (float)(i + 1) / (float)agent_count);
        }

        nimcp_error_t err = omni_wm_tom_bridge_predict_social_trajectory(
            bridge, agent_ids[i], horizon_steps, &out_trajectories[i]);
        if (err != NIMCP_SUCCESS) {
            /* Initialize empty trajectory on error */
            memset(&out_trajectories[i], 0, sizeof(tom_social_trajectory_t));
            out_trajectories[i].agent_id = agent_ids[i];
        }
    }

    /* Update statistics */
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.joint_predictions++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Counterfactual Reasoning API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_tom_bridge_counterfactual_belief(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id,
    const float* hypothetical_belief,
    tom_social_trajectory_t* out_trajectory) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__counterfactual_belie", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(hypothetical_belief, NIMCP_ERROR_NULL_POINTER, "hypothetical_belief is NULL");
    NIMCP_CHECK_THROW(out_trajectory, NIMCP_ERROR_NULL_POINTER, "out_trajectory is NULL");
    if (!bridge->config.enable_counterfactual_reasoning) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Find agent */
    int32_t idx = find_tracked_agent(bridge, agent_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    tom_agent_mental_state_t* agent = &bridge->tracked_agents[idx];

    /* Create temporary mental state with hypothetical belief */
    tom_agent_mental_state_t hypothetical_state = *agent;
    memcpy(hypothetical_state.belief_state, hypothetical_belief,
           OMNI_WM_STATE_DIM * sizeof(float));

    /* Compute divergence from actual belief */
    float divergence = compute_state_divergence(
        agent->belief_state, hypothetical_belief, OMNI_WM_STATE_DIM);

    /* Predict trajectory under hypothetical belief */
    /* In full implementation, would roll out WM with modified belief */
    memset(out_trajectory, 0, sizeof(tom_social_trajectory_t));
    out_trajectory->agent_id = agent_id;
    out_trajectory->horizon_steps = bridge->config.default_prediction_horizon;
    out_trajectory->prediction_timestamp_us = get_current_time_us();

    /* Confidence reduced by divergence */
    out_trajectory->trajectory_confidence = agent->confidence * (1.0f - fminf(divergence, 1.0f));

    /* Placeholder trajectory prediction */
    for (uint32_t t = 0; t < out_trajectory->horizon_steps; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && out_trajectory->horizon_steps > 256) {
            omni_wm_tom_bridge_heartbeat("omni_wm_tom__loop",
                             (float)(t + 1) / (float)out_trajectory->horizon_steps);
        }

        out_trajectory->step_confidences[t] = out_trajectory->trajectory_confidence *
                                              powf(0.95f, (float)t);
        out_trajectory->predicted_emotions[t] = WM_TOM_EMOTION_NEUTRAL;
    }

    /* Update statistics */
    bridge->stats.counterfactual_queries++;
    bridge->stats.mean_counterfactual_divergence =
        0.1f * divergence + 0.9f * bridge->stats.mean_counterfactual_divergence;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Counterfactual belief query for agent %u, divergence=%.2f",
                       agent_id, divergence);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_tom_bridge_counterfactual_action(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id,
    const float* hypothetical_action,
    uint32_t action_dim,
    tom_social_trajectory_t* out_trajectory) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__counterfactual_actio", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(hypothetical_action, NIMCP_ERROR_NULL_POINTER, "hypothetical_action is NULL");
    NIMCP_CHECK_THROW(out_trajectory, NIMCP_ERROR_NULL_POINTER, "out_trajectory is NULL");
    NIMCP_CHECK_THROW(action_dim > 0, NIMCP_ERROR_INVALID_PARAM, "action_dim must be greater than 0");
    NIMCP_CHECK_THROW(action_dim <= OMNI_WM_ACTION_DIM, NIMCP_ERROR_INVALID_PARAM, "action_dim exceeds OMNI_WM_ACTION_DIM");
    if (!bridge->config.enable_counterfactual_reasoning) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Find agent */
    int32_t idx = find_tracked_agent(bridge, agent_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    tom_agent_mental_state_t* agent = &bridge->tracked_agents[idx];

    /* Initialize trajectory */
    memset(out_trajectory, 0, sizeof(tom_social_trajectory_t));
    out_trajectory->agent_id = agent_id;
    out_trajectory->horizon_steps = bridge->config.default_prediction_horizon;
    out_trajectory->prediction_timestamp_us = get_current_time_us();

    /* In full implementation, would roll out WM with hypothetical action */
    /* Use hypothetical action as first step */
    memcpy(out_trajectory->predicted_actions[0], hypothetical_action,
           action_dim * sizeof(float));

    out_trajectory->trajectory_confidence = agent->confidence * 0.8f;

    for (uint32_t t = 0; t < out_trajectory->horizon_steps; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && out_trajectory->horizon_steps > 256) {
            omni_wm_tom_bridge_heartbeat("omni_wm_tom__loop",
                             (float)(t + 1) / (float)out_trajectory->horizon_steps);
        }

        out_trajectory->step_confidences[t] = out_trajectory->trajectory_confidence *
                                              powf(0.95f, (float)t);
        out_trajectory->predicted_emotions[t] = agent->emotional_state;
    }

    /* Update statistics */
    bridge->stats.counterfactual_queries++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * False Belief Detection API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_tom_bridge_detect_false_beliefs(
    omni_wm_tom_bridge_t* bridge,
    tom_belief_reality_gap_t* out_gaps,
    uint32_t* out_gap_count) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__detect_false_beliefs", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(out_gaps, NIMCP_ERROR_NULL_POINTER, "out_gaps is NULL");
    NIMCP_CHECK_THROW(out_gap_count, NIMCP_ERROR_NULL_POINTER, "out_gap_count is NULL");
    if (!bridge->config.enable_false_belief_detection) {
        *out_gap_count = 0;
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update all gaps */
    update_belief_reality_gaps(bridge);

    /* Count and copy gaps with false beliefs */
    uint32_t count = 0;
    for (uint32_t i = 0; i < bridge->belief_gap_count && count < WM_TOM_MAX_AGENTS; i++) {
        if (bridge->belief_gaps[i].has_false_belief) {
            out_gaps[count] = bridge->belief_gaps[i];
            count++;
        }
    }

    *out_gap_count = count;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Detected %u false beliefs", count);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_tom_bridge_get_belief_gap(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id,
    tom_belief_reality_gap_t* out_gap) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__get_belief_gap", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(out_gap, NIMCP_ERROR_NULL_POINTER, "out_gap is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    int32_t idx = find_belief_gap(bridge, agent_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    *out_gap = bridge->belief_gaps[idx];

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Social Training API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_tom_bridge_train_from_interaction(
    omni_wm_tom_bridge_t* bridge,
    const tom_social_interaction_t* interaction) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__train_from_interacti", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(interaction, NIMCP_ERROR_NULL_POINTER, "interaction is NULL");
    if (!bridge->config.enable_social_training) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Add to interaction buffer (circular buffer) */
    uint32_t next_head = (bridge->interaction_buffer_head + 1) %
                          bridge->interaction_buffer_capacity;

    bridge->interaction_buffer[bridge->interaction_buffer_head] = *interaction;
    bridge->interaction_buffer_head = next_head;

    /* If buffer full, advance tail */
    if (next_head == bridge->interaction_buffer_tail) {
        bridge->interaction_buffer_tail = (bridge->interaction_buffer_tail + 1) %
                                          bridge->interaction_buffer_capacity;
    }

    /* In full implementation, would:
     * 1. Extract state transition from interaction
     * 2. Compute RSSM gradients
     * 3. Update dynamics weights
     * For now, update statistics */

    bridge->stats.interactions_processed++;
    bridge->stats.training_updates++;

    /* Update social context based on interaction type */
    if (interaction->is_cooperative) {
        bridge->cooperative_mode = true;
        bridge->competitive_mode = false;
    } else if (interaction->is_competitive) {
        bridge->competitive_mode = true;
        bridge->cooperative_mode = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Trained from interaction: initiator=%u, responder=%u, coop=%d",
                       interaction->initiator_id, interaction->responder_id,
                       interaction->is_cooperative);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_tom_bridge_train_batch(omni_wm_tom_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__train_batch", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_social_training) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if buffer has interactions */
    if (bridge->interaction_buffer_head == bridge->interaction_buffer_tail) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_SUCCESS; /* Nothing to train on */
    }

    /* In full implementation, would:
     * 1. Sample batch from buffer
     * 2. Compute batch gradients
     * 3. Apply updates
     * For now, update statistics */

    bridge->stats.training_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Mirror Neuron Integration API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_tom_bridge_on_mirror_action(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id,
    const float* action,
    uint32_t action_dim,
    float confidence) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__on_mirror_action", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(action, NIMCP_ERROR_NULL_POINTER, "action is NULL");
    NIMCP_CHECK_THROW(action_dim > 0, NIMCP_ERROR_INVALID_PARAM, "action_dim must be greater than 0");
    NIMCP_CHECK_THROW(action_dim <= OMNI_WM_ACTION_DIM, NIMCP_ERROR_INVALID_PARAM, "action_dim exceeds OMNI_WM_ACTION_DIM");
    if (!bridge->config.enable_mirror_integration) return NIMCP_SUCCESS;
    if (confidence < bridge->config.mirror_action_threshold) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update ToM effects with mirror action */
    bridge->tom_to_wm.mirror_action_observed = true;
    memcpy(bridge->tom_to_wm.observed_action, action, action_dim * sizeof(float));
    bridge->tom_to_wm.action_understanding_confidence = confidence;
    bridge->tom_to_wm.observed_action_agent = agent_id;

    /* Find and update agent's intention vector */
    int32_t idx = find_tracked_agent(bridge, agent_id);
    if (idx >= 0) {
        tom_agent_mental_state_t* agent = &bridge->tracked_agents[idx];

        /* Integrate observed action into intention */
        float alpha = confidence * 0.5f;
        for (uint32_t i = 0; i < action_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && action_dim > 256) {
                omni_wm_tom_bridge_heartbeat("omni_wm_tom__loop",
                                 (float)(i + 1) / (float)action_dim);
            }

            agent->intention_vector[i] = (1.0f - alpha) * agent->intention_vector[i] +
                                         alpha * action[i];
        }
        agent->last_update_us = get_current_time_us();
    }

    /* Update statistics */
    bridge->stats.mirror_actions_received++;
    bridge->stats.mean_mirror_confidence =
        0.1f * confidence + 0.9f * bridge->stats.mean_mirror_confidence;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Mirror action received from agent %u, confidence=%.2f",
                       agent_id, confidence);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Empathy Simulation API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_tom_bridge_empathy_simulation(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id,
    float* out_simulated_state) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__empathy_simulation", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(out_simulated_state, NIMCP_ERROR_NULL_POINTER, "out_simulated_state is NULL");
    if (!bridge->config.enable_empathy_simulation) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Find agent */
    int32_t idx = find_tracked_agent(bridge, agent_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    tom_agent_mental_state_t* agent = &bridge->tracked_agents[idx];

    /* In full implementation, would:
     * 1. Set WM state to agent's believed state
     * 2. Run simulation from agent's perspective
     * 3. Return simulated experiential state
     * For now, return weighted belief state */

    float empathy = bridge->config.empathy_strength;
    for (uint32_t i = 0; i < OMNI_WM_STATE_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && OMNI_WM_STATE_DIM > 256) {
            omni_wm_tom_bridge_heartbeat("omni_wm_tom__loop",
                             (float)(i + 1) / (float)OMNI_WM_STATE_DIM);
        }

        out_simulated_state[i] = agent->belief_state[i] * empathy;
    }

    /* Store in WM effects */
    memcpy(bridge->wm_to_tom.empathy_simulation_result, out_simulated_state,
           OMNI_WM_STATE_DIM * sizeof(float));
    bridge->wm_to_tom.perspective_confidence = agent->confidence * empathy;

    /* Update statistics */
    bridge->stats.empathy_simulations++;
    bridge->stats.mean_empathy_accuracy =
        0.1f * agent->confidence + 0.9f * bridge->stats.mean_empathy_accuracy;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Empathy simulation for agent %u, empathy=%.2f",
                       agent_id, empathy);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Agent Tracking API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_tom_bridge_track_agent(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id,
    const tom_agent_mental_state_t* initial_state) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__track_agent", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if already tracked */
    if (find_tracked_agent(bridge, agent_id) >= 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_SUCCESS; /* Already tracked */
    }

    /* Check capacity */
    if (bridge->tracked_agent_count >= bridge->tracked_agent_capacity) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Add agent */
    uint32_t idx = bridge->tracked_agent_count;
    if (initial_state) {
        bridge->tracked_agents[idx] = *initial_state;
    } else {
        memset(&bridge->tracked_agents[idx], 0, sizeof(tom_agent_mental_state_t));
        bridge->tracked_agents[idx].emotional_state = WM_TOM_EMOTION_NEUTRAL;
        bridge->tracked_agents[idx].confidence = 0.5f;
    }
    bridge->tracked_agents[idx].agent_id = agent_id;
    bridge->tracked_agents[idx].last_update_us = get_current_time_us();

    bridge->tracked_agent_count++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Started tracking agent %u", agent_id);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_tom_bridge_untrack_agent(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__untrack_agent", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    int32_t idx = find_tracked_agent(bridge, agent_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Remove by swapping with last element */
    if ((uint32_t)idx < bridge->tracked_agent_count - 1) {
        bridge->tracked_agents[idx] = bridge->tracked_agents[bridge->tracked_agent_count - 1];
    }
    bridge->tracked_agent_count--;

    /* Also remove from belief gaps */
    int32_t gap_idx = find_belief_gap(bridge, agent_id);
    if (gap_idx >= 0) {
        if ((uint32_t)gap_idx < bridge->belief_gap_count - 1) {
            bridge->belief_gaps[gap_idx] = bridge->belief_gaps[bridge->belief_gap_count - 1];
        }
        bridge->belief_gap_count--;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Stopped tracking agent %u", agent_id);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_tom_bridge_get_agent_state(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id,
    tom_agent_mental_state_t* out_state) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__get_agent_state", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(out_state, NIMCP_ERROR_NULL_POINTER, "out_state is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    int32_t idx = find_tracked_agent(bridge, agent_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    *out_state = bridge->tracked_agents[idx];

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_tom_bridge_set_focus_agent(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__set_focus_agent", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Verify agent is tracked */
    if (find_tracked_agent(bridge, agent_id) < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    bridge->focus_agent = agent_id;

    /* Boost attention weight for focus agent */
    for (uint32_t i = 0; i < bridge->tracked_agent_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->tracked_agent_count > 256) {
            omni_wm_tom_bridge_heartbeat("omni_wm_tom__loop",
                             (float)(i + 1) / (float)bridge->tracked_agent_count);
        }

        if (bridge->tracked_agents[i].agent_id == agent_id) {
            bridge->tom_to_wm.social_attention_weights[i] = 1.0f;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Social Context API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_tom_bridge_set_cooperative_context(
    omni_wm_tom_bridge_t* bridge,
    bool is_cooperative) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__set_cooperative_cont", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->cooperative_mode = is_cooperative;
    bridge->tom_to_wm.is_cooperative_context = is_cooperative;
    if (is_cooperative) {
        bridge->competitive_mode = false;
        bridge->tom_to_wm.is_competitive_context = false;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_tom_bridge_set_competitive_context(
    omni_wm_tom_bridge_t* bridge,
    bool is_competitive) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__set_competitive_cont", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->competitive_mode = is_competitive;
    bridge->tom_to_wm.is_competitive_context = is_competitive;
    if (is_competitive) {
        bridge->cooperative_mode = false;
        bridge->tom_to_wm.is_cooperative_context = false;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

const omni_wm_to_tom_effects_t* omni_wm_tom_bridge_get_wm_effects(
    const omni_wm_tom_bridge_t* bridge) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__get_wm_effects", 0.0f);


    return &bridge->wm_to_tom;
}

const tom_to_omni_wm_effects_t* omni_wm_tom_bridge_get_tom_effects(
    const omni_wm_tom_bridge_t* bridge) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__get_tom_effects", 0.0f);


    return &bridge->tom_to_wm;
}

nimcp_error_t omni_wm_tom_bridge_get_stats(
    const omni_wm_tom_bridge_t* bridge,
    omni_wm_tom_bridge_stats_t* stats) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__get_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_tom_bridge_reset_stats(
    omni_wm_tom_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__reset_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(omni_wm_tom_bridge_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_tom_bridge_connect_bio_async(
    omni_wm_tom_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__connect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_bio_async) return NIMCP_SUCCESS;
    if (bridge->base.bio_async_enabled) return NIMCP_SUCCESS;

    /* Check if router is initialized */
    if (!bio_router_is_initialized()) {
        NIMCP_LOGGING_DEBUG("Bio-async router not initialized, skipping registration");
        return NIMCP_SUCCESS;
    }

    /* Register module with router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_WM_TOM_BRIDGE,
        .module_name = "wm_tom_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (!bridge->base.bio_ctx) {
        NIMCP_LOGGING_WARN("Failed to register with bio-async router");
        return NIMCP_SUCCESS; /* Non-fatal */
    }

    /* Register message handlers */
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_TOM_MENTAL_STATE_PRED,
                                handle_mental_state_pred);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_TOM_TRAJECTORY_PRED,
                                handle_trajectory_pred);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_TOM_COUNTERFACTUAL_REQ,
                                handle_counterfactual_req);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_TOM_FALSE_BELIEF_DETECT,
                                handle_false_belief_detect);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_TOM_SOCIAL_INTERACTION,
                                handle_social_interaction);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_TOM_EMPATHY_SIMULATION,
                                handle_empathy_simulation);

    bridge->base.bio_async_enabled = true;
    NIMCP_LOGGING_INFO("WM ToM Bridge connected to bio-async router");

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_tom_bridge_disconnect_bio_async(
    omni_wm_tom_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__disconnect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->base.bio_async_enabled) return NIMCP_SUCCESS;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("WM ToM Bridge disconnected from bio-async router");

    return NIMCP_SUCCESS;
}

bool omni_wm_tom_bridge_is_bio_async_connected(
    const omni_wm_tom_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__is_bio_async_connect", 0.0f);


    return bridge_base_is_bio_async_connected(bridge ? &bridge->base : NULL);
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

const char* omni_wm_tom_msg_type_to_string(omni_wm_tom_msg_type_t msg_type) {
    switch (msg_type) {
        case BIO_MSG_WM_TOM_MENTAL_STATE_PRED:
            return "MENTAL_STATE_PRED";
        case BIO_MSG_WM_TOM_MENTAL_STATE_RESULT:
            return "MENTAL_STATE_RESULT";
        case BIO_MSG_WM_TOM_BELIEF_UPDATE:
            return "BELIEF_UPDATE";
        case BIO_MSG_WM_TOM_DESIRE_UPDATE:
            return "DESIRE_UPDATE";
        case BIO_MSG_WM_TOM_INTENTION_UPDATE:
            return "INTENTION_UPDATE";
        case BIO_MSG_WM_TOM_TRAJECTORY_PRED:
            return "TRAJECTORY_PRED";
        case BIO_MSG_WM_TOM_TRAJECTORY_RESULT:
            return "TRAJECTORY_RESULT";
        case BIO_MSG_WM_TOM_TRAJECTORY_STEP:
            return "TRAJECTORY_STEP";
        case BIO_MSG_WM_TOM_COUNTERFACTUAL_REQ:
            return "COUNTERFACTUAL_REQ";
        case BIO_MSG_WM_TOM_COUNTERFACTUAL_RESULT:
            return "COUNTERFACTUAL_RESULT";
        case BIO_MSG_WM_TOM_WHAT_IF_BELIEF:
            return "WHAT_IF_BELIEF";
        case BIO_MSG_WM_TOM_FALSE_BELIEF_DETECT:
            return "FALSE_BELIEF_DETECT";
        case BIO_MSG_WM_TOM_BELIEF_REALITY_GAP:
            return "BELIEF_REALITY_GAP";
        case BIO_MSG_WM_TOM_BELIEF_SYNC:
            return "BELIEF_SYNC";
        case BIO_MSG_WM_TOM_SOCIAL_INTERACTION:
            return "SOCIAL_INTERACTION";
        case BIO_MSG_WM_TOM_INTERACTION_OUTCOME:
            return "INTERACTION_OUTCOME";
        case BIO_MSG_WM_TOM_COOPERATION_SIGNAL:
            return "COOPERATION_SIGNAL";
        case BIO_MSG_WM_TOM_COMPETITION_SIGNAL:
            return "COMPETITION_SIGNAL";
        case BIO_MSG_WM_TOM_JOINT_PREDICTION:
            return "JOINT_PREDICTION";
        case BIO_MSG_WM_TOM_JOINT_RESULT:
            return "JOINT_RESULT";
        case BIO_MSG_WM_TOM_GROUP_DYNAMICS:
            return "GROUP_DYNAMICS";
        case BIO_MSG_WM_TOM_EMPATHY_SIMULATION:
            return "EMPATHY_SIMULATION";
        case BIO_MSG_WM_TOM_EMPATHY_RESULT:
            return "EMPATHY_RESULT";
        case BIO_MSG_WM_TOM_MIRROR_ACTION:
            return "MIRROR_ACTION";
        case BIO_MSG_WM_TOM_BRIDGE_STATUS:
            return "BRIDGE_STATUS";
        case BIO_MSG_WM_TOM_BRIDGE_ERROR:
            return "BRIDGE_ERROR";
        case BIO_MSG_WM_TOM_STATS_UPDATE:
            return "STATS_UPDATE";
        default:
            return "UNKNOWN";
    }
}

const char* omni_wm_tom_emotion_to_string(wm_tom_emotion_t emotion) {
    switch (emotion) {
        case WM_TOM_EMOTION_UNKNOWN:
            return "UNKNOWN";
        case WM_TOM_EMOTION_NEUTRAL:
            return "NEUTRAL";
        case WM_TOM_EMOTION_JOY:
            return "JOY";
        case WM_TOM_EMOTION_SADNESS:
            return "SADNESS";
        case WM_TOM_EMOTION_ANGER:
            return "ANGER";
        case WM_TOM_EMOTION_FEAR:
            return "FEAR";
        case WM_TOM_EMOTION_SURPRISE:
            return "SURPRISE";
        case WM_TOM_EMOTION_DISGUST:
            return "DISGUST";
        case WM_TOM_EMOTION_ANXIETY:
            return "ANXIETY";
        case WM_TOM_EMOTION_CALM:
            return "CALM";
        default:
            return "INVALID";
    }
}

nimcp_error_t omni_wm_tom_bridge_validate_config(
    const omni_wm_tom_bridge_config_t* config) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_tom_bridge_heartbeat("omni_wm_tom__validate_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* Validate sensitivity range */
    if (config->sensitivity < 0.5f || config->sensitivity > 2.0f) {
        NIMCP_LOGGING_WARN("Sensitivity %.2f out of range [0.5, 2.0]",
                          config->sensitivity);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate prediction horizon */
    if (config->default_prediction_horizon == 0 ||
        config->default_prediction_horizon > WM_TOM_MAX_HORIZON) {
        NIMCP_LOGGING_WARN("Invalid default_prediction_horizon: %u",
                          config->default_prediction_horizon);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate false belief threshold */
    if (config->enable_false_belief_detection) {
        if (config->false_belief_threshold < 0.0f ||
            config->false_belief_threshold > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid false_belief_threshold: %.2f",
                              config->false_belief_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate max tracked agents */
    if (config->max_tracked_agents == 0 ||
        config->max_tracked_agents > WM_TOM_MAX_AGENTS) {
        NIMCP_LOGGING_WARN("Invalid max_tracked_agents: %u",
                          config->max_tracked_agents);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate counterfactual settings */
    if (config->enable_counterfactual_reasoning) {
        if (config->max_counterfactuals == 0 ||
            config->max_counterfactuals > WM_TOM_MAX_COUNTERFACTUALS) {
            NIMCP_LOGGING_WARN("Invalid max_counterfactuals: %u",
                              config->max_counterfactuals);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate learning rates */
    if (config->enable_mental_state_prediction) {
        if (config->mental_state_learning_rate <= 0.0f ||
            config->mental_state_learning_rate > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid mental_state_learning_rate: %.4f",
                              config->mental_state_learning_rate);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    if (config->enable_social_training) {
        if (config->social_training_learning_rate <= 0.0f ||
            config->social_training_learning_rate > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid social_training_learning_rate: %.4f",
                              config->social_training_learning_rate);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate empathy strength */
    if (config->enable_empathy_simulation) {
        if (config->empathy_strength < 0.0f ||
            config->empathy_strength > 2.0f) {
            NIMCP_LOGGING_WARN("Invalid empathy_strength: %.2f",
                              config->empathy_strength);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate mirror threshold */
    if (config->enable_mirror_integration) {
        if (config->mirror_action_threshold < 0.0f ||
            config->mirror_action_threshold > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid mirror_action_threshold: %.2f",
                              config->mirror_action_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    return NIMCP_SUCCESS;
}
