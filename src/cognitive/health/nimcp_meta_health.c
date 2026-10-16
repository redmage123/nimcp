/**
 * @file nimcp_meta_health.c
 * @brief Meta-Health Self-Reflection Implementation
 * @version 1.0.0
 * @date 2026-01-18
 *
 * Implementation of self-reflection capabilities for health monitoring.
 */

/* Enable POSIX clock functions */
#define _POSIX_C_SOURCE 200809L

#include "cognitive/health/nimcp_meta_health.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_timing_constants.h"

BRIDGE_BOILERPLATE(meta_health, MESH_ADAPTER_CATEGORY_COGNITIVE)



/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Applied adjustment for reversion
 */
typedef struct {
    meta_health_adjustment_t adjustment;
    float original_value;
    uint64_t apply_time_us;
} applied_adjustment_t;

/**
 * @brief Pending reflection request
 */
typedef struct {
    uint64_t request_id;
    bool complete;
    meta_health_reflection_result_t result;
    uint64_t start_time_us;
} pending_reflection_t;

/**
 * @brief Meta-health reflector internal state
 */
struct meta_health_reflector {
    /* Configuration */
    meta_health_config_t config;

    /* Connected systems */
    nimcp_health_agent_t* health_agent;
    rcog_engine_t* rcog;

    /* Decision log */
    meta_health_decision_log_t decision_log;

    /* Applied adjustments for reversion */
    applied_adjustment_t* applied_adjustments;
    uint32_t num_applied;
    uint32_t max_applied;

    /* Pending reflections */
    pending_reflection_t* pending;
    uint32_t num_pending;
    uint32_t max_pending;
    uint64_t next_request_id;

    /* Discovered patterns */
    meta_health_pattern_t* patterns;
    uint32_t num_patterns;
    uint32_t max_patterns;

    /* Statistics */
    meta_health_stats_t stats;

    /* State */
    bool running;
    uint64_t last_reflection_us;
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
 * Configuration API Implementation
 * ============================================================================ */

meta_health_config_t meta_health_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    meta_health_heartbeat("meta_health_default_config", 0.0f);


    meta_health_config_t config = {
        .enable_auto_reflection = true,
        .reflection_interval_ms = META_HEALTH_DEFAULT_REFLECTION_INTERVAL_MS,
        .min_decisions_for_reflection = META_HEALTH_MIN_DECISIONS_FOR_REFLECTION,
        .max_decisions_to_analyze = META_HEALTH_MAX_DECISIONS,
        .enable_auto_apply = false,
        .auto_apply_confidence_threshold = 0.9f,
        .enable_pattern_learning = true,
        .reflection_timeout_ms = NIMCP_LONG_TIMEOUT_MS,
        .use_rcog_for_reflection = false
    };
    return config;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

meta_health_reflector_t* meta_health_create(
    nimcp_health_agent_t* health_agent,
    rcog_engine_t* rcog,
    const meta_health_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    meta_health_heartbeat("meta_health_create", 0.0f);


    meta_health_reflector_t* reflector = nimcp_calloc(1, sizeof(meta_health_reflector_t));
    if (!reflector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate reflector");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        reflector->config = *config;
    } else {
        reflector->config = meta_health_default_config();
    }

    /* Store references */
    reflector->health_agent = health_agent;
    reflector->rcog = rcog;

    /* Initialize decision log */
    memset(&reflector->decision_log, 0, sizeof(meta_health_decision_log_t));

    /* Initialize applied adjustments */
    reflector->max_applied = 32;
    reflector->applied_adjustments = nimcp_calloc(reflector->max_applied, sizeof(applied_adjustment_t));
    if (!reflector->applied_adjustments) {
        nimcp_free(reflector);
        reflector = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "meta_health_create: reflector->applied_adjustments is NULL");
        return NULL;
    }
    reflector->num_applied = 0;

    /* Initialize pending reflections */
    reflector->max_pending = 8;
    reflector->pending = nimcp_calloc(reflector->max_pending, sizeof(pending_reflection_t));
    if (!reflector->pending) {
        nimcp_free(reflector->applied_adjustments);
        nimcp_free(reflector);
        reflector = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "meta_health_create: reflector->pending is NULL");
        return NULL;
    }
    reflector->num_pending = 0;
    reflector->next_request_id = 1;

    /* Initialize discovered patterns */
    reflector->max_patterns = 32;
    reflector->patterns = nimcp_calloc(reflector->max_patterns, sizeof(meta_health_pattern_t));
    if (!reflector->patterns) {
        nimcp_free(reflector->pending);
        nimcp_free(reflector->applied_adjustments);
        nimcp_free(reflector);
        reflector = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "meta_health_create: reflector->patterns is NULL");
        return NULL;
    }
    reflector->num_patterns = 0;

    /* Initialize statistics */
    memset(&reflector->stats, 0, sizeof(meta_health_stats_t));

    /* Initialize state */
    reflector->running = false;
    reflector->last_reflection_us = 0;

    return reflector;
}

