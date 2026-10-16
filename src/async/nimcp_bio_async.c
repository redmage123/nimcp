/**
 * @file nimcp_bio_async.c
 * @brief Biologically-Inspired Asynchronous Computation System Implementation
 *
 * WHAT: Implementation of bio-async using NIMCP utilities
 * WHY:  Biologically realistic async with proper memory and threading
 * HOW:  Uses unified memory, thread pool, atomics, rwlocks
 *
 * IMPLEMENTATION NOTES:
 * - All allocations use unified memory for CoW support
 * - Thread pool for async workers
 * - RW locks for concurrent read access
 * - Atomics for lock-free hot paths
 * - Cache-line aligned structures
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 1.0.0
 */

#include "async/nimcp_bio_async.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_biological_timescales.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/thread/nimcp_thread_pool.h"
#include "utils/thread/nimcp_atomic.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "security/nimcp_security.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/platform/nimcp_platform_once.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
//=============================================================================
// Module Logging Configuration
//=============================================================================

/** Module name for logging */
#define LOG_MODULE "bio_async"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(bio_async)

#define BIO_ASYNC_MODULE "bio_async"

/** Enable verbose trace logging (set to 0 for production) */
#ifndef BIO_ASYNC_VERBOSE_TRACE
#define BIO_ASYNC_VERBOSE_TRACE 0
#endif

/** Conditional trace logging for hot paths */
#if BIO_ASYNC_VERBOSE_TRACE
#define BIO_TRACE(...) LOG_TRACE(__VA_ARGS__)
#else
#define BIO_TRACE(...) ((void)0)
#endif

//=============================================================================
// Constants
//=============================================================================

#define BIO_MAGIC_PROMISE 0x42494F50  /* 'BIOP' */
#define BIO_MAGIC_FUTURE  0x42494F46  /* 'BIOF' */
#define BIO_MAGIC_PHASE   0x42494F53  /* 'BIOS' */
#define BIO_MAGIC_GLIAL   0x42494F47  /* 'BIOG' */
#define BIO_MAGIC_PREDICT 0x42494F52  /* 'BIOR' */

#define BIO_MAX_CALLBACKS 32
#define BIO_MAX_OSCILLATORS 256
#define BIO_MAX_REGIONS 1024
#define BIO_MAX_SIGNAL_NAME 64

/** Maximum tracked unified memory handles */
#define BIO_MAX_TRACKED_HANDLES 4096

/** Confidence threshold below which future is considered decayed */
#define BIO_CONFIDENCE_THRESHOLD 0.05f

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Callback node for bio-future
 */
typedef struct bio_callback_node {
    nimcp_bio_callback_t callback;
    void* user_data;
    struct bio_callback_node* next;
} bio_callback_node_t;

/**
 * @brief Shared state for bio-promise/future pair
 */
typedef struct nimcp_bio_shared_state {
    uint32_t magic;

    /* Channel info */
    nimcp_bio_channel_type_t channel;

    /* State machine (atomic) */
    nimcp_atomic_uint32_t state;

    /* Reference counting (atomic) */
    nimcp_atomic_uint32_t refcount;

    /* Concentration tracking (atomic float via uint32) */
    nimcp_atomic_uint32_t concentration_bits;  /**< Float as bits for atomics */

    /* Timing */
    uint64_t create_time_ms;
    uint64_t complete_time_ms;

    /* Result storage */
    void* result;
    size_t result_size;
    nimcp_error_t error;

    /* Synchronization */
    nimcp_mutex_t mutex;
    nimcp_cond_t cond;

    /* Callbacks */
    bio_callback_node_t* callbacks;

} __attribute__((aligned(BIO_CACHE_LINE_SIZE))) nimcp_bio_shared_state_t;

/**
 * @brief Bio-promise structure
 */
struct nimcp_bio_promise_struct {
    uint32_t magic;
    nimcp_bio_shared_state_t* shared;
};

/**
 * @brief Bio-future structure
 */
struct nimcp_bio_future_struct {
    uint32_t magic;
    nimcp_bio_shared_state_t* shared;
};

/**
 * @brief Oscillator for phase coupling
 */
typedef struct {
    float phase;              /**< Current phase [0, 2π] */
    float natural_freq;       /**< Natural frequency (radians/ms) */
    nimcp_bio_future_t future; /**< Associated future */
    bool completed;           /**< Future has completed */
} oscillator_t;

/**
 * @brief Phase sync structure
 */
struct nimcp_phase_sync_struct {
    uint32_t magic;
    nimcp_oscillation_band_t band;

    /* Oscillators */
    oscillator_t* oscillators;
    size_t count;
    size_t capacity;

    /* Kuramoto parameters */
    float coupling_K;
    float coherence_threshold;

    /* Cached order parameter */
    nimcp_atomic_uint32_t order_r_bits;
    nimcp_atomic_uint32_t mean_phase_bits;

    /* Synchronization */
    nimcp_rwlock_t rwlock;
    nimcp_cond_t cond;
    nimcp_mutex_t cond_mutex;
};

/**
 * @brief Predictive model callback entry
 */
typedef struct predict_callback_entry {
    nimcp_prediction_error_callback_t callback;
    void* user_data;
    float surprise_threshold;
    struct predict_callback_entry* next;
} predict_callback_entry_t;

/**
 * @brief Predictive model structure
 */
struct nimcp_predictive_model_struct {
    uint32_t magic;

    char signal_name[BIO_MAX_SIGNAL_NAME];

    /* Bayesian state */
    float prediction;
    float precision;
    float last_surprise;
    float learning_rate;

    /* Callbacks */
    predict_callback_entry_t* callbacks;

    /* Synchronization */
    nimcp_rwlock_t rwlock;
};

/**
 * @brief Region calcium state for glial wave
 */
typedef struct {
    float calcium;         /**< Calcium concentration (μM) */
    float ip3;             /**< IP3 concentration (μM) */
    bool reached;          /**< Wave has reached this region */
    nimcp_wave_callback_t callback;
    void* callback_data;
} region_state_t;

/**
 * @brief Glial wave structure
 */
struct nimcp_glial_wave_struct {
    uint32_t magic;

    uint32_t source_region;
    float initial_calcium;
    uint64_t start_time_ms;

    /* Region states */
    region_state_t* regions;
    size_t num_regions;

    /* Wave properties */
    float radius;
    float speed;
    bool active;

    /* Synchronization */
    nimcp_rwlock_t rwlock;
    nimcp_cond_t cond;
    nimcp_mutex_t cond_mutex;
};

//=============================================================================
// Handle Tracking for Unified Memory
//=============================================================================

/**
 * @brief Entry for tracking unified memory handle-to-pointer mapping
 */
typedef struct {
    void* ptr;                      /**< Pointer returned to caller */
    unified_mem_handle_t handle;    /**< Corresponding unified memory handle */
} bio_handle_entry_t;

/**
 * @brief Handle tracker for unified memory
 */
typedef struct {
    bio_handle_entry_t entries[BIO_MAX_TRACKED_HANDLES];
    nimcp_atomic_uint32_t count;
    nimcp_mutex_t mutex;
    bool initialized;
} bio_handle_tracker_t;

static bio_handle_tracker_t g_handle_tracker = {0};

/** P1-48 fix: Use platform_once for thread-safe one-time initialization */
static nimcp_platform_once_t g_handle_tracker_once = NIMCP_PLATFORM_ONCE_INIT;

/**
 * @brief One-time initialization routine for handle tracker
 *
 * P1-48 fix: Called via nimcp_platform_once to ensure exactly one thread
 * initializes the tracker, eliminating the TOCTOU race on mutex init.
 */
static void handle_tracker_init_once_routine(void) {
    nimcp_error_t err = nimcp_mutex_init(&g_handle_tracker.mutex, NULL);
    if (err != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to initialize handle tracker mutex: %d", err);
        return;
    }

    nimcp_atomic_init_u32(&g_handle_tracker.count, 0);
    memset(g_handle_tracker.entries, 0, sizeof(g_handle_tracker.entries));
    g_handle_tracker.initialized = true;

    LOG_INFO("Handle tracker initialized successfully");
}

/**
 * @brief Initialize handle tracker
 *
 * P1-48 fix: Uses nimcp_platform_once for thread-safe one-time initialization,
 * replacing the previous double-checked locking pattern which had a TOCTOU race
 * on the mutex init itself.
 */
static nimcp_error_t handle_tracker_init(void) {
    nimcp_platform_once(&g_handle_tracker_once, handle_tracker_init_once_routine);

    if (!g_handle_tracker.initialized) {
        LOG_ERROR("Handle tracker initialization failed");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Shutdown handle tracker
 */
static void handle_tracker_shutdown(void) {
    if (!g_handle_tracker.initialized) {
        LOG_DEBUG("Handle tracker not initialized, skipping shutdown");
        return;
    }

    LOG_DEBUG("Shutting down handle tracker");

    nimcp_mutex_lock(&g_handle_tracker.mutex);

    /* Free any remaining tracked handles */
    uint32_t freed_count = 0;
    for (uint32_t i = 0; i < BIO_MAX_TRACKED_HANDLES; i++) {
        if (g_handle_tracker.entries[i].handle) {
            unified_mem_free(g_handle_tracker.entries[i].handle);
            freed_count++;
        }
    }

    if (freed_count > 0) {
        LOG_WARNING("Handle tracker shutdown freed %u leaked handles", freed_count);
    }

    /* Zero out all entries to prevent stale data on re-init.
     * This is critical because handle_tracker_init checks initialized flag
     * but doesn't re-zero entries if they contain stale pointers. */
    memset(g_handle_tracker.entries, 0, sizeof(g_handle_tracker.entries));
    nimcp_atomic_init_u32(&g_handle_tracker.count, 0);

    // P2 fix: Set initialized=false BEFORE destroying mutex
    // This prevents other threads from trying to use the mutex after destruction
    g_handle_tracker.initialized = false;

    nimcp_mutex_unlock(&g_handle_tracker.mutex);
    nimcp_mutex_destroy(&g_handle_tracker.mutex);

    /* P1-48 fix: Reset platform_once so handle tracker can be re-initialized */
    g_handle_tracker_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;

    LOG_INFO("Handle tracker shutdown complete");
}

/**
 * @brief Register a pointer-handle mapping
 * @return true on success, false if table full
 */
static bool handle_tracker_register(void* ptr, unified_mem_handle_t handle) {
    if (!g_handle_tracker.initialized) {
        LOG_DEBUG("Handle tracker not initialized, cannot register %p", ptr);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "handle_tracker_register: g_handle_tracker is NULL");
        return false;
    }
    if (!ptr || !handle) {
        LOG_WARNING("Invalid arguments to handle_tracker_register: ptr=%p, handle=%p", ptr, (void*)handle);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "handle_tracker_register: required parameter is NULL (ptr, handle)");
        return false;
    }

    nimcp_mutex_lock(&g_handle_tracker.mutex);

    /* Find empty slot */
    for (uint32_t i = 0; i < BIO_MAX_TRACKED_HANDLES; i++) {
        if (g_handle_tracker.entries[i].ptr == NULL) {
            g_handle_tracker.entries[i].ptr = ptr;
            g_handle_tracker.entries[i].handle = handle;
            uint32_t new_count = nimcp_atomic_fetch_add_u32(&g_handle_tracker.count, 1, NIMCP_MEMORY_ORDER_RELAXED) + 1;
            nimcp_mutex_unlock(&g_handle_tracker.mutex);
            BIO_TRACE("Registered handle %p -> ptr %p (total: %u)", (void*)handle, ptr, new_count);
            return true;
        }
    }

    /* Track overflow occurrences for monitoring */
    static nimcp_atomic_uint32_t s_overflow_count = {0};
    uint32_t overflow_count = nimcp_atomic_fetch_add_u32(&s_overflow_count, 1, NIMCP_MEMORY_ORDER_RELAXED) + 1;

    nimcp_mutex_unlock(&g_handle_tracker.mutex);

    /* Log with overflow count to help diagnose if this is a transient or persistent issue */
    LOG_ERROR("Handle tracker full (%d entries) - cannot track allocation at %p "
              "(overflow count: %u, consider increasing BIO_MAX_TRACKED_HANDLES)",
              BIO_MAX_TRACKED_HANDLES, ptr, overflow_count);
    return false;
}

