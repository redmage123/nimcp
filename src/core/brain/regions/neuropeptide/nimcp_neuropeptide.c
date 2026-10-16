/**
 * @file nimcp_neuropeptide.c
 * @brief Neuropeptide System - Core Implementation
 * @version 1.0.0
 * @date 2026-03-05
 *
 * WHAT: Kinetic simulation of 8 neuropeptides with receptor binding dynamics
 * WHY:  Neuropeptides provide slow neuromodulation for behavioral drives
 * HOW:  First-order kinetics for synthesis/degradation + Hill equation for binding
 *
 * @author NIMCP Development Team
 */

#include "core/brain/regions/neuropeptide/nimcp_neuropeptide.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/thread/nimcp_thread.h"

#include <string.h>
#include <math.h>
#include <time.h>

/*=============================================================================
 * Logging & Boilerplate
 *===========================================================================*/

#define LOG_MODULE "NEUROPEPTIDE"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(neuropeptide, MESH_ADAPTER_CATEGORY_SUBCORTICAL)

/*=============================================================================
 * Helpers
 *===========================================================================*/

static float nimcp_clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static uint64_t npt_get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static bool npt_valid_type(neuropeptide_type_t type) {
    return (type >= NPT_OXYTOCIN && type < NPT_COUNT);
}

/*=============================================================================
 * Default Configuration
 *===========================================================================*/

npt_config_t npt_default_config(void) {
    npt_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    /*
     * Biologically motivated defaults:
     * - Synthesis rates reflect peptide production speed
     * - Degradation rates reflect enzymatic breakdown (peptidases)
     * - Release thresholds reflect firing rate needed for vesicular exocytosis
     * - Kd values reflect receptor affinity (lower = higher affinity)
     * - Gains reflect coupling strength to downstream pathways
     */

    /* Oxytocin: moderate synthesis, slow degradation, low threshold */
    cfg.base_synthesis_rates[NPT_OXYTOCIN]   = 0.5f;
    cfg.degradation_rates[NPT_OXYTOCIN]      = 0.02f;
    cfg.release_thresholds[NPT_OXYTOCIN]     = 8.0f;
    cfg.kd_values[NPT_OXYTOCIN]              = 10.0f;
    cfg.gains[NPT_OXYTOCIN]                  = 1.0f;

    /* Vasopressin: fast synthesis under stress, moderate degradation */
    cfg.base_synthesis_rates[NPT_VASOPRESSIN] = 0.6f;
    cfg.degradation_rates[NPT_VASOPRESSIN]    = 0.03f;
    cfg.release_thresholds[NPT_VASOPRESSIN]   = 10.0f;
    cfg.kd_values[NPT_VASOPRESSIN]            = 8.0f;
    cfg.gains[NPT_VASOPRESSIN]                = 1.0f;

    /* NPY: slow synthesis, very slow degradation (persistent anxiolysis) */
    cfg.base_synthesis_rates[NPT_NPY]        = 0.3f;
    cfg.degradation_rates[NPT_NPY]           = 0.01f;
    cfg.release_thresholds[NPT_NPY]          = 5.0f;
    cfg.kd_values[NPT_NPY]                   = 12.0f;
    cfg.gains[NPT_NPY]                       = 0.8f;

    /* Substance P: fast synthesis, fast degradation (acute pain) */
    cfg.base_synthesis_rates[NPT_SUBSTANCE_P] = 1.0f;
    cfg.degradation_rates[NPT_SUBSTANCE_P]    = 0.08f;
    cfg.release_thresholds[NPT_SUBSTANCE_P]   = 15.0f;
    cfg.kd_values[NPT_SUBSTANCE_P]            = 5.0f;
    cfg.gains[NPT_SUBSTANCE_P]                = 1.2f;

    /* Orexin: moderate synthesis, moderate degradation */
    cfg.base_synthesis_rates[NPT_OREXIN]     = 0.4f;
    cfg.degradation_rates[NPT_OREXIN]        = 0.03f;
    cfg.release_thresholds[NPT_OREXIN]       = 7.0f;
    cfg.kd_values[NPT_OREXIN]               = 10.0f;
    cfg.gains[NPT_OREXIN]                   = 1.0f;

    /* CRH: fast synthesis under stress, moderate degradation */
    cfg.base_synthesis_rates[NPT_CRH]        = 0.8f;
    cfg.degradation_rates[NPT_CRH]           = 0.05f;
    cfg.release_thresholds[NPT_CRH]          = 12.0f;
    cfg.kd_values[NPT_CRH]                   = 8.0f;
    cfg.gains[NPT_CRH]                       = 1.0f;

    /* Endorphin: slow synthesis, slow degradation (sustained analgesia) */
    cfg.base_synthesis_rates[NPT_ENDORPHIN]  = 0.2f;
    cfg.degradation_rates[NPT_ENDORPHIN]     = 0.015f;
    cfg.release_thresholds[NPT_ENDORPHIN]    = 20.0f;
    cfg.kd_values[NPT_ENDORPHIN]             = 6.0f;
    cfg.gains[NPT_ENDORPHIN]                 = 1.5f;

    /* CCK: moderate synthesis, moderate degradation */
    cfg.base_synthesis_rates[NPT_CCK]        = 0.5f;
    cfg.degradation_rates[NPT_CCK]           = 0.04f;
    cfg.release_thresholds[NPT_CCK]          = 10.0f;
    cfg.kd_values[NPT_CCK]                   = 10.0f;
    cfg.gains[NPT_CCK]                       = 1.0f;

    return cfg;
}

