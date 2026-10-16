/**
 * @file nimcp_rcog_tool_router.c
 * @brief Recursive Cognition Tool Router Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "cognitive/recursive/nimcp_rcog_tool_router.h"
#include "cognitive/recursive/nimcp_rcog_context_store.h"
#include "cognitive/recursive/nimcp_rcog_bio_async_bridge.h"
#include "cognitive/recursive/nimcp_rcog_immune_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_time.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(rcog_tool_router, MESH_ADAPTER_CATEGORY_COGNITIVE)

void rcog_tool_router_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_rcog_tool_router_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int rcog_tool_router_training_begin(void* ctx) {
    rcog_tool_router_t* router = (rcog_tool_router_t*)ctx;
    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_tool_router_training_begin: NULL argument");
        return -1;
    }
    rcog_tool_router_heartbeat_instance(
        g_rcog_tool_router_instance_health_agent,
        "rcog_router_training_begin", 0.0f);
    return 0;
}

int rcog_tool_router_training_step(void* ctx, float progress) {
    rcog_tool_router_t* router = (rcog_tool_router_t*)ctx;
    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_tool_router_training_step: NULL argument");
        return -1;
    }
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    rcog_tool_router_heartbeat_instance(
        g_rcog_tool_router_instance_health_agent,
        "rcog_router_training_step", clamped);
    return 0;
}

int rcog_tool_router_training_end(void* ctx) {
    rcog_tool_router_t* router = (rcog_tool_router_t*)ctx;
    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_tool_router_training_end: NULL argument");
        return -1;
    }
    rcog_tool_router_heartbeat_instance(
        g_rcog_tool_router_instance_health_agent,
        "rcog_router_training_end", 1.0f);
    return 0;
}

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Category registration
 */
typedef struct {
    char name[RCOG_ROUTER_MAX_CATEGORY_NAME];
    char description[RCOG_ROUTER_MAX_TOOL_DESC];
    rcog_capability_tier_t default_tier;
    char tools[RCOG_ROUTER_MAX_TOOLS_PER_CATEGORY][RCOG_ROUTER_MAX_TOOL_NAME];
    size_t num_tools;
    bool in_use;
} rcog_category_entry_t;

/**
 * @brief Tool registration entry
 */
typedef struct {
    rcog_tool_def_t def;
    rcog_tool_stats_t stats;
    bool in_use;
    uint64_t registered_ms;
    uint32_t current_invocations;  /**< Active concurrent invocations */
} rcog_tool_entry_t;

/**
 * @brief Async invocation tracking
 */
struct rcog_async_handle {
    uint64_t handle_id;
    char tool_name[RCOG_ROUTER_MAX_TOOL_NAME];
    rcog_tool_callback_t callback;
    void* user_data;
    bool completed;
    bool cancelled;
    rcog_tool_result_t result;
    uint64_t started_ms;
};

/**
 * @brief Tool router internal structure
 */
struct rcog_tool_router {
    /* Configuration */
    rcog_tool_router_config_t config;

    /* Tool registry */
    rcog_tool_entry_t tools[RCOG_ROUTER_MAX_TOOLS];
    size_t num_tools;

    /* Category registry */
    rcog_category_entry_t categories[RCOG_ROUTER_MAX_CATEGORIES];
    size_t num_categories;

    /* Connections */
    struct rcog_context_store* context_store;
    struct rcog_bio_async_bridge* bio_async;
    struct rcog_immune_bridge* immune;

    /* Async handles */
    struct rcog_async_handle* async_handles[RCOG_ROUTER_MAX_CONCURRENT];
    size_t num_async_handles;
    uint64_t next_handle_id;

    /* Statistics */
    rcog_router_stats_t stats;

    /* Rate limiting */
    uint64_t last_rate_check_ms;
    uint32_t calls_this_second;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

static int find_tool_index(const rcog_tool_router_t* router, const char* name);
static int find_category_index(const rcog_tool_router_t* router, const char* name);
static bool check_tier_access(rcog_capability_tier_t caller, rcog_capability_tier_t required);
static bool check_rate_limit(rcog_tool_router_t* router, const rcog_tool_entry_t* tool);
static void update_tool_stats(rcog_tool_entry_t* tool, bool success, uint64_t duration_ms,
                             size_t input_size, size_t output_size);

/* Builtin tool handlers */
static rcog_error_t builtin_memory_read(const void* input, size_t input_size,
                                        void* context, void** output, size_t* output_size);
static rcog_error_t builtin_memory_write(const void* input, size_t input_size,
                                         void* context, void** output, size_t* output_size);
static rcog_error_t builtin_memory_query(const void* input, size_t input_size,
                                         void* context, void** output, size_t* output_size);
static rcog_error_t builtin_feature_extract(const void* input, size_t input_size,
                                            void* context, void** output, size_t* output_size);
static rcog_error_t builtin_pattern_match(const void* input, size_t input_size,
                                          void* context, void** output, size_t* output_size);
static rcog_error_t builtin_output_text(const void* input, size_t input_size,
                                        void* context, void** output, size_t* output_size);

/*=============================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

static int find_tool_index(const rcog_tool_router_t* router, const char* name) {
    if (!router || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_tool_index: required parameter is NULL (router, name)");
        return -1;
    }

    for (size_t i = 0; i < RCOG_ROUTER_MAX_TOOLS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && RCOG_ROUTER_MAX_TOOLS > 256) {
            rcog_tool_router_heartbeat("rcog_tool_ro_loop",
                             (float)(i + 1) / (float)RCOG_ROUTER_MAX_TOOLS);
        }

        if (router->tools[i].in_use &&
            strncmp(router->tools[i].def.name, name, RCOG_ROUTER_MAX_TOOL_NAME) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static int find_category_index(const rcog_tool_router_t* router, const char* name) {
    if (!router || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_category_index: required parameter is NULL (router, name)");
        return -1;
    }

    for (size_t i = 0; i < RCOG_ROUTER_MAX_CATEGORIES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && RCOG_ROUTER_MAX_CATEGORIES > 256) {
            rcog_tool_router_heartbeat("rcog_tool_ro_loop",
                             (float)(i + 1) / (float)RCOG_ROUTER_MAX_CATEGORIES);
        }

        if (router->categories[i].in_use &&
            strncmp(router->categories[i].name, name, RCOG_ROUTER_MAX_CATEGORY_NAME) == 0) {
            return (int)i;
        }
    }
    /* Category not found — normal lookup failure, not an error */
    return -1;
}