/**
 * @brief Find and remove a handle by pointer
 * @return The handle if found, NULL otherwise
 */
static unified_mem_handle_t handle_tracker_remove(void* ptr) {
    if (!g_handle_tracker.initialized) {
        BIO_TRACE("Handle tracker not initialized, ptr %p assumed malloc'd", ptr);
        return NULL;
    }
    if (!ptr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ptr is NULL");

        return NULL;
    }

    nimcp_mutex_lock(&g_handle_tracker.mutex);

    for (uint32_t i = 0; i < BIO_MAX_TRACKED_HANDLES; i++) {
        if (g_handle_tracker.entries[i].ptr == ptr) {
            unified_mem_handle_t handle = g_handle_tracker.entries[i].handle;
            g_handle_tracker.entries[i].ptr = NULL;
            g_handle_tracker.entries[i].handle = NULL;
            uint32_t remaining = nimcp_atomic_fetch_sub_u32(&g_handle_tracker.count, 1, NIMCP_MEMORY_ORDER_RELAXED) - 1;
            nimcp_mutex_unlock(&g_handle_tracker.mutex);
            BIO_TRACE("Removed handle %p for ptr %p (remaining: %u)", (void*)handle, ptr, remaining);
            return handle;
        }
    }

    nimcp_mutex_unlock(&g_handle_tracker.mutex);
    BIO_TRACE("Pointer %p not in handle tracker, assuming malloc'd", ptr);
    return NULL;  /* Not found - allocated via nimcp_malloc */
}

//=============================================================================
// Global State
//=============================================================================

/**
 * @brief Per-channel state tracking for refractory period enforcement
 */
typedef struct {
    uint64_t last_completion_ms;     /**< Time of last promise completion */
    nimcp_mutex_t mutex;             /**< Protects last_completion_ms */
} bio_channel_state_t;

static struct {
    _Atomic bool initialized;  /* P2-63 fix: atomic to prevent torn reads across threads */
    nimcp_bio_async_config_t config;

    /* Memory management */
    unified_mem_manager_t mem_mgr;

    /* Thread pool */
    nimcp_thread_pool_t* thread_pool;

    /* Statistics */
    nimcp_bio_async_stats_t stats;
    nimcp_rwlock_t stats_lock;

    /* Per-channel state for refractory period tracking */
    bio_channel_state_t channel_state[BIO_CHANNEL_COUNT];

    /* Simulation time */
    uint64_t simulation_time_ms;
    nimcp_mutex_t time_mutex;

} g_bio_async = {0};

//=============================================================================
// Helper Functions - Memory
//=============================================================================

/**
 * @brief Allocate memory using unified memory if available
 *
 * Uses handle tracker to properly map pointers back to unified memory handles
 * for correct deallocation.
 */
static void* bio_alloc(size_t size) {
    BIO_TRACE("bio_alloc: requesting %zu bytes", size);

    if (g_bio_async.mem_mgr && g_handle_tracker.initialized) {
        unified_mem_request_t req = unified_mem_request_direct(size);
        unified_mem_handle_t handle = unified_mem_alloc(g_bio_async.mem_mgr, &req);
        if (handle) {
            void* ptr = (void*)unified_mem_write(handle);
            if (ptr) {
                if (handle_tracker_register(ptr, handle)) {
                    BIO_TRACE("bio_alloc: unified memory allocated %zu bytes at %p", size, ptr);
                    return ptr;
                }
                /* Failed to register - free handle and fallback to malloc */
                LOG_WARNING("bio_alloc: handle registration failed, falling back to malloc");
                unified_mem_free(handle);
            } else {
                LOG_WARNING("bio_alloc: unified_mem_write returned NULL");
                unified_mem_free(handle);
            }
        } else {
            LOG_DEBUG("bio_alloc: unified_mem_alloc failed for %zu bytes, using malloc", size);
        }
    }

    void* ptr = nimcp_malloc(size);
    BIO_TRACE("bio_alloc: malloc allocated %zu bytes at %p", size, ptr);
    return ptr;
}

/**
 * @brief Allocate aligned memory
 *
 * Uses handle tracker for unified memory allocations.
 */
static void* bio_aligned_alloc(size_t alignment, size_t size) {
    BIO_TRACE("bio_aligned_alloc: requesting %zu bytes with alignment %zu", size, alignment);

    if (g_bio_async.mem_mgr && g_handle_tracker.initialized) {
        unified_mem_request_t req = {
            .size = size,
            .initial_data = NULL,
            .strategy = UNIFIED_STRATEGY_POOL_DIRECT,
            .enable_cow = false,
            .alignment = alignment
        };
        unified_mem_handle_t handle = unified_mem_alloc(g_bio_async.mem_mgr, &req);
        if (handle) {
            void* ptr = (void*)unified_mem_write(handle);
            if (ptr) {
                if (handle_tracker_register(ptr, handle)) {
                    BIO_TRACE("bio_aligned_alloc: unified memory allocated %zu bytes at %p", size, ptr);
                    return ptr;
                }
                /* Failed to register - free handle and fallback to malloc */
                LOG_WARNING("bio_aligned_alloc: handle registration failed, falling back to aligned_alloc");
                unified_mem_free(handle);
            } else {
                LOG_WARNING("bio_aligned_alloc: unified_mem_write returned NULL");
                unified_mem_free(handle);
            }
        } else {
            LOG_DEBUG("bio_aligned_alloc: unified_mem_alloc failed, using aligned_alloc");
        }
    }

    void* ptr = nimcp_aligned_alloc(alignment, size);
    BIO_TRACE("bio_aligned_alloc: aligned_alloc allocated %zu bytes at %p", size, ptr);
    return ptr;
}

/**
 * @brief Free memory
 *
 * Checks handle tracker for unified memory allocations, otherwise uses nimcp_free.
 */
static void bio_free(void* ptr) {
    if (!ptr) return;

    BIO_TRACE("bio_free: freeing %p", ptr);

    /* Check if this pointer was allocated via unified memory */
    unified_mem_handle_t handle = handle_tracker_remove(ptr);
    if (handle) {
        /* Unified memory - use proper free */
        BIO_TRACE("bio_free: using unified_mem_free for %p", ptr);
        unified_mem_free(handle);
    } else {
        /* Regular allocation - use nimcp_free */
        BIO_TRACE("bio_free: using nimcp_free for %p", ptr);
        nimcp_free(ptr);
    }
}

/**
 * @brief Atomic float load (via bit cast)
 */
static inline float atomic_load_float(nimcp_atomic_uint32_t* bits) {
    uint32_t b = nimcp_atomic_load_u32(bits, NIMCP_MEMORY_ORDER_ACQUIRE);
    float f;
    memcpy(&f, &b, sizeof(f));
    return f;
}

/**
 * @brief Atomic float store (via bit cast)
 */
static inline void atomic_store_float(nimcp_atomic_uint32_t* bits, float f) {
    uint32_t b;
    memcpy(&b, &f, sizeof(b));
    nimcp_atomic_store_u32(bits, b, NIMCP_MEMORY_ORDER_RELEASE);
}

/**
 * @brief Get current time in milliseconds
 * WHAT: Return simulation time or real time based on config
 * WHY:  Allow deterministic testing with nimcp_bio_async_step() when use_real_time=false
 * HOW:  Check use_real_time config flag to select time source
 *
 * NOTE: When use_real_time=true (default), we use real wall-clock time.
 *       This is critical for timeout handling in wait functions - otherwise
 *       timeouts would never trigger unless nimcp_bio_async_step() is called.
 */
static inline uint64_t bio_time_ms(void) {
    if (g_bio_async.initialized && !g_bio_async.config.use_real_time) {
        /* Use simulation time for deterministic decay calculations
         * Only when explicitly configured to NOT use real time */
        return g_bio_async.simulation_time_ms;
    }
    return nimcp_platform_time_monotonic_ms();
}

//=============================================================================
// Shared State Management
//=============================================================================

static nimcp_bio_shared_state_t* shared_state_create(
    nimcp_bio_channel_type_t channel,
    size_t result_size)
{
    LOG_DEBUG("Creating shared state for channel %s with result_size %zu",
              nimcp_bio_channel_name(channel), result_size);

    nimcp_bio_shared_state_t* shared = (nimcp_bio_shared_state_t*)
        bio_aligned_alloc(BIO_CACHE_LINE_SIZE, sizeof(nimcp_bio_shared_state_t));
    if (!shared) {
        LOG_ERROR("Failed to allocate bio shared state (%zu bytes)",
                  sizeof(nimcp_bio_shared_state_t));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "shared_state_create: shared is NULL");
        return NULL;
    }

    memset(shared, 0, sizeof(nimcp_bio_shared_state_t));
    shared->magic = BIO_MAGIC_PROMISE;
    shared->channel = channel;

    /* Initialize atomics */
    nimcp_atomic_init_u32(&shared->state, BIO_FUTURE_PENDING);
    nimcp_atomic_init_u32(&shared->refcount, 1);

    /* Initialize concentration at baseline */
    float baseline;
    switch (channel) {
        case BIO_CHANNEL_DOPAMINE: baseline = BIO_DA_BASELINE_UM; break;
        case BIO_CHANNEL_SEROTONIN: baseline = BIO_5HT_BASELINE_UM; break;
        case BIO_CHANNEL_NOREPINEPHRINE: baseline = BIO_NE_BASELINE_UM; break;
        case BIO_CHANNEL_ACETYLCHOLINE: baseline = BIO_ACH_BASELINE_UM; break;
        default: baseline = 0.0F;
    }
    atomic_store_float(&shared->concentration_bits, baseline);
    LOG_DEBUG("Shared state baseline concentration: %.4f µM", baseline);

    /* Initialize timing */
    shared->create_time_ms = bio_time_ms();
    shared->complete_time_ms = 0;

    /* Initialize result */
    shared->result = NULL;
    shared->result_size = result_size;
    shared->error = NIMCP_SUCCESS;

    /* Initialize synchronization */
    if (nimcp_mutex_init(&shared->mutex, NULL) != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to initialize shared state mutex");
        bio_free(shared);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "shared_state_create: validation failed");
        return NULL;
    }
    if (nimcp_cond_init(&shared->cond) != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to initialize shared state condition variable");
        nimcp_mutex_destroy(&shared->mutex);
        bio_free(shared);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "shared_state_create: validation failed");
        return NULL;
    }

    shared->callbacks = NULL;

    LOG_DEBUG("Shared state created at %p for channel %s",
              (void*)shared, nimcp_bio_channel_name(channel));
    return shared;
}

static void shared_state_addref(nimcp_bio_shared_state_t* shared) {
    if (!shared || shared->magic != BIO_MAGIC_PROMISE) {
        LOG_WARNING("shared_state_addref: invalid shared state %p", (void*)shared);
        return;
    }
    uint32_t new_ref = nimcp_atomic_fetch_add_u32(&shared->refcount, 1, NIMCP_MEMORY_ORDER_ACQ_REL) + 1;
    BIO_TRACE("shared_state_addref: %p refcount now %u", (void*)shared, new_ref);
}

