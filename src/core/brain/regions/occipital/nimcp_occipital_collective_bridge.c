//=============================================================================
// nimcp_occipital_collective_bridge.c - Occipital-Collective Cognition Integration
//=============================================================================
/**
 * @file nimcp_occipital_collective_bridge.c
 * @brief Implementation of Occipital-Collective Cognition bridge
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include "core/brain/regions/occipital/nimcp_occipital_collective_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"
#include "cognitive/collective_cognition/nimcp_collective_cognition.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE "OCCIPITAL_COLLECTIVE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(occipital_collective_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* Use standardized enum values from nimcp_bio_messages.h:
 * BIO_MSG_OCCIPITAL_COLLECTIVE_ATTENTION     0x2E21
 * BIO_MSG_OCCIPITAL_COLLECTIVE_FEATURE       0x2E22
 * BIO_MSG_OCCIPITAL_COLLECTIVE_STATE_UPDATE  0x2E23
 * BIO_MSG_OCCIPITAL_COLLECTIVE_GAZE_FOLLOW   0x2E24
 */

//=============================================================================
// Internal Structure
//=============================================================================

struct occipital_collective_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Bio-async context */
    bio_module_context_t bio_ctx;

    /* Configuration */
    occipital_collective_config_t config;

    /* Connected systems */
    occipital_adapter_t* occipital;
    collective_cognition_t* collective;

    /* Local state */
    uint32_t local_instance_id;
    float local_attention_x;
    float local_attention_y;
    float local_confidence[VISUAL_AREA_COUNT];
    bool is_attention_leader;

    /* Joint attention targets */
    joint_attention_target_t targets[OCCIPITAL_COLLECTIVE_MAX_TARGETS];
    uint32_t target_count;
    uint32_t next_target_id;

    /* Shared features */
    shared_visual_feature_t features[OCCIPITAL_COLLECTIVE_MAX_FEATURES];
    uint32_t feature_count;
    uint32_t next_feature_id;

    /* Collective visual state */
    collective_visual_state_t instances[OCCIPITAL_COLLECTIVE_MAX_INSTANCES];
    uint32_t instance_count;

    /* Synchronization metrics */
    float gamma_sync;
    float theta_sync;
    float attention_coherence;

    /* Statistics */
    occipital_collective_stats_t stats;

    /* Timing */
    uint64_t last_update_ms;
    uint64_t last_broadcast_ms;

    /* State flags */
    bool initialized;
    bool bio_async_connected;
};

//=============================================================================
// Configuration API
//=============================================================================

void occipital_collective_default_config(occipital_collective_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(*config));

    config->attention_mode = JOINT_ATTENTION_COORDINATE;
    config->attention_threshold = 0.5f;
    config->follow_threshold = 0.7f;

    config->sharing_strategy = VISUAL_SHARE_FEATURES;
    config->share_v1_features = true;
    config->share_v2_features = true;
    config->share_v4_features = true;
    config->share_v5_features = true;

    config->gamma_sync_threshold = 0.6f;
    config->theta_sync_threshold = 0.5f;

    config->update_interval_ms = OCCIPITAL_COLLECTIVE_DEFAULT_UPDATE_MS;
    config->feature_timeout_ms = 1000;
    config->target_timeout_ms = 5000;

    config->enable_bio_async = true;
    config->enable_gaze_following = true;
    config->enable_feature_merge = true;
}

//=============================================================================
// Lifecycle API
//=============================================================================

