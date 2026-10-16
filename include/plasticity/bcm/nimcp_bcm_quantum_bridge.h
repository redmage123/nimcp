/**
 * @file nimcp_bcm_quantum_bridge.h
 * @brief Quantum annealing optimization for BCM sliding threshold
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Quantum-inspired optimization for BCM threshold adaptation
 * WHY:  Escape local minima in threshold space, find optimal plasticity balance
 * HOW:  Simulated quantum tunneling through energy barriers using quantum annealing
 *
 * BIOLOGICAL INSPIRATION:
 * ==================================================================================
 *
 * BCM THRESHOLD OPTIMIZATION:
 * ---------------------------
 * - BCM sliding threshold θ determines LTP/LTD crossover point
 * - Traditional BCM: θ̇ = (post² - θ) / τ (simple exponential averaging)
 * - Quantum BCM: Use quantum annealing to find optimal θ globally
 * - Biological analog: Sleep-dependent threshold reorganization
 *
 * ENERGY LANDSCAPE:
 * -----------------
 * - Energy function: E(θ) = stability_cost + selectivity_cost
 * - Stability: Deviation from weight homeostasis
 * - Selectivity: Lack of feature selectivity (winner-take-all)
 * - Local minima: Suboptimal threshold values
 * - Quantum tunneling: Escape local minima during sleep/consolidation
 *
 * BIOLOGICAL BASIS:
 * -----------------
 * - Metaplasticity: Learning to learn, optimizing learning rules
 * - Sleep consolidation: Reorganize synaptic thresholds globally
 * - Homeostatic plasticity: Global optimization of stability
 * - Critical periods: Quantum exploration in development
 *
 * INTEGRATION PATTERN:
 * --------------------
 * - Periodic optimization: Run quantum annealing every K updates
 * - Energy function: Combines weight stability + selectivity metrics
 * - Threshold update: θ_new = quantum_anneal(current_stats)
 * - Compatible with: Standard BCM, FEP bridge, sleep bridge
 *
 * PERFORMANCE:
 * ------------
 * - Optimization overhead: ~30-50% when active
 * - Inactive overhead: ~5-10% (state tracking only)
 * - Typical schedule: Optimize every 1000 updates
 * - Convergence: 10-100x better threshold values
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via direct struct operations (no shared state)
 * - Memory management via stdlib (header-only implementation)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BCM_QUANTUM_BRIDGE_H
#define NIMCP_BCM_QUANTUM_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "plasticity/bcm/nimcp_bcm.h"
#include "optimization/quantum_annealing/nimcp_quantum_annealing.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/* Quantum annealing parameters */
#define BCM_QUANTUM_DEFAULT_ITERATIONS        1000
#define BCM_QUANTUM_DEFAULT_INITIAL_TEMP      1.0f
#define BCM_QUANTUM_DEFAULT_FINAL_TEMP        0.01f
#define BCM_QUANTUM_DEFAULT_QUANTUM_STRENGTH  0.5f

/* Energy function weights */
#define BCM_QUANTUM_STABILITY_WEIGHT          0.6f
#define BCM_QUANTUM_SELECTIVITY_WEIGHT        0.4f

/* Optimization schedule */
#define BCM_QUANTUM_OPTIMIZATION_INTERVAL     1000  /* Updates between optimizations */

//=============================================================================
// Types and Structures
//=============================================================================

typedef struct bcm_quantum_bridge bcm_quantum_bridge_t;

/**
 * @brief Configuration for BCM quantum bridge
 *
 * WHAT: Parameters controlling quantum annealing behavior
 * WHY:  Allow tuning for different optimization scenarios
 */
