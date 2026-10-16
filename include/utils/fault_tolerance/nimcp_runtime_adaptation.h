/**
 * @file nimcp_runtime_adaptation.h
 * @brief Runtime Parameter Adaptation System - No Compiler Required
 *
 * WHAT: Adaptive parameter tuning for runtime self-healing
 * WHY:  Production environments lack compilers - all fixes must be runtime
 * HOW:  Modify hyperparameters, configurations, weights, policies at runtime
 *
 * KEY PRINCIPLE: NO CODE GENERATION OR COMPILATION
 * - Adjust learning rates, batch sizes, thresholds
 * - Modify neural network weights (not structure)
 * - Toggle features, change modes, adjust limits
 * - Update decision policies and behavioral rules
 * - Enable/disable layers (runtime only, not code changes)
 *
 * EXAMPLE ADAPTATIONS:
 * 1. NaN Detected → Reduce learning rate 50%, enable gradient clipping
 * 2. Memory Pressure → Reduce batch size 50%, enable memory compaction
 * 3. Slow Convergence → Increase learning rate 20%, adjust momentum
 * 4. Overfitting → Increase dropout rate, reduce model capacity
 * 5. Performance Degradation → Reduce precision, enable caching
 *
 * @author NIMCP Team
 * @date 2025-11-19
 * @version 1.0.0
 */

#ifndef NIMCP_RUNTIME_ADAPTATION_H
#define NIMCP_RUNTIME_ADAPTATION_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Runtime Parameter Types
//=============================================================================

/**
 * @brief Runtime-adjustable parameters (NO COMPILATION REQUIRED)
 *
 * WHAT: Parameters that can be modified at runtime
 * WHY:  Enable self-healing without code changes
 * HOW:  Direct memory access to brain configuration
 */
typedef enum {
    // Learning Parameters
    RUNTIME_PARAM_LEARNING_RATE,       /**< Learning rate (0.0001 - 1.0) */
    RUNTIME_PARAM_BATCH_SIZE,          /**< Batch size (1 - 1000) */
    RUNTIME_PARAM_MOMENTUM,            /**< SGD momentum (0.0 - 0.99) */
    RUNTIME_PARAM_WEIGHT_DECAY,        /**< L2 regularization (0.0 - 0.1) */
    RUNTIME_PARAM_EPSILON,             /**< Adam epsilon (1e-8 - 1e-4) */

    // Regularization Parameters
    RUNTIME_PARAM_DROPOUT_RATE,        /**< Dropout probability (0.0 - 0.9) */
    RUNTIME_PARAM_L1_LAMBDA,           /**< L1 regularization (0.0 - 0.1) */
    RUNTIME_PARAM_L2_LAMBDA,           /**< L2 regularization (0.0 - 0.1) */
    RUNTIME_PARAM_NOISE_STDDEV,        /**< Input noise std dev (0.0 - 1.0) */

    // Gradient Control
    RUNTIME_PARAM_GRADIENT_CLIP_VALUE, /**< Gradient clipping threshold (0.1 - 10.0) */
    RUNTIME_PARAM_GRADIENT_CLIP_NORM,  /**< Gradient norm clipping (0.1 - 10.0) */

    // Neuron Parameters
    RUNTIME_PARAM_TEMPERATURE,         /**< Neuron temperature (0.1 - 10.0) */
    RUNTIME_PARAM_ACTIVATION_THRESHOLD,/**< Firing threshold (-10.0 - 10.0) */
    RUNTIME_PARAM_REFRACTORY_PERIOD,   /**< Refractory period ms (0.1 - 10.0) */
    RUNTIME_PARAM_LEAK_FACTOR,         /**< Membrane leak (0.0 - 1.0) */

    // Plasticity Parameters
    RUNTIME_PARAM_PLASTICITY_RATE,     /**< STDP learning rate (0.0 - 1.0) */
    RUNTIME_PARAM_STDP_WINDOW_MS,      /**< STDP time window (1.0 - 100.0) */
    RUNTIME_PARAM_HOMEOSTATIC_TARGET,  /**< Target firing rate (0.1 - 100.0) */
    RUNTIME_PARAM_HOMEOSTATIC_RATE,    /**< Homeostatic adaptation rate (0.0 - 1.0) */

    // Neuromodulation Parameters
    RUNTIME_PARAM_DOPAMINE_LEVEL,      /**< Dopamine level (0.0 - 2.0) */
    RUNTIME_PARAM_SEROTONIN_LEVEL,     /**< Serotonin level (0.0 - 2.0) */
    RUNTIME_PARAM_ACETYLCHOLINE_LEVEL, /**< Acetylcholine level (0.0 - 2.0) */
    RUNTIME_PARAM_NOREPINEPHRINE_LEVEL,/**< Norepinephrine level (0.0 - 2.0) */

    // Memory Parameters
    RUNTIME_PARAM_MEMORY_CAPACITY,     /**< Working memory capacity (1 - 100) */
    RUNTIME_PARAM_FORGETTING_RATE,     /**< Memory decay rate (0.0 - 1.0) */
    RUNTIME_PARAM_CONSOLIDATION_THRESHOLD, /**< Consolidation threshold (0.0 - 1.0) */

    // Performance Parameters
    RUNTIME_PARAM_MAX_THREADS,         /**< Thread pool size (1 - 32) */
    RUNTIME_PARAM_CACHE_SIZE_MB,       /**< Cache size in MB (1 - 10000) */
    RUNTIME_PARAM_PREFETCH_DISTANCE,   /**< Prefetch distance (0 - 1000) */

    // Numerical Stability
    RUNTIME_PARAM_MIN_WEIGHT_VALUE,    /**< Minimum weight (-1000.0 - 0.0) */
    RUNTIME_PARAM_MAX_WEIGHT_VALUE,    /**< Maximum weight (0.0 - 1000.0) */
    RUNTIME_PARAM_EPSILON_STABILIZER,  /**< Numerical epsilon (1e-12 - 1e-6) */

    RUNTIME_PARAM_COUNT                /**< Total number of parameters */
} runtime_parameter_t;

