/**
 * @file nimcp_kg_degradation.c
 * @brief Graceful Degradation for Brain Knowledge Graph
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Implementation of circuit breaker pattern with write buffering and cache
 * fallback for resilient KG operations.
 */

#include "core/brain/nimcp_kg_degradation.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(kg_degradation, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_CALLBACKS           16
#define MAX_CACHE_ENTRIES       4096
#define CACHE_HASH_SIZE         1024

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Cache entry
 */
typedef struct kg_cache_entry {
    brain_kg_node_id_t id;           /**< Node ID */
    void* data;                      /**< Cached data */
    size_t size;                     /**< Data size */
    uint64_t timestamp;              /**< Cache time */
    struct kg_cache_entry* next;     /**< Hash chain */
} kg_cache_entry_t;

/**
 * @brief Callback registration
 */
typedef struct {
    kg_degradation_callback_fn callback;  /**< Callback function */
    void* user_data;                      /**< User context */
    bool active;                          /**< Active flag */
} kg_callback_t;

/**
 * @brief Degradation context implementation
 */
struct kg_degradation_ctx {
    kg_degradation_config_t config;      /**< Configuration */
    kg_degradation_level_t level;        /**< Current degradation level */

    /* Circuit breaker state */
    kg_circuit_state_t circuit_state;    /**< Circuit state */
    uint32_t failure_count;              /**< Consecutive failures */
    uint32_t success_count;              /**< Successes in half-open */
    uint64_t circuit_opened_at;          /**< When circuit opened */
    uint32_t half_open_requests;         /**< Requests allowed in half-open */

    /* Write buffer */
    kg_io_request_t* write_buffer;       /**< Buffered writes */
    uint32_t buffer_count;               /**< Current buffer count */
    uint32_t buffer_capacity;            /**< Buffer capacity */
    uint64_t buffer_memory;              /**< Memory used by buffer */
    uint64_t oldest_write_ts;            /**< Oldest buffered write */
    FILE* disk_file;                     /**< Disk buffer file */

    /* Cache */
    kg_cache_entry_t** cache;            /**< Hash table */
    uint32_t cache_entries;              /**< Number of entries */
    uint64_t cache_memory;               /**< Memory used by cache */

    /* Statistics */
    kg_degradation_stats_t stats;        /**< Runtime stats */
    uint64_t degradation_start_ts;       /**< When degradation started */

    /* Callbacks */
    kg_callback_t callbacks[MAX_CALLBACKS]; /**< Registered callbacks */
    uint32_t callback_count;                /**< Number of callbacks */

