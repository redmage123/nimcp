/**
 * @file nimcp_cochlea_medulla_bridge.c
 * @brief Cochlea-Medulla brainstem integration implementation
 *
 * WHAT: Connect cochlear processing to brainstem nuclei and protective reflexes
 * WHY:  Enable biological auditory pathway and protective mechanisms
 * HOW:  Cochlear nucleus -> Superior olive -> Inferior colliculus pathway
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "perception/bridges/nimcp_cochlea_medulla_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cochlea_medulla_bridge)

#define LOG_MODULE "COCHLEA_MEDULLA_BRIDGE"

//=============================================================================
// Internal Structure
//=============================================================================

struct cochlea_medulla_bridge {
    bridge_base_t base;                     /**< MUST be first */
    cochlea_medulla_config_t config;

    /* Connected systems */
    cochlea_t* cochlea;
    medulla_t* medulla;

    /* Brainstem nuclei outputs */
    cn_output_t cn_out;
    soc_output_t soc_out;
    ic_output_t ic_out;

    /* Acoustic reflex */
    acoustic_reflex_state_t reflex;
    protection_level_t protection_level;

    /* Arousal state */
    arousal_level_t arousal_level;
    float arousal_gain_db;

    /* Circadian */
    circadian_phase_t circadian_phase;
    uint32_t circadian_hour;
    float circadian_sensitivity;

    /* Bidirectional timestamps */
    uint64_t last_outbound_ms;
    uint64_t last_inbound_ms;

    /* Accumulated time */
    float accumulated_time_ms;
};

//=============================================================================
// Helper: Get current time in milliseconds
//=============================================================================

static uint64_t medulla_bridge_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

//=============================================================================
// Helper: Compute arousal gain
//=============================================================================

static float compute_arousal_gain(arousal_level_t level, float range_db) {
    float factor = 0.0f;
    switch (level) {
        case AROUSAL_LEVEL_SLEEP:    factor = -1.0f;  break;
        case AROUSAL_LEVEL_DROWSY:   factor = -0.5f;  break;
        case AROUSAL_LEVEL_RELAXED:  factor = 0.0f;   break;
        case AROUSAL_LEVEL_ALERT:    factor = 0.3f;   break;
        case AROUSAL_LEVEL_VIGILANT: factor = 0.7f;   break;
        case AROUSAL_LEVEL_STARTLE:  factor = 1.0f;   break;
        default:                     factor = 0.0f;   break;
    }
    return factor * range_db;
}

//=============================================================================
// Helper: Compute circadian sensitivity
//=============================================================================

static float compute_circadian_sensitivity(circadian_phase_t phase) {
    switch (phase) {
        case CIRCADIAN_NIGHT:       return 0.7f;
        case CIRCADIAN_DAWN:        return 0.8f;
        case CIRCADIAN_MORNING:     return 1.0f;
        case CIRCADIAN_MIDDAY:      return 1.2f;
        case CIRCADIAN_AFTERNOON:   return 1.1f;
        case CIRCADIAN_EVENING:     return 0.9f;
        case CIRCADIAN_DUSK:        return 0.8f;
        default:                    return 1.0f;
    }
}

//=============================================================================
// Helper: Map hour to circadian phase
//=============================================================================

static circadian_phase_t hour_to_phase(uint32_t hour) {
    if (hour >= 23 || hour < 5)  return CIRCADIAN_NIGHT;
    if (hour < 7)                return CIRCADIAN_DAWN;
    if (hour < 10)               return CIRCADIAN_MORNING;
    if (hour < 14)               return CIRCADIAN_MIDDAY;
    if (hour < 17)               return CIRCADIAN_AFTERNOON;
    if (hour < 20)               return CIRCADIAN_EVENING;
    return CIRCADIAN_DUSK;
}

//=============================================================================
// Helper: Free nucleus output arrays
//=============================================================================

static void free_cn_output(cn_output_t* cn) {
    if (cn->bushy_cell_output) { nimcp_free(cn->bushy_cell_output); cn->bushy_cell_output = NULL; }
    if (cn->stellate_cell_output) { nimcp_free(cn->stellate_cell_output); cn->stellate_cell_output = NULL; }
    if (cn->octopus_cell_output) { nimcp_free(cn->octopus_cell_output); cn->octopus_cell_output = NULL; }
    cn->num_channels = 0;
}

