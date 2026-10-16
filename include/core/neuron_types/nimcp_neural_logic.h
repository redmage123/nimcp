/**
 * @file nimcp_neural_logic.h
 * @brief GPU-Accelerated Neural Logic Gates (Spiking Implementation)
 *
 * WHAT: Logical operations implemented as spiking neural circuits
 * WHY:  Replace symbolic logic engine with fast, GPU-accelerated neural logic
 * HOW:  Specialized neurons (AND/OR/NOT/XOR/IMPLIES) using spike-based computation
 *
 * ARCHITECTURE:
 *
 *   ┌─────────────────────────────────────────────────┐
 *   │          Neural Logic Circuit                   │
 *   │                                                 │
 *   │  [Input A]   [Input B]                         │
 *   │      │           │                              │
 *   │      └─────┬─────┘                              │
 *   │            │                                    │
 *   │      [Logic Neuron]                            │
 *   │      (AND/OR/NOT/XOR)                          │
 *   │            │                                    │
 *   │       [Output]                                  │
 *   │                                                 │
 *   └─────────────────────────────────────────────────┘
 *
 * DESIGN PHILOSOPHY:
 * - Each logic gate = specialized spiking neuron
 * - Fast (0.1-1ms) vs. symbolic (100ms+)
 * - GPU-accelerated (100x speedup vs CPU)
 * - Fully differentiable (backpropagation through logic)
 * - Biologically inspired (coincidence detection, inhibition)
 *
 * BIOLOGICAL MOTIVATION:
 * - AND: Coincidence detector neurons (Jeffress, 1948)
 * - OR: Low-threshold integrator neurons
 * - NOT: Inhibitory interneurons (basket cells)
 * - XOR: Balanced excitation/inhibition circuits
 * - IMPLIES: Conditional firing patterns in PFC
 *
 * PERFORMANCE:
 * - GPU: ~0.1ms for 1000 logic operations
 * - CPU: ~10ms for 1000 logic operations
 * - Speedup: ~100x with CUDA
 *
 * @author NIMCP Development Team
 * @date 2025-11-08
 * @version 2.7.0 Phase 9.0
 */

#ifndef NIMCP_NEURAL_LOGIC_H
#define NIMCP_NEURAL_LOGIC_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"
#include "core/neuron_types/nimcp_neuron_types.h"
#include "utils/error/nimcp_error_codes.h"

/* Bio-async communication system */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

// Forward declaration for brain integration
#ifndef NIMCP_BRAIN_T_DEFINED
#define NIMCP_BRAIN_T_DEFINED
typedef struct brain_struct* brain_t;
#endif

// Conditional CUDA support
// Note: Only include cuda_runtime.h when compiling with nvcc (__CUDACC__).
// Including it from gcc/g++ inside extern "C" blocks causes C++ template
// linkage errors because CUDA headers contain C++ template specializations.
#ifdef __CUDACC__
#include <cuda_runtime.h>
#define NIMCP_DEVICE __device__
#define NIMCP_HOST __host__
#define NIMCP_GLOBAL __global__
#define NIMCP_SHARED __shared__
#else
#define NIMCP_DEVICE
#define NIMCP_HOST
#define NIMCP_GLOBAL
#define NIMCP_SHARED
#endif

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Logic Gate Types
//=============================================================================

/**
 * @brief Logic gate operation types
 *
 * Maps to specialized neuron types (650-699 range)
 */
typedef enum {
    LOGIC_GATE_AND      = 0,  /**< Conjunction (∧) - NEURON_LOGIC_AND */
    LOGIC_GATE_OR       = 1,  /**< Disjunction (∨) - NEURON_LOGIC_OR */
    LOGIC_GATE_NOT      = 2,  /**< Negation (¬) - NEURON_LOGIC_NOT */
    LOGIC_GATE_XOR      = 3,  /**< Exclusive OR (⊕) - NEURON_LOGIC_XOR */
    LOGIC_GATE_IMPLIES  = 4,  /**< Implication (→) - NEURON_LOGIC_IMPLIES */
    LOGIC_GATE_VARIABLE = 5,  /**< Input variable (pass-through) */
    LOGIC_GATE_COUNT
} logic_gate_type_t;

//=============================================================================
// Logic Neuron State (GPU-Optimized)
//=============================================================================

/**
 * @brief Logic neuron state - compact for GPU efficiency
 *
 * LAYOUT: 64 bytes (fits in cache line)
 * ALIGNMENT: 64-byte aligned for coalesced access
 *
 * DESIGN: Optimized for GPU memory bandwidth
 */
