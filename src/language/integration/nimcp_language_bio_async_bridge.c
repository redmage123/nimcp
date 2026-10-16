/**
 * @file nimcp_language_bio_async_bridge.c
 * @brief Implementation of Language Module Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "language/integration/nimcp_language_bio_async_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(language_bio_async_bridge)

#define LOG_MODULE "LANGUAGE_BIO_ASYNC_BRIDGE"


/* ============================================================================
 * Internal Bridge Structure
 * ============================================================================ */

struct language_bio_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    language_bio_bridge_config_t config;
    language_orchestrator_t* orchestrator;
    bio_router_t router;
    bio_module_context_t module_ctx;

    lang_bio_subscription_t* subscriptions;
    uint32_t subscription_count;
    uint32_t subscription_capacity;

    bool connected;
    uint64_t last_broadcast_us;
    uint32_t time_since_comprehension_broadcast_ms;
    uint32_t time_since_production_broadcast_ms;

    uint64_t next_request_id;
    uint64_t current_utterance_id;

    language_bio_bridge_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static lang_bio_subscription_t* find_subscription(
    language_bio_bridge_t* bridge,
    bio_module_id_t module_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_subscription: bridge is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].module_id == module_id &&
            bridge->subscriptions[i].active) {
            return &bridge->subscriptions[i];
        }
    }
    return NULL;  /* Not found - normal lookup behavior */
}

static int count_subscribers_for_type(
    const language_bio_bridge_t* bridge,
    lang_bio_msg_type_t type
) {
    if (!bridge || type >= LANG_MSG_COUNT) return 0;

    int count = 0;
    uint64_t mask = (1ULL << type);

    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].active &&
            (bridge->subscriptions[i].msg_type_mask & mask)) {
            count++;
        }
    }
    return count;
}

static void init_message_header(
    bio_message_header_t* header,
    uint32_t msg_type,
    nimcp_bio_channel_type_t channel,
    uint16_t flags
) {
    if (!header) return;

    memset(header, 0, sizeof(*header));
    header->type = msg_type;
    header->source_module = BIO_MODULE_ID_LANGUAGE_BRIDGE;
    header->channel = channel;
    header->flags = flags;
    header->timestamp_us = get_timestamp_us();
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int language_bio_bridge_default_config(language_bio_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    config->comprehension_broadcast_interval_ms = LANG_BIO_DEFAULT_BROADCAST_INTERVAL_MS;
    config->production_broadcast_interval_ms = 50;
    config->enable_auto_broadcast = true;
    config->max_inbox_process_per_update = 32;
    config->message_ttl_ms = LANG_BIO_MESSAGE_TTL_MS;
    config->anomaly_urgency_threshold = LANG_BIO_ANOMALY_URGENCY_THRESHOLD;
    config->default_channel = BIO_CHANNEL_ACETYLCHOLINE;
    config->urgent_channel = BIO_CHANNEL_NOREPINEPHRINE;
    config->semantic_channel = BIO_CHANNEL_DOPAMINE;
    config->max_subscriptions = LANG_BIO_MAX_SUBSCRIPTIONS;
    config->enable_comprehension_routing = true;
    config->enable_production_routing = true;
    config->enable_anomaly_routing = true;
    config->enable_semantic_broadcast = true;
    config->enable_syntactic_broadcast = true;
    config->enable_phonological_broadcast = true;
    config->enable_region_sync = true;
    config->enable_logging = false;

    return 0;
}

language_bio_bridge_t* language_bio_bridge_create(
    const language_bio_bridge_config_t* config
) {
    language_bio_bridge_t* bridge = nimcp_calloc(1, sizeof(language_bio_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_bio_bridge_create: allocation failed");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        language_bio_bridge_default_config(&bridge->config);
    }

    bridge->subscription_capacity = bridge->config.max_subscriptions;
    bridge->subscriptions = nimcp_calloc(
        bridge->subscription_capacity,
        sizeof(lang_bio_subscription_t)
    );
    if (!bridge->subscriptions) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_bio_bridge_create: bridge->subscriptions is NULL");
        return NULL;
    }

    bridge->last_broadcast_us = get_timestamp_us();
    bridge->next_request_id = 1;

    return bridge;
}

void language_bio_bridge_destroy(language_bio_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "language_bio_async");

    if (bridge->connected) {
        language_bio_bridge_disconnect(bridge);
    }

    nimcp_free(bridge->subscriptions);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int language_bio_bridge_connect(
    language_bio_bridge_t* bridge,
    language_orchestrator_t* orchestrator,
    bio_router_t router
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_connect: bridge is NULL");
        return -1;
    }

    bridge->orchestrator = orchestrator;
    bridge->router = router;
    bridge->connected = true;

    /* Register with router if available */
    if (router) {
        bio_module_info_t info = {0};
        info.module_id = BIO_MODULE_ID_LANGUAGE_BRIDGE;
        info.module_name = "language_bio_bridge";
        info.inbox_capacity = LANG_BIO_MAX_INBOX_SIZE;
        info.user_data = bridge;

        bridge->module_ctx = bio_router_register_module(&info);
    }

    return 0;
}

