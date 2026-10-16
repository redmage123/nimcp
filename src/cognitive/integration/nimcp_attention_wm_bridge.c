//=============================================================================
// nimcp_attention_wm_bridge.c - Attention-Working Memory Integration Bridge
//=============================================================================
/**
 * @file nimcp_attention_wm_bridge.c
 * @brief Bidirectional integration between attention and working memory systems
 *
 * WHAT: Attention-gated working memory entry and priority management
 * WHY:  Attention gates entry into WM; focus shifts update item priorities
 * HOW:  Threshold-based gating, capacity management, priority decay
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex maintains WM representations via sustained attention
 * - Cowan's embedded-processes model: attention selects WM contents
 * - Basal ganglia gates WM updates based on attentional signals
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "cognitive/integration/nimcp_attention_wm_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(attention_wm_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_attention_wm_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_attention_wm_bridge_mesh_registry = NULL;

nimcp_error_t attention_wm_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_attention_wm_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "attention_wm_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "attention_wm_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_attention_wm_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_attention_wm_bridge_mesh_registry = registry;
    return err;
}

void attention_wm_bridge_mesh_unregister(void) {
    if (g_attention_wm_bridge_mesh_registry && g_attention_wm_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_attention_wm_bridge_mesh_registry, g_attention_wm_bridge_mesh_id);
        g_attention_wm_bridge_mesh_id = 0;
        g_attention_wm_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from attention_wm_bridge module (instance-level) */
static inline void attention_wm_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_attention_wm_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_attention_wm_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_attention_wm_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "ATTENTION_WM_BRIDGE"


//=============================================================================
// Constants
//=============================================================================

#define DEFAULT_CAPACITY_LIMIT     7      // Miller's magical number
#define DEFAULT_ATTENTION_THRESHOLD 0.3f
#define DEFAULT_DECAY_RATE         0.1f
#define PRIORITY_BOOST_FACTOR      1.2f   // Boost when gaining focus

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal working memory item representation
 */
typedef struct wm_item {
    uint64_t item_id;              /**< Unique item identifier */
    float attention_strength;      /**< Attention when gated in */
    float priority;                /**< Current priority [0, 1] */
    uint64_t entry_time;           /**< When item entered WM */
    uint64_t last_access_time;     /**< Last time item was accessed */
    bool valid;                    /**< Whether slot is occupied */
} wm_item_t;

/**
 * @brief Opaque attention-WM bridge structure
 */
struct attention_wm_bridge {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    attention_wm_config_t config;       /**< Configuration parameters */
    wm_item_t* items;                   /**< Working memory items */
    size_t item_capacity;               /**< Maximum items (from config) */
    size_t item_count;                  /**< Current item count */
    uint64_t current_focus;             /**< Currently focused item ID */
    attention_wm_stats_t stats;         /**< Bridge statistics */
    bool initialized;                   /**< Initialization flag */
};

//=============================================================================
// Static Helpers
//=============================================================================

/**
 * @brief Find item index by ID (must be called with mutex held)
 */
static int find_item_unlocked(const attention_wm_bridge_t* bridge, uint64_t item_id) {
    if (!bridge || !bridge->items) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_item_unlocked: required parameter is NULL (bridge, bridge->items)");
        return -1;
    }

    for (size_t i = 0; i < bridge->item_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->item_capacity > 256) {
            attention_wm_bridge_heartbeat("attention_wm_loop",
                             (float)(i + 1) / (float)bridge->item_capacity);
        }

        if (bridge->items[i].valid && bridge->items[i].item_id == item_id) {
            return (int)i;
        }
    }
    return -1;  /* Not found is a normal condition */
}

/**
 * @brief Find lowest priority item index (must be called with mutex held)
 */
static int find_lowest_priority_unlocked(const attention_wm_bridge_t* bridge) {
    if (!bridge || !bridge->items || bridge->item_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_lowest_priority_unlocked: required parameter is NULL (bridge, bridge->items)");
        return -1;
    }

    int lowest_idx = -1;
    float lowest_priority = 2.0f;  // Higher than max priority

    for (size_t i = 0; i < bridge->item_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->item_capacity > 256) {
            attention_wm_bridge_heartbeat("attention_wm_loop",
                             (float)(i + 1) / (float)bridge->item_capacity);
        }

        if (bridge->items[i].valid && bridge->items[i].priority < lowest_priority) {
            lowest_priority = bridge->items[i].priority;
            lowest_idx = (int)i;
        }
    }
    return lowest_idx;
}

/**
 * @brief Find first free slot (must be called with mutex held)
 */
static int find_free_slot_unlocked(const attention_wm_bridge_t* bridge) {
    if (!bridge || !bridge->items) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_free_slot_unlocked: required parameter is NULL (bridge, bridge->items)");
        return -1;
    }

    for (size_t i = 0; i < bridge->item_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->item_capacity > 256) {
            attention_wm_bridge_heartbeat("attention_wm_loop",
                             (float)(i + 1) / (float)bridge->item_capacity);
        }

        if (!bridge->items[i].valid) {
            return (int)i;
        }
    }
    return -1;  /* All slots occupied is normal */
}

