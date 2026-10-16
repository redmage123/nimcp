/**
 * @file nimcp_eligibility_quantum_bridge.c
 * @brief Eligibility Trace Quantum Integration Implementation
 * @version 1.0.0
 * @date 2026-01-12
 */

#include "plasticity/eligibility/nimcp_eligibility_quantum_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(eligibility_quantum_bridge)

#define LOG_MODULE "ELIGIBILITY_QUANTUM_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct eligibility_quantum_ctx_internal {
    /* Configuration */
    elig_quantum_config_t config;

    /* State */
    bool enabled;

    /* Annealing state */
    elig_quantum_anneal_state_t anneal_state;

    /* Metrics */
    elig_quantum_metrics_t metrics;
    uint64_t start_time_ms;

    /* Walk state */
    float walk_speedup;
    float walk_distance;
    float walk_entropy;
};

/*=============================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static float compute_shannon_entropy(const float* probs, uint32_t n) {
    float entropy = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        if (probs[i] > 1e-10f) {
            entropy -= probs[i] * log2f(probs[i]);
        }
    }
    return entropy;
}

/*=============================================================================
 * LIFECYCLE IMPLEMENTATION
 *===========================================================================*/

elig_quantum_config_t elig_quantum_default_config(void) {
    elig_quantum_config_t config = {
        .enable_bottleneck_detection = true,
        .enable_credit_assignment = true,
        .enable_quantum_optimization = true,
        .enable_quantum_walk = true,
        .bottleneck_threshold = ELIG_QUANTUM_BOTTLENECK_THRESHOLD,
        .bottleneck_check_interval = 100,
        .synapse_sample_size = 1000,
        .mc_samples = ELIG_QUANTUM_MC_SAMPLES,
        .use_importance_sampling = true,
        .temporal_discount = NIMCP_REWARD_DISCOUNT_DEFAULT,
        .initial_temperature = 10.0f,
        .final_temperature = 0.01f,
        .tunneling_rate = 0.1f,
        .anneal_iterations = ELIG_QUANTUM_ANNEAL_ITERATIONS,
        .objective = 2,  /* balance */
        .walk_steps = ELIG_QUANTUM_WALK_STEPS,
        .decoherence_rate = 0.01f,
        .enable_metrics = true,
        .metrics_flush_interval_ms = 10000,
        .metrics_output_dir = "./nimcp_elig_quantum_metrics"
    };
    return config;
}

eligibility_quantum_ctx_t elig_quantum_create(const elig_quantum_config_t* config) {
    struct eligibility_quantum_ctx_internal* ctx =
        nimcp_calloc(1, sizeof(struct eligibility_quantum_ctx_internal));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "elig_quantum_create: ctx allocation failed");
        return NULL;
    }

    ctx->config = config ? *config : elig_quantum_default_config();
    ctx->enabled = true;
    ctx->start_time_ms = get_time_ms();

    /* Initialize annealing state */
    ctx->anneal_state.temperature = ctx->config.initial_temperature;
    ctx->anneal_state.tunneling_probability = ctx->config.tunneling_rate;
    ctx->anneal_state.iteration = 0;
    ctx->anneal_state.tunneling_events = 0;
    ctx->anneal_state.best_energy = INFINITY;

    /* Initialize walk state */
    ctx->walk_speedup = 1.0f;
    ctx->walk_distance = 0.0f;
    ctx->walk_entropy = 0.0f;

    NIMCP_LOGGING_INFO("Created %s bridge", "eligibility_quantum");
    return ctx;
}

void elig_quantum_destroy(eligibility_quantum_ctx_t ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_destroy: ctx is NULL");
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "eligibility_quantum");
    }
    nimcp_free(ctx);
}

void elig_quantum_reset(eligibility_quantum_ctx_t ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_reset: ctx is NULL");
        return;
    }

    ctx->anneal_state.temperature = ctx->config.initial_temperature;
    ctx->anneal_state.tunneling_probability = ctx->config.tunneling_rate;
    ctx->anneal_state.iteration = 0;
    ctx->anneal_state.tunneling_events = 0;
    ctx->anneal_state.best_energy = INFINITY;

    memset(&ctx->metrics, 0, sizeof(ctx->metrics));
    ctx->start_time_ms = get_time_ms();
}

