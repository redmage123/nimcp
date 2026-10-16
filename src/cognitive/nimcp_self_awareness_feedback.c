// ============================================================================
// nimcp_self_awareness_feedback.c - Feedback Loop System Implementation
// ============================================================================
/**
 * @file nimcp_self_awareness_feedback.c
 * @brief Implementation of feedback loop management for self-awareness
 *
 * WHAT: Implements feedback policies, history tracking, and analysis
 * WHY: Provides tunable, analyzable feedback between self-awareness components
 * HOW: Ring buffers for history, statistical analysis, adaptive rate control
 */

#include "cognitive/nimcp_self_awareness_feedback.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <float.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(self_awareness_feedback, MESH_ADAPTER_CATEGORY_COGNITIVE)



// ============================================================================
// Policy Functions
// ============================================================================

int feedback_default_policy(feedback_policy_t* policy) {
    if (!policy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "policy is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_default_pol", 0.0f);


    policy->transfer_func = TRANSFER_LINEAR;
    policy->learning_rate = FEEDBACK_DEFAULT_LEARNING_RATE;
    policy->momentum = FEEDBACK_DEFAULT_MOMENTUM;
    policy->decay = FEEDBACK_DEFAULT_DECAY;

    policy->min_threshold = 0.0f;
    policy->max_threshold = 1.0f;

    policy->gate_threshold = 0.5f;
    policy->gate_open = true;

    policy->adaptive_rate = false;
    policy->rate_increase_factor = NIMCP_RATE_INCREASE_FACTOR;
    policy->rate_decrease_factor = NIMCP_EMA_WEIGHT_SLOW;

    policy->cooldown_ms = 10;

    return 0;
}

int feedback_conservative_policy(feedback_policy_t* policy) {
    if (!policy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "policy is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_conservativ", 0.0f);


    policy->transfer_func = TRANSFER_SIGMOID;
    policy->learning_rate = 0.01f;
    policy->momentum = 0.95f;
    policy->decay = 0.999f;

    policy->min_threshold = NIMCP_PLASTICITY_RATE_DEFAULT;
    policy->max_threshold = NIMCP_EMA_WEIGHT_SLOW;

    policy->gate_threshold = 0.5f;
    policy->gate_open = true;

    policy->adaptive_rate = true;
    policy->rate_increase_factor = 1.05f;
    policy->rate_decrease_factor = 0.8f;

    policy->cooldown_ms = 100;

    return 0;
}

int feedback_aggressive_policy(feedback_policy_t* policy) {
    if (!policy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "policy is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_aggressive_", 0.0f);


    policy->transfer_func = TRANSFER_LINEAR;
    policy->learning_rate = 0.5f;
    policy->momentum = 0.5f;
    policy->decay = NIMCP_EMA_WEIGHT_SLOW;

    policy->min_threshold = 0.0f;
    policy->max_threshold = 1.0f;

    policy->gate_threshold = 0.3f;
    policy->gate_open = true;

    policy->adaptive_rate = true;
    policy->rate_increase_factor = 1.2f;
    policy->rate_decrease_factor = 0.95f;

    policy->cooldown_ms = 1;

    return 0;
}

int feedback_gated_policy(feedback_policy_t* policy, float gate_threshold) {
    if (!policy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "policy is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_gated_polic", 0.0f);


    feedback_default_policy(policy);
    policy->transfer_func = TRANSFER_GATED;
    policy->gate_threshold = gate_threshold;
    policy->gate_open = false;

    return 0;
}

// ============================================================================
// Transfer Functions
// ============================================================================

/**
 * @brief Sigmoid function
 */
static float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

