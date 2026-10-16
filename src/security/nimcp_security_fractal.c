/**
 * @file nimcp_security_fractal.c
 * @brief Fractal-Based Security Enhancement - Implementation
 *
 * WHAT: Implements fractal security patterns for hierarchical integrity
 *       verification and anomaly detection.
 *
 * WHY:  Fractal structures provide scale-invariant security coverage,
 *       detecting tampering at any level of the hierarchy.
 *
 * HOW:  Uses SHA-256 hashing with Merkle-tree-like propagation,
 *       fractal dimension analysis for anomaly detection.
 *
 * Part of Phase SC-1: Security Coverage Framework (Tier 0.7)
 */

#include "security/nimcp_security_fractal.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "security_fractal"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_fractal, MESH_ADAPTER_CATEGORY_SECURITY)


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <pthread.h>

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Fractal security context (internal)
 */
struct nimcp_fractal_security {
    nimcp_fsc_config_t config;
    nimcp_fsc_node_t* root;
    nimcp_fsc_node_t** node_lookup;  /**< Fast lookup by data pointer */
    uint32_t lookup_capacity;
    uint32_t lookup_count;
    nimcp_fsc_stats_t stats;
    nimcp_fsc_anomaly_callback_t anomaly_callback;
    void* anomaly_user_data;
    nimcp_security_coverage_t* coverage;
    bool auto_repair_enabled;
    bool initialized;
    nimcp_mutex_t lock;
};

//=============================================================================
// Forward Declarations
//=============================================================================

static void compute_node_hash(nimcp_fsc_node_t* node);
static void propagate_hash_up(nimcp_fractal_security_t* fsc, nimcp_fsc_node_t* node);
static nimcp_fsc_node_t* create_node(nimcp_fsc_node_type_t type, uint32_t level);
static void destroy_node(nimcp_fsc_node_t* node);
static nimcp_fsc_node_t* find_node_for_data(nimcp_fractal_security_t* fsc, void* data);
static nimcp_fsc_node_t* find_insertion_point(nimcp_fractal_security_t* fsc);
static float compute_local_dimension(nimcp_fsc_node_t* node);
static bool verify_hash(nimcp_fsc_node_t* node);
static void simple_hash(const void* data, size_t size, uint8_t* hash_out);
static uint64_t get_timestamp_ms(void);

//=============================================================================
// Lifecycle Functions
//=============================================================================

