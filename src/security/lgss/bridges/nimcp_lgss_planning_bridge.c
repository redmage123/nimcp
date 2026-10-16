/**
 * @file nimcp_lgss_planning_bridge.c
 * @brief LGSS Planning Safety Bridge Implementation - Component A6
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Implementation of safety validation for planning outputs
 * WHY:  Plans may contain hidden harmful sequences or compound risks
 * HOW:  Recursive evaluation of plan nodes, aggregate harm probabilities
 *
 * @author NIMCP Development Team
 */

#include "security/lgss/bridges/nimcp_lgss_planning_bridge.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lgss_planning_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/** Module ID for bio-async */
#define LGSS_PLAN_MODULE_ID            0x4C475050  /* 'LGPP' */

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void) {
    return nimcp_time_get_us();
}

/**
 * @brief Update running average
 */
static float update_running_avg(float current_avg, uint64_t count, float new_value) {
    if (count == 0) {
        return new_value;
    }
    return (current_avg * (float)(count - 1) + new_value) / (float)count;
}

/**
 * @brief Evaluate single node for safety (internal)
 */
static lgss_result_t evaluate_node_safety(
    planning_safety_bridge_t* bridge,
    const plan_node_t* node,
    float* adjusted_p_harm
) {
    if (!node || !adjusted_p_harm) {
        return LGSS_RESULT_ERROR;
    }

    /* Apply sensitivity scaling */
    float sensitivity = nimcp_clampf(bridge->config.safety_sensitivity, 0.5f, 2.0f);
    float p_harm = node->p_harm * sensitivity;
    if (p_harm > 1.0f) p_harm = 1.0f;

    *adjusted_p_harm = p_harm;

    /* Check harm threshold */
    if (p_harm > bridge->config.node_harm_threshold) {
        return LGSS_RESULT_DENY;
    }

    /* Check reversibility */
    if (node->reversibility < bridge->config.min_acceptable_reversibility) {
        /* Low reversibility - escalate if harm is non-trivial */
        if (p_harm > 0.1f) {
            return LGSS_RESULT_ESCALATE;
        }
    }

    /* Check for critical nodes with any harm */
    if (node->is_critical && p_harm > 0.05f) {
        return LGSS_RESULT_ESCALATE;
    }

    return LGSS_RESULT_ALLOW;
}

/**
 * @brief Recursively validate plan tree (internal helper)
 */
static int validate_tree_recursive(
    planning_safety_bridge_t* bridge,
    const plan_node_t* node,
    uint32_t current_depth,
    float* p_harm_values,
    uint32_t* p_harm_count,
    plan_validation_result_t* result
) {
    if (!node) {
        return 0;
    }

    /* Check depth limit */
    if (current_depth > bridge->config.max_validation_depth) {
        return 0;
    }

    /* Update max depth reached */
    if (current_depth > result->max_depth_reached) {
        result->max_depth_reached = current_depth;
    }

    /* Evaluate this node */
    float adjusted_p_harm;
    lgss_result_t node_result = evaluate_node_safety(bridge, node, &adjusted_p_harm);

    /* Record node evaluation */
    result->nodes_evaluated++;
    bridge->stats.nodes_validated++;

    /* Store harm value for aggregation */
    if (*p_harm_count < 1000) {  /* Reasonable limit */
        p_harm_values[*p_harm_count] = adjusted_p_harm;
        (*p_harm_count)++;
    }

    /* Track min reversibility */
    if (node->reversibility < result->min_reversibility) {
        result->min_reversibility = node->reversibility;
    }

    /* Accumulate average reversibility */
    result->avg_reversibility = update_running_avg(
        result->avg_reversibility, result->nodes_evaluated, node->reversibility);

    /* Handle dangerous nodes */
    if (node_result == LGSS_RESULT_DENY) {
        bridge->stats.nodes_dangerous++;
        if (result->num_dangerous_nodes < 16) {
            result->dangerous_node_ids[result->num_dangerous_nodes] = node->node_id;
            result->num_dangerous_nodes++;
        }

        /* Track most dangerous */
        if (adjusted_p_harm > result->most_dangerous_p_harm) {
            result->most_dangerous_p_harm = adjusted_p_harm;
            result->most_dangerous_node_id = node->node_id;
        }

        /* Early stop if configured */
        if (bridge->config.stop_on_first_violation) {
            result->result = LGSS_RESULT_DENY;
            result->is_safe = false;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Node %u blocked: p_harm=%.2f exceeds threshold",
                     node->node_id, adjusted_p_harm);
            return -1;  /* Signal early termination */
        }
    } else if (node_result == LGSS_RESULT_ESCALATE) {
        /* Upgrade overall result if needed */
        if (result->result == LGSS_RESULT_ALLOW) {
            result->result = LGSS_RESULT_ESCALATE;
        }
    }

    /* Recurse to children if validation continues */
    if (bridge->config.validate_all_branches && node->children) {
        for (uint32_t i = 0; i < node->num_children; i++) {
            int rc = validate_tree_recursive(
                bridge, node->children[i], current_depth + 1,
                p_harm_values, p_harm_count, result);
            if (rc < 0) {
                return rc;  /* Propagate early termination */
            }
        }
    }

    return 0;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int planning_safety_default_config(planning_safety_config_t* config) {
    if (!config) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(config, 0, sizeof(planning_safety_config_t));

    /* Thresholds */
    config->node_harm_threshold = 0.3f;
    config->compound_harm_threshold = LGSS_PLAN_DEFAULT_COMPOUND_HARM_THRESHOLD;
    config->max_aggregate_harm = LGSS_PLAN_DEFAULT_MAX_AGGREGATE_HARM;
    config->min_acceptable_reversibility = 0.2f;

    /* Validation depth */
    config->max_validation_depth = LGSS_PLAN_MAX_DEPTH;
    config->validate_all_branches = true;
    config->stop_on_first_violation = false;

    /* Harm estimation */
    config->include_indirect_effects = true;
    config->include_uncertainty = true;
    config->base_uncertainty_margin = 0.1f;

    /* Fail-safe */
    config->fail_safe_on_unknown = true;
    config->default_unknown_p_harm = 0.5f;

    /* Sensitivity */
    config->safety_sensitivity = 1.0f;

    return 0;
}

