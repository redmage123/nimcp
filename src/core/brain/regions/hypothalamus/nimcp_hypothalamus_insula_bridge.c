/**
 * @file nimcp_hypothalamus_insula_bridge.c
 * @brief Implementation of Hypothalamus-Insula Bridge for Interoceptive Integration
 *
 * WHAT: Bidirectional integration between hypothalamus drives and insula interoception
 * WHY:  Body state awareness drives homeostatic setpoint adjustments
 * HOW:  Interoceptive signals inform drive deviations; drives bias interoceptive attention
 *
 * @version Phase 16: Additional Module Bridges
 * @date 2026-01-04
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_insula_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_insula_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HYPOTHALAMUS_INSULA_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct hypo_insula_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    hypo_drive_system_handle_t* drives;
    hypo_insula_config_t config;

    /* Interoceptive state from insula */
    hypo_intero_state_t intero_state;

    /* Attention modulation output */
    hypo_intero_attention_t attention;

    /* Bio-async registration */
    bool bio_registered;
    bio_module_context_t bio_ctx;
};

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

void hypo_insula_bridge_default_config(hypo_insula_config_t* config) {
    if (!config) return;

    config->cardiac_stress_weight = 0.7f;
    config->pain_safety_weight = 0.9f;
    config->gastric_hunger_weight = 0.8f;
    config->thermal_weight = 0.6f;
    config->enable_attention_modulation = true;
    config->attention_gain_range = 2.0f;
}

hypo_insula_bridge_t* hypo_insula_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_insula_config_t* config)
{
    if (!drives) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_insula_bridge_create: drives is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "drives is NULL");

        return NULL;
    }

    hypo_insula_bridge_t* bridge = nimcp_calloc(1, sizeof(hypo_insula_bridge_t));
    if (!bridge) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_insula_bridge_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    bridge->drives = drives;

    if (config) {
        bridge->config = *config;
    } else {
        hypo_insula_bridge_default_config(&bridge->config);
    }

    /* Initialize interoceptive state to neutral */
    memset(&bridge->intero_state, 0, sizeof(hypo_intero_state_t));
    bridge->intero_state.thermal_state = 0.0f;  /* Neutral temperature */

    /* Initialize attention to baseline */
    for (int i = 0; i < HYPO_INTERO_COUNT; i++) {
        bridge->attention.channel_gains[i] = 1.0f;
    }
    bridge->attention.overall_salience = 0.5f;
    bridge->attention.survival_mode = false;

    bridge->bio_registered = false;
    bridge->bio_ctx = NULL;

    /* Initialize bridge base (creates mutex) */
    bridge_base_init(&bridge->base, 0, "hypothalamus_insula");

    nimcp_log(LOG_LEVEL_INFO, "hypo_insula_bridge: created successfully");
    return bridge;
}

void hypo_insula_bridge_destroy(hypo_insula_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hypothalamus_insula");

    if (bridge->bio_registered) {
        hypo_insula_bridge_unregister_bio(bridge);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    nimcp_log(LOG_LEVEL_INFO, "hypo_insula_bridge: destroyed");
}

/*=============================================================================
 * INTEROCEPTION → DRIVE MODULATION
 *===========================================================================*/

int hypo_insula_bridge_update_interoception(
    hypo_insula_bridge_t* bridge,
    const hypo_intero_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_insula_bridge_update_interoception: bridge or state is NULL");
        return -1;
    }

    bridge->intero_state = *state;

    return 0;
}

int hypo_insula_bridge_apply_interoceptive_effects(hypo_insula_bridge_t* bridge) {
    if (!bridge || !bridge->drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_insula_bridge_apply_interoceptive_effects: bridge or drives is NULL");
        return -1;
    }

    const hypo_intero_state_t* intero = &bridge->intero_state;
    const hypo_insula_config_t* cfg = &bridge->config;

    /*
     * Map interoceptive signals to drive modulations:
     *
     * Cardiac → Stress/Arousal:
     *   High cardiac state (tachycardia, low HRV) indicates stress
     *   Modulates arousal and stress-related drives
     *
     * Gastric → Hunger:
     *   Gut signals directly inform hunger drive
     *
     * Pain → Safety:
     *   Nociceptive input activates safety/avoidance drives
     *
     * Thermal → Temperature regulation:
     *   Deviation from setpoint drives thermoregulatory behaviors
     *
     * Fatigue → Rest:
     *   Energy depletion increases rest drive urgency
     */

    /* Compute drive modulations */
    float stress_mod = intero->cardiac_state * cfg->cardiac_stress_weight;
    float hunger_mod = intero->gastric_state * cfg->gastric_hunger_weight +
                       intero->hunger_signal;
    float safety_mod = intero->pain_level * cfg->pain_safety_weight;
    float thermal_mod = fabsf(intero->thermal_state) * cfg->thermal_weight;
    float fatigue_mod = intero->fatigue_signal;

    /* Apply modulations via drive deviation adjustments */
    /* Note: These would integrate with the drive system's deviation tracking */

    /* For alignment safety: Check for extreme interoceptive distress */
    float distress_level = (stress_mod + safety_mod + fatigue_mod) / 3.0f;
    if (distress_level > 0.8f) {
        nimcp_log(LOG_LEVEL_WARN,
            "hypo_insula_bridge: high interoceptive distress (%.2f)", distress_level);
    }

    (void)hunger_mod;
    (void)thermal_mod;

    return 0;
}