static bool check_tier_access(rcog_capability_tier_t caller, rcog_capability_tier_t required) {
    /* Root tier has NO tool access */
    if (caller == RCOG_TIER_ROOT) {
        return false;
    }

    /* Higher numbered tiers can access lower tier tools */
    /* L4 can access L1-L4, L3 can access L1-L3, etc. */
    return caller >= required;
}

static bool check_rate_limit(rcog_tool_router_t* router, const rcog_tool_entry_t* tool) {
    uint64_t now = nimcp_platform_time_monotonic_ms();

    /* Global rate limit */
    if (router->config.access_policy.global_rate_limit > 0) {
        if (now - router->last_rate_check_ms >= 1000) {
            router->last_rate_check_ms = now;
            router->calls_this_second = 0;
        }

        if (router->calls_this_second >= router->config.access_policy.global_rate_limit) {
            return false;
        }
    }

    /* Per-tool rate limit */
    if (tool->def.rate_limit_per_sec > 0) {
        /* Simplified: just check concurrent count as approximation */
        if (tool->current_invocations >= tool->def.rate_limit_per_sec) {
            return false;
        }
    }

    return true;
}

static void update_tool_stats(rcog_tool_entry_t* tool, bool success, uint64_t duration_ms,
                             size_t input_size, size_t output_size) {
    tool->stats.invocations++;

    if (success) {
        tool->stats.successes++;
    } else {
        tool->stats.failures++;
    }

    /* Update average duration */
    float total_duration = tool->stats.avg_duration_ms * (tool->stats.invocations - 1);
    tool->stats.avg_duration_ms = (total_duration + duration_ms) / tool->stats.invocations;

    tool->stats.total_input_bytes += input_size;
    tool->stats.total_output_bytes += output_size;
    tool->stats.last_invoked_ms = nimcp_platform_time_monotonic_ms();
}

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

rcog_tool_router_config_t rcog_tool_router_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_default_config", 0.0f);


    rcog_tool_router_config_t config;
    memset(&config, 0, sizeof(config));

    /* Access policy */
    config.access_policy.allow_cross_tier = false;
    config.access_policy.require_auth_token = false;
    config.access_policy.global_rate_limit = 0;  /* Unlimited */
    config.access_policy.max_concurrent = RCOG_ROUTER_MAX_CONCURRENT;
    config.access_policy.audit_all_calls = false;
    config.access_policy.allow_experimental = false;

    /* Execution */
    config.default_timeout_ms = RCOG_ROUTER_DEFAULT_TOOL_TIMEOUT_MS;
    config.default_max_input_size = 10 * 1024 * 1024;  /* 10 MB */
    config.default_max_output_size = 10 * 1024 * 1024;
    config.enable_async = true;
    config.enable_streaming = false;

    /* Integration */
    config.enable_bio_async = true;
    config.enable_immune_check = true;

    /* Monitoring */
    config.enable_metrics = true;
    config.enable_tracing = false;
    config.verbose_logging = false;

    return config;
}

rcog_tool_router_t* rcog_tool_router_create(
    const rcog_tool_router_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_create", 0.0f);


    rcog_tool_router_t* router = nimcp_calloc(1, sizeof(rcog_tool_router_t));
    if (!router) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate router");

        return NULL;

    }

    /* Apply config */
    if (config) {
        router->config = *config;
    } else {
        router->config = rcog_tool_router_default_config();
    }

    /* Create mutex */
    mutex_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    attr.type = MUTEX_TYPE_NORMAL;
    router->mutex = nimcp_mutex_create(&attr);
    if (!router->mutex) {
        nimcp_free(router);
        router = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rcog_tool_router_create: router->mutex is NULL");
        return NULL;
    }

    router->next_handle_id = 1;

    return router;
}

rcog_tool_router_t* rcog_tool_router_create_default(void) {
    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_create_default", 0.0f);


    return rcog_tool_router_create(NULL);
}

void rcog_tool_router_destroy(rcog_tool_router_t* router) {
    if (!router) return;

    /* Free async handles */
    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_destroy", 0.0f);


    for (size_t i = 0; i < router->num_async_handles; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && router->num_async_handles > 256) {
            rcog_tool_router_heartbeat("rcog_tool_ro_loop",
                             (float)(i + 1) / (float)router->num_async_handles);
        }

        if (router->async_handles[i]) {
            nimcp_free(router->async_handles[i]);
        }
    }

    /* Free tool parameters */
    for (size_t i = 0; i < RCOG_ROUTER_MAX_TOOLS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && RCOG_ROUTER_MAX_TOOLS > 256) {
            rcog_tool_router_heartbeat("rcog_tool_ro_loop",
                             (float)(i + 1) / (float)RCOG_ROUTER_MAX_TOOLS);
        }

        if (router->tools[i].in_use && router->tools[i].def.params) {
            nimcp_free(router->tools[i].def.params);
        }
    }

    if (router->mutex) {
        nimcp_mutex_destroy(router->mutex);
    }

    nimcp_free(router);
    router = NULL;
}

/*=============================================================================
 * CONNECTION
 *===========================================================================*/

