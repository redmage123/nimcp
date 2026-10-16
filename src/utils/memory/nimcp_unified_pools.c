/**
 * @file nimcp_unified_pools.c
 * @brief Unified Memory Pool + COW Integration Implementation (Phase 4)
 *
 * WHAT: Production-ready unified memory management
 * WHY:  Single point of control for all memory allocation
 * HOW:  Coordinate brain_pools, layer_pools, COW with pressure/quotas
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 *
 * @author NIMCP Development Team
 * @date 2025-11-24
 * @version 4.0.0
 */

#include "utils/memory/nimcp_unified_pools.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(unified_pools)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief COW template entry
 */
typedef struct cow_template {
    void* data;                     /* Template data */
    size_t size;                    /* Size in bytes */
    uint32_t refcount;              /* Reference count */
    unified_pool_type_t pool_hint;  /* Pool for private copies */
    uint64_t created_ms;            /* Creation timestamp */
    struct cow_template* next;      /* Free list link */
} cow_template_t;

/**
 * @brief Unified COW handle
 */
struct unified_cow_handle {
    cow_template_t* template_ref;   /* Reference to template */
    void* private_data;             /* Private copy (NULL if shared) */
    bool is_private;                /* True if has private copy */
    unified_pool_type_t source_pool;/* Pool for private copy */
};

/**
 * @brief Allocation tracking entry
 */
typedef struct alloc_entry {
    void* ptr;                      /* Allocated pointer */
    size_t size;                    /* Allocation size */
    unified_pool_type_t source;     /* Source pool */
    bool is_cow;                    /* Is COW allocation */
    bool borrowed;                  /* Was borrowed */
    struct alloc_entry* next;       /* Hash chain */
} alloc_entry_t;

/**
 * @brief Per-pool state
 */
typedef struct {
    pool_quota_t quota;             /* Quota configuration */
    size_t current_bytes;           /* Currently allocated */
    uint64_t total_acquires;        /* Total acquisitions */
    uint64_t total_releases;        /* Total releases */
    uint64_t cow_triggers;          /* COW copies triggered */
    uint64_t quota_violations;      /* Quota exceeded count */
    uint64_t acquire_time_total_ns; /* Total acquire time */
    uint64_t acquire_count;         /* Acquire count for avg */
} pool_state_t;

/**
 * @brief Unified pools manager
 */
struct unified_pools {
    /* Underlying pools */
    brain_pools_t brain_pools;
    layer_pools_t layer_pools;
    bool owns_pools;                /* Whether we created the pools */

    /* Configuration */
    unified_pools_config_t config;

    /* Per-pool state */
    pool_state_t pool_states[UNIFIED_POOL_COUNT];

    /* COW templates */
    cow_template_t* templates;      /* Active templates list */
    cow_template_t* template_free;  /* Free template list */
    size_t template_count;          /* Active template count */
    size_t template_capacity;       /* Max templates */

    /* Allocation tracking (simple hash table) */
    alloc_entry_t* alloc_table[256];/* Hash table for tracking */
    alloc_entry_t* entry_free;      /* Free entry list */

    /* Pressure state */
    pressure_level_t current_pressure;
    uint64_t last_gc_ms;
    uint64_t gc_runs;

    /* Adaptive state */
    uint64_t last_resize_ms;
    uint64_t expansion_count;
    uint64_t contraction_count;
    float last_entropy;

    /* Callbacks */
    pressure_callback_t pressure_cb;
    void* pressure_cb_data;

    /* Thread safety */
    nimcp_platform_mutex_t mutex;

    /* Timing */
    uint64_t creation_time_ms;
};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Hash pointer for allocation tracking
 */
static inline uint8_t hash_ptr(const void* ptr)
{
    uintptr_t val = (uintptr_t)ptr;
    return (uint8_t)((val >> 4) ^ (val >> 12));
}

/**
 * @brief Calculate pressure index
 *
 * WHAT: Compute normalized pressure across all pools
 * WHY:  Enable unified pressure monitoring
 * HOW:  P = Σ(used_i/quota_i)^2 / n
 */
static float calculate_pressure_index(const struct unified_pools* pools)
{
    if (!pools) return 0.0F;

    float sum = 0.0F;
    int count = 0;

    for (int i = 0; i < UNIFIED_POOL_COUNT; i++) {
        if (pools->pool_states[i].quota.max_bytes > 0) {
            float ratio = (float)pools->pool_states[i].current_bytes /
                         (float)pools->pool_states[i].quota.max_bytes;
            sum += ratio * ratio;
            count++;
        }
    }

    return count > 0 ? sqrtf(sum / count) : 0.0F;
}

/**
 * @brief Determine pressure level from index
 */
static pressure_level_t determine_pressure_level(
    const struct unified_pools* pools,
    float index)
{
    if (!pools) return PRESSURE_NONE;

    const pressure_config_t* cfg = &pools->config.pressure;

    if (index >= cfg->critical_threshold) return PRESSURE_CRITICAL;
    if (index >= cfg->high_threshold) return PRESSURE_HIGH;
    if (index >= cfg->medium_threshold) return PRESSURE_MEDIUM;
    if (index >= cfg->low_threshold) return PRESSURE_LOW;
    return PRESSURE_NONE;
}

