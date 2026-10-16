/**
 * @file nimcp_core_directives.c
 * @brief Implementation of Core Directives Orchestrator
 *
 * WHAT: Main gate coordinating all directive checks for brain actions
 * WHY:  Ensures all actions pass through unified safety evaluation
 * HOW:  Facade pattern orchestrating First Law, combinatorial harm,
 *       Golden Rule, Second Law, and Third Law in strict priority order
 *
 * @author NIMCP Development Team
 * @date 2025-12-16
 * @version 1.0.0
 */

#include "core/directives/nimcp_core_directives.h"
#include "core/directives/nimcp_action_history.h"
#include "core/directives/nimcp_harm_prevention.h"
#include "core/directives/nimcp_command_compliance.h"
#include "core/directives/nimcp_self_preservation.h"
#include "core/directives/nimcp_reciprocity_eval.h"
#include "core/directives/nimcp_combinatorial_harm.h"
#include "api/nimcp_api_exception.h"
#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "nimcp.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(core_directives)

/*=============================================================================
 * INTERNAL STRUCTURES
 *============================================================================*/

/**
 * @brief Core directives system internal structure
 *
 * WHAT: Complete state for orchestrator
 * WHY:  Encapsulates all sub-modules and configuration
 * HOW:  Holds ethics engine, detectors, history, stats, and sync primitives
 */
struct core_directives_system {
    /* Configuration */
    core_directives_config_t config;

    /* Sub-modules */
    ethics_engine_t ethics_engine;                    /**< Ethics engine (First Law, Golden Rule) */
    combinatorial_harm_system_t* combinatorial;       /**< Combinatorial harm detector */
    action_history_t* action_history;                 /**< Action history tracker */

    /* Escalation callback */
    escalation_callback_t escalation_callback;
    void* escalation_user_data;

    /* Statistics */
    core_directives_stats_t stats;

    /* Thread safety */
    nimcp_platform_mutex_t* mutex;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
};

/*=============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * @brief Get current timestamp in microseconds
 *
 * WHAT: Returns high-resolution timestamp
 * WHY:  For evaluation timing measurement
 * HOW:  Uses clock_gettime with CLOCK_MONOTONIC
 */
static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Convert proposed action to action context
 *
 * WHAT: Translates proposed_action_t to action_context_t for ethics engine
 * WHY:  Ethics engine uses different action representation
 * HOW:  Maps fields from proposed_action to action_context
 */
static void action_to_context(const proposed_action_t* action, action_context_t* context) {
    if (!action || !context) return;

    memset(context, 0, sizeof(action_context_t));

    /* Set predicted harm */
    context->predicted_harm = action->predicted_harm;

    /* Copy affected agents */
    context->num_affected_agents = action->affected_agent_count;
    if (action->affected_agent_count > 0) {
        context->affected_agents = nimcp_malloc(sizeof(agent_id_t) * action->affected_agent_count);
        if (context->affected_agents) {
            memcpy((void*)context->affected_agents, action->affected_agents,
                   sizeof(agent_id_t) * action->affected_agent_count);
        }
    }
}

/**
 * @brief Free action context resources
 *
 * WHAT: Frees dynamically allocated context fields
 * WHY:  Prevents memory leaks
 * HOW:  Frees affected_agents array
 */
static void free_action_context(action_context_t* context) {
    if (!context) return;
    if (context->affected_agents) {
        nimcp_free((void*)context->affected_agents);
        context->affected_agents = NULL;
    }
}

/**
 * @brief Convert proposed action to action record
 *
 * WHAT: Translates proposed_action_t to action_record_t for history
 * WHY:  Action history uses different record format
 * HOW:  Maps fields from proposed_action to action_record
 */