int rcog_tool_router_connect_context_store(
    rcog_tool_router_t* router,
    struct rcog_context_store* store
) {
    if (!router) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_connect_context_stor", 0.0f);


    nimcp_mutex_lock(router->mutex);
    router->context_store = store;
    nimcp_mutex_unlock(router->mutex);

    return RCOG_OK;
}

int rcog_tool_router_connect_bio_async(
    rcog_tool_router_t* router,
    struct rcog_bio_async_bridge* bio_async
) {
    if (!router) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_connect_bio_async", 0.0f);


    nimcp_mutex_lock(router->mutex);
    router->bio_async = bio_async;
    nimcp_mutex_unlock(router->mutex);

    return RCOG_OK;
}

int rcog_tool_router_connect_immune(
    rcog_tool_router_t* router,
    struct rcog_immune_bridge* immune
) {
    if (!router) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_connect_immune", 0.0f);


    nimcp_mutex_lock(router->mutex);
    router->immune = immune;
    nimcp_mutex_unlock(router->mutex);

    return RCOG_OK;
}

/*=============================================================================
 * TOOL REGISTRATION
 *===========================================================================*/

int rcog_tool_router_register(
    rcog_tool_router_t* router,
    const rcog_tool_def_t* def
) {
    if (!router || !def) return RCOG_ERROR_NULL_POINTER;
    if (strlen(def->name) == 0) return RCOG_ERROR_INVALID_CONFIG;
    if (!def->handler) return RCOG_ERROR_INVALID_CONFIG;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_register", 0.0f);


    nimcp_mutex_lock(router->mutex);

    /* Check if already registered */
    int existing = find_tool_index(router, def->name);
    if (existing >= 0) {
        nimcp_mutex_unlock(router->mutex);
        return RCOG_ERROR_ALREADY_INITIALIZED;
    }

    /* Find free slot */
    int slot = -1;
    for (size_t i = 0; i < RCOG_ROUTER_MAX_TOOLS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && RCOG_ROUTER_MAX_TOOLS > 256) {
            rcog_tool_router_heartbeat("rcog_tool_ro_loop",
                             (float)(i + 1) / (float)RCOG_ROUTER_MAX_TOOLS);
        }

        if (!router->tools[i].in_use) {
            slot = (int)i;
            break;
        }
    }

    if (slot < 0) {
        nimcp_mutex_unlock(router->mutex);
        return RCOG_ERROR_CONTEXT_FULL;
    }

    /* Register tool */
    rcog_tool_entry_t* entry = &router->tools[slot];
    entry->def = *def;
    entry->in_use = true;
    entry->registered_ms = nimcp_platform_time_monotonic_ms();
    entry->current_invocations = 0;
    memset(&entry->stats, 0, sizeof(entry->stats));
    strncpy(entry->stats.tool_name, def->name, RCOG_ROUTER_MAX_TOOL_NAME - 1);

    /* Apply defaults */
    if (entry->def.timeout_ms == 0) {
        entry->def.timeout_ms = router->config.default_timeout_ms;
    }
    if (entry->def.max_input_size == 0) {
        entry->def.max_input_size = router->config.default_max_input_size;
    }
    if (entry->def.max_output_size == 0) {
        entry->def.max_output_size = router->config.default_max_output_size;
    }

    router->num_tools++;
    router->stats.tools_registered = router->num_tools;

    /* Add to category if specified */
    if (strlen(def->category) > 0) {
        int cat_idx = find_category_index(router, def->category);
        if (cat_idx >= 0) {
            rcog_category_entry_t* cat = &router->categories[cat_idx];
            if (cat->num_tools < RCOG_ROUTER_MAX_TOOLS_PER_CATEGORY) {
                strncpy(cat->tools[cat->num_tools], def->name,
                       RCOG_ROUTER_MAX_TOOL_NAME - 1);
                cat->num_tools++;
            }
        }
    }

    nimcp_mutex_unlock(router->mutex);
    return RCOG_OK;
}

size_t rcog_tool_router_register_batch(
    rcog_tool_router_t* router,
    const rcog_tool_def_t* defs,
    size_t num_tools
) {
    if (!router || !defs) return 0;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_register_batch", 0.0f);


    size_t registered = 0;
    for (size_t i = 0; i < num_tools; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_tools > 256) {
            rcog_tool_router_heartbeat("rcog_tool_ro_loop",
                             (float)(i + 1) / (float)num_tools);
        }

        if (rcog_tool_router_register(router, &defs[i]) == RCOG_OK) {
            registered++;
        }
    }

    return registered;
}

int rcog_tool_router_unregister(
    rcog_tool_router_t* router,
    const char* tool_name
) {
    if (!router || !tool_name) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_unregister", 0.0f);


    nimcp_mutex_lock(router->mutex);

    int idx = find_tool_index(router, tool_name);
    if (idx < 0) {
        nimcp_mutex_unlock(router->mutex);
        return RCOG_ERROR_TOOL_NOT_FOUND;
    }

    rcog_tool_entry_t* entry = &router->tools[idx];

    /* Check if currently in use */
    if (entry->current_invocations > 0) {
        nimcp_mutex_unlock(router->mutex);
        return RCOG_ERROR_INVALID_CONFIG;  /* Cannot unregister while in use */
    }

    /* Remove from category */
    if (strlen(entry->def.category) > 0) {
        int cat_idx = find_category_index(router, entry->def.category);
        if (cat_idx >= 0) {
            rcog_category_entry_t* cat = &router->categories[cat_idx];
            for (size_t i = 0; i < cat->num_tools; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && cat->num_tools > 256) {
                    rcog_tool_router_heartbeat("rcog_tool_ro_loop",
                                     (float)(i + 1) / (float)cat->num_tools);
                }

                if (strncmp(cat->tools[i], tool_name, RCOG_ROUTER_MAX_TOOL_NAME) == 0) {
                    /* Shift remaining */
                    for (size_t j = i; j < cat->num_tools - 1; j++) {
                        strncpy(cat->tools[j], cat->tools[j + 1], RCOG_ROUTER_MAX_TOOL_NAME);
                    }
                    cat->num_tools--;
                    break;
                }
            }
        }
    }

    /* Free params if allocated */
    if (entry->def.params) {
        nimcp_free(entry->def.params);
    }

    memset(entry, 0, sizeof(*entry));
    router->num_tools--;
    router->stats.tools_registered = router->num_tools;

    nimcp_mutex_unlock(router->mutex);
    return RCOG_OK;
}

