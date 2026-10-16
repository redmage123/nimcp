/**
 * @file nimcp_prefrontal_adapter.c
 * @brief Implementation of Prefrontal Cortex brain adapter
 *
 * WHAT: Unified adapter connecting prefrontal cortex sub-modules to the brain system
 * WHY:  Enable seamless integration with cognitive layers, executive functions, decision-making
 * HOW:  Orchestrates executive function, planning, inhibition, and working memory
 *
 * @version Phase PFC-1: Prefrontal Cortex Brain Integration
 * @date 2025-12-30
 */

#include "core/brain/regions/prefrontal/nimcp_prefrontal_adapter.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(prefrontal_adapter, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define PFC_LOG_MODULE "PREFRONTAL"

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Working memory slot for goal maintenance
 */
typedef struct {
    uint32_t item_id;                   /**< Item identifier */
    float priority;                     /**< Current priority */
    float activation;                   /**< Decay-based activation */
    uint32_t goal_id;                   /**< Associated goal */
    double timestamp;                   /**< When added */
} pfc_wm_slot_t;

/**
 * @brief Rule representation for cognitive flexibility
 */
typedef struct {
    uint32_t rule_id;                   /**< Rule identifier */
    uint32_t task_id;                   /**< Task this rule applies to */
    float* context_weights;             /**< Context feature weights */
    uint32_t context_size;              /**< Size of context */
    float confidence;                   /**< Rule confidence */
    uint32_t usage_count;               /**< Times rule was applied */
} pfc_rule_t;

/**
 * @brief Internal adapter structure
 */
struct prefrontal_adapter {
    /* Configuration */
    prefrontal_config_t config;

    /* Goal management */
    prefrontal_goal_t* goals;
    uint32_t goal_capacity;
    uint32_t goal_count;
    uint32_t next_goal_id;

    /* Action planning */
    action_plan_t* plans;
    uint32_t plan_capacity;
    uint32_t plan_count;

    /* Decision evaluation */
    decision_option_t* eval_buffer;
    uint32_t eval_capacity;

    /* Working memory */
    pfc_wm_slot_t* working_memory;
    uint32_t wm_count;
    uint32_t wm_head;

    /* Rule learning */
    pfc_rule_t* rules;
    uint32_t rule_capacity;
    uint32_t rule_count;
    uint32_t current_task_id;

    /* Inhibitory control */
    float inhibitory_strength;          /**< Current inhibitory capacity */
    float inhibitory_fatigue;           /**< Accumulated fatigue */
    uint64_t last_inhibition_time_us;

    /* Callbacks */
    prefrontal_goal_callback_t goal_callback;
    void* goal_callback_data;
    prefrontal_decision_callback_t decision_callback;
    void* decision_callback_data;
    prefrontal_inhibition_callback_t inhibition_callback;
    void* inhibition_callback_data;
    prefrontal_action_callback_t action_callback;
    void* action_callback_data;

    /* State */
    prefrontal_status_t status;
    prefrontal_error_t last_error;
    double current_time_ms;

    /* Memory pool for hot-path allocations */
    memory_pool_t action_pool;

    /* Bio-async communication context */
    bio_module_context_t bio_ctx;
    nimcp_bio_channel_type_t default_channel;

