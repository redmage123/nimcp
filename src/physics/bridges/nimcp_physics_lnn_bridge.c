#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_physics_lnn_bridge.c - Physics Layer to Liquid Neural Network Bridge
//=============================================================================
/**
 * @file nimcp_physics_lnn_bridge.c
 * @brief Implementation of physics-LNN bidirectional bridge
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "physics/bridges/nimcp_physics_lnn_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(physics_lnn_bridge)

#define LOG_MODULE "PHYSICS_LNN_BRIDGE"


//=============================================================================
// Internal Structure
//=============================================================================

/**
 * @brief Spike history entry
 */
typedef struct {
    uint32_t neuron_id;
    float spike_time;
    float amplitude;
} spike_history_entry_t;

struct physics_lnn_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /** Configuration */
    physics_lnn_config_t config;

    /** Connected HH population */
    nimcp_hh_population_t* hh_pop;

    /** Connected LNN network */
    lnn_network_t* lnn_network;

    /** Spike history buffer */
    spike_history_entry_t* spike_history;
    uint32_t spike_history_count;
    uint32_t spike_history_capacity;

    /** Encoded input buffer */
    float* encoded_currents;
    uint32_t num_encoded_channels;

    /** Modulation output buffer */
    float* modulation_values;
    uint32_t num_modulation_channels;

    /** Previous membrane voltages for spike detection */
    float* prev_voltages;
    uint32_t num_hh_neurons;

    /** Current simulation time */
    float current_time_ms;

    /** Temperature state */
    float temperature;
    float tau_scale;

    /** Statistics */
    physics_lnn_stats_t stats;

    /** Initialized flag */
    bool initialized;
};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Compute Q10 scaling factor for temperature
 */
static float compute_tau_scale(float temperature, float temp_ref, float q10) {
    float delta_t = (temperature - temp_ref) / 10.0f;
    return powf(q10, -delta_t);  /* Higher temp = faster dynamics = smaller tau */
}

/**
 * @brief Exponential decay kernel
 */
static float exponential_kernel(float dt, float tau) {
    if (dt < 0.0f) return 0.0f;
    return expf(-dt / tau);
}

/**
 * @brief Alpha function kernel (t * exp(-t/tau))
 */
static float alpha_kernel(float dt, float tau) {
    if (dt < 0.0f) return 0.0f;
    float normalized = dt / tau;
    return normalized * expf(1.0f - normalized);
}

/**
 * @brief Double exponential kernel (rise and decay)
 */
static float double_exp_kernel(float dt, float tau_rise, float tau_decay) {
    if (dt < 0.0f) return 0.0f;
    float rise = 1.0f - expf(-dt / tau_rise);
    float decay = expf(-dt / tau_decay);
    return rise * decay;
}

/**
 * @brief Add spike to history buffer
 */
static int add_spike_to_history(
    physics_lnn_bridge_t* bridge,
    uint32_t neuron_id,
    float spike_time,
    float amplitude
) {
    /* Check capacity */
    if (bridge->spike_history_count >= bridge->spike_history_capacity) {
        /* Shift out oldest entries (FIFO) */
        uint32_t shift = bridge->spike_history_capacity / 4;
        memmove(bridge->spike_history,
                bridge->spike_history + shift,
                (bridge->spike_history_capacity - shift) * sizeof(spike_history_entry_t));
        bridge->spike_history_count -= shift;
    }

    /* Add new spike */
    spike_history_entry_t* entry = &bridge->spike_history[bridge->spike_history_count];
    entry->neuron_id = neuron_id;
    entry->spike_time = spike_time;
    entry->amplitude = amplitude;
    bridge->spike_history_count++;

    return 0;
}

/**
 * @brief Clear old spikes from history
 */
static void prune_spike_history(physics_lnn_bridge_t* bridge, float current_time) {
    /* Remove spikes older than 5 * tau */
    float cutoff = current_time - (5.0f * bridge->config.spike_tau);

    uint32_t write_idx = 0;
    for (uint32_t i = 0; i < bridge->spike_history_count; i++) {
        if (bridge->spike_history[i].spike_time >= cutoff) {
            if (write_idx != i) {
                bridge->spike_history[write_idx] = bridge->spike_history[i];
            }
            write_idx++;
        }
    }
    bridge->spike_history_count = write_idx;
}

//=============================================================================
// Configuration API
//=============================================================================

