/**
 * @file nimcp_eligibility_utils_quantum_bridge.c
 * @brief Eligibility Utils-Quantum Bidirectional Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-12
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "plasticity/eligibility/nimcp_eligibility_utils_quantum_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(eligibility_utils_quantum_bridge)

/* Security integration */
#define LOG_MODULE "ELIGIBILITY_UTILS_QUANTUM_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct elig_uq_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    elig_uq_bridge_config_t config;

    /* Connected contexts */
    eligibility_utils_ctx_t utils_ctx;
    eligibility_quantum_ctx_t quantum_ctx;

    /* State */
    elig_uq_bridge_state_t state;

    /* Statistics */
    elig_uq_bridge_stats_t stats;

    /* Previous values for delta computation */
    float previous_energy;
    float previous_ltp_ltd_ratio;

    /* Coherence history for stability */
    float coherence_history[ELIG_UQ_STABILITY_WINDOW];
    uint32_t coherence_history_index;
    uint32_t coherence_samples;

    /* Auto feedback state */
    bool auto_feedback_enabled;

    /* Initialization flag */
    bool initialized;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(elig_uq_bridge, struct elig_uq_bridge_struct)

/*=============================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

static float lerpf(float a, float b, float t) {
    return a + t * (b - a);
}

/*=============================================================================
 * CONFIGURATION API
 *===========================================================================*/

elig_uq_bridge_config_t elig_uq_bridge_default_config(void) {
    elig_uq_bridge_config_t config = {
        /* Forward triggers */
        .enable_metric_triggers = true,
        .metric_trigger_threshold = ELIG_UQ_METRIC_TRIGGER_THRESHOLD,
        .ltp_ltd_ratio_min = ELIG_UQ_LTP_LTD_RATIO_MIN,
        .ltp_ltd_ratio_max = ELIG_UQ_LTP_LTD_RATIO_MAX,
        .pool_exhaustion_threshold = ELIG_UQ_POOL_EXHAUSTION_THRESHOLD,
        .bottleneck_escalation_threshold = ELIG_UQ_BOTTLENECK_ESCALATION,
        .history_min_samples = ELIG_UQ_HISTORY_MIN_SAMPLES,
        .latency_spike_threshold_us = ELIG_UQ_LATENCY_SPIKE_US,

        /* Backward feedback */
        .enable_credit_feedback = true,
        .enable_param_feedback = true,
        .enable_diffusion_feedback = true,
        .enable_step_feedback = true,
        .credit_metrics_weight = ELIG_UQ_CREDIT_METRICS_WEIGHT,
        .param_integration_rate = ELIG_UQ_PARAM_INTEGRATION_RATE,
        .diffusion_priority_scale = ELIG_UQ_DIFFUSION_PRIORITY_SCALE,
        .step_adjustment_max = ELIG_UQ_STEP_ADJUSTMENT_MAX,

        /* Feedback loop control */
        .enable_auto_feedback_loop = true,
        .feedback_loop_interval_ms = 100,
        .coherence_threshold = ELIG_UQ_MIN_COHERENCE,
        .stability_window = ELIG_UQ_STABILITY_WINDOW,

        /* Bio-async */
        .enable_bio_async = false
    };
    return config;
}

bool elig_uq_bridge_validate_config(const elig_uq_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_bridge_validate_config: config is NULL");
        return false;
    }

    /* Validate ratio bounds */
    if (config->ltp_ltd_ratio_min < 0.0f) return false;
    if (config->ltp_ltd_ratio_max <= config->ltp_ltd_ratio_min) {
        return false;
    }

    /* Validate thresholds */
    if (config->pool_exhaustion_threshold < 0.0f ||
        config->pool_exhaustion_threshold > 1.0f) return false;
    if (config->bottleneck_escalation_threshold < 0.0f ||
        config->bottleneck_escalation_threshold > 1.0f) return false;

    /* Validate feedback rates */
    if (config->param_integration_rate < 0.0f ||
        config->param_integration_rate > 1.0f) return false;
    if (config->credit_metrics_weight < 0.0f ||
        config->credit_metrics_weight > 1.0f) return false;

    return true;
}

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

elig_uq_bridge_t elig_uq_bridge_create(const elig_uq_bridge_config_t* config) {
    struct elig_uq_bridge_struct* bridge =
        nimcp_calloc(1, sizeof(struct elig_uq_bridge_struct));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "elig_uq_bridge_create: bridge allocation failed");
        return NULL;
    }

    if (bridge_base_init(&bridge->base, 0, "eligibility_utils_quantum") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "elig_uq_bridge_create: bridge_base_init failed");
        return NULL;
    }

    bridge->config = config ? *config : elig_uq_bridge_default_config();

    if (!elig_uq_bridge_validate_config(&bridge->config)) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_bridge_create: elig_uq_bridge_validate_config is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->state.utils_quantum_coherence = 1.0f;
    bridge->state.stability_metric = 1.0f;
    bridge->state.current_optimized_dt = ELIG_INTEGRATION_DT;
    bridge->state.current_optimized_tolerance = 0.001f;

    /* Initialize statistics */
    bridge->stats.min_coherence = 1.0f;
    bridge->stats.max_coherence = 1.0f;
    bridge->stats.avg_coherence = 1.0f;

    /* Initialize history */
    for (uint32_t i = 0; i < ELIG_UQ_STABILITY_WINDOW; i++) {
        bridge->coherence_history[i] = 1.0f;
    }

    bridge->auto_feedback_enabled = bridge->config.enable_auto_feedback_loop;
    bridge->previous_energy = INFINITY;
    bridge->initialized = true;

    NIMCP_LOGGING_INFO("Created %s bridge", "eligibility_utils_quantum");
    return bridge;
}

