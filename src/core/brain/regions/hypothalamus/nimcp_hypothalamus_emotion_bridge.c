/**
 * @file nimcp_hypothalamus_emotion_bridge.c
 * @brief Implementation of Hypothalamus-Emotion Bridge for HPA Axis Stress Response
 *
 * WHAT: Bidirectional integration between hypothalamus HPA axis and emotional system
 * WHY:  Emotions drive stress response; HPA output modulates emotional processing
 * HOW:  Emotional arousal → CRH release; cortisol → emotional dampening
 *
 * @version Phase 16: Additional Module Bridges
 * @date 2026-01-04
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_emotion_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_emotion_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HYPOTHALAMUS_EMOTION_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct hypo_emotion_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    hypo_drive_system_handle_t* drives;
    hypo_emotion_config_t config;

    /* Emotional input state */
    hypo_emotion_input_t emotion_input;

    /* HPA axis output state */
    hypo_hpa_output_t hpa_output;

    /* Timing for chronic stress detection */
    uint64_t stress_onset_time_ms;
    uint64_t last_update_time_ms;
    bool stress_active;

    /* Bio-async registration */
    bool bio_registered;
    bio_module_context_t bio_ctx;
};

static uint64_t get_current_time_ms(void) {
    /* Placeholder - would use actual time source */
    static uint64_t mock_time = 0;
    return mock_time += 100;  /* Increment 100ms per call */
}

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

void hypo_emotion_bridge_default_config(hypo_emotion_config_t* config) {
    if (!config) return;

    config->fear_crh_weight = 0.9f;
    config->anger_snc_weight = 0.7f;
    config->joy_suppression = 0.6f;
    config->arousal_amplification = 0.8f;
    config->cortisol_emotion_dampening = 0.5f;
    config->chronic_threshold = 2.0f;  /* 2 hours to chronic */
    config->recovery_rate = 0.1f;
    config->enable_emotional_dampening = true;
}

hypo_emotion_bridge_t* hypo_emotion_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_emotion_config_t* config)
{
    if (!drives) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_emotion_bridge_create: drives is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "drives is NULL");

        return NULL;
    }

    hypo_emotion_bridge_t* bridge = nimcp_calloc(1, sizeof(hypo_emotion_bridge_t));
    if (!bridge) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_emotion_bridge_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    bridge->drives = drives;

    if (config) {
        bridge->config = *config;
    } else {
        hypo_emotion_bridge_default_config(&bridge->config);
    }

    /* Initialize neutral emotional state */
    memset(&bridge->emotion_input, 0, sizeof(hypo_emotion_input_t));
    bridge->emotion_input.valence = 0.0f;
    bridge->emotion_input.arousal = 0.0f;
    bridge->emotion_input.dominance = 0.5f;
    bridge->emotion_input.primary_emotion = HYPO_EMO_NEUTRAL;
    bridge->emotion_input.emotional_regulation = 0.5f;

    /* Initialize baseline HPA state */
    memset(&bridge->hpa_output, 0, sizeof(hypo_hpa_output_t));
    bridge->hpa_output.response_state = HYPO_HPA_BASELINE;
    bridge->hpa_output.stress_resilience = 0.5f;

    bridge->stress_onset_time_ms = 0;
    bridge->last_update_time_ms = get_current_time_ms();
    bridge->stress_active = false;

    bridge->bio_registered = false;
    bridge->bio_ctx = NULL;

    /* Initialize bridge base (creates mutex) */
    bridge_base_init(&bridge->base, 0, "hypothalamus_emotion");

    nimcp_log(LOG_LEVEL_INFO, "hypo_emotion_bridge: created successfully");
    return bridge;
}

