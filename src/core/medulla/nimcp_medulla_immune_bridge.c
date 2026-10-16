/**
 * @file nimcp_medulla_immune_bridge.c
 * @brief Medulla-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between brain immune system and medulla oblongata
 * WHY:  Creates critical autonomic-immune feedback loops for survival
 * HOW:  Cytokines modulate arousal/protection; medulla state modulates immunity
 */

#include "core/medulla/nimcp_medulla_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE "MEDULLA_IMMUNE_BRIDGE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(medulla_immune_bridge)

//=============================================================================
// Internal Structure
//=============================================================================

struct medulla_immune_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /** Configuration */
    medulla_immune_config_t config;

    /** Connected medulla */
    medulla_t medulla;

    /** Connected immune system */
    brain_immune_system_t* immune;

    /** Cached cytokine effects */
    medulla_cytokine_effects_t cytokine_effects;

    /** Cached immune effects */
    medulla_immune_effects_t immune_effects;

    /** Statistics */
    medulla_immune_stats_t stats;

    /** Bio-async connected flag */
    bool bio_async_connected;

    /** Thread safety mutex */
    nimcp_platform_mutex_t mutex;

    /** Last update timestamp */
    uint64_t last_update_ms;

    /** Accumulated arousal modulation for averaging */
    float arousal_mod_sum;

    /** Accumulated immune factor for averaging */
    float immune_factor_sum;
};

//=============================================================================
// Default Configuration
//=============================================================================

void medulla_immune_default_config(medulla_immune_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_immune_default_config: config is NULL");
        return;
    }

    memset(config, 0, sizeof(medulla_immune_config_t));

    config->enable_immune_to_medulla = true;
    config->enable_medulla_to_immune = true;
    config->enable_circadian_modulation = true;
    config->update_interval_ms = MEDULLA_IMMUNE_UPDATE_INTERVAL_MS;
    config->cytokine_sensitivity = 1.0f;
    config->protection_coupling = 1.0f;
    config->arousal_coupling = 1.0f;
    config->circadian_coupling = 1.0f;
    config->enable_bio_async = true;
    config->emergency_on_storm = true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

medulla_immune_bridge_t medulla_immune_create(
    const medulla_immune_config_t* config,
    medulla_t medulla,
    brain_immune_system_t* immune
) {
    /* Guard: Null checks */
    if (!medulla || !immune) {
        NIMCP_LOGGING_ERROR("Null medulla or immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_immune_create: medulla or immune is NULL");
        return NULL;
    }

    /* Allocate bridge */
    medulla_immune_bridge_t bridge = nimcp_malloc(sizeof(struct medulla_immune_bridge));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "medulla_immune_create: failed to allocate bridge");
        NIMCP_LOGGING_ERROR("Failed to allocate medulla-immune bridge");
        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(struct medulla_immune_bridge));

    /* Copy configuration or use defaults */
    if (config) {
        bridge->config = *config;
    } else {
        medulla_immune_default_config(&bridge->config);
    }

    /* Store references */
    bridge->medulla = medulla;
    bridge->immune = immune;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "medulla_immune") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "medulla_immune_create: failed to initialize bridge base");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "medulla_immune_create: failed to create mutex");
        NIMCP_LOGGING_ERROR("Failed to create bridge mutex");
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize effects to neutral */
    bridge->cytokine_effects.arousal_modulation = 0.0f;
    bridge->cytokine_effects.protection_adjustment = 0;
    bridge->cytokine_effects.circadian_disruption = 0.0f;
    bridge->cytokine_effects.inflammation_level = INFLAMMATION_NONE;
    bridge->cytokine_effects.trigger_emergency = false;
    bridge->cytokine_effects.inflammation_arousal_factor = 1.0f;

    bridge->immune_effects.arousal_immune_factor = 1.0f;
    bridge->immune_effects.protection_immune_factor = 1.0f;
    bridge->immune_effects.circadian_immune_factor = 1.0f;
    bridge->immune_effects.combined_immune_factor = 1.0f;
    bridge->immune_effects.enhance_surveillance = false;

    NIMCP_LOGGING_INFO("Medulla-immune bridge created successfully");
    NIMCP_LOGGING_INFO("  Immune→Medulla: %s",
        bridge->config.enable_immune_to_medulla ? "ENABLED" : "DISABLED");
    NIMCP_LOGGING_INFO("  Medulla→Immune: %s",
        bridge->config.enable_medulla_to_immune ? "ENABLED" : "DISABLED");
    NIMCP_LOGGING_INFO("  Circadian mod: %s",
        bridge->config.enable_circadian_modulation ? "ENABLED" : "DISABLED");

    return bridge;
}

