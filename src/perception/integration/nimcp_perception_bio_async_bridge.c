/**
 * @file nimcp_perception_bio_async_bridge.c
 * @brief Implementation of Perception Module Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "perception/integration/nimcp_perception_bio_async_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(perception_bio_async_bridge)

#define LOG_MODULE "PERCEPTION_BIO_ASYNC_BRIDGE"


/* ============================================================================
 * Internal Bridge Structure
 * ============================================================================ */

struct perception_bio_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    perception_bio_bridge_config_t config;
    bio_router_t router;

    /* Per-modality subscription registries */
    percept_bio_subscription_t* subscriptions[PERCEPT_MODALITY_COUNT];
    uint32_t subscription_count[PERCEPT_MODALITY_COUNT];
    uint32_t subscription_capacity[PERCEPT_MODALITY_COUNT];

    /* Connection state */
    bool connected;
    bio_module_context_t module_contexts[PERCEPT_MODALITY_COUNT + 1]; /* +1 for binding */

    /* Timing state */
    uint64_t last_broadcast_us;
    uint32_t time_since_visual_broadcast_ms;
    uint32_t time_since_auditory_broadcast_ms;
    uint32_t time_since_somato_broadcast_ms;
    uint32_t time_since_binding_broadcast_ms;

    /* Binding state */
    uint32_t next_binding_id;
    uint32_t next_feature_id;
    uint32_t next_object_id;

    /* Statistics */
    perception_bio_async_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static percept_bio_subscription_t* find_subscription(
    perception_bio_bridge_t* b,
    percept_modality_t modality,
    uint32_t module_id
) {
    if (modality >= PERCEPT_MODALITY_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "find_subscription: modality out of range");
        return NULL;
    }

    for (uint32_t i = 0; i < b->subscription_count[modality]; i++) {
        if (b->subscriptions[modality][i].module_id == module_id &&
            b->subscriptions[modality][i].active) {
            return &b->subscriptions[modality][i];
        }
    }
    return NULL;  /* Not found - normal lookup behavior */
}

static int count_subscribers_for_type(
    const perception_bio_bridge_t* b,
    percept_bio_msg_type_t type
) {
    int count = 0;
    uint32_t mask_bit = (type < 32) ? (1U << type) : 0;
    uint32_t mask_bit_high = (type >= 32 && type < 64) ? (1U << (type - 32)) : 0;

    for (int m = 0; m < PERCEPT_MODALITY_COUNT; m++) {
        for (uint32_t i = 0; i < b->subscription_count[m]; i++) {
            if (!b->subscriptions[m][i].active) continue;

            bool matches = false;
            if (type < 32) {
                matches = (b->subscriptions[m][i].msg_type_mask_low & mask_bit) != 0;
            } else if (type < 64) {
                matches = (b->subscriptions[m][i].msg_type_mask_high & mask_bit_high) != 0;
            }

            if (matches) count++;
        }
    }
    return count;
}

static void init_percept_header(
    percept_message_header_t* header,
    percept_bio_msg_type_t type,
    percept_modality_t modality,
    nimcp_bio_channel_type_t channel,
    float salience
) {
    memset(header, 0, sizeof(*header));
    header->bio_header.type = BIO_MSG_VISUAL_FEATURE_DETECTED + type;
    header->bio_header.source_module = BIO_MODULE_ID_PERCEPTION + (modality * 0x10);
    header->bio_header.channel = channel;
    header->bio_header.flags = 0;
    header->bio_header.timestamp_us = get_timestamp_us();
    header->percept_type = type;
    header->modality = modality;
    header->salience = salience;
    header->timestamp_us = get_timestamp_us();
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int perception_bio_async_default_config(perception_bio_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_default_config: config is NULL");
        return -1;
    }

    /* Broadcast timing - aligned with typical perceptual refresh rates */
    config->visual_broadcast_interval_ms = PERCEPT_BIO_DEFAULT_BROADCAST_INTERVAL_MS;
    config->auditory_broadcast_interval_ms = 20;   /* ~50 Hz for audio */
    config->somato_broadcast_interval_ms = 50;     /* 20 Hz for somatosensory */
    config->binding_broadcast_interval_ms = 100;   /* 10 Hz for binding */
    config->enable_auto_broadcast = true;

    /* Message handling */
    config->max_inbox_process_per_update = 64;
    config->message_ttl_ms = PERCEPT_BIO_MESSAGE_TTL_MS;

    /* Priority settings - using neuromodulator channels */
    config->salience_urgent_threshold = PERCEPT_BIO_SALIENCE_URGENT_THRESHOLD;
    config->default_channel = BIO_CHANNEL_ACETYLCHOLINE;    /* Fast attention */
    config->feature_channel = BIO_CHANNEL_DOPAMINE;         /* Feature/reward */
    config->object_channel = BIO_CHANNEL_SEROTONIN;         /* Object state */
    config->urgent_channel = BIO_CHANNEL_NOREPINEPHRINE;    /* Urgent alerts */

    /* Subscription limits */
    config->max_subscriptions_per_modality = PERCEPT_BIO_MAX_SUBSCRIPTIONS;

    /* Feature flags - enable all modalities */
    config->enable_visual_routing = true;
    config->enable_auditory_routing = true;
    config->enable_somato_routing = true;
    config->enable_olfactory_routing = true;
    config->enable_gustatory_routing = true;

    /* Cross-modal integration */
    config->enable_binding = true;
    config->enable_salience_integration = true;
    config->enable_prediction_error = true;

    /* Debugging */
    config->enable_logging = false;

    return 0;
}

