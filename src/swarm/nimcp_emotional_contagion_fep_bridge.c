/**
 * @file nimcp_emotional_contagion_fep_bridge.c
 */

#include "swarm/nimcp_emotional_contagion_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(emotional_contagion_fep_bridge)

#define LOG_MODULE "EMOTIONAL_CONTAGION_FEP_BRIDGE"


void emotional_contagion_fep_default_config(emotional_contagion_fep_config_t* config) {
    if (!config) return;
    config->intensity_precision_weight = 0.75f;
    config->contagion_inference_gain = 1.2f;
    config->resistance_prior_strength = 0.6f;
    config->enable_affective_inference = true;
}

emotional_contagion_fep_bridge_t* emotional_contagion_fep_create(const emotional_contagion_fep_config_t* config, emotional_contagion_t* contagion_system, fep_system_t* fep_system) {
    if (!contagion_system || !fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotional_contagion_fep_create: required parameter is NULL (contagion_system, fep_system)");
        return NULL;
    }
    emotional_contagion_fep_bridge_t* bridge = (emotional_contagion_fep_bridge_t*)nimcp_malloc(sizeof(emotional_contagion_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "emotional_contagion_fep_create: allocation failed");

        return NULL;

    }
    memset(bridge, 0, sizeof(emotional_contagion_fep_bridge_t));
    if (config) bridge->config = *config;
    else emotional_contagion_fep_default_config(&bridge->config);
    bridge->fep_system = fep_system;
    bridge->contagion_system = contagion_system;
    if (bridge_base_init(&bridge->base, 0, "emotional_contagion_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }
    NIMCP_LOGGING_INFO("Created %s bridge", "emotional_contagion_fep");
    return bridge;
}

void emotional_contagion_fep_destroy(emotional_contagion_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "emotional_contagion_fep");
    if (bridge->base.bio_async_enabled) emotional_contagion_fep_disconnect_bio_async(bridge);
    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int emotional_contagion_fep_update(emotional_contagion_fep_bridge_t* bridge) {
    if (!bridge) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    float fe = fep_get_free_energy(bridge->fep_system);
    // Compute precision as inverse of free energy (high FE = low precision)
    float precision = 1.0f / (1.0f + fe);
    emotion_type_t dominant_emotion;
    float avg_intensity;
    emotional_contagion_get_dominant_emotion(bridge->contagion_system, &dominant_emotion, &avg_intensity);
    float coherence = 0.6f;
    emotional_contagion_get_coherence(bridge->contagion_system, &coherence);
    bridge->fep_effects.intensity_modulation = precision * bridge->config.intensity_precision_weight - 0.5f;
    bridge->fep_effects.contagion_rate_adjustment = fmaxf(0.5f, 1.0f + fe * 0.2f);
    bridge->fep_effects.susceptibility_adjustment = -fe * 0.15f;
    bridge->contagion_effects.precision_from_emotion = 0.4f + avg_intensity * bridge->config.intensity_precision_weight;
    // Use generic emotion modulation instead of specific enum values
    bridge->contagion_effects.learning_modulation = 0.8f + avg_intensity * 0.2f;
    bridge->contagion_effects.action_bias_from_emotion = avg_intensity * 0.5f;
    if (bridge->state.last_dominant_emotion != dominant_emotion) {
        bridge->stats.emotional_transitions++;
        bridge->state.last_dominant_emotion = dominant_emotion;
    }
    bridge->state.last_intensity = avg_intensity;
    bridge->state.last_update_time = nimcp_platform_time_monotonic_ms();
    bridge->stats.total_updates++;
    bridge->stats.avg_collective_intensity = (bridge->stats.avg_collective_intensity * (bridge->stats.total_updates - 1) + avg_intensity) / bridge->stats.total_updates;
    bridge->stats.avg_emotional_fe = (bridge->stats.avg_emotional_fe * (bridge->stats.total_updates - 1) + fe) / bridge->stats.total_updates;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotional_contagion_fep_apply_modulation(emotional_contagion_fep_bridge_t* bridge) {
    if (!bridge) return -1;
    return 0;
}

int emotional_contagion_fep_get_effects(const emotional_contagion_fep_bridge_t* bridge, emotional_contagion_fep_effects_t* effects) {
    if (!bridge || !effects) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->fep_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotional_contagion_fep_get_contagion_effects(const emotional_contagion_fep_bridge_t* bridge, fep_emotional_contagion_effects_t* effects) {
    if (!bridge || !effects) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->contagion_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotional_contagion_fep_get_stats(const emotional_contagion_fep_bridge_t* bridge, emotional_contagion_fep_stats_t* stats) {
    if (!bridge || !stats) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotional_contagion_fep_connect_bio_async(emotional_contagion_fep_bridge_t* bridge) {
    if (!bridge || bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = { .module_id = BIO_MODULE_FEP_EMOTIONAL_CONTAGION, .module_name = "emotional_contagion_fep_bridge", .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL, .user_data = bridge };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) bridge->base.bio_async_enabled = true;
    return 0;
}

int emotional_contagion_fep_disconnect_bio_async(emotional_contagion_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    if (bridge->base.bio_ctx) bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool emotional_contagion_fep_is_bio_async_connected(const emotional_contagion_fep_bridge_t* bridge) {
    return bridge && bridge->base.bio_async_enabled;
}