int language_bio_bridge_disconnect(language_bio_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_disconnect: bridge is NULL");
        return -1;
    }

    if (bridge->router && bridge->module_ctx) {
        bio_router_unregister_module(bridge->module_ctx);
        bridge->module_ctx = NULL;
    }

    bridge->orchestrator = NULL;
    bridge->router = NULL;
    bridge->connected = false;

    return 0;
}

bool language_bio_bridge_is_connected(const language_bio_bridge_t* bridge) {
    return bridge ? bridge->connected : false;
}

bio_module_context_t language_bio_bridge_get_context(
    const language_bio_bridge_t* bridge
) {
    return bridge ? bridge->module_ctx : NULL;
}

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

int language_bio_bridge_process_inbox(
    language_bio_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_process_inbox: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    uint32_t processed = 0;

    /* Process incoming messages via router */
    if (bridge->module_ctx) {
        processed = bio_router_process_inbox(
            bridge->module_ctx,
            max_messages ? max_messages : bridge->config.max_inbox_process_per_update
        );
    }

    bridge->stats.messages_received += processed;
    return (int)processed;
}

int language_bio_bridge_update(
    language_bio_bridge_t* bridge,
    uint32_t delta_ms
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    bridge->time_since_comprehension_broadcast_ms += delta_ms;
    bridge->time_since_production_broadcast_ms += delta_ms;

    /* Auto-broadcast not implemented here - done by orchestrator */

    return 0;
}

/* ============================================================================
 * Utterance Coordination API
 * ============================================================================ */

int language_bio_bridge_broadcast_utterance_start(
    language_bio_bridge_t* bridge,
    uint64_t utterance_id,
    language_input_type_t input_type,
    language_mode_t mode
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_broadcast_utterance_start: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    lang_bio_utterance_msg_t msg = {0};
    init_message_header(
        &msg.header,
        LANG_MSG_UTTERANCE_START,
        bridge->config.default_channel,
        0
    );

    msg.utterance_id = utterance_id;
    msg.input_type = input_type;
    msg.mode = mode;
    msg.expected_length = 0;
    msg.timestamp_us = get_timestamp_us();

    bridge->current_utterance_id = utterance_id;
    bridge->stats.utterances_started++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    /* Route via bio_router if connected */
    if (bridge->module_ctx) {
        bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    }

    return 0;
}

int language_bio_bridge_broadcast_utterance_end(
    language_bio_bridge_t* bridge,
    uint64_t utterance_id
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_broadcast_utterance_end: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    lang_bio_utterance_msg_t msg = {0};
    init_message_header(
        &msg.header,
        LANG_MSG_UTTERANCE_END,
        bridge->config.default_channel,
        0
    );

    msg.utterance_id = utterance_id;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.utterances_completed++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    if (bridge->module_ctx) {
        bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    }

    return 0;
}

/* ============================================================================
 * Comprehension Coordination API
 * ============================================================================ */

