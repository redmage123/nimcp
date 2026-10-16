/**
 * @file nimcp_reticular.c
 * @brief Reticular Formation Implementation
 *
 * Implements arousal control, consciousness regulation, motor tone,
 * autonomic functions, pain modulation, reflexes, and sensory gating.
 */

#include "core/brain/regions/reticular/nimcp_reticular.h"
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
#include "constants/nimcp_constants.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(reticular, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

#define RETICULAR_LOG_TAG "RETICULAR"

/* Nucleus names */
static const char* s_nucleus_names[RETICULAR_NUCLEUS_COUNT] = {
    "Dorsal Raphe",
    "Median Raphe",
    "Raphe Magnus",
    "Raphe Obscurus",
    "Locus Coeruleus",
    "Lateral Tegmental",
    "Pedunculopontine",
    "Laterodorsal Tegmental",
    "Pontine Oral",
    "Pontine Caudal",
    "Gigantocellular",
    "Parvocellular",
    "Paramedian",
    "Ventral Medullary",
    "VTA"
};

/* Primary modulators for each nucleus */
static const reticular_modulator_t s_nucleus_modulators[RETICULAR_NUCLEUS_COUNT] = {
    RETICULAR_MODULATOR_SEROTONIN,       /* Dorsal Raphe */
    RETICULAR_MODULATOR_SEROTONIN,       /* Median Raphe */
    RETICULAR_MODULATOR_SEROTONIN,       /* Raphe Magnus */
    RETICULAR_MODULATOR_SEROTONIN,       /* Raphe Obscurus */
    RETICULAR_MODULATOR_NOREPINEPHRINE,  /* Locus Coeruleus */
    RETICULAR_MODULATOR_NOREPINEPHRINE,  /* Lateral Tegmental */
    RETICULAR_MODULATOR_ACETYLCHOLINE,   /* PPN */
    RETICULAR_MODULATOR_ACETYLCHOLINE,   /* LDT */
    RETICULAR_MODULATOR_GLUTAMATE,       /* Pontine Oral */
    RETICULAR_MODULATOR_GLUTAMATE,       /* Pontine Caudal */
    RETICULAR_MODULATOR_GLUTAMATE,       /* Gigantocellular */
    RETICULAR_MODULATOR_GLUTAMATE,       /* Parvocellular */
    RETICULAR_MODULATOR_GLUTAMATE,       /* Paramedian */
    RETICULAR_MODULATOR_GLUTAMATE,       /* Ventral Medullary */
    RETICULAR_MODULATOR_DOPAMINE         /* VTA */
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Sigmoid activation function
 */
static inline float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

/**
 * @brief Exponential decay
 */
static inline float exp_decay(float current, float target, float tau, float dt) {
    return target + (current - target) * expf(-dt / tau);
}

/**
 * @brief Determine arousal state from continuous level
 */
static reticular_arousal_state_t arousal_level_to_state(float level) {
    if (level < 0.1f) return RETICULAR_AROUSAL_DEEP_SLEEP;
    if (level < 0.25f) return RETICULAR_AROUSAL_LIGHT_SLEEP;
    if (level < 0.35f) return RETICULAR_AROUSAL_DROWSY;
    if (level < 0.5f) return RETICULAR_AROUSAL_RELAXED;
    if (level < 0.75f) return RETICULAR_AROUSAL_ALERT;
    return RETICULAR_AROUSAL_HYPERVIGILANT;
}

/**
 * @brief Initialize single nucleus
 */
static void init_nucleus(reticular_nucleus_state_t* nucleus,
                         reticular_nucleus_t type) {
    memset(nucleus, 0, sizeof(*nucleus));
    nucleus->type = type;
    strncpy(nucleus->name, s_nucleus_names[type], RETICULAR_MAX_NAME_LEN - 1);
    nucleus->primary_modulator = s_nucleus_modulators[type];
    nucleus->baseline_rate = RETICULAR_DEFAULT_TONIC_RATE;
    nucleus->firing_rate = nucleus->baseline_rate;
    nucleus->activity = 0.5f;
    nucleus->enabled = true;
}

/**
 * @brief Initialize single modulator
 */
static void init_modulator(reticular_modulator_state_t* mod,
                           reticular_modulator_t type,
                           float baseline) {
    memset(mod, 0, sizeof(*mod));
    mod->type = type;
    mod->baseline = baseline;
    mod->concentration = baseline;
    mod->release_rate = 0.0f;
    mod->decay_rate = 0.1f;
}

/**
 * @brief Initialize single reflex
 */
static void init_reflex(reticular_reflex_state_t* reflex,
                        reticular_reflex_t type,
                        float threshold,
                        float gain) {
    memset(reflex, 0, sizeof(*reflex));
    reflex->type = type;
    reflex->threshold = threshold;
    reflex->gain = gain;
    reflex->active = false;
}

/**
 * @brief Update single nucleus
 */
static void update_nucleus(reticular_nucleus_state_t* nucleus, float dt) {
    if (!nucleus->enabled) return;

    /* Compute net input */
    float net_input = nucleus->excitatory_input - nucleus->inhibitory_input;

    /* Update activity with sigmoid transfer */
    float target_activity = sigmoid(net_input);
    nucleus->activity = exp_decay(nucleus->activity, target_activity, 0.1f, dt);

    /* Update firing rate based on activity */
    nucleus->firing_rate = nucleus->baseline_rate +
        (30.0f - nucleus->baseline_rate) * nucleus->activity;

    /* Update modulator output */
    nucleus->modulator_output = nucleus->activity * nucleus->firing_rate / 30.0f;

    /* Decay inputs */
    nucleus->excitatory_input *= expf(-dt / 0.05f);
    nucleus->inhibitory_input *= expf(-dt / 0.05f);
}

/**
 * @brief Update single modulator
 */
static void update_modulator(reticular_modulator_state_t* mod, float dt) {
    /* Apply release and decay */
    float change = mod->release_rate - mod->decay_rate * (mod->concentration - mod->baseline);
    mod->concentration += change * dt;
    mod->concentration = nimcp_clampf(mod->concentration, 0.0f, 1.0f);
}

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

int reticular_default_config(reticular_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    memset(config, 0, sizeof(*config));

    /* Arousal parameters */
    config->arousal_gain = 1.0f;
    config->arousal_decay = 0.05f;
    config->sleep_threshold = RETICULAR_SLEEP_THRESHOLD;
    config->wake_threshold = RETICULAR_ALERT_THRESHOLD;
    config->hypervigilant_threshold = 0.85f;

    /* Neuromodulator baselines */
    config->serotonin_baseline = NIMCP_SEROTONIN_BASELINE;
    config->norepinephrine_baseline = 0.3f;
    config->acetylcholine_baseline = 0.4f;
    config->dopamine_baseline = 0.4f;

    /* Autonomic parameters */
    config->cardiovascular_gain = 1.0f;
    config->respiratory_gain = 1.0f;
    config->sympathetic_baseline = 0.4f;
    config->parasympathetic_baseline = 0.5f;

    /* Motor control */
    config->postural_tone_baseline = 0.5f;
    config->atonia_threshold = 0.2f;

    /* Pain modulation */
    config->pain_gate_baseline = 0.3f;
    config->analgesia_gain = 1.0f;

    /* Sensory gating */
    config->thalamic_gate_baseline = 0.7f;
    config->habituation_rate = 0.01f;

    /* Reflex parameters */
    config->reflex_threshold_base = 0.5f;
    config->startle_habituation = 0.1f;

    /* Integration */
    config->enable_bio_async = true;
    config->enable_kg_wiring = true;
    config->enable_immune = true;
    config->enable_security = true;
    config->enable_logging = true;
    config->enable_quantum = false;

    /* Resources */
    config->max_history_size = RETICULAR_HISTORY_SIZE;
    config->update_interval_ms = 10;

    config->platform_tier = PLATFORM_TIER_FULL;

    return 0;
}

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

nimcp_reticular_t* reticular_create(const reticular_config_t* config) {
    reticular_config_t default_config;
    if (!config) {
        reticular_default_config(&default_config);
        config = &default_config;
    }

    nimcp_reticular_t* reticular = (nimcp_reticular_t*)nimcp_calloc(
        1, sizeof(nimcp_reticular_t));
    if (!reticular) {
        NIMCP_LOG_ERROR(RETICULAR_LOG_TAG, "Failed to allocate reticular formation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return NULL;
    }

    /* Copy config */
    memcpy(&reticular->config, config, sizeof(reticular_config_t));

    /* Create mutex */
    reticular->mutex = nimcp_mutex_create(NULL);
    if (!reticular->mutex) {
        NIMCP_LOG_ERROR(RETICULAR_LOG_TAG, "Failed to create mutex");
        nimcp_free(reticular);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "reticular_create: reticular->mutex is NULL");
        return NULL;
    }

    /* Initialize nuclei */
    for (int i = 0; i < RETICULAR_NUCLEUS_COUNT; i++) {
        init_nucleus(&reticular->nuclei[i], (reticular_nucleus_t)i);
    }

    /* Initialize modulators */
    init_modulator(&reticular->modulators[RETICULAR_MODULATOR_SEROTONIN],
                   RETICULAR_MODULATOR_SEROTONIN, config->serotonin_baseline);
    init_modulator(&reticular->modulators[RETICULAR_MODULATOR_NOREPINEPHRINE],
                   RETICULAR_MODULATOR_NOREPINEPHRINE, config->norepinephrine_baseline);
    init_modulator(&reticular->modulators[RETICULAR_MODULATOR_ACETYLCHOLINE],
                   RETICULAR_MODULATOR_ACETYLCHOLINE, config->acetylcholine_baseline);
    init_modulator(&reticular->modulators[RETICULAR_MODULATOR_DOPAMINE],
                   RETICULAR_MODULATOR_DOPAMINE, config->dopamine_baseline);
    init_modulator(&reticular->modulators[RETICULAR_MODULATOR_HISTAMINE],
                   RETICULAR_MODULATOR_HISTAMINE, 0.4f);
    init_modulator(&reticular->modulators[RETICULAR_MODULATOR_OREXIN],
                   RETICULAR_MODULATOR_OREXIN, 0.5f);
    init_modulator(&reticular->modulators[RETICULAR_MODULATOR_GABA],
                   RETICULAR_MODULATOR_GABA, 0.5f);
    init_modulator(&reticular->modulators[RETICULAR_MODULATOR_GLUTAMATE],
                   RETICULAR_MODULATOR_GLUTAMATE, 0.5f);

    /* Initialize autonomic states */
    for (int i = 0; i < RETICULAR_AUTONOMIC_COUNT; i++) {
        reticular->autonomic[i].type = (reticular_autonomic_t)i;
        reticular->autonomic[i].sympathetic_tone = config->sympathetic_baseline;
        reticular->autonomic[i].parasympathetic_tone = config->parasympathetic_baseline;
        reticular->autonomic[i].balance = 0.0f;
        reticular->autonomic[i].setpoint = 0.5f;
        reticular->autonomic[i].current_value = 0.5f;
    }

    /* Initialize reflexes with defaults */
    init_reflex(&reticular->reflexes[RETICULAR_REFLEX_SWALLOWING],
                RETICULAR_REFLEX_SWALLOWING, 0.6f, 1.0f);
    init_reflex(&reticular->reflexes[RETICULAR_REFLEX_COUGHING],
                RETICULAR_REFLEX_COUGHING, 0.7f, 1.0f);
    init_reflex(&reticular->reflexes[RETICULAR_REFLEX_VOMITING],
                RETICULAR_REFLEX_VOMITING, 0.9f, 1.0f);
    init_reflex(&reticular->reflexes[RETICULAR_REFLEX_SNEEZING],
                RETICULAR_REFLEX_SNEEZING, 0.6f, 1.0f);
    init_reflex(&reticular->reflexes[RETICULAR_REFLEX_GAGGING],
                RETICULAR_REFLEX_GAGGING, 0.7f, 1.0f);
    init_reflex(&reticular->reflexes[RETICULAR_REFLEX_YAWNING],
                RETICULAR_REFLEX_YAWNING, 0.5f, 0.8f);
    init_reflex(&reticular->reflexes[RETICULAR_REFLEX_STARTLE],
                RETICULAR_REFLEX_STARTLE, 0.4f, 1.5f);
    init_reflex(&reticular->reflexes[RETICULAR_REFLEX_RIGHTING],
                RETICULAR_REFLEX_RIGHTING, 0.3f, 1.0f);

    /* Initialize motor state */
    reticular->motor.postural_tone = config->postural_tone_baseline;
    reticular->motor.limb_tone = config->postural_tone_baseline;
    reticular->motor.atonia_level = 0.0f;
    reticular->motor.locomotor_drive = 0.0f;
    reticular->motor.startle_readiness = 0.5f;
    reticular->motor.rem_atonia_active = false;

    /* Initialize pain state */
    reticular->pain.gate_control = config->pain_gate_baseline;
    reticular->pain.endogenous_analgesia = 0.0f;
    reticular->pain.serotonin_analgesia = 0.0f;
    reticular->pain.noradrenergic_mod = 0.0f;
    reticular->pain.stress_analgesia = 0.0f;
    reticular->pain.pain_threshold = 0.5f;

    /* Initialize sensory gating */
    reticular->sensory_gate.thalamic_gate = config->thalamic_gate_baseline;
    reticular->sensory_gate.visual_gate = 0.7f;
    reticular->sensory_gate.auditory_gate = 0.7f;
    reticular->sensory_gate.somatosensory_gate = 0.7f;
    reticular->sensory_gate.attention_bias = 0.5f;
    reticular->sensory_gate.habituation_level = 0.0f;

    /* Initial arousal state */
    reticular->arousal_level = RETICULAR_DEFAULT_AROUSAL;
    reticular->arousal_state = RETICULAR_AROUSAL_RELAXED;
    reticular->arousal_momentum = 0.0f;

    /* Circadian */
    reticular->circadian_drive = 0.5f;
    reticular->homeostatic_sleep_pressure = 0.3f;

    NIMCP_LOG_INFO(RETICULAR_LOG_TAG, "Reticular formation created");

    return reticular;
}

void reticular_destroy(nimcp_reticular_t* reticular) {
    if (!reticular) return;

    NIMCP_LOG_INFO(RETICULAR_LOG_TAG,
                   "Destroying reticular formation (transitions=%lu, reflexes=%lu)",
                   reticular->stats.arousal_transitions,
                   reticular->stats.reflexes_triggered);

    /* Disconnect from systems */
    if (reticular->connected) {
        reticular_bio_async_disconnect(reticular);
        reticular_kg_unregister(reticular);
    }

    /* Clean up */
    if (reticular->mutex) {
        nimcp_mutex_free(reticular->mutex);
    }

    nimcp_free(reticular);
}

int reticular_init(nimcp_reticular_t* reticular) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }

    nimcp_mutex_lock(reticular->mutex);

    /* Reset state */
    reticular->arousal_level = RETICULAR_DEFAULT_AROUSAL;
    reticular->arousal_state = RETICULAR_AROUSAL_RELAXED;
    reticular->arousal_momentum = 0.0f;
    reticular->arousal_state_start_us = 0;

    /* Reset nuclei activities */
    for (int i = 0; i < RETICULAR_NUCLEUS_COUNT; i++) {
        reticular->nuclei[i].activity = 0.5f;
        reticular->nuclei[i].firing_rate = reticular->nuclei[i].baseline_rate;
        reticular->nuclei[i].excitatory_input = 0.0f;
        reticular->nuclei[i].inhibitory_input = 0.0f;
    }

    /* Reset modulator concentrations to baseline */
    for (int i = 0; i < RETICULAR_MODULATOR_COUNT; i++) {
        reticular->modulators[i].concentration = reticular->modulators[i].baseline;
        reticular->modulators[i].release_rate = 0.0f;
    }

    memset(&reticular->stats, 0, sizeof(reticular_stats_t));

    reticular->initialized = true;
    reticular->last_update_us = 0;
    reticular->simulation_time_us = 0;

    nimcp_mutex_unlock(reticular->mutex);

    NIMCP_LOG_INFO(RETICULAR_LOG_TAG, "Reticular formation initialized");
    return 0;
}