static void free_soc_output(soc_output_t* soc) {
    if (soc->mso_output) { nimcp_free(soc->mso_output); soc->mso_output = NULL; }
    if (soc->lso_output) { nimcp_free(soc->lso_output); soc->lso_output = NULL; }
    soc->num_channels = 0;
}

static void free_ic_output(ic_output_t* ic) {
    if (ic->frequency_map) { nimcp_free(ic->frequency_map); ic->frequency_map = NULL; }
    if (ic->amplitude_map) { nimcp_free(ic->amplitude_map); ic->amplitude_map = NULL; }
    ic->num_channels = 0;
}

//=============================================================================
// Helper: Allocate nucleus output arrays
//=============================================================================

static int alloc_cn_output(cn_output_t* cn, uint32_t channels) {
    cn->num_channels = channels;
    cn->bushy_cell_output = (float*)nimcp_calloc(channels, sizeof(float));
    cn->stellate_cell_output = (float*)nimcp_calloc(channels, sizeof(float));
    cn->octopus_cell_output = (float*)nimcp_calloc(channels, sizeof(float));
    if (!cn->bushy_cell_output || !cn->stellate_cell_output || !cn->octopus_cell_output) {
        free_cn_output(cn);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "alloc_cn_output: required parameter is NULL (cn->bushy_cell_output, cn->stellate_cell_output, cn->octopus_cell_output)");
        return -1;
    }
    return 0;
}

static int alloc_soc_output(soc_output_t* soc, uint32_t channels) {
    soc->num_channels = channels;
    soc->mso_output = (float*)nimcp_calloc(channels, sizeof(float));
    soc->lso_output = (float*)nimcp_calloc(channels, sizeof(float));
    soc->itd_estimate_us = 0.0f;
    soc->ild_estimate_db = 0.0f;
    soc->azimuth_deg = 0.0f;
    if (!soc->mso_output || !soc->lso_output) {
        free_soc_output(soc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "alloc_soc_output: required parameter is NULL (soc->mso_output, soc->lso_output)");
        return -1;
    }
    return 0;
}

static int alloc_ic_output(ic_output_t* ic, uint32_t channels) {
    ic->num_channels = channels;
    ic->frequency_map = (float*)nimcp_calloc(channels, sizeof(float));
    ic->amplitude_map = (float*)nimcp_calloc(channels, sizeof(float));
    ic->dominant_frequency_hz = 0.0f;
    ic->overall_intensity_db = 0.0f;
    if (!ic->frequency_map || !ic->amplitude_map) {
        free_ic_output(ic);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "alloc_ic_output: required parameter is NULL (ic->frequency_map, ic->amplitude_map)");
        return -1;
    }
    return 0;
}

//=============================================================================
// Configuration
//=============================================================================

cochlea_medulla_config_t cochlea_medulla_config_default(void) {
    cochlea_medulla_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    /* Reflex parameters */
    cfg.reflex_threshold_db = COCHLEA_MEDULLA_REFLEX_THRESHOLD_DB;
    cfg.reflex_latency_ms = 70.0f;   /* Typical latency */
    cfg.max_attenuation_db = COCHLEA_MEDULLA_REFLEX_MAX_ATTEN_DB;

    /* Arousal coupling */
    cfg.enable_arousal_coupling = true;
    cfg.arousal_gain_range_db = 6.0f;

    /* Circadian coupling */
    cfg.enable_circadian_coupling = true;
    for (int h = 0; h < 24; h++) {
        /* Default sensitivity curve: peaks midday */
        float norm = (float)h / 24.0f;
        cfg.circadian_sensitivity[h] = 0.8f + 0.4f * sinf(norm * NIMCP_TWO_PI_F - 1.5708f);
    }

    /* Processing options */
    cfg.enable_cn_simulation = true;
    cfg.enable_soc_binaural = true;
    cfg.enable_ic_integration = true;

    return cfg;
}

//=============================================================================
// Core API
//=============================================================================