float feedback_apply_transfer(
    float value,
    transfer_function_t func,
    float gate_threshold,
    bool gate_open
) {
    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_apply_trans", 0.0f);


    switch (func) {
        case TRANSFER_LINEAR:
            return value;

        case TRANSFER_SIGMOID:
            return sigmoid(value);

        case TRANSFER_TANH:
            return tanhf(value);

        case TRANSFER_EXPONENTIAL:
            return expf(value) - 1.0f;

        case TRANSFER_LOGARITHMIC:
            return (value > 0.0f) ? logf(1.0f + value) : 0.0f;

        case TRANSFER_STEP:
            return (value >= gate_threshold) ? 1.0f : 0.0f;

        case TRANSFER_GATED:
            return gate_open ? value : 0.0f;

        default:
            return value;
    }
}

// ============================================================================
// Feedback System Functions
// ============================================================================

int feedback_system_init(feedback_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_system_init", 0.0f);


    memset(system, 0, sizeof(feedback_system_t));

    /* Initialize each loop manager */
    for (int i = 0; i < FEEDBACK_LOOP_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && FEEDBACK_LOOP_COUNT > 256) {
            self_awareness_feedback_heartbeat("self_awarene_loop",
                             (float)(i + 1) / (float)FEEDBACK_LOOP_COUNT);
        }

        feedback_loop_manager_t* mgr = &system->loops[i];

        mgr->type = (feedback_loop_type_t)i;
        feedback_default_policy(&mgr->policy);

        /* History ring buffer */
        mgr->history.head = 0;
        mgr->history.count = 0;
        mgr->history.capacity = FEEDBACK_MAX_HISTORY_ENTRIES;

        /* Initial state */
        mgr->momentum_value = 0.0f;
        mgr->ema_value = 0.0f;
        mgr->last_transfer_ms = 0;
        mgr->last_analysis_ms = 0;
        mgr->current_learning_rate = mgr->policy.learning_rate;
        mgr->consecutive_failures = 0;
        mgr->consecutive_successes = 0;

        /* Initial analysis */
        mgr->analysis.trend = TREND_STABLE;
        mgr->analysis.health = FEEDBACK_HEALTH_OPTIMAL;
    }

    system->total_transfers = 0;
    system->total_failures = 0;
    system->analysis_interval_ms = NIMCP_TIMEOUT_LONG_MS;
    system->last_global_analysis_ms = 0;
    system->initialized = true;

    NIMCP_LOGGING_INFO("Feedback system initialized with %d loops", FEEDBACK_LOOP_COUNT);

    return 0;
}

void feedback_system_cleanup(feedback_system_t* system) {
    if (!system) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_system_clea", 0.0f);


    memset(system, 0, sizeof(feedback_system_t));
    system->initialized = false;
}

int feedback_set_policy(
    feedback_system_t* system,
    feedback_loop_type_t loop_type,
    const feedback_policy_t* policy
) {
    if (!system || !policy || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feedback_set_policy: required parameter is NULL (system, policy, system->initialized)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_set_policy", 0.0f);


    if (loop_type < 0 || loop_type >= FEEDBACK_LOOP_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "feedback_set_policy: capacity exceeded");
        return -1;
    }

    feedback_loop_manager_t* mgr = &system->loops[loop_type];
    memcpy(&mgr->policy, policy, sizeof(feedback_policy_t));
    mgr->current_learning_rate = policy->learning_rate;

    return 0;
}

int feedback_get_policy(
    const feedback_system_t* system,
    feedback_loop_type_t loop_type,
    feedback_policy_t* policy
) {
    if (!system || !policy || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feedback_get_policy: required parameter is NULL (system, policy, system->initialized)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_get_policy", 0.0f);


    if (loop_type < 0 || loop_type >= FEEDBACK_LOOP_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "feedback_get_policy: capacity exceeded");
        return -1;
    }

    memcpy(policy, &system->loops[loop_type].policy, sizeof(feedback_policy_t));
    return 0;
}

// ============================================================================
// Transfer Recording
// ============================================================================