/**
 * @brief Track allocation in hash table
 */
static bool track_allocation(
    struct unified_pools* pools,
    void* ptr,
    size_t size,
    unified_pool_type_t source,
    bool is_cow,
    bool borrowed)
{
    if (!pools || !ptr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "track_allocation: required parameter is NULL (pools, ptr)");
        return false;
    }

    /* Get or create entry */
    alloc_entry_t* entry;
    if (pools->entry_free) {
        entry = pools->entry_free;
        pools->entry_free = entry->next;
    } else {
        entry = nimcp_calloc(1, sizeof(alloc_entry_t));
        if (!entry) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "track_allocation: entry is NULL");
            return false;
        }
    }

    /* Fill entry */
    entry->ptr = ptr;
    entry->size = size;
    entry->source = source;
    entry->is_cow = is_cow;
    entry->borrowed = borrowed;

    /* Insert into hash table */
    uint8_t hash = hash_ptr(ptr);
    entry->next = pools->alloc_table[hash];
    pools->alloc_table[hash] = entry;

    /* Update pool state */
    pools->pool_states[source].current_bytes += size;
    pools->pool_states[source].total_acquires++;

    return true;
}

/**
 * @brief Find and remove allocation from tracking
 */
static alloc_entry_t* untrack_allocation(struct unified_pools* pools, void* ptr)
{
    if (!pools || !ptr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "untrack_allocation: required parameter is NULL (pools, ptr)");
        return NULL;
    }

    uint8_t hash = hash_ptr(ptr);
    alloc_entry_t** prev = &pools->alloc_table[hash];
    alloc_entry_t* entry = *prev;

    while (entry) {
        if (entry->ptr == ptr) {
            *prev = entry->next;

            /* Update pool state */
            pools->pool_states[entry->source].current_bytes -= entry->size;
            pools->pool_states[entry->source].total_releases++;

            return entry;
        }
        prev = &entry->next;
        entry = entry->next;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "untrack_allocation: operation failed");
    return NULL;
}

/**
 * @brief Free allocation entry back to pool
 */
static void free_alloc_entry(struct unified_pools* pools, alloc_entry_t* entry)
{
    if (!pools || !entry) return;
    entry->next = pools->entry_free;
    pools->entry_free = entry;
}

/**
 * @brief Select best pool for allocation
 */
static unified_pool_type_t select_pool(
    struct unified_pools* pools,
    size_t size,
    unified_pool_type_t hint)
{
    if (!pools) return UNIFIED_POOL_GENERIC;

    /* Use hint if valid and has quota */
    if (hint < UNIFIED_POOL_COUNT) {
        pool_state_t* state = &pools->pool_states[hint];
        if (state->quota.max_bytes > 0 &&
            state->current_bytes + size <= state->quota.max_bytes) {
            return hint;
        }
    }

    /* Find pool with best available space */
    float best_ratio = 1.0F;
    unified_pool_type_t best = UNIFIED_POOL_GENERIC;

    for (int i = 0; i < UNIFIED_POOL_COUNT; i++) {
        pool_state_t* state = &pools->pool_states[i];
        if (state->quota.max_bytes == 0) continue;

        float ratio = (float)(state->current_bytes + size) /
                     (float)state->quota.max_bytes;
        if (ratio < best_ratio) {
            best_ratio = ratio;
            best = (unified_pool_type_t)i;
        }
    }

    return best;
}

/**
 * @brief Allocate from underlying pool
 */
static void* allocate_from_pool(
    struct unified_pools* pools,
    unified_pool_type_t pool,
    size_t size)
{
    if (!pools) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pools is NULL");

        return NULL;

    }

    void* ptr = NULL;

    switch (pool) {
        case UNIFIED_POOL_BRAIN_DECISION:
            ptr = brain_pools_acquire_decision(pools->brain_pools);
            break;
        case UNIFIED_POOL_BRAIN_ACTIVATION:
            ptr = brain_pools_acquire_activation(pools->brain_pools,
                                                  size / sizeof(float));
            break;
        case UNIFIED_POOL_BRAIN_SPIKE:
            ptr = brain_pools_acquire_spike_event(pools->brain_pools);
            break;
        case UNIFIED_POOL_LAYER_COGNITIVE:
            ptr = layer_pools_acquire_workspace_entry(pools->layer_pools);
            break;
        case UNIFIED_POOL_LAYER_MIDDLEWARE:
            ptr = layer_pools_acquire_event_entry(pools->layer_pools);
            break;
        case UNIFIED_POOL_LAYER_TRAINING:
            ptr = layer_pools_acquire_learning_signal(pools->layer_pools);
            break;
        default:
            /* Generic allocation via nimcp_malloc */
            ptr = nimcp_malloc(size);
            break;
    }

    return ptr;
}

/**
 * @brief Release to underlying pool
 */
