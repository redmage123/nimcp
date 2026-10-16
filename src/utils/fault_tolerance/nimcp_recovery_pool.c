/**
 * @file nimcp_recovery_pool.c
 * @brief Pre-Allocated Memory Pool Implementation
 *
 * IMPLEMENTATION NOTES:
 * - Bump allocator: Simple, fast, no fragmentation during use
 * - Thread-safe: All operations mutex-protected
 * - Tracking: Linked list of allocations for reset
 * - Alignment: 8-byte aligned for all platforms
 * - Validation: Canary guards for corruption detection
 *
 * MEMORY LAYOUT:
 * ```
 * +------------------+
 * | Pool Header      | <- recovery_pool structure
 * +------------------+
 * | Pre-allocated    | <- Emergency buffer (size_bytes)
 * | Buffer           |    [offset = 0...size_bytes]
 * +------------------+
 * | Allocation List  | <- Linked list of allocations
 * +------------------+
 * ```
 *
 * @author NIMCP Team
 * @date 2025-11-20
 */

#include "utils/fault_tolerance/nimcp_recovery_pool.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "utils_recovery_pool"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(recovery_pool)

#include "utils/logging/nimcp_logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "utils/memory/nimcp_unified_memory.h"

//=============================================================================
// Internal Constants
//=============================================================================

#define POOL_MAGIC 0xDEADBEEF        /**< Magic number for validation */
#define ALLOCATION_MAGIC 0xABCDABCD  /**< Magic number for allocations */
#define ALIGNMENT 8                   /**< 8-byte alignment for all platforms */
#define MAX_ERROR_MSG 512            /**< Error message buffer size */
#define DEFAULT_POOL_SIZE (1024 * 1024)  /**< 1MB default pool size */
#define MAX_POOL_SIZE (100 * 1024 * 1024)  /**< 100MB max pool size */

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Allocation tracking entry
 *
 * WHAT: Track individual allocations for reset
 * WHY:  Enable proper cleanup and statistics
 * HOW:  Linked list of allocations
 */
typedef struct allocation_entry {
    uint32_t magic;                /**< Magic for validation */
    void* ptr;                     /**< Pointer to allocated memory */
    size_t size;                   /**< Allocation size */
    bool is_freed;                 /**< Marked as freed (for debugging) */
    struct allocation_entry* next; /**< Next allocation in list */
} allocation_entry_t;

/**
 * @brief Recovery pool structure
 *
 * WHAT: Emergency memory pool for OOM recovery
 * WHY:  Pre-allocated buffer for guaranteed recovery
 * HOW:  Bump allocator with tracking
 */
struct recovery_pool {
    uint32_t magic;                /**< Magic number for validation */

    // Pool buffer
    uint8_t* buffer;               /**< Pre-allocated emergency buffer */
    size_t size;                   /**< Total pool size (bytes) */
    size_t offset;                 /**< Current allocation offset */

    // Allocation tracking
    allocation_entry_t* allocations;  /**< Linked list of allocations */
    uint32_t allocation_count;        /**< Current allocation count */

    // Emergency mode
    bool emergency_mode;           /**< Currently in emergency mode */
    uint32_t emergency_activations;  /**< Lifetime emergency activations */

    // Statistics
    recovery_pool_stats_t stats;   /**< Pool usage statistics */

    // Thread safety
    nimcp_mutex_t mutex;           /**< Mutex for thread safety */
};

//=============================================================================
// Global State
//=============================================================================

// Global recovery pool (for NIMCP integration)
static recovery_pool_t* g_global_pool = NULL;

// Thread-local error message
static __thread char error_buffer[MAX_ERROR_MSG] = {0};

//=============================================================================
// Internal Helper Functions - Error Handling
//=============================================================================

/**
 * @brief Set error message for current thread
 *
 * WHAT: Store error message in thread-local buffer
 * WHY:  Thread-safe error reporting
 * HOW:  Use __thread storage class
 *
 * @param format Printf-style format string
 * @param ... Format arguments
 */
