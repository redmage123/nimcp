//=============================================================================
// nimcp_brain_bio_async.h - Bio-Async Integration API for Brain Module
//=============================================================================
/**
 * @file nimcp_brain_bio_async.h
 * @brief Public API for bio-async integration in brain module
 *
 * WHAT: Async message handling and communication for brain
 * WHY:  Enable decoupled communication with cognitive modules
 * HOW:  Bio-router registration, message handlers, predictive signals
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 1.0.0
 */

#ifndef NIMCP_BRAIN_BIO_ASYNC_H
#define NIMCP_BRAIN_BIO_ASYNC_H

#include "core/brain/nimcp_brain.h"
#include "async/nimcp_bio_async.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Initialization and Shutdown API
//=============================================================================

/**
 * @brief Initialize bio-async for brain module
 *
 * WHAT: Register brain with bio-router and setup handlers
 * WHY:  Enable async communication with other modules
 * HOW:  Create module context, register handlers, init predictors
 *
 * @param brain Brain instance
 * @return NIMCP_SUCCESS or error code
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Must be called before multi-threaded access
 * LOGGING: LOG_INFO on success, LOG_ERROR on failure
 */
nimcp_error_t brain_bio_async_init(brain_t brain);

/**
 * @brief Shutdown bio-async for brain module
 *
 * WHAT: Unregister from bio-router and cleanup resources
 * WHY:  Clean shutdown
 * HOW:  Destroy predictors, unregister, free context
 *
 * @param brain Brain instance
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Must not be called concurrently
 * LOGGING: LOG_INFO on shutdown
 */
void brain_bio_async_shutdown(brain_t brain);

//=============================================================================
// Message Processing API
//=============================================================================

/**
 * @brief Process incoming messages (call from brain update loop)
 *
 * WHAT: Process pending messages in inbox
 * WHY:  Handle async communication
 * HOW:  Call bio_router_process_inbox
 *
 * @param brain Brain instance
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 *
 * COMPLEXITY: O(n) where n = messages processed
 * THREAD SAFETY: Thread-safe
 * LOGGING: LOG_TRACE on message processing
 *
 * USAGE:
 * Call this periodically from brain update loop:
 * ```c
 * brain_update(brain, dt);
 * brain_bio_async_process_messages(brain, 10);  // Process up to 10 msgs
 * ```
 */
uint32_t brain_bio_async_process_messages(brain_t brain, uint32_t max_messages);

/**
 * @brief Update brain state and publish signals (call periodically)
 *
 * WHAT: Update predictive signals for brain state
 * WHY:  Keep observers informed of state changes
 * HOW:  Publish predictive signals
 *
 * @param brain Brain instance
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe
 * LOGGING: LOG_TRACE on signal publishing
 *
 * USAGE:
 * Call this periodically (e.g., every 100ms):
 * ```c
 * brain_bio_async_update(brain);  // Publish state signals
 * ```
 */
void brain_bio_async_update(brain_t brain);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bio-async statistics
 *
 * @param brain Brain instance
 * @param messages_sent Output: messages sent
 * @param messages_received Output: messages received
 * @param state_queries Output: state queries handled
 * @param activation_requests Output: activation requests handled
 * @return true if stats available, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe (mutex protected)
 */
bool brain_bio_async_get_stats(
    brain_t brain,
    uint64_t* messages_sent,
    uint64_t* messages_received,
    uint64_t* state_queries,
    uint64_t* activation_requests
);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_BRAIN_BIO_ASYNC_H
