//=============================================================================
// nimcp_lnn_config.h - Configuration Structures for Liquid Neural Networks
//=============================================================================
/**
 * @file nimcp_lnn_config.h
 * @brief Configuration structures for LNN networks
 *
 * WHAT: Hierarchical configuration for Liquid Neural Networks
 * WHY:  LNNs require neuron, layer, and network-level parameters
 * HOW:  Three-tier config (neuron → layer → network) with validation
 *
 * BIOLOGICAL GROUNDING:
 * - Learnable time constants model adaptive neural timescales
 * - Sparse wiring reflects biological connectivity patterns
 * - ODE integration captures continuous-time dynamics
 *
 * USAGE:
 *   lnn_config_t config;
 *   lnn_config_default(&config);  // Initialize with sensible defaults
 *   lnn_config_validate(&config);  // Validate before use
 *   lnn_config_destroy(&config);   // Free allocated resources
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 * @version 1.0.0
 */

#ifndef NIMCP_LNN_CONFIG_H
#define NIMCP_LNN_CONFIG_H

#include "nimcp_lnn_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Neuron-Level Configuration
//=============================================================================
// NOTE: lnn_neuron_config_t is defined in nimcp_lnn_types.h

//=============================================================================
// Layer-Level Configuration
//=============================================================================

/**
 * @brief Configuration for LNN layers
 *
 * WHAT: Parameters for a layer of LTC neurons
 * WHY:  Layers can have different sizes, wiring, and ODE methods
 * HOW:  Struct containing layer size, neuron config, wiring, ODE settings
 */
typedef struct {
    /* Architecture */
    uint32_t n_neurons;             /**< Number of neurons in layer */

    /* Activation */
    lnn_activation_t activation;    /**< Activation function */

    /* Time constants */
    float tau_base_init;            /**< Initial base τ (ms) */
    float tau_min;                  /**< Minimum τ (ms) */
    float tau_max;                  /**< Maximum τ (ms) */
    bool learn_tau;                 /**< Whether to learn τ parameters */

    /* Initialization */
    float weight_init_std;          /**< Weight initialization std dev */
    uint64_t seed;                  /**< Random seed (0 = use time) */

    /* Wiring */
    lnn_wiring_type_t wiring_type;  /**< Connectivity pattern */
    float sparsity;                 /**< Target sparsity [0, 1) - default 0.0 */

    /* ODE solver */
    lnn_ode_method_t ode_method;    /**< Integration method (RK4, Euler, etc.) */
    float dt;                       /**< Integration time step (ms) - default 1.0 */

    /* Normalization */
    bool use_layer_norm;            /**< Enable layer normalization - default false */
    float layer_norm_eps;           /**< Layer norm epsilon */
} lnn_layer_config_t;

//=============================================================================
// Network-Level Configuration
//=============================================================================

/**
 * @brief Complete LNN network configuration
 *
 * WHAT: Full configuration for multi-layer LNN network
 * WHY:  Networks require architecture, training, and integration settings
 * HOW:  Comprehensive struct with all network parameters
 *
 * CONFIGURATION SECTIONS:
 * - Architecture: Layer structure, input/output dimensions
 * - ODE: Solver settings, adaptive step size
 * - Training: BPTT/adjoint mode, gradient clipping
 * - Wiring: NCP-specific neuron counts
 * - Parallelization: Thread pool, SIMD
 * - Bio-async: Module ID, enable flag
 * - Immune: Instability detection, integration
 * - Logging: Debug output control
 * - Memory: Allocation limits, history preallocation
 */
struct lnn_config_s {
    /* Architecture */
    uint32_t n_layers;              /**< Number of layers (excluding input) */
    lnn_layer_config_t* layer_configs; /**< Per-layer configurations [n_layers] */
    uint32_t n_inputs;              /**< Input dimension */
    uint32_t n_outputs;             /**< Output dimension */

    /* ODE settings */
    lnn_ode_method_t default_ode_method; /**< Default solver (RK4 recommended) */
    float default_dt;               /**< Default time step (ms) - default 1.0 */
    float adaptive_dt_min;          /**< Minimum adaptive dt (ms) - default 0.01 */
    float adaptive_dt_max;          /**< Maximum adaptive dt (ms) - default 10.0 */
    float adaptive_error_tol;       /**< Error tolerance for adaptive solvers - default 1e-5 */

    /* Training */
    lnn_train_mode_t train_mode;    /**< Training mode (BPTT, adjoint, RTRL, eProp) */
    uint32_t bptt_truncation;       /**< Truncation length for BPTT - default 100 */
    bool use_gradient_checkpointing; /**< Enable gradient checkpointing - default false */
    uint32_t checkpoint_interval;   /**< Checkpoint every N steps - default 10 */
    float gradient_clip_norm;       /**< Gradient clipping threshold - default 1.0 */