occipital_collective_bridge_t* occipital_collective_create(
    const occipital_collective_config_t* config,
    occipital_adapter_t* occipital,
    collective_cognition_t* collective
) {
    occipital_collective_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {
        LOG_ERROR("Failed to allocate occipital-collective bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    /* Create mutex for thread safety */
    if (bridge_base_init(&bridge->base, 0, "occipital_collective") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        LOG_ERROR("Failed to create mutex for occipital-collective bridge");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "occipital_collective_create: bridge->base is NULL");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        occipital_collective_default_config(&bridge->config);
    }

    /* Connect systems */
    bridge->occipital = occipital;
    bridge->collective = collective;

    /* Initialize local state */
    bridge->local_instance_id = (uint32_t)(nimcp_time_get_us() & 0xFFFF);
    bridge->local_attention_x = 0.5f;
    bridge->local_attention_y = 0.5f;
    for (int i = 0; i < VISUAL_AREA_COUNT; ++i) {
        bridge->local_confidence[i] = 0.0f;
    }
    bridge->is_attention_leader = false;

    /* Initialize targets and features */
    bridge->target_count = 0;
    bridge->next_target_id = 1;
    bridge->feature_count = 0;
    bridge->next_feature_id = 1;
    bridge->instance_count = 0;

    /* Sync metrics */
    bridge->gamma_sync = 0.0f;
    bridge->theta_sync = 0.0f;
    bridge->attention_coherence = 0.0f;

    /* Timing */
    bridge->last_update_ms = nimcp_time_get_ms();
    bridge->last_broadcast_ms = 0;

    bridge->initialized = true;

    LOG_INFO("Occipital-collective bridge created (local_id=%u)", bridge->local_instance_id);

    return bridge;
}

void occipital_collective_destroy(occipital_collective_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "occipital_collective");

    if (bridge->bio_async_connected) {
        occipital_collective_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    LOG_INFO("Occipital-collective bridge destroyed (local_id=%u)", bridge->local_instance_id);

    nimcp_free(bridge);
}

int occipital_collective_reset(occipital_collective_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset targets and features */
    memset(bridge->targets, 0, sizeof(bridge->targets));
    bridge->target_count = 0;
    bridge->next_target_id = 1;

    memset(bridge->features, 0, sizeof(bridge->features));
    bridge->feature_count = 0;
    bridge->next_feature_id = 1;

    /* Reset collective state */
    memset(bridge->instances, 0, sizeof(bridge->instances));
    bridge->instance_count = 0;

    /* Reset sync metrics */
    bridge->gamma_sync = 0.0f;
    bridge->theta_sync = 0.0f;
    bridge->attention_coherence = 0.0f;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Reset timing */
    bridge->last_update_ms = nimcp_time_get_ms();
    bridge->last_broadcast_ms = 0;

    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_DEBUG("Occipital-collective bridge reset");

    return 0;
}

//=============================================================================
// Connection API
//=============================================================================

int occipital_collective_connect_occipital(
    occipital_collective_bridge_t* bridge,
    occipital_adapter_t* occipital
) {
    if (!bridge) {
        LOG_ERROR("Null bridge in connect_occipital");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_collective_connect_occipital: bridge is NULL");
        return -1;
    }

    bridge->occipital = occipital;

    if (occipital) {
        LOG_INFO("Connected to occipital adapter");
    } else {
        LOG_INFO("Disconnected from occipital adapter");
    }

    return 0;
}

int occipital_collective_connect_collective(
    occipital_collective_bridge_t* bridge,
    collective_cognition_t* collective
) {
    if (!bridge) {
        LOG_ERROR("Null bridge in connect_collective");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_collective_connect_collective: bridge is NULL");
        return -1;
    }

    bridge->collective = collective;

    if (collective) {
        LOG_INFO("Connected to collective cognition system");
    } else {
        LOG_INFO("Disconnected from collective cognition system");
    }

    return 0;
}

//=============================================================================
// Internal Helpers
//=============================================================================

static int find_instance_index(
    const occipital_collective_bridge_t* bridge,
    uint32_t instance_id
) {
    for (uint32_t i = 0; i < bridge->instance_count; ++i) {
        if (bridge->instances[i].instance_id == instance_id) {
            return (int)i;
        }
    }
    return -1;  /* Not found is a normal search result */
}

static int find_target_index(
    const occipital_collective_bridge_t* bridge,
    uint32_t target_id
) {
    for (uint32_t i = 0; i < bridge->target_count; ++i) {
        if (bridge->targets[i].target_id == target_id) {
            return (int)i;
        }
    }
    return -1;  /* Not found is a normal search result */
}

static void update_local_visual_state(occipital_collective_bridge_t* bridge) {
    /* Update local instance in the instances array */
    int idx = find_instance_index(bridge, bridge->local_instance_id);
    if (idx < 0) {
        if (bridge->instance_count < OCCIPITAL_COLLECTIVE_MAX_INSTANCES) {
            idx = (int)bridge->instance_count++;
        } else {
            LOG_WARNING("Cannot update local visual state: max instances (%u) reached",
                        OCCIPITAL_COLLECTIVE_MAX_INSTANCES);
            return;
        }
    }

    collective_visual_state_t* state = &bridge->instances[idx];
    state->instance_id = bridge->local_instance_id;
    state->attention_x = bridge->local_attention_x;
    state->attention_y = bridge->local_attention_y;
    state->v1_confidence = bridge->local_confidence[VISUAL_AREA_V1];
    state->v2_confidence = bridge->local_confidence[VISUAL_AREA_V2];
    state->v4_confidence = bridge->local_confidence[VISUAL_AREA_V4];
    state->v5_confidence = bridge->local_confidence[VISUAL_AREA_V5_MT];

    /* Calculate overall confidence */
    state->overall_confidence = (state->v1_confidence + state->v2_confidence +
                                 state->v4_confidence + state->v5_confidence) / 4.0f;

    state->is_leading_attention = bridge->is_attention_leader;
    state->last_update_ms = nimcp_time_get_ms();

    /* Count active targets */
    uint32_t active = 0;
    for (uint32_t i = 0; i < bridge->target_count; ++i) {
        if (bridge->targets[i].active) active++;
    }
    state->active_targets = active;
}

static void compute_attention_coherence(occipital_collective_bridge_t* bridge) {
    if (bridge->instance_count < 2) {
        bridge->attention_coherence = 1.0f;
        return;
    }

    /* Calculate average attention position */
    float avg_x = 0.0f, avg_y = 0.0f;
    for (uint32_t i = 0; i < bridge->instance_count; ++i) {
        avg_x += bridge->instances[i].attention_x;
        avg_y += bridge->instances[i].attention_y;
    }
    avg_x /= (float)bridge->instance_count;
    avg_y /= (float)bridge->instance_count;

    /* Calculate variance (lower variance = higher coherence) */
    float variance = 0.0f;
    for (uint32_t i = 0; i < bridge->instance_count; ++i) {
        float dx = bridge->instances[i].attention_x - avg_x;
        float dy = bridge->instances[i].attention_y - avg_y;
        variance += dx * dx + dy * dy;
    }
    variance /= (float)bridge->instance_count;

    /* Convert variance to coherence (0-1) */
    /* Max variance is 0.5 (corner to corner diagonal) */
    bridge->attention_coherence = 1.0f - (variance / 0.5f);
    if (bridge->attention_coherence < 0.0f) bridge->attention_coherence = 0.0f;
    if (bridge->attention_coherence > 1.0f) bridge->attention_coherence = 1.0f;

    float new_avg = (bridge->stats.avg_attention_coherence * 0.9f) +
        (bridge->attention_coherence * 0.1f);
    if (isfinite(new_avg)) bridge->stats.avg_attention_coherence = new_avg;
}

static void prune_stale_targets(occipital_collective_bridge_t* bridge) {
    uint64_t now = nimcp_time_get_ms();
    uint32_t timeout = bridge->config.target_timeout_ms;

    uint32_t write_idx = 0;
    for (uint32_t read_idx = 0; read_idx < bridge->target_count; ++read_idx) {
        joint_attention_target_t* target = &bridge->targets[read_idx];

        /* Fix potential underflow when now < created_ms */
        if (target->active && now >= target->created_ms &&
            (now - target->created_ms) < timeout) {
            if (write_idx != read_idx) {
                bridge->targets[write_idx] = bridge->targets[read_idx];
            }
            write_idx++;
        }
    }

    bridge->target_count = write_idx;
}

static void prune_stale_features(occipital_collective_bridge_t* bridge) {
    uint64_t now = nimcp_time_get_ms();
    uint32_t timeout = bridge->config.feature_timeout_ms;

    uint32_t write_idx = 0;
    for (uint32_t read_idx = 0; read_idx < bridge->feature_count; ++read_idx) {
        shared_visual_feature_t* feature = &bridge->features[read_idx];

        /* Fix potential underflow when now < timestamp_ms */
        if (now >= feature->timestamp_ms &&
            (now - feature->timestamp_ms) < timeout) {
            if (write_idx != read_idx) {
                bridge->features[write_idx] = bridge->features[read_idx];
            }
            write_idx++;
        }
    }

    bridge->feature_count = write_idx;
}

static void elect_attention_leader(occipital_collective_bridge_t* bridge) {
    /* Leader is instance with highest confidence and most active targets */
    uint32_t best_id = bridge->local_instance_id;
    float best_score = bridge->local_confidence[VISUAL_AREA_V1];

    for (uint32_t i = 0; i < bridge->instance_count; ++i) {
        const collective_visual_state_t* inst = &bridge->instances[i];
        float score = inst->overall_confidence + (float)inst->active_targets * 0.1f;

        if (score > best_score) {
            best_score = score;
            best_id = inst->instance_id;
        }
    }

    bridge->is_attention_leader = (best_id == bridge->local_instance_id);
}

//=============================================================================
// Bio-Async Message Handler
//=============================================================================

/**
 * @brief Handle incoming bio-async messages for the Occipital collective
 *
 * @param msg Pointer to message (header + payload)
 * @param msg_size Total message size
 * @param response_promise Promise for response (may be NULL)
 * @param user_data Bridge context
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t occipital_collective_message_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
);

//=============================================================================
// KG-Driven Wiring Callback
//=============================================================================

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * Called by the wiring orchestrator with message types discovered from KG
 * HANDLES_MESSAGE relations. Registers the appropriate handlers dynamically.
 */
static int occipital_collective_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;
    }

    occipital_collective_bridge_t* bridge = (occipital_collective_bridge_t*)user_data;
    if (!bridge) {
        LOG_WARNING("Wiring callback: bridge is NULL");
        return 0;
    }

    LOG_DEBUG("KG wiring callback: registering %u message handlers", message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_OCCIPITAL_COLLECTIVE_ATTENTION:
            case BIO_MSG_OCCIPITAL_COLLECTIVE_FEATURE:
            case BIO_MSG_OCCIPITAL_COLLECTIVE_STATE_UPDATE:
            case BIO_MSG_OCCIPITAL_COLLECTIVE_GAZE_FOLLOW:
                bio_router_register_handler(ctx, message_types[i],
                    occipital_collective_message_handler);
                LOG_DEBUG("Registered handler for message type 0x%04X",
                          message_types[i]);
                break;

            default:
                LOG_DEBUG("Unknown message type 0x%04X in wiring callback",
                          message_types[i]);
                break;
        }
    }

    return 0;
}

