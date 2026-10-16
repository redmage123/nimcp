//=============================================================================
// nimcp_neuron_model_bio_async.h - Bio-Async Integration for Neuron Model
//=============================================================================
/**
 * @file nimcp_neuron_model_bio_async.h
 * @brief Public API for neuron model bio-async spike event publishing
 *
 * WHAT: Async spike event publishing and activation request handling for neurons
 * WHY:  Notify observers of neuron spikes without tight coupling
 * HOW:  Publish BIO_MSG_NEURON_ACTIVATION_RESPONSE on spike events
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 1.0.0
 */

#ifndef NIMCP_NEURON_MODEL_BIO_ASYNC_H
#define NIMCP_NEURON_MODEL_BIO_ASYNC_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Initialize bio-async for neuron model module
 *
 * WHAT: Register neuron model with bio-router
 * WHY:  Enable async spike event publishing
 * HOW:  Create global context, register handlers
 *
 * @return NIMCP_SUCCESS or error code
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe (mutex protected)
 */
nimcp_error_t neuron_model_bio_async_init(void);

/**
 * @brief Shutdown bio-async for neuron model module
 *
 * WHAT: Clean up bio-async resources
 * WHY:  Prevent memory leaks on shutdown
 * HOW:  Unregister from router, release memory
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe (mutex protected)
 */
void neuron_model_bio_async_shutdown(void);

//=============================================================================
// Event Publishing Functions
//=============================================================================

/**
 * @brief Publish spike event for a neuron
 *
 * WHAT: Announce that a neuron has spiked
 * WHY:  Notify downstream modules (STDP, global workspace, etc.)
 * HOW:  Send BIO_MSG_NEURON_ACTIVATION_RESPONSE via dopamine channel
 *
 * @param neuron_id ID of the spiking neuron
 * @param spike_time_ms Time of spike in milliseconds
 * @param activation Activation level at spike
 * @param region_id Brain region containing the neuron
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe
 */
void neuron_model_publish_spike(
    uint32_t neuron_id,
    float spike_time_ms,
    float activation,
    uint32_t region_id
);

/**
 * @brief Publish neuron state (for non-spiking updates)
 *
 * WHAT: Announce neuron voltage/state update
 * WHY:  Enable monitoring and visualization of neuron states
 * HOW:  Send via acetylcholine channel (fast queries)
 *
 * @param neuron_id ID of the neuron
 * @param voltage Current membrane voltage
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe
 */
void neuron_model_publish_state(uint32_t neuron_id, float voltage);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get bio-async statistics for neuron model module
 *
 * @param spikes_published Output: total spikes published
 * @param activation_requests Output: activation requests received
 * @param messages_sent Output: total messages sent
 * @param messages_received Output: total messages received
 * @return true if stats retrieved successfully
 */
bool neuron_model_bio_async_get_stats(
    uint64_t* spikes_published,
    uint64_t* activation_requests,
    uint64_t* messages_sent,
    uint64_t* messages_received
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NEURON_MODEL_BIO_ASYNC_H
