//=============================================================================
// nimcp_reconsolidation.c - Memory Reconsolidation System Implementation
//=============================================================================
/**
 * @file nimcp_reconsolidation.c
 * @brief Implementation of memory reconsolidation for Prime Resonant memory
 *
 * This file implements the reconsolidation system that manages memory lability
 * after retrieval, enabling memory updates and strengthening.
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_reconsolidation.h"
#include "cognitive/memory/nimcp_memory_kg_events.h"  /* W6: KG event emitters */
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdatomic.h>

// Platform abstraction for threading (use NIMCP thread layer)
#include "utils/thread/nimcp_thread.h"

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(reconsolidation, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Type Aliases
//=============================================================================

/** Alias for resonance config used in reconsolidation */
typedef resonance_config_t reconsolidation_resonance_config_t;

//=============================================================================
// Internal Constants
//=============================================================================

/** Magic number for system validation */
#define RECON_SYSTEM_MAGIC 0x5245434F  // "RECO"

/** Hash table size for window lookup (must be power of 2) */
#define WINDOW_HASH_TABLE_SIZE 256

/** Weight for signature component in update magnitude */
#define UPDATE_MAGNITUDE_SIG_WEIGHT 0.6f

/** Weight for quaternion component in update magnitude */
#define UPDATE_MAGNITUDE_QUAT_WEIGHT 0.4f

//=============================================================================
// Internal Type Definitions
//=============================================================================

/**
 * @brief Hash table entry for window lookup by memory ID
 */
typedef struct window_hash_entry {
    uint64_t memory_id;                    /**< Memory ID key */
    size_t window_index;                   /**< Index into windows array */
    struct window_hash_entry* next;        /**< Collision chain */
} window_hash_entry_t;

/**
 * @brief Internal reconsolidation system structure
 */
struct reconsolidation_system_struct {
    //-------------------------------------------------------------------------
    // Validation
    //-------------------------------------------------------------------------
    uint32_t magic;                        /**< Magic number for validation */

    //-------------------------------------------------------------------------
    // PR Memory Integration
    //-------------------------------------------------------------------------
    entangle_graph_t entanglement;         /**< Entanglement graph reference */
    pr_node_manager_t node_manager;        /**< Node manager reference */
    resonance_config_t resonance_config;   /**< Resonance config for interference */
    bool has_resonance_config;             /**< Whether resonance config was provided */

    //-------------------------------------------------------------------------
    // Active Reconsolidation Windows
    //-------------------------------------------------------------------------
    reconsolidation_window_t* windows;     /**< Array of windows */
    size_t num_windows;                    /**< Number of active windows */
    size_t max_windows;                    /**< Maximum capacity */
    size_t* free_indices;                  /**< Stack of free window indices */
    size_t free_count;                     /**< Number of free indices */

    //-------------------------------------------------------------------------
    // Window Lookup Hash Table
    //-------------------------------------------------------------------------
    window_hash_entry_t** hash_table;      /**< Hash table for memory ID lookup */
    window_hash_entry_t* entry_pool;       /**< Pool of hash entries */
    size_t entry_pool_size;                /**< Size of entry pool */
    size_t entries_used;                   /**< Number of entries in use */

    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------
    reconsolidation_config_t config;       /**< Current configuration */

    //-------------------------------------------------------------------------
    // Protein Synthesis Simulation
    //-------------------------------------------------------------------------
    bool protein_synthesis_blocked;        /**< PSI simulation flag */

    //-------------------------------------------------------------------------
    // Statistics
    //-------------------------------------------------------------------------
    _Atomic uint64_t total_retrievals;
    _Atomic uint64_t total_windows_created;
    _Atomic uint64_t total_updates;
    _Atomic uint64_t total_strengthenings;
    _Atomic uint64_t total_interference_blocks;
    _Atomic uint64_t total_synthesis_blocks;
    _Atomic uint64_t total_rollbacks;
    _Atomic uint64_t total_expired;
    _Atomic uint64_t peak_active_windows;
    double sum_lability_duration;          /**< For computing mean */
    double sum_update_magnitude;
    double sum_interference_strength;

    //-------------------------------------------------------------------------
    // Thread Synchronization
    //-------------------------------------------------------------------------
    nimcp_mutex_t* mutex;                  /**< System-wide mutex */
};

//=============================================================================
// Forward Declarations (Internal Functions)
//=============================================================================

static uint64_t hash_memory_id(uint64_t id);
static reconsolidation_window_t* find_window_by_memory(
    reconsolidation_system_t* system, pr_memory_node_t* memory);
static size_t allocate_window_index(reconsolidation_system_t* system);
static void free_window_index(reconsolidation_system_t* system, size_t index);
static bool add_window_to_hash(reconsolidation_system_t* system, uint64_t memory_id, size_t index);
static void remove_window_from_hash(reconsolidation_system_t* system, uint64_t memory_id);
static void init_window(reconsolidation_window_t* window, pr_memory_node_t* memory, float current_time, float lability_duration);
static void cleanup_window(reconsolidation_window_t* window);
static void apply_lability_decay(reconsolidation_window_t* window, float delta_time, float decay_rate);
static void transition_to_stable(reconsolidation_system_t* system, reconsolidation_window_t* window);
static void transition_to_restabilizing(reconsolidation_window_t* window, float current_time);
static void apply_update_to_memory(reconsolidation_window_t* window);
static void apply_strengthening_to_memory(reconsolidation_window_t* window, float boost);
static float compute_signature_similarity(const prime_signature_t* s1, const prime_signature_t* s2);
static float compute_quaternion_distance(nimcp_quaternion_t q1, nimcp_quaternion_t q2);

//=============================================================================
// Configuration Functions
//=============================================================================

NIMCP_EXPORT reconsolidation_config_t reconsolidation_config_default(void) {
    reconsolidation_config_t config = {
        .lability_duration = RECON_DEFAULT_LABILITY_DURATION,
        .lability_decay_rate = RECON_DEFAULT_DECAY_RATE,
        .update_threshold = RECON_DEFAULT_UPDATE_THRESHOLD,
        .interference_threshold = RECON_DEFAULT_INTERFERENCE_THRESHOLD,
        .strengthen_boost = RECON_DEFAULT_STRENGTHEN_BOOST,
        .max_windows = RECON_DEFAULT_MAX_WINDOWS,
        .max_interfering_per_window = RECON_MAX_INTERFERENCE_CHECK,
        .enable_interference_detection = true,
        .enable_auto_strengthen = true,
        .enable_auto_commit = false,
        .restabilization_time = 1.0f
    };
    return config;
}

NIMCP_EXPORT reconsolidation_config_t reconsolidation_config_test(void) {
    reconsolidation_config_t config = reconsolidation_config_default();
    config.lability_duration = RECON_TEST_LABILITY_DURATION;
    config.lability_decay_rate = 0.1f;  // Faster decay for testing
    config.restabilization_time = 0.1f;
    return config;
}

NIMCP_EXPORT bool reconsolidation_config_validate(const reconsolidation_config_t* config) {
    if (config == NULL) {
        return false;
    }

    if (config->lability_duration <= 0.0f) {
        return false;
    }

    if (config->lability_decay_rate < 0.0f) {
        return false;
    }

    if (config->update_threshold < 0.0f || config->update_threshold > 1.0f) {
        return false;
    }

    if (config->interference_threshold < 0.0f || config->interference_threshold > 1.0f) {
        return false;
    }

    if (config->max_windows == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reconsolidation_config_validate: config->max_windows is zero");
        return false;
    }

    return true;
}

//=============================================================================
// System Lifecycle Functions
//=============================================================================

NIMCP_EXPORT reconsolidation_system_t* reconsolidation_create(
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    const reconsolidation_resonance_config_t* resonance_config,
    const reconsolidation_config_t* config)
{
    // Validate required parameters
    if (entanglement == NULL || node_manager == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reconsolidation_create: validation failed");
        return NULL;
    }

    // Use default config if not provided
    reconsolidation_config_t cfg;
    if (config != NULL) {
        if (!reconsolidation_config_validate(config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reconsolidation_create: reconsolidation_config_validate is NULL");
            return NULL;
        }
        cfg = *config;
    } else {
        cfg = reconsolidation_config_default();
    }

    // Allocate system structure
    reconsolidation_system_t* system = (reconsolidation_system_t*)nimcp_calloc(
        1, sizeof(reconsolidation_system_t));
    if (system == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate system");

        return NULL;
    }

    system->magic = RECON_SYSTEM_MAGIC;
    system->entanglement = entanglement;
    system->node_manager = node_manager;

    // Store resonance config if provided
    if (resonance_config != NULL) {
        system->resonance_config = *resonance_config;
        system->has_resonance_config = true;
    } else {
        system->resonance_config = resonance_config_default();
        system->has_resonance_config = false;
    }

    system->config = cfg;
    system->max_windows = cfg.max_windows;

    // Allocate windows array
    system->windows = (reconsolidation_window_t*)nimcp_calloc(
        cfg.max_windows, sizeof(reconsolidation_window_t));
    if (system->windows == NULL) {
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reconsolidation_create: validation failed");
        return NULL;
    }

    // Allocate free indices stack
    system->free_indices = (size_t*)nimcp_calloc(cfg.max_windows, sizeof(size_t));
    if (system->free_indices == NULL) {
        nimcp_free(system->windows);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reconsolidation_create: validation failed");
        return NULL;
    }

    // Initialize free indices (all slots available)
    for (size_t i = 0; i < cfg.max_windows; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cfg.max_windows > 256) {
            reconsolidation_heartbeat("reconsolidat_loop",
                             (float)(i + 1) / (float)cfg.max_windows);
        }

        system->free_indices[i] = cfg.max_windows - 1 - i;
    }
    system->free_count = cfg.max_windows;

    // Allocate hash table
    system->hash_table = (window_hash_entry_t**)nimcp_calloc(
        WINDOW_HASH_TABLE_SIZE, sizeof(window_hash_entry_t*));
    if (system->hash_table == NULL) {
        nimcp_free(system->free_indices);
        nimcp_free(system->windows);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reconsolidation_create: validation failed");
        return NULL;
    }

    // Allocate entry pool for hash table
    system->entry_pool_size = cfg.max_windows;
    system->entry_pool = (window_hash_entry_t*)nimcp_calloc(
        system->entry_pool_size, sizeof(window_hash_entry_t));
    if (system->entry_pool == NULL) {
        nimcp_free(system->hash_table);
        nimcp_free(system->free_indices);
        nimcp_free(system->windows);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reconsolidation_create: validation failed");
        return NULL;
    }
    system->entries_used = 0;

    // Allocate interference arrays for each window
    for (size_t i = 0; i < cfg.max_windows; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cfg.max_windows > 256) {
            reconsolidation_heartbeat("reconsolidat_loop",
                             (float)(i + 1) / (float)cfg.max_windows);
        }

        system->windows[i].max_interfering = cfg.max_interfering_per_window;
        if (cfg.max_interfering_per_window > 0) {
            system->windows[i].interfering_memories = (pr_memory_node_t**)nimcp_calloc(
                cfg.max_interfering_per_window, sizeof(pr_memory_node_t*));
            // Allocation failure is not fatal - just disables interference tracking
        }
    }

    // Create mutex
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_RECURSIVE;
    system->mutex = nimcp_mutex_create(&attr);
    if (system->mutex == NULL) {
        // Cleanup on mutex creation failure
        for (size_t i = 0; i < cfg.max_windows; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && cfg.max_windows > 256) {
                reconsolidation_heartbeat("reconsolidat_loop",
                                 (float)(i + 1) / (float)cfg.max_windows);
            }

            if (system->windows[i].interfering_memories != NULL) {
                nimcp_free(system->windows[i].interfering_memories);
            }
        }
        nimcp_free(system->entry_pool);
        nimcp_free(system->hash_table);
        nimcp_free(system->free_indices);
        nimcp_free(system->windows);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reconsolidation_create: validation failed");
        return NULL;
    }

    // Initialize atomic statistics
    atomic_store(&system->total_retrievals, 0);
    atomic_store(&system->total_windows_created, 0);
    atomic_store(&system->total_updates, 0);
    atomic_store(&system->total_strengthenings, 0);
    atomic_store(&system->total_interference_blocks, 0);
    atomic_store(&system->total_synthesis_blocks, 0);
    atomic_store(&system->total_rollbacks, 0);
    atomic_store(&system->total_expired, 0);
    atomic_store(&system->peak_active_windows, 0);

    return system;
}

