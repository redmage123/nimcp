/**
 * @file nimcp_cingulate_quantum_bridge.h
 * @brief Quantum bridge for Cingulate Cortex - quantum conflict resolution
 *
 * WHAT: Quantum-accelerated conflict resolution and error signal propagation
 * WHY:  Leverage quantum computing for faster conflict arbitration
 * HOW:  Uses quantum superposition to evaluate all conflict resolutions simultaneously
 *
 * ARCHITECTURE:
 * - Wraps quantum reasoning context for cingulate-specific operations
 * - Provides quantum conflict resolution (Grover-like search)
 * - Implements quantum error signal propagation
 * - Supports superposition of response options for parallel evaluation
 *
 * QUANTUM CONCEPTS:
 * - Grover Search: O(sqrt(N)) search for optimal conflict resolution
 * - Amplitude Encoding: Conflict levels encoded as quantum amplitudes
 * - Quantum Interference: Contradictory paths cancel out
 * - Phase Kickback: Propagate error signals through quantum circuit
 *
 * BIOLOGICAL ANALOGY:
 * The ACC may implement a form of parallel constraint satisfaction:
 * - Multiple response options evaluated simultaneously
 * - Conflicting options interfere destructively
 * - Winner emerges through dynamics similar to quantum measurement
 *
 * @version Phase B4: Cingulate Cortex Integration
 * @date 2025-12-30
 */

#ifndef NIMCP_CINGULATE_QUANTUM_BRIDGE_H
#define NIMCP_CINGULATE_QUANTUM_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/cingulate/nimcp_cingulate_adapter.h"

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

/* Forward declare opaque quantum bridge type */
typedef struct cingulate_quantum_bridge cingulate_quantum_bridge_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Default configuration values
 */
#define CINGULATE_QUANTUM_DEFAULT_MAX_QUBITS         8
#define CINGULATE_QUANTUM_DEFAULT_MAX_ITERATIONS     32
#define CINGULATE_QUANTUM_DEFAULT_MIN_CONFIDENCE     0.7f
#define CINGULATE_QUANTUM_DEFAULT_INTERFERENCE_GAIN  0.8f

/**
 * @brief Quantum bridge configuration
 */
typedef struct {
    /* Quantum parameters */
    uint32_t max_qubits;                /**< Maximum qubits (limits options) */
    uint32_t max_iterations;            /**< Maximum Grover iterations */
    float min_confidence;               /**< Minimum confidence threshold */
    float interference_gain;            /**< Interference strength [0, 1] */

    /* Error propagation */
    bool enable_error_propagation;      /**< Enable quantum error propagation */
    float error_phase_shift;            /**< Phase shift for error encoding */

    /* Integration */
    bool enable_classical_fallback;     /**< Fall back to classical if quantum fails */
    uint32_t seed;                      /**< Random seed for simulation */
} cingulate_quantum_config_t;

/*=============================================================================
 * CONFLICT RESOLUTION STRUCTURES
 *===========================================================================*/

/**
 * @brief Quantum state for conflict resolution
 */
typedef struct {
    float* amplitudes;                  /**< Quantum amplitude vector */
    uint32_t num_states;                /**< Number of basis states (2^n) */
    uint32_t num_qubits;                /**< Number of qubits used */
    float total_probability;            /**< Sum of |amplitude|^2 (should be 1) */
} cingulate_quantum_state_t;

/**
 * @brief Option encoded for quantum processing
 */
typedef struct {
    uint32_t option_id;                 /**< Original option ID */
    float initial_amplitude;            /**< Initial quantum amplitude */
    float activation;                   /**< Classical activation level */
    float constraint_satisfaction;      /**< How well option satisfies constraints */
    bool is_marked;                     /**< Marked for Grover amplification */
} cingulate_quantum_option_t;

/**
 * @brief Result of quantum conflict resolution
 */
typedef struct {
    uint32_t selected_option;           /**< Winning option ID */
    float selection_confidence;         /**< Confidence in selection [0, 1] */
    float conflict_resolution;          /**< Degree of conflict resolved [0, 1] */
    uint32_t iterations_used;           /**< Grover iterations performed */
    float speedup_achieved;             /**< Estimated quantum speedup */
    bool used_fallback;                 /**< Classical fallback was used */
} cingulate_quantum_result_t;

/**
 * @brief Error signal for quantum propagation
 */
typedef struct {
    float error_magnitude;              /**< Error magnitude [0, 1] */
    float phase;                        /**< Quantum phase encoding */
    uint32_t source_option;             /**< Option that caused error */
    uint32_t target_options[16];        /**< Options affected by error */
    uint32_t num_targets;               /**< Number of affected options */
} cingulate_quantum_error_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default quantum bridge configuration
 *
 * @return Default configuration with sensible values
 */
cingulate_quantum_config_t cingulate_quantum_default_config(void);

/**
 * @brief Create quantum bridge for cingulate cortex
 *
 * WHAT: Create quantum processing bridge for conflict resolution
 * WHY:  Enable quantum-accelerated decision making
 * HOW:  Initialize quantum simulator with cingulate-specific operators
 *
 * @param cingulate Cingulate adapter to connect (optional)
 * @param config Configuration (NULL for defaults)
 * @return New quantum bridge, or NULL on failure
 */
