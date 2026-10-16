//=============================================================================
// nimcp_subthalamic.c - Subthalamic Nucleus Implementation
//=============================================================================
/**
 * @file nimcp_subthalamic.c
 * @brief Subthalamic nucleus implementation for action suppression
 *
 * WHAT: STN model for fast global action suppression
 * WHY:  Prevents premature actions via hyperdirect pathway
 * HOW:  Receives cortical (hyperdirect) and GPe (indirect) input
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 */

#include "core/brain/subcortical/nimcp_subthalamic.h"
#include "api/nimcp_api_exception.h"
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
#include "constants/nimcp_learning_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(subthalamic, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


//=============================================================================
// Lifecycle Functions
//=============================================================================

void subthalamic_default_config(subthalamic_config_t* config) {
    if (!config) return;

    config->num_neurons = STN_DEFAULT_NEURONS;
    config->num_actions = 8;
    config->tonic_firing_rate = STN_TONIC_FIRING;
    config->max_firing_rate = STN_MAX_FIRING;
    config->hyperdirect_gain = 2.0f;
    config->indirect_gain = 1.5f;
    config->hyperdirect_delay_ms = STN_HYPERDIRECT_DELAY;
    config->indirect_delay_ms = STN_INDIRECT_DELAY;
    config->urgency_threshold = 0.8f;
}

subthalamic_nucleus_t* subthalamic_create(const subthalamic_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subthalamic_create: config is NULL");
        return NULL;
    }

    subthalamic_nucleus_t* stn = nimcp_malloc(sizeof(subthalamic_nucleus_t));
    if (!stn) {
        NIMCP_LOGGING_ERROR("Failed to allocate subthalamic nucleus");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stn is NULL");

        return NULL;
    }
    memset(stn, 0, sizeof(subthalamic_nucleus_t));

    stn->num_neurons = config->num_neurons;
    stn->num_actions = config->num_actions;
    stn->config = *config;
    stn->mode = STN_MODE_BASELINE;

    /* Allocate neurons */
    stn->neurons = nimcp_calloc(config->num_neurons, sizeof(stn_neuron_t));
    if (!stn->neurons) {
        NIMCP_LOGGING_ERROR("Failed to allocate STN neurons");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "subthalamic_create: failed to allocate neurons");
        nimcp_free(stn);
        return NULL;
    }

    /* Initialize neurons */
    for (uint32_t i = 0; i < config->num_neurons; i++) {
        stn->neurons[i].id = i;
        stn->neurons[i].firing_rate = config->tonic_firing_rate;
        stn->neurons[i].membrane_potential = -60.0f;
        stn->neurons[i].cortical_input = 0.0f;
        stn->neurons[i].gpe_inhibition = 0.0f;
    }

    /* Allocate output and input buffers */
    stn->output = nimcp_calloc(config->num_actions, sizeof(float));
    stn->cortical_input = nimcp_calloc(config->num_actions, sizeof(float));
    stn->gpe_input = nimcp_calloc(config->num_actions, sizeof(float));

    if (!stn->output || !stn->cortical_input || !stn->gpe_input) {
        NIMCP_LOGGING_ERROR("Failed to allocate STN buffers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "subthalamic_create: failed to allocate buffers");
        if (stn->output) nimcp_free(stn->output);
        if (stn->cortical_input) nimcp_free(stn->cortical_input);
        if (stn->gpe_input) nimcp_free(stn->gpe_input);
        nimcp_free(stn->neurons);
        nimcp_free(stn);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subthalamic_create: validation failed");
        return NULL;
    }

    /* Initialize output with baseline */
    for (uint32_t a = 0; a < config->num_actions; a++) {
        stn->output[a] = config->tonic_firing_rate / config->max_firing_rate;
    }
    stn->global_output = config->tonic_firing_rate / config->max_firing_rate;

    /* Calculate delay samples (assuming 1ms time step) */
    stn->hyperdirect_delay_samples = (uint32_t)(config->hyperdirect_delay_ms);
    stn->indirect_delay_samples = (uint32_t)(config->indirect_delay_ms);

    /* Allocate delay buffers if delays are significant */
    if (stn->hyperdirect_delay_samples > 0) {
        stn->hyperdirect_buffer = nimcp_calloc(
            stn->hyperdirect_delay_samples * config->num_actions, sizeof(float));
    }
    if (stn->indirect_delay_samples > 0) {
        stn->indirect_buffer = nimcp_calloc(
            stn->indirect_delay_samples * config->num_actions, sizeof(float));
    }
    stn->buffer_index = 0;

    /* Allocate mutex */
    stn->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!stn->mutex) {
        NIMCP_LOGGING_ERROR("Failed to allocate STN mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "subthalamic_create: failed to allocate mutex");
        if (stn->hyperdirect_buffer) nimcp_free(stn->hyperdirect_buffer);
        if (stn->indirect_buffer) nimcp_free(stn->indirect_buffer);
        nimcp_free(stn->output);
        nimcp_free(stn->cortical_input);
        nimcp_free(stn->gpe_input);
        nimcp_free(stn->neurons);
        nimcp_free(stn);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subthalamic_create: validation failed");
        return NULL;
    }
    nimcp_mutex_init(stn->mutex, NULL);

    NIMCP_LOGGING_DEBUG("Created subthalamic nucleus with %u neurons, %u actions",
                        config->num_neurons, config->num_actions);

    return stn;
}