int language_bio_bridge_broadcast_phoneme(
    language_bio_bridge_t* bridge,
    const language_phoneme_t* phoneme,
    uint64_t utterance_id
) {
    if (!bridge || !bridge->connected || !phoneme) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_broadcast_phoneme: required parameter is NULL (bridge, bridge->connected, phoneme)");
        return -1;
    }
    if (!bridge->config.enable_phonological_broadcast) return 0;

    lang_bio_phoneme_msg_t msg = {0};
    init_message_header(
        &msg.header,
        LANG_MSG_PHONEME_RECOGNIZED,
        bridge->config.default_channel,
        0
    );

    msg.phoneme_id = phoneme->id;
    msg.category = phoneme->category;
    msg.confidence = phoneme->confidence;
    msg.duration_ms = phoneme->duration_ms;
    memcpy(msg.formants, phoneme->formants, sizeof(msg.formants));
    msg.is_word_boundary = phoneme->is_word_boundary;
    msg.is_phrase_boundary = phoneme->is_phrase_boundary;
    msg.utterance_id = utterance_id;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    if (bridge->module_ctx) {
        bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    }

    return 0;
}

int language_bio_bridge_broadcast_word(
    language_bio_bridge_t* bridge,
    const language_word_t* word,
    uint64_t utterance_id
) {
    if (!bridge || !bridge->connected || !word) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_broadcast_word: required parameter is NULL (bridge, bridge->connected, word)");
        return -1;
    }
    if (!bridge->config.enable_comprehension_routing) return 0;

    lang_bio_word_msg_t msg = {0};
    init_message_header(
        &msg.header,
        LANG_MSG_WORD_RECOGNIZED,
        bridge->config.default_channel,
        0
    );

    msg.word_id = word->id;
    strncpy(msg.word_form, word->form, LANG_BIO_MAX_WORD_LEN - 1);
    msg.pos = word->pos;
    msg.confidence = word->confidence;
    msg.frequency = word->frequency;
    msg.activation = word->activation;
    msg.sense_id = word->sense_id;
    msg.num_senses = word->num_senses;
    msg.utterance_id = utterance_id;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.words_recognized++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    if (bridge->module_ctx) {
        bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    }

    return 0;
}

int language_bio_bridge_broadcast_concept(
    language_bio_bridge_t* bridge,
    const language_concept_t* concept,
    uint64_t utterance_id
) {
    if (!bridge || !bridge->connected || !concept) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_broadcast_concept: required parameter is NULL (bridge, bridge->connected, concept)");
        return -1;
    }
    if (!bridge->config.enable_semantic_broadcast) return 0;

    lang_bio_concept_msg_t msg = {0};
    init_message_header(
        &msg.header,
        LANG_MSG_CONCEPT_ACTIVATED,
        bridge->config.semantic_channel,
        0
    );

    msg.concept_id = concept->id;
    strncpy(msg.concept_name, concept->name, LANG_BIO_MAX_CONCEPT_NAME_LEN - 1);
    msg.activation = concept->activation;
    msg.relevance = concept->relevance;
    msg.role = concept->role;
    msg.source_word_id = concept->source_word_id;
    msg.is_target = concept->is_target;
    msg.semantic_vector = NULL;  /* Not transferred in message */
    msg.semantic_dim = 0;
    msg.utterance_id = utterance_id;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.concepts_activated++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    if (bridge->module_ctx) {
        bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    }

    return 0;
}

int language_bio_bridge_broadcast_comprehension_complete(
    language_bio_bridge_t* bridge,
    const language_comprehension_result_t* result
) {
    if (!bridge || !bridge->connected || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_broadcast_comprehension_complete: required parameter is NULL (bridge, bridge->connected, result)");
        return -1;
    }
    if (!bridge->config.enable_comprehension_routing) return 0;

    lang_bio_comprehension_complete_msg_t msg = {0};
    init_message_header(
        &msg.header,
        LANG_MSG_COMPREHENSION_COMPLETE,
        bridge->config.semantic_channel,
        0
    );

    msg.utterance_id = bridge->current_utterance_id;
    msg.word_count = result->word_count;
    msg.concept_count = result->concept_count;
    msg.semantic_coherence = result->semantic_coherence;
    msg.syntactic_score = result->syntactic_wellformedness;
    msg.overall_confidence = result->overall_confidence;
    msg.processing_time_ms = result->processing_time_ms;
    msg.semantic_anomaly = result->semantic_anomaly;
    msg.syntactic_anomaly = result->syntactic_anomaly;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    if (bridge->module_ctx) {
        bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    }

    return 0;
}

