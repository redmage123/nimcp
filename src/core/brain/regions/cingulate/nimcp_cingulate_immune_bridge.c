/**
 * @file nimcp_cingulate_immune_bridge.c
 * @brief Cingulate Cortex-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-30
 *
 * WHAT: Bidirectional integration between brain immune system and cingulate cortex
 * WHY:  Creates cognitive-immune feedback loops for realistic error/conflict modeling
 * HOW:  Cytokines modulate error/conflict detection; cognitive stress modulates immunity
 */

#include "core/brain/regions/cingulate/nimcp_cingulate_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE "CINGULATE_IMMUNE_BRIDGE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "constants/nimcp_threshold_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(cingulate_immune_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * Internal Structure
 *===========================================================================*/

struct cingulate_immune_bridge {
    bridge_base_t base;                     /**< MUST be first: base infrastructure */

    /* Configuration */
    cingulate_immune_config_t config;

    /* Connected systems */
    cingulate_adapter_t* cingulate;
    brain_immune_system_t* immune;

    /* Current state */
    cingulate_immune_state_t state;

    /* Cached effects */
    cingulate_cytokine_effects_t cytokine_effects;
    cingulate_immune_effects_t immune_effects;

    /* Statistics */
    cingulate_immune_stats_t stats;

    /* Error/conflict tracking */
    float recent_error_sum;
    uint32_t recent_error_count;
    float recent_conflict_sum;
    uint32_t recent_conflict_count;

    /* Bio-async connected flag */
    bool bio_async_connected;

    /* Thread safety mutex */
    nimcp_platform_mutex_t mutex;

    /* Timing */
    uint64_t last_update_ms;

    /* Accumulators for averaging */
    float ern_mod_sum;
    float conflict_mod_sum;
    float immune_factor_sum;
};

/*=============================================================================
 * Default Configuration
 *===========================================================================*/

void cingulate_immune_default_config(cingulate_immune_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(cingulate_immune_config_t));

    config->enable_immune_to_cingulate = true;
    config->enable_cingulate_to_immune = true;
    config->enable_error_tracking = true;
    config->enable_conflict_tracking = true;
    config->enable_emotional_modulation = true;
    config->cytokine_sensitivity = 1.0f;
    config->error_coupling = 1.0f;
    config->conflict_coupling = 1.0f;
    config->emotion_coupling = 1.0f;
    config->enable_bio_async = true;
    config->update_interval_ms = CINGULATE_IMMUNE_UPDATE_INTERVAL_MS;
    config->chronic_error_threshold = ERROR_RATE_CHRONIC_THRESHOLD;
    config->chronic_conflict_threshold = CONFLICT_CHRONIC_THRESHOLD;
}

/*=============================================================================
 * Utility Functions
 *===========================================================================*/

float cingulate_immune_compute_inflammation_factor(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_NONE_CINGULATE_FACTOR;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_LOCAL_CINGULATE_FACTOR;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_REGIONAL_CINGULATE_FACTOR;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_SYSTEMIC_CINGULATE_FACTOR;
        case INFLAMMATION_STORM:    return INFLAMMATION_STORM_CINGULATE_FACTOR;
        default:                    return 1.0f;
    }
}

float cingulate_immune_compute_error_immune(float error_rate) {
    if (error_rate < ERROR_RATE_LOW_THRESHOLD) {
        return 1.0f;  /* Normal */
    } else if (error_rate < ERROR_RATE_HIGH_THRESHOLD) {
        /* Linear increase from 1.0 to 1.15 */
        float ratio = (error_rate - ERROR_RATE_LOW_THRESHOLD) /
                      (ERROR_RATE_HIGH_THRESHOLD - ERROR_RATE_LOW_THRESHOLD);
        return 1.0f + ratio * ERROR_RATE_IMMUNE_MODULATION;
    } else {
        /* High error rate: 1.15 to 1.30 */
        float ratio = (error_rate - ERROR_RATE_HIGH_THRESHOLD) /
                      (ERROR_RATE_CHRONIC_THRESHOLD - ERROR_RATE_HIGH_THRESHOLD);
        if (ratio > 1.0f) ratio = 1.0f;
        return 1.15f + ratio * 0.15f;
    }
}