    /* Statistics */
    prefrontal_stats_t stats;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Set error state
 */
static void set_error(prefrontal_adapter_t* adapter, prefrontal_error_t error) {
    if (!adapter) return;
    adapter->last_error = error;
    if (error != PREFRONTAL_ERROR_NONE) {
        adapter->status = PREFRONTAL_STATUS_ERROR;
        LOG_ERROR("[%s] Error set: %d", PFC_LOG_MODULE, error);
    }
}

/**
 * @brief Find goal by ID
 */
static prefrontal_goal_t* find_goal(prefrontal_adapter_t* adapter, uint32_t goal_id) {
    if (!adapter || goal_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_goal: adapter is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < adapter->goal_count; i++) {
        if (adapter->goals[i].goal_id == goal_id) {
            return &adapter->goals[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_goal: validation failed");
    return NULL;
}

/**
 * @brief Find plan by goal ID
 */
static action_plan_t* find_plan(prefrontal_adapter_t* adapter, uint32_t goal_id) {
    if (!adapter || goal_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_plan: adapter is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < adapter->plan_count; i++) {
        if (adapter->plans[i].goal_id == goal_id) {
            return &adapter->plans[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_plan: validation failed");
    return NULL;
}

/**
 * @brief Emit goal state change callback
 */
static void emit_goal_callback(prefrontal_adapter_t* adapter,
                                const prefrontal_goal_t* goal,
                                goal_state_t old_state,
                                goal_state_t new_state) {
    if (adapter->goal_callback) {
        adapter->goal_callback(goal, old_state, new_state, adapter->goal_callback_data);
    }
}

/**
 * @brief Emit decision callback
 */
static void emit_decision_callback(prefrontal_adapter_t* adapter,
                                    const decision_result_t* result) {
    if (adapter->decision_callback) {
        adapter->decision_callback(result, adapter->decision_callback_data);
    }
}

/**
 * @brief Emit inhibition callback
 */
static void emit_inhibition_callback(prefrontal_adapter_t* adapter,
                                      const impulse_record_t* record) {
    if (adapter->inhibition_callback) {
        adapter->inhibition_callback(record, adapter->inhibition_callback_data);
    }
}

/**
 * @brief Compute expected utility
 */
static float compute_utility(const decision_option_t* option, float risk_aversion) {
    /* Expected utility = probability * value - risk_aversion * risk * cost */
    float expected_value = option->probability * option->action.expected_value;
    float risk_penalty = risk_aversion * option->risk * option->action.cost;
    return expected_value - risk_penalty - option->action.cost;
}

/**
 * @brief Update inhibitory fatigue
 */
static void update_inhibitory_fatigue(prefrontal_adapter_t* adapter, float effort) {
    /* Inhibition depletes over time (ego depletion model) */
    adapter->inhibitory_fatigue += effort * 0.1f;
    if (adapter->inhibitory_fatigue > 1.0f) {
        adapter->inhibitory_fatigue = 1.0f;
    }

    /* Fatigue recovers slowly */
    float recovery = 0.001f;  /* Per update */
    adapter->inhibitory_fatigue -= recovery;
    if (adapter->inhibitory_fatigue < 0.0f) {
        adapter->inhibitory_fatigue = 0.0f;
    }
}

/*=============================================================================
 * BIO-ASYNC MESSAGE HANDLERS (Forward declarations)
 *===========================================================================*/

static nimcp_error_t handle_goal_eval_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t handle_decision_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t handle_inhibition_check(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

/*=============================================================================
 * KG-DRIVEN WIRING CALLBACK
 *===========================================================================*/

/**
 * @brief KG-driven wiring handler callback
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 *
 * @param ctx Bio-async module context
 * @param message_types Array of message types to handle (from KG)
 * @param message_count Number of message types
 * @param user_data Prefrontal adapter pointer
 * @return 0 on success, -1 on error
 */
static int prefrontal_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    LOG_INFO("[%s] prefrontal_wiring_handler_callback: registering %u handlers from KG",
             PFC_LOG_MODULE, message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_GOAL_EVAL_REQUEST:
                bio_router_register_handler(ctx, message_types[i], handle_goal_eval_request);
                LOG_DEBUG("[%s]   Registered handler for BIO_MSG_GOAL_EVAL_REQUEST", PFC_LOG_MODULE);
                break;

            case BIO_MSG_DECISION_REQUEST:
                bio_router_register_handler(ctx, message_types[i], handle_decision_request);
                LOG_DEBUG("[%s]   Registered handler for BIO_MSG_DECISION_REQUEST", PFC_LOG_MODULE);
                break;

            case BIO_MSG_INHIBITION_CHECK:
                bio_router_register_handler(ctx, message_types[i], handle_inhibition_check);
                LOG_DEBUG("[%s]   Registered handler for BIO_MSG_INHIBITION_CHECK", PFC_LOG_MODULE);
                break;

            default:
                LOG_DEBUG("[%s]   Unknown message type %u - skipping", PFC_LOG_MODULE, message_types[i]);
                break;
        }
    }

    return 0;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

prefrontal_config_t prefrontal_default_config(void) {
    prefrontal_config_t config;
    memset(&config, 0, sizeof(config));

    config.max_goals = PREFRONTAL_DEFAULT_MAX_GOALS;
    config.max_actions = PREFRONTAL_DEFAULT_MAX_ACTIONS;
    config.max_options = 16;
    config.working_memory_slots = PREFRONTAL_DEFAULT_WORKING_MEMORY_SLOTS;
    config.enable_working_memory = true;
    config.planning_horizon = PREFRONTAL_DEFAULT_PLANNING_HORIZON;
    config.planning_discount = 0.95f;  /* Gamma for future value */
    config.enable_hierarchical_planning = true;
    config.decision_timeout_ms = PREFRONTAL_DEFAULT_DECISION_TIMEOUT_MS;
    config.decision_threshold = 0.6f;
    config.enable_value_learning = true;
    config.inhibition_threshold = PREFRONTAL_DEFAULT_INHIBITION_THRESHOLD;
    config.impulse_decay_rate = 0.1f;
    config.enable_conflict_monitoring = true;
    config.switch_cost = 0.15f;
    config.enable_rule_learning = true;
    config.enable_events = true;
    config.enable_training = false;
    config.learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    config.exploration_rate = NIMCP_EXPLORATION_RATE_DEFAULT;
    /* Bio-async: enabled by default, use dopamine for reward-based processing */
    config.enable_bio_async = true;
    config.default_channel = BIO_CHANNEL_DOPAMINE;

    return config;
}

prefrontal_adapter_t* prefrontal_create(const prefrontal_config_t* config) {
    LOG_INFO("[%s] Creating prefrontal cortex adapter", PFC_LOG_MODULE);

    prefrontal_adapter_t* adapter = (prefrontal_adapter_t*)nimcp_calloc(1, sizeof(prefrontal_adapter_t));
    if (!adapter) {
        LOG_ERROR("[%s] Failed to allocate adapter memory", PFC_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "prefrontal_create: adapter is NULL");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        adapter->config = *config;
        LOG_DEBUG("[%s] Using provided configuration", PFC_LOG_MODULE);
    } else {
        adapter->config = prefrontal_default_config();
        LOG_DEBUG("[%s] Using default configuration", PFC_LOG_MODULE);
    }

    /* Allocate goals */
    adapter->goal_capacity = adapter->config.max_goals;
    adapter->goals = (prefrontal_goal_t*)nimcp_calloc(
        adapter->goal_capacity, sizeof(prefrontal_goal_t));
    if (!adapter->goals) {
        LOG_ERROR("[%s] Failed to allocate goals", PFC_LOG_MODULE);
        prefrontal_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "prefrontal_create: adapter->goals is NULL");
        return NULL;
    }
    adapter->next_goal_id = 1;

    /* Allocate plans */
    adapter->plan_capacity = adapter->config.max_goals;
    adapter->plans = (action_plan_t*)nimcp_calloc(
        adapter->plan_capacity, sizeof(action_plan_t));
    if (!adapter->plans) {
        LOG_ERROR("[%s] Failed to allocate plans", PFC_LOG_MODULE);
        prefrontal_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "prefrontal_create: adapter->plans is NULL");
        return NULL;
    }

    /* Allocate action arrays for each plan */
    for (uint32_t i = 0; i < adapter->plan_capacity; i++) {
        adapter->plans[i].actions = (prefrontal_action_t*)nimcp_calloc(
            adapter->config.max_actions, sizeof(prefrontal_action_t));
        if (!adapter->plans[i].actions) {
            LOG_ERROR("[%s] Failed to allocate plan actions", PFC_LOG_MODULE);
            prefrontal_destroy(adapter);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "prefrontal_create: adapter->plans is NULL");
            return NULL;
        }
    }

    /* Allocate decision evaluation buffer */
    adapter->eval_capacity = adapter->config.max_options;
    adapter->eval_buffer = (decision_option_t*)nimcp_calloc(
        adapter->eval_capacity, sizeof(decision_option_t));
    if (!adapter->eval_buffer) {
        LOG_ERROR("[%s] Failed to allocate evaluation buffer", PFC_LOG_MODULE);
        prefrontal_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "prefrontal_create: adapter->eval_buffer is NULL");
        return NULL;
    }

    /* Allocate working memory */
    if (adapter->config.enable_working_memory) {
        adapter->working_memory = (pfc_wm_slot_t*)nimcp_calloc(
            adapter->config.working_memory_slots, sizeof(pfc_wm_slot_t));
        if (!adapter->working_memory) {
            LOG_ERROR("[%s] Failed to allocate working memory", PFC_LOG_MODULE);
            prefrontal_destroy(adapter);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "prefrontal_create: adapter->working_memory is NULL");
            return NULL;
        }
    }

    /* Allocate rule storage */
    if (adapter->config.enable_rule_learning) {
        adapter->rule_capacity = 32;  /* Default rule capacity */
        adapter->rules = (pfc_rule_t*)nimcp_calloc(
            adapter->rule_capacity, sizeof(pfc_rule_t));
        if (!adapter->rules) {
            LOG_ERROR("[%s] Failed to allocate rules", PFC_LOG_MODULE);
            prefrontal_destroy(adapter);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "prefrontal_create: adapter->rules is NULL");
            return NULL;
        }
    }

    /* Initialize memory pool for hot-path allocations */
    memory_pool_config_t pool_config = {
        .block_size = adapter->config.max_actions * sizeof(prefrontal_action_t),
        .num_blocks = 4,
        .alignment = 16,
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    adapter->action_pool = memory_pool_create(&pool_config);
    if (!adapter->action_pool) {
        LOG_ERROR("[%s] Failed to create action memory pool", PFC_LOG_MODULE);
        prefrontal_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "prefrontal_create: adapter->action_pool is NULL");
        return NULL;
    }

    /* Initialize inhibitory control */
    adapter->inhibitory_strength = 1.0f;
    adapter->inhibitory_fatigue = 0.0f;

    /* Initialize bio-async communication */
    adapter->bio_ctx = NULL;
    adapter->default_channel = adapter->config.default_channel;

    if (adapter->config.enable_bio_async && bio_router_is_initialized()) {
        LOG_DEBUG("[%s] Registering with bio-async router", PFC_LOG_MODULE);

        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_PREFRONTAL,
            .module_name = "prefrontal_cortex",
            .inbox_capacity = 64,
            .user_data = adapter
        };

        adapter->bio_ctx = bio_router_register_module(&bio_info);
        if (adapter->bio_ctx) {
            /* KG-Driven Wiring: Register callback for orchestrator to invoke
             * When orchestrator starts, it discovers HANDLES_MESSAGE relations
             * from the KG and invokes this callback with the message types */
            nimcp_error_t cb_result = bio_router_register_wiring_callback(
                BIO_MODULE_PREFRONTAL,
                (void*)prefrontal_wiring_handler_callback,
                adapter
            );

            if (cb_result != NIMCP_SUCCESS) {
                /* Fallback: Direct registration if orchestrator not available
                 * This ensures backward compatibility with non-KG systems */
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_GOAL_EVAL_REQUEST, handle_goal_eval_request)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_DECISION_REQUEST, handle_decision_request)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_INHIBITION_CHECK, handle_inhibition_check)
                );
                LOG_INFO("[%s] Bio-async enabled (legacy direct registration)", PFC_LOG_MODULE);
            } else {
                LOG_INFO("[%s] Bio-async enabled (KG-driven wiring callback registered)", PFC_LOG_MODULE);
            }
        } else {
            LOG_WARNING("[%s] Failed to register with bio-async router", PFC_LOG_MODULE);
        }
    }

    /* Initialize state */
    adapter->status = PREFRONTAL_STATUS_IDLE;
    adapter->last_error = PREFRONTAL_ERROR_NONE;
    adapter->current_time_ms = 0.0;

    LOG_INFO("[%s] Prefrontal cortex adapter created successfully", PFC_LOG_MODULE);
    return adapter;
}