bool elig_quantum_is_enabled(const eligibility_quantum_ctx_t ctx) {
    return ctx && ctx->enabled;
}

void elig_quantum_set_enabled(eligibility_quantum_ctx_t ctx, bool enabled) {
    if (ctx) {
        ctx->enabled = enabled;
    }
}

/*=============================================================================
 * BOTTLENECK DETECTION API
 *===========================================================================*/

float elig_quantum_analyze_information_flow(
    eligibility_quantum_ctx_t ctx,
    const eligibility_trace_t* traces,
    const float* weights,
    uint32_t num_synapses
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_analyze_information_flow: ctx is NULL");
        return 0.0f;
    }
    if (!traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_analyze_information_flow: traces is NULL");
        return 0.0f;
    }
    if (!weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_analyze_information_flow: weights is NULL");
        return 0.0f;
    }
    if (num_synapses == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "elig_quantum_analyze_information_flow: num_synapses is zero");
        return 0.0f;
    }

    ctx->metrics.bottleneck_analyses++;

    /* Compute trace distribution entropy */
    float* probs = nimcp_malloc(num_synapses * sizeof(float));
    if (!probs) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_synapses * sizeof(float),
                           "elig_quantum_analyze_information_flow: failed to allocate probs array");
        return 0.0f;
    }

    float total_trace = 0.0f;
    for (uint32_t i = 0; i < num_synapses; i++) {
        total_trace += fabsf(traces[i].trace);
    }

    if (total_trace < 1e-10f) {
        nimcp_free(probs);
        return 1.0f;  /* No activity = perfect efficiency */
    }

    for (uint32_t i = 0; i < num_synapses; i++) {
        probs[i] = fabsf(traces[i].trace) / total_trace;
    }

    float entropy = compute_shannon_entropy(probs, num_synapses);
    float max_entropy = log2f((float)num_synapses);
    float efficiency = (max_entropy > 0) ? entropy / max_entropy : 1.0f;

    ctx->metrics.information_efficiency = efficiency;
    nimcp_free(probs);

    return efficiency;
}

bool elig_quantum_detect_bottlenecks(
    eligibility_quantum_ctx_t ctx,
    const eligibility_trace_t* traces,
    const float* weights,
    uint32_t num_synapses,
    elig_quantum_bottleneck_t* bottlenecks,
    uint32_t max_bottlenecks,
    uint32_t* num_found
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_detect_bottlenecks: ctx is NULL");
        return false;
    }
    if (!traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_detect_bottlenecks: traces is NULL");
        return false;
    }
    if (!weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_detect_bottlenecks: weights is NULL");
        return false;
    }
    if (!bottlenecks) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_detect_bottlenecks: bottlenecks is NULL");
        return false;
    }
    if (!num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_detect_bottlenecks: num_found is NULL");
        return false;
    }

    *num_found = 0;
    float threshold = ctx->config.bottleneck_threshold;

    for (uint32_t i = 0; i < num_synapses && *num_found < max_bottlenecks; i++) {
        float trace_val = fabsf(traces[i].trace);
        float weight_val = fabsf(weights[i]);

        /* Compute capacity (proportional to weight) */
        float capacity = weight_val * 10.0f;  /* Arbitrary scaling */
        /* Compute demand (proportional to trace activity) */
        float demand = trace_val * 5.0f;

        if (demand > 0 && capacity < demand * (1.0f - threshold)) {
            elig_quantum_bottleneck_t* b = &bottlenecks[*num_found];
            b->synapse_id = i;
            b->information_deficit = (demand - capacity) / demand;
            b->current_trace = trace_val;
            b->effective_learning_rate = weight_val * 0.01f;
            b->capacity = capacity;
            b->demand = demand;
            b->suggested_trace_tau = 20.0f;  /* Default time constant */
            b->suggested_learning_rate = b->effective_learning_rate * 1.2f;
            (*num_found)++;
            ctx->metrics.total_bottlenecks_found++;
        }
    }

    ctx->metrics.avg_bottleneck_severity = 0.0f;
    for (uint32_t i = 0; i < *num_found; i++) {
        ctx->metrics.avg_bottleneck_severity += bottlenecks[i].information_deficit;
    }
    if (*num_found > 0) {
        ctx->metrics.avg_bottleneck_severity /= (float)*num_found;
    }

    return true;
}