bool rcog_tool_router_has_tool(
    const rcog_tool_router_t* router,
    const char* tool_name
) {
    if (!router || !tool_name) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_has_tool", 0.0f);


    return find_tool_index(router, tool_name) >= 0;
}

int rcog_tool_router_get_tool(
    const rcog_tool_router_t* router,
    const char* tool_name,
    rcog_tool_def_t* def
) {
    if (!router || !tool_name || !def) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_get_tool", 0.0f);


    int idx = find_tool_index(router, tool_name);
    if (idx < 0) return RCOG_ERROR_TOOL_NOT_FOUND;

    *def = router->tools[idx].def;
    return RCOG_OK;
}

/*=============================================================================
 * CATEGORY MANAGEMENT
 *===========================================================================*/

int rcog_tool_router_register_category(
    rcog_tool_router_t* router,
    const char* category,
    const char* description,
    rcog_capability_tier_t default_tier
) {
    if (!router || !category) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_register_category", 0.0f);


    nimcp_mutex_lock(router->mutex);

    /* Check if exists */
    if (find_category_index(router, category) >= 0) {
        nimcp_mutex_unlock(router->mutex);
        return RCOG_ERROR_ALREADY_INITIALIZED;
    }

    /* Find free slot */
    int slot = -1;
    for (size_t i = 0; i < RCOG_ROUTER_MAX_CATEGORIES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && RCOG_ROUTER_MAX_CATEGORIES > 256) {
            rcog_tool_router_heartbeat("rcog_tool_ro_loop",
                             (float)(i + 1) / (float)RCOG_ROUTER_MAX_CATEGORIES);
        }

        if (!router->categories[i].in_use) {
            slot = (int)i;
            break;
        }
    }

    if (slot < 0) {
        nimcp_mutex_unlock(router->mutex);
        return RCOG_ERROR_CONTEXT_FULL;
    }

    rcog_category_entry_t* entry = &router->categories[slot];
    strncpy(entry->name, category, RCOG_ROUTER_MAX_CATEGORY_NAME - 1);
    if (description) {
        strncpy(entry->description, description, RCOG_ROUTER_MAX_TOOL_DESC - 1);
    }
    entry->default_tier = default_tier;
    entry->num_tools = 0;
    entry->in_use = true;

    router->num_categories++;
    router->stats.categories_registered = router->num_categories;

    nimcp_mutex_unlock(router->mutex);
    return RCOG_OK;
}

int rcog_tool_router_get_category_tools(
    const rcog_tool_router_t* router,
    const char* category,
    char (*tools)[RCOG_ROUTER_MAX_TOOL_NAME],
    size_t max_tools,
    size_t* num_tools
) {
    if (!router || !category || !tools || !num_tools) return RCOG_ERROR_NULL_POINTER;

    *num_tools = 0;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_get_category_tools", 0.0f);


    int idx = find_category_index(router, category);
    if (idx < 0) return RCOG_ERROR_CONTEXT_NOT_FOUND;

    const rcog_category_entry_t* cat = &router->categories[idx];

    for (size_t i = 0; i < cat->num_tools && *num_tools < max_tools; i++) {
        strncpy(tools[*num_tools], cat->tools[i], RCOG_ROUTER_MAX_TOOL_NAME - 1);
        (*num_tools)++;
    }

    return RCOG_OK;
}

int rcog_tool_router_list_categories(
    const rcog_tool_router_t* router,
    char (*categories)[RCOG_ROUTER_MAX_CATEGORY_NAME],
    size_t max_categories,
    size_t* num_categories
) {
    if (!router || !categories || !num_categories) return RCOG_ERROR_NULL_POINTER;

    *num_categories = 0;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_list_categories", 0.0f);


    for (size_t i = 0; i < RCOG_ROUTER_MAX_CATEGORIES && *num_categories < max_categories; i++) {
        if (router->categories[i].in_use) {
            strncpy(categories[*num_categories], router->categories[i].name,
                   RCOG_ROUTER_MAX_CATEGORY_NAME - 1);
            (*num_categories)++;
        }
    }

    return RCOG_OK;
}

/*=============================================================================
 * TOOL INVOCATION
 *===========================================================================*/

