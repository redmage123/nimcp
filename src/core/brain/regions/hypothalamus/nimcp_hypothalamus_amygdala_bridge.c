/**
 * @file nimcp_hypothalamus_amygdala_bridge.c
 * @brief Implementation of Hypothalamus <-> Amygdala Bridge
 *
 * @version Phase 12: Cognitive Layer Integration
 * @date 2026-01-04
 */

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_amygdala_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_amygdala_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HYPOTHALAMUS_AMYGDALA_BRIDGE"

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

hypo_amyg_bridge_config_t hypo_amyg_bridge_default_config(void) {
    hypo_amyg_bridge_config_t config = {0};

    /* Stress modulation */
    config.stress_scale = HYPO_AMYG_STRESS_SCALE;
    config.cortisol_sensitivity = 0.5f;
    config.crh_sensitivity = 0.6f;

    /* Fear-to-drive mapping */
    config.fear_safety_scale = HYPO_AMYG_FEAR_SAFETY_SCALE;
    config.anxiety_stress_scale = 0.4f;
    config.threat_boost_factor = HYPO_AMYG_THREAT_BOOST;

    /* Drive modulation by fear */
    config.enable_fear_drive_modulation = true;
    config.fear_feeding_inhibition = HYPO_AMYG_FEAR_FEEDING_INHIBIT;
    config.fear_curiosity_inhibition = 0.4f;
    config.fear_social_modulation = -0.2f; /* Fear can increase social seeking */

    /* Thresholds */
    config.anxiety_chronic_threshold = HYPO_AMYG_ANXIETY_THRESHOLD;
    config.fear_acute_threshold = 0.7f;
    config.threat_response_threshold = 0.6f;

    /* Bio-async */
    config.broadcast_enabled = true;

    return config;
}

