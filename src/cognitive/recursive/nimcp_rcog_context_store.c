/**
 * @file nimcp_rcog_context_store.c
 * @brief Context Store Implementation for Recursive Cognition
 *
 * WHAT: Implements RLM's "environment as external variable" pattern
 * WHY:  Input data stored but NEVER directly injected into processing context
 * HOW:  Hash table of named variables with query-based access patterns
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 * @version 1.0.0
 */

#include "cognitive/recursive/nimcp_rcog_context_store.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_time.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(rcog_context_store, MESH_ADAPTER_CATEGORY_COGNITIVE)

void rcog_context_store_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_rcog_context_store_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int rcog_context_store_training_begin(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_context_store_training_begin: NULL argument");
        return -1;
    }
    rcog_context_store_heartbeat_instance(
        g_rcog_context_store_instance_health_agent,
        "rcog_ctx_training_begin", 0.0f);
    (void)ctx;
    return 0;
}

int rcog_context_store_training_step(void* ctx, float progress) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_context_store_training_step: NULL argument");
        return -1;
    }
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    rcog_context_store_heartbeat_instance(
        g_rcog_context_store_instance_health_agent,
        "rcog_ctx_training_step", clamped);
    (void)ctx;
    return 0;
}

int rcog_context_store_training_end(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_context_store_training_end: NULL argument");
        return -1;
    }
    rcog_context_store_heartbeat_instance(
        g_rcog_context_store_instance_health_agent,
        "rcog_ctx_training_end", 1.0f);
    (void)ctx;
    return 0;
}

//=============================================================================
// Internal Constants
//=============================================================================

#define HASH_TABLE_SIZE 128
#define DECOMPRESSION_CACHE_SIZE 4

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Single context variable entry
 */
typedef struct rcog_variable {
    char name[RCOG_MAX_VARIABLE_NAME_LEN];
    void* data;                     /**< Raw or compressed data */
    size_t size;                    /**< Size of data */
    size_t original_size;           /**< Size before compression */
    rcog_data_type_t dtype;
    bool compressed;
    bool shared_with_swarm;
    float salience;
    uint64_t created_ms;
    uint64_t accessed_ms;
    uint32_t access_count;
    struct rcog_variable* next;     /**< Hash chain */
} rcog_variable_t;

/**
 * @brief Decompression cache entry
 */
typedef struct {
    char name[RCOG_MAX_VARIABLE_NAME_LEN];
    void* decompressed_data;
    size_t decompressed_size;
    uint64_t last_used_ms;
} rcog_cache_entry_t;

/**
 * @brief Context store internal structure
 */
struct rcog_context_store {
    /* Configuration */
    rcog_context_store_config_t config;

    /* Hash table of variables */
    rcog_variable_t* buckets[HASH_TABLE_SIZE];

    /* Statistics */
    size_t variable_count;
    size_t total_size;
    size_t compressed_size;
    uint64_t query_count;
    uint64_t cache_hits;
    uint64_t cache_misses;
    double total_query_time_ms;

    /* Decompression cache */
    rcog_cache_entry_t cache[DECOMPRESSION_CACHE_SIZE];

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Predictive prefetch state */
    bool prediction_enabled;
    char last_accessed[RCOG_MAX_VARIABLE_NAME_LEN];
};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_current_time_ms(void)
{
    return nimcp_platform_time_monotonic_ms();
}

/**
 * @brief Simple string hash function (djb2)
 */
static uint32_t hash_string(const char* str)
{
    uint32_t hash = 5381;
    int c = 0;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + (uint32_t)c;
    }
    return hash % HASH_TABLE_SIZE;
}

/**
 * @brief Find variable by name
 */
static rcog_variable_t* find_variable(
    const rcog_context_store_t* store,
    const char* name)
{
    uint32_t bucket = hash_string(name);
    rcog_variable_t* var = store->buckets[bucket];

    while (var) {
        if (strcmp(var->name, name) == 0) {
            return var;
        }
        var = var->next;
    }

    return NULL;
}

/**
 * @brief Create a new variable entry
 */
