//=============================================================================
// nimcp_plasticity_ternary.h - Ternary Plasticity Updates
//=============================================================================
/**
 * @file nimcp_plasticity_ternary.h
 * @brief Ternary representation for synaptic plasticity
 *
 * WHAT: Three-state plasticity {LTD, STABLE, LTP}
 * WHY:  Discrete plasticity updates for efficient hardware/simulation
 * HOW:  Map continuous STDP/BCM to ternary state transitions
 *
 * BIOLOGICAL BASIS:
 * - Synaptic weights change in discrete steps at molecular level
 * - LTP: Long-term potentiation (strengthening)
 * - LTD: Long-term depression (weakening)
 * - Stable: Maintenance of current weight (homeostasis)
 *
 * INTEGRATION WITH PLASTICITY RULES:
 * - STDP: Pre-post timing → LTP/LTD decision
 * - BCM: Post > threshold → LTP, Post < threshold → LTD
 * - Homeostatic: Nudge toward target firing rate
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#ifndef NIMCP_PLASTICITY_TERNARY_H
#define NIMCP_PLASTICITY_TERNARY_H

#include "utils/ternary/nimcp_ternary.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Plasticity Ternary Types
//=============================================================================

/**
 * @brief Ternary plasticity direction
 */
typedef trit_t trit_plasticity_t;

#define TRIT_PLASTICITY_LTD     TRIT_NEGATIVE  /**< Long-term depression */
#define TRIT_PLASTICITY_STABLE  TRIT_UNKNOWN   /**< No change */
#define TRIT_PLASTICITY_LTP     TRIT_POSITIVE  /**< Long-term potentiation */

//=============================================================================
// STDP Ternary Functions
//=============================================================================

/**
 * @brief Compute ternary STDP update from spike timing
 *
 * WHAT: Determine LTP/LTD from pre-post timing difference
 * WHY:  Discretize STDP for ternary weight updates
 * HOW:  Standard STDP window with dead zone
 *
 * STDP RULE:
 * - dt = t_post - t_pre
 * - dt > 0 (causal): LTP (pre fires before post)
 * - dt < 0 (anti-causal): LTD (post fires before pre)
 * - |dt| too small or large: STABLE
 *
 * @param dt_ms Time difference (t_post - t_pre) in ms
 * @param tau_plus LTP time constant (ms)
 * @param tau_minus LTD time constant (ms)
 * @param dead_zone Minimum |dt| for plasticity
 * @return Ternary plasticity direction
 */
static inline trit_plasticity_t trit_stdp_compute(
    float dt_ms,
    float tau_plus,
    float tau_minus,
    float dead_zone
) {
    float abs_dt = (dt_ms >= 0) ? dt_ms : -dt_ms;

    /* Dead zone: too close in time */
    if (abs_dt < dead_zone) return TRIT_PLASTICITY_STABLE;

    /* Too far apart: outside STDP window */
    float cutoff = 5.0f * ((tau_plus > tau_minus) ? tau_plus : tau_minus);
    if (abs_dt > cutoff) return TRIT_PLASTICITY_STABLE;

    /* Causal: LTP */
    if (dt_ms > 0) return TRIT_PLASTICITY_LTP;

    /* Anti-causal: LTD */
    return TRIT_PLASTICITY_LTD;
}

/**
 * @brief Compute ternary STDP with magnitude threshold
 *
 * @param delta Continuous STDP delta value
 * @param threshold Minimum magnitude for change
 * @return Ternary plasticity direction
 */
static inline trit_plasticity_t trit_stdp_from_delta(
    float delta,
    float threshold
) {
    if (delta >= threshold) return TRIT_PLASTICITY_LTP;
    if (delta <= -threshold) return TRIT_PLASTICITY_LTD;
    return TRIT_PLASTICITY_STABLE;
}

//=============================================================================
// BCM Ternary Functions
//=============================================================================

