/**
 * @file nimcp_metabolic_immune_bridge.c
 * @brief Metabolic Plasticity-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-19
 */

#include "plasticity/metabolic/nimcp_metabolic_immune_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "security/nimcp_bbb_helpers.h"

#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(metabolic_immune_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(metabolic_immune_bridge)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current inflammation level
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune) return INFLAMMATION_NONE;
    /* Would query immune system inflammation_sites */
    /* For now, return NONE - actual implementation would check max level */
    return INFLAMMATION_NONE;
}

/**
 * @brief Map inflammation level to energy cost multiplier
 */
static float inflammation_to_cost_multiplier(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_COST_NONE;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_COST_LOCAL;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_COST_REGIONAL;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_COST_SYSTEMIC;
        case INFLAMMATION_STORM:    return INFLAMMATION_COST_STORM;
        default:                    return INFLAMMATION_COST_NONE;
    }
}

/**
 * @brief Map inflammation level to recovery impairment
 */
static float inflammation_to_recovery_factor(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_RECOVERY_NONE;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_RECOVERY_LOCAL;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_RECOVERY_REGIONAL;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_RECOVERY_SYSTEMIC;
        case INFLAMMATION_STORM:    return INFLAMMATION_RECOVERY_STORM;
        default:                    return INFLAMMATION_RECOVERY_NONE;
    }
}

/**
 * @brief Calculate immune capacity from ATP level
 */