//=============================================================================
// Bio-Async Message Handler Implementation
//=============================================================================

static nimcp_error_t occipital_collective_message_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    occipital_collective_bridge_t* bridge = (occipital_collective_bridge_t*)user_data;
    if (!bridge || !msg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_collective_message_handler: required parameter is NULL (bridge, msg)");
        return -1;
    }

    (void)response_promise;  /* Currently unused - fire-and-forget messages */

    const bio_message_header_t* header = (const bio_message_header_t*)msg;
    const void* payload = (const uint8_t*)msg + sizeof(bio_message_header_t);
    size_t payload_size = msg_size - sizeof(bio_message_header_t);

    switch (header->type) {
        case BIO_MSG_OCCIPITAL_COLLECTIVE_ATTENTION: {
            /* Handle joint attention from remote instance */
            if (payload_size < sizeof(joint_attention_target_t)) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_collective_message_handler: validation failed");
                return -1;
            }
            const joint_attention_target_t* target = (const joint_attention_target_t*)payload;
            nimcp_mutex_lock(bridge->base.mutex);
            if (bridge->target_count < OCCIPITAL_COLLECTIVE_MAX_TARGETS) {
                bridge->targets[bridge->target_count++] = *target;
                bridge->stats.attention_events_recv++;
            }
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
        case BIO_MSG_OCCIPITAL_COLLECTIVE_FEATURE: {
            /* Handle shared feature from remote instance */
            if (payload_size < sizeof(shared_visual_feature_t)) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_collective_message_handler: validation failed");
                return -1;
            }
            const shared_visual_feature_t* feature = (const shared_visual_feature_t*)payload;
            nimcp_mutex_lock(bridge->base.mutex);
            if (bridge->feature_count < OCCIPITAL_COLLECTIVE_MAX_FEATURES) {
                bridge->features[bridge->feature_count++] = *feature;
                bridge->stats.features_received++;
            }
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
        case BIO_MSG_OCCIPITAL_COLLECTIVE_STATE_UPDATE: {
            /* Handle visual state update from remote instance */
            if (payload_size < sizeof(collective_visual_state_t)) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_collective_message_handler: validation failed");
                return -1;
            }
            const collective_visual_state_t* state = (const collective_visual_state_t*)payload;
            nimcp_mutex_lock(bridge->base.mutex);
            int idx = find_instance_index(bridge, state->instance_id);
            if (idx < 0 && bridge->instance_count < OCCIPITAL_COLLECTIVE_MAX_INSTANCES) {
                idx = (int)bridge->instance_count++;
            }
            if (idx >= 0) {
                bridge->instances[idx] = *state;
            }
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
        default:
            LOG_DEBUG("Unknown message type: 0x%04X", header->type);
            return 0;  /* Unknown message type is not an error worth throwing */
    }
}

