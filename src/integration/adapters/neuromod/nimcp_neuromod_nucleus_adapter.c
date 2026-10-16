/**
 * @file nimcp_neuromod_nucleus_adapter.c
 * @brief Neuromodulatory Nucleus Adapter Implementation
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Implements specific neuromodulatory nucleus dynamics
 * WHY:  Each nucleus has distinct firing patterns and projection targets
 * HOW:  Models tonic/phasic firing, autoreceptors, and output concentration
 *
 * @author NIMCP Development Team
 */

#include "integration/adapters/neuromod/nimcp_neuromod_nucleus_adapter.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuromod_nucleus_adapter)

//=============================================================================
// Internal Structure
//=============================================================================

struct nimcp_nucleus_adapter_struct {
    nimcp_nucleus_adapter_config_t config;
    nimcp_module_interface_t interface;

    /* Firing state */
    float firing_rate;                  /**< Current firing rate (Hz) */
    float output_level;                 /**< Output neuromodulator level */
    float autoreceptor_level;           /**< Autoreceptor feedback */
    float burst_timer;                  /**< Time remaining in burst */
    float refractory_timer;             /**< Time in refractory period */

    /* Spike timing */
    float time_since_spike;             /**< Time since last spike (ms) */
    float isi_sum;                      /**< Sum of ISIs for averaging */
    uint32_t isi_count;                 /**< ISI count */

    /* State tracking */
    nimcp_nucleus_adapter_state_t state;
    nimcp_nucleus_adapter_stats_t stats;
    bool is_initialized;
};

//=============================================================================
// Nucleus-specific parameters
//=============================================================================

static const char* nucleus_names[NUCLEUS_COUNT] = {
    "Locus_Coeruleus_Adapter",
    "VTA_Adapter",
    "Raphe_Adapter",
    "Basal_Forebrain_Adapter"
};

//=============================================================================
// Module Interface Callbacks
//=============================================================================

