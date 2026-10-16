/**
 * @file nimcp_motor_substrate_bridge.c
 * @brief Motor-Neural Substrate Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/motor/nimcp_motor_substrate_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"
#include "utils/geometry/nimcp_lie_group.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(motor_substrate_bridge)

#define LOG_MODULE "MOTOR_SUBSTRATE_BRIDGE"

//=============================================================================
// Motor-Neural Substrate Bridge Implementation
//=============================================================================

#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"

struct motor_substrate_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* motor;
    neural_substrate_t* substrate;
    motor_substrate_config_t config;
    motor_substrate_effects_t effects;
    bio_module_context_t ctx;
    bio_router_t* router;
    bool bio_async_connected;
    uint64_t update_count;
    float prev_overall_capacity;
};

motor_substrate_config_t motor_substrate_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    motor_substrate_bridge_heartbeat("motor_sub_default_config", 0.0f);

    motor_substrate_config_t cfg = {
        .enable_atp_modulation = true,
        .enable_fatigue_modulation = true,
        .enable_bio_async = false,
        .atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .min_capacity = 0.2f
    };
    return cfg;
}

motor_substrate_bridge_t* motor_substrate_bridge_create(void* motor,
                                                               neural_substrate_t* substrate,
                                                               const motor_substrate_config_t* config) {
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_substrate_bridge_create: substrate is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    motor_substrate_bridge_heartbeat("motor_sub_create", 0.0f);

    motor_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(motor_substrate_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "motor_substrate_bridge_create: allocation failed");
        return NULL;
    }

    bridge_base_init(&bridge->base, BIO_MODULE_SUBSTRATE_MOTOR, "motor_substrate");

    bridge->motor = motor;
    bridge->substrate = substrate;
    bridge->config = config ? *config : motor_substrate_default_config();

    /* Initialize effects to full capacity (no degradation) */
    bridge->effects.motor_precision = 1.0f;
    bridge->effects.motor_speed = 1.0f;
    bridge->effects.motor_endurance = 1.0f;
    bridge->effects.coordination = 1.0f;
    bridge->effects.overall_capacity = 1.0f;

    bridge->bio_async_connected = false;
    bridge->ctx = NULL;
    bridge->router = NULL;
    bridge->update_count = 0;
    bridge->prev_overall_capacity = 1.0f;

    return bridge;
}

