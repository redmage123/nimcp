//=============================================================================
// nimcp_snn_config.h - SNN Configuration Management
//=============================================================================
/**
 * @file nimcp_snn_config.h
 * @brief Configuration management for Spiking Neural Networks
 *
 * WHAT: Configuration creation, validation, and preset functions
 * WHY:  Centralize SNN configuration with sensible defaults
 * HOW:  Factory functions for common architectures, validation API
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 * @version 1.0.0
 */

#ifndef NIMCP_SNN_CONFIG_H
#define NIMCP_SNN_CONFIG_H

#include "snn/nimcp_snn_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration Lifecycle
//=============================================================================

/**
 * @brief Initialize SNN configuration with default values
 *
 * WHAT: Set all configuration fields to sensible biological defaults
 * WHY:  Provide a working baseline configuration
 * HOW:  Sets LIF-based defaults, rate coding, STDP learning
 *
 * DEFAULT VALUES:
 * - dt = 0.1 ms (100 µs timestep)
 * - tau_mem = 20 ms (membrane time constant)
 * - tau_syn = 5 ms (synaptic time constant)
 * - v_thresh = -50 mV (spike threshold)
 * - v_reset = -70 mV (reset potential)
 * - v_rest = -65 mV (resting potential)
 * - t_ref = 2 ms (refractory period)
 * - train_mode = SNN_TRAIN_STDP
 * - encoding = SNN_ENCODE_POISSON
 * - decoding = SNN_DECODE_RATE
 *
 * @param config Configuration structure to initialize (must not be NULL)
 * @return SNN_SUCCESS on success, error code on failure
 *
 * COMPLEXITY: O(1)
 */
int snn_config_default(snn_config_t* config);

/**
 * @brief Create feedforward SNN configuration
 *
 * WHAT: Configure a multi-layer feedforward SNN
 * WHY:  Common architecture for classification/regression
 * HOW:  Sets layer sizes and feedforward topology
 *
 * @param config Configuration structure to initialize
 * @param n_inputs Number of input neurons
 * @param n_hidden Number of hidden neurons (single layer)
 * @param n_outputs Number of output neurons
 * @return SNN_SUCCESS on success, error code on failure
 *
 * COMPLEXITY: O(1)
 */
int snn_config_feedforward(snn_config_t* config,
                           uint32_t n_inputs,
                           uint32_t n_hidden,
                           uint32_t n_outputs);

/**
 * @brief Create multi-layer feedforward SNN configuration
 *
 * WHAT: Configure a deep SNN with multiple hidden layers
 * WHY:  More expressive networks for complex tasks
 * HOW:  Sets layer sizes from array, feedforward topology
 *
 * @param config Configuration structure to initialize
 * @param layer_sizes Array of layer sizes (input to output)
 * @param n_layers Number of layers
 * @return SNN_SUCCESS on success, error code on failure
 *
 * COMPLEXITY: O(n_layers)
 */
int snn_config_multilayer(snn_config_t* config,
                          const uint32_t* layer_sizes,
                          uint32_t n_layers);

/**
 * @brief Create reservoir SNN configuration (LSM/ESN style)
 *
 * WHAT: Configure a liquid state machine / echo state network
 * WHY:  Powerful for temporal processing, minimal training
 * HOW:  Random recurrent reservoir with trained readout
 *
 * BIOLOGICAL BASIS:
 * - Reservoir mimics cortical microcircuits
 * - Rich dynamics from recurrent connections
 * - Only readout layer is trained
 *
 * @param config Configuration structure to initialize
 * @param n_inputs Number of input neurons
 * @param n_reservoir Number of reservoir neurons
 * @param n_outputs Number of output neurons
 * @param connectivity Reservoir connectivity ratio [0, 1]
 * @return SNN_SUCCESS on success, error code on failure
 *
 * COMPLEXITY: O(1)
 */
int snn_config_reservoir(snn_config_t* config,
                         uint32_t n_inputs,
                         uint32_t n_reservoir,
                         uint32_t n_outputs,
                         float connectivity);

