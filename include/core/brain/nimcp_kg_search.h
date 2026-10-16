/**
 * @file nimcp_kg_search.h
 * @brief Knowledge Graph Search and Query API
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Rich search and query capabilities for the brain knowledge graph
 * WHY:  Modules need to discover and filter KG content by various criteria
 * HOW:  Indexed search with support for multiple operators and result ranking
 *
 * SEARCH ARCHITECTURE:
 * ```
 * +-------------------------------------------------------------------+
 * |                    KG SEARCH SYSTEM                               |
 * +-------------------------------------------------------------------+
 * |                                                                   |
 * |  +-----------------+    +-----------------+    +---------------+  |
 * |  | Search Index    |    | Query Builder   |    | Result Set    |  |
 * |  |-----------------|    |-----------------|    |---------------|  |
 * |  | - Module index  |    | - Conditions    |    | - Sorted      |  |
 * |  | - Layer index   |    | - Operators     |    | - Paginated   |  |
 * |  | - Hemisphere    |    | - Scope control |    | - Scored      |  |
 * |  | - System        |    | - Pagination    |    | - Timed       |  |
 * |  | - Full-text     |    | - Sorting       |    |               |  |
 * |  +-----------------+    +-----------------+    +---------------+  |
 * |           |                    |                     ^            |
 * |           v                    v                     |            |
 * |  +----------------------------------------------------+          |
 * |  |              Query Execution Engine                 |          |
 * |  |  - Condition matching                               |          |
 * |  |  - AND/OR logic                                     |          |
 * |  |  - Relevance scoring                                |          |
 * |  +-----------------------------------------------------+          |
 * |                                                                   |
 * +-------------------------------------------------------------------+
 * ```
 *
 * SEARCH OPERATORS:
 * - EQUALS: Exact match
 * - CONTAINS: Substring match
 * - STARTS_WITH/ENDS_WITH: Prefix/suffix match
 * - REGEX: Regular expression match
 * - GREATER_THAN/LESS_THAN/BETWEEN: Numeric comparisons
 * - IN: Value in list
 * - HAS_TAG: Tag presence
 * - FULL_TEXT: Full-text search with relevance scoring
 *
 * USAGE:
 * ```c
 * // Create search index
 * kg_search_index_t* idx = kg_search_index_create();
 * kg_search_index_add_module(idx, node_id, &module_meta);
 *
 * // Build query
 * kg_search_query_t* query = kg_search_query_create();
 * kg_search_query_add_condition(query, "subsystem", KG_SEARCH_OP_EQUALS, "perception");
 * kg_search_query_set_scope(query, true, false, false, false);  // modules only
 *
 * // Execute search
 * kg_search_results_t* results = kg_search_execute(idx, query);
 *
 * // Process results
 * for (uint32_t i = 0; i < results->result_count; i++) {
 *     printf("Found: node %u, score %.2f\n",
 *            results->results[i].node_id,
 *            results->results[i].relevance_score);
 * }
 *
 * // Cleanup
 * kg_search_results_free(results);
 * kg_search_query_destroy(query);
 * kg_search_index_destroy(idx);
 * ```
 *
 * THREAD SAFETY: Search index operations are thread-safe via internal locking
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_SEARCH_H
#define NIMCP_KG_SEARCH_H

#include "core/brain/nimcp_brain_kg.h"
#include "core/brain/nimcp_kg_metadata.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum field name length in search conditions */
#define KG_SEARCH_MAX_FIELD_LEN      64

/** Maximum value length in search conditions */
#define KG_SEARCH_MAX_VALUE_LEN      256

/** Maximum conditions per query */
#define KG_SEARCH_MAX_CONDITIONS     32

/** Default result limit */
#define KG_SEARCH_DEFAULT_LIMIT      100

/** Maximum result limit */
#define KG_SEARCH_MAX_LIMIT          1000

/* ============================================================================
 * Search Operator Enumeration
 * ============================================================================ */

/**
 * @brief Search query operators
 *
 * WHAT: Comparison operators for search conditions
 * WHY:  Enable flexible filtering on metadata fields
 * HOW:  Applied during query execution against indexed values
 */