perception_bio_bridge_t* perception_bio_async_bridge_create(
    const perception_bio_bridge_config_t* config
) {
    perception_bio_bridge_t* bridge = nimcp_calloc(1, sizeof(perception_bio_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        perception_bio_async_default_config(&bridge->config);
    }

    /* Allocate per-modality subscription arrays */
    for (int m = 0; m < PERCEPT_MODALITY_COUNT; m++) {
        bridge->subscription_capacity[m] = bridge->config.max_subscriptions_per_modality;
        bridge->subscriptions[m] = nimcp_calloc(
            bridge->subscription_capacity[m],
            sizeof(percept_bio_subscription_t)
        );
        if (!bridge->subscriptions[m]) {
            /* Cleanup on failure */
            for (int j = 0; j < m; j++) {
                nimcp_free(bridge->subscriptions[j]);
            }
            nimcp_free(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_bridge_create: bridge->subscriptions is NULL");
            return NULL;
        }
        bridge->subscription_count[m] = 0;
    }

    /* Initialize state */
    bridge->last_broadcast_us = get_timestamp_us();
    bridge->next_binding_id = 1;
    bridge->next_feature_id = 1;
    bridge->next_object_id = 1;

    return bridge;
}

void perception_bio_async_bridge_destroy(perception_bio_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "perception_bio_async");

    /* Disconnect if connected */
    if (bridge->connected) {
        perception_bio_async_disconnect(bridge);
    }

    /* Free subscription arrays */
    for (int m = 0; m < PERCEPT_MODALITY_COUNT; m++) {
        nimcp_free(bridge->subscriptions[m]);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int perception_bio_async_connect(
    perception_bio_bridge_t* bridge,
    bio_router_t router
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_connect: bridge is NULL");
        return -1;
    }

    bridge->router = router;
    bridge->connected = true;

    /* Register module contexts would happen here if router is active */
    /* For now, just track connection state */

    return 0;
}

int perception_bio_async_disconnect(perception_bio_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    bridge->router = NULL;
    bridge->connected = false;

    /* Clear module contexts */
    for (int i = 0; i <= PERCEPT_MODALITY_COUNT; i++) {
        bridge->module_contexts[i] = NULL;
    }

    return 0;
}

bool perception_bio_async_is_connected(const perception_bio_bridge_t* bridge) {
    return bridge ? bridge->connected : false;
}

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

int perception_bio_async_process_inbox(
    perception_bio_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_process_inbox: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    uint32_t processed = 0;
    (void)max_messages;

    /* Process incoming attention modulation and prediction update requests */
    /* TODO: Integrate with bio_router inbox when available */

    bridge->stats.messages_received += processed;
    return (int)processed;
}

int perception_bio_async_update(
    perception_bio_bridge_t* bridge,
    uint32_t delta_ms
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    /* Update timing counters */
    bridge->time_since_visual_broadcast_ms += delta_ms;
    bridge->time_since_auditory_broadcast_ms += delta_ms;
    bridge->time_since_somato_broadcast_ms += delta_ms;
    bridge->time_since_binding_broadcast_ms += delta_ms;

    /* Auto-broadcast handling would go here */
    if (bridge->config.enable_auto_broadcast) {
        /* Reset timers when broadcast intervals elapse */
        if (bridge->time_since_visual_broadcast_ms >=
            bridge->config.visual_broadcast_interval_ms) {
            bridge->time_since_visual_broadcast_ms = 0;
        }
        if (bridge->time_since_auditory_broadcast_ms >=
            bridge->config.auditory_broadcast_interval_ms) {
            bridge->time_since_auditory_broadcast_ms = 0;
        }
    }

    return 0;
}

/* ============================================================================
 * Visual Perception Broadcast API
 * ============================================================================ */

int perception_bio_async_broadcast_visual_feature(
    perception_bio_bridge_t* bridge,
    visual_feature_type_t feature_type,
    const float* position,
    float strength,
    const void* feature_data
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_visual_feature: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_visual_feature: position is NULL");
        return -1;
    }
    if (!bridge->config.enable_visual_routing) return 0;

    percept_visual_feature_msg_t msg = {0};
    init_percept_header(&msg.header, PERCEPT_MSG_VISUAL_FEATURE,
                        PERCEPT_MODALITY_VISUAL,
                        bridge->config.feature_channel, strength);

    msg.feature_type = feature_type;
    msg.position[0] = position[0];
    msg.position[1] = position[1];
    msg.strength = strength;
    msg.feature_id = bridge->next_feature_id++;

    /* Copy feature-specific data if provided */
    if (feature_data) {
        switch (feature_type) {
            case VISUAL_FEATURE_EDGE:
                memcpy(&msg.data.edge, feature_data, sizeof(msg.data.edge));
                break;
            case VISUAL_FEATURE_COLOR:
                memcpy(&msg.data.color, feature_data, sizeof(msg.data.color));
                break;
            case VISUAL_FEATURE_MOTION:
                memcpy(&msg.data.motion, feature_data, sizeof(msg.data.motion));
                break;
            case VISUAL_FEATURE_DEPTH:
                memcpy(&msg.data.depth, feature_data, sizeof(msg.data.depth));
                break;
            default:
                break;
        }
    }

    /* Count subscribers and update stats */
    int subs = count_subscribers_for_type(bridge, PERCEPT_MSG_VISUAL_FEATURE);
    if (subs > 0) {
        bridge->stats.modality_stats[PERCEPT_MODALITY_VISUAL].features_sent++;
        bridge->stats.messages_sent++;
        bridge->stats.broadcasts_sent++;
    }

    bridge->stats.last_broadcast_time_us = msg.header.timestamp_us;
    return 0;
}

int perception_bio_async_broadcast_visual_object(
    perception_bio_bridge_t* bridge,
    uint32_t object_id,
    uint32_t category_id,
    const char* label,
    const float* position,
    float confidence
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_visual_object: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_visual_object: position is NULL");
        return -1;
    }
    if (!bridge->config.enable_visual_routing) return 0;

    percept_object_detected_msg_t msg = {0};
    init_percept_header(&msg.header, PERCEPT_MSG_OBJECT_DETECTED,
                        PERCEPT_MODALITY_VISUAL,
                        bridge->config.object_channel, confidence);

    msg.object_id = object_id ? object_id : bridge->next_object_id++;
    msg.category_id = category_id;
    if (label) {
        strncpy(msg.object_label, label, sizeof(msg.object_label) - 1);
    }
    msg.position[0] = position[0];
    msg.position[1] = position[1];
    msg.position[2] = position[2];
    msg.confidence = confidence;
    msg.primary_modality = PERCEPT_MODALITY_VISUAL;
    msg.modality_mask = (1U << PERCEPT_MODALITY_VISUAL);
    msg.binding_state = BINDING_STATE_NONE;
    msg.first_seen_us = get_timestamp_us();
    msg.last_updated_us = msg.first_seen_us;

    /* Update stats */
    bridge->stats.modality_stats[PERCEPT_MODALITY_VISUAL].objects_detected++;
    bridge->stats.objects_tracked++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.header.timestamp_us;

    return 0;
}

