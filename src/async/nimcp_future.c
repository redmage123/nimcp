/**
 * @file nimcp_future.c
 * @brief High-performance futures/promises implementation with lock-free operations
 *
 * WHAT: Lock-free futures/promises system with combinators for async composition
 * WHY:  Enable efficient async workflows across brain modules with <1μs overhead
 * HOW:  Atomic state machine + condition variables + refcounted shared state
 *
 * ARCHITECTURE:
 *
 *   Promise (Producer)              Futures (Consumers)
 *   ┌─────────────────┐             ┌──────────────────┐
 *   │  promise_t      │             │   future_t       │
 *   │  - shared_state │────────────>│   - shared_state │
 *   └─────────────────┘      │      └──────────────────┘
 *                            │      ┌──────────────────┐
 *                            └─────>│   future_t       │
 *                                   │   - shared_state │
 *                                   └──────────────────┘
 *
 *   Shared State (Refcounted):
 *   ┌──────────────────────────────────────────┐
 *   │  state: PENDING → COMPLETED/FAILED       │
 *   │  refcount: N (promise + futures)         │
 *   │  result: void* (allocated on demand)     │
 *   │  callbacks: linked list                   │
 *   │  mutex + condvar: for blocking waits     │
 *   └──────────────────────────────────────────┘
 *
 * STATE TRANSITIONS (Atomic):
 *   [PENDING] ──complete()──> [COMPLETED]
 *       │
 *       ├──────fail()─────> [FAILED]
 *       │
 *       └────cancel()────> [CANCELLED]
 *
 * BIO-ASYNC BACKEND (Optional):
 * When bio-async backend is enabled, this module delegates to biologically-
 * inspired async mechanisms:
 * - Promises use neuromodulator-based signaling (DOPAMINE channel)
 * - Futures track confidence decay over time
 * - all() combinator uses phase coupling synchronization
 * - any() combinator uses competitive neuromodulator release
 *
 * PERFORMANCE OPTIMIZATIONS:
 * - Fast-path lock-free reads via atomic state check
 * - Refcounted shared state for zero-copy future cloning
 * - Callback execution on completion thread (no queuing overhead)
 * - Cache-line alignment to prevent false sharing
 *
 * NOTE: Exception handling is integrated via api/nimcp_api_exception.h
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 1.0.0
 */

#include "async/nimcp_future.h"
#include "api/nimcp_api_exception.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/exception/nimcp_exception_macros.h"

#include "async/nimcp_bio_async.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/thread/nimcp_atomic.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_cond.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define LOG_MODULE "async_future"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(future)

//=============================================================================
// Security Integration (Forward Declarations to Avoid Header Conflicts)
//=============================================================================

/**
 * WHAT: Forward declarations for security integration
 * WHY:  Avoid header conflicts between nimcp_common.h and nimcp_error_codes.h
 * HOW:  Declare only the functions we need, use void* for opaque types
 */
typedef struct nimcp_sec_integration nimcp_sec_integration_t;

// Security module categories (must match nimcp_security_integration.h)
#define NIMCP_SEC_CAT_UTILITY 7

// Security integration function declarations
extern int nimcp_sec_register_module(
    nimcp_sec_integration_t* ctx,
    const char* name,
    int category,
    uint32_t* module_id
);

extern int nimcp_sec_unregister_module(
    nimcp_sec_integration_t* ctx,
    uint32_t module_id
);

extern int nimcp_sec_record_interaction(
    nimcp_sec_integration_t* ctx,
    uint32_t module_id,
    bool success,
    double weight
);

// Convenience macro for security success recording
#define NIMCP_SEC_SUCCESS(ctx, module_id) \
    nimcp_sec_record_interaction(ctx, module_id, true, 1.0)

//=============================================================================
// Constants and Configuration
//=============================================================================

/**
 * WHAT: Cache line size for alignment
 * WHY:  Prevent false sharing between threads
 * HOW:  Align shared state to 64-byte boundaries
 */
#define NIMCP_CACHE_LINE_SIZE 64

/**
 * WHAT: Magic value for validation
 * WHY:  Detect corrupted/invalid handles
 * HOW:  Check magic on each operation
 */
#define FUTURE_MAGIC 0x46555455  // 'FUTU'

//=============================================================================
// Module Integration State
//=============================================================================

/**
 * WHAT: Global state for unified memory and security integration
 * WHY:  Track module-level registration for all futures/promises
 * HOW:  Set during nimcp_future_init(), used by all operations
 */
static nimcp_sec_integration_t* g_future_security_ctx = NULL;
static uint32_t g_future_security_module_id = 0;
static unified_mem_manager_t g_future_memory_mgr = NULL;
static nimcp_atomic_bool_t g_future_initialized = {0};
static nimcp_atomic_bool_t g_future_initializing = {0};  // Guard against concurrent init

//=============================================================================
// Bio-Async Backend Integration
//=============================================================================

/**
 * WHAT: Bio-async backend enable flag
 * WHY:  Allow switching between traditional and bio-inspired async
 * HOW:  When enabled, delegate to bio-async functions
 */
static bool g_bio_async_backend_enabled = false;

/**
 * WHAT: Default neuromodulator channel for bio-async promises
 * WHY:  DOPAMINE provides fast completion signaling with medium decay
 * HOW:  Used when creating bio-promises without explicit channel
 */
#define BIO_DEFAULT_CHANNEL BIO_CHANNEL_DOPAMINE

/**
 * WHAT: Default oscillation band for phase sync combinators
 * WHY:  GAMMA provides fast binding for tight synchronization
 * HOW:  Used for all() combinator
 */
#define BIO_DEFAULT_SYNC_BAND BIO_OSC_GAMMA

/**
 * WHAT: Allocation header for unified memory handle tracking
 * WHY:  Need to store handle alongside data for proper deallocation
 * HOW:  Prefix each allocation with header containing handle
 */
typedef struct {
    unified_mem_handle_t handle;  /**< Unified memory handle (NULL if malloc) */
    size_t size;                  /**< Allocation size for debugging */
} future_alloc_header_t;

/**
 * WHAT: Internal allocation wrapper with handle tracking
 * WHY:  Use unified memory if available, fallback to malloc
 * HOW:  Allocate extra space for header, store handle, return data pointer
 */
static void* future_alloc(size_t size) {
    if (g_future_memory_mgr) {
        // Allocate extra space for header
        size_t total_size = sizeof(future_alloc_header_t) + size;
        unified_mem_request_t req = unified_mem_request_direct(total_size);
        unified_mem_handle_t handle = unified_mem_alloc(g_future_memory_mgr, &req);
        if (handle) {
            void* base = (void*)unified_mem_write(handle);
            if (base) {
                future_alloc_header_t* header = (future_alloc_header_t*)base;
                header->handle = handle;
                header->size = size;
                return (char*)base + sizeof(future_alloc_header_t);
            }
            unified_mem_free(handle);
        }
        LOG_WARNING("Unified memory allocation failed for %zu bytes, falling back to malloc", size);
    }

    // Fallback to nimcp_malloc with header
    size_t total_size = sizeof(future_alloc_header_t) + size;
    void* base = nimcp_malloc(total_size);
    if (base) {
        future_alloc_header_t* header = (future_alloc_header_t*)base;
        header->handle = NULL;  // NULL indicates nimcp_malloc was used
        header->size = size;
        return (char*)base + sizeof(future_alloc_header_t);
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "future_alloc: validation failed");
    return NULL;
}

/**
 * WHAT: Internal deallocation wrapper with handle tracking
 * WHY:  Properly free unified memory or malloc allocations
 * HOW:  Read header to get handle, free appropriately
 */
static void future_free(void* ptr) {
    if (!ptr) return;

    // Get header from data pointer
    future_alloc_header_t* header = (future_alloc_header_t*)((char*)ptr - sizeof(future_alloc_header_t));

    if (header->handle) {
        // Unified memory - free the handle
        unified_mem_free(header->handle);
    } else {
        // Standard nimcp_malloc - free the base pointer
        nimcp_free(header);
    }
}

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * WHAT: Callback destructor function type
 * WHY:  Clean up user_data when callback is removed without invocation
 * HOW:  Called when future is destroyed while still pending
 */
typedef void (*callback_destructor_t)(void* user_data);

/**
 * WHAT: Callback node for then() combinator
 * WHY:  Chain multiple callbacks on future completion
 * HOW:  Linked list of callbacks
 */
typedef struct callback_node {
    nimcp_future_callback_t callback;  /**< User callback function */
    void* user_data;                   /**< User context */
    callback_destructor_t destructor;  /**< Optional destructor for user_data */
    struct callback_node* next;        /**< Next callback in chain */
} callback_node_t;

/**
 * WHAT: Shared state between promise and futures
 * WHY:  Single source of truth for async operation state
 * HOW:  Reference counted, atomically managed state
 *
 * LAYOUT: Cache-line aligned to prevent false sharing
 */
typedef struct nimcp_future_shared_state {
    // Magic value for validation
    uint32_t magic;

    // State machine (atomic)
    nimcp_atomic_uint32_t state;  /**< Current state (PENDING/COMPLETED/FAILED/CANCELLED) */

    // Reference counting (atomic)
    nimcp_atomic_uint32_t refcount;  /**< Promise + futures count */

    // Synchronization primitives
    nimcp_platform_mutex_t mutex;  /**< Protects condvar and callbacks */
    nimcp_platform_cond_t cond;    /**< Signals state changes */

    // Result storage
    void* result;              /**< Result data (NULL if failed/cancelled) */
    size_t result_size;        /**< Size of result in bytes */
    nimcp_error_t error;       /**< Error code if failed */

    // Callback chain
    callback_node_t* callbacks;  /**< Linked list of then() callbacks */

    // Timing for statistics
    uint64_t create_time_ns;   /**< Creation timestamp */
    uint64_t complete_time_ns; /**< Completion timestamp */

} __attribute__((aligned(NIMCP_CACHE_LINE_SIZE))) nimcp_future_shared_state_t;

/**
 * WHAT: Promise handle structure
 * WHY:  Producer-side handle to shared state
 * HOW:  Wrapper around shared state pointer, optionally bio-async handle
 */