/*=============================================================================
 * Lifecycle
 *===========================================================================*/

neuropeptide_system_t* npt_create(const npt_config_t* config) {
    neuropeptide_system_t* sys = (neuropeptide_system_t*)nimcp_calloc(
        1, sizeof(neuropeptide_system_t));
    if (!sys) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "npt_create: failed to allocate neuropeptide_system_t");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        sys->config = *config;
    } else {
        sys->config = npt_default_config();
    }

    /* Initialize peptide states from config */
    for (int i = 0; i < NPT_COUNT; i++) {
        sys->peptides[i].concentration     = 0.0f;
        sys->peptides[i].synthesis_rate    = sys->config.base_synthesis_rates[i];
        sys->peptides[i].degradation_rate  = sys->config.degradation_rates[i];
        sys->peptides[i].release_threshold = sys->config.release_thresholds[i];
        sys->peptides[i].receptor_occupancy = 0.0f;
        sys->peptides[i].downstream_effect  = 0.0f;
        sys->peptides[i].firing_rate        = 0.0f;
        sys->peptides[i].kd                 = sys->config.kd_values[i];
        sys->peptides[i].gain               = sys->config.gains[i];
    }

    /* Create mutex for thread safety */
    sys->lock = nimcp_mutex_create(NULL);
    if (!sys->lock) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "npt_create: failed to create mutex");
        nimcp_free(sys);
        return NULL;
    }

    sys->magic = NPT_MAGIC;
    sys->last_update_us = npt_get_timestamp_us();
    sys->update_count = 0;
    sys->initialized = true;

    /* Derived drives start at zero */
    sys->social_drive = 0.0f;
    sys->stress_level = 0.0f;
    sys->wakefulness  = 0.0f;
    sys->pain_level   = 0.0f;
    sys->satiety      = 0.0f;
    sys->euphoria     = 0.0f;

    LOG_INFO(LOG_MODULE, "Neuropeptide system created (%d peptides)", NPT_COUNT);
    return sys;
}

void npt_destroy(neuropeptide_system_t* system) {
    if (!system) {
        return;
    }
    if (system->magic != NPT_MAGIC) {
        LOG_WARN(LOG_MODULE, "npt_destroy: invalid magic (corruption?)");
        return;
    }

    system->magic = 0;
    system->initialized = false;

    if (system->lock) {
        nimcp_mutex_destroy(system->lock);
        system->lock = NULL;
    }

    nimcp_free(system);
    LOG_INFO(LOG_MODULE, "Neuropeptide system destroyed");
}