int rcog_tool_router_invoke(
    rcog_tool_router_t* router,
    const rcog_tool_request_t* request,
    rcog_tool_result_t* result
) {
    if (!router || !request || !result) return RCOG_ERROR_NULL_POINTER;
    if (!request->tool_name) return RCOG_ERROR_INVALID_CONFIG;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_invoke", 0.0f);


    memset(result, 0, sizeof(*result));
    result->tool_name = request->tool_name;

    nimcp_mutex_lock(router->mutex);

    /* Find tool */
    int idx = find_tool_index(router, request->tool_name);
    if (idx < 0) {
        nimcp_mutex_unlock(router->mutex);
        result->error = RCOG_ERROR_TOOL_NOT_FOUND;
        result->error_message = "Tool not found";
        return RCOG_ERROR_TOOL_NOT_FOUND;
    }

    rcog_tool_entry_t* tool = &router->tools[idx];

    /* Check tier access */
    if (!check_tier_access(request->caller_tier, tool->def.min_tier)) {
        nimcp_mutex_unlock(router->mutex);
        result->error = RCOG_ERROR_TOOL_ACCESS_DENIED;
        result->error_message = "Access denied: insufficient tier";
        router->stats.total_access_denied++;
        tool->stats.access_denied++;
        return RCOG_ERROR_TOOL_ACCESS_DENIED;
    }

    /* Check experimental flag */
    if ((tool->def.flags & RCOG_TOOL_FLAG_EXPERIMENTAL) &&
        !router->config.access_policy.allow_experimental) {
        nimcp_mutex_unlock(router->mutex);
        result->error = RCOG_ERROR_TOOL_ACCESS_DENIED;
        result->error_message = "Experimental tools not allowed";
        return RCOG_ERROR_TOOL_ACCESS_DENIED;
    }

    /* Check rate limit */
    if (!check_rate_limit(router, tool)) {
        nimcp_mutex_unlock(router->mutex);
        result->error = RCOG_ERROR_TIMEOUT;
        result->error_message = "Rate limit exceeded";
        return RCOG_ERROR_TIMEOUT;
    }

    /* Check concurrent limit */
    if (router->stats.current_concurrent >= router->config.access_policy.max_concurrent) {
        nimcp_mutex_unlock(router->mutex);
        result->error = RCOG_ERROR_WORKER_POOL_EXHAUSTED;
        result->error_message = "Max concurrent invocations reached";
        return RCOG_ERROR_WORKER_POOL_EXHAUSTED;
    }

    /* Check input size */
    if (tool->def.max_input_size > 0 && request->input_size > tool->def.max_input_size) {
        nimcp_mutex_unlock(router->mutex);
        result->error = RCOG_ERROR_CONTEXT_TOO_LARGE;
        result->error_message = "Input too large";
        return RCOG_ERROR_CONTEXT_TOO_LARGE;
    }

    /* Track invocation */
    tool->current_invocations++;
    router->stats.current_concurrent++;
    router->stats.total_invocations++;
    router->calls_this_second++;

    if (router->stats.current_concurrent > router->stats.peak_concurrent) {
        router->stats.peak_concurrent = router->stats.current_concurrent;
    }

    nimcp_mutex_unlock(router->mutex);

    /* Execute tool */
    uint64_t start_time = nimcp_platform_time_monotonic_ms();

    void* output = NULL;
    size_t output_size = 0;

    rcog_error_t err = tool->def.handler(
        request->input,
        request->input_size,
        tool->def.context,
        &output,
        &output_size
    );

    uint64_t duration = nimcp_platform_time_monotonic_ms() - start_time;

    /* Update result */
    result->output = output;
    result->output_size = output_size;
    result->output_type = tool->def.output_type;
    result->duration_ms = duration;
    result->error = err;
    result->success = (err == RCOG_OK);

    if (err != RCOG_OK) {
        result->error_message = rcog_error_name(err);
    }

    /* Update stats */
    nimcp_mutex_lock(router->mutex);

    tool->current_invocations--;
    router->stats.current_concurrent--;

    if (result->success) {
        router->stats.total_successes++;
    } else {
        router->stats.total_failures++;
    }

    update_tool_stats(tool, result->success, duration, request->input_size, output_size);

    /* Update router average */
    float total_dur = router->stats.avg_duration_ms * (router->stats.total_invocations - 1);
    router->stats.avg_duration_ms = (total_dur + duration) / router->stats.total_invocations;

    nimcp_mutex_unlock(router->mutex);

    return err;
}

int rcog_tool_router_invoke_async(
    rcog_tool_router_t* router,
    const rcog_tool_request_t* request,
    rcog_tool_callback_t callback,
    rcog_async_handle_t** handle
) {
    if (!router || !request || !callback || !handle) return RCOG_ERROR_NULL_POINTER;

    if (!router->config.enable_async) {
        return RCOG_ERROR_INVALID_CONFIG;
    }

    /* For now, just do sync and call callback */
    /* Full async would require thread pool integration */

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_invoke_async", 0.0f);


    rcog_tool_result_t result;
    int err = rcog_tool_router_invoke(router, request, &result);

    callback(&result, request->user_data);

    *handle = NULL;  /* No handle needed for sync execution */
    return err;
}

int rcog_tool_router_invoke_streaming(
    rcog_tool_router_t* router,
    const rcog_tool_request_t* request,
    rcog_tool_stream_callback_t stream_callback,
    void* user_data
) {
    if (!router || !request || !stream_callback) return RCOG_ERROR_NULL_POINTER;

    if (!router->config.enable_streaming) {
        return RCOG_ERROR_INVALID_CONFIG;
    }

    /* For now, invoke sync and send as single chunk */
    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_invoke_streaming", 0.0f);


    rcog_tool_result_t result;
    int err = rcog_tool_router_invoke(router, request, &result);

    if (result.output && result.output_size > 0) {
        stream_callback(result.output, result.output_size, true, user_data);
    } else {
        stream_callback(NULL, 0, true, user_data);
    }

    rcog_tool_router_free_result(&result);
    return err;
}

int rcog_tool_router_cancel(
    rcog_tool_router_t* router,
    rcog_async_handle_t* handle
) {
    if (!router || !handle) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_cancel", 0.0f);


    handle->cancelled = true;
    return RCOG_OK;
}

void rcog_tool_router_free_result(rcog_tool_result_t* result) {
    if (!result) return;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_free_result", 0.0f);


    if (result->output) {
        nimcp_free(result->output);
        result->output = NULL;
    }
    result->output_size = 0;
}

/*=============================================================================
 * ACCESS CONTROL
 *===========================================================================*/

bool rcog_tool_router_can_access(
    const rcog_tool_router_t* router,
    const char* tool_name,
    rcog_capability_tier_t tier
) {
    if (!router || !tool_name) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_can_access", 0.0f);


    int idx = find_tool_index(router, tool_name);
    if (idx < 0) {
        return false;
    }

    return check_tier_access(tier, router->tools[idx].def.min_tier);
}