int reticular_reset(nimcp_reticular_t* reticular) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }
    return reticular_init(reticular);
}

/*=============================================================================
 * AROUSAL CONTROL
 *===========================================================================*/

int reticular_update_arousal(nimcp_reticular_t* reticular, float dt) {
    if (!reticular || !reticular->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_update_arousal: required parameter is NULL (reticular, reticular->initialized)");
        return -1;
    }

    nimcp_mutex_lock(reticular->mutex);

    /* Compute arousal drive from nuclei */
    float lc_contribution = reticular->nuclei[RETICULAR_NUCLEUS_LOCUS_COERULEUS].activity * 0.3f;
    float raphe_contribution = reticular->nuclei[RETICULAR_NUCLEUS_RAPHE_DORSAL].activity * 0.2f;
    float ppn_contribution = reticular->nuclei[RETICULAR_NUCLEUS_PEDUNCULOPONTINE].activity * 0.25f;
    float vta_contribution = reticular->nuclei[RETICULAR_NUCLEUS_VTA].activity * 0.15f;

    /* Modulator effects */
    float ne_effect = (reticular->modulators[RETICULAR_MODULATOR_NOREPINEPHRINE].concentration - 0.3f) * 0.5f;
    float ach_effect = (reticular->modulators[RETICULAR_MODULATOR_ACETYLCHOLINE].concentration - 0.3f) * 0.4f;
    float orexin_effect = (reticular->modulators[RETICULAR_MODULATOR_OREXIN].concentration - 0.5f) * 0.3f;

    /* Circadian and homeostatic influence */
    float circadian_effect = (reticular->circadian_drive - 0.5f) * 0.2f;
    float sleep_pressure_effect = -reticular->homeostatic_sleep_pressure * 0.3f;

    /* Compute target arousal */
    float arousal_drive = lc_contribution + raphe_contribution + ppn_contribution +
                          vta_contribution + ne_effect + ach_effect + orexin_effect +
                          circadian_effect + sleep_pressure_effect;

    float target_arousal = nimcp_clampf(0.5f + arousal_drive, 0.0f, 1.0f);

    /* Apply arousal dynamics with momentum */
    reticular->arousal_momentum = exp_decay(reticular->arousal_momentum,
                                            (target_arousal - reticular->arousal_level) * 2.0f,
                                            0.5f, dt);
    reticular->arousal_level += reticular->arousal_momentum * dt;
    reticular->arousal_level = nimcp_clampf(reticular->arousal_level, 0.0f, 1.0f);

    /* Determine state */
    reticular_arousal_state_t new_state = arousal_level_to_state(reticular->arousal_level);

    /* Handle state transitions */
    if (new_state != reticular->arousal_state) {
        reticular->stats.arousal_transitions++;

        /* Track sleep/wake transitions */
        if (new_state <= RETICULAR_AROUSAL_LIGHT_SLEEP &&
            reticular->arousal_state > RETICULAR_AROUSAL_LIGHT_SLEEP) {
            reticular->stats.sleep_cycles++;
        }
        if (new_state >= RETICULAR_AROUSAL_ALERT &&
            reticular->arousal_state < RETICULAR_AROUSAL_ALERT) {
            reticular->stats.wake_episodes++;
        }

        reticular->arousal_state = new_state;
        reticular->arousal_state_start_us = reticular->simulation_time_us;

        /* Broadcast state change */
        if (reticular->bio_router && reticular->config.enable_bio_async) {
            reticular_bio_async_broadcast(reticular, RETICULAR_BIO_MSG_AROUSAL_CHANGE,
                                          &new_state, sizeof(new_state));
        }

        NIMCP_LOG_DEBUG(RETICULAR_LOG_TAG, "Arousal transition to %s (level=%.3f)",
                        reticular_arousal_state_string(new_state),
                        reticular->arousal_level);
    }

    /* Update sleep time tracking */
    if (reticular->arousal_state <= RETICULAR_AROUSAL_REM_SLEEP) {
        reticular->stats.time_in_sleep_us += dt * 1000000.0f;
    } else {
        reticular->stats.time_in_wake_us += dt * 1000000.0f;
    }

    /* Update average arousal */
    reticular->stats.avg_arousal_level =
        (reticular->stats.avg_arousal_level * 0.99f) + (reticular->arousal_level * 0.01f);

    nimcp_mutex_unlock(reticular->mutex);
    return 0;
}