//=============================================================================
// Update API
//=============================================================================

int occipital_collective_update(
    occipital_collective_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_mutex_lock(bridge->base.mutex);

    (void)delta_ms;

    uint64_t now = nimcp_time_get_ms();

    /* Update local visual state from occipital adapter */
    if (bridge->occipital) {
        /* Would query occipital adapter for V1-V5 confidence */
        /* For now, maintain current values */
    }

    /* Update local state */
    update_local_visual_state(bridge);

    /* Prune stale data */
    prune_stale_targets(bridge);
    prune_stale_features(bridge);

    /* Compute coherence */
    compute_attention_coherence(bridge);

    /* Elect attention leader */
    elect_attention_leader(bridge);

    /* Broadcast state if interval elapsed */
    if ((now - bridge->last_broadcast_ms) >= bridge->config.update_interval_ms) {
        /* Would broadcast via bio-async or collective cognition messaging */
        bridge->last_broadcast_ms = now;
    }

    bridge->last_update_ms = now;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Joint Attention API
//=============================================================================

int occipital_collective_initiate_attention(
    occipital_collective_bridge_t* bridge,
    float x,
    float y,
    float salience,
    uint32_t* target_id
) {
    if (!bridge || !target_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_collective_initiate_attention: required parameter is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->target_count >= OCCIPITAL_COLLECTIVE_MAX_TARGETS) {
        nimcp_mutex_unlock(bridge->base.mutex);
        LOG_WARNING("Cannot initiate attention: max targets reached");
        /* Capacity limit — normal operational bound, not immune-worthy */
        return -1;
    }

    joint_attention_target_t* target = &bridge->targets[bridge->target_count++];
    memset(target, 0, sizeof(*target));

    target->target_id = bridge->next_target_id++;
    target->x = x;
    target->y = y;
    target->salience = salience;
    target->attending_count = 1;
    target->initiator_id = bridge->local_instance_id;
    target->created_ms = nimcp_time_get_ms();
    target->active = true;

    *target_id = target->target_id;

    /* Update local attention */
    bridge->local_attention_x = x;
    bridge->local_attention_y = y;

    bridge->stats.joint_targets_created++;
    bridge->stats.attention_events_sent++;

    /* Copy target for broadcast (while still holding mutex) */
    joint_attention_target_t target_copy = *target;
    bool should_broadcast = bridge->bio_async_connected && bridge->bio_ctx;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Broadcast attention via bio-async if connected */
    if (should_broadcast) {
        /* Prepare message with header */
        uint8_t msg_buf[sizeof(bio_message_header_t) + sizeof(joint_attention_target_t)];
        bio_message_header_t* header = (bio_message_header_t*)msg_buf;
        memset(header, 0, sizeof(*header));
        header->type = BIO_MSG_OCCIPITAL_COLLECTIVE_ATTENTION;
        header->source_module = BIO_MODULE_OCCIPITAL_COLLECTIVE;
        header->target_module = 0;  /* Broadcast */
        header->payload_size = sizeof(joint_attention_target_t);
        header->channel = BIO_CHANNEL_ACETYLCHOLINE;  /* Attention uses ACh */
        header->flags = BIO_MSG_FLAG_BROADCAST;

        memcpy(msg_buf + sizeof(bio_message_header_t), &target_copy, sizeof(joint_attention_target_t));

        bio_router_broadcast(bridge->bio_ctx, msg_buf, sizeof(msg_buf));
    }

    LOG_DEBUG("Initiated joint attention: target_id=%u at (%.2f, %.2f)",
              target_copy.target_id, x, y);

    return 0;
}

int occipital_collective_follow_attention(
    occipital_collective_bridge_t* bridge,
    uint32_t target_id
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_mutex_lock(bridge->base.mutex);

    int idx = find_target_index(bridge, target_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        LOG_WARNING("Cannot follow attention: target %u not found", target_id);
        /* Target not found — normal search miss, not immune-worthy */
        return -1;
    }

    joint_attention_target_t* target = &bridge->targets[idx];
    target->attending_count++;

    /* Update local attention to target */
    bridge->local_attention_x = target->x;
    bridge->local_attention_y = target->y;

    bridge->stats.gaze_follows++;
    bridge->stats.attention_events_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_DEBUG("Following joint attention: target_id=%u", target_id);

    return 0;
}

int occipital_collective_release_attention(
    occipital_collective_bridge_t* bridge,
    uint32_t target_id
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_mutex_lock(bridge->base.mutex);

    int idx = find_target_index(bridge, target_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        /* Target not found — may have been pruned, not immune-worthy */
        return -1;
    }

    joint_attention_target_t* target = &bridge->targets[idx];
    if (target->attending_count > 0) {
        target->attending_count--;
    }

    if (target->attending_count == 0) {
        target->active = false;
    }

    bridge->stats.attention_events_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int occipital_collective_get_targets(
    const occipital_collective_bridge_t* bridge,
    joint_attention_target_t* targets,
    uint32_t max_targets,
    uint32_t* count
) {
    if (!bridge || !targets || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_collective_get_targets: required parameter is NULL");
        return -1;
    }

    nimcp_mutex_lock(((occipital_collective_bridge_t*)bridge)->base.mutex);

    uint32_t copy_count = bridge->target_count;
    if (copy_count > max_targets) copy_count = max_targets;

    memcpy(targets, bridge->targets, copy_count * sizeof(joint_attention_target_t));
    *count = copy_count;

    nimcp_mutex_unlock(((occipital_collective_bridge_t*)bridge)->base.mutex);

    return 0;
}

//=============================================================================
// Feature Sharing API
//=============================================================================

int occipital_collective_share_feature(
    occipital_collective_bridge_t* bridge,
    const shared_visual_feature_t* feature
) {
    if (!bridge || !feature) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_collective_share_feature: required parameter is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->feature_count >= OCCIPITAL_COLLECTIVE_MAX_FEATURES) {
        LOG_DEBUG("Feature buffer full, evicting oldest feature");
        /* Evict oldest feature */
        memmove(&bridge->features[0], &bridge->features[1],
                (OCCIPITAL_COLLECTIVE_MAX_FEATURES - 1) * sizeof(shared_visual_feature_t));
        bridge->feature_count--;
    }

    shared_visual_feature_t* stored = &bridge->features[bridge->feature_count++];
    *stored = *feature;
    stored->feature_id = bridge->next_feature_id++;
    stored->source_instance = bridge->local_instance_id;
    stored->timestamp_ms = nimcp_time_get_ms();

    bridge->stats.features_shared++;

    /* Copy feature for broadcast (while still holding mutex) */
    shared_visual_feature_t feature_copy = *stored;
    bool should_broadcast = bridge->bio_async_connected && bridge->bio_ctx;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Broadcast feature via bio-async if connected */
    if (should_broadcast) {
        /* Prepare message with header */
        uint8_t msg_buf[sizeof(bio_message_header_t) + sizeof(shared_visual_feature_t)];
        bio_message_header_t* header = (bio_message_header_t*)msg_buf;
        memset(header, 0, sizeof(*header));
        header->type = BIO_MSG_OCCIPITAL_COLLECTIVE_FEATURE;
        header->source_module = BIO_MODULE_OCCIPITAL_COLLECTIVE;
        header->target_module = 0;  /* Broadcast */
        header->payload_size = sizeof(shared_visual_feature_t);
        header->channel = BIO_CHANNEL_DOPAMINE;  /* Visual features use DA */
        header->flags = BIO_MSG_FLAG_BROADCAST;

        memcpy(msg_buf + sizeof(bio_message_header_t), &feature_copy, sizeof(shared_visual_feature_t));

        bio_router_broadcast(bridge->bio_ctx, msg_buf, sizeof(msg_buf));
    }

    return 0;
}

int occipital_collective_get_features(
    const occipital_collective_bridge_t* bridge,
    int area,
    shared_visual_feature_t* features,
    uint32_t max_features,
    uint32_t* count
) {
    if (!bridge || !features || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_collective_get_features: required parameter is NULL");
        return -1;
    }

    nimcp_mutex_lock(((occipital_collective_bridge_t*)bridge)->base.mutex);

    uint32_t copied = 0;
    for (uint32_t i = 0; i < bridge->feature_count && copied < max_features; ++i) {
        const shared_visual_feature_t* f = &bridge->features[i];

        if (area < 0 || (int)f->source_area == area) {
            features[copied++] = *f;
        }
    }

    *count = copied;

    nimcp_mutex_unlock(((occipital_collective_bridge_t*)bridge)->base.mutex);

    return 0;
}

int occipital_collective_merge_features(
    occipital_collective_bridge_t* bridge,
    float tolerance
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_feature_merge) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    int merged = 0;

    /* Simple O(n^2) merge - for each pair of features */
    for (uint32_t i = 0; i < bridge->feature_count; ++i) {
        shared_visual_feature_t* f1 = &bridge->features[i];
        if (f1->feature_id == 0) continue;  /* Already merged */

        for (uint32_t j = i + 1; j < bridge->feature_count; ++j) {
            shared_visual_feature_t* f2 = &bridge->features[j];
            if (f2->feature_id == 0) continue;
            if (f1->source_area != f2->source_area) continue;

            /* Check if features are close enough to merge */
            float dx = f1->x - f2->x;
            float dy = f1->y - f2->y;
            float dist = sqrtf(dx * dx + dy * dy);

            if (dist < tolerance) {
                /* Merge f2 into f1 - include all fields */
                f1->confidence = (f1->confidence + f2->confidence) / 2.0f;
                f1->x = (f1->x + f2->x) / 2.0f;
                f1->y = (f1->y + f2->y) / 2.0f;
                f1->orientation = (f1->orientation + f2->orientation) / 2.0f;
                f1->scale = (f1->scale + f2->scale) / 2.0f;

                /* Mark f2 as merged */
                f2->feature_id = 0;
                merged++;
            }
        }
    }

    /* Compact the array */
    uint32_t write_idx = 0;
    for (uint32_t read_idx = 0; read_idx < bridge->feature_count; ++read_idx) {
        if (bridge->features[read_idx].feature_id != 0) {
            if (write_idx != read_idx) {
                bridge->features[write_idx] = bridge->features[read_idx];
            }
            write_idx++;
        }
    }
    bridge->feature_count = write_idx;

    nimcp_mutex_unlock(bridge->base.mutex);

    return merged;
}