int rcog_tool_router_get_accessible_tools(
    const rcog_tool_router_t* router,
    rcog_capability_tier_t tier,
    char (*tools)[RCOG_ROUTER_MAX_TOOL_NAME],
    size_t max_tools,
    size_t* num_tools
) {
    if (!router || !tools || !num_tools) return RCOG_ERROR_NULL_POINTER;

    *num_tools = 0;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_get_accessible_tools", 0.0f);


    for (size_t i = 0; i < RCOG_ROUTER_MAX_TOOLS && *num_tools < max_tools; i++) {
        if (router->tools[i].in_use &&
            check_tier_access(tier, router->tools[i].def.min_tier)) {
            strncpy(tools[*num_tools], router->tools[i].def.name,
                   RCOG_ROUTER_MAX_TOOL_NAME - 1);
            (*num_tools)++;
        }
    }

    return RCOG_OK;
}

int rcog_tool_router_get_min_tier(
    const rcog_tool_router_t* router,
    const char* tool_name,
    rcog_capability_tier_t* tier
) {
    if (!router || !tool_name || !tier) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_get_min_tier", 0.0f);


    int idx = find_tool_index(router, tool_name);
    if (idx < 0) return RCOG_ERROR_TOOL_NOT_FOUND;

    *tier = router->tools[idx].def.min_tier;
    return RCOG_OK;
}

int rcog_tool_router_set_access_policy(
    rcog_tool_router_t* router,
    const rcog_access_policy_t* policy
) {
    if (!router || !policy) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_set_access_policy", 0.0f);


    nimcp_mutex_lock(router->mutex);
    router->config.access_policy = *policy;
    nimcp_mutex_unlock(router->mutex);

    return RCOG_OK;
}

/*=============================================================================
 * DISCOVERY
 *===========================================================================*/

int rcog_tool_router_list_tools(
    const rcog_tool_router_t* router,
    char (*tools)[RCOG_ROUTER_MAX_TOOL_NAME],
    size_t max_tools,
    size_t* num_tools
) {
    if (!router || !tools || !num_tools) return RCOG_ERROR_NULL_POINTER;

    *num_tools = 0;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_list_tools", 0.0f);


    for (size_t i = 0; i < RCOG_ROUTER_MAX_TOOLS && *num_tools < max_tools; i++) {
        if (router->tools[i].in_use) {
            strncpy(tools[*num_tools], router->tools[i].def.name,
                   RCOG_ROUTER_MAX_TOOL_NAME - 1);
            (*num_tools)++;
        }
    }

    return RCOG_OK;
}

int rcog_tool_router_list_tools_by_tier(
    const rcog_tool_router_t* router,
    rcog_capability_tier_t tier,
    char (*tools)[RCOG_ROUTER_MAX_TOOL_NAME],
    size_t max_tools,
    size_t* num_tools
) {
    if (!router || !tools || !num_tools) return RCOG_ERROR_NULL_POINTER;

    *num_tools = 0;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_list_tools_by_tier", 0.0f);


    for (size_t i = 0; i < RCOG_ROUTER_MAX_TOOLS && *num_tools < max_tools; i++) {
        if (router->tools[i].in_use && router->tools[i].def.min_tier == tier) {
            strncpy(tools[*num_tools], router->tools[i].def.name,
                   RCOG_ROUTER_MAX_TOOL_NAME - 1);
            (*num_tools)++;
        }
    }

    return RCOG_OK;
}

int rcog_tool_router_search_tools(
    const rcog_tool_router_t* router,
    const char* pattern,
    char (*tools)[RCOG_ROUTER_MAX_TOOL_NAME],
    size_t max_tools,
    size_t* num_tools
) {
    if (!router || !pattern || !tools || !num_tools) return RCOG_ERROR_NULL_POINTER;

    *num_tools = 0;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_search_tools", 0.0f);


    for (size_t i = 0; i < RCOG_ROUTER_MAX_TOOLS && *num_tools < max_tools; i++) {
        if (router->tools[i].in_use) {
            /* Simple substring match (could be enhanced with glob pattern) */
            if (strstr(router->tools[i].def.name, pattern) != NULL) {
                strncpy(tools[*num_tools], router->tools[i].def.name,
                       RCOG_ROUTER_MAX_TOOL_NAME - 1);
                (*num_tools)++;
            }
        }
    }

    return RCOG_OK;
}

size_t rcog_tool_router_get_tool_count(const rcog_tool_router_t* router) {
    if (!router) return 0;
    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_get_tool_count", 0.0f);


    return router->num_tools;
}

/*=============================================================================
 * BUILTIN TOOLS
 *===========================================================================*/

static rcog_error_t builtin_memory_read(
    const void* input, size_t input_size,
    void* context, void** output, size_t* output_size
) {
    /* Placeholder implementation */
    (void)input;
    (void)input_size;
    (void)context;

    const char* result = "{\"status\": \"ok\", \"data\": null}";
    size_t len = strlen(result);

    *output = nimcp_malloc(len + 1);
    if (!*output) return RCOG_ERROR_OUT_OF_MEMORY;

    memcpy(*output, result, len + 1);
    *output_size = len;

    return RCOG_OK;
}

static rcog_error_t builtin_memory_write(
    const void* input, size_t input_size,
    void* context, void** output, size_t* output_size
) {
    (void)input;
    (void)input_size;
    (void)context;

    const char* result = "{\"status\": \"ok\", \"written\": true}";
    size_t len = strlen(result);

    *output = nimcp_malloc(len + 1);
    if (!*output) return RCOG_ERROR_OUT_OF_MEMORY;

    memcpy(*output, result, len + 1);
    *output_size = len;

    return RCOG_OK;
}