float reticular_get_arousal(const nimcp_reticular_t* reticular) {
    if (!reticular) return 0.0f;
    return reticular->arousal_level;
}

reticular_arousal_state_t reticular_get_arousal_state(
    const nimcp_reticular_t* reticular) {
    if (!reticular) return RETICULAR_AROUSAL_RELAXED;
    return reticular->arousal_state;
}

int reticular_apply_arousal_stimulus(nimcp_reticular_t* reticular,
                                     float stimulus,
                                     const char* source) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }

    nimcp_mutex_lock(reticular->mutex);

    /* Apply stimulus to relevant nuclei */
    if (stimulus > 0) {
        /* Excitatory stimulus - activate LC and PPN */
        reticular->nuclei[RETICULAR_NUCLEUS_LOCUS_COERULEUS].excitatory_input +=
            stimulus * 0.5f;
        reticular->nuclei[RETICULAR_NUCLEUS_PEDUNCULOPONTINE].excitatory_input +=
            stimulus * 0.4f;
    } else {
        /* Inhibitory stimulus - activate GABA-ergic inhibition */
        float inhib = -stimulus;
        reticular->nuclei[RETICULAR_NUCLEUS_LOCUS_COERULEUS].inhibitory_input +=
            inhib * 0.5f;
        reticular->nuclei[RETICULAR_NUCLEUS_PEDUNCULOPONTINE].inhibitory_input +=
            inhib * 0.4f;
    }

    /* Direct arousal modulation */
    reticular->arousal_momentum += stimulus * reticular->config.arousal_gain * 0.5f;

    nimcp_mutex_unlock(reticular->mutex);

    NIMCP_LOG_DEBUG(RETICULAR_LOG_TAG, "Arousal stimulus %.3f from %s",
                    stimulus, source ? source : "unknown");
    return 0;
}

int reticular_initiate_sleep(nimcp_reticular_t* reticular,
                             reticular_arousal_state_t target_state) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }
    if (target_state > RETICULAR_AROUSAL_REM_SLEEP) return -1;

    nimcp_mutex_lock(reticular->mutex);

    /* Reduce arousal-promoting nuclei activity */
    reticular->nuclei[RETICULAR_NUCLEUS_LOCUS_COERULEUS].inhibitory_input += 0.5f;
    reticular->nuclei[RETICULAR_NUCLEUS_VTA].inhibitory_input += 0.3f;
    reticular->nuclei[RETICULAR_NUCLEUS_PEDUNCULOPONTINE].inhibitory_input += 0.2f;

    /* Increase GABA */
    reticular->modulators[RETICULAR_MODULATOR_GABA].release_rate += 0.2f;

    /* Decrease NE and ACh */
    reticular->modulators[RETICULAR_MODULATOR_NOREPINEPHRINE].release_rate -= 0.3f;
    reticular->modulators[RETICULAR_MODULATOR_ACETYLCHOLINE].release_rate -= 0.2f;

    /* For REM sleep, activate PPN/LDT for ACh while maintaining atonia */
    if (target_state == RETICULAR_AROUSAL_REM_SLEEP) {
        reticular->nuclei[RETICULAR_NUCLEUS_PEDUNCULOPONTINE].excitatory_input += 0.3f;
        reticular->nuclei[RETICULAR_NUCLEUS_LATERODORSAL_TEGMENTAL].excitatory_input += 0.3f;
        reticular->motor.rem_atonia_active = true;
    }

    nimcp_mutex_unlock(reticular->mutex);

    NIMCP_LOG_INFO(RETICULAR_LOG_TAG, "Initiating sleep transition to %s",
                   reticular_arousal_state_string(target_state));
    return 0;
}