typedef struct __attribute__((aligned(64))) {
    // Core state (16 bytes)
    float membrane_potential;    /**< Current V_m (mV) */
    float threshold;             /**< Spike threshold */
    float output_state;          /**< Logical output [0,1] */
    logic_gate_type_t gate_type; /**< Logic operation type */

    // Input tracking (16 bytes)
    float input_a_activity;      /**< Input A recent activity */
    float input_b_activity;      /**< Input B recent activity */
    uint64_t last_spike_time;    /**< Last output spike (μs) */

    // Gate-specific params (16 bytes)
    float integration_window;    /**< Temporal integration (ms) */
    float excitatory_weight;     /**< Excitatory weight */
    float inhibitory_weight;     /**< Inhibitory weight */
    uint32_t refractory_period;  /**< Refractory period (μs) */

    // Statistics (16 bytes)
    uint32_t neuron_id;          /**< Global neuron ID */
    uint32_t total_spikes;       /**< Total spikes emitted */
    uint32_t true_outputs;       /**< Count of logical TRUE outputs */
    uint32_t false_outputs;      /**< Count of logical FALSE outputs */
} logic_neuron_state_t;

/**
 * @brief Variable binding state (for variable neurons)
 *
 * LAYOUT: 128 bytes
 * ALIGNMENT: 64-byte aligned
 */
typedef struct __attribute__((aligned(64))) {
    uint32_t variable_id;        /**< Variable identifier (hash) */
    uint32_t pattern_dim;        /**< Pattern dimensionality */
    float binding_strength;      /**< Binding confidence [0,1] */
    float decay_rate;            /**< Binding decay (per ms) */
    bool is_bound;               /**< Currently bound? */
    uint32_t _padding[11];       /**< Padding to 64 bytes */
    float* bound_pattern;        /**< Activation pattern (GPU pointer) */
} variable_binding_state_t;

//=============================================================================
// Neural Logic Network
//=============================================================================

/**
 * @brief Neural logic network (opaque handle)
 *
 * CONTAINS:
 * - Logic neuron states (GPU/CPU memory)
 * - Variable bindings (GPU/CPU memory)
 * - Connectivity graph
 * - Execution context
 */
typedef struct neural_logic_network_struct* neural_logic_network_t;

/**
 * @brief Neural logic network configuration
 */
typedef struct {
    uint32_t max_logic_neurons;  /**< Maximum logic gate neurons */
    uint32_t max_variables;      /**< Maximum variable bindings */
    uint32_t variable_pattern_dim; /**< Pattern dimensionality */

    // GPU configuration
    uint32_t threads_per_block;  /**< CUDA threads per block */
    bool use_gpu;                /**< Enable GPU acceleration */
    bool pin_host_memory;        /**< Pin CPU memory for fast transfer */

    // Temporal parameters
    float timestep_us;           /**< Simulation timestep (μs) */
    float integration_window_ms; /**< Default integration window (ms) */

    // Learning configuration
    bool enable_learning;        /**< Enable weight adaptation */
    float learning_rate;         /**< Synaptic learning rate */

    // Bio-async communication
    bool enable_bio_async;       /**< Enable bio-async messaging */

    // Quantum acceleration
    bool enable_quantum_neural_logic; /**< Enable quantum SAT solving */
} neural_logic_config_t;

//=============================================================================
// Network Lifecycle
//=============================================================================

/**
 * @brief Create neural logic network
 *
 * WHAT: Initialize neural logic network (GPU or CPU)
 * WHY:  Provide hardware-accelerated logical reasoning
 * HOW:  Allocate memory, initialize neurons, detect GPU
 *
 * @param config Network configuration
 * @return Network handle, or NULL on failure
 *
 * BEHAVIOR:
 * - Detects GPU availability
 * - Allocates GPU memory if available
 * - Falls back to CPU if GPU unavailable
 * - Initializes logic neuron parameters
 *
 * COMPLEXITY: O(n) where n = max_logic_neurons
 * THREAD SAFETY: Not thread-safe (call from single thread)
 */
NIMCP_EXPORT neural_logic_network_t neural_logic_create(
    const neural_logic_config_t* config
);

/**
 * @brief Destroy neural logic network
 *
 * @param network Network handle (NULL-safe)
 *
 * CLEANUP:
 * - Synchronizes GPU
 * - Frees GPU memory
 * - Frees CPU memory
 * - Destroys variable bindings
 */
NIMCP_EXPORT void neural_logic_destroy(neural_logic_network_t network);

//=============================================================================
// Logic Neuron Creation
//=============================================================================