hypo_amyg_bridge_t* hypo_amyg_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_amyg_bridge_config_t* config) {

    if (!drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_amyg_bridge_create: drives is NULL");
        return NULL;
    }

    hypo_amyg_bridge_t* bridge = nimcp_calloc(1, sizeof(hypo_amyg_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_amyg_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = hypo_amyg_bridge_default_config();
    }

    /* Store drives reference */
    bridge->drives = drives;
    bridge->amygdala = NULL;

    /* Initialize state */
    memset(&bridge->current_stress, 0, sizeof(bridge->current_stress));
    memset(&bridge->current_drive_context, 0, sizeof(bridge->current_drive_context));
    memset(&bridge->current_fear, 0, sizeof(bridge->current_fear));
    memset(&bridge->last_threat, 0, sizeof(bridge->last_threat));
    memset(&bridge->current_fear_output, 0, sizeof(bridge->current_fear_output));

    /* Initialize integrated state */
    bridge->integrated_stress_level = 0.0f;
    bridge->chronic_stress_accumulator = 0.0f;
    bridge->in_threat_response = false;

    /* Initialize fear-drive modulation */
    memset(bridge->fear_drive_modulation, 0, sizeof(bridge->fear_drive_modulation));

    /* Initialize timing */
    bridge->last_update_us = nimcp_time_now_us();
    bridge->last_threat_us = 0;
    bridge->chronic_stress_start_us = 0;

    /* Initialize bio context */
    memset(&bridge->bio_ctx, 0, sizeof(bridge->bio_ctx));

    /* Initialize statistics */
    bridge->stress_signals_sent = 0;
    bridge->fear_updates_received = 0;
    bridge->threat_events_processed = 0;
    bridge->chronic_stress_episodes = 0;
    bridge->safety_drive_boosts = 0;

    /* Initialize bridge base (creates mutex) */
    bridge_base_init(&bridge->base, 0, "hypothalamus_amygdala");

    NIMCP_LOG_INFO("Hypothalamus-Amygdala bridge created");
    return bridge;
}

void hypo_amyg_bridge_destroy(hypo_amyg_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hypothalamus_amygdala");

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOG_INFO("Hypothalamus-Amygdala bridge destroyed");
}

void hypo_amyg_bridge_reset(hypo_amyg_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    /* Reset state */
    memset(&bridge->current_stress, 0, sizeof(bridge->current_stress));
    memset(&bridge->current_drive_context, 0, sizeof(bridge->current_drive_context));
    memset(&bridge->current_fear, 0, sizeof(bridge->current_fear));
    memset(&bridge->last_threat, 0, sizeof(bridge->last_threat));
    memset(&bridge->current_fear_output, 0, sizeof(bridge->current_fear_output));

    /* Reset integrated state */
    bridge->integrated_stress_level = 0.0f;
    bridge->chronic_stress_accumulator = 0.0f;
    bridge->in_threat_response = false;

    /* Reset fear-drive modulation */
    memset(bridge->fear_drive_modulation, 0, sizeof(bridge->fear_drive_modulation));

    /* Reset timing */
    bridge->last_update_us = nimcp_time_now_us();
    bridge->last_threat_us = 0;
    bridge->chronic_stress_start_us = 0;

    /* Reset statistics */
    bridge->stress_signals_sent = 0;
    bridge->fear_updates_received = 0;
    bridge->threat_events_processed = 0;
    bridge->chronic_stress_episodes = 0;
    bridge->safety_drive_boosts = 0;

    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_DEBUG("Hypothalamus-Amygdala bridge reset");
}

/*=============================================================================
 * CORE FUNCTIONS
 *===========================================================================*/

int hypo_amyg_bridge_update(hypo_amyg_bridge_t* bridge, float dt_ms) {
    if (!bridge || !bridge->drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_amyg_bridge_update: required parameter is NULL (bridge, bridge->drives)");
        return -1;
    }

    uint64_t now_us = nimcp_time_now_us();

    /* Query amygdala OUTSIDE mutex to avoid deadlock with external module */
    hypo_amyg_fear_feedback_t fear = {0};
    bool have_fear = false;
    if (bridge->amygdala) {
        fear.fear_level = amygdala_get_fear_level(bridge->amygdala);
        fear.anxiety_level = amygdala_get_anxiety_level(bridge->amygdala);
        fear.threat_level = amygdala_get_threat_level(bridge->amygdala);
        fear.timestamp_us = now_us;
        have_fear = true;
    }

    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    /* Compute outgoing stress modulation */
    bridge->current_stress = hypo_amyg_bridge_compute_stress(bridge);
    bridge->current_drive_context = hypo_amyg_bridge_compute_drive_context(bridge);

    /* Apply amygdala fear feedback if available */
    if (have_fear) {
        hypo_amyg_bridge_process_fear(bridge, &fear);
    }

    /* Decay fear-drive modulation */
    if (bridge->config.enable_fear_drive_modulation) {
        float decay = expf(-0.05f * dt_ms / 1000.0f);
        for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
            bridge->fear_drive_modulation[i] *= decay;
        }
    }

    /* Update chronic stress accumulator */
    if (bridge->current_fear.anxiety_level > bridge->config.anxiety_chronic_threshold) {
        bridge->chronic_stress_accumulator += dt_ms / 1000.0f * 0.1f;
        bridge->chronic_stress_accumulator = nimcp_clampf(bridge->chronic_stress_accumulator, 0.0f, 1.0f);

        /* Enter chronic stress if threshold exceeded */
        if (bridge->chronic_stress_accumulator > 0.5f && bridge->chronic_stress_start_us == 0) {
            hypo_amyg_bridge_enter_chronic_stress(bridge);
        }
    } else {
        /* Decay chronic stress */
        bridge->chronic_stress_accumulator -= dt_ms / 1000.0f * 0.02f;
        bridge->chronic_stress_accumulator = nimcp_clampf(bridge->chronic_stress_accumulator, 0.0f, 1.0f);

        if (bridge->chronic_stress_accumulator < 0.2f && bridge->chronic_stress_start_us > 0) {
            hypo_amyg_bridge_exit_chronic_stress(bridge);
        }
    }

    /* Check if threat response has timed out */
    if (bridge->in_threat_response) {
        uint64_t response_duration_us = now_us - bridge->last_threat_us;
        if (response_duration_us > 30000000) { /* 30 seconds */
            bridge->in_threat_response = false;
        }
    }

    /* Update integrated stress level */
    bridge->integrated_stress_level = nimcp_clampf(
        0.4f * bridge->current_fear.fear_level +
        0.3f * bridge->current_fear.anxiety_level +
        0.3f * bridge->chronic_stress_accumulator,
        0.0f, 1.0f);

    bridge->last_update_us = now_us;

    /* Snapshot broadcast flag before releasing lock */
    bool should_broadcast = bridge->config.broadcast_enabled;

    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    /* Broadcast OUTSIDE mutex to avoid deadlock with bio-router */
    if (should_broadcast) {
        hypo_amyg_bridge_broadcast_stress(bridge);
        hypo_amyg_bridge_broadcast_drive_context(bridge);
    }

    return 0;
}