//=============================================================================
// Query API
//=============================================================================

int occipital_collective_get_summary(
    const occipital_collective_bridge_t* bridge,
    collective_visual_summary_t* summary
) {
    if (!bridge || !summary) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_collective_get_summary: required parameter is NULL");
        return -1;
    }

    nimcp_mutex_lock(((occipital_collective_bridge_t*)bridge)->base.mutex);

    memset(summary, 0, sizeof(*summary));

    summary->total_instances = bridge->instance_count;
    summary->shared_feature_count = bridge->feature_count;
    summary->attention_coherence = bridge->attention_coherence;
    summary->gamma_sync = bridge->gamma_sync;

    /* Count active targets */
    for (uint32_t i = 0; i < bridge->target_count; ++i) {
        if (bridge->targets[i].active) {
            summary->active_joint_targets++;
        }
    }

    /* Calculate average confidence */
    float total_conf = 0.0f;
    for (uint32_t i = 0; i < bridge->instance_count; ++i) {
        total_conf += bridge->instances[i].overall_confidence;
    }
    if (bridge->instance_count > 0) {
        summary->average_confidence = total_conf / (float)bridge->instance_count;
    }

    /* Check if collective is attending same target */
    summary->collective_attending = (bridge->attention_coherence > 0.8f);

    /* Find attention leader */
    for (uint32_t i = 0; i < bridge->instance_count; ++i) {
        if (bridge->instances[i].is_leading_attention) {
            summary->attention_leader_id = bridge->instances[i].instance_id;
            break;
        }
    }

    nimcp_mutex_unlock(((occipital_collective_bridge_t*)bridge)->base.mutex);

    return 0;
}

