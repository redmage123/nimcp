/**
 * @file nimcp_collective_workspace.c
 * @brief Implementation of Collective Workspace for drone swarms
 *
 * WHAT: CRDT-based distributed workspace for swarm-wide attention and coordination
 * WHY:  Enable emergent collective cognition through shared situational awareness
 * HOW:  Vector clock causality tracking + salience-based conflict resolution + broadcast policies
 *
 * IMPLEMENTATION NOTES:
 * - CRDT merge using vector clock comparison + salience tiebreaker
 * - Items sorted by salience (descending) for fast top-K access
 * - Coherence computed as average pairwise similarity of top items
 * - Pruning uses age + salience criteria
 * - Mutex protects all workspace operations (thread-safe)
 * - BBB security validation on all external inputs
 * - Bio-async integration for distributed message passing
 *
 * PERFORMANCE:
 * - Add item: O(N) where N = item_count (find insertion point)
 * - Merge item: O(N) for search + O(1) for merge
 * - Get top items: O(K) (items already sorted)
 * - Prune: O(N) scan + compact
 * - Coherence: O(N × D) where D = content_dim
 *
 * MEMORY:
 * - Workspace: ~200 bytes base
 * - Items: 32 × ~200 bytes = ~6.4 KB
 * - Total: ~7 KB per workspace (very lightweight)
 *
 * SECURITY:
 * - BBB validation on all pointer parameters
 * - BBB audit logging for significant operations
 * - BBB network data validation for received items
 *
 * BIO-ASYNC:
 * - Optional bio-async context for message-driven updates
 * - Inbox processing for distributed synchronization
 * - Message handlers for workspace item propagation
 *
 * @author NIMCP Swarm Intelligence Team
 * @date 2025-12-08
 * @version 2.0.0
 */

#include "swarm/nimcp_collective_workspace.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_security.h"
#include "security/nimcp_bbb_helpers.h"
#include "api/nimcp_api_exception.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(collective_workspace)

//=============================================================================
// Module Constants
//=============================================================================

#define MODULE_NAME "collective_workspace"

// Message types for bio-async integration
#define MSG_TYPE_WORKSPACE_ITEM_ADD      0x1001
#define MSG_TYPE_WORKSPACE_ITEM_MERGE    0x1002
#define MSG_TYPE_WORKSPACE_BROADCAST     0x1003
#define MSG_TYPE_WORKSPACE_PRUNE_REQUEST 0x1004

// Validation constants
#define MAX_CONTENT_VALUE 1000.0f  // Maximum reasonable content value
#define MAX_VECTOR_CLOCK_VALUE (1ULL << 48)  // Max reasonable vector clock value

//=============================================================================
// Logging Macros
//=============================================================================

/**
 * WHAT: Enhanced logging macros that use nimcp_log API
 * WHY:  Consistent logging across NIMCP modules with proper categorization
 * HOW:  Wrap nimcp_log calls with appropriate log levels
 */
#undef LOG_ERROR
#undef LOG_WARN
#undef LOG_INFO
#undef LOG_DEBUG
#define LOG_ERROR(fmt, ...)   nimcp_log(LOG_LEVEL_ERROR, MODULE_NAME, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)    nimcp_log(LOG_LEVEL_WARN, MODULE_NAME, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)    nimcp_log(LOG_LEVEL_INFO, MODULE_NAME, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)   nimcp_log(LOG_LEVEL_DEBUG, MODULE_NAME, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds
 *
 * WHAT: System monotonic time in milliseconds
 * WHY:  Needed for timestamps, age calculations, and pruning
 * HOW:  Use clock_gettime(CLOCK_MONOTONIC) for monotonic clock
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Compare vector clocks for causality
 *
 * WHAT: Determine causal relationship between two vector clocks
 * WHY:  CRDT merge requires understanding which event happened first
 * HOW:  Element-wise comparison using happened-before relation
 *
 * ALGORITHM:
 * - If all A[i] <= B[i] and some A[j] < B[j]: A happened-before B (return -1)
 * - If all B[i] <= A[i] and some B[j] < A[j]: B happened-before A (return 1)
 * - If all A[i] = B[i]: Same event (return 0)
 * - Otherwise: Concurrent (return 2)
 *
 * @param clock_a First vector clock
 * @param clock_b Second vector clock
 * @param swarm_size Number of drones (vector clock dimension)
 * @return -1 (a < b), 0 (a = b), 1 (a > b), 2 (concurrent)
 */
static int compare_vector_clocks(
    const uint64_t* clock_a,
    const uint64_t* clock_b,
    uint16_t swarm_size
) {
    bool a_less_or_equal = true;
    bool b_less_or_equal = true;
    bool equal = true;

    for (uint16_t i = 0; i < swarm_size; i++) {
        if (clock_a[i] > clock_b[i]) {
            b_less_or_equal = false;
            equal = false;
        }
        if (clock_b[i] > clock_a[i]) {
            a_less_or_equal = false;
            equal = false;
        }
    }

    if (equal) {
        return 0;  // Same event
    } else if (a_less_or_equal) {
        return -1; // a happened-before b
    } else if (b_less_or_equal) {
        return 1;  // b happened-before a
    } else {
        return 2;  // Concurrent
    }
}

/**
 * @brief Merge two vector clocks (element-wise max)
 *
 * WHAT: Combine two vector clocks into one
 * WHY:  CRDT merge requires taking maximum of each dimension
 * HOW:  For each dimension i: dest[i] = max(dest[i], source[i])
 *
 * @param dest Destination vector clock (modified in-place)
 * @param source Source vector clock
 * @param swarm_size Number of drones
 */
static void merge_vector_clocks(
    uint64_t* dest,
    const uint64_t* source,
    uint16_t swarm_size
) {
    for (uint16_t i = 0; i < swarm_size; i++) {
        if (source[i] > dest[i]) {
            dest[i] = source[i];
        }
    }
}

/**
 * @brief Compute cosine similarity between two vectors
 *
 * WHAT: Measure similarity between two content vectors
 * WHY:  Coherence computation requires measuring alignment
 * HOW:  cos(θ) = (a · b) / (||a|| × ||b||)
 *
 * @param a First vector
 * @param b Second vector
 * @param dim Vector dimensionality
 * @return Similarity in [0,1] (0 = orthogonal, 1 = identical)
 */