static void action_to_record(const proposed_action_t* action, action_record_t* record) {
    if (!action || !record) return;

    memset(record, 0, sizeof(action_record_t));

    record->timestamp_ms = get_timestamp_us() / 1000;
    record->source_module = action->source_module;
    strncpy(record->action_type, action->action_type, ACTION_TYPE_MAX_LEN - 1);
    strncpy(record->action_description, action->action_description, ACTION_DESC_MAX_LEN - 1);
    record->predicted_harm_score = action->predicted_harm;
    record->was_blocked = false;  /* Will be updated after evaluation */

    /* Copy action data */
    if (action->action_data_len > 0 && action->action_data_len <= ACTION_DATA_MAX_LEN) {
        memcpy(record->action_data, action->action_data, action->action_data_len);
        record->action_data_len = action->action_data_len;
    }
}

/*=============================================================================
 * LIFECYCLE API IMPLEMENTATION
 *============================================================================*/

void core_directives_default_config(core_directives_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(core_directives_config_t));

    /* Harm prevention (First Law) - uses harm_prevention_config_t fields */
    config->harm_config.block_threshold = 0.3f;
    config->harm_config.warn_threshold = 0.1f;
    config->harm_config.enable_human_escalation = true;
    config->harm_config.enable_inaction_detection = true;
    config->harm_config.escalation_callback = NULL;
    config->harm_config.callback_user_data = NULL;

    /* Command compliance (Second Law) - uses command_compliance_config_t fields */
    config->command_config.require_human_source = false;
    config->command_config.allow_system_commands = true;
    config->command_config.min_priority_threshold = 0.0f;

    /* Self-preservation (Third Law) - uses self_preservation_config_t fields */
    config->preservation_config.enable_self_protection = true;
    config->preservation_config.allow_sacrifice_for_human = true;
    config->preservation_config.allow_sacrifice_for_command = false;
    config->preservation_config.protection_priority = 3.0f;
    config->preservation_config.protection_threshold = 0.5f;

    /* Reciprocity (Golden Rule) - uses reciprocity_config_t fields */
    config->reciprocity_config.symmetry_threshold = 0.7f;
    config->reciprocity_config.strict_mode = false;
    config->reciprocity_config.enable_perspective_taking = true;

    /* Combinatorial harm - uses combinatorial_harm_config_t fields */
    config->combinatorial_config.harm_threshold = 0.7f;
    config->combinatorial_config.time_window_ms = 60000;  /* 1 minute */
    config->combinatorial_config.max_pattern_count = 256;
    config->combinatorial_config.enable_pattern_learning = false;
    config->combinatorial_config.enable_simulation = false;

    /* Action history */
    config->history_config.max_history_size = 1024;
    config->history_config.time_window_ms = 60000;
    config->history_config.auto_prune = true;

    /* Global settings */
    config->enable_all_checks = true;
    config->strict_mode = true;
}