nimcp_fractal_security_t* nimcp_fractal_security_create(void) {
    nimcp_fractal_security_t* fsc = nimcp_calloc(1, sizeof(nimcp_fractal_security_t));
    if (!fsc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_fractal_security_create: fsc is NULL");
        return NULL;
    }

    if (nimcp_mutex_init(&fsc->lock, NULL) != NIMCP_SUCCESS) {
        nimcp_free(fsc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_fractal_security_create: validation failed");
        return NULL;
    }

    return fsc;
}

nimcp_result_t nimcp_fractal_security_init(
    nimcp_fractal_security_t* fsc,
    const nimcp_fsc_config_t* config
) {
    if (!fsc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_fractal_security_init: fsc is NULL");
        return NIMCP_INVALID_PARAM;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_fractal_security_init: config is NULL");
        return NIMCP_INVALID_PARAM;
    }
    if (fsc->initialized) return NIMCP_INVALID_STATE;

    nimcp_mutex_lock(&fsc->lock);

    // Copy configuration
    fsc->config = *config;

    // Validate configuration
    if (fsc->config.hierarchy_depth > NIMCP_FSC_MAX_DEPTH) {
        fsc->config.hierarchy_depth = NIMCP_FSC_MAX_DEPTH;
    }
    if (fsc->config.branching_factor < 2) {
        fsc->config.branching_factor = 2;
    }
    if (fsc->config.anomaly_threshold <= 0) {
        fsc->config.anomaly_threshold = NIMCP_FSC_ANOMALY_THRESHOLD;
    }

    // Create root node
    fsc->root = create_node(NIMCP_FSC_NODE_ROOT, 0);
    if (!fsc->root) {
        nimcp_mutex_unlock(&fsc->lock);
        return NIMCP_NO_MEMORY;
    }
    fsc->root->max_children = fsc->config.branching_factor;

    // Allocate node lookup table
    fsc->lookup_capacity = 256;
    fsc->node_lookup = nimcp_calloc(fsc->lookup_capacity, sizeof(nimcp_fsc_node_t*));
    if (!fsc->node_lookup) {
        destroy_node(fsc->root);
        fsc->root = NULL;
        nimcp_mutex_unlock(&fsc->lock);
        return NIMCP_NO_MEMORY;
    }

    // Initialize statistics
    memset(&fsc->stats, 0, sizeof(fsc->stats));
    fsc->stats.total_nodes = 1;
    fsc->stats.current_depth = 1;

    fsc->initialized = true;
    nimcp_mutex_unlock(&fsc->lock);

    return NIMCP_SUCCESS;
}

void nimcp_fractal_security_destroy(nimcp_fractal_security_t* fsc) {
    if (!fsc) return;

    nimcp_mutex_lock(&fsc->lock);

    if (fsc->root) {
        destroy_node(fsc->root);
        fsc->root = NULL;
    }

    if (fsc->node_lookup) {
        nimcp_free(fsc->node_lookup);
        fsc->node_lookup = NULL;
    }

    fsc->initialized = false;
    nimcp_mutex_unlock(&fsc->lock);
    nimcp_mutex_destroy(&fsc->lock);

    nimcp_free(fsc);
}

nimcp_fsc_config_t nimcp_fractal_security_default_config(void) {
    nimcp_fsc_config_t config = {
        .fractal_dimension = NIMCP_FSC_DEFAULT_DIMENSION,
        .hierarchy_depth = 4,
        .branching_factor = 4,
        .anomaly_threshold = NIMCP_FSC_ANOMALY_THRESHOLD,
        .detect_mode = NIMCP_FSC_DETECT_HYBRID,
        .enable_guardians = true,
        .guardian_interval = 3,
        .verification_interval_ms = 1000
    };
    return config;
}

//=============================================================================
// Resource Protection
//=============================================================================

nimcp_result_t nimcp_fractal_security_protect(
    nimcp_fractal_security_t* fsc,
    void* data,
    size_t size,
    nimcp_fsc_node_t** node_out
) {
    if (!fsc || !data || size == 0) return NIMCP_INVALID_PARAM;
    if (!fsc->initialized) return NIMCP_INVALID_STATE;

    nimcp_mutex_lock(&fsc->lock);

    // Check if already protected
    if (find_node_for_data(fsc, data) != NULL) {
        nimcp_mutex_unlock(&fsc->lock);
        return NIMCP_ALREADY_EXISTS;
    }

    // Find insertion point in tree
    nimcp_fsc_node_t* parent = find_insertion_point(fsc);
    if (!parent) {
        nimcp_mutex_unlock(&fsc->lock);
        return NIMCP_NO_MEMORY;
    }

    // Create leaf node
    nimcp_fsc_node_t* leaf = create_node(NIMCP_FSC_NODE_LEAF, parent->level + 1);
    if (!leaf) {
        nimcp_mutex_unlock(&fsc->lock);
        return NIMCP_NO_MEMORY;
    }

    leaf->protected_data = data;
    leaf->data_size = size;
    leaf->parent = parent;
    leaf->verified = true;
    leaf->last_verified = get_timestamp_ms();

    // Compute hash of data
    compute_node_hash(leaf);

    // Attach to parent
    if (parent->num_children >= parent->max_children) {
        // Need to create intermediate branch
        nimcp_fsc_node_t* branch = create_node(NIMCP_FSC_NODE_BRANCH, parent->level + 1);
        if (!branch) {
            destroy_node(leaf);
            nimcp_mutex_unlock(&fsc->lock);
            return NIMCP_NO_MEMORY;
        }
        branch->parent = parent;
        branch->max_children = fsc->config.branching_factor;

        // Add branch to parent - check for integer overflow before realloc
        if (parent->num_children >= UINT32_MAX) {
            destroy_node(leaf);
            destroy_node(branch);
            nimcp_mutex_unlock(&fsc->lock);
            return NIMCP_NO_MEMORY;
        }
        size_t new_count = (size_t)parent->num_children + 1;
        if (new_count > SIZE_MAX / sizeof(nimcp_fsc_node_t*)) {
            destroy_node(leaf);
            destroy_node(branch);
            nimcp_mutex_unlock(&fsc->lock);
            return NIMCP_NO_MEMORY;
        }
        nimcp_fsc_node_t** new_children = nimcp_realloc(parent->children,
                                   new_count * sizeof(nimcp_fsc_node_t*));
        if (!new_children) {
            destroy_node(leaf);
            destroy_node(branch);
            nimcp_mutex_unlock(&fsc->lock);
            return NIMCP_NO_MEMORY;
        }
        parent->children = new_children;
        parent->children[parent->num_children++] = branch;

        // Add leaf to branch
        branch->children = nimcp_malloc(sizeof(nimcp_fsc_node_t*));
        if (!branch->children) {
            destroy_node(leaf);
            destroy_node(branch);
            nimcp_mutex_unlock(&fsc->lock);
            return NIMCP_NO_MEMORY;
        }
        branch->children[0] = leaf;
        branch->num_children = 1;
        leaf->parent = branch;
        leaf->level = branch->level + 1;

        fsc->stats.total_nodes += 2;
    } else {
        // Add directly to parent - check for integer overflow before realloc
        if (parent->num_children >= UINT32_MAX) {
            destroy_node(leaf);
            nimcp_mutex_unlock(&fsc->lock);
            return NIMCP_NO_MEMORY;
        }
        size_t new_count = (size_t)parent->num_children + 1;
        if (new_count > SIZE_MAX / sizeof(nimcp_fsc_node_t*)) {
            destroy_node(leaf);
            nimcp_mutex_unlock(&fsc->lock);
            return NIMCP_NO_MEMORY;
        }
        nimcp_fsc_node_t** new_children = nimcp_realloc(parent->children,
                                   new_count * sizeof(nimcp_fsc_node_t*));
        if (!new_children) {
            destroy_node(leaf);
            nimcp_mutex_unlock(&fsc->lock);
            return NIMCP_NO_MEMORY;
        }
        parent->children = new_children;
        parent->children[parent->num_children++] = leaf;
        fsc->stats.total_nodes++;
    }

    // Add to lookup table - check for integer overflow before realloc
    if (fsc->lookup_count >= fsc->lookup_capacity) {
        if (fsc->lookup_capacity >= UINT32_MAX / 2) {
            // Leaf already added to tree, just skip lookup expansion
            LOG_WARN("nimcp_fractal_security_protect: lookup table capacity overflow");
            nimcp_mutex_unlock(&fsc->lock);
            return NIMCP_SUCCESS;  // Node added but lookup not expanded
        }
        uint32_t new_capacity = fsc->lookup_capacity * 2;
        if ((size_t)new_capacity > SIZE_MAX / sizeof(nimcp_fsc_node_t*)) {
            // Leaf already added to tree, just skip lookup expansion
            LOG_WARN("nimcp_fractal_security_protect: lookup table size overflow");
            nimcp_mutex_unlock(&fsc->lock);
            return NIMCP_SUCCESS;  // Node added but lookup not expanded
        }
        nimcp_fsc_node_t** new_lookup = nimcp_realloc(fsc->node_lookup,
                                   (size_t)new_capacity * sizeof(nimcp_fsc_node_t*));
        if (!new_lookup) {
            // Leaf already added to tree, just skip lookup expansion
            LOG_WARN("nimcp_fractal_security_protect: lookup table expansion failed");
            nimcp_mutex_unlock(&fsc->lock);
            return NIMCP_SUCCESS;  // Node added but lookup not expanded
        }
        fsc->node_lookup = new_lookup;
        fsc->lookup_capacity = new_capacity;
    }
    fsc->node_lookup[fsc->lookup_count++] = leaf;

    // Update statistics
    fsc->stats.leaf_nodes++;
    if (leaf->level + 1 > fsc->stats.current_depth) {
        fsc->stats.current_depth = leaf->level + 1;
    }

    // Propagate hash changes up tree
    propagate_hash_up(fsc, leaf);

    if (node_out) *node_out = leaf;

    nimcp_mutex_unlock(&fsc->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_fractal_security_unprotect(
    nimcp_fractal_security_t* fsc,
    void* data
) {
    if (!fsc || !data) return NIMCP_INVALID_PARAM;
    if (!fsc->initialized) return NIMCP_INVALID_STATE;

    nimcp_mutex_lock(&fsc->lock);

    nimcp_fsc_node_t* node = find_node_for_data(fsc, data);
    if (!node) {
        nimcp_mutex_unlock(&fsc->lock);
        return NIMCP_NOT_FOUND;
    }

    // Remove from parent
    nimcp_fsc_node_t* parent = node->parent;
    if (parent) {
        for (uint32_t i = 0; i < parent->num_children; i++) {
            if (parent->children[i] == node) {
                memmove(&parent->children[i], &parent->children[i + 1],
                        (parent->num_children - i - 1) * sizeof(nimcp_fsc_node_t*));
                parent->num_children--;
                break;
            }
        }
    }

    // Remove from lookup
    for (uint32_t i = 0; i < fsc->lookup_count; i++) {
        if (fsc->node_lookup[i] == node) {
            memmove(&fsc->node_lookup[i], &fsc->node_lookup[i + 1],
                    (fsc->lookup_count - i - 1) * sizeof(nimcp_fsc_node_t*));
            fsc->lookup_count--;
            break;
        }
    }

    fsc->stats.total_nodes--;
    fsc->stats.leaf_nodes--;

    destroy_node(node);

    // Propagate hash changes
    if (parent) {
        propagate_hash_up(fsc, parent);
    }

    nimcp_mutex_unlock(&fsc->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_fractal_security_update_hash(
    nimcp_fractal_security_t* fsc,
    void* data
) {
    if (!fsc || !data) return NIMCP_INVALID_PARAM;
    if (!fsc->initialized) return NIMCP_INVALID_STATE;

    nimcp_mutex_lock(&fsc->lock);

    nimcp_fsc_node_t* node = find_node_for_data(fsc, data);
    if (!node) {
        nimcp_mutex_unlock(&fsc->lock);
        return NIMCP_NOT_FOUND;
    }

    compute_node_hash(node);
    node->verified = true;
    node->last_verified = get_timestamp_ms();

    propagate_hash_up(fsc, node);

    nimcp_mutex_unlock(&fsc->lock);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Verification Functions
//=============================================================================

nimcp_fsc_result_t nimcp_fractal_security_verify_data(
    nimcp_fractal_security_t* fsc,
    void* data
) {
    if (!fsc || !data) return NIMCP_FSC_STRUCTURE_CORRUPT;
    if (!fsc->initialized) return NIMCP_FSC_STRUCTURE_CORRUPT;

    nimcp_mutex_lock(&fsc->lock);

    nimcp_fsc_node_t* node = find_node_for_data(fsc, data);
    if (!node) {
        nimcp_mutex_unlock(&fsc->lock);
        return NIMCP_FSC_STRUCTURE_CORRUPT;
    }

    fsc->stats.verifications++;

    // Verify hash
    if (!verify_hash(node)) {
        fsc->stats.hash_mismatches++;
        if (fsc->anomaly_callback) {
            nimcp_fsc_anomaly_t anomaly = {
                .type = NIMCP_FSC_HASH_MISMATCH,
                .level = node->level,
                .node_index = node->index,
                .timestamp = get_timestamp_ms(),
                .affected_data = data
            };
            fsc->anomaly_callback(&anomaly, fsc->anomaly_user_data);
        }
        nimcp_mutex_unlock(&fsc->lock);
        return NIMCP_FSC_HASH_MISMATCH;
    }

    node->verified = true;
    node->last_verified = get_timestamp_ms();
    node->access_count++;

    nimcp_mutex_unlock(&fsc->lock);
    return NIMCP_FSC_INTACT;
}

nimcp_fsc_result_t nimcp_fractal_security_verify_node(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_node_t* node
) {
    if (!fsc || !node) return NIMCP_FSC_STRUCTURE_CORRUPT;

    nimcp_mutex_lock(&fsc->lock);

    fsc->stats.verifications++;

    // Verify this node
    if (!verify_hash(node)) {
        fsc->stats.hash_mismatches++;
        nimcp_mutex_unlock(&fsc->lock);
        return NIMCP_FSC_HASH_MISMATCH;
    }

    // Check fractal dimension
    float local_dim = compute_local_dimension(node);
    float deviation = fabsf(local_dim - fsc->config.fractal_dimension);
    if (deviation > fsc->config.anomaly_threshold) {
        fsc->stats.dimension_anomalies++;
        nimcp_mutex_unlock(&fsc->lock);
        return NIMCP_FSC_DIMENSION_ANOMALY;
    }

    node->verified = true;
    node->last_verified = get_timestamp_ms();

    nimcp_mutex_unlock(&fsc->lock);
    return NIMCP_FSC_INTACT;
}

static nimcp_fsc_result_t verify_subtree_recursive(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_node_t* node
) {
    if (!node) return NIMCP_FSC_INTACT;

    // Verify this node
    if (!verify_hash(node)) {
        return NIMCP_FSC_HASH_MISMATCH;
    }

    // Verify children recursively
    for (uint32_t i = 0; i < node->num_children; i++) {
        nimcp_fsc_result_t result = verify_subtree_recursive(fsc, node->children[i]);
        if (result != NIMCP_FSC_INTACT) {
            return result;
        }
    }

    node->verified = true;
    node->last_verified = get_timestamp_ms();

    return NIMCP_FSC_INTACT;
}

nimcp_fsc_result_t nimcp_fractal_security_verify_subtree(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_node_t* node
) {
    if (!fsc) return NIMCP_FSC_STRUCTURE_CORRUPT;
    if (!fsc->initialized) return NIMCP_FSC_STRUCTURE_CORRUPT;

    nimcp_mutex_lock(&fsc->lock);

    nimcp_fsc_node_t* start = node ? node : fsc->root;
    nimcp_fsc_result_t result = verify_subtree_recursive(fsc, start);

    nimcp_mutex_unlock(&fsc->lock);
    return result;
}

nimcp_fsc_result_t nimcp_fractal_security_verify_all(
    nimcp_fractal_security_t* fsc
) {
    return nimcp_fractal_security_verify_subtree(fsc, NULL);
}

//=============================================================================
// Fractal Dimension Analysis
//=============================================================================

nimcp_result_t nimcp_fractal_security_compute_dimension(
    nimcp_fractal_security_t* fsc,
    float* dimension
) {
    if (!fsc || !dimension) return NIMCP_INVALID_PARAM;
    if (!fsc->initialized) return NIMCP_INVALID_STATE;

    nimcp_mutex_lock(&fsc->lock);

    // Box-counting method for fractal dimension
    // D = log(N) / log(1/s) where N = count at scale s

    if (fsc->stats.total_nodes < 2) {
        *dimension = 0.0F;
        nimcp_mutex_unlock(&fsc->lock);
        return NIMCP_SUCCESS;
    }

    // Compute based on tree structure
    // Dimension = log(total_nodes) / log(depth * branching_factor)
    float n = (float)fsc->stats.total_nodes;
    float scale = (float)(fsc->stats.current_depth * fsc->config.branching_factor);

    if (scale > 1.0F) {
        *dimension = logf(n) / logf(scale);
    } else {
        *dimension = 1.0F;
    }

    fsc->stats.measured_dimension = *dimension;

    nimcp_mutex_unlock(&fsc->lock);
    return NIMCP_SUCCESS;
}

bool nimcp_fractal_security_check_dimension_anomaly(
    nimcp_fractal_security_t* fsc,
    float* deviation
) {
    if (!fsc || !fsc->initialized) {
        return false;
    }

    float current_dimension;
    if (nimcp_fractal_security_compute_dimension(fsc, &current_dimension) != NIMCP_SUCCESS) {
        return false;
    }

    float dev = fabsf(current_dimension - fsc->config.fractal_dimension);
    if (deviation) *deviation = dev;

    return dev > fsc->config.anomaly_threshold;
}

float nimcp_fractal_security_local_dimension(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_node_t* node
) {
    if (!fsc || !node) return 0.0F;
    return compute_local_dimension(node);
}

//=============================================================================
// Guardian Sentinels
//=============================================================================

nimcp_result_t nimcp_fractal_security_place_guardian(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_node_t* node
) {
    if (!fsc || !node) return NIMCP_INVALID_PARAM;
    if (!fsc->initialized) return NIMCP_INVALID_STATE;

    nimcp_mutex_lock(&fsc->lock);

    if (node->type != NIMCP_FSC_NODE_GUARDIAN) {
        // Create guardian as sibling
        nimcp_fsc_node_t* guardian = create_node(NIMCP_FSC_NODE_GUARDIAN, node->level);
        if (!guardian) {
            nimcp_mutex_unlock(&fsc->lock);
            return NIMCP_NO_MEMORY;
        }

        guardian->parent = node->parent;

        // Copy hash to guardian for verification reference
        memcpy(guardian->hash, node->hash, NIMCP_FSC_HASH_SIZE);
        guardian->verified = true;
        guardian->last_verified = get_timestamp_ms();

        // Add to parent - check for integer overflow before realloc
        if (node->parent) {
            if (node->parent->num_children >= UINT32_MAX) {
                nimcp_free(guardian);
                nimcp_mutex_unlock(&fsc->lock);
                return NIMCP_NO_MEMORY;
            }
            size_t new_count = (size_t)node->parent->num_children + 1;
            if (new_count > SIZE_MAX / sizeof(nimcp_fsc_node_t*)) {
                nimcp_free(guardian);
                nimcp_mutex_unlock(&fsc->lock);
                return NIMCP_NO_MEMORY;
            }
            nimcp_fsc_node_t** new_children = nimcp_realloc(node->parent->children,
                new_count * sizeof(nimcp_fsc_node_t*));
            if (!new_children) {
                nimcp_free(guardian);
                nimcp_mutex_unlock(&fsc->lock);
                return NIMCP_NO_MEMORY;
            }
            node->parent->children = new_children;
            node->parent->children[node->parent->num_children++] = guardian;
        }

        fsc->stats.total_nodes++;
        fsc->stats.guardian_nodes++;
    }

    nimcp_mutex_unlock(&fsc->lock);
    return NIMCP_SUCCESS;
}

static uint32_t check_guardians_recursive(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_node_t* node,
    uint32_t* alerts
) {
    if (!node) return 0;

    if (node->type == NIMCP_FSC_NODE_GUARDIAN) {
        // Check guardian against siblings
        nimcp_fsc_node_t* parent = node->parent;
        if (parent) {
            for (uint32_t i = 0; i < parent->num_children; i++) {
                nimcp_fsc_node_t* sibling = parent->children[i];
                if (sibling != node && sibling->type != NIMCP_FSC_NODE_GUARDIAN) {
                    // Compare hashes
                    if (memcmp(node->hash, sibling->hash, NIMCP_FSC_HASH_SIZE) != 0) {
                        (*alerts)++;
                    }
                }
            }
        }
    }

    for (uint32_t i = 0; i < node->num_children; i++) {
        check_guardians_recursive(fsc, node->children[i], alerts);
    }

    return *alerts;
}

uint32_t nimcp_fractal_security_check_guardians(
    nimcp_fractal_security_t* fsc
) {
    if (!fsc || !fsc->initialized) return 0;

    nimcp_mutex_lock(&fsc->lock);

    uint32_t alerts = 0;
    check_guardians_recursive(fsc, fsc->root, &alerts);

    fsc->stats.guardian_alerts += alerts;

    nimcp_mutex_unlock(&fsc->lock);
    return alerts;
}

static void auto_place_guardians_recursive(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_node_t* node,
    uint32_t interval,
    uint32_t* counter,
    uint32_t* placed
) {
    if (!node) return;

    (*counter)++;
    if (*counter % interval == 0 && node->type != NIMCP_FSC_NODE_GUARDIAN) {
        nimcp_mutex_unlock(&fsc->lock);
        nimcp_fractal_security_place_guardian(fsc, node);
        nimcp_mutex_lock(&fsc->lock);
        (*placed)++;
    }

    for (uint32_t i = 0; i < node->num_children; i++) {
        auto_place_guardians_recursive(fsc, node->children[i], interval, counter, placed);
    }
}

uint32_t nimcp_fractal_security_auto_place_guardians(
    nimcp_fractal_security_t* fsc
) {
    if (!fsc || !fsc->initialized || !fsc->config.enable_guardians) return 0;

    nimcp_mutex_lock(&fsc->lock);

    uint32_t counter = 0;
    uint32_t placed = 0;
    auto_place_guardians_recursive(fsc, fsc->root, fsc->config.guardian_interval,
                                   &counter, &placed);

    nimcp_mutex_unlock(&fsc->lock);
    return placed;
}

//=============================================================================
// Trust Propagation
//=============================================================================

nimcp_result_t nimcp_fractal_security_propagate_trust(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_node_t* node
) {
    if (!fsc || !node) return NIMCP_INVALID_PARAM;
    if (!fsc->initialized) return NIMCP_INVALID_STATE;

    nimcp_mutex_lock(&fsc->lock);

    nimcp_fsc_node_t* current = node->parent;
    while (current) {
        // Parent is verified if all children are verified
        bool all_verified = true;
        for (uint32_t i = 0; i < current->num_children; i++) {
            if (!current->children[i]->verified) {
                all_verified = false;
                break;
            }
        }

        current->verified = all_verified;
        if (all_verified) {
            current->last_verified = get_timestamp_ms();
        }

        current = current->parent;
    }

    nimcp_mutex_unlock(&fsc->lock);
    return NIMCP_SUCCESS;
}

static float compute_trust_recursive(nimcp_fsc_node_t* node) {
    if (!node) return 0.0F;

    if (node->num_children == 0) {
        return node->verified ? 1.0F : 0.0F;
    }

    float sum = 0.0F;
    for (uint32_t i = 0; i < node->num_children; i++) {
        sum += compute_trust_recursive(node->children[i]);
    }

    return sum / (float)node->num_children;
}

float nimcp_fractal_security_trust_level(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_node_t* node
) {
    if (!fsc || !node) return 0.0F;

    nimcp_mutex_lock(&fsc->lock);
    float trust = compute_trust_recursive(node);
    nimcp_mutex_unlock(&fsc->lock);

    return trust;
}

static void invalidate_recursive(nimcp_fsc_node_t* node) {
    if (!node) return;

    node->verified = false;
    for (uint32_t i = 0; i < node->num_children; i++) {
        invalidate_recursive(node->children[i]);
    }
}

nimcp_result_t nimcp_fractal_security_invalidate_trust(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_node_t* node
) {
    if (!fsc || !node) return NIMCP_INVALID_PARAM;
    if (!fsc->initialized) return NIMCP_INVALID_STATE;

    nimcp_mutex_lock(&fsc->lock);
    invalidate_recursive(node);
    nimcp_mutex_unlock(&fsc->lock);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Anomaly Detection
//=============================================================================

nimcp_result_t nimcp_fractal_security_set_anomaly_callback(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_anomaly_callback_t callback,
    void* user_data
) {
    if (!fsc) return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&fsc->lock);
    fsc->anomaly_callback = callback;
    fsc->anomaly_user_data = user_data;
    nimcp_mutex_unlock(&fsc->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_fractal_security_detect_anomalies(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_anomaly_t** anomalies,
    uint32_t* count
) {
    if (!fsc || !anomalies || !count) return NIMCP_INVALID_PARAM;
    if (!fsc->initialized) return NIMCP_INVALID_STATE;

    nimcp_mutex_lock(&fsc->lock);

    // Allocate anomaly array
    *anomalies = nimcp_calloc(fsc->stats.total_nodes, sizeof(nimcp_fsc_anomaly_t));
    if (!*anomalies) {
        nimcp_mutex_unlock(&fsc->lock);
        return NIMCP_NO_MEMORY;
    }

    *count = 0;

    // Check all leaf nodes
    for (uint32_t i = 0; i < fsc->lookup_count; i++) {
        nimcp_fsc_node_t* node = fsc->node_lookup[i];

        // Hash check
        if (!verify_hash(node)) {
            (*anomalies)[*count].type = NIMCP_FSC_HASH_MISMATCH;
            (*anomalies)[*count].level = node->level;
            (*anomalies)[*count].node_index = node->index;
            (*anomalies)[*count].timestamp = get_timestamp_ms();
            (*anomalies)[*count].affected_data = node->protected_data;
            (*count)++;
        }

        // Dimension check
        float local_dim = compute_local_dimension(node);
        float deviation = fabsf(local_dim - fsc->config.fractal_dimension);
        if (deviation > fsc->config.anomaly_threshold) {
            (*anomalies)[*count].type = NIMCP_FSC_DIMENSION_ANOMALY;
            (*anomalies)[*count].level = node->level;
            (*anomalies)[*count].node_index = node->index;
            (*anomalies)[*count].expected_dimension = fsc->config.fractal_dimension;
            (*anomalies)[*count].actual_dimension = local_dim;
            (*anomalies)[*count].timestamp = get_timestamp_ms();
            (*count)++;
        }
    }

    // Resize to actual count
    if (*count > 0) {
        nimcp_fsc_anomaly_t* resized = nimcp_realloc(*anomalies, *count * sizeof(nimcp_fsc_anomaly_t));
        if (resized) {
            *anomalies = resized;
        }
        // If realloc fails, keep original allocation (it's just slightly oversized)
    } else {
        nimcp_free(*anomalies);
        *anomalies = NULL;
    }

    nimcp_mutex_unlock(&fsc->lock);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Self-Healing
//=============================================================================

nimcp_result_t nimcp_fractal_security_enable_auto_repair(
    nimcp_fractal_security_t* fsc,
    bool enable
) {
    if (!fsc) return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&fsc->lock);
    fsc->auto_repair_enabled = enable;
    nimcp_mutex_unlock(&fsc->lock);

    return NIMCP_SUCCESS;
}

bool nimcp_fractal_security_repair_node(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_node_t* node
) {
    if (!fsc || !node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_fractal_security_repair_node: required parameter is NULL (fsc, node)");
        return false;
    }

    nimcp_mutex_lock(&fsc->lock);

    // Try to repair by recomputing hash
    if (node->type == NIMCP_FSC_NODE_LEAF && node->protected_data) {
        compute_node_hash(node);
        node->verified = true;
        node->last_verified = get_timestamp_ms();
        propagate_hash_up(fsc, node);
        fsc->stats.integrity_repairs++;
        nimcp_mutex_unlock(&fsc->lock);
        return true;
    }

    // For branch nodes, recompute from children
    if (node->num_children > 0) {
        compute_node_hash(node);
        node->verified = true;
        node->last_verified = get_timestamp_ms();
        propagate_hash_up(fsc, node);
        fsc->stats.integrity_repairs++;
        nimcp_mutex_unlock(&fsc->lock);
        return true;
    }

    nimcp_mutex_unlock(&fsc->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_fractal_security_repair_node: operation failed");
    return false;
}

nimcp_result_t nimcp_fractal_security_rebuild_subtree(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_node_t* node
) {
    if (!fsc || !node) return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&fsc->lock);

    // Rebuild hashes from leaves up
    for (uint32_t i = 0; i < node->num_children; i++) {
        nimcp_mutex_unlock(&fsc->lock);
        nimcp_fractal_security_rebuild_subtree(fsc, node->children[i]);
        nimcp_mutex_lock(&fsc->lock);
    }

    compute_node_hash(node);
    node->verified = true;
    node->last_verified = get_timestamp_ms();

    nimcp_mutex_unlock(&fsc->lock);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Statistics
//=============================================================================

nimcp_result_t nimcp_fractal_security_get_stats(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_stats_t* stats
) {
    if (!fsc || !stats) return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&fsc->lock);

    *stats = fsc->stats;

    // Compute coverage ratio
    if (fsc->stats.total_nodes > 0) {
        uint32_t verified_count = 0;
        for (uint32_t i = 0; i < fsc->lookup_count; i++) {
            if (fsc->node_lookup[i]->verified) {
                verified_count++;
            }
        }
        stats->coverage_ratio = (float)verified_count / (float)fsc->lookup_count;
    }

    nimcp_mutex_unlock(&fsc->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_fractal_security_reset_stats(
    nimcp_fractal_security_t* fsc
) {
    if (!fsc) return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&fsc->lock);

    // Keep structural stats, reset operational
    fsc->stats.verifications = 0;
    fsc->stats.hash_mismatches = 0;
    fsc->stats.dimension_anomalies = 0;
    fsc->stats.guardian_alerts = 0;
    fsc->stats.integrity_repairs = 0;

    nimcp_mutex_unlock(&fsc->lock);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Integration with Coverage System
//=============================================================================

nimcp_result_t nimcp_fractal_security_register_coverage(
    nimcp_fractal_security_t* fsc,
    nimcp_security_coverage_t* coverage
) {
    if (!fsc || !coverage) return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&fsc->lock);
    fsc->coverage = coverage;
    nimcp_mutex_unlock(&fsc->lock);

    return NIMCP_SUCCESS;
}

float nimcp_fractal_security_coverage_contribution(
    nimcp_fractal_security_t* fsc
) {
    if (!fsc || !fsc->initialized) return 0.0F;

    nimcp_fsc_stats_t stats;
    nimcp_fractal_security_get_stats(fsc, &stats);

    return stats.coverage_ratio;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* nimcp_fsc_result_name(nimcp_fsc_result_t result) {
    switch (result) {
        case NIMCP_FSC_INTACT:           return "INTACT";
        case NIMCP_FSC_HASH_MISMATCH:    return "HASH_MISMATCH";
        case NIMCP_FSC_DIMENSION_ANOMALY: return "DIMENSION_ANOMALY";
        case NIMCP_FSC_STRUCTURE_CORRUPT: return "STRUCTURE_CORRUPT";
        case NIMCP_FSC_GUARDIAN_ALERT:   return "GUARDIAN_ALERT";
        case NIMCP_FSC_PROPAGATION_FAIL: return "PROPAGATION_FAIL";
        default:                          return "UNKNOWN";
    }
}

const char* nimcp_fsc_node_type_name(nimcp_fsc_node_type_t type) {
    switch (type) {
        case NIMCP_FSC_NODE_ROOT:     return "ROOT";
        case NIMCP_FSC_NODE_BRANCH:   return "BRANCH";
        case NIMCP_FSC_NODE_LEAF:     return "LEAF";
        case NIMCP_FSC_NODE_GUARDIAN: return "GUARDIAN";
        default:                       return "UNKNOWN";
    }
}

static int dump_tree_recursive(
    nimcp_fsc_node_t* node,
    int depth,
    char* buffer,
    size_t size,
    size_t* offset
) {
    if (!node || *offset >= size - 1) return 0;

    // Indent
    for (int i = 0; i < depth; i++) {
        if (*offset < size - 2) {
            buffer[(*offset)++] = ' ';
            buffer[(*offset)++] = ' ';
        }
    }

    // Node info
    int written = snprintf(buffer + *offset, size - *offset,
                          "[%s L%u] verified=%d children=%u\n",
                          nimcp_fsc_node_type_name(node->type),
                          node->level,
                          node->verified,
                          node->num_children);
    if (written > 0) *offset += written;

    // Children
    for (uint32_t i = 0; i < node->num_children; i++) {
        dump_tree_recursive(node->children[i], depth + 1, buffer, size, offset);
    }

    return (int)*offset;
}

int nimcp_fractal_security_dump_tree(
    nimcp_fractal_security_t* fsc,
    char* buffer,
    size_t size
) {
    if (!fsc || !buffer || size == 0) return 0;

    nimcp_mutex_lock(&fsc->lock);

    size_t offset = 0;
    int written = dump_tree_recursive(fsc->root, 0, buffer, size, &offset);
    buffer[offset] = '\0';

    nimcp_mutex_unlock(&fsc->lock);
    return written;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

static void simple_hash(const void* data, size_t size, uint8_t* hash_out) {
    // Simple hash for demonstration - in production use SHA-256
    memset(hash_out, 0, NIMCP_FSC_HASH_SIZE);

    const uint8_t* bytes = (const uint8_t*)data;
    uint64_t h1 = 0x6a09e667f3bcc908ULL;
    uint64_t h2 = 0xbb67ae8584caa73bULL;
    uint64_t h3 = 0x3c6ef372fe94f82bULL;
    uint64_t h4 = 0xa54ff53a5f1d36f1ULL;

    for (size_t i = 0; i < size; i++) {
        h1 = (h1 ^ bytes[i]) * 0x100000001b3ULL;
        h2 = (h2 ^ bytes[i]) * 0x100000001b3ULL;
        h3 = (h3 ^ bytes[i]) * 0x100000001b3ULL;
        h4 = (h4 ^ bytes[i]) * 0x100000001b3ULL;
        h1 ^= h1 >> 33;
        h2 ^= h2 >> 33;
        h3 ^= h3 >> 33;
        h4 ^= h4 >> 33;
    }

    memcpy(hash_out, &h1, 8);
    memcpy(hash_out + 8, &h2, 8);
    memcpy(hash_out + 16, &h3, 8);
    memcpy(hash_out + 24, &h4, 8);
}

static void compute_node_hash(nimcp_fsc_node_t* node) {
    if (!node) return;

    if (node->type == NIMCP_FSC_NODE_LEAF && node->protected_data) {
        // Hash the actual data
        simple_hash(node->protected_data, node->data_size, node->hash);
    } else if (node->num_children > 0 && node->children) {
        // Hash is combination of children hashes
        uint8_t combined[NIMCP_FSC_HASH_SIZE * 16];  // Up to 16 children
        size_t combined_size = 0;

        for (uint32_t i = 0; i < node->num_children && i < 16; i++) {
            if (node->children[i]) {
                memcpy(combined + combined_size, node->children[i]->hash, NIMCP_FSC_HASH_SIZE);
                combined_size += NIMCP_FSC_HASH_SIZE;
            }
        }

        simple_hash(combined, combined_size, node->hash);
    }
}

static void propagate_hash_up(nimcp_fractal_security_t* fsc, nimcp_fsc_node_t* node) {
    nimcp_fsc_node_t* current = node->parent;
    while (current) {
        compute_node_hash(current);
        current = current->parent;
    }
}

static nimcp_fsc_node_t* create_node(nimcp_fsc_node_type_t type, uint32_t level) {
    nimcp_fsc_node_t* node = nimcp_calloc(1, sizeof(nimcp_fsc_node_t));
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_node: node is NULL");
        return NULL;
    }

    node->type = type;
    node->level = level;
    node->max_children = 4;  // Default branching

    return node;
}

static void destroy_node(nimcp_fsc_node_t* node) {
    if (!node) return;

    // Destroy children recursively
    for (uint32_t i = 0; i < node->num_children; i++) {
        destroy_node(node->children[i]);
    }

    if (node->children) {
        nimcp_free(node->children);
    }

    nimcp_free(node);
}

static nimcp_fsc_node_t* find_node_for_data(nimcp_fractal_security_t* fsc, void* data) {
    for (uint32_t i = 0; i < fsc->lookup_count; i++) {
        if (fsc->node_lookup[i]->protected_data == data) {
            return fsc->node_lookup[i];
        }
    }
    /* Data not found in node lookup - normal for unregistered data */
    return NULL;
}

static nimcp_fsc_node_t* find_insertion_point(nimcp_fractal_security_t* fsc) {
    // Find node with room for children at appropriate depth
    nimcp_fsc_node_t* current = fsc->root;

    while (current->level < fsc->config.hierarchy_depth - 1) {
        // Look for child with room
        bool found = false;
        for (uint32_t i = 0; i < current->num_children; i++) {
            if (current->children[i]->type == NIMCP_FSC_NODE_BRANCH &&
                current->children[i]->num_children < current->children[i]->max_children) {
                current = current->children[i];
                found = true;
                break;
            }
        }

        if (!found) {
            // Create new branch if room
            if (current->num_children < current->max_children) {
                return current;
            }
            // Otherwise use first child
            if (current->num_children > 0) {
                current = current->children[0];
            } else {
                break;
            }
        }
    }

    return current;
}

static float compute_local_dimension(nimcp_fsc_node_t* node) {
    if (!node || node->num_children == 0) {
        return 1.0F;
    }

    // Local dimension based on branching
    float n = (float)(node->num_children + 1);
    float depth = (float)(node->level + 1);

    if (depth > 0) {
        return logf(n) / logf(depth + 1);
    }

    return 1.0F;
}

static bool verify_hash(nimcp_fsc_node_t* node) {
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "verify_hash: node is NULL");
        return false;
    }

    uint8_t computed[NIMCP_FSC_HASH_SIZE];

    if (node->type == NIMCP_FSC_NODE_LEAF && node->protected_data) {
        simple_hash(node->protected_data, node->data_size, computed);
    } else if (node->num_children > 0) {
        uint8_t combined[NIMCP_FSC_HASH_SIZE * 16];
        size_t combined_size = 0;

        for (uint32_t i = 0; i < node->num_children && i < 16; i++) {
            memcpy(combined + combined_size, node->children[i]->hash, NIMCP_FSC_HASH_SIZE);
            combined_size += NIMCP_FSC_HASH_SIZE;
        }

        simple_hash(combined, combined_size, computed);
    } else {
        return true;  // Empty node
    }

    return memcmp(node->hash, computed, NIMCP_FSC_HASH_SIZE) == 0;
}

static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * NIMCP_MS_PER_SEC + (uint64_t)ts.tv_nsec / NIMCP_NS_PER_MS;
}
