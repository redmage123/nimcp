/**
 * @file nimcp_occipital_substrate_bridge.c
 * @brief Occipital Cortex-Neural Substrate Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/occipital/nimcp_occipital_substrate_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(occipital_substrate_bridge)

#define LOG_MODULE "OCCIPITAL_SUBSTRATE_BRIDGE"

//=============================================================================
// Occipital Cortex-Neural Substrate Bridge Implementation
//=============================================================================

#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"

struct occipital_substrate_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* occipital;
    neural_substrate_t* substrate;
    occipital_substrate_config_t config;
    occipital_substrate_effects_t effects;
    bio_module_context_t ctx;
    bio_router_t* router;
    bool bio_async_connected;
    uint64_t update_count;
    float prev_overall_capacity;
};

occipital_substrate_config_t occipital_substrate_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    occipital_substrate_bridge_heartbeat("occipital_sub_default_config", 0.0f);

    occipital_substrate_config_t cfg = {
        .enable_atp_modulation = true,
        .enable_fatigue_modulation = true,
        .enable_bio_async = false,
        .atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .min_capacity = 0.2f
    };
    return cfg;
}

occipital_substrate_bridge_t* occipital_substrate_bridge_create(void* occipital,
                                                               neural_substrate_t* substrate,
                                                               const occipital_substrate_config_t* config) {
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_substrate_bridge_create: substrate is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    occipital_substrate_bridge_heartbeat("occipital_sub_create", 0.0f);

    occipital_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(occipital_substrate_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "occipital_substrate_bridge_create: allocation failed");
        return NULL;
    }

    bridge_base_init(&bridge->base, BIO_MODULE_SUBSTRATE_OCCIPITAL, "occipital_substrate");

    bridge->occipital = occipital;
    bridge->substrate = substrate;
    bridge->config = config ? *config : occipital_substrate_default_config();

    /* Initialize effects to full capacity (no degradation) */
    bridge->effects.primary_visual = 1.0f;
    bridge->effects.color_processing = 1.0f;
    bridge->effects.form_processing = 1.0f;
    bridge->effects.motion_perception = 1.0f;
    bridge->effects.overall_capacity = 1.0f;

    bridge->bio_async_connected = false;
    bridge->ctx = NULL;
    bridge->router = NULL;
    bridge->update_count = 0;
    bridge->prev_overall_capacity = 1.0f;

    return bridge;
}