/**
 * @brief Create logic gate neuron
 *
 * WHAT: Add logic gate neuron to network
 * WHY:  Build logical circuits from individual gates
 * HOW:  Allocate neuron, set gate-specific parameters
 *
 * @param network Network handle
 * @param gate_type Logic gate type (AND/OR/NOT/XOR/IMPLIES)
 * @param threshold Spike threshold (default: auto-configured)
 * @return Neuron ID, or UINT32_MAX on failure
 *
 * EXAMPLE:
 * ```c
 * // Create AND gate
 * uint32_t and_gate = neural_logic_create_gate(net, LOGIC_GATE_AND, 1.8f);
 *
 * // Create NOT gate
 * uint32_t not_gate = neural_logic_create_gate(net, LOGIC_GATE_NOT, 0.5f);
 * ```
 *
 * COMPLEXITY: O(1)
 */
NIMCP_EXPORT uint32_t neural_logic_create_gate(
    neural_logic_network_t network,
    logic_gate_type_t gate_type,
    float threshold
);

/**
 * @brief Create variable binding neuron
 *
 * WHAT: Create neuron that binds variables to patterns
 * WHY:  Enable compositional reasoning (X → pattern)
 * HOW:  Allocate binding state, initialize pattern storage
 *
 * @param network Network handle
 * @param variable_name Variable name (e.g., "X", "Y")
 * @return Variable ID, or UINT32_MAX on failure
 *
 * EXAMPLE:
 * ```c
 * uint32_t var_x = neural_logic_create_variable(net, "X");
 * uint32_t var_y = neural_logic_create_variable(net, "Y");
 * ```
 */
NIMCP_EXPORT uint32_t neural_logic_create_variable(
    neural_logic_network_t network,
    const char* variable_name
);

//=============================================================================
// Circuit Connectivity
//=============================================================================

/**
 * @brief Connect neurons (create synapse)
 *
 * WHAT: Create synaptic connection between neurons
 * WHY:  Build logic circuits from gates
 * HOW:  Add weighted synapse with specified polarity
 *
 * @param network Network handle
 * @param source_id Source neuron ID
 * @param target_id Target neuron ID
 * @param weight Synaptic weight (positive=excitatory, negative=inhibitory)
 * @return true on success
 *
 * EXAMPLE:
 * ```c
 * // A AND B circuit:
 * //   Input A ──→ AND Gate
 * //   Input B ──→ AND Gate
 * neural_logic_connect(net, input_a, and_gate, 1.0f);
 * neural_logic_connect(net, input_b, and_gate, 1.0f);
 * ```
 */
NIMCP_EXPORT bool neural_logic_connect(
    neural_logic_network_t network,
    uint32_t source_id,
    uint32_t target_id,
    float weight
);

//=============================================================================
// Simulation (GPU Kernel Launch)
//=============================================================================

/**
 * @brief Update all logic neurons for one timestep (GPU kernel)
 *
 * WHAT: Simulate one timestep of logic network
 * WHY:  Compute logic outputs via spiking dynamics
 * HOW:  Launch GPU kernel (1 thread per neuron) or CPU loop
 *
 * @param network Network handle
 * @param timestamp Current simulation time (μs)
 * @param delta_t Timestep duration (μs, typically 100)
 * @return Number of neurons that spiked
 *
 * BEHAVIOR:
 * - Each neuron reads input spikes
 * - Computes membrane potential
 * - Fires spike if threshold crossed
 * - Updates logical output state
 *
 * PERFORMANCE:
 * - GPU: ~0.1ms for 1000 neurons
 * - CPU: ~10ms for 1000 neurons
 *
 * COMPLEXITY: O(n) where n = num_neurons
 */
NIMCP_EXPORT uint32_t neural_logic_update(
    neural_logic_network_t network,
    uint64_t timestamp,
    uint64_t delta_t
);

/**
 * @brief Synchronize GPU execution (blocking)
 *
 * @param network Network handle
 * @return true on success
 *
 * BEHAVIOR: Waits for GPU kernels to complete
 */
NIMCP_EXPORT bool neural_logic_synchronize(neural_logic_network_t network);

//=============================================================================
// Logical Evaluation (High-Level API)
//=============================================================================

/**
 * @brief Evaluate logic expression
 *
 * WHAT: Evaluate logical expression via neural simulation
 * WHY:  Provide high-level interface to neural logic
 * HOW:  Set inputs, simulate network, read outputs
 *
 * @param network Network handle
 * @param gate_id Logic gate neuron ID
 * @param inputs Input values (boolean encoded as float 0/1)
 * @param num_inputs Number of inputs (typically 1-2)
 * @param output Logical output [0,1]
 * @return true on success
 *
 * EXAMPLE:
 * ```c
 * float inputs[2] = {1.0f, 1.0f};  // A=true, B=true
 * float output;
 * neural_logic_evaluate(net, and_gate, inputs, 2, &output);
 * // output = 1.0 (true)
 * ```
 *
 * COMPLEXITY: O(t * n) where t = simulation time, n = neurons
 */
