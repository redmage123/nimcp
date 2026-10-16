/**
 * @file nimcp_biological_timescales_fep_bridge.c
 * @brief Implementation of FEP bridge for biological timescales
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 */

#include "async/nimcp_biological_timescales_fep_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/validation/nimcp_common.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(biological_timescales_fep_bridge)

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int biological_timescales_fep_default_config(biological_timescales_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biological_timescales_fep_default_config: NULL config");
        return -1;
    }

    /* Set precision decay rates per oscillation band (biologically realistic) */
    config->precision_decay_rates[BIO_OSC_DELTA] = 0.01f;  /* Slow decay */
    config->precision_decay_rates[BIO_OSC_THETA] = 0.02f;
    config->precision_decay_rates[BIO_OSC_ALPHA] = 0.05f;
    config->precision_decay_rates[BIO_OSC_BETA] = 0.1f;
    config->precision_decay_rates[BIO_OSC_GAMMA] = 0.2f;   /* Fast decay */

    /* Set temporal horizons per band */
    config->temporal_horizon_ms[BIO_OSC_DELTA] = 1000.0f;  /* Long horizon */
    config->temporal_horizon_ms[BIO_OSC_THETA] = 500.0f;
    config->temporal_horizon_ms[BIO_OSC_ALPHA] = 200.0f;
    config->temporal_horizon_ms[BIO_OSC_BETA] = 100.0f;
    config->temporal_horizon_ms[BIO_OSC_GAMMA] = 50.0f;    /* Short horizon */

    config->enable_hierarchical_timing = true;
    config->map_levels_to_bands = true;

    /* Default mapping: lower FEP levels to faster bands */
    for (uint32_t i = 0; i < FEP_MAX_HIERARCHY_LEVELS; i++) {
        if (i < 5) {
            config->level_to_band[i] = (nimcp_oscillation_band_t)(4 - i); /* Reverse */
        } else {
            config->level_to_band[i] = BIO_OSC_DELTA;
        }
    }

    config->learning_rate = NIMCP_LEARNING_RATE_COARSE;
    config->enable_precision_learning = true;
    config->enable_decay_adaptation = true;
    config->enable_temporal_prediction = true;
    config->prediction_tolerance_ms = 10.0f;

    return 0;
}