/**
 * @brief Compute ternary BCM update
 *
 * WHAT: Bienenstock-Cooper-Munro plasticity rule
 * WHY:  Rate-based plasticity with sliding threshold
 * HOW:  Compare postsynaptic activity to threshold
 *
 * BCM RULE:
 * - activity > threshold: LTP
 * - activity < threshold: LTD
 * - activity ≈ threshold: STABLE
 *
 * @param post_activity Postsynaptic firing rate/activity
 * @param theta_m Modification threshold
 * @param dead_zone Width of STABLE region around theta
 * @return Ternary plasticity direction
 */
static inline trit_plasticity_t trit_bcm_compute(
    float post_activity,
    float theta_m,
    float dead_zone
) {
    float diff = post_activity - theta_m;

    if (diff > dead_zone) return TRIT_PLASTICITY_LTP;
    if (diff < -dead_zone) return TRIT_PLASTICITY_LTD;
    return TRIT_PLASTICITY_STABLE;
}

//=============================================================================
// Homeostatic Ternary Functions
//=============================================================================

/**
 * @brief Compute ternary homeostatic update
 *
 * WHAT: Nudge weights to restore target firing rate
 * WHY:  Maintain network stability
 * HOW:  Compare current rate to target
 *
 * @param current_rate Current firing rate
 * @param target_rate Target firing rate
 * @param tolerance Rate difference tolerance
 * @return Ternary plasticity direction
 */
static inline trit_plasticity_t trit_homeostatic_compute(
    float current_rate,
    float target_rate,
    float tolerance
) {
    float diff = current_rate - target_rate;

    /* Rate too high: decrease weights (LTD) */
    if (diff > tolerance) return TRIT_PLASTICITY_LTD;

    /* Rate too low: increase weights (LTP) */
    if (diff < -tolerance) return TRIT_PLASTICITY_LTP;

    /* Rate in acceptable range */
    return TRIT_PLASTICITY_STABLE;
}

//=============================================================================
// Combined Plasticity Rules
//=============================================================================

/**
 * @brief Combine multiple plasticity signals
 *
 * WHAT: Aggregate STDP, BCM, homeostatic signals
 * WHY:  Multiple rules may act on same synapse
 * HOW:  Voting with configurable weights
 *
 * @param signals Array of plasticity signals
 * @param weights Array of signal weights
 * @param n_signals Number of signals
 * @param threshold Weighted sum threshold
 * @return Combined ternary direction
 */
static inline trit_plasticity_t trit_plasticity_combine(
    const trit_plasticity_t* signals,
    const float* weights,
    size_t n_signals,
    float threshold
) {
    if (!signals || n_signals == 0) return TRIT_PLASTICITY_STABLE;

    float weighted_sum = 0.0f;

    for (size_t i = 0; i < n_signals; i++) {
        float w = (weights) ? weights[i] : 1.0f;
        weighted_sum += (float)signals[i] * w;
    }

    if (weighted_sum >= threshold) return TRIT_PLASTICITY_LTP;
    if (weighted_sum <= -threshold) return TRIT_PLASTICITY_LTD;
    return TRIT_PLASTICITY_STABLE;
}

/**
 * @brief Apply neuromodulator gating to plasticity
 *
 * WHAT: Modulate plasticity based on neuromodulator state
 * WHY:  Dopamine, ACh, etc. gate learning
 * HOW:  Multiply plasticity by modulator level, then discretize
 *
 * @param base_plasticity Base ternary plasticity
 * @param modulator_level Neuromodulator level [0, 1]
 * @param gate_threshold Threshold for gating
 * @return Gated plasticity (may become STABLE if modulator low)
 */
static inline trit_plasticity_t trit_plasticity_gate(
    trit_plasticity_t base_plasticity,
    float modulator_level,
    float gate_threshold
) {
    /* Low modulator blocks all plasticity */
    if (modulator_level < gate_threshold) return TRIT_PLASTICITY_STABLE;

    /* Otherwise pass through */
    return base_plasticity;
}

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Plasticity event statistics
 */
