/**
 * @file nimcp_hypothalamus_brainstem_bridge.c
 * @brief Implementation of Hypothalamus <-> Brainstem Bridge
 *
 * WHAT: Bidirectional bridge between hypothalamus drives and brainstem arousal
 * WHY:  Steering subsystem (hypothalamus) must modulate brainstem arousal
 * HOW:  Maps drive urgency to arousal, pain/pleasure to reward, stress to protection
 *
 * @version Phase 11: Brainstem/Medulla Integration
 * @date 2026-01-04
 */

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_brainstem_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/time/nimcp_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_brainstem_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HYPOTHALAMUS_BRAINSTEM_BRIDGE"


/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define HYPO_BRAINSTEM_BRIDGE_MODULE_ID  0x1180

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Compute arousal target from drive urgencies
 */
static float compute_arousal_target(hypo_brainstem_bridge_t* bridge) {
    if (!bridge || !bridge->drives) {
        return 0.5f;  /* Default neutral arousal */
    }

    float arousal = 0.5f;  /* Baseline */

    /* Get drive urgencies */
    float urgencies[HYPO_DRIVE_COUNT];
    if (!hypo_drive_get_urgencies(bridge->drives, urgencies)) {
        return arousal;
    }

    /* Survival drives boost arousal (fight/flight) */
    float survival_contribution = 0.0f;
    survival_contribution += urgencies[HYPO_DRIVE_SAFETY] * 2.0f;  /* Safety is strongest */
    survival_contribution += urgencies[HYPO_DRIVE_HUNGER] * 0.5f;
    survival_contribution += urgencies[HYPO_DRIVE_THIRST] * 0.5f;
    survival_contribution += urgencies[HYPO_DRIVE_TEMPERATURE] * 0.3f;

    /* Fatigue reduces arousal */
    float fatigue_reduction = urgencies[HYPO_DRIVE_FATIGUE] * 0.4f;

    /* Curiosity slightly boosts arousal (exploration) */
    float curiosity_boost = urgencies[HYPO_DRIVE_CURIOSITY] * 0.2f;

    /* Compute final arousal target */
    arousal += survival_contribution * bridge->config.survival_arousal_scale;
    arousal -= fatigue_reduction;
    arousal += curiosity_boost;

    /* Add pain contribution (pain increases arousal) */
    arousal += bridge->total_pain * 0.3f;

    /* Clamp to [0, 1] */
    if (arousal < 0.0f) arousal = 0.0f;
    if (arousal > 1.0f) arousal = 1.0f;

    return arousal;
}

/**
 * @brief Check if emergency arousal is needed
 */
static bool check_emergency(hypo_brainstem_bridge_t* bridge) {
    if (!bridge || !bridge->drives) {
        return false;
    }

    float urgencies[HYPO_DRIVE_COUNT];
    if (!hypo_drive_get_urgencies(bridge->drives, urgencies)) {
        return false;
    }

    /* Safety drive above emergency threshold triggers emergency */
    if (urgencies[HYPO_DRIVE_SAFETY] > bridge->config.emergency_threshold) {
        return true;
    }

    /* High pain triggers emergency */
    if (bridge->total_pain > bridge->config.emergency_threshold) {
        return true;
    }

    return false;
}

/**
 * @brief Decay old pain/pleasure signals
 */
static void decay_signals(hypo_brainstem_bridge_t* bridge, uint64_t current_us) {
    /* Decay pain signals */
    float total_pain = 0.0f;
    for (uint32_t i = 0; i < bridge->pain_count; ) {
        float elapsed_ms = (float)(current_us - bridge->active_pain[i].timestamp_us) / 1000.0f;
        float dur = bridge->active_pain[i].duration_ms;
        if (dur <= 0.0f || elapsed_ms > dur) {
            /* Remove expired or zero-duration signal */
            bridge->active_pain[i] = bridge->active_pain[bridge->pain_count - 1];
            bridge->pain_count--;
        } else {
            /* Compute decay */
            float decay = 1.0f - (elapsed_ms / dur);
            total_pain += bridge->active_pain[i].intensity * decay;
            i++;
        }
    }
    bridge->total_pain = total_pain;

    /* Decay pleasure signals */
    float total_pleasure = 0.0f;
    for (uint32_t i = 0; i < bridge->pleasure_count; ) {
        float elapsed_ms = (float)(current_us - bridge->active_pleasure[i].timestamp_us) / 1000.0f;
        float dur = bridge->active_pleasure[i].duration_ms;
        if (dur <= 0.0f || elapsed_ms > dur) {
            /* Remove expired or zero-duration signal */
            bridge->active_pleasure[i] = bridge->active_pleasure[bridge->pleasure_count - 1];
            bridge->pleasure_count--;
        } else {
            /* Compute decay */
            float decay = 1.0f - (elapsed_ms / dur);
            total_pleasure += bridge->active_pleasure[i].intensity * decay;
            i++;
        }
    }
    bridge->total_pleasure = total_pleasure;
}