biological_timescales_fep_bridge_t* biological_timescales_fep_create(
    const biological_timescales_fep_config_t* config,
    fep_system_t* fep_system
) {
    if (!config || !fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biological_timescales_fep_create: NULL parameter");
        NIMCP_LOGGING_ERROR("biological_timescales_fep_create: NULL parameter");
        return NULL;
    }

    biological_timescales_fep_bridge_t* bridge =
        (biological_timescales_fep_bridge_t*)nimcp_malloc(
            sizeof(biological_timescales_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "biological_timescales_fep_create: Failed to allocate");
        NIMCP_LOGGING_ERROR("biological_timescales_fep_create: Failed to allocate");
        return NULL;
    }

    memset(bridge, 0, sizeof(biological_timescales_fep_bridge_t));

    /* Copy configuration */
    memcpy(&bridge->config, config, sizeof(biological_timescales_fep_config_t));

    /* Connect FEP system */
    bridge->fep_system = fep_system;

    /* Initialize state */
    bridge->state.fep_active = true;
    bridge->state.timescales_initialized = true;
    bridge->state.current_band = BIO_OSC_GAMMA; /* Start with fast dynamics */

    /* Initialize effects */
    bridge->fep_effects.selected_band = BIO_OSC_GAMMA;
    bridge->fep_effects.enable_fast_dynamics = true;

    /* Initialize precision per band */
    for (int i = 0; i < 5; i++) {
        bridge->fep_effects.precision_per_band[i] = 1.0f;
        bridge->fep_effects.band_preferences[i] = 0.2f; /* Equal initially */
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "biological_timescales_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_MUTEX_INIT, "biological_timescales_fep_create: Failed to create mutex");
        NIMCP_LOGGING_ERROR("biological_timescales_fep_create: Failed to create mutex");
        nimcp_free(bridge);
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created biological timescales FEP bridge");

    return bridge;
}

void biological_timescales_fep_destroy(biological_timescales_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect from bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        biological_timescales_fep_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Integration Implementation
 * ============================================================================ */

int biological_timescales_fep_update_effects(biological_timescales_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biological_timescales_fep_update_effects: NULL bridge or fep_system");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current free energy */
    float free_energy = fep_get_free_energy(bridge->fep_system);
    bridge->stats.avg_free_energy =
        NIMCP_EMA_WEIGHT_SLOW * bridge->stats.avg_free_energy + NIMCP_EMA_WEIGHT_FAST * free_energy;

    /* Map free energy to timescale selection */
    /* Low FE = high certainty = fast timescales (gamma) */
    /* High FE = low certainty = slow timescales (delta) */
    if (free_energy < 1.0f) {
        bridge->fep_effects.selected_band = BIO_OSC_GAMMA;
        bridge->fep_effects.enable_fast_dynamics = true;
    } else if (free_energy < 3.0f) {
        bridge->fep_effects.selected_band = BIO_OSC_BETA;
        bridge->fep_effects.enable_fast_dynamics = true;
    } else if (free_energy < 5.0f) {
        bridge->fep_effects.selected_band = BIO_OSC_ALPHA;
        bridge->fep_effects.enable_fast_dynamics = false;
    } else if (free_energy < 8.0f) {
        bridge->fep_effects.selected_band = BIO_OSC_THETA;
        bridge->fep_effects.enable_fast_dynamics = false;
    } else {
        bridge->fep_effects.selected_band = BIO_OSC_DELTA;
        bridge->fep_effects.enable_fast_dynamics = false;
    }

    /* Update band preferences based on FEP certainty */
    float certainty = expf(-free_energy / 5.0f);
    for (int i = 0; i < 5; i++) {
        /* Higher bands (faster) preferred when certain */
        bridge->fep_effects.band_preferences[i] = certainty * (float)(i + 1) / 5.0f;
    }

    /* Update precision per band */
    for (int i = 0; i < 5; i++) {
        /* Fast bands have higher precision */
        bridge->fep_effects.precision_per_band[i] =
            (5.0f - (float)i) / 5.0f * (1.0f + certainty);
    }

    /* Set temporal resolution */
    bridge->fep_effects.temporal_resolution_ms =
        bridge->config.temporal_horizon_ms[bridge->fep_effects.selected_band];

    /* Modulate decay rate based on prediction confidence */
    bridge->fep_effects.decay_rate_modulation = 1.0f / (1.0f + certainty);

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int biological_timescales_fep_observe_timing(
    biological_timescales_fep_bridge_t* bridge,
    float interval_ms,
    nimcp_oscillation_band_t band
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biological_timescales_fep_observe_timing: NULL bridge");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update timing observations */
    bridge->timescales_effects.observed_interval_ms = interval_ms;
    bridge->timescales_effects.active_band = band;

    /* Compute timing prediction error */
    float predicted_interval = bridge->fep_effects.predicted_interval_ms;
    bridge->timescales_effects.timing_prediction_error =
        interval_ms - predicted_interval;

    /* Compute timing surprise */
    float error_magnitude = fabsf(bridge->timescales_effects.timing_prediction_error);
    bridge->timescales_effects.timing_surprise = error_magnitude / 10.0f;

    /* Check for unexpected timing */
    bridge->timescales_effects.unexpected_timing_event =
        error_magnitude > bridge->config.prediction_tolerance_ms;

    /* Measure decay rate from interval consistency */
    if (bridge->state.last_interval_ms > 0.0f) {
        float interval_ratio = interval_ms / bridge->state.last_interval_ms;
        bridge->timescales_effects.decay_consistency =
            1.0f - fminf(fabsf(interval_ratio - 1.0f), 1.0f);
    }

    /* Update statistics */
    bridge->stats.predictions_per_band[band]++;
    bridge->stats.avg_timing_error_ms =
        0.95f * bridge->stats.avg_timing_error_ms + 0.05f * error_magnitude;
    bridge->stats.avg_timing_surprise =
        0.95f * bridge->stats.avg_timing_surprise +
        0.05f * bridge->timescales_effects.timing_surprise;

    if (error_magnitude <= bridge->config.prediction_tolerance_ms) {
        bridge->state.accurate_predictions++;
    } else {
        bridge->stats.timing_violations++;
    }

    /* Update state */
    bridge->state.last_interval_ms = interval_ms;
    bridge->state.last_event_time_us = nimcp_platform_time_monotonic_us();
    if (band != bridge->state.current_band) {
        bridge->stats.band_switches++;
        bridge->state.current_band = band;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int biological_timescales_fep_predict_timing(
    biological_timescales_fep_bridge_t* bridge,
    nimcp_oscillation_band_t band,
    float* predicted_interval_ms,
    float* precision
) {
    if (!bridge || !predicted_interval_ms || !precision) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biological_timescales_fep_predict_timing: NULL bridge or output parameters");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Use band period as baseline prediction */
    *predicted_interval_ms = BIO_HZ_TO_PERIOD_MS(nimcp_oscillation_center_freq(band));

    /* Precision from FEP and band */
    *precision = bridge->fep_effects.precision_per_band[band];

    /* Update effects */
    bridge->fep_effects.predicted_interval_ms = *predicted_interval_ms;
    bridge->fep_effects.prediction_precision = *precision;

    /* Update state */
    bridge->state.active_predictions++;
    bridge->state.total_predictions++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

nimcp_oscillation_band_t biological_timescales_fep_select_band(
    biological_timescales_fep_bridge_t* bridge,
    uint32_t fep_level
) {
    if (!bridge) {
        return BIO_OSC_GAMMA; /* Safe default */
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    nimcp_oscillation_band_t band;

    if (bridge->config.map_levels_to_bands && fep_level < FEP_MAX_HIERARCHY_LEVELS) {
        band = (nimcp_oscillation_band_t)bridge->config.level_to_band[fep_level];
    } else {
        band = bridge->fep_effects.selected_band;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return band;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

int biological_timescales_fep_connect_bio_async(biological_timescales_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biological_timescales_fep_connect_bio_async: NULL bridge");
        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0; /* Already connected */
    }

    bridge->base.bio_async_enabled = true;

    NIMCP_LOGGING_INFO("Connected biological timescales FEP bridge");

    return 0;
}

int biological_timescales_fep_disconnect_bio_async(biological_timescales_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biological_timescales_fep_disconnect_bio_async: NULL bridge");
        return -1;
    }

    if (!bridge->base.bio_async_enabled) {
        return 0;
    }

    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Disconnected biological timescales FEP bridge");

    return 0;
}

bool biological_timescales_fep_is_bio_async_connected(
    const biological_timescales_fep_bridge_t* bridge
) {
    return bridge && bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int biological_timescales_fep_get_effects(
    const biological_timescales_fep_bridge_t* bridge,
    biological_timescales_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biological_timescales_fep_get_effects: NULL bridge or effects");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->fep_effects, sizeof(biological_timescales_fep_effects_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int biological_timescales_fep_get_timescales_effects(
    const biological_timescales_fep_bridge_t* bridge,
    fep_biological_timescales_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biological_timescales_fep_get_timescales_effects: NULL bridge or effects");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->timescales_effects, sizeof(fep_biological_timescales_effects_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int biological_timescales_fep_get_stats(
    const biological_timescales_fep_bridge_t* bridge,
    biological_timescales_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biological_timescales_fep_get_stats: NULL bridge or stats");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(stats, &bridge->stats, sizeof(biological_timescales_fep_stats_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int biological_timescales_fep_reset_stats(biological_timescales_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biological_timescales_fep_reset_stats: NULL bridge");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(biological_timescales_fep_stats_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from the knowledge graph
 *
 * WHAT: Retrieves structural self-knowledge about the Biological_Timescales_FEP_Bridge module
 * WHY:  Enables runtime introspection and self-awareness capabilities
 * HOW:  Queries KG for Biological_Timescales_FEP_Bridge entity and logs observations/relations
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge was found, 0 otherwise
 */
int biological_timescales_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Biological_Timescales_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            LOG_DEBUG("Biological_Timescales_FEP_Bridge self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Biological_Timescales_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Biological_Timescales_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
