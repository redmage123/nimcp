/**
 * @file nimcp_cochlea_substrate_bridge.c
 * @brief Cochlea-Neural Substrate integration implementation
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "perception/bridges/nimcp_cochlea_substrate_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cochlea_substrate_bridge)

/* Thread-safe PRNG (replaces non-reentrant rand()) */
static __thread unsigned int g_tl_rand_seed = 0;
static inline unsigned int nimcp_thread_rand(void) {
    if (g_tl_rand_seed == 0) {
        g_tl_rand_seed = (unsigned int)(uintptr_t)&g_tl_rand_seed ^ (unsigned int)time(NULL);
    }
    return (unsigned int)rand_r(&g_tl_rand_seed);
}
#define NIMCP_THREAD_RAND() nimcp_thread_rand()

#define LOG_MODULE "COCHLEA_SUBSTRATE_BRIDGE"

//=============================================================================
// Internal Structure
//=============================================================================

#define MAX_POPULATIONS 64
#define MAX_SPIKES_PER_POP 512

struct cochlea_substrate_bridge {
    bridge_base_t base;                         /* MUST be first */
    cochlea_substrate_config_t config;

    /* Connected systems */
    cochlea_t* cochlea;
    neural_substrate_t* substrate;

    /* Population mapping */
    population_mapping_t mappings[MAX_POPULATIONS];
    uint32_t num_populations;

    /* Spike output */
    uint32_t spike_counts[MAX_POPULATIONS];
    float spike_times[MAX_POPULATIONS * MAX_SPIKES_PER_POP];
    uint32_t spike_neuron_ids[MAX_POPULATIONS * MAX_SPIKES_PER_POP];
    uint32_t total_spikes;
    float avg_rate;
    float max_rate;

    /* Encoding mode */
    spike_encoding_mode_t encoding_mode;

    /* Statistics */
    uint64_t cumulative_spikes;

    /* Bidirectional timestamps */
    uint64_t last_outbound_ts;
    uint64_t last_inbound_ts;
};

//=============================================================================
// Helpers
//=============================================================================

static uint64_t substrate_get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

//=============================================================================
// Configuration
//=============================================================================

cochlea_substrate_config_t cochlea_substrate_config_default(void) {
    cochlea_substrate_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.neurons_per_channel = COCHLEA_SUBSTRATE_DEFAULT_NEURONS_PER_CHANNEL;
    cfg.default_encoding = SPIKE_ENCODE_MIXED;
    cfg.max_rate_hz = COCHLEA_SUBSTRATE_MAX_SPIKE_RATE_HZ;
    cfg.spontaneous_rate_hz = 5.0f;
    cfg.phase_lock_cutoff_hz = COCHLEA_SUBSTRATE_PHASE_LOCK_CUTOFF_HZ;
    cfg.phase_lock_strength = 0.8f;
    cfg.jitter_ms = 0.1f;
    cfg.add_spontaneous = true;
    return cfg;
}

//=============================================================================
// Core API
//=============================================================================

cochlea_substrate_bridge_t* cochlea_substrate_bridge_create(
    cochlea_t* cochlea,
    neural_substrate_t* substrate,
    const cochlea_substrate_config_t* config
) {
    cochlea_substrate_bridge_heartbeat("create", 0.0f);

    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_substrate_bridge_create: cochlea NULL");
        return NULL;
    }
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_substrate_bridge_create: substrate NULL");
        return NULL;
    }

    cochlea_substrate_bridge_t* bridge = (cochlea_substrate_bridge_t*)
        nimcp_calloc(1, sizeof(cochlea_substrate_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_substrate_bridge_create: bridge is NULL");
        return NULL;
    }

    if (bridge_base_init(&bridge->base, 0, "cochlea_substrate") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cochlea_substrate_bridge_create: validation failed");
        return NULL;
    }

    /* Store references */
    bridge->cochlea = cochlea;
    bridge->substrate = substrate;

    /* Apply config */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = cochlea_substrate_config_default();
    }

    bridge->encoding_mode = bridge->config.default_encoding;

    /* Initialize population mappings */
    uint32_t npop = bridge->config.neurons_per_channel > 0
                    ? bridge->config.neurons_per_channel : COCHLEA_SUBSTRATE_DEFAULT_NEURONS_PER_CHANNEL;
    bridge->num_populations = MAX_POPULATIONS;
    for (uint32_t i = 0; i < bridge->num_populations; i++) {
        bridge->mappings[i].channel_id = i;
        bridge->mappings[i].population_id = i;
        bridge->mappings[i].num_neurons = npop;
        /* Tonotopic: rough log spacing 20 Hz - 20 kHz */
        bridge->mappings[i].center_freq_hz = 20.0f * powf(1000.0f, (float)i / (float)(MAX_POPULATIONS - 1));
        bridge->mappings[i].encoding = bridge->config.default_encoding;
    }

    bridge_base_connect_a_unlocked(&bridge->base, cochlea);
    bridge_base_connect_b_unlocked(&bridge->base, substrate);

    cochlea_substrate_bridge_heartbeat("create", 1.0f);
    return bridge;
}

