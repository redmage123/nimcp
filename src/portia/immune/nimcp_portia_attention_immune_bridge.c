/**
 * @file nimcp_portia_attention_immune_bridge.c
 * @brief Portia Attention-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "portia/immune/nimcp_portia_attention_immune_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(portia_attention_immune_bridge)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Compute cytokine resource allocation effects
 *
 * WHAT: Calculate resource budget reduction from cytokine levels
 * WHY:  Immune activation consumes metabolic resources
 * HOW:  Query immune cytokines, compute weighted impact on budget
 */
static void compute_cytokine_attention_effects(portia_attention_immune_bridge_t* bridge) {
    /* Guard clause */
    if (!bridge || !bridge->immune_system) {
        return;
    }

    /* Reset effects */
    memset(&bridge->cytokine_effects, 0, sizeof(cytokine_attention_effects_t));

    /* Use inflammation as proxy for cytokine levels */
    brain_inflammation_level_t inflam = INFLAMMATION_NONE;
    float inflam_factor = 1.0f;

    /* Compute individual cytokine effects on resource budget */
    bridge->cytokine_effects.il1_budget_reduction =
        CYTOKINE_IL1_BUDGET_IMPACT * inflam_factor;
    bridge->cytokine_effects.il6_budget_reduction =
        CYTOKINE_IL6_BUDGET_IMPACT * inflam_factor;
    bridge->cytokine_effects.tnf_budget_reduction =
        CYTOKINE_TNF_BUDGET_IMPACT * inflam_factor;
    bridge->cytokine_effects.ifn_gamma_budget_reduction =
        CYTOKINE_IFN_GAMMA_BUDGET_IMPACT * inflam_factor;

    /* Anti-inflammatory recovery */
    bridge->cytokine_effects.il10_budget_recovery =
        CYTOKINE_IL10_BUDGET_RECOVERY * (1.0f - inflam_factor);

    /* Compute total budget factor */
    float total_reduction =
        fabsf(bridge->cytokine_effects.il1_budget_reduction) +
        fabsf(bridge->cytokine_effects.il6_budget_reduction) +
        fabsf(bridge->cytokine_effects.tnf_budget_reduction) +
        fabsf(bridge->cytokine_effects.ifn_gamma_budget_reduction);

    total_reduction -= bridge->cytokine_effects.il10_budget_recovery;
    total_reduction *= bridge->config.cytokine_sensitivity;

    bridge->cytokine_effects.total_budget_factor =
        fmaxf(0.0f, fminf(1.0f, 1.0f - total_reduction));

    /* Other allocation effects */
    bridge->cytokine_effects.allocation_efficiency_loss = total_reduction * 0.6f;
    bridge->cytokine_effects.reallocation_delay = total_reduction * 0.4f;
    bridge->cytokine_effects.hysteresis_increase = total_reduction * 0.3f;
}

/**
 * @brief Compute inflammation resource allocation effects
 *
 * WHAT: Calculate allocation impairment from inflammation
 * WHY:  Inflammation reduces cognitive resources and flexibility
 * HOW:  Map inflammation level to allocation parameter reductions
 */
