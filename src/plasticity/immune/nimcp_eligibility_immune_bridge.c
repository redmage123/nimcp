/**
 * @file nimcp_eligibility_immune_bridge.c
 * @brief Eligibility Trace-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and eligibility trace systems
 * WHY:  Biological realism - cytokines impair credit assignment, learning failures trigger inflammation
 * HOW:  Monitor cytokine levels to modulate trace decay, monitor learning to trigger immune responses
 */

#include "plasticity/immune/nimcp_eligibility_immune_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(eligibility_immune_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(eligibility_immune_bridge)


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get inflammation level from immune system
 *
 * WHAT: Query current maximum inflammation level
 * WHY:  Max inflammation determines trace impairment
 * HOW:  Query immune system for max inflammation site level
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune) return INFLAMMATION_NONE;

    /* Would query immune system inflammation_sites */
    /* For now, return NONE - actual implementation would check inflammation_sites array */
    return INFLAMMATION_NONE;
}

/**
 * @brief Get inflammation duration
 *
 * WHAT: Calculate how long inflammation has persisted
 * WHY:  Chronic inflammation (>7 days) has different effects
 * HOW:  Find oldest active inflammation site, compute duration
 */
static float get_inflammation_duration_sec(const brain_immune_system_t* immune) {
    if (!immune) return 0.0f;

    /* Would query immune system for inflammation sites */
    /* For now, return 0 - actual implementation would check inflammation_sites array */
    return 0.0f;
}

/**
 * @brief Map inflammation level to trace decay multiplier
 *
 * WHAT: Convert inflammation to decay_lambda modifier
 * WHY:  Higher inflammation = faster decay (shorter traces)
 * HOW:  Lookup table based on inflammation level
 */
static float inflammation_to_decay_multiplier(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_TRACE_MULTIPLIER_NONE;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_TRACE_MULTIPLIER_LOCAL;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_TRACE_MULTIPLIER_REGIONAL;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_TRACE_MULTIPLIER_SYSTEMIC;
        case INFLAMMATION_STORM:    return INFLAMMATION_TRACE_MULTIPLIER_STORM;
        default:                    return INFLAMMATION_TRACE_MULTIPLIER_NONE;
    }
}

/**
 * @brief Map inflammation level to learning rate factor
 *
 * WHAT: Convert inflammation to LR reduction factor
 * WHY:  Higher inflammation = reduced learning effectiveness
 * HOW:  Lookup table based on inflammation level
 */