cingulate_quantum_bridge_t* cingulate_quantum_bridge_create(
    cingulate_adapter_t* cingulate,
    const cingulate_quantum_config_t* config);

/**
 * @brief Destroy quantum bridge
 *
 * @param bridge Bridge to destroy
 */
void cingulate_quantum_bridge_destroy(cingulate_quantum_bridge_t* bridge);

/**
 * @brief Reset quantum bridge state
 *
 * @param bridge Bridge to reset
 * @return true on success
 */
bool cingulate_quantum_bridge_reset(cingulate_quantum_bridge_t* bridge);

/*=============================================================================
 * QUANTUM CONFLICT RESOLUTION
 *===========================================================================*/

/**
 * @brief Initialize quantum state from response options
 *
 * WHAT: Encode response options as quantum superposition
 * WHY:  Prepare state for quantum conflict resolution
 * HOW:  Map activations to amplitudes, normalize
 *
 * BIOLOGICAL: Maps to parallel activation of response options
 * in motor preparation areas
 *
 * @param bridge Quantum bridge instance
 * @param options Response options to encode
 * @param num_options Number of options
 * @return true on success
 */
bool cingulate_quantum_encode_options(
    cingulate_quantum_bridge_t* bridge,
    const cingulate_response_option_t* options,
    uint32_t num_options);

/**
 * @brief Apply conflict constraints to quantum state
 *
 * WHAT: Apply constraints that penalize conflicting options
 * WHY:  Drive state toward non-conflicting resolution
 * HOW:  Phase-encode conflict, apply interference
 *
 * @param bridge Quantum bridge instance
 * @param conflict Conflict to resolve
 * @return true on success
 */
bool cingulate_quantum_apply_constraints(
    cingulate_quantum_bridge_t* bridge,
    const cingulate_conflict_t* conflict);

/**
 * @brief Run quantum conflict resolution (Grover-like)
 *
 * WHAT: Find optimal conflict resolution using quantum search
 * WHY:  O(sqrt(N)) speedup over classical search
 * HOW:  Apply Grover iterations to amplify best resolution
 *
 * QUANTUM ALGORITHM:
 * 1. Initialize uniform superposition over options
 * 2. Apply oracle marking "good" resolutions
 * 3. Apply diffusion operator
 * 4. Repeat O(sqrt(N)) times
 * 5. Measure to get resolution
 *
 * @param bridge Quantum bridge instance
 * @param result Output: resolution result
 * @return true on success
 */
bool cingulate_quantum_resolve_conflict(
    cingulate_quantum_bridge_t* bridge,
    cingulate_quantum_result_t* result);

/**
 * @brief Get probability distribution over options
 *
 * WHAT: Return current probability distribution
 * WHY:  Inspect quantum state before measurement
 * HOW:  Compute |amplitude|^2 for each option
 *
 * @param bridge Quantum bridge instance
 * @param probabilities Output array (must be large enough)
 * @param num_options Number of options
 * @return true on success
 */
bool cingulate_quantum_get_probabilities(
    const cingulate_quantum_bridge_t* bridge,
    float* probabilities,
    uint32_t num_options);

/*=============================================================================
 * QUANTUM ERROR PROPAGATION
 *===========================================================================*/

/**
 * @brief Encode error signal for quantum propagation
 *
 * WHAT: Encode error event as quantum phase
 * WHY:  Propagate error signal to affected options
 * HOW:  Map error magnitude to phase rotation
 *
 * BIOLOGICAL: Maps to ERN signal propagation through
 * ACC-prefrontal connections
 *
 * @param bridge Quantum bridge instance
 * @param error Error event from cingulate
 * @param quantum_error Output: encoded quantum error
 * @return true on success
 */
bool cingulate_quantum_encode_error(
    cingulate_quantum_bridge_t* bridge,
    const cingulate_error_event_t* error,
    cingulate_quantum_error_t* quantum_error);

/**
 * @brief Propagate error through quantum state
 *
 * WHAT: Apply error-induced phase shifts to quantum state
 * WHY:  Reduce amplitude of error-causing options
 * HOW:  Apply controlled phase rotation
 *
 * @param bridge Quantum bridge instance
 * @param quantum_error Error to propagate
 * @return true on success
 */
bool cingulate_quantum_propagate_error(
    cingulate_quantum_bridge_t* bridge,
    const cingulate_quantum_error_t* quantum_error);

/**
 * @brief Compute error backpropagation gradient
 *
 * WHAT: Compute how error affects each option's amplitude
 * WHY:  Enable learning from errors
 * HOW:  Quantum gradient estimation
 *
 * @param bridge Quantum bridge instance
 * @param quantum_error Error signal
 * @param gradients Output: gradient for each option
 * @param num_options Number of options
 * @return true on success
 */
bool cingulate_quantum_error_gradient(
    cingulate_quantum_bridge_t* bridge,
    const cingulate_quantum_error_t* quantum_error,
    float* gradients,
    uint32_t num_options);

