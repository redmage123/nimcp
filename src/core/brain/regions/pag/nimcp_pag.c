/**
 * @file nimcp_pag.c
 * @brief Periaqueductal Gray (PAG) Implementation
 *
 * Implements defensive behavior control, pain modulation, vocalization,
 * autonomic regulation, and emotional expression.
 */

#include "core/brain/regions/pag/nimcp_pag.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(pag, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

#define PAG_LOG_TAG "PAG"
#define PAG_MODULE_ID 0x5100
#define PAG_DEFAULT_THREAT_HISTORY 64
#define PAG_FREEZE_BASE_DURATION_MS 2000.0f
#define PAG_MIN_RESPONSE_LATENCY_US 50000  /* 50ms minimum */

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Compute winner-take-all competition between columns
 */
static void compute_column_competition(nimcp_pag_t* pag) {
    if (!pag) return;

    /* Find max activity */
    float max_activity = 0.0f;
    pag_column_t max_column = PAG_COLUMN_DORSOLATERAL;

    for (int i = 0; i < PAG_COLUMN_COUNT; i++) {
        if (pag->columns[i].activity > max_activity) {
            max_activity = pag->columns[i].activity;
            max_column = (pag_column_t)i;
        }
    }

    /* Apply lateral inhibition */
    float competition = pag->config.column_competition_strength;
    for (int i = 0; i < PAG_COLUMN_COUNT; i++) {
        if ((pag_column_t)i != max_column && max_activity > 0.1f) {
            pag->columns[i].inhibition += competition * max_activity;
            pag->columns[i].activity *= (1.0f - competition * 0.5f);
            pag->columns[i].activity = fmaxf(0.0f, pag->columns[i].activity);
        }
    }
}

/**
 * @brief Select defensive response based on column activity and context
 */
static pag_defense_type_t select_defense_response(nimcp_pag_t* pag) {
    if (!pag) return PAG_DEFENSE_FREEZE;

    float dl_activity = pag->columns[PAG_COLUMN_DORSOLATERAL].activity;
    float l_activity = pag->columns[PAG_COLUMN_LATERAL].activity;
    float vl_activity = pag->columns[PAG_COLUMN_VENTROLATERAL].activity;

    /* Active coping columns (dlPAG, lPAG) vs passive (vlPAG) */
    float active_total = dl_activity + l_activity;
    float passive_total = vl_activity;

    pag_defense_type_t response;

    if (active_total > passive_total && active_total > pag->config.threat_threshold) {
        /* Active coping selected */
        if (pag->defense.escape_route_available) {
            /* Flight preferred if escape available */
            response = PAG_DEFENSE_FLIGHT;
            pag->defense.response_probability[PAG_DEFENSE_FLIGHT] = 0.8f;
            pag->defense.response_probability[PAG_DEFENSE_FIGHT] = 0.2f;
        } else {
            /* Fight if cornered */
            float bias = pag->config.flight_vs_fight_bias;
            if (bias < 0.0f) {
                response = PAG_DEFENSE_FIGHT;
                pag->defense.response_probability[PAG_DEFENSE_FIGHT] = 0.7f;
                pag->defense.response_probability[PAG_DEFENSE_FLIGHT] = 0.3f;
            } else {
                response = PAG_DEFENSE_FLIGHT;
                pag->defense.response_probability[PAG_DEFENSE_FLIGHT] = 0.6f;
                pag->defense.response_probability[PAG_DEFENSE_FIGHT] = 0.4f;
            }
        }
        pag->defense.coping = PAG_COPING_ACTIVE;
    } else if (passive_total > pag->config.threat_threshold) {
        /* Passive coping selected */
        if (pag->defense.threat_level >= PAG_THREAT_CONTACT) {
            /* Fawn/submit if contact */
            response = PAG_DEFENSE_FAWN;
            pag->defense.response_probability[PAG_DEFENSE_FAWN] = 0.7f;
            pag->defense.response_probability[PAG_DEFENSE_FREEZE] = 0.3f;
        } else {
            /* Freeze if threat proximal but no contact */
            response = PAG_DEFENSE_FREEZE;
            pag->defense.response_probability[PAG_DEFENSE_FREEZE] = 0.8f;
            pag->defense.response_probability[PAG_DEFENSE_FAWN] = 0.2f;
        }
        pag->defense.coping = PAG_COPING_PASSIVE;
    } else {
        /* Below threshold - maintain current or return to baseline */
        response = pag->defense.active_defense;
        pag->defense.coping = PAG_COPING_MIXED;
    }

    return response;
}

/**
 * @brief Compute autonomic outputs based on column activity
 */
static void compute_autonomic_output(nimcp_pag_t* pag) {
    if (!pag) return;

    float dl_activity = pag->columns[PAG_COLUMN_DORSOLATERAL].activity;
    float vl_activity = pag->columns[PAG_COLUMN_VENTROLATERAL].activity;

    /* Cardiovascular modulation */
    /* dlPAG: tachycardia, hypertension */
    /* vlPAG: bradycardia, hypotension */
    float cv_coupling = pag->config.cardiovascular_coupling;
    pag->autonomic.heart_rate_modulation = (dl_activity - vl_activity) * cv_coupling;
    pag->autonomic.blood_pressure_modulation = (dl_activity - 0.5f * vl_activity) * cv_coupling;

    /* Respiratory modulation */
    float resp_coupling = pag->config.respiratory_coupling;
    pag->autonomic.respiratory_rate_mod = dl_activity * resp_coupling;
    pag->autonomic.respiratory_depth_mod = (dl_activity - vl_activity * 0.3f) * resp_coupling;

    /* Apnea during freeze */
    pag->autonomic.apnea_triggered = (vl_activity > 0.8f &&
                                       pag->defense.active_defense == PAG_DEFENSE_FREEZE);

    /* Pupil dilation (sympathetic activation) */
    pag->autonomic.pupil_dilation = fminf(1.0f, dl_activity + 0.3f * pag->defense.threat_intensity);

    /* Tonic immobility check */
    pag->autonomic.tonic_immobility = (vl_activity > 0.9f &&
                                        pag->defense.threat_level >= PAG_THREAT_CONTACT);

    /* Muscle tone */
    if (pag->autonomic.tonic_immobility) {
        pag->autonomic.muscle_tone = 0.0f;  /* Complete flaccidity */
    } else if (pag->defense.active_defense == PAG_DEFENSE_FREEZE) {
        pag->autonomic.muscle_tone = 0.9f;  /* Rigid freeze */
    } else {
        pag->autonomic.muscle_tone = 0.5f + 0.3f * dl_activity;
    }

    /* Other autonomic */
    pag->autonomic.sweating = 0.2f * dl_activity + 0.1f * pag->defense.threat_intensity;
    pag->autonomic.piloerection = 0.3f * pag->defense.threat_intensity;
    pag->autonomic.vasoconstriction = 0.4f * dl_activity;
    pag->autonomic.bladder_sphincter_tone = 1.0f - 0.3f * pag->defense.threat_intensity;
}