static rcog_variable_t* create_variable(
    const char* name,
    const void* data,
    size_t size,
    rcog_data_type_t dtype)
{
    rcog_variable_t* var = nimcp_calloc(1, sizeof(rcog_variable_t));
    if (!var) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate var");

        return NULL;

    }

    strncpy(var->name, name, RCOG_MAX_VARIABLE_NAME_LEN - 1);
    var->name[RCOG_MAX_VARIABLE_NAME_LEN - 1] = '\0';

    var->data = nimcp_malloc(size);
    if (!var->data) {
        nimcp_free(var);
        var = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_variable: var->data is NULL");
        return NULL;
    }

    memcpy(var->data, data, size);
    var->size = size;
    var->original_size = size;
    var->dtype = dtype;
    var->compressed = false;
    var->shared_with_swarm = false;
    var->salience = 0.0f;
    var->created_ms = get_current_time_ms();
    var->accessed_ms = var->created_ms;
    var->access_count = 0;
    var->next = NULL;

    return var;
}

/**
 * @brief Destroy a variable entry
 */
static void destroy_variable(rcog_variable_t* var)
{
    if (var) {
        if (var->data) {
            nimcp_free(var->data);
        }
        nimcp_free(var);
        var = NULL;
    }
}

/**
 * @brief Count items in text (lines or characters)
 */
static size_t count_text_items(const char* text, size_t size)
{
    if (!text || size == 0) return 0;

    /* Count lines for multi-line text, characters otherwise */
    size_t lines = 1;
    for (size_t i = 0; i < size && text[i]; i++) {
        if (text[i] == '\n') lines++;
    }
    return (lines > 1) ? lines : size;
}

/**
 * @brief Get slice of text data
 */
static rcog_error_t get_text_slice(
    const char* text,
    size_t text_size,
    size_t start,
    size_t end,
    size_t output_limit,
    rcog_query_result_t* result)
{
    if (start >= text_size) {
        result->data = NULL;
        result->size = 0;
        result->truncated = false;
        return RCOG_OK;
    }

    if (end > text_size) end = text_size;
    if (end <= start) {
        result->data = NULL;
        result->size = 0;
        return RCOG_OK;
    }

    size_t slice_size = end - start;
    bool truncated = false;

    if (output_limit > 0 && slice_size > output_limit) {
        slice_size = output_limit;
        truncated = true;
    }

    char* slice = nimcp_malloc(slice_size + 1);
    if (!slice) return RCOG_ERROR_OUT_OF_MEMORY;

    memcpy(slice, text + start, slice_size);
    slice[slice_size] = '\0';

    result->data = slice;
    result->size = slice_size;
    result->dtype = RCOG_DTYPE_TEXT;
    result->owns_data = true;
    result->truncated = truncated;
    result->total_size = end - start;

    return RCOG_OK;
}

/**
 * @brief Search for pattern in text
 */
static rcog_error_t search_text(
    const char* text,
    size_t text_size,
    const char* pattern,
    size_t output_limit,
    rcog_query_result_t* result)
{
    if (!pattern || !pattern[0]) {
        return RCOG_ERROR_INVALID_QUERY;
    }

    size_t pattern_len = strlen(pattern);
    size_t match_count = 0;
    size_t buffer_size = 1024;
    size_t buffer_used = 0;

    char* buffer = nimcp_malloc(buffer_size);
    if (!buffer) return RCOG_ERROR_OUT_OF_MEMORY;

    /* Simple substring search - find all matches */
    for (size_t i = 0; i + pattern_len <= text_size; i++) {
        if (strncmp(text + i, pattern, pattern_len) == 0) {
            match_count++;

            /* Extract context around match (line containing match) */
            size_t line_start = i;
            while (line_start > 0 && text[line_start - 1] != '\n') {
                line_start--;
            }

            size_t line_end = i + pattern_len;
            while (line_end < text_size && text[line_end] != '\n') {
                line_end++;
            }

            size_t line_len = line_end - line_start;

            /* Grow buffer if needed */
            if (buffer_used + line_len + 2 > buffer_size) {
                if (output_limit > 0 && buffer_used >= output_limit) {
                    break;  /* Hit output limit */
                }
                buffer_size *= 2;
                char* new_buffer = nimcp_realloc(buffer, buffer_size);
                if (!new_buffer) {
                    nimcp_free(buffer);
                    buffer = NULL;
                    return RCOG_ERROR_OUT_OF_MEMORY;
                }
                buffer = new_buffer;
            }

            /* Append match line */
            memcpy(buffer + buffer_used, text + line_start, line_len);
            buffer_used += line_len;
            buffer[buffer_used++] = '\n';

            /* Skip to end of current match */
            i = line_end;
        }
    }

    buffer[buffer_used] = '\0';

    result->data = buffer;
    result->size = buffer_used;
    result->dtype = RCOG_DTYPE_TEXT;
    result->owns_data = true;
    result->truncated = (output_limit > 0 && buffer_used >= output_limit);
    result->match_count = match_count;
    result->found = (match_count > 0);

    return RCOG_OK;
}

