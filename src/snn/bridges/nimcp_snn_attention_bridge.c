/*
 * DEPRECATED — STATUE (audit 2026-04-30)
 *
 * snn_attention_bridge_create has zero callers in production code.
 * cycle_coordinator declares an attention_bridge field but no init site
 * wires it. The SNN lightweight CSR migration (CLAUDE.md cb-migration)
 * likely supersedes this bridge. Do NOT delete — the SNN wave campaign
 * may bring it back. Either wire a consumer or delete before the next
 * major version. Do not extend.
 */

/**
 * @file nimcp_snn_attention_bridge.c
 * @brief SNN-Attention integration bridge implementation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_attention_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_attention_bridge)

//=============================================================================
// Bio-Async Module ID
//=============================================================================

#define BIO_MODULE_SNN_ATTENTION_BRIDGE 0x0610

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * WHAT: Initialize config with biologically-plausible defaults
 * WHY:  Convenient starting point
 * HOW:  Literature-based parameter values
 */
void snn_attention_config_default(snn_attention_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_attention_config_default: null config pointer");
        return;
    }

    /* Spike-to-attention conversion */
    config->spike_rate_min = 10.0f;          /* 10 Hz baseline */
    config->spike_rate_max = 80.0f;          /* 80 Hz saturated attention */
    config->gate_scaling_factor = 1.0f;

    /* Gamma oscillation parameters */
    config->enable_gamma_sync = true;
    config->gamma_frequency = 40.0f;         /* 40 Hz gamma center */
    config->gamma_bandwidth = 10.0f;         /* 30-50 Hz range */
    config->gamma_phase_threshold = 0.5f;    /* 50% coherence */

    /* Attention-to-spike modulation */
    config->attention_boost_factor = 1.5f;   /* 50% boost */
    config->salience_spike_scaling = 2.0f;
    config->modulate_synaptic_weights = true;
    config->weight_modulation_gain = 0.3f;   /* 30% modulation */

    /* Population mapping */
    config->attention_population_id = 0;     /* Set by user */
    config->input_population_id = 0;         /* Set by user */

    /* Update timing */
    config->update_interval_ms = 25.0f;      /* 40 Hz update rate */

    /* Bio-async */
    config->enable_bio_async = false;
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * WHAT: Create SNN-attention bridge
 * WHY:  Initialize bidirectional integration
 * HOW:  Allocate, validate, connect components
 */
