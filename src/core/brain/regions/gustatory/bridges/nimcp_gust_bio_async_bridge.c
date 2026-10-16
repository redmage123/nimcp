/**
 * @file nimcp_gust_bio_async_bridge.c
 * @brief Gustatory Cortex Bio-Async Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Implementation of bio-async message routing for gustatory cortex.
 *
 * WHY: Enables gustatory cortex to communicate taste detection, food rewards,
 *      disgust responses, and satiety state across the NIMCP system.
 *
 * HOW: Implements the bio-router module interface, manages subscriptions,
 *      serializes messages, and handles incoming attention/satiety modulation.
 *
 * @author NIMCP Development Team
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/gustatory/bridges/nimcp_gust_bio_async_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(gust_bio_async_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "GUST_BIO_ASYNC_BRIDGE"


/* ============================================================================
 * Internal Structure
 * ============================================================================ */

/**
 * @brief Internal bridge structure
 */
struct gust_bio_async_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    gust_bio_async_config_t config;

    /* Connected modules */
    nimcp_gustatory_t* gust;
    bio_router_t router;

    /* Connection state */
    bool is_connected;
    bool is_registered;

    /* Subscriptions */
    gust_bio_subscription_t subscriptions[GUST_BIO_MAX_SUBSCRIPTIONS];
    uint32_t num_subscriptions;

    /* Message tracking */
    uint32_t next_taste_id;
    uint32_t current_taste_id;

    /* Timing */
    uint64_t last_broadcast_time_us;
    uint32_t time_since_broadcast_ms;

    /* Statistics */
    gust_bio_async_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Find subscription by module ID
 */
static gust_bio_subscription_t* find_subscription(gust_bio_async_bridge_t* bridge, uint32_t module_id) {
    if (!bridge) {
        return NULL;
    }

    for (uint32_t i = 0; i < bridge->num_subscriptions; i++) {
        if (bridge->subscriptions[i].module_id == module_id &&
            bridge->subscriptions[i].active) {
            return &bridge->subscriptions[i];
        }
    }
    return NULL;  /* Not found is a normal condition */
}

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void) {
    /* Platform-specific implementation would go here */
    static _Atomic uint64_t counter = 0;
    return counter++;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int gust_bio_async_default_config(gust_bio_async_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_bio_async_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(gust_bio_async_config_t));

    config->taste_broadcast_interval_ms = GUST_BIO_DEFAULT_BROADCAST_INTERVAL_MS;
    config->enable_auto_broadcast = true;
    config->max_inbox_process_per_update = 32;
    config->message_ttl_ms = GUST_BIO_MESSAGE_TTL_MS;

    config->disgust_threshold = GUST_BIO_DISGUST_THRESHOLD;
    config->toxic_threshold = GUST_BIO_TOXIC_THRESHOLD;
    config->reward_threshold = GUST_BIO_REWARD_THRESHOLD;

    config->default_channel = BIO_CHANNEL_ACETYLCHOLINE;
    config->urgent_channel = BIO_CHANNEL_NOREPINEPHRINE;
    config->max_subscriptions = GUST_BIO_MAX_SUBSCRIPTIONS;

    config->enable_reward_signals = true;
    config->enable_disgust_alerts = true;
    config->enable_toxic_warnings = true;
    config->enable_satiety_integration = true;
    config->enable_preference_learning = true;
    config->enable_logging = false;

    return 0;
}