static void shared_state_release(nimcp_bio_shared_state_t* shared) {
    if (!shared || shared->magic != BIO_MAGIC_PROMISE) {
        LOG_WARNING("shared_state_release: invalid shared state %p", (void*)shared);
        return;
    }

    uint32_t old_ref = nimcp_atomic_fetch_sub_u32(&shared->refcount, 1, NIMCP_MEMORY_ORDER_ACQ_REL);
    BIO_TRACE("shared_state_release: %p refcount %u -> %u", (void*)shared, old_ref, old_ref - 1);

    if (old_ref == 1) {
        /* Last reference - cleanup */
        LOG_DEBUG("shared_state_release: destroying shared state %p (channel %s)",
                  (void*)shared, nimcp_bio_channel_name(shared->channel));

        if (shared->result) {
            BIO_TRACE("shared_state_release: freeing result buffer");
            bio_free(shared->result);
        }

        /* Free callbacks */
        uint32_t cb_count = 0;
        bio_callback_node_t* cb = shared->callbacks;
        while (cb) {
            bio_callback_node_t* next = cb->next;
            bio_free(cb);
            cb = next;
            cb_count++;
        }
        if (cb_count > 0) {
            LOG_DEBUG("shared_state_release: freed %u callback nodes", cb_count);
        }

        nimcp_cond_destroy(&shared->cond);
        nimcp_mutex_destroy(&shared->mutex);

        shared->magic = 0;
        bio_free(shared);
        LOG_DEBUG("shared_state_release: shared state destroyed");
    }
}

/**
 * @brief Update concentration based on decay
 */
static float shared_state_update_concentration(nimcp_bio_shared_state_t* shared) {
    uint32_t state = nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_ACQUIRE);
    if (state != BIO_FUTURE_COMPLETED) {
        return 0.0F;
    }

    float elapsed_ms = (float)(bio_time_ms() - shared->complete_time_ms);
    float tau = nimcp_bio_channel_decay_tau(shared->channel);

    /* Get peak and baseline for this channel */
    float peak, baseline;
    switch (shared->channel) {
        case BIO_CHANNEL_DOPAMINE:
            peak = BIO_DA_PEAK_PHASIC_UM;
            baseline = BIO_DA_BASELINE_UM;
            break;
        case BIO_CHANNEL_SEROTONIN:
            peak = 1.0F;
            baseline = BIO_5HT_BASELINE_UM;
            break;
        case BIO_CHANNEL_NOREPINEPHRINE:
            peak = 1.0F;
            baseline = BIO_NE_BASELINE_UM;
            break;
        case BIO_CHANNEL_ACETYLCHOLINE:
            peak = 1.0F;
            baseline = BIO_ACH_BASELINE_UM;
            break;
        default:
            peak = 1.0F;
            baseline = 0.0F;
    }

    /* Exponential decay: c(t) = baseline + (peak - baseline) * exp(-t/tau) */
    float concentration = baseline + (peak - baseline) * bio_exponential_decay(1.0F, elapsed_ms, tau);
    atomic_store_float(&shared->concentration_bits, concentration);

    /* Check for full decay */
    /* P1-47 fix: Guard against division by zero when peak == baseline */
    if (peak == baseline) {
        /* No decay possible when peak equals baseline - confidence is 1.0 (fully decayed) */
        return concentration;
    }
    float confidence = (concentration - baseline) / (peak - baseline);
    if (confidence < BIO_CONFIDENCE_THRESHOLD) {
        uint32_t expected = BIO_FUTURE_COMPLETED;
        nimcp_atomic_compare_exchange_u32(&shared->state, &expected, BIO_FUTURE_DECAYED,
                                          NIMCP_MEMORY_ORDER_ACQ_REL);
    }

    return concentration;
}

/**
 * @brief Invoke all registered callbacks
 * NOTE: P1-24 fix: This function MUST be called WITHOUT holding shared->mutex.
 *       Callers must unlock the mutex before calling this function.
 *       This prevents deadlock if callbacks try to acquire other locks.
 */
static void shared_state_invoke_callbacks(nimcp_bio_shared_state_t* shared) {
    uint32_t state = nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_ACQUIRE);
    float concentration = atomic_load_float(&shared->concentration_bits);

    /* Calculate confidence */
    float peak, baseline;
    switch (shared->channel) {
        case BIO_CHANNEL_DOPAMINE:
            peak = BIO_DA_PEAK_PHASIC_UM;
            baseline = BIO_DA_BASELINE_UM;
            break;
        default:
            peak = 1.0F;
            baseline = 0.0F;
    }
    float confidence = (peak > baseline) ? (concentration - baseline) / (peak - baseline) : 0.0F;
    if (confidence < 0.0F) confidence = 0.0F;
    if (confidence > 1.0F) confidence = 1.0F;

    nimcp_error_t error = (state == BIO_FUTURE_FAILED) ? shared->error : NIMCP_SUCCESS;
    const void* result = (state == BIO_FUTURE_COMPLETED) ? shared->result : NULL;

    /* Copy callback list under lock, then invoke outside lock.
     * Callers unlock before calling this function (P1-24 fix), so we must
     * re-acquire the mutex to safely traverse the callback linked list. */
    bio_callback_node_t* callbacks_copy[BIO_MAX_CALLBACKS];
    void* userdata_copy[BIO_MAX_CALLBACKS];
    uint32_t callback_count = 0;

    nimcp_mutex_lock(&shared->mutex);
    bio_callback_node_t* cb = shared->callbacks;
    while (cb && callback_count < BIO_MAX_CALLBACKS) {
        if (cb->callback) {
            callbacks_copy[callback_count] = cb;
            userdata_copy[callback_count] = cb->user_data;
            callback_count++;
        }
        cb = cb->next;
    }
    nimcp_mutex_unlock(&shared->mutex);

    /* Invoke callbacks outside the lock to prevent deadlock */
    for (uint32_t i = 0; i < callback_count; i++) {
        callbacks_copy[i]->callback(result, confidence, error, userdata_copy[i]);
    }
}

//=============================================================================
// Module Initialization
//=============================================================================

nimcp_bio_async_config_t nimcp_bio_async_default_config(void) {
    nimcp_bio_async_config_t config = {0};

    /* Channel configurations with computational scaling */
    config.channel_configs[BIO_CHANNEL_DOPAMINE] = (nimcp_channel_config_t){
        .baseline_concentration = BIO_DA_BASELINE_UM,
        .peak_concentration = BIO_DA_PEAK_PHASIC_UM,
        .decay_tau_ms = BIO_COMP_DA_DECAY_TAU_MS,
        .diffusion_coef = BIO_DA_DIFFUSION_COEF,
        .refractory_period_ms = 10.0F,
        .enable_diffusion = false
    };

    config.channel_configs[BIO_CHANNEL_SEROTONIN] = (nimcp_channel_config_t){
        .baseline_concentration = BIO_5HT_BASELINE_UM,
        .peak_concentration = 1.0F,
        .decay_tau_ms = BIO_COMP_5HT_DECAY_TAU_MS,
        .diffusion_coef = BIO_5HT_DIFFUSION_COEF,
        .refractory_period_ms = 100.0F,
        .enable_diffusion = false
    };

    config.channel_configs[BIO_CHANNEL_NOREPINEPHRINE] = (nimcp_channel_config_t){
        .baseline_concentration = BIO_NE_BASELINE_UM,
        .peak_concentration = 1.0F,
        .decay_tau_ms = BIO_COMP_NE_DECAY_TAU_MS,
        .diffusion_coef = BIO_NE_DIFFUSION_COEF,
        .refractory_period_ms = 20.0F,
        .enable_diffusion = false
    };

    config.channel_configs[BIO_CHANNEL_ACETYLCHOLINE] = (nimcp_channel_config_t){
        .baseline_concentration = BIO_ACH_BASELINE_UM,
        .peak_concentration = 1.0F,
        .decay_tau_ms = BIO_COMP_ACH_DECAY_TAU_MS,
        .diffusion_coef = BIO_ACH_DIFFUSION_COEF,
        .refractory_period_ms = 5.0F,
        .enable_diffusion = false
    };

    /* Phase coupling */
    config.phase_config = (nimcp_phase_config_t){
        .coherence_threshold = BIO_PHASE_COHERENCE_THRESHOLD,
        .coupling_strength = BIO_KURAMOTO_K_GAMMA,
        .frequency_spread = 0.1F,
        .max_oscillators = BIO_MAX_OSCILLATORS,
        .enable_cross_frequency = false
    };

    /* Predictive coding */
    config.predictive_config = (nimcp_predictive_config_t){
        .default_prior_precision = BIO_PRED_PRIOR_PRECISION,
        .default_likelihood_precision = BIO_PRED_LIKELIHOOD_PRECISION,
        .learning_rate = BIO_PRED_PRECISION_LEARNING_RATE,
        .surprise_threshold = BIO_PRED_SURPRISE_THRESHOLD,
        .max_predictors = 256
    };

    /* Glial signaling */
    config.glial_config = (nimcp_glial_config_t){
        .wave_speed_um_s = BIO_COMP_CA_WAVE_SPEED,
        .wave_threshold_um = BIO_CA_WAVE_THRESHOLD_UM,
        .decay_rate = 0.1F,
        .max_concurrent_waves = 16,
        .mode = BIO_WAVE_ISOTROPIC
    };

    /* Threading */
    config.thread_pool_size = BIO_DEFAULT_THREAD_POOL_SIZE;
    config.enable_thread_affinity = false;

    /* Memory */
    config.max_memory_bytes = 0;  /* Unlimited */
    config.use_unified_memory = true;

    /* Timing */
    config.simulation_dt_ms = 1.0F;
    config.use_real_time = true;
    config.time_acceleration = 1.0F;

    /* Debug */
    config.enable_statistics = true;
    config.enable_logging = true;

    return config;
}