struct nimcp_promise_struct {
    uint32_t magic;                      /**< Magic for validation */
    nimcp_future_shared_state_t* shared; /**< Shared state (traditional mode) */
    nimcp_bio_promise_t bio_promise;     /**< Bio-async promise (bio mode) */
    bool is_bio_mode;                    /**< true if using bio-async backend */
};

/**
 * WHAT: Future handle structure
 * WHY:  Consumer-side handle to shared state
 * HOW:  Wrapper around shared state pointer, optionally bio-async handle
 */
struct nimcp_future_struct {
    uint32_t magic;                      /**< Magic for validation */
    nimcp_future_shared_state_t* shared; /**< Shared state (traditional mode) */
    nimcp_bio_future_t bio_future;       /**< Bio-async future (bio mode) */
    bool is_bio_mode;                    /**< true if using bio-async backend */
};

//=============================================================================
// Global Statistics (Thread-Safe Atomic Counters)
//=============================================================================

static nimcp_atomic_uint64_t g_stats_promises_created = {0};
static nimcp_atomic_uint64_t g_stats_promises_destroyed = {0};
static nimcp_atomic_uint64_t g_stats_futures_created = {0};
static nimcp_atomic_uint64_t g_stats_futures_destroyed = {0};
static nimcp_atomic_uint64_t g_stats_active_promises = {0};
static nimcp_atomic_uint64_t g_stats_active_futures = {0};
static nimcp_atomic_uint64_t g_stats_completions = {0};
static nimcp_atomic_uint64_t g_stats_failures = {0};
static nimcp_atomic_uint64_t g_stats_cancellations = {0};
static nimcp_atomic_uint64_t g_stats_waits_total = {0};
static nimcp_atomic_uint64_t g_stats_waits_immediate = {0};
static nimcp_atomic_uint64_t g_stats_waits_blocked = {0};
static nimcp_atomic_uint64_t g_stats_waits_timeout = {0};
static nimcp_atomic_uint64_t g_stats_then_chains = {0};
static nimcp_atomic_uint64_t g_stats_all_operations = {0};
static nimcp_atomic_uint64_t g_stats_any_operations = {0};
static nimcp_atomic_uint64_t g_stats_map_operations = {0};
static nimcp_atomic_uint64_t g_stats_total_wait_time_ns = {0};
static nimcp_atomic_uint64_t g_stats_total_completion_time_ns = {0};
static nimcp_atomic_uint64_t g_stats_total_memory_bytes = {0};
static nimcp_atomic_uint64_t g_stats_result_memory_bytes = {0};
static nimcp_atomic_uint64_t g_stats_refcount_increments = {0};
static nimcp_atomic_uint64_t g_stats_refcount_decrements = {0};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * WHAT: Create shared state
 * WHY:  Initialize reference-counted shared state
 * HOW:  Allocate, initialize atomics, mutexes, condvars
 */
static nimcp_future_shared_state_t* shared_state_create(size_t result_size)
{
    // Allocate shared state via unified memory wrapper
    nimcp_future_shared_state_t* shared = (nimcp_future_shared_state_t*)
        future_alloc(sizeof(nimcp_future_shared_state_t));
    if (!shared) {
        LOG_ERROR("Failed to allocate shared state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "shared_state_create: shared is NULL");
        return NULL;
    }

    memset(shared, 0, sizeof(nimcp_future_shared_state_t));

    // Initialize magic
    shared->magic = FUTURE_MAGIC;

    // Initialize state to PENDING
    nimcp_atomic_init_u32(&shared->state, NIMCP_FUTURE_PENDING);

    // Initialize refcount to 1 (for the promise)
    nimcp_atomic_init_u32(&shared->refcount, 1);

    // Initialize synchronization
    if (nimcp_platform_mutex_init(&shared->mutex, false) != 0) {
        future_free(shared);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "shared_state_create: validation failed");
        return NULL;
    }

    if (nimcp_platform_cond_init(&shared->cond) != 0) {
        nimcp_platform_mutex_destroy(&shared->mutex);
        future_free(shared);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "shared_state_create: validation failed");
        return NULL;
    }

    // Initialize result storage
    shared->result = NULL;
    shared->result_size = result_size;
    shared->error = NIMCP_SUCCESS;

    // Initialize callbacks
    shared->callbacks = NULL;

    // Initialize timing
    shared->create_time_ns = nimcp_platform_time_monotonic_ms() * 1000000ULL;
    shared->complete_time_ns = 0;

    // Update statistics
    nimcp_atomic_fetch_add_u64(&g_stats_total_memory_bytes,
                               sizeof(nimcp_future_shared_state_t),
                               NIMCP_MEMORY_ORDER_RELAXED);

    return shared;
}

/**
 * WHAT: Increment reference count
 * WHY:  Track number of handles to shared state
 * HOW:  Atomic increment with ACQ_REL ordering
 */
static void shared_state_addref(nimcp_future_shared_state_t* shared)
{
    if (!shared || shared->magic != FUTURE_MAGIC) {
        return;
    }

    nimcp_atomic_fetch_add_u32(&shared->refcount, 1, NIMCP_MEMORY_ORDER_ACQ_REL);
    nimcp_atomic_fetch_add_u64(&g_stats_refcount_increments, 1, NIMCP_MEMORY_ORDER_RELAXED);
}

/**
 * WHAT: Decrement reference count and free if last ref
 * WHY:  Clean up shared state when no more handles exist
 * HOW:  Atomic decrement, free on zero
 */
static void shared_state_release(nimcp_future_shared_state_t* shared)
{
    if (!shared || shared->magic != FUTURE_MAGIC) {
        return;
    }

    nimcp_atomic_fetch_add_u64(&g_stats_refcount_decrements, 1, NIMCP_MEMORY_ORDER_RELAXED);

    uint32_t old_ref = nimcp_atomic_fetch_sub_u32(&shared->refcount, 1, NIMCP_MEMORY_ORDER_ACQ_REL);

    if (old_ref == 1) {
        // Last reference - clean up

        // Free result if allocated
        if (shared->result) {
            nimcp_atomic_fetch_sub_u64(&g_stats_result_memory_bytes,
                                       shared->result_size,
                                       NIMCP_MEMORY_ORDER_RELAXED);
            nimcp_atomic_fetch_sub_u64(&g_stats_total_memory_bytes,
                                       shared->result_size,
                                       NIMCP_MEMORY_ORDER_RELAXED);
            future_free(shared->result);
        }

        // Free callbacks, calling destructor for user_data if callback was not invoked
        callback_node_t* cb = shared->callbacks;
        uint32_t state = nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_RELAXED);
        bool was_pending = (state == NIMCP_FUTURE_PENDING);
        while (cb) {
            callback_node_t* next = cb->next;
            // If the future was never completed and there's a destructor, call it
            // to clean up the user_data (e.g., combinator tracker references)
            if (was_pending && cb->destructor) {
                cb->destructor(cb->user_data);
            }
            future_free(cb);
            cb = next;
        }

        // Destroy sync primitives
        nimcp_platform_cond_destroy(&shared->cond);
        nimcp_platform_mutex_destroy(&shared->mutex);

        // Invalidate magic
        shared->magic = 0;

        // Update statistics
        nimcp_atomic_fetch_sub_u64(&g_stats_total_memory_bytes,
                                   sizeof(nimcp_future_shared_state_t),
                                   NIMCP_MEMORY_ORDER_RELAXED);

        // Free shared state
        future_free(shared);
    }
}

/**
 * WHAT: Invoke all registered callbacks
 * WHY:  Execute then() chains when future completes
 * HOW:  Walk callback list, invoke each with result/error
 *
 * NOTE: Must hold mutex when calling
 */
static void shared_state_invoke_callbacks(nimcp_future_shared_state_t* shared)
{
    if (!shared || shared->magic != FUTURE_MAGIC) {
        return;
    }

    uint32_t state = nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_ACQUIRE);

    callback_node_t* cb = shared->callbacks;
    while (cb) {
        if (cb->callback) {
            const void* result = (state == NIMCP_FUTURE_COMPLETED) ? shared->result : NULL;
            nimcp_error_t error = (state == NIMCP_FUTURE_FAILED) ? shared->error : NIMCP_SUCCESS;

            // Invoke callback
            cb->callback(result, error, cb->user_data);
        }

        cb = cb->next;
    }
}

//=============================================================================
// Promise API Implementation
//=============================================================================

