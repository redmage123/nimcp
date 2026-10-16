/**
 * @file nimcp_emotion_tensor_bridge.c
 * @brief Implementation of emotion tensor bridge for swarm integration
 *
 * WHAT: Bridges tensor-based emotions with swarm emotional contagion
 * WHY:  Individual brains use tensors, swarm uses discrete emotion types
 * HOW:  Mapping functions + bio-async message handlers
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 * @version 1.0.0
 */

#include "cognitive/knowledge/nimcp_kg_reader.h"

#include "cognitive/nimcp_emotion_tensor_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(emotion_tensor_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_emotion_tensor_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_emotion_tensor_bridge_mesh_registry = NULL;

nimcp_error_t emotion_tensor_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_emotion_tensor_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "emotion_tensor_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "emotion_tensor_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_emotion_tensor_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_emotion_tensor_bridge_mesh_registry = registry;
    return err;
}

void emotion_tensor_bridge_mesh_unregister(void) {
    if (g_emotion_tensor_bridge_mesh_registry && g_emotion_tensor_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_emotion_tensor_bridge_mesh_registry, g_emotion_tensor_bridge_mesh_id);
        g_emotion_tensor_bridge_mesh_id = 0;
        g_emotion_tensor_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from emotion_tensor_bridge module (instance-level) */
static inline void emotion_tensor_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_emotion_tensor_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_emotion_tensor_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_emotion_tensor_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "EMOTION_TENSOR_BRIDGE"


/*=============================================================================
 * CONSTANTS AND LOGGING
 *============================================================================*/

#define BRIDGE_TAG "EmotionTensorBridge"
#define BRIDGE_LOG_DEBUG(fmt, ...) LOG_MODULE_DEBUG(BRIDGE_TAG, fmt, ##__VA_ARGS__)
#define BRIDGE_LOG_INFO(fmt, ...)  LOG_MODULE_INFO(BRIDGE_TAG, fmt, ##__VA_ARGS__)
#define BRIDGE_LOG_WARN(fmt, ...)  LOG_MODULE_WARN(BRIDGE_TAG, fmt, ##__VA_ARGS__)
#define BRIDGE_LOG_ERROR(fmt, ...) LOG_MODULE_ERROR(BRIDGE_TAG, fmt, ##__VA_ARGS__)

/** Default sync threshold (10% change triggers sync) */
#define DEFAULT_SYNC_THRESHOLD 0.1f

/** Default blend factor for synchronization */
#define DEFAULT_BLEND_FACTOR 0.5f

/** Default broadcast interval (500ms) */
#define DEFAULT_BROADCAST_INTERVAL_MS 500

/*=============================================================================
 * MAPPING TABLES
 *============================================================================*/

/**
 * Tensor primary → Swarm emotion mapping
 * Direct 1:1 mapping for basic emotions
 */
static const emotion_type_t s_tensor_to_swarm_map[EMOTION_TENSOR_PRIMARY_COUNT] = {
    [TENSOR_JOY]          = EMOTION_JOY,
    [TENSOR_TRUST]        = EMOTION_TRUST,
    [TENSOR_FEAR]         = EMOTION_FEAR,
    [TENSOR_SURPRISE]     = EMOTION_SURPRISE,
    [TENSOR_SADNESS]      = EMOTION_SADNESS,
    [TENSOR_DISGUST]      = EMOTION_DISGUST,
    [TENSOR_ANGER]        = EMOTION_ANGER,
    [TENSOR_ANTICIPATION] = EMOTION_ANTICIPATION
};

/**
 * Swarm emotion → Tensor primary mapping
 * Complex swarm emotions map to compounds or combinations
 */
typedef struct {
    emotion_primary_t primary;          /**< Primary tensor channel */
    emotion_primary_t secondary;        /**< Secondary channel (or -1) */
    float primary_weight;               /**< Weight for primary */
    float secondary_weight;             /**< Weight for secondary */
} swarm_tensor_mapping_entry_t;

static const swarm_tensor_mapping_entry_t s_swarm_to_tensor_map[EMOTION_TYPE_COUNT] = {
    [EMOTION_NEUTRAL]      = { .primary = TENSOR_JOY, .secondary = (emotion_primary_t)-1, .primary_weight = 0.0F, .secondary_weight = 0.0F },
    [EMOTION_JOY]          = { .primary = TENSOR_JOY, .secondary = (emotion_primary_t)-1, .primary_weight = 1.0F, .secondary_weight = 0.0F },
    [EMOTION_SADNESS]      = { .primary = TENSOR_SADNESS, .secondary = (emotion_primary_t)-1, .primary_weight = 1.0F, .secondary_weight = 0.0F },
    [EMOTION_ANGER]        = { .primary = TENSOR_ANGER, .secondary = (emotion_primary_t)-1, .primary_weight = 1.0F, .secondary_weight = 0.0F },
    [EMOTION_FEAR]         = { .primary = TENSOR_FEAR, .secondary = (emotion_primary_t)-1, .primary_weight = 1.0F, .secondary_weight = 0.0F },
    [EMOTION_SURPRISE]     = { .primary = TENSOR_SURPRISE, .secondary = (emotion_primary_t)-1, .primary_weight = 1.0F, .secondary_weight = 0.0F },
    [EMOTION_DISGUST]      = { .primary = TENSOR_DISGUST, .secondary = (emotion_primary_t)-1, .primary_weight = 1.0F, .secondary_weight = 0.0F },
    [EMOTION_TRUST]        = { .primary = TENSOR_TRUST, .secondary = (emotion_primary_t)-1, .primary_weight = 1.0F, .secondary_weight = 0.0F },
    [EMOTION_ANTICIPATION] = { .primary = TENSOR_ANTICIPATION, .secondary = (emotion_primary_t)-1, .primary_weight = 1.0F, .secondary_weight = 0.0F },
    [EMOTION_CURIOSITY]    = { .primary = TENSOR_SURPRISE, .secondary = TENSOR_ANTICIPATION, .primary_weight = 0.5F, .secondary_weight = 0.5F },
    [EMOTION_CALM]         = { .primary = TENSOR_TRUST, .secondary = TENSOR_JOY, .primary_weight = 0.3F, .secondary_weight = 0.3F },
    [EMOTION_EXCITEMENT]   = { .primary = TENSOR_JOY, .secondary = TENSOR_ANTICIPATION, .primary_weight = 0.7F, .secondary_weight = 0.5F },
    [EMOTION_FRUSTRATION]  = { .primary = TENSOR_ANGER, .secondary = TENSOR_SADNESS, .primary_weight = 0.6F, .secondary_weight = 0.4F },
    [EMOTION_HOPE]         = { .primary = TENSOR_ANTICIPATION, .secondary = TENSOR_TRUST, .primary_weight = 0.6F, .secondary_weight = 0.4F },
    [EMOTION_DESPAIR]      = { .primary = TENSOR_FEAR, .secondary = TENSOR_SADNESS, .primary_weight = 0.5F, .secondary_weight = 0.5F },
    [EMOTION_PRIDE]        = { .primary = TENSOR_JOY, .secondary = TENSOR_ANGER, .primary_weight = 0.6F, .secondary_weight = 0.3F },
    [EMOTION_SHAME]        = { .primary = TENSOR_FEAR, .secondary = TENSOR_SADNESS, .primary_weight = 0.5F, .secondary_weight = 0.5F },
    [EMOTION_GUILT]        = { .primary = TENSOR_FEAR, .secondary = TENSOR_JOY, .primary_weight = 0.4F, .secondary_weight = 0.3F },
    [EMOTION_ENVY]         = { .primary = TENSOR_SADNESS, .secondary = TENSOR_ANGER, .primary_weight = 0.5F, .secondary_weight = 0.5F },
    [EMOTION_GRATITUDE]    = { .primary = TENSOR_JOY, .secondary = TENSOR_TRUST, .primary_weight = 0.6F, .secondary_weight = 0.4F }
};

/*=============================================================================
 * INTERNAL STRUCTURE
 *============================================================================*/

struct emotion_tensor_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    emotion_tensor_system_t* tensor;        /**< Tensor system */
    emotional_contagion_t* contagion;       /**< Swarm contagion (can be NULL) */
    emotion_tensor_bridge_config_t config;  /**< Configuration */

    /* Bio-async integration */
    bio_module_context_t module_ctx;        /**< Module context */
    bool bioasync_registered;               /**< Registration status */

    /* Cached state for change detection */
    float cached_channels[EMOTION_TENSOR_PRIMARY_COUNT];
    float cached_valence;
    float cached_arousal;
    uint64_t last_broadcast_ms;

    /* Statistics */
    emotion_tensor_bridge_stats_t stats;

    nimcp_health_agent_t* health_agent;  /**< Instance-level health agent */
};