static float cosine_similarity(
    const float* a,
    const float* b,
    uint32_t dim
) {
    float dot = 0.0F;
    float norm_a = 0.0F;
    float norm_b = 0.0F;

    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    float denom = sqrtf(norm_a) * sqrtf(norm_b);
    if (denom < 1e-9F) {
        return 0.0F;  // Zero vectors
    }

    return dot / denom;
}

/**
 * @brief Blend two content vectors (weighted average)
 *
 * WHAT: Combine two content vectors with specified weight
 * WHY:  Concurrent updates with similar salience should blend content
 * HOW:  dest = (1 - w) × dest + w × source
 *
 * @param dest Destination vector (modified in-place)
 * @param source Source vector
 * @param weight_source Weight for source [0,1]
 * @param dim Vector dimensionality
 */
static void blend_content_vectors(
    float* dest,
    const float* source,
    float weight_source,
    uint32_t dim
) {
    float weight_dest = 1.0F - weight_source;
    for (uint32_t i = 0; i < dim; i++) {
        dest[i] = weight_dest * dest[i] + weight_source * source[i];
    }
}

/**
 * @brief Check if two item types are compatible for blending
 *
 * WHAT: Determine if two workspace item types can have their content blended
 * WHY:  Only semantically compatible types should be merged
 * HOW:  Define compatibility rules based on semantic similarity
 *
 * COMPATIBILITY RULES:
 * - Same type always compatible
 * - PERCEPTION + MEMORY compatible (both observations)
 * - GOAL + PREDICTION compatible (both future-oriented)
 * - STATE + META compatible (both swarm state)
 *
 * @param type_a First item type
 * @param type_b Second item type
 * @return true if types can be blended
 */
static bool types_compatible(
    workspace_item_type_t type_a,
    workspace_item_type_t type_b
) {
    // Same type always compatible
    if (type_a == type_b) {
        return true;
    }

    // PERCEPTION and MEMORY can blend
    if ((type_a == WORKSPACE_ITEM_PERCEPTION && type_b == WORKSPACE_ITEM_MEMORY) ||
        (type_a == WORKSPACE_ITEM_MEMORY && type_b == WORKSPACE_ITEM_PERCEPTION)) {
        return true;
    }

    // GOAL and PREDICTION can blend
    if ((type_a == WORKSPACE_ITEM_GOAL && type_b == WORKSPACE_ITEM_PREDICTION) ||
        (type_a == WORKSPACE_ITEM_PREDICTION && type_b == WORKSPACE_ITEM_GOAL)) {
        return true;
    }

    // STATE and META can blend
    if ((type_a == WORKSPACE_ITEM_STATE && type_b == WORKSPACE_ITEM_META) ||
        (type_a == WORKSPACE_ITEM_META && type_b == WORKSPACE_ITEM_STATE)) {
        return true;
    }

    return false;  // Types not compatible
}

/**
 * @brief Find item by ID
 *
 * WHAT: Linear search for workspace item by ID
 * WHY:  Need to find existing items for merge operations
 * HOW:  Scan items array for matching item_id
 *
 * @param workspace Collective workspace
 * @param item_id Item identifier to find
 * @return Index of item, or -1 if not found
 *
 * COMPLEXITY: O(N) where N = item_count
 */
static int32_t find_item_by_id(
    const collective_workspace_t* workspace,
    uint32_t item_id
) {
    for (uint32_t i = 0; i < workspace->item_count; i++) {
        if (workspace->items[i].item_id == item_id) {
            return (int32_t)i;
        }
    }
    return -1;  /* Not found - normal search miss */
}

/**
 * @brief Insert item maintaining salience order (descending)
 *
 * WHAT: Add item to workspace in sorted position by salience
 * WHY:  Keep items sorted for fast top-K access
 * HOW:  Find insertion point, shift items down, insert
 *
 * ALGORITHM:
 * 1. If workspace not full:
 *    - Find insertion point (first item with lower salience)
 *    - Shift items down to make space
 *    - Insert new item
 * 2. If workspace full:
 *    - If new item salience <= lowest: Reject
 *    - Otherwise: Find insertion point, shift (evict lowest), insert
 *
 * @param workspace Collective workspace
 * @param item Item to insert
 * @return Index where item was inserted, or -1 if rejected
 *
 * COMPLEXITY: O(N) where N = item_count (shift operation)
 * SIDE EFFECTS: May evict lowest-salience item if workspace full
 */
static int32_t insert_item_sorted(
    collective_workspace_t* workspace,
    const collective_workspace_item_t* item
) {
    // If workspace not full, find insertion point
    if (workspace->item_count < COLLECTIVE_WORKSPACE_MAX_ITEMS) {
        // Find insertion point (binary search would be better for large N)
        uint32_t insert_idx = workspace->item_count;
        for (uint32_t i = 0; i < workspace->item_count; i++) {
            if (item->salience > workspace->items[i].salience) {
                insert_idx = i;
                break;
            }
        }

        // Shift items down
        for (uint32_t i = workspace->item_count; i > insert_idx; i--) {
            workspace->items[i] = workspace->items[i - 1];
        }

        // Insert new item
        workspace->items[insert_idx] = *item;
        workspace->item_count++;

        return (int32_t)insert_idx;
    }

    // Workspace full - check if new item displaces lowest
    uint32_t lowest_idx = workspace->item_count - 1;
    if (item->salience <= workspace->items[lowest_idx].salience) {
        return -1;  // New item too low salience
    }

    // Find insertion point
    uint32_t insert_idx = lowest_idx;
    for (uint32_t i = 0; i < workspace->item_count; i++) {
        if (item->salience > workspace->items[i].salience) {
            insert_idx = i;
            break;
        }
    }

    // Shift items down (evict lowest)
    for (uint32_t i = lowest_idx; i > insert_idx; i--) {
        workspace->items[i] = workspace->items[i - 1];
    }

    // Insert new item
    workspace->items[insert_idx] = *item;
    workspace->items_pruned++;  // Count evicted item as pruned

    return (int32_t)insert_idx;
}

