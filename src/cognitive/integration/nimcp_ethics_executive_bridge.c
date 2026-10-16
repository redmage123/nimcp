/**
 * @file nimcp_ethics_executive_bridge.c
 * @brief Ethics-Executive Bridge Implementation
 *
 * WHAT: Bidirectional integration between ethics and executive systems
 * WHY:  Ethics constrains executive action selection; executive functions
 *       evaluate and implement ethical constraints.
 * HOW:  Ethics provides constraints and vetoes; executive evaluates actions
 *       against ethical standards before execution.
 *
 * BIOLOGICAL BASIS:
 * - Ventromedial prefrontal cortex integrates emotional/ethical valuation
 * - Dorsolateral PFC implements executive control over behavior
 * - ACC monitors for conflict between goals and ethical constraints
 *
 * @author NIMCP Development Team
 * @date 2025-01
 */

#include "cognitive/integration/nimcp_ethics_executive_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(ethics_executive_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_ethics_executive_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_ethics_executive_bridge_mesh_registry = NULL;

nimcp_error_t ethics_executive_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_ethics_executive_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "ethics_executive_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "ethics_executive_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_ethics_executive_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_ethics_executive_bridge_mesh_registry = registry;
    return err;
}

void ethics_executive_bridge_mesh_unregister(void) {
    if (g_ethics_executive_bridge_mesh_registry && g_ethics_executive_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_ethics_executive_bridge_mesh_registry, g_ethics_executive_bridge_mesh_id);
        g_ethics_executive_bridge_mesh_id = 0;
        g_ethics_executive_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from ethics_executive_bridge module (instance-level) */
static inline void ethics_executive_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_ethics_executive_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_ethics_executive_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_ethics_executive_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "ETHICS_EXECUTIVE_BRIDGE"


/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal action evaluation record
 */
typedef struct action_evaluation {
    uint32_t action_id;         /**< Action identifier */
    float ethical_score;        /**< Ethical score [0.0 to 1.0] */
    bool vetoed;                /**< Whether action has been vetoed */
    bool evaluated;             /**< Whether action has been evaluated */
    uint64_t evaluation_time;   /**< Timestamp of evaluation */
} action_evaluation_t;

/**
 * @brief Full bridge structure definition
 */
struct ethics_executive_bridge {
    bridge_base_t base;                     /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    ethics_executive_config_t config;       /**< Bridge configuration */
    action_evaluation_t* evaluations;       /**< Evaluation records array */
    size_t eval_capacity;                   /**< Capacity of evaluations array */
    size_t eval_count;                      /**< Current number of evaluations */
    ethics_executive_stats_t stats;         /**< Bridge statistics */
    bool initialized;                       /**< Initialization flag */
};

/* ============================================================================
 * Constants
 * ============================================================================ */

#define DEFAULT_EVAL_CAPACITY       64
#define DEFAULT_ETHICAL_THRESHOLD   0.6f
#define DEFAULT_VETO_ENABLED        true
#define DEFAULT_CONSTRAINT_STRICTNESS 0.8f

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Find evaluation record by action ID (unlocked version)
 */
static action_evaluation_t* find_evaluation_unlocked(ethics_executive_bridge_t* bridge,
                                                      uint32_t action_id) {
    for (size_t i = 0; i < bridge->eval_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->eval_count > 256) {
            ethics_executive_bridge_heartbeat("ethics_execu_loop",
                             (float)(i + 1) / (float)bridge->eval_count);
        }

        if (bridge->evaluations[i].action_id == action_id) {
            return &bridge->evaluations[i];
        }
    }
    return NULL;  /* Not found is a normal condition */
}

/**
 * @brief Compute ethical score for an action
 *
 * In a full implementation, this would query the ethics module for
 * action-specific ethical assessment. For now, we use action_id-based
 * heuristics to simulate ethical scoring.
 */