void elig_uq_bridge_destroy(elig_uq_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_bridge_destroy: bridge is NULL");
        return;
    }
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "eligibility_utils_quantum");
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int elig_uq_bridge_attach_utils(elig_uq_bridge_t bridge,
                                eligibility_utils_ctx_t utils_ctx) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_bridge_attach_utils: bridge is NULL");
        return -1;
    }
    if (!utils_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_bridge_attach_utils: utils_ctx is NULL");
        return -1;
    }
    bridge->utils_ctx = utils_ctx;
    return 0;
}

int elig_uq_bridge_attach_quantum(elig_uq_bridge_t bridge,
                                  eligibility_quantum_ctx_t quantum_ctx) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_bridge_attach_quantum: bridge is NULL");
        return -1;
    }
    if (!quantum_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_bridge_attach_quantum: quantum_ctx is NULL");
        return -1;
    }
    bridge->quantum_ctx = quantum_ctx;
    return 0;
}

bool elig_uq_bridge_is_connected(const elig_uq_bridge_t bridge) {
    return bridge && bridge->utils_ctx && bridge->quantum_ctx;
}

void elig_uq_bridge_reset(elig_uq_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_bridge_reset: bridge is NULL");
        return;
    }

    /* Reset state */
    memset(&bridge->state, 0, sizeof(bridge->state));
    bridge->state.utils_quantum_coherence = 1.0f;
    bridge->state.stability_metric = 1.0f;
    bridge->state.current_optimized_dt = ELIG_INTEGRATION_DT;
    bridge->state.current_optimized_tolerance = 0.001f;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.min_coherence = 1.0f;
    bridge->stats.max_coherence = 1.0f;
    bridge->stats.avg_coherence = 1.0f;

    /* Reset history */
    for (uint32_t i = 0; i < ELIG_UQ_STABILITY_WINDOW; i++) {
        bridge->coherence_history[i] = 1.0f;
    }
    bridge->coherence_history_index = 0;
    bridge->coherence_samples = 0;

    bridge->previous_energy = INFINITY;
}

/*=============================================================================
 * FORWARD DIRECTION: Utils -> Quantum
 *===========================================================================*/

int elig_uq_evaluate_metrics(elig_uq_bridge_t bridge,
                             elig_uq_forward_effect_t* effect) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_evaluate_metrics: bridge is NULL");
        return -1;
    }
    if (!effect) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_evaluate_metrics: effect is NULL");
        return -1;
    }

    memset(effect, 0, sizeof(*effect));
    effect->timestamp_ms = get_time_ms();

    /* Get metrics from utils if connected */
    eligibility_metrics_t metrics = {0};
    if (bridge->utils_ctx) {
        eligibility_utils_get_metrics(bridge->utils_ctx, &metrics);
    }

    /* Get pool stats if connected */
    uint32_t total = 0, used = 0, free_count = 0;
    if (bridge->utils_ctx) {
        eligibility_utils_pool_stats(bridge->utils_ctx, &total, &used, &free_count);
    }

    /* Populate effect with current metrics */
    effect->mean_trace_value = metrics.mean_trace_value;
    effect->information_efficiency = metrics.information_efficiency;
    effect->bottleneck_count = metrics.bottleneck_count;
    effect->avg_latency_us = metrics.avg_update_latency_us;

    /* Compute LTP/LTD ratio from trace histogram */
    uint32_t ltp_count = 0, ltd_count = 0;
    for (int i = 10; i < 20; i++) ltp_count += metrics.trace_histogram[i];
    for (int i = 0; i < 10; i++) ltd_count += metrics.trace_histogram[i];

    effect->ltp_ltd_ratio = (ltd_count > 0) ?
        (float)ltp_count / (float)ltd_count : 10.0f;

    /* Pool state */
    effect->pool_utilization = (total > 0) ? (float)used / (float)total : 0.0f;
    effect->pool_free_count = free_count;

    /* Integration state */
    effect->current_integration_dt = bridge->state.current_optimized_dt;
    effect->adaptive_error = bridge->state.current_optimized_tolerance;

    /* Update bridge state */
    bridge->state.current_ltp_ltd_ratio = effect->ltp_ltd_ratio;
    bridge->state.current_pool_utilization = effect->pool_utilization;
    bridge->state.current_info_efficiency = effect->information_efficiency;

    /* Check triggers */
    const elig_uq_bridge_config_t* cfg = &bridge->config;
    effect->trigger_type = ELIG_UQ_TRIGGER_NONE;

    if (cfg->enable_metric_triggers) {
        /* LTP/LTD imbalance trigger */
        if (effect->ltp_ltd_ratio < cfg->ltp_ltd_ratio_min ||
            effect->ltp_ltd_ratio > cfg->ltp_ltd_ratio_max) {
            effect->trigger_type = ELIG_UQ_TRIGGER_LTP_LTD_IMBALANCE;
            effect->request_annealing = true;
            bridge->stats.ltp_ltd_triggers++;
        }

        /* Pool pressure trigger */
        if (effect->pool_utilization > cfg->pool_exhaustion_threshold) {
            effect->trigger_type = ELIG_UQ_TRIGGER_POOL_PRESSURE;
            effect->request_quantum_walk = true;
            bridge->stats.pool_triggers++;
        }

        /* Bottleneck escalation trigger */
        if (effect->bottleneck_count > 0 &&
            (1.0f - effect->information_efficiency) > cfg->bottleneck_escalation_threshold) {
            effect->trigger_type = ELIG_UQ_TRIGGER_BOTTLENECK;
            effect->request_quantum_shannon = true;
            bridge->stats.bottleneck_triggers++;
        }

        /* Latency spike trigger */
        if (effect->avg_latency_us > cfg->latency_spike_threshold_us) {
            effect->trigger_type = ELIG_UQ_TRIGGER_LATENCY_SPIKE;
            effect->request_annealing = true;
            bridge->stats.latency_triggers++;
        }
    }

    bridge->state.last_forward_ms = effect->timestamp_ms;
    bridge->stats.total_forward_events++;

    return 0;
}