static rcog_error_t builtin_memory_query(
    const void* input, size_t input_size,
    void* context, void** output, size_t* output_size
) {
    (void)input;
    (void)input_size;
    (void)context;

    const char* result = "{\"status\": \"ok\", \"matches\": []}";
    size_t len = strlen(result);

    *output = nimcp_malloc(len + 1);
    if (!*output) return RCOG_ERROR_OUT_OF_MEMORY;

    memcpy(*output, result, len + 1);
    *output_size = len;

    return RCOG_OK;
}

static rcog_error_t builtin_feature_extract(
    const void* input, size_t input_size,
    void* context, void** output, size_t* output_size
) {
    (void)input;
    (void)input_size;
    (void)context;

    const char* result = "{\"status\": \"ok\", \"features\": []}";
    size_t len = strlen(result);

    *output = nimcp_malloc(len + 1);
    if (!*output) return RCOG_ERROR_OUT_OF_MEMORY;

    memcpy(*output, result, len + 1);
    *output_size = len;

    return RCOG_OK;
}

static rcog_error_t builtin_pattern_match(
    const void* input, size_t input_size,
    void* context, void** output, size_t* output_size
) {
    (void)input;
    (void)input_size;
    (void)context;

    const char* result = "{\"status\": \"ok\", \"matched\": false}";
    size_t len = strlen(result);

    *output = nimcp_malloc(len + 1);
    if (!*output) return RCOG_ERROR_OUT_OF_MEMORY;

    memcpy(*output, result, len + 1);
    *output_size = len;

    return RCOG_OK;
}

static rcog_error_t builtin_output_text(
    const void* input, size_t input_size,
    void* context, void** output, size_t* output_size
) {
    (void)context;

    /* Echo input as output */
    if (input && input_size > 0) {
        *output = nimcp_malloc(input_size);
        if (!*output) return RCOG_ERROR_OUT_OF_MEMORY;
        memcpy(*output, input, input_size);
        *output_size = input_size;
    } else {
        *output = NULL;
        *output_size = 0;
    }

    return RCOG_OK;
}

size_t rcog_tool_router_register_l1_builtins(rcog_tool_router_t* router) {
    if (!router) return 0;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_register_l1_builtins", 0.0f);


    size_t registered = 0;

    /* Register L1 reasoning category */
    rcog_tool_router_register_category(router, "reasoning",
        "Reasoning and memory tools", RCOG_TIER_L1_REASONING);

    /* memory_read */
    rcog_tool_def_t def = rcog_tool_def_create("memory_read",
        builtin_memory_read, RCOG_TIER_L1_REASONING);
    strncpy(def.description, "Read from context store", RCOG_ROUTER_MAX_TOOL_DESC - 1);
    strncpy(def.category, "reasoning", RCOG_ROUTER_MAX_CATEGORY_NAME - 1);
    def.input_type = RCOG_TOOL_IO_TEXT;
    def.output_type = RCOG_TOOL_IO_JSON;
    if (rcog_tool_router_register(router, &def) == RCOG_OK) registered++;

    /* memory_write */
    def = rcog_tool_def_create("memory_write",
        builtin_memory_write, RCOG_TIER_L1_REASONING);
    strncpy(def.description, "Write to context store", RCOG_ROUTER_MAX_TOOL_DESC - 1);
    strncpy(def.category, "reasoning", RCOG_ROUTER_MAX_CATEGORY_NAME - 1);
    def.input_type = RCOG_TOOL_IO_JSON;
    def.output_type = RCOG_TOOL_IO_JSON;
    def.flags = RCOG_TOOL_FLAG_SIDE_EFFECTS;
    if (rcog_tool_router_register(router, &def) == RCOG_OK) registered++;

    /* memory_query */
    def = rcog_tool_def_create("memory_query",
        builtin_memory_query, RCOG_TIER_L1_REASONING);
    strncpy(def.description, "Query context with pattern", RCOG_ROUTER_MAX_TOOL_DESC - 1);
    strncpy(def.category, "reasoning", RCOG_ROUTER_MAX_CATEGORY_NAME - 1);
    def.input_type = RCOG_TOOL_IO_TEXT;
    def.output_type = RCOG_TOOL_IO_JSON;
    if (rcog_tool_router_register(router, &def) == RCOG_OK) registered++;

    return registered;
}

size_t rcog_tool_router_register_l2_builtins(rcog_tool_router_t* router) {
    if (!router) return 0;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_register_l2_builtins", 0.0f);


    size_t registered = 0;

    /* Register L2 perception category */
    rcog_tool_router_register_category(router, "perception",
        "Perception and feature extraction tools", RCOG_TIER_L2_PERCEPTION);

    /* feature_extract */
    rcog_tool_def_t def = rcog_tool_def_create("feature_extract",
        builtin_feature_extract, RCOG_TIER_L2_PERCEPTION);
    strncpy(def.description, "Extract features from input", RCOG_ROUTER_MAX_TOOL_DESC - 1);
    strncpy(def.category, "perception", RCOG_ROUTER_MAX_CATEGORY_NAME - 1);
    def.input_type = RCOG_TOOL_IO_ANY;
    def.output_type = RCOG_TOOL_IO_JSON;
    if (rcog_tool_router_register(router, &def) == RCOG_OK) registered++;

    /* pattern_match */
    def = rcog_tool_def_create("pattern_match",
        builtin_pattern_match, RCOG_TIER_L2_PERCEPTION);
    strncpy(def.description, "Match patterns in data", RCOG_ROUTER_MAX_TOOL_DESC - 1);
    strncpy(def.category, "perception", RCOG_ROUTER_MAX_CATEGORY_NAME - 1);
    def.input_type = RCOG_TOOL_IO_ANY;
    def.output_type = RCOG_TOOL_IO_JSON;
    if (rcog_tool_router_register(router, &def) == RCOG_OK) registered++;

    return registered;
}

