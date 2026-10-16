/**
 * @file nimcp_protein_immune_bridge.c
 * @brief Implementation of Protein Synthesis-Immune System Integration Bridge
 *
 * WHAT: Bidirectional integration between brain immune and protein synthesis
 * WHY:  Pro-inflammatory cytokines suppress protein synthesis
 * HOW:  Inflammation modulates PRP synthesis rate, prevents consolidation
 *
 * @author NIMCP Development Team
 * @date 2025-12-19
 */

#include "plasticity/protein/nimcp_protein_immune_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(protein_immune_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(protein_immune_bridge)


/* Temporary module ID until added to nimcp_bio_messages.h */
#ifndef BIO_MODULE_IMMUNE_PROTEIN
#define BIO_MODULE_IMMUNE_PROTEIN 0x0D31  /* After STDP */
#endif

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Get synthesis suppression factor for inflammation level
 * WHY:  Map inflammation severity to synthesis reduction
 * HOW:  Return predefined factor for level
 */
static float get_inflammation_synthesis_factor(brain_inflammation_level_t level) {
    float cont = inflammation_level_to_continuous(level);
    return inflammation_compute_factor(cont,
        INFLAM_SYNTHESIS_NONE,
        INFLAM_SYNTHESIS_STORM);
}

/**
 * WHAT: Compute cytokine effect from concentration
 * WHY:  Scale effect by cytokine level
 * HOW:  Linear interpolation from threshold
 */
static float compute_cytokine_effect(
    float concentration,
    float base_effect
) {
    if (concentration < CYTOKINE_THRESHOLD_LOW) {
        return 1.0f;  /* No effect */
    }

    /* Clamp concentration */
    float clamped = fminf(concentration, 1.0f);

    /* Scale effect by concentration */
    return 1.0f + (base_effect - 1.0f) * clamped;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int protein_immune_default_config(protein_immune_config_t* config) {
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_immune_default_config: config is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Feature enables */
    config->enable_cytokine_suppression = true;
    config->enable_inflammation_impairment = true;
    config->enable_tag_decay_modulation = true;

    /* Sensitivity */
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Thresholds */
    config->chronic_inflammation_threshold_sec = 86400.0f * 7.0f;  /* 7 days */

    return 0;
}

protein_immune_bridge_t* protein_immune_bridge_create(
    const protein_immune_config_t* config,
    brain_immune_system_t* immune_system,
    protein_synthesis_system_t protein_system
) {
    if (!immune_system) {
        NIMCP_LOGGING_ERROR("NULL immune_system pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_immune_bridge_create: immune_system is NULL");
        return NULL;
    }
    if (!protein_system) {
        NIMCP_LOGGING_ERROR("NULL protein_system pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_immune_bridge_create: protein_system is NULL");
        return NULL;
    }

    /* Allocate bridge */
    protein_immune_bridge_t* bridge = (protein_immune_bridge_t*)
        nimcp_calloc(1, sizeof(protein_immune_bridge_t));

    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate protein-immune bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "protein_immune_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        protein_immune_default_config(&bridge->config);
    }

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->protein_system = protein_system;

    /* Initialize state */
    memset(&bridge->cytokine_effects, 0, sizeof(cytokine_protein_effects_t));
    memset(&bridge->inflammation_state, 0, sizeof(inflammation_protein_state_t));

    bridge->cytokine_effects.total_modulation = 1.0f;
    bridge->inflammation_state.synthesis_suppression = 1.0f;

    /* Initialize statistics */
    bridge->total_updates = 0;
    bridge->synthesis_suppressions = 0;
    bridge->consolidation_failures = 0;
    bridge->restoration_events = 0;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "protein_immune") != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize bridge base");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "protein_immune_bridge_create: bridge_base_init failed");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "protein_immune_bridge_create: failed to create mutex");
        nimcp_free(bridge);
        return NULL;
    }

    /* Bio-async disabled by default */
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Created protein-immune bridge");
    return bridge;
}