int reticular_wake(nimcp_reticular_t* reticular, float urgency) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }

    nimcp_mutex_lock(reticular->mutex);

    urgency = nimcp_clampf(urgency, 0.0f, 1.0f);

    /* Activate arousal systems based on urgency */
    float activation = 0.3f + urgency * 0.5f;

    reticular->nuclei[RETICULAR_NUCLEUS_LOCUS_COERULEUS].excitatory_input += activation;
    reticular->nuclei[RETICULAR_NUCLEUS_PEDUNCULOPONTINE].excitatory_input += activation * 0.8f;
    reticular->nuclei[RETICULAR_NUCLEUS_VTA].excitatory_input += activation * 0.5f;

    /* Increase NE and ACh */
    reticular->modulators[RETICULAR_MODULATOR_NOREPINEPHRINE].release_rate += activation * 0.5f;
    reticular->modulators[RETICULAR_MODULATOR_ACETYLCHOLINE].release_rate += activation * 0.4f;

    /* Decrease GABA */
    reticular->modulators[RETICULAR_MODULATOR_GABA].release_rate -= activation * 0.3f;

    /* Disable REM atonia */
    reticular->motor.rem_atonia_active = false;

    nimcp_mutex_unlock(reticular->mutex);

    NIMCP_LOG_INFO(RETICULAR_LOG_TAG, "Wake initiated (urgency=%.2f)", urgency);
    return 0;
}

/*=============================================================================
 * NEUROMODULATOR API
 *===========================================================================*/

float reticular_get_modulator(const nimcp_reticular_t* reticular,
                              reticular_modulator_t modulator) {
    if (!reticular || modulator >= RETICULAR_MODULATOR_COUNT) return 0.0f;
    return reticular->modulators[modulator].concentration;
}

int reticular_set_modulator_release(nimcp_reticular_t* reticular,
                                    reticular_modulator_t modulator,
                                    float rate) {
    if (!reticular || modulator >= RETICULAR_MODULATOR_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "reticular_wake: reticular is NULL");
        return -1;
    }

    nimcp_mutex_lock(reticular->mutex);
    reticular->modulators[modulator].release_rate = rate;
    nimcp_mutex_unlock(reticular->mutex);

    return 0;
}

int reticular_get_all_modulators(const nimcp_reticular_t* reticular,
                                 reticular_modulator_state_t* states) {
    if (!reticular || !states) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_wake: required parameter is NULL (reticular, states)");
        return -1;
    }

    nimcp_mutex_lock(((nimcp_reticular_t*)reticular)->mutex);
    memcpy(states, reticular->modulators,
           RETICULAR_MODULATOR_COUNT * sizeof(reticular_modulator_state_t));
    nimcp_mutex_unlock(((nimcp_reticular_t*)reticular)->mutex);

    return 0;
}

int reticular_compute_modulator_effects(nimcp_reticular_t* reticular,
                                        float* arousal_delta) {
    if (!reticular || !arousal_delta) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_wake: required parameter is NULL (reticular, arousal_delta)");
        return -1;
    }

    nimcp_mutex_lock(reticular->mutex);

    /* Each modulator has different effects on arousal */
    float delta = 0.0f;

    /* NE increases arousal */
    delta += (reticular->modulators[RETICULAR_MODULATOR_NOREPINEPHRINE].concentration -
              reticular->config.norepinephrine_baseline) * 0.4f;

    /* ACh increases arousal (during wake) */
    if (reticular->arousal_state > RETICULAR_AROUSAL_REM_SLEEP) {
        delta += (reticular->modulators[RETICULAR_MODULATOR_ACETYLCHOLINE].concentration -
                  reticular->config.acetylcholine_baseline) * 0.3f;
    }

    /* DA increases arousal/motivation */
    delta += (reticular->modulators[RETICULAR_MODULATOR_DOPAMINE].concentration -
              reticular->config.dopamine_baseline) * 0.2f;

    /* GABA decreases arousal */
    delta -= (reticular->modulators[RETICULAR_MODULATOR_GABA].concentration - 0.5f) * 0.3f;

    /* Histamine increases arousal */
    delta += (reticular->modulators[RETICULAR_MODULATOR_HISTAMINE].concentration - 0.4f) * 0.2f;

    /* Orexin increases arousal (strongly) */
    delta += (reticular->modulators[RETICULAR_MODULATOR_OREXIN].concentration - 0.5f) * 0.4f;

    *arousal_delta = delta;

    nimcp_mutex_unlock(reticular->mutex);
    return 0;
}

/*=============================================================================
 * NUCLEUS CONTROL
 *===========================================================================*/

float reticular_get_nucleus_activity(const nimcp_reticular_t* reticular,
                                     reticular_nucleus_t nucleus) {
    if (!reticular || nucleus >= RETICULAR_NUCLEUS_COUNT) return 0.0f;
    return reticular->nuclei[nucleus].activity;
}

int reticular_stimulate_nucleus(nimcp_reticular_t* reticular,
                                reticular_nucleus_t nucleus,
                                float excitation,
                                float inhibition) {
    if (!reticular || nucleus >= RETICULAR_NUCLEUS_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "reticular_wake: reticular is NULL");
        return -1;
    }

    nimcp_mutex_lock(reticular->mutex);

    reticular->nuclei[nucleus].excitatory_input += excitation;
    reticular->nuclei[nucleus].inhibitory_input += inhibition;

    nimcp_mutex_unlock(reticular->mutex);
    return 0;
}

int reticular_get_nucleus_state(const nimcp_reticular_t* reticular,
                                reticular_nucleus_t nucleus,
                                reticular_nucleus_state_t* state) {
    if (!reticular || !state || nucleus >= RETICULAR_NUCLEUS_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_wake: required parameter is NULL (reticular, state)");
        return -1;
    }

    nimcp_mutex_lock(((nimcp_reticular_t*)reticular)->mutex);
    memcpy(state, &reticular->nuclei[nucleus], sizeof(reticular_nucleus_state_t));
    nimcp_mutex_unlock(((nimcp_reticular_t*)reticular)->mutex);

    return 0;
}

/*=============================================================================
 * AUTONOMIC CONTROL
 *===========================================================================*/

int reticular_update_autonomic(nimcp_reticular_t* reticular, float dt) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }

    nimcp_mutex_lock(reticular->mutex);

    /* Arousal affects sympathetic/parasympathetic balance */
    float arousal_effect = (reticular->arousal_level - 0.5f) * 2.0f;

    for (int i = 0; i < RETICULAR_AUTONOMIC_COUNT; i++) {
        reticular_autonomic_state_t* auto_state = &reticular->autonomic[i];

        /* Arousal increases sympathetic, decreases parasympathetic */
        float symp_target = reticular->config.sympathetic_baseline + arousal_effect * 0.3f;
        float para_target = reticular->config.parasympathetic_baseline - arousal_effect * 0.2f;

        symp_target = nimcp_clampf(symp_target, 0.0f, 1.0f);
        para_target = nimcp_clampf(para_target, 0.0f, 1.0f);

        auto_state->sympathetic_tone = exp_decay(auto_state->sympathetic_tone,
                                                  symp_target, 0.5f, dt);
        auto_state->parasympathetic_tone = exp_decay(auto_state->parasympathetic_tone,
                                                      para_target, 0.5f, dt);

        /* Compute balance */
        auto_state->balance = auto_state->sympathetic_tone - auto_state->parasympathetic_tone;

        /* Update current value based on balance and setpoint */
        float target_value = auto_state->setpoint + auto_state->balance * 0.2f;
        auto_state->current_value = exp_decay(auto_state->current_value,
                                               target_value, 0.3f, dt);
    }

    reticular->stats.autonomic_adjustments++;

    nimcp_mutex_unlock(reticular->mutex);
    return 0;
}

float reticular_get_autonomic_balance(const nimcp_reticular_t* reticular,
                                      reticular_autonomic_t function) {
    if (!reticular || function >= RETICULAR_AUTONOMIC_COUNT) return 0.0f;
    return reticular->autonomic[function].balance;
}