void prefrontal_destroy(prefrontal_adapter_t* adapter) {
    if (!adapter) return;

    LOG_INFO("[%s] Destroying prefrontal cortex adapter", PFC_LOG_MODULE);

    /* Unregister from bio-async router */
    if (adapter->bio_ctx) {
        bio_router_unregister_module(adapter->bio_ctx);
        adapter->bio_ctx = NULL;
    }

    /* Free goals */
    if (adapter->goals) {
        nimcp_free(adapter->goals);
    }

    /* Free plans */
    if (adapter->plans) {
        for (uint32_t i = 0; i < adapter->plan_capacity; i++) {
            if (adapter->plans[i].actions) {
                nimcp_free(adapter->plans[i].actions);
            }
        }
        nimcp_free(adapter->plans);
    }

    /* Free evaluation buffer */
    if (adapter->eval_buffer) {
        nimcp_free(adapter->eval_buffer);
    }

    /* Free working memory */
    if (adapter->working_memory) {
        nimcp_free(adapter->working_memory);
    }

    /* Free rules */
    if (adapter->rules) {
        for (uint32_t i = 0; i < adapter->rule_count; i++) {
            if (adapter->rules[i].context_weights) {
                nimcp_free(adapter->rules[i].context_weights);
            }
        }
        nimcp_free(adapter->rules);
    }

    /* Destroy memory pool */
    if (adapter->action_pool) {
        memory_pool_destroy(adapter->action_pool);
    }

    nimcp_free(adapter);
    LOG_DEBUG("[%s] Prefrontal cortex adapter destroyed", PFC_LOG_MODULE);
}

bool prefrontal_reset(prefrontal_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_reset: adapter is NULL");
        return false;
    }

    LOG_DEBUG("[%s] Resetting adapter state", PFC_LOG_MODULE);

    /* Clear goals */
    memset(adapter->goals, 0, adapter->goal_capacity * sizeof(prefrontal_goal_t));
    adapter->goal_count = 0;

    /* Clear plans */
    for (uint32_t i = 0; i < adapter->plan_capacity; i++) {
        memset(adapter->plans[i].actions, 0,
               adapter->config.max_actions * sizeof(prefrontal_action_t));
        adapter->plans[i].action_count = 0;
        adapter->plans[i].current_step = 0;
    }
    adapter->plan_count = 0;

    /* Clear working memory */
    if (adapter->working_memory) {
        memset(adapter->working_memory, 0,
               adapter->config.working_memory_slots * sizeof(pfc_wm_slot_t));
        adapter->wm_count = 0;
        adapter->wm_head = 0;
    }

    /* Reset inhibitory control */
    adapter->inhibitory_strength = 1.0f;
    adapter->inhibitory_fatigue = 0.0f;

    /* Reset state */
    adapter->status = PREFRONTAL_STATUS_IDLE;
    adapter->last_error = PREFRONTAL_ERROR_NONE;

    LOG_DEBUG("[%s] Adapter reset complete", PFC_LOG_MODULE);
    return true;
}

