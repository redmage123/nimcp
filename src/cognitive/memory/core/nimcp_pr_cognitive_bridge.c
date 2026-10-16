//=============================================================================
// nimcp_pr_cognitive_bridge.c - Prime Resonant Cognitive Bridge Implementation
//=============================================================================
/**
 * @file nimcp_pr_cognitive_bridge.c
 * @brief Implementation of the PR memory-cognitive integration bridge
 */

#include "cognitive/memory/core/nimcp_pr_cognitive_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "glial/myelin_sheath/nimcp_myelin_math.h"

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pr_cognitive_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_pr_cognitive_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_pr_cognitive_bridge_mesh_registry = NULL;

nimcp_error_t pr_cognitive_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_pr_cognitive_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "pr_cognitive_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_MEMORY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "pr_cognitive_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_pr_cognitive_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_pr_cognitive_bridge_mesh_registry = registry;
    return err;
}

void pr_cognitive_bridge_mesh_unregister(void) {
    if (g_pr_cognitive_bridge_mesh_registry && g_pr_cognitive_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_pr_cognitive_bridge_mesh_registry, g_pr_cognitive_bridge_mesh_id);
        g_pr_cognitive_bridge_mesh_id = 0;
        g_pr_cognitive_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from pr_cognitive_bridge module (instance-level) */
static inline void pr_cognitive_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_pr_cognitive_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_pr_cognitive_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_pr_cognitive_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PR_COGNITIVE_BRIDGE"

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal bridge structure
 */
struct pr_cognitive_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    // Configuration
    pr_cognitive_config_t config;

    // Core memory system
    z_ladder_t z_ladder;

    // Cognitive links
    pr_attention_link_t attention;
    pr_emotion_link_t emotion;
    pr_executive_link_t executive;
    pr_wm_link_t wm;
    pr_hub_link_t hub;

    // Callbacks
    pr_attention_modulate_cb_t attention_cb;
    void* attention_cb_data;

    pr_emotion_modulate_cb_t emotion_cb;
    void* emotion_cb_data;

    pr_executive_gate_cb_t executive_cb;
    void* executive_cb_data;

    // Statistics
    pr_cognitive_stats_t stats;

    // State
    bool initialized;
    uint64_t creation_time_ms;

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(pr_cognitive_bridge, struct pr_cognitive_bridge_struct)

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Initialize attention link
 */
static void init_attention_link(pr_attention_link_t* link) {
    memset(link, 0, sizeof(*link));
    link->state = PR_COG_LINK_DISCONNECTED;
    link->focus_weight = 0.5f;
    link->salience_gain = PR_COG_ATTENTION_STRENGTH;
    link->filter_threshold = 0.3f;
}

/**
 * @brief Initialize emotion link
 */
static void init_emotion_link(pr_emotion_link_t* link) {
    memset(link, 0, sizeof(*link));
    link->state = PR_COG_LINK_DISCONNECTED;
    link->current_valence = 0.0f;
    link->current_arousal = 0.0f;
    link->valence_sensitivity = PR_COG_EMOTION_STRENGTH;
    link->arousal_threshold = PR_COG_EMOTION_AROUSAL_THRESHOLD;
}

/**
 * @brief Initialize executive link
 */
static void init_executive_link(pr_executive_link_t* link) {
    memset(link, 0, sizeof(*link));
    link->state = PR_COG_LINK_DISCONNECTED;
    link->encoding_gate = 1.0f;  // Open by default
    link->retrieval_gate = 1.0f;
    link->inhibition_level = 0.0f;
    link->encoding_permitted = true;
    link->retrieval_permitted = true;
}

/**
 * @brief Initialize working memory link
 */
static void init_wm_link(pr_wm_link_t* link, uint32_t max_slots) {
    memset(link, 0, sizeof(*link));
    link->state = PR_COG_LINK_DISCONNECTED;
    link->max_slots = max_slots;
    for (uint32_t i = 0; i < PR_COG_MAX_WM_SLOTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_COG_MAX_WM_SLOTS > 256) {
            pr_cognitive_bridge_heartbeat("pr_cognitive_loop",
                             (float)(i + 1) / (float)PR_COG_MAX_WM_SLOTS);
        }

        link->slots[i].node_id = 0;
        link->slots[i].wm_slot_index = i;
        link->slots[i].is_synced = false;
    }
}

/**
 * @brief Initialize hub link
 */
static void init_hub_link(pr_hub_link_t* link) {
    memset(link, 0, sizeof(*link));
    link->state = PR_COG_LINK_DISCONNECTED;
    link->hub = NULL;
    link->module_id = PR_COG_BRIDGE_MODULE_ID;
}

/**
 * @brief Update gate permissions based on levels and thresholds
 */
static void update_gate_permissions(
    pr_executive_link_t* link,
    float encoding_threshold,
    float retrieval_threshold
) {
    link->encoding_permitted =
        (link->encoding_gate >= encoding_threshold) &&
        (link->inhibition_level < PR_COG_INHIBITION_THRESHOLD);

    link->retrieval_permitted =
        (link->retrieval_gate >= retrieval_threshold) &&
        (link->inhibition_level < PR_COG_INHIBITION_THRESHOLD);
}

/**
 * @brief Hub event callback handler
 */