static void compute_inflammation_attention_effects(portia_attention_immune_bridge_t* bridge) {
    /* Guard clause */
    if (!bridge || !bridge->immune_system) {
        return;
    }

    /* Get inflammation level */
    brain_inflammation_level_t level = INFLAMMATION_NONE;
    bridge->inflammation_state.current_level = level;

    /* Map inflammation to budget factor */
    switch (level) {
        case INFLAMMATION_NONE:
            bridge->inflammation_state.budget_factor = INFLAMMATION_NONE_BUDGET_FACTOR;
            break;
        case INFLAMMATION_LOCAL:
            bridge->inflammation_state.budget_factor = INFLAMMATION_LOCAL_BUDGET_FACTOR;
            break;
        case INFLAMMATION_REGIONAL:
            bridge->inflammation_state.budget_factor = INFLAMMATION_REGIONAL_BUDGET_FACTOR;
            break;
        case INFLAMMATION_SYSTEMIC:
            bridge->inflammation_state.budget_factor = INFLAMMATION_SYSTEMIC_BUDGET_FACTOR;
            break;
        case INFLAMMATION_STORM:
            bridge->inflammation_state.budget_factor = INFLAMMATION_STORM_BUDGET_FACTOR;
            break;
        default:
            bridge->inflammation_state.budget_factor = 1.0f;
    }

    /* Apply sensitivity */
    float reduction = 1.0f - bridge->inflammation_state.budget_factor;
    reduction *= bridge->config.inflammation_sensitivity;
    bridge->inflammation_state.budget_factor = 1.0f - reduction;

    /* Efficiency loss */
    bridge->inflammation_state.efficiency_loss =
        INFLAMMATION_EFFICIENCY_BASE +
        (INFLAMMATION_EFFICIENCY_PER_LEVEL * (float)level);

    /* Other effects scale with inflammation severity */
    float severity = (float)level / (float)INFLAMMATION_STORM;
    bridge->inflammation_state.priority_impairment = severity * 0.5f;
    bridge->inflammation_state.reallocation_slowdown = severity * 0.6f;

    /* Allocation biases */
    bridge->inflammation_state.threat_allocation_bias = severity * 0.4f;
    bridge->inflammation_state.flexibility_reduction = severity * 0.5f;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int portia_attention_immune_default_config(portia_attention_immune_config_t* config) {
    /* Guard clause */
    if (!config) {
        NIMCP_LOGGING_ERROR("Null config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_attention_immune_default_config: config is NULL");
        return -1;
    }

    /* Feature enables */
    config->enable_cytokine_budget_reduction = true;
    config->enable_inflammation_allocation_impairment = true;
    config->enable_resource_depletion_suppression = true;
    config->enable_resource_scarcity_stress = true;
    config->enable_preemption_inflammation = true;

    /* Sensitivity tuning */
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->resource_immune_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Thresholds */
    config->depletion_threshold = RESOURCE_DEPLETION_THRESHOLD;
    config->scarcity_threshold = RESOURCE_SCARCITY_THRESHOLD;
    config->preemption_threshold = RESOURCE_PREEMPTION_THRESHOLD;

    return 0;
}

portia_attention_immune_bridge_t* portia_attention_immune_create(
    const portia_attention_immune_config_t* config,
    brain_immune_system_t* immune_system,
    portia_attention_state_t attention_system
) {
    /* Guard clauses */
    if (!immune_system) {
        NIMCP_LOGGING_ERROR("Null immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_system is NULL");

        return NULL;
    }

    if (!attention_system) {
        NIMCP_LOGGING_ERROR("Null attention system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_system is NULL");


        return NULL;
    }

    /* Allocate bridge */
    portia_attention_immune_bridge_t* bridge =
        (portia_attention_immune_bridge_t*)nimcp_calloc(
            1, sizeof(portia_attention_immune_bridge_t)
        );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_immune_create: allocation failed");

        return NULL;
    }

    /* Store system handles */
    bridge->immune_system = immune_system;
    bridge->attention_system = attention_system;

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        portia_attention_immune_default_config(&bridge->config);
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "portia_attention_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_attention_immune_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->last_update_time = 0;
    bridge->preemption_counter = 0;
    bridge->base.bio_async_enabled = false;

    /* Initialize modulation structure to prevent reading uninitialized values */
    memset(&bridge->attention_modulation, 0, sizeof(bridge->attention_modulation));
    bridge->attention_modulation.available_resources = 1.0f;  /* Full resources by default */

    NIMCP_LOGGING_INFO("Created Portia attention-immune bridge");
    return bridge;
}

void portia_attention_immune_destroy(portia_attention_immune_bridge_t* bridge) {
    /* Guard clause */
    if (!bridge) {
        return;
    }

    /* bridge_base_cleanup handles bio-async disconnect + mutex destroy+free */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Destroyed Portia attention-immune bridge");
}

/* ============================================================================
 * Immune → Attention API Implementation
 * ============================================================================ */