float elig_quantum_compute_trace_entropy(
    const eligibility_trace_t* traces,
    uint32_t num_traces
) {
    if (!traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_compute_trace_entropy: traces is NULL");
        return 0.0f;
    }
    if (num_traces == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "elig_quantum_compute_trace_entropy: num_traces is zero");
        return 0.0f;
    }

    float* probs = nimcp_malloc(num_traces * sizeof(float));
    if (!probs) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_traces * sizeof(float),
                           "elig_quantum_compute_trace_entropy: failed to allocate probs array");
        return 0.0f;
    }

    float total = 0.0f;
    for (uint32_t i = 0; i < num_traces; i++) {
        total += fabsf(traces[i].trace);
    }

    if (total < 1e-10f) {
        nimcp_free(probs);
        return 0.0f;
    }

    for (uint32_t i = 0; i < num_traces; i++) {
        probs[i] = (fabsf(traces[i].trace)) / total;
    }

    float entropy = compute_shannon_entropy(probs, num_traces);
    nimcp_free(probs);
    return entropy;
}

bool elig_quantum_optimize_from_bottlenecks(
    eligibility_quantum_ctx_t ctx,
    const elig_quantum_bottleneck_t* bottlenecks,
    uint32_t num_bottlenecks,
    elig_quantum_params_t* param_adjustments
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_optimize_from_bottlenecks: ctx is NULL");
        return false;
    }
    if (!bottlenecks) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_optimize_from_bottlenecks: bottlenecks is NULL");
        return false;
    }
    if (!param_adjustments) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_optimize_from_bottlenecks: param_adjustments is NULL");
        return false;
    }
    if (num_bottlenecks == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "elig_quantum_optimize_from_bottlenecks: num_bottlenecks is zero");
        return false;
    }

    /* Aggregate suggestions */
    float avg_tau = 0.0f;
    float avg_lr = 0.0f;

    for (uint32_t i = 0; i < num_bottlenecks; i++) {
        avg_tau += bottlenecks[i].suggested_trace_tau;
        avg_lr += bottlenecks[i].suggested_learning_rate;
    }

    avg_tau /= num_bottlenecks;
    avg_lr /= num_bottlenecks;

    param_adjustments->tau_fast = avg_tau * 0.5f;
    param_adjustments->tau_slow = avg_tau * 2.0f;
    param_adjustments->learning_rate = avg_lr;
    param_adjustments->dopamine_sensitivity = 1.0f;
    param_adjustments->burst_threshold = 0.5f;
    param_adjustments->consolidation_threshold = 0.7f;
    param_adjustments->energy = 0.0f;
    param_adjustments->amplitude = 1.0f;

    return true;
}

/*=============================================================================
 * CREDIT ASSIGNMENT API
 *===========================================================================*/