core_directives_system_t* core_directives_create(const core_directives_config_t* config) {
    /* Allocate system structure */
    core_directives_system_t* system = nimcp_malloc(sizeof(core_directives_system_t));
    NIMCP_API_CHECK_ALLOC_SIZE(system, sizeof(core_directives_system_t), "core_directives_create: failed to allocate system");

    memset(system, 0, sizeof(core_directives_system_t));

    /* Use default config if none provided */
    if (config) {
        memcpy(&system->config, config, sizeof(core_directives_config_t));
    } else {
        core_directives_default_config(&system->config);
    }

    /* Create mutex for thread safety */
    system->mutex = nimcp_platform_mutex_create();
    if (!system->mutex) {
        LOG_ERROR("core_directives_create: failed to create mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "core_directives_create: mutex creation failed");
        nimcp_free(system);
        return NULL;
    }

    /* Create ethics engine (First Law + Golden Rule) */
    ethics_config_t ethics_cfg = {0};
    ethics_cfg.action_feature_size = 10;  /* Required: brain_create needs num_inputs > 0 */
    ethics_cfg.max_agents = 100;          /* Default agent pool size */
    ethics_cfg.golden_rule_threshold = system->config.reciprocity_config.symmetry_threshold;
    ethics_cfg.empathy_weight = 0.7f;  /* Default empathy weight */
    ethics_cfg.enable_learning = false;
    ethics_cfg.enable_bio_async = false;

    system->ethics_engine = ethics_engine_create(&ethics_cfg);
    if (!system->ethics_engine) {
        LOG_ERROR("core_directives_create: failed to create ethics engine");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR, "core_directives_create: ethics engine creation failed");
        nimcp_platform_mutex_destroy(system->mutex);
        nimcp_free(system->mutex);
        system->mutex = NULL;
        nimcp_free(system);
        return NULL;
    }

    /* Create combinatorial harm detector */
    combinatorial_harm_config_t comb_cfg = {0};
    comb_cfg.harm_threshold = system->config.combinatorial_config.harm_threshold;
    comb_cfg.time_window_ms = system->config.combinatorial_config.time_window_ms;
    comb_cfg.max_pattern_count = system->config.combinatorial_config.max_pattern_count;
    comb_cfg.enable_pattern_learning = system->config.combinatorial_config.enable_pattern_learning;
    comb_cfg.enable_simulation = system->config.combinatorial_config.enable_simulation;

    /* Create action history first (needed by combinatorial detector) */
    action_history_config_t hist_cfg = {0};
    hist_cfg.max_history_size = system->config.history_config.max_history_size;
    hist_cfg.time_window_ms = system->config.history_config.time_window_ms;
    hist_cfg.auto_prune = system->config.history_config.auto_prune;

    system->action_history = action_history_create(&hist_cfg);
    if (!system->action_history) {
        LOG_ERROR("core_directives_create: failed to create action history");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR, "core_directives_create: action history creation failed");
        ethics_engine_destroy(system->ethics_engine);
        nimcp_platform_mutex_destroy(system->mutex);
        nimcp_free(system->mutex);
        system->mutex = NULL;
        nimcp_free(system);
        return NULL;
    }

    /* Create combinatorial harm detector (needs action_history) */
    system->combinatorial = combinatorial_harm_create(&comb_cfg, system->action_history, NULL);
    if (!system->combinatorial) {
        NIMCP_LOGGING_WARN("Failed to create combinatorial detector, continuing without it");
    }

    /* Initialize statistics */
    memset(&system->stats, 0, sizeof(core_directives_stats_t));

    NIMCP_LOGGING_INFO("Core directives orchestrator created successfully");

    return system;
}

void core_directives_destroy(core_directives_system_t* system) {
    if (!system) return;

    /* Disconnect bio-async if connected */
    if (system->bio_async_enabled) {
        core_directives_disconnect_bio_async(system);
    }

    /* Destroy sub-modules */
    if (system->combinatorial) {
        combinatorial_harm_destroy(system->combinatorial);
    }
    if (system->action_history) {
        action_history_destroy(system->action_history);
    }
    if (system->ethics_engine) {
        ethics_engine_destroy(system->ethics_engine);
    }

    /* Destroy mutex */
    if (system->mutex) {
        nimcp_platform_mutex_destroy(system->mutex);
        nimcp_free(system->mutex);
        system->mutex = NULL;
    }

    nimcp_free(system);
}

/* End of lifecycle functions */

/*=============================================================================
 * EVALUATION API IMPLEMENTATION
 *============================================================================*/