/**
 * @brief Remove item at index
 *
 * WHAT: Delete item from workspace and compact array
 * WHY:  Pruning and eviction require item removal
 * HOW:  Shift items up to fill gap
 *
 * @param workspace Collective workspace
 * @param index Index of item to remove
 *
 * COMPLEXITY: O(N) where N = item_count (shift operation)
 */
static void remove_item_at_index(
    collective_workspace_t* workspace,
    uint32_t index
) {
    if (index >= workspace->item_count) {
        return;
    }

    // Shift items up
    for (uint32_t i = index; i < workspace->item_count - 1; i++) {
        workspace->items[i] = workspace->items[i + 1];
    }

    workspace->item_count--;
}

/**
 * @brief Update collective metrics (coherence, focus vector)
 *
 * WHAT: Recompute swarm alignment and attention metrics
 * WHY:  Track collective cognitive state for meta-cognition
 * HOW:  Compute focus vector (average content), then coherence (similarity to focus)
 *
 * ALGORITHM:
 * 1. Compute average salience
 * 2. For items within coherence window:
 *    - Compute weighted average content (focus vector)
 * 3. Compute coherence as average similarity to focus vector
 * 4. Clamp coherence to [0,1]
 *
 * @param workspace Collective workspace
 * @param current_time_ms Current timestamp
 *
 * COMPLEXITY: O(N × D) where N = items, D = content_dim
 * SIDE EFFECTS: Updates collective_coherence, collective_salience, swarm_focus_vector
 */
static void update_collective_metrics(
    collective_workspace_t* workspace,
    uint64_t current_time_ms
) {
    if (!workspace->meta_cognition_active || workspace->item_count == 0) {
        workspace->collective_coherence = 0.0F;
        workspace->collective_salience = 0.0F;
        memset(workspace->swarm_focus_vector, 0,
               COLLECTIVE_WORKSPACE_CONTENT_DIM * sizeof(float));
        return;
    }

    // Compute average salience
    float total_salience = 0.0F;
    for (uint32_t i = 0; i < workspace->item_count; i++) {
        total_salience += workspace->items[i].salience;
    }
    workspace->collective_salience = total_salience / workspace->item_count;

    // Get items within coherence window
    uint32_t window_items = 0;
    float focus_vector[COLLECTIVE_WORKSPACE_CONTENT_DIM] = {0};

    for (uint32_t i = 0; i < workspace->item_count; i++) {
        uint64_t age_ms = current_time_ms - workspace->items[i].timestamp_ms;
        if (age_ms <= workspace->config.coherence_window_ms) {
            window_items++;
            // Weight by salience when computing focus
            float weight = workspace->items[i].salience;
            for (uint32_t d = 0; d < COLLECTIVE_WORKSPACE_CONTENT_DIM; d++) {
                focus_vector[d] += weight * workspace->items[i].content[d];
            }
        }
    }

    if (window_items == 0) {
        workspace->collective_coherence = 0.0F;
        memset(workspace->swarm_focus_vector, 0,
               COLLECTIVE_WORKSPACE_CONTENT_DIM * sizeof(float));
        return;
    }

    // Normalize focus vector
    float total_weight = 0.0F;
    for (uint32_t i = 0; i < workspace->item_count; i++) {
        uint64_t age_ms = current_time_ms - workspace->items[i].timestamp_ms;
        if (age_ms <= workspace->config.coherence_window_ms) {
            total_weight += workspace->items[i].salience;
        }
    }

    if (total_weight > 0.0F) {
        for (uint32_t d = 0; d < COLLECTIVE_WORKSPACE_CONTENT_DIM; d++) {
            focus_vector[d] /= total_weight;
        }
    }

    memcpy(workspace->swarm_focus_vector, focus_vector,
           COLLECTIVE_WORKSPACE_CONTENT_DIM * sizeof(float));

    // Compute coherence as average similarity to focus vector
    float total_similarity = 0.0F;
    uint32_t similarity_count = 0;

    for (uint32_t i = 0; i < workspace->item_count; i++) {
        uint64_t age_ms = current_time_ms - workspace->items[i].timestamp_ms;
        if (age_ms <= workspace->config.coherence_window_ms) {
            float similarity = cosine_similarity(
                workspace->items[i].content,
                focus_vector,
                COLLECTIVE_WORKSPACE_CONTENT_DIM
            );
            total_similarity += similarity;
            similarity_count++;
        }
    }

    workspace->collective_coherence = (similarity_count > 0) ?
        (total_similarity / similarity_count) : 0.0F;

    // Clamp to [0,1]
    if (workspace->collective_coherence < 0.0F) {
        workspace->collective_coherence = 0.0F;
    }
    if (workspace->collective_coherence > 1.0F) {
        workspace->collective_coherence = 1.0F;
    }
}

/**
 * @brief Validate workspace item for security
 *
 * WHAT: BBB security validation for workspace item structure
 * WHY:  Prevent malformed or malicious items from corrupting workspace
 * HOW:  Check salience range, vector clock values, content bounds
 *
 * VALIDATION CHECKS:
 * - Salience in [0,1]
 * - Vector clock values reasonable
 * - Content values not NaN/Inf and within bounds
 * - Item type valid
 *
 * @param item Item to validate
 * @param swarm_size Swarm size for vector clock validation
 * @return true if valid
 */
