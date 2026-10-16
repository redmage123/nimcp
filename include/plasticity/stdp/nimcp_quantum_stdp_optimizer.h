//=============================================================================
// nimcp_quantum_stdp_optimizer.h - Quantum-Inspired STDP Learning Rate Optimizer
//=============================================================================

#ifndef NIMCP_QUANTUM_STDP_OPTIMIZER_H
#define NIMCP_QUANTUM_STDP_OPTIMIZER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_quantum_stdp_optimizer.h
 * @brief Quantum-inspired optimization for STDP learning parameters
 *
 * WHAT: Uses quantum annealing concepts to optimize STDP learning rates
 * WHY:  Find optimal learning rates adaptively based on network activity
 * HOW:  Implements quantum tunneling for escaping local minima in parameter space
 *
 * QUANTUM CONCEPTS USED:
 * - Quantum Annealing: Gradual reduction of tunneling probability
 * - Superposition: Multiple candidate learning rates simultaneously
 * - Tunneling: Escape local minima in learning rate landscape
 * - Amplitude Estimation: Estimate optimal parameter values
 *
 * BIOLOGICAL ANALOGY:
 * - Metaplasticity: Learning-to-learn mechanisms in the brain
 * - Homeostatic regulation: Maintaining stable overall activity
 * - BCM sliding threshold: Activity-dependent threshold adjustment
 *
 * KEY FEATURES:
 * - Adaptive learning rate scaling based on activity statistics
 * - Quantum tunneling for exploration of parameter space
 * - Ensemble of candidate parameters with amplitude weighting
 * - Integration with existing STDP infrastructure
 *
 * @author NIMCP Development Team
 * @date 2025-01-20
 */

//=============================================================================
// Constants
//=============================================================================

#define QSTDP_MAX_ENSEMBLE       16     /**< Maximum ensemble size */
#define QSTDP_MAX_HISTORY        256    /**< Maximum history length */
#define QSTDP_DEFAULT_SEED       12345  /**< Default random seed */

//=============================================================================
// Types
//=============================================================================

/**
 * WHAT: Optimization objective types
 * WHY:  Different goals require different optimization strategies
 */
typedef enum {
    QSTDP_OBJ_STABILITY,       /**< Minimize weight variance */
    QSTDP_OBJ_SPARSITY,        /**< Maximize weight sparsity */
    QSTDP_OBJ_BALANCE,         /**< Balance LTP/LTD */
    QSTDP_OBJ_INFORMATION,     /**< Maximize information capacity */
    QSTDP_OBJ_HOMEOSTASIS,     /**< Maintain target firing rate */
    QSTDP_OBJ_CONVERGENCE      /**< Minimize learning rate for stability */
} qstdp_objective_t;

/**
 * WHAT: Annealing schedule types
 * WHY:  Control rate of tunneling probability decay
 */
typedef enum {
    QSTDP_SCHEDULE_LINEAR,     /**< Linear decay */
    QSTDP_SCHEDULE_EXPONENTIAL,/**< Exponential decay */
    QSTDP_SCHEDULE_LOGARITHMIC,/**< Slow logarithmic decay */
    QSTDP_SCHEDULE_ADAPTIVE    /**< Activity-dependent schedule */
} qstdp_schedule_t;

/**
 * WHAT: Configuration for quantum STDP optimizer
 */
typedef struct {
    qstdp_objective_t objective;       /**< Optimization objective */
    qstdp_schedule_t schedule;         /**< Annealing schedule */
    uint32_t ensemble_size;            /**< Number of candidate parameters */
    uint32_t history_length;           /**< Activity history to consider */
    float initial_temperature;         /**< Initial quantum temperature */
    float final_temperature;           /**< Final temperature (end of annealing) */
    float tunneling_rate;              /**< Initial tunneling probability */
    float exploration_radius;          /**< Parameter exploration range */
    float target_firing_rate;          /**< Target rate for homeostasis */
    float ltp_ltd_balance;             /**< Desired LTP/LTD ratio (1.0 = balanced) */
    bool enable_momentum;              /**< Use momentum in parameter updates */
    float momentum_decay;              /**< Momentum decay factor */
    uint32_t seed;                     /**< Random seed */
} qstdp_optimizer_config_t;