void hypo_emotion_bridge_destroy(hypo_emotion_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hypothalamus_emotion");

    if (bridge->bio_registered) {
        hypo_emotion_bridge_unregister_bio(bridge);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    nimcp_log(LOG_LEVEL_INFO, "hypo_emotion_bridge: destroyed");
}

/*=============================================================================
 * EMOTION → HPA AXIS
 *===========================================================================*/

int hypo_emotion_bridge_update_emotion(
    hypo_emotion_bridge_t* bridge,
    const hypo_emotion_input_t* emotion)
{
    if (!bridge || !emotion) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_emotion_bridge_update_emotion: bridge or emotion is NULL");
        return -1;
    }

    bridge->emotion_input = *emotion;
    bridge->last_update_time_ms = get_current_time_ms();

    return 0;
}

int hypo_emotion_bridge_process_hpa_response(hypo_emotion_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    const hypo_emotion_input_t* emo = &bridge->emotion_input;
    const hypo_emotion_config_t* cfg = &bridge->config;
    hypo_hpa_output_t* hpa = &bridge->hpa_output;

    uint64_t current_time = get_current_time_ms();

    /*
     * EMOTION → HPA RESPONSE MAPPING:
     *
     * 1. Fear/Anxiety: Strong CRH release
     *    - High arousal + negative valence + fear = acute stress
     *
     * 2. Anger/Frustration: Moderate CRH + SNS activation
     *    - Moderate arousal + negative valence + anger
     *
     * 3. Sadness/Despair: Chronic HPA pattern
     *    - Low arousal + negative valence + sadness
     *
     * 4. Joy/Excitement: HPA suppression
     *    - Positive valence + joy = cortisol reduction
     */

    /* Compute CRH release based on emotional state */
    float crh_drive = 0.0f;

    /* Fear is the strongest HPA activator */
    crh_drive += emo->emotion_intensities[HYPO_EMO_FEAR] * cfg->fear_crh_weight;

    /* Anger contributes moderately */
    crh_drive += emo->emotion_intensities[HYPO_EMO_ANGER] * cfg->anger_snc_weight * 0.7f;

    /* Sadness has a chronic pattern - lower acute but persistent */
    crh_drive += emo->emotion_intensities[HYPO_EMO_SADNESS] * 0.4f;

    /* Surprise causes transient activation */
    crh_drive += emo->emotion_intensities[HYPO_EMO_SURPRISE] * 0.3f;

    /* Joy suppresses HPA */
    float joy_effect = emo->emotion_intensities[HYPO_EMO_JOY] * cfg->joy_suppression;
    crh_drive = crh_drive * (1.0f - joy_effect);

    /* Arousal amplifies the response */
    crh_drive *= (1.0f + emo->arousal * cfg->arousal_amplification);

    /* Emotional regulation dampens the response */
    crh_drive *= (1.0f - emo->emotional_regulation * 0.5f);

    /* Shadow emotions (maladaptive) intensify the response */
    if (emo->shadow_active) {
        crh_drive *= 1.3f;
    }

    crh_drive = nimcp_clamp01(crh_drive);

    /* Update HPA cascade: CRH → ACTH → Cortisol (with delays modeled) */
    /* CRH responds quickly */
    hpa->crh_level = 0.7f * hpa->crh_level + 0.3f * crh_drive;

    /* ACTH follows CRH with lag */
    hpa->acth_level = 0.85f * hpa->acth_level + 0.15f * hpa->crh_level;

    /* Cortisol follows ACTH with longer lag */
    hpa->cortisol_level = 0.9f * hpa->cortisol_level + 0.1f * hpa->acth_level;

    /* Accumulate cortisol for chronic stress tracking */
    hpa->cortisol_accumulated = nimcp_clamp01(
        hpa->cortisol_accumulated + hpa->cortisol_level * 0.01f -
        (1.0f - hpa->cortisol_level) * 0.005f
    );

    /* Determine response state */
    bool acute_trigger = crh_drive > 0.5f;
    bool was_stressed = bridge->stress_active;

    if (acute_trigger && !was_stressed) {
        /* Entering stress state */
        bridge->stress_onset_time_ms = current_time;
        bridge->stress_active = true;
        hpa->response_state = HYPO_HPA_ACUTE_STRESS;
    } else if (bridge->stress_active) {
        /* Check for chronic transition */
        float hours_stressed = (float)(current_time - bridge->stress_onset_time_ms) /
                              (1000.0f * 60.0f * 60.0f);

        if (crh_drive < 0.2f) {
            /* Stress relieved - enter recovery */
            bridge->stress_active = false;
            hpa->response_state = HYPO_HPA_RECOVERY;
            hpa->recovery_progress = 0.0f;
        } else if (hours_stressed > cfg->chronic_threshold) {
            /* Chronic stress develops */
            hpa->response_state = HYPO_HPA_CHRONIC_STRESS;

            /* Check for exhaustion */
            if (hpa->cortisol_accumulated > 0.9f) {
                hpa->response_state = HYPO_HPA_EXHAUSTION;
                nimcp_log(LOG_LEVEL_WARN,
                    "hypo_emotion_bridge: HPA axis exhaustion detected");
            }
        }
    } else if (hpa->response_state == HYPO_HPA_RECOVERY) {
        /* Recovery progresses */
        hpa->recovery_progress += cfg->recovery_rate;
        hpa->cortisol_accumulated *= 0.99f;  /* Slow clearance */

        if (hpa->recovery_progress >= 1.0f) {
            hpa->response_state = HYPO_HPA_BASELINE;
            hpa->recovery_progress = 1.0f;

            /* Build resilience through recovery */
            hpa->stress_resilience = nimcp_clamp01(hpa->stress_resilience + 0.02f);
        }
    } else if (crh_drive > 0.2f) {
        /* Mild alertness */
        hpa->response_state = HYPO_HPA_ALERT;
    } else {
        hpa->response_state = HYPO_HPA_BASELINE;
    }

    /* Alignment safety: Log concerning states */
    if (hpa->response_state == HYPO_HPA_CHRONIC_STRESS ||
        hpa->response_state == HYPO_HPA_EXHAUSTION) {
        nimcp_log(LOG_LEVEL_WARN,
            "hypo_emotion_bridge: concerning HPA state (%d), cortisol_acc=%.2f",
            hpa->response_state, hpa->cortisol_accumulated);
    }

    return 0;
}

