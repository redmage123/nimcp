/**
 * @file nimcp_amygdala_stress_bridge.c
 * @brief Amygdala-Stress/Wellbeing Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-22
 */

#include "core/brain/subcortical/nimcp_amygdala_stress_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(amygdala_stress_bridge, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Normalize threat level enum to [0, 1]
 *
 * WHAT: Convert threat enum to continuous value
 * WHY:  Enable numerical computations
 * HOW:  Map NONE=0, LOW=0.25, MODERATE=0.5, HIGH=0.75, SEVERE=1.0
 */
static float normalize_threat_level(amyg_threat_level_t threat) {
    switch (threat) {
        case AMYG_THREAT_NONE:     return 0.0f;
        case AMYG_THREAT_LOW:      return 0.25f;
        case AMYG_THREAT_MODERATE: return 0.5f;
        case AMYG_THREAT_HIGH:     return 0.75f;
        case AMYG_THREAT_SEVERE:   return 1.0f;
        default:                   return 0.0f;
    }
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

int amygdala_stress_default_config(amygdala_stress_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* Feature enables */
    config->enable_fear_cortisol = true;
    config->enable_anxiety_cortisol = true;
    config->enable_stress_sensitization = true;
    config->enable_wellbeing_buffering = true;
    config->enable_allostatic_load = true;

    /* Sensitivity tuning */
    config->cortisol_sensitivity = 1.0f;
    config->sensitization_sensitivity = 1.0f;
    config->wellbeing_sensitivity = 1.0f;

    /* Thresholds */
    config->stress_mild_threshold = STRESS_MILD_THRESHOLD;
    config->stress_high_threshold = STRESS_HIGH_THRESHOLD;
    config->wellbeing_threshold = WELLBEING_HIGH_THRESHOLD;
    config->allostatic_threshold = ALLOSTATIC_LOAD_THRESHOLD_CHRONIC;

    return 0;
}

amygdala_stress_bridge_t* amygdala_stress_create(const amygdala_stress_config_t* config) {
    /* Allocate structure */
    amygdala_stress_bridge_t* bridge = nimcp_malloc(sizeof(amygdala_stress_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    /* Zero initialize */
    memset(bridge, 0, sizeof(amygdala_stress_bridge_t));

    /* Set config (use defaults if NULL) */
    if (config) {
        bridge->config = *config;
    } else {
        amygdala_stress_default_config(&bridge->config);
    }

    /* Initialize bridge base (allocates mutex, sets module name) */
    if (bridge_base_init(&bridge->base, 0, "amygdala_stress") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created amygdala-stress bridge");
    return bridge;
}

void amygdala_stress_destroy(amygdala_stress_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Cleanup bridge base (disconnects bio-async, destroys+frees mutex) */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed amygdala-stress bridge");
}

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

int amygdala_stress_connect_amygdala(amygdala_stress_bridge_t* bridge, amygdala_t* amygdala) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(amygdala, NIMCP_ERROR_NULL_POINTER, "amygdala is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->amygdala = amygdala;
    bridge->base.system_a = amygdala;
    bridge->amygdala_connected = true;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = bridge->amygdala_connected && bridge->stress_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected amygdala to stress bridge");
    return 0;
}

int amygdala_stress_connect_stress(amygdala_stress_bridge_t* bridge, void* stress) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stress, NIMCP_ERROR_NULL_POINTER, "stress is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stress_system = stress;
    bridge->base.system_b = stress;
    bridge->stress_connected = true;
    bridge->base.system_b_connected = true;
    bridge->base.bridge_active = bridge->amygdala_connected && bridge->stress_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected stress system to bridge");
    return 0;
}

int amygdala_stress_connect_wellbeing(amygdala_stress_bridge_t* bridge, void* wellbeing) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(wellbeing, NIMCP_ERROR_NULL_POINTER, "wellbeing is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->wellbeing_system = wellbeing;
    bridge->wellbeing_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected wellbeing system to bridge");
    return 0;
}

/* ============================================================================
 * Amygdala → Stress Functions
 * ============================================================================ */

int amygdala_stress_apply_fear_cortisol(amygdala_stress_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_fear_cortisol) return 0;
    NIMCP_CHECK_THROW(bridge->amygdala_connected, NIMCP_ERROR_INVALID_STATE, "amygdala not connected");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Query amygdala fear level */
    float fear = amygdala_get_fear_level(bridge->amygdala);
    bridge->amygdala_effects.fear_level = fear;

    /* Compute cortisol contribution */
    float cortisol_contrib = fear * AMYGDALA_FEAR_CORTISOL_FACTOR * bridge->config.cortisol_sensitivity;
    bridge->amygdala_effects.fear_cortisol_contribution = cortisol_contrib;

    /* Add to total cortisol */
    bridge->cortisol_level = nimcp_clamp01(bridge->cortisol_level + cortisol_contrib);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int amygdala_stress_apply_anxiety_cortisol(amygdala_stress_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_anxiety_cortisol) return 0;
    NIMCP_CHECK_THROW(bridge->amygdala_connected, NIMCP_ERROR_INVALID_STATE, "amygdala not connected");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Query amygdala anxiety level */
    float anxiety = amygdala_get_anxiety_level(bridge->amygdala);
    bridge->amygdala_effects.anxiety_level = anxiety;

    /* Compute chronic cortisol contribution */
    float cortisol_contrib = anxiety * AMYGDALA_ANXIETY_CORTISOL_FACTOR * bridge->config.cortisol_sensitivity;
    bridge->amygdala_effects.anxiety_cortisol_contribution = cortisol_contrib;

    /* Add to total cortisol */
    bridge->cortisol_level = nimcp_clamp01(bridge->cortisol_level + cortisol_contrib);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int amygdala_stress_update_allostatic_load(amygdala_stress_bridge_t* bridge, float delta_sec) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_allostatic_load) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Accumulate load from cortisol */
    if (bridge->cortisol_level > bridge->config.stress_mild_threshold) {
        float load_increment = bridge->cortisol_level * ALLOSTATIC_LOAD_ACCUMULATION_RATE * delta_sec;
        bridge->allostatic_load.current_load = nimcp_clamp01(bridge->allostatic_load.current_load + load_increment);

        /* Update peak */
        if (bridge->allostatic_load.current_load > bridge->allostatic_load.peak_load) {
            bridge->allostatic_load.peak_load = bridge->allostatic_load.current_load;
        }

        /* Track stress episodes */
        if (bridge->cortisol_level > bridge->config.stress_high_threshold) {
            bridge->allostatic_load.stress_episodes++;
            bridge->stress_episodes++;
        }
    }

    /* Decay load (slow recovery) */
    float wellbeing_boost = bridge->wellbeing_effects.load_recovery_boost > 0
        ? bridge->wellbeing_effects.load_recovery_boost
        : 1.0f;
    float decay = ALLOSTATIC_LOAD_DECAY_RATE * delta_sec * wellbeing_boost;
    bridge->allostatic_load.current_load = fmaxf(0.0f, bridge->allostatic_load.current_load - decay);

    /* Check chronic burden */
    bridge->allostatic_load.is_chronic_burden =
        bridge->allostatic_load.current_load >= bridge->config.allostatic_threshold;

    /* Track chronic burden duration */
    if (bridge->allostatic_load.is_chronic_burden) {
        bridge->allostatic_load.load_duration_sec += delta_sec;
        bridge->load_accumulator_sec += delta_sec;
    } else {
        bridge->allostatic_load.load_duration_sec = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Stress → Amygdala Functions
 * ============================================================================ */

int amygdala_stress_apply_sensitization(amygdala_stress_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_stress_sensitization) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Determine sensitization based on cortisol level */
    float sensitization = 0.0f;
    if (bridge->cortisol_level < bridge->config.stress_mild_threshold) {
        sensitization = 0.0f;
    } else if (bridge->cortisol_level < STRESS_MODERATE_THRESHOLD) {
        sensitization = STRESS_MILD_SENSITIZATION;
    } else if (bridge->cortisol_level < bridge->config.stress_high_threshold) {
        sensitization = STRESS_MODERATE_SENSITIZATION;
    } else {
        sensitization = STRESS_HIGH_SENSITIZATION;
    }

    /* Apply sensitivity multiplier */
    sensitization *= bridge->config.sensitization_sensitivity;

    /* Reduce sensitization by wellbeing buffer */
    sensitization = fmaxf(0.0f, sensitization + bridge->wellbeing_buffer);

    /* Store effects */
    bridge->stress_effects.cortisol_level = bridge->cortisol_level;
    bridge->stress_effects.sensitization_factor = sensitization;
    bridge->stress_effects.fear_acquisition_boost = sensitization * 0.5f;  /* 50% of sensitization */
    bridge->stress_effects.fear_extinction_impairment = sensitization * 0.4f;  /* 40% of sensitization */
    bridge->stress_effects.anxiety_elevation = sensitization * 0.3f;  /* 30% of sensitization */
    bridge->stress_effects.is_chronic_stress = bridge->cortisol_level >= bridge->config.stress_high_threshold;

    /* Track stress duration */
    if (bridge->stress_effects.is_chronic_stress) {
        /* Duration tracking handled in update function */
    } else {
        bridge->stress_effects.stress_duration_sec = 0.0f;
    }

    bridge->amygdala_sensitization = sensitization;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float amygdala_stress_get_effective_reactivity(const amygdala_stress_bridge_t* bridge) {
    if (!bridge) return 1.0f;

    /* Base reactivity is 1.0, sensitization adds to it */
    return 1.0f + bridge->amygdala_sensitization;
}

/* ============================================================================
 * Wellbeing → Amygdala Functions
 * ============================================================================ */

int amygdala_stress_apply_wellbeing_buffer(amygdala_stress_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_wellbeing_buffering) return 0;
    if (!bridge->wellbeing_connected) {
        /* No wellbeing system connected, no buffering */
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Use externally-set wellbeing level if available, otherwise default to 0.5 */
    /* NOTE: In a real implementation, query wellbeing system here if level is 0 */
    float wellbeing_level = bridge->wellbeing_effects.wellbeing_level;
    if (wellbeing_level <= 0.0f) {
        wellbeing_level = 0.5f;  /* Placeholder default */
        bridge->wellbeing_effects.wellbeing_level = wellbeing_level;
    }
    bridge->wellbeing_effects.is_high_wellbeing = wellbeing_level >= bridge->config.wellbeing_threshold;

    if (bridge->wellbeing_effects.is_high_wellbeing) {
        /* High wellbeing reduces sensitization */
        float buffer = WELLBEING_BUFFER_SENSITIZATION * bridge->config.wellbeing_sensitivity;
        bridge->wellbeing_effects.reactivity_buffer = -buffer;  /* Negative = reduction */
        bridge->wellbeing_effects.sensitization_reduction = -buffer;
        bridge->wellbeing_effects.extinction_enhancement = WELLBEING_EXTINCTION_BOOST;
        bridge->wellbeing_effects.load_recovery_boost = WELLBEING_LOAD_RECOVERY_BOOST;

        bridge->wellbeing_buffer = buffer;  /* Already negative from constant, reduces sensitization */
        bridge->wellbeing_interventions++;
    } else {
        /* Low/moderate wellbeing - no special buffering */
        bridge->wellbeing_effects.reactivity_buffer = 0.0f;
        bridge->wellbeing_effects.sensitization_reduction = 0.0f;
        bridge->wellbeing_effects.extinction_enhancement = 0.0f;
        bridge->wellbeing_effects.load_recovery_boost = 1.0f;
        bridge->wellbeing_buffer = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float amygdala_stress_get_extinction_boost(const amygdala_stress_bridge_t* bridge) {
    if (!bridge) return 1.0f;

    /* Base extinction rate is 1.0, wellbeing can boost it */
    return 1.0f + bridge->wellbeing_effects.extinction_enhancement;
}

/* ============================================================================
 * Update Function
 * ============================================================================ */

int amygdala_stress_update(amygdala_stress_bridge_t* bridge, uint64_t delta_ms) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    float delta_sec = delta_ms / 1000.0f;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Decay cortisol */
    bridge->cortisol_level = fmaxf(0.0f, bridge->cortisol_level - CORTISOL_DECAY_RATE * delta_sec);

    /* Track stress duration if chronic */
    if (bridge->cortisol_level >= bridge->config.stress_high_threshold) {
        bridge->stress_accumulator_sec += delta_sec;
        bridge->stress_effects.stress_duration_sec += delta_sec;
    } else {
        bridge->stress_effects.stress_duration_sec = 0.0f;
    }

    bridge->last_update_time_ms += delta_ms;
    bridge->total_updates++;
    bridge->base.total_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Apply all pathways */
    int ret;

    /* Amygdala → Stress */
    ret = amygdala_stress_apply_fear_cortisol(bridge);
    if (ret != 0) return ret;

    ret = amygdala_stress_apply_anxiety_cortisol(bridge);
    if (ret != 0) return ret;

    /* Update allostatic load */
    ret = amygdala_stress_update_allostatic_load(bridge, delta_sec);
    if (ret != 0) return ret;

    /* Wellbeing → Amygdala */
    ret = amygdala_stress_apply_wellbeing_buffer(bridge);
    if (ret != 0) return ret;

    /* Stress → Amygdala */
    ret = amygdala_stress_apply_sensitization(bridge);
    if (ret != 0) return ret;

    return 0;
}

/* ============================================================================
 * Query Functions
 * ============================================================================ */

float amygdala_stress_get_cortisol(const amygdala_stress_bridge_t* bridge) {
    return bridge ? bridge->cortisol_level : 0.0f;
}

float amygdala_stress_get_allostatic_load(const amygdala_stress_bridge_t* bridge) {
    return bridge ? bridge->allostatic_load.current_load : 0.0f;
}

float amygdala_stress_get_sensitization(const amygdala_stress_bridge_t* bridge) {
    return bridge ? bridge->amygdala_sensitization : 0.0f;
}

bool amygdala_stress_is_chronic_burden(const amygdala_stress_bridge_t* bridge) {
    return bridge ? bridge->allostatic_load.is_chronic_burden : false;
}

int amygdala_stress_get_effects(
    const amygdala_stress_bridge_t* bridge,
    amygdala_stress_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(effects, NIMCP_ERROR_NULL_POINTER, "effects is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->amygdala_effects;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int amygdala_stress_get_stress_effects(
    const amygdala_stress_bridge_t* bridge,
    stress_amygdala_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(effects, NIMCP_ERROR_NULL_POINTER, "effects is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->stress_effects;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int amygdala_stress_get_wellbeing_effects(
    const amygdala_stress_bridge_t* bridge,
    wellbeing_amygdala_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(effects, NIMCP_ERROR_NULL_POINTER, "effects is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->wellbeing_effects;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int amygdala_stress_get_allostatic_state(
    const amygdala_stress_bridge_t* bridge,
    allostatic_load_state_t* state
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->allostatic_load;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Functions
 * ============================================================================ */

int amygdala_stress_connect_bio_async(amygdala_stress_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;  /* Already connected */

    bio_module_info_t info = {
        .module_id = BIO_MODULE_AMYGDALA_STRESS,
        .module_name = "amygdala_stress_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
        return NIMCP_ERROR_OPERATION_FAILED;
    }
}

int amygdala_stress_disconnect_bio_async(amygdala_stress_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

bool amygdala_stress_is_bio_async_connected(const amygdala_stress_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Statistics Functions
 * ============================================================================ */

int amygdala_stress_get_statistics(
    const amygdala_stress_bridge_t* bridge,
    uint64_t* total_updates,
    uint32_t* stress_episodes,
    uint32_t* wellbeing_interventions,
    uint32_t* chronic_burden_episodes
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    if (total_updates) *total_updates = bridge->total_updates;
    if (stress_episodes) *stress_episodes = bridge->stress_episodes;
    if (wellbeing_interventions) *wellbeing_interventions = bridge->wellbeing_interventions;
    if (chronic_burden_episodes) *chronic_burden_episodes = bridge->chronic_burden_episodes;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}