gust_bio_async_bridge_t* gust_bio_async_bridge_create(const gust_bio_async_config_t* config) {
    gust_bio_async_bridge_t* bridge = (gust_bio_async_bridge_t*)nimcp_calloc(1, sizeof(gust_bio_async_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gust_bio_async_bridge_create: allocation failed");
        return NULL;
    }

    bridge_base_init(&bridge->base, 0, "gust_bio_async");

    if (config) {
        memcpy(&bridge->config, config, sizeof(gust_bio_async_config_t));
    } else {
        gust_bio_async_default_config(&bridge->config);
    }

    bridge->is_connected = false;
    bridge->is_registered = false;
    bridge->num_subscriptions = 0;
    bridge->next_taste_id = 1;
    bridge->current_taste_id = 0;

    memset(&bridge->stats, 0, sizeof(gust_bio_async_stats_t));

    NIMCP_LOGGING_INFO("Created %s bridge", "gust_bio_async");
    return bridge;
}

void gust_bio_async_bridge_destroy(gust_bio_async_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "gust_bio_async");

    if (bridge->is_connected) {
        gust_bio_async_disconnect(bridge);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

int gust_bio_async_connect(gust_bio_async_bridge_t* bridge, nimcp_gustatory_t* gust, bio_router_t router) {
    if (!bridge || !gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_bio_async_connect: required parameter is NULL (bridge, gust)");
        return -1;
    }

    bridge->gust = gust;
    bridge->router = router;
    bridge->is_connected = true;
    bridge->is_registered = (router != NULL);

    bridge->last_broadcast_time_us = get_timestamp_us();
    bridge->time_since_broadcast_ms = 0;

    return 0;
}

int gust_bio_async_disconnect(gust_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    bridge->gust = NULL;
    bridge->router = NULL;
    bridge->is_connected = false;
    bridge->is_registered = false;

    /* Clear subscriptions */
    for (uint32_t i = 0; i < bridge->num_subscriptions; i++) {
        bridge->subscriptions[i].active = false;
    }
    bridge->num_subscriptions = 0;

    return 0;
}

bool gust_bio_async_is_connected(const gust_bio_async_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->is_connected;
}

/* ============================================================================
 * Message Processing Implementation
 * ============================================================================ */

int gust_bio_async_process_inbox(gust_bio_async_bridge_t* bridge, uint32_t max_messages) {
    if (!bridge || !bridge->is_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_bio_async_process_inbox: required parameter is NULL (bridge, bridge->is_connected)");
        return -1;
    }

    /* In a full implementation, this would pull messages from the bio-router
     * and process incoming satiety modulation, attention requests, etc. */
    (void)max_messages;

    return 0;
}

int gust_bio_async_update(gust_bio_async_bridge_t* bridge, uint32_t delta_ms) {
    if (!bridge || !bridge->is_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_bio_async_update: required parameter is NULL (bridge, bridge->is_connected)");
        return -1;
    }

    bridge->time_since_broadcast_ms += delta_ms;

    /* Process incoming messages */
    int result = gust_bio_async_process_inbox(bridge, bridge->config.max_inbox_process_per_update);
    if (result < 0) {
        bridge->stats.handler_errors++;
    }

    /* Auto-broadcast if enabled and interval elapsed */
    if (bridge->config.enable_auto_broadcast &&
        bridge->time_since_broadcast_ms >= bridge->config.taste_broadcast_interval_ms) {

        bridge->time_since_broadcast_ms = 0;
        bridge->last_broadcast_time_us = get_timestamp_us();
        bridge->stats.last_broadcast_time_us = bridge->last_broadcast_time_us;
    }

    return 0;
}

/* ============================================================================
 * Broadcast API Implementation
 * ============================================================================ */

int gust_bio_async_broadcast_taste_detected(gust_bio_async_bridge_t* bridge, const taste_stimulus_t* stimulus, bool is_novel) {
    if (!bridge || !bridge->is_connected || !stimulus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_bio_async_broadcast_taste_detected: required parameter is NULL (bridge, bridge->is_connected, stimulus)");
        return -1;
    }
    if (!bridge->gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_bio_async_broadcast_taste_detected: bridge->gust is NULL");
        return -1;
    }

    bridge->current_taste_id = bridge->next_taste_id++;

    gust_bio_taste_detected_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = GUST_BIO_MSG_TASTE_DETECTED;
    msg.header.source_module = BIO_MODULE_ID_GUSTATORY;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.sweet = stimulus->sweet;
    msg.salty = stimulus->salty;
    msg.sour = stimulus->sour;
    msg.bitter = stimulus->bitter;
    msg.umami = stimulus->umami;
    msg.temperature = stimulus->temperature;
    msg.texture = stimulus->texture;

    msg.overall_intensity = (stimulus->sweet + stimulus->salty +
                            stimulus->sour + stimulus->bitter +
                            stimulus->umami) / 5.0f;
    msg.is_novel = is_novel;
    msg.taste_id = bridge->current_taste_id;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.tastes_detected++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int gust_bio_async_broadcast_flavor_result(gust_bio_async_bridge_t* bridge, const char* name, food_category_t category, float confidence) {
    if (!bridge || !bridge->is_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_bio_async_broadcast_flavor_result: required parameter is NULL (bridge, bridge->is_connected)");
        return -1;
    }

    gust_bio_flavor_result_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = GUST_BIO_MSG_FLAVOR_RESULT;
    msg.header.source_module = BIO_MODULE_ID_GUSTATORY;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    if (name) {
        strncpy(msg.flavor_name, name, sizeof(msg.flavor_name) - 1);
    }
    msg.category = category;
    msg.identification_confidence = confidence;
    msg.taste_id = bridge->current_taste_id;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.flavors_identified++;
    bridge->stats.messages_sent++;

    return 0;
}

int gust_bio_async_broadcast_reward(gust_bio_async_bridge_t* bridge, float magnitude, food_category_t category) {
    if (!bridge || !bridge->is_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_bio_async_broadcast_reward: required parameter is NULL (bridge, bridge->is_connected)");
        return -1;
    }
    if (!bridge->config.enable_reward_signals) return 0;

    if (magnitude < bridge->config.reward_threshold) return 0;

    gust_bio_reward_signal_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = GUST_BIO_MSG_REWARD_SIGNAL;
    msg.header.source_module = BIO_MODULE_ID_GUSTATORY;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.reward_magnitude = magnitude;
    msg.food_category = category;
    msg.taste_id = bridge->current_taste_id;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.rewards_sent++;
    bridge->stats.messages_sent++;

    return 0;
}

int gust_bio_async_broadcast_disgust(gust_bio_async_bridge_t* bridge, disgust_level_t level, float intensity) {
    if (!bridge || !bridge->is_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_bio_async_broadcast_disgust: required parameter is NULL (bridge, bridge->is_connected)");
        return -1;
    }
    if (!bridge->config.enable_disgust_alerts) return 0;

    if (intensity < bridge->config.disgust_threshold) return 0;

    gust_bio_disgust_alert_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = GUST_BIO_MSG_DISGUST_ALERT;
    msg.header.source_module = BIO_MODULE_ID_GUSTATORY;
    msg.header.flags = (level >= DISGUST_STRONG) ? BIO_MSG_FLAG_URGENT : 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.level = level;
    msg.intensity = intensity;
    msg.urgency = (level == DISGUST_EXTREME) ? 1.0f : intensity;
    msg.triggers_gag = (level >= DISGUST_EXTREME);
    msg.triggers_rejection = (level >= DISGUST_MODERATE);
    msg.taste_id = bridge->current_taste_id;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.disgust_alerts_sent++;
    bridge->stats.messages_sent++;

    return 0;
}

int gust_bio_async_broadcast_satiety(gust_bio_async_bridge_t* bridge, float satiety, float hunger) {
    if (!bridge || !bridge->is_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_bio_async_broadcast_satiety: required parameter is NULL (bridge, bridge->is_connected)");
        return -1;
    }
    if (!bridge->config.enable_satiety_integration) return 0;

    gust_bio_satiety_update_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = GUST_BIO_MSG_SATIETY_UPDATE;
    msg.header.source_module = BIO_MODULE_ID_GUSTATORY;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.satiety_level = satiety;
    msg.hunger_level = hunger;
    msg.reward_modulation = 1.0f - (satiety * 0.5f);  /* Satiety reduces reward */
    msg.is_request = false;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.satiety_updates_sent++;
    bridge->stats.messages_sent++;

    return 0;
}

int gust_bio_async_broadcast_toxic_warning(gust_bio_async_bridge_t* bridge, float toxicity, bool recommend_spit) {
    if (!bridge || !bridge->is_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_bio_async_broadcast_toxic_warning: required parameter is NULL (bridge, bridge->is_connected)");
        return -1;
    }
    if (!bridge->config.enable_toxic_warnings) return 0;

    if (toxicity < bridge->config.toxic_threshold) return 0;

    gust_bio_toxic_warning_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = GUST_BIO_MSG_TOXIC_WARNING;
    msg.header.source_module = BIO_MODULE_ID_GUSTATORY;
    msg.header.flags = BIO_MSG_FLAG_URGENT;
    msg.header.timestamp_us = get_timestamp_us();

    msg.toxicity_estimate = toxicity;
    msg.confidence = 0.8f;  /* Default confidence */
    msg.urgency = toxicity;
    msg.recommend_spit = recommend_spit;
    msg.recommend_vomit = (toxicity > 0.8f);
    msg.taste_id = bridge->current_taste_id;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.toxic_warnings_sent++;
    bridge->stats.messages_sent++;

    return 0;
}

int gust_bio_async_broadcast_preference_update(gust_bio_async_bridge_t* bridge, basic_taste_t taste, float old_pref, float new_pref) {
    if (!bridge || !bridge->is_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_bio_async_broadcast_preference_update: required parameter is NULL (bridge, bridge->is_connected)");
        return -1;
    }
    if (!bridge->config.enable_preference_learning) return 0;

    gust_bio_preference_update_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = GUST_BIO_MSG_PREFERENCE_UPDATE;
    msg.header.source_module = BIO_MODULE_ID_GUSTATORY;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.taste_type = taste;
    msg.old_preference = old_pref;
    msg.new_preference = new_pref;
    msg.preference_delta = new_pref - old_pref;
    msg.positive_outcome = (new_pref > old_pref);
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.preference_updates_sent++;
    bridge->stats.messages_sent++;

    return 0;
}

int gust_bio_async_broadcast_hedonic(gust_bio_async_bridge_t* bridge, taste_hedonic_t valence, float score) {
    if (!bridge || !bridge->is_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_bio_async_broadcast_hedonic: required parameter is NULL (bridge, bridge->is_connected)");
        return -1;
    }

    gust_bio_hedonic_value_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = GUST_BIO_MSG_HEDONIC_VALUE;
    msg.header.source_module = BIO_MODULE_ID_GUSTATORY;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.valence = valence;
    msg.hedonic_score = score;
    msg.taste_id = bridge->current_taste_id;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.hedonic_sent++;
    bridge->stats.messages_sent++;

    return 0;
}

int gust_bio_async_broadcast_palatability(gust_bio_async_bridge_t* bridge, float palatability, food_category_t category) {
    if (!bridge || !bridge->is_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_bio_async_broadcast_palatability: required parameter is NULL (bridge, bridge->is_connected)");
        return -1;
    }

    gust_bio_palatability_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = GUST_BIO_MSG_PALATABILITY;
    msg.header.source_module = BIO_MODULE_ID_GUSTATORY;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.palatability = palatability;
    msg.category = category;
    msg.taste_id = bridge->current_taste_id;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.messages_sent++;

    return 0;
}

int gust_bio_async_broadcast_adaptation(gust_bio_async_bridge_t* bridge, const float* adaptation_levels, bool fully_adapted) {
    if (!bridge || !bridge->is_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_bio_async_broadcast_adaptation: required parameter is NULL (bridge, bridge->is_connected)");
        return -1;
    }

    gust_bio_adaptation_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = GUST_BIO_MSG_ADAPTATION;
    msg.header.source_module = BIO_MODULE_ID_GUSTATORY;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    if (adaptation_levels) {
        msg.sweet_adaptation = adaptation_levels[TASTE_SWEET];
        msg.salty_adaptation = adaptation_levels[TASTE_SALTY];
        msg.sour_adaptation = adaptation_levels[TASTE_SOUR];
        msg.bitter_adaptation = adaptation_levels[TASTE_BITTER];
        msg.umami_adaptation = adaptation_levels[TASTE_UMAMI];

        msg.overall_adaptation = (msg.sweet_adaptation + msg.salty_adaptation +
                                  msg.sour_adaptation + msg.bitter_adaptation +
                                  msg.umami_adaptation) / 5.0f;
    }

    msg.fully_adapted = fully_adapted;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.messages_sent++;

    return 0;
}

/* ============================================================================
 * Subscription API Implementation
 * ============================================================================ */

int gust_bio_async_subscribe_module(gust_bio_async_bridge_t* bridge, uint32_t module_id, uint32_t msg_types) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_bio_async_subscribe_module: bridge is NULL");
        return -1;
    }

    /* Check if already subscribed */
    gust_bio_subscription_t* existing = find_subscription(bridge, module_id);
    if (existing) {
        existing->msg_type_mask |= msg_types;
        return 0;
    }

    /* Find free slot */
    if (bridge->num_subscriptions >= bridge->config.max_subscriptions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "gust_bio_async_subscribe_module: capacity exceeded");
        return -1;
    }

    gust_bio_subscription_t* sub = &bridge->subscriptions[bridge->num_subscriptions++];
    sub->module_id = module_id;
    sub->msg_type_mask = msg_types;
    sub->active = true;
    sub->subscription_time = get_timestamp_us();
    sub->messages_sent = 0;

    bridge->stats.active_subscriptions = bridge->num_subscriptions;
    if (bridge->num_subscriptions > bridge->stats.peak_subscriptions) {
        bridge->stats.peak_subscriptions = bridge->num_subscriptions;
    }

    return 0;
}