/* ============================================================================
 * Production Coordination API
 * ============================================================================ */

uint64_t language_bio_bridge_request_production(
    language_bio_bridge_t* bridge,
    const float* semantic_input,
    uint32_t semantic_dim,
    language_output_type_t output_type
) {
    if (!bridge || !bridge->connected) return 0;
    if (!bridge->config.enable_production_routing) return 0;

    lang_bio_production_request_msg_t msg = {0};
    init_message_header(
        &msg.header,
        LANG_MSG_PRODUCTION_REQUEST,
        bridge->config.default_channel,
        0
    );

    uint64_t request_id = bridge->next_request_id++;
    msg.request_id = request_id;
    msg.output_type = output_type;
    msg.semantic_input = (float*)semantic_input;  /* Pointer, not copied */
    msg.semantic_dim = semantic_dim;
    msg.max_words = 0;  /* No limit */
    msg.urgency = 0.5f;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.messages_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    if (bridge->module_ctx) {
        bio_router_send(bridge->module_ctx, &msg, sizeof(msg), 0);
    }

    return request_id;
}

int language_bio_bridge_broadcast_production_complete(
    language_bio_bridge_t* bridge,
    const language_production_plan_t* plan
) {
    if (!bridge || !bridge->connected || !plan) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_broadcast_production_complete: required parameter is NULL (bridge, bridge->connected, plan)");
        return -1;
    }
    if (!bridge->config.enable_production_routing) return 0;

    lang_bio_production_complete_msg_t msg = {0};
    init_message_header(
        &msg.header,
        LANG_MSG_PRODUCTION_COMPLETE,
        bridge->config.default_channel,
        0
    );

    msg.request_id = 0;  /* Would need tracking */
    msg.word_count = plan->word_count;
    msg.phoneme_count = plan->phoneme_count;
    msg.fluency_score = plan->fluency_score;
    msg.planning_time_ms = plan->planning_time_ms;
    msg.articulation_time_ms = 0;  /* Not in plan struct */
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.productions_completed++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    if (bridge->module_ctx) {
        bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    }

    return 0;
}

/* ============================================================================
 * Anomaly/Error Broadcast API
 * ============================================================================ */

int language_bio_bridge_broadcast_semantic_anomaly(
    language_bio_bridge_t* bridge,
    uint32_t word_position,
    uint32_t word_id,
    uint32_t expected_id,
    float anomaly_magnitude,
    uint64_t utterance_id
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_broadcast_semantic_anomaly: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_anomaly_routing) return 0;

    lang_bio_anomaly_msg_t msg = {0};

    /* Use urgent channel for severe anomalies */
    nimcp_bio_channel_type_t channel = bridge->config.default_channel;
    uint16_t flags = 0;
    if (anomaly_magnitude >= bridge->config.anomaly_urgency_threshold) {
        channel = bridge->config.urgent_channel;
        flags = BIO_MSG_FLAG_URGENT;
    }

    init_message_header(&msg.header, LANG_MSG_SEMANTIC_ANOMALY, channel, flags);

    msg.is_semantic = true;
    msg.anomaly_magnitude = anomaly_magnitude;
    msg.surprise = anomaly_magnitude * 2.0f;  /* Simple surprise estimate */
    msg.word_position = word_position;
    msg.word_id = word_id;
    msg.expected_id = expected_id;
    msg.utterance_id = utterance_id;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.semantic_anomalies++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    if (bridge->module_ctx) {
        bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    }

    return 0;
}