/**
 * @brief Compute analgesia based on stress and column activity
 */
static void compute_analgesia(nimcp_pag_t* pag, float dt) {
    if (!pag) return;

    float vl_activity = pag->columns[PAG_COLUMN_VENTROLATERAL].activity;
    float dl_activity = pag->columns[PAG_COLUMN_DORSOLATERAL].activity;

    /* Opioid pathway (vlPAG mediated) */
    float opioid_activation = vl_activity * pag->config.opioid_sensitivity;
    if (pag->analgesia.opioid_tolerance) {
        opioid_activation *= 0.5f;  /* Reduced effect with tolerance */
    }
    pag->analgesia.pathway_activity[PAG_PAIN_PATHWAY_OPIOID] = opioid_activation;

    /* Non-opioid pathway (stress-induced, dlPAG mediated) */
    float stress_factor = pag->analgesia.stress_induced_factor;
    float non_opioid_activation = dl_activity * pag->config.non_opioid_sensitivity * stress_factor;
    pag->analgesia.pathway_activity[PAG_PAIN_PATHWAY_NON_OPIOID] = non_opioid_activation;

    /* Cannabinoid pathway */
    pag->analgesia.pathway_activity[PAG_PAIN_PATHWAY_CANNABINOID] =
        0.3f * (vl_activity + dl_activity);

    /* Serotonergic pathway */
    pag->analgesia.pathway_activity[PAG_PAIN_PATHWAY_SEROTONERGIC] =
        0.4f * vl_activity;

    /* Noradrenergic pathway */
    pag->analgesia.pathway_activity[PAG_PAIN_PATHWAY_NORADRENERGIC] =
        0.5f * dl_activity;

    /* Compute total analgesia */
    float total_analgesia = 0.0f;
    for (int i = 0; i < PAG_PAIN_PATHWAY_COUNT; i++) {
        total_analgesia += pag->analgesia.pathway_activity[i];
    }
    total_analgesia = fminf(1.0f, total_analgesia * pag->config.analgesia_gain);
    pag->analgesia.analgesia_level = total_analgesia;

    /* Descending inhibition (to RVM -> spinal cord) */
    pag->analgesia.descending_inhibition =
        total_analgesia * pag->config.descending_inhibition_gain;

    /* Opioid tone tracking */
    pag->analgesia.opioid_tone = opioid_activation;

    /* Update duration */
    pag->analgesia.duration_us += (uint64_t)(dt * 1000000.0f);
}

/**
 * @brief Update emotional state based on threat and column activity
 */
static void update_emotional_state(nimcp_pag_t* pag) {
    if (!pag) return;

    /* Fear - driven by threat level and vlPAG */
    float vl = pag->columns[PAG_COLUMN_VENTROLATERAL].activity;
    float dl = pag->columns[PAG_COLUMN_DORSOLATERAL].activity;
    float l = pag->columns[PAG_COLUMN_LATERAL].activity;

    pag->emotion.emotion_levels[PAG_EMOTION_FEAR] =
        0.5f * pag->defense.threat_intensity + 0.3f * vl + 0.2f * dl;

    /* Rage - driven by dlPAG and lPAG during fight */
    pag->emotion.emotion_levels[PAG_EMOTION_RAGE] =
        (pag->defense.active_defense == PAG_DEFENSE_FIGHT) ?
        0.7f * dl + 0.3f * l : 0.2f * dl;

    /* Pain affect */
    pag->emotion.emotion_levels[PAG_EMOTION_PAIN] =
        pag->pain_active ? pag->current_pain.unpleasantness * (1.0f - pag->analgesia.analgesia_level) : 0.0f;

    /* Panic - separation distress, high threat with no escape */
    pag->emotion.emotion_levels[PAG_EMOTION_PANIC] =
        (!pag->defense.escape_route_available && pag->defense.threat_level >= PAG_THREAT_IMMINENT) ?
        0.8f * pag->defense.threat_intensity : 0.1f * pag->defense.threat_intensity;

    /* Find dominant emotion */
    float max_emotion = 0.0f;
    pag_emotion_type_t dominant = PAG_EMOTION_FEAR;
    for (int i = 0; i < PAG_EMOTION_COUNT; i++) {
        if (pag->emotion.emotion_levels[i] > max_emotion) {
            max_emotion = pag->emotion.emotion_levels[i];
            dominant = (pag_emotion_type_t)i;
        }
    }
    pag->emotion.dominant_emotion = dominant;
    pag->emotion.emotional_intensity = max_emotion;

    /* Valence and arousal */
    pag->emotion.valence = -0.5f * (pag->emotion.emotion_levels[PAG_EMOTION_FEAR] +
                                     pag->emotion.emotion_levels[PAG_EMOTION_PAIN] +
                                     pag->emotion.emotion_levels[PAG_EMOTION_PANIC]);
    pag->emotion.arousal = fminf(1.0f, dl + 0.5f * vl + 0.5f * pag->defense.threat_intensity);
}

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

int pag_default_config(pag_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    /* Defensive behavior parameters */
    config->threat_threshold = 0.3f;
    config->defense_decay_rate = 0.1f;
    config->coping_switch_threshold = 0.5f;
    config->flight_vs_fight_bias = 0.2f;  /* Slight flight preference */
    config->freeze_duration_base_ms = PAG_FREEZE_BASE_DURATION_MS;

    /* Pain modulation parameters */
    config->analgesia_gain = 1.0f;
    config->opioid_sensitivity = 0.8f;
    config->non_opioid_sensitivity = 0.6f;
    config->descending_inhibition_gain = 0.7f;
    config->stress_analgesia_threshold = 0.6f;

    /* Autonomic parameters */
    config->autonomic_gain = 1.0f;
    config->cardiovascular_coupling = 0.8f;
    config->respiratory_coupling = 0.6f;

    /* Vocalization parameters */
    config->vocal_threshold = 0.4f;
    config->vocal_intensity_gain = 1.0f;

    /* Column parameters */
    config->column_competition_strength = 0.3f;
    config->column_decay_rate = 0.05f;

    /* Integration settings */
    config->enable_bio_async = true;
    config->enable_kg_wiring = true;
    config->enable_immune = true;
    config->enable_security = true;
    config->enable_logging = true;
    config->enable_quantum = false;
    config->enable_hypothalamus_link = true;

    /* Resource limits */
    config->max_threat_history = PAG_DEFAULT_THREAT_HISTORY;
    config->update_interval_ms = 10;

    config->platform_tier = PLATFORM_TIER_FULL;

    return 0;
}

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

