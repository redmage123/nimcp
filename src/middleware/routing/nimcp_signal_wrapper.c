
#define LOG_MODULE "nimcp_signal_wrapper"
#define LOG_MODULE_ID 0x052B
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(signal_wrapper)

//=============================================================================
// nimcp_signal_wrapper.c - CoW-Based Signal Reference Wrapper Implementation
//=============================================================================
/**
 * @file nimcp_signal_wrapper.c
 * @brief Implementation of CoW-based signal wrapper for zero-copy routing
 *
 * @author NIMCP Development Team
 * @date 2025-11-21
 */

#include "middleware/routing/nimcp_signal_wrapper.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_cow_manager.h"
#include "api/nimcp_api_exception.h"
#include <string.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/exception/nimcp_exception_macros.h"


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Signal wrapper structure
 */
struct signal_wrapper_struct {
    // CoW handles for shared data
    cow_handle_t dest_ids_handle;    /**< CoW handle for destination IDs */
    cow_handle_t signal_data_handle; /**< CoW handle for signal data */

    // CoW managers (owned by wrapper)
    cow_manager_t dest_ids_manager;  /**< Manager for destination IDs */
    cow_manager_t signal_data_manager; /**< Manager for signal data */

    // Metadata
    uint32_t num_destinations;       /**< Number of destination IDs */
    uint32_t signal_size;            /**< Number of signal values */

    // Reference tracking
    bool is_owner;                   /**< True if this is the original wrapper */
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Copy function for destination IDs
 */
static bool copy_dest_ids(void* dest, const void* src, size_t size, void* user_data) {
    (void)user_data;
    memcpy(dest, src, size);
    return true;
}

/**
 * @brief Copy function for signal data
 */
static bool copy_signal_data(void* dest, const void* src, size_t size, void* user_data) {
    (void)user_data;
    memcpy(dest, src, size);
    return true;
}

//=============================================================================
// API Implementation
//=============================================================================

NIMCP_EXPORT signal_wrapper_t signal_wrapper_create(
    const uint32_t* dest_ids,
    uint32_t num_destinations,
    const float* signal_data,
    uint32_t signal_size
) {
    // Guard: validate inputs
    if (!dest_ids || num_destinations == 0 || !signal_data || signal_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "signal_wrapper_create: required parameter is NULL (dest_ids, signal_data)");
        return NULL;
    }

    // Allocate wrapper structure
    signal_wrapper_t wrapper = nimcp_calloc(1, sizeof(struct signal_wrapper_struct));
    if (!wrapper) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wrapper is NULL");

        return NULL;

    }

    // Store metadata
    wrapper->num_destinations = num_destinations;
    wrapper->signal_size = signal_size;
    wrapper->is_owner = true;

    // Create CoW manager for destination IDs
    cow_manager_config_t dest_config = cow_default_config(
        num_destinations * sizeof(uint32_t),
        NULL  // Use default malloc for CoW data
    );
    dest_config.copy_fn = copy_dest_ids;

    wrapper->dest_ids_manager = cow_manager_create(&dest_config, dest_ids);
    if (!wrapper->dest_ids_manager) {
        nimcp_free(wrapper);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "signal_wrapper_create: wrapper->dest_ids_manager is NULL");
        return NULL;
    }

    // Create CoW manager for signal data
    cow_manager_config_t signal_config = cow_default_config(
        signal_size * sizeof(float),
        NULL  // Use default malloc for CoW data
    );
    signal_config.copy_fn = copy_signal_data;

    wrapper->signal_data_manager = cow_manager_create(&signal_config, signal_data);
    if (!wrapper->signal_data_manager) {
        cow_manager_destroy(wrapper->dest_ids_manager);
        nimcp_free(wrapper);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "signal_wrapper_create: wrapper->signal_data_manager is NULL");
        return NULL;
    }

    // Acquire initial CoW handles
    wrapper->dest_ids_handle = cow_acquire(wrapper->dest_ids_manager);
    wrapper->signal_data_handle = cow_acquire(wrapper->signal_data_manager);

    if (!wrapper->dest_ids_handle || !wrapper->signal_data_handle) {
        // Cleanup on failure
        if (wrapper->dest_ids_handle) cow_release(wrapper->dest_ids_handle);
        if (wrapper->signal_data_handle) cow_release(wrapper->signal_data_handle);
        cow_manager_destroy(wrapper->dest_ids_manager);
        cow_manager_destroy(wrapper->signal_data_manager);
        nimcp_free(wrapper);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "signal_wrapper_create: validation failed");
        return NULL;
    }

    return wrapper;
}

