/**
 * @file nimcp_rcog_context_store.h
 * @brief Context Store for Recursive Cognition - Environment-Based Input Pattern
 *
 * WHAT: Implements RLM's "environment as external variable" pattern
 * WHY:  Input data stored but NEVER directly injected into processing context
 * HOW:  Named variables with query-based access patterns
 *
 * RLM PATTERN:
 * Instead of stuffing all context into a prompt, the context is loaded
 * as a variable that can be queried. This enables:
 * - Working with arbitrarily large contexts
 * - Selective retrieval (only what's needed)
 * - No information loss from summarization
 * - Parallel access by multiple subtasks
 *
 * BIOLOGICAL ANALOGY:
 * - Context store acts like long-term memory
 * - Queries act like retrieval cues
 * - Output limits act like working memory capacity
 * - Salience determines what gets shared with swarm
 *
 * INTEGRATION POINTS:
 * - Working Memory: Can back context store for persistence
 * - Hippocampus: Episodic memory integration
 * - KG Reader: Semantic knowledge queries
 * - Collective Workspace: Swarm sharing via stigmergy
 * - Bio-Async: Predictive prefetching
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 * @version 1.0.0
 */

#ifndef NIMCP_RCOG_CONTEXT_STORE_H
#define NIMCP_RCOG_CONTEXT_STORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "cognitive/recursive/nimcp_rcog_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Context store configuration
 */
typedef struct {
    size_t max_variables;           /**< Max named variables (default: 64) */
    size_t max_variable_size;       /**< Max size per variable in bytes (default: 1MB) */
    size_t max_total_size;          /**< Max total storage (default: 16MB) */
    size_t output_limit_per_query;  /**< Max chars per query result (default: 8192) */
    bool enable_compression;        /**< LZ4 compress large variables */
    size_t compression_threshold;   /**< Compress if size > this (default: 64KB) */
    bool enable_persistence;        /**< Persist across sessions */
    const char* persistence_path;   /**< Path for persistent storage */
    bool enable_access_tracking;    /**< Track variable access patterns */
    bool enable_predictive_prefetch; /**< Prefetch based on access patterns */
} rcog_context_store_config_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default context store configuration
 *
 * @return Configuration with sensible defaults
 */
rcog_context_store_config_t rcog_context_store_default_config(void);

/**
 * @brief Create context store with configuration
 *
 * @param config Configuration (NULL for defaults)
 * @return Context store handle or NULL on failure
 */
rcog_context_store_t* rcog_context_store_create(
    const rcog_context_store_config_t* config
);

/**
 * @brief Create context store with default configuration
 *
 * @return Context store handle or NULL on failure
 */
rcog_context_store_t* rcog_context_store_create_default(void);

/**
 * @brief Destroy context store and free all resources
 *
 * @param store Context store handle (NULL safe)
 */
void rcog_context_store_destroy(rcog_context_store_t* store);

/**
 * @brief Clear all variables from context store
 *
 * @param store Context store handle
 * @return RCOG_OK on success
 */
rcog_error_t rcog_context_store_clear(rcog_context_store_t* store);

//=============================================================================
// Variable Management
//=============================================================================

/**
 * @brief Store a named variable
 *
 * WHAT: Store data as named variable for later query
 * WHY:  RLM pattern - context as environment, not injected
 * HOW:  Copy data, optionally compress, track metadata
 *
 * @param store Context store handle
 * @param name Variable name (max RCOG_MAX_VARIABLE_NAME_LEN chars)
 * @param data Data to store (copied)
 * @param size Size of data in bytes
 * @param dtype Data type
 * @return RCOG_OK on success
 *
 * ERRORS:
 * - RCOG_ERROR_NULL_POINTER: store, name, or data is NULL
 * - RCOG_ERROR_CONTEXT_TOO_LARGE: size exceeds max_variable_size
 * - RCOG_ERROR_CONTEXT_FULL: max_variables or max_total_size exceeded
 * - RCOG_ERROR_OUT_OF_MEMORY: allocation failed
 */
rcog_error_t rcog_context_store_set(
    rcog_context_store_t* store,
    const char* name,
    const void* data,
    size_t size,
    rcog_data_type_t dtype
);

/**
 * @brief Store text as named variable (convenience)
 *
 * @param store Context store handle
 * @param name Variable name
 * @param text Null-terminated text
 * @return RCOG_OK on success
 */
rcog_error_t rcog_context_store_set_text(
    rcog_context_store_t* store,
    const char* name,
    const char* text
);

/**
 * @brief Remove a variable from the store
 *
 * @param store Context store handle
 * @param name Variable name
 * @return RCOG_OK on success, RCOG_ERROR_CONTEXT_NOT_FOUND if not found
 */
rcog_error_t rcog_context_store_remove(
    rcog_context_store_t* store,
    const char* name
);

/**
 * @brief Check if a variable exists
 *
 * @param store Context store handle
 * @param name Variable name
 * @return true if variable exists
 */
bool rcog_context_store_exists(
    const rcog_context_store_t* store,
    const char* name
);

/**
 * @brief Get variable metadata without retrieving data
 *
 * @param store Context store handle
 * @param name Variable name
 * @param metadata Output metadata structure
 * @return RCOG_OK on success
 */
rcog_error_t rcog_context_store_get_metadata(
    const rcog_context_store_t* store,
    const char* name,
    rcog_variable_metadata_t* metadata
);

/**
 * @brief List all variable names
 *
 * @param store Context store handle
 * @param names Output array of names (caller allocates)
 * @param max_names Size of names array
 * @param count Output: actual number of variables
 * @return RCOG_OK on success
 */