nimcp_pag_t* pag_create(const pag_config_t* config) {
    pag_config_t default_config;
    if (!config) {
        pag_default_config(&default_config);
        config = &default_config;
    }

    nimcp_pag_t* pag = (nimcp_pag_t*)nimcp_calloc(1, sizeof(nimcp_pag_t));
    if (!pag) {
        NIMCP_LOG_ERROR(PAG_LOG_TAG, "Failed to allocate PAG");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pag_create: pag is NULL");
        return NULL;
    }

    /* Copy config */
    memcpy(&pag->config, config, sizeof(pag_config_t));

    /* Create mutex */
    pag->mutex = nimcp_mutex_create(NULL);
    if (!pag->mutex) {
        NIMCP_LOG_ERROR(PAG_LOG_TAG, "Failed to create mutex");
        nimcp_free(pag);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pag_create: pag->mutex is NULL");
        return NULL;
    }

    /* Initialize columns */
    for (int i = 0; i < PAG_COLUMN_COUNT; i++) {
        pag->columns[i].column = (pag_column_t)i;
        pag->columns[i].activity = 0.0f;
        pag->columns[i].modulation = 1.0f;
        pag->columns[i].baseline = 0.1f;
        pag->columns[i].inhibition = 0.0f;
        pag->columns[i].excitation = 0.0f;
        pag->columns[i].active = false;
    }

    NIMCP_LOG_INFO(PAG_LOG_TAG, "PAG created");

    return pag;
}

void pag_destroy(nimcp_pag_t* pag) {
    if (!pag) return;

    NIMCP_LOG_INFO(PAG_LOG_TAG, "Destroying PAG (threats=%lu, pain_signals=%lu)",
                   pag->stats.threats_detected, pag->stats.pain_signals_processed);

    /* Disconnect from systems */
    if (pag->connected) {
        pag_bio_async_disconnect(pag);
        pag_kg_unregister(pag);
    }

    /* Clean up */
    if (pag->mutex) {
        nimcp_mutex_free(pag->mutex);
    }

    nimcp_free(pag);
}

int pag_init(nimcp_pag_t* pag) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_init: pag is NULL");
        return -1;
    }

    nimcp_mutex_lock(pag->mutex);

    /* Reset defensive state */
    memset(&pag->defense, 0, sizeof(pag_defense_state_t));
    pag->defense.threat_level = PAG_THREAT_NONE;
    pag->defense.active_defense = PAG_DEFENSE_FREEZE;
    pag->defense.coping = PAG_COPING_MIXED;
    pag->defense.escape_route_available = true;

    /* Reset analgesia */
    memset(&pag->analgesia, 0, sizeof(pag_analgesia_state_t));

    /* Reset pain */
    memset(&pag->current_pain, 0, sizeof(pag_pain_input_t));
    pag->pain_active = false;

    /* Reset vocalization */
    memset(&pag->vocal, 0, sizeof(pag_vocal_state_t));

    /* Reset autonomic */
    memset(&pag->autonomic, 0, sizeof(pag_autonomic_state_t));
    pag->autonomic.muscle_tone = 0.5f;
    pag->autonomic.bladder_sphincter_tone = 1.0f;

    /* Reset emotional state */
    memset(&pag->emotion, 0, sizeof(pag_emotional_state_t));

    /* Reset columns to baseline */
    for (int i = 0; i < PAG_COLUMN_COUNT; i++) {
        pag->columns[i].activity = pag->columns[i].baseline;
        pag->columns[i].inhibition = 0.0f;
        pag->columns[i].excitation = 0.0f;
        pag->columns[i].active = false;
    }

    /* Reset stats */
    memset(&pag->stats, 0, sizeof(pag_stats_t));

    pag->initialized = true;
    pag->last_update_us = 0;

    nimcp_mutex_unlock(pag->mutex);

    NIMCP_LOG_INFO(PAG_LOG_TAG, "PAG initialized");
    return 0;
}

int pag_reset(nimcp_pag_t* pag) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_reset: pag is NULL");
        return -1;
    }
    return pag_init(pag);
}

/*=============================================================================
 * THREAT AND DEFENSIVE BEHAVIOR
 *===========================================================================*/

int pag_process_threat(nimcp_pag_t* pag, pag_threat_level_t threat_level,
                       float intensity, float direction, float distance) {
    if (!pag || !pag->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_reset: required parameter is NULL (pag, pag->initialized)");
        return -1;
    }

    nimcp_mutex_lock(pag->mutex);

    /* Update threat state */
    pag->defense.threat_level = threat_level;
    pag->defense.threat_intensity = fmaxf(0.0f, fminf(1.0f, intensity));
    pag->defense.threat_direction = direction;
    pag->defense.threat_distance = distance;

    if (threat_level != PAG_THREAT_NONE && !pag->defense.defense_active) {
        pag->defense.threat_onset_us = pag->last_update_us;
        pag->defense.defense_active = true;
        pag->stats.threats_detected++;
    }

    /* Activate appropriate columns based on threat level */
    float activation_strength = intensity;

    switch (threat_level) {
        case PAG_THREAT_DISTAL:
            /* dmPAG: defensive attention */
            pag->columns[PAG_COLUMN_DORSOMEDIAL].excitation += activation_strength * 0.5f;
            break;

        case PAG_THREAT_PROXIMAL:
            /* dlPAG/lPAG: prepare for active coping */
            pag->columns[PAG_COLUMN_DORSOLATERAL].excitation += activation_strength * 0.7f;
            pag->columns[PAG_COLUMN_LATERAL].excitation += activation_strength * 0.4f;
            break;

        case PAG_THREAT_IMMINENT:
            /* Strong activation of action columns */
            if (pag->defense.escape_route_available) {
                pag->columns[PAG_COLUMN_DORSOLATERAL].excitation += activation_strength;
            } else {
                pag->columns[PAG_COLUMN_VENTROLATERAL].excitation += activation_strength * 0.6f;
                pag->columns[PAG_COLUMN_DORSOLATERAL].excitation += activation_strength * 0.4f;
            }
            break;

        case PAG_THREAT_CONTACT:
            /* vlPAG for freeze/fawn if overwhelmed */
            pag->columns[PAG_COLUMN_VENTROLATERAL].excitation += activation_strength * 0.8f;
            /* But also potential fight response */
            pag->columns[PAG_COLUMN_DORSOLATERAL].excitation += activation_strength * 0.4f;
            break;

        case PAG_THREAT_NONE:
        default:
            /* Gradual return to baseline handled in update */
            break;
    }

    /* Update column activities */
    for (int i = 0; i < PAG_COLUMN_COUNT; i++) {
        float net_input = pag->columns[i].excitation - pag->columns[i].inhibition;
        pag->columns[i].activity += net_input * pag->columns[i].modulation;
        pag->columns[i].activity = fmaxf(0.0f, fminf(1.0f, pag->columns[i].activity));
        pag->columns[i].active = (pag->columns[i].activity > pag->config.threat_threshold);
    }

    /* Column competition */
    compute_column_competition(pag);

    /* Select defensive response */
    pag_defense_type_t old_defense = pag->defense.active_defense;
    pag->defense.active_defense = select_defense_response(pag);

    /* Track coping switches */
    if (old_defense != pag->defense.active_defense) {
        bool was_active = (old_defense == PAG_DEFENSE_FIGHT || old_defense == PAG_DEFENSE_FLIGHT);
        bool is_active = (pag->defense.active_defense == PAG_DEFENSE_FIGHT ||
                          pag->defense.active_defense == PAG_DEFENSE_FLIGHT);
        if (was_active != is_active) {
            pag->stats.coping_switches++;
        }
    }

    /* Update defense intensity */
    pag->defense.defense_intensity = fmaxf(
        pag->columns[PAG_COLUMN_DORSOLATERAL].activity,
        pag->columns[PAG_COLUMN_VENTROLATERAL].activity);

    /* Compute motor output */
    if (pag->defense.active_defense == PAG_DEFENSE_FIGHT ||
        pag->defense.active_defense == PAG_DEFENSE_FLIGHT) {
        pag->defense.motor_output = pag->columns[PAG_COLUMN_DORSOLATERAL].activity;
    } else {
        pag->defense.motor_output = 0.0f;  /* Freeze/fawn inhibit motor */
    }

    /* Update stats */
    pag->stats.defense_activations[pag->defense.active_defense]++;

    /* Broadcast threat detection */
    if (pag->bio_router && pag->config.enable_bio_async) {
        pag_bio_async_broadcast(pag, PAG_BIO_MSG_THREAT_DETECTED,
                                &pag->defense, sizeof(pag_defense_state_t));
    }

    nimcp_mutex_unlock(pag->mutex);

    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Threat: level=%d, intensity=%.2f, defense=%s",
                    threat_level, intensity,
                    pag_defense_string(pag->defense.active_defense));

    return 0;
}

