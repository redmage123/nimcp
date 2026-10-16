//=============================================================================
// nimcp_substantia_nigra.c - Substantia Nigra Implementation
//=============================================================================
/**
 * @file nimcp_substantia_nigra.c
 * @brief Substantia nigra implementation (SNc dopamine and SNr output)
 *
 * WHAT: SN model with dopamine production (SNc) and output pathway (SNr)
 * WHY:  SNc provides reward signals; SNr is BG output to thalamus
 * HOW:  SNc signals reward prediction error; SNr similar to GPi
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 */

#include "core/brain/subcortical/nimcp_substantia_nigra.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(substantia_nigra, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


//=============================================================================
// Lifecycle Functions
//=============================================================================

void substantia_nigra_default_config(substantia_nigra_config_t* config,
                                      sn_part_t part) {
    if (!config) return;

    config->part = part;
    config->num_neurons = SN_DEFAULT_NEURONS;
    config->num_actions = 8;

    if (part == SN_PART_COMPACTA) {
        config->tonic_firing_rate = SNC_TONIC_FIRING;
        config->burst_firing_rate = SNC_BURST_FIRING;
        config->pause_firing_rate = SNC_PAUSE_FIRING;
        config->dopamine_release_rate = DOPAMINE_RELEASE_RATE;
        config->rpe_burst_threshold = 0.1f;
        config->rpe_pause_threshold = -0.1f;
    } else {
        config->tonic_firing_rate = SNR_TONIC_FIRING;
        config->burst_firing_rate = 0.0f;
        config->pause_firing_rate = 0.0f;
        config->dopamine_release_rate = 0.0f;
        config->rpe_burst_threshold = 0.0f;
        config->rpe_pause_threshold = 0.0f;
    }
}

substantia_nigra_t* substantia_nigra_create(const substantia_nigra_config_t* config) {
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config for substantia nigra");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;
    }
    if (config->num_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substantia_nigra_create: num_neurons is zero");
        return NULL;
    }

    substantia_nigra_t* sn = nimcp_malloc(sizeof(substantia_nigra_t));
    if (!sn) {
        NIMCP_LOGGING_ERROR("Failed to allocate substantia nigra");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sn is NULL");

        return NULL;
    }
    memset(sn, 0, sizeof(substantia_nigra_t));

    sn->part = config->part;
    sn->num_neurons = config->num_neurons;
    sn->num_actions = config->num_actions;
    sn->config = *config;
    sn->dopamine_level = 0.5f;  /* Baseline */

    if (config->part == SN_PART_COMPACTA) {
        /* Allocate dopamine neurons */
        sn->da_neurons = nimcp_calloc(config->num_neurons, sizeof(da_neuron_t));
        if (!sn->da_neurons) {
            NIMCP_LOGGING_ERROR("Failed to allocate DA neurons");
            nimcp_free(sn);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "substantia_nigra_create: sn->da_neurons is NULL");
            return NULL;
        }

        /* Initialize DA neurons */
        for (uint32_t i = 0; i < config->num_neurons; i++) {
            sn->da_neurons[i].id = i;
            sn->da_neurons[i].firing_rate = config->tonic_firing_rate;
            sn->da_neurons[i].state = DA_STATE_TONIC;
            sn->da_neurons[i].dopamine_released = 0.0f;
            sn->da_neurons[i].membrane_potential = -60.0f;
        }

        sn->da_state = DA_STATE_TONIC;
    } else {
        /* Allocate SNr neurons */
        sn->snr_neurons = nimcp_calloc(config->num_neurons, sizeof(snr_neuron_t));
        if (!sn->snr_neurons) {
            NIMCP_LOGGING_ERROR("Failed to allocate SNr neurons");
            nimcp_free(sn);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "substantia_nigra_create: sn->snr_neurons is NULL");
            return NULL;
        }

        /* Initialize SNr neurons */
        if (config->num_actions == 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substantia_nigra_create: num_actions is zero");
            nimcp_free(sn->snr_neurons);
            nimcp_free(sn);
            return NULL;
        }
        uint32_t neurons_per_action = config->num_neurons / config->num_actions;
        for (uint32_t i = 0; i < config->num_neurons; i++) {
            sn->snr_neurons[i].id = i;
            sn->snr_neurons[i].firing_rate = config->tonic_firing_rate;
            sn->snr_neurons[i].inhibition = 0.0f;
            sn->snr_neurons[i].target_action = i / neurons_per_action;
            if (sn->snr_neurons[i].target_action >= config->num_actions) {
                sn->snr_neurons[i].target_action = config->num_actions - 1;
            }
        }

        /* Allocate output and input buffers */
        sn->output = nimcp_calloc(config->num_actions, sizeof(float));
        sn->striatal_input = nimcp_calloc(config->num_actions, sizeof(float));
        sn->stn_input = nimcp_calloc(config->num_actions, sizeof(float));

        if (!sn->output || !sn->striatal_input || !sn->stn_input) {
            NIMCP_LOGGING_ERROR("Failed to allocate SNr buffers");
            if (sn->output) nimcp_free(sn->output);
            if (sn->striatal_input) nimcp_free(sn->striatal_input);
            if (sn->stn_input) nimcp_free(sn->stn_input);
            nimcp_free(sn->snr_neurons);
            nimcp_free(sn);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substantia_nigra_create: validation failed");
            return NULL;
        }

        /* Initialize output with tonic firing */
        float max_rate = 200.0f;
        for (uint32_t a = 0; a < config->num_actions; a++) {
            sn->output[a] = config->tonic_firing_rate / max_rate;
        }
    }

    /* Allocate mutex */
    sn->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!sn->mutex) {
        NIMCP_LOGGING_ERROR("Failed to allocate SN mutex");
        if (sn->da_neurons) nimcp_free(sn->da_neurons);
        if (sn->snr_neurons) nimcp_free(sn->snr_neurons);
        if (sn->output) nimcp_free(sn->output);
        if (sn->striatal_input) nimcp_free(sn->striatal_input);
        if (sn->stn_input) nimcp_free(sn->stn_input);
        nimcp_free(sn);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substantia_nigra_create: validation failed");
        return NULL;
    }
    nimcp_mutex_init(sn->mutex, NULL);

    NIMCP_LOGGING_DEBUG("Created substantia nigra %s with %u neurons",
                        substantia_nigra_part_name(config->part),
                        config->num_neurons);

    return sn;
}