/**
 * WHAT: Candidate parameter set in the ensemble
 */
typedef struct {
    float learning_rate;      /**< Proposed learning rate */
    float a_plus;             /**< Proposed LTP amplitude */
    float a_minus;            /**< Proposed LTD amplitude */
    float tau_plus;           /**< Proposed LTP time constant */
    float tau_minus;          /**< Proposed LTD time constant */
    float amplitude;          /**< Quantum amplitude (probability weight) */
    float energy;             /**< Energy (objective function value) */
} qstdp_candidate_t;

/**
 * WHAT: Activity statistics for optimization
 */
typedef struct {
    float mean_weight;        /**< Average synaptic weight */
    float weight_variance;    /**< Weight variance */
    float ltp_rate;           /**< LTP events per second */
    float ltd_rate;           /**< LTD events per second */
    float ltp_ltd_ratio;      /**< LTP/LTD ratio */
    float firing_rate;        /**< Average firing rate */
    float sparsity;           /**< Fraction of near-zero weights */
} qstdp_activity_stats_t;

/**
 * WHAT: Quantum STDP optimizer context (opaque handle)
 */
typedef struct qstdp_optimizer_struct* qstdp_optimizer_t;

/**
 * WHAT: Internal structure for quantum STDP optimizer
 */
typedef struct qstdp_optimizer_struct {
    qstdp_optimizer_config_t config;

    /* Ensemble of candidates */
    qstdp_candidate_t candidates[QSTDP_MAX_ENSEMBLE];
    uint32_t n_candidates;

    /* Current best parameters */
    qstdp_candidate_t best;

    /* Annealing state */
    float temperature;
    float tunneling_prob;
    uint64_t iteration;

    /* Momentum for parameter updates */
    float lr_momentum;
    float a_plus_momentum;
    float a_minus_momentum;

    /* Activity history */
    qstdp_activity_stats_t history[QSTDP_MAX_HISTORY];
    uint32_t history_idx;
    uint32_t history_count;

    /* Random state */
    uint32_t rng_state;

    /* Statistics */
    uint64_t optimizations_performed;
    uint64_t tunneling_events;
    float best_energy_ever;
} qstdp_optimizer_internal_t;

/**
 * WHAT: Statistics from the optimizer
 */
typedef struct {
    uint64_t optimizations_performed;
    uint64_t tunneling_events;
    float best_energy_ever;
    float current_temperature;
    float current_tunneling_prob;
    uint32_t active_candidates;
} qstdp_optimizer_stats_t;

//=============================================================================
// Default Configuration
//=============================================================================

static inline qstdp_optimizer_config_t qstdp_optimizer_default_config(void) {
    return (qstdp_optimizer_config_t){
        .objective = QSTDP_OBJ_BALANCE,
        .schedule = QSTDP_SCHEDULE_EXPONENTIAL,
        .ensemble_size = 8,
        .history_length = 64,
        .initial_temperature = 1.0f,
        .final_temperature = 0.01f,
        .tunneling_rate = 0.3f,
        .exploration_radius = 0.5f,
        .target_firing_rate = 5.0f,  /* Hz */
        .ltp_ltd_balance = 1.0f,
        .enable_momentum = true,
        .momentum_decay = 0.9f,
        .seed = QSTDP_DEFAULT_SEED
    };
}

//=============================================================================
// Random Number Generation
//=============================================================================

static inline uint32_t qstdp_rand(uint32_t* state) {
    *state = (*state) * 1103515245 + 12345;
    return (*state >> 16) & 0x7FFF;
}

static inline float qstdp_randf(uint32_t* state) {
    return (float)qstdp_rand(state) / 32767.0f;
}