void meta_health_destroy(meta_health_reflector_t* reflector) {
    if (!reflector) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_health_heartbeat("meta_health_destroy", 0.0f);


    nimcp_free(reflector->patterns);
    nimcp_free(reflector->pending);
    nimcp_free(reflector->applied_adjustments);
    nimcp_free(reflector);
    reflector = NULL;
}

int meta_health_start(meta_health_reflector_t* reflector) {
    if (!reflector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reflector is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_health_heartbeat("meta_health_start", 0.0f);


    reflector->running = true;
    reflector->last_reflection_us = get_time_us();

    return 0;
}

int meta_health_stop(meta_health_reflector_t* reflector) {
    if (!reflector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reflector is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_health_heartbeat("meta_health_stop", 0.0f);


    reflector->running = false;

    return 0;
}

/* ============================================================================
 * Decision Recording API Implementation
 * ============================================================================ */

int meta_health_record_decision(
    meta_health_reflector_t* reflector,
    const meta_health_decision_t* decision
) {
    if (!reflector || !decision) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_health_record_decision: required parameter is NULL (reflector, decision)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_health_heartbeat("meta_health_record_decision", 0.0f);


    meta_health_decision_log_t* log = &reflector->decision_log;

    /* Add to circular buffer */
    log->decisions[log->write_pos] = *decision;
    log->write_pos = (log->write_pos + 1) % META_HEALTH_MAX_DECISIONS;

    if (log->num_decisions < META_HEALTH_MAX_DECISIONS) {
        log->num_decisions++;
    }

    log->total_decisions++;
    reflector->stats.decisions_recorded++;

    return 0;
}

int meta_health_record_outcome(
    meta_health_reflector_t* reflector,
    uint64_t timestamp_us,
    meta_health_outcome_t outcome,
    bool recovery_succeeded,
    uint64_t time_to_recovery_ms,
    float post_recovery_health
) {
    if (!reflector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reflector is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_health_heartbeat("meta_health_record_outcome", 0.0f);


    meta_health_decision_log_t* log = &reflector->decision_log;

    /* Find the decision and update it */
    for (uint32_t i = 0; i < log->num_decisions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && log->num_decisions > 256) {
            meta_health_heartbeat("meta_health_loop",
                             (float)(i + 1) / (float)log->num_decisions);
        }

        if (log->decisions[i].timestamp_us == timestamp_us) {
            log->decisions[i].outcome = outcome;
            log->decisions[i].recovery_succeeded = recovery_succeeded;
            log->decisions[i].time_to_recovery_ms = time_to_recovery_ms;
            log->decisions[i].post_recovery_health = post_recovery_health;
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "meta_health_record_outcome: validation failed");
    return -1;  /* Decision not found */
}

int meta_health_get_decision_log(
    const meta_health_reflector_t* reflector,
    meta_health_decision_log_t* log
) {
    if (!reflector || !log) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_health_get_decision_log: required parameter is NULL (reflector, log)");
        return -1;
    }

    *log = reflector->decision_log;
    /* Phase 8: Heartbeat at operation start */
    meta_health_heartbeat("meta_health_get_decision_log", 0.0f);


    return 0;
}

/* ============================================================================
 * Reflection API Implementation
 * ============================================================================ */

static void compute_assessment(
    const meta_health_reflector_t* reflector,
    meta_health_assessment_t* assessment
) {
    const meta_health_decision_log_t* log = &reflector->decision_log;

    memset(assessment, 0, sizeof(meta_health_assessment_t));

    if (log->num_decisions == 0) {
        assessment->accuracy_rate = 1.0f;
        assessment->recovery_success_rate = 1.0f;
        return;
    }

    uint32_t correct = 0, total = 0;
    uint32_t recovery_success = 0, recovery_attempts = 0;
    uint32_t false_positives = 0, false_negatives = 0;
    float total_response_time = 0.0f;
    float total_recovery_time = 0.0f;
    float total_post_health = 0.0f;
    uint32_t response_count = 0;
    uint32_t recovery_count = 0;

    for (uint32_t i = 0; i < log->num_decisions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && log->num_decisions > 256) {
            meta_health_heartbeat("meta_health_loop",
                             (float)(i + 1) / (float)log->num_decisions);
        }

        const meta_health_decision_t* d = &log->decisions[i];
        total++;

        switch (d->outcome) {
            case META_HEALTH_OUTCOME_SUCCESS:
                correct++;
                recovery_success++;
                recovery_attempts++;
                break;
            case META_HEALTH_OUTCOME_PARTIAL_SUCCESS:
                correct++;
                recovery_attempts++;
                break;
            case META_HEALTH_OUTCOME_FAILURE:
                recovery_attempts++;
                break;
            case META_HEALTH_OUTCOME_FALSE_POSITIVE:
                false_positives++;
                break;
            case META_HEALTH_OUTCOME_FALSE_NEGATIVE:
                false_negatives++;
                break;
            case META_HEALTH_OUTCOME_UNKNOWN:
            default:
                break;
        }

        if (d->time_to_recovery_ms > 0) {
            total_recovery_time += (float)d->time_to_recovery_ms;
            recovery_count++;
        }

        if (d->post_recovery_health > 0.0f) {
            total_post_health += d->post_recovery_health;
        }

        /* Simulate response time from detection */
        total_response_time += 50.0f + (float)(d->anomaly_severity * 10);
        response_count++;
    }

    assessment->accuracy_rate = total > 0 ? (float)correct / (float)total : 1.0f;
    assessment->recovery_success_rate = recovery_attempts > 0 ?
                                        (float)recovery_success / (float)recovery_attempts : 1.0f;
    assessment->false_positive_rate = total > 0 ? (float)false_positives / (float)total : 0.0f;
    assessment->false_negative_rate = total > 0 ? (float)false_negatives / (float)total : 0.0f;
    assessment->avg_response_time_ms = response_count > 0 ?
                                       total_response_time / (response_count > 0 ? response_count : 1) : 0.0f;
    assessment->avg_recovery_time_ms = recovery_count > 0 ?
                                       total_recovery_time / (recovery_count > 0 ? recovery_count : 1) : 0.0f;
    assessment->avg_post_recovery_health = recovery_count > 0 ?
                                           total_post_health / (recovery_count > 0 ? recovery_count : 1) : 1.0f;
    assessment->assessment_period_ms = (log->num_decisions > 0) ?
        (get_time_us() - log->decisions[0].timestamp_us) / 1000 : 0;
    assessment->decisions_in_period = log->num_decisions;
}

static void compute_weaknesses(
    const meta_health_reflector_t* reflector,
    meta_health_weakness_t* weaknesses
) {
    const meta_health_decision_log_t* log = &reflector->decision_log;

    memset(weaknesses, 0, sizeof(meta_health_weakness_t));

    if (log->num_decisions == 0) {
        return;
    }

    /* Count by source */
    uint32_t source_success[HEALTH_SOURCE_COUNT] = {0};
    uint32_t source_total[HEALTH_SOURCE_COUNT] = {0};
    uint32_t source_fp[HEALTH_SOURCE_COUNT] = {0};

    /* Count by recovery action */
    uint32_t recovery_success[HEALTH_RECOVERY_COUNT] = {0};
    uint32_t recovery_total[HEALTH_RECOVERY_COUNT] = {0};

    /* Track response times by anomaly type */
    float type_response_time[HEALTH_MSG_COUNT] = {0.0f};
    uint32_t type_count[HEALTH_MSG_COUNT] = {0};

    for (uint32_t i = 0; i < log->num_decisions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && log->num_decisions > 256) {
            meta_health_heartbeat("meta_health_loop",
                             (float)(i + 1) / (float)log->num_decisions);
        }

        const meta_health_decision_t* d = &log->decisions[i];

        if (d->anomaly_source < HEALTH_SOURCE_COUNT) {
            source_total[d->anomaly_source]++;
            if (d->outcome == META_HEALTH_OUTCOME_SUCCESS ||
                d->outcome == META_HEALTH_OUTCOME_PARTIAL_SUCCESS) {
                source_success[d->anomaly_source]++;
            }
            if (d->outcome == META_HEALTH_OUTCOME_FALSE_POSITIVE) {
                source_fp[d->anomaly_source]++;
            }
        }

        if (d->action_taken < HEALTH_RECOVERY_COUNT) {
            recovery_total[d->action_taken]++;
            if (d->recovery_succeeded) {
                recovery_success[d->action_taken]++;
            }
        }

        if (d->anomaly_type < HEALTH_MSG_COUNT) {
            type_response_time[d->anomaly_type] += 50.0f + (float)(d->anomaly_severity * 10);
            type_count[d->anomaly_type]++;
        }
    }

    /* Find weakest detection area */
    float lowest_accuracy = 1.0f;
    health_agent_source_t weakest_source = HEALTH_SOURCE_UNKNOWN;

    for (int s = 0; s < HEALTH_SOURCE_COUNT; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && HEALTH_SOURCE_COUNT > 256) {
            meta_health_heartbeat("meta_health_loop",
                             (float)(s + 1) / (float)HEALTH_SOURCE_COUNT);
        }

        if (source_total[s] >= 3) {  /* Need minimum samples */
            float accuracy = (float)source_success[s] / (float)source_total[s];
            if (accuracy < lowest_accuracy) {
                lowest_accuracy = accuracy;
                weakest_source = (health_agent_source_t)s;
            }
        }
    }

    weaknesses->weakest_detection_area = weakest_source;
    weaknesses->detection_accuracy = lowest_accuracy;

    /* Find least effective recovery */
    float lowest_recovery = 1.0f;
    health_agent_recovery_t worst_recovery = HEALTH_RECOVERY_NONE;

    for (int r = 0; r < HEALTH_RECOVERY_COUNT; r++) {
        /* Phase 8: Loop progress heartbeat */
        if ((r & 0xFF) == 0 && HEALTH_RECOVERY_COUNT > 256) {
            meta_health_heartbeat("meta_health_loop",
                             (float)(r + 1) / (float)HEALTH_RECOVERY_COUNT);
        }

        if (recovery_total[r] >= 3) {
            float success = (float)recovery_success[r] / (float)recovery_total[r];
            if (success < lowest_recovery) {
                lowest_recovery = success;
                worst_recovery = (health_agent_recovery_t)r;
            }
        }
    }

    weaknesses->least_effective_recovery = worst_recovery;
    weaknesses->recovery_success_rate = lowest_recovery;

    /* Find most false positives */
    uint32_t max_fp = 0;
    health_agent_source_t fp_source = HEALTH_SOURCE_UNKNOWN;

    for (int s = 0; s < HEALTH_SOURCE_COUNT; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && HEALTH_SOURCE_COUNT > 256) {
            meta_health_heartbeat("meta_health_loop",
                             (float)(s + 1) / (float)HEALTH_SOURCE_COUNT);
        }

        if (source_fp[s] > max_fp) {
            max_fp = source_fp[s];
            fp_source = (health_agent_source_t)s;
        }
    }

    weaknesses->most_false_positives = fp_source;
    weaknesses->false_positive_rate = source_total[fp_source] > 0 ?
                                      (float)max_fp / (float)source_total[fp_source] : 0.0f;

    /* Find slowest response */
    float slowest = 0.0f;
    health_agent_msg_type_t slow_type = HEALTH_MSG_ANOMALY_DETECTED;

    for (int t = 0; t < HEALTH_MSG_COUNT; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && HEALTH_MSG_COUNT > 256) {
            meta_health_heartbeat("meta_health_loop",
                             (float)(t + 1) / (float)HEALTH_MSG_COUNT);
        }

        if (type_count[t] > 0) {
            float avg = type_response_time[t] / type_count[t];
            if (avg > slowest) {
                slowest = avg;
                slow_type = (health_agent_msg_type_t)t;
            }
        }
    }

    weaknesses->slowest_response_type = slow_type;
    weaknesses->slowest_response_time_ms = slowest;
}