static bool validate_workspace_item(
    const collective_workspace_item_t* item,
    uint16_t swarm_size
) {
    // Validate salience
    if (item->salience < 0.0F || item->salience > 1.0F || isnan(item->salience)) {
        LOG_ERROR("Invalid salience: %.3f", item->salience);
        bbb_audit_log(BBB_AUDIT_WARNING, MODULE_NAME, "validation_failed",
                     "Invalid salience: %.3f", item->salience);
        return false;
    }

    // Validate vector clock
    for (uint16_t i = 0; i < swarm_size; i++) {
        if (item->vector_clock[i] > MAX_VECTOR_CLOCK_VALUE) {
            LOG_ERROR("Vector clock[%u] too large: %lu", i, item->vector_clock[i]);
            bbb_audit_log(BBB_AUDIT_WARNING, MODULE_NAME, "validation_failed",
                         "Vector clock[%u] too large", i);
            return false;
        }
    }

    // Validate content
    for (uint32_t i = 0; i < COLLECTIVE_WORKSPACE_CONTENT_DIM; i++) {
        if (isnan(item->content[i]) || isinf(item->content[i])) {
            LOG_ERROR("Invalid content[%u]: NaN or Inf", i);
            bbb_audit_log(BBB_AUDIT_WARNING, MODULE_NAME, "validation_failed",
                         "Invalid content: NaN or Inf");
            return false;
        }
        if (fabsf(item->content[i]) > MAX_CONTENT_VALUE) {
            LOG_ERROR("Content[%u] out of bounds: %.3f", i, item->content[i]);
            bbb_audit_log(BBB_AUDIT_WARNING, MODULE_NAME, "validation_failed",
                         "Content out of bounds");
            return false;
        }
    }

    // Validate item type
    if (item->type < WORKSPACE_ITEM_NONE ||
        (item->type > WORKSPACE_ITEM_META && item->type < WORKSPACE_ITEM_CUSTOM)) {
        LOG_ERROR("Invalid item type: %d", item->type);
        return false;  /* Validation failure, caller handles */
    }

    // Validate source drone
    if (item->source_drone >= swarm_size) {
        LOG_ERROR("Source drone %u >= swarm_size %u", item->source_drone, swarm_size);
        return false;  /* Validation failure, caller handles */
    }

    return true;
}

// Bio-async integration is handled externally via callback registration
// The workspace provides callback-compatible functions for integration

//=============================================================================
// Public API Implementation
//=============================================================================

collective_workspace_config_t collective_workspace_default_config(
    uint16_t local_drone_id,
    uint16_t swarm_size
) {
    collective_workspace_config_t config = {
        .local_drone_id = local_drone_id,
        .swarm_size = swarm_size,
        .broadcast_threshold = COLLECTIVE_WORKSPACE_DEFAULT_BROADCAST_THRESHOLD,
        .coherence_window_ms = COLLECTIVE_WORKSPACE_DEFAULT_COHERENCE_WINDOW_MS,
        .pruning_interval_ms = COLLECTIVE_WORKSPACE_PRUNING_INTERVAL_MS,
        .item_ttl_ms = COLLECTIVE_WORKSPACE_ITEM_TTL_MS,
        .min_salience = COLLECTIVE_WORKSPACE_MIN_SALIENCE,
        .enable_meta_cognition = true
    };
    return config;
}

collective_workspace_t* collective_workspace_create_simple(
    uint16_t local_drone_id,
    uint16_t swarm_size
) {
    collective_workspace_config_t config =
        collective_workspace_default_config(local_drone_id, swarm_size);
    return collective_workspace_create(&config);
}

collective_workspace_t* collective_workspace_create(
    const collective_workspace_config_t* config
) {
    // BBB validation
    if (!bbb_check_pointer(config, "collective_workspace_create")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_workspace_create: config is NULL");
        return NULL;
    }

    // Validate configuration
    char error_msg[NIMCP_ERROR_BUFFER_SIZE];
    if (!collective_workspace_validate_config(config, error_msg, sizeof(error_msg))) {
        LOG_ERROR("Invalid config: %s", error_msg);
        bbb_audit_log(BBB_AUDIT_ERROR, MODULE_NAME, "create_failed",
                     "Invalid configuration: %s", error_msg);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_workspace_create: invalid configuration");
        return NULL;
    }

    // Allocate workspace
    collective_workspace_t* workspace =
        (collective_workspace_t*)nimcp_calloc(1, sizeof(collective_workspace_t));
    if (!workspace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "collective_workspace_create: failed to allocate workspace");
        bbb_audit_log(BBB_AUDIT_ERROR, MODULE_NAME, "create_failed",
                     "Memory allocation failed");
        return NULL;
    }
    workspace->config = *config;
    workspace->local_drone_id = config->local_drone_id;
    workspace->swarm_size = config->swarm_size;
    workspace->local_clock = 0;
    workspace->item_count = 0;
    workspace->collective_coherence = 0.0F;
    workspace->collective_salience = 0.0F;
    workspace->meta_cognition_active = config->enable_meta_cognition;
    workspace->creation_time_ms = get_time_ms();
    workspace->last_prune_time_ms = workspace->creation_time_ms;

    // Initialize statistics
    workspace->total_items_received = 0;
    workspace->total_items_sent = 0;
    workspace->merge_conflicts = 0;
    workspace->items_pruned = 0;
    workspace->broadcasts_sent = 0;

    // Initialize mutex
    if (nimcp_mutex_init(&workspace->mutex, NULL) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "collective_workspace_create: failed to initialize mutex");
        bbb_audit_log(BBB_AUDIT_ERROR, MODULE_NAME, "create_failed",
                     "Mutex initialization failed");
        nimcp_free(workspace);
        return NULL;
    }

    // Register with BBB
    bbb_register_module(MODULE_NAME, BBB_MODULE_TYPE_SWARM);

    // Audit log creation
    bbb_audit_log(BBB_AUDIT_INFO, MODULE_NAME, "workspace_created",
                 "drone_id=%u, swarm_size=%u, broadcast_threshold=%.3f",
                 config->local_drone_id, config->swarm_size, config->broadcast_threshold);

    LOG_INFO("Created collective workspace: drone_id=%u, swarm_size=%u, broadcast_threshold=%.3f",
             config->local_drone_id, config->swarm_size, config->broadcast_threshold);

    return workspace;
}

void collective_workspace_destroy(collective_workspace_t* workspace) {
    if (!workspace) {
        return;
    }

    // Audit log destruction
    bbb_audit_log(BBB_AUDIT_INFO, MODULE_NAME, "workspace_destroyed",
                 "items=%u, sent=%u, received=%u, conflicts=%lu",
                 workspace->item_count, workspace->total_items_sent,
                 workspace->total_items_received, workspace->merge_conflicts);

    LOG_INFO("Destroying collective workspace: items=%u, sent=%u, received=%u, conflicts=%lu",
             workspace->item_count, workspace->total_items_sent,
             workspace->total_items_received, workspace->merge_conflicts);

    nimcp_mutex_destroy(&workspace->mutex);
    nimcp_free(workspace);
}