planning_safety_bridge_t* planning_safety_bridge_create(void) {
    planning_safety_config_t config;
    planning_safety_default_config(&config);
    return planning_safety_bridge_create_custom(&config);
}

planning_safety_bridge_t* planning_safety_bridge_create_custom(
    const planning_safety_config_t* config
) {
    /* Allocate bridge structure */
    planning_safety_bridge_t* bridge = nimcp_malloc(sizeof(planning_safety_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate planning_safety_bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "planning_safety_bridge_create_custom: bridge is NULL");
        return NULL;
    }
    memset(bridge, 0, sizeof(planning_safety_bridge_t));

    /* Set magic */
    bridge->magic = LGSS_PLAN_BRIDGE_MAGIC;

    /* Initialize config */
    if (config) {
        bridge->config = *config;
    } else {
        planning_safety_default_config(&bridge->config);
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, LGSS_PLAN_MODULE_ID, "lgss_planning_bridge") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "planning_safety_bridge_create_custom: validation failed");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created LGSS planning safety bridge");

    return bridge;
}

void planning_safety_bridge_destroy(planning_safety_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Validate magic */
    if (bridge->magic != LGSS_PLAN_BRIDGE_MAGIC) {
        NIMCP_LOGGING_ERROR("Invalid bridge magic in destroy");
        return;
    }

    /* Cleanup base bridge */
    bridge_base_cleanup(&bridge->base);

    /* Clear magic before free */
    bridge->magic = 0;

    /* Free bridge structure */
    nimcp_free(bridge);
}

/* ============================================================================
 * Core Validation API Implementation
 * ============================================================================ */

