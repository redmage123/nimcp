/**
 * @file nimcp_spike_event.h
 * @brief Spike Event Message Protocol for P2P Neuron Communication
 *
 * WHAT: Defines spike events as discrete messages for neuron-to-neuron communication
 * WHY:  Biological neurons communicate via action potentials (spikes), not shared memory
 * HOW:  Timestamped spike events with source/target routing information
 *
 * ARCHITECTURE:
 * This implements true biological message-passing between neurons:
 * - Each spike is an independent event (like neurotransmitter release)
 * - No direct state access between neurons
 * - Timestamped for temporal precision
 * - Supports both CPU queues and GPU shared memory
 *
 * DESIGN PATTERNS:
 * - Message Pattern: Spike as immutable message
 * - Event-Driven: Neurons react to incoming spike events
 * - Queue-Based: FIFO spike delivery
 *
 * BIOLOGICAL MAPPING:
 * - spike_event_t → Action potential
 * - timestamp → Spike timing (ms precision)
 * - source_id → Presynaptic neuron
 * - target_id → Postsynaptic neuron
 * - amplitude → Spike magnitude (usually 1.0 for all-or-nothing)
 * - synapse_id → Specific synapse on target neuron
 *
 * PERFORMANCE:
 * - Event size: 24 bytes (cache-friendly)
 * - GPU-compatible: Aligned for coalesced memory access
 * - Lock-free queues: Atomic operations for thread safety
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.6 (GPU P2P)
 */

#ifndef NIMCP_SPIKE_EVENT_H
#define NIMCP_SPIKE_EVENT_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Spike Event Types
//=============================================================================

/**
 * @brief Spike event types for different signaling modes
 */
typedef enum {
    SPIKE_TYPE_EXCITATORY,   /**< Excitatory spike (EPSP) */
    SPIKE_TYPE_INHIBITORY,   /**< Inhibitory spike (IPSP) */
    SPIKE_TYPE_MODULATORY,   /**< Neuromodulatory signal */
    SPIKE_TYPE_BACKPROP,     /**< Backpropagating action potential */
    SPIKE_TYPE_CALCIUM,      /**< Calcium spike (dendritic) */
    SPIKE_TYPE_BURST,        /**< Burst spike (multiple rapid spikes) */
} spike_type_t;

/**
 * @brief Spike event message structure
 *
 * LAYOUT: 24 bytes, aligned for GPU coalesced access
 * INVARIANTS:
 * - timestamp > 0 (must be valid time)
 * - source_id != target_id (no self-spikes, unless autapse)
 * - 0.0 <= amplitude <= 10.0 (biological range)
 */
typedef struct {
    uint64_t timestamp;      /**< Spike time (microseconds) - 8 bytes */
    uint32_t source_id;      /**< Source neuron ID - 4 bytes */
    uint32_t target_id;      /**< Target neuron ID - 4 bytes */
    uint32_t synapse_id;     /**< Specific synapse index - 4 bytes */
    float amplitude;         /**< Spike amplitude (usually 1.0) - 4 bytes */
} spike_event_t;

/**
 * @brief Temporal spike train - sequence of spike events
 *
 * WHAT: Represents complete spike history for a neuron
 * WHY:  STDP and temporal coding require precise spike timing
 * HOW:  Circular buffer of spike events with timestamps
 *
 * USAGE:
 * - STDP: Compare pre/post spike times
 * - Temporal coding: Pattern recognition in spike sequences
 * - Burst detection: Identify rapid spike clusters
 *
 * NOTE: Opaque pointer - implementation details hidden in .c file
 */
typedef struct spike_train_struct spike_train_t;

//=============================================================================
// Spike Event Queue (Lock-Free for GPU)
//=============================================================================

/**
 * @brief Lock-free spike event queue for CPU/GPU communication
 *
 * DESIGN: Multiple-producer, multiple-consumer queue
 * THREAD SAFETY: Lock-free atomic operations
 * GPU COMPATIBLE: Can be accessed from CUDA kernels
 *
 * NOTE: Opaque pointer - implementation details hidden in .c file
 */