//=============================================================================
// Public API Implementation
//=============================================================================

rcog_context_store_config_t rcog_context_store_default_config(void)
{
    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_default_config", 0.0f);


    rcog_context_store_config_t config = {0};

    config.max_variables = RCOG_MAX_VARIABLES;
    config.max_variable_size = RCOG_MAX_VARIABLE_SIZE;
    config.max_total_size = 16 * 1024 * 1024;  /* 16 MB */
    config.output_limit_per_query = RCOG_DEFAULT_OUTPUT_LIMIT;
    config.enable_compression = true;
    config.compression_threshold = 64 * 1024;  /* 64 KB */
    config.enable_persistence = false;
    config.persistence_path = NULL;
    config.enable_access_tracking = true;
    config.enable_predictive_prefetch = false;

    return config;
}

rcog_context_store_t* rcog_context_store_create(
    const rcog_context_store_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_create", 0.0f);


    rcog_context_store_t* store = nimcp_calloc(1, sizeof(rcog_context_store_t));
    if (!store) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate store");

        return NULL;

    }

    if (config) {
        store->config = *config;
    } else {
        store->config = rcog_context_store_default_config();
    }

    /* Initialize hash table buckets */
    memset(store->buckets, 0, sizeof(store->buckets));

    /* Initialize statistics */
    store->variable_count = 0;
    store->total_size = 0;
    store->compressed_size = 0;
    store->query_count = 0;
    store->cache_hits = 0;
    store->cache_misses = 0;
    store->total_query_time_ms = 0.0;

    /* Initialize cache */
    memset(store->cache, 0, sizeof(store->cache));

    /* Create mutex */
    mutex_attr_t attr = {.type = MUTEX_TYPE_RECURSIVE};
    store->mutex = nimcp_mutex_create(&attr);
    if (!store->mutex) {
        nimcp_free(store);
        store = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rcog_context_store_create: store->mutex is NULL");
        return NULL;
    }

    store->prediction_enabled = store->config.enable_predictive_prefetch;
    store->last_accessed[0] = '\0';

    return store;
}

rcog_context_store_t* rcog_context_store_create_default(void)
{
    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_create_default", 0.0f);


    return rcog_context_store_create(NULL);
}

void rcog_context_store_destroy(rcog_context_store_t* store)
{
    if (!store) return;

    /* Clear all variables */
    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_destroy", 0.0f);


    rcog_context_store_clear(store);

    /* Free cache entries */
    for (int i = 0; i < DECOMPRESSION_CACHE_SIZE; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && DECOMPRESSION_CACHE_SIZE > 256) {
            rcog_context_store_heartbeat("rcog_context_loop",
                             (float)(i + 1) / (float)DECOMPRESSION_CACHE_SIZE);
        }

        if (store->cache[i].decompressed_data) {
            nimcp_free(store->cache[i].decompressed_data);
        }
    }

    /* Destroy mutex */
    if (store->mutex) {
        nimcp_mutex_destroy(store->mutex);
    }

    nimcp_free(store);
    store = NULL;
}

