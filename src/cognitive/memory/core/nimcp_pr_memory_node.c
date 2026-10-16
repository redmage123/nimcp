//=============================================================================
// nimcp_pr_memory_node.c - Prime Resonant Memory Node Implementation
//=============================================================================
/**
 * @file nimcp_pr_memory_node.c
 * @brief Implementation of COW-enabled memory nodes with prime signatures
 *
 * WHAT: Full implementation of PR memory node operations
 * WHY:  Enable content-addressable retrieval with semantic metadata and COW
 * HOW:  Combines prime signatures, quaternions, and unified memory management
 *
 * IMPLEMENTATION NOTES:
 * - All public functions validate inputs and return appropriate error codes
 * - Atomic operations used for flags, access counts, and entanglement counts
 * - COW operations delegate to unified_mem API
 * - Timestamps use monotonic clock for consistency
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(pr_memory_node, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Internal Constants
//=============================================================================

/** Magic number for serialization validation */
#define PR_NODE_SERIAL_MAGIC 0x50524E44  // "PRND"

/** Minimum strength before considering demotion */
#define PR_NODE_DEMOTION_THRESHOLD 0.1f

/** Minimum strength for promotion consideration */
#define PR_NODE_PROMOTION_STRENGTH_MIN 0.5f

/** CRC32 polynomial for checksum */
#define CRC32_POLYNOMIAL 0xEDB88320

//=============================================================================
// Internal Node Manager Structure
//=============================================================================

/**
 * @brief Internal node manager structure
 */
struct pr_node_manager_struct {
    unified_mem_manager_t mem_manager;    /**< Unified memory manager */
    _Atomic uint64_t next_id;             /**< Next available node ID */
    _Atomic uint64_t total_created;       /**< Total nodes created */
    bool track_nodes;                     /**< Whether to track nodes */
    pr_memory_node_t** tracked_nodes;     /**< Array of tracked nodes */
    size_t max_tracked;                   /**< Maximum tracked nodes */
    _Atomic size_t tracked_count;         /**< Current tracked count */
};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds (monotonic)
 */
static uint64_t get_current_time_ms(void) {
    struct timespec ts;

#ifdef CLOCK_MONOTONIC
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
    }
#endif

    // Fallback to real time
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
    }

    // Last resort fallback
    return (uint64_t)time(NULL) * 1000ULL;
}

/**
 * @brief Check if float is NaN
 */
static inline bool is_nan(float x) {
    return x != x;
}

/**
 * @brief Simple CRC32 checksum
 */
static uint32_t compute_crc32(const void* data, size_t size) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && size > 256) {
            pr_memory_node_heartbeat("pr_memory_no_loop",
                             (float)(i + 1) / (float)size);
        }

        crc ^= bytes[i];
        for (int j = 0; j < 8; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && 8 > 256) {
                pr_memory_node_heartbeat("pr_memory_no_loop",
                                 (float)(j + 1) / (float)8);
            }

            if (crc & 1) {
                crc = (crc >> 1) ^ CRC32_POLYNOMIAL;
            } else {
                crc >>= 1;
            }
        }
    }

    return ~crc;
}

/**
 * @brief Initialize node state from config
 */
static void init_node_state(pr_memory_node_t* node, const pr_node_config_t* config) {
    // Set tier
    node->tier = config->initial_tier;

    // Set decay rate based on tier
    node->decay_rate = pr_memory_node_default_decay_rate(config->initial_tier);

    // Initialize quaternion state
    node->state.w = config->initial_strength;  // Consolidation = initial strength
    node->state.x = nimcp_clampf(config->emotional_valence, -1.0f, 1.0f);
    node->state.y = nimcp_clampf(config->salience, 0.0f, 1.0f);
    node->state.z = nimcp_clampf(config->accessibility, 0.0f, 1.0f);

    // Initialize consolidation state
    node->current_strength = nimcp_clampf(config->initial_strength, 0.0f, 1.0f);
    node->promotion_eligibility = 0.0f;

    // Initialize temporal info
    uint64_t now = get_current_time_ms();
    node->created_time_ms = now;
    node->last_accessed_ms = now;
    atomic_store(&node->access_count, 0);

    // Initialize entanglement
    atomic_store(&node->entanglement_count, 0);

    // Initialize flags
    atomic_store(&node->flags, PR_NODE_FLAG_NONE);
    node->reserved = 0;
}

/**
 * @brief Generate next unique node ID
 */
static uint64_t generate_node_id(pr_node_manager_t manager) {
    return atomic_fetch_add(&manager->next_id, 1);
}

//=============================================================================
// Node Manager Implementation
//=============================================================================

pr_node_manager_config_t pr_node_manager_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_pr_node_manager_defa", 0.0f);


    pr_node_manager_config_t config = {
        .mem_manager = NULL,
        .starting_id = 1,
        .track_nodes = false,
        .max_tracked_nodes = 0
    };
    return config;
}

pr_node_manager_t pr_node_manager_create(const pr_node_manager_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_pr_node_manager_crea", 0.0f);


    pr_node_manager_t manager = (pr_node_manager_t)nimcp_malloc(
        sizeof(struct pr_node_manager_struct));
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate manager");

        return NULL;
    }

    // Use defaults if no config provided
    pr_node_manager_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = pr_node_manager_default_config();
    }

    // Initialize manager
    manager->mem_manager = cfg.mem_manager;
    atomic_store(&manager->next_id, cfg.starting_id);
    atomic_store(&manager->total_created, 0);
    manager->track_nodes = cfg.track_nodes;
    manager->tracked_nodes = NULL;
    manager->max_tracked = 0;
    atomic_store(&manager->tracked_count, 0);

    // Allocate tracking array if requested
    if (cfg.track_nodes && cfg.max_tracked_nodes > 0) {
        manager->tracked_nodes = (pr_memory_node_t**)nimcp_calloc(
            cfg.max_tracked_nodes, sizeof(pr_memory_node_t*));
        if (!manager->tracked_nodes) {
            nimcp_free(manager);
            manager = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_node_manager_create: manager->tracked_nodes is NULL");
            return NULL;
        }
        manager->max_tracked = cfg.max_tracked_nodes;
    }

    return manager;
}