void subthalamic_destroy(subthalamic_nucleus_t* stn) {
    if (!stn) return;

    // Don't lock mutex during destruction - object is being destroyed
    // Other threads should not be accessing a dying object
    nimcp_free(stn->neurons);
    nimcp_free(stn->output);
    nimcp_free(stn->cortical_input);
    nimcp_free(stn->gpe_input);
    if (stn->hyperdirect_buffer) nimcp_free(stn->hyperdirect_buffer);
    if (stn->indirect_buffer) nimcp_free(stn->indirect_buffer);

    nimcp_mutex_destroy(stn->mutex);
    nimcp_free(stn->mutex);

    nimcp_free(stn);

    NIMCP_LOGGING_DEBUG("Destroyed subthalamic nucleus");
}

int subthalamic_reset(subthalamic_nucleus_t* stn) {
    if (!stn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stn is NULL");

        return -1;

    }

    nimcp_mutex_lock(stn->mutex);

    stn->mode = STN_MODE_BASELINE;

    /* Reset neurons */
    for (uint32_t i = 0; i < stn->num_neurons; i++) {
        stn->neurons[i].firing_rate = stn->config.tonic_firing_rate;
        stn->neurons[i].cortical_input = 0.0f;
        stn->neurons[i].gpe_inhibition = 0.0f;
    }

    /* Reset output */
    for (uint32_t a = 0; a < stn->num_actions; a++) {
        stn->output[a] = stn->config.tonic_firing_rate / stn->config.max_firing_rate;
    }
    stn->global_output = stn->config.tonic_firing_rate / stn->config.max_firing_rate;

    /* Clear inputs */
    memset(stn->cortical_input, 0, stn->num_actions * sizeof(float));
    memset(stn->gpe_input, 0, stn->num_actions * sizeof(float));

    /* Clear delay buffers */
    if (stn->hyperdirect_buffer) {
        memset(stn->hyperdirect_buffer, 0,
               stn->hyperdirect_delay_samples * stn->num_actions * sizeof(float));
    }
    if (stn->indirect_buffer) {
        memset(stn->indirect_buffer, 0,
               stn->indirect_delay_samples * stn->num_actions * sizeof(float));
    }
    stn->buffer_index = 0;

    /* Reset statistics */
    memset(&stn->stats, 0, sizeof(stn->stats));

    nimcp_mutex_unlock(stn->mutex);

    return 0;
}

//=============================================================================
// Input Functions
//=============================================================================

