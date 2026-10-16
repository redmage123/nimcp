//=============================================================================
// nimcp_brain_distributed.c - Brain Distributed Operations Implementation
//=============================================================================
/**
 * @file nimcp_brain_distributed.c
 * @brief Brain distributed operations: Copy-on-Write and P2P synchronization
 *
 * WHAT: Distributed brain operations combining COW cloning and P2P coordination
 * WHY:  Enable efficient brain replication and multi-brain collaborative learning
 * HOW:  Combines COW optimization with distributed cognition for scalable systems
 *
 * EXTRACTED FROM: nimcp_brain.c
 * - Lines 4251-4446: Copy-on-Write API (brain_clone*, brain_cow_*)
 * - Lines 8599-8792: Distributed Brain API (brain_sync*, brain_p2p*, brain_consensus*)
 *
 * ARCHITECTURE:
 * - Copy-on-Write (COW): Efficient brain cloning with shared networks
 * - Distributed Coordination: P2P multi-brain synchronization
 * - Reference Counting: Safe shared resource management
 *
 * DESIGN PATTERNS:
 * - Strategy Pattern: Task-specific behaviors inherited from original
 * - Factory Pattern: brain_create_distributed creates configured instances
 * - Mediator Pattern: Distributed cognition coordinates P2P communication
 *
 * PERFORMANCE:
 * - COW cloning: <10ms vs ~350ms for full copy
 * - COW memory: ~1MB overhead vs ~50MB for full copy
 * - Distributed sync: O(P) where P = peer count
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.7
 */

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

// Logging integration
#include "utils/logging/nimcp_logging.h"

#include "core/brain/distributed/nimcp_brain_distributed.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/containers/nimcp_hash_table.h"
#include "utils/cache/nimcp_cache.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_once.h"

#define LOG_MODULE "BRAIN_DISTRIBUTED"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_distributed, MESH_ADAPTER_CATEGORY_COGNITIVE)


// NOTE: COW Clone Synchronization is now lock-free using atomic operations.
// The previous mutex-based approach had a race condition vulnerability.
// See nimcp_brain_internal.h for the new atomic reference counting design.

//=============================================================================
// Error Handling
//=============================================================================

/**
 * @brief Set error message with printf-style formatting
 *
 * NOTE: The actual thread-local error buffer is defined in:
 *       src/core/brain/strategy/nimcp_brain_strategy.c (line 341)
 *
 * This function is declared here but implemented in nimcp_brain_strategy.c
 * to ensure all brain modules use the same error storage.
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Uses thread-local storage
 *
 * @param format Printf-style format string
 */
extern void set_error(const char* format, ...);

//=============================================================================
// Forward Declarations (Internal Brain Functions)
//=============================================================================

/**
 * These functions are declared in nimcp_brain.h but need to be accessible
 * here. They're part of the brain API.
 */

// From nimcp_brain.c - brain allocation and creation
extern brain_t brain_create(
    const char* task_name,
    brain_size_t size,
    brain_task_t task,
    uint32_t num_inputs,
    uint32_t num_outputs
);

// From nimcp_brain.c - brain destruction
extern void brain_destroy(brain_t brain);

// From nimcp_brain.c - internal brain structure access
// Note: brain_struct is opaque in header, but we need these internals
// The actual brain_struct definition is in nimcp_brain.c

//=============================================================================
// Internal Helper: Allocate Brain Structure
//=============================================================================

/**
 * @brief Allocate and initialize brain structure
 *
 * WHAT: Creates empty brain_t with initialized fields
 * WHY:  Needed for COW cloning without full brain_create
 * HOW:  Allocates struct, initializes all fields to safe defaults
 *
 * NOTE: This is a simplified version for COW clones. Full brain_create
 *       does much more initialization (glial, oscillations, etc.)
 *
 * @return Allocated brain or NULL on failure
 */
