/**
 * @file nimcp_hypothalamus_sleep_bridge.c
 * @brief Implementation of Hypothalamus-Sleep/Wake Bridge for Circadian Integration
 *
 * WHAT: Bidirectional integration between hypothalamus SCN and sleep/wake system
 * WHY:  Circadian timing drives sleep propensity; sleep state modulates drives
 * HOW:  SCN phase → sleep pressure/timing; sleep state → drive modulation
 *
 * @version Phase 16: Additional Module Bridges
 * @date 2026-01-04
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_sleep_bridge.h"
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
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_sleep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HYPOTHALAMUS_SLEEP_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct hypo_sleep_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    hypo_drive_system_handle_t* drives;
    hypo_sleep_config_t config;

    /* SCN circadian state */
    hypo_scn_output_t scn_state;

    /* Sleep feedback state */
    hypo_sleep_feedback_t sleep_feedback;

    /* Computed alertness */
    hypo_alertness_t alertness;

    /* Bio-async registration */
    bool bio_registered;
    bio_module_context_t bio_ctx;
};

/**
 * @brief Compute circadian phase zone from hour
 */
static hypo_circadian_phase_t compute_phase_zone(float phase_hours) {
    /* Normalize to 0-24 */
    while (phase_hours < 0.0f) phase_hours += 24.0f;
    while (phase_hours >= 24.0f) phase_hours -= 24.0f;

    if (phase_hours >= 22.0f || phase_hours < 5.0f) {
        return HYPO_CIRCADIAN_NIGHT;
    } else if (phase_hours >= 5.0f && phase_hours < 8.0f) {
        return HYPO_CIRCADIAN_DAWN;
    } else if (phase_hours >= 8.0f && phase_hours < 18.0f) {
        return HYPO_CIRCADIAN_DAY;
    } else {
        return HYPO_CIRCADIAN_DUSK;
    }
}

/**
 * @brief Compute circadian sleep propensity curve
 * Models the "Process C" circadian component of the two-process model
 */
static float compute_circadian_sleep_propensity(float phase_hours) {
    /* Normalize */
    while (phase_hours < 0.0f) phase_hours += 24.0f;
    while (phase_hours >= 24.0f) phase_hours -= 24.0f;

    /* Sinusoidal model with peak at ~3am (phase 3) and trough at ~3pm (phase 15) */
    float radians = (phase_hours - 3.0f) * (NIMCP_TWO_PI_F / 24.0f);
    float propensity = 0.5f + 0.5f * cosf(radians);

    return nimcp_clamp01(propensity);
}

/**
 * @brief Compute circadian wake propensity (inverse of sleep propensity)
 */
static float compute_circadian_wake_propensity(float phase_hours) {
    return 1.0f - compute_circadian_sleep_propensity(phase_hours);
}

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

void hypo_sleep_bridge_default_config(hypo_sleep_config_t* config) {
    if (!config) return;

    config->phase_wake_threshold = 6.0f;
    config->phase_sleep_threshold = 22.0f;
    config->pressure_to_drive_weight = 0.8f;
    config->melatonin_arousal_weight = 0.6f;
    config->enable_drive_suppression = true;
    config->drive_suppression_factor = 0.2f;
}

hypo_sleep_bridge_t* hypo_sleep_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_sleep_config_t* config)
{
    if (!drives) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_sleep_bridge_create: drives is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "drives is NULL");

        return NULL;
    }

    hypo_sleep_bridge_t* bridge = nimcp_calloc(1, sizeof(hypo_sleep_bridge_t));
    if (!bridge) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_sleep_bridge_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    bridge->drives = drives;

    if (config) {
        bridge->config = *config;
    } else {
        hypo_sleep_bridge_default_config(&bridge->config);
    }

    /* Initialize SCN to daytime state */
    bridge->scn_state.phase = 12.0f;  /* Noon */
    bridge->scn_state.zone = HYPO_CIRCADIAN_DAY;
    bridge->scn_state.amplitude = 1.0f;
    bridge->scn_state.melatonin_level = 0.0f;
    bridge->scn_state.cortisol_anticipation = 0.0f;
    bridge->scn_state.wake_propensity = compute_circadian_wake_propensity(12.0f);
    bridge->scn_state.sleep_propensity = compute_circadian_sleep_propensity(12.0f);

    /* Initialize sleep feedback to awake */
    memset(&bridge->sleep_feedback, 0, sizeof(hypo_sleep_feedback_t));
    bridge->sleep_feedback.is_asleep = false;

    bridge->alertness = HYPO_ALERTNESS_NORMAL;
    bridge->bio_registered = false;
    bridge->bio_ctx = NULL;

    /* Initialize bridge base (creates mutex) */
    bridge_base_init(&bridge->base, 0, "hypothalamus_sleep");

    nimcp_log(LOG_LEVEL_INFO, "hypo_sleep_bridge: created successfully");
    return bridge;
}