NIMCP_EXPORT void reconsolidation_destroy(reconsolidation_system_t* system) {
    if (system == NULL) {
        return;
    }

    // Validate magic
    if (system->magic != RECON_SYSTEM_MAGIC) {
        return;
    }

    // Clear magic to prevent double-free
    system->magic = 0;

    // Lock for cleanup
    nimcp_mutex_lock(system->mutex);

    // Cleanup all active windows
    for (size_t i = 0; i < system->max_windows; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->max_windows > 256) {
            reconsolidation_heartbeat("reconsolidat_loop",
                             (float)(i + 1) / (float)system->max_windows);
        }

        if (system->windows[i].memory != NULL) {
            cleanup_window(&system->windows[i]);
        }
        if (system->windows[i].interfering_memories != NULL) {
            nimcp_free(system->windows[i].interfering_memories);
            system->windows[i].interfering_memories = NULL;
        }
    }

    nimcp_mutex_unlock(system->mutex);

    // Free resources
    nimcp_mutex_destroy(system->mutex);
    nimcp_free(system->entry_pool);
    nimcp_free(system->hash_table);
    nimcp_free(system->free_indices);
    nimcp_free(system->windows);
    nimcp_free(system);
    system = NULL;
}

//=============================================================================
// Core Reconsolidation Functions
//=============================================================================