int portia_attention_immune_apply_cytokine_effects(portia_attention_immune_bridge_t* bridge) {
    /* Guard clause */
    if (!bridge || !bridge->attention_system) {
        NIMCP_LOGGING_ERROR("Invalid bridge or attention system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_attention_immune_apply_cytokine_effects: required parameter is NULL (bridge, bridge->attention_system)");
        return -1;
    }

    if (!bridge->config.enable_cytokine_budget_reduction) {
        return 0;  /* Feature disabled */
    }

    /* Lock */
    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute cytokine effects */
    compute_cytokine_attention_effects(bridge);

    /* Apply budget factor to attention system */
    float budget_factor = bridge->cytokine_effects.total_budget_factor;

    /* Would apply to attention system here */
    /* portia_attention_scale_budget(bridge->attention_system, budget_factor); */

    bridge->cytokine_impairments++;
    bridge->total_updates++;

    /* Unlock */
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied cytokine effects: budget_factor=%.3f", budget_factor);
    return 0;
}

int portia_attention_immune_apply_inflammation_effects(
    portia_attention_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge || !bridge->attention_system) {
        NIMCP_LOGGING_ERROR("Invalid bridge or attention system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_attention_immune_apply_inflammation_effects: required parameter is NULL (bridge, bridge->attention_system)");
        return -1;
    }

    if (!bridge->config.enable_inflammation_allocation_impairment) {
        return 0;  /* Feature disabled */
    }

    /* Lock */
    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute inflammation effects */
    compute_inflammation_attention_effects(bridge);

    /* Apply effects */
    float budget_factor = bridge->inflammation_state.budget_factor;
    float efficiency_loss = bridge->inflammation_state.efficiency_loss;

    /* Would apply to attention system here */
    /* portia_attention_scale_budget(bridge->attention_system, budget_factor); */
    /* portia_attention_reduce_efficiency(bridge->attention_system, efficiency_loss); */

    bridge->total_updates++;

    /* Unlock */
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied inflammation effects: budget=%.3f, efficiency_loss=%.3f",
                       budget_factor, efficiency_loss);
    return 0;
}

float portia_attention_immune_compute_budget_factor(
    const portia_attention_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge) {
        return 1.0f;
    }

    /* Combine cytokine and inflammation effects */
    float cytokine_factor = bridge->cytokine_effects.total_budget_factor;
    float inflam_factor = bridge->inflammation_state.budget_factor;

    return fminf(cytokine_factor, inflam_factor);
}

float portia_attention_immune_compute_efficiency_loss(
    const portia_attention_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge) {
        return 0.0f;
    }

    /* Combine cytokine and inflammation efficiency losses */
    float cytokine_loss = bridge->cytokine_effects.allocation_efficiency_loss;
    float inflam_loss = bridge->inflammation_state.efficiency_loss;

    return fmaxf(cytokine_loss, inflam_loss);
}

/* ============================================================================
 * Attention → Immune Internal Helpers (Unlocked versions)
 * ============================================================================
 * These functions perform the actual work WITHOUT acquiring the mutex.
 * Caller MUST hold bridge->base.mutex before calling.
 * This prevents deadlocks when update() calls these functions.
 * ============================================================================ */

/**
 * @brief Internal unlocked version - caller MUST hold bridge->base.mutex
 */
static int portia_attention_immune_trigger_depletion_suppression_unlocked(
    portia_attention_immune_bridge_t* bridge
) {
    if (!bridge->config.enable_resource_depletion_suppression) {
        return 0;  /* Feature disabled */
    }

    /* Get resource availability */
    portia_attention_stats_t stats;
    if (portia_attention_get_stats(bridge->attention_system, &stats) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_attention_immune_trigger_depletion_suppression_unlocked: validation failed");
        return -1;
    }

    float available = stats.total_allocated;  /* Proxy for availability */

    if (available < bridge->config.depletion_threshold) {
        /* Trigger metabolic immune suppression */
        float suppression = RESOURCE_DEPLETION_SUPPRESSION *
                           bridge->config.resource_immune_sensitivity;

        bridge->attention_modulation.depletion_immune_suppression = suppression;
        bridge->depletion_suppressions++;

        NIMCP_LOGGING_DEBUG("Resource depletion triggered immune suppression: %.3f",
                           suppression);
    } else {
        bridge->attention_modulation.depletion_immune_suppression = 0.0f;
    }

    return 0;
}

