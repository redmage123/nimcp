//=============================================================================
// nimcp_thread.h - NIMCP Thread Utilities
//=============================================================================

#ifndef NIMCP_THREAD_H
#define NIMCP_THREAD_H

/* Ensure POSIX features are enabled for pthread_rwlock_t */
#if !defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE < 200112L
#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "utils/platform/nimcp_platform_thread.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_cond.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_thread.h
 * @brief Professional pthread wrapper with error handling and resource management
 *
 * Features:
 * - Consistent error handling with thread-local error messages
 * - Named resource locks (string-based mutex acquisition)
 * - Thread attributes (stack size, priority, detached)
 * - Mutex types (normal, recursive, errorcheck)
 * - Condition variables with timeout
 * - Reference-counted resource locks
 */

//=============================================================================
// Result Codes
//=============================================================================

/* Use consistent guard with nimcp_common.h */
#ifndef NIMCP_RESULT_T_DEFINED
#define NIMCP_RESULT_T_DEFINED
typedef int32_t nimcp_result_t;
#endif

/* Include canonical error type if not already defined */
#ifndef NIMCP_ERROR_TYPE_DEFINED
#include "utils/error/nimcp_error_codes.h"
#endif

/* Thread-specific error codes not in canonical header */
#ifndef NIMCP_ERROR_SYSTEM
#define NIMCP_ERROR_SYSTEM (-2)       // System error (check errno)
#endif
#ifndef NIMCP_BUSY
#define NIMCP_BUSY (-5)               // Resource busy (trylock)
#endif

//=============================================================================
// Type Definitions
//=============================================================================

typedef nimcp_platform_thread_t nimcp_thread_t;
typedef nimcp_platform_mutex_t nimcp_mutex_t;
typedef nimcp_platform_cond_t nimcp_cond_t;
typedef nimcp_platform_mutex_t nimcp_spinlock_t;  // Using mutex for portability

// Note: rwlock and once are not yet in platform layer, keeping pthread for now
#include <pthread.h>
typedef pthread_once_t nimcp_once_t;
typedef pthread_rwlock_t nimcp_rwlock_t;   // Read-write lock

// Thread error information
typedef struct {
    int error_code;
    char error_message[256];
} nimcp_thread_error_t;

// Thread attributes
typedef struct {
    size_t stack_size;
    int priority;
    bool detached;
} thread_attr_t;

// Mutex attributes
typedef enum { MUTEX_TYPE_NORMAL, MUTEX_TYPE_RECURSIVE, MUTEX_TYPE_ERRORCHECK } mutex_type_t;

typedef struct {
    mutex_type_t type;
} mutex_attr_t;

// Resource lock entry (for named locks)
typedef struct resource_entry {
    char* resource_id;
    nimcp_mutex_t* lock;
    size_t ref_count;
    struct resource_entry* next;
} resource_entry_t;

// Resource lock bucket
typedef struct {
    nimcp_mutex_t bucket_mutex;
    resource_entry_t* entries;
} resource_bucket_t;

//=============================================================================
// Constants
//=============================================================================

// Note: pthread_once is not yet in platform layer
#define NIMCP_ONCE_INIT PTHREAD_ONCE_INIT
#define NIMCP_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define NIMCP_THREAD_DEFAULT_STACK_SIZE (2 * 1024 * 1024)
#define RESOURCE_LOCK_BUCKETS 256

// Resource lock table
typedef struct {
    nimcp_mutex_t global_mutex;
    resource_bucket_t buckets[RESOURCE_LOCK_BUCKETS];
    bool initialized;
} resource_lock_table_t;

//=============================================================================
// Thread Management
//=============================================================================

/**
 * @brief Initialize thread subsystem
 * Call once before using thread utilities
 */
nimcp_result_t nimcp_thread_init(void);

/**
 * @brief Create a new thread
 * @param thread Output thread handle
 * @param start_routine Thread function
 * @param arg Argument to thread function
 * @param attr Thread attributes (NULL for defaults)
 */
nimcp_result_t nimcp_thread_create(nimcp_thread_t* thread, void* (*start_routine)(void*), void* arg,
                                   const thread_attr_t* attr);

/**
 * @brief Wait for thread to finish
 */
nimcp_result_t nimcp_thread_join(nimcp_thread_t thread, void** retval);

/**
 * @brief Detach thread (auto-cleanup on exit)
 */