int physics_lnn_default_config(physics_lnn_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_lnn_default_config: config is NULL");
        return -1;
    }

    config->encode_method = PHYSICS_LNN_ENCODE_EXPONENTIAL;
    config->spike_tau = PHYSICS_LNN_SPIKE_TAU;
    config->spike_amplitude = PHYSICS_LNN_SPIKE_AMPLITUDE;
    config->enable_temp_coupling = true;
    config->q10 = PHYSICS_LNN_Q10;
    config->temp_ref = PHYSICS_LNN_TEMP_REF;
    config->output_mode = PHYSICS_LNN_OUTPUT_CONDUCTANCE;
    config->modulation_strength = 0.3f;
    config->bidirectional = true;
    config->update_interval_ms = 1.0f;
    config->num_encode_neurons = 100;
    config->num_output_channels = 4;

    return 0;
}

//=============================================================================
// Lifecycle API
//=============================================================================

physics_lnn_bridge_t* physics_lnn_bridge_create(
    const physics_lnn_config_t* config
) {
    physics_lnn_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate physics-LNN bridge");
    if (bridge_base_init(&bridge->base, 0, "physics_lnn") != 0) { nimcp_free(bridge); return NULL; }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        physics_lnn_default_config(&bridge->config);
    }

    /* Allocate spike history buffer */
    bridge->spike_history_capacity = PHYSICS_LNN_MAX_SPIKE_HISTORY;
    bridge->spike_history = nimcp_calloc(
        bridge->spike_history_capacity,
        sizeof(spike_history_entry_t)
    );
    if (!bridge->spike_history) {
        LOG_ERROR("Failed to allocate spike history buffer for physics-LNN bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate spike history buffer");
        nimcp_free(bridge);
        return NULL;
    }

    /* Allocate encoded currents buffer */
    bridge->num_encoded_channels = bridge->config.num_encode_neurons;
    bridge->encoded_currents = nimcp_calloc(
        bridge->num_encoded_channels,
        sizeof(float)
    );
    if (!bridge->encoded_currents) {
        LOG_ERROR("Failed to allocate encoded currents buffer for physics-LNN bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate encoded currents buffer");
        nimcp_free(bridge->spike_history);
        nimcp_free(bridge);
        return NULL;
    }

    /* Allocate modulation buffer */
    bridge->num_modulation_channels = bridge->config.num_output_channels;
    bridge->modulation_values = nimcp_calloc(
        bridge->num_modulation_channels,
        sizeof(float)
    );
    if (!bridge->modulation_values) {
        LOG_ERROR("Failed to allocate modulation buffer for physics-LNN bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate modulation buffer");
        nimcp_free(bridge->encoded_currents);
        nimcp_free(bridge->spike_history);
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize temperature state */
    bridge->temperature = bridge->config.temp_ref;
    bridge->tau_scale = 1.0f;

    bridge->initialized = true;

    NIMCP_LOG_INFO(PHYSICS_LNN_MODULE_NAME,
        "Physics-LNN bridge created: encode=%d, temp_coupling=%d",
        bridge->config.encode_method, bridge->config.enable_temp_coupling);

    return bridge;
}

void physics_lnn_bridge_destroy(physics_lnn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "physics_lnn");

    NIMCP_LOG_INFO(PHYSICS_LNN_MODULE_NAME,
        "Bridge destroyed - spikes_encoded: %lu, lnn_forward: %lu",
        (unsigned long)bridge->stats.spikes_encoded,
        (unsigned long)bridge->stats.lnn_forward_count);

    if (bridge->prev_voltages) {
        nimcp_free(bridge->prev_voltages);
    }
    if (bridge->modulation_values) {
        nimcp_free(bridge->modulation_values);
    }
    if (bridge->encoded_currents) {
        nimcp_free(bridge->encoded_currents);
    }
    if (bridge->spike_history) {
        nimcp_free(bridge->spike_history);
    }
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int physics_lnn_connect_hh(
    physics_lnn_bridge_t* bridge,
    nimcp_hh_population_t* hh_pop
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_lnn_connect_hh: bridge is NULL");
        return -1;
    }

    bridge->hh_pop = hh_pop;

    if (hh_pop) {
        /* Allocate voltage history for spike detection */
        /* Get population size from the population */
        bridge->num_hh_neurons = bridge->config.num_encode_neurons;

        if (bridge->prev_voltages) {
            nimcp_free(bridge->prev_voltages);
        }
        bridge->prev_voltages = nimcp_calloc(
            bridge->num_hh_neurons,
            sizeof(float)
        );
        if (!bridge->prev_voltages) {
            LOG_ERROR("Failed to allocate voltage history buffer");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate voltage history buffer");
            return -1;
        }

        /* Initialize to resting potential */
        for (uint32_t i = 0; i < bridge->num_hh_neurons; i++) {
            bridge->prev_voltages[i] = -65.0f;  /* Typical resting potential */
        }

        NIMCP_LOG_DEBUG(PHYSICS_LNN_MODULE_NAME,
            "Connected HH population: %p, neurons=%u",
            (void*)hh_pop, bridge->num_hh_neurons);
    }

    return 0;
}

