/**
 * @file nimcp_deadlock_detector.h
 * @brief Deadlock detection and prevention system
 *
 * WHAT: Detect and prevent deadlocks in multithreaded code
 * WHY:  Avoid hanging threads and production outages
 * HOW:  Timeout-based locks + lock ordering + dependency tracking
 *
 * FEATURES:
 * - Timeout-based mutex wrappers (fail instead of hang)
 * - Lock ordering enforcement (prevent circular wait)
 * - Thread dependency tracking (detect cycles)
 * - Automatic deadlock detection
 * - Lock statistics and diagnostics
 *
 * USAGE:
 * 1. Initialize: deadlock_detector_init()
 * 2. Create tracked mutex: tracked_mutex_init(&mutex, "name", timeout_ms)
 * 3. Lock with timeout: tracked_mutex_lock(&mutex)
 * 4. Unlock: tracked_mutex_unlock(&mutex)
 * 5. Check for deadlocks: deadlock_detector_check()
 * 6. Report: deadlock_detector_report()
 *
 * LOCK ORDERING:
 * - Mutexes are assigned order numbers (0, 1, 2, ...)
 * - Threads must acquire mutexes in ascending order
 * - Violation of order triggers warning/error
 *
 * @author NIMCP Team
 * @date 2025-11-09
 */

#ifndef NIMCP_DEADLOCK_DETECTOR_H
#define NIMCP_DEADLOCK_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration
//=============================================================================

#define MAX_TRACKED_MUTEXES 128    /**< Max mutexes to track */
#define MAX_THREADS 64              /**< Max threads to track */
#define MAX_LOCKS_PER_THREAD 16     /**< Max locks per thread */
#define DEFAULT_TIMEOUT_MS 5000     /**< Default lock timeout (5 seconds) */

/**
 * @brief Tracked mutex with timeout and ordering
 */
typedef struct {
    nimcp_mutex_t mutex;         /**< Underlying mutex */
    const char* name;              /**< Debug name */
    uint32_t order;                /**< Lock order number */
    uint32_t timeout_ms;           /**< Lock timeout in ms */
    pthread_t owner;               /**< Current owner thread */
    bool owner_valid;              /**< P2-U20: Whether owner field is valid (portable pthread_t check) */
    bool is_locked;                /**< Lock state */
    uint64_t lock_count;           /**< Total locks */
    uint64_t timeout_count;        /**< Timeout failures */
    uint64_t contention_count;     /**< Contention events */
    uint64_t total_wait_time_us;   /**< Total wait time */
} tracked_mutex_t;

/**
 * @brief Deadlock detector configuration
 */
typedef struct {
    bool enable_detector;          /**< Enable deadlock detection */
    bool enable_lock_ordering;     /**< Enforce lock ordering */
    bool enable_timeout;           /**< Use timeouts instead of blocking */
    bool abort_on_deadlock;        /**< Abort if deadlock detected */
    bool abort_on_order_violation; /**< Abort on lock order violation */
    uint32_t default_timeout_ms;   /**< Default mutex timeout */
    uint32_t check_interval_ms;    /**< How often to check for deadlocks */
} deadlock_detector_config_t;

/**
 * @brief Deadlock detector statistics
 */
typedef struct {
    uint64_t total_locks;          /**< Total lock attempts */
    uint64_t total_unlocks;        /**< Total unlocks */
    uint64_t lock_timeouts;        /**< Lock timeout failures */
    uint64_t order_violations;     /**< Lock order violations */
    uint64_t deadlocks_detected;   /**< Deadlocks detected */
    uint64_t cycles_detected;      /**< Dependency cycles found */
    uint32_t active_threads;       /**< Currently active threads */
    uint32_t active_mutexes;       /**< Currently tracked mutexes */
} deadlock_detector_stats_t;

/**
 * @brief Lock dependency (for cycle detection)
 */
typedef struct {
    pthread_t thread_id;           /**< Thread ID */
    tracked_mutex_t* waiting_on;   /**< Mutex being waited on */
    tracked_mutex_t* holding[MAX_LOCKS_PER_THREAD]; /**< Mutexes held */
    uint32_t num_holding;          /**< Number of locks held */
    bool in_use;                   /**< Slot is in use (portable check, don't compare pthread_t to 0) */
} lock_dependency_t;

//=============================================================================
// Public API
//=============================================================================

/**
 * @brief Initialize deadlock detector
 *
 * WHAT: Setup deadlock detection system
 * WHY:  Enable deadlock prevention and detection
 * HOW:  Initialize tracking data structures
 *
 * @param config Configuration (NULL for defaults)
 * @return true on success
 */
bool deadlock_detector_init(const deadlock_detector_config_t* config);