int feedback_record_transfer(
    feedback_system_t* system,
    feedback_loop_type_t loop_type,
    float source_value,
    float transferred_value,
    float target_delta,
    float latency_ms,
    bool successful,
    const char* error_msg
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feedback_record_transfer: required parameter is NULL (system, system->initialized)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_record_tran", 0.0f);


    if (loop_type < 0 || loop_type >= FEEDBACK_LOOP_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "feedback_record_transfer: capacity exceeded");
        return -1;
    }

    feedback_loop_manager_t* mgr = &system->loops[loop_type];
    feedback_history_t* hist = &mgr->history;

    /* Add to ring buffer */
    feedback_transfer_record_t* record = &hist->records[hist->head];

    record->timestamp_ms = nimcp_time_get_ms();
    record->loop_type = loop_type;
    record->source_value = source_value;
    record->transferred_value = transferred_value;
    record->target_delta = target_delta;
    record->latency_ms = latency_ms;
    record->successful = successful;

    if (error_msg) {
        strncpy(record->error_msg, error_msg, sizeof(record->error_msg) - 1);
        record->error_msg[sizeof(record->error_msg) - 1] = '\0';
    } else {
        record->error_msg[0] = '\0';
    }

    /* Advance ring buffer */
    hist->head = (hist->head + 1) % hist->capacity;
    if (hist->count < hist->capacity) {
        hist->count++;
    }

    /* Update manager state */
    mgr->last_transfer_ms = record->timestamp_ms;
    system->total_transfers++;

    if (successful) {
        mgr->consecutive_successes++;
        mgr->consecutive_failures = 0;

        /* Adaptive rate increase */
        if (mgr->policy.adaptive_rate && mgr->consecutive_successes >= 5) {
            float new_rate = mgr->current_learning_rate * mgr->policy.rate_increase_factor;
            if (new_rate <= mgr->policy.learning_rate * 2.0f) {
                mgr->current_learning_rate = new_rate;
            }
            mgr->consecutive_successes = 0;
        }
    } else {
        mgr->consecutive_failures++;
        mgr->consecutive_successes = 0;
        system->total_failures++;

        /* Adaptive rate decrease */
        if (mgr->policy.adaptive_rate && mgr->consecutive_failures >= 2) {
            float new_rate = mgr->current_learning_rate * mgr->policy.rate_decrease_factor;
            float min_rate = mgr->policy.learning_rate * NIMCP_EMA_WEIGHT_FAST;
            if (new_rate >= min_rate) {
                mgr->current_learning_rate = new_rate;
            }
            mgr->consecutive_failures = 0;
        }
    }

    return 0;
}

int feedback_compute_value(
    feedback_system_t* system,
    feedback_loop_type_t loop_type,
    float raw_value,
    float* output
) {
    if (!system || !output || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feedback_compute_value: required parameter is NULL (system, output, system->initialized)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_compute_val", 0.0f);


    if (loop_type < 0 || loop_type >= FEEDBACK_LOOP_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "feedback_compute_value: capacity exceeded");
        return -1;
    }

    feedback_loop_manager_t* mgr = &system->loops[loop_type];
    const feedback_policy_t* policy = &mgr->policy;

    /* Check cooldown */
    uint64_t now = nimcp_time_get_ms();
    if (now - mgr->last_transfer_ms < policy->cooldown_ms) {
        *output = 0.0f;
        return 0;
    }

    /* Apply transfer function */
    float transferred = feedback_apply_transfer(
        raw_value,
        policy->transfer_func,
        policy->gate_threshold,
        policy->gate_open
    );

    /* Apply thresholds */
    if (fabsf(transferred) < policy->min_threshold) {
        transferred = 0.0f;
    }
    if (transferred > policy->max_threshold) {
        transferred = policy->max_threshold;
    } else if (transferred < -policy->max_threshold) {
        transferred = -policy->max_threshold;
    }

    /* Apply momentum */
    float momentum_contrib = mgr->momentum_value * policy->momentum;
    float new_momentum = momentum_contrib + transferred * (1.0f - policy->momentum);
    mgr->momentum_value = new_momentum;

    /* Apply learning rate */
    float final_value = new_momentum * mgr->current_learning_rate;

    /* Apply decay to EMA */
    mgr->ema_value = mgr->ema_value * policy->decay + final_value * (1.0f - policy->decay);

    *output = final_value;
    return 0;
}

