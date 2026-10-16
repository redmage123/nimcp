/**
 * @file nimcp_queue_internal.h
 * @brief Internal shared definitions for queue implementations
 *
 * WHAT: Internal structures and helper functions shared across queue types
 * WHY:  Code reuse and consistent behavior across BLOCKING, SPSC, MPMC
 * HOW:  Shared vtable pattern with type-specific implementations
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 2.0.0
 */

#ifndef NIMCP_QUEUE_INTERNAL_H
#define NIMCP_QUEUE_INTERNAL_H

#include "utils/containers/nimcp_queue.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Common Internal Structure
//=============================================================================

/**
 * @brief Base queue structure - shared fields across all implementations
 */
struct nimcp_queue {
    // Type identification
    nimcp_queue_type_t type;        /**< Queue implementation type */

    // Configuration
    nimcp_queue_config_t config;    /**< Immutable after creation */

    // Statistics
    nimcp_queue_status_t status;    /**< Runtime statistics */

    // Implementation-specific data
    void* impl_data;                /**< Type-specific implementation data */

    // Buffer storage
    uint8_t* buffer;                /**< Circular buffer storage */
    size_t capacity;                /**< Actual capacity (may be rounded up) */
};

//=============================================================================
// Implementation-Specific Create/Destroy Functions
//=============================================================================

/**
 * @brief Create blocking queue implementation
 */
nimcp_result_t nimcp_queue_blocking_create(
    struct nimcp_queue* queue,
    const nimcp_queue_config_t* config
);

/**
 * @brief Destroy blocking queue implementation
 */
void nimcp_queue_blocking_destroy(struct nimcp_queue* queue);

/**
 * @brief Create SPSC queue implementation
 */
nimcp_result_t nimcp_queue_spsc_create(
    struct nimcp_queue* queue,
    const nimcp_queue_config_t* config
);

/**
 * @brief Destroy SPSC queue implementation
 */
void nimcp_queue_spsc_destroy(struct nimcp_queue* queue);

/**
 * @brief Create MPMC queue implementation
 */
nimcp_result_t nimcp_queue_mpmc_create(
    struct nimcp_queue* queue,
    const nimcp_queue_config_t* config
);

/**
 * @brief Destroy MPMC queue implementation
 */
void nimcp_queue_mpmc_destroy(struct nimcp_queue* queue);

//=============================================================================
// Implementation-Specific Operation Functions
//=============================================================================

// Blocking queue operations
nimcp_result_t nimcp_queue_blocking_enqueue(struct nimcp_queue* queue, const void* item, uint32_t timeout_ms);
nimcp_result_t nimcp_queue_blocking_dequeue(struct nimcp_queue* queue, void* item, uint32_t timeout_ms);
nimcp_result_t nimcp_queue_blocking_peek(struct nimcp_queue* queue, void* item);
nimcp_result_t nimcp_queue_blocking_clear(struct nimcp_queue* queue);
bool nimcp_queue_blocking_is_empty(struct nimcp_queue* queue);
bool nimcp_queue_blocking_is_full(struct nimcp_queue* queue);
size_t nimcp_queue_blocking_get_size(struct nimcp_queue* queue);

// SPSC queue operations
nimcp_result_t nimcp_queue_spsc_enqueue(struct nimcp_queue* queue, const void* item, uint32_t timeout_ms);
nimcp_result_t nimcp_queue_spsc_dequeue(struct nimcp_queue* queue, void* item, uint32_t timeout_ms);
nimcp_result_t nimcp_queue_spsc_peek(struct nimcp_queue* queue, void* item);
nimcp_result_t nimcp_queue_spsc_clear(struct nimcp_queue* queue);
bool nimcp_queue_spsc_is_empty(struct nimcp_queue* queue);
bool nimcp_queue_spsc_is_full(struct nimcp_queue* queue);
size_t nimcp_queue_spsc_get_size(struct nimcp_queue* queue);

// MPMC queue operations
nimcp_result_t nimcp_queue_mpmc_enqueue(struct nimcp_queue* queue, const void* item, uint32_t timeout_ms);
nimcp_result_t nimcp_queue_mpmc_dequeue(struct nimcp_queue* queue, void* item, uint32_t timeout_ms);
nimcp_result_t nimcp_queue_mpmc_peek(struct nimcp_queue* queue, void* item);
nimcp_result_t nimcp_queue_mpmc_clear(struct nimcp_queue* queue);
bool nimcp_queue_mpmc_is_empty(struct nimcp_queue* queue);
bool nimcp_queue_mpmc_is_full(struct nimcp_queue* queue);
size_t nimcp_queue_mpmc_get_size(struct nimcp_queue* queue);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_QUEUE_INTERNAL_H