typedef enum {
    KG_SEARCH_OP_EQUALS = 0,         /**< Exact string match */
    KG_SEARCH_OP_NOT_EQUALS,         /**< Not equal */
    KG_SEARCH_OP_CONTAINS,           /**< Substring match (case-insensitive) */
    KG_SEARCH_OP_STARTS_WITH,        /**< Prefix match */
    KG_SEARCH_OP_ENDS_WITH,          /**< Suffix match */
    KG_SEARCH_OP_REGEX,              /**< Regular expression match */
    KG_SEARCH_OP_GREATER_THAN,       /**< Numeric greater than */
    KG_SEARCH_OP_LESS_THAN,          /**< Numeric less than */
    KG_SEARCH_OP_BETWEEN,            /**< Numeric range (uses value and value2) */
    KG_SEARCH_OP_IN,                 /**< Value in comma-separated list */
    KG_SEARCH_OP_HAS_TAG,            /**< Has specified tag */
    KG_SEARCH_OP_FULL_TEXT           /**< Full-text search with relevance scoring */
} kg_search_op_t;

/* ============================================================================
 * Search Result Level Enumeration
 * ============================================================================ */

/**
 * @brief Hierarchy level of search result
 */
typedef enum {
    KG_RESULT_MODULE = 0,            /**< Module-level result */
    KG_RESULT_LAYER,                 /**< Layer-level result */
    KG_RESULT_HEMISPHERE,            /**< Hemisphere-level result */
    KG_RESULT_SYSTEM                 /**< System-level result */
} kg_search_result_level_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Single search condition
 *
 * WHAT: One filter criterion in a search query
 * WHY:  Allow compound queries with multiple filters
 * HOW:  Field + operator + value(s) pattern
 */
typedef struct {
    char field[KG_SEARCH_MAX_FIELD_LEN];  /**< Metadata field to search */
    kg_search_op_t op;                     /**< Comparison operator */
    char value[KG_SEARCH_MAX_VALUE_LEN];   /**< Value to compare */
    char value2[KG_SEARCH_MAX_VALUE_LEN];  /**< Second value (for BETWEEN operator) */
} kg_search_condition_t;

/**
 * @brief Search query (multiple conditions)
 *
 * WHAT: Complete search specification with conditions, scope, and options
 * WHY:  Package all search parameters in one structure
 * HOW:  Conditions array + boolean logic + scope + pagination + sorting
 */
typedef struct {
    /* Conditions */
    kg_search_condition_t* conditions;  /**< Array of search conditions */
    uint32_t condition_count;           /**< Number of conditions */
    bool match_all;                     /**< AND (true) vs OR (false) logic */

    /* Scope - which hierarchy levels to search */
    bool search_modules;                /**< Include modules in search */
    bool search_layers;                 /**< Include layers in search */
    bool search_hemispheres;            /**< Include hemispheres in search */
    bool search_system;                 /**< Include system-level in search */

    /* Pagination */
    uint32_t offset;                    /**< Skip first N results */
    uint32_t limit;                     /**< Maximum results to return */

    /* Sorting */
    char sort_field[KG_SEARCH_MAX_FIELD_LEN]; /**< Field to sort by (empty = relevance) */
    bool sort_ascending;                /**< Sort order (true = ascending) */
} kg_search_query_t;

/**
 * @brief Search result entry
 *
 * WHAT: One matching item from a search
 * WHY:  Return relevant info without full metadata copy
 * HOW:  Node ID + level + metadata pointer + score
 */
typedef struct {
    brain_kg_node_id_t node_id;         /**< KG node ID of match */
    kg_search_result_level_t level;     /**< Hierarchy level of result */
    kg_metadata_t* metadata;            /**< Pointer to metadata (not owned) */
    float relevance_score;              /**< Match relevance 0.0-1.0 */
} kg_search_result_t;

/**
 * @brief Search results container
 *
 * WHAT: Complete search response with results and statistics
 * WHY:  Provide results plus metadata about the search itself
 * HOW:  Results array + counts + timing information
 */
typedef struct {
    kg_search_result_t* results;        /**< Array of search results */
    uint32_t result_count;              /**< Number of results returned */
    uint32_t total_matches;             /**< Total matches before pagination */
    uint64_t search_time_us;            /**< Query execution time in microseconds */
} kg_search_results_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

/**
 * @brief Search index handle (opaque)
 *
 * Internal structure managing indexed metadata for efficient search
 */
typedef struct kg_search_index kg_search_index_t;

/* ============================================================================
 * Search Index Management API
 * ============================================================================ */

/**
 * @brief Create a new search index
 *
 * WHAT: Allocate and initialize empty search index
 * WHY:  Index is required before adding searchable content
 * HOW:  Allocates internal hash tables and inverted indices
 *
 * @return Search index handle or NULL on error
 */
kg_search_index_t* kg_search_index_create(void);

/**
 * @brief Destroy search index and free resources
 *
 * WHAT: Clean up search index
 * WHY:  Prevent memory leaks
 * HOW:  Free all internal structures
 *
 * @param idx Index to destroy (NULL safe)
 */