size_t rcog_tool_router_register_l3_builtins(rcog_tool_router_t* router) {
    if (!router) return 0;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_register_l3_builtins", 0.0f);


    size_t registered = 0;

    /* Register L3 action category */
    rcog_tool_router_register_category(router, "action",
        "Action and output tools", RCOG_TIER_L3_ACTION);

    /* output_text */
    rcog_tool_def_t def = rcog_tool_def_create("output_text",
        builtin_output_text, RCOG_TIER_L3_ACTION);
    strncpy(def.description, "Generate text output", RCOG_ROUTER_MAX_TOOL_DESC - 1);
    strncpy(def.category, "action", RCOG_ROUTER_MAX_CATEGORY_NAME - 1);
    def.input_type = RCOG_TOOL_IO_TEXT;
    def.output_type = RCOG_TOOL_IO_TEXT;
    def.flags = RCOG_TOOL_FLAG_SIDE_EFFECTS;
    if (rcog_tool_router_register(router, &def) == RCOG_OK) registered++;

    return registered;
}

size_t rcog_tool_router_register_all_builtins(rcog_tool_router_t* router) {
    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_register_all_builtin", 0.0f);


    size_t total = 0;
    total += rcog_tool_router_register_l1_builtins(router);
    total += rcog_tool_router_register_l2_builtins(router);
    total += rcog_tool_router_register_l3_builtins(router);
    return total;
}

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

int rcog_tool_router_get_stats(
    const rcog_tool_router_t* router,
    rcog_router_stats_t* stats
) {
    if (!router || !stats) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_get_stats", 0.0f);


    nimcp_mutex_lock(((rcog_tool_router_t*)router)->mutex);
    *stats = router->stats;
    nimcp_mutex_unlock(((rcog_tool_router_t*)router)->mutex);

    return RCOG_OK;
}

int rcog_tool_router_get_tool_stats(
    const rcog_tool_router_t* router,
    const char* tool_name,
    rcog_tool_stats_t* stats
) {
    if (!router || !tool_name || !stats) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_get_tool_stats", 0.0f);


    int idx = find_tool_index(router, tool_name);
    if (idx < 0) return RCOG_ERROR_TOOL_NOT_FOUND;

    *stats = router->tools[idx].stats;
    return RCOG_OK;
}

void rcog_tool_router_reset_stats(rcog_tool_router_t* router) {
    if (!router) return;

    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_reset_stats", 0.0f);


    nimcp_mutex_lock(router->mutex);

    memset(&router->stats, 0, sizeof(router->stats));
    router->stats.tools_registered = router->num_tools;
    router->stats.categories_registered = router->num_categories;

    for (size_t i = 0; i < RCOG_ROUTER_MAX_TOOLS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && RCOG_ROUTER_MAX_TOOLS > 256) {
            rcog_tool_router_heartbeat("rcog_tool_ro_loop",
                             (float)(i + 1) / (float)RCOG_ROUTER_MAX_TOOLS);
        }

        if (router->tools[i].in_use) {
            memset(&router->tools[i].stats, 0, sizeof(rcog_tool_stats_t));
            strncpy(router->tools[i].stats.tool_name, router->tools[i].def.name,
                   RCOG_ROUTER_MAX_TOOL_NAME - 1);
        }
    }

    nimcp_mutex_unlock(router->mutex);
}

/*=============================================================================
 * UTILITY
 *===========================================================================*/

rcog_tool_def_t rcog_tool_def_create(
    const char* name,
    rcog_tool_fn handler,
    rcog_capability_tier_t tier
) {
    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_rcog_tool_def_create", 0.0f);


    rcog_tool_def_t def;
    memset(&def, 0, sizeof(def));

    if (name) {
        strncpy(def.name, name, RCOG_ROUTER_MAX_TOOL_NAME - 1);
    }
    def.handler = handler;
    def.min_tier = tier;
    def.exec_mode = RCOG_TOOL_SYNC;
    def.input_type = RCOG_TOOL_IO_ANY;
    def.output_type = RCOG_TOOL_IO_ANY;

    return def;
}

rcog_tool_request_t rcog_tool_request_create(
    const char* tool_name,
    const void* input,
    size_t input_size,
    rcog_capability_tier_t tier
) {
    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_rcog_tool_request_cr", 0.0f);


    rcog_tool_request_t req;
    memset(&req, 0, sizeof(req));

    req.tool_name = tool_name;
    req.input = input;
    req.input_size = input_size;
    req.caller_tier = tier;
    req.input_type = RCOG_TOOL_IO_ANY;

    return req;
}

const char* rcog_tool_tier_name(rcog_capability_tier_t tier) {
    switch (tier) {
        case RCOG_TIER_ROOT: return "root";
        case RCOG_TIER_L1_REASONING: return "L1_reasoning";
        case RCOG_TIER_L2_PERCEPTION: return "L2_perception";
        case RCOG_TIER_L3_ACTION: return "L3_action";
        case RCOG_TIER_L4_SPECIALIZED: return "L4_specialized";
        default: return "unknown";
    }
}

const char* rcog_tool_io_type_name(rcog_tool_io_type_t type) {
    switch (type) {
        case RCOG_TOOL_IO_ANY: return "any";
        case RCOG_TOOL_IO_TEXT: return "text";
        case RCOG_TOOL_IO_JSON: return "json";
        case RCOG_TOOL_IO_BINARY: return "binary";
        case RCOG_TOOL_IO_TENSOR: return "tensor";
        case RCOG_TOOL_IO_EMBEDDING: return "embedding";
        case RCOG_TOOL_IO_GRAPH: return "graph";
        default: return "unknown";
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int rcog_tool_router_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    rcog_tool_router_heartbeat("rcog_tool_ro_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Recursive_Cognition_Tool_Router_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                rcog_tool_router_heartbeat("rcog_tool_ro_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Log self-knowledge observations */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Recursive_Cognition_Tool_Router_Module");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Recursive_Cognition_Tool_Router_Module");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}