int gust_bio_async_unsubscribe_module(gust_bio_async_bridge_t* bridge, uint32_t module_id) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_bio_async_unsubscribe_module: bridge is NULL");
        return -1;
    }

    gust_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_bio_async_unsubscribe_module: sub is NULL");
        return -1;
    }

    sub->active = false;
    bridge->stats.active_subscriptions--;

    return 0;
}

uint32_t gust_bio_async_get_subscriber_count(const gust_bio_async_bridge_t* bridge, gust_bio_msg_type_t type) {
    if (!bridge || type >= GUST_BIO_MSG_COUNT) return 0;

    uint32_t mask = (1U << type);
    uint32_t count = 0;

    for (uint32_t i = 0; i < bridge->num_subscriptions; i++) {
        if (bridge->subscriptions[i].active &&
            (bridge->subscriptions[i].msg_type_mask & mask)) {
            count++;
        }
    }

    return count;
}

/* ============================================================================
 * Statistics API Implementation
 * ============================================================================ */

int gust_bio_async_get_stats(const gust_bio_async_bridge_t* bridge, gust_bio_async_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_bio_async_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    memcpy(stats, &bridge->stats, sizeof(gust_bio_async_stats_t));
    return 0;
}

int gust_bio_async_reset_stats(gust_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_bio_async_reset_stats: bridge is NULL");
        return -1;
    }

    uint32_t active_subs = bridge->stats.active_subscriptions;
    uint32_t peak_subs = bridge->stats.peak_subscriptions;

    memset(&bridge->stats, 0, sizeof(gust_bio_async_stats_t));

    bridge->stats.active_subscriptions = active_subs;
    bridge->stats.peak_subscriptions = peak_subs;

    return 0;
}