    /* Thread safety */
    nimcp_mutex_t* mutex;                /**< Context mutex */
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/**
 * @brief Compute hash for node ID
 */
static uint32_t hash_node_id(brain_kg_node_id_t id) {
    return (uint32_t)(id % CACHE_HASH_SIZE);
}

/**
 * @brief Invoke all registered callbacks
 */
static void invoke_callbacks(
    kg_degradation_ctx_t* ctx,
    kg_degradation_level_t old_level,
    kg_degradation_level_t new_level
) {
    for (uint32_t i = 0; i < MAX_CALLBACKS; i++) {
        if (ctx->callbacks[i].active && ctx->callbacks[i].callback) {
            ctx->callbacks[i].callback(old_level, new_level, ctx,
                                       ctx->callbacks[i].user_data);
        }
    }
}

/**
 * @brief Update degradation level based on circuit state
 */
static void update_degradation_level(kg_degradation_ctx_t* ctx) {
    kg_degradation_level_t old_level = ctx->level;
    kg_degradation_level_t new_level = old_level;

    switch (ctx->circuit_state) {
        case KG_CIRCUIT_CLOSED:
            new_level = KG_DEGRADE_NONE;
            break;

        case KG_CIRCUIT_HALF_OPEN:
            new_level = KG_DEGRADE_READ_ONLY;
            break;

        case KG_CIRCUIT_OPEN:
            /* Check cache availability */
            if (ctx->cache_entries > 0) {
                new_level = KG_DEGRADE_CACHE_ONLY;
            } else {
                new_level = KG_DEGRADE_OFFLINE;
            }
            break;
    }

    if (new_level != old_level) {
        ctx->level = new_level;

        if (new_level > KG_DEGRADE_NONE && old_level == KG_DEGRADE_NONE) {
            ctx->degradation_start_ts = get_timestamp_ms();
        } else if (new_level == KG_DEGRADE_NONE && old_level > KG_DEGRADE_NONE) {
            ctx->stats.total_degradation_time_ms +=
                get_timestamp_ms() - ctx->degradation_start_ts;
        }

        invoke_callbacks(ctx, old_level, new_level);
    }
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int kg_degradation_default_config(kg_degradation_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    memset(config, 0, sizeof(*config));

    /* Cache defaults */
    config->cache.cache_size_mb = KG_DEGRADATION_DEFAULT_CACHE_MB;
    config->cache.stale_threshold_ms = KG_DEGRADATION_DEFAULT_STALE_MS;
    config->cache.max_stale_serve_ms = KG_DEGRADATION_DEFAULT_STALE_MS * 2;
    config->cache.serve_stale_on_error = true;
    config->cache.prefetch_on_access = false;

    /* Circuit breaker defaults */
    config->circuit.failure_threshold = KG_DEGRADATION_DEFAULT_FAILURE_THRESHOLD;
    config->circuit.success_threshold = KG_DEGRADATION_DEFAULT_SUCCESS_THRESHOLD;
    config->circuit.timeout_ms = KG_DEGRADATION_DEFAULT_TIMEOUT_MS;
    config->circuit.half_open_requests = 3;
    config->circuit.enable_adaptive_timeout = false;

    /* Buffer defaults */
    config->max_buffer_size = KG_DEGRADATION_DEFAULT_BUFFER_SIZE;
    config->enable_disk_buffer = false;
    config->auto_recovery = true;
    config->recovery_interval_ms = 10000;

    return 0;
}

kg_degradation_ctx_t* kg_degradation_create(const kg_degradation_config_t* config) {
    kg_degradation_ctx_t* ctx = nimcp_calloc(1, sizeof(kg_degradation_ctx_t));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ctx is NULL");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        ctx->config = *config;
    } else {
        kg_degradation_default_config(&ctx->config);
    }

    /* Initialize state */
    ctx->level = KG_DEGRADE_NONE;
    ctx->circuit_state = KG_CIRCUIT_CLOSED;

    /* Allocate write buffer */
    ctx->buffer_capacity = ctx->config.max_buffer_size;
    ctx->write_buffer = nimcp_calloc(ctx->buffer_capacity, sizeof(kg_io_request_t));
    if (!ctx->write_buffer) {
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_degradation_create: ctx->write_buffer is NULL");
        return NULL;
    }

    /* Allocate cache hash table */
    ctx->cache = nimcp_calloc(CACHE_HASH_SIZE, sizeof(kg_cache_entry_t*));
    if (!ctx->cache) {
        nimcp_free(ctx->write_buffer);
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_degradation_create: ctx->cache is NULL");
        return NULL;
    }

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    ctx->mutex = nimcp_mutex_create(&attr);
    if (!ctx->mutex) {
        nimcp_free(ctx->cache);
        nimcp_free(ctx->write_buffer);
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_degradation_create: ctx->mutex is NULL");
        return NULL;
    }

    /* Open disk buffer if enabled */
    if (ctx->config.enable_disk_buffer && ctx->config.disk_buffer_path[0] != '\0') {
        ctx->disk_file = fopen(ctx->config.disk_buffer_path, "a+b");
        /* Failure is non-fatal */
    }

    return ctx;
}

void kg_degradation_destroy(kg_degradation_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    /* Close disk file */
    if (ctx->disk_file) {
        fclose(ctx->disk_file);
    }

    /* Free write buffer */
    if (ctx->write_buffer) {
        for (uint32_t i = 0; i < ctx->buffer_count; i++) {
            if (ctx->write_buffer[i].payload) {
                nimcp_free(ctx->write_buffer[i].payload);
            }
        }
        nimcp_free(ctx->write_buffer);
    }

    /* Free cache */
    if (ctx->cache) {
        for (uint32_t i = 0; i < CACHE_HASH_SIZE; i++) {
            kg_cache_entry_t* entry = ctx->cache[i];
            while (entry) {
                kg_cache_entry_t* next = entry->next;
                if (entry->data) {
                    nimcp_free(entry->data);
                }
                nimcp_free(entry);
                entry = next;
            }
        }
        nimcp_free(ctx->cache);
    }

    /* Destroy mutex */
    if (ctx->mutex) {
        nimcp_mutex_free(ctx->mutex);
    }

    nimcp_free(ctx);
}

/* ============================================================================
 * State Query API
 * ============================================================================ */

kg_degradation_level_t kg_degradation_get_level(const kg_degradation_ctx_t* ctx) {
    if (!ctx) {
        return KG_DEGRADE_OFFLINE;
    }
    return ctx->level;
}

kg_circuit_state_t kg_degradation_get_circuit_state(const kg_degradation_ctx_t* ctx) {
    if (!ctx) {
        return KG_CIRCUIT_OPEN;
    }
    return ctx->circuit_state;
}

bool kg_degradation_can_write(const kg_degradation_ctx_t* ctx) {
    if (!ctx) {
        return false;
    }
    return ctx->level == KG_DEGRADE_NONE;
}

bool kg_degradation_can_read(const kg_degradation_ctx_t* ctx) {
    if (!ctx) {
        return false;
    }
    return ctx->level != KG_DEGRADE_OFFLINE;
}

int kg_degradation_get_stats(
    const kg_degradation_ctx_t* ctx,
    kg_degradation_stats_t* stats
) {
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_degradation_get_stats: required parameter is NULL (ctx, stats)");
        return -1;
    }