int reticular_set_autonomic_setpoint(nimcp_reticular_t* reticular,
                                     reticular_autonomic_t function,
                                     float setpoint) {
    if (!reticular || function >= RETICULAR_AUTONOMIC_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "reticular_update_autonomic: reticular is NULL");
        return -1;
    }

    nimcp_mutex_lock(reticular->mutex);
    reticular->autonomic[function].setpoint = nimcp_clampf(setpoint, 0.0f, 1.0f);
    nimcp_mutex_unlock(reticular->mutex);

    return 0;
}

int reticular_apply_sympathetic_drive(nimcp_reticular_t* reticular, float intensity) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }

    nimcp_mutex_lock(reticular->mutex);

    intensity = nimcp_clampf(intensity, 0.0f, 1.0f);

    for (int i = 0; i < RETICULAR_AUTONOMIC_COUNT; i++) {
        reticular->autonomic[i].sympathetic_tone += intensity * 0.3f;
        reticular->autonomic[i].sympathetic_tone =
            nimcp_clampf(reticular->autonomic[i].sympathetic_tone, 0.0f, 1.0f);
    }

    /* Also increase NE */
    reticular->modulators[RETICULAR_MODULATOR_NOREPINEPHRINE].release_rate += intensity * 0.2f;

    nimcp_mutex_unlock(reticular->mutex);
    return 0;
}

int reticular_apply_parasympathetic_drive(nimcp_reticular_t* reticular, float intensity) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }

    nimcp_mutex_lock(reticular->mutex);

    intensity = nimcp_clampf(intensity, 0.0f, 1.0f);

    for (int i = 0; i < RETICULAR_AUTONOMIC_COUNT; i++) {
        reticular->autonomic[i].parasympathetic_tone += intensity * 0.3f;
        reticular->autonomic[i].parasympathetic_tone =
            nimcp_clampf(reticular->autonomic[i].parasympathetic_tone, 0.0f, 1.0f);
    }

    /* Also increase ACh */
    reticular->modulators[RETICULAR_MODULATOR_ACETYLCHOLINE].release_rate += intensity * 0.15f;

    nimcp_mutex_unlock(reticular->mutex);
    return 0;
}

/*=============================================================================
 * REFLEX CONTROL
 *===========================================================================*/

int reticular_trigger_reflex(nimcp_reticular_t* reticular,
                             reticular_reflex_t reflex,
                             float stimulus) {
    if (!reticular || reflex >= RETICULAR_REFLEX_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "reticular_apply_parasympathetic_drive: reticular is NULL");
        return -1;
    }

    nimcp_mutex_lock(reticular->mutex);

    reticular_reflex_state_t* ref = &reticular->reflexes[reflex];

    ref->current_activation += stimulus;

    /* Check threshold */
    if (ref->current_activation >= ref->threshold && !ref->active) {
        ref->active = true;
        ref->trigger_count++;
        ref->last_triggered_us = reticular->simulation_time_us;
        reticular->stats.reflexes_triggered++;

        /* Broadcast reflex trigger */
        if (reticular->bio_router && reticular->config.enable_bio_async) {
            reticular_bio_async_broadcast(reticular, RETICULAR_BIO_MSG_REFLEX_TRIGGER,
                                          &reflex, sizeof(reflex));
        }

        NIMCP_LOG_DEBUG(RETICULAR_LOG_TAG, "Reflex %s triggered",
                        reticular_reflex_string(reflex));
    }

    nimcp_mutex_unlock(reticular->mutex);
    return 0;
}

bool reticular_is_reflex_active(const nimcp_reticular_t* reticular,
                                reticular_reflex_t reflex) {
    if (!reticular || reflex >= RETICULAR_REFLEX_COUNT) {
        return false;
    }
    return reticular->reflexes[reflex].active;
}

int reticular_set_reflex_threshold(nimcp_reticular_t* reticular,
                                   reticular_reflex_t reflex,
                                   float threshold) {
    if (!reticular || reflex >= RETICULAR_REFLEX_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "reticular_apply_parasympathetic_drive: reticular is NULL");
        return -1;
    }

    nimcp_mutex_lock(reticular->mutex);
    reticular->reflexes[reflex].threshold = nimcp_clampf(threshold, 0.1f, 1.0f);
    nimcp_mutex_unlock(reticular->mutex);

    return 0;
}

int reticular_get_reflex_state(const nimcp_reticular_t* reticular,
                               reticular_reflex_t reflex,
                               reticular_reflex_state_t* state) {
    if (!reticular || !state || reflex >= RETICULAR_REFLEX_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_apply_parasympathetic_drive: required parameter is NULL (reticular, state)");
        return -1;
    }

    nimcp_mutex_lock(((nimcp_reticular_t*)reticular)->mutex);
    memcpy(state, &reticular->reflexes[reflex], sizeof(reticular_reflex_state_t));
    nimcp_mutex_unlock(((nimcp_reticular_t*)reticular)->mutex);

    return 0;
}

/*=============================================================================
 * MOTOR CONTROL
 *===========================================================================*/

int reticular_update_motor_tone(nimcp_reticular_t* reticular, float dt) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }

    nimcp_mutex_lock(reticular->mutex);

    /* Arousal affects motor tone */
    float arousal = reticular->arousal_level;

    /* During REM sleep, activate atonia */
    if (reticular->arousal_state == RETICULAR_AROUSAL_REM_SLEEP) {
        reticular->motor.rem_atonia_active = true;
        reticular->motor.atonia_level = exp_decay(reticular->motor.atonia_level,
                                                   0.9f, 0.2f, dt);
    } else {
        reticular->motor.rem_atonia_active = false;
        reticular->motor.atonia_level = exp_decay(reticular->motor.atonia_level,
                                                   0.0f, 0.3f, dt);
    }

    /* Postural tone depends on arousal and atonia */
    float target_postural = reticular->config.postural_tone_baseline +
                            (arousal - 0.5f) * 0.4f;
    target_postural *= (1.0f - reticular->motor.atonia_level);
    target_postural = nimcp_clampf(target_postural, 0.0f, 1.0f);

    reticular->motor.postural_tone = exp_decay(reticular->motor.postural_tone,
                                                target_postural, 0.2f, dt);
    reticular->motor.limb_tone = reticular->motor.postural_tone * 0.9f;

    /* Startle readiness depends on arousal */
    reticular->motor.startle_readiness = nimcp_clampf(arousal * 1.2f, 0.0f, 1.0f);

    nimcp_mutex_unlock(reticular->mutex);
    return 0;
}

float reticular_get_postural_tone(const nimcp_reticular_t* reticular) {
    if (!reticular) return 0.0f;
    return reticular->motor.postural_tone;
}

int reticular_set_rem_atonia(nimcp_reticular_t* reticular, bool active) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }

    nimcp_mutex_lock(reticular->mutex);
    reticular->motor.rem_atonia_active = active;
    nimcp_mutex_unlock(reticular->mutex);

    return 0;
}

float reticular_get_locomotor_drive(const nimcp_reticular_t* reticular) {
    if (!reticular) return 0.0f;
    return reticular->motor.locomotor_drive;
}

int reticular_set_locomotor_drive(nimcp_reticular_t* reticular, float drive) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }

    nimcp_mutex_lock(reticular->mutex);
    reticular->motor.locomotor_drive = nimcp_clampf(drive, 0.0f, 1.0f);

    /* Locomotion activates PPN */
    reticular->nuclei[RETICULAR_NUCLEUS_PEDUNCULOPONTINE].excitatory_input +=
        drive * 0.3f;

    nimcp_mutex_unlock(reticular->mutex);
    return 0;
}

/*=============================================================================
 * PAIN MODULATION
 *===========================================================================*/

