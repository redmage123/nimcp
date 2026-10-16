/**
 * @file nimcp_hypothalamus_broca_bridge.c
 * @brief Implementation of Hypothalamus-Broca Bridge for Speech Modulation
 *
 * WHAT: Integration between hypothalamus drives/stress and Broca's speech production
 * WHY:  Stress and arousal significantly affect speech - stuttering, rate, volume
 * HOW:  HPA cortisol → fluency; arousal → rate/volume; social drive → initiation
 *
 * @version Phase 17: Perception & Speech Modulation
 * @date 2026-01-04
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_broca_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_broca_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HYPOTHALAMUS_BROCA_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct hypo_broca_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    hypo_drive_system_handle_t* drives;
    hypo_broca_config_t config;

    /* Current modulation output */
    hypo_speech_modulation_t modulation;

    /* Stress state */
    hypo_stress_input_t stress;

    /* Arousal from external source */
    float arousal;

    /* Bio-async registration */
    bool bio_registered;
    bio_module_context_t bio_ctx;
};

/**
 * @brief Compute fluency based on Yerkes-Dodson inverted-U
 *
 * Optimal cortisol/stress produces best performance.
 * Too low or too high impairs fluency.
 */
static float compute_fluency_curve(float cortisol, float optimal) {
    /* Distance from optimal */
    float distance = fabsf(cortisol - optimal);

    /* Inverted-U curve: fluency = 1 - distance^2 (normalized) */
    float max_distance = (optimal > 0.5f) ? optimal : (1.0f - optimal);
    float normalized_dist = (max_distance > 0.0f) ? (distance / max_distance) : 0.0f;

    float fluency = 1.0f - (normalized_dist * normalized_dist);
    return nimcp_clamp01(fluency);
}

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

void hypo_broca_bridge_default_config(hypo_broca_config_t* config) {
    if (!config) return;

    config->optimal_cortisol = 0.3f;
    config->stress_fluency_weight = 0.6f;
    config->chronic_impairment = 0.4f;
    config->arousal_rate_weight = 0.5f;
    config->arousal_volume_weight = 0.3f;
    config->social_initiation_weight = 0.7f;
    config->alarm_threshold = 0.8f;
    config->enable_alarm_vocalization = true;
}

hypo_broca_bridge_t* hypo_broca_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_broca_config_t* config)
{
    if (!drives) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_broca_bridge_create: drives is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "drives is NULL");

        return NULL;
    }

    hypo_broca_bridge_t* bridge = nimcp_calloc(1, sizeof(hypo_broca_bridge_t));
    if (!bridge) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_broca_bridge_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    bridge->drives = drives;

    if (config) {
        bridge->config = *config;
    } else {
        hypo_broca_bridge_default_config(&bridge->config);
    }

    /* Initialize modulation to normal speech */
    bridge->modulation.state = HYPO_SPEECH_NORMAL;
    bridge->modulation.initiation_mode = HYPO_INIT_NORMAL;
    bridge->modulation.rate_multiplier = 1.0f;
    bridge->modulation.volume_multiplier = 1.0f;
    bridge->modulation.fluency_level = 1.0f;
    bridge->modulation.hesitation_probability = 0.05f;
    bridge->modulation.word_finding_delay = 0.0f;
    bridge->modulation.prosody_variation = 0.5f;
    bridge->modulation.pitch_baseline = 0.0f;
    bridge->modulation.initiation_threshold = 0.5f;
    bridge->modulation.urgency_to_speak = 0.5f;
    bridge->modulation.alarm_mode = false;
    bridge->modulation.alarm_intensity = 0.0f;

    /* Initialize stress to baseline */
    memset(&bridge->stress, 0, sizeof(hypo_stress_input_t));
    bridge->stress.cortisol_level = 0.2f;  /* Low baseline */

    bridge->arousal = 0.5f;

    bridge->bio_registered = false;
    bridge->bio_ctx = NULL;

    /* Initialize bridge base (creates mutex) */
    bridge_base_init(&bridge->base, 0, "hypothalamus_broca");

    nimcp_log(LOG_LEVEL_INFO, "hypo_broca_bridge: created successfully");
    return bridge;
}