nimcp_promise_t nimcp_promise_create(size_t result_size)
{
    LOG_DEBUG("Creating promise with result_size=%zu", result_size);

    // Allocate promise handle via unified memory wrapper
    nimcp_promise_t promise = (nimcp_promise_t)future_alloc(sizeof(struct nimcp_promise_struct));
    if (!promise) {
        LOG_ERROR("Failed to allocate promise handle");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_promise_create: promise is NULL");
        return NULL;
    }

    // Initialize all fields to NULL/false
    memset(promise, 0, sizeof(struct nimcp_promise_struct));
    promise->magic = FUTURE_MAGIC;

    // Check if bio-async backend is enabled
    if (g_bio_async_backend_enabled && nimcp_bio_async_is_initialized()) {
        // Use bio-async backend with DOPAMINE channel for fast completion
        nimcp_bio_promise_t bio_promise = nimcp_bio_promise_create(
            BIO_DEFAULT_CHANNEL, result_size);
        if (!bio_promise) {
            LOG_ERROR("Failed to create bio-async promise");
            future_free(promise);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_promise_create: bio_promise is NULL");
            return NULL;
        }

        promise->bio_promise = bio_promise;
        promise->is_bio_mode = true;
        promise->shared = NULL;  // Not used in bio mode

        LOG_DEBUG("Bio-async promise created: %p (channel: %s)",
                  (void*)promise, nimcp_bio_channel_name(BIO_DEFAULT_CHANNEL));
    } else {
        // Traditional mode: create shared state
        nimcp_future_shared_state_t* shared = shared_state_create(result_size);
        if (!shared) {
            LOG_ERROR("Failed to create shared state for promise");
            future_free(promise);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_promise_create: shared is NULL");
            return NULL;
        }

        promise->shared = shared;
        promise->bio_promise = NULL;
        promise->is_bio_mode = false;
    }

    // Update statistics
    nimcp_atomic_fetch_add_u64(&g_stats_promises_created, 1, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_fetch_add_u64(&g_stats_active_promises, 1, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_fetch_add_u64(&g_stats_total_memory_bytes,
                               sizeof(struct nimcp_promise_struct),
                               NIMCP_MEMORY_ORDER_RELAXED);

    // Record security interaction
    if (g_future_security_ctx && g_future_security_module_id != 0) {
        NIMCP_SEC_SUCCESS(g_future_security_ctx, g_future_security_module_id);
    }

    LOG_DEBUG("Promise created successfully: %p (bio_mode=%d)", (void*)promise, promise->is_bio_mode);
    return promise;
}

nimcp_error_t nimcp_promise_complete(nimcp_promise_t promise, const void* result)
{
    LOG_DEBUG("Completing promise: %p", (void*)promise);

    NIMCP_CHECK_THROW(promise && promise->magic == FUTURE_MAGIC,
                       NIMCP_ERROR_NULL_POINTER, "Promise complete failed: invalid promise handle");

    // Bio-async backend: delegate to bio-promise
    if (promise->is_bio_mode) {
        NIMCP_CHECK_THROW(promise->bio_promise != NULL, NIMCP_ERROR_INVALID_STATE,
                          "Promise complete failed: NULL bio-promise");

        nimcp_error_t err = nimcp_bio_promise_complete(promise->bio_promise, result);
        if (err == NIMCP_SUCCESS) {
            nimcp_atomic_fetch_add_u64(&g_stats_completions, 1, NIMCP_MEMORY_ORDER_RELAXED);
            if (g_future_security_ctx && g_future_security_module_id != 0) {
                NIMCP_SEC_SUCCESS(g_future_security_ctx, g_future_security_module_id);
            }
        }
        return err;
    }

    // Traditional mode: use shared state
    nimcp_future_shared_state_t* shared = promise->shared;
    NIMCP_CHECK_THROW(shared && shared->magic == FUTURE_MAGIC, NIMCP_ERROR_INVALID_STATE,
                      "Promise complete failed: invalid shared state");

    // Validate result parameter if result_size > 0
    NIMCP_CHECK_THROW(shared->result_size == 0 || result != NULL,
                       NIMCP_ERROR_NULL_POINTER, "Promise complete failed: NULL result for non-void promise");

    // Allocate and copy result BEFORE setting state to COMPLETED
    // This prevents a race where consumer sees COMPLETED but result isn't ready
    void* result_copy = NULL;
    if (shared->result_size > 0) {
        result_copy = future_alloc(shared->result_size);
        NIMCP_CHECK_THROW(result_copy != NULL, NIMCP_ERROR_NO_MEMORY,
                          "Promise complete failed: memory allocation for result (%zu bytes)", shared->result_size);
        memcpy(result_copy, result, shared->result_size);
    }

    // Save current result pointer in case CAS fails (another thread may have
    // already completed with a different value)
    void* old_result = shared->result;

    // Store the result pointer FIRST (before state change)
    // Use a plain store here - the CAS will provide the release barrier
    if (shared->result_size > 0) {
        shared->result = result_copy;
    }

    // Try to transition from PENDING to COMPLETED (CAS operation)
    // The ACQ_REL semantics ensure that the result pointer write above
    // is visible to any thread that sees the state as COMPLETED
    uint32_t expected = NIMCP_FUTURE_PENDING;
    if (!nimcp_atomic_compare_exchange_u32(&shared->state, &expected, NIMCP_FUTURE_COMPLETED,
                                           NIMCP_MEMORY_ORDER_ACQ_REL)) {
        // Already completed/failed/cancelled - restore old result and free our copy
        shared->result = old_result;
        future_free(result_copy);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE,
                          "nimcp_promise_complete: promise already completed/failed/cancelled");
    }

    // Update memory statistics after successful completion
    if (shared->result_size > 0) {

        nimcp_atomic_fetch_add_u64(&g_stats_result_memory_bytes,
                                   shared->result_size,
                                   NIMCP_MEMORY_ORDER_RELAXED);
        nimcp_atomic_fetch_add_u64(&g_stats_total_memory_bytes,
                                   shared->result_size,
                                   NIMCP_MEMORY_ORDER_RELAXED);
    }

    // Record completion time
    shared->complete_time_ns = nimcp_platform_time_monotonic_ms() * 1000000ULL;
    uint64_t duration_ns = shared->complete_time_ns - shared->create_time_ns;
    nimcp_atomic_fetch_add_u64(&g_stats_total_completion_time_ns, duration_ns,
                               NIMCP_MEMORY_ORDER_RELAXED);

    // Update statistics
    nimcp_atomic_fetch_add_u64(&g_stats_completions, 1, NIMCP_MEMORY_ORDER_RELAXED);

    // Wake all waiters and extract callback list for invocation outside mutex
    // WHY: Invoking callbacks while holding mutex can cause deadlocks if
    //      callbacks try to acquire other locks or re-enter the future API
    nimcp_platform_mutex_lock(&shared->mutex);
    nimcp_platform_cond_broadcast(&shared->cond);

    // Extract callback list while holding mutex
    callback_node_t* callbacks_to_invoke = shared->callbacks;
    shared->callbacks = NULL;  // Clear list so it won't be invoked again

    nimcp_platform_mutex_unlock(&shared->mutex);

    // Invoke callbacks AFTER releasing mutex to prevent deadlocks
    callback_node_t* cb = callbacks_to_invoke;
    while (cb) {
        if (cb->callback) {
            cb->callback(shared->result, NIMCP_SUCCESS, cb->user_data);
        }
        callback_node_t* next = cb->next;
        future_free(cb);
        cb = next;
    }

    // Record security interaction
    if (g_future_security_ctx && g_future_security_module_id != 0) {
        NIMCP_SEC_SUCCESS(g_future_security_ctx, g_future_security_module_id);
    }

    LOG_DEBUG("Promise completed successfully: %p", (void*)promise);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_promise_fail(nimcp_promise_t promise, nimcp_error_t error)
{
    LOG_DEBUG("Failing promise: %p with error=%d", (void*)promise, error);

    NIMCP_CHECK_THROW(promise && promise->magic == FUTURE_MAGIC,
                       NIMCP_ERROR_NULL_POINTER, "Promise fail failed: invalid promise handle");

    // Validate error is a failure code (must not be SUCCESS)
    NIMCP_CHECK_THROW(error != NIMCP_SUCCESS, NIMCP_ERROR_INVALID_PARAM,
                       "Promise fail failed: error code cannot be SUCCESS");

    // Bio-async backend: delegate to bio-promise
    if (promise->is_bio_mode) {
        NIMCP_CHECK_THROW(promise->bio_promise != NULL, NIMCP_ERROR_INVALID_STATE,
                          "Promise fail failed: NULL bio-promise");

        nimcp_error_t err = nimcp_bio_promise_fail(promise->bio_promise, error);
        if (err == NIMCP_SUCCESS) {
            nimcp_atomic_fetch_add_u64(&g_stats_failures, 1, NIMCP_MEMORY_ORDER_RELAXED);
        }
        return err;
    }

    // Traditional mode: use shared state
    nimcp_future_shared_state_t* shared = promise->shared;
    NIMCP_CHECK_THROW(shared && shared->magic == FUTURE_MAGIC, NIMCP_ERROR_INVALID_STATE,
                      "Promise fail failed: invalid shared state");

    // P1-22 fix: Store error code BEFORE the CAS state transition.
    // This ensures that any thread that sees FAILED state will also see
    // the correct error code, preventing stale reads.
    shared->error = error;
    __atomic_thread_fence(__ATOMIC_RELEASE);  // Ensure error is visible before state change

    // Try to transition from PENDING to FAILED (CAS operation)
    uint32_t expected = NIMCP_FUTURE_PENDING;
    if (!nimcp_atomic_compare_exchange_u32(&shared->state, &expected, NIMCP_FUTURE_FAILED,
                                           NIMCP_MEMORY_ORDER_ACQ_REL)) {
        // Already completed/failed/cancelled - restore error to avoid confusion
        shared->error = NIMCP_SUCCESS;
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE,
                          "nimcp_promise_fail: promise already completed/failed/cancelled");
    }

    // Record completion time
    shared->complete_time_ns = nimcp_platform_time_monotonic_ms() * 1000000ULL;

    // Update statistics
    nimcp_atomic_fetch_add_u64(&g_stats_failures, 1, NIMCP_MEMORY_ORDER_RELAXED);

    // Wake all waiters and extract callback list for invocation outside mutex
    // WHY: Invoking callbacks while holding mutex can cause deadlocks if
    //      callbacks try to acquire other locks or re-enter the future API
    nimcp_platform_mutex_lock(&shared->mutex);
    nimcp_platform_cond_broadcast(&shared->cond);

    // Extract callback list while holding mutex
    callback_node_t* callbacks_to_invoke = shared->callbacks;
    shared->callbacks = NULL;  // Clear list so it won't be invoked again

    nimcp_platform_mutex_unlock(&shared->mutex);

    // Invoke callbacks AFTER releasing mutex to prevent deadlocks
    callback_node_t* cb = callbacks_to_invoke;
    while (cb) {
        if (cb->callback) {
            cb->callback(NULL, error, cb->user_data);
        }
        callback_node_t* next = cb->next;
        future_free(cb);
        cb = next;
    }

    return NIMCP_SUCCESS;
}

nimcp_future_t nimcp_promise_get_future(nimcp_promise_t promise)
{
    if (!promise || promise->magic != FUTURE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_promise_get_future: promise is NULL");
        return NULL;
    }

    // Allocate future handle via unified memory wrapper
    nimcp_future_t future = (nimcp_future_t)future_alloc(sizeof(struct nimcp_future_struct));
    if (!future) {
        LOG_ERROR("Failed to allocate future handle");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_promise_get_future: future is NULL");
        return NULL;
    }

    // Initialize all fields to NULL/false
    memset(future, 0, sizeof(struct nimcp_future_struct));
    future->magic = FUTURE_MAGIC;

    // Bio-async backend: get bio-future from bio-promise
    if (promise->is_bio_mode) {
        if (!promise->bio_promise) {
            LOG_ERROR("Failed to get future: NULL bio-promise");
            future_free(future);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_promise_get_future: promise->bio_promise is NULL");
            return NULL;
        }

        nimcp_bio_future_t bio_future = nimcp_bio_promise_get_future(promise->bio_promise);
        if (!bio_future) {
            LOG_ERROR("Failed to get bio-future from bio-promise");
            future_free(future);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_promise_get_future: bio_future is NULL");
            return NULL;
        }

        future->bio_future = bio_future;
        future->is_bio_mode = true;
        future->shared = NULL;

        LOG_DEBUG("Bio-async future created from promise: %p", (void*)future);
    } else {
        // Traditional mode: use shared state
        nimcp_future_shared_state_t* shared = promise->shared;
        if (!shared || shared->magic != FUTURE_MAGIC) {
            future_free(future);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_promise_get_future: shared is NULL");
            return NULL;
        }

        // Increment refcount
        shared_state_addref(shared);

        future->shared = shared;
        future->bio_future = NULL;
        future->is_bio_mode = false;
    }

    // Update statistics
    nimcp_atomic_fetch_add_u64(&g_stats_futures_created, 1, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_fetch_add_u64(&g_stats_active_futures, 1, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_fetch_add_u64(&g_stats_total_memory_bytes,
                               sizeof(struct nimcp_future_struct),
                               NIMCP_MEMORY_ORDER_RELAXED);

    return future;
}

void nimcp_promise_destroy(nimcp_promise_t promise)
{
    if (!promise || promise->magic != FUTURE_MAGIC) {
        return;
    }

    LOG_DEBUG("Destroying promise: %p (bio_mode=%d)", (void*)promise, promise->is_bio_mode);

    // Update statistics
    nimcp_atomic_fetch_add_u64(&g_stats_promises_destroyed, 1, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_fetch_sub_u64(&g_stats_active_promises, 1, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_fetch_sub_u64(&g_stats_total_memory_bytes,
                               sizeof(struct nimcp_promise_struct),
                               NIMCP_MEMORY_ORDER_RELAXED);

    // Invalidate magic
    promise->magic = 0;

    // Bio-async backend: destroy bio-promise
    if (promise->is_bio_mode) {
        if (promise->bio_promise) {
            nimcp_bio_promise_destroy(promise->bio_promise);
            promise->bio_promise = NULL;
        }
    } else {
        // Traditional mode: release shared state
        if (promise->shared) {
            shared_state_release(promise->shared);
            promise->shared = NULL;
        }
    }

    // Free promise handle via unified memory wrapper
    future_free(promise);
}

//=============================================================================
// Future API Implementation
//=============================================================================

/**
 * WHAT: Map bio-future state to traditional future state
 * WHY:  Bio-async has additional states (DECAYED, REFRACTORY)
 * HOW:  Map extended states to closest traditional equivalent
 */
static nimcp_future_state_t bio_state_to_future_state(nimcp_bio_future_state_t bio_state)
{
    switch (bio_state) {
        case BIO_FUTURE_PENDING:
            return NIMCP_FUTURE_PENDING;
        case BIO_FUTURE_COMPLETED:
            return NIMCP_FUTURE_COMPLETED;
        case BIO_FUTURE_FAILED:
            return NIMCP_FUTURE_FAILED;
        case BIO_FUTURE_CANCELLED:
            return NIMCP_FUTURE_CANCELLED;
        case BIO_FUTURE_DECAYED:
            // Decayed maps to COMPLETED (result still accessible, just low confidence)
            return NIMCP_FUTURE_COMPLETED;
        case BIO_FUTURE_REFRACTORY:
            // Refractory maps to PENDING (temporarily blocked)
            return NIMCP_FUTURE_PENDING;
        default:
            return NIMCP_FUTURE_PENDING;
    }
}

nimcp_future_state_t nimcp_future_state(nimcp_future_t future)
{
    if (!future || future->magic != FUTURE_MAGIC) {
        return NIMCP_FUTURE_PENDING;
    }

    // Bio-async backend: get bio-future state
    if (future->is_bio_mode) {
        if (!future->bio_future) {
            return NIMCP_FUTURE_PENDING;
        }
        nimcp_bio_future_state_t bio_state = nimcp_bio_future_state(future->bio_future);
        return bio_state_to_future_state(bio_state);
    }

    // Traditional mode: use shared state
    nimcp_future_shared_state_t* shared = future->shared;
    if (!shared || shared->magic != FUTURE_MAGIC) {
        return NIMCP_FUTURE_PENDING;
    }

    return (nimcp_future_state_t)nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_ACQUIRE);
}

bool nimcp_future_wait(nimcp_future_t future)
{
    if (!future || future->magic != FUTURE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_future_wait: future is NULL");
        return false;
    }

    uint64_t wait_start_ns = nimcp_platform_time_monotonic_ms() * 1000000ULL;

    // Bio-async backend: use bio-future wait
    if (future->is_bio_mode) {
        if (!future->bio_future) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_future_wait: future->bio_future is NULL");
            return false;
        }

        // Wait indefinitely (0 timeout = wait for decay)
        nimcp_error_t err = nimcp_bio_future_wait(future->bio_future, NULL, 0);

        // Update statistics
        uint64_t wait_end_ns = nimcp_platform_time_monotonic_ms() * 1000000ULL;
        uint64_t wait_time_ns = wait_end_ns - wait_start_ns;
        nimcp_atomic_fetch_add_u64(&g_stats_waits_total, 1, NIMCP_MEMORY_ORDER_RELAXED);
        nimcp_atomic_fetch_add_u64(&g_stats_waits_blocked, 1, NIMCP_MEMORY_ORDER_RELAXED);
        nimcp_atomic_fetch_add_u64(&g_stats_total_wait_time_ns, wait_time_ns, NIMCP_MEMORY_ORDER_RELAXED);

        // NIMCP_SUCCESS means completed, error means failed/decayed/cancelled
        return (err == NIMCP_SUCCESS);
    }

    // Traditional mode: use shared state
    nimcp_future_shared_state_t* shared = future->shared;
    if (!shared || shared->magic != FUTURE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_future_wait: shared is NULL");
        return false;
    }

    // Fast path: check if already ready
    uint32_t state = nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_ACQUIRE);
    if (state != NIMCP_FUTURE_PENDING) {
        nimcp_atomic_fetch_add_u64(&g_stats_waits_total, 1, NIMCP_MEMORY_ORDER_RELAXED);
        nimcp_atomic_fetch_add_u64(&g_stats_waits_immediate, 1, NIMCP_MEMORY_ORDER_RELAXED);
        return (state == NIMCP_FUTURE_COMPLETED);
    }

    // Slow path: wait on condition variable
    nimcp_platform_mutex_lock(&shared->mutex);

    while ((state = nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_ACQUIRE)) == NIMCP_FUTURE_PENDING) {
        nimcp_platform_cond_wait(&shared->cond, &shared->mutex);
    }

    nimcp_platform_mutex_unlock(&shared->mutex);

    // Update statistics
    uint64_t wait_end_ns = nimcp_platform_time_monotonic_ms() * 1000000ULL;
    uint64_t wait_time_ns = wait_end_ns - wait_start_ns;

    nimcp_atomic_fetch_add_u64(&g_stats_waits_total, 1, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_fetch_add_u64(&g_stats_waits_blocked, 1, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_fetch_add_u64(&g_stats_total_wait_time_ns, wait_time_ns, NIMCP_MEMORY_ORDER_RELAXED);

    return (state == NIMCP_FUTURE_COMPLETED);
}

bool nimcp_future_wait_timeout(nimcp_future_t future, uint32_t timeout_ms)
{
    /* Guard: validate future handle */
    if (!future || future->magic != FUTURE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_future_wait_timeout: future is NULL");
        return false;
    }

    /* Note: timeout_ms of 0 means immediate timeout (no wait).
     * Use nimcp_future_wait() for indefinite waiting. */
    if (timeout_ms == 0) {
        /* For zero timeout, just check current state without blocking */
        return nimcp_future_is_ready(future) &&
               (nimcp_future_state(future) == NIMCP_FUTURE_COMPLETED);
    }

    uint64_t wait_start_ns = nimcp_platform_time_monotonic_ms() * 1000000ULL;

    // Bio-async backend: use bio-future timed wait
    if (future->is_bio_mode) {
        if (!future->bio_future) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_future_wait_timeout: future->bio_future is NULL");
            return false;
        }

        nimcp_error_t err = nimcp_bio_future_wait(future->bio_future, NULL, (uint64_t)timeout_ms);

        // Update statistics
        uint64_t wait_end_ns = nimcp_platform_time_monotonic_ms() * 1000000ULL;
        uint64_t wait_time_ns = wait_end_ns - wait_start_ns;
        nimcp_atomic_fetch_add_u64(&g_stats_waits_total, 1, NIMCP_MEMORY_ORDER_RELAXED);

        if (err == NIMCP_SUCCESS) {
            nimcp_atomic_fetch_add_u64(&g_stats_waits_blocked, 1, NIMCP_MEMORY_ORDER_RELAXED);
            nimcp_atomic_fetch_add_u64(&g_stats_total_wait_time_ns, wait_time_ns, NIMCP_MEMORY_ORDER_RELAXED);
            return true;
        } else {
            // Could be timeout, failure, or decay
            nimcp_atomic_fetch_add_u64(&g_stats_waits_timeout, 1, NIMCP_MEMORY_ORDER_RELAXED);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_future_wait_timeout: validation failed");
            return false;
        }
    }

    // Traditional mode: use shared state
    nimcp_future_shared_state_t* shared = future->shared;
    if (!shared || shared->magic != FUTURE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_future_wait_timeout: shared is NULL");
        return false;
    }

    // Fast path: check if already ready
    uint32_t state = nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_ACQUIRE);
    if (state != NIMCP_FUTURE_PENDING) {
        nimcp_atomic_fetch_add_u64(&g_stats_waits_total, 1, NIMCP_MEMORY_ORDER_RELAXED);
        nimcp_atomic_fetch_add_u64(&g_stats_waits_immediate, 1, NIMCP_MEMORY_ORDER_RELAXED);
        return (state == NIMCP_FUTURE_COMPLETED);
    }

    // Slow path: timed wait on condition variable
    nimcp_platform_mutex_lock(&shared->mutex);

    int wait_result = 0;
    while ((state = nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_ACQUIRE)) == NIMCP_FUTURE_PENDING) {
        wait_result = nimcp_platform_cond_timedwait(&shared->cond, &shared->mutex, timeout_ms);

        if (wait_result == ETIMEDOUT) {
            break;
        }
    }

    nimcp_platform_mutex_unlock(&shared->mutex);

    // Update statistics
    uint64_t wait_end_ns = nimcp_platform_time_monotonic_ms() * 1000000ULL;
    uint64_t wait_time_ns = wait_end_ns - wait_start_ns;

    nimcp_atomic_fetch_add_u64(&g_stats_waits_total, 1, NIMCP_MEMORY_ORDER_RELAXED);

    if (wait_result == ETIMEDOUT) {
        /* P2-58 fix: Timeout is normal behavior, not an error. Removed false
         * positive NIMCP_THROW_TO_IMMUNE. The caller decides what to do on timeout. */
        nimcp_atomic_fetch_add_u64(&g_stats_waits_timeout, 1, NIMCP_MEMORY_ORDER_RELAXED);
        return false;
    }

    nimcp_atomic_fetch_add_u64(&g_stats_waits_blocked, 1, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_fetch_add_u64(&g_stats_total_wait_time_ns, wait_time_ns, NIMCP_MEMORY_ORDER_RELAXED);

    return (state == NIMCP_FUTURE_COMPLETED);
}

nimcp_error_t nimcp_future_get(nimcp_future_t future, void* out_result)
{
    NIMCP_CHECK_THROW(future && future->magic == FUTURE_MAGIC,
                       NIMCP_ERROR_NULL_POINTER, "future is NULL or invalid");
    NIMCP_CHECK_THROW(out_result != NULL, NIMCP_ERROR_NULL_POINTER, "out_result is NULL");

    /* P2-56 fix: Return clear error for bio-mode futures instead of crashing
     * on NULL shared state. Bio-mode futures use bio_future_get instead. */
    if (future->is_bio_mode) {
        LOG_ERROR("nimcp_future_get: bio-mode futures must use nimcp_bio_future_get()");
        return NIMCP_ERROR_INVALID_STATE;
    }

    nimcp_future_shared_state_t* shared = future->shared;
    NIMCP_CHECK_THROW(shared && shared->magic == FUTURE_MAGIC, NIMCP_ERROR_INVALID_STATE,
                      "nimcp_future_get_result: invalid shared state");

    // Check if completed
    uint32_t state = nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_ACQUIRE);
    NIMCP_CHECK_THROW(state == NIMCP_FUTURE_COMPLETED, NIMCP_ERROR_INVALID_STATE,
                      "nimcp_future_get_result: future not completed (state=%u)", state);

    // Copy result
    if (shared->result_size > 0 && shared->result) {
        memcpy(out_result, shared->result, shared->result_size);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_future_get_error(nimcp_future_t future)
{
    if (!future || future->magic != FUTURE_MAGIC) {
        return NIMCP_SUCCESS;
    }

    nimcp_future_shared_state_t* shared = future->shared;
    if (!shared || shared->magic != FUTURE_MAGIC) {
        return NIMCP_SUCCESS;
    }

    uint32_t state = nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_ACQUIRE);
    if (state != NIMCP_FUTURE_FAILED) {
        return NIMCP_SUCCESS;
    }

    return shared->error;
}

bool nimcp_future_is_ready(nimcp_future_t future)
{
    if (!future || future->magic != FUTURE_MAGIC) {
        return false;
    }

    // Bio-async backend: use bio-future ready check
    if (future->is_bio_mode) {
        if (!future->bio_future) {
            return false;
        }
        return nimcp_bio_future_is_ready(future->bio_future);
    }

    // Traditional mode: use shared state
    nimcp_future_shared_state_t* shared = future->shared;
    if (!shared || shared->magic != FUTURE_MAGIC) {
        return false;
    }

    uint32_t state = nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_ACQUIRE);
    return (state != NIMCP_FUTURE_PENDING);
}