static int hub_event_handler(
    const cognitive_event_data_t* event,
    void* user_data
) {
    pr_cognitive_bridge_t bridge = (pr_cognitive_bridge_t)user_data;
    if (!bridge || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hub_event_handler: required parameter is NULL (bridge, event)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->hub.events_received++;

    // Process event based on type
    switch (event->event_type) {
        case COG_EVENT_ATTENTION_SHIFT:
            // Attention shifted - update our attention state
            if (bridge->attention.state == PR_COG_LINK_CONNECTED) {
                // Could extract attention parameters from payload
                bridge->attention.last_update_ms = get_current_time_ms();
            }
            break;

        case COG_EVENT_EMOTION_UPDATE:
            // Emotion updated - update our emotion state
            if (bridge->emotion.state == PR_COG_LINK_CONNECTED) {
                // Could extract emotion parameters from payload
                bridge->emotion.last_update_ms = get_current_time_ms();
            }
            break;

        case COG_EVENT_MEMORY_ACCESS:
            // Another module accessed memory
            break;

        default:
            break;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Configuration Functions
//=============================================================================

pr_cognitive_config_t pr_cognitive_bridge_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_default_config", 0.0f);


    pr_cognitive_config_t config = {
        // Attention
        .enable_attention_link = true,
        .attention_strength = PR_COG_ATTENTION_STRENGTH,
        .salience_decay_rate = 0.05f,  // 5% per second

        // Emotion
        .enable_emotion_link = true,
        .emotion_strength = PR_COG_EMOTION_STRENGTH,
        .arousal_boost_factor = 0.3f,

        // Executive
        .enable_executive_link = true,
        .encoding_threshold = PR_COG_EXECUTIVE_THRESHOLD,
        .retrieval_threshold = PR_COG_EXECUTIVE_THRESHOLD,

        // Working memory
        .enable_wm_sync = true,
        .max_wm_slots = PR_COG_MAX_WM_SLOTS,
        .wm_sync_interval_ms = 50,

        // Hub
        .enable_hub_events = true,
        .subscribe_attention_events = true,
        .subscribe_emotion_events = true,

        // General
        .update_interval_ms = PR_COG_DEFAULT_UPDATE_MS
    };
    return config;
}

bool pr_cognitive_bridge_validate_config(const pr_cognitive_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_cognitive_bridge_validate_config: config is NULL");
        return false;
    }

    // Validate ranges
    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_validate_config", 0.0f);


    if (config->attention_strength < 0.0f || config->attention_strength > 1.0f) {
        return false;
    }
    if (config->emotion_strength < 0.0f || config->emotion_strength > 1.0f) {
        return false;
    }
    if (config->encoding_threshold < 0.0f || config->encoding_threshold > 1.0f) {
        return false;
    }
    if (config->retrieval_threshold < 0.0f || config->retrieval_threshold > 1.0f) {
        return false;
    }
    if (config->max_wm_slots > PR_COG_MAX_WM_SLOTS) {
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

pr_cognitive_bridge_t pr_cognitive_bridge_create(
    const pr_cognitive_config_t* config,
    z_ladder_t z_ladder
) {
    if (!z_ladder) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "z_ladder is NULL");

        return NULL;

    }

    // Use default config if not provided
    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_create", 0.0f);


    pr_cognitive_config_t cfg;
    if (config) {
        if (!pr_cognitive_bridge_validate_config(config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_cognitive_bridge_create: pr_cognitive_bridge_validate_config is NULL");
            return NULL;
        }
        cfg = *config;
    } else {
        cfg = pr_cognitive_bridge_default_config();
    }

    // Allocate bridge
    pr_cognitive_bridge_t bridge = nimcp_calloc(1, sizeof(struct pr_cognitive_bridge_struct));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    // Initialize base bridge infrastructure
    if (bridge_base_init(&bridge->base, 0, "pr_cognitive") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "pr_cognitive_bridge_create: validation failed");
        return NULL;
    }

    // Store configuration and z_ladder
    bridge->config = cfg;
    bridge->z_ladder = z_ladder;

    // Initialize links
    init_attention_link(&bridge->attention);
    init_emotion_link(&bridge->emotion);
    init_executive_link(&bridge->executive);
    init_wm_link(&bridge->wm, cfg.max_wm_slots);
    init_hub_link(&bridge->hub);

    // Initialize statistics
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    // Set creation time
    bridge->creation_time_ms = get_current_time_ms();
    bridge->initialized = true;

    return bridge;
}

void pr_cognitive_bridge_destroy(pr_cognitive_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "pr_cognitive");

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_destroy", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    // Disconnect all links
    if (bridge->hub.state == PR_COG_LINK_CONNECTED && bridge->hub.hub) {
        cognitive_hub_unregister_module(bridge->hub.hub, bridge->hub.module_id);
        bridge->hub.state = PR_COG_LINK_DISCONNECTED;
        bridge->hub.hub = NULL;
    }

    bridge->attention.state = PR_COG_LINK_DISCONNECTED;
    bridge->emotion.state = PR_COG_LINK_DISCONNECTED;
    bridge->executive.state = PR_COG_LINK_DISCONNECTED;
    bridge->wm.state = PR_COG_LINK_DISCONNECTED;

    bridge->initialized = false;

    nimcp_mutex_unlock(bridge->base.mutex);
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

pr_cognitive_error_t pr_cognitive_bridge_reset(pr_cognitive_bridge_t bridge) {
    if (!bridge) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    // Disconnect hub if connected
    if (bridge->hub.state == PR_COG_LINK_CONNECTED && bridge->hub.hub) {
        cognitive_hub_unregister_module(bridge->hub.hub, bridge->hub.module_id);
    }

    // Reset all links
    init_attention_link(&bridge->attention);
    init_emotion_link(&bridge->emotion);
    init_executive_link(&bridge->executive);
    init_wm_link(&bridge->wm, bridge->config.max_wm_slots);
    init_hub_link(&bridge->hub);

    // Reset statistics
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

//=============================================================================
// Attention Integration
//=============================================================================

pr_cognitive_error_t pr_cognitive_bridge_connect_attention(
    pr_cognitive_bridge_t bridge,
    void* attention_context
) {
    if (!bridge) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_connect_attention", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->attention.state == PR_COG_LINK_CONNECTED) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_COG_ERROR_ALREADY_CONNECTED;
    }

    bridge->attention.attention_context = attention_context;
    bridge->attention.state = PR_COG_LINK_CONNECTED;
    bridge->attention.last_update_ms = get_current_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

pr_cognitive_error_t pr_cognitive_bridge_disconnect_attention(
    pr_cognitive_bridge_t bridge
) {
    if (!bridge) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_disconnect_attention", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->attention.state = PR_COG_LINK_DISCONNECTED;
    bridge->attention.attention_context = NULL;

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

pr_cognitive_error_t pr_cognitive_bridge_update_attention(
    pr_cognitive_bridge_t bridge,
    float focus_weight,
    float filter_threshold
) {
    if (!bridge) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_update_attention", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->attention.state != PR_COG_LINK_CONNECTED) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_COG_ERROR_NOT_CONNECTED;
    }

    bridge->attention.focus_weight = nimcp_myelin_clamp(focus_weight, 0.0f, 1.0f);
    bridge->attention.filter_threshold = nimcp_myelin_clamp(filter_threshold, 0.0f, 1.0f);
    bridge->attention.last_update_ms = get_current_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

float pr_cognitive_bridge_apply_attention_boost(
    pr_cognitive_bridge_t bridge,
    uint64_t node_id,
    float attention_weight
) {
    if (!bridge || !bridge->initialized) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_apply_attention_boos", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->attention.state != PR_COG_LINK_CONNECTED) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1.0f;
    }

    // Find node in z_ladder
    pr_memory_node_t* node = z_ladder_find(bridge->z_ladder, node_id);
    if (!node) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1.0f;
    }

    // Get current state
    nimcp_quaternion_t state = pr_memory_node_get_state(node);
    float old_salience = state.y;

    // Calculate new salience with attention boost
    float attention_boost = attention_weight * bridge->attention.salience_gain;
    float new_salience = nimcp_myelin_clamp(
        old_salience + attention_boost,
        PR_COG_MIN_SALIENCE,
        PR_COG_MAX_SALIENCE
    );

    // Update node state
    state.y = new_salience;
    pr_memory_node_update_state(node, state);

    // Update statistics
    bridge->stats.encodings_modulated++;
    bridge->stats.avg_salience_modulation =
        (bridge->stats.avg_salience_modulation * (bridge->stats.encodings_modulated - 1) +
         (new_salience - old_salience)) / bridge->stats.encodings_modulated;

    // Invoke callback if registered
    if (bridge->attention_cb) {
        nimcp_mutex_unlock(bridge->base.mutex);
        bridge->attention_cb(bridge, node_id, old_salience, new_salience,
                            bridge->attention_cb_data);
        return new_salience;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return new_salience;
}

pr_cognitive_error_t pr_cognitive_bridge_get_attention_state(
    pr_cognitive_bridge_t bridge,
    pr_attention_link_t* link_state
) {
    if (!bridge || !link_state) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_get_attention_state", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *link_state = bridge->attention;
    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

//=============================================================================
// Emotion Integration
//=============================================================================

pr_cognitive_error_t pr_cognitive_bridge_connect_emotion(
    pr_cognitive_bridge_t bridge,
    void* emotion_context
) {
    if (!bridge) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_connect_emotion", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->emotion.state == PR_COG_LINK_CONNECTED) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_COG_ERROR_ALREADY_CONNECTED;
    }

    bridge->emotion.emotion_context = emotion_context;
    bridge->emotion.state = PR_COG_LINK_CONNECTED;
    bridge->emotion.last_update_ms = get_current_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

pr_cognitive_error_t pr_cognitive_bridge_disconnect_emotion(
    pr_cognitive_bridge_t bridge
) {
    if (!bridge) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_disconnect_emotion", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->emotion.state = PR_COG_LINK_DISCONNECTED;
    bridge->emotion.emotion_context = NULL;

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

pr_cognitive_error_t pr_cognitive_bridge_update_emotion(
    pr_cognitive_bridge_t bridge,
    float valence,
    float arousal
) {
    if (!bridge) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_update_emotion", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->emotion.state != PR_COG_LINK_CONNECTED) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_COG_ERROR_NOT_CONNECTED;
    }

    bridge->emotion.current_valence = nimcp_myelin_clamp(valence, -1.0f, 1.0f);
    bridge->emotion.current_arousal = nimcp_myelin_clamp(arousal, 0.0f, 1.0f);
    bridge->emotion.last_update_ms = get_current_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

pr_cognitive_error_t pr_cognitive_bridge_apply_emotion_tag(
    pr_cognitive_bridge_t bridge,
    uint64_t node_id,
    float valence,
    float arousal
) {
    if (!bridge) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_apply_emotion_tag", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->emotion.state != PR_COG_LINK_CONNECTED) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_COG_ERROR_NOT_CONNECTED;
    }

    // Find node
    pr_memory_node_t* node = z_ladder_find(bridge->z_ladder, node_id);
    if (!node) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_COG_ERROR_NULL_POINTER;
    }

    // Get current state
    nimcp_quaternion_t state = pr_memory_node_get_state(node);

    // Apply valence (quat.x)
    float clamped_valence = nimcp_myelin_clamp(valence, -1.0f, 1.0f);
    state.x = clamped_valence * bridge->emotion.valence_sensitivity;

    // Apply arousal boost to consolidation (quat.w) if above threshold
    float clamped_arousal = nimcp_myelin_clamp(arousal, 0.0f, 1.0f);
    if (clamped_arousal >= bridge->emotion.arousal_threshold) {
        float arousal_boost = clamped_arousal * bridge->config.arousal_boost_factor;
        state.w = nimcp_myelin_clamp(state.w + arousal_boost, 0.0f, 1.0f);
        bridge->stats.emotional_boosts++;
    }

    // Update node state
    pr_memory_node_update_state(node, state);

    // Update statistics
    bridge->stats.avg_valence_modulation =
        (bridge->stats.avg_valence_modulation * bridge->stats.emotional_boosts +
         fabsf(clamped_valence)) / (bridge->stats.emotional_boosts + 1);

    // Invoke callback if registered
    if (bridge->emotion_cb) {
        nimcp_mutex_unlock(bridge->base.mutex);
        bridge->emotion_cb(bridge, node_id, clamped_valence, clamped_arousal,
                          bridge->emotion_cb_data);
        return PR_COG_SUCCESS;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return PR_COG_SUCCESS;
}

pr_cognitive_error_t pr_cognitive_bridge_get_emotion_state(
    pr_cognitive_bridge_t bridge,
    pr_emotion_link_t* link_state
) {
    if (!bridge || !link_state) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_get_emotion_state", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *link_state = bridge->emotion;
    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

//=============================================================================
// Executive Integration
//=============================================================================

pr_cognitive_error_t pr_cognitive_bridge_connect_executive(
    pr_cognitive_bridge_t bridge,
    void* executive_context
) {
    if (!bridge) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_connect_executive", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->executive.state == PR_COG_LINK_CONNECTED) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_COG_ERROR_ALREADY_CONNECTED;
    }

    bridge->executive.executive_context = executive_context;
    bridge->executive.state = PR_COG_LINK_CONNECTED;
    bridge->executive.last_update_ms = get_current_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

pr_cognitive_error_t pr_cognitive_bridge_disconnect_executive(
    pr_cognitive_bridge_t bridge
) {
    if (!bridge) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_disconnect_executive", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->executive.state = PR_COG_LINK_DISCONNECTED;
    bridge->executive.executive_context = NULL;

    // Reset gates to open when disconnected
    bridge->executive.encoding_gate = 1.0f;
    bridge->executive.retrieval_gate = 1.0f;
    bridge->executive.inhibition_level = 0.0f;
    bridge->executive.encoding_permitted = true;
    bridge->executive.retrieval_permitted = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

pr_cognitive_error_t pr_cognitive_bridge_update_executive(
    pr_cognitive_bridge_t bridge,
    float encoding_gate,
    float retrieval_gate,
    float inhibition
) {
    if (!bridge) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_update_executive", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->executive.state != PR_COG_LINK_CONNECTED) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_COG_ERROR_NOT_CONNECTED;
    }

    bridge->executive.encoding_gate = nimcp_myelin_clamp(encoding_gate, 0.0f, 1.0f);
    bridge->executive.retrieval_gate = nimcp_myelin_clamp(retrieval_gate, 0.0f, 1.0f);
    bridge->executive.inhibition_level = nimcp_myelin_clamp(inhibition, 0.0f, 1.0f);
    bridge->executive.last_update_ms = get_current_time_ms();

    // Update permissions
    update_gate_permissions(
        &bridge->executive,
        bridge->config.encoding_threshold,
        bridge->config.retrieval_threshold
    );

    // Update statistics
    bridge->stats.avg_executive_gate =
        (bridge->stats.avg_executive_gate * bridge->stats.update_count +
         (encoding_gate + retrieval_gate) / 2.0f) / (bridge->stats.update_count + 1);

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

bool pr_cognitive_bridge_encoding_permitted(pr_cognitive_bridge_t bridge) {
    if (!bridge || !bridge->initialized) return true;  // Default to permitted

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_encoding_permitted", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bool permitted = true;
    if (bridge->executive.state == PR_COG_LINK_CONNECTED) {
        permitted = bridge->executive.encoding_permitted;
        if (!permitted) {
            bridge->stats.executive_blocks++;

            // Invoke callback if registered
            if (bridge->executive_cb) {
                nimcp_mutex_unlock(bridge->base.mutex);
                bridge->executive_cb(bridge, true, permitted,
                                    bridge->executive.encoding_gate,
                                    bridge->executive_cb_data);
                return permitted;
            }
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return permitted;
}

bool pr_cognitive_bridge_retrieval_permitted(pr_cognitive_bridge_t bridge) {
    if (!bridge || !bridge->initialized) return true;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_retrieval_permitted", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bool permitted = true;
    if (bridge->executive.state == PR_COG_LINK_CONNECTED) {
        permitted = bridge->executive.retrieval_permitted;
        if (!permitted) {
            bridge->stats.executive_blocks++;

            if (bridge->executive_cb) {
                nimcp_mutex_unlock(bridge->base.mutex);
                bridge->executive_cb(bridge, false, permitted,
                                    bridge->executive.retrieval_gate,
                                    bridge->executive_cb_data);
                return permitted;
            }
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return permitted;
}

pr_cognitive_error_t pr_cognitive_bridge_get_executive_state(
    pr_cognitive_bridge_t bridge,
    pr_executive_link_t* link_state
) {
    if (!bridge || !link_state) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_get_executive_state", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *link_state = bridge->executive;
    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

//=============================================================================
// Working Memory Integration
//=============================================================================

pr_cognitive_error_t pr_cognitive_bridge_sync_working_memory(
    pr_cognitive_bridge_t bridge,
    void* wm_context
) {
    if (!bridge) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_sync_working_memory", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    // Connect WM if not already
    if (bridge->wm.state == PR_COG_LINK_DISCONNECTED) {
        bridge->wm.wm_context = wm_context;
        bridge->wm.state = PR_COG_LINK_CONNECTED;
    }

    // Get Z0 nodes from z_ladder
    pr_memory_node_t* z0_nodes[PR_COG_MAX_WM_SLOTS];
    size_t z0_count = 0;
    z_ladder_get_nodes(bridge->z_ladder, PR_MEMORY_TIER_Z0,
                       z0_nodes, bridge->config.max_wm_slots, &z0_count);

    // Sync slots with Z0 nodes
    uint32_t active = 0;
    for (uint32_t i = 0; i < bridge->config.max_wm_slots && i < z0_count; i++) {
        if (z0_nodes[i]) {
            bridge->wm.slots[i].node_id = pr_memory_node_get_id(z0_nodes[i]);
            bridge->wm.slots[i].activity_level =
                pr_memory_node_get_state(z0_nodes[i]).z;  // accessibility as activity
            bridge->wm.slots[i].salience =
                pr_memory_node_get_state(z0_nodes[i]).y;
            bridge->wm.slots[i].is_synced = true;
            bridge->wm.slots[i].last_sync_ms = get_current_time_ms();
            active++;
        } else {
            bridge->wm.slots[i].node_id = 0;
            bridge->wm.slots[i].is_synced = false;
        }
    }

    // Clear remaining slots
    for (uint32_t i = (uint32_t)z0_count; i < bridge->config.max_wm_slots; i++) {
        bridge->wm.slots[i].node_id = 0;
        bridge->wm.slots[i].is_synced = false;
    }

    bridge->wm.active_slots = active;
    bridge->wm.last_sync_ms = get_current_time_ms();
    bridge->stats.wm_syncs++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

pr_cognitive_error_t pr_cognitive_bridge_map_wm_slot(
    pr_cognitive_bridge_t bridge,
    uint32_t wm_slot_index,
    uint64_t node_id
) {
    if (!bridge) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_map_wm_slot", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (wm_slot_index >= bridge->config.max_wm_slots) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_COG_ERROR_CAPACITY;
    }

    // Verify node exists
    pr_memory_node_t* node = z_ladder_find(bridge->z_ladder, node_id);
    if (!node) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_COG_ERROR_NULL_POINTER;
    }

    // Map slot
    bridge->wm.slots[wm_slot_index].node_id = node_id;
    bridge->wm.slots[wm_slot_index].is_synced = true;
    bridge->wm.slots[wm_slot_index].last_sync_ms = get_current_time_ms();

    // Update active count
    uint32_t active = 0;
    for (uint32_t i = 0; i < bridge->config.max_wm_slots; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.max_wm_slots > 256) {
            pr_cognitive_bridge_heartbeat("pr_cognitive_loop",
                             (float)(i + 1) / (float)bridge->config.max_wm_slots);
        }

        if (bridge->wm.slots[i].node_id != 0) active++;
    }
    bridge->wm.active_slots = active;

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

pr_cognitive_error_t pr_cognitive_bridge_unmap_wm_slot(
    pr_cognitive_bridge_t bridge,
    uint32_t wm_slot_index
) {
    if (!bridge) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_unmap_wm_slot", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (wm_slot_index >= bridge->config.max_wm_slots) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_COG_ERROR_CAPACITY;
    }

    bridge->wm.slots[wm_slot_index].node_id = 0;
    bridge->wm.slots[wm_slot_index].is_synced = false;

    // Update active count
    uint32_t active = 0;
    for (uint32_t i = 0; i < bridge->config.max_wm_slots; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.max_wm_slots > 256) {
            pr_cognitive_bridge_heartbeat("pr_cognitive_loop",
                             (float)(i + 1) / (float)bridge->config.max_wm_slots);
        }

        if (bridge->wm.slots[i].node_id != 0) active++;
    }
    bridge->wm.active_slots = active;

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

pr_cognitive_error_t pr_cognitive_bridge_get_wm_state(
    pr_cognitive_bridge_t bridge,
    pr_wm_link_t* link_state
) {
    if (!bridge || !link_state) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_get_wm_state", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *link_state = bridge->wm;
    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

//=============================================================================
// Cognitive Hub Integration
//=============================================================================

pr_cognitive_error_t pr_cognitive_bridge_connect_hub(
    pr_cognitive_bridge_t bridge,
    cognitive_integration_hub_t hub
) {
    if (!bridge || !hub) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_connect_hub", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->hub.state == PR_COG_LINK_CONNECTED) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_COG_ERROR_ALREADY_CONNECTED;
    }

    // Register with hub
    int ret = cognitive_hub_register_module(
        hub,
        PR_COG_BRIDGE_MODULE_ID,
        COG_CATEGORY_MEMORY,
        "PR_Cognitive_Bridge",
        bridge
    );
    if (ret != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_COG_ERROR_HUB_FAILED;
    }

    bridge->hub.hub = hub;
    bridge->hub.state = PR_COG_LINK_CONNECTED;

    // Subscribe to events if configured
    if (bridge->config.subscribe_attention_events) {
        cognitive_hub_subscribe(hub, PR_COG_BRIDGE_MODULE_ID,
                               COG_EVENT_ATTENTION_SHIFT,
                               hub_event_handler, bridge);
        bridge->hub.subscribed_attention = true;
    }

    if (bridge->config.subscribe_emotion_events) {
        cognitive_hub_subscribe(hub, PR_COG_BRIDGE_MODULE_ID,
                               COG_EVENT_EMOTION_UPDATE,
                               hub_event_handler, bridge);
        bridge->hub.subscribed_emotion = true;
    }

    // Subscribe to memory events (always)
    cognitive_hub_subscribe(hub, PR_COG_BRIDGE_MODULE_ID,
                           COG_EVENT_MEMORY_ACCESS,
                           hub_event_handler, bridge);
    bridge->hub.subscribed_memory = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

pr_cognitive_error_t pr_cognitive_bridge_disconnect_hub(
    pr_cognitive_bridge_t bridge
) {
    if (!bridge) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_disconnect_hub", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->hub.state != PR_COG_LINK_CONNECTED || !bridge->hub.hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_COG_ERROR_NOT_CONNECTED;
    }

    // Unsubscribe from events
    if (bridge->hub.subscribed_attention) {
        cognitive_hub_unsubscribe(bridge->hub.hub, PR_COG_BRIDGE_MODULE_ID,
                                  COG_EVENT_ATTENTION_SHIFT);
    }
    if (bridge->hub.subscribed_emotion) {
        cognitive_hub_unsubscribe(bridge->hub.hub, PR_COG_BRIDGE_MODULE_ID,
                                  COG_EVENT_EMOTION_UPDATE);
    }
    if (bridge->hub.subscribed_memory) {
        cognitive_hub_unsubscribe(bridge->hub.hub, PR_COG_BRIDGE_MODULE_ID,
                                  COG_EVENT_MEMORY_ACCESS);
    }

    // Unregister module
    cognitive_hub_unregister_module(bridge->hub.hub, PR_COG_BRIDGE_MODULE_ID);

    bridge->hub.hub = NULL;
    bridge->hub.state = PR_COG_LINK_DISCONNECTED;
    bridge->hub.subscribed_attention = false;
    bridge->hub.subscribed_emotion = false;
    bridge->hub.subscribed_memory = false;

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

pr_cognitive_error_t pr_cognitive_bridge_broadcast_memory_event(
    pr_cognitive_bridge_t bridge,
    pr_memory_event_type_t event_type,
    uint64_t node_id,
    pr_memory_tier_t tier
) {
    if (!bridge) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_broadcast_memory_eve", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->hub.state != PR_COG_LINK_CONNECTED || !bridge->hub.hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_COG_ERROR_NOT_CONNECTED;
    }

    // Build event payload
    pr_memory_event_payload_t payload = {
        .node_id = node_id,
        .event_type = event_type,
        .tier = tier,
        .timestamp_ms = get_current_time_ms()
    };

    // Get node state if node exists
    pr_memory_node_t* node = z_ladder_find(bridge->z_ladder, node_id);
    if (node) {
        payload.state = pr_memory_node_get_state(node);
        payload.strength = node->current_strength;
    } else {
        payload.state = (nimcp_quaternion_t){1.0f, 0.0f, 0.0f, 0.0f};
        payload.strength = 0.0f;
    }

    // Map our event type to cognitive hub event type
    cognitive_event_type_t cog_event = COG_EVENT_MEMORY_ACCESS;
    switch (event_type) {
        case PR_MEM_EVENT_ENCODED:
        case PR_MEM_EVENT_UPDATED:
            cog_event = COG_EVENT_INPUT_RECEIVED;
            break;
        case PR_MEM_EVENT_RETRIEVED:
            cog_event = COG_EVENT_MEMORY_ACCESS;
            break;
        case PR_MEM_EVENT_CONSOLIDATED:
            cog_event = COG_EVENT_CONSOLIDATION;
            break;
        case PR_MEM_EVENT_FORGOTTEN:
            cog_event = COG_EVENT_STATE_CHANGE;
            break;
        default:
            cog_event = COG_EVENT_MEMORY_ACCESS;
    }

    // Build cognitive event data
    cognitive_event_data_t event_data = {
        .event_type = cog_event,
        .source_module_id = PR_COG_BRIDGE_MODULE_ID,
        .timestamp = payload.timestamp_ms * 1000,  // Convert to microseconds
        .priority = COG_PRIORITY_NORMAL,
        .payload = &payload,
        .payload_size = sizeof(payload)
    };

    // Publish event
    int ret = cognitive_hub_publish(
        bridge->hub.hub,
        PR_COG_BRIDGE_MODULE_ID,
        cog_event,
        &event_data
    );

    if (ret == 0) {
        bridge->hub.events_published++;
        bridge->stats.events_broadcast++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return (ret == 0) ? PR_COG_SUCCESS : PR_COG_ERROR_HUB_FAILED;
}

pr_cognitive_error_t pr_cognitive_bridge_get_hub_state(
    pr_cognitive_bridge_t bridge,
    pr_hub_link_t* link_state
) {
    if (!bridge || !link_state) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_get_hub_state", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *link_state = bridge->hub;
    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

//=============================================================================
// Update and Maintenance
//=============================================================================

pr_cognitive_error_t pr_cognitive_bridge_update(pr_cognitive_bridge_t bridge) {
    if (!bridge) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_update", 0.0f);


    uint64_t start_time = get_current_time_ms();

    nimcp_mutex_lock(bridge->base.mutex);

    // Update attention link if connected
    if (bridge->attention.state == PR_COG_LINK_CONNECTED) {
        // Attention update is driven by external calls
        // Here we just refresh the timestamp
        bridge->attention.last_update_ms = start_time;
    }

    // Update emotion link if connected
    if (bridge->emotion.state == PR_COG_LINK_CONNECTED) {
        bridge->emotion.last_update_ms = start_time;
    }

    // Update executive link if connected
    if (bridge->executive.state == PR_COG_LINK_CONNECTED) {
        bridge->executive.last_update_ms = start_time;
    }

    // Sync working memory if enabled and due
    if (bridge->config.enable_wm_sync &&
        bridge->wm.state == PR_COG_LINK_CONNECTED) {
        uint64_t since_last = start_time - bridge->wm.last_sync_ms;
        if (since_last >= bridge->config.wm_sync_interval_ms) {
            // Perform sync (without releasing lock to avoid race)
            pr_memory_node_t* z0_nodes[PR_COG_MAX_WM_SLOTS];
            size_t z0_count = 0;
            z_ladder_get_nodes(bridge->z_ladder, PR_MEMORY_TIER_Z0,
                              z0_nodes, bridge->config.max_wm_slots, &z0_count);

            uint32_t active = 0;
            for (uint32_t i = 0; i < bridge->config.max_wm_slots && i < z0_count; i++) {
                if (z0_nodes[i]) {
                    bridge->wm.slots[i].node_id = pr_memory_node_get_id(z0_nodes[i]);
                    bridge->wm.slots[i].activity_level =
                        pr_memory_node_get_state(z0_nodes[i]).z;
                    bridge->wm.slots[i].is_synced = true;
                    active++;
                }
            }
            bridge->wm.active_slots = active;
            bridge->wm.last_sync_ms = start_time;
            bridge->stats.wm_syncs++;
        }
    }

    // Update statistics
    uint64_t elapsed = get_current_time_ms() - start_time;
    bridge->stats.total_update_time_us += elapsed * 1000;
    bridge->stats.update_count++;
    bridge->stats.last_update_ms = start_time;

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

pr_cognitive_error_t pr_cognitive_bridge_apply_salience_decay(
    pr_cognitive_bridge_t bridge,
    float dt_seconds
) {
    if (!bridge) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;
    if (dt_seconds <= 0.0f) return PR_COG_SUCCESS;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_apply_salience_decay", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    // Get all Z0 and Z1 nodes
    pr_memory_node_t* nodes[256];
    size_t count = 0;

    // Decay Z0 (working memory)
    z_ladder_get_nodes(bridge->z_ladder, PR_MEMORY_TIER_Z0, nodes, 256, &count);
    float decay_factor = expf(-bridge->config.salience_decay_rate * dt_seconds);

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_cognitive_bridge_heartbeat("pr_cognitive_loop",
                             (float)(i + 1) / (float)count);
        }

        if (nodes[i]) {
            nimcp_quaternion_t state = pr_memory_node_get_state(nodes[i]);
            state.y *= decay_factor;  // Decay salience
            if (state.y < PR_COG_MIN_SALIENCE) state.y = PR_COG_MIN_SALIENCE;
            pr_memory_node_update_state(nodes[i], state);
        }
    }

    // Decay Z1 (short-term) with lower rate
    z_ladder_get_nodes(bridge->z_ladder, PR_MEMORY_TIER_Z1, nodes, 256, &count);
    decay_factor = expf(-bridge->config.salience_decay_rate * dt_seconds * 0.5f);

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_cognitive_bridge_heartbeat("pr_cognitive_loop",
                             (float)(i + 1) / (float)count);
        }

        if (nodes[i]) {
            nimcp_quaternion_t state = pr_memory_node_get_state(nodes[i]);
            state.y *= decay_factor;
            if (state.y < PR_COG_MIN_SALIENCE) state.y = PR_COG_MIN_SALIENCE;
            pr_memory_node_update_state(nodes[i], state);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

//=============================================================================
// Callback Registration
//=============================================================================

pr_cognitive_error_t pr_cognitive_bridge_set_attention_callback(
    pr_cognitive_bridge_t bridge,
    pr_attention_modulate_cb_t callback,
    void* user_data
) {
    if (!bridge) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_set_attention_callba", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_cb = callback;
    bridge->attention_cb_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

pr_cognitive_error_t pr_cognitive_bridge_set_emotion_callback(
    pr_cognitive_bridge_t bridge,
    pr_emotion_modulate_cb_t callback,
    void* user_data
) {
    if (!bridge) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_set_emotion_callback", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->emotion_cb = callback;
    bridge->emotion_cb_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

pr_cognitive_error_t pr_cognitive_bridge_set_executive_callback(
    pr_cognitive_bridge_t bridge,
    pr_executive_gate_cb_t callback,
    void* user_data
) {
    if (!bridge) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_set_executive_callba", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->executive_cb = callback;
    bridge->executive_cb_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

//=============================================================================
// Statistics and Queries
//=============================================================================

pr_cognitive_error_t pr_cognitive_bridge_get_stats(
    pr_cognitive_bridge_t bridge,
    pr_cognitive_stats_t* stats
) {
    if (!bridge || !stats) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

pr_cognitive_error_t pr_cognitive_bridge_reset_stats(
    pr_cognitive_bridge_t bridge
) {
    if (!bridge) return PR_COG_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_COG_ERROR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);

    return PR_COG_SUCCESS;
}

bool pr_cognitive_bridge_is_connected(pr_cognitive_bridge_t bridge) {
    if (!bridge || !bridge->initialized) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_cognitive_bridge_heartbeat("pr_cognitive_is_connected", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bool all_connected = true;

    // Check each enabled link
    if (bridge->config.enable_attention_link &&
        bridge->attention.state != PR_COG_LINK_CONNECTED) {
        all_connected = false;
    }

    if (bridge->config.enable_emotion_link &&
        bridge->emotion.state != PR_COG_LINK_CONNECTED) {
        all_connected = false;
    }

    if (bridge->config.enable_executive_link &&
        bridge->executive.state != PR_COG_LINK_CONNECTED) {
        all_connected = false;
    }

    if (bridge->config.enable_hub_events &&
        bridge->hub.state != PR_COG_LINK_CONNECTED) {
        all_connected = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return all_connected;
}

const char* pr_cognitive_error_string(pr_cognitive_error_t error) {
    switch (error) {
        case PR_COG_SUCCESS:
            return "Success";
        case PR_COG_ERROR_NULL_POINTER:
            return "NULL pointer argument";
        case PR_COG_ERROR_NOT_INITIALIZED:
            return "Bridge not initialized";
        case PR_COG_ERROR_ALREADY_CONNECTED:
            return "Link already connected";
        case PR_COG_ERROR_NOT_CONNECTED:
            return "Link not connected";
        case PR_COG_ERROR_HUB_FAILED:
            return "Cognitive hub operation failed";
        case PR_COG_ERROR_INVALID_CONFIG:
            return "Invalid configuration";
        case PR_COG_ERROR_NO_MEMORY:
            return "Memory allocation failed";
        case PR_COG_ERROR_SYNC_FAILED:
            return "Working memory sync failed";
        case PR_COG_ERROR_GATE_BLOCKED:
            return "Operation blocked by executive gate";
        case PR_COG_ERROR_CAPACITY:
            return "Capacity limit reached";
        default:
            return "Unknown error";
    }
}

const char* pr_memory_event_type_string(pr_memory_event_type_t event_type) {
    switch (event_type) {
        case PR_MEM_EVENT_ENCODED:
            return "ENCODED";
        case PR_MEM_EVENT_RETRIEVED:
            return "RETRIEVED";
        case PR_MEM_EVENT_CONSOLIDATED:
            return "CONSOLIDATED";
        case PR_MEM_EVENT_FORGOTTEN:
            return "FORGOTTEN";
        case PR_MEM_EVENT_UPDATED:
            return "UPDATED";
        case PR_MEM_EVENT_WM_SYNC:
            return "WM_SYNC";
        default:
            return "UNKNOWN";
    }
}

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_cognitive_bridge_set_instance_health_agent(
    pr_cognitive_bridge_t bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_cognitive_bridge_training_begin(pr_cognitive_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_cognitive_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_cognitive_bridge_heartbeat_instance(bridge->health_agent, "pr_cognitive_bridge_training_begin", 0.0f);
    return 0;
}

int pr_cognitive_bridge_training_end(pr_cognitive_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_cognitive_bridge_training_end: NULL argument");
        return -1;
    }
    pr_cognitive_bridge_heartbeat_instance(bridge->health_agent, "pr_cognitive_bridge_training_end", 1.0f);
    return 0;
}

int pr_cognitive_bridge_training_step(pr_cognitive_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_cognitive_bridge_training_step: NULL argument");
        return -1;
    }
    pr_cognitive_bridge_heartbeat_instance(bridge->health_agent, "pr_cognitive_bridge_training_step", progress);
    return 0;
}