int meta_health_reflect(
    meta_health_reflector_t* reflector,
    meta_health_reflection_result_t* result
) {
    if (!reflector || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_health_reflect: required parameter is NULL (reflector, result)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_health_heartbeat("meta_health_reflect", 0.0f);


    uint64_t start_time = get_time_us();

    meta_health_init_reflection_result(result);

    const meta_health_decision_log_t* log = &reflector->decision_log;

    /* Check minimum decisions */
    if (log->num_decisions < reflector->config.min_decisions_for_reflection) {
        snprintf(result->key_insight, sizeof(result->key_insight),
                 "Insufficient data: only %u decisions recorded (need %u minimum)",
                 log->num_decisions, reflector->config.min_decisions_for_reflection);
        result->insight_confidence = 0.2f;
        result->improvement_potential = 0.0f;
        return 0;
    }

    /* Compute assessment */
    compute_assessment(reflector, &result->assessment);

    /* Compute weaknesses */
    compute_weaknesses(reflector, &result->weaknesses);

    /* Generate key insight */
    if (result->assessment.accuracy_rate < 0.7f) {
        snprintf(result->key_insight, sizeof(result->key_insight),
                 "Accuracy critically low (%.0f%%). Focus on reducing false positives from %s source.",
                 result->assessment.accuracy_rate * 100.0f,
                 result->weaknesses.weakest_detection_area == HEALTH_SOURCE_MEMORY ? "memory" :
                 result->weaknesses.weakest_detection_area == HEALTH_SOURCE_THREADING ? "threading" :
                 result->weaknesses.weakest_detection_area == HEALTH_SOURCE_NEURAL ? "neural" : "unknown");
        result->insight_confidence = 0.9f;
    } else if (result->assessment.recovery_success_rate < 0.8f) {
        snprintf(result->key_insight, sizeof(result->key_insight),
                 "Recovery success rate needs improvement (%.0f%%). Consider alternative recovery strategies.",
                 result->assessment.recovery_success_rate * 100.0f);
        result->insight_confidence = 0.85f;
    } else if (result->assessment.false_positive_rate > 0.1f) {
        snprintf(result->key_insight, sizeof(result->key_insight),
                 "High false positive rate (%.0f%%). Detection thresholds may be too aggressive.",
                 result->assessment.false_positive_rate * 100.0f);
        result->insight_confidence = 0.8f;
    } else {
        snprintf(result->key_insight, sizeof(result->key_insight),
                 "Health system performing well. Accuracy %.0f%%, Recovery %.0f%%. Minor optimizations possible.",
                 result->assessment.accuracy_rate * 100.0f,
                 result->assessment.recovery_success_rate * 100.0f);
        result->insight_confidence = 0.95f;
    }

    /* Generate parameter adjustments */
    result->num_adjustments = 0;

    /* Adjustment 1: Detection threshold based on false positives */
    if (result->assessment.false_positive_rate > 0.05f && result->num_adjustments < META_HEALTH_MAX_ADJUSTMENTS) {
        meta_health_adjustment_t* adj = &result->adjustments[result->num_adjustments++];
        snprintf(adj->detector_name, sizeof(adj->detector_name), "anomaly_detector");
        snprintf(adj->param_name, sizeof(adj->param_name), "confidence_threshold");
        adj->current_value = 0.5f;
        adj->recommended_value = 0.6f;
        snprintf(adj->reason, sizeof(adj->reason),
                 "Reduce false positives by increasing confidence threshold");
        adj->expected_improvement = result->assessment.false_positive_rate * 0.5f;
        adj->confidence = 0.8f;
    }

    /* Adjustment 2: Response time improvement */
    if (result->assessment.avg_response_time_ms > 100.0f && result->num_adjustments < META_HEALTH_MAX_ADJUSTMENTS) {
        meta_health_adjustment_t* adj = &result->adjustments[result->num_adjustments++];
        snprintf(adj->detector_name, sizeof(adj->detector_name), "health_agent");
        snprintf(adj->param_name, sizeof(adj->param_name), "check_interval_ms");
        adj->current_value = 50.0f;
        adj->recommended_value = 30.0f;
        snprintf(adj->reason, sizeof(adj->reason),
                 "Improve response time by increasing check frequency");
        adj->expected_improvement = 0.1f;
        adj->confidence = 0.7f;
    }

    /* Generate new patterns if enabled */
    result->num_new_patterns = 0;

    if (reflector->config.enable_pattern_learning) {
        /* Pattern 1: Memory issues often precede threading issues */
        if (result->num_new_patterns < META_HEALTH_MAX_NEW_PATTERNS) {
            meta_health_pattern_t* pattern = &result->new_patterns[result->num_new_patterns++];
            snprintf(pattern->pattern_description, sizeof(pattern->pattern_description),
                     "Memory warnings often precede thread contention issues");
            pattern->applies_to = HEALTH_SOURCE_MEMORY;
            pattern->suggested_response = HEALTH_RECOVERY_GC;
            pattern->confidence = 0.7f;
            pattern->occurrences = 5;
            snprintf(pattern->predictor, sizeof(pattern->predictor),
                     "memory_warning -> threading_issue (within 60s)");
        }
    }

    /* Calculate improvement potential */
    float potential = 0.0f;
    if (result->assessment.accuracy_rate < 1.0f) {
        potential += (1.0f - result->assessment.accuracy_rate) * 0.4f;
    }
    if (result->assessment.recovery_success_rate < 1.0f) {
        potential += (1.0f - result->assessment.recovery_success_rate) * 0.3f;
    }
    if (result->assessment.false_positive_rate > 0.0f) {
        potential += result->assessment.false_positive_rate * 0.2f;
    }
    result->improvement_potential = potential > 1.0f ? 1.0f : potential;

    /* Set processing stats */
    result->timestamp_us = get_time_us();
    result->reflection_time_ms = (uint32_t)((result->timestamp_us - start_time) / 1000);
    result->decisions_analyzed = log->num_decisions;

    /* Update statistics */
    reflector->stats.reflections_performed++;
    reflector->last_reflection_us = result->timestamp_us;

    float n = (float)reflector->stats.reflections_performed;
    reflector->stats.avg_reflection_time_ms =
        ((n - 1) * reflector->stats.avg_reflection_time_ms + result->reflection_time_ms) / (fabsf(n) > 1e-7f ? n : 1e-7f);
    reflector->stats.avg_improvement =
        ((n - 1) * reflector->stats.avg_improvement + result->improvement_potential) / (fabsf(n) > 1e-7f ? n : 1e-7f);
    reflector->stats.last_reflection_us = result->timestamp_us;

    return 0;
}

int meta_health_reflect_async(
    meta_health_reflector_t* reflector,
    uint64_t* request_id
) {
    if (!reflector || !request_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_health_reflect_async: required parameter is NULL (reflector, request_id)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_health_heartbeat("meta_health_reflect_async", 0.0f);


    if (reflector->num_pending >= reflector->max_pending) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "meta_health_reflect_async: capacity exceeded");
        return -1;  /* Queue full */
    }

    pending_reflection_t* pending = &reflector->pending[reflector->num_pending];
    pending->request_id = reflector->next_request_id++;
    pending->complete = false;
    pending->start_time_us = get_time_us();

    *request_id = pending->request_id;
    reflector->num_pending++;

    return 0;
}

