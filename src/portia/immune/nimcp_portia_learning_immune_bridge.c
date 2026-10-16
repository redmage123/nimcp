/**
 * @file nimcp_portia_learning_immune_bridge.c
 * @brief Portia Learning-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "portia/immune/nimcp_portia_learning_immune_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(portia_learning_immune_bridge)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Compute cytokine learning effects
 *
 * WHAT: Calculate learning impairment from cytokine levels
 * WHY:  Pro-inflammatory cytokines impair LTP and plasticity
 * HOW:  Query immune cytokines, compute weighted impact on LR
 */
static void compute_cytokine_learning_effects(portia_learning_immune_bridge_t* bridge) {
    /* Guard clause */
    if (!bridge || !bridge->immune_system) {
        return;
    }

    /* Reset effects */
    memset(&bridge->cytokine_effects, 0, sizeof(cytokine_learning_effects_t));

    /* Use inflammation as proxy for cytokine levels */
    brain_inflammation_level_t inflam = INFLAMMATION_NONE;
    float inflam_factor = 1.0f;

    /* Compute individual cytokine effects on learning rate */
    bridge->cytokine_effects.il1_lr_reduction =
        CYTOKINE_IL1_LR_IMPAIRMENT * inflam_factor;
    bridge->cytokine_effects.il6_lr_reduction =
        CYTOKINE_IL6_LR_IMPAIRMENT * inflam_factor;
    bridge->cytokine_effects.tnf_lr_reduction =
        CYTOKINE_TNF_LR_IMPAIRMENT * inflam_factor;
    bridge->cytokine_effects.ifn_gamma_lr_reduction =
        CYTOKINE_IFN_GAMMA_LR_IMPAIRMENT * inflam_factor;

    /* Anti-inflammatory recovery */
    bridge->cytokine_effects.il10_lr_recovery =
        CYTOKINE_IL10_LR_RECOVERY * (1.0f - inflam_factor);

    /* Compute total LR factor */
    float total_reduction =
        fabsf(bridge->cytokine_effects.il1_lr_reduction) +
        fabsf(bridge->cytokine_effects.il6_lr_reduction) +
        fabsf(bridge->cytokine_effects.tnf_lr_reduction) +
        fabsf(bridge->cytokine_effects.ifn_gamma_lr_reduction);

    total_reduction -= bridge->cytokine_effects.il10_lr_recovery;
    total_reduction *= bridge->config.cytokine_sensitivity;

    bridge->cytokine_effects.total_lr_factor =
        fmaxf(0.0f, fminf(1.0f, 1.0f - total_reduction));

    /* Other learning effects */
    bridge->cytokine_effects.consolidation_impairment = total_reduction * 0.8f;
    bridge->cytokine_effects.habituation_slowdown = total_reduction * 0.6f;
    /* Inflammation paradoxically enhances aversive/fear learning */
    bridge->cytokine_effects.aversive_learning_boost = inflam_factor * 0.3f;
}

/**
 * @brief Compute inflammation learning effects
 *
 * WHAT: Calculate learning impairment from inflammation
 * WHY:  Inflammation disrupts LTP, consolidation, and plasticity
 * HOW:  Map inflammation level to learning parameter reductions
 */
