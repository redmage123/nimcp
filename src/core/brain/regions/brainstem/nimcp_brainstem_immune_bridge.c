/**
 * @file nimcp_brainstem_immune_bridge.c
 * @brief Brainstem-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-30
 *
 * WHAT: Bidirectional integration between brain immune system and brainstem
 * WHY:  Creates critical autonomic-immune feedback loops for survival
 * HOW:  Cytokines modulate arousal/reflexes; brainstem state modulates immunity
 */

#include "core/brain/regions/brainstem/nimcp_brainstem_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE "BRAINSTEM_IMMUNE_BRIDGE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brainstem_immune_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * Internal Structure
 *===========================================================================*/

struct brainstem_immune_bridge {
    bridge_base_t base;                    /**< MUST be first: base infrastructure */

    /* Configuration */
    brainstem_immune_config_t config;

    /* Connected systems */
    brainstem_adapter_t* brainstem;
    brain_immune_system_t* immune;

    /* Current state */
    brainstem_immune_state_t state;

    /* Cached effects */
    brainstem_cytokine_effects_t cytokine_effects;
    brainstem_immune_effects_t immune_effects;

    /* Statistics */
    brainstem_immune_stats_t stats;

    /* Bio-async connected flag */
    bool bio_async_connected;

    /* Thread safety mutex */
    nimcp_platform_mutex_t mutex;

    /* Timing */
    uint64_t last_update_ms;

    /* Accumulators for averaging */
    float arousal_mod_sum;
    float immune_factor_sum;
    float reflex_pot_sum;
};

/*=============================================================================
 * Default Configuration
 *===========================================================================*/

void brainstem_immune_default_config(brainstem_immune_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(brainstem_immune_config_t));

    config->enable_immune_to_brainstem = true;
    config->enable_brainstem_to_immune = true;
    config->enable_reflex_modulation = true;
    config->enable_vital_monitoring = true;
    config->cytokine_sensitivity = 1.0f;
    config->arousal_coupling = 1.0f;
    config->protection_coupling = 1.0f;
    config->enable_bio_async = true;
    config->update_interval_ms = BRAINSTEM_IMMUNE_UPDATE_INTERVAL_MS;
    config->emergency_on_storm = true;
}

/*=============================================================================
 * Utility Functions
 *===========================================================================*/

float brainstem_immune_compute_inflammation_arousal(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_NONE_BRAINSTEM_FACTOR;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_LOCAL_BRAINSTEM_FACTOR;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_REGIONAL_BRAINSTEM_FACTOR;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_SYSTEMIC_BRAINSTEM_FACTOR;
        case INFLAMMATION_STORM:    return INFLAMMATION_STORM_BRAINSTEM_FACTOR;
        default:                    return 1.0f;
    }
}

float brainstem_immune_compute_arousal_immune(float arousal) {
    if (arousal < BRAINSTEM_AROUSAL_LOW_THRESHOLD) {
        /* Low arousal -> immune depression */
        float ratio = arousal / BRAINSTEM_AROUSAL_LOW_THRESHOLD;
        return 0.5f + ratio * 0.5f;
    } else if (arousal > BRAINSTEM_AROUSAL_HIGH_THRESHOLD) {
        /* High arousal -> enhanced surveillance */
        float ratio = (arousal - BRAINSTEM_AROUSAL_HIGH_THRESHOLD) /
                      (1.0f - BRAINSTEM_AROUSAL_HIGH_THRESHOLD);
        return 1.0f + ratio * 0.5f;
    }
    /* Normal arousal -> baseline immunity */
    return 1.0f;
}

float brainstem_immune_compute_reflex_potentiation(brain_inflammation_level_t level) {
    return (float)level * INFLAMMATION_REFLEX_POTENTIATION;
}

const char* brainstem_immune_state_to_string(brainstem_immune_state_t state) {
    switch (state) {
        case BRAINSTEM_IMMUNE_NORMAL:           return "NORMAL";
        case BRAINSTEM_IMMUNE_MILD_DEPRESSION:  return "MILD_DEPRESSION";
        case BRAINSTEM_IMMUNE_MODERATE_SICKNESS: return "MODERATE_SICKNESS";
        case BRAINSTEM_IMMUNE_SEVERE_SICKNESS:  return "SEVERE_SICKNESS";
        case BRAINSTEM_IMMUNE_PROTECTIVE:       return "PROTECTIVE";
        case BRAINSTEM_IMMUNE_RECOVERING:       return "RECOVERING";
        default:                                return "UNKNOWN";
    }
}

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

