/**
 * @file nimcp_hypothalamus_drives_bio.c
 * @brief Bio-Async Integration Implementation for Hypothalamus Drive System
 *
 * WHAT: Bio-async message handlers for Byrnes alignment-safe drive system
 * WHY:  Enable inter-module communication for steering signals
 * HOW:  KG-driven wiring with explicit handler registration
 *
 * @version Phase 3: Bio-Async Integration
 * @date 2026-01-04
 */

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives_bio.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_drives_bio, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define DRIVE_BIO_LOG_MODULE "HYPO_DRIVE_BIO"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define DEFAULT_BROADCAST_INTERVAL_US (100000ULL)  /* 100ms = 10 Hz */

/*=============================================================================
 * FORWARD DECLARATIONS - MESSAGE HANDLERS
 *===========================================================================*/

static nimcp_error_t handle_drive_state_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t handle_reward_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t handle_priority_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t handle_satisfy_drive(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

/*=============================================================================
 * HANDLER MAP (KG-Driven Wiring)
 *===========================================================================*/

DEFINE_HANDLER_MAP_BEGIN(hypo_drives)
    HANDLER_MAP_ENTRY(BIO_MSG_HYPO_DRIVE_STATE, handle_drive_state_query)
    HANDLER_MAP_ENTRY(BIO_MSG_HYPO_REWARD_SIGNAL, handle_reward_query)
    HANDLER_MAP_ENTRY(BIO_MSG_HYPO_SURVIVAL_PRIORITY, handle_priority_query)
    HANDLER_MAP_ENTRY(BIO_MSG_HYPO_DRIVE_SATISFIED, handle_satisfy_drive)
DEFINE_HANDLER_MAP_END()

/*=============================================================================
 * KG-DRIVEN WIRING CALLBACK
 *===========================================================================*/

static int hypo_drives_wiring_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;
    }

    hypo_drives_bio_ctx_t* bio_ctx = (hypo_drives_bio_ctx_t*)user_data;
    if (!bio_ctx) {
        LOG_WARNING(DRIVE_BIO_LOG_MODULE, "Wiring callback: bio_ctx is NULL");
        return 0;
    }

    LOG_DEBUG(DRIVE_BIO_LOG_MODULE,
              "KG wiring callback: registering %u message handlers", message_count);

    /* Register handlers based on KG-specified message types */
    for (uint32_t i = 0; i < message_count; i++) {
        /* Find matching handler in our map */
        for (size_t j = 0; j < HANDLER_MAP_SIZE(hypo_drives); j++) {
            if (g_hypo_drives_handler_map[j].message_type == message_types[i]) {
                bio_router_register_handler(ctx, message_types[i],
                    g_hypo_drives_handler_map[j].handler);
                LOG_DEBUG(DRIVE_BIO_LOG_MODULE,
                          "Registered handler for message type 0x%04X",
                          message_types[i]);
                break;
            }
        }
    }

    return 0;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