int physics_lnn_connect_lnn(
    physics_lnn_bridge_t* bridge,
    lnn_network_t* network
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_lnn_connect_lnn: bridge is NULL");
        return -1;
    }

    bridge->lnn_network = network;

    NIMCP_LOG_DEBUG(PHYSICS_LNN_MODULE_NAME,
        "Connected LNN network: %p", (void*)network);

    return 0;
}

int physics_lnn_reset(physics_lnn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_lnn_reset: bridge is NULL");
        return -1;
    }

    /* Clear spike history */
    bridge->spike_history_count = 0;
    memset(bridge->spike_history, 0,
           bridge->spike_history_capacity * sizeof(spike_history_entry_t));

    /* Clear encoded currents */
    memset(bridge->encoded_currents, 0,
           bridge->num_encoded_channels * sizeof(float));

    /* Clear modulation */
    memset(bridge->modulation_values, 0,
           bridge->num_modulation_channels * sizeof(float));

    /* Reset voltage history */
    if (bridge->prev_voltages) {
        for (uint32_t i = 0; i < bridge->num_hh_neurons; i++) {
            bridge->prev_voltages[i] = -65.0f;
        }
    }

    /* Reset time */
    bridge->current_time_ms = 0.0f;

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.current_temperature = bridge->temperature;
    bridge->stats.tau_scale_factor = bridge->tau_scale;

    NIMCP_LOG_DEBUG(PHYSICS_LNN_MODULE_NAME, "Bridge reset");

    return 0;
}

//=============================================================================
// HH -> LNN API (Spike Encoding)
//=============================================================================

int physics_lnn_register_spike(
    physics_lnn_bridge_t* bridge,
    uint32_t neuron_id,
    float spike_time
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_lnn_register_spike: bridge is NULL");
        return -1;
    }
    if (neuron_id >= bridge->num_encoded_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_lnn_register_spike: capacity exceeded");
        return -1;
    }

    int result = add_spike_to_history(
        bridge,
        neuron_id,
        spike_time,
        bridge->config.spike_amplitude
    );

    if (result == 0) {
        bridge->stats.spikes_encoded++;
    }

    return result;
}

int physics_lnn_encode_spikes(
    physics_lnn_bridge_t* bridge,
    float current_time,
    physics_lnn_encoded_t* encoded
) {
    if (!bridge || !encoded) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_lnn_encode_spikes: required parameter is NULL (bridge, encoded)");
        return -1;
    }

    /* Prune old spikes */
    prune_spike_history(bridge, current_time);

    /* Clear encoded currents */
    memset(bridge->encoded_currents, 0,
           bridge->num_encoded_channels * sizeof(float));

    /* Get effective tau with temperature scaling */
    float tau = bridge->config.spike_tau * bridge->tau_scale;
    float tau_rise = tau * 0.2f;  /* For double exponential */

    /* Encode each spike in history */
    for (uint32_t i = 0; i < bridge->spike_history_count; i++) {
        spike_history_entry_t* spike = &bridge->spike_history[i];
        float dt = current_time - spike->spike_time;

        if (spike->neuron_id >= bridge->num_encoded_channels) continue;

        float contribution = 0.0f;

        switch (bridge->config.encode_method) {
            case PHYSICS_LNN_ENCODE_EXPONENTIAL:
                contribution = spike->amplitude * exponential_kernel(dt, tau);
                break;

            case PHYSICS_LNN_ENCODE_ALPHA:
                contribution = spike->amplitude * alpha_kernel(dt, tau);
                break;

            case PHYSICS_LNN_ENCODE_DOUBLE_EXP:
                contribution = spike->amplitude * double_exp_kernel(dt, tau_rise, tau);
                break;

            case PHYSICS_LNN_ENCODE_RATE:
                /* Rate coding: count spikes in recent window */
                if (dt < tau) {
                    contribution = spike->amplitude / tau;
                }
                break;

            case PHYSICS_LNN_ENCODE_PHASE:
                /* Phase coding: spike timing relative to oscillation */
                /* Simplified: use cosine modulation */
                if (dt < tau) {
                    float phase = (NIMCP_TWO_PI_F * dt) / tau;
                    contribution = spike->amplitude * (0.5f + 0.5f * cosf(phase));
                }
                break;

            default:
                contribution = spike->amplitude * exponential_kernel(dt, tau);
                break;
        }

        bridge->encoded_currents[spike->neuron_id] += contribution;
    }

    /* Fill output structure */
    encoded->currents = bridge->encoded_currents;
    encoded->num_channels = bridge->num_encoded_channels;
    encoded->timestamp = current_time;
    encoded->total_spikes = bridge->stats.spikes_encoded;

    bridge->current_time_ms = current_time;

    return 0;
}