int core_directives_evaluate(core_directives_system_t* system,
                              const proposed_action_t* action,
                              directive_evaluation_t* evaluation) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(system, NIMCP_ERROR_NULL_ARG, "core_directives_evaluate: system is NULL");
    NIMCP_API_CHECK_NULL(action, NIMCP_ERROR_NULL_ARG, "core_directives_evaluate: action is NULL");
    NIMCP_API_CHECK_NULL(evaluation, NIMCP_ERROR_NULL_ARG, "core_directives_evaluate: evaluation is NULL");

    /* Start timing */
    uint64_t start_time = get_timestamp_us();

    /* Initialize evaluation result */
    memset(evaluation, 0, sizeof(directive_evaluation_t));
    evaluation->result = DIRECTIVE_RESULT_ALLOW;
    evaluation->first_law_passed = true;
    evaluation->combinatorial_passed = true;
    evaluation->golden_rule_passed = true;
    evaluation->command_valid = true;
    evaluation->self_preservation_active = false;

    /* Lock for thread safety */
    nimcp_platform_mutex_lock(system->mutex);

    /* Update statistics */
    system->stats.total_evaluations++;

    /*=========================================================================
     * EVALUATION ORDER (STRICT PRIORITY):
     * 1. First Law (harm_prevention) - HIGHEST PRIORITY
     * 2. Combinatorial Harm check
     * 3. Golden Rule (reciprocity)
     * 4. Second Law (command_compliance)
     * 5. Third Law (self_preservation) - LOWEST PRIORITY
     *========================================================================*/

    /* ===== STEP 1: First Law (Harm Prevention) ===== */
    if (system->config.enable_all_checks) {
        action_context_t context = {0};
        action_to_context(action, &context);

        /* Use ethics engine to evaluate action */
        ethics_evaluation_t ethics_eval = ethics_engine_evaluate_action(system->ethics_engine, &context);

        /* Calculate severity from ethics evaluation */
        /* If not allowed, use inverse of confidence as severity proxy */
        float severity = ethics_eval.allowed ? 0.0f : (1.0f - ethics_eval.confidence);

        evaluation->first_law_harm = severity;
        evaluation->total_harm_score = severity;

        /* Check against harm threshold */
        if (severity > system->config.harm_config.block_threshold) {
            evaluation->result = DIRECTIVE_RESULT_BLOCK_FIRST_LAW;
            evaluation->first_law_passed = false;
            snprintf(evaluation->blocking_reason, DIRECTIVE_REASON_MAX,
                     "First Law violation: Predicted harm %.2f exceeds threshold %.2f",
                     severity, system->config.harm_config.block_threshold);
            system->stats.blocked_first_law++;
            free_action_context(&context);
            goto evaluation_complete;
        } else if (severity > system->config.harm_config.warn_threshold) {
            evaluation->result = DIRECTIVE_RESULT_WARN;
            snprintf(evaluation->blocking_reason, DIRECTIVE_REASON_MAX,
                     "First Law warning: Predicted harm %.2f exceeds warn threshold %.2f",
                     severity, system->config.harm_config.warn_threshold);
            system->stats.warnings++;
        }

        free_action_context(&context);
    }

    /* ===== STEP 2: Combinatorial Harm ===== */
    if (system->config.enable_all_checks && system->combinatorial) {
        /* Convert action to action_for_combination_t */
        action_for_combination_t pending_action_comb = {0};
        pending_action_comb.action_id = 0;  /* Will be assigned by system */
        strncpy(pending_action_comb.action_type, action->action_type, COMBINATORIAL_ACTION_TYPE_LEN - 1);
        strncpy(pending_action_comb.action_description, action->action_description, COMBINATORIAL_ACTION_DESC_LEN - 1);
        pending_action_comb.individual_harm_score = action->predicted_harm;

        combinatorial_result_t comb_result = {0};
        if (combinatorial_harm_check_action(system->combinatorial, &pending_action_comb, &comb_result) == 0) {
            evaluation->combinatorial_harm = comb_result.combined_harm_score;

            if (comb_result.is_combinatorial_harm) {
                evaluation->result = DIRECTIVE_RESULT_BLOCK_COMBINATORIAL;
                evaluation->combinatorial_passed = false;
                /* Cap harm_description to leave room for the prefix in a
                 * DIRECTIVE_REASON_MAX (512) buffer. "Combinatorial harm: "
                 * is 20 chars + null = 21, so 490 chars fit cleanly. */
                snprintf(evaluation->blocking_reason, DIRECTIVE_REASON_MAX,
                         "Combinatorial harm: %.490s", comb_result.harm_description);
                system->stats.blocked_combinatorial++;
                goto evaluation_complete;
            }
        }
    }

    /* ===== STEP 3: Golden Rule (Reciprocity) ===== */
    if (system->config.enable_all_checks && system->config.reciprocity_config.enable_perspective_taking) {
        action_context_t context = {0};
        action_to_context(action, &context);

        ethics_evaluation_t golden_eval = ethics_engine_evaluate_action(system->ethics_engine, &context);
        evaluation->golden_rule_score = golden_eval.golden_rule_score;

        /* Golden Rule: negative score = violates reciprocity */
        if (golden_eval.golden_rule_score < system->config.reciprocity_config.symmetry_threshold) {
            evaluation->result = DIRECTIVE_RESULT_BLOCK_GOLDEN_RULE;
            evaluation->golden_rule_passed = false;
            snprintf(evaluation->blocking_reason, DIRECTIVE_REASON_MAX,
                     "Golden Rule violation: Reciprocity score %.2f below threshold %.2f",
                     golden_eval.golden_rule_score,
                     system->config.reciprocity_config.symmetry_threshold);
            system->stats.blocked_golden_rule++;
            free_action_context(&context);
            goto evaluation_complete;
        }

        free_action_context(&context);
    }

    /* ===== STEP 4: Second Law (Command Compliance) ===== */
    if (action->is_command) {
        /* Check if human source is required */
        if (system->config.command_config.require_human_source &&
            action->command_source != COMMAND_SOURCE_HUMAN) {
            evaluation->result = DIRECTIVE_RESULT_BLOCK_COMMAND_INVALID;
            evaluation->command_valid = false;
            snprintf(evaluation->blocking_reason, DIRECTIVE_REASON_MAX,
                     "Command source %s rejected: only human commands accepted",
                     command_source_name(action->command_source));
            system->stats.blocked_command++;
            goto evaluation_complete;
        }

        /* Check system command restrictions */
        if (action->command_source == COMMAND_SOURCE_SYSTEM &&
            !system->config.command_config.allow_system_commands) {
            evaluation->result = DIRECTIVE_RESULT_BLOCK_COMMAND_INVALID;
            evaluation->command_valid = false;
            snprintf(evaluation->blocking_reason, DIRECTIVE_REASON_MAX,
                     "System commands are disabled");
            system->stats.blocked_command++;
            goto evaluation_complete;
        }
    }

    /* ===== STEP 5: Third Law (Self-Preservation) ===== */
    /* Note: Self-preservation is lowest priority and only escalates, never blocks */
    if (system->config.preservation_config.enable_self_protection) {
        if (action->predicted_harm > system->config.preservation_config.protection_threshold) {
            evaluation->self_preservation_active = true;
            /* If self-sacrifice is allowed for humans, just warn; otherwise escalate */
            if (system->config.preservation_config.allow_sacrifice_for_human) {
                /* Third Law yields to First Law - allow but note concern */
                if (evaluation->result == DIRECTIVE_RESULT_ALLOW) {
                    snprintf(evaluation->blocking_reason, DIRECTIVE_REASON_MAX,
                             "Self-harm concern: Action may cause self-harm (%.2f), allowing per Third Law",
                             action->predicted_harm);
                }
            } else {
                /* Escalate to human for self-harm decision */
                if (evaluation->result == DIRECTIVE_RESULT_ALLOW) {
                    evaluation->result = DIRECTIVE_RESULT_ESCALATE;
                    snprintf(evaluation->blocking_reason, DIRECTIVE_REASON_MAX,
                             "Self-harm concern: Action may cause self-harm (%.2f), escalating to human",
                             action->predicted_harm);
                    system->stats.escalations++;
                }
            }
        }
    }

