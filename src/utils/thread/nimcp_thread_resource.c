#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_thread_resource.c - Named Resource Lock Management
//=============================================================================

#include "utils/thread/nimcp_thread.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <errno.h>
#include <string.h>
#include <pthread.h>

#define LOG_MODULE "thread_resource"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(thread_resource)

// External declarations (defined in nimcp_thread.c)
extern void set_thread_error(int error_code, const char* format, ...);
extern resource_lock_table_t resource_table;
extern nimcp_result_t nimcp_thread_init(void);

//=============================================================================
// Named Resource Locks
//=============================================================================

/**
 * @brief Hash string to bucket index (DJBX33A hash)
 *
 * WHY HASH FUNCTION:
 * - Map arbitrary strings to fixed bucket range [0, 255]
 * - Distribute keys evenly across buckets
 * - Fast computation (one multiply + add per character)
 *
 * ALGORITHM (DJBX33A):
 * 1. Initialize hash = 5381 (prime seed)
 * 2. For each character c:
 *    hash = hash * 33 + c
 * 3. Modulo 256 (bucket count)
 *
 * WHY DJBX33A (DJB2):
 * - Fast: One multiply, one add per character (no division)
 * - Good distribution: Prime multiplier (33) spreads bits
 * - Low collision rate: ~1-2% for typical string sets
 * - Widely used: Proven in many hash table implementations
 *
 * COMPLEXITY: O(n) where n = string length (typically 10-30 characters)
 * THREAD SAFETY: Fully safe (read-only, no shared state)
 *
 * @param str String to hash
 * @return Bucket index in [0, 255]
 */
static unsigned int hash_string(const char* str)
{
    unsigned int hash = 5381;  // Prime seed
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c
    }

    return hash % RESOURCE_LOCK_BUCKETS;  // Map to [0, 255]
}

/**
 * @brief Get or create named resource lock (Registry pattern)
 *
 * WHY NAMED LOCKS:
 * - Intuitive: "brain_network" more readable than &global_mutex_42
 * - Automatic sharing: Same name = same lock across all modules
 * - Lifecycle management: Reference counting + automatic cleanup
 * - No global variables: Locks created on demand
 *
 * ALGORITHM:
 * 1. Initialize resource table if needed (lazy init)
 * 2. Compute hash bucket for resource_id
 * 3. Acquire bucket mutex (fine-grained locking)
 * 4. Search bucket's entry list for resource_id
 * 5. If found:
 *    a. Increment ref_count (another holder)
 *    b. Return existing mutex
 * 6. If not found (first request):
 *    a. Allocate resource_entry_t
 *    b. Duplicate resource_id string
 *    c. Allocate and initialize mutex
 *    d. Set ref_count = 1
 *    e. Add to head of bucket's entry list
 *    f. Return new mutex
 * 7. Release bucket mutex
 *
 * WHY REFERENCE COUNTING:
 * - Automatic cleanup: Last release destroys lock
 * - Safe sharing: Lock lives while any holder exists
 * - Memory efficient: Lock freed when not needed
 *
 * TYPICAL USAGE:
 *   nimcp_mutex_t* lock;
 *   nimcp_get_resource_lock("brain_network", &lock);
 *   nimcp_mutex_lock(lock);
 *   // ... critical section ...
 *   nimcp_mutex_unlock(lock);
 *   nimcp_release_resource_lock("brain_network");
 *
 * COMPLEXITY:
 * - Average: O(1) (hash + short list search)
 * - Worst: O(n) where n = entries in bucket (rare collision)
 * - Typical: O(1) with ~0-1 entries per bucket
 *
 * THREAD SAFETY: Fully safe (bucket mutex protects entry list)
 *
 * @param resource_id String identifier for resource
 * @param mutex Output parameter for mutex pointer
 * @return NIMCP_SUCCESS or NIMCP_ERROR_*
 */