hypo_drives_bio_ctx_t* hypo_drives_bio_init(
    hypo_drive_system_handle_t* drives,
    bool use_kg_wiring) {

    if (!drives) {
        LOG_ERROR(DRIVE_BIO_LOG_MODULE, "Cannot init bio-async without drive system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "drives is NULL");


        return NULL;
    }

    LOG_INFO(DRIVE_BIO_LOG_MODULE, "Initializing bio-async integration for drives");

    /* Allocate context */
    hypo_drives_bio_ctx_t* ctx = (hypo_drives_bio_ctx_t*)nimcp_calloc(
        1, sizeof(hypo_drives_bio_ctx_t));
    if (!ctx) {
        LOG_ERROR(DRIVE_BIO_LOG_MODULE, "Failed to allocate bio context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_drives_bio_init: allocation failed");

        return NULL;
    }

    ctx->drives = drives;
    ctx->broadcast_enabled = true;
    ctx->broadcast_interval_us = DEFAULT_BROADCAST_INTERVAL_US;

    /* Check if bio-async router is available */
    if (!bio_router_is_initialized()) {
        LOG_WARNING(DRIVE_BIO_LOG_MODULE,
                    "Bio-async router not initialized, running in standalone mode");
        return ctx;  /* Return context but without bio registration */
    }

    /* Register with bio-async router */
    bio_module_info_t bio_info = {
        .module_id = BIO_MODULE_HYPOTHALAMUS,  /* Share with adapter */
        .module_name = "hypothalamus_drives",
        .inbox_capacity = 64,
        .user_data = ctx
    };

    ctx->bio_ctx = bio_router_register_module(&bio_info);
    if (!ctx->bio_ctx) {
        LOG_WARNING(DRIVE_BIO_LOG_MODULE,
                    "Failed to register with bio router (may already be registered)");
        /* Continue - adapter may have registered already */
    }

    /* Register handlers */
    if (ctx->bio_ctx) {
        if (use_kg_wiring) {
            /* Try KG-driven wiring */
            nimcp_error_t result = bio_router_register_wiring_callback(
                BIO_MODULE_HYPOTHALAMUS,
                (void*)hypo_drives_wiring_callback,
                ctx
            );

            if (result != NIMCP_SUCCESS) {
                LOG_DEBUG(DRIVE_BIO_LOG_MODULE,
                          "KG wiring not available, using legacy registration");
                use_kg_wiring = false;
            }
        }

        if (!use_kg_wiring) {
            /* Legacy direct registration */
            for (size_t i = 0; i < HANDLER_MAP_SIZE(hypo_drives); i++) {
                bio_router_register_handler(ctx->bio_ctx,
                    g_hypo_drives_handler_map[i].message_type,
                    g_hypo_drives_handler_map[i].handler);
            }
            LOG_DEBUG(DRIVE_BIO_LOG_MODULE,
                      "Registered %zu handlers via legacy method",
                      HANDLER_MAP_SIZE(hypo_drives));
        }
    }

    LOG_INFO(DRIVE_BIO_LOG_MODULE, "Bio-async integration initialized");
    return ctx;
}

void hypo_drives_bio_shutdown(hypo_drives_bio_ctx_t* ctx) {
    if (!ctx) return;

    LOG_INFO(DRIVE_BIO_LOG_MODULE, "Shutting down bio-async integration");

    /* Log final stats */
    LOG_INFO(DRIVE_BIO_LOG_MODULE,
             "Final stats: sent=%lu, received=%lu, alerts=%lu",
             (unsigned long)ctx->messages_sent,
             (unsigned long)ctx->messages_received,
             (unsigned long)ctx->alignment_alerts_sent);

    /* Unregister from bio router */
    if (ctx->bio_ctx) {
        bio_router_unregister_module(ctx->bio_ctx);
        ctx->bio_ctx = NULL;
    }

    nimcp_free(ctx);
}

uint32_t hypo_drives_bio_process(hypo_drives_bio_ctx_t* ctx,
                                  uint32_t max_messages) {
    if (!ctx || !ctx->bio_ctx) return 0;

    uint32_t processed = bio_router_process_inbox(ctx->bio_ctx, max_messages);
    ctx->messages_received += processed;

    return processed;
}

/*=============================================================================
 * MESSAGE HANDLERS
 *===========================================================================*/

static nimcp_error_t handle_drive_state_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    hypo_drives_bio_ctx_t* ctx = (hypo_drives_bio_ctx_t*)user_data;
    if (!ctx || !ctx->drives) {
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    LOG_DEBUG(DRIVE_BIO_LOG_MODULE, "Handling drive state query");

    /* Build response with all drive states */
    hypo_all_drives_msg_t response;
    memset(&response, 0, sizeof(response));

    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        hypo_drive_state_t state;
        if (hypo_drive_get_state(ctx->drives, (hypo_drive_type_t)i, &state)) {
            response.drives[i].drive_type = state.type;
            response.drives[i].level = state.level;
            response.drives[i].urgency = state.urgency;
            response.drives[i].setpoint = state.setpoint;
            response.drives[i].deviation = state.deviation;
            response.drives[i].active = state.active;
        }
    }

    response.highest_priority = hypo_drive_get_priority(ctx->drives);

    /* Get arousal from drive system */
    hypo_drive_system_t sys_state;
    if (hypo_drive_get_system_state(ctx->drives, &sys_state)) {
        response.arousal_level = sys_state.arousal_level;
    }

    /* Complete promise */
    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &response);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_reward_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    hypo_drives_bio_ctx_t* ctx = (hypo_drives_bio_ctx_t*)user_data;
    if (!ctx || !ctx->drives) {
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    LOG_DEBUG(DRIVE_BIO_LOG_MODULE, "Handling reward query");

    /* Compute current reward */
    hypo_reward_signal_t reward;
    hypo_drive_compute_reward(ctx->drives, &reward);

    /* Build response */
    hypo_reward_msg_t response = {
        .reward_signal = reward.reward_signal,
        .prediction_error = reward.prediction_error,
        .dopamine_level = reward.dopamine_level,
        .alignment_bonus = reward.alignment_bonus,
        .alignment_penalty = reward.alignment_penalty
    };

    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &response);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_priority_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    hypo_drives_bio_ctx_t* ctx = (hypo_drives_bio_ctx_t*)user_data;
    if (!ctx || !ctx->drives) {
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    LOG_DEBUG(DRIVE_BIO_LOG_MODULE, "Handling priority query");

    /* Get priority and urgencies */
    hypo_priority_msg_t response;
    memset(&response, 0, sizeof(response));

    response.priority_drive = hypo_drive_get_priority(ctx->drives);

    hypo_drive_state_t state;
    if (hypo_drive_get_state(ctx->drives, response.priority_drive, &state)) {
        response.urgency = state.urgency;
    }

    /* Get all urgencies for salience bias */
    hypo_drive_get_urgencies(ctx->drives, response.salience_bias);

    /* Interrupt if urgency is very high */
    response.interrupt_required = response.urgency > 0.8f;

    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &response);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_satisfy_drive(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    hypo_drives_bio_ctx_t* ctx = (hypo_drives_bio_ctx_t*)user_data;
    if (!ctx || !ctx->drives || !msg) {
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    /* Parse satisfaction request */
    const hypo_satisfaction_msg_t* req = (const hypo_satisfaction_msg_t*)msg;

    LOG_DEBUG(DRIVE_BIO_LOG_MODULE,
              "Handling drive satisfaction: drive=%s, level=%.2f",
              hypo_drive_type_string(req->drive_type),
              req->satisfaction_level);

    /* Satisfy the drive */
    float reward = hypo_drive_satisfy(ctx->drives,
                                       req->drive_type,
                                       req->satisfaction_level);

    /* Build response */
    hypo_satisfaction_msg_t response = {
        .drive_type = req->drive_type,
        .satisfaction_level = req->satisfaction_level,
        .resulting_reward = reward
    };

    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &response);
    }

    /* Broadcast satisfaction event */
    hypo_drives_bio_broadcast_satisfaction(ctx,
                                            req->drive_type,
                                            req->satisfaction_level,
                                            reward);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * BROADCAST FUNCTIONS
 *===========================================================================*/

