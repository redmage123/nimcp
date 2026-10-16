/**
 * @file nimcp_weights.h
 * @brief Immutable network weights separated from mutable statistics
 * @version 2.7.0
 * @date 2025-11-04
 *
 * Enhancement 1: Separate Weights from Statistics
 *
 * WHAT: Split adaptive_network into immutable weights (shared) and mutable stats (private)
 * WHY: Enable indefinite COW sharing even during training
 * HOW: Reference-counted weights, per-brain statistics
 *
 * Benefits:
 * - 95%+ memory savings for shared training scenarios
 * - Each brain gets private statistics
 * - Cleaner separation of concerns
 */

#ifndef NIMCP_WEIGHTS_H
#define NIMCP_WEIGHTS_H

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include "common/nimcp_export.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "plasticity/adaptive/nimcp_adaptive.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Immutable Network Weights (Shared)
//=============================================================================

/**
 * @brief Immutable network weights shared across COW clones
 *
 * Weights are reference-counted and never modified after initial training.
 * Multiple brains can safely share the same weights indefinitely.
 */
typedef struct network_weights_struct {
    neural_network_t base_network;        ///< Neural network with weights
    adaptive_spike_params_t spike_params; ///< Spike encoding configuration

    // Reference counting for COW
    uint32_t* refcount;                   ///< Number of networks sharing weights
    pthread_mutex_t* refcount_mutex;      ///< Thread-safe refcount updates

    // Metadata
    uint32_t num_neurons;                 ///< Total neurons in network
    uint32_t num_synapses;                ///< Total synaptic connections
    size_t memory_bytes;                  ///< Total memory footprint

    // Pruning configuration
    float pruning_threshold;              ///< Threshold for pruning weak synapses
} network_weights_t;

/**
 * @brief Create network weights
 *
 * @param base_network Neural network with initialized weights
 * @param spike_params Spike encoding parameters
 * @return Weights handle or NULL on error
 */
NIMCP_EXPORT network_weights_t* network_weights_create(
    neural_network_t base_network,
    const adaptive_spike_params_t* spike_params
);

/**
 * @brief Increment reference count (for COW sharing)
 *
 * Thread-safe operation.
 *
 * @param weights Network weights
 * @return New reference count
 */
NIMCP_EXPORT uint32_t network_weights_retain(network_weights_t* weights);

/**
 * @brief Decrement reference count (for COW cleanup)
 *
 * Destroys weights when refcount reaches 0.
 * Thread-safe operation.
 *
 * @param weights Network weights
 * @return Remaining reference count (0 if destroyed)
 */
NIMCP_EXPORT uint32_t network_weights_release(network_weights_t* weights);

/**
 * @brief Get current reference count
 *
 * @param weights Network weights
 * @return Current reference count
 */
NIMCP_EXPORT uint32_t network_weights_get_refcount(const network_weights_t* weights);

/**
 * @brief Clone weights (creates deep copy)
 *
 * Used when COW trigger requires private copy.
 *
 * @param weights Source weights
 * @return New weights with refcount=1, or NULL on error
 */
NIMCP_EXPORT network_weights_t* network_weights_clone(const network_weights_t* weights);

//=============================================================================
// Mutable Network Statistics (Private per brain)
//=============================================================================

/**
 * @brief Mutable network statistics private to each brain
 *
 * Statistics track runtime behavior and are updated during inference/learning.
 * Each brain has its own statistics even when sharing weights.
 */
typedef struct network_stats_struct {
    // Inference statistics
    float running_sparsity;               ///< Running average sparsity (0-1)
    uint64_t total_inferences;            ///< Total forward passes
    float avg_inference_time_us;          ///< Average inference time (microseconds)
    uint32_t active_neuron_count;         ///< Recent active neuron count

    // Learning statistics
    uint64_t total_learning_steps;        ///< Total learning updates
    float avg_learning_time_us;           ///< Average learning time (microseconds)
    float current_loss;                   ///< Current training loss
    float accuracy;                       ///< Current accuracy (validation)

    // Adaptive threshold tracking
    float avg_threshold;                  ///< Average adaptive threshold
    float min_threshold;                  ///< Minimum threshold seen
    float max_threshold;                  ///< Maximum threshold seen

    // Memory statistics
    size_t memory_bytes;                  ///< Memory used by statistics
} nimcp_network_stats_t;

/**
 * @brief Create network statistics
 *
 * @return Statistics handle or NULL on error
 */
NIMCP_EXPORT nimcp_network_stats_t* network_stats_create(void);

/**
 * @brief Destroy network statistics
 *
 * @param stats Statistics to destroy
 */
NIMCP_EXPORT void network_stats_destroy(nimcp_network_stats_t* stats);

/**
 * @brief Reset statistics to initial state
 *
 * @param stats Statistics to reset
 */
NIMCP_EXPORT void network_stats_reset(nimcp_network_stats_t* stats);

/**
 * @brief Update running sparsity statistic
 *
 * @param stats Network statistics
 * @param active_count Number of active neurons
 * @param total_count Total number of neurons
 */
NIMCP_EXPORT void network_stats_update_sparsity(
    nimcp_network_stats_t* stats,
    uint32_t active_count,
    uint32_t total_count
);

/**
 * @brief Record inference timing
 *
 * @param stats Network statistics
 * @param time_us Inference time in microseconds
 */
NIMCP_EXPORT void network_stats_record_inference(
    nimcp_network_stats_t* stats,
    float time_us
);

/**
 * @brief Record learning timing and loss
 *
 * @param stats Network statistics
 * @param time_us Learning time in microseconds
 * @param loss Training loss value
 */
NIMCP_EXPORT void network_stats_record_learning(
    nimcp_network_stats_t* stats,
    float time_us,
    float loss
);

//=============================================================================
// Integrated Adaptive Network (Weights + Stats)
//=============================================================================

/**
 * @brief Check if network owns its weights
 *
 * @param network Adaptive network
 * @return true if network owns weights (not COW sharing)
 */
NIMCP_EXPORT bool adaptive_network_owns_weights(const adaptive_network_t network);

/**
 * @brief Get weights from adaptive network
 *
 * WARNING: Internal use only. Modifying weights breaks COW semantics.
 *
 * @param network Adaptive network
 * @return Weights handle (do not modify!)
 */
NIMCP_EXPORT const network_weights_t* adaptive_network_get_weights(
    const adaptive_network_t network
);

/**
 * @brief Get statistics from adaptive network
 *
 * @param network Adaptive network
 * @return Statistics handle (can be modified)
 */
NIMCP_EXPORT nimcp_network_stats_t* adaptive_network_get_stats(
    const adaptive_network_t network
);

/**
 * @brief Trigger COW on weights (create private copy)
 *
 * Clones shared weights so this network can modify them independently.
 *
 * @param network Adaptive network
 * @return true on success, false on error
 */
NIMCP_EXPORT bool adaptive_network_trigger_weight_cow(adaptive_network_t network);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_WEIGHTS_H