bool collective_workspace_add_item(
    collective_workspace_t* workspace,
    const collective_workspace_item_t* item
) {
    // BBB validation
    if (!bbb_check_pointer(workspace, "collective_workspace_add_item")) {
        return false;
    }
    if (!bbb_check_pointer(item, "collective_workspace_add_item")) {
        return false;
    }

    // Validate item structure
    if (!validate_workspace_item(item, workspace->swarm_size)) {
        LOG_ERROR("Item validation failed");
        return false;
    }

    nimcp_mutex_lock(&workspace->mutex);

    // Increment local clock
    workspace->local_clock++;

    // Create modified item with initialized vector clock
    collective_workspace_item_t local_item = *item;
    memset(local_item.vector_clock, 0, sizeof(local_item.vector_clock));
    local_item.vector_clock[workspace->local_drone_id] = workspace->local_clock;
    local_item.source_drone = workspace->local_drone_id;
    local_item.timestamp_ms = get_time_ms();
    local_item.broadcast_count = 0;

    // Mark for broadcast if above threshold
    local_item.marked_for_broadcast =
        (local_item.salience >= workspace->config.broadcast_threshold);

    // Insert into workspace
    int32_t insert_idx = insert_item_sorted(workspace, &local_item);
    if (insert_idx < 0) {
        LOG_DEBUG("Item rejected (workspace full, salience too low): id=0x%08x, salience=%.3f",
                  local_item.item_id, local_item.salience);
        nimcp_mutex_unlock(&workspace->mutex);
        return false;  /* Normal capacity condition */
    }

    // Update metrics
    update_collective_metrics(workspace, local_item.timestamp_ms);

    // Audit log
    bbb_audit_log(BBB_AUDIT_DEBUG, MODULE_NAME, "item_added",
                 "id=0x%08x, type=%s, salience=%.3f, broadcast=%d",
                 local_item.item_id,
                 workspace_item_type_to_string(local_item.type),
                 local_item.salience,
                 local_item.marked_for_broadcast);

    LOG_DEBUG("Added local item: id=0x%08x, type=%s, salience=%.3f, broadcast=%d",
              local_item.item_id,
              workspace_item_type_to_string(local_item.type),
              local_item.salience,
              local_item.marked_for_broadcast);

    nimcp_mutex_unlock(&workspace->mutex);
    return true;
}

bool collective_workspace_merge_item(
    collective_workspace_t* workspace,
    const collective_workspace_item_t* item
) {
    // BBB validation
    if (!bbb_check_pointer(workspace, "collective_workspace_merge_item")) {
        return false;
    }
    if (!bbb_check_pointer(item, "collective_workspace_merge_item")) {
        return false;
    }

    // Validate item structure
    if (!validate_workspace_item(item, workspace->swarm_size)) {
        LOG_ERROR("Item validation failed");
        bbb_audit_log(BBB_AUDIT_WARNING, MODULE_NAME, "merge_rejected",
                     "Item validation failed: id=0x%08x", item->item_id);
        return false;
    }

    // Additional network data validation
    if (!bbb_validate_network_data(item, sizeof(*item), "collective_workspace_merge_item")) {
        LOG_ERROR("Network data validation failed");
        return false;
    }

    nimcp_mutex_lock(&workspace->mutex);

    workspace->total_items_received++;

    // Find existing item with same ID
    int32_t existing_idx = find_item_by_id(workspace, item->item_id);

    if (existing_idx < 0) {
        // New item - insert
        collective_workspace_item_t new_item = *item;
        new_item.marked_for_broadcast = false;  // Don't re-broadcast received items

        int32_t insert_idx = insert_item_sorted(workspace, &new_item);
        if (insert_idx < 0) {
            LOG_DEBUG("Received item rejected (workspace full, salience too low): id=0x%08x",
                      item->item_id);
            nimcp_mutex_unlock(&workspace->mutex);
            return false;  /* Normal capacity condition */
        }

        // Update local vector clock (merge)
        merge_vector_clocks(workspace->items[insert_idx].vector_clock,
                          item->vector_clock, workspace->swarm_size);

        update_collective_metrics(workspace, get_time_ms());

        bbb_audit_log(BBB_AUDIT_DEBUG, MODULE_NAME, "item_merged_new",
                     "id=0x%08x, salience=%.3f, source_drone=%u",
                     item->item_id, item->salience, item->source_drone);

        LOG_DEBUG("Merged new item: id=0x%08x, salience=%.3f, source_drone=%u",
                  item->item_id, item->salience, item->source_drone);

        nimcp_mutex_unlock(&workspace->mutex);
        return true;
    }

    // Existing item - resolve conflict
    collective_workspace_item_t* existing = &workspace->items[existing_idx];

    // Compare vector clocks
    int clock_cmp = compare_vector_clocks(
        item->vector_clock,
        existing->vector_clock,
        workspace->swarm_size
    );

    bool replaced = false;
    bool blended = false;

    if (clock_cmp == 0) {
        // Same event (duplicate) - ignore
        LOG_DEBUG("Duplicate item ignored: id=0x%08x", item->item_id);
        nimcp_mutex_unlock(&workspace->mutex);
        return true;

    } else if (clock_cmp == -1) {
        // Incoming happened-before existing - keep existing (newer)
        LOG_DEBUG("Incoming item older (causal), keeping existing: id=0x%08x",
                  item->item_id);
        nimcp_mutex_unlock(&workspace->mutex);
        return true;

    } else if (clock_cmp == 1) {
        // Existing happened-before incoming - replace with incoming (newer)
        *existing = *item;
        existing->marked_for_broadcast = false;
        replaced = true;

        LOG_DEBUG("Existing item replaced (causal): id=0x%08x, new_salience=%.3f",
                  item->item_id, item->salience);

    } else {
        // Concurrent - resolve by salience
        workspace->merge_conflicts++;

        float salience_diff = fabsf(item->salience - existing->salience);

        if (item->salience > existing->salience) {
            // Incoming wins
            *existing = *item;
            existing->marked_for_broadcast = false;
            replaced = true;

            bbb_audit_log(BBB_AUDIT_DEBUG, MODULE_NAME, "conflict_resolved_incoming",
                         "id=0x%08x, incoming_salience=%.3f, existing_salience=%.3f",
                         item->item_id, item->salience, existing->salience);

            LOG_DEBUG("Concurrent conflict resolved by salience (incoming wins): "
                      "id=0x%08x, incoming_salience=%.3f, existing_salience=%.3f",
                      item->item_id, item->salience, existing->salience);

        } else if (existing->salience > item->salience) {
            // Existing wins
            LOG_DEBUG("Concurrent conflict resolved by salience (existing wins): "
                      "id=0x%08x, existing_salience=%.3f, incoming_salience=%.3f",
                      item->item_id, existing->salience, item->salience);

        } else if (salience_diff < 0.05F &&
                   types_compatible(item->type, existing->type)) {
            // Similar salience and compatible types - blend content
            float blend_weight = 0.5F;  // Equal weight
            blend_content_vectors(existing->content, item->content,
                                blend_weight, COLLECTIVE_WORKSPACE_CONTENT_DIM);

            // Average salience
            existing->salience = (existing->salience + item->salience) / 2.0F;

            blended = true;

            bbb_audit_log(BBB_AUDIT_DEBUG, MODULE_NAME, "items_blended",
                         "id=0x%08x, blended_salience=%.3f",
                         item->item_id, existing->salience);

            LOG_DEBUG("Concurrent items blended: id=0x%08x, blended_salience=%.3f",
                      item->item_id, existing->salience);

        } else {
            // Equal salience, incompatible types - use timestamp
            if (item->timestamp_ms > existing->timestamp_ms) {
                *existing = *item;
                existing->marked_for_broadcast = false;
                replaced = true;

                LOG_DEBUG("Concurrent conflict resolved by timestamp (incoming newer): "
                          "id=0x%08x", item->item_id);
            } else {
                LOG_DEBUG("Concurrent conflict resolved by timestamp (existing newer): "
                          "id=0x%08x", item->item_id);
            }
        }

        // Merge vector clocks (always take max)
        merge_vector_clocks(existing->vector_clock, item->vector_clock,
                          workspace->swarm_size);
    }

    // Re-sort if salience changed
    if (replaced || blended) {
        // Remove from current position
        collective_workspace_item_t temp_item = *existing;
        remove_item_at_index(workspace, existing_idx);

        // Re-insert at correct position
        insert_item_sorted(workspace, &temp_item);
    }

    update_collective_metrics(workspace, get_time_ms());

    nimcp_mutex_unlock(&workspace->mutex);
    return true;
}