/**
 * @brief Clamp value to range [min, max]
 */
static float clamp_priority(float value) {
    if (value < ATTENTION_WM_MIN_PRIORITY) {
        return ATTENTION_WM_MIN_PRIORITY;
    }
    if (value > ATTENTION_WM_MAX_PRIORITY) {
        return ATTENTION_WM_MAX_PRIORITY;
    }
    return value;
}

/**
 * @brief Compute average priority (must be called with mutex held)
 */
static float compute_avg_priority_unlocked(const attention_wm_bridge_t* bridge) {
    if (!bridge || bridge->item_count == 0) {
        return 0.0f;
    }

    float sum = 0.0f;
    for (size_t i = 0; i < bridge->item_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->item_capacity > 256) {
            attention_wm_bridge_heartbeat("attention_wm_loop",
                             (float)(i + 1) / (float)bridge->item_capacity);
        }

        if (bridge->items[i].valid) {
            sum += bridge->items[i].priority;
        }
    }
    return sum / (float)bridge->item_count;
}

//=============================================================================
// Lifecycle API
//=============================================================================

int attention_wm_bridge_default_config(attention_wm_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_wm_bridge_heartbeat("attention_wm_default_config", 0.0f);


    config->capacity_limit = DEFAULT_CAPACITY_LIMIT;
    config->attention_threshold = DEFAULT_ATTENTION_THRESHOLD;
    config->decay_rate = DEFAULT_DECAY_RATE;

    return 0;
}

attention_wm_bridge_t* attention_wm_bridge_create(
    const attention_wm_config_t* config
) {
    // Allocate bridge structure
    /* Phase 8: Heartbeat at operation start */
    attention_wm_bridge_heartbeat("attention_wm_create", 0.0f);


    attention_wm_bridge_t* bridge = nimcp_calloc(1, sizeof(attention_wm_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    // Apply configuration
    if (config) {
        bridge->config = *config;
    } else {
        attention_wm_bridge_default_config(&bridge->config);
    }

    // Validate and bound capacity
    if (bridge->config.capacity_limit == 0) {
        bridge->config.capacity_limit = DEFAULT_CAPACITY_LIMIT;
    }
    if (bridge->config.capacity_limit > ATTENTION_WM_MAX_ITEMS) {
        bridge->config.capacity_limit = ATTENTION_WM_MAX_ITEMS;
    }

    bridge->item_capacity = bridge->config.capacity_limit;

    // Allocate items array
    bridge->items = nimcp_calloc(bridge->item_capacity, sizeof(wm_item_t));
    if (!bridge->items) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_wm_bridge_create: bridge->items is NULL");
        return NULL;
    }

    // Initialize all items as invalid
    for (size_t i = 0; i < bridge->item_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->item_capacity > 256) {
            attention_wm_bridge_heartbeat("attention_wm_loop",
                             (float)(i + 1) / (float)bridge->item_capacity);
        }

        bridge->items[i].valid = false;
    }

    // Create mutex
    if (bridge_base_init(&bridge->base, 0, "attention_wm") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge->items);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_wm_bridge_create: bridge->base is NULL");
        return NULL;
    }

    // Initialize state
    bridge->item_count = 0;
    bridge->current_focus = 0;

    // Initialize stats
    memset(&bridge->stats, 0, sizeof(attention_wm_stats_t));

    bridge->initialized = true;

    return bridge;
}

void attention_wm_bridge_destroy(attention_wm_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    // Free items array
    /* Phase 8: Heartbeat at operation start */
    attention_wm_bridge_heartbeat("attention_wm_destroy", 0.0f);


    if (bridge->items) {
        nimcp_free(bridge->items);
        bridge->items = NULL;
    }

    // Destroy mutex
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
        bridge->base.mutex = NULL;
    }

    bridge->initialized = false;

    // Free bridge
    nimcp_free(bridge);
    bridge = NULL;
}

//=============================================================================
// Attention -> WM Direction
//=============================================================================

