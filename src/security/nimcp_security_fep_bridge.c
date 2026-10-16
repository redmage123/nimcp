/**
 * @file nimcp_security_fep_bridge.c
 * @brief Implementation of security-FEP bridge
 */

#include "security/nimcp_security_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_fep_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


int security_fep_default_config(security_fep_config_t* config) {
    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    config->threat_fe_threshold = 10.0f;
    config->skepticism_threshold = 5.0f;
    config->precision_learning_rate = 0.05f;
    config->enable_adaptive_security = true;
    config->enable_online_learning = true;
    config->learning_rate = 0.01f;
    return 0;
}

security_fep_bridge_t* security_fep_create(const security_fep_config_t* config,
    fep_system_t* fep_system) {
    if (!fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_fep_create: fep_system is NULL");
        return NULL;
    }

    security_fep_bridge_t* bridge = (security_fep_bridge_t*)nimcp_malloc(sizeof(security_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_fep_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(security_fep_bridge_t));
    if (config) bridge->config = *config;
    else security_fep_default_config(&bridge->config);

    bridge->fep_system = fep_system;
    if (bridge_base_init(&bridge->base, 0, "security_fep") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "security_fep_create: failed to init bridge base");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_fep_create: mutex creation failed");
        nimcp_free(bridge);
        return NULL;
    }

    bridge->state.active = true;
    bridge->state.current_precision = 1.0f;
    NIMCP_LOGGING_INFO("Security FEP bridge created");
    return bridge;
}

void security_fep_destroy(security_fep_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->base.bio_async_enabled) security_fep_disconnect_bio_async(bridge);
    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int security_fep_update(security_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->state.active, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or inactive");

    nimcp_platform_mutex_lock(bridge->base.mutex);
    float fe = fep_get_free_energy(bridge->fep_system);
    float surprise = fep_compute_surprise(bridge->fep_system);

    // Map free energy to threat level
    bridge->fep_effects.threat_level_score = fe / bridge->config.threat_fe_threshold;
    if (bridge->fep_effects.threat_level_score > 1.0f)
        bridge->fep_effects.threat_level_score = 1.0f;

    // Map surprise to skepticism
    bridge->fep_effects.skepticism_score = surprise / bridge->config.skepticism_threshold;
    if (bridge->fep_effects.skepticism_score > 1.0f)
        bridge->fep_effects.skepticism_score = 1.0f;

    // Precision-based input strictness
    bridge->fep_effects.input_strictness = bridge->state.current_precision;

    // Recommend threat level based on FE
    if (fe < 2.0f) {
        bridge->fep_effects.recommended_threat_level = NIMCP_THREAT_NONE;
    } else if (fe < 5.0f) {
        bridge->fep_effects.recommended_threat_level = NIMCP_THREAT_LOW;
    } else if (fe < 10.0f) {
        bridge->fep_effects.recommended_threat_level = NIMCP_THREAT_MEDIUM;
    } else if (fe < 20.0f) {
        bridge->fep_effects.recommended_threat_level = NIMCP_THREAT_HIGH;
    } else {
        bridge->fep_effects.recommended_threat_level = NIMCP_THREAT_CRITICAL;
    }

    bridge->state.update_count++;
    bridge->stats.avg_free_energy = fe;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_fep_validate_input(security_fep_bridge_t* bridge, const char* input,
    nimcp_input_validation_t* result, nimcp_threat_level_t* threat) {
    NIMCP_FEP_CHECK_THROW(bridge && input && result && threat, NIMCP_ERROR_NULL_POINTER, "NULL parameter in security_fep_validate_input");

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Standard security validation
    *result = nimcp_security_validate_input(input, 1024, threat);

    // Enhance with FEP-based skepticism
    if (bridge->fep_effects.skepticism_score > 0.7f) {
        // High skepticism → stricter validation
        if (*result == NIMCP_INPUT_VALID && *threat == NIMCP_THREAT_NONE) {
            *threat = NIMCP_THREAT_LOW;  // Elevate to low threat
        }
    }

    // Update FEP if online learning enabled
    if (bridge->config.enable_online_learning) {
        float input_features[16] = {0};
        // Simple feature: threat level
        input_features[0] = (float)(*threat) / (float)NIMCP_THREAT_CRITICAL;
        fep_process_observation(bridge->fep_system, input_features, 16);
    }

    bridge->state.validation_count++;
    bridge->stats.total_validations++;
    bridge->security_effects.inputs_validated++;

    if (*threat > NIMCP_THREAT_NONE) {
        bridge->stats.threats_found++;
        bridge->security_effects.threats_detected++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_fep_apply_modulation(security_fep_bridge_t* bridge) {
    if (!bridge || !bridge->config.enable_adaptive_security) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    float threat_rate = (float)bridge->security_effects.threats_detected /
                       (float)(bridge->state.validation_count + 1);

    float target_precision = (threat_rate > 0.2f) ? 2.0f : 0.5f;
    float alpha = bridge->config.precision_learning_rate;
    bridge->state.current_precision =
        (1.0f - alpha) * bridge->state.current_precision + alpha * target_precision;
    bridge->stats.current_precision = bridge->state.current_precision;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_fep_report_threat(security_fep_bridge_t* bridge,
    nimcp_threat_level_t level) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->security_effects.threats_detected++;
    float level_normalized = (float)level / (float)NIMCP_THREAT_CRITICAL;
    bridge->security_effects.avg_threat_level =
        0.9f * bridge->security_effects.avg_threat_level + 0.1f * level_normalized;

    // High threat → increase precision
    if (level >= NIMCP_THREAT_HIGH) {
        fep_update_precision(bridge->fep_system);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_fep_get_stats(const security_fep_bridge_t* bridge,
    security_fep_stats_t* stats) {
    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    *stats = bridge->stats;
    return 0;
}

int security_fep_get_effects(const security_fep_bridge_t* bridge,
    security_fep_effects_t* effects) {
    NIMCP_FEP_CHECK_THROW(bridge && effects, NIMCP_ERROR_NULL_POINTER, "bridge or effects is NULL");
    *effects = bridge->fep_effects;
    return 0;
}

int security_fep_connect_bio_async(security_fep_bridge_t* bridge) {
    if (!bridge || bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECURITY_CORE_FEP,
        .module_name = "security_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Security FEP bridge connected to bio-async");
    }
    return 0;
}

int security_fep_disconnect_bio_async(security_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;
    return 0;
}

bool security_fep_is_bio_async_connected(const security_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}