typedef struct {
    uint64_t n_ltp;      /**< Count of LTP events */
    uint64_t n_stable;   /**< Count of STABLE events */
    uint64_t n_ltd;      /**< Count of LTD events */
    float ltp_ratio;     /**< Fraction of LTP */
    float ltd_ratio;     /**< Fraction of LTD */
} trit_plasticity_stats_t;

/**
 * @brief Count plasticity events
 *
 * @param events Array of plasticity events
 * @param n_events Number of events
 * @param stats Output statistics
 */
static inline void trit_plasticity_count(
    const trit_plasticity_t* events,
    size_t n_events,
    trit_plasticity_stats_t* stats
) {
    if (!stats) return;

    stats->n_ltp = 0;
    stats->n_stable = 0;
    stats->n_ltd = 0;

    if (!events || n_events == 0) {
        stats->ltp_ratio = 0.0f;
        stats->ltd_ratio = 0.0f;
        return;
    }

    for (size_t i = 0; i < n_events; i++) {
        if (events[i] == TRIT_PLASTICITY_LTP) stats->n_ltp++;
        else if (events[i] == TRIT_PLASTICITY_LTD) stats->n_ltd++;
        else stats->n_stable++;
    }

    stats->ltp_ratio = (float)stats->n_ltp / (float)n_events;
    stats->ltd_ratio = (float)stats->n_ltd / (float)n_events;
}

/**
 * @brief Get plasticity name string
 *
 * @param p Plasticity direction
 * @return String name
 */
static inline const char* trit_plasticity_name(trit_plasticity_t p) {
    switch (p) {
        case TRIT_PLASTICITY_LTD:    return "LTD";
        case TRIT_PLASTICITY_STABLE: return "STABLE";
        case TRIT_PLASTICITY_LTP:    return "LTP";
        default:                      return "INVALID";
    }
}

//=============================================================================
// Discrete Ternary STDP Weight Updates
//=============================================================================

/**
 * @brief Configuration for discrete ternary STDP
 *
 * WHAT: Parameters for ternary weight updates
 * WHY:  Enable discrete STDP with configurable step sizes
 * HOW:  Fixed step sizes with optional multiplicative/additive modes
 */
typedef struct {
    float ltp_step;               /**< Weight increase per LTP event */
    float ltd_step;               /**< Weight decrease per LTD event */
    float weight_min;             /**< Minimum weight bound */
    float weight_max;             /**< Maximum weight bound */
    bool multiplicative;          /**< Use multiplicative update (weight-dependent) */
    float tau_plus;               /**< LTP time constant (ms) */
    float tau_minus;              /**< LTD time constant (ms) */
    float dead_zone;              /**< Minimum timing for plasticity (ms) */
    bool soft_bounds;             /**< Use soft (exponential) bounds */
} trit_stdp_config_t;

/**
 * @brief Ternary STDP context for tracking state
 */
typedef struct {
    trit_stdp_config_t config;    /**< Configuration */
    trit_plasticity_stats_t stats;/**< Running statistics */
    float mean_weight;            /**< Running mean weight */
    uint64_t update_count;        /**< Total updates applied */
} trit_stdp_ctx_t;

/**
 * @brief Get default ternary STDP configuration
 *
 * DEFAULTS:
 * - ltp_step: 0.01 (1% increase)
 * - ltd_step: 0.01 (1% decrease)
 * - weight_min: 0.0
 * - weight_max: 1.0
 * - tau_plus: 20.0 ms
 * - tau_minus: 20.0 ms
 * - dead_zone: 1.0 ms
 *
 * @param config Configuration to initialize
 */
static inline void trit_stdp_default_config(trit_stdp_config_t* config) {
    if (!config) return;

    config->ltp_step = 0.01f;
    config->ltd_step = 0.01f;
    config->weight_min = 0.0f;
    config->weight_max = 1.0f;
    config->multiplicative = false;
    config->tau_plus = 20.0f;
    config->tau_minus = 20.0f;
    config->dead_zone = 1.0f;
    config->soft_bounds = false;
}

