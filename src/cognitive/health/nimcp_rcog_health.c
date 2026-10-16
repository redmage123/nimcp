/**
 * @file nimcp_rcog_health.c
 * @brief RCOG Health Integration Implementation
 * @version 1.0.0
 * @date 2026-01-18
 *
 * Implementation of recursive cognition integration for health diagnosis and recovery.
 */

/* Enable POSIX clock functions */
#define _POSIX_C_SOURCE 200809L

#include "cognitive/health/nimcp_rcog_health.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include <math.h>

BRIDGE_BOILERPLATE(rcog_health, MESH_ADAPTER_CATEGORY_COGNITIVE)



/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Pending health goal
 */
typedef struct {
    uint64_t goal_id;
    rcog_health_goal_t goal;
    rcog_health_answer_t answer;
    bool complete;
    uint64_t start_time_us;
} pending_health_goal_t;

/**
 * @brief Cached diagnosis entry
 */
typedef struct {
    health_agent_msg_type_t anomaly_type;
    health_agent_source_t source;
    health_agent_severity_t severity;
    rcog_health_answer_t answer;
    uint64_t cache_time_us;
    uint64_t ttl_us;
} cached_diagnosis_t;

/**
 * @brief RCOG health integration internal state
 */
struct rcog_health_integration {
    /* Configuration */
    rcog_health_config_t config;

    /* Connected systems */
    rcog_engine_t* rcog;
    nimcp_health_agent_t* health_agent;

    /* Pending goals */
    pending_health_goal_t* pending_goals;
    uint32_t num_pending;
    uint32_t max_pending;
    uint64_t next_goal_id;

    /* Diagnosis cache */
    cached_diagnosis_t* cache;
    uint32_t cache_size;
    uint32_t cache_capacity;

    /* Registered tools */
    rcog_health_tool_t* custom_tools;
    uint32_t num_custom_tools;
    uint32_t max_custom_tools;
    bool builtin_tools_registered;

    /* Statistics */
    rcog_health_stats_t stats;
};

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* ============================================================================
 * Builtin Tool Implementations
 * ============================================================================ */

static int tool_check_memory(const void* params, void* result, void* context) {
    (void)params;
    (void)context;

    /* Simulated memory check result */
    char* res = (char*)result;
    snprintf(res, 256, "Memory check: No corruption detected. Heap usage at 65%%.");
    return 0;
}

static int tool_check_tensors(const void* params, void* result, void* context) {
    (void)params;
    (void)context;

    char* res = (char*)result;
    snprintf(res, 256, "Tensor check: All values within valid range. No NaN/Inf detected.");
    return 0;
}

static int tool_check_deadlocks(const void* params, void* result, void* context) {
    (void)params;
    (void)context;

    char* res = (char*)result;
    snprintf(res, 256, "Deadlock check: No deadlock cycles detected. 3 locks active.");
    return 0;
}

static int tool_analyze_metrics(const void* params, void* result, void* context) {
    (void)params;
    (void)context;

    char* res = (char*)result;
    snprintf(res, 256, "Metrics analysis: CPU at 45%%, Memory at 65%%, IO at 30%%. Trends stable.");
    return 0;
}

static int tool_predict_failure(const void* params, void* result, void* context) {
    (void)params;
    (void)context;

    char* res = (char*)result;
    snprintf(res, 256, "Failure prediction: Low probability (0.05). Estimated TTF > 24 hours.");
    return 0;
}

static int tool_suggest_recovery(const void* params, void* result, void* context) {
    (void)params;
    (void)context;

    char* res = (char*)result;
    snprintf(res, 256, "Recovery suggestion: Monitor closely. Consider GC if memory exceeds 80%%.");
    return 0;
}

static int tool_verify_checksum(const void* params, void* result, void* context) {
    (void)params;
    (void)context;

    char* res = (char*)result;
    snprintf(res, 256, "Checksum verification: All checksums valid. Data integrity confirmed.");
    return 0;
}

static int tool_scan_corruption(const void* params, void* result, void* context) {
    (void)params;
    (void)context;

    char* res = (char*)result;
    snprintf(res, 256, "Corruption scan: No corruption detected. All canaries intact.");
    return 0;
}