int reticular_update_pain_modulation(nimcp_reticular_t* reticular, float dt) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }

    nimcp_mutex_lock(reticular->mutex);

    /* Serotonin-mediated analgesia from raphe magnus */
    reticular->pain.serotonin_analgesia =
        reticular->nuclei[RETICULAR_NUCLEUS_RAPHE_MAGNUS].activity * 0.5f;

    /* Noradrenergic modulation from LC */
    reticular->pain.noradrenergic_mod =
        reticular->nuclei[RETICULAR_NUCLEUS_LOCUS_COERULEUS].activity * 0.3f;

    /* Compute total endogenous analgesia */
    reticular->pain.endogenous_analgesia =
        reticular->pain.serotonin_analgesia +
        reticular->pain.noradrenergic_mod +
        reticular->pain.stress_analgesia * 0.5f;
    reticular->pain.endogenous_analgesia =
        nimcp_clampf(reticular->pain.endogenous_analgesia, 0.0f, 1.0f);

    /* Gate control: higher analgesia closes the gate */
    float target_gate = reticular->config.pain_gate_baseline +
                        reticular->pain.endogenous_analgesia * 0.5f;
    reticular->pain.gate_control = exp_decay(reticular->pain.gate_control,
                                              target_gate, 0.3f, dt);

    /* Pain threshold increases with analgesia */
    reticular->pain.pain_threshold = 0.5f + reticular->pain.endogenous_analgesia * 0.4f;

    /* Decay stress analgesia */
    reticular->pain.stress_analgesia *= expf(-dt / 1.0f);

    nimcp_mutex_unlock(reticular->mutex);
    return 0;
}

float reticular_get_pain_gate(const nimcp_reticular_t* reticular) {
    if (!reticular) return 0.0f;
    return reticular->pain.gate_control;
}

int reticular_apply_pain_inhibition(nimcp_reticular_t* reticular, float inhibition) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }

    nimcp_mutex_lock(reticular->mutex);

    inhibition = nimcp_clampf(inhibition, 0.0f, 1.0f);

    /* Activate raphe magnus for descending inhibition */
    reticular->nuclei[RETICULAR_NUCLEUS_RAPHE_MAGNUS].excitatory_input += inhibition * 0.5f;

    /* Increase serotonin release */
    reticular->modulators[RETICULAR_MODULATOR_SEROTONIN].release_rate += inhibition * 0.2f;

    nimcp_mutex_unlock(reticular->mutex);
    return 0;
}

float reticular_get_pain_threshold(const nimcp_reticular_t* reticular) {
    if (!reticular) return 0.5f;
    return reticular->pain.pain_threshold;
}

int reticular_activate_stress_analgesia(nimcp_reticular_t* reticular, float stress_level) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }

    nimcp_mutex_lock(reticular->mutex);

    stress_level = nimcp_clampf(stress_level, 0.0f, 1.0f);
    reticular->pain.stress_analgesia = fmaxf(reticular->pain.stress_analgesia,
                                              stress_level * reticular->config.analgesia_gain);

    nimcp_mutex_unlock(reticular->mutex);
    return 0;
}

/*=============================================================================
 * SENSORY GATING
 *===========================================================================*/

int reticular_update_sensory_gating(nimcp_reticular_t* reticular, float dt) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }

    nimcp_mutex_lock(reticular->mutex);

    /* Arousal affects gate openness */
    float arousal = reticular->arousal_level;

    /* Higher arousal opens gates (more sensory processing) */
    float target_gate = reticular->config.thalamic_gate_baseline +
                        (arousal - 0.5f) * 0.4f;
    target_gate = nimcp_clampf(target_gate, 0.0f, 1.0f);

    /* Apply attention bias */
    target_gate *= (1.0f + reticular->sensory_gate.attention_bias * 0.3f);
    target_gate = nimcp_clampf(target_gate, 0.0f, 1.0f);

    /* Apply habituation */
    target_gate *= (1.0f - reticular->sensory_gate.habituation_level * 0.3f);

    reticular->sensory_gate.thalamic_gate = exp_decay(
        reticular->sensory_gate.thalamic_gate, target_gate, 0.2f, dt);

    /* Modality-specific gates follow overall gate with slight variations */
    reticular->sensory_gate.visual_gate = reticular->sensory_gate.thalamic_gate * 1.05f;
    reticular->sensory_gate.auditory_gate = reticular->sensory_gate.thalamic_gate * 1.0f;
    reticular->sensory_gate.somatosensory_gate = reticular->sensory_gate.thalamic_gate * 0.95f;

    /* Clamp modality gates */
    reticular->sensory_gate.visual_gate =
        nimcp_clampf(reticular->sensory_gate.visual_gate, 0.0f, 1.0f);
    reticular->sensory_gate.auditory_gate =
        nimcp_clampf(reticular->sensory_gate.auditory_gate, 0.0f, 1.0f);
    reticular->sensory_gate.somatosensory_gate =
        nimcp_clampf(reticular->sensory_gate.somatosensory_gate, 0.0f, 1.0f);

    /* Decay habituation */
    reticular->sensory_gate.habituation_level *= expf(-dt * 0.1f);

    nimcp_mutex_unlock(reticular->mutex);
    return 0;
}

float reticular_get_thalamic_gate(const nimcp_reticular_t* reticular) {
    if (!reticular) return 0.5f;
    return reticular->sensory_gate.thalamic_gate;
}

int reticular_set_attention_bias(nimcp_reticular_t* reticular, float bias) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }

    nimcp_mutex_lock(reticular->mutex);
    reticular->sensory_gate.attention_bias = nimcp_clampf(bias, 0.0f, 1.0f);
    reticular->stats.attention_alerts++;
    nimcp_mutex_unlock(reticular->mutex);

    return 0;
}

int reticular_apply_habituation(nimcp_reticular_t* reticular, float stimulus) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }

    nimcp_mutex_lock(reticular->mutex);

    reticular->sensory_gate.habituation_level +=
        stimulus * reticular->config.habituation_rate;
    reticular->sensory_gate.habituation_level =
        nimcp_clampf(reticular->sensory_gate.habituation_level, 0.0f, 1.0f);

    nimcp_mutex_unlock(reticular->mutex);
    return 0;
}

float reticular_get_modality_gate(const nimcp_reticular_t* reticular, int modality) {
    if (!reticular) return 0.5f;

    switch (modality) {
        case 0: return reticular->sensory_gate.visual_gate;
        case 1: return reticular->sensory_gate.auditory_gate;
        case 2: return reticular->sensory_gate.somatosensory_gate;
        default: return reticular->sensory_gate.thalamic_gate;
    }
}

/*=============================================================================
 * CIRCADIAN INTEGRATION
 *===========================================================================*/

int reticular_set_circadian_input(nimcp_reticular_t* reticular,
                                  float circadian_phase,
                                  float circadian_amplitude) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }

    nimcp_mutex_lock(reticular->mutex);

    /* Phase wraps at 24 hours */
    while (circadian_phase < 0.0f) circadian_phase += 24.0f;
    while (circadian_phase >= 24.0f) circadian_phase -= 24.0f;

    /* Compute circadian drive: high during day (6-22), low at night */
    float phase_rad = (circadian_phase / 24.0f) * NIMCP_TWO_PI_F;
    float base_drive = 0.5f + 0.5f * cosf(phase_rad - NIMCP_PI_F); /* Peak at noon */

    reticular->circadian_drive = base_drive * circadian_amplitude;

    /* Modulate orexin based on circadian drive */
    reticular->modulators[RETICULAR_MODULATOR_OREXIN].release_rate =
        reticular->circadian_drive * 0.3f;

    nimcp_mutex_unlock(reticular->mutex);
    return 0;
}

int reticular_update_sleep_pressure(nimcp_reticular_t* reticular, float wake_duration) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }

    nimcp_mutex_lock(reticular->mutex);

    /* Process S: exponential rise during wake, decay during sleep */
    if (reticular->arousal_state > RETICULAR_AROUSAL_REM_SLEEP) {
        /* Awake: pressure rises */
        reticular->homeostatic_sleep_pressure =
            1.0f - expf(-wake_duration / 16.0f); /* Time constant ~16 hours */
    } else {
        /* Asleep: pressure decays */
        reticular->homeostatic_sleep_pressure *= 0.95f;
    }

    reticular->homeostatic_sleep_pressure =
        nimcp_clampf(reticular->homeostatic_sleep_pressure, 0.0f, 1.0f);

    nimcp_mutex_unlock(reticular->mutex);
    return 0;
}