void medulla_immune_destroy(medulla_immune_bridge_t bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->bio_async_connected) {
        medulla_immune_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_DEBUG("Medulla-immune bridge destroyed");
}

//=============================================================================
// Utility Functions
//=============================================================================

float medulla_immune_compute_inflammation_arousal(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_NONE_AROUSAL_FACTOR;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_LOCAL_AROUSAL_FACTOR;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_REGIONAL_AROUSAL_FACTOR;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_SYSTEMIC_AROUSAL_FACTOR;
        case INFLAMMATION_STORM:    return INFLAMMATION_STORM_AROUSAL_FACTOR;
        default:                    return 1.0f;
    }
}

float medulla_immune_compute_protection_immune(protection_level_t level) {
    switch (level) {
        case PROTECTION_LEVEL_NORMAL:   return PROTECTION_NORMAL_IMMUNE_FACTOR;
        case PROTECTION_LEVEL_CAUTIOUS:  return PROTECTION_ELEVATED_IMMUNE_FACTOR;
        case PROTECTION_LEVEL_GUARDED:   return PROTECTION_HIGH_IMMUNE_FACTOR;
        case PROTECTION_LEVEL_DEFENSIVE: return PROTECTION_HIGH_IMMUNE_FACTOR;
        case PROTECTION_LEVEL_CRITICAL: return PROTECTION_CRITICAL_IMMUNE_FACTOR;
        case PROTECTION_LEVEL_SHUTDOWN: return PROTECTION_SHUTDOWN_IMMUNE_FACTOR;
        default:                        return 1.0f;
    }
}

float medulla_immune_compute_circadian_immune(circadian_phase_t phase) {
    switch (phase) {
        /* Day phases - active immune surveillance */
        case CIRCADIAN_PHASE_EARLY_MORNING:
        case CIRCADIAN_PHASE_MORNING:
        case CIRCADIAN_PHASE_AFTERNOON:
        case CIRCADIAN_PHASE_EVENING:
            return CIRCADIAN_DAY_IMMUNE_FACTOR;

        /* Night phases - immune repair/consolidation */
        case CIRCADIAN_PHASE_LATE_EVENING:
        case CIRCADIAN_PHASE_NIGHT:
        case CIRCADIAN_PHASE_DEEP_NIGHT:
        case CIRCADIAN_PHASE_PRE_DAWN:
            return CIRCADIAN_NIGHT_IMMUNE_FACTOR;

        default:
            return 1.0f;
    }
}

//=============================================================================
// Immune → Medulla Update
//=============================================================================