NIMCP_EXPORT reconsolidation_error_t reconsolidation_on_retrieval(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory,
    float activation_strength,
    float current_time)
{
    if (system == NULL || memory == NULL) {
        return RECON_ERROR_NULL_POINTER;
    }

    if (system->magic != RECON_SYSTEM_MAGIC) {
        return RECON_ERROR_INVALID_STATE;
    }

    // Clamp activation
    if (activation_strength < 0.0f) activation_strength = 0.0f;
    if (activation_strength > 1.0f) activation_strength = 1.0f;

    nimcp_mutex_lock(system->mutex);

    // Update statistics
    atomic_fetch_add(&system->total_retrievals, 1);

    // Check if memory already has an active window
    reconsolidation_window_t* window = find_window_by_memory(system, memory);

    if (window != NULL) {
        // Memory is already in reconsolidation - refresh/accumulate
        window->retrieval_count++;
        window->cumulative_activation += activation_strength;

        // Reset lability if still in LABILE state
        if (window->state == RECON_LABILE) {
            window->lability_remaining = system->config.lability_duration;
            window->lability_strength = 1.0f;
            window->retrieval_time = current_time;
        }

        nimcp_mutex_unlock(system->mutex);
        return RECON_SUCCESS;
    }

    // Create new window
    size_t index = allocate_window_index(system);
    if (index == (size_t)-1) {
        nimcp_mutex_unlock(system->mutex);
        return RECON_ERROR_CAPACITY;
    }

    window = &system->windows[index];
    init_window(window, memory, current_time, system->config.lability_duration);
    window->cumulative_activation = activation_strength;

    // Copy original state for potential rollback
    const prime_signature_t* sig = pr_memory_node_get_signature(memory);
    if (sig != NULL) {
        memcpy(&window->original_signature, sig, sizeof(prime_signature_t));
    }
    window->original_quaternion = pr_memory_node_get_state(memory);
    window->original_strength = memory->current_strength;

    // Add to hash table
    uint64_t mem_id = pr_memory_node_get_id(memory);
    if (!add_window_to_hash(system, mem_id, index)) {
        free_window_index(system, index);
        nimcp_mutex_unlock(system->mutex);
        return RECON_ERROR_NO_MEMORY;
    }

    system->num_windows++;
    atomic_fetch_add(&system->total_windows_created, 1);

    // Update peak
    uint64_t current = system->num_windows;
    uint64_t peak = atomic_load(&system->peak_active_windows);
    while (current > peak && !atomic_compare_exchange_weak(
        &system->peak_active_windows, &peak, current)) {
        // Retry
    }

    nimcp_mutex_unlock(system->mutex);

    /* W6: emit KG event for reconsolidation window open */
    memory_kg_emit_reconsolidation_opened(
        memory_kg_events_get_registered_brain(),
        mem_id, activation_strength);

    return RECON_SUCCESS;
}

NIMCP_EXPORT size_t reconsolidation_update(
    reconsolidation_system_t* system,
    float current_time,
    float delta_time)
{
    if (system == NULL || delta_time <= 0.0f) {
        return 0;
    }

    if (system->magic != RECON_SYSTEM_MAGIC) {
        return 0;
    }

    nimcp_mutex_lock(system->mutex);

    size_t processed = 0;
    size_t i = 0;

    while (i < system->max_windows) {
        reconsolidation_window_t* window = &system->windows[i];

        if (window->memory == NULL) {
            i++;
            continue;
        }

        processed++;

        switch (window->state) {
            case RECON_LABILE:
                // Apply lability decay
                apply_lability_decay(window, delta_time, system->config.lability_decay_rate);

                // Check if lability window has expired
                window->lability_remaining -= delta_time;
                if (window->lability_remaining <= 0.0f ||
                    window->lability_strength < RECON_LABILITY_EPSILON) {

                    // Window expired - decide outcome
                    if (window->has_proposed_update && system->config.enable_auto_commit) {
                        // Auto-commit the update
                        if (system->protein_synthesis_blocked) {
                            window->outcome = RECON_OUTCOME_BLOCKED;
                            atomic_fetch_add(&system->total_synthesis_blocks, 1);
                        } else {
                            apply_update_to_memory(window);
                            window->outcome = RECON_OUTCOME_UPDATED;
                            atomic_fetch_add(&system->total_updates, 1);
                        }
                        transition_to_restabilizing(window, current_time);
                    } else if (system->config.enable_auto_strengthen) {
                        // Auto-strengthen
                        if (!system->protein_synthesis_blocked) {
                            apply_strengthening_to_memory(window, system->config.strengthen_boost);
                            window->outcome = RECON_OUTCOME_STRENGTHENED;
                            atomic_fetch_add(&system->total_strengthenings, 1);
                        } else {
                            window->outcome = RECON_OUTCOME_BLOCKED;
                            atomic_fetch_add(&system->total_synthesis_blocks, 1);
                        }
                        transition_to_restabilizing(window, current_time);
                    } else {
                        // Just expire
                        window->outcome = RECON_OUTCOME_EXPIRED;
                        atomic_fetch_add(&system->total_expired, 1);
                        transition_to_stable(system, window);
                    }
                }
                break;

            case RECON_UPDATING:
                // Updates are applied immediately, so just transition
                transition_to_restabilizing(window, current_time);
                break;

            case RECON_RESTABILIZING:
                // Check if restabilization is complete
                if (current_time - window->state_enter_time >= system->config.restabilization_time) {
                    // Record lability duration for statistics
                    float duration = current_time - window->retrieval_time;
                    system->sum_lability_duration += duration;
                    transition_to_stable(system, window);
                }
                break;

            case RECON_STABLE:
                // Should not have windows in stable state
                // Clean up any orphaned windows
                transition_to_stable(system, window);
                break;
        }

        i++;
    }

    nimcp_mutex_unlock(system->mutex);
    return processed;
}

NIMCP_EXPORT reconsolidation_error_t reconsolidation_propose_update(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory,
    const prime_signature_t* new_signature,
    const nimcp_quaternion_t* new_quaternion)
{
    if (system == NULL || memory == NULL) {
        return RECON_ERROR_NULL_POINTER;
    }

    if (new_signature == NULL && new_quaternion == NULL) {
        return RECON_ERROR_NULL_POINTER;  // Must provide at least one
    }

    if (system->magic != RECON_SYSTEM_MAGIC) {
        return RECON_ERROR_INVALID_STATE;
    }

    nimcp_mutex_lock(system->mutex);

    reconsolidation_window_t* window = find_window_by_memory(system, memory);
    if (window == NULL) {
        nimcp_mutex_unlock(system->mutex);
        return RECON_ERROR_NOT_FOUND;
    }

    if (window->state != RECON_LABILE) {
        nimcp_mutex_unlock(system->mutex);
        return RECON_ERROR_NOT_LABILE;
    }

    // Store proposed updates
    if (new_signature != NULL) {
        memcpy(&window->proposed_signature, new_signature, sizeof(prime_signature_t));
    } else {
        memcpy(&window->proposed_signature, &window->original_signature, sizeof(prime_signature_t));
    }

    if (new_quaternion != NULL) {
        window->proposed_quaternion = *new_quaternion;
    } else {
        window->proposed_quaternion = window->original_quaternion;
    }

    window->has_proposed_update = true;

    // Compute update magnitude
    float sig_diff = 0.0f;
    if (new_signature != NULL) {
        sig_diff = 1.0f - compute_signature_similarity(
            &window->original_signature, new_signature);
    }

    float quat_diff = 0.0f;
    if (new_quaternion != NULL) {
        quat_diff = compute_quaternion_distance(
            window->original_quaternion, *new_quaternion) / (float)M_PI;
    }

    window->update_magnitude = UPDATE_MAGNITUDE_SIG_WEIGHT * sig_diff +
                               UPDATE_MAGNITUDE_QUAT_WEIGHT * quat_diff;

    nimcp_mutex_unlock(system->mutex);
    return RECON_SUCCESS;
}