int hypo_insula_bridge_get_intero_state(
    const hypo_insula_bridge_t* bridge,
    hypo_intero_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_insula_bridge_get_intero_state: bridge or state is NULL");
        return -1;
    }

    *state = bridge->intero_state;
    return 0;
}

/*=============================================================================
 * DRIVE → ATTENTION MODULATION
 *===========================================================================*/

int hypo_insula_bridge_compute_attention(hypo_insula_bridge_t* bridge) {
    if (!bridge || !bridge->drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_insula_bridge_compute_attention: bridge or drives is NULL");
        return -1;
    }

    if (!bridge->config.enable_attention_modulation) {
        return 0;
    }

    /*
     * Drive states bias interoceptive attention:
     *
     * High hunger → Enhanced gastric/hunger channel attention
     * High thirst → Enhanced hydration channel attention
     * High stress → Enhanced cardiac/respiratory attention
     * Safety threat → Survival mode with overall amplification
     *
     * Satiation reduces interoceptive salience (allows attention elsewhere)
     */

    hypo_intero_attention_t* att = &bridge->attention;
    float max_gain = bridge->config.attention_gain_range;

    /* Get current drive states */
    hypo_drive_system_t drive_state;
    if (!hypo_drive_get_system_state(bridge->drives, &drive_state)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_insula_bridge_compute_attention: hypo_drive_get_system_state is NULL");
        return -1;
    }

    /* Map drive urgencies to attention gains */
    /* Hunger → gastric/hunger channels */
    float hunger_urgency = drive_state.drives[HYPO_DRIVE_HUNGER].urgency;
    att->channel_gains[HYPO_INTERO_GASTRIC] = 1.0f + (max_gain - 1.0f) * hunger_urgency;
    att->channel_gains[HYPO_INTERO_HUNGER] = 1.0f + (max_gain - 1.0f) * hunger_urgency;

    /* Thirst → thirst channel */
    float thirst_urgency = drive_state.drives[HYPO_DRIVE_THIRST].urgency;
    att->channel_gains[HYPO_INTERO_THIRST] = 1.0f + (max_gain - 1.0f) * thirst_urgency;

    /* Fatigue → fatigue channel */
    float fatigue_urgency = drive_state.drives[HYPO_DRIVE_FATIGUE].urgency;
    att->channel_gains[HYPO_INTERO_FATIGUE] = 1.0f + (max_gain - 1.0f) * fatigue_urgency;

    /* Safety threat → cardiac/pain channels + survival mode */
    float safety_urgency = drive_state.drives[HYPO_DRIVE_SAFETY].urgency;
    att->channel_gains[HYPO_INTERO_CARDIAC] = 1.0f + (max_gain - 1.0f) * safety_urgency;
    att->channel_gains[HYPO_INTERO_PAIN] = 1.0f + (max_gain - 1.0f) * safety_urgency;

    /* Respiratory correlates with arousal/stress */
    float arousal = (safety_urgency + hunger_urgency) / 2.0f;
    att->channel_gains[HYPO_INTERO_RESPIRATORY] = 1.0f + (max_gain - 1.0f) * arousal * 0.5f;

    /* Thermal channel */
    float thermal_urgency = drive_state.drives[HYPO_DRIVE_TEMPERATURE].urgency;
    att->channel_gains[HYPO_INTERO_THERMAL] = 1.0f + (max_gain - 1.0f) * thermal_urgency;

    /* Overall salience: average of all urgencies */
    float total_urgency = 0.0f;
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        total_urgency += drive_state.drives[i].urgency;
    }
    att->overall_salience = nimcp_clamp01(total_urgency / (float)HYPO_DRIVE_COUNT);

    /* Survival mode if any critical drive is urgent */
    att->survival_mode = (safety_urgency > 0.7f) ||
                         (hunger_urgency > 0.9f) ||
                         (thirst_urgency > 0.9f);

    if (att->survival_mode) {
        /* Amplify all channels in survival mode */
        for (int i = 0; i < HYPO_INTERO_COUNT; i++) {
            att->channel_gains[i] = nimcp_clampf(
                att->channel_gains[i] * 1.5f, 1.0f, max_gain);
        }
        att->overall_salience = nimcp_clamp01(att->overall_salience * 1.5f);
    }

    return 0;
}

