/**
 * @file nimcp_feature_extractor_fep_bridge.c
 * @brief Free Energy Principle - Feature Extractor Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "middleware/features/nimcp_feature_extractor_fep_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(feature_extractor_fep_bridge)

int feature_extractor_fep_bridge_default_config(
    feature_extractor_fep_config_t* config
) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->enable_hierarchical_selection = true;
    config->enable_precision_gating = true;
    config->enable_prediction_gating = true;
    config->enable_entropy_feedback = true;
    config->enable_oscillation_state = true;

    config->hierarchy_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->precision_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->entropy_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->oscillation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    return 0;
}

feature_extractor_fep_bridge_t* feature_extractor_fep_bridge_create(
    const feature_extractor_fep_config_t* config
) {
    feature_extractor_fep_bridge_t* bridge = (feature_extractor_fep_bridge_t*)
        nimcp_calloc(1, sizeof(feature_extractor_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate feature-FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    feature_extractor_fep_config_t default_cfg;
    if (!config) {
        feature_extractor_fep_bridge_default_config(&default_cfg);
        config = &default_cfg;
    }
    bridge->config = *config;

    bridge->state.fep_hierarchy_level = 0;
    bridge->state.fep_precision = 0.5f;
    bridge->effects.active_hierarchy_level = 0;
    bridge->effects.enabled_feature_count = FEP_PRECISION_MED_FEATURE_COUNT;
    bridge->effects.extract_rate_features = true;

    if (bridge_base_init(&bridge->base, 0, "feature_extractor_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        feature_extractor_fep_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feature_extractor_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Feature-FEP bridge created");
    return bridge;
}

void feature_extractor_fep_bridge_destroy(
    feature_extractor_fep_bridge_t* bridge
) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        feature_extractor_fep_bridge_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Feature-FEP bridge destroyed");
}

int feature_extractor_fep_bridge_connect_extractor(
    feature_extractor_fep_bridge_t* bridge,
    feature_extractor_t extractor
) {
    if (!bridge || !extractor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feature_extractor_fep_bridge_connect_extractor: required parameter is NULL (bridge, extractor)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->feature_extractor = extractor;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Feature extractor connected to FEP bridge");
    return 0;
}

int feature_extractor_fep_bridge_connect_fep(
    feature_extractor_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    if (!bridge || !fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feature_extractor_fep_bridge_connect_fep: required parameter is NULL (bridge, fep)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("FEP system connected to feature bridge");
    return 0;
}

int feature_extractor_fep_bridge_disconnect(
    feature_extractor_fep_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->feature_extractor = NULL;
    bridge->fep_system = NULL;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Feature-FEP bridge disconnected");
    return 0;
}

int feature_extractor_fep_select_hierarchical_features(
    feature_extractor_fep_bridge_t* bridge,
    uint32_t hierarchy_level
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_hierarchical_selection) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->state.fep_hierarchy_level = hierarchy_level;
    bridge->effects.active_hierarchy_level = hierarchy_level;
    bridge->effects.extract_rate_features = (hierarchy_level >= FEP_HIERARCHY_LEVEL_RATE);
    bridge->effects.extract_temporal_features = (hierarchy_level >= FEP_HIERARCHY_LEVEL_TEMPORAL);
    bridge->effects.extract_population_features = (hierarchy_level >= FEP_HIERARCHY_LEVEL_POPULATION);
    bridge->effects.extract_oscillation_features = (hierarchy_level >= FEP_HIERARCHY_LEVEL_OSCILLATION);

    bridge->stats.hierarchical_adjustments++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Hierarchical features selected: level %u", hierarchy_level);
    return 0;
}

int feature_extractor_fep_gate_by_precision(
    feature_extractor_fep_bridge_t* bridge,
    float precision
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_precision_gating) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->state.fep_precision = precision;

    if (precision > 0.7f) {
        bridge->effects.enabled_feature_count = FEP_PRECISION_HIGH_FEATURE_COUNT;
    } else if (precision > 0.4f) {
        bridge->effects.enabled_feature_count = FEP_PRECISION_MED_FEATURE_COUNT;
    } else {
        bridge->effects.enabled_feature_count = FEP_PRECISION_LOW_FEATURE_COUNT;
    }

    bridge->stats.precision_updates++;
    bridge->stats.avg_precision =
        (bridge->stats.avg_precision * (bridge->stats.precision_updates - 1) +
         precision) / bridge->stats.precision_updates;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Precision gating: %.3f → %u features",
                        precision, bridge->effects.enabled_feature_count);
    return 0;
}

int feature_extractor_fep_set_expected_features(
    feature_extractor_fep_bridge_t* bridge,
    float expected_rate
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_prediction_gating) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->effects.expected_rate = expected_rate;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Expected rate set: %.3f Hz", expected_rate);
    return 0;
}

int feature_extractor_fep_report_features(
    feature_extractor_fep_bridge_t* bridge,
    const middleware_features_t* features
) {
    if (!bridge || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feature_extractor_fep_report_features: required parameter is NULL (bridge, features)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->state.current_features = *features;
    bridge->stats.feature_extractions++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Features reported to FEP");
    return 0;
}

int feature_extractor_fep_update_uncertainty_from_entropy(
    feature_extractor_fep_bridge_t* bridge,
    float entropy
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_entropy_feedback) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->state.feature_entropy = entropy;

    float uncertainty;
    if (entropy < ENTROPY_LOW_UNCERTAINTY) {
        uncertainty = 0.2f;
    } else if (entropy > ENTROPY_HIGH_UNCERTAINTY) {
        uncertainty = 0.9f;
    } else {
        uncertainty = 0.5f;
    }

    bridge->state.observation_uncertainty = uncertainty;
    bridge->stats.avg_entropy =
        (bridge->stats.avg_entropy * bridge->stats.feature_extractions + entropy) /
        (bridge->stats.feature_extractions + 1);

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Entropy → uncertainty: %.3f → %.3f", entropy, uncertainty);
    return 0;
}

int feature_extractor_fep_infer_state_from_oscillations(
    feature_extractor_fep_bridge_t* bridge,
    float gamma_power,
    float beta_power,
    float alpha_power
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_oscillation_state) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    float inferred_precision;
    if (gamma_power > GAMMA_HIGH_PRECISION_THRESHOLD) {
        inferred_precision = 0.9f;
    } else if (beta_power > BETA_MED_PRECISION_THRESHOLD) {
        inferred_precision = 0.6f;
    } else if (alpha_power > ALPHA_LOW_PRECISION_THRESHOLD) {
        inferred_precision = 0.3f;
    } else {
        inferred_precision = 0.5f;
    }

    bridge->state.inferred_precision = inferred_precision;
    bridge->stats.oscillation_state_updates++;
    bridge->stats.avg_gamma_power =
        (bridge->stats.avg_gamma_power * (bridge->stats.oscillation_state_updates - 1) +
         gamma_power) / bridge->stats.oscillation_state_updates;
    bridge->stats.avg_alpha_power =
        (bridge->stats.avg_alpha_power * (bridge->stats.oscillation_state_updates - 1) +
         alpha_power) / bridge->stats.oscillation_state_updates;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Oscillations → precision: γ=%.3f,β=%.3f,α=%.3f → %.3f",
                        gamma_power, beta_power, alpha_power, inferred_precision);
    return 0;
}

int feature_extractor_fep_bridge_update(
    feature_extractor_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    return 0;
}

int feature_extractor_fep_bridge_get_state(
    const feature_extractor_fep_bridge_t* bridge,
    feature_extractor_fep_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feature_extractor_fep_bridge_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_platform_mutex_lock((void*)bridge->base.mutex);
    *state = bridge->state;
    nimcp_platform_mutex_unlock((void*)bridge->base.mutex);

    return 0;
}

int feature_extractor_fep_bridge_get_stats(
    const feature_extractor_fep_bridge_t* bridge,
    feature_extractor_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feature_extractor_fep_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    nimcp_platform_mutex_lock((void*)bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock((void*)bridge->base.mutex);

    return 0;
}

int feature_extractor_fep_bridge_connect_bio_async(
    feature_extractor_fep_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_FEATURE_EXTRACTOR_BRIDGE,
        .module_name = "feature_extractor_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    return 0;
}

int feature_extractor_fep_bridge_disconnect_bio_async(
    feature_extractor_fep_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

bool feature_extractor_fep_bridge_is_bio_async_connected(
    const feature_extractor_fep_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