nimcp_result_t nimcp_thread_detach(nimcp_thread_t thread);

/**
 * @brief Exit current thread
 */
void nimcp_thread_exit(void* retval) __attribute__((noreturn));

/**
 * @brief Get current thread ID
 */
nimcp_thread_t nimcp_thread_self(void);

/**
 * @brief Compare thread IDs
 */
bool nimcp_thread_equal(nimcp_thread_t t1, nimcp_thread_t t2);

/**
 * @brief Execute function exactly once (thread-safe initialization)
 */
nimcp_result_t nimcp_once(nimcp_once_t* once_control, void (*init_routine)(void));

//=============================================================================
// Mutex Operations
//=============================================================================

/**
 * @brief Initialize mutex
 */
nimcp_result_t nimcp_mutex_init(nimcp_mutex_t* mutex, const mutex_attr_t* attr);

/**
 * @brief Create and initialize a mutex (dynamically allocated)
 * @param attr Mutex attributes (NULL for default NORMAL type)
 * @return Pointer to initialized mutex or NULL on failure
 */
nimcp_mutex_t* nimcp_mutex_create(const mutex_attr_t* attr);

/**
 * @brief Destroy mutex (does NOT free memory)
 * @note For embedded mutexes initialized with nimcp_mutex_init()
 */
nimcp_result_t nimcp_mutex_destroy(nimcp_mutex_t* mutex);

/**
 * @brief Destroy and free a heap-allocated mutex
 * @note For mutexes created with nimcp_mutex_create()
 * @note Do NOT use on embedded mutexes - use nimcp_mutex_destroy() instead
 */
nimcp_result_t nimcp_mutex_free(nimcp_mutex_t* mutex);

/**
 * @brief Lock mutex (blocking)
 */
nimcp_result_t nimcp_mutex_lock(nimcp_mutex_t* mutex);

/**
 * @brief Try to lock mutex (non-blocking)
 * @return NIMCP_SUCCESS if locked, NIMCP_BUSY if already locked
 */
nimcp_result_t nimcp_mutex_trylock(nimcp_mutex_t* mutex);

/**
 * @brief Unlock mutex
 */
nimcp_result_t nimcp_mutex_unlock(nimcp_mutex_t* mutex);

//=============================================================================
// Spinlock Operations (implemented as mutexes for portability)
//=============================================================================

/**
 * @brief Initialize spinlock
 */
nimcp_result_t nimcp_spinlock_init(nimcp_spinlock_t* lock);

/**
 * @brief Destroy spinlock
 */
nimcp_result_t nimcp_spinlock_destroy(nimcp_spinlock_t* lock);

/**
 * @brief Lock spinlock (blocking)
 */
nimcp_result_t nimcp_spinlock_lock(nimcp_spinlock_t* lock);

/**
 * @brief Unlock spinlock
 */
nimcp_result_t nimcp_spinlock_unlock(nimcp_spinlock_t* lock);

//=============================================================================
// Read-Write Lock Operations
//=============================================================================

/**
 * @brief Initialize read-write lock
 */
nimcp_result_t nimcp_rwlock_init(nimcp_rwlock_t* lock);

/**
 * @brief Destroy read-write lock
 */
nimcp_result_t nimcp_rwlock_destroy(nimcp_rwlock_t* lock);

/**
 * @brief Acquire read lock (shared access)
 */
nimcp_result_t nimcp_rwlock_rdlock(nimcp_rwlock_t* lock);

/**
 * @brief Acquire write lock (exclusive access)
 */
nimcp_result_t nimcp_rwlock_wrlock(nimcp_rwlock_t* lock);

/**
 * @brief Try to acquire read lock without blocking
 * @return NIMCP_SUCCESS if locked, NIMCP_BUSY if would block
 */
nimcp_result_t nimcp_rwlock_tryrdlock(nimcp_rwlock_t* lock);

/**
 * @brief Try to acquire write lock without blocking
 * @return NIMCP_SUCCESS if locked, NIMCP_BUSY if would block
 */
nimcp_result_t nimcp_rwlock_trywrlock(nimcp_rwlock_t* lock);

/**
 * @brief Try to acquire read lock with timeout
 * @param lock Read-write lock
 * @param timeout_ms Timeout in milliseconds
 * @return NIMCP_SUCCESS if locked, NIMCP_BUSY if timeout
 */
nimcp_result_t nimcp_rwlock_timedrdlock(nimcp_rwlock_t* lock, uint32_t timeout_ms);

