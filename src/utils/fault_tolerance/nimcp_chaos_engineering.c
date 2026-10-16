/**
 * @file nimcp_chaos_engineering.c
 * @brief Chaos Engineering Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Controlled fault injection for testing system resilience
 * WHY:  Verify fault tolerance works in practice, not just theory
 * HOW:  Inject failures, verify recovery, measure impact
 *
 * BIOLOGICAL BASIS:
 * - Immune system stress testing (vaccination = controlled infection)
 * - Neural plasticity through damage (stroke recovery reveals redundancy)
 * - Adaptation to stressors (hormesis - low-dose stress improves resilience)
 * - Antifragility (systems that benefit from disorder)
 *
 * IMMUNE SYSTEM INTEGRATION:
 * - All experiments are logged for security audit
 * - Security module can veto unsafe experiments
 * - Fault injection respects security boundaries
 */

#include "utils/fault_tolerance/nimcp_chaos_engineering.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(chaos_engineering)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Injection callback registration
 */
typedef struct {
    ce_fault_type_t fault_type;
    ce_inject_callback_t inject_callback;
    ce_rollback_callback_t rollback_callback;
    void* user_data;
    bool active;
} ce_callback_entry_t;

/**
 * @brief Active fault injection
 */
typedef struct {
    ce_fault_spec_t fault;
    ce_target_spec_t target;
    uint64_t start_time_ms;
    uint64_t end_time_ms;
    bool active;
} ce_active_fault_t;

/**
 * @brief Game day tracking
 */
typedef struct {
    uint32_t game_day_id;
    uint32_t experiment_ids[CE_MAX_EXPERIMENTS];
    uint32_t experiment_count;
    uint64_t scheduled_at_ms;
    uint64_t started_at_ms;
    bool running;
    bool completed;
} ce_game_day_t;

/**
 * @brief Internal context structure
 */
struct ce_context {
    ce_config_t config;

    /* Experiments */
    ce_experiment_t experiments[CE_MAX_EXPERIMENTS];
    uint32_t experiment_count;
    uint32_t next_experiment_id;

    /* Callbacks */
    ce_callback_entry_t callbacks[32];
    uint32_t callback_count;

    /* Event callbacks */
    ce_event_callback_t event_callbacks[8];
    void* event_user_data[8];
    uint32_t event_callback_count;

    /* Active faults */
    ce_active_fault_t active_faults[16];
    uint32_t active_fault_count;

    /* Game days */
    ce_game_day_t game_days[4];
    uint32_t game_day_count;
    uint32_t next_game_day_id;

    /* Dry run mode */
    bool dry_run;

    /* Threading */
    nimcp_mutex_t mutex;
    nimcp_thread_t experiment_thread;
    bool thread_running;

