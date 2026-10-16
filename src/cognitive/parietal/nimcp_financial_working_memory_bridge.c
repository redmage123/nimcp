//=============================================================================
// nimcp_financial_working_memory_bridge.c - Financial Working Memory Bridge
//=============================================================================
/**
 * @file nimcp_financial_working_memory_bridge.c
 * @brief Implementation of working memory bridge for financial data
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include "cognitive/parietal/nimcp_financial_working_memory_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/error/nimcp_error_codes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

/* Health agent: using pre-existing custom implementation */
static nimcp_health_agent_t* g_fin_wm_health_agent = NULL;


/* Forward declarations for immune/BBB validation stubs */
extern int brain_immune_validate_operation(brain_immune_system_t* immune,
                                            const char* operation,
                                            uint32_t severity);
extern int brain_immune_present_antigen(brain_immune_system_t* immune,
                                         int source,
                                         const uint8_t* epitope,
                                         size_t epitope_len,
                                         uint32_t severity,
                                         uint32_t source_node,
                                         uint32_t* antigen_id);
extern int bbb_validate_data(bbb_system_t bbb, const void* data,
                              size_t size, const char* context);

/* Stub declarations for subsystem integration globals */
static void* g_fin_wm_bridge_immune = NULL;
static void* g_fin_wm_bridge_bbb = NULL;

BRIDGE_DEFINE_MESH_REGISTRATION(fin_wm, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// KG Wiring Integration (Change Set 1)
//=============================================================================

struct kg_wiring;
typedef struct kg_wiring kg_wiring_t;

/* KG message type defines for working memory bridge module */
#define KG_MSG_FIN_WM_ITEM_ADD      "FIN_WM_ITEM_ADD"
#define KG_MSG_FIN_WM_ITEM_EVICT    "FIN_WM_ITEM_EVICT"
#define KG_MSG_FIN_WM_DECAY         "FIN_WM_DECAY"
#define KG_MSG_FIN_WM_CLEAR         "FIN_WM_CLEAR"

//=============================================================================
// Global Health Heartbeat Helper
//=============================================================================

static inline void fin_wm_heartbeat(const char* operation, float progress) {
    if (g_fin_wm_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_fin_wm_health_agent, operation, progress);
    }
}

//=============================================================================
// Thread-Local Error Storage
//=============================================================================

static _Thread_local char fin_wm_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_wm_last_error, sizeof(fin_wm_last_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Global Validation Helper
//=============================================================================

static int fin_wm_bridge_validate_subsystems(const char* operation) {
    if (g_fin_wm_bridge_immune) {
        int rc = brain_immune_validate_operation(g_fin_wm_bridge_immune, operation, 5);
        if (rc != 0) {
            set_error("financial_wm_bridge: immune validation failed for %s", operation);
            return FIN_WM_ERR_SUBSYSTEM;
        }
    }
    if (g_fin_wm_bridge_bbb) {
        int rc = bbb_validate_data(g_fin_wm_bridge_bbb, NULL, 0, operation);
        if (rc != 0) {
            set_error("financial_wm_bridge: BBB validation failed for %s", operation);
            return FIN_WM_ERR_SUBSYSTEM;
        }
    }
    return FIN_WM_ERR_OK;
}

//=============================================================================
// Internal Structure
//=============================================================================

struct financial_wm_bridge {
    fin_wm_bridge_config_t config;
    fin_wm_bridge_stats_t stats;

    /* Working memory slots (circular buffer with salience ordering) */
    fin_wm_item_t* items;
    uint32_t item_count;
    uint32_t capacity;

    /* Subsystem pointers */
    working_memory_t* wm;
    brain_immune_system_t* immune;
    bbb_system_t bbb;
    kg_wiring_t* kg_wiring;
    nimcp_health_agent_t* health_agent;
    void* logger;
    bio_async_context_t* bio_async;

    /* Validation flags */
    bool enable_bbb_validation;
    bool enable_immune_validation;
};

//=============================================================================
// Instance-Level Heartbeat Helper (Phase 8)
//=============================================================================

static inline void wm_heartbeat_instance(financial_wm_bridge_t* bridge,
                                          const char* op, float progress) {
    /* Global heartbeat */
    if (g_fin_wm_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_fin_wm_health_agent, op, progress);
    }
    /* Instance heartbeat */
    if (bridge && bridge->health_agent &&
        bridge->health_agent != g_fin_wm_health_agent) {
        nimcp_health_agent_heartbeat_ex(bridge->health_agent, op, progress);
        bridge->stats.health_heartbeats++;
    }
}