int elig_uq_notify_ltp_ltd_imbalance(elig_uq_bridge_t bridge,
                                     uint64_t ltp_count,
                                     uint64_t ltd_count,
                                     elig_uq_forward_effect_t* effect) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_notify_ltp_ltd_imbalance: bridge is NULL");
        return -1;
    }
    if (!effect) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_notify_ltp_ltd_imbalance: effect is NULL");
        return -1;
    }

    memset(effect, 0, sizeof(*effect));
    effect->timestamp_ms = get_time_ms();

    effect->ltp_ltd_ratio = (ltd_count > 0) ?
        (float)ltp_count / (float)ltd_count : 10.0f;

    const elig_uq_bridge_config_t* cfg = &bridge->config;

    if (effect->ltp_ltd_ratio < cfg->ltp_ltd_ratio_min ||
        effect->ltp_ltd_ratio > cfg->ltp_ltd_ratio_max) {
        effect->trigger_type = ELIG_UQ_TRIGGER_LTP_LTD_IMBALANCE;
        effect->request_annealing = true;
        bridge->stats.ltp_ltd_triggers++;
    }

    bridge->state.current_ltp_ltd_ratio = effect->ltp_ltd_ratio;
    bridge->state.last_forward_ms = effect->timestamp_ms;
    bridge->stats.total_forward_events++;

    return 0;
}

int elig_uq_notify_pool_pressure(elig_uq_bridge_t bridge,
                                 float utilization,
                                 uint32_t free_count,
                                 elig_uq_forward_effect_t* effect) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_notify_pool_pressure: bridge is NULL");
        return -1;
    }
    if (!effect) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_notify_pool_pressure: effect is NULL");
        return -1;
    }

    memset(effect, 0, sizeof(*effect));
    effect->timestamp_ms = get_time_ms();

    effect->pool_utilization = utilization;
    effect->pool_free_count = free_count;

    if (utilization > bridge->config.pool_exhaustion_threshold) {
        effect->trigger_type = ELIG_UQ_TRIGGER_POOL_PRESSURE;
        effect->request_quantum_walk = true;
        bridge->stats.pool_triggers++;
    }

    bridge->state.current_pool_utilization = utilization;
    bridge->state.last_forward_ms = effect->timestamp_ms;
    bridge->stats.total_forward_events++;

    return 0;
}

int elig_uq_escalate_bottleneck(elig_uq_bridge_t bridge,
                                const eligibility_bottleneck_t* bottlenecks,
                                uint32_t num_bottlenecks,
                                elig_uq_forward_effect_t* effect) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_escalate_bottleneck: bridge is NULL");
        return -1;
    }
    if (!effect) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_escalate_bottleneck: effect is NULL");
        return -1;
    }

    memset(effect, 0, sizeof(*effect));
    effect->timestamp_ms = get_time_ms();

    effect->bottleneck_count = num_bottlenecks;

    /* Compute average deficit */
    float avg_deficit = 0.0f;
    if (bottlenecks && num_bottlenecks > 0) {
        for (uint32_t i = 0; i < num_bottlenecks; i++) {
            avg_deficit += bottlenecks[i].information_deficit;
        }
        avg_deficit /= num_bottlenecks;
    }

    if (avg_deficit > bridge->config.bottleneck_escalation_threshold) {
        effect->trigger_type = ELIG_UQ_TRIGGER_BOTTLENECK;
        effect->request_quantum_shannon = true;
        bridge->stats.bottleneck_triggers++;
    }

    bridge->state.last_forward_ms = effect->timestamp_ms;
    bridge->stats.total_forward_events++;

    return 0;
}