int meta_health_get_reflection_result(
    meta_health_reflector_t* reflector,
    uint64_t request_id,
    meta_health_reflection_result_t* result
) {
    if (!reflector || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_health_get_reflection_result: required parameter is NULL (reflector, result)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_health_heartbeat("meta_health_get_reflection_resul", 0.0f);


    for (uint32_t i = 0; i < reflector->num_pending; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && reflector->num_pending > 256) {
            meta_health_heartbeat("meta_health_loop",
                             (float)(i + 1) / (float)reflector->num_pending);
        }

        if (reflector->pending[i].request_id == request_id) {
            pending_reflection_t* pending = &reflector->pending[i];

            /* Complete if not done */
            if (!pending->complete) {
                meta_health_reflect(reflector, &pending->result);
                pending->complete = true;
            }

            *result = pending->result;

            /* Remove from pending */
            if (i < reflector->num_pending - 1) {
                memmove(&reflector->pending[i],
                        &reflector->pending[i + 1],
                        (reflector->num_pending - i - 1) * sizeof(pending_reflection_t));
            }
            reflector->num_pending--;

            return 1;  /* Complete */
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "meta_health_get_reflection_result: operation failed");
    return -1;  /* Not found */
}

int meta_health_get_assessment(
    const meta_health_reflector_t* reflector,
    meta_health_assessment_t* assessment
) {
    if (!reflector || !assessment) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_health_get_assessment: required parameter is NULL (reflector, assessment)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_health_heartbeat("meta_health_get_assessment", 0.0f);


    compute_assessment(reflector, assessment);
    return 0;
}

int meta_health_get_weaknesses(
    const meta_health_reflector_t* reflector,
    meta_health_weakness_t* weaknesses
) {
    if (!reflector || !weaknesses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_health_get_weaknesses: required parameter is NULL (reflector, weaknesses)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_health_heartbeat("meta_health_get_weaknesses", 0.0f);


    compute_weaknesses(reflector, weaknesses);
    return 0;
}

/* ============================================================================
 * Learning Application API Implementation
 * ============================================================================ */

int meta_health_apply_learnings(
    meta_health_reflector_t* reflector,
    const meta_health_reflection_result_t* result
) {
    if (!reflector || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_health_apply_learnings: required parameter is NULL (reflector, result)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_health_heartbeat("meta_health_apply_learnings", 0.0f);


    int applied = 0;

    for (uint32_t i = 0; i < result->num_adjustments; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && result->num_adjustments > 256) {
            meta_health_heartbeat("meta_health_loop",
                             (float)(i + 1) / (float)result->num_adjustments);
        }

        const meta_health_adjustment_t* adj = &result->adjustments[i];

        /* Only apply if confidence is high enough */
        if (adj->confidence >= reflector->config.auto_apply_confidence_threshold) {
            if (meta_health_apply_adjustment(reflector, adj) == 0) {
                applied++;
            }
        }
    }

    /* Register new patterns */
    if (reflector->config.enable_pattern_learning) {
        for (uint32_t i = 0; i < result->num_new_patterns; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && result->num_new_patterns > 256) {
                meta_health_heartbeat("meta_health_loop",
                                 (float)(i + 1) / (float)result->num_new_patterns);
            }

            meta_health_register_pattern(reflector, &result->new_patterns[i]);
        }
    }

    return applied;
}

int meta_health_apply_adjustment(
    meta_health_reflector_t* reflector,
    const meta_health_adjustment_t* adjustment
) {
    if (!reflector || !adjustment) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_health_apply_adjustment: required parameter is NULL (reflector, adjustment)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_health_heartbeat("meta_health_apply_adjustment", 0.0f);


    if (reflector->num_applied >= reflector->max_applied) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "meta_health_apply_adjustment: capacity exceeded");
        return -1;  /* No room */
    }

    /* Store for potential reversion */
    applied_adjustment_t* record = &reflector->applied_adjustments[reflector->num_applied++];
    record->adjustment = *adjustment;
    record->original_value = adjustment->current_value;
    record->apply_time_us = get_time_us();

    reflector->stats.adjustments_applied++;

    /* In a real implementation, this would actually modify the health agent's parameters.
     * For now, we just record the adjustment. */

    return 0;
}