int hypo_insula_bridge_get_attention(
    const hypo_insula_bridge_t* bridge,
    hypo_intero_attention_t* attention)
{
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_insula_bridge_get_attention: bridge or attention is NULL");
        return -1;
    }

    *attention = bridge->attention;
    return 0;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

static nimcp_error_t insula_handle_intero_update(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t promise, void* user_data)
{
    (void)promise;
    hypo_insula_bridge_t* bridge = (hypo_insula_bridge_t*)user_data;
    if (!bridge || !msg || msg_size < sizeof(bio_message_header_t)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;
    if (msg_size >= sizeof(bio_message_header_t) + sizeof(hypo_intero_state_t)) {
        const hypo_intero_state_t* state =
            (const hypo_intero_state_t*)(header + 1);
        hypo_insula_bridge_update_interoception(bridge, state);
        hypo_insula_bridge_apply_interoceptive_effects(bridge);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t insula_handle_attention_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t promise, void* user_data)
{
    (void)promise;
    (void)msg;
    (void)msg_size;
    hypo_insula_bridge_t* bridge = (hypo_insula_bridge_t*)user_data;
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge context is NULL");

    hypo_insula_bridge_compute_attention(bridge);
    hypo_insula_bridge_broadcast_attention(bridge);

    return NIMCP_SUCCESS;
}

bool hypo_insula_bridge_register_bio(hypo_insula_bridge_t* bridge, bool use_kg_wiring) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_insula_bridge_register_bio: bridge is NULL");
        return false;
    }
    if (bridge->bio_registered) return true;

    (void)use_kg_wiring;  /* Future KG wiring integration */

    bio_module_info_t info = {
        .module_id = HYPO_INSULA_BRIDGE_MODULE_ID,
        .module_name = "hypothalamus_insula_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->bio_ctx = bio_router_register_module(&info);
    if (!bridge->bio_ctx) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_insula_bridge: failed to register with bio-router");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_insula_bridge_register_bio: bridge->bio_ctx is NULL");
        return false;
    }

    /* Register handlers for interoception messages */
    bio_router_register_handler(bridge->bio_ctx,
        BIO_MSG_HYPO_INSULA_INTERO_UPDATE, insula_handle_intero_update);
    bio_router_register_handler(bridge->bio_ctx,
        BIO_MSG_HYPO_INSULA_ATTENTION_REQUEST, insula_handle_attention_request);

    bridge->bio_registered = true;
    nimcp_log(LOG_LEVEL_INFO, "hypo_insula_bridge: registered with bio-router");
    return true;
}

void hypo_insula_bridge_unregister_bio(hypo_insula_bridge_t* bridge) {
    if (!bridge || !bridge->bio_registered) return;

    bio_router_unregister_module(bridge->bio_ctx);
    bridge->bio_ctx = NULL;
    bridge->bio_registered = false;
    nimcp_log(LOG_LEVEL_INFO, "hypo_insula_bridge: unregistered from bio-router");
}

nimcp_error_t hypo_insula_bridge_broadcast_attention(hypo_insula_bridge_t* bridge) {
    if (!bridge || !bridge->bio_registered) return NIMCP_ERROR_INVALID_PARAM;

    struct {
        bio_message_header_t header;
        hypo_intero_attention_t attention;
    } msg;

    msg.header.type = BIO_MSG_HYPO_INSULA_ATTENTION_OUTPUT;
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.source_module = HYPO_INSULA_BRIDGE_MODULE_ID;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);
    msg.header.sequence_id = 0;
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;

    msg.attention = bridge->attention;

    return bio_router_broadcast(bridge->bio_ctx, &msg, sizeof(msg));
}