void hypo_broca_bridge_destroy(hypo_broca_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hypothalamus_broca");

    if (bridge->bio_registered) {
        hypo_broca_bridge_unregister_bio(bridge);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    nimcp_log(LOG_LEVEL_INFO, "hypo_broca_bridge: destroyed");
}

/*=============================================================================
 * HYPOTHALAMUS → SPEECH MODULATION
 *===========================================================================*/

int hypo_broca_bridge_update_stress(
    hypo_broca_bridge_t* bridge,
    const hypo_stress_input_t* stress)
{
    if (!bridge || !stress) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_broca_bridge_update_stress: required parameter is NULL (bridge, stress)");
        return -1;
    }

    bridge->stress = *stress;
    return 0;
}

int hypo_broca_bridge_set_arousal(hypo_broca_bridge_t* bridge, float arousal) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->arousal = nimcp_clamp01(arousal);
    return 0;
}

int hypo_broca_bridge_compute_modulation(hypo_broca_bridge_t* bridge) {
    if (!bridge || !bridge->drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_broca_bridge_compute_modulation: required parameter is NULL (bridge, bridge->drives)");
        return -1;
    }

    const hypo_broca_config_t* cfg = &bridge->config;
    hypo_speech_modulation_t* mod = &bridge->modulation;
    const hypo_stress_input_t* stress = &bridge->stress;

    /* Get current drive states */
    hypo_drive_system_t drive_state;
    if (!hypo_drive_get_system_state(bridge->drives, &drive_state)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_broca_bridge_compute_modulation: hypo_drive_get_system_state is NULL");
        return -1;
    }

    /*
     * STRESS → FLUENCY (Yerkes-Dodson):
     *
     * Optimal cortisol = best fluency
     * Too low = sluggish speech
     * Too high = impaired, stuttering
     */
    float base_fluency = compute_fluency_curve(
        stress->cortisol_level, cfg->optimal_cortisol);

    /* Chronic stress adds additional impairment */
    if (stress->chronic_stress_active) {
        base_fluency *= (1.0f - stress->cortisol_chronic * cfg->chronic_impairment);
    }

    mod->fluency_level = nimcp_clamp01(base_fluency);

    /* Hesitation probability inverse of fluency */
    mod->hesitation_probability = nimcp_clamp01(0.05f + (1.0f - mod->fluency_level) * 0.3f);

    /* Word-finding delay from chronic stress */
    mod->word_finding_delay = stress->cortisol_chronic * cfg->chronic_impairment;

    /*
     * AROUSAL → RATE AND VOLUME:
     *
     * High arousal = faster, louder
     * Low arousal = slower, quieter
     */
    float arousal = bridge->arousal;

    /* Rate: 0.7 at low arousal, 1.5 at high arousal, 1.0 at 0.5 */
    mod->rate_multiplier = 0.7f + arousal * cfg->arousal_rate_weight * 1.6f;
    mod->rate_multiplier = nimcp_clampf(mod->rate_multiplier, 0.5f, 2.0f);

    /* Volume similar mapping */
    mod->volume_multiplier = 0.7f + arousal * cfg->arousal_volume_weight * 2.0f;
    mod->volume_multiplier = nimcp_clampf(mod->volume_multiplier, 0.5f, 2.0f);

    /* Prosody variation decreases with extreme stress */
    if (stress->acute_stress > 0.7f) {
        mod->prosody_variation = nimcp_clamp01(0.5f - (stress->acute_stress - 0.7f));
    } else {
        mod->prosody_variation = 0.5f + arousal * 0.3f;
    }

    /* Pitch rises with stress */
    mod->pitch_baseline = (stress->acute_stress - 0.3f) * 0.5f;
    mod->pitch_baseline = nimcp_clampf(mod->pitch_baseline, -0.3f, 0.5f);

    /*
     * SOCIAL DRIVE → INITIATION:
     *
     * High social drive = eager to speak
     * Low social drive = reluctant
     */
    float social_urgency = drive_state.drives[HYPO_DRIVE_SOCIAL].urgency;

    mod->initiation_threshold = 0.5f - (social_urgency * cfg->social_initiation_weight * 0.4f);
    mod->initiation_threshold = nimcp_clampf(mod->initiation_threshold, 0.1f, 0.9f);

    mod->urgency_to_speak = social_urgency * cfg->social_initiation_weight;

    /* Determine initiation mode */
    if (social_urgency > 0.7f) {
        mod->initiation_mode = HYPO_INIT_EAGER;
    } else if (social_urgency < 0.2f && stress->acute_stress > 0.5f) {
        mod->initiation_mode = HYPO_INIT_AVOIDANT;
    } else if (social_urgency < 0.3f) {
        mod->initiation_mode = HYPO_INIT_RELUCTANT;
    } else {
        mod->initiation_mode = HYPO_INIT_NORMAL;
    }

    /*
     * SAFETY → ALARM VOCALIZATION:
     *
     * Extreme threat triggers alarm vocalizations (screaming, calling for help)
     */
    float safety_urgency = drive_state.drives[HYPO_DRIVE_SAFETY].urgency;

    if (cfg->enable_alarm_vocalization && safety_urgency > cfg->alarm_threshold) {
        mod->alarm_mode = true;
        float denom = 1.0f - cfg->alarm_threshold;
        mod->alarm_intensity = (denom > 1e-6f) ?
            (safety_urgency - cfg->alarm_threshold) / denom : 1.0f;
        mod->initiation_mode = HYPO_INIT_COMPULSIVE;
        mod->volume_multiplier = 2.0f;  /* Max volume for alarm */

        nimcp_log(LOG_LEVEL_WARN,
            "hypo_broca_bridge: alarm vocalization active (safety=%.2f)",
            safety_urgency);
    } else {
        mod->alarm_mode = false;
        mod->alarm_intensity = 0.0f;
    }

    /*
     * DETERMINE OVERALL SPEECH STATE:
     */
    if (mod->alarm_mode) {
        mod->state = HYPO_SPEECH_EMERGENCY;
    } else if (mod->fluency_level < 0.3f) {
        mod->state = HYPO_SPEECH_BLOCKED;
    } else if (mod->fluency_level < 0.5f) {
        mod->state = HYPO_SPEECH_IMPAIRED;
    } else if (mod->fluency_level < 0.7f) {
        mod->state = HYPO_SPEECH_STRESSED;
    } else if (mod->fluency_level > 0.9f && arousal > 0.3f && arousal < 0.7f) {
        mod->state = HYPO_SPEECH_ENHANCED;
    } else {
        mod->state = HYPO_SPEECH_NORMAL;
    }

    /* Alignment safety: Log concerning states */
    if (mod->state == HYPO_SPEECH_BLOCKED || mod->state == HYPO_SPEECH_IMPAIRED) {
        nimcp_log(LOG_LEVEL_WARN,
            "hypo_broca_bridge: speech impairment detected (fluency=%.2f, state=%d)",
            mod->fluency_level, mod->state);
    }

    mod->timestamp_us = nimcp_time_get_us();

    return 0;
}