bool nimcp_future_cancel(nimcp_future_t future)
{
    if (!future || future->magic != FUTURE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_future_cancel: future is NULL");
        return false;
    }

    // Bio-async backend: use bio-future cancel
    if (future->is_bio_mode) {
        if (!future->bio_future) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_future_cancel: future->bio_future is NULL");
            return false;
        }
        bool cancelled = nimcp_bio_future_cancel(future->bio_future);
        if (cancelled) {
            nimcp_atomic_fetch_add_u64(&g_stats_cancellations, 1, NIMCP_MEMORY_ORDER_RELAXED);
        }
        return cancelled;
    }

    // Traditional mode: use shared state
    nimcp_future_shared_state_t* shared = future->shared;
    if (!shared || shared->magic != FUTURE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_future_cancel: shared is NULL");
        return false;
    }

    // Try to transition from PENDING to CANCELLED (CAS operation)
    uint32_t expected = NIMCP_FUTURE_PENDING;
    if (!nimcp_atomic_compare_exchange_u32(&shared->state, &expected, NIMCP_FUTURE_CANCELLED,
                                           NIMCP_MEMORY_ORDER_ACQ_REL)) {
        // Already completed/failed/cancelled
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_future_cancel: shared is NULL");
        return false;
    }

    // Update statistics
    nimcp_atomic_fetch_add_u64(&g_stats_cancellations, 1, NIMCP_MEMORY_ORDER_RELAXED);

    // Wake all waiters and extract callback list for invocation outside mutex
    // WHY: Invoking callbacks while holding mutex can cause deadlocks if
    //      callbacks try to acquire other locks or re-enter the future API
    nimcp_platform_mutex_lock(&shared->mutex);
    nimcp_platform_cond_broadcast(&shared->cond);

    // Extract callback list while holding mutex
    callback_node_t* callbacks_to_invoke = shared->callbacks;
    shared->callbacks = NULL;  // Clear list so it won't be invoked again

    nimcp_platform_mutex_unlock(&shared->mutex);

    // Invoke callbacks AFTER releasing mutex to prevent deadlocks
    callback_node_t* cb = callbacks_to_invoke;
    while (cb) {
        if (cb->callback) {
            // For cancelled futures, pass NULL result and SUCCESS error
            // (cancellation is not a failure, it's a valid termination)
            cb->callback(NULL, NIMCP_SUCCESS, cb->user_data);
        }
        callback_node_t* next = cb->next;
        future_free(cb);
        cb = next;
    }

    return true;
}

