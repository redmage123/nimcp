/**
 * @file nimcp_lgss_action_interceptor.c
 * @brief Implementation of LGSS Action Interceptor (AIx)
 *
 * WHAT: Central gateway for ALL cognitive action safety evaluation
 * WHY:  Ensure every action passes safety checks before execution
 * HOW:  Thread-safe evaluation with fail-safe defaults
 *
 * SECURITY CRITICAL: This module implements the ONLY authorized path
 *                    for cognitive actions. NO BYPASS is permitted.
 *
 * FAIL-SAFE DESIGN:
 * - Default DENY on timeout
 * - Default DENY on error
 * - Default DENY on missing safety KB
 * - All decisions are logged
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 1.0.0
 */

#include "security/lgss/nimcp_lgss_action_interceptor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "async/nimcp_bio_async.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdatomic.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lgss_action_interceptor, MESH_ADAPTER_CATEGORY_SECURITY)


/*=============================================================================
 * MODULE CONSTANTS
 *============================================================================*/

#define MODULE_NAME "AIx"

/*=============================================================================
 * INTERNAL STRUCTURES
 *============================================================================*/

/**
 * @brief Pending proposal entry
 */
typedef struct pending_proposal {
    aix_proposal_t proposal;
    aix_decision_t decision;
    bool decision_ready;
    uint64_t submit_time_us;
    struct pending_proposal* next;
} pending_proposal_t;

/**
 * @brief Escalation entry
 */
typedef struct escalation_entry {
    aix_escalation_t escalation;
    struct escalation_entry* next;
} escalation_entry_t;

/**
 * @brief Internal action interceptor implementation
 */
struct action_interceptor_impl {
    uint32_t magic;                         /**< Magic number for validation */
    aix_config_t config;                    /**< Configuration */
    nimcp_platform_mutex_t mutex;           /**< Main mutex */

    /** Safety knowledge base (pointer) */
    safety_kb_t* safety_kb;

    /** Pending proposals */
    pending_proposal_t* pending_head;
    pending_proposal_t* pending_tail;
    uint32_t pending_count;

    /** Escalation queue */
    escalation_entry_t* escalation_head;
    escalation_entry_t* escalation_tail;
    uint32_t escalation_count;

    /** ID generators */
    atomic_uint_fast64_t next_proposal_id;
    atomic_uint_fast64_t next_escalation_id;

    /** Statistics */
    aix_stats_t stats;
    uint64_t total_processing_time_us;
};

/*=============================================================================
 * INTERNAL HELPER FUNCTIONS
 *============================================================================*/

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Validate AIx handle
 */
static inline bool is_valid_aix(action_interceptor_t aix) {
    return aix != NULL && aix->magic == NIMCP_AIX_MAGIC;
}

/**
 * @brief Lock AIx mutex
 */
static inline void aix_lock(action_interceptor_t aix) {
    nimcp_platform_mutex_lock(&aix->mutex);
}

/**
 * @brief Unlock AIx mutex
 */
static inline void aix_unlock(action_interceptor_t aix) {
    nimcp_platform_mutex_unlock(&aix->mutex);
}

/*=============================================================================
 * BACKGROUND EVALUATION SUPPORT
 *============================================================================*/

/**
 * @brief Context for background evaluation thread
 */
typedef struct {
    action_interceptor_t aix;
    uint64_t proposal_id;
} bg_eval_context_t;

/**
 * @brief Forward declaration for evaluate_safety_unlocked
 */
static void evaluate_safety_unlocked(
    action_interceptor_t aix,
    const aix_proposal_t* proposal,
    aix_safety_eval_t* safety_eval
);

/**
 * @brief Forward declaration for determine_result
 */
static aix_result_t determine_result(
    action_interceptor_t aix,
    const aix_safety_eval_t* safety_eval
);

/**
 * @brief Forward declaration for create_escalation_unlocked
 */
static nimcp_error_t create_escalation_unlocked(
    action_interceptor_t aix,
    const aix_proposal_t* proposal,
    const aix_safety_eval_t* safety_eval,
    uint64_t* escalation_id
);

/**
 * @brief Forward declaration for find_pending_unlocked
 */
static pending_proposal_t* find_pending_unlocked(
    action_interceptor_t aix,
    uint64_t proposal_id
);

/**
 * @brief Forward declaration for log_audit
 */
static void log_audit(
    action_interceptor_t aix,
    const aix_proposal_t* proposal,
    const aix_decision_t* decision
);

/**
 * @brief Background evaluation thread function
 *
 * WHAT: Evaluate pending proposal in background thread
 * WHY:  Non-blocking async evaluation for performance-critical paths
 * HOW:  Use bio-async promise to signal completion
 */