rcog_error_t rcog_context_store_clear(rcog_context_store_t* store)
{
    if (!store) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_clear", 0.0f);


    nimcp_mutex_lock(store->mutex);

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && HASH_TABLE_SIZE > 256) {
            rcog_context_store_heartbeat("rcog_context_loop",
                             (float)(i + 1) / (float)HASH_TABLE_SIZE);
        }

        rcog_variable_t* var = store->buckets[i];
        while (var) {
            rcog_variable_t* next = var->next;
            destroy_variable(var);
            var = next;
        }
        store->buckets[i] = NULL;
    }

    store->variable_count = 0;
    store->total_size = 0;
    store->compressed_size = 0;

    nimcp_mutex_unlock(store->mutex);

    return RCOG_OK;
}

rcog_error_t rcog_context_store_set(
    rcog_context_store_t* store,
    const char* name,
    const void* data,
    size_t size,
    rcog_data_type_t dtype)
{
    if (!store || !name || !data) return RCOG_ERROR_NULL_POINTER;
    if (strlen(name) >= RCOG_MAX_VARIABLE_NAME_LEN) return RCOG_ERROR_INVALID_CONFIG;
    if (size > store->config.max_variable_size) return RCOG_ERROR_CONTEXT_TOO_LARGE;

    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_set", 0.0f);


    nimcp_mutex_lock(store->mutex);

    /* Check if variable already exists */
    rcog_variable_t* existing = find_variable(store, name);
    if (existing) {
        /* Update existing variable */
        void* new_data = nimcp_malloc(size);
        if (!new_data) {
            nimcp_mutex_unlock(store->mutex);
            return RCOG_ERROR_OUT_OF_MEMORY;
        }

        store->total_size -= existing->size;
        nimcp_free(existing->data);

        memcpy(new_data, data, size);
        existing->data = new_data;
        existing->size = size;
        existing->original_size = size;
        existing->dtype = dtype;
        existing->compressed = false;
        existing->accessed_ms = get_current_time_ms();

        store->total_size += size;

        nimcp_mutex_unlock(store->mutex);
        return RCOG_OK;
    }

    /* Check capacity limits */
    if (store->variable_count >= store->config.max_variables) {
        nimcp_mutex_unlock(store->mutex);
        return RCOG_ERROR_CONTEXT_FULL;
    }

    if (store->total_size + size > store->config.max_total_size) {
        nimcp_mutex_unlock(store->mutex);
        return RCOG_ERROR_CONTEXT_FULL;
    }

    /* Create new variable */
    rcog_variable_t* var = create_variable(name, data, size, dtype);
    if (!var) {
        nimcp_mutex_unlock(store->mutex);
        return RCOG_ERROR_OUT_OF_MEMORY;
    }

    /* Insert into hash table */
    uint32_t bucket = hash_string(name);
    var->next = store->buckets[bucket];
    store->buckets[bucket] = var;

    store->variable_count++;
    store->total_size += size;

    nimcp_mutex_unlock(store->mutex);

    return RCOG_OK;
}

rcog_error_t rcog_context_store_set_text(
    rcog_context_store_t* store,
    const char* name,
    const char* text)
{
    if (!text) return RCOG_ERROR_NULL_POINTER;
    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_set_text", 0.0f);


    return rcog_context_store_set(store, name, text, strlen(text) + 1, RCOG_DTYPE_TEXT);
}

rcog_error_t rcog_context_store_remove(
    rcog_context_store_t* store,
    const char* name)
{
    if (!store || !name) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_remove", 0.0f);


    nimcp_mutex_lock(store->mutex);

    uint32_t bucket = hash_string(name);
    rcog_variable_t* prev = NULL;
    rcog_variable_t* var = store->buckets[bucket];

    while (var) {
        if (strcmp(var->name, name) == 0) {
            if (prev) {
                prev->next = var->next;
            } else {
                store->buckets[bucket] = var->next;
            }

            store->variable_count--;
            store->total_size -= var->size;

            destroy_variable(var);

            nimcp_mutex_unlock(store->mutex);
            return RCOG_OK;
        }
        prev = var;
        var = var->next;
    }

    nimcp_mutex_unlock(store->mutex);
    return RCOG_ERROR_CONTEXT_NOT_FOUND;
}