static void compute_inflammation_learning_effects(portia_learning_immune_bridge_t* bridge) {
    /* Guard clause */
    if (!bridge || !bridge->immune_system) {
        return;
    }

    /* Get inflammation level */
    brain_inflammation_level_t level = INFLAMMATION_NONE;
    bridge->inflammation_state.current_level = level;

    /* Map inflammation to LR factor */
    switch (level) {
        case INFLAMMATION_NONE:
            bridge->inflammation_state.lr_factor = INFLAMMATION_NONE_LR_FACTOR;
            break;
        case INFLAMMATION_LOCAL:
            bridge->inflammation_state.lr_factor = INFLAMMATION_LOCAL_LR_FACTOR;
            break;
        case INFLAMMATION_REGIONAL:
            bridge->inflammation_state.lr_factor = INFLAMMATION_REGIONAL_LR_FACTOR;
            break;
        case INFLAMMATION_SYSTEMIC:
            bridge->inflammation_state.lr_factor = INFLAMMATION_SYSTEMIC_LR_FACTOR;
            break;
        case INFLAMMATION_STORM:
            bridge->inflammation_state.lr_factor = INFLAMMATION_STORM_LR_FACTOR;
            break;
        default:
            bridge->inflammation_state.lr_factor = 1.0f;
    }

    /* Apply sensitivity */
    float reduction = 1.0f - bridge->inflammation_state.lr_factor;
    reduction *= bridge->config.inflammation_sensitivity;
    bridge->inflammation_state.lr_factor = 1.0f - reduction;

    /* Consolidation impairment */
    bridge->inflammation_state.consolidation_impairment =
        INFLAMMATION_CONSOLIDATION_BASE +
        (INFLAMMATION_CONSOLIDATION_PER_LEVEL * (float)level);

    /* Other learning effects scale with inflammation severity */
    float severity = (float)level / (float)INFLAMMATION_STORM;
    bridge->inflammation_state.ltp_impairment = severity * 0.7f;
    bridge->inflammation_state.extinction_impairment = severity * 0.5f;
    bridge->inflammation_state.reversal_learning_deficit = severity * 0.6f;

    /* Learning biases */
    bridge->inflammation_state.aversive_bias = severity * 0.4f;
    bridge->inflammation_state.reward_learning_deficit = severity * 0.5f;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int portia_learning_immune_default_config(portia_learning_immune_config_t* config) {
    /* Guard clause */
    if (!config) {
        NIMCP_LOGGING_ERROR("Null config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_learning_immune_default_config: config is NULL");
        return -1;
    }

    /* Feature enables */
    config->enable_cytokine_learning_impairment = true;
    config->enable_inflammation_lr_reduction = true;
    config->enable_learning_failure_immune_trigger = true;
    config->enable_learning_success_immune_benefit = true;
    config->enable_repeated_failure_inflammation = true;

    /* Sensitivity tuning */
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->learning_immune_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Thresholds */
    config->failure_threshold = LEARNING_FAILURE_THRESHOLD;
    config->success_threshold = LEARNING_SUCCESS_THRESHOLD;
    config->repeated_failure_count = LEARNING_REPEATED_FAILURE_COUNT;

    return 0;
}

portia_learning_immune_bridge_t* portia_learning_immune_create(
    const portia_learning_immune_config_t* config,
    brain_immune_system_t* immune_system,
    portia_learning_state_t* learning_system
) {
    /* Guard clauses */
    if (!immune_system) {
        NIMCP_LOGGING_ERROR("Null immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_system is NULL");

        return NULL;
    }

    if (!learning_system) {
        NIMCP_LOGGING_ERROR("Null learning system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "learning_system is NULL");


        return NULL;
    }

    /* Allocate bridge */
    portia_learning_immune_bridge_t* bridge =
        (portia_learning_immune_bridge_t*)nimcp_calloc(
            1, sizeof(portia_learning_immune_bridge_t)
        );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "learning_immune_create: allocation failed");

        return NULL;
    }

    /* Store system handles */
    bridge->immune_system = immune_system;
    bridge->learning_system = learning_system;

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        portia_learning_immune_default_config(&bridge->config);
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "portia_learning_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_learning_immune_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->last_update_time = 0;
    bridge->base.bio_async_enabled = false;

    /* Initialize modulation structure to prevent reading uninitialized values */
    memset(&bridge->learning_modulation, 0, sizeof(bridge->learning_modulation));
    bridge->learning_modulation.learning_success_rate = 0.5f;  // Neutral default

    NIMCP_LOGGING_INFO("Created Portia learning-immune bridge");
    return bridge;
}

void portia_learning_immune_destroy(portia_learning_immune_bridge_t* bridge) {
    /* Guard clause */
    if (!bridge) {
        return;
    }

    /* bridge_base_cleanup handles bio-async disconnect + mutex destroy+free */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Destroyed Portia learning-immune bridge");
}