int perception_bio_async_broadcast_visual_attention(
    perception_bio_bridge_t* bridge,
    const float* new_target,
    float salience
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_visual_attention: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!new_target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_visual_attention: new_target is NULL");
        return -1;
    }

    percept_message_header_t header = {0};
    nimcp_bio_channel_type_t channel = (salience >= bridge->config.salience_urgent_threshold)
        ? bridge->config.urgent_channel
        : bridge->config.default_channel;

    init_percept_header(&header, PERCEPT_MSG_VISUAL_ATTENTION_SHIFT,
                        PERCEPT_MODALITY_VISUAL, channel, salience);

    if (salience >= bridge->config.salience_urgent_threshold) {
        header.bio_header.flags |= BIO_MSG_FLAG_URGENT;
    }

    /* Update stats */
    bridge->stats.modality_stats[PERCEPT_MODALITY_VISUAL].attention_shifts++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = header.timestamp_us;

    (void)new_target;  /* Used in full implementation */
    return 0;
}

/* ============================================================================
 * Auditory Perception Broadcast API
 * ============================================================================ */

int perception_bio_async_broadcast_auditory_feature(
    perception_bio_bridge_t* bridge,
    auditory_feature_type_t feature_type,
    float frequency_hz,
    float amplitude,
    float duration_ms
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_auditory_feature: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_auditory_routing) return 0;

    percept_auditory_feature_msg_t msg = {0};
    init_percept_header(&msg.header, PERCEPT_MSG_AUDITORY_FEATURE,
                        PERCEPT_MODALITY_AUDITORY,
                        bridge->config.feature_channel, amplitude);

    msg.feature_type = feature_type;
    msg.frequency_hz = frequency_hz;
    msg.amplitude = amplitude;
    msg.duration_ms = duration_ms;
    msg.feature_id = bridge->next_feature_id++;

    /* Update stats */
    bridge->stats.modality_stats[PERCEPT_MODALITY_AUDITORY].features_sent++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.header.timestamp_us;

    return 0;
}

int perception_bio_async_broadcast_word_recognized(
    perception_bio_bridge_t* bridge,
    const char* word_text,
    float confidence
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_word_recognized: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!word_text) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_word_recognized: word_text is NULL");
        return -1;
    }
    if (!bridge->config.enable_auditory_routing) return 0;

    percept_auditory_feature_msg_t msg = {0};
    init_percept_header(&msg.header, PERCEPT_MSG_AUDITORY_WORD,
                        PERCEPT_MODALITY_AUDITORY,
                        bridge->config.object_channel, confidence);

    msg.feature_type = AUDITORY_FEATURE_WORD;
    msg.feature_id = bridge->next_feature_id++;
    strncpy(msg.data.word.word_text, word_text, sizeof(msg.data.word.word_text) - 1);
    msg.data.word.word_confidence = confidence;

    /* Update stats */
    bridge->stats.modality_stats[PERCEPT_MODALITY_AUDITORY].features_sent++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.header.timestamp_us;

    return 0;
}