void nimcp_future_destroy(nimcp_future_t future)
{
    if (!future || future->magic != FUTURE_MAGIC) {
        return;
    }

    LOG_DEBUG("Destroying future: %p (bio_mode=%d)", (void*)future, future->is_bio_mode);

    // Update statistics
    nimcp_atomic_fetch_add_u64(&g_stats_futures_destroyed, 1, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_fetch_sub_u64(&g_stats_active_futures, 1, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_fetch_sub_u64(&g_stats_total_memory_bytes,
                               sizeof(struct nimcp_future_struct),
                               NIMCP_MEMORY_ORDER_RELAXED);

    // Invalidate magic
    future->magic = 0;

    // Bio-async backend: destroy bio-future
    if (future->is_bio_mode) {
        if (future->bio_future) {
            nimcp_bio_future_destroy(future->bio_future);
            future->bio_future = NULL;
        }
    } else {
        // Traditional mode: release shared state
        if (future->shared) {
            shared_state_release(future->shared);
            future->shared = NULL;
        }
    }

    // Free future handle via unified memory wrapper
    future_free(future);
}

//=============================================================================
// Combinator API Implementation
//=============================================================================

nimcp_error_t nimcp_future_then(nimcp_future_t future, nimcp_future_callback_t callback, void* user_data)
{
    NIMCP_CHECK_THROW(future && future->magic == FUTURE_MAGIC,
                       NIMCP_ERROR_NULL_POINTER, "future is NULL or invalid");
    NIMCP_CHECK_THROW(callback != NULL, NIMCP_ERROR_NULL_POINTER, "callback is NULL");

    /* P2-57 fix: Return clear error for bio-mode futures.
     * Bio-mode futures use nimcp_bio_future_on_complete() for callbacks. */
    if (future->is_bio_mode) {
        LOG_ERROR("nimcp_future_then: bio-mode futures must use nimcp_bio_future_on_complete()");
        return NIMCP_ERROR_INVALID_STATE;
    }

    nimcp_future_shared_state_t* shared = future->shared;
    NIMCP_CHECK_THROW(shared && shared->magic == FUTURE_MAGIC, NIMCP_ERROR_INVALID_STATE,
                      "nimcp_future_then: invalid shared state");

    // Check if already completed (invoke immediately)
    uint32_t state = nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_ACQUIRE);
    if (state != NIMCP_FUTURE_PENDING) {
        const void* result = (state == NIMCP_FUTURE_COMPLETED) ? shared->result : NULL;
        nimcp_error_t error = (state == NIMCP_FUTURE_FAILED) ? shared->error : NIMCP_SUCCESS;

        callback(result, error, user_data);

        nimcp_atomic_fetch_add_u64(&g_stats_then_chains, 1, NIMCP_MEMORY_ORDER_RELAXED);
        return NIMCP_SUCCESS;
    }

    // Allocate callback node via unified memory wrapper
    callback_node_t* node = (callback_node_t*)future_alloc(sizeof(callback_node_t));
    NIMCP_CHECK_THROW(node != NULL, NIMCP_ERROR_NO_MEMORY, "Failed to allocate callback node");

    node->callback = callback;
    node->user_data = user_data;
    node->destructor = NULL;  // Regular then() callbacks don't need destructors
    node->next = NULL;

    // Add to callback list (thread-safe)
    nimcp_platform_mutex_lock(&shared->mutex);

    // Double-check state after acquiring lock
    state = nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_ACQUIRE);
    if (state != NIMCP_FUTURE_PENDING) {
        nimcp_platform_mutex_unlock(&shared->mutex);

        const void* result = (state == NIMCP_FUTURE_COMPLETED) ? shared->result : NULL;
        nimcp_error_t error = (state == NIMCP_FUTURE_FAILED) ? shared->error : NIMCP_SUCCESS;

        callback(result, error, user_data);
        future_free(node);

        nimcp_atomic_fetch_add_u64(&g_stats_then_chains, 1, NIMCP_MEMORY_ORDER_RELAXED);
        return NIMCP_SUCCESS;
    }

    // Prepend to callback list (faster than append)
    node->next = shared->callbacks;
    shared->callbacks = node;

    nimcp_platform_mutex_unlock(&shared->mutex);

    nimcp_atomic_fetch_add_u64(&g_stats_then_chains, 1, NIMCP_MEMORY_ORDER_RELAXED);
    return NIMCP_SUCCESS;
}

