/**
 * @file nimcp_graph_dao.c
 * @brief Data Access Object Pattern Implementation for GPU Graphs
 *
 * WHAT: Concrete DAO implementation for GPU graph storage
 * WHY:  Efficient graph data management with GPU-CPU synchronization
 * HOW:  LRU cache + GPU storage with thread-safe operations
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#include "gpu/graph/nimcp_graph_dao.h"
#include "gpu/graph/nimcp_graph_gpu.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(graph_dao)

//=============================================================================
// Internal Constants
//=============================================================================

#define HASH_TABLE_SIZE 256

//=============================================================================
// Forward Declarations (Internal)
//=============================================================================

// LRU Cache Operations
static nimcp_graph_lru_cache_t* lru_cache_create(size_t capacity);
static void lru_cache_destroy(nimcp_graph_lru_cache_t* cache);
static nimcp_graph_cache_entry_t* lru_cache_get(nimcp_graph_lru_cache_t* cache, int id);
static bool lru_cache_put(nimcp_graph_lru_cache_t* cache, int id, void* data);
static void lru_cache_remove(nimcp_graph_lru_cache_t* cache, int id);
static void lru_cache_touch(nimcp_graph_lru_cache_t* cache, nimcp_graph_cache_entry_t* entry);
static nimcp_graph_cache_entry_t* lru_cache_evict(nimcp_graph_lru_cache_t* cache);

// DAO Operations (vtable implementations)
static int dao_create_impl(void* dao, void* graph_data);
static int dao_read_impl(void* dao, int id, void** out_data);
static int dao_update_impl(void* dao, int id, void* graph_data);
static int dao_delete_impl(void* dao, int id);
static int dao_query_impl(void* dao, nimcp_graph_query_t* query, void** results);
static int dao_batch_insert_impl(void* dao, void** graphs, size_t count);
static int dao_sync_to_gpu_impl(void* dao);
static int dao_sync_from_gpu_impl(void* dao);

// Internal helpers
static uint64_t get_timestamp_us(void);
static int hash_id(int id);
static int find_gpu_slot(nimcp_gpu_graph_dao_t* dao, int id);
static int allocate_gpu_slot(nimcp_gpu_graph_dao_t* dao);

//=============================================================================
// Factory Functions
//=============================================================================

nimcp_gpu_graph_dao_t* nimcp_graph_dao_create_gpu(
    nimcp_gpu_context_t* gpu_context,
    size_t cache_size)
{
    if (!gpu_context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_context is NULL");

        return NULL;
    }

    nimcp_gpu_graph_dao_t* dao = (nimcp_gpu_graph_dao_t*)nimcp_calloc(
        1, sizeof(nimcp_gpu_graph_dao_t));
    if (!dao) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dao is NULL");

        return NULL;
    }

    // Set up vtable
    dao->ops.create = dao_create_impl;
    dao->ops.read = dao_read_impl;
    dao->ops.update = dao_update_impl;
    dao->ops.delete_graph = dao_delete_impl;
    dao->ops.query = dao_query_impl;
    dao->ops.batch_insert = dao_batch_insert_impl;
    dao->ops.sync_to_gpu = dao_sync_to_gpu_impl;
    dao->ops.sync_from_gpu = dao_sync_from_gpu_impl;

    dao->gpu_ctx = gpu_context;

    // Initialize cache
    dao->cache_size = (cache_size > 0) ? cache_size : NIMCP_DAO_DEFAULT_CACHE_SIZE;
    dao->host_cache = lru_cache_create(dao->cache_size);
    if (!dao->host_cache) {
        nimcp_free(dao);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_graph_dao_create_gpu: dao->host_cache is NULL");
        return NULL;
    }

    // Initialize GPU storage
    dao->gpu_capacity = NIMCP_DAO_DEFAULT_GPU_CAPACITY;
    dao->gpu_storage = (void**)nimcp_calloc(dao->gpu_capacity, sizeof(void*));
    dao->gpu_ids = (int*)nimcp_calloc(dao->gpu_capacity, sizeof(int));
    if (!dao->gpu_storage || !dao->gpu_ids) {
        lru_cache_destroy(dao->host_cache);
        nimcp_free(dao->gpu_storage);
        nimcp_free(dao->gpu_ids);
        nimcp_free(dao);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_graph_dao_create_gpu: required parameter is NULL (dao->gpu_storage, dao->gpu_ids)");
        return NULL;
    }

    // Initialize IDs to invalid
    for (size_t i = 0; i < dao->gpu_capacity; i++) {
        dao->gpu_ids[i] = -1;
    }

    dao->gpu_count = 0;
    dao->next_id = 1;

    // Initialize mutex
    if (nimcp_mutex_init(&dao->lock, NULL) != NIMCP_SUCCESS) {
        lru_cache_destroy(dao->host_cache);
        nimcp_free(dao->gpu_storage);
        nimcp_free(dao->gpu_ids);
        nimcp_free(dao);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_graph_dao_create_gpu: validation failed");
        return NULL;
    }

    dao->initialized = true;
    return dao;
}

nimcp_gpu_graph_dao_t* nimcp_graph_dao_create_hybrid(
    nimcp_gpu_context_t* gpu_context,
    size_t gpu_capacity)
{
    nimcp_gpu_graph_dao_t* dao = nimcp_graph_dao_create_gpu(
        gpu_context, NIMCP_DAO_DEFAULT_CACHE_SIZE * 4);  // Larger cache for hybrid

    if (dao && gpu_capacity > 0) {
        // Reallocate GPU storage with custom capacity
        nimcp_free(dao->gpu_storage);
        nimcp_free(dao->gpu_ids);

        dao->gpu_capacity = gpu_capacity;
        dao->gpu_storage = (void**)nimcp_calloc(gpu_capacity, sizeof(void*));
        dao->gpu_ids = (int*)nimcp_calloc(gpu_capacity, sizeof(int));

        if (!dao->gpu_storage || !dao->gpu_ids) {
            nimcp_graph_dao_destroy(dao);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_graph_dao_create_hybrid: required parameter is NULL (dao->gpu_storage, dao->gpu_ids)");
            return NULL;
        }

        for (size_t i = 0; i < gpu_capacity; i++) {
            dao->gpu_ids[i] = -1;
        }
    }

    return dao;
}

void nimcp_graph_dao_destroy(nimcp_gpu_graph_dao_t* dao)
{
    if (!dao) {
        return;
    }

    /* Mark as not initialized BEFORE locking to prevent new operations from
     * entering. All CRUD operations check dao->initialized before acquiring
     * the lock, so this drains concurrent callers safely. */
    dao->initialized = false;

    /* Acquire lock to wait for any in-flight operations to complete */
    nimcp_mutex_lock(&dao->lock);

    // Free GPU-resident graphs
    for (size_t i = 0; i < dao->gpu_capacity; i++) {
        if (dao->gpu_storage[i]) {
            nimcp_gpu_graph_destroy((nimcp_gpu_graph_t*)dao->gpu_storage[i]);
        }
    }

    nimcp_free(dao->gpu_storage);
    dao->gpu_storage = NULL;
    nimcp_free(dao->gpu_ids);
    dao->gpu_ids = NULL;

    if (dao->host_cache) {
        lru_cache_destroy(dao->host_cache);
        dao->host_cache = NULL;
    }

    nimcp_mutex_unlock(&dao->lock);
    nimcp_mutex_destroy(&dao->lock);

    nimcp_free(dao);
}