/**
 * @brief Internal unlocked version - caller MUST hold bridge->base.mutex
 */
static int portia_attention_immune_trigger_scarcity_stress_unlocked(
    portia_attention_immune_bridge_t* bridge
) {
    if (!bridge->config.enable_resource_scarcity_stress) {
        return 0;  /* Feature disabled */
    }

    /* Get resource state */
    portia_attention_stats_t stats;
    if (portia_attention_get_stats(bridge->attention_system, &stats) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_attention_immune_trigger_scarcity_stress_unlocked: validation failed");
        return -1;
    }

    float utilization = stats.total_allocated;

    if (utilization > bridge->config.scarcity_threshold) {
        /* Release IL-6 for resource scarcity signaling */
        float il6_amount = RESOURCE_SCARCITY_IL6_RELEASE *
                          bridge->config.resource_immune_sensitivity;

        bridge->attention_modulation.scarcity_stress_level =
            (utilization - bridge->config.scarcity_threshold) /
            (1.0f - bridge->config.scarcity_threshold);

        /* Would release IL-6 here */
        /* brain_immune_release_cytokine(bridge->immune_system, BRAIN_CYTOKINE_IL6,
                                        0, il6_amount, 0, NULL); */

        bridge->scarcity_stresses++;

        NIMCP_LOGGING_DEBUG("Resource scarcity triggered IL-6 release: %.3f", il6_amount);
    }

    return 0;
}

/**
 * @brief Internal unlocked version - caller MUST hold bridge->base.mutex
 */
static int portia_attention_immune_trigger_preemption_inflammation_unlocked(
    portia_attention_immune_bridge_t* bridge
) {
    if (!bridge->config.enable_preemption_inflammation) {
        return 0;  /* Feature disabled */
    }

    /* Check preemption count */
    portia_attention_stats_t stats;
    if (portia_attention_get_stats(bridge->attention_system, &stats) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_attention_immune_trigger_preemption_inflammation_unlocked: validation failed");
        return -1;
    }

    bridge->preemption_counter = (uint32_t)stats.preemptions;

    if (bridge->preemption_counter >= bridge->config.preemption_threshold) {
        /* Trigger chronic inflammation from resource competition */
        float il1_amount = RESOURCE_PREEMPTION_IL1_RELEASE *
                          bridge->config.resource_immune_sensitivity;

        bridge->attention_modulation.chronic_preemption_inflammation = true;
        bridge->preemption_inflammations++;

        /* Would release IL-1β here */
        /* brain_immune_release_cytokine(bridge->immune_system, BRAIN_CYTOKINE_IL1,
                                        0, il1_amount, 0, NULL); */

        NIMCP_LOGGING_WARN("Repeated preemptions triggered inflammation: %.3f", il1_amount);

        /* Reset counter */
        bridge->preemption_counter = 0;
    } else {
        bridge->attention_modulation.chronic_preemption_inflammation = false;
    }

    return 0;
}

/* ============================================================================
 * Attention → Immune API Implementation
 * ============================================================================ */