NIMCP_EXPORT reconsolidation_error_t reconsolidation_commit_update(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory,
    float current_time)
{
    if (system == NULL || memory == NULL) {
        return RECON_ERROR_NULL_POINTER;
    }

    if (system->magic != RECON_SYSTEM_MAGIC) {
        return RECON_ERROR_INVALID_STATE;
    }

    nimcp_mutex_lock(system->mutex);

    reconsolidation_window_t* window = find_window_by_memory(system, memory);
    if (window == NULL) {
        nimcp_mutex_unlock(system->mutex);
        return RECON_ERROR_NOT_FOUND;
    }

    if (window->state != RECON_LABILE) {
        nimcp_mutex_unlock(system->mutex);
        return RECON_ERROR_NOT_LABILE;
    }

    if (!window->has_proposed_update) {
        nimcp_mutex_unlock(system->mutex);
        return RECON_ERROR_NO_PROPOSED_UPDATE;
    }

    // Check for protein synthesis blockade
    if (system->protein_synthesis_blocked) {
        window->outcome = RECON_OUTCOME_BLOCKED;
        atomic_fetch_add(&system->total_synthesis_blocks, 1);
        transition_to_restabilizing(window, current_time);
        nimcp_mutex_unlock(system->mutex);
        return RECON_ERROR_BLOCKED;
    }

    // Check for interference
    if (system->config.enable_interference_detection &&
        window->interference_strength >= system->config.interference_threshold) {
        window->outcome = RECON_OUTCOME_INTERFERENCE;
        atomic_fetch_add(&system->total_interference_blocks, 1);
        transition_to_restabilizing(window, current_time);
        nimcp_mutex_unlock(system->mutex);
        return RECON_ERROR_INTERFERENCE;
    }

    // Decide: update vs strengthen based on magnitude
    if (window->update_magnitude < system->config.update_threshold) {
        // Small change - strengthen instead
        apply_strengthening_to_memory(window, system->config.strengthen_boost);
        window->outcome = RECON_OUTCOME_STRENGTHENED;
        atomic_fetch_add(&system->total_strengthenings, 1);
        system->sum_update_magnitude += window->update_magnitude;
    } else {
        // Apply the update
        window->state = RECON_UPDATING;
        apply_update_to_memory(window);
        window->outcome = RECON_OUTCOME_UPDATED;
        atomic_fetch_add(&system->total_updates, 1);
        system->sum_update_magnitude += window->update_magnitude;
    }

    transition_to_restabilizing(window, current_time);

    int emitted_outcome = (int)window->outcome;
    uint64_t emitted_mem_id = pr_memory_node_get_id(memory);

    nimcp_mutex_unlock(system->mutex);

    /* W6: emit KG event for reconsolidation commit */
    memory_kg_emit_reconsolidation_committed(
        memory_kg_events_get_registered_brain(),
        emitted_mem_id, emitted_outcome);

    return RECON_SUCCESS;
}

NIMCP_EXPORT reconsolidation_error_t reconsolidation_rollback(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory)
{
    if (system == NULL || memory == NULL) {
        return RECON_ERROR_NULL_POINTER;
    }

    if (system->magic != RECON_SYSTEM_MAGIC) {
        return RECON_ERROR_INVALID_STATE;
    }

    nimcp_mutex_lock(system->mutex);

    reconsolidation_window_t* window = find_window_by_memory(system, memory);
    if (window == NULL) {
        nimcp_mutex_unlock(system->mutex);
        return RECON_ERROR_NOT_FOUND;
    }

    if (window->state == RECON_STABLE) {
        nimcp_mutex_unlock(system->mutex);
        return RECON_ERROR_INVALID_STATE;
    }

    // Restore original state
    pr_memory_node_set_signature(memory, &window->original_signature);
    pr_memory_node_update_state(memory, window->original_quaternion);
    memory->current_strength = window->original_strength;

    window->outcome = RECON_OUTCOME_ROLLBACK;
    atomic_fetch_add(&system->total_rollbacks, 1);

    transition_to_stable(system, window);

    nimcp_mutex_unlock(system->mutex);
    return RECON_SUCCESS;
}

NIMCP_EXPORT reconsolidation_error_t reconsolidation_strengthen(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory,
    float boost_amount)
{
    if (system == NULL || memory == NULL) {
        return RECON_ERROR_NULL_POINTER;
    }

    if (system->magic != RECON_SYSTEM_MAGIC) {
        return RECON_ERROR_INVALID_STATE;
    }

    nimcp_mutex_lock(system->mutex);

    reconsolidation_window_t* window = find_window_by_memory(system, memory);
    if (window == NULL) {
        nimcp_mutex_unlock(system->mutex);
        return RECON_ERROR_NOT_FOUND;
    }

    if (window->state != RECON_LABILE) {
        nimcp_mutex_unlock(system->mutex);
        return RECON_ERROR_NOT_LABILE;
    }

    // Check for protein synthesis blockade
    if (system->protein_synthesis_blocked) {
        window->outcome = RECON_OUTCOME_BLOCKED;
        atomic_fetch_add(&system->total_synthesis_blocks, 1);
        transition_to_restabilizing(window, pr_node_current_time_ms() / 1000.0f);
        nimcp_mutex_unlock(system->mutex);
        return RECON_ERROR_BLOCKED;
    }

    float total_boost = system->config.strengthen_boost + boost_amount;
    apply_strengthening_to_memory(window, total_boost);
    window->outcome = RECON_OUTCOME_STRENGTHENED;
    atomic_fetch_add(&system->total_strengthenings, 1);

    transition_to_restabilizing(window, pr_node_current_time_ms() / 1000.0f);

    nimcp_mutex_unlock(system->mutex);
    return RECON_SUCCESS;
}

//=============================================================================
// Interference Functions
//=============================================================================

NIMCP_EXPORT bool reconsolidation_check_interference(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory,
    pr_memory_node_t* new_memory,
    float* interference_out)
{
    if (system == NULL || memory == NULL || new_memory == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "reconsolidation_check_interference: validation failed");
        return false;
    }

    if (interference_out != NULL) {
        *interference_out = 0.0f;
    }

    // Compute resonance between memories
    const prime_signature_t* sig1 = pr_memory_node_get_signature(memory);
    const prime_signature_t* sig2 = pr_memory_node_get_signature(new_memory);

    if (sig1 == NULL || sig2 == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reconsolidation_check_interference: validation failed");
        return false;
    }

    // Compute similarity
    float jaccard = compute_signature_similarity(sig1, sig2);

    // Also consider quaternion similarity
    nimcp_quaternion_t q1 = pr_memory_node_get_state(memory);
    nimcp_quaternion_t q2 = pr_memory_node_get_state(new_memory);
    float quat_sim = 1.0f - (compute_quaternion_distance(q1, q2) / (float)M_PI);
    if (quat_sim < 0.0f) quat_sim = 0.0f;

    // Combined resonance
    float resonance = 0.6f * jaccard + 0.4f * quat_sim;

    // Interference = high resonance but different content
    // High resonance means they activate similar neural circuits
    // Different signature means they have different content
    float sig_diff = 1.0f - jaccard;

    // Interference is strong when resonance is high but signatures differ
    float interference = resonance * sig_diff;

    if (interference_out != NULL) {
        *interference_out = interference;
    }

    return interference >= system->config.interference_threshold;
}

