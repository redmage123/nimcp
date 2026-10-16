/**
 * @file nimcp_omni_wm_cognitive_bridge.c
 * @brief World Model Cognitive Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-17
 *
 * WHAT: Bidirectional bridge connecting World Model (RSSM) with cognitive layer systems
 * WHY:  Enable prediction-informed planning and goal-conditioned world modeling
 * HOW:  World model predictions guide executive planning; cognitive goals condition predictions
 *
 * IMPLEMENTATION NOTES:
 * =====================
 * This implementation integrates several key cognitive systems:
 *
 * 1. EXECUTIVE FUNCTION INTEGRATION:
 *    - State predictions for multi-step planning
 *    - Action consequence predictions for decision-making
 *    - Plan evaluation via world model simulation
 *
 * 2. ATTENTION-PREDICTION COUPLING:
 *    - Attention focus boosts prediction priority
 *    - Prediction errors guide attention allocation
 *    - Selective prediction for attended regions
 *
 * 3. GOAL-CONDITIONED PREDICTION:
 *    - Goals condition world model state predictions
 *    - Progress tracking via state-goal distance
 *    - Goal achievement and failure notifications
 *
 * 4. SALIENCE-DRIVEN PRIORITIZATION:
 *    - Novelty, surprise, urgency affect prediction resources
 *    - High salience triggers detailed predictions
 *    - Low salience allows coarse predictions
 *
 * 5. WORKING MEMORY CONTEXT:
 *    - WM items provide context for predictions
 *    - Limited capacity affects prediction scope
 *    - Decay and refresh dynamics
 */

#include "cognitive/omni/bridges/nimcp_omni_wm_cognitive_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ============================================================================
 * Module-level Constants
 * ============================================================================ */

#define LOG_MODULE "wm_cognitive_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_threshold_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(omni_wm_cognitive_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_omni_wm_cognitive_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_omni_wm_cognitive_bridge_mesh_registry = NULL;

nimcp_error_t omni_wm_cognitive_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_omni_wm_cognitive_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "omni_wm_cognitive_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "omni_wm_cognitive_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_omni_wm_cognitive_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_omni_wm_cognitive_bridge_mesh_registry = registry;
    return err;
}

void omni_wm_cognitive_bridge_mesh_unregister(void) {
    if (g_omni_wm_cognitive_bridge_mesh_registry && g_omni_wm_cognitive_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_omni_wm_cognitive_bridge_mesh_registry, g_omni_wm_cognitive_bridge_mesh_id);
        g_omni_wm_cognitive_bridge_mesh_id = 0;
        g_omni_wm_cognitive_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from omni_wm_cognitive_bridge module (instance-level) */
static inline void omni_wm_cognitive_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_omni_wm_cognitive_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_omni_wm_cognitive_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_omni_wm_cognitive_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

void omni_wm_cognitive_bridge_set_instance_health_agent(
    omni_wm_cognitive_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "omni_wm_cognitive_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int omni_wm_cognitive_bridge_training_begin(omni_wm_cognitive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_cognitive_bridge_training_begin: NULL argument");
        return -1;
    }
    omni_wm_cognitive_bridge_heartbeat_instance(g_omni_wm_cognitive_bridge_health_agent, "training_begin", 0.0f);
    (void)bridge;
    return 0;
}

int omni_wm_cognitive_bridge_training_step(omni_wm_cognitive_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_cognitive_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    omni_wm_cognitive_bridge_heartbeat_instance(g_omni_wm_cognitive_bridge_health_agent, "training_step", progress);
    (void)bridge;
    return 0;
}

int omni_wm_cognitive_bridge_training_end(omni_wm_cognitive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_cognitive_bridge_training_end: NULL argument");
        return -1;
    }
    omni_wm_cognitive_bridge_heartbeat_instance(g_omni_wm_cognitive_bridge_health_agent, "training_end", 1.0f);
    (void)bridge;
    return 0;
}

/** Default state prediction buffer size */
#define DEFAULT_STATE_BUFFER_SIZE 256

/** Default action consequence buffer capacity */
#define DEFAULT_ACTION_BUFFER_CAPACITY 16

/** Context cache TTL in microseconds (5 seconds) */
#define CONTEXT_CACHE_TTL_US 5000000

/** Minimum salience for priority boost */
#define MIN_SALIENCE_BOOST_THRESHOLD 0.3f

/** Maximum attention bandwidth */
#define MAX_ATTENTION_BANDWIDTH 1.0f

/* ============================================================================
 * Internal Helper Forward Declarations
 * ============================================================================ */

static nimcp_error_t allocate_prediction_buffers(omni_wm_cognitive_bridge_t* bridge);
static void free_prediction_buffers(omni_wm_cognitive_bridge_t* bridge);
static nimcp_error_t allocate_wm_context_cache(omni_wm_cognitive_bridge_t* bridge);
static void free_wm_context_cache(omni_wm_cognitive_bridge_t* bridge);
static nimcp_error_t update_wm_to_cognitive_effects(omni_wm_cognitive_bridge_t* bridge);
static nimcp_error_t update_cognitive_to_wm_effects(omni_wm_cognitive_bridge_t* bridge);
static nimcp_error_t process_goal_updates(omni_wm_cognitive_bridge_t* bridge, float dt);
static nimcp_error_t process_attention_decay(omni_wm_cognitive_bridge_t* bridge, float dt);
static nimcp_error_t compute_combined_salience(omni_wm_cognitive_bridge_t* bridge);
static uint64_t get_current_time_us(void);
static int find_goal_by_id(const omni_wm_cognitive_bridge_t* bridge, uint32_t goal_id);

/* Bio-async handlers */
static nimcp_error_t handle_state_prediction(const void* msg, size_t msg_size,
                                              nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_goal_update(const void* msg, size_t msg_size,
                                         nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_attention_focus(const void* msg, size_t msg_size,
                                             nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_salience_update(const void* msg, size_t msg_size,
                                             nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_wm_context(const void* msg, size_t msg_size,
                                        nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_meta_lr_update(const void* msg, size_t msg_size,
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
 * @brief Find goal index by ID
 */
static int find_goal_by_id(const omni_wm_cognitive_bridge_t* bridge, uint32_t goal_id) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_goal_by_id: bridge is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->num_goals; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_goals > 256) {
            omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_loop",
                             (float)(i + 1) / (float)bridge->num_goals);
        }

        if (bridge->goals[i].goal_id == goal_id && bridge->goals[i].is_active) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_goal_by_id: validation failed");
    return -1;
}

/**
 * @brief Allocate prediction buffers
 */
static nimcp_error_t allocate_prediction_buffers(omni_wm_cognitive_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Allocate state prediction buffer */
    bridge->state_prediction_buffer = nimcp_calloc(DEFAULT_STATE_BUFFER_SIZE, sizeof(float));
    if (!bridge->state_prediction_buffer) return NIMCP_ERROR_NO_MEMORY;
    bridge->state_prediction_dim = DEFAULT_STATE_BUFFER_SIZE;

    /* Allocate action consequence buffer */
    bridge->action_consequence_buffer = nimcp_calloc(DEFAULT_ACTION_BUFFER_CAPACITY, sizeof(float*));
    if (!bridge->action_consequence_buffer) {
        nimcp_free(bridge->state_prediction_buffer);
        bridge->state_prediction_buffer = NULL;
        return NIMCP_ERROR_NO_MEMORY;
    }

    for (uint32_t i = 0; i < DEFAULT_ACTION_BUFFER_CAPACITY; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && DEFAULT_ACTION_BUFFER_CAPACITY > 256) {
            omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_loop",
                             (float)(i + 1) / (float)DEFAULT_ACTION_BUFFER_CAPACITY);
        }

        bridge->action_consequence_buffer[i] = nimcp_calloc(DEFAULT_STATE_BUFFER_SIZE, sizeof(float));
        if (!bridge->action_consequence_buffer[i]) {
            /* Clean up previously allocated buffers */
            for (uint32_t j = 0; j < i; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && i > 256) {
                    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_loop",
                                     (float)(j + 1) / (float)i);
                }

                nimcp_free(bridge->action_consequence_buffer[j]);
            }
            nimcp_free(bridge->action_consequence_buffer);
            nimcp_free(bridge->state_prediction_buffer);
            bridge->state_prediction_buffer = NULL;
            bridge->action_consequence_buffer = NULL;
            return NIMCP_ERROR_NO_MEMORY;
        }
    }
    bridge->action_consequence_capacity = DEFAULT_ACTION_BUFFER_CAPACITY;

    return NIMCP_SUCCESS;
}