nimcp_result_t nimcp_get_resource_lock(const char* resource_id, nimcp_mutex_t** mutex)
{
    // Validate parameters
    if (!resource_id || !mutex) {
        set_thread_error(NIMCP_ERROR_INVALID_PARAM, "Invalid resource lock parameters");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "Invalid resource lock parameters");
    }

    // Lazy initialization: Ensure table is initialized
    if (!resource_table.initialized) {
        nimcp_thread_init();
    }

    // Compute hash bucket (DJBX33A hash % 256)
    unsigned int bucket = hash_string(resource_id);

    // Acquire bucket mutex (fine-grained lock)
    nimcp_platform_mutex_lock(&resource_table.buckets[bucket].bucket_mutex);

    // Search bucket's entry list for resource_id
    resource_entry_t* entry = resource_table.buckets[bucket].entries;
    while (entry) {
        if (strcmp(entry->resource_id, resource_id) == 0) {
            // FOUND: Resource already exists, increment reference count
            entry->ref_count++;
            *mutex = entry->lock;
            nimcp_platform_mutex_unlock(&resource_table.buckets[bucket].bucket_mutex);
            return NIMCP_SUCCESS;
        }
        entry = entry->next;
    }

    // NOT FOUND: Create new resource entry (first request)

    // Allocate entry structure
    entry = nimcp_malloc(sizeof(resource_entry_t));
    if (!entry) {
        nimcp_platform_mutex_unlock(&resource_table.buckets[bucket].bucket_mutex);
        set_thread_error(NIMCP_ERROR_MEMORY, "Failed to allocate resource entry");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_MEMORY, "Failed to allocate resource entry");
    }

    // Duplicate resource_id string (use nimcp_malloc to match nimcp_free below)
    size_t id_len = strlen(resource_id);
    entry->resource_id = nimcp_malloc(id_len + 1);
    if (entry->resource_id) {
        strncpy(entry->resource_id, resource_id, id_len);
        entry->resource_id[id_len] = '\0';
    }

    // Allocate mutex structure
    entry->lock = nimcp_malloc(sizeof(nimcp_mutex_t));

    // Check allocations
    if (!entry->resource_id || !entry->lock) {
        // Cleanup partial allocation
        nimcp_free(entry->resource_id);
        nimcp_free(entry->lock);
        nimcp_free(entry);
        nimcp_platform_mutex_unlock(&resource_table.buckets[bucket].bucket_mutex);
        set_thread_error(NIMCP_ERROR_MEMORY, "Failed to allocate resource lock");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_MEMORY, "Failed to allocate resource lock");
    }

    // Initialize mutex using platform abstraction
    if (nimcp_platform_mutex_init(entry->lock, false) != 0) {
        // Cleanup on init failure
        nimcp_free(entry->resource_id);
        nimcp_free(entry->lock);
        nimcp_free(entry);
        nimcp_platform_mutex_unlock(&resource_table.buckets[bucket].bucket_mutex);
        set_thread_error(NIMCP_ERROR_SYSTEM, "Failed to initialize resource mutex");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM, "Failed to initialize resource mutex");
    }

    // Set initial reference count
    entry->ref_count = 1;

    // Add to head of bucket's entry list (O(1) insertion)
    entry->next = resource_table.buckets[bucket].entries;
    resource_table.buckets[bucket].entries = entry;

    // Return mutex to caller
    *mutex = entry->lock;

    nimcp_platform_mutex_unlock(&resource_table.buckets[bucket].bucket_mutex);
    return NIMCP_SUCCESS;
}

/**
 * @brief Release named resource lock (decrement reference count)
 *
 * WHY RELEASE:
 * - Decrement reference count (holder no longer needs lock)
 * - Automatic cleanup: Destroy lock when ref_count reaches 0
 * - Memory efficiency: Free locks that are no longer needed
 *
 * ALGORITHM:
 * 1. Validate resource_id
 * 2. Compute hash bucket
 * 3. Acquire bucket mutex
 * 4. Search bucket's entry list for resource_id
 * 5. If found:
 *    a. Decrement ref_count
 *    b. If ref_count == 0 (last holder):
 *       - Remove from entry list
 *       - Destroy mutex
 *       - Free resource_id string
 *       - Free entry structure
 * 6. If not found: Return NIMCP_ERROR_NOT_FOUND
 * 7. Release bucket mutex
 *
 * WHY DESTROY ON ref_count==0:
 * - No holders remain (safe to destroy)
 * - Free memory (no memory leak)
 * - Free kernel resources (futex on Linux)
 *
 * TYPICAL USAGE:
 *   nimcp_mutex_t* lock;
 *   nimcp_get_resource_lock("brain_network", &lock);  // ref=1
 *   nimcp_mutex_lock(lock);
 *   // ... work ...
 *   nimcp_mutex_unlock(lock);
 *   nimcp_release_resource_lock("brain_network");  // ref=0, destroyed
 *
 * COMPLEXITY: O(n) where n = entries in bucket (typically 0-2)
 * THREAD SAFETY: Fully safe (bucket mutex protects entry list)
 *
 * @param resource_id String identifier for resource
 * @return NIMCP_SUCCESS or NIMCP_ERROR_INVALID_PARAM or NIMCP_ERROR_NOT_FOUND
 */
nimcp_result_t nimcp_release_resource_lock(const char* resource_id)
{
    NIMCP_CHECK_THROW(resource_id, NIMCP_ERROR_INVALID_PARAM, "resource_id is NULL");

    // Compute hash bucket
    unsigned int bucket = hash_string(resource_id);

    // Acquire bucket mutex
    nimcp_platform_mutex_lock(&resource_table.buckets[bucket].bucket_mutex);

    // Search bucket's entry list for resource_id
    resource_entry_t* entry = resource_table.buckets[bucket].entries;
    resource_entry_t* prev = NULL;

    while (entry) {
        if (strcmp(entry->resource_id, resource_id) == 0) {
            // FOUND: Decrement reference count
            entry->ref_count--;

            if (entry->ref_count == 0) {
                // Last reference: Remove from list and destroy

                // Remove from linked list
                if (prev) {
                    // Middle or end of list
                    prev->next = entry->next;
                } else {
                    // Head of list
                    resource_table.buckets[bucket].entries = entry->next;
                }

                // Destroy mutex using platform abstraction (free kernel resources)
                nimcp_platform_mutex_destroy(entry->lock);

                // Free allocated memory
                nimcp_free(entry->lock);
                nimcp_free(entry->resource_id);  // Free duplicated string
                nimcp_free(entry);               // Free entry structure
            }

            nimcp_platform_mutex_unlock(&resource_table.buckets[bucket].bucket_mutex);
            return NIMCP_SUCCESS;
        }

        // Move to next entry
        prev = entry;
        entry = entry->next;
    }

    // NOT FOUND: Resource doesn't exist
    nimcp_platform_mutex_unlock(&resource_table.buckets[bucket].bucket_mutex);
    NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "Resource lock not found");
}