NIMCP_EXPORT size_t reconsolidation_register_encoding(
    reconsolidation_system_t* system,
    pr_memory_node_t* new_memory,
    float current_time)
{
    (void)current_time;  // May be used in future for temporal tracking

    if (system == NULL || new_memory == NULL) {
        return 0;
    }

    if (!system->config.enable_interference_detection) {
        return 0;
    }

    if (system->magic != RECON_SYSTEM_MAGIC) {
        return 0;
    }

    nimcp_mutex_lock(system->mutex);

    size_t affected = 0;

    // Check against all labile memories
    for (size_t i = 0; i < system->max_windows; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->max_windows > 256) {
            reconsolidation_heartbeat("reconsolidat_loop",
                             (float)(i + 1) / (float)system->max_windows);
        }

        reconsolidation_window_t* window = &system->windows[i];

        if (window->memory == NULL || window->state != RECON_LABILE) {
            continue;
        }

        float interference = 0.0f;
        if (reconsolidation_check_interference(system, window->memory, new_memory, &interference)) {
            // Track interfering memory
            if (window->interfering_memories != NULL &&
                window->num_interfering < window->max_interfering) {
                window->interfering_memories[window->num_interfering] = new_memory;
                window->num_interfering++;
            }

            // Update interference strength (max of all)
            if (interference > window->interference_strength) {
                window->interference_strength = interference;
            }

            affected++;
        }
    }

    nimcp_mutex_unlock(system->mutex);
    return affected;
}

//=============================================================================
// Query Functions
//=============================================================================

NIMCP_EXPORT bool reconsolidation_is_labile(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory)
{
    if (system == NULL || memory == NULL) {
        return false;
    }

    if (system->magic != RECON_SYSTEM_MAGIC) {
        return false;
    }

    nimcp_mutex_lock(system->mutex);
    reconsolidation_window_t* window = find_window_by_memory(system, memory);
    bool result = (window != NULL && window->state != RECON_STABLE);
    nimcp_mutex_unlock(system->mutex);

    return result;
}

NIMCP_EXPORT float reconsolidation_get_lability_strength(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory)
{
    if (system == NULL || memory == NULL) {
        return 0.0f;
    }

    if (system->magic != RECON_SYSTEM_MAGIC) {
        return 0.0f;
    }

    nimcp_mutex_lock(system->mutex);
    reconsolidation_window_t* window = find_window_by_memory(system, memory);
    float result = (window != NULL) ? window->lability_strength : 0.0f;
    nimcp_mutex_unlock(system->mutex);

    return result;
}

NIMCP_EXPORT reconsolidation_state_t reconsolidation_get_state(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory)
{
    if (system == NULL || memory == NULL) {
        return RECON_STABLE;
    }

    if (system->magic != RECON_SYSTEM_MAGIC) {
        return RECON_STABLE;
    }

    nimcp_mutex_lock(system->mutex);
    reconsolidation_window_t* window = find_window_by_memory(system, memory);
    reconsolidation_state_t result = (window != NULL) ? window->state : RECON_STABLE;
    nimcp_mutex_unlock(system->mutex);

    return result;
}

NIMCP_EXPORT reconsolidation_error_t reconsolidation_get_labile_memories(
    reconsolidation_system_t* system,
    labile_memory_info_t* out_info,
    size_t max_results,
    size_t* count_out)
{
    if (system == NULL || out_info == NULL || count_out == NULL) {
        return RECON_ERROR_NULL_POINTER;
    }

    if (system->magic != RECON_SYSTEM_MAGIC) {
        return RECON_ERROR_INVALID_STATE;
    }

    nimcp_mutex_lock(system->mutex);

    size_t count = 0;
    for (size_t i = 0; i < system->max_windows && count < max_results; i++) {
        reconsolidation_window_t* window = &system->windows[i];

        if (window->memory == NULL) {
            continue;
        }

        out_info[count].memory_id = window->memory_id;
        out_info[count].state = window->state;
        out_info[count].lability_remaining = window->lability_remaining;
        out_info[count].lability_strength = window->lability_strength;
        out_info[count].has_proposed_update = window->has_proposed_update;
        out_info[count].interference_strength = window->interference_strength;
        count++;
    }

    *count_out = count;

    nimcp_mutex_unlock(system->mutex);
    return RECON_SUCCESS;
}

NIMCP_EXPORT float reconsolidation_compute_update_magnitude(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory,
    const prime_signature_t* new_signature,
    const nimcp_quaternion_t* new_quaternion)
{
    if (system == NULL || memory == NULL) {
        return -1.0f;
    }

    if (new_signature == NULL && new_quaternion == NULL) {
        return 0.0f;  // No change
    }

    const prime_signature_t* current_sig = pr_memory_node_get_signature(memory);
    nimcp_quaternion_t current_quat = pr_memory_node_get_state(memory);

    float sig_diff = 0.0f;
    if (new_signature != NULL && current_sig != NULL) {
        sig_diff = 1.0f - compute_signature_similarity(current_sig, new_signature);
    }

    float quat_diff = 0.0f;
    if (new_quaternion != NULL) {
        quat_diff = compute_quaternion_distance(current_quat, *new_quaternion) / (float)M_PI;
    }

    return UPDATE_MAGNITUDE_SIG_WEIGHT * sig_diff +
           UPDATE_MAGNITUDE_QUAT_WEIGHT * quat_diff;
}

//=============================================================================
// Protein Synthesis Simulation Functions
//=============================================================================

NIMCP_EXPORT void reconsolidation_block_synthesis(
    reconsolidation_system_t* system,
    bool blocked)
{
    if (system == NULL) {
        return;
    }

    nimcp_mutex_lock(system->mutex);
    system->protein_synthesis_blocked = blocked;
    nimcp_mutex_unlock(system->mutex);
}

NIMCP_EXPORT bool reconsolidation_is_synthesis_blocked(
    reconsolidation_system_t* system)
{
    if (system == NULL) {
        return false;
    }

    nimcp_mutex_lock(system->mutex);
    bool result = system->protein_synthesis_blocked;
    nimcp_mutex_unlock(system->mutex);

    return result;
}

//=============================================================================
// Window Management Functions
//=============================================================================

NIMCP_EXPORT reconsolidation_error_t reconsolidation_force_close(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory,
    bool restore_original)
{
    if (system == NULL || memory == NULL) {
        return RECON_ERROR_NULL_POINTER;
    }

    if (system->magic != RECON_SYSTEM_MAGIC) {
        return RECON_ERROR_INVALID_STATE;
    }

    nimcp_mutex_lock(system->mutex);

    reconsolidation_window_t* window = find_window_by_memory(system, memory);
    if (window == NULL) {
        nimcp_mutex_unlock(system->mutex);
        return RECON_ERROR_NOT_FOUND;
    }

    if (restore_original) {
        pr_memory_node_set_signature(memory, &window->original_signature);
        pr_memory_node_update_state(memory, window->original_quaternion);
        memory->current_strength = window->original_strength;
    }

    window->outcome = RECON_OUTCOME_EXPIRED;
    transition_to_stable(system, window);

    nimcp_mutex_unlock(system->mutex);
    return RECON_SUCCESS;
}