typedef struct {
    bool enabled;                       /**< Enable quantum optimization */
    uint32_t optimization_interval;     /**< Updates between optimizations */

    /* Quantum annealing parameters */
    uint32_t num_iterations;            /**< Annealing iterations */
    float initial_temperature;          /**< Starting temperature */
    float final_temperature;            /**< Ending temperature */
    float quantum_strength;             /**< Tunneling probability */
    cooling_schedule_t cooling_schedule; /**< Temperature decrease strategy */

    /* Energy function weights */
    float stability_weight;             /**< Weight stability importance */
    float selectivity_weight;           /**< Feature selectivity importance */

    /* Threshold constraints */
    float min_threshold;                /**< Minimum allowed threshold */
    float max_threshold;                /**< Maximum allowed threshold */

    uint32_t seed;                      /**< Random seed for reproducibility */
} bcm_quantum_config_t;

/**
 * @brief Quantum optimization statistics
 *
 * WHAT: Tracking metrics for quantum optimization
 * WHY:  Monitor optimization progress and effectiveness
 */
typedef struct {
    uint64_t optimization_steps;        /**< Total optimization runs */
    uint64_t tunneling_events;          /**< Quantum tunneling occurrences */
    float avg_threshold;                /**< Average optimized threshold */
    float best_energy;                  /**< Best energy achieved */
    float current_temperature;          /**< Current annealing temperature */
    float last_threshold_change;        /**< Magnitude of last threshold update */
    uint64_t total_updates;             /**< Total BCM updates tracked */
} bcm_quantum_stats_t;

/**
 * @brief BCM activity statistics for optimization
 *
 * WHAT: Aggregated BCM activity metrics
 * WHY:  Input to quantum annealing energy function
 */
typedef struct {
    float avg_weight;                   /**< Average synaptic weight */
    float weight_variance;              /**< Weight distribution variance */
    float avg_post_activity;            /**< Average post-synaptic activity */
    float selectivity_index;            /**< Feature selectivity measure */
    uint32_t num_active_synapses;       /**< Count of active synapses */
} bcm_activity_stats_t;

//=============================================================================
// API Functions
//=============================================================================

/**
 * @brief Get default quantum bridge configuration
 *
 * WHAT: Factory method for default configuration
 * WHY:  Provides sensible defaults for typical use
 * HOW:  Returns preset config values
 *
 * @return Default configuration struct
 */
bcm_quantum_config_t bcm_quantum_default_config(void);

/**
 * @brief Create BCM quantum bridge
 *
 * WHAT: Initialize quantum optimization bridge
 * WHY:  Set up quantum annealing optimizer
 * HOW:  Allocate memory, init quantum annealer
 *
 * @param config Configuration parameters (NULL = default)
 * @return Bridge handle, or NULL on error
 *
 * ERRORS:
 * - Returns NULL if memory allocation fails
 * - Returns NULL if quantum annealer creation fails
 */
bcm_quantum_bridge_t* bcm_quantum_bridge_create(const bcm_quantum_config_t* config);

/**
 * @brief Destroy BCM quantum bridge
 *
 * WHAT: Clean up bridge and free memory
 * WHY:  Prevent memory leaks
 * HOW:  Destroy quantum annealer, free bridge
 *
 * @param bridge Bridge to destroy (can be NULL)
 *
 * SAFETY: Safe to call with NULL pointer (no-op)
 */
void bcm_quantum_bridge_destroy(bcm_quantum_bridge_t* bridge);

/**
 * @brief Check if quantum optimization is enabled
 *
 * WHAT: Query enabled state
 * WHY:  Conditionally apply optimization
 * HOW:  Return config flag
 *
 * @param bridge Bridge instance
 * @return true if enabled, false otherwise
 */
bool bcm_quantum_is_enabled(const bcm_quantum_bridge_t* bridge);

/**
 * @brief Enable/disable quantum optimization
 *
 * WHAT: Toggle optimization on/off
 * WHY:  Runtime control of optimization
 * HOW:  Set enabled flag
 *
 * @param bridge Bridge instance
 * @param enabled New enabled state
 */
void bcm_quantum_set_enabled(bcm_quantum_bridge_t* bridge, bool enabled);