void kg_search_index_destroy(kg_search_index_t* idx);

/**
 * @brief Add module metadata to search index
 *
 * WHAT: Index a module for searching
 * WHY:  Make module discoverable by its metadata
 * HOW:  Extract indexed fields and add to appropriate indices
 *
 * @param idx Search index handle
 * @param id Module's KG node ID
 * @param meta Module metadata to index
 * @return 0 on success, -1 on error
 */
int kg_search_index_add_module(
    kg_search_index_t* idx,
    brain_kg_node_id_t id,
    const kg_meta_module_t* meta
);

/**
 * @brief Add layer metadata to search index
 *
 * WHAT: Index a cortical layer for searching
 * WHY:  Enable layer-level queries
 * HOW:  Extract and index layer metadata fields
 *
 * @param idx Search index handle
 * @param layer Layer index (0-5 for layers I-VI)
 * @param meta Layer metadata to index
 * @return 0 on success, -1 on error
 */
int kg_search_index_add_layer(
    kg_search_index_t* idx,
    uint8_t layer,
    const kg_layer_metadata_t* meta
);

/**
 * @brief Add hemisphere metadata to search index
 *
 * WHAT: Index a hemisphere for searching
 * WHY:  Enable hemisphere-level queries
 * HOW:  Extract and index hemisphere metadata fields
 *
 * @param idx Search index handle
 * @param hemi Hemisphere identifier (0=LEFT, 1=RIGHT, 2=BILATERAL)
 * @param meta Hemisphere metadata to index
 * @return 0 on success, -1 on error
 */
int kg_search_index_add_hemisphere(
    kg_search_index_t* idx,
    uint8_t hemi,
    const kg_hemisphere_metadata_t* meta
);

/**
 * @brief Add system metadata to search index
 *
 * WHAT: Index system-level metadata for searching
 * WHY:  Enable brain-wide queries
 * HOW:  Extract and index system metadata fields
 *
 * @param idx Search index handle
 * @param meta System metadata to index
 * @return 0 on success, -1 on error
 */
int kg_search_index_add_system(
    kg_search_index_t* idx,
    const kg_system_metadata_t* meta
);

/**
 * @brief Rebuild search index from scratch
 *
 * WHAT: Clear and reconstruct all search indices
 * WHY:  Recovery after corruption or major changes
 * HOW:  Re-index all currently registered metadata
 *
 * @param idx Search index handle
 * @return 0 on success, -1 on error
 */
int kg_search_index_rebuild(kg_search_index_t* idx);

/**
 * @brief Remove a module from the search index
 *
 * WHAT: Remove a module's metadata from search indices
 * WHY:  Keep index in sync when modules are removed
 * HOW:  Remove from all relevant indices
 *
 * @param idx Search index handle
 * @param id Module's KG node ID
 * @return 0 on success, -1 if not found
 */
int kg_search_index_remove_module(
    kg_search_index_t* idx,
    brain_kg_node_id_t id
);

/**
 * @brief Update a module's metadata in the search index
 *
 * WHAT: Re-index a module after metadata changes
 * WHY:  Keep index accurate after updates
 * HOW:  Remove old entries and add new ones
 *
 * @param idx Search index handle
 * @param id Module's KG node ID
 * @param meta Updated module metadata
 * @return 0 on success, -1 on error
 */
int kg_search_index_update_module(
    kg_search_index_t* idx,
    brain_kg_node_id_t id,
    const kg_meta_module_t* meta
);

/**
 * @brief Get statistics about the search index
 *
 * WHAT: Query index size and coverage
 * WHY:  Monitoring and debugging
 * HOW:  Return counts from internal structures
 *
 * @param idx Search index handle
 * @param module_count Output: number of indexed modules
 * @param layer_count Output: number of indexed layers
 * @param hemisphere_count Output: number of indexed hemispheres
 * @param term_count Output: number of unique indexed terms
 * @return 0 on success
 */
int kg_search_index_get_stats(
    const kg_search_index_t* idx,
    uint32_t* module_count,
    uint32_t* layer_count,
    uint32_t* hemisphere_count,
    uint32_t* term_count
);

/* ============================================================================
 * Query Execution API
 * ============================================================================ */

/**
 * @brief Execute a search query
 *
 * WHAT: Run query against search index and return results
 * WHY:  Core search operation
 * HOW:  Match conditions, apply logic, sort, paginate
 *
 * @param idx Search index handle
 * @param query Search query specification
 * @return Search results (caller must free with kg_search_results_free)
 */