static void release_to_pool(
    struct unified_pools* pools,
    void* ptr,
    unified_pool_type_t pool)
{
    if (!pools || !ptr) return;

    switch (pool) {
        case UNIFIED_POOL_BRAIN_DECISION:
            brain_pools_release_decision(pools->brain_pools, ptr);
            break;
        case UNIFIED_POOL_BRAIN_ACTIVATION:
            brain_pools_release_activation(pools->brain_pools, ptr);
            break;
        case UNIFIED_POOL_BRAIN_SPIKE:
            brain_pools_release_spike_event(pools->brain_pools, ptr);
            break;
        case UNIFIED_POOL_LAYER_COGNITIVE:
            layer_pools_release_workspace_entry(pools->layer_pools, ptr);
            break;
        case UNIFIED_POOL_LAYER_MIDDLEWARE:
            layer_pools_release_event_entry(pools->layer_pools, ptr);
            break;
        case UNIFIED_POOL_LAYER_TRAINING:
            layer_pools_release_learning_signal(pools->layer_pools, ptr);
            break;
        default:
            nimcp_free(ptr);
            break;
    }
}

//=============================================================================
// Configuration Functions
//=============================================================================

unified_pools_config_t unified_pools_default_config(void)
{
    unified_pools_config_t config;
    memset(&config, 0, sizeof(config));

    /* Underlying pool configs */
    config.brain_config = brain_pools_default_config();
    config.layer_config = layer_pools_default_config();

    /* Default quotas (1MB per pool type) */
    for (int i = 0; i < UNIFIED_POOL_COUNT; i++) {
        config.quotas[i].max_bytes = 1024 * 1024;
        config.quotas[i].reserved_bytes = 64 * 1024;
        config.quotas[i].priority = 1.0F;
        config.quotas[i].mode = QUOTA_MODE_ADAPTIVE;
    }

    /* Total limit 16MB */
    config.total_memory_limit = 16 * 1024 * 1024;

    /* Adaptive sizing */
    config.adaptive.enabled = true;
    config.adaptive.entropy_threshold = 0.1F;
    config.adaptive.min_utilization = 0.3F;
    config.adaptive.max_utilization = 0.85F;
    config.adaptive.sample_window_ms = 1000;
    config.adaptive.resize_cooldown_ms = 5000;
    config.adaptive.growth_factor = 1.5F;
    config.adaptive.shrink_factor = 0.75F;

    /* Pressure management */
    config.pressure.enabled = true;
    config.pressure.low_threshold = 0.5F;
    config.pressure.medium_threshold = 0.7F;
    config.pressure.high_threshold = 0.85F;
    config.pressure.critical_threshold = 0.95F;
    config.pressure.enable_cow_on_pressure = true;
    config.pressure.enable_gc_on_pressure = true;
    config.pressure.gc_interval_ms = 1000;

    /* COW coordinator */
    config.cow.enabled = true;
    config.cow.lazy_initialization = true;
    config.cow.share_across_pools = true;
    config.cow.max_shared_templates = 256;
    config.cow.template_ttl_ms = 60000;

    /* Observability */
    config.observability.enable_metrics = true;
    config.observability.enable_tracing = false;
    config.observability.enable_histograms = false;
    config.observability.metrics_interval_ms = 1000;
    config.observability.trace_buffer_size = 1024;

    return config;
}

unified_pools_config_t unified_pools_production_config(void)
{
    unified_pools_config_t config = unified_pools_default_config();

    /* Larger quotas for production */
    for (int i = 0; i < UNIFIED_POOL_COUNT; i++) {
        config.quotas[i].max_bytes = 4 * 1024 * 1024;
        config.quotas[i].reserved_bytes = 256 * 1024;
    }
    config.total_memory_limit = 64 * 1024 * 1024;

    /* More aggressive pressure management */
    config.pressure.gc_interval_ms = 500;

    /* More COW templates */
    config.cow.max_shared_templates = 1024;

    return config;
}

unified_pools_config_t unified_pools_debug_config(void)
{
    unified_pools_config_t config = unified_pools_default_config();

    /* Enable all observability */
    config.observability.enable_metrics = true;
    config.observability.enable_tracing = true;
    config.observability.enable_histograms = true;
    config.observability.trace_buffer_size = 4096;

    return config;
}

//=============================================================================
// Core API Implementation
//=============================================================================

unified_pools_t unified_pools_create(const unified_pools_config_t* config)
{
    /* Use defaults if no config */
    unified_pools_config_t local_config;
    if (!config) {
        local_config = unified_pools_default_config();
        config = &local_config;
    }

    /* Allocate manager */
    struct unified_pools* pools = nimcp_calloc(1, sizeof(struct unified_pools));
    if (!pools) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pools is NULL");

        return NULL;

    }

    /* Store config */
    pools->config = *config;
    pools->creation_time_ms = nimcp_time_monotonic_ms();

    /* Initialize mutex */
    if (nimcp_platform_mutex_init(&pools->mutex, false) != 0) {
        nimcp_free(pools);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "unified_pools_create: validation failed");
        return NULL;
    }

    /* Create underlying pools */
    pools->brain_pools = brain_pools_create(&config->brain_config);
    if (!pools->brain_pools) {
        nimcp_platform_mutex_destroy(&pools->mutex);
        nimcp_free(pools);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unified_pools_create: pools->brain_pools is NULL");
        return NULL;
    }

    pools->layer_pools = layer_pools_create(&config->layer_config,
                                             pools->brain_pools);
    if (!pools->layer_pools) {
        brain_pools_destroy(pools->brain_pools);
        nimcp_platform_mutex_destroy(&pools->mutex);
        nimcp_free(pools);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unified_pools_create: pools->layer_pools is NULL");
        return NULL;
    }

    pools->owns_pools = true;

    /* Initialize pool states */
    for (int i = 0; i < UNIFIED_POOL_COUNT; i++) {
        pools->pool_states[i].quota = config->quotas[i];
    }

    /* Pre-allocate COW templates */
    pools->template_capacity = config->cow.max_shared_templates;

    return pools;
}

