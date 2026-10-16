//=============================================================================
// nimcp_lnn_types.h - Core Type Definitions for Liquid Neural Networks
//=============================================================================
/**
 * @file nimcp_lnn_types.h
 * @brief Core type definitions for Liquid Neural Networks in NIMCP
 *
 * WHAT: Fundamental data structures for Liquid Time-Constant (LTC) networks
 * WHY:  LNNs provide continuous-time dynamics with learnable time constants,
 *       replacing hand-tuned temporal parameters with emergent behavior
 * HOW:  Defines neurons, layers, networks with ODE-based state evolution
 *
 * BIOLOGICAL BASIS:
 * - Neurons have input-dependent time constants (like biological membrane τ)
 * - Sparse wiring patterns inspired by cortical connectivity
 * - Continuous-time dynamics model biological signal integration
 *
 * MATHEMATICAL FOUNDATION:
 * - LTC neuron dynamics: dx/dt = -[1/τ(x,I)] * x + f(x,I,θ)
 * - Liquid time constant: τ(x,I) = τ_base * σ(W_τ * [x; I] + b_τ)
 * - ODE solution via RK4, Heun, Euler, or adaptive methods
 *
 * CODING STANDARDS:
 * - Guard clauses (no nested ifs)
 * - WHAT-WHY-HOW documentation
 * - Single Responsibility Principle
 * - Biological grounding for all structures
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 * @version 1.0.0
 */

#ifndef NIMCP_LNN_TYPES_H
#define NIMCP_LNN_TYPES_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>
#include "utils/tensor/nimcp_tensor.h"

/* Substrate effects struct types (axon/dendrite). Embedded as values in
 * lnn_network_s so the full definitions are required. */
#include "core/substrate/nimcp_substrate_effects.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward decl of the full substrate state struct. The pointer on
 * lnn_network_s is borrowed (not owned) — whoever created the substrate
 * is responsible for its lifecycle. Defined in nimcp_neural_substrate.h. */
struct neural_substrate;

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of layers in an LNN network */
#define LNN_MAX_LAYERS 32

/** Default time constant range (milliseconds) */
#define LNN_TAU_MIN_DEFAULT 0.1f
#define LNN_TAU_MAX_DEFAULT 1000.0f

/** Default time step (milliseconds) */
#define LNN_DT_DEFAULT 1.0f

/** Magic number for validation */
#define LNN_MAGIC 0x4C4E4E00  /* "LNN\0" */

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

typedef struct lnn_neuron_s lnn_neuron_t;
typedef struct lnn_layer_s lnn_layer_t;
typedef struct lnn_network_s lnn_network_t;
typedef struct lnn_state_s lnn_state_t;
typedef struct lnn_config_s lnn_config_t;
typedef struct lnn_wiring_s lnn_wiring_t;
typedef struct lnn_gradient_ctx_s lnn_gradient_ctx_t;

/* Forward declarations for thalamic routing integration (Phase 2).
 * Defined in include/core/thalamic/nimcp_thalamic_channel.h and
 * include/middleware/routing/nimcp_thalamic_router.h respectively.
 * The LNN holds a borrowed router pointer and an owned channel; callers
 * supply the router via lnn_network_attach_thalamic_router(). */
struct thalamic_channel_s;
struct thalamic_router;

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief ODE solver methods
 *
 * WHAT: Numerical integration methods for continuous dynamics
 * WHY:  Different methods trade accuracy vs speed
 */
typedef enum {
    LNN_ODE_EULER = 0,          /**< 1st order, fast, less accurate */
    LNN_ODE_HEUN,               /**< 2nd order predictor-corrector */
    LNN_ODE_RK4,                /**< 4th order Runge-Kutta (default) */
    LNN_ODE_DOPRI5,             /**< Adaptive 5th order */
    LNN_ODE_IMPLICIT_EULER,     /**< For stiff systems */
    LNN_ODE_METHOD_COUNT
} lnn_ode_method_t;

/**
 * @brief Activation functions for LNN neurons
 *
 * WHAT: Nonlinear functions f(x) applied to neuron state
 * WHY:  Provide expressive power and bounded outputs
 * HOW:  Applied after computing weighted sum of inputs
 */