/**
 * @brief Free prediction buffers
 */
static void free_prediction_buffers(omni_wm_cognitive_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_free(bridge->state_prediction_buffer);
    bridge->state_prediction_buffer = NULL;
    bridge->state_prediction_dim = 0;

    if (bridge->action_consequence_buffer) {
        for (uint32_t i = 0; i < bridge->action_consequence_capacity; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->action_consequence_capacity > 256) {
                omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_loop",
                                 (float)(i + 1) / (float)bridge->action_consequence_capacity);
            }

            nimcp_free(bridge->action_consequence_buffer[i]);
        }
        nimcp_free(bridge->action_consequence_buffer);
        bridge->action_consequence_buffer = NULL;
    }
    bridge->action_consequence_capacity = 0;
}

/**
 * @brief Allocate WM context cache
 */
static nimcp_error_t allocate_wm_context_cache(omni_wm_cognitive_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    uint32_t dim = DEFAULT_STATE_BUFFER_SIZE;
    bridge->wm_context_cache = nimcp_calloc(dim, sizeof(float));
    if (!bridge->wm_context_cache) return NIMCP_ERROR_NO_MEMORY;

    bridge->wm_context_cache_dim = dim;
    bridge->wm_context_valid = false;
    bridge->wm_context_time = 0;

    return NIMCP_SUCCESS;
}

/**
 * @brief Free WM context cache
 */
static void free_wm_context_cache(omni_wm_cognitive_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_free(bridge->wm_context_cache);
    bridge->wm_context_cache = NULL;
    bridge->wm_context_cache_dim = 0;
    bridge->wm_context_valid = false;
}

/**
 * @brief Update effects flowing from WM to cognitive systems
 */
static nimcp_error_t update_wm_to_cognitive_effects(omni_wm_cognitive_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    omni_wm_to_cognitive_effects_t* effects = &bridge->wm_to_cognitive;

    /* If world model connected, extract prediction information */
    if (bridge->world_model) {
        /* Placeholder: would extract actual WM predictions */
        effects->prediction_confidence = 0.8f;
        effects->prediction_uncertainty = 0.2f;
        effects->prediction_horizon = bridge->config.action_consequence_horizon;

        /* Compute average prediction error */
        effects->avg_prediction_error = 0.1f; /* Placeholder */
        effects->max_prediction_error = 0.15f;
    }

    /* Compute recommended learning rate based on prediction performance */
    float base_lr = NIMCP_LEARNING_RATE_FINE;
    float pe_factor = 1.0f + effects->avg_prediction_error;
    effects->recommended_lr = base_lr * pe_factor;

    /* Task difficulty estimate */
    effects->task_difficulty = effects->avg_prediction_error;
    effects->transfer_potential = 1.0f - effects->task_difficulty;

    /* Update goal progress predictions */
    effects->num_goals_tracked = bridge->num_goals;

    return NIMCP_SUCCESS;
}

/**
 * @brief Update effects flowing from cognitive systems to WM
 */
static nimcp_error_t update_cognitive_to_wm_effects(omni_wm_cognitive_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    cognitive_to_omni_wm_effects_t* effects = &bridge->cognitive_to_wm;

    /* Update goal information */
    effects->active_goals = bridge->goals;
    effects->num_active_goals = bridge->num_goals;

    /* Update attention focus */
    effects->attention_focus = bridge->current_focus;
    effects->attention_active = bridge->focus_active;
    if (bridge->focus_active) {
        effects->attention_bandwidth = bridge->current_focus.focus_bandwidth;
    } else {
        effects->attention_bandwidth = MAX_ATTENTION_BANDWIDTH;
    }

    /* Update salience */
    effects->salience = bridge->current_salience;
    effects->high_salience_event = (bridge->current_salience.combined_salience >
                                    bridge->config.salience_threshold);

    /* Update WM context info */
    effects->wm_context = bridge->wm_context_cache;
    effects->wm_context_dim = bridge->wm_context_cache_dim;

    /* Query executive for cognitive load if connected */
    if (bridge->executive) {
        /* Placeholder: would query actual executive state */
        effects->cognitive_load = 0.5f;
        effects->active_task_count = 1;
        effects->inhibition_active = false;
    }

    /* Query working memory for utilization if connected */
    if (bridge->working_memory) {
        /* Placeholder: would query actual WM state */
        effects->wm_utilization = 0.6f;
        effects->wm_item_count = 4;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Process goal updates (progress, deadline checks)
 */
static nimcp_error_t process_goal_updates(omni_wm_cognitive_bridge_t* bridge, float dt) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    uint64_t now = get_current_time_us();

    for (uint32_t i = 0; i < bridge->num_goals; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_goals > 256) {
            omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_loop",
                             (float)(i + 1) / (float)bridge->num_goals);
        }

        wm_cognitive_goal_t* goal = &bridge->goals[i];
        if (!goal->is_active) continue;

        /* Check deadline */
        if (goal->deadline_us > 0 && now > goal->deadline_us) {
            /* Deadline passed without completion */
            goal->is_active = false;
            bridge->stats.goals_failed++;
            NIMCP_LOGGING_DEBUG("Goal %u deadline passed", goal->goal_id);
        }

        /* Apply priority decay */
        if (bridge->config.goal_priority_decay > 0.0f) {
            goal->priority *= (1.0f - bridge->config.goal_priority_decay * dt);
            if (goal->priority < 0.01f) {
                goal->priority = 0.01f; /* Minimum priority */
            }
        }

        /* Check progress completion */
        if (goal->progress >= bridge->config.goal_progress_threshold) {
            goal->is_active = false;
            bridge->stats.goals_achieved++;
            NIMCP_LOGGING_DEBUG("Goal %u achieved (progress=%.2f)",
                               goal->goal_id, goal->progress);
        }
    }

    /* Compact goal array (remove inactive) */
    uint32_t write_idx = 0;
    for (uint32_t read_idx = 0; read_idx < bridge->num_goals; read_idx++) {
        /* Phase 8: Loop progress heartbeat */
        if ((read_idx & 0xFF) == 0 && bridge->num_goals > 256) {
            omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_loop",
                             (float)(read_idx + 1) / (float)bridge->num_goals);
        }

        if (bridge->goals[read_idx].is_active) {
            if (write_idx != read_idx) {
                bridge->goals[write_idx] = bridge->goals[read_idx];
            }
            write_idx++;
        }
    }
    bridge->num_goals = write_idx;

    return NIMCP_SUCCESS;
}