cochlea_medulla_bridge_t* cochlea_medulla_bridge_create(
    cochlea_t* cochlea,
    medulla_t* medulla,
    const cochlea_medulla_config_t* config)
{
    cochlea_medulla_bridge_heartbeat("create", 0.0f);

    cochlea_medulla_bridge_t* bridge =
        (cochlea_medulla_bridge_t*)nimcp_calloc(1, sizeof(cochlea_medulla_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "cochlea_medulla_bridge_create: alloc failed");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = cochlea_medulla_config_default();
    }

    if (bridge_base_init(&bridge->base, 0, "cochlea_medulla_bridge") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cochlea_medulla_bridge_create: validation failed");
        return NULL;
    }

    bridge->cochlea = cochlea;
    bridge->medulla = medulla;
    bridge->arousal_level = AROUSAL_LEVEL_ALERT;
    bridge->arousal_gain_db = 0.0f;
    bridge->circadian_phase = CIRCADIAN_MIDDAY;
    bridge->circadian_hour = 12;
    bridge->circadian_sensitivity = 1.0f;
    bridge->protection_level = PROTECTION_NONE;
    bridge->accumulated_time_ms = 0.0f;

    /* Connect systems via base */
    if (cochlea) {
        bridge_base_connect_a_unlocked(&bridge->base, cochlea);
    }
    if (medulla) {
        bridge_base_connect_b_unlocked(&bridge->base, medulla);
    }

    /* Allocate nuclei output arrays */
    if (bridge->config.enable_cn_simulation) {
        if (alloc_cn_output(&bridge->cn_out, COCHLEA_MEDULLA_CN_CHANNELS) != 0) {
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                "cochlea_medulla_bridge_create: CN alloc failed");
            return NULL;
        }
    }

    if (bridge->config.enable_soc_binaural) {
        if (alloc_soc_output(&bridge->soc_out, COCHLEA_MEDULLA_SOC_CHANNELS) != 0) {
            free_cn_output(&bridge->cn_out);
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                "cochlea_medulla_bridge_create: SOC alloc failed");
            return NULL;
        }
    }

    if (bridge->config.enable_ic_integration) {
        if (alloc_ic_output(&bridge->ic_out, COCHLEA_MEDULLA_IC_CHANNELS) != 0) {
            free_cn_output(&bridge->cn_out);
            free_soc_output(&bridge->soc_out);
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                "cochlea_medulla_bridge_create: IC alloc failed");
            return NULL;
        }
    }

    /* Initialize reflex state */
    memset(&bridge->reflex, 0, sizeof(acoustic_reflex_state_t));
    bridge->reflex.decay_time_constant_ms = 500.0f;

    cochlea_medulla_bridge_heartbeat("create", 1.0f);
    return bridge;
}

void cochlea_medulla_bridge_destroy(cochlea_medulla_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "cochlea_medulla");
    cochlea_medulla_bridge_heartbeat("destroy", 0.0f);

    free_cn_output(&bridge->cn_out);
    free_soc_output(&bridge->soc_out);
    free_ic_output(&bridge->ic_out);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