//=============================================================================
// Logging Macros (Phase 8: Change Set 2/3)
//=============================================================================

#define FIN_WM_LOG_DEBUG(bridge, fmt, ...) /* placeholder */
#define FIN_WM_LOG_INFO(bridge, fmt, ...)  /* placeholder */
#define FIN_WM_LOG_WARN(bridge, fmt, ...)  /* placeholder */
#define FIN_WM_LOG_ERROR(bridge, fmt, ...) /* placeholder */

//=============================================================================
// KG Publish Helper
//=============================================================================

static int wm_kg_publish(financial_wm_bridge_t* bridge, const char* msg_type,
                          const void* payload, size_t size) {
    if (bridge && bridge->kg_wiring) {
        /* kg_wiring_publish would be called here */
        (void)msg_type; (void)payload; (void)size;
        bridge->stats.kg_messages_sent++;
        return 0;
    }
    return 0;
}

//=============================================================================
// Instance-Level Validation Helper
//=============================================================================

static int wm_validate_subsystems(financial_wm_bridge_t* bridge, const char* operation) {
    if (!bridge) return FIN_WM_ERR_NULL;

    if (bridge->enable_bbb_validation && bridge->bbb) {
        int rc = bbb_validate_data(bridge->bbb, NULL, 0, operation);
        if (rc != 0) {
            set_error("BBB validation failed for %s", operation);
            bridge->stats.bbb_validations++;
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_BBB_VALIDATION,
                "financial_wm: BBB validation failed for %s", operation);
            return FIN_WM_ERR_VALIDATION;
        }
        bridge->stats.bbb_validations++;
    }

    if (bridge->enable_immune_validation && bridge->immune) {
        int rc = brain_immune_validate_operation(bridge->immune, operation, 5);
        if (rc != 0) {
            set_error("Immune validation failed for %s", operation);
            bridge->stats.immune_checks++;
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_BBB_VALIDATION,
                "financial_wm: immune validation failed for %s", operation);
            return FIN_WM_ERR_VALIDATION;
        }
        bridge->stats.immune_checks++;
    }

    return FIN_WM_ERR_OK;
}

//=============================================================================
// Antigen Presentation Helper
//=============================================================================

static void wm_present_antigen(financial_wm_bridge_t* bridge,
                                const char* anomaly, uint32_t severity) {
    if (bridge && bridge->immune) {
        uint8_t sig[64] = {0};
        snprintf((char*)sig, sizeof(sig), "fin_wm:%s", anomaly);
        uint32_t antigen_id = 0;
        brain_immune_present_antigen(bridge->immune, 0, sig, strlen((char*)sig),
                                      severity, 0, &antigen_id);
    }
}

//=============================================================================
// Salience Utility Functions
//=============================================================================

/**
 * @brief Find the index of the item with minimum salience
 */