nimcp_error_t nimcp_bio_async_init(const nimcp_bio_async_config_t* config) {
    if (g_bio_async.initialized) {
        LOG_WARNING("Bio-async already initialized");
        return NIMCP_SUCCESS;
    }

    LOG_INFO("Initializing bio-async system");

    /* Use provided config or defaults */
    if (config) {
        g_bio_async.config = *config;
    } else {
        g_bio_async.config = nimcp_bio_async_default_config();
    }

    /* Initialize handle tracker (must be before memory manager) */
    nimcp_error_t tracker_err = handle_tracker_init();
    if (tracker_err != NIMCP_SUCCESS) {
        LOG_WARNING("Failed to init handle tracker, unified memory disabled");
    }

    /* Initialize memory manager */
    if (g_bio_async.config.use_unified_memory && g_handle_tracker.initialized) {
        unified_mem_config_t mem_config = unified_mem_default_config();
        g_bio_async.mem_mgr = unified_mem_create(&mem_config);
        if (!g_bio_async.mem_mgr) {
            LOG_WARNING("Failed to create unified memory manager, using malloc");
        }
    }

    /* Initialize thread pool */
    size_t pool_size = g_bio_async.config.thread_pool_size;
    if (pool_size == 0) {
        pool_size = BIO_DEFAULT_THREAD_POOL_SIZE;
    }
    g_bio_async.thread_pool = nimcp_pool_create(pool_size);
    if (!g_bio_async.thread_pool) {
        LOG_ERROR("Failed to create thread pool");
        if (g_bio_async.mem_mgr) {
            unified_mem_destroy(g_bio_async.mem_mgr);
            g_bio_async.mem_mgr = NULL;
        }
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_THREAD_CREATE,
                          "nimcp_bio_async_init: thread pool creation failed");
    }

    /* Initialize stats lock */
    if (nimcp_rwlock_init(&g_bio_async.stats_lock) != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to create stats rwlock");
        nimcp_pool_destroy(g_bio_async.thread_pool);
        if (g_bio_async.mem_mgr) {
            unified_mem_destroy(g_bio_async.mem_mgr);
        }
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_MUTEX_INIT,
                          "nimcp_bio_async_init: stats rwlock init failed");
    }

    /* Initialize time mutex */
    if (nimcp_mutex_init(&g_bio_async.time_mutex, NULL) != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to create time mutex");
        nimcp_rwlock_destroy(&g_bio_async.stats_lock);
        nimcp_pool_destroy(g_bio_async.thread_pool);
        if (g_bio_async.mem_mgr) {
            unified_mem_destroy(g_bio_async.mem_mgr);
        }
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_MUTEX_INIT,
                          "nimcp_bio_async_init: time mutex init failed");
    }

    /* Initialize per-channel state mutexes for refractory period tracking */
    for (int i = 0; i < BIO_CHANNEL_COUNT; i++) {
        if (nimcp_mutex_init(&g_bio_async.channel_state[i].mutex, NULL) != NIMCP_SUCCESS) {
            LOG_ERROR("Failed to create channel state mutex for channel %d", i);
            /* Cleanup previously initialized mutexes */
            for (int j = 0; j < i; j++) {
                nimcp_mutex_destroy(&g_bio_async.channel_state[j].mutex);
            }
            nimcp_mutex_destroy(&g_bio_async.time_mutex);
            nimcp_rwlock_destroy(&g_bio_async.stats_lock);
            nimcp_pool_destroy(g_bio_async.thread_pool);
            if (g_bio_async.mem_mgr) {
                unified_mem_destroy(g_bio_async.mem_mgr);
            }
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_MUTEX_INIT,
                              "nimcp_bio_async_init: channel state mutex init failed");
        }
        g_bio_async.channel_state[i].last_completion_ms = 0;
    }

    /* Clear statistics */
    memset(&g_bio_async.stats, 0, sizeof(g_bio_async.stats));
    g_bio_async.simulation_time_ms = 0;

    g_bio_async.initialized = true;
    LOG_INFO("Bio-async initialized with %zu threads", pool_size);

    return NIMCP_SUCCESS;
}

void nimcp_bio_async_shutdown(void) {
    if (!g_bio_async.initialized) {
        return;
    }

    LOG_INFO("Shutting down bio-async system");

    /* Destroy thread pool */
    if (g_bio_async.thread_pool) {
        nimcp_pool_destroy(g_bio_async.thread_pool);
        g_bio_async.thread_pool = NULL;
    }

    /* Destroy locks */
    nimcp_rwlock_destroy(&g_bio_async.stats_lock);
    nimcp_mutex_destroy(&g_bio_async.time_mutex);

    /* Destroy per-channel state mutexes */
    for (int i = 0; i < BIO_CHANNEL_COUNT; i++) {
        nimcp_mutex_destroy(&g_bio_async.channel_state[i].mutex);
    }

    /* Shutdown handle tracker (frees any remaining tracked handles) */
    handle_tracker_shutdown();

    /* Destroy memory manager */
    if (g_bio_async.mem_mgr) {
        unified_mem_destroy(g_bio_async.mem_mgr);
        g_bio_async.mem_mgr = NULL;
    }

    g_bio_async.initialized = false;
    LOG_INFO("Bio-async shutdown complete");
}

bool nimcp_bio_async_is_initialized(void) {
    return g_bio_async.initialized;
}

nimcp_error_t nimcp_bio_async_get_stats(nimcp_bio_async_stats_t* stats) {
    NIMCP_CHECK_THROW(stats != NULL, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    nimcp_rwlock_rdlock(&g_bio_async.stats_lock);
    *stats = g_bio_async.stats;
    nimcp_rwlock_unlock(&g_bio_async.stats_lock);

    return NIMCP_SUCCESS;
}

void nimcp_bio_async_reset_stats(void) {
    nimcp_rwlock_wrlock(&g_bio_async.stats_lock);
    memset(&g_bio_async.stats, 0, sizeof(g_bio_async.stats));
    nimcp_rwlock_unlock(&g_bio_async.stats_lock);
}

nimcp_error_t nimcp_bio_async_step(float dt_ms) {
    NIMCP_CHECK_THROW(g_bio_async.initialized, NIMCP_BIO_ERROR_NOT_INITIALIZED,
                      "nimcp_bio_async_step: bio-async system not initialized");

    if (dt_ms <= 0.0F) {
        dt_ms = g_bio_async.config.simulation_dt_ms;
    }

    nimcp_mutex_lock(&g_bio_async.time_mutex);
    g_bio_async.simulation_time_ms += (uint64_t)dt_ms;
    nimcp_mutex_unlock(&g_bio_async.time_mutex);

    /* Update statistics */
    nimcp_rwlock_wrlock(&g_bio_async.stats_lock);
    g_bio_async.stats.simulation_steps++;
    g_bio_async.stats.simulation_time_ms += dt_ms;
    nimcp_rwlock_unlock(&g_bio_async.stats_lock);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Bio-Promise API
//=============================================================================

nimcp_bio_promise_t nimcp_bio_promise_create(
    nimcp_bio_channel_type_t channel,
    size_t result_size)
{
    if (!g_bio_async.initialized) {
        LOG_ERROR("Bio-async not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_bio_promise_create: g_bio_async is NULL");
        return NULL;
    }

    if (channel >= BIO_CHANNEL_COUNT) {
        LOG_ERROR("Invalid channel type: %d", channel);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_bio_promise_create: capacity exceeded");
        return NULL;
    }

    nimcp_bio_promise_t promise = (nimcp_bio_promise_t)
        bio_alloc(sizeof(struct nimcp_bio_promise_struct));
    if (!promise) {
        LOG_ERROR("Failed to allocate bio promise");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_bio_promise_create: promise is NULL");
        return NULL;
    }

    nimcp_bio_shared_state_t* shared = shared_state_create(channel, result_size);
    if (!shared) {
        bio_free(promise);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_bio_promise_create: shared is NULL");
        return NULL;
    }

    promise->magic = BIO_MAGIC_PROMISE;
    promise->shared = shared;

    /* Update statistics */
    nimcp_rwlock_wrlock(&g_bio_async.stats_lock);
    g_bio_async.stats.total_futures_created++;
    g_bio_async.stats.channel_stats[channel].active_futures++;
    nimcp_rwlock_unlock(&g_bio_async.stats_lock);

    LOG_DEBUG("Created bio promise for channel %s", nimcp_bio_channel_name(channel));
    return promise;
}

nimcp_error_t nimcp_bio_promise_complete_sized(
    nimcp_bio_promise_t promise,
    const void* result,
    size_t result_size)
{
    LOG_DEBUG("nimcp_bio_promise_complete_sized: promise=%p, result=%p, size=%zu",
              (void*)promise, result, result_size);

    NIMCP_CHECK_THROW(promise && promise->magic == BIO_MAGIC_PROMISE,
                       NIMCP_ERROR_NULL_POINTER, "nimcp_bio_promise_complete_sized: invalid promise");

    nimcp_bio_shared_state_t* shared = promise->shared;
    NIMCP_CHECK_THROW(shared && shared->magic == BIO_MAGIC_PROMISE,
                      NIMCP_ERROR_INVALID_STATE,
                      "nimcp_bio_promise_complete_sized: invalid shared state");

    /* Validate result */
    NIMCP_CHECK_THROW(result_size == 0 || result != NULL,
                       NIMCP_ERROR_NULL_POINTER, "nimcp_bio_promise_complete_sized: result required but NULL");

    /* Enforce refractory period: check if minimum time has passed since last completion */
    nimcp_bio_channel_type_t channel = shared->channel;
    if (channel < BIO_CHANNEL_COUNT) {
        float refractory_ms = g_bio_async.config.channel_configs[channel].refractory_period_ms;
        if (refractory_ms > 0.0F) {
            uint64_t current_time = bio_time_ms();
            nimcp_mutex_lock(&g_bio_async.channel_state[channel].mutex);
            uint64_t last_completion = g_bio_async.channel_state[channel].last_completion_ms;
            uint64_t elapsed = current_time - last_completion;
            if (last_completion > 0 && elapsed < (uint64_t)refractory_ms) {
                nimcp_mutex_unlock(&g_bio_async.channel_state[channel].mutex);
                LOG_DEBUG("nimcp_bio_promise_complete: refractory period not elapsed "
                          "(channel=%s, elapsed=%lu ms, required=%.1f ms)",
                          nimcp_bio_channel_name(channel), (unsigned long)elapsed, refractory_ms);
                NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE,
                                  "nimcp_bio_promise_complete: refractory period not elapsed");
            }
            nimcp_mutex_unlock(&g_bio_async.channel_state[channel].mutex);
        }
    }

    /* Determine actual copy size:
     * - If shared->result_size was set at creation, use min of that and result_size
     * - If shared->result_size is 0, use result_size (handler provides size)
     */
    size_t copy_size = result_size;
    if (shared->result_size > 0 && result_size > shared->result_size) {
        copy_size = shared->result_size;  // Cap to allocated capacity
        LOG_DEBUG("nimcp_bio_promise_complete_sized: capping copy to capacity %zu", copy_size);
    }

    /* Copy result */
    if (copy_size > 0) {
        shared->result = bio_alloc(copy_size);
        NIMCP_CHECK_THROW(shared->result != NULL, NIMCP_ERROR_NO_MEMORY,
                          "nimcp_bio_promise_complete_sized: failed to allocate result buffer (%zu bytes)", copy_size);
        memcpy(shared->result, result, copy_size);
        shared->result_size = copy_size;  // Update to actual size
        LOG_DEBUG("nimcp_bio_promise_complete_sized: copied %zu bytes of result data", copy_size);
    }

    /* Set peak concentration */
    float peak;
    switch (shared->channel) {
        case BIO_CHANNEL_DOPAMINE: peak = BIO_DA_PEAK_PHASIC_UM; break;
        case BIO_CHANNEL_SEROTONIN: peak = 1.0F; break;
        case BIO_CHANNEL_NOREPINEPHRINE: peak = 1.0F; break;
        case BIO_CHANNEL_ACETYLCHOLINE: peak = 1.0F; break;
        default: peak = 1.0F;
    }
    atomic_store_float(&shared->concentration_bits, peak);
    LOG_DEBUG("nimcp_bio_promise_complete: set peak concentration %.4f µM", peak);

    /* Record completion time */
    shared->complete_time_ms = bio_time_ms();
    uint64_t latency_ms = shared->complete_time_ms - shared->create_time_ms;

    /* P1-46 fix: Explicit release fence ensures all result data (result buffer,
     * concentration, completion time) is fully visible to other threads before
     * the CAS publishes the state transition. The CAS itself uses ACQ_REL, but
     * we add an explicit fence for defense-in-depth on weakly-ordered architectures. */
    nimcp_atomic_thread_fence(NIMCP_MEMORY_ORDER_RELEASE);

    /* Transition state */
    uint32_t expected = BIO_FUTURE_PENDING;
    if (!nimcp_atomic_compare_exchange_u32(&shared->state, &expected, BIO_FUTURE_COMPLETED,
                                           NIMCP_MEMORY_ORDER_ACQ_REL)) {
        /* Already completed/failed/cancelled */
        LOG_WARNING("nimcp_bio_promise_complete: promise already in state %u", expected);
        if (shared->result) {
            bio_free(shared->result);
            shared->result = NULL;
        }
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE,
                          "nimcp_bio_promise_complete: promise already completed/failed/cancelled");
    }

    /* Update channel last completion time for refractory period tracking */
    if (channel < BIO_CHANNEL_COUNT) {
        nimcp_mutex_lock(&g_bio_async.channel_state[channel].mutex);
        g_bio_async.channel_state[channel].last_completion_ms = shared->complete_time_ms;
        nimcp_mutex_unlock(&g_bio_async.channel_state[channel].mutex);
    }

    /* Wake waiters */
    nimcp_mutex_lock(&shared->mutex);
    nimcp_cond_broadcast(&shared->cond);
    nimcp_mutex_unlock(&shared->mutex);

    /* Invoke callbacks OUTSIDE the lock to prevent deadlock risk */
    shared_state_invoke_callbacks(shared);

    /* Update statistics */
    nimcp_rwlock_wrlock(&g_bio_async.stats_lock);
    g_bio_async.stats.total_futures_completed++;
    g_bio_async.stats.channel_stats[shared->channel].releases++;
    nimcp_rwlock_unlock(&g_bio_async.stats_lock);

    LOG_INFO("Completed bio promise on channel %s (latency: %lu ms)",
             nimcp_bio_channel_name(shared->channel), (unsigned long)latency_ms);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_bio_promise_complete(
    nimcp_bio_promise_t promise,
    const void* result)
{
    /* Legacy API: uses pre-set result_size from promise creation */
    NIMCP_CHECK_THROW(promise && promise->magic == BIO_MAGIC_PROMISE,
                       NIMCP_ERROR_NULL_POINTER, "nimcp_bio_promise_complete: invalid promise");

    nimcp_bio_shared_state_t* shared = promise->shared;
    NIMCP_CHECK_THROW(shared && shared->magic == BIO_MAGIC_PROMISE,
                      NIMCP_ERROR_INVALID_STATE,
                      "nimcp_bio_promise_complete: invalid shared state");

    /* Use pre-set result_size or 0 for void results */
    return nimcp_bio_promise_complete_sized(promise, result, shared->result_size);
}

nimcp_error_t nimcp_bio_promise_fail(
    nimcp_bio_promise_t promise,
    nimcp_error_t error)
{
    LOG_DEBUG("nimcp_bio_promise_fail: promise=%p, error=%d", (void*)promise, error);

    NIMCP_CHECK_THROW(promise && promise->magic == BIO_MAGIC_PROMISE,
                       NIMCP_ERROR_NULL_POINTER, "nimcp_bio_promise_fail: invalid promise");

    nimcp_bio_shared_state_t* shared = promise->shared;
    NIMCP_CHECK_THROW(shared && shared->magic == BIO_MAGIC_PROMISE,
                      NIMCP_ERROR_INVALID_STATE,
                      "nimcp_bio_promise_fail: invalid shared state");

    NIMCP_CHECK_THROW(error != NIMCP_SUCCESS, NIMCP_ERROR_INVALID_PARAM,
                       "nimcp_bio_promise_fail: cannot fail with NIMCP_SUCCESS");

    /* Store error BEFORE state transition CAS to ensure another thread
     * reading the FAILED state will always see the error value.
     * Use release ordering to prevent CPU reordering the error store
     * after the state change. */
    __atomic_store_n(&shared->error, error, __ATOMIC_RELEASE);

    /* Transition state */
    uint32_t expected = BIO_FUTURE_PENDING;
    if (!nimcp_atomic_compare_exchange_u32(&shared->state, &expected, BIO_FUTURE_FAILED,
                                           NIMCP_MEMORY_ORDER_ACQ_REL)) {
        LOG_WARNING("nimcp_bio_promise_fail: promise already in state %u", expected);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE,
                          "nimcp_bio_promise_fail: promise already completed/failed/cancelled");
    }

    /* Wake waiters */
    nimcp_mutex_lock(&shared->mutex);
    nimcp_cond_broadcast(&shared->cond);
    nimcp_mutex_unlock(&shared->mutex);

    /* Invoke callbacks OUTSIDE the lock to prevent deadlock risk */
    shared_state_invoke_callbacks(shared);

    LOG_WARNING("Bio promise failed on channel %s with error %d",
                nimcp_bio_channel_name(shared->channel), error);
    return NIMCP_SUCCESS;
}