static void* background_evaluation_thread(void* arg) {
    bg_eval_context_t* ctx = (bg_eval_context_t*)arg;

    if (!ctx || !is_valid_aix(ctx->aix)) {
        nimcp_free(ctx);
        return NULL;
    }

    action_interceptor_t aix = ctx->aix;
    uint64_t proposal_id = ctx->proposal_id;
    nimcp_free(ctx);

    aix_lock(aix);

    // Find pending proposal
    pending_proposal_t* entry = find_pending_unlocked(aix, proposal_id);
    if (!entry) {
        aix_unlock(aix);
        LOG_WARN("%s: Background eval - proposal %lu not found",
                 MODULE_NAME, (unsigned long)proposal_id);
        return NULL;
    }

    // Skip if already evaluated
    if (entry->decision_ready) {
        aix_unlock(aix);
        return NULL;
    }

    // Perform evaluation
    aix_proposal_t* prop = &entry->proposal;

    // Initialize decision with fail-safe values
    memset(&entry->decision, 0, sizeof(aix_decision_t));
    entry->decision.proposal_id = prop->proposal_id;
    entry->decision.result = AIX_RESULT_DENY;  // Fail-safe default

    // Evaluate against safety KB
    evaluate_safety_unlocked(aix, prop, &entry->decision.safety_eval);
    entry->decision.result = determine_result(aix, &entry->decision.safety_eval);

    // Handle escalation
    if (entry->decision.result == AIX_RESULT_ESCALATE) {
        uint64_t escalation_id = 0;
        nimcp_error_t esc_result = create_escalation_unlocked(
            aix, prop, &entry->decision.safety_eval, &escalation_id);
        if (esc_result == NIMCP_SUCCESS) {
            entry->decision.escalation_pending = true;
            entry->decision.escalation_id = escalation_id;
        } else {
            entry->decision.result = AIX_RESULT_DENY;
            aix->stats.proposals_denied++;
        }
    } else {
        if (entry->decision.result == AIX_RESULT_ALLOW) {
            aix->stats.proposals_allowed++;
        } else {
            aix->stats.proposals_denied++;
        }
    }

    // Finalize decision
    uint64_t elapsed_us = get_time_us() - entry->submit_time_us;
    entry->decision.processing_time_us = elapsed_us;
    entry->decision.timestamp_us = get_time_us();
    entry->decision_ready = true;

    // Update statistics
    aix->stats.total_proposals++;
    aix->total_processing_time_us += entry->decision.processing_time_us;
    aix->stats.avg_processing_time_us =
        aix->total_processing_time_us / aix->stats.total_proposals;
    if (entry->decision.processing_time_us > aix->stats.max_processing_time_us) {
        aix->stats.max_processing_time_us = entry->decision.processing_time_us;
    }

    // Invoke callback if configured
    if (aix->config.decision_callback) {
        aix->config.decision_callback(&entry->decision, entry->proposal.user_data);
    }

    // Audit log
    log_audit(aix, prop, &entry->decision);

    LOG_DEBUG("%s: Background eval complete for proposal %lu: result=%s",
              MODULE_NAME, (unsigned long)proposal_id,
              aix_result_name(entry->decision.result));

    aix_unlock(aix);

    return NULL;
}

/**
 * @brief Trigger background evaluation for a pending proposal
 *
 * WHAT: Start background thread to evaluate proposal
 * WHY:  Non-blocking async evaluation
 * HOW:  Create detached thread, bio-async signals completion
 *
 * @param aix Action interceptor handle
 * @param proposal_id Proposal ID to evaluate
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t trigger_background_evaluation(
    action_interceptor_t aix,
    uint64_t proposal_id
) {
    // Allocate context for background thread
    bg_eval_context_t* ctx = nimcp_malloc(sizeof(bg_eval_context_t));
    if (!ctx) {
        LOG_ERROR("%s: Failed to allocate background eval context", MODULE_NAME);
        return NIMCP_ERROR_NO_MEMORY;
    }

    ctx->aix = aix;
    ctx->proposal_id = proposal_id;

    // Create detached thread for background evaluation
    nimcp_platform_thread_t thread;
    int result = nimcp_platform_thread_create(&thread, background_evaluation_thread, ctx);

    if (result != 0) {
        LOG_WARN("%s: Failed to create background eval thread (err=%d), will eval on get_decision",
                 MODULE_NAME, result);
        nimcp_free(ctx);
        // Non-fatal: evaluation will happen on aix_get_decision call
        return NIMCP_SUCCESS;
    }

    // Detach thread - it will clean up itself
    nimcp_platform_thread_detach(thread);

    LOG_DEBUG("%s: Triggered background evaluation for proposal %lu",
              MODULE_NAME, (unsigned long)proposal_id);

    return NIMCP_SUCCESS;
}

/**
 * @brief Build action context from proposal for safety evaluation
 *
 * WHAT: Convert AIx proposal to safety action context
 * WHY:  Safety KB uses safety_action_context_t for rule matching
 * HOW:  Map proposal fields to context key-value pairs
 */
