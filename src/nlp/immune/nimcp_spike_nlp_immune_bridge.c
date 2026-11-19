/**
 * @file nimcp_spike_nlp_immune_bridge.c
 * @brief Spike-based NLP-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "nlp/immune/nimcp_spike_nlp_immune_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_router.h"
#include <string.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(spike_nlp_immune_bridge)

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Compute spike synchrony from spike times
 *
 * WHAT: Measure temporal correlation of spikes
 * WHY:  Excessive synchrony indicates pathological activity
 * HOW:  Count spikes in small time bins, compute variance
 */
static float compute_spike_synchrony(
    const uint64_t* spike_times,
    uint32_t num_spikes,
    float time_window_ms
) {
    if (num_spikes < 2) return 0.0f;

    /* Simple synchrony: spikes clustered in time */
    uint32_t clustered = 0;
    for (uint32_t i = 1; i < num_spikes; i++) {
        uint64_t isi = spike_times[i] - spike_times[i-1];
        if (isi < 10) {  /* 10ms window */
            clustered++;
        }
    }

    return (float)clustered / (float)(num_spikes - 1);
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int spike_nlp_immune_default_config(spike_nlp_immune_config_t* config) {
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "spike_nlp_immune_default_config: NULL config");
        return -1;
    }

    config->enable_cytokine_spike_modulation = true;
    config->enable_inflammation_jitter = true;
    config->enable_pattern_anomaly_detection = true;
    config->enable_healthy_dynamics_il10 = true;
    config->enable_aberrant_pattern_inflammation = true;

    config->cytokine_sensitivity = 1.0f;
    config->inflammation_sensitivity = 1.0f;
    config->anomaly_sensitivity = 1.0f;

    config->synchrony_threshold = SPIKE_SYNCHRONY_THRESHOLD;
    config->burst_threshold = SPIKE_BURST_THRESHOLD;
    config->silence_threshold_ms = SPIKE_SILENCE_THRESHOLD;
    config->rate_anomaly_threshold = SPIKE_RATE_ANOMALY_THRESHOLD;

    return 0;
}

spike_nlp_immune_bridge_t* spike_nlp_immune_bridge_create(
    const spike_nlp_immune_config_t* config,
    brain_immune_system_t* immune_system,
    neural_network_t network
) {
    if (!immune_system || !network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spike_nlp_immune_bridge_create: immune_system and network required");
        NIMCP_LOGGING_ERROR("spike_nlp_immune_bridge_create: invalid parameters");
        return NULL;
    }

    spike_nlp_immune_bridge_t* bridge = nimcp_malloc(sizeof(spike_nlp_immune_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "spike_nlp_immune_bridge_create: allocation failed");
        NIMCP_LOGGING_ERROR("spike_nlp_immune_bridge_create: allocation failed");
        return NULL;
    }

    memset(bridge, 0, sizeof(spike_nlp_immune_bridge_t));

    spike_nlp_immune_config_t default_config;
    if (!config) {
        spike_nlp_immune_default_config(&default_config);
        config = &default_config;
    }

    bridge->config = *config;
    bridge->immune_system = immune_system;
    bridge->network = network;
    bridge->last_update_time = get_time_ms();

    nimcp_mutex_t* mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (mutex) {
        nimcp_mutex_init(mutex, NULL);
        bridge->base.mutex = mutex;
    }

    NIMCP_LOGGING_INFO("spike_nlp_immune_bridge: created successfully");
    return bridge;
}

void spike_nlp_immune_bridge_destroy(spike_nlp_immune_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        spike_nlp_immune_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        nimcp_mutex_t* mutex = (nimcp_mutex_t*)bridge->base.mutex;
        nimcp_mutex_destroy(mutex);
        nimcp_free(mutex);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Immune → Spike NLP API
 * ============================================================================ */

int spike_nlp_immune_apply_cytokine_effects(spike_nlp_immune_bridge_t* bridge) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "spike_nlp_immune_apply_cytokine_effects: NULL bridge or immune_system");
        return -1;
    }

    brain_immune_stats_t stats;
    if (brain_immune_get_stats(bridge->immune_system, &stats) != 0) {
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "spike_nlp_immune_apply_cytokine_effects: failed to get immune stats");
        return -1;
    }

    float inflammation_factor = (float)stats.inflammation_sites / 10.0f;
    inflammation_factor = nimcp_clamp01(inflammation_factor);

    bridge->cytokine_effects.il1_rate_factor =
        1.0f + (CYTOKINE_IL1_SPIKE_RATE_FACTOR - 1.0f) * inflammation_factor;
    bridge->cytokine_effects.tnf_rate_factor =
        1.0f + (CYTOKINE_TNF_SPIKE_RATE_FACTOR - 1.0f) * inflammation_factor;
    bridge->cytokine_effects.il10_rate_factor =
        1.0f + (CYTOKINE_IL10_SPIKE_RATE_FACTOR - 1.0f) * (1.0f - inflammation_factor);

    bridge->cytokine_effects.total_rate_modulation =
        bridge->cytokine_effects.il1_rate_factor *
        bridge->cytokine_effects.tnf_rate_factor *
        bridge->cytokine_effects.il10_rate_factor;

    bridge->cytokine_modulations++;
    return 0;
}

