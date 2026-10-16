#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_cortical_hierarchy.c - Cortical Hierarchy Implementation
//=============================================================================
/**
 * @file nimcp_cortical_hierarchy.c
 * @brief Implementation of multi-area cortical hierarchy
 * @version 1.0.0
 * @date 2025-12-15
 *
 * Implementation follows NIMCP coding standards:
 * - WHAT/WHY/HOW documentation for all functions
 * - Guard clauses (early returns)
 * - Functions under 50 lines
 * - Memory safety with nimcp_malloc/nimcp_free
 * - Thread-safe with mutexes
 * - Comprehensive error checking
 */

#include "core/cortical_columns/nimcp_cortical_hierarchy.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <float.h>

#define LOG_MODULE "cortical_hierarchy"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cortical_hierarchy)

// Logging macros
#define HIERARCHY_LOG_ERROR(...) NIMCP_LOGGING_ERROR(__VA_ARGS__)
#define HIERARCHY_LOG_WARN(...) NIMCP_LOGGING_WARN(__VA_ARGS__)
#define HIERARCHY_LOG_INFO(...) NIMCP_LOGGING_INFO(__VA_ARGS__)
#define HIERARCHY_LOG_DEBUG(...) NIMCP_LOGGING_DEBUG(__VA_ARGS__)

// Constants
#define DEFAULT_MAX_AREAS 16
#define DEFAULT_MAX_CONNECTIONS 64
#define DEFAULT_RF_BASE 0.5f  // degrees visual angle for V1
#define DEFAULT_EXPANSION_FACTOR 2.5f
#define NUM_LAYERS 5  // Cortical layers (I, II/III, IV, V, VI)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * WHAT: Internal cortical area structure
 * WHY:  Encapsulates area state, activity, and connectivity
 * HOW:  Stores area properties and dynamic state
 */
struct cortical_area {
    uint32_t id;                     // Unique area identifier
    cortical_area_config_t config;   // Area configuration
    float* activity;                 // Current activity (size = num_hypercolumns)
    float* prediction;               // Predicted activity from feedback
    float prediction_error;          // Sum of prediction errors
    uint32_t num_ff_inputs;          // Count of feedforward inputs
    uint32_t num_fb_inputs;          // Count of feedback inputs
    bool is_active;                  // Area has received input this cycle
};

/**
 * WHAT: Internal inter-area connection structure
 * WHY:  Represents projection from one area to another
 * HOW:  Stores connection parameters and weights
 */
struct inter_area_connection {
    uint32_t id;                     // Unique connection identifier
    uint32_t source_area_id;         // Source area
    uint32_t target_area_id;         // Target area
    connection_type_t type;          // FF, FB, or lateral
    uint32_t source_layer;           // Source layer index [0-4]
    uint32_t target_layer;           // Target layer index [0-4]
    float weight;                    // Connection strength
    float delay_ms;                  // Transmission delay
    bool is_active;                  // Connection enabled
};

/**
 * WHAT: Main cortical hierarchy structure
 * WHY:  Manages all areas and connections
 * HOW:  Coordinates multi-area processing with thread safety
 */
struct cortical_hierarchy {
    cortical_hierarchy_config_t config;
    cortical_area_t** areas;         // Array of area pointers
    uint32_t num_areas;
    inter_area_connection_t** connections;
    uint32_t num_connections;
    uint64_t propagation_count;      // Total propagations executed
    nimcp_mutex_t mutex;             // Thread safety
    bio_module_context_t bio_ctx;    // Bio-async context
    bool bio_async_enabled;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Set canonical laminar connectivity for connection type
 * WHY:  FF and FB have different laminar projection patterns
 * HOW:  Assigns source/target layers based on neuroscience literature
 */
static void set_canonical_layers(
    connection_type_t type,
    uint32_t* source_layer,
    uint32_t* target_layer)
{
    // WHAT: Assign layers based on connection type
    // WHY:  Biological cortex has stereotyped laminar patterns
    // HOW:  Match literature (Markov et al. 2014)

    switch (type) {
        case CONNECTION_TYPE_FEEDFORWARD:
            // FF: L2/3, L4 → L4
            *source_layer = 2;  // Layer IV
            *target_layer = 2;  // Layer IV
            break;

        case CONNECTION_TYPE_FEEDBACK:
            // FB: L5, L6 → L1, L5
            *source_layer = 3;  // Layer V
            *target_layer = 0;  // Layer I
            break;

        case CONNECTION_TYPE_LATERAL:
            // Lateral: L2/3 → L2/3
            *source_layer = 1;  // Layer II/III
            *target_layer = 1;  // Layer II/III
            break;

        default:
            // P2 FIX: Unknown connection type - use safe defaults
            *source_layer = 0;
            *target_layer = 0;
            break;
    }
}

/**
 * WHAT: Compute receptive field size for area
 * WHY:  RF expands hierarchically
 * HOW:  RF = base × expansion^level
 */
static float compute_rf_size(
    float rf_base,
    float expansion_factor,
    uint32_t level)
{
    // WHAT: Calculate RF size from level
    // WHY:  Higher areas integrate over larger spatial regions
    // HOW:  Exponential expansion

    return rf_base * powf(expansion_factor, (float)level);
}

/**
 * WHAT: Find area by ID
 * WHY:  Frequently need to look up areas
 * HOW:  Linear search through area array
 */
static cortical_area_t* find_area_by_id(
    const cortical_hierarchy_t* hierarchy,
    uint32_t area_id)
{
    // Guard: Validate input
    if (!hierarchy || !hierarchy->areas) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_area_by_id: required parameter is NULL (hierarchy, hierarchy->areas)");
        return NULL;
    }

    // Linear search
    for (uint32_t i = 0; i < hierarchy->num_areas; i++) {
        if (hierarchy->areas[i] && hierarchy->areas[i]->id == area_id) {
            return hierarchy->areas[i];
        }
    }