int pag_get_defense_state(const nimcp_pag_t* pag, pag_defense_state_t* defense) {
    if (!pag || !defense) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_get_defense_state: required parameter is NULL (pag, defense)");
        return -1;
    }

    nimcp_mutex_lock(((nimcp_pag_t*)pag)->mutex);
    memcpy(defense, &pag->defense, sizeof(pag_defense_state_t));
    nimcp_mutex_unlock(((nimcp_pag_t*)pag)->mutex);

    return 0;
}

int pag_set_defense_response(nimcp_pag_t* pag, pag_defense_type_t defense_type,
                             float intensity) {
    if (!pag || defense_type >= PAG_DEFENSE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pag_get_defense_state: pag is NULL");
        return -1;
    }

    nimcp_mutex_lock(pag->mutex);

    pag->defense.active_defense = defense_type;
    pag->defense.defense_intensity = fmaxf(0.0f, fminf(1.0f, intensity));
    pag->defense.defense_active = (intensity > 0.0f);

    /* Set coping strategy */
    if (defense_type == PAG_DEFENSE_FIGHT || defense_type == PAG_DEFENSE_FLIGHT) {
        pag->defense.coping = PAG_COPING_ACTIVE;
    } else {
        pag->defense.coping = PAG_COPING_PASSIVE;
    }

    nimcp_mutex_unlock(pag->mutex);

    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Defense set: %s, intensity=%.2f",
                    pag_defense_string(defense_type), intensity);

    return 0;
}

int pag_check_escape_route(nimcp_pag_t* pag, bool* available) {
    if (!pag || !available) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_check_escape_route: required parameter is NULL (pag, available)");
        return -1;
    }

    nimcp_mutex_lock(pag->mutex);
    *available = pag->defense.escape_route_available;
    nimcp_mutex_unlock(pag->mutex);

    return 0;
}

int pag_set_escape_route(nimcp_pag_t* pag, bool available) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_set_escape_route: pag is NULL");
        return -1;
    }

    nimcp_mutex_lock(pag->mutex);
    pag->defense.escape_route_available = available;
    nimcp_mutex_unlock(pag->mutex);

    return 0;
}

int pag_clear_threat(nimcp_pag_t* pag) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_clear_threat: pag is NULL");
        return -1;
    }

    nimcp_mutex_lock(pag->mutex);

    pag->defense.threat_level = PAG_THREAT_NONE;
    pag->defense.threat_intensity = 0.0f;
    pag->defense.defense_active = false;
    pag->defense.habituated = false;

    /* Clear column excitation */
    for (int i = 0; i < PAG_COLUMN_COUNT; i++) {
        pag->columns[i].excitation = 0.0f;
    }

    if (pag->bio_router && pag->config.enable_bio_async) {
        pag_bio_async_broadcast(pag, PAG_BIO_MSG_DEFENSE_TERMINATED, NULL, 0);
    }

    nimcp_mutex_unlock(pag->mutex);

    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Threat cleared");

    return 0;
}

pag_coping_strategy_t pag_get_coping_strategy(const nimcp_pag_t* pag) {
    if (!pag) return PAG_COPING_MIXED;
    return pag->defense.coping;
}

/*=============================================================================
 * PAIN MODULATION
 *===========================================================================*/

int pag_process_pain(nimcp_pag_t* pag, const pag_pain_input_t* pain) {
    if (!pag || !pain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_process_pain: required parameter is NULL (pag, pain)");
        return -1;
    }

    nimcp_mutex_lock(pag->mutex);

    memcpy(&pag->current_pain, pain, sizeof(pag_pain_input_t));
    pag->pain_active = (pain->intensity > 0.0f);

    /* Pain activates vlPAG for analgesia pathway */
    pag->columns[PAG_COLUMN_VENTROLATERAL].excitation += pain->intensity * 0.5f;

    /* High pain can also trigger dlPAG (escape from pain source) */
    if (pain->intensity > 0.6f) {
        pag->columns[PAG_COLUMN_DORSOLATERAL].excitation += pain->intensity * 0.3f;
    }

    /* Trigger stress-induced analgesia if pain is severe */
    if (pain->intensity > pag->config.stress_analgesia_threshold) {
        pag->analgesia.stress_induced_factor = pain->intensity;
        pag->analgesia.onset_timestamp_us = pag->last_update_us;
    }

    /* Pain vocalization */
    if (pain->intensity > pag->config.vocal_threshold) {
        pag->vocal.type = PAG_VOCAL_DISTRESS;
        pag->vocal.intensity = pain->intensity * pag->config.vocal_intensity_gain;
        pag->vocal.active = true;
        pag->vocal.onset_us = pag->last_update_us;
    }

    pag->stats.pain_signals_processed++;

    if (pag->bio_router && pag->config.enable_bio_async) {
        pag_bio_async_broadcast(pag, PAG_BIO_MSG_PAIN_RECEIVED,
                                pain, sizeof(pag_pain_input_t));
    }

    nimcp_mutex_unlock(pag->mutex);

    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Pain processed: intensity=%.2f, loc=%u",
                    pain->intensity, pain->location_code);

    return 0;
}

int pag_get_analgesia_state(const nimcp_pag_t* pag, pag_analgesia_state_t* analgesia) {
    if (!pag || !analgesia) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_get_analgesia_state: required parameter is NULL (pag, analgesia)");
        return -1;
    }

    nimcp_mutex_lock(((nimcp_pag_t*)pag)->mutex);
    memcpy(analgesia, &pag->analgesia, sizeof(pag_analgesia_state_t));
    nimcp_mutex_unlock(((nimcp_pag_t*)pag)->mutex);

    return 0;
}