nimcp_bio_future_t nimcp_bio_promise_get_future(nimcp_bio_promise_t promise) {
    BIO_TRACE("nimcp_bio_promise_get_future: promise=%p", (void*)promise);

    if (!promise || promise->magic != BIO_MAGIC_PROMISE) {
        LOG_ERROR("nimcp_bio_promise_get_future: invalid promise");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_bio_promise_get_future: promise is NULL");
        return NULL;
    }

    nimcp_bio_shared_state_t* shared = promise->shared;
    if (!shared || shared->magic != BIO_MAGIC_PROMISE) {
        LOG_ERROR("nimcp_bio_promise_get_future: invalid shared state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_bio_promise_get_future: shared is NULL");
        return NULL;
    }

    nimcp_bio_future_t future = (nimcp_bio_future_t)
        bio_alloc(sizeof(struct nimcp_bio_future_struct));
    if (!future) {
        LOG_ERROR("nimcp_bio_promise_get_future: failed to allocate future");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_bio_promise_get_future: future is NULL");
        return NULL;
    }

    shared_state_addref(shared);

    future->magic = BIO_MAGIC_FUTURE;
    future->shared = shared;

    LOG_DEBUG("Created bio future %p from promise %p (channel %s)",
              (void*)future, (void*)promise, nimcp_bio_channel_name(shared->channel));
    return future;
}

void nimcp_bio_promise_destroy(nimcp_bio_promise_t promise) {
    BIO_TRACE("nimcp_bio_promise_destroy: promise=%p", (void*)promise);

    if (!promise || promise->magic != BIO_MAGIC_PROMISE) {
        LOG_WARNING("nimcp_bio_promise_destroy: invalid promise %p", (void*)promise);
        return;
    }

    promise->magic = 0;
    shared_state_release(promise->shared);
    bio_free(promise);
}

//=============================================================================
// Bio-Future API
//=============================================================================

nimcp_bio_future_state_t nimcp_bio_future_state(nimcp_bio_future_t future) {
    if (!future || future->magic != BIO_MAGIC_FUTURE) {
        BIO_TRACE("nimcp_bio_future_state: invalid future");
        return BIO_FUTURE_PENDING;
    }

    nimcp_bio_shared_state_t* shared = future->shared;
    if (!shared || shared->magic != BIO_MAGIC_PROMISE) {
        BIO_TRACE("nimcp_bio_future_state: invalid shared state");
        return BIO_FUTURE_PENDING;
    }

    /* Update concentration/decay */
    shared_state_update_concentration(shared);

    nimcp_bio_future_state_t state = (nimcp_bio_future_state_t)nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_ACQUIRE);
    BIO_TRACE("nimcp_bio_future_state: future=%p state=%d", (void*)future, state);
    return state;
}

nimcp_error_t nimcp_bio_future_wait(
    nimcp_bio_future_t future,
    void* out_result,
    uint64_t timeout_ms)
{
    LOG_DEBUG("nimcp_bio_future_wait: future=%p timeout=%lu ms", (void*)future, (unsigned long)timeout_ms);

    NIMCP_CHECK_THROW(future && future->magic == BIO_MAGIC_FUTURE,
                       NIMCP_ERROR_NULL_POINTER, "nimcp_bio_future_wait: invalid future");

    nimcp_bio_shared_state_t* shared = future->shared;
    NIMCP_CHECK_THROW(shared && shared->magic == BIO_MAGIC_PROMISE,
                      NIMCP_ERROR_INVALID_STATE,
                      "nimcp_bio_future_wait: invalid shared state");

    /* Fast path: check if already ready */
    uint32_t state = nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_ACQUIRE);
    if (state != BIO_FUTURE_PENDING) {
        LOG_DEBUG("nimcp_bio_future_wait: fast path - already in state %u", state);
        goto handle_result;
    }

    /* Slow path: wait */
    LOG_DEBUG("nimcp_bio_future_wait: entering slow path wait");
    nimcp_mutex_lock(&shared->mutex);

    while ((state = nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_ACQUIRE)) == BIO_FUTURE_PENDING) {
        if (timeout_ms > 0) {
            int wait_result = nimcp_cond_timedwait(&shared->cond, &shared->mutex, (uint32_t)timeout_ms);
            if (wait_result != NIMCP_SUCCESS) {
                nimcp_mutex_unlock(&shared->mutex);
                LOG_DEBUG("nimcp_bio_future_wait: timeout after %lu ms", (unsigned long)timeout_ms);
                NIMCP_CHECK_THROW(false, NIMCP_ERROR_TIMEOUT,
                                  "nimcp_bio_future_wait: timeout waiting for future");
            }
        } else {
            nimcp_cond_wait(&shared->cond, &shared->mutex);
        }
    }

    nimcp_mutex_unlock(&shared->mutex);
    LOG_DEBUG("nimcp_bio_future_wait: woke up with state %u", state);

handle_result:
    /* Update concentration */
    shared_state_update_concentration(shared);
    state = nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_ACQUIRE);

    if (state == BIO_FUTURE_COMPLETED) {
        if (out_result && shared->result && shared->result_size > 0) {
            memcpy(out_result, shared->result, shared->result_size);
            LOG_DEBUG("nimcp_bio_future_wait: copied %zu bytes of result", shared->result_size);
        }
        LOG_DEBUG("nimcp_bio_future_wait: completed successfully");
        return NIMCP_SUCCESS;
    } else if (state == BIO_FUTURE_FAILED) {
        LOG_DEBUG("nimcp_bio_future_wait: future failed with error %d", shared->error);
        return shared->error;
    } else if (state == BIO_FUTURE_DECAYED) {
        LOG_DEBUG("nimcp_bio_future_wait: future decayed");
        return NIMCP_BIO_ERROR_DECAY_COMPLETE;
    } else if (state == BIO_FUTURE_CANCELLED) {
        LOG_DEBUG("nimcp_bio_future_wait: future cancelled");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_CANCELLED,
                          "nimcp_bio_future_wait: future was cancelled");
    }

    LOG_ERROR("nimcp_bio_future_wait: unexpected state %u", state);
    NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE,
                      "nimcp_bio_future_wait: unexpected state");
}

float nimcp_bio_future_get_confidence(nimcp_bio_future_t future) {
    if (!future || future->magic != BIO_MAGIC_FUTURE) {
        return 0.0F;
    }

    nimcp_bio_shared_state_t* shared = future->shared;
    if (!shared || shared->magic != BIO_MAGIC_PROMISE) {
        return 0.0F;
    }

    /* Update concentration */
    float concentration = shared_state_update_concentration(shared);

    /* Calculate confidence using same peak/baseline as shared_state_update_concentration */
    float peak, baseline;
    switch (shared->channel) {
        case BIO_CHANNEL_DOPAMINE:
            peak = BIO_DA_PEAK_PHASIC_UM;
            baseline = BIO_DA_BASELINE_UM;
            break;
        case BIO_CHANNEL_SEROTONIN:
            peak = 1.0F;
            baseline = BIO_5HT_BASELINE_UM;
            break;
        case BIO_CHANNEL_NOREPINEPHRINE:
            peak = 1.0F;
            baseline = BIO_NE_BASELINE_UM;
            break;
        case BIO_CHANNEL_ACETYLCHOLINE:
            peak = 1.0F;
            baseline = BIO_ACH_BASELINE_UM;
            break;
        default:
            peak = 1.0F;
            baseline = 0.0F;
    }

    if (peak <= baseline) return 0.0F;

    float confidence = (concentration - baseline) / (peak - baseline);
    if (confidence < 0.0F) confidence = 0.0F;
    if (confidence > 1.0F) confidence = 1.0F;

    return confidence;
}