static void set_error(const char* format, ...) __attribute__((format(printf, 1, 2)));
static void set_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(error_buffer, MAX_ERROR_MSG, format, args);
    va_end(args);
}

//=============================================================================
// Internal Helper Functions - Alignment
//=============================================================================

/**
 * @brief Round up size to alignment boundary
 *
 * WHAT: Align size to ALIGNMENT bytes
 * WHY:  Ensure all allocations are properly aligned
 * HOW:  Round up to next multiple of ALIGNMENT
 *
 * @param size Size to align
 * @return Aligned size
 */
static inline size_t align_size(size_t size) {
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

/**
 * @brief Check if pointer is aligned
 *
 * WHAT: Verify pointer alignment
 * WHY:  Validate allocations
 * HOW:  Check if address is multiple of ALIGNMENT
 *
 * @param ptr Pointer to check
 * @return true if aligned, false otherwise
 */
static inline bool is_aligned(const void* ptr) {
    return ((uintptr_t)ptr % ALIGNMENT) == 0;
}

//=============================================================================
// Internal Helper Functions - Validation
//=============================================================================

/**
 * @brief Validate pool structure
 *
 * WHAT: Check pool internal consistency
 * WHY:  Detect corruption early
 * HOW:  Verify magic, bounds, invariants
 *
 * @param pool Pool to validate
 * @return true if valid, false if corrupted
 */
static bool validate_pool_internal(const recovery_pool_t* pool) {
    // Guard: NULL check
    if (!pool) {
        set_error("validate_pool_internal: NULL pool");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_pool_internal: pool is NULL");
        return false;
    }

    // Check magic
    if (pool->magic != POOL_MAGIC) {
        set_error("validate_pool_internal: Invalid magic 0x%X (expected 0x%X)",
                  pool->magic, POOL_MAGIC);
        return false;
    }

    // Check buffer
    if (!pool->buffer) {
        set_error("validate_pool_internal: NULL buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_pool_internal: pool->buffer is NULL");
        return false;
    }

    // Check size
    if (pool->size == 0) {
        set_error("validate_pool_internal: Zero size");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_pool_internal: pool->size is zero");
        return false;
    }

    // Check offset bounds
    if (pool->offset > pool->size) {
        set_error("validate_pool_internal: Offset %zu exceeds size %zu",
                  pool->offset, pool->size);
        return false;
    }

    // Check allocation tracking
    uint32_t tracked_count = 0;
    for (allocation_entry_t* entry = pool->allocations; entry; entry = entry->next) {
        // Validate allocation magic
        if (entry->magic != ALLOCATION_MAGIC) {
            set_error("validate_pool_internal: Allocation entry corrupted (magic 0x%X)",
                      entry->magic);
            return false;
        }

        // Validate pointer is within pool
        if (entry->ptr < (void*)pool->buffer ||
            entry->ptr >= (void*)(pool->buffer + pool->size)) {
            set_error("validate_pool_internal: Allocation pointer outside pool bounds");
            return false;
        }

        tracked_count++;
    }

    // Verify allocation count
    if (tracked_count != pool->allocation_count) {
        set_error("validate_pool_internal: Allocation count mismatch (tracked %u, reported %u)",
                  tracked_count, pool->allocation_count);
        return false;
    }

    return true;
}

//=============================================================================
// Public API - Pool Creation and Destruction
//=============================================================================

recovery_pool_t* recovery_pool_create(size_t size_bytes) {
    // Guard: Size validation
    if (size_bytes == 0) {
        set_error("recovery_pool_create: size_bytes is 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "recovery_pool_create: size_bytes is zero");
        return NULL;
    }

    if (size_bytes > MAX_POOL_SIZE) {
        set_error("recovery_pool_create: size_bytes %zu exceeds maximum %u",
                  size_bytes, MAX_POOL_SIZE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "recovery_pool_create: validation failed");
        return NULL;
    }

    // Allocate pool structure
    recovery_pool_t* pool = (recovery_pool_t*)nimcp_malloc(sizeof(recovery_pool_t));
    if (!pool) {
        set_error("recovery_pool_create: Failed to allocate pool structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "recovery_pool_create: pool is NULL");
        return NULL;
    }

    // Zero-initialize
    memset(pool, 0, sizeof(recovery_pool_t));

    // Initialize magic
    pool->magic = POOL_MAGIC;

    // Allocate buffer
    pool->buffer = (uint8_t*)nimcp_malloc(size_bytes);
    if (!pool->buffer) {
        set_error("recovery_pool_create: Failed to allocate buffer of %zu bytes", size_bytes);
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "recovery_pool_create: pool->buffer is NULL");
        return NULL;
    }

    // Initialize pool state
    pool->size = size_bytes;
    pool->offset = 0;
    pool->allocations = NULL;
    pool->allocation_count = 0;
    pool->emergency_mode = false;
    pool->emergency_activations = 0;

    // Initialize statistics
    memset(&pool->stats, 0, sizeof(recovery_pool_stats_t));
    pool->stats.pool_size_bytes = size_bytes;
    pool->stats.largest_free_block = size_bytes;

    // Initialize mutex
    if (nimcp_mutex_init(&pool->mutex, NULL) != NIMCP_SUCCESS) {
        set_error("recovery_pool_create: Failed to initialize mutex");
        nimcp_free(pool->buffer);
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "recovery_pool_create: validation failed");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Recovery pool created: %zu bytes (%zu KB)",
                       size_bytes, size_bytes / 1024);

    return pool;
}

void recovery_pool_destroy(recovery_pool_t* pool) {
    // Guard: NULL check
    if (!pool) {
        return;
    }

    NIMCP_LOGGING_INFO("Destroying recovery pool (%zu bytes, %u allocations)",
                       pool->size, pool->allocation_count);

    // Free allocation tracking list
    allocation_entry_t* entry = pool->allocations;
    while (entry) {
        allocation_entry_t* next = entry->next;
        nimcp_free(entry);
        entry = next;
    }

    // Destroy mutex
    nimcp_mutex_destroy(&pool->mutex);

    // Free buffer
    if (pool->buffer) {
        nimcp_free(pool->buffer);
    }

    // Zero and free pool structure
    memset(pool, 0, sizeof(recovery_pool_t));
    nimcp_free(pool);
}

//=============================================================================
// Public API - Emergency Mode Control
//=============================================================================

bool recovery_pool_enter_emergency_mode(recovery_pool_t* pool) {
    // Guard: NULL check
    if (!pool) {
        set_error("recovery_pool_enter_emergency_mode: NULL pool");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_pool_enter_emergency_mode: pool is NULL");
        return false;
    }

    // Lock mutex
    if (nimcp_mutex_lock(&pool->mutex) != NIMCP_SUCCESS) {
        set_error("recovery_pool_enter_emergency_mode: Failed to lock mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "recovery_pool_enter_emergency_mode: validation failed");
        return false;
    }

    // Set emergency mode
    if (!pool->emergency_mode) {
        pool->emergency_mode = true;
        pool->emergency_activations++;
        pool->stats.emergency_activations++;

        NIMCP_LOGGING_WARN("Emergency mode activated (activation #%u)",
                           pool->emergency_activations);
    }

    // Unlock mutex
    nimcp_mutex_unlock(&pool->mutex);

    return true;
}

bool recovery_pool_exit_emergency_mode(recovery_pool_t* pool) {
    // Guard: NULL check
    if (!pool) {
        set_error("recovery_pool_exit_emergency_mode: NULL pool");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_pool_exit_emergency_mode: pool is NULL");
        return false;
    }

    // Lock mutex
    if (nimcp_mutex_lock(&pool->mutex) != NIMCP_SUCCESS) {
        set_error("recovery_pool_exit_emergency_mode: Failed to lock mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "recovery_pool_exit_emergency_mode: validation failed");
        return false;
    }

    // Clear emergency mode
    if (pool->emergency_mode) {
        pool->emergency_mode = false;
        NIMCP_LOGGING_INFO("Emergency mode deactivated");
    }

    // Unlock mutex
    nimcp_mutex_unlock(&pool->mutex);

    return true;
}

bool recovery_pool_is_emergency_mode(const recovery_pool_t* pool) {
    // Guard: NULL check
    if (!pool) {
        return false;
    }

    // Lock mutex
    nimcp_mutex_t* mutex = (nimcp_mutex_t*)&pool->mutex;
    if (nimcp_mutex_lock(mutex) != NIMCP_SUCCESS) {
        return false;
    }

    bool result = pool->emergency_mode;

    // Unlock mutex
    nimcp_mutex_unlock(mutex);

    return result;
}

//=============================================================================
// Public API - Memory Allocation
//=============================================================================

void* recovery_pool_alloc(recovery_pool_t* pool, size_t size) {
    // Guard: NULL check
    if (!pool) {
        set_error("recovery_pool_alloc: NULL pool");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "recovery_pool_alloc: pool is NULL");
        return NULL;
    }

    // Guard: Zero size
    if (size == 0) {
        set_error("recovery_pool_alloc: Zero size");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "recovery_pool_alloc: size is zero");
        return NULL;
    }

    // Lock mutex
    if (nimcp_mutex_lock(&pool->mutex) != NIMCP_SUCCESS) {
        set_error("recovery_pool_alloc: Failed to lock mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "recovery_pool_alloc: validation failed");
        return NULL;
    }

    // Align size
    size_t aligned_size = align_size(size);

    // Check space
    if (pool->offset + aligned_size > pool->size) {
        pool->stats.failed_allocations++;
        pool->stats.pool_exhausted = true;
        set_error("recovery_pool_alloc: Pool exhausted (need %zu, have %zu)",
                  aligned_size, pool->size - pool->offset);

        NIMCP_LOGGING_ERROR("Pool exhausted: need %zu bytes, have %zu bytes available",
                            aligned_size, pool->size - pool->offset);

        nimcp_mutex_unlock(&pool->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_pool_alloc: operation failed");
        return NULL;
    }

    // Allocate from pool
    void* ptr = pool->buffer + pool->offset;
    pool->offset += aligned_size;

    // Verify alignment
    if (!is_aligned(ptr)) {
        set_error("recovery_pool_alloc: Alignment failure");
        nimcp_mutex_unlock(&pool->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "recovery_pool_alloc: is_aligned is NULL");
        return NULL;
    }

    // Track allocation
    allocation_entry_t* entry = (allocation_entry_t*)nimcp_malloc(sizeof(allocation_entry_t));
    if (entry) {
        entry->magic = ALLOCATION_MAGIC;
        entry->ptr = ptr;
        entry->size = aligned_size;
        entry->is_freed = false;
        entry->next = pool->allocations;
        pool->allocations = entry;
        pool->allocation_count++;
    }

    // Update statistics
    pool->stats.current_used_bytes += aligned_size;
    pool->stats.total_allocated_bytes += aligned_size;
    pool->stats.total_allocations++;

    if (pool->stats.current_used_bytes > pool->stats.peak_used_bytes) {
        pool->stats.peak_used_bytes = pool->stats.current_used_bytes;
    }

    if (pool->allocation_count > pool->stats.peak_allocation_count) {
        pool->stats.peak_allocation_count = pool->allocation_count;
    }

    pool->stats.largest_free_block = pool->size - pool->offset;

    // Unlock mutex
    nimcp_mutex_unlock(&pool->mutex);

    return ptr;
}

void* recovery_pool_calloc(recovery_pool_t* pool, size_t count, size_t size) {
    // Guard: NULL check
    if (!pool) {
        set_error("recovery_pool_calloc: NULL pool");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "recovery_pool_calloc: pool is NULL");
        return NULL;
    }

    // Guard: Overflow check
    if (count > 0 && size > SIZE_MAX / count) {
        set_error("recovery_pool_calloc: Size overflow");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "recovery_pool_calloc: validation failed");
        return NULL;
    }

    size_t total_size = count * size;

    // Allocate
    void* ptr = recovery_pool_alloc(pool, total_size);
    if (!ptr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ptr is NULL");

        return NULL;
    }

    // Zero initialize
    memset(ptr, 0, total_size);

    return ptr;
}

void recovery_pool_free(recovery_pool_t* pool, void* ptr) {
    // Guard: NULL checks
    if (!pool || !ptr) {
        return;
    }

    // Lock mutex
    if (nimcp_mutex_lock(&pool->mutex) != NIMCP_SUCCESS) {
        return;
    }

    // Find allocation entry
    for (allocation_entry_t* entry = pool->allocations; entry; entry = entry->next) {
        if (entry->ptr == ptr) {
            // Mark as freed (for debugging)
            entry->is_freed = true;

            // NOTE: Space is NOT reclaimed until reset
            // This is intentional for bump allocator simplicity

            break;
        }
    }

    // Unlock mutex
    nimcp_mutex_unlock(&pool->mutex);
}

//=============================================================================
// Public API - Pool Management
//=============================================================================

bool recovery_pool_reset(recovery_pool_t* pool) {
    // Guard: NULL check
    if (!pool) {
        set_error("recovery_pool_reset: NULL pool");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_pool_reset: pool is NULL");
        return false;
    }

    // Lock mutex
    if (nimcp_mutex_lock(&pool->mutex) != NIMCP_SUCCESS) {
        set_error("recovery_pool_reset: Failed to lock mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "recovery_pool_reset: validation failed");
        return false;
    }

    NIMCP_LOGGING_INFO("Resetting recovery pool (%u allocations, %zu bytes used)",
                       pool->allocation_count, pool->offset);

    // Free allocation tracking list
    allocation_entry_t* entry = pool->allocations;
    while (entry) {
        allocation_entry_t* next = entry->next;
        nimcp_free(entry);
        entry = next;
    }
    pool->allocations = NULL;
    pool->allocation_count = 0;

    // Reset bump allocator
    pool->offset = 0;

    // Update statistics
    pool->stats.current_used_bytes = 0;
    pool->stats.reset_count++;
    pool->stats.pool_exhausted = false;
    pool->stats.largest_free_block = pool->size;

    // Unlock mutex
    nimcp_mutex_unlock(&pool->mutex);

    return true;
}

bool recovery_pool_get_stats(const recovery_pool_t* pool, recovery_pool_stats_t* stats) {
    // Guard: NULL checks
    if (!pool || !stats) {
        set_error("recovery_pool_get_stats: NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_pool_get_stats: required parameter is NULL (pool, stats)");
        return false;
    }

    // Lock mutex
    nimcp_mutex_t* mutex = (nimcp_mutex_t*)&pool->mutex;
    if (nimcp_mutex_lock(mutex) != NIMCP_SUCCESS) {
        set_error("recovery_pool_get_stats: Failed to lock mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "recovery_pool_get_stats: validation failed");
        return false;
    }

    // Copy statistics
    memcpy(stats, &pool->stats, sizeof(recovery_pool_stats_t));

    // Update dynamic fields
    stats->allocation_count = pool->allocation_count;
    stats->is_emergency_mode = pool->emergency_mode;

    // Unlock mutex
    nimcp_mutex_unlock(mutex);

    return true;
}

bool recovery_pool_has_space(const recovery_pool_t* pool, size_t required_size) {
    // Guard: NULL check
    if (!pool) {
        return false;
    }

    // Lock mutex
    nimcp_mutex_t* mutex = (nimcp_mutex_t*)&pool->mutex;
    if (nimcp_mutex_lock(mutex) != NIMCP_SUCCESS) {
        return false;
    }

    size_t aligned_size = align_size(required_size);
    bool has_space = (pool->offset + aligned_size <= pool->size);

    // Unlock mutex
    nimcp_mutex_unlock(mutex);

    return has_space;
}

size_t recovery_pool_get_available(const recovery_pool_t* pool) {
    // Guard: NULL check
    if (!pool) {
        return 0;
    }

    // Lock mutex
    nimcp_mutex_t* mutex = (nimcp_mutex_t*)&pool->mutex;
    if (nimcp_mutex_lock(mutex) != NIMCP_SUCCESS) {
        return 0;
    }

    size_t available = pool->size - pool->offset;

    // Unlock mutex
    nimcp_mutex_unlock(mutex);

    return available;
}

//=============================================================================
// Public API - Debugging and Diagnostics
//=============================================================================

bool recovery_pool_validate(const recovery_pool_t* pool) {
    // Guard: NULL check
    if (!pool) {
        set_error("recovery_pool_validate: NULL pool");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_pool_validate: pool is NULL");
        return false;
    }

    // Lock mutex
    nimcp_mutex_t* mutex = (nimcp_mutex_t*)&pool->mutex;
    if (nimcp_mutex_lock(mutex) != NIMCP_SUCCESS) {
        set_error("recovery_pool_validate: Failed to lock mutex");
        return false;
    }

    bool result = validate_pool_internal(pool);

    // Unlock mutex
    nimcp_mutex_unlock(mutex);

    return result;
}

void recovery_pool_dump(const recovery_pool_t* pool) {
    // Guard: NULL check
    if (!pool) {
        printf("recovery_pool_dump: NULL pool\n");
        return;
    }

    // Lock mutex
    nimcp_mutex_t* mutex = (nimcp_mutex_t*)&pool->mutex;
    if (nimcp_mutex_lock(mutex) != NIMCP_SUCCESS) {
        printf("recovery_pool_dump: Failed to lock mutex\n");
        return;
    }

    printf("\n=== Recovery Pool Dump ===\n");
    printf("Pool Size:     %zu bytes (%zu KB)\n", pool->size, pool->size / 1024);
    printf("Current Used:  %zu bytes (%.1f%%)\n",
           pool->offset, (pool->offset * 100.0) / pool->size);
    printf("Available:     %zu bytes (%.1f%%)\n",
           pool->size - pool->offset, ((pool->size - pool->offset) * 100.0) / pool->size);
    printf("Allocations:   %u\n", pool->allocation_count);
    printf("Emergency Mode: %s\n", pool->emergency_mode ? "YES" : "NO");
    printf("Emergency Acts: %u\n", pool->emergency_activations);
    printf("\nStatistics:\n");
    printf("  Peak Used:        %zu bytes\n", pool->stats.peak_used_bytes);
    printf("  Total Allocated:  %zu bytes\n", pool->stats.total_allocated_bytes);
    printf("  Total Allocs:     %u\n", pool->stats.total_allocations);
    printf("  Failed Allocs:    %u\n", pool->stats.failed_allocations);
    printf("  Reset Count:      %u\n", pool->stats.reset_count);
    printf("  Pool Exhausted:   %s\n", pool->stats.pool_exhausted ? "YES" : "NO");

    printf("\nAllocations:\n");
    uint32_t i = 0;
    for (allocation_entry_t* entry = pool->allocations; entry; entry = entry->next) {
        printf("  [%u] ptr=%p size=%zu freed=%s\n",
               i++, entry->ptr, entry->size, entry->is_freed ? "YES" : "NO");
    }

    printf("========================\n\n");

    // Unlock mutex
    nimcp_mutex_unlock(mutex);
}

const char* recovery_pool_get_error(void) {
    return error_buffer;
}

void recovery_pool_clear_error(void) {
    error_buffer[0] = '\0';
}

//=============================================================================
// Public API - Integration Helpers
//=============================================================================

void recovery_pool_set_global(recovery_pool_t* pool) {
    g_global_pool = pool;

    if (pool) {
        NIMCP_LOGGING_INFO("Global recovery pool set (%zu bytes)", pool->size);
    } else {
        NIMCP_LOGGING_INFO("Global recovery pool cleared");
    }
}

recovery_pool_t* recovery_pool_get_global(void) {
    return g_global_pool;
}