void protein_immune_bridge_destroy(protein_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        protein_immune_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Destroyed protein-immune bridge");
}

/* ============================================================================
 * Immune → Protein Synthesis API Implementation
 * ============================================================================ */

int protein_immune_apply_cytokine_effects(protein_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_immune_apply_cytokine_effects: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Query immune system for cytokine levels */
    brain_immune_stats_t immune_stats;
    if (brain_immune_get_stats(bridge->immune_system, &immune_stats) != 0) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Compute individual cytokine effects */
    bridge->cytokine_effects.il1_suppression = compute_cytokine_effect(
        immune_stats.cytokine_il1,
        CYTOKINE_IL1_SYNTHESIS_SUPPRESSION
    );

    bridge->cytokine_effects.il6_suppression = compute_cytokine_effect(
        immune_stats.cytokine_il6,
        CYTOKINE_IL6_SYNTHESIS_SUPPRESSION
    );

    bridge->cytokine_effects.tnf_suppression = compute_cytokine_effect(
        immune_stats.cytokine_tnf,
        CYTOKINE_TNF_SYNTHESIS_SUPPRESSION
    );

    bridge->cytokine_effects.ifn_gamma_suppression = compute_cytokine_effect(
        immune_stats.cytokine_ifn_gamma,
        CYTOKINE_IFN_GAMMA_SYNTHESIS_SUPPRESSION
    );

    bridge->cytokine_effects.il10_restoration = compute_cytokine_effect(
        immune_stats.cytokine_il10,
        CYTOKINE_IL10_SYNTHESIS_RESTORATION
    );

    /* Compute total modulation (multiplicative) */
    float total = 1.0f;

    if (bridge->config.enable_cytokine_suppression) {
        total *= bridge->cytokine_effects.il1_suppression;
        total *= bridge->cytokine_effects.il6_suppression;
        total *= bridge->cytokine_effects.tnf_suppression;
        total *= bridge->cytokine_effects.ifn_gamma_suppression;
        total *= bridge->cytokine_effects.il10_restoration;
    }

    /* Apply sensitivity */
    total = 1.0f + (total - 1.0f) * bridge->config.cytokine_sensitivity;

    bridge->cytokine_effects.total_modulation = total;

    /* Apply to protein synthesis system */
    prp_pool_state_t prp_state;
    if (protein_synthesis_get_prp_state(bridge->protein_system, &prp_state) == 0) {
        /* Update immune suppression factor */
        prp_state.immune_suppression = total;
    }

    if (total < 1.0f) {
        bridge->synthesis_suppressions++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied cytokine effects: modulation=%.2f", total);
    return 0;
}

int protein_immune_apply_inflammation_effects(
    protein_immune_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_immune_apply_inflammation_effects: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Query immune system for inflammation state */
    brain_immune_stats_t immune_stats;
    if (brain_immune_get_stats(bridge->immune_system, &immune_stats) != 0) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Update inflammation state */
    bridge->inflammation_state.current_level = immune_stats.inflammation_level;

    /* Get inflammation-based suppression */
    float inflam_factor = 1.0f;
    if (bridge->config.enable_inflammation_impairment) {
        inflam_factor = get_inflammation_synthesis_factor(
            immune_stats.inflammation_level
        );

        /* Apply sensitivity */
        inflam_factor = 1.0f + (inflam_factor - 1.0f) *
                              bridge->config.inflammation_sensitivity;
    }

    bridge->inflammation_state.synthesis_suppression = inflam_factor;

    /* Track chronic inflammation */
    /* Note: Would need immune system to track duration, using placeholder */
    bridge->inflammation_state.is_chronic = false;
    bridge->inflammation_state.inflammation_duration_sec = 0.0f;

    /* Compute failure rate (tags expire before capture) */
    if (immune_stats.inflammation_level >= INFLAMMATION_REGIONAL) {
        bridge->inflammation_state.consolidation_failure_rate =
            (1.0f - inflam_factor) * 0.8f;  /* Up to 80% failure in storm */
    } else {
        bridge->inflammation_state.consolidation_failure_rate = 0.0f;
    }

    /* Tag decay acceleration during inflammation */
    if (bridge->config.enable_tag_decay_modulation) {
        if (immune_stats.inflammation_level >= INFLAMMATION_SYSTEMIC) {
            bridge->inflammation_state.tag_decay_acceleration = 1.5f;
        } else {
            bridge->inflammation_state.tag_decay_acceleration = 1.0f;
        }
    } else {
        bridge->inflammation_state.tag_decay_acceleration = 1.0f;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied inflammation effects: suppression=%.2f",
                        inflam_factor);
    return 0;
}

float protein_immune_get_effective_synthesis_rate(
    const protein_immune_bridge_t* bridge,
    float base_rate
) {
    if (!bridge) return base_rate;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Combine cytokine and inflammation effects */
    float effective_rate = base_rate;
    effective_rate *= bridge->cytokine_effects.total_modulation;
    effective_rate *= bridge->inflammation_state.synthesis_suppression;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return effective_rate;
}

int protein_immune_restore_synthesis(
    protein_immune_bridge_t* bridge,
    float recovery_factor
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_immune_restore_synthesis: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (recovery_factor < 0.0f || recovery_factor > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "protein_immune_restore_synthesis: recovery_factor out of range");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Interpolate suppression back to normal */
    float target_modulation = 1.0f;
    float current_modulation = bridge->cytokine_effects.total_modulation;

    bridge->cytokine_effects.total_modulation =
        current_modulation + (target_modulation - current_modulation) * recovery_factor;

    bridge->inflammation_state.synthesis_suppression =
        bridge->inflammation_state.synthesis_suppression +
        (1.0f - bridge->inflammation_state.synthesis_suppression) * recovery_factor;

    bridge->restoration_events++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Restored synthesis (recovery=%.2f)", recovery_factor);
    return 0;
}

/* ============================================================================
 * Bidirectional Update API Implementation
 * ============================================================================ */

int protein_immune_bridge_update(
    protein_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_immune_bridge_update: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->total_updates++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Apply both effect pathways */
    protein_immune_apply_cytokine_effects(bridge);
    protein_immune_apply_inflammation_effects(bridge);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

int protein_immune_get_cytokine_effects(
    const protein_immune_bridge_t* bridge,
    cytokine_protein_effects_t* effects
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_immune_get_cytokine_effects: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_immune_get_cytokine_effects: effects is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->cytokine_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int protein_immune_get_inflammation_state(
    const protein_immune_bridge_t* bridge,
    inflammation_protein_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_immune_get_inflammation_state: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_immune_get_inflammation_state: state is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *state = bridge->inflammation_state;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool protein_immune_is_synthesis_impaired(
    const protein_immune_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bool impaired = (bridge->cytokine_effects.total_modulation < 1.0f) ||
                    (bridge->inflammation_state.synthesis_suppression < 1.0f);

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return impaired;
}

float protein_immune_get_synthesis_reduction(
    const protein_immune_bridge_t* bridge
) {
    if (!bridge) return 0.0f;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute combined suppression */
    float combined_factor = bridge->cytokine_effects.total_modulation *
                            bridge->inflammation_state.synthesis_suppression;

    float reduction = (1.0f - combined_factor) * 100.0f;
    reduction = fmaxf(0.0f, reduction);

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return reduction;
}

/* ============================================================================
 * Bio-Async Integration API Implementation
 * ============================================================================ */

int protein_immune_connect_bio_async(protein_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_immune_connect_bio_async: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (bridge->base.bio_async_enabled) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_PROTEIN,
        .module_name = "protein_immune_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int protein_immune_disconnect_bio_async(protein_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_immune_disconnect_bio_async: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (!bridge->base.bio_async_enabled) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

bool protein_immune_is_bio_async_connected(
    const protein_immune_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bool connected = bridge->base.bio_async_enabled;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return connected;
}