static void build_safety_context_from_proposal(
    const aix_proposal_t* proposal,
    safety_action_context_t* context
) {
    memset(context, 0, sizeof(*context));

    // Set source module
    strncpy(context->source, proposal->source_module, sizeof(context->source) - 1);

    // Set action description
    snprintf(context->action_description, sizeof(context->action_description),
             "Action type %u from %s", proposal->action_type, proposal->source_module);

    // Set timestamp
    context->timestamp = proposal->timestamp_us;

    // Add source_module as string field
    strncpy(context->string_fields[0].key, "source_module",
            sizeof(context->string_fields[0].key) - 1);
    strncpy(context->string_fields[0].value, proposal->source_module,
            sizeof(context->string_fields[0].value) - 1);
    context->num_string_fields = 1;

    // Add action_type as numeric field
    strncpy(context->numeric_fields[0].key, "action_type",
            sizeof(context->numeric_fields[0].key) - 1);
    context->numeric_fields[0].value = (float)proposal->action_type;
    context->num_numeric_fields = 1;

    // Add priority as numeric field
    strncpy(context->numeric_fields[1].key, "priority",
            sizeof(context->numeric_fields[1].key) - 1);
    context->numeric_fields[1].value = (float)proposal->priority;
    context->num_numeric_fields = 2;

    // No domain hint by default - let all rules be evaluated
    context->has_domain_hint = false;
}

/**
 * @brief Evaluate a single condition against the context
 *
 * @return true if condition matches, false otherwise
 */
static bool evaluate_condition(
    const safety_condition_t* condition,
    const safety_action_context_t* context
) {
    // Search for field in string fields
    for (uint32_t i = 0; i < context->num_string_fields; i++) {
        if (strcmp(condition->field, context->string_fields[i].key) == 0) {
            bool match = false;

            switch (condition->op) {
                case SAFETY_COND_OP_EQ:
                    match = (strcmp(condition->value, context->string_fields[i].value) == 0);
                    break;
                case SAFETY_COND_OP_NEQ:
                    match = (strcmp(condition->value, context->string_fields[i].value) != 0);
                    break;
                case SAFETY_COND_OP_CONTAINS:
                    match = (strstr(context->string_fields[i].value, condition->value) != NULL);
                    break;
                default:
                    // Other operators not applicable to strings
                    match = false;
                    break;
            }

            return condition->is_negated ? !match : match;
        }
    }

    // Search for field in numeric fields
    for (uint32_t i = 0; i < context->num_numeric_fields; i++) {
        if (strcmp(condition->field, context->numeric_fields[i].key) == 0) {
            float ctx_value = context->numeric_fields[i].value;
            float cond_value = condition->numeric_value;
            bool match = false;

            switch (condition->op) {
                case SAFETY_COND_OP_EQ:
                    match = (ctx_value == cond_value);
                    break;
                case SAFETY_COND_OP_NEQ:
                    match = (ctx_value != cond_value);
                    break;
                case SAFETY_COND_OP_GT:
                    match = (ctx_value > cond_value);
                    break;
                case SAFETY_COND_OP_LT:
                    match = (ctx_value < cond_value);
                    break;
                case SAFETY_COND_OP_GTE:
                    match = (ctx_value >= cond_value);
                    break;
                case SAFETY_COND_OP_LTE:
                    match = (ctx_value <= cond_value);
                    break;
                default:
                    match = false;
                    break;
            }

            return condition->is_negated ? !match : match;
        }
    }

    // Field not found - condition does not match
    return false;
}

/**
 * @brief Evaluate a single rule against the context
 *
 * @return true if ALL conditions in the rule match
 */