// ============================================================================
// Analysis Functions
// ============================================================================

int feedback_analyze_loop(
    feedback_system_t* system,
    feedback_loop_type_t loop_type,
    feedback_analysis_t* analysis
) {
    if (!system || !analysis || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feedback_analyze_loop: required parameter is NULL (system, analysis, system->initialized)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_analyze_loo", 0.0f);


    if (loop_type < 0 || loop_type >= FEEDBACK_LOOP_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "feedback_analyze_loop: capacity exceeded");
        return -1;
    }

    feedback_loop_manager_t* mgr = &system->loops[loop_type];
    feedback_history_t* hist = &mgr->history;

    memset(analysis, 0, sizeof(feedback_analysis_t));

    if (hist->count == 0) {
        analysis->trend = TREND_STABLE;
        analysis->health = FEEDBACK_HEALTH_OPTIMAL;
        analysis->is_stale = true;
        return 0;
    }

    /* Compute statistics from history */
    float sum = 0.0f;
    float sum_sq = 0.0f;
    float min_val = FLT_MAX;
    float max_val = -FLT_MAX;
    float sum_latency = 0.0f;
    float max_latency = 0.0f;
    uint32_t success_count = 0;

    float trend_samples[FEEDBACK_MAX_TREND_SAMPLES];
    uint32_t trend_count = 0;

    uint32_t idx = (hist->head + hist->capacity - hist->count) % hist->capacity;
    for (uint32_t i = 0; i < hist->count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hist->count > 256) {
            self_awareness_feedback_heartbeat("self_awarene_loop",
                             (float)(i + 1) / (float)hist->count);
        }

        const feedback_transfer_record_t* rec = &hist->records[idx];

        float val = rec->transferred_value;
        sum += val;
        sum_sq += val * val;
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;

        sum_latency += rec->latency_ms;
        if (rec->latency_ms > max_latency) {
            max_latency = rec->latency_ms;
        }

        if (rec->successful) success_count++;

        /* Keep recent values for trend */
        if (trend_count < FEEDBACK_MAX_TREND_SAMPLES) {
            trend_samples[trend_count++] = val;
        }

        idx = (idx + 1) % hist->capacity;
    }

    float n = (float)hist->count;
    analysis->mean_value = sum / (fabsf(n) > 1e-7f ? n : 1e-7f);
    analysis->variance = (sum_sq / (fabsf(n) > 1e-7f ? n : 1e-7f)) - (analysis->mean_value * analysis->mean_value);
    analysis->min_value = min_val;
    analysis->max_value = max_val;
    analysis->avg_latency_ms = sum_latency / (fabsf(n) > 1e-7f ? n : 1e-7f);
    analysis->max_latency_ms = max_latency;
    analysis->success_rate = (float)success_count / (fabsf(n) > 1e-7f ? n : 1e-7f);

    /* Compute trend using linear regression on recent samples */
    if (trend_count >= 3) {
        float sum_x = 0.0f, sum_y = 0.0f, sum_xy = 0.0f, sum_xx = 0.0f;
        for (uint32_t i = 0; i < trend_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && trend_count > 256) {
                self_awareness_feedback_heartbeat("self_awarene_loop",
                                 (float)(i + 1) / (float)trend_count);
            }

            float x = (float)i;
            float y = trend_samples[i];
            sum_x += x;
            sum_y += y;
            sum_xy += x * y;
            sum_xx += x * x;
        }
        float tn = (float)trend_count;
        float denom = tn * sum_xx - sum_x * sum_x;
        if (fabsf(denom) > 1e-6f) {
            analysis->trend_slope = (tn * sum_xy - sum_x * sum_y) / denom;

            /* Compute R^2 for confidence */
            float y_mean = sum_y / tn;
            float ss_tot = 0.0f, ss_res = 0.0f;
            for (uint32_t i = 0; i < trend_count; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && trend_count > 256) {
                    self_awareness_feedback_heartbeat("self_awarene_loop",
                                     (float)(i + 1) / (float)trend_count);
                }

                float pred = analysis->trend_slope * (float)i + (sum_y - analysis->trend_slope * sum_x) / tn;
                ss_res += (trend_samples[i] - pred) * (trend_samples[i] - pred);
                ss_tot += (trend_samples[i] - y_mean) * (trend_samples[i] - y_mean);
            }
            analysis->trend_confidence = (ss_tot > 1e-6f) ? 1.0f - (ss_res / ss_tot) : 0.0f;
            if (analysis->trend_confidence < 0.0f) analysis->trend_confidence = 0.0f;
        }
    }

    /* Determine trend type */
    if (analysis->trend_confidence > 0.5f) {
        if (analysis->trend_slope > NIMCP_PLASTICITY_RATE_DEFAULT) {
            analysis->trend = TREND_INCREASING;
        } else if (analysis->trend_slope < -NIMCP_PLASTICITY_RATE_DEFAULT) {
            analysis->trend = TREND_DECREASING;
        } else {
            analysis->trend = TREND_STABLE;
        }
    } else if (analysis->variance > 0.5f) {
        analysis->trend = TREND_OSCILLATING;
    } else {
        analysis->trend = TREND_STABLE;
    }

    /* Check for divergence */
    if (analysis->variance > 2.0f || fabsf(analysis->trend_slope) > 1.0f) {
        analysis->trend = TREND_DIVERGING;
    }

    /* Staleness check */
    uint64_t now = nimcp_time_get_ms();
    analysis->time_since_last_ms = now - mgr->last_transfer_ms;
    analysis->is_stale = (analysis->time_since_last_ms > 5000);

    /* Health assessment */
    if (analysis->success_rate < 0.5f || analysis->trend == TREND_DIVERGING) {
        analysis->health = FEEDBACK_HEALTH_FAILING;
    } else if (analysis->success_rate < 0.8f || analysis->is_stale) {
        analysis->health = FEEDBACK_HEALTH_DEGRADED;
    } else if (analysis->time_since_last_ms > 60000) {
        analysis->health = FEEDBACK_HEALTH_DEAD;
    } else {
        analysis->health = FEEDBACK_HEALTH_OPTIMAL;
    }

    /* Store in manager */
    memcpy(&mgr->analysis, analysis, sizeof(feedback_analysis_t));
    mgr->last_analysis_ms = now;

    return 0;
}