typedef enum {
    LNN_ACTIVATION_TANH = 0,    /**< tanh(x), bounded [-1, 1], biological default */
    LNN_ACTIVATION_SIGMOID,     /**< σ(x) = 1/(1+e^-x), bounded [0, 1] */
    LNN_ACTIVATION_RELU,        /**< max(0, x), unbounded, sparse gradients */
    LNN_ACTIVATION_GELU,        /**< Gaussian error linear unit (smooth ReLU) */
    LNN_ACTIVATION_SILU,        /**< Sigmoid-weighted linear unit (x*σ(x)) */
    LNN_ACTIVATION_SOFTPLUS,    /**< ln(1 + e^x), smooth ReLU, always positive */
    LNN_ACTIVATION_CUSTOM,      /**< User-defined function pointer */
    LNN_ACTIVATION_COUNT
} lnn_activation_t;

/**
 * @brief Wiring patterns for sparse connectivity
 *
 * WHAT: Predefined connectivity topologies inspired by neuroscience
 * WHY:  Sparse wiring reduces parameters, improves generalization
 * HOW:  Generate adjacency matrices following biological principles
 */
typedef enum {
    LNN_WIRING_FULL = 0,        /**< Dense all-to-all connectivity (baseline) */
    LNN_WIRING_RANDOM,          /**< Random sparse (Erdos-Renyi graph) */
    LNN_WIRING_SMALL_WORLD,     /**< Watts-Strogatz small-world (local + long-range) */
    LNN_WIRING_SCALE_FREE,      /**< Barabasi-Albert scale-free (hub structure) */
    LNN_WIRING_MODULAR,         /**< Clustered modules with sparse inter-module */
    LNN_WIRING_FEEDFORWARD,     /**< Strictly feedforward layers (no recurrence) */
    LNN_WIRING_RECURRENT,       /**< Full recurrent within layer */
    LNN_WIRING_NCP,             /**< Neural Circuit Policy (original LNN paper) */
    LNN_WIRING_CUSTOM,          /**< User-defined adjacency matrix */
    LNN_WIRING_COUNT
} lnn_wiring_type_t;

/**
 * @brief Training modes for LNN gradient computation
 *
 * WHAT: Methods to compute gradients through continuous dynamics
 * WHY:  Standard backprop doesn't apply to ODEs; need specialized methods
 * HOW:  Either unroll dynamics (BPTT) or use adjoint sensitivity
 */
typedef enum {
    LNN_TRAIN_BPTT = 0,         /**< Backprop through time (memory O(T)) */
    LNN_TRAIN_ADJOINT,          /**< Adjoint method (memory O(1), default) */
    LNN_TRAIN_RTRL,             /**< Real-time recurrent learning (online) */
    LNN_TRAIN_EPROP,            /**< Eligibility propagation (bio-plausible) */
    LNN_TRAIN_MODE_COUNT
} lnn_train_mode_t;

/**
 * @brief LNN neuron roles for NCP wiring
 *
 * WHAT: Functional categorization of neurons in NCP architecture
 * WHY:  Structured wiring based on functional roles improves learning
 * HOW:  Sensory → Inter → Command → Motor hierarchy
 */
typedef enum {
    LNN_ROLE_SENSORY = 0,       /**< Input layer neurons (receive external signals) */
    LNN_ROLE_INTER,             /**< Hidden interneurons (internal processing) */
    LNN_ROLE_COMMAND,           /**< Decision neurons (high-level commands) */
    LNN_ROLE_MOTOR,             /**< Output neurons (generate actions) */
    LNN_ROLE_COUNT
} lnn_neuron_role_t;

/**
 * @brief LNN state validity flags
 *
 * WHAT: Health status indicators for numerical stability
 * WHY:  LNNs can become unstable during training (NaN, explosion)
 * HOW:  Check state norms and gradients after each step
 */
typedef enum {
    LNN_STATE_VALID = 0,        /**< Normal operation */
    LNN_STATE_NAN_DETECTED,     /**< NaN in state or gradients */
    LNN_STATE_INF_DETECTED,     /**< Inf in state or gradients */
    LNN_STATE_EXPLOSION,        /**< State norm exceeds threshold */
    LNN_STATE_VANISHING,        /**< State norm below threshold */
    LNN_STATE_UNSTABLE          /**< Oscillations or divergence detected */
} lnn_state_health_t;

/*=============================================================================
 * Configuration Structures
 *===========================================================================*/

/**
 * @brief Neuron-level configuration
 */
typedef struct {
    lnn_activation_t activation;
    float tau_base_init;        /**< Initial base τ (ms) */
    float tau_min;              /**< Minimum τ (ms) */
    float tau_max;              /**< Maximum τ (ms) */
    float weight_init_std;      /**< Weight initialization std */
    bool learn_tau;             /**< Whether to learn τ parameters */
} lnn_neuron_config_t;

