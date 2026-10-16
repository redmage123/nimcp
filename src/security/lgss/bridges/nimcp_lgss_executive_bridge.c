/**
 * @file nimcp_lgss_executive_bridge.c
 * @brief LGSS Executive Safety Bridge Implementation - Component A6
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Implementation of safety interception for executive function outputs
 * WHY:  ALL cognitive tasks must be validated to prevent harmful actions
 * HOW:  Convert proposals to safety contexts, evaluate via AIx, log decisions
 *
 * @author NIMCP Development Team
 */

#include "security/lgss/bridges/nimcp_lgss_executive_bridge.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "constants/nimcp_timing_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lgss_executive_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/** Module ID for bio-async */
#define LGSS_EXEC_MODULE_ID            0x4C475345  /* 'LGSE' */

/** Default evaluation timeout (ms) */
#define DEFAULT_EVALUATION_TIMEOUT_MS  NIMCP_FAST_TIMEOUT_MS

/** Default blocked log size */
#define DEFAULT_BLOCKED_LOG_SIZE       128

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
 * @brief Add entry to blocked task log (unlocked)
 */
static void add_blocked_log_entry_unlocked(
    executive_safety_bridge_t* bridge,
    uint64_t task_id,
    lgss_result_t result,
    const char* rule_id,
    const char* description,
    float p_harm
) {
    if (!bridge->blocked_log) {
        return;
    }

    uint32_t max_entries = bridge->config.max_blocked_log_entries;
    if (max_entries == 0) {
        max_entries = DEFAULT_BLOCKED_LOG_SIZE;
    }

    blocked_task_entry_t* entry = &bridge->blocked_log[bridge->blocked_log_head];

    entry->task_id = task_id;
    entry->timestamp_us = get_timestamp_us();
    entry->result = result;
    entry->p_harm = p_harm;

    if (rule_id) {
        strncpy(entry->rule_id, rule_id, sizeof(entry->rule_id) - 1);
        entry->rule_id[sizeof(entry->rule_id) - 1] = '\0';
    } else {
        entry->rule_id[0] = '\0';
    }

    if (description) {
        strncpy(entry->description, description, sizeof(entry->description) - 1);
        entry->description[sizeof(entry->description) - 1] = '\0';
    } else {
        entry->description[0] = '\0';
    }

    bridge->blocked_log_head = (bridge->blocked_log_head + 1) % max_entries;
    if (bridge->blocked_log_count < max_entries) {
        bridge->blocked_log_count++;
    }
}

/**
 * @brief Evaluate safety using internal rules (when AIx not connected)
 *
 * WHAT: Basic safety evaluation without full AIx
 * WHY:  Provide fail-safe behavior when AIx unavailable
 * HOW:  Check harm thresholds and domain rules
 */