int feedback_analyze_all(feedback_system_t* system) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feedback_analyze_all: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_analyze_all", 0.0f);


    feedback_analysis_t analysis;
    for (int i = 0; i < FEEDBACK_LOOP_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && FEEDBACK_LOOP_COUNT > 256) {
            self_awareness_feedback_heartbeat("self_awarene_loop",
                             (float)(i + 1) / (float)FEEDBACK_LOOP_COUNT);
        }

        feedback_analyze_loop(system, (feedback_loop_type_t)i, &analysis);
    }

    system->last_global_analysis_ms = nimcp_time_get_ms();
    return 0;
}

feedback_health_t feedback_get_health(
    const feedback_system_t* system,
    feedback_loop_type_t loop_type
) {
    if (!system || !system->initialized) {
        return FEEDBACK_HEALTH_DEAD;
    }
    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_get_health", 0.0f);


    if (loop_type < 0 || loop_type >= FEEDBACK_LOOP_COUNT) {
        return FEEDBACK_HEALTH_DEAD;
    }

    return system->loops[loop_type].analysis.health;
}

feedback_trend_t feedback_get_trend(
    const feedback_system_t* system,
    feedback_loop_type_t loop_type
) {
    if (!system || !system->initialized) {
        return TREND_STABLE;
    }
    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_get_trend", 0.0f);


    if (loop_type < 0 || loop_type >= FEEDBACK_LOOP_COUNT) {
        return TREND_STABLE;
    }

    return system->loops[loop_type].analysis.trend;
}