//=============================================================================
// Parameter Metadata
//=============================================================================

/**
 * @brief Parameter constraints and metadata
 */
typedef struct {
    runtime_parameter_t param_type;    /**< Parameter type */
    const char* name;                  /**< Human-readable name */
    const char* description;           /**< What it does */

    float min_value;                   /**< Minimum allowed value */
    float max_value;                   /**< Maximum allowed value */
    float default_value;               /**< Default value */
    float current_value;               /**< Current value */

    bool is_critical;                  /**< Critical parameter? */
    const char* unit;                  /**< Unit (%, ms, etc.) */
} parameter_info_t;

//=============================================================================
// Feature Toggle System (Runtime Enable/Disable)
//=============================================================================

/**
 * @brief Runtime-toggleable features
 */
typedef enum {
    RUNTIME_FEATURE_DROPOUT,           /**< Dropout regularization */
    RUNTIME_FEATURE_BATCH_NORM,        /**< Batch normalization */
    RUNTIME_FEATURE_GRADIENT_CLIPPING, /**< Gradient clipping */
    RUNTIME_FEATURE_WEIGHT_CLIPPING,   /**< Weight clipping */
    RUNTIME_FEATURE_LAYER_FREEZING,    /**< Layer parameter freezing */

    RUNTIME_FEATURE_PLASTICITY,        /**< Synaptic plasticity */
    RUNTIME_FEATURE_HOMEOSTASIS,       /**< Homeostatic plasticity */
    RUNTIME_FEATURE_NEUROMODULATION,   /**< Neuromodulator system */

    RUNTIME_FEATURE_MEMORY_COMPACTION, /**< Memory defragmentation */
    RUNTIME_FEATURE_PREFETCHING,       /**< Data prefetching */
    RUNTIME_FEATURE_CACHING,           /**< Result caching */
    RUNTIME_FEATURE_CHECKPOINTING,     /**< Auto-checkpointing */

    RUNTIME_FEATURE_DEBUG_LOGGING,     /**< Verbose logging */
    RUNTIME_FEATURE_NAN_DETECTION,     /**< NaN checking */
    RUNTIME_FEATURE_BOUNDS_CHECKING,   /**< Array bounds checking */

    RUNTIME_FEATURE_COUNT              /**< Total features */
} runtime_feature_t;

//=============================================================================
// Runtime Adaptation Context
//=============================================================================

/**
 * @brief Opaque handle for runtime adaptation context
 */
typedef struct runtime_adaptation_context_internal* runtime_adaptation_context_t;

/**
 * @brief Adaptation history entry
 */
typedef struct {
    runtime_parameter_t parameter;     /**< Parameter adjusted */
    float old_value;                   /**< Previous value */
    float new_value;                   /**< New value */
    uint64_t timestamp_us;             /**< When adjusted */
    char reason[256];                  /**< Why adjusted */
    bool was_successful;               /**< Did it help? */
} adaptation_history_t;

//=============================================================================
// Initialization & Lifecycle
//=============================================================================