/*=============================================================================
 * Core Simulation
 *===========================================================================*/

npt_error_t npt_update(neuropeptide_system_t* system, float dt_s) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "npt_update: system is NULL");
        return NPT_ERR_NULL_PTR;
    }
    if (!system->initialized || system->magic != NPT_MAGIC) {
        return NPT_ERR_NOT_INIT;
    }
    if (!isfinite(dt_s) || dt_s <= 0.0f) {
        return NPT_ERR_INVALID_PARAM;
    }

    nimcp_mutex_lock(system->lock);

    for (int i = 0; i < NPT_COUNT; i++) {
        neuropeptide_state_t* p = &system->peptides[i];

        /* Step 1: Activity-dependent synthesis (vesicular release) */
        if (isfinite(p->firing_rate) && p->firing_rate >= p->release_threshold) {
            float delta = p->synthesis_rate * dt_s;
            if (isfinite(delta)) {
                p->concentration += delta;
            }
        }

        /* Step 2: Enzymatic degradation (first-order kinetics) */
        float decay = p->degradation_rate * p->concentration * dt_s;
        if (isfinite(decay)) {
            p->concentration -= decay;
        }

        /* Step 3: Clamp concentration to valid range */
        p->concentration = nimcp_clampf(p->concentration, 0.0f, NPT_MAX_CONCENTRATION);

        /* Step 4: Compute receptor occupancy (Hill/Michaelis-Menten) */
        /* occupancy = [C] / ([C] + Kd) */
        float kd = p->kd > 0.0f ? p->kd : NPT_DEFAULT_KD;
        float denom = p->concentration + kd;
        if (denom > 0.0f) {
            p->receptor_occupancy = p->concentration / denom;
        } else {
            p->receptor_occupancy = 0.0f;
        }
        p->receptor_occupancy = nimcp_clampf(p->receptor_occupancy, 0.0f, 1.0f);

        /* Step 5: Compute downstream effect */
        float gain = isfinite(p->gain) ? p->gain : NPT_DEFAULT_GAIN;
        p->downstream_effect = p->receptor_occupancy * gain;
        p->downstream_effect = nimcp_clampf(p->downstream_effect, 0.0f, 10.0f);
    }

    /* Update derived behavioral drives from peptide effects */
    system->social_drive = system->peptides[NPT_OXYTOCIN].downstream_effect;
    system->stress_level = system->peptides[NPT_CRH].downstream_effect;
    system->wakefulness  = system->peptides[NPT_OREXIN].downstream_effect;
    system->pain_level   = system->peptides[NPT_SUBSTANCE_P].downstream_effect;
    system->satiety      = system->peptides[NPT_CCK].downstream_effect;
    system->euphoria     = system->peptides[NPT_ENDORPHIN].downstream_effect;

    system->last_update_us = npt_get_timestamp_us();
    system->update_count++;

    nimcp_mutex_unlock(system->lock);
    return NPT_OK;
}

/*=============================================================================
 * Accessors
 *===========================================================================*/

npt_error_t npt_get_concentration(
    const neuropeptide_system_t* system,
    neuropeptide_type_t type,
    float* concentration)
{
    if (!system || !concentration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "npt_get_concentration: NULL argument");
        return NPT_ERR_NULL_PTR;
    }
    if (!npt_valid_type(type)) {
        return NPT_ERR_INVALID_TYPE;
    }
    if (!system->initialized || system->magic != NPT_MAGIC) {
        return NPT_ERR_NOT_INIT;
    }

    *concentration = system->peptides[type].concentration;
    return NPT_OK;
}