    // P2 FIX: Not finding an area is a normal "not found" result, not an error.
    // Removed false-positive NIMCP_THROW_TO_IMMUNE that treated lookup misses as errors.
    return NULL;
}

/**
 * WHAT: Find connection by ID
 * WHY:  Need to look up connections for removal/modification
 * HOW:  Linear search through connection array
 */
static inter_area_connection_t* find_connection_by_id(
    const cortical_hierarchy_t* hierarchy,
    uint32_t connection_id)
{
    // Guard: Validate input
    if (!hierarchy || !hierarchy->connections) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_connection_by_id: required parameter is NULL (hierarchy, hierarchy->connections)");
        return NULL;
    }

    // Linear search
    for (uint32_t i = 0; i < hierarchy->num_connections; i++) {
        if (hierarchy->connections[i] &&
            hierarchy->connections[i]->id == connection_id) {
            return hierarchy->connections[i];
        }
    }

    // P2 FIX: Not finding a connection is a normal "not found" result, not an error.
    // Removed false-positive NIMCP_THROW_TO_IMMUNE that treated lookup misses as errors.
    return NULL;
}

//=============================================================================
// Core API Implementation
//=============================================================================

cortical_hierarchy_config_t cortical_hierarchy_default_config(void)
{
    // WHAT: Create default configuration
    // WHY:  Provide sensible defaults from neuroscience
    // HOW:  Return pre-configured struct

    cortical_hierarchy_config_t config = {
        .max_areas = DEFAULT_MAX_AREAS,
        .max_connections = DEFAULT_MAX_CONNECTIONS,
        .default_rf_base = DEFAULT_RF_BASE,
        .default_expansion_factor = DEFAULT_EXPANSION_FACTOR,
        .enable_predictive_coding = true,
        .enable_bio_async = true
    };

    return config;
}

cortical_hierarchy_t* cortical_hierarchy_create(
    const cortical_hierarchy_config_t* config)
{
    // Guard: Validate config
    if (!config) {
        HIERARCHY_LOG_ERROR("NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_hierarchy_create: config is NULL");
        return NULL;
    }

    // P2-61 FIX: Validate max_areas and max_connections are non-zero
    // Zero values would cause zero-size malloc and unusable hierarchy
    if (config->max_areas == 0 || config->max_connections == 0) {
        HIERARCHY_LOG_ERROR("max_areas (%u) and max_connections (%u) must be non-zero",
                           config->max_areas, config->max_connections);
        return NULL;
    }

    // WHAT: Allocate hierarchy structure
    // WHY:  Need main container for areas/connections
    // HOW:  Use nimcp_malloc with size checks

    cortical_hierarchy_t* hierarchy =
        (cortical_hierarchy_t*)nimcp_malloc(sizeof(cortical_hierarchy_t));
    if (!hierarchy) {
        HIERARCHY_LOG_ERROR("Failed to allocate hierarchy");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_hierarchy_create: hierarchy is NULL");
        return NULL;
    }

    // Initialize
    memset(hierarchy, 0, sizeof(cortical_hierarchy_t));
    hierarchy->config = *config;

    // WHAT: Allocate area array
    // WHY:  Store all cortical areas
    // HOW:  Array of pointers

    hierarchy->areas = (cortical_area_t**)nimcp_malloc(
        config->max_areas * sizeof(cortical_area_t*));
    if (!hierarchy->areas) {
        HIERARCHY_LOG_ERROR("Failed to allocate area array");
        nimcp_free(hierarchy);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_hierarchy_create: hierarchy->areas is NULL");
        return NULL;
    }
    memset(hierarchy->areas, 0, config->max_areas * sizeof(cortical_area_t*));

    // WHAT: Allocate connection array
    // WHY:  Store all inter-area connections
    // HOW:  Array of pointers

    hierarchy->connections = (inter_area_connection_t**)nimcp_malloc(
        config->max_connections * sizeof(inter_area_connection_t*));
    if (!hierarchy->connections) {
        HIERARCHY_LOG_ERROR("Failed to allocate connection array");
        nimcp_free(hierarchy->areas);
        nimcp_free(hierarchy);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_hierarchy_create: hierarchy->connections is NULL");
        return NULL;
    }
    memset(hierarchy->connections, 0,
           config->max_connections * sizeof(inter_area_connection_t*));

    // Initialize mutex for thread safety
    if (nimcp_mutex_init(&hierarchy->mutex, NULL) != 0) {
        HIERARCHY_LOG_ERROR("Failed to init mutex");
        nimcp_free(hierarchy->connections);
        nimcp_free(hierarchy->areas);
        nimcp_free(hierarchy);
        return NULL;
    }

    HIERARCHY_LOG_INFO("Created cortical hierarchy (max_areas=%u, max_connections=%u)",
                       config->max_areas, config->max_connections);

    return hierarchy;
}

void cortical_hierarchy_destroy(cortical_hierarchy_t* hierarchy)
{
    // Guard: Check for NULL
    if (!hierarchy) {
        return;
    }

    // WHAT: Free all areas
    // WHY:  Prevent memory leaks
    // HOW:  Iterate and free each area's resources

    if (hierarchy->areas) {
        for (uint32_t i = 0; i < hierarchy->num_areas; i++) {
            if (hierarchy->areas[i]) {
                if (hierarchy->areas[i]->activity) {
                    nimcp_free(hierarchy->areas[i]->activity);
                }
                if (hierarchy->areas[i]->prediction) {
                    nimcp_free(hierarchy->areas[i]->prediction);
                }
                nimcp_free(hierarchy->areas[i]);
            }
        }
        nimcp_free(hierarchy->areas);
    }

    // WHAT: Free all connections
    // WHY:  Clean up connection resources
    // HOW:  Iterate and free each connection

    if (hierarchy->connections) {
        for (uint32_t i = 0; i < hierarchy->num_connections; i++) {
            if (hierarchy->connections[i]) {
                nimcp_free(hierarchy->connections[i]);
            }
        }
        nimcp_free(hierarchy->connections);
    }

    // Clean up mutex
    nimcp_mutex_destroy(&hierarchy->mutex);

    // Disconnect bio-async if connected
    if (hierarchy->bio_async_enabled && hierarchy->bio_ctx) {
        bio_router_unregister_module(hierarchy->bio_ctx);
    }

    nimcp_free(hierarchy);
    HIERARCHY_LOG_DEBUG("Destroyed cortical hierarchy");
}

