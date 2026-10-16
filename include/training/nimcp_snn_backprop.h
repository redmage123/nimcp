//=============================================================================
// nimcp_snn_backprop.h - SNN Backpropagation Training Integration
//=============================================================================
/**
 * @file nimcp_snn_backprop.h
 * @brief Gradient-based training for Spiking Neural Networks
 *
 * WHAT: Backpropagation through time (BPTT) and biologically-plausible
 *       training algorithms for SNNs using surrogate gradients
 * WHY:  SNNs need gradient-based training but discrete spikes are
 *       non-differentiable; surrogate gradients provide smooth approximations
 * HOW:  Replace spike derivative with smooth surrogates, unroll through time,
 *       compute gradients via chain rule, integrate with NIMCP training pipeline
 *
 * BIOLOGICAL BASIS:
 * - E-prop (Bellec et al. 2020): Online BPTT with eligibility traces
 * - SuperSpike (Zenke & Ganguli 2018): Fast surrogate gradient learning
 * - Slayer (Shrestha & Orchard 2018): Spike layer error reassignment
 *
 * MATHEMATICAL FOUNDATION:
 * Standard gradient descent fails for SNNs because spike function is
 * non-differentiable:
 *   S(t) = Θ(V(t) - V_thresh)  where Θ is Heaviside step
 *   dS/dV = δ(V - V_thresh)     Dirac delta (infinite at threshold)
 *
 * Solution: Surrogate gradient σ'(V) approximates δ(V):
 *   SuperSpike:    σ'(V) = 1 / (β|V| + 1)²
 *   Fast Sigmoid:  σ'(V) = V / (1 + |V|)²
 *   Piecewise:     σ'(V) = max(0, 1 - |V|/a)
 *
 * INTEGRATION WITH NIMCP:
 * - Uses existing gradient_manager for gradient accumulation/scaling
 * - Leverages loss_functions for spike-aware loss computation
 * - Connects to SNN network API for forward/backward passes
 * - Bio-async enabled for distributed training
 *
 * CODING STANDARDS:
 * - Guard clauses (no nested ifs)
 * - Functions < 50 lines
 * - WHAT-WHY-HOW documentation on all functions
 * - Single Responsibility Principle
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 * @version 1.0.0
 */

#ifndef NIMCP_SNN_BACKPROP_H
#define NIMCP_SNN_BACKPROP_H

#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_network.h"
#include "middleware/training/nimcp_gradient_manager.h"
#include "middleware/training/nimcp_loss_functions.h"
#include "utils/tensor/nimcp_tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum sequence length for BPTT unrolling */
#define SNN_BPTT_MAX_UNROLL 1000

/** Default surrogate gradient beta (steepness) */
#define SNN_SURROGATE_BETA_DEFAULT 1.0f

/** Default eligibility trace decay (ms) */
#define SNN_ELIGIBILITY_TAU_DEFAULT 20.0f