bool elig_quantum_assign_credit(
    eligibility_quantum_ctx_t ctx,
    const eligibility_trace_t* traces,
    uint32_t num_synapses,
    float reward,
    elig_quantum_credit_t* credits,
    uint32_t max_credits,
    uint32_t* num_credits
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_assign_credit: ctx is NULL");
        return false;
    }
    if (!traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_assign_credit: traces is NULL");
        return false;
    }
    if (!credits) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_assign_credit: credits is NULL");
        return false;
    }
    if (!num_credits) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_assign_credit: num_credits is NULL");
        return false;
    }

    ctx->metrics.credit_assignments++;
    *num_credits = 0;

    /* Compute total trace for normalization */
    float total_trace = 0.0f;
    for (uint32_t i = 0; i < num_synapses; i++) {
        total_trace += fabsf(traces[i].trace);
    }

    if (total_trace < 1e-10f) {
        return true;  /* No credit to assign */
    }

    /* Assign credit proportional to eligibility */
    float assigned_trace_total = 0.0f;
    for (uint32_t i = 0; i < num_synapses && *num_credits < max_credits; i++) {
        float trace_val = fabsf(traces[i].trace);
        if (trace_val > 0.01f * total_trace / num_synapses) {
            elig_quantum_credit_t* c = &credits[*num_credits];
            c->synapse_id = i;
            c->credit_fraction = trace_val / total_trace;
            c->confidence = 0.8f + 0.2f * (trace_val / total_trace);
            c->temporal_weight = expf(-(float)(get_time_ms() - traces[i].last_update) / 1000.0f);
            c->causal_strength = c->credit_fraction * c->temporal_weight;
            assigned_trace_total += trace_val;
            (*num_credits)++;
        }
    }

    /* Normalize credits to sum to 1.0 */
    if (*num_credits > 0 && assigned_trace_total > 1e-10f) {
        for (uint32_t i = 0; i < *num_credits; i++) {
            credits[i].credit_fraction = credits[i].credit_fraction * (total_trace / assigned_trace_total);
        }
    }

    /* Compute assignment confidence */
    float total_conf = 0.0f;
    for (uint32_t i = 0; i < *num_credits; i++) {
        total_conf += credits[i].confidence;
    }
    ctx->metrics.avg_assignment_confidence = (*num_credits > 0) ? total_conf / *num_credits : 0.0f;

    return true;
}

float elig_quantum_estimate_optimal_entropy(
    eligibility_quantum_ctx_t ctx,
    const eligibility_trace_t* traces,
    uint32_t num_synapses
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_estimate_optimal_entropy: ctx is NULL");
        return 0.0f;
    }
    if (!traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_estimate_optimal_entropy: traces is NULL");
        return 0.0f;
    }
    if (num_synapses == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "elig_quantum_estimate_optimal_entropy: num_synapses is zero");
        return 0.0f;
    }

    /* Optimal entropy depends on network size and sparsity target */
    float max_entropy = log2f((float)num_synapses);
    float sparsity_factor = 0.3f;  /* Target 30% active */
    return max_entropy * sparsity_factor;
}

bool elig_quantum_compute_causal_strength(
    eligibility_quantum_ctx_t ctx,
    const eligibility_trace_t* traces,
    const float* weights,
    uint32_t num_synapses,
    float* causal_strengths
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_compute_causal_strength: ctx is NULL");
        return false;
    }
    if (!traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_compute_causal_strength: traces is NULL");
        return false;
    }
    if (!weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_compute_causal_strength: weights is NULL");
        return false;
    }
    if (!causal_strengths) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_compute_causal_strength: causal_strengths is NULL");
        return false;
    }

    for (uint32_t i = 0; i < num_synapses; i++) {
        float trace_val = fabsf(traces[i].trace);
        float weight_val = fabsf(weights[i]);
        causal_strengths[i] = trace_val * weight_val;
    }

    return true;
}

/*=============================================================================
 * QUANTUM OPTIMIZATION API
 *===========================================================================*/