int perception_bio_async_broadcast_auditory_spatial(
    perception_bio_bridge_t* bridge,
    float azimuth,
    float elevation,
    float distance
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_auditory_spatial: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_auditory_routing) return 0;

    percept_auditory_feature_msg_t msg = {0};
    init_percept_header(&msg.header, PERCEPT_MSG_AUDITORY_SPATIAL,
                        PERCEPT_MODALITY_AUDITORY,
                        bridge->config.default_channel, 0.5f);

    msg.feature_type = AUDITORY_FEATURE_SPATIAL;
    msg.azimuth = azimuth;
    msg.elevation = elevation;
    msg.distance = distance;
    msg.feature_id = bridge->next_feature_id++;

    /* Update stats */
    bridge->stats.modality_stats[PERCEPT_MODALITY_AUDITORY].features_sent++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.header.timestamp_us;

    return 0;
}

/* ============================================================================
 * Somatosensory Perception Broadcast API
 * ============================================================================ */

int perception_bio_async_broadcast_touch(
    perception_bio_bridge_t* bridge,
    uint32_t body_part,
    const float* position,
    float pressure
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_touch: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_touch: position is NULL");
        return -1;
    }
    if (!bridge->config.enable_somato_routing) return 0;

    percept_message_header_t header = {0};
    init_percept_header(&header, PERCEPT_MSG_SOMATO_TOUCH,
                        PERCEPT_MODALITY_SOMATOSENSORY,
                        bridge->config.default_channel, pressure);

    /* Update stats */
    bridge->stats.modality_stats[PERCEPT_MODALITY_SOMATOSENSORY].features_sent++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = header.timestamp_us;

    (void)body_part;
    (void)position;
    return 0;
}

int perception_bio_async_broadcast_pain(
    perception_bio_bridge_t* bridge,
    uint32_t body_part,
    float intensity,
    bool is_urgent
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_pain: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_somato_routing) return 0;

    percept_message_header_t header = {0};
    nimcp_bio_channel_type_t channel = is_urgent
        ? bridge->config.urgent_channel
        : bridge->config.default_channel;

    init_percept_header(&header, PERCEPT_MSG_SOMATO_PAIN,
                        PERCEPT_MODALITY_SOMATOSENSORY, channel, intensity);

    if (is_urgent) {
        header.bio_header.flags |= BIO_MSG_FLAG_URGENT;
    }

    /* Update stats */
    bridge->stats.modality_stats[PERCEPT_MODALITY_SOMATOSENSORY].features_sent++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = header.timestamp_us;

    (void)body_part;
    return 0;
}

int perception_bio_async_broadcast_proprioception(
    perception_bio_bridge_t* bridge,
    uint32_t body_part,
    const float* position,
    const float* velocity
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_proprioception: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!position || !velocity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_proprioception: required parameter is NULL (position, velocity)");
        return -1;
    }
    if (!bridge->config.enable_somato_routing) return 0;

    percept_message_header_t header = {0};
    init_percept_header(&header, PERCEPT_MSG_SOMATO_PROPRIOCEPTION,
                        PERCEPT_MODALITY_SOMATOSENSORY,
                        bridge->config.default_channel, 0.5f);

    /* Update stats */
    bridge->stats.modality_stats[PERCEPT_MODALITY_SOMATOSENSORY].features_sent++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = header.timestamp_us;

    (void)body_part;
    return 0;
}

/* ============================================================================
 * Cross-Modal Integration API
 * ============================================================================ */

uint32_t perception_bio_async_request_binding(
    perception_bio_bridge_t* bridge,
    const percept_modality_t* modalities,
    uint32_t modality_count,
    const uint32_t* feature_ids,
    uint32_t temporal_window_ms
) {
    if (!bridge || !bridge->connected) return 0;
    if (!modalities || !feature_ids || modality_count == 0) return 0;
    if (!bridge->config.enable_binding) return 0;

    percept_binding_request_msg_t msg = {0};
    init_percept_header(&msg.header, PERCEPT_MSG_BINDING_REQUEST,
                        PERCEPT_MODALITY_VISUAL,  /* Binding is typically visual-centric */
                        bridge->config.default_channel, 0.5f);

    msg.binding_id = bridge->next_binding_id++;
    msg.modality_count = (modality_count > PERCEPT_BIO_MAX_MODALITIES)
        ? PERCEPT_BIO_MAX_MODALITIES : modality_count;

    for (uint32_t i = 0; i < msg.modality_count; i++) {
        msg.modalities[i] = modalities[i];

        /* Assign feature IDs to appropriate slots */
        switch (modalities[i]) {
            case PERCEPT_MODALITY_VISUAL:
                msg.visual_feature_id = feature_ids[i];
                break;
            case PERCEPT_MODALITY_AUDITORY:
                msg.auditory_feature_id = feature_ids[i];
                break;
            case PERCEPT_MODALITY_SOMATOSENSORY:
                msg.somato_feature_id = feature_ids[i];
                break;
            case PERCEPT_MODALITY_OLFACTORY:
                msg.olfactory_feature_id = feature_ids[i];
                break;
            case PERCEPT_MODALITY_GUSTATORY:
                msg.gustatory_feature_id = feature_ids[i];
                break;
            default:
                break;
        }
    }

    uint64_t now = get_timestamp_us();
    msg.window_start_us = now;
    msg.window_end_us = now + (temporal_window_ms * 1000ULL);
    msg.timestamp_us = now;

    /* Update stats */
    bridge->stats.binding_requests++;
    bridge->stats.messages_sent++;
    bridge->stats.last_broadcast_time_us = now;

    return msg.binding_id;
}