void unified_pools_destroy(unified_pools_t pools)
{
    if (!pools) return;

    nimcp_platform_mutex_lock(&pools->mutex);

    /* Free COW templates */
    cow_template_t* tmpl = pools->templates;
    while (tmpl) {
        cow_template_t* next = tmpl->next;
        if (tmpl->data) nimcp_free(tmpl->data);
        nimcp_free(tmpl);
        tmpl = next;
    }

    tmpl = pools->template_free;
    while (tmpl) {
        cow_template_t* next = tmpl->next;
        nimcp_free(tmpl);
        tmpl = next;
    }

    /* Free allocation entries */
    for (int i = 0; i < 256; i++) {
        alloc_entry_t* entry = pools->alloc_table[i];
        while (entry) {
            alloc_entry_t* next = entry->next;
            nimcp_free(entry);
            entry = next;
        }
    }

    alloc_entry_t* entry = pools->entry_free;
    while (entry) {
        alloc_entry_t* next = entry->next;
        nimcp_free(entry);
        entry = next;
    }

    /* Destroy underlying pools */
    if (pools->owns_pools) {
        if (pools->layer_pools) layer_pools_destroy(pools->layer_pools);
        if (pools->brain_pools) brain_pools_destroy(pools->brain_pools);
    }

    nimcp_platform_mutex_unlock(&pools->mutex);
    nimcp_platform_mutex_destroy(&pools->mutex);

    nimcp_free(pools);
}

//=============================================================================
// Unified Allocation API
//=============================================================================

allocation_result_t unified_pools_acquire(
    unified_pools_t pools,
    size_t size,
    unified_pool_type_t hint)
{
    allocation_result_t result = {0};

    if (!pools || size == 0) return result;

    nimcp_platform_mutex_lock(&pools->mutex);

    /* Select pool */
    unified_pool_type_t pool = select_pool(pools, size, hint);

    /* Check quota */
    pool_state_t* state = &pools->pool_states[pool];
    if (state->quota.mode == QUOTA_MODE_HARD &&
        state->current_bytes + size > state->quota.max_bytes) {
        state->quota_violations++;
        nimcp_platform_mutex_unlock(&pools->mutex);
        return result;
    }

    /* Allocate */
    void* ptr = allocate_from_pool(pools, pool, size);
    if (!ptr) {
        nimcp_platform_mutex_unlock(&pools->mutex);
        return result;
    }

    /* Track */
    track_allocation(pools, ptr, size, pool, false, false);

    /* Update pressure */
    float pressure_idx = calculate_pressure_index(pools);
    pressure_level_t new_level = determine_pressure_level(pools, pressure_idx);
    if (new_level != pools->current_pressure) {
        pools->current_pressure = new_level;
        if (pools->pressure_cb) {
            pools->pressure_cb(pools, new_level, pressure_idx,
                              pools->pressure_cb_data);
        }
    }

    nimcp_platform_mutex_unlock(&pools->mutex);

    /* Fill result */
    result.ptr = ptr;
    result.source = pool;
    result.is_cow = false;
    result.borrowed = false;
    result.actual_size = size;

    return result;
}

void unified_pools_release(unified_pools_t pools, void* ptr)
{
    if (!pools || !ptr) return;

    nimcp_platform_mutex_lock(&pools->mutex);

    /* Find allocation */
    alloc_entry_t* entry = untrack_allocation(pools, ptr);
    if (!entry) {
        nimcp_platform_mutex_unlock(&pools->mutex);
        return;
    }

    /* Release to pool */
    release_to_pool(pools, ptr, entry->source);

    /* Return entry to free list */
    free_alloc_entry(pools, entry);

    nimcp_platform_mutex_unlock(&pools->mutex);
}

//=============================================================================
// COW API Implementation
//=============================================================================