static nimcp_layer_error_t nucleus_init(void* module, void* config) {
    nimcp_nucleus_adapter_t adapter = (nimcp_nucleus_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;

    if (config) {
        adapter->config = *(nimcp_nucleus_adapter_config_t*)config;
    }

    /* Initialize state */
    adapter->firing_rate = adapter->config.tonic_rate_hz;
    adapter->output_level = adapter->config.tonic_rate_hz / 10.0f;  /* Normalized */
    adapter->autoreceptor_level = 0.0f;
    adapter->burst_timer = 0.0f;
    adapter->refractory_timer = 0.0f;
    adapter->time_since_spike = 0.0f;
    adapter->isi_sum = 0.0f;
    adapter->isi_count = 0;

    adapter->is_initialized = true;
    adapter->state.is_active = true;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t nucleus_shutdown(void* module) {
    nimcp_nucleus_adapter_t adapter = (nimcp_nucleus_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;

    adapter->is_initialized = false;
    adapter->state.is_active = false;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t nucleus_update(void* module, float dt) {
    nimcp_nucleus_adapter_t adapter = (nimcp_nucleus_adapter_t)module;
    if (!adapter || !adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    float dt_ms = dt * 1000.0f;

    /* Update timing */
    adapter->time_since_spike += dt_ms;

    /* Update burst timer */
    if (adapter->burst_timer > 0.0f) {
        adapter->burst_timer -= dt_ms;
        adapter->state.in_burst = true;
        adapter->firing_rate = adapter->config.phasic_burst_rate_hz;
    } else {
        adapter->state.in_burst = false;
        /* Return to tonic rate, modulated by autoreceptors */
        float tonic = adapter->config.tonic_rate_hz;
        if (adapter->config.enable_autoreceptors) {
            tonic *= (1.0f - adapter->autoreceptor_level * 0.5f);
        }
        adapter->firing_rate = tonic;
    }

    /* Update refractory timer */
    if (adapter->refractory_timer > 0.0f) {
        adapter->refractory_timer -= dt_ms;
    }

    /* Generate spikes based on firing rate */
    float expected_isi = 1000.0f / adapter->firing_rate;  /* ms */
    if (adapter->time_since_spike >= expected_isi && adapter->refractory_timer <= 0.0f) {
        /* Spike! */
        adapter->stats.spikes_generated++;

        /* Track ISI */
        adapter->isi_sum += adapter->time_since_spike;
        adapter->isi_count++;
        adapter->time_since_spike = 0.0f;

        /* Enter refractory period */
        adapter->refractory_timer = adapter->config.refractory_ms;

        /* Increase output level */
        float release_amount = 0.1f;
        if (adapter->state.in_burst) {
            release_amount = 0.3f;  /* Larger release during bursts */
        }
        adapter->output_level += release_amount;
        adapter->stats.total_output += release_amount;
    }

    /* Autoreceptor feedback */
    if (adapter->config.enable_autoreceptors) {
        float target_auto = adapter->output_level * 0.5f;
        float auto_tau = 100.0f;  /* 100 ms time constant */
        adapter->autoreceptor_level += (target_auto - adapter->autoreceptor_level) / auto_tau * dt_ms;
    }

    /* Output decay */
    float output_tau = 200.0f;  /* Output decay time constant */
    adapter->output_level *= expf(-dt_ms / output_tau);

    /* Clamp output */
    if (adapter->output_level > 1.0f) adapter->output_level = 1.0f;
    if (adapter->output_level < 0.0f) adapter->output_level = 0.0f;

    /* Update state */
    adapter->state.current_firing_rate_hz = adapter->firing_rate;
    adapter->state.output_concentration = adapter->output_level;
    adapter->state.autoreceptor_inhibition = adapter->autoreceptor_level;

    if (adapter->isi_count > 0) {
        adapter->state.mean_isi_ms = adapter->isi_sum / (float)adapter->isi_count;
    }

    adapter->stats.updates_processed++;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t nucleus_handle_message(void* module, const nimcp_layer_msg_t* msg) {
    nimcp_nucleus_adapter_t adapter = (nimcp_nucleus_adapter_t)module;
    if (!adapter || !msg) return NIMCP_LAYER_ERR_NULL_PTR;

    adapter->stats.messages_handled++;

    switch (msg->header.msg_type) {
        case NIMCP_LAYER_MSG_EXCITE:
            /* Excitation triggers burst (context-dependent) */
            if (msg->payload && msg->header.payload_size >= sizeof(float)) {
                float intensity = *(float*)msg->payload;
                adapter->burst_timer = 50.0f * intensity;  /* 50ms burst */
                adapter->state.burst_count++;
                adapter->stats.bursts_generated++;
            }
            break;

        case NIMCP_LAYER_MSG_INHIBIT:
            /* Inhibition suppresses firing */
            adapter->firing_rate *= 0.5f;
            break;

        case NIMCP_LAYER_MSG_MODULATE:
            /* Modulation adjusts tonic rate */
            if (msg->payload && msg->header.payload_size >= sizeof(float)) {
                float mod = *(float*)msg->payload;
                adapter->config.tonic_rate_hz *= (1.0f + mod * 0.2f);
                if (adapter->config.tonic_rate_hz < 0.1f) {
                    adapter->config.tonic_rate_hz = 0.1f;
                }
                if (adapter->config.tonic_rate_hz > 50.0f) {
                    adapter->config.tonic_rate_hz = 50.0f;
                }
            }
            break;

        case NIMCP_LAYER_MSG_DATA_PUSH:
            /* Input signal - can trigger burst based on salience */
            if (msg->payload && msg->header.payload_size >= sizeof(float)) {
                float signal = *(float*)msg->payload;
                if (signal > 0.5f) {
                    adapter->burst_timer = 30.0f;
                    adapter->state.burst_count++;
                    adapter->stats.bursts_generated++;
                }
            }
            break;

        default:
            break;
    }

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t nucleus_get_state(void* module, void* state_out, size_t* size) {
    nimcp_nucleus_adapter_t adapter = (nimcp_nucleus_adapter_t)module;
    if (!adapter || !state_out || !size) return NIMCP_LAYER_ERR_NULL_PTR;

    if (*size < sizeof(nimcp_nucleus_adapter_state_t)) {
        *size = sizeof(nimcp_nucleus_adapter_state_t);
        return NIMCP_LAYER_ERR_CAPACITY;
    }

    *(nimcp_nucleus_adapter_state_t*)state_out = adapter->state;
    *size = sizeof(nimcp_nucleus_adapter_state_t);

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t nucleus_set_state(void* module, const void* state, size_t size) {
    nimcp_nucleus_adapter_t adapter = (nimcp_nucleus_adapter_t)module;
    if (!adapter || !state) return NIMCP_LAYER_ERR_NULL_PTR;

    if (size < sizeof(nimcp_nucleus_adapter_state_t)) {
        return NIMCP_LAYER_ERR_INVALID_MSG;
    }

    adapter->state = *(const nimcp_nucleus_adapter_state_t*)state;

    return NIMCP_LAYER_OK;
}

static const char* nucleus_get_name(void* module) {
    nimcp_nucleus_adapter_t adapter = (nimcp_nucleus_adapter_t)module;
    if (!adapter || adapter->config.nucleus_type >= NUCLEUS_COUNT) {
        return "Unknown_Nucleus_Adapter";
    }
    return nucleus_names[adapter->config.nucleus_type];
}

//=============================================================================
// Public API Implementation
//=============================================================================

nimcp_nucleus_adapter_config_t nimcp_nucleus_adapter_default_config(nucleus_type_t nucleus_type) {
    nimcp_nucleus_adapter_config_t config;

    config.nucleus_type = nucleus_type;
    config.enable_autoreceptors = true;
    config.enable_logging = false;

    switch (nucleus_type) {
        case NUCLEUS_LC:
            /* Locus Coeruleus - Norepinephrine */
            config.num_neurons = 1500;       /* ~1500 neurons per side in humans */
            config.tonic_rate_hz = 1.0f;     /* Low tonic firing */
            config.phasic_burst_rate_hz = 15.0f;  /* Phasic bursts up to 15 Hz */
            config.refractory_ms = 20.0f;
            config.axon_conduction_ms = 50.0f;  /* Widely projecting */
            break;

        case NUCLEUS_VTA:
            /* Ventral Tegmental Area - Dopamine */
            config.num_neurons = 400000;     /* ~400K DA neurons in VTA/SNc */
            config.tonic_rate_hz = 4.0f;     /* Higher tonic than LC */
            config.phasic_burst_rate_hz = 20.0f;  /* Reward bursts */
            config.refractory_ms = 15.0f;
            config.axon_conduction_ms = 30.0f;
            break;

        case NUCLEUS_RAPHE:
            /* Raphe Nuclei - Serotonin */
            config.num_neurons = 28000;      /* ~28K in dorsal raphe */
            config.tonic_rate_hz = 0.5f;     /* Very slow, regular firing */
            config.phasic_burst_rate_hz = 5.0f;   /* Modest phasic response */
            config.refractory_ms = 50.0f;    /* Long refractory */
            config.axon_conduction_ms = 100.0f;  /* Very slow release */
            break;

        case NUCLEUS_BASAL_FOREBRAIN:
            /* Basal Forebrain - Acetylcholine */
            config.num_neurons = 200000;     /* ~200K cholinergic neurons */
            config.tonic_rate_hz = 5.0f;     /* Moderate tonic */
            config.phasic_burst_rate_hz = 30.0f;  /* Fast attention signals */
            config.refractory_ms = 10.0f;
            config.axon_conduction_ms = 20.0f;
            break;

        default:
            /* Default to LC-like */
            config.nucleus_type = NUCLEUS_LC;
            config.num_neurons = 1500;
            config.tonic_rate_hz = 1.0f;
            config.phasic_burst_rate_hz = 15.0f;
            config.refractory_ms = 20.0f;
            config.axon_conduction_ms = 50.0f;
            break;
    }

    return config;
}

nimcp_nucleus_adapter_t nimcp_nucleus_adapter_create(
    const nimcp_nucleus_adapter_config_t* config
) {
    nimcp_nucleus_adapter_t adapter = (nimcp_nucleus_adapter_t)nimcp_calloc(
        1, sizeof(struct nimcp_nucleus_adapter_struct));
    NIMCP_API_CHECK_ALLOC(adapter, "Failed to allocate neuromod nucleus adapter");

    adapter->config = config ? *config : nimcp_nucleus_adapter_default_config(NUCLEUS_LC);

    /* Set up module interface */
    adapter->interface.init = nucleus_init;
    adapter->interface.shutdown = nucleus_shutdown;
    adapter->interface.update = nucleus_update;
    adapter->interface.handle_message = nucleus_handle_message;
    adapter->interface.get_state = nucleus_get_state;
    adapter->interface.set_state = nucleus_set_state;
    adapter->interface.get_name = nucleus_get_name;

    return adapter;
}

void nimcp_nucleus_adapter_destroy(nimcp_nucleus_adapter_t adapter) {
    if (!adapter) return;

    if (adapter->is_initialized) {
        nucleus_shutdown(adapter);
    }

    nimcp_free(adapter);
}

nimcp_module_interface_t* nimcp_nucleus_adapter_get_interface(
    nimcp_nucleus_adapter_t adapter
) {
    return adapter ? &adapter->interface : NULL;
}

nimcp_layer_error_t nimcp_nucleus_adapter_trigger_burst(
    nimcp_nucleus_adapter_t adapter,
    float intensity
) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    adapter->burst_timer = 50.0f * intensity;
    adapter->state.burst_count++;
    adapter->stats.bursts_generated++;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_nucleus_adapter_set_tonic_rate(
    nimcp_nucleus_adapter_t adapter,
    float rate_hz
) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;

    adapter->config.tonic_rate_hz = rate_hz;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_nucleus_adapter_get_output(
    nimcp_nucleus_adapter_t adapter,
    float* concentration_out
) {
    if (!adapter || !concentration_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *concentration_out = adapter->output_level;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_nucleus_adapter_get_state(
    nimcp_nucleus_adapter_t adapter,
    nimcp_nucleus_adapter_state_t* state_out
) {
    if (!adapter || !state_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *state_out = adapter->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_nucleus_adapter_get_stats(
    nimcp_nucleus_adapter_t adapter,
    nimcp_nucleus_adapter_stats_t* stats_out
) {
    if (!adapter || !stats_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *stats_out = adapter->stats;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_nucleus_adapter_reset_stats(nimcp_nucleus_adapter_t adapter) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    memset(&adapter->stats, 0, sizeof(adapter->stats));
    return NIMCP_LAYER_OK;
}
