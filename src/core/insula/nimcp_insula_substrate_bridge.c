/**
 * @file nimcp_insula_substrate_bridge.c
 * @brief Insula-Neural Substrate Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/insula/nimcp_insula_substrate_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(insula_substrate_bridge)

#define LOG_MODULE "INSULA_SUBSTRATE_BRIDGE"

//=============================================================================
// Insula-Neural Substrate Bridge Implementation
//=============================================================================

#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"

struct insula_substrate_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* insula;
    neural_substrate_t* substrate;
    insula_substrate_config_t config;
    insula_substrate_effects_t effects;
    bio_module_context_t ctx;
    bio_router_t* router;
    bool bio_async_connected;
    uint64_t update_count;
    float prev_overall_capacity;
};

insula_substrate_config_t insula_substrate_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    insula_substrate_bridge_heartbeat("insula_sub_default_config", 0.0f);

    insula_substrate_config_t cfg = {
        .enable_atp_modulation = true,
        .enable_fatigue_modulation = true,
        .enable_bio_async = false,
        .atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .min_capacity = 0.2f
    };
    return cfg;
}

insula_substrate_bridge_t* insula_substrate_bridge_create(void* insula,
                                                               neural_substrate_t* substrate,
                                                               const insula_substrate_config_t* config) {
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_substrate_bridge_create: substrate is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    insula_substrate_bridge_heartbeat("insula_sub_create", 0.0f);

    insula_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(insula_substrate_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "insula_substrate_bridge_create: allocation failed");
        return NULL;
    }

    bridge_base_init(&bridge->base, BIO_MODULE_SUBSTRATE_INSULA, "insula_substrate");

    bridge->insula = insula;
    bridge->substrate = substrate;
    bridge->config = config ? *config : insula_substrate_default_config();

    /* Initialize effects to full capacity (no degradation) */
    bridge->effects.interoceptive_accuracy = 1.0f;
    bridge->effects.emotional_awareness = 1.0f;
    bridge->effects.disgust_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->effects.empathic_response = 1.0f;
    bridge->effects.overall_capacity = 1.0f;

    bridge->bio_async_connected = false;
    bridge->ctx = NULL;
    bridge->router = NULL;
    bridge->update_count = 0;
    bridge->prev_overall_capacity = 1.0f;

    return bridge;
}

void insula_substrate_bridge_destroy(insula_substrate_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "insula_substrate");

    /* Phase 8: Heartbeat at operation start */
    insula_substrate_bridge_heartbeat("insula_sub_destroy", 0.0f);

    if (bridge->bio_async_connected && bridge->ctx) {
        bio_router_unregister_module(bridge->ctx);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int insula_substrate_bridge_update(insula_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_substrate_bridge_update: bridge is NULL");
        return -1;
    }
    if (!bridge->substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_substrate_bridge_update: substrate is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    insula_substrate_bridge_heartbeat("insula_sub_update", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "insula_substrate_bridge_update: validation failed");
        return -1;
    }

    float atp = metabolic.atp_level;
    float metabolic_cap = metabolic.metabolic_capacity;
    float min_cap = bridge->config.min_capacity;

    /*
     * ATP modulation: ATP depletion reduces interoceptive accuracy
     * and emotional awareness quality.
     */
    if (bridge->config.enable_atp_modulation) {
        bridge->effects.interoceptive_accuracy = nimcp_clampf(
            atp * bridge->config.atp_sensitivity, min_cap, 1.0f);
        bridge->effects.emotional_awareness = nimcp_clampf(
            atp * 0.95f * bridge->config.atp_sensitivity, min_cap, 1.0f);
    }

    /*
     * Fatigue modulation: Fatigue impairs disgust sensitivity
     * and empathic response processing.
     */
    if (bridge->config.enable_fatigue_modulation) {
        bridge->effects.disgust_sensitivity = nimcp_clampf(
            metabolic_cap * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
        bridge->effects.empathic_response = nimcp_clampf(
            metabolic_cap * 0.9f * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
    }

    /* Combined overall capacity from all effect dimensions */
    bridge->effects.overall_capacity =
        (bridge->effects.interoceptive_accuracy +
         bridge->effects.emotional_awareness +
         bridge->effects.disgust_sensitivity +
         bridge->effects.empathic_response) / 4.0f;

    bridge->update_count++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int insula_substrate_bridge_get_effects(const insula_substrate_bridge_t* bridge,
                                           insula_substrate_effects_t* effects) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_substrate_bridge_get_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_substrate_bridge_get_effects: effects is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    insula_substrate_bridge_heartbeat("insula_sub_get_effects", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int insula_substrate_bridge_apply_effects(insula_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_substrate_bridge_apply_effects: bridge is NULL");
        return -1;
    }

    if (!bridge->bio_async_connected || !bridge->ctx) return 0;

    /* Phase 8: Heartbeat at operation start */
    insula_substrate_bridge_heartbeat("insula_sub_apply_effects", 0.0f);

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
                        BIO_MODULE_SUBSTRATE_INSULA, 0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_DOPAMINE;
    msg.bridge_module_id = BIO_MODULE_SUBSTRATE_INSULA;
    msg.processing_capacity = bridge->effects.interoceptive_accuracy;
    msg.overall_capacity = bridge->effects.overall_capacity;
    msg.effect_values[0] = bridge->effects.interoceptive_accuracy;
    msg.effect_values[1] = bridge->effects.emotional_awareness;
    msg.effect_values[2] = bridge->effects.disgust_sensitivity;
    msg.effect_values[3] = bridge->effects.empathic_response;
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
                            BIO_MODULE_SUBSTRATE_INSULA, 0, sizeof(update_msg));
        update_msg.bridge_module_id = BIO_MODULE_SUBSTRATE_INSULA;
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

int insula_substrate_bridge_register_bio_async(insula_substrate_bridge_t* bridge,
                                                  bio_router_t* router) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_substrate_bridge_register_bio_async: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    insula_substrate_bridge_heartbeat("insula_sub_register_bio_async", 0.0f);

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
        .module_id = BIO_MODULE_SUBSTRATE_INSULA,
        .module_name = "insula_substrate_bridge",
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