    /* Wiring (NCP) */
    uint32_t ncp_sensory;           /**< Number of sensory neurons (NCP mode) */
    uint32_t ncp_inter;             /**< Number of interneurons (NCP mode) */
    uint32_t ncp_command;           /**< Number of command neurons (NCP mode) */
    uint32_t ncp_motor;             /**< Number of motor neurons (NCP mode) */

    /* Parallelization */
    uint32_t n_threads;             /**< Number of worker threads (0 = auto-detect) */
    bool enable_simd;               /**< Enable SIMD optimizations - default true */

    /* Bio-async */
    bool enable_bio_async;          /**< Enable bio-async integration - default false */
    uint16_t bio_module_id;         /**< Bio-async module ID (BIO_MODULE_LNN_*) */

    /* Immune */
    bool enable_immune_integration; /**< Enable immune system integration - default false */
    float instability_threshold;    /**< State norm threshold for instability - default 1e6 */

    /* Logging */
    bool enable_logging;            /**< Enable debug logging - default false */
    int log_level;                  /**< Log level (0=ERROR, 1=WARN, 2=INFO, 3=DEBUG) */

    /* Memory */
    size_t max_memory_bytes;        /**< Maximum memory usage (0 = unlimited) */
    bool preallocate_history;       /**< Preallocate state history - default false */
};

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Initialize configuration with sensible defaults
 *
 * WHAT: Populate config struct with default values
 * WHY:  Provides working baseline configuration
 * HOW:  Sets tau_base=10ms, RK4 solver, tanh activation, etc.
 *
 * DEFAULT VALUES:
 * - Neuron: tau_base=10ms, tau_min=0.1ms, tau_max=1000ms, tanh activation
 * - ODE: RK4 solver, dt=1.0ms
 * - Training: Adjoint mode, gradient_clip_norm=1.0
 * - Parallelization: Auto-detect threads, SIMD enabled
 *
 * @param config Configuration struct to initialize (must be non-NULL)
 * @return 0 on success, negative on error
 */
int lnn_config_default(lnn_config_t* config);

/**
 * @brief Create NCP (Neural Circuit Policy) configuration
 *
 * WHAT: Configure network with NCP wiring pattern
 * WHY:  NCP is original LNN architecture from Hasani et al.
 * HOW:  Creates 4-layer network (sensory → inter → command → motor)
 *
 * ARCHITECTURE:
 *   Input → Sensory → Interneurons → Command → Motor → Output
 *           (n_inputs)  (n_inter)    (n_command) (n_outputs)
 *
 * WIRING:
 * - Sensory: Receive inputs, project to interneurons
 * - Inter: Hidden processing, project to command
 * - Command: Decision-making, project to motor
 * - Motor: Output neurons
 *
 * @param config Configuration struct to populate (must be non-NULL)
 * @param n_inputs Number of input neurons (sensory layer size)
 * @param n_inter Number of interneurons (hidden processing)
 * @param n_command Number of command neurons (decision layer)
 * @param n_outputs Number of output neurons (motor layer size)
 * @return 0 on success, negative on error
 */
int lnn_config_ncp(lnn_config_t* config,
                   uint32_t n_inputs,
                   uint32_t n_inter,
                   uint32_t n_command,
                   uint32_t n_outputs);

/**
 * @brief Validate configuration before network creation
 *
 * WHAT: Check configuration for inconsistencies and invalid values
 * WHY:  Catch errors early before expensive network creation
 * HOW:  Validate dimensions, bounds, parameter ranges
 *
 * VALIDATION CHECKS:
 * - n_layers > 0
 * - layer_configs != NULL if n_layers > 0
 * - n_inputs, n_outputs > 0
 * - tau_min < tau_max
 * - 0 <= sparsity < 1
 * - dt > 0
 * - gradient_clip_norm > 0
 *
 * @param config Configuration to validate (must be non-NULL)
 * @return 0 if valid, negative error code with specific failure
 */
int lnn_config_validate(const lnn_config_t* config);

/**
 * @brief Destroy configuration and free allocated resources
 *
 * WHAT: Free layer_configs array and reset struct
 * WHY:  Prevent memory leaks from dynamically allocated layer configs
 * HOW:  nimcp_free(layer_configs), zero out struct
 *
 * NOTE: Safe to call multiple times (checks for NULL)
 *
 * @param config Configuration to destroy (NULL-safe)
 */
void lnn_config_destroy(lnn_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_CONFIG_H */