int perception_bio_async_broadcast_binding_result(
    perception_bio_bridge_t* bridge,
    uint32_t binding_id,
    binding_state_t result,
    uint32_t bound_object_id,
    float binding_strength
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_binding_result: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_binding) return 0;

    percept_binding_result_msg_t msg = {0};
    init_percept_header(&msg.header, PERCEPT_MSG_BINDING_RESULT,
                        PERCEPT_MODALITY_VISUAL,
                        bridge->config.object_channel, binding_strength);

    msg.binding_id = binding_id;
    msg.result = result;
    msg.binding_strength = binding_strength;
    msg.bound_object_id = bound_object_id;
    msg.timestamp_us = get_timestamp_us();

    /* Update stats based on result */
    if (result == BINDING_STATE_FULL || result == BINDING_STATE_SPATIAL ||
        result == BINDING_STATE_TEMPORAL) {
        bridge->stats.bindings_successful++;
    } else if (result == BINDING_STATE_CONFLICT) {
        bridge->stats.binding_conflicts++;
        msg.has_conflict = true;
    } else {
        bridge->stats.bindings_failed++;
    }

    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

int perception_bio_async_broadcast_multimodal_object(
    perception_bio_bridge_t* bridge,
    uint32_t object_id,
    uint32_t modality_mask,
    const float* position,
    float confidence
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_multimodal_object: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_multimodal_object: position is NULL");
        return -1;
    }
    if (!bridge->config.enable_binding) return 0;

    percept_object_detected_msg_t msg = {0};
    init_percept_header(&msg.header, PERCEPT_MSG_MULTIMODAL_OBJECT,
                        PERCEPT_MODALITY_VISUAL,  /* Primary reporting modality */
                        bridge->config.object_channel, confidence);

    msg.object_id = object_id ? object_id : bridge->next_object_id++;
    msg.position[0] = position[0];
    msg.position[1] = position[1];
    msg.position[2] = position[2];
    msg.confidence = confidence;
    msg.modality_mask = modality_mask;
    msg.binding_state = BINDING_STATE_FULL;
    msg.first_seen_us = get_timestamp_us();
    msg.last_updated_us = msg.first_seen_us;

    /* Count contributing modalities */
    int mod_count = 0;
    for (int i = 0; i < PERCEPT_MODALITY_COUNT; i++) {
        if (modality_mask & (1U << i)) mod_count++;
    }
    (void)mod_count;

    /* Update stats */
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.header.timestamp_us;

    return 0;
}

/* ============================================================================
 * Salience and Attention API
 * ============================================================================ */

int perception_bio_async_broadcast_salience(
    perception_bio_bridge_t* bridge,
    percept_modality_t modality,
    const float* peak_position,
    float peak_salience
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_salience: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!peak_position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_salience: peak_position is NULL");
        return -1;
    }
    if (modality >= PERCEPT_MODALITY_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "perception_bio_async_broadcast_salience: capacity exceeded");
        return -1;
    }

    percept_salience_map_msg_t msg = {0};
    nimcp_bio_channel_type_t channel = (peak_salience >= bridge->config.salience_urgent_threshold)
        ? bridge->config.urgent_channel
        : bridge->config.default_channel;

    init_percept_header(&msg.header, PERCEPT_MSG_SALIENCE_MAP,
                        modality, channel, peak_salience);

    msg.modality = modality;
    msg.peak_position[0] = peak_position[0];
    msg.peak_position[1] = peak_position[1];
    msg.peak_salience = peak_salience;
    msg.num_regions = 1;
    msg.regions[0].position[0] = peak_position[0];
    msg.regions[0].position[1] = peak_position[1];
    msg.regions[0].salience = peak_salience;
    msg.timestamp_us = get_timestamp_us();

    /* Update stats */
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

int perception_bio_async_broadcast_novelty(
    perception_bio_bridge_t* bridge,
    percept_modality_t modality,
    float novelty_level,
    const float* position
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_novelty: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_novelty: position is NULL");
        return -1;
    }
    if (modality >= PERCEPT_MODALITY_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "perception_bio_async_broadcast_novelty: capacity exceeded");
        return -1;
    }

    percept_message_header_t header = {0};
    nimcp_bio_channel_type_t channel = (novelty_level >= bridge->config.salience_urgent_threshold)
        ? bridge->config.urgent_channel
        : bridge->config.feature_channel;

    init_percept_header(&header, PERCEPT_MSG_NOVELTY_DETECTED,
                        modality, channel, novelty_level);

    if (novelty_level >= bridge->config.salience_urgent_threshold) {
        header.bio_header.flags |= BIO_MSG_FLAG_URGENT;
    }

    /* Update stats */
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = header.timestamp_us;

    return 0;
}