bool collective_workspace_get_top_items(
    const collective_workspace_t* workspace,
    collective_workspace_item_t* top_items,
    uint32_t max_items,
    uint32_t* actual_count
) {
    // BBB validation
    if (!bbb_check_pointer(workspace, "collective_workspace_get_top_items")) {
        return false;
    }
    if (!bbb_check_pointer(top_items, "collective_workspace_get_top_items")) {
        return false;
    }
    if (!bbb_check_pointer(actual_count, "collective_workspace_get_top_items")) {
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)&workspace->mutex);

    uint32_t count = (workspace->item_count < max_items) ?
                     workspace->item_count : max_items;

    memcpy(top_items, workspace->items,
           count * sizeof(collective_workspace_item_t));

    *actual_count = count;

    nimcp_mutex_unlock((nimcp_mutex_t*)&workspace->mutex);
    return true;
}

bool collective_workspace_get_broadcast_items(
    collective_workspace_t* workspace,
    collective_workspace_item_t* broadcast_items,
    uint32_t max_items,
    uint32_t* actual_count
) {
    // BBB validation
    if (!bbb_check_pointer(workspace, "collective_workspace_get_broadcast_items")) {
        return false;
    }
    if (!bbb_check_pointer(broadcast_items, "collective_workspace_get_broadcast_items")) {
        return false;
    }
    if (!bbb_check_pointer(actual_count, "collective_workspace_get_broadcast_items")) {
        return false;
    }

    nimcp_mutex_lock(&workspace->mutex);

    uint32_t count = 0;
    for (uint32_t i = 0; i < workspace->item_count && count < max_items; i++) {
        if (workspace->items[i].marked_for_broadcast) {
            broadcast_items[count] = workspace->items[i];
            broadcast_items[count].broadcast_count++;
            count++;

            // Clear flag (consumed)
            workspace->items[i].marked_for_broadcast = false;
        }
    }

    *actual_count = count;
    workspace->total_items_sent += count;
    workspace->broadcasts_sent += (count > 0) ? 1 : 0;

    if (count > 0) {
        bbb_audit_log(BBB_AUDIT_DEBUG, MODULE_NAME, "items_broadcast",
                     "count=%u", count);
        LOG_DEBUG("Got %u items for broadcast", count);
    }

    nimcp_mutex_unlock(&workspace->mutex);
    return true;
}

bool collective_workspace_should_broadcast(
    const collective_workspace_t* workspace,
    const collective_workspace_item_t* item
) {
    // BBB validation
    if (!bbb_check_pointer(workspace, "collective_workspace_should_broadcast")) {
        return false;
    }
    if (!bbb_check_pointer(item, "collective_workspace_should_broadcast")) {
        return false;
    }

    // Check salience threshold
    if (item->salience < workspace->config.broadcast_threshold) {
        return false;
    }

    // Check broadcast count (prevent storms)
    if (item->broadcast_count >= 3) {  // Max 3 broadcasts per item
        return false;
    }

    // Check age (don't broadcast stale items)
    uint64_t current_time_ms = get_time_ms();
    uint64_t age_ms = current_time_ms - item->timestamp_ms;
    if (age_ms > workspace->config.item_ttl_ms) {
        return false;
    }

    return true;
}