/*=============================================================================
 * SUPERPOSITION EVALUATION
 *===========================================================================*/

/**
 * @brief Create superposition of control signals
 *
 * WHAT: Create quantum superposition of possible control levels
 * WHY:  Evaluate all control adjustments in parallel
 * HOW:  Encode control levels as basis states
 *
 * @param bridge Quantum bridge instance
 * @param min_control Minimum control level
 * @param max_control Maximum control level
 * @param num_levels Number of discretization levels
 * @return true on success
 */
bool cingulate_quantum_superpose_control(
    cingulate_quantum_bridge_t* bridge,
    float min_control,
    float max_control,
    uint32_t num_levels);

/**
 * @brief Evaluate control levels in superposition
 *
 * WHAT: Apply evaluation oracle to superposition
 * WHY:  Mark good control levels for amplification
 * HOW:  Phase-encode evaluation function
 *
 * @param bridge Quantum bridge instance
 * @param conflict Current conflict state
 * @param error Current error state
 * @return true on success
 */
bool cingulate_quantum_evaluate_control(
    cingulate_quantum_bridge_t* bridge,
    const cingulate_conflict_t* conflict,
    const cingulate_error_event_t* error);

/**
 * @brief Measure optimal control level
 *
 * WHAT: Collapse superposition to select control level
 * WHY:  Get concrete control adjustment value
 * HOW:  Probabilistic measurement weighted by evaluation
 *
 * @param bridge Quantum bridge instance
 * @param optimal_control Output: optimal control level
 * @param confidence Output: confidence in selection
 * @return true on success
 */
bool cingulate_quantum_measure_control(
    cingulate_quantum_bridge_t* bridge,
    float* optimal_control,
    float* confidence);

/*=============================================================================
 * INTEGRATION WITH CINGULATE ADAPTER
 *===========================================================================*/

/**
 * @brief Update quantum bridge from cingulate state
 *
 * WHAT: Sync quantum state with cingulate adapter
 * WHY:  Keep quantum model consistent with classical
 * HOW:  Read response options and conflict state
 *
 * @param bridge Quantum bridge instance
 * @return true on success
 */
bool cingulate_quantum_bridge_update(cingulate_quantum_bridge_t* bridge);

/**
 * @brief Apply quantum resolution to cingulate
 *
 * WHAT: Apply quantum resolution result to adapter
 * WHY:  Commit quantum decision to classical system
 * HOW:  Generate control signal from quantum result
 *
 * @param bridge Quantum bridge instance
 * @param result Quantum resolution result
 * @return true on success
 */
bool cingulate_quantum_apply_resolution(
    cingulate_quantum_bridge_t* bridge,
    const cingulate_quantum_result_t* result);

/**
 * @brief Full quantum-assisted conflict resolution
 *
 * WHAT: End-to-end quantum conflict resolution pipeline
 * WHY:  Convenience function for common use case
 * HOW:  Encode -> Apply constraints -> Resolve -> Apply
 *
 * @param bridge Quantum bridge instance
 * @param result Output: resolution result
 * @return true on success
 */
bool cingulate_quantum_full_resolution(
    cingulate_quantum_bridge_t* bridge,
    cingulate_quantum_result_t* result);

/*=============================================================================
 * STATISTICS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Quantum bridge statistics
 */
typedef struct {
    uint64_t resolutions_attempted;     /**< Total resolution attempts */
    uint64_t resolutions_successful;    /**< Successful resolutions */
    uint64_t fallbacks_used;            /**< Classical fallbacks used */
    float avg_iterations;               /**< Average Grover iterations */
    float avg_confidence;               /**< Average selection confidence */
    float avg_speedup;                  /**< Average quantum speedup */
    uint64_t errors_propagated;         /**< Errors propagated */
    float avg_error_gradient;           /**< Average error gradient magnitude */
} cingulate_quantum_stats_t;

/**
 * @brief Get quantum bridge statistics
 *
 * @param bridge Quantum bridge instance
 * @param stats Output: statistics structure
 * @return true on success
 */
bool cingulate_quantum_get_stats(
    const cingulate_quantum_bridge_t* bridge,
    cingulate_quantum_stats_t* stats);

/**
 * @brief Get quantum bridge configuration
 *
 * @param bridge Quantum bridge instance
 * @param config Output: configuration structure
 * @return true on success
 */
bool cingulate_quantum_get_config(
    const cingulate_quantum_bridge_t* bridge,
    cingulate_quantum_config_t* config);

/**
 * @brief Get quantum state for inspection
 *
 * WHAT: Return current quantum state (for debugging)
 * WHY:  Allow inspection of quantum computation
 * HOW:  Copy internal state to user buffer
 *
 * @param bridge Quantum bridge instance
 * @param state Output: quantum state (caller allocates amplitudes)
 * @return true on success
 */
bool cingulate_quantum_get_state(
    const cingulate_quantum_bridge_t* bridge,
    cingulate_quantum_state_t* state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CINGULATE_QUANTUM_BRIDGE_H */
