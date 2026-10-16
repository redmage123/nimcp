/**
 * @file nimcp_gw_cognitive_bridge.c
 * @brief Bridge between Global Workspace and Cognitive Modules
 *
 * WHAT: Implementation of GW-Cognitive integration enabling broadcast to all
 *       cognitive modules and module competition for GW access.
 *
 * WHY: Global Workspace Theory (GWT) proposes that conscious awareness arises
 *      from a global broadcast mechanism where winning content is shared across
 *      all specialized cognitive modules.
 *
 * HOW: Cognitive modules register as receivers and submit content to compete
 *      for GW access. Winning content is broadcast to all registered modules.
 *      Competition is based on priority, relevance, and current GW state.
 *
 * BIOLOGICAL BASIS:
 * - Global Workspace corresponds to widespread cortical activation
 * - Competition occurs in thalamo-cortical loops (attention competition)
 * - Broadcast implemented by synchronized gamma oscillations
 * - Prefrontal-parietal network coordinates conscious access
 * - "Ignition" threshold determines when content reaches awareness
 *
 * @author NIMCP Development Team
 * @date 2025-01
 */

#include "cognitive/integration/nimcp_gw_cognitive_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(gw_cognitive_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_gw_cognitive_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_gw_cognitive_bridge_mesh_registry = NULL;

nimcp_error_t gw_cognitive_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_gw_cognitive_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "gw_cognitive_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "gw_cognitive_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_gw_cognitive_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_gw_cognitive_bridge_mesh_registry = registry;
    return err;
}