/**
 * @brief Process attention focus decay
 */
static nimcp_error_t process_attention_decay(omni_wm_cognitive_bridge_t* bridge, float dt) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->focus_active) return NIMCP_SUCCESS;

    /* Apply decay to focus strength */
    float decay = bridge->current_focus.decay_rate * dt;
    bridge->current_focus.focus_strength -= decay;

    if (bridge->current_focus.focus_strength <= 0.0f) {
        /* Focus has decayed completely */
        bridge->current_focus.focus_strength = 0.0f;
        bridge->focus_active = false;
        NIMCP_LOGGING_DEBUG("Attention focus decayed");
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Compute combined salience from components
 */
static nimcp_error_t compute_combined_salience(omni_wm_cognitive_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    wm_cognitive_salience_t* sal = &bridge->current_salience;

    /* Weighted combination */
    float w_novelty = bridge->config.novelty_weight;
    float w_surprise = bridge->config.surprise_weight;
    float w_urgency = bridge->config.urgency_weight;

    /* Normalize weights */
    float w_total = w_novelty + w_surprise + w_urgency;
    if (w_total > 0.0f) {
        w_novelty /= w_total;
        w_surprise /= w_total;
        w_urgency /= w_total;
    }

    sal->combined_salience = w_novelty * sal->novelty +
                             w_surprise * sal->surprise +
                             w_urgency * sal->urgency;

    /* Clamp to [0, 1] */
    if (sal->combined_salience < 0.0f) sal->combined_salience = 0.0f;
    if (sal->combined_salience > 1.0f) sal->combined_salience = 1.0f;

    sal->timestamp_us = get_current_time_us();

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Message Handlers
 * ============================================================================ */

static nimcp_error_t handle_state_prediction(const void* msg, size_t msg_size,
                                              nimcp_bio_promise_t promise, void* user_data) {
    (void)msg_size;
    (void)promise;
    NIMCP_CHECK_THROW(msg, NIMCP_ERROR_NULL_POINTER, "msg is NULL");
    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_cognitive_bridge_t* bridge = (omni_wm_cognitive_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.state_predictions++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_goal_update(const void* msg, size_t msg_size,
                                         nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;
    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_cognitive_bridge_t* bridge = (omni_wm_cognitive_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.goals_received++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_attention_focus(const void* msg, size_t msg_size,
                                             nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;
    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_cognitive_bridge_t* bridge = (omni_wm_cognitive_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.attention_focus_events++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_salience_update(const void* msg, size_t msg_size,
                                             nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;
    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    /* Salience updates processed in main update cycle */
    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_wm_context(const void* msg, size_t msg_size,
                                        nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;
    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    /* WM context processed in main update cycle */
    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_meta_lr_update(const void* msg, size_t msg_size,
                                            nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;
    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    /* Meta-learning updates processed in main update cycle */
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

omni_wm_cognitive_bridge_config_t omni_wm_cognitive_bridge_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_default_config", 0.0f);


    omni_wm_cognitive_bridge_config_t config;

    memset(&config, 0, sizeof(omni_wm_cognitive_bridge_config_t));

    /* General settings */
    config.enable_modulation = true;
    config.sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Goal conditioning settings */
    config.enable_goal_conditioning = true;
    config.max_active_goals = WM_COGNITIVE_MAX_GOALS;
    config.goal_progress_threshold = 0.95f;
    config.goal_priority_decay = 0.01f;

    /* Attention integration settings */
    config.enable_attention_modulation = true;
    config.attention_prediction_boost = 2.0f;
    config.attention_bandwidth_min = 0.1f;
    config.attention_bandwidth_max = 1.0f;
    config.focus_decay_rate = WM_COGNITIVE_DEFAULT_FOCUS_DECAY;

    /* Executive integration settings */
    config.enable_executive_integration = true;
    config.action_consequence_horizon = WM_COGNITIVE_DEFAULT_HORIZON;
    config.plan_evaluation_threshold = 0.7f;
    config.enable_inhibition_check = true;

    /* Salience integration settings */
    config.enable_salience_integration = true;
    config.novelty_weight = 0.3f;
    config.surprise_weight = 0.4f;
    config.urgency_weight = 0.3f;
    config.salience_threshold = 0.5f;

    /* Working memory integration settings */
    config.enable_working_memory_context = true;
    config.max_wm_context_items = WM_COGNITIVE_MAX_WM_ITEMS;
    config.wm_context_decay = NIMCP_ELIGIBILITY_DECAY_DEFAULT;

    /* Meta-learning integration settings */
    config.enable_meta_learning = true;
    config.meta_lr_scale = 1.0f;
    config.adaptation_threshold = 0.3f;

    /* Bio-async settings */
    config.enable_bio_async = true;

    return config;
}

omni_wm_cognitive_bridge_t* omni_wm_cognitive_bridge_create(
    const omni_wm_cognitive_bridge_config_t* config) {

    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_create", 0.0f);


    omni_wm_cognitive_bridge_t* bridge = nimcp_calloc(1, sizeof(omni_wm_cognitive_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate WM cognitive bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_cognitive_bridge_default_config: bridge is NULL");
        return NULL;
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_WM_COGNITIVE_BRIDGE,
                         "wm_cognitive_bridge") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to initialize bridge base");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "omni_wm_cognitive_bridge_default_config: operation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = omni_wm_cognitive_bridge_default_config();
    }

    /* Allocate prediction buffers */
    nimcp_error_t err = allocate_prediction_buffers(bridge);
    if (err != NIMCP_SUCCESS) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to allocate prediction buffers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "omni_wm_cognitive_bridge_default_config: validation failed");
        return NULL;
    }

    /* Allocate WM context cache */
    err = allocate_wm_context_cache(bridge);
    if (err != NIMCP_SUCCESS) {
        free_prediction_buffers(bridge);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to allocate WM context cache");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "omni_wm_cognitive_bridge_default_config: validation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->num_goals = 0;
    bridge->next_goal_id = 1;
    bridge->focus_active = false;
    bridge->last_focus_update_us = 0;
    bridge->last_salience_update_us = 0;

    /* Initialize WM to cognitive effects arrays */
    bridge->wm_to_cognitive.predicted_state = bridge->state_prediction_buffer;
    bridge->wm_to_cognitive.predicted_state_dim = bridge->state_prediction_dim;

    NIMCP_LOGGING_INFO("WM Cognitive Bridge created successfully");
    return bridge;
}

void omni_wm_cognitive_bridge_destroy(omni_wm_cognitive_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        omni_wm_cognitive_bridge_disconnect_bio_async(bridge);
    }

    /* Free WM to cognitive effects dynamic arrays */
    /* Note: predicted_state points to state_prediction_buffer, freed below */

    nimcp_free(bridge->wm_to_cognitive.action_consequences);
    nimcp_free(bridge->wm_to_cognitive.action_values);
    nimcp_free(bridge->wm_to_cognitive.goal_progress_predictions);
    nimcp_free(bridge->wm_to_cognitive.goal_achievement_probs);
    nimcp_free(bridge->wm_to_cognitive.prediction_error_map);

    /* Free cognitive to WM effects dynamic arrays */
    /* Note: active_goals points to bridge->goals, not dynamically allocated */
    /* Note: wm_context points to wm_context_cache, freed below */

    /* Free internal buffers */
    free_prediction_buffers(bridge);
    free_wm_context_cache(bridge);

    /* Cleanup base and free */
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;

    NIMCP_LOGGING_INFO("WM Cognitive Bridge destroyed");
}

nimcp_error_t omni_wm_cognitive_bridge_reset(omni_wm_cognitive_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_reset", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset effects (but preserve buffer pointers) */
    float* pred_state = bridge->wm_to_cognitive.predicted_state;
    uint32_t pred_dim = bridge->wm_to_cognitive.predicted_state_dim;
    memset(&bridge->wm_to_cognitive, 0, sizeof(omni_wm_to_cognitive_effects_t));
    bridge->wm_to_cognitive.predicted_state = pred_state;
    bridge->wm_to_cognitive.predicted_state_dim = pred_dim;

    float* wm_ctx = bridge->cognitive_to_wm.wm_context;
    memset(&bridge->cognitive_to_wm, 0, sizeof(cognitive_to_omni_wm_effects_t));
    bridge->cognitive_to_wm.wm_context = wm_ctx;

    /* Reset goals */
    memset(bridge->goals, 0, sizeof(bridge->goals));
    bridge->num_goals = 0;
    bridge->next_goal_id = 1;

    /* Reset attention state */
    memset(&bridge->current_focus, 0, sizeof(wm_cognitive_focus_t));
    bridge->focus_active = false;
    bridge->last_focus_update_us = 0;

    /* Reset salience state */
    memset(&bridge->current_salience, 0, sizeof(wm_cognitive_salience_t));
    bridge->last_salience_update_us = 0;

    /* Invalidate WM context cache */
    bridge->wm_context_valid = false;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(omni_wm_cognitive_bridge_stats_t));

    /* Reset base bridge (unlocked since we already hold the mutex) */
    bridge_base_reset_unlocked(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_cognitive_bridge_connect(
    omni_wm_cognitive_bridge_t* bridge,
    omni_world_model_t* world_model,
    executive_controller_t* executive,
    working_memory_t* working_memory,
    salience_evaluator_t salience,
    meta_learner_t meta_learner) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_connect", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(world_model, NIMCP_ERROR_INVALID_PARAM, "world_model is NULL (required)");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->world_model = world_model;
    bridge->executive = executive;
    bridge->working_memory = working_memory;
    bridge->salience = salience;
    bridge->meta_learner = meta_learner;

    /* Update base connection state */
    bridge->base.system_a = world_model;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("WM Cognitive Bridge connected: WM=%p, Exec=%p, WorkMem=%p, Sal=%p, Meta=%p",
                       (void*)world_model, (void*)executive, (void*)working_memory,
                       (void*)salience, (void*)meta_learner);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_cognitive_bridge_connect_world_model(
    omni_wm_cognitive_bridge_t* bridge,
    omni_world_model_t* world_model) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_connect_world_model", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(world_model, NIMCP_ERROR_NULL_POINTER, "world_model is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->world_model = world_model;
    bridge->base.system_a = world_model;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_cognitive_bridge_connect_executive(
    omni_wm_cognitive_bridge_t* bridge,
    executive_controller_t* executive) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_connect_executive", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(executive, NIMCP_ERROR_NULL_POINTER, "executive is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->executive = executive;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_cognitive_bridge_connect_working_memory(
    omni_wm_cognitive_bridge_t* bridge,
    working_memory_t* working_memory) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_connect_working_memo", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(working_memory, NIMCP_ERROR_NULL_POINTER, "working_memory is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->working_memory = working_memory;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_cognitive_bridge_connect_salience(
    omni_wm_cognitive_bridge_t* bridge,
    salience_evaluator_t salience) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_connect_salience", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(salience, NIMCP_ERROR_NULL_POINTER, "salience is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->salience = salience;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_cognitive_bridge_connect_meta_learner(
    omni_wm_cognitive_bridge_t* bridge,
    meta_learner_t meta_learner) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_connect_meta_learner", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(meta_learner, NIMCP_ERROR_NULL_POINTER, "meta_learner is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->meta_learner = meta_learner;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_cognitive_bridge_connect_attention(
    omni_wm_cognitive_bridge_t* bridge,
    attention_system_t* attention) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_connect_attention", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(attention, NIMCP_ERROR_NULL_POINTER, "attention is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention = attention;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

bool omni_wm_cognitive_bridge_is_connected(const omni_wm_cognitive_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_is_connected", 0.0f);


    return bridge->world_model != NULL;
}

/* ============================================================================
 * Update API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_cognitive_bridge_update(
    omni_wm_cognitive_bridge_t* bridge,
    float dt) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_update", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Validate dt - negative timesteps are invalid */
    if (dt < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "omni_wm_cognitive_bridge_update: negative dt (%.4f)", dt);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Must be connected (world_model set) to perform updates */
    if (!bridge->world_model) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED,
            "omni_wm_cognitive_bridge_update: bridge not connected (world_model is NULL)");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    if (!bridge->config.enable_modulation) return NIMCP_SUCCESS;

    uint64_t start_time = get_current_time_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Process goal updates */
    nimcp_error_t err = process_goal_updates(bridge, dt);
    if (err != NIMCP_SUCCESS) {
        bridge->stats.errors_goal++;
    }

    /* Process attention decay */
    if (bridge->config.enable_attention_modulation) {
        err = process_attention_decay(bridge, dt);
        if (err != NIMCP_SUCCESS) {
            bridge->stats.errors_attention++;
        }
    }

    /* Compute combined salience */
    if (bridge->config.enable_salience_integration) {
        compute_combined_salience(bridge);
    }

    /* Update effects in both directions */
    update_wm_to_cognitive_effects(bridge);
    update_cognitive_to_wm_effects(bridge);

    /* Update WM context if enabled and stale */
    if (bridge->config.enable_working_memory_context && bridge->working_memory) {
        uint64_t now = get_current_time_us();
        if (!bridge->wm_context_valid ||
            (now - bridge->wm_context_time) > CONTEXT_CACHE_TTL_US) {
            omni_wm_cognitive_bridge_update_wm_context(bridge);
        }
    }

    /* Update timing statistics */
    bridge->stats.total_updates++;
    uint64_t elapsed = get_current_time_us() - start_time;
    bridge->stats.total_processing_time_ms += (double)elapsed / 1000.0;
    bridge->stats.mean_update_time_ms = bridge->stats.total_processing_time_ms /
                                         (double)bridge->stats.total_updates;
    bridge->stats.last_update_time_us = start_time;

    /* Record base update */
    bridge_base_record_update(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Goal Management API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_cognitive_bridge_register_goal(
    omni_wm_cognitive_bridge_t* bridge,
    const float* target_state,
    uint32_t state_dim,
    float priority,
    uint64_t deadline_us,
    const char* description,
    uint32_t* goal_id_out) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_register_goal", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(target_state, NIMCP_ERROR_INVALID_PARAM, "target_state is NULL");
    NIMCP_CHECK_THROW(state_dim > 0, NIMCP_ERROR_INVALID_PARAM, "state_dim must be greater than 0");
    NIMCP_CHECK_THROW(state_dim <= 64, NIMCP_ERROR_INVALID_PARAM, "state_dim exceeds max of 64");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check capacity */
    if (bridge->num_goals >= bridge->config.max_active_goals) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_LOGGING_WARN("Goal capacity reached (%u)", bridge->config.max_active_goals);
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    /* Find empty slot (should be at num_goals after compaction) */
    uint32_t idx = bridge->num_goals;
    wm_cognitive_goal_t* goal = &bridge->goals[idx];

    /* Initialize goal */
    goal->goal_id = bridge->next_goal_id++;
    memcpy(goal->target_state, target_state, state_dim * sizeof(float));
    goal->state_dim = state_dim;
    goal->priority = (priority < 0.0f) ? 0.0f : (priority > 1.0f) ? 1.0f : priority;
    goal->progress = 0.0f;
    goal->deadline_us = deadline_us;
    goal->created_us = get_current_time_us();
    goal->is_active = true;

    if (description) {
        strncpy(goal->description, description, sizeof(goal->description) - 1);
        goal->description[sizeof(goal->description) - 1] = '\0';
    } else {
        snprintf(goal->description, sizeof(goal->description), "Goal_%u", goal->goal_id);
    }

    bridge->num_goals++;
    bridge->stats.goals_received++;

    if (goal_id_out) *goal_id_out = goal->goal_id;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Registered goal: id=%u, priority=%.2f, desc='%s'",
                       goal->goal_id, goal->priority, goal->description);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_cognitive_bridge_update_goal_progress(
    omni_wm_cognitive_bridge_t* bridge,
    uint32_t goal_id,
    float progress) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_update_goal_progress", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    int idx = find_goal_by_id(bridge, goal_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    bridge->goals[idx].progress = (progress < 0.0f) ? 0.0f :
                                   (progress > 1.0f) ? 1.0f : progress;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_cognitive_bridge_goal_achieved(
    omni_wm_cognitive_bridge_t* bridge,
    uint32_t goal_id) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_goal_achieved", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    int idx = find_goal_by_id(bridge, goal_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    bridge->goals[idx].progress = 1.0f;
    bridge->goals[idx].is_active = false;
    bridge->stats.goals_achieved++;

    NIMCP_LOGGING_DEBUG("Goal %u achieved", goal_id);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_cognitive_bridge_goal_failed(
    omni_wm_cognitive_bridge_t* bridge,
    uint32_t goal_id) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_goal_failed", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    int idx = find_goal_by_id(bridge, goal_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    bridge->goals[idx].is_active = false;
    bridge->stats.goals_failed++;

    NIMCP_LOGGING_DEBUG("Goal %u failed", goal_id);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_cognitive_bridge_remove_goal(
    omni_wm_cognitive_bridge_t* bridge,
    uint32_t goal_id) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_remove_goal", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    int idx = find_goal_by_id(bridge, goal_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Remove goal by shifting remaining goals down to compact the array */
    bridge->goals[idx].is_active = false;
    for (uint32_t i = (uint32_t)idx; i + 1 < bridge->num_goals; i++) {
        bridge->goals[i] = bridge->goals[i + 1];
    }
    bridge->num_goals--;

    /* Clear the now-unused last slot */
    memset(&bridge->goals[bridge->num_goals], 0, sizeof(wm_cognitive_goal_t));

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_cognitive_bridge_set_intention(
    omni_wm_cognitive_bridge_t* bridge,
    const float** action_sequence,
    uint32_t action_dim,
    uint32_t sequence_length,
    uint32_t goal_id,
    float confidence) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_set_intention", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(action_sequence, NIMCP_ERROR_INVALID_PARAM, "action_sequence is NULL");
    NIMCP_CHECK_THROW(action_dim > 0, NIMCP_ERROR_INVALID_PARAM, "action_dim must be greater than 0");
    NIMCP_CHECK_THROW(sequence_length > 0, NIMCP_ERROR_INVALID_PARAM, "sequence_length must be greater than 0");
    if (sequence_length > WM_COGNITIVE_MAX_ACTION_SEQUENCE) {
        sequence_length = WM_COGNITIVE_MAX_ACTION_SEQUENCE;
    }
    if (action_dim > 16) action_dim = 16; /* Max action dim in struct */

    nimcp_mutex_lock(bridge->base.mutex);

    wm_cognitive_intention_t* intent = &bridge->cognitive_to_wm.current_intention;

    /* Copy action sequence */
    for (uint32_t i = 0; i < sequence_length; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sequence_length > 256) {
            omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_loop",
                             (float)(i + 1) / (float)sequence_length);
        }

        memcpy(intent->action_sequence[i], action_sequence[i],
               action_dim * sizeof(float));
    }

    intent->action_dim = action_dim;
    intent->sequence_length = sequence_length;
    intent->confidence = (confidence < 0.0f) ? 0.0f :
                          (confidence > 1.0f) ? 1.0f : confidence;
    intent->goal_id = goal_id;
    intent->timestamp_us = get_current_time_us();

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Attention API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_cognitive_bridge_set_attention_focus(
    omni_wm_cognitive_bridge_t* bridge,
    const float* focus_location,
    uint32_t focus_dim,
    float strength,
    float bandwidth) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_set_attention_focus", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(focus_location, NIMCP_ERROR_INVALID_PARAM, "focus_location is NULL");
    NIMCP_CHECK_THROW(focus_dim > 0, NIMCP_ERROR_INVALID_PARAM, "focus_dim must be greater than 0");
    if (focus_dim > 64) focus_dim = 64; /* Max dim in struct */

    nimcp_mutex_lock(bridge->base.mutex);

    wm_cognitive_focus_t* focus = &bridge->current_focus;

    memcpy(focus->focus_location, focus_location, focus_dim * sizeof(float));
    focus->focus_dim = focus_dim;
    focus->focus_strength = (strength < 0.0f) ? 0.0f :
                             (strength > 1.0f) ? 1.0f : strength;
    focus->focus_bandwidth = (bandwidth < bridge->config.attention_bandwidth_min) ?
                              bridge->config.attention_bandwidth_min :
                              (bandwidth > bridge->config.attention_bandwidth_max) ?
                              bridge->config.attention_bandwidth_max : bandwidth;
    focus->focus_start_us = get_current_time_us();
    focus->decay_rate = bridge->config.focus_decay_rate;

    bridge->focus_active = true;
    bridge->last_focus_update_us = focus->focus_start_us;
    bridge->stats.attention_focus_events++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_cognitive_bridge_attention_shift(
    omni_wm_cognitive_bridge_t* bridge,
    const float* new_focus_location,
    uint32_t focus_dim,
    float new_strength) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_attention_shift", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Record shift */
    bridge->stats.attention_shifts++;

    /* Compute shift duration if previous focus existed */
    if (bridge->focus_active && bridge->current_focus.focus_start_us > 0) {
        uint64_t duration = get_current_time_us() - bridge->current_focus.focus_start_us;
        float duration_ms = (float)duration / 1000.0f;

        /* Update running average */
        float alpha = 0.1f;
        bridge->stats.mean_focus_duration_ms =
            alpha * duration_ms +
            (1.0f - alpha) * bridge->stats.mean_focus_duration_ms;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Set new focus (use default bandwidth) */
    return omni_wm_cognitive_bridge_set_attention_focus(
        bridge, new_focus_location, focus_dim, new_strength,
        (bridge->config.attention_bandwidth_min + bridge->config.attention_bandwidth_max) / 2.0f);
}

nimcp_error_t omni_wm_cognitive_bridge_clear_attention(
    omni_wm_cognitive_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_clear_attention", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    memset(&bridge->current_focus, 0, sizeof(wm_cognitive_focus_t));
    bridge->focus_active = false;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Prediction API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_cognitive_bridge_predict_state(
    omni_wm_cognitive_bridge_t* bridge,
    const float* current_state,
    uint32_t state_dim,
    const float* action,
    uint32_t action_dim,
    float* predicted_state_out,
    float* confidence_out) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_predict_state", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(predicted_state_out, NIMCP_ERROR_INVALID_PARAM, "predicted_state_out is NULL");
    NIMCP_CHECK_THROW(state_dim > 0, NIMCP_ERROR_INVALID_PARAM, "state_dim must be greater than 0");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->stats.state_predictions++;

    /* In full implementation, would:
     * 1. Query world model for state prediction
     * 2. Apply goal conditioning if enabled
     * 3. Apply attention focus boost if applicable
     * For now, return placeholder prediction */

    /* Copy current state as baseline if provided */
    uint32_t copy_dim = state_dim < bridge->state_prediction_dim ?
                        state_dim : bridge->state_prediction_dim;
    if (current_state) {
        memcpy(predicted_state_out, current_state, copy_dim * sizeof(float));
    } else {
        memset(predicted_state_out, 0, copy_dim * sizeof(float));
    }

    /* Apply simple dynamics placeholder (slight drift) */
    if (action && action_dim > 0) {
        for (uint32_t i = 0; i < copy_dim && i < action_dim; i++) {
            predicted_state_out[i] += action[i] * 0.1f;
        }
    }

    /* Set confidence based on whether WM is connected */
    float conf = bridge->world_model ? 0.8f : 0.3f;

    /* Boost confidence for attended regions */
    if (bridge->focus_active && bridge->config.enable_attention_modulation) {
        conf *= bridge->config.attention_prediction_boost;
        if (conf > 1.0f) conf = 1.0f;
    }

    if (confidence_out) *confidence_out = conf;

    /* Update mean confidence statistic */
    float alpha = 0.1f;
    bridge->stats.mean_prediction_confidence =
        alpha * conf + (1.0f - alpha) * bridge->stats.mean_prediction_confidence;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_cognitive_bridge_predict_action_consequences(
    omni_wm_cognitive_bridge_t* bridge,
    const float* current_state,
    uint32_t state_dim,
    const float** actions,
    uint32_t action_dim,
    uint32_t num_actions,
    float** consequences_out,
    float* values_out) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_predict_action_conse", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(actions, NIMCP_ERROR_NULL_POINTER, "actions is NULL");
    NIMCP_CHECK_THROW(consequences_out, NIMCP_ERROR_NULL_POINTER, "consequences_out is NULL");
    NIMCP_CHECK_THROW(state_dim > 0, NIMCP_ERROR_INVALID_PARAM, "state_dim must be greater than 0");
    NIMCP_CHECK_THROW(num_actions > 0, NIMCP_ERROR_INVALID_PARAM, "num_actions must be greater than 0");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->stats.action_consequence_preds += num_actions;

    /* Predict consequence for each action */
    for (uint32_t i = 0; i < num_actions && i < bridge->action_consequence_capacity; i++) {
        if (!consequences_out[i]) continue;

        /* Copy current state */
        uint32_t copy_dim = state_dim < bridge->state_prediction_dim ?
                            state_dim : bridge->state_prediction_dim;
        if (current_state) {
            memcpy(consequences_out[i], current_state, copy_dim * sizeof(float));
        } else {
            memset(consequences_out[i], 0, copy_dim * sizeof(float));
        }

        /* Apply action effect (placeholder) */
        if (actions[i] && action_dim > 0) {
            for (uint32_t j = 0; j < copy_dim && j < action_dim; j++) {
                consequences_out[i][j] += actions[i][j] * 0.1f;
            }
        }

        /* Compute value if requested (placeholder: negative distance to any goal) */
        if (values_out) {
            float best_value = -1000.0f;
            for (uint32_t g = 0; g < bridge->num_goals; g++) {
                /* Phase 8: Loop progress heartbeat */
                if ((g & 0xFF) == 0 && bridge->num_goals > 256) {
                    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_loop",
                                     (float)(g + 1) / (float)bridge->num_goals);
                }

                if (!bridge->goals[g].is_active) continue;

                /* Compute distance to goal */
                float dist = 0.0f;
                uint32_t gdim = bridge->goals[g].state_dim;
                for (uint32_t k = 0; k < copy_dim && k < gdim; k++) {
                    float diff = consequences_out[i][k] - bridge->goals[g].target_state[k];
                    dist += diff * diff;
                }
                dist = sqrtf(dist);

                /* Value is negative distance (closer = better) */
                float value = -dist * bridge->goals[g].priority;
                if (value > best_value) best_value = value;
            }
            values_out[i] = best_value;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_cognitive_bridge_evaluate_plan(
    omni_wm_cognitive_bridge_t* bridge,
    const float* initial_state,
    uint32_t state_dim,
    const float** action_sequence,
    uint32_t action_dim,
    uint32_t sequence_length,
    uint32_t goal_id,
    float* expected_value_out,
    float* success_prob_out) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_evaluate_plan", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(initial_state, NIMCP_ERROR_INVALID_PARAM, "initial_state is NULL");
    NIMCP_CHECK_THROW(action_sequence, NIMCP_ERROR_INVALID_PARAM, "action_sequence is NULL");
    NIMCP_CHECK_THROW(state_dim > 0, NIMCP_ERROR_INVALID_PARAM, "state_dim must be greater than 0");
    NIMCP_CHECK_THROW(sequence_length > 0, NIMCP_ERROR_INVALID_PARAM, "sequence_length must be greater than 0");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->stats.plan_evaluations++;

    /* Simulate plan execution */
    float* current = nimcp_calloc(state_dim, sizeof(float));
    if (!current) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }
    memcpy(current, initial_state, state_dim * sizeof(float));

    float total_value = 0.0f;
    float cumulative_confidence = 1.0f;

    /* Find goal if specified */
    const wm_cognitive_goal_t* goal = NULL;
    if (goal_id > 0) {
        int idx = find_goal_by_id(bridge, goal_id);
        if (idx >= 0) goal = &bridge->goals[idx];
    }

    /* Simulate each step */
    for (uint32_t step = 0; step < sequence_length; step++) {
        /* Phase 8: Loop progress heartbeat */
        if ((step & 0xFF) == 0 && sequence_length > 256) {
            omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_loop",
                             (float)(step + 1) / (float)sequence_length);
        }

        /* Apply action (placeholder dynamics) */
        if (action_sequence[step] && action_dim > 0) {
            for (uint32_t i = 0; i < state_dim && i < action_dim; i++) {
                current[i] += action_sequence[step][i] * 0.1f;
            }
        }

        /* Compute step value (distance to goal if specified) */
        if (goal) {
            float dist = 0.0f;
            for (uint32_t i = 0; i < state_dim && i < goal->state_dim; i++) {
                float diff = current[i] - goal->target_state[i];
                dist += diff * diff;
            }
            dist = sqrtf(dist);
            total_value += -dist * goal->priority;
        }

        /* Confidence decays with horizon */
        cumulative_confidence *= 0.95f;
    }

    /* Compute final metrics */
    if (expected_value_out) {
        *expected_value_out = total_value / (float)sequence_length;
    }

    if (success_prob_out) {
        /* Success probability based on final distance to goal */
        float success = 0.5f;
        if (goal) {
            float final_dist = 0.0f;
            for (uint32_t i = 0; i < state_dim && i < goal->state_dim; i++) {
                float diff = current[i] - goal->target_state[i];
                final_dist += diff * diff;
            }
            final_dist = sqrtf(final_dist);

            /* Convert distance to probability (closer = higher prob) */
            success = expf(-final_dist);
        }
        *success_prob_out = success * cumulative_confidence;
    }

    /* Update mean plan confidence statistic */
    float plan_conf = cumulative_confidence;
    float alpha = 0.1f;
    bridge->stats.mean_plan_confidence =
        alpha * plan_conf + (1.0f - alpha) * bridge->stats.mean_plan_confidence;

    nimcp_free(current);
    current = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Salience API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_cognitive_bridge_update_salience(
    omni_wm_cognitive_bridge_t* bridge,
    float novelty,
    float surprise,
    float urgency) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_update_salience", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->current_salience.novelty = (novelty < 0.0f) ? 0.0f :
                                        (novelty > 1.0f) ? 1.0f : novelty;
    bridge->current_salience.surprise = (surprise < 0.0f) ? 0.0f :
                                         (surprise > 1.0f) ? 1.0f : surprise;
    bridge->current_salience.urgency = (urgency < 0.0f) ? 0.0f :
                                        (urgency > 1.0f) ? 1.0f : urgency;

    /* Compute combined salience */
    compute_combined_salience(bridge);

    /* Update statistics */
    if (novelty > bridge->config.salience_threshold) {
        bridge->stats.high_novelty_events++;
    }
    if (surprise > bridge->config.salience_threshold) {
        bridge->stats.high_surprise_events++;
    }
    if (urgency > bridge->config.salience_threshold) {
        bridge->stats.high_urgency_events++;
    }

    /* Update mean salience */
    float alpha = 0.1f;
    bridge->stats.mean_salience =
        alpha * bridge->current_salience.combined_salience +
        (1.0f - alpha) * bridge->stats.mean_salience;

    bridge->last_salience_update_us = get_current_time_us();

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_cognitive_bridge_get_prediction_error_map(
    omni_wm_cognitive_bridge_t* bridge,
    float* pe_map_out,
    uint32_t map_dim,
    float* max_pe_out) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_get_prediction_error", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(pe_map_out, NIMCP_ERROR_INVALID_PARAM, "pe_map_out is NULL");
    NIMCP_CHECK_THROW(map_dim > 0, NIMCP_ERROR_INVALID_PARAM, "map_dim must be greater than 0");

    nimcp_mutex_lock(bridge->base.mutex);

    /* In full implementation, would query WM for actual PE map.
     * For now, generate placeholder based on current state */

    float max_pe = 0.0f;

    for (uint32_t i = 0; i < map_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && map_dim > 256) {
            omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_loop",
                             (float)(i + 1) / (float)map_dim);
        }

        /* Placeholder: uniform low PE with some random variation */
        pe_map_out[i] = 0.1f + 0.05f * ((float)(i % 10) / 10.0f);

        if (pe_map_out[i] > max_pe) {
            max_pe = pe_map_out[i];
        }
    }

    /* Boost PE in regions away from attention focus */
    if (bridge->focus_active && bridge->config.enable_attention_modulation) {
        for (uint32_t i = 0; i < map_dim && i < bridge->current_focus.focus_dim; i++) {
            /* Distance from focus (simplified) */
            float dist_from_focus = fabsf(bridge->current_focus.focus_location[i]);
            pe_map_out[i] *= (1.0f + dist_from_focus);
            if (pe_map_out[i] > max_pe) {
                max_pe = pe_map_out[i];
            }
        }
    }

    if (max_pe_out) *max_pe_out = max_pe;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Working Memory Context API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_cognitive_bridge_update_wm_context(
    omni_wm_cognitive_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_update_wm_context", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* In full implementation, would:
     * 1. Query working_memory for current items
     * 2. Aggregate into context vector
     * 3. Apply decay weighting
     * For now, mark as valid with placeholder data */

    memset(bridge->wm_context_cache, 0, bridge->wm_context_cache_dim * sizeof(float));
    bridge->wm_context_valid = true;
    bridge->wm_context_time = get_current_time_us();

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_cognitive_bridge_get_wm_context(
    const omni_wm_cognitive_bridge_t* bridge,
    float* context_out,
    uint32_t context_dim,
    float* utilization_out) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_get_wm_context", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(context_out, NIMCP_ERROR_INVALID_PARAM, "context_out is NULL");
    NIMCP_CHECK_THROW(context_dim > 0, NIMCP_ERROR_INVALID_PARAM, "context_dim must be greater than 0");

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t copy_dim = context_dim < bridge->wm_context_cache_dim ?
                        context_dim : bridge->wm_context_cache_dim;

    if (bridge->wm_context_valid && bridge->wm_context_cache) {
        memcpy(context_out, bridge->wm_context_cache, copy_dim * sizeof(float));
    } else {
        memset(context_out, 0, context_dim * sizeof(float));
    }

    if (utilization_out) {
        *utilization_out = bridge->cognitive_to_wm.wm_utilization;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Meta-Learning API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_cognitive_bridge_get_recommended_lr(
    const omni_wm_cognitive_bridge_t* bridge,
    float* recommended_lr_out) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_get_recommended_lr", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(recommended_lr_out, NIMCP_ERROR_INVALID_PARAM, "recommended_lr_out is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    *recommended_lr_out = bridge->wm_to_cognitive.recommended_lr *
                          bridge->config.meta_lr_scale;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_cognitive_bridge_trigger_adaptation(
    omni_wm_cognitive_bridge_t* bridge,
    float prediction_error) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_trigger_adaptation", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if PE exceeds adaptation threshold */
    if (prediction_error >= bridge->config.adaptation_threshold) {
        bridge->stats.adaptation_triggers++;

        /* In full implementation, would notify meta_learner to begin adaptation */
        NIMCP_LOGGING_DEBUG("Triggered adaptation: PE=%.3f >= threshold=%.3f",
                           prediction_error, bridge->config.adaptation_threshold);
    }

    /* Update mean prediction error */
    float alpha = 0.1f;
    bridge->stats.mean_prediction_error =
        alpha * prediction_error + (1.0f - alpha) * bridge->stats.mean_prediction_error;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

const omni_wm_to_cognitive_effects_t* omni_wm_cognitive_bridge_get_wm_effects(
    const omni_wm_cognitive_bridge_t* bridge) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_get_wm_effects", 0.0f);


    return &bridge->wm_to_cognitive;
}

const cognitive_to_omni_wm_effects_t* omni_wm_cognitive_bridge_get_cognitive_effects(
    const omni_wm_cognitive_bridge_t* bridge) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_get_cognitive_effect", 0.0f);


    return &bridge->cognitive_to_wm;
}

nimcp_error_t omni_wm_cognitive_bridge_get_stats(
    const omni_wm_cognitive_bridge_t* bridge,
    omni_wm_cognitive_bridge_stats_t* stats) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_get_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_cognitive_bridge_reset_stats(
    omni_wm_cognitive_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_reset_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(omni_wm_cognitive_bridge_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

const wm_cognitive_goal_t* omni_wm_cognitive_bridge_get_goal(
    const omni_wm_cognitive_bridge_t* bridge,
    uint32_t goal_id) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_get_goal", 0.0f);


    int idx = find_goal_by_id(bridge, goal_id);
    if (idx < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: validation failed");
        return NULL;
    }

    return &bridge->goals[idx];
}

uint32_t omni_wm_cognitive_bridge_get_num_goals(
    const omni_wm_cognitive_bridge_t* bridge) {

    if (!bridge) return 0;
    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_get_num_goals", 0.0f);


    return bridge->num_goals;
}

/* ============================================================================
 * Bio-Async API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_cognitive_bridge_connect_bio_async(
    omni_wm_cognitive_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_connect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_bio_async) return NIMCP_SUCCESS;
    if (bridge->base.bio_async_enabled) return NIMCP_SUCCESS; /* Already connected */

    /* Check if router is initialized */
    if (!bio_router_is_initialized()) {
        NIMCP_LOGGING_DEBUG("Bio-async router not initialized, skipping registration");
        return NIMCP_SUCCESS;
    }

    /* Register module with router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_WM_COGNITIVE_BRIDGE,
        .module_name = "wm_cognitive_bridge",
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
                                BIO_MSG_WM_COGNITIVE_STATE_PRED,
                                handle_state_prediction);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_COGNITIVE_GOAL_UPDATE,
                                handle_goal_update);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_ATTENTION_FOCUS,
                                handle_attention_focus);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_COGNITIVE_SALIENCE,
                                handle_salience_update);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_COGNITIVE_WORKING_MEM,
                                handle_wm_context);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_COGNITIVE_META_LEARNING,
                                handle_meta_lr_update);

    bridge->base.bio_async_enabled = true;
    NIMCP_LOGGING_INFO("WM Cognitive Bridge connected to bio-async router");

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_cognitive_bridge_disconnect_bio_async(
    omni_wm_cognitive_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_disconnect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->base.bio_async_enabled) return NIMCP_SUCCESS;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("WM Cognitive Bridge disconnected from bio-async router");

    return NIMCP_SUCCESS;
}

bool omni_wm_cognitive_bridge_is_bio_async_connected(
    const omni_wm_cognitive_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_is_bio_async_connect", 0.0f);


    return bridge_base_is_bio_async_connected(bridge ? &bridge->base : NULL);
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

const char* omni_wm_cognitive_msg_type_to_string(omni_wm_cognitive_msg_type_t msg_type) {
    /* Message types are defined in nimcp_bio_messages.h */
    switch (msg_type) {
        case BIO_MSG_WM_COGNITIVE_STATE_PRED:
            return "STATE_PRED";
        case BIO_MSG_WM_COGNITIVE_GOAL_UPDATE:
            return "GOAL_UPDATE";
        case BIO_MSG_WM_COGNITIVE_ACTION_CONSEQUENCE:
            return "ACTION_CONSEQUENCE";
        case BIO_MSG_WM_ATTENTION_FOCUS:
            return "ATTENTION_FOCUS";
        case BIO_MSG_WM_COGNITIVE_WORKING_MEM:
            return "WORKING_MEM";
        case BIO_MSG_WM_COGNITIVE_SALIENCE:
            return "SALIENCE";
        case BIO_MSG_WM_COGNITIVE_META_LEARNING:
            return "META_LEARNING";
        default:
            return "UNKNOWN";
    }
}

nimcp_error_t omni_wm_cognitive_bridge_validate_config(
    const omni_wm_cognitive_bridge_config_t* config) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_cognitive_bridge_heartbeat("omni_wm_cogn_validate_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* Validate sensitivity range */
    if (config->sensitivity < 0.5f || config->sensitivity > 2.0f) {
        NIMCP_LOGGING_WARN("Sensitivity %.2f out of range [0.5, 2.0]",
                          config->sensitivity);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate goal settings */
    if (config->enable_goal_conditioning) {
        if (config->max_active_goals == 0 ||
            config->max_active_goals > WM_COGNITIVE_MAX_GOALS) {
            NIMCP_LOGGING_WARN("Invalid max_active_goals: %u",
                              config->max_active_goals);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->goal_progress_threshold < 0.0f ||
            config->goal_progress_threshold > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid goal_progress_threshold: %.2f",
                              config->goal_progress_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate attention settings */
    if (config->enable_attention_modulation) {
        if (config->attention_prediction_boost < 1.0f ||
            config->attention_prediction_boost > 3.0f) {
            NIMCP_LOGGING_WARN("Invalid attention_prediction_boost: %.2f",
                              config->attention_prediction_boost);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->attention_bandwidth_min < 0.0f ||
            config->attention_bandwidth_max < config->attention_bandwidth_min) {
            NIMCP_LOGGING_WARN("Invalid attention bandwidth range: [%.2f, %.2f]",
                              config->attention_bandwidth_min,
                              config->attention_bandwidth_max);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate salience weights */
    if (config->enable_salience_integration) {
        float w_sum = config->novelty_weight + config->surprise_weight +
                      config->urgency_weight;
        if (w_sum <= 0.0f) {
            NIMCP_LOGGING_WARN("Salience weights sum to %.2f (must be > 0)", w_sum);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->salience_threshold < 0.0f || config->salience_threshold > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid salience_threshold: %.2f",
                              config->salience_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate working memory settings */
    if (config->enable_working_memory_context) {
        if (config->max_wm_context_items == 0 ||
            config->max_wm_context_items > WM_COGNITIVE_MAX_WM_ITEMS) {
            NIMCP_LOGGING_WARN("Invalid max_wm_context_items: %u",
                              config->max_wm_context_items);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate meta-learning settings */
    if (config->enable_meta_learning) {
        if (config->meta_lr_scale <= 0.0f) {
            NIMCP_LOGGING_WARN("Invalid meta_lr_scale: %.4f",
                              config->meta_lr_scale);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->adaptation_threshold < 0.0f ||
            config->adaptation_threshold > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid adaptation_threshold: %.2f",
                              config->adaptation_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    return NIMCP_SUCCESS;
}