brainstem_immune_bridge_t brainstem_immune_create(
    const brainstem_immune_config_t* config,
    brainstem_adapter_t* brainstem,
    brain_immune_system_t* immune
) {
    /* Guard: Null checks */
    if (!brainstem || !immune) {
        NIMCP_LOGGING_ERROR("Null brainstem or immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_immune_create: required parameter is NULL (brainstem, immune)");
        return NULL;
    }

    /* Allocate bridge */
    brainstem_immune_bridge_t bridge = nimcp_malloc(sizeof(struct brainstem_immune_bridge));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate brainstem-immune bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(struct brainstem_immune_bridge));

    /* Copy configuration or use defaults */
    if (config) {
        bridge->config = *config;
    } else {
        brainstem_immune_default_config(&bridge->config);
    }

    /* Store references */
    bridge->brainstem = brainstem;
    bridge->immune = immune;
    bridge->state = BRAINSTEM_IMMUNE_NORMAL;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "brainstem_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create bridge mutex");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brainstem_immune_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize effects to neutral */
    bridge->cytokine_effects.arousal_modulation = 0.0f;
    bridge->cytokine_effects.inflammation_level = INFLAMMATION_NONE;
    bridge->cytokine_effects.inflammation_arousal_factor = 1.0f;
    bridge->cytokine_effects.reflex_potentiation = 0.0f;
    bridge->cytokine_effects.vital_stability_factor = 1.0f;

    bridge->immune_effects.arousal_immune_factor = 1.0f;
    bridge->immune_effects.protection_immune_factor = 1.0f;
    bridge->immune_effects.combined_immune_factor = 1.0f;

    NIMCP_LOGGING_INFO("Brainstem-immune bridge created successfully");
    NIMCP_LOGGING_INFO("  Immune->Brainstem: %s",
        bridge->config.enable_immune_to_brainstem ? "ENABLED" : "DISABLED");
    NIMCP_LOGGING_INFO("  Brainstem->Immune: %s",
        bridge->config.enable_brainstem_to_immune ? "ENABLED" : "DISABLED");

    return bridge;
}

void brainstem_immune_destroy(brainstem_immune_bridge_t bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->bio_async_connected) {
        brainstem_immune_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_DEBUG("Brainstem-immune bridge destroyed");
}

/*=============================================================================
 * Immune -> Brainstem Update
 *===========================================================================*/