bool collective_workspace_mark_for_broadcast(
    collective_workspace_t* workspace,
    uint32_t item_id
) {
    // BBB validation
    if (!bbb_check_pointer(workspace, "collective_workspace_mark_for_broadcast")) {
        return false;
    }

    nimcp_mutex_lock(&workspace->mutex);

    int32_t idx = find_item_by_id(workspace, item_id);
    if (idx < 0) {
        nimcp_mutex_unlock(&workspace->mutex);
        return false;  /* Item not found - normal search miss */
    }

    workspace->items[idx].marked_for_broadcast = true;

    bbb_audit_log(BBB_AUDIT_DEBUG, MODULE_NAME, "item_marked_broadcast",
                 "id=0x%08x", item_id);
    LOG_DEBUG("Marked item for broadcast: id=0x%08x", item_id);

    nimcp_mutex_unlock(&workspace->mutex);
    return true;
}

uint32_t collective_workspace_prune(
    collective_workspace_t* workspace,
    uint64_t current_time_ms
) {
    // BBB validation
    if (!bbb_check_pointer(workspace, "collective_workspace_prune")) {
        return 0;
    }

    nimcp_mutex_lock(&workspace->mutex);

    workspace->last_prune_time_ms = current_time_ms;

    uint32_t pruned_count = 0;
    uint32_t i = 0;

    while (i < workspace->item_count) {
        bool should_prune = false;

        // Check age
        uint64_t age_ms = current_time_ms - workspace->items[i].timestamp_ms;
        if (age_ms > workspace->config.item_ttl_ms) {
            should_prune = true;
            LOG_DEBUG("Pruning stale item: id=0x%08x, age_ms=%lu",
                      workspace->items[i].item_id, age_ms);
        }

        // Check salience
        if (!should_prune && workspace->items[i].salience < workspace->config.min_salience) {
            should_prune = true;
            LOG_DEBUG("Pruning low-salience item: id=0x%08x, salience=%.3f",
                      workspace->items[i].item_id, workspace->items[i].salience);
        }

        if (should_prune) {
            remove_item_at_index(workspace, i);
            pruned_count++;
            // Don't increment i (next item shifted into this position)
        } else {
            i++;
        }
    }

    workspace->items_pruned += pruned_count;

    if (pruned_count > 0) {
        update_collective_metrics(workspace, current_time_ms);
        bbb_audit_log(BBB_AUDIT_DEBUG, MODULE_NAME, "items_pruned",
                     "count=%u", pruned_count);
        LOG_DEBUG("Pruned %u items", pruned_count);
    }

    nimcp_mutex_unlock(&workspace->mutex);
    return pruned_count;
}

float collective_workspace_get_coherence(
    const collective_workspace_t* workspace
) {
    if (!bbb_check_pointer(workspace, "collective_workspace_get_coherence")) {
        return 0.0F;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)&workspace->mutex);
    float coherence = workspace->collective_coherence;
    nimcp_mutex_unlock((nimcp_mutex_t*)&workspace->mutex);

    return coherence;
}

bool collective_workspace_get_focus_vector(
    const collective_workspace_t* workspace,
    float* focus_vector
) {
    // BBB validation
    if (!bbb_check_pointer(workspace, "collective_workspace_get_focus_vector")) {
        return false;
    }
    if (!bbb_check_pointer(focus_vector, "collective_workspace_get_focus_vector")) {
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)&workspace->mutex);
    memcpy(focus_vector, workspace->swarm_focus_vector,
           COLLECTIVE_WORKSPACE_CONTENT_DIM * sizeof(float));
    nimcp_mutex_unlock((nimcp_mutex_t*)&workspace->mutex);

    return true;
}

uint32_t collective_workspace_get_item_count(
    const collective_workspace_t* workspace
) {
    if (!bbb_check_pointer(workspace, "collective_workspace_get_item_count")) {
        return 0;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)&workspace->mutex);
    uint32_t count = workspace->item_count;
    nimcp_mutex_unlock((nimcp_mutex_t*)&workspace->mutex);

    return count;
}

bool collective_workspace_get_statistics(
    const collective_workspace_t* workspace,
    uint32_t* total_received,
    uint32_t* total_sent,
    uint64_t* merge_conflicts,
    uint64_t* items_pruned
) {
    if (!bbb_check_pointer(workspace, "collective_workspace_get_statistics")) {
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)&workspace->mutex);

    if (total_received) *total_received = workspace->total_items_received;
    if (total_sent) *total_sent = workspace->total_items_sent;
    if (merge_conflicts) *merge_conflicts = workspace->merge_conflicts;
    if (items_pruned) *items_pruned = workspace->items_pruned;

    nimcp_mutex_unlock((nimcp_mutex_t*)&workspace->mutex);
    return true;
}

const char* workspace_item_type_to_string(workspace_item_type_t type) {
    switch (type) {
        case WORKSPACE_ITEM_NONE:        return "NONE";
        case WORKSPACE_ITEM_PERCEPTION:  return "PERCEPTION";
        case WORKSPACE_ITEM_GOAL:        return "GOAL";
        case WORKSPACE_ITEM_MEMORY:      return "MEMORY";
        case WORKSPACE_ITEM_THREAT:      return "THREAT";
        case WORKSPACE_ITEM_OPPORTUNITY: return "OPPORTUNITY";
        case WORKSPACE_ITEM_STATE:       return "STATE";
        case WORKSPACE_ITEM_COMMAND:     return "COMMAND";
        case WORKSPACE_ITEM_QUERY:       return "QUERY";
        case WORKSPACE_ITEM_PREDICTION:  return "PREDICTION";
        case WORKSPACE_ITEM_META:        return "META";
        default:
            if (type >= WORKSPACE_ITEM_CUSTOM) {
                return "CUSTOM";
            }
            return "UNKNOWN";
    }
}