unified_cow_handle_t unified_pools_cow_acquire(
    unified_pools_t pools,
    const void* template_data,
    size_t size,
    unified_pool_type_t pool_hint)
{
    if (!pools || !template_data || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unified_pools_cow_acquire: required parameter is NULL (pools, template_data)");
        return NULL;
    }

    nimcp_platform_mutex_lock(&pools->mutex);

    /* Get or create template entry */
    cow_template_t* tmpl;
    if (pools->template_free) {
        tmpl = pools->template_free;
        pools->template_free = tmpl->next;
    } else {
        tmpl = nimcp_calloc(1, sizeof(cow_template_t));
        if (!tmpl) {
            nimcp_platform_mutex_unlock(&pools->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unified_pools_cow_acquire: tmpl is NULL");
            return NULL;
        }
    }

    /* Copy template data */
    tmpl->data = nimcp_malloc(size);
    if (!tmpl->data) {
        tmpl->next = pools->template_free;
        pools->template_free = tmpl;
        nimcp_platform_mutex_unlock(&pools->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unified_pools_cow_acquire: tmpl->data is NULL");
        return NULL;
    }
    memcpy(tmpl->data, template_data, size);

    tmpl->size = size;
    tmpl->refcount = 1;
    tmpl->pool_hint = pool_hint;
    tmpl->created_ms = nimcp_time_monotonic_ms();

    /* Add to active list */
    tmpl->next = pools->templates;
    pools->templates = tmpl;
    pools->template_count++;

    /* Create handle */
    struct unified_cow_handle* handle = nimcp_calloc(1,
                                          sizeof(struct unified_cow_handle));
    if (!handle) {
        /* Cleanup template */
        pools->templates = tmpl->next;
        pools->template_count--;
        nimcp_free(tmpl->data);
        tmpl->next = pools->template_free;
        pools->template_free = tmpl;
        nimcp_platform_mutex_unlock(&pools->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unified_pools_cow_acquire: handle is NULL");
        return NULL;
    }

    handle->template_ref = tmpl;
    handle->private_data = NULL;
    handle->is_private = false;
    handle->source_pool = pool_hint;

    nimcp_platform_mutex_unlock(&pools->mutex);

    return handle;
}

const void* unified_pools_cow_read(unified_cow_handle_t handle)
{
    if (!handle) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "handle is NULL");

        return NULL;

    }

    /* Return private copy if exists, else template */
    return handle->is_private ? handle->private_data
                              : handle->template_ref->data;
}

void* unified_pools_cow_write(unified_cow_handle_t handle)
{
    if (!handle) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "handle is NULL");

        return NULL;

    }

    /* Already private */
    if (handle->is_private) {
        return handle->private_data;
    }

    /* Trigger copy-on-write */
    cow_template_t* tmpl = handle->template_ref;
    void* private_copy = nimcp_malloc(tmpl->size);
    if (!private_copy) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "private_copy is NULL");

        return NULL;

    }

    memcpy(private_copy, tmpl->data, tmpl->size);

    handle->private_data = private_copy;
    handle->is_private = true;

    return handle->private_data;
}

void unified_pools_cow_release(unified_pools_t pools, unified_cow_handle_t handle)
{
    if (!pools || !handle) return;

    nimcp_platform_mutex_lock(&pools->mutex);

    /* Free private copy if exists */
    if (handle->is_private && handle->private_data) {
        nimcp_free(handle->private_data);
    }

    /* Decrement template refcount */
    cow_template_t* tmpl = handle->template_ref;
    if (tmpl) {
        tmpl->refcount--;

        /* Cleanup template if no more references */
        if (tmpl->refcount == 0) {
            /* Remove from active list */
            cow_template_t** prev = &pools->templates;
            while (*prev && *prev != tmpl) {
                prev = &(*prev)->next;
            }
            if (*prev) {
                *prev = tmpl->next;
            }
            pools->template_count--;

            /* Free template data */
            if (tmpl->data) nimcp_free(tmpl->data);

            /* Return to free list */
            tmpl->data = NULL;
            tmpl->next = pools->template_free;
            pools->template_free = tmpl;
        }
    }

    nimcp_platform_mutex_unlock(&pools->mutex);

    /* Free handle */
    nimcp_free(handle);
}

//=============================================================================
// Quota Management API
//=============================================================================

bool unified_pools_set_quota(
    unified_pools_t pools,
    unified_pool_type_t pool,
    const pool_quota_t* quota)
{
    if (!pools || pool >= UNIFIED_POOL_COUNT || !quota) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unified_pools_set_quota: required parameter is NULL (pools, quota)");
        return false;
    }

    nimcp_platform_mutex_lock(&pools->mutex);
    pools->pool_states[pool].quota = *quota;
    nimcp_platform_mutex_unlock(&pools->mutex);

    return true;
}

bool unified_pools_get_quota(
    unified_pools_t pools,
    unified_pool_type_t pool,
    pool_quota_t* quota)
{
    if (!pools || pool >= UNIFIED_POOL_COUNT || !quota) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unified_pools_get_quota: required parameter is NULL (pools, quota)");
        return false;
    }

    nimcp_platform_mutex_lock(&pools->mutex);
    *quota = pools->pool_states[pool].quota;
    nimcp_platform_mutex_unlock(&pools->mutex);

    return true;
}

bool unified_pools_check_quota(
    unified_pools_t pools,
    unified_pool_type_t pool,
    size_t size)
{
    if (!pools || pool >= UNIFIED_POOL_COUNT) {
        return false;
    }

    nimcp_platform_mutex_lock(&pools->mutex);
    pool_state_t* state = &pools->pool_states[pool];
    bool fits = (state->current_bytes + size <= state->quota.max_bytes);
    nimcp_platform_mutex_unlock(&pools->mutex);

    return fits;
}