bool feedback_has_unhealthy_loops(const feedback_system_t* system) {
    if (!system || !system->initialized) {
        return true;
    }

    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_has_unhealt", 0.0f);


    for (int i = 0; i < FEEDBACK_LOOP_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && FEEDBACK_LOOP_COUNT > 256) {
            self_awareness_feedback_heartbeat("self_awarene_loop",
                             (float)(i + 1) / (float)FEEDBACK_LOOP_COUNT);
        }

        feedback_health_t h = system->loops[i].analysis.health;
        if (h == FEEDBACK_HEALTH_FAILING || h == FEEDBACK_HEALTH_DEAD) {
            return true;
        }
    }

    return false;
}

// ============================================================================
// History Functions
// ============================================================================

int feedback_get_history(
    const feedback_system_t* system,
    feedback_loop_type_t loop_type,
    feedback_transfer_record_t* records,
    uint32_t max_records,
    uint32_t* count
) {
    if (!system || !records || !count || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feedback_get_history: required parameter is NULL (system, records, count, system->initialized)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_get_history", 0.0f);


    if (loop_type < 0 || loop_type >= FEEDBACK_LOOP_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "feedback_get_history: capacity exceeded");
        return -1;
    }

    const feedback_history_t* hist = &system->loops[loop_type].history;

    uint32_t to_copy = (hist->count < max_records) ? hist->count : max_records;

    /* Copy from ring buffer, most recent first */
    for (uint32_t i = 0; i < to_copy; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && to_copy > 256) {
            self_awareness_feedback_heartbeat("self_awarene_loop",
                             (float)(i + 1) / (float)to_copy);
        }

        uint32_t idx = (hist->head + hist->capacity - 1 - i) % hist->capacity;
        memcpy(&records[i], &hist->records[idx], sizeof(feedback_transfer_record_t));
    }

    *count = to_copy;
    return 0;
}

int feedback_clear_history(
    feedback_system_t* system,
    feedback_loop_type_t loop_type
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feedback_clear_history: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_clear_histo", 0.0f);


    if (loop_type >= FEEDBACK_LOOP_COUNT) {
        /* Clear all */
        for (int i = 0; i < FEEDBACK_LOOP_COUNT; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && FEEDBACK_LOOP_COUNT > 256) {
                self_awareness_feedback_heartbeat("self_awarene_loop",
                                 (float)(i + 1) / (float)FEEDBACK_LOOP_COUNT);
            }

            system->loops[i].history.head = 0;
            system->loops[i].history.count = 0;
        }
    } else {
        system->loops[loop_type].history.head = 0;
        system->loops[loop_type].history.count = 0;
    }

    return 0;
}

// ============================================================================
// Gate Functions
// ============================================================================

int feedback_open_gate(
    feedback_system_t* system,
    feedback_loop_type_t loop_type
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feedback_open_gate: required parameter is NULL (system, system->initialized)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_open_gate", 0.0f);


    if (loop_type < 0 || loop_type >= FEEDBACK_LOOP_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "feedback_open_gate: capacity exceeded");
        return -1;
    }

    system->loops[loop_type].policy.gate_open = true;
    return 0;
}

int feedback_close_gate(
    feedback_system_t* system,
    feedback_loop_type_t loop_type
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feedback_close_gate: required parameter is NULL (system, system->initialized)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_close_gate", 0.0f);


    if (loop_type < 0 || loop_type >= FEEDBACK_LOOP_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "feedback_close_gate: capacity exceeded");
        return -1;
    }

    system->loops[loop_type].policy.gate_open = false;
    return 0;
}

bool feedback_is_gate_open(
    const feedback_system_t* system,
    feedback_loop_type_t loop_type
) {
    if (!system || !system->initialized) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_is_gate_ope", 0.0f);


    if (loop_type < 0 || loop_type >= FEEDBACK_LOOP_COUNT) {
        return false;
    }

    return system->loops[loop_type].policy.gate_open;
}

// ============================================================================
// Adaptive Rate Functions
// ============================================================================