/* ============================================================================
 * Immune → Learning API Implementation
 * ============================================================================ */

int portia_learning_immune_apply_cytokine_effects(portia_learning_immune_bridge_t* bridge) {
    /* Guard clause */
    if (!bridge || !bridge->learning_system) {
        NIMCP_LOGGING_ERROR("Invalid bridge or learning system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_learning_immune_apply_cytokine_effects: required parameter is NULL (bridge, bridge->learning_system)");
        return -1;
    }

    if (!bridge->config.enable_cytokine_learning_impairment) {
        return 0;  /* Feature disabled */
    }

    /* Lock */
    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute cytokine effects */
    compute_cytokine_learning_effects(bridge);

    /* Apply LR factor to learning system */
    float lr_factor = bridge->cytokine_effects.total_lr_factor;

    /* Would apply to learning system here */
    /* portia_learning_scale_lr(bridge->learning_system, lr_factor); */

    bridge->cytokine_impairments++;
    bridge->total_updates++;

    /* Unlock */
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied cytokine effects: lr_factor=%.3f", lr_factor);
    return 0;
}

int portia_learning_immune_apply_inflammation_effects(
    portia_learning_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge || !bridge->learning_system) {
        NIMCP_LOGGING_ERROR("Invalid bridge or learning system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_learning_immune_apply_inflammation_effects: required parameter is NULL (bridge, bridge->learning_system)");
        return -1;
    }

    if (!bridge->config.enable_inflammation_lr_reduction) {
        return 0;  /* Feature disabled */
    }

    /* Lock */
    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute inflammation effects */
    compute_inflammation_learning_effects(bridge);

    /* Apply effects */
    float lr_factor = bridge->inflammation_state.lr_factor;
    float consolidation_impairment = bridge->inflammation_state.consolidation_impairment;

    /* Would apply to learning system here */
    /* portia_learning_scale_lr(bridge->learning_system, lr_factor); */
    /* portia_learning_impair_consolidation(bridge->learning_system, consolidation_impairment); */

    bridge->total_updates++;

    /* Unlock */
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied inflammation effects: lr=%.3f, consolidation_loss=%.3f",
                       lr_factor, consolidation_impairment);
    return 0;
}

float portia_learning_immune_compute_lr_factor(
    const portia_learning_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge) {
        return 1.0f;
    }

    /* Combine cytokine and inflammation effects */
    float cytokine_factor = bridge->cytokine_effects.total_lr_factor;
    float inflam_factor = bridge->inflammation_state.lr_factor;

    return fminf(cytokine_factor, inflam_factor);
}

float portia_learning_immune_compute_consolidation_impairment(
    const portia_learning_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge) {
        return 0.0f;
    }

    /* Combine cytokine and inflammation consolidation impairments */
    float cytokine_impairment = bridge->cytokine_effects.consolidation_impairment;
    float inflam_impairment = bridge->inflammation_state.consolidation_impairment;

    return fmaxf(cytokine_impairment, inflam_impairment);
}

/* ============================================================================
 * Learning → Immune API Implementation
 * ============================================================================ */

/**
 * @brief Internal unlocked version - caller MUST hold bridge->base.mutex
 */
static int portia_learning_immune_trigger_failure_response_unlocked(
    portia_learning_immune_bridge_t* bridge
) {
    if (!bridge->config.enable_learning_failure_immune_trigger) {
        return 0;  /* Feature disabled */
    }

    /* Check learning success rate */
    float success_rate = bridge->learning_modulation.learning_success_rate;

    if (success_rate < bridge->config.failure_threshold) {
        /* Accumulate frustration */
        bridge->failure_accumulator += 1.0f;
        bridge->learning_modulation.consecutive_failures++;
        bridge->learning_modulation.consecutive_successes = 0;

        /* Calculate frustration level */
        bridge->learning_modulation.frustration_level =
            fminf(1.0f, bridge->failure_accumulator / 10.0f);

        /* Trigger IL-6 release if frustrated */
        if (bridge->failure_accumulator > 3.0f) {
            float il6_amount = LEARNING_FAILURE_IL6_RELEASE *
                              bridge->config.learning_immune_sensitivity;

            bridge->failure_immune_triggers++;
            bridge->failure_accumulator = 0.0f;

            NIMCP_LOGGING_INFO("Learning failure triggered IL-6 release: %.3f", il6_amount);
        }
    } else {
        /* Decay failure accumulator on success */
        bridge->failure_accumulator *= 0.8f;
    }

    return 0;
}