float cingulate_immune_compute_conflict_immune(float conflict_level) {
    if (conflict_level < CONFLICT_LOW_THRESHOLD) {
        return 1.0f;  /* Normal */
    } else if (conflict_level < CONFLICT_HIGH_THRESHOLD) {
        /* Linear increase from 1.0 to 1.20 */
        float ratio = (conflict_level - CONFLICT_LOW_THRESHOLD) /
                      (CONFLICT_HIGH_THRESHOLD - CONFLICT_LOW_THRESHOLD);
        return 1.0f + ratio * CONFLICT_IMMUNE_MODULATION;
    } else {
        /* High conflict: 1.20 to 1.35 */
        float ratio = (conflict_level - CONFLICT_HIGH_THRESHOLD) /
                      (CONFLICT_CHRONIC_THRESHOLD - CONFLICT_HIGH_THRESHOLD);
        if (ratio > 1.0f) ratio = 1.0f;
        return 1.20f + ratio * 0.15f;
    }
}

float cingulate_immune_compute_emotion_immune(float valence) {
    /* Map valence [-1, +1] to immune factor [1.25, 0.85] */
    /* Negative emotions (valence < 0) -> pro-inflammatory (> 1.0) */
    /* Positive emotions (valence > 0) -> anti-inflammatory (< 1.0) */
    if (valence < -0.1f) {
        /* Negative: interpolate from 1.0 to 1.25 as valence goes -0.1 to -1 */
        float ratio = (-valence - 0.1f) / 0.9f;
        if (ratio > 1.0f) ratio = 1.0f;
        return 1.0f + ratio * (EMOTION_NEGATIVE_IMMUNE_FACTOR - 1.0f);
    } else if (valence > 0.1f) {
        /* Positive: interpolate from 1.0 to 0.85 as valence goes 0.1 to 1 */
        float ratio = (valence - 0.1f) / 0.9f;
        if (ratio > 1.0f) ratio = 1.0f;
        return 1.0f - ratio * (1.0f - EMOTION_POSITIVE_IMMUNE_FACTOR);
    }
    /* Neutral */
    return EMOTION_NEUTRAL_IMMUNE_FACTOR;
}

const char* cingulate_immune_state_to_string(cingulate_immune_state_t state) {
    switch (state) {
        case CINGULATE_IMMUNE_NORMAL:           return "NORMAL";
        case CINGULATE_IMMUNE_MILD_IMPAIRMENT:  return "MILD_IMPAIRMENT";
        case CINGULATE_IMMUNE_MODERATE_IMPAIRMENT: return "MODERATE_IMPAIRMENT";
        case CINGULATE_IMMUNE_SEVERE_IMPAIRMENT: return "SEVERE_IMPAIRMENT";
        case CINGULATE_IMMUNE_STRESSED:         return "STRESSED";
        case CINGULATE_IMMUNE_RECOVERING:       return "RECOVERING";
        default:                                return "UNKNOWN";
    }
}

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

cingulate_immune_bridge_t cingulate_immune_create(
    const cingulate_immune_config_t* config,
    cingulate_adapter_t* cingulate,
    brain_immune_system_t* immune
) {
    /* Guard: Null checks */
    if (!cingulate || !immune) {
        NIMCP_LOGGING_ERROR("Null cingulate or immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_immune_create: required parameter is NULL (cingulate, immune)");
        return NULL;
    }

    /* Allocate bridge */
    cingulate_immune_bridge_t bridge = nimcp_malloc(sizeof(struct cingulate_immune_bridge));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate cingulate-immune bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(struct cingulate_immune_bridge));

    /* Copy configuration or use defaults */
    if (config) {
        bridge->config = *config;
    } else {
        cingulate_immune_default_config(&bridge->config);
    }

    /* Store references */
    bridge->cingulate = cingulate;
    bridge->immune = immune;
    bridge->state = CINGULATE_IMMUNE_NORMAL;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "cingulate_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create bridge mutex");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cingulate_immune_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize effects to neutral */
    bridge->cytokine_effects.ern_modulation = 1.0f;
    bridge->cytokine_effects.error_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->cytokine_effects.n2_modulation = 1.0f;
    bridge->cytokine_effects.conflict_threshold = NIMCP_CONFLICT_THRESHOLD;
    bridge->cytokine_effects.control_capacity = 1.0f;
    bridge->cytokine_effects.emotional_regulation = 1.0f;
    bridge->cytokine_effects.self_referential_capacity = 1.0f;
    bridge->cytokine_effects.dmn_connectivity = 1.0f;
    bridge->cytokine_effects.combined_modulation = 1.0f;

    bridge->immune_effects.error_immune_factor = 1.0f;
    bridge->immune_effects.conflict_immune_factor = 1.0f;
    bridge->immune_effects.emotion_immune_factor = 1.0f;
    bridge->immune_effects.combined_immune_factor = 1.0f;

    NIMCP_LOGGING_INFO("Cingulate-immune bridge created successfully");
    NIMCP_LOGGING_INFO("  Immune->Cingulate: %s",
        bridge->config.enable_immune_to_cingulate ? "ENABLED" : "DISABLED");
    NIMCP_LOGGING_INFO("  Cingulate->Immune: %s",
        bridge->config.enable_cingulate_to_immune ? "ENABLED" : "DISABLED");

    return bridge;
}

