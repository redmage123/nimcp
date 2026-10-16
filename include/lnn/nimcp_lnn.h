/**
 * @file nimcp_lnn.h
 * @brief Main LNN (Liquid Neural Network) library facade
 *
 * WHAT: Unified API for the LNN library, providing a single include and
 *       convenience wrappers for all LNN functionality.
 *
 * WHY: Simplifies user interaction by exposing a cohesive interface rather
 *      than requiring multiple includes and module-specific APIs. Manages
 *      library lifecycle (init/shutdown) and global state.
 *
 * HOW: Includes all LNN headers and provides:
 *      - Library initialization/shutdown (thread pool, bio-async, SIMD)
 *      - Forward/backward computation wrappers
 *      - State access and manipulation
 *      - Integration with optimizer/bio-async/immune system
 *      - Serialization helpers
 *
 * Biological basis: Acts as the "central executive" coordinating all LNN
 * subsystems, analogous to prefrontal cortex orchestrating distributed
 * brain regions.
 */

#ifndef NIMCP_LNN_H
#define NIMCP_LNN_H

/*
 * Include all LNN module headers OUTSIDE of extern "C" block.
 * Each header has its own extern "C" guards as needed.
 * This prevents issues with C++ templates in CUDA headers.
 */
#include "lnn/nimcp_lnn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "lnn/nimcp_lnn_config.h"
#include "lnn/nimcp_lnn_neuron.h"
#include "lnn/nimcp_lnn_layer.h"
#include "lnn/nimcp_lnn_network.h"
#include "lnn/nimcp_lnn_ode.h"
#include "lnn/nimcp_lnn_wiring.h"
#include "lnn/nimcp_lnn_gradient.h"
#include "lnn/nimcp_lnn_training.h"
#include "lnn/nimcp_lnn_bio_async.h"
#include "lnn/nimcp_lnn_immune.h"
#include "lnn/nimcp_lnn_parallel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Library version */
#define LNN_VERSION_MAJOR 1
#define LNN_VERSION_MINOR 0
#define LNN_VERSION_PATCH 0

/**
 * @brief Initialize the LNN library
 *
 * WHAT: Initializes global library state, thread pool, SIMD detection,
 *       and bio-async registration.
 *
 * WHY: Required before any LNN operations to set up parallel execution
 *      infrastructure and detect hardware capabilities.
 *
 * HOW: Calls lnn_parallel_init() to create thread pool with n_threads,
 *      detects available SIMD extensions, and registers bio-async module.
 *      Sets global initialized flag.
 *
 * @param n_threads Number of worker threads (0 = auto-detect CPU cores)
 * @return 0 on success, negative error code on failure
 */
int lnn_init(uint32_t n_threads);

/**
 * @brief Shutdown the LNN library
 *
 * WHAT: Cleans up global resources, destroys thread pool, and resets state.
 *
 * WHY: Ensures proper resource cleanup and allows re-initialization.
 *
 * HOW: Calls lnn_parallel_shutdown() to destroy thread pool and worker
 *      threads. Clears initialized flag. Safe to call multiple times.
 */
void lnn_shutdown(void);

/**
 * @brief Get library version string
 *
 * WHAT: Returns semantic version string for the LNN library.
 *
 * WHY: Allows runtime version checking and logging.
 *
 * HOW: Returns static string "MAJOR.MINOR.PATCH" format.
 *
 * @return Version string (e.g., "1.0.0")
 */
const char* lnn_version(void);

/**
 * @brief Check if library is initialized
 *
 * WHAT: Returns whether lnn_init() has been successfully called.
 *
 * WHY: Allows safe guards before LNN operations.
 *
 * HOW: Checks global initialized flag set by lnn_init().
 *
 * @return true if initialized, false otherwise
 */
bool lnn_is_initialized(void);

/* ========================================================================
 * Forward Computation Wrappers
 * ======================================================================== */

/**
 * @brief Single forward step through network
 *
 * WHAT: Processes one input vector through network for one timestep.
 *
 * WHY: Convenience wrapper for single-step inference (e.g., real-time
 *      sensory processing).
 *
 * HOW: Delegates to lnn_network_forward_step(). Input should be
 *      [input_size], output will be [output_size].
 *
 * @param network Network to process
 * @param input Input tensor [input_size]
 * @param output Output tensor [output_size] (pre-allocated)
 * @param dt Timestep in seconds (typically 0.001-0.1)
 * @return 0 on success, negative error code on failure
 */