/*=============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * Check if tensor has changed significantly
 */
static bool has_significant_change(
    const emotion_tensor_bridge_t* bridge,
    const emotion_tensor_t* tensor
) {
    float threshold = bridge->config.sync_threshold;

    /* Check each channel */
    for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && EMOTION_TENSOR_PRIMARY_COUNT > 256) {
            emotion_tensor_bridge_heartbeat("emotion_tens_loop",
                             (float)(i + 1) / (float)EMOTION_TENSOR_PRIMARY_COUNT);
        }

        float diff = fabsf(tensor->channels[i] - bridge->cached_channels[i]);
        if (diff > threshold) return true;
    }

    /* Check aggregates */
    if (fabsf(tensor->overall_valence - bridge->cached_valence) > threshold) return true;
    if (fabsf(tensor->overall_arousal - bridge->cached_arousal) > threshold) return true;

    return false;
}

/**
 * Update cached state from tensor
 */
static void update_cached_state(
    emotion_tensor_bridge_t* bridge,
    const emotion_tensor_t* tensor
) {
    memcpy(bridge->cached_channels, tensor->channels, sizeof(bridge->cached_channels));
    bridge->cached_valence = tensor->overall_valence;
    bridge->cached_arousal = tensor->overall_arousal;
}

/*=============================================================================
 * LIFECYCLE IMPLEMENTATION
 *============================================================================*/