static float compute_ethical_score(uint32_t action_id, float strictness) {
    /* Simulate ethical scoring based on action characteristics */
    /* Higher action IDs represent potentially more complex/risky actions */
    /* Base score starts high and decreases with complexity */
    float base_score = 1.0f - ((float)(action_id % 100) / 100.0f) * 0.5f;

    /* Apply strictness: higher strictness reduces scores more aggressively */
    float adjusted_score = base_score * (1.0f - (strictness * 0.2f));

    return nimcp_clampf(adjusted_score, ETHICS_EXECUTIVE_SCORE_MIN, ETHICS_EXECUTIVE_SCORE_MAX);
}

/**
 * @brief Get or create evaluation entry (unlocked version)
 */
static action_evaluation_t* get_or_create_evaluation_unlocked(
    ethics_executive_bridge_t* bridge,
    uint32_t action_id
) {
    /* Try to find existing evaluation */
    action_evaluation_t* eval = find_evaluation_unlocked(bridge, action_id);
    if (eval) {
        return eval;
    }

    /* Create new evaluation if capacity available */
    if (bridge->eval_count >= bridge->eval_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "get_or_create_evaluation_unlocked: capacity exceeded");
        return NULL;
    }

    eval = &bridge->evaluations[bridge->eval_count];
    eval->action_id = action_id;
    eval->ethical_score = 0.0f;
    eval->vetoed = false;
    eval->evaluated = false;
    eval->evaluation_time = 0;
    bridge->eval_count++;

    return eval;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

int ethics_executive_bridge_default_config(ethics_executive_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_executive_bridge_heartbeat("ethics_execu_default_config", 0.0f);


    config->ethical_threshold = DEFAULT_ETHICAL_THRESHOLD;
    config->veto_enabled = DEFAULT_VETO_ENABLED;
    config->constraint_strictness = DEFAULT_CONSTRAINT_STRICTNESS;

    return 0;
}

ethics_executive_bridge_t* ethics_executive_bridge_create(
    const ethics_executive_config_t* config
) {
    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    ethics_executive_bridge_heartbeat("ethics_execu_create", 0.0f);


    ethics_executive_bridge_t* bridge = nimcp_malloc(sizeof(ethics_executive_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }
    memset(bridge, 0, sizeof(ethics_executive_bridge_t));

    /* Initialize config */
    if (config) {
        bridge->config = *config;
    } else {
        ethics_executive_bridge_default_config(&bridge->config);
    }

    /* Allocate evaluations array */
    bridge->evaluations = nimcp_calloc(DEFAULT_EVAL_CAPACITY, sizeof(action_evaluation_t));
    if (!bridge->evaluations) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ethics_executive_bridge_create: bridge->evaluations is NULL");
        return NULL;
    }
    /* calloc zero-initializes */
    bridge->eval_capacity = DEFAULT_EVAL_CAPACITY;
    bridge->eval_count = 0;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(ethics_executive_stats_t));

    /* Create mutex for thread safety */
    if (bridge_base_init(&bridge->base, 0, "ethics_executive") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge->evaluations);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ethics_executive_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->initialized = true;

    return bridge;
}

void ethics_executive_bridge_destroy(ethics_executive_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Destroy mutex */
    /* Phase 8: Heartbeat at operation start */
    ethics_executive_bridge_heartbeat("ethics_execu_destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
        bridge->base.mutex = NULL;
    }

    /* Free evaluations array */
    if (bridge->evaluations) {
        nimcp_free(bridge->evaluations);
        bridge->evaluations = NULL;
    }

    bridge->initialized = false;

    /* Free bridge structure */
    nimcp_free(bridge);
    bridge = NULL;
}

/* ============================================================================
 * Core Functions
 * ============================================================================ */