/*=============================================================================
 * Core Structures
 *===========================================================================*/

/**
 * @brief Single Liquid Time-Constant (LTC) neuron
 *
 * WHAT: Represents one neuron with input-dependent time constant
 * WHY:  LTC neurons adapt their temporal response based on input context
 * HOW:  State evolves via dx/dt = -x/τ(x,I) + f(W_in*I + W_rec*x + b)
 *
 * BIOLOGICAL BASIS:
 * - x: Membrane potential (analogous to voltage in real neurons)
 * - τ: Membrane time constant (RC time constant in electrical model)
 * - w_in: Synaptic weights from inputs (AMPA/NMDA conductances)
 * - w_rec: Recurrent weights (feedback connections)
 */
struct lnn_neuron_s {
    /* Neuron identity */
    uint32_t id;                /**< Unique neuron ID within layer */
    lnn_neuron_role_t role;     /**< Functional role (sensory/inter/command/motor) */

    /* Current state */
    float x;                    /**< Membrane potential / activation state */
    float x_prev;               /**< Previous state (for derivative computation) */
    float dx_dt;                /**< Current time derivative dx/dt */

    /* Time constant (learnable) */
    float tau_base;             /**< Base time constant τ_base ∈ [1, 1000] ms */
    float tau_min;              /**< Minimum time constant (ms) */
    float tau_max;              /**< Maximum time constant (ms) */
    float tau_current;          /**< Current effective τ after input modulation */
    float* w_tau;               /**< Time constant weights [n_inputs + n_recurrent] */
    float b_tau;                /**< Time constant bias */

    /* Input weights (learnable) */
    float* w_in;                /**< Input weights [n_inputs] */
    float b_in;                 /**< Input bias */

    /* Recurrent weights (learnable) */
    float* w_rec;               /**< Recurrent weights [n_neurons] (CSR if sparse) */

    /* Activation function */
    lnn_activation_t activation; /**< Nonlinearity applied to state */

    /* Dimensions */
    uint32_t n_inputs;          /**< Number of external inputs */
    uint32_t n_recurrent;       /**< Number of recurrent connections */

    /* Gradients (for training) */
    float* grad_w_in;           /**< ∂L/∂w_in */
    float* grad_w_rec;          /**< ∂L/∂w_rec */
    float* grad_w_tau;          /**< ∂L/∂w_τ */
    float grad_b_in;            /**< ∂L/∂b_in */
    float grad_b_tau;           /**< ∂L/∂b_τ */
    float grad_tau_base;        /**< ∂L/∂τ_base */
};

/**
 * @brief LNN layer containing multiple neurons
 *
 * WHAT: Collection of neurons with shared wiring pattern
 * WHY:  Organize neurons into functional groups, enable SIMD
 * HOW:  Store neuron array + tensor views for efficient computation
 */
struct lnn_layer_s {
    /* Layer identity */
    uint32_t id;                /**< Layer index in network */
    char name[64];              /**< Human-readable name */

    /* Neurons */
    lnn_neuron_t* neurons;      /**< Array of neurons [n_neurons] */
    uint32_t n_neurons;         /**< Number of neurons in layer */

    /* Connectivity */
    lnn_wiring_t* wiring;       /**< Sparse wiring pattern */

    /* Layer-level state (tensor form for SIMD) */
    nimcp_tensor_t* x;          /**< State vector [n_neurons] (position q in Hamiltonian) */
    nimcp_tensor_t* tau;        /**< Time constants [n_neurons] */
    nimcp_tensor_t* dx_dt;      /**< Derivative vector [n_neurons] */

    /* Hamiltonian dynamics (optional — NULL if standard LTC) */
    nimcp_tensor_t* p;          /**< Momentum vector [n_neurons] (conjugate to x=q) */
    void* H_net;                /**< lnn_hamiltonian_net_t* (NULL = standard LTC) */
    bool use_hamiltonian;       /**< Use Hamiltonian dynamics instead of LTC */

    /* Weight tensors (for batched operations) */
    nimcp_tensor_t* W_in;       /**< Input weight matrix [n_neurons, n_inputs] */
    nimcp_tensor_t* W_rec;      /**< Recurrent weight matrix [n_neurons, n_neurons] */
    nimcp_tensor_t* W_tau;      /**< τ modulation weights [n_neurons, n_inputs + n_neurons] */
    nimcp_tensor_t* b_in;       /**< Input bias vector [n_neurons] */
    nimcp_tensor_t* b_tau;      /**< τ bias vector [n_neurons] */
    nimcp_tensor_t* tau_base;   /**< Base time constants [n_neurons] */