static float atp_to_immune_capacity(float atp_level) {
    if (atp_level >= METABOLIC_IMMUNE_ATP_HEALTHY) {
        return 1.0f;  /* Full immune capacity */
    } else if (atp_level >= METABOLIC_IMMUNE_ATP_IMPAIRED) {
        /* Linear decline from 70% to 50% ATP */
        float range = METABOLIC_IMMUNE_ATP_HEALTHY - METABOLIC_IMMUNE_ATP_IMPAIRED;
        float offset = atp_level - METABOLIC_IMMUNE_ATP_IMPAIRED;
        return 0.5f + 0.5f * (offset / range);
    } else if (atp_level >= METABOLIC_IMMUNE_ATP_SUPPRESSED) {
        /* Severe decline from 50% to 30% ATP */
        float range = METABOLIC_IMMUNE_ATP_IMPAIRED - METABOLIC_IMMUNE_ATP_SUPPRESSED;
        float offset = atp_level - METABOLIC_IMMUNE_ATP_SUPPRESSED;
        return 0.2f + 0.3f * (offset / range);
    } else {
        /* Critical suppression below 30% */
        return 0.2f * (atp_level / METABOLIC_IMMUNE_ATP_SUPPRESSED);
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int metabolic_immune_default_config(metabolic_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled by default */
    config->enable_cytokine_metabolic_burden = true;
    config->enable_inflammation_cost_increase = true;
    config->enable_recovery_impairment = true;
    config->enable_atp_immune_feedback = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->atp_feedback_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    return 0;
}

metabolic_immune_bridge_t* metabolic_immune_bridge_create(
    const metabolic_immune_config_t* config,
    brain_immune_system_t* immune_system,
    metabolic_plasticity_t* metabolic
) {
    /* Guard: require immune system and metabolic system */
    if (!immune_system || !metabolic) {
        NIMCP_LOGGING_ERROR("Cannot create metabolic-immune bridge without both systems");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_immune_bridge_create: required parameter is NULL (immune_system, metabolic)");
        return NULL;
    }

    /* Allocate bridge */
    metabolic_immune_bridge_t* bridge = (metabolic_immune_bridge_t*)
        nimcp_malloc(sizeof(metabolic_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "metabolic_immune_bridge_create: bridge allocation failed");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(metabolic_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->metabolic = metabolic;

    /* Initialize default effect values (1.0 = no modification) */
    bridge->cytokine_effects.total_cost_multiplier = 1.0f;
    bridge->inflammation_state.energy_cost_increase = 1.0f;
    bridge->inflammation_state.recovery_impairment = 1.0f;
    bridge->atp_effects.immune_capacity = 1.0f;
    bridge->atp_effects.cytokine_production = 1.0f;

    /* Apply configuration */
    if (config) {
        bridge->enable_cytokine_metabolic_burden = config->enable_cytokine_metabolic_burden;
        bridge->enable_inflammation_cost_increase = config->enable_inflammation_cost_increase;
        bridge->enable_recovery_impairment = config->enable_recovery_impairment;
        bridge->enable_atp_immune_feedback = config->enable_atp_immune_feedback;
    } else {
        metabolic_immune_config_t default_cfg;
        metabolic_immune_default_config(&default_cfg);
        bridge->enable_cytokine_metabolic_burden = default_cfg.enable_cytokine_metabolic_burden;
        bridge->enable_inflammation_cost_increase = default_cfg.enable_inflammation_cost_increase;
        bridge->enable_recovery_impairment = default_cfg.enable_recovery_impairment;
        bridge->enable_atp_immune_feedback = default_cfg.enable_atp_immune_feedback;
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "metabolic_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);    return NULL;
    }

    NIMCP_LOGGING_INFO("Metabolic-immune bridge created successfully");
    return bridge;
}

void metabolic_immune_bridge_destroy(metabolic_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        metabolic_immune_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        nimcp_mutex_destroy((nimcp_mutex_t*)bridge->base.mutex);
    }

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Metabolic-immune bridge destroyed");
}

/* ============================================================================
 * Immune → Metabolic Implementation
 * ============================================================================ */

int metabolic_immune_apply_cytokine_effects(metabolic_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_cytokine_metabolic_burden) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_immune_apply_cytokine_effects: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Compute cytokine effects */
    cytokine_metabolic_effects_t* effects = &bridge->cytokine_effects;

    /* Pro-inflammatory cytokines → metabolic burden */
    /* Note: Would query actual cytokine levels from immune system */
    effects->il1_burden = CYTOKINE_IL1_METABOLIC_BURDEN;
    effects->il6_burden = CYTOKINE_IL6_METABOLIC_BURDEN;
    effects->tnf_burden = CYTOKINE_TNF_METABOLIC_BURDEN;
    effects->ifn_gamma_burden = CYTOKINE_IFN_GAMMA_METABOLIC_BURDEN;

    /* Anti-inflammatory cytokines → metabolic relief */
    effects->il10_relief = CYTOKINE_IL10_METABOLIC_RELIEF;

    /* Aggregate baseline increase (additive) */
    effects->total_baseline_increase =
        effects->il1_burden +
        effects->il6_burden +
        effects->tnf_burden +
        effects->ifn_gamma_burden +
        effects->il10_relief;

    /* Cost multiplier (1.0 + burden) */
    effects->total_cost_multiplier = 1.0f + effects->total_baseline_increase;
    effects->total_cost_multiplier = nimcp_clampf(effects->total_cost_multiplier, 0.5f, 3.0f);

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int metabolic_immune_apply_inflammation_effects(metabolic_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_inflammation_cost_increase) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_immune_apply_inflammation_effects: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Get current inflammation level */
    brain_inflammation_level_t level = get_max_inflammation_level(bridge->immune_system);
    bridge->inflammation_state.current_level = level;

    /* Map to cost increase */
    bridge->inflammation_state.energy_cost_increase =
        inflammation_to_cost_multiplier(level);

    /* Map to recovery impairment */
    if (bridge->enable_recovery_impairment) {
        bridge->inflammation_state.recovery_impairment =
            inflammation_to_recovery_factor(level);
    } else {
        bridge->inflammation_state.recovery_impairment = 1.0f;
    }

    /* Check for mitochondrial impairment (regional+) */
    bridge->inflammation_state.mitochondrial_impaired =
        (level >= INFLAMMATION_REGIONAL);

    /* Update statistics */
    if (bridge->inflammation_state.energy_cost_increase > 1.0f) {
        bridge->inflammation_cost_increases++;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

float metabolic_immune_get_effective_cost(
    const metabolic_immune_bridge_t* bridge,
    float base_cost
) {
    if (!bridge) return base_cost;

    float cost = base_cost;

    /* Apply cytokine cost multiplier */
    if (bridge->enable_cytokine_metabolic_burden) {
        cost *= bridge->cytokine_effects.total_cost_multiplier;
    }

    /* Apply inflammation cost multiplier */
    if (bridge->enable_inflammation_cost_increase) {
        cost *= bridge->inflammation_state.energy_cost_increase;
    }

    return cost;
}

float metabolic_immune_get_effective_recovery(
    const metabolic_immune_bridge_t* bridge,
    float base_rate
) {
    if (!bridge) return base_rate;
    if (!bridge->enable_recovery_impairment) return base_rate;

    /* Apply inflammation recovery impairment */
    return base_rate * bridge->inflammation_state.recovery_impairment;
}

/* ============================================================================
 * Metabolic → Immune Implementation
 * ============================================================================ */

int metabolic_immune_update_atp_effects(metabolic_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_atp_immune_feedback) return 0;
    if (!bridge->metabolic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_immune_update_atp_effects: bridge->metabolic is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Get current ATP state */
    float atp_level = metabolic_plasticity_get_atp_level(bridge->metabolic);
    energy_state_t energy_state = metabolic_plasticity_get_energy_state(bridge->metabolic);

    bridge->atp_effects.atp_level = atp_level;
    bridge->atp_effects.energy_state = energy_state;

    /* Calculate immune capacity from ATP */
    bridge->atp_effects.immune_capacity = atp_to_immune_capacity(atp_level);

    /* Cytokine production capacity (requires ATP) */
    bridge->atp_effects.cytokine_production = bridge->atp_effects.immune_capacity;

    /* Check for suppression */
    bool was_suppressed = bridge->atp_effects.immune_suppressed;
    bridge->atp_effects.immune_suppressed = (atp_level < METABOLIC_IMMUNE_ATP_SUPPRESSED);

    /* Count suppression events */
    if (bridge->atp_effects.immune_suppressed && !was_suppressed) {
        bridge->immune_suppression_events++;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

bool metabolic_immune_is_impaired_by_atp(const metabolic_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    if (!bridge->enable_atp_immune_feedback) {
        return false;
    }

    return (bridge->atp_effects.atp_level < METABOLIC_IMMUNE_ATP_IMPAIRED);
}

float metabolic_immune_get_immune_capacity(const metabolic_immune_bridge_t* bridge) {
    if (!bridge) return 1.0f;
    if (!bridge->enable_atp_immune_feedback) return 1.0f;

    return bridge->atp_effects.immune_capacity;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int metabolic_immune_bridge_update(
    metabolic_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Update inflammation duration */
    if (bridge->inflammation_state.current_level > INFLAMMATION_NONE) {
        bridge->inflammation_state.inflammation_duration_sec += delta_ms / 1000.0f;
    } else {
        bridge->inflammation_state.inflammation_duration_sec = 0.0f;
    }

    /* Update statistics */
    bridge->total_updates++;

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    /* Apply effects (both directions) */
    metabolic_immune_apply_cytokine_effects(bridge);
    metabolic_immune_apply_inflammation_effects(bridge);
    metabolic_immune_update_atp_effects(bridge);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int metabolic_immune_get_cytokine_effects(
    const metabolic_immune_bridge_t* bridge,
    cytokine_metabolic_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(effects, &bridge->cytokine_effects, sizeof(cytokine_metabolic_effects_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int metabolic_immune_get_inflammation_state(
    const metabolic_immune_bridge_t* bridge,
    inflammation_metabolic_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(state, &bridge->inflammation_state, sizeof(inflammation_metabolic_state_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int metabolic_immune_get_atp_effects(
    const metabolic_immune_bridge_t* bridge,
    atp_immune_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_immune_get_atp_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(effects, &bridge->atp_effects, sizeof(atp_immune_effects_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

int metabolic_immune_connect_bio_async(metabolic_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_METABOLIC,
        .module_name = "metabolic_immune_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metabolic_immune_connect_bio_async: validation failed");
        return -1;
    }
}

int metabolic_immune_disconnect_bio_async(metabolic_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

bool metabolic_immune_is_bio_async_connected(const metabolic_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