int pag_activate_pain_pathway(nimcp_pag_t* pag, pag_pain_pathway_t pathway,
                              float activation) {
    if (!pag || pathway >= PAG_PAIN_PATHWAY_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pag_get_analgesia_state: pag is NULL");
        return -1;
    }

    nimcp_mutex_lock(pag->mutex);

    pag->analgesia.pathway_activity[pathway] = fmaxf(0.0f, fminf(1.0f, activation));

    /* Update total analgesia */
    float total = 0.0f;
    for (int i = 0; i < PAG_PAIN_PATHWAY_COUNT; i++) {
        total += pag->analgesia.pathway_activity[i];
    }
    pag->analgesia.analgesia_level = fminf(1.0f, total * pag->config.analgesia_gain);

    if (pag->bio_router && pag->config.enable_bio_async) {
        pag_bio_async_broadcast(pag, PAG_BIO_MSG_ANALGESIA_ONSET,
                                &pag->analgesia, sizeof(pag_analgesia_state_t));
    }

    nimcp_mutex_unlock(pag->mutex);

    return 0;
}

float pag_get_descending_inhibition(const nimcp_pag_t* pag) {
    if (!pag) return 0.0f;
    return pag->analgesia.descending_inhibition;
}

int pag_trigger_stress_analgesia(nimcp_pag_t* pag, float stress_level) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_trigger_stress_analgesia: pag is NULL");
        return -1;
    }

    nimcp_mutex_lock(pag->mutex);

    pag->analgesia.stress_induced_factor = fmaxf(0.0f, fminf(1.0f, stress_level));

    if (stress_level > pag->config.stress_analgesia_threshold) {
        /* Activate non-opioid pathway */
        pag->analgesia.pathway_activity[PAG_PAIN_PATHWAY_NON_OPIOID] =
            stress_level * pag->config.non_opioid_sensitivity;
        pag->analgesia.onset_timestamp_us = pag->last_update_us;
        pag->stats.analgesia_episodes++;
    }

    nimcp_mutex_unlock(pag->mutex);

    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Stress-induced analgesia: level=%.2f", stress_level);

    return 0;
}

bool pag_has_opioid_tolerance(const nimcp_pag_t* pag) {
    if (!pag) {
        return false;
    }
    return pag->analgesia.opioid_tolerance;
}

/*=============================================================================
 * VOCALIZATION
 *===========================================================================*/

int pag_trigger_vocalization(nimcp_pag_t* pag, pag_vocal_type_t type,
                             float intensity) {
    if (!pag || type >= PAG_VOCAL_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pag_has_opioid_tolerance: pag is NULL");
        return -1;
    }

    nimcp_mutex_lock(pag->mutex);

    pag->vocal.type = type;
    pag->vocal.intensity = fmaxf(0.0f, fminf(1.0f, intensity)) * pag->config.vocal_intensity_gain;
    pag->vocal.urgency = pag->defense.threat_intensity;
    pag->vocal.active = (intensity > 0.0f);
    pag->vocal.onset_us = pag->last_update_us;

    /* Activate lPAG for vocalization */
    pag->columns[PAG_COLUMN_LATERAL].excitation += intensity * 0.6f;

    pag->stats.vocalizations[type]++;

    if (pag->bio_router && pag->config.enable_bio_async) {
        pag_bio_async_broadcast(pag, PAG_BIO_MSG_VOCALIZATION,
                                &pag->vocal, sizeof(pag_vocal_state_t));
    }

    nimcp_mutex_unlock(pag->mutex);

    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Vocalization: type=%s, intensity=%.2f",
                    pag_vocal_string(type), intensity);

    return 0;
}

int pag_get_vocalization_state(const nimcp_pag_t* pag, pag_vocal_state_t* vocal) {
    if (!pag || !vocal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_get_vocalization_state: required parameter is NULL (pag, vocal)");
        return -1;
    }

    nimcp_mutex_lock(((nimcp_pag_t*)pag)->mutex);
    memcpy(vocal, &pag->vocal, sizeof(pag_vocal_state_t));
    nimcp_mutex_unlock(((nimcp_pag_t*)pag)->mutex);

    return 0;
}

int pag_stop_vocalization(nimcp_pag_t* pag) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_stop_vocalization: pag is NULL");
        return -1;
    }

    nimcp_mutex_lock(pag->mutex);
    pag->vocal.active = false;
    pag->vocal.intensity = 0.0f;
    nimcp_mutex_unlock(pag->mutex);

    return 0;
}

/*=============================================================================
 * AUTONOMIC CONTROL
 *===========================================================================*/

int pag_get_autonomic_state(const nimcp_pag_t* pag, pag_autonomic_state_t* autonomic) {
    if (!pag || !autonomic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_get_autonomic_state: required parameter is NULL (pag, autonomic)");
        return -1;
    }

    nimcp_mutex_lock(((nimcp_pag_t*)pag)->mutex);
    memcpy(autonomic, &pag->autonomic, sizeof(pag_autonomic_state_t));
    nimcp_mutex_unlock(((nimcp_pag_t*)pag)->mutex);

    return 0;
}

int pag_get_cardiovascular_output(const nimcp_pag_t* pag,
                                  float* heart_rate_mod, float* bp_mod) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_get_autonomic_state: pag is NULL");
        return -1;
    }

    nimcp_mutex_lock(((nimcp_pag_t*)pag)->mutex);
    if (heart_rate_mod) *heart_rate_mod = pag->autonomic.heart_rate_modulation;
    if (bp_mod) *bp_mod = pag->autonomic.blood_pressure_modulation;
    nimcp_mutex_unlock(((nimcp_pag_t*)pag)->mutex);

    return 0;
}

int pag_get_respiratory_output(const nimcp_pag_t* pag,
                               float* rate_mod, float* depth_mod) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_get_autonomic_state: pag is NULL");
        return -1;
    }

    nimcp_mutex_lock(((nimcp_pag_t*)pag)->mutex);
    if (rate_mod) *rate_mod = pag->autonomic.respiratory_rate_mod;
    if (depth_mod) *depth_mod = pag->autonomic.respiratory_depth_mod;
    nimcp_mutex_unlock(((nimcp_pag_t*)pag)->mutex);

    return 0;
}

bool pag_is_tonic_immobility(const nimcp_pag_t* pag) {
    if (!pag) {
        return false;
    }
    return pag->autonomic.tonic_immobility;
}

/*=============================================================================
 * EMOTIONAL STATE
 *===========================================================================*/

int pag_get_emotional_state(const nimcp_pag_t* pag, pag_emotional_state_t* emotion) {
    if (!pag || !emotion) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_get_emotional_state: required parameter is NULL (pag, emotion)");
        return -1;
    }

    nimcp_mutex_lock(((nimcp_pag_t*)pag)->mutex);
    memcpy(emotion, &pag->emotion, sizeof(pag_emotional_state_t));
    nimcp_mutex_unlock(((nimcp_pag_t*)pag)->mutex);

    return 0;
}