int occipital_collective_get_instance_state(
    const occipital_collective_bridge_t* bridge,
    uint32_t instance_id,
    collective_visual_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_collective_get_instance_state: required parameter is NULL");
        return -1;
    }

    nimcp_mutex_lock(((occipital_collective_bridge_t*)bridge)->base.mutex);

    int idx = find_instance_index(bridge, instance_id);
    if (idx < 0) {
        nimcp_mutex_unlock(((occipital_collective_bridge_t*)bridge)->base.mutex);
        /* Instance not found — normal query miss, not immune-worthy */
        return -1;
    }

    *state = bridge->instances[idx];

    nimcp_mutex_unlock(((occipital_collective_bridge_t*)bridge)->base.mutex);

    return 0;
}

uint32_t occipital_collective_get_local_id(
    const occipital_collective_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_collective_get_local_id: bridge is NULL");
        return 0;
    }

    nimcp_mutex_lock(((occipital_collective_bridge_t*)bridge)->base.mutex);
    uint32_t id = bridge->local_instance_id;
    nimcp_mutex_unlock(((occipital_collective_bridge_t*)bridge)->base.mutex);

    return id;
}

bool occipital_collective_is_attention_leader(
    const occipital_collective_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(((occipital_collective_bridge_t*)bridge)->base.mutex);
    bool is_leader = bridge->is_attention_leader;
    nimcp_mutex_unlock(((occipital_collective_bridge_t*)bridge)->base.mutex);

    return is_leader;
}

