/**
 * @file nimcp_fault_attention.h
 * @brief Attention Mechanism for Error Prioritization
 * @version 1.0.0
 * @date 2025-11-20
 *
 * WHAT: Intelligent fault prioritization using attention-based weighting
 * WHY:  Limited cognitive resources require focusing on critical errors first
 * HOW:  Compute attention weights from severity, recency, frequency, and impact
 *
 * BIOLOGICAL BASIS:
 * - Mimics attentional control in prefrontal cortex
 * - Salience mapping determines resource allocation
 * - Adaptive learning adjusts priorities based on recovery outcomes
 *
 * ATTENTION FORMULA:
 * attention[i] = severity_weight   * fault[i].severity +
 *                recency_weight    * (1.0 / time_since_fault) +
 *                frequency_weight  * fault[i].occurrence_count +
 *                impact_weight     * fault[i].users_affected
 *
 * FEATURES:
 * - Multi-factor attention computation
 * - Automatic focus selection (highest priority)
 * - Adaptive weight learning from recovery outcomes
 * - Configurable priority factors
 * - Performance: <10μs per computation (10 faults)
 *
 * INTEGRATION POINTS:
 * 1. Fault Detection (src/utils/fault_tolerance/nimcp_health_monitor.h)
 *    - Receives active_fault_t array
 *    - Computes attention weights
 *
 * 2. Recovery System (src/utils/fault_tolerance/nimcp_recovery.h)
 *    - Gets focused fault for priority recovery
 *    - Reports recovery outcomes for learning
 *
 * 3. Resource Allocation (src/utils/fault_tolerance/nimcp_recovery_pool.h)
 *    - Allocates resources based on attention weights
 *    - Prevents low-priority faults from blocking critical ones
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FAULT_ATTENTION_H
#define NIMCP_FAULT_ATTENTION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define FAULT_ATTENTION_MAX_FAULTS 64                  /**< Maximum faults tracked */
#define FAULT_ATTENTION_DEFAULT_SEVERITY_WEIGHT 0.4f   /**< Default severity weight */
#define FAULT_ATTENTION_DEFAULT_RECENCY_WEIGHT 0.3f    /**< Default recency weight */
#define FAULT_ATTENTION_DEFAULT_FREQUENCY_WEIGHT 0.2f  /**< Default frequency weight */
#define FAULT_ATTENTION_DEFAULT_IMPACT_WEIGHT 0.1f     /**< Default impact weight */
#define FAULT_ATTENTION_DEFAULT_LEARNING_RATE 0.05f    /**< Default learning rate */

//=============================================================================
// Types and Structures
//=============================================================================

/**
 * @brief Active fault representation
 *
 * WHAT: Fault data for attention computation
 * WHY:  Encapsulates all factors needed for prioritization
 * HOW:  Populated by health monitor, consumed by attention mechanism
 */
typedef struct {
    uint32_t fault_id;              /**< Unique fault identifier */
    float severity;                 /**< Severity [0.0, 1.0] */
    uint32_t occurrence_count;      /**< Number of occurrences */
    uint32_t users_affected;        /**< Number of users impacted */
    uint64_t first_occurrence_ms;   /**< Timestamp of first occurrence */
    uint64_t last_occurrence_ms;    /**< Timestamp of last occurrence */
    bool is_active;                 /**< Whether fault is currently active */
    char description[128];          /**< Fault description */
} active_fault_t;

/**
 * @brief Attention mechanism configuration
 *
 * WHAT: Configuration parameters for attention computation
 * WHY:  Allow customization of prioritization strategy
 * HOW:  Weight factors must sum to 1.0, learning rate controls adaptation
 */
typedef struct {
    // Priority factor weights (must sum to 1.0)
    float severity_weight;      /**< Weight for error severity [0.0, 1.0] */
    float recency_weight;       /**< Weight for recency (1/time_since) [0.0, 1.0] */
    float frequency_weight;     /**< Weight for occurrence frequency [0.0, 1.0] */
    float impact_weight;        /**< Weight for user impact [0.0, 1.0] */

    // Adaptive learning
    bool enable_adaptive_weights;   /**< Enable weight adaptation */
    float learning_rate;            /**< Learning rate for weight updates [0.0, 1.0] */

    // Focus behavior
    uint32_t max_tracked_faults;    /**< Maximum faults to track */
    float min_attention_threshold;  /**< Minimum attention for processing [0.0, 1.0] */
} fault_attention_config_t;

/**
 * @brief Attention mechanism statistics
 */
typedef struct {
    uint64_t total_computations;    /**< Total weight computations */
    uint64_t total_updates;         /**< Total weight updates */
    uint32_t current_fault_count;   /**< Current number of faults */
    uint32_t focused_fault_id;      /**< Currently focused fault ID */
    float max_attention_weight;     /**< Maximum attention weight */
    float avg_computation_time_us;  /**< Average computation time */
} fault_attention_stats_t;

/**
 * @brief Attention mechanism instance (opaque)
 */