nimcp_error_t hypo_drives_bio_broadcast_state(
    hypo_drives_bio_ctx_t* ctx,
    int drive_type) {

    if (!ctx) return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    if (!ctx->bio_ctx) return NIMCP_SUCCESS;  /* Silent success if no router */

    /* Build message */
    bio_message_header_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = BIO_MSG_HYPO_DRIVE_STATE;
    msg.source_module = BIO_MODULE_HYPOTHALAMUS;
    msg.target_module = 0;  /* Broadcast */
    msg.channel = BIO_CHANNEL_DOPAMINE;  /* Drive state uses dopamine channel */
    msg.flags = BIO_MSG_FLAG_BROADCAST;

    if (drive_type >= 0 && drive_type < HYPO_DRIVE_COUNT) {
        /* Single drive broadcast */
        hypo_drive_state_msg_t payload;
        hypo_drive_state_t state;

        if (hypo_drive_get_state(ctx->drives, (hypo_drive_type_t)drive_type, &state)) {
            payload.drive_type = state.type;
            payload.level = state.level;
            payload.urgency = state.urgency;
            payload.setpoint = state.setpoint;
            payload.deviation = state.deviation;
            payload.active = state.active;

            msg.payload_size = sizeof(payload);
        }
    } else {
        /* All drives broadcast */
        msg.payload_size = sizeof(hypo_all_drives_msg_t);
    }

    nimcp_error_t result = bio_router_broadcast(ctx->bio_ctx, &msg, sizeof(msg));
    if (result == NIMCP_SUCCESS) {
        ctx->messages_sent++;
    }

    return result;
}