int elig_uq_provide_history(elig_uq_bridge_t bridge,
                            const eligibility_trace_t* traces,
                            uint32_t num_samples,
                            elig_uq_forward_effect_t* effect) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_provide_history: bridge is NULL");
        return -1;
    }
    if (!effect) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_provide_history: effect is NULL");
        return -1;
    }

    memset(effect, 0, sizeof(*effect));
    effect->timestamp_ms = get_time_ms();

    effect->history_samples = num_samples;

    /* Compute variance of history */
    if (traces && num_samples > 1) {
        float mean = 0.0f;
        for (uint32_t i = 0; i < num_samples; i++) {
            mean += traces[i].trace;
        }
        mean /= num_samples;

        float variance = 0.0f;
        for (uint32_t i = 0; i < num_samples; i++) {
            float diff = traces[i].trace - mean;
            variance += diff * diff;
        }
        variance /= (num_samples - 1);
        effect->history_variance = variance;
    }

    if (num_samples >= bridge->config.history_min_samples) {
        effect->trigger_type = ELIG_UQ_TRIGGER_HISTORY_READY;
        effect->request_quantum_walk = true;
        bridge->stats.history_triggers++;
    }

    bridge->state.last_forward_ms = effect->timestamp_ms;
    bridge->stats.total_forward_events++;

    return 0;
}

int elig_uq_request_optimization(elig_uq_bridge_t bridge,
                                 const elig_quantum_params_t* current_params,
                                 elig_uq_forward_effect_t* effect) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_request_optimization: bridge is NULL");
        return -1;
    }
    if (!effect) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_request_optimization: effect is NULL");
        return -1;
    }

    memset(effect, 0, sizeof(*effect));
    effect->timestamp_ms = get_time_ms();
    effect->trigger_type = ELIG_UQ_TRIGGER_METRIC_ANOMALY;
    effect->request_annealing = true;

    bridge->stats.metric_triggers++;
    bridge->state.last_forward_ms = effect->timestamp_ms;
    bridge->stats.total_forward_events++;

    return 0;
}

/*=============================================================================
 * BACKWARD DIRECTION: Quantum -> Utils
 *===========================================================================*/

int elig_uq_apply_credit_feedback(elig_uq_bridge_t bridge,
                                  const elig_quantum_credit_t* credits,
                                  uint32_t num_credits,
                                  elig_uq_backward_effect_t* effect) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_apply_credit_feedback: bridge is NULL");
        return -1;
    }
    if (!effect) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_apply_credit_feedback: effect is NULL");
        return -1;
    }

    uint64_t start_us = get_time_us();
    memset(effect, 0, sizeof(*effect));
    effect->feedback_type = ELIG_UQ_FEEDBACK_CREDIT_ASSIGNMENT;

    if (credits && num_credits > 0) {
        /* Compute average credit statistics */
        float sum_credit = 0.0f;
        float sum_confidence = 0.0f;

        for (uint32_t i = 0; i < num_credits; i++) {
            sum_credit += credits[i].credit_fraction;
            sum_confidence += credits[i].confidence;
        }

        effect->avg_credit_fraction = sum_credit / num_credits;
        effect->credit_confidence = sum_confidence / num_credits;

        /* Compute entropy of credit distribution */
        float entropy = 0.0f;
        for (uint32_t i = 0; i < num_credits; i++) {
            float p = credits[i].credit_fraction;
            if (p > 1e-10f) {
                entropy -= p * log2f(p);
            }
        }
        effect->credit_entropy = entropy;

        /* Update cumulative credit feedback */
        bridge->state.cumulative_credit_feedback =
            lerpf(bridge->state.cumulative_credit_feedback,
                  effect->avg_credit_fraction,
                  bridge->config.credit_metrics_weight);
    }

    effect->processing_time_us = get_time_us() - start_us;
    effect->timestamp_ms = get_time_ms();

    bridge->state.last_backward_ms = effect->timestamp_ms;
    bridge->stats.credit_feedbacks++;
    bridge->stats.total_backward_events++;

    return 0;
}