typedef struct fault_attention fault_attention_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create attention mechanism with default configuration
 *
 * WHAT: Allocates and initializes attention mechanism
 * WHY:  Entry point for attention-based prioritization
 * HOW:  Allocates structure, sets default weights, initializes statistics
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~1KB for structure + weight arrays
 *
 * @return Attention mechanism handle, NULL on failure
 *
 * @note Default weights: severity=0.4, recency=0.3, frequency=0.2, impact=0.1
 * @note Adaptive learning disabled by default
 */
fault_attention_t* fault_attention_create(void);

/**
 * @brief Create attention mechanism with custom configuration
 *
 * WHAT: Creates attention mechanism with specified weights
 * WHY:  Allow domain-specific prioritization strategies
 * HOW:  Validates config, allocates structure, applies configuration
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~1KB for structure + weight arrays
 *
 * @param config Configuration parameters (NULL for defaults)
 * @return Attention mechanism handle, NULL on invalid config
 *
 * @note Validates that weights sum to 1.0 (±0.01)
 * @note Returns NULL if weights are negative or sum incorrectly
 */
fault_attention_t* fault_attention_create_custom(
    const fault_attention_config_t* config
);

/**
 * @brief Destroy attention mechanism and free resources
 *
 * WHAT: Releases memory and cleans up
 * WHY:  Prevent memory leaks
 * HOW:  Frees weight arrays and structure
 *
 * COMPLEXITY: O(1)
 * MEMORY: Frees ~1KB
 *
 * @param attention Attention mechanism handle (NULL safe)
 */
void fault_attention_destroy(fault_attention_t* attention);

//=============================================================================
// Attention Computation Functions
//=============================================================================

/**
 * @brief Compute attention weights for active faults
 *
 * WHAT: Calculates normalized attention weight for each fault
 * WHY:  Core prioritization mechanism
 * HOW:  Apply formula: sum weighted factors, normalize to [0,1]
 *
 * ALGORITHM:
 * 1. For each fault:
 *    a. Compute time_since = current_time - last_occurrence
 *    b. Normalize occurrence_count and users_affected
 *    c. Apply formula: weight = Σ(factor_weight * factor_value)
 * 2. Normalize all weights to [0, 1] range
 * 3. Update focused_fault_idx to max weight index
 *
 * COMPLEXITY: O(n) where n = fault count
 * MEMORY: O(1) - reuses internal weight array
 * PERFORMANCE: <10μs for 10 faults
 *
 * @param attention Attention mechanism handle (non-NULL)
 * @param faults Array of active faults (non-NULL)
 * @param fault_count Number of faults (must be ≤ MAX_FAULTS)
 * @param current_time_ms Current timestamp in milliseconds
 * @return true on success, false on invalid parameters
 *
 * @note Weights are stored internally, retrieve with get_weight()
 * @note Focus is automatically updated to highest weight fault
 * @note Thread-safe if each thread uses separate attention instance
 */
bool fault_attention_compute_weights(
    fault_attention_t* attention,
    const active_fault_t* faults,
    uint32_t fault_count,
    uint64_t current_time_ms
);

/**
 * @brief Get computed attention weight for specific fault
 *
 * WHAT: Retrieves attention weight for fault at index
 * WHY:  Access individual prioritization scores
 * HOW:  Looks up weight in internal array
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1)
 *
 * @param attention Attention mechanism handle (non-NULL)
 * @param fault_index Fault index [0, fault_count)
 * @param weight Output parameter for weight value (non-NULL)
 * @return true on success, false if index out of bounds
 *
 * @note Must call compute_weights() before get_weight()
 * @note Weight range: [0.0, 1.0]
 */
bool fault_attention_get_weight(
    const fault_attention_t* attention,
    uint32_t fault_index,
    float* weight
);

/**
 * @brief Get all computed attention weights
 *
 * WHAT: Retrieves all attention weights in one call
 * WHY:  Efficient batch access for resource allocation
 * HOW:  Copies internal weight array to output
 *
 * COMPLEXITY: O(n) where n = fault_count
 * MEMORY: O(1)
 *
 * @param attention Attention mechanism handle (non-NULL)
 * @param weights Output array (non-NULL, size ≥ fault_count)
 * @param max_count Maximum weights to copy
 * @return Number of weights copied, 0 on error
 */
uint32_t fault_attention_get_all_weights(
    const fault_attention_t* attention,
    float* weights,
    uint32_t max_count
);

//=============================================================================
// Focus Management Functions
//=============================================================================

/**
 * @brief Get index of currently focused fault
 *
 * WHAT: Returns index of highest priority fault
 * WHY:  Direct access to critical fault for immediate recovery
 * HOW:  Returns index of maximum attention weight
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1)
 *
 * @param attention Attention mechanism handle (non-NULL)
 * @param focused_index Output parameter for fault index (non-NULL)
 * @return true on success, false if no faults present
 *
 * @note Updated automatically by compute_weights()
 * @note Use with fault array: focused_fault = faults[focused_index]
 */
