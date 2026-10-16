/**
 * @file nimcp_hypothalamus_cognitive_hub_bridge.c
 * @brief Hypothalamus - Cognitive Integration Hub Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bridge connecting the hypothalamus orchestrator to the cognitive integration hub
 * WHY:  Enable bidirectional communication between drive states and cognitive processing
 * HOW:  Implements event subscription, publication, and drive-cognitive coordination
 *
 * BIOLOGICAL BASIS:
 * - Hypothalamus-cortex connections: Drive states influence all cognitive processing
 * - Prefrontal-hypothalamus feedback: Executive control modulates drive expression
 * - Stress axis (HPA): Cortisol affects memory, attention, decision-making
 *
 * @author NIMCP Development Team
 */

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_cognitive_hub_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_orchestrator.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"
#include "async/nimcp_bio_router.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_cognitive_hub_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HYPOTHALAMUS_COGNITIVE_HUB_BRIDGE"


/* ============================================================================
 * INTERNAL CONSTANTS
 * ============================================================================ */

#define HYPO_COG_HUB_BRIDGE_NAME "HypothalamusDrives"

/** Bio-async module ID for hypothalamus cognitive bridge */
#define HYPO_COG_BIO_ASYNC_MODULE_ID 0x48594347  /* "HYCG" */

/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/**
 * @brief Internal hypothalamus-cognitive hub bridge structure
 */
struct hypo_cognitive_hub_bridge {
    bridge_base_t base;                       /**< MUST be first: base bridge infrastructure */

    hypo_cognitive_hub_config_t config;       /**< Bridge configuration */

    /* External connections */
    hypo_orchestrator_t orch;                 /**< Connected orchestrator */
    cognitive_integration_hub_t cog_hub;      /**< Connected cognitive hub */
    uint32_t orch_bridge_id;                  /**< Bridge ID in orchestrator */

    /* Bio-async integration */
    bio_module_context_t bio_ctx;             /**< Bio-async module context */
    bool bio_async_connected;                 /**< Bio-async connection status */

    /* State */
    bool initialized;                         /**< Initialization flag */
    bool connected;                           /**< Connection status */
    hypo_cognitive_hub_state_t cognitive_state; /**< Received cognitive state */

    /* Timing */
    uint64_t last_broadcast_time_ms;          /**< Last drive broadcast time */
    uint64_t accumulated_time_ms;             /**< Accumulated time since broadcast */

    /* Callbacks */
    hypo_cog_emotion_callback_t emotion_callback;
    void* emotion_callback_user_data;
    hypo_cog_attention_callback_t attention_callback;
    void* attention_callback_user_data;
    hypo_cog_decision_callback_t decision_callback;
    void* decision_callback_user_data;
    hypo_cog_learning_callback_t learning_callback;
    void* learning_callback_user_data;

    /* Statistics */
    hypo_cognitive_hub_stats_t stats;         /**< Bridge statistics */

};

/* ============================================================================
 * INTERNAL HELPER FUNCTIONS
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    }
    return 0;
}

/**
 * @brief Convert hypo urgency to cognitive priority
 */
static cognitive_event_priority_t urgency_to_priority(hypo_urgency_t urgency) {
    switch (urgency) {
        case HYPO_URGENCY_URGENT:
            return COG_PRIORITY_CRITICAL;
        case HYPO_URGENCY_ELEVATED:
            return COG_PRIORITY_HIGH;
        case HYPO_URGENCY_MODERATE:
            return COG_PRIORITY_NORMAL;
        case HYPO_URGENCY_LOW:
        case HYPO_URGENCY_NONE:
        default:
            return COG_PRIORITY_LOW;
    }
}

/* ============================================================================
 * COGNITIVE HUB EVENT CALLBACK
 * ============================================================================ */

/**
 * @brief Internal callback for cognitive hub events
 *
 * WHAT: Handle events from cognitive hub
 * WHY:  Process cognitive state updates and dispatch to user callbacks
 * HOW:  Parse event type and payload, update internal state, invoke callbacks
 */
