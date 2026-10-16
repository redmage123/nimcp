/**
 * @file nimcp_hypothalamus_medulla_bridge.c
 * @brief Implementation of Hypothalamus <-> Medulla Bridge
 *
 * WHAT: Bridge between hypothalamus drives and medulla autonomic functions
 * WHY:  Steering subsystem must control autonomic outputs (arousal, protection)
 * HOW:  Maps drive states to medulla control signals, receives vital feedback
 *
 * @version Phase 11: Brainstem/Medulla Integration
 * @date 2026-01-04
 */

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_medulla_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/time/nimcp_time.h"
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
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_medulla_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HYPOTHALAMUS_MEDULLA_BRIDGE"


/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define HYPO_MEDULLA_BRIDGE_MODULE_ID  0x1190

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Compute arousal target from drive urgencies
 */
static float compute_arousal_from_drives(hypo_medulla_bridge_t* bridge) {
    if (!bridge || !bridge->drives) {
        return bridge->config.arousal_baseline;
    }

    float arousal = bridge->config.arousal_baseline;

    /* Get drive urgencies */
    float urgencies[HYPO_DRIVE_COUNT];
    if (!hypo_drive_get_urgencies(bridge->drives, urgencies)) {
        return arousal;
    }

    /* Survival drives increase arousal */
    arousal += urgencies[HYPO_DRIVE_SAFETY] * bridge->config.arousal_scale * 2.0f;
    arousal += urgencies[HYPO_DRIVE_HUNGER] * bridge->config.arousal_scale * 0.5f;
    arousal += urgencies[HYPO_DRIVE_THIRST] * bridge->config.arousal_scale * 0.5f;

    /* Curiosity slightly increases arousal */
    arousal += urgencies[HYPO_DRIVE_CURIOSITY] * bridge->config.arousal_scale * 0.3f;

    /* Fatigue decreases arousal */
    arousal -= urgencies[HYPO_DRIVE_FATIGUE] * bridge->config.arousal_scale * 0.6f;

    /* Clamp */
    if (arousal < 0.0f) arousal = 0.0f;
    if (arousal > bridge->config.arousal_max) arousal = bridge->config.arousal_max;

    return arousal;
}

/**
 * @brief Compute protection level from drive urgencies
 */
static protection_level_t compute_protection_from_drives(hypo_medulla_bridge_t* bridge) {
    if (!bridge || !bridge->drives) {
        return PROTECTION_LEVEL_NORMAL;
    }

    /* Get drive urgencies */
    float urgencies[HYPO_DRIVE_COUNT];
    if (!hypo_drive_get_urgencies(bridge->drives, urgencies)) {
        return PROTECTION_LEVEL_NORMAL;
    }

    float safety_urgency = urgencies[HYPO_DRIVE_SAFETY];

    /* Map safety urgency to protection level */
    if (safety_urgency > 0.95f && bridge->config.enable_emergency_escalation) {
        return PROTECTION_LEVEL_SHUTDOWN;
    } else if (safety_urgency > 0.85f) {
        return PROTECTION_LEVEL_CRITICAL;
    } else if (safety_urgency > 0.7f) {
        return PROTECTION_LEVEL_DEFENSIVE;
    } else if (safety_urgency > 0.5f) {
        return PROTECTION_LEVEL_GUARDED;
    } else if (safety_urgency > 0.3f) {
        return PROTECTION_LEVEL_CAUTIOUS;
    }

    return PROTECTION_LEVEL_NORMAL;
}

/**
 * @brief Compute circadian arousal baseline
 */
static float compute_circadian_arousal(float phase_hours) {
    /* Simple sinusoidal circadian rhythm */
    /* Peak arousal around 10:00 and 16:00, low around 03:00 */
    float radians = (phase_hours - 10.0f) * (NIMCP_TWO_PI_F / 24.0f);
    float circadian_factor = 0.5f + 0.3f * cosf(radians);  /* 0.2 to 0.8 */
    return circadian_factor;
}

/*=============================================================================
 * BIO-ASYNC MESSAGE HANDLERS
 *===========================================================================*/

/**
 * @brief Handle medulla state message
 */