int lnn_forward_step(lnn_network_t* network,
                     const nimcp_tensor_t* input,
                     nimcp_tensor_t* output,
                     float dt);

/**
 * @brief Forward pass for sequence
 *
 * WHAT: Processes temporal sequence through network.
 *
 * WHY: Common pattern for time-series processing (speech, sensor streams).
 *
 * HOW: Delegates to lnn_network_forward_sequence(). Inputs should be
 *      [seq_len, input_size], outputs will be [seq_len, output_size].
 *
 * @param network Network to process
 * @param inputs Input sequence [seq_len, input_size]
 * @param outputs Output sequence [seq_len, output_size] (pre-allocated)
 * @param seq_len Sequence length
 * @param dt Timestep in seconds
 * @return 0 on success, negative error code on failure
 */
int lnn_forward_sequence(lnn_network_t* network,
                         const nimcp_tensor_t* inputs,
                         nimcp_tensor_t* outputs,
                         uint32_t seq_len,
                         float dt);

/**
 * @brief Forward pass for batched sequences
 *
 * WHAT: Processes multiple sequences in parallel.
 *
 * WHY: Training typically requires batched data for efficiency.
 *
 * HOW: Delegates to lnn_network_forward_batch(). Inputs should be
 *      [batch_size, seq_len, input_size], outputs will be
 *      [batch_size, seq_len, output_size].
 *
 * @param network Network to process
 * @param inputs Batched sequences [batch_size, seq_len, input_size]
 * @param outputs Batched outputs [batch_size, seq_len, output_size]
 * @param batch_size Number of sequences
 * @param seq_len Length of each sequence
 * @param dt Timestep in seconds
 * @return 0 on success, negative error code on failure
 */
int lnn_forward_batch(lnn_network_t* network,
                      const nimcp_tensor_t* inputs,
                      nimcp_tensor_t* outputs,
                      uint32_t batch_size,
                      uint32_t seq_len,
                      float dt);

/* ========================================================================
 * Training Wrappers
 * ======================================================================== */

/**
 * @brief Set training mode
 *
 * WHAT: Toggles between training and inference mode.
 *
 * WHY: Training mode enables gradient computation and parameter updates.
 *
 * HOW: Sets network->training flag. When true, forward pass stores
 *      activations for backpropagation.
 *
 * @param network Network to configure
 * @param training true for training mode, false for inference
 */
void lnn_set_training(lnn_network_t* network, bool training);

/**
 * @brief Backward pass to compute gradients
 *
 * WHAT: Backpropagates error gradients through network.
 *
 * WHY: Required for learning - computes parameter gradients from loss.
 *
 * HOW: Delegates to lnn_network_backward(). Requires network in training
 *      mode and prior forward pass. Computes gradients for all learnable
 *      parameters (tau, A, sensory/motor weights).
 *
 * @param network Network to backpropagate through
 * @param loss_grad Gradient of loss w.r.t. network output
 * @return 0 on success, negative error code on failure
 */
int lnn_backward(lnn_network_t* network, const nimcp_tensor_t* loss_grad);

/**
 * @brief Reset network state
 *
 * WHAT: Clears all neuron states (v, h) to zero.
 *
 * WHY: Required between sequences to prevent state leakage.
 *
 * HOW: Delegates to lnn_network_reset_state(). Zeros all state vectors
 *      in all layers.
 *
 * @param network Network to reset
 */
void lnn_reset_state(lnn_network_t* network);

/**
 * @brief Reset accumulated gradients
 *
 * WHAT: Clears all parameter gradients to zero.
 *
 * WHY: Required before each backward pass to avoid accumulating stale
 *      gradients from previous batches.
 *
 * HOW: Delegates to lnn_network_reset_gradients(). Zeros gradients for
 *      tau, A, and all weight matrices.
 *
 * @param network Network to reset
 */
void lnn_reset_gradients(lnn_network_t* network);

/* ========================================================================
 * State Access Wrappers
 * ======================================================================== */

/**
 * @brief Get network state vector
 *
 * WHAT: Retrieves concatenated state (v, h) from all neurons.
 *
 * WHY: Allows external state monitoring, visualization, or checkpointing.
 *
 * HOW: Delegates to lnn_network_get_state(). Concatenates states from all
 *      layers into single tensor [total_state_size].
 *
 * @param network Network to query
 * @param state Output state tensor (pre-allocated to network->total_state_size)
 * @return 0 on success, negative error code on failure
 */
