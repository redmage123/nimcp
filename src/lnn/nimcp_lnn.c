/**
 * @file nimcp_lnn.c
 * @brief Implementation of main LNN library facade
 *
 * WHAT: Implements library lifecycle, initialization, and convenience
 *       wrappers for the LNN API.
 *
 * WHY: Provides unified entry point for LNN library, managing global
 *      state and delegating to specialized modules.
 *
 * HOW: Maintains initialized flag, coordinates thread pool setup,
 *      and forwards API calls to appropriate module implementations.
 */

#include "lnn/nimcp_lnn.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(lnn)

/* Forward declaration - defined in nimcp_lnn_gradient_part_core.c */
int lnn_network_backward(lnn_network_t* network, const nimcp_tensor_t* loss_grad);

/* Thread-safe initialization using pthread_once */
static pthread_once_t g_lnn_init_once = PTHREAD_ONCE_INIT;
static pthread_once_t g_lnn_version_once = PTHREAD_ONCE_INIT;

/* Global library state with atomic flag for thread-safe access */
static atomic_bool g_lnn_initialized = false;
static atomic_uint g_thread_count = 0;

/* Static buffer for version string, initialized once */
static char g_version_str[32];

/**
 * @brief Initialize the LNN library
 *
 * WHAT: Sets up parallel execution infrastructure and global state.
 *
 * WHY: Required before any LNN operations to enable multithreading
 *      and SIMD optimizations.
 *
 * HOW: Calls lnn_parallel_init() to create thread pool, detects CPU
 *      core count if n_threads=0, and sets initialized flag.
 *
 * THREAD SAFETY: Uses atomic operations for the initialized flag to
 *                prevent race conditions during concurrent init calls.
 */
int lnn_init(uint32_t n_threads) {
    // Guard: already initialized (atomic read)
    if (atomic_load(&g_lnn_initialized)) {
        NIMCP_LOGGING_WARN("LNN already initialized");
        return 0;
    }

    // Initialize parallel execution subsystem
    int ret = lnn_parallel_init(n_threads);
    if (ret != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize parallel subsystem");
        NIMCP_THROW_BRAIN(NIMCP_ERROR_NETWORK_CREATION, 0, "LNN",
                         "Failed to initialize LNN parallel subsystem with %u threads", n_threads);
        return ret;
    }

    // Store thread count atomically (0 means auto-detected)
    atomic_store(&g_thread_count, n_threads);

    // Set initialized flag atomically
    atomic_store(&g_lnn_initialized, true);

    NIMCP_LOGGING_INFO("LNN library initialized with %u threads",
                       n_threads == 0 ? lnn_parallel_get_num_threads() : n_threads);

    return 0;
}

/**
 * @brief Shutdown the LNN library
 *
 * WHAT: Cleans up global resources and thread pool.
 *
 * WHY: Ensures proper cleanup and allows re-initialization.
 *
 * HOW: Calls lnn_parallel_shutdown() to destroy worker threads,
 *      clears initialized flag. Safe to call multiple times.
 *
 * THREAD SAFETY: Uses atomic operations to prevent race conditions.
 */
void lnn_shutdown(void) {
    // Guard: not initialized (atomic read)
    if (!atomic_load(&g_lnn_initialized)) {
        return;
    }

    // Clear initialized flag first atomically to prevent concurrent operations
    atomic_store(&g_lnn_initialized, false);

    // Shutdown parallel subsystem
    lnn_parallel_shutdown();

    // Clear state atomically
    atomic_store(&g_thread_count, 0);

    NIMCP_LOGGING_INFO("LNN library shutdown complete");
}

/**
 * @brief One-time version string initialization
 *
 * WHAT: Initializes the global version string buffer once.
 * WHY: Thread-safe initialization via pthread_once.
 */
static void lnn_version_init_once(void) {
    snprintf(g_version_str, sizeof(g_version_str), "%d.%d.%d",
             LNN_VERSION_MAJOR, LNN_VERSION_MINOR, LNN_VERSION_PATCH);
}

/**
 * @brief Get library version string
 *
 * WHAT: Returns semantic version in "MAJOR.MINOR.PATCH" format.
 *
 * WHY: Enables runtime version checking and logging.
 *
 * HOW: Returns static string constructed from version macros.
 *
 * THREAD SAFETY: Uses pthread_once to ensure the version string
 *                is initialized exactly once, even with concurrent calls.
 */