static int hypo_cog_on_event(const cognitive_event_data_t* event, void* user_data) {
    if (!event || !user_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cog_on_event: required parameter is NULL (event, user_data)");
        return -1;
    }

    hypo_cognitive_hub_bridge_t* bridge = (hypo_cognitive_hub_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update statistics */
    bridge->stats.events_received++;
    bridge->stats.last_receive_timestamp = get_timestamp_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Dispatch based on event type */
    switch (event->event_type) {
        case COG_EVENT_EMOTION_UPDATE: {
            nimcp_mutex_lock(bridge->base.mutex);

            /* Update cognitive state */
            if (event->payload && event->payload_size >= sizeof(float) * 2) {
                const float* emotion_data = (const float*)event->payload;
                float valence = nimcp_clampf(emotion_data[0], -1.0f, 1.0f);
                float arousal = nimcp_clampf(emotion_data[1], 0.0f, 1.0f);

                bridge->cognitive_state.emotional_arousal = arousal;
                bridge->cognitive_state.last_update_timestamp = get_timestamp_ms();

                /* Extract callback safely */
                hypo_cog_emotion_callback_t callback = bridge->emotion_callback;
                void* cb_data = bridge->emotion_callback_user_data;

                bridge->stats.emotion_updates_received++;

                nimcp_mutex_unlock(bridge->base.mutex);

                /* Invoke callback outside lock */
                if (callback) {
                    uint32_t emotion_type = (event->payload_size > sizeof(float) * 2) ?
                        *((const uint32_t*)(emotion_data + 2)) : 0;
                    callback(valence, arousal, emotion_type, cb_data);
                }
            } else {
                nimcp_mutex_unlock(bridge->base.mutex);
            }
            break;
        }

        case COG_EVENT_ATTENTION_SHIFT: {
            nimcp_mutex_lock(bridge->base.mutex);

            /* Update cognitive state */
            float attention_level = nimcp_clampf(
                (float)event->priority / (float)COG_PRIORITY_CRITICAL, 0.0f, 1.0f);
            bridge->cognitive_state.attention_demand = attention_level;
            bridge->cognitive_state.last_update_timestamp = get_timestamp_ms();

            /* Extract callback safely */
            hypo_cog_attention_callback_t callback = bridge->attention_callback;
            void* cb_data = bridge->attention_callback_user_data;

            bridge->stats.attention_updates_received++;

            nimcp_mutex_unlock(bridge->base.mutex);

            /* Invoke callback outside lock */
            if (callback && event->payload) {
                const uint64_t* focus_id = (const uint64_t*)event->payload;
                callback(*focus_id, attention_level, cb_data);
            }
            break;
        }

        case COG_EVENT_DECISION_MADE: {
            nimcp_mutex_lock(bridge->base.mutex);

            /* Update cognitive state - decision indicates executive activity */
            bridge->cognitive_state.executive_control = 0.8f;  /* High control during decision */
            bridge->cognitive_state.last_update_timestamp = get_timestamp_ms();

            /* Extract callback safely */
            hypo_cog_decision_callback_t callback = bridge->decision_callback;
            void* cb_data = bridge->decision_callback_user_data;

            bridge->stats.decision_updates_received++;

            nimcp_mutex_unlock(bridge->base.mutex);

            /* Invoke callback outside lock */
            if (callback && event->payload && event->payload_size >= sizeof(uint64_t) + sizeof(bool)) {
                const uint64_t* decision_id = (const uint64_t*)event->payload;
                const bool* success = (const bool*)(decision_id + 1);
                float reward = 0.5f;  /* Default reward */
                if (event->payload_size >= sizeof(uint64_t) + sizeof(bool) + sizeof(float)) {
                    reward = *((const float*)(success + 1));
                }
                callback(*decision_id, *success, reward, cb_data);
            }
            break;
        }

        case COG_EVENT_LEARNING_COMPLETE: {
            nimcp_mutex_lock(bridge->base.mutex);

            /* Update cognitive state */
            bridge->cognitive_state.learning_activity = 0.9f;  /* High learning activity */
            bridge->cognitive_state.last_update_timestamp = get_timestamp_ms();

            /* Extract callback safely */
            hypo_cog_learning_callback_t callback = bridge->learning_callback;
            void* cb_data = bridge->learning_callback_user_data;

            bridge->stats.learning_updates_received++;

            nimcp_mutex_unlock(bridge->base.mutex);

            /* Invoke callback outside lock */
            if (callback && event->payload && event->payload_size >= sizeof(uint64_t)) {
                const uint64_t* learning_id = (const uint64_t*)event->payload;
                float skill = 0.5f, info_gain = 0.5f;
                if (event->payload_size >= sizeof(uint64_t) + sizeof(float) * 2) {
                    const float* floats = (const float*)(learning_id + 1);
                    skill = floats[0];
                    info_gain = floats[1];
                }
                callback(*learning_id, skill, info_gain, cb_data);
            }
            break;
        }

        case COG_EVENT_CONSOLIDATION: {
            nimcp_mutex_lock(bridge->base.mutex);

            /* Update cognitive state - memory consolidation pressure */
            bridge->cognitive_state.memory_consolidation = 0.8f;
            bridge->cognitive_state.last_update_timestamp = get_timestamp_ms();

            nimcp_mutex_unlock(bridge->base.mutex);
            break;
        }

        default:
            /* Unhandled event type - not an error */
            break;
    }

    return 0;
}

/**
 * @brief Internal query handler for hypothalamus state queries
 */
static int hypo_cog_query_handler(
    const cognitive_query_t* query,
    cognitive_query_result_t* result,
    void* context
) {
    if (!query || !result || !context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cog_query_handler: required parameter is NULL (query, result, context)");
        return -1;
    }

    hypo_cognitive_hub_bridge_t* bridge = (hypo_cognitive_hub_bridge_t*)context;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.queries_handled++;
    hypo_orchestrator_t orch = bridge->orch;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Initialize result */
    result->status = 0;
    result->result_data = NULL;
    result->result_size = 0;
    result->error_message[0] = '\0';

    if (!orch) {
        strncpy(result->error_message, "Orchestrator not connected",
                sizeof(result->error_message) - 1);
        result->status = -1;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cog_query_handler: orch is NULL");
        return -1;
    }

    /* Handle query based on type */
    switch (query->query_type) {
        case COG_QUERY_STATUS: {
            /* Return orchestrator state */
            hypo_orch_state_t state;
            if (hypo_orch_get_state(orch, &state) == 0) {
                const char* state_name = hypo_orch_state_name(state);
                size_t len = strlen(state_name) + 1;
                char* status_str = nimcp_malloc(len);
                if (status_str) {
                    strncpy(status_str, state_name, len);
                    result->result_data = status_str;
                    result->result_size = len;
                }
            } else {
                strncpy(result->error_message, "Failed to get orchestrator state",
                        sizeof(result->error_message) - 1);
                result->status = -1;
            }
            break;
        }

        case COG_QUERY_STATE: {
            /* Return drive state */
            hypo_unified_drive_state_t drive_state;
            if (hypo_orch_get_drive_state(orch, &drive_state) == 0) {
                /* Create summary struct */
                typedef struct {
                    float unified_drive_level;
                    uint32_t urgency;
                    uint32_t active_drives;
                    uint32_t primary_drive_type;
                } drive_state_summary_t;

                drive_state_summary_t* summary = nimcp_malloc(sizeof(drive_state_summary_t));
                if (summary) {
                    summary->unified_drive_level = drive_state.unified_drive_level;
                    summary->urgency = (uint32_t)drive_state.urgency;
                    summary->active_drives = drive_state.active_drives;
                    summary->primary_drive_type = drive_state.primary_drive_type;

                    result->result_data = summary;
                    result->result_size = sizeof(drive_state_summary_t);
                }
            } else {
                strncpy(result->error_message, "Failed to get drive state",
                        sizeof(result->error_message) - 1);
                result->status = -1;
            }
            break;
        }

        case COG_QUERY_METRICS: {
            /* Return orchestrator statistics */
            hypo_orch_stats_t stats;
            if (hypo_orch_get_stats(orch, &stats) == 0) {
                typedef struct {
                    uint32_t registered_bridges;
                    uint64_t events_published;
                    uint64_t drives_activated;
                    float avg_drive_level;
                } hypo_metrics_t;

                hypo_metrics_t* metrics = nimcp_malloc(sizeof(hypo_metrics_t));
                if (metrics) {
                    metrics->registered_bridges = stats.registered_bridges;
                    metrics->events_published = stats.events_published;
                    metrics->drives_activated = stats.drives_activated;
                    metrics->avg_drive_level = stats.avg_drive_level;

                    result->result_data = metrics;
                    result->result_size = sizeof(hypo_metrics_t);
                }
            } else {
                strncpy(result->error_message, "Failed to get statistics",
                        sizeof(result->error_message) - 1);
                result->status = -1;
            }
            break;
        }

        default:
            strncpy(result->error_message, "Unsupported query type",
                    sizeof(result->error_message) - 1);
            result->status = -1;
            break;
    }

    return result->status;
}

/* ============================================================================
 * ORCHESTRATOR EVENT CALLBACK
 * ============================================================================ */

/**
 * @brief Internal callback for hypothalamus orchestrator events
 *
 * WHAT: Handle drive events from orchestrator
 * WHY:  Broadcast significant drive changes to cognitive hub
 * HOW:  Check event type and significance, publish to cognitive hub
 */
static int hypo_cog_on_orch_event(const hypo_event_data_t* event, void* user_data) {
    if (!event || !user_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cog_on_orch_event: required parameter is NULL (event, user_data)");
        return -1;
    }

    hypo_cognitive_hub_bridge_t* bridge = (hypo_cognitive_hub_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if broadcasting is enabled */
    if (!bridge->config.enable_drive_broadcast || !bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Check if we should broadcast based on urgency filter */
    if (bridge->config.broadcast_urgency_only &&
        event->urgency < HYPO_URGENCY_ELEVATED) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    cognitive_integration_hub_t hub = bridge->cog_hub;
    uint32_t module_id = bridge->config.module_id;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Handle significant events */
    switch (event->event_type) {
        case HYPO_EVENT_DRIVE_ACTIVATED:
        case HYPO_EVENT_DRIVE_SATISFIED:
        case HYPO_EVENT_DRIVE_CONFLICT:
        case HYPO_EVENT_HOMEOSTATIC_ALERT: {
            /* Broadcast drive change to cognitive hub */
            hypo_drive_broadcast_payload_t payload;
            memset(&payload, 0, sizeof(payload));

            payload.primary_drive_type = event->drive.drive_type;
            payload.primary_drive_level = event->drive.drive_level;
            payload.urgency_level = (uint32_t)event->urgency;
            payload.unified_drive_level = event->drive.drive_level;  /* Simplified */
            payload.timestamp = event->timestamp;

            if (event->event_type == HYPO_EVENT_DRIVE_CONFLICT) {
                payload.in_conflict = true;
            }

            cognitive_event_data_t cog_event;
            cog_event.event_type = COG_EVENT_STATE_CHANGE;
            cog_event.source_module_id = module_id;
            cog_event.timestamp = event->timestamp;
            cog_event.priority = urgency_to_priority(event->urgency);
            cog_event.payload = &payload;
            cog_event.payload_size = sizeof(payload);

            /* Publish asynchronously to avoid blocking */
            cognitive_hub_publish_async(hub, module_id, COG_EVENT_STATE_CHANGE, &cog_event);

            nimcp_mutex_lock(bridge->base.mutex);
            bridge->stats.events_published++;
            bridge->stats.drive_broadcasts++;
            nimcp_mutex_unlock(bridge->base.mutex);
            break;
        }

        case HYPO_EVENT_STRESS_RESPONSE: {
            /* Propagate stress if enabled */
            nimcp_mutex_lock(bridge->base.mutex);
            bool propagate = bridge->config.enable_stress_propagation &&
                             event->stress.stress_level >= bridge->config.stress_propagation_threshold;
            nimcp_mutex_unlock(bridge->base.mutex);

            if (propagate) {
                hypo_cognitive_hub_propagate_stress(bridge, event->stress.stress_level);
            }
            break;
        }

        default:
            break;
    }

    return 0;
}

/* ============================================================================
 * LIFECYCLE IMPLEMENTATION
 * ============================================================================ */

int hypo_cognitive_hub_default_config(hypo_cognitive_hub_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    config->module_id = HYPO_COG_HUB_DEFAULT_MODULE_ID;

    /* Drive broadcasting */
    config->enable_drive_broadcast = true;
    config->drive_update_interval_ms = HYPO_COG_HUB_DEFAULT_UPDATE_INTERVAL_MS;
    config->broadcast_on_change = true;
    config->broadcast_urgency_only = false;

    /* Cognitive feedback */
    config->enable_cognitive_feedback = true;
    config->cognitive_weight = HYPO_COG_HUB_DEFAULT_COGNITIVE_WEIGHT;
    config->auto_subscribe_emotion = true;
    config->auto_subscribe_attention = true;
    config->auto_subscribe_decision = true;
    config->auto_subscribe_learning = true;

    /* Stress/fatigue propagation */
    config->enable_stress_propagation = true;
    config->enable_fatigue_modulation = true;
    config->stress_propagation_threshold = 0.5f;
    config->fatigue_threshold = 0.6f;

    /* Query handling */
    config->enable_query_handler = true;

    /* Event buffering */
    config->event_buffer_size = HYPO_COG_HUB_MAX_EVENT_BUFFER;

    return 0;
}

hypo_cognitive_hub_bridge_t* hypo_cognitive_hub_create(
    const hypo_cognitive_hub_config_t* config
) {
    /* Allocate bridge structure */
    hypo_cognitive_hub_bridge_t* bridge = (hypo_cognitive_hub_bridge_t*)nimcp_calloc(
        1, sizeof(hypo_cognitive_hub_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_cognitive_hub_create: allocation failed");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        hypo_cognitive_hub_default_config(&bridge->config);
    }

    /* Create mutex for thread safety */
    if (bridge_base_init(&bridge->base, 0, "hypothalamus_cognitive_hub") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_cognitive_hub_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->orch = NULL;
    bridge->cog_hub = NULL;
    bridge->orch_bridge_id = 0;
    bridge->bio_ctx = NULL;
    bridge->bio_async_connected = false;
    bridge->connected = false;

    /* Initialize timing */
    bridge->last_broadcast_time_ms = get_timestamp_ms();
    bridge->accumulated_time_ms = 0;

    /* Initialize callbacks */
    bridge->emotion_callback = NULL;
    bridge->emotion_callback_user_data = NULL;
    bridge->attention_callback = NULL;
    bridge->attention_callback_user_data = NULL;
    bridge->decision_callback = NULL;
    bridge->decision_callback_user_data = NULL;
    bridge->learning_callback = NULL;
    bridge->learning_callback_user_data = NULL;

    /* Initialize cognitive state */
    memset(&bridge->cognitive_state, 0, sizeof(hypo_cognitive_hub_state_t));

    /* Initialize stats */
    memset(&bridge->stats, 0, sizeof(hypo_cognitive_hub_stats_t));

    bridge->initialized = true;

    return bridge;
}

void hypo_cognitive_hub_destroy(hypo_cognitive_hub_bridge_t* bridge) {
    if (!bridge) {
        return;
    }
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hypothalamus_cognitive_hub");

    /* Disconnect if connected */
    if (bridge->connected) {
        hypo_cognitive_hub_disconnect(bridge);
    }

    /* Disconnect bio-async if connected */
    if (bridge->bio_async_connected) {
        hypo_cognitive_hub_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
        bridge->base.mutex = NULL;
    }

    bridge->initialized = false;

    nimcp_free(bridge);
}

int hypo_cognitive_hub_reset(hypo_cognitive_hub_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_reset: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset timing */
    bridge->last_broadcast_time_ms = get_timestamp_ms();
    bridge->accumulated_time_ms = 0;

    /* Reset cognitive state */
    memset(&bridge->cognitive_state, 0, sizeof(hypo_cognitive_hub_state_t));

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(hypo_cognitive_hub_stats_t));

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * CONNECTION IMPLEMENTATION
 * ============================================================================ */

int hypo_cognitive_hub_connect(
    hypo_cognitive_hub_bridge_t* bridge,
    hypo_orchestrator_t orch,
    cognitive_integration_hub_t hub
) {
    if (!bridge || !bridge->initialized || !orch || !hub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_connect: required parameter is NULL (bridge, bridge->initialized, orch, hub)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if already connected */
    if (bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_cognitive_hub_connect: validation failed");
        return -1;
    }

    /* Store references */
    bridge->orch = orch;
    bridge->cog_hub = hub;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Register with cognitive hub */
    int result = cognitive_hub_register_module(
        hub,
        bridge->config.module_id,
        COG_CATEGORY_EXECUTIVE,  /* Drives steer all behavior */
        HYPO_COG_HUB_BRIDGE_NAME,
        bridge
    );

    if (result != 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->orch = NULL;
        bridge->cog_hub = NULL;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_cognitive_hub_connect: validation failed");
        return -1;
    }

    /* Subscribe to cognitive events if enabled */
    if (bridge->config.enable_cognitive_feedback) {
        if (bridge->config.auto_subscribe_emotion) {
            cognitive_hub_subscribe(
                hub,
                bridge->config.module_id,
                COG_EVENT_EMOTION_UPDATE,
                hypo_cog_on_event,
                bridge
            );
        }

        if (bridge->config.auto_subscribe_attention) {
            cognitive_hub_subscribe(
                hub,
                bridge->config.module_id,
                COG_EVENT_ATTENTION_SHIFT,
                hypo_cog_on_event,
                bridge
            );
        }

        if (bridge->config.auto_subscribe_decision) {
            cognitive_hub_subscribe(
                hub,
                bridge->config.module_id,
                COG_EVENT_DECISION_MADE,
                hypo_cog_on_event,
                bridge
            );
        }

        if (bridge->config.auto_subscribe_learning) {
            cognitive_hub_subscribe(
                hub,
                bridge->config.module_id,
                COG_EVENT_LEARNING_COMPLETE,
                hypo_cog_on_event,
                bridge
            );

            /* Also subscribe to consolidation for memory pressure */
            cognitive_hub_subscribe(
                hub,
                bridge->config.module_id,
                COG_EVENT_CONSOLIDATION,
                hypo_cog_on_event,
                bridge
            );
        }
    }

    /* Register query handler if enabled */
    if (bridge->config.enable_query_handler) {
        cognitive_hub_register_query_handler(
            hub,
            bridge->config.module_id,
            hypo_cog_query_handler
        );
    }

    /* Register with hypothalamus orchestrator for drive events */
    uint32_t bridge_id = 0;
    result = hypo_orch_register_bridge(
        orch,
        HYPO_BRIDGE_GLOBAL_WORKSPACE,  /* We integrate with cognitive processing */
        HYPO_COG_HUB_BRIDGE_NAME,
        bridge,
        bridge,
        &bridge_id
    );

    if (result == 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->orch_bridge_id = bridge_id;
        nimcp_mutex_unlock(bridge->base.mutex);

        /* Subscribe to drive events */
        hypo_orch_subscribe(
            orch,
            bridge_id,
            HYPO_EVENT_DRIVE_ACTIVATED,
            hypo_cog_on_orch_event,
            bridge
        );

        hypo_orch_subscribe(
            orch,
            bridge_id,
            HYPO_EVENT_DRIVE_SATISFIED,
            hypo_cog_on_orch_event,
            bridge
        );

        hypo_orch_subscribe(
            orch,
            bridge_id,
            HYPO_EVENT_DRIVE_CONFLICT,
            hypo_cog_on_orch_event,
            bridge
        );

        hypo_orch_subscribe(
            orch,
            bridge_id,
            HYPO_EVENT_STRESS_RESPONSE,
            hypo_cog_on_orch_event,
            bridge
        );

        hypo_orch_subscribe(
            orch,
            bridge_id,
            HYPO_EVENT_HOMEOSTATIC_ALERT,
            hypo_cog_on_orch_event,
            bridge
        );
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->connected = true;
    bridge->last_broadcast_time_ms = get_timestamp_ms();
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int hypo_cognitive_hub_disconnect(hypo_cognitive_hub_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_disconnect: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_disconnect: bridge->connected is NULL");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->cog_hub;
    hypo_orchestrator_t orch = bridge->orch;
    uint32_t module_id = bridge->config.module_id;
    uint32_t orch_bridge_id = bridge->orch_bridge_id;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Unsubscribe from cognitive hub events */
    if (hub) {
        if (bridge->config.auto_subscribe_emotion) {
            cognitive_hub_unsubscribe(hub, module_id, COG_EVENT_EMOTION_UPDATE);
        }
        if (bridge->config.auto_subscribe_attention) {
            cognitive_hub_unsubscribe(hub, module_id, COG_EVENT_ATTENTION_SHIFT);
        }
        if (bridge->config.auto_subscribe_decision) {
            cognitive_hub_unsubscribe(hub, module_id, COG_EVENT_DECISION_MADE);
        }
        if (bridge->config.auto_subscribe_learning) {
            cognitive_hub_unsubscribe(hub, module_id, COG_EVENT_LEARNING_COMPLETE);
            cognitive_hub_unsubscribe(hub, module_id, COG_EVENT_CONSOLIDATION);
        }

        /* Unregister module */
        cognitive_hub_unregister_module(hub, module_id);
    }

    /* Unregister from orchestrator */
    if (orch && orch_bridge_id != 0) {
        hypo_orch_unsubscribe(orch, orch_bridge_id, HYPO_EVENT_DRIVE_ACTIVATED);
        hypo_orch_unsubscribe(orch, orch_bridge_id, HYPO_EVENT_DRIVE_SATISFIED);
        hypo_orch_unsubscribe(orch, orch_bridge_id, HYPO_EVENT_DRIVE_CONFLICT);
        hypo_orch_unsubscribe(orch, orch_bridge_id, HYPO_EVENT_STRESS_RESPONSE);
        hypo_orch_unsubscribe(orch, orch_bridge_id, HYPO_EVENT_HOMEOSTATIC_ALERT);

        hypo_orch_unregister_bridge(orch, orch_bridge_id);
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->orch = NULL;
    bridge->cog_hub = NULL;
    bridge->orch_bridge_id = 0;
    bridge->connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool hypo_cognitive_hub_is_connected(const hypo_cognitive_hub_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        return false;
    }

    /* Cast away const for mutex lock */
    hypo_cognitive_hub_bridge_t* mutable_bridge = (hypo_cognitive_hub_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);
    bool connected = bridge->connected;
    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * UPDATE IMPLEMENTATION
 * ============================================================================ */

int hypo_cognitive_hub_update(
    hypo_cognitive_hub_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_update: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Not an error, just nothing to do */
    }

    /* Accumulate time */
    bridge->accumulated_time_ms += delta_ms;

    /* Check if we should broadcast */
    bool should_broadcast = false;
    if (bridge->config.enable_drive_broadcast &&
        bridge->config.drive_update_interval_ms > 0) {

        if ((float)bridge->accumulated_time_ms >= bridge->config.drive_update_interval_ms) {
            should_broadcast = true;
            bridge->accumulated_time_ms = 0;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Broadcast drives if interval elapsed */
    if (should_broadcast) {
        hypo_cognitive_hub_broadcast_drives(bridge);
    }

    /* Decay cognitive state values over time */
    nimcp_mutex_lock(bridge->base.mutex);
    float decay = 1.0f - (float)delta_ms / 10000.0f;  /* Decay over 10 seconds */
    if (decay < 0.0f) decay = 0.0f;

    bridge->cognitive_state.cognitive_load *= decay;
    bridge->cognitive_state.executive_control *= decay;
    bridge->cognitive_state.learning_activity *= decay;
    bridge->cognitive_state.memory_consolidation *= decay;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int hypo_cognitive_hub_broadcast_drives(hypo_cognitive_hub_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_broadcast_drives: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected || !bridge->orch || !bridge->cog_hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_broadcast_drives: required parameter is NULL (bridge->connected, bridge->orch, bridge->cog_hub)");
        return -1;
    }

    hypo_orchestrator_t orch = bridge->orch;
    cognitive_integration_hub_t hub = bridge->cog_hub;
    uint32_t module_id = bridge->config.module_id;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Get current drive state from orchestrator */
    hypo_unified_drive_state_t drive_state;
    if (hypo_orch_get_drive_state(orch, &drive_state) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_cognitive_hub_broadcast_drives: validation failed");
        return -1;
    }

    /* Check stress state */
    bool in_stress = false;
    hypo_orch_is_stressed(orch, &in_stress);

    /* Create broadcast payload */
    hypo_drive_broadcast_payload_t payload;
    memset(&payload, 0, sizeof(payload));

    payload.primary_drive_type = drive_state.primary_drive_type;
    payload.primary_drive_level = drive_state.bridge_drives[drive_state.primary_source].drive_level;
    payload.urgency_level = (uint32_t)drive_state.urgency;
    payload.unified_drive_level = drive_state.unified_drive_level;
    payload.arousal_level = 0.5f;  /* Would come from arousal system */
    payload.stress_level = in_stress ? 0.8f : 0.2f;
    payload.fatigue_level = 0.0f;  /* Would come from sleep system */
    payload.active_drive_count = drive_state.active_drives;
    payload.in_conflict = (drive_state.active_drives > 1);
    payload.in_stress_response = in_stress;
    payload.timestamp = get_timestamp_ms();

    /* Create cognitive event */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_STATE_CHANGE;
    event.source_module_id = module_id;
    event.timestamp = payload.timestamp * 1000;  /* Convert to microseconds */
    event.priority = urgency_to_priority(drive_state.urgency);
    event.payload = &payload;
    event.payload_size = sizeof(payload);

    /* Publish to hub */
    int result = cognitive_hub_publish(hub, module_id, COG_EVENT_STATE_CHANGE, &event);

    if (result == 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.events_published++;
        bridge->stats.drive_broadcasts++;
        bridge->stats.last_broadcast_timestamp = payload.timestamp;
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return result;
}

int hypo_cognitive_hub_receive_cognitive_state(hypo_cognitive_hub_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_receive_cognitive_state: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Cognitive state is updated via event callbacks - this is a sync point */
    nimcp_mutex_lock(bridge->base.mutex);

    /* Update cognitive load based on accumulated state */
    float load = 0.0f;
    load += bridge->cognitive_state.attention_demand * 0.3f;
    load += bridge->cognitive_state.executive_control * 0.3f;
    load += bridge->cognitive_state.learning_activity * 0.2f;
    load += bridge->cognitive_state.memory_consolidation * 0.2f;

    bridge->cognitive_state.cognitive_load = nimcp_clampf(load, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * DRIVE MODULATION IMPLEMENTATION
 * ============================================================================ */

int hypo_cognitive_hub_modulate_curiosity(
    hypo_cognitive_hub_bridge_t* bridge,
    float info_gain
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_modulate_curiosity: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected || !bridge->orch) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_modulate_curiosity: required parameter is NULL (bridge->connected, bridge->orch)");
        return -1;
    }

    hypo_orchestrator_t orch = bridge->orch;
    uint32_t bridge_id = bridge->orch_bridge_id;
    float weight = bridge->config.cognitive_weight;

    bridge->stats.curiosity_modulations++;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Report drive satisfaction to orchestrator */
    float satisfaction = nimcp_clampf(info_gain * weight, 0.0f, 1.0f);

    return hypo_orch_report_drive(
        orch,
        bridge_id,
        HYPO_DRIVE_CURIOSITY,
        1.0f - satisfaction,  /* Drive level decreases with satisfaction */
        satisfaction > 0.5f ? HYPO_URGENCY_LOW : HYPO_URGENCY_MODERATE,
        "Curiosity modulated by information gain"
    );
}

int hypo_cognitive_hub_modulate_social(
    hypo_cognitive_hub_bridge_t* bridge,
    float social_reward
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_modulate_social: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected || !bridge->orch) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_modulate_social: required parameter is NULL (bridge->connected, bridge->orch)");
        return -1;
    }

    hypo_orchestrator_t orch = bridge->orch;
    uint32_t bridge_id = bridge->orch_bridge_id;
    float weight = bridge->config.cognitive_weight;

    bridge->stats.social_modulations++;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Report drive satisfaction to orchestrator */
    float satisfaction = nimcp_clampf(social_reward * weight, 0.0f, 1.0f);

    return hypo_orch_report_drive(
        orch,
        bridge_id,
        HYPO_DRIVE_SOCIAL,
        1.0f - satisfaction,
        satisfaction > 0.5f ? HYPO_URGENCY_LOW : HYPO_URGENCY_MODERATE,
        "Social drive modulated by social reward"
    );
}

int hypo_cognitive_hub_modulate_competence(
    hypo_cognitive_hub_bridge_t* bridge,
    float skill_acquisition
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_modulate_competence: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected || !bridge->orch) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_modulate_competence: required parameter is NULL (bridge->connected, bridge->orch)");
        return -1;
    }

    hypo_orchestrator_t orch = bridge->orch;
    uint32_t bridge_id = bridge->orch_bridge_id;
    float weight = bridge->config.cognitive_weight;

    bridge->stats.competence_modulations++;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Report drive satisfaction to orchestrator */
    float satisfaction = nimcp_clampf(skill_acquisition * weight, 0.0f, 1.0f);

    return hypo_orch_report_drive(
        orch,
        bridge_id,
        HYPO_DRIVE_COMPETENCE,
        1.0f - satisfaction,
        satisfaction > 0.5f ? HYPO_URGENCY_LOW : HYPO_URGENCY_MODERATE,
        "Competence drive modulated by skill acquisition"
    );
}

/* ============================================================================
 * STRESS/FATIGUE PROPAGATION IMPLEMENTATION
 * ============================================================================ */

int hypo_cognitive_hub_propagate_stress(
    hypo_cognitive_hub_bridge_t* bridge,
    float stress_level
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_propagate_stress: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected || !bridge->cog_hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_propagate_stress: required parameter is NULL (bridge->connected, bridge->cog_hub)");
        return -1;
    }

    if (!bridge->config.enable_stress_propagation) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    if (stress_level < bridge->config.stress_propagation_threshold) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    cognitive_integration_hub_t hub = bridge->cog_hub;
    uint32_t module_id = bridge->config.module_id;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Create stress propagation payload */
    hypo_stress_propagation_payload_t payload;
    memset(&payload, 0, sizeof(payload));

    stress_level = nimcp_clampf(stress_level, 0.0f, 1.0f);

    payload.stress_level = stress_level;
    payload.cortisol_level = stress_level * 0.8f;  /* Simplified cortisol model */
    payload.stressor_type = 0;  /* Generic stressor */
    payload.is_acute = (stress_level > 0.8f);

    /* Calculate cognitive effects */
    /* High stress reduces cognitive capacity */
    payload.cognitive_capacity_mod = 1.0f - (stress_level * 0.4f);
    /* Stress biases attention toward threats */
    payload.attention_bias = stress_level * 0.5f;
    /* Stress enhances emotional memory encoding */
    payload.memory_encoding_boost = stress_level * 0.3f;

    payload.timestamp = get_timestamp_ms();

    /* Create cognitive event */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_STATE_CHANGE;
    event.source_module_id = module_id;
    event.timestamp = payload.timestamp * 1000;
    event.priority = (stress_level > 0.8f) ? COG_PRIORITY_CRITICAL : COG_PRIORITY_HIGH;
    event.payload = &payload;
    event.payload_size = sizeof(payload);

    /* Publish to hub */
    int result = cognitive_hub_publish(hub, module_id, COG_EVENT_STATE_CHANGE, &event);

    if (result == 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.events_published++;
        bridge->stats.stress_propagations++;
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return result;
}

int hypo_cognitive_hub_propagate_fatigue(
    hypo_cognitive_hub_bridge_t* bridge,
    float fatigue_level
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_propagate_fatigue: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected || !bridge->cog_hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_propagate_fatigue: required parameter is NULL (bridge->connected, bridge->cog_hub)");
        return -1;
    }

    if (!bridge->config.enable_fatigue_modulation) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    if (fatigue_level < bridge->config.fatigue_threshold) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    cognitive_integration_hub_t hub = bridge->cog_hub;
    uint32_t module_id = bridge->config.module_id;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Create fatigue modulation payload */
    hypo_fatigue_modulation_payload_t payload;
    memset(&payload, 0, sizeof(payload));

    fatigue_level = nimcp_clampf(fatigue_level, 0.0f, 1.0f);

    payload.fatigue_level = fatigue_level;
    payload.alertness = 1.0f - fatigue_level;
    payload.sleep_pressure = fatigue_level * 0.8f;

    /* Calculate cognitive capacity modulations */
    /* Fatigue reduces all cognitive capacities */
    payload.cognitive_capacity_mod = 1.0f - (fatigue_level * 0.5f);
    payload.attention_capacity_mod = 1.0f - (fatigue_level * 0.6f);
    payload.processing_speed_mod = 1.0f - (fatigue_level * 0.4f);

    payload.timestamp = get_timestamp_ms();

    /* Create cognitive event */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_STATE_CHANGE;
    event.source_module_id = module_id;
    event.timestamp = payload.timestamp * 1000;
    event.priority = (fatigue_level > 0.9f) ? COG_PRIORITY_HIGH : COG_PRIORITY_NORMAL;
    event.payload = &payload;
    event.payload_size = sizeof(payload);

    /* Publish to hub */
    int result = cognitive_hub_publish(hub, module_id, COG_EVENT_STATE_CHANGE, &event);

    if (result == 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.events_published++;
        bridge->stats.fatigue_modulations++;
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return result;
}

/* ============================================================================
 * CALLBACK REGISTRATION IMPLEMENTATION
 * ============================================================================ */

int hypo_cognitive_hub_set_emotion_callback(
    hypo_cognitive_hub_bridge_t* bridge,
    hypo_cog_emotion_callback_t callback,
    void* user_data
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_set_emotion_callback: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->emotion_callback = callback;
    bridge->emotion_callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int hypo_cognitive_hub_set_attention_callback(
    hypo_cognitive_hub_bridge_t* bridge,
    hypo_cog_attention_callback_t callback,
    void* user_data
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_set_attention_callback: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_callback = callback;
    bridge->attention_callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int hypo_cognitive_hub_set_decision_callback(
    hypo_cognitive_hub_bridge_t* bridge,
    hypo_cog_decision_callback_t callback,
    void* user_data
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_set_decision_callback: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->decision_callback = callback;
    bridge->decision_callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int hypo_cognitive_hub_set_learning_callback(
    hypo_cognitive_hub_bridge_t* bridge,
    hypo_cog_learning_callback_t callback,
    void* user_data
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_set_learning_callback: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->learning_callback = callback;
    bridge->learning_callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * BIO-ASYNC INTEGRATION IMPLEMENTATION
 * ============================================================================ */

int hypo_cognitive_hub_connect_bio_async(
    hypo_cognitive_hub_bridge_t* bridge,
    bio_router_t router
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_connect_bio_async: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->bio_async_connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_cognitive_hub_connect_bio_async: validation failed");
        return -1;  /* Already connected */
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Get router - use global if not provided */
    if (!router) {
        if (!bio_router_is_initialized()) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "hypo_cognitive_hub_connect_bio_async: bio_router_is_initialized is NULL");
            return -1;  /* No router available */
        }
        router = bio_router_get_global();
    }

    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router is NULL");


        return -1;
    }

    /* Register as bio-async module */
    bio_module_info_t info;
    info.module_id = HYPO_COG_BIO_ASYNC_MODULE_ID;
    info.module_name = "HypoCogHubBridge";
    info.inbox_capacity = 0;  /* Use default */
    info.user_data = bridge;

    bio_module_context_t ctx = bio_router_register_module(&info);
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_ctx = ctx;
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int hypo_cognitive_hub_disconnect_bio_async(hypo_cognitive_hub_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_disconnect_bio_async: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->bio_async_connected || !bridge->bio_ctx) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_disconnect_bio_async: required parameter is NULL (bridge->bio_async_connected, bridge->bio_ctx)");
        return -1;
    }

    bio_module_context_t ctx = bridge->bio_ctx;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Unregister from bio-async */
    bio_router_unregister_module(ctx);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_ctx = NULL;
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool hypo_cognitive_hub_bio_async_connected(
    const hypo_cognitive_hub_bridge_t* bridge
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_bio_async_connected: required parameter is NULL (bridge, bridge->initialized)");
        return false;
    }

    /* Cast away const for mutex lock */
    hypo_cognitive_hub_bridge_t* mutable_bridge = (hypo_cognitive_hub_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * STATE QUERY IMPLEMENTATION
 * ============================================================================ */

int hypo_cognitive_hub_get_cognitive_state(
    const hypo_cognitive_hub_bridge_t* bridge,
    hypo_cognitive_hub_state_t* state
) {
    if (!bridge || !bridge->initialized || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_get_cognitive_state: required parameter is NULL (bridge, bridge->initialized, state)");
        return -1;
    }

    /* Cast away const for mutex lock */
    hypo_cognitive_hub_bridge_t* mutable_bridge = (hypo_cognitive_hub_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);
    *state = bridge->cognitive_state;
    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return 0;
}

int hypo_cognitive_hub_get_config(
    const hypo_cognitive_hub_bridge_t* bridge,
    hypo_cognitive_hub_config_t* config
) {
    if (!bridge || !bridge->initialized || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_get_config: required parameter is NULL (bridge, bridge->initialized, config)");
        return -1;
    }

    /* Cast away const for mutex lock */
    hypo_cognitive_hub_bridge_t* mutable_bridge = (hypo_cognitive_hub_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);
    *config = bridge->config;
    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * STATISTICS IMPLEMENTATION
 * ============================================================================ */

int hypo_cognitive_hub_get_stats(
    const hypo_cognitive_hub_bridge_t* bridge,
    hypo_cognitive_hub_stats_t* stats
) {
    if (!bridge || !bridge->initialized || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_get_stats: required parameter is NULL (bridge, bridge->initialized, stats)");
        return -1;
    }

    /* Cast away const for mutex lock */
    hypo_cognitive_hub_bridge_t* mutable_bridge = (hypo_cognitive_hub_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return 0;
}

int hypo_cognitive_hub_reset_stats(hypo_cognitive_hub_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_cognitive_hub_reset_stats: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(hypo_cognitive_hub_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * UTILITY IMPLEMENTATION
 * ============================================================================ */

const char* hypo_cog_hub_event_name(hypo_cog_hub_event_type_t event_type) {
    static const char* names[] = {
        "DRIVE_STATE_CHANGED",
        "DRIVE_URGENT",
        "DRIVE_SATISFIED",
        "DRIVE_CONFLICT",
        "STRESS_CHANGED",
        "FATIGUE_CHANGED",
        "AROUSAL_CHANGED",
        "HOMEOSTATIC_ALERT"
    };

    if (event_type >= HYPO_COG_EVENT_COUNT) {
        return "UNKNOWN";
    }

    return names[event_type];
}

void hypo_cognitive_hub_print_summary(const hypo_cognitive_hub_bridge_t* bridge) {
    if (!bridge) {
        printf("Hypothalamus-Cognitive Hub Bridge: NULL\n");
        return;
    }

    /* Cast away const for mutex lock */
    hypo_cognitive_hub_bridge_t* mutable_bridge = (hypo_cognitive_hub_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);

    printf("=== Hypothalamus-Cognitive Hub Bridge ===\n");
    printf("  Initialized: %s\n", bridge->initialized ? "yes" : "no");
    printf("  Connected: %s\n", bridge->connected ? "yes" : "no");
    printf("  Bio-async connected: %s\n", bridge->bio_async_connected ? "yes" : "no");
    printf("  Module ID: 0x%08X\n", bridge->config.module_id);
    printf("\n");
    printf("Configuration:\n");
    printf("  Drive broadcast: %s\n", bridge->config.enable_drive_broadcast ? "enabled" : "disabled");
    printf("  Cognitive feedback: %s\n", bridge->config.enable_cognitive_feedback ? "enabled" : "disabled");
    printf("  Stress propagation: %s\n", bridge->config.enable_stress_propagation ? "enabled" : "disabled");
    printf("  Fatigue modulation: %s\n", bridge->config.enable_fatigue_modulation ? "enabled" : "disabled");
    printf("  Update interval: %.1f ms\n", bridge->config.drive_update_interval_ms);
    printf("  Cognitive weight: %.2f\n", bridge->config.cognitive_weight);
    printf("\n");
    printf("Cognitive State:\n");
    printf("  Cognitive load: %.2f\n", bridge->cognitive_state.cognitive_load);
    printf("  Attention demand: %.2f\n", bridge->cognitive_state.attention_demand);
    printf("  Emotional arousal: %.2f\n", bridge->cognitive_state.emotional_arousal);
    printf("  Executive control: %.2f\n", bridge->cognitive_state.executive_control);
    printf("  Memory consolidation: %.2f\n", bridge->cognitive_state.memory_consolidation);
    printf("  Learning activity: %.2f\n", bridge->cognitive_state.learning_activity);
    printf("==========================================\n");

    nimcp_mutex_unlock(mutable_bridge->base.mutex);
}

void hypo_cognitive_hub_print_stats(const hypo_cognitive_hub_stats_t* stats) {
    if (!stats) {
        printf("Statistics: NULL\n");
        return;
    }

    printf("=== Hypothalamus-Cognitive Hub Bridge Statistics ===\n");
    printf("Events:\n");
    printf("  Received: %u\n", stats->events_received);
    printf("  Published: %u\n", stats->events_published);
    printf("  Drive broadcasts: %u\n", stats->drive_broadcasts);
    printf("  Stress propagations: %u\n", stats->stress_propagations);
    printf("  Fatigue modulations: %u\n", stats->fatigue_modulations);
    printf("\n");
    printf("Feedback received:\n");
    printf("  Emotion updates: %u\n", stats->emotion_updates_received);
    printf("  Attention updates: %u\n", stats->attention_updates_received);
    printf("  Decision updates: %u\n", stats->decision_updates_received);
    printf("  Learning updates: %u\n", stats->learning_updates_received);
    printf("\n");
    printf("Drive modulations:\n");
    printf("  Curiosity: %u\n", stats->curiosity_modulations);
    printf("  Social: %u\n", stats->social_modulations);
    printf("  Competence: %u\n", stats->competence_modulations);
    printf("\n");
    printf("Queries handled: %u\n", stats->queries_handled);
    printf("====================================================\n");
}