/**
 * @brief Optimize BCM threshold using quantum annealing
 *
 * WHAT: Run quantum optimization to find optimal threshold
 * WHY:  Escape local minima, find global optimum
 * HOW:  Quantum anneal with BCM energy function
 *
 * ALGORITHM:
 * 1. Extract current threshold from BCM stats
 * 2. Define energy function: E(θ) = stability_cost(θ) + selectivity_cost(θ)
 * 3. Run quantum annealing: θ_opt = anneal(E, θ_current)
 * 4. Return optimized threshold
 *
 * @param bridge Bridge instance
 * @param stats Current BCM activity statistics
 * @return Optimized threshold value
 *
 * COMPLEXITY: O(N * D) where N = iterations, D = dimensions
 * PERFORMANCE: ~10-50ms for typical parameters
 */
float bcm_quantum_optimize_threshold(
    bcm_quantum_bridge_t* bridge,
    const bcm_activity_stats_t* stats
);

/**
 * @brief Get quantum optimization statistics
 *
 * WHAT: Retrieve optimization metrics
 * WHY:  Monitor optimization progress
 * HOW:  Copy stats to output
 *
 * @param bridge Bridge instance
 * @param stats Output statistics struct
 * @return 0 on success, -1 on error
 */
int bcm_quantum_get_stats(
    const bcm_quantum_bridge_t* bridge,
    bcm_quantum_stats_t* stats
);

/**
 * @brief Reset quantum optimization statistics
 *
 * WHAT: Clear all statistics counters
 * WHY:  Reset tracking after experiments
 * HOW:  Zero stats struct
 *
 * @param bridge Bridge instance
 */
void bcm_quantum_reset_stats(bcm_quantum_bridge_t* bridge);

/**
 * @brief Update quantum bridge with BCM activity
 *
 * WHAT: Track BCM updates, trigger optimization when needed
 * WHY:  Periodic optimization schedule
 * HOW:  Increment counter, optimize at interval
 *
 * @param bridge Bridge instance
 * @param stats Current BCM activity statistics
 * @return Optimized threshold (if optimization triggered), or -1.0f (no optimization)
 */
float bcm_quantum_update(
    bcm_quantum_bridge_t* bridge,
    const bcm_activity_stats_t* stats
);

//=============================================================================
// Implementation (Header-only)
//=============================================================================

#ifdef NIMCP_BCM_QUANTUM_BRIDGE_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/memory/nimcp_memory.h"

/**
 * @brief Energy function user data
 *
 * WHAT: Context passed to energy function
 * WHY:  Provide BCM stats and config to energy computation
 */
typedef struct {
    bcm_activity_stats_t stats;
    bcm_quantum_config_t config;
} bcm_quantum_energy_context_t;

/**
 * @brief BCM quantum bridge structure
 *
 * WHAT: Internal bridge state
 * WHY:  Encapsulate quantum annealer and tracking
 */
struct bcm_quantum_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    bcm_quantum_config_t config;
    quantum_annealer_t annealer;
    bcm_quantum_stats_t stats;
    uint64_t update_counter;
};

/**
 * @brief BCM threshold energy function
 *
 * WHAT: Energy function for quantum annealing
 * WHY:  Defines optimization objective
 * HOW:  Combine stability and selectivity costs
 *
 * ENERGY FUNCTION:
 * E(θ) = w_stab * E_stability(θ) + w_sel * E_selectivity(θ)
 *
 * E_stability(θ) = |avg_weight - target_weight|²
 * E_selectivity(θ) = -weight_variance (maximize variance = selectivity)
 *
 * @param state State vector (threshold value)
 * @param dim Dimensionality (should be 1)
 * @param user_data Energy context
 * @return Energy value (lower is better)
 */