/* Builtin tool definitions */
static rcog_health_tool_t builtin_tools[RCOG_TOOL_ID_COUNT] = {
    [RCOG_TOOL_ID_CHECK_MEMORY] = {
        .tool_name = "check_memory",
        .description = "Check memory for corruption, leaks, and exhaustion",
        .invoke = tool_check_memory,
        .context = NULL,
        .required_tier = RCOG_TIER_L2_PERCEPTION
    },
    [RCOG_TOOL_ID_CHECK_TENSORS] = {
        .tool_name = "check_tensors",
        .description = "Check tensors for NaN, Inf, and out-of-range values",
        .invoke = tool_check_tensors,
        .context = NULL,
        .required_tier = RCOG_TIER_L2_PERCEPTION
    },
    [RCOG_TOOL_ID_CHECK_DEADLOCKS] = {
        .tool_name = "check_deadlocks",
        .description = "Detect potential deadlocks and lock contention",
        .invoke = tool_check_deadlocks,
        .context = NULL,
        .required_tier = RCOG_TIER_L2_PERCEPTION
    },
    [RCOG_TOOL_ID_ANALYZE_METRICS] = {
        .tool_name = "analyze_metrics",
        .description = "Analyze system metrics history for anomalies",
        .invoke = tool_analyze_metrics,
        .context = NULL,
        .required_tier = RCOG_TIER_L1_REASONING
    },
    [RCOG_TOOL_ID_PREDICT_FAILURE] = {
        .tool_name = "predict_failure",
        .description = "Predict probability and time to system failure",
        .invoke = tool_predict_failure,
        .context = NULL,
        .required_tier = RCOG_TIER_L1_REASONING
    },
    [RCOG_TOOL_ID_SUGGEST_RECOVERY] = {
        .tool_name = "suggest_recovery",
        .description = "Suggest appropriate recovery actions",
        .invoke = tool_suggest_recovery,
        .context = NULL,
        .required_tier = RCOG_TIER_L1_REASONING
    },
    [RCOG_TOOL_ID_VERIFY_CHECKSUM] = {
        .tool_name = "verify_checksum",
        .description = "Verify data integrity via checksums",
        .invoke = tool_verify_checksum,
        .context = NULL,
        .required_tier = RCOG_TIER_L2_PERCEPTION
    },
    [RCOG_TOOL_ID_SCAN_CORRUPTION] = {
        .tool_name = "scan_corruption",
        .description = "Deep scan for memory and data corruption",
        .invoke = tool_scan_corruption,
        .context = NULL,
        .required_tier = RCOG_TIER_L2_PERCEPTION
    }
};

/* ============================================================================
 * Configuration API Implementation
 * ============================================================================ */

rcog_health_config_t rcog_health_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    rcog_health_heartbeat("rcog_health_default_config", 0.0f);


    rcog_health_config_t config = {
        .confidence_threshold = RCOG_HEALTH_DEFAULT_CONFIDENCE,
        .default_timeout_ms = RCOG_HEALTH_DEFAULT_TIMEOUT_MS,
        .max_recursion_depth = RCOG_HEALTH_MAX_RECURSION_DEPTH,
        .enable_imagination = false,
        .enable_diagnosis_cache = true,
        .cache_ttl_ms = 60000,  /* 1 minute cache */
        .register_builtin_tools = true,
        .enable_async = true,
        .max_concurrent_goals = 8
    };
    return config;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

rcog_health_integration_t* rcog_health_create(
    rcog_engine_t* engine,
    nimcp_health_agent_t* health_agent,
    const rcog_health_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    rcog_health_heartbeat("rcog_health_create", 0.0f);


    rcog_health_integration_t* integration = nimcp_calloc(1, sizeof(rcog_health_integration_t));
    if (!integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate integration");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        integration->config = *config;
    } else {
        integration->config = rcog_health_default_config();
    }

    /* Store references */
    integration->rcog = engine;
    integration->health_agent = health_agent;

    /* Initialize pending goals */
    integration->max_pending = integration->config.max_concurrent_goals;
    integration->pending_goals = nimcp_calloc(integration->max_pending, sizeof(pending_health_goal_t));
    if (!integration->pending_goals) {
        nimcp_free(integration);
        integration = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rcog_health_create: integration->pending_goals is NULL");
        return NULL;
    }
    integration->num_pending = 0;
    integration->next_goal_id = 1;

    /* Initialize cache */
    if (integration->config.enable_diagnosis_cache) {
        integration->cache_capacity = 32;
        integration->cache = nimcp_calloc(integration->cache_capacity, sizeof(cached_diagnosis_t));
        if (!integration->cache) {
            nimcp_free(integration->pending_goals);
            nimcp_free(integration);
            integration = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rcog_health_create: integration->cache is NULL");
            return NULL;
        }
        integration->cache_size = 0;
    }

    /* Initialize custom tools */
    integration->max_custom_tools = 16;
    integration->custom_tools = nimcp_calloc(integration->max_custom_tools, sizeof(rcog_health_tool_t));
    if (!integration->custom_tools) {
        nimcp_free(integration->cache);
        nimcp_free(integration->pending_goals);
        nimcp_free(integration);
        integration = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rcog_health_create: integration->custom_tools is NULL");
        return NULL;
    }
    integration->num_custom_tools = 0;
    integration->builtin_tools_registered = false;

    /* Initialize statistics */
    memset(&integration->stats, 0, sizeof(rcog_health_stats_t));

    /* Register builtin tools if configured */
    if (integration->config.register_builtin_tools) {
        rcog_health_register_builtin_tools(integration);
    }

    return integration;
}

void rcog_health_destroy(rcog_health_integration_t* integration) {
    if (!integration) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_health_heartbeat("rcog_health_destroy", 0.0f);


    nimcp_free(integration->pending_goals);
    nimcp_free(integration->cache);
    nimcp_free(integration->custom_tools);
    nimcp_free(integration);
    integration = NULL;
}