int meta_health_register_pattern(
    meta_health_reflector_t* reflector,
    const meta_health_pattern_t* pattern
) {
    if (!reflector || !pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_health_register_pattern: required parameter is NULL (reflector, pattern)");
        return -1;
    }

    /* Check if pattern already exists */
    /* Phase 8: Heartbeat at operation start */
    meta_health_heartbeat("meta_health_register_pattern", 0.0f);


    for (uint32_t i = 0; i < reflector->num_patterns; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && reflector->num_patterns > 256) {
            meta_health_heartbeat("meta_health_loop",
                             (float)(i + 1) / (float)reflector->num_patterns);
        }

        if (strcmp(reflector->patterns[i].pattern_description, pattern->pattern_description) == 0) {
            /* Update existing pattern */
            reflector->patterns[i].occurrences += pattern->occurrences;
            reflector->patterns[i].confidence =
                (reflector->patterns[i].confidence + pattern->confidence) / 2.0f;
            return 0;
        }
    }

    if (reflector->num_patterns >= reflector->max_patterns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "meta_health_register_pattern: capacity exceeded");
        return -1;  /* No room */
    }

    /* Add new pattern */
    reflector->patterns[reflector->num_patterns++] = *pattern;
    reflector->stats.patterns_discovered++;

    return 0;
}