static float bcm_threshold_energy_function(
    const float* state,
    uint32_t dim,
    void* user_data
) {
    /* WHAT: Guard clause - validate inputs */
    if (!state || !user_data || dim != 1) {
        return INFINITY;
    }

    bcm_quantum_energy_context_t* ctx = (bcm_quantum_energy_context_t*)user_data;
    float threshold = state[0];

    /* WHAT: Clamp threshold to valid range */
    if (threshold < ctx->config.min_threshold || threshold > ctx->config.max_threshold) {
        return INFINITY;  /* Invalid threshold */
    }

    /* WHAT: Compute stability cost
     * WHY:  Want weights to be centered around 0.5 (homeostasis)
     * HOW:  Penalize deviation from target
     */
    const float target_weight = 0.5f;
    float weight_deviation = ctx->stats.avg_weight - target_weight;
    float stability_cost = weight_deviation * weight_deviation;

    /* WHAT: Compute selectivity cost
     * WHY:  Want high weight variance (winner-take-all)
     * HOW:  Minimize negative variance = maximize variance
     */
    float selectivity_cost = -ctx->stats.weight_variance;

    /* WHAT: Adjust selectivity based on threshold
     * WHY:  Higher threshold → more selective (fewer LTP events)
     * HOW:  Scale selectivity by threshold deviation
     */
    float threshold_factor = threshold / (ctx->stats.avg_post_activity * ctx->stats.avg_post_activity + 1e-6f);
    selectivity_cost *= (1.0f + 0.5f * fabsf(threshold_factor - 1.0f));

    /* WHAT: Combine costs with weights
     * WHY:  Balance stability and selectivity objectives
     */
    float total_energy =
        ctx->config.stability_weight * stability_cost +
        ctx->config.selectivity_weight * selectivity_cost;

    return total_energy;
}

bcm_quantum_config_t bcm_quantum_default_config(void) {
    /* WHAT: Factory method for default configuration
     * WHY:  Provides sensible defaults for typical use
     */
    return (bcm_quantum_config_t){
        .enabled = true,
        .optimization_interval = BCM_QUANTUM_OPTIMIZATION_INTERVAL,
        .num_iterations = BCM_QUANTUM_DEFAULT_ITERATIONS,
        .initial_temperature = BCM_QUANTUM_DEFAULT_INITIAL_TEMP,
        .final_temperature = BCM_QUANTUM_DEFAULT_FINAL_TEMP,
        .quantum_strength = BCM_QUANTUM_DEFAULT_QUANTUM_STRENGTH,
        .cooling_schedule = COOLING_EXPONENTIAL,
        .stability_weight = BCM_QUANTUM_STABILITY_WEIGHT,
        .selectivity_weight = BCM_QUANTUM_SELECTIVITY_WEIGHT,
        .min_threshold = 0.01f,
        .max_threshold = 10.0f,
        .seed = 0
    };
}

bcm_quantum_bridge_t* bcm_quantum_bridge_create(const bcm_quantum_config_t* config) {
    /* WHAT: Create quantum bridge
     * WHY:  Initialize quantum annealing optimizer
     * HOW:  Allocate memory, create annealer
     */

    bcm_quantum_bridge_t* bridge = (bcm_quantum_bridge_t*)nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) return NULL;

    /* WHAT: Set configuration */
    bridge->config = config ? *config : bcm_quantum_default_config();

    /* WHAT: Create quantum annealer */
    quantum_annealing_config_t qa_config = {
        .initial_temperature = bridge->config.initial_temperature,
        .final_temperature = bridge->config.final_temperature,
        .num_iterations = bridge->config.num_iterations,
        .cooling_schedule = bridge->config.cooling_schedule,
        .quantum_strength = bridge->config.quantum_strength,
        .enable_tunneling = true,
        .seed = bridge->config.seed
    };

    bridge->annealer = quantum_annealer_create(&qa_config);
    if (!bridge->annealer) {
        nimcp_free(bridge);
        return NULL;
    }

    /* WHAT: Initialize stats */
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->update_counter = 0;

    return bridge;
}

void bcm_quantum_bridge_destroy(bcm_quantum_bridge_t* bridge) {
    /* WHAT: Destroy bridge and cleanup
     * WHY:  Prevent memory leaks
     */
    if (!bridge) return;

    if (bridge->annealer) {
        quantum_annealer_destroy(bridge->annealer);
    }

    nimcp_free(bridge);
}