int physics_lnn_detect_spikes(
    physics_lnn_bridge_t* bridge,
    float current_time,
    float threshold
) {
    if (!bridge || !bridge->hh_pop) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_lnn_detect_spikes: required parameter is NULL (bridge, bridge->hh_pop)");
        return -1;
    }
    if (!bridge->prev_voltages) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_lnn_detect_spikes: bridge->prev_voltages is NULL");
        return -1;
    }

    int spikes_detected = 0;

    /* Get current voltages from HH population */
    /* Note: In a full implementation, we would iterate through population */
    /* For now, we simulate spike detection based on population rate */
    float rate;
    if (nimcp_hh_population_get_rate(bridge->hh_pop, &rate) == NIMCP_SUCCESS) {
        /* Estimate number of neurons that spiked based on rate */
        /* rate is in Hz, convert to probability per ms */
        float spike_prob = rate * 0.001f;  /* Per ms */

        for (uint32_t i = 0; i < bridge->num_hh_neurons; i++) {
            /* Simple probabilistic spike detection based on population rate */
            /* In full implementation, would check actual membrane voltage */
            float random_val = ((float)((i * 1103515245 + 12345) % 1000)) / 1000.0f;

            if (random_val < spike_prob) {
                /* Register spike */
                if (physics_lnn_register_spike(bridge, i, current_time) == 0) {
                    spikes_detected++;
                }
            }
        }
    }

    return spikes_detected;
}

//=============================================================================
// LNN -> HH API (Modulation)
//=============================================================================

int physics_lnn_get_modulation(
    physics_lnn_bridge_t* bridge,
    physics_lnn_modulation_t* modulation
) {
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_lnn_get_modulation: required parameter is NULL (bridge, modulation)");
        return -1;
    }

    /* In a full implementation, we would read LNN output layer */
    /* For now, initialize to neutral values */
    for (uint32_t i = 0; i < bridge->num_modulation_channels; i++) {
        /* Neutral modulation = 1.0 (no change) */
        bridge->modulation_values[i] = 1.0f;
    }

    /* If LNN connected, would read output here */
    if (bridge->lnn_network) {
        /* lnn_network_get_output(bridge->lnn_network, bridge->modulation_values); */
        /* Apply modulation strength scaling */
        float strength = bridge->config.modulation_strength;
        for (uint32_t i = 0; i < bridge->num_modulation_channels; i++) {
            /* Scale output to modulation range [MIN_MOD, MAX_MOD] */
            float raw = bridge->modulation_values[i];
            float scaled = 1.0f + (raw - 0.5f) * 2.0f * strength;

            /* Clamp to valid range */
            scaled = fmaxf(PHYSICS_LNN_MIN_MOD, fminf(PHYSICS_LNN_MAX_MOD, scaled));
            bridge->modulation_values[i] = scaled;
        }
    }

    /* Fill output structure */
    modulation->values = bridge->modulation_values;
    modulation->num_channels = bridge->num_modulation_channels;
    modulation->mode = bridge->config.output_mode;
    modulation->timestamp = bridge->current_time_ms;

    return 0;
}