static inline float qstdp_randn(uint32_t* state) {
    float u1 = qstdp_randf(state) + 1e-10f;
    float u2 = qstdp_randf(state);
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * WHAT: Create quantum STDP optimizer
 * WHY:  Initialize optimization context with configuration
 * HOW:  Allocate memory, initialize ensemble with random candidates
 *
 * @param config Configuration (NULL for defaults)
 * @return Optimizer context or NULL on error
 */
static inline qstdp_optimizer_t qstdp_optimizer_create(
    const qstdp_optimizer_config_t* config
) {
    qstdp_optimizer_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = qstdp_optimizer_default_config();
    }

    /* Validate config */
    if (cfg.ensemble_size == 0 || cfg.ensemble_size > QSTDP_MAX_ENSEMBLE) {
        return NULL;
    }
    if (cfg.history_length == 0 || cfg.history_length > QSTDP_MAX_HISTORY) {
        return NULL;
    }

    /* Allocate context */
    qstdp_optimizer_internal_t* ctx =
        (qstdp_optimizer_internal_t*)nimcp_calloc(1, sizeof(qstdp_optimizer_internal_t));
    if (!ctx) return NULL;

    ctx->config = cfg;
    ctx->n_candidates = cfg.ensemble_size;
    ctx->temperature = cfg.initial_temperature;
    ctx->tunneling_prob = cfg.tunneling_rate;
    ctx->rng_state = cfg.seed;
    ctx->best_energy_ever = 1e10f;

    /* Initialize ensemble with random candidates */
    for (uint32_t i = 0; i < ctx->n_candidates; i++) {
        ctx->candidates[i].learning_rate = 0.01f * (0.5f + qstdp_randf(&ctx->rng_state));
        ctx->candidates[i].a_plus = 0.005f * (0.5f + qstdp_randf(&ctx->rng_state));
        ctx->candidates[i].a_minus = 0.005f * (0.5f + qstdp_randf(&ctx->rng_state));
        ctx->candidates[i].tau_plus = 15.0f + 10.0f * qstdp_randf(&ctx->rng_state);
        ctx->candidates[i].tau_minus = 15.0f + 10.0f * qstdp_randf(&ctx->rng_state);
        ctx->candidates[i].amplitude = 1.0f / ctx->n_candidates;
        ctx->candidates[i].energy = 1e10f;
    }

    /* Initialize best with first candidate */
    ctx->best = ctx->candidates[0];

    return (qstdp_optimizer_t)ctx;
}

/**
 * WHAT: Destroy quantum STDP optimizer
 * WHY:  Free all allocated resources
 */
static inline void qstdp_optimizer_destroy(qstdp_optimizer_t ctx) {
    nimcp_free(ctx);
}

//=============================================================================
// Energy (Objective) Functions
//=============================================================================

/**
 * WHAT: Compute energy for stability objective
 * WHY:  Lower variance means more stable weights
 */
static inline float qstdp_energy_stability(
    const qstdp_candidate_t* candidate,
    const qstdp_activity_stats_t* stats
) {
    /* Penalize high variance and extreme learning rates */
    float variance_term = stats->weight_variance * 10.0f;
    float lr_term = candidate->learning_rate * candidate->learning_rate * 100.0f;
    return variance_term + lr_term;
}

/**
 * WHAT: Compute energy for sparsity objective
 * WHY:  Sparse representations are efficient
 */
static inline float qstdp_energy_sparsity(
    const qstdp_candidate_t* candidate,
    const qstdp_activity_stats_t* stats
) {
    /* Reward high sparsity */
    float sparsity_reward = -logf(stats->sparsity + 1e-6f);
    float lr_regularization = candidate->learning_rate * 10.0f;
    return sparsity_reward + lr_regularization;
}

/**
 * WHAT: Compute energy for LTP/LTD balance objective
 * WHY:  Balanced learning maintains stable dynamics
 */
static inline float qstdp_energy_balance(
    const qstdp_candidate_t* candidate,
    const qstdp_activity_stats_t* stats,
    float target_ratio
) {
    float ratio_error = fabsf(stats->ltp_ltd_ratio - target_ratio);
    float imbalance = fabsf(candidate->a_plus - candidate->a_minus) /
                      (candidate->a_plus + candidate->a_minus + 1e-6f);
    return ratio_error * 10.0f + imbalance;
}

/**
 * WHAT: Compute energy for homeostasis objective
 * WHY:  Maintain target firing rate
 */
static inline float qstdp_energy_homeostasis(
    const qstdp_candidate_t* candidate,
    const qstdp_activity_stats_t* stats,
    float target_rate
) {
    float rate_error = fabsf(stats->firing_rate - target_rate) / target_rate;
    return rate_error * rate_error * 100.0f;
}