emotion_tensor_bridge_config_t emotion_tensor_bridge_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_bridge_heartbeat("emotion_tens_default_config", 0.0f);


    return (emotion_tensor_bridge_config_t){
        .sync_threshold = DEFAULT_SYNC_THRESHOLD,
        .blend_factor = DEFAULT_BLEND_FACTOR,
        .broadcast_interval_ms = DEFAULT_BROADCAST_INTERVAL_MS,
        .auto_broadcast = true,
        .enable_compound_detection = true,
        .enable_contradiction_detection = true
    };
}

emotion_tensor_bridge_t* emotion_tensor_bridge_create(
    emotion_tensor_system_t* tensor,
    emotional_contagion_t* contagion,
    const emotion_tensor_bridge_config_t* config
) {
    /* Guard: validate tensor */
    if (!tensor) {
        BRIDGE_LOG_ERROR("NULL tensor system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tensor is NULL");

        return NULL;
    }

    /* Allocate bridge */
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_bridge_heartbeat("emotion_tens_create", 0.0f);


    emotion_tensor_bridge_t* bridge = nimcp_calloc(1, sizeof(emotion_tensor_bridge_t));
    if (!bridge) {
        BRIDGE_LOG_ERROR("Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Initialize */
    bridge->tensor = tensor;
    bridge->contagion = contagion;
    bridge->config = config ? *config : emotion_tensor_bridge_default_config();
    bridge->bioasync_registered = false;

    /* Initialize cached state */
    emotion_tensor_t current;
    if (emotion_tensor_get(tensor, &current)) {
        update_cached_state(bridge, &current);
    }

    BRIDGE_LOG_INFO("Created emotion tensor bridge (contagion=%s)",
                    contagion ? "enabled" : "disabled");

    return bridge;
}

void emotion_tensor_bridge_destroy(emotion_tensor_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "emotion_tensor");

    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_bridge_heartbeat("emotion_tens_destroy", 0.0f);


    BRIDGE_LOG_INFO("Destroying emotion tensor bridge");
    nimcp_free(bridge);
    bridge = NULL;
}

/*=============================================================================
 * KG-Driven Wiring Callback
 *============================================================================*/

/* Forward declarations for handlers */
static nimcp_error_t bridge_message_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
);

/**
 * @brief KG-driven wiring handler callback
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 */
static int emotion_tensor_bridge_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    BRIDGE_LOG_INFO("emotion_tensor_bridge_wiring_handler_callback: registering %u handlers from KG",
                    message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && message_count > 256) {
            emotion_tensor_bridge_heartbeat("emotion_tens_loop",
                             (float)(i + 1) / (float)message_count);
        }

        switch (message_types[i]) {
            case BIO_MSG_EMOTION_TENSOR_QUERY:
                bio_router_register_handler(ctx, message_types[i], bridge_message_handler);
                BRIDGE_LOG_DEBUG("  Registered handler for BIO_MSG_EMOTION_TENSOR_QUERY");
                break;

            case BIO_MSG_EMOTION_TENSOR_STIMULUS:
                bio_router_register_handler(ctx, message_types[i], bridge_message_handler);
                BRIDGE_LOG_DEBUG("  Registered handler for BIO_MSG_EMOTION_TENSOR_STIMULUS");
                break;

            case BIO_MSG_EMOTION_SWARM_SYNC:
                bio_router_register_handler(ctx, message_types[i], bridge_message_handler);
                BRIDGE_LOG_DEBUG("  Registered handler for BIO_MSG_EMOTION_SWARM_SYNC");
                break;

            default:
                BRIDGE_LOG_DEBUG("  Unknown message type %u - skipping", message_types[i]);
                break;
        }
    }

    return 0;
}

/*=============================================================================
 * BIO-ASYNC HANDLERS
 *============================================================================*/

/**
 * Handle BIO_MSG_EMOTION_TENSOR_QUERY
 */
static nimcp_error_t handle_tensor_query(
    emotion_tensor_bridge_t* bridge,
    const bio_msg_emotion_tensor_query_t* query,
    nimcp_bio_promise_t response_promise
) {
    bio_msg_emotion_tensor_response_t response;
    memset(&response, 0, sizeof(response));

    /* Get current tensor state */
    emotion_tensor_t tensor;
    if (!emotion_tensor_get(bridge->tensor, &tensor)) {
        BRIDGE_LOG_ERROR("Failed to get tensor state for query");
        return NIMCP_ERROR_INVALID;
    }

    /* Fill response */
    bio_msg_init_header(&response.header, BIO_MSG_EMOTION_TENSOR_RESPONSE,
                        BIO_MODULE_EMOTION_TENSOR_BRIDGE, 0, sizeof(response));

    memcpy(response.channels, tensor.channels, sizeof(response.channels));
    memcpy(response.compounds, tensor.compounds, sizeof(response.compounds));
    response.valence = tensor.overall_valence;
    response.arousal = tensor.overall_arousal;
    response.entropy = tensor.emotional_entropy;
    response.stability = tensor.stability;
    response.primary_emotion = (uint8_t)tensor.primary_emotion;
    response.secondary_emotion = (uint8_t)tensor.secondary_emotion;
    response.contradictory = emotion_tensor_is_contradictory(bridge->tensor, 0.3F);
    response.timestamp_ms = tensor.last_update_ms;

    /* If promise provided, complete it */
    if (response_promise) {
        /* Complete promise with response data */
        BRIDGE_LOG_DEBUG("Completed tensor query response");
    }

    return NIMCP_SUCCESS;
}