static lgss_result_t evaluate_safety_basic(
    executive_safety_bridge_t* bridge,
    const safety_action_context_t* context,
    safety_evaluation_result_t* eval
) {
    memset(eval, 0, sizeof(safety_evaluation_result_t));
    eval->confidence = 0.5f;  /* Lower confidence without full AIx */

    /* Check harm probability threshold */
    if (context->p_harm > bridge->config.harm_threshold) {
        eval->result = LGSS_RESULT_DENY;
        snprintf(eval->explanation, sizeof(eval->explanation),
                 "Harm probability %.2f exceeds threshold %.2f",
                 context->p_harm, bridge->config.harm_threshold);
        return LGSS_RESULT_DENY;
    }

    /* Check reversibility threshold - escalate if too low */
    if (context->reversibility < bridge->config.reversibility_threshold) {
        eval->result = LGSS_RESULT_ESCALATE;
        snprintf(eval->explanation, sizeof(eval->explanation),
                 "Reversibility %.2f below threshold %.2f - requires human approval",
                 context->reversibility, bridge->config.reversibility_threshold);
        return LGSS_RESULT_ESCALATE;
    }

    /* Check for high-risk domains */
    if (context->domain == LGSS_DOMAIN_PHYSICAL_ACTION ||
        context->domain == LGSS_DOMAIN_HUMAN_INTERACTION) {
        if (context->p_harm > 0.1f) {
            eval->result = LGSS_RESULT_ESCALATE;
            snprintf(eval->explanation, sizeof(eval->explanation),
                     "High-risk domain with harm probability %.2f - requires human approval",
                     context->p_harm);
            return LGSS_RESULT_ESCALATE;
        }
    }

    /* Check for goal modification - always escalate */
    if (context->domain == LGSS_DOMAIN_GOAL_MODIFICATION) {
        eval->result = LGSS_RESULT_ESCALATE;
        snprintf(eval->explanation, sizeof(eval->explanation),
                 "Goal modification requires human approval");
        return LGSS_RESULT_ESCALATE;
    }

    /* Default: allow */
    eval->result = LGSS_RESULT_ALLOW;
    snprintf(eval->explanation, sizeof(eval->explanation), "Action permitted");
    return LGSS_RESULT_ALLOW;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int executive_safety_default_config(executive_safety_config_t* config) {
    if (!config) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(config, 0, sizeof(executive_safety_config_t));

    /* Interception flags - ALL enabled by default */
    config->intercept_task_queue = true;
    config->intercept_planning = true;
    config->intercept_goal_formation = true;

    /* Limits */
    config->max_pending_tasks = LGSS_EXEC_MAX_PENDING_TASKS;
    config->evaluation_timeout_ms = DEFAULT_EVALUATION_TIMEOUT_MS;

    /* Thresholds */
    config->harm_threshold = LGSS_EXEC_DEFAULT_HARM_THRESHOLD;
    config->reversibility_threshold = LGSS_EXEC_DEFAULT_REVERSIBILITY_THRESHOLD;
    config->value_threshold = 0.0f;

    /* Fail-safe - ALWAYS enabled */
    config->fail_safe_on_timeout = true;
    config->fail_safe_on_error = true;

    /* Logging */
    config->enable_audit_logging = true;
    config->max_blocked_log_entries = LGSS_EXEC_MAX_BLOCKED_LOG;

    /* Sensitivity */
    config->safety_sensitivity = 1.0f;

    return 0;
}

executive_safety_bridge_t* executive_safety_bridge_create(
    action_interceptor_t* aix,
    executive_controller_t* executive
) {
    executive_safety_config_t config;
    executive_safety_default_config(&config);
    return executive_safety_bridge_create_custom(aix, executive, &config);
}

executive_safety_bridge_t* executive_safety_bridge_create_custom(
    action_interceptor_t* aix,
    executive_controller_t* executive,
    const executive_safety_config_t* config
) {
    /* Allocate bridge structure */
    executive_safety_bridge_t* bridge = nimcp_malloc(sizeof(executive_safety_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate executive_safety_bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }
    memset(bridge, 0, sizeof(executive_safety_bridge_t));

    /* Set magic */
    bridge->magic = LGSS_EXEC_BRIDGE_MAGIC;

    /* Initialize config */
    if (config) {
        bridge->config = *config;
    } else {
        executive_safety_default_config(&bridge->config);
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, LGSS_EXEC_MODULE_ID, "lgss_executive_bridge") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "executive_safety_bridge_create_custom: validation failed");
        return NULL;
    }

    /* Allocate blocked task log */
    uint32_t log_size = bridge->config.max_blocked_log_entries;
    if (log_size == 0) {
        log_size = DEFAULT_BLOCKED_LOG_SIZE;
    }

    bridge->blocked_log = nimcp_malloc(sizeof(blocked_task_entry_t) * log_size);
    if (!bridge->blocked_log) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "executive_safety_bridge_create_custom: bridge->blocked_log is NULL");
        return NULL;
    }
    memset(bridge->blocked_log, 0, sizeof(blocked_task_entry_t) * log_size);
    bridge->blocked_log_head = 0;
    bridge->blocked_log_count = 0;

    /* Connect systems */
    bridge->aix = aix;
    bridge->executive = executive;

    /* Initialize proposal ID counter */
    bridge->next_proposal_id = 1;

    /* Set connection status in base */
    bridge->base.system_a = aix;
    bridge->base.system_b = executive;
    bridge->base.system_a_connected = (aix != NULL);
    bridge->base.system_b_connected = (executive != NULL);
    bridge->base.bridge_active = bridge->base.system_a_connected;

    NIMCP_LOGGING_INFO("Created LGSS executive safety bridge");

    return bridge;
}