static brain_t allocate_brain_simple(void)
{
    // Allocate brain structure
    brain_t brain = nimcp_calloc(1, sizeof(struct brain_struct));
    if (!brain) {
        set_error("Failed to allocate brain structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "allocate_brain_simple: brain is NULL");
        return NULL;
    }

    // Initialize basic fields
    brain->last_input = NULL;
    brain->cached_decision = NULL;
    brain->input_size = 0;

    // Initialize cache mutex for thread-safe access
    if (nimcp_platform_mutex_init(&brain->cache_mutex, false) != 0) {
        set_error("Failed to initialize cache mutex");
        nimcp_free(brain);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "allocate_brain_simple: validation failed");
        return NULL;
    }

    brain->distributed = NULL;

    // Initialize long-term memory consolidation buffer
    brain->longterm_capacity = 100;
    brain->longterm_count = 0;
    brain->longterm_memory = nimcp_calloc(brain->longterm_capacity,
                                          sizeof(*brain->longterm_memory));
    if (!brain->longterm_memory) {
        brain->longterm_capacity = 0;
    }

    // Initialize COW fields
    brain->is_cow_clone = false;
    brain->owns_network = true;
    brain->original_network = NULL;
    brain->network_is_cached = false;

    // Initialize reference counting fields (atomic operations)
    brain->network_refcount_atomic = NULL;
    brain->can_use_readonly = false;

    // Initialize snapshot fields
    brain->is_snapshot = false;

    // Layer C: temperature scaling defaults — T=1.0 is identity (no behavior
    // change), ece=-1 marks "never calibrated". calloc would have left T=0.0,
    // which would produce divide-by-zero in nimcp_apply_temperature.
    brain->decoder_temperature                  = 1.0F;
    brain->decoder_temperature_calibrated_ece   = -1.0F;
    brain->decoder_temperature_calibrated_at_us = 0;

    return brain;
}

//=============================================================================
// Copy-on-Write Brain Cloning Implementation
//=============================================================================

/**
 * @brief Ensure brain has writable network (trigger COW if needed)
 *
 * WHAT: Detects COW clone and makes private copy before write
 * WHY:  Prevent modifying shared network, maintain data safety
 * HOW:  Check is_cow_clone flag, copy network if true
 *
 * THREAD SAFETY: Not thread-safe - caller must ensure exclusive access
 * PERFORMANCE: O(n) where n = network size (only on first write)
 *
 * @param brain Brain handle
 * @return true on success (or if already writable), false on error
 */
static bool ensure_writable_network(brain_t brain)
{
    // Guard: Validate parameter
    if (!brain) {
        set_error("NULL brain in ensure_writable_network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ensure_writable_network: brain is NULL");
        return false;
    }

    // If not a COW clone, network is already writable
    if (!brain->is_cow_clone) {
        return true;
    }

    // COW clone detected - need to make private copy
    // For Phase 2, we'll create a full copy of the network
    if (!brain->network) {
        set_error("COW clone has NULL network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ensure_writable_network: brain->network is NULL");
        return false;
    }

    // Save the original network pointer
    adaptive_network_t shared_network = brain->network;

    // Phase 2 workaround: Use save/load to clone the network
    // TODO: Phase 3 should implement proper adaptive_network_clone() or incremental COW

    // Generate unique temporary filename
    char temp_file[NIMCP_SHORT_PATH_SIZE];
    snprintf(temp_file, sizeof(temp_file), "/tmp/nimcp_cow_temp_%p_%ld.bin",
             (void*)brain, (long)time(NULL));

    // Save shared network to temp file
    if (!adaptive_network_save(shared_network, temp_file, SERIALIZE_FORMAT_BINARY)) {
        set_error("Failed to save network for COW copy");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ensure_writable_network: adaptive_network_save is NULL");
        return false;
    }

    // Load into new network
    brain->network = adaptive_network_load(temp_file);

    // Clean up temp file
    unlink(temp_file);

    if (!brain->network) {
        // Failed to load - restore shared network and fail
        brain->network = shared_network;
        set_error("Failed to load network copy for COW");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ensure_writable_network: brain->network is NULL");
        return false;
    }

    // Successfully made private copy of network
    // Note: Keep is_cow_clone = true because strategy is still shared!
    // But now we own the network and can destroy it
    brain->owns_network = true;
    brain->original_network = NULL;

    brain_clear_error();
    return true;
}

/**
 * @brief Clone brain using copy-on-write optimization
 *
 * WHAT: Creates lightweight clone sharing network with original
 * WHY:  Enable efficient replication (86% memory savings)
 * HOW:  Shares adaptive_network_t, copies metadata
 *
 * PERFORMANCE: <10ms vs ~350ms for full copy
 * MEMORY: ~1MB overhead vs ~50MB for full copy
 *
 * @param original Brain to clone
 * @return Cloned brain or NULL on error
 */