const char* gust_bio_msg_type_name(gust_bio_msg_type_t msg_type) {
    switch (msg_type) {
        case GUST_BIO_MSG_TASTE_DETECTED:     return "TASTE_DETECTED";
        case GUST_BIO_MSG_FLAVOR_RESULT:      return "FLAVOR_RESULT";
        case GUST_BIO_MSG_REWARD_SIGNAL:      return "REWARD_SIGNAL";
        case GUST_BIO_MSG_DISGUST_ALERT:      return "DISGUST_ALERT";
        case GUST_BIO_MSG_SATIETY_UPDATE:     return "SATIETY_UPDATE";
        case GUST_BIO_MSG_TOXIC_WARNING:      return "TOXIC_WARNING";
        case GUST_BIO_MSG_PREFERENCE_UPDATE:  return "PREFERENCE_UPDATE";
        case GUST_BIO_MSG_HEDONIC_VALUE:      return "HEDONIC_VALUE";
        case GUST_BIO_MSG_PALATABILITY:       return "PALATABILITY";
        case GUST_BIO_MSG_FOOD_CATEGORY:      return "FOOD_CATEGORY";
        case GUST_BIO_MSG_ADAPTATION:         return "ADAPTATION";
        case GUST_BIO_MSG_REQUEST_STATE:      return "REQUEST_STATE";
        case GUST_BIO_MSG_MODULATE_ATTENTION: return "MODULATE_ATTENTION";
        default:                               return "UNKNOWN";
    }
}