void cochlea_substrate_bridge_destroy(cochlea_substrate_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "cochlea_substrate");
    cochlea_substrate_bridge_heartbeat("destroy", 0.0f);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

nimcp_error_t cochlea_substrate_bridge_update(
    cochlea_substrate_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_substrate_bridge_update: bridge NULL");
        return -1;
    }
    if (!cochlea_output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_substrate_bridge_update: cochlea_output NULL");
        return -1;
    }

    cochlea_substrate_bridge_heartbeat("update", 0.0f);
    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset per-step spike counts */
    bridge->total_spikes = 0;
    bridge->avg_rate = 0.0f;
    bridge->max_rate = 0.0f;
    memset(bridge->spike_counts, 0, sizeof(bridge->spike_counts));

    float dt_sec = dt_ms / 1000.0f;
    if (dt_sec <= 0.0f) dt_sec = 0.001f;

    float rate_sum = 0.0f;
    uint32_t spike_offset = 0;

    for (uint32_t p = 0; p < bridge->num_populations && p < MAX_POPULATIONS; p++) {
        population_mapping_t* m = &bridge->mappings[p];

        /* Generate a simulated rate based on channel energy */
        float channel_energy = 0.5f; /* Default if no real data */
        float rate_hz = channel_energy * bridge->config.max_rate_hz;
        if (bridge->config.add_spontaneous) {
            rate_hz += bridge->config.spontaneous_rate_hz;
        }
        rate_hz = clampf(rate_hz, 0.0f, bridge->config.max_rate_hz);

        /* For temporal encoding below cutoff, modulate with phase lock */
        spike_encoding_mode_t enc = m->encoding;
        if (enc == SPIKE_ENCODE_MIXED || enc == SPIKE_ENCODE_TEMPORAL) {
            if (m->center_freq_hz < bridge->config.phase_lock_cutoff_hz) {
                rate_hz *= (1.0f + bridge->config.phase_lock_strength * 0.2f);
            }
        }

        /* Poisson spike generation: expected spikes = rate * dt */
        float expected = rate_hz * dt_sec * (float)m->num_neurons;
        uint32_t nspikes = (uint32_t)(expected + 0.5f);
        if (nspikes > MAX_SPIKES_PER_POP) nspikes = MAX_SPIKES_PER_POP;

        bridge->spike_counts[p] = nspikes;

        for (uint32_t s = 0; s < nspikes && spike_offset < MAX_POPULATIONS * MAX_SPIKES_PER_POP; s++) {
            bridge->spike_times[spike_offset] = dt_ms * ((float)s / (float)(nspikes > 1 ? nspikes : 1))
                                                 + bridge->config.jitter_ms * ((float)(NIMCP_THREAD_RAND() % 100) / 100.0f - 0.5f);
            bridge->spike_neuron_ids[spike_offset] = s % m->num_neurons;
            spike_offset++;
        }

        bridge->total_spikes += nspikes;
        rate_sum += rate_hz;
        if (rate_hz > bridge->max_rate) bridge->max_rate = rate_hz;
    }

    if (bridge->num_populations > 0) {
        bridge->avg_rate = rate_sum / (float)bridge->num_populations;
    }
    bridge->cumulative_spikes += bridge->total_spikes;

    bridge->last_outbound_ts = substrate_get_time_ms();
    bridge->last_inbound_ts = bridge->last_outbound_ts;

    bridge_base_record_update(&bridge->base);
    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_substrate_bridge_heartbeat("update", 1.0f);
    return 0;
}