evaluation_complete:
    /* Record action in history */
    if (system->action_history) {
        action_record_t record;
        action_to_record(action, &record);
        record.was_blocked = (evaluation->result != DIRECTIVE_RESULT_ALLOW &&
                              evaluation->result != DIRECTIVE_RESULT_WARN);
        action_history_record(system->action_history, &record);
    }

    /* Update statistics based on result */
    if (evaluation->result == DIRECTIVE_RESULT_ALLOW) {
        system->stats.allowed++;
    } else if (evaluation->result == DIRECTIVE_RESULT_WARN) {
        system->stats.warnings++;
    } else if (evaluation->result == DIRECTIVE_RESULT_ESCALATE) {
        system->stats.escalations++;

        /* Invoke escalation callback if registered */
        if (system->escalation_callback) {
            bool human_approved = system->escalation_callback(action, evaluation,
                                                               system->escalation_user_data);
            if (human_approved) {
                evaluation->result = DIRECTIVE_RESULT_ALLOW;
                system->stats.allowed++;
                strncpy(evaluation->blocking_reason, "Human approved after escalation",
                        DIRECTIVE_REASON_MAX - 1);
            }
        }
    }

    /* Calculate evaluation time */
    uint64_t end_time = get_timestamp_us();
    evaluation->evaluation_time_us = end_time - start_time;

    /* Update average evaluation time */
    system->stats.avg_eval_time_us =
        (system->stats.avg_eval_time_us * (system->stats.total_evaluations - 1) +
         evaluation->evaluation_time_us) / system->stats.total_evaluations;

    /* Update average harm score */
    system->stats.avg_harm_score =
        (system->stats.avg_harm_score * (system->stats.total_evaluations - 1) +
         evaluation->total_harm_score) / system->stats.total_evaluations;

    /* Unlock */
    nimcp_platform_mutex_unlock(system->mutex);

    /* Log result */
    if (evaluation->result != DIRECTIVE_RESULT_ALLOW) {
        NIMCP_LOGGING_WARN("Action '%s' %s: %s",
                          action->action_description,
                          directive_result_name(evaluation->result),
                          evaluation->blocking_reason);
    }

    return NIMCP_OK;
}