void substantia_nigra_destroy(substantia_nigra_t* sn) {
    if (!sn) return;

    nimcp_mutex_lock(sn->mutex);

    if (sn->da_neurons) nimcp_free(sn->da_neurons);
    if (sn->snr_neurons) nimcp_free(sn->snr_neurons);
    if (sn->output) nimcp_free(sn->output);
    if (sn->striatal_input) nimcp_free(sn->striatal_input);
    if (sn->stn_input) nimcp_free(sn->stn_input);

    nimcp_mutex_unlock(sn->mutex);
    nimcp_mutex_destroy(sn->mutex);
    nimcp_free(sn->mutex);

    nimcp_free(sn);

    NIMCP_LOGGING_DEBUG("Destroyed substantia nigra");
}

int substantia_nigra_reset(substantia_nigra_t* sn) {
    if (!sn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sn is NULL");

        return -1;

    }

    nimcp_mutex_lock(sn->mutex);

    sn->dopamine_level = 0.5f;
    sn->reward_prediction = 0.0f;
    sn->rpe = 0.0f;
    sn->da_state = DA_STATE_TONIC;

    if (sn->part == SN_PART_COMPACTA) {
        for (uint32_t i = 0; i < sn->num_neurons; i++) {
            sn->da_neurons[i].firing_rate = sn->config.tonic_firing_rate;
            sn->da_neurons[i].state = DA_STATE_TONIC;
            sn->da_neurons[i].dopamine_released = 0.0f;
        }
    } else {
        for (uint32_t i = 0; i < sn->num_neurons; i++) {
            sn->snr_neurons[i].firing_rate = sn->config.tonic_firing_rate;
            sn->snr_neurons[i].inhibition = 0.0f;
        }

        float max_rate = 200.0f;
        for (uint32_t a = 0; a < sn->num_actions; a++) {
            sn->output[a] = sn->config.tonic_firing_rate / max_rate;
        }

        memset(sn->striatal_input, 0, sn->num_actions * sizeof(float));
        memset(sn->stn_input, 0, sn->num_actions * sizeof(float));
    }

    nimcp_mutex_unlock(sn->mutex);

    return 0;
}

//=============================================================================
// SNc (Dopamine) Functions
//=============================================================================