/**
 * WHAT: Compute energy for convergence objective
 * WHY:  Minimize learning rate when close to optimum
 */
static inline float qstdp_energy_convergence(
    const qstdp_candidate_t* candidate,
    const qstdp_activity_stats_t* stats
) {
    /* Prefer low learning rates when variance is already low */
    float lr_cost = candidate->learning_rate * (1.0f + stats->weight_variance);
    return lr_cost * 100.0f;
}

/**
 * WHAT: Compute energy based on configured objective
 */
static inline float qstdp_compute_energy(
    const qstdp_optimizer_config_t* config,
    const qstdp_candidate_t* candidate,
    const qstdp_activity_stats_t* stats
) {
    switch (config->objective) {
        case QSTDP_OBJ_STABILITY:
            return qstdp_energy_stability(candidate, stats);
        case QSTDP_OBJ_SPARSITY:
            return qstdp_energy_sparsity(candidate, stats);
        case QSTDP_OBJ_BALANCE:
            return qstdp_energy_balance(candidate, stats, config->ltp_ltd_balance);
        case QSTDP_OBJ_HOMEOSTASIS:
            return qstdp_energy_homeostasis(candidate, stats, config->target_firing_rate);
        case QSTDP_OBJ_CONVERGENCE:
            return qstdp_energy_convergence(candidate, stats);
        case QSTDP_OBJ_INFORMATION:
            /* Maximize entropy-like measure */
            return -logf(stats->weight_variance + 1e-6f);
        default:
            return qstdp_energy_balance(candidate, stats, 1.0f);
    }
}

//=============================================================================
// Quantum Operations
//=============================================================================

/**
 * WHAT: Apply quantum tunneling to candidate
 * WHY:  Escape local minima by jumping to distant parameter regions
 * HOW:  With probability proportional to temperature, make large random jump
 *
 * @param ctx Optimizer context
 * @param candidate Candidate to potentially tunnel
 * @return true if tunneling occurred
 */
static inline bool qstdp_quantum_tunnel(
    qstdp_optimizer_internal_t* ctx,
    qstdp_candidate_t* candidate
) {
    if (qstdp_randf(&ctx->rng_state) > ctx->tunneling_prob) {
        return false;
    }

    /* Tunneling occurs: make large jump in parameter space */
    float radius = ctx->config.exploration_radius * ctx->temperature;

    candidate->learning_rate *= (1.0f + radius * qstdp_randn(&ctx->rng_state));
    candidate->a_plus *= (1.0f + radius * qstdp_randn(&ctx->rng_state));
    candidate->a_minus *= (1.0f + radius * qstdp_randn(&ctx->rng_state));
    candidate->tau_plus += radius * 5.0f * qstdp_randn(&ctx->rng_state);
    candidate->tau_minus += radius * 5.0f * qstdp_randn(&ctx->rng_state);

    /* Clamp to valid ranges */
    candidate->learning_rate = fmaxf(1e-5f, fminf(1.0f, candidate->learning_rate));
    candidate->a_plus = fmaxf(1e-5f, fminf(0.1f, candidate->a_plus));
    candidate->a_minus = fmaxf(1e-5f, fminf(0.1f, candidate->a_minus));
    candidate->tau_plus = fmaxf(5.0f, fminf(100.0f, candidate->tau_plus));
    candidate->tau_minus = fmaxf(5.0f, fminf(100.0f, candidate->tau_minus));

    ctx->tunneling_events++;
    return true;
}

/**
 * WHAT: Update candidate amplitudes based on energies
 * WHY:  Implement Boltzmann-like amplitude distribution
 * HOW:  Lower energy -> higher amplitude (probability)
 */
static inline void qstdp_update_amplitudes(
    qstdp_optimizer_internal_t* ctx
) {
    float total = 0.0f;

    /* Compute Boltzmann weights */
    for (uint32_t i = 0; i < ctx->n_candidates; i++) {
        float boltz = expf(-ctx->candidates[i].energy / ctx->temperature);
        ctx->candidates[i].amplitude = boltz;
        total += boltz;
    }

    /* Normalize */
    if (total > 1e-10f) {
        for (uint32_t i = 0; i < ctx->n_candidates; i++) {
            ctx->candidates[i].amplitude /= total;
        }
    }
}