void pr_node_manager_destroy(pr_node_manager_t manager) {
    if (!manager) {
        return;
    }

    // Free tracking array (but NOT the nodes themselves)
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_pr_node_manager_dest", 0.0f);


    if (manager->tracked_nodes) {
        nimcp_free(manager->tracked_nodes);
    }

    nimcp_free(manager);
    manager = NULL;
}

unified_mem_manager_t pr_node_manager_get_mem_manager(pr_node_manager_t manager) {
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "manager is NULL");

        return NULL;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_pr_node_manager_get_", 0.0f);


    return manager->mem_manager;
}

uint64_t pr_node_manager_get_node_count(pr_node_manager_t manager) {
    if (!manager) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_pr_node_manager_get_", 0.0f);


    if (manager->track_nodes) {
        return atomic_load(&manager->tracked_count);
    }

    return atomic_load(&manager->total_created);
}

//=============================================================================
// Node Creation and Destruction
//=============================================================================

pr_node_config_t pr_memory_node_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_default_config", 0.0f);


    pr_node_config_t config = {
        .initial_tier = PR_MEMORY_TIER_Z0,
        .initial_strength = 1.0f,
        .emotional_valence = 0.0f,
        .salience = 0.5f,
        .accessibility = 1.0f,
        .compute_signature = true,
        .enable_cow = true
    };
    return config;
}

pr_memory_node_t* pr_memory_node_create(
    pr_node_manager_t manager,
    const void* data,
    size_t data_size,
    const pr_node_config_t* config
) {
    // Validate inputs
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "manager is NULL");

        return NULL;
    }

    if (!data && data_size > 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_memory_node_create: data is NULL");
        return NULL;
    }

    // Use defaults if no config
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_create", 0.0f);


    pr_node_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = pr_memory_node_default_config();
    }

    // Allocate node structure
    pr_memory_node_t* node = (pr_memory_node_t*)nimcp_malloc(sizeof(pr_memory_node_t));
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate node");

        return NULL;
    }

    // Initialize to zero
    memset(node, 0, sizeof(pr_memory_node_t));

    // Generate unique ID
    node->node_id = generate_node_id(manager);

    // Store data size
    node->data_size = data_size;

    // Allocate data via unified memory if we have a manager
    if (data_size > 0 && manager->mem_manager) {
        unified_mem_request_t req = unified_mem_request(
            data_size,
            data,
            cfg.enable_cow
        );

        node->data_handle = unified_mem_alloc(manager->mem_manager, &req);
        if (!node->data_handle) {
            nimcp_free(node);
            node = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_memory_node_create: node->data_handle is NULL");
            return NULL;
        }
    } else if (data_size > 0) {
        // No memory manager - use direct allocation
        // Create a simple direct allocation (no COW)
        node->data_handle = NULL;  // Will need to handle this case
    } else {
        node->data_handle = NULL;
    }

    // Initialize state from config
    init_node_state(node, &cfg);

    // Compute prime signature if requested
    if (cfg.compute_signature && data && data_size > 0) {
        prime_signature_t* sig = prime_sig_from_content(data, data_size);
        if (sig) {
            // Copy signature into node
            memcpy(&node->signature, sig, sizeof(prime_signature_t));
            prime_sig_destroy(sig);
        } else {
            // Signature computation failed - initialize empty
            memset(&node->signature, 0, sizeof(prime_signature_t));
        }
    } else {
        // Initialize empty signature
        memset(&node->signature, 0, sizeof(prime_signature_t));
    }

    // Update manager statistics
    atomic_fetch_add(&manager->total_created, 1);

    // Track node if tracking enabled
    if (manager->track_nodes && manager->tracked_nodes) {
        size_t idx = atomic_fetch_add(&manager->tracked_count, 1);
        if (idx < manager->max_tracked) {
            manager->tracked_nodes[idx] = node;
        }
    }

    return node;
}

pr_memory_node_t* pr_memory_node_create_with_signature(
    pr_node_manager_t manager,
    const void* data,
    size_t data_size,
    const prime_signature_t* signature,
    const pr_node_config_t* config
) {
    // Create node without auto-computing signature
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_create_with_signatur", 0.0f);


    pr_node_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = pr_memory_node_default_config();
    }
    cfg.compute_signature = false;

    pr_memory_node_t* node = pr_memory_node_create(manager, data, data_size, &cfg);
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;
    }

    // Set provided signature
    if (signature) {
        memcpy(&node->signature, signature, sizeof(prime_signature_t));
    }

    return node;
}

void pr_memory_node_destroy(pr_memory_node_t* node) {
    if (!node) {
        return;
    }

    // Release COW handle
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_destroy", 0.0f);


    if (node->data_handle) {
        unified_mem_free(node->data_handle);
        node->data_handle = NULL;
    }

    // Free node structure
    nimcp_free(node);
    node = NULL;
}