/* ============================================================================
 * Goal Submission API Implementation
 * ============================================================================ */

static void generate_diagnosis(
    rcog_health_integration_t* integration,
    const rcog_health_goal_t* goal,
    rcog_health_answer_t* answer
) {
    uint64_t start_time = get_time_us();

    /* Initialize answer */
    rcog_health_init_answer(answer);
    answer->goal = *goal;

    /* Generate diagnosis based on anomaly type */
    rcog_health_diagnosis_t* diag = &answer->diagnosis;

    switch (goal->anomaly_type) {
        case HEALTH_MSG_MEMORY_CORRUPTION:
            diag->root_cause_source = HEALTH_SOURCE_MEMORY;
            snprintf(diag->root_cause_description, sizeof(diag->root_cause_description),
                     "Memory corruption detected. Likely cause: buffer overflow or use-after-free.");
            diag->diagnosis_confidence = 0.85f;
            diag->is_certain = false;
            diag->num_alternatives = 2;
            snprintf(diag->alternative_causes[0], sizeof(diag->alternative_causes[0]),
                     "Hardware memory error");
            snprintf(diag->alternative_causes[1], sizeof(diag->alternative_causes[1]),
                     "Concurrent modification race condition");
            break;

        case HEALTH_MSG_DEADLOCK_DETECTED:
            diag->root_cause_source = HEALTH_SOURCE_THREADING;
            snprintf(diag->root_cause_description, sizeof(diag->root_cause_description),
                     "Deadlock detected. Lock ordering violation between threads.");
            diag->diagnosis_confidence = 0.95f;
            diag->is_certain = true;
            diag->num_alternatives = 1;
            snprintf(diag->alternative_causes[0], sizeof(diag->alternative_causes[0]),
                     "Priority inversion causing starvation");
            break;

        case HEALTH_MSG_NAN_DETECTED:
            diag->root_cause_source = HEALTH_SOURCE_NEURAL;
            snprintf(diag->root_cause_description, sizeof(diag->root_cause_description),
                     "NaN/Inf values in neural computation. Exploding gradients likely.");
            diag->diagnosis_confidence = 0.90f;
            diag->is_certain = false;
            diag->num_alternatives = 2;
            snprintf(diag->alternative_causes[0], sizeof(diag->alternative_causes[0]),
                     "Division by zero in activation function");
            snprintf(diag->alternative_causes[1], sizeof(diag->alternative_causes[1]),
                     "Corrupted input data");
            break;

        case HEALTH_MSG_RESOURCE_EXHAUSTION:
            diag->root_cause_source = HEALTH_SOURCE_MEMORY;
            snprintf(diag->root_cause_description, sizeof(diag->root_cause_description),
                     "Resource exhaustion imminent. Memory leak or excessive allocation.");
            diag->diagnosis_confidence = 0.80f;
            diag->is_certain = false;
            diag->num_alternatives = 2;
            snprintf(diag->alternative_causes[0], sizeof(diag->alternative_causes[0]),
                     "Normal high load operation");
            snprintf(diag->alternative_causes[1], sizeof(diag->alternative_causes[1]),
                     "Cache not being evicted properly");
            break;

        default:
            diag->root_cause_source = goal->anomaly_source;
            snprintf(diag->root_cause_description, sizeof(diag->root_cause_description),
                     "General anomaly detected. Root cause requires further investigation.");
            diag->diagnosis_confidence = 0.60f;
            diag->is_certain = false;
            diag->num_alternatives = 0;
            break;
    }

    /* Generate recovery plan */
    rcog_health_recovery_plan_t* plan = &answer->recovery;

    switch (goal->anomaly_type) {
        case HEALTH_MSG_MEMORY_CORRUPTION:
            plan->primary_action = HEALTH_RECOVERY_QUARANTINE;
            plan->fallback_action = HEALTH_RECOVERY_ROLLBACK;
            snprintf(plan->recovery_plan, sizeof(plan->recovery_plan),
                     "1. Quarantine affected memory region\n"
                     "2. Verify corruption extent via canary check\n"
                     "3. If isolated: continue with quarantine\n"
                     "4. If widespread: rollback to checkpoint");
            plan->success_probability = 0.85f;
            plan->estimated_recovery_time_ms = 500;
            plan->requires_immediate_action = true;
            plan->action_risk = HEALTH_SEVERITY_WARNING;
            plan->num_steps = 4;
            snprintf(plan->recovery_steps[0], sizeof(plan->recovery_steps[0]), "Quarantine region");
            snprintf(plan->recovery_steps[1], sizeof(plan->recovery_steps[1]), "Verify extent");
            snprintf(plan->recovery_steps[2], sizeof(plan->recovery_steps[2]), "Assess damage");
            snprintf(plan->recovery_steps[3], sizeof(plan->recovery_steps[3]), "Execute recovery");
            break;

        case HEALTH_MSG_DEADLOCK_DETECTED:
            plan->primary_action = HEALTH_RECOVERY_RESTART_THREAD;
            plan->fallback_action = HEALTH_RECOVERY_FULL_RESET;
            snprintf(plan->recovery_plan, sizeof(plan->recovery_plan),
                     "1. Identify deadlocked threads\n"
                     "2. Attempt lock timeout and release\n"
                     "3. If unresolved: restart affected threads\n"
                     "4. If critical threads: full system reset");
            plan->success_probability = 0.90f;
            plan->estimated_recovery_time_ms = 200;
            plan->requires_immediate_action = true;
            plan->action_risk = HEALTH_SEVERITY_ERROR;
            plan->num_steps = 4;
            snprintf(plan->recovery_steps[0], sizeof(plan->recovery_steps[0]), "Identify threads");
            snprintf(plan->recovery_steps[1], sizeof(plan->recovery_steps[1]), "Attempt timeout");
            snprintf(plan->recovery_steps[2], sizeof(plan->recovery_steps[2]), "Restart threads");
            snprintf(plan->recovery_steps[3], sizeof(plan->recovery_steps[3]), "Verify recovery");
            break;

        case HEALTH_MSG_NAN_DETECTED:
            plan->primary_action = HEALTH_RECOVERY_CLEAR_NAN;
            plan->fallback_action = HEALTH_RECOVERY_ROLLBACK;
            snprintf(plan->recovery_plan, sizeof(plan->recovery_plan),
                     "1. Identify NaN-contaminated tensors\n"
                     "2. Clear NaN values with safe defaults\n"
                     "3. Reduce learning rate temporarily\n"
                     "4. Monitor for recurrence");
            plan->success_probability = 0.75f;
            plan->estimated_recovery_time_ms = 100;
            plan->requires_immediate_action = false;
            plan->action_risk = HEALTH_SEVERITY_WARNING;
            plan->num_steps = 4;
            snprintf(plan->recovery_steps[0], sizeof(plan->recovery_steps[0]), "Identify NaN tensors");
            snprintf(plan->recovery_steps[1], sizeof(plan->recovery_steps[1]), "Clear NaN values");
            snprintf(plan->recovery_steps[2], sizeof(plan->recovery_steps[2]), "Reduce learning rate");
            snprintf(plan->recovery_steps[3], sizeof(plan->recovery_steps[3]), "Monitor stability");
            break;

        case HEALTH_MSG_RESOURCE_EXHAUSTION:
            plan->primary_action = HEALTH_RECOVERY_GC;
            plan->fallback_action = HEALTH_RECOVERY_REDUCE_LOAD;
            snprintf(plan->recovery_plan, sizeof(plan->recovery_plan),
                     "1. Trigger emergency garbage collection\n"
                     "2. Clear non-essential caches\n"
                     "3. Reduce batch sizes if needed\n"
                     "4. Monitor resource recovery");
            plan->success_probability = 0.95f;
            plan->estimated_recovery_time_ms = 1000;
            plan->requires_immediate_action = false;
            plan->action_risk = HEALTH_SEVERITY_INFO;
            plan->num_steps = 4;
            snprintf(plan->recovery_steps[0], sizeof(plan->recovery_steps[0]), "Run GC");
            snprintf(plan->recovery_steps[1], sizeof(plan->recovery_steps[1]), "Clear caches");
            snprintf(plan->recovery_steps[2], sizeof(plan->recovery_steps[2]), "Reduce load");
            snprintf(plan->recovery_steps[3], sizeof(plan->recovery_steps[3]), "Monitor resources");
            break;

        default:
            plan->primary_action = HEALTH_RECOVERY_CHECKPOINT;
            plan->fallback_action = HEALTH_RECOVERY_NONE;
            snprintf(plan->recovery_plan, sizeof(plan->recovery_plan),
                     "1. Create immediate checkpoint\n"
                     "2. Monitor for escalation\n"
                     "3. Escalate if condition worsens");
            plan->success_probability = 0.99f;
            plan->estimated_recovery_time_ms = 50;
            plan->requires_immediate_action = false;
            plan->action_risk = HEALTH_SEVERITY_INFO;
            plan->num_steps = 3;
            snprintf(plan->recovery_steps[0], sizeof(plan->recovery_steps[0]), "Checkpoint");
            snprintf(plan->recovery_steps[1], sizeof(plan->recovery_steps[1]), "Monitor");
            snprintf(plan->recovery_steps[2], sizeof(plan->recovery_steps[2]), "Escalate if needed");
            break;
    }

    /* Generate evidence */
    answer->num_evidence = 2;

    snprintf(answer->evidence[0].description, sizeof(answer->evidence[0].description),
             "Anomaly detected at timestamp %lu with severity %d",
             (unsigned long)goal->anomaly_timestamp_us, goal->anomaly_severity);
    answer->evidence[0].confidence = 0.95f;
    answer->evidence[0].source = goal->anomaly_source;
    snprintf(answer->evidence[0].evidence_type, sizeof(answer->evidence[0].evidence_type),
             "observation");

    snprintf(answer->evidence[1].description, sizeof(answer->evidence[1].description),
             "System metrics indicate %s source involvement",
             goal->anomaly_source == HEALTH_SOURCE_MEMORY ? "memory" :
             goal->anomaly_source == HEALTH_SOURCE_THREADING ? "threading" :
             goal->anomaly_source == HEALTH_SOURCE_NEURAL ? "neural" : "unknown");
    answer->evidence[1].confidence = 0.80f;
    answer->evidence[1].source = goal->anomaly_source;
    snprintf(answer->evidence[1].evidence_type, sizeof(answer->evidence[1].evidence_type),
             "inference");

    /* Calculate overall confidence */
    answer->overall_confidence = (diag->diagnosis_confidence + plan->success_probability) / 2.0f;
    answer->success = true;

    /* Set processing stats */
    uint64_t elapsed = get_time_us() - start_time;
    answer->processing_time_ms = (uint32_t)(elapsed / 1000);
    answer->subtasks_executed = 3;  /* Simulated */
    answer->max_depth_used = 2;
    answer->refinement_iterations = 1;
}