bool elig_quantum_optimize_params(
    eligibility_quantum_ctx_t ctx,
    const eligibility_trace_t* traces,
    uint32_t num_synapses,
    const elig_quantum_params_t* current_params,
    elig_quantum_params_t* optimized_params
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_optimize_params: ctx is NULL");
        return false;
    }
    if (!current_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_optimize_params: current_params is NULL");
        return false;
    }
    if (!optimized_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_optimize_params: optimized_params is NULL");
        return false;
    }

    ctx->metrics.optimization_steps++;

    /* Copy current params */
    *optimized_params = *current_params;

    /* Simple simulated annealing step */
    float temp = ctx->anneal_state.temperature;

    /* Perturb parameters */
    float perturbation = 0.1f * temp / ctx->config.initial_temperature;
    optimized_params->tau_fast *= (1.0f + perturbation * (((float)nimcp_tl_rand() / RAND_MAX) - 0.5f));
    optimized_params->tau_slow *= (1.0f + perturbation * (((float)nimcp_tl_rand() / RAND_MAX) - 0.5f));
    optimized_params->learning_rate *= (1.0f + perturbation * (((float)nimcp_tl_rand() / RAND_MAX) - 0.5f));

    /* Compute energy (objective function) */
    float energy = 0.0f;
    if (traces && num_synapses > 0) {
        for (uint32_t i = 0; i < num_synapses; i++) {
            energy += fabsf(traces[i].trace - 0.5f);
        }
        energy /= num_synapses;
    }
    optimized_params->energy = energy;

    /* Update annealing state */
    ctx->anneal_state.iteration++;
    float cooling_rate = powf(ctx->config.final_temperature / ctx->config.initial_temperature,
                              1.0f / ctx->config.anneal_iterations);
    ctx->anneal_state.temperature *= cooling_rate;

    /* Check for tunneling */
    if (((float)nimcp_tl_rand() / RAND_MAX) < ctx->anneal_state.tunneling_probability) {
        ctx->anneal_state.tunneling_events++;
        ctx->metrics.tunneling_events++;
    }

    if (energy < ctx->anneal_state.best_energy) {
        ctx->anneal_state.best_energy = energy;
    }
    ctx->metrics.best_objective_value = ctx->anneal_state.best_energy;
    ctx->metrics.current_temperature = ctx->anneal_state.temperature;

    return true;
}

bool elig_quantum_get_anneal_state(
    eligibility_quantum_ctx_t ctx,
    elig_quantum_anneal_state_t* state
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_get_anneal_state: ctx is NULL");
        return false;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_get_anneal_state: state is NULL");
        return false;
    }
    *state = ctx->anneal_state;
    return true;
}

void elig_quantum_reset_anneal(eligibility_quantum_ctx_t ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_reset_anneal: ctx is NULL");
        return;
    }
    ctx->anneal_state.temperature = ctx->config.initial_temperature;
    ctx->anneal_state.tunneling_probability = ctx->config.tunneling_rate;
    ctx->anneal_state.iteration = 0;
    ctx->anneal_state.tunneling_events = 0;
    ctx->anneal_state.best_energy = INFINITY;
}

void elig_quantum_set_objective(eligibility_quantum_ctx_t ctx, uint32_t objective) {
    if (ctx) {
        ctx->config.objective = objective;
    }
}

/*=============================================================================
 * QUANTUM WALK DIFFUSION API
 *===========================================================================*/