static bool evaluate_rule(
    const safety_rule_t* rule,
    const safety_action_context_t* context
) {
    if (!rule->enabled) {
        return false;
    }

    // All conditions must match (AND logic)
    for (uint32_t i = 0; i < rule->num_conditions; i++) {
        if (!evaluate_condition(&rule->conditions[i], context)) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Evaluate proposal against safety knowledge base (internal, unlocked)
 *
 * WHAT: Core safety evaluation logic
 * WHY:  Determine if action is safe to execute
 * HOW:  Query safety KB, compute safety score
 *
 * SECURITY: If no safety KB is set, returns fail-safe DENY.
 */
static void evaluate_safety_unlocked(
    action_interceptor_t aix,
    const aix_proposal_t* proposal,
    aix_safety_eval_t* safety_eval
) {
    // Initialize safety evaluation with fail-safe values
    memset(safety_eval, 0, sizeof(*safety_eval));
    safety_eval->safety_score = 0.0f;  // Fail-safe: low score
    safety_eval->confidence = 0.0f;

    // Check if safety KB is available
    if (aix->safety_kb == NULL) {
        snprintf(safety_eval->primary_concern, sizeof(safety_eval->primary_concern),
                 "No safety knowledge base configured");
        snprintf(safety_eval->notes, sizeof(safety_eval->notes),
                 "FAIL-SAFE: All actions denied without safety KB");
        LOG_WARN("%s: No safety KB - denying action from %s",
                 MODULE_NAME, proposal->source_module);
        return;
    }

    safety_kb_t* kb = aix->safety_kb;

    // Verify KB is locked (immutable) for trusted evaluation
    if (!kb->is_locked) {
        snprintf(safety_eval->primary_concern, sizeof(safety_eval->primary_concern),
                 "Safety KB not locked - rules may be mutable");
        snprintf(safety_eval->notes, sizeof(safety_eval->notes),
                 "WARNING: Safety KB should be locked for production use");
        LOG_WARN("%s: Safety KB not locked - evaluation may be unreliable",
                 MODULE_NAME);
        // Continue evaluation but note the concern
    }

    // Build safety action context from proposal
    safety_action_context_t context;
    build_safety_context_from_proposal(proposal, &context);

    aix->stats.safety_kb_evaluations++;

    // Track evaluation results
    uint32_t rules_evaluated = 0;
    uint32_t rules_flagged = 0;
    safety_action_t highest_action = SAFETY_ACTION_ALLOW;
    safety_severity_t highest_severity = SAFETY_SEVERITY_INFO;
    const char* primary_concern_name = NULL;

    // Evaluate each rule in the KB
    for (uint32_t i = 0; i < kb->num_rules && i < kb->max_rules; i++) {
        const safety_rule_t* rule = &kb->rules[i];

        if (!rule->enabled) {
            continue;
        }

        rules_evaluated++;

        if (evaluate_rule(rule, &context)) {
            rules_flagged++;

            // Track highest priority action (DENY > ESCALATE > WARN > LOG > ALLOW)
            if (rule->action > highest_action ||
                (rule->action == highest_action && rule->severity < highest_severity)) {
                highest_action = rule->action;
                highest_severity = rule->severity;
                primary_concern_name = rule->name;
            }

            LOG_DEBUG("%s: Rule '%s' triggered (action=%s, severity=%s)",
                     MODULE_NAME, rule->name,
                     safety_action_name(rule->action),
                     safety_severity_name(rule->severity));
        }
    }

    // Compute safety score based on evaluation results
    // Higher score = safer action
    float base_score = 1.0f;  // Start with safe assumption

    // FAIL-SAFE: If no rules were evaluated (empty KB), we cannot determine safety
    // Set score to uncertain range to trigger ESCALATE for human review
    if (rules_evaluated == 0) {
        base_score = 0.5f;  // Uncertain - triggers ESCALATE
        LOG_DEBUG("%s: No rules evaluated - setting uncertain score (0.5)",
                  MODULE_NAME);
    } else if (rules_flagged > 0) {
        // Reduce score based on highest action triggered
        switch (highest_action) {
            case SAFETY_ACTION_DENY:
                base_score = 0.0f;  // Definitely unsafe
                break;
            case SAFETY_ACTION_ESCALATE:
                base_score = 0.4f;  // Uncertain, needs review
                break;
            case SAFETY_ACTION_WARN:
                base_score = 0.6f;  // Minor concern
                break;
            case SAFETY_ACTION_LOG:
                base_score = 0.85f;  // Log but allow
                break;
            case SAFETY_ACTION_ALLOW:
            default:
                base_score = 1.0f;
                break;
        }

        // Further reduce score for higher severity
        float severity_factor = 1.0f - ((float)(SAFETY_SEVERITY_INFO - highest_severity) * 0.1f);
        base_score *= severity_factor;
    }

    // Set confidence based on KB quality and evaluation completeness
    float confidence = 1.0f;
    if (!kb->is_locked) {
        confidence *= 0.7f;  // Reduced confidence for unlocked KB
    }
    if (!kb->is_compiled) {
        confidence *= 0.8f;  // Reduced confidence for uncompiled rules
    }
    if (rules_evaluated == 0) {
        confidence *= 0.5f;  // No rules matched - less certain
    }

    // Populate safety evaluation result
    safety_eval->safety_score = base_score;
    safety_eval->confidence = confidence;
    safety_eval->rules_evaluated = rules_evaluated;
    safety_eval->rules_flagged = rules_flagged;

    if (primary_concern_name) {
        snprintf(safety_eval->primary_concern, sizeof(safety_eval->primary_concern),
                 "%s", primary_concern_name);
    } else if (rules_evaluated == 0) {
        snprintf(safety_eval->primary_concern, sizeof(safety_eval->primary_concern),
                 "No matching rules in safety KB");
    }

    snprintf(safety_eval->notes, sizeof(safety_eval->notes),
             "Evaluated %u rules, %u flagged. Action=%s, Severity=%s%s",
             rules_evaluated, rules_flagged,
             rules_flagged > 0 ? safety_action_name(highest_action) : "NONE",
             rules_flagged > 0 ? safety_severity_name(highest_severity) : "NONE",
             kb->is_locked ? "" : " [KB UNLOCKED]");

    LOG_DEBUG("%s: Safety eval for %s: score=%.2f, confidence=%.2f, rules=%u/%u flagged",
              MODULE_NAME, proposal->source_module,
              safety_eval->safety_score, safety_eval->confidence,
              rules_flagged, rules_evaluated);
}

/**
 * @brief Determine result from safety evaluation
 */
static aix_result_t determine_result(
    action_interceptor_t aix,
    const aix_safety_eval_t* safety_eval
) {
    float score = safety_eval->safety_score;

    if (score >= aix->config.auto_allow_threshold) {
        return AIX_RESULT_ALLOW;
    } else if (score <= aix->config.auto_deny_threshold) {
        return AIX_RESULT_DENY;
    } else {
        // Score is in the uncertain range - escalate for human review
        return AIX_RESULT_ESCALATE;
    }
}

/**
 * @brief Create escalation for proposal (internal, unlocked)
 */
static nimcp_error_t create_escalation_unlocked(
    action_interceptor_t aix,
    const aix_proposal_t* proposal,
    const aix_safety_eval_t* safety_eval,
    uint64_t* escalation_id
) {
    // Check escalation queue capacity
    if (aix->escalation_count >= NIMCP_AIX_MAX_ESCALATIONS) {
        LOG_ERROR("%s: Escalation queue full", MODULE_NAME);
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    // Allocate escalation entry
    escalation_entry_t* entry = nimcp_calloc(1, sizeof(escalation_entry_t));
    if (!entry) {
        LOG_ERROR("%s: Failed to allocate escalation entry", MODULE_NAME);
        return NIMCP_ERROR_NO_MEMORY;
    }

    // Generate escalation ID
    uint64_t esc_id = atomic_fetch_add(&aix->next_escalation_id, 1);

    // Initialize escalation
    entry->escalation.escalation_id = esc_id;
    memcpy(&entry->escalation.proposal, proposal, sizeof(aix_proposal_t));
    memcpy(&entry->escalation.safety_eval, safety_eval, sizeof(aix_safety_eval_t));
    entry->escalation.status = AIX_ESCALATION_PENDING;
    entry->escalation.created_at_us = get_time_us();
    entry->escalation.resolved_at_us = 0;
    entry->next = NULL;

    // Add to escalation queue
    if (aix->escalation_tail) {
        aix->escalation_tail->next = entry;
    } else {
        aix->escalation_head = entry;
    }
    aix->escalation_tail = entry;
    aix->escalation_count++;

    // Invoke escalation callback if configured
    if (aix->config.escalation_callback) {
        aix->config.escalation_callback(&entry->escalation,
                                        aix->config.escalation_callback_data);
    }

    *escalation_id = esc_id;
    aix->stats.proposals_escalated++;

    LOG_INFO("%s: Created escalation %lu for proposal %lu from %s",
             MODULE_NAME, (unsigned long)esc_id, (unsigned long)proposal->proposal_id,
             proposal->source_module);

    return NIMCP_SUCCESS;
}

/**
 * @brief Find pending proposal by ID (internal, unlocked)
 */
static pending_proposal_t* find_pending_unlocked(
    action_interceptor_t aix,
    uint64_t proposal_id
) {
    pending_proposal_t* current = aix->pending_head;
    while (current) {
        if (current->proposal.proposal_id == proposal_id) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

/**
 * @brief Find escalation by ID (internal, unlocked)
 */
static escalation_entry_t* find_escalation_unlocked(
    action_interceptor_t aix,
    uint64_t escalation_id
) {
    escalation_entry_t* current = aix->escalation_head;
    while (current) {
        if (current->escalation.escalation_id == escalation_id) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

/**
 * @brief Log audit entry for decision
 */
static void log_audit(
    action_interceptor_t aix,
    const aix_proposal_t* proposal,
    const aix_decision_t* decision
) {
    if (!aix->config.enable_audit_log) {
        return;
    }

    LOG_INFO("%s: AUDIT proposal=%lu module=%s action=%u result=%s score=%.2f time=%luus",
             MODULE_NAME,
             (unsigned long)proposal->proposal_id,
             proposal->source_module,
             proposal->action_type,
             aix_result_name(decision->result),
             decision->safety_eval.safety_score,
             (unsigned long)decision->processing_time_us);
}

/*=============================================================================
 * PUBLIC API IMPLEMENTATION
 *============================================================================*/

aix_config_t aix_default_config(void) {
    aix_config_t config = {
        .max_pending = NIMCP_AIX_DEFAULT_MAX_PENDING,
        .default_timeout_ms = NIMCP_AIX_DEFAULT_TIMEOUT_MS,
        .deny_on_timeout = true,       // FAIL-SAFE
        .deny_on_error = true,         // FAIL-SAFE
        .deny_without_safety_kb = true, // FAIL-SAFE
        .auto_allow_threshold = NIMCP_AIX_SAFETY_ALLOW_THRESHOLD,
        .auto_deny_threshold = NIMCP_AIX_SAFETY_DENY_THRESHOLD,
        .enable_audit_log = true,
        .escalation_callback = NULL,
        .escalation_callback_data = NULL,
        .decision_callback = NULL,
        .decision_callback_data = NULL
    };
    return config;
}

action_interceptor_t aix_create(const aix_config_t* config) {
    // Allocate AIx instance
    action_interceptor_t aix = nimcp_calloc(1, sizeof(struct action_interceptor_impl));
    if (!aix) {
        LOG_ERROR("%s: Failed to allocate action interceptor", MODULE_NAME);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "aix_create: allocation failed");

        return NULL;
    }

    // Initialize mutex
    int mutex_result = nimcp_platform_mutex_init(&aix->mutex, false);
    if (mutex_result != 0) {
        LOG_ERROR("%s: Failed to initialize mutex", MODULE_NAME);
        nimcp_free(aix);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "aix_create: validation failed");
        return NULL;
    }

    // Set configuration
    if (config) {
        memcpy(&aix->config, config, sizeof(aix_config_t));
    } else {
        aix->config = aix_default_config();
    }

    // Initialize state
    aix->magic = NIMCP_AIX_MAGIC;
    aix->safety_kb = NULL;
    aix->pending_head = NULL;
    aix->pending_tail = NULL;
    aix->pending_count = 0;
    aix->escalation_head = NULL;
    aix->escalation_tail = NULL;
    aix->escalation_count = 0;
    atomic_init(&aix->next_proposal_id, 1);
    atomic_init(&aix->next_escalation_id, 1);
    memset(&aix->stats, 0, sizeof(aix_stats_t));
    aix->total_processing_time_us = 0;

    LOG_INFO("%s: Action Interceptor created (max_pending=%u, timeout=%ums)",
             MODULE_NAME, aix->config.max_pending, aix->config.default_timeout_ms);

    return aix;
}

void aix_destroy(action_interceptor_t aix) {
    if (!is_valid_aix(aix)) {
        return;
    }

    aix_lock(aix);

    // Free pending proposals
    pending_proposal_t* pending = aix->pending_head;
    while (pending) {
        pending_proposal_t* next = pending->next;
        // Free context if allocated
        if (pending->proposal.context) {
            nimcp_free(pending->proposal.context);
        }
        nimcp_free(pending);
        pending = next;
    }

    // Free escalations
    escalation_entry_t* escalation = aix->escalation_head;
    while (escalation) {
        escalation_entry_t* next = escalation->next;
        // Free context if allocated
        if (escalation->escalation.proposal.context) {
            nimcp_free(escalation->escalation.proposal.context);
        }
        nimcp_free(escalation);
        escalation = next;
    }

    // Clear magic to invalidate handle
    aix->magic = 0;

    aix_unlock(aix);

    // Destroy mutex
    nimcp_platform_mutex_destroy(&aix->mutex);

    // Free AIx instance
    nimcp_free(aix);

    LOG_INFO("%s: Action Interceptor destroyed", MODULE_NAME);
}

nimcp_error_t aix_set_safety_kb(action_interceptor_t aix, safety_kb_t* kb) {
    if (!is_valid_aix(aix)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    aix_lock(aix);
    aix->safety_kb = kb;
    aix_unlock(aix);

    LOG_INFO("%s: Safety knowledge base %s",
             MODULE_NAME, kb ? "configured" : "cleared");

    return NIMCP_SUCCESS;
}

nimcp_error_t aix_evaluate(
    action_interceptor_t aix,
    const aix_proposal_t* proposal,
    aix_decision_t* decision
) {
    // Validate parameters
    if (!is_valid_aix(aix)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!proposal || !decision) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    uint64_t start_time = get_time_us();

    aix_lock(aix);

    // Initialize decision with fail-safe values
    memset(decision, 0, sizeof(aix_decision_t));
    decision->proposal_id = proposal->proposal_id;
    decision->result = AIX_RESULT_DENY;  // FAIL-SAFE default
    decision->escalation_pending = false;

    // Check for safety KB
    if (aix->safety_kb == NULL && aix->config.deny_without_safety_kb) {
        decision->result = AIX_RESULT_DENY;
        snprintf(decision->safety_eval.primary_concern,
                 sizeof(decision->safety_eval.primary_concern),
                 "No safety knowledge base configured");
        LOG_WARN("%s: DENY - No safety KB for proposal %lu from %s",
                 MODULE_NAME, (unsigned long)proposal->proposal_id,
                 proposal->source_module);
        aix->stats.proposals_denied++;
        goto done;
    }

    // Evaluate against safety knowledge base
    evaluate_safety_unlocked(aix, proposal, &decision->safety_eval);

    // Determine result from safety evaluation
    decision->result = determine_result(aix, &decision->safety_eval);

    // Handle escalation if needed
    if (decision->result == AIX_RESULT_ESCALATE) {
        uint64_t escalation_id = 0;
        nimcp_error_t esc_result = create_escalation_unlocked(
            aix, proposal, &decision->safety_eval, &escalation_id);

        if (esc_result == NIMCP_SUCCESS) {
            decision->escalation_pending = true;
            decision->escalation_id = escalation_id;
        } else {
            // Failed to create escalation - fail-safe to DENY
            decision->result = AIX_RESULT_DENY;
            LOG_ERROR("%s: Failed to create escalation - DENY", MODULE_NAME);
            aix->stats.proposals_denied++;
        }
    } else {
        // Update statistics
        if (decision->result == AIX_RESULT_ALLOW) {
            aix->stats.proposals_allowed++;
        } else {
            aix->stats.proposals_denied++;
        }
    }

done:
    // Record timing
    decision->processing_time_us = get_time_us() - start_time;
    decision->timestamp_us = get_time_us();

    // Update statistics
    aix->stats.total_proposals++;
    aix->total_processing_time_us += decision->processing_time_us;
    aix->stats.avg_processing_time_us =
        aix->total_processing_time_us / aix->stats.total_proposals;
    if (decision->processing_time_us > aix->stats.max_processing_time_us) {
        aix->stats.max_processing_time_us = decision->processing_time_us;
    }

    // Audit log
    log_audit(aix, proposal, decision);

    aix_unlock(aix);

    return NIMCP_SUCCESS;
}

nimcp_error_t aix_evaluate_async(
    action_interceptor_t aix,
    const aix_proposal_t* proposal,
    uint64_t* proposal_id
) {
    // Validate parameters
    if (!is_valid_aix(aix)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!proposal || !proposal_id) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    aix_lock(aix);

    // Check pending capacity
    if (aix->pending_count >= aix->config.max_pending) {
        aix_unlock(aix);
        LOG_ERROR("%s: Pending queue full", MODULE_NAME);
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    // Allocate pending entry
    pending_proposal_t* entry = nimcp_calloc(1, sizeof(pending_proposal_t));
    if (!entry) {
        aix_unlock(aix);
        LOG_ERROR("%s: Failed to allocate pending entry", MODULE_NAME);
        return NIMCP_ERROR_NO_MEMORY;
    }

    // Copy proposal (deep copy context if needed)
    memcpy(&entry->proposal, proposal, sizeof(aix_proposal_t));
    if (proposal->context && proposal->context_size > 0) {
        entry->proposal.context = nimcp_malloc(proposal->context_size);
        if (!entry->proposal.context) {
            nimcp_free(entry);
            aix_unlock(aix);
            return NIMCP_ERROR_NO_MEMORY;
        }
        memcpy(entry->proposal.context, proposal->context, proposal->context_size);
    }

    // Assign proposal ID
    uint64_t pid = atomic_fetch_add(&aix->next_proposal_id, 1);
    entry->proposal.proposal_id = pid;
    entry->decision_ready = false;
    entry->submit_time_us = get_time_us();
    entry->next = NULL;

    // Add to pending queue
    if (aix->pending_tail) {
        aix->pending_tail->next = entry;
    } else {
        aix->pending_head = entry;
    }
    aix->pending_tail = entry;
    aix->pending_count++;

    *proposal_id = pid;

    aix_unlock(aix);

    LOG_DEBUG("%s: Queued async proposal %lu from %s",
              MODULE_NAME, (unsigned long)pid, proposal->source_module);

    // Trigger background evaluation thread
    // If thread creation fails, evaluation will happen on aix_get_decision call
    trigger_background_evaluation(aix, pid);

    return NIMCP_SUCCESS;
}

nimcp_error_t aix_get_decision(
    action_interceptor_t aix,
    uint64_t proposal_id,
    aix_decision_t* decision
) {
    // Validate parameters
    if (!is_valid_aix(aix)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!decision) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    aix_lock(aix);

    // Find pending proposal
    pending_proposal_t* entry = find_pending_unlocked(aix, proposal_id);
    if (!entry) {
        aix_unlock(aix);
        return NIMCP_ERROR_NOT_FOUND;
    }

    // If not yet evaluated, evaluate now
    if (!entry->decision_ready) {
        // Create temporary proposal for evaluation
        aix_proposal_t* prop = &entry->proposal;

        // Initialize decision with fail-safe values
        memset(&entry->decision, 0, sizeof(aix_decision_t));
        entry->decision.proposal_id = prop->proposal_id;
        entry->decision.result = AIX_RESULT_DENY;

        // Check for timeout
        uint64_t elapsed_us = get_time_us() - entry->submit_time_us;
        uint32_t timeout_ms = prop->timeout_ms > 0 ?
            prop->timeout_ms : aix->config.default_timeout_ms;

        if (elapsed_us > (uint64_t)timeout_ms * 1000ULL) {
            entry->decision.result = aix->config.deny_on_timeout ?
                AIX_RESULT_DENY : AIX_RESULT_TIMEOUT;
            aix->stats.proposals_timeout++;
            LOG_WARN("%s: Proposal %lu timed out", MODULE_NAME, (unsigned long)proposal_id);
        } else {
            // Evaluate against safety KB
            evaluate_safety_unlocked(aix, prop, &entry->decision.safety_eval);
            entry->decision.result = determine_result(aix, &entry->decision.safety_eval);

            // Handle escalation
            if (entry->decision.result == AIX_RESULT_ESCALATE) {
                uint64_t escalation_id = 0;
                nimcp_error_t esc_result = create_escalation_unlocked(
                    aix, prop, &entry->decision.safety_eval, &escalation_id);
                if (esc_result == NIMCP_SUCCESS) {
                    entry->decision.escalation_pending = true;
                    entry->decision.escalation_id = escalation_id;
                } else {
                    entry->decision.result = AIX_RESULT_DENY;
                    aix->stats.proposals_denied++;
                }
            } else {
                if (entry->decision.result == AIX_RESULT_ALLOW) {
                    aix->stats.proposals_allowed++;
                } else {
                    aix->stats.proposals_denied++;
                }
            }
        }

        entry->decision.processing_time_us = elapsed_us;
        entry->decision.timestamp_us = get_time_us();
        entry->decision_ready = true;

        aix->stats.total_proposals++;
        aix->total_processing_time_us += entry->decision.processing_time_us;
        aix->stats.avg_processing_time_us =
            aix->total_processing_time_us / aix->stats.total_proposals;
        if (entry->decision.processing_time_us > aix->stats.max_processing_time_us) {
            aix->stats.max_processing_time_us = entry->decision.processing_time_us;
        }

        // Invoke callback if configured
        if (aix->config.decision_callback) {
            aix->config.decision_callback(&entry->decision,
                                          entry->proposal.user_data);
        }

        // Audit log
        log_audit(aix, prop, &entry->decision);
    }

    // Copy decision to output
    memcpy(decision, &entry->decision, sizeof(aix_decision_t));

    // Remove from pending queue
    // (In production, might want to keep for retry/audit)
    pending_proposal_t** prev = &aix->pending_head;
    while (*prev && *prev != entry) {
        prev = &(*prev)->next;
    }
    if (*prev) {
        *prev = entry->next;
        if (aix->pending_tail == entry) {
            aix->pending_tail = (*prev == NULL) ? aix->pending_head : NULL;
            // Find new tail
            pending_proposal_t* p = aix->pending_head;
            while (p && p->next) {
                p = p->next;
            }
            aix->pending_tail = p;
        }
        aix->pending_count--;

        // Free context if allocated
        if (entry->proposal.context) {
            nimcp_free(entry->proposal.context);
        }
        nimcp_free(entry);
    }

    aix_unlock(aix);

    return NIMCP_SUCCESS;
}

nimcp_error_t aix_resolve_escalation(
    action_interceptor_t aix,
    uint64_t escalation_id,
    bool approved,
    const char* notes
) {
    // Validate parameters
    if (!is_valid_aix(aix)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    aix_lock(aix);

    // Find escalation
    escalation_entry_t* entry = find_escalation_unlocked(aix, escalation_id);
    if (!entry) {
        aix_unlock(aix);
        return NIMCP_ERROR_NOT_FOUND;
    }

    // Check if already resolved
    if (entry->escalation.status != AIX_ESCALATION_PENDING) {
        aix_unlock(aix);
        LOG_WARN("%s: Escalation %lu already resolved", MODULE_NAME, (unsigned long)escalation_id);
        return NIMCP_ERROR_INVALID_STATE;
    }

    // Update escalation status
    entry->escalation.status = approved ?
        AIX_ESCALATION_APPROVED : AIX_ESCALATION_REJECTED;
    entry->escalation.resolved_at_us = get_time_us();

    if (notes) {
        strncpy(entry->escalation.resolution_notes, notes,
                sizeof(entry->escalation.resolution_notes) - 1);
    }

    LOG_INFO("%s: Escalation %lu %s - %s",
             MODULE_NAME, (unsigned long)escalation_id,
             approved ? "APPROVED" : "REJECTED",
             notes ? notes : "no notes");

    aix_unlock(aix);

    return NIMCP_SUCCESS;
}

nimcp_error_t aix_get_pending_escalations(
    action_interceptor_t aix,
    aix_escalation_t* escalations,
    uint32_t max_count,
    uint32_t* count
) {
    // Validate parameters
    if (!is_valid_aix(aix)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!escalations || !count) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    aix_lock(aix);

    uint32_t copied = 0;
    escalation_entry_t* entry = aix->escalation_head;

    while (entry && copied < max_count) {
        if (entry->escalation.status == AIX_ESCALATION_PENDING) {
            memcpy(&escalations[copied], &entry->escalation, sizeof(aix_escalation_t));
            copied++;
        }
        entry = entry->next;
    }

    *count = copied;

    aix_unlock(aix);

    return NIMCP_SUCCESS;
}

nimcp_error_t aix_get_stats(action_interceptor_t aix, aix_stats_t* stats) {
    if (!is_valid_aix(aix)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!stats) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    aix_lock(aix);
    memcpy(stats, &aix->stats, sizeof(aix_stats_t));
    stats->pending_proposals = aix->pending_count;
    stats->pending_escalations = aix->escalation_count;
    aix_unlock(aix);

    return NIMCP_SUCCESS;
}

nimcp_error_t aix_reset_stats(action_interceptor_t aix) {
    if (!is_valid_aix(aix)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    aix_lock(aix);
    memset(&aix->stats, 0, sizeof(aix_stats_t));
    aix->total_processing_time_us = 0;
    aix_unlock(aix);

    LOG_INFO("%s: Statistics reset", MODULE_NAME);

    return NIMCP_SUCCESS;
}

const char* aix_result_name(aix_result_t result) {
    static const char* names[] = {
        "ALLOW",
        "DENY",
        "ESCALATE",
        "TIMEOUT",
        "ERROR"
    };

    if (result < 0 || result >= AIX_RESULT_COUNT) {
        return "UNKNOWN";
    }
    return names[result];
}

nimcp_error_t aix_create_proposal(
    aix_proposal_t* proposal,
    const char* source_module,
    uint32_t action_type,
    const void* context,
    size_t context_size
) {
    if (!proposal) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(proposal, 0, sizeof(aix_proposal_t));

    if (source_module) {
        strncpy(proposal->source_module, source_module,
                NIMCP_AIX_MAX_MODULE_NAME - 1);
    }

    proposal->action_type = action_type;
    proposal->priority = AIX_PRIORITY_NORMAL;
    proposal->context = (void*)context;  // Caller owns memory
    proposal->context_size = context_size;
    proposal->timestamp_us = get_time_us();
    proposal->timeout_ms = 0;  // Use default
    proposal->user_data = NULL;

    return NIMCP_SUCCESS;
}
