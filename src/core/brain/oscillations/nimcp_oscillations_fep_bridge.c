/**
 * @file nimcp_oscillations_fep_bridge.c
 * @brief Free Energy Principle - Oscillations Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Implementation of bidirectional FEP-oscillations integration
 * WHY:  Neural oscillations provide temporal structure for predictive processing
 * HOW:  Map FEP errors/predictions to oscillatory power, derive precision from bands
 */

#include "core/brain/oscillations/nimcp_oscillations_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(oscillations_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

static inline float safe_divide(float num, float denom, float default_val) {
    return (fabsf(denom) > 1e-10f) ? (num / denom) : default_val;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int oscillations_fep_bridge_default_config(
    oscillations_fep_config_t* config
) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Enable all coupling mechanisms */
    config->enable_pe_gamma_coupling = true;
    config->enable_prediction_beta_coupling = true;
    config->enable_precision_alpha_coupling = true;
    config->enable_theta_gamma_pac = true;

    /* Set coupling gains */
    config->pe_gamma_gain = OSC_FEP_PE_GAMMA_COUPLING;
    config->prediction_beta_gain = OSC_FEP_PREDICTION_BETA_COUPLING;
    config->precision_alpha_gain = OSC_FEP_PRECISION_ALPHA_COUPLING;
    config->hierarchy_theta_gain = OSC_FEP_HIERARCHY_THETA_COUPLING;

    /* Set thresholds */
    config->pac_threshold = OSC_FEP_PAC_THRESHOLD;
    config->coherence_threshold = OSC_FEP_COHERENCE_THRESHOLD;

    /* Learning rates */
    config->power_adaptation_rate = 0.05f;
    config->phase_adaptation_rate = 0.1f;

    return 0;
}

