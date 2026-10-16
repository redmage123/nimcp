/**
 * @file nimcp_pattern_db_fep_bridge.c
 * @brief Implementation of pattern database-FEP bridge
 */

#include "security/nimcp_pattern_db_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(pattern_db_fep_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


int pattern_fep_default_config(pattern_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL config in pattern_fep_default_config");
        return -1;
    }
    config->match_fe_threshold = 5.0f;
    config->surprise_add_threshold = 10.0f;
    config->precision_learning_rate = 0.05f;
    config->enable_adaptive_matching = true;
    config->enable_online_learning = true;
    config->learning_rate = 0.01f;
    return 0;
}

pattern_fep_bridge_t* pattern_fep_create(const pattern_fep_config_t* config,
    nimcp_pattern_db_t pattern_db, fep_system_t* fep_system) {
    if (!pattern_db || !fep_system) {
        NIMCP_LOGGING_ERROR("Pattern FEP bridge: NULL pointers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL pointers in pattern_fep_create");
        return NULL;
    }

    pattern_fep_bridge_t* bridge = (pattern_fep_bridge_t*)nimcp_malloc(sizeof(pattern_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate pattern FEP bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(pattern_fep_bridge_t));
    if (config) bridge->config = *config;
    else pattern_fep_default_config(&bridge->config);

    bridge->fep_system = fep_system;
    bridge->pattern_db = pattern_db;
    if (bridge_base_init(&bridge->base, 0, "pattern_db_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_MUTEX_INIT, "Failed to create pattern FEP bridge mutex");
        nimcp_free(bridge);
        return NULL;
    }

    bridge->state.active = true;
    bridge->state.current_precision = 1.0f;
    NIMCP_LOGGING_INFO("Pattern DB FEP bridge created");
    return bridge;
}

void pattern_fep_destroy(pattern_fep_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->base.bio_async_enabled) pattern_fep_disconnect_bio_async(bridge);
    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int pattern_fep_update(pattern_fep_bridge_t* bridge) {
    if (!bridge || !bridge->state.active) return -1;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    float fe = fep_get_free_energy(bridge->fep_system);
    bridge->fep_effects.match_score = 1.0f - (fe / bridge->config.match_fe_threshold);
    if (bridge->fep_effects.match_score < 0.0f) bridge->fep_effects.match_score = 0.0f;
    bridge->fep_effects.surprise_score = fep_compute_surprise(bridge->fep_system);
    bridge->fep_effects.match_threshold = bridge->state.current_precision;
    bridge->state.update_count++;
    bridge->stats.avg_free_energy = fe;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int pattern_fep_match(pattern_fep_bridge_t* bridge, const char* input,
    nimcp_pattern_match_result_t* result) {
    if (!bridge || !input || !result) return -1;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    nimcp_error_t err = nimcp_pattern_db_match(bridge->pattern_db, input, result);
    if (err != NIMCP_SUCCESS) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pattern_fep_update: validation failed");
        return -1;
    }

    bridge->state.match_count++;
    bridge->stats.total_matches++;
    if (result->matched) {
        bridge->pattern_effects.patterns_matched++;
        bridge->pattern_effects.avg_match_score =
            0.9f * bridge->pattern_effects.avg_match_score + 0.1f * result->threat_score;
    } else {
        bridge->pattern_effects.mismatches++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int pattern_fep_apply_modulation(pattern_fep_bridge_t* bridge) {
    if (!bridge || !bridge->config.enable_adaptive_matching) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    float match_rate = (float)bridge->pattern_effects.patterns_matched /
                      (float)(bridge->state.match_count + 1);
    float target_precision = (match_rate > 0.5f) ? 2.0f : 0.5f;
    float alpha = bridge->config.precision_learning_rate;
    bridge->state.current_precision =
        (1.0f - alpha) * bridge->state.current_precision + alpha * target_precision;
    bridge->stats.current_precision = bridge->state.current_precision;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int pattern_fep_get_effects(const pattern_fep_bridge_t* bridge, pattern_fep_effects_t* effects) {
    if (!bridge || !effects) return -1;
    *effects = bridge->fep_effects;
    return 0;
}

int pattern_fep_get_stats(const pattern_fep_bridge_t* bridge, pattern_fep_stats_t* stats) {
    if (!bridge || !stats) return -1;
    *stats = bridge->stats;
    return 0;
}

int pattern_fep_connect_bio_async(pattern_fep_bridge_t* bridge) {
    if (!bridge || bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECURITY_PATTERN_FEP,
        .module_name = "pattern_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Pattern DB FEP bridge connected to bio-async");
    }
    return 0;
}

int pattern_fep_disconnect_bio_async(pattern_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;
    return 0;
}

bool pattern_fep_is_bio_async_connected(const pattern_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}