int pag_set_emotion_input(nimcp_pag_t* pag, pag_emotion_type_t emotion_type,
                          float intensity) {
    if (!pag || emotion_type >= PAG_EMOTION_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pag_get_emotional_state: pag is NULL");
        return -1;
    }

    nimcp_mutex_lock(pag->mutex);

    pag->emotion.emotion_levels[emotion_type] =
        fmaxf(0.0f, fminf(1.0f, intensity));

    /* Emotions can drive column activity */
    switch (emotion_type) {
        case PAG_EMOTION_FEAR:
        case PAG_EMOTION_PANIC:
            pag->columns[PAG_COLUMN_VENTROLATERAL].excitation += intensity * 0.3f;
            break;
        case PAG_EMOTION_RAGE:
            pag->columns[PAG_COLUMN_DORSOLATERAL].excitation += intensity * 0.4f;
            break;
        default:
            break;
    }

    if (pag->bio_router && pag->config.enable_bio_async) {
        pag_bio_async_broadcast(pag, PAG_BIO_MSG_EMOTION_UPDATE,
                                &pag->emotion, sizeof(pag_emotional_state_t));
    }

    nimcp_mutex_unlock(pag->mutex);

    return 0;
}

pag_emotion_type_t pag_get_dominant_emotion(const nimcp_pag_t* pag) {
    if (!pag) return PAG_EMOTION_FEAR;
    return pag->emotion.dominant_emotion;
}

/*=============================================================================
 * COLUMN CONTROL
 *===========================================================================*/

float pag_get_column_activity(const nimcp_pag_t* pag, pag_column_t column) {
    if (!pag || column >= PAG_COLUMN_COUNT) return 0.0f;
    return pag->columns[column].activity;
}

int pag_set_column_modulation(nimcp_pag_t* pag, pag_column_t column,
                              float modulation) {
    if (!pag || column >= PAG_COLUMN_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pag_get_column_activity: pag is NULL");
        return -1;
    }

    nimcp_mutex_lock(pag->mutex);
    pag->columns[column].modulation = fmaxf(0.0f, fminf(2.0f, modulation));
    nimcp_mutex_unlock(pag->mutex);

    return 0;
}

int pag_get_column_state(const nimcp_pag_t* pag, pag_column_t column,
                         pag_column_state_t* state) {
    if (!pag || !state || column >= PAG_COLUMN_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_get_column_activity: required parameter is NULL (pag, state)");
        return -1;
    }

    nimcp_mutex_lock(((nimcp_pag_t*)pag)->mutex);
    memcpy(state, &pag->columns[column], sizeof(pag_column_state_t));
    nimcp_mutex_unlock(((nimcp_pag_t*)pag)->mutex);

    return 0;
}

pag_column_t pag_get_dominant_column(const nimcp_pag_t* pag) {
    if (!pag) return PAG_COLUMN_DORSOLATERAL;

    float max_activity = 0.0f;
    pag_column_t dominant = PAG_COLUMN_DORSOLATERAL;

    for (int i = 0; i < PAG_COLUMN_COUNT; i++) {
        if (pag->columns[i].activity > max_activity) {
            max_activity = pag->columns[i].activity;
            dominant = (pag_column_t)i;
        }
    }

    return dominant;
}

/*=============================================================================
 * KG WIRING INTEGRATION
 *===========================================================================*/

int pag_kg_register(nimcp_pag_t* pag, struct nimcp_brain_kg* kg,
                    uint64_t admin_token) {
    if (!pag || !kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_get_dominant_column: required parameter is NULL (pag, kg)");
        return -1;
    }

    nimcp_mutex_lock(pag->mutex);

    pag->kg = kg;
    pag->kg_state.admin_token = admin_token;

    /* Register main PAG node */
    pag->kg_state.region_node_id = PAG_MODULE_ID;

    /* Register column nodes */
    for (int i = 0; i < PAG_COLUMN_COUNT; i++) {
        pag->kg_state.column_node_ids[i] = PAG_MODULE_ID + 1 + i;
    }

    /* Register defense state nodes */
    for (int i = 0; i < PAG_DEFENSE_COUNT; i++) {
        pag->kg_state.defense_node_ids[i] = PAG_MODULE_ID + 100 + i;
    }

    /* Register pain pathway nodes */
    for (int i = 0; i < PAG_PAIN_PATHWAY_COUNT; i++) {
        pag->kg_state.pain_pathway_node_ids[i] = PAG_MODULE_ID + 200 + i;
    }

    /* Register emotion nodes */
    for (int i = 0; i < PAG_EMOTION_COUNT; i++) {
        pag->kg_state.emotion_node_ids[i] = PAG_MODULE_ID + 300 + i;
    }

    pag->kg_state.registered = true;
    pag->stats.kg_updates++;

    nimcp_mutex_unlock(pag->mutex);

    NIMCP_LOG_INFO(PAG_LOG_TAG, "Registered with KG, node_id=%lu",
                   pag->kg_state.region_node_id);

    return 0;
}

int pag_kg_unregister(nimcp_pag_t* pag) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_kg_unregister: pag is NULL");
        return -1;
    }

    nimcp_mutex_lock(pag->mutex);

    if (pag->kg_state.registered) {
        pag->kg_state.registered = false;
        pag->kg = NULL;
    }

    nimcp_mutex_unlock(pag->mutex);
    return 0;
}

/* pag_kg_update_state is implemented in nimcp_pag_kg_wiring.c */

int pag_kg_query(nimcp_pag_t* pag, const char* query,
                 void* result, size_t result_size) {
    if (!pag || !query || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_kg_unregister: required parameter is NULL (pag, query, result)");
        return -1;
    }
    if (!pag->kg_state.registered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_kg_unregister: pag->kg_state is NULL");
        return -1;
    }

    /* Actual KG query implementation would go here */
    (void)result_size;

    return 0;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/* pag_bio_async_connect and pag_bio_async_disconnect are implemented in nimcp_pag_bio_async_bridge.c */

int pag_bio_async_broadcast(nimcp_pag_t* pag, pag_bio_msg_type_t msg_type,
                            const void* payload, size_t payload_size) {
    if (!pag || !pag->bio_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_kg_unregister: required parameter is NULL (pag, pag->bio_router)");
        return -1;
    }

    pag->stats.bio_msgs_sent++;

    /* Actual broadcast implementation would use bio-async API */
    (void)msg_type;
    (void)payload;
    (void)payload_size;

    return 0;
}

int pag_bio_async_subscribe(nimcp_pag_t* pag, uint32_t subscription_mask) {
    if (!pag || !pag->bio_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_bio_async_subscribe: required parameter is NULL (pag, pag->bio_router)");
        return -1;
    }

    /* Subscribe to messages */
    (void)subscription_mask;

    return 0;
}

/*=============================================================================
 * OTHER SYSTEM INTEGRATIONS
 *===========================================================================*/

int pag_security_connect(nimcp_pag_t* pag, struct nimcp_security_context* security) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_security_connect: pag is NULL");
        return -1;
    }
    pag->security = security;
    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Connected to security context");
    return 0;
}