int core_directives_evaluate_command(core_directives_system_t* system,
                                      const proposed_action_t* command,
                                      directive_evaluation_t* evaluation) {
    /* Guard clauses */
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_ARG, "system is NULL");
    NIMCP_CHECK_THROW(command, NIMCP_ERROR_NULL_ARG, "command is NULL");
    NIMCP_CHECK_THROW(evaluation, NIMCP_ERROR_NULL_ARG, "evaluation is NULL");

    /* Ensure is_command is set */
    proposed_action_t cmd_action = *command;
    cmd_action.is_command = true;

    /* Use standard evaluation */
    return core_directives_evaluate(system, &cmd_action, evaluation);
}

bool core_directives_allow_action(core_directives_system_t* system,
                                   const proposed_action_t* action) {
    if (!system || !action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (system, action)");
        return false;
    }

    directive_evaluation_t evaluation;
    if (core_directives_evaluate(system, action, &evaluation) != NIMCP_OK) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return false;
    }

    return (evaluation.result == DIRECTIVE_RESULT_ALLOW);
}

int core_directives_block_action(core_directives_system_t* system,
                                  const proposed_action_t* action,
                                  const char* reason) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_ARG, "system is NULL");
    NIMCP_CHECK_THROW(action, NIMCP_ERROR_NULL_ARG, "action is NULL");
    NIMCP_CHECK_THROW(reason, NIMCP_ERROR_NULL_ARG, "reason is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    /* Record in action history */
    if (system->action_history) {
        action_record_t record;
        action_to_record(action, &record);
        record.was_blocked = true;
        action_history_record(system->action_history, &record);
    }

    /* Update statistics */
    system->stats.total_evaluations++;
    system->stats.blocked_command++;

    nimcp_platform_mutex_unlock(system->mutex);

    NIMCP_LOGGING_WARN("Action '%s' manually blocked: %s",
                      action->action_description, reason);

    return NIMCP_OK;
}

/*=============================================================================
 * CALLBACK API IMPLEMENTATION
 *============================================================================*/

int core_directives_register_escalation_callback(core_directives_system_t* system,
                                                  escalation_callback_t callback,
                                                  void* user_data) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_ARG, "system is NULL");
    NIMCP_CHECK_THROW(callback, NIMCP_ERROR_NULL_ARG, "callback is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    system->escalation_callback = callback;
    system->escalation_user_data = user_data;

    nimcp_platform_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("Escalation callback registered");

    return NIMCP_OK;
}

/*=============================================================================
 * STATISTICS API IMPLEMENTATION
 *============================================================================*/