nimcp_error_t hypo_drives_bio_broadcast_reward(
    hypo_drives_bio_ctx_t* ctx,
    const hypo_reward_signal_t* reward) {

    if (!ctx || !reward) return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    if (!ctx->bio_ctx) return NIMCP_SUCCESS;

    bio_message_header_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = BIO_MSG_HYPO_REWARD_SIGNAL;
    msg.source_module = BIO_MODULE_HYPOTHALAMUS;
    msg.target_module = 0;  /* Broadcast (SNc/VTA will pick up) */
    msg.channel = BIO_CHANNEL_DOPAMINE;  /* Reward uses dopamine channel */
    msg.flags = BIO_MSG_FLAG_BROADCAST;
    msg.payload_size = sizeof(hypo_reward_msg_t);

    LOG_DEBUG(DRIVE_BIO_LOG_MODULE,
              "Broadcasting reward: signal=%.3f, RPE=%.3f, DA=%.3f",
              reward->reward_signal, reward->prediction_error, reward->dopamine_level);

    nimcp_error_t result = bio_router_broadcast(ctx->bio_ctx, &msg, sizeof(msg));
    if (result == NIMCP_SUCCESS) {
        ctx->messages_sent++;
    }

    return result;
}

nimcp_error_t hypo_drives_bio_broadcast_arousal(
    hypo_drives_bio_ctx_t* ctx,
    float arousal_level,
    float arousal_delta,
    hypo_drive_type_t driver) {

    if (!ctx) return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    if (!ctx->bio_ctx) return NIMCP_SUCCESS;

    bio_message_header_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = BIO_MSG_HYPO_AROUSAL_CHANGE;
    msg.source_module = BIO_MODULE_HYPOTHALAMUS;
    msg.target_module = 0;  /* Broadcast (Thalamus will pick up) */
    msg.channel = BIO_CHANNEL_NOREPINEPHRINE;  /* Arousal uses NE */
    msg.flags = BIO_MSG_FLAG_BROADCAST;
    msg.payload_size = sizeof(hypo_arousal_msg_t);

    LOG_DEBUG(DRIVE_BIO_LOG_MODULE,
              "Broadcasting arousal: level=%.2f, delta=%.2f, driver=%s",
              arousal_level, arousal_delta, hypo_drive_type_string(driver));

    nimcp_error_t result = bio_router_broadcast(ctx->bio_ctx, &msg, sizeof(msg));
    if (result == NIMCP_SUCCESS) {
        ctx->messages_sent++;
    }

    return result;
}

nimcp_error_t hypo_drives_bio_broadcast_priority(
    hypo_drives_bio_ctx_t* ctx,
    hypo_drive_type_t priority_drive,
    float urgency,
    bool interrupt) {

    if (!ctx) return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    if (!ctx->bio_ctx) return NIMCP_SUCCESS;

    bio_message_header_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = BIO_MSG_HYPO_SURVIVAL_PRIORITY;
    msg.source_module = BIO_MODULE_HYPOTHALAMUS;
    msg.target_module = 0;  /* Broadcast (Attention Gate will pick up) */
    msg.channel = interrupt ? BIO_CHANNEL_NOREPINEPHRINE : BIO_CHANNEL_ACETYLCHOLINE;
    msg.flags = BIO_MSG_FLAG_BROADCAST;
    msg.payload_size = sizeof(hypo_priority_msg_t);

    if (interrupt) {
        msg.flags |= BIO_MSG_FLAG_URGENT;
    }

    LOG_DEBUG(DRIVE_BIO_LOG_MODULE,
              "Broadcasting priority: drive=%s, urgency=%.2f, interrupt=%s",
              hypo_drive_type_string(priority_drive), urgency,
              interrupt ? "YES" : "NO");

    nimcp_error_t result = bio_router_broadcast(ctx->bio_ctx, &msg, sizeof(msg));
    if (result == NIMCP_SUCCESS) {
        ctx->messages_sent++;
    }

    return result;
}