int subthalamic_set_cortical_input(subthalamic_nucleus_t* stn,
                                    const float* input,
                                    bool global) {
    if (!stn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subthalamic_set_cortical_input: stn is NULL");
        return -1;
    }
    if (!input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subthalamic_set_cortical_input: input is NULL");
        return -1;
    }

    nimcp_mutex_lock(stn->mutex);

    if (global) {
        /* Single global stop signal */
        float val = input[0];
        for (uint32_t a = 0; a < stn->num_actions; a++) {
            stn->cortical_input[a] = val;
        }
        if (val > stn->config.urgency_threshold) {
            stn->mode = STN_MODE_HYPERDIRECT;
        }
    } else {
        /* Per-action input */
        memcpy(stn->cortical_input, input, stn->num_actions * sizeof(float));

        /* Check for hyperdirect activation */
        float max_input = 0;
        for (uint32_t a = 0; a < stn->num_actions; a++) {
            if (stn->cortical_input[a] > max_input) {
                max_input = stn->cortical_input[a];
            }
        }
        if (max_input > stn->config.urgency_threshold) {
            stn->mode = STN_MODE_HYPERDIRECT;
        }
    }

    nimcp_mutex_unlock(stn->mutex);

    return 0;
}

int subthalamic_set_gpe_input(subthalamic_nucleus_t* stn, const float* input) {
    if (!stn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subthalamic_set_gpe_input: stn is NULL");
        return -1;
    }
    if (!input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subthalamic_set_gpe_input: input is NULL");
        return -1;
    }

    nimcp_mutex_lock(stn->mutex);
    memcpy(stn->gpe_input, input, stn->num_actions * sizeof(float));

    /* Check for indirect pathway activation (GPe reduced → STN disinhibition) */
    float avg_gpe = 0;
    for (uint32_t a = 0; a < stn->num_actions; a++) {
        avg_gpe += stn->gpe_input[a];
    }
    avg_gpe /= stn->num_actions;

    if (avg_gpe < 0.3f && stn->mode == STN_MODE_BASELINE) {
        stn->mode = STN_MODE_INDIRECT;
    }

    nimcp_mutex_unlock(stn->mutex);

    return 0;
}

int subthalamic_emergency_stop(subthalamic_nucleus_t* stn, float urgency) {
    if (!stn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stn is NULL");

        return -1;

    }

    nimcp_mutex_lock(stn->mutex);

    /* Set maximum cortical input for all actions */
    for (uint32_t a = 0; a < stn->num_actions; a++) {
        stn->cortical_input[a] = urgency;
    }

    stn->mode = STN_MODE_SUPPRESSION;
    stn->stats.suppression_events++;

    /* Immediately update output to reflect emergency state */
    float max_rate = stn->config.max_firing_rate;
    float hd_gain = stn->config.hyperdirect_gain;
    float tonic = stn->config.tonic_firing_rate;

    float global_sum = 0;
    for (uint32_t a = 0; a < stn->num_actions; a++) {
        /* Emergency firing: strongly elevated due to urgency */
        float firing_rate = tonic + (urgency * hd_gain * tonic * 4.0f);
        firing_rate = fmaxf(0.0f, fminf(max_rate, firing_rate));
        stn->output[a] = firing_rate / max_rate;
        global_sum += stn->output[a];
    }
    stn->global_output = global_sum / stn->num_actions;

    NIMCP_LOGGING_DEBUG("STN emergency stop with urgency %.2f, global_output=%.2f",
                        urgency, stn->global_output);

    nimcp_mutex_unlock(stn->mutex);

    return 0;
}

//=============================================================================
// Processing Functions
//=============================================================================