/** Maximum gradient norm for clipping */
#define SNN_GRADIENT_CLIP_DEFAULT 10.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Surrogate gradient methods for spike backpropagation
 *
 * WHAT: Smooth approximations of the spike function derivative
 * WHY:  Heaviside step function has undefined/infinite derivative
 * HOW:  Replace Dirac delta with smooth, bounded function
 *
 * REFERENCES:
 * - SuperSpike: Zenke & Ganguli, "SuperSpike: Supervised Learning in
 *   Multilayer Spiking Neural Networks", Neural Computation 2018
 * - Fast Sigmoid: Shrestha & Orchard, "SLAYER", NeurIPS 2018
 * - Arctan: Bohte et al., "Error-backpropagation in temporally encoded
 *   networks of spiking neurons", Neurocomputing 2002
 *
 * NOTE: Base surrogate constants (SNN_SURROGATE_SIGMOID through
 * SNN_SURROGATE_RECTANGULAR) are defined in nimcp_snn_types.h as
 * snn_surrogate_t.  When that header is included (which is always,
 * since we #include it above), we typedef snn_surrogate_method_t to
 * snn_surrogate_t and only add backprop-specific extensions.
 */
#ifdef NIMCP_SNN_TYPES_H
/* Base surrogate constants already defined in nimcp_snn_types.h. */
typedef snn_surrogate_t snn_surrogate_method_t;
/* Backprop-specific extension (value follows SNN_SURROGATE_COUNT from types) */
#define SNN_SURROGATE_EXPONENTIAL ((snn_surrogate_method_t)(SNN_SURROGATE_COUNT))
/* Total including the extension */
#define SNN_SURROGATE_BACKPROP_COUNT (SNN_SURROGATE_EXPONENTIAL + 1)
#else
typedef enum {
    SNN_SURROGATE_SUPERSPIKE = 0,
    SNN_SURROGATE_FAST_SIGMOID,
    SNN_SURROGATE_SIGMOID,
    SNN_SURROGATE_ARCTAN,
    SNN_SURROGATE_TRIANGULAR,
    SNN_SURROGATE_RECTANGULAR,
    SNN_SURROGATE_EXPONENTIAL,
    SNN_SURROGATE_COUNT
} snn_surrogate_method_t;
#define SNN_SURROGATE_BACKPROP_COUNT SNN_SURROGATE_COUNT
#endif

/**
 * @brief Training algorithm for SNN backpropagation
 *
 * WHAT: Different approaches to gradient computation through time
 * WHY:  Trade-off between biological plausibility, memory, and accuracy
 * HOW:  Each method has different computational/memory characteristics
 *
 * BIOLOGICAL PLAUSIBILITY (most to least):
 * 1. E_PROP: Local eligibility traces (Bellec et al. 2020)
 * 2. RTRL: Real-time recurrent learning
 * 3. SLAYER: Spike layer error reassignment
 * 4. BPTT: Full backprop through time (least biologically plausible)
 *
 * NOTE: Some constants (SNN_TRAIN_EPROP, SNN_TRAIN_SLAYER,
 * SNN_TRAIN_DECOLLE) overlap with snn_train_mode_t in
 * nimcp_snn_types.h.  When that header is included, we typedef
 * snn_train_algorithm_t to snn_train_mode_t and only add the
 * backprop-specific algorithms.
 */
#ifdef NIMCP_SNN_TYPES_H
/* Base train mode constants already defined in nimcp_snn_types.h. */
typedef snn_train_mode_t snn_train_algorithm_t;
/* Backprop-specific algorithm constants (values follow SNN_TRAIN_COUNT) */
#define SNN_TRAIN_BPTT           ((snn_train_algorithm_t)(SNN_TRAIN_COUNT))
#define SNN_TRAIN_TRUNCATED_BPTT ((snn_train_algorithm_t)(SNN_TRAIN_COUNT + 1))
#define SNN_TRAIN_RTRL           ((snn_train_algorithm_t)(SNN_TRAIN_COUNT + 2))
#define SNN_TRAIN_HYBRID         ((snn_train_algorithm_t)(SNN_TRAIN_COUNT + 3))
#define SNN_TRAIN_MODE_COUNT     ((snn_train_algorithm_t)(SNN_TRAIN_COUNT + 4))
#else
typedef enum {
    SNN_TRAIN_BPTT = 0,
    SNN_TRAIN_TRUNCATED_BPTT,
    SNN_TRAIN_EPROP,
    SNN_TRAIN_RTRL,
    SNN_TRAIN_SLAYER,
    SNN_TRAIN_DECOLLE,
    SNN_TRAIN_HYBRID,
    SNN_TRAIN_MODE_COUNT
} snn_train_algorithm_t;
#endif

/**
 * @brief Loss functions for SNN training
 *
 * WHAT: Objective functions for spike-based learning
 * WHY:  Different tasks require different loss formulations
 * HOW:  Measure error in spike timing, count, or rate
 */
typedef enum {
    SNN_LOSS_SPIKE_COUNT = 0,       /**< L2 on spike count per neuron */
    SNN_LOSS_FIRST_SPIKE_TIME,      /**< L2 on first spike timing */
    SNN_LOSS_RATE_CODED_MSE,        /**< MSE on decoded firing rates */
    SNN_LOSS_RATE_CODED_CROSS_ENTROPY, /**< Cross-entropy on spike rates */
    SNN_LOSS_TEMPORAL_CROSS_ENTROPY, /**< Cross-entropy over time */
    SNN_LOSS_VAN_ROSSUM,            /**< van Rossum spike distance metric */
    SNN_LOSS_VICTOR_PURPURA,        /**< Victor-Purpura spike distance */
    SNN_LOSS_MEMBRANE_POTENTIAL,    /**< MSE on final membrane potential */
    SNN_LOSS_CUSTOM,                /**< User-defined loss function */
    SNN_LOSS_TYPE_COUNT
} snn_loss_type_t;

/**
 * @brief Temporal encoding for spike sequences
 *
 * WHAT: How to represent time dimension in gradient computation
 * WHY:  Different tasks need different temporal representations
 * HOW:  Batch across time vs. sequential processing
 */
typedef enum {
    SNN_TEMPORAL_BATCH = 0,         /**< Process full sequence as batch */
    SNN_TEMPORAL_ONLINE,            /**< Process one timestep at a time */
    SNN_TEMPORAL_SLIDING_WINDOW     /**< Sliding window over time */
} snn_temporal_mode_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Surrogate gradient configuration
 *
 * WHAT: Parameters for surrogate gradient computation
 * WHY:  Control shape and scale of surrogate function
 * HOW:  beta controls steepness, width controls support
 */
typedef struct {
    snn_surrogate_method_t method;  /**< Surrogate gradient method */
    float beta;                      /**< Steepness parameter (default 1.0) */
    float width;                     /**< Width parameter for triangular/rectangular */
    bool adaptive_beta;              /**< Adapt beta during training */
    float beta_min;                  /**< Minimum beta value */
    float beta_max;                  /**< Maximum beta value */
} snn_surrogate_config_t;

/**
 * @brief BPTT (Backprop Through Time) configuration
 *
 * WHAT: Parameters for temporal gradient unrolling
 * WHY:  Control memory usage and gradient quality
 * HOW:  Truncate unroll, detach gradients periodically
 */
typedef struct {
    uint32_t unroll_steps;           /**< Number of timesteps to unroll */
    bool truncate;                   /**< Use truncated BPTT */
    uint32_t truncation_length;      /**< Truncation window (steps) */
    bool detach_spike_grad;          /**< Detach spike gradients (reduce memory) */
    bool accumulate_over_time;       /**< Accumulate gradients across timesteps */
} snn_bptt_config_t;

/**
 * @brief E-prop (Eligibility Propagation) configuration
 *
 * WHAT: Parameters for online eligibility-based learning
 * WHY:  Biologically plausible, memory-efficient alternative to BPTT
 * HOW:  Local eligibility traces modulated by global learning signal
 *
 * REFERENCE: Bellec et al. "A solution to the learning dilemma for
 *            recurrent networks of spiking neurons", Nature Comm. 2020
 */
typedef struct {
    float eligibility_tau;           /**< Eligibility trace decay (ms) */
    float learning_signal_delay;     /**< Delay for learning signal (ms) */
    bool use_symmetric_eprop;        /**< Use symmetric e-prop variant */
    bool adaptive_learning_signal;   /**< Adapt learning signal strength */
    float kappa;                     /**< Dampening factor [0, 1] */
} snn_eprop_config_t;

/**
 * @brief RTRL (Real-Time Recurrent Learning) configuration
 *
 * WHAT: Parameters for online gradient computation
 * WHY:  True online learning without storing activations
 * HOW:  Maintain gradient estimates in real-time
 */
typedef struct {
    bool sparse_jacobian;            /**< Use sparse Jacobian approximation */
    float sparsity_threshold;        /**< Threshold for Jacobian sparsity */
    uint32_t max_jacobian_rank;      /**< Maximum rank for low-rank approximation */
} snn_rtrl_config_t;

/**
 * @brief SNN loss function configuration
 *
 * WHAT: Configuration for spike-specific loss computation
 * WHY:  Different losses for different tasks and encodings
 * HOW:  Configure target encoding and error metric
 */
typedef struct {
    snn_loss_type_t type;            /**< Loss function type */
    nimcp_loss_reduction_t reduction; /**< Reduction mode (mean/sum/none) */

    /* Spike count loss parameters */
    float target_rate;               /**< Target firing rate (Hz) */
    float rate_regularization;       /**< L2 penalty on firing rate */

    /* Timing-based loss parameters */
    float timing_precision;          /**< Temporal precision window (ms) */
    float earliest_spike_time;       /**< Earliest valid spike time (ms) */
    float latest_spike_time;         /**< Latest valid spike time (ms) */

    /* van Rossum / Victor-Purpura parameters */
    float tau_vr;                    /**< van Rossum time constant (ms) */
    float cost_vp;                   /**< Victor-Purpura spike time cost */

    /* Custom loss function */
    nimcp_loss_forward_fn custom_forward;  /**< Custom forward function */
    nimcp_loss_backward_fn custom_backward; /**< Custom backward function */
    void* custom_user_data;          /**< User data for custom loss */
} snn_loss_config_t;

/**
 * @brief Main SNN backprop trainer configuration
 *
 * WHAT: Complete configuration for SNN gradient training
 * WHY:  Unified configuration for all training components
 * HOW:  Combines algorithm, surrogate, loss, and optimization settings
 */
typedef struct {
    /* Training algorithm */
    snn_train_algorithm_t algorithm; /**< Training algorithm */
    snn_temporal_mode_t temporal_mode; /**< Temporal processing mode */

    /* Surrogate gradient */
    snn_surrogate_config_t surrogate; /**< Surrogate gradient config */

    /* Algorithm-specific configs */
    snn_bptt_config_t bptt;          /**< BPTT configuration */
    snn_eprop_config_t eprop;        /**< E-prop configuration */
    snn_rtrl_config_t rtrl;          /**< RTRL configuration */

    /* Loss function */
    snn_loss_config_t loss;          /**< Loss function configuration */

    /* Optimization */
    float learning_rate;             /**< Base learning rate */
    float weight_decay;              /**< L2 regularization coefficient */
    bool use_gradient_clipping;      /**< Enable gradient clipping */
    float gradient_clip_norm;        /**< Maximum gradient norm */

    /* Batch processing */
    uint32_t batch_size;             /**< Batch size for training */
    uint32_t sequence_length;        /**< Sequence length (timesteps) */
    bool shuffle_batches;            /**< Shuffle training data */

    /* Regularization */
    float spike_regularization;      /**< L1 penalty on spike count */
    float membrane_regularization;   /**< Penalty on membrane fluctuations */
    bool use_homeostatic;            /**< Enable homeostatic regulation */
    float target_population_rate;    /**< Target population firing rate (Hz) */

    /* Integration with NIMCP training pipeline */
    bool use_gradient_manager;       /**< Use nimcp_gradient_manager */
    nimcp_gradient_manager_config_t grad_manager_config; /**< Gradient manager config */

    /* Memory management */
    bool preallocate_buffers;        /**< Preallocate gradient buffers */
    size_t max_memory_bytes;         /**< Maximum memory for training state */

    /* Debugging */
    bool track_gradient_stats;       /**< Track gradient statistics */
    bool verbose;                    /**< Enable verbose logging */

    /* Anti-collapse */
    float diversity_loss_weight;     /**< Weight for output diversity loss (default: 0.1) */
    bool use_gradient_normalization; /**< Normalize grads to fixed norm instead of clipping */
} snn_backprop_config_t;

//=============================================================================
// Opaque Context Types
//=============================================================================

/**
 * @brief SNN backprop trainer context (opaque)
 */
typedef struct snn_backprop_ctx_s snn_backprop_ctx_t;

/**
 * @brief SNN training batch (opaque)
 */
typedef struct snn_batch_s snn_batch_t;

//=============================================================================
// Statistics and Result Structures
//=============================================================================

/**
 * @brief SNN training step result
 *
 * WHAT: Results from a single training iteration
 * WHY:  Track loss, gradients, and activity metrics
 * HOW:  Populated during backward pass
 */
typedef struct {
    float loss;                      /**< Training loss value */
    float gradient_norm;             /**< L2 norm of gradients */
    float weight_norm;               /**< L2 norm of weights */
    uint32_t total_spikes;           /**< Total spikes in forward pass */
    float mean_firing_rate;          /**< Mean firing rate (Hz) */
    float forward_time_ms;           /**< Forward pass time */
    float backward_time_ms;          /**< Backward pass time */
    uint32_t gradient_clips;         /**< Number of gradient clips applied */
    bool gradients_valid;            /**< Whether gradients are valid (no NaN/Inf) */
} snn_train_result_t;

/**
 * @brief SNN backprop training statistics
 *
 * WHAT: Cumulative training metrics
 * WHY:  Monitor training progress and convergence
 * HOW:  Updated after each training step
 */
typedef struct {
    uint64_t total_steps;            /**< Total training steps */
    uint64_t total_epochs;           /**< Total epochs completed */
    double total_loss;               /**< Cumulative loss */
    double min_loss;                 /**< Minimum loss observed */
    double max_loss;                 /**< Maximum loss observed */
    double avg_loss;                 /**< Average loss (running) */
    double loss_variance;            /**< Loss variance */

    /* Gradient statistics */
    double avg_grad_norm;            /**< Average gradient norm */
    double max_grad_norm;            /**< Maximum gradient norm */
    uint64_t gradient_explosions;    /**< Number of gradient explosions */
    uint64_t gradient_vanishing;     /**< Number of gradient vanishing events */

    /* Activity statistics */
    double avg_firing_rate;          /**< Average network firing rate */
    double avg_spikes_per_step;      /**< Average spikes per timestep */
    uint64_t silent_neurons;         /**< Neurons that never spiked */

    /* Timing */
    double total_forward_time_ms;    /**< Total forward pass time */
    double total_backward_time_ms;   /**< Total backward pass time */
    double avg_step_time_ms;         /**< Average time per step */
} snn_backprop_stats_t;

//=============================================================================
// Default Configuration Functions
//=============================================================================

/**
 * @brief Get default surrogate gradient configuration
 *
 * WHAT: Initialize surrogate config with SuperSpike defaults
 * WHY:  Convenient starting point (Zenke & Ganguli 2018)
 * HOW:  SuperSpike with beta=1.0
 *
 * @return Default surrogate configuration
 */
snn_surrogate_config_t snn_surrogate_default_config(void);

/**
 * @brief Get default BPTT configuration
 *
 * WHAT: Initialize BPTT config with reasonable defaults
 * WHY:  Standard truncated BPTT setup
 * HOW:  50-step unroll with truncation
 *
 * @param sequence_length Sequence length for training
 * @return Default BPTT configuration
 */
snn_bptt_config_t snn_bptt_default_config(uint32_t sequence_length);

/**
 * @brief Get default E-prop configuration
 *
 * WHAT: Initialize E-prop config with paper defaults
 * WHY:  Settings from Bellec et al. 2020
 * HOW:  20ms eligibility traces, symmetric e-prop
 *
 * @return Default E-prop configuration
 */
snn_eprop_config_t snn_eprop_default_config(void);

/**
 * @brief Get default SNN loss configuration
 *
 * WHAT: Initialize loss config for rate-coded training
 * WHY:  Most common SNN training scenario
 * HOW:  Rate-coded MSE with mean reduction
 *
 * @param loss_type Type of loss function
 * @return Default loss configuration
 */
snn_loss_config_t snn_loss_default_config(snn_loss_type_t loss_type);

/**
 * @brief Get default SNN backprop configuration
 *
 * WHAT: Initialize complete training config with defaults
 * WHY:  Quick setup for common use cases
 * HOW:  BPTT with SuperSpike, rate-coded MSE
 *
 * @param algorithm Training algorithm to use
 * @return Default backprop configuration
 */
snn_backprop_config_t snn_backprop_default_config(snn_train_algorithm_t algorithm);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create SNN backprop trainer
 *
 * WHAT: Initialize trainer with network and configuration
 * WHY:  Prepare for gradient-based SNN training
 * HOW:  Allocate buffers for gradients, eligibility traces, activations
 *
 * @param network SNN network to train
 * @param config Training configuration
 * @return Trainer context or NULL on failure
 *
 * COMPLEXITY: O(n_neurons × sequence_length × n_synapses)
 * MEMORY: ~8 bytes per synapse per timestep for BPTT
 */
snn_backprop_ctx_t* snn_backprop_create(
    snn_network_t* network,
    const snn_backprop_config_t* config
);

/**
 * @brief Destroy SNN backprop trainer
 *
 * WHAT: Free all trainer resources
 * WHY:  Proper cleanup
 * HOW:  Free gradient buffers, eligibility traces, statistics
 *
 * @param ctx Trainer context to destroy
 *
 * COMPLEXITY: O(n_buffers)
 */
void snn_backprop_destroy(snn_backprop_ctx_t* ctx);

/**
 * @brief Reset trainer state
 *
 * WHAT: Clear eligibility traces, gradients, statistics
 * WHY:  Start fresh training episode
 * HOW:  Zero all state variables
 *
 * @param ctx Trainer context
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(n_neurons × n_synapses)
 */
int snn_backprop_reset(snn_backprop_ctx_t* ctx);

//=============================================================================
// Surrogate Gradient Functions
//=============================================================================

/**
 * @brief Compute surrogate gradient
 *
 * WHAT: Evaluate surrogate gradient function at membrane potential
 * WHY:  Smooth approximation of spike derivative for backprop
 * HOW:  Apply configured surrogate function
 *
 * @param ctx Trainer context
 * @param membrane_v Membrane potential relative to threshold
 * @return Surrogate gradient value
 *
 * COMPLEXITY: O(1)
 */
float snn_surrogate_gradient(
    const snn_backprop_ctx_t* ctx,
    float membrane_v
);

/**
 * @brief Compute surrogate gradients for tensor
 *
 * WHAT: Vectorized surrogate gradient computation
 * WHY:  Efficient batch processing
 * HOW:  Apply surrogate function element-wise
 *
 * @param ctx Trainer context
 * @param membrane_v Membrane potential tensor [n_neurons]
 * @return Surrogate gradient tensor [n_neurons] (caller must destroy)
 *
 * COMPLEXITY: O(n_neurons)
 */
nimcp_tensor_t* snn_surrogate_gradient_tensor(
    const snn_backprop_ctx_t* ctx,
    const nimcp_tensor_t* membrane_v
);

/**
 * @brief Set surrogate gradient method
 *
 * WHAT: Change surrogate function during training
 * WHY:  Experiment with different surrogates
 * HOW:  Update configuration, recompute gradients if needed
 *
 * @param ctx Trainer context
 * @param method New surrogate method
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 */
int snn_backprop_set_surrogate(
    snn_backprop_ctx_t* ctx,
    snn_surrogate_method_t method
);

//=============================================================================
// Forward Pass with Recording
//=============================================================================

/**
 * @brief Forward pass with activation recording
 *
 * WHAT: Run SNN forward simulation while recording activations
 * WHY:  Need activation history for backward pass
 * HOW:  Simulate network, store membrane potentials, spikes, currents
 *
 * @param ctx Trainer context
 * @param inputs Input values [batch_size × n_inputs]
 * @param batch_size Number of samples in batch
 * @param duration_ms Simulation duration per sample
 * @param outputs Output buffer [batch_size × n_outputs] (optional)
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(batch_size × (duration/dt) × n_neurons × avg_synapses)
 */
int snn_backprop_forward(
    snn_backprop_ctx_t* ctx,
    const float* inputs,
    uint32_t batch_size,
    float duration_ms,
    float* outputs
);

/**
 * @brief Forward pass with tensor inputs
 *
 * WHAT: Tensor-based forward pass
 * WHY:  Integration with tensor pipeline
 * HOW:  Extract data from tensor, call scalar forward
 *
 * @param ctx Trainer context
 * @param input_tensor Input tensor [batch_size, n_inputs] or [batch, seq, n_inputs]
 * @param output_tensor Output tensor [batch_size, n_outputs] (optional, pre-allocated)
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(batch_size × sequence × n_neurons × avg_synapses)
 */
int snn_backprop_forward_tensor(
    snn_backprop_ctx_t* ctx,
    const nimcp_tensor_t* input_tensor,
    nimcp_tensor_t* output_tensor
);

//=============================================================================
// Backward Pass (Gradient Computation)
//=============================================================================

/**
 * @brief Backward pass - compute gradients
 *
 * WHAT: Backpropagate error through time to compute weight gradients
 * WHY:  Gradient descent requires gradients w.r.t. all parameters
 * HOW:  Chain rule through surrogate gradients and LIF dynamics
 *
 * ALGORITHM (BPTT):
 * 1. Compute output error from loss function
 * 2. For t = T down to 1:
 *    a. Backprop error through synapses
 *    b. Apply surrogate gradient at spike times
 *    c. Backprop through membrane dynamics (LIF)
 *    d. Accumulate weight gradients
 * 3. Apply gradient clipping if enabled
 *
 * @param ctx Trainer context
 * @param targets Target values [batch_size × n_outputs]
 * @param batch_size Number of samples in batch
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(batch_size × sequence_length × n_neurons × avg_synapses)
 */
int snn_backprop_backward(
    snn_backprop_ctx_t* ctx,
    const float* targets,
    uint32_t batch_size
);

/**
 * @brief Backward pass with tensor targets
 *
 * WHAT: Tensor-based backward pass
 * WHY:  Integration with tensor pipeline
 * HOW:  Extract targets, call scalar backward
 *
 * @param ctx Trainer context
 * @param target_tensor Target tensor [batch_size, n_outputs]
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(batch_size × sequence × n_neurons × avg_synapses)
 */
int snn_backprop_backward_tensor(
    snn_backprop_ctx_t* ctx,
    const nimcp_tensor_t* target_tensor
);

//=============================================================================
// Weight Update (Optimizer Step)
//=============================================================================

/**
 * @brief Apply gradients to weights
 *
 * WHAT: Update network weights using computed gradients
 * WHY:  Execute gradient descent step
 * HOW:  w -= learning_rate × gradient (with optional momentum/adam)
 *
 * @param ctx Trainer context
 * @param learning_rate Learning rate (uses config if 0)
 * @return Number of weights updated
 *
 * COMPLEXITY: O(n_synapses)
 */
int snn_backprop_step(
    snn_backprop_ctx_t* ctx,
    float learning_rate
);

/**
 * @brief Zero accumulated gradients
 *
 * WHAT: Clear gradient buffers
 * WHY:  Prepare for next training iteration
 * HOW:  Zero all gradient tensors
 *
 * @param ctx Trainer context
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(n_synapses)
 */
int snn_backprop_zero_grad(snn_backprop_ctx_t* ctx);

//=============================================================================
// Complete Training Step
//=============================================================================

/**
 * @brief Complete training step (forward + backward + update)
 *
 * WHAT: Single training iteration
 * WHY:  Convenience function for training loop
 * HOW:  Calls forward, backward, step in sequence
 *
 * @param ctx Trainer context
 * @param inputs Input values [batch_size × n_inputs]
 * @param targets Target values [batch_size × n_outputs]
 * @param batch_size Number of samples in batch
 * @param duration_ms Simulation duration per sample
 * @param result Training result (loss, gradients, etc.)
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(batch_size × (duration/dt) × n_neurons × avg_synapses)
 */
int snn_backprop_train_step(
    snn_backprop_ctx_t* ctx,
    const float* inputs,
    const float* targets,
    uint32_t batch_size,
    float duration_ms,
    snn_train_result_t* result
);

/**
 * @brief Training step with tensors
 *
 * WHAT: Tensor-based complete training step
 * WHY:  Cleaner API for tensor-based workflows
 * HOW:  Extract data, call scalar train_step
 *
 * @param ctx Trainer context
 * @param input_tensor Input tensor [batch, n_inputs] or [batch, seq, n_inputs]
 * @param target_tensor Target tensor [batch, n_outputs]
 * @param result Training result
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(batch × seq × n_neurons × avg_synapses)
 */
int snn_backprop_train_step_tensor(
    snn_backprop_ctx_t* ctx,
    const nimcp_tensor_t* input_tensor,
    const nimcp_tensor_t* target_tensor,
    snn_train_result_t* result
);

//=============================================================================
// Batch Processing
//=============================================================================

/**
 * @brief Create training batch
 *
 * WHAT: Prepare batch of spike data for training
 * WHY:  Batch processing for efficiency
 * HOW:  Allocate and populate batch structure
 *
 * @param inputs Input data [batch_size × n_inputs]
 * @param targets Target data [batch_size × n_outputs]
 * @param batch_size Number of samples
 * @param n_inputs Input dimension
 * @param n_outputs Output dimension
 * @return Batch handle or NULL on failure
 *
 * COMPLEXITY: O(batch_size × (n_inputs + n_outputs))
 */
snn_batch_t* snn_batch_create(
    const float* inputs,
    const float* targets,
    uint32_t batch_size,
    uint32_t n_inputs,
    uint32_t n_outputs
);

/**
 * @brief Destroy training batch
 *
 * @param batch Batch to destroy
 */
void snn_batch_destroy(snn_batch_t* batch);

/**
 * @brief Train on batch
 *
 * WHAT: Process entire batch through training pipeline
 * WHY:  Convenience function for batch training
 * HOW:  Calls train_step for each sample in batch
 *
 * @param ctx Trainer context
 * @param batch Training batch
 * @param duration_ms Simulation duration per sample
 * @param result Aggregated training result
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(batch_size × (duration/dt) × n_neurons × avg_synapses)
 */
int snn_backprop_train_batch(
    snn_backprop_ctx_t* ctx,
    const snn_batch_t* batch,
    float duration_ms,
    snn_train_result_t* result
);

//=============================================================================
// Loss Function Integration
//=============================================================================

/**
 * @brief Compute SNN loss
 *
 * WHAT: Evaluate loss function on network outputs
 * WHY:  Measure training error
 * HOW:  Apply configured loss function to spike outputs
 *
 * @param ctx Trainer context
 * @param outputs Network outputs [batch_size × n_outputs]
 * @param targets Target values [batch_size × n_outputs]
 * @param batch_size Number of samples
 * @return Loss value
 *
 * COMPLEXITY: O(batch_size × n_outputs)
 */
float snn_backprop_compute_loss(
    snn_backprop_ctx_t* ctx,
    const float* outputs,
    const float* targets,
    uint32_t batch_size
);

/**
 * @brief Compute loss gradients
 *
 * WHAT: Compute dL/dOutput for backpropagation
 * WHY:  Starting point for backward pass
 * HOW:  Differentiate loss function
 *
 * @param ctx Trainer context
 * @param outputs Network outputs [batch_size × n_outputs]
 * @param targets Target values [batch_size × n_outputs]
 * @param batch_size Number of samples
 * @param gradients Output gradient buffer [batch_size × n_outputs]
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(batch_size × n_outputs)
 */
int snn_backprop_compute_loss_grad(
    snn_backprop_ctx_t* ctx,
    const float* outputs,
    const float* targets,
    uint32_t batch_size,
    float* gradients
);

//=============================================================================
// Gradient Manager Integration
//=============================================================================

/**
 * @brief Connect to gradient manager
 *
 * WHAT: Integrate with NIMCP gradient manager
 * WHY:  Leverage gradient accumulation, scaling, clipping
 * HOW:  Create and attach gradient manager context
 *
 * @param ctx Trainer context
 * @param grad_manager Gradient manager (or NULL to create default)
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 */
int snn_backprop_connect_gradient_manager(
    snn_backprop_ctx_t* ctx,
    nimcp_gradient_manager_ctx_t* grad_manager
);

/**
 * @brief Get connected gradient manager
 *
 * @param ctx Trainer context
 * @return Gradient manager or NULL if not connected
 */
nimcp_gradient_manager_ctx_t* snn_backprop_get_gradient_manager(
    snn_backprop_ctx_t* ctx
);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get training statistics
 *
 * WHAT: Retrieve cumulative training metrics
 * WHY:  Monitor training progress
 * HOW:  Copy internal statistics to output struct
 *
 * @param ctx Trainer context
 * @param stats Output statistics structure
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 */
int snn_backprop_get_stats(
    const snn_backprop_ctx_t* ctx,
    snn_backprop_stats_t* stats
);

/**
 * @brief Reset training statistics
 *
 * WHAT: Clear all statistics counters
 * WHY:  Start fresh monitoring period
 * HOW:  Zero all statistics fields
 *
 * @param ctx Trainer context
 *
 * COMPLEXITY: O(1)
 */
void snn_backprop_reset_stats(snn_backprop_ctx_t* ctx);

/**
 * @brief Get gradient norm
 *
 * WHAT: Compute L2 norm of current gradients
 * WHY:  Monitor gradient scale and detect explosions
 * HOW:  sqrt(sum(grad²))
 *
 * @param ctx Trainer context
 * @return Gradient L2 norm
 *
 * COMPLEXITY: O(n_synapses)
 */
float snn_backprop_get_gradient_norm(const snn_backprop_ctx_t* ctx);

/**
 * @brief Get weight norm
 *
 * WHAT: Compute L2 norm of network weights
 * WHY:  Monitor weight scale and regularization
 * HOW:  sqrt(sum(w²))
 *
 * @param ctx Trainer context
 * @return Weight L2 norm
 *
 * COMPLEXITY: O(n_synapses)
 */
float snn_backprop_get_weight_norm(const snn_backprop_ctx_t* ctx);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get algorithm name
 *
 * @param algorithm Training algorithm enum
 * @return String name
 */
const char* snn_train_algorithm_name(snn_train_algorithm_t algorithm);

/**
 * @brief Get surrogate method name
 *
 * @param method Surrogate gradient method enum
 * @return String name
 */
const char* snn_surrogate_method_name(snn_surrogate_method_t method);

/**
 * @brief Get loss type name
 *
 * @param type SNN loss type enum
 * @return String name
 */
const char* snn_loss_type_name(snn_loss_type_t type);

/**
 * @brief Validate backprop configuration
 *
 * WHAT: Check configuration validity
 * WHY:  Catch errors before training
 * HOW:  Validate ranges, dependencies, compatibility
 *
 * @param config Configuration to validate
 * @return SNN_SUCCESS if valid
 *
 * COMPLEXITY: O(1)
 */
int snn_backprop_validate_config(const snn_backprop_config_t* config);

/**
 * @brief Mark context as managed by UTM (disables local anti-collapse + gradient norm)
 */
void snn_backprop_set_managed_by_utm(snn_backprop_ctx_t* ctx, bool managed);

/**
 * @brief Get the input gradient from the last backward pass
 *
 * Returns dL/d_input computed via surrogate gradient backpropagation
 * from output to input populations.
 *
 * @param ctx Backprop context
 * @param out_size Output: number of elements in returned array
 * @return Pointer to float array (owned by ctx, do NOT free), or NULL
 */
const float* snn_backprop_get_input_grad(const snn_backprop_ctx_t* ctx,
                                         uint32_t* out_size);

/**
 * @brief C2: Extract all synapse weights as a flat array for UTM param groups
 * @param ctx Backprop context
 * @param out_count Output: number of floats in returned array
 * @return Malloc'd flat weight array (caller must free), or NULL
 */
float* snn_backprop_get_flat_weights(snn_backprop_ctx_t* ctx, size_t* out_count);

/**
 * @brief C2: Write flat weights back to sparse synapses
 * @param ctx Backprop context
 * @param weights Flat weight array
 * @param count Number of weights
 * @return 0 on success
 */
int snn_backprop_set_flat_weights(snn_backprop_ctx_t* ctx, const float* weights, size_t count);

/**
 * @brief C2: Get pointer to flat weight gradients (from gradient buffer tensor)
 * @param ctx Backprop context
 * @param out_count Output: number of gradient floats
 * @return Pointer to gradient data (owned by ctx, do NOT free), or NULL
 */
float* snn_backprop_get_flat_weight_grads(snn_backprop_ctx_t* ctx, size_t* out_count);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_BACKPROP_H */