//=============================================================================
// Area Management Implementation
//=============================================================================

int cortical_hierarchy_add_area(
    cortical_hierarchy_t* hierarchy,
    const cortical_area_config_t* config,
    uint32_t* area_id_out)
{
    // Guard: Validate inputs
    if (!hierarchy || !config || !area_id_out) {
        HIERARCHY_LOG_ERROR("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_hierarchy_add_area: required parameter is NULL (hierarchy, config, area_id_out)");
        return -1;
    }

    // Guard: Check capacity
    if (hierarchy->num_areas >= hierarchy->config.max_areas) {
        HIERARCHY_LOG_ERROR("Max areas reached (%u)", hierarchy->config.max_areas);
        return -2;
    }

    // Guard: Validate config
    if (config->num_hypercolumns == 0 || config->neurons_per_hypercolumn == 0) {
        HIERARCHY_LOG_ERROR("Invalid area config (zero hypercolumns/neurons)");
        return -3;
    }

    // WHAT: Allocate new area
    // WHY:  Create area instance
    // HOW:  Allocate and initialize structure

    cortical_area_t* area =
        (cortical_area_t*)nimcp_malloc(sizeof(cortical_area_t));
    if (!area) {
        HIERARCHY_LOG_ERROR("Failed to allocate area");
        return -4;
    }
    memset(area, 0, sizeof(cortical_area_t));

    // Assign ID (sequential)
    area->id = hierarchy->num_areas;
    area->config = *config;

    // WHAT: Allocate activity buffer
    // WHY:  Store current area activation
    // HOW:  Array of floats, one per hypercolumn

    area->activity = (float*)nimcp_malloc(
        config->num_hypercolumns * sizeof(float));
    if (!area->activity) {
        HIERARCHY_LOG_ERROR("Failed to allocate activity buffer");
        nimcp_free(area);
        return -5;
    }
    memset(area->activity, 0, config->num_hypercolumns * sizeof(float));

    // WHAT: Allocate prediction buffer (if predictive coding enabled)
    // WHY:  Store top-down predictions
    // HOW:  Same size as activity

    if (hierarchy->config.enable_predictive_coding) {
        area->prediction = (float*)nimcp_malloc(
            config->num_hypercolumns * sizeof(float));
        if (!area->prediction) {
            HIERARCHY_LOG_ERROR("Failed to allocate prediction buffer");
            nimcp_free(area->activity);
            nimcp_free(area);
            return -6;
        }
        memset(area->prediction, 0, config->num_hypercolumns * sizeof(float));
    }

    // Add to hierarchy
    nimcp_mutex_lock(&hierarchy->mutex);

    // P2 FIX: Scan for NULL slots left by remove_area before appending.
    // This prevents wasting array capacity after areas are removed.
    // num_areas is a high-water mark (iteration bound), not the live count.
    uint32_t slot = hierarchy->num_areas;  // Default: append at end
    for (uint32_t i = 0; i < hierarchy->num_areas; i++) {
        if (hierarchy->areas[i] == NULL) {
            slot = i;
            break;
        }
    }

    hierarchy->areas[slot] = area;
    if (slot == hierarchy->num_areas) {
        hierarchy->num_areas++;  // Only bump high-water mark when appending
    }
    *area_id_out = area->id;

    nimcp_mutex_unlock(&hierarchy->mutex);

    HIERARCHY_LOG_INFO("Added area %u (type=%d, level=%u, hypercolumns=%u)",
                       area->id, config->type, config->hierarchy_level,
                       config->num_hypercolumns);

    return 0;
}

int cortical_hierarchy_remove_area(
    cortical_hierarchy_t* hierarchy,
    uint32_t area_id)
{
    // Guard: Validate hierarchy
    if (!hierarchy) {
        HIERARCHY_LOG_ERROR("NULL hierarchy");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_hierarchy_remove_area: hierarchy is NULL");
        return -1;
    }

    // Guard: Find area
    cortical_area_t* area = find_area_by_id(hierarchy, area_id);
    if (!area) {
        HIERARCHY_LOG_ERROR("Area %u not found", area_id);
        return -2;
    }

    nimcp_mutex_lock(&hierarchy->mutex);

    // WHAT: Remove all connections involving this area
    // WHY:  Prevent dangling connections
    // HOW:  Scan and remove connections referencing area_id

    for (uint32_t i = 0; i < hierarchy->num_connections; i++) {
        inter_area_connection_t* conn = hierarchy->connections[i];
        if (conn && (conn->source_area_id == area_id ||
                     conn->target_area_id == area_id)) {
            nimcp_free(conn);
            hierarchy->connections[i] = NULL;
        }
    }

    // Remove from array FIRST (set to NULL, don't compact)
    // Must do this before freeing to avoid use-after-free
    for (uint32_t i = 0; i < hierarchy->num_areas; i++) {
        if (hierarchy->areas[i] && hierarchy->areas[i]->id == area_id) {
            hierarchy->areas[i] = NULL;
            break;
        }
    }

    // Free area resources AFTER removing from array
    if (area->activity) {
        nimcp_free(area->activity);
        area->activity = NULL;
    }
    if (area->prediction) {
        nimcp_free(area->prediction);
        area->prediction = NULL;
    }
    nimcp_free(area);

    nimcp_mutex_unlock(&hierarchy->mutex);

    HIERARCHY_LOG_INFO("Removed area %u", area_id);
    return 0;
}