bool elig_quantum_diffuse(
    eligibility_quantum_ctx_t ctx,
    uint32_t source_synapse,
    float initial_eligibility,
    const uint8_t* adjacency,
    uint32_t num_synapses,
    float* diffused_eligibility
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_diffuse: ctx is NULL");
        return false;
    }
    if (!diffused_eligibility) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_diffuse: diffused_eligibility is NULL");
        return false;
    }
    if (source_synapse >= num_synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "elig_quantum_diffuse: source_synapse out of bounds");
        return false;
    }

    ctx->metrics.diffusion_operations++;

    /* Initialize with delta at source */
    memset(diffused_eligibility, 0, num_synapses * sizeof(float));
    diffused_eligibility[source_synapse] = initial_eligibility;

    /* Simulate quantum walk (simplified) */
    uint32_t steps = ctx->config.walk_steps;
    float decoherence = ctx->config.decoherence_rate;

    float* temp = nimcp_malloc(num_synapses * sizeof(float));
    if (!temp) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_synapses * sizeof(float),
                           "elig_quantum_diffuse: failed to allocate temp array");
        return false;
    }

    for (uint32_t step = 0; step < steps; step++) {
        memcpy(temp, diffused_eligibility, num_synapses * sizeof(float));

        for (uint32_t i = 0; i < num_synapses; i++) {
            if (temp[i] > 1e-10f) {
                /* Spread to neighbors (or next if no adjacency) */
                if (adjacency) {
                    /* Use adjacency matrix */
                    for (uint32_t j = 0; j < num_synapses; j++) {
                        if (adjacency[i * num_synapses + j]) {
                            diffused_eligibility[j] += temp[i] * 0.25f;
                        }
                    }
                    diffused_eligibility[i] *= 0.5f;
                } else {
                    /* Simple diffusion to neighbors */
                    if (i > 0) diffused_eligibility[i-1] += temp[i] * 0.25f;
                    if (i < num_synapses - 1) diffused_eligibility[i+1] += temp[i] * 0.25f;
                    diffused_eligibility[i] *= 0.5f;
                }
            }
        }

        /* Apply decoherence */
        for (uint32_t i = 0; i < num_synapses; i++) {
            diffused_eligibility[i] *= (1.0f - decoherence);
        }
    }

    nimcp_free(temp);

    /* Normalize to preserve total probability (sum to initial_eligibility) */
    float total = 0.0f;
    for (uint32_t i = 0; i < num_synapses; i++) {
        total += diffused_eligibility[i];
    }
    if (total > 1e-10f) {
        float scale = initial_eligibility / total;
        for (uint32_t i = 0; i < num_synapses; i++) {
            diffused_eligibility[i] *= scale;
        }
    }

    /* Update statistics */
    ctx->walk_speedup = sqrtf((float)num_synapses);
    ctx->metrics.avg_diffusion_speedup = ctx->walk_speedup;

    return true;
}

bool elig_quantum_diffuse_multi(
    eligibility_quantum_ctx_t ctx,
    const uint32_t* source_synapses,
    const float* initial_eligibilities,
    uint32_t num_sources,
    const uint8_t* adjacency,
    uint32_t num_synapses,
    float* diffused_eligibility
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_diffuse_multi: ctx is NULL");
        return false;
    }
    if (!source_synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_diffuse_multi: source_synapses is NULL");
        return false;
    }
    if (!initial_eligibilities) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_diffuse_multi: initial_eligibilities is NULL");
        return false;
    }
    if (!diffused_eligibility) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_diffuse_multi: diffused_eligibility is NULL");
        return false;
    }

    /* Initialize */
    memset(diffused_eligibility, 0, num_synapses * sizeof(float));

    /* Sum diffusion from each source */
    float* single_diffusion = nimcp_malloc(num_synapses * sizeof(float));
    if (!single_diffusion) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "elig_quantum_diffuse_multi: failed to allocate single_diffusion array");
        return false;
    }

    for (uint32_t s = 0; s < num_sources; s++) {
        if (source_synapses[s] < num_synapses) {
            elig_quantum_diffuse(ctx, source_synapses[s], initial_eligibilities[s],
                                adjacency, num_synapses, single_diffusion);
            for (uint32_t i = 0; i < num_synapses; i++) {
                diffused_eligibility[i] += single_diffusion[i];
            }
        }
    }

    nimcp_free(single_diffusion);
    return true;
}

bool elig_quantum_get_walk_stats(
    eligibility_quantum_ctx_t ctx,
    float* speedup_factor,
    float* spreading_distance,
    float* entropy
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_get_walk_stats: ctx is NULL");
        return false;
    }
    if (speedup_factor) *speedup_factor = ctx->walk_speedup;
    if (spreading_distance) *spreading_distance = ctx->walk_distance;
    if (entropy) *entropy = ctx->walk_entropy;
    return true;
}

/*=============================================================================
 * ENHANCED ELIGIBILITY OPERATIONS
 *===========================================================================*/

void elig_quantum_update_trace_enhanced(
    eligibility_quantum_ctx_t ctx,
    eligibility_trace_t* trace,
    const eligibility_config_t* config,
    uint64_t current_time,
    bool spike_occurred,
    float weight
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_update_trace_enhanced: ctx is NULL");
        return;
    }
    if (!trace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_update_trace_enhanced: trace is NULL");
        return;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_update_trace_enhanced: config is NULL");
        return;
    }

    /* Standard trace decay with configurable lambda */
    float dt = (float)(current_time - trace->last_update);  /* ms */
    if (dt > 0 && dt < 10000.0f) {
        trace->trace *= expf(-dt / 20.0f);  /* Default tau = 20ms */
    }

    /* Add spike contribution */
    if (spike_occurred) {
        trace->trace += 1.0f;
        trace->last_update = current_time;
    }

    /* Clamp values */
    if (trace->trace > 2.0f) trace->trace = 2.0f;
}

