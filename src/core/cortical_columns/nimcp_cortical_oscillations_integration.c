/**
 * @file nimcp_cortical_oscillations_integration.c
 * @brief Implementation of cortical oscillations integration
 * @version 1.0.0
 * @date 2025-12-15
 */

#include "core/cortical_columns/nimcp_cortical_oscillations_integration.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cortical_oscillations_integration)


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Normalize phase to [0, 2π]
 *
 * WHAT: Wrap phase to valid range
 * WHY:  Prevent phase accumulation overflow
 * HOW:  Use fmod to wrap to [0, 2π]
 */
static float normalize_phase(float phase) {
    float two_pi = 2.0f * (float)M_PI;
    float result = fmodf(phase, two_pi);
    if (result < 0.0f) {
        result += two_pi;
    }
    return result;
}

/**
 * @brief Compute circular mean of phases
 *
 * WHAT: Calculate mean angle using circular statistics
 * WHY:  Linear averaging doesn't work for angles
 * HOW:  Mean = atan2(Σsin(θ), Σcos(θ))
 */
static float circular_mean(const float* phases, uint32_t count) {
    if (!phases || count == 0) return 0.0f;

    float sum_sin = 0.0f;
    float sum_cos = 0.0f;

    for (uint32_t i = 0; i < count; i++) {
        sum_sin += sinf(phases[i]);
        sum_cos += cosf(phases[i]);
    }

    return atan2f(sum_sin / (float)count, sum_cos / (float)count);
}

/**
 * @brief Compute circular variance
 *
 * WHAT: Calculate variance using circular statistics
 * WHY:  Measure phase dispersion
 * HOW:  Var = 1 - R, where R = |mean resultant vector|
 */
static float circular_variance(const float* phases, uint32_t count) {
    if (!phases || count == 0) return 1.0f;

    float sum_sin = 0.0f;
    float sum_cos = 0.0f;

    for (uint32_t i = 0; i < count; i++) {
        sum_sin += sinf(phases[i]);
        sum_cos += cosf(phases[i]);
    }

    float r = sqrtf(sum_sin * sum_sin + sum_cos * sum_cos) / (float)count;
    return 1.0f - r;
}