rcog_error_t rcog_context_store_list(
    const rcog_context_store_t* store,
    char** names,
    size_t max_names,
    size_t* count
);

//=============================================================================
// Query API (Core RLM Pattern)
//=============================================================================

/**
 * @brief Query variable with access pattern
 *
 * WHAT: Retrieve data from variable using specified access pattern
 * WHY:  Selective retrieval avoids overwhelming processing context
 * HOW:  Apply access pattern, respect output limit, return result
 *
 * @param store Context store handle
 * @param name Variable name
 * @param pattern Access pattern (FULL, SLICE, SEARCH, etc.)
 * @param params Query parameters (can be NULL for defaults)
 * @param result Output result structure
 * @return RCOG_OK on success
 *
 * ACCESS PATTERNS:
 * - FULL: Return entire variable (truncated at output_limit)
 * - SLICE: Return range [start:end]
 * - SEARCH: Find pattern in variable, return matches
 * - HEAD: Return first N items/characters
 * - TAIL: Return last N items/characters
 * - SAMPLE: Random sample of N items
 * - AGGREGATE: Apply aggregation function
 * - METADATA: Return metadata only, no data
 *
 * IMPORTANT: Caller must call rcog_query_result_free() when done!
 */
rcog_error_t rcog_context_store_query(
    rcog_context_store_t* store,
    const char* name,
    rcog_access_pattern_t pattern,
    const rcog_query_params_t* params,
    rcog_query_result_t* result
);

/**
 * @brief Query with helper function (like RLM Python helpers)
 *
 * @param store Context store handle
 * @param name Variable name
 * @param helper Helper function to execute on variable
 * @param helper_context Context for helper function
 * @param result Output result structure
 * @return RCOG_OK on success
 */
rcog_error_t rcog_context_store_exec(
    rcog_context_store_t* store,
    const char* name,
    rcog_helper_fn helper,
    void* helper_context,
    rcog_query_result_t* result
);

/**
 * @brief Search across all variables
 *
 * @param store Context store handle
 * @param pattern Search pattern
 * @param max_results Maximum results to return
 * @param results Output array of results (caller allocates)
 * @param result_count Output: actual number of results
 * @return RCOG_OK on success
 */
rcog_error_t rcog_context_store_search_all(
    rcog_context_store_t* store,
    const char* pattern,
    size_t max_results,
    rcog_query_result_t* results,
    size_t* result_count
);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Context store statistics
 */
typedef struct {
    size_t variable_count;          /**< Current number of variables */
    size_t total_size;              /**< Total storage used (bytes) */
    size_t compressed_size;         /**< Storage after compression */
    size_t query_count;             /**< Total queries executed */
    size_t cache_hits;              /**< Decompression cache hits */
    size_t cache_misses;            /**< Decompression cache misses */
    float avg_query_time_ms;        /**< Average query latency */
    size_t max_variable_size;       /**< Largest variable stored */
} rcog_context_store_stats_t;

/**
 * @brief Get context store statistics
 *
 * @param store Context store handle
 * @param stats Output statistics structure
 * @return RCOG_OK on success
 */
rcog_error_t rcog_context_store_get_stats(
    const rcog_context_store_t* store,
    rcog_context_store_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * @param store Context store handle
 */
void rcog_context_store_reset_stats(rcog_context_store_t* store);

//=============================================================================
// Advanced Features
//=============================================================================

/**
 * @brief Enable predictive access based on access patterns
 *
 * Learns which variables are typically accessed together and
 * prefetches/decompresses proactively.
 *
 * @param store Context store handle
 * @param enable Enable or disable
 * @return RCOG_OK on success
 */
rcog_error_t rcog_context_store_enable_prediction(
    rcog_context_store_t* store,
    bool enable
);

/**
 * @brief Set salience for swarm sharing
 *
 * Variables with salience above broadcast threshold will be
 * shared with collective workspace.
 *
 * @param store Context store handle
 * @param name Variable name
 * @param salience Salience value [0, 1]
 * @return RCOG_OK on success
 */
rcog_error_t rcog_context_store_set_salience(
    rcog_context_store_t* store,
    const char* name,
    float salience
);

/**
 * @brief Mark variable as shared with swarm
 *
 * @param store Context store handle
 * @param name Variable name
 * @param shared Whether to share
 * @return RCOG_OK on success
 */
rcog_error_t rcog_context_store_set_shared(
    rcog_context_store_t* store,
    const char* name,
    bool shared
);

/**
 * @brief Get variables marked for swarm sharing
 *
 * @param store Context store handle
 * @param names Output array of names (caller allocates)
 * @param max_names Size of names array
 * @param count Output: actual number of shared variables
 * @return RCOG_OK on success
 */
rcog_error_t rcog_context_store_get_shared(
    const rcog_context_store_t* store,
    char** names,
    size_t max_names,
    size_t* count
);

//=============================================================================
// Thread Safety
//=============================================================================

/**
 * @brief Lock context store for exclusive access
 *
 * Use when performing multiple operations atomically.
 *
 * @param store Context store handle
 * @return RCOG_OK on success
 */
rcog_error_t rcog_context_store_lock(rcog_context_store_t* store);

/**
 * @brief Unlock context store
 *
 * @param store Context store handle
 * @return RCOG_OK on success
 */
rcog_error_t rcog_context_store_unlock(rcog_context_store_t* store);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RCOG_CONTEXT_STORE_H */