static nimcp_error_t med_handle_state(const void* msg, size_t msg_size,
                                       nimcp_bio_promise_t promise, void* ctx) {
    hypo_medulla_bridge_t* bridge = (hypo_medulla_bridge_t*)ctx;
    if (!bridge || !msg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "med_handle_state: bridge or msg is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Extract status from message payload */
    const struct {
        bio_message_header_t header;
        hypo_medulla_status_t status;
    }* state_msg = msg;

    hypo_medulla_bridge_process_status(bridge, &state_msg->status);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

hypo_medulla_bridge_config_t hypo_medulla_bridge_default_config(void) {
    hypo_medulla_bridge_config_t config = {0};

    /* Arousal control */
    config.arousal_scale = HYPO_MED_AROUSAL_SCALE;
    config.arousal_baseline = 0.5f;
    config.arousal_max = 1.0f;
    config.arousal_transition_rate = 0.1f;  /* 10% per second */

    /* Protection control */
    config.protection_threshold = HYPO_MED_PROTECTION_THRESHOLD;
    config.enable_emergency_escalation = true;

    /* Circadian integration */
    config.enable_circadian_sync = true;
    config.circadian_sync_interval_ms = HYPO_MED_CIRCADIAN_SYNC_MS;

    /* Fatigue/sleep integration */
    config.enable_fatigue_sleep = true;
    config.fatigue_sleep_scale = 0.8f;

    /* Bio-async */
    config.broadcast_enabled = true;

    return config;
}

hypo_medulla_bridge_t* hypo_medulla_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_medulla_bridge_config_t* config) {

    if (!drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_medulla_bridge_create: drives is NULL");
        return NULL;
    }

    hypo_medulla_bridge_t* bridge = (hypo_medulla_bridge_t*)nimcp_calloc(1, sizeof(hypo_medulla_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_medulla_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = hypo_medulla_bridge_default_config();
    }

    /* Store drive system reference */
    bridge->drives = drives;

    /* Initialize medulla status with defaults */
    bridge->medulla_status.state = MEDULLA_STATE_STOPPED;
    bridge->medulla_status.arousal_level = AROUSAL_LEVEL_AWAKE;
    bridge->medulla_status.arousal_value = 0.5f;
    bridge->medulla_status.protection = PROTECTION_LEVEL_NORMAL;
    bridge->medulla_status.circadian = CIRCADIAN_PHASE_MORNING;

    /* Initialize timing */
    bridge->last_circadian_sync_us = 0;
    bridge->last_update_us = 0;

    /* Initialize bridge base (creates mutex) */
    bridge_base_init(&bridge->base, 0, "hypothalamus_medulla");

    return bridge;
}

void hypo_medulla_bridge_destroy(hypo_medulla_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hypothalamus_medulla");

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

void hypo_medulla_bridge_reset(hypo_medulla_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset medulla status */
    bridge->medulla_status.state = MEDULLA_STATE_STOPPED;
    bridge->medulla_status.arousal_level = AROUSAL_LEVEL_AWAKE;
    bridge->medulla_status.arousal_value = 0.5f;
    bridge->medulla_status.protection = PROTECTION_LEVEL_NORMAL;
    bridge->medulla_status.is_emergency = false;

    /* Reset timing */
    bridge->last_circadian_sync_us = 0;
    bridge->last_update_us = 0;

    /* Reset statistics */
    bridge->arousal_commands_sent = 0;
    bridge->protection_commands_sent = 0;
    bridge->circadian_syncs = 0;
    bridge->status_updates_received = 0;

    nimcp_mutex_unlock(bridge->base.mutex);
}

/*=============================================================================
 * CORE FUNCTIONS
 *===========================================================================*/

int hypo_medulla_bridge_update(hypo_medulla_bridge_t* bridge, float dt_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_medulla_bridge_update: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now = nimcp_time_get_us();
    bridge->last_update_us = now;

    /* Compute and send arousal command */
    hypo_medulla_arousal_cmd_t arousal_cmd = hypo_medulla_bridge_compute_arousal(bridge);
    bridge->last_arousal_cmd = arousal_cmd;

    /* Compute and send protection command */
    hypo_medulla_protection_cmd_t protection_cmd = hypo_medulla_bridge_compute_protection(bridge);
    bridge->last_protection_cmd = protection_cmd;

    /* Check if we need to sync circadian */
    if (bridge->config.enable_circadian_sync) {
        uint64_t elapsed_ms = (now - bridge->last_circadian_sync_us) / 1000;
        if (elapsed_ms >= bridge->config.circadian_sync_interval_ms) {
            /* Get circadian phase from drives if available */
            float phase = 12.0f;  /* Default to noon */
            /* TODO: Get actual circadian phase from hypothalamus */

            hypo_medulla_circadian_cmd_t circadian_cmd = hypo_medulla_bridge_compute_circadian(bridge, phase);
            bridge->last_circadian_cmd = circadian_cmd;
            bridge->last_circadian_sync_us = now;
            bridge->circadian_syncs++;
        }
    }

    /* Send commands if medulla is connected directly */
    if (bridge->medulla) {
        /* Apply arousal */
        if (arousal_cmd.force_immediate) {
            medulla_test_set_arousal(bridge->medulla, arousal_cmd.target_arousal);
        } else {
            /* Gradual adjustment */
            float current = medulla_get_arousal_level(bridge->medulla);
            float delta = (arousal_cmd.target_arousal - current) * arousal_cmd.transition_rate;
            if (delta > 0) {
                medulla_boost_arousal(bridge->medulla, delta);
            } else {
                medulla_reduce_arousal(bridge->medulla, -delta);
            }
        }

        /* Apply protection if changed */
        if (protection_cmd.target_level != bridge->medulla_status.protection) {
            medulla_test_set_protection(bridge->medulla, protection_cmd.target_level);
        }
    }

    /* Broadcast via bio-async if enabled */
    if (bridge->config.broadcast_enabled && bridge->bio_ctx) {
        hypo_medulla_bridge_broadcast_arousal(bridge);
        if (protection_cmd.target_level != bridge->medulla_status.protection) {
            hypo_medulla_bridge_broadcast_protection(bridge);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

hypo_medulla_arousal_cmd_t hypo_medulla_bridge_compute_arousal(
    hypo_medulla_bridge_t* bridge) {

    hypo_medulla_arousal_cmd_t cmd = {0};

    if (!bridge) {
        cmd.target_arousal = 0.5f;
        return cmd;
    }

    cmd.target_arousal = compute_arousal_from_drives(bridge);
    cmd.transition_rate = bridge->config.arousal_transition_rate;
    cmd.source_drive = hypo_drive_get_priority(bridge->drives);
    cmd.timestamp_us = nimcp_time_get_us();

    /* Check for emergency (forces immediate transition) */
    float urgencies[HYPO_DRIVE_COUNT];
    if (hypo_drive_get_urgencies(bridge->drives, urgencies)) {
        if (urgencies[HYPO_DRIVE_SAFETY] > 0.9f) {
            cmd.force_immediate = true;
            cmd.target_arousal = 1.0f;  /* Max arousal for emergency */
        }
    }

    return cmd;
}

hypo_medulla_protection_cmd_t hypo_medulla_bridge_compute_protection(
    hypo_medulla_bridge_t* bridge) {

    hypo_medulla_protection_cmd_t cmd = {0};

    if (!bridge) {
        cmd.target_level = PROTECTION_LEVEL_NORMAL;
        return cmd;
    }

    cmd.target_level = compute_protection_from_drives(bridge);
    cmd.timestamp_us = nimcp_time_get_us();

    /* Get stress intensity from safety drive */
    float urgencies[HYPO_DRIVE_COUNT];
    if (hypo_drive_get_urgencies(bridge->drives, urgencies)) {
        cmd.stress_intensity = urgencies[HYPO_DRIVE_SAFETY];
        cmd.threat_source = HYPO_DRIVE_SAFETY;
        cmd.is_emergency = (urgencies[HYPO_DRIVE_SAFETY] > 0.9f);
    }

    return cmd;
}

hypo_medulla_circadian_cmd_t hypo_medulla_bridge_compute_circadian(
    hypo_medulla_bridge_t* bridge,
    float circadian_phase) {

    hypo_medulla_circadian_cmd_t cmd = {0};

    if (!bridge) {
        return cmd;
    }

    cmd.circadian_phase = circadian_phase;
    cmd.target_arousal_baseline = compute_circadian_arousal(circadian_phase);
    cmd.timestamp_us = nimcp_time_get_us();

    /* Check for sleep pressure from fatigue drive */
    if (bridge->config.enable_fatigue_sleep && bridge->drives) {
        float urgencies[HYPO_DRIVE_COUNT];
        if (hypo_drive_get_urgencies(bridge->drives, urgencies)) {
            cmd.sleep_pressure_level = urgencies[HYPO_DRIVE_FATIGUE] * bridge->config.fatigue_sleep_scale;
            cmd.is_sleep_pressure = (cmd.sleep_pressure_level > 0.5f);
        }
    }

    return cmd;
}

void hypo_medulla_bridge_process_status(
    hypo_medulla_bridge_t* bridge,
    const hypo_medulla_status_t* status) {

    if (!bridge || !status) {
        return;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->medulla_status = *status;
    bridge->status_updates_received++;

    /* Modulate drives based on medulla state using nucleus inputs */
    if (bridge->drives) {
        /* Emergency state boosts paraventricular (stress/safety) */
        if (status->is_emergency) {
            hypo_drive_set_nucleus_input(bridge->drives, HYPO_NUCLEUS_PARAVENTRICULAR, 0.5f);
        }

        /* Low arousal boosts preoptic (sleep/fatigue) */
        if (status->arousal_value < 0.3f) {
            hypo_drive_set_nucleus_input(bridge->drives, HYPO_NUCLEUS_PREOPTIC, 0.3f);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
}

bool hypo_medulla_bridge_get_status(
    const hypo_medulla_bridge_t* bridge,
    hypo_medulla_status_t* status) {

    if (!bridge || !status) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_medulla_bridge_update: required parameter is NULL (bridge, status)");
        return false;
    }

    nimcp_mutex_lock(((hypo_medulla_bridge_t*)bridge)->base.mutex);
    *status = bridge->medulla_status;
    nimcp_mutex_unlock(((hypo_medulla_bridge_t*)bridge)->base.mutex);

    return true;
}

/*=============================================================================
 * MEDULLA CONNECTION
 *===========================================================================*/

bool hypo_medulla_bridge_connect(
    hypo_medulla_bridge_t* bridge,
    medulla_t medulla) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_medulla_bridge_update: bridge is NULL");
        return false;
    }

    bridge->medulla = medulla;
    return true;
}

int hypo_medulla_bridge_send_arousal(
    hypo_medulla_bridge_t* bridge,
    const hypo_medulla_arousal_cmd_t* cmd) {

    if (!bridge || !cmd) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_medulla_bridge_send_arousal: bridge or cmd is NULL");
        return -1;
    }

    /* Direct connection */
    if (bridge->medulla) {
        if (cmd->force_immediate) {
            medulla_test_set_arousal(bridge->medulla, cmd->target_arousal);
        } else {
            float current = medulla_get_arousal_level(bridge->medulla);
            float delta = (cmd->target_arousal - current) * cmd->transition_rate;
            if (delta > 0) {
                medulla_boost_arousal(bridge->medulla, delta);
            } else {
                medulla_reduce_arousal(bridge->medulla, -delta);
            }
        }
        bridge->arousal_commands_sent++;
        return 0;
    }

    /* Bio-async fallback */
    if (bridge->bio_ctx && bridge->config.broadcast_enabled) {
        return hypo_medulla_bridge_broadcast_arousal(bridge) == NIMCP_SUCCESS ? 0 : -1;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
    return -1;
}

int hypo_medulla_bridge_send_protection(
    hypo_medulla_bridge_t* bridge,
    const hypo_medulla_protection_cmd_t* cmd) {

    if (!bridge || !cmd) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_medulla_bridge_send_protection: bridge or cmd is NULL");
        return -1;
    }

    /* Direct connection */
    if (bridge->medulla) {
        medulla_test_set_protection(bridge->medulla, cmd->target_level);
        bridge->protection_commands_sent++;
        return 0;
    }

    /* Bio-async fallback */
    if (bridge->bio_ctx && bridge->config.broadcast_enabled) {
        return hypo_medulla_bridge_broadcast_protection(bridge) == NIMCP_SUCCESS ? 0 : -1;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
    return -1;
}

int hypo_medulla_bridge_request_emergency(
    hypo_medulla_bridge_t* bridge,
    const char* reason) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_medulla_bridge_request_emergency: bridge is NULL");
        return -1;
    }

    if (bridge->medulla) {
        return medulla_emergency_shutdown(bridge->medulla, reason);
    }

    /* Bio-async emergency broadcast */
    if (bridge->bio_ctx) {
        hypo_medulla_protection_cmd_t cmd = {0};
        cmd.target_level = PROTECTION_LEVEL_SHUTDOWN;
        cmd.is_emergency = true;
        cmd.stress_intensity = 1.0f;
        cmd.threat_source = HYPO_DRIVE_SAFETY;
        cmd.timestamp_us = nimcp_time_get_us();

        return hypo_medulla_bridge_send_protection(bridge, &cmd);
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: operation failed");
    return -1;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

bool hypo_medulla_bridge_register_bio(
    hypo_medulla_bridge_t* bridge,
    bool use_kg_wiring) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge is NULL");
        return false;
    }

    /* Register module */
    bio_module_info_t info = {
        .module_id = HYPO_MEDULLA_BRIDGE_MODULE_ID,
        .module_name = "hypothalamus_medulla_bridge",
        .inbox_capacity = 16,
        .user_data = bridge
    };

    bridge->bio_ctx = bio_router_register_module(&info);
    if (!bridge->bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge->bio_ctx is NULL");
        return false;
    }

    /* Register handlers for incoming messages */
    if (bio_router_register_handler(bridge->bio_ctx, BIO_MSG_MEDULLA_STATE,
                                     med_handle_state) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge->bio_ctx is NULL");
        return false;
    }

    return true;
}

uint32_t hypo_medulla_bridge_process_bio(
    hypo_medulla_bridge_t* bridge,
    uint32_t max_messages) {

    if (!bridge || !bridge->bio_ctx) {
        return 0;
    }

    return bio_router_process_inbox(bridge->bio_ctx, max_messages);
}

nimcp_error_t hypo_medulla_bridge_broadcast_arousal(
    hypo_medulla_bridge_t* bridge) {

    if (!bridge || !bridge->bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_medulla_bridge_broadcast_arousal: bridge or bio_ctx is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!bridge->config.broadcast_enabled) {
        return NIMCP_SUCCESS;
    }

    /* Construct message */
    struct {
        bio_message_header_t header;
        hypo_medulla_arousal_cmd_t cmd;
    } msg;

    msg.header.type = BIO_MSG_MEDULLA_AROUSAL_SET;
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.source_module = HYPO_MEDULLA_BRIDGE_MODULE_ID;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.payload_size = sizeof(hypo_medulla_arousal_cmd_t);
    msg.cmd = bridge->last_arousal_cmd;

    bridge->arousal_commands_sent++;

    return bio_router_broadcast(bridge->bio_ctx, &msg.header, sizeof(msg));
}

nimcp_error_t hypo_medulla_bridge_broadcast_protection(
    hypo_medulla_bridge_t* bridge) {

    if (!bridge || !bridge->bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_medulla_bridge_broadcast_protection: bridge or bio_ctx is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!bridge->config.broadcast_enabled) {
        return NIMCP_SUCCESS;
    }

    /* Construct message */
    struct {
        bio_message_header_t header;
        hypo_medulla_protection_cmd_t cmd;
    } msg;

    msg.header.type = BIO_MSG_MEDULLA_PROTECTION_SET;
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.source_module = HYPO_MEDULLA_BRIDGE_MODULE_ID;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.payload_size = sizeof(hypo_medulla_protection_cmd_t);
    msg.cmd = bridge->last_protection_cmd;

    bridge->protection_commands_sent++;

    return bio_router_broadcast(bridge->bio_ctx, &msg.header, sizeof(msg));
}

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

void hypo_medulla_bridge_get_stats(
    const hypo_medulla_bridge_t* bridge,
    uint64_t* arousal_cmds,
    uint64_t* protection_cmds,
    uint64_t* circadian_syncs) {

    if (!bridge) {
        return;
    }

    if (arousal_cmds) *arousal_cmds = bridge->arousal_commands_sent;
    if (protection_cmds) *protection_cmds = bridge->protection_commands_sent;
    if (circadian_syncs) *circadian_syncs = bridge->circadian_syncs;
}