/*=============================================================================
 * GOAL MANAGEMENT
 *===========================================================================*/

uint32_t prefrontal_activate_goal(prefrontal_adapter_t* adapter,
                                   const prefrontal_goal_t* goal) {
    if (!adapter || !goal) {
        set_error(adapter, PREFRONTAL_ERROR_INVALID_INPUT);
        return 0;
    }

    /* Check capacity */
    if (adapter->goal_count >= adapter->goal_capacity) {
        set_error(adapter, PREFRONTAL_ERROR_BUFFER_OVERFLOW);
        return 0;
    }

    /* Check for conflicts with existing goals */
    for (uint32_t i = 0; i < adapter->goal_count; i++) {
        if (adapter->goals[i].state == GOAL_STATE_ACTIVE &&
            adapter->goals[i].priority == GOAL_PRIORITY_CRITICAL &&
            goal->priority == GOAL_PRIORITY_CRITICAL) {
            /* Conflict: two critical goals */
            set_error(adapter, PREFRONTAL_ERROR_GOAL_CONFLICT);
            LOG_WARNING("[%s] Goal conflict: multiple critical goals", PFC_LOG_MODULE);
            return 0;
        }
    }

    /* Add goal */
    uint32_t idx = adapter->goal_count++;
    adapter->goals[idx] = *goal;
    adapter->goals[idx].goal_id = adapter->next_goal_id++;
    adapter->goals[idx].state = GOAL_STATE_ACTIVE;
    adapter->goals[idx].progress = 0.0f;

    /* Update status */
    adapter->status = PREFRONTAL_STATUS_GOAL_SELECTION;

    /* Emit callback */
    emit_goal_callback(adapter, &adapter->goals[idx], GOAL_STATE_INACTIVE, GOAL_STATE_ACTIVE);

    LOG_INFO("[%s] Goal activated: id=%u, priority=%d, description='%s'",
             PFC_LOG_MODULE, adapter->goals[idx].goal_id,
             adapter->goals[idx].priority, adapter->goals[idx].description);

    /* Update statistics */
    adapter->stats.goals_activated++;

    return adapter->goals[idx].goal_id;
}

bool prefrontal_deactivate_goal(prefrontal_adapter_t* adapter,
                                 uint32_t goal_id,
                                 goal_state_t new_state) {
    if (!adapter || goal_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prefrontal_reset: adapter is NULL");
        return false;
    }

    prefrontal_goal_t* goal = find_goal(adapter, goal_id);
    if (!goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_reset: goal is NULL");
        return false;
    }

    goal_state_t old_state = goal->state;
    goal->state = new_state;

    /* Update statistics */
    switch (new_state) {
        case GOAL_STATE_ACHIEVED:
            adapter->stats.goals_achieved++;
            break;
        case GOAL_STATE_FAILED:
            adapter->stats.goals_failed++;
            break;
        case GOAL_STATE_SUSPENDED:
            adapter->stats.goals_suspended++;
            break;
        default:
            break;
    }

    /* Emit callback */
    emit_goal_callback(adapter, goal, old_state, new_state);

    LOG_INFO("[%s] Goal deactivated: id=%u, new_state=%d", PFC_LOG_MODULE, goal_id, new_state);

    return true;
}

bool prefrontal_get_active_goals(const prefrontal_adapter_t* adapter,
                                  prefrontal_goal_t* goals,
                                  uint32_t* count) {
    if (!adapter || !goals || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_reset: required parameter is NULL (adapter, goals, count)");
        return false;
    }

    uint32_t active_count = 0;
    for (uint32_t i = 0; i < adapter->goal_count && active_count < *count; i++) {
        if (adapter->goals[i].state == GOAL_STATE_ACTIVE) {
            goals[active_count++] = adapter->goals[i];
        }
    }

    *count = active_count;
    return true;
}

bool prefrontal_update_goal_progress(prefrontal_adapter_t* adapter,
                                      uint32_t goal_id,
                                      float progress) {
    if (!adapter || goal_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prefrontal_reset: adapter is NULL");
        return false;
    }

    prefrontal_goal_t* goal = find_goal(adapter, goal_id);
    if (!goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_reset: goal is NULL");
        return false;
    }

    goal->progress = progress;

    /* Check for completion */
    if (progress >= 1.0f && goal->state == GOAL_STATE_ACTIVE) {
        prefrontal_deactivate_goal(adapter, goal_id, GOAL_STATE_ACHIEVED);
    }

    return true;
}

/*=============================================================================
 * PLANNING
 *===========================================================================*/

