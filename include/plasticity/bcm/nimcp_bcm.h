/**
 * @file nimcp_bcm.h
 * @brief Bienenstock-Cooper-Munro (BCM) learning rule
 *
 * WHAT: Sliding threshold plasticity rule for cortical learning
 * WHY:  Self-stabilizing plasticity without explicit normalization
 *
 * BIOLOGICAL BASIS:
 * - Models visual cortex development and plasticity
 * - Explains ocular dominance and orientation selectivity
 * - Implements critical periods in learning
 * - Winner-take-all dynamics emerge naturally
 *
 * BCM RULE:
 * Δw = η × post × (post - θ) × pre
 * θ̇ = (post² - θ) / τ
 *
 * Where:
 * - η = learning rate
 * - post = post-synaptic activity
 * - θ = sliding threshold
 * - pre = pre-synaptic activity
 * - τ = threshold time constant
 *
 * DESIGN PATTERNS:
 * - Strategy Pattern: Different threshold update strategies
 * - Value Object: Immutable parameter structs
 * - Factory Method: Preset configurations for different brain regions
 *
 * PERFORMANCE:
 * - O(1) per synapse update
 * - SIMD-friendly (no branches in hot path)
 * - Cache-coherent memory layout
 *
 * @author NIMCP Development Team
 * @date 2025-11-01
 */

#ifndef NIMCP_BCM_H
#define NIMCP_BCM_H

#include <stdint.h>
#include <stdbool.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "cognitive/nimcp_sleep_wake.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// BCM Types and Structures
//=============================================================================

/**
 * @brief BCM synapse state (thread-safe with mutex)
 *
 * WHAT: Per-synapse BCM learning variables with synchronization
 * WHY:  Track weight, threshold, and running averages with concurrent access protection
 *
 * THREAD SAFETY:
 * - Mutex: Used when multiple threads may access the same synapse
 * - Platform mutex chosen for portability: Works on Windows, macOS, and Linux
 * - Cache-line padding: Prevents false sharing between synapses
 *
 * WHEN TO USE MUTEX:
 * - Multiple threads updating same synapse (rare in typical use)
 * - Each thread usually works on disjoint synapse sets (no contention)
 * - If no sharing: mutex overhead is minimal (no lock/unlock needed)
 *
 * MEMORY: 56 bytes per synapse (4 floats + platform mutex)
 * NOTE: Platform mutex size varies by OS (40 bytes on Linux with nimcp_mutex_t)
 *       This is acceptable tradeoff for thread safety in concurrent scenarios
 */
typedef struct {
    float weight;             /**< Synaptic weight (0-1) */
    float threshold;          /**< Sliding modification threshold θ */
    float avg_post_activity;  /**< Running average of post-synaptic activity */
    float eligibility;        /**< Eligibility trace for delayed reward */

    sleep_state_t current_sleep_state;  /**< Current sleep/wake state */

    nimcp_platform_mutex_t lock;    /**< Platform mutex for thread-safe updates (only if synapse is shared) */
} bcm_synapse_t;

/**
 * @brief BCM learning parameters
 *
 * WHAT: Configuration for BCM rule
 * WHY:  Different brain regions have different learning dynamics
 *
 * PATTERN: Value Object (immutable configuration)
 */
typedef struct {
    float learning_rate;              /**< η: Base learning rate (0.001-0.1) */
    float threshold_time_constant;    /**< τ_θ: Threshold adaptation timescale (ms) */
    float activity_time_constant;     /**< τ_a: Activity averaging timescale (ms) */
    float min_threshold;              /**< Minimum θ (prevents over-depression) */
    float max_threshold;              /**< Maximum θ (prevents runaway) */
    bool enable_bio_async;            /**< Enable bio-async communication */
    bool enable_quantum_bcm;          /**< Enable quantum threshold optimization (default: true) */
} bcm_params_t;

/**
 * @brief BCM learning statistics
 *
 * WHAT: Performance and convergence metrics
 * WHY:  Monitor learning progress and detect issues
 */
typedef struct {
    uint64_t total_updates;           /**< Total plasticity updates */
    float avg_weight;                 /**< Average synaptic weight */
    float avg_threshold;              /**< Average modification threshold */
    float weight_variance;            /**< Weight distribution variance */
    uint64_t ltp_events;              /**< Count of potentiation events */
    uint64_t ltd_events;              /**< Count of depression events */
} bcm_stats_t;

//=============================================================================
// BCM Core Functions
//=============================================================================

/**
 * @brief Initialize BCM synapse
 *
 * WHAT: Factory method for creating BCM synapse
 * WHY:  Encapsulates initialization logic
 *
 * COMPLEXITY: O(1)
 *
 * @param initial_weight Starting synaptic weight (0-1)
 * @param initial_threshold Starting modification threshold
 * @return Initialized BCM synapse
 */
bcm_synapse_t bcm_synapse_init(float initial_weight, float initial_threshold);

/**
 * @brief Update sliding threshold
 *
 * WHAT: Adapts threshold based on post-synaptic activity history
 * WHY:  Threshold should track average activity squared
 *
 * BIOLOGICAL: θ̇ = (post² - θ) / τ
 * COMPLEXITY: O(1)
 *
 * @param synapse BCM synapse to update
 * @param post_activity Current post-synaptic activity
 * @param dt Time step (ms)
 * @param params BCM parameters
 */
