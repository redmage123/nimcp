/**
 * @file nimcp_endocannabinoid.c
 * @brief Endocannabinoid System (ECS) - Core Implementation
 * @date 2026-03-05
 *
 * Implements retrograde synaptic modulation via 2-AG and anandamide signaling.
 * Update logic:
 *   (1) 2-AG synthesis proportional to recent depolarization, degradation by MAGL
 *   (2) AEA tonic synthesis toward baseline, degradation by FAAH
 *   (3) DSI/DSE strength derived from 2-AG level via sigmoid
 *   (4) Tonic inhibition derived from AEA level
 * All outputs clamped to [0,1].
 */

#include "core/brain/regions/endocannabinoid/nimcp_endocannabinoid.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

#include <string.h>
#include <math.h>
#include <time.h>

#define LOG_MODULE "ENDOCANNABINOID"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(endocannabinoid, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* Lock field is void* in header for portability; cast to nimcp_mutex_t* here */
#define ECB_LOCK(sys)   nimcp_mutex_lock((nimcp_mutex_t*)(sys)->lock)
#define ECB_UNLOCK(sys) nimcp_mutex_unlock((nimcp_mutex_t*)(sys)->lock)

/*=============================================================================
 * Helpers
 *===========================================================================*/

static float nimcp_clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/**
 * @brief Sigmoid function mapping [0,1] input to [0,1] output
 * center=0.5, steepness=10 by default
 */
static float sigmoid01(float x, float center, float steepness) {
    float exponent = -steepness * (x - center);
    if (exponent > 80.0f) return 0.0f;
    if (exponent < -80.0f) return 1.0f;
    return 1.0f / (1.0f + expf(exponent));
}

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/*=============================================================================
 * Default Configuration
 *===========================================================================*/

ecb_config_t ecb_default_config(void) {
    ecb_config_t cfg = {
        .base_two_ag = 0.15f,       /* Low baseline 2-AG (activity-dependent) */
        .base_aea    = 0.30f,       /* Moderate baseline anandamide (tonic) */
        .magl_rate   = 2.0f,        /* 2-AG half-life ~350ms */
        .faah_rate   = 0.5f,        /* AEA half-life ~1.4s */
        .cb1_gain    = 1.0f,        /* Default CB1 sensitivity */
        .cb2_gain    = 1.0f         /* Default CB2 sensitivity */
    };
    return cfg;
}

/*=============================================================================
 * Lifecycle
 *===========================================================================*/

endocannabinoid_system_t* ecb_create(const ecb_config_t* config) {
    endocannabinoid_system_t* sys = nimcp_calloc(1, sizeof(endocannabinoid_system_t));
    if (!sys) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "ecb_create: failed to allocate endocannabinoid_system_t");
        return NULL;
    }

    sys->magic = ECB_MAGIC;

    /* Apply configuration */
    if (config) {
        sys->config = *config;
    } else {
        sys->config = ecb_default_config();
    }

    /* Initialize endocannabinoid levels to baseline */
    sys->two_ag_level = sys->config.base_two_ag;
    sys->aea_level    = sys->config.base_aea;

    /* Initialize enzyme activity at baseline rates */
    sys->magl_activity = 0.5f;
    sys->faah_activity = 0.5f;

    /* Initialize modulation outputs */
    sys->dsi_strength      = 0.0f;
    sys->dse_strength      = 0.0f;
    sys->tonic_inhibition  = 0.0f;

    sys->depolarization_accumulator = 0.0f;

    /* Set default CB1 density (high in cortex/hippocampus, moderate elsewhere) */
    for (int i = 0; i < ECB_NUM_REGIONS; i++) {
        sys->cb1_density[i] = 0.5f;     /* Moderate default */
        sys->cb2_density[i] = 0.1f;     /* Low default (immune-focused) */
    }
    /* Region 0 = cortex: high CB1 */
    sys->cb1_density[0] = 0.9f;
    /* Region 1 = hippocampus: high CB1 */
    sys->cb1_density[1] = 0.85f;
    /* Region 2 = basal ganglia: high CB1 */
    sys->cb1_density[2] = 0.8f;
    /* Region 3 = cerebellum: moderate CB1 */
    sys->cb1_density[3] = 0.6f;

    /* Create mutex for thread safety */
    sys->lock = (void*)nimcp_mutex_create(NULL);
    if (!sys->lock) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "ecb_create: failed to create mutex");
        nimcp_free(sys);
        return NULL;
    }

    sys->last_update_us = get_time_us();

    LOG_INFO(LOG_MODULE, "Endocannabinoid system created (2-AG=%.2f, AEA=%.2f, MAGL=%.1f/s, FAAH=%.1f/s)",
             sys->two_ag_level, sys->aea_level, sys->config.magl_rate, sys->config.faah_rate);

    return sys;
}