bool prefrontal_generate_plan(prefrontal_adapter_t* adapter,
                               uint32_t goal_id,
                               action_plan_t* plan) {
    if (!adapter || goal_id == 0 || !plan) {
        set_error(adapter, PREFRONTAL_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_reset: required parameter is NULL (adapter, plan)");
        return false;
    }

    prefrontal_goal_t* goal = find_goal(adapter, goal_id);
    if (!goal || goal->state != GOAL_STATE_ACTIVE) {
        set_error(adapter, PREFRONTAL_ERROR_PLANNING_FAILURE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prefrontal_reset: goal is NULL");
        return false;
    }

    adapter->status = PREFRONTAL_STATUS_PLANNING;

    /* Find or create plan slot */
    action_plan_t* internal_plan = find_plan(adapter, goal_id);
    if (!internal_plan) {
        if (adapter->plan_count >= adapter->plan_capacity) {
            set_error(adapter, PREFRONTAL_ERROR_BUFFER_OVERFLOW);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "prefrontal_reset: capacity exceeded");
            return false;
        }
        internal_plan = &adapter->plans[adapter->plan_count++];
    }

    /* Generate plan (simplified planning algorithm) */
    internal_plan->goal_id = goal_id;
    internal_plan->current_step = 0;

    /* Compute number of steps based on goal complexity */
    uint32_t num_steps = (uint32_t)(goal->value * adapter->config.planning_horizon);
    if (num_steps < 1) num_steps = 1;
    if (num_steps > adapter->config.max_actions) num_steps = adapter->config.max_actions;

    /* Generate action sequence */
    float cumulative_discount = 1.0f;
    float total_value = 0.0f;
    float total_cost = 0.0f;

    for (uint32_t i = 0; i < num_steps; i++) {
        prefrontal_action_t* action = &internal_plan->actions[i];

        action->action_id = i + 1;
        action->type = ACTION_TYPE_COGNITIVE;
        snprintf(action->description, sizeof(action->description),
                 "Step %u toward goal %u", i + 1, goal_id);

        /* Compute action value with temporal discounting */
        action->expected_value = (goal->value / num_steps) * cumulative_discount;
        action->cost = 0.1f * (1.0f + (float)i * 0.05f);  /* Increasing cost */
        action->confidence = 0.8f - (float)i * 0.02f;  /* Decreasing confidence */
        action->goal_id = goal_id;

        total_value += action->expected_value;
        total_cost += action->cost;
        cumulative_discount *= adapter->config.planning_discount;
    }

    internal_plan->action_count = num_steps;
    internal_plan->expected_total_value = total_value;
    internal_plan->total_cost = total_cost;
    internal_plan->plan_confidence = 0.7f;  /* Base confidence */

    /* Copy to output */
    *plan = *internal_plan;

    /* Update statistics */
    adapter->stats.plans_generated++;
    adapter->stats.avg_plan_length = (adapter->stats.avg_plan_length *
        (adapter->stats.plans_generated - 1) + num_steps) /
        adapter->stats.plans_generated;

    adapter->status = PREFRONTAL_STATUS_IDLE;

    LOG_INFO("[%s] Plan generated for goal %u: %u steps, expected value=%.2f",
             PFC_LOG_MODULE, goal_id, num_steps, total_value);

    return true;
}

bool prefrontal_get_next_action(prefrontal_adapter_t* adapter,
                                 uint32_t goal_id,
                                 prefrontal_action_t* action) {
    if (!adapter || goal_id == 0 || !action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_get_next_action: required parameter is NULL (adapter, action)");
        return false;
    }

    action_plan_t* plan = find_plan(adapter, goal_id);
    if (!plan || plan->current_step >= plan->action_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "prefrontal_get_next_action: plan not found or exhausted");
        return false;
    }

    *action = plan->actions[plan->current_step];
    plan->current_step++;

    adapter->status = PREFRONTAL_STATUS_EXECUTING;

    return true;
}

bool prefrontal_report_action_outcome(prefrontal_adapter_t* adapter,
                                       uint32_t action_id,
                                       bool success,
                                       float outcome) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_report_action_outcome: adapter is NULL");
        return false;
    }

    adapter->status = PREFRONTAL_STATUS_MONITORING;

    /* Find action in plans */
    for (uint32_t p = 0; p < adapter->plan_count; p++) {
        action_plan_t* plan = &adapter->plans[p];
        for (uint32_t a = 0; a < plan->action_count; a++) {
            if (plan->actions[a].action_id == action_id) {
                /* Update action based on outcome */
                if (success) {
                    /* Update goal progress */
                    prefrontal_goal_t* goal = find_goal(adapter, plan->goal_id);
                    if (goal) {
                        float step_progress = 1.0f / plan->action_count;
                        prefrontal_update_goal_progress(adapter, plan->goal_id,
                            goal->progress + step_progress);
                    }
                } else {
                    /* Plan revision may be needed */
                    adapter->stats.plans_revised++;
                }

                /* Invoke action callback */
                if (adapter->action_callback) {
                    adapter->action_callback(&plan->actions[a], false, outcome,
                                              adapter->action_callback_data);
                }

                adapter->status = PREFRONTAL_STATUS_IDLE;
                return true;
            }
        }
    }

    adapter->status = PREFRONTAL_STATUS_IDLE;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prefrontal_report_action_outcome: action_id not found in plans");
    return false;
}

/*=============================================================================
 * DECISION-MAKING
 *===========================================================================*/