int elig_uq_apply_param_feedback(elig_uq_bridge_t bridge,
                                 const elig_quantum_params_t* optimized,
                                 elig_uq_backward_effect_t* effect) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_apply_param_feedback: bridge is NULL");
        return -1;
    }
    if (!optimized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_apply_param_feedback: optimized is NULL");
        return -1;
    }
    if (!effect) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_apply_param_feedback: effect is NULL");
        return -1;
    }

    uint64_t start_us = get_time_us();
    memset(effect, 0, sizeof(*effect));
    effect->feedback_type = ELIG_UQ_FEEDBACK_PARAM_OPTIMIZATION;

    /* Store optimized parameters */
    effect->optimized_tau_fast = optimized->tau_fast;
    effect->optimized_tau_slow = optimized->tau_slow;
    effect->optimized_learning_rate = optimized->learning_rate;
    effect->optimization_energy = optimized->energy;

    /* Compute recommended integration timestep */
    /* Smaller tau -> need smaller dt for accuracy */
    float min_tau = fminf(optimized->tau_fast, optimized->tau_slow);
    if (min_tau < 1e-6f) min_tau = 1e-6f;

    float new_dt = fminf(0.1f * min_tau, ELIG_INTEGRATION_DT);
    effect->recommended_dt = new_dt;

    /* Adjust tolerance based on optimization energy */
    /* Lower energy (better optimization) -> tighter tolerance */
    float energy_factor = expf(-optimized->energy / 10.0f);
    effect->recommended_tolerance = 0.001f * (2.0f - energy_factor);
    effect->recommended_tolerance = nimcp_clampf(effect->recommended_tolerance, 0.0001f, 0.01f);

    /* Blend into bridge state */
    float rate = bridge->config.param_integration_rate;
    bridge->state.current_optimized_dt =
        lerpf(bridge->state.current_optimized_dt, effect->recommended_dt, rate);
    bridge->state.current_optimized_tolerance =
        lerpf(bridge->state.current_optimized_tolerance, effect->recommended_tolerance, rate);

    /* Update coherence based on improvement */
    float improvement = bridge->previous_energy - optimized->energy;
    if (improvement > 0 && isfinite(improvement)) {
        float coherence_boost = fminf(0.1f * improvement, 0.2f);
        bridge->state.utils_quantum_coherence =
            fminf(1.0f, bridge->state.utils_quantum_coherence + coherence_boost);
        bridge->stats.successful_optimizations++;

        /* Track improvement */
        bridge->stats.avg_optimization_improvement =
            lerpf(bridge->stats.avg_optimization_improvement, improvement, 0.1f);
    }

    bridge->previous_energy = optimized->energy;

    effect->processing_time_us = get_time_us() - start_us;
    effect->timestamp_ms = get_time_ms();

    bridge->state.last_backward_ms = effect->timestamp_ms;
    bridge->stats.param_feedbacks++;
    bridge->stats.total_backward_events++;

    return 0;
}

int elig_uq_apply_diffusion_feedback(elig_uq_bridge_t bridge,
                                     const float* diffused_eligibility,
                                     uint32_t num_synapses,
                                     elig_uq_backward_effect_t* effect) {
    BRIDGE_BBB_VALIDATE(bridge, diffused_eligibility, sizeof(*diffused_eligibility));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_apply_diffusion_feedback: bridge is NULL");
        return -1;
    }
    if (!effect) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_apply_diffusion_feedback: effect is NULL");
        return -1;
    }

    uint64_t start_us = get_time_us();
    memset(effect, 0, sizeof(*effect));
    effect->feedback_type = ELIG_UQ_FEEDBACK_DIFFUSION_RESULT;

    if (diffused_eligibility && num_synapses > 0) {
        /* Compute diffusion statistics */
        float max_val = 0.0f;
        float sum_val = 0.0f;

        for (uint32_t i = 0; i < num_synapses; i++) {
            if (diffused_eligibility[i] > max_val) {
                max_val = diffused_eligibility[i];
            }
            sum_val += diffused_eligibility[i];
        }

        effect->max_diffused_priority = max_val;
        effect->mean_diffused_priority = sum_val / num_synapses;

        /* Estimate speedup (sqrt(N) theoretical) */
        effect->diffusion_speedup = sqrtf((float)num_synapses);
        bridge->stats.cumulative_speedup += effect->diffusion_speedup;

        /* Update priority factor */
        bridge->state.current_diffusion_priority =
            lerpf(bridge->state.current_diffusion_priority,
                  max_val * bridge->config.diffusion_priority_scale,
                  0.2f);
    }

    effect->processing_time_us = get_time_us() - start_us;
    effect->timestamp_ms = get_time_ms();

    bridge->state.last_backward_ms = effect->timestamp_ms;
    bridge->stats.diffusion_feedbacks++;
    bridge->stats.total_backward_events++;

    return 0;
}