int spike_nlp_immune_apply_inflammation_jitter(spike_nlp_immune_bridge_t* bridge) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "spike_nlp_immune_apply_inflammation_jitter: NULL bridge or immune_system");
        return -1;
    }

    brain_immune_stats_t stats;
    if (brain_immune_get_stats(bridge->immune_system, &stats) != 0) {
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "spike_nlp_immune_apply_inflammation_jitter: failed to get immune stats");
        return -1;
    }

    brain_inflammation_level_t level;
    if (stats.inflammation_sites < 3) {
        level = INFLAMMATION_LOCAL;
    } else if (stats.inflammation_sites < 10) {
        level = INFLAMMATION_REGIONAL;
    } else {
        level = INFLAMMATION_SYSTEMIC;
    }

    bridge->cytokine_effects.timing_jitter_ms =
        INFLAMMATION_JITTER_BASE +
        (float)level * INFLAMMATION_JITTER_PER_LEVEL * bridge->config.inflammation_sensitivity;

    return 0;
}

float spike_nlp_immune_compute_rate_modulation(const spike_nlp_immune_bridge_t* bridge) {
    if (!bridge) return 1.0f;
    return bridge->cytokine_effects.total_rate_modulation;
}

/* ============================================================================
 * Spike NLP → Immune API
 * ============================================================================ */

int spike_nlp_immune_detect_pattern_anomalies(
    spike_nlp_immune_bridge_t* bridge,
    const uint64_t* spike_times,
    uint32_t num_spikes,
    float time_window_ms
) {
    if (!bridge || !spike_times || num_spikes == 0) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "spike_nlp_immune_detect_pattern_anomalies: invalid parameters");
        return -1;
    }

    /* Compute synchrony */
    bridge->spike_modulation.synchrony_level =
        compute_spike_synchrony(spike_times, num_spikes, time_window_ms);

    /* Detect excessive synchrony */
    bridge->spike_modulation.excessive_synchrony =
        bridge->spike_modulation.synchrony_level > bridge->config.synchrony_threshold;

    /* Trigger inflammation if anomaly detected */
    if (bridge->spike_modulation.excessive_synchrony && bridge->config.enable_aberrant_pattern_inflammation) {
        uint32_t cytokine_id;
        brain_immune_release_cytokine(
            bridge->immune_system,
            BRAIN_CYTOKINE_IL1,
            0,
            bridge->spike_modulation.synchrony_level,
            0,
            &cytokine_id
        );
        bridge->spike_modulation.pattern_triggered_inflammation = true;
        bridge->anomaly_triggers++;
    }

    bridge->spike_modulation.total_spikes += num_spikes;
    return 0;
}

int spike_nlp_immune_release_il10_from_healthy(
    spike_nlp_immune_bridge_t* bridge,
    const uint64_t* spike_times,
    uint32_t num_spikes
) {
    if (!bridge || !spike_times || num_spikes == 0) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "spike_nlp_immune_release_il10_from_healthy: invalid parameters");
        return -1;
    }
    if (!bridge->config.enable_healthy_dynamics_il10) return 0;

    float synchrony = compute_spike_synchrony(spike_times, num_spikes, 100.0f);

    /* Healthy = low synchrony, regular firing */
    if (synchrony < 0.3f && num_spikes > 5) {
        uint32_t cytokine_id;
        brain_immune_release_cytokine(
            bridge->immune_system,
            BRAIN_CYTOKINE_IL10,
            0,
            0.3f,  /* moderate IL-10 */
            0,
            &cytokine_id
        );
        bridge->spike_modulation.il10_from_healthy_dynamics = 0.3f;
        bridge->healthy_boosts++;
    }

    return 0;
}

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

int spike_nlp_immune_bridge_update(
    spike_nlp_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "spike_nlp_immune_bridge_update: NULL bridge");
        return -1;
    }

    nimcp_mutex_t* mutex = (nimcp_mutex_t*)bridge->base.mutex;
    if (mutex) nimcp_mutex_lock(mutex);

    spike_nlp_immune_apply_cytokine_effects(bridge);
    spike_nlp_immune_apply_inflammation_jitter(bridge);

    bridge->total_updates++;
    bridge->last_update_time = get_time_ms();

    if (mutex) nimcp_mutex_unlock(mutex);
    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

float spike_nlp_immune_get_rate_modulation(const spike_nlp_immune_bridge_t* bridge) {
    return spike_nlp_immune_compute_rate_modulation(bridge);
}

float spike_nlp_immune_get_timing_jitter(const spike_nlp_immune_bridge_t* bridge) {
    return bridge ? bridge->cytokine_effects.timing_jitter_ms : 0.0f;
}

bool spike_nlp_immune_has_pattern_anomaly(const spike_nlp_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->spike_modulation.excessive_synchrony ||
           bridge->spike_modulation.excessive_bursting ||
           bridge->spike_modulation.prolonged_silence;
}

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

int spike_nlp_immune_connect_bio_async(spike_nlp_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "spike_nlp_immune_connect_bio_async: NULL bridge");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_SPIKE_NLP,
        .module_name = "spike_nlp_immune_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("spike_nlp_immune_bridge: connected to bio-async router");
    } else {
        NIMCP_LOGGING_DEBUG("spike_nlp_immune_bridge: bio-async router not available");
    }
    return 0;
}

int spike_nlp_immune_disconnect_bio_async(spike_nlp_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "spike_nlp_immune_disconnect_bio_async: NULL bridge");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("spike_nlp_immune_bridge: bio-async disconnected");
    return 0;
}

bool spike_nlp_immune_is_bio_async_connected(const spike_nlp_immune_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}