void motor_substrate_bridge_destroy(motor_substrate_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "motor_substrate");

    /* Phase 8: Heartbeat at operation start */
    motor_substrate_bridge_heartbeat("motor_sub_destroy", 0.0f);

    if (bridge->bio_async_connected && bridge->ctx) {
        bio_router_unregister_module(bridge->ctx);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int motor_substrate_bridge_update(motor_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_substrate_bridge_update: bridge is NULL");
        return -1;
    }
    if (!bridge->substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_substrate_bridge_update: substrate is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    motor_substrate_bridge_heartbeat("motor_sub_update", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_substrate_bridge_update: validation failed");
        return -1;
    }

    float atp = metabolic.atp_level;
    float metabolic_cap = metabolic.metabolic_capacity;
    float min_cap = bridge->config.min_capacity;

    /*
     * ATP modulation: ATP depletion reduces motor precision
     * and motor speed quality.
     */
    if (bridge->config.enable_atp_modulation) {
        bridge->effects.motor_precision = nimcp_clampf(
            atp * bridge->config.atp_sensitivity, min_cap, 1.0f);
        bridge->effects.motor_speed = nimcp_clampf(
            atp * 0.95f * bridge->config.atp_sensitivity, min_cap, 1.0f);
    }

    /*
     * Fatigue modulation: Fatigue impairs motor endurance
     * and coordination.
     */
    if (bridge->config.enable_fatigue_modulation) {
        bridge->effects.motor_endurance = nimcp_clampf(
            metabolic_cap * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
        bridge->effects.coordination = nimcp_clampf(
            metabolic_cap * 0.9f * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
    }

    /* Combined overall capacity using SO(3) geodesic distance to measure
     * how far the current motor state deviates from the identity (full capacity).
     * The 4 motor dimensions are mapped to an axis-angle representation in SO(3),
     * where the geodesic distance from identity captures the total degradation
     * more accurately than a simple average (respects rotational geometry). */
    {
        float axis[3] = {
            bridge->effects.motor_precision - bridge->effects.motor_speed,
            bridge->effects.motor_endurance - bridge->effects.coordination,
            (bridge->effects.motor_precision + bridge->effects.motor_speed) * 0.5f -
            (bridge->effects.motor_endurance + bridge->effects.coordination) * 0.5f
        };
        float axis_norm = sqrtf(axis[0]*axis[0] + axis[1]*axis[1] + axis[2]*axis[2]);
        float mean_capacity = (bridge->effects.motor_precision +
                               bridge->effects.motor_speed +
                               bridge->effects.motor_endurance +
                               bridge->effects.coordination) / 4.0f;

        if (axis_norm > 1e-6f) {
            /* Use geodesic distance from identity as anisotropy penalty */
            float unit_axis[3] = { axis[0]/axis_norm, axis[1]/axis_norm, axis[2]/axis_norm };
            float angle = axis_norm * 0.5f;  /* Scale deviation to small angle */
            so3_rotation_t R = so3_from_axis_angle(unit_axis, angle);
            so3_rotation_t I = so3_identity();
            float geo_dist = so3_distance(&I, &R);
            /* Penalize capacity by geodesic distance (anisotropy in motor state) */
            bridge->effects.overall_capacity = nimcp_clampf(
                mean_capacity - 0.1f * geo_dist, min_cap, 1.0f);
        } else {
            /* Isotropic motor state — use simple average */
            bridge->effects.overall_capacity = mean_capacity;
        }
    }

    bridge->update_count++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int motor_substrate_bridge_get_effects(const motor_substrate_bridge_t* bridge,
                                           motor_substrate_effects_t* effects) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_substrate_bridge_get_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_substrate_bridge_get_effects: effects is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    motor_substrate_bridge_heartbeat("motor_sub_get_effects", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int motor_substrate_bridge_apply_effects(motor_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_substrate_bridge_apply_effects: bridge is NULL");
        return -1;
    }

    if (!bridge->bio_async_connected || !bridge->ctx) return 0;

    /* Phase 8: Heartbeat at operation start */
    motor_substrate_bridge_heartbeat("motor_sub_apply_effects", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Read current metabolic state for broadcast */
    substrate_metabolic_state_t metabolic;
    float atp_level = 1.0f;
    float fatigue_level = 0.0f;
    if (bridge->substrate && substrate_get_metabolic_state(bridge->substrate, &metabolic) == 0) {
        atp_level = metabolic.atp_level;
        fatigue_level = 1.0f - metabolic.metabolic_capacity;
    }

    /* Broadcast substrate modulation message */
    bio_msg_substrate_modulation_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_SUBSTRATE_MODULATION,
                        BIO_MODULE_SUBSTRATE_MOTOR, 0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_DOPAMINE;
    msg.bridge_module_id = BIO_MODULE_SUBSTRATE_MOTOR;
    msg.processing_capacity = bridge->effects.motor_precision;
    msg.overall_capacity = bridge->effects.overall_capacity;
    msg.effect_values[0] = bridge->effects.motor_precision;
    msg.effect_values[1] = bridge->effects.motor_speed;
    msg.effect_values[2] = bridge->effects.motor_endurance;
    msg.effect_values[3] = bridge->effects.coordination;
    msg.atp_level = atp_level;
    msg.fatigue_level = fatigue_level;
    msg.update_count = bridge->update_count;
    msg.critical_low = (atp_level < bridge->config.min_capacity);
    bio_router_broadcast(bridge->ctx, &msg, sizeof(msg));

    /* Send capacity update if significant change occurred */
    float delta = bridge->effects.overall_capacity - bridge->prev_overall_capacity;
    if (delta < -0.1f || delta > 0.1f) {
        bio_msg_substrate_capacity_update_t update_msg;
        memset(&update_msg, 0, sizeof(update_msg));
        bio_msg_init_header(&update_msg.header, BIO_MSG_SUBSTRATE_CAPACITY_UPDATE,
                            BIO_MODULE_SUBSTRATE_MOTOR, 0, sizeof(update_msg));
        update_msg.bridge_module_id = BIO_MODULE_SUBSTRATE_MOTOR;
        update_msg.old_capacity = bridge->prev_overall_capacity;
        update_msg.new_capacity = bridge->effects.overall_capacity;
        update_msg.delta = delta;
        update_msg.significant_change = true;
        bio_router_broadcast(bridge->ctx, &update_msg, sizeof(update_msg));
    }
    bridge->prev_overall_capacity = bridge->effects.overall_capacity;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int motor_substrate_bridge_register_bio_async(motor_substrate_bridge_t* bridge,
                                                  bio_router_t* router) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_substrate_bridge_register_bio_async: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    motor_substrate_bridge_heartbeat("motor_sub_register_bio_async", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Unregister existing connection if present */
    if (bridge->bio_async_connected && bridge->ctx) {
        bio_router_unregister_module(bridge->ctx);
        bridge->ctx = NULL;
        bridge->bio_async_connected = false;
    }

    /* NULL router means disconnect only */
    if (!router) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    bridge->router = router;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SUBSTRATE_MOTOR,
        .module_name = "motor_substrate_bridge",
        .inbox_capacity = 16,
        .user_data = bridge
    };
    bridge->ctx = bio_router_register_module(&info);
    if (bridge->ctx) {
        bridge->bio_async_connected = true;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}