int elig_uq_apply_step_feedback(elig_uq_bridge_t bridge,
                                const elig_quantum_anneal_state_t* anneal_state,
                                elig_uq_backward_effect_t* effect) {
    BRIDGE_BBB_VALIDATE(bridge, anneal_state, sizeof(*anneal_state));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_apply_step_feedback: bridge is NULL");
        return -1;
    }
    if (!anneal_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_apply_step_feedback: anneal_state is NULL");
        return -1;
    }
    if (!effect) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_apply_step_feedback: effect is NULL");
        return -1;
    }

    uint64_t start_us = get_time_us();
    memset(effect, 0, sizeof(*effect));
    effect->feedback_type = ELIG_UQ_FEEDBACK_STEP_SIZE;

    /* Use temperature to guide step size */
    /* High temperature -> larger step (exploration) */
    /* Low temperature -> smaller step (exploitation) */
    float temp_factor = nimcp_clampf(anneal_state->temperature / 10.0f, 0.1f, 2.0f);
    effect->recommended_dt = ELIG_INTEGRATION_DT * temp_factor;
    effect->recommended_dt = nimcp_clampf(effect->recommended_dt,
                                    ELIG_INTEGRATION_DT * 0.1f,
                                    ELIG_INTEGRATION_DT * 2.0f);

    /* Tunneling events -> loosen tolerance */
    float tunnel_factor = 1.0f + 0.1f * (float)anneal_state->tunneling_events;
    tunnel_factor = fminf(tunnel_factor, 2.0f);
    effect->recommended_tolerance = 0.001f * tunnel_factor;

    /* Apply with max adjustment limit */
    float max_adj = bridge->config.step_adjustment_max;
    float dt_change = effect->recommended_dt - bridge->state.current_optimized_dt;
    dt_change = nimcp_clampf(dt_change,
                       -max_adj * bridge->state.current_optimized_dt,
                       max_adj * bridge->state.current_optimized_dt);

    bridge->state.current_optimized_dt += dt_change;
    bridge->state.current_optimized_tolerance = effect->recommended_tolerance;

    effect->processing_time_us = get_time_us() - start_us;
    effect->timestamp_ms = get_time_ms();

    bridge->state.last_backward_ms = effect->timestamp_ms;
    bridge->stats.step_feedbacks++;
    bridge->stats.total_backward_events++;

    return 0;
}

int elig_uq_get_integration_params(elig_uq_bridge_t bridge,
                                   float* dt,
                                   float* tolerance) {
    BRIDGE_BBB_VALIDATE(bridge, dt, sizeof(*dt));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_get_integration_params: bridge is NULL");
        return -1;
    }

    if (dt) *dt = bridge->state.current_optimized_dt;
    if (tolerance) *tolerance = bridge->state.current_optimized_tolerance;

    return 0;
}

/*=============================================================================
 * FEEDBACK LOOP API
 *===========================================================================*/

int elig_uq_feedback_loop_tick(elig_uq_bridge_t bridge,
                               eligibility_trace_t* traces,
                               float* weights,
                               uint32_t num_synapses,
                               elig_uq_forward_effect_t* forward_effect,
                               elig_uq_backward_effect_t* backward_effect) {
    BRIDGE_BBB_VALIDATE(bridge, weights, sizeof(*weights));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_feedback_loop_tick: bridge is NULL");
        return -1;
    }

    uint64_t start_us = get_time_us();

    elig_uq_forward_effect_t fwd = {0};
    elig_uq_backward_effect_t bwd = {0};

    /* === FORWARD PHASE: Utils -> Quantum === */

    /* 1. Evaluate current metrics */
    if (elig_uq_evaluate_metrics(bridge, &fwd) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return -1;
    }

    /* 2. Execute requested quantum operations if quantum context attached */
    if (bridge->quantum_ctx && traces && num_synapses > 0) {
        elig_quantum_params_t current_params = {
            .tau_fast = 10.0f,
            .tau_slow = 100.0f,
            .learning_rate = NIMCP_LEARNING_RATE_FINE,
            .dopamine_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
            .burst_threshold = 0.5f,
            .consolidation_threshold = 0.3f,
            .energy = INFINITY,
            .amplitude = 1.0f
        };

        elig_quantum_params_t optimized_params = current_params;

        /* Execute annealing if requested */
        if (fwd.request_annealing) {
            elig_quantum_optimize_params(bridge->quantum_ctx,
                                        traces, num_synapses,
                                        &current_params, &optimized_params);
            elig_uq_apply_param_feedback(bridge, &optimized_params, &bwd);
        }

        /* Execute credit assignment if requested */
        if (fwd.request_qmc_credit) {
            elig_quantum_credit_t credits[100];
            uint32_t num_credits = 0;

            elig_quantum_assign_credit(bridge->quantum_ctx,
                                      traces, num_synapses,
                                      1.0f, /* reward */
                                      credits, 100, &num_credits);

            if (num_credits > 0) {
                elig_uq_apply_credit_feedback(bridge, credits, num_credits, &bwd);
            }
        }

        /* Execute quantum walk if requested */
        if (fwd.request_quantum_walk && num_synapses > 0) {
            float* diffused = nimcp_malloc(num_synapses * sizeof(float));
            if (diffused) {
                elig_quantum_diffuse(bridge->quantum_ctx,
                                    0, traces[0].trace,
                                    NULL, num_synapses, diffused);
                elig_uq_apply_diffusion_feedback(bridge, diffused, num_synapses, &bwd);
                nimcp_free(diffused);
            }
        }

        /* Execute quantum-Shannon if requested */
        if (fwd.request_quantum_shannon && weights) {
            elig_quantum_bottleneck_t bottlenecks[10];
            uint32_t num_found = 0;

            elig_quantum_detect_bottlenecks(bridge->quantum_ctx,
                                           traces, weights, num_synapses,
                                           bottlenecks, 10, &num_found);

            if (num_found > 0) {
                bwd.feedback_type = ELIG_UQ_FEEDBACK_BOTTLENECK_RESOLVED;
                bwd.bottlenecks_resolved = num_found;
                bridge->stats.bottleneck_resolutions += num_found;
            }
        }
    }

    /* === Update coherence === */
    bridge->state.utils_quantum_coherence *= ELIG_UQ_COHERENCE_DECAY;
    bridge->state.utils_quantum_coherence =
        fmaxf(bridge->state.utils_quantum_coherence, ELIG_UQ_MIN_COHERENCE);

    /* Update coherence history */
    bridge->coherence_history[bridge->coherence_history_index] =
        bridge->state.utils_quantum_coherence;
    bridge->coherence_history_index =
        (bridge->coherence_history_index + 1) % ELIG_UQ_STABILITY_WINDOW;
    if (bridge->coherence_samples < ELIG_UQ_STABILITY_WINDOW) {
        bridge->coherence_samples++;
    }

    /* Update coherence statistics */
    if (bridge->state.utils_quantum_coherence < bridge->stats.min_coherence) {
        bridge->stats.min_coherence = bridge->state.utils_quantum_coherence;
    }
    if (bridge->state.utils_quantum_coherence > bridge->stats.max_coherence) {
        bridge->stats.max_coherence = bridge->state.utils_quantum_coherence;
    }
    bridge->stats.avg_coherence =
        lerpf(bridge->stats.avg_coherence, bridge->state.utils_quantum_coherence, 0.01f);

    /* Compute stability */
    if (bridge->coherence_samples >= 2) {
        float variance = 0.0f;
        float mean = 0.0f;
        for (uint32_t i = 0; i < bridge->coherence_samples; i++) {
            mean += bridge->coherence_history[i];
        }
        mean /= bridge->coherence_samples;
        for (uint32_t i = 0; i < bridge->coherence_samples; i++) {
            float diff = bridge->coherence_history[i] - mean;
            variance += diff * diff;
        }
        variance /= (bridge->coherence_samples - 1);
        /* Stability = 1 - normalized variance */
        bridge->state.stability_metric = 1.0f / (1.0f + sqrtf(variance) * 10.0f);
    }

    /* Update timing statistics */
    uint64_t round_trip_us = get_time_us() - start_us;
    bridge->stats.avg_round_trip_us =
        lerpf(bridge->stats.avg_round_trip_us, (double)round_trip_us, 0.1);

    bridge->state.last_feedback_loop_ms = get_time_ms();
    bridge->stats.feedback_loop_iterations++;

    /* Copy results if requested */
    if (forward_effect) *forward_effect = fwd;
    if (backward_effect) *backward_effect = bwd;

    return 0;
}