int lnn_get_state(const lnn_network_t* network, nimcp_tensor_t** state);

/**
 * @brief Set network state vector
 *
 * WHAT: Injects external state into all neurons.
 *
 * WHY: Enables state restoration from checkpoints or warm-start from
 *      prior computation.
 *
 * HOW: Delegates to lnn_network_set_state(). Distributes state values
 *      across all layers.
 *
 * @param network Network to modify
 * @param state State tensor [total_state_size]
 * @return 0 on success, negative error code on failure
 */
int lnn_set_state(lnn_network_t* network, const nimcp_tensor_t* state);

/**
 * @brief Get time constants (tau)
 *
 * WHAT: Retrieves learned tau parameters from all neurons.
 *
 * WHY: Allows inspection of learned timescales (fast vs slow neurons).
 *
 * HOW: Delegates to lnn_network_get_tau(). Concatenates tau from all
 *      layers into single tensor [total_neurons].
 *
 * @param network Network to query
 * @param tau Output tau tensor (pre-allocated to total_neurons)
 * @return 0 on success, negative error code on failure
 */
int lnn_get_tau(const lnn_network_t* network, nimcp_tensor_t** tau);

/**
 * @brief Get network statistics
 *
 * WHAT: Retrieves runtime statistics (forward/backward time, sparsity).
 *
 * WHY: Enables performance profiling and monitoring.
 *
 * HOW: Delegates to lnn_network_get_stats(). Fills stats structure with
 *      timing and activity metrics.
 *
 * @param network Network to query
 * @param stats Output statistics structure
 * @return 0 on success, negative error code on failure
 */
int lnn_get_stats(const lnn_network_t* network, lnn_network_stats_t* stats);

/* ========================================================================
 * Integration Wrappers
 * ======================================================================== */

/**
 * @brief Connect network to optimizer
 *
 * WHAT: Links network gradients to external optimizer (Adam, SGD, etc.).
 *
 * WHY: Allows parameter updates after backward pass.
 *
 * HOW: Delegates to lnn_training_connect_optimizer(). Optimizer will
 *      update network parameters based on computed gradients.
 *
 * @param network Network to connect
 * @param optimizer Optimizer instance (nimcp_optimizer_t*)
 * @return 0 on success, negative error code on failure
 */
int lnn_connect_optimizer(lnn_network_t* network, void* optimizer);

/**
 * @brief Connect network to bio-async router
 *
 * WHAT: Registers network with bio-async messaging system.
 *
 * WHY: Enables inter-module communication (e.g., with immune system).
 *
 * HOW: Delegates to lnn_bio_async_connect(). Registers inbox/outbox
 *      for asynchronous messages.
 *
 * @param network Network to connect
 * @return 0 on success, negative error code on failure
 */
int lnn_connect_bio_async(lnn_network_t* network);

/**
 * @brief Connect network to immune system
 *
 * WHAT: Links network to brain immune bridge for modulation.
 *
 * WHY: Allows immune system to modulate learning (fever suppression).
 *
 * HOW: Delegates to lnn_immune_connect(). Immune bridge can adjust
 *      learning rates based on inflammation.
 *
 * @param network Network to connect
 * @param immune_bridge Immune bridge instance (lnn_immune_bridge_t*)
 * @return 0 on success, negative error code on failure
 */
int lnn_connect_immune(lnn_network_t* network, void* immune_bridge);

/* ========================================================================
 * Serialization Wrappers
 * ======================================================================== */

/**
 * @brief Save network to file
 *
 * WHAT: Serializes network architecture and parameters to disk.
 *
 * WHY: Enables model checkpointing and deployment.
 *
 * HOW: Delegates to lnn_network_save(). Writes config, wiring,
 *      parameters (tau, A, weights) in binary format.
 *
 * @param network Network to save
 * @param path Output file path
 * @return 0 on success, negative error code on failure
 */
int lnn_save(const lnn_network_t* network, const char* path);

/**
 * @brief Load network from file
 *
 * WHAT: Deserializes network from disk.
 *
 * WHY: Enables model restoration from checkpoint.
 *
 * HOW: Delegates to lnn_network_load(). Reads config, creates network,
 *      loads parameters. Caller must call lnn_network_destroy() when done.
 *
 * @param path Input file path
 * @return Loaded network, or NULL on failure
 */
lnn_network_t* lnn_load(const char* path);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_H */