/**
 * @brief Initialize ternary STDP context
 *
 * @param ctx Context to initialize
 * @param config Configuration (NULL for defaults)
 */
static inline void trit_stdp_init(
    trit_stdp_ctx_t* ctx,
    const trit_stdp_config_t* config
) {
    if (!ctx) return;

    if (config) {
        ctx->config = *config;
    } else {
        trit_stdp_default_config(&ctx->config);
    }

    ctx->stats.n_ltp = 0;
    ctx->stats.n_stable = 0;
    ctx->stats.n_ltd = 0;
    ctx->stats.ltp_ratio = 0.0f;
    ctx->stats.ltd_ratio = 0.0f;
    ctx->mean_weight = 0.5f;
    ctx->update_count = 0;
}

/**
 * @brief Apply discrete ternary weight update
 *
 * WHAT: Update weight based on ternary plasticity direction
 * WHY:  Discrete updates enable efficient hardware implementation
 * HOW:  Add/subtract fixed step, respecting bounds
 *
 * MODES:
 * - Additive: w += step (or w -= step)
 * - Multiplicative: w += step * (w_max - w) for LTP
 *                   w -= step * (w - w_min) for LTD
 *
 * @param weight Current weight (modified in place)
 * @param direction Ternary plasticity direction
 * @param config STDP configuration
 * @return New weight value
 */
static inline float trit_stdp_apply(
    float weight,
    trit_plasticity_t direction,
    const trit_stdp_config_t* config
) {
    if (!config) return weight;

    float new_weight = weight;

    switch (direction) {
        case TRIT_PLASTICITY_LTP:
            if (config->multiplicative) {
                /* Multiplicative: step proportional to distance from max */
                new_weight += config->ltp_step * (config->weight_max - weight);
            } else if (config->soft_bounds) {
                /* Soft bounds: exponential approach */
                float headroom = config->weight_max - weight;
                new_weight += config->ltp_step * headroom / config->weight_max;
            } else {
                /* Additive: fixed step */
                new_weight += config->ltp_step;
            }
            break;

        case TRIT_PLASTICITY_LTD:
            if (config->multiplicative) {
                /* Multiplicative: step proportional to distance from min */
                new_weight -= config->ltd_step * (weight - config->weight_min);
            } else if (config->soft_bounds) {
                /* Soft bounds: exponential approach */
                float headroom = weight - config->weight_min;
                new_weight -= config->ltd_step * headroom / config->weight_max;
            } else {
                /* Additive: fixed step */
                new_weight -= config->ltd_step;
            }
            break;

        case TRIT_PLASTICITY_STABLE:
        default:
            /* No change */
            break;
    }

    /* Apply hard bounds */
    if (new_weight < config->weight_min) new_weight = config->weight_min;
    if (new_weight > config->weight_max) new_weight = config->weight_max;

    return new_weight;
}

/**
 * @brief Process spike pair and update weight
 *
 * WHAT: Full STDP pipeline from spike timing to weight update
 * WHY:  Convenience function for common use case
 * HOW:  Compute direction from timing, then apply update
 *
 * @param weight Current weight (modified)
 * @param dt_ms Spike timing difference (t_post - t_pre)
 * @param ctx STDP context (modified with statistics)
 * @return New weight value
 */
static inline float trit_stdp_process_pair(
    float weight,
    float dt_ms,
    trit_stdp_ctx_t* ctx
) {
    if (!ctx) return weight;

    /* Compute ternary direction from timing */
    trit_plasticity_t direction = trit_stdp_compute(
        dt_ms,
        ctx->config.tau_plus,
        ctx->config.tau_minus,
        ctx->config.dead_zone
    );

    /* Apply update */
    float new_weight = trit_stdp_apply(weight, direction, &ctx->config);

    /* Update statistics */
    if (direction == TRIT_PLASTICITY_LTP) {
        ctx->stats.n_ltp++;
    } else if (direction == TRIT_PLASTICITY_LTD) {
        ctx->stats.n_ltd++;
    } else {
        ctx->stats.n_stable++;
    }
    ctx->update_count++;

    /* Update running mean */
    float alpha = 0.001f;
    ctx->mean_weight = (1.0f - alpha) * ctx->mean_weight + alpha * new_weight;

    return new_weight;
}