    /* Security integration */
    bool security_registered;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds
 */
static uint64_t ce_get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Find experiment by ID
 */
static ce_experiment_t* ce_find_experiment(ce_context_t* ctx, uint32_t experiment_id) {
    for (uint32_t i = 0; i < ctx->experiment_count; i++) {
        if (ctx->experiments[i].experiment_id == experiment_id) {
            return &ctx->experiments[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_find_experiment: validation failed");
    return NULL;
}

/**
 * @brief Notify event callbacks
 */
static void ce_notify_event(ce_context_t* ctx, uint32_t experiment_id, ce_state_t state, const char* message) {
    for (uint32_t i = 0; i < ctx->event_callback_count; i++) {
        if (ctx->event_callbacks[i]) {
            ctx->event_callbacks[i](experiment_id, state, message, ctx->event_user_data[i]);
        }
    }
}

/**
 * @brief Audit log for security
 */
static void ce_audit_log(const char* action, uint32_t experiment_id, const char* message) {
    bbb_audit_log(BBB_AUDIT_WARNING, "CE", action,
                  "experiment=%u: %s", experiment_id, message);
}

/**
 * @brief Check safety guardrails
 */
static bool ce_check_guardrails(ce_context_t* ctx, ce_experiment_t* exp) {
    if (!ctx || !exp) {
        return false;
    }

    for (uint32_t i = 0; i < exp->guardrail_count; i++) {
        ce_guardrail_config_t* guard = &exp->guardrails[i];

        switch (guard->type) {
            case CE_GUARDRAIL_BLAST_RADIUS:
                if (exp->target.node_count > (uint32_t)guard->threshold) {
                    LOG_WARNING("CE", "Blast radius guardrail violated: %u > %.0f",
                                      exp->target.node_count, guard->threshold);
                    if (guard->abort_on_violation) {
                        return false;
                    }
                }
                break;

            case CE_GUARDRAIL_TIME_LIMIT:
                /* Check during experiment */
                break;

            default:
                break;
        }
    }

    /* Check global safety */
    if (ctx->config.enable_safety_checks) {
        if (exp->target.node_count > ctx->config.max_blast_radius) {
            LOG_WARNING("CE", "Global blast radius exceeded: %u > %u",
                              exp->target.node_count, ctx->config.max_blast_radius);
            return false;
        }

        if (exp->max_duration_ms > ctx->config.max_experiment_duration_ms) {
            LOG_WARNING("CE", "Global duration exceeded: %lu > %lu",
                              (unsigned long)exp->max_duration_ms,
                              (unsigned long)ctx->config.max_experiment_duration_ms);
            return false;
        }
    }

    return true;
}

/**
 * @brief Execute fault injection
 */
static bool ce_execute_inject(ce_context_t* ctx, const ce_fault_spec_t* fault, const ce_target_spec_t* target) {
    if (!ctx || !fault || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_execute_inject: required parameter is NULL (ctx, fault, target)");
        return false;
    }

    /* Find callback */
    for (uint32_t i = 0; i < ctx->callback_count; i++) {
        if (ctx->callbacks[i].active &&
            ctx->callbacks[i].fault_type == fault->type &&
            ctx->callbacks[i].inject_callback) {

            if (ctx->dry_run) {
                LOG_INFO("CE", "[DRY RUN] Would inject %s fault",
                               ce_fault_type_to_string(fault->type));
                return true;
            }

            return ctx->callbacks[i].inject_callback(fault, target, ctx->callbacks[i].user_data);
        }
    }

    /* Default injection (simulated) */
    if (ctx->dry_run) {
        LOG_INFO("CE", "[DRY RUN] Would inject %s fault (default)",
                       ce_fault_type_to_string(fault->type));
        return true;
    }

    LOG_DEBUG("CE", "Simulating %s fault injection", ce_fault_type_to_string(fault->type));
    return true;
}

/**
 * @brief Execute fault rollback
 */
static bool ce_execute_rollback(ce_context_t* ctx, const ce_fault_spec_t* fault, const ce_target_spec_t* target) {
    if (!ctx || !fault || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_execute_rollback: required parameter is NULL (ctx, fault, target)");
        return false;
    }

    /* Find callback */
    for (uint32_t i = 0; i < ctx->callback_count; i++) {
        if (ctx->callbacks[i].active &&
            ctx->callbacks[i].fault_type == fault->type &&
            ctx->callbacks[i].rollback_callback) {

            if (ctx->dry_run) {
                LOG_INFO("CE", "[DRY RUN] Would rollback %s fault",
                               ce_fault_type_to_string(fault->type));
                return true;
            }

            return ctx->callbacks[i].rollback_callback(fault, target, ctx->callbacks[i].user_data);
        }
    }

    LOG_DEBUG("CE", "Simulating %s fault rollback", ce_fault_type_to_string(fault->type));
    return true;
}

/**
 * @brief Evaluate hypotheses
 */
static void ce_evaluate_hypotheses(ce_experiment_t* exp) {
    if (!exp) return;

    for (uint32_t i = 0; i < exp->hypothesis_count; i++) {
        ce_hypothesis_t* hyp = &exp->hypotheses[i];

        /* Find matching metric */
        for (uint32_t m = 0; m < exp->metric_count; m++) {
            if (strcmp(exp->metrics[m].name, hyp->metric_name) == 0) {
                hyp->actual_value = (float)exp->metrics[m].during_experiment;

                if (hyp->actual_value >= hyp->expected_min &&
                    hyp->actual_value <= hyp->expected_max) {
                    hyp->result = CE_HYPOTHESIS_CONFIRMED;
                } else {
                    hyp->result = CE_HYPOTHESIS_REFUTED;
                }
                break;
            }
        }

        if (hyp->result == CE_HYPOTHESIS_UNKNOWN) {
            hyp->result = CE_HYPOTHESIS_INCONCLUSIVE;
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

ce_config_t ce_default_config(void) {
    ce_config_t config = {
        .enable_safety_checks = true,
        .max_blast_radius = CE_BLAST_RADIUS_MAX,
        .max_experiment_duration_ms = CE_DEFAULT_DURATION_MS * 5,
        .max_error_rate_increase = 10.0f,
        .min_availability = 0.9f,
        .require_confirmation = false,
        .auto_abort_on_critical = true,
        .enable_dry_run = false
    };
    return config;
}

ce_context_t* ce_create(const ce_config_t* config) {
    if (!config) {
        LOG_ERROR("CE", "NULL configuration provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;
    }

    ce_context_t* ctx = (ce_context_t*)nimcp_malloc(sizeof(ce_context_t));
    if (!ctx) {
        LOG_ERROR("CE", "Failed to allocate context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }

    memset(ctx, 0, sizeof(ce_context_t));
    ctx->config = *config;
    ctx->next_experiment_id = 1;
    ctx->next_game_day_id = 1;
    ctx->dry_run = config->enable_dry_run;

    if (nimcp_mutex_init(&ctx->mutex, NULL) != 0) {
        LOG_ERROR("CE", "Failed to initialize mutex");
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "ce_create: validation failed");
        return NULL;
    }

    /* Register with security module */
    ctx->security_registered = bbb_register_module("chaos_engineering", BBB_MODULE_TYPE_CORE);

    bbb_audit_log(BBB_AUDIT_INFO, "CE", "CREATE",
                  "Created chaos engineering context, safety=%d, dry_run=%d",
                  config->enable_safety_checks, config->enable_dry_run);

    LOG_INFO("CE", "Created chaos engineering context");

    return ctx;
}

void ce_destroy(ce_context_t* ctx) {
    if (!ctx) return;

    /* Stop any running experiments */
    nimcp_mutex_lock(&ctx->mutex);
    for (uint32_t i = 0; i < ctx->experiment_count; i++) {
        if (ctx->experiments[i].state == CE_STATE_RUNNING) {
            ctx->experiments[i].state = CE_STATE_ABORTED;
        }
    }
    ctx->thread_running = false;
    nimcp_mutex_unlock(&ctx->mutex);

    /* Rollback active faults */
    for (uint32_t i = 0; i < ctx->active_fault_count; i++) {
        if (ctx->active_faults[i].active) {
            ce_execute_rollback(ctx, &ctx->active_faults[i].fault, &ctx->active_faults[i].target);
        }
    }

    bbb_audit_log(BBB_AUDIT_INFO, "CE", "DESTROY",
                  "Destroying chaos engineering context");

    nimcp_mutex_destroy(&ctx->mutex);
    nimcp_free(ctx);

    LOG_INFO("CE", "Destroyed chaos engineering context");
}

//=============================================================================
// Experiment Lifecycle
//=============================================================================

uint32_t ce_create_experiment(ce_context_t* ctx, const char* name, const char* description) {
    if (!ctx || !name) return 0;

    nimcp_mutex_lock(&ctx->mutex);

    if (ctx->experiment_count >= CE_MAX_EXPERIMENTS) {
        LOG_WARNING("CE", "Maximum experiments reached");
        nimcp_mutex_unlock(&ctx->mutex);
        return 0;
    }

    ce_experiment_t* exp = &ctx->experiments[ctx->experiment_count];
    memset(exp, 0, sizeof(ce_experiment_t));

    exp->experiment_id = ctx->next_experiment_id++;
    strncpy(exp->name, name, sizeof(exp->name) - 1);
    if (description) {
        strncpy(exp->description, description, sizeof(exp->description) - 1);
    }
    exp->state = CE_STATE_CREATED;
    exp->max_duration_ms = CE_DEFAULT_DURATION_MS;
    exp->auto_rollback = true;

    ctx->experiment_count++;

    uint32_t id = exp->experiment_id;

    nimcp_mutex_unlock(&ctx->mutex);

    ce_audit_log("CREATE_EXPERIMENT", id, name);
    LOG_DEBUG("CE", "Created experiment: %s (id=%u)", name, id);

    return id;
}

bool ce_set_fault(ce_context_t* ctx, uint32_t experiment_id, const ce_fault_spec_t* fault) {
    if (!ctx || !fault || experiment_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_set_fault: required parameter is NULL (ctx, fault)");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    ce_experiment_t* exp = ce_find_experiment(ctx, experiment_id);
    if (!exp) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_set_fault: exp is NULL");
        return false;
    }

    exp->fault = *fault;

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

bool ce_set_target(ce_context_t* ctx, uint32_t experiment_id, const ce_target_spec_t* target) {
    if (!ctx || !target || experiment_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_set_target: required parameter is NULL (ctx, target)");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    ce_experiment_t* exp = ce_find_experiment(ctx, experiment_id);
    if (!exp) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_set_target: exp is NULL");
        return false;
    }

    exp->target = *target;

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

bool ce_add_hypothesis(ce_context_t* ctx, uint32_t experiment_id, const ce_hypothesis_t* hypothesis) {
    if (!ctx || !hypothesis || experiment_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_add_hypothesis: required parameter is NULL (ctx, hypothesis)");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    ce_experiment_t* exp = ce_find_experiment(ctx, experiment_id);
    if (!exp || exp->hypothesis_count >= CE_MAX_HYPOTHESIS) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "ce_add_hypothesis: exp is NULL");
        return false;
    }

    exp->hypotheses[exp->hypothesis_count++] = *hypothesis;

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

bool ce_add_guardrail(ce_context_t* ctx, uint32_t experiment_id, const ce_guardrail_config_t* guardrail) {
    if (!ctx || !guardrail || experiment_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_add_guardrail: required parameter is NULL (ctx, guardrail)");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    ce_experiment_t* exp = ce_find_experiment(ctx, experiment_id);
    if (!exp || exp->guardrail_count >= 8) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "ce_add_guardrail: exp is NULL");
        return false;
    }

    exp->guardrails[exp->guardrail_count++] = *guardrail;

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

bool ce_add_metric(ce_context_t* ctx, uint32_t experiment_id, const char* metric_name) {
    if (!ctx || !metric_name || experiment_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_add_metric: required parameter is NULL (ctx, metric_name)");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    ce_experiment_t* exp = ce_find_experiment(ctx, experiment_id);
    if (!exp || exp->metric_count >= CE_MAX_METRICS) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "ce_add_metric: exp is NULL");
        return false;
    }

    strncpy(exp->metrics[exp->metric_count].name, metric_name,
            sizeof(exp->metrics[exp->metric_count].name) - 1);
    exp->metric_count++;

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

//=============================================================================
// Experiment Execution
//=============================================================================

bool ce_start_experiment(ce_context_t* ctx, uint32_t experiment_id) {
    if (!ctx || experiment_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ce_start_experiment: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    ce_experiment_t* exp = ce_find_experiment(ctx, experiment_id);
    if (!exp) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_start_experiment: exp is NULL");
        return false;
    }

    if (exp->state != CE_STATE_CREATED && exp->state != CE_STATE_READY) {
        LOG_WARNING("CE", "Experiment %u not in startable state", experiment_id);
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ce_start_experiment: validation failed");
        return false;
    }

    /* Reject experiments with no fault type configured */
    if (exp->fault.type == CE_FAULT_NONE) {
        LOG_WARNING("CE", "Experiment %u has no fault type configured", experiment_id);
        exp->state = CE_STATE_FAILED;
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ce_start_experiment: no fault type configured");
        return false;
    }

    /* Check safety */
    if (!ce_check_guardrails(ctx, exp)) {
        exp->state = CE_STATE_FAILED;
        nimcp_mutex_unlock(&ctx->mutex);
        ce_audit_log("GUARDRAIL_BLOCKED", experiment_id, "Safety guardrails violated");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ce_start_experiment: ce_check_guardrails is NULL");
        return false;
    }

    /* Start experiment */
    exp->state = CE_STATE_RUNNING;
    exp->started_at_ms = ce_get_time_ms();

    nimcp_mutex_unlock(&ctx->mutex);

    /* Inject fault */
    bool injected = ce_execute_inject(ctx, &exp->fault, &exp->target);

    if (injected) {
        nimcp_mutex_lock(&ctx->mutex);

        /* Track active fault */
        if (ctx->active_fault_count < 16) {
            ce_active_fault_t* af = &ctx->active_faults[ctx->active_fault_count++];
            af->fault = exp->fault;
            af->target = exp->target;
            af->start_time_ms = exp->started_at_ms;
            af->end_time_ms = exp->started_at_ms + exp->fault.duration_ms;
            af->active = true;
        }

        nimcp_mutex_unlock(&ctx->mutex);

        ce_audit_log("START_EXPERIMENT", experiment_id, exp->name);
        ce_notify_event(ctx, experiment_id, CE_STATE_RUNNING, "Experiment started");

        LOG_INFO("CE", "Started experiment: %s (id=%u)", exp->name, experiment_id);
    } else {
        nimcp_mutex_lock(&ctx->mutex);
        exp->state = CE_STATE_FAILED;
        nimcp_mutex_unlock(&ctx->mutex);

        ce_audit_log("INJECTION_FAILED", experiment_id, "Fault injection failed");
    }

    return injected;
}

bool ce_pause_experiment(ce_context_t* ctx, uint32_t experiment_id) {
    if (!ctx || experiment_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ce_pause_experiment: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    ce_experiment_t* exp = ce_find_experiment(ctx, experiment_id);
    if (!exp || exp->state != CE_STATE_RUNNING) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ce_pause_experiment: exp is NULL");
        return false;
    }

    exp->state = CE_STATE_PAUSED;

    nimcp_mutex_unlock(&ctx->mutex);

    /* Rollback fault temporarily */
    ce_execute_rollback(ctx, &exp->fault, &exp->target);

    ce_audit_log("PAUSE_EXPERIMENT", experiment_id, "Paused");
    ce_notify_event(ctx, experiment_id, CE_STATE_PAUSED, "Experiment paused");

    return true;
}

bool ce_resume_experiment(ce_context_t* ctx, uint32_t experiment_id) {
    if (!ctx || experiment_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ce_resume_experiment: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    ce_experiment_t* exp = ce_find_experiment(ctx, experiment_id);
    if (!exp || exp->state != CE_STATE_PAUSED) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ce_resume_experiment: exp is NULL");
        return false;
    }

    exp->state = CE_STATE_RUNNING;

    nimcp_mutex_unlock(&ctx->mutex);

    /* Re-inject fault */
    ce_execute_inject(ctx, &exp->fault, &exp->target);

    ce_audit_log("RESUME_EXPERIMENT", experiment_id, "Resumed");
    ce_notify_event(ctx, experiment_id, CE_STATE_RUNNING, "Experiment resumed");

    return true;
}

bool ce_abort_experiment(ce_context_t* ctx, uint32_t experiment_id, const char* reason) {
    if (!ctx || experiment_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ce_abort_experiment: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    ce_experiment_t* exp = ce_find_experiment(ctx, experiment_id);
    if (!exp) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_abort_experiment: exp is NULL");
        return false;
    }

    if (exp->state != CE_STATE_RUNNING && exp->state != CE_STATE_PAUSED) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ce_abort_experiment: validation failed");
        return false;
    }

    exp->state = CE_STATE_ABORTED;
    exp->ended_at_ms = ce_get_time_ms();

    nimcp_mutex_unlock(&ctx->mutex);

    /* Rollback */
    if (exp->auto_rollback) {
        ce_execute_rollback(ctx, &exp->fault, &exp->target);
    }

    /* Clear from active faults */
    nimcp_mutex_lock(&ctx->mutex);
    for (uint32_t i = 0; i < ctx->active_fault_count; i++) {
        if (ctx->active_faults[i].active) {
            ctx->active_faults[i].active = false;
        }
    }
    nimcp_mutex_unlock(&ctx->mutex);

    ce_audit_log("ABORT_EXPERIMENT", experiment_id, reason ? reason : "Manual abort");
    ce_notify_event(ctx, experiment_id, CE_STATE_ABORTED, reason ? reason : "Experiment aborted");

    LOG_WARNING("CE", "Aborted experiment: %u (%s)", experiment_id, reason ? reason : "unspecified");

    return true;
}

ce_state_t ce_get_experiment_state(ce_context_t* ctx, uint32_t experiment_id) {
    if (!ctx || experiment_id == 0) return CE_STATE_FAILED;

    nimcp_mutex_lock(&ctx->mutex);

    ce_experiment_t* exp = ce_find_experiment(ctx, experiment_id);
    ce_state_t state = exp ? exp->state : CE_STATE_FAILED;

    nimcp_mutex_unlock(&ctx->mutex);

    return state;
}

bool ce_get_experiment(ce_context_t* ctx, uint32_t experiment_id, ce_experiment_t* experiment) {
    if (!ctx || !experiment || experiment_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_get_experiment: required parameter is NULL (ctx, experiment)");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    ce_experiment_t* exp = ce_find_experiment(ctx, experiment_id);
    if (!exp) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_get_experiment: exp is NULL");
        return false;
    }

    *experiment = *exp;

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

bool ce_get_result(ce_context_t* ctx, uint32_t experiment_id, ce_result_t* result) {
    if (!ctx || !result || experiment_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_get_result: required parameter is NULL (ctx, result)");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    ce_experiment_t* exp = ce_find_experiment(ctx, experiment_id);
    if (!exp) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_get_result: exp is NULL");
        return false;
    }

    if (exp->state != CE_STATE_COMPLETED && exp->state != CE_STATE_ABORTED) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ce_get_result: validation failed");
        return false;
    }

    memset(result, 0, sizeof(ce_result_t));

    result->experiment_id = experiment_id;
    result->final_state = exp->state;
    result->duration_ms = exp->ended_at_ms - exp->started_at_ms;
    result->faults_injected = 1;  /* Simplified */

    /* Count hypothesis results */
    for (uint32_t i = 0; i < exp->hypothesis_count; i++) {
        if (exp->hypotheses[i].result == CE_HYPOTHESIS_CONFIRMED) {
            result->hypotheses_confirmed++;
        } else if (exp->hypotheses[i].result == CE_HYPOTHESIS_REFUTED) {
            result->hypotheses_refuted++;
        }
    }

    result->system_recovered = (exp->state == CE_STATE_COMPLETED);

    snprintf(result->summary, sizeof(result->summary),
             "Experiment '%s': %u hypotheses confirmed, %u refuted. Duration: %lu ms",
             exp->name, result->hypotheses_confirmed, result->hypotheses_refuted,
             (unsigned long)result->duration_ms);

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

//=============================================================================
// Direct Fault Injection
//=============================================================================

bool ce_inject_fault(ce_context_t* ctx, const ce_fault_spec_t* fault, const ce_target_spec_t* target) {
    if (!ctx || !fault || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_inject_fault: required parameter is NULL (ctx, fault, target)");
        return false;
    }

    ce_audit_log("DIRECT_INJECT", 0, ce_fault_type_to_string(fault->type));

    return ce_execute_inject(ctx, fault, target);
}

bool ce_rollback_fault(ce_context_t* ctx, const ce_fault_spec_t* fault, const ce_target_spec_t* target) {
    if (!ctx || !fault || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_rollback_fault: required parameter is NULL (ctx, fault, target)");
        return false;
    }

    ce_audit_log("DIRECT_ROLLBACK", 0, ce_fault_type_to_string(fault->type));

    return ce_execute_rollback(ctx, fault, target);
}

bool ce_inject_network_latency(ce_context_t* ctx, uint32_t target_id, uint32_t latency_ms, uint64_t duration_ms) {
    ce_fault_spec_t fault = {
        .type = CE_FAULT_NETWORK_LATENCY,
        .pattern = CE_PATTERN_ONCE,
        .intensity = (float)latency_ms,
        .duration_ms = duration_ms,
        .probability = 1.0f
    };

    ce_target_spec_t target = {
        .strategy = CE_TARGET_SPECIFIC,
        .node_count = 1
    };
    target.node_ids[0] = target_id;
    snprintf(target.name, sizeof(target.name), "node_%u", target_id);

    return ce_inject_fault(ctx, &fault, &target);
}

bool ce_inject_packet_loss(ce_context_t* ctx, uint32_t target_id, float loss_percent, uint64_t duration_ms) {
    ce_fault_spec_t fault = {
        .type = CE_FAULT_NETWORK_LOSS,
        .pattern = CE_PATTERN_ONCE,
        .intensity = loss_percent,
        .duration_ms = duration_ms,
        .probability = 1.0f
    };

    ce_target_spec_t target = {
        .strategy = CE_TARGET_SPECIFIC,
        .node_count = 1
    };
    target.node_ids[0] = target_id;

    return ce_inject_fault(ctx, &fault, &target);
}

bool ce_inject_cpu_stress(ce_context_t* ctx, uint32_t target_id, uint32_t cpu_percent, uint64_t duration_ms) {
    ce_fault_spec_t fault = {
        .type = CE_FAULT_CPU_STRESS,
        .pattern = CE_PATTERN_ONCE,
        .intensity = (float)cpu_percent,
        .duration_ms = duration_ms,
        .probability = 1.0f
    };

    ce_target_spec_t target = {
        .strategy = CE_TARGET_SPECIFIC,
        .node_count = 1
    };
    target.node_ids[0] = target_id;

    return ce_inject_fault(ctx, &fault, &target);
}

bool ce_inject_memory_pressure(ce_context_t* ctx, uint32_t target_id, size_t bytes, uint64_t duration_ms) {
    ce_fault_spec_t fault = {
        .type = CE_FAULT_MEMORY_LEAK,
        .pattern = CE_PATTERN_ONCE,
        .intensity = (float)bytes,
        .duration_ms = duration_ms,
        .probability = 1.0f
    };

    ce_target_spec_t target = {
        .strategy = CE_TARGET_SPECIFIC,
        .node_count = 1
    };
    target.node_ids[0] = target_id;

    return ce_inject_fault(ctx, &fault, &target);
}

bool ce_kill_process(ce_context_t* ctx, uint32_t target_id) {
    ce_fault_spec_t fault = {
        .type = CE_FAULT_PROCESS_KILL,
        .pattern = CE_PATTERN_ONCE,
        .intensity = 100.0f,
        .duration_ms = 0,
        .probability = 1.0f
    };

    ce_target_spec_t target = {
        .strategy = CE_TARGET_SPECIFIC,
        .node_count = 1
    };
    target.node_ids[0] = target_id;

    return ce_inject_fault(ctx, &fault, &target);
}

bool ce_create_partition(ce_context_t* ctx, const uint32_t* group1, uint32_t count1,
                          const uint32_t* group2, uint32_t count2, uint64_t duration_ms) {
    if (!ctx || !group1 || !group2 || count1 == 0 || count2 == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_kill_process: required parameter is NULL (ctx, group1, group2)");
        return false;
    }

    ce_fault_spec_t fault = {
        .type = CE_FAULT_NETWORK_PARTITION,
        .pattern = CE_PATTERN_ONCE,
        .intensity = 100.0f,
        .duration_ms = duration_ms,
        .probability = 1.0f
    };

    ce_target_spec_t target = {
        .strategy = CE_TARGET_SPECIFIC,
        .node_count = count1 + count2
    };

    /* Store both groups */
    uint32_t idx = 0;
    for (uint32_t i = 0; i < count1 && idx < CE_MAX_TARGETS; i++) {
        target.node_ids[idx++] = group1[i];
    }
    for (uint32_t i = 0; i < count2 && idx < CE_MAX_TARGETS; i++) {
        target.node_ids[idx++] = group2[i];
    }

    snprintf(target.name, sizeof(target.name), "partition_%u_%u", count1, count2);

    ce_audit_log("NETWORK_PARTITION", 0, target.name);

    return ce_inject_fault(ctx, &fault, &target);
}

//=============================================================================
// Callbacks
//=============================================================================

bool ce_register_inject_callback(ce_context_t* ctx, ce_fault_type_t fault_type,
                                   ce_inject_callback_t callback, void* user_data) {
    if (!ctx || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_kill_process: required parameter is NULL (ctx, callback)");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    if (ctx->callback_count >= 32) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "ce_kill_process: capacity exceeded");
        return false;
    }

    ce_callback_entry_t* entry = &ctx->callbacks[ctx->callback_count++];
    entry->fault_type = fault_type;
    entry->inject_callback = callback;
    entry->rollback_callback = NULL;
    entry->user_data = user_data;
    entry->active = true;

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

bool ce_register_rollback_callback(ce_context_t* ctx, ce_fault_type_t fault_type,
                                     ce_rollback_callback_t callback, void* user_data) {
    if (!ctx || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_kill_process: required parameter is NULL (ctx, callback)");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    /* Find existing entry for this fault type */
    for (uint32_t i = 0; i < ctx->callback_count; i++) {
        if (ctx->callbacks[i].fault_type == fault_type) {
            ctx->callbacks[i].rollback_callback = callback;
            nimcp_mutex_unlock(&ctx->mutex);
            return true;
        }
    }

    /* Create new entry */
    if (ctx->callback_count >= 32) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "ce_kill_process: capacity exceeded");
        return false;
    }

    ce_callback_entry_t* entry = &ctx->callbacks[ctx->callback_count++];
    entry->fault_type = fault_type;
    entry->inject_callback = NULL;
    entry->rollback_callback = callback;
    entry->user_data = user_data;
    entry->active = true;

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

bool ce_register_event_callback(ce_context_t* ctx, ce_event_callback_t callback, void* user_data) {
    if (!ctx || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_register_event_callback: required parameter is NULL (ctx, callback)");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    if (ctx->event_callback_count >= 8) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "ce_register_event_callback: capacity exceeded");
        return false;
    }

    ctx->event_callbacks[ctx->event_callback_count] = callback;
    ctx->event_user_data[ctx->event_callback_count] = user_data;
    ctx->event_callback_count++;

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

//=============================================================================
// Safety Functions
//=============================================================================

bool ce_is_safe_to_run(ce_context_t* ctx, uint32_t experiment_id) {
    if (!ctx || experiment_id == 0) {
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    ce_experiment_t* exp = ce_find_experiment(ctx, experiment_id);
    if (!exp) {
        nimcp_mutex_unlock(&ctx->mutex);
        return false;
    }

    bool safe = ce_check_guardrails(ctx, exp);

    nimcp_mutex_unlock(&ctx->mutex);

    return safe;
}

uint32_t ce_validate_experiment(ce_context_t* ctx, uint32_t experiment_id,
                                  char errors[][256], uint32_t max_errors) {
    if (!ctx || !errors || max_errors == 0 || experiment_id == 0) return 0;

    nimcp_mutex_lock(&ctx->mutex);

    ce_experiment_t* exp = ce_find_experiment(ctx, experiment_id);
    if (!exp) {
        snprintf(errors[0], 256, "Experiment %u not found", experiment_id);
        nimcp_mutex_unlock(&ctx->mutex);
        return 1;
    }

    uint32_t error_count = 0;

    /* Check fault spec */
    if (exp->fault.type == CE_FAULT_NONE) {
        snprintf(errors[error_count++], 256, "No fault type specified");
    }

    /* Check target */
    if (exp->target.node_count == 0 && exp->target.strategy == CE_TARGET_SPECIFIC) {
        if (error_count < max_errors) {
            snprintf(errors[error_count++], 256, "No targets specified for specific targeting");
        }
    }

    /* Check duration */
    if (exp->fault.duration_ms == 0 && exp->max_duration_ms == 0) {
        if (error_count < max_errors) {
            snprintf(errors[error_count++], 256, "No duration specified");
        }
    }

    /* Check blast radius */
    if (exp->target.node_count > ctx->config.max_blast_radius) {
        if (error_count < max_errors) {
            snprintf(errors[error_count++], 256, "Blast radius %u exceeds maximum %u",
                     exp->target.node_count, ctx->config.max_blast_radius);
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);

    return error_count;
}

void ce_set_dry_run(ce_context_t* ctx, bool enabled) {
    if (!ctx) return;

    nimcp_mutex_lock(&ctx->mutex);
    ctx->dry_run = enabled;
    nimcp_mutex_unlock(&ctx->mutex);

    LOG_INFO("CE", "Dry run mode %s", enabled ? "enabled" : "disabled");
}

bool ce_is_dry_run(ce_context_t* ctx) {
    if (!ctx) {
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);
    bool dry = ctx->dry_run;
    nimcp_mutex_unlock(&ctx->mutex);

    return dry;
}

//=============================================================================
// Game Days
//=============================================================================

uint32_t ce_schedule_game_day(ce_context_t* ctx, const uint32_t* experiment_ids,
                                uint32_t count, uint64_t start_time_ms) {
    if (!ctx || !experiment_ids || count == 0) return 0;

    nimcp_mutex_lock(&ctx->mutex);

    if (ctx->game_day_count >= 4) {
        nimcp_mutex_unlock(&ctx->mutex);
        return 0;
    }

    ce_game_day_t* gd = &ctx->game_days[ctx->game_day_count++];
    gd->game_day_id = ctx->next_game_day_id++;
    gd->scheduled_at_ms = start_time_ms;
    gd->running = false;
    gd->completed = false;

    gd->experiment_count = count < CE_MAX_EXPERIMENTS ? count : CE_MAX_EXPERIMENTS;
    memcpy(gd->experiment_ids, experiment_ids, gd->experiment_count * sizeof(uint32_t));

    uint32_t id = gd->game_day_id;

    nimcp_mutex_unlock(&ctx->mutex);

    ce_audit_log("SCHEDULE_GAME_DAY", id, "Scheduled");
    LOG_INFO("CE", "Scheduled game day %u with %u experiments", id, count);

    return id;
}

bool ce_start_game_day(ce_context_t* ctx, uint32_t game_day_id) {
    if (!ctx || game_day_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ce_start_game_day: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    ce_game_day_t* gd = NULL;
    for (uint32_t i = 0; i < ctx->game_day_count; i++) {
        if (ctx->game_days[i].game_day_id == game_day_id) {
            gd = &ctx->game_days[i];
            break;
        }
    }

    if (!gd || gd->running) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ce_start_game_day: gd is NULL");
        return false;
    }

    gd->running = true;
    gd->started_at_ms = ce_get_time_ms();

    nimcp_mutex_unlock(&ctx->mutex);

    /* Start all experiments */
    for (uint32_t i = 0; i < gd->experiment_count; i++) {
        ce_start_experiment(ctx, gd->experiment_ids[i]);
    }

    ce_audit_log("START_GAME_DAY", game_day_id, "Started");
    LOG_INFO("CE", "Started game day %u", game_day_id);

    return true;
}

bool ce_abort_game_day(ce_context_t* ctx, uint32_t game_day_id) {
    if (!ctx || game_day_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ce_abort_game_day: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    ce_game_day_t* gd = NULL;
    for (uint32_t i = 0; i < ctx->game_day_count; i++) {
        if (ctx->game_days[i].game_day_id == game_day_id) {
            gd = &ctx->game_days[i];
            break;
        }
    }

    if (!gd || !gd->running) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ce_abort_game_day: required parameter is NULL (gd, gd->running)");
        return false;
    }

    nimcp_mutex_unlock(&ctx->mutex);

    /* Abort all experiments */
    for (uint32_t i = 0; i < gd->experiment_count; i++) {
        ce_abort_experiment(ctx, gd->experiment_ids[i], "Game day aborted");
    }

    nimcp_mutex_lock(&ctx->mutex);
    gd->running = false;
    gd->completed = false;
    nimcp_mutex_unlock(&ctx->mutex);

    ce_audit_log("ABORT_GAME_DAY", game_day_id, "Aborted");
    LOG_WARNING("CE", "Aborted game day %u", game_day_id);

    return true;
}

//=============================================================================
// Reporting
//=============================================================================

size_t ce_generate_report(ce_context_t* ctx, uint32_t experiment_id, char* buffer, size_t buffer_size) {
    if (!ctx || !buffer || buffer_size == 0 || experiment_id == 0) return 0;

    nimcp_mutex_lock(&ctx->mutex);

    ce_experiment_t* exp = ce_find_experiment(ctx, experiment_id);
    if (!exp) {
        nimcp_mutex_unlock(&ctx->mutex);
        return 0;
    }

    size_t written = snprintf(buffer, buffer_size,
        "=== Chaos Experiment Report ===\n"
        "Name: %s\n"
        "ID: %u\n"
        "Description: %s\n"
        "State: %s\n"
        "Fault Type: %s\n"
        "Target Strategy: %s\n"
        "Target Count: %u\n"
        "Duration: %lu ms\n"
        "\n--- Hypotheses ---\n",
        exp->name, exp->experiment_id, exp->description,
        ce_state_to_string(exp->state),
        ce_fault_type_to_string(exp->fault.type),
        ce_target_strategy_to_string(exp->target.strategy),
        exp->target.node_count,
        (unsigned long)(exp->ended_at_ms - exp->started_at_ms));

    for (uint32_t i = 0; i < exp->hypothesis_count && written < buffer_size - 256; i++) {
        written += snprintf(buffer + written, buffer_size - written,
            "%u. %s\n   Expected: [%.2f, %.2f], Actual: %.2f, Result: %s\n",
            i + 1, exp->hypotheses[i].description,
            exp->hypotheses[i].expected_min, exp->hypotheses[i].expected_max,
            exp->hypotheses[i].actual_value,
            ce_hypothesis_result_to_string(exp->hypotheses[i].result));
    }

    nimcp_mutex_unlock(&ctx->mutex);

    return written;
}

uint32_t ce_list_experiments(ce_context_t* ctx, ce_experiment_t* experiments, uint32_t max_experiments) {
    if (!ctx || !experiments || max_experiments == 0) return 0;

    nimcp_mutex_lock(&ctx->mutex);

    uint32_t count = ctx->experiment_count < max_experiments ? ctx->experiment_count : max_experiments;
    memcpy(experiments, ctx->experiments, count * sizeof(ce_experiment_t));

    nimcp_mutex_unlock(&ctx->mutex);

    return count;
}

//=============================================================================
// String Conversion
//=============================================================================

const char* ce_fault_type_to_string(ce_fault_type_t type) {
    switch (type) {
        case CE_FAULT_NONE: return "None";
        case CE_FAULT_PROCESS_KILL: return "ProcessKill";
        case CE_FAULT_PROCESS_PAUSE: return "ProcessPause";
        case CE_FAULT_MEMORY_LEAK: return "MemoryLeak";
        case CE_FAULT_MEMORY_CORRUPT: return "MemoryCorrupt";
        case CE_FAULT_CPU_STRESS: return "CpuStress";
        case CE_FAULT_DISK_FULL: return "DiskFull";
        case CE_FAULT_DISK_SLOW: return "DiskSlow";
        case CE_FAULT_NETWORK_LATENCY: return "NetworkLatency";
        case CE_FAULT_NETWORK_LOSS: return "NetworkLoss";
        case CE_FAULT_NETWORK_PARTITION: return "NetworkPartition";
        case CE_FAULT_NETWORK_CORRUPT: return "NetworkCorrupt";
        case CE_FAULT_CLOCK_SKEW: return "ClockSkew";
        case CE_FAULT_DNS_FAILURE: return "DNSFailure";
        case CE_FAULT_SERVICE_OUTAGE: return "ServiceOutage";
        case CE_FAULT_BYZANTINE: return "Byzantine";
        case CE_FAULT_STATE_CORRUPT: return "StateCorrupt";
        case CE_FAULT_CHECKPOINT_CORRUPT: return "CheckpointCorrupt";
        case CE_FAULT_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

const char* ce_state_to_string(ce_state_t state) {
    switch (state) {
        case CE_STATE_CREATED: return "Created";
        case CE_STATE_READY: return "Ready";
        case CE_STATE_RUNNING: return "Running";
        case CE_STATE_PAUSED: return "Paused";
        case CE_STATE_COMPLETING: return "Completing";
        case CE_STATE_COMPLETED: return "Completed";
        case CE_STATE_FAILED: return "Failed";
        case CE_STATE_ABORTED: return "Aborted";
        default: return "Unknown";
    }
}

const char* ce_hypothesis_result_to_string(ce_hypothesis_result_t result) {
    switch (result) {
        case CE_HYPOTHESIS_UNKNOWN: return "Unknown";
        case CE_HYPOTHESIS_CONFIRMED: return "Confirmed";
        case CE_HYPOTHESIS_REFUTED: return "Refuted";
        case CE_HYPOTHESIS_INCONCLUSIVE: return "Inconclusive";
        default: return "Unknown";
    }
}

const char* ce_pattern_to_string(ce_pattern_t pattern) {
    switch (pattern) {
        case CE_PATTERN_ONCE: return "Once";
        case CE_PATTERN_PERIODIC: return "Periodic";
        case CE_PATTERN_RANDOM: return "Random";
        case CE_PATTERN_BURST: return "Burst";
        case CE_PATTERN_PROGRESSIVE: return "Progressive";
        default: return "Unknown";
    }
}

const char* ce_target_strategy_to_string(ce_target_strategy_t strategy) {
    switch (strategy) {
        case CE_TARGET_SPECIFIC: return "Specific";
        case CE_TARGET_RANDOM: return "Random";
        case CE_TARGET_PERCENTAGE: return "Percentage";
        case CE_TARGET_ROUND_ROBIN: return "RoundRobin";
        default: return "Unknown";
    }
}

const char* ce_guardrail_to_string(ce_guardrail_t guardrail) {
    switch (guardrail) {
        case CE_GUARDRAIL_NONE: return "None";
        case CE_GUARDRAIL_BLAST_RADIUS: return "BlastRadius";
        case CE_GUARDRAIL_TIME_LIMIT: return "TimeLimit";
        case CE_GUARDRAIL_ERROR_RATE: return "ErrorRate";
        case CE_GUARDRAIL_LATENCY: return "Latency";
        case CE_GUARDRAIL_AVAILABILITY: return "Availability";
        default: return "Unknown";
    }
}