NIMCP_EXPORT size_t reconsolidation_force_close_all(
    reconsolidation_system_t* system,
    bool restore_original)
{
    if (system == NULL) {
        return 0;
    }

    if (system->magic != RECON_SYSTEM_MAGIC) {
        return 0;
    }

    nimcp_mutex_lock(system->mutex);

    size_t closed = 0;
    for (size_t i = 0; i < system->max_windows; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->max_windows > 256) {
            reconsolidation_heartbeat("reconsolidat_loop",
                             (float)(i + 1) / (float)system->max_windows);
        }

        reconsolidation_window_t* window = &system->windows[i];

        if (window->memory == NULL) {
            continue;
        }

        if (restore_original) {
            pr_memory_node_set_signature(window->memory, &window->original_signature);
            pr_memory_node_update_state(window->memory, window->original_quaternion);
            window->memory->current_strength = window->original_strength;
        }

        window->outcome = RECON_OUTCOME_EXPIRED;
        transition_to_stable(system, window);
        closed++;
    }

    nimcp_mutex_unlock(system->mutex);
    return closed;
}

NIMCP_EXPORT size_t reconsolidation_get_active_window_count(
    reconsolidation_system_t* system)
{
    if (system == NULL) {
        return 0;
    }

    nimcp_mutex_lock(system->mutex);
    size_t count = system->num_windows;
    nimcp_mutex_unlock(system->mutex);

    return count;
}

//=============================================================================
// Statistics Functions
//=============================================================================