//=============================================================================
// Pressure Management API
//=============================================================================

pressure_level_t unified_pools_get_pressure_level(unified_pools_t pools)
{
    if (!pools) return PRESSURE_NONE;

    nimcp_platform_mutex_lock(&pools->mutex);
    pressure_level_t level = pools->current_pressure;
    nimcp_platform_mutex_unlock(&pools->mutex);

    return level;
}

bool unified_pools_get_pressure_metrics(
    unified_pools_t pools,
    pressure_metrics_t* metrics)
{
    if (!pools || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unified_pools_get_pressure_metrics: required parameter is NULL (pools, metrics)");
        return false;
    }

    nimcp_platform_mutex_lock(&pools->mutex);

    metrics->current_level = pools->current_pressure;
    metrics->pressure_index = calculate_pressure_index(pools);
    metrics->pressure_trend = 0.0F;  /* TODO: Track trend */
    metrics->gc_runs = pools->gc_runs;
    metrics->cow_forced = 0;  /* TODO: Track forced COW */
    metrics->borrowing_events = 0;  /* TODO: Track borrowing */

    uint64_t total_violations = 0;
    for (int i = 0; i < UNIFIED_POOL_COUNT; i++) {
        total_violations += pools->pool_states[i].quota_violations;
    }
    metrics->quota_rejections = total_violations;

    nimcp_platform_mutex_unlock(&pools->mutex);

    return true;
}

bool unified_pools_on_pressure(
    unified_pools_t pools,
    pressure_callback_t callback,
    void* user_data)
{
    if (!pools) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unified_pools_on_pressure: pools is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(&pools->mutex);
    pools->pressure_cb = callback;
    pools->pressure_cb_data = user_data;
    nimcp_platform_mutex_unlock(&pools->mutex);

    return true;
}

size_t unified_pools_gc(unified_pools_t pools)
{
    if (!pools) return 0;

    nimcp_platform_mutex_lock(&pools->mutex);

    size_t reclaimed = 0;
    uint64_t now = nimcp_time_monotonic_ms();

    /* Check cooldown */
    if (now - pools->last_gc_ms < pools->config.pressure.gc_interval_ms) {
        nimcp_platform_mutex_unlock(&pools->mutex);
        return 0;
    }

    /* Cleanup expired COW templates */
    uint64_t ttl = pools->config.cow.template_ttl_ms;
    cow_template_t** prev = &pools->templates;
    cow_template_t* tmpl = *prev;

    while (tmpl) {
        cow_template_t* next = tmpl->next;

        if (tmpl->refcount == 0 || (now - tmpl->created_ms > ttl)) {
            *prev = next;
            pools->template_count--;
            reclaimed += tmpl->size;

            if (tmpl->data) nimcp_free(tmpl->data);
            tmpl->data = NULL;
            tmpl->next = pools->template_free;
            pools->template_free = tmpl;
        } else {
            prev = &tmpl->next;
        }

        tmpl = next;
    }

    pools->last_gc_ms = now;
    pools->gc_runs++;

    nimcp_platform_mutex_unlock(&pools->mutex);

    return reclaimed;
}

//=============================================================================
// Adaptive Sizing API
//=============================================================================

uint32_t unified_pools_adaptive_resize(unified_pools_t pools)
{
    if (!pools || !pools->config.adaptive.enabled) return 0;

    nimcp_platform_mutex_lock(&pools->mutex);

    uint64_t now = nimcp_time_monotonic_ms();

    /* Check cooldown */
    if (now - pools->last_resize_ms < pools->config.adaptive.resize_cooldown_ms) {
        nimcp_platform_mutex_unlock(&pools->mutex);
        return 0;
    }

    uint32_t resized = 0;

    /* Check each pool */
    for (int i = 0; i < UNIFIED_POOL_COUNT; i++) {
        pool_state_t* state = &pools->pool_states[i];
        if (state->quota.max_bytes == 0) continue;

        float util = (float)state->current_bytes / (float)state->quota.max_bytes;

        if (util > pools->config.adaptive.max_utilization) {
            /* Expand */
            state->quota.max_bytes = (size_t)(state->quota.max_bytes *
                                     pools->config.adaptive.growth_factor);
            pools->expansion_count++;
            resized++;
        } else if (util < pools->config.adaptive.min_utilization) {
            /* Contract (but not below reserved) */
            size_t new_max = (size_t)(state->quota.max_bytes *
                            pools->config.adaptive.shrink_factor);
            if (new_max >= state->quota.reserved_bytes) {
                state->quota.max_bytes = new_max;
                pools->contraction_count++;
                resized++;
            }
        }
    }

    if (resized > 0) {
        pools->last_resize_ms = now;
    }

    nimcp_platform_mutex_unlock(&pools->mutex);

    return resized;
}