    nimcp_mutex_lock(((kg_degradation_ctx_t*)ctx)->mutex);
    *stats = ctx->stats;
    nimcp_mutex_unlock(((kg_degradation_ctx_t*)ctx)->mutex);

    return 0;
}

/* ============================================================================
 * Write Buffer API
 * ============================================================================ */

int kg_degradation_buffer_write(
    kg_degradation_ctx_t* ctx,
    const kg_io_request_t* write
) {
    if (!ctx || !write) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_degradation_buffer_write: required parameter is NULL (ctx, write)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Check buffer capacity */
    if (ctx->buffer_count >= ctx->buffer_capacity) {
        ctx->stats.dropped_writes++;
        nimcp_mutex_unlock(ctx->mutex);
        return -2; /* Buffer full */
    }

    /* Copy request */
    kg_io_request_t* entry = &ctx->write_buffer[ctx->buffer_count];
    *entry = *write;
    entry->timestamp = get_timestamp_ms();

    /* Clone payload if present */
    if (write->payload && write->payload_size > 0) {
        entry->payload = nimcp_malloc(write->payload_size);
        if (!entry->payload) {
            nimcp_mutex_unlock(ctx->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_degradation_buffer_write: entry->payload is NULL");
            return -1;
        }
        memcpy(entry->payload, write->payload, write->payload_size);
        ctx->buffer_memory += write->payload_size;
    }

    /* Update counters */
    if (ctx->buffer_count == 0) {
        ctx->oldest_write_ts = entry->timestamp;
    }
    ctx->buffer_count++;
    ctx->stats.buffered_writes++;

    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

int kg_degradation_flush_buffer(kg_degradation_ctx_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    if (ctx->buffer_count == 0) {
        nimcp_mutex_unlock(ctx->mutex);
        return 0;
    }

    /* In a real implementation, we would replay each write to the KG here */
    /* For now, just clear the buffer */
    int flushed = (int)ctx->buffer_count;

    for (uint32_t i = 0; i < ctx->buffer_count; i++) {
        if (ctx->write_buffer[i].payload) {
            nimcp_free(ctx->write_buffer[i].payload);
        }
    }

    ctx->buffer_count = 0;
    ctx->buffer_memory = 0;
    ctx->oldest_write_ts = 0;

    nimcp_mutex_unlock(ctx->mutex);

    return flushed;
}

uint32_t kg_degradation_get_buffer_size(const kg_degradation_ctx_t* ctx) {
    if (!ctx) {
        return 0;
    }
    return ctx->buffer_count;
}

uint64_t kg_degradation_get_buffer_memory(const kg_degradation_ctx_t* ctx) {
    if (!ctx) {
        return 0;
    }
    return ctx->buffer_memory;
}

int kg_degradation_discard_buffer(kg_degradation_ctx_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    int discarded = (int)ctx->buffer_count;

    for (uint32_t i = 0; i < ctx->buffer_count; i++) {
        if (ctx->write_buffer[i].payload) {
            nimcp_free(ctx->write_buffer[i].payload);
        }
    }

    ctx->buffer_count = 0;
    ctx->buffer_memory = 0;
    ctx->oldest_write_ts = 0;

    nimcp_mutex_unlock(ctx->mutex);

    return discarded;
}

/* ============================================================================
 * Circuit Breaker API
 * ============================================================================ */

int kg_degradation_record_success(kg_degradation_ctx_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    ctx->failure_count = 0;

    switch (ctx->circuit_state) {
        case KG_CIRCUIT_CLOSED:
            /* Already closed, no change */
            break;

        case KG_CIRCUIT_HALF_OPEN:
            ctx->success_count++;
            if (ctx->success_count >= ctx->config.circuit.success_threshold) {
                /* Close circuit */
                ctx->circuit_state = KG_CIRCUIT_CLOSED;
                ctx->success_count = 0;
                ctx->stats.successful_recoveries++;
                update_degradation_level(ctx);
            }
            break;

        case KG_CIRCUIT_OPEN:
            /* Unexpected success in open state */
            break;
    }

    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

int kg_degradation_record_failure(kg_degradation_ctx_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    ctx->failure_count++;
    ctx->success_count = 0;

    switch (ctx->circuit_state) {
        case KG_CIRCUIT_CLOSED:
            if (ctx->failure_count >= ctx->config.circuit.failure_threshold) {
                /* Open circuit */
                ctx->circuit_state = KG_CIRCUIT_OPEN;
                ctx->circuit_opened_at = get_timestamp_ms();
                ctx->half_open_requests = 0;
                ctx->stats.circuit_trips++;
                update_degradation_level(ctx);
            }
            break;

        case KG_CIRCUIT_HALF_OPEN:
            /* Failed in half-open, reopen */
            ctx->circuit_state = KG_CIRCUIT_OPEN;
            ctx->circuit_opened_at = get_timestamp_ms();
            ctx->half_open_requests = 0;
            update_degradation_level(ctx);
            break;

        case KG_CIRCUIT_OPEN:
            /* Check if timeout elapsed */
            {
                uint64_t elapsed = get_timestamp_ms() - ctx->circuit_opened_at;
                if (elapsed >= ctx->config.circuit.timeout_ms) {
                    /* Transition to half-open */
                    ctx->circuit_state = KG_CIRCUIT_HALF_OPEN;
                    ctx->success_count = 0;
                    ctx->half_open_requests = 0;
                    update_degradation_level(ctx);
                }
            }
            break;
    }

    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

int kg_degradation_force_open_circuit(kg_degradation_ctx_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    if (ctx->circuit_state != KG_CIRCUIT_OPEN) {
        ctx->circuit_state = KG_CIRCUIT_OPEN;
        ctx->circuit_opened_at = get_timestamp_ms();
        ctx->half_open_requests = 0;
        ctx->stats.circuit_trips++;
        update_degradation_level(ctx);
    }

    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

int kg_degradation_force_close_circuit(kg_degradation_ctx_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    ctx->circuit_state = KG_CIRCUIT_CLOSED;
    ctx->failure_count = 0;
    ctx->success_count = 0;
    ctx->stats.successful_recoveries++;
    update_degradation_level(ctx);

    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

uint32_t kg_degradation_get_failure_count(const kg_degradation_ctx_t* ctx) {
    if (!ctx) {
        return 0;
    }
    return ctx->failure_count;
}

uint32_t kg_degradation_get_success_count(const kg_degradation_ctx_t* ctx) {
    if (!ctx) {
        return 0;
    }
    return ctx->success_count;
}

/* ============================================================================
 * Cache API
 * ============================================================================ */

int kg_degradation_cache_put(
    kg_degradation_ctx_t* ctx,
    brain_kg_node_id_t id,
    const void* data,
    size_t size
) {
    if (!ctx || !data || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_degradation_cache_put: required parameter is NULL (ctx, data)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    uint32_t hash = hash_node_id(id);

    /* Check if entry exists */
    kg_cache_entry_t* entry = ctx->cache[hash];
    while (entry) {
        if (entry->id == id) {
            /* Update existing entry */
            void* new_data = nimcp_malloc(size);
            if (!new_data) {
                nimcp_mutex_unlock(ctx->mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_degradation_cache_put: new_data is NULL");
                return -1;
            }
            memcpy(new_data, data, size);

            ctx->cache_memory -= entry->size;
            if (entry->data) {
                nimcp_free(entry->data);
            }
            entry->data = new_data;
            entry->size = size;
            entry->timestamp = get_timestamp_ms();
            ctx->cache_memory += size;

            nimcp_mutex_unlock(ctx->mutex);
            return 0;
        }
        entry = entry->next;
    }

    /* Create new entry */
    entry = nimcp_calloc(1, sizeof(kg_cache_entry_t));
    if (!entry) {
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_degradation_cache_put: entry is NULL");
        return -1;
    }

    entry->data = nimcp_malloc(size);
    if (!entry->data) {
        nimcp_free(entry);
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_degradation_cache_put: entry->data is NULL");
        return -1;
    }

    entry->id = id;
    memcpy(entry->data, data, size);
    entry->size = size;
    entry->timestamp = get_timestamp_ms();

    /* Add to hash chain */
    entry->next = ctx->cache[hash];
    ctx->cache[hash] = entry;

    ctx->cache_entries++;
    ctx->cache_memory += size;

    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

int kg_degradation_cache_get(
    const kg_degradation_ctx_t* ctx,
    brain_kg_node_id_t id,
    void** data,
    size_t* size,
    bool* is_stale
) {
    if (!ctx || !data || !size || !is_stale) {
        return -2;
    }

    nimcp_mutex_lock(((kg_degradation_ctx_t*)ctx)->mutex);

    uint32_t hash = hash_node_id(id);

    kg_cache_entry_t* entry = ctx->cache[hash];
    while (entry) {
        if (entry->id == id) {
            /* Found entry */
            *data = nimcp_malloc(entry->size);
            if (!*data) {
                nimcp_mutex_unlock(((kg_degradation_ctx_t*)ctx)->mutex);
                return -2;
            }

            memcpy(*data, entry->data, entry->size);
            *size = entry->size;

            /* Check staleness */
            uint64_t age = get_timestamp_ms() - entry->timestamp;
            *is_stale = (age > ctx->config.cache.stale_threshold_ms);

            if (*is_stale) {
                ((kg_degradation_ctx_t*)ctx)->stats.stale_serves++;
            }
            ((kg_degradation_ctx_t*)ctx)->stats.cache_hits++;

            nimcp_mutex_unlock(((kg_degradation_ctx_t*)ctx)->mutex);
            return 0;
        }
        entry = entry->next;
    }

    ((kg_degradation_ctx_t*)ctx)->stats.cache_misses++;
    nimcp_mutex_unlock(((kg_degradation_ctx_t*)ctx)->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_degradation_cache_get: operation failed");
    return -1;
}

int kg_degradation_cache_invalidate(
    kg_degradation_ctx_t* ctx,
    brain_kg_node_id_t id
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    uint32_t hash = hash_node_id(id);

    kg_cache_entry_t** prev = &ctx->cache[hash];
    kg_cache_entry_t* entry = *prev;

    while (entry) {
        if (entry->id == id) {
            *prev = entry->next;
            ctx->cache_memory -= entry->size;
            ctx->cache_entries--;

            if (entry->data) {
                nimcp_free(entry->data);
            }
            nimcp_free(entry);

            nimcp_mutex_unlock(ctx->mutex);
            return 0;
        }
        prev = &entry->next;
        entry = entry->next;
    }

    nimcp_mutex_unlock(ctx->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_degradation_cache_invalidate: operation failed");
    return -1;
}

int kg_degradation_cache_clear(kg_degradation_ctx_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    int cleared = (int)ctx->cache_entries;

    for (uint32_t i = 0; i < CACHE_HASH_SIZE; i++) {
        kg_cache_entry_t* entry = ctx->cache[i];
        while (entry) {
            kg_cache_entry_t* next = entry->next;
            if (entry->data) {
                nimcp_free(entry->data);
            }
            nimcp_free(entry);
            entry = next;
        }
        ctx->cache[i] = NULL;
    }

    ctx->cache_entries = 0;
    ctx->cache_memory = 0;

    nimcp_mutex_unlock(ctx->mutex);

    return cleared;
}

int kg_degradation_cache_stats(
    const kg_degradation_ctx_t* ctx,
    uint32_t* entries,
    uint64_t* memory_bytes
) {
    if (!ctx || !entries || !memory_bytes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_degradation_cache_stats: required parameter is NULL (ctx, entries, memory_bytes)");
        return -1;
    }

    nimcp_mutex_lock(((kg_degradation_ctx_t*)ctx)->mutex);

    *entries = ctx->cache_entries;
    *memory_bytes = ctx->cache_memory;

    nimcp_mutex_unlock(((kg_degradation_ctx_t*)ctx)->mutex);

    return 0;
}

/* ============================================================================
 * Recovery API
 * ============================================================================ */

int kg_degradation_attempt_recovery(kg_degradation_ctx_t* ctx) {
    if (!ctx) {
        return -2;
    }

    nimcp_mutex_lock(ctx->mutex);

    ctx->stats.recovery_attempts++;

    /* In a real implementation, we would test KG connectivity here */
    /* For now, just simulate by checking circuit state */

    if (ctx->circuit_state == KG_CIRCUIT_OPEN) {
        uint64_t elapsed = get_timestamp_ms() - ctx->circuit_opened_at;
        if (elapsed >= ctx->config.circuit.timeout_ms) {
            /* Transition to half-open for testing */
            ctx->circuit_state = KG_CIRCUIT_HALF_OPEN;
            ctx->success_count = 0;
            ctx->half_open_requests = 0;
            update_degradation_level(ctx);
            nimcp_mutex_unlock(ctx->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_degradation_attempt_recovery: capacity exceeded");
            return -1; /* Still degraded, but testing */
        }
    }

    if (ctx->circuit_state == KG_CIRCUIT_CLOSED) {
        nimcp_mutex_unlock(ctx->mutex);
        return 0; /* Recovered */
    }

    nimcp_mutex_unlock(ctx->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_degradation_attempt_recovery: validation failed");
    return -1; /* Still degraded */
}

int kg_degradation_sync_buffer_to_db(kg_degradation_ctx_t* ctx) {
    return kg_degradation_flush_buffer(ctx);
}

int kg_degradation_set_level(
    kg_degradation_ctx_t* ctx,
    kg_degradation_level_t level
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    kg_degradation_level_t old_level = ctx->level;
    ctx->level = level;

    if (level != old_level) {
        invoke_callbacks(ctx, old_level, level);
    }

    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

/* ============================================================================
 * Callback API
 * ============================================================================ */

int kg_degradation_register_callback(
    kg_degradation_ctx_t* ctx,
    kg_degradation_callback_fn callback,
    void* user_data
) {
    if (!ctx || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_degradation_register_callback: required parameter is NULL (ctx, callback)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Find free slot */
    for (uint32_t i = 0; i < MAX_CALLBACKS; i++) {
        if (!ctx->callbacks[i].active) {
            ctx->callbacks[i].callback = callback;
            ctx->callbacks[i].user_data = user_data;
            ctx->callbacks[i].active = true;
            ctx->callback_count++;
            nimcp_mutex_unlock(ctx->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(ctx->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_degradation_register_callback: operation failed");
    return -1; /* Max callbacks reached */
}

int kg_degradation_unregister_callback(
    kg_degradation_ctx_t* ctx,
    kg_degradation_callback_fn callback
) {
    if (!ctx || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_degradation_unregister_callback: required parameter is NULL (ctx, callback)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    for (uint32_t i = 0; i < MAX_CALLBACKS; i++) {
        if (ctx->callbacks[i].active && ctx->callbacks[i].callback == callback) {
            ctx->callbacks[i].active = false;
            ctx->callbacks[i].callback = NULL;
            ctx->callbacks[i].user_data = NULL;
            ctx->callback_count--;
            nimcp_mutex_unlock(ctx->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(ctx->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_degradation_unregister_callback: operation failed");
    return -1; /* Not found */
}

/* ============================================================================
 * Utility API
 * ============================================================================ */

static const char* degradation_level_strings[] = {
    "NONE",
    "READ_ONLY",
    "STALE_READS",
    "CACHE_ONLY",
    "OFFLINE"
};

const char* kg_degradation_level_to_string(kg_degradation_level_t level) {
    if (level >= 0 && level <= KG_DEGRADE_OFFLINE) {
        return degradation_level_strings[level];
    }
    return "UNKNOWN";
}

static const char* circuit_state_strings[] = {
    "CLOSED",
    "HALF_OPEN",
    "OPEN"
};

const char* kg_circuit_state_to_string(kg_circuit_state_t state) {
    if (state >= 0 && state <= KG_CIRCUIT_OPEN) {
        return circuit_state_strings[state];
    }
    return "UNKNOWN";
}

static const char* io_type_strings[] = {
    "NODE_CREATE",
    "NODE_UPDATE",
    "NODE_DELETE",
    "EDGE_CREATE",
    "EDGE_UPDATE",
    "EDGE_DELETE",
    "METADATA_UPDATE"
};

const char* kg_io_type_to_string(kg_io_type_t type) {
    if (type >= 0 && type < KG_IO_TYPE_COUNT) {
        return io_type_strings[type];
    }
    return "UNKNOWN";
}

void kg_io_request_free_payload(kg_io_request_t* request) {
    if (request && request->payload) {
        nimcp_free(request->payload);
        request->payload = NULL;
        request->payload_size = 0;
    }
}