/**
 * @brief Batch process multiple synapses with ternary STDP
 *
 * WHAT: Apply ternary STDP to array of weights
 * WHY:  Efficient vectorized processing
 * HOW:  Iterate and apply per-synapse updates
 *
 * @param weights Array of weights (modified in place)
 * @param directions Array of ternary directions
 * @param n_synapses Number of synapses
 * @param config STDP configuration
 * @param stats Output: batch statistics (optional)
 */
static inline void trit_stdp_batch_apply(
    float* weights,
    const trit_plasticity_t* directions,
    size_t n_synapses,
    const trit_stdp_config_t* config,
    trit_plasticity_stats_t* stats
) {
    if (!weights || !directions || !config || n_synapses == 0) return;

    trit_plasticity_stats_t local_stats = {0};

    for (size_t i = 0; i < n_synapses; i++) {
        weights[i] = trit_stdp_apply(weights[i], directions[i], config);

        /* Count events */
        if (directions[i] == TRIT_PLASTICITY_LTP) {
            local_stats.n_ltp++;
        } else if (directions[i] == TRIT_PLASTICITY_LTD) {
            local_stats.n_ltd++;
        } else {
            local_stats.n_stable++;
        }
    }

    /* Compute ratios */
    local_stats.ltp_ratio = (float)local_stats.n_ltp / (float)n_synapses;
    local_stats.ltd_ratio = (float)local_stats.n_ltd / (float)n_synapses;

    if (stats) {
        *stats = local_stats;
    }
}

/**
 * @brief Convert weight array to ternary representation
 *
 * WHAT: Quantize continuous weights to ternary {-1, 0, +1}
 * WHY:  Extreme compression for ternary neural networks
 * HOW:  Threshold-based ternarization
 *
 * @param weights Input floating-point weights
 * @param ternary_out Output ternary weights
 * @param n_weights Number of weights
 * @param threshold Threshold for non-zero assignment
 * @param scale Output: computed scale factor
 */
static inline void trit_weights_ternarize(
    const float* weights,
    trit_t* ternary_out,
    size_t n_weights,
    float threshold,
    float* scale
) {
    if (!weights || !ternary_out || n_weights == 0) return;

    /* Compute statistics for scaling */
    float sum_pos = 0.0f, sum_neg = 0.0f;
    size_t n_pos = 0, n_neg = 0;

    for (size_t i = 0; i < n_weights; i++) {
        if (weights[i] > threshold) {
            ternary_out[i] = TRIT_POSITIVE;
            sum_pos += weights[i];
            n_pos++;
        } else if (weights[i] < -threshold) {
            ternary_out[i] = TRIT_NEGATIVE;
            sum_neg += -weights[i];
            n_neg++;
        } else {
            ternary_out[i] = TRIT_UNKNOWN;
        }
    }

    /* Compute scale as mean of absolute non-zero values */
    if (scale) {
        float total = sum_pos + sum_neg;
        size_t count = n_pos + n_neg;
        *scale = (count > 0) ? (total / (float)count) : 1.0f;
    }
}

/**
 * @brief Reconstruct continuous weights from ternary
 *
 * WHAT: Convert ternary weights back to floating-point
 * WHY:  Enable forward pass with ternarized weights
 * HOW:  Multiply ternary by scale factor
 *
 * @param ternary_weights Input ternary weights
 * @param weights_out Output floating-point weights
 * @param n_weights Number of weights
 * @param scale Scale factor
 */
static inline void trit_weights_dequantize(
    const trit_t* ternary_weights,
    float* weights_out,
    size_t n_weights,
    float scale
) {
    if (!ternary_weights || !weights_out || n_weights == 0) return;

    for (size_t i = 0; i < n_weights; i++) {
        weights_out[i] = (float)ternary_weights[i] * scale;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PLASTICITY_TERNARY_H */