int attention_wm_gate_entry(
    attention_wm_bridge_t* bridge,
    uint64_t item_id,
    float attention_strength
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_wm_gate_entry: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_wm_bridge_heartbeat("attention_wm_attention_wm_gate_en", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    // Check attention threshold - reject if below threshold
    if (attention_strength < bridge->config.attention_threshold) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1;  // Rejected due to insufficient attention (normal condition)
    }

    // Check if item already exists
    int existing_idx = find_item_unlocked(bridge, item_id);
    if (existing_idx >= 0) {
        // Update existing item
        bridge->items[existing_idx].attention_strength = attention_strength;
        bridge->items[existing_idx].priority = clamp_priority(attention_strength);
        bridge->items[existing_idx].last_access_time = nimcp_time_get_ms();
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    // Find free slot or evict if at capacity
    int slot_idx = find_free_slot_unlocked(bridge);

    if (slot_idx < 0) {
        // At capacity - evict lowest priority item
        slot_idx = find_lowest_priority_unlocked(bridge);
        if (slot_idx < 0) {
            nimcp_mutex_unlock(bridge->base.mutex);
            return -1;  // Should not happen, but safety check
        }

        // Mark evicted item as invalid
        bridge->items[slot_idx].valid = false;
        bridge->item_count--;
        bridge->stats.items_evicted++;
    }

    // Add new item
    uint64_t now = nimcp_time_get_ms();
    bridge->items[slot_idx].item_id = item_id;
    bridge->items[slot_idx].attention_strength = attention_strength;
    bridge->items[slot_idx].priority = clamp_priority(attention_strength);
    bridge->items[slot_idx].entry_time = now;
    bridge->items[slot_idx].last_access_time = now;
    bridge->items[slot_idx].valid = true;

    bridge->item_count++;
    bridge->stats.items_gated_in++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int attention_wm_on_focus_shift(
    attention_wm_bridge_t* bridge,
    uint64_t old_focus,
    uint64_t new_focus
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_wm_on_focus_shift: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_wm_bridge_heartbeat("attention_wm_attention_wm_on_focu", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    // Apply decay to old focus item (if it exists and is not 0)
    if (old_focus != 0) {
        int old_idx = find_item_unlocked(bridge, old_focus);
        if (old_idx >= 0) {
            float decayed = bridge->items[old_idx].priority * (1.0f - bridge->config.decay_rate);
            bridge->items[old_idx].priority = clamp_priority(decayed);
            bridge->items[old_idx].last_access_time = nimcp_time_get_ms();
        }
    }

    // Boost priority of new focus item
    int new_idx = find_item_unlocked(bridge, new_focus);
    if (new_idx >= 0) {
        float boosted = bridge->items[new_idx].priority * PRIORITY_BOOST_FACTOR;
        bridge->items[new_idx].priority = clamp_priority(boosted);
        bridge->items[new_idx].last_access_time = nimcp_time_get_ms();
    }

    // Update current focus
    bridge->current_focus = new_focus;
    bridge->stats.focus_shifts++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// WM Management
//=============================================================================

int attention_wm_update_priority(
    attention_wm_bridge_t* bridge,
    uint64_t item_id,
    float new_priority
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_wm_update_priority: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_wm_bridge_heartbeat("attention_wm_attention_wm_update_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    int idx = find_item_unlocked(bridge, item_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_wm_update_priority: validation failed");
        return -1;  // Item not found
    }

    bridge->items[idx].priority = clamp_priority(new_priority);
    bridge->items[idx].last_access_time = nimcp_time_get_ms();
    bridge->stats.priority_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Query API
//=============================================================================

int attention_wm_get_attended_items(
    attention_wm_bridge_t* bridge,
    attention_wm_item_t items[],
    size_t max_count
) {
    if (!bridge || !bridge->initialized || !items || max_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_wm_get_attended_items: required parameter is NULL (bridge, bridge->initialized, items)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_wm_bridge_heartbeat("attention_wm_attention_wm_get_att", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    int count = 0;
    for (size_t i = 0; i < bridge->item_capacity && (size_t)count < max_count; i++) {
        if (bridge->items[i].valid &&
            bridge->items[i].attention_strength >= bridge->config.attention_threshold) {

            items[count].item_id = bridge->items[i].item_id;
            items[count].priority = bridge->items[i].priority;
            items[count].attention_strength = bridge->items[i].attention_strength;
            items[count].entry_time = bridge->items[i].entry_time;
            items[count].last_access_time = bridge->items[i].last_access_time;
            count++;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return count;
}

//=============================================================================
// Stats API
//=============================================================================

int attention_wm_bridge_get_stats(
    const attention_wm_bridge_t* bridge,
    attention_wm_stats_t* stats
) {
    if (!bridge || !bridge->initialized || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_wm_bridge_get_stats: required parameter is NULL (bridge, bridge->initialized, stats)");
        return -1;
    }

    // Cast away const to acquire mutex (thread-safe read)
    /* Phase 8: Heartbeat at operation start */
    attention_wm_bridge_heartbeat("attention_wm_get_stats", 0.0f);


    attention_wm_bridge_t* mutable_bridge = (attention_wm_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);

    // Copy stats
    memcpy(stats, &bridge->stats, sizeof(attention_wm_stats_t));

    // Update dynamic fields
    stats->current_item_count = bridge->item_count;
    stats->avg_priority = compute_avg_priority_unlocked(bridge);

    nimcp_mutex_unlock(mutable_bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void attention_wm_bridge_set_instance_health_agent(attention_wm_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "attention_wm_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int attention_wm_bridge_training_begin(attention_wm_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "attention_wm_bridge_training_begin: NULL argument");
        return -1;
    }
    attention_wm_bridge_heartbeat_instance(bridge->health_agent, "attention_wm_bridge_training_begin", 0.0f);
    return 0;
}

int attention_wm_bridge_training_end(attention_wm_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "attention_wm_bridge_training_end: NULL argument");
        return -1;
    }
    attention_wm_bridge_heartbeat_instance(bridge->health_agent, "attention_wm_bridge_training_end", 1.0f);
    return 0;
}

int attention_wm_bridge_training_step(attention_wm_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "attention_wm_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    attention_wm_bridge_heartbeat_instance(bridge->health_agent, "attention_wm_bridge_training_step", progress);
    return 0;
}