int language_bio_bridge_broadcast_syntactic_anomaly(
    language_bio_bridge_t* bridge,
    uint32_t word_position,
    uint32_t word_id,
    float anomaly_magnitude,
    uint64_t utterance_id
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_broadcast_syntactic_anomaly: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_anomaly_routing) return 0;

    lang_bio_anomaly_msg_t msg = {0};

    nimcp_bio_channel_type_t channel = bridge->config.default_channel;
    uint16_t flags = 0;
    if (anomaly_magnitude >= bridge->config.anomaly_urgency_threshold) {
        channel = bridge->config.urgent_channel;
        flags = BIO_MSG_FLAG_URGENT;
    }

    init_message_header(&msg.header, LANG_MSG_SYNTACTIC_ANOMALY, channel, flags);

    msg.is_semantic = false;
    msg.anomaly_magnitude = anomaly_magnitude;
    msg.surprise = anomaly_magnitude * 1.5f;
    msg.word_position = word_position;
    msg.word_id = word_id;
    msg.expected_id = 0;
    msg.utterance_id = utterance_id;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.syntactic_anomalies++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    if (bridge->module_ctx) {
        bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    }

    return 0;
}

int language_bio_bridge_broadcast_ambiguity(
    language_bio_bridge_t* bridge,
    uint32_t word_position,
    uint32_t word_id,
    uint32_t num_interpretations,
    uint64_t utterance_id
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_broadcast_ambiguity: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_anomaly_routing) return 0;

    lang_bio_anomaly_msg_t msg = {0};
    init_message_header(
        &msg.header,
        LANG_MSG_AMBIGUITY_DETECTED,
        bridge->config.default_channel,
        0
    );

    msg.is_semantic = true;
    msg.anomaly_magnitude = 0.5f;  /* Moderate magnitude for ambiguity */
    msg.surprise = (float)num_interpretations * 0.2f;
    msg.word_position = word_position;
    msg.word_id = word_id;
    msg.expected_id = num_interpretations;  /* Store count in expected_id */
    msg.utterance_id = utterance_id;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    if (bridge->module_ctx) {
        bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    }

    return 0;
}

int language_bio_bridge_broadcast_error(
    language_bio_bridge_t* bridge,
    int32_t error_code,
    const char* message,
    lang_region_id_t source_region
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_broadcast_error: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    lang_bio_error_msg_t msg = {0};
    init_message_header(
        &msg.header,
        LANG_MSG_ERROR,
        bridge->config.urgent_channel,
        BIO_MSG_FLAG_URGENT
    );

    msg.error_code = error_code;
    if (message) {
        strncpy(msg.message, message, LANG_BIO_MAX_ERROR_MSG_LEN - 1);
    }
    msg.utterance_id = bridge->current_utterance_id;
    msg.source_region = source_region;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.errors_reported++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    if (bridge->module_ctx) {
        bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    }

    return 0;
}

/* ============================================================================
 * Semantic/Syntactic Broadcast API
 * ============================================================================ */

int language_bio_bridge_broadcast_semantic_state(
    language_bio_bridge_t* bridge,
    const uint32_t* concept_ids,
    const float* activations,
    uint32_t concept_count,
    float context_coherence,
    uint64_t utterance_id
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_broadcast_semantic_state: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_semantic_broadcast) return 0;
    if (!concept_ids || !activations || concept_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_broadcast_semantic_state: required parameter is NULL (concept_ids, activations)");
        return -1;
    }

    lang_bio_semantic_broadcast_msg_t msg = {0};
    init_message_header(
        &msg.header,
        LANG_MSG_SEMANTIC_BROADCAST,
        bridge->config.semantic_channel,
        0
    );

    msg.concept_count = concept_count;
    msg.concept_ids = (uint32_t*)concept_ids;  /* Pointer, not copied */
    msg.activations = (float*)activations;
    msg.context_coherence = context_coherence;
    msg.topic_id = 0;
    msg.utterance_id = utterance_id;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    if (bridge->module_ctx) {
        bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    }

    return 0;
}

int language_bio_bridge_broadcast_syntactic_state(
    language_bio_bridge_t* bridge,
    parse_state_t parse_state,
    phrase_type_t current_phrase,
    uint32_t parse_depth,
    float parse_probability,
    uint64_t utterance_id
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_broadcast_syntactic_state: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_syntactic_broadcast) return 0;

    lang_bio_syntactic_broadcast_msg_t msg = {0};
    init_message_header(
        &msg.header,
        LANG_MSG_SYNTACTIC_BROADCAST,
        bridge->config.default_channel,
        0
    );

    msg.parse_state = parse_state;
    msg.current_phrase = current_phrase;
    msg.parse_depth = parse_depth;
    msg.parse_probability = parse_probability;
    msg.is_complete = (parse_state == PARSE_STATE_COMPLETE);
    msg.utterance_id = utterance_id;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    if (bridge->module_ctx) {
        bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    }

    return 0;
}