hypo_amyg_stress_modulation_t hypo_amyg_bridge_compute_stress(
    hypo_amyg_bridge_t* bridge) {

    hypo_amyg_stress_modulation_t stress = {0};

    if (!bridge || !bridge->drives) return stress;

    /* Get drive urgencies to compute stress */
    float urgencies[HYPO_DRIVE_COUNT];
    if (!hypo_drive_get_urgencies(bridge->drives, urgencies)) {
        return stress;
    }

    /* Compute total physiological stress from survival drives */
    float phys_stress = 0.0f;
    phys_stress += urgencies[HYPO_DRIVE_HUNGER] * 0.3f;
    phys_stress += urgencies[HYPO_DRIVE_THIRST] * 0.3f;
    phys_stress += urgencies[HYPO_DRIVE_TEMPERATURE] * 0.2f;
    phys_stress += urgencies[HYPO_DRIVE_FATIGUE] * 0.2f;
    phys_stress = nimcp_clampf(phys_stress, 0.0f, 1.0f);

    /* Compute psychological stress from psychological drives */
    float psych_stress = 0.0f;
    psych_stress += urgencies[HYPO_DRIVE_SAFETY] * 0.4f;
    psych_stress += urgencies[HYPO_DRIVE_SOCIAL] * 0.3f;
    psych_stress += urgencies[HYPO_DRIVE_AUTONOMY] * 0.3f;
    psych_stress = nimcp_clampf(psych_stress, 0.0f, 1.0f);

    /* CRH level from stress */
    stress.crh_level = nimcp_clampf(
        (phys_stress + psych_stress) * 0.5f * bridge->config.stress_scale,
        0.0f, 1.0f);

    /* Cortisol follows CRH with some lag (simplified) */
    stress.cortisol_level = nimcp_clampf(
        stress.crh_level * 0.8f + bridge->chronic_stress_accumulator * 0.2f,
        0.0f, 1.0f);

    /* Get arousal from drive system */
    hypo_drive_system_t sys_state;
    if (hypo_drive_get_system_state(bridge->drives, &sys_state)) {
        stress.arousal_level = sys_state.arousal_level;
        /* NE correlates with arousal */
        stress.norepinephrine_level = nimcp_clampf(stress.arousal_level * 0.7f, 0.0f, 1.0f);
    }

    /* Chronicity from accumulator */
    stress.stress_chronicity = bridge->chronic_stress_accumulator;

    /* Find primary stressor */
    float max_urgency = 0.0f;
    stress.primary_stressor = HYPO_DRIVE_SAFETY;
    for (int d = 0; d < HYPO_DRIVE_COUNT; d++) {
        if (urgencies[d] > max_urgency) {
            max_urgency = urgencies[d];
            stress.primary_stressor = (hypo_drive_type_t)d;
        }
    }

    stress.timestamp_us = nimcp_time_now_us();

    return stress;
}

hypo_amyg_drive_context_t hypo_amyg_bridge_compute_drive_context(
    hypo_amyg_bridge_t* bridge) {

    hypo_amyg_drive_context_t context = {0};

    if (!bridge || !bridge->drives) return context;

    float urgencies[HYPO_DRIVE_COUNT];
    if (!hypo_drive_get_urgencies(bridge->drives, urgencies)) {
        return context;
    }

    /* Total urgency */
    context.total_drive_urgency = 0.0f;
    for (int d = 0; d < HYPO_DRIVE_COUNT; d++) {
        context.total_drive_urgency += urgencies[d];
    }

    /* Specific drives */
    context.safety_drive_level = urgencies[HYPO_DRIVE_SAFETY];
    context.social_drive_level = urgencies[HYPO_DRIVE_SOCIAL];

    /* Physiological stress (survival drives) */
    context.physiological_stress = nimcp_clampf(
        (urgencies[HYPO_DRIVE_HUNGER] +
         urgencies[HYPO_DRIVE_THIRST] +
         urgencies[HYPO_DRIVE_TEMPERATURE] +
         urgencies[HYPO_DRIVE_FATIGUE]) / 4.0f,
        0.0f, 1.0f);

    /* Psychological stress (psychological drives) */
    context.psychological_stress = nimcp_clampf(
        (urgencies[HYPO_DRIVE_SOCIAL] +
         urgencies[HYPO_DRIVE_CURIOSITY] +
         urgencies[HYPO_DRIVE_SAFETY] +
         urgencies[HYPO_DRIVE_AUTONOMY] +
         urgencies[HYPO_DRIVE_COMPETENCE]) / 5.0f,
        0.0f, 1.0f);

    /* Check for critical drives */
    context.any_drive_critical = false;
    for (int d = 0; d < HYPO_DRIVE_COUNT; d++) {
        if (urgencies[d] > 0.9f) {
            context.any_drive_critical = true;
            break;
        }
    }

    context.timestamp_us = nimcp_time_now_us();

    return context;
}