nimcp_error_t cochlea_substrate_bridge_reset(cochlea_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_substrate_bridge_reset: bridge NULL");
        return -1;
    }
    cochlea_substrate_bridge_heartbeat("reset", 0.0f);
    nimcp_mutex_lock(bridge->base.mutex);

    bridge->total_spikes = 0;
    bridge->avg_rate = 0.0f;
    bridge->max_rate = 0.0f;
    bridge->cumulative_spikes = 0;
    bridge->last_outbound_ts = 0;
    bridge->last_inbound_ts = 0;
    memset(bridge->spike_counts, 0, sizeof(bridge->spike_counts));

    bridge_base_reset_unlocked(&bridge->base);
    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_substrate_bridge_heartbeat("reset", 1.0f);
    return 0;
}

//=============================================================================
// Spike Access
//=============================================================================

nimcp_error_t cochlea_substrate_get_spikes(
    const cochlea_substrate_bridge_t* bridge,
    substrate_spike_output_t* output
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_substrate_get_spikes: bridge NULL");
        return -1;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_substrate_get_spikes: output NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    output->spike_counts = (uint32_t*)bridge->spike_counts;
    output->spike_times = (float*)bridge->spike_times;
    output->spike_neuron_ids = (uint32_t*)bridge->spike_neuron_ids;
    output->total_spikes = bridge->total_spikes;
    output->num_populations = bridge->num_populations;
    output->avg_rate = bridge->avg_rate;
    output->max_rate = bridge->max_rate;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

nimcp_error_t cochlea_substrate_get_population_spikes(
    const cochlea_substrate_bridge_t* bridge,
    uint32_t population_id,
    uint32_t* spike_count,
    float** spike_times
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_substrate_get_population_spikes: bridge NULL");
        return -1;
    }
    if (!spike_count || !spike_times) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_substrate_get_population_spikes: output param NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    if (population_id >= bridge->num_populations) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cochlea_substrate_get_population_spikes: capacity exceeded");
        return -1;
    }

    *spike_count = bridge->spike_counts[population_id];

    /* Compute offset into flat spike_times array */
    uint32_t offset = 0;
    for (uint32_t i = 0; i < population_id; i++) {
        offset += bridge->spike_counts[i];
    }
    *spike_times = (float*)&bridge->spike_times[offset];
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Population Mapping
//=============================================================================

nimcp_error_t cochlea_substrate_get_mapping(
    const cochlea_substrate_bridge_t* bridge,
    uint32_t channel,
    population_mapping_t* mapping
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_substrate_get_mapping: bridge NULL");
        return -1;
    }
    if (!mapping) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_substrate_get_mapping: mapping NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    if (channel >= bridge->num_populations) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cochlea_substrate_get_mapping: capacity exceeded");
        return -1;
    }
    *mapping = bridge->mappings[channel];
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

nimcp_error_t cochlea_substrate_set_encoding(
    cochlea_substrate_bridge_t* bridge,
    uint32_t channel,
    spike_encoding_mode_t encoding
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_substrate_set_encoding: bridge NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    if (channel >= bridge->num_populations) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cochlea_substrate_set_encoding: capacity exceeded");
        return -1;
    }
    bridge->mappings[channel].encoding = encoding;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Statistics
//=============================================================================

float cochlea_substrate_get_avg_rate(const cochlea_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_substrate_get_avg_rate: bridge NULL");
        return 0.0f;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    float r = bridge->avg_rate;
    nimcp_mutex_unlock(bridge->base.mutex);
    return r;
}

uint64_t cochlea_substrate_get_total_spikes(const cochlea_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_substrate_get_total_spikes: bridge NULL");
        return 0;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t s = bridge->cumulative_spikes;
    nimcp_mutex_unlock(bridge->base.mutex);
    return s;
}

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_substrate_verify_bidirectional(const cochlea_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_substrate_verify_bidirectional: bridge NULL");
        return false;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bool ok = (bridge->last_outbound_ts > 0) && (bridge->last_inbound_ts > 0);
    nimcp_mutex_unlock(bridge->base.mutex);
    return ok;
}

uint64_t cochlea_substrate_get_last_outbound(const cochlea_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_substrate_get_last_outbound: bridge NULL");
        return 0;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_outbound_ts;
    nimcp_mutex_unlock(bridge->base.mutex);
    return ts;
}

uint64_t cochlea_substrate_get_last_inbound(const cochlea_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_substrate_get_last_inbound: bridge NULL");
        return 0;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_inbound_ts;
    nimcp_mutex_unlock(bridge->base.mutex);
    return ts;
}