pr_memory_node_t* pr_memory_node_clone(
    const pr_memory_node_t* node,
    pr_node_manager_t manager
) {
    if (!node || !manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_memory_node_clone: required parameter is NULL (node, manager)");
        return NULL;
    }

    // Allocate new node
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_clone", 0.0f);


    pr_memory_node_t* clone = (pr_memory_node_t*)nimcp_malloc(sizeof(pr_memory_node_t));
    if (!clone) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate clone");

        return NULL;
    }

    // Copy all metadata
    memcpy(clone, node, sizeof(pr_memory_node_t));

    // Generate new unique ID
    clone->node_id = generate_node_id(manager);

    // Clone COW handle (shares underlying data)
    if (node->data_handle) {
        clone->data_handle = unified_mem_clone(node->data_handle);
        if (!clone->data_handle) {
            nimcp_free(clone);
            clone = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_memory_node_clone: clone->data_handle is NULL");
            return NULL;
        }
    }

    // Reset temporal counters
    uint64_t now = get_current_time_ms();
    clone->created_time_ms = now;
    clone->last_accessed_ms = now;
    atomic_store(&clone->access_count, 0);

    // Reset entanglement (clone starts independent)
    atomic_store(&clone->entanglement_count, 0);

    // Clear transient flags
    atomic_store(&clone->flags, PR_NODE_FLAG_NONE);

    // Update manager statistics
    atomic_fetch_add(&manager->total_created, 1);

    return clone;
}

//=============================================================================
// Data Access Implementation
//=============================================================================

const void* pr_memory_node_read(pr_memory_node_t* node) {
    if (!node || !node->data_handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_memory_node_read: required parameter is NULL (node, node->data_handle)");
        return NULL;
    }

    // Update access metadata
    node->last_accessed_ms = get_current_time_ms();
    atomic_fetch_add(&node->access_count, 1);

    // Return read-only pointer
    return unified_mem_read(node->data_handle);
}

void* pr_memory_node_write(pr_memory_node_t* node) {
    if (!node || !node->data_handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_memory_node_write: required parameter is NULL (node, node->data_handle)");
        return NULL;
    }

    // Check if locked
    uint32_t flags = atomic_load(&node->flags);
    if (flags & PR_NODE_FLAG_LOCKED) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "pr_memory_node_write: validation failed");
        return NULL;
    }

    // Update access metadata
    node->last_accessed_ms = get_current_time_ms();
    atomic_fetch_add(&node->access_count, 1);

    // Mark signature as potentially dirty
    atomic_fetch_or(&node->flags, PR_NODE_FLAG_DIRTY);

    // Return writable pointer (may trigger COW)
    return unified_mem_write(node->data_handle);
}

bool pr_memory_node_is_shared(const pr_memory_node_t* node) {
    if (!node || !node->data_handle) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_is_shared", 0.0f);


    return unified_mem_is_shared(node->data_handle);
}