int snc_update_reward(substantia_nigra_t* sn, float reward, float expected) {
    if (!sn || sn->part != SN_PART_COMPACTA) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snc_update_reward: sn is NULL");
        return -1;
    }

    nimcp_mutex_lock(sn->mutex);

    sn->reward_prediction = expected;

    /* Compute reward prediction error (TD error) */
    float rpe = reward - expected;
    sn->rpe = rpe;

    /* Update dopamine state based on RPE */
    if (rpe > sn->config.rpe_burst_threshold) {
        /* Positive RPE: phasic burst */
        sn->da_state = DA_STATE_BURST;
        for (uint32_t i = 0; i < sn->num_neurons; i++) {
            sn->da_neurons[i].state = DA_STATE_BURST;
            sn->da_neurons[i].firing_rate = sn->config.burst_firing_rate;
        }
    } else if (rpe < sn->config.rpe_pause_threshold) {
        /* Negative RPE: phasic pause */
        sn->da_state = DA_STATE_PAUSE;
        for (uint32_t i = 0; i < sn->num_neurons; i++) {
            sn->da_neurons[i].state = DA_STATE_PAUSE;
            sn->da_neurons[i].firing_rate = sn->config.pause_firing_rate;
        }
    } else {
        /* Within threshold: tonic firing */
        sn->da_state = DA_STATE_TONIC;
        for (uint32_t i = 0; i < sn->num_neurons; i++) {
            sn->da_neurons[i].state = DA_STATE_TONIC;
            sn->da_neurons[i].firing_rate = sn->config.tonic_firing_rate;
        }
    }

    /* Update dopamine level based on firing */
    float avg_firing = 0;
    for (uint32_t i = 0; i < sn->num_neurons; i++) {
        avg_firing += sn->da_neurons[i].firing_rate;
    }
    if (sn->num_neurons > 0) {
        avg_firing /= sn->num_neurons;
    }

    /* Normalize to [0, 1] range */
    float da_range = sn->config.burst_firing_rate - sn->config.pause_firing_rate;
    if (fabsf(da_range) > 0.001f) {
        sn->dopamine_level = (avg_firing - sn->config.pause_firing_rate) / da_range;
    } else {
        sn->dopamine_level = 0.5f;  /* Default baseline when range is zero */
    }
    sn->dopamine_level = fmaxf(0.0f, fminf(1.0f, sn->dopamine_level));

    /* Update statistics */
    sn->stats.avg_firing_rate = avg_firing;
    sn->stats.dopamine_level = sn->dopamine_level;
    sn->stats.avg_rpe = rpe;
    if (sn->da_state == DA_STATE_BURST) sn->stats.burst_count++;
    if (sn->da_state == DA_STATE_PAUSE) sn->stats.pause_count++;

    nimcp_mutex_unlock(sn->mutex);

    return 0;
}

float snc_get_dopamine(const substantia_nigra_t* sn) {
    if (!sn || sn->part != SN_PART_COMPACTA) return 0.5f;

    nimcp_mutex_lock((nimcp_mutex_t*)sn->mutex);
    float da = sn->dopamine_level;
    nimcp_mutex_unlock((nimcp_mutex_t*)sn->mutex);

    return da;
}

float snc_get_rpe(const substantia_nigra_t* sn) {
    if (!sn || sn->part != SN_PART_COMPACTA) return 0.0f;

    nimcp_mutex_lock((nimcp_mutex_t*)sn->mutex);
    float rpe = sn->rpe;
    nimcp_mutex_unlock((nimcp_mutex_t*)sn->mutex);

    return rpe;
}

da_firing_state_t snc_get_state(const substantia_nigra_t* sn) {
    if (!sn || sn->part != SN_PART_COMPACTA) return DA_STATE_TONIC;

    nimcp_mutex_lock((nimcp_mutex_t*)sn->mutex);
    da_firing_state_t state = sn->da_state;
    nimcp_mutex_unlock((nimcp_mutex_t*)sn->mutex);

    return state;
}

int snc_set_prediction(substantia_nigra_t* sn, float prediction) {
    if (!sn || sn->part != SN_PART_COMPACTA) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snc_set_prediction: sn is NULL");
        return -1;
    }

    nimcp_mutex_lock(sn->mutex);
    sn->reward_prediction = prediction;
    nimcp_mutex_unlock(sn->mutex);

    return 0;
}