/*=============================================================================
 * BIO-ASYNC MESSAGE HANDLERS
 *===========================================================================*/

/**
 * @brief Handle pain signal message
 */
static nimcp_error_t bs_handle_pain(const void* msg, size_t msg_size,
                                     nimcp_bio_promise_t promise, void* ctx) {
    hypo_brainstem_bridge_t* bridge = (hypo_brainstem_bridge_t*)ctx;
    if (!bridge || !msg) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Extract pain signal from message payload */
    const struct {
        bio_message_header_t header;
        hypo_pain_signal_t signal;
    }* pain_msg = msg;

    hypo_brainstem_bridge_process_pain(bridge, &pain_msg->signal);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle pleasure signal message
 */
static nimcp_error_t bs_handle_pleasure(const void* msg, size_t msg_size,
                                         nimcp_bio_promise_t promise, void* ctx) {
    hypo_brainstem_bridge_t* bridge = (hypo_brainstem_bridge_t*)ctx;
    if (!bridge || !msg) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Extract pleasure signal from message payload */
    const struct {
        bio_message_header_t header;
        hypo_pleasure_signal_t signal;
    }* pleasure_msg = msg;

    hypo_brainstem_bridge_process_pleasure(bridge, &pleasure_msg->signal);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle arousal state message
 */
static nimcp_error_t bs_handle_arousal_state(const void* msg, size_t msg_size,
                                              nimcp_bio_promise_t promise, void* ctx) {
    hypo_brainstem_bridge_t* bridge = (hypo_brainstem_bridge_t*)ctx;
    if (!bridge || !msg) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Extract arousal state from message payload */
    const struct {
        bio_message_header_t header;
        hypo_arousal_state_t state;
    }* arousal_msg = msg;

    hypo_brainstem_bridge_process_arousal_state(bridge, &arousal_msg->state);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

hypo_brainstem_bridge_config_t hypo_brainstem_bridge_default_config(void) {
    hypo_brainstem_bridge_config_t config = {0};

    /* Arousal modulation */
    config.survival_arousal_scale = HYPO_BS_SURVIVAL_AROUSAL_SCALE;
    config.stress_arousal_scale = 0.2f;
    config.circadian_arousal_bias = 0.1f;

    /* Pain/pleasure processing */
    config.pain_reward_factor = HYPO_BS_PAIN_REWARD_FACTOR;
    config.pleasure_reward_factor = HYPO_BS_PLEASURE_REWARD_FACTOR;
    config.pain_drive_boost = 0.3f;  /* Pain boosts SAFETY drive */

    /* Thresholds */
    config.emergency_threshold = 0.85f;
    config.protection_threshold = 0.7f;

    /* Integration */
    config.enable_pain_reward = true;
    config.enable_pleasure_reward = true;
    config.enable_arousal_feedback = true;
    config.broadcast_enabled = true;

    return config;
}

hypo_brainstem_bridge_t* hypo_brainstem_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_brainstem_bridge_config_t* config) {

    if (!drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_brainstem_bridge_create: drives is NULL");
        return NULL;
    }

    hypo_brainstem_bridge_t* bridge = (hypo_brainstem_bridge_t*)nimcp_calloc(1, sizeof(hypo_brainstem_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_brainstem_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = hypo_brainstem_bridge_default_config();
    }

    /* Store drive system reference */
    bridge->drives = drives;

    /* Initialize state */
    bridge->current_arousal = 0.5f;
    bridge->current_protection = 0;  /* NORMAL */
    bridge->pain_count = 0;
    bridge->pleasure_count = 0;
    bridge->total_pain = 0.0f;
    bridge->total_pleasure = 0.0f;

    /* Initialize bridge base (creates mutex) */
    bridge_base_init(&bridge->base, 0, "hypothalamus_brainstem");

    return bridge;
}

void hypo_brainstem_bridge_destroy(hypo_brainstem_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hypothalamus_brainstem");

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

void hypo_brainstem_bridge_reset(hypo_brainstem_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset pain/pleasure */
    bridge->pain_count = 0;
    bridge->pleasure_count = 0;
    bridge->total_pain = 0.0f;
    bridge->total_pleasure = 0.0f;

    /* Reset state */
    bridge->current_arousal = 0.5f;
    bridge->current_protection = 0;

    /* Reset computed outputs */
    bridge->pain_reward_contribution = 0.0f;
    bridge->pleasure_reward_contribution = 0.0f;
    bridge->arousal_request = 0.5f;

    /* Reset statistics */
    bridge->pain_signals_received = 0;
    bridge->pleasure_signals_received = 0;
    bridge->arousal_requests_sent = 0;
    bridge->protection_requests_sent = 0;

    nimcp_mutex_unlock(bridge->base.mutex);
}

/*=============================================================================
 * CORE FUNCTIONS
 *===========================================================================*/

hypo_arousal_request_t hypo_brainstem_bridge_update(hypo_brainstem_bridge_t* bridge) {
    hypo_arousal_request_t request = {0};

    if (!bridge) {
        return request;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now = nimcp_time_get_us();

    /* Decay old pain/pleasure signals */
    decay_signals(bridge, now);

    /* Compute arousal target */
    float target = compute_arousal_target(bridge);
    bridge->arousal_request = target;

    /* Check for emergency */
    bool emergency = check_emergency(bridge);

    /* Fill request */
    request.target_arousal = target;
    request.urgency = emergency ? 1.0f : fabsf(target - bridge->current_arousal);
    request.source_drive = hypo_drive_get_priority(bridge->drives);
    request.is_emergency = emergency;
    request.timestamp_us = now;

    /* Compute reward contributions */
    if (bridge->config.enable_pain_reward) {
        bridge->pain_reward_contribution = bridge->total_pain * bridge->config.pain_reward_factor;
    }
    if (bridge->config.enable_pleasure_reward) {
        bridge->pleasure_reward_contribution = bridge->total_pleasure * bridge->config.pleasure_reward_factor;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return request;
}

float hypo_brainstem_bridge_process_pain(
    hypo_brainstem_bridge_t* bridge,
    const hypo_pain_signal_t* signal) {

    if (!bridge || !signal) {
        return 0.0f;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float reward_contribution = 0.0f;

    /* Store signal if we have room */
    if (bridge->pain_count < HYPO_BS_MAX_PAIN_SOURCES) {
        bridge->active_pain[bridge->pain_count++] = *signal;
    }

    /* Update aggregate */
    bridge->total_pain += signal->intensity;
    if (bridge->total_pain > 1.0f) {
        bridge->total_pain = 1.0f;
    }

    /* Compute reward contribution (negative) */
    if (bridge->config.enable_pain_reward) {
        reward_contribution = signal->intensity * bridge->config.pain_reward_factor;
        bridge->pain_reward_contribution += reward_contribution;
    }

    /* Boost SAFETY drive based on pain using nucleus input */
    if (bridge->config.pain_drive_boost > 0.0f && bridge->drives) {
        float boost = signal->intensity * bridge->config.pain_drive_boost;
        /* Use nucleus input to increase drive urgency */
        hypo_drive_set_nucleus_input(bridge->drives, HYPO_NUCLEUS_PARAVENTRICULAR, boost);
    }

    bridge->pain_signals_received++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return reward_contribution;
}

float hypo_brainstem_bridge_process_pleasure(
    hypo_brainstem_bridge_t* bridge,
    const hypo_pleasure_signal_t* signal) {

    if (!bridge || !signal) {
        return 0.0f;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float reward_contribution = 0.0f;

    /* Store signal if we have room */
    if (bridge->pleasure_count < HYPO_BS_MAX_PAIN_SOURCES) {
        bridge->active_pleasure[bridge->pleasure_count++] = *signal;
    }

    /* Update aggregate */
    bridge->total_pleasure += signal->intensity;
    if (bridge->total_pleasure > 1.0f) {
        bridge->total_pleasure = 1.0f;
    }

    /* Compute reward contribution (positive) */
    if (bridge->config.enable_pleasure_reward) {
        reward_contribution = signal->intensity * bridge->config.pleasure_reward_factor;
        bridge->pleasure_reward_contribution += reward_contribution;
    }

    bridge->pleasure_signals_received++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return reward_contribution;
}

void hypo_brainstem_bridge_process_arousal_state(
    hypo_brainstem_bridge_t* bridge,
    const hypo_arousal_state_t* state) {

    if (!bridge || !state) {
        return;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->current_arousal = state->current_arousal;
    bridge->current_protection = state->protection_level;

    /* Optionally modulate drives based on arousal feedback */
    if (bridge->config.enable_arousal_feedback && bridge->drives) {
        /* Low arousal increases preoptic (fatigue) activity */
        if (state->current_arousal < 0.3f) {
            hypo_drive_set_nucleus_input(bridge->drives, HYPO_NUCLEUS_PREOPTIC, 0.3f);
        }

        /* Emergency state boosts paraventricular (stress) activity */
        if (state->in_emergency) {
            hypo_drive_set_nucleus_input(bridge->drives, HYPO_NUCLEUS_PARAVENTRICULAR, 0.5f);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
}

float hypo_brainstem_bridge_get_reward_contribution(
    const hypo_brainstem_bridge_t* bridge) {

    if (!bridge) {
        return 0.0f;
    }

    return bridge->pain_reward_contribution + bridge->pleasure_reward_contribution;
}

bool hypo_brainstem_bridge_needs_emergency(
    const hypo_brainstem_bridge_t* bridge) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_brainstem_bridge_update: bridge is NULL");
        return false;
    }

    return check_emergency((hypo_brainstem_bridge_t*)bridge);
}

/*=============================================================================
 * BRAINSTEM CONNECTION
 *===========================================================================*/

bool hypo_brainstem_bridge_connect(
    hypo_brainstem_bridge_t* bridge,
    void* brainstem) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_brainstem_bridge_update: bridge is NULL");
        return false;
    }

    bridge->brainstem = brainstem;
    return true;
}

int hypo_brainstem_bridge_send_arousal_request(
    hypo_brainstem_bridge_t* bridge,
    const hypo_arousal_request_t* request) {

    if (!bridge || !request) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_brainstem_bridge_update: required parameter is NULL (bridge, request)");
        return -1;
    }

    /* Broadcast via bio-async */
    if (bridge->bio_ctx && bridge->config.broadcast_enabled) {
        struct {
            bio_message_header_t header;
            hypo_arousal_request_t request;
        } msg;

        msg.header.type = BIO_MSG_BRAINSTEM_AROUSAL_REQUEST;
        msg.header.timestamp_us = nimcp_time_get_us();
        msg.header.source_module = HYPO_BRAINSTEM_BRIDGE_MODULE_ID;
        msg.header.target_module = 0;  /* Broadcast */
        msg.header.flags = BIO_MSG_FLAG_BROADCAST;
        msg.header.payload_size = sizeof(hypo_arousal_request_t);
        msg.request = *request;

        bio_router_broadcast(bridge->bio_ctx, &msg.header, sizeof(msg));
        bridge->arousal_requests_sent++;
    }

    return 0;
}

int hypo_brainstem_bridge_send_protection_request(
    hypo_brainstem_bridge_t* bridge,
    const hypo_protection_request_t* request) {

    if (!bridge || !request) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, request)");
        return -1;
    }

    /* Broadcast via bio-async */
    if (bridge->bio_ctx && bridge->config.broadcast_enabled) {
        struct {
            bio_message_header_t header;
            hypo_protection_request_t request;
        } msg;

        msg.header.type = BIO_MSG_BRAINSTEM_PROTECTION_REQUEST;
        msg.header.timestamp_us = nimcp_time_get_us();
        msg.header.source_module = HYPO_BRAINSTEM_BRIDGE_MODULE_ID;
        msg.header.target_module = 0;  /* Broadcast */
        msg.header.flags = BIO_MSG_FLAG_BROADCAST;
        msg.header.payload_size = sizeof(hypo_protection_request_t);
        msg.request = *request;

        bio_router_broadcast(bridge->bio_ctx, &msg.header, sizeof(msg));
        bridge->protection_requests_sent++;
    }

    return 0;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

bool hypo_brainstem_bridge_register_bio(
    hypo_brainstem_bridge_t* bridge,
    bool use_kg_wiring) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge is NULL");
        return false;
    }

    /* Register module */
    bio_module_info_t info = {
        .module_id = HYPO_BRAINSTEM_BRIDGE_MODULE_ID,
        .module_name = "hypothalamus_brainstem_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->bio_ctx = bio_router_register_module(&info);
    if (!bridge->bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge->bio_ctx is NULL");
        return false;
    }

    /* Register handlers for incoming messages */
    if (bio_router_register_handler(bridge->bio_ctx, BIO_MSG_BRAINSTEM_PAIN,
                                     bs_handle_pain) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge->bio_ctx is NULL");
        return false;
    }

    if (bio_router_register_handler(bridge->bio_ctx, BIO_MSG_BRAINSTEM_PLEASURE,
                                     bs_handle_pleasure) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: operation failed");
        return false;
    }

    if (bio_router_register_handler(bridge->bio_ctx, BIO_MSG_BRAINSTEM_AROUSAL_STATE,
                                     bs_handle_arousal_state) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: operation failed");
        return false;
    }

    return true;
}

uint32_t hypo_brainstem_bridge_process_bio(
    hypo_brainstem_bridge_t* bridge,
    uint32_t max_messages) {

    if (!bridge || !bridge->bio_ctx) {
        return 0;
    }

    return bio_router_process_inbox(bridge->bio_ctx, max_messages);
}

nimcp_error_t hypo_brainstem_bridge_broadcast_arousal(
    hypo_brainstem_bridge_t* bridge) {

    if (!bridge) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    hypo_arousal_request_t request = hypo_brainstem_bridge_update(bridge);
    return hypo_brainstem_bridge_send_arousal_request(bridge, &request) == 0 ?
           NIMCP_SUCCESS : NIMCP_ERROR_OPERATION_FAILED;
}

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

void hypo_brainstem_bridge_get_stats(
    const hypo_brainstem_bridge_t* bridge,
    uint64_t* pain_signals,
    uint64_t* pleasure_signals,
    uint64_t* arousal_requests) {

    if (!bridge) {
        return;
    }

    if (pain_signals) *pain_signals = bridge->pain_signals_received;
    if (pleasure_signals) *pleasure_signals = bridge->pleasure_signals_received;
    if (arousal_requests) *arousal_requests = bridge->arousal_requests_sent;
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

const char* hypo_pain_type_string(hypo_pain_type_t type) {
    switch (type) {
        case HYPO_PAIN_NOCICEPTIVE:     return "NOCICEPTIVE";
        case HYPO_PAIN_THERMAL:         return "THERMAL";
        case HYPO_PAIN_CHEMICAL:        return "CHEMICAL";
        case HYPO_PAIN_VISCERAL:        return "VISCERAL";
        case HYPO_PAIN_SOCIAL:          return "SOCIAL";
        case HYPO_PAIN_PREDICTION_ERROR: return "PREDICTION_ERROR";
        default:                        return "UNKNOWN";
    }
}

const char* hypo_pleasure_type_string(hypo_pleasure_type_t type) {
    switch (type) {
        case HYPO_PLEASURE_CONSUMATORY:     return "CONSUMATORY";
        case HYPO_PLEASURE_SOCIAL:          return "SOCIAL";
        case HYPO_PLEASURE_THERMAL_COMFORT: return "THERMAL_COMFORT";
        case HYPO_PLEASURE_SAFETY:          return "SAFETY";
        case HYPO_PLEASURE_MASTERY:         return "MASTERY";
        case HYPO_PLEASURE_CURIOSITY:       return "CURIOSITY";
        default:                            return "UNKNOWN";
    }
}