NIMCP_EXPORT reconsolidation_error_t reconsolidation_get_stats(
    reconsolidation_system_t* system,
    reconsolidation_stats_t* stats)
{
    if (system == NULL || stats == NULL) {
        return RECON_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(system->mutex);

    stats->total_retrievals = atomic_load(&system->total_retrievals);
    stats->total_windows_created = atomic_load(&system->total_windows_created);
    stats->total_updates = atomic_load(&system->total_updates);
    stats->total_strengthenings = atomic_load(&system->total_strengthenings);
    stats->total_interference_blocks = atomic_load(&system->total_interference_blocks);
    stats->total_synthesis_blocks = atomic_load(&system->total_synthesis_blocks);
    stats->total_rollbacks = atomic_load(&system->total_rollbacks);
    stats->total_expired = atomic_load(&system->total_expired);
    stats->current_active_windows = system->num_windows;
    stats->peak_active_windows = atomic_load(&system->peak_active_windows);

    // Compute means
    uint64_t completed = stats->total_updates + stats->total_strengthenings +
                         stats->total_rollbacks + stats->total_expired +
                         stats->total_interference_blocks + stats->total_synthesis_blocks;
    if (completed > 0) {
        stats->mean_lability_duration = (float)(system->sum_lability_duration / (double)completed);
    } else {
        stats->mean_lability_duration = 0.0f;
    }

    uint64_t updated = stats->total_updates + stats->total_strengthenings;
    if (updated > 0) {
        stats->mean_update_magnitude = (float)(system->sum_update_magnitude / (double)updated);
    } else {
        stats->mean_update_magnitude = 0.0f;
    }

    uint64_t interfered = stats->total_interference_blocks;
    if (interfered > 0) {
        stats->mean_interference_strength = (float)(system->sum_interference_strength / (double)interfered);
    } else {
        stats->mean_interference_strength = 0.0f;
    }

    nimcp_mutex_unlock(system->mutex);
    return RECON_SUCCESS;
}

NIMCP_EXPORT void reconsolidation_reset_stats(reconsolidation_system_t* system) {
    if (system == NULL) {
        return;
    }

    nimcp_mutex_lock(system->mutex);

    atomic_store(&system->total_retrievals, 0);
    atomic_store(&system->total_windows_created, 0);
    atomic_store(&system->total_updates, 0);
    atomic_store(&system->total_strengthenings, 0);
    atomic_store(&system->total_interference_blocks, 0);
    atomic_store(&system->total_synthesis_blocks, 0);
    atomic_store(&system->total_rollbacks, 0);
    atomic_store(&system->total_expired, 0);
    atomic_store(&system->peak_active_windows, system->num_windows);
    system->sum_lability_duration = 0.0;
    system->sum_update_magnitude = 0.0;
    system->sum_interference_strength = 0.0;

    nimcp_mutex_unlock(system->mutex);
}

//=============================================================================
// Configuration Update Functions
//=============================================================================

NIMCP_EXPORT reconsolidation_error_t reconsolidation_set_config(
    reconsolidation_system_t* system,
    const reconsolidation_config_t* config)
{
    if (system == NULL || config == NULL) {
        return RECON_ERROR_NULL_POINTER;
    }

    if (!reconsolidation_config_validate(config)) {
        return RECON_ERROR_INVALID_CONFIG;
    }

    nimcp_mutex_lock(system->mutex);

    // Copy config but preserve max_windows (cannot be changed)
    size_t saved_max_windows = system->config.max_windows;
    system->config = *config;
    system->config.max_windows = saved_max_windows;

    nimcp_mutex_unlock(system->mutex);
    return RECON_SUCCESS;
}

NIMCP_EXPORT reconsolidation_error_t reconsolidation_get_config(
    reconsolidation_system_t* system,
    reconsolidation_config_t* config)
{
    if (system == NULL || config == NULL) {
        return RECON_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(system->mutex);
    *config = system->config;
    nimcp_mutex_unlock(system->mutex);

    return RECON_SUCCESS;
}

//=============================================================================
// Utility Functions
//=============================================================================

NIMCP_EXPORT const char* reconsolidation_error_string(reconsolidation_error_t error) {
    switch (error) {
        case RECON_SUCCESS:              return "Success";
        case RECON_ERROR_NULL_POINTER:   return "Null pointer";
        case RECON_ERROR_NOT_FOUND:      return "Memory not found";
        case RECON_ERROR_NOT_LABILE:     return "Memory is not labile";
        case RECON_ERROR_ALREADY_UPDATING: return "Memory is already being updated";
        case RECON_ERROR_NO_PROPOSED_UPDATE: return "No update has been proposed";
        case RECON_ERROR_INTERFERENCE:   return "Interference blocked operation";
        case RECON_ERROR_BLOCKED:        return "Protein synthesis blocked";
        case RECON_ERROR_CAPACITY:       return "Maximum windows reached";
        case RECON_ERROR_NO_MEMORY:      return "Memory allocation failed";
        case RECON_ERROR_INVALID_STATE:  return "Invalid state for operation";
        case RECON_ERROR_INVALID_CONFIG: return "Invalid configuration";
        default:                         return "Unknown error";
    }
}

NIMCP_EXPORT const char* reconsolidation_state_name(reconsolidation_state_t state) {
    switch (state) {
        case RECON_STABLE:        return "STABLE";
        case RECON_LABILE:        return "LABILE";
        case RECON_UPDATING:      return "UPDATING";
        case RECON_RESTABILIZING: return "RESTABILIZING";
        default:                  return "UNKNOWN";
    }
}

NIMCP_EXPORT const char* reconsolidation_outcome_name(reconsolidation_outcome_t outcome) {
    switch (outcome) {
        case RECON_OUTCOME_NONE:         return "NONE";
        case RECON_OUTCOME_STRENGTHENED: return "STRENGTHENED";
        case RECON_OUTCOME_UPDATED:      return "UPDATED";
        case RECON_OUTCOME_INTERFERENCE: return "INTERFERENCE";
        case RECON_OUTCOME_BLOCKED:      return "BLOCKED";
        case RECON_OUTCOME_EXPIRED:      return "EXPIRED";
        case RECON_OUTCOME_ROLLBACK:     return "ROLLBACK";
        default:                         return "UNKNOWN";
    }
}

NIMCP_EXPORT void reconsolidation_print_window_status(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory)
{
    if (system == NULL || memory == NULL) {
        printf("Reconsolidation: NULL parameter\n");
        return;
    }

    nimcp_mutex_lock(system->mutex);

    reconsolidation_window_t* window = find_window_by_memory(system, memory);
    if (window == NULL) {
        printf("Memory %lu: Not in reconsolidation\n", pr_memory_node_get_id(memory));
        nimcp_mutex_unlock(system->mutex);
        return;
    }

    printf("Memory %lu Reconsolidation Status:\n", window->memory_id);
    printf("  State: %s\n", reconsolidation_state_name(window->state));
    printf("  Outcome: %s\n", reconsolidation_outcome_name(window->outcome));
    printf("  Lability: %.2f (%.1fs remaining)\n",
           window->lability_strength, window->lability_remaining);
    printf("  Retrievals: %u (cumulative activation: %.2f)\n",
           window->retrieval_count, window->cumulative_activation);
    printf("  Has proposed update: %s\n", window->has_proposed_update ? "yes" : "no");
    if (window->has_proposed_update) {
        printf("  Update magnitude: %.3f\n", window->update_magnitude);
    }
    printf("  Interference: %.2f (%zu interfering memories)\n",
           window->interference_strength, window->num_interfering);

    nimcp_mutex_unlock(system->mutex);
}

NIMCP_EXPORT void reconsolidation_print_summary(reconsolidation_system_t* system) {
    if (system == NULL) {
        printf("Reconsolidation System: NULL\n");
        return;
    }

    reconsolidation_stats_t stats;
    reconsolidation_get_stats(system, &stats);

    printf("=== Reconsolidation System Summary ===\n");
    printf("Active windows: %lu / %zu\n", stats.current_active_windows, system->max_windows);
    printf("Peak windows: %lu\n", stats.peak_active_windows);
    printf("Total retrievals: %lu\n", stats.total_retrievals);
    printf("Windows created: %lu\n", stats.total_windows_created);
    printf("Updates: %lu\n", stats.total_updates);
    printf("Strengthenings: %lu\n", stats.total_strengthenings);
    printf("Interference blocks: %lu\n", stats.total_interference_blocks);
    printf("Synthesis blocks: %lu\n", stats.total_synthesis_blocks);
    printf("Rollbacks: %lu\n", stats.total_rollbacks);
    printf("Expired: %lu\n", stats.total_expired);
    printf("Mean lability duration: %.2fs\n", stats.mean_lability_duration);
    printf("Mean update magnitude: %.3f\n", stats.mean_update_magnitude);
    printf("Protein synthesis: %s\n",
           system->protein_synthesis_blocked ? "BLOCKED" : "normal");
    printf("=====================================\n");
}

NIMCP_EXPORT bool reconsolidation_validate(reconsolidation_system_t* system) {
    if (system == NULL) {
        return false;
    }

    if (system->magic != RECON_SYSTEM_MAGIC) {
        return false;
    }

    nimcp_mutex_lock(system->mutex);

    bool valid = true;

    // Check window count consistency
    size_t counted_windows = 0;
    for (size_t i = 0; i < system->max_windows; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->max_windows > 256) {
            reconsolidation_heartbeat("reconsolidat_loop",
                             (float)(i + 1) / (float)system->max_windows);
        }

        if (system->windows[i].memory != NULL) {
            counted_windows++;

            // Validate window state
            reconsolidation_window_t* window = &system->windows[i];
            if (window->state < RECON_STABLE || window->state > RECON_RESTABILIZING) {
                valid = false;
                printf("Invalid state in window %zu\n", i);
            }

            // Validate memory reference
            if (pr_memory_node_get_id(window->memory) != window->memory_id) {
                valid = false;
                printf("Memory ID mismatch in window %zu\n", i);
            }
        }
    }

    if (counted_windows != system->num_windows) {
        valid = false;
        printf("Window count mismatch: counted %zu, tracked %zu\n",
               counted_windows, system->num_windows);
    }

    // Check free indices consistency
    if (system->free_count + system->num_windows != system->max_windows) {
        valid = false;
        printf("Free index count mismatch\n");
    }

    nimcp_mutex_unlock(system->mutex);
    return valid;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Hash function for memory ID lookup
 */
static uint64_t hash_memory_id(uint64_t id) {
    // FNV-1a variant
    uint64_t hash = 14695981039346656037ULL;
    hash ^= id;
    hash *= 1099511628211ULL;
    return hash;
}

/**
 * @brief Find reconsolidation window by memory node
 */
static reconsolidation_window_t* find_window_by_memory(
    reconsolidation_system_t* system, pr_memory_node_t* memory)
{
    if (memory == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory is NULL");

        return NULL;
    }

    uint64_t mem_id = pr_memory_node_get_id(memory);
    uint64_t hash = hash_memory_id(mem_id);
    size_t bucket = hash % WINDOW_HASH_TABLE_SIZE;

    window_hash_entry_t* entry = system->hash_table[bucket];
    while (entry != NULL) {
        if (entry->memory_id == mem_id) {
            return &system->windows[entry->window_index];
        }
        entry = entry->next;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "find_window_by_memory: validation failed");
    return NULL;
}

/**
 * @brief Allocate a free window index
 */
static size_t allocate_window_index(reconsolidation_system_t* system) {
    if (system->free_count == 0) {
        return (size_t)-1;
    }

    system->free_count--;
    return system->free_indices[system->free_count];
}

/**
 * @brief Return window index to free pool
 */
static void free_window_index(reconsolidation_system_t* system, size_t index) {
    if (system->free_count < system->max_windows) {
        system->free_indices[system->free_count] = index;
        system->free_count++;
    }
}

/**
 * @brief Add window to hash table
 */
static bool add_window_to_hash(reconsolidation_system_t* system, uint64_t memory_id, size_t index) {
    if (system->entries_used >= system->entry_pool_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "add_window_to_hash: capacity exceeded");
        return false;
    }

    uint64_t hash = hash_memory_id(memory_id);
    size_t bucket = hash % WINDOW_HASH_TABLE_SIZE;

    window_hash_entry_t* entry = &system->entry_pool[system->entries_used++];
    entry->memory_id = memory_id;
    entry->window_index = index;
    entry->next = system->hash_table[bucket];
    system->hash_table[bucket] = entry;

    return true;
}

/**
 * @brief Remove window from hash table
 */
static void remove_window_from_hash(reconsolidation_system_t* system, uint64_t memory_id) {
    uint64_t hash = hash_memory_id(memory_id);
    size_t bucket = hash % WINDOW_HASH_TABLE_SIZE;

    window_hash_entry_t** prev = &system->hash_table[bucket];
    window_hash_entry_t* entry = *prev;

    while (entry != NULL) {
        if (entry->memory_id == memory_id) {
            *prev = entry->next;
            // Note: We don't actually free the entry from the pool
            // This is a simplification; in production you'd want proper pooling
            return;
        }
        prev = &entry->next;
        entry = entry->next;
    }
}

/**
 * @brief Initialize a reconsolidation window
 */
static void init_window(reconsolidation_window_t* window, pr_memory_node_t* memory, float current_time, float lability_duration) {
    window->memory = memory;
    window->memory_id = pr_memory_node_get_id(memory);
    window->state = RECON_LABILE;
    window->outcome = RECON_OUTCOME_NONE;
    window->retrieval_time = current_time;
    window->lability_remaining = lability_duration;
    window->lability_strength = 1.0f;
    window->state_enter_time = current_time;
    window->has_proposed_update = false;
    window->update_magnitude = 0.0f;
    window->num_interfering = 0;
    window->interference_strength = 0.0f;
    window->retrieval_count = 1;
    window->cumulative_activation = 0.0f;
}

/**
 * @brief Cleanup a reconsolidation window
 */
static void cleanup_window(reconsolidation_window_t* window) {
    window->memory = NULL;
    window->memory_id = 0;
    window->state = RECON_STABLE;
    window->outcome = RECON_OUTCOME_NONE;
    window->has_proposed_update = false;
    window->num_interfering = 0;
    window->interference_strength = 0.0f;
    // Note: Don't free interfering_memories array - it's reused
}

/**
 * @brief Apply lability decay based on time
 */
static void apply_lability_decay(reconsolidation_window_t* window, float delta_time, float decay_rate) {
    // Exponential decay: lability *= exp(-decay_rate * dt)
    window->lability_strength *= expf(-decay_rate * delta_time);

    // Clamp to valid range
    if (window->lability_strength < 0.0f) {
        window->lability_strength = 0.0f;
    }
}

/**
 * @brief Transition window to stable state (complete reconsolidation)
 */
static void transition_to_stable(reconsolidation_system_t* system, reconsolidation_window_t* window) {
    if (window->memory == NULL) {
        return;
    }

    // Find window index
    size_t index = window - system->windows;

    // Remove from hash table
    remove_window_from_hash(system, window->memory_id);

    // Cleanup window
    cleanup_window(window);

    // Return index to pool
    free_window_index(system, index);

    // Update count
    if (system->num_windows > 0) {
        system->num_windows--;
    }
}

/**
 * @brief Transition window to restabilizing state
 */
static void transition_to_restabilizing(reconsolidation_window_t* window, float current_time) {
    window->state = RECON_RESTABILIZING;
    window->state_enter_time = current_time;
}

/**
 * @brief Apply proposed update to memory
 */
static void apply_update_to_memory(reconsolidation_window_t* window) {
    if (window->memory == NULL || !window->has_proposed_update) {
        return;
    }

    // Update signature
    pr_memory_node_set_signature(window->memory, &window->proposed_signature);

    // Update quaternion state
    pr_memory_node_update_state(window->memory, window->proposed_quaternion);

    // Clear dirty flag
    pr_memory_node_clear_flags(window->memory, PR_NODE_FLAG_DIRTY);
}

/**
 * @brief Apply strengthening to memory
 */
static void apply_strengthening_to_memory(reconsolidation_window_t* window, float boost) {
    if (window->memory == NULL) {
        return;
    }

    // Get current state
    nimcp_quaternion_t state = pr_memory_node_get_state(window->memory);

    // Boost consolidation (w component)
    state.w += boost;
    if (state.w > 1.0f) state.w = 1.0f;

    // Also boost accessibility (z component) slightly
    state.z += boost * 0.5f;
    if (state.z > 1.0f) state.z = 1.0f;

    // Apply updated state
    pr_memory_node_update_state(window->memory, state);

    // Reinforce memory strength
    pr_memory_node_reinforce(window->memory, boost);
}

/**
 * @brief Compute signature similarity (Jaccard)
 */
static float compute_signature_similarity(const prime_signature_t* s1, const prime_signature_t* s2) {
    if (s1 == NULL || s2 == NULL) {
        return 0.0f;
    }

    // Use prime signature Jaccard function
    float sim = prime_sig_jaccard(s1, s2);
    if (sim < 0.0f) {
        return 0.0f;
    }
    return sim;
}

/**
 * @brief Compute quaternion geodesic distance
 */
static float compute_quaternion_distance(nimcp_quaternion_t q1, nimcp_quaternion_t q2) {
    return quat_geodesic_distance(q1, q2);
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void reconsolidation_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_reconsolidation_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration
 * ============================================================================ */

int reconsolidation_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reconsolidation_training_begin: NULL argument");
        return -1;
    }
    reconsolidation_heartbeat_instance(NULL, "reconsolidation_training_begin", 0.0f);

    reconsolidation_system_t* sys = (reconsolidation_system_t*)instance;

    nimcp_mutex_lock(sys->mutex);

    /* Reset training-relevant statistics */
    atomic_store(&sys->total_retrievals, 0);
    atomic_store(&sys->total_updates, 0);
    atomic_store(&sys->total_strengthenings, 0);
    atomic_store(&sys->total_interference_blocks, 0);
    atomic_store(&sys->total_rollbacks, 0);
    atomic_store(&sys->total_expired, 0);
    sys->sum_lability_duration = 0.0;
    sys->sum_update_magnitude = 0.0;
    sys->sum_interference_strength = 0.0;

    /* Disable protein synthesis blocking for training (allow all updates) */
    sys->protein_synthesis_blocked = false;

    nimcp_mutex_unlock(sys->mutex);

    reconsolidation_heartbeat_instance(NULL, "reconsolidation_training_begin", 1.0f);
    return 0;
}