/**
 * @brief Create runtime adaptation context
 *
 * WHAT: Initialize parameter adaptation system
 * WHY:  Enable runtime self-tuning
 * HOW:  Allocate context, initialize parameter registry
 *
 * @param brain Brain instance to adapt
 * @return Adaptation context, NULL on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 * MALLOC: Yes (context structure)
 */
runtime_adaptation_context_t runtime_adaptation_create(brain_t brain);

/**
 * @brief Destroy adaptation context
 *
 * @param ctx Adaptation context (can be NULL)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
void runtime_adaptation_destroy(runtime_adaptation_context_t ctx);

//=============================================================================
// Parameter Adjustment API
//=============================================================================

/**
 * @brief Adjust runtime parameter
 *
 * WHAT: Modify parameter value at runtime
 * WHY:  Self-healing without code changes
 * HOW:  Validate bounds, update brain configuration, log change
 *
 * VALIDATION:
 * - Check parameter exists
 * - Verify value within bounds
 * - Ensure not critical during operation
 * - Log adjustment for debugging
 *
 * @param ctx Adaptation context
 * @param param Parameter to adjust
 * @param new_value New value
 * @param reason Why adjusting (for logging)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (requires mutex if multi-threaded)
 * SIDE-EFFECTS: Modifies brain configuration
 */
bool runtime_adaptation_set_parameter(
    runtime_adaptation_context_t ctx,
    runtime_parameter_t param,
    float new_value,
    const char* reason
);

/**
 * @brief Get current parameter value
 *
 * @param ctx Adaptation context
 * @param param Parameter to query
 * @return Current value, -1.0 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float runtime_adaptation_get_parameter(
    runtime_adaptation_context_t ctx,
    runtime_parameter_t param
);

/**
 * @brief Reset parameter to default value
 *
 * @param ctx Adaptation context
 * @param param Parameter to reset
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
bool runtime_adaptation_reset_parameter(
    runtime_adaptation_context_t ctx,
    runtime_parameter_t param
);

/**
 * @brief Reset all parameters to defaults
 *
 * @param ctx Adaptation context
 * @return Number of parameters reset
 *
 * COMPLEXITY: O(n) where n = parameter count
 * THREAD-SAFE: No
 */
uint32_t runtime_adaptation_reset_all(runtime_adaptation_context_t ctx);

//=============================================================================
// Feature Toggle API
//=============================================================================

/**
 * @brief Enable runtime feature
 *
 * @param ctx Adaptation context
 * @param feature Feature to enable
 * @param reason Why enabling
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
bool runtime_adaptation_enable_feature(
    runtime_adaptation_context_t ctx,
    runtime_feature_t feature,
    const char* reason
);

/**
 * @brief Disable runtime feature
 *
 * @param ctx Adaptation context
 * @param feature Feature to disable
 * @param reason Why disabling
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
bool runtime_adaptation_disable_feature(
    runtime_adaptation_context_t ctx,
    runtime_feature_t feature,
    const char* reason
);

/**
 * @brief Check if feature is enabled
 *
 * @param ctx Adaptation context
 * @param feature Feature to check
 * @return true if enabled, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool runtime_adaptation_is_feature_enabled(
    runtime_adaptation_context_t ctx,
    runtime_feature_t feature
);

//=============================================================================
// Batch Adjustment API
//=============================================================================

/**
 * @brief Parameter adjustment batch
 */
typedef struct {
    runtime_parameter_t param;         /**< Parameter to adjust */
    float value;                       /**< New value */
} parameter_change_t;

/**
 * @brief Apply multiple parameter changes atomically
 *
 * WHAT: Adjust multiple parameters together
 * WHY:  Some adjustments must be coordinated
 * HOW:  Validate all, apply all, or rollback all
 *
 * EXAMPLE:
 * - Reduce learning rate + enable gradient clipping
 * - Reduce batch size + increase cache size
 * - Enable dropout + reduce temperature
 *
 * @param ctx Adaptation context
 * @param changes Array of parameter changes
 * @param num_changes Number of changes
 * @param reason Why making these changes
 * @return true if all applied successfully
 *
 * COMPLEXITY: O(n) where n = num_changes
 * THREAD-SAFE: No
 */
bool runtime_adaptation_apply_batch(
    runtime_adaptation_context_t ctx,
    parameter_change_t* changes,
    uint32_t num_changes,
    const char* reason
);

//=============================================================================
// Automated Adaptation Policies
//=============================================================================

