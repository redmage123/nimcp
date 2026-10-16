/**
 * @file nimcp_signal_exception_queue.h
 * @brief Signal exception queue using NIMCP queue utilities
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Queue for deferring signal crash processing to exception system
 * WHY:  Signal handlers must be minimal; defer exception processing to main thread
 * HOW:  Uses NIMCP SPSC lock-free queue for signal-safe enqueue
 *
 * ARCHITECTURE:
 * ```
 * Signal Handler (producer)    Main Thread (consumer)
 *        |                            |
 *        v                            v
 * +------------------+         +------------------+
 * | try_enqueue      |  ---->  | dequeue          |
 * | (lock-free SPSC) |         | (lock-free SPSC) |
 * +------------------+         +------------------+
 *        |                            |
 *        v                            v
 * nimcp_queue (SPSC)           Exception Processing
 * (pre-allocated at init,      (create exception,
 *  no malloc in signal path)   present to immune)
 * ```
 *
 * SIGNAL SAFETY:
 * - Queue is pre-allocated at initialization (before any signals)
 * - Enqueue uses lock-free SPSC algorithm (no malloc, no mutex)
 * - Only try_enqueue is called from signal handler (never blocks)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SIGNAL_EXCEPTION_QUEUE_H
#define NIMCP_SIGNAL_EXCEPTION_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <signal.h>

#include "utils/signal/nimcp_signal_handler.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Size of the signal exception queue (power of 2 for SPSC efficiency) */
#define SIGNAL_EXCEPTION_QUEUE_SIZE 16

/* ============================================================================
 * Queue Entry
 * ============================================================================ */

/**
 * @brief Single entry in the signal exception queue
 *
 * Each entry contains a copy of the crash context and metadata.
 */
typedef struct {
    signal_crash_context_t ctx;        /**< Copy of crash context */
    uint64_t timestamp_us;             /**< Timestamp when crash was captured */
} signal_exception_entry_t;

/* ============================================================================
 * Queue Statistics
 * ============================================================================ */

/**
 * @brief Queue statistics
 */
typedef struct {
    uint64_t enqueue_count;    /**< Total entries enqueued */
    uint64_t dequeue_count;    /**< Total entries dequeued */
    uint64_t overflow_count;   /**< Entries dropped due to full queue */
    size_t pending_count;      /**< Current entries pending */
    size_t queue_capacity;     /**< Queue capacity */
} signal_exception_queue_stats_t;

/* ============================================================================
 * Queue API
 * ============================================================================ */

/**
 * @brief Initialize the signal exception queue
 *
 * WHAT: Create and initialize the SPSC queue for signal exceptions
 * WHY:  Must be called before installing signal handlers
 * HOW:  Allocates SPSC queue using nimcp_queue API
 *
 * IMPORTANT: Call this at application startup, before any signals can occur.
 *
 * @return 0 on success, -1 on error
 */
int signal_exception_queue_init(void);

/**
 * @brief Shutdown the signal exception queue
 *
 * WHAT: Destroy queue and release resources
 * WHY:  Clean shutdown
 * HOW:  Destroys nimcp_queue instance
 */
void signal_exception_queue_shutdown(void);

/**
 * @brief Check if queue is initialized
 *
 * @return true if initialized and ready for use
 */
bool signal_exception_queue_is_initialized(void);

/**
 * @brief Enqueue a crash context (signal-safe)
 *
 * WHAT: Add crash to queue for deferred processing
 * WHY:  Signal handlers must be minimal; defer exception creation
 * HOW:  Uses SPSC try_enqueue (lock-free, non-blocking)
 *
 * SIGNAL SAFETY: This function is async-signal-safe.
 * - Uses pre-allocated SPSC queue
 * - No memory allocation
 * - No mutex operations
 * - Lock-free algorithm
 *
 * @param sig Signal number that caused the crash
 * @param ctx Crash context to enqueue (copied, not stored by reference)
 * @return true if enqueued successfully, false if queue is full
 */
bool signal_exception_queue_enqueue(int sig, const signal_crash_context_t* ctx);

/**
 * @brief Dequeue a crash entry (thread-safe, NOT signal-safe)
 *
 * WHAT: Remove oldest crash from queue for processing
 * WHY:  Main thread processes crashes as exceptions
 * HOW:  Uses SPSC try_dequeue (lock-free)
 *
 * NOTE: This function is thread-safe but should only be called
 *       from the single consumer thread (SPSC constraint).
 *
 * @param out Output buffer for dequeued entry
 * @return true if entry was dequeued, false if queue is empty
 */
bool signal_exception_queue_dequeue(signal_exception_entry_t* out);

/**
 * @brief Get count of pending entries
 *
 * @return Number of pending entries
 */
size_t signal_exception_queue_pending_count(void);

/**
 * @brief Check if queue is empty
 *
 * @return true if queue is empty
 */
bool signal_exception_queue_is_empty(void);

/**
 * @brief Check if queue is full
 *
 * @return true if queue is full
 */
bool signal_exception_queue_is_full(void);

/**
 * @brief Get queue statistics
 *
 * @param stats Output buffer for statistics
 */
void signal_exception_queue_get_stats(signal_exception_queue_stats_t* stats);

/**
 * @brief Reset queue statistics
 */
void signal_exception_queue_reset_stats(void);

/* ============================================================================
 * Processing API
 * ============================================================================ */

/**
 * @brief Process pending signal exceptions
 *
 * WHAT: Dequeue crashes and convert to exceptions
 * WHY:  Bridge between signal handler queue and exception system
 * HOW:  Dequeue entries, create signal exceptions, present to immune
 *
 * This function:
 * 1. Dequeues pending crash entries
 * 2. Creates nimcp_signal_exception_t for each
 * 3. Presents to immune system
 * 4. Dispatches through exception handler chain
 *
 * @param max_count Maximum entries to process (0 = all pending)
 * @return Number of entries processed
 */
size_t signal_exception_queue_process(size_t max_count);

/**
 * @brief Register callback for exception processing
 *
 * WHAT: Set custom handler for processed signal exceptions
 * WHY:  Allow application-specific exception handling
 * HOW:  Store callback function pointer
 *
 * @param callback Function to call for each exception (NULL to clear)
 * @param user_data User data passed to callback
 */
typedef void (*signal_exception_callback_t)(
    const signal_exception_entry_t* entry,
    void* user_data
);

void signal_exception_queue_set_callback(
    signal_exception_callback_t callback,
    void* user_data
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SIGNAL_EXCEPTION_QUEUE_H */