/**
 * @brief Shutdown deadlock detector
 *
 * WHAT: Clean up and report final statistics
 * WHY:  Graceful shutdown
 * HOW:  Print stats, free resources
 */
void deadlock_detector_shutdown(void);

/**
 * @brief Get default configuration
 *
 * @return Default config with detector enabled
 */
deadlock_detector_config_t deadlock_detector_default_config(void);

/**
 * @brief Initialize tracked mutex
 *
 * WHAT: Create mutex with tracking and timeout
 * WHY:  Enable deadlock detection for this mutex
 * HOW:  Init pthread_mutex, assign order number
 *
 * @param mutex Tracked mutex to initialize
 * @param name Debug name
 * @param timeout_ms Lock timeout (0 = use default)
 * @return true on success
 */
bool tracked_mutex_init(tracked_mutex_t* mutex, const char* name, uint32_t timeout_ms);

/**
 * @brief Destroy tracked mutex
 *
 * @param mutex Mutex to destroy
 */
void tracked_mutex_destroy(tracked_mutex_t* mutex);

/**
 * @brief Lock mutex with timeout and order checking
 *
 * WHAT: Acquire lock with timeout and ordering enforcement
 * WHY:  Prevent deadlocks and detect violations
 * HOW:  Check order, try lock with timeout, track ownership
 *
 * @param mutex Mutex to lock
 * @return true on success, false on timeout or order violation
 */
bool tracked_mutex_lock(tracked_mutex_t* mutex);

/**
 * @brief Try to lock mutex without blocking
 *
 * @param mutex Mutex to try
 * @return true if locked, false if would block
 */
bool tracked_mutex_trylock(tracked_mutex_t* mutex);

/**
 * @brief Unlock tracked mutex
 *
 * WHAT: Release lock and update tracking
 * WHY:  Allow other threads to acquire
 * HOW:  Update ownership, unlock mutex
 *
 * @param mutex Mutex to unlock
 */
void tracked_mutex_unlock(tracked_mutex_t* mutex);

/**
 * @brief Check for deadlocks
 *
 * WHAT: Detect circular wait conditions
 * WHY:  Find deadlocks before they hang
 * HOW:  Build dependency graph, find cycles
 *
 * @return Number of deadlocks detected
 */
uint32_t deadlock_detector_check(void);

/**
 * @brief Get deadlock detector statistics
 *
 * @return Current statistics
 */
deadlock_detector_stats_t deadlock_detector_get_stats(void);

/**
 * @brief Print deadlock detector statistics
 */
void deadlock_detector_print_stats(void);

/**
 * @brief Report current lock state
 *
 * WHAT: Print all tracked mutexes and their owners
 * WHY:  Debug lock contention and deadlocks
 * HOW:  Iterate tracked mutexes, print state
 */
void deadlock_detector_report(void);

/**
 * @brief Print lock dependency graph
 *
 * WHAT: Show which threads are waiting on which locks
 * WHY:  Visualize dependencies for debugging
 * HOW:  Print thread -> mutex -> thread chains
 */
void deadlock_detector_print_dependencies(void);

/**
 * @brief Enable/disable detector at runtime
 *
 * @param enable True to enable, false to disable
 */
void deadlock_detector_set_enabled(bool enable);

/**
 * @brief Check if detector is enabled
 *
 * @return true if enabled
 */
bool deadlock_detector_is_enabled(void);

/**
 * @brief Set default timeout for all new mutexes
 *
 * @param timeout_ms Timeout in milliseconds
 */
void deadlock_detector_set_default_timeout(uint32_t timeout_ms);

//=============================================================================
// Convenience Macros
//=============================================================================

#ifdef NIMCP_ENABLE_DEADLOCK_DETECTION

// Tracked mutex operations with automatic naming
#define TRACKED_MUTEX_INIT(mutex, timeout_ms) \
    tracked_mutex_init((mutex), #mutex, (timeout_ms))

#define TRACKED_MUTEX_LOCK(mutex) \
    tracked_mutex_lock(mutex)

#define TRACKED_MUTEX_UNLOCK(mutex) \
    tracked_mutex_unlock(mutex)

#else

// Fallback to standard pthread mutexes (no tracking)
#define TRACKED_MUTEX_INIT(mutex, timeout_ms) \
    (nimcp_mutex_init(&(mutex)->mutex, NULL) == 0)

#define TRACKED_MUTEX_LOCK(mutex) \
    (nimcp_mutex_lock(&(mutex)->mutex) == 0)

#define TRACKED_MUTEX_UNLOCK(mutex) \
    nimcp_mutex_unlock(&(mutex)->mutex)

#endif // NIMCP_ENABLE_DEADLOCK_DETECTION

#ifdef __cplusplus
}
#endif

#endif // NIMCP_DEADLOCK_DETECTOR_H