float reticular_get_sleep_propensity(const nimcp_reticular_t* reticular) {
    if (!reticular) return 0.0f;

    /* Two-process model: sleep propensity = pressure - circadian drive */
    float propensity = reticular->homeostatic_sleep_pressure -
                       reticular->circadian_drive;
    return nimcp_clampf(propensity, 0.0f, 1.0f);
}

/*=============================================================================
 * KG WIRING INTEGRATION
 *===========================================================================*/

int reticular_kg_register(nimcp_reticular_t* reticular,
                          struct nimcp_brain_kg* kg,
                          uint64_t admin_token) {
    if (!reticular || !kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_get_sleep_propensity: required parameter is NULL (reticular, kg)");
        return -1;
    }

    nimcp_mutex_lock(reticular->mutex);

    reticular->kg = kg;
    reticular->kg_state.admin_token = admin_token;

    /* Register main reticular node */
    reticular->kg_state.region_node_id = RETICULAR_MODULE_ID;

    /* Register nucleus nodes */
    for (int i = 0; i < RETICULAR_NUCLEUS_COUNT; i++) {
        reticular->kg_state.nucleus_node_ids[i] = RETICULAR_MODULE_ID + 1 + i;
    }

    /* Register arousal state nodes */
    for (int i = 0; i < RETICULAR_AROUSAL_COUNT; i++) {
        reticular->kg_state.arousal_node_ids[i] = RETICULAR_MODULE_ID + 100 + i;
    }

    /* Register modulator nodes */
    for (int i = 0; i < RETICULAR_MODULATOR_COUNT; i++) {
        reticular->kg_state.modulator_node_ids[i] = RETICULAR_MODULE_ID + 200 + i;
    }

    reticular->kg_state.registered = true;
    reticular->stats.kg_updates++;

    nimcp_mutex_unlock(reticular->mutex);

    NIMCP_LOG_INFO(RETICULAR_LOG_TAG, "Registered with KG, node_id=%lu",
                   reticular->kg_state.region_node_id);

    return 0;
}

int reticular_kg_unregister(nimcp_reticular_t* reticular) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }

    nimcp_mutex_lock(reticular->mutex);

    if (reticular->kg_state.registered) {
        reticular->kg_state.registered = false;
        reticular->kg = NULL;
    }

    nimcp_mutex_unlock(reticular->mutex);
    return 0;
}

/* reticular_kg_update_state is implemented in nimcp_reticular_kg_wiring.c */

int reticular_kg_query(nimcp_reticular_t* reticular, const char* query,
                       void* result, size_t result_size) {
    if (!reticular || !query || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_kg_unregister: required parameter is NULL (reticular, query, result)");
        return -1;
    }
    if (!reticular->kg_state.registered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_kg_unregister: reticular->kg_state is NULL");
        return -1;
    }

    /* Query KG - actual implementation would use KG API */
    (void)result_size;

    return 0;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

int reticular_bio_async_connect(nimcp_reticular_t* reticular,
                                struct nimcp_bio_router* router) {
    if (!reticular || !router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_kg_unregister: required parameter is NULL (reticular, router)");
        return -1;
    }

    nimcp_mutex_lock(reticular->mutex);

    reticular->bio_router = router;
    reticular->connected = true;

    /* Register message handler - actual bio-async registration would go here */

    nimcp_mutex_unlock(reticular->mutex);

    NIMCP_LOG_INFO(RETICULAR_LOG_TAG, "Connected to bio-async router");
    return 0;
}

int reticular_bio_async_disconnect(nimcp_reticular_t* reticular) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }

    nimcp_mutex_lock(reticular->mutex);

    if (reticular->bio_router) {
        reticular->bio_router = NULL;
        reticular->connected = false;
    }

    nimcp_mutex_unlock(reticular->mutex);
    return 0;
}

int reticular_bio_async_broadcast(nimcp_reticular_t* reticular,
                                  reticular_bio_msg_type_t msg_type,
                                  const void* payload,
                                  size_t payload_size) {
    if (!reticular || !reticular->bio_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_bio_async_disconnect: required parameter is NULL (reticular, reticular->bio_router)");
        return -1;
    }

    reticular->stats.bio_msgs_sent++;

    /* Actual broadcast implementation would use bio-async API */
    (void)msg_type;
    (void)payload;
    (void)payload_size;

    return 0;
}

int reticular_bio_async_subscribe(nimcp_reticular_t* reticular,
                                  uint32_t subscription_mask) {
    if (!reticular || !reticular->bio_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_bio_async_disconnect: required parameter is NULL (reticular, reticular->bio_router)");
        return -1;
    }

    /* Subscribe to messages */
    (void)subscription_mask;

    return 0;
}

/*=============================================================================
 * OTHER SYSTEM INTEGRATIONS
 *===========================================================================*/

int reticular_immune_connect(nimcp_reticular_t* reticular,
                             struct nimcp_immune_system* immune) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }
    reticular->immune = immune;
    NIMCP_LOG_DEBUG(RETICULAR_LOG_TAG, "Connected to immune system");
    return 0;
}

int reticular_security_connect(nimcp_reticular_t* reticular,
                               struct nimcp_security_context* security) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }
    reticular->security = security;
    NIMCP_LOG_DEBUG(RETICULAR_LOG_TAG, "Connected to security context");
    return 0;
}

int reticular_snn_connect(nimcp_reticular_t* reticular,
                          struct nimcp_snn_network* snn,
                          struct nimcp_plasticity_engine* plasticity) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }
    reticular->snn = snn;
    reticular->plasticity = plasticity;
    NIMCP_LOG_DEBUG(RETICULAR_LOG_TAG, "Connected to SNN/plasticity");
    return 0;
}

int reticular_hypothalamus_connect(nimcp_reticular_t* reticular,
                                   struct nimcp_hypothalamus* hypo) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }
    reticular->hypothalamus = hypo;
    NIMCP_LOG_DEBUG(RETICULAR_LOG_TAG, "Connected to hypothalamus");
    return 0;
}

int reticular_thalamus_connect(nimcp_reticular_t* reticular,
                               struct nimcp_thalamus* thalamus) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }
    reticular->thalamus = thalamus;
    NIMCP_LOG_DEBUG(RETICULAR_LOG_TAG, "Connected to thalamus");
    return 0;
}

int reticular_cognitive_connect(nimcp_reticular_t* reticular,
                                struct nimcp_cognitive_hub* hub) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }
    reticular->cognitive_hub = hub;
    NIMCP_LOG_DEBUG(RETICULAR_LOG_TAG, "Connected to cognitive hub");
    return 0;
}

int reticular_training_connect(nimcp_reticular_t* reticular,
                               struct nimcp_training_context* training) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }
    reticular->training = training;
    NIMCP_LOG_DEBUG(RETICULAR_LOG_TAG, "Connected to training system");
    return 0;
}

int reticular_perception_connect(nimcp_reticular_t* reticular,
                                 struct nimcp_perception_system* perception) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }
    reticular->perception = perception;
    NIMCP_LOG_DEBUG(RETICULAR_LOG_TAG, "Connected to perception system");
    return 0;
}

int reticular_symbolic_connect(nimcp_reticular_t* reticular,
                               struct nimcp_symbolic_engine* symbolic) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }
    reticular->symbolic = symbolic;
    NIMCP_LOG_DEBUG(RETICULAR_LOG_TAG, "Connected to symbolic engine");
    return 0;
}

int reticular_swarm_connect(nimcp_reticular_t* reticular,
                            struct nimcp_swarm_context* swarm) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }
    reticular->swarm = swarm;
    NIMCP_LOG_DEBUG(RETICULAR_LOG_TAG, "Connected to swarm system");
    return 0;
}

int reticular_dragonfly_connect(nimcp_reticular_t* reticular,
                                struct nimcp_dragonfly_context* dragonfly) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }
    reticular->dragonfly = dragonfly;
    NIMCP_LOG_DEBUG(RETICULAR_LOG_TAG, "Connected to dragonfly system");
    return 0;
}