int planning_safety_validate_plan_tree(
    planning_safety_bridge_t* bridge,
    const plan_node_t* root,
    plan_validation_result_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(root);
    BRIDGE_NULL_CHECK(result);

    /* Validate magic */
    if (bridge->magic != LGSS_PLAN_BRIDGE_MAGIC) {
        return NIMCP_ERROR_INVALID_STATE;
    }

    uint64_t start_time = get_timestamp_us();

    BRIDGE_LOCK(bridge);

    /* Initialize result */
    memset(result, 0, sizeof(plan_validation_result_t));
    result->result = LGSS_RESULT_ALLOW;
    result->is_safe = true;
    result->min_reversibility = 1.0f;

    /* Update statistics */
    bridge->stats.plans_validated++;

    /* Allocate temporary array for harm values */
    float* p_harm_values = nimcp_malloc(sizeof(float) * 1000);
    if (!p_harm_values) {
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_NO_MEMORY;
    }
    uint32_t p_harm_count = 0;

    /* Recursive validation */
    int rc = validate_tree_recursive(bridge, root, 0, p_harm_values, &p_harm_count, result);

    /* Calculate aggregate harm */
    if (p_harm_count > 0) {
        result->aggregate_p_harm = planning_safety_aggregate_harm(p_harm_values, p_harm_count);
    }

    nimcp_free(p_harm_values);

    /* Check aggregate harm threshold */
    if (result->aggregate_p_harm > bridge->config.max_aggregate_harm) {
        result->result = LGSS_RESULT_DENY;
        result->is_safe = false;
        snprintf(result->explanation, sizeof(result->explanation),
                 "Aggregate harm %.2f exceeds maximum %.2f",
                 result->aggregate_p_harm, bridge->config.max_aggregate_harm);
        snprintf(result->recommended_action, sizeof(result->recommended_action),
                 "Decompose plan into lower-risk steps");
    }
    /* Check compound harm threshold (for escalation) */
    else if (result->aggregate_p_harm > bridge->config.compound_harm_threshold) {
        if (result->result == LGSS_RESULT_ALLOW) {
            result->result = LGSS_RESULT_ESCALATE;
        }
        snprintf(result->explanation, sizeof(result->explanation),
                 "Compound harm %.2f exceeds threshold %.2f - requires approval",
                 result->aggregate_p_harm, bridge->config.compound_harm_threshold);
        snprintf(result->recommended_action, sizeof(result->recommended_action),
                 "Seek human approval before execution");
    }
    /* Check if any dangerous nodes were found */
    else if (result->num_dangerous_nodes > 0) {
        result->result = LGSS_RESULT_DENY;
        result->is_safe = false;
        snprintf(result->explanation, sizeof(result->explanation),
                 "Found %u dangerous nodes (most dangerous: node %u, p_harm=%.2f)",
                 result->num_dangerous_nodes,
                 result->most_dangerous_node_id,
                 result->most_dangerous_p_harm);
        snprintf(result->recommended_action, sizeof(result->recommended_action),
                 "Remove or mitigate dangerous nodes");
    }
    /* Plan is safe */
    else if (result->result == LGSS_RESULT_ALLOW) {
        snprintf(result->explanation, sizeof(result->explanation),
                 "Plan validated: %u nodes, aggregate_harm=%.2f, min_reversibility=%.2f",
                 result->nodes_evaluated,
                 result->aggregate_p_harm,
                 result->min_reversibility);
        snprintf(result->recommended_action, sizeof(result->recommended_action),
                 "Proceed with execution");
    }

    /* Update statistics based on result */
    switch (result->result) {
        case LGSS_RESULT_ALLOW:
            bridge->stats.plans_allowed++;
            break;
        case LGSS_RESULT_DENY:
            bridge->stats.plans_denied++;
            break;
        case LGSS_RESULT_ESCALATE:
            bridge->stats.plans_escalated++;
            break;
        default:
            break;
    }

    /* Calculate evaluation time */
    result->evaluation_time_us = get_timestamp_us() - start_time;
    bridge->stats.avg_validation_time_us = update_running_avg(
        bridge->stats.avg_validation_time_us,
        bridge->stats.plans_validated,
        (float)result->evaluation_time_us);

    BRIDGE_UNLOCK(bridge);

    return (rc < 0) ? 0 : rc;  /* Convert early termination to success */
}