NIMCP_EXPORT signal_wrapper_t signal_wrapper_acquire(signal_wrapper_t wrapper) {
    // Guard: validate input
    if (!wrapper) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wrapper is NULL");

        return NULL;

    }

    /* P2 fix: Validate source wrapper's CoW handles are valid
     * WHY:  If source wrapper is in invalid state (e.g., after partial failure),
     *       attempting to acquire would create inconsistent reference.
     */
    if (!wrapper->dest_ids_handle || !wrapper->signal_data_handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "signal_wrapper_acquire: required parameter is NULL (wrapper->dest_ids_handle, wrapper->signal_data_handle)");
        return NULL;
    }
    if (!wrapper->dest_ids_manager || !wrapper->signal_data_manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "signal_wrapper_acquire: required parameter is NULL (wrapper->dest_ids_manager, wrapper->signal_data_manager)");
        return NULL;
    }

    // Allocate new wrapper structure (shares managers and handles)
    signal_wrapper_t new_wrapper = nimcp_calloc(1, sizeof(struct signal_wrapper_struct));
    if (!new_wrapper) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "new_wrapper is NULL");

        return NULL;

    }

    // Copy metadata
    new_wrapper->num_destinations = wrapper->num_destinations;
    new_wrapper->signal_size = wrapper->signal_size;
    new_wrapper->is_owner = false;  // Not owner, just a reference

    // Store manager pointers so non-owner can query refcount and handles
    // remain valid. Only the owner destroys managers (after checking refcount).
    new_wrapper->dest_ids_manager = wrapper->dest_ids_manager;
    new_wrapper->signal_data_manager = wrapper->signal_data_manager;

    // Acquire new CoW handles (refcount++)
    new_wrapper->dest_ids_handle = cow_acquire(wrapper->dest_ids_manager);
    new_wrapper->signal_data_handle = cow_acquire(wrapper->signal_data_manager);

    if (!new_wrapper->dest_ids_handle || !new_wrapper->signal_data_handle) {
        // Cleanup on failure
        if (new_wrapper->dest_ids_handle) cow_release(new_wrapper->dest_ids_handle);
        if (new_wrapper->signal_data_handle) cow_release(new_wrapper->signal_data_handle);
        nimcp_free(new_wrapper);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "signal_wrapper_acquire: validation failed");
        return NULL;
    }

    return new_wrapper;
}

NIMCP_EXPORT void signal_wrapper_release(signal_wrapper_t wrapper) {
    // Guard: validate input
    if (!wrapper) return;

    // Release CoW handles
    if (wrapper->dest_ids_handle) {
        cow_release(wrapper->dest_ids_handle);
    }
    if (wrapper->signal_data_handle) {
        cow_release(wrapper->signal_data_handle);
    }

    // If this is the owner, destroy managers only when no more active handles
    if (wrapper->is_owner) {
        if (wrapper->dest_ids_manager) {
            cow_stats_t stats;
            bool has_stats = cow_get_stats(wrapper->dest_ids_manager, &stats);
            if (!has_stats || (stats.active_shared + stats.active_private) == 0) {
                cow_manager_destroy(wrapper->dest_ids_manager);
            }
        }
        if (wrapper->signal_data_manager) {
            cow_stats_t stats;
            bool has_stats = cow_get_stats(wrapper->signal_data_manager, &stats);
            if (!has_stats || (stats.active_shared + stats.active_private) == 0) {
                cow_manager_destroy(wrapper->signal_data_manager);
            }
        }
    }

    // Free wrapper structure
    nimcp_free(wrapper);
}

NIMCP_EXPORT const uint32_t* signal_wrapper_read_destinations(
    signal_wrapper_t wrapper,
    uint32_t* num_destinations_out
) {
    // Guard: validate inputs
    if (!wrapper || !wrapper->dest_ids_handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "signal_wrapper_read_destinations: required parameter is NULL (wrapper, wrapper->dest_ids_handle)");
        return NULL;
    }

    // Return metadata if requested
    if (num_destinations_out) {
        *num_destinations_out = wrapper->num_destinations;
    }

    // Return read-only pointer (shared data)
    return (const uint32_t*)cow_read(wrapper->dest_ids_handle);
}

NIMCP_EXPORT const float* signal_wrapper_read_data(
    signal_wrapper_t wrapper,
    uint32_t* signal_size_out
) {
    // Guard: validate inputs
    if (!wrapper || !wrapper->signal_data_handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "signal_wrapper_read_data: required parameter is NULL (wrapper, wrapper->signal_data_handle)");
        return NULL;
    }

    // Return metadata if requested
    if (signal_size_out) {
        *signal_size_out = wrapper->signal_size;
    }

    // Return read-only pointer (shared data)
    return (const float*)cow_read(wrapper->signal_data_handle);
}

NIMCP_EXPORT uint32_t* signal_wrapper_write_destinations(
    signal_wrapper_t wrapper,
    uint32_t* num_destinations_out
) {
    // Guard: validate inputs
    if (!wrapper || !wrapper->dest_ids_handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "signal_wrapper_write_destinations: required parameter is NULL (wrapper, wrapper->dest_ids_handle)");
        return NULL;
    }

    // Return metadata if requested
    if (num_destinations_out) {
        *num_destinations_out = wrapper->num_destinations;
    }

    // Return writable pointer (triggers CoW if shared)
    return (uint32_t*)cow_write(wrapper->dest_ids_handle);
}

NIMCP_EXPORT float* signal_wrapper_write_data(
    signal_wrapper_t wrapper,
    uint32_t* signal_size_out
) {
    // Guard: validate inputs
    if (!wrapper || !wrapper->signal_data_handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "signal_wrapper_write_data: required parameter is NULL (wrapper, wrapper->signal_data_handle)");
        return NULL;
    }

    // Return metadata if requested
    if (signal_size_out) {
        *signal_size_out = wrapper->signal_size;
    }

    // Return writable pointer (triggers CoW if shared)
    return (float*)cow_write(wrapper->signal_data_handle);
}

NIMCP_EXPORT bool signal_wrapper_is_shared(signal_wrapper_t wrapper) {
    // Guard: validate input
    if (!wrapper) {
        return false;
    }

    // Check if refcount > 1 (multiple wrappers share the data)
    return signal_wrapper_refcount(wrapper) > 1;
}

NIMCP_EXPORT size_t signal_wrapper_refcount(signal_wrapper_t wrapper) {
    // Guard: validate input
    if (!wrapper || !wrapper->dest_ids_manager) return 0;

    // Get stats from dest_ids manager (representative)
    cow_stats_t stats;
    if (!cow_get_stats(wrapper->dest_ids_manager, &stats)) {
        return 0;
    }

    // Return total active handles (shared + private)
    return stats.active_shared + stats.active_private;
}