/**
 * @brief Apply adaptation policy for NaN detection
 *
 * WHAT: Standard parameter adjustments for NaN errors
 * WHY:  Common failure pattern needs consistent response
 * HOW:
 *   - Reduce learning rate by 50%
 *   - Enable gradient clipping
 *   - Increase epsilon stabilizer
 *   - Enable NaN detection
 *
 * @param ctx Adaptation context
 * @return true on success
 */
bool runtime_adaptation_policy_nan_detected(runtime_adaptation_context_t ctx);

/**
 * @brief Apply adaptation policy for memory pressure
 *
 * WHAT: Reduce memory usage
 * HOW:
 *   - Reduce batch size by 50%
 *   - Enable memory compaction
 *   - Reduce cache size
 *   - Disable prefetching
 *
 * @param ctx Adaptation context
 * @return true on success
 */
bool runtime_adaptation_policy_memory_pressure(runtime_adaptation_context_t ctx);

/**
 * @brief Apply adaptation policy for gradient explosion
 *
 * HOW:
 *   - Enable gradient clipping
 *   - Reduce learning rate by 75%
 *   - Enable weight clipping
 *   - Reduce momentum
 *
 * @param ctx Adaptation context
 * @return true on success
 */
bool runtime_adaptation_policy_gradient_explosion(runtime_adaptation_context_t ctx);

/**
 * @brief Apply adaptation policy for slow convergence
 *
 * HOW:
 *   - Increase learning rate by 20%
 *   - Increase momentum
 *   - Increase plasticity rate
 *   - Enable prefetching
 *
 * @param ctx Adaptation context
 * @return true on success
 */
bool runtime_adaptation_policy_slow_convergence(runtime_adaptation_context_t ctx);

/**
 * @brief Apply adaptation policy for overfitting
 *
 * HOW:
 *   - Increase dropout rate
 *   - Increase weight decay
 *   - Enable L2 regularization
 *   - Reduce model complexity
 *
 * @param ctx Adaptation context
 * @return true on success
 */
bool runtime_adaptation_policy_overfitting(runtime_adaptation_context_t ctx);

//=============================================================================
// History & Analytics
//=============================================================================

/**
 * @brief Get parameter information
 *
 * @param param Parameter type
 * @param info Output parameter info structure
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool runtime_adaptation_get_param_info(
    runtime_parameter_t param,
    parameter_info_t* info
);

/**
 * @brief Get adaptation history
 *
 * @param ctx Adaptation context
 * @param history Output array for history
 * @param max_entries Maximum entries to return
 * @return Number of entries returned
 *
 * COMPLEXITY: O(n)
 * THREAD-SAFE: Yes
 */
uint32_t runtime_adaptation_get_history(
    runtime_adaptation_context_t ctx,
    adaptation_history_t* history,
    uint32_t max_entries
);

/**
 * @brief Get parameter name
 *
 * @param param Parameter type
 * @return Parameter name string
 */
const char* runtime_adaptation_param_name(runtime_parameter_t param);

/**
 * @brief Get feature name
 *
 * @param feature Feature type
 * @return Feature name string
 */
const char* runtime_adaptation_feature_name(runtime_feature_t feature);

//=============================================================================
// Persistence
//=============================================================================

/**
 * @brief Save current parameter configuration
 *
 * @param ctx Adaptation context
 * @param filepath Output file path
 * @return true on success
 *
 * COMPLEXITY: O(n)
 * THREAD-SAFE: Yes
 */
bool runtime_adaptation_save_config(
    runtime_adaptation_context_t ctx,
    const char* filepath
);

/**
 * @brief Load parameter configuration
 *
 * @param ctx Adaptation context
 * @param filepath Input file path
 * @return true on success
 *
 * COMPLEXITY: O(n)
 * THREAD-SAFE: No
 */
bool runtime_adaptation_load_config(
    runtime_adaptation_context_t ctx,
    const char* filepath
);

//=============================================================================
// Reporting & Debugging
//=============================================================================

/**
 * @brief Generate adaptation report
 *
 * @param ctx Adaptation context
 * @param output Output stream
 *
 * COMPLEXITY: O(n)
 * THREAD-SAFE: Yes
 */
void runtime_adaptation_report(
    runtime_adaptation_context_t ctx,
    FILE* output
);

/**
 * @brief Export configuration to JSON
 *
 * @param ctx Adaptation context
 * @param json_buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written, -1 on error
 *
 * COMPLEXITY: O(n)
 * THREAD-SAFE: Yes
 */
int32_t runtime_adaptation_export_json(
    runtime_adaptation_context_t ctx,
    char* json_buffer,
    size_t buffer_size
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_RUNTIME_ADAPTATION_H