int perception_bio_async_broadcast_cross_modal_attention(
    perception_bio_bridge_t* bridge,
    percept_modality_t source_modality,
    percept_modality_t target_modality,
    const float* position
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_cross_modal_attention: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_broadcast_cross_modal_attention: position is NULL");
        return -1;
    }
    if (source_modality >= PERCEPT_MODALITY_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "perception_bio_async_broadcast_cross_modal_attention: capacity exceeded");
        return -1;
    }
    if (target_modality >= PERCEPT_MODALITY_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "perception_bio_async_broadcast_cross_modal_attention: capacity exceeded");
        return -1;
    }

    percept_message_header_t header = {0};
    init_percept_header(&header, PERCEPT_MSG_CROSS_MODAL_ATTENTION,
                        source_modality,
                        bridge->config.default_channel, 0.7f);

    /* Update stats for both modalities */
    bridge->stats.modality_stats[source_modality].attention_shifts++;
    bridge->stats.modality_stats[target_modality].attention_shifts++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = header.timestamp_us;

    (void)target_modality;
    return 0;
}

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

int perception_bio_async_subscribe_module(
    perception_bio_bridge_t* bridge,
    uint32_t module_id,
    percept_modality_t modality,
    uint32_t msg_types_low,
    uint32_t msg_types_high
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_subscribe_module: bridge is NULL");
        return -1;
    }

    /* Handle subscription to all modalities */
    if (modality >= PERCEPT_MODALITY_COUNT) {
        for (int m = 0; m < PERCEPT_MODALITY_COUNT; m++) {
            int result = perception_bio_async_subscribe_module(
                bridge, module_id, (percept_modality_t)m, msg_types_low, msg_types_high);
            if (result < 0) return result;
        }
        return 0;
    }

    /* Check for existing subscription */
    percept_bio_subscription_t* existing = find_subscription(bridge, modality, module_id);
    if (existing) {
        existing->msg_type_mask_low |= msg_types_low;
        existing->msg_type_mask_high |= msg_types_high;
        return 0;
    }

    /* Find free slot */
    if (bridge->subscription_count[modality] >= bridge->subscription_capacity[modality]) {
        bridge->stats.routing_errors++;
        return PERCEPT_BIO_ERROR_QUEUE_FULL;
    }

    /* Find inactive slot or use next available */
    for (uint32_t i = 0; i < bridge->subscription_capacity[modality]; i++) {
        if (!bridge->subscriptions[modality][i].active) {
            percept_bio_subscription_t* sub = &bridge->subscriptions[modality][i];
            sub->module_id = module_id;
            sub->msg_type_mask_low = msg_types_low;
            sub->msg_type_mask_high = msg_types_high;
            sub->modality = modality;
            sub->active = true;
            sub->subscription_time = get_timestamp_us();
            sub->messages_sent = 0;
            bridge->subscription_count[modality]++;
            bridge->stats.modality_stats[modality].active_subscriptions++;
            return 0;
        }
    }

    return PERCEPT_BIO_ERROR_QUEUE_FULL;
}

int perception_bio_async_unsubscribe_module(
    perception_bio_bridge_t* bridge,
    uint32_t module_id,
    percept_modality_t modality
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_unsubscribe_module: bridge is NULL");
        return -1;
    }

    /* Handle unsubscription from all modalities */
    if (modality >= PERCEPT_MODALITY_COUNT) {
        for (int m = 0; m < PERCEPT_MODALITY_COUNT; m++) {
            perception_bio_async_unsubscribe_module(
                bridge, module_id, (percept_modality_t)m);
        }
        return 0;
    }

    percept_bio_subscription_t* sub = find_subscription(bridge, modality, module_id);
    if (!sub) {
        return -1;  /* Subscription not found - normal lookup behavior */
    }

    sub->active = false;
    sub->msg_type_mask_low = 0;
    sub->msg_type_mask_high = 0;
    bridge->subscription_count[modality]--;
    bridge->stats.modality_stats[modality].active_subscriptions--;

    return 0;
}

uint32_t perception_bio_async_get_subscriber_count(
    const perception_bio_bridge_t* bridge,
    percept_bio_msg_type_t msg_type
) {
    if (!bridge) return 0;
    return (uint32_t)count_subscribers_for_type(bridge, msg_type);
}

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

int perception_bio_async_get_stats(
    const perception_bio_bridge_t* bridge,
    perception_bio_async_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

int perception_bio_async_reset_stats(perception_bio_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_bio_async_reset_stats: bridge is NULL");
        return -1;
    }

    /* Preserve active subscription counts */
    uint32_t active_subs[PERCEPT_MODALITY_COUNT];
    for (int m = 0; m < PERCEPT_MODALITY_COUNT; m++) {
        active_subs[m] = bridge->stats.modality_stats[m].active_subscriptions;
    }

    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Restore active subscription counts */
    for (int m = 0; m < PERCEPT_MODALITY_COUNT; m++) {
        bridge->stats.modality_stats[m].active_subscriptions = active_subs[m];
    }

    return 0;
}