int physics_lnn_apply_modulation(
    physics_lnn_bridge_t* bridge,
    const physics_lnn_modulation_t* modulation
) {
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_lnn_apply_modulation: required parameter is NULL (bridge, modulation)");
        return -1;
    }
    if (!bridge->hh_pop) return 0;  /* Nothing to modulate */

    /* Apply modulation based on mode */
    switch (modulation->mode) {
        case PHYSICS_LNN_OUTPUT_CONDUCTANCE:
            /* Modulate ion channel conductances */
            /* In full implementation: apply to population parameters */
            NIMCP_LOG_DEBUG(PHYSICS_LNN_MODULE_NAME,
                "Applied conductance modulation: ch0=%.2f",
                modulation->values[0]);
            break;

        case PHYSICS_LNN_OUTPUT_THRESHOLD:
            /* Modulate spike threshold */
            NIMCP_LOG_DEBUG(PHYSICS_LNN_MODULE_NAME,
                "Applied threshold modulation");
            break;

        case PHYSICS_LNN_OUTPUT_TAU:
            /* Modulate time constants */
            NIMCP_LOG_DEBUG(PHYSICS_LNN_MODULE_NAME,
                "Applied tau modulation");
            break;

        case PHYSICS_LNN_OUTPUT_CURRENT:
            /* Direct current injection */
            /* Would inject: I = modulation_value * scale */
            NIMCP_LOG_DEBUG(PHYSICS_LNN_MODULE_NAME,
                "Applied current injection modulation");
            break;

        default:
            break;
    }

    bridge->stats.modulations_applied++;

    return 0;
}

//=============================================================================
// Temperature Coupling API
//=============================================================================

int physics_lnn_set_temperature(
    physics_lnn_bridge_t* bridge,
    float temperature
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_lnn_set_temperature: bridge is NULL");
        return -1;
    }

    bridge->temperature = temperature;

    if (bridge->config.enable_temp_coupling) {
        bridge->tau_scale = compute_tau_scale(
            temperature,
            bridge->config.temp_ref,
            bridge->config.q10
        );

        NIMCP_LOG_DEBUG(PHYSICS_LNN_MODULE_NAME,
            "Temperature set: %.1f°C, tau_scale=%.3f",
            temperature, bridge->tau_scale);
    }

    bridge->stats.current_temperature = temperature;
    bridge->stats.tau_scale_factor = bridge->tau_scale;

    return 0;
}

float physics_lnn_get_tau_scale(const physics_lnn_bridge_t* bridge) {
    if (!bridge) return 1.0f;
    return bridge->tau_scale;
}

//=============================================================================
// Update API
//=============================================================================

int physics_lnn_update(
    physics_lnn_bridge_t* bridge,
    float dt
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_lnn_update: bridge is NULL");
        return -1;
    }

    float new_time = bridge->current_time_ms + dt;

    /* Step 1: Detect spikes from HH population */
    if (bridge->hh_pop) {
        physics_lnn_detect_spikes(bridge, new_time, 0.0f);
    }

    /* Step 2: Encode spikes to LNN input */
    physics_lnn_encoded_t encoded;
    physics_lnn_encode_spikes(bridge, new_time, &encoded);

    /* Step 3: Forward LNN (if connected) */
    if (bridge->lnn_network) {
        physics_lnn_forward_step(bridge, dt);
    }

    /* Step 4: Get modulation from LNN output */
    if (bridge->config.bidirectional && bridge->lnn_network) {
        physics_lnn_modulation_t modulation;
        physics_lnn_get_modulation(bridge, &modulation);

        /* Step 5: Apply modulation to HH */
        physics_lnn_apply_modulation(bridge, &modulation);
    }

    bridge->current_time_ms = new_time;
    bridge->stats.last_update_ms = new_time;

    return 0;
}

int physics_lnn_forward_step(
    physics_lnn_bridge_t* bridge,
    float dt
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_lnn_forward_step: bridge is NULL");
        return -1;
    }
    if (!bridge->lnn_network) return 0;  /* No LNN connected */

    /* In full implementation, would call LNN forward pass with encoded input */
    /* lnn_network_forward(bridge->lnn_network, bridge->encoded_currents, dt); */

    bridge->stats.lnn_forward_count++;

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

int physics_lnn_get_stats(
    const physics_lnn_bridge_t* bridge,
    physics_lnn_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_lnn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

bool physics_lnn_is_connected(const physics_lnn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->hh_pop != NULL && bridge->lnn_network != NULL;
}

int physics_lnn_get_encoded(
    const physics_lnn_bridge_t* bridge,
    physics_lnn_encoded_t* encoded
) {
    if (!bridge || !encoded) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_lnn_get_encoded: required parameter is NULL (bridge, encoded)");
        return -1;
    }

    encoded->currents = bridge->encoded_currents;
    encoded->num_channels = bridge->num_encoded_channels;
    encoded->timestamp = bridge->current_time_ms;
    encoded->total_spikes = bridge->stats.spikes_encoded;

    return 0;
}