uint32_t cortical_hierarchy_get_num_areas(
    const cortical_hierarchy_t* hierarchy)
{
    // Guard: Validate hierarchy
    if (!hierarchy) {
        return 0;
    }

    // P3-58 FIX: Acquire mutex to prevent race with concurrent add/remove
    nimcp_mutex_lock(&((cortical_hierarchy_t*)hierarchy)->mutex);

    // WHAT: Count only non-NULL (live) areas
    // WHY:  remove_area sets slots to NULL but keeps num_areas as iteration bound
    // HOW:  Scan array and count non-NULL entries
    uint32_t count = 0;
    for (uint32_t i = 0; i < hierarchy->num_areas; i++) {
        if (hierarchy->areas[i]) {
            count++;
        }
    }

    nimcp_mutex_unlock(&((cortical_hierarchy_t*)hierarchy)->mutex);
    return count;
}

const cortical_area_config_t* cortical_hierarchy_get_area_config(
    const cortical_hierarchy_t* hierarchy,
    uint32_t area_id)
{
    // Guard: Validate hierarchy
    if (!hierarchy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hierarchy is NULL");
        return NULL;
    }

    // P2 FIX: Protect against data race with concurrent add_area/remove_area.
    // Cast away const for mutex (common C pattern for logically-const operations).
    nimcp_mutex_lock(&((cortical_hierarchy_t*)hierarchy)->mutex);

    // Find area
    cortical_area_t* area = find_area_by_id(hierarchy, area_id);
    if (!area) {
        nimcp_mutex_unlock(&((cortical_hierarchy_t*)hierarchy)->mutex);
        return NULL;
    }

    // P1-54 FIX: Copy the config into a thread-local buffer before releasing
    // the mutex. Previously we returned a pointer to internal data after
    // releasing the lock, which was a use-after-free risk if the area was
    // concurrently removed.
    static _Thread_local cortical_area_config_t config_copy;
    config_copy = area->config;
    nimcp_mutex_unlock(&((cortical_hierarchy_t*)hierarchy)->mutex);
    return &config_copy;
}

//=============================================================================
// Connection Management Implementation
//=============================================================================

int cortical_hierarchy_connect_areas(
    cortical_hierarchy_t* hierarchy,
    const inter_area_connection_config_t* config,
    uint32_t* connection_id_out)
{
    // Guard: Validate inputs
    if (!hierarchy || !config || !connection_id_out) {
        HIERARCHY_LOG_ERROR("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_hierarchy_connect_areas: required parameter is NULL (hierarchy, config, connection_id_out)");
        return -1;
    }

    // Guard: Check capacity
    if (hierarchy->num_connections >= hierarchy->config.max_connections) {
        HIERARCHY_LOG_ERROR("Max connections reached");
        return -2;
    }

    // Guard: Verify source and target areas exist
    cortical_area_t* source = find_area_by_id(hierarchy, config->source_area_id);
    cortical_area_t* target = find_area_by_id(hierarchy, config->target_area_id);
    if (!source || !target) {
        HIERARCHY_LOG_ERROR("Source or target area not found");
        return -3;
    }

    // WHAT: Allocate connection
    // WHY:  Create inter-area projection
    // HOW:  Allocate and configure connection structure

    inter_area_connection_t* conn =
        (inter_area_connection_t*)nimcp_malloc(sizeof(inter_area_connection_t));
    if (!conn) {
        HIERARCHY_LOG_ERROR("Failed to allocate connection");
        return -4;
    }
    memset(conn, 0, sizeof(inter_area_connection_t));

    // Configure connection
    conn->id = hierarchy->num_connections;
    conn->source_area_id = config->source_area_id;
    conn->target_area_id = config->target_area_id;
    conn->type = config->type;
    conn->weight = config->weight;
    conn->delay_ms = config->delay_ms;
    conn->is_active = true;

    // WHAT: Set laminar connectivity
    // WHY:  FF and FB have different layer patterns
    // HOW:  Use canonical or user-specified layers

    if (config->use_canonical_layers) {
        set_canonical_layers(config->type, &conn->source_layer, &conn->target_layer);
    } else {
        conn->source_layer = config->source_layer;
        conn->target_layer = config->target_layer;
    }

    // Add to hierarchy
    nimcp_mutex_lock(&hierarchy->mutex);

    // P1-55 FIX: Move area connection count increments inside the mutex-protected
    // section to prevent data races with concurrent connect/disconnect operations.
    if (config->type == CONNECTION_TYPE_FEEDFORWARD) {
        target->num_ff_inputs++;
    } else if (config->type == CONNECTION_TYPE_FEEDBACK) {
        target->num_fb_inputs++;
    }

    hierarchy->connections[hierarchy->num_connections] = conn;
    hierarchy->num_connections++;
    *connection_id_out = conn->id;

    nimcp_mutex_unlock(&hierarchy->mutex);

    HIERARCHY_LOG_INFO("Connected areas %u→%u (type=%d, L%u→L%u, weight=%.3f)",
                       config->source_area_id, config->target_area_id,
                       config->type, conn->source_layer, conn->target_layer,
                       config->weight);

    return 0;
}