void ecb_destroy(endocannabinoid_system_t* system) {
    if (!system) {
        return;
    }
    if (system->magic != ECB_MAGIC) {
        LOG_WARN(LOG_MODULE, "ecb_destroy: invalid magic 0x%08X", system->magic);
        return;
    }
    system->magic = 0;

    if (system->lock) {
        nimcp_mutex_destroy((nimcp_mutex_t*)system->lock);
        system->lock = NULL;
    }

    nimcp_free(system);
}

/*=============================================================================
 * Update Logic
 *===========================================================================*/

int ecb_update(endocannabinoid_system_t* system, float dt_s) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ecb_update: system is NULL");
        return -1;
    }
    if (system->magic != ECB_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "ecb_update: invalid magic");
        return -1;
    }
    if (!isfinite(dt_s) || dt_s <= 0.0f) {
        return -1;
    }

    ECB_LOCK(system);

    /*
     * (1) 2-AG dynamics:
     *     - Synthesis driven by accumulated postsynaptic depolarization
     *     - Degradation by MAGL enzyme
     */
    float two_ag_synthesis = system->depolarization_accumulator * 0.8f * dt_s;
    float two_ag_degradation = system->two_ag_level * system->config.magl_rate *
                               system->magl_activity * dt_s;
    system->two_ag_level += two_ag_synthesis - two_ag_degradation;
    system->two_ag_level = nimcp_clampf(system->two_ag_level, 0.0f, 1.0f);

    /* Decay the depolarization accumulator (short-term memory ~200ms) */
    float depol_decay = 5.0f;  /* 1/0.2s = 5 per second */
    system->depolarization_accumulator *= expf(-depol_decay * dt_s);
    if (!isfinite(system->depolarization_accumulator)) {
        system->depolarization_accumulator = 0.0f;
    }

    /*
     * (2) AEA (anandamide) dynamics:
     *     - Tonic synthesis toward baseline
     *     - Degradation by FAAH enzyme
     */
    float aea_drive = (system->config.base_aea - system->aea_level) * 1.0f * dt_s;
    float aea_degradation = system->aea_level * system->config.faah_rate *
                            system->faah_activity * dt_s;
    system->aea_level += aea_drive - aea_degradation;
    system->aea_level = nimcp_clampf(system->aea_level, 0.0f, 1.0f);

    /*
     * (3) DSI/DSE from 2-AG via sigmoid:
     *     - DSI: suppression of inhibitory presynaptic release
     *     - DSE: suppression of excitatory presynaptic release (weaker)
     */
    system->dsi_strength = sigmoid01(system->two_ag_level, 0.3f, 12.0f) *
                           system->config.cb1_gain;
    system->dsi_strength = nimcp_clampf(system->dsi_strength, 0.0f, 1.0f);

    system->dse_strength = sigmoid01(system->two_ag_level, 0.5f, 10.0f) *
                           system->config.cb1_gain * 0.6f;  /* DSE weaker than DSI */
    system->dse_strength = nimcp_clampf(system->dse_strength, 0.0f, 1.0f);

    /*
     * (4) Tonic inhibition from AEA:
     *     - Baseline presynaptic suppression via CB1
     */
    system->tonic_inhibition = system->aea_level * system->config.cb1_gain * 0.5f;
    system->tonic_inhibition = nimcp_clampf(system->tonic_inhibition, 0.0f, 1.0f);

    system->last_update_us = get_time_us();

    ECB_UNLOCK(system);

    return 0;
}

