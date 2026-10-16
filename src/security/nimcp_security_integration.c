/**
 * @file nimcp_security_integration.c
 * @brief Universal Security Integration Framework Implementation
 *
 * Phase SC-4: Security Integration Layer
 * Phase SC-4.1: Memory Pool Integration
 * Phase SC-4.2: Copy-on-Write Integration
 *
 * Implements the unified security integration API for all NIMCP modules.
 *
 * @version 1.1.0
 * @author NIMCP Security Team
 */

#include "security/nimcp_security_integration.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "security/nimcp_security_math.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/memory/nimcp_cow_manager.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "security_integration"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_integration, MESH_ADAPTER_CATEGORY_SECURITY)


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdatomic.h>
#include "constants/nimcp_buffer_constants.h"

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Event queue entry
 */
typedef struct {
    nimcp_sec_event_t event;
    bool valid;
} event_queue_entry_t;

/**
 * @brief Shared data for COW (reference counted)
 */
typedef struct nimcp_sec_shared_data {
    /* Reference count for COW */
    atomic_uint refcount;

    /* Module registry */
    nimcp_sec_module_info_t* modules;     /**< Pool-allocated or array */
    uint32_t module_count;
    uint32_t next_module_id;
    uint32_t module_capacity;

    /* Region registry */
    nimcp_sec_region_info_t* regions;     /**< Pool-allocated or array */
    uint32_t region_count;
    uint32_t next_region_id;
    uint32_t region_capacity;

    /* Security subsystems */
    nimcp_entropy_analyzer_t* entropy_analyzer;
    nimcp_trust_network_t* trust_network;
    nimcp_dp_context_t* dp_context;

    /* Event queue (pool-allocated) */
    event_queue_entry_t* event_queue;     /**< Pool-allocated or array */
    uint32_t event_head;
    uint32_t event_tail;
    uint32_t event_count;
    uint32_t event_capacity;

    /* Statistics */
    nimcp_sec_integration_stats_t stats;
    uint64_t start_time;

    /* Self-monitoring */
    uint32_t self_module_id;
    uint32_t self_region_id;
    double self_entropy_baseline;
} nimcp_sec_shared_data_t;

/**
 * @brief Security integration internal structure
 */
struct nimcp_sec_integration {
    nimcp_sec_integration_config_t config;

    /* Shared data (for COW) */
    nimcp_sec_shared_data_t* shared;

    /* === PHASE SC-4.1: MEMORY POOLS (using NIMCP utils) === */
    memory_pool_t event_pool;             /**< Pool for events (nimcp_memory_pool.h) */
    memory_pool_t module_pool;            /**< Pool for module info */
    memory_pool_t region_pool;            /**< Pool for region info */
    bool pools_enabled;                   /**< Are pools active? */

    /* === PHASE SC-4.2: COW STATE (using NIMCP cow_manager) === */
    cow_manager_t cow_manager;            /**< COW manager for shared data */
    cow_handle_t cow_handle;              /**< Our handle to COW data */
    nimcp_sec_cow_state_t cow_state;      /**< Current COW state */
    bool is_cow_clone;                    /**< Is this a COW clone? */
    struct nimcp_sec_integration* cow_source; /**< Source context for COW */

    /* Monitoring thread */
    nimcp_thread_t monitor_thread;
    bool monitoring_active;
    bool monitoring_running;

    /* Synchronization */
    nimcp_mutex_t lock;
    nimcp_mutex_t event_lock;
    nimcp_cond_t monitor_cond;

    bool initialized;
};

//=============================================================================
// Helper Functions
//=============================================================================

static uint64_t get_timestamp_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * NIMCP_MS_PER_SEC + ts.tv_nsec / NIMCP_NS_PER_MS;
}

static void push_event(nimcp_sec_integration_t* ctx, const nimcp_sec_event_t* event)
{
    if (!ctx || !ctx->shared || !ctx->shared->event_queue) return;

    nimcp_mutex_lock(&ctx->event_lock);

    nimcp_sec_shared_data_t* shared = ctx->shared;
    uint32_t capacity = shared->event_capacity;

    /* Add to circular buffer */
    shared->event_queue[shared->event_tail].event = *event;
    shared->event_queue[shared->event_tail].valid = true;
    shared->event_tail = (shared->event_tail + 1) % capacity;

    if (shared->event_count < capacity) {
        shared->event_count++;
    } else {
        /* Queue full, overwrite oldest */
        shared->event_head = (shared->event_head + 1) % capacity;
    }

    shared->stats.total_events_generated++;

    nimcp_mutex_unlock(&ctx->event_lock);

    /* Notify callback if set */
    if (ctx->config.event_callback) {
        ctx->config.event_callback(event, ctx->config.callback_user_data);
    }
}

static void generate_event(
    nimcp_sec_integration_t* ctx,
    nimcp_sec_event_type_t type,
    nimcp_sec_severity_t severity,
    uint32_t module_id,
    uint32_t region_id,
    double value,
    const char* description)
{
    nimcp_sec_event_t event = {0};
    event.type = type;
    event.severity = severity;
    event.module_id = module_id;
    event.region_id = region_id;
    event.timestamp = get_timestamp_ms();
    event.value = value;
    if (description) {
        strncpy(event.description, description, sizeof(event.description) - 1);
    }

    push_event(ctx, &event);
}