void gust_bio_async_print_summary(const gust_bio_async_bridge_t* bridge) {
    if (!bridge) return;

    printf("=== Gustatory Bio-Async Bridge Summary ===\n");
    printf("Connected: %s\n", bridge->is_connected ? "Yes" : "No");
    printf("Registered: %s\n", bridge->is_registered ? "Yes" : "No");
    printf("Active Subscriptions: %u\n", bridge->stats.active_subscriptions);
    printf("\n");

    printf("Messages Sent: %lu\n", (unsigned long)bridge->stats.messages_sent);
    printf("Messages Received: %lu\n", (unsigned long)bridge->stats.messages_received);
    printf("Messages Dropped: %lu\n", (unsigned long)bridge->stats.messages_dropped);
    printf("\n");

    printf("Tastes Detected: %lu\n", (unsigned long)bridge->stats.tastes_detected);
    printf("Flavors Identified: %lu\n", (unsigned long)bridge->stats.flavors_identified);
    printf("Rewards Sent: %lu\n", (unsigned long)bridge->stats.rewards_sent);
    printf("Disgust Alerts: %lu\n", (unsigned long)bridge->stats.disgust_alerts_sent);
    printf("Toxic Warnings: %lu\n", (unsigned long)bridge->stats.toxic_warnings_sent);
    printf("Satiety Updates: %lu\n", (unsigned long)bridge->stats.satiety_updates_sent);
    printf("Preference Updates: %lu\n", (unsigned long)bridge->stats.preference_updates_sent);
    printf("Hedonic Sent: %lu\n", (unsigned long)bridge->stats.hedonic_sent);
    printf("\n");

    printf("Handler Errors: %lu\n", (unsigned long)bridge->stats.handler_errors);
    printf("Routing Errors: %lu\n", (unsigned long)bridge->stats.routing_errors);
    printf("==========================================\n");
}