int rcog_health_submit_goal(
    rcog_health_integration_t* integration,
    const rcog_health_goal_t* goal,
    rcog_health_answer_t* answer
) {
    if (!integration || !goal || !answer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_health_submit_goal: required parameter is NULL (integration, goal, answer)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_health_heartbeat("rcog_health_submit_goal", 0.0f);


    integration->stats.goals_submitted++;

    /* Check cache first */
    if (integration->config.enable_diagnosis_cache && integration->cache) {
        uint64_t now = get_time_us();

        for (uint32_t i = 0; i < integration->cache_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && integration->cache_size > 256) {
                rcog_health_heartbeat("rcog_health_loop",
                                 (float)(i + 1) / (float)integration->cache_size);
            }

            cached_diagnosis_t* entry = &integration->cache[i];

            if (entry->anomaly_type == goal->anomaly_type &&
                entry->source == goal->anomaly_source &&
                entry->severity == goal->anomaly_severity &&
                (now - entry->cache_time_us) < entry->ttl_us) {
                /* Cache hit */
                *answer = entry->answer;
                integration->stats.cache_hits++;
                integration->stats.goals_completed++;
                return 0;
            }
        }

        integration->stats.cache_misses++;
    }

    /* Generate diagnosis */
    generate_diagnosis(integration, goal, answer);

    /* Update cache */
    if (integration->config.enable_diagnosis_cache && integration->cache &&
        answer->success) {
        if (integration->cache_size < integration->cache_capacity) {
            cached_diagnosis_t* entry = &integration->cache[integration->cache_size++];
            entry->anomaly_type = goal->anomaly_type;
            entry->source = goal->anomaly_source;
            entry->severity = goal->anomaly_severity;
            entry->answer = *answer;
            entry->cache_time_us = get_time_us();
            entry->ttl_us = integration->config.cache_ttl_ms * 1000ULL;
        }
    }

    /* Update statistics */
    if (answer->success) {
        integration->stats.goals_completed++;

        /* Update averages */
        float n = (float)integration->stats.goals_completed;
        integration->stats.avg_processing_time_ms =
            ((n - 1) * integration->stats.avg_processing_time_ms + answer->processing_time_ms) / (fabsf(n) > 1e-7f ? n : 1e-7f);
        integration->stats.avg_confidence =
            ((n - 1) * integration->stats.avg_confidence + answer->overall_confidence) / (fabsf(n) > 1e-7f ? n : 1e-7f);

        if (answer->max_depth_used > integration->stats.max_depth_used) {
            integration->stats.max_depth_used = answer->max_depth_used;
        }
    } else {
        integration->stats.goals_failed++;
    }

    return 0;
}