const char* perception_bio_msg_type_name(percept_bio_msg_type_t msg_type) {
    switch (msg_type) {
        /* Visual messages */
        case PERCEPT_MSG_VISUAL_FEATURE: return "VISUAL_FEATURE";
        case PERCEPT_MSG_VISUAL_EDGE: return "VISUAL_EDGE";
        case PERCEPT_MSG_VISUAL_COLOR: return "VISUAL_COLOR";
        case PERCEPT_MSG_VISUAL_MOTION: return "VISUAL_MOTION";
        case PERCEPT_MSG_VISUAL_FACE: return "VISUAL_FACE";
        case PERCEPT_MSG_VISUAL_OBJECT: return "VISUAL_OBJECT";
        case PERCEPT_MSG_VISUAL_SCENE: return "VISUAL_SCENE";
        case PERCEPT_MSG_VISUAL_DEPTH: return "VISUAL_DEPTH";
        case PERCEPT_MSG_VISUAL_ATTENTION_SHIFT: return "VISUAL_ATTENTION_SHIFT";
        case PERCEPT_MSG_VISUAL_SACCADE: return "VISUAL_SACCADE";

        /* Auditory messages */
        case PERCEPT_MSG_AUDITORY_FEATURE: return "AUDITORY_FEATURE";
        case PERCEPT_MSG_AUDITORY_TONE: return "AUDITORY_TONE";
        case PERCEPT_MSG_AUDITORY_ONSET: return "AUDITORY_ONSET";
        case PERCEPT_MSG_AUDITORY_OFFSET: return "AUDITORY_OFFSET";
        case PERCEPT_MSG_AUDITORY_PHONEME: return "AUDITORY_PHONEME";
        case PERCEPT_MSG_AUDITORY_WORD: return "AUDITORY_WORD";
        case PERCEPT_MSG_AUDITORY_SPEAKER: return "AUDITORY_SPEAKER";
        case PERCEPT_MSG_AUDITORY_SPATIAL: return "AUDITORY_SPATIAL";
        case PERCEPT_MSG_AUDITORY_ATTENTION_SHIFT: return "AUDITORY_ATTENTION_SHIFT";
        case PERCEPT_MSG_AUDITORY_STREAM: return "AUDITORY_STREAM";

        /* Somatosensory messages */
        case PERCEPT_MSG_SOMATO_FEATURE: return "SOMATO_FEATURE";
        case PERCEPT_MSG_SOMATO_TOUCH: return "SOMATO_TOUCH";
        case PERCEPT_MSG_SOMATO_PRESSURE: return "SOMATO_PRESSURE";
        case PERCEPT_MSG_SOMATO_VIBRATION: return "SOMATO_VIBRATION";
        case PERCEPT_MSG_SOMATO_TEXTURE: return "SOMATO_TEXTURE";
        case PERCEPT_MSG_SOMATO_PAIN: return "SOMATO_PAIN";
        case PERCEPT_MSG_SOMATO_TEMPERATURE: return "SOMATO_TEMPERATURE";
        case PERCEPT_MSG_SOMATO_PROPRIOCEPTION: return "SOMATO_PROPRIOCEPTION";
        case PERCEPT_MSG_SOMATO_BODY_POSITION: return "SOMATO_BODY_POSITION";

        /* Olfactory messages */
        case PERCEPT_MSG_OLFACTORY_FEATURE: return "OLFACTORY_FEATURE";
        case PERCEPT_MSG_OLFACTORY_ODOR: return "OLFACTORY_ODOR";
        case PERCEPT_MSG_OLFACTORY_INTENSITY: return "OLFACTORY_INTENSITY";
        case PERCEPT_MSG_OLFACTORY_CATEGORY: return "OLFACTORY_CATEGORY";
        case PERCEPT_MSG_OLFACTORY_FAMILIAR: return "OLFACTORY_FAMILIAR";

        /* Gustatory messages */
        case PERCEPT_MSG_GUSTATORY_FEATURE: return "GUSTATORY_FEATURE";
        case PERCEPT_MSG_GUSTATORY_TASTE: return "GUSTATORY_TASTE";
        case PERCEPT_MSG_GUSTATORY_INTENSITY: return "GUSTATORY_INTENSITY";
        case PERCEPT_MSG_GUSTATORY_CATEGORY: return "GUSTATORY_CATEGORY";
        case PERCEPT_MSG_GUSTATORY_PALATABILITY: return "GUSTATORY_PALATABILITY";

        /* Binding messages */
        case PERCEPT_MSG_BINDING_REQUEST: return "BINDING_REQUEST";
        case PERCEPT_MSG_BINDING_RESULT: return "BINDING_RESULT";
        case PERCEPT_MSG_BINDING_CONFLICT: return "BINDING_CONFLICT";
        case PERCEPT_MSG_MULTIMODAL_OBJECT: return "MULTIMODAL_OBJECT";
        case PERCEPT_MSG_MULTIMODAL_SYNC: return "MULTIMODAL_SYNC";
        case PERCEPT_MSG_CROSS_MODAL_ATTENTION: return "CROSS_MODAL_ATTENTION";
        case PERCEPT_MSG_TEMPORAL_BINDING: return "TEMPORAL_BINDING";

        /* Object messages */
        case PERCEPT_MSG_OBJECT_DETECTED: return "OBJECT_DETECTED";
        case PERCEPT_MSG_OBJECT_IDENTIFIED: return "OBJECT_IDENTIFIED";
        case PERCEPT_MSG_OBJECT_TRACKED: return "OBJECT_TRACKED";
        case PERCEPT_MSG_OBJECT_LOST: return "OBJECT_LOST";
        case PERCEPT_MSG_OBJECT_CATEGORY: return "OBJECT_CATEGORY";
        case PERCEPT_MSG_OBJECT_AFFORDANCE: return "OBJECT_AFFORDANCE";

        /* Salience messages */
        case PERCEPT_MSG_SALIENCE_MAP: return "SALIENCE_MAP";
        case PERCEPT_MSG_ATTENTION_TARGET: return "ATTENTION_TARGET";
        case PERCEPT_MSG_NOVELTY_DETECTED: return "NOVELTY_DETECTED";
        case PERCEPT_MSG_SURPRISE_SIGNAL: return "SURPRISE_SIGNAL";
        case PERCEPT_MSG_HABITUATION: return "HABITUATION";

        /* Top-down messages */
        case PERCEPT_MSG_GAIN_MODULATION: return "GAIN_MODULATION";
        case PERCEPT_MSG_PREDICTION_UPDATE: return "PREDICTION_UPDATE";
        case PERCEPT_MSG_ATTENTION_BIAS: return "ATTENTION_BIAS";
        case PERCEPT_MSG_EXPECTATION_SET: return "EXPECTATION_SET";

        /* System messages */
        case PERCEPT_MSG_STATE_QUERY: return "STATE_QUERY";
        case PERCEPT_MSG_STATE_RESPONSE: return "STATE_RESPONSE";
        case PERCEPT_MSG_CONFIG_UPDATE: return "CONFIG_UPDATE";
        case PERCEPT_MSG_CALIBRATION: return "CALIBRATION";
        case PERCEPT_MSG_ERROR_REPORT: return "ERROR_REPORT";

        default: return "UNKNOWN";
    }
}