NIMCP_EXPORT bool neural_logic_evaluate(
    neural_logic_network_t network,
    uint32_t gate_id,
    const float* inputs,
    uint32_t num_inputs,
    float* output
);

//=============================================================================
// Variable Binding
//=============================================================================

/**
 * @brief Bind variable to neural pattern
 *
 * WHAT: Associate symbolic variable with activation pattern
 * WHY:  Enable compositional reasoning (X = [0.8, 0.2, ...])
 * HOW:  Store pattern in variable neuron
 *
 * @param network Network handle
 * @param variable_id Variable neuron ID
 * @param pattern Activation pattern (size = variable_pattern_dim)
 * @param binding_strength Confidence [0,1]
 * @return true on success
 *
 * EXAMPLE:
 * ```c
 * float pattern[64] = {0.8, 0.2, 0.5, ...};
 * neural_logic_bind_variable(net, var_x, pattern, 1.0f);
 * ```
 */
NIMCP_EXPORT bool neural_logic_bind_variable(
    neural_logic_network_t network,
    uint32_t variable_id,
    const float* pattern,
    float binding_strength
);

/**
 * @brief Query variable binding
 *
 * @param network Network handle
 * @param variable_id Variable neuron ID
 * @param pattern Output pattern (must be pre-allocated)
 * @param pattern_dim Pattern dimensionality
 * @return true if variable is bound
 */
NIMCP_EXPORT bool neural_logic_query_variable(
    neural_logic_network_t network,
    uint32_t variable_id,
    float* pattern,
    uint32_t pattern_dim
);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get logic neuron state (GPU → CPU transfer)
 *
 * @param network Network handle
 * @param neuron_id Logic neuron ID
 * @param state Output state structure
 * @return true on success
 */
NIMCP_EXPORT bool neural_logic_get_state(
    neural_logic_network_t network,
    uint32_t neuron_id,
    logic_neuron_state_t* state
);

/**
 * @brief Get network statistics
 *
 * @param network Network handle
 * @param total_gates Output: total logic gates
 * @param total_variables Output: total variables
 * @param total_spikes Output: total spikes emitted
 * @param avg_eval_time Output: average evaluation time (μs)
 * @param gpu_memory_used Output: GPU memory (bytes)
 * @return true on success
 */
NIMCP_EXPORT bool neural_logic_get_stats(
    neural_logic_network_t network,
    uint32_t* total_gates,
    uint32_t* total_variables,
    uint64_t* total_spikes,
    float* avg_eval_time,
    uint64_t* gpu_memory_used
);

//=============================================================================
// GPU Kernel Declarations (Compiled Only with CUDA)
//=============================================================================

#ifdef NIMCP_ENABLE_CUDA

/**
 * @brief GPU kernel: Update logic neurons
 *
 * LAUNCH CONFIG: <<<blocks, threads_per_block>>>
 * - blocks: ceil(num_neurons / threads_per_block)
 * - threads_per_block: 256
 *
 * ALGORITHM:
 * - Each thread = 1 logic neuron
 * - Read input activities
 * - Compute gate-specific logic
 * - Update membrane potential
 * - Fire spike if threshold crossed
 */
NIMCP_GLOBAL void kernel_update_logic_neurons(
    logic_neuron_state_t* neurons,
    const float* input_activities,
    uint32_t num_neurons,
    uint64_t timestamp,
    uint64_t delta_t
);

/**
 * @brief GPU device function: Compute AND gate
 *
 * DEVICE ONLY: Called from GPU kernels
 */
NIMCP_DEVICE float gpu_compute_and_gate(
    float input_a,
    float input_b,
    float threshold,
    float integration_window
);

/**
 * @brief GPU device function: Compute OR gate
 */
NIMCP_DEVICE float gpu_compute_or_gate(
    float input_a,
    float input_b,
    float threshold
);

/**
 * @brief GPU device function: Compute NOT gate
 */
NIMCP_DEVICE float gpu_compute_not_gate(
    float input,
    float baseline_rate,
    float inhibition_strength
);

/**
 * @brief GPU device function: Compute XOR gate
 */
NIMCP_DEVICE float gpu_compute_xor_gate(
    float input_a,
    float input_b,
    float threshold,
    float balance_tolerance
);

/**
 * @brief GPU device function: Compute IMPLIES gate
 */