bool fault_attention_get_focused_index(
    const fault_attention_t* attention,
    uint32_t* focused_index
);

/**
 * @brief Get focused fault directly
 *
 * WHAT: Returns copy of highest priority fault
 * WHY:  Convenience function for immediate access
 * HOW:  Combines get_focused_index() with fault lookup
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1)
 *
 * @param attention Attention mechanism handle (non-NULL)
 * @param faults Fault array used in last compute_weights() (non-NULL)
 * @param fault_count Number of faults in array
 * @param focused_fault Output parameter for fault copy (non-NULL)
 * @return true on success, false if no focus or invalid params
 */
bool fault_attention_get_focused_fault(
    const fault_attention_t* attention,
    const active_fault_t* faults,
    uint32_t fault_count,
    active_fault_t* focused_fault
);

//=============================================================================
// Adaptive Learning Functions
//=============================================================================

/**
 * @brief Update attention weights based on recovery outcome
 *
 * WHAT: Adjusts priority factor weights using gradient descent
 * WHY:  Learn which factors predict successful recovery
 * HOW:  Increase weights for dominant factors on success, decrease on failure
 *
 * ALGORITHM:
 * 1. Identify dominant factor for focused fault
 * 2. If recovery succeeded:
 *    - Increase dominant factor weight by learning_rate
 *    - Decrease others proportionally
 * 3. If recovery failed:
 *    - Decrease dominant factor weight by learning_rate
 *    - Increase others proportionally
 * 4. Normalize weights to sum to 1.0
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1)
 * PERFORMANCE: <1μs
 *
 * @param attention Attention mechanism handle (non-NULL)
 * @param fault_index Index of fault that was recovered
 * @param recovery_success true if recovery succeeded, false otherwise
 * @return true on success, false if adaptive learning disabled
 *
 * @note Only updates if enable_adaptive_weights is true
 * @note Learning rate controls adaptation speed (default 0.05)
 * @note Weights always maintained to sum to 1.0
 */
bool fault_attention_update_weights(
    fault_attention_t* attention,
    uint32_t fault_index,
    bool recovery_success
);

/**
 * @brief Reset weights to default configuration
 *
 * WHAT: Restores original weight configuration
 * WHY:  Undo maladaptive learning
 * HOW:  Reapplies initial configuration
 *
 * @param attention Attention mechanism handle (non-NULL)
 * @return true on success, false on NULL parameter
 */
bool fault_attention_reset_weights(fault_attention_t* attention);

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get current configuration
 *
 * WHAT: Retrieves current attention configuration
 * WHY:  Inspect current weights and settings
 * HOW:  Copies configuration to output parameter
 *
 * @param attention Attention mechanism handle (non-NULL)
 * @param config Output parameter for configuration (non-NULL)
 * @return true on success, false on NULL parameters
 */
bool fault_attention_get_config(
    const fault_attention_t* attention,
    fault_attention_config_t* config
);

/**
 * @brief Update configuration
 *
 * WHAT: Modifies attention configuration
 * WHY:  Dynamic adjustment of prioritization strategy
 * HOW:  Validates and applies new configuration
 *
 * @param attention Attention mechanism handle (non-NULL)
 * @param config New configuration (non-NULL, validated)
 * @return true on success, false if config invalid
 *
 * @note Validates weights sum to 1.0 (±0.01)
 */
bool fault_attention_set_config(
    fault_attention_t* attention,
    const fault_attention_config_t* config
);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get attention mechanism statistics
 *
 * WHAT: Retrieves performance and usage statistics
 * WHY:  Monitor effectiveness and performance
 * HOW:  Copies internal statistics to output
 *
 * @param attention Attention mechanism handle (non-NULL)
 * @param stats Output parameter for statistics (non-NULL)
 * @return true on success, false on NULL parameters
 */
bool fault_attention_get_stats(
    const fault_attention_t* attention,
    fault_attention_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * WHAT: Clears all statistics to zero
 * WHY:  Fresh measurement period
 * HOW:  Zeroes statistics structure
 *
 * @param attention Attention mechanism handle (non-NULL)
 * @return true on success, false on NULL parameter
 */
bool fault_attention_reset_stats(fault_attention_t* attention);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Returns default attention configuration
 * WHY:  Convenience for custom configuration creation
 * HOW:  Returns structure with default values
 *
 * @return Default configuration structure
 */
fault_attention_config_t fault_attention_default_config(void);

/**
 * @brief Validate configuration
 *
 * WHAT: Checks if configuration is valid
 * WHY:  Prevent invalid configurations
 * HOW:  Validates weights sum to 1.0, all values in range
 *
 * @param config Configuration to validate (non-NULL)
 * @return true if valid, false otherwise
 *
 * @note Checks: weights ≥ 0, weights sum to 1.0 (±0.01)
 * @note Checks: learning_rate in [0.0, 1.0]
 */
bool fault_attention_validate_config(
    const fault_attention_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_FAULT_ATTENTION_H