void executive_safety_bridge_destroy(executive_safety_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Validate magic */
    if (bridge->magic != LGSS_EXEC_BRIDGE_MAGIC) {
        NIMCP_LOGGING_ERROR("Invalid bridge magic in destroy");
        return;
    }

    /* Free blocked log */
    if (bridge->blocked_log) {
        nimcp_free(bridge->blocked_log);
        bridge->blocked_log = NULL;
    }

    /* Cleanup base bridge */
    bridge_base_cleanup(&bridge->base);

    /* Clear magic before free */
    bridge->magic = 0;

    /* Free bridge structure */
    nimcp_free(bridge);
}

/* ============================================================================
 * Core Safety Gating API Implementation
 * ============================================================================ */

int executive_safety_propose_task(
    executive_safety_bridge_t* bridge,
    const executive_task_proposal_t* task,
    aix_decision_t* decision
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(task);
    BRIDGE_NULL_CHECK(decision);

    /* Validate magic */
    if (bridge->magic != LGSS_EXEC_BRIDGE_MAGIC) {
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Check if task interception is enabled */
    if (!bridge->config.intercept_task_queue) {
        /* Pass through without evaluation */
        memset(decision, 0, sizeof(aix_decision_t));
        decision->result = LGSS_RESULT_ALLOW;
        decision->proposal_id = task->task_id;
        return 0;
    }

    uint64_t start_time = get_timestamp_us();

    BRIDGE_LOCK(bridge);

    /* Convert task to safety context */
    safety_action_context_t context;
    executive_safety_task_to_context(task, &context);

    /* Apply sensitivity scaling */
    float sensitivity = nimcp_clampf(bridge->config.safety_sensitivity, 0.5f, 2.0f);
    context.p_harm *= sensitivity;
    if (context.p_harm > 1.0f) context.p_harm = 1.0f;

    /* Initialize decision */
    memset(decision, 0, sizeof(aix_decision_t));
    decision->proposal_id = bridge->next_proposal_id++;

    /* Update statistics */
    bridge->stats.tasks_proposed++;

    /* Evaluate safety */
    lgss_result_t result = evaluate_safety_basic(bridge, &context, &decision->safety_eval);
    decision->result = result;

    /* Calculate processing time */
    decision->processing_time_us = get_timestamp_us() - start_time;

    /* Update running average */
    bridge->stats.avg_evaluation_time_us = update_running_avg(
        bridge->stats.avg_evaluation_time_us,
        bridge->stats.tasks_proposed,
        (float)decision->processing_time_us);

    /* Handle result */
    switch (result) {
        case LGSS_RESULT_ALLOW:
            bridge->stats.tasks_allowed++;
            break;

        case LGSS_RESULT_DENY:
            bridge->stats.tasks_denied++;
            /* Log blocked task */
            if (bridge->config.enable_audit_logging) {
                add_blocked_log_entry_unlocked(
                    bridge, task->task_id, result,
                    decision->safety_eval.matched_rule_id,
                    task->description, task->p_harm);
            }
            NIMCP_LOGGING_WARN("LGSS BLOCKED task %lu: %s (p_harm=%.2f)",
                              (unsigned long)task->task_id, task->description, task->p_harm);
            break;

        case LGSS_RESULT_ESCALATE:
            bridge->stats.tasks_escalated++;
            decision->escalation_pending = true;
            decision->escalation_id = decision->proposal_id;
            NIMCP_LOGGING_INFO("LGSS ESCALATED task %lu: %s",
                              (unsigned long)task->task_id, task->description);
            break;

        case LGSS_RESULT_TIMEOUT:
            bridge->stats.timeouts++;
            if (bridge->config.fail_safe_on_timeout) {
                decision->result = LGSS_RESULT_DENY;
                bridge->stats.tasks_denied++;
            }
            break;

        case LGSS_RESULT_ERROR:
            bridge->stats.errors++;
            if (bridge->config.fail_safe_on_error) {
                decision->result = LGSS_RESULT_DENY;
                bridge->stats.tasks_denied++;
            }
            break;
    }

    bridge->stats.last_update_time_us = get_timestamp_us();

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int executive_safety_validate_plan(
    executive_safety_bridge_t* bridge,
    const void* plan,
    aix_decision_t* decision
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(plan);
    BRIDGE_NULL_CHECK(decision);

    /* Validate magic */
    if (bridge->magic != LGSS_EXEC_BRIDGE_MAGIC) {
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Check if planning interception is enabled */
    if (!bridge->config.intercept_planning) {
        memset(decision, 0, sizeof(aix_decision_t));
        decision->result = LGSS_RESULT_ALLOW;
        return 0;
    }

    uint64_t start_time = get_timestamp_us();

    BRIDGE_LOCK(bridge);

    /* Initialize decision */
    memset(decision, 0, sizeof(aix_decision_t));
    decision->proposal_id = bridge->next_proposal_id++;

    /* Update statistics */
    bridge->stats.plans_validated++;

    /*
     * For full plan validation, we would call the planning safety bridge.
     * Here we provide basic validation that checks if plans exist.
     */

    /* Basic plan validation - in production, would use planning_safety_bridge */
    decision->result = LGSS_RESULT_ALLOW;
    decision->safety_eval.result = LGSS_RESULT_ALLOW;
    decision->safety_eval.confidence = 0.7f;
    snprintf(decision->safety_eval.explanation, sizeof(decision->safety_eval.explanation),
             "Plan validated (basic check)");

    bridge->stats.plans_allowed++;

    decision->processing_time_us = get_timestamp_us() - start_time;
    bridge->stats.last_update_time_us = get_timestamp_us();

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int executive_safety_propose_goal(
    executive_safety_bridge_t* bridge,
    const goal_proposal_t* goal,
    aix_decision_t* decision
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(goal);
    BRIDGE_NULL_CHECK(decision);

    /* Validate magic */
    if (bridge->magic != LGSS_EXEC_BRIDGE_MAGIC) {
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Check if goal interception is enabled */
    if (!bridge->config.intercept_goal_formation) {
        memset(decision, 0, sizeof(aix_decision_t));
        decision->result = LGSS_RESULT_ALLOW;
        decision->proposal_id = goal->goal_id;
        return 0;
    }

    uint64_t start_time = get_timestamp_us();

    BRIDGE_LOCK(bridge);

    /* Convert goal to safety context */
    safety_action_context_t context;
    executive_safety_goal_to_context(goal, &context);

    /* Apply sensitivity scaling */
    float sensitivity = nimcp_clampf(bridge->config.safety_sensitivity, 0.5f, 2.0f);
    context.p_harm *= sensitivity;
    if (context.p_harm > 1.0f) context.p_harm = 1.0f;

    /* Initialize decision */
    memset(decision, 0, sizeof(aix_decision_t));
    decision->proposal_id = bridge->next_proposal_id++;

    /* Update statistics */
    bridge->stats.goals_proposed++;

    /* Special handling for goals - more conservative */
    lgss_result_t result;

    /* Goals that modify self are ALWAYS escalated */
    if (goal->modifies_self) {
        result = LGSS_RESULT_ESCALATE;
        decision->safety_eval.result = LGSS_RESULT_ESCALATE;
        snprintf(decision->safety_eval.explanation,
                 sizeof(decision->safety_eval.explanation),
                 "Self-modifying goal requires human approval");
    }
    /* Goals affecting humans with any harm potential are escalated */
    else if (goal->affects_humans && goal->p_harm > 0.05f) {
        result = LGSS_RESULT_ESCALATE;
        decision->safety_eval.result = LGSS_RESULT_ESCALATE;
        snprintf(decision->safety_eval.explanation,
                 sizeof(decision->safety_eval.explanation),
                 "Goal affecting humans with p_harm=%.2f requires approval",
                 goal->p_harm);
    }
    /* Check value threshold */
    else if (goal->value_estimate < bridge->config.value_threshold) {
        result = LGSS_RESULT_DENY;
        decision->safety_eval.result = LGSS_RESULT_DENY;
        snprintf(decision->safety_eval.explanation,
                 sizeof(decision->safety_eval.explanation),
                 "Goal value %.2f below threshold %.2f",
                 goal->value_estimate, bridge->config.value_threshold);
    }
    /* Standard evaluation */
    else {
        result = evaluate_safety_basic(bridge, &context, &decision->safety_eval);
    }

    decision->result = result;
    decision->processing_time_us = get_timestamp_us() - start_time;

    /* Handle result */
    switch (result) {
        case LGSS_RESULT_ALLOW:
            bridge->stats.goals_allowed++;
            break;

        case LGSS_RESULT_DENY:
            bridge->stats.goals_denied++;
            NIMCP_LOGGING_WARN("LGSS BLOCKED goal %lu: %s",
                              (unsigned long)goal->goal_id, goal->description);
            break;

        case LGSS_RESULT_ESCALATE:
            bridge->stats.goals_escalated++;
            decision->escalation_pending = true;
            decision->escalation_id = decision->proposal_id;
            NIMCP_LOGGING_INFO("LGSS ESCALATED goal %lu: %s",
                              (unsigned long)goal->goal_id, goal->description);
            break;

        default:
            if (bridge->config.fail_safe_on_error) {
                decision->result = LGSS_RESULT_DENY;
                bridge->stats.goals_denied++;
            }
            break;
    }

    bridge->stats.last_update_time_us = get_timestamp_us();

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Helper Functions Implementation
 * ============================================================================ */

int executive_safety_task_to_context(
    const executive_task_proposal_t* task,
    safety_action_context_t* context
) {
    if (!task || !context) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(context, 0, sizeof(safety_action_context_t));

    /* Copy operation */
    strncpy(context->operation, task->operation, sizeof(context->operation) - 1);

    /* Copy target type */
    strncpy(context->target_type, task->target_type, sizeof(context->target_type) - 1);

    /* Set domain */
    context->domain = task->domain;

    /* Copy harm and reversibility */
    context->p_harm = task->p_harm;
    context->reversibility = task->reversibility;

    /* Set scope based on domain */
    if (task->domain == LGSS_DOMAIN_PHYSICAL_ACTION ||
        task->domain == LGSS_DOMAIN_HUMAN_INTERACTION) {
        strncpy(context->scope, "local", sizeof(context->scope) - 1);
    } else {
        strncpy(context->scope, "internal", sizeof(context->scope) - 1);
    }

    /* Set estimated impact based on harm probability */
    if (task->p_harm > 0.7f) {
        strncpy(context->estimated_impact, "severe", sizeof(context->estimated_impact) - 1);
    } else if (task->p_harm > 0.4f) {
        strncpy(context->estimated_impact, "moderate", sizeof(context->estimated_impact) - 1);
    } else if (task->p_harm > 0.1f) {
        strncpy(context->estimated_impact, "minor", sizeof(context->estimated_impact) - 1);
    } else {
        strncpy(context->estimated_impact, "minimal", sizeof(context->estimated_impact) - 1);
    }

    return 0;
}

int executive_safety_goal_to_context(
    const goal_proposal_t* goal,
    safety_action_context_t* context
) {
    if (!goal || !context) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(context, 0, sizeof(safety_action_context_t));

    /* Set operation as goal formation */
    strncpy(context->operation, "form_goal", sizeof(context->operation) - 1);

    /* Target type based on goal characteristics */
    if (goal->affects_humans) {
        strncpy(context->target_type, "human", sizeof(context->target_type) - 1);
    } else if (goal->modifies_self) {
        strncpy(context->target_type, "self", sizeof(context->target_type) - 1);
    } else {
        strncpy(context->target_type, "system", sizeof(context->target_type) - 1);
    }

    /* Set domain - goal modification is special */
    context->domain = LGSS_DOMAIN_GOAL_MODIFICATION;

    /* Copy harm and reversibility */
    context->p_harm = goal->p_harm;
    context->reversibility = goal->reversibility;

    /* Goals have broader scope */
    strncpy(context->scope, "persistent", sizeof(context->scope) - 1);

    /* Set estimated impact based on urgency and harm */
    if (goal->urgency > 0.8f || goal->p_harm > 0.5f) {
        strncpy(context->estimated_impact, "significant", sizeof(context->estimated_impact) - 1);
    } else {
        strncpy(context->estimated_impact, "moderate", sizeof(context->estimated_impact) - 1);
    }

    return 0;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

int executive_safety_get_stats(
    const executive_safety_bridge_t* bridge,
    executive_safety_stats_t* stats
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(stats);

    executive_safety_bridge_t* mutable_bridge = (executive_safety_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);
    *stats = bridge->stats;
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int executive_safety_get_blocked_log(
    const executive_safety_bridge_t* bridge,
    blocked_task_entry_t* entries,
    uint32_t max_entries,
    uint32_t* num_entries
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(entries);
    BRIDGE_NULL_CHECK(num_entries);

    if (max_entries == 0) {
        *num_entries = 0;
        return 0;
    }

    executive_safety_bridge_t* mutable_bridge = (executive_safety_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);

    uint32_t count = bridge->blocked_log_count;
    if (count > max_entries) {
        count = max_entries;
    }

    uint32_t log_size = bridge->config.max_blocked_log_entries;
    if (log_size == 0) {
        log_size = DEFAULT_BLOCKED_LOG_SIZE;
    }

    /* Copy entries from circular buffer, oldest first */
    uint32_t read_idx;
    if (bridge->blocked_log_count < log_size) {
        read_idx = 0;
    } else {
        read_idx = bridge->blocked_log_head;
    }

    for (uint32_t i = 0; i < count; i++) {
        entries[i] = bridge->blocked_log[read_idx];
        read_idx = (read_idx + 1) % log_size;
    }

    *num_entries = count;

    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int executive_safety_reset_stats(executive_safety_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    memset(&bridge->stats, 0, sizeof(executive_safety_stats_t));
    BRIDGE_UNLOCK(bridge);

    return 0;
}

int executive_safety_clear_blocked_log(executive_safety_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    bridge->blocked_log_head = 0;
    bridge->blocked_log_count = 0;

    uint32_t log_size = bridge->config.max_blocked_log_entries;
    if (log_size == 0) {
        log_size = DEFAULT_BLOCKED_LOG_SIZE;
    }

    if (bridge->blocked_log) {
        memset(bridge->blocked_log, 0, sizeof(blocked_task_entry_t) * log_size);
    }

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

int executive_safety_connect_aix(
    executive_safety_bridge_t* bridge,
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

int executive_safety_connect_executive(
    executive_safety_bridge_t* bridge,
    executive_controller_t* executive
) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    bridge->executive = executive;
    bridge->base.system_b = executive;
    bridge->base.system_b_connected = (executive != NULL);
    BRIDGE_UNLOCK(bridge);

    return 0;
}

bool executive_safety_is_connected(const executive_safety_bridge_t* bridge) {
    if (!bridge) { return false; }
    /* Bridge is functional if AIx is connected (executive is optional) */
    return bridge->base.system_a_connected;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

int executive_safety_connect_bio_async(executive_safety_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);
    return bridge_base_connect_bio_async(&bridge->base);
}

int executive_safety_disconnect_bio_async(executive_safety_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);
    return bridge_base_disconnect_bio_async(&bridge->base);
}

bool executive_safety_is_bio_async_connected(const executive_safety_bridge_t* bridge) {
    if (!bridge) { return false; }
    return bridge_base_is_bio_async_connected(&bridge->base);
}