bool rcog_context_store_exists(
    const rcog_context_store_t* store,
    const char* name)
{
    if (!store || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_context_store_exists: required parameter is NULL (store, name)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_exists", 0.0f);


    nimcp_mutex_lock(((rcog_context_store_t*)store)->mutex);
    bool exists = (find_variable(store, name) != NULL);
    nimcp_mutex_unlock(((rcog_context_store_t*)store)->mutex);

    return exists;
}

rcog_error_t rcog_context_store_get_metadata(
    const rcog_context_store_t* store,
    const char* name,
    rcog_variable_metadata_t* metadata)
{
    if (!store || !name || !metadata) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_get_metadata", 0.0f);


    nimcp_mutex_lock(((rcog_context_store_t*)store)->mutex);

    rcog_variable_t* var = find_variable(store, name);
    if (!var) {
        nimcp_mutex_unlock(((rcog_context_store_t*)store)->mutex);
        return RCOG_ERROR_CONTEXT_NOT_FOUND;
    }

    strncpy(metadata->name, var->name, RCOG_MAX_VARIABLE_NAME_LEN);
    metadata->dtype = var->dtype;
    metadata->size = var->original_size;
    metadata->compressed = var->compressed;
    metadata->shared_with_swarm = var->shared_with_swarm;
    metadata->salience = var->salience;
    metadata->created_ms = var->created_ms;
    metadata->accessed_ms = var->accessed_ms;
    metadata->access_count = var->access_count;

    /* Count items based on type */
    if (var->dtype == RCOG_DTYPE_TEXT) {
        metadata->item_count = count_text_items(var->data, var->size);
    } else {
        metadata->item_count = var->size;
    }

    nimcp_mutex_unlock(((rcog_context_store_t*)store)->mutex);
    return RCOG_OK;
}

rcog_error_t rcog_context_store_list(
    const rcog_context_store_t* store,
    char** names,
    size_t max_names,
    size_t* count)
{
    if (!store || !names || !count) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_list", 0.0f);


    nimcp_mutex_lock(((rcog_context_store_t*)store)->mutex);

    size_t n = 0;
    for (int i = 0; i < HASH_TABLE_SIZE && n < max_names; i++) {
        rcog_variable_t* var = store->buckets[i];
        while (var && n < max_names) {
            names[n] = nimcp_strdup(var->name);
            if (!names[n]) {
                /* Free already-allocated names on OOM */
                for (size_t j = 0; j < n; j++) {
                    nimcp_free(names[j]);
                    names[j] = NULL;
                }
                *count = 0;
                nimcp_mutex_unlock(((rcog_context_store_t*)store)->mutex);
                return RCOG_ERROR_OUT_OF_MEMORY;
            }
            n++;
            var = var->next;
        }
    }

    *count = n;

    nimcp_mutex_unlock(((rcog_context_store_t*)store)->mutex);
    return RCOG_OK;
}

rcog_error_t rcog_context_store_query(
    rcog_context_store_t* store,
    const char* name,
    rcog_access_pattern_t pattern,
    const rcog_query_params_t* params,
    rcog_query_result_t* result)
{
    if (!store || !name || !result) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_query", 0.0f);


    uint64_t start_time = get_current_time_ms();
    rcog_query_params_t default_params = rcog_query_params_default();
    if (!params) params = &default_params;

    nimcp_mutex_lock(store->mutex);

    rcog_variable_t* var = find_variable(store, name);
    if (!var) {
        nimcp_mutex_unlock(store->mutex);
        return RCOG_ERROR_CONTEXT_NOT_FOUND;
    }

    /* Update access tracking */
    var->accessed_ms = get_current_time_ms();
    var->access_count++;
    strncpy(store->last_accessed, name, RCOG_MAX_VARIABLE_NAME_LEN);

    /* Initialize result */
    memset(result, 0, sizeof(rcog_query_result_t));
    result->dtype = var->dtype;

    /* Use the more restrictive of params limit and store config limit */
    size_t output_limit = 0;
    if (params->output_limit > 0 && store->config.output_limit_per_query > 0) {
        output_limit = params->output_limit < store->config.output_limit_per_query ?
                       params->output_limit : store->config.output_limit_per_query;
    } else if (params->output_limit > 0) {
        output_limit = params->output_limit;
    } else {
        output_limit = store->config.output_limit_per_query;
    }

    rcog_error_t err = RCOG_OK;

    switch (pattern) {
        case RCOG_ACCESS_METADATA:
            /* Metadata only - no data returned */
            result->total_size = var->original_size;
            break;

        case RCOG_ACCESS_FULL:
            /* Return full variable (up to limit) */
            if (var->dtype == RCOG_DTYPE_TEXT) {
                err = get_text_slice(var->data, var->size, 0, var->size,
                                     output_limit, result);
            } else {
                size_t copy_size = var->size;
                bool truncated = false;
                if (output_limit > 0 && copy_size > output_limit) {
                    copy_size = output_limit;
                    truncated = true;
                }

                void* copy = nimcp_malloc(copy_size);
                if (!copy) {
                    err = RCOG_ERROR_OUT_OF_MEMORY;
                } else {
                    memcpy(copy, var->data, copy_size);
                    result->data = copy;
                    result->size = copy_size;
                    result->owns_data = true;
                    result->truncated = truncated;
                    result->total_size = var->size;
                }
            }
            break;

        case RCOG_ACCESS_SLICE:
            if (var->dtype == RCOG_DTYPE_TEXT) {
                err = get_text_slice(var->data, var->size,
                                     params->start, params->end,
                                     output_limit, result);
            } else {
                /* Binary slice */
                if (params->start < var->size) {
                    size_t end = params->end < var->size ? params->end : var->size;
                    size_t slice_size = end - params->start;
                    if (output_limit > 0 && slice_size > output_limit) {
                        slice_size = output_limit;
                        result->truncated = true;
                    }

                    void* slice = nimcp_malloc(slice_size);
                    if (!slice) {
                        err = RCOG_ERROR_OUT_OF_MEMORY;
                    } else {
                        memcpy(slice, (char*)var->data + params->start, slice_size);
                        result->data = slice;
                        result->size = slice_size;
                        result->owns_data = true;
                        result->total_size = end - params->start;
                    }
                }
            }
            break;

        case RCOG_ACCESS_HEAD:
            if (var->dtype == RCOG_DTYPE_TEXT) {
                size_t count = params->count > 0 ? params->count : 10;
                err = get_text_slice(var->data, var->size, 0, count,
                                     output_limit, result);
            } else {
                size_t head_size = params->count > 0 ? params->count : 1024;
                if (head_size > var->size) head_size = var->size;

                void* head = nimcp_malloc(head_size);
                if (!head) {
                    err = RCOG_ERROR_OUT_OF_MEMORY;
                } else {
                    memcpy(head, var->data, head_size);
                    result->data = head;
                    result->size = head_size;
                    result->owns_data = true;
                    result->total_size = var->size;
                }
            }
            break;

        case RCOG_ACCESS_TAIL:
            if (var->dtype == RCOG_DTYPE_TEXT) {
                size_t count = params->count > 0 ? params->count : 10;
                /* var->size includes NUL terminator; use text_len for offset calc */
                size_t text_len = var->size > 0 ? var->size - 1 : 0;
                size_t start = text_len > count ? text_len - count : 0;
                err = get_text_slice(var->data, var->size, start, text_len,
                                     output_limit, result);
            } else {
                size_t tail_size = params->count > 0 ? params->count : 1024;
                if (tail_size > var->size) tail_size = var->size;
                size_t offset = var->size - tail_size;

                void* tail = nimcp_malloc(tail_size);
                if (!tail) {
                    err = RCOG_ERROR_OUT_OF_MEMORY;
                } else {
                    memcpy(tail, (char*)var->data + offset, tail_size);
                    result->data = tail;
                    result->size = tail_size;
                    result->owns_data = true;
                    result->total_size = var->size;
                }
            }
            break;

        case RCOG_ACCESS_SEARCH:
            if (var->dtype == RCOG_DTYPE_TEXT) {
                err = search_text(var->data, var->size, params->pattern,
                                  output_limit, result);
            } else {
                err = RCOG_ERROR_INVALID_QUERY;  /* Search only for text */
            }
            break;

        case RCOG_ACCESS_SAMPLE:
            /* TODO: Implement random sampling */
            err = RCOG_ERROR_INVALID_QUERY;
            break;

        case RCOG_ACCESS_AGGREGATE:
            /* TODO: Implement aggregation functions */
            err = RCOG_ERROR_INVALID_QUERY;
            break;

        default:
            err = RCOG_ERROR_INVALID_QUERY;
    }

    /* Set found flag for non-search queries (search sets it based on matches) */
    if (err == RCOG_OK && pattern != RCOG_ACCESS_SEARCH) {
        result->found = true;
    }

    /* Update statistics */
    store->query_count++;
    uint64_t end_time = get_current_time_ms();
    store->total_query_time_ms += (double)(end_time - start_time);

    nimcp_mutex_unlock(store->mutex);
    return err;
}

rcog_error_t rcog_context_store_exec(
    rcog_context_store_t* store,
    const char* name,
    rcog_helper_fn helper,
    void* helper_context,
    rcog_query_result_t* result)
{
    if (!store || !name || !helper || !result) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_exec", 0.0f);


    nimcp_mutex_lock(store->mutex);

    rcog_variable_t* var = find_variable(store, name);
    if (!var) {
        nimcp_mutex_unlock(store->mutex);
        return RCOG_ERROR_CONTEXT_NOT_FOUND;
    }

    /* Update access tracking */
    var->accessed_ms = get_current_time_ms();
    var->access_count++;

    /* Capture variable fields before releasing the lock to avoid
     * calling the user-supplied callback while holding the mutex,
     * which would risk deadlock if the callback re-enters the store. */
    void* var_data = var->data;
    size_t var_size = var->size;
    rcog_data_type_t var_dtype = var->dtype;

    nimcp_mutex_unlock(store->mutex);

    /* Call helper function outside the lock */
    rcog_error_t err = helper(var_data, var_size, var_dtype,
                               helper_context, result);

    nimcp_mutex_lock(store->mutex);
    store->query_count++;
    nimcp_mutex_unlock(store->mutex);

    return err;
}

rcog_error_t rcog_context_store_search_all(
    rcog_context_store_t* store,
    const char* pattern,
    size_t max_results,
    rcog_query_result_t* results,
    size_t* result_count)
{
    if (!store || !pattern || !results || !result_count) {
        return RCOG_ERROR_NULL_POINTER;
    }

    *result_count = 0;

    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_search_all", 0.0f);


    nimcp_mutex_lock(store->mutex);

    for (int i = 0; i < HASH_TABLE_SIZE && *result_count < max_results; i++) {
        rcog_variable_t* var = store->buckets[i];
        while (var && *result_count < max_results) {
            if (var->dtype == RCOG_DTYPE_TEXT) {
                rcog_query_result_t* res = &results[*result_count];
                rcog_error_t err = search_text(var->data, var->size, pattern,
                                               store->config.output_limit_per_query, res);
                if (err == RCOG_OK && res->match_count > 0) {
                    (*result_count)++;
                }
            }
            var = var->next;
        }
    }

    nimcp_mutex_unlock(store->mutex);
    return RCOG_OK;
}

rcog_error_t rcog_context_store_get_stats(
    const rcog_context_store_t* store,
    rcog_context_store_stats_t* stats)
{
    if (!store || !stats) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_get_stats", 0.0f);


    nimcp_mutex_lock(((rcog_context_store_t*)store)->mutex);

    stats->variable_count = store->variable_count;
    stats->total_size = store->total_size;
    stats->compressed_size = store->compressed_size > 0 ?
                             store->compressed_size : store->total_size;
    stats->query_count = store->query_count;
    stats->cache_hits = store->cache_hits;
    stats->cache_misses = store->cache_misses;
    stats->avg_query_time_ms = store->query_count > 0 ?
                               (float)(store->total_query_time_ms / store->query_count) : 0.0f;

    /* Find largest variable */
    stats->max_variable_size = 0;
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && HASH_TABLE_SIZE > 256) {
            rcog_context_store_heartbeat("rcog_context_loop",
                             (float)(i + 1) / (float)HASH_TABLE_SIZE);
        }

        rcog_variable_t* var = store->buckets[i];
        while (var) {
            if (var->size > stats->max_variable_size) {
                stats->max_variable_size = var->size;
            }
            var = var->next;
        }
    }

    nimcp_mutex_unlock(((rcog_context_store_t*)store)->mutex);
    return RCOG_OK;
}