bool prefrontal_evaluate_options(prefrontal_adapter_t* adapter,
                                  const decision_option_t* options,
                                  uint32_t num_options,
                                  decision_result_t* result) {
    if (!adapter || !options || num_options == 0 || !result) {
        set_error(adapter, PREFRONTAL_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_evaluate_options: required parameter is NULL (adapter, options, result)");
        return false;
    }

    adapter->status = PREFRONTAL_STATUS_DECISION;
    memset(result, 0, sizeof(decision_result_t));

    double start_time = adapter->current_time_ms;

    /* Copy options to evaluation buffer */
    uint32_t eval_count = (num_options < adapter->eval_capacity) ?
                          num_options : adapter->eval_capacity;
    memcpy(adapter->eval_buffer, options, eval_count * sizeof(decision_option_t));

    /* Compute utilities and find best option */
    float best_utility = -1e9f;
    uint32_t best_idx = 0;
    float risk_aversion = 0.5f;  /* Default risk aversion */

    for (uint32_t i = 0; i < eval_count; i++) {
        float utility = compute_utility(&adapter->eval_buffer[i], risk_aversion);
        adapter->eval_buffer[i].expected_utility = utility;
        adapter->eval_buffer[i].desirability = utility;

        if (utility > best_utility) {
            best_utility = utility;
            best_idx = i;
        }
    }

    /* Detect conflict (multiple options with similar utility) */
    uint32_t conflict_count = 0;
    for (uint32_t i = 0; i < eval_count; i++) {
        if (i != best_idx &&
            fabsf(adapter->eval_buffer[i].expected_utility - best_utility) < 0.1f) {
            conflict_count++;
        }
    }

    /* Check for timeout */
    double decision_time = adapter->current_time_ms - start_time;
    if (decision_time > adapter->config.decision_timeout_ms) {
        set_error(adapter, PREFRONTAL_ERROR_DECISION_TIMEOUT);
        adapter->stats.decision_timeouts++;
        adapter->status = PREFRONTAL_STATUS_IDLE;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prefrontal_evaluate_options: decision timeout");
        return false;
    }

    /* Fill result */
    result->selected_option = &adapter->eval_buffer[best_idx];
    result->options_evaluated = eval_count;
    result->decision_confidence = adapter->config.decision_threshold +
        (1.0f - adapter->config.decision_threshold) * (1.0f - conflict_count * 0.2f);
    result->decision_time_ms = (float)decision_time;
    result->conflict_level = conflict_count > 2 ? 3 : conflict_count;
    result->was_inhibited = false;

    /* Update statistics */
    adapter->stats.decisions_made++;
    adapter->stats.avg_decision_time_ms = (adapter->stats.avg_decision_time_ms *
        (adapter->stats.decisions_made - 1) + result->decision_time_ms) /
        adapter->stats.decisions_made;
    adapter->stats.avg_decision_confidence = (adapter->stats.avg_decision_confidence *
        (adapter->stats.decisions_made - 1) + result->decision_confidence) /
        adapter->stats.decisions_made;

    /* Emit callback */
    emit_decision_callback(adapter, result);

    adapter->status = PREFRONTAL_STATUS_IDLE;

    LOG_DEBUG("[%s] Decision made: selected option %u, confidence=%.2f, conflict=%u",
              PFC_LOG_MODULE, best_idx, result->decision_confidence, result->conflict_level);

    return true;
}

bool prefrontal_quick_decision(prefrontal_adapter_t* adapter,
                                const decision_option_t* options,
                                uint32_t num_options,
                                uint32_t* selected) {
    if (!adapter || !options || num_options == 0 || !selected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_quick_decision: required parameter is NULL (adapter, options, selected)");
        return false;
    }

    /* Quick decision: just pick highest expected value */
    float best_value = -1e9f;
    *selected = 0;

    for (uint32_t i = 0; i < num_options; i++) {
        if (options[i].action.expected_value > best_value) {
            best_value = options[i].action.expected_value;
            *selected = i;
        }
    }

    return true;
}

bool prefrontal_get_decision_state(const prefrontal_adapter_t* adapter,
                                    uint32_t* conflict_level,
                                    uint32_t* dominant_option) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_get_decision_state: adapter is NULL");
        return false;
    }

    if (adapter->status != PREFRONTAL_STATUS_DECISION) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "prefrontal_get_decision_state: not in decision state");
        return false;  /* Not in decision process */
    }

    if (conflict_level) *conflict_level = 0;
    if (dominant_option) *dominant_option = 0;

    return true;
}

/*=============================================================================
 * INHIBITORY CONTROL
 *===========================================================================*/

bool prefrontal_check_inhibition(prefrontal_adapter_t* adapter,
                                  const prefrontal_action_t* action,
                                  impulse_record_t* record) {
    if (!adapter || !action || !record) {
        return false;
    }

    memset(record, 0, sizeof(impulse_record_t));
    record->impulse_action = *action;

    adapter->status = PREFRONTAL_STATUS_INHIBITION;

    /* Compute impulse strength based on expected value and urgency */
    record->impulse_strength = action->expected_value * 0.7f + 0.3f;

    /* Check if action conflicts with active goals */
    bool conflicts = false;
    for (uint32_t i = 0; i < adapter->goal_count; i++) {
        if (adapter->goals[i].state == GOAL_STATE_ACTIVE) {
            /* Simplified conflict detection: high-value impulsive actions may conflict */
            if (action->expected_value > adapter->goals[i].value * 0.8f &&
                action->goal_id != adapter->goals[i].goal_id) {
                conflicts = true;
                break;
            }
        }
    }

    /* Apply inhibitory control */
    float effective_inhibition = adapter->inhibitory_strength *
        (1.0f - adapter->inhibitory_fatigue);

    bool should_inhibit = false;

    if (conflicts && record->impulse_strength > adapter->config.inhibition_threshold) {
        should_inhibit = true;
        record->inhibition_strength = effective_inhibition;
        record->was_suppressed = (effective_inhibition >= record->impulse_strength);
        snprintf(record->suppression_reason, sizeof(record->suppression_reason),
                 "Conflicts with active goal");
    }

    /* Update fatigue */
    if (should_inhibit) {
        update_inhibitory_fatigue(adapter, record->inhibition_strength);
        adapter->stats.impulses_detected++;

        if (record->was_suppressed) {
            adapter->stats.impulses_suppressed++;
        } else {
            adapter->stats.inhibition_failures++;
        }
    }

    adapter->status = PREFRONTAL_STATUS_IDLE;

    return should_inhibit;
}

bool prefrontal_suppress_impulse(prefrontal_adapter_t* adapter,
                                  const prefrontal_action_t* action,
                                  const char* reason) {
    if (!adapter || !action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_suppress_impulse: required parameter is NULL (adapter, action)");
        return false;
    }

    impulse_record_t record;
    memset(&record, 0, sizeof(record));
    record.impulse_action = *action;
    record.impulse_strength = 1.0f;
    record.inhibition_strength = adapter->inhibitory_strength;
    record.was_suppressed = true;

    if (reason) {
        strncpy(record.suppression_reason, reason, sizeof(record.suppression_reason) - 1);
    }

    /* Update fatigue */
    update_inhibitory_fatigue(adapter, record.inhibition_strength);

    /* Emit callback */
    emit_inhibition_callback(adapter, &record);

    adapter->stats.impulses_detected++;
    adapter->stats.impulses_suppressed++;

    return true;
}

bool prefrontal_get_inhibition_stats(const prefrontal_adapter_t* adapter,
                                      float* success_rate,
                                      float* fatigue_level) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_get_inhibition_stats: adapter is NULL");
        return false;
    }

    if (success_rate) {
        if (adapter->stats.impulses_detected > 0) {
            *success_rate = (float)adapter->stats.impulses_suppressed /
                            (float)adapter->stats.impulses_detected;
        } else {
            *success_rate = 1.0f;
        }
    }

    if (fatigue_level) {
        *fatigue_level = adapter->inhibitory_fatigue;
    }

    return true;
}