/**
 * WHAT: Internal helper to register callback with destructor
 * WHY:  Combinators need cleanup when callbacks aren't invoked
 * HOW:  Like nimcp_future_then but accepts a destructor function
 */
static nimcp_error_t future_then_with_destructor(
    nimcp_future_t future,
    nimcp_future_callback_t callback,
    void* user_data,
    callback_destructor_t destructor)
{
    NIMCP_CHECK_THROW(future && future->magic == FUTURE_MAGIC && callback,
                       NIMCP_ERROR_NULL_POINTER, "future or callback is NULL/invalid");

    nimcp_future_shared_state_t* shared = future->shared;
    NIMCP_CHECK_THROW(shared && shared->magic == FUTURE_MAGIC, NIMCP_ERROR_INVALID_STATE,
                      "nimcp_future_then_with_cleanup: invalid shared state");

    // Check if already completed (invoke immediately)
    uint32_t state = nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_ACQUIRE);
    if (state != NIMCP_FUTURE_PENDING) {
        const void* result = (state == NIMCP_FUTURE_COMPLETED) ? shared->result : NULL;
        nimcp_error_t error = (state == NIMCP_FUTURE_FAILED) ? shared->error : NIMCP_SUCCESS;
        callback(result, error, user_data);
        return NIMCP_SUCCESS;
    }

    // Allocate callback node via unified memory wrapper
    callback_node_t* node = (callback_node_t*)future_alloc(sizeof(callback_node_t));
    NIMCP_CHECK_THROW(node != NULL, NIMCP_ERROR_NO_MEMORY,
                       "Failed to allocate callback node with destructor");

    node->callback = callback;
    node->user_data = user_data;
    node->destructor = destructor;  // Set destructor for cleanup
    node->next = NULL;

    // Add to callback list (thread-safe)
    nimcp_platform_mutex_lock(&shared->mutex);

    // Double-check state after acquiring lock
    state = nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_ACQUIRE);
    if (state != NIMCP_FUTURE_PENDING) {
        nimcp_platform_mutex_unlock(&shared->mutex);
        const void* result = (state == NIMCP_FUTURE_COMPLETED) ? shared->result : NULL;
        nimcp_error_t error = (state == NIMCP_FUTURE_FAILED) ? shared->error : NIMCP_SUCCESS;
        callback(result, error, user_data);
        future_free(node);
        return NIMCP_SUCCESS;
    }

    // Prepend to callback list
    node->next = shared->callbacks;
    shared->callbacks = node;

    nimcp_platform_mutex_unlock(&shared->mutex);
    return NIMCP_SUCCESS;
}

/**
 * WHAT: Tracking structure for all() combinator
 * WHY:  Coordinate completion of multiple futures
 * HOW:  Atomic counter and result array with reference counting
 */
typedef struct {
    nimcp_promise_t combined_promise;
    bool* results;
    size_t count;
    nimcp_atomic_uint32_t remaining;
    nimcp_atomic_uint32_t refcount;         /**< Reference count for cleanup */
    nimcp_atomic_bool_t has_failure;        /**< Track if any future failed */
    nimcp_error_t first_error;              /**< First error encountered */
} all_tracker_t;

/**
 * WHAT: Per-callback context for all() combinator
 * WHY:  Pass index information to each callback
 * HOW:  Wrapper with tracker pointer and index
 */
typedef struct {
    all_tracker_t* tracker;
    size_t index;
} all_callback_ctx_t;

/**
 * WHAT: Release reference to all tracker
 * WHY:  Clean up when all references are gone
 * HOW:  Atomic decrement, free on zero
 */
static void all_tracker_release(all_tracker_t* tracker)
{
    uint32_t old_ref = nimcp_atomic_fetch_sub_u32(&tracker->refcount, 1, NIMCP_MEMORY_ORDER_ACQ_REL);
    if (old_ref == 1) {
        // Last reference - clean up
        nimcp_promise_destroy(tracker->combined_promise);
        future_free(tracker->results);
        future_free(tracker);
    }
}

/**
 * WHAT: Destructor for all_callback_ctx_t
 * WHY:  Clean up when callback is not invoked (future destroyed while pending)
 * HOW:  Free ctx and release tracker reference
 */
static void all_callback_destructor(void* user_data)
{
    all_callback_ctx_t* ctx = (all_callback_ctx_t*)user_data;
    all_tracker_release(ctx->tracker);
    future_free(ctx);
}

/**
 * WHAT: Callback for all() combinator
 * WHY:  Triggered when each input future completes
 * HOW:  Decrements counter, completes when all done
 */
static void all_callback(const void* result, nimcp_error_t error, void* user_data)
{
    all_callback_ctx_t* ctx = (all_callback_ctx_t*)user_data;
    all_tracker_t* tracker = ctx->tracker;
    size_t index = ctx->index;
    (void)result;  // Unused

    // Record success/failure for this specific index
    tracker->results[index] = (error == NIMCP_SUCCESS);

    // Track first failure
    if (error != NIMCP_SUCCESS) {
        bool expected = false;
        if (nimcp_atomic_compare_exchange_bool(&tracker->has_failure, &expected, true,
                                               NIMCP_MEMORY_ORDER_ACQ_REL)) {
            tracker->first_error = error;
        }
    }

    // Free callback context
    future_free(ctx);

    // Decrement remaining count
    uint32_t old_remaining = nimcp_atomic_fetch_sub_u32(&tracker->remaining, 1,
                                                         NIMCP_MEMORY_ORDER_ACQ_REL);

    // If this was the last future, complete the combined promise
    if (old_remaining == 1) {
        bool any_failed = nimcp_atomic_load_bool(&tracker->has_failure, NIMCP_MEMORY_ORDER_ACQUIRE);
        if (!any_failed) {
            nimcp_promise_complete(tracker->combined_promise, tracker->results);
        } else {
            nimcp_promise_fail(tracker->combined_promise, tracker->first_error);
        }
    }

    // Release our reference to tracker
    all_tracker_release(tracker);
}

nimcp_future_t nimcp_future_all(nimcp_future_t* futures, size_t count)
{
    if (!futures || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_future_all: futures is NULL");
        return NULL;
    }

    // Create promise for combined result
    nimcp_promise_t combined = nimcp_promise_create(count * sizeof(bool));
    if (!combined) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combined is NULL");

        return NULL;
    }

    // Get future from promise before registering callbacks
    nimcp_future_t result_future = nimcp_promise_get_future(combined);
    if (!result_future) {
        nimcp_promise_destroy(combined);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_future_all: result_future is NULL");
        return NULL;
    }

    // Allocate tracking structure via unified memory wrapper
    all_tracker_t* tracker = (all_tracker_t*)future_alloc(sizeof(all_tracker_t));
    if (!tracker) {
        LOG_ERROR("Failed to allocate all_tracker_t");
        nimcp_future_destroy(result_future);
        nimcp_promise_destroy(combined);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_future_all: tracker is NULL");
        return NULL;
    }

    // Allocate and zero-initialize results array
    tracker->results = (bool*)future_alloc(count * sizeof(bool));
    if (!tracker->results) {
        LOG_ERROR("Failed to allocate results array for all()");
        future_free(tracker);
        nimcp_future_destroy(result_future);
        nimcp_promise_destroy(combined);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_future_all: tracker->results is NULL");
        return NULL;
    }
    memset(tracker->results, 0, count * sizeof(bool));  // Zero-init like calloc

    tracker->combined_promise = combined;
    tracker->count = count;
    nimcp_atomic_init_u32(&tracker->remaining, (uint32_t)count);
    // Initialize refcount to count (one per callback)
    nimcp_atomic_init_u32(&tracker->refcount, (uint32_t)count);
    nimcp_atomic_init_bool(&tracker->has_failure, false);
    tracker->first_error = NIMCP_SUCCESS;

    // Register callbacks on all input futures with per-callback context
    for (size_t i = 0; i < count; i++) {
        all_callback_ctx_t* ctx = (all_callback_ctx_t*)future_alloc(sizeof(all_callback_ctx_t));
        if (!ctx) {
            LOG_ERROR("Failed to allocate all_callback_ctx_t");
            // On allocation failure, release unused references and fail
            for (size_t j = i; j < count; j++) {
                all_tracker_release(tracker);
            }
            nimcp_future_destroy(result_future);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_future_all: ctx is NULL");
            return NULL;
        }
        ctx->tracker = tracker;
        ctx->index = i;
        future_then_with_destructor(futures[i], all_callback, ctx, all_callback_destructor);
    }

    // Update statistics
    nimcp_atomic_fetch_add_u64(&g_stats_all_operations, 1, NIMCP_MEMORY_ORDER_RELAXED);

    // Don't destroy promise here - tracker owns it
    return result_future;
}

/**
 * WHAT: Tracking structure for any() combinator
 * WHY:  Race multiple futures to first completion
 * HOW:  Atomic flag for first winner with reference counting
 */
typedef struct {
    nimcp_promise_t combined_promise;
    nimcp_atomic_bool_t first_completed;
    nimcp_atomic_uint32_t refcount;         /**< Reference count for cleanup */
    size_t count;                           /**< Total number of futures */
} any_tracker_t;

/**
 * WHAT: Per-callback context for any() combinator
 * WHY:  Pass index information to each callback
 * HOW:  Wrapper with tracker pointer and index
 */
typedef struct {
    any_tracker_t* tracker;
    size_t index;
} any_callback_ctx_t;

/**
 * WHAT: Release reference to any tracker
 * WHY:  Clean up when all references are gone
 * HOW:  Atomic decrement, free on zero
 */
static void any_tracker_release(any_tracker_t* tracker)
{
    uint32_t old_ref = nimcp_atomic_fetch_sub_u32(&tracker->refcount, 1, NIMCP_MEMORY_ORDER_ACQ_REL);
    if (old_ref == 1) {
        // Last reference - clean up
        nimcp_promise_destroy(tracker->combined_promise);
        future_free(tracker);
    }
}

/**
 * WHAT: Destructor for any_callback_ctx_t
 * WHY:  Clean up when callback is not invoked (future destroyed while pending)
 * HOW:  Free ctx and release tracker reference
 */