void rcog_context_store_reset_stats(rcog_context_store_t* store)
{
    if (!store) return;

    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_reset_stats", 0.0f);


    nimcp_mutex_lock(store->mutex);

    store->query_count = 0;
    store->cache_hits = 0;
    store->cache_misses = 0;
    store->total_query_time_ms = 0.0;

    nimcp_mutex_unlock(store->mutex);
}

rcog_error_t rcog_context_store_enable_prediction(
    rcog_context_store_t* store,
    bool enable)
{
    if (!store) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_enable_prediction", 0.0f);


    nimcp_mutex_lock(store->mutex);
    store->prediction_enabled = enable;
    nimcp_mutex_unlock(store->mutex);

    return RCOG_OK;
}

rcog_error_t rcog_context_store_set_salience(
    rcog_context_store_t* store,
    const char* name,
    float salience)
{
    if (!store || !name) return RCOG_ERROR_NULL_POINTER;
    if (salience < 0.0f || salience > 1.0f) return RCOG_ERROR_INVALID_CONFIG;

    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_set_salience", 0.0f);


    nimcp_mutex_lock(store->mutex);

    rcog_variable_t* var = find_variable(store, name);
    if (!var) {
        nimcp_mutex_unlock(store->mutex);
        return RCOG_ERROR_CONTEXT_NOT_FOUND;
    }

    var->salience = salience;

    nimcp_mutex_unlock(store->mutex);
    return RCOG_OK;
}

