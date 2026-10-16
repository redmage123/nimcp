//=============================================================================
// nimcp_brain_validation.c - Brain Creation Validation
//=============================================================================
/**
 * @file nimcp_brain_validation.c
 * @brief Validation and helper functions for brain creation
 *
 * WHAT: Parameter validation and caching helpers for brain factory
 * WHY:  Separates validation logic from initialization and creation
 * HOW:  Provides validators and decision caching for efficiency
 *
 * EXTRACTED FROM: nimcp_brain_factory.c
 * DATE: 2025-11-19
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 */

//=============================================================================
// Includes
//=============================================================================

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

// Logging integration
#include "utils/logging/nimcp_logging.h"

// Unified memory integration
#include "utils/memory/nimcp_unified_memory.h"

#include "core/brain/factory/validation/nimcp_brain_validation.h"
#include "core/brain/factory/nimcp_brain_factory.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cjson/cJSON.h>

// Core dependencies
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/cache/nimcp_cache.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_security.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BRAIN_VALID"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_validation, MESH_ADAPTER_CATEGORY_COGNITIVE)


// External error function declaration
extern void set_error(const char* format, ...);

// External decision copy function declaration
extern brain_decision_t* copy_decision_deep(const brain_decision_t* decision);

//=============================================================================
// Validation Functions
//=============================================================================

bool nimcp_brain_factory_is_cached_input(brain_t brain, const float* features, uint32_t num_features)
{
    if (!brain->last_input || !brain->cached_decision) {
        return false;  /* No cache yet - normal */
    }
    if (brain->input_size != num_features) {
        return false;  /* Size mismatch - normal cache miss */
    }

    return memcmp(brain->last_input, features, num_features * sizeof(float)) == 0;
}

/**
 * @brief Cache decision for input
 *
 * WHY: Store decision for potential reuse
 * Improves batch processing performance
 *
 * COMPLEXITY: O(n) for input copy
 *
 * @param brain Brain handle
 * @param features Input to cache
 * @param num_features Feature count
 * @param decision Decision to cache
 */
void nimcp_brain_factory_cache_decision(brain_t brain, const float* features, uint32_t num_features,
                           brain_decision_t* decision)
{
    // CRITICAL: This function must only be called while cache_mutex is locked!
    // Caller is responsible for mutex protection.
#ifndef NDEBUG
    // DEBUG: Verify caller holds the cache_mutex (trylock should fail if held)
    int trylock_result = nimcp_platform_mutex_trylock(&brain->cache_mutex);
    if (trylock_result == 0) {
        // We acquired the lock, which means caller didn't hold it - programming error!
        nimcp_platform_mutex_unlock(&brain->cache_mutex);
        LOG_MODULE_ERROR(LOG_MODULE, "CRITICAL: nimcp_brain_factory_cache_decision called without cache_mutex locked!");
        return;  // Abort to prevent data race
    }
    // If trylock_result == EBUSY, that's expected - lock is held (presumably by caller)
#endif

    // Resize input buffer if needed (defensive: handle size changes)
    if (!brain->last_input || brain->input_size != num_features) {
        nimcp_free(brain->last_input);  // Free old buffer (safe if NULL)
        brain->last_input = nimcp_malloc(num_features * sizeof(float));
        if (!brain->last_input) {
            set_error("Failed to allocate cache input buffer");
            return;
        }
        brain->input_size = num_features;
    }

    memcpy(brain->last_input, features, num_features * sizeof(float));

    // Create new decision copy FIRST (before freeing old)
    // This reduces the race window where cached_decision could be NULL
    brain_decision_t* new_cached = copy_decision_deep(decision);
    if (!new_cached) {
        set_error("Failed to copy decision for cache");
        return;
    }

    // Now atomically replace old cached decision
    brain_decision_t* old_cached = brain->cached_decision;
    brain->cached_decision = new_cached;

    // Free old decision AFTER replacement (cache always has valid decision)
    if (old_cached) {
        brain_free_decision(old_cached);
    }
}

/**
 * @brief Clear decision cache (thread-safe)
 *
 * WHAT: Invalidates cached input and decision
 * WHY:  Cache must be cleared after network modifications
 * HOW:  Mutex-protected deallocation of cache structures
 *
 * BIOLOGICAL RATIONALE:
 * Thread-safe cache invalidation mimics synaptic reorganization that
 * invalidates previously stable neural response patterns. When synaptic
 * weights change (learning/pruning), cached neural activations become
 * obsolete, requiring recomputation from modified connectivity.
 *
 * COMPLEXITY: O(1)
 *
 * @param brain Brain handle
 */
void nimcp_brain_factory_clear_cache(brain_t brain)
{
    // Guard: Validate parameters
    if (!brain) {
        return;
    }

    // Lock cache mutex for thread-safe invalidation
    if (nimcp_platform_mutex_lock(&brain->cache_mutex) != 0) {
        set_error("Failed to lock cache mutex during clear_cache");
        return;
    }

    // Free cached input vector
    nimcp_free(brain->last_input);
    brain->last_input = NULL;

    // Free cached decision
    if (brain->cached_decision) {
        brain_free_decision(brain->cached_decision);
        brain->cached_decision = NULL;
    }

    // Always attempt unlock, even if operations above failed
    // Store result to check for critical errors
    int unlock_result = nimcp_platform_mutex_unlock(&brain->cache_mutex);
    if (unlock_result != 0) {
        // CRITICAL: Mutex unlock failed - cache may be permanently locked!
        // This is a severe error that could deadlock future operations.
        set_error("CRITICAL: Failed to unlock cache mutex in clear_cache - potential deadlock");
    }
}

//=============================================================================
// Brain Factory - Creation with Validation
//=============================================================================

/**
 * @brief Validate brain creation parameters
 *
 * WHY: Guard clause pattern - early exit on invalid input
 * Prevents invalid state propagation
 *
 * COMPLEXITY: O(1)
 *
 * @param task_name Brain name
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @return true if valid
 */
bool nimcp_brain_factory_validate_creation_params(const char* task_name, uint32_t num_inputs,
                                     uint32_t num_outputs)
{
    if (!task_name) {
        set_error("task_name cannot be NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_validate_creation_params: task_name is NULL");
        return false;
    }

    if (num_inputs == 0) {
        set_error("num_inputs must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_factory_validate_creation_params: num_inputs is zero");
        return false;
    }

    if (num_inputs > 10000) {
        set_error("num_inputs must be <= 10000");
        return false;
    }

    if (num_outputs == 0) {
        set_error("num_outputs must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_factory_validate_creation_params: num_outputs is zero");
        return false;
    }

    if (num_outputs > 10000) {
        set_error("num_outputs must be <= 10000");
        return false;
    }

    return true;
}

/**
 * @brief Allocate and initialize brain structure
 *
 * WHY: Separates allocation from configuration
 * Makes memory management explicit
 *
 * COMPLEXITY: O(1)
 *
 * @return Allocated brain or NULL on error
 */