int pag_bbb_register(nimcp_pag_t* pag) {
    if (!pag || !pag->security) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_bbb_register: required parameter is NULL (pag, pag->security)");
        return -1;
    }
    /* BBB registration implementation */
    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Registered with BBB");
    return 0;
}

int pag_immune_connect(nimcp_pag_t* pag, struct nimcp_immune_system* immune) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_immune_connect: pag is NULL");
        return -1;
    }
    pag->immune = immune;
    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Connected to immune system");
    return 0;
}

int pag_immune_alert(nimcp_pag_t* pag, uint32_t alert_type, float severity) {
    if (!pag || !pag->immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_immune_alert: required parameter is NULL (pag, pag->immune)");
        return -1;
    }
    pag->stats.immune_alerts++;
    /* Immune alert implementation */
    (void)alert_type;
    (void)severity;
    return 0;
}

int pag_snn_connect(nimcp_pag_t* pag, struct nimcp_snn_network* snn,
                    struct nimcp_plasticity_engine* plasticity) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_immune_alert: pag is NULL");
        return -1;
    }
    pag->snn = snn;
    pag->plasticity = plasticity;
    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Connected to SNN/plasticity");
    return 0;
}

int pag_update_plasticity(nimcp_pag_t* pag, float reward) {
    if (!pag || !pag->plasticity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_update_plasticity: required parameter is NULL (pag, pag->plasticity)");
        return -1;
    }
    /* Plasticity update implementation */
    (void)reward;
    return 0;
}

int pag_hypothalamus_connect(nimcp_pag_t* pag, struct nimcp_hypothalamus* hypo) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_hypothalamus_connect: pag is NULL");
        return -1;
    }
    pag->hypothalamus = hypo;
    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Connected to hypothalamus");
    return 0;
}

int pag_receive_drive_signal(nimcp_pag_t* pag, uint32_t drive_type,
                             float drive_level) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_hypothalamus_connect: pag is NULL");
        return -1;
    }

    nimcp_mutex_lock(pag->mutex);

    /* Hypothalamic drives can modulate PAG columns */
    /* Safety drive -> increases vlPAG activity */
    /* Hunger/thirst under threat -> increases dlPAG (fight for resources) */

    pag->stats.hypothalamus_exchanges++;

    nimcp_mutex_unlock(pag->mutex);

    (void)drive_type;
    (void)drive_level;

    return 0;
}

int pag_send_to_hypothalamus(nimcp_pag_t* pag) {
    if (!pag || !pag->hypothalamus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_send_to_hypothalamus: required parameter is NULL (pag, pag->hypothalamus)");
        return -1;
    }

    /* Send defensive state to hypothalamus */
    pag->stats.hypothalamus_exchanges++;

    if (pag->bio_router && pag->config.enable_bio_async) {
        pag_bio_async_broadcast(pag, PAG_BIO_MSG_HYPOTHALAMUS_SIGNAL,
                                &pag->defense, sizeof(pag_defense_state_t));
    }

    return 0;
}

int pag_thalamus_connect(nimcp_pag_t* pag, struct nimcp_thalamus* thalamus) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_thalamus_connect: pag is NULL");
        return -1;
    }
    pag->thalamus = thalamus;
    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Connected to thalamus");
    return 0;
}

int pag_amygdala_connect(nimcp_pag_t* pag, struct nimcp_amygdala* amygdala) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_amygdala_connect: pag is NULL");
        return -1;
    }
    pag->amygdala = amygdala;
    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Connected to amygdala");
    return 0;
}

int pag_prefrontal_connect(nimcp_pag_t* pag, struct nimcp_prefrontal* prefrontal) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_prefrontal_connect: pag is NULL");
        return -1;
    }
    pag->prefrontal = prefrontal;
    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Connected to prefrontal cortex");
    return 0;
}

int pag_brainstem_connect(nimcp_pag_t* pag, struct nimcp_brainstem* brainstem) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_brainstem_connect: pag is NULL");
        return -1;
    }
    pag->brainstem = brainstem;
    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Connected to brainstem");
    return 0;
}

int pag_rvm_connect(nimcp_pag_t* pag, struct nimcp_rvm* rvm) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_rvm_connect: pag is NULL");
        return -1;
    }
    pag->rvm = rvm;
    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Connected to RVM (descending pain modulation)");
    return 0;
}

int pag_cognitive_connect(nimcp_pag_t* pag, struct nimcp_cognitive_hub* hub) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_cognitive_connect: pag is NULL");
        return -1;
    }
    pag->cognitive_hub = hub;
    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Connected to cognitive hub");
    return 0;
}

int pag_training_connect(nimcp_pag_t* pag, struct nimcp_training_context* training) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_training_connect: pag is NULL");
        return -1;
    }
    pag->training = training;
    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Connected to training system");
    return 0;
}

int pag_perception_connect(nimcp_pag_t* pag, struct nimcp_perception_system* perception) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_perception_connect: pag is NULL");
        return -1;
    }
    pag->perception = perception;
    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Connected to perception system");
    return 0;
}

int pag_symbolic_connect(nimcp_pag_t* pag, struct nimcp_symbolic_engine* symbolic) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_symbolic_connect: pag is NULL");
        return -1;
    }
    pag->symbolic = symbolic;
    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Connected to symbolic engine");
    return 0;
}

int pag_swarm_connect(nimcp_pag_t* pag, struct nimcp_swarm_context* swarm) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_swarm_connect: pag is NULL");
        return -1;
    }
    pag->swarm = swarm;
    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Connected to swarm system");
    return 0;
}

int pag_dragonfly_connect(nimcp_pag_t* pag, struct nimcp_dragonfly_context* dragonfly) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_dragonfly_connect: pag is NULL");
        return -1;
    }
    pag->dragonfly = dragonfly;
    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Connected to dragonfly system");
    return 0;
}

int pag_portia_connect(nimcp_pag_t* pag, struct nimcp_portia_context* portia) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_portia_connect: pag is NULL");
        return -1;
    }
    pag->portia = portia;
    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Connected to portia system");
    return 0;
}

int pag_qmc_connect(nimcp_pag_t* pag, struct nimcp_qmc_context* qmc) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_qmc_connect: pag is NULL");
        return -1;
    }
    pag->qmc = qmc;
    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Connected to QMC system");
    return 0;
}

int pag_omni_connect(nimcp_pag_t* pag, struct nimcp_omni_predictor* omni) {
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_omni_connect: pag is NULL");
        return -1;
    }
    pag->omni = omni;
    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "Connected to omnidirectional predictor");
    return 0;
}