NIMCP_DEVICE float gpu_compute_implies_gate(
    float input_a,
    float input_b,
    float antecedent_threshold,
    float consequent_threshold
);

#endif // NIMCP_ENABLE_CUDA

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get default neural logic configuration
 *
 * @param max_neurons Maximum logic neurons
 * @return Default configuration
 */
NIMCP_EXPORT neural_logic_config_t neural_logic_default_config(
    uint32_t max_neurons
);

/**
 * @brief Check if GPU is available for neural logic
 *
 * @return true if CUDA GPU available
 */
NIMCP_EXPORT bool neural_logic_gpu_available(void);

/**
 * @brief Get gate type name
 *
 * @param gate_type Logic gate type
 * @return Human-readable name (e.g., "AND", "OR")
 */
NIMCP_EXPORT const char* neural_logic_gate_name(logic_gate_type_t gate_type);

/**
 * @brief Associate brain with neural logic network for neuromodulation
 *
 * WHAT: Set brain reference for DA + ACh modulation
 * WHY:  Enable neurochemical modulation of logic gates
 * HOW:  Store brain pointer for neurotransmitter reading
 *
 * @param network Neural logic network instance
 * @param brain Brain instance (or NULL to clear)
 *
 * BIOLOGY:
 * - Dopamine modulates logical flexibility vs rigidity
 *   High DA → lower thresholds, permissive logic (exploratory)
 *   Low DA → higher thresholds, rigid logic (perseverative)
 *
 * - Acetylcholine modulates logical precision
 *   High ACh → precise thresholds, accurate logic
 *   Low ACh → imprecise thresholds, error-prone logic
 *
 * CLINICAL EXAMPLES:
 * - Depression (low DA): Rigid, black-and-white thinking
 * - Mania (high DA): Loose associations, illogical leaps
 * - ADHD (low ACh): Logical errors, misses contradictions
 * - Dementia (low ACh): Impaired reasoning, confabulation
 */
NIMCP_EXPORT void neural_logic_set_brain(neural_logic_network_t network, brain_t brain);

//=============================================================================
// Bio-Async Communication API
//=============================================================================

/**
 * @brief Get bio-async module context
 *
 * @param network Neural logic network handle
 * @return Bio-async module context, or NULL if not enabled
 */
NIMCP_EXPORT bio_module_context_t neural_logic_get_bio_context(neural_logic_network_t network);

/**
 * @brief Process pending bio-async messages
 *
 * @param network Neural logic network handle
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
NIMCP_EXPORT uint32_t neural_logic_process_bio_messages(neural_logic_network_t network, uint32_t max_messages);

/**
 * @brief Broadcast logic gate evaluation result via bio-async
 *
 * @param network Neural logic network handle
 * @param gate_id Gate neuron ID that was evaluated
 * @param output Logic gate output [0,1]
 * @param spiked Whether the gate spiked
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t neural_logic_broadcast_result(
    neural_logic_network_t network,
    uint32_t gate_id,
    float output,
    bool spiked
);

/**
 * @brief Broadcast logic circuit step completion
 *
 * @param network Neural logic network handle
 * @param spikes_generated Number of spikes generated
 * @param gates_evaluated Number of gates evaluated
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t neural_logic_broadcast_circuit_complete(
    neural_logic_network_t network,
    uint32_t spikes_generated,
    uint32_t gates_evaluated
);

//=============================================================================
// Quantum Acceleration API
//=============================================================================

/**
 * WHAT: Get quantum bridge instance (lazy initialization)
 * WHY:  Enable quantum-accelerated SAT solving for neural circuits
 * HOW:  Create bridge on first access, reuse thereafter
 *
 * @param network Neural logic network handle
 * @return Quantum bridge handle (opaque), or NULL if disabled
 *
 * USAGE:
 * - Automatically creates bridge if quantum enabled
 * - Returns cached bridge on subsequent calls
 * - Returns NULL if quantum disabled in config
 */
NIMCP_EXPORT void* neural_logic_get_quantum_bridge(neural_logic_network_t network);

/**
 * WHAT: Check if quantum acceleration is enabled
 *
 * @param network Neural logic network handle
 * @return true if quantum enabled and available
 */
NIMCP_EXPORT bool neural_logic_is_quantum_enabled(neural_logic_network_t network);

/**
 * WHAT: Enable/disable quantum acceleration at runtime
 *
 * @param network Neural logic network handle
 * @param enabled Enable quantum mode
 */
NIMCP_EXPORT void neural_logic_set_quantum_enabled(neural_logic_network_t network, bool enabled);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NEURAL_LOGIC_H