int hypo_emotion_bridge_get_hpa_output(
    const hypo_emotion_bridge_t* bridge,
    hypo_hpa_output_t* output)
{
    if (!bridge || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_emotion_bridge_get_hpa_output: bridge or output is NULL");
        return -1;
    }

    *output = bridge->hpa_output;
    return 0;
}

/*=============================================================================
 * HPA AXIS → EMOTIONAL MODULATION
 *===========================================================================*/

int hypo_emotion_bridge_compute_emotional_modulation(
    hypo_emotion_bridge_t* bridge,
    float* dampening_factor)
{
    if (!bridge || !dampening_factor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_emotion_bridge_compute_emotional_modulation: bridge or dampening_factor is NULL");
        return -1;
    }

    if (!bridge->config.enable_emotional_dampening) {
        *dampening_factor = 1.0f;  /* No dampening */
        return 0;
    }

    const hypo_hpa_output_t* hpa = &bridge->hpa_output;

    /*
     * Cortisol effects on emotional processing:
     *
     * Acute cortisol: Enhances emotional memory consolidation
     * Chronic cortisol: Blunts emotional responsivity (anhedonia)
     *
     * The dampening factor represents how much to attenuate
     * emotional signals (1.0 = no change, 0.5 = 50% reduction)
     */

    float dampening = 1.0f;

    /* Chronic stress leads to emotional blunting */
    if (hpa->response_state == HYPO_HPA_CHRONIC_STRESS ||
        hpa->response_state == HYPO_HPA_EXHAUSTION) {
        float chronic_effect = hpa->cortisol_accumulated *
                               bridge->config.cortisol_emotion_dampening;
        dampening = 1.0f - chronic_effect;
    }

    /* Acute stress can briefly enhance emotional salience */
    if (hpa->response_state == HYPO_HPA_ACUTE_STRESS) {
        dampening = 1.0f + hpa->cortisol_level * 0.2f;
    }

    *dampening_factor = nimcp_clamp01(dampening);
    return 0;
}