/**
 * WHAT: Collapse to single candidate (measurement)
 * WHY:  Select final parameters weighted by amplitude
 * HOW:  Sample from amplitude distribution
 */
static inline qstdp_candidate_t qstdp_collapse(
    qstdp_optimizer_internal_t* ctx
) {
    float r = qstdp_randf(&ctx->rng_state);
    float cumsum = 0.0f;

    for (uint32_t i = 0; i < ctx->n_candidates; i++) {
        cumsum += ctx->candidates[i].amplitude;
        if (r < cumsum) {
            return ctx->candidates[i];
        }
    }

    return ctx->candidates[ctx->n_candidates - 1];
}

/**
 * WHAT: Update annealing schedule
 * WHY:  Gradually reduce temperature and tunneling probability
 */
static inline void qstdp_anneal(qstdp_optimizer_internal_t* ctx) {
    float progress = (float)ctx->iteration / 1000.0f;  /* Assume ~1000 iterations */
    progress = fminf(1.0f, progress);

    switch (ctx->config.schedule) {
        case QSTDP_SCHEDULE_LINEAR:
            ctx->temperature = ctx->config.initial_temperature *
                              (1.0f - progress) +
                              ctx->config.final_temperature * progress;
            break;

        case QSTDP_SCHEDULE_EXPONENTIAL:
            ctx->temperature = ctx->config.initial_temperature *
                              powf(ctx->config.final_temperature /
                                   ctx->config.initial_temperature, progress);
            break;

        case QSTDP_SCHEDULE_LOGARITHMIC:
            ctx->temperature = ctx->config.initial_temperature /
                              (1.0f + logf(1.0f + 10.0f * progress));
            break;

        case QSTDP_SCHEDULE_ADAPTIVE:
            /* Slow down if finding better solutions */
            if (ctx->best.energy < ctx->best_energy_ever) {
                ctx->temperature *= 0.98f;
            } else {
                ctx->temperature *= 0.995f;
            }
            ctx->temperature = fmaxf(ctx->config.final_temperature, ctx->temperature);
            break;
    }

    /* Update tunneling probability proportionally */
    ctx->tunneling_prob = ctx->config.tunneling_rate * ctx->temperature;
    ctx->iteration++;
}

//=============================================================================
// Core Optimization Function
//=============================================================================

/**
 * WHAT: Record activity statistics
 * WHY:  Build history for optimization decisions
 *
 * @param ctx Optimizer context
 * @param stats Current activity statistics
 */
static inline void qstdp_optimizer_record_activity(
    qstdp_optimizer_t ctx,
    const qstdp_activity_stats_t* stats
) {
    if (!ctx || !stats) return;
    qstdp_optimizer_internal_t* internal = (qstdp_optimizer_internal_t*)ctx;

    internal->history[internal->history_idx] = *stats;
    internal->history_idx = (internal->history_idx + 1) % internal->config.history_length;
    if (internal->history_count < internal->config.history_length) {
        internal->history_count++;
    }
}

/**
 * WHAT: Get average activity statistics over history
 */
static inline qstdp_activity_stats_t qstdp_get_avg_stats(
    qstdp_optimizer_internal_t* ctx
) {
    qstdp_activity_stats_t avg = {0};

    if (ctx->history_count == 0) {
        avg.ltp_ltd_ratio = 1.0f;
        avg.firing_rate = 5.0f;
        return avg;
    }

    for (uint32_t i = 0; i < ctx->history_count; i++) {
        avg.mean_weight += ctx->history[i].mean_weight;
        avg.weight_variance += ctx->history[i].weight_variance;
        avg.ltp_rate += ctx->history[i].ltp_rate;
        avg.ltd_rate += ctx->history[i].ltd_rate;
        avg.ltp_ltd_ratio += ctx->history[i].ltp_ltd_ratio;
        avg.firing_rate += ctx->history[i].firing_rate;
        avg.sparsity += ctx->history[i].sparsity;
    }

    float n = (float)ctx->history_count;
    avg.mean_weight /= n;
    avg.weight_variance /= n;
    avg.ltp_rate /= n;
    avg.ltd_rate /= n;
    avg.ltp_ltd_ratio /= n;
    avg.firing_rate /= n;
    avg.sparsity /= n;

    return avg;
}