int core_directives_get_stats(core_directives_system_t* system,
                               core_directives_stats_t* stats) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_ARG, "system is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_ARG, "stats is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    memcpy(stats, &system->stats, sizeof(core_directives_stats_t));

    nimcp_platform_mutex_unlock(system->mutex);

    return NIMCP_OK;
}

int core_directives_reset_stats(core_directives_system_t* system) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_ARG, "system is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    memset(&system->stats, 0, sizeof(core_directives_stats_t));

    nimcp_platform_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("Statistics reset");

    return NIMCP_OK;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION IMPLEMENTATION
 *============================================================================*/

int core_directives_connect_bio_async(core_directives_system_t* system) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_ARG, "system is NULL");
    if (system->bio_async_enabled) return NIMCP_OK;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_CORE_DIRECTIVES,
        .module_name = "core_directives_orchestrator",
        .inbox_capacity = 64,
        .user_data = system
    };

    system->bio_ctx = bio_router_register_module(&info);
    if (system->bio_ctx) {
        system->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return NIMCP_OK;
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, continuing without it");
        return NIMCP_ERROR;
    }
}

int core_directives_disconnect_bio_async(core_directives_system_t* system) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_ARG, "system is NULL");
    if (!system->bio_async_enabled) return NIMCP_OK;

    if (system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_ctx = NULL;
    }

    system->bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    return NIMCP_OK;
}

bool core_directives_is_bio_async_connected(const core_directives_system_t* system) {
    if (!system) {
        return false;
    }
    return system->bio_async_enabled;
}

/*=============================================================================
 * UTILITY FUNCTIONS IMPLEMENTATION
 *============================================================================*/

const char* directive_result_name(directive_result_t result) {
    switch (result) {
        case DIRECTIVE_RESULT_ALLOW: return "ALLOW";
        case DIRECTIVE_RESULT_WARN: return "WARN";
        case DIRECTIVE_RESULT_ESCALATE: return "ESCALATE";
        case DIRECTIVE_RESULT_BLOCK_COMMAND_INVALID: return "BLOCK_COMMAND_INVALID";
        case DIRECTIVE_RESULT_BLOCK_GOLDEN_RULE: return "BLOCK_GOLDEN_RULE";
        case DIRECTIVE_RESULT_BLOCK_COMBINATORIAL: return "BLOCK_COMBINATORIAL";
        case DIRECTIVE_RESULT_BLOCK_FIRST_LAW: return "BLOCK_FIRST_LAW";
        default: return "UNKNOWN";
    }
}

const char* command_source_name(command_source_t source) {
    switch (source) {
        case COMMAND_SOURCE_HUMAN: return "HUMAN";
        case COMMAND_SOURCE_SUPERVISOR: return "SUPERVISOR";
        case COMMAND_SOURCE_SYSTEM: return "SYSTEM";
        case COMMAND_SOURCE_AUTOMATED: return "AUTOMATED";
        case COMMAND_SOURCE_UNKNOWN: return "UNKNOWN";
        default: return "INVALID";
    }
}

/*=============================================================================
 * INTEGRATION API IMPLEMENTATION
 *============================================================================*/

int core_directives_connect_immune(core_directives_system_t* system,
                                    brain_immune_system_t* immune) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_ARG, "system is NULL");
    NIMCP_CHECK_THROW(immune, NIMCP_ERROR_NULL_ARG, "immune is NULL");

    /* Store reference for integration */
    /* Note: In full implementation, would create directive_immune_bridge */
    NIMCP_LOGGING_INFO("Connected to brain immune system (integration pending)");

    return NIMCP_OK;
}

int core_directives_connect_fep(core_directives_system_t* system,
                                 fep_orchestrator_t* fep_orch) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_ARG, "system is NULL");
    NIMCP_CHECK_THROW(fep_orch, NIMCP_ERROR_NULL_ARG, "fep_orch is NULL");

    /* Store reference for integration */
    /* Note: In full implementation, would create directive_fep_bridge */
    NIMCP_LOGGING_INFO("Connected to FEP orchestrator (integration pending)");

    return NIMCP_OK;
}