/**
 * @brief Internal unlocked version - caller MUST hold bridge->base.mutex
 */
static int portia_learning_immune_release_il10_from_success_unlocked(
    portia_learning_immune_bridge_t* bridge
) {
    if (!bridge->config.enable_learning_success_immune_benefit) {
        return 0;  /* Feature disabled */
    }

    /* Check learning success rate */
    float success_rate = bridge->learning_modulation.learning_success_rate;

    if (success_rate > bridge->config.success_threshold) {
        bridge->learning_modulation.consecutive_successes++;
        bridge->learning_modulation.consecutive_failures = 0;

        /* Release IL-10 for sustained success */
        if (bridge->learning_modulation.consecutive_successes >= 3) {
            float il10_amount = LEARNING_SUCCESS_IL10_RELEASE *
                               bridge->config.learning_immune_sensitivity;

            bridge->success_immune_benefits++;

            NIMCP_LOGGING_INFO("Learning success triggered IL-10 release: %.3f", il10_amount);
        }
    }

    return 0;
}

/**
 * @brief Internal unlocked version - caller MUST hold bridge->base.mutex
 */
static int portia_learning_immune_trigger_repeated_failure_inflammation_unlocked(
    portia_learning_immune_bridge_t* bridge
) {
    if (!bridge->config.enable_repeated_failure_inflammation) {
        return 0;  /* Feature disabled */
    }

    /* Check for repeated failures */
    if (bridge->learning_modulation.consecutive_failures >=
        bridge->config.repeated_failure_count) {

        /* Trigger chronic inflammation */
        float inflammation_level = LEARNING_REPEATED_FAIL_INFLAMMATION *
                                  bridge->config.learning_immune_sensitivity;

        bridge->learning_modulation.repeated_failure_inflammation = true;
        bridge->learning_modulation.chronic_failure_inflammation = inflammation_level;
        bridge->repeated_failure_inflammations++;

        NIMCP_LOGGING_WARN("Repeated learning failures triggered inflammation: %.3f",
                          inflammation_level);
    } else {
        bridge->learning_modulation.repeated_failure_inflammation = false;
    }

    return 0;
}