/*=============================================================================
 * WORKING MEMORY INTEGRATION
 *===========================================================================*/

bool prefrontal_wm_push(prefrontal_adapter_t* adapter,
                         uint32_t item_id,
                         float priority,
                         uint32_t goal_id) {
    if (!adapter || !adapter->working_memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_wm_push: required parameter is NULL (adapter, adapter->working_memory)");
        return false;
    }

    if (adapter->wm_count >= adapter->config.working_memory_slots) {
        /* WM full - evict lowest priority item */
        uint32_t min_idx = 0;
        float min_priority = adapter->working_memory[0].priority;

        for (uint32_t i = 1; i < adapter->wm_count; i++) {
            if (adapter->working_memory[i].priority < min_priority) {
                min_priority = adapter->working_memory[i].priority;
                min_idx = i;
            }
        }

        /* If new item has higher priority, evict */
        if (priority > min_priority) {
            adapter->working_memory[min_idx].item_id = item_id;
            adapter->working_memory[min_idx].priority = priority;
            adapter->working_memory[min_idx].activation = 1.0f;
            adapter->working_memory[min_idx].goal_id = goal_id;
            adapter->working_memory[min_idx].timestamp = adapter->current_time_ms;
            return true;
        } else {
            set_error(adapter, PREFRONTAL_ERROR_WORKING_MEMORY_FULL);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "prefrontal_wm_push: working memory full, priority too low to evict");
            return false;
        }
    }

    /* Add to WM */
    uint32_t idx = adapter->wm_count++;
    adapter->working_memory[idx].item_id = item_id;
    adapter->working_memory[idx].priority = priority;
    adapter->working_memory[idx].activation = 1.0f;
    adapter->working_memory[idx].goal_id = goal_id;
    adapter->working_memory[idx].timestamp = adapter->current_time_ms;

    return true;
}

bool prefrontal_wm_update(prefrontal_adapter_t* adapter,
                           uint32_t item_id,
                           float new_priority) {
    if (!adapter || !adapter->working_memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_wm_update: required parameter is NULL (adapter, adapter->working_memory)");
        return false;
    }

    for (uint32_t i = 0; i < adapter->wm_count; i++) {
        if (adapter->working_memory[i].item_id == item_id) {
            adapter->working_memory[i].activation = 1.0f;  /* Refresh */
            if (new_priority >= 0.0f) {
                adapter->working_memory[i].priority = new_priority;
            }
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "prefrontal_wm_update: item_id not found in working memory");
    return false;
}

bool prefrontal_wm_get_contents(const prefrontal_adapter_t* adapter,
                                 uint32_t* item_ids,
                                 float* priorities,
                                 uint32_t* count) {
    if (!adapter || !item_ids || !count || !adapter->working_memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_wm_get_contents: required parameter is NULL (adapter, item_ids, count, adapter->working_memory)");
        return false;
    }

    uint32_t to_copy = (*count < adapter->wm_count) ? *count : adapter->wm_count;

    for (uint32_t i = 0; i < to_copy; i++) {
        item_ids[i] = adapter->working_memory[i].item_id;
        if (priorities) {
            priorities[i] = adapter->working_memory[i].priority;
        }
    }

    *count = to_copy;
    return true;
}

/*=============================================================================
 * COGNITIVE FLEXIBILITY
 *===========================================================================*/

bool prefrontal_task_switch(prefrontal_adapter_t* adapter,
                             uint32_t new_task_id,
                             float* switch_cost_out) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_task_switch: adapter is NULL");
        return false;
    }

    float cost = 0.0f;

    if (adapter->current_task_id != new_task_id && adapter->current_task_id != 0) {
        cost = adapter->config.switch_cost;
        adapter->stats.task_switches++;
        adapter->stats.avg_switch_cost_ms = (adapter->stats.avg_switch_cost_ms *
            (adapter->stats.task_switches - 1) + cost * 100.0f) /
            adapter->stats.task_switches;
    }

    adapter->current_task_id = new_task_id;

    if (switch_cost_out) {
        *switch_cost_out = cost;
    }

    return true;
}

bool prefrontal_learn_rule(prefrontal_adapter_t* adapter,
                            const float* context,
                            uint32_t context_size,
                            uint32_t action,
                            float outcome) {
    if (!adapter || !context || context_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_learn_rule: required parameter is NULL (adapter, context)");
        return false;
    }
    if (!adapter->config.enable_rule_learning) {
        LOG_DEBUG("prefrontal_learn_rule: rule learning is disabled");
        return false;
    }

    /* Simplified rule learning: store context-action-outcome association */
    if (adapter->rule_count >= adapter->rule_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "prefrontal_learn_rule: rule capacity exceeded");
        return false;  /* Rule capacity reached */
    }

    pfc_rule_t* rule = &adapter->rules[adapter->rule_count];
    rule->rule_id = adapter->rule_count + 1;
    rule->task_id = adapter->current_task_id;
    rule->context_size = context_size;
    rule->context_weights = (float*)nimcp_calloc(context_size, sizeof(float));

    if (!rule->context_weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "prefrontal_learn_rule: failed to allocate context_weights");
        return false;
    }

    memcpy(rule->context_weights, context, context_size * sizeof(float));
    rule->confidence = outcome > 0.0f ? outcome : 0.5f;
    rule->usage_count = 1;

    adapter->rule_count++;

    return true;
}

/*=============================================================================
 * CALLBACK REGISTRATION
 *===========================================================================*/

bool prefrontal_set_goal_callback(prefrontal_adapter_t* adapter,
                                   prefrontal_goal_callback_t callback,
                                   void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_set_goal_callback: adapter is NULL");
        return false;
    }
    adapter->goal_callback = callback;
    adapter->goal_callback_data = user_data;
    return true;
}

bool prefrontal_set_decision_callback(prefrontal_adapter_t* adapter,
                                       prefrontal_decision_callback_t callback,
                                       void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_set_decision_callback: adapter is NULL");
        return false;
    }
    adapter->decision_callback = callback;
    adapter->decision_callback_data = user_data;
    return true;
}

bool prefrontal_set_inhibition_callback(prefrontal_adapter_t* adapter,
                                         prefrontal_inhibition_callback_t callback,
                                         void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_set_inhibition_callback: adapter is NULL");
        return false;
    }
    adapter->inhibition_callback = callback;
    adapter->inhibition_callback_data = user_data;
    return true;
}