int reticular_portia_connect(nimcp_reticular_t* reticular,
                             struct nimcp_portia_context* portia) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }
    reticular->portia = portia;
    NIMCP_LOG_DEBUG(RETICULAR_LOG_TAG, "Connected to portia system");
    return 0;
}

int reticular_qmc_connect(nimcp_reticular_t* reticular,
                          struct nimcp_qmc_context* qmc) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }
    reticular->qmc = qmc;
    NIMCP_LOG_DEBUG(RETICULAR_LOG_TAG, "Connected to QMC system");
    return 0;
}

int reticular_omni_connect(nimcp_reticular_t* reticular,
                           struct nimcp_omni_predictor* omni) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }
    reticular->omni = omni;
    NIMCP_LOG_DEBUG(RETICULAR_LOG_TAG, "Connected to omnidirectional predictor");
    return 0;
}

int reticular_pag_connect(nimcp_reticular_t* reticular,
                          struct nimcp_pag* pag) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }
    reticular->pag = pag;
    NIMCP_LOG_DEBUG(RETICULAR_LOG_TAG, "Connected to PAG");
    return 0;
}

int reticular_substrate_connect(nimcp_reticular_t* reticular,
                                struct nimcp_neural_substrate* substrate) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }
    reticular->substrate = substrate;
    NIMCP_LOG_DEBUG(RETICULAR_LOG_TAG, "Connected to neural substrate");
    return 0;
}

/*=============================================================================
 * UPDATE AND STATE
 *===========================================================================*/

int reticular_update(nimcp_reticular_t* reticular, float dt) {
    if (!reticular || !reticular->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_update: required parameter is NULL (reticular, reticular->initialized)");
        return -1;
    }

    nimcp_mutex_lock(reticular->mutex);

    /* Update simulation time */
    reticular->simulation_time_us += (uint64_t)(dt * 1000000.0f);

    /* Update all nuclei */
    for (int i = 0; i < RETICULAR_NUCLEUS_COUNT; i++) {
        update_nucleus(&reticular->nuclei[i], dt);

        /* Accumulate modulator release from nuclei */
        reticular_modulator_t mod = reticular->nuclei[i].primary_modulator;
        reticular->modulators[mod].release_rate +=
            reticular->nuclei[i].modulator_output * 0.1f;
    }

    /* Track total neuromodulator release */
    reticular->stats.total_serotonin_released +=
        reticular->modulators[RETICULAR_MODULATOR_SEROTONIN].release_rate * dt;
    reticular->stats.total_norepinephrine_released +=
        reticular->modulators[RETICULAR_MODULATOR_NOREPINEPHRINE].release_rate * dt;
    reticular->stats.total_acetylcholine_released +=
        reticular->modulators[RETICULAR_MODULATOR_ACETYLCHOLINE].release_rate * dt;
    reticular->stats.total_dopamine_released +=
        reticular->modulators[RETICULAR_MODULATOR_DOPAMINE].release_rate * dt;

    /* Update all modulators */
    for (int i = 0; i < RETICULAR_MODULATOR_COUNT; i++) {
        update_modulator(&reticular->modulators[i], dt);
    }

    /* Reset release rates for next cycle */
    for (int i = 0; i < RETICULAR_MODULATOR_COUNT; i++) {
        reticular->modulators[i].release_rate *= 0.9f;
    }

    /* Update reflex states (decay activations) */
    for (int i = 0; i < RETICULAR_REFLEX_COUNT; i++) {
        reticular->reflexes[i].current_activation *= expf(-dt / 0.1f);
        if (reticular->reflexes[i].active &&
            reticular->reflexes[i].current_activation < 0.1f) {
            reticular->reflexes[i].active = false;
        }
    }

    nimcp_mutex_unlock(reticular->mutex);

    /* Update subsystems (these have their own locking) */
    reticular_update_arousal(reticular, dt);
    reticular_update_autonomic(reticular, dt);
    reticular_update_motor_tone(reticular, dt);
    reticular_update_pain_modulation(reticular, dt);
    reticular_update_sensory_gating(reticular, dt);

    /* Update KG if connected */
    if (reticular->kg && reticular->config.enable_kg_wiring) {
        reticular_kg_update_state(reticular);
    }

    reticular->last_update_us = reticular->simulation_time_us;

    return 0;
}

int reticular_get_stats(const nimcp_reticular_t* reticular,
                        reticular_stats_t* stats) {
    if (!reticular || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_update: required parameter is NULL (reticular, stats)");
        return -1;
    }

    nimcp_mutex_lock(((nimcp_reticular_t*)reticular)->mutex);
    memcpy(stats, &reticular->stats, sizeof(reticular_stats_t));
    nimcp_mutex_unlock(((nimcp_reticular_t*)reticular)->mutex);

    return 0;
}

int reticular_reset_stats(nimcp_reticular_t* reticular) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }

    nimcp_mutex_lock(reticular->mutex);
    memset(&reticular->stats, 0, sizeof(reticular_stats_t));
    nimcp_mutex_unlock(reticular->mutex);

    return 0;
}

/*=============================================================================
 * QUANTUM OPTIMIZATION
 *===========================================================================*/

int reticular_qmc_optimize_arousal(nimcp_reticular_t* reticular) {
    if (!reticular || !reticular->qmc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_qmc_optimize_arousal: required parameter is NULL (reticular, reticular->qmc)");
        return -1;
    }

    /* Use QMC for arousal optimization - actual implementation would use QMC API */

    NIMCP_LOG_DEBUG(RETICULAR_LOG_TAG, "QMC arousal optimization requested");
    return 0;
}

int reticular_qmcts_predict_state(nimcp_reticular_t* reticular,
                                  uint32_t num_iterations,
                                  reticular_arousal_state_t* predicted_state) {
    if (!reticular || !predicted_state || !reticular->qmc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_qmc_optimize_arousal: required parameter is NULL (reticular, predicted_state, reticular->qmc)");
        return -1;
    }

    /* Use QMCTS for state prediction - actual implementation would use QMCTS API */

    NIMCP_LOG_DEBUG(RETICULAR_LOG_TAG, "QMCTS state prediction: %u iterations",
                    num_iterations);

    /* Fallback to current state */
    *predicted_state = reticular->arousal_state;
    return 0;
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

const char* reticular_arousal_state_string(reticular_arousal_state_t state) {
    static const char* names[] = {
        "Deep Sleep",
        "Light Sleep",
        "REM Sleep",
        "Drowsy",
        "Relaxed",
        "Alert",
        "Hypervigilant"
    };
    if (state >= RETICULAR_AROUSAL_COUNT) return "Unknown";
    return names[state];
}

const char* reticular_nucleus_string(reticular_nucleus_t nucleus) {
    if (nucleus >= RETICULAR_NUCLEUS_COUNT) return "Unknown";
    return s_nucleus_names[nucleus];
}

const char* reticular_modulator_string(reticular_modulator_t modulator) {
    static const char* names[] = {
        "Serotonin",
        "Norepinephrine",
        "Acetylcholine",
        "Dopamine",
        "Histamine",
        "Orexin",
        "GABA",
        "Glutamate"
    };
    if (modulator >= RETICULAR_MODULATOR_COUNT) return "Unknown";
    return names[modulator];
}

const char* reticular_reflex_string(reticular_reflex_t reflex) {
    static const char* names[] = {
        "Swallowing",
        "Coughing",
        "Vomiting",
        "Sneezing",
        "Gagging",
        "Yawning",
        "Startle",
        "Righting"
    };
    if (reflex >= RETICULAR_REFLEX_COUNT) return "Unknown";
    return names[reflex];
}

const char* reticular_autonomic_string(reticular_autonomic_t function) {
    static const char* names[] = {
        "Cardiovascular",
        "Respiratory",
        "Vasomotor",
        "Digestive"
    };
    if (function >= RETICULAR_AUTONOMIC_COUNT) return "Unknown";
    return names[function];
}