int hypo_emotion_bridge_get_emotion_input(
    const hypo_emotion_bridge_t* bridge,
    hypo_emotion_input_t* emotion)
{
    if (!bridge || !emotion) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_emotion_bridge_get_emotion_input: bridge or emotion is NULL");
        return -1;
    }

    *emotion = bridge->emotion_input;
    return 0;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

static nimcp_error_t emotion_handle_update(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t promise, void* user_data)
{
    (void)promise;
    hypo_emotion_bridge_t* bridge = (hypo_emotion_bridge_t*)user_data;
    if (!bridge || !msg || msg_size < sizeof(bio_message_header_t)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;
    if (msg_size >= sizeof(bio_message_header_t) + sizeof(hypo_emotion_input_t)) {
        const hypo_emotion_input_t* emotion =
            (const hypo_emotion_input_t*)(header + 1);
        hypo_emotion_bridge_update_emotion(bridge, emotion);
        hypo_emotion_bridge_process_hpa_response(bridge);
        hypo_emotion_bridge_broadcast_hpa(bridge);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t emotion_handle_hpa_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t promise, void* user_data)
{
    (void)promise;
    (void)msg;
    (void)msg_size;
    hypo_emotion_bridge_t* bridge = (hypo_emotion_bridge_t*)user_data;
    if (!bridge) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    hypo_emotion_bridge_broadcast_hpa(bridge);

    return NIMCP_SUCCESS;
}

bool hypo_emotion_bridge_register_bio(hypo_emotion_bridge_t* bridge, bool use_kg_wiring) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_emotion_bridge_register_bio: bridge is NULL");
        return false;
    }
    if (bridge->bio_registered) return true;

    (void)use_kg_wiring;  /* Future KG wiring integration */

    bio_module_info_t info = {
        .module_id = HYPO_EMOTION_BRIDGE_MODULE_ID,
        .module_name = "hypothalamus_emotion_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->bio_ctx = bio_router_register_module(&info);
    if (!bridge->bio_ctx) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_emotion_bridge: failed to register with bio-router");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_emotion_bridge_register_bio: bridge->bio_ctx is NULL");
        return false;
    }

    /* Register handlers for emotion messages */
    bio_router_register_handler(bridge->bio_ctx,
        BIO_MSG_HYPO_EMOTION_UPDATE, emotion_handle_update);
    bio_router_register_handler(bridge->bio_ctx,
        BIO_MSG_HYPO_EMOTION_HPA_REQUEST, emotion_handle_hpa_request);

    bridge->bio_registered = true;
    nimcp_log(LOG_LEVEL_INFO, "hypo_emotion_bridge: registered with bio-router");
    return true;
}

void hypo_emotion_bridge_unregister_bio(hypo_emotion_bridge_t* bridge) {
    if (!bridge || !bridge->bio_registered) return;

    bio_router_unregister_module(bridge->bio_ctx);
    bridge->bio_ctx = NULL;
    bridge->bio_registered = false;
    nimcp_log(LOG_LEVEL_INFO, "hypo_emotion_bridge: unregistered from bio-router");
}

nimcp_error_t hypo_emotion_bridge_broadcast_hpa(hypo_emotion_bridge_t* bridge) {
    if (!bridge || !bridge->bio_registered) return NIMCP_ERROR_INVALID_PARAM;

    struct {
        bio_message_header_t header;
        hypo_hpa_output_t hpa;
    } msg;

    msg.header.type = BIO_MSG_HYPO_EMOTION_HPA_OUTPUT;
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.source_module = HYPO_EMOTION_BRIDGE_MODULE_ID;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);
    msg.header.sequence_id = 0;
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;

    msg.hpa = bridge->hpa_output;

    return bio_router_broadcast(bridge->bio_ctx, &msg, sizeof(msg));
}