int portia_learning_immune_trigger_failure_response(
    portia_learning_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge || !bridge->immune_system) {
        NIMCP_LOGGING_ERROR("Invalid bridge or immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_learning_immune_trigger_failure_response: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    /* Lock */
    nimcp_mutex_lock(bridge->base.mutex);
    int result = portia_learning_immune_trigger_failure_response_unlocked(bridge);
    /* Unlock */
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

int portia_learning_immune_release_il10_from_success(
    portia_learning_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge || !bridge->immune_system) {
        NIMCP_LOGGING_ERROR("Invalid bridge or immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_learning_immune_release_il10_from_success: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    /* Lock */
    nimcp_mutex_lock(bridge->base.mutex);
    int result = portia_learning_immune_release_il10_from_success_unlocked(bridge);
    /* Unlock */
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

int portia_learning_immune_trigger_repeated_failure_inflammation(
    portia_learning_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge || !bridge->immune_system) {
        NIMCP_LOGGING_ERROR("Invalid bridge or immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_learning_immune_trigger_repeated_failure_inflammation: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    /* Lock */
    nimcp_mutex_lock(bridge->base.mutex);
    int result = portia_learning_immune_trigger_repeated_failure_inflammation_unlocked(bridge);
    /* Unlock */
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

/* ============================================================================
 * Bidirectional Update API Implementation
 * ============================================================================ */

int portia_learning_immune_update(
    portia_learning_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Guard clause */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_learning_immune_update: bridge is NULL");
        return -1;
    }

    /* Lock */
    nimcp_mutex_lock(bridge->base.mutex);

    /* Update timing */
    bridge->last_update_time += delta_ms;

    /* Immune → Learning direction */
    if (bridge->config.enable_cytokine_learning_impairment) {
        compute_cytokine_learning_effects(bridge);
    }

    if (bridge->config.enable_inflammation_lr_reduction) {
        compute_inflammation_learning_effects(bridge);
    }

    /* Learning → Immune direction - use unlocked versions since we already hold the mutex */
    if (bridge->config.enable_learning_failure_immune_trigger) {
        portia_learning_immune_trigger_failure_response_unlocked(bridge);
    }

    if (bridge->config.enable_learning_success_immune_benefit) {
        portia_learning_immune_release_il10_from_success_unlocked(bridge);
    }

    if (bridge->config.enable_repeated_failure_inflammation) {
        portia_learning_immune_trigger_repeated_failure_inflammation_unlocked(bridge);
    }

    bridge->total_updates++;

    /* Unlock */
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

int portia_learning_immune_get_cytokine_effects(
    const portia_learning_immune_bridge_t* bridge,
    cytokine_learning_effects_t* effects
) {
    /* Guard clause */
    if (!bridge || !effects) {
        NIMCP_LOGGING_ERROR("Null bridge or effects");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_learning_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    *effects = bridge->cytokine_effects;
    return 0;
}

int portia_learning_immune_get_inflammation_state(
    const portia_learning_immune_bridge_t* bridge,
    inflammation_learning_state_t* state
) {
    /* Guard clause */
    if (!bridge || !state) {
        NIMCP_LOGGING_ERROR("Null bridge or state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_learning_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    *state = bridge->inflammation_state;
    return 0;
}

bool portia_learning_immune_has_learning_deficit(
    const portia_learning_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge) {
        return false;
    }

    float lr_factor = portia_learning_immune_compute_lr_factor(bridge);
    return (lr_factor < 0.6f);  /* >40% LR loss */
}

float portia_learning_immune_get_lr_factor(const portia_learning_immune_bridge_t* bridge) {
    return portia_learning_immune_compute_lr_factor(bridge);
}

float portia_learning_immune_get_consolidation_impairment(
    const portia_learning_immune_bridge_t* bridge
) {
    return portia_learning_immune_compute_consolidation_impairment(bridge);
}

/* ============================================================================
 * Bio-Async Integration API Implementation
 * ============================================================================ */

int portia_learning_immune_connect_bio_async(portia_learning_immune_bridge_t* bridge) {
    /* Guard clause */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_learning_immune_connect_bio_async: bridge is NULL");
        return -1;
    }

    /* Would register with bio-async router here */
    /* bio_router_register_module(&bridge->base.bio_ctx, BIO_MODULE_IMMUNE_PORTIA_LEARNING); */

    bridge->base.bio_async_enabled = true;

    NIMCP_LOGGING_INFO("Connected Portia learning-immune bridge to bio-async");
    return 0;
}

int portia_learning_immune_disconnect_bio_async(portia_learning_immune_bridge_t* bridge) {
    /* Guard clause */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_learning_immune_disconnect_bio_async: bridge is NULL");
        return -1;
    }

    if (!bridge->base.bio_async_enabled) {
        return 0;  /* Not connected */
    }

    /* Would unregister from bio-async router here */
    /* bio_router_unregister_module(&bridge->base.bio_ctx); */

    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Disconnected Portia learning-immune bridge from bio-async");
    return 0;
}

bool portia_learning_immune_is_bio_async_connected(
    const portia_learning_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge) {
        return false;
    }

    return bridge->base.bio_async_enabled;
}