bool nimcp_bio_future_is_ready(nimcp_bio_future_t future) {
    nimcp_bio_future_state_t state = nimcp_bio_future_state(future);
    return state != BIO_FUTURE_PENDING;
}

float nimcp_bio_future_get_age_ms(nimcp_bio_future_t future) {
    if (!future || future->magic != BIO_MAGIC_FUTURE) {
        return -1.0F;
    }

    nimcp_bio_shared_state_t* shared = future->shared;
    if (!shared || shared->magic != BIO_MAGIC_PROMISE) {
        return -1.0F;
    }

    uint32_t state = nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_ACQUIRE);
    if (state == BIO_FUTURE_PENDING) {
        return -1.0F;
    }

    return (float)(bio_time_ms() - shared->complete_time_ms);
}

nimcp_error_t nimcp_bio_future_then(
    nimcp_bio_future_t future,
    nimcp_bio_callback_t callback,
    void* user_data)
{
    NIMCP_CHECK_THROW(future && future->magic == BIO_MAGIC_FUTURE && callback,
                       NIMCP_ERROR_NULL_POINTER, "nimcp_bio_future_then: future or callback is NULL/invalid");

    nimcp_bio_shared_state_t* shared = future->shared;
    NIMCP_CHECK_THROW(shared && shared->magic == BIO_MAGIC_PROMISE,
                      NIMCP_ERROR_INVALID_STATE,
                      "nimcp_bio_future_then: invalid shared state");

    /* Check if already ready */
    uint32_t state = nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_ACQUIRE);
    if (state != BIO_FUTURE_PENDING) {
        /* Invoke immediately */
        float confidence = nimcp_bio_future_get_confidence(future);
        const void* result = (state == BIO_FUTURE_COMPLETED) ? shared->result : NULL;
        nimcp_error_t error = (state == BIO_FUTURE_FAILED) ? shared->error : NIMCP_SUCCESS;
        callback(result, confidence, error, user_data);
        return NIMCP_SUCCESS;
    }

    /* Allocate callback node */
    bio_callback_node_t* node = (bio_callback_node_t*)bio_alloc(sizeof(bio_callback_node_t));
    NIMCP_CHECK_THROW(node != NULL, NIMCP_ERROR_NO_MEMORY,
                       "nimcp_bio_future_then: failed to allocate callback node");

    node->callback = callback;
    node->user_data = user_data;
    node->next = NULL;

    /* Add to list */
    nimcp_mutex_lock(&shared->mutex);

    /* Double-check state */
    state = nimcp_atomic_load_u32(&shared->state, NIMCP_MEMORY_ORDER_ACQUIRE);
    if (state != BIO_FUTURE_PENDING) {
        nimcp_mutex_unlock(&shared->mutex);
        bio_free(node);

        float confidence = nimcp_bio_future_get_confidence(future);
        const void* result = (state == BIO_FUTURE_COMPLETED) ? shared->result : NULL;
        nimcp_error_t error = (state == BIO_FUTURE_FAILED) ? shared->error : NIMCP_SUCCESS;
        callback(result, confidence, error, user_data);
        return NIMCP_SUCCESS;
    }

    node->next = shared->callbacks;
    shared->callbacks = node;

    nimcp_mutex_unlock(&shared->mutex);

    return NIMCP_SUCCESS;
}

bool nimcp_bio_future_cancel(nimcp_bio_future_t future) {
    if (!future || future->magic != BIO_MAGIC_FUTURE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_bio_future_cancel: future is NULL");
        return false;
    }

    nimcp_bio_shared_state_t* shared = future->shared;
    if (!shared || shared->magic != BIO_MAGIC_PROMISE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_bio_future_cancel: shared is NULL");
        return false;
    }

    uint32_t expected = BIO_FUTURE_PENDING;
    if (!nimcp_atomic_compare_exchange_u32(&shared->state, &expected, BIO_FUTURE_CANCELLED,
                                           NIMCP_MEMORY_ORDER_ACQ_REL)) {
        return false;
    }

    nimcp_mutex_lock(&shared->mutex);
    nimcp_cond_broadcast(&shared->cond);
    nimcp_mutex_unlock(&shared->mutex);

    /* Invoke callbacks OUTSIDE the lock to prevent deadlock risk */
    shared_state_invoke_callbacks(shared);

    return true;
}

void nimcp_bio_future_destroy(nimcp_bio_future_t future) {
    BIO_TRACE("nimcp_bio_future_destroy: future=%p", (void*)future);

    if (!future || future->magic != BIO_MAGIC_FUTURE) {
        LOG_WARNING("nimcp_bio_future_destroy: invalid future %p", (void*)future);
        return;
    }

    nimcp_bio_shared_state_t* shared = future->shared;

    /* Update statistics */
    if (shared && g_bio_async.initialized) {
        nimcp_rwlock_wrlock(&g_bio_async.stats_lock);
        if (shared->channel < BIO_CHANNEL_COUNT) {
            g_bio_async.stats.channel_stats[shared->channel].active_futures--;
        }
        nimcp_rwlock_unlock(&g_bio_async.stats_lock);
    }

    future->magic = 0;
    shared_state_release(shared);
    bio_free(future);
    LOG_DEBUG("nimcp_bio_future_destroy: future destroyed");
}

//=============================================================================
// Phase Coupling API
//=============================================================================

nimcp_phase_sync_t nimcp_phase_sync_create(nimcp_oscillation_band_t band) {
    LOG_DEBUG("nimcp_phase_sync_create: band=%s", nimcp_oscillation_band_name(band));

    if (!g_bio_async.initialized) {
        LOG_ERROR("nimcp_phase_sync_create: bio-async not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_phase_sync_create: g_bio_async is NULL");
        return NULL;
    }

    if (band >= BIO_OSC_BAND_COUNT) {
        LOG_ERROR("nimcp_phase_sync_create: invalid band %d", band);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_phase_sync_create: capacity exceeded");
        return NULL;
    }

    nimcp_phase_sync_t sync = (nimcp_phase_sync_t)
        bio_aligned_alloc(BIO_CACHE_LINE_SIZE, sizeof(struct nimcp_phase_sync_struct));
    if (!sync) {
        LOG_ERROR("nimcp_phase_sync_create: failed to allocate phase sync");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_phase_sync_create: sync is NULL");
        return NULL;
    }

    memset(sync, 0, sizeof(struct nimcp_phase_sync_struct));
    sync->magic = BIO_MAGIC_PHASE;
    sync->band = band;

    /* Allocate oscillators */
    sync->capacity = g_bio_async.config.phase_config.max_oscillators;
    sync->oscillators = (oscillator_t*)bio_alloc(sync->capacity * sizeof(oscillator_t));
    if (!sync->oscillators) {
        LOG_ERROR("nimcp_phase_sync_create: failed to allocate oscillators array");
        bio_free(sync);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_phase_sync_create: sync->oscillators is NULL");
        return NULL;
    }
    sync->count = 0;

    /* Set coupling parameters based on band */
    switch (band) {
        case BIO_OSC_DELTA: sync->coupling_K = BIO_KURAMOTO_K_DELTA; break;
        case BIO_OSC_THETA: sync->coupling_K = BIO_KURAMOTO_K_THETA; break;
        case BIO_OSC_ALPHA: sync->coupling_K = BIO_KURAMOTO_K_ALPHA; break;
        case BIO_OSC_BETA: sync->coupling_K = BIO_KURAMOTO_K_BETA; break;
        case BIO_OSC_GAMMA: sync->coupling_K = BIO_KURAMOTO_K_GAMMA; break;
        default: sync->coupling_K = 1.0F;
    }
    sync->coherence_threshold = g_bio_async.config.phase_config.coherence_threshold;

    /* Initialize order parameter */
    atomic_store_float(&sync->order_r_bits, 0.0F);
    atomic_store_float(&sync->mean_phase_bits, 0.0F);

    /* Initialize synchronization */
    if (nimcp_rwlock_init(&sync->rwlock) != NIMCP_SUCCESS) {
        bio_free(sync->oscillators);
        bio_free(sync);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_phase_sync_create: validation failed");
        return NULL;
    }
    if (nimcp_cond_init(&sync->cond) != NIMCP_SUCCESS) {
        nimcp_rwlock_destroy(&sync->rwlock);
        bio_free(sync->oscillators);
        bio_free(sync);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_phase_sync_create: validation failed");
        return NULL;
    }
    if (nimcp_mutex_init(&sync->cond_mutex, NULL) != NIMCP_SUCCESS) {
        nimcp_cond_destroy(&sync->cond);
        nimcp_rwlock_destroy(&sync->rwlock);
        bio_free(sync->oscillators);
        bio_free(sync);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_phase_sync_create: validation failed");
        return NULL;
    }

    LOG_DEBUG("Created phase sync for band %s", nimcp_oscillation_band_name(band));
    return sync;
}

nimcp_error_t nimcp_phase_sync_add_future(
    nimcp_phase_sync_t sync,
    nimcp_bio_future_t future)
{
    NIMCP_CHECK_THROW(sync && sync->magic == BIO_MAGIC_PHASE,
                       NIMCP_ERROR_NULL_POINTER, "nimcp_phase_sync_add_future: sync is NULL/invalid");
    NIMCP_CHECK_THROW(future && future->magic == BIO_MAGIC_FUTURE,
                       NIMCP_ERROR_NULL_POINTER, "nimcp_phase_sync_add_future: future is NULL/invalid");

    nimcp_rwlock_wrlock(&sync->rwlock);

    if (sync->count >= sync->capacity) {
        nimcp_rwlock_unlock(&sync->rwlock);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_OUT_OF_RANGE,
                          "nimcp_phase_sync_add_future: capacity exceeded");
    }

    /* Create oscillator with random initial phase and natural frequency */
    float center_freq = nimcp_oscillation_center_freq(sync->band);
    float omega = center_freq * 2.0F * (float)M_PI / 1000.0F;  /* rad/ms */
    float spread = g_bio_async.config.phase_config.frequency_spread;

    /* Add small random variation (simple LCG with thread-local seed for thread safety) */
    static __thread uint32_t seed = 12345;
    seed = seed * 1103515245 + 12345;
    float rand_01 = (float)(seed & 0x7FFFFFFF) / (float)0x7FFFFFFF;
    float freq_variation = (rand_01 - 0.5F) * 2.0F * spread;

    oscillator_t* osc = &sync->oscillators[sync->count];
    osc->phase = rand_01 * 2.0F * (float)M_PI;  /* Random initial phase */
    osc->natural_freq = omega * (1.0F + freq_variation);
    osc->future = future;
    osc->completed = false;

    sync->count++;

    /* Recalculate order parameter after adding oscillator */
    if (sync->count > 0) {
        /* Use BIO_MAX_OSCILLATORS as the limit since oscillators array is sized to capacity */
        const size_t MAX_PHASES = 256;  /* Match BIO_MAX_OSCILLATORS */
        float phases[256];
        size_t count = sync->count < MAX_PHASES ? sync->count : MAX_PHASES;
        for (size_t i = 0; i < count; i++) {
            phases[i] = sync->oscillators[i].phase;
        }
        float r, psi;
        bio_kuramoto_order_parameter(phases, count, &r, &psi);
        /* Wrap mean phase to [0, 2π] range (atan2 returns [-π, π]) */
        if (psi < 0.0F) psi += 2.0F * (float)M_PI;
        atomic_store_float(&sync->order_r_bits, r);
        atomic_store_float(&sync->mean_phase_bits, psi);
    }

    nimcp_rwlock_unlock(&sync->rwlock);

    return NIMCP_SUCCESS;
}

/**
 * @brief Update oscillator phases using Kuramoto model
 */