snn_attention_bridge_t* snn_attention_bridge_create(
    const snn_attention_config_t* config,
    snn_network_t* snn,
    multihead_attention_t* attention
) {
    /* Guard: Validate inputs */
    if (!config || !snn || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null parameters to snn_attention_bridge_create: config=%p, snn=%p, attn=%p",
                             (void*)config, (void*)snn, (void*)attention);
        return NULL;
    }

    /* Allocate bridge */
    snn_attention_bridge_t* bridge = nimcp_malloc(sizeof(snn_attention_bridge_t));
    if (!bridge) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(snn_attention_bridge_t),
                          "Failed to allocate SNN-attention bridge");
        return NULL;
    }

    /* Initialize structure */
    memset(bridge, 0, sizeof(snn_attention_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_attention") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->attention = attention;
    bridge->config = *config;

    /* Get populations */
    if (config->attention_population_id > 0) {
        bridge->attention_pop = snn_network_get_population(snn, config->attention_population_id);
        if (!bridge->attention_pop) {
            NIMCP_LOGGING_WARN("Attention population ID %u not found", config->attention_population_id);
        }
    }

    if (config->input_population_id > 0) {
        bridge->input_pop = snn_network_get_population(snn, config->input_population_id);
        if (!bridge->input_pop) {
            NIMCP_LOGGING_WARN("Input population ID %u not found", config->input_population_id);
        }
    }

    /* Initialize state */
    bridge->state.gamma.frequency = config->gamma_frequency;
    bridge->last_update_time = 0.0f;

    NIMCP_LOGGING_INFO("Created SNN-attention bridge");
    return bridge;
}

/**
 * WHAT: Destroy bridge and free resources
 * WHY:  Proper cleanup
 * HOW:  Disconnect, free memory
 */
void snn_attention_bridge_destroy(snn_attention_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_attention_bridge_destroy: null bridge pointer");
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        snn_attention_bridge_disconnect_bio_async(bridge);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-attention bridge");
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * WHAT: Connect to bio-async messaging
 * WHY:  Enable distributed coordination
 * HOW:  Register with router
 */
int snn_attention_bridge_connect_bio_async(snn_attention_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_attention_bridge_connect_bio_async: null bridge pointer");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_ATTENTION_BRIDGE,
        .module_name = "snn_attention_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_attention_bridge_connect_bio_async: validation failed");
    return -1;
}

/**
 * WHAT: Disconnect from bio-async
 * WHY:  Clean shutdown
 * HOW:  Unregister from router
 */
int snn_attention_bridge_disconnect_bio_async(snn_attention_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_attention_bridge_disconnect_bio_async: null bridge pointer");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

/**
 * WHAT: Check bio-async connection status
 * WHY:  Query before sending messages
 * HOW:  Return flag
 */
bool snn_attention_bridge_is_bio_async_connected(const snn_attention_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

//=============================================================================
// Processing Functions
//=============================================================================

/**
 * WHAT: Process spike input through attention
 * WHY:  Main processing pipeline
 * HOW:  Compute rate, detect gamma, modulate
 */
int snn_attention_bridge_process(
    snn_attention_bridge_t* bridge,
    const float* input,
    float* output
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_attention_bridge_process: null bridge pointer");
        return -1;
    }

    /* Update bridge state */
    int ret = snn_attention_bridge_update(bridge, bridge->config.update_interval_ms);
    if (ret != 0) {
        return ret;
    }

    /* If output buffer provided, fill with attention signal */
    if (output) {
        output[0] = bridge->state.attention_gate_signal;
        if (bridge->config.enable_gamma_sync) {
            output[1] = bridge->state.gamma.coherence;
        }
    }

    return 0;
}

/**
 * WHAT: Update bridge state
 * WHY:  Synchronize SNN and attention
 * HOW:  Compute spike rate, gamma, apply modulation
 */
int snn_attention_bridge_update(snn_attention_bridge_t* bridge, float dt) {
    /* Guard: Validate bridge */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_attention_bridge_update: null bridge pointer");
        return -1;
    }

    /* Check if update needed based on interval */
    if (bridge->last_update_time > 0 &&
        (dt < bridge->config.update_interval_ms)) {
        return 0;  /* Skip update, too soon */
    }

    /* Compute spike rate from attention population */
    if (bridge->attention_pop) {
        float window_ms = bridge->config.update_interval_ms;
        bridge->state.current_spike_rate = snn_network_get_population_rate(
            bridge->snn,
            bridge->config.attention_population_id,
            window_ms
        );

        /* Compute gate signal from spike rate */
        bridge->state.attention_gate_signal = snn_attention_compute_gate_signal(
            bridge,
            bridge->state.current_spike_rate
        );

        /* Apply gate to attention system */
        if (bridge->attention && *bridge->attention) {
            multihead_attention_set_gate(*bridge->attention, bridge->state.attention_gate_signal);
        }
    }

    /* Detect gamma oscillations */
    if (bridge->config.enable_gamma_sync && bridge->attention_pop) {
        snn_attention_detect_gamma(bridge, bridge->attention_pop, &bridge->state.gamma);

        /* Apply gamma synchronization */
        if (bridge->state.gamma.is_synchronized) {
            snn_attention_apply_gamma_sync(bridge);
        }
    }

    /* Get attention strength and boost input population */
    bridge->state.attention_strength = (bridge->attention && *bridge->attention) ?
        multihead_attention_get_strength(*bridge->attention) : 0.0f;
    if (bridge->input_pop && bridge->state.attention_strength > 0.0f) {
        snn_attention_boost_input_population(bridge);
    }

    /* Update statistics */
    bridge->state.sync_count++;
    bridge->state.avg_spike_rate =
        (bridge->state.avg_spike_rate * (bridge->state.sync_count - 1) +
         bridge->state.current_spike_rate) / bridge->state.sync_count;
    bridge->state.avg_gate_signal =
        (bridge->state.avg_gate_signal * (bridge->state.sync_count - 1) +
         bridge->state.attention_gate_signal) / bridge->state.sync_count;

    bridge->last_update_time += dt;
    return 0;
}

//=============================================================================
// Spike-to-Attention Functions
//=============================================================================

/**
 * WHAT: Compute gate signal from spike rate
 * WHY:  Map spike rate to attention strength
 * HOW:  Linear scaling with saturation
 */
float snn_attention_compute_gate_signal(
    const snn_attention_bridge_t* bridge,
    float spike_rate
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_attention_compute_gate_signal: null bridge pointer");
        return 0.0f;
    }

    float min_rate = bridge->config.spike_rate_min;
    float max_rate = bridge->config.spike_rate_max;

    /* Linear scaling between min and max */
    float normalized = (spike_rate - min_rate) / (max_rate - min_rate);

    /* Clamp to [0, 1] */
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    return normalized * bridge->config.gate_scaling_factor;
}