brain_t brain_clone_cow(brain_t original)
{
    if (!original) {
        set_error("Cannot clone NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_clone_cow: original is NULL");
        return NULL;
    }

    if (!original->network) {
        set_error("Cannot clone brain with NULL network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_clone_cow: original->network is NULL");
        return NULL;
    }

    // Allocate clone structure
    brain_t clone = allocate_brain_simple();
    if (!clone) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "clone is NULL");

        return NULL;
    }

    // Copy config and metadata (small, private per brain)
    clone->config = original->config;
    clone->num_output_labels = original->num_output_labels;
    clone->input_size = original->input_size;

    // Share network via direct pointer (Phase 2: Simple sharing)
    // The network itself is shared - both brains point to same network
    clone->network = original->network;
    clone->original_network = original->network;

    // Mark as COW clone
    clone->is_cow_clone = true;
    clone->owns_network = false;  // Doesn't own network - it's shared
    clone->network_is_cached = false;  // Not using nimcp_cache yet

    // Phase 3: Set up atomic reference counting for shared network
    //
    // CONCURRENCY MODEL: Use atomic compare-and-swap for lock-free initialization
    // of the shared refcount. Only one thread will successfully allocate and
    // initialize the refcount; other threads will use the already-allocated one.
    //
    // MEMORY ORDERING:
    // - Use ACQ_REL on CAS to ensure proper visibility of the refcount value
    // - Use ACQUIRE on atomic load to see the latest refcount state
    // - Use RELEASE on atomic store to make our writes visible to other threads
    //
    // ALGORITHM:
    // 1. Atomically load original's refcount pointer
    // 2. If NULL, try to CAS in a newly allocated refcount (initialized to 2)
    // 3. If CAS fails, someone else beat us - free our allocation and use theirs
    // 4. If non-NULL, atomically increment the existing refcount

    _Atomic(uint32_t)* expected = __atomic_load_n(&original->network_refcount_atomic, __ATOMIC_ACQUIRE);

    if (!expected) {
        // First clone - try to initialize shared refcount atomically
        _Atomic(uint32_t)* new_refcount = nimcp_malloc(sizeof(_Atomic(uint32_t)));
        if (new_refcount) {
            // Initialize to 2 (original + this clone) BEFORE publishing
            __atomic_store_n(new_refcount, 2, __ATOMIC_RELAXED);

            // Atomically try to set original's refcount pointer
            // Only one thread will succeed in this CAS
            _Atomic(uint32_t)* null_ptr = NULL;
            if (__atomic_compare_exchange_n(&original->network_refcount_atomic,
                                            &null_ptr, new_refcount,
                                            false,  // strong CAS
                                            __ATOMIC_ACQ_REL,
                                            __ATOMIC_ACQUIRE)) {
                // CAS succeeded - we initialized the refcount
                // CRITICAL: Original no longer exclusively owns network - it's now shared
                original->owns_network = false;
            } else {
                // CAS failed - another thread beat us
                // Free our allocation and use theirs
                nimcp_free(new_refcount);
                // Increment the refcount that the other thread set up
                // BUG FIX: Use the pointer value, not address of pointer
                __atomic_fetch_add(original->network_refcount_atomic, 1, __ATOMIC_ACQ_REL);
            }
        }
    } else {
        // Additional clone - atomically increment existing refcount
        // expected is already _Atomic(uint32_t)*, use it directly
        __atomic_fetch_add(expected, 1, __ATOMIC_ACQ_REL);
    }

    // Clone shares the refcount with original (atomically load the final value)
    clone->network_refcount_atomic = __atomic_load_n(&original->network_refcount_atomic, __ATOMIC_ACQUIRE);
    clone->can_use_readonly = true;  // Can use read-only inference

    // Allocate output labels array (need space for max outputs, not just existing labels)
    clone->output_labels = nimcp_calloc(clone->config.num_outputs, sizeof(char*));
    if (clone->output_labels && original->output_labels) {
        // Copy existing labels from original
        for (uint32_t i = 0; i < original->num_output_labels; i++) {
            clone->output_labels[i] = nimcp_strdup(original->output_labels[i]);
        }
    }

    // Rebuild O(1) label lookup hash table for clone
    hash_table_config_t ht_config = {
        .initial_buckets = 256,
        .key_type = HASH_KEY_STRING,
        .hash_algorithm = HASH_ALG_FNV1A,
        .case_insensitive = false,
        .thread_safe = false
    };
    clone->label_index = hash_table_create(&ht_config);
    if (clone->label_index && clone->output_labels) {
        for (uint32_t i = 0; i < clone->num_output_labels; i++) {
            if (clone->output_labels[i]) {
                hash_table_insert_string(clone->label_index, clone->output_labels[i], &i, sizeof(uint32_t));
            }
        }
    }

    // Share strategy (read-only, safe to share)
    clone->strategy = original->strategy;

    // Copy stats from original (these are cached network stats)
    // The clone shares the network, so it should have the same structural stats
    clone->stats = original->stats;

    // Initialize private fields (cache, etc)
    clone->last_input = NULL;
    clone->cached_decision = NULL;
    clone->distributed = NULL;

    // Record COW statistics for tracking memory savings
    // Estimate shared network size based on neurons and synapses
    size_t network_size = (clone->stats.num_neurons * sizeof(void*) * 2) +  // Neuron structures
                         (clone->stats.num_synapses * sizeof(float) * 2);   // Synapse weights
    nimcp_cache_record_reference(network_size);

    return clone;
}