float hypo_amyg_bridge_get_stress_level(const hypo_amyg_bridge_t* bridge) {
    if (!bridge) return 0.0f;

    /* Cast away const to acquire mutex -- reads must be synchronized */
    hypo_amyg_bridge_t* mutable = (hypo_amyg_bridge_t*)bridge;
    if (mutable->base.mutex) nimcp_mutex_lock(mutable->base.mutex);
    float level = bridge->integrated_stress_level;
    if (mutable->base.mutex) nimcp_mutex_unlock(mutable->base.mutex);
    return level;
}

bool hypo_amyg_bridge_is_chronic_stress(const hypo_amyg_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Cast away const to acquire mutex -- reads must be synchronized */
    hypo_amyg_bridge_t* mutable = (hypo_amyg_bridge_t*)bridge;
    if (mutable->base.mutex) nimcp_mutex_lock(mutable->base.mutex);
    bool result = (bridge->chronic_stress_start_us > 0);
    if (mutable->base.mutex) nimcp_mutex_unlock(mutable->base.mutex);
    return result;
}

/*=============================================================================
 * FEAR FEEDBACK PROCESSING
 *===========================================================================*/

float hypo_amyg_bridge_process_fear(
    hypo_amyg_bridge_t* bridge,
    const hypo_amyg_fear_feedback_t* fear) {

    if (!bridge || !fear) return 0.0f;

    /* Store current fear state */
    bridge->current_fear = *fear;
    bridge->fear_updates_received++;

    float safety_boost = 0.0f;

    /* Fear boosts safety drive */
    if (fear->fear_level > 0.1f) {
        safety_boost = fear->fear_level * bridge->config.fear_safety_scale;

        /* Apply to safety drive via nucleus */
        hypo_drive_set_nucleus_input(bridge->drives, HYPO_NUCLEUS_PARAVENTRICULAR,
                                     safety_boost * 0.5f);
        hypo_drive_set_nucleus_input(bridge->drives, HYPO_NUCLEUS_LATERAL,
                                     safety_boost * 0.3f);

        bridge->safety_drive_boosts++;
    }

    /* Fear-induced drive modulation */
    if (bridge->config.enable_fear_drive_modulation && fear->fear_level > 0.2f) {
        /* Fear suppresses feeding (hunger) */
        bridge->fear_drive_modulation[HYPO_DRIVE_HUNGER] =
            -fear->fear_level * bridge->config.fear_feeding_inhibition;

        /* Fear suppresses exploration (curiosity) */
        bridge->fear_drive_modulation[HYPO_DRIVE_CURIOSITY] =
            -fear->fear_level * bridge->config.fear_curiosity_inhibition;

        /* Fear can increase OR decrease social drive */
        bridge->fear_drive_modulation[HYPO_DRIVE_SOCIAL] =
            fear->fear_level * bridge->config.fear_social_modulation;

        /* Apply modulations via preoptic area (inhibitory) */
        if (bridge->fear_drive_modulation[HYPO_DRIVE_HUNGER] < 0) {
            hypo_drive_set_nucleus_input(bridge->drives, HYPO_NUCLEUS_ARCUATE,
                                        -bridge->fear_drive_modulation[HYPO_DRIVE_HUNGER] * 0.5f);
        }
    }

    return safety_boost;
}