const char* lnn_version(void) {
    pthread_once(&g_lnn_version_once, lnn_version_init_once);
    return g_version_str;
}

/**
 * @brief Check if library is initialized
 *
 * WHAT: Returns initialization status.
 *
 * WHY: Allows guard clauses before LNN operations.
 *
 * HOW: Returns global initialized flag.
 *
 * THREAD SAFETY: Uses atomic load for thread-safe read.
 */
bool lnn_is_initialized(void) {
    return atomic_load(&g_lnn_initialized);
}

/* ========================================================================
 * Forward Computation Wrappers
 * ======================================================================== */

/**
 * @brief Single forward step
 *
 * WHAT: Delegates to network module for single-step inference.
 *
 * WHY: Convenience wrapper for common real-time use case.
 *
 * HOW: Validates inputs and calls lnn_network_forward_step().
 */
int lnn_forward_step(lnn_network_t* network,
                     const nimcp_tensor_t* input,
                     nimcp_tensor_t* output,
                     float dt) {
    // Guard: null pointers
    if (!network || !input || !output) {
        NIMCP_LOGGING_ERROR("Null pointer in lnn_forward_step");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null pointer in lnn_forward_step: network=%p, input=%p, output=%p",
                             (void*)network, (void*)input, (void*)output);
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Guard: not initialized (atomic read)
    if (!atomic_load(&g_lnn_initialized)) {
        NIMCP_LOGGING_ERROR("LNN library not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                             "LNN library not initialized for forward_step");
        return NIMCP_ERROR_INVALID_STATE;
    }

    // Delegate to network module
    return lnn_network_forward_step(network, input, output, dt);
}

/**
 * @brief Forward sequence
 *
 * WHAT: Delegates to network module for temporal sequence processing.
 *
 * WHY: Convenience wrapper for time-series inference.
 *
 * HOW: Validates inputs and calls lnn_network_forward_sequence().
 */
int lnn_forward_sequence(lnn_network_t* network,
                         const nimcp_tensor_t* inputs,
                         nimcp_tensor_t* outputs,
                         uint32_t seq_len,
                         float dt) {
    // Guard: null pointers
    if (!network || !inputs || !outputs) {
        NIMCP_LOGGING_ERROR("Null pointer in lnn_forward_sequence");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null pointer in lnn_forward_sequence: network=%p, inputs=%p, outputs=%p",
                             (void*)network, (void*)inputs, (void*)outputs);
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Guard: not initialized (atomic read)
    if (!atomic_load(&g_lnn_initialized)) {
        NIMCP_LOGGING_ERROR("LNN library not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                             "LNN library not initialized for forward_sequence");
        return NIMCP_ERROR_INVALID_STATE;
    }

    // Guard: invalid sequence length
    if (seq_len == 0) {
        NIMCP_LOGGING_ERROR("Invalid sequence length: 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Invalid sequence length (0) in lnn_forward_sequence");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Delegate to network module
    return lnn_network_forward_sequence(network, inputs, outputs, seq_len, dt);
}

/**
 * @brief Forward batch
 *
 * WHAT: Delegates to network module for batched sequence processing.
 *
 * WHY: Convenience wrapper for training-style batched inference.
 *
 * HOW: Validates inputs and calls lnn_network_forward_batch().
 */
int lnn_forward_batch(lnn_network_t* network,
                      const nimcp_tensor_t* inputs,
                      nimcp_tensor_t* outputs,
                      uint32_t batch_size,
                      uint32_t seq_len,
                      float dt) {
    // Guard: null pointers
    if (!network || !inputs || !outputs) {
        NIMCP_LOGGING_ERROR("Null pointer in lnn_forward_batch");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null pointer in lnn_forward_batch: network=%p, inputs=%p, outputs=%p",
                             (void*)network, (void*)inputs, (void*)outputs);
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Guard: not initialized (atomic read)
    if (!atomic_load(&g_lnn_initialized)) {
        NIMCP_LOGGING_ERROR("LNN library not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                             "LNN library not initialized for forward_batch");
        return NIMCP_ERROR_INVALID_STATE;
    }

    // Guard: invalid dimensions
    if (batch_size == 0 || seq_len == 0) {
        NIMCP_LOGGING_ERROR("Invalid batch_size (%u) or seq_len (%u)",
                           batch_size, seq_len);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_DIMENSION_MISMATCH,
                             "Invalid batch_size (%u) or seq_len (%u) in lnn_forward_batch",
                             batch_size, seq_len);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Delegate to network module
    return lnn_network_forward_batch(network, inputs, outputs,
                                      batch_size, seq_len, dt);
}

/* ========================================================================
 * Training Wrappers
 * ======================================================================== */

/**
 * @brief Set training mode
 *
 * WHAT: Toggles training/inference mode in network.
 *
 * WHY: Training mode enables gradient tracking for backpropagation.
 *
 * HOW: Delegates to network module to set training flag.
 */
void lnn_set_training(lnn_network_t* network, bool training) {
    // Guard: null pointer
    if (!network) {
        NIMCP_LOGGING_ERROR("Null network in lnn_set_training");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null network pointer in lnn_set_training");
        return;
    }

    // Delegate to network module
    lnn_network_set_training(network, training);
}

/**
 * @brief Backward pass
 *
 * WHAT: Computes gradients via backpropagation through time.
 *
 * WHY: Required for learning - computes parameter gradients.
 *
 * HOW: Validates state and delegates to lnn_network_backward().
 */
int lnn_backward(lnn_network_t* network, const nimcp_tensor_t* loss_grad) {
    // Guard: null pointers
    if (!network || !loss_grad) {
        NIMCP_LOGGING_ERROR("Null pointer in lnn_backward");
        NIMCP_THROW_BRAIN(NIMCP_ERROR_NULL_POINTER, 0, "LNN",
                         "Null pointer in lnn_backward: network=%p, loss_grad=%p",
                         (void*)network, (void*)loss_grad);
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Guard: not initialized (atomic read)
    if (!atomic_load(&g_lnn_initialized)) {
        NIMCP_LOGGING_ERROR("LNN library not initialized");
        NIMCP_THROW_BRAIN(NIMCP_ERROR_INVALID_STATE, 0, "LNN",
                         "LNN library not initialized for backward pass");
        return NIMCP_ERROR_INVALID_STATE;
    }

    // Delegate to network module
    return lnn_network_backward(network, loss_grad);
}

/**
 * @brief Reset network state
 *
 * WHAT: Clears all neuron states to zero.
 *
 * WHY: Prevents state leakage between sequences.
 *
 * HOW: Delegates to lnn_network_reset_state().
 */
void lnn_reset_state(lnn_network_t* network) {
    // Guard: null pointer
    if (!network) {
        NIMCP_LOGGING_ERROR("Null network in lnn_reset_state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null network pointer in lnn_reset_state");
        return;
    }

    // Delegate to network module
    lnn_network_reset_state(network);
}

/**
 * @brief Reset gradients
 *
 * WHAT: Clears all parameter gradients to zero.
 *
 * WHY: Prevents gradient accumulation from prior batches.
 *
 * HOW: Delegates to lnn_network_reset_gradients().
 */
void lnn_reset_gradients(lnn_network_t* network) {
    // Guard: null pointer
    if (!network) {
        NIMCP_LOGGING_ERROR("Null network in lnn_reset_gradients");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null network pointer in lnn_reset_gradients");
        return;
    }

    // Delegate to network module
    lnn_network_reset_gradients(network);
}

/* ========================================================================
 * State Access Wrappers
 * ======================================================================== */

/**
 * @brief Get network state
 *
 * WHAT: Retrieves concatenated state vector from all neurons.
 *
 * WHY: Enables state monitoring and checkpointing.
 *
 * HOW: Delegates to lnn_network_get_state().
 */
int lnn_get_state(const lnn_network_t* network, nimcp_tensor_t** state) {
    // Guard: null pointers
    if (!network || !state) {
        NIMCP_LOGGING_ERROR("Null pointer in lnn_get_state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null pointer in lnn_get_state: network=%p, state=%p",
                             (void*)network, (void*)state);
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Delegate to network module
    return lnn_network_get_state(network, state);
}

/**
 * @brief Set network state
 *
 * WHAT: Injects external state into network neurons.
 *
 * WHY: Enables state restoration from checkpoints.
 *
 * HOW: Delegates to lnn_network_set_state().
 */
int lnn_set_state(lnn_network_t* network, const nimcp_tensor_t* state) {
    // Guard: null pointers
    if (!network || !state) {
        NIMCP_LOGGING_ERROR("Null pointer in lnn_set_state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null pointer in lnn_set_state: network=%p, state=%p",
                             (void*)network, (void*)state);
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Delegate to network module
    return lnn_network_set_state(network, state);
}

/**
 * @brief Get time constants
 *
 * WHAT: Retrieves learned tau parameters from all neurons.
 *
 * WHY: Allows inspection of learned timescales.
 *
 * HOW: Delegates to lnn_network_get_tau().
 */
int lnn_get_tau(const lnn_network_t* network, nimcp_tensor_t** tau) {
    // Guard: null pointers
    if (!network || !tau) {
        NIMCP_LOGGING_ERROR("Null pointer in lnn_get_tau");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null pointer in lnn_get_tau: network=%p, tau=%p",
                             (void*)network, (void*)tau);
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Delegate to network module
    return lnn_network_get_tau(network, tau);
}

/**
 * @brief Get network statistics
 *
 * WHAT: Retrieves runtime statistics (timing, sparsity).
 *
 * WHY: Enables performance profiling.
 *
 * HOW: Delegates to lnn_network_get_stats().
 */
int lnn_get_stats(const lnn_network_t* network, lnn_network_stats_t* stats) {
    // Guard: null pointers
    if (!network || !stats) {
        NIMCP_LOGGING_ERROR("Null pointer in lnn_get_stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null pointer in lnn_get_stats: network=%p, stats=%p",
                             (void*)network, (void*)stats);
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Delegate to network module
    return lnn_network_get_stats(network, stats);
}

/* ========================================================================
 * Integration Wrappers
 * ======================================================================== */

/**
 * @brief Connect optimizer
 *
 * WHAT: Links network gradients to external optimizer.
 *
 * WHY: Enables parameter updates after backward pass.
 *
 * HOW: Delegates to lnn_training_connect_optimizer().
 */
int lnn_connect_optimizer(lnn_network_t* network, void* optimizer) {
    // Guard: null pointers
    if (!network || !optimizer) {
        NIMCP_LOGGING_ERROR("Null pointer in lnn_connect_optimizer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null pointer in lnn_connect_optimizer: network=%p, optimizer=%p",
                             (void*)network, optimizer);
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Delegate to training module
    return lnn_training_connect_optimizer(network, optimizer);
}

/**
 * @brief Connect bio-async
 *
 * WHAT: Registers network with bio-async messaging system.
 *
 * WHY: Enables inter-module communication.
 *
 * HOW: Delegates to lnn_bio_async_connect().
 */
int lnn_connect_bio_async(lnn_network_t* network) {
    // Guard: null pointer
    if (!network) {
        NIMCP_LOGGING_ERROR("Null network in lnn_connect_bio_async");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null network pointer in lnn_connect_bio_async");
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Delegate to bio-async module with default module ID
    return lnn_bio_async_connect(network, BIO_MODULE_LNN_CORE);
}

/**
 * @brief Connect immune system
 *
 * WHAT: Links network to immune bridge for modulation.
 *
 * WHY: Allows immune system to suppress learning during fever.
 *
 * HOW: Delegates to lnn_immune_connect().
 */
int lnn_connect_immune(lnn_network_t* network, void* immune_bridge) {
    // Guard: null pointers
    if (!network || !immune_bridge) {
        NIMCP_LOGGING_ERROR("Null pointer in lnn_connect_immune");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null pointer in lnn_connect_immune: network=%p, immune_bridge=%p",
                             (void*)network, immune_bridge);
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Delegate to immune module
    return lnn_immune_connect(network, immune_bridge);
}

/* ========================================================================
 * Serialization Wrappers
 * ======================================================================== */

/**
 * @brief Save network
 *
 * WHAT: Serializes network architecture and parameters to disk.
 *
 * WHY: Enables model checkpointing.
 *
 * HOW: Delegates to lnn_network_save().
 */
int lnn_save(const lnn_network_t* network, const char* path) {
    // Guard: null pointers
    if (!network || !path) {
        NIMCP_LOGGING_ERROR("Null pointer in lnn_save");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null pointer in lnn_save: network=%p, path=%p",
                             (void*)network, (void*)path);
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Delegate to network module
    return lnn_network_save(network, path);
}

/**
 * @brief Load network
 *
 * WHAT: Deserializes network from disk.
 *
 * WHY: Enables model restoration from checkpoint.
 *
 * HOW: Delegates to lnn_network_load().
 */
lnn_network_t* lnn_load(const char* path) {
    // Guard: null pointer
    if (!path) {
        NIMCP_LOGGING_ERROR("Null path in lnn_load");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null path pointer in lnn_load");
        return NULL;
    }

    // Delegate to network module
    return lnn_network_load(path);
}