const char* perception_modality_name(percept_modality_t modality) {
    static const char* names[] = {
        "VISUAL",
        "AUDITORY",
        "SOMATOSENSORY",
        "OLFACTORY",
        "GUSTATORY"
    };

    if (modality >= PERCEPT_MODALITY_COUNT) return "UNKNOWN";
    return names[modality];
}

void perception_bio_async_print_summary(const perception_bio_bridge_t* bridge) {
    if (!bridge) {
        printf("Perception Bio-Async Bridge: NULL\n");
        return;
    }

    printf("=== Perception Bio-Async Bridge Summary ===\n");
    printf("Connected: %s\n", bridge->connected ? "YES" : "NO");
    printf("\n--- Per-Modality Subscriptions ---\n");
    for (int m = 0; m < PERCEPT_MODALITY_COUNT; m++) {
        printf("  %s: %u active\n",
               perception_modality_name((percept_modality_t)m),
               bridge->subscription_count[m]);
    }

    printf("\n--- Overall Message Statistics ---\n");
    printf("Total sent: %lu\n", (unsigned long)bridge->stats.messages_sent);
    printf("Total received: %lu\n", (unsigned long)bridge->stats.messages_received);
    printf("Broadcasts: %lu\n", (unsigned long)bridge->stats.broadcasts_sent);
    printf("Dropped: %lu\n", (unsigned long)bridge->stats.messages_dropped);

    printf("\n--- Per-Modality Statistics ---\n");
    for (int m = 0; m < PERCEPT_MODALITY_COUNT; m++) {
        const percept_modality_stats_t* ms = &bridge->stats.modality_stats[m];
        printf("  %s:\n", perception_modality_name((percept_modality_t)m));
        printf("    Features sent: %lu\n", (unsigned long)ms->features_sent);
        printf("    Objects detected: %lu\n", (unsigned long)ms->objects_detected);
        printf("    Attention shifts: %lu\n", (unsigned long)ms->attention_shifts);
    }

    printf("\n--- Cross-Modal Statistics ---\n");
    printf("Binding requests: %lu\n", (unsigned long)bridge->stats.binding_requests);
    printf("Bindings successful: %lu\n", (unsigned long)bridge->stats.bindings_successful);
    printf("Bindings failed: %lu\n", (unsigned long)bridge->stats.bindings_failed);
    printf("Binding conflicts: %lu\n", (unsigned long)bridge->stats.binding_conflicts);

    printf("\n--- Object Tracking ---\n");
    printf("Objects tracked: %lu\n", (unsigned long)bridge->stats.objects_tracked);
    printf("Objects lost: %lu\n", (unsigned long)bridge->stats.objects_lost);

    printf("\n--- Errors ---\n");
    printf("Handler errors: %lu\n", (unsigned long)bridge->stats.handler_errors);
    printf("Routing errors: %lu\n", (unsigned long)bridge->stats.routing_errors);
    printf("==========================================\n");
}