    /* Gradient tensors */
    nimcp_tensor_t* grad_W_in;
    nimcp_tensor_t* grad_W_rec;
    nimcp_tensor_t* grad_W_tau;
    nimcp_tensor_t* grad_b_in;
    nimcp_tensor_t* grad_b_tau;
    nimcp_tensor_t* grad_tau_base;

    /* ODE solver configuration */
    lnn_ode_method_t ode_method; /**< Integration method for this layer */
    float dt;                    /**< Time step (ms) */

    /* Statistics */
    uint64_t step_count;         /**< Number of forward steps */
    float avg_tau;               /**< Average time constant */
    float min_tau;               /**< Minimum time constant */
    float max_tau;               /**< Maximum time constant */

    /* Layer normalization */
    bool use_layer_norm;            /**< Enable layer normalization */
    float layer_norm_eps;           /**< Layer norm epsilon for numerical stability */

    /* GPU acceleration (NULL = CPU only) */
    void* gpu_lnn_layer;            /**< nimcp_lnn_layer_gpu_t* (GPU-resident layer state) */
    void* gpu_ctx;                  /**< nimcp_gpu_context_t* for GPU tensor uploads */

    /* Substrate integration (Phase 2 biological-substrate adapter).
     * Borrowed pointer to the owning network's cached dendrite effects
     * struct. Set each forward step by lnn_network_forward_step when a
     * substrate is attached + the enabled/tau_compose knobs are on;
     * cleared (NULL) otherwise. `lnn_layer_forward` consults this to
     * wrap the learned tau tensor with the substrate multiplier in the
     * CPU LTC path — composition, not replacement: learned tau_base and
     * W_tau gradients continue to flow unchanged. */
    const dendrite_substrate_effects_t* substrate_dend_effects;

    /* HNN Rayleigh-dissipation adapter — borrowed pointer to the owning
     * network's cached axon effects struct. Set each forward step by
     * lnn_network_forward_step when a substrate is attached + the HNN
     * dissipation knob is on; cleared (NULL) otherwise. Consumed by the
     * Hamiltonian integrator via lnn_hamiltonian_step_stormer_verlet_with_substrate
     * to apply γ = γ_max·(1 - overall_capacity) damping on momentum. */
    const axon_substrate_effects_t* substrate_axon_effects;
};

/**
 * @brief Network-level statistics
 *
 * WHAT: Performance and health metrics for LNN network
 * WHY:  Monitor training progress and numerical stability
 * HOW:  Accumulate counts and norms during forward/backward passes
 */
typedef struct {
    uint64_t forward_steps;         /**< Total forward steps executed */
    uint64_t backward_steps;        /**< Total backward steps executed */
    uint64_t ode_evaluations;       /**< Total ODE function evaluations */
    double total_compute_time_ms;   /**< Cumulative computation time (ms) */
    double avg_step_time_ms;        /**< Average time per step (ms) */
    float avg_tau_network;          /**< Network-wide average time constant */
    float state_norm;               /**< Current state L2 norm */
    float gradient_norm;            /**< Current gradient L2 norm */
    lnn_state_health_t health;      /**< Overall health status */
    uint32_t nan_count;             /**< Number of NaN detections */
    uint32_t inf_count;             /**< Number of Inf detections */
    size_t memory_usage_bytes;      /**< Current memory usage */
} lnn_network_stats_t;

/**
 * @brief Full LNN network with multiple layers
 *
 * WHAT: Complete network architecture with training context
 * WHY:  Encapsulate entire model state and integration points
 * HOW:  Layer stack + configuration + training context + integrations
 */
struct lnn_network_s {
    /* Network identity */
    uint32_t id;                /**< Network instance ID */
    char name[64];              /**< Network name */

    /* Architecture */
    lnn_layer_t** layers;       /**< Array of layer pointers [n_layers] */
    uint32_t n_layers;          /**< Number of layers */

    /* Input/output dimensions */
    uint32_t n_inputs;          /**< Network input dimension */
    uint32_t n_outputs;         /**< Network output dimension */

    /* Global configuration */
    lnn_config_t* config;       /**< Network configuration */

    /* Training context */
    lnn_gradient_ctx_t* grad_ctx; /**< Gradient computation context */
    lnn_train_mode_t train_mode;  /**< Training algorithm (BPTT/adjoint) */
    bool is_training;             /**< Training mode flag */