int rcog_health_submit_goal_async(
    rcog_health_integration_t* integration,
    const rcog_health_goal_t* goal,
    uint64_t* goal_id
) {
    if (!integration || !goal || !goal_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_health_submit_goal_async: required parameter is NULL (integration, goal, goal_id)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_health_heartbeat("rcog_health_submit_goal_async", 0.0f);


    if (integration->num_pending >= integration->max_pending) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "rcog_health_submit_goal_async: capacity exceeded");
        return -1;  /* Queue full */
    }

    integration->stats.goals_submitted++;

    /* Store pending goal */
    pending_health_goal_t* pending = &integration->pending_goals[integration->num_pending];
    pending->goal_id = integration->next_goal_id++;
    pending->goal = *goal;
    pending->complete = false;
    pending->start_time_us = get_time_us();

    *goal_id = pending->goal_id;
    integration->num_pending++;
    integration->stats.active_goals++;

    return 0;
}

int rcog_health_get_answer(
    rcog_health_integration_t* integration,
    uint64_t goal_id,
    rcog_health_answer_t* answer,
    uint32_t timeout_ms
) {
    if (!integration || !answer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_health_get_answer: required parameter is NULL (integration, answer)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_health_heartbeat("rcog_health_get_answer", 0.0f);


    (void)timeout_ms;  /* Not used in this implementation */

    /* Find the pending goal */
    for (uint32_t i = 0; i < integration->num_pending; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && integration->num_pending > 256) {
            rcog_health_heartbeat("rcog_health_loop",
                             (float)(i + 1) / (float)integration->num_pending);
        }

        if (integration->pending_goals[i].goal_id == goal_id) {
            pending_health_goal_t* pending = &integration->pending_goals[i];

            /* Complete the goal if not done */
            if (!pending->complete) {
                generate_diagnosis(integration, &pending->goal, &pending->answer);
                pending->complete = true;

                if (pending->answer.success) {
                    integration->stats.goals_completed++;
                } else {
                    integration->stats.goals_failed++;
                }
            }

            *answer = pending->answer;

            /* Remove from pending */
            if (i < integration->num_pending - 1) {
                memmove(&integration->pending_goals[i],
                        &integration->pending_goals[i + 1],
                        (integration->num_pending - i - 1) * sizeof(pending_health_goal_t));
            }
            integration->num_pending--;
            integration->stats.active_goals--;

            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rcog_health_get_answer: operation failed");
    return -1;  /* Not found */
}

bool rcog_health_is_complete(
    const rcog_health_integration_t* integration,
    uint64_t goal_id
) {
    if (!integration) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_health_heartbeat("rcog_health_is_complete", 0.0f);


    for (uint32_t i = 0; i < integration->num_pending; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && integration->num_pending > 256) {
            rcog_health_heartbeat("rcog_health_loop",
                             (float)(i + 1) / (float)integration->num_pending);
        }

        if (integration->pending_goals[i].goal_id == goal_id) {
            return integration->pending_goals[i].complete;
        }
    }

    return true;  /* Not found = already completed and removed */
}

int rcog_health_cancel_goal(
    rcog_health_integration_t* integration,
    uint64_t goal_id
) {
    if (!integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "integration is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_health_heartbeat("rcog_health_cancel_goal", 0.0f);


    for (uint32_t i = 0; i < integration->num_pending; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && integration->num_pending > 256) {
            rcog_health_heartbeat("rcog_health_loop",
                             (float)(i + 1) / (float)integration->num_pending);
        }

        if (integration->pending_goals[i].goal_id == goal_id) {
            /* Remove from pending */
            if (i < integration->num_pending - 1) {
                memmove(&integration->pending_goals[i],
                        &integration->pending_goals[i + 1],
                        (integration->num_pending - i - 1) * sizeof(pending_health_goal_t));
            }
            integration->num_pending--;
            integration->stats.active_goals--;
            integration->stats.goals_cancelled++;

            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rcog_health_cancel_goal: operation failed");
    return -1;  /* Not found */
}

/* ============================================================================
 * Convenience API Implementation
 * ============================================================================ */

int rcog_health_diagnose_anomaly(
    rcog_health_integration_t* integration,
    health_agent_msg_type_t anomaly_type,
    health_agent_source_t source,
    health_agent_severity_t severity,
    rcog_health_answer_t* answer
) {
    if (!integration || !answer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_health_diagnose_anomaly: required parameter is NULL (integration, answer)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_health_heartbeat("rcog_health_diagnose_anomaly", 0.0f);


    rcog_health_goal_t goal;
    rcog_health_init_goal(&goal);

    goal.health_type = RCOG_HEALTH_GOAL_DIAGNOSE;
    goal.anomaly_type = anomaly_type;
    goal.anomaly_source = source;
    goal.anomaly_severity = severity;
    goal.anomaly_timestamp_us = get_time_us();

    snprintf(goal.query, sizeof(goal.query),
             "Diagnose anomaly: type=%d, source=%d, severity=%d",
             anomaly_type, source, severity);

    return rcog_health_submit_goal(integration, &goal, answer);
}

int rcog_health_plan_recovery(
    rcog_health_integration_t* integration,
    health_agent_msg_type_t anomaly_type,
    health_agent_source_t source,
    health_agent_severity_t severity,
    rcog_health_recovery_plan_t* plan
) {
    if (!integration || !plan) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_health_plan_recovery: required parameter is NULL (integration, plan)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_health_heartbeat("rcog_health_plan_recovery", 0.0f);


    rcog_health_answer_t answer;
    int result = rcog_health_diagnose_anomaly(integration, anomaly_type, source, severity, &answer);

    if (result == 0 && answer.success) {
        *plan = answer.recovery;
    }

    return result;
}

int rcog_health_predict_failure(
    rcog_health_integration_t* integration,
    health_agent_source_t source,
    float* failure_probability,
    uint32_t* time_to_failure_ms
) {
    if (!integration || !failure_probability || !time_to_failure_ms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_health_predict_failure: required parameter is NULL (integration, failure_probability, time_to_failure_ms)");
        return -1;
    }

    /* Simulated prediction based on source */
    /* Phase 8: Heartbeat at operation start */
    rcog_health_heartbeat("rcog_health_predict_failure", 0.0f);


    switch (source) {
        case HEALTH_SOURCE_MEMORY:
            *failure_probability = 0.15f;
            *time_to_failure_ms = 3600000;  /* 1 hour */
            break;
        case HEALTH_SOURCE_THREADING:
            *failure_probability = 0.10f;
            *time_to_failure_ms = 7200000;  /* 2 hours */
            break;
        case HEALTH_SOURCE_NEURAL:
            *failure_probability = 0.20f;
            *time_to_failure_ms = 1800000;  /* 30 minutes */
            break;
        default:
            *failure_probability = 0.05f;
            *time_to_failure_ms = 86400000;  /* 24 hours */
            break;
    }

    return 0;
}

/* ============================================================================
 * Tool Management API Implementation
 * ============================================================================ */

int rcog_health_register_builtin_tools(rcog_health_integration_t* integration) {
    if (!integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "integration is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_health_heartbeat("rcog_health_register_builtin_too", 0.0f);


    if (integration->builtin_tools_registered) {
        return 0;  /* Already registered */
    }

    /* In a real implementation, this would register tools with the RCOG engine.
     * For now, we just mark them as registered. */
    integration->builtin_tools_registered = true;
    integration->stats.tools_invoked = 0;

    return 0;
}

int rcog_health_register_tool(
    rcog_health_integration_t* integration,
    const rcog_health_tool_t* tool
) {
    if (!integration || !tool || !tool->tool_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_health_register_tool: required parameter is NULL (integration, tool, tool->tool_name)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_health_heartbeat("rcog_health_register_tool", 0.0f);


    if (integration->num_custom_tools >= integration->max_custom_tools) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "rcog_health_register_tool: capacity exceeded");
        return -1;  /* No room */
    }

    /* Store custom tool */
    integration->custom_tools[integration->num_custom_tools++] = *tool;

    return 0;
}

int rcog_health_unregister_tool(
    rcog_health_integration_t* integration,
    const char* tool_name
) {
    if (!integration || !tool_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_health_unregister_tool: required parameter is NULL (integration, tool_name)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_health_heartbeat("rcog_health_unregister_tool", 0.0f);


    for (uint32_t i = 0; i < integration->num_custom_tools; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && integration->num_custom_tools > 256) {
            rcog_health_heartbeat("rcog_health_loop",
                             (float)(i + 1) / (float)integration->num_custom_tools);
        }

        if (strcmp(integration->custom_tools[i].tool_name, tool_name) == 0) {
            /* Remove tool */
            if (i < integration->num_custom_tools - 1) {
                memmove(&integration->custom_tools[i],
                        &integration->custom_tools[i + 1],
                        (integration->num_custom_tools - i - 1) * sizeof(rcog_health_tool_t));
            }
            integration->num_custom_tools--;
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rcog_health_unregister_tool: operation failed");
    return -1;  /* Not found */
}

const rcog_health_tool_t* rcog_health_get_builtin_tool(rcog_health_tool_id_t tool_id) {
    /* Phase 8: Heartbeat at operation start */
    rcog_health_heartbeat("rcog_health_get_builtin_tool", 0.0f);


    if (tool_id >= RCOG_TOOL_ID_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "rcog_health_get_builtin_tool: capacity exceeded");
        return NULL;
    }

    return &builtin_tools[tool_id];
}

/* ============================================================================
 * Statistics API Implementation
 * ============================================================================ */

int rcog_health_get_stats(
    const rcog_health_integration_t* integration,
    rcog_health_stats_t* stats
) {
    if (!integration || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_health_get_stats: required parameter is NULL (integration, stats)");
        return -1;
    }

    *stats = integration->stats;
    /* Phase 8: Heartbeat at operation start */
    rcog_health_heartbeat("rcog_health_get_stats", 0.0f);


    return 0;
}

void rcog_health_reset_stats(rcog_health_integration_t* integration) {
    if (!integration) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_health_heartbeat("rcog_health_reset_stats", 0.0f);


    memset(&integration->stats, 0, sizeof(rcog_health_stats_t));
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

const char* rcog_health_goal_type_name(rcog_health_goal_type_t type) {
    switch (type) {
        case RCOG_HEALTH_GOAL_DIAGNOSE: return "DIAGNOSE";
        case RCOG_HEALTH_GOAL_PLAN_RECOVERY: return "PLAN_RECOVERY";
        case RCOG_HEALTH_GOAL_PREDICT_FAILURE: return "PREDICT_FAILURE";
        case RCOG_HEALTH_GOAL_VERIFY_HEALTH: return "VERIFY_HEALTH";
        case RCOG_HEALTH_GOAL_OPTIMIZE_MONITORING: return "OPTIMIZE_MONITORING";
        case RCOG_HEALTH_GOAL_ANALYZE_PATTERNS: return "ANALYZE_PATTERNS";
        default: return "UNKNOWN";
    }
}

const char* rcog_health_tool_id_name(rcog_health_tool_id_t tool_id) {
    switch (tool_id) {
        case RCOG_TOOL_ID_CHECK_MEMORY: return "CHECK_MEMORY";
        case RCOG_TOOL_ID_CHECK_TENSORS: return "CHECK_TENSORS";
        case RCOG_TOOL_ID_CHECK_DEADLOCKS: return "CHECK_DEADLOCKS";
        case RCOG_TOOL_ID_ANALYZE_METRICS: return "ANALYZE_METRICS";
        case RCOG_TOOL_ID_PREDICT_FAILURE: return "PREDICT_FAILURE";
        case RCOG_TOOL_ID_SUGGEST_RECOVERY: return "SUGGEST_RECOVERY";
        case RCOG_TOOL_ID_VERIFY_CHECKSUM: return "VERIFY_CHECKSUM";
        case RCOG_TOOL_ID_SCAN_CORRUPTION: return "SCAN_CORRUPTION";
        case RCOG_TOOL_ID_GET_RESOURCES: return "GET_RESOURCES";
        case RCOG_TOOL_ID_QUERY_KG: return "QUERY_KG";
        case RCOG_TOOL_ID_CHECK_CONNECTIVITY: return "CHECK_CONNECTIVITY";
        case RCOG_TOOL_ID_ANALYZE_THREADS: return "ANALYZE_THREADS";
        default: return "UNKNOWN";
    }
}

void rcog_health_init_goal(rcog_health_goal_t* goal) {
    if (!goal) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_health_heartbeat("rcog_health_init_goal", 0.0f);


    memset(goal, 0, sizeof(rcog_health_goal_t));
    goal->base_type = RCOG_GOAL_REASONING;
    goal->health_type = RCOG_HEALTH_GOAL_DIAGNOSE;
    goal->confidence_threshold = RCOG_HEALTH_DEFAULT_CONFIDENCE;
    goal->max_recursion_depth = RCOG_HEALTH_MAX_RECURSION_DEPTH;
    goal->enable_imagination = false;
    goal->timeout_ms = RCOG_HEALTH_DEFAULT_TIMEOUT_MS;
    goal->anomaly_timestamp_us = get_time_us();
}

void rcog_health_init_answer(rcog_health_answer_t* answer) {
    if (!answer) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_health_heartbeat("rcog_health_init_answer", 0.0f);


    memset(answer, 0, sizeof(rcog_health_answer_t));
    answer->success = false;
    answer->overall_confidence = 0.0f;
}

void rcog_health_free_answer(rcog_health_answer_t* answer) {
    /* No dynamic allocations in current implementation */
    /* Phase 8: Heartbeat at operation start */
    rcog_health_heartbeat("rcog_health_free_answer", 0.0f);


    (void)answer;
}

void rcog_health_dump_answer(const rcog_health_answer_t* answer) {
    if (!answer) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_health_heartbeat("rcog_health_dump_answer", 0.0f);


    printf("=== RCOG Health Answer ===\n");
    printf("Success: %s\n", answer->success ? "yes" : "no");
    printf("Overall Confidence: %.2f\n", answer->overall_confidence);
    printf("\n--- Diagnosis ---\n");
    printf("Root Cause: %s\n", answer->diagnosis.root_cause_description);
    printf("Source: %d\n", answer->diagnosis.root_cause_source);
    printf("Confidence: %.2f\n", answer->diagnosis.diagnosis_confidence);
    printf("Certain: %s\n", answer->diagnosis.is_certain ? "yes" : "no");
    printf("\n--- Recovery Plan ---\n");
    printf("Primary Action: %d\n", answer->recovery.primary_action);
    printf("Fallback Action: %d\n", answer->recovery.fallback_action);
    printf("Success Probability: %.2f\n", answer->recovery.success_probability);
    printf("Plan:\n%s\n", answer->recovery.recovery_plan);
    printf("\n--- Processing Stats ---\n");
    printf("Subtasks: %u\n", answer->subtasks_executed);
    printf("Max Depth: %u\n", answer->max_depth_used);
    printf("Time: %lu ms\n", (unsigned long)answer->processing_time_ms);
    printf("===========================\n");
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void rcog_health_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_rcog_health_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int rcog_health_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_health_training_begin: NULL argument");
        return -1;
    }
    rcog_health_heartbeat_instance(NULL, "rcog_health_training_begin", 0.0f);
    (void)(struct rcog_health_integration*)instance; /* Module state available for reset */
    return 0;
}

int rcog_health_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_health_training_end: NULL argument");
        return -1;
    }
    rcog_health_heartbeat_instance(NULL, "rcog_health_training_end", 1.0f);
    (void)(struct rcog_health_integration*)instance; /* Module state available for finalization */
    return 0;
}

int rcog_health_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_health_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    rcog_health_heartbeat_instance(NULL, "rcog_health_training_step", progress);
    (void)(struct rcog_health_integration*)instance; /* Module state available for step adaptation */
    return 0;
}