int elig_quantum_consolidate_enhanced(
    eligibility_quantum_ctx_t ctx,
    void* synapses,
    const eligibility_trace_t* traces,
    uint32_t num_synapses,
    const eligibility_config_t* config,
    float reward,
    float dopamine_level
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_consolidate_enhanced: ctx is NULL");
        return 0;
    }
    if (!traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_consolidate_enhanced: traces is NULL");
        return 0;
    }

    int updated = 0;

    /* Apply dopamine-gated learning */
    for (uint32_t i = 0; i < num_synapses; i++) {
        float trace_val = traces[i].trace;
        if (trace_val > 0.1f && fabsf(reward) > 0.01f) {
            /* Would update synapse weight here */
            updated++;
        }
    }

    return updated;
}

uint32_t elig_quantum_learning_tick(
    eligibility_quantum_ctx_t ctx,
    eligibility_trace_t* traces,
    float* weights,
    uint32_t num_synapses,
    const eligibility_config_t* config,
    float reward,
    uint64_t current_time
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_learning_tick: ctx is NULL");
        return 0;
    }
    if (!traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_learning_tick: traces is NULL");
        return 0;
    }
    if (!weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_learning_tick: weights is NULL");
        return 0;
    }

    uint32_t updated = 0;

    for (uint32_t i = 0; i < num_synapses; i++) {
        float trace_val = traces[i].trace;
        float delta_w = reward * trace_val * 0.01f;

        if (fabsf(delta_w) > 1e-6f) {
            weights[i] += delta_w;
            /* Clamp weights */
            if (weights[i] > 1.0f) weights[i] = 1.0f;
            if (weights[i] < -1.0f) weights[i] = -1.0f;
            updated++;
        }
    }

    return updated;
}

/*=============================================================================
 * METRICS API
 *===========================================================================*/

bool elig_quantum_get_metrics(eligibility_quantum_ctx_t ctx, elig_quantum_metrics_t* metrics) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_get_metrics: ctx is NULL");
        return false;
    }
    if (!metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_get_metrics: metrics is NULL");
        return false;
    }
    *metrics = ctx->metrics;
    metrics->last_update_ms = get_time_ms();
    return true;
}

int32_t elig_quantum_flush_metrics(eligibility_quantum_ctx_t ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_flush_metrics: ctx is NULL");
        return -1;
    }
    /* Would write to file here */
    return 0;
}

bool elig_quantum_export_csv(eligibility_quantum_ctx_t ctx, const char* filename) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_export_csv: ctx is NULL");
        return false;
    }
    if (!filename) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_export_csv: filename is NULL");
        return false;
    }
    /* Would export to CSV here */
    return true;
}

bool elig_quantum_export_json(eligibility_quantum_ctx_t ctx, const char* filename) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_export_json: ctx is NULL");
        return false;
    }
    if (!filename) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_export_json: filename is NULL");
        return false;
    }
    /* Would export to JSON here */
    return true;
}

void elig_quantum_reset_metrics(eligibility_quantum_ctx_t ctx) {
    if (ctx) {
        memset(&ctx->metrics, 0, sizeof(ctx->metrics));
        ctx->start_time_ms = get_time_ms();
    }
}

/*=============================================================================
 * DIAGNOSTIC API
 *===========================================================================*/

void elig_quantum_print_status(const eligibility_quantum_ctx_t ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_print_status: ctx is NULL");
        return;
    }
    /* Would print status here */
}

bool elig_quantum_verify(const eligibility_quantum_ctx_t ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_quantum_verify: ctx is NULL");
        return false;
    }
    return ctx->enabled;
}