static void any_callback_destructor(void* user_data)
{
    any_callback_ctx_t* ctx = (any_callback_ctx_t*)user_data;
    any_tracker_release(ctx->tracker);
    future_free(ctx);
}

/**
 * WHAT: Callback for any() combinator
 * WHY:  Triggered when each input future completes
 * HOW:  First to complete wins, completes combined promise
 */
static void any_callback(const void* result, nimcp_error_t error, void* user_data)
{
    any_callback_ctx_t* ctx = (any_callback_ctx_t*)user_data;
    any_tracker_t* tracker = ctx->tracker;
    size_t my_index = ctx->index;
    (void)result;  // Unused

    // Free callback context first
    future_free(ctx);

    // Try to be the first to complete
    bool expected = false;
    if (nimcp_atomic_compare_exchange_bool(&tracker->first_completed, &expected, true,
                                          NIMCP_MEMORY_ORDER_ACQ_REL)) {
        // We won! Complete the combined promise with our index
        if (error == NIMCP_SUCCESS) {
            nimcp_promise_complete(tracker->combined_promise, &my_index);
        } else {
            nimcp_promise_fail(tracker->combined_promise, error);
        }
    }

    // Release our reference to tracker
    any_tracker_release(tracker);
}

nimcp_future_t nimcp_future_any(nimcp_future_t* futures, size_t count)
{
    if (!futures || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_future_any: futures is NULL");
        return NULL;
    }

    // Create promise for combined result (index of first completer)
    nimcp_promise_t combined = nimcp_promise_create(sizeof(size_t));
    if (!combined) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combined is NULL");

        return NULL;
    }

    // Get future from promise
    nimcp_future_t result_future = nimcp_promise_get_future(combined);
    if (!result_future) {
        nimcp_promise_destroy(combined);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_future_any: result_future is NULL");
        return NULL;
    }

    // Allocate tracking structure via unified memory wrapper
    any_tracker_t* tracker = (any_tracker_t*)future_alloc(sizeof(any_tracker_t));
    if (!tracker) {
        LOG_ERROR("Failed to allocate any_tracker_t");
        nimcp_future_destroy(result_future);
        nimcp_promise_destroy(combined);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_future_any: tracker is NULL");
        return NULL;
    }

    tracker->combined_promise = combined;
    nimcp_atomic_init_bool(&tracker->first_completed, false);
    // Initialize refcount to count (one per callback)
    nimcp_atomic_init_u32(&tracker->refcount, (uint32_t)count);
    tracker->count = count;

    // Register callbacks on all input futures with per-callback context
    for (size_t i = 0; i < count; i++) {
        any_callback_ctx_t* ctx = (any_callback_ctx_t*)future_alloc(sizeof(any_callback_ctx_t));
        if (!ctx) {
            LOG_ERROR("Failed to allocate any_callback_ctx_t");
            // On allocation failure, release unused references and fail
            for (size_t j = i; j < count; j++) {
                any_tracker_release(tracker);
            }
            nimcp_future_destroy(result_future);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_future_any: ctx is NULL");
            return NULL;
        }
        ctx->tracker = tracker;
        ctx->index = i;
        future_then_with_destructor(futures[i], any_callback, ctx, any_callback_destructor);
    }

    // Update statistics
    nimcp_atomic_fetch_add_u64(&g_stats_any_operations, 1, NIMCP_MEMORY_ORDER_RELAXED);

    return result_future;
}

/**
 * WHAT: Tracking structure for map() combinator
 * WHY:  Transform future result when it completes
 * HOW:  Apply transformation function asynchronously with reference counting
 */
typedef struct {
    nimcp_promise_t output_promise;
    nimcp_future_transform_t transform;
    void* user_data;
    size_t output_size;
    nimcp_atomic_uint32_t refcount;  /**< Reference count for safe cleanup */
} map_tracker_t;

/**
 * WHAT: Release reference to map tracker
 * WHY:  Clean up when all references are gone (prevents double-free)
 * HOW:  Atomic decrement, free on zero
 */
static void map_tracker_release(map_tracker_t* tracker)
{
    uint32_t old_ref = nimcp_atomic_fetch_sub_u32(&tracker->refcount, 1, NIMCP_MEMORY_ORDER_ACQ_REL);
    if (old_ref == 1) {
        // Last reference - clean up
        nimcp_promise_destroy(tracker->output_promise);
        future_free(tracker);
    }
}

/**
 * WHAT: Destructor for map_tracker_t
 * WHY:  Clean up when callback is not invoked (future destroyed while pending)
 * HOW:  Release tracker reference (may free if last reference)
 */
static void map_callback_destructor(void* user_data)
{
    map_tracker_t* tracker = (map_tracker_t*)user_data;
    map_tracker_release(tracker);
}

/**
 * WHAT: Callback for map() combinator
 * WHY:  Triggered when source future completes
 * HOW:  Applies transformation, completes output promise
 */
static void map_callback(const void* result, nimcp_error_t error, void* user_data)
{
    map_tracker_t* tracker = (map_tracker_t*)user_data;

    if (error != NIMCP_SUCCESS) {
        // Source future failed - propagate error
        nimcp_promise_fail(tracker->output_promise, error);
    } else {
        // Allocate output buffer via unified memory wrapper
        void* output = future_alloc(tracker->output_size);
        if (!output) {
            LOG_ERROR("Failed to allocate output buffer for map() transform");
            nimcp_promise_fail(tracker->output_promise, NIMCP_ERROR_NO_MEMORY);
        } else {
            // Apply transformation
            nimcp_error_t transform_error = tracker->transform(result, output, tracker->user_data);

            if (transform_error == NIMCP_SUCCESS) {
                nimcp_promise_complete(tracker->output_promise, output);
            } else {
                nimcp_promise_fail(tracker->output_promise, transform_error);
            }

            future_free(output);
        }
    }

    // Release our reference to tracker (may free if last reference)
    map_tracker_release(tracker);
}

nimcp_future_t nimcp_future_map(
    nimcp_future_t source,
    nimcp_future_transform_t transform,
    size_t output_size,
    void* user_data)
{
    if (!source || !transform || output_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_future_map: required parameter is NULL (source, transform)");
        return NULL;
    }

    // Create promise for transformed result
    nimcp_promise_t output_promise = nimcp_promise_create(output_size);
    if (!output_promise) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "output_promise is NULL");

        return NULL;
    }

    // Get future from promise
    nimcp_future_t result_future = nimcp_promise_get_future(output_promise);
    if (!result_future) {
        nimcp_promise_destroy(output_promise);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_future_map: result_future is NULL");
        return NULL;
    }

    // Allocate tracking structure via unified memory wrapper
    map_tracker_t* tracker = (map_tracker_t*)future_alloc(sizeof(map_tracker_t));
    if (!tracker) {
        LOG_ERROR("Failed to allocate map_tracker_t");
        nimcp_future_destroy(result_future);
        nimcp_promise_destroy(output_promise);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_future_map: tracker is NULL");
        return NULL;
    }

    tracker->output_promise = output_promise;
    tracker->transform = transform;
    tracker->user_data = user_data;
    tracker->output_size = output_size;
    // Initialize refcount to 1 (one reference for the callback/destructor)
    nimcp_atomic_init_u32(&tracker->refcount, 1);

    // Register callback on source future with destructor for cleanup
    future_then_with_destructor(source, map_callback, tracker, map_callback_destructor);

    // Update statistics
    nimcp_atomic_fetch_add_u64(&g_stats_map_operations, 1, NIMCP_MEMORY_ORDER_RELAXED);

    // Return future from output promise
    return result_future;
}

//=============================================================================
// Statistics API Implementation
//=============================================================================