float occipital_collective_get_coherence(
    const occipital_collective_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_collective_get_coherence: bridge is NULL");
        return 0.0f;
    }

    nimcp_mutex_lock(((occipital_collective_bridge_t*)bridge)->base.mutex);
    float coherence = bridge->attention_coherence;
    nimcp_mutex_unlock(((occipital_collective_bridge_t*)bridge)->base.mutex);

    return coherence;
}

//=============================================================================
// Statistics API
//=============================================================================

int occipital_collective_get_stats(
    const occipital_collective_bridge_t* bridge,
    occipital_collective_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_collective_get_stats: required parameter is NULL");
        return -1;
    }

    nimcp_mutex_lock(((occipital_collective_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(((occipital_collective_bridge_t*)bridge)->base.mutex);

    return 0;
}

void occipital_collective_reset_stats(occipital_collective_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_collective_reset_stats: bridge is NULL");
        return;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
}

//=============================================================================
// Bio-Async API
//=============================================================================

int occipital_collective_connect_bio_async(occipital_collective_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* If already connected, disconnect first */
    if (bridge->bio_async_connected && bridge->bio_ctx) {
        nimcp_mutex_unlock(bridge->base.mutex);
        occipital_collective_disconnect_bio_async(bridge);
        nimcp_mutex_lock(bridge->base.mutex);
    }

    /* Check if router is initialized */
    if (!bio_router_is_initialized()) {
        LOG_WARNING("Bio-async router not initialized, skipping registration");
        bridge->bio_async_connected = false;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Register with the bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_OCCIPITAL_COLLECTIVE,
        .module_name = "occipital_collective_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->bio_ctx = bio_router_register_module(&info);
    if (!bridge->bio_ctx) {
        LOG_WARNING("Failed to register with bio-async router");
        bridge->bio_async_connected = false;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_collective_connect_bio_async: bridge->bio_ctx is NULL");
        return -1;
    }

    /* Try KG-driven wiring callback first */
    nimcp_error_t cb_result = bio_router_register_wiring_callback(
        BIO_MODULE_OCCIPITAL_COLLECTIVE,
        (void*)occipital_collective_wiring_handler_callback,
        bridge
    );

    if (cb_result != NIMCP_SUCCESS) {
        /* Fall back to legacy direct registration */
        LOG_DEBUG("KG wiring not available, using legacy registration");

        LEGACY_HANDLER_REGISTRATION(
            bio_router_register_handler(bridge->bio_ctx,
                BIO_MSG_OCCIPITAL_COLLECTIVE_ATTENTION,
                occipital_collective_message_handler)
        );
        LEGACY_HANDLER_REGISTRATION(
            bio_router_register_handler(bridge->bio_ctx,
                BIO_MSG_OCCIPITAL_COLLECTIVE_FEATURE,
                occipital_collective_message_handler)
        );
        LEGACY_HANDLER_REGISTRATION(
            bio_router_register_handler(bridge->bio_ctx,
                BIO_MSG_OCCIPITAL_COLLECTIVE_STATE_UPDATE,
                occipital_collective_message_handler)
        );
        LEGACY_HANDLER_REGISTRATION(
            bio_router_register_handler(bridge->bio_ctx,
                BIO_MSG_OCCIPITAL_COLLECTIVE_GAZE_FOLLOW,
                occipital_collective_message_handler)
        );
    }

    bridge->bio_async_connected = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_INFO("Connected to bio-async router (module_id=0x%04X)", BIO_MODULE_OCCIPITAL_COLLECTIVE);
    return 0;
}

int occipital_collective_disconnect_bio_async(occipital_collective_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->bio_async_connected && bridge->bio_ctx) {
        /* Unregister handlers first */
        bio_router_unregister_handler(bridge->bio_ctx, BIO_MSG_OCCIPITAL_COLLECTIVE_ATTENTION);
        bio_router_unregister_handler(bridge->bio_ctx, BIO_MSG_OCCIPITAL_COLLECTIVE_FEATURE);
        bio_router_unregister_handler(bridge->bio_ctx, BIO_MSG_OCCIPITAL_COLLECTIVE_STATE_UPDATE);
        bio_router_unregister_handler(bridge->bio_ctx, BIO_MSG_OCCIPITAL_COLLECTIVE_GAZE_FOLLOW);

        /* Unregister module */
        bio_router_unregister_module(bridge->bio_ctx);
        bridge->bio_ctx = NULL;
    }

    bridge->bio_async_connected = false;

    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_INFO("Disconnected from bio-async router");
    return 0;
}