npt_error_t npt_get_downstream_effect(
    const neuropeptide_system_t* system,
    neuropeptide_type_t type,
    float* effect)
{
    if (!system || !effect) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "npt_get_downstream_effect: NULL argument");
        return NPT_ERR_NULL_PTR;
    }
    if (!npt_valid_type(type)) {
        return NPT_ERR_INVALID_TYPE;
    }
    if (!system->initialized || system->magic != NPT_MAGIC) {
        return NPT_ERR_NOT_INIT;
    }

    *effect = system->peptides[type].downstream_effect;
    return NPT_OK;
}

npt_error_t npt_stimulate_release(
    neuropeptide_system_t* system,
    neuropeptide_type_t type,
    float stimulus)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "npt_stimulate_release: system is NULL");
        return NPT_ERR_NULL_PTR;
    }
    if (!npt_valid_type(type)) {
        return NPT_ERR_INVALID_TYPE;
    }
    if (!system->initialized || system->magic != NPT_MAGIC) {
        return NPT_ERR_NOT_INIT;
    }
    if (!isfinite(stimulus) || stimulus < 0.0f) {
        return NPT_ERR_INVALID_PARAM;
    }

    nimcp_mutex_lock(system->lock);
    system->peptides[type].concentration += stimulus;
    system->peptides[type].concentration = nimcp_clampf(
        system->peptides[type].concentration, 0.0f, NPT_MAX_CONCENTRATION);
    nimcp_mutex_unlock(system->lock);

    return NPT_OK;
}

npt_error_t npt_set_firing_rate(
    neuropeptide_system_t* system,
    neuropeptide_type_t type,
    float rate)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "npt_set_firing_rate: system is NULL");
        return NPT_ERR_NULL_PTR;
    }
    if (!npt_valid_type(type)) {
        return NPT_ERR_INVALID_TYPE;
    }
    if (!system->initialized || system->magic != NPT_MAGIC) {
        return NPT_ERR_NOT_INIT;
    }
    if (!isfinite(rate)) {
        return NPT_ERR_INVALID_PARAM;
    }

    nimcp_mutex_lock(system->lock);
    system->peptides[type].firing_rate = nimcp_clampf(rate, 0.0f, 200.0f);
    nimcp_mutex_unlock(system->lock);

    return NPT_OK;
}

npt_error_t npt_get_all_states(
    const neuropeptide_system_t* system,
    neuropeptide_state_t states[NPT_COUNT])
{
    if (!system || !states) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "npt_get_all_states: NULL argument");
        return NPT_ERR_NULL_PTR;
    }
    if (!system->initialized || system->magic != NPT_MAGIC) {
        return NPT_ERR_NOT_INIT;
    }

    memcpy(states, system->peptides, sizeof(neuropeptide_state_t) * NPT_COUNT);
    return NPT_OK;
}

/*=============================================================================
 * Utility
 *===========================================================================*/

const char* npt_type_name(neuropeptide_type_t type) {
    switch (type) {
        case NPT_OXYTOCIN:    return "Oxytocin";
        case NPT_VASOPRESSIN: return "Vasopressin";
        case NPT_NPY:         return "NPY";
        case NPT_SUBSTANCE_P: return "Substance P";
        case NPT_OREXIN:      return "Orexin";
        case NPT_CRH:         return "CRH";
        case NPT_ENDORPHIN:   return "Endorphin";
        case NPT_CCK:         return "CCK";
        default:              return "UNKNOWN";
    }
}

const char* npt_error_string(npt_error_t error) {
    switch (error) {
        case NPT_OK:             return "Success";
        case NPT_ERR_NULL_PTR:   return "Null pointer";
        case NPT_ERR_INVALID_PARAM: return "Invalid parameter";
        case NPT_ERR_NOT_INIT:   return "Not initialized";
        case NPT_ERR_ALREADY_INIT: return "Already initialized";
        case NPT_ERR_NO_MEMORY:  return "Out of memory";
        case NPT_ERR_INVALID_TYPE: return "Invalid peptide type";
        case NPT_ERR_INVALID_STATE: return "Invalid state";
        default:                 return "Unknown error";
    }
}