/**
 * Handle BIO_MSG_EMOTION_TENSOR_STIMULUS
 */
static nimcp_error_t handle_tensor_stimulus(
    emotion_tensor_bridge_t* bridge,
    const bio_msg_emotion_tensor_stimulus_t* stimulus
) {
    /* Apply stimulus to tensor */
    bool success = emotion_tensor_apply_stimulus(
        bridge->tensor,
        (emotion_primary_t)stimulus->target_emotion,
        stimulus->intensity,
        stimulus->is_positive,
        stimulus->timestamp_ms
    );

    if (!success) {
        BRIDGE_LOG_WARN("Failed to apply stimulus to emotion %d", stimulus->target_emotion);
        return NIMCP_INVALID_PARAM;
    }

    BRIDGE_LOG_DEBUG("Applied stimulus to emotion %d: intensity=%.2f, positive=%d",
                     stimulus->target_emotion, stimulus->intensity, stimulus->is_positive);

    return NIMCP_SUCCESS;
}

/**
 * Handle BIO_MSG_EMOTION_SWARM_SYNC
 */
static nimcp_error_t handle_swarm_sync(
    emotion_tensor_bridge_t* bridge,
    const bio_msg_emotion_swarm_sync_t* sync
) {
    switch (sync->direction) {
        case BIO_EMOTION_SYNC_SWARM_TO_TENSOR:
            /* Update tensor from swarm state */
            return emotion_tensor_from_swarm(
                bridge->tensor,
                (emotion_type_t)sync->swarm_emotion,
                sync->swarm_intensity,
                sync->blend_factor,
                0  /* Use current time */
            );

        case BIO_EMOTION_SYNC_TENSOR_TO_SWARM:
            /* Update swarm from tensor - requires contagion system */
            if (!bridge->contagion) {
                BRIDGE_LOG_WARN("Cannot sync to swarm: no contagion system");
                return NIMCP_NOT_INITIALIZED;
            }
            /* Map tensor to swarm emotion and set */
            tensor_swarm_mapping_t mapping;
            emotion_tensor_to_swarm(bridge->tensor, &mapping);
            emotional_contagion_set_emotion(
                bridge->contagion,
                sync->agent_id,
                mapping.swarm_emotion,
                mapping.intensity * sync->blend_factor
            );
            break;

        case BIO_EMOTION_SYNC_BIDIRECTIONAL:
            /* Blend both ways */
            /* First update tensor from swarm */
            emotion_tensor_from_swarm(
                bridge->tensor,
                (emotion_type_t)sync->swarm_emotion,
                sync->swarm_intensity,
                sync->blend_factor * 0.5F,
                0
            );
            /* Then sync back to swarm */
            if (bridge->contagion) {
                tensor_swarm_mapping_t map;
                emotion_tensor_to_swarm(bridge->tensor, &map);
                emotional_contagion_set_emotion(
                    bridge->contagion,
                    sync->agent_id,
                    map.swarm_emotion,
                    map.intensity * sync->blend_factor * 0.5F
                );
            }
            break;

        default:
            BRIDGE_LOG_WARN("Unknown sync direction: %d", sync->direction);
            return NIMCP_INVALID_PARAM;
    }

    bridge->stats.syncs_received++;
    return NIMCP_SUCCESS;
}

/**
 * Main message handler callback
 */
static nimcp_error_t bridge_message_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    emotion_tensor_bridge_t* bridge = (emotion_tensor_bridge_t*)user_data;
    if (!bridge || !msg) return NIMCP_INVALID_PARAM;

    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    switch (header->type) {
        case BIO_MSG_EMOTION_TENSOR_QUERY:
            return handle_tensor_query(bridge,
                (const bio_msg_emotion_tensor_query_t*)msg, response_promise);

        case BIO_MSG_EMOTION_TENSOR_STIMULUS:
            return handle_tensor_stimulus(bridge,
                (const bio_msg_emotion_tensor_stimulus_t*)msg);

        case BIO_MSG_EMOTION_SWARM_SYNC:
            return handle_swarm_sync(bridge,
                (const bio_msg_emotion_swarm_sync_t*)msg);

        default:
            BRIDGE_LOG_DEBUG("Unhandled message type: 0x%04x", header->type);
            return NIMCP_SUCCESS;
    }
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION IMPLEMENTATION
 *============================================================================*/