void gw_cognitive_bridge_mesh_unregister(void) {
    if (g_gw_cognitive_bridge_mesh_registry && g_gw_cognitive_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_gw_cognitive_bridge_mesh_registry, g_gw_cognitive_bridge_mesh_id);
        g_gw_cognitive_bridge_mesh_id = 0;
        g_gw_cognitive_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from gw_cognitive_bridge module (instance-level) */
static inline void gw_cognitive_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_gw_cognitive_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_gw_cognitive_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_gw_cognitive_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "GW_COGNITIVE_BRIDGE"

/* Forward declaration - defined later in this file */
int gw_cognitive_resolve_competition(gw_cognitive_bridge_t* bridge);

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Broadcast receiver entry
 */
typedef struct broadcast_receiver {
    uint32_t module_id;
    gw_cognitive_receiver_callback_t callback;
    void* user_data;
    bool active;
} broadcast_receiver_t;

/**
 * @brief Content stored in the Global Workspace
 */
typedef struct gw_content {
    gw_cognitive_content_type_t content_type;
    void* data;
    size_t data_size;
    uint32_t source_module;
    float priority;
    uint64_t timestamp;
} gw_content_t;

/**
 * @brief Competition entry for a module
 */
typedef struct competition_entry {
    uint32_t module_id;
    gw_cognitive_content_t content;
    void* content_data_copy;       /* Copy of content data for persistence */
    float priority;
    bool pending;
} competition_entry_t;

/**
 * @brief Full GW-Cognitive bridge structure
 */
struct gw_cognitive_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    gw_cognitive_config_t config;
    gw_content_t current_content;
    broadcast_receiver_t* receivers;
    size_t receiver_capacity;
    size_t receiver_count;
    competition_entry_t* competitors;
    size_t competitor_capacity;
    size_t competitor_count;
    gw_cognitive_stats_t stats;
    bool initialized;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds (simplified)
 */
static uint64_t get_timestamp_ms(void) {
    /* Use a simple counter for now; could be replaced with actual time */
    static _Atomic uint64_t counter = 0;
    return atomic_fetch_add(&counter, 1) + 1;
}

/**
 * @brief Free current content data if allocated
 */
static void free_current_content(gw_cognitive_bridge_t* bridge) {
    if (bridge->current_content.data) {
        nimcp_free(bridge->current_content.data);
        bridge->current_content.data = NULL;
        bridge->current_content.data_size = 0;
    }
}

/**
 * @brief Free competitor content data copy
 */
static void free_competitor_data(competition_entry_t* entry) {
    if (entry->content_data_copy) {
        nimcp_free(entry->content_data_copy);
        entry->content_data_copy = NULL;
    }
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

int gw_cognitive_default_config(gw_cognitive_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_cognitive_bridge_heartbeat("gw_cognitive_gw_cognitive_default", 0.0f);


    config->broadcast_threshold = 0.5f;
    config->competition_timeout_ms = 100;
    config->max_competitors = 16;
    config->enable_auto_broadcast = true;
    config->enable_priority_decay = false;
    config->priority_decay_rate = 0.1f;
    config->min_competition_priority = 0.1f;

    return 0;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

gw_cognitive_bridge_t* gw_cognitive_bridge_create(
    const gw_cognitive_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    gw_cognitive_bridge_heartbeat("gw_cognitive_create", 0.0f);


    gw_cognitive_bridge_t* bridge = nimcp_calloc(1, sizeof(gw_cognitive_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        gw_cognitive_default_config(&bridge->config);
    }

    /* Allocate receivers array with capacity of 64 */
    bridge->receiver_capacity = GW_COGNITIVE_MAX_RECEIVERS;
    bridge->receivers = nimcp_calloc(bridge->receiver_capacity, sizeof(broadcast_receiver_t));
    if (!bridge->receivers) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gw_cognitive_bridge_create: bridge->receivers is NULL");
        return NULL;
    }
    bridge->receiver_count = 0;

    /* Allocate competitors array */
    bridge->competitor_capacity = bridge->config.max_competitors;
    bridge->competitors = nimcp_calloc(bridge->competitor_capacity, sizeof(competition_entry_t));
    if (!bridge->competitors) {
        nimcp_free(bridge->receivers);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gw_cognitive_bridge_create: bridge->competitors is NULL");
        return NULL;
    }
    bridge->competitor_count = 0;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "gw_cognitive") != 0) {
        nimcp_free(bridge->competitors);
        nimcp_free(bridge->receivers);
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        nimcp_free(bridge->competitors);
        nimcp_free(bridge->receivers);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_cognitive_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize current content */
    memset(&bridge->current_content, 0, sizeof(gw_content_t));

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(gw_cognitive_stats_t));

    bridge->initialized = true;

    return bridge;
}

void gw_cognitive_bridge_destroy(gw_cognitive_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Free current content data if allocated */
    /* Phase 8: Heartbeat at operation start */
    gw_cognitive_bridge_heartbeat("gw_cognitive_destroy", 0.0f);


    free_current_content(bridge);

    /* Free competitor content data copies */
    if (bridge->competitors) {
        for (size_t i = 0; i < bridge->competitor_capacity; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->competitor_capacity > 256) {
                gw_cognitive_bridge_heartbeat("gw_cognitive_loop",
                                 (float)(i + 1) / (float)bridge->competitor_capacity);
            }

            free_competitor_data(&bridge->competitors[i]);
        }
        nimcp_free(bridge->competitors);
    }

    /* Free receivers array */
    if (bridge->receivers) {
        nimcp_free(bridge->receivers);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    bridge->initialized = false;

    nimcp_free(bridge);
    bridge = NULL;
}

/* ============================================================================
 * Core Functions
 * ============================================================================ */

int gw_cognitive_broadcast(
    gw_cognitive_bridge_t* bridge,
    gw_cognitive_content_type_t content_type,
    const void* content_data,
    size_t content_size)
{
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_cognitive_broadcast: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_cognitive_bridge_heartbeat("gw_cognitive_gw_cognitive_broadca", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Update current content */
    free_current_content(bridge);

    if (content_data && content_size > 0) {
        bridge->current_content.data = nimcp_malloc(content_size);
        if (!bridge->current_content.data) {
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gw_cognitive_broadcast: bridge->current_content is NULL");
            return -1;
        }
        memcpy(bridge->current_content.data, content_data, content_size);
        bridge->current_content.data_size = content_size;
    }
    bridge->current_content.content_type = content_type;
    bridge->current_content.timestamp = get_timestamp_ms();

    /* Snapshot active receivers under lock, then invoke callbacks outside lock
     * to avoid deadlock/priority-inversion from user callbacks. */
    size_t recv_cap = bridge->receiver_capacity;
    size_t snap_count = 0;

    /* Stack-allocate snapshot array for typical small receiver lists.
     * Use heap for large lists (>64 receivers). */
    broadcast_receiver_t stack_snap[64];
    broadcast_receiver_t* snap = (recv_cap <= 64) ? stack_snap : NULL;
    if (!snap && recv_cap > 0) {
        snap = (broadcast_receiver_t*)nimcp_malloc(recv_cap * sizeof(broadcast_receiver_t));
    }

    if (snap) {
        for (size_t i = 0; i < recv_cap; i++) {
            if (bridge->receivers[i].active && bridge->receivers[i].callback) {
                snap[snap_count++] = bridge->receivers[i];
            }
        }
    }

    /* Increment statistics */
    bridge->stats.broadcasts_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Invoke callbacks outside the lock */
    if (snap) {
        for (size_t i = 0; i < snap_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && snap_count > 256) {
                gw_cognitive_bridge_heartbeat("gw_cognitive_loop",
                                 (float)(i + 1) / (float)snap_count);
            }

            snap[i].callback(
                content_type,
                content_data,
                content_size,
                snap[i].user_data
            );
        }
        if (snap != stack_snap) {
            nimcp_free(snap);
        }
    }

    return 0;
}

int gw_cognitive_compete_for_access(
    gw_cognitive_bridge_t* bridge,
    uint32_t module_id,
    const gw_cognitive_content_t* content,
    float priority)
{
    if (!bridge || !bridge->initialized || !content) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_cognitive_compete_for_access: required parameter is NULL (bridge, bridge->initialized, content)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_cognitive_bridge_heartbeat("gw_cognitive_gw_cognitive_compete", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if priority meets threshold (normal rejection — not an error) */
    if (priority < bridge->config.broadcast_threshold) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1;  /* Rejected - priority too low */
    }

    /* Check if priority meets minimum competition priority (normal rejection) */
    if (priority < bridge->config.min_competition_priority) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1;  /* Rejected */
    }

    /* Find if this module already has a pending entry */
    int existing_slot = -1;
    for (size_t i = 0; i < bridge->competitor_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->competitor_capacity > 256) {
            gw_cognitive_bridge_heartbeat("gw_cognitive_loop",
                             (float)(i + 1) / (float)bridge->competitor_capacity);
        }

        if (bridge->competitors[i].pending &&
            bridge->competitors[i].module_id == module_id) {
            existing_slot = (int)i;
            break;
        }
    }

    /* Find slot for this competitor */
    int slot = existing_slot;
    if (slot < 0) {
        /* Find empty slot */
        for (size_t i = 0; i < bridge->competitor_capacity; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->competitor_capacity > 256) {
                gw_cognitive_bridge_heartbeat("gw_cognitive_loop",
                                 (float)(i + 1) / (float)bridge->competitor_capacity);
            }

            if (!bridge->competitors[i].pending) {
                slot = (int)i;
                break;
            }
        }
    }

    if (slot < 0) {
        /* No room for new competitors */
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1;  /* Rejected - competition full */
    }

    /* Free existing data if updating */
    if (existing_slot >= 0) {
        free_competitor_data(&bridge->competitors[slot]);
    } else {
        bridge->competitor_count++;
    }

    /* Add to competitors array */
    bridge->competitors[slot].module_id = module_id;
    bridge->competitors[slot].content = *content;
    bridge->competitors[slot].priority = priority;
    bridge->competitors[slot].pending = true;

    /* Copy content data for persistence */
    if (content->content_data && content->content_size > 0) {
        bridge->competitors[slot].content_data_copy = nimcp_malloc(content->content_size);
        if (bridge->competitors[slot].content_data_copy) {
            memcpy(bridge->competitors[slot].content_data_copy,
                   content->content_data, content->content_size);
            /* Update content pointer to our copy */
            bridge->competitors[slot].content.content_data =
                bridge->competitors[slot].content_data_copy;
        }
    } else {
        bridge->competitors[slot].content_data_copy = NULL;
    }

    /* Update active competitors stat */
    bridge->stats.active_competitors = (uint32_t)bridge->competitor_count;

    /* Check if this priority wins (highest priority) */
    bool is_winner = true;
    for (size_t i = 0; i < bridge->competitor_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->competitor_capacity > 256) {
            gw_cognitive_bridge_heartbeat("gw_cognitive_loop",
                             (float)(i + 1) / (float)bridge->competitor_capacity);
        }

        if (bridge->competitors[i].pending && (int)i != slot) {
            if (bridge->competitors[i].priority > priority) {
                is_winner = false;
                break;
            }
        }
    }

    if (is_winner && bridge->config.enable_auto_broadcast) {
        /* Winner gets broadcast immediately */
        nimcp_mutex_unlock(bridge->base.mutex);

        /* Resolve competition (which will broadcast) */
        gw_cognitive_resolve_competition(bridge);
        return 0;  /* Won */
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 1;  /* Pending - waiting for competition resolution */
}