/**
 * @brief Create cortical column SNN configuration
 *
 * WHAT: Configure a biologically-realistic cortical column
 * WHY:  Model cortical computation with 6-layer structure
 * HOW:  Populations for L2/3, L4, L5, L6 with proper connectivity
 *
 * BIOLOGICAL BASIS:
 * - L4: Primary input (thalamic)
 * - L2/3: Association, lateral connections
 * - L5: Primary output (subcortical)
 * - L6: Feedback to thalamus
 *
 * @param config Configuration structure to initialize
 * @param n_minicolumns Number of minicolumns
 * @param neurons_per_minicolumn Neurons per minicolumn per layer
 * @return SNN_SUCCESS on success, error code on failure
 *
 * COMPLEXITY: O(1)
 */
int snn_config_cortical_column(snn_config_t* config,
                               uint32_t n_minicolumns,
                               uint32_t neurons_per_minicolumn);

/**
 * @brief Validate SNN configuration
 *
 * WHAT: Check configuration for consistency and valid ranges
 * WHY:  Catch configuration errors before network creation
 * HOW:  Validate dimensions, time constants, learning parameters
 *
 * VALIDATION CHECKS:
 * - n_inputs > 0, n_outputs > 0
 * - dt in [SNN_DT_MIN, SNN_DT_MAX]
 * - tau_mem > 0, tau_syn > 0
 * - v_thresh > v_reset
 * - learning_rate >= 0
 * - surrogate_beta > 0
 *
 * @param config Configuration to validate
 * @return SNN_SUCCESS if valid, error code describing first issue
 *
 * COMPLEXITY: O(1)
 */
int snn_config_validate(const snn_config_t* config);

/**
 * @brief Destroy configuration and free resources
 *
 * WHAT: Clean up any dynamically allocated config resources
 * WHY:  Prevent memory leaks
 * HOW:  Free layer_sizes array if allocated, zero structure
 *
 * @param config Configuration to destroy (can be NULL)
 *
 * COMPLEXITY: O(1)
 */
void snn_config_destroy(snn_config_t* config);

//=============================================================================
// Encoder Configuration
//=============================================================================

/**
 * @brief Set rate coding encoder configuration
 *
 * WHAT: Configure rate-based spike encoding
 * WHY:  Simple, robust encoding for most applications
 * HOW:  Input value maps to firing rate
 *
 * @param config Main SNN configuration
 * @param max_rate Maximum firing rate (Hz)
 * @param time_window Encoding window (ms)
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 */
int snn_config_encoder_rate(snn_config_t* config,
                            float max_rate,
                            float time_window);

/**
 * @brief Set population coding encoder configuration
 *
 * WHAT: Configure population-based spike encoding
 * WHY:  Robust encoding with overlapping tuning curves
 * HOW:  Each value activates a subset of neurons
 *
 * @param config Main SNN configuration
 * @param population_size Neurons per encoded value
 * @param sigma Tuning curve width
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 */
int snn_config_encoder_population(snn_config_t* config,
                                  uint32_t population_size,
                                  float sigma);

/**
 * @brief Set latency coding encoder configuration
 *
 * WHAT: Configure latency-based spike encoding
 * WHY:  Energy-efficient (one spike per neuron)
 * HOW:  Higher values → earlier spikes
 *
 * @param config Main SNN configuration
 * @param max_latency Maximum time to first spike (ms)
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 */
int snn_config_encoder_latency(snn_config_t* config,
                               float max_latency);

//=============================================================================
// Training Configuration
//=============================================================================

/**
 * @brief Configure STDP training (uses existing synapse_t infrastructure)
 *
 * WHAT: Enable spike-timing dependent plasticity
 * WHY:  Biological, local learning rule
 * HOW:  Leverages existing stdp_params in synapse_t
 *
 * @param config Main SNN configuration
 * @param learning_rate STDP learning rate
 * @param time_window STDP time window (ms)
 * @param a_plus LTP amplitude
 * @param a_minus LTD amplitude
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 */
int snn_config_train_stdp(snn_config_t* config,
                          float learning_rate,
                          float time_window,
                          float a_plus,
                          float a_minus);