int subthalamic_process(subthalamic_nucleus_t* stn) {
    if (!stn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stn is NULL");

        return -1;

    }

    nimcp_mutex_lock(stn->mutex);

    float tonic = stn->config.tonic_firing_rate;
    float max_rate = stn->config.max_firing_rate;
    float hd_gain = stn->config.hyperdirect_gain;
    float ind_gain = stn->config.indirect_gain;

    float global_sum = 0;

    for (uint32_t a = 0; a < stn->num_actions; a++) {
        float cortical = stn->cortical_input[a];
        float gpe = stn->gpe_input[a];

        /* Hyperdirect pathway: cortical excitation */
        float hd_excite = cortical * hd_gain;

        /* Indirect pathway: GPe inhibition (low GPe = disinhibition = increased firing) */
        float ind_disinhibit = (1.0f - gpe) * ind_gain;

        /* Compute firing rate */
        float firing_rate = tonic + (hd_excite * tonic) + (ind_disinhibit * tonic * 0.5f);

        /* Clamp */
        firing_rate = fmaxf(0.0f, fminf(max_rate, firing_rate));

        /* Normalize */
        stn->output[a] = firing_rate / max_rate;
        global_sum += stn->output[a];
    }

    /* Global output is average */
    stn->global_output = global_sum / stn->num_actions;

    /* Update mode based on activity */
    /* Note: baseline output is tonic/max (~0.08), so thresholds adjusted */
    float baseline_output = stn->config.tonic_firing_rate / stn->config.max_firing_rate;
    if (stn->global_output > baseline_output * 3.0f) {
        /* Significantly elevated - hyperdirect activation */
        if (stn->mode != STN_MODE_SUPPRESSION) {
            stn->mode = STN_MODE_HYPERDIRECT;
        }
    } else if (stn->global_output <= baseline_output * 1.2f) {
        /* Near baseline - return to baseline mode (unless suppression) */
        if (stn->mode != STN_MODE_SUPPRESSION) {
            stn->mode = STN_MODE_BASELINE;
        }
    }

    /* Update statistics */
    stn->stats.avg_firing_rate = stn->global_output * max_rate;

    /* Track max firing rate across process calls */
    float current_max = stn->global_output * max_rate;
    if (current_max > stn->stats.max_firing_rate) {
        stn->stats.max_firing_rate = current_max;
    }

    nimcp_mutex_unlock(stn->mutex);

    return 0;
}

int subthalamic_get_output(const subthalamic_nucleus_t* stn, float* output) {
    if (!stn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subthalamic_get_output: stn is NULL");
        return -1;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subthalamic_get_output: output is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)stn->mutex);
    memcpy(output, stn->output, stn->num_actions * sizeof(float));
    nimcp_mutex_unlock((nimcp_mutex_t*)stn->mutex);

    return 0;
}

float subthalamic_get_global_output(const subthalamic_nucleus_t* stn) {
    if (!stn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subthalamic_get_global_output: stn is NULL");
        return 0.0f;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)stn->mutex);
    float global = stn->global_output;
    nimcp_mutex_unlock((nimcp_mutex_t*)stn->mutex);

    return global;
}

int subthalamic_step(subthalamic_nucleus_t* stn, float dt) {
    if (!stn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stn is NULL");

        return -1;

    }

    nimcp_mutex_lock(stn->mutex);

    /* Decay cortical input over time */
    float decay = NIMCP_EMA_DECAY_FAST;  /* Per ms */
    for (uint32_t a = 0; a < stn->num_actions; a++) {
        stn->cortical_input[a] *= powf(decay, dt);
    }

    /* Check if we should return to baseline */
    float max_input = 0;
    for (uint32_t a = 0; a < stn->num_actions; a++) {
        if (stn->cortical_input[a] > max_input) {
            max_input = stn->cortical_input[a];
        }
    }
    if (max_input < 0.1f && stn->mode == STN_MODE_SUPPRESSION) {
        stn->mode = STN_MODE_BASELINE;
    }

    nimcp_mutex_unlock(stn->mutex);

    return subthalamic_process(stn);
}

stn_mode_t subthalamic_get_mode(const subthalamic_nucleus_t* stn) {
    if (!stn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subthalamic_get_mode: stn is NULL");
        return STN_MODE_BASELINE;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)stn->mutex);
    stn_mode_t mode = stn->mode;
    nimcp_mutex_unlock((nimcp_mutex_t*)stn->mutex);

    return mode;
}

//=============================================================================
// Statistics Functions
//=============================================================================

int subthalamic_get_stats(const subthalamic_nucleus_t* stn, stn_stats_t* stats) {
    if (!stn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subthalamic_get_stats: stn is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subthalamic_get_stats: stats is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)stn->mutex);
    *stats = stn->stats;
    nimcp_mutex_unlock((nimcp_mutex_t*)stn->mutex);

    return 0;
}

const char* subthalamic_mode_name(stn_mode_t mode) {
    switch (mode) {
        case STN_MODE_BASELINE: return "Baseline";
        case STN_MODE_HYPERDIRECT: return "Hyperdirect";
        case STN_MODE_INDIRECT: return "Indirect";
        case STN_MODE_SUPPRESSION: return "Suppression";
        default: return "Unknown";
    }
}