int portia_attention_immune_trigger_depletion_suppression(
    portia_attention_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge || !bridge->immune_system) {
        NIMCP_LOGGING_ERROR("Invalid bridge or immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_attention_immune_trigger_depletion_suppression: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    /* Lock */
    nimcp_mutex_lock(bridge->base.mutex);

    /* Call unlocked version */
    int result = portia_attention_immune_trigger_depletion_suppression_unlocked(bridge);

    /* Unlock */
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

int portia_attention_immune_trigger_scarcity_stress(portia_attention_immune_bridge_t* bridge) {
    /* Guard clause */
    if (!bridge || !bridge->immune_system) {
        NIMCP_LOGGING_ERROR("Invalid bridge or immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_attention_immune_trigger_scarcity_stress: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    /* Lock */
    nimcp_mutex_lock(bridge->base.mutex);

    /* Call unlocked version */
    int result = portia_attention_immune_trigger_scarcity_stress_unlocked(bridge);

    /* Unlock */
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

int portia_attention_immune_trigger_preemption_inflammation(
    portia_attention_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge || !bridge->immune_system) {
        NIMCP_LOGGING_ERROR("Invalid bridge or immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_attention_immune_trigger_preemption_inflammation: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    /* Lock */
    nimcp_mutex_lock(bridge->base.mutex);

    /* Call unlocked version */
    int result = portia_attention_immune_trigger_preemption_inflammation_unlocked(bridge);

    /* Unlock */
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

/* ============================================================================
 * Bidirectional Update API Implementation
 * ============================================================================ */

int portia_attention_immune_update(
    portia_attention_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Guard clause */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_attention_immune_update: bridge is NULL");
        return -1;
    }

    /* Lock */
    nimcp_mutex_lock(bridge->base.mutex);

    /* Update timing */
    bridge->last_update_time += delta_ms;

    /* Immune → Attention direction */
    if (bridge->config.enable_cytokine_budget_reduction) {
        compute_cytokine_attention_effects(bridge);
    }

    if (bridge->config.enable_inflammation_allocation_impairment) {
        compute_inflammation_attention_effects(bridge);
    }

    /* Attention → Immune direction - use unlocked versions since we already hold the mutex */
    if (bridge->config.enable_resource_depletion_suppression) {
        portia_attention_immune_trigger_depletion_suppression_unlocked(bridge);
    }

    if (bridge->config.enable_resource_scarcity_stress) {
        portia_attention_immune_trigger_scarcity_stress_unlocked(bridge);
    }

    if (bridge->config.enable_preemption_inflammation) {
        portia_attention_immune_trigger_preemption_inflammation_unlocked(bridge);
    }

    bridge->total_updates++;

    /* Unlock */
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

int portia_attention_immune_get_cytokine_effects(
    const portia_attention_immune_bridge_t* bridge,
    cytokine_attention_effects_t* effects
) {
    /* Guard clause */
    if (!bridge || !effects) {
        NIMCP_LOGGING_ERROR("Null bridge or effects");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_attention_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    *effects = bridge->cytokine_effects;
    return 0;
}

int portia_attention_immune_get_inflammation_state(
    const portia_attention_immune_bridge_t* bridge,
    inflammation_attention_state_t* state
) {
    /* Guard clause */
    if (!bridge || !state) {
        NIMCP_LOGGING_ERROR("Null bridge or state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_attention_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    *state = bridge->inflammation_state;
    return 0;
}

bool portia_attention_immune_has_resource_deficit(
    const portia_attention_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge) {
        return false;
    }

    float budget_factor = portia_attention_immune_compute_budget_factor(bridge);
    return (budget_factor < 0.8f);  /* >20% budget loss */
}

float portia_attention_immune_get_budget_factor(
    const portia_attention_immune_bridge_t* bridge
) {
    return portia_attention_immune_compute_budget_factor(bridge);
}

float portia_attention_immune_get_efficiency_loss(
    const portia_attention_immune_bridge_t* bridge
) {
    return portia_attention_immune_compute_efficiency_loss(bridge);
}

/* ============================================================================
 * Bio-Async Integration API Implementation
 * ============================================================================ */

int portia_attention_immune_connect_bio_async(portia_attention_immune_bridge_t* bridge) {
    /* Guard clause */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_attention_immune_connect_bio_async: bridge is NULL");
        return -1;
    }

    /* Would register with bio-async router here */
    /* bio_router_register_module(&bridge->base.bio_ctx, BIO_MODULE_IMMUNE_PORTIA_ATTENTION); */

    bridge->base.bio_async_enabled = true;

    NIMCP_LOGGING_INFO("Connected Portia attention-immune bridge to bio-async");
    return 0;
}

int portia_attention_immune_disconnect_bio_async(portia_attention_immune_bridge_t* bridge) {
    /* Guard clause */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_attention_immune_disconnect_bio_async: bridge is NULL");
        return -1;
    }

    if (!bridge->base.bio_async_enabled) {
        return 0;  /* Not connected */
    }

    /* Would unregister from bio-async router here */
    /* bio_router_unregister_module(&bridge->base.bio_ctx); */

    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Disconnected Portia attention-immune bridge from bio-async");
    return 0;
}

bool portia_attention_immune_is_bio_async_connected(
    const portia_attention_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge) {
        return false;
    }

    return bridge->base.bio_async_enabled;
}