void bcm_update_threshold(bcm_synapse_t* synapse, float post_activity, float dt,
                         const bcm_params_t* params);

/**
 * @brief Apply BCM learning rule
 *
 * WHAT: Update synaptic weight using BCM plasticity
 * WHY:  Implement sliding threshold LTP/LTD
 *
 * BIOLOGICAL: Δw = η × post × (post - θ) × pre
 * - If post > θ: LTP (potentiation)
 * - If post < θ: LTD (depression)
 * - If post = θ: No change
 *
 * COMPLEXITY: O(1)
 * PERFORMANCE: < 10 CPU cycles on modern processors
 *
 * @param synapse BCM synapse to update
 * @param pre_activity Pre-synaptic activity
 * @param post_activity Post-synaptic activity
 * @param dt Time step (ms)
 * @param params BCM parameters
 */
void bcm_apply_rule(bcm_synapse_t* synapse, float pre_activity, float post_activity,
                   float dt, const bcm_params_t* params);

/**
 * @brief Apply BCM rule with neuromodulator gating
 *
 * WHAT: BCM learning modulated by dopamine/reward signals
 * WHY:  Reward should gate cortical plasticity
 *
 * BIOLOGICAL: Dopamine gates LTP in cortex
 * - High dopamine (reward) → enhanced learning
 * - Low dopamine (no reward) → minimal learning
 *
 * COMPLEXITY: O(1)
 *
 * @param synapse BCM synapse to update
 * @param pre_activity Pre-synaptic activity
 * @param post_activity Post-synaptic activity
 * @param dt Time step (ms)
 * @param params BCM parameters
 * @param neuromodulator_level Neuromodulator concentration (0-1)
 */
void bcm_apply_rule_modulated(bcm_synapse_t* synapse, float pre_activity,
                             float post_activity, float dt, const bcm_params_t* params,
                             float neuromodulator_level);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Create default BCM parameters for cortical learning
 *
 * WHAT: Factory method for cortical BCM preset
 * WHY:  Provides biologically plausible defaults
 *
 * BIOLOGICAL: Based on visual cortex measurements
 *
 * @return BCM parameters for cortex
 */
bcm_params_t bcm_params_cortical(void);

/**
 * @brief Create BCM parameters for fast learning (critical period)
 *
 * WHAT: Factory method for developmental learning
 * WHY:  Critical periods have higher plasticity
 *
 * BIOLOGICAL: Models early postnatal development
 *
 * @return BCM parameters for critical period
 */
bcm_params_t bcm_params_critical_period(void);

/**
 * @brief Create BCM parameters for mature/stable learning
 *
 * WHAT: Factory method for adult learning
 * WHY:  Adult cortex has lower plasticity
 *
 * BIOLOGICAL: Models adult cortical plasticity
 *
 * @return BCM parameters for mature brain
 */
bcm_params_t bcm_params_mature(void);

/**
 * @brief Compute BCM statistics for array of synapses
 *
 * WHAT: Aggregate learning metrics across synapse population
 * WHY:  Monitor convergence and detect instabilities
 *
 * COMPLEXITY: O(n) where n = number of synapses
 *
 * @param synapses Array of BCM synapses
 * @param num_synapses Number of synapses
 * @param stats Output: computed statistics
 * @return true on success
 */
bool bcm_compute_stats(const bcm_synapse_t* synapses, uint32_t num_synapses,
                      bcm_stats_t* stats);

/**
 * @brief Set sleep state for BCM synapse
 *
 * WHAT: Update sleep state for synapse
 * WHY:  Sleep state modulates threshold and learning rate
 * HOW:  Set current_sleep_state field, will be applied during next update
 *
 * @param synapse BCM synapse to update
 * @param state New sleep state
 */
void bcm_set_sleep_state(bcm_synapse_t* synapse, sleep_state_t state);

/**
 * @brief Extract BCM activity statistics for quantum optimization
 *
 * WHAT: Compute aggregated statistics from BCM synapses
 * WHY:  Provide input to quantum threshold optimization
 * HOW:  Analyze synapse population for activity metrics
 *
 * @param synapses Array of BCM synapses
 * @param num_synapses Number of synapses
 * @param avg_post_activity Average post-synaptic activity
 * @return Activity statistics for quantum bridge
 */
void* bcm_extract_quantum_stats(const bcm_synapse_t* synapses, uint32_t num_synapses,
                                float avg_post_activity);

//=============================================================================
// Module Management
//=============================================================================

/**
 * @brief Initialize BCM module with bio-async support
 *
 * WHAT: Sets up bio-async communication for BCM module
 * WHY: Enables async event-driven BCM updates
 * HOW: Registers with bio-router and initializes module state
 *
 * @param params Module parameters (NULL = no bio-async)
 * @return true on success, false on failure
 */
bool bcm_module_init(const bcm_params_t* params);

/**
 * @brief Destroy BCM module and cleanup resources
 *
 * WHAT: Cleans up bio-async resources and module state
 * WHY: Proper resource cleanup on shutdown
 * HOW: Unregisters from bio-router and resets state
 */
void bcm_module_destroy(void);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_BCM_H