/**
 * @brief Mark brain as a snapshot with preserved stats
 *
 * WHAT: Sets snapshot flag and preserves current stats
 * WHY:  Snapshots should preserve stats at snapshot time, not reflect future changes
 * HOW:  Stores current stats in brain->snapshot_stats
 *
 * @param brain Brain to mark as snapshot
 * @param stats Stats to preserve
 */
void brain_mark_as_snapshot(brain_t brain, const brain_stats_t* stats)
{
    if (!brain || !stats) {
        return;
    }

    brain->is_snapshot = true;
    brain->snapshot_stats = *stats;
}

//=============================================================================
// Distributed Brain API Implementation
//=============================================================================

/**
 * WHAT: Create distributed brain with P2P coordination
 * WHY:  Enable multi-brain collaborative learning and shared chemical signals
 * HOW:  Create standard brain, then attach distributed cognition coordinator
 *
 * THREAD SAFETY: Thread-safe creation
 * PERFORMANCE: O(n) where n = num_neurons + network initialization
 */
brain_t brain_create_distributed(
    const char* task_name,
    brain_size_t size,
    brain_task_t task,
    uint32_t num_inputs,
    uint32_t num_outputs,
    p2p_node_t p2p_node
)
{
    // Guard: Validate P2P node
    if (!p2p_node) {
        set_error("NULL p2p_node provided to brain_create_distributed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create_distributed: p2p_node is NULL");
        return NULL;
    }

    // Create standard brain first
    brain_t brain = brain_create(task_name, size, task, num_inputs, num_outputs);
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;
    }

    // Enable distributed coordination
    if (!brain_enable_distributed(brain, p2p_node)) {
        brain_destroy(brain);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_create_distributed: brain_enable_distributed is NULL");
        return NULL;
    }

    brain_clear_error();
    return brain;
}

/**
 * WHAT: Enable distributed coordination on existing brain
 * WHY:  Allow conversion of standalone brain to distributed mode
 * HOW:  Creates distrib_cognition coordinator and starts sync threads
 *
 * THREAD SAFETY: Thread-safe if brain not actively processing
 */