void cingulate_immune_destroy(cingulate_immune_bridge_t bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->bio_async_connected) {
        cingulate_immune_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_DEBUG("Cingulate-immune bridge destroyed");
}

/*=============================================================================
 * Immune -> Cingulate Update
 *===========================================================================*/

int cingulate_immune_update_immune_to_cingulate(
    cingulate_immune_bridge_t bridge,
    cingulate_cytokine_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_immune_to_cingulate) return NIMCP_SUCCESS;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current inflammation level from immune system */
    brain_immune_stats_t immune_stats;
    if (brain_immune_get_stats(bridge->immune, &immune_stats) != 0) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Compute cytokine effects on cingulate function */

    /* IL-1B -> ERN reduction */
    float il1_effect = CYTOKINE_IL1_ERN_IMPACT *
        bridge->config.cytokine_sensitivity * immune_stats.cytokine_il1;

    /* IL-6 -> N2 reduction */
    float il6_effect = CYTOKINE_IL6_N2_IMPACT *
        bridge->config.cytokine_sensitivity * immune_stats.cytokine_il6;

    /* TNF-a -> Cognitive control impairment */
    float tnf_effect = CYTOKINE_TNF_COGNITIVE_IMPACT *
        bridge->config.cytokine_sensitivity * immune_stats.cytokine_tnf;

    /* IL-10 -> Recovery boost */
    float il10_effect = CYTOKINE_IL10_RECOVERY_IMPACT *
        bridge->config.cytokine_sensitivity * immune_stats.cytokine_il10;

    /* Compute inflammation factor */
    float inflammation_factor = cingulate_immune_compute_inflammation_factor(
        immune_stats.inflammation_level);

    /* Compute modulated values */
    float ern_mod = 1.0f + il1_effect;
    if (ern_mod < 0.1f) ern_mod = 0.1f;
    if (ern_mod > 1.2f) ern_mod = 1.2f;

    float n2_mod = 1.0f + il6_effect;
    if (n2_mod < 0.1f) n2_mod = 0.1f;
    if (n2_mod > 1.2f) n2_mod = 1.2f;

    float control_cap = inflammation_factor + tnf_effect + il10_effect;
    if (control_cap < 0.1f) control_cap = 0.1f;
    if (control_cap > 1.0f) control_cap = 1.0f;

    /* Update cached effects */
    bridge->cytokine_effects.ern_modulation = ern_mod;
    bridge->cytokine_effects.error_sensitivity = ern_mod;
    bridge->cytokine_effects.n2_modulation = n2_mod;
    bridge->cytokine_effects.conflict_threshold = 0.5f / n2_mod;  /* Higher threshold = less detection */
    bridge->cytokine_effects.control_capacity = control_cap;
    bridge->cytokine_effects.attention_boost_capacity = control_cap;
    bridge->cytokine_effects.emotional_regulation = inflammation_factor;
    bridge->cytokine_effects.pain_sensitivity = 1.0f / inflammation_factor;  /* More sensitive */
    bridge->cytokine_effects.self_referential_capacity = inflammation_factor;
    bridge->cytokine_effects.dmn_connectivity = inflammation_factor;
    bridge->cytokine_effects.il1_contribution = il1_effect;
    bridge->cytokine_effects.il6_contribution = il6_effect;
    bridge->cytokine_effects.tnf_contribution = tnf_effect;
    bridge->cytokine_effects.il10_contribution = il10_effect;
    bridge->cytokine_effects.inflammation_level = immune_stats.inflammation_level;
    bridge->cytokine_effects.combined_modulation = inflammation_factor;

    /* Update state based on inflammation */
    if (immune_stats.inflammation_level == INFLAMMATION_STORM) {
        bridge->state = CINGULATE_IMMUNE_SEVERE_IMPAIRMENT;
        bridge->stats.impairment_episodes++;
    } else if (immune_stats.inflammation_level == INFLAMMATION_SYSTEMIC) {
        bridge->state = CINGULATE_IMMUNE_MODERATE_IMPAIRMENT;
    } else if (immune_stats.inflammation_level >= INFLAMMATION_LOCAL) {
        bridge->state = CINGULATE_IMMUNE_MILD_IMPAIRMENT;
    } else if (immune_stats.cytokine_il10 > 0.3f) {
        bridge->state = CINGULATE_IMMUNE_RECOVERING;
        bridge->stats.recovery_episodes++;
    } else {
        bridge->state = CINGULATE_IMMUNE_NORMAL;
    }

    bridge->stats.immune_to_cingulate_count++;

    /* Update averaging stats */
    bridge->ern_mod_sum += ern_mod;
    bridge->conflict_mod_sum += n2_mod;
    if (bridge->stats.total_updates > 0) {
        bridge->stats.avg_ern_modulation =
            bridge->ern_mod_sum / (float)bridge->stats.total_updates;
        bridge->stats.avg_conflict_modulation =
            bridge->conflict_mod_sum / (float)bridge->stats.total_updates;
    }

    /* Copy effects to output if requested */
    if (effects) {
        *effects = bridge->cytokine_effects;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/*=============================================================================
 * Cingulate -> Immune Update
 *===========================================================================*/

int cingulate_immune_update_cingulate_to_immune(
    cingulate_immune_bridge_t bridge,
    cingulate_immune_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_cingulate_to_immune) return NIMCP_SUCCESS;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute error-based immune modulation */
    float error_rate = 0.0f;
    if (bridge->recent_error_count > 0) {
        error_rate = bridge->recent_error_sum / (float)bridge->recent_error_count;
    }
    float error_immune = cingulate_immune_compute_error_immune(error_rate);
    error_immune = 1.0f + (error_immune - 1.0f) * bridge->config.error_coupling;

    bool chronic_errors = error_rate > bridge->config.chronic_error_threshold;
    if (chronic_errors) {
        bridge->stats.high_error_episodes++;
    }

    /* Compute conflict-based immune modulation */
    float conflict_level = 0.0f;
    if (bridge->recent_conflict_count > 0) {
        conflict_level = bridge->recent_conflict_sum / (float)bridge->recent_conflict_count;
    }
    float conflict_immune = cingulate_immune_compute_conflict_immune(conflict_level);
    conflict_immune = 1.0f + (conflict_immune - 1.0f) * bridge->config.conflict_coupling;

    bool chronic_conflict = conflict_level > bridge->config.chronic_conflict_threshold;
    if (chronic_conflict) {
        bridge->stats.high_conflict_episodes++;
    }

    /* Compute emotion-based immune modulation */
    float valence = bridge->immune_effects.emotional_valence;  /* From previous report */
    float arousal = bridge->immune_effects.emotional_arousal;
    float emotion_immune = 1.0f;
    if (bridge->config.enable_emotional_modulation) {
        emotion_immune = cingulate_immune_compute_emotion_immune(valence);
        /* Arousal amplifies the effect */
        emotion_immune = 1.0f + (emotion_immune - 1.0f) * (0.5f + 0.5f * arousal);
        emotion_immune = 1.0f + (emotion_immune - 1.0f) * bridge->config.emotion_coupling;
    }

    /* Compute combined factor */
    float combined = error_immune * conflict_immune * emotion_immune;

    /* Determine stress response */
    bool stress_active = chronic_errors || chronic_conflict ||
                         (valence < -0.5f && arousal > 0.5f);
    if (stress_active) {
        bridge->stats.stress_episodes++;
        bridge->state = CINGULATE_IMMUNE_STRESSED;
    }

    /* Cortisol analog based on stress level */
    float cortisol = 0.0f;
    if (stress_active) {
        cortisol = 0.3f + 0.7f * (error_rate + conflict_level) / 2.0f;
        if (cortisol > 1.0f) cortisol = 1.0f;
    }

    /* Chronic stress detection */
    if (chronic_errors && chronic_conflict) {
        bridge->stats.chronic_stress_detections++;
    }

    /* Update cached effects */
    bridge->immune_effects.error_rate = error_rate;
    bridge->immune_effects.error_immune_factor = error_immune;
    bridge->immune_effects.chronic_errors = chronic_errors;
    bridge->immune_effects.conflict_level = conflict_level;
    bridge->immune_effects.conflict_immune_factor = conflict_immune;
    bridge->immune_effects.chronic_conflict = chronic_conflict;
    bridge->immune_effects.emotion_immune_factor = emotion_immune;
    bridge->immune_effects.combined_immune_factor = combined;
    bridge->immune_effects.stress_response_active = stress_active;
    bridge->immune_effects.cortisol_analog = cortisol;

    bridge->stats.cingulate_to_immune_count++;

    /* Update averaging stats */
    bridge->immune_factor_sum += combined;
    if (bridge->stats.total_updates > 0) {
        bridge->stats.avg_immune_factor =
            bridge->immune_factor_sum / (float)bridge->stats.total_updates;
    }

    /* Reset tracking counters for next period */
    bridge->recent_error_sum = 0.0f;
    bridge->recent_error_count = 0;
    bridge->recent_conflict_sum = 0.0f;
    bridge->recent_conflict_count = 0;

    /* Copy effects to output if requested */
    if (effects) {
        *effects = bridge->immune_effects;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/*=============================================================================
 * Combined Update
 *===========================================================================*/

int cingulate_immune_update(cingulate_immune_bridge_t bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    int result;

    /* Update immune -> cingulate */
    result = cingulate_immune_update_immune_to_cingulate(bridge, NULL);
    if (result != NIMCP_SUCCESS) {
        NIMCP_LOGGING_WARN("Immune->Cingulate update failed: %d", result);
    }

    /* Update cingulate -> immune */
    result = cingulate_immune_update_cingulate_to_immune(bridge, NULL);
    if (result != NIMCP_SUCCESS) {
        NIMCP_LOGGING_WARN("Cingulate->Immune update failed: %d", result);
    }

    /* Increment update counter */
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->stats.total_updates++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * Query Functions
 *===========================================================================*/

int cingulate_immune_get_cytokine_effects(
    cingulate_immune_bridge_t bridge,
    cingulate_cytokine_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_NULL_POINTER, "bridge or effects is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->cytokine_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int cingulate_immune_get_immune_effects(
    cingulate_immune_bridge_t bridge,
    cingulate_immune_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_NULL_POINTER, "bridge or effects is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->immune_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

cingulate_immune_state_t cingulate_immune_get_state(
    cingulate_immune_bridge_t bridge
) {
    if (!bridge) return CINGULATE_IMMUNE_NORMAL;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    cingulate_immune_state_t state = bridge->state;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return state;
}

int cingulate_immune_get_stats(
    cingulate_immune_bridge_t bridge,
    cingulate_immune_stats_t* stats
) {
    NIMCP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * Error and Conflict Reporting
 *===========================================================================*/

int cingulate_immune_report_error(
    cingulate_immune_bridge_t bridge,
    float error_severity
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_error_tracking) return NIMCP_SUCCESS;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->recent_error_sum += error_severity;
    bridge->recent_error_count++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int cingulate_immune_report_conflict(
    cingulate_immune_bridge_t bridge,
    float conflict_level
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_conflict_tracking) return NIMCP_SUCCESS;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->recent_conflict_sum += conflict_level;
    bridge->recent_conflict_count++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int cingulate_immune_report_emotion(
    cingulate_immune_bridge_t bridge,
    float valence,
    float arousal
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_emotional_modulation) return NIMCP_SUCCESS;

    /* Clamp values */
    if (valence < -1.0f) valence = -1.0f;
    if (valence > 1.0f) valence = 1.0f;
    if (arousal < 0.0f) arousal = 0.0f;
    if (arousal > 1.0f) arousal = 1.0f;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->immune_effects.emotional_valence = valence;
    bridge->immune_effects.emotional_arousal = arousal;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/*=============================================================================
 * Bio-Async Connection
 *===========================================================================*/

int cingulate_immune_connect_bio_async(cingulate_immune_bridge_t bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->bio_async_connected) return NIMCP_SUCCESS;

    /* Register with bio-router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_BRAIN + 0x300,  /* Cingulate-immune bridge ID */
        .module_name = "cingulate_immune_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->bio_async_connected = true;
        NIMCP_LOGGING_DEBUG("Cingulate-immune bridge connected to bio-async");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    return NIMCP_SUCCESS;
}

int cingulate_immune_disconnect_bio_async(cingulate_immune_bridge_t bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->bio_async_connected) return NIMCP_SUCCESS;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->bio_async_connected = false;
    NIMCP_LOGGING_DEBUG("Cingulate-immune bridge disconnected from bio-async");

    return NIMCP_SUCCESS;
}

bool cingulate_immune_is_bio_async_connected(cingulate_immune_bridge_t bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->bio_async_connected;
}