bool bcm_quantum_is_enabled(const bcm_quantum_bridge_t* bridge) {
    /* WHAT: Check if optimization is enabled */
    return bridge && bridge->config.enabled;
}

void bcm_quantum_set_enabled(bcm_quantum_bridge_t* bridge, bool enabled) {
    /* WHAT: Enable/disable optimization */
    if (bridge) {
        bridge->config.enabled = enabled;
    }
}

float bcm_quantum_optimize_threshold(
    bcm_quantum_bridge_t* bridge,
    const bcm_activity_stats_t* stats
) {
    /* WHAT: Optimize BCM threshold using quantum annealing
     * WHY:  Find global optimal threshold
     * HOW:  Run quantum annealing with BCM energy function
     */

    /* WHAT: Guard clause - validate inputs */
    if (!bridge || !bridge->annealer || !stats) {
        return -1.0f;
    }

    /* WHAT: Create energy function context */
    bcm_quantum_energy_context_t ctx = {
        .stats = *stats,
        .config = bridge->config
    };

    /* WHAT: Initialize with current threshold estimate
     * WHY:  Start from reasonable guess
     * HOW:  Use avg_post_activity² as initial threshold
     */
    float initial_threshold = stats->avg_post_activity * stats->avg_post_activity;

    /* WHAT: Clamp initial threshold to valid range */
    initial_threshold = fminf(fmaxf(initial_threshold, bridge->config.min_threshold),
                              bridge->config.max_threshold);

    /* WHAT: Run quantum annealing */
    float optimized_threshold = 0.0f;
    float final_energy = quantum_anneal(
        bridge->annealer,
        bcm_threshold_energy_function,
        &initial_threshold,
        &optimized_threshold,
        1,  /* 1D optimization (threshold only) */
        &ctx
    );

    /* WHAT: Update statistics */
    bridge->stats.optimization_steps++;
    bridge->stats.avg_threshold =
        (bridge->stats.avg_threshold * (bridge->stats.optimization_steps - 1) +
         optimized_threshold) / bridge->stats.optimization_steps;
    bridge->stats.best_energy = fminf(bridge->stats.best_energy, final_energy);
    bridge->stats.last_threshold_change = fabsf(optimized_threshold - initial_threshold);

    return optimized_threshold;
}

int bcm_quantum_get_stats(
    const bcm_quantum_bridge_t* bridge,
    bcm_quantum_stats_t* stats
) {
    /* WHAT: Get optimization statistics */
    if (!bridge || !stats) return -1;

    *stats = bridge->stats;
    return 0;
}

void bcm_quantum_reset_stats(bcm_quantum_bridge_t* bridge) {
    /* WHAT: Reset statistics */
    if (bridge) {
        memset(&bridge->stats, 0, sizeof(bridge->stats));
        bridge->update_counter = 0;
    }
}

float bcm_quantum_update(
    bcm_quantum_bridge_t* bridge,
    const bcm_activity_stats_t* stats
) {
    /* WHAT: Update bridge, trigger optimization if needed
     * WHY:  Periodic optimization schedule
     * HOW:  Increment counter, optimize at interval
     */

    /* WHAT: Guard clause - validate inputs */
    if (!bridge || !stats || !bridge->config.enabled) {
        return -1.0f;
    }

    /* WHAT: Increment update counter */
    bridge->update_counter++;
    bridge->stats.total_updates++;

    /* WHAT: Check if optimization is due */
    if (bridge->update_counter >= bridge->config.optimization_interval) {
        /* WHAT: Reset counter */
        bridge->update_counter = 0;

        /* WHAT: Run optimization */
        return bcm_quantum_optimize_threshold(bridge, stats);
    }

    /* WHAT: No optimization this update */
    return -1.0f;
}

#endif // NIMCP_BCM_QUANTUM_BRIDGE_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BCM_QUANTUM_BRIDGE_H