bool prefrontal_set_action_callback(prefrontal_adapter_t* adapter,
                                     prefrontal_action_callback_t callback,
                                     void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_set_action_callback: adapter is NULL");
        return false;
    }
    adapter->action_callback = callback;
    adapter->action_callback_data = user_data;
    return true;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

prefrontal_status_t prefrontal_get_status(const prefrontal_adapter_t* adapter) {
    if (!adapter) return PREFRONTAL_STATUS_ERROR;
    return adapter->status;
}

prefrontal_error_t prefrontal_get_last_error(const prefrontal_adapter_t* adapter) {
    if (!adapter) return PREFRONTAL_ERROR_INTERNAL;
    return adapter->last_error;
}

const char* prefrontal_error_string(prefrontal_error_t error) {
    switch (error) {
        case PREFRONTAL_ERROR_NONE: return "No error";
        case PREFRONTAL_ERROR_INVALID_INPUT: return "Invalid input";
        case PREFRONTAL_ERROR_GOAL_CONFLICT: return "Goal conflict";
        case PREFRONTAL_ERROR_PLANNING_FAILURE: return "Planning failed";
        case PREFRONTAL_ERROR_DECISION_TIMEOUT: return "Decision timeout";
        case PREFRONTAL_ERROR_INHIBITION_TRIGGERED: return "Inhibition triggered";
        case PREFRONTAL_ERROR_WORKING_MEMORY_FULL: return "Working memory full";
        case PREFRONTAL_ERROR_BUFFER_OVERFLOW: return "Buffer overflow";
        case PREFRONTAL_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* prefrontal_status_string(prefrontal_status_t status) {
    switch (status) {
        case PREFRONTAL_STATUS_IDLE: return "Idle";
        case PREFRONTAL_STATUS_GOAL_SELECTION: return "Goal selection";
        case PREFRONTAL_STATUS_PLANNING: return "Planning";
        case PREFRONTAL_STATUS_DECISION: return "Decision";
        case PREFRONTAL_STATUS_INHIBITION: return "Inhibition";
        case PREFRONTAL_STATUS_EXECUTING: return "Executing";
        case PREFRONTAL_STATUS_MONITORING: return "Monitoring";
        case PREFRONTAL_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

bool prefrontal_get_stats(const prefrontal_adapter_t* adapter,
                           prefrontal_stats_t* stats) {
    if (!adapter || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_status_string: required parameter is NULL (adapter, stats)");
        return false;
    }
    *stats = adapter->stats;
    return true;
}

bool prefrontal_get_config(const prefrontal_adapter_t* adapter,
                            prefrontal_config_t* config) {
    if (!adapter || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_status_string: required parameter is NULL (adapter, config)");
        return false;
    }
    *config = adapter->config;
    return true;
}

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

bio_module_context_t prefrontal_get_bio_context(prefrontal_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->bio_ctx;
}

uint32_t prefrontal_process_bio_messages(prefrontal_adapter_t* adapter,
                                          uint32_t max_messages) {
    if (!adapter || !adapter->bio_ctx) return 0;

    uint32_t processed = bio_router_process_inbox(adapter->bio_ctx, max_messages);
    if (processed > 0) {
        LOG_DEBUG("[%s] Processed %u bio-async messages", PFC_LOG_MODULE, processed);
    }
    return processed;
}

nimcp_bio_future_t prefrontal_request_goal_eval_async(
    prefrontal_adapter_t* adapter,
    const prefrontal_goal_t* goal) {

    if (!adapter || !adapter->bio_ctx || !goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_get_bio_context: required parameter is NULL (adapter, adapter->bio_ctx, goal)");
        return NULL;
    }

    /* Send goal evaluation request - simplified message */
    bio_message_header_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = BIO_MSG_GOAL_EVAL_REQUEST;
    msg.source_module = BIO_MODULE_PREFRONTAL;
    msg.target_module = BIO_MODULE_PREFRONTAL;
    msg.payload_size = sizeof(msg);
    msg.channel = adapter->default_channel;

    nimcp_bio_promise_t promise = bio_router_send_async(
        adapter->bio_ctx, &msg, sizeof(msg), adapter->default_channel);

    if (!promise) {
        LOG_ERROR("[%s] Failed to send goal eval request", PFC_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_get_bio_context: promise is NULL");
        return NULL;
    }

    return nimcp_bio_promise_get_future(promise);
}

nimcp_error_t prefrontal_broadcast_decision(
    prefrontal_adapter_t* adapter,
    const decision_result_t* result) {

    if (!adapter || !result) {
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    if (!adapter->bio_ctx) {
        return NIMCP_SUCCESS;  /* Not an error if bio-async disabled */
    }

    bio_message_header_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = BIO_MSG_DECISION_RESPONSE;
    msg.source_module = BIO_MODULE_PREFRONTAL;
    msg.target_module = 0;  /* Broadcast */
    msg.payload_size = sizeof(msg);
    msg.channel = adapter->default_channel;
    msg.flags = BIO_MSG_FLAG_BROADCAST;

    return bio_router_broadcast(adapter->bio_ctx, &msg, sizeof(msg));
}

/*=============================================================================
 * BIO-ASYNC MESSAGE HANDLERS
 *===========================================================================*/

static nimcp_error_t handle_goal_eval_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    prefrontal_adapter_t* adapter = (prefrontal_adapter_t*)user_data;
    (void)msg;
    (void)msg_size;

    if (!adapter) {
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    /* Complete promise with acknowledgement (simplified) */
    (void)response_promise;  /* Response handling not yet implemented */

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_decision_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    prefrontal_adapter_t* adapter = (prefrontal_adapter_t*)user_data;
    (void)msg;
    (void)msg_size;

    if (!adapter) {
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    (void)response_promise;  /* Response handling not yet implemented */

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_inhibition_check(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    prefrontal_adapter_t* adapter = (prefrontal_adapter_t*)user_data;
    (void)msg;
    (void)msg_size;

    if (!adapter) {
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    (void)response_promise;  /* Response handling not yet implemented */

    return NIMCP_SUCCESS;
}