int gw_cognitive_register_receiver(
    gw_cognitive_bridge_t* bridge,
    uint32_t module_id,
    gw_cognitive_receiver_callback_t callback,
    void* user_data)
{
    if (!bridge || !bridge->initialized || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_cognitive_register_receiver: required parameter is NULL (bridge, bridge->initialized, callback)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_cognitive_bridge_heartbeat("gw_cognitive_gw_cognitive_registe", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if already registered */
    for (size_t i = 0; i < bridge->receiver_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->receiver_capacity > 256) {
            gw_cognitive_bridge_heartbeat("gw_cognitive_loop",
                             (float)(i + 1) / (float)bridge->receiver_capacity);
        }

        if (bridge->receivers[i].active &&
            bridge->receivers[i].module_id == module_id) {
            /* Update existing registration */
            bridge->receivers[i].callback = callback;
            bridge->receivers[i].user_data = user_data;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    /* Find empty slot */
    for (size_t i = 0; i < bridge->receiver_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->receiver_capacity > 256) {
            gw_cognitive_bridge_heartbeat("gw_cognitive_loop",
                             (float)(i + 1) / (float)bridge->receiver_capacity);
        }

        if (!bridge->receivers[i].active) {
            bridge->receivers[i].module_id = module_id;
            bridge->receivers[i].callback = callback;
            bridge->receivers[i].user_data = user_data;
            bridge->receivers[i].active = true;
            bridge->receiver_count++;
            bridge->stats.registered_receivers = (uint32_t)bridge->receiver_count;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return -1;  /* No room for more receivers */
}

int gw_cognitive_unregister_receiver(
    gw_cognitive_bridge_t* bridge,
    uint32_t module_id)
{
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_cognitive_unregister_receiver: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_cognitive_bridge_heartbeat("gw_cognitive_gw_cognitive_unregis", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Find and deactivate receiver */
    for (size_t i = 0; i < bridge->receiver_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->receiver_capacity > 256) {
            gw_cognitive_bridge_heartbeat("gw_cognitive_loop",
                             (float)(i + 1) / (float)bridge->receiver_capacity);
        }

        if (bridge->receivers[i].active &&
            bridge->receivers[i].module_id == module_id) {
            bridge->receivers[i].active = false;
            bridge->receivers[i].callback = NULL;
            bridge->receivers[i].user_data = NULL;
            if (bridge->receiver_count > 0) {
                bridge->receiver_count--;
            }
            bridge->stats.registered_receivers = (uint32_t)bridge->receiver_count;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return -1;  /* Not found */
}

int gw_cognitive_get_conscious_content(
    gw_cognitive_bridge_t* bridge,
    gw_cognitive_conscious_content_t* content_out)
{
    if (!bridge || !bridge->initialized || !content_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_cognitive_get_conscious_content: required parameter is NULL (bridge, bridge->initialized, content_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_cognitive_bridge_heartbeat("gw_cognitive_gw_cognitive_get_con", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if there is content (normal: workspace may be empty) */
    if (!bridge->current_content.data && bridge->current_content.data_size == 0) {
        content_out->has_content = false;
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1;  /* No content */
    }

    /* Copy current content to output */
    content_out->content_type = bridge->current_content.content_type;
    content_out->source_module_id = bridge->current_content.source_module;
    content_out->winning_priority = bridge->current_content.priority;
    content_out->duration_ms = 0;  /* Would need time tracking for this */
    content_out->broadcast_count = 1;  /* Simplified */
    content_out->has_content = true;

    /* Copy data if buffer provided and large enough */
    if (content_out->content_buffer && content_out->buffer_size > 0) {
        size_t copy_size = bridge->current_content.data_size;
        if (copy_size > content_out->buffer_size) {
            copy_size = content_out->buffer_size;
        }
        if (bridge->current_content.data && copy_size > 0) {
            memcpy(content_out->content_buffer, bridge->current_content.data, copy_size);
        }
        content_out->content_size = bridge->current_content.data_size;
    } else {
        content_out->content_size = bridge->current_content.data_size;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int gw_cognitive_resolve_competition(gw_cognitive_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_cognitive_resolve_competition: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_cognitive_bridge_heartbeat("gw_cognitive_gw_cognitive_resolve", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Find highest priority competitor */
    int winner_slot = -1;
    float highest_priority = -1.0f;

    for (size_t i = 0; i < bridge->competitor_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->competitor_capacity > 256) {
            gw_cognitive_bridge_heartbeat("gw_cognitive_loop",
                             (float)(i + 1) / (float)bridge->competitor_capacity);
        }

        if (bridge->competitors[i].pending) {
            if (bridge->competitors[i].priority > highest_priority) {
                highest_priority = bridge->competitors[i].priority;
                winner_slot = (int)i;
            }
        }
    }

    if (winner_slot < 0) {
        /* No competitors */
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Get winner's content */
    competition_entry_t* winner = &bridge->competitors[winner_slot];

    /* Update current content with winner's data */
    free_current_content(bridge);

    bridge->current_content.content_type = winner->content.content_type;
    bridge->current_content.source_module = winner->module_id;
    bridge->current_content.priority = winner->priority;
    bridge->current_content.timestamp = get_timestamp_ms();

    if (winner->content.content_data && winner->content.content_size > 0) {
        bridge->current_content.data = nimcp_malloc(winner->content.content_size);
        if (bridge->current_content.data) {
            memcpy(bridge->current_content.data,
                   winner->content.content_data,
                   winner->content.content_size);
            bridge->current_content.data_size = winner->content.content_size;
        }
    }

    /* Update statistics */
    bridge->stats.competitions_held++;
    bridge->stats.content_updates++;

    /* Update average winning priority */
    if (bridge->stats.competitions_held == 1) {
        bridge->stats.avg_winning_priority = highest_priority;
    } else {
        bridge->stats.avg_winning_priority =
            (bridge->stats.avg_winning_priority * (bridge->stats.competitions_held - 1) +
             highest_priority) / bridge->stats.competitions_held;
    }

    /* Store broadcast data for after unlock */
    gw_cognitive_content_type_t broadcast_type = winner->content.content_type;
    void* broadcast_data = NULL;
    size_t broadcast_size = winner->content.content_size;

    if (winner->content.content_data && broadcast_size > 0) {
        broadcast_data = nimcp_malloc(broadcast_size);
        if (broadcast_data) {
            memcpy(broadcast_data, winner->content.content_data, broadcast_size);
        }
    }

    /* Clear all competitors */
    for (size_t i = 0; i < bridge->competitor_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->competitor_capacity > 256) {
            gw_cognitive_bridge_heartbeat("gw_cognitive_loop",
                             (float)(i + 1) / (float)bridge->competitor_capacity);
        }

        if (bridge->competitors[i].pending) {
            free_competitor_data(&bridge->competitors[i]);
            bridge->competitors[i].pending = false;
        }
    }
    bridge->competitor_count = 0;
    bridge->stats.active_competitors = 0;

    /* Snapshot active receivers under lock, then invoke outside lock
     * to avoid deadlock/priority-inversion from user callbacks. */
    size_t recv_cap2 = bridge->receiver_capacity;
    size_t snap_count2 = 0;

    broadcast_receiver_t stack_snap2[64];
    broadcast_receiver_t* snap2 = (recv_cap2 <= 64) ? stack_snap2 : NULL;
    if (!snap2 && recv_cap2 > 0) {
        snap2 = (broadcast_receiver_t*)nimcp_malloc(recv_cap2 * sizeof(broadcast_receiver_t));
    }

    if (snap2) {
        for (size_t i = 0; i < recv_cap2; i++) {
            if (bridge->receivers[i].active && bridge->receivers[i].callback) {
                snap2[snap_count2++] = bridge->receivers[i];
            }
        }
    }

    bridge->stats.broadcasts_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Invoke callbacks outside the lock */
    if (snap2) {
        for (size_t i = 0; i < snap_count2; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && snap_count2 > 256) {
                gw_cognitive_bridge_heartbeat("gw_cognitive_loop",
                                 (float)(i + 1) / (float)snap_count2);
            }

            snap2[i].callback(
                broadcast_type,
                broadcast_data,
                broadcast_size,
                snap2[i].user_data
            );
        }
        if (snap2 != stack_snap2) {
            nimcp_free(snap2);
        }
    }

    /* Free temporary broadcast data */
    if (broadcast_data) {
        nimcp_free(broadcast_data);
        broadcast_data = NULL;
    }

    return 0;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

int gw_cognitive_get_stats(
    const gw_cognitive_bridge_t* bridge,
    gw_cognitive_stats_t* stats_out)
{
    if (!bridge || !bridge->initialized || !stats_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_cognitive_get_stats: required parameter is NULL (bridge, bridge->initialized, stats_out)");
        return -1;
    }

    /* Note: For const correctness, we cast away const for mutex lock.
     * This is acceptable because the mutex operation doesn't modify
     * the logical state of the bridge. */
    /* Phase 8: Heartbeat at operation start */
    gw_cognitive_bridge_heartbeat("gw_cognitive_gw_cognitive_get_sta", 0.0f);


    nimcp_mutex_lock(((gw_cognitive_bridge_t*)bridge)->base.mutex);

    *stats_out = bridge->stats;

    nimcp_mutex_unlock(((gw_cognitive_bridge_t*)bridge)->base.mutex);

    return 0;
}

int gw_cognitive_reset_stats(gw_cognitive_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_cognitive_reset_stats: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_cognitive_bridge_heartbeat("gw_cognitive_gw_cognitive_reset_s", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    memset(&bridge->stats, 0, sizeof(gw_cognitive_stats_t));

    /* Restore current counts */
    bridge->stats.registered_receivers = (uint32_t)bridge->receiver_count;
    bridge->stats.active_competitors = (uint32_t)bridge->competitor_count;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void gw_cognitive_bridge_set_instance_health_agent(gw_cognitive_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "gw_cognitive_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int gw_cognitive_bridge_training_begin(gw_cognitive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gw_cognitive_bridge_training_begin: NULL argument");
        return -1;
    }
    gw_cognitive_bridge_heartbeat_instance(bridge->health_agent, "gw_cognitive_bridge_training_begin", 0.0f);
    return 0;
}

int gw_cognitive_bridge_training_end(gw_cognitive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gw_cognitive_bridge_training_end: NULL argument");
        return -1;
    }
    gw_cognitive_bridge_heartbeat_instance(bridge->health_agent, "gw_cognitive_bridge_training_end", 1.0f);
    return 0;
}

int gw_cognitive_bridge_training_step(gw_cognitive_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gw_cognitive_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    gw_cognitive_bridge_heartbeat_instance(bridge->health_agent, "gw_cognitive_bridge_training_step", progress);
    return 0;
}