int cortical_hierarchy_disconnect_areas(
    cortical_hierarchy_t* hierarchy,
    uint32_t connection_id)
{
    // Guard: Validate hierarchy
    if (!hierarchy) {
        HIERARCHY_LOG_ERROR("NULL hierarchy");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_hierarchy_disconnect_areas: hierarchy is NULL");
        return -1;
    }

    // Find connection
    inter_area_connection_t* conn = find_connection_by_id(hierarchy, connection_id);
    if (!conn) {
        HIERARCHY_LOG_ERROR("Connection %u not found", connection_id);
        return -2;
    }

    nimcp_mutex_lock(&hierarchy->mutex);

    // P2-60 FIX: Search for the connection by ID to find its actual array index,
    // rather than using connection_id as the index directly. The connection_id
    // may not correspond to the array index if connections have been removed.
    for (uint32_t i = 0; i < hierarchy->num_connections; i++) {
        if (hierarchy->connections[i] && hierarchy->connections[i]->id == connection_id) {
            nimcp_free(hierarchy->connections[i]);
            hierarchy->connections[i] = NULL;
            break;
        }
    }

    nimcp_mutex_unlock(&hierarchy->mutex);

    HIERARCHY_LOG_INFO("Disconnected connection %u", connection_id);
    return 0;
}

int cortical_hierarchy_apply_canonical_connections(
    cortical_hierarchy_t* hierarchy)
{
    // Guard: Validate hierarchy
    if (!hierarchy) {
        HIERARCHY_LOG_ERROR("NULL hierarchy");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_hierarchy_apply_canonical_connections: hierarchy is NULL");
        return -1;
    }

    // WHAT: Create standard visual hierarchy V1→V2→V4→IT
    // WHY:  Quick setup of biological connectivity
    // HOW:  Find areas by type and connect sequentially

    // Find V1, V2, V4, IT areas
    uint32_t v1_id = UINT32_MAX, v2_id = UINT32_MAX;
    uint32_t v4_id = UINT32_MAX, it_id = UINT32_MAX;

    for (uint32_t i = 0; i < hierarchy->num_areas; i++) {
        cortical_area_t* area = hierarchy->areas[i];
        if (!area) continue;

        switch (area->config.type) {
            case CORTICAL_AREA_V1: v1_id = area->id; break;
            case CORTICAL_AREA_V2: v2_id = area->id; break;
            case CORTICAL_AREA_V4: v4_id = area->id; break;
            case CORTICAL_AREA_IT: it_id = area->id; break;
            default: break;
        }
    }

    // WHAT: Connect sequential areas with FF and FB
    // WHY:  Implement bidirectional hierarchy
    // HOW:  Create FF (lower→higher) and FB (higher→lower)

    uint32_t conn_id;
    int ret;

    // V1 → V2
    if (v1_id != UINT32_MAX && v2_id != UINT32_MAX) {
        inter_area_connection_config_t config = {
            .source_area_id = v1_id,
            .target_area_id = v2_id,
            .type = CONNECTION_TYPE_FEEDFORWARD,
            .weight = 0.8f,
            .delay_ms = 10.0f,
            .use_canonical_layers = true
        };
        ret = cortical_hierarchy_connect_areas(hierarchy, &config, &conn_id);
        if (ret != 0) return ret;

        // V2 → V1 (feedback)
        config.source_area_id = v2_id;
        config.target_area_id = v1_id;
        config.type = CONNECTION_TYPE_FEEDBACK;
        config.weight = 0.3f;
        ret = cortical_hierarchy_connect_areas(hierarchy, &config, &conn_id);
        if (ret != 0) return ret;
    }

    // V2 → V4
    if (v2_id != UINT32_MAX && v4_id != UINT32_MAX) {
        inter_area_connection_config_t config = {
            .source_area_id = v2_id,
            .target_area_id = v4_id,
            .type = CONNECTION_TYPE_FEEDFORWARD,
            .weight = 0.8f,
            .delay_ms = 15.0f,
            .use_canonical_layers = true
        };
        ret = cortical_hierarchy_connect_areas(hierarchy, &config, &conn_id);
        if (ret != 0) return ret;

        // V4 → V2 (feedback)
        config.source_area_id = v4_id;
        config.target_area_id = v2_id;
        config.type = CONNECTION_TYPE_FEEDBACK;
        config.weight = 0.3f;
        ret = cortical_hierarchy_connect_areas(hierarchy, &config, &conn_id);
        if (ret != 0) return ret;
    }

    // V4 → IT
    if (v4_id != UINT32_MAX && it_id != UINT32_MAX) {
        inter_area_connection_config_t config = {
            .source_area_id = v4_id,
            .target_area_id = it_id,
            .type = CONNECTION_TYPE_FEEDFORWARD,
            .weight = 0.8f,
            .delay_ms = 20.0f,
            .use_canonical_layers = true
        };
        ret = cortical_hierarchy_connect_areas(hierarchy, &config, &conn_id);
        if (ret != 0) return ret;

        // IT → V4 (feedback)
        config.source_area_id = it_id;
        config.target_area_id = v4_id;
        config.type = CONNECTION_TYPE_FEEDBACK;
        config.weight = 0.3f;
        ret = cortical_hierarchy_connect_areas(hierarchy, &config, &conn_id);
        if (ret != 0) return ret;
    }

    HIERARCHY_LOG_INFO("Applied canonical visual hierarchy connections");
    return 0;
}

//=============================================================================
// Propagation Implementation
//=============================================================================