oscillations_fep_bridge_t* oscillations_fep_bridge_create(
    const oscillations_fep_config_t* config
) {
    oscillations_fep_bridge_t* bridge = (oscillations_fep_bridge_t*)
        nimcp_calloc(1, sizeof(oscillations_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate oscillations-FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    /* Apply configuration */
    oscillations_fep_config_t default_cfg;
    if (!config) {
        oscillations_fep_bridge_default_config(&default_cfg);
        config = &default_cfg;
    }
    bridge->config = *config;

    /* Initialize state */
    memset(&bridge->state, 0, sizeof(oscillations_fep_state_t));
    bridge->state.gamma_alpha_ratio = 1.0f;
    bridge->state.effective_precision = 1.0f;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "oscillations_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        oscillations_fep_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "oscillations_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Oscillations-FEP bridge created");
    return bridge;
}

void oscillations_fep_bridge_destroy(
    oscillations_fep_bridge_t* bridge
) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        oscillations_fep_bridge_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Oscillations-FEP bridge destroyed");
}

/* ============================================================================
 * Connection Implementation
 * ============================================================================ */

int oscillations_fep_bridge_connect_fep(
    oscillations_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    if (!bridge || !fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_fep_bridge_connect_fep: required parameter is NULL (bridge, fep)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Oscillations-FEP bridge connected to FEP system");
    return 0;
}

int oscillations_fep_bridge_connect_oscillations(
    oscillations_fep_bridge_t* bridge,
    brain_complex_oscillation_state_t* osc_state
) {
    if (!bridge || !osc_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_fep_bridge_connect_oscillations: required parameter is NULL (bridge, osc_state)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->osc_state = osc_state;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Oscillations-FEP bridge connected to oscillation state");
    return 0;
}

int oscillations_fep_bridge_disconnect(
    oscillations_fep_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->osc_state = NULL;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * FEP → Oscillations Implementation
 * ============================================================================ */

int oscillations_fep_modulate_gamma_from_pe(
    oscillations_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_fep_modulate_gamma_from_pe: required parameter is NULL (bridge, bridge->fep_system)");
        return -1;
    }
    if (!bridge->config.enable_pe_gamma_coupling) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    fep_system_t* fep = bridge->fep_system;

    /* Compute total prediction error magnitude */
    float total_pe = 0.0f;
    float total_precision = 0.0f;
    uint32_t count = 0;

    for (uint32_t l = 0; l < fep->num_levels; l++) {
        fep_hierarchy_level_t* level = &fep->levels[l];
        total_pe += level->errors.magnitude;

        /* Weight by precision */
        for (uint32_t i = 0; i < level->errors.dim; i++) {
            total_precision += level->errors.precision[i];
            count++;
        }
    }

    float mean_pe = (fep->num_levels > 0) ?
        total_pe / (float)fep->num_levels : 0.0f;
    float mean_precision = (count > 0) ?
        total_precision / (float)count : 1.0f;

    /* Gamma power = PE × precision × gain */
    float gamma_modulation = mean_pe * mean_precision *
        bridge->config.pe_gamma_gain;
    gamma_modulation = nimcp_clampf(gamma_modulation, 0.0f, 10.0f);

    bridge->effects.gamma_modulation = gamma_modulation;
    bridge->state.current_gamma = gamma_modulation;
    bridge->stats.gamma_modulations++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int oscillations_fep_modulate_beta_from_predictions(
    oscillations_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_fep_modulate_beta_from_predictions: required parameter is NULL (bridge, bridge->fep_system)");
        return -1;
    }
    if (!bridge->config.enable_prediction_beta_coupling) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    fep_system_t* fep = bridge->fep_system;

    /* Compute prediction strength from beliefs */
    float total_prediction_strength = 0.0f;

    for (uint32_t l = 0; l < fep->num_levels; l++) {
        fep_hierarchy_level_t* level = &fep->levels[l];

        /* Prediction strength from belief variance */
        for (uint32_t i = 0; i < level->beliefs.dim; i++) {
            /* Stronger predictions = lower variance */
            float confidence = safe_divide(1.0f, level->beliefs.variance[i], 1.0f);
            total_prediction_strength += confidence;
        }
    }

    float mean_prediction_strength = 0.0f;
    uint32_t total_dims = 0;
    for (uint32_t l = 0; l < fep->num_levels; l++) {
        total_dims += fep->levels[l].beliefs.dim;
    }
    if (total_dims > 0) {
        mean_prediction_strength = total_prediction_strength / (float)total_dims;
    }

    /* Beta power = prediction strength × gain */
    float beta_modulation = mean_prediction_strength *
        bridge->config.prediction_beta_gain;
    beta_modulation = nimcp_clampf(beta_modulation, 0.0f, 10.0f);

    bridge->effects.beta_modulation = beta_modulation;
    bridge->state.current_beta = beta_modulation;
    bridge->stats.beta_modulations++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int oscillations_fep_modulate_alpha_from_precision(
    oscillations_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_fep_modulate_alpha_from_precision: required parameter is NULL (bridge, bridge->fep_system)");
        return -1;
    }
    if (!bridge->config.enable_precision_alpha_coupling) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    fep_system_t* fep = bridge->fep_system;

    /* Compute mean precision */
    float total_precision = 0.0f;
    uint32_t count = 0;

    for (uint32_t l = 0; l < fep->num_levels; l++) {
        fep_hierarchy_level_t* level = &fep->levels[l];
        for (uint32_t i = 0; i < level->beliefs.dim; i++) {
            total_precision += level->beliefs.precision[i];
            count++;
        }
    }

    float mean_precision = (count > 0) ?
        total_precision / (float)count : 1.0f;

    /* Alpha power = 1/precision × gain (inverse relationship) */
    float alpha_modulation = safe_divide(
        bridge->config.precision_alpha_gain,
        mean_precision,
        1.0f);
    alpha_modulation = nimcp_clampf(alpha_modulation, 0.0f, 10.0f);

    bridge->effects.alpha_modulation = alpha_modulation;
    bridge->state.current_alpha = alpha_modulation;
    bridge->stats.alpha_modulations++;

    /* Update gamma/alpha ratio (precision) */
    bridge->state.gamma_alpha_ratio = safe_divide(
        bridge->state.current_gamma,
        bridge->state.current_alpha,
        1.0f);
    bridge->state.effective_precision = bridge->state.gamma_alpha_ratio;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int oscillations_fep_generate_theta_gamma_pac(
    oscillations_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_fep_generate_theta_gamma_pac: required parameter is NULL (bridge, bridge->fep_system)");
        return -1;
    }
    if (!bridge->config.enable_theta_gamma_pac) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Theta modulates gamma based on hierarchy level */
    /* Higher hierarchy levels → stronger theta modulation */
    fep_system_t* fep = bridge->fep_system;

    float theta_modulation = (float)fep->num_levels *
        bridge->config.hierarchy_theta_gain;
    theta_modulation = nimcp_clampf(theta_modulation, 0.0f, 10.0f);

    bridge->effects.theta_modulation = theta_modulation;
    bridge->state.current_theta = theta_modulation;

    /* Simplified PAC strength based on hierarchy activity */
    float pac_strength = theta_modulation * bridge->state.current_gamma / 10.0f;
    pac_strength = nimcp_clampf(pac_strength, 0.0f, 1.0f);

    bridge->effects.theta_gamma_pac = pac_strength;
    bridge->state.theta_gamma_pac_strength = pac_strength;

    if (pac_strength > bridge->config.pac_threshold) {
        bridge->stats.pac_detections++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Oscillations → FEP Implementation
 * ============================================================================ */

int oscillations_fep_derive_precision_from_ratio(
    oscillations_fep_bridge_t* bridge,
    float* precision
) {
    if (!bridge || !precision) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_fep_derive_precision_from_ratio: required parameter is NULL (bridge, precision)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Precision = gamma/alpha ratio */
    *precision = bridge->state.gamma_alpha_ratio;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int oscillations_fep_weight_errors_by_gamma(
    oscillations_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_fep_weight_errors_by_gamma: required parameter is NULL (bridge, bridge->fep_system)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    fep_system_t* fep = bridge->fep_system;
    float gamma_weight = bridge->state.current_gamma;

    /* Apply gamma-based weighting to prediction errors */
    for (uint32_t l = 0; l < fep->num_levels; l++) {
        fep_hierarchy_level_t* level = &fep->levels[l];

        for (uint32_t i = 0; i < level->errors.dim; i++) {
            /* Scale weighted error by gamma power */
            level->errors.weighted_error[i] =
                level->errors.error[i] * gamma_weight;
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int oscillations_fep_bind_beliefs_via_coherence(
    oscillations_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->osc_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_fep_bind_beliefs_via_coherence: required parameter is NULL (bridge, bridge->osc_state)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute phase coherence from oscillation state */
    /* Note: This is a placeholder - actual implementation would use
     * brain_complex_oscillation_compute_coherence() */

    /* For now, set coherence based on gamma power (simplified) */
    float coherence = bridge->state.current_gamma / 10.0f;
    coherence = nimcp_clampf(coherence, 0.0f, 1.0f);

    bridge->state.cross_regional_coherence = coherence;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Analysis Implementation
 * ============================================================================ */

int oscillations_fep_compute_band_power(
    oscillations_fep_bridge_t* bridge,
    oscillation_band_power_t* band_power
) {
    if (!bridge || !band_power) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_fep_compute_band_power: required parameter is NULL (bridge, band_power)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Extract current band power from state */
    band_power->delta = 1.0f; /* Placeholder */
    band_power->theta = bridge->state.current_theta;
    band_power->alpha = bridge->state.current_alpha;
    band_power->beta = bridge->state.current_beta;
    band_power->gamma = bridge->state.current_gamma;

    bridge->state.band_power = *band_power;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int oscillations_fep_detect_pac(
    oscillations_fep_bridge_t* bridge,
    float* pac_strength,
    float* preferred_phase
) {
    if (!bridge || !pac_strength || !preferred_phase) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_fep_detect_pac: required parameter is NULL (bridge, pac_strength, preferred_phase)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    *pac_strength = bridge->state.theta_gamma_pac_strength;
    *preferred_phase = bridge->state.pac_preferred_phase;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Update Cycle Implementation
 * ============================================================================ */

int oscillations_fep_bridge_update(
    oscillations_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    (void)delta_ms; /* Unused for now */

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* 1. FEP → Oscillations: Modulate oscillatory power */
    if (bridge->config.enable_pe_gamma_coupling) {
        oscillations_fep_modulate_gamma_from_pe(bridge);
    }

    if (bridge->config.enable_prediction_beta_coupling) {
        oscillations_fep_modulate_beta_from_predictions(bridge);
    }

    if (bridge->config.enable_precision_alpha_coupling) {
        oscillations_fep_modulate_alpha_from_precision(bridge);
    }

    if (bridge->config.enable_theta_gamma_pac) {
        oscillations_fep_generate_theta_gamma_pac(bridge);
    }

    /* 2. Oscillations → FEP: Derive precision, weight errors */
    oscillations_fep_weight_errors_by_gamma(bridge);
    oscillations_fep_bind_beliefs_via_coherence(bridge);

    /* 3. Update statistics */
    bridge->stats.avg_gamma_power =
        (bridge->stats.avg_gamma_power * 0.99f) +
        (bridge->state.current_gamma * 0.01f);

    bridge->stats.avg_beta_power =
        (bridge->stats.avg_beta_power * 0.99f) +
        (bridge->state.current_beta * 0.01f);

    bridge->stats.avg_alpha_power =
        (bridge->stats.avg_alpha_power * 0.99f) +
        (bridge->state.current_alpha * 0.01f);

    bridge->stats.avg_theta_power =
        (bridge->stats.avg_theta_power * 0.99f) +
        (bridge->state.current_theta * 0.01f);

    bridge->stats.avg_theta_gamma_pac =
        (bridge->stats.avg_theta_gamma_pac * 0.99f) +
        (bridge->state.theta_gamma_pac_strength * 0.01f);

    bridge->stats.avg_gamma_alpha_ratio =
        (bridge->stats.avg_gamma_alpha_ratio * 0.99f) +
        (bridge->state.gamma_alpha_ratio * 0.01f);

    bridge->stats.avg_coherence =
        (bridge->stats.avg_coherence * 0.99f) +
        (bridge->state.cross_regional_coherence * 0.01f);

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * State/Stats Implementation
 * ============================================================================ */

int oscillations_fep_bridge_get_state(
    const oscillations_fep_bridge_t* bridge,
    oscillations_fep_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_fep_bridge_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    *state = bridge->state;
    return 0;
}

int oscillations_fep_bridge_get_stats(
    const oscillations_fep_bridge_t* bridge,
    oscillations_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_fep_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int oscillations_fep_bridge_connect_bio_async(
    oscillations_fep_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_OSCILLATIONS_BRIDGE,
        .module_name = "oscillations_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Oscillations-FEP bridge connected to bio-async");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }
    return 0;
}

int oscillations_fep_bridge_disconnect_bio_async(
    oscillations_fep_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool oscillations_fep_bridge_is_bio_async_connected(
    const oscillations_fep_bridge_t* bridge
) {
    return bridge && bridge->base.bio_async_enabled;
}