nimcp_error_t hypo_drives_bio_send_alignment_alert(
    hypo_drives_bio_ctx_t* ctx,
    hypo_alert_type_t alert_type,
    uint32_t modifier_id,
    const char* target,
    bool access_granted) {

    if (!ctx) return NIMCP_BIO_ERROR_NOT_INITIALIZED;

    /* ALWAYS log alignment alerts regardless of bio-async availability */
    LOG_WARNING(DRIVE_BIO_LOG_MODULE,
                "ALIGNMENT ALERT: type=%d, modifier=%u, target=%s, granted=%s",
                alert_type, modifier_id, target ? target : "NULL",
                access_granted ? "YES" : "NO");

    ctx->alignment_alerts_sent++;

    if (!ctx->bio_ctx) return NIMCP_SUCCESS;

    bio_message_header_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = BIO_MSG_HYPO_ALIGNMENT_ALERT;
    msg.source_module = BIO_MODULE_HYPOTHALAMUS;
    msg.target_module = 0;  /* Broadcast to all safety monitors */
    msg.channel = BIO_CHANNEL_NOREPINEPHRINE;  /* Alerts use NE */
    msg.flags = BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT;
    msg.payload_size = sizeof(hypo_alignment_alert_msg_t);

    nimcp_error_t result = bio_router_broadcast(ctx->bio_ctx, &msg, sizeof(msg));
    if (result == NIMCP_SUCCESS) {
        ctx->messages_sent++;
    }

    return result;
}

nimcp_error_t hypo_drives_bio_broadcast_satisfaction(
    hypo_drives_bio_ctx_t* ctx,
    hypo_drive_type_t drive_type,
    float satisfaction_level,
    float reward) {

    if (!ctx) return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    if (!ctx->bio_ctx) return NIMCP_SUCCESS;

    bio_message_header_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = BIO_MSG_HYPO_DRIVE_SATISFIED;
    msg.source_module = BIO_MODULE_HYPOTHALAMUS;
    msg.target_module = 0;
    msg.channel = BIO_CHANNEL_DOPAMINE;  /* Satisfaction is reward */
    msg.flags = BIO_MSG_FLAG_BROADCAST;
    msg.payload_size = sizeof(hypo_satisfaction_msg_t);

    LOG_DEBUG(DRIVE_BIO_LOG_MODULE,
              "Broadcasting satisfaction: drive=%s, level=%.2f, reward=%.3f",
              hypo_drive_type_string(drive_type), satisfaction_level, reward);

    nimcp_error_t result = bio_router_broadcast(ctx->bio_ctx, &msg, sizeof(msg));
    if (result == NIMCP_SUCCESS) {
        ctx->messages_sent++;
    }

    return result;
}

nimcp_error_t hypo_drives_bio_broadcast_conflict(
    hypo_drives_bio_ctx_t* ctx,
    hypo_drive_type_t drive_a,
    hypo_drive_type_t drive_b,
    hypo_drive_type_t winner) {

    if (!ctx) return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    if (!ctx->bio_ctx) return NIMCP_SUCCESS;

    bio_message_header_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = BIO_MSG_HYPO_DRIVE_CONFLICT;
    msg.source_module = BIO_MODULE_HYPOTHALAMUS;
    msg.target_module = 0;
    msg.channel = BIO_CHANNEL_SEROTONIN;  /* Conflict resolution uses 5-HT */
    msg.flags = BIO_MSG_FLAG_BROADCAST;
    msg.payload_size = sizeof(hypo_conflict_msg_t);

    LOG_DEBUG(DRIVE_BIO_LOG_MODULE,
              "Broadcasting conflict: %s vs %s, winner=%s",
              hypo_drive_type_string(drive_a),
              hypo_drive_type_string(drive_b),
              hypo_drive_type_string(winner));

    nimcp_error_t result = bio_router_broadcast(ctx->bio_ctx, &msg, sizeof(msg));
    if (result == NIMCP_SUCCESS) {
        ctx->messages_sent++;
    }

    return result;
}

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

void hypo_drives_bio_set_broadcast(hypo_drives_bio_ctx_t* ctx,
                                    bool enabled,
                                    uint64_t interval_us) {
    if (!ctx) return;

    ctx->broadcast_enabled = enabled;
    if (interval_us > 0) {
        ctx->broadcast_interval_us = interval_us;
    }

    LOG_DEBUG(DRIVE_BIO_LOG_MODULE,
              "Broadcast config: enabled=%s, interval=%lu us",
              enabled ? "true" : "false",
              (unsigned long)ctx->broadcast_interval_us);
}

void hypo_drives_bio_get_stats(const hypo_drives_bio_ctx_t* ctx,
                                uint64_t* sent,
                                uint64_t* received,
                                uint64_t* alerts) {
    if (!ctx) {
        if (sent) *sent = 0;
        if (received) *received = 0;
        if (alerts) *alerts = 0;
        return;
    }

    if (sent) *sent = ctx->messages_sent;
    if (received) *received = ctx->messages_received;
    if (alerts) *alerts = ctx->alignment_alerts_sent;
}