/* ============================================================================
 * Inter-Region Synchronization API
 * ============================================================================ */

int language_bio_bridge_sync_regions(
    language_bio_bridge_t* bridge,
    lang_region_id_t source_region,
    lang_region_id_t target_region,
    const void* data,
    uint32_t data_size,
    uint64_t utterance_id
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_sync_regions: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_region_sync) return 0;

    lang_bio_region_sync_msg_t msg = {0};
    init_message_header(
        &msg.header,
        LANG_MSG_BROCA_WERNICKE_SYNC,
        bridge->config.default_channel,
        0
    );

    msg.source_region = source_region;
    msg.target_region = target_region;
    msg.data = (float*)data;  /* Cast to float* for structure compatibility */
    msg.data_size = data_size;
    msg.utterance_id = utterance_id;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.messages_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    if (bridge->module_ctx) {
        bio_router_send(bridge->module_ctx, &msg, sizeof(msg), 0);
    }

    return 0;
}

int language_bio_bridge_arcuate_transfer(
    language_bio_bridge_t* bridge,
    bool to_broca,
    const void* data,
    uint32_t data_size
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_arcuate_transfer: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_region_sync) return 0;

    lang_region_id_t source = to_broca ? LANG_REGION_WERNICKE : LANG_REGION_BROCA;
    lang_region_id_t target = to_broca ? LANG_REGION_BROCA : LANG_REGION_WERNICKE;

    lang_bio_region_sync_msg_t msg = {0};
    init_message_header(
        &msg.header,
        LANG_MSG_ARCUATE_TRANSFER,
        bridge->config.default_channel,
        0
    );

    msg.source_region = source;
    msg.target_region = target;
    msg.data = (float*)data;
    msg.data_size = data_size;
    msg.utterance_id = bridge->current_utterance_id;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.messages_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    if (bridge->module_ctx) {
        bio_router_send(bridge->module_ctx, &msg, sizeof(msg), 0);
    }

    return 0;
}

/* ============================================================================
 * State Change API
 * ============================================================================ */

int language_bio_bridge_broadcast_state_change(
    language_bio_bridge_t* bridge,
    language_state_t old_state,
    language_state_t new_state,
    language_mode_t mode
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_broadcast_state_change: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    lang_bio_state_change_msg_t msg = {0};
    init_message_header(
        &msg.header,
        LANG_MSG_STATE_CHANGE,
        bridge->config.default_channel,
        0
    );

    msg.old_state = old_state;
    msg.new_state = new_state;
    msg.mode = mode;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    if (bridge->module_ctx) {
        bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    }

    return 0;
}

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

int language_bio_bridge_subscribe(
    language_bio_bridge_t* bridge,
    bio_module_id_t module_id,
    uint64_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_subscribe: bridge is NULL");
        return -1;
    }

    /* Check if already subscribed */
    lang_bio_subscription_t* existing = find_subscription(bridge, module_id);
    if (existing) {
        existing->msg_type_mask = msg_types;
        return 0;
    }

    /* Check capacity */
    if (bridge->subscription_count >= bridge->subscription_capacity) {
        return LANG_BIO_ERROR_SUBSCRIPTION_FULL;
    }

    /* Find empty slot */
    for (uint32_t i = 0; i < bridge->subscription_capacity; i++) {
        if (!bridge->subscriptions[i].active) {
            bridge->subscriptions[i].module_id = module_id;
            bridge->subscriptions[i].msg_type_mask = msg_types;
            bridge->subscriptions[i].active = true;
            bridge->subscriptions[i].subscription_time = get_timestamp_us();
            bridge->subscriptions[i].messages_sent = 0;
            bridge->subscription_count++;

            if (bridge->subscription_count > bridge->stats.peak_subscriptions) {
                bridge->stats.peak_subscriptions = bridge->subscription_count;
            }
            bridge->stats.active_subscriptions = bridge->subscription_count;

            return 0;
        }
    }

    return LANG_BIO_ERROR_SUBSCRIPTION_FULL;
}