kg_search_results_t* kg_search_execute(
    const kg_search_index_t* idx,
    const kg_search_query_t* query
);

/**
 * @brief Free search results
 *
 * WHAT: Deallocate search results container
 * WHY:  Prevent memory leaks
 * HOW:  Free results array and container
 *
 * @param results Results to free (NULL safe)
 */
void kg_search_results_free(kg_search_results_t* results);

/* ============================================================================
 * Query Builder API
 * ============================================================================ */

/**
 * @brief Create a new empty search query
 *
 * WHAT: Allocate and initialize search query with defaults
 * WHY:  Provide structured way to build queries
 * HOW:  Allocate query struct, set sensible defaults
 *
 * Default settings:
 * - match_all = true (AND logic)
 * - All scope flags = true
 * - offset = 0, limit = KG_SEARCH_DEFAULT_LIMIT
 * - sort_ascending = true
 *
 * @return Query handle or NULL on error
 */
kg_search_query_t* kg_search_query_create(void);

/**
 * @brief Destroy search query and free resources
 *
 * WHAT: Clean up search query
 * WHY:  Prevent memory leaks
 * HOW:  Free conditions array and query struct
 *
 * @param query Query to destroy (NULL safe)
 */
void kg_search_query_destroy(kg_search_query_t* query);

/**
 * @brief Add a condition to the query
 *
 * WHAT: Append a search condition
 * WHY:  Build compound queries incrementally
 * HOW:  Allocate condition, copy parameters, append to array
 *
 * @param query Query to modify
 * @param field Metadata field name to search
 * @param op Comparison operator
 * @param value Value to compare against
 * @return 0 on success, -1 on error (e.g., max conditions reached)
 */
int kg_search_query_add_condition(
    kg_search_query_t* query,
    const char* field,
    kg_search_op_t op,
    const char* value
);

/**
 * @brief Add a BETWEEN condition to the query
 *
 * WHAT: Append a range condition
 * WHY:  Numeric range queries need two values
 * HOW:  Same as add_condition but with second value
 *
 * @param query Query to modify
 * @param field Metadata field name to search
 * @param value1 Lower bound
 * @param value2 Upper bound
 * @return 0 on success, -1 on error
 */
int kg_search_query_add_between(
    kg_search_query_t* query,
    const char* field,
    const char* value1,
    const char* value2
);

/**
 * @brief Set search scope (which hierarchy levels to include)
 *
 * WHAT: Configure which levels are included in search
 * WHY:  Allow focused searches on specific levels
 * HOW:  Set scope flags
 *
 * @param query Query to modify
 * @param modules Include modules in search
 * @param layers Include layers in search
 * @param hemispheres Include hemispheres in search
 * @param system Include system-level in search
 * @return 0 on success
 */
int kg_search_query_set_scope(
    kg_search_query_t* query,
    bool modules,
    bool layers,
    bool hemispheres,
    bool system
);

/**
 * @brief Set pagination parameters
 *
 * WHAT: Configure result offset and limit
 * WHY:  Support paginated result retrieval
 * HOW:  Set offset and limit fields
 *
 * @param query Query to modify
 * @param offset Number of results to skip
 * @param limit Maximum results to return
 * @return 0 on success
 */
int kg_search_query_set_pagination(
    kg_search_query_t* query,
    uint32_t offset,
    uint32_t limit
);

/**
 * @brief Set sorting parameters
 *
 * WHAT: Configure result ordering
 * WHY:  Allow sorting by any metadata field
 * HOW:  Set sort field and direction
 *
 * @param query Query to modify
 * @param field Field to sort by (NULL or empty for relevance)
 * @param ascending Sort direction (true = ascending)
 * @return 0 on success
 */
int kg_search_query_set_sort(
    kg_search_query_t* query,
    const char* field,
    bool ascending
);

/**
 * @brief Set boolean logic for conditions
 *
 * WHAT: Configure how conditions combine
 * WHY:  Support both AND and OR queries
 * HOW:  Set match_all flag
 *
 * @param query Query to modify
 * @param match_all true for AND logic, false for OR logic
 * @return 0 on success
 */
int kg_search_query_set_logic(
    kg_search_query_t* query,
    bool match_all
);

/**
 * @brief Clear all conditions from a query
 *
 * WHAT: Remove all conditions, keep other settings
 * WHY:  Reuse query structure for different searches
 * HOW:  Free conditions array, reset count
 *
 * @param query Query to modify
 * @return 0 on success
 */
int kg_search_query_clear_conditions(kg_search_query_t* query);

