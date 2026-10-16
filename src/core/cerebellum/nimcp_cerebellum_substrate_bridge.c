/**
 * @file nimcp_cerebellum_substrate_bridge.c
 * @brief Cerebellum-Neural Substrate Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/cerebellum/nimcp_cerebellum_substrate_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cerebellum_substrate_bridge)

#define LOG_MODULE "CEREBELLUM_SUBSTRATE_BRIDGE"

//=============================================================================
// Cerebellum-Neural Substrate Bridge Implementation
//=============================================================================

#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"

struct cerebellum_substrate_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* cerebellum;
    neural_substrate_t* substrate;
    cerebellum_substrate_config_t config;
    cerebellum_substrate_effects_t effects;
    bio_module_context_t ctx;
    bio_router_t* router;
    bool bio_async_connected;
    uint64_t update_count;
    float prev_overall_capacity;
};

cerebellum_substrate_config_t cerebellum_substrate_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    cerebellum_substrate_bridge_heartbeat("cerebellum_sub_default_config", 0.0f);

    cerebellum_substrate_config_t cfg = {
        .enable_atp_modulation = true,
        .enable_fatigue_modulation = true,
        .enable_bio_async = false,
        .atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .min_capacity = 0.2f
    };
    return cfg;
}

cerebellum_substrate_bridge_t* cerebellum_substrate_bridge_create(void* cerebellum,
                                                                    neural_substrate_t* substrate,
                                                                    const cerebellum_substrate_config_t* config) {
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_substrate_bridge_create: substrate is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    cerebellum_substrate_bridge_heartbeat("cerebellum_sub_create", 0.0f);

    cerebellum_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(cerebellum_substrate_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cerebellum_substrate_bridge_create: allocation failed");
        return NULL;
    }

    bridge_base_init(&bridge->base, BIO_MODULE_SUBSTRATE_CEREBELLUM, "cerebellum_substrate");

    bridge->cerebellum = cerebellum;
    bridge->substrate = substrate;
    bridge->config = config ? *config : cerebellum_substrate_default_config();

    /* Initialize effects to full capacity (no degradation) */
    bridge->effects.motor_coordination = 1.0f;
    bridge->effects.timing_precision = 1.0f;
    bridge->effects.procedural_learning = 1.0f;
    bridge->effects.error_correction = 1.0f;
    bridge->effects.overall_capacity = 1.0f;

    bridge->bio_async_connected = false;
    bridge->ctx = NULL;
    bridge->router = NULL;
    bridge->update_count = 0;
    bridge->prev_overall_capacity = 1.0f;

    return bridge;
}

void cerebellum_substrate_bridge_destroy(cerebellum_substrate_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "cerebellum_substrate");

    /* Phase 8: Heartbeat at operation start */
    cerebellum_substrate_bridge_heartbeat("cerebellum_sub_destroy", 0.0f);

    if (bridge->bio_async_connected && bridge->ctx) {
        bio_router_unregister_module(bridge->ctx);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int cerebellum_substrate_bridge_update(cerebellum_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_substrate_bridge_update: bridge is NULL");
        return -1;
    }
    if (!bridge->substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_substrate_bridge_update: substrate is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cerebellum_substrate_bridge_heartbeat("cerebellum_sub_update", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cerebellum_substrate_bridge_update: validation failed");
        return -1;
    }

    float atp = metabolic.atp_level;
    float metabolic_cap = metabolic.metabolic_capacity;
    float min_cap = bridge->config.min_capacity;

    /*
     * ATP modulation: ATP depletion reduces motor coordination
     * and timing precision.
     */
    if (bridge->config.enable_atp_modulation) {
        bridge->effects.motor_coordination = nimcp_clampf(
            atp * bridge->config.atp_sensitivity, min_cap, 1.0f);
        bridge->effects.timing_precision = nimcp_clampf(
            atp * 0.95f * bridge->config.atp_sensitivity, min_cap, 1.0f);
    }

    /*
     * Fatigue modulation: Fatigue impairs procedural learning
     * and error correction.
     */
    if (bridge->config.enable_fatigue_modulation) {
        bridge->effects.procedural_learning = nimcp_clampf(
            metabolic_cap * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
        bridge->effects.error_correction = nimcp_clampf(
            metabolic_cap * 0.9f * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
    }

    /* Combined overall capacity from all effect dimensions */
    bridge->effects.overall_capacity =
        (bridge->effects.motor_coordination +
         bridge->effects.timing_precision +
         bridge->effects.procedural_learning +
         bridge->effects.error_correction) / 4.0f;

    bridge->update_count++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int cerebellum_substrate_bridge_get_effects(const cerebellum_substrate_bridge_t* bridge,
                                              cerebellum_substrate_effects_t* effects) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_substrate_bridge_get_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_substrate_bridge_get_effects: effects is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cerebellum_substrate_bridge_heartbeat("cerebellum_sub_get_effects", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int cerebellum_substrate_bridge_apply_effects(cerebellum_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_substrate_bridge_apply_effects: bridge is NULL");
        return -1;
    }

    if (!bridge->bio_async_connected || !bridge->ctx) return 0;

    /* Phase 8: Heartbeat at operation start */
    cerebellum_substrate_bridge_heartbeat("cerebellum_sub_apply_effects", 0.0f);

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
                        BIO_MODULE_SUBSTRATE_CEREBELLUM, 0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_DOPAMINE;
    msg.bridge_module_id = BIO_MODULE_SUBSTRATE_CEREBELLUM;
    msg.processing_capacity = bridge->effects.motor_coordination;
    msg.overall_capacity = bridge->effects.overall_capacity;
    msg.effect_values[0] = bridge->effects.motor_coordination;
    msg.effect_values[1] = bridge->effects.timing_precision;
    msg.effect_values[2] = bridge->effects.procedural_learning;
    msg.effect_values[3] = bridge->effects.error_correction;
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
                            BIO_MODULE_SUBSTRATE_CEREBELLUM, 0, sizeof(update_msg));
        update_msg.bridge_module_id = BIO_MODULE_SUBSTRATE_CEREBELLUM;
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

int cerebellum_substrate_bridge_register_bio_async(cerebellum_substrate_bridge_t* bridge,
                                                     bio_router_t* router) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_substrate_bridge_register_bio_async: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cerebellum_substrate_bridge_heartbeat("cerebellum_sub_register_bio_async", 0.0f);

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
        .module_id = BIO_MODULE_SUBSTRATE_CEREBELLUM,
        .module_name = "cerebellum_substrate_bridge",
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