static float inflammation_to_lr_factor(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_LR_FACTOR_NONE;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_LR_FACTOR_LOCAL;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_LR_FACTOR_REGIONAL;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_LR_FACTOR_SYSTEMIC;
        case INFLAMMATION_STORM:    return INFLAMMATION_LR_FACTOR_STORM;
        default:                    return INFLAMMATION_LR_FACTOR_NONE;
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int eligibility_immune_default_config(eligibility_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled by default */
    config->enable_cytokine_trace_modulation = true;
    config->enable_inflammation_impairment = true;
    config->enable_learning_failure_detection = true;
    config->enable_consolidation_monitoring = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->stress_trigger_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Evidence-based thresholds */
    config->learning_failure_threshold = LEARNING_FAILURE_STRESS_THRESHOLD;
    config->consolidation_failure_count = CONSOLIDATION_FAILURE_THRESHOLD;

    return 0;
}

eligibility_immune_bridge_t* eligibility_immune_bridge_create(
    const eligibility_immune_config_t* config,
    brain_immune_system_t* immune_system,
    eligibility_config_t* eligibility_config
) {
    /* Guard: require immune system and eligibility config */
    if (!immune_system || !eligibility_config) {
        LOG_MODULE_ERROR("eligibility_immune_bridge",
                  "Cannot create bridge without immune system and eligibility config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_immune_bridge_create: required parameter is NULL (immune_system, eligibility_config)");
        return NULL;
    }

    /* Allocate bridge */
    eligibility_immune_bridge_t* bridge = (eligibility_immune_bridge_t*)
        nimcp_malloc(sizeof(eligibility_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR("eligibility_immune_bridge", "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "eligibility_immune_bridge_create: bridge allocation failed");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(eligibility_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->eligibility_config = eligibility_config;

    /* Store baseline parameters for restoration */
    bridge->baseline_decay_lambda = eligibility_config->decay_lambda;
    bridge->baseline_learning_rate = eligibility_config->learning_rate;
    bridge->baseline_trace_window_ms = 1000.0f;  /* Assume ~1000ms baseline */

    /* Apply configuration */
    if (config) {
        bridge->enable_cytokine_trace_modulation = config->enable_cytokine_trace_modulation;
        bridge->enable_inflammation_impairment = config->enable_inflammation_impairment;
        bridge->enable_learning_failure_detection = config->enable_learning_failure_detection;
        bridge->enable_consolidation_monitoring = config->enable_consolidation_monitoring;
    } else {
        /* Use defaults */
        eligibility_immune_config_t default_cfg;
        eligibility_immune_default_config(&default_cfg);
        bridge->enable_cytokine_trace_modulation = default_cfg.enable_cytokine_trace_modulation;
        bridge->enable_inflammation_impairment = default_cfg.enable_inflammation_impairment;
        bridge->enable_learning_failure_detection = default_cfg.enable_learning_failure_detection;
        bridge->enable_consolidation_monitoring = default_cfg.enable_consolidation_monitoring;
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "eligibility_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);    return NULL;
    }

    LOG_MODULE_INFO("eligibility_immune_bridge", "Bridge created successfully");
    return bridge;
}

void eligibility_immune_bridge_destroy(eligibility_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Destroy mutex */
    if (bridge->base.mutex) {
        nimcp_mutex_destroy((nimcp_mutex_t*)bridge->base.mutex);
    }

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    LOG_MODULE_INFO("eligibility_immune_bridge", "Bridge destroyed");
}

/* ============================================================================
 * Immune → Eligibility Trace Implementation
 * ============================================================================ */

int eligibility_immune_apply_cytokine_effects(eligibility_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_cytokine_trace_modulation) return 0;
    if (!bridge->immune_system || !bridge->eligibility_config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_immune_apply_cytokine_effects: required parameter is NULL (bridge->immune_system, bridge->eligibility_config)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    cytokine_trace_effects_t* effects = &bridge->cytokine_effects;

    /* Pro-inflammatory cytokines → trace shortening */
    /* Note: Would query actual cytokine levels from immune system */
    effects->il1_trace_shortening = 0.0f;  /* IL-1β level * IL1_DECAY_IMPACT */
    effects->il6_trace_shortening = 0.0f;  /* IL-6 level * IL6_DECAY_IMPACT */
    effects->tnf_trace_shortening = 0.0f;  /* TNF-α level * TNF_DECAY_IMPACT */

    /* Anti-inflammatory cytokines → trace restoration */
    effects->il10_trace_restoration = 0.0f;  /* IL-10 level * IL10_DECAY_IMPACT */

    /* Aggregate effects on decay rate */
    effects->total_decay_modifier =
        effects->il1_trace_shortening +
        effects->il6_trace_shortening +
        effects->tnf_trace_shortening +
        effects->il10_trace_restoration;

    /* Learning rate modifier (similar to decay) */
    float proinflam_total = fabsf(effects->il1_trace_shortening) +
                           fabsf(effects->il6_trace_shortening) +
                           fabsf(effects->tnf_trace_shortening);
    effects->learning_rate_modifier = nimcp_clampf(1.0f - (proinflam_total * 0.5f), 0.5f, 1.0f);

    /* Consolidation impairment from cytokines */
    effects->consolidation_impairment = nimcp_clampf(proinflam_total * 0.6f, 0.0f, 0.8f);

    /* Credit assignment window (shorter with inflammation) */
    effects->credit_assignment_window_ms = bridge->baseline_trace_window_ms *
        (1.0f + effects->total_decay_modifier);
    effects->credit_assignment_window_ms = nimcp_clampf(
        effects->credit_assignment_window_ms, 50.0f, 1000.0f
    );

    /* Apply to eligibility config */
    float lambda_modifier = 1.0f + (effects->total_decay_modifier * 0.3f);
    lambda_modifier = nimcp_clampf(lambda_modifier, 0.6f, 1.0f);
    bridge->eligibility_config->decay_lambda = bridge->baseline_decay_lambda * lambda_modifier;

    float lr_modifier = effects->learning_rate_modifier;
    bridge->eligibility_config->learning_rate = bridge->baseline_learning_rate * lr_modifier;

    bridge->cytokine_modulations++;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int eligibility_immune_apply_inflammation_effects(eligibility_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_inflammation_impairment) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_immune_apply_inflammation_effects: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    inflammation_trace_state_t* state = &bridge->inflammation_state;

    /* Get inflammation state */
    state->current_level = get_max_inflammation_level(bridge->immune_system);
    state->inflammation_duration_sec = get_inflammation_duration_sec(bridge->immune_system);
    state->is_chronic = (state->inflammation_duration_sec >= CHRONIC_INFLAMMATION_THRESHOLD_SEC);

    /* Map inflammation to decay modifier */
    state->decay_lambda_modifier = inflammation_to_decay_multiplier(state->current_level);

    /* Map inflammation to learning rate factor */
    state->learning_rate_factor = inflammation_to_lr_factor(state->current_level);

    /* Effective trace window (shorter with inflammation) */
    state->trace_window_ms = bridge->baseline_trace_window_ms * state->decay_lambda_modifier;

    /* Distal reward impairment (chronic inflammation worse) */
    float inflammation_intensity = (float)state->current_level / (float)INFLAMMATION_STORM;
    state->distal_reward_impairment = nimcp_clampf(inflammation_intensity * 0.8f, 0.0f, 1.0f);
    if (state->is_chronic) {
        state->distal_reward_impairment = nimcp_clampf(
            state->distal_reward_impairment * 1.3f, 0.0f, 1.0f
        );
    }

    /* Dopamine system disruption */
    state->dopamine_synthesis_reduction = nimcp_clampf(inflammation_intensity * 0.7f, 0.0f, 0.9f);
    state->burst_amplitude_reduction = nimcp_clampf(inflammation_intensity * 0.6f, 0.0f, 0.8f);

    bridge->trace_shortenings++;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

float eligibility_immune_get_effective_lambda(const eligibility_immune_bridge_t* bridge) {
    if (!bridge || !bridge->eligibility_config) return 0.95f;

    /* Return current decay_lambda (already modulated by cytokines/inflammation) */
    return bridge->eligibility_config->decay_lambda;
}

float eligibility_immune_get_lr_factor(const eligibility_immune_bridge_t* bridge) {
    if (!bridge) return 1.0f;

    /* Combine cytokine and inflammation LR modifiers */
    float cytokine_lr = bridge->cytokine_effects.learning_rate_modifier;
    float inflammation_lr = bridge->inflammation_state.learning_rate_factor;

    /* Take minimum (most conservative) */
    return fminf(cytokine_lr, inflammation_lr);
}

int eligibility_immune_restore_baseline(eligibility_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->eligibility_config) return -1;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Check if IL-10 is promoting recovery */
    float il10_recovery = bridge->cytokine_effects.il10_trace_restoration;

    if (il10_recovery > 0.1f) {
        /* Gradually restore baseline parameters */
        float current_lambda = bridge->eligibility_config->decay_lambda;
        float target_lambda = bridge->baseline_decay_lambda;
        float lambda_step = (target_lambda - current_lambda) * il10_recovery * 0.2f;
        bridge->eligibility_config->decay_lambda = nimcp_clampf(
            current_lambda + lambda_step, 0.6f, target_lambda
        );

        float current_lr = bridge->eligibility_config->learning_rate;
        float target_lr = bridge->baseline_learning_rate;
        float lr_step = (target_lr - current_lr) * il10_recovery * 0.2f;
        bridge->eligibility_config->learning_rate = nimcp_clampf(
            current_lr + lr_step, target_lr * 0.5f, target_lr
        );
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Eligibility → Immune Implementation
 * ============================================================================ */

int eligibility_immune_detect_learning_failure(
    eligibility_immune_bridge_t* bridge,
    float reward
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_learning_failure_detection) return 0;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    learning_failure_stress_t* stress = &bridge->learning_stress;

    /* Update cumulative negative reward */
    if (reward < 0.0f) {
        stress->cumulative_negative_reward += fabsf(reward);
        stress->consecutive_failures++;
    } else {
        /* Positive reward resets consecutive failures */
        stress->consecutive_failures = 0;
        stress->cumulative_negative_reward *= 0.9f;  /* Slow decay */
    }

    /* Compute average reward error (moving average) */
    stress->average_reward_error = stress->average_reward_error * 0.95f + fabsf(reward) * 0.05f;
    if (!isfinite(stress->average_reward_error)) stress->average_reward_error = fabsf(reward);

    /* Detect learned helplessness */
    if (stress->consecutive_failures > 50 && stress->average_reward_error > 0.5f) {
        stress->learned_helplessness = true;
    }

    /* Compute stress level */
    float normalized_cum_reward = nimcp_clampf(stress->cumulative_negative_reward / 10.0f, 0.0f, 1.0f);
    float failure_factor = nimcp_clampf(stress->consecutive_failures / 50.0f, 0.0f, 1.0f);
    stress->stress_level = (normalized_cum_reward * 0.6f) + (failure_factor * 0.4f);

    /* Trigger immune if stress exceeds threshold */
    if (stress->stress_level >= LEARNING_FAILURE_STRESS_THRESHOLD) {
        stress->immune_triggered = true;
        bridge->learning_failure_triggers++;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int eligibility_immune_monitor_consolidation(
    eligibility_immune_bridge_t* bridge,
    uint32_t num_active_traces,
    bool burst_occurred
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_consolidation_monitoring) return 0;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    learning_failure_stress_t* stress = &bridge->learning_stress;

    /* Update unconsolidated trace count */
    stress->unconsolidated_traces = num_active_traces;

    /* Track consolidation events */
    if (burst_occurred) {
        stress->last_consolidation_ms = 0;  /* Would be actual timestamp */
        stress->consolidation_frustration *= 0.8f;  /* Reduce frustration */
    } else {
        /* Frustration increases with unconsolidated traces */
        if (num_active_traces > 10) {
            stress->consolidation_frustration += 0.01f;
            stress->consolidation_frustration = nimcp_clampf(
                stress->consolidation_frustration, 0.0f, 1.0f
            );
        }
    }

    /* Trigger immune if too many unconsolidated traces */
    if (num_active_traces > CONSOLIDATION_FAILURE_THRESHOLD) {
        stress->immune_triggered = true;
        bridge->consolidation_failures++;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int eligibility_immune_trigger_from_learning_stress(eligibility_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->immune_system) return -1;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    learning_failure_stress_t* stress = &bridge->learning_stress;

    /* Only trigger if stress detected */
    if (!stress->immune_triggered) {
        nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
        return 0;
    }

    /* Present "learning failure antigen" to immune system */
    /* Note: Would call brain_immune_present_antigen() with custom epitope */
    uint8_t learning_failure_epitope[] = {0xFA, 0x11, 0xED};  /* "FAILED" */
    uint32_t antigen_id;

    /* Severity based on stress level (1-10 scale) */
    uint32_t severity = 1;  /* Low */
    if (stress->stress_level > 0.9f) {
        severity = 10;  /* Critical */
    } else if (stress->stress_level > 0.7f) {
        severity = 8;   /* High */
    } else if (stress->stress_level > 0.5f) {
        severity = 5;   /* Medium */
    }

    /* brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_MANUAL,
        learning_failure_epitope,
        sizeof(learning_failure_epitope),
        severity,
        0,
        &antigen_id
    ); */

    /* Reset trigger flag after presenting */
    stress->immune_triggered = false;

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int eligibility_immune_bridge_update(
    eligibility_immune_bridge_t* bridge,
    uint64_t delta_ms,
    float current_reward,
    uint32_t num_active_traces,
    bool burst_occurred
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Immune → Eligibility */
    eligibility_immune_apply_cytokine_effects(bridge);
    eligibility_immune_apply_inflammation_effects(bridge);
    eligibility_immune_restore_baseline(bridge);

    /* Eligibility → Immune */
    eligibility_immune_detect_learning_failure(bridge, current_reward);
    eligibility_immune_monitor_consolidation(bridge, num_active_traces, burst_occurred);
    eligibility_immune_trigger_from_learning_stress(bridge);

    bridge->total_updates++;

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int eligibility_immune_get_cytokine_effects(
    const eligibility_immune_bridge_t* bridge,
    cytokine_trace_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(effects, &bridge->cytokine_effects, sizeof(cytokine_trace_effects_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int eligibility_immune_get_inflammation_state(
    const eligibility_immune_bridge_t* bridge,
    inflammation_trace_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(state, &bridge->inflammation_state, sizeof(inflammation_trace_state_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

float eligibility_immune_get_stress_level(const eligibility_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->learning_stress.stress_level;
}

bool eligibility_immune_is_trace_impaired(const eligibility_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Trace impairment threshold: decay_lambda < 85% of baseline */
    float current_lambda = bridge->eligibility_config->decay_lambda;
    float threshold = bridge->baseline_decay_lambda * 0.85f;

    return current_lambda < threshold;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define ELIGIBILITY_IMMUNE_MODULE_NAME "eligibility_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 */
int eligibility_immune_connect_bio_async(eligibility_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_ELIGIBILITY,
        .module_name = ELIGIBILITY_IMMUNE_MODULE_NAME,
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("eligibility_immune_bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int eligibility_immune_disconnect_bio_async(eligibility_immune_bridge_t* bridge) {
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

    NIMCP_LOGGING_DEBUG("eligibility_immune_bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool eligibility_immune_is_bio_async_connected(const eligibility_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