nimcp_error_t cochlea_medulla_bridge_update(
    cochlea_medulla_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_bridge_update: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!cochlea_output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_bridge_update: cochlea_output NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_medulla_bridge_heartbeat("update", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->accumulated_time_ms += dt_ms;

    /* Update acoustic reflex decay */
    if (bridge->reflex.reflex_triggered) {
        bridge->reflex.reflex_duration_ms += dt_ms;

        /* Decay attenuation over time */
        float decay_factor = expf(-dt_ms / bridge->reflex.decay_time_constant_ms);
        bridge->reflex.current_attenuation_db *= decay_factor;

        if (bridge->reflex.current_attenuation_db < 0.1f) {
            bridge->reflex.reflex_triggered = false;
            bridge->reflex.current_attenuation_db = 0.0f;
            bridge->protection_level = PROTECTION_NONE;
        }
    }

    /* Update arousal gain */
    if (bridge->config.enable_arousal_coupling) {
        bridge->arousal_gain_db = compute_arousal_gain(
            bridge->arousal_level, bridge->config.arousal_gain_range_db);
    }

    /* Update circadian sensitivity */
    if (bridge->config.enable_circadian_coupling) {
        bridge->circadian_sensitivity = compute_circadian_sensitivity(bridge->circadian_phase);
    }

    /* Record outbound timestamp (cochlea -> medulla) */
    bridge->last_outbound_ms = medulla_bridge_time_ms();

    bridge_base_record_update(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_medulla_bridge_heartbeat("update", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_medulla_bridge_reset(cochlea_medulla_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_bridge_reset: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_medulla_bridge_heartbeat("reset", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset nuclei outputs */
    if (bridge->cn_out.bushy_cell_output) {
        memset(bridge->cn_out.bushy_cell_output, 0, bridge->cn_out.num_channels * sizeof(float));
    }
    if (bridge->cn_out.stellate_cell_output) {
        memset(bridge->cn_out.stellate_cell_output, 0, bridge->cn_out.num_channels * sizeof(float));
    }
    if (bridge->cn_out.octopus_cell_output) {
        memset(bridge->cn_out.octopus_cell_output, 0, bridge->cn_out.num_channels * sizeof(float));
    }

    if (bridge->soc_out.mso_output) {
        memset(bridge->soc_out.mso_output, 0, bridge->soc_out.num_channels * sizeof(float));
    }
    if (bridge->soc_out.lso_output) {
        memset(bridge->soc_out.lso_output, 0, bridge->soc_out.num_channels * sizeof(float));
    }
    bridge->soc_out.itd_estimate_us = 0.0f;
    bridge->soc_out.ild_estimate_db = 0.0f;
    bridge->soc_out.azimuth_deg = 0.0f;

    if (bridge->ic_out.frequency_map) {
        memset(bridge->ic_out.frequency_map, 0, bridge->ic_out.num_channels * sizeof(float));
    }
    if (bridge->ic_out.amplitude_map) {
        memset(bridge->ic_out.amplitude_map, 0, bridge->ic_out.num_channels * sizeof(float));
    }
    bridge->ic_out.dominant_frequency_hz = 0.0f;
    bridge->ic_out.overall_intensity_db = 0.0f;

    /* Reset reflex */
    memset(&bridge->reflex, 0, sizeof(acoustic_reflex_state_t));
    bridge->reflex.decay_time_constant_ms = 500.0f;
    bridge->protection_level = PROTECTION_NONE;

    /* Reset arousal to alert */
    bridge->arousal_level = AROUSAL_LEVEL_ALERT;
    bridge->arousal_gain_db = 0.0f;

    /* Reset circadian */
    bridge->circadian_phase = CIRCADIAN_MIDDAY;
    bridge->circadian_hour = 12;
    bridge->circadian_sensitivity = 1.0f;

    bridge->accumulated_time_ms = 0.0f;
    bridge->last_outbound_ms = 0;
    bridge->last_inbound_ms = 0;

    bridge_base_reset_unlocked(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_medulla_bridge_heartbeat("reset", 1.0f);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Protective Functions
//=============================================================================

nimcp_error_t cochlea_medulla_trigger_protective_cutoff(
    cochlea_medulla_bridge_t* bridge,
    float sound_level_db,
    protection_level_t level)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_trigger_protective_cutoff: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_medulla_bridge_heartbeat("trigger_protective_cutoff", 0.5f);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->reflex.reflex_triggered = true;
    bridge->reflex.trigger_level_db = sound_level_db;
    bridge->reflex.reflex_onset_time_ms = bridge->config.reflex_latency_ms;
    bridge->reflex.reflex_duration_ms = 0.0f;
    bridge->protection_level = level;

    /* Compute attenuation based on protection level */
    float max_atten = bridge->config.max_attenuation_db;
    switch (level) {
        case PROTECTION_NONE:       bridge->reflex.current_attenuation_db = 0.0f; break;
        case PROTECTION_MILD:       bridge->reflex.current_attenuation_db = max_atten * 0.25f; break;
        case PROTECTION_MODERATE:   bridge->reflex.current_attenuation_db = max_atten * 0.5f; break;
        case PROTECTION_SEVERE:     bridge->reflex.current_attenuation_db = max_atten * 0.75f; break;
        case PROTECTION_EMERGENCY:  bridge->reflex.current_attenuation_db = max_atten; break;
        default:                    bridge->reflex.current_attenuation_db = 0.0f; break;
    }

    /* Inbound: medulla tells cochlea to attenuate */
    bridge->last_inbound_ms = medulla_bridge_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

bool cochlea_medulla_is_protection_active(const cochlea_medulla_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_is_protection_active: bridge NULL");
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool active = bridge->reflex.reflex_triggered;
    nimcp_mutex_unlock(bridge->base.mutex);

    return active;
}

float cochlea_medulla_get_attenuation(const cochlea_medulla_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_get_attenuation: bridge NULL");
        return 0.0f;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float atten = bridge->reflex.current_attenuation_db;
    nimcp_mutex_unlock(bridge->base.mutex);

    return atten;
}

nimcp_error_t cochlea_medulla_get_reflex_state(
    const cochlea_medulla_bridge_t* bridge,
    acoustic_reflex_state_t* state)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_get_reflex_state: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_get_reflex_state: state NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->reflex;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Arousal Modulation
//=============================================================================

nimcp_error_t cochlea_medulla_set_arousal(
    cochlea_medulla_bridge_t* bridge,
    arousal_level_t level)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_set_arousal: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_medulla_bridge_heartbeat("set_arousal", 0.5f);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->arousal_level = level;
    if (bridge->config.enable_arousal_coupling) {
        bridge->arousal_gain_db = compute_arousal_gain(level, bridge->config.arousal_gain_range_db);
    }

    /* Inbound: arousal system modulates cochlea */
    bridge->last_inbound_ms = medulla_bridge_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

arousal_level_t cochlea_medulla_get_arousal(
    const cochlea_medulla_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_get_arousal: bridge NULL");
        return AROUSAL_LEVEL_ALERT;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    arousal_level_t level = bridge->arousal_level;
    nimcp_mutex_unlock(bridge->base.mutex);

    return level;
}

float cochlea_medulla_get_arousal_gain(
    const cochlea_medulla_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_get_arousal_gain: bridge NULL");
        return 0.0f;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float gain = bridge->arousal_gain_db;
    nimcp_mutex_unlock(bridge->base.mutex);

    return gain;
}

//=============================================================================
// Circadian Modulation
//=============================================================================

nimcp_error_t cochlea_medulla_set_circadian_phase(
    cochlea_medulla_bridge_t* bridge,
    circadian_phase_t phase)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_set_circadian_phase: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_medulla_bridge_heartbeat("set_circadian_phase", 0.5f);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->circadian_phase = phase;
    if (bridge->config.enable_circadian_coupling) {
        bridge->circadian_sensitivity = compute_circadian_sensitivity(phase);
    }

    /* Inbound: circadian clock modulates cochlea */
    bridge->last_inbound_ms = medulla_bridge_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_medulla_set_circadian_hour(
    cochlea_medulla_bridge_t* bridge,
    uint32_t hour)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_set_circadian_hour: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (hour > 23) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "cochlea_medulla_set_circadian_hour: hour > 23");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    cochlea_medulla_bridge_heartbeat("set_circadian_hour", 0.5f);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->circadian_hour = hour;
    bridge->circadian_phase = hour_to_phase(hour);

    if (bridge->config.enable_circadian_coupling) {
        /* Use per-hour sensitivity from config */
        bridge->circadian_sensitivity = bridge->config.circadian_sensitivity[hour];
    }

    bridge->last_inbound_ms = medulla_bridge_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

float cochlea_medulla_get_circadian_sensitivity(
    const cochlea_medulla_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_get_circadian_sensitivity: bridge NULL");
        return 1.0f;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float sens = bridge->circadian_sensitivity;
    nimcp_mutex_unlock(bridge->base.mutex);

    return sens;
}

//=============================================================================
// Brainstem Nucleus Access
//=============================================================================

nimcp_error_t cochlea_medulla_get_cn_output(
    const cochlea_medulla_bridge_t* bridge,
    cn_output_t* output)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_get_cn_output: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_get_cn_output: output NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Shallow copy: share pointers (read-only access) */
    *output = bridge->cn_out;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_medulla_get_soc_output(
    const cochlea_medulla_bridge_t* bridge,
    soc_output_t* output)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_get_soc_output: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_get_soc_output: output NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Shallow copy: share pointers (read-only access) */
    *output = bridge->soc_out;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_medulla_get_ic_output(
    const cochlea_medulla_bridge_t* bridge,
    ic_output_t* output)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_get_ic_output: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_get_ic_output: output NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Shallow copy: share pointers (read-only access) */
    *output = bridge->ic_out;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_medulla_verify_bidirectional(
    const cochlea_medulla_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_verify_bidirectional: bridge NULL");
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool bidir = (bridge->last_outbound_ms > 0) && (bridge->last_inbound_ms > 0);
    nimcp_mutex_unlock(bridge->base.mutex);

    return bidir;
}

uint64_t cochlea_medulla_get_last_outbound(
    const cochlea_medulla_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_get_last_outbound: bridge NULL");
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_outbound_ms;
    nimcp_mutex_unlock(bridge->base.mutex);

    return ts;
}

uint64_t cochlea_medulla_get_last_inbound(
    const cochlea_medulla_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_medulla_get_last_inbound: bridge NULL");
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_inbound_ms;
    nimcp_mutex_unlock(bridge->base.mutex);

    return ts;
}