/**
 * WHAT: Perform one optimization step
 * WHY:  Update parameter ensemble based on activity
 * HOW:  Evaluate energies, update amplitudes, apply tunneling, anneal
 *
 * @param ctx Optimizer context
 * @param stats Current activity statistics (or NULL to use history)
 * @return Recommended learning rate
 */
static inline float qstdp_optimizer_step(
    qstdp_optimizer_t ctx,
    const qstdp_activity_stats_t* stats
) {
    if (!ctx) return 0.01f;
    qstdp_optimizer_internal_t* internal = (qstdp_optimizer_internal_t*)ctx;

    /* Get stats to use */
    qstdp_activity_stats_t effective_stats;
    if (stats) {
        effective_stats = *stats;
        qstdp_optimizer_record_activity(ctx, stats);
    } else {
        effective_stats = qstdp_get_avg_stats(internal);
    }

    /* Evaluate energy for each candidate */
    for (uint32_t i = 0; i < internal->n_candidates; i++) {
        internal->candidates[i].energy = qstdp_compute_energy(
            &internal->config,
            &internal->candidates[i],
            &effective_stats
        );

        /* Update best if improved */
        if (internal->candidates[i].energy < internal->best.energy) {
            internal->best = internal->candidates[i];
        }
    }

    /* Update global best */
    if (internal->best.energy < internal->best_energy_ever) {
        internal->best_energy_ever = internal->best.energy;
    }

    /* Update amplitudes based on energies */
    qstdp_update_amplitudes(internal);

    /* Apply quantum tunneling to each candidate */
    for (uint32_t i = 0; i < internal->n_candidates; i++) {
        qstdp_quantum_tunnel(internal, &internal->candidates[i]);
    }

    /* Anneal temperature */
    qstdp_anneal(internal);

    /* Apply momentum to best parameters if enabled */
    if (internal->config.enable_momentum) {
        qstdp_candidate_t collapsed = qstdp_collapse(internal);

        internal->lr_momentum = internal->config.momentum_decay * internal->lr_momentum +
                               (1.0f - internal->config.momentum_decay) *
                               (collapsed.learning_rate - internal->best.learning_rate);

        internal->best.learning_rate += internal->lr_momentum;
        internal->best.learning_rate = fmaxf(1e-5f, fminf(1.0f, internal->best.learning_rate));
    }

    internal->optimizations_performed++;

    return internal->best.learning_rate;
}

/**
 * WHAT: Get current best parameters
 * WHY:  Access optimized STDP parameters
 *
 * @param ctx Optimizer context
 * @param learning_rate Output: optimal learning rate
 * @param a_plus Output: optimal LTP amplitude
 * @param a_minus Output: optimal LTD amplitude
 * @param tau_plus Output: optimal LTP time constant
 * @param tau_minus Output: optimal LTD time constant
 * @return 0 on success
 */
static inline int qstdp_optimizer_get_params(
    qstdp_optimizer_t ctx,
    float* learning_rate,
    float* a_plus,
    float* a_minus,
    float* tau_plus,
    float* tau_minus
) {
    if (!ctx) return -1;
    qstdp_optimizer_internal_t* internal = (qstdp_optimizer_internal_t*)ctx;

    if (learning_rate) *learning_rate = internal->best.learning_rate;
    if (a_plus) *a_plus = internal->best.a_plus;
    if (a_minus) *a_minus = internal->best.a_minus;
    if (tau_plus) *tau_plus = internal->best.tau_plus;
    if (tau_minus) *tau_minus = internal->best.tau_minus;

    return 0;
}

/**
 * WHAT: Set initial parameters
 * WHY:  Initialize optimization from known good starting point
 */