float hypo_amyg_bridge_process_threat(
    hypo_amyg_bridge_t* bridge,
    const hypo_amyg_threat_event_t* threat) {

    if (!bridge || !threat) return 0.0f;

    bridge->last_threat = *threat;
    bridge->threat_events_processed++;

    float stress_response = 0.0f;

    if (threat->threat_intensity > bridge->config.threat_response_threshold) {
        /* Enter threat response mode */
        bridge->in_threat_response = true;
        bridge->last_threat_us = nimcp_time_now_us();

        /* Compute stress response magnitude */
        stress_response = threat->threat_intensity * bridge->config.threat_boost_factor;
        stress_response = nimcp_clampf(stress_response, 0.0f, 1.0f);

        /* Trigger acute stress via hypothalamus */
        hypo_amyg_bridge_trigger_acute_stress(bridge, stress_response);

        NIMCP_LOG_DEBUG("Threat response triggered: intensity=%.2f, response=%.2f",
                       threat->threat_intensity, stress_response);
    }

    return stress_response;
}

void hypo_amyg_bridge_process_fear_output(
    hypo_amyg_bridge_t* bridge,
    const hypo_amyg_fear_output_t* output) {

    if (!bridge || !output) return;

    bridge->current_fear_output = *output;

    /* Autonomic activation -> arousal via posterior hypothalamus */
    if (output->autonomic_activation > 0.3f) {
        hypo_drive_set_nucleus_input(bridge->drives, HYPO_NUCLEUS_POSTERIOR,
                                     output->autonomic_activation * 0.4f);
    }

    /* Hormonal activation -> HPA axis via PVN */
    if (output->hormonal_activation > 0.3f) {
        hypo_drive_set_nucleus_input(bridge->drives, HYPO_NUCLEUS_PARAVENTRICULAR,
                                     output->hormonal_activation * 0.5f);
    }
}

bool hypo_amyg_bridge_get_fear_state(
    const hypo_amyg_bridge_t* bridge,
    hypo_amyg_fear_feedback_t* fear) {

    if (!bridge || !fear) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_amyg_bridge_get_fear_state: required parameter is NULL (bridge, fear)");
        return false;
    }

    *fear = bridge->current_fear;
    return true;
}

bool hypo_amyg_bridge_get_fear_drive_modulation(
    const hypo_amyg_bridge_t* bridge,
    float* modulation) {

    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_amyg_bridge_get_fear_drive_modulation: required parameter is NULL (bridge, modulation)");
        return false;
    }

    memcpy(modulation, bridge->fear_drive_modulation,
           sizeof(float) * HYPO_DRIVE_COUNT);
    return true;
}

/*=============================================================================
 * AMYGDALA CONNECTION
 *===========================================================================*/

bool hypo_amyg_bridge_connect(
    hypo_amyg_bridge_t* bridge,
    amygdala_t* amygdala) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_amyg_bridge_connect: bridge is NULL");
        return false;
    }

    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    bridge->amygdala = amygdala;

    /* Also register hypothalamus with amygdala */
    if (amygdala) {
        amygdala_connect_hypothalamus(amygdala, bridge->drives);
    }

    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_INFO("Amygdala connected to hypothalamus bridge");
    return true;
}

int hypo_amyg_bridge_send_stress(
    hypo_amyg_bridge_t* bridge,
    const hypo_amyg_stress_modulation_t* stress) {

    if (!bridge || !stress) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_amyg_bridge_send_stress: required parameter is NULL (bridge, stress)");
        return -1;
    }

    /* If amygdala connected, apply stress modulation */
    if (bridge->amygdala) {
        /* Set neuromodulators based on stress */
        amygdala_set_neuromodulators(bridge->amygdala,
                                     0.5f, /* Dopamine baseline */
                                     stress->norepinephrine_level,
                                     stress->cortisol_level);
    }

    bridge->stress_signals_sent++;
    return 0;
}

int hypo_amyg_bridge_send_drive_context(
    hypo_amyg_bridge_t* bridge,
    const hypo_amyg_drive_context_t* context) {

    if (!bridge || !context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_amyg_bridge_send_drive_context: required parameter is NULL (bridge, context)");
        return -1;
    }

    /* Drive context affects amygdala anxiety baseline */
    if (bridge->amygdala && context->any_drive_critical) {
        /* Critical drives increase baseline anxiety */
        float current_anxiety = amygdala_get_anxiety_level(bridge->amygdala);
        float new_anxiety = nimcp_clampf(current_anxiety + 0.1f, 0.0f, 1.0f);
        amygdala_set_anxiety(bridge->amygdala, new_anxiety);
    }

    return 0;
}