bool unified_pools_get_adaptive_metrics(
    unified_pools_t pools,
    adaptive_metrics_t* metrics)
{
    if (!pools || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unified_pools_get_adaptive_metrics: required parameter is NULL (pools, metrics)");
        return false;
    }

    nimcp_platform_mutex_lock(&pools->mutex);

    metrics->expansions = pools->expansion_count;
    metrics->contractions = pools->contraction_count;
    metrics->entropy_current = pools->last_entropy;
    metrics->entropy_delta = 0.0F;  /* TODO */
    metrics->utilization_trend = 0.0F;  /* TODO */
    metrics->last_resize_ms = pools->last_resize_ms;

    nimcp_platform_mutex_unlock(&pools->mutex);

    return true;
}

bool unified_pools_get_recommended_config(
    unified_pools_t pools,
    unified_pools_config_t* recommended)
{
    if (!pools || !recommended) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unified_pools_get_recommended_config: required parameter is NULL (pools, recommended)");
        return false;
    }

    nimcp_platform_mutex_lock(&pools->mutex);

    /* Start with current config */
    *recommended = pools->config;

    /* Adjust quotas based on actual usage */
    for (int i = 0; i < UNIFIED_POOL_COUNT; i++) {
        pool_state_t* state = &pools->pool_states[i];
        if (state->total_acquires == 0) continue;

        /* Recommend 2x peak usage as quota */
        size_t peak = state->current_bytes;  /* Simplified */
        recommended->quotas[i].max_bytes = peak * 2;
    }

    nimcp_platform_mutex_unlock(&pools->mutex);

    return true;
}

//=============================================================================
// Metrics API Implementation
//=============================================================================

bool unified_pools_get_metrics(
    unified_pools_t pools,
    unified_metrics_t* metrics)
{
    if (!pools || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unified_pools_get_metrics: required parameter is NULL (pools, metrics)");
        return false;
    }

    nimcp_platform_mutex_lock(&pools->mutex);

    memset(metrics, 0, sizeof(unified_metrics_t));

    /* Aggregate metrics */
    size_t total_alloc = 0;
    for (int i = 0; i < UNIFIED_POOL_COUNT; i++) {
        pool_state_t* state = &pools->pool_states[i];

        metrics->pools[i].type = (unified_pool_type_t)i;
        metrics->pools[i].allocated_bytes = state->current_bytes;
        metrics->pools[i].quota_bytes = state->quota.max_bytes;
        metrics->pools[i].utilization = state->quota.max_bytes > 0 ?
            (float)state->current_bytes / (float)state->quota.max_bytes : 0.0F;
        metrics->pools[i].total_acquires = state->total_acquires;
        metrics->pools[i].total_releases = state->total_releases;
        metrics->pools[i].cow_triggers = state->cow_triggers;
        metrics->pools[i].quota_violations = state->quota_violations;

        total_alloc += state->current_bytes;
    }

    metrics->total_allocated = total_alloc;
    metrics->total_limit = pools->config.total_memory_limit;
    metrics->global_utilization = pools->config.total_memory_limit > 0 ?
        (float)total_alloc / (float)pools->config.total_memory_limit : 0.0F;

    /* Pressure metrics */
    metrics->pressure.current_level = pools->current_pressure;
    metrics->pressure.pressure_index = calculate_pressure_index(pools);
    metrics->pressure.gc_runs = pools->gc_runs;

    /* COW metrics */
    metrics->cow.total_templates = pools->template_capacity;
    metrics->cow.active_templates = pools->template_count;

    /* Timing */
    uint64_t now = nimcp_time_monotonic_ms();
    metrics->uptime_ms = now - pools->creation_time_ms;
    metrics->metrics_timestamp_ms = now;

    nimcp_platform_mutex_unlock(&pools->mutex);

    return true;
}

bool unified_pools_get_pool_metrics(
    unified_pools_t pools,
    unified_pool_type_t pool,
    pool_metrics_t* metrics)
{
    if (!pools || pool >= UNIFIED_POOL_COUNT || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unified_pools_get_pool_metrics: required parameter is NULL (pools, metrics)");
        return false;
    }

    nimcp_platform_mutex_lock(&pools->mutex);

    pool_state_t* state = &pools->pool_states[pool];

    metrics->type = pool;
    metrics->allocated_bytes = state->current_bytes;
    metrics->quota_bytes = state->quota.max_bytes;
    metrics->utilization = state->quota.max_bytes > 0 ?
        (float)state->current_bytes / (float)state->quota.max_bytes : 0.0F;
    metrics->total_acquires = state->total_acquires;
    metrics->total_releases = state->total_releases;
    metrics->cow_triggers = state->cow_triggers;
    metrics->quota_violations = state->quota_violations;

    nimcp_platform_mutex_unlock(&pools->mutex);

    return true;
}