//=============================================================================
// CRUD Operations
//=============================================================================

int nimcp_graph_dao_create(
    nimcp_gpu_graph_dao_t* dao,
    nimcp_gpu_graph_t* graph)
{
    if (!dao || !graph || !dao->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_graph_dao_create: required parameter is NULL (dao, graph, dao->initialized)");
        return -1;
    }

    nimcp_mutex_lock(&dao->lock);

    // Allocate ID
    int id = dao->next_id++;
    if (dao->next_id >= NIMCP_DAO_MAX_GRAPH_ID) {
        dao->next_id = 1;  // Wrap around
    }

    // Store in GPU storage if space available
    int slot = allocate_gpu_slot(dao);
    if (slot >= 0) {
        dao->gpu_storage[slot] = graph;
        dao->gpu_ids[slot] = id;
        dao->gpu_count++;
    } else {
        // Store in cache only
        if (!lru_cache_put(dao->host_cache, id, graph)) {
            nimcp_mutex_unlock(&dao->lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_graph_dao_create: lru_cache_put is NULL");
            return -1;
        }
    }

    nimcp_mutex_unlock(&dao->lock);
    return id;
}

nimcp_error_t nimcp_graph_dao_read(
    nimcp_gpu_graph_dao_t* dao,
    int id,
    nimcp_gpu_graph_t** out_graph)
{
    if (!dao || !out_graph || !dao->initialized) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    *out_graph = NULL;

    nimcp_mutex_lock(&dao->lock);

    // Check cache first
    nimcp_graph_cache_entry_t* entry = lru_cache_get(dao->host_cache, id);
    if (entry) {
        *out_graph = (nimcp_gpu_graph_t*)entry->graph_data;
        dao->host_cache->hits++;
        nimcp_mutex_unlock(&dao->lock);
        return NIMCP_SUCCESS;
    }

    dao->host_cache->misses++;

    // Check GPU storage
    int slot = find_gpu_slot(dao, id);
    if (slot >= 0) {
        *out_graph = (nimcp_gpu_graph_t*)dao->gpu_storage[slot];

        // Add to cache for faster future access
        lru_cache_put(dao->host_cache, id, *out_graph);

        nimcp_mutex_unlock(&dao->lock);
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_unlock(&dao->lock);
    return NIMCP_ERROR_NOT_FOUND;
}

nimcp_error_t nimcp_graph_dao_update(
    nimcp_gpu_graph_dao_t* dao,
    int id,
    nimcp_gpu_graph_t* graph)
{
    if (!dao || !graph || !dao->initialized) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(&dao->lock);

    // Update in GPU storage
    int slot = find_gpu_slot(dao, id);
    if (slot >= 0) {
        nimcp_gpu_graph_t* old = (nimcp_gpu_graph_t*)dao->gpu_storage[slot];
        if (old != graph) {
            nimcp_gpu_graph_destroy(old);
        }
        dao->gpu_storage[slot] = graph;
    }

    // Update in cache
    nimcp_graph_cache_entry_t* entry = lru_cache_get(dao->host_cache, id);
    if (entry) {
        if (entry->graph_data != graph && entry->graph_data != NULL) {
            // Only free if different and not NULL
            if (slot < 0) {  // Only if not in GPU storage
                nimcp_gpu_graph_destroy((nimcp_gpu_graph_t*)entry->graph_data);
            }
        }
        entry->graph_data = graph;
        entry->dirty = true;
    } else if (slot < 0) {
        // Not in GPU storage or cache - create new entry
        nimcp_mutex_unlock(&dao->lock);
        return NIMCP_ERROR_NOT_FOUND;
    }

    nimcp_mutex_unlock(&dao->lock);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_graph_dao_delete(
    nimcp_gpu_graph_dao_t* dao,
    int id)
{
    if (!dao || !dao->initialized) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(&dao->lock);

    bool found = false;

    // Remove from GPU storage
    int slot = find_gpu_slot(dao, id);
    if (slot >= 0) {
        nimcp_gpu_graph_destroy((nimcp_gpu_graph_t*)dao->gpu_storage[slot]);
        dao->gpu_storage[slot] = NULL;
        dao->gpu_ids[slot] = -1;
        dao->gpu_count--;
        found = true;
    }

    // Remove from cache (don't free if was in GPU storage)
    nimcp_graph_cache_entry_t* entry = lru_cache_get(dao->host_cache, id);
    if (entry) {
        if (!found && entry->graph_data) {
            nimcp_gpu_graph_destroy((nimcp_gpu_graph_t*)entry->graph_data);
        }
        lru_cache_remove(dao->host_cache, id);
        found = true;
    }

    nimcp_mutex_unlock(&dao->lock);
    return found ? NIMCP_SUCCESS : NIMCP_ERROR_NOT_FOUND;
}

//=============================================================================
// Batch Operations
//=============================================================================

size_t nimcp_graph_dao_batch_insert(
    nimcp_gpu_graph_dao_t* dao,
    nimcp_gpu_graph_t** graphs,
    size_t count,
    int* out_ids)
{
    if (!dao || !graphs || count == 0 || !dao->initialized) {
        return 0;
    }

    size_t inserted = 0;
    for (size_t i = 0; i < count; i++) {
        if (graphs[i]) {
            int id = nimcp_graph_dao_create(dao, graphs[i]);
            if (id > 0) {
                if (out_ids) {
                    out_ids[inserted] = id;
                }
                inserted++;
            }
        }
    }

    return inserted;
}

size_t nimcp_graph_dao_batch_read(
    nimcp_gpu_graph_dao_t* dao,
    const int* ids,
    size_t count,
    nimcp_gpu_graph_t** out_graphs)
{
    if (!dao || !ids || !out_graphs || count == 0 || !dao->initialized) {
        return 0;
    }

    size_t read_count = 0;
    for (size_t i = 0; i < count; i++) {
        nimcp_gpu_graph_t* graph = NULL;
        if (nimcp_graph_dao_read(dao, ids[i], &graph) == NIMCP_SUCCESS) {
            out_graphs[read_count++] = graph;
        }
    }

    return read_count;
}

size_t nimcp_graph_dao_batch_delete(
    nimcp_gpu_graph_dao_t* dao,
    const int* ids,
    size_t count)
{
    if (!dao || !ids || count == 0 || !dao->initialized) {
        return 0;
    }

    size_t deleted = 0;
    for (size_t i = 0; i < count; i++) {
        if (nimcp_graph_dao_delete(dao, ids[i]) == NIMCP_SUCCESS) {
            deleted++;
        }
    }

    return deleted;
}

//=============================================================================
// Query Operations
//=============================================================================

nimcp_error_t nimcp_graph_dao_query(
    nimcp_gpu_graph_dao_t* dao,
    nimcp_graph_query_t* query,
    nimcp_graphql_result_t** results)
{
    if (!dao || !query || !results || !dao->initialized) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Create GraphQL executor
    nimcp_graphql_executor_t* exec = nimcp_graphql_executor_create(dao->gpu_ctx);
    if (!exec) {
        return NIMCP_ERROR_NO_MEMORY;
    }

    // Execute query (simplified - would need to iterate over stored graphs)
    nimcp_error_t err = nimcp_graphql_execute_query(exec, query);

    if (err == NIMCP_SUCCESS && query->result) {
        *results = query->result;
        query->result = NULL;  // Transfer ownership
    }

    nimcp_graphql_executor_destroy(exec);
    return err;
}

//=============================================================================
// Filter Parsing and Evaluation
//=============================================================================

/**
 * @brief Filter operator types
 */
typedef enum {
    FILTER_OP_NONE,      /**< No filter or parse error */
    FILTER_OP_EQ,        /**< Equal (=, ==) */
    FILTER_OP_NE,        /**< Not equal (!=) */
    FILTER_OP_LT,        /**< Less than (<) */
    FILTER_OP_LE,        /**< Less than or equal (<=) */
    FILTER_OP_GT,        /**< Greater than (>) */
    FILTER_OP_GE,        /**< Greater than or equal (>=) */
} filter_op_t;

/**
 * @brief Filter field types
 */
typedef enum {
    FILTER_FIELD_NONE,       /**< Unknown field */
    FILTER_FIELD_VERTICES,   /**< num_vertices */
    FILTER_FIELD_EDGES,      /**< num_edges */
    FILTER_FIELD_DENSITY,    /**< graph density (edges / max_edges) */
    FILTER_FIELD_ID,         /**< graph ID */
} filter_field_t;

/**
 * @brief Parsed filter condition
 */
typedef struct {
    filter_field_t field;    /**< Field to filter on */
    filter_op_t op;          /**< Comparison operator */
    double value;            /**< Comparison value */
    bool valid;              /**< Whether parse was successful */
} filter_condition_t;

/**
 * @brief Skip whitespace in string
 */
static const char* skip_whitespace(const char* str) {
    while (*str == ' ' || *str == '\t') {
        str++;
    }
    return str;
}

/**
 * @brief Parse a filter field name
 *
 * Supported fields:
 * - vertices, num_vertices, v: Number of vertices
 * - edges, num_edges, e: Number of edges
 * - density, d: Graph density
 * - id: Graph ID
 */
static filter_field_t parse_filter_field(const char** str) {
    const char* s = skip_whitespace(*str);

    // Match field names (case-insensitive prefix matching)
    if (strncmp(s, "vertices", 8) == 0 || strncmp(s, "num_vertices", 12) == 0) {
        *str = s + (s[0] == 'n' ? 12 : 8);
        return FILTER_FIELD_VERTICES;
    }
    if (s[0] == 'v' && (s[1] == ' ' || s[1] == '<' || s[1] == '>' || s[1] == '=' || s[1] == '!')) {
        *str = s + 1;
        return FILTER_FIELD_VERTICES;
    }

    if (strncmp(s, "edges", 5) == 0 || strncmp(s, "num_edges", 9) == 0) {
        *str = s + (s[0] == 'n' ? 9 : 5);
        return FILTER_FIELD_EDGES;
    }
    if (s[0] == 'e' && (s[1] == ' ' || s[1] == '<' || s[1] == '>' || s[1] == '=' || s[1] == '!')) {
        *str = s + 1;
        return FILTER_FIELD_EDGES;
    }

    if (strncmp(s, "density", 7) == 0) {
        *str = s + 7;
        return FILTER_FIELD_DENSITY;
    }
    if (s[0] == 'd' && (s[1] == ' ' || s[1] == '<' || s[1] == '>' || s[1] == '=' || s[1] == '!')) {
        *str = s + 1;
        return FILTER_FIELD_DENSITY;
    }

    if (strncmp(s, "id", 2) == 0) {
        *str = s + 2;
        return FILTER_FIELD_ID;
    }

    return FILTER_FIELD_NONE;
}

/**
 * @brief Parse a comparison operator
 */
static filter_op_t parse_filter_operator(const char** str) {
    const char* s = skip_whitespace(*str);

    if (s[0] == '=' && s[1] == '=') {
        *str = s + 2;
        return FILTER_OP_EQ;
    }
    if (s[0] == '=') {
        *str = s + 1;
        return FILTER_OP_EQ;
    }
    if (s[0] == '!' && s[1] == '=') {
        *str = s + 2;
        return FILTER_OP_NE;
    }
    if (s[0] == '<' && s[1] == '=') {
        *str = s + 2;
        return FILTER_OP_LE;
    }
    if (s[0] == '<') {
        *str = s + 1;
        return FILTER_OP_LT;
    }
    if (s[0] == '>' && s[1] == '=') {
        *str = s + 2;
        return FILTER_OP_GE;
    }
    if (s[0] == '>') {
        *str = s + 1;
        return FILTER_OP_GT;
    }

    return FILTER_OP_NONE;
}

/**
 * @brief Parse a numeric value
 */
static bool parse_filter_value(const char** str, double* value) {
    const char* s = skip_whitespace(*str);
    char* end = NULL;

    *value = strtod(s, &end);
    if (end == s) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parse_filter_value: validation failed");
        return false;  // No number found
    }

    *str = end;
    return true;
}

/**
 * @brief Parse a complete filter expression
 *
 * Syntax: field operator value
 * Examples:
 *   "vertices > 100"
 *   "edges <= 1000"
 *   "density >= 0.5"
 *   "id = 42"
 */
static filter_condition_t parse_filter(const char* filter) {
    filter_condition_t cond = {
        .field = FILTER_FIELD_NONE,
        .op = FILTER_OP_NONE,
        .value = 0.0,
        .valid = false
    };

    if (!filter || filter[0] == '\0') {
        return cond;  // Empty filter matches all
    }

    const char* s = filter;

    cond.field = parse_filter_field(&s);
    if (cond.field == FILTER_FIELD_NONE) {
        return cond;
    }

    cond.op = parse_filter_operator(&s);
    if (cond.op == FILTER_OP_NONE) {
        return cond;
    }

    if (!parse_filter_value(&s, &cond.value)) {
        return cond;
    }

    cond.valid = true;
    return cond;
}

/**
 * @brief Evaluate a filter condition against a graph
 */
static bool evaluate_filter(
    const filter_condition_t* cond,
    nimcp_gpu_graph_t* graph,
    int graph_id)
{
    if (!cond->valid) {
        return true;  // Invalid filter matches all
    }

    double field_value = 0.0;

    switch (cond->field) {
        case FILTER_FIELD_VERTICES:
            field_value = (double)graph->num_vertices;
            break;

        case FILTER_FIELD_EDGES:
            field_value = (double)graph->num_edges;
            break;

        case FILTER_FIELD_DENSITY:
            if (graph->num_vertices > 1) {
                size_t max_edges = graph->num_vertices * (graph->num_vertices - 1) / 2;
                field_value = (max_edges > 0) ?
                              (double)graph->num_edges / (double)max_edges : 0.0;
            } else {
                field_value = 0.0;
            }
            break;

        case FILTER_FIELD_ID:
            field_value = (double)graph_id;
            break;

        default:
            return true;  // Unknown field matches all
    }

    switch (cond->op) {
        case FILTER_OP_EQ:  return field_value == cond->value;
        case FILTER_OP_NE:  return field_value != cond->value;
        case FILTER_OP_LT:  return field_value < cond->value;
        case FILTER_OP_LE:  return field_value <= cond->value;
        case FILTER_OP_GT:  return field_value > cond->value;
        case FILTER_OP_GE:  return field_value >= cond->value;
        default:            return true;
    }
}

size_t nimcp_graph_dao_find(
    nimcp_gpu_graph_dao_t* dao,
    const char* filter,
    int* out_ids,
    size_t max_results)
{
    if (!dao || !out_ids || max_results == 0 || !dao->initialized) {
        return 0;
    }

    // Parse filter expression
    filter_condition_t cond = parse_filter(filter);

    // If no valid filter, return all IDs
    if (!filter || filter[0] == '\0' || !cond.valid) {
        return nimcp_graph_dao_list_ids(dao, out_ids, max_results);
    }

    nimcp_mutex_lock(&dao->lock);

    size_t count = 0;

    // Iterate over GPU storage and evaluate filter
    for (size_t i = 0; i < dao->gpu_capacity && count < max_results; i++) {
        if (dao->gpu_ids[i] >= 0 && dao->gpu_storage[i]) {
            nimcp_gpu_graph_t* graph = (nimcp_gpu_graph_t*)dao->gpu_storage[i];
            if (evaluate_filter(&cond, graph, dao->gpu_ids[i])) {
                out_ids[count++] = dao->gpu_ids[i];
            }
        }
    }

    nimcp_mutex_unlock(&dao->lock);
    return count;
}

size_t nimcp_graph_dao_count(
    nimcp_gpu_graph_dao_t* dao,
    const char* filter)
{
    if (!dao || !dao->initialized) {
        return 0;
    }

    // Parse filter expression
    filter_condition_t cond = parse_filter(filter);

    // If no valid filter, return total count
    if (!filter || filter[0] == '\0' || !cond.valid) {
        nimcp_mutex_lock(&dao->lock);
        size_t count = dao->gpu_count + (dao->host_cache ? dao->host_cache->count : 0);
        nimcp_mutex_unlock(&dao->lock);
        return count;
    }

    nimcp_mutex_lock(&dao->lock);

    size_t count = 0;

    // Count graphs matching filter in GPU storage
    for (size_t i = 0; i < dao->gpu_capacity; i++) {
        if (dao->gpu_ids[i] >= 0 && dao->gpu_storage[i]) {
            nimcp_gpu_graph_t* graph = (nimcp_gpu_graph_t*)dao->gpu_storage[i];
            if (evaluate_filter(&cond, graph, dao->gpu_ids[i])) {
                count++;
            }
        }
    }

    nimcp_mutex_unlock(&dao->lock);
    return count;
}

//=============================================================================
// Synchronization
//=============================================================================

nimcp_error_t nimcp_graph_dao_sync_to_gpu(nimcp_gpu_graph_dao_t* dao)
{
    if (!dao || !dao->initialized) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(&dao->lock);

    // Sync dirty cache entries to GPU
    // This would involve copying modified graphs from host to device
    // Implementation depends on how graph data is structured

    nimcp_mutex_unlock(&dao->lock);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_graph_dao_sync_from_gpu(nimcp_gpu_graph_dao_t* dao)
{
    if (!dao || !dao->initialized) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(&dao->lock);

    // Sync GPU graphs to host cache
    // This would involve copying graph data from device to host
    // Implementation depends on how graph data is structured

    nimcp_mutex_unlock(&dao->lock);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_graph_dao_flush(nimcp_gpu_graph_dao_t* dao)
{
    if (!dao || !dao->initialized) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Sync first, then clear
    nimcp_error_t err = nimcp_graph_dao_sync_to_gpu(dao);
    if (err != NIMCP_SUCCESS) {
        return err;
    }

    return nimcp_graph_dao_cache_clear(dao, false);
}

//=============================================================================
// Cache Management
//=============================================================================

void nimcp_graph_dao_cache_stats(
    const nimcp_gpu_graph_dao_t* dao,
    uint64_t* out_hits,
    uint64_t* out_misses,
    size_t* out_count)
{
    if (!dao || !dao->host_cache) {
        if (out_hits) *out_hits = 0;
        if (out_misses) *out_misses = 0;
        if (out_count) *out_count = 0;
        return;
    }

    if (out_hits) *out_hits = dao->host_cache->hits;
    if (out_misses) *out_misses = dao->host_cache->misses;
    if (out_count) *out_count = dao->host_cache->count;
}

nimcp_error_t nimcp_graph_dao_cache_clear(
    nimcp_gpu_graph_dao_t* dao,
    bool sync_first)
{
    if (!dao || !dao->initialized) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (sync_first) {
        nimcp_error_t err = nimcp_graph_dao_sync_to_gpu(dao);
        if (err != NIMCP_SUCCESS) {
            return err;
        }
    }

    nimcp_mutex_lock(&dao->lock);

    // Clear cache entries (don't free graphs that are in GPU storage)
    while (dao->host_cache->count > 0) {
        nimcp_graph_cache_entry_t* entry = lru_cache_evict(dao->host_cache);
        if (entry) {
            // Check if graph is in GPU storage
            int slot = find_gpu_slot(dao, entry->graph_id);
            if (slot < 0 && entry->graph_data) {
                // Not in GPU storage, safe to free
                nimcp_gpu_graph_destroy((nimcp_gpu_graph_t*)entry->graph_data);
            }
            nimcp_free(entry);
        }
    }

    nimcp_mutex_unlock(&dao->lock);
    return NIMCP_SUCCESS;
}

size_t nimcp_graph_dao_prefetch(
    nimcp_gpu_graph_dao_t* dao,
    const int* ids,
    size_t count)
{
    if (!dao || !ids || count == 0 || !dao->initialized) {
        return 0;
    }

    size_t prefetched = 0;
    for (size_t i = 0; i < count; i++) {
        nimcp_gpu_graph_t* graph = NULL;
        if (nimcp_graph_dao_read(dao, ids[i], &graph) == NIMCP_SUCCESS) {
            prefetched++;
        }
    }

    return prefetched;
}

//=============================================================================
// Utility Functions
//=============================================================================

bool nimcp_graph_dao_exists(
    const nimcp_gpu_graph_dao_t* dao,
    int id)
{
    if (!dao || !dao->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_graph_dao_exists: required parameter is NULL (dao, dao->initialized)");
        return false;
    }

    nimcp_gpu_graph_dao_t* mut_dao = (nimcp_gpu_graph_dao_t*)dao;
    nimcp_mutex_lock(&mut_dao->lock);

    bool exists = false;

    // Check cache
    nimcp_graph_cache_entry_t* entry = lru_cache_get(mut_dao->host_cache, id);
    if (entry) {
        exists = true;
    } else {
        // Check GPU storage
        int slot = find_gpu_slot(mut_dao, id);
        exists = (slot >= 0);
    }

    nimcp_mutex_unlock(&mut_dao->lock);
    return exists;
}

void nimcp_graph_dao_stats(
    const nimcp_gpu_graph_dao_t* dao,
    size_t* out_total,
    size_t* out_gpu,
    size_t* out_cached)
{
    if (!dao || !dao->initialized) {
        if (out_total) *out_total = 0;
        if (out_gpu) *out_gpu = 0;
        if (out_cached) *out_cached = 0;
        return;
    }

    if (out_gpu) *out_gpu = dao->gpu_count;
    if (out_cached) *out_cached = dao->host_cache ? dao->host_cache->count : 0;
    if (out_total) *out_total = dao->gpu_count +
                                (dao->host_cache ? dao->host_cache->count : 0);
}

size_t nimcp_graph_dao_list_ids(
    const nimcp_gpu_graph_dao_t* dao,
    int* out_ids,
    size_t max_count)
{
    if (!dao || !out_ids || max_count == 0 || !dao->initialized) {
        return 0;
    }

    nimcp_gpu_graph_dao_t* mut_dao = (nimcp_gpu_graph_dao_t*)dao;
    nimcp_mutex_lock(&mut_dao->lock);

    size_t count = 0;

    // List GPU storage IDs
    for (size_t i = 0; i < dao->gpu_capacity && count < max_count; i++) {
        if (dao->gpu_ids[i] >= 0) {
            out_ids[count++] = dao->gpu_ids[i];
        }
    }

    nimcp_mutex_unlock(&mut_dao->lock);
    return count;
}

//=============================================================================
// DAO vtable implementations
//=============================================================================

static int dao_create_impl(void* dao, void* graph_data)
{
    return nimcp_graph_dao_create((nimcp_gpu_graph_dao_t*)dao,
                                   (nimcp_gpu_graph_t*)graph_data);
}

static int dao_read_impl(void* dao, int id, void** out_data)
{
    nimcp_gpu_graph_t* graph = NULL;
    nimcp_error_t err = nimcp_graph_dao_read((nimcp_gpu_graph_dao_t*)dao, id, &graph);
    if (err == NIMCP_SUCCESS) {
        *out_data = graph;
        return NIMCP_SUCCESS;
    }
    return (int)err;
}

static int dao_update_impl(void* dao, int id, void* graph_data)
{
    return (int)nimcp_graph_dao_update((nimcp_gpu_graph_dao_t*)dao, id,
                                        (nimcp_gpu_graph_t*)graph_data);
}

static int dao_delete_impl(void* dao, int id)
{
    return (int)nimcp_graph_dao_delete((nimcp_gpu_graph_dao_t*)dao, id);
}

static int dao_query_impl(void* dao, nimcp_graph_query_t* query, void** results)
{
    nimcp_graphql_result_t* result = NULL;
    nimcp_error_t err = nimcp_graph_dao_query((nimcp_gpu_graph_dao_t*)dao,
                                               query, &result);
    if (err == NIMCP_SUCCESS) {
        *results = result;
    }
    return (int)err;
}

static int dao_batch_insert_impl(void* dao, void** graphs, size_t count)
{
    return (int)nimcp_graph_dao_batch_insert((nimcp_gpu_graph_dao_t*)dao,
                                              (nimcp_gpu_graph_t**)graphs,
                                              count, NULL);
}

static int dao_sync_to_gpu_impl(void* dao)
{
    return (int)nimcp_graph_dao_sync_to_gpu((nimcp_gpu_graph_dao_t*)dao);
}

static int dao_sync_from_gpu_impl(void* dao)
{
    return (int)nimcp_graph_dao_sync_from_gpu((nimcp_gpu_graph_dao_t*)dao);
}

//=============================================================================
// LRU Cache Implementation
//=============================================================================

static nimcp_graph_lru_cache_t* lru_cache_create(size_t capacity)
{
    nimcp_graph_lru_cache_t* cache = (nimcp_graph_lru_cache_t*)nimcp_calloc(
        1, sizeof(nimcp_graph_lru_cache_t));
    if (!cache) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cache is NULL");

        return NULL;
    }

    cache->entries = (nimcp_graph_cache_entry_t**)nimcp_calloc(
        HASH_TABLE_SIZE, sizeof(nimcp_graph_cache_entry_t*));
    if (!cache->entries) {
        nimcp_free(cache);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lru_cache_create: cache->entries is NULL");
        return NULL;
    }

    cache->capacity = capacity;
    cache->count = 0;
    cache->head = NULL;
    cache->tail = NULL;
    cache->hits = 0;
    cache->misses = 0;

    return cache;
}

static void lru_cache_destroy(nimcp_graph_lru_cache_t* cache)
{
    if (!cache) {
        return;
    }

    // Free all entries (walk LRU list)
    nimcp_graph_cache_entry_t* entry = cache->head;
    while (entry) {
        nimcp_graph_cache_entry_t* next = entry->lru_next;
        // Don't free graph_data here - it may be shared with GPU storage
        nimcp_free(entry);
        entry = next;
    }

    nimcp_free(cache->entries);
    nimcp_free(cache);
}

static int hash_id(int id)
{
    return id % HASH_TABLE_SIZE;
}

static nimcp_graph_cache_entry_t* lru_cache_get(nimcp_graph_lru_cache_t* cache, int id)
{
    if (!cache) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cache is NULL");

        return NULL;
    }

    int hash = hash_id(id);
    nimcp_graph_cache_entry_t* entry = cache->entries[hash];

    // Linear probing in hash chain
    while (entry) {
        if (entry->graph_id == id) {
            lru_cache_touch(cache, entry);
            return entry;
        }
        entry = entry->next;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lru_cache_get: validation failed");
    return NULL;
}

static bool lru_cache_put(nimcp_graph_lru_cache_t* cache, int id, void* data)
{
    if (!cache) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lru_cache_put: cache is NULL");
        return false;
    }

    // Check if already exists
    nimcp_graph_cache_entry_t* existing = lru_cache_get(cache, id);
    if (existing) {
        existing->graph_data = data;
        existing->dirty = true;
        return true;
    }

    // Evict if at capacity
    while (cache->count >= cache->capacity) {
        nimcp_graph_cache_entry_t* evicted = lru_cache_evict(cache);
        if (evicted) {
            // Note: caller is responsible for freeing graph_data if needed
            nimcp_free(evicted);
        }
    }

    // Create new entry
    nimcp_graph_cache_entry_t* entry = (nimcp_graph_cache_entry_t*)nimcp_calloc(
        1, sizeof(nimcp_graph_cache_entry_t));
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lru_cache_put: entry is NULL");
        return false;
    }

    entry->graph_id = id;
    entry->graph_data = data;
    entry->access_time = get_timestamp_us();
    entry->access_count = 1;
    entry->dirty = false;

    // Add to hash table (uses next/prev for hash chain)
    int hash = hash_id(id);
    entry->next = cache->entries[hash];
    entry->prev = NULL;
    if (cache->entries[hash]) {
        cache->entries[hash]->prev = entry;
    }
    cache->entries[hash] = entry;

    // Add to LRU list (front, uses lru_next/lru_prev)
    entry->lru_prev = NULL;
    entry->lru_next = cache->head;
    if (cache->head) {
        cache->head->lru_prev = entry;
    }
    cache->head = entry;
    if (!cache->tail) {
        cache->tail = entry;
    }

    cache->count++;
    return true;
}

static void lru_cache_remove(nimcp_graph_lru_cache_t* cache, int id)
{
    if (!cache) {
        return;
    }

    int hash = hash_id(id);
    nimcp_graph_cache_entry_t* entry = cache->entries[hash];
    nimcp_graph_cache_entry_t* prev = NULL;

    while (entry) {
        if (entry->graph_id == id) {
            // Remove from hash chain (uses next/prev)
            if (prev) {
                prev->next = entry->next;
            } else {
                cache->entries[hash] = entry->next;
            }
            if (entry->next) {
                entry->next->prev = entry->prev;
            }

            // Remove from LRU list (uses lru_next/lru_prev)
            if (entry->lru_prev) {
                entry->lru_prev->lru_next = entry->lru_next;
            } else {
                cache->head = entry->lru_next;
            }

            if (entry->lru_next) {
                entry->lru_next->lru_prev = entry->lru_prev;
            } else {
                cache->tail = entry->lru_prev;
            }

            cache->count--;
            nimcp_free(entry);
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

static void lru_cache_touch(nimcp_graph_lru_cache_t* cache, nimcp_graph_cache_entry_t* entry)
{
    if (!cache || !entry) {
        return;
    }

    entry->access_time = get_timestamp_us();
    entry->access_count++;

    // Move to front of LRU list
    if (entry == cache->head) {
        return;  // Already at front
    }

    // Remove from current LRU position
    if (entry->lru_prev) {
        entry->lru_prev->lru_next = entry->lru_next;
    }
    if (entry->lru_next) {
        entry->lru_next->lru_prev = entry->lru_prev;
    } else {
        cache->tail = entry->lru_prev;
    }

    // Insert at front of LRU list
    entry->lru_prev = NULL;
    entry->lru_next = cache->head;
    if (cache->head) {
        cache->head->lru_prev = entry;
    }
    cache->head = entry;
}

static nimcp_graph_cache_entry_t* lru_cache_evict(nimcp_graph_lru_cache_t* cache)
{
    if (!cache || !cache->tail) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lru_cache_evict: required parameter is NULL (cache, cache->tail)");
        return NULL;
    }

    nimcp_graph_cache_entry_t* entry = cache->tail;

    // Remove from LRU list (uses lru_next/lru_prev)
    if (entry->lru_prev) {
        entry->lru_prev->lru_next = NULL;
    } else {
        cache->head = NULL;
    }
    cache->tail = entry->lru_prev;

    // Remove from hash table (uses next/prev)
    int hash = hash_id(entry->graph_id);
    nimcp_graph_cache_entry_t* curr = cache->entries[hash];
    nimcp_graph_cache_entry_t* prev_hash = NULL;

    while (curr) {
        if (curr == entry) {
            if (prev_hash) {
                prev_hash->next = curr->next;
            } else {
                cache->entries[hash] = curr->next;
            }
            if (curr->next) {
                curr->next->prev = prev_hash;
            }
            break;
        }
        prev_hash = curr;
        curr = curr->next;
    }

    cache->count--;
    return entry;
}

//=============================================================================
// Internal Helpers
//=============================================================================

static uint64_t get_timestamp_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static int find_gpu_slot(nimcp_gpu_graph_dao_t* dao, int id)
{
    for (size_t i = 0; i < dao->gpu_capacity; i++) {
        if (dao->gpu_ids[i] == id) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_gpu_slot: validation failed");
    return -1;
}

static int allocate_gpu_slot(nimcp_gpu_graph_dao_t* dao)
{
    for (size_t i = 0; i < dao->gpu_capacity; i++) {
        if (dao->gpu_ids[i] < 0) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "allocate_gpu_slot: validation failed");
    return -1;  // No free slot
}