void hypo_sleep_bridge_destroy(hypo_sleep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hypothalamus_sleep");

    if (bridge->bio_registered) {
        hypo_sleep_bridge_unregister_bio(bridge);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    nimcp_log(LOG_LEVEL_INFO, "hypo_sleep_bridge: destroyed");
}

/*=============================================================================
 * SCN → SLEEP MODULATION
 *===========================================================================*/

int hypo_sleep_bridge_update_scn(
    hypo_sleep_bridge_t* bridge,
    const hypo_scn_output_t* scn)
{
    if (!bridge || !scn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_sleep_bridge_update_scn: bridge or scn is NULL");
        return -1;
    }

    bridge->scn_state = *scn;

    /* Update phase zone if not set */
    bridge->scn_state.zone = compute_phase_zone(scn->phase);

    /* Compute propensities if not provided */
    if (scn->wake_propensity == 0.0f && scn->sleep_propensity == 0.0f) {
        bridge->scn_state.wake_propensity = compute_circadian_wake_propensity(scn->phase);
        bridge->scn_state.sleep_propensity = compute_circadian_sleep_propensity(scn->phase);
    }

    return 0;
}

int hypo_sleep_bridge_compute_sleep_propensity(hypo_sleep_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    hypo_scn_output_t* scn = &bridge->scn_state;

    /* Compute circadian propensities from phase */
    scn->sleep_propensity = compute_circadian_sleep_propensity(scn->phase);
    scn->wake_propensity = compute_circadian_wake_propensity(scn->phase);

    /* Melatonin follows sleep propensity with some lag */
    float melatonin_target = (scn->zone == HYPO_CIRCADIAN_NIGHT ||
                             scn->zone == HYPO_CIRCADIAN_DUSK) ? 1.0f : 0.0f;
    scn->melatonin_level = 0.9f * scn->melatonin_level + 0.1f * melatonin_target;

    /* Cortisol anticipation rises before dawn */
    if (scn->zone == HYPO_CIRCADIAN_NIGHT && scn->phase > 3.0f) {
        scn->cortisol_anticipation = (scn->phase - 3.0f) / 3.0f;
    } else if (scn->zone == HYPO_CIRCADIAN_DAWN) {
        scn->cortisol_anticipation = 1.0f;
    } else {
        scn->cortisol_anticipation *= 0.9f;  /* Decay during day */
    }

    scn->cortisol_anticipation = nimcp_clamp01(scn->cortisol_anticipation);

    return 0;
}

int hypo_sleep_bridge_get_scn(
    const hypo_sleep_bridge_t* bridge,
    hypo_scn_output_t* scn)
{
    if (!bridge || !scn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_sleep_bridge_get_scn: bridge or scn is NULL");
        return -1;
    }

    *scn = bridge->scn_state;
    return 0;
}

hypo_alertness_t hypo_sleep_bridge_get_alertness(const hypo_sleep_bridge_t* bridge) {
    if (!bridge) return HYPO_ALERTNESS_NORMAL;
    return bridge->alertness;
}

/*=============================================================================
 * SLEEP STATE → DRIVE MODULATION
 *===========================================================================*/

int hypo_sleep_bridge_update_sleep_state(
    hypo_sleep_bridge_t* bridge,
    const hypo_sleep_feedback_t* feedback)
{
    if (!bridge || !feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_sleep_bridge_update_sleep_state: bridge or feedback is NULL");
        return -1;
    }

    bridge->sleep_feedback = *feedback;

    /* Update alertness based on sleep state */
    if (feedback->is_asleep) {
        if (feedback->sleep_state == 3) {  /* Deep NREM */
            bridge->alertness = HYPO_ALERTNESS_UNCONSCIOUS;
        } else {
            bridge->alertness = HYPO_ALERTNESS_DROWSY;
        }
    } else {
        /* Awake - alertness depends on sleep pressure and circadian phase */
        float pressure = feedback->sleep_pressure;
        float wake_prop = bridge->scn_state.wake_propensity;

        if (pressure > 0.8f || wake_prop < 0.3f) {
            bridge->alertness = HYPO_ALERTNESS_DROWSY;
        } else if (wake_prop > 0.7f && pressure < 0.3f) {
            bridge->alertness = HYPO_ALERTNESS_VIGILANT;
        } else {
            bridge->alertness = HYPO_ALERTNESS_NORMAL;
        }
    }

    return 0;
}

int hypo_sleep_bridge_apply_sleep_effects(hypo_sleep_bridge_t* bridge) {
    if (!bridge || !bridge->drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_sleep_bridge_apply_sleep_effects: bridge or drives is NULL");
        return -1;
    }

    const hypo_sleep_feedback_t* feedback = &bridge->sleep_feedback;
    const hypo_sleep_config_t* cfg = &bridge->config;

    /*
     * Sleep state → Drive modulation:
     *
     * 1. Sleep pressure → Rest drive urgency
     *    High sleep pressure increases fatigue/rest drive
     *
     * 2. Sleep state → Drive suppression
     *    During sleep, most drives are suppressed (except safety)
     *
     * 3. Melatonin → Arousal suppression
     *    High melatonin reduces arousal and activity drives
     */

    /* Map sleep pressure to rest drive */
    float rest_urgency = feedback->sleep_pressure * cfg->pressure_to_drive_weight;
    (void)rest_urgency;  /* Would integrate with fatigue drive */

    /* During sleep, suppress non-essential drives */
    if (feedback->is_asleep && cfg->enable_drive_suppression) {
        float suppression = cfg->drive_suppression_factor;

        /* Safety drive remains active even during sleep */
        nimcp_log(LOG_LEVEL_DEBUG,
            "hypo_sleep_bridge: suppressing drives during sleep (factor=%.2f)",
            suppression);

        /* Alignment note: Safety monitoring continues during sleep */
    }

    /* Melatonin suppresses arousal */
    float melatonin_effect = bridge->scn_state.melatonin_level *
                             cfg->melatonin_arousal_weight;
    (void)melatonin_effect;  /* Would modulate arousal drives */

    /* Alignment safety: Log if sleep deprivation is critical */
    if (feedback->sleep_debt > 0.9f) {
        nimcp_log(LOG_LEVEL_WARN,
            "hypo_sleep_bridge: critical sleep debt (%.2f) - cognitive impairment risk",
            feedback->sleep_debt);
    }

    return 0;
}

int hypo_sleep_bridge_get_sleep_feedback(
    const hypo_sleep_bridge_t* bridge,
    hypo_sleep_feedback_t* feedback)
{
    if (!bridge || !feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_sleep_bridge_get_sleep_feedback: bridge or feedback is NULL");
        return -1;
    }

    *feedback = bridge->sleep_feedback;
    return 0;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

static nimcp_error_t sleep_handle_state_update(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t promise, void* user_data)
{
    (void)promise;
    hypo_sleep_bridge_t* bridge = (hypo_sleep_bridge_t*)user_data;
    if (!bridge || !msg || msg_size < sizeof(bio_message_header_t)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;
    if (msg_size >= sizeof(bio_message_header_t) + sizeof(hypo_sleep_feedback_t)) {
        const hypo_sleep_feedback_t* feedback =
            (const hypo_sleep_feedback_t*)(header + 1);
        hypo_sleep_bridge_update_sleep_state(bridge, feedback);
        hypo_sleep_bridge_apply_sleep_effects(bridge);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t sleep_handle_scn_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t promise, void* user_data)
{
    (void)promise;
    (void)msg;
    (void)msg_size;
    hypo_sleep_bridge_t* bridge = (hypo_sleep_bridge_t*)user_data;
    if (!bridge) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    hypo_sleep_bridge_compute_sleep_propensity(bridge);
    hypo_sleep_bridge_broadcast_scn(bridge);

    return NIMCP_SUCCESS;
}

bool hypo_sleep_bridge_register_bio(hypo_sleep_bridge_t* bridge, bool use_kg_wiring) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_sleep_bridge_register_bio: bridge is NULL");
        return false;
    }
    if (bridge->bio_registered) return true;

    (void)use_kg_wiring;  /* Future KG wiring integration */

    bio_module_info_t info = {
        .module_id = HYPO_SLEEP_BRIDGE_MODULE_ID,
        .module_name = "hypothalamus_sleep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->bio_ctx = bio_router_register_module(&info);
    if (!bridge->bio_ctx) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_sleep_bridge: failed to register with bio-router");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_sleep_bridge_register_bio: bridge->bio_ctx is NULL");
        return false;
    }

    /* Register handlers for sleep messages */
    bio_router_register_handler(bridge->bio_ctx,
        BIO_MSG_HYPO_SLEEP_STATE_UPDATE, sleep_handle_state_update);
    bio_router_register_handler(bridge->bio_ctx,
        BIO_MSG_HYPO_SLEEP_SCN_REQUEST, sleep_handle_scn_request);

    bridge->bio_registered = true;
    nimcp_log(LOG_LEVEL_INFO, "hypo_sleep_bridge: registered with bio-router");
    return true;
}

void hypo_sleep_bridge_unregister_bio(hypo_sleep_bridge_t* bridge) {
    if (!bridge || !bridge->bio_registered) return;

    bio_router_unregister_module(bridge->bio_ctx);
    bridge->bio_ctx = NULL;
    bridge->bio_registered = false;
    nimcp_log(LOG_LEVEL_INFO, "hypo_sleep_bridge: unregistered from bio-router");
}

nimcp_error_t hypo_sleep_bridge_broadcast_scn(hypo_sleep_bridge_t* bridge) {
    if (!bridge || !bridge->bio_registered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_sleep_bridge_broadcast_scn: bridge is NULL or not bio_registered");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    struct {
        bio_message_header_t header;
        hypo_scn_output_t scn;
    } msg;

    msg.header.type = BIO_MSG_HYPO_SLEEP_SCN_OUTPUT;
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.source_module = HYPO_SLEEP_BRIDGE_MODULE_ID;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);
    msg.header.sequence_id = 0;
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;

    msg.scn = bridge->scn_state;

    return bio_router_broadcast(bridge->bio_ctx, &msg, sizeof(msg));
}