rcog_error_t rcog_context_store_set_shared(
    rcog_context_store_t* store,
    const char* name,
    bool shared)
{
    if (!store || !name) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_set_shared", 0.0f);


    nimcp_mutex_lock(store->mutex);

    rcog_variable_t* var = find_variable(store, name);
    if (!var) {
        nimcp_mutex_unlock(store->mutex);
        return RCOG_ERROR_CONTEXT_NOT_FOUND;
    }

    var->shared_with_swarm = shared;

    nimcp_mutex_unlock(store->mutex);
    return RCOG_OK;
}

rcog_error_t rcog_context_store_get_shared(
    const rcog_context_store_t* store,
    char** names,
    size_t max_names,
    size_t* count)
{
    if (!store || !names || !count) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_get_shared", 0.0f);


    nimcp_mutex_lock(((rcog_context_store_t*)store)->mutex);

    size_t n = 0;
    for (int i = 0; i < HASH_TABLE_SIZE && n < max_names; i++) {
        rcog_variable_t* var = store->buckets[i];
        while (var && n < max_names) {
            if (var->shared_with_swarm) {
                names[n] = nimcp_strdup(var->name);
                if (!names[n]) {
                    for (size_t j = 0; j < n; j++) {
                        nimcp_free(names[j]);
                        names[j] = NULL;
                    }
                    *count = 0;
                    nimcp_mutex_unlock(((rcog_context_store_t*)store)->mutex);
                    return RCOG_ERROR_OUT_OF_MEMORY;
                }
                n++;
            }
            var = var->next;
        }
    }

    *count = n;

    nimcp_mutex_unlock(((rcog_context_store_t*)store)->mutex);
    return RCOG_OK;
}

rcog_error_t rcog_context_store_lock(rcog_context_store_t* store)
{
    if (!store) return RCOG_ERROR_NULL_POINTER;
    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_lock", 0.0f);


    nimcp_mutex_lock(store->mutex);
    /* Caller is responsible for calling rcog_context_store_unlock() */
    return RCOG_OK;
}

rcog_error_t rcog_context_store_unlock(rcog_context_store_t* store)
{
    if (!store) return RCOG_ERROR_NULL_POINTER;
    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_unlock", 0.0f);


    nimcp_mutex_unlock(store->mutex);
    return RCOG_OK;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int rcog_context_store_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    rcog_context_store_heartbeat("rcog_context_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Recursive_Cognition_Context_Store_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                rcog_context_store_heartbeat("rcog_context_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Log self-knowledge observations */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Recursive_Cognition_Context_Store_Module");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Recursive_Cognition_Context_Store_Module");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}