int language_bio_bridge_unsubscribe(
    language_bio_bridge_t* bridge,
    bio_module_id_t module_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_unsubscribe: bridge is NULL");
        return -1;
    }

    lang_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        return LANG_BIO_ERROR_MODULE_NOT_FOUND;
    }

    sub->active = false;
    bridge->subscription_count--;
    bridge->stats.active_subscriptions = bridge->subscription_count;

    return 0;
}

int language_bio_bridge_update_subscription(
    language_bio_bridge_t* bridge,
    bio_module_id_t module_id,
    uint64_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_update_subscription: bridge is NULL");
        return -1;
    }

    lang_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        return LANG_BIO_ERROR_MODULE_NOT_FOUND;
    }

    sub->msg_type_mask = msg_types;
    return 0;
}

uint32_t language_bio_bridge_get_subscriber_count(
    const language_bio_bridge_t* bridge,
    lang_bio_msg_type_t msg_type
) {
    if (!bridge) return 0;
    return (uint32_t)count_subscribers_for_type(bridge, msg_type);
}

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

int language_bio_bridge_get_stats(
    const language_bio_bridge_t* bridge,
    language_bio_bridge_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

int language_bio_bridge_reset_stats(language_bio_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_bio_bridge_reset_stats: bridge is NULL");
        return -1;
    }

    uint32_t active_subs = bridge->stats.active_subscriptions;
    uint32_t peak_subs = bridge->stats.peak_subscriptions;

    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->stats.active_subscriptions = active_subs;
    bridge->stats.peak_subscriptions = peak_subs;

    return 0;
}

const char* language_bio_msg_type_name(lang_bio_msg_type_t msg_type) {
    switch (msg_type) {
        case LANG_MSG_UTTERANCE_START:          return "UTTERANCE_START";
        case LANG_MSG_UTTERANCE_END:            return "UTTERANCE_END";
        case LANG_MSG_COMPREHENSION_START:      return "COMPREHENSION_START";
        case LANG_MSG_PHONEME_RECOGNIZED:       return "PHONEME_RECOGNIZED";
        case LANG_MSG_WORD_RECOGNIZED:          return "WORD_RECOGNIZED";
        case LANG_MSG_CONCEPT_ACTIVATED:        return "CONCEPT_ACTIVATED";
        case LANG_MSG_PARSE_UPDATE:             return "PARSE_UPDATE";
        case LANG_MSG_COMPREHENSION_COMPLETE:   return "COMPREHENSION_COMPLETE";
        case LANG_MSG_PRODUCTION_REQUEST:       return "PRODUCTION_REQUEST";
        case LANG_MSG_PRODUCTION_STARTED:       return "PRODUCTION_STARTED";
        case LANG_MSG_WORD_PLANNED:             return "WORD_PLANNED";
        case LANG_MSG_PHONEME_ENCODED:          return "PHONEME_ENCODED";
        case LANG_MSG_MOTOR_COMMAND:            return "MOTOR_COMMAND";
        case LANG_MSG_UTTERANCE_READY:          return "UTTERANCE_READY";
        case LANG_MSG_PRODUCTION_COMPLETE:      return "PRODUCTION_COMPLETE";
        case LANG_MSG_SEMANTIC_ANOMALY:         return "SEMANTIC_ANOMALY";
        case LANG_MSG_SYNTACTIC_ANOMALY:        return "SYNTACTIC_ANOMALY";
        case LANG_MSG_AMBIGUITY_DETECTED:       return "AMBIGUITY_DETECTED";
        case LANG_MSG_GARDEN_PATH:              return "GARDEN_PATH";
        case LANG_MSG_STATE_CHANGE:             return "STATE_CHANGE";
        case LANG_MSG_MODE_CHANGE:              return "MODE_CHANGE";
        case LANG_MSG_ERROR:                    return "ERROR";
        case LANG_MSG_BROCA_WERNICKE_SYNC:      return "BROCA_WERNICKE_SYNC";
        case LANG_MSG_ARCUATE_TRANSFER:         return "ARCUATE_TRANSFER";
        case LANG_MSG_SEMANTIC_BROADCAST:       return "SEMANTIC_BROADCAST";
        case LANG_MSG_SYNTACTIC_BROADCAST:      return "SYNTACTIC_BROADCAST";
        case LANG_MSG_PHONOLOGICAL_BROADCAST:   return "PHONOLOGICAL_BROADCAST";
        case LANG_MSG_WORD_LEARNED:             return "WORD_LEARNED";
        case LANG_MSG_GRAMMAR_LEARNED:          return "GRAMMAR_LEARNED";
        case LANG_MSG_TRAINING_UPDATE:          return "TRAINING_UPDATE";
        case LANG_MSG_PERCEPTION_INPUT:         return "PERCEPTION_INPUT";
        case LANG_MSG_COGNITIVE_REQUEST:        return "COGNITIVE_REQUEST";
        case LANG_MSG_MOTOR_FEEDBACK:           return "MOTOR_FEEDBACK";
        case LANG_MSG_ATTENTION_MODULATION:     return "ATTENTION_MODULATION";
        default:                                return "UNKNOWN";
    }
}