/**
 * WHAT: Detect gamma oscillation
 * WHY:  Identify synchronized attention state
 * HOW:  Analyze spike time autocorrelation
 */
int snn_attention_detect_gamma(
    snn_attention_bridge_t* bridge,
    snn_population_t* population,
    snn_gamma_state_t* gamma_state
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_attention_detect_gamma: null bridge pointer");
        return -1;
    }
    if (!population) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_attention_detect_gamma: null population pointer");
        return -1;
    }
    if (!gamma_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_attention_detect_gamma: null gamma_state pointer");
        return -1;
    }

    /* Simplified gamma detection: check population synchrony */
    float synchrony = population->population_synchrony;
    gamma_state->coherence = synchrony;
    gamma_state->is_synchronized = (synchrony >= bridge->config.gamma_phase_threshold);

    /* Update phase (simplified) */
    float freq = bridge->config.gamma_frequency;
    float dt = bridge->config.update_interval_ms / 1000.0f;
    gamma_state->phase = fmodf(gamma_state->phase + 2.0f * M_PI * freq * dt, 2.0f * M_PI);

    /* Amplitude from coherence */
    gamma_state->amplitude = synchrony;
    gamma_state->frequency = freq;

    if (gamma_state->is_synchronized) {
        gamma_state->burst_count++;
    }

    return 0;
}

/**
 * WHAT: Apply gamma synchronization to attention
 * WHY:  Gamma modulates attention temporally
 * HOW:  Boost gate at gamma peaks
 */
int snn_attention_apply_gamma_sync(snn_attention_bridge_t* bridge) {
    /* Guard: Validate bridge */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_attention_apply_gamma_sync: null bridge pointer");
        return -1;
    }

    /* Modulate gate by gamma phase */
    float phase = bridge->state.gamma.phase;
    float gamma_boost = 1.0f + 0.3f * cosf(phase);  /* ±30% modulation */

    /* Apply boost */
    float modulated_gate = bridge->state.attention_gate_signal * gamma_boost;
    if (modulated_gate > 1.0f) modulated_gate = 1.0f;

    if (bridge->attention && *bridge->attention) {
        multihead_attention_set_gate(*bridge->attention, modulated_gate);
    }
    return 0;
}

//=============================================================================
// Attention-to-Spike Functions
//=============================================================================

/**
 * WHAT: Boost input population based on attention
 * WHY:  Attention enhances sensory responses
 * HOW:  Scale input current by attention strength
 */