/**
 * @brief Try to acquire write lock with timeout
 * @param lock Read-write lock
 * @param timeout_ms Timeout in milliseconds
 * @return NIMCP_SUCCESS if locked, NIMCP_BUSY if timeout
 */
nimcp_result_t nimcp_rwlock_timedwrlock(nimcp_rwlock_t* lock, uint32_t timeout_ms);

/**
 * @brief Unlock read-write lock
 */
nimcp_result_t nimcp_rwlock_unlock(nimcp_rwlock_t* lock);

//=============================================================================
// Condition Variables
//=============================================================================

/**
 * @brief Initialize condition variable
 */
nimcp_result_t nimcp_cond_init(nimcp_cond_t* cond);

/**
 * @brief Create a new condition variable
 * @return Pointer to initialized condition variable or NULL on failure
 */
nimcp_cond_t* nimcp_cond_create(void);

/**
 * @brief Destroy condition variable
 */
nimcp_result_t nimcp_cond_destroy(nimcp_cond_t* cond);

/**
 * @brief Wait on condition variable (releases mutex)
 */
nimcp_result_t nimcp_cond_wait(nimcp_cond_t* cond, nimcp_mutex_t* mutex);

/**
 * @brief Wait on condition with timeout (milliseconds)
 */
nimcp_result_t nimcp_cond_timedwait(nimcp_cond_t* cond, nimcp_mutex_t* mutex, uint32_t timeout_ms);

/**
 * @brief Signal one waiting thread
 */
nimcp_result_t nimcp_cond_signal(nimcp_cond_t* cond);

/**
 * @brief Signal all waiting threads
 */
nimcp_result_t nimcp_cond_broadcast(nimcp_cond_t* cond);

//=============================================================================
// Named Resource Locks
//=============================================================================

/**
 * @brief Get or create a mutex by string ID
 *
 * Example: nimcp_get_resource_lock("brain_network", &mutex);
 *
 * Reference counted - automatically freed when all holders release
 */
nimcp_result_t nimcp_get_resource_lock(const char* resource_id, nimcp_mutex_t** mutex);

/**
 * @brief Release a resource lock (decrement refcount)
 */
nimcp_result_t nimcp_release_resource_lock(const char* resource_id);

//=============================================================================
// Thread Naming and Affinity
//=============================================================================

/**
 * @brief Maximum thread name length (pthread limit on Linux)
 */
#define NIMCP_THREAD_NAME_MAX 16

/**
 * @brief Set name of calling thread (for debugging)
 * @param name Thread name (max 15 chars + null terminator)
 * @return NIMCP_SUCCESS or NIMCP_ERROR_SYSTEM
 */
nimcp_result_t nimcp_thread_set_name(const char* name);

/**
 * @brief Get name of calling thread
 * @param name Buffer to receive name
 * @param len Buffer length (must be at least NIMCP_THREAD_NAME_MAX)
 * @return NIMCP_SUCCESS or NIMCP_ERROR_INVALID_PARAM or NIMCP_ERROR_SYSTEM
 */
nimcp_result_t nimcp_thread_get_name(char* name, size_t len);

/**
 * @brief Set CPU affinity for thread (Linux-specific, returns success on other platforms)
 * @param thread Thread handle
 * @param cpu_id CPU core ID to bind to
 * @return NIMCP_SUCCESS or NIMCP_ERROR_SYSTEM
 */
nimcp_result_t nimcp_thread_set_affinity(nimcp_thread_t thread, uint32_t cpu_id);

/**
 * @brief Get CPU affinity for thread (Linux-specific, returns 0 on other platforms)
 * @param thread Thread handle
 * @param cpu_id Output parameter for CPU core ID
 * @return NIMCP_SUCCESS or NIMCP_ERROR_INVALID_PARAM or NIMCP_ERROR_SYSTEM
 */
nimcp_result_t nimcp_thread_get_affinity(nimcp_thread_t thread, uint32_t* cpu_id);

//=============================================================================
// Error Handling
//=============================================================================

/**
 * @brief Get last error message for this thread
 */
const char* nimcp_thread_get_error(void);

/**
 * @brief Clear error state
 */
void nimcp_thread_clear_error(void);

//=============================================================================
// Cleanup
//=============================================================================

/**
 * @brief Cleanup thread subsystem
 * Call on shutdown
 */
void nimcp_thread_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_THREAD_H