void elig_uq_set_auto_feedback(elig_uq_bridge_t bridge, bool enabled) {
    if (bridge) {
        bridge->auto_feedback_enabled = enabled;
    }
}

bool elig_uq_is_auto_feedback_enabled(const elig_uq_bridge_t bridge) {
    return bridge && bridge->auto_feedback_enabled;
}

float elig_uq_get_coherence(const elig_uq_bridge_t bridge) {
    if (!bridge) return -1.0f;
    return bridge->state.utils_quantum_coherence;
}

float elig_uq_get_stability(const elig_uq_bridge_t bridge) {
    if (!bridge) return -1.0f;
    return bridge->state.stability_metric;
}

/*=============================================================================
 * STATE AND STATISTICS API
 *===========================================================================*/

int elig_uq_bridge_get_state(const elig_uq_bridge_t bridge,
                             elig_uq_bridge_state_t* state) {
    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_bridge_get_state: bridge is NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_bridge_get_state: state is NULL");
        return -1;
    }
    *state = bridge->state;
    return 0;
}

int elig_uq_bridge_get_stats(const elig_uq_bridge_t bridge,
                             elig_uq_bridge_stats_t* stats) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_bridge_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_bridge_get_stats: stats is NULL");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int elig_uq_bridge_reset_stats(elig_uq_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_bridge_reset_stats: bridge is NULL");
        return -1;
    }

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.min_coherence = bridge->state.utils_quantum_coherence;
    bridge->stats.max_coherence = bridge->state.utils_quantum_coherence;
    bridge->stats.avg_coherence = bridge->state.utils_quantum_coherence;

    return 0;
}

int elig_uq_bridge_update(elig_uq_bridge_t bridge, float dt_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_bridge_update: bridge is NULL");
        return -1;
    }

    /* Decay coherence over time */
    float decay_factor = powf(ELIG_UQ_COHERENCE_DECAY, dt_ms);
    bridge->state.utils_quantum_coherence *= decay_factor;
    bridge->state.utils_quantum_coherence =
        fmaxf(bridge->state.utils_quantum_coherence, ELIG_UQ_MIN_COHERENCE);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