const char* language_bio_region_name(lang_region_id_t region) {
    switch (region) {
        case LANG_REGION_BROCA:         return "Broca's Area";
        case LANG_REGION_WERNICKE:      return "Wernicke's Area";
        case LANG_REGION_STG:           return "Superior Temporal Gyrus";
        case LANG_REGION_MTG:           return "Middle Temporal Gyrus";
        case LANG_REGION_ATL:           return "Anterior Temporal Lobe";
        case LANG_REGION_IFG:           return "Inferior Frontal Gyrus";
        case LANG_REGION_SMA:           return "Supplementary Motor Area";
        case LANG_REGION_PREMOTOR:      return "Premotor Cortex";
        case LANG_REGION_INSULA:        return "Insula";
        case LANG_REGION_ANGULAR_GYRUS: return "Angular Gyrus";
        case LANG_REGION_ARCUATE:       return "Arcuate Fasciculus";
        default:                        return "Unknown Region";
    }
}

void language_bio_bridge_print_summary(const language_bio_bridge_t* bridge) {
    if (!bridge) {
        printf("Language Bio-Async Bridge: NULL\n");
        return;
    }

    printf("=== Language Bio-Async Bridge Summary ===\n");
    printf("Connected: %s\n", bridge->connected ? "Yes" : "No");
    printf("Active Subscriptions: %u (peak: %u)\n",
           bridge->stats.active_subscriptions,
           bridge->stats.peak_subscriptions);
    printf("\n--- Message Statistics ---\n");
    printf("Messages Sent: %lu\n", (unsigned long)bridge->stats.messages_sent);
    printf("Messages Received: %lu\n", (unsigned long)bridge->stats.messages_received);
    printf("Messages Dropped: %lu\n", (unsigned long)bridge->stats.messages_dropped);
    printf("Broadcasts Sent: %lu\n", (unsigned long)bridge->stats.broadcasts_sent);
    printf("\n--- Event Statistics ---\n");
    printf("Utterances Started: %lu\n", (unsigned long)bridge->stats.utterances_started);
    printf("Utterances Completed: %lu\n", (unsigned long)bridge->stats.utterances_completed);
    printf("Words Recognized: %lu\n", (unsigned long)bridge->stats.words_recognized);
    printf("Concepts Activated: %lu\n", (unsigned long)bridge->stats.concepts_activated);
    printf("Productions Completed: %lu\n", (unsigned long)bridge->stats.productions_completed);
    printf("Semantic Anomalies: %lu\n", (unsigned long)bridge->stats.semantic_anomalies);
    printf("Syntactic Anomalies: %lu\n", (unsigned long)bridge->stats.syntactic_anomalies);
    printf("Errors Reported: %lu\n", (unsigned long)bridge->stats.errors_reported);
    printf("\n--- Error Statistics ---\n");
    printf("Handler Errors: %lu\n", (unsigned long)bridge->stats.handler_errors);
    printf("Routing Errors: %lu\n", (unsigned long)bridge->stats.routing_errors);
    printf("==========================================\n");
}