bool brain_enable_distributed(brain_t brain, p2p_node_t p2p_node)
{
    // Guard: Validate parameters
    if (!brain) {
        set_error("NULL brain provided to brain_enable_distributed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_enable_distributed: brain is NULL");
        return false;
    }

    if (!p2p_node) {
        set_error("NULL p2p_node provided to brain_enable_distributed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_enable_distributed: p2p_node is NULL");
        return false;
    }

    // Guard: Check if already distributed
    if (brain->distributed) {
        set_error("Brain is already distributed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_enable_distributed: validation failed");
        return false;
    }

    // Create distributed cognition configuration
    distrib_cognition_config_t config;
    config.enable_neuromod_sync = true;
    config.neuromod_broadcast_interval_ms = 100;
    config.neuromod_diffusion_rate = 0.5F;
    config.enable_glial_sync = true;
    config.glial_sync_interval_ms = 100;
    config.enable_region_sync = true;
    config.region_sync_interval_ms = 100;
    config.sync_mode = SYNC_MODE_BIDIRECTIONAL;
    config.max_message_queue = 1000;

    // Create distributed cognition coordinator
    brain->distributed = distrib_cognition_create(&config, p2p_node);
    if (!brain->distributed) {
        set_error("Failed to create distributed cognition coordinator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_enable_distributed: brain->distributed is NULL");
        return false;
    }

    // Start distributed coordination
    if (!distrib_cognition_start(brain->distributed)) {
        set_error("Failed to start distributed cognition");
        distrib_cognition_destroy(brain->distributed);
        brain->distributed = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_enable_distributed: distrib_cognition_start is NULL");
        return false;
    }

    // Update brain config
    brain->config.enable_distributed = true;
    brain->config.p2p_node = p2p_node;

    brain_clear_error();
    return true;
}

/**
 * WHAT: Synchronize neuromodulators with peer brains
 * WHY:  Allow explicit control of sync timing for performance tuning
 * HOW:  Calls distrib_cognition_broadcast_neuromod for all neuromod types
 *
 * THREAD SAFETY: Thread-safe
 * PERFORMANCE: O(P × N) where P=peers, N=neuromod types
 */
bool brain_sync_neuromodulators(brain_t brain)
{
    // Guard: Validate brain
    if (!brain) {
        set_error("NULL brain provided to brain_sync_neuromodulators");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_sync_neuromodulators: brain is NULL");
        return false;
    }

    // Guard: Check if distributed
    if (!brain->distributed) {
        set_error("Brain is not distributed - cannot sync neuromodulators");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_sync_neuromodulators: brain->distributed is NULL");
        return false;
    }

    // Broadcast all neuromodulator types with default concentrations
    // In a full implementation, these would be read from the brain's neuromodulator pool
    bool success = true;
    success &= distrib_cognition_broadcast_neuromod(brain->distributed, NEUROMOD_DOPAMINE, 0.5F);
    success &= distrib_cognition_broadcast_neuromod(brain->distributed, NEUROMOD_SEROTONIN, 0.5F);
    success &= distrib_cognition_broadcast_neuromod(brain->distributed, NEUROMOD_NOREPINEPHRINE, 0.5F);
    success &= distrib_cognition_broadcast_neuromod(brain->distributed, NEUROMOD_ACETYLCHOLINE, 0.5F);

    if (!success) {
        set_error("Failed to broadcast some neuromodulators");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_sync_neuromodulators: success is NULL");
        return false;
    }

    brain_clear_error();
    return true;
}

/**
 * WHAT: Get distributed cognition statistics
 * WHY:  Monitor distributed brain performance and health
 * HOW:  Forwards query to underlying distrib_cognition coordinator
 */
bool brain_get_distributed_stats(
    brain_t brain,
    distrib_cognition_stats_t* stats
)
{
    // Guard: Validate parameters
    if (!brain) {
        set_error("NULL brain provided to brain_get_distributed_stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_distributed_stats: brain is NULL");
        return false;
    }

    if (!stats) {
        set_error("NULL stats provided to brain_get_distributed_stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_distributed_stats: stats is NULL");
        return false;
    }

    // Guard: Check if distributed
    if (!brain->distributed) {
        set_error("Brain is not distributed - no stats available");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_distributed_stats: brain->distributed is NULL");
        return false;
    }

    // Forward to distributed cognition
    if (!distrib_cognition_get_stats(brain->distributed, stats)) {
        set_error("Failed to get distributed cognition stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_get_distributed_stats: distrib_cognition_get_stats is NULL");
        return false;
    }

    brain_clear_error();
    return true;
}

/**
 * WHAT: Check if brain is distributed
 * WHY:  Allow callers to query brain mode before calling distributed APIs
 * HOW:  Return true if distributed coordinator exists
 */
bool brain_is_distributed(brain_t brain)
{
    if (!brain) {
        return false;
    }

    return brain->distributed != NULL;
}