static int32_t find_min_salience_index(financial_wm_bridge_t* bridge) {
    if (!bridge || bridge->item_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_min_salience_index: bridge is NULL");
        return -1;
    }

    uint32_t min_idx = 0;
    float min_sal = bridge->items[0].salience;

    for (uint32_t i = 1; i < bridge->item_count; i++) {
        if (bridge->items[i].salience < min_sal) {
            min_sal = bridge->items[i].salience;
            min_idx = i;
        }
    }
    return (int32_t)min_idx;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

void financial_wm_bridge_default_config(fin_wm_bridge_config_t* config) {
    if (!config) return;

    config->capacity = FIN_WM_CAPACITY;
    config->decay_rate = 0.1f;  /* 10% decay per second */
    config->enable_immune_validation = false;
    config->enable_bbb_validation = false;
}

int financial_wm_bridge_create(financial_wm_bridge_t** bridge,
                               const fin_wm_bridge_config_t* config) {
    if (!bridge) {
        set_error("NULL output pointer in create");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_wm_bridge_create: NULL output pointer");
        return FIN_WM_ERR_NULL;
    }

    fin_wm_heartbeat("financial_wm_bridge_create", 0.0f);

    financial_wm_bridge_t* b = (financial_wm_bridge_t*)nimcp_malloc(sizeof(financial_wm_bridge_t));
    if (!b) {
        set_error("Failed to allocate financial_wm_bridge_t");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate financial_wm_bridge_t");
        return FIN_WM_ERR_MEMORY;
    }
    memset(b, 0, sizeof(*b));

    /* Apply configuration */
    if (config) {
        b->config = *config;
    } else {
        financial_wm_bridge_default_config(&b->config);
    }

    /* Validate and clamp capacity */
    if (b->config.capacity == 0) {
        b->config.capacity = FIN_WM_CAPACITY;
    }
    if (b->config.capacity > FIN_WM_MAX_CAPACITY) {
        b->config.capacity = FIN_WM_MAX_CAPACITY;
    }
    b->capacity = b->config.capacity;

    /* Allocate item storage */
    b->items = (fin_wm_item_t*)nimcp_calloc(b->capacity, sizeof(fin_wm_item_t));
    if (!b->items) {
        set_error("Failed to allocate item storage");
        nimcp_free(b);
        b = NULL;
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate working memory item storage");
        return FIN_WM_ERR_MEMORY;
    }
    /* calloc zero-initializes */
    b->item_count = 0;

    /* Initialize flags from config */
    b->enable_immune_validation = b->config.enable_immune_validation;
    b->enable_bbb_validation = b->config.enable_bbb_validation;

    /* Initialize stats */
    memset(&b->stats, 0, sizeof(b->stats));

    *bridge = b;

    fin_wm_heartbeat("financial_wm_bridge_create", 1.0f);
    return FIN_WM_ERR_OK;
}

void financial_wm_bridge_destroy(financial_wm_bridge_t* bridge) {
    if (!bridge) return;

    fin_wm_heartbeat("financial_wm_bridge_destroy", 0.0f);

    if (bridge->items) {
        nimcp_free(bridge->items);
        bridge->items = NULL;
    }

    nimcp_free(bridge);
    bridge = NULL;

    fin_wm_heartbeat("financial_wm_bridge_destroy", 1.0f);
}

//=============================================================================
// Subsystem Setters
//=============================================================================

int financial_wm_bridge_set_working_memory(financial_wm_bridge_t* bridge,
                                            working_memory_t* wm) {
    if (!bridge) {
        set_error("NULL bridge in set_working_memory");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_wm_bridge_set_working_memory: NULL bridge");
        return FIN_WM_ERR_NULL;
    }
    bridge->wm = wm;
    return FIN_WM_ERR_OK;
}

int financial_wm_bridge_set_immune(financial_wm_bridge_t* bridge,
                                    brain_immune_system_t* immune) {
    if (!bridge) {
        set_error("NULL bridge in set_immune");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_wm_bridge_set_immune: NULL bridge");
        return FIN_WM_ERR_NULL;
    }
    bridge->immune = immune;
    return FIN_WM_ERR_OK;
}

int financial_wm_bridge_set_bbb(financial_wm_bridge_t* bridge, bbb_system_t bbb) {
    if (!bridge) {
        set_error("NULL bridge in set_bbb");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_wm_bridge_set_bbb: NULL bridge");
        return FIN_WM_ERR_NULL;
    }
    bridge->bbb = bbb;
    return FIN_WM_ERR_OK;
}

int financial_wm_bridge_set_kg_wiring(financial_wm_bridge_t* bridge, kg_wiring_t* kg) {
    if (!bridge) {
        set_error("NULL bridge in set_kg_wiring");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_wm_bridge_set_kg_wiring: NULL bridge");
        return FIN_WM_ERR_NULL;
    }
    bridge->kg_wiring = kg;
    return FIN_WM_ERR_OK;
}

int financial_wm_bridge_set_health_agent(financial_wm_bridge_t* bridge,
                                          nimcp_health_agent_t* agent) {
    if (!bridge) {
        set_error("NULL bridge in set_health_agent");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_wm_bridge_set_health_agent: NULL bridge");
        return FIN_WM_ERR_NULL;
    }
    bridge->health_agent = agent;
    return FIN_WM_ERR_OK;
}

int financial_wm_bridge_set_logger(financial_wm_bridge_t* bridge, void* logger) {
    if (!bridge) {
        set_error("NULL bridge in set_logger");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_wm_bridge_set_logger: NULL bridge");
        return FIN_WM_ERR_NULL;
    }
    bridge->logger = logger;
    return FIN_WM_ERR_OK;
}

int financial_wm_bridge_set_bio_async(financial_wm_bridge_t* bridge,
                                       bio_async_context_t* ctx) {
    if (!bridge) {
        set_error("NULL bridge in set_bio_async");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_wm_bridge_set_bio_async: NULL bridge");
        return FIN_WM_ERR_NULL;
    }
    bridge->bio_async = ctx;
    return FIN_WM_ERR_OK;
}

//=============================================================================
// Core API Implementation
//=============================================================================

int financial_wm_bridge_add(financial_wm_bridge_t* bridge,
                            const fin_wm_item_t* item) {
    if (!bridge) {
        set_error("NULL bridge in add");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_wm_bridge_add: NULL bridge");
        return FIN_WM_ERR_NULL;
    }
    if (!item) {
        set_error("NULL item in add");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_wm_bridge_add: NULL item");
        return FIN_WM_ERR_NULL;
    }

    /* Instance-level validation */
    int val_rc = wm_validate_subsystems(bridge, "wm_add");
    if (val_rc != FIN_WM_ERR_OK) return val_rc;

    wm_heartbeat_instance(bridge, "wm_add", 0.0f);

    /* Validate item salience */
    float salience = nimcp_clampf(item->salience, 0.0f, 1.0f);
    if (salience < 0.001f) {
        wm_present_antigen(bridge, "zero_salience_item", 2);
    }

    uint32_t insert_idx = 0;

    /* Check if we have capacity */
    if (bridge->item_count < bridge->capacity) {
        /* Room available, append */
        insert_idx = bridge->item_count;
        bridge->item_count++;
    } else {
        /* Capacity reached - evict lowest salience item */
        int32_t min_idx = find_min_salience_index(bridge);
        if (min_idx < 0) {
            set_error("Failed to find eviction candidate");
            return FIN_WM_ERR_FULL;
        }

        /* Only evict if new item has higher salience */
        if (salience <= bridge->items[min_idx].salience) {
            set_error("New item salience too low for eviction");
            return FIN_WM_ERR_FULL;
        }

        /* Log eviction */
        FIN_WM_LOG_DEBUG(bridge, "Evicting item %u (salience %.3f) for new item (salience %.3f)",
                         min_idx, bridge->items[min_idx].salience, salience);

        /* Publish eviction message */
        wm_kg_publish(bridge, KG_MSG_FIN_WM_ITEM_EVICT,
                      &bridge->items[min_idx], sizeof(fin_wm_item_t));

        insert_idx = (uint32_t)min_idx;
        bridge->stats.items_evicted++;
    }

    /* Copy item data */
    bridge->items[insert_idx] = *item;
    bridge->items[insert_idx].salience = salience;

    /* Publish add message */
    wm_kg_publish(bridge, KG_MSG_FIN_WM_ITEM_ADD,
                  &bridge->items[insert_idx], sizeof(fin_wm_item_t));

    bridge->stats.items_added++;

    wm_heartbeat_instance(bridge, "wm_add", 1.0f);
    return FIN_WM_ERR_OK;
}

int financial_wm_bridge_get_active(financial_wm_bridge_t* bridge,
                                    fin_wm_item_t* items,
                                    uint32_t* count) {
    if (!bridge) {
        set_error("NULL bridge in get_active");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_wm_bridge_get_active: NULL bridge");
        return FIN_WM_ERR_NULL;
    }
    if (!items || !count) {
        set_error("NULL output in get_active");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_wm_bridge_get_active: NULL output");
        return FIN_WM_ERR_NULL;
    }

    wm_heartbeat_instance(bridge, "wm_get_active", 0.0f);

    /* Copy all active items */
    for (uint32_t i = 0; i < bridge->item_count; i++) {
        items[i] = bridge->items[i];
    }
    *count = bridge->item_count;

    bridge->stats.queries++;

    wm_heartbeat_instance(bridge, "wm_get_active", 1.0f);
    return FIN_WM_ERR_OK;
}

int financial_wm_bridge_decay_step(financial_wm_bridge_t* bridge, float dt_sec) {
    if (!bridge) {
        set_error("NULL bridge in decay_step");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_wm_bridge_decay_step: NULL bridge");
        return FIN_WM_ERR_NULL;
    }

    if (dt_sec <= 0.0f) return FIN_WM_ERR_OK;

    wm_heartbeat_instance(bridge, "wm_decay", 0.0f);

    float decay_factor = bridge->config.decay_rate * dt_sec;

    /* Apply decay and remove items that fall below threshold */
    uint32_t write_idx = 0;
    for (uint32_t i = 0; i < bridge->item_count; i++) {
        bridge->items[i].salience -= decay_factor;

        if (bridge->items[i].salience > 0.0f) {
            /* Keep this item */
            if (write_idx != i) {
                bridge->items[write_idx] = bridge->items[i];
            }
            write_idx++;
        } else {
            /* Item decayed to zero - evict */
            FIN_WM_LOG_DEBUG(bridge, "Item %u decayed to zero, evicting", i);
            wm_kg_publish(bridge, KG_MSG_FIN_WM_ITEM_EVICT,
                          &bridge->items[i], sizeof(fin_wm_item_t));
            bridge->stats.items_evicted++;
        }
    }

    bridge->item_count = write_idx;

    /* Publish decay event */
    wm_kg_publish(bridge, KG_MSG_FIN_WM_DECAY, &dt_sec, sizeof(float));

    wm_heartbeat_instance(bridge, "wm_decay", 1.0f);
    return FIN_WM_ERR_OK;
}

int financial_wm_bridge_refresh(financial_wm_bridge_t* bridge, uint32_t item_index) {
    if (!bridge) {
        set_error("NULL bridge in refresh");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_wm_bridge_refresh: NULL bridge");
        return FIN_WM_ERR_NULL;
    }

    if (item_index >= bridge->item_count) {
        set_error("Invalid item index %u (count=%u)", item_index, bridge->item_count);
        return FIN_WM_ERR_NOT_FOUND;
    }

    wm_heartbeat_instance(bridge, "wm_refresh", 0.0f);

    /* Boost salience back to 1.0 (or close to it) */
    float boost = 1.0f - bridge->items[item_index].salience;
    bridge->items[item_index].salience += boost * 0.8f;  /* 80% recovery */
    bridge->items[item_index].salience = nimcp_clampf(bridge->items[item_index].salience, 0.0f, 1.0f);

    bridge->stats.refreshes++;

    wm_heartbeat_instance(bridge, "wm_refresh", 1.0f);
    return FIN_WM_ERR_OK;
}

int financial_wm_bridge_get_by_type(financial_wm_bridge_t* bridge,
                                     fin_wm_item_type_t type,
                                     fin_wm_item_t* items,
                                     uint32_t* count) {
    if (!bridge) {
        set_error("NULL bridge in get_by_type");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_wm_bridge_get_by_type: NULL bridge");
        return FIN_WM_ERR_NULL;
    }
    if (!items || !count) {
        set_error("NULL output in get_by_type");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_wm_bridge_get_by_type: NULL output");
        return FIN_WM_ERR_NULL;
    }

    wm_heartbeat_instance(bridge, "wm_get_by_type", 0.0f);

    uint32_t max_items = *count;
    uint32_t found = 0;

    for (uint32_t i = 0; i < bridge->item_count && found < max_items; i++) {
        if (bridge->items[i].type == type) {
            items[found++] = bridge->items[i];
        }
    }

    *count = found;
    bridge->stats.queries++;

    wm_heartbeat_instance(bridge, "wm_get_by_type", 1.0f);
    return FIN_WM_ERR_OK;
}

int financial_wm_bridge_clear(financial_wm_bridge_t* bridge) {
    if (!bridge) {
        set_error("NULL bridge in clear");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_wm_bridge_clear: NULL bridge");
        return FIN_WM_ERR_NULL;
    }

    /* Instance-level validation */
    int val_rc = wm_validate_subsystems(bridge, "wm_clear");
    if (val_rc != FIN_WM_ERR_OK) return val_rc;

    wm_heartbeat_instance(bridge, "wm_clear", 0.0f);

    /* Clear all items */
    bridge->stats.items_evicted += bridge->item_count;
    bridge->item_count = 0;
    memset(bridge->items, 0, sizeof(fin_wm_item_t) * bridge->capacity);

    /* Publish clear message */
    wm_kg_publish(bridge, KG_MSG_FIN_WM_CLEAR, NULL, 0);

    wm_heartbeat_instance(bridge, "wm_clear", 1.0f);
    return FIN_WM_ERR_OK;
}

//=============================================================================
// Statistics & Diagnostics
//=============================================================================

int financial_wm_bridge_get_stats(const financial_wm_bridge_t* bridge,
                                   fin_wm_bridge_stats_t* stats) {
    if (!bridge) {
        set_error("NULL bridge in get_stats");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_wm_bridge_get_stats: NULL bridge");
        return FIN_WM_ERR_NULL;
    }
    if (!stats) {
        set_error("NULL stats output in get_stats");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_wm_bridge_get_stats: NULL stats");
        return FIN_WM_ERR_NULL;
    }

    *stats = bridge->stats;
    return FIN_WM_ERR_OK;
}

void financial_wm_bridge_reset_stats(financial_wm_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

const char* financial_wm_bridge_get_last_error(void) {
    return fin_wm_last_error;
}