static inline int qstdp_optimizer_set_params(
    qstdp_optimizer_t ctx,
    float learning_rate,
    float a_plus,
    float a_minus,
    float tau_plus,
    float tau_minus
) {
    if (!ctx) return -1;
    qstdp_optimizer_internal_t* internal = (qstdp_optimizer_internal_t*)ctx;

    internal->best.learning_rate = learning_rate;
    internal->best.a_plus = a_plus;
    internal->best.a_minus = a_minus;
    internal->best.tau_plus = tau_plus;
    internal->best.tau_minus = tau_minus;

    /* Reinitialize ensemble around new best */
    for (uint32_t i = 0; i < internal->n_candidates; i++) {
        float noise = 0.1f;
        internal->candidates[i].learning_rate = learning_rate * (1.0f + noise * qstdp_randn(&internal->rng_state));
        internal->candidates[i].a_plus = a_plus * (1.0f + noise * qstdp_randn(&internal->rng_state));
        internal->candidates[i].a_minus = a_minus * (1.0f + noise * qstdp_randn(&internal->rng_state));
        internal->candidates[i].tau_plus = tau_plus + noise * 5.0f * qstdp_randn(&internal->rng_state);
        internal->candidates[i].tau_minus = tau_minus + noise * 5.0f * qstdp_randn(&internal->rng_state);
    }

    return 0;
}

/**
 * WHAT: Reset optimizer to initial state
 * WHY:  Start fresh optimization
 */
static inline int qstdp_optimizer_reset(qstdp_optimizer_t ctx) {
    if (!ctx) return -1;
    qstdp_optimizer_internal_t* internal = (qstdp_optimizer_internal_t*)ctx;

    internal->temperature = internal->config.initial_temperature;
    internal->tunneling_prob = internal->config.tunneling_rate;
    internal->iteration = 0;
    internal->history_idx = 0;
    internal->history_count = 0;
    internal->lr_momentum = 0.0f;
    internal->a_plus_momentum = 0.0f;
    internal->a_minus_momentum = 0.0f;

    /* Reinitialize ensemble with random candidates */
    for (uint32_t i = 0; i < internal->n_candidates; i++) {
        internal->candidates[i].learning_rate = 0.01f * (0.5f + qstdp_randf(&internal->rng_state));
        internal->candidates[i].a_plus = 0.005f * (0.5f + qstdp_randf(&internal->rng_state));
        internal->candidates[i].a_minus = 0.005f * (0.5f + qstdp_randf(&internal->rng_state));
        internal->candidates[i].tau_plus = 15.0f + 10.0f * qstdp_randf(&internal->rng_state);
        internal->candidates[i].tau_minus = 15.0f + 10.0f * qstdp_randf(&internal->rng_state);
        internal->candidates[i].amplitude = 1.0f / internal->n_candidates;
        internal->candidates[i].energy = 1e10f;
    }

    internal->best = internal->candidates[0];

    return 0;
}

/**
 * WHAT: Get optimizer statistics
 */
static inline int qstdp_optimizer_get_stats(
    qstdp_optimizer_t ctx,
    qstdp_optimizer_stats_t* stats
) {
    if (!ctx || !stats) return -1;
    qstdp_optimizer_internal_t* internal = (qstdp_optimizer_internal_t*)ctx;

    stats->optimizations_performed = internal->optimizations_performed;
    stats->tunneling_events = internal->tunneling_events;
    stats->best_energy_ever = internal->best_energy_ever;
    stats->current_temperature = internal->temperature;
    stats->current_tunneling_prob = internal->tunneling_prob;
    stats->active_candidates = internal->n_candidates;

    return 0;
}

/**
 * WHAT: Get amplitude-weighted average of all candidates
 * WHY:  Smooth estimate considering all candidates
 */
static inline float qstdp_optimizer_get_expected_lr(qstdp_optimizer_t ctx) {
    if (!ctx) return 0.01f;
    qstdp_optimizer_internal_t* internal = (qstdp_optimizer_internal_t*)ctx;

    float expected = 0.0f;
    for (uint32_t i = 0; i < internal->n_candidates; i++) {
        expected += internal->candidates[i].amplitude *
                   internal->candidates[i].learning_rate;
    }
    return expected;
}

/**
 * WHAT: Get configuration
 */
static inline int qstdp_optimizer_get_config(
    qstdp_optimizer_t ctx,
    qstdp_optimizer_config_t* config
) {
    if (!ctx || !config) return -1;
    qstdp_optimizer_internal_t* internal = (qstdp_optimizer_internal_t*)ctx;
    *config = internal->config;
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_QUANTUM_STDP_OPTIMIZER_H */