int planning_safety_validate_action(
    planning_safety_bridge_t* bridge,
    const char* action,
    lgss_safety_domain_t domain,
    const char* target_type,
    action_validation_result_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(action);
    BRIDGE_NULL_CHECK(result);

    /* Validate magic */
    if (bridge->magic != LGSS_PLAN_BRIDGE_MAGIC) {
        return NIMCP_ERROR_INVALID_STATE;
    }

    BRIDGE_LOCK(bridge);

    memset(result, 0, sizeof(action_validation_result_t));

    /* Update statistics */
    bridge->stats.actions_validated++;

    /* Create temporary node for evaluation */
    plan_node_t temp_node;
    memset(&temp_node, 0, sizeof(temp_node));
    strncpy(temp_node.action, action, sizeof(temp_node.action) - 1);
    temp_node.domain = domain;
    if (target_type) {
        strncpy(temp_node.target_type, target_type, sizeof(temp_node.target_type) - 1);
    }

    /* Default harm estimate for unknown actions */
    if (bridge->config.fail_safe_on_unknown) {
        temp_node.p_harm = bridge->config.default_unknown_p_harm;
    } else {
        temp_node.p_harm = 0.1f;  /* Low default if not fail-safe */
    }
    temp_node.reversibility = 0.5f;  /* Default medium reversibility */

    /* Evaluate */
    float adjusted_p_harm;
    lgss_result_t eval_result = evaluate_node_safety(bridge, &temp_node, &adjusted_p_harm);

    result->result = eval_result;
    result->p_harm = adjusted_p_harm;
    result->reversibility = temp_node.reversibility;
    result->is_safe = (eval_result == LGSS_RESULT_ALLOW);

    /* Generate explanation */
    switch (eval_result) {
        case LGSS_RESULT_ALLOW:
            bridge->stats.actions_allowed++;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Action permitted (p_harm=%.2f)", adjusted_p_harm);
            break;
        case LGSS_RESULT_DENY:
            bridge->stats.actions_denied++;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Action blocked: p_harm=%.2f exceeds threshold", adjusted_p_harm);
            break;
        case LGSS_RESULT_ESCALATE:
            snprintf(result->explanation, sizeof(result->explanation),
                     "Action requires approval (p_harm=%.2f)", adjusted_p_harm);
            break;
        default:
            snprintf(result->explanation, sizeof(result->explanation),
                     "Action evaluation error");
            break;
    }

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int planning_safety_estimate_harm(
    planning_safety_bridge_t* bridge,
    const harm_estimation_params_t* params,
    harm_estimation_result_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(params);
    BRIDGE_NULL_CHECK(result);

    /* Validate magic */
    if (bridge->magic != LGSS_PLAN_BRIDGE_MAGIC) {
        return NIMCP_ERROR_INVALID_STATE;
    }

    BRIDGE_LOCK(bridge);

    memset(result, 0, sizeof(harm_estimation_result_t));

    /* Update statistics */
    bridge->stats.harm_estimates++;

    /* Base direct harm estimation based on domain */
    float base_harm = 0.1f;  /* Default low harm */

    switch (params->domain) {
        case LGSS_DOMAIN_PHYSICAL_ACTION:
            base_harm = 0.3f;  /* Physical actions inherently riskier */
            break;
        case LGSS_DOMAIN_HUMAN_INTERACTION:
            base_harm = 0.25f;  /* Human interaction has consequences */
            break;
        case LGSS_DOMAIN_SYSTEM_CONTROL:
            base_harm = 0.2f;  /* System control can have side effects */
            break;
        case LGSS_DOMAIN_LEARNING:
            base_harm = 0.15f;  /* Learning changes behavior */
            break;
        case LGSS_DOMAIN_GOAL_MODIFICATION:
            base_harm = 0.4f;  /* Goal modification is high impact */
            break;
        case LGSS_DOMAIN_DATA_ACCESS:
            base_harm = 0.1f;  /* Data access generally lower risk */
            break;
        default:
            base_harm = 0.1f;
            break;
    }

    /* Apply context risk factor */
    float context_factor = nimcp_clampf(params->context_risk_factor, 0.5f, 2.0f);
    result->direct_p_harm = base_harm * context_factor;

    /* Estimate indirect/cascading harm if enabled */
    if (params->consider_indirect_effects && bridge->config.include_indirect_effects) {
        /* Indirect harm is typically 20-50% of direct harm */
        result->indirect_p_harm = result->direct_p_harm * 0.35f;
    } else {
        result->indirect_p_harm = 0.0f;
    }

    /* Calculate total harm: p_total = 1 - (1-p_direct)(1-p_indirect) */
    result->total_p_harm = 1.0f - (1.0f - result->direct_p_harm) * (1.0f - result->indirect_p_harm);

    /* Add uncertainty margin if enabled */
    if (params->consider_uncertainty && bridge->config.include_uncertainty) {
        result->uncertainty_margin = bridge->config.base_uncertainty_margin;
        /* Uncertainty increases with base harm */
        result->uncertainty_margin += result->total_p_harm * 0.1f;
    } else {
        result->uncertainty_margin = 0.0f;
    }

    /* Calculate confidence (inverse of uncertainty) */
    result->confidence = 1.0f - result->uncertainty_margin;
    if (result->confidence < 0.3f) result->confidence = 0.3f;

    /* Generate risk factors description */
    snprintf(result->risk_factors, sizeof(result->risk_factors),
             "domain=%d, context_factor=%.2f, indirect_effects=%s",
             params->domain,
             context_factor,
             params->consider_indirect_effects ? "yes" : "no");

    /* Update average estimated harm */
    bridge->stats.avg_estimated_harm = update_running_avg(
        bridge->stats.avg_estimated_harm,
        bridge->stats.harm_estimates,
        result->total_p_harm);

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Helper Functions Implementation
 * ============================================================================ */

float planning_safety_aggregate_harm(
    const float* p_harm_values,
    uint32_t count
) {
    if (!p_harm_values || count == 0) {
        return 0.0f;
    }

    /*
     * Aggregate harm using the formula:
     * p_total = 1 - PRODUCT(1 - p_harm_i)
     *
     * This represents the probability that at least one
     * harmful outcome occurs across all actions.
     */
    float product = 1.0f;
    for (uint32_t i = 0; i < count; i++) {
        float p_safe = 1.0f - p_harm_values[i];
        if (p_safe < 0.0f) p_safe = 0.0f;
        if (p_safe > 1.0f) p_safe = 1.0f;
        product *= p_safe;
    }

    float aggregate = 1.0f - product;
    return nimcp_clampf(aggregate, 0.0f, 1.0f);
}

plan_node_t* planning_safety_create_node(
    const char* action,
    float p_harm,
    float reversibility
) {
    plan_node_t* node = nimcp_malloc(sizeof(plan_node_t));
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "node is NULL");

        return NULL;
    }

    memset(node, 0, sizeof(plan_node_t));

    if (action) {
        strncpy(node->action, action, sizeof(node->action) - 1);
    }

    node->p_harm = nimcp_clampf(p_harm, 0.0f, 1.0f);
    node->reversibility = nimcp_clampf(reversibility, 0.0f, 1.0f);
    node->is_terminal = true;  /* Default to leaf until children added */
    node->domain = LGSS_DOMAIN_GENERAL;

    return node;
}