int snn_attention_boost_input_population(snn_attention_bridge_t* bridge) {
    /* Guard: Validate bridge and population */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_attention_boost_input_population: null bridge pointer");
        return -1;
    }
    if (!bridge->input_pop) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_attention_boost_input_population: null input_pop pointer");
        return -1;
    }

    /* Compute boost current */
    float boost = bridge->config.attention_boost_factor * bridge->state.attention_strength;
    bridge->state.input_boost_current = boost;

    /* Note: Actual application would modify population input currents */
    /* This would require access to neuron_t structures in the population */
    /* Implementation depends on SNN API for setting input currents */

    return 0;
}

/**
 * WHAT: Modulate synaptic weights by attention
 * WHY:  Attention gates plasticity
 * HOW:  Scale synaptic efficacy
 */
int snn_attention_modulate_weights(
    snn_attention_bridge_t* bridge,
    const float* attention_weights,
    uint32_t seq_length
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_attention_modulate_weights: null bridge pointer");
        return -1;
    }
    if (!attention_weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_attention_modulate_weights: null attention_weights pointer");
        return -1;
    }

    if (!bridge->config.modulate_synaptic_weights) {
        return 0;  /* Modulation disabled */
    }

    /* Note: Actual implementation would scale synaptic weights */
    /* This requires access to synapse structures and weight modification API */
    /* Placeholder for demonstration */

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

/**
 * WHAT: Get current bridge state
 * WHY:  External monitoring
 * HOW:  Copy state structure
 */
int snn_attention_bridge_get_state(
    const snn_attention_bridge_t* bridge,
    snn_attention_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_attention_bridge_get_state: null bridge pointer");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_attention_bridge_get_state: null state pointer");
        return -1;
    }
    *state = bridge->state;
    return 0;
}

/**
 * WHAT: Get gamma state
 * WHY:  Monitor oscillations
 * HOW:  Copy gamma structure
 */
int snn_attention_get_gamma_state(
    const snn_attention_bridge_t* bridge,
    snn_gamma_state_t* gamma
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_attention_get_gamma_state: null bridge pointer");
        return -1;
    }
    if (!gamma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_attention_get_gamma_state: null gamma pointer");
        return -1;
    }
    *gamma = bridge->state.gamma;
    return 0;
}

/**
 * WHAT: Get gate signal
 * WHY:  Quick query
 * HOW:  Return cached value
 */
float snn_attention_get_gate_signal(const snn_attention_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_attention_get_gate_signal: null bridge pointer");
        return 0.0f;
    }
    return bridge->state.attention_gate_signal;
}

/**
 * WHAT: Check gamma synchronization
 * WHY:  Binary query
 * HOW:  Return flag
 */
bool snn_attention_is_gamma_synchronized(const snn_attention_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->state.gamma.is_synchronized;
}

//=============================================================================
// Statistics
//=============================================================================

/**
 * WHAT: Get bridge statistics
 * WHY:  Monitor performance
 * HOW:  Return computed metrics
 */
int snn_attention_get_stats(
    const snn_attention_bridge_t* bridge,
    uint32_t* sync_count,
    float* avg_gate,
    float* avg_spike_rate
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_attention_get_stats: null bridge pointer");
        return -1;
    }

    if (sync_count) *sync_count = bridge->state.sync_count;
    if (avg_gate) *avg_gate = bridge->state.avg_gate_signal;
    if (avg_spike_rate) *avg_spike_rate = bridge->state.avg_spike_rate;

    return 0;
}

/**
 * WHAT: Reset statistics
 * WHY:  Start fresh measurement
 * HOW:  Zero counters
 */
void snn_attention_reset_stats(snn_attention_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_attention_reset_stats: null bridge pointer");
        return;
    }

    bridge->state.sync_count = 0;
    bridge->state.avg_gate_signal = 0.0f;
    bridge->state.avg_spike_rate = 0.0f;
}