/**
 * @brief Clone a search query
 *
 * WHAT: Create a deep copy of a query
 * WHY:  Modify query without affecting original
 * HOW:  Allocate new query and copy all fields
 *
 * @param query Query to clone
 * @return Cloned query or NULL on error
 */
kg_search_query_t* kg_search_query_clone(const kg_search_query_t* query);

/* ============================================================================
 * Convenience Search Functions
 * ============================================================================ */

/**
 * @brief Search by tag
 *
 * WHAT: Find all items with a specific tag
 * WHY:  Common search pattern for tag-based organization
 * HOW:  Execute HAS_TAG condition on modules scope
 *
 * @param idx Search index handle
 * @param tag Tag to search for
 * @return Search results (caller must free)
 */
kg_search_results_t* kg_search_by_tag(
    const kg_search_index_t* idx,
    const char* tag
);

/**
 * @brief Search by module type
 *
 * WHAT: Find all modules of a specific type
 * WHY:  Common search pattern for type filtering
 * HOW:  Execute EQUALS condition on module_type field
 *
 * @param idx Search index handle
 * @param module_type Module type (e.g., "SNN", "LNN", "CNN", "COGNITIVE")
 * @return Search results (caller must free)
 */
kg_search_results_t* kg_search_by_type(
    const kg_search_index_t* idx,
    const char* module_type
);

/**
 * @brief Search by subsystem
 *
 * WHAT: Find all modules in a specific subsystem
 * WHY:  Common search pattern for subsystem filtering
 * HOW:  Execute EQUALS condition on subsystem field
 *
 * @param idx Search index handle
 * @param subsystem Subsystem name (e.g., "core", "perception", "cognition")
 * @return Search results (caller must free)
 */
kg_search_results_t* kg_search_by_subsystem(
    const kg_search_index_t* idx,
    const char* subsystem
);

/**
 * @brief Full-text search across all indexed content
 *
 * WHAT: Search names, descriptions, and text fields
 * WHY:  Provide discovery when exact field is unknown
 * HOW:  Execute FULL_TEXT condition on all text fields
 *
 * @param idx Search index handle
 * @param query_text Search text
 * @return Search results with relevance scores (caller must free)
 */
kg_search_results_t* kg_search_full_text(
    const kg_search_index_t* idx,
    const char* query_text
);

/**
 * @brief Search by health score range
 *
 * WHAT: Find modules within a health score range
 * WHY:  Common pattern for health monitoring
 * HOW:  Execute BETWEEN condition on health_score field
 *
 * @param idx Search index handle
 * @param min_health Minimum health score (0.0-1.0)
 * @param max_health Maximum health score (0.0-1.0)
 * @return Search results (caller must free)
 */
kg_search_results_t* kg_search_by_health(
    const kg_search_index_t* idx,
    float min_health,
    float max_health
);

/**
 * @brief Search by hemisphere
 *
 * WHAT: Find all modules in a specific hemisphere
 * WHY:  Common pattern for lateralization queries
 * HOW:  Execute EQUALS condition on hemisphere field
 *
 * @param idx Search index handle
 * @param hemisphere Hemisphere (0=LEFT, 1=RIGHT, 2=BILATERAL)
 * @return Search results (caller must free)
 */
kg_search_results_t* kg_search_by_hemisphere(
    const kg_search_index_t* idx,
    uint8_t hemisphere
);

/**
 * @brief Search by cortical layer
 *
 * WHAT: Find all modules in a specific cortical layer
 * WHY:  Common pattern for layer-based queries
 * HOW:  Execute EQUALS condition on cortical_layer field
 *
 * @param idx Search index handle
 * @param layer Cortical layer (0-5 for layers I-VI)
 * @return Search results (caller must free)
 */
kg_search_results_t* kg_search_by_layer(
    const kg_search_index_t* idx,
    uint8_t layer
);

/**
 * @brief Search by status
 *
 * WHAT: Find all modules with a specific status
 * WHY:  Common pattern for state monitoring
 * HOW:  Execute EQUALS condition on status field
 *
 * @param idx Search index handle
 * @param status Status string (e.g., "running", "paused", "error")
 * @return Search results (caller must free)
 */
kg_search_results_t* kg_search_by_status(
    const kg_search_index_t* idx,
    const char* status
);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

/**
 * @brief Convert search operator to string
 *
 * @param op Search operator
 * @return Human-readable operator name
 */
const char* kg_search_op_to_string(kg_search_op_t op);

/**
 * @brief Convert result level to string
 *
 * @param level Result level
 * @return Human-readable level name
 */
const char* kg_search_result_level_to_string(kg_search_result_level_t level);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_SEARCH_H */