nimcp_error_t nimcp_future_get_stats(nimcp_future_stats_t* stats)
{
    NIMCP_CHECK_THROW(stats != NULL, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    // Read atomic counters with relaxed ordering (statistics don't need strict ordering)
    stats->promises_created = nimcp_atomic_load_u64(&g_stats_promises_created, NIMCP_MEMORY_ORDER_RELAXED);
    stats->promises_destroyed = nimcp_atomic_load_u64(&g_stats_promises_destroyed, NIMCP_MEMORY_ORDER_RELAXED);
    stats->futures_created = nimcp_atomic_load_u64(&g_stats_futures_created, NIMCP_MEMORY_ORDER_RELAXED);
    stats->futures_destroyed = nimcp_atomic_load_u64(&g_stats_futures_destroyed, NIMCP_MEMORY_ORDER_RELAXED);
    stats->active_promises = nimcp_atomic_load_u64(&g_stats_active_promises, NIMCP_MEMORY_ORDER_RELAXED);
    stats->active_futures = nimcp_atomic_load_u64(&g_stats_active_futures, NIMCP_MEMORY_ORDER_RELAXED);

    stats->completions = nimcp_atomic_load_u64(&g_stats_completions, NIMCP_MEMORY_ORDER_RELAXED);
    stats->failures = nimcp_atomic_load_u64(&g_stats_failures, NIMCP_MEMORY_ORDER_RELAXED);
    stats->cancellations = nimcp_atomic_load_u64(&g_stats_cancellations, NIMCP_MEMORY_ORDER_RELAXED);

    stats->waits_total = nimcp_atomic_load_u64(&g_stats_waits_total, NIMCP_MEMORY_ORDER_RELAXED);
    stats->waits_immediate = nimcp_atomic_load_u64(&g_stats_waits_immediate, NIMCP_MEMORY_ORDER_RELAXED);
    stats->waits_blocked = nimcp_atomic_load_u64(&g_stats_waits_blocked, NIMCP_MEMORY_ORDER_RELAXED);
    stats->waits_timeout = nimcp_atomic_load_u64(&g_stats_waits_timeout, NIMCP_MEMORY_ORDER_RELAXED);

    stats->then_chains = nimcp_atomic_load_u64(&g_stats_then_chains, NIMCP_MEMORY_ORDER_RELAXED);
    stats->all_operations = nimcp_atomic_load_u64(&g_stats_all_operations, NIMCP_MEMORY_ORDER_RELAXED);
    stats->any_operations = nimcp_atomic_load_u64(&g_stats_any_operations, NIMCP_MEMORY_ORDER_RELAXED);
    stats->map_operations = nimcp_atomic_load_u64(&g_stats_map_operations, NIMCP_MEMORY_ORDER_RELAXED);

    stats->total_wait_time_ns = nimcp_atomic_load_u64(&g_stats_total_wait_time_ns, NIMCP_MEMORY_ORDER_RELAXED);
    stats->total_completion_time_ns = nimcp_atomic_load_u64(&g_stats_total_completion_time_ns, NIMCP_MEMORY_ORDER_RELAXED);

    stats->total_memory_bytes = (size_t)nimcp_atomic_load_u64(&g_stats_total_memory_bytes, NIMCP_MEMORY_ORDER_RELAXED);
    stats->result_memory_bytes = (size_t)nimcp_atomic_load_u64(&g_stats_result_memory_bytes, NIMCP_MEMORY_ORDER_RELAXED);

    stats->refcount_increments = nimcp_atomic_load_u64(&g_stats_refcount_increments, NIMCP_MEMORY_ORDER_RELAXED);
    stats->refcount_decrements = nimcp_atomic_load_u64(&g_stats_refcount_decrements, NIMCP_MEMORY_ORDER_RELAXED);

    // Calculate derived statistics
    if (stats->waits_blocked > 0) {
        stats->avg_wait_time_ns = stats->total_wait_time_ns / stats->waits_blocked;
    } else {
        stats->avg_wait_time_ns = 0;
    }

    if (stats->completions > 0) {
        stats->avg_completion_time_ns = stats->total_completion_time_ns / stats->completions;
    } else {
        stats->avg_completion_time_ns = 0;
    }

    // Calculate shared futures
    stats->shared_futures = stats->active_futures;

    // Calculate memory saved (rough estimate based on result sharing)
    stats->memory_saved_bytes = 0;

    return NIMCP_SUCCESS;
}

void nimcp_future_reset_stats(void)
{
    nimcp_atomic_store_u64(&g_stats_promises_created, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&g_stats_promises_destroyed, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&g_stats_futures_created, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&g_stats_futures_destroyed, 0, NIMCP_MEMORY_ORDER_RELAXED);

    // Also reset active counters (for test isolation)
    nimcp_atomic_store_u64(&g_stats_active_promises, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&g_stats_active_futures, 0, NIMCP_MEMORY_ORDER_RELAXED);

    nimcp_atomic_store_u64(&g_stats_completions, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&g_stats_failures, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&g_stats_cancellations, 0, NIMCP_MEMORY_ORDER_RELAXED);

    nimcp_atomic_store_u64(&g_stats_waits_total, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&g_stats_waits_immediate, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&g_stats_waits_blocked, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&g_stats_waits_timeout, 0, NIMCP_MEMORY_ORDER_RELAXED);

    nimcp_atomic_store_u64(&g_stats_then_chains, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&g_stats_all_operations, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&g_stats_any_operations, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&g_stats_map_operations, 0, NIMCP_MEMORY_ORDER_RELAXED);

    nimcp_atomic_store_u64(&g_stats_total_wait_time_ns, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&g_stats_total_completion_time_ns, 0, NIMCP_MEMORY_ORDER_RELAXED);

    nimcp_atomic_store_u64(&g_stats_total_memory_bytes, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&g_stats_result_memory_bytes, 0, NIMCP_MEMORY_ORDER_RELAXED);

    nimcp_atomic_store_u64(&g_stats_refcount_increments, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&g_stats_refcount_decrements, 0, NIMCP_MEMORY_ORDER_RELAXED);

    LOG_DEBUG("Future statistics reset");
}

//=============================================================================
// Module Initialization API Implementation
//=============================================================================

nimcp_error_t nimcp_future_init(void* security_ctx, void* memory_mgr)
{
    // Thread-safe check: already initialized
    if (nimcp_atomic_load_bool(&g_future_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        LOG_WARNING("Future module already initialized");
        return NIMCP_SUCCESS;  // Idempotent
    }

    // Thread-safe init guard: only one thread can initialize
    bool expected = false;
    if (!nimcp_atomic_compare_exchange_bool(&g_future_initializing, &expected, true,
                                            NIMCP_MEMORY_ORDER_ACQ_REL)) {
        // Another thread is initializing, wait for it to complete
        while (!nimcp_atomic_load_bool(&g_future_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
            // Spin-wait (could use a proper wait mechanism)
        }
        return NIMCP_SUCCESS;
    }

    LOG_INFO("Initializing async/futures module");

    // Store memory manager reference
    g_future_memory_mgr = (unified_mem_manager_t)memory_mgr;
    if (g_future_memory_mgr) {
        LOG_INFO("Future module using unified memory manager");
    } else {
        LOG_DEBUG("Future module using standard malloc/free");
    }

    // Store security context
    g_future_security_ctx = (nimcp_sec_integration_t*)security_ctx;

    // Register with security module if context provided
    if (g_future_security_ctx) {
        int result = nimcp_sec_register_module(
            g_future_security_ctx,
            "async_futures",
            NIMCP_SEC_CAT_UTILITY,
            &g_future_security_module_id
        );

        if (result != NIMCP_SUCCESS) {
            LOG_ERROR("Failed to register async/futures module with security");
            nimcp_atomic_store_bool(&g_future_initializing, false, NIMCP_MEMORY_ORDER_RELEASE);
            return (nimcp_error_t)result;
        }

        LOG_INFO("Future module registered with security (ID: %u)", g_future_security_module_id);
    }

    nimcp_atomic_store_bool(&g_future_initialized, true, NIMCP_MEMORY_ORDER_RELEASE);
    LOG_INFO("Async/futures module initialized successfully");
    return NIMCP_SUCCESS;
}

/**
 * @brief Initialize futures module with bio-async backend
 *
 * WHAT: Initializes futures with biological async mechanisms
 * WHY:  Enable neuromodulator-based signaling, phase coupling, predictive coding
 * HOW:  Initialize both traditional and bio-async systems, enable bio backend
 *
 * @param security_ctx Security integration context
 * @param memory_mgr Unified memory manager
 * @param bio_config Bio-async configuration (NULL for defaults)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_future_init_bio(
    void* security_ctx,
    void* memory_mgr,
    const void* bio_config)
{
    // First initialize traditional futures
    nimcp_error_t err = nimcp_future_init(security_ctx, memory_mgr);
    if (err != NIMCP_SUCCESS) {
        return err;
    }

    // Initialize bio-async system
    err = nimcp_bio_async_init((const nimcp_bio_async_config_t*)bio_config);
    if (err != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to initialize bio-async backend: %d", err);
        nimcp_future_shutdown();
        return err;
    }

    // Enable bio-async backend
    g_bio_async_backend_enabled = true;
    LOG_INFO("Bio-async backend enabled for futures module");

    return NIMCP_SUCCESS;
}

/**
 * @brief Enable or disable bio-async backend
 *
 * @param enable true to enable, false to disable
 * @return NIMCP_SUCCESS or error if bio-async not initialized
 */
nimcp_error_t nimcp_future_set_bio_backend(bool enable)
{
    NIMCP_CHECK_THROW(nimcp_atomic_load_bool(&g_future_initialized, NIMCP_MEMORY_ORDER_ACQUIRE),
                      NIMCP_ERROR_INVALID_STATE,
                      "nimcp_future_set_bio_backend: future module not initialized");

    NIMCP_CHECK_THROW(!enable || nimcp_bio_async_is_initialized(),
                      NIMCP_BIO_ERROR_NOT_INITIALIZED,
                      "nimcp_future_set_bio_backend: bio-async not initialized");

    g_bio_async_backend_enabled = enable;
    LOG_INFO("Bio-async backend %s", enable ? "enabled" : "disabled");
    return NIMCP_SUCCESS;
}

/**
 * @brief Check if bio-async backend is enabled
 *
 * @return true if bio-async backend is active
 */
bool nimcp_future_is_bio_backend_enabled(void)
{
    return g_bio_async_backend_enabled && nimcp_bio_async_is_initialized();
}

void nimcp_future_shutdown(void)
{
    if (!nimcp_atomic_load_bool(&g_future_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        return;
    }

    LOG_INFO("Shutting down async/futures module");

    // Check for leaked resources
    uint64_t active_promises = nimcp_atomic_load_u64(&g_stats_active_promises, NIMCP_MEMORY_ORDER_RELAXED);
    uint64_t active_futures = nimcp_atomic_load_u64(&g_stats_active_futures, NIMCP_MEMORY_ORDER_RELAXED);

    if (active_promises > 0 || active_futures > 0) {
        LOG_WARNING("Future module shutdown with active resources: %llu promises, %llu futures",
                    (unsigned long long)active_promises, (unsigned long long)active_futures);
    }

    // Shutdown bio-async backend if enabled
    if (g_bio_async_backend_enabled) {
        LOG_INFO("Shutting down bio-async backend");
        nimcp_bio_async_shutdown();
        g_bio_async_backend_enabled = false;
    }

    // Unregister from security
    if (g_future_security_ctx && g_future_security_module_id != 0) {
        nimcp_sec_unregister_module(g_future_security_ctx, g_future_security_module_id);
        LOG_INFO("Future module unregistered from security");
    }

    // Clear state
    g_future_security_ctx = NULL;
    g_future_security_module_id = 0;
    g_future_memory_mgr = NULL;
    nimcp_atomic_store_bool(&g_future_initialized, false, NIMCP_MEMORY_ORDER_RELEASE);
    nimcp_atomic_store_bool(&g_future_initializing, false, NIMCP_MEMORY_ORDER_RELEASE);

    LOG_INFO("Async/futures module shutdown complete");
}

bool nimcp_future_is_initialized(void)
{
    return nimcp_atomic_load_bool(&g_future_initialized, NIMCP_MEMORY_ORDER_ACQUIRE);
}

uint32_t nimcp_future_get_security_id(void)
{
    return g_future_security_module_id;
}

//=============================================================================
// Knowledge Graph Self-Awareness Integration
//=============================================================================

/**
 * @brief Query self-knowledge from the knowledge graph
 *
 * WHAT: Retrieves structural self-knowledge about the Async_Future module
 * WHY:  Enables runtime introspection and self-awareness capabilities
 * HOW:  Queries KG for Async_Future entity and logs observations/relations
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge was found, 0 otherwise
 */
int async_future_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Async_Future");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            LOG_DEBUG("Async_Future self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Async_Future");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Async_Future");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