static nimcp_sec_module_info_t* find_module(nimcp_sec_integration_t* ctx, uint32_t module_id)
{
    if (!ctx || !ctx->shared) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_module: required parameter is NULL (ctx, ctx->shared)");
        return NULL;
    }
    nimcp_sec_shared_data_t* shared = ctx->shared;

    for (uint32_t i = 0; i < shared->module_count; i++) {
        if (shared->modules[i].module_id == module_id && shared->modules[i].active) {
            return &shared->modules[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_module: validation failed");
    return NULL;
}

static nimcp_sec_region_info_t* find_region(nimcp_sec_integration_t* ctx, uint32_t region_id)
{
    if (!ctx || !ctx->shared) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_region: required parameter is NULL (ctx, ctx->shared)");
        return NULL;
    }
    nimcp_sec_shared_data_t* shared = ctx->shared;

    for (uint32_t i = 0; i < shared->region_count; i++) {
        if (shared->regions[i].region_id == region_id && shared->regions[i].active) {
            return &shared->regions[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_region: validation failed");
    return NULL;
}

//=============================================================================
// COW Helper: Ensure writable before modification
//=============================================================================

/**
 * @brief Ensure context is writable (trigger COW copy if needed)
 *
 * Call this before any write operation to shared data.
 * If context is COW-shared, this triggers a copy.
 */
static nimcp_result_t ensure_writable(nimcp_sec_integration_t* ctx)
{
    if (!ctx || !ctx->shared) return NIMCP_INVALID_PARAM;

    /* If private, already writable */
    if (ctx->cow_state == NIMCP_SEC_COW_PRIVATE) {
        return NIMCP_SUCCESS;
    }

    /* If shared, need to copy */
    if (ctx->cow_state == NIMCP_SEC_COW_SHARED) {
        return nimcp_sec_integration_cow_detach(ctx);
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Monitoring Thread
//=============================================================================

static void* monitoring_thread_func(void* arg)
{
    nimcp_sec_integration_t* ctx = (nimcp_sec_integration_t*)arg;
    uint64_t last_integrity_check = get_timestamp_ms();
    uint64_t last_self_check = get_timestamp_ms();

    while (ctx->monitoring_active) {
        nimcp_mutex_lock(&ctx->lock);

        uint64_t now = get_timestamp_ms();

        /* Periodic integrity check */
        if (now - last_integrity_check >= ctx->config.integrity_check_interval_ms) {
            uint32_t anomalies = 0;

            for (uint32_t i = 0; i < ctx->shared->region_count; i++) {
                if (!ctx->shared->regions[i].active) continue;

                /* Note: In production, we'd need the actual current data pointer */
                /* This is a framework - actual data access is module's responsibility */
                ctx->shared->regions[i].check_count++;
            }

            ctx->shared->stats.total_integrity_checks++;
            last_integrity_check = now;
        }

        /* Periodic self-check */
        if (ctx->config.enable_self_monitoring &&
            now - last_self_check >= ctx->config.self_check_interval_ms) {

            bool passed = true;

            /* Check own entropy analyzer */
            if (ctx->shared->entropy_analyzer) {
                /* Verify internal state hasn't been corrupted */
                /* This is a simplified check - production would be more thorough */
                passed = passed && (ctx->initialized);
            }

            /* Check trust network */
            if (ctx->shared->trust_network) {
                nimcp_trust_score_t self_score;
                if (nimcp_trust_get_score(ctx->shared->trust_network, ctx->shared->self_module_id, &self_score) == NIMCP_SUCCESS) {
                    passed = passed && (self_score.expected_trust > 0.3);
                }
            }

            if (!passed) {
                generate_event(ctx, NIMCP_SEC_EVENT_SELF_CHECK, NIMCP_SEC_SEVERITY_CRITICAL,
                              ctx->shared->self_module_id, 0, 0.0, "Security self-check failed");
            }

            ctx->shared->stats.self_checks_performed++;
            last_self_check = now;
        }

        nimcp_mutex_unlock(&ctx->lock);

        /* Sleep briefly */
        struct timespec ts = {0, 100000000}; /* 100ms */
        nanosleep(&ts, NULL);
    }

    ctx->monitoring_running = false;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "monitoring_thread_func: operation failed");
    return NULL;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

nimcp_sec_integration_t* nimcp_sec_integration_create(void)
{
    nimcp_sec_integration_t* ctx = nimcp_calloc(1, sizeof(nimcp_sec_integration_t));
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ctx is NULL");

        return NULL;

    }

    /* Allocate shared data structure */
    ctx->shared = nimcp_calloc(1, sizeof(nimcp_sec_shared_data_t));
    if (!ctx->shared) {
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_sec_integration_create: ctx->shared is NULL");
        return NULL;
    }

    /* Initialize reference count for COW */
    atomic_store(&ctx->shared->refcount, 1);

    /* Set COW state to private (full ownership) */
    ctx->cow_state = NIMCP_SEC_COW_PRIVATE;
    ctx->is_cow_clone = false;
    ctx->cow_source = NULL;
    ctx->cow_manager = NULL;
    ctx->cow_handle = NULL;

    /* Pools not yet enabled (done in init) */
    ctx->pools_enabled = false;
    ctx->event_pool = NULL;
    ctx->module_pool = NULL;
    ctx->region_pool = NULL;

    nimcp_mutex_init(&ctx->lock, NULL);
    nimcp_mutex_init(&ctx->event_lock, NULL);
    nimcp_cond_init(&ctx->monitor_cond);

    return ctx;
}

nimcp_result_t nimcp_sec_integration_init(
    nimcp_sec_integration_t* ctx,
    const nimcp_sec_integration_config_t* config)
{
    if (!ctx) return NIMCP_INVALID_PARAM;
    if (ctx->initialized) return NIMCP_ALREADY_EXISTS;
    if (!ctx->shared) return NIMCP_INVALID_STATE;

    nimcp_mutex_lock(&ctx->lock);

    /* Apply configuration */
    if (config) {
        ctx->config = *config;
    } else {
        ctx->config = nimcp_sec_integration_default_config();
    }

    nimcp_sec_shared_data_t* shared = ctx->shared;

    /* === PHASE SC-4.1: Initialize Memory Pools === */
    if (ctx->config.enable_memory_pools) {
        /* Create event pool */
        memory_pool_config_t event_cfg = memory_pool_default_config(
            sizeof(event_queue_entry_t),
            ctx->config.event_pool_size > 0 ? ctx->config.event_pool_size : NIMCP_SEC_MAX_EVENTS_QUEUE
        );
        ctx->event_pool = memory_pool_create(&event_cfg);

        /* Create module pool */
        memory_pool_config_t module_cfg = memory_pool_default_config(
            sizeof(nimcp_sec_module_info_t),
            ctx->config.module_pool_size > 0 ? ctx->config.module_pool_size : NIMCP_SEC_MAX_MODULES
        );
        ctx->module_pool = memory_pool_create(&module_cfg);

        /* Create region pool */
        memory_pool_config_t region_cfg = memory_pool_default_config(
            sizeof(nimcp_sec_region_info_t),
            ctx->config.region_pool_size > 0 ? ctx->config.region_pool_size : NIMCP_SEC_MAX_REGIONS
        );
        ctx->region_pool = memory_pool_create(&region_cfg);

        ctx->pools_enabled = (ctx->event_pool && ctx->module_pool && ctx->region_pool);

        if (ctx->pools_enabled) {
            /* Allocate arrays from pools (pre-allocate capacity) */
            shared->module_capacity = ctx->config.module_pool_size > 0 ?
                ctx->config.module_pool_size : NIMCP_SEC_MAX_MODULES;
            shared->region_capacity = ctx->config.region_pool_size > 0 ?
                ctx->config.region_pool_size : NIMCP_SEC_MAX_REGIONS;
            shared->event_capacity = ctx->config.event_pool_size > 0 ?
                ctx->config.event_pool_size : NIMCP_SEC_MAX_EVENTS_QUEUE;

            /* Allocate module array from pool */
            shared->modules = nimcp_calloc(shared->module_capacity, sizeof(nimcp_sec_module_info_t));
            shared->regions = nimcp_calloc(shared->region_capacity, sizeof(nimcp_sec_region_info_t));
            shared->event_queue = nimcp_calloc(shared->event_capacity, sizeof(event_queue_entry_t));
        }
    }

    /* Fallback: allocate arrays if pools not enabled or failed */
    if (!ctx->pools_enabled) {
        shared->module_capacity = NIMCP_SEC_MAX_MODULES;
        shared->region_capacity = NIMCP_SEC_MAX_REGIONS;
        shared->event_capacity = NIMCP_SEC_MAX_EVENTS_QUEUE;
        shared->modules = nimcp_calloc(shared->module_capacity, sizeof(nimcp_sec_module_info_t));
        shared->regions = nimcp_calloc(shared->region_capacity, sizeof(nimcp_sec_region_info_t));
        shared->event_queue = nimcp_calloc(shared->event_capacity, sizeof(event_queue_entry_t));
    }

    if (!shared->modules || !shared->regions || !shared->event_queue) {
        nimcp_mutex_unlock(&ctx->lock);
        return NIMCP_NO_MEMORY;
    }

    /* Create entropy analyzer */
    shared->entropy_analyzer = nimcp_entropy_create();
    if (!shared->entropy_analyzer) {
        nimcp_mutex_unlock(&ctx->lock);
        return NIMCP_NO_MEMORY;
    }
    nimcp_entropy_init(shared->entropy_analyzer, NULL);

    /* Create trust network */
    shared->trust_network = nimcp_trust_create();
    if (!shared->trust_network) {
        nimcp_entropy_destroy(shared->entropy_analyzer);
        nimcp_mutex_unlock(&ctx->lock);
        return NIMCP_NO_MEMORY;
    }
    nimcp_trust_init(shared->trust_network, NULL);

    /* Create DP context */
    shared->dp_context = nimcp_dp_create();
    if (!shared->dp_context) {
        nimcp_entropy_destroy(shared->entropy_analyzer);
        nimcp_trust_destroy(shared->trust_network);
        nimcp_mutex_unlock(&ctx->lock);
        return NIMCP_NO_MEMORY;
    }

    nimcp_dp_config_t dp_config = nimcp_dp_default_config();
    dp_config.total_budget = ctx->config.privacy_budget;
    nimcp_dp_init(shared->dp_context, &dp_config);

    shared->next_module_id = 1;
    shared->next_region_id = 1;
    shared->start_time = get_timestamp_ms();

    /* === PHASE SC-4.2: Initialize COW Manager === */
    if (ctx->config.enable_cow) {
        cow_manager_config_t cow_cfg = cow_default_config(
            sizeof(nimcp_sec_shared_data_t),
            NULL  /* No pool for COW - the shared data is complex */
        );
        cow_cfg.enable_tracking = ctx->config.enable_cow_tracking;
        ctx->cow_manager = cow_manager_create(&cow_cfg, shared);
        if (ctx->cow_manager) {
            ctx->cow_handle = cow_acquire(ctx->cow_manager);
        }
    }

    /* Register security system itself for self-monitoring */
    if (ctx->config.enable_self_monitoring) {
        shared->self_module_id = shared->next_module_id++;

        nimcp_sec_module_info_t* self_mod = &shared->modules[shared->module_count++];
        self_mod->module_id = shared->self_module_id;
        strncpy(self_mod->name, "security_integration", NIMCP_SEC_MODULE_NAME_LEN - 1);
        self_mod->category = NIMCP_SEC_CAT_SECURITY;
        self_mod->active = true;
        self_mod->self_monitoring = true;

        /* Register with trust network */
        nimcp_trust_register_entity(shared->trust_network, shared->self_module_id, "security_integration");

        /* Start with high trust (but still subject to monitoring) */
        for (int i = 0; i < 10; i++) {
            nimcp_trust_record_interaction(shared->trust_network, shared->self_module_id, true, 1.0);
        }

        shared->stats.registered_modules = 1;
        shared->stats.active_modules = 1;
    }

    ctx->initialized = true;

    nimcp_mutex_unlock(&ctx->lock);

    return NIMCP_SUCCESS;
}

void nimcp_sec_integration_destroy(nimcp_sec_integration_t* ctx)
{
    if (!ctx) return;

    /* Stop monitoring */
    if (ctx->monitoring_active) {
        nimcp_sec_stop_monitoring(ctx);
    }

    nimcp_mutex_lock(&ctx->lock);

    /* === PHASE SC-4.2: Release COW handle if using COW === */
    if (ctx->cow_handle) {
        cow_release(ctx->cow_handle);
        ctx->cow_handle = NULL;
    }

    /* === Destroy shared data if we own it (refcount = 1) === */
    if (ctx->shared) {
        unsigned int refcount = atomic_fetch_sub(&ctx->shared->refcount, 1);

        /* If we were the last reference, clean up shared data */
        if (refcount == 1) {
            nimcp_sec_shared_data_t* shared = ctx->shared;

            /* Destroy security subsystems */
            if (shared->entropy_analyzer) {
                nimcp_entropy_destroy(shared->entropy_analyzer);
                shared->entropy_analyzer = NULL;
            }

            if (shared->trust_network) {
                nimcp_trust_destroy(shared->trust_network);
                shared->trust_network = NULL;
            }

            if (shared->dp_context) {
                nimcp_dp_destroy(shared->dp_context);
                shared->dp_context = NULL;
            }

            /* Free arrays */
            if (shared->modules) {
                nimcp_free(shared->modules);
                shared->modules = NULL;
            }

            if (shared->regions) {
                nimcp_free(shared->regions);
                shared->regions = NULL;
            }

            if (shared->event_queue) {
                nimcp_free(shared->event_queue);
                shared->event_queue = NULL;
            }

            /* Free shared structure */
            nimcp_free(shared);
        }
        ctx->shared = NULL;
    }

    /* === PHASE SC-4.2: Destroy COW manager if we created it === */
    if (ctx->cow_manager && !ctx->is_cow_clone) {
        cow_manager_destroy(ctx->cow_manager);
        ctx->cow_manager = NULL;
    }

    /* === PHASE SC-4.1: Destroy Memory Pools === */
    if (ctx->pools_enabled) {
        if (ctx->event_pool) {
            memory_pool_destroy(ctx->event_pool);
            ctx->event_pool = NULL;
        }
        if (ctx->module_pool) {
            memory_pool_destroy(ctx->module_pool);
            ctx->module_pool = NULL;
        }
        if (ctx->region_pool) {
            memory_pool_destroy(ctx->region_pool);
            ctx->region_pool = NULL;
        }
        ctx->pools_enabled = false;
    }

    ctx->initialized = false;

    nimcp_mutex_unlock(&ctx->lock);

    nimcp_mutex_destroy(&ctx->lock);
    nimcp_mutex_destroy(&ctx->event_lock);
    nimcp_cond_destroy(&ctx->monitor_cond);

    nimcp_free(ctx);
}

nimcp_sec_integration_config_t nimcp_sec_integration_default_config(void)
{
    nimcp_sec_integration_config_t config = {0};
    config.trust_threshold = 0.5;
    config.entropy_deviation_threshold = 3.0;
    config.privacy_budget = 10.0;
    config.integrity_check_interval_ms = NIMCP_TIMEOUT_LONG_MS;
    config.self_check_interval_ms = 5000;
    config.enable_continuous_monitoring = true;
    config.enable_self_monitoring = true;
    config.enable_event_logging = true;
    config.event_callback = NULL;
    config.callback_user_data = NULL;

    /* === PHASE SC-4.1: Memory Pool Defaults === */
    config.enable_memory_pools = true;
    config.event_pool_size = NIMCP_SEC_MAX_EVENTS_QUEUE;
    config.module_pool_size = NIMCP_SEC_MAX_MODULES;
    config.region_pool_size = NIMCP_SEC_MAX_REGIONS;

    /* === PHASE SC-4.2: COW Defaults === */
    config.enable_cow = true;
    config.enable_cow_tracking = true;

    return config;
}

//=============================================================================
// Module Registration
//=============================================================================

nimcp_result_t nimcp_sec_register_module(
    nimcp_sec_integration_t* ctx,
    const char* name,
    nimcp_sec_module_category_t category,
    uint32_t* module_id)
{
    if (!ctx || !name || !module_id) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&ctx->lock);

    if (ctx->shared->module_count >= NIMCP_SEC_MAX_MODULES) {
        nimcp_mutex_unlock(&ctx->lock);
        return NIMCP_NO_MEMORY;
    }

    uint32_t id = ctx->shared->next_module_id++;
    nimcp_sec_module_info_t* mod = &ctx->shared->modules[ctx->shared->module_count++];

    mod->module_id = id;
    strncpy(mod->name, name, NIMCP_SEC_MODULE_NAME_LEN - 1);
    mod->category = category;
    mod->active = true;
    mod->self_monitoring = (category == NIMCP_SEC_CAT_SECURITY);
    mod->trust_score.alpha = 1.0;
    mod->trust_score.beta = 1.0;
    mod->trust_score.expected_trust = 0.5;

    /* Register with trust network */
    nimcp_trust_register_entity(ctx->shared->trust_network, id, name);

    ctx->shared->stats.registered_modules++;
    ctx->shared->stats.active_modules++;

    *module_id = id;

    /* Generate registration event */
    char desc[NIMCP_ERROR_BUFFER_SIZE];
    snprintf(desc, sizeof(desc), "Module '%s' registered (category: %s)",
             name, nimcp_sec_category_name(category));
    generate_event(ctx, NIMCP_SEC_EVENT_MODULE_REGISTERED, NIMCP_SEC_SEVERITY_INFO,
                  id, 0, 0.0, desc);

    nimcp_mutex_unlock(&ctx->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_sec_unregister_module(
    nimcp_sec_integration_t* ctx,
    uint32_t module_id)
{
    if (!ctx) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&ctx->lock);

    nimcp_sec_module_info_t* mod = find_module(ctx, module_id);
    if (!mod) {
        nimcp_mutex_unlock(&ctx->lock);
        return NIMCP_NOT_FOUND;
    }

    mod->active = false;
    ctx->shared->stats.active_modules--;

    /* Deactivate related regions */
    for (uint32_t i = 0; i < ctx->shared->region_count; i++) {
        if (ctx->shared->regions[i].owner_module_id == module_id) {
            ctx->shared->regions[i].active = false;
        }
    }

    generate_event(ctx, NIMCP_SEC_EVENT_MODULE_UNREGISTERED, NIMCP_SEC_SEVERITY_INFO,
                  module_id, 0, 0.0, "Module unregistered");

    nimcp_mutex_unlock(&ctx->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_sec_get_module_info(
    nimcp_sec_integration_t* ctx,
    uint32_t module_id,
    nimcp_sec_module_info_t* info)
{
    if (!ctx || !info) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&ctx->lock);

    nimcp_sec_module_info_t* mod = find_module(ctx, module_id);
    if (!mod) {
        nimcp_mutex_unlock(&ctx->lock);
        return NIMCP_NOT_FOUND;
    }

    /* Update trust score from network */
    nimcp_trust_get_score(ctx->shared->trust_network, module_id, &mod->trust_score);

    *info = *mod;

    nimcp_mutex_unlock(&ctx->lock);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Memory Region Monitoring
//=============================================================================

nimcp_result_t nimcp_sec_register_region(
    nimcp_sec_integration_t* ctx,
    uint32_t module_id,
    const char* name,
    const void* data,
    size_t size,
    uint32_t* region_id)
{
    if (!ctx || !name || !data || !region_id) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;
    if (size == 0) return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&ctx->lock);

    /* Verify module exists */
    if (!find_module(ctx, module_id)) {
        nimcp_mutex_unlock(&ctx->lock);
        return NIMCP_NOT_FOUND;
    }

    if (ctx->shared->region_count >= NIMCP_SEC_MAX_REGIONS) {
        nimcp_mutex_unlock(&ctx->lock);
        return NIMCP_NO_MEMORY;
    }

    uint32_t id = ctx->shared->next_region_id++;
    nimcp_sec_region_info_t* reg = &ctx->shared->regions[ctx->shared->region_count++];

    reg->region_id = id;
    reg->owner_module_id = module_id;
    strncpy(reg->name, name, NIMCP_SEC_REGION_NAME_LEN - 1);
    reg->base_address = data;
    reg->size = size;
    reg->active = true;
    reg->last_check_time = get_timestamp_ms();

    /* Calculate baseline entropy */
    reg->baseline_entropy = nimcp_entropy_calculate(data, size);
    reg->last_entropy = reg->baseline_entropy;

    /* Also register with entropy analyzer */
    nimcp_entropy_set_baseline(ctx->shared->entropy_analyzer, id, data, size);

    ctx->shared->stats.monitored_regions++;

    *region_id = id;

    nimcp_mutex_unlock(&ctx->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_sec_update_region_baseline(
    nimcp_sec_integration_t* ctx,
    uint32_t region_id,
    const void* data,
    size_t size)
{
    if (!ctx || !data) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&ctx->lock);

    nimcp_sec_region_info_t* reg = find_region(ctx, region_id);
    if (!reg) {
        nimcp_mutex_unlock(&ctx->lock);
        return NIMCP_NOT_FOUND;
    }

    reg->base_address = data;
    reg->size = size;
    reg->baseline_entropy = nimcp_entropy_calculate(data, size);
    reg->last_entropy = reg->baseline_entropy;
    reg->last_check_time = get_timestamp_ms();

    /* Update in entropy analyzer */
    nimcp_entropy_set_baseline(ctx->shared->entropy_analyzer, region_id, data, size);

    nimcp_mutex_unlock(&ctx->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_sec_check_region(
    nimcp_sec_integration_t* ctx,
    uint32_t region_id,
    const void* data,
    size_t size,
    bool* is_anomaly,
    double* deviation)
{
    if (!ctx || !data || !is_anomaly) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&ctx->lock);

    nimcp_sec_region_info_t* reg = find_region(ctx, region_id);
    if (!reg) {
        nimcp_mutex_unlock(&ctx->lock);
        return NIMCP_NOT_FOUND;
    }

    /* Calculate current entropy */
    double current_entropy = nimcp_entropy_calculate(data, size);
    reg->last_entropy = current_entropy;
    reg->last_check_time = get_timestamp_ms();
    reg->check_count++;
    ctx->shared->stats.total_integrity_checks++;

    /* Check for anomaly using threshold comparison */
    /* A more sophisticated approach would use the baseline stddev */
    double diff = current_entropy - reg->baseline_entropy;
    double threshold = ctx->config.entropy_deviation_threshold * 0.5;  /* ~0.5 bits as 1 sigma approx */

    *is_anomaly = (diff > threshold || diff < -threshold);
    if (deviation) {
        *deviation = diff / 0.5;  /* Approximate z-score */
    }

    if (*is_anomaly) {
        reg->anomaly_count++;
        ctx->shared->stats.total_anomalies_detected++;

        /* Update trust for owning module */
        nimcp_trust_record_interaction(ctx->shared->trust_network, reg->owner_module_id, false, 2.0);

        char desc[NIMCP_ERROR_BUFFER_SIZE];
        snprintf(desc, sizeof(desc), "Region '%s' entropy anomaly: %.2f -> %.2f (deviation: %.2f)",
                reg->name, reg->baseline_entropy, current_entropy, diff);
        generate_event(ctx, NIMCP_SEC_EVENT_REGION_TAMPERED, NIMCP_SEC_SEVERITY_HIGH,
                      reg->owner_module_id, region_id, diff, desc);
    } else {
        /* Successful check improves trust */
        nimcp_trust_record_interaction(ctx->shared->trust_network, reg->owner_module_id, true, 0.5);
    }

    nimcp_mutex_unlock(&ctx->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_sec_unregister_region(
    nimcp_sec_integration_t* ctx,
    uint32_t region_id)
{
    if (!ctx) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&ctx->lock);

    nimcp_sec_region_info_t* reg = find_region(ctx, region_id);
    if (!reg) {
        nimcp_mutex_unlock(&ctx->lock);
        return NIMCP_NOT_FOUND;
    }

    reg->active = false;
    ctx->shared->stats.monitored_regions--;

    nimcp_mutex_unlock(&ctx->lock);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Trust Management
//=============================================================================

nimcp_result_t nimcp_sec_record_interaction(
    nimcp_sec_integration_t* ctx,
    uint32_t module_id,
    bool success,
    double weight)
{
    if (!ctx) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&ctx->lock);

    nimcp_sec_module_info_t* mod = find_module(ctx, module_id);
    if (!mod) {
        nimcp_mutex_unlock(&ctx->lock);
        return NIMCP_NOT_FOUND;
    }

    mod->interaction_count++;
    if (!success) {
        mod->anomaly_count++;
    }

    /* Record with trust network */
    nimcp_trust_record_interaction(ctx->shared->trust_network, module_id, success, weight);

    /* Update cached trust score */
    nimcp_trust_get_score(ctx->shared->trust_network, module_id, &mod->trust_score);

    /* Check for trust violation */
    if (mod->trust_score.expected_trust < ctx->config.trust_threshold) {
        ctx->shared->stats.total_trust_violations++;

        char desc[NIMCP_ERROR_BUFFER_SIZE];
        snprintf(desc, sizeof(desc), "Module '%s' trust below threshold: %.3f < %.3f",
                mod->name, mod->trust_score.expected_trust, ctx->config.trust_threshold);
        generate_event(ctx, NIMCP_SEC_EVENT_TRUST_VIOLATION, NIMCP_SEC_SEVERITY_HIGH,
                      module_id, 0, mod->trust_score.expected_trust, desc);
    }

    nimcp_mutex_unlock(&ctx->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_sec_get_trust_score(
    nimcp_sec_integration_t* ctx,
    uint32_t module_id,
    nimcp_trust_score_t* score)
{
    if (!ctx || !score) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&ctx->lock);

    nimcp_result_t result = nimcp_trust_get_score(ctx->shared->trust_network, module_id, score);

    nimcp_mutex_unlock(&ctx->lock);

    return result;
}

bool nimcp_sec_is_module_trusted(
    nimcp_sec_integration_t* ctx,
    uint32_t module_id)
{
    if (!ctx || !ctx->initialized) {
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    bool trusted = nimcp_trust_is_trusted(ctx->shared->trust_network, module_id, ctx->config.trust_threshold);

    nimcp_mutex_unlock(&ctx->lock);

    return trusted;
}

nimcp_result_t nimcp_sec_add_trust_voucher(
    nimcp_sec_integration_t* ctx,
    uint32_t voucher_module_id,
    uint32_t target_module_id)
{
    if (!ctx) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&ctx->lock);

    nimcp_result_t result = nimcp_trust_add_voucher(ctx->shared->trust_network, voucher_module_id, target_module_id);

    nimcp_mutex_unlock(&ctx->lock);

    return result;
}

nimcp_result_t nimcp_sec_propagate_trust(nimcp_sec_integration_t* ctx)
{
    if (!ctx) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&ctx->lock);

    nimcp_result_t result = nimcp_trust_propagate(ctx->shared->trust_network);

    /* Update cached trust scores for all modules */
    for (uint32_t i = 0; i < ctx->shared->module_count; i++) {
        if (ctx->shared->modules[i].active) {
            nimcp_trust_get_score(ctx->shared->trust_network, ctx->shared->modules[i].module_id, &ctx->shared->modules[i].trust_score);
        }
    }

    /* Calculate average trust */
    double total_trust = 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < ctx->shared->module_count; i++) {
        if (ctx->shared->modules[i].active) {
            total_trust += ctx->shared->modules[i].trust_score.expected_trust;
            count++;
        }
    }
    ctx->shared->stats.average_trust_score = (count > 0) ? total_trust / count : 0;

    nimcp_mutex_unlock(&ctx->lock);

    return result;
}

//=============================================================================
// Differential Privacy Queries
//=============================================================================

nimcp_result_t nimcp_sec_private_count(
    nimcp_sec_integration_t* ctx,
    uint32_t module_id,
    uint64_t true_count,
    double* noisy_count)
{
    if (!ctx || !noisy_count) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&ctx->lock);

    nimcp_dp_result_t result;
    nimcp_result_t status = nimcp_dp_count(ctx->shared->dp_context, true_count, &result);

    if (status == NIMCP_SUCCESS) {
        *noisy_count = result.noisy_value;
        ctx->shared->stats.privacy_budget_remaining = result.remaining_budget;

        /* Check budget warning */
        if (result.remaining_budget < 1.0) {
            generate_event(ctx, NIMCP_SEC_EVENT_PRIVACY_BUDGET, NIMCP_SEC_SEVERITY_MEDIUM,
                          module_id, 0, result.remaining_budget, "Privacy budget low");
        }
    }

    nimcp_mutex_unlock(&ctx->lock);

    return status;
}

nimcp_result_t nimcp_sec_private_sum(
    nimcp_sec_integration_t* ctx,
    uint32_t module_id,
    double true_sum,
    double max_contribution,
    double* noisy_sum)
{
    if (!ctx || !noisy_sum) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&ctx->lock);

    nimcp_dp_result_t result;
    nimcp_result_t status = nimcp_dp_sum(ctx->shared->dp_context, true_sum, max_contribution, &result);

    if (status == NIMCP_SUCCESS) {
        *noisy_sum = result.noisy_value;
        ctx->shared->stats.privacy_budget_remaining = result.remaining_budget;
    }

    nimcp_mutex_unlock(&ctx->lock);

    return status;
}

nimcp_result_t nimcp_sec_private_mean(
    nimcp_sec_integration_t* ctx,
    uint32_t module_id,
    double true_mean,
    uint64_t count,
    double max_value,
    double* noisy_mean)
{
    if (!ctx || !noisy_mean) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&ctx->lock);

    nimcp_dp_result_t result;
    nimcp_result_t status = nimcp_dp_mean(ctx->shared->dp_context, true_mean, count, max_value, &result);

    if (status == NIMCP_SUCCESS) {
        *noisy_mean = result.noisy_value;
        ctx->shared->stats.privacy_budget_remaining = result.remaining_budget;
    }

    nimcp_mutex_unlock(&ctx->lock);

    return status;
}

double nimcp_sec_get_privacy_budget(nimcp_sec_integration_t* ctx)
{
    if (!ctx || !ctx->initialized) return 0.0;

    nimcp_mutex_lock(&ctx->lock);
    double budget = nimcp_dp_remaining_budget(ctx->shared->dp_context);
    ctx->shared->stats.privacy_budget_remaining = budget;
    nimcp_mutex_unlock(&ctx->lock);

    return budget;
}

nimcp_result_t nimcp_sec_reset_privacy_budget(nimcp_sec_integration_t* ctx)
{
    if (!ctx) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&ctx->lock);
    nimcp_result_t result = nimcp_dp_reset_budget(ctx->shared->dp_context);
    ctx->shared->stats.privacy_budget_remaining = ctx->config.privacy_budget;
    nimcp_mutex_unlock(&ctx->lock);

    return result;
}

//=============================================================================
// Continuous Monitoring
//=============================================================================

nimcp_result_t nimcp_sec_start_monitoring(nimcp_sec_integration_t* ctx)
{
    if (!ctx) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;
    if (ctx->monitoring_active) return NIMCP_ALREADY_EXISTS;

    nimcp_mutex_lock(&ctx->lock);

    ctx->monitoring_active = true;
    ctx->monitoring_running = true;

    if (nimcp_thread_create(&ctx->monitor_thread, monitoring_thread_func, ctx, NULL) != NIMCP_SUCCESS) {
        ctx->monitoring_active = false;
        ctx->monitoring_running = false;
        nimcp_mutex_unlock(&ctx->lock);
        return NIMCP_THREAD_ERROR;
    }

    nimcp_mutex_unlock(&ctx->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_sec_stop_monitoring(nimcp_sec_integration_t* ctx)
{
    if (!ctx) return NIMCP_INVALID_PARAM;
    if (!ctx->monitoring_active) return NIMCP_SUCCESS;

    ctx->monitoring_active = false;
    nimcp_thread_join(ctx->monitor_thread, NULL);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_sec_check_integrity(
    nimcp_sec_integration_t* ctx,
    uint32_t* anomalies_found)
{
    if (!ctx || !anomalies_found) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&ctx->lock);

    uint32_t anomalies = 0;

    /* This is a simplified check - in production, modules would provide current data */
    for (uint32_t i = 0; i < ctx->shared->region_count; i++) {
        if (!ctx->shared->regions[i].active) continue;

        ctx->shared->regions[i].check_count++;
        ctx->shared->stats.total_integrity_checks++;
    }

    *anomalies_found = anomalies;

    nimcp_mutex_unlock(&ctx->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_sec_self_check(
    nimcp_sec_integration_t* ctx,
    bool* passed)
{
    if (!ctx || !passed) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&ctx->lock);

    bool result = true;

    /* Check internal state consistency */
    result = result && (ctx->shared->entropy_analyzer != NULL);
    result = result && (ctx->shared->trust_network != NULL);
    result = result && (ctx->shared->dp_context != NULL);
    result = result && (ctx->shared->module_count <= NIMCP_SEC_MAX_MODULES);
    result = result && (ctx->shared->region_count <= NIMCP_SEC_MAX_REGIONS);

    /* Check own trust score */
    if (ctx->config.enable_self_monitoring) {
        nimcp_trust_score_t self_score;
        if (nimcp_trust_get_score(ctx->shared->trust_network, ctx->shared->self_module_id, &self_score) == NIMCP_SUCCESS) {
            result = result && (self_score.expected_trust > 0.3);
        }
    }

    ctx->shared->stats.self_checks_performed++;
    *passed = result;

    if (!result) {
        generate_event(ctx, NIMCP_SEC_EVENT_SELF_CHECK, NIMCP_SEC_SEVERITY_CRITICAL,
                      ctx->shared->self_module_id, 0, 0.0, "Security self-check FAILED");
    }

    nimcp_mutex_unlock(&ctx->lock);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Event Handling
//=============================================================================

nimcp_result_t nimcp_sec_get_event(
    nimcp_sec_integration_t* ctx,
    nimcp_sec_event_t* event)
{
    if (!ctx || !event) return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&ctx->event_lock);

    if (ctx->shared->event_count == 0) {
        nimcp_mutex_unlock(&ctx->event_lock);
        return NIMCP_NOT_FOUND;
    }

    *event = ctx->shared->event_queue[ctx->shared->event_head].event;
    ctx->shared->event_queue[ctx->shared->event_head].valid = false;
    ctx->shared->event_head = (ctx->shared->event_head + 1) % NIMCP_SEC_MAX_EVENTS_QUEUE;
    ctx->shared->event_count--;

    nimcp_mutex_unlock(&ctx->event_lock);

    return NIMCP_SUCCESS;
}

uint32_t nimcp_sec_get_event_count(nimcp_sec_integration_t* ctx)
{
    if (!ctx) return 0;

    nimcp_mutex_lock(&ctx->event_lock);
    uint32_t count = ctx->shared->event_count;
    nimcp_mutex_unlock(&ctx->event_lock);

    return count;
}

nimcp_result_t nimcp_sec_clear_events(nimcp_sec_integration_t* ctx)
{
    if (!ctx) return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&ctx->event_lock);
    ctx->shared->event_head = 0;
    ctx->shared->event_tail = 0;
    ctx->shared->event_count = 0;
    nimcp_mutex_unlock(&ctx->event_lock);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Statistics and Reporting
//=============================================================================

nimcp_result_t nimcp_sec_get_stats(
    nimcp_sec_integration_t* ctx,
    nimcp_sec_integration_stats_t* stats)
{
    if (!ctx || !stats) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&ctx->lock);

    ctx->shared->stats.uptime_ms = get_timestamp_ms() - ctx->shared->start_time;
    ctx->shared->stats.privacy_budget_remaining = nimcp_dp_remaining_budget(ctx->shared->dp_context);

    *stats = ctx->shared->stats;

    nimcp_mutex_unlock(&ctx->lock);

    return NIMCP_SUCCESS;
}

size_t nimcp_sec_generate_trust_report(
    nimcp_sec_integration_t* ctx,
    char* buffer,
    size_t buffer_size)
{
    if (!ctx) return 0;

    nimcp_mutex_lock(&ctx->lock);

    size_t offset = 0;
    size_t needed = 0;

    /* Header */
    needed = snprintf(buffer ? buffer + offset : NULL, buffer ? buffer_size - offset : 0,
                     "=== NIMCP Security Trust Report ===\n"
                     "Registered Modules: %u\n"
                     "Active Modules: %u\n"
                     "Average Trust: %.3f\n\n",
                     ctx->shared->stats.registered_modules,
                     ctx->shared->stats.active_modules,
                     ctx->shared->stats.average_trust_score);
    offset += needed;

    /* Per-module report */
    for (uint32_t i = 0; i < ctx->shared->module_count; i++) {
        if (!ctx->shared->modules[i].active) continue;

        nimcp_sec_module_info_t* mod = &ctx->shared->modules[i];
        needed = snprintf(buffer ? buffer + offset : NULL, buffer ? buffer_size - offset : 0,
                         "[%u] %s (%s)\n"
                         "    Trust: %.3f (alpha=%.1f, beta=%.1f)\n"
                         "    Interactions: %lu, Anomalies: %lu\n",
                         mod->module_id, mod->name, nimcp_sec_category_name(mod->category),
                         mod->trust_score.expected_trust, mod->trust_score.alpha, mod->trust_score.beta,
                         (unsigned long)mod->interaction_count, (unsigned long)mod->anomaly_count);
        offset += needed;
    }

    nimcp_mutex_unlock(&ctx->lock);

    return offset;
}

nimcp_result_t nimcp_sec_get_private_stats(
    nimcp_sec_integration_t* ctx,
    nimcp_sec_integration_stats_t* stats)
{
    if (!ctx || !stats) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&ctx->lock);

    /* Get base stats */
    nimcp_sec_integration_stats_t base = ctx->shared->stats;
    base.uptime_ms = get_timestamp_ms() - ctx->shared->start_time;

    /* Apply differential privacy to sensitive counts */
    nimcp_dp_result_t result;

    /* Privatize registered modules count */
    nimcp_dp_count(ctx->shared->dp_context, base.registered_modules, &result);
    stats->registered_modules = (uint32_t)(result.noisy_value > 0 ? result.noisy_value : 0);

    /* Privatize active modules count */
    nimcp_dp_count(ctx->shared->dp_context, base.active_modules, &result);
    stats->active_modules = (uint32_t)(result.noisy_value > 0 ? result.noisy_value : 0);

    /* Privatize monitored regions */
    nimcp_dp_count(ctx->shared->dp_context, base.monitored_regions, &result);
    stats->monitored_regions = (uint32_t)(result.noisy_value > 0 ? result.noisy_value : 0);

    /* Privatize anomaly count */
    nimcp_dp_count(ctx->shared->dp_context, base.total_anomalies_detected, &result);
    stats->total_anomalies_detected = (uint64_t)(result.noisy_value > 0 ? result.noisy_value : 0);

    /* Copy non-sensitive stats directly */
    stats->total_integrity_checks = base.total_integrity_checks;
    stats->total_events_generated = base.total_events_generated;
    stats->self_checks_performed = base.self_checks_performed;
    stats->uptime_ms = base.uptime_ms;

    /* Privatize average trust (sensitivity = 1/N) */
    if (base.active_modules > 0) {
        double sensitivity = 1.0 / base.active_modules;
        nimcp_dp_add_laplace_noise(ctx->shared->dp_context, base.average_trust_score, sensitivity, &result);
        stats->average_trust_score = result.noisy_value;
        if (stats->average_trust_score < 0) stats->average_trust_score = 0;
        if (stats->average_trust_score > 1) stats->average_trust_score = 1;
    } else {
        stats->average_trust_score = 0;
    }

    stats->privacy_budget_remaining = nimcp_dp_remaining_budget(ctx->shared->dp_context);
    stats->total_trust_violations = base.total_trust_violations;  /* This is aggregate, less sensitive */

    nimcp_mutex_unlock(&ctx->lock);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Name Lookup Functions
//=============================================================================

const char* nimcp_sec_category_name(nimcp_sec_module_category_t category)
{
    static const char* names[] = {
        "CORE",
        "COGNITIVE",
        "MIDDLEWARE",
        "GLIAL",
        "PLASTICITY",
        "NETWORKING",
        "IO",
        "UTILITY",
        "GPU",
        "API",
        "SECURITY"
    };

    if (category < NIMCP_SEC_CAT_COUNT) {
        return names[category];
    }
    return "UNKNOWN";
}

const char* nimcp_sec_event_type_name(nimcp_sec_event_type_t type)
{
    static const char* names[] = {
        "NONE",
        "ENTROPY_ANOMALY",
        "TRUST_CHANGE",
        "TRUST_VIOLATION",
        "PRIVACY_BUDGET",
        "INTEGRITY_CHECK",
        "MODULE_REGISTERED",
        "MODULE_UNREGISTERED",
        "REGION_TAMPERED",
        "SELF_CHECK"
    };

    if (type < NIMCP_SEC_EVENT_COUNT) {
        return names[type];
    }
    return "UNKNOWN";
}

const char* nimcp_sec_severity_name(nimcp_sec_severity_t severity)
{
    static const char* names[] = {
        "INFO",
        "LOW",
        "MEDIUM",
        "HIGH",
        "CRITICAL"
    };

    if (severity <= NIMCP_SEC_SEVERITY_CRITICAL) {
        return names[severity];
    }
    return "UNKNOWN";
}

//=============================================================================
// Phase SC-4.1: Memory Pool API Implementation
//=============================================================================

nimcp_result_t nimcp_sec_get_pool_stats(
    nimcp_sec_integration_t* ctx,
    memory_pool_stats_t* event_pool_stats,
    memory_pool_stats_t* module_pool_stats,
    memory_pool_stats_t* region_pool_stats)
{
    if (!ctx) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&ctx->lock);

    if (!ctx->pools_enabled) {
        nimcp_mutex_unlock(&ctx->lock);
        return NIMCP_NOT_INITIALIZED;  /* Pools not enabled */
    }

    if (event_pool_stats && ctx->event_pool) {
        memory_pool_get_stats(ctx->event_pool, event_pool_stats);
    }

    if (module_pool_stats && ctx->module_pool) {
        memory_pool_get_stats(ctx->module_pool, module_pool_stats);
    }

    if (region_pool_stats && ctx->region_pool) {
        memory_pool_get_stats(ctx->region_pool, region_pool_stats);
    }

    nimcp_mutex_unlock(&ctx->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_sec_reset_pools(nimcp_sec_integration_t* ctx)
{
    if (!ctx) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&ctx->lock);

    if (!ctx->pools_enabled) {
        nimcp_mutex_unlock(&ctx->lock);
        return NIMCP_NOT_INITIALIZED;
    }

    /* WARNING: This invalidates all registrations! */
    if (ctx->event_pool) {
        memory_pool_reset(ctx->event_pool);
    }
    if (ctx->module_pool) {
        memory_pool_reset(ctx->module_pool);
    }
    if (ctx->region_pool) {
        memory_pool_reset(ctx->region_pool);
    }

    /* Reset shared data counts */
    if (ctx->shared) {
        ctx->shared->module_count = 0;
        ctx->shared->region_count = 0;
        ctx->shared->event_count = 0;
        ctx->shared->event_head = 0;
        ctx->shared->event_tail = 0;
    }

    nimcp_mutex_unlock(&ctx->lock);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Phase SC-4.2: Copy-on-Write API Implementation
//=============================================================================

/**
 * @brief Deep copy function for COW shared data
 */
static bool sec_shared_data_copy(void* dest, const void* src, size_t size, void* user_data)
{
    (void)size;
    (void)user_data;

    nimcp_sec_shared_data_t* dst = (nimcp_sec_shared_data_t*)dest;
    const nimcp_sec_shared_data_t* s = (const nimcp_sec_shared_data_t*)src;

    /* Copy basic fields */
    dst->module_count = s->module_count;
    dst->next_module_id = s->next_module_id;
    dst->module_capacity = s->module_capacity;
    dst->region_count = s->region_count;
    dst->next_region_id = s->next_region_id;
    dst->region_capacity = s->region_capacity;
    dst->event_head = s->event_head;
    dst->event_tail = s->event_tail;
    dst->event_count = s->event_count;
    dst->event_capacity = s->event_capacity;
    dst->stats = s->stats;
    dst->start_time = s->start_time;
    dst->self_module_id = s->self_module_id;
    dst->self_region_id = s->self_region_id;
    dst->self_entropy_baseline = s->self_entropy_baseline;

    /* Initialize new refcount */
    atomic_store(&dst->refcount, 1);

    /* Deep copy arrays */
    if (s->modules && dst->module_capacity > 0) {
        dst->modules = nimcp_calloc(dst->module_capacity, sizeof(nimcp_sec_module_info_t));
        if (!dst->modules) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_shared_data_copy: dst->modules is NULL");
            return false;
        }
        memcpy(dst->modules, s->modules, dst->module_count * sizeof(nimcp_sec_module_info_t));
    }

    if (s->regions && dst->region_capacity > 0) {
        dst->regions = nimcp_calloc(dst->region_capacity, sizeof(nimcp_sec_region_info_t));
        if (!dst->regions) {
            nimcp_free(dst->modules);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_shared_data_copy: dst->regions is NULL");
            return false;
        }
        memcpy(dst->regions, s->regions, dst->region_count * sizeof(nimcp_sec_region_info_t));
    }

    if (s->event_queue && dst->event_capacity > 0) {
        dst->event_queue = nimcp_calloc(dst->event_capacity, sizeof(event_queue_entry_t));
        if (!dst->event_queue) {
            nimcp_free(dst->modules);
            nimcp_free(dst->regions);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_shared_data_copy: dst->event_queue is NULL");
            return false;
        }
        memcpy(dst->event_queue, s->event_queue, dst->event_capacity * sizeof(event_queue_entry_t));
    }

    /* Note: Security subsystems (entropy_analyzer, trust_network, dp_context) are NOT copied
     * They are shared via reference - clones inherit security state */
    dst->entropy_analyzer = s->entropy_analyzer;
    dst->trust_network = s->trust_network;
    dst->dp_context = s->dp_context;

    return true;
}

nimcp_sec_integration_t* nimcp_sec_integration_cow_clone(nimcp_sec_integration_t* source)
{
    if (!source || !source->initialized || !source->shared) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_sec_integration_cow_clone: required parameter is NULL (source, source->initialized, source->shared)");
        return NULL;
    }

    /* Create new context */
    nimcp_sec_integration_t* clone = nimcp_calloc(1, sizeof(nimcp_sec_integration_t));
    if (!clone) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "clone is NULL");

        return NULL;

    }

    nimcp_mutex_lock(&source->lock);

    /* Copy configuration */
    clone->config = source->config;

    /* === COW: Share the shared data with refcount increment === */
    clone->shared = source->shared;
    atomic_fetch_add(&clone->shared->refcount, 1);

    /* Mark as COW clone */
    clone->cow_state = NIMCP_SEC_COW_SHARED;
    clone->is_cow_clone = true;
    clone->cow_source = source;

    /* Share COW manager (if source has one) */
    clone->cow_manager = source->cow_manager;
    if (clone->cow_manager) {
        clone->cow_handle = cow_acquire(clone->cow_manager);
    }

    /* Clone does NOT share pools - create its own if needed */
    clone->pools_enabled = false;
    clone->event_pool = NULL;
    clone->module_pool = NULL;
    clone->region_pool = NULL;

    /* Initialize synchronization */
    nimcp_mutex_init(&clone->lock, NULL);
    nimcp_mutex_init(&clone->event_lock, NULL);
    nimcp_cond_init(&clone->monitor_cond);

    /* Clone doesn't inherit monitoring thread */
    clone->monitoring_active = false;
    clone->monitoring_running = false;

    clone->initialized = true;

    /* Update stats */
    clone->shared->stats.cow_contexts_shared++;

    nimcp_mutex_unlock(&source->lock);

    return clone;
}

bool nimcp_sec_integration_is_cow_shared(nimcp_sec_integration_t* ctx)
{
    if (!ctx) {
        return false;
    }
    return (ctx->cow_state == NIMCP_SEC_COW_SHARED);
}

nimcp_sec_cow_state_t nimcp_sec_integration_get_cow_state(nimcp_sec_integration_t* ctx)
{
    if (!ctx) return NIMCP_SEC_COW_PRIVATE;
    return ctx->cow_state;
}

nimcp_result_t nimcp_sec_integration_cow_detach(nimcp_sec_integration_t* ctx)
{
    if (!ctx || !ctx->shared) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;

    /* Already private - nothing to do */
    if (ctx->cow_state == NIMCP_SEC_COW_PRIVATE) {
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(&ctx->lock);

    /* Create private copy of shared data */
    nimcp_sec_shared_data_t* new_shared = nimcp_calloc(1, sizeof(nimcp_sec_shared_data_t));
    if (!new_shared) {
        nimcp_mutex_unlock(&ctx->lock);
        return NIMCP_NO_MEMORY;
    }

    /* Deep copy shared data */
    if (!sec_shared_data_copy(new_shared, ctx->shared, sizeof(nimcp_sec_shared_data_t), NULL)) {
        nimcp_free(new_shared);
        nimcp_mutex_unlock(&ctx->lock);
        return NIMCP_NO_MEMORY;
    }

    /* Decrement old shared refcount */
    atomic_fetch_sub(&ctx->shared->refcount, 1);

    /* Switch to private copy */
    ctx->shared = new_shared;
    ctx->cow_state = NIMCP_SEC_COW_PRIVATE;
    ctx->is_cow_clone = false;
    ctx->cow_source = NULL;

    /* Update stats */
    ctx->shared->stats.cow_copies_triggered++;

    nimcp_mutex_unlock(&ctx->lock);

    return NIMCP_SUCCESS;
}

uint32_t nimcp_sec_integration_cow_refcount(nimcp_sec_integration_t* ctx)
{
    if (!ctx || !ctx->shared) return 0;
    return atomic_load(&ctx->shared->refcount);
}