size_t pr_memory_node_get_data_size(const pr_memory_node_t* node) {
    if (!node) {
        return 0;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_get_data_size", 0.0f);


    return node->data_size;
}

pr_node_error_t pr_memory_node_make_private(pr_memory_node_t* node) {
    if (!node) {
        return PR_NODE_ERROR_NULL_POINTER;
    }

    if (!node->data_handle) {
        return PR_NODE_SUCCESS;  // No data to make private
    }

    if (!unified_mem_make_private(node->data_handle)) {
        return PR_NODE_ERROR_COW_FAILED;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_make_private", 0.0f);


    return PR_NODE_SUCCESS;
}

//=============================================================================
// State Management Implementation
//=============================================================================

pr_node_error_t pr_memory_node_update_state(
    pr_memory_node_t* node,
    nimcp_quaternion_t state
) {
    if (!node) {
        return PR_NODE_ERROR_NULL_POINTER;
    }

    // Clamp components to valid ranges
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_update_state", 0.0f);


    node->state.w = nimcp_clampf(state.w, 0.0f, 1.0f);        // Consolidation
    node->state.x = nimcp_clampf(state.x, -1.0f, 1.0f);      // Emotion
    node->state.y = nimcp_clampf(state.y, 0.0f, 1.0f);       // Salience
    node->state.z = nimcp_clampf(state.z, 0.0f, 1.0f);       // Accessibility

    // Sync consolidation with current_strength
    node->current_strength = node->state.w;

    return PR_NODE_SUCCESS;
}

nimcp_quaternion_t pr_memory_node_get_state(const pr_memory_node_t* node) {
    if (!node) {
        return quat_identity();
    }
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_get_state", 0.0f);


    return node->state;
}

pr_node_error_t pr_memory_node_blend_state(
    pr_memory_node_t* node,
    nimcp_quaternion_t other,
    float t
) {
    if (!node) {
        return PR_NODE_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_blend_state", 0.0f);


    t = nimcp_clampf(t, 0.0f, 1.0f);

    // Use SLERP for smooth blending
    node->state = quat_slerp(node->state, other, t);

    // Sync consolidation with current_strength
    node->current_strength = node->state.w;

    return PR_NODE_SUCCESS;
}

pr_node_error_t pr_memory_node_update_state_components(
    pr_memory_node_t* node,
    float consolidation,
    float emotion,
    float salience,
    float accessibility
) {
    if (!node) {
        return PR_NODE_ERROR_NULL_POINTER;
    }

    // Only update components that are not NaN
    if (!is_nan(consolidation)) {
        node->state.w = nimcp_clampf(consolidation, 0.0f, 1.0f);
        node->current_strength = node->state.w;
    }

    if (!is_nan(emotion)) {
        node->state.x = nimcp_clampf(emotion, -1.0f, 1.0f);
    }

    if (!is_nan(salience)) {
        node->state.y = nimcp_clampf(salience, 0.0f, 1.0f);
    }

    if (!is_nan(accessibility)) {
        node->state.z = nimcp_clampf(accessibility, 0.0f, 1.0f);
    }

    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_update_state_compone", 0.0f);


    return PR_NODE_SUCCESS;
}

//=============================================================================
// Signature Management Implementation
//=============================================================================

pr_node_error_t pr_memory_node_update_signature(pr_memory_node_t* node) {
    if (!node) {
        return PR_NODE_ERROR_NULL_POINTER;
    }

    // Get data for signature computation
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_update_signature", 0.0f);


    const void* data = NULL;
    if (node->data_handle) {
        data = unified_mem_read(node->data_handle);
    }

    if (!data || node->data_size == 0) {
        // Clear signature for empty data
        memset(&node->signature, 0, sizeof(prime_signature_t));
        atomic_fetch_and(&node->flags, ~PR_NODE_FLAG_DIRTY);
        return PR_NODE_SUCCESS;
    }

    // Compute new signature
    prime_signature_t* new_sig = prime_sig_from_content(data, node->data_size);
    if (!new_sig) {
        return PR_NODE_ERROR_NO_MEMORY;
    }

    // Copy into node
    memcpy(&node->signature, new_sig, sizeof(prime_signature_t));
    prime_sig_destroy(new_sig);

    // Clear dirty flag
    atomic_fetch_and(&node->flags, ~PR_NODE_FLAG_DIRTY);

    return PR_NODE_SUCCESS;
}

pr_node_error_t pr_memory_node_set_signature(
    pr_memory_node_t* node,
    const prime_signature_t* signature
) {
    if (!node || !signature) {
        return PR_NODE_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_set_signature", 0.0f);


    memcpy(&node->signature, signature, sizeof(prime_signature_t));

    // Clear dirty flag since we explicitly set signature
    atomic_fetch_and(&node->flags, ~PR_NODE_FLAG_DIRTY);

    return PR_NODE_SUCCESS;
}

const prime_signature_t* pr_memory_node_get_signature(const pr_memory_node_t* node) {
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_get_signature", 0.0f);


    return &node->signature;
}

float pr_memory_node_signature_similarity(
    const pr_memory_node_t* node,
    const pr_memory_node_t* other,
    prime_sig_similarity_method_t method
) {
    if (!node || !other) {
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_signature_similarity", 0.0f);


    return prime_sig_similarity(&node->signature, &other->signature, method);
}

//=============================================================================
// Tier Management Implementation
//=============================================================================

pr_node_error_t pr_memory_node_promote(pr_memory_node_t* node) {
    if (!node) {
        return PR_NODE_ERROR_NULL_POINTER;
    }

    // Check if already at top tier
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_promote", 0.0f);


    if (node->tier >= PR_MEMORY_TIER_Z3) {
        return PR_NODE_ERROR_ALREADY_TOP;
    }

    // Check if locked
    uint32_t flags = atomic_load(&node->flags);
    if (flags & PR_NODE_FLAG_LOCKED) {
        return PR_NODE_ERROR_LOCKED;
    }

    // Set promoting flag
    atomic_fetch_or(&node->flags, PR_NODE_FLAG_PROMOTING);

    // Promote tier
    node->tier = (pr_memory_tier_t)(node->tier + 1);

    // Update decay rate for new tier
    node->decay_rate = pr_memory_node_default_decay_rate(node->tier);

    // Reset promotion eligibility
    node->promotion_eligibility = 0.0f;

    // Clear promoting flag
    atomic_fetch_and(&node->flags, ~PR_NODE_FLAG_PROMOTING);

    return PR_NODE_SUCCESS;
}

pr_node_error_t pr_memory_node_demote(pr_memory_node_t* node) {
    if (!node) {
        return PR_NODE_ERROR_NULL_POINTER;
    }

    // Check if already at bottom tier
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_demote", 0.0f);


    if (node->tier <= PR_MEMORY_TIER_Z0) {
        return PR_NODE_ERROR_ALREADY_BOTTOM;
    }

    // Check if locked
    uint32_t flags = atomic_load(&node->flags);
    if (flags & PR_NODE_FLAG_LOCKED) {
        return PR_NODE_ERROR_LOCKED;
    }

    // Set demoting flag
    atomic_fetch_or(&node->flags, PR_NODE_FLAG_DEMOTING);

    // Demote tier
    node->tier = (pr_memory_tier_t)(node->tier - 1);

    // Update decay rate for new tier
    node->decay_rate = pr_memory_node_default_decay_rate(node->tier);

    // Clear demoting flag
    atomic_fetch_and(&node->flags, ~PR_NODE_FLAG_DEMOTING);

    return PR_NODE_SUCCESS;
}

pr_node_error_t pr_memory_node_set_tier(
    pr_memory_node_t* node,
    pr_memory_tier_t tier
) {
    if (!node) {
        return PR_NODE_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_set_tier", 0.0f);


    if (tier >= PR_MEMORY_TIER_COUNT) {
        return PR_NODE_ERROR_INVALID_TIER;
    }

    node->tier = tier;
    node->decay_rate = pr_memory_node_default_decay_rate(tier);

    return PR_NODE_SUCCESS;
}

pr_memory_tier_t pr_memory_node_get_tier(const pr_memory_node_t* node) {
    if (!node) {
        return PR_MEMORY_TIER_Z0;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_get_tier", 0.0f);


    return node->tier;
}

const char* pr_memory_tier_name(pr_memory_tier_t tier) {
    switch (tier) {
        case PR_MEMORY_TIER_Z0:
            return "Working (Z0)";
        case PR_MEMORY_TIER_Z1:
            return "Short-term (Z1)";
        case PR_MEMORY_TIER_Z2:
            return "Long-term (Z2)";
        case PR_MEMORY_TIER_Z3:
            return "Permanent (Z3)";
        default:
            return "Unknown";
    }
}

//=============================================================================
// Decay and Consolidation Implementation
//=============================================================================

float pr_memory_node_apply_decay(pr_memory_node_t* node, float elapsed_seconds) {
    if (!node) {
        return 0.0f;
    }

    // Z3 never decays
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_apply_decay", 0.0f);


    if (node->tier == PR_MEMORY_TIER_Z3 || node->decay_rate <= 0.0f) {
        return node->current_strength;
    }

    // Set decaying flag
    atomic_fetch_or(&node->flags, PR_NODE_FLAG_DECAYING);

    // Apply exponential decay: S(t) = S(0) * e^(-k*t)
    float decay_factor = expf(-node->decay_rate * elapsed_seconds);
    node->current_strength *= decay_factor;

    // Clamp to valid range
    node->current_strength = nimcp_clampf(node->current_strength, 0.0f, 1.0f);

    // Sync with quaternion consolidation component
    node->state.w = node->current_strength;

    // Clear decaying flag
    atomic_fetch_and(&node->flags, ~PR_NODE_FLAG_DECAYING);

    return node->current_strength;
}

float pr_memory_node_reinforce(pr_memory_node_t* node, float reinforcement) {
    if (!node) {
        return 0.0f;
    }

    // Add reinforcement
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_reinforce", 0.0f);


    node->current_strength += nimcp_clampf(reinforcement, 0.0f, 1.0f);
    node->current_strength = nimcp_clampf(node->current_strength, 0.0f, 1.0f);

    // Sync with quaternion
    node->state.w = node->current_strength;

    // Boost promotion eligibility based on reinforcement
    node->promotion_eligibility += reinforcement * 0.1f;
    node->promotion_eligibility = nimcp_clampf(
        node->promotion_eligibility,
        PR_NODE_MIN_PROMOTION_ELIGIBILITY,
        PR_NODE_MAX_PROMOTION_ELIGIBILITY
    );

    return node->current_strength;
}

bool pr_memory_node_check_eligibility(const pr_memory_node_t* node) {
    if (!node) {
        return false;
    }

    // Cannot promote from Z3
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_check_eligibility", 0.0f);


    if (node->tier >= PR_MEMORY_TIER_Z3) {
        return false;
    }

    // Check promotion eligibility threshold
    if (node->promotion_eligibility < PR_NODE_PROMOTION_THRESHOLD) {
        return false;
    }

    // Check minimum strength
    if (node->current_strength < PR_NODE_PROMOTION_STRENGTH_MIN) {
        return false;
    }

    return true;
}

float pr_memory_node_update_eligibility(
    pr_memory_node_t* node,
    float access_boost,
    float time_factor
) {
    if (!node) {
        return 0.0f;
    }

    // Combine access pattern with time factor
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_update_eligibility", 0.0f);


    float boost = nimcp_clampf(access_boost, 0.0f, 1.0f) * nimcp_clampf(time_factor, 0.0f, 1.0f);

    // Update eligibility with gradual increase
    node->promotion_eligibility += boost * 0.05f;
    node->promotion_eligibility = nimcp_clampf(
        node->promotion_eligibility,
        PR_NODE_MIN_PROMOTION_ELIGIBILITY,
        PR_NODE_MAX_PROMOTION_ELIGIBILITY
    );

    return node->promotion_eligibility;
}

float pr_memory_node_default_decay_rate(pr_memory_tier_t tier) {
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_default_decay_rate", 0.0f);


    switch (tier) {
        case PR_MEMORY_TIER_Z0:
            return PR_NODE_DECAY_Z0;
        case PR_MEMORY_TIER_Z1:
            return PR_NODE_DECAY_Z1;
        case PR_MEMORY_TIER_Z2:
            return PR_NODE_DECAY_Z2;
        case PR_MEMORY_TIER_Z3:
            return PR_NODE_DECAY_Z3;
        default:
            return PR_NODE_DECAY_Z0;
    }
}

pr_node_error_t pr_memory_node_set_decay_rate(
    pr_memory_node_t* node,
    float decay_rate
) {
    if (!node) {
        return PR_NODE_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_set_decay_rate", 0.0f);


    node->decay_rate = (decay_rate >= 0.0f) ? decay_rate : 0.0f;

    return PR_NODE_SUCCESS;
}

//=============================================================================
// Entanglement Management Implementation
//=============================================================================

uint32_t pr_memory_node_add_entanglement(pr_memory_node_t* node) {
    if (!node) {
        return 0;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_add_entanglement", 0.0f);


    return atomic_fetch_add(&node->entanglement_count, 1) + 1;
}

uint32_t pr_memory_node_remove_entanglement(pr_memory_node_t* node) {
    if (!node) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_remove_entanglement", 0.0f);


    uint32_t current = atomic_load(&node->entanglement_count);
    if (current == 0) {
        return 0;
    }

    return atomic_fetch_sub(&node->entanglement_count, 1) - 1;
}

uint32_t pr_memory_node_get_entanglement_count(const pr_memory_node_t* node) {
    if (!node) {
        return 0;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_get_entanglement_cou", 0.0f);


    return atomic_load(&node->entanglement_count);
}

//=============================================================================
// Flags and Status Implementation
//=============================================================================

uint32_t pr_memory_node_set_flags(pr_memory_node_t* node, uint32_t flags) {
    if (!node) {
        return 0;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_set_flags", 0.0f);


    return atomic_fetch_or(&node->flags, flags);
}

uint32_t pr_memory_node_clear_flags(pr_memory_node_t* node, uint32_t flags) {
    if (!node) {
        return 0;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_clear_flags", 0.0f);


    return atomic_fetch_and(&node->flags, ~flags);
}

uint32_t pr_memory_node_get_flags(const pr_memory_node_t* node) {
    if (!node) {
        return 0;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_get_flags", 0.0f);


    return atomic_load(&node->flags);
}

bool pr_memory_node_has_flag(const pr_memory_node_t* node, pr_node_flags_t flag) {
    if (!node) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_has_flag", 0.0f);


    return (atomic_load(&node->flags) & flag) != 0;
}

pr_node_error_t pr_memory_node_lock(pr_memory_node_t* node) {
    if (!node) {
        return PR_NODE_ERROR_NULL_POINTER;
    }

    // Try to set lock flag atomically
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_lock", 0.0f);


    uint32_t expected = atomic_load(&node->flags);
    if (expected & PR_NODE_FLAG_LOCKED) {
        return PR_NODE_ERROR_LOCKED;
    }

    // Attempt to set the lock
    while (!atomic_compare_exchange_weak(&node->flags, &expected,
                                          expected | PR_NODE_FLAG_LOCKED)) {
        if (expected & PR_NODE_FLAG_LOCKED) {
            return PR_NODE_ERROR_LOCKED;
        }
    }

    return PR_NODE_SUCCESS;
}

pr_node_error_t pr_memory_node_unlock(pr_memory_node_t* node) {
    if (!node) {
        return PR_NODE_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_unlock", 0.0f);


    atomic_fetch_and(&node->flags, ~PR_NODE_FLAG_LOCKED);
    return PR_NODE_SUCCESS;
}

//=============================================================================
// Statistics and Information Implementation
//=============================================================================

pr_node_error_t pr_memory_node_get_stats(
    const pr_memory_node_t* node,
    pr_node_stats_t* stats
) {
    if (!node || !stats) {
        return PR_NODE_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_get_stats", 0.0f);


    uint64_t now = get_current_time_ms();

    stats->node_id = node->node_id;
    stats->tier = node->tier;
    stats->data_size = node->data_size;
    stats->access_count = atomic_load(&node->access_count);
    stats->age_ms = (now >= node->created_time_ms) ?
                    (now - node->created_time_ms) : 0;
    stats->idle_ms = (now >= node->last_accessed_ms) ?
                     (now - node->last_accessed_ms) : 0;
    stats->current_strength = node->current_strength;
    stats->promotion_eligibility = node->promotion_eligibility;
    stats->is_cow_shared = pr_memory_node_is_shared(node);
    stats->entanglement_count = atomic_load(&node->entanglement_count);
    stats->flags = atomic_load(&node->flags);

    return PR_NODE_SUCCESS;
}

uint64_t pr_memory_node_get_id(const pr_memory_node_t* node) {
    if (!node) {
        return PR_NODE_INVALID_ID;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_get_id", 0.0f);


    return node->node_id;
}

uint64_t pr_memory_node_get_age_ms(
    const pr_memory_node_t* node,
    uint64_t current_time_ms
) {
    if (!node) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_get_age_ms", 0.0f);


    if (current_time_ms >= node->created_time_ms) {
        return current_time_ms - node->created_time_ms;
    }

    return 0;
}

uint64_t pr_memory_node_get_idle_ms(
    const pr_memory_node_t* node,
    uint64_t current_time_ms
) {
    if (!node) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_get_idle_ms", 0.0f);


    if (current_time_ms >= node->last_accessed_ms) {
        return current_time_ms - node->last_accessed_ms;
    }

    return 0;
}

//=============================================================================
// Serialization Implementation
//=============================================================================

size_t pr_memory_node_serialization_size(const pr_memory_node_t* node) {
    if (!node) {
        return 0;
    }

    // Header + signature + state + metadata + data
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_serialization_size", 0.0f);


    return sizeof(pr_node_serial_header_t) +
           sizeof(prime_signature_t) +
           sizeof(nimcp_quaternion_t) +
           64 +  // Metadata fields
           node->data_size;
}

pr_node_error_t pr_memory_node_serialize(
    const pr_memory_node_t* node,
    void* buffer,
    size_t buffer_size,
    size_t* written_size
) {
    if (!node) {
        return PR_NODE_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_serialize", 0.0f);


    size_t required = pr_memory_node_serialization_size(node);

    // If no buffer, just return required size
    if (!buffer) {
        if (written_size) {
            *written_size = required;
        }
        return PR_NODE_SUCCESS;
    }

    if (buffer_size < required) {
        if (written_size) {
            *written_size = required;
        }
        return PR_NODE_ERROR_INVALID_SIZE;
    }

    // Set serializing flag
    // Note: Using const_cast pattern for flag setting
    pr_memory_node_t* mutable_node = (pr_memory_node_t*)node;
    atomic_fetch_or(&mutable_node->flags, PR_NODE_FLAG_SERIALIZING);

    uint8_t* ptr = (uint8_t*)buffer;
    size_t offset = 0;

    // Write header
    pr_node_serial_header_t header = {
        .magic = PR_NODE_SERIAL_MAGIC,
        .version = PR_NODE_SERIALIZATION_VERSION,
        .node_id = node->node_id,
        .data_size = node->data_size,
        .total_size = required,
        .checksum = 0,  // Computed later
        .flags = atomic_load(&node->flags)
    };
    memcpy(ptr + offset, &header, sizeof(header));
    offset += sizeof(header);

    // Write signature
    memcpy(ptr + offset, &node->signature, sizeof(prime_signature_t));
    offset += sizeof(prime_signature_t);

    // Write quaternion state
    memcpy(ptr + offset, &node->state, sizeof(nimcp_quaternion_t));
    offset += sizeof(nimcp_quaternion_t);

    // Write metadata
    memcpy(ptr + offset, &node->tier, sizeof(pr_memory_tier_t));
    offset += sizeof(pr_memory_tier_t);

    memcpy(ptr + offset, &node->created_time_ms, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    memcpy(ptr + offset, &node->last_accessed_ms, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    uint64_t access = atomic_load(&node->access_count);
    memcpy(ptr + offset, &access, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    memcpy(ptr + offset, &node->decay_rate, sizeof(float));
    offset += sizeof(float);

    memcpy(ptr + offset, &node->promotion_eligibility, sizeof(float));
    offset += sizeof(float);

    memcpy(ptr + offset, &node->current_strength, sizeof(float));
    offset += sizeof(float);

    uint32_t entangle = atomic_load(&node->entanglement_count);
    memcpy(ptr + offset, &entangle, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Padding to 64 bytes
    while (offset < sizeof(pr_node_serial_header_t) + sizeof(prime_signature_t) +
                    sizeof(nimcp_quaternion_t) + 64) {
        ptr[offset++] = 0;
    }

    // Write data
    if (node->data_size > 0 && node->data_handle) {
        const void* data = unified_mem_read(node->data_handle);
        if (data) {
            memcpy(ptr + offset, data, node->data_size);
        } else {
            memset(ptr + offset, 0, node->data_size);
        }
    }
    offset += node->data_size;

    // Compute and write checksum
    pr_node_serial_header_t* hdr = (pr_node_serial_header_t*)buffer;
    hdr->checksum = compute_crc32(ptr + sizeof(pr_node_serial_header_t),
                                   offset - sizeof(pr_node_serial_header_t));

    // Clear serializing flag
    atomic_fetch_and(&mutable_node->flags, ~PR_NODE_FLAG_SERIALIZING);

    if (written_size) {
        *written_size = offset;
    }

    return PR_NODE_SUCCESS;
}

pr_memory_node_t* pr_memory_node_deserialize(
    pr_node_manager_t manager,
    const void* buffer,
    size_t buffer_size,
    size_t* bytes_read
) {
    if (!manager || !buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_memory_node_deserialize: required parameter is NULL (manager, buffer)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_deserialize", 0.0f);


    if (buffer_size < sizeof(pr_node_serial_header_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_memory_node_deserialize: validation failed");
        return NULL;
    }

    const uint8_t* ptr = (const uint8_t*)buffer;
    size_t offset = 0;

    // Read and validate header
    pr_node_serial_header_t header;
    memcpy(&header, ptr + offset, sizeof(header));
    offset += sizeof(header);

    // Validate magic number
    if (header.magic != PR_NODE_SERIAL_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_memory_node_deserialize: validation failed");
        return NULL;
    }

    // Validate version
    if (header.version > PR_NODE_SERIALIZATION_VERSION) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_memory_node_deserialize: validation failed");
        return NULL;
    }

    // Validate size
    if (buffer_size < header.total_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_memory_node_deserialize: validation failed");
        return NULL;
    }

    // Verify checksum
    uint32_t expected_crc = compute_crc32(ptr + sizeof(pr_node_serial_header_t),
                                           header.total_size - sizeof(pr_node_serial_header_t));
    if (header.checksum != expected_crc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_memory_node_deserialize: validation failed");
        return NULL;
    }

    // Read signature
    prime_signature_t signature;
    memcpy(&signature, ptr + offset, sizeof(prime_signature_t));
    offset += sizeof(prime_signature_t);

    // Read quaternion state
    nimcp_quaternion_t state;
    memcpy(&state, ptr + offset, sizeof(nimcp_quaternion_t));
    offset += sizeof(nimcp_quaternion_t);

    // Read metadata
    pr_memory_tier_t tier;
    memcpy(&tier, ptr + offset, sizeof(pr_memory_tier_t));
    offset += sizeof(pr_memory_tier_t);

    uint64_t created_time_ms = 0;
    memcpy(&created_time_ms, ptr + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    uint64_t last_accessed_ms = 0;
    memcpy(&last_accessed_ms, ptr + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    uint64_t access_count = 0;
    memcpy(&access_count, ptr + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    float decay_rate = 0.0f;
    memcpy(&decay_rate, ptr + offset, sizeof(float));
    offset += sizeof(float);

    float promotion_eligibility = 0.0f;
    memcpy(&promotion_eligibility, ptr + offset, sizeof(float));
    offset += sizeof(float);

    float current_strength = 0.0f;
    memcpy(&current_strength, ptr + offset, sizeof(float));
    offset += sizeof(float);

    uint32_t entanglement_count = 0;
    memcpy(&entanglement_count, ptr + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Skip padding
    offset = sizeof(pr_node_serial_header_t) + sizeof(prime_signature_t) +
             sizeof(nimcp_quaternion_t) + 64;

    // Get data pointer
    const void* data = (header.data_size > 0) ? (ptr + offset) : NULL;

    // Create node with deserialized data
    pr_node_config_t config = {
        .initial_tier = tier,
        .initial_strength = current_strength,
        .emotional_valence = state.x,
        .salience = state.y,
        .accessibility = state.z,
        .compute_signature = false,  // We have the signature
        .enable_cow = true
    };

    pr_memory_node_t* node = pr_memory_node_create(
        manager, data, header.data_size, &config);
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;
    }

    // Restore exact state
    memcpy(&node->signature, &signature, sizeof(prime_signature_t));
    node->state = state;
    node->created_time_ms = created_time_ms;
    node->last_accessed_ms = last_accessed_ms;
    atomic_store(&node->access_count, access_count);
    node->decay_rate = decay_rate;
    node->promotion_eligibility = promotion_eligibility;
    node->current_strength = current_strength;
    atomic_store(&node->entanglement_count, entanglement_count);

    if (bytes_read) {
        *bytes_read = header.total_size;
    }

    return node;
}

//=============================================================================
// Utility Functions Implementation
//=============================================================================

const char* pr_node_error_string(pr_node_error_t error) {
    switch (error) {
        case PR_NODE_SUCCESS:
            return "Success";
        case PR_NODE_ERROR_NULL_POINTER:
            return "Null pointer";
        case PR_NODE_ERROR_INVALID_TIER:
            return "Invalid tier";
        case PR_NODE_ERROR_INVALID_STATE:
            return "Invalid state";
        case PR_NODE_ERROR_NO_MEMORY:
            return "Memory allocation failed";
        case PR_NODE_ERROR_COW_FAILED:
            return "COW operation failed";
        case PR_NODE_ERROR_LOCKED:
            return "Node is locked";
        case PR_NODE_ERROR_ALREADY_TOP:
            return "Already at highest tier";
        case PR_NODE_ERROR_ALREADY_BOTTOM:
            return "Already at lowest tier";
        case PR_NODE_ERROR_SERIALIZE:
            return "Serialization failed";
        case PR_NODE_ERROR_DESERIALIZE:
            return "Deserialization failed";
        case PR_NODE_ERROR_VERSION:
            return "Incompatible version";
        case PR_NODE_ERROR_INVALID_SIZE:
            return "Invalid size";
        default:
            return "Unknown error";
    }
}

uint64_t pr_node_current_time_ms(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_pr_node_current_time", 0.0f);


    return get_current_time_ms();
}

float pr_memory_node_resonance(
    const pr_memory_node_t* node1,
    const pr_memory_node_t* node2,
    float signature_weight,
    float state_weight
) {
    if (!node1 || !node2) {
        return -1.0f;
    }

    // Normalize weights
    /* Phase 8: Heartbeat at operation start */
    pr_memory_node_heartbeat("pr_memory_no_resonance", 0.0f);


    float total_weight = signature_weight + state_weight;
    if (total_weight < PR_NODE_EPSILON) {
        return 0.0f;
    }

    float sig_w = signature_weight / (fabsf(total_weight) > 1e-7f ? total_weight : 1e-7f);
    float state_w = state_weight / (fabsf(total_weight) > 1e-7f ? total_weight : 1e-7f);

    // Compute signature similarity (Jaccard)
    float sig_similarity = prime_sig_jaccard(&node1->signature, &node2->signature);
    if (sig_similarity < 0.0f) {
        sig_similarity = 0.0f;
    }

    // Compute state similarity using geodesic distance
    float state_distance = quat_geodesic_distance(node1->state, node2->state);
    // Convert distance to similarity (distance 0 = similarity 1, distance pi = similarity 0)
    float state_similarity = 1.0f - (state_distance / (float)M_PI);
    state_similarity = nimcp_clampf(state_similarity, 0.0f, 1.0f);

    // Combine weighted scores
    float resonance = sig_w * sig_similarity + state_w * state_similarity;

    return nimcp_clampf(resonance, 0.0f, 1.0f);
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void pr_memory_node_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_pr_memory_node_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration
 * ============================================================================ */

int pr_memory_node_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_memory_node_training_begin: NULL argument");
        return -1;
    }
    pr_memory_node_heartbeat_instance(NULL, "pr_memory_node_training_begin", 0.0f);

    struct pr_node_manager_struct* mgr = (struct pr_node_manager_struct*)instance;

    /* Reset ID counters for training epoch tracking */
    atomic_store(&mgr->total_created, 0);

    /* Reset tracked node count to baseline for training measurement */
    if (mgr->track_nodes) {
        atomic_store(&mgr->tracked_count, 0);
    }

    pr_memory_node_heartbeat_instance(NULL, "pr_memory_node_training_begin", 1.0f);
    return 0;
}

int pr_memory_node_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_memory_node_training_end: NULL argument");
        return -1;
    }
    pr_memory_node_heartbeat_instance(NULL, "pr_memory_node_training_end", 0.0f);

    struct pr_node_manager_struct* mgr = (struct pr_node_manager_struct*)instance;

    /* Compute final training metrics from node stats */
    uint64_t nodes_created = atomic_load(&mgr->total_created);
    size_t tracked = atomic_load(&mgr->tracked_count);
    (void)nodes_created;
    (void)tracked;

    pr_memory_node_heartbeat_instance(NULL, "pr_memory_node_training_end", 1.0f);
    return 0;
}

int pr_memory_node_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_memory_node_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    pr_memory_node_heartbeat_instance(NULL, "pr_memory_node_training_step", progress);

    struct pr_node_manager_struct* mgr = (struct pr_node_manager_struct*)instance;

    /* Apply decay to all tracked nodes based on training progress */
    if (mgr->track_nodes && mgr->tracked_nodes) {
        size_t count = atomic_load(&mgr->tracked_count);
        for (size_t i = 0; i < count && i < mgr->max_tracked; i++) {
            pr_memory_node_t* node = mgr->tracked_nodes[i];
            if (node) {
                /* Gradually strengthen nodes during training via reinforcement */
                float boost = 0.001f * progress;
                node->current_strength = nimcp_clampf(
                    node->current_strength + boost, 0.0f, 1.0f);
                /* Update quaternion consolidation component */
                node->state.w = nimcp_clampf(
                    node->state.w + boost * 0.5f, 0.0f, 1.0f);
            }
        }
    }

    return 0;
}