int reconsolidation_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reconsolidation_training_end: NULL argument");
        return -1;
    }
    reconsolidation_heartbeat_instance(NULL, "reconsolidation_training_end", 0.0f);

    reconsolidation_system_t* sys = (reconsolidation_system_t*)instance;

    nimcp_mutex_lock(sys->mutex);

    /* Capture final training metrics */
    uint64_t total_updates = atomic_load(&sys->total_updates);
    uint64_t total_strengthenings = atomic_load(&sys->total_strengthenings);
    uint64_t total_rollbacks = atomic_load(&sys->total_rollbacks);
    (void)total_updates;
    (void)total_strengthenings;
    (void)total_rollbacks;

    /* Close any remaining active reconsolidation windows */
    for (size_t i = 0; i < sys->max_windows; i++) {
        if (sys->windows[i].state == RECON_LABILE ||
            sys->windows[i].state == RECON_RESTABILIZING) {
            sys->windows[i].state = RECON_STABLE;
        }
    }

    nimcp_mutex_unlock(sys->mutex);

    reconsolidation_heartbeat_instance(NULL, "reconsolidation_training_end", 1.0f);
    return 0;
}

int reconsolidation_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reconsolidation_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    reconsolidation_heartbeat_instance(NULL, "reconsolidation_training_step", progress);

    reconsolidation_system_t* sys = (reconsolidation_system_t*)instance;

    nimcp_mutex_lock(sys->mutex);

    /*
     * Adapt reconsolidation dynamics during training:
     * - Early training: longer lability windows (more plasticity)
     * - Late training: shorter windows (more stability)
     */
    float base_duration = sys->config.lability_duration;
    float adapted_duration = base_duration * (1.5f - progress);  /* 1.5x -> 0.5x */
    if (adapted_duration < 0.1f) adapted_duration = 0.1f;

    /* Adapt update threshold: lower early (accept more updates), higher late */
    float base_threshold = sys->config.update_threshold;
    sys->config.update_threshold = base_threshold * (0.5f + 0.5f * progress);

    /* Apply adapted lability to active windows */
    for (size_t i = 0; i < sys->max_windows; i++) {
        if (sys->windows[i].state == RECON_LABILE) {
            sys->windows[i].lability_remaining = adapted_duration;
        }
    }

    /* Restore base threshold */
    sys->config.update_threshold = base_threshold;

    nimcp_mutex_unlock(sys->mutex);

    return 0;
}