int cortical_hierarchy_propagate_feedforward(
    cortical_hierarchy_t* hierarchy,
    uint32_t start_level,
    uint32_t end_level)
{
    // Guard: Validate hierarchy
    if (!hierarchy) {
        HIERARCHY_LOG_ERROR("NULL hierarchy");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_hierarchy_propagate_feedforward: hierarchy is NULL");
        return -1;
    }

    // Guard: Validate level range
    if (start_level > end_level) {
        HIERARCHY_LOG_ERROR("Invalid level range (%u > %u)", start_level, end_level);
        return -2;
    }

    // P2-62 FIX: Acquire mutex for propagation duration to prevent
    // concurrent modification of areas/connections during propagation
    nimcp_mutex_lock(&hierarchy->mutex);

    // WHAT: Process areas level by level (ascending)
    // WHY:  Bottom-up processing requires sequential level processing
    // HOW:  For each level, propagate through FF connections

    for (uint32_t level = start_level; level <= end_level; level++) {
        // Find all areas at this level
        for (uint32_t i = 0; i < hierarchy->num_areas; i++) {
            cortical_area_t* target = hierarchy->areas[i];
            if (!target || target->config.hierarchy_level != level) {
                continue;
            }

            // Zero out activity for levels above start (start level has external input)
            if (level > start_level) {
                memset(target->activity, 0,
                       target->config.num_hypercolumns * sizeof(float));
            }

            // Accumulate inputs from all FF connections
            for (uint32_t j = 0; j < hierarchy->num_connections; j++) {
                inter_area_connection_t* conn = hierarchy->connections[j];
                if (!conn || !conn->is_active) continue;
                if (conn->type != CONNECTION_TYPE_FEEDFORWARD) continue;
                if (conn->target_area_id != target->id) continue;

                // Find source area
                cortical_area_t* source =
                    find_area_by_id(hierarchy, conn->source_area_id);
                if (!source) continue;

                // WHAT: Propagate activity through connection
                // WHY:  Feedforward drives target area activation
                // HOW:  Spatial pooling when source > target (larger receptive fields)

                uint32_t src_n = source->config.num_hypercolumns;
                uint32_t tgt_n = target->config.num_hypercolumns;

                if (src_n <= tgt_n) {
                    // Source fits into target: 1:1 mapping
                    for (uint32_t k = 0; k < src_n; k++) {
                        target->activity[k] += conn->weight * source->activity[k];
                    }
                } else {
                    // Source larger than target: pool multiple source columns per target
                    // Each target column receives average of its receptive field
                    uint32_t pool_size = src_n / tgt_n;
                    uint32_t remainder = src_n % tgt_n;
                    for (uint32_t k = 0; k < tgt_n; k++) {
                        float sum = 0.0f;
                        uint32_t start = k * pool_size + (k < remainder ? k : remainder);
                        uint32_t count = pool_size + (k < remainder ? 1 : 0);
                        for (uint32_t s = start; s < start + count && s < src_n; s++) {
                            sum += source->activity[s];
                        }
                        target->activity[k] += conn->weight * (sum / (float)count);
                    }
                }
            }

            target->is_active = true;
        }
    }

    hierarchy->propagation_count++;
    nimcp_mutex_unlock(&hierarchy->mutex);  // P2-62: Release propagation mutex
    return 0;
}

int cortical_hierarchy_propagate_feedback(
    cortical_hierarchy_t* hierarchy,
    uint32_t start_level,
    uint32_t end_level)
{
    // Guard: Validate hierarchy
    if (!hierarchy) {
        HIERARCHY_LOG_ERROR("NULL hierarchy");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_hierarchy_propagate_feedback: hierarchy is NULL");
        return -1;
    }

    // Guard: Validate level range (descending)
    if (start_level < end_level) {
        HIERARCHY_LOG_ERROR("Invalid level range for feedback (%u < %u)",
                           start_level, end_level);
        return -2;
    }

    // P2-62 FIX: Acquire mutex for propagation duration
    nimcp_mutex_lock(&hierarchy->mutex);

    // WHAT: Process areas level by level (descending)
    // WHY:  Top-down modulation requires reverse processing
    // HOW:  For each level, propagate through FB connections

    for (uint32_t level = start_level; level != end_level - 1; level--) {
        // Find all areas at this level
        for (uint32_t i = 0; i < hierarchy->num_areas; i++) {
            cortical_area_t* target = hierarchy->areas[i];
            if (!target || target->config.hierarchy_level != level) {
                continue;
            }

            // Initialize prediction buffer if enabled
            if (hierarchy->config.enable_predictive_coding && target->prediction) {
                memset(target->prediction, 0,
                       target->config.num_hypercolumns * sizeof(float));
            }

            // Accumulate inputs from all FB connections
            for (uint32_t j = 0; j < hierarchy->num_connections; j++) {
                inter_area_connection_t* conn = hierarchy->connections[j];
                if (!conn || !conn->is_active) continue;
                if (conn->type != CONNECTION_TYPE_FEEDBACK) continue;
                if (conn->target_area_id != target->id) continue;

                // Find source area
                cortical_area_t* source =
                    find_area_by_id(hierarchy, conn->source_area_id);
                if (!source) continue;

                // WHAT: Propagate feedback modulation
                // WHY:  Top-down signals provide predictions/attention
                // HOW:  Modulate activity or generate predictions

                uint32_t size = (source->config.num_hypercolumns <
                                target->config.num_hypercolumns)
                    ? source->config.num_hypercolumns
                    : target->config.num_hypercolumns;

                if (hierarchy->config.enable_predictive_coding && target->prediction) {
                    // Accumulate prediction
                    for (uint32_t k = 0; k < size; k++) {
                        target->prediction[k] += conn->weight * source->activity[k];
                    }
                } else {
                    // Direct modulation
                    for (uint32_t k = 0; k < size; k++) {
                        target->activity[k] *= (1.0f + conn->weight * source->activity[k]);
                    }
                }
            }
        }

        // Handle underflow for unsigned
        if (level == 0) break;
    }

    hierarchy->propagation_count++;
    nimcp_mutex_unlock(&hierarchy->mutex);  // P2-62: Release propagation mutex
    return 0;
}