void elig_uq_bridge_print_summary(const elig_uq_bridge_t bridge) {
    if (!bridge) {
        printf("Eligibility Utils-Quantum Bridge: NULL\n");
        return;
    }

    printf("=== Eligibility Utils-Quantum Bridge Summary ===\n");
    printf("Connected: utils=%s, quantum=%s\n",
           bridge->utils_ctx ? "yes" : "no",
           bridge->quantum_ctx ? "yes" : "no");
    printf("\nState:\n");
    printf("  Coherence: %.3f\n", bridge->state.utils_quantum_coherence);
    printf("  Stability: %.3f\n", bridge->state.stability_metric);
    printf("  LTP/LTD ratio: %.3f\n", bridge->state.current_ltp_ltd_ratio);
    printf("  Pool utilization: %.1f%%\n", bridge->state.current_pool_utilization * 100);
    printf("  Optimized dt: %.6f\n", bridge->state.current_optimized_dt);
    printf("  Optimized tolerance: %.6f\n", bridge->state.current_optimized_tolerance);

    printf("\nForward Statistics:\n");
    printf("  Total events: %lu\n", (unsigned long)bridge->stats.total_forward_events);
    printf("  LTP/LTD triggers: %lu\n", (unsigned long)bridge->stats.ltp_ltd_triggers);
    printf("  Pool triggers: %lu\n", (unsigned long)bridge->stats.pool_triggers);
    printf("  Bottleneck triggers: %lu\n", (unsigned long)bridge->stats.bottleneck_triggers);

    printf("\nBackward Statistics:\n");
    printf("  Total events: %lu\n", (unsigned long)bridge->stats.total_backward_events);
    printf("  Credit feedbacks: %lu\n", (unsigned long)bridge->stats.credit_feedbacks);
    printf("  Param feedbacks: %lu\n", (unsigned long)bridge->stats.param_feedbacks);
    printf("  Diffusion feedbacks: %lu\n", (unsigned long)bridge->stats.diffusion_feedbacks);
    printf("  Successful optimizations: %lu\n", (unsigned long)bridge->stats.successful_optimizations);

    printf("\nFeedback Loop:\n");
    printf("  Iterations: %lu\n", (unsigned long)bridge->stats.feedback_loop_iterations);
    printf("  Avg round-trip: %.2f us\n", bridge->stats.avg_round_trip_us);
    printf("  Coherence range: [%.3f, %.3f], avg: %.3f\n",
           bridge->stats.min_coherence, bridge->stats.max_coherence,
           bridge->stats.avg_coherence);
    printf("================================================\n");
}

/*=============================================================================
 * DIAGNOSTIC API
 *===========================================================================*/

bool elig_uq_bridge_verify(const elig_uq_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_bridge_verify: bridge is NULL");
        return false;
    }
    if (!bridge->initialized) return false;

    /* Check coherence bounds */
    if (bridge->state.utils_quantum_coherence < 0.0f ||
        bridge->state.utils_quantum_coherence > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_bridge_verify: bridge->initialized is NULL");
        return false;
    }

    /* Check stability bounds */
    if (bridge->state.stability_metric < 0.0f ||
        bridge->state.stability_metric > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "elig_uq_bridge_verify: operation failed");
        return false;
    }

    /* Check dt is positive */
    if (bridge->state.current_optimized_dt <= 0.0f) {
        return false;
    }

    return true;
}

bool elig_uq_bridge_export_csv(const elig_uq_bridge_t bridge,
                               const char* filename) {
    BRIDGE_BBB_VALIDATE(bridge, filename, sizeof(*filename));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_bridge_export_csv: bridge is NULL");
        return false;
    }
    if (!filename) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_uq_bridge_export_csv: filename is NULL");
        return false;
    }

    FILE* f = fopen(filename, "w");
    if (!f) return false;

    fprintf(f, "metric,value\n");
    fprintf(f, "coherence,%.6f\n", bridge->state.utils_quantum_coherence);
    fprintf(f, "stability,%.6f\n", bridge->state.stability_metric);
    fprintf(f, "ltp_ltd_ratio,%.6f\n", bridge->state.current_ltp_ltd_ratio);
    fprintf(f, "pool_utilization,%.6f\n", bridge->state.current_pool_utilization);
    fprintf(f, "optimized_dt,%.6f\n", bridge->state.current_optimized_dt);
    fprintf(f, "optimized_tolerance,%.6f\n", bridge->state.current_optimized_tolerance);
    fprintf(f, "total_forward_events,%lu\n", (unsigned long)bridge->stats.total_forward_events);
    fprintf(f, "total_backward_events,%lu\n", (unsigned long)bridge->stats.total_backward_events);
    fprintf(f, "feedback_loop_iterations,%lu\n", (unsigned long)bridge->stats.feedback_loop_iterations);
    fprintf(f, "avg_round_trip_us,%.2f\n", bridge->stats.avg_round_trip_us);
    fprintf(f, "min_coherence,%.6f\n", bridge->stats.min_coherence);
    fprintf(f, "max_coherence,%.6f\n", bridge->stats.max_coherence);
    fprintf(f, "avg_coherence,%.6f\n", bridge->stats.avg_coherence);

    fclose(f);
    return true;
}