int hypo_broca_bridge_get_modulation(
    const hypo_broca_bridge_t* bridge,
    hypo_speech_modulation_t* modulation)
{
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_broca_bridge_get_modulation: required parameter is NULL (bridge, modulation)");
        return -1;
    }

    *modulation = bridge->modulation;
    return 0;
}

/*=============================================================================
 * SPEECH STATE QUERIES
 *===========================================================================*/

hypo_speech_state_t hypo_broca_bridge_get_state(const hypo_broca_bridge_t* bridge) {
    if (!bridge) return HYPO_SPEECH_NORMAL;
    return bridge->modulation.state;
}

bool hypo_broca_bridge_is_impaired(const hypo_broca_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->modulation.state == HYPO_SPEECH_IMPAIRED ||
           bridge->modulation.state == HYPO_SPEECH_BLOCKED;
}

bool hypo_broca_bridge_is_alarm_active(const hypo_broca_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->modulation.alarm_mode;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

static nimcp_error_t broca_handle_stress_update(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t promise, void* user_data)
{
    (void)promise;
    hypo_broca_bridge_t* bridge = (hypo_broca_bridge_t*)user_data;
    if (!bridge || !msg || msg_size < sizeof(bio_message_header_t)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;
    if (msg_size >= sizeof(bio_message_header_t) + sizeof(hypo_stress_input_t)) {
        const hypo_stress_input_t* stress =
            (const hypo_stress_input_t*)(header + 1);
        hypo_broca_bridge_update_stress(bridge, stress);
        hypo_broca_bridge_compute_modulation(bridge);
        hypo_broca_bridge_broadcast_modulation(bridge);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t broca_handle_arousal_update(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t promise, void* user_data)
{
    (void)promise;
    hypo_broca_bridge_t* bridge = (hypo_broca_bridge_t*)user_data;
    if (!bridge || !msg || msg_size < sizeof(bio_message_header_t)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;
    if (msg_size >= sizeof(bio_message_header_t) + sizeof(float)) {
        const float* arousal = (const float*)(header + 1);
        hypo_broca_bridge_set_arousal(bridge, *arousal);
        hypo_broca_bridge_compute_modulation(bridge);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t broca_handle_modulation_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t promise, void* user_data)
{
    (void)promise;
    (void)msg;
    (void)msg_size;
    hypo_broca_bridge_t* bridge = (hypo_broca_bridge_t*)user_data;
    if (!bridge) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    hypo_broca_bridge_compute_modulation(bridge);
    hypo_broca_bridge_broadcast_modulation(bridge);

    return NIMCP_SUCCESS;
}

bool hypo_broca_bridge_register_bio(hypo_broca_bridge_t* bridge, bool use_kg_wiring) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_broca_bridge_register_bio: bridge is NULL");
        return false;
    }
    if (bridge->bio_registered) return true;

    (void)use_kg_wiring;  /* Future KG wiring integration */

    bio_module_info_t info = {
        .module_id = HYPO_BROCA_BRIDGE_MODULE_ID,
        .module_name = "hypothalamus_broca_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->bio_ctx = bio_router_register_module(&info);
    if (!bridge->bio_ctx) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_broca_bridge: failed to register with bio-router");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_broca_bridge_register_bio: bridge->bio_ctx is NULL");
        return false;
    }

    /* Register handlers for speech messages */
    bio_router_register_handler(bridge->bio_ctx,
        BIO_MSG_HYPO_BROCA_STRESS_UPDATE, broca_handle_stress_update);
    bio_router_register_handler(bridge->bio_ctx,
        BIO_MSG_HYPO_BROCA_AROUSAL_UPDATE, broca_handle_arousal_update);
    bio_router_register_handler(bridge->bio_ctx,
        BIO_MSG_HYPO_BROCA_MODULATION_REQUEST, broca_handle_modulation_request);

    bridge->bio_registered = true;
    nimcp_log(LOG_LEVEL_INFO, "hypo_broca_bridge: registered with bio-router");
    return true;
}

void hypo_broca_bridge_unregister_bio(hypo_broca_bridge_t* bridge) {
    if (!bridge || !bridge->bio_registered) return;

    bio_router_unregister_module(bridge->bio_ctx);
    bridge->bio_ctx = NULL;
    bridge->bio_registered = false;
    nimcp_log(LOG_LEVEL_INFO, "hypo_broca_bridge: unregistered from bio-router");
}

nimcp_error_t hypo_broca_bridge_broadcast_modulation(hypo_broca_bridge_t* bridge) {
    if (!bridge || !bridge->bio_registered) return NIMCP_ERROR_INVALID_PARAM;

    struct {
        bio_message_header_t header;
        hypo_speech_modulation_t modulation;
    } msg;

    msg.header.type = BIO_MSG_HYPO_BROCA_MODULATION_OUTPUT;
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.source_module = HYPO_BROCA_BRIDGE_MODULE_ID;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);
    msg.header.sequence_id = 0;
    msg.header.channel = BIO_CHANNEL_ACETYLCHOLINE;  /* Motor/speech channel */

    /* Add urgent flag if alarm mode */
    if (bridge->modulation.alarm_mode) {
        msg.header.flags |= BIO_MSG_FLAG_URGENT;
    }

    msg.modulation = bridge->modulation;

    return bio_router_broadcast(bridge->bio_ctx, &msg, sizeof(msg));
}