int snc_step(substantia_nigra_t* sn, float dt) {
    if (!sn || sn->part != SN_PART_COMPACTA) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snc_step: sn is NULL");
        return -1;
    }

    nimcp_mutex_lock(sn->mutex);

    /* Decay back to tonic state over time */
    float decay_rate = 0.1f * dt;  /* Decay time constant */

    for (uint32_t i = 0; i < sn->num_neurons; i++) {
        float target = sn->config.tonic_firing_rate;
        float current = sn->da_neurons[i].firing_rate;
        sn->da_neurons[i].firing_rate += (target - current) * decay_rate;
    }

    /* Check if we've returned to tonic */
    float avg_firing = 0;
    for (uint32_t i = 0; i < sn->num_neurons; i++) {
        avg_firing += sn->da_neurons[i].firing_rate;
    }
    if (sn->num_neurons > 0) {
        avg_firing /= sn->num_neurons;
    }

    if (fabsf(avg_firing - sn->config.tonic_firing_rate) < 0.5f) {
        sn->da_state = DA_STATE_TONIC;
    }

    /* Update dopamine level */
    float da_range = sn->config.burst_firing_rate - sn->config.pause_firing_rate;
    if (fabsf(da_range) > 0.001f) {
        sn->dopamine_level = (avg_firing - sn->config.pause_firing_rate) / da_range;
    } else {
        sn->dopamine_level = 0.5f;  /* Default baseline when range is zero */
    }
    sn->dopamine_level = fmaxf(0.0f, fminf(1.0f, sn->dopamine_level));

    nimcp_mutex_unlock(sn->mutex);

    return 0;
}

//=============================================================================
// SNr (Output) Functions
//=============================================================================

int snr_set_striatal_input(substantia_nigra_t* sn, const float* inhibition) {
    if (!sn || !inhibition || sn->part != SN_PART_RETICULATA) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snr_set_striatal_input: required parameter is NULL (sn, inhibition)");
        return -1;
    }

    nimcp_mutex_lock(sn->mutex);
    memcpy(sn->striatal_input, inhibition, sn->num_actions * sizeof(float));
    nimcp_mutex_unlock(sn->mutex);

    return 0;
}

int snr_set_stn_input(substantia_nigra_t* sn, const float* excitation) {
    if (!sn || !excitation || sn->part != SN_PART_RETICULATA) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snr_set_stn_input: required parameter is NULL (sn, excitation)");
        return -1;
    }

    nimcp_mutex_lock(sn->mutex);
    memcpy(sn->stn_input, excitation, sn->num_actions * sizeof(float));
    nimcp_mutex_unlock(sn->mutex);

    return 0;
}

int snr_process(substantia_nigra_t* sn) {
    if (!sn || sn->part != SN_PART_RETICULATA) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snr_process: sn is NULL");
        return -1;
    }

    nimcp_mutex_lock(sn->mutex);

    float tonic = sn->config.tonic_firing_rate;
    float max_rate = 200.0f;

    for (uint32_t a = 0; a < sn->num_actions; a++) {
        float striatal_inhib = sn->striatal_input[a];
        float stn_excite = sn->stn_input[a];

        /* SNr: inhibited by D1 striatal, excited by STN */
        float firing_rate = tonic * (1.0f - striatal_inhib)
                          + (stn_excite * tonic * 0.5f);

        /* Clamp */
        firing_rate = fmaxf(0.0f, fminf(max_rate, firing_rate));

        /* Normalize */
        sn->output[a] = firing_rate / max_rate;
    }

    nimcp_mutex_unlock(sn->mutex);

    return 0;
}

int snr_get_output(const substantia_nigra_t* sn, float* output) {
    if (!sn || !output || sn->part != SN_PART_RETICULATA) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snr_get_output: required parameter is NULL (sn, output)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)sn->mutex);
    memcpy(output, sn->output, sn->num_actions * sizeof(float));
    nimcp_mutex_unlock((nimcp_mutex_t*)sn->mutex);

    return 0;
}

int snr_step(substantia_nigra_t* sn, float dt) {
    if (!sn || sn->part != SN_PART_RETICULATA) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snr_step: sn is NULL");
        return -1;
    }
    (void)dt;
    return snr_process(sn);
}

//=============================================================================
// Common Functions
//=============================================================================

int substantia_nigra_get_stats(const substantia_nigra_t* sn, sn_stats_t* stats) {
    if (!sn || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substantia_nigra_get_stats: required parameter is NULL (sn, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)sn->mutex);
    *stats = sn->stats;
    nimcp_mutex_unlock((nimcp_mutex_t*)sn->mutex);

    return 0;
}

const char* substantia_nigra_part_name(sn_part_t part) {
    switch (part) {
        case SN_PART_COMPACTA: return "SNc";
        case SN_PART_RETICULATA: return "SNr";
        default: return "Unknown";
    }
}

const char* da_firing_state_name(da_firing_state_t state) {
    switch (state) {
        case DA_STATE_TONIC: return "Tonic";
        case DA_STATE_BURST: return "Burst";
        case DA_STATE_PAUSE: return "Pause";
        default: return "Unknown";
    }
}