int cortical_hierarchy_compute_prediction_error(
    cortical_hierarchy_t* hierarchy,
    uint32_t area_id,
    float* error_out)
{
    // Guard: Validate inputs
    if (!hierarchy || !error_out) {
        HIERARCHY_LOG_ERROR("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_hierarchy_compute_prediction_error: required parameter is NULL (hierarchy, error_out)");
        return -1;
    }

    // Guard: Check if predictive coding enabled
    if (!hierarchy->config.enable_predictive_coding) {
        HIERARCHY_LOG_ERROR("Predictive coding not enabled");
        return -2;
    }

    // Find area
    cortical_area_t* area = find_area_by_id(hierarchy, area_id);
    if (!area || !area->prediction) {
        HIERARCHY_LOG_ERROR("Area %u not found or no prediction buffer", area_id);
        return -3;
    }

    // WHAT: Compute prediction error
    // WHY:  Error signals drive predictive learning
    // HOW:  Sum absolute differences between actual and predicted

    float total_error = 0.0f;
    for (uint32_t i = 0; i < area->config.num_hypercolumns; i++) {
        float error = fabsf(area->activity[i] - area->prediction[i]);
        total_error += error;
    }

    area->prediction_error = total_error;
    *error_out = total_error;

    return 0;
}

//=============================================================================
// Activity Query Implementation
//=============================================================================

int cortical_hierarchy_set_area_input(
    cortical_hierarchy_t* hierarchy,
    uint32_t area_id,
    const float* activity,
    uint32_t size)
{
    // Guard: Validate inputs
    if (!hierarchy || !activity) {
        HIERARCHY_LOG_ERROR("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_hierarchy_set_area_input: required parameter is NULL (hierarchy, activity)");
        return -1;
    }

    // Find area
    cortical_area_t* area = find_area_by_id(hierarchy, area_id);
    if (!area) {
        HIERARCHY_LOG_ERROR("Area %u not found", area_id);
        return -2;
    }

    // Guard: Validate size
    if (size > area->config.num_hypercolumns) {
        HIERARCHY_LOG_WARN("Input size %u exceeds area capacity %u, truncating",
                          size, area->config.num_hypercolumns);
        size = area->config.num_hypercolumns;
    }

    // WHAT: Copy activity to area buffer
    // WHY:  Set external input for processing
    // HOW:  Direct memory copy

    nimcp_mutex_lock(&hierarchy->mutex);

    memcpy(area->activity, activity, size * sizeof(float));
    area->is_active = true;

    nimcp_mutex_unlock(&hierarchy->mutex);

    return 0;
}

int cortical_hierarchy_get_area_activity(
    const cortical_hierarchy_t* hierarchy,
    uint32_t area_id,
    float* activity_out,
    uint32_t max_size,
    uint32_t* actual_size_out)
{
    // Guard: Validate inputs
    if (!hierarchy || !activity_out || !actual_size_out) {
        HIERARCHY_LOG_ERROR("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_hierarchy_get_area_activity: required parameter is NULL (hierarchy, activity_out, actual_size_out)");
        return -1;
    }

    // Find area
    cortical_area_t* area = find_area_by_id(hierarchy, area_id);
    if (!area) {
        HIERARCHY_LOG_ERROR("Area %u not found", area_id);
        return -2;
    }

    // WHAT: Copy activity from area
    // WHY:  Read out area responses
    // HOW:  Copy up to max_size values

    uint32_t copy_size = (max_size < area->config.num_hypercolumns)
        ? max_size : area->config.num_hypercolumns;

    memcpy(activity_out, area->activity, copy_size * sizeof(float));
    *actual_size_out = copy_size;

    return 0;
}

int cortical_hierarchy_get_receptive_field_size(
    const cortical_hierarchy_t* hierarchy,
    uint32_t area_id,
    float* rf_size_out)
{
    // Guard: Validate inputs
    if (!hierarchy || !rf_size_out) {
        HIERARCHY_LOG_ERROR("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_hierarchy_get_receptive_field_size: required parameter is NULL (hierarchy, rf_size_out)");
        return -1;
    }

    // Find area
    cortical_area_t* area = find_area_by_id(hierarchy, area_id);
    if (!area) {
        HIERARCHY_LOG_ERROR("Area %u not found", area_id);
        return -2;
    }

    // WHAT: Calculate RF size from level
    // WHY:  RF expands hierarchically
    // HOW:  Use area's expansion factor or hierarchy default

    float expansion = (area->config.rf_expansion_factor > 0.0f)
        ? area->config.rf_expansion_factor
        : hierarchy->config.default_expansion_factor;

    *rf_size_out = compute_rf_size(
        hierarchy->config.default_rf_base,
        expansion,
        area->config.hierarchy_level);

    return 0;
}

//=============================================================================
// Statistics Implementation
//=============================================================================

int cortical_hierarchy_get_area_stats(
    const cortical_hierarchy_t* hierarchy,
    uint32_t area_id,
    cortical_area_stats_t* stats_out)
{
    // Guard: Validate inputs
    if (!hierarchy || !stats_out) {
        HIERARCHY_LOG_ERROR("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_hierarchy_get_area_stats: required parameter is NULL (hierarchy, stats_out)");
        return -1;
    }

    // Find area
    cortical_area_t* area = find_area_by_id(hierarchy, area_id);
    if (!area) {
        HIERARCHY_LOG_ERROR("Area %u not found", area_id);
        return -2;
    }

    // WHAT: Compute area statistics
    // WHY:  Monitor area state
    // HOW:  Aggregate activity metrics

    stats_out->area_id = area->id;
    stats_out->type = area->config.type;
    stats_out->hierarchy_level = area->config.hierarchy_level;

    // Compute RF size
    float expansion = (area->config.rf_expansion_factor > 0.0f)
        ? area->config.rf_expansion_factor
        : hierarchy->config.default_expansion_factor;
    stats_out->receptive_field_size = compute_rf_size(
        hierarchy->config.default_rf_base,
        expansion,
        area->config.hierarchy_level);

    // Compute activity statistics
    float sum = 0.0f, max_val = -FLT_MAX;
    uint32_t active_count = 0;

    for (uint32_t i = 0; i < area->config.num_hypercolumns; i++) {
        float val = area->activity[i];
        sum += val;
        if (val > max_val) max_val = val;
        if (val > 0.01f) active_count++;  // Threshold for "active"
    }

    stats_out->mean_activity = sum / area->config.num_hypercolumns;
    stats_out->peak_activity = max_val;
    stats_out->num_active_columns = active_count;
    stats_out->num_ff_inputs = area->num_ff_inputs;
    stats_out->num_fb_inputs = area->num_fb_inputs;

    return 0;
}