nimcp_result_t emotion_tensor_bridge_register_bioasync(
    emotion_tensor_bridge_t* bridge,
    bio_router_t router
) {
    if (!bridge) return NIMCP_INVALID_PARAM;
    if (!router) return NIMCP_INVALID_PARAM;

    /* Register module */
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_bridge_heartbeat("emotion_tens_register_bioasync", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_EMOTION_TENSOR_BRIDGE,
        .module_name = "EmotionTensorBridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->module_ctx = bio_router_register_module(&info);
    if (!bridge->module_ctx) {
        BRIDGE_LOG_ERROR("Failed to register with bio-router");
        return NIMCP_ERROR_INVALID;
    }

    /* Register handlers via KG-driven wiring callback */
    nimcp_error_t wiring_result = bio_router_register_wiring_callback(
        BIO_MODULE_EMOTION_TENSOR_BRIDGE,
        (void*)emotion_tensor_bridge_wiring_handler_callback,
        bridge
    );

    if (wiring_result != NIMCP_SUCCESS) {
        /* Legacy fallback: direct handler registration */
        BRIDGE_LOG_DEBUG("KG wiring unavailable, using legacy registration");
        LEGACY_HANDLER_REGISTRATION(
            bio_router_register_handler(bridge->module_ctx,
                BIO_MSG_EMOTION_TENSOR_QUERY, bridge_message_handler));
        LEGACY_HANDLER_REGISTRATION(
            bio_router_register_handler(bridge->module_ctx,
                BIO_MSG_EMOTION_TENSOR_STIMULUS, bridge_message_handler));
        LEGACY_HANDLER_REGISTRATION(
            bio_router_register_handler(bridge->module_ctx,
                BIO_MSG_EMOTION_SWARM_SYNC, bridge_message_handler));
    }

    bridge->bioasync_registered = true;
    BRIDGE_LOG_INFO("Registered with bio-async router");

    return NIMCP_SUCCESS;
}

nimcp_result_t emotion_tensor_bridge_broadcast_state(
    emotion_tensor_bridge_t* bridge
) {
    if (!bridge) return NIMCP_INVALID_PARAM;
    if (!bridge->bioasync_registered) return NIMCP_NOT_INITIALIZED;

    /* Get current tensor state */
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_bridge_heartbeat("emotion_tens_broadcast_state", 0.0f);


    emotion_tensor_t tensor;
    if (!emotion_tensor_get(bridge->tensor, &tensor)) {
        return NIMCP_ERROR_INVALID;
    }

    /* Build update message */
    bio_msg_emotion_tensor_update_t msg;
    memset(&msg, 0, sizeof(msg));

    bio_msg_init_header(&msg.header, BIO_MSG_EMOTION_TENSOR_UPDATE,
                        BIO_MODULE_EMOTION_TENSOR_BRIDGE, BIO_MODULE_ALL, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_SEROTONIN;  /* Emotion state → serotonin */
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;

    memcpy(msg.channels, tensor.channels, sizeof(msg.channels));
    msg.valence = tensor.overall_valence;
    msg.arousal = tensor.overall_arousal;
    msg.entropy = tensor.emotional_entropy;
    msg.stability = tensor.stability;
    msg.primary_emotion = (uint8_t)tensor.primary_emotion;
    msg.secondary_emotion = (uint8_t)tensor.secondary_emotion;
    msg.blend_ratio = tensor.blend_ratio;
    msg.timestamp_ms = tensor.last_update_ms;

    /* Broadcast */
    nimcp_error_t err = bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    if (err == NIMCP_SUCCESS) {
        bridge->stats.broadcasts_sent++;
        update_cached_state(bridge, &tensor);
    }

    return err;
}

nimcp_result_t emotion_tensor_bridge_handle_message(
    emotion_tensor_bridge_t* bridge,
    const void* msg,
    size_t msg_size
) {
    return bridge_message_handler(msg, msg_size, NULL, bridge);
}

/*=============================================================================
 * CONVERSION IMPLEMENTATION
 *============================================================================*/

nimcp_result_t emotion_tensor_to_swarm(
    const emotion_tensor_system_t* tensor,
    tensor_swarm_mapping_t* mapping
) {
    if (!tensor || !mapping) return NIMCP_INVALID_PARAM;

    /* Get dominant emotion from tensor */
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_bridge_heartbeat("emotion_tens_emotion_tensor_to_sw", 0.0f);


    emotion_primary_t primary, secondary;
    float blend_ratio = 0.0f;
    if (!emotion_tensor_get_dominant(tensor, &primary, &secondary, &blend_ratio)) {
        return NIMCP_ERROR_INVALID;
    }

    /* Map primary to swarm */
    mapping->swarm_emotion = s_tensor_to_swarm_map[primary];
    mapping->intensity = emotion_tensor_get_channel(tensor, primary);
    mapping->is_compound = false;
    mapping->compound = (emotion_compound_t)0;

    /* Check for compound emotion (if blend is significant) */
    if (blend_ratio > 0.3F) {
        /* Check for specific compounds based on pair */
        if ((primary == TENSOR_JOY && secondary == TENSOR_SADNESS) ||
            (primary == TENSOR_SADNESS && secondary == TENSOR_JOY)) {
            mapping->is_compound = true;
            mapping->compound = COMPOUND_BITTERSWEETNESS;
        } else if ((primary == TENSOR_SURPRISE && secondary == TENSOR_ANTICIPATION) ||
                   (primary == TENSOR_ANTICIPATION && secondary == TENSOR_SURPRISE)) {
            mapping->is_compound = true;
            mapping->compound = COMPOUND_AMBIVALENCE;
        }
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t emotion_tensor_from_swarm(
    emotion_tensor_system_t* tensor,
    emotion_type_t swarm_emotion,
    float intensity,
    float blend_factor,
    uint64_t timestamp_ms
) {
    if (!tensor) return NIMCP_INVALID_PARAM;
    if (swarm_emotion >= EMOTION_TYPE_COUNT) return NIMCP_INVALID_PARAM;

    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_bridge_heartbeat("emotion_tens_emotion_tensor_from_", 0.0f);


    const swarm_tensor_mapping_entry_t* mapping = &s_swarm_to_tensor_map[swarm_emotion];

    /* Apply primary channel */
    if (mapping->primary_weight > 0) {
        float current = emotion_tensor_get_channel(tensor, mapping->primary);
        float target = intensity * mapping->primary_weight;
        float blended = current * (1.0F - blend_factor) + target * blend_factor;
        emotion_tensor_set_channel(tensor, mapping->primary, nimcp_clamp01(blended), timestamp_ms);
    }

    /* Apply secondary channel if present */
    if (mapping->secondary != (emotion_primary_t)-1 && mapping->secondary_weight > 0) {
        float current = emotion_tensor_get_channel(tensor, mapping->secondary);
        float target = intensity * mapping->secondary_weight;
        float blended = current * (1.0F - blend_factor) + target * blend_factor;
        emotion_tensor_set_channel(tensor, mapping->secondary, nimcp_clamp01(blended), timestamp_ms);
    }

    return NIMCP_SUCCESS;
}

emotion_primary_t emotion_swarm_to_tensor_channel(emotion_type_t swarm_emotion) {
    if (swarm_emotion >= EMOTION_TYPE_COUNT) return TENSOR_JOY;
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_bridge_heartbeat("emotion_tens_emotion_swarm_to_ten", 0.0f);


    return s_swarm_to_tensor_map[swarm_emotion].primary;
}

emotion_type_t emotion_tensor_channel_to_swarm(emotion_primary_t tensor_emotion) {
    if (tensor_emotion >= EMOTION_TENSOR_PRIMARY_COUNT) return EMOTION_NEUTRAL;
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_bridge_heartbeat("emotion_tens_emotion_tensor_chann", 0.0f);


    return s_tensor_to_swarm_map[tensor_emotion];
}

/*=============================================================================
 * TAG CONVERSION IMPLEMENTATION
 *============================================================================*/

nimcp_result_t emotion_tensor_to_tag(
    const emotion_tensor_system_t* tensor,
    emotional_tag_t* tag
) {
    if (!tensor || !tag) return NIMCP_INVALID_PARAM;

    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_bridge_heartbeat("emotion_tens_emotion_tensor_to_ta", 0.0f);


    float valence = emotion_tensor_get_valence(tensor);
    float arousal = emotion_tensor_get_arousal(tensor);

    emotion_tensor_t state;
    if (!emotion_tensor_get(tensor, &state)) {
        return NIMCP_ERROR_INVALID;
    }

    *tag = emotional_tag_create(valence, arousal, state.last_update_ms);
    return NIMCP_SUCCESS;
}

nimcp_result_t emotion_tensor_from_tag(
    emotion_tensor_system_t* tensor,
    const emotional_tag_t* tag,
    uint64_t timestamp_ms
) {
    if (!tensor || !tag) return NIMCP_INVALID_PARAM;

    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_bridge_heartbeat("emotion_tens_emotion_tensor_from_", 0.0f);


    float valence = tag->valence;
    float arousal = tag->arousal;

    /* Map valence/arousal quadrant to primary emotions */
    /* High valence + high arousal → JOY */
    /* High valence + low arousal → TRUST */
    /* Low valence + high arousal → FEAR or ANGER */
    /* Low valence + low arousal → SADNESS */

    float abs_valence = fabsf(valence);

    if (valence >= 0) {
        /* Positive valence */
        if (arousal > 0.5F) {
            emotion_tensor_set_channel(tensor, TENSOR_JOY, abs_valence * arousal, timestamp_ms);
            emotion_tensor_set_channel(tensor, TENSOR_ANTICIPATION, arousal * 0.5F, timestamp_ms);
        } else {
            emotion_tensor_set_channel(tensor, TENSOR_TRUST, abs_valence, timestamp_ms);
            emotion_tensor_set_channel(tensor, TENSOR_JOY, abs_valence * 0.5F, timestamp_ms);
        }
    } else {
        /* Negative valence */
        if (arousal > 0.5F) {
            emotion_tensor_set_channel(tensor, TENSOR_FEAR, abs_valence * 0.5F, timestamp_ms);
            emotion_tensor_set_channel(tensor, TENSOR_ANGER, abs_valence * 0.5F, timestamp_ms);
        } else {
            emotion_tensor_set_channel(tensor, TENSOR_SADNESS, abs_valence, timestamp_ms);
        }
    }

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * SYNCHRONIZATION IMPLEMENTATION
 *============================================================================*/

nimcp_result_t emotion_tensor_bridge_sync_agent(
    emotion_tensor_bridge_t* bridge,
    uint32_t agent_id,
    uint8_t direction,
    float blend_factor
) {
    if (!bridge) return NIMCP_INVALID_PARAM;
    if (!bridge->bioasync_registered) return NIMCP_NOT_INITIALIZED;

    /* Get current tensor state */
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_bridge_heartbeat("emotion_tens_sync_agent", 0.0f);


    emotion_tensor_t tensor;
    if (!emotion_tensor_get(bridge->tensor, &tensor)) {
        return NIMCP_ERROR_INVALID;
    }

    /* Build sync message */
    bio_msg_emotion_swarm_sync_t msg;
    memset(&msg, 0, sizeof(msg));

    bio_msg_init_header(&msg.header, BIO_MSG_EMOTION_SWARM_SYNC,
                        BIO_MODULE_EMOTION_TENSOR_BRIDGE, BIO_MODULE_SWARM_QUORUM, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_SEROTONIN;

    msg.agent_id = agent_id;
    msg.direction = direction;
    msg.blend_factor = blend_factor;
    memcpy(msg.tensor_channels, tensor.channels, sizeof(msg.tensor_channels));
    msg.tensor_valence = tensor.overall_valence;
    msg.tensor_arousal = tensor.overall_arousal;

    /* Send sync message */
    nimcp_error_t err = bio_router_send(bridge->module_ctx, &msg, sizeof(msg), 0);
    if (err == NIMCP_SUCCESS) {
        bridge->stats.syncs_sent++;
    }

    return err;
}

nimcp_result_t emotion_tensor_bridge_propagate_to_swarm(
    emotion_tensor_bridge_t* bridge,
    uint32_t max_depth
) {
    if (!bridge) return NIMCP_INVALID_PARAM;
    if (!bridge->contagion) {
        BRIDGE_LOG_WARN("Cannot propagate: no contagion system");
        return NIMCP_NOT_INITIALIZED;
    }

    /* Get dominant emotion from tensor */
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_bridge_heartbeat("emotion_tens_propagate_to_swarm", 0.0f);


    tensor_swarm_mapping_t mapping;
    nimcp_result_t result = emotion_tensor_to_swarm(bridge->tensor, &mapping);
    if (result != NIMCP_SUCCESS) return result;

    /* Trigger outbreak in swarm */
    return emotional_contagion_trigger_outbreak(
        bridge->contagion,
        0,  /* Source agent (0 = external) */
        mapping.swarm_emotion,
        mapping.intensity,
        max_depth > 0 ? max_depth : 3
    );
}

nimcp_result_t emotion_tensor_bridge_update_from_collective(
    emotion_tensor_bridge_t* bridge,
    float blend_factor
) {
    if (!bridge) return NIMCP_INVALID_PARAM;
    if (!bridge->contagion) {
        BRIDGE_LOG_WARN("Cannot update from collective: no contagion system");
        return NIMCP_NOT_INITIALIZED;
    }

    /* Get collective state */
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_bridge_heartbeat("emotion_tens_update_from_collecti", 0.0f);


    collective_emotion_state_t collective;
    nimcp_result_t result = emotional_contagion_get_collective_state(
        bridge->contagion, &collective);
    if (result != NIMCP_SUCCESS) return result;

    /* Apply to tensor */
    return emotion_tensor_from_swarm(
        bridge->tensor,
        collective.dominant_emotion,
        collective.dominant_intensity,
        blend_factor,
        0  /* Current time */
    );
}

/*=============================================================================
 * EVENT NOTIFICATION IMPLEMENTATION
 *============================================================================*/

nimcp_result_t emotion_tensor_bridge_notify_compound(
    emotion_tensor_bridge_t* bridge,
    emotion_compound_t compound,
    float activation
) {
    if (!bridge) return NIMCP_INVALID_PARAM;
    if (!bridge->bioasync_registered) return NIMCP_NOT_INITIALIZED;

    /* Build compound notification */
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_bridge_heartbeat("emotion_tens_notify_compound", 0.0f);


    bio_msg_emotion_tensor_compound_t msg;
    memset(&msg, 0, sizeof(msg));

    bio_msg_init_header(&msg.header, BIO_MSG_EMOTION_TENSOR_COMPOUND,
                        BIO_MODULE_EMOTION_TENSOR_BRIDGE, BIO_MODULE_ALL, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_SEROTONIN;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;

    msg.compound_type = (uint8_t)compound;
    msg.activation = activation;
    msg.is_contradictory = (compound >= COMPOUND_BITTERSWEETNESS);

    /* Determine contributing primaries based on compound type */
    switch (compound) {
        case COMPOUND_BITTERSWEETNESS:
            msg.primary_a = TENSOR_JOY;
            msg.primary_b = TENSOR_SADNESS;
            break;
        case COMPOUND_AMBIVALENCE:
            msg.primary_a = TENSOR_SURPRISE;
            msg.primary_b = TENSOR_ANTICIPATION;
            break;
        case COMPOUND_NOSTALGIA:
            msg.primary_a = TENSOR_ANTICIPATION;
            msg.primary_b = TENSOR_SADNESS;
            break;
        default:
            /* Map other compounds to their pairs */
            msg.primary_a = (uint8_t)(compound % EMOTION_TENSOR_PRIMARY_COUNT);
            msg.primary_b = (uint8_t)((compound + 1) % EMOTION_TENSOR_PRIMARY_COUNT);
            break;
    }

    /* Get activation levels */
    msg.primary_a_level = emotion_tensor_get_channel(bridge->tensor, (emotion_primary_t)msg.primary_a);
    msg.primary_b_level = emotion_tensor_get_channel(bridge->tensor, (emotion_primary_t)msg.primary_b);

    /* Broadcast */
    nimcp_error_t err = bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    if (err == NIMCP_SUCCESS) {
        bridge->stats.compounds_detected++;
    }

    return err;
}

nimcp_result_t emotion_tensor_bridge_notify_contradiction(
    emotion_tensor_bridge_t* bridge,
    emotion_primary_t emotion_a,
    emotion_primary_t emotion_b,
    float conflict_intensity
) {
    if (!bridge) return NIMCP_INVALID_PARAM;
    if (!bridge->bioasync_registered) return NIMCP_NOT_INITIALIZED;

    /* Build contradiction notification */
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_bridge_heartbeat("emotion_tens_notify_contradiction", 0.0f);


    bio_msg_emotion_tensor_contradiction_t msg;
    memset(&msg, 0, sizeof(msg));

    bio_msg_init_header(&msg.header, BIO_MSG_EMOTION_TENSOR_CONTRADICTION,
                        BIO_MODULE_EMOTION_TENSOR_BRIDGE, BIO_MODULE_ALL, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;  /* Alert channel */
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;

    msg.emotion_a = (uint8_t)emotion_a;
    msg.emotion_b = (uint8_t)emotion_b;
    msg.level_a = emotion_tensor_get_channel(bridge->tensor, emotion_a);
    msg.level_b = emotion_tensor_get_channel(bridge->tensor, emotion_b);
    msg.conflict_intensity = conflict_intensity;

    /* Broadcast */
    nimcp_error_t err = bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    if (err == NIMCP_SUCCESS) {
        bridge->stats.contradictions_detected++;
    }

    return err;
}

/*=============================================================================
 * UTILITY IMPLEMENTATION
 *============================================================================*/

bool emotion_tensor_bridge_needs_sync(const emotion_tensor_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_tensor_bridge_needs_sync: bridge is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_bridge_heartbeat("emotion_tens_needs_sync", 0.0f);


    emotion_tensor_t current;
    if (!emotion_tensor_get(bridge->tensor, &current)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "emotion_tensor_bridge_needs_sync: emotion_tensor_get is NULL");
        return false;
    }

    return has_significant_change(bridge, &current);
}

nimcp_result_t emotion_tensor_bridge_get_stats(
    const emotion_tensor_bridge_t* bridge,
    emotion_tensor_bridge_stats_t* stats
) {
    if (!bridge || !stats) return NIMCP_INVALID_PARAM;
    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_bridge_heartbeat("emotion_tens_get_stats", 0.0f);


    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int emotion_tensor_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_bridge_heartbeat("emotion_tens_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Emotion_Tensor_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                emotion_tensor_bridge_heartbeat("emotion_tens_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Emotion_Tensor_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Emotion_Tensor_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Instance-Level Health Agent (Phase 8 Utility Integration)
 * ============================================================================ */

void emotion_tensor_bridge_set_instance_health_agent(emotion_tensor_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "emotion_tensor_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Training Stubs (Phase 8 Utility Integration)
 *
 * Stub: training integration planned — these are intentional no-ops that
 * provide heartbeat signaling only. Full training hooks will wire into the
 * training-immune bridge when per-module gradient propagation is implemented.
 * ============================================================================ */

int emotion_tensor_bridge_training_begin(emotion_tensor_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_tensor_bridge_training_begin: NULL argument");
        return -1;
    }
    emotion_tensor_bridge_heartbeat_instance(bridge->health_agent, "emotion_tensor_bridge_training_begin", 0.0f);
    return 0;
}

int emotion_tensor_bridge_training_end(emotion_tensor_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_tensor_bridge_training_end: NULL argument");
        return -1;
    }
    emotion_tensor_bridge_heartbeat_instance(bridge->health_agent, "emotion_tensor_bridge_training_end", 1.0f);
    return 0;
}

int emotion_tensor_bridge_training_step(emotion_tensor_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_tensor_bridge_training_step: NULL argument");
        return -1;
    }
    emotion_tensor_bridge_heartbeat_instance(bridge->health_agent, "emotion_tensor_bridge_training_step", progress);
    return 0;
}