int pag_qmc_optimize_defense(nimcp_pag_t* pag) {
    if (!pag || !pag->qmc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_qmc_optimize_defense: required parameter is NULL (pag, pag->qmc)");
        return -1;
    }

    /* Use QMC for defense optimization */
    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "QMC defense optimization requested");
    return 0;
}

int pag_qmcts_threat_response(nimcp_pag_t* pag, uint32_t num_iterations,
                              pag_defense_type_t* best_response) {
    if (!pag || !best_response || !pag->qmc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_qmc_optimize_defense: required parameter is NULL (pag, best_response, pag->qmc)");
        return -1;
    }

    NIMCP_LOG_DEBUG(PAG_LOG_TAG, "QMCTS threat response: %u iterations", num_iterations);

    /* Fallback to standard defense selection */
    nimcp_mutex_lock(pag->mutex);
    *best_response = select_defense_response(pag);
    nimcp_mutex_unlock(pag->mutex);

    return 0;
}

/*=============================================================================
 * UPDATE AND STATE
 *===========================================================================*/

int pag_update(nimcp_pag_t* pag, float dt) {
    if (!pag || !pag->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_update: required parameter is NULL (pag, pag->initialized)");
        return -1;
    }

    nimcp_mutex_lock(pag->mutex);

    /* Update timestamp */
    pag->last_update_us += (uint64_t)(dt * 1000000.0f);

    /* Decay column activities */
    float decay = pag->config.column_decay_rate * dt;
    for (int i = 0; i < PAG_COLUMN_COUNT; i++) {
        /* Decay toward baseline */
        float diff = pag->columns[i].activity - pag->columns[i].baseline;
        pag->columns[i].activity -= diff * decay;

        /* Clear excitation/inhibition for next cycle */
        pag->columns[i].excitation *= (1.0f - decay);
        pag->columns[i].inhibition *= (1.0f - decay);

        /* Update active flag */
        pag->columns[i].active = (pag->columns[i].activity > pag->config.threat_threshold);

        /* Track average activity */
        pag->stats.avg_column_activity[i] =
            pag->stats.avg_column_activity[i] * 0.99f +
            pag->columns[i].activity * 0.01f;
    }

    /* Decay defensive state if no threat */
    if (pag->defense.threat_level == PAG_THREAT_NONE && pag->defense.defense_active) {
        pag->defense.defense_intensity *= (1.0f - pag->config.defense_decay_rate * dt);
        if (pag->defense.defense_intensity < 0.05f) {
            pag->defense.defense_active = false;
        }
    }

    /* Update defense duration */
    if (pag->defense.defense_active) {
        pag->defense.duration_us += (uint64_t)(dt * 1000000.0f);
    }

    /* Decay stress-induced analgesia */
    if (pag->analgesia.stress_induced_factor > 0.0f) {
        pag->analgesia.stress_induced_factor *= (1.0f - 0.02f * dt);
    }

    /* Compute derived states */
    compute_autonomic_output(pag);
    compute_analgesia(pag, dt);
    update_emotional_state(pag);

    /* Track total analgesia time */
    if (pag->analgesia.analgesia_level > 0.3f) {
        pag->stats.total_analgesia_time_us += (uint64_t)(dt * 1000000.0f);
        pag->stats.avg_analgesia_level =
            pag->stats.avg_analgesia_level * 0.99f +
            pag->analgesia.analgesia_level * 0.01f;
    }

    /* Decay vocalization */
    if (pag->vocal.active) {
        pag->vocal.intensity *= (1.0f - 0.1f * dt);
        if (pag->vocal.intensity < 0.05f) {
            pag->vocal.active = false;
        }
    }

    /* Track autonomic averages */
    pag->stats.avg_heart_rate_modulation =
        pag->stats.avg_heart_rate_modulation * 0.99f +
        pag->autonomic.heart_rate_modulation * 0.01f;
    pag->stats.avg_respiratory_modulation =
        pag->stats.avg_respiratory_modulation * 0.99f +
        pag->autonomic.respiratory_rate_mod * 0.01f;

    /* Broadcast autonomic changes */
    if (pag->bio_router && pag->config.enable_bio_async) {
        pag_bio_async_broadcast(pag, PAG_BIO_MSG_AUTONOMIC_CHANGE,
                                &pag->autonomic, sizeof(pag_autonomic_state_t));
    }

    nimcp_mutex_unlock(pag->mutex);

    return 0;
}

int pag_get_stats(const nimcp_pag_t* pag, pag_stats_t* stats) {
    if (!pag || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_get_stats: required parameter is NULL (pag, stats)");
        return -1;
    }

    nimcp_mutex_lock(((nimcp_pag_t*)pag)->mutex);
    memcpy(stats, &pag->stats, sizeof(pag_stats_t));
    nimcp_mutex_unlock(((nimcp_pag_t*)pag)->mutex);

    return 0;
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

const char* pag_column_string(pag_column_t column) {
    static const char* names[] = {
        "Dorsolateral",
        "Lateral",
        "Dorsomedial",
        "Ventrolateral"
    };
    if (column >= PAG_COLUMN_COUNT) return "Unknown";
    return names[column];
}

const char* pag_defense_string(pag_defense_type_t defense) {
    static const char* names[] = {
        "Fight",
        "Flight",
        "Freeze",
        "Fawn"
    };
    if (defense >= PAG_DEFENSE_COUNT) return "Unknown";
    return names[defense];
}

const char* pag_threat_string(pag_threat_level_t level) {
    static const char* names[] = {
        "Distal",
        "Proximal",
        "Imminent",
        "Contact",
        "None"
    };
    if (level > PAG_THREAT_NONE) return "Unknown";
    return names[level];
}

const char* pag_emotion_string(pag_emotion_type_t emotion) {
    static const char* names[] = {
        "Fear",
        "Rage",
        "Pain",
        "Panic",
        "Maternal",
        "Reproductive"
    };
    if (emotion >= PAG_EMOTION_COUNT) return "Unknown";
    return names[emotion];
}

const char* pag_vocal_string(pag_vocal_type_t vocal) {
    static const char* names[] = {
        "None",
        "Alarm",
        "Aggression",
        "Submission",
        "Distress",
        "Pleasure",
        "Startle"
    };
    if (vocal >= PAG_VOCAL_COUNT) return "Unknown";
    return names[vocal];
}

const char* pag_pain_pathway_string(pag_pain_pathway_t pathway) {
    static const char* names[] = {
        "Opioid",
        "Non-Opioid",
        "Cannabinoid",
        "Serotonergic",
        "Noradrenergic"
    };
    if (pathway >= PAG_PAIN_PATHWAY_COUNT) return "Unknown";
    return names[pathway];
}

const char* pag_coping_string(pag_coping_strategy_t coping) {
    static const char* names[] = {
        "Active",
        "Passive",
        "Mixed"
    };
    if (coping > PAG_COPING_MIXED) return "Unknown";
    return names[coping];
}

nimcp_mutex_t* pag_get_mutex(nimcp_pag_t* pag) {
    if (!pag) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag is NULL");

        return NULL;

    }
    return pag->mutex;
}