void collective_workspace_print_state(
    const collective_workspace_t* workspace,
    bool verbose
) {
    if (!bbb_check_pointer(workspace, "collective_workspace_print_state")) {
        fprintf(stderr, "NULL workspace\n");
        return;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)&workspace->mutex);

    fprintf(stderr, "\n=== Collective Workspace State ===\n");
    fprintf(stderr, "Drone ID: %u / %u\n",
            workspace->local_drone_id, workspace->swarm_size);
    fprintf(stderr, "Items: %u / %u\n",
            workspace->item_count, COLLECTIVE_WORKSPACE_MAX_ITEMS);
    fprintf(stderr, "Collective coherence: %.3f\n", workspace->collective_coherence);
    fprintf(stderr, "Collective salience: %.3f\n", workspace->collective_salience);
    fprintf(stderr, "Local clock: %lu\n", workspace->local_clock);
    fprintf(stderr, "Items received: %u\n", workspace->total_items_received);
    fprintf(stderr, "Items sent: %u\n", workspace->total_items_sent);
    fprintf(stderr, "Merge conflicts: %lu\n", workspace->merge_conflicts);
    fprintf(stderr, "Items pruned: %lu\n", workspace->items_pruned);
    fprintf(stderr, "Broadcasts sent: %lu\n", workspace->broadcasts_sent);

    if (verbose && workspace->item_count > 0) {
        fprintf(stderr, "\nWorkspace Items (sorted by salience):\n");
        for (uint32_t i = 0; i < workspace->item_count; i++) {
            const collective_workspace_item_t* item = &workspace->items[i];
            fprintf(stderr, "  [%u] ID=0x%08x, Type=%s, Salience=%.3f, "
                           "Source=%u, Age=%lums, Broadcasts=%u, Marked=%d\n",
                    i, item->item_id,
                    workspace_item_type_to_string(item->type),
                    item->salience,
                    item->source_drone,
                    get_time_ms() - item->timestamp_ms,
                    item->broadcast_count,
                    item->marked_for_broadcast);

            if (verbose) {
                fprintf(stderr, "      Vector clock: [");
                for (uint16_t j = 0; j < workspace->swarm_size; j++) {
                    fprintf(stderr, "%lu%s", item->vector_clock[j],
                           (j < workspace->swarm_size - 1) ? ", " : "");
                }
                fprintf(stderr, "]\n");

                fprintf(stderr, "      Content: [");
                for (uint32_t j = 0; j < COLLECTIVE_WORKSPACE_CONTENT_DIM; j++) {
                    fprintf(stderr, "%.2f%s", item->content[j],
                           (j < COLLECTIVE_WORKSPACE_CONTENT_DIM - 1) ? ", " : "");
                }
                fprintf(stderr, "]\n");
            }
        }
    }

    if (workspace->meta_cognition_active) {
        fprintf(stderr, "\nSwarm Focus Vector: [");
        for (uint32_t i = 0; i < COLLECTIVE_WORKSPACE_CONTENT_DIM; i++) {
            fprintf(stderr, "%.2f%s", workspace->swarm_focus_vector[i],
                   (i < COLLECTIVE_WORKSPACE_CONTENT_DIM - 1) ? ", " : "");
        }
        fprintf(stderr, "]\n");
    }

    fprintf(stderr, "================================\n\n");

    nimcp_mutex_unlock((nimcp_mutex_t*)&workspace->mutex);
}

bool collective_workspace_validate_config(
    const collective_workspace_config_t* config,
    char* error_msg,
    size_t error_msg_len
) {
    if (!config) {
        if (error_msg) {
            snprintf(error_msg, error_msg_len, "NULL configuration");
        }
        return false;
    }

    if (config->local_drone_id >= config->swarm_size) {
        if (error_msg) {
            snprintf(error_msg, error_msg_len,
                    "local_drone_id (%u) must be < swarm_size (%u)",
                    config->local_drone_id, config->swarm_size);
        }
        return false;
    }

    if (config->swarm_size == 0 ||
        config->swarm_size > COLLECTIVE_WORKSPACE_MAX_SWARM_SIZE) {
        if (error_msg) {
            snprintf(error_msg, error_msg_len,
                    "swarm_size (%u) must be in [1, %u]",
                    config->swarm_size, COLLECTIVE_WORKSPACE_MAX_SWARM_SIZE);
        }
        return false;
    }

    if (config->broadcast_threshold < 0.0F || config->broadcast_threshold > 1.0F) {
        if (error_msg) {
            snprintf(error_msg, error_msg_len,
                    "broadcast_threshold (%.3f) must be in [0.0, 1.0]",
                    config->broadcast_threshold);
        }
        return false;
    }

    if (config->min_salience < 0.0F || config->min_salience > 1.0F) {
        if (error_msg) {
            snprintf(error_msg, error_msg_len,
                    "min_salience (%.3f) must be in [0.0, 1.0]",
                    config->min_salience);
        }
        return false;
    }

    if (config->coherence_window_ms == 0) {
        if (error_msg) {
            snprintf(error_msg, error_msg_len,
                    "coherence_window_ms must be > 0");
        }
        return false;
    }

    if (config->pruning_interval_ms == 0) {
        if (error_msg) {
            snprintf(error_msg, error_msg_len,
                    "pruning_interval_ms must be > 0");
        }
        return false;
    }

    if (config->item_ttl_ms == 0) {
        if (error_msg) {
            snprintf(error_msg, error_msg_len,
                    "item_ttl_ms must be > 0");
        }
        return false;
    }

    return true;
}

//=============================================================================
// Knowledge Graph Self-Awareness Integration
//=============================================================================

/**
 * @brief Query knowledge graph for self-knowledge about collective workspace
 *
 * WHAT: Query knowledge graph for self-knowledge about collective workspace module
 * WHY:  Enable self-awareness by introspecting module's identity in KG
 * HOW:  Query entity, observations, and relations from knowledge graph
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if entity found, 0 if not found or error
 */
int collective_workspace_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) {
        return 0;
    }

    const kg_entity_t* self = kg_reader_get_entity(kg, "Collective_Workspace");
    if (self) {
        LOG_INFO("KG Self-Knowledge: Found entity '%s' of type '%s'",
                 self->name, self->entity_type);
        for (uint32_t i = 0; i < self->num_observations; i++) {
            LOG_DEBUG("  Observation[%u]: %s", i, self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Collective_Workspace");
    if (connections) {
        LOG_INFO("KG Self-Knowledge: Collective_Workspace has %u outgoing connections",
                 connections->count);
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Collective_Workspace");
    if (incoming) {
        LOG_INFO("KG Self-Knowledge: Collective_Workspace has %u incoming connections",
                 incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