bool unified_pools_get_cow_metrics(
    unified_pools_t pools,
    cow_metrics_t* metrics)
{
    if (!pools || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unified_pools_get_cow_metrics: required parameter is NULL (pools, metrics)");
        return false;
    }

    nimcp_platform_mutex_lock(&pools->mutex);

    memset(metrics, 0, sizeof(cow_metrics_t));
    metrics->total_templates = pools->template_capacity;
    metrics->active_templates = pools->template_count;

    /* Calculate memory saved */
    size_t saved = 0;
    size_t overhead = 0;
    uint32_t total_refs = 0;

    cow_template_t* tmpl = pools->templates;
    while (tmpl) {
        if (tmpl->refcount > 1) {
            saved += tmpl->size * (tmpl->refcount - 1);
        }
        overhead += sizeof(cow_template_t);
        total_refs += tmpl->refcount;
        tmpl = tmpl->next;
    }

    metrics->memory_saved_bytes = saved;
    metrics->copy_overhead_bytes = overhead;
    metrics->cow_efficiency = (saved + overhead) > 0 ?
        (float)saved / (float)(saved + overhead) : 0.0F;
    metrics->avg_refcount = pools->template_count > 0 ?
        (float)total_refs / (float)pools->template_count : 0.0F;

    nimcp_platform_mutex_unlock(&pools->mutex);

    return true;
}

void unified_pools_reset_metrics(unified_pools_t pools)
{
    if (!pools) return;

    nimcp_platform_mutex_lock(&pools->mutex);

    for (int i = 0; i < UNIFIED_POOL_COUNT; i++) {
        pools->pool_states[i].total_acquires = 0;
        pools->pool_states[i].total_releases = 0;
        pools->pool_states[i].cow_triggers = 0;
        pools->pool_states[i].quota_violations = 0;
    }

    pools->gc_runs = 0;
    pools->expansion_count = 0;
    pools->contraction_count = 0;

    nimcp_platform_mutex_unlock(&pools->mutex);
}

//=============================================================================
// Underlying Pool Access
//=============================================================================

brain_pools_t unified_pools_get_brain_pools(unified_pools_t pools)
{
    return pools ? pools->brain_pools : NULL;
}

layer_pools_t unified_pools_get_layer_pools(unified_pools_t pools)
{
    return pools ? pools->layer_pools : NULL;
}

//=============================================================================
// Utility Functions
//=============================================================================

size_t unified_pools_calculate_memory(const unified_pools_config_t* config)
{
    unified_pools_config_t def;
    if (!config) {
        def = unified_pools_default_config();
        config = &def;
    }

    size_t total = 0;

    /* Underlying pools */
    total += brain_pools_calculate_memory(&config->brain_config);
    total += layer_pools_calculate_memory(&config->layer_config);

    /* COW templates */
    total += config->cow.max_shared_templates * sizeof(cow_template_t);

    /* Allocation tracking */
    total += 256 * sizeof(alloc_entry_t*);

    /* Manager overhead */
    total += sizeof(struct unified_pools);

    return total;
}

const char* unified_pools_get_pool_name(unified_pool_type_t pool)
{
    switch (pool) {
        case UNIFIED_POOL_BRAIN_DECISION:   return "Brain/Decision";
        case UNIFIED_POOL_BRAIN_ACTIVATION: return "Brain/Activation";
        case UNIFIED_POOL_BRAIN_SPIKE:      return "Brain/Spike";
        case UNIFIED_POOL_BRAIN_FEATURE:    return "Brain/Feature";
        case UNIFIED_POOL_LAYER_COGNITIVE:  return "Layer/Cognitive";
        case UNIFIED_POOL_LAYER_MIDDLEWARE: return "Layer/Middleware";
        case UNIFIED_POOL_LAYER_TRAINING:   return "Layer/Training";
        case UNIFIED_POOL_BUFFER:           return "Buffer";
        case UNIFIED_POOL_GENERIC:          return "Generic";
        default:                            return "Unknown";
    }
}

const char* unified_pools_get_pressure_name(pressure_level_t level)
{
    switch (level) {
        case PRESSURE_NONE:     return "None";
        case PRESSURE_LOW:      return "Low";
        case PRESSURE_MEDIUM:   return "Medium";
        case PRESSURE_HIGH:     return "High";
        case PRESSURE_CRITICAL: return "Critical";
        default:                return "Unknown";
    }
}

bool unified_pools_is_healthy(unified_pools_t pools)
{
    if (!pools) {
        return false;
    }

    nimcp_platform_mutex_lock(&pools->mutex);

    /* Check pressure level */
    bool healthy = (pools->current_pressure < PRESSURE_CRITICAL);

    /* Check underlying pools */
    healthy = healthy && brain_pools_is_performant(pools->brain_pools);
    healthy = healthy && layer_pools_is_performant(pools->layer_pools);

    nimcp_platform_mutex_unlock(&pools->mutex);

    return healthy;
}

bool unified_pools_is_performant(unified_pools_t pools)
{
    if (!pools) {
        return false;
    }

    nimcp_platform_mutex_lock(&pools->mutex);

    /* Check pressure level */
    bool performant = (pools->current_pressure <= PRESSURE_MEDIUM);

    /* Fresh pools are performant */
    uint64_t total_ops = 0;
    for (int i = 0; i < UNIFIED_POOL_COUNT; i++) {
        total_ops += pools->pool_states[i].total_acquires;
    }

    if (total_ops == 0) {
        nimcp_platform_mutex_unlock(&pools->mutex);
        return true;
    }

    nimcp_platform_mutex_unlock(&pools->mutex);

    return performant;
}