static void phase_sync_update(nimcp_phase_sync_t sync, float dt_ms) {
    nimcp_rwlock_wrlock(&sync->rwlock);

    if (sync->count == 0) {
        nimcp_rwlock_unlock(&sync->rwlock);
        return;
    }

    /* Collect phases */
    float* phases = (float*)bio_alloc(sync->count * sizeof(float));
    if (!phases) {
        nimcp_rwlock_unlock(&sync->rwlock);
        return;
    }

    for (size_t i = 0; i < sync->count; i++) {
        phases[i] = sync->oscillators[i].phase;

        /* Check if future completed */
        if (!sync->oscillators[i].completed) {
            if (nimcp_bio_future_is_ready(sync->oscillators[i].future)) {
                sync->oscillators[i].completed = true;
            }
        }
    }

    /* Calculate order parameter */
    float r, psi;
    bio_kuramoto_order_parameter(phases, sync->count, &r, &psi);
    /* Wrap mean phase to [0, 2π] range (atan2 returns [-π, π]) */
    if (psi < 0.0F) psi += 2.0F * (float)M_PI;
    atomic_store_float(&sync->order_r_bits, r);
    atomic_store_float(&sync->mean_phase_bits, psi);

    /* Update each oscillator */
    for (size_t i = 0; i < sync->count; i++) {
        sync->oscillators[i].phase = bio_kuramoto_step(
            sync->oscillators[i].phase,
            sync->oscillators[i].natural_freq,
            sync->coupling_K,
            r,
            psi,
            dt_ms
        );
    }

    bio_free(phases);
    nimcp_rwlock_unlock(&sync->rwlock);
}

nimcp_error_t nimcp_phase_sync_wait_all(
    nimcp_phase_sync_t sync,
    uint64_t timeout_ms)
{
    return nimcp_phase_sync_wait_coherent(sync, sync->coherence_threshold, timeout_ms);
}

nimcp_error_t nimcp_phase_sync_wait_coherent(
    nimcp_phase_sync_t sync,
    float coherence_threshold,
    uint64_t timeout_ms)
{
    NIMCP_CHECK_THROW(sync && sync->magic == BIO_MAGIC_PHASE,
                       NIMCP_ERROR_NULL_POINTER, "nimcp_phase_sync_wait_coherent: sync is NULL/invalid");

    /* Default timeout based on band */
    if (timeout_ms == 0) {
        float period_ms = BIO_HZ_TO_PERIOD_MS(nimcp_oscillation_center_freq(sync->band));
        timeout_ms = (uint64_t)(period_ms * 10);  /* 10 cycles */
    }

    uint64_t start_ms = bio_time_ms();
    float dt_ms = g_bio_async.config.simulation_dt_ms;

    while (true) {
        /* Update phases */
        phase_sync_update(sync, dt_ms);

        /* Check coherence */
        float r = atomic_load_float(&sync->order_r_bits);
        if (r >= coherence_threshold) {
            /* Check all futures completed */
            nimcp_rwlock_rdlock(&sync->rwlock);
            bool all_completed = true;
            for (size_t i = 0; i < sync->count && all_completed; i++) {
                all_completed = sync->oscillators[i].completed;
            }
            nimcp_rwlock_unlock(&sync->rwlock);

            if (all_completed) {
                /* Update statistics */
                nimcp_rwlock_wrlock(&g_bio_async.stats_lock);
                g_bio_async.stats.phase_stats.sync_requests++;
                g_bio_async.stats.phase_stats.sync_achieved++;
                g_bio_async.stats.phase_stats.avg_coherence = r;
                nimcp_rwlock_unlock(&g_bio_async.stats_lock);

                return NIMCP_SUCCESS;
            }
        }

        /* Check timeout */
        uint64_t elapsed = bio_time_ms() - start_ms;
        if (elapsed >= timeout_ms) {
            nimcp_rwlock_wrlock(&g_bio_async.stats_lock);
            g_bio_async.stats.phase_stats.sync_requests++;
            g_bio_async.stats.phase_stats.sync_timeouts++;
            nimcp_rwlock_unlock(&g_bio_async.stats_lock);

            return NIMCP_BIO_ERROR_PHASE_INCOHERENT;
        }

        /* Small sleep to avoid spinning */
        nimcp_mutex_lock(&sync->cond_mutex);
        nimcp_cond_timedwait(&sync->cond, &sync->cond_mutex, 1);  /* 1ms */
        nimcp_mutex_unlock(&sync->cond_mutex);
    }
}

float nimcp_phase_sync_get_coherence(nimcp_phase_sync_t sync) {
    if (!sync || sync->magic != BIO_MAGIC_PHASE) {
        return 0.0F;
    }
    return atomic_load_float(&sync->order_r_bits);
}

float nimcp_phase_sync_get_mean_phase(nimcp_phase_sync_t sync) {
    if (!sync || sync->magic != BIO_MAGIC_PHASE) {
        return 0.0F;
    }
    return atomic_load_float(&sync->mean_phase_bits);
}

size_t nimcp_phase_sync_get_count(nimcp_phase_sync_t sync) {
    if (!sync || sync->magic != BIO_MAGIC_PHASE) {
        return 0;
    }
    nimcp_rwlock_rdlock(&sync->rwlock);
    size_t count = sync->count;
    nimcp_rwlock_unlock(&sync->rwlock);
    return count;
}

void nimcp_phase_sync_destroy(nimcp_phase_sync_t sync) {
    if (!sync || sync->magic != BIO_MAGIC_PHASE) {
        return;
    }

    sync->magic = 0;

    nimcp_mutex_destroy(&sync->cond_mutex);
    nimcp_cond_destroy(&sync->cond);
    nimcp_rwlock_destroy(&sync->rwlock);

    bio_free(sync->oscillators);
    bio_free(sync);
}

//=============================================================================
// Predictive Coding API
//=============================================================================

nimcp_predictive_model_t nimcp_predictive_create(
    const char* signal_name,
    float initial_prediction,
    float initial_precision)
{
    if (!g_bio_async.initialized || !signal_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_predictive_create: required parameter is NULL (g_bio_async, signal_name)");
        return NULL;
    }

    nimcp_predictive_model_t model = (nimcp_predictive_model_t)
        bio_alloc(sizeof(struct nimcp_predictive_model_struct));
    if (!model) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "model is NULL");

        return NULL;
    }

    memset(model, 0, sizeof(struct nimcp_predictive_model_struct));
    model->magic = BIO_MAGIC_PREDICT;

    strncpy(model->signal_name, signal_name, BIO_MAX_SIGNAL_NAME - 1);
    model->signal_name[BIO_MAX_SIGNAL_NAME - 1] = '\0';

    model->prediction = initial_prediction;
    model->precision = (initial_precision > 0) ? initial_precision :
                       g_bio_async.config.predictive_config.default_prior_precision;
    model->last_surprise = 0.0F;
    model->learning_rate = g_bio_async.config.predictive_config.learning_rate;

    model->callbacks = NULL;

    if (nimcp_rwlock_init(&model->rwlock) != NIMCP_SUCCESS) {
        bio_free(model);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_predictive_create: validation failed");
        return NULL;
    }

    LOG_DEBUG("Created predictive model for signal '%s'", signal_name);
    return model;
}

nimcp_error_t nimcp_predictive_on_error(
    nimcp_predictive_model_t model,
    nimcp_prediction_error_callback_t callback,
    void* user_data,
    float surprise_threshold)
{
    NIMCP_CHECK_THROW(model && model->magic == BIO_MAGIC_PREDICT && callback,
                       NIMCP_ERROR_NULL_POINTER, "nimcp_predictive_on_error: model or callback is NULL/invalid");

    predict_callback_entry_t* entry = (predict_callback_entry_t*)
        bio_alloc(sizeof(predict_callback_entry_t));
    NIMCP_CHECK_THROW(entry != NULL, NIMCP_ERROR_NO_MEMORY,
                       "nimcp_predictive_on_error: failed to allocate callback entry");

    entry->callback = callback;
    entry->user_data = user_data;
    entry->surprise_threshold = surprise_threshold;
    entry->next = NULL;

    nimcp_rwlock_wrlock(&model->rwlock);
    entry->next = model->callbacks;
    model->callbacks = entry;
    nimcp_rwlock_unlock(&model->rwlock);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_predictive_observe(
    nimcp_predictive_model_t model,
    float actual_value)
{
    NIMCP_CHECK_THROW(model && model->magic == BIO_MAGIC_PREDICT,
                       NIMCP_ERROR_NULL_POINTER, "nimcp_predictive_observe: model is NULL/invalid");

    nimcp_rwlock_wrlock(&model->rwlock);

    float prediction = model->prediction;
    float precision = model->precision;

    /* Calculate prediction error */
    float error = bio_prediction_error(prediction, actual_value, precision);

    /* Calculate surprise */
    float surprise = bio_surprise(error / precision, precision);
    model->last_surprise = surprise;

    /* Bayesian update of prediction */
    float likelihood_precision = g_bio_async.config.predictive_config.default_likelihood_precision;
    model->prediction = bio_bayesian_update(prediction, actual_value, precision, likelihood_precision);
    model->precision = bio_posterior_precision(precision, likelihood_precision);

    /* Invoke callbacks if surprise exceeds threshold */
    predict_callback_entry_t* cb = model->callbacks;
    while (cb) {
        if (surprise >= cb->surprise_threshold) {
            cb->callback(model->signal_name, prediction, actual_value, error, surprise, cb->user_data);

            /* Update statistics */
            nimcp_rwlock_wrlock(&g_bio_async.stats_lock);
            g_bio_async.stats.predictive_stats.callbacks_triggered++;
            nimcp_rwlock_unlock(&g_bio_async.stats_lock);
        } else {
            nimcp_rwlock_wrlock(&g_bio_async.stats_lock);
            g_bio_async.stats.predictive_stats.callbacks_suppressed++;
            nimcp_rwlock_unlock(&g_bio_async.stats_lock);
        }
        cb = cb->next;
    }

    /* Update statistics */
    nimcp_rwlock_wrlock(&g_bio_async.stats_lock);
    g_bio_async.stats.predictive_stats.predictions_made++;
    g_bio_async.stats.predictive_stats.avg_surprise = surprise;
    g_bio_async.stats.predictive_stats.avg_precision = model->precision;
    nimcp_rwlock_unlock(&g_bio_async.stats_lock);

    nimcp_rwlock_unlock(&model->rwlock);

    return NIMCP_SUCCESS;
}

float nimcp_predictive_get_prediction(nimcp_predictive_model_t model) {
    if (!model || model->magic != BIO_MAGIC_PREDICT) {
        return 0.0F;
    }
    nimcp_rwlock_rdlock(&model->rwlock);
    float pred = model->prediction;
    nimcp_rwlock_unlock(&model->rwlock);
    return pred;
}

float nimcp_predictive_get_precision(nimcp_predictive_model_t model) {
    if (!model || model->magic != BIO_MAGIC_PREDICT) {
        return 0.0F;
    }
    nimcp_rwlock_rdlock(&model->rwlock);
    float prec = model->precision;
    nimcp_rwlock_unlock(&model->rwlock);
    return prec;
}

float nimcp_predictive_get_last_surprise(nimcp_predictive_model_t model) {
    if (!model || model->magic != BIO_MAGIC_PREDICT) {
        return 0.0F;
    }
    nimcp_rwlock_rdlock(&model->rwlock);
    float surp = model->last_surprise;
    nimcp_rwlock_unlock(&model->rwlock);
    return surp;
}

nimcp_error_t nimcp_predictive_set_prediction(
    nimcp_predictive_model_t model,
    float new_prediction,
    float new_precision)
{
    NIMCP_CHECK_THROW(model && model->magic == BIO_MAGIC_PREDICT,
                      NIMCP_ERROR_NULL_POINTER,
                      "nimcp_predictive_set_prediction: model is NULL/invalid");

    nimcp_rwlock_wrlock(&model->rwlock);
    model->prediction = new_prediction;
    if (new_precision > 0.0F) {
        model->precision = new_precision;
    }
    nimcp_rwlock_unlock(&model->rwlock);

    return NIMCP_SUCCESS;
}

void nimcp_predictive_destroy(nimcp_predictive_model_t model) {
    if (!model || model->magic != BIO_MAGIC_PREDICT) {
        return;
    }

    model->magic = 0;

    /* Free callbacks */
    predict_callback_entry_t* cb = model->callbacks;
    while (cb) {
        predict_callback_entry_t* next = cb->next;
        bio_free(cb);
        cb = next;
    }

    nimcp_rwlock_destroy(&model->rwlock);
    bio_free(model);
}

//=============================================================================
// Glial Signaling API
//=============================================================================

nimcp_glial_wave_t nimcp_glial_wave_initiate(
    uint32_t source_region,
    float initial_calcium)
{
    if (!g_bio_async.initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_glial_wave_initiate: g_bio_async is NULL");
        return NULL;
    }

    nimcp_glial_wave_t wave = (nimcp_glial_wave_t)
        bio_alloc(sizeof(struct nimcp_glial_wave_struct));
    if (!wave) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wave is NULL");

        return NULL;
    }

    memset(wave, 0, sizeof(struct nimcp_glial_wave_struct));
    wave->magic = BIO_MAGIC_GLIAL;
    wave->source_region = source_region;
    wave->initial_calcium = initial_calcium;
    wave->start_time_ms = bio_time_ms();

    /* Allocate region states */
    wave->num_regions = BIO_MAX_REGIONS;
    wave->regions = (region_state_t*)bio_alloc(wave->num_regions * sizeof(region_state_t));
    if (!wave->regions) {
        bio_free(wave);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_glial_wave_initiate: wave->regions is NULL");
        return NULL;
    }

    /* Initialize regions */
    for (size_t i = 0; i < wave->num_regions; i++) {
        wave->regions[i].calcium = BIO_CA_BASELINE_UM;
        wave->regions[i].ip3 = 0.0F;
        wave->regions[i].reached = false;
        wave->regions[i].callback = NULL;
        wave->regions[i].callback_data = NULL;
    }

    /* Set source region to initial calcium */
    if (source_region < wave->num_regions) {
        wave->regions[source_region].calcium = initial_calcium;
        wave->regions[source_region].reached = true;
    }

    wave->radius = 0.0F;
    wave->speed = g_bio_async.config.glial_config.wave_speed_um_s / 1000.0F;  /* um/ms */
    wave->active = true;

    if (nimcp_rwlock_init(&wave->rwlock) != NIMCP_SUCCESS) {
        bio_free(wave->regions);
        bio_free(wave);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_glial_wave_initiate: validation failed");
        return NULL;
    }
    if (nimcp_cond_init(&wave->cond) != NIMCP_SUCCESS) {
        nimcp_rwlock_destroy(&wave->rwlock);
        bio_free(wave->regions);
        bio_free(wave);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_glial_wave_initiate: validation failed");
        return NULL;
    }
    if (nimcp_mutex_init(&wave->cond_mutex, NULL) != NIMCP_SUCCESS) {
        nimcp_cond_destroy(&wave->cond);
        nimcp_rwlock_destroy(&wave->rwlock);
        bio_free(wave->regions);
        bio_free(wave);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_glial_wave_initiate: validation failed");
        return NULL;
    }

    LOG_DEBUG("Initiated calcium wave from region %u with Ca=%.2f", source_region, initial_calcium);
    return wave;
}