/**
 * @brief Get current time in microseconds
 *
 * WHAT: Platform-independent microsecond timestamp
 * WHY:  Track precise timing for oscillations
 * HOW:  Use clock_gettime on Linux
 */
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int cortical_oscillation_default_config(cortical_oscillation_config_t* config) {
    if (!config) {
        NIMCP_LOGGING_ERROR("Null config pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Oscillation frequencies */
    config->gamma_frequency = 40.0f;
    config->theta_frequency = 6.0f;
    config->alpha_frequency = 10.0f;
    config->beta_frequency = 20.0f;

    /* Phase-locking parameters */
    config->phase_lock_threshold = 0.7f;
    config->phase_lock_window = 25.0f;
    config->enable_phase_reset = true;
    config->phase_reset_strength = 0.8f;

    /* Cross-frequency coupling */
    config->enable_theta_gamma_coupling = true;
    config->enable_alpha_gating = true;
    config->pac_modulation_depth = 0.5f;
    config->alpha_gating_threshold = 0.3f;

    /* Competition timing */
    config->gate_competition_by_phase = true;
    config->competition_phase_window = (float)M_PI / 4.0f;
    config->min_competition_interval_ms = 20.0f;

    /* Coherence parameters */
    config->coherence_update_rate = 0.1f;
    config->coherence_window_size = 100;
    config->min_coherence_for_binding = 0.6f;

    return 0;
}

cortical_oscillation_integration_t* cortical_oscillation_create(
    const cortical_oscillation_config_t* config,
    hypercolumn_t* hypercolumn,
    brain_complex_oscillation_state_t* oscillator
) {
    /* Validate inputs */
    if (!hypercolumn) {
        NIMCP_LOGGING_ERROR("Null hypercolumn pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypercolumn is NULL");

        return NULL;
    }

    /* Allocate integration structure */
    cortical_oscillation_integration_t* integration =
        (cortical_oscillation_integration_t*)nimcp_malloc(sizeof(cortical_oscillation_integration_t));
    if (!integration) {
        NIMCP_LOGGING_ERROR("Failed to allocate integration structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_oscillation_create: integration alloc failed");

        return NULL;
    }

    memset(integration, 0, sizeof(cortical_oscillation_integration_t));

    /* Set configuration */
    if (config) {
        integration->config = *config;
    } else {
        cortical_oscillation_default_config(&integration->config);
    }

    /* Connect systems */
    integration->hypercolumn = hypercolumn;
    integration->oscillator = oscillator;

    /* Initialize phase state */
    integration->phase_state.gamma_phase = 0.0f;
    integration->phase_state.theta_phase = 0.0f;
    integration->phase_state.alpha_phase = 0.0f;
    integration->phase_state.beta_phase = 0.0f;
    integration->phase_state.gamma_amplitude = 1.0f;
    integration->phase_state.theta_amplitude = 1.0f;
    integration->phase_state.alpha_amplitude = 0.5f;
    integration->phase_state.beta_amplitude = 0.3f;
    integration->phase_state.last_update_us = get_time_us();

    /* Initialize coupling state */
    integration->coupling_state.theta_gamma_coupling = 0.0f;
    integration->coupling_state.preferred_theta_phase = 0.0f;
    integration->coupling_state.alpha_beta_coupling = 0.0f;
    integration->coupling_state.coupling_quality = 0.0f;

    /* Initialize coherence state */
    integration->coherence_state.gamma_coherence = 0.0f;
    integration->coherence_state.theta_coherence = 0.0f;
    integration->coherence_state.mean_phase_gamma = 0.0f;
    integration->coherence_state.mean_phase_theta = 0.0f;
    integration->coherence_state.phase_variance = 1.0f;
    integration->coherence_state.binding_active = false;

    /* Initialize gating state */
    integration->gating_state.feedforward_gain = 1.0f;
    integration->gating_state.feedback_gain = 0.5f;
    integration->gating_state.lateral_gain = 0.7f;
    integration->gating_state.competition_allowed = true;
    integration->gating_state.last_competition_us = 0;

    /* Create mutex */
    integration->mutex = nimcp_platform_mutex_create();
    if (!integration->mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(integration);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_oscillation_create: integration->mutex is NULL");
        return NULL;
    }

    /* Bio-async disabled by default */
    integration->bio_async_enabled = false;
    integration->bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Created cortical oscillations integration");
    return integration;
}

void cortical_oscillation_destroy(cortical_oscillation_integration_t* integration) {
    if (!integration) return;

    /* Disconnect bio-async if connected */
    if (integration->bio_async_enabled) {
        cortical_oscillation_disconnect_bio_async(integration);
    }

    /* Destroy mutex */
    if (integration->mutex) {
        nimcp_platform_mutex_destroy(integration->mutex);
        nimcp_free(integration->mutex);
        integration->mutex = NULL;
    }

    nimcp_free(integration);
    NIMCP_LOGGING_INFO("Destroyed cortical oscillations integration");
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int cortical_oscillation_update_phase(
    cortical_oscillation_integration_t* integration,
    float delta_time_ms
) {
    if (!integration) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(integration->mutex);

    /* Convert time to seconds */
    float dt = delta_time_ms / 1000.0f;

    /* Update phases: phase += 2π * frequency * dt */
    float two_pi = 2.0f * (float)M_PI;

    integration->phase_state.gamma_phase += two_pi * integration->config.gamma_frequency * dt;
    integration->phase_state.theta_phase += two_pi * integration->config.theta_frequency * dt;
    integration->phase_state.alpha_phase += two_pi * integration->config.alpha_frequency * dt;
    integration->phase_state.beta_phase += two_pi * integration->config.beta_frequency * dt;

    /* Normalize phases to [0, 2π] */
    integration->phase_state.gamma_phase = normalize_phase(integration->phase_state.gamma_phase);
    integration->phase_state.theta_phase = normalize_phase(integration->phase_state.theta_phase);
    integration->phase_state.alpha_phase = normalize_phase(integration->phase_state.alpha_phase);
    integration->phase_state.beta_phase = normalize_phase(integration->phase_state.beta_phase);

    /* Update timestamp */
    integration->phase_state.last_update_us = get_time_us();
    integration->stats.total_updates++;

    nimcp_platform_mutex_unlock(integration->mutex);
    return 0;
}

int cortical_oscillation_compute_coherence(
    cortical_oscillation_integration_t* integration
) {
    if (!integration || !integration->hypercolumn) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(integration->mutex);

    /* Get hypercolumn statistics */
    cc_hypercolumn_stats_t hcol_stats;
    hypercolumn_get_stats(integration->hypercolumn, &hcol_stats);

    /* For now, use simplified coherence based on entropy */
    /* Lower entropy = higher coherence */
    float coherence = 1.0f - (hcol_stats.entropy / logf((float)hcol_stats.num_minicolumns));
    if (coherence < 0.0f) coherence = 0.0f;
    if (coherence > 1.0f) coherence = 1.0f;

    /* Update gamma coherence with exponential moving average */
    float alpha = integration->config.coherence_update_rate;
    integration->coherence_state.gamma_coherence =
        alpha * coherence + (1.0f - alpha) * integration->coherence_state.gamma_coherence;

    /* Set theta coherence (simplified - would need temporal tracking) */
    integration->coherence_state.theta_coherence = integration->coherence_state.gamma_coherence * 0.8f;

    /* Update mean phases */
    integration->coherence_state.mean_phase_gamma = integration->phase_state.gamma_phase;
    integration->coherence_state.mean_phase_theta = integration->phase_state.theta_phase;

    /* Update phase variance */
    integration->coherence_state.phase_variance = 1.0f - integration->coherence_state.gamma_coherence;

    /* Check binding threshold */
    integration->coherence_state.binding_active =
        (integration->coherence_state.gamma_coherence >= integration->config.min_coherence_for_binding);

    if (integration->coherence_state.binding_active) {
        integration->stats.binding_events++;
    }

    nimcp_platform_mutex_unlock(integration->mutex);
    return 0;
}

int cortical_oscillation_apply_theta_gamma_coupling(
    cortical_oscillation_integration_t* integration
) {
    if (!integration) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!integration->config.enable_theta_gamma_coupling) {
        return 0;
    }

    nimcp_platform_mutex_lock(integration->mutex);

    /* Compute phase difference */
    float phase_diff = integration->phase_state.theta_phase -
                      integration->coupling_state.preferred_theta_phase;

    /* Modulate gamma amplitude by theta phase */
    /* gamma_amp *= (1 + depth * cos(phase_diff)) */
    float modulation = 1.0f + integration->config.pac_modulation_depth * cosf(phase_diff);
    if (modulation < 0.1f) modulation = 0.1f;
    if (modulation > 2.0f) modulation = 2.0f;

    integration->phase_state.gamma_amplitude *= modulation;

    /* Update coupling strength */
    float coupling = fabsf(cosf(phase_diff));
    float alpha = integration->config.coherence_update_rate;
    integration->coupling_state.theta_gamma_coupling =
        alpha * coupling + (1.0f - alpha) * integration->coupling_state.theta_gamma_coupling;

    /* Update average coupling statistic */
    integration->stats.avg_theta_gamma_coupling =
        (integration->stats.avg_theta_gamma_coupling * 0.99f) +
        (integration->coupling_state.theta_gamma_coupling * 0.01f);

    nimcp_platform_mutex_unlock(integration->mutex);
    return 0;
}

int cortical_oscillation_update_gating(
    cortical_oscillation_integration_t* integration
) {
    if (!integration) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!integration->config.enable_alpha_gating) {
        return 0;
    }

    nimcp_platform_mutex_lock(integration->mutex);

    /* Alpha power inversely correlates with feedforward gain */
    /* Low alpha = active processing, high feedforward */
    float alpha_power = integration->phase_state.alpha_amplitude;

    if (alpha_power < integration->config.alpha_gating_threshold) {
        /* Low alpha: active processing */
        integration->gating_state.feedforward_gain = 1.0f;
        integration->gating_state.feedback_gain = 0.3f;
    } else {
        /* High alpha: inhibition, increase feedback */
        integration->gating_state.feedforward_gain = 0.5f;
        integration->gating_state.feedback_gain = 0.8f;
    }

    /* Lateral gain based on gamma coherence */
    integration->gating_state.lateral_gain = integration->coherence_state.gamma_coherence;

    nimcp_platform_mutex_unlock(integration->mutex);
    return 0;
}

/* ============================================================================
 * Competition and Phase Locking Implementation
 * ============================================================================ */

bool cortical_oscillation_gate_competition(
    const cortical_oscillation_integration_t* integration
) {
    if (!integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_oscillation_gate_competition: integration is NULL");
        return false;
    }

    if (!integration->config.gate_competition_by_phase) {
        return true;
    }

    /* Check minimum interval since last competition */
    uint64_t current_time = get_time_us();
    uint64_t min_interval_us = (uint64_t)(integration->config.min_competition_interval_ms * 1000.0f);

    if (integration->gating_state.last_competition_us > 0) {
        uint64_t elapsed = current_time - integration->gating_state.last_competition_us;
        if (elapsed < min_interval_us) {
            return false;  /* Normal flow: competition interval not yet met */
        }
    }

    /* Competition occurs near gamma peaks (around π/2 and 3π/2) */
    float gamma_phase = integration->phase_state.gamma_phase;
    float half_pi = (float)M_PI / 2.0f;
    float three_half_pi = 3.0f * (float)M_PI / 2.0f;
    float window = integration->config.competition_phase_window;

    bool near_peak1 = fabsf(gamma_phase - half_pi) < window;
    bool near_peak2 = fabsf(gamma_phase - three_half_pi) < window;

    return (near_peak1 || near_peak2);
}

int cortical_oscillation_reset_phase(
    cortical_oscillation_integration_t* integration,
    float target_gamma_phase,
    float target_theta_phase
) {
    if (!integration) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(integration->mutex);

    float strength = integration->config.phase_reset_strength;

    /* Blend current phase with target */
    integration->phase_state.gamma_phase =
        (1.0f - strength) * integration->phase_state.gamma_phase +
        strength * target_gamma_phase;

    integration->phase_state.theta_phase =
        (1.0f - strength) * integration->phase_state.theta_phase +
        strength * target_theta_phase;

    /* Normalize phases */
    integration->phase_state.gamma_phase = normalize_phase(integration->phase_state.gamma_phase);
    integration->phase_state.theta_phase = normalize_phase(integration->phase_state.theta_phase);

    integration->stats.phase_resets++;

    nimcp_platform_mutex_unlock(integration->mutex);
    return 0;
}

bool cortical_oscillation_is_binding_active(
    const cortical_oscillation_integration_t* integration
) {
    if (!integration) {
        return false;
    }

    return integration->coherence_state.binding_active;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int cortical_oscillation_get_phase_state(
    const cortical_oscillation_integration_t* integration,
    oscillation_phase_state_t* state
) {
    if (!integration || !state) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(integration->mutex);
    *state = integration->phase_state;
    nimcp_platform_mutex_unlock(integration->mutex);

    return 0;
}

int cortical_oscillation_get_coherence_state(
    const cortical_oscillation_integration_t* integration,
    coherence_state_t* state
) {
    if (!integration || !state) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(integration->mutex);
    *state = integration->coherence_state;
    nimcp_platform_mutex_unlock(integration->mutex);

    return 0;
}

int cortical_oscillation_get_coupling_state(
    const cortical_oscillation_integration_t* integration,
    coupling_state_t* state
) {
    if (!integration || !state) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(integration->mutex);
    *state = integration->coupling_state;
    nimcp_platform_mutex_unlock(integration->mutex);

    return 0;
}

int cortical_oscillation_get_gating_state(
    const cortical_oscillation_integration_t* integration,
    gating_state_t* state
) {
    if (!integration || !state) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(integration->mutex);
    *state = integration->gating_state;
    nimcp_platform_mutex_unlock(integration->mutex);

    return 0;
}

int cortical_oscillation_get_stats(
    const cortical_oscillation_integration_t* integration,
    cortical_oscillation_stats_t* stats
) {
    if (!integration || !stats) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(integration->mutex);
    *stats = integration->stats;
    nimcp_platform_mutex_unlock(integration->mutex);

    return 0;
}

/* ============================================================================
 * Connection Implementation
 * ============================================================================ */

int cortical_oscillation_connect_oscillator(
    cortical_oscillation_integration_t* integration,
    brain_complex_oscillation_state_t* oscillator
) {
    if (!integration) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(integration->mutex);
    integration->oscillator = oscillator;
    nimcp_platform_mutex_unlock(integration->mutex);

    NIMCP_LOGGING_INFO("Connected external oscillator");
    return 0;
}

int cortical_oscillation_disconnect_oscillator(
    cortical_oscillation_integration_t* integration
) {
    if (!integration) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(integration->mutex);
    integration->oscillator = NULL;
    nimcp_platform_mutex_unlock(integration->mutex);

    NIMCP_LOGGING_INFO("Disconnected oscillator");
    return 0;
}

/* ============================================================================
 * Bio-async Implementation
 * ============================================================================ */

int cortical_oscillation_connect_bio_async(
    cortical_oscillation_integration_t* integration
) {
    if (!integration) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (integration->bio_async_enabled) {
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_CORTICAL_OSCILLATIONS_INTEGRATION,
        .module_name = "cortical_oscillations_integration",
        .inbox_capacity = 32,
        .user_data = integration
    };

    integration->bio_ctx = bio_router_register_module(&info);
    if (integration->bio_ctx) {
        integration->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
        return 0;
    }
}

int cortical_oscillation_disconnect_bio_async(
    cortical_oscillation_integration_t* integration
) {
    if (!integration) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!integration->bio_async_enabled) {
        return 0;
    }

    if (integration->bio_ctx) {
        bio_router_unregister_module(integration->bio_ctx);
        integration->bio_ctx = NULL;
    }

    integration->bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    return 0;
}

bool cortical_oscillation_is_bio_async_connected(
    const cortical_oscillation_integration_t* integration
) {
    if (!integration) {
        return false;
    }

    return integration->bio_async_enabled;
}

/* ============================================================================
 * KG Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for cortical oscillations module self-knowledge
 * WHY:  Enable self-awareness and introspection about this module's role
 * HOW:  Query KG for entity info, log observations, check relations
 */
int cortical_oscillations_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Cortical_Oscillations_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            NIMCP_LOGGING_DEBUG("Cortical oscillations self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Cortical_Oscillations_Module");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Cortical_Oscillations_Module");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