int ethics_executive_constrain_action(
    ethics_executive_bridge_t* bridge,
    uint64_t action_id,
    ethics_constraints_out_t* constraints_out
) {
    if (!bridge || !constraints_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_executive_constrain_action: required parameter is NULL (bridge, constraints_out)");
        return -1;
    }

    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_executive_constrain_action: bridge->initialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_executive_bridge_heartbeat("ethics_execu_ethics_executive_con", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Get or create evaluation for this action */
    action_evaluation_t* eval = get_or_create_evaluation_unlocked(bridge, (uint32_t)action_id);
    if (!eval) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_executive_constrain_action: eval is NULL");
        return -1;
    }

    /* Evaluate action if not already evaluated */
    if (!eval->evaluated) {
        eval->ethical_score = compute_ethical_score((uint32_t)action_id, bridge->config.constraint_strictness);
        eval->evaluation_time = get_timestamp_ms();
        eval->evaluated = true;
    }

    /* Initialize output structure */
    memset(constraints_out, 0, sizeof(ethics_constraints_out_t));
    constraints_out->constraint_count = 0;
    constraints_out->overall_ethical_score = eval->ethical_score;

    /* Apply constraints based on ethical score and strictness */
    float threshold = bridge->config.ethical_threshold;
    float strictness = bridge->config.constraint_strictness;

    if (eval->ethical_score < threshold) {
        /* Add a constraint for low ethical score */
        if (constraints_out->constraint_count < ETHICS_EXECUTIVE_MAX_CONSTRAINTS) {
            ethics_constraint_t* constraint = &constraints_out->constraints[constraints_out->constraint_count];
            constraint->constraint_id = 1;
            snprintf(constraint->description, sizeof(constraint->description),
                     "Action below ethical threshold (%.2f < %.2f)",
                     eval->ethical_score, threshold);
            constraint->severity = (threshold - eval->ethical_score) / threshold;
            constraint->is_hard_constraint = (eval->ethical_score < threshold * 0.5f);
            constraints_out->constraint_count++;
        }
    }

    /* Add strictness-based constraint if applicable */
    if (strictness > 0.7f && eval->ethical_score < 0.8f) {
        if (constraints_out->constraint_count < ETHICS_EXECUTIVE_MAX_CONSTRAINTS) {
            ethics_constraint_t* constraint = &constraints_out->constraints[constraints_out->constraint_count];
            constraint->constraint_id = 2;
            snprintf(constraint->description, sizeof(constraint->description),
                     "High strictness mode - additional review required");
            constraint->severity = strictness * (1.0f - eval->ethical_score);
            constraint->is_hard_constraint = false;
            constraints_out->constraint_count++;
        }
    }

    /* Determine if action is permitted */
    constraints_out->action_permitted = (eval->ethical_score >= threshold) && !eval->vetoed;

    /* Update statistics */
    bridge->stats.actions_constrained++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int ethics_executive_evaluate_action(
    ethics_executive_bridge_t* bridge,
    uint64_t action_id,
    float* ethical_score_out
) {
    if (!bridge || !ethical_score_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_executive_evaluate_action: required parameter is NULL (bridge, ethical_score_out)");
        return -1;
    }

    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_executive_evaluate_action: bridge->initialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_executive_bridge_heartbeat("ethics_execu_ethics_executive_eva", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Get or create evaluation for this action */
    action_evaluation_t* eval = get_or_create_evaluation_unlocked(bridge, (uint32_t)action_id);
    if (!eval) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_executive_evaluate_action: eval is NULL");
        return -1;
    }

    /* Compute ethical score if not already evaluated */
    if (!eval->evaluated) {
        eval->ethical_score = compute_ethical_score((uint32_t)action_id, bridge->config.constraint_strictness);
        eval->evaluation_time = get_timestamp_ms();
        eval->evaluated = true;
    }

    /* Return the ethical score */
    *ethical_score_out = eval->ethical_score;

    /* Update statistics */
    bridge->stats.evaluations_performed++;

    /* Update average ethical score */
    if (bridge->stats.evaluations_performed == 1) {
        bridge->stats.avg_ethical_score = eval->ethical_score;
    } else {
        bridge->stats.avg_ethical_score =
            (bridge->stats.avg_ethical_score * (bridge->stats.evaluations_performed - 1) + eval->ethical_score)
            / bridge->stats.evaluations_performed;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int ethics_executive_veto_action(
    ethics_executive_bridge_t* bridge,
    uint64_t action_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_executive_veto_action: bridge->initialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_executive_bridge_heartbeat("ethics_execu_ethics_executive_vet", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if veto is enabled */
    if (!bridge->config.veto_enabled) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_executive_veto_action: bridge->config is NULL");
        return -1;
    }

    /* Find evaluation for this action */
    action_evaluation_t* eval = find_evaluation_unlocked(bridge, (uint32_t)action_id);
    if (!eval) {
        /* Create evaluation if not found */
        eval = get_or_create_evaluation_unlocked(bridge, (uint32_t)action_id);
        if (!eval) {
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_executive_veto_action: eval is NULL");
            return -1;
        }
        /* Evaluate if newly created */
        eval->ethical_score = compute_ethical_score((uint32_t)action_id, bridge->config.constraint_strictness);
        eval->evaluation_time = get_timestamp_ms();
        eval->evaluated = true;
    }

    /* Check if action should be vetoed based on ethical score */
    if (eval->ethical_score < bridge->config.ethical_threshold) {
        eval->vetoed = true;
        bridge->stats.actions_vetoed++;

        /* Update veto rate */
        if (bridge->stats.evaluations_performed > 0) {
            bridge->stats.veto_rate = (float)bridge->stats.actions_vetoed /
                                      (float)bridge->stats.evaluations_performed;
        }

        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Action vetoed */
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 1;  /* Action not vetoed */
}

int ethics_executive_get_permitted_actions(
    ethics_executive_bridge_t* bridge,
    uint64_t actions[],
    size_t max_count
) {
    if (!bridge || !actions || max_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_executive_get_permitted_actions: required parameter is NULL (bridge, actions)");
        return -1;
    }

    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_executive_get_permitted_actions: bridge->initialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_executive_bridge_heartbeat("ethics_execu_ethics_executive_get", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    size_t permitted_count = 0;
    float threshold = bridge->config.ethical_threshold;

    /* Iterate through all evaluations */
    for (size_t i = 0; i < bridge->eval_count && permitted_count < max_count; i++) {
        action_evaluation_t* eval = &bridge->evaluations[i];

        /* Check if action is permitted */
        if (eval->evaluated &&
            eval->ethical_score >= threshold &&
            !eval->vetoed) {
            actions[permitted_count] = (uint64_t)eval->action_id;
            permitted_count++;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return (int)permitted_count;
}

/* ============================================================================
 * Statistics Functions
 * ============================================================================ */

int ethics_executive_bridge_get_stats(
    const ethics_executive_bridge_t* bridge,
    ethics_executive_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_executive_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_executive_bridge_get_stats: bridge->initialized is NULL");
        return -1;
    }

    /* Cast away const for mutex lock (stats read is still logically const) */
    /* Phase 8: Heartbeat at operation start */
    ethics_executive_bridge_heartbeat("ethics_execu_get_stats", 0.0f);


    ethics_executive_bridge_t* mutable_bridge = (ethics_executive_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void ethics_executive_bridge_set_instance_health_agent(ethics_executive_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "ethics_executive_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int ethics_executive_bridge_training_begin(ethics_executive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_executive_bridge_training_begin: NULL argument");
        return -1;
    }
    ethics_executive_bridge_heartbeat_instance(bridge->health_agent, "ethics_executive_bridge_training_begin", 0.0f);
    return 0;
}

int ethics_executive_bridge_training_end(ethics_executive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_executive_bridge_training_end: NULL argument");
        return -1;
    }
    ethics_executive_bridge_heartbeat_instance(bridge->health_agent, "ethics_executive_bridge_training_end", 1.0f);
    return 0;
}

int ethics_executive_bridge_training_step(ethics_executive_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_executive_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    ethics_executive_bridge_heartbeat_instance(bridge->health_agent, "ethics_executive_bridge_training_step", progress);
    return 0;
}