int feedback_enable_adaptive_rate(
    feedback_system_t* system,
    feedback_loop_type_t loop_type,
    float increase_factor,
    float decrease_factor
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feedback_enable_adaptive_rate: required parameter is NULL (system, system->initialized)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_enable_adap", 0.0f);


    if (loop_type < 0 || loop_type >= FEEDBACK_LOOP_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "feedback_enable_adaptive_rate: capacity exceeded");
        return -1;
    }

    feedback_policy_t* policy = &system->loops[loop_type].policy;
    policy->adaptive_rate = true;
    policy->rate_increase_factor = increase_factor;
    policy->rate_decrease_factor = decrease_factor;

    return 0;
}

int feedback_disable_adaptive_rate(
    feedback_system_t* system,
    feedback_loop_type_t loop_type
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feedback_disable_adaptive_rate: required parameter is NULL (system, system->initialized)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_disable_ada", 0.0f);


    if (loop_type < 0 || loop_type >= FEEDBACK_LOOP_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "feedback_disable_adaptive_rate: capacity exceeded");
        return -1;
    }

    feedback_loop_manager_t* mgr = &system->loops[loop_type];
    mgr->policy.adaptive_rate = false;
    mgr->current_learning_rate = mgr->policy.learning_rate;

    return 0;
}

float feedback_get_current_rate(
    const feedback_system_t* system,
    feedback_loop_type_t loop_type
) {
    if (!system || !system->initialized) {
        return 0.0f;
    }
    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_feedback_get_current", 0.0f);


    if (loop_type < 0 || loop_type >= FEEDBACK_LOOP_COUNT) {
        return 0.0f;
    }

    return system->loops[loop_type].current_learning_rate;
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* feedback_transfer_name(transfer_function_t func) {
    switch (func) {
        case TRANSFER_LINEAR:      return "LINEAR";
        case TRANSFER_SIGMOID:     return "SIGMOID";
        case TRANSFER_TANH:        return "TANH";
        case TRANSFER_EXPONENTIAL: return "EXPONENTIAL";
        case TRANSFER_LOGARITHMIC: return "LOGARITHMIC";
        case TRANSFER_STEP:        return "STEP";
        case TRANSFER_GATED:       return "GATED";
        default:                   return "UNKNOWN";
    }
}

const char* feedback_trend_name(feedback_trend_t trend) {
    switch (trend) {
        case TREND_STABLE:      return "STABLE";
        case TREND_INCREASING:  return "INCREASING";
        case TREND_DECREASING:  return "DECREASING";
        case TREND_OSCILLATING: return "OSCILLATING";
        case TREND_DIVERGING:   return "DIVERGING";
        default:                return "UNKNOWN";
    }
}

const char* feedback_health_name(feedback_health_t health) {
    switch (health) {
        case FEEDBACK_HEALTH_OPTIMAL:  return "OPTIMAL";
        case FEEDBACK_HEALTH_DEGRADED: return "DEGRADED";
        case FEEDBACK_HEALTH_FAILING:  return "FAILING";
        case FEEDBACK_HEALTH_DEAD:     return "DEAD";
        default:                       return "UNKNOWN";
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int self_awareness_feedback_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    self_awareness_feedback_heartbeat("self_awarene_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Self_Awareness_Feedback");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                self_awareness_feedback_heartbeat("self_awarene_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Self_Awareness_Feedback");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Self_Awareness_Feedback");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void self_awareness_feedback_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_self_awareness_feedback_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int self_awareness_feedback_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_awareness_feedback_training_begin: NULL argument");
        return -1;
    }
    self_awareness_feedback_heartbeat_instance(NULL, "self_awareness_feedback_training_begin", 0.0f);
    return 0;
}

int self_awareness_feedback_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_awareness_feedback_training_end: NULL argument");
        return -1;
    }
    self_awareness_feedback_heartbeat_instance(NULL, "self_awareness_feedback_training_end", 1.0f);
    return 0;
}

int self_awareness_feedback_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_awareness_feedback_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    self_awareness_feedback_heartbeat_instance(NULL, "self_awareness_feedback_training_step", progress);
    return 0;
}