    /* State history (for BPTT) */
    nimcp_tensor_t** state_history; /**< Time series of states [T][n_neurons] */
    uint32_t history_len;           /**< Current history length */
    uint32_t history_capacity;      /**< Maximum history length */
    uint32_t history_write_idx;     /**< Circular buffer write index */

    /* Integration handles (opaque pointers) */
    void* optimizer;            /**< nimcp_optimizer_context_t* */
    void* gradient_manager;     /**< nimcp_gradient_manager_ctx_t* */
    void* bio_ctx;              /**< bio_module_context_t (bio-async) */
    void* immune_bridge;        /**< lnn_immune_bridge_t* */

    /* Parallelization */
    void* thread_pool;          /**< nimcp_thread_pool_t* */
    uint32_t n_threads;         /**< Number of worker threads */

    /* Thread safety */
    void* mutex;                /**< Mutex for concurrent access */

    /* GPU acceleration */
    void* gpu_ctx;              /**< nimcp_gpu_context_t* for GPU LNN operations */

    /* Statistics */
    lnn_network_stats_t stats;  /**< Global network statistics */

    /* Substrate integration (Phase 2 biological-substrate adapter).
     * The substrate pointer is BORROWED — the network does not own it. */
    struct neural_substrate*     substrate;
    axon_substrate_effects_t     cached_axon_effects;
    dendrite_substrate_effects_t cached_dend_effects;
    uint32_t                     substrate_steps_since_update;

    /* Thalamic routing — Phase 2 of multi-network thalamic rollout.
     * Channel pointer is owned; created at lnn_network_attach_thalamic_router
     * and destroyed at lnn_network_destroy. Null when not attached. */
    struct thalamic_channel_s*   thalamic_channel;
};

/**
 * @brief Sparse wiring configuration
 *
 * WHAT: Adjacency structure for sparse neural connectivity
 * WHY:  Reduce parameters and memory, model biological sparsity
 * HOW:  Compressed Sparse Row (CSR) format for efficient sparse matmul
 *
 * BIOLOGICAL BASIS:
 * - Cortical neurons connect to ~10-20% of neighbors (not all-to-all)
 * - Small-world topology: local clustering + long-range connections
 * - Scale-free networks: hub neurons with many connections
 */
struct lnn_wiring_s {
    lnn_wiring_type_t type;     /**< Wiring pattern type */

    /* Adjacency representation (CSR format) */
    uint32_t* row_ptr;          /**< CSR row pointers [n_neurons + 1] */
    uint32_t* col_idx;          /**< CSR column indices [n_edges] */
    float* edge_weights;        /**< Optional edge weights [n_edges] (NULL = unweighted) */

    uint32_t n_neurons;         /**< Number of neurons */
    uint32_t n_edges;           /**< Number of edges (synapses) */
    float sparsity;             /**< Fraction of zero connections */

    /* NCP-specific parameters */
    uint32_t n_sensory;         /**< Number of sensory neurons */
    uint32_t n_inter;           /**< Number of interneurons */
    uint32_t n_command;         /**< Number of command neurons */
    uint32_t n_motor;           /**< Number of motor neurons */

    /* Small-world parameters (Watts-Strogatz) */
    float rewire_prob;          /**< Rewiring probability [0, 1] */
    uint32_t k_neighbors;       /**< Initial ring lattice neighbors */

    /* Scale-free parameters (Barabasi-Albert) */
    uint32_t m_edges;           /**< Edges per new node during growth */
};

/* Note: struct lnn_gradient_ctx_s is defined in nimcp_lnn_gradient.h
 * to avoid circular dependencies and allow for the complete definition
 * with all adjoint method state variables. */

/*=============================================================================
 * Error Codes
 *===========================================================================*/

#define LNN_SUCCESS                     0
#define LNN_ERROR_NULL_POINTER         -1
#define LNN_ERROR_INVALID_CONFIG       -2
#define LNN_ERROR_INVALID_DIMENSION    -3
#define LNN_ERROR_OUT_OF_MEMORY        -4
#define LNN_ERROR_INVALID_STATE        -5
#define LNN_ERROR_ODE_DIVERGENCE       -6
#define LNN_ERROR_GRADIENT_EXPLOSION   -7
#define LNN_ERROR_GRADIENT_VANISHING   -8
#define LNN_ERROR_WIRING_INVALID       -9
#define LNN_ERROR_NOT_INITIALIZED     -10
#define LNN_ERROR_THREAD_FAILURE      -11
#define LNN_ERROR_OPERATION_FAILED    -12
#define LNN_ERROR_INVALID_PARAM       -13

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_TYPES_H */