/*=============================================================================
 * Synaptic Modulation API
 *===========================================================================*/

int ecb_on_postsynaptic_depolarization(endocannabinoid_system_t* system,
                                        uint32_t neuron_id,
                                        float depolarization) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ecb_on_postsynaptic_depolarization: system is NULL");
        return -1;
    }
    if (system->magic != ECB_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "ecb_on_postsynaptic_depolarization: invalid magic");
        return -1;
    }
    if (!isfinite(depolarization)) {
        return -1;
    }

    float clamped = nimcp_clampf(depolarization, 0.0f, 1.0f);

    ECB_LOCK(system);

    /* Accumulate depolarization signal for 2-AG synthesis */
    system->depolarization_accumulator += clamped * 0.1f;
    system->depolarization_accumulator = nimcp_clampf(
        system->depolarization_accumulator, 0.0f, 1.0f);

    ECB_UNLOCK(system);

    (void)neuron_id;  /* Future: per-neuron regional modulation */
    return 0;
}

float ecb_get_presynaptic_suppression(endocannabinoid_system_t* system,
                                       uint32_t synapse_id) {
    if (!system || system->magic != ECB_MAGIC) {
        return 0.0f;
    }

    ECB_LOCK(system);

    /*
     * Combined presynaptic suppression:
     * - Tonic component from AEA (baseline)
     * - Phasic component from 2-AG (activity-dependent DSI)
     *
     * Total suppression = tonic + phasic * (1 - tonic)
     * This avoids exceeding 1.0 when both are active.
     */
    float tonic = system->tonic_inhibition;
    float phasic = system->dsi_strength;
    float total = tonic + phasic * (1.0f - tonic);
    total = nimcp_clampf(total, 0.0f, 1.0f);

    ECB_UNLOCK(system);

    (void)synapse_id;  /* Future: per-synapse regional density lookup */
    return total;
}

float ecb_get_retrograde_signal(endocannabinoid_system_t* system,
                                 ecb_type_t type) {
    if (!system || system->magic != ECB_MAGIC) {
        return -1.0f;
    }
    if (type < 0 || type >= ECB_TYPE_COUNT) {
        return -1.0f;
    }

    ECB_LOCK(system);

    float result;
    switch (type) {
        case ECB_2AG:
            result = system->two_ag_level;
            break;
        case ECB_AEA:
            result = system->aea_level;
            break;
        default:
            result = -1.0f;
            break;
    }

    ECB_UNLOCK(system);

    return result;
}

/*=============================================================================
 * Pain Modulation API
 *===========================================================================*/

int ecb_modulate_pain(endocannabinoid_system_t* system,
                       float pain_signal,
                       float* modulated_out) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ecb_modulate_pain: system is NULL");
        return -1;
    }
    if (!modulated_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ecb_modulate_pain: modulated_out is NULL");
        return -1;
    }
    if (system->magic != ECB_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "ecb_modulate_pain: invalid magic");
        return -1;
    }
    if (!isfinite(pain_signal)) {
        *modulated_out = 0.0f;
        return -1;
    }

    float clamped_pain = nimcp_clampf(pain_signal, 0.0f, 1.0f);

    ECB_LOCK(system);

    /*
     * Pain gate modulation:
     * Both CB1 and CB2 contribute to analgesia.
     * CB1 provides fast, central analgesia.
     * CB2 provides anti-inflammatory analgesia.
     *
     * analgesia = cb1_component + cb2_component
     * modulated_pain = pain * (1 - analgesia)
     */
    float cb1_analgesia = system->two_ag_level * system->config.cb1_gain * 0.5f +
                          system->aea_level * system->config.cb1_gain * 0.3f;
    float cb2_analgesia = system->aea_level * system->config.cb2_gain * 0.2f;

    float total_analgesia = nimcp_clampf(cb1_analgesia + cb2_analgesia, 0.0f, 0.8f);

    *modulated_out = clamped_pain * (1.0f - total_analgesia);
    *modulated_out = nimcp_clampf(*modulated_out, 0.0f, 1.0f);

    ECB_UNLOCK(system);

    return 0;
}