void occipital_substrate_bridge_destroy(occipital_substrate_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "occipital_substrate");

    /* Phase 8: Heartbeat at operation start */
    occipital_substrate_bridge_heartbeat("occipital_sub_destroy", 0.0f);

    if (bridge->bio_async_connected && bridge->ctx) {
        bio_router_unregister_module(bridge->ctx);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int occipital_substrate_bridge_update(occipital_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_substrate_bridge_update: bridge is NULL");
        return -1;
    }
    if (!bridge->substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_substrate_bridge_update: substrate is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    occipital_substrate_bridge_heartbeat("occipital_sub_update", 0.0f);

    /* Read metabolic state OUTSIDE mutex (external call) */
    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_substrate_bridge_update: validation failed");
        return -1;
    }

    float atp = metabolic.atp_level;
    float metabolic_cap = metabolic.metabolic_capacity;

    nimcp_mutex_lock(bridge->base.mutex);

    float min_cap = bridge->config.min_capacity;

    /*
     * ATP modulation: ATP depletion reduces primary visual processing
     * and color processing quality.
     */
    if (bridge->config.enable_atp_modulation) {
        bridge->effects.primary_visual = nimcp_clampf(
            atp * bridge->config.atp_sensitivity, min_cap, 1.0f);
        bridge->effects.color_processing = nimcp_clampf(
            atp * 0.95f * bridge->config.atp_sensitivity, min_cap, 1.0f);
    }

    /*
     * Fatigue modulation: Fatigue impairs form processing
     * and motion perception.
     */
    if (bridge->config.enable_fatigue_modulation) {
        bridge->effects.form_processing = nimcp_clampf(
            metabolic_cap * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
        bridge->effects.motion_perception = nimcp_clampf(
            metabolic_cap * 0.9f * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
    }

    /* Combined overall capacity from all effect dimensions */
    bridge->effects.overall_capacity =
        (bridge->effects.primary_visual +
         bridge->effects.color_processing +
         bridge->effects.form_processing +
         bridge->effects.motion_perception) / 4.0f;

    bridge->update_count++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int occipital_substrate_bridge_get_effects(const occipital_substrate_bridge_t* bridge,
                                           occipital_substrate_effects_t* effects) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_substrate_bridge_get_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_substrate_bridge_get_effects: effects is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    occipital_substrate_bridge_heartbeat("occipital_sub_get_effects", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int occipital_substrate_bridge_apply_effects(occipital_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_substrate_bridge_apply_effects: bridge is NULL");
        return -1;
    }

    if (!bridge->bio_async_connected || !bridge->ctx) return 0;

    /* Phase 8: Heartbeat at operation start */
    occipital_substrate_bridge_heartbeat("occipital_sub_apply_effects", 0.0f);

    /* Read current metabolic state OUTSIDE mutex (external call) */
    substrate_metabolic_state_t metabolic;
    float atp_level = 1.0f;
    float fatigue_level = 0.0f;
    if (bridge->substrate && substrate_get_metabolic_state(bridge->substrate, &metabolic) == 0) {
        atp_level = metabolic.atp_level;
        fatigue_level = 1.0f - metabolic.metabolic_capacity;
    }

    /* Take snapshot of effects under lock */
    nimcp_mutex_lock(bridge->base.mutex);

    bio_msg_substrate_modulation_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_SUBSTRATE_MODULATION,
                        BIO_MODULE_SUBSTRATE_OCCIPITAL, 0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_DOPAMINE;
    msg.bridge_module_id = BIO_MODULE_SUBSTRATE_OCCIPITAL;
    msg.processing_capacity = bridge->effects.primary_visual;
    msg.overall_capacity = bridge->effects.overall_capacity;
    msg.effect_values[0] = bridge->effects.primary_visual;
    msg.effect_values[1] = bridge->effects.color_processing;
    msg.effect_values[2] = bridge->effects.form_processing;
    msg.effect_values[3] = bridge->effects.motion_perception;
    msg.atp_level = atp_level;
    msg.fatigue_level = fatigue_level;
    msg.update_count = bridge->update_count;
    msg.critical_low = (atp_level < bridge->config.min_capacity);

    /* Check if capacity update needed */
    float delta = bridge->effects.overall_capacity - bridge->prev_overall_capacity;
    bool need_capacity_update = (delta < -0.1f || delta > 0.1f);
    bio_msg_substrate_capacity_update_t update_msg;
    if (need_capacity_update) {
        memset(&update_msg, 0, sizeof(update_msg));
        bio_msg_init_header(&update_msg.header, BIO_MSG_SUBSTRATE_CAPACITY_UPDATE,
                            BIO_MODULE_SUBSTRATE_OCCIPITAL, 0, sizeof(update_msg));
        update_msg.bridge_module_id = BIO_MODULE_SUBSTRATE_OCCIPITAL;
        update_msg.old_capacity = bridge->prev_overall_capacity;
        update_msg.new_capacity = bridge->effects.overall_capacity;
        update_msg.delta = delta;
        update_msg.significant_change = true;
    }
    bridge->prev_overall_capacity = bridge->effects.overall_capacity;
    bio_module_context_t ctx_copy = bridge->ctx;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Broadcast OUTSIDE mutex (external calls) */
    bio_router_broadcast(ctx_copy, &msg, sizeof(msg));
    if (need_capacity_update) {
        bio_router_broadcast(ctx_copy, &update_msg, sizeof(update_msg));
    }

    return 0;
}

int occipital_substrate_bridge_register_bio_async(occipital_substrate_bridge_t* bridge,
                                                  bio_router_t* router) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_substrate_bridge_register_bio_async: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    occipital_substrate_bridge_heartbeat("occipital_sub_register_bio_async", 0.0f);

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
        .module_id = BIO_MODULE_SUBSTRATE_OCCIPITAL,
        .module_name = "occipital_substrate_bridge",
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