/**
 * @brief Configure reward-modulated STDP training
 *
 * WHAT: Enable R-STDP for reinforcement learning
 * WHY:  Combines STDP with reward signal for goal-directed learning
 * HOW:  Eligibility traces modulated by reward
 *
 * @param config Main SNN configuration
 * @param learning_rate Base learning rate
 * @param eligibility_decay Eligibility trace decay rate
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 */
int snn_config_train_rstdp(snn_config_t* config,
                           float learning_rate,
                           float eligibility_decay);

/**
 * @brief Configure surrogate gradient training
 *
 * WHAT: Enable backpropagation with surrogate gradients
 * WHY:  Most accurate training, but less biologically plausible
 * HOW:  Smooth approximation of spike function derivative
 *
 * @param config Main SNN configuration
 * @param surrogate Surrogate gradient function
 * @param beta Surrogate gradient sharpness
 * @param learning_rate Learning rate
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 */
int snn_config_train_surrogate(snn_config_t* config,
                               snn_surrogate_t surrogate,
                               float beta,
                               float learning_rate);

/**
 * @brief Configure e-prop training
 *
 * WHAT: Enable eligibility propagation
 * WHY:  Bio-plausible alternative to backprop
 * HOW:  Forward eligibility traces with learning signals
 *
 * REFERENCE: Bellec et al. (2020) "e-prop"
 *
 * @param config Main SNN configuration
 * @param learning_rate Learning rate
 * @param eligibility_decay Eligibility trace decay
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 */
int snn_config_train_eprop(snn_config_t* config,
                           float learning_rate,
                           float eligibility_decay);

//=============================================================================
// Integration Configuration
//=============================================================================

/**
 * @brief Enable bio-async integration
 *
 * WHAT: Configure SNN for bio-async messaging
 * WHY:  Inter-module spike event communication
 * HOW:  Registers with bio-router on network creation
 *
 * @param config Main SNN configuration
 * @param enable true to enable, false to disable
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 */
int snn_config_enable_bio_async(snn_config_t* config, bool enable);

/**
 * @brief Enable immune system integration
 *
 * WHAT: Configure SNN for brain immune integration
 * WHY:  Cytokine effects on excitability and plasticity
 * HOW:  Creates immune bridge on network creation
 *
 * @param config Main SNN configuration
 * @param enable true to enable, false to disable
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 */
int snn_config_enable_immune(snn_config_t* config, bool enable);

/**
 * @brief Enable axon infrastructure for spike delays
 *
 * WHAT: Use existing axon_t for realistic conduction delays
 * WHY:  Biologically accurate spike propagation timing
 * HOW:  Integrates with core/axon module
 *
 * @param config Main SNN configuration
 * @param enable true to enable, false to disable
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 */
int snn_config_enable_axon_delays(snn_config_t* config, bool enable);

/**
 * @brief Enable dendritic integration
 *
 * WHAT: Use existing dendrite_t for spatial integration
 * WHY:  Biologically accurate input integration
 * HOW:  Integrates with core/dendrite module
 *
 * @param config Main SNN configuration
 * @param enable true to enable, false to disable
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 */
int snn_config_enable_dendrites(snn_config_t* config, bool enable);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Print configuration summary
 *
 * WHAT: Output configuration details for debugging
 * WHY:  Verify configuration before expensive operations
 * HOW:  Formats and prints to provided buffer
 *
 * @param config Configuration to print
 * @param buffer Output buffer
 * @param buffer_size Buffer size in bytes
 * @return Number of characters written, or required size if buffer too small
 *
 * COMPLEXITY: O(1)
 */
int snn_config_print(const snn_config_t* config,
                     char* buffer,
                     size_t buffer_size);

/**
 * @brief Clone configuration
 *
 * WHAT: Deep copy of configuration
 * WHY:  Create independent copy for modification
 * HOW:  Copy all fields, duplicate any allocated arrays
 *
 * @param src Source configuration
 * @param dst Destination configuration
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(n_layers) if layer_sizes allocated
 */
int snn_config_clone(const snn_config_t* src, snn_config_t* dst);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_CONFIG_H */