int hypo_amyg_bridge_query_fear(
    hypo_amyg_bridge_t* bridge,
    hypo_amyg_fear_feedback_t* fear) {

    if (!bridge || !fear) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_amyg_bridge_query_fear: required parameter is NULL (bridge, fear)");
        return -1;
    }

    if (bridge->amygdala) {
        fear->fear_level = amygdala_get_fear_level(bridge->amygdala);
        fear->anxiety_level = amygdala_get_anxiety_level(bridge->amygdala);
        fear->threat_level = amygdala_get_threat_level(bridge->amygdala);
        fear->timestamp_us = nimcp_time_now_us();
        return 0;
    }

    /* Amygdala not connected — not an error, just no data available */
    return -1;
}

/*=============================================================================
 * STRESS RESPONSE
 *===========================================================================*/

int hypo_amyg_bridge_trigger_acute_stress(
    hypo_amyg_bridge_t* bridge,
    float intensity) {

    if (!bridge || !bridge->drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_amyg_bridge_trigger_acute_stress: required parameter is NULL (bridge, bridge->drives)");
        return -1;
    }

    intensity = nimcp_clampf(intensity, 0.0f, 1.0f);

    /* Activate stress nuclei */
    /* PVN for CRH release */
    hypo_drive_set_nucleus_input(bridge->drives, HYPO_NUCLEUS_PARAVENTRICULAR,
                                 intensity * 0.8f);

    /* Posterior hypothalamus for arousal */
    hypo_drive_set_nucleus_input(bridge->drives, HYPO_NUCLEUS_POSTERIOR,
                                 intensity * 0.7f);

    /* Tuberomammillary for histamine arousal */
    hypo_drive_set_nucleus_input(bridge->drives, HYPO_NUCLEUS_TUBEROMAMMILLARY,
                                 intensity * 0.5f);

    NIMCP_LOG_DEBUG("Acute stress triggered: intensity=%.2f", intensity);
    return 0;
}

void hypo_amyg_bridge_enter_chronic_stress(hypo_amyg_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->chronic_stress_start_us == 0) {
        bridge->chronic_stress_start_us = nimcp_time_now_us();
        bridge->chronic_stress_episodes++;

        NIMCP_LOG_INFO("Entered chronic stress state");
    }
}

void hypo_amyg_bridge_exit_chronic_stress(hypo_amyg_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->chronic_stress_start_us > 0) {
        bridge->chronic_stress_start_us = 0;

        NIMCP_LOG_INFO("Exited chronic stress state");
    }
}