nimcp_error_t nimcp_glial_wave_step(nimcp_glial_wave_t wave, float dt_ms) {
    NIMCP_CHECK_THROW(wave && wave->magic == BIO_MAGIC_GLIAL,
                       NIMCP_ERROR_NULL_POINTER, "nimcp_glial_wave_step: wave is NULL/invalid");

    if (!wave->active) {
        return NIMCP_BIO_ERROR_WAVE_EXTINCT;
    }

    if (dt_ms <= 0.0F) {
        dt_ms = g_bio_async.config.simulation_dt_ms;
    }

    nimcp_rwlock_wrlock(&wave->rwlock);

    /* Advance wave radius */
    wave->radius += wave->speed * dt_ms;

    /* Simple isotropic propagation model */
    /* In a real implementation, we'd use proper reaction-diffusion */
    float threshold = g_bio_async.config.glial_config.wave_threshold_um;
    float decay_rate = g_bio_async.config.glial_config.decay_rate;

    /* Update each region */
    float max_calcium = 0.0F;
    for (size_t i = 0; i < wave->num_regions; i++) {
        region_state_t* region = &wave->regions[i];

        /* Simplified: assume regions are at distance i from source */
        float distance = fabsf((float)i - (float)wave->source_region);

        if (distance <= wave->radius && !region->reached) {
            /* Wave reached this region
             * Use constant attenuation length for biological realism.
             * Calcium waves attenuate with distance regardless of propagation time.
             * Attenuation length of 5 units ensures significant distance-based decay */
            const float ATTENUATION_LENGTH = 5.0F;
            region->calcium = wave->initial_calcium * bio_exponential_decay(1.0F, distance, ATTENUATION_LENGTH);

            if (region->calcium >= threshold) {
                region->reached = true;

                /* Invoke callback */
                if (region->callback) {
                    region->callback(wave, (uint32_t)i, region->calcium, region->callback_data);
                }
            }
        }

        /* Decay calcium */
        if (region->calcium > BIO_CA_BASELINE_UM) {
            region->calcium = BIO_CA_BASELINE_UM +
                (region->calcium - BIO_CA_BASELINE_UM) * (1.0F - decay_rate * dt_ms / 1000.0F);
        }

        if (region->calcium > max_calcium) {
            max_calcium = region->calcium;
        }
    }

    /* Check if wave is extinct */
    if (max_calcium < threshold) {
        wave->active = false;
    }

    nimcp_rwlock_unlock(&wave->rwlock);

    /* Signal waiters */
    nimcp_mutex_lock(&wave->cond_mutex);
    nimcp_cond_broadcast(&wave->cond);
    nimcp_mutex_unlock(&wave->cond_mutex);

    return wave->active ? NIMCP_SUCCESS : NIMCP_BIO_ERROR_WAVE_EXTINCT;
}

float nimcp_glial_wave_get_level_at(nimcp_glial_wave_t wave, uint32_t region_id) {
    if (!wave || wave->magic != BIO_MAGIC_GLIAL) {
        return 0.0F;
    }

    if (region_id >= wave->num_regions) {
        return 0.0F;
    }

    nimcp_rwlock_rdlock(&wave->rwlock);
    float level = wave->regions[region_id].calcium;
    nimcp_rwlock_unlock(&wave->rwlock);

    return level;
}

bool nimcp_glial_wave_has_reached(nimcp_glial_wave_t wave, uint32_t region_id) {
    if (!wave || wave->magic != BIO_MAGIC_GLIAL) {
        return false;
    }

    if (region_id >= wave->num_regions) {
        return false;
    }

    nimcp_rwlock_rdlock(&wave->rwlock);
    bool reached = wave->regions[region_id].reached;
    nimcp_rwlock_unlock(&wave->rwlock);

    return reached;
}

nimcp_error_t nimcp_glial_wave_wait_for_region(
    nimcp_glial_wave_t wave,
    uint32_t region_id,
    uint64_t timeout_ms)
{
    NIMCP_CHECK_THROW(wave && wave->magic == BIO_MAGIC_GLIAL,
                       NIMCP_ERROR_NULL_POINTER, "nimcp_glial_wave_wait_for_region: wave is NULL/invalid");

    NIMCP_CHECK_THROW(region_id < wave->num_regions, NIMCP_ERROR_OUT_OF_RANGE,
                      "nimcp_glial_wave_wait_for_region: region_id out of range");

    uint64_t start_ms = bio_time_ms();
    float dt_ms = g_bio_async.config.simulation_dt_ms;

    while (true) {
        /* Check if reached */
        if (nimcp_glial_wave_has_reached(wave, region_id)) {
            return NIMCP_SUCCESS;
        }

        /* Check if wave extinct */
        if (!wave->active) {
            return NIMCP_BIO_ERROR_WAVE_EXTINCT;
        }

        /* Step wave */
        nimcp_glial_wave_step(wave, dt_ms);

        /* Check timeout */
        if (timeout_ms > 0) {
            uint64_t elapsed = bio_time_ms() - start_ms;
            if (elapsed >= timeout_ms) {
                NIMCP_CHECK_THROW(false, NIMCP_ERROR_TIMEOUT,
                                  "nimcp_glial_wave_wait_for_region: timeout waiting for wave");
            }
        }

        /* Small wait */
        nimcp_mutex_lock(&wave->cond_mutex);
        nimcp_cond_timedwait(&wave->cond, &wave->cond_mutex, 1);
        nimcp_mutex_unlock(&wave->cond_mutex);
    }
}

nimcp_error_t nimcp_glial_wave_on_arrival(
    nimcp_glial_wave_t wave,
    uint32_t region_id,
    nimcp_wave_callback_t callback,
    void* user_data)
{
    NIMCP_CHECK_THROW(wave && wave->magic == BIO_MAGIC_GLIAL && callback,
                       NIMCP_ERROR_NULL_POINTER, "nimcp_glial_wave_on_arrival: wave or callback is NULL/invalid");

    NIMCP_CHECK_THROW(region_id < wave->num_regions, NIMCP_ERROR_OUT_OF_RANGE,
                      "nimcp_glial_wave_on_arrival: region_id out of range");

    nimcp_rwlock_wrlock(&wave->rwlock);
    wave->regions[region_id].callback = callback;
    wave->regions[region_id].callback_data = user_data;
    nimcp_rwlock_unlock(&wave->rwlock);

    return NIMCP_SUCCESS;
}

float nimcp_glial_wave_get_radius(nimcp_glial_wave_t wave) {
    if (!wave || wave->magic != BIO_MAGIC_GLIAL) {
        return 0.0F;
    }
    nimcp_rwlock_rdlock(&wave->rwlock);
    float radius = wave->radius;
    nimcp_rwlock_unlock(&wave->rwlock);
    return radius;
}

bool nimcp_glial_wave_is_active(nimcp_glial_wave_t wave) {
    if (!wave || wave->magic != BIO_MAGIC_GLIAL) {
        return false;
    }
    return wave->active;
}

void nimcp_glial_wave_destroy(nimcp_glial_wave_t wave) {
    if (!wave || wave->magic != BIO_MAGIC_GLIAL) {
        return;
    }

    wave->magic = 0;

    nimcp_mutex_destroy(&wave->cond_mutex);
    nimcp_cond_destroy(&wave->cond);
    nimcp_rwlock_destroy(&wave->rwlock);

    bio_free(wave->regions);
    bio_free(wave);
}

//=============================================================================
// Knowledge Graph Self-Awareness Integration
//=============================================================================

/**
 * @brief Query self-knowledge from the knowledge graph
 *
 * WHAT: Retrieves structural self-knowledge about the Bio_Async module
 * WHY:  Enables runtime introspection and self-awareness capabilities
 * HOW:  Queries KG for Bio_Async entity and logs observations/relations
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge was found, 0 otherwise
 */
int bio_async_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Bio_Async");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            LOG_DEBUG("Bio_Async self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Bio_Async");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Bio_Async");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