int cortical_hierarchy_get_stats(
    const cortical_hierarchy_t* hierarchy,
    cortical_hierarchy_stats_t* stats_out)
{
    // Guard: Validate inputs
    if (!hierarchy || !stats_out) {
        HIERARCHY_LOG_ERROR("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_hierarchy_get_stats: required parameter is NULL (hierarchy, stats_out)");
        return -1;
    }

    // WHAT: Aggregate hierarchy-wide statistics
    // WHY:  Monitor overall system state
    // HOW:  Count areas, connections, compute aggregates

    memset(stats_out, 0, sizeof(cortical_hierarchy_stats_t));

    // Count live (non-NULL) areas - remove_area sets slots to NULL
    stats_out->num_areas = cortical_hierarchy_get_num_areas(hierarchy);
    stats_out->num_connections = hierarchy->num_connections;
    stats_out->total_propagations = hierarchy->propagation_count;

    uint32_t max_level = 0;
    uint32_t ff_count = 0, fb_count = 0;
    float total_error = 0.0f;

    // Count connection types
    for (uint32_t i = 0; i < hierarchy->num_connections; i++) {
        inter_area_connection_t* conn = hierarchy->connections[i];
        if (!conn) continue;

        if (conn->type == CONNECTION_TYPE_FEEDFORWARD) {
            ff_count++;
        } else if (conn->type == CONNECTION_TYPE_FEEDBACK) {
            fb_count++;
        }
    }

    // Find max level and accumulate errors
    for (uint32_t i = 0; i < hierarchy->num_areas; i++) {
        cortical_area_t* area = hierarchy->areas[i];
        if (!area) continue;

        if (area->config.hierarchy_level > max_level) {
            max_level = area->config.hierarchy_level;
        }

        total_error += area->prediction_error;
    }

    stats_out->num_ff_connections = ff_count;
    stats_out->num_fb_connections = fb_count;
    stats_out->max_hierarchy_level = max_level;
    stats_out->total_prediction_error = total_error;

    return 0;
}

//=============================================================================
// Bio-Async Integration Implementation
//=============================================================================

int cortical_hierarchy_connect_bio_async(cortical_hierarchy_t* hierarchy)
{
    // Guard: Validate hierarchy
    if (!hierarchy) {
        HIERARCHY_LOG_ERROR("NULL hierarchy");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_hierarchy_connect_bio_async: hierarchy is NULL");
        return -1;
    }

    // Guard: Check if already connected
    if (hierarchy->bio_async_enabled) {
        HIERARCHY_LOG_WARN("Bio-async already connected");
        return 0;
    }

    // Guard: Check if bio-async router available
    if (!bio_router_is_initialized()) {
        HIERARCHY_LOG_WARN("Bio-async router not initialized");
        return -2;
    }

    // WHAT: Register with bio-async router
    // WHY:  Enable inter-area messaging
    // HOW:  Register module with router

    bio_module_info_t info = {
        .module_id = BIO_MODULE_CORTICAL_HIERARCHY,
        .module_name = "cortical_hierarchy",
        .inbox_capacity = 64,
        .user_data = hierarchy
    };

    hierarchy->bio_ctx = bio_router_register_module(&info);
    if (!hierarchy->bio_ctx) {
        HIERARCHY_LOG_ERROR("Failed to register with bio-async router");
        return -3;
    }

    hierarchy->bio_async_enabled = true;
    HIERARCHY_LOG_INFO("Connected to bio-async router");

    return 0;
}

int cortical_hierarchy_disconnect_bio_async(cortical_hierarchy_t* hierarchy)
{
    // Guard: Validate hierarchy
    if (!hierarchy) {
        HIERARCHY_LOG_ERROR("NULL hierarchy");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_hierarchy_disconnect_bio_async: hierarchy is NULL");
        return -1;
    }

    // Guard: Check if connected
    if (!hierarchy->bio_async_enabled) {
        return 0;
    }

    // WHAT: Unregister from bio-async router
    // WHY:  Clean shutdown
    // HOW:  Call router unregister

    if (hierarchy->bio_ctx) {
        bio_router_unregister_module(hierarchy->bio_ctx);
        hierarchy->bio_ctx = NULL;
    }

    hierarchy->bio_async_enabled = false;
    HIERARCHY_LOG_INFO("Disconnected from bio-async router");

    return 0;
}

bool cortical_hierarchy_is_bio_async_connected(
    const cortical_hierarchy_t* hierarchy)
{
    // Guard: Validate hierarchy
    if (!hierarchy) {
        return false;
    }

    return hierarchy->bio_async_enabled;
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

/**
 * WHAT: Query knowledge graph for cortical hierarchy module self-knowledge
 * WHY:  Enable self-awareness and introspection about this module's role
 * HOW:  Query KG for entity info, log observations, check relations
 */
int cortical_hierarchy_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Cortical_Hierarchy_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            HIERARCHY_LOG_DEBUG("Cortical hierarchy self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Cortical_Hierarchy_Module");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Cortical_Hierarchy_Module");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