uint64_t hypo_amyg_bridge_get_threat_response_duration(
    const hypo_amyg_bridge_t* bridge) {

    if (!bridge || !bridge->in_threat_response) return 0;

    uint64_t now_us = nimcp_time_now_us();
    return (now_us - bridge->last_threat_us) / 1000; /* Return in ms */
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/* Module ID for this bridge */
#define HYPO_AMYG_BRIDGE_MODULE_ID  0x1170

/**
 * @brief Handler for fear level message
 */
static nimcp_error_t amyg_handle_fear_level(const void* msg, size_t msg_size,
                                             nimcp_bio_promise_t promise, void* ctx) {
    hypo_amyg_bridge_t* bridge = (hypo_amyg_bridge_t*)ctx;
    (void)msg_size;
    (void)promise;

    if (!bridge || !msg) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const struct {
        bio_message_header_t header;
        hypo_amyg_fear_feedback_t fear;
    }* fear_msg = msg;

    hypo_amyg_bridge_process_fear(bridge, &fear_msg->fear);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handler for threat detected message
 */
static nimcp_error_t amyg_handle_threat_detected(const void* msg, size_t msg_size,
                                                  nimcp_bio_promise_t promise, void* ctx) {
    hypo_amyg_bridge_t* bridge = (hypo_amyg_bridge_t*)ctx;
    (void)msg_size;
    (void)promise;

    if (!bridge || !msg) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const struct {
        bio_message_header_t header;
        hypo_amyg_threat_event_t threat;
    }* threat_msg = msg;

    hypo_amyg_bridge_process_threat(bridge, &threat_msg->threat);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handler for fear output message
 */
static nimcp_error_t amyg_handle_fear_output(const void* msg, size_t msg_size,
                                              nimcp_bio_promise_t promise, void* ctx) {
    hypo_amyg_bridge_t* bridge = (hypo_amyg_bridge_t*)ctx;
    (void)msg_size;
    (void)promise;

    if (!bridge || !msg) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const struct {
        bio_message_header_t header;
        hypo_amyg_fear_output_t output;
    }* output_msg = msg;

    hypo_amyg_bridge_process_fear_output(bridge, &output_msg->output);

    return NIMCP_SUCCESS;
}

bool hypo_amyg_bridge_register_bio(
    hypo_amyg_bridge_t* bridge,
    bool use_kg_wiring) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_amyg_bridge_register_bio: bridge is NULL");
        return false;
    }

    (void)use_kg_wiring; /* For future KG-driven wiring */

    /* Create module info for registration */
    bio_module_info_t info = {
        .module_id = HYPO_AMYG_BRIDGE_MODULE_ID,
        .module_name = "hypothalamus_amygdala_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    /* Register with router */
    bridge->bio_ctx = bio_router_register_module(&info);
    if (!bridge->bio_ctx) {
        NIMCP_LOG_ERROR("Failed to register amygdala bridge with bio router");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_amyg_bridge_register_bio: bio_ctx is NULL");
        return false;
    }

    /* Register message handlers */
    if (bio_router_register_handler(bridge->bio_ctx, BIO_MSG_AMYGDALA_FEAR_LEVEL,
                                     amyg_handle_fear_level) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "hypo_amyg_bridge_register_bio: fear handler registration failed");
        return false;
    }

    if (bio_router_register_handler(bridge->bio_ctx, BIO_MSG_AMYGDALA_THREAT_DETECTED,
                                     amyg_handle_threat_detected) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "hypo_amyg_bridge_register_bio: threat handler registration failed");
        return false;
    }

    if (bio_router_register_handler(bridge->bio_ctx, BIO_MSG_AMYGDALA_FEAR_OUTPUT,
                                     amyg_handle_fear_output) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "hypo_amyg_bridge_register_bio: fear output handler registration failed");
        return false;
    }

    NIMCP_LOG_INFO("Amygdala bridge registered with bio-async");
    return true;
}

uint32_t hypo_amyg_bridge_process_bio(
    hypo_amyg_bridge_t* bridge,
    uint32_t max_messages) {

    if (!bridge || !bridge->bio_ctx) return 0;

    return bio_router_process_inbox(bridge->bio_ctx, max_messages);
}

nimcp_error_t hypo_amyg_bridge_broadcast_stress(
    hypo_amyg_bridge_t* bridge) {

    if (!bridge || !bridge->bio_ctx || !bridge->config.broadcast_enabled) {
        return NIMCP_SUCCESS;
    }

    struct {
        bio_message_header_t header;
        hypo_amyg_stress_modulation_t stress;
    } msg;

    msg.header.type = BIO_MSG_AMYGDALA_STRESS_MODULATION;
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.source_module = HYPO_AMYG_BRIDGE_MODULE_ID;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.payload_size = sizeof(hypo_amyg_stress_modulation_t);
    msg.stress = bridge->current_stress;

    return bio_router_broadcast(bridge->bio_ctx, &msg.header, sizeof(msg));
}

nimcp_error_t hypo_amyg_bridge_broadcast_drive_context(
    hypo_amyg_bridge_t* bridge) {

    if (!bridge || !bridge->bio_ctx || !bridge->config.broadcast_enabled) {
        return NIMCP_SUCCESS;
    }

    struct {
        bio_message_header_t header;
        hypo_amyg_drive_context_t context;
    } msg;

    msg.header.type = BIO_MSG_AMYGDALA_DRIVE_CONTEXT;
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.source_module = HYPO_AMYG_BRIDGE_MODULE_ID;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.payload_size = sizeof(hypo_amyg_drive_context_t);
    msg.context = bridge->current_drive_context;

    return bio_router_broadcast(bridge->bio_ctx, &msg.header, sizeof(msg));
}

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

void hypo_amyg_bridge_get_stats(
    const hypo_amyg_bridge_t* bridge,
    uint64_t* stress_signals,
    uint64_t* fear_updates,
    uint64_t* threat_events,
    uint64_t* chronic_episodes) {

    if (!bridge) return;

    if (stress_signals) *stress_signals = bridge->stress_signals_sent;
    if (fear_updates) *fear_updates = bridge->fear_updates_received;
    if (threat_events) *threat_events = bridge->threat_events_processed;
    if (chronic_episodes) *chronic_episodes = bridge->chronic_stress_episodes;
}
