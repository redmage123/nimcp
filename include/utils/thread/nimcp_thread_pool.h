//=============================================================================
// nimcp_thread_pool.h - Simple Thread Pool for Batch Processing
//=============================================================================

#ifndef NIMCP_THREAD_POOL_H
#define NIMCP_THREAD_POOL_H

#include <stdbool.h>
#include <stddef.h>
#include "utils/thread/nimcp_thread.h"

/**
 * @file nimcp_thread_pool.h
 * @brief Simple fixed-size thread pool for parallel task execution
 *
 * Features:
 * - Fixed number of worker threads
 * - Circular task queue with blocking
 * - Submit tasks and wait for completion
 * - Automatic work distribution
 *
 * Example usage:
 *   nimcp_thread_pool_t* pool = nimcp_pool_create(4);  // 4 threads
 *   for (int i = 0; i < 100; i++) {
 *       nimcp_pool_submit(pool, my_task, &data[i]);
 *   }
 *   nimcp_pool_wait(pool);  // Wait for all tasks
 *   nimcp_pool_destroy(pool);
 */

//=============================================================================
// Constants
//=============================================================================

#define NIMCP_POOL_MAX_THREADS 64
#define NIMCP_POOL_MAX_QUEUE 1024

//=============================================================================
// Types
//=============================================================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Task function signature
 * @param arg User-provided argument
 */
typedef void (*nimcp_task_fn)(void* arg);

/**
 * @brief Thread pool handle (opaque)
 */
typedef struct nimcp_thread_pool nimcp_thread_pool_t;

//=============================================================================
// Thread Pool API
//=============================================================================

/**
 * @brief Create a thread pool
 * @param num_threads Number of worker threads (1-64)
 * @return Pool handle or NULL on error
 */
nimcp_thread_pool_t* nimcp_pool_create(size_t num_threads);

/**
 * @brief Destroy thread pool (waits for completion)
 * @param pool Pool handle
 */
void nimcp_pool_destroy(nimcp_thread_pool_t* pool);

/**
 * @brief Submit a task to the pool
 * @param pool Pool handle
 * @param task Task function
 * @param arg Argument for task function
 * @return NIMCP_SUCCESS or error code
 *
 * Blocks if queue is full
 */
nimcp_result_t nimcp_pool_submit(nimcp_thread_pool_t* pool, nimcp_task_fn task, void* arg);

/**
 * @brief Wait for all submitted tasks to complete
 * @param pool Pool handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_pool_wait(nimcp_thread_pool_t* pool);

/**
 * @brief Get number of pending tasks
 * @param pool Pool handle
 * @return Number of tasks in queue
 */
size_t nimcp_pool_pending(nimcp_thread_pool_t* pool);

/**
 * @brief Get number of active worker threads
 * @param pool Pool handle
 * @return Number of active threads
 */
size_t nimcp_pool_active(nimcp_thread_pool_t* pool);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_THREAD_POOL_H