int brainstem_immune_update_immune_to_brainstem(
    brainstem_immune_bridge_t bridge,
    brainstem_cytokine_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_immune_to_brainstem) return NIMCP_SUCCESS;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current inflammation level from immune system */
    brain_immune_stats_t immune_stats;
    if (brain_immune_get_stats(bridge->immune, &immune_stats) != 0) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Compute cytokine effects on arousal */
    float cytokine_arousal_mod = 0.0f;

    /* IL-1B effect */
    float il1_effect = CYTOKINE_IL1_BRAINSTEM_IMPACT *
        bridge->config.cytokine_sensitivity * immune_stats.cytokine_il1;
    cytokine_arousal_mod += il1_effect;

    /* IL-6 effect */
    float il6_effect = CYTOKINE_IL6_BRAINSTEM_IMPACT *
        bridge->config.cytokine_sensitivity * immune_stats.cytokine_il6;
    cytokine_arousal_mod += il6_effect;

    /* TNF-a effect */
    float tnf_effect = CYTOKINE_TNF_BRAINSTEM_IMPACT *
        bridge->config.cytokine_sensitivity * immune_stats.cytokine_tnf;
    cytokine_arousal_mod += tnf_effect;

    /* IL-10 effect (recovery) */
    float il10_effect = CYTOKINE_IL10_BRAINSTEM_IMPACT *
        bridge->config.cytokine_sensitivity * immune_stats.cytokine_il10;
    cytokine_arousal_mod += il10_effect;

    /* Clamp arousal modulation */
    if (cytokine_arousal_mod < -1.0f) cytokine_arousal_mod = -1.0f;
    if (cytokine_arousal_mod > 1.0f) cytokine_arousal_mod = 1.0f;

    /* Compute inflammation arousal factor */
    float inflammation_factor = brainstem_immune_compute_inflammation_arousal(
        immune_stats.inflammation_level);

    /* Compute reflex potentiation */
    float reflex_pot = 0.0f;
    if (bridge->config.enable_reflex_modulation) {
        reflex_pot = brainstem_immune_compute_reflex_potentiation(
            immune_stats.inflammation_level);
    }

    /* Check for emergency condition */
    if (bridge->config.emergency_on_storm &&
        immune_stats.inflammation_level == INFLAMMATION_STORM) {
        bridge->stats.storms_detected++;
        bridge->stats.emergencies_triggered++;
        /* Trigger protective mode on brainstem */
        brainstem_trigger_protection(bridge->brainstem, 1.0f);
    }

    /* Update cached effects */
    bridge->cytokine_effects.arousal_modulation = cytokine_arousal_mod;
    bridge->cytokine_effects.il1_contribution = il1_effect;
    bridge->cytokine_effects.il6_contribution = il6_effect;
    bridge->cytokine_effects.tnf_contribution = tnf_effect;
    bridge->cytokine_effects.il10_contribution = il10_effect;
    bridge->cytokine_effects.inflammation_level = immune_stats.inflammation_level;
    bridge->cytokine_effects.inflammation_arousal_factor = inflammation_factor;
    bridge->cytokine_effects.reflex_potentiation = reflex_pot;
    bridge->cytokine_effects.vital_stability_factor =
        1.0f - (1.0f - inflammation_factor) * 0.5f; /* Less impact on vitals */

    /* Update state based on inflammation */
    if (immune_stats.inflammation_level == INFLAMMATION_STORM) {
        bridge->state = BRAINSTEM_IMMUNE_SEVERE_SICKNESS;
        bridge->stats.sickness_episodes++;
    } else if (immune_stats.inflammation_level == INFLAMMATION_SYSTEMIC) {
        bridge->state = BRAINSTEM_IMMUNE_MODERATE_SICKNESS;
    } else if (immune_stats.inflammation_level >= INFLAMMATION_LOCAL) {
        bridge->state = BRAINSTEM_IMMUNE_MILD_DEPRESSION;
    } else if (immune_stats.cytokine_il10 > 0.3f) {
        bridge->state = BRAINSTEM_IMMUNE_RECOVERING;
    } else {
        bridge->state = BRAINSTEM_IMMUNE_NORMAL;
    }

    /* Apply effects to brainstem if significant */
    if (fabsf(cytokine_arousal_mod) > 0.05f) {
        /* Apply arousal reduction via brainstem adapter */
        if (cytokine_arousal_mod < 0) {
            brainstem_reduce_arousal(bridge->brainstem, -cytokine_arousal_mod * 0.1f);
        } else {
            brainstem_boost_arousal(bridge->brainstem, cytokine_arousal_mod * 0.1f);
        }
        bridge->stats.immune_to_brainstem_count++;
    }

    /* Update averaging stats */
    bridge->arousal_mod_sum += cytokine_arousal_mod;
    bridge->reflex_pot_sum += reflex_pot;
    if (bridge->stats.total_updates > 0) {
        bridge->stats.avg_arousal_modulation =
            bridge->arousal_mod_sum / (float)bridge->stats.total_updates;
        bridge->stats.avg_reflex_potentiation =
            bridge->reflex_pot_sum / (float)bridge->stats.total_updates;
    }

    /* Copy effects to output if requested */
    if (effects) {
        *effects = bridge->cytokine_effects;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/*=============================================================================
 * Brainstem -> Immune Update
 *===========================================================================*/

int brainstem_immune_update_brainstem_to_immune(
    brainstem_immune_bridge_t bridge,
    brainstem_immune_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_brainstem_to_immune) return NIMCP_SUCCESS;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current brainstem state */
    float arousal = brainstem_get_arousal_value(bridge->brainstem);
    brainstem_arousal_level_t arousal_level = brainstem_get_arousal_level(bridge->brainstem);
    brainstem_status_t status = brainstem_get_status(bridge->brainstem);

    /* Compute arousal-based immune factor */
    float arousal_immune = brainstem_immune_compute_arousal_immune(arousal);
    arousal_immune = 1.0f + (arousal_immune - 1.0f) * bridge->config.arousal_coupling;

    /* Compute protection-based immune factor */
    float protection_immune = 1.0f;
    if (status == BRAINSTEM_STATUS_ALERT) {
        protection_immune = BRAINSTEM_PROTECTION_ALERT_FACTOR;
        bridge->stats.protective_activations++;
        bridge->state = BRAINSTEM_IMMUNE_PROTECTIVE;
    } else if (status == BRAINSTEM_STATUS_PROTECTIVE) {
        protection_immune = BRAINSTEM_PROTECTION_HIGH_FACTOR;
    }
    protection_immune = 1.0f + (protection_immune - 1.0f) * bridge->config.protection_coupling;

    /* Compute combined factor */
    float combined = arousal_immune * protection_immune;

    /* Determine surveillance enhancement */
    bool enhance = arousal > BRAINSTEM_AROUSAL_HIGH_THRESHOLD ||
                   status == BRAINSTEM_STATUS_ALERT ||
                   status == BRAINSTEM_STATUS_PROTECTIVE;

    /* Determine stress response */
    bool stress = arousal > BRAINSTEM_AROUSAL_HYPER_THRESHOLD;

    /* Cholinergic suppression during rest */
    bool cholinergic = arousal < 0.3f && status != BRAINSTEM_STATUS_PROTECTIVE;

    /* Update cached effects */
    bridge->immune_effects.arousal_immune_factor = arousal_immune;
    bridge->immune_effects.arousal_level = arousal_level;
    bridge->immune_effects.arousal_value = arousal;
    bridge->immune_effects.protection_immune_factor = protection_immune;
    bridge->immune_effects.status = status;
    bridge->immune_effects.combined_immune_factor = combined;
    bridge->immune_effects.enhance_surveillance = enhance;
    bridge->immune_effects.trigger_stress_response = stress;
    bridge->immune_effects.cholinergic_suppression = cholinergic;

    bridge->stats.brainstem_to_immune_count++;

    /* Update averaging stats */
    bridge->immune_factor_sum += combined;
    if (bridge->stats.total_updates > 0) {
        bridge->stats.avg_immune_factor =
            bridge->immune_factor_sum / (float)bridge->stats.total_updates;
    }

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

int brainstem_immune_update(brainstem_immune_bridge_t bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    int result;

    /* Update immune -> brainstem */
    result = brainstem_immune_update_immune_to_brainstem(bridge, NULL);
    if (result != NIMCP_SUCCESS) {
        NIMCP_LOGGING_WARN("Immune->Brainstem update failed: %d", result);
    }

    /* Update brainstem -> immune */
    result = brainstem_immune_update_brainstem_to_immune(bridge, NULL);
    if (result != NIMCP_SUCCESS) {
        NIMCP_LOGGING_WARN("Brainstem->Immune update failed: %d", result);
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

int brainstem_immune_get_cytokine_effects(
    brainstem_immune_bridge_t bridge,
    brainstem_cytokine_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_NULL_POINTER, "bridge or effects is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->cytokine_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int brainstem_immune_get_immune_effects(
    brainstem_immune_bridge_t bridge,
    brainstem_immune_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_NULL_POINTER, "bridge or effects is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->immune_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

brainstem_immune_state_t brainstem_immune_get_state(
    brainstem_immune_bridge_t bridge
) {
    if (!bridge) return BRAINSTEM_IMMUNE_NORMAL;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    brainstem_immune_state_t state = bridge->state;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return state;
}

int brainstem_immune_get_stats(
    brainstem_immune_bridge_t bridge,
    brainstem_immune_stats_t* stats
) {
    NIMCP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * Bio-Async Connection
 *===========================================================================*/

int brainstem_immune_connect_bio_async(brainstem_immune_bridge_t bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->bio_async_connected) return NIMCP_SUCCESS;

    /* Register with bio-router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_BRAIN + 0x200,  /* Brainstem-immune bridge ID */
        .module_name = "brainstem_immune_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->bio_async_connected = true;
        NIMCP_LOGGING_DEBUG("Brainstem-immune bridge connected to bio-async");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    return NIMCP_SUCCESS;
}

int brainstem_immune_disconnect_bio_async(brainstem_immune_bridge_t bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->bio_async_connected) return NIMCP_SUCCESS;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->bio_async_connected = false;
    NIMCP_LOGGING_DEBUG("Brainstem-immune bridge disconnected from bio-async");

    return NIMCP_SUCCESS;
}

bool brainstem_immune_is_bio_async_connected(brainstem_immune_bridge_t bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->bio_async_connected;
}