typedef struct spike_queue_struct spike_queue_t;

//=============================================================================
// Spike Train Operations
//=============================================================================

/**
 * @brief Create a spike train buffer
 *
 * @param capacity Maximum number of spikes to store
 * @return Spike train handle, or NULL on failure
 */
NIMCP_EXPORT spike_train_t* spike_train_create(uint32_t capacity);

/**
 * @brief Destroy spike train and free resources
 *
 * @param train Spike train handle (NULL-safe)
 */
NIMCP_EXPORT void spike_train_destroy(spike_train_t* train);

/**
 * @brief Add spike event to train
 *
 * @param train Spike train
 * @param timestamp Spike time (microseconds)
 * @param amplitude Spike amplitude
 * @return true on success, false if buffer full
 */
NIMCP_EXPORT bool spike_train_add(spike_train_t* train, uint64_t timestamp, float amplitude);

/**
 * @brief Get most recent spike time
 *
 * @param train Spike train
 * @return Last spike timestamp, or 0 if no spikes
 */
NIMCP_EXPORT uint64_t spike_train_get_last_spike(const spike_train_t* train);

/**
 * @brief Get spike at specific index
 *
 * @param train Spike train
 * @param index Spike index (0 = oldest, count-1 = newest)
 * @param event Output spike event
 * @return true if spike exists, false otherwise
 */
NIMCP_EXPORT bool spike_train_get_spike(const spike_train_t* train, uint32_t index,
                                        spike_event_t* event);

/**
 * @brief Compute instantaneous firing rate
 *
 * @param train Spike train
 * @param time_window Window duration (microseconds)
 * @return Firing rate in Hz
 */
NIMCP_EXPORT float spike_train_compute_rate(spike_train_t* train, uint64_t time_window);

/**
 * @brief Clear all spikes from train
 *
 * @param train Spike train
 */
NIMCP_EXPORT void spike_train_clear(spike_train_t* train);

//=============================================================================
// Spike Queue Operations
//=============================================================================

/**
 * @brief Create spike event queue
 *
 * @param capacity Queue capacity (rounded up to power of 2)
 * @param gpu_enabled Whether to allocate GPU memory
 * @return Queue handle, or NULL on failure
 */
NIMCP_EXPORT spike_queue_t* spike_queue_create(uint32_t capacity, bool gpu_enabled);

/**
 * @brief Destroy spike queue
 *
 * @param queue Queue handle (NULL-safe)
 */
NIMCP_EXPORT void spike_queue_destroy(spike_queue_t* queue);

/**
 * @brief Push spike event to queue (thread-safe)
 *
 * @param queue Spike queue
 * @param event Spike event to enqueue
 * @return true on success, false if queue full
 */
NIMCP_EXPORT bool spike_queue_push(spike_queue_t* queue, const spike_event_t* event);

/**
 * @brief Pop spike event from queue (thread-safe)
 *
 * @param queue Spike queue
 * @param event Output spike event
 * @return true if event retrieved, false if queue empty
 */
NIMCP_EXPORT bool spike_queue_pop(spike_queue_t* queue, spike_event_t* event);

/**
 * @brief Get number of events in queue
 *
 * @param queue Spike queue
 * @return Number of queued events
 */
NIMCP_EXPORT uint32_t spike_queue_size(const spike_queue_t* queue);

/**
 * @brief Check if queue is empty
 *
 * @param queue Spike queue
 * @return true if empty
 */
NIMCP_EXPORT bool spike_queue_is_empty(const spike_queue_t* queue);

/**
 * @brief Synchronize queue with GPU (if GPU-enabled)
 *
 * @param queue Spike queue
 * @param direction true = CPU->GPU, false = GPU->CPU
 * @return true on success, false if not GPU-enabled
 */
NIMCP_EXPORT bool spike_queue_sync_gpu(spike_queue_t* queue, bool direction);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SPIKE_EVENT_H
