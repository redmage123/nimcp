//=============================================================================
// nimcp_routing_table.h - Dynamic Neural Routing Rules
//=============================================================================

#ifndef NIMCP_ROUTING_TABLE_H
#define NIMCP_ROUTING_TABLE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_routing_table.h
 * @brief Dynamic routing rules with learning and pruning
 *
 * WHAT: Learnable routing table mapping sources to destinations
 * WHY:  Adaptively route information based on task demands and experience
 * HOW:  Hebbian route strengthening, priority resolution, automatic pruning
 *
 * BIOLOGICAL BASIS:
 * - Thalamocortical loops learn task-specific routing patterns
 * - Hebbian learning: "neurons that fire together, wire together"
 * - Synaptic competition and pruning maintain efficiency
 * - Multi-path routing provides redundancy and flexibility
 * - Dynamic remapping supports cognitive flexibility
 *
 * ALGORITHMS:
 * - Route learning: increment strength on usage (Hebbian)
 * - Route pruning: remove low-strength routes (homeostasis)
 * - Priority resolution: highest priority route wins conflicts
 * - Multi-path support: multiple destinations per source
 * - Conflict resolution: when same dest has multiple sources
 */

// ============================================================================
// CONSTANTS
// ============================================================================

#define ROUTING_TABLE_MAX_ROUTES 10000       // Maximum stored routes
#define ROUTING_TABLE_MAX_PATHS 16           // Max destinations per source
#define ROUTING_MIN_STRENGTH 0.1f            // Minimum strength before pruning
#define ROUTING_STRENGTH_DECAY 0.99f         // Per-update decay factor

// ============================================================================
// TYPES
// ============================================================================

/**
 * @brief Routing rule
 */
typedef struct {
    uint32_t source_id;          // Source module/region
    uint32_t dest_id;            // Destination module/region
    float strength;              // Route strength [0.0, 1.0]
    uint32_t priority;           // Conflict resolution priority
    uint32_t usage_count;        // Times route used
    uint64_t last_used_ms;       // Last usage timestamp
} routing_rule_t;

/**
 * @brief Route query result
 */
typedef struct {
    uint32_t* dest_ids;          // Destination IDs
    float* strengths;            // Route strengths
    uint32_t num_dests;          // Number of destinations
} route_query_t;

/**
 * @brief Routing table configuration
 */
typedef struct {
    uint32_t max_routes;                // Maximum routes
    uint32_t max_paths_per_source;      // Max fan-out
    float min_strength;                 // Pruning threshold
    float strength_decay;               // Decay per update
    bool enable_learning;               // Hebbian strengthening
    bool enable_pruning;                // Automatic pruning
    bool enable_multi_path;             // Allow multiple destinations
    float learning_rate;                // Hebbian learning rate
} routing_table_config_t;

/**
 * @brief Opaque routing table handle
 */
typedef struct routing_table routing_table_t;

// ============================================================================
// API
// ============================================================================

/**
 * @brief Create routing table with configuration
 *
 * WHAT: Initialize dynamic routing table
 * WHY:  Set up data structures for route storage and lookup
 * HOW:  Allocate hash table, initialize learning parameters
 *
 * @param config Table configuration (NULL for defaults)
 * @return Table handle or NULL on failure
 */
routing_table_t* routing_table_create(const routing_table_config_t* config);

/**
 * @brief Destroy routing table and free resources
 */
void routing_table_destroy(routing_table_t* table);

/**
 * @brief Add or update routing rule
 *
 * WHAT: Create/strengthen route from source to destination
 * WHY:  Learn task-specific routing patterns
 * HOW:  If exists, increment strength; else create new route
 *
 * @param table Table handle
 * @param source_id Source identifier
 * @param dest_id Destination identifier
 * @param initial_strength Initial strength if creating new route
 * @return true on success, false on error (table full)
 */
bool routing_table_add_route(routing_table_t* table,
                             uint32_t source_id,
                             uint32_t dest_id,
                             float initial_strength);

/**
 * @brief Query routes for source
 *
 * WHAT: Get all destination routes for source
 * WHY:  Determine where to send signal
 * HOW:  Lookup routes, return above strength threshold
 *
 * @param table Table handle
 * @param source_id Source identifier
 * @param result Output: route query result (caller must free)
 * @return true on success, false if no routes found
 */
bool routing_table_query_routes(const routing_table_t* table,
                                uint32_t source_id,
                                route_query_t* result);

/**
 * @brief Get specific route strength
 *
 * @param table Table handle
 * @param source_id Source identifier
 * @param dest_id Destination identifier
 * @param strength Output: route strength
 * @return true if route exists, false otherwise
 */
bool routing_table_get_strength(const routing_table_t* table,
                                uint32_t source_id,
                                uint32_t dest_id,
                                float* strength);

/**
 * @brief Set route priority
 *
 * WHAT: Assign priority for conflict resolution
 * WHY:  Control routing precedence when multiple routes compete
 * HOW:  Store priority value, use in query results
 *
 * @param table Table handle
 * @param source_id Source identifier
 * @param dest_id Destination identifier
 * @param priority Priority level (higher = more important)
 * @return true on success, false if route not found
 */
bool routing_table_set_priority(routing_table_t* table,
                                uint32_t source_id,
                                uint32_t dest_id,
                                uint32_t priority);

/**
 * @brief Update route usage (Hebbian learning)
 *
 * WHAT: Strengthen route based on usage
 * WHY:  Implement "use it or lose it" learning
 * HOW:  Increment strength, update timestamp, apply decay to others
 *
 * @param table Table handle
 * @param source_id Source identifier
 * @param dest_id Destination identifier
 * @return true on success, false if route not found
 */
bool routing_table_use_route(routing_table_t* table,
                             uint32_t source_id,
                             uint32_t dest_id);

/**
 * @brief Remove specific route
 *
 * @param table Table handle
 * @param source_id Source identifier
 * @param dest_id Destination identifier
 * @return true on success, false if route not found
 */
bool routing_table_remove_route(routing_table_t* table,
                                uint32_t source_id,
                                uint32_t dest_id);

/**
 * @brief Prune weak routes
 *
 * WHAT: Remove routes below strength threshold
 * WHY:  Maintain efficiency, free space for new routes
 * HOW:  Iterate routes, remove if strength < min_strength
 *
 * @param table Table handle
 * @param num_pruned Output: number of routes removed (can be NULL)
 * @return true on success, false on error
 */
bool routing_table_prune(routing_table_t* table, uint32_t* num_pruned);

/**
 * @brief Get table statistics
 *
 * @param table Table handle
 * @param num_routes Output: current route count
 * @param avg_strength Output: average route strength
 * @param total_usage Output: total route usages
 * @return true on success, false on error
 */
bool routing_table_get_stats(const routing_table_t* table,
                             uint32_t* num_routes,
                             float* avg_strength,
                             uint64_t* total_usage);

/**
 * @brief Clear all routes
 *
 * WHAT: Remove all routing rules
 * WHY:  Reset routing table for new task
 * HOW:  Free all routes, reset counters
 */
void routing_table_clear(routing_table_t* table);

/**
 * @brief Free route query result (helper)
 */
void routing_table_free_query(route_query_t* result);

/**
 * @brief Get default configuration
 */
routing_table_config_t routing_table_default_config(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_ROUTING_TABLE_H