int medulla_immune_update_immune_to_medulla(
    medulla_immune_bridge_t bridge,
    medulla_cytokine_effects_t* effects
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_immune_update_immune_to_medulla: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bridge->config.enable_immune_to_medulla) return NIMCP_SUCCESS;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current inflammation level from immune system */
    brain_immune_stats_t immune_stats;
    if (brain_immune_get_stats(bridge->immune, &immune_stats) != 0) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Compute cytokine effects on arousal */
    float cytokine_arousal_mod = 0.0f;

    /* IL-1β effect */
    cytokine_arousal_mod += CYTOKINE_IL1_AROUSAL_IMPACT *
        bridge->config.cytokine_sensitivity * immune_stats.cytokine_il1;

    /* IL-6 effect */
    cytokine_arousal_mod += CYTOKINE_IL6_AROUSAL_IMPACT *
        bridge->config.cytokine_sensitivity * immune_stats.cytokine_il6;

    /* TNF-α effect */
    cytokine_arousal_mod += CYTOKINE_TNF_AROUSAL_IMPACT *
        bridge->config.cytokine_sensitivity * immune_stats.cytokine_tnf;

    /* IL-10 effect (recovery) */
    cytokine_arousal_mod += CYTOKINE_IL10_AROUSAL_IMPACT *
        bridge->config.cytokine_sensitivity * immune_stats.cytokine_il10;

    /* IFN-γ effect */
    cytokine_arousal_mod += CYTOKINE_IFN_AROUSAL_IMPACT *
        bridge->config.cytokine_sensitivity * immune_stats.cytokine_ifn_gamma;

    /* Clamp arousal modulation */
    if (cytokine_arousal_mod < -1.0f) cytokine_arousal_mod = -1.0f;
    if (cytokine_arousal_mod > 1.0f) cytokine_arousal_mod = 1.0f;

    /* Compute inflammation arousal factor */
    float inflammation_factor = medulla_immune_compute_inflammation_arousal(
        immune_stats.inflammation_level);

    /* Check for emergency condition */
    bool trigger_emergency = false;
    if (bridge->config.emergency_on_storm &&
        immune_stats.inflammation_level == INFLAMMATION_STORM) {
        trigger_emergency = true;
        bridge->stats.storms_detected++;
    }

    /* Compute protection adjustment based on inflammation */
    int protection_adj = 0;
    switch (immune_stats.inflammation_level) {
        case INFLAMMATION_NONE:     protection_adj = 0; break;
        case INFLAMMATION_LOCAL:    protection_adj = 0; break;
        case INFLAMMATION_REGIONAL: protection_adj = 1; break;
        case INFLAMMATION_SYSTEMIC: protection_adj = 2; break;
        case INFLAMMATION_STORM:    protection_adj = 3; break;
        default: break;
    }

    /* Update cached effects */
    bridge->cytokine_effects.arousal_modulation = cytokine_arousal_mod;
    bridge->cytokine_effects.protection_adjustment = protection_adj;
    bridge->cytokine_effects.inflammation_level = immune_stats.inflammation_level;
    bridge->cytokine_effects.trigger_emergency = trigger_emergency;
    bridge->cytokine_effects.inflammation_arousal_factor = inflammation_factor;

    /* Apply effects to medulla if significant */
    if (fabsf(cytokine_arousal_mod) > 0.05f || protection_adj > 0) {
        /* For now, the medulla will read these effects via the bridge
         * In a full implementation, we would call medulla modulation functions */
        bridge->stats.immune_to_medulla_count++;
    }

    /* Trigger emergency if needed */
    if (trigger_emergency) {
        medulla_emergency_shutdown(bridge->medulla, "Cytokine storm detected");
        bridge->stats.emergencies_triggered++;
    }

    /* Update averaging stats */
    bridge->arousal_mod_sum += cytokine_arousal_mod;
    if (bridge->stats.total_updates > 0) {
        bridge->stats.avg_arousal_modulation =
            bridge->arousal_mod_sum / (float)bridge->stats.total_updates;
    }

    /* Copy effects to output if requested */
    if (effects) {
        *effects = bridge->cytokine_effects;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Medulla → Immune Update
//=============================================================================

int medulla_immune_update_medulla_to_immune(
    medulla_immune_bridge_t bridge,
    medulla_immune_effects_t* effects
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_immune_update_medulla_to_immune: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bridge->config.enable_medulla_to_immune) return NIMCP_SUCCESS;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current medulla state */
    medulla_stats_t medulla_stats;
    if (medulla_get_stats(bridge->medulla, &medulla_stats) != 0) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Compute arousal-based immune factor */
    float arousal = medulla_stats.current_arousal;
    float arousal_immune = 1.0f;

    if (arousal < AROUSAL_LOW_THRESHOLD) {
        /* Low arousal → immune depression */
        arousal_immune = 0.5f + (arousal / AROUSAL_LOW_THRESHOLD) * 0.5f;
    } else if (arousal > AROUSAL_HIGH_THRESHOLD) {
        /* High arousal → enhanced surveillance */
        float high_factor = (arousal - AROUSAL_HIGH_THRESHOLD) /
            (1.0f - AROUSAL_HIGH_THRESHOLD);
        arousal_immune = 1.0f + high_factor * 0.5f;
    }

    arousal_immune *= bridge->config.arousal_coupling;

    /* Compute protection-based immune factor */
    protection_level_t prot_level = medulla_get_protection_level(bridge->medulla);
    float protection_immune = medulla_immune_compute_protection_immune(prot_level);
    protection_immune = 1.0f + (protection_immune - 1.0f) * bridge->config.protection_coupling;

    /* Compute circadian-based immune factor */
    float circadian_immune = 1.0f;
    if (bridge->config.enable_circadian_modulation) {
        circadian_phase_t phase = medulla_get_circadian_phase(bridge->medulla);
        circadian_immune = medulla_immune_compute_circadian_immune(phase);
        circadian_immune = 1.0f + (circadian_immune - 1.0f) * bridge->config.circadian_coupling;
        bridge->immune_effects.circadian_phase = phase;
    }

    /* Compute combined factor */
    float combined = arousal_immune * protection_immune * circadian_immune;

    /* Determine if surveillance should be enhanced */
    bool enhance = (prot_level >= PROTECTION_LEVEL_CAUTIOUS) ||
                   (arousal > AROUSAL_HIGH_THRESHOLD);

    /* Update cached effects */
    bridge->immune_effects.arousal_immune_factor = arousal_immune;
    bridge->immune_effects.protection_immune_factor = protection_immune;
    bridge->immune_effects.circadian_immune_factor = circadian_immune;
    bridge->immune_effects.combined_immune_factor = combined;
    bridge->immune_effects.arousal_level = arousal;
    bridge->immune_effects.protection_level = prot_level;
    bridge->immune_effects.enhance_surveillance = enhance;

    /* Apply effects to immune system */
    /* In a full implementation, we would modulate immune activity based on these factors */
    bridge->stats.medulla_to_immune_count++;

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

//=============================================================================
// Combined Update
//=============================================================================

int medulla_immune_update(medulla_immune_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_immune_update: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    int result;

    /* Update immune → medulla */
    result = medulla_immune_update_immune_to_medulla(bridge, NULL);
    if (result != NIMCP_SUCCESS) {
        NIMCP_LOGGING_WARN("Immune→Medulla update failed: %d", result);
    }

    /* Update medulla → immune */
    result = medulla_immune_update_medulla_to_immune(bridge, NULL);
    if (result != NIMCP_SUCCESS) {
        NIMCP_LOGGING_WARN("Medulla→Immune update failed: %d", result);
    }

    /* Increment update counter */
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->stats.total_updates++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Query Functions
//=============================================================================

int medulla_immune_get_cytokine_effects(
    medulla_immune_bridge_t bridge,
    medulla_cytokine_effects_t* effects
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_immune_get_cytokine_effects: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_immune_get_cytokine_effects: effects is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->cytokine_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int medulla_immune_get_immune_effects(
    medulla_immune_bridge_t bridge,
    medulla_immune_effects_t* effects
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_immune_get_immune_effects: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_immune_get_immune_effects: effects is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->immune_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int medulla_immune_get_stats(
    medulla_immune_bridge_t bridge,
    medulla_immune_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_immune_get_stats: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_immune_get_stats: stats is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Bio-Async Connection
//=============================================================================

int medulla_immune_connect_bio_async(medulla_immune_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_immune_connect_bio_async: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (bridge->bio_async_connected) return NIMCP_SUCCESS;

    /* Register with bio-router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_BRAIN + 0x100,  /* Medulla-immune bridge ID */
        .module_name = "medulla_immune_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->bio_async_connected = true;
        NIMCP_LOGGING_DEBUG("Medulla-immune bridge connected to bio-async");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    return NIMCP_SUCCESS;
}

int medulla_immune_disconnect_bio_async(medulla_immune_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_immune_disconnect_bio_async: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bridge->bio_async_connected) return NIMCP_SUCCESS;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->bio_async_connected = false;
    NIMCP_LOGGING_DEBUG("Medulla-immune bridge disconnected from bio-async");

    return NIMCP_SUCCESS;
}

bool medulla_immune_is_bio_async_connected(medulla_immune_bridge_t bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->bio_async_connected;
}