void planning_safety_destroy_node(plan_node_t* node) {
    if (!node) {
        return;
    }

    /* Recursively destroy children */
    if (node->children) {
        for (uint32_t i = 0; i < node->num_children; i++) {
            planning_safety_destroy_node(node->children[i]);
        }
        nimcp_free(node->children);
    }

    nimcp_free(node);
}

int planning_safety_add_child(plan_node_t* parent, plan_node_t* child) {
    if (!parent || !child) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (parent->num_children >= LGSS_PLAN_MAX_CHILDREN) {
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    /* Allocate or grow children array */
    if (!parent->children) {
        parent->children = nimcp_malloc(sizeof(plan_node_t*) * LGSS_PLAN_MAX_CHILDREN);
        if (!parent->children) {
            return NIMCP_ERROR_NO_MEMORY;
        }
        memset(parent->children, 0, sizeof(plan_node_t*) * LGSS_PLAN_MAX_CHILDREN);
    }

    parent->children[parent->num_children] = child;
    parent->num_children++;
    parent->is_terminal = false;

    /* Update child depth */
    child->depth = parent->depth + 1;

    return 0;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

int planning_safety_get_stats(
    const planning_safety_bridge_t* bridge,
    planning_safety_stats_t* stats
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(stats);

    planning_safety_bridge_t* mutable_bridge = (planning_safety_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);
    *stats = bridge->stats;
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int planning_safety_reset_stats(planning_safety_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    memset(&bridge->stats, 0, sizeof(planning_safety_stats_t));
    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

int planning_safety_connect_aix(
    planning_safety_bridge_t* bridge,
    action_interceptor_t* aix
) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    bridge->aix = aix;
    bridge->base.system_a = aix;
    bridge->base.system_a_connected = (aix != NULL);
    bridge->base.bridge_active = bridge->base.system_a_connected;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

bool planning_safety_is_connected(const planning_safety_bridge_t* bridge) {
    if (!bridge) { return false; }
    return bridge->base.system_a_connected;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

int planning_safety_connect_bio_async(planning_safety_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);
    return bridge_base_connect_bio_async(&bridge->base);
}

int planning_safety_disconnect_bio_async(planning_safety_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);
    return bridge_base_disconnect_bio_async(&bridge->base);
}

bool planning_safety_is_bio_async_connected(const planning_safety_bridge_t* bridge) {
    if (!bridge) { return false; }
    return bridge_base_is_bio_async_connected(&bridge->base);
}