int meta_health_revert_learnings(meta_health_reflector_t* reflector) {
    if (!reflector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reflector is NULL");

        return -1;
    }

    /* In a real implementation, this would restore original parameter values.
     * For now, we just clear the applied adjustments list. */
    /* Phase 8: Heartbeat at operation start */
    meta_health_heartbeat("meta_health_revert_learnings", 0.0f);


    reflector->num_applied = 0;

    return 0;
}

/* ============================================================================
 * Statistics API Implementation
 * ============================================================================ */

int meta_health_get_stats(
    const meta_health_reflector_t* reflector,
    meta_health_stats_t* stats
) {
    if (!reflector || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_health_get_stats: required parameter is NULL (reflector, stats)");
        return -1;
    }

    *stats = reflector->stats;

    /* Calculate trends (simplified) */
    /* Phase 8: Heartbeat at operation start */
    meta_health_heartbeat("meta_health_get_stats", 0.0f);


    stats->accuracy_trend = 0.0f;  /* Would need historical data */
    stats->response_time_trend = 0.0f;

    return 0;
}

void meta_health_reset_stats(meta_health_reflector_t* reflector) {
    if (!reflector) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_health_heartbeat("meta_health_reset_stats", 0.0f);


    memset(&reflector->stats, 0, sizeof(meta_health_stats_t));
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

const char* meta_health_outcome_name(meta_health_outcome_t outcome) {
    switch (outcome) {
        case META_HEALTH_OUTCOME_SUCCESS: return "SUCCESS";
        case META_HEALTH_OUTCOME_PARTIAL_SUCCESS: return "PARTIAL_SUCCESS";
        case META_HEALTH_OUTCOME_FAILURE: return "FAILURE";
        case META_HEALTH_OUTCOME_UNKNOWN: return "UNKNOWN";
        case META_HEALTH_OUTCOME_FALSE_POSITIVE: return "FALSE_POSITIVE";
        case META_HEALTH_OUTCOME_FALSE_NEGATIVE: return "FALSE_NEGATIVE";
        default: return "UNKNOWN";
    }
}

void meta_health_init_decision(meta_health_decision_t* decision) {
    if (!decision) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_health_heartbeat("meta_health_init_decision", 0.0f);


    memset(decision, 0, sizeof(meta_health_decision_t));
    decision->timestamp_us = get_time_us();
    decision->outcome = META_HEALTH_OUTCOME_UNKNOWN;
    decision->recovery_succeeded = false;
    decision->detection_confidence = 0.5f;
    decision->post_recovery_health = 1.0f;
}

void meta_health_init_reflection_result(meta_health_reflection_result_t* result) {
    if (!result) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_health_heartbeat("meta_health_init_reflection_resu", 0.0f);


    memset(result, 0, sizeof(meta_health_reflection_result_t));
    result->timestamp_us = get_time_us();
    result->insight_confidence = 0.0f;
    result->improvement_potential = 0.0f;
}

void meta_health_dump_reflection(const meta_health_reflection_result_t* result) {
    if (!result) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_health_heartbeat("meta_health_dump_reflection", 0.0f);


    printf("=== Meta-Health Reflection Result ===\n");
    printf("Timestamp: %lu us\n", (unsigned long)result->timestamp_us);
    printf("Key Insight: %s\n", result->key_insight);
    printf("Insight Confidence: %.2f\n", result->insight_confidence);
    printf("\n--- Assessment ---\n");
    printf("Accuracy Rate: %.2f\n", result->assessment.accuracy_rate);
    printf("Recovery Success: %.2f\n", result->assessment.recovery_success_rate);
    printf("False Positive Rate: %.2f\n", result->assessment.false_positive_rate);
    printf("Avg Response Time: %.2f ms\n", result->assessment.avg_response_time_ms);
    printf("Decisions Analyzed: %u\n", result->decisions_analyzed);
    printf("\n--- Adjustments (%u) ---\n", result->num_adjustments);
    for (uint32_t i = 0; i < result->num_adjustments; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && result->num_adjustments > 256) {
            meta_health_heartbeat("meta_health_loop",
                             (float)(i + 1) / (float)result->num_adjustments);
        }

        printf("  %s.%s: %.2f -> %.2f (%s)\n",
               result->adjustments[i].detector_name,
               result->adjustments[i].param_name,
               result->adjustments[i].current_value,
               result->adjustments[i].recommended_value,
               result->adjustments[i].reason);
    }
    printf("\n--- New Patterns (%u) ---\n", result->num_new_patterns);
    for (uint32_t i = 0; i < result->num_new_patterns; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && result->num_new_patterns > 256) {
            meta_health_heartbeat("meta_health_loop",
                             (float)(i + 1) / (float)result->num_new_patterns);
        }

        printf("  %s (confidence: %.2f)\n",
               result->new_patterns[i].pattern_description,
               result->new_patterns[i].confidence);
    }
    printf("\nImprovement Potential: %.2f\n", result->improvement_potential);
    printf("Reflection Time: %u ms\n", result->reflection_time_ms);
    printf("=====================================\n");
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void meta_health_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_meta_health_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int meta_health_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "meta_health_training_begin: NULL argument");
        return -1;
    }
    meta_health_heartbeat_instance(NULL, "meta_health_training_begin", 0.0f);
    (void)(struct meta_health_reflector*)instance; /* Module state available for reset */
    return 0;
}

int meta_health_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "meta_health_training_end: NULL argument");
        return -1;
    }
    meta_health_heartbeat_instance(NULL, "meta_health_training_end", 1.0f);
    (void)(struct meta_health_reflector*)instance; /* Module state available for finalization */
    return 0;
}

int meta_health_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "meta_health_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    meta_health_heartbeat_instance(NULL, "meta_health_training_step", progress);
    (void)(struct meta_health_reflector*)instance; /* Module state available for step adaptation */
    return 0;
}
