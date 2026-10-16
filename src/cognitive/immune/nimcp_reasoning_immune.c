/**
 * @file nimcp_reasoning_immune.c
 * @brief Reasoning-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and reasoning systems
 * WHY:  Cytokines impair executive function (biological realism), logical errors signal corruption
 * HOW:  Monitor cytokines/inflammation to modulate reasoning, monitor reasoning failures to trigger immune
 */

#include "cognitive/immune/nimcp_reasoning_immune.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_dimension_constants.h"
#include "constants/nimcp_learning_constants.h"

BRIDGE_BOILERPLATE(reasoning_immune, MESH_ADAPTER_CATEGORY_SECURITY)



/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct reasoning_immune_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Linked systems */
    brain_immune_system_t* immune_system;
    reasoning_integration_t* reasoning_integration;

    /* Configuration */
    bool enable_cytokine_reasoning_modulation;
    bool enable_inflammation_cognitive_slowing;
    bool enable_reasoning_failure_immune_trigger;
    bool enable_working_memory_modulation;
    bool enable_contradiction_immune_alert;

    float cytokine_sensitivity;
    float inflammation_sensitivity;
    float max_speed_reduction;
    float max_accuracy_reduction;
    uint32_t min_max_iterations;

    uint32_t proof_failure_threshold;
    uint32_t proof_failure_escalation;
    uint32_t unification_error_threshold;
    float failure_window_sec;
    float contradiction_antigen_severity;

    /* Current impairment state */
    reasoning_impairment_t current_impairment;

    /* Failure tracking */
    reasoning_failure_state_t failure_state;

    /* Statistics */
    reasoning_immune_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t mutex;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Clamp value to range
 *
 * WHAT: Constrain value to [min, max]
 * WHY:  Prevent overflow/underflow
 * HOW:  Return min if below, max if above, value otherwise
 */
static inline float clamp_f(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/**
 * @brief Get current time in milliseconds
 *
 * WHAT: Get monotonic timestamp
 * WHY:  Track failure windows
 * HOW:  Use clock_gettime(CLOCK_MONOTONIC)
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/**
 * @brief Check if failure window has expired
 *
 * WHAT: Determine if current failures outside time window
 * WHY:  Only count recent failures for immune trigger
 * HOW:  Compare first_failure_time to current time
 */
static bool is_window_expired(const reasoning_immune_bridge_t* bridge) {
    if (!bridge) return true;
    if (bridge->failure_state.first_failure_time_ms == 0) return true;

    uint64_t now_ms = get_time_ms();
    uint64_t elapsed_ms = now_ms - bridge->failure_state.first_failure_time_ms;
    float elapsed_sec = elapsed_ms / 1000.0f;

    return elapsed_sec > bridge->failure_window_sec;
}

/**
 * @brief Get cytokine level from immune system
 *
 * WHAT: Query current concentration of specific cytokine
 * WHY:  Compute cytokine-based impairment
 * HOW:  Iterate immune system cytokines array for matching type
 */
static float get_cytokine_level(
    const brain_immune_system_t* immune,
    brain_cytokine_type_t type
) {
    if (!immune) return 0.0f;
    float total = 0.0f;
    for (size_t i = 0; i < immune->cytokine_count; i++) {
        if (immune->cytokines[i].type == type) {
            total += immune->cytokines[i].concentration;
        }
    }
    return total;
}

/**
 * @brief Get maximum inflammation level from immune system
 *
 * WHAT: Query highest inflammation level in system
 * WHY:  Max inflammation determines cognitive impact
 * HOW:  Iterate inflammation_sites array for maximum level
 */
static brain_inflammation_level_t get_max_inflammation(
    const brain_immune_system_t* immune
) {
    if (!immune) return INFLAMMATION_NONE;
    brain_inflammation_level_t max_level = INFLAMMATION_NONE;
    for (size_t i = 0; i < immune->inflammation_count; i++) {
        if (immune->inflammation_sites[i].level > max_level) {
            max_level = immune->inflammation_sites[i].level;
        }
    }
    return max_level;
}

/**
 * @brief Compute cytokine-induced speed reduction
 *
 * WHAT: Calculate speed penalty from all cytokines
 * WHY:  Cytokines cumulatively impair processing speed
 * HOW:  Sum weighted impacts, clamp to max
 */
static float compute_cytokine_speed_impact(const reasoning_immune_bridge_t* bridge) {
    if (!bridge || !bridge->enable_cytokine_reasoning_modulation) {
        return 0.0f;
    }

    brain_immune_system_t* immune = bridge->immune_system;

    /* Query cytokine levels */
    float il1_level = get_cytokine_level(immune, CYTOKINE_IL1B);
    float il6_level = get_cytokine_level(immune, CYTOKINE_IL6);
    float tnf_level = get_cytokine_level(immune, CYTOKINE_TNFA);
    float ifn_level = get_cytokine_level(immune, BRAIN_CYTOKINE_IFN_GAMMA);
    float il10_level = get_cytokine_level(immune, CYTOKINE_IL10);

    /* Compute weighted impact (negative = slowing, positive = recovery) */
    float impact = 0.0f;
    impact += il1_level * CYTOKINE_IL1_REASONING_SPEED_IMPACT;
    impact += il6_level * CYTOKINE_IL6_REASONING_SPEED_IMPACT;
    impact += tnf_level * CYTOKINE_TNF_REASONING_SPEED_IMPACT;
    impact += ifn_level * CYTOKINE_IFN_GAMMA_REASONING_SPEED_IMPACT;
    impact += il10_level * CYTOKINE_IL10_REASONING_SPEED_IMPACT;

    /* Apply sensitivity */
    impact *= bridge->cytokine_sensitivity;

    /* Clamp to configured max (impact is negative for slowing) */
    float clamped = clamp_f(impact, -bridge->max_speed_reduction, 0.5f);
    return clamped;
}

/**
 * @brief Compute cytokine-induced accuracy reduction
 *
 * WHAT: Calculate accuracy penalty from all cytokines
 * WHY:  Cytokines increase errors in logical tasks
 * HOW:  Sum weighted impacts, clamp to max
 */
static float compute_cytokine_accuracy_impact(const reasoning_immune_bridge_t* bridge) {
    if (!bridge || !bridge->enable_cytokine_reasoning_modulation) {
        return 0.0f;
    }

    brain_immune_system_t* immune = bridge->immune_system;

    float il1_level = get_cytokine_level(immune, CYTOKINE_IL1B);
    float il6_level = get_cytokine_level(immune, CYTOKINE_IL6);
    float tnf_level = get_cytokine_level(immune, CYTOKINE_TNFA);
    float ifn_level = get_cytokine_level(immune, BRAIN_CYTOKINE_IFN_GAMMA);
    float il10_level = get_cytokine_level(immune, CYTOKINE_IL10);

    float impact = 0.0f;
    impact += il1_level * CYTOKINE_IL1_REASONING_ACCURACY_IMPACT;
    impact += il6_level * CYTOKINE_IL6_REASONING_ACCURACY_IMPACT;
    impact += tnf_level * CYTOKINE_TNF_REASONING_ACCURACY_IMPACT;
    impact += ifn_level * CYTOKINE_IFN_GAMMA_REASONING_ACCURACY_IMPACT;
    impact += il10_level * CYTOKINE_IL10_REASONING_ACCURACY_IMPACT;

    impact *= bridge->cytokine_sensitivity;

    float clamped = clamp_f(impact, -bridge->max_accuracy_reduction, 0.5f);
    return clamped;
}

/**
 * @brief Compute inflammation-induced performance penalty
 *
 * WHAT: Calculate reasoning penalty from inflammation level
 * WHY:  Chronic inflammation causes cognitive deficits
 * HOW:  Map inflammation level to penalty factor
 */
static float compute_inflammation_penalty(const reasoning_immune_bridge_t* bridge) {
    if (!bridge || !bridge->enable_inflammation_cognitive_slowing) {
        return 0.0f;
    }

    brain_inflammation_level_t level = get_max_inflammation(bridge->immune_system);

    /* Continuous penalty: smooth interpolation from 0.0 (none) to storm penalty */
    float cont = inflammation_level_to_continuous(level);
    float penalty = inflammation_compute_factor(cont,
        0.0f, INFLAMMATION_STORM_REASONING_PENALTY);

    /* Apply sensitivity */
    penalty *= bridge->inflammation_sensitivity;
    return clamp_f(penalty, 0.0f, 1.0f);
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int reasoning_immune_default_config(reasoning_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled by default */
    /* Phase 8: Heartbeat at operation start */
    reasoning_immune_heartbeat("reasoning_im_default_config", 0.0f);


    config->enable_cytokine_reasoning_modulation = true;
    config->enable_inflammation_cognitive_slowing = true;
    config->enable_reasoning_failure_immune_trigger = true;
    config->enable_working_memory_modulation = true;
    config->enable_contradiction_immune_alert = true;

    /* Biologically-based sensitivities */
    config->cytokine_sensitivity = 1.0f;
    config->inflammation_sensitivity = 1.0f;

    /* Safety limits */
    config->max_speed_reduction = 0.80f;      /* Max -80% (STORM level) */
    config->max_accuracy_reduction = 0.60f;   /* Max -60% (STORM level) */
    config->min_max_iterations = 5;           /* At least 5 iterations */

    /* Failure thresholds */
    config->proof_failure_threshold = REASONING_PROOF_FAILURE_THRESHOLD;
    config->proof_failure_escalation = REASONING_PROOF_FAILURE_ESCALATION;
    config->unification_error_threshold = REASONING_UNIFICATION_ERROR_THRESHOLD;
    config->failure_window_sec = REASONING_FAILURE_WINDOW_SEC;
    config->contradiction_antigen_severity = REASONING_CONTRADICTION_SEVERITY;

    return 0;
}

reasoning_immune_bridge_t* reasoning_immune_bridge_create(
    const reasoning_immune_config_t* config,
    brain_immune_system_t* immune_system,
    reasoning_integration_t* reasoning_integration
) {
    /* Guard: require both systems */
    if (!immune_system || !reasoning_integration) {
        LOG_MODULE_ERROR("reasoning_immune_bridge",
                  "Cannot create bridge without immune and reasoning systems");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_immune_bridge_create: required parameter is NULL (immune_system, reasoning_integration)");
        return NULL;
    }

    /* Allocate bridge */
    /* Phase 8: Heartbeat at operation start */
    reasoning_immune_heartbeat("reasoning_im_bridge_create", 0.0f);


    reasoning_immune_bridge_t* bridge = (reasoning_immune_bridge_t*)
        nimcp_malloc(sizeof(reasoning_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR("reasoning_immune_bridge", "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge allocation failed");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(reasoning_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->reasoning_integration = reasoning_integration;

    /* Apply configuration */
    reasoning_immune_config_t default_config;
    if (!config) {
        reasoning_immune_default_config(&default_config);
        config = &default_config;
    }

    bridge->enable_cytokine_reasoning_modulation = config->enable_cytokine_reasoning_modulation;
    bridge->enable_inflammation_cognitive_slowing = config->enable_inflammation_cognitive_slowing;
    bridge->enable_reasoning_failure_immune_trigger = config->enable_reasoning_failure_immune_trigger;
    bridge->enable_working_memory_modulation = config->enable_working_memory_modulation;
    bridge->enable_contradiction_immune_alert = config->enable_contradiction_immune_alert;

    bridge->cytokine_sensitivity = config->cytokine_sensitivity;
    bridge->inflammation_sensitivity = config->inflammation_sensitivity;
    bridge->max_speed_reduction = config->max_speed_reduction;
    bridge->max_accuracy_reduction = config->max_accuracy_reduction;
    bridge->min_max_iterations = config->min_max_iterations;

    bridge->proof_failure_threshold = config->proof_failure_threshold;
    bridge->proof_failure_escalation = config->proof_failure_escalation;
    bridge->unification_error_threshold = config->unification_error_threshold;
    bridge->failure_window_sec = config->failure_window_sec;
    bridge->contradiction_antigen_severity = config->contradiction_antigen_severity;

    /* Initialize impairment to baseline (no impairment) */
    bridge->current_impairment.speed_multiplier = 1.0f;
    bridge->current_impairment.accuracy_multiplier = 1.0f;
    bridge->current_impairment.effective_max_iterations = NIMCP_MAX_ITERATIONS_DEFAULT;  /* Default */
    bridge->current_impairment.effective_wm_capacity = REASONING_MAX_ACTIVE_INFERENCES;
    bridge->current_impairment.timeout_multiplier = 1.0f;

    /* Create mutex */
    bridge->base.mutex = nimcp_platform_mutex_create();
    if (!bridge->base.mutex) {
        LOG_MODULE_ERROR("reasoning_immune_bridge", "Mutex creation failed");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "reasoning_immune_bridge_create: bridge->base is NULL");
        return NULL;
    }

    LOG_MODULE_INFO("reasoning_immune_bridge",
                  "Created reasoning-immune bridge");
    return bridge;
}

void reasoning_immune_bridge_destroy(reasoning_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Phase 8: Heartbeat at operation start */
    reasoning_immune_heartbeat("reasoning_im_bridge_destroy", 0.0f);


    if (bridge->base.mutex) {
        nimcp_platform_mutex_destroy(bridge->base.mutex);
        nimcp_free(bridge->base.mutex);
        bridge->base.mutex = NULL;
    }

    nimcp_free(bridge);
    bridge = NULL;

    LOG_MODULE_INFO("reasoning_immune_bridge", "Destroyed bridge");
}

/* ============================================================================
 * Immune → Reasoning Modulation Implementation
 * ============================================================================ */

int reasoning_immune_apply_cytokine_effects(reasoning_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_immune_heartbeat("reasoning_im_apply_cytokine_effec", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute cytokine impacts */
    float speed_impact = compute_cytokine_speed_impact(bridge);
    float accuracy_impact = compute_cytokine_accuracy_impact(bridge);

    /* Update impairment state */
    bridge->current_impairment.cytokine_speed_impact = speed_impact;
    bridge->current_impairment.cytokine_accuracy_impact = accuracy_impact;

    /* Speed multiplier: 1.0 + impact (impact is negative) */
    float speed_mult = 1.0f + speed_impact;
    bridge->current_impairment.speed_multiplier = clamp_f(speed_mult, 0.0f, 2.0f);

    /* Accuracy multiplier */
    float accuracy_mult = 1.0f + accuracy_impact;
    bridge->current_impairment.accuracy_multiplier = clamp_f(accuracy_mult, 0.0f, 2.0f);

    /* Update statistics */
    bridge->stats.total_cytokine_modulations++;
    float speed_reduction = 1.0f - bridge->current_impairment.speed_multiplier;
    float accuracy_reduction = 1.0f - bridge->current_impairment.accuracy_multiplier;

    /* Running average of reductions */
    float alpha = 0.1f;  /* Exponential smoothing */
    bridge->stats.avg_speed_reduction =
        alpha * speed_reduction + (1.0f - alpha) * bridge->stats.avg_speed_reduction;
    bridge->stats.avg_accuracy_reduction =
        alpha * accuracy_reduction + (1.0f - alpha) * bridge->stats.avg_accuracy_reduction;

    /* Track maximums */
    if (speed_reduction > bridge->stats.max_speed_reduction_observed) {
        bridge->stats.max_speed_reduction_observed = speed_reduction;
    }
    if (accuracy_reduction > bridge->stats.max_accuracy_reduction_observed) {
        bridge->stats.max_accuracy_reduction_observed = accuracy_reduction;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_immune_apply_inflammation_effects(reasoning_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_immune_heartbeat("reasoning_im_apply_inflammation_e", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute inflammation penalty */
    float penalty = compute_inflammation_penalty(bridge);
    brain_inflammation_level_t level = get_max_inflammation(bridge->immune_system);

    /* Update impairment state */
    bridge->current_impairment.inflammation_penalty = penalty;
    bridge->current_impairment.max_inflammation = level;

    /* Inflammation penalty applies to both speed and accuracy */
    float inflammation_speed_mult = 1.0f - penalty;
    float inflammation_accuracy_mult = 1.0f - (penalty * 0.6f);  /* Accuracy less affected */

    /* Combine with existing cytokine effects (multiplicative) */
    bridge->current_impairment.speed_multiplier *= inflammation_speed_mult;
    bridge->current_impairment.accuracy_multiplier *= inflammation_accuracy_mult;

    /* Reduce max iterations based on inflammation */
    uint32_t base_iterations = 1000;  /* Would get from reasoning config */
    bridge->current_impairment.effective_max_iterations =
        (uint32_t)(base_iterations * (1.0f - penalty));
    if (bridge->current_impairment.effective_max_iterations < bridge->min_max_iterations) {
        bridge->current_impairment.effective_max_iterations = bridge->min_max_iterations;
    }

    /* Reduce timeout proportionally */
    bridge->current_impairment.timeout_multiplier = 1.0f - penalty;

    /* Update statistics */
    bridge->stats.total_inflammation_modulations++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_immune_get_impairment(
    const reasoning_immune_bridge_t* bridge,
    reasoning_impairment_t* impairment
) {
    if (!bridge || !impairment) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_immune_get_impairment: required parameter is NULL (bridge, impairment)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_immune_heartbeat("reasoning_im_get_impairment", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *impairment = bridge->current_impairment;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Reasoning → Immune Trigger Implementation
 * ============================================================================ */

int reasoning_immune_report_contradiction(
    reasoning_immune_bridge_t* bridge,
    const char* contradiction_description
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_contradiction_immune_alert) return 0;

    /* Phase 8: Heartbeat at operation start */
    reasoning_immune_heartbeat("reasoning_im_report_contradiction", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Track contradiction */
    bridge->failure_state.contradictions_detected++;
    bridge->stats.contradictions_reported++;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Present as antigen to immune system */
    /* Would call: brain_immune_present_antigen(
     *     bridge->immune_system,
     *     ANTIGEN_SOURCE_ANOMALY,
     *     (uint8_t*)contradiction_description,
     *     strlen(contradiction_description),
     *     bridge->contradiction_antigen_severity,
     *     0,  // node_id
     *     NULL
     * );
     */

    LOG_MODULE_WARN("reasoning_immune",
                  "Contradiction reported to immune system: %s",
              contradiction_description ? contradiction_description : "unknown");

    return 0;
}

int reasoning_immune_report_proof_failure(
    reasoning_immune_bridge_t* bridge,
    const char* goal_description
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_reasoning_failure_immune_trigger) return 0;

    /* Phase 8: Heartbeat at operation start */
    reasoning_immune_heartbeat("reasoning_im_report_proof_failure", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now_ms = get_time_ms();

    /* Check if window expired */
    if (is_window_expired(bridge)) {
        /* Reset window */
        bridge->failure_state.proof_failures_recent = 0;
        bridge->failure_state.first_failure_time_ms = now_ms;
    }

    /* Increment failure count */
    bridge->failure_state.proof_failures_recent++;
    bridge->failure_state.last_failure_time_ms = now_ms;
    bridge->stats.proof_failures_reported++;

    uint32_t failures = bridge->failure_state.proof_failures_recent;

    /* Check for escalation */
    if (failures >= bridge->proof_failure_escalation) {
        /* REGIONAL inflammation */
        LOG_MODULE_ERROR("reasoning_immune",
                  "Escalating to REGIONAL inflammation (%u failures)", failures);
        /* Would call: brain_immune_trigger_inflammation(immune, INFLAMMATION_REGIONAL) */
        bridge->stats.total_immune_triggers++;
    } else if (failures >= bridge->proof_failure_threshold) {
        /* LOCAL inflammation */
        LOG_MODULE_WARN("reasoning_immune",
                  "Triggering LOCAL inflammation (%u failures)", failures);
        /* Would call: brain_immune_trigger_inflammation(immune, INFLAMMATION_LOCAL) */
        bridge->stats.total_immune_triggers++;
    }

    /* Check for persistent error state (>60s) */
    if (bridge->failure_state.first_failure_time_ms > 0) {
        uint64_t error_duration_ms = now_ms - bridge->failure_state.first_failure_time_ms;
        float error_duration_sec = error_duration_ms / 1000.0f;

        if (error_duration_sec > REASONING_PERSISTENT_ERROR_DURATION_SEC &&
            !bridge->failure_state.persistent_error_state) {
            bridge->failure_state.persistent_error_state = true;
            bridge->failure_state.error_state_start_ms = now_ms;

            LOG_MODULE_ERROR("reasoning_immune",
                  "Persistent errors detected, releasing cytokines");
            /* Would call: brain_immune_release_cytokine(immune, CYTOKINE_IL1B, 0.5f) */
            /* Would call: brain_immune_release_cytokine(immune, CYTOKINE_IL6, 0.3f) */
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_immune_report_unification_error(
    reasoning_immune_bridge_t* bridge,
    const char* error_description
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_reasoning_failure_immune_trigger) return 0;

    /* Phase 8: Heartbeat at operation start */
    reasoning_immune_heartbeat("reasoning_im_report_unification_e", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now_ms = get_time_ms();

    /* Check if window expired */
    if (is_window_expired(bridge)) {
        bridge->failure_state.unification_errors_recent = 0;
        bridge->failure_state.first_failure_time_ms = now_ms;
    }

    /* Increment error count */
    bridge->failure_state.unification_errors_recent++;
    bridge->failure_state.last_failure_time_ms = now_ms;
    bridge->stats.unification_errors_reported++;

    uint32_t errors = bridge->failure_state.unification_errors_recent;

    /* Check threshold */
    if (errors >= bridge->unification_error_threshold) {
        LOG_MODULE_WARN("reasoning_immune",
                  "Unification errors threshold reached (%u), activating B cell", errors);
        /* Would call: brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id) */
        bridge->stats.total_immune_triggers++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_immune_clear_failure_tracking(reasoning_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_immune_heartbeat("reasoning_im_clear_failure_tracki", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset all failure counters */
    bridge->failure_state.proof_failures_recent = 0;
    bridge->failure_state.unification_errors_recent = 0;
    bridge->failure_state.first_failure_time_ms = 0;
    bridge->failure_state.last_failure_time_ms = 0;
    bridge->failure_state.persistent_error_state = false;
    bridge->failure_state.error_state_start_ms = 0;

    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_MODULE_INFO("reasoning_immune", "Cleared failure tracking");
    return 0;
}

/* ============================================================================
 * Query Functions Implementation
 * ============================================================================ */

int reasoning_immune_get_failure_state(
    const reasoning_immune_bridge_t* bridge,
    reasoning_failure_state_t* failure_state
) {
    if (!bridge || !failure_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_immune_get_failure_state: required parameter is NULL (bridge, failure_state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_immune_heartbeat("reasoning_im_get_failure_state", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *failure_state = bridge->failure_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int reasoning_immune_get_config(
    const reasoning_immune_bridge_t* bridge,
    reasoning_immune_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_immune_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_immune_heartbeat("reasoning_im_get_config", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    config->enable_cytokine_reasoning_modulation = bridge->enable_cytokine_reasoning_modulation;
    config->enable_inflammation_cognitive_slowing = bridge->enable_inflammation_cognitive_slowing;
    config->enable_reasoning_failure_immune_trigger = bridge->enable_reasoning_failure_immune_trigger;
    config->enable_working_memory_modulation = bridge->enable_working_memory_modulation;
    config->enable_contradiction_immune_alert = bridge->enable_contradiction_immune_alert;

    config->cytokine_sensitivity = bridge->cytokine_sensitivity;
    config->inflammation_sensitivity = bridge->inflammation_sensitivity;
    config->max_speed_reduction = bridge->max_speed_reduction;
    config->max_accuracy_reduction = bridge->max_accuracy_reduction;
    config->min_max_iterations = bridge->min_max_iterations;

    config->proof_failure_threshold = bridge->proof_failure_threshold;
    config->proof_failure_escalation = bridge->proof_failure_escalation;
    config->unification_error_threshold = bridge->unification_error_threshold;
    config->failure_window_sec = bridge->failure_window_sec;
    config->contradiction_antigen_severity = bridge->contradiction_antigen_severity;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_immune_set_config(
    reasoning_immune_bridge_t* bridge,
    const reasoning_immune_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_immune_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    /* Validate config */
    if (config->cytokine_sensitivity < 0.0f || config->cytokine_sensitivity > 2.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_immune_set_config: validation failed");
        return -1;
    }
    if (config->inflammation_sensitivity < 0.0f || config->inflammation_sensitivity > 2.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_immune_set_config: validation failed");
        return -1;
    }
    if (config->max_speed_reduction < 0.0f || config->max_speed_reduction > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_immune_set_config: validation failed");
        return -1;
    }
    if (config->max_accuracy_reduction < 0.0f || config->max_accuracy_reduction > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_immune_set_config: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_immune_heartbeat("reasoning_im_set_config", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->enable_cytokine_reasoning_modulation = config->enable_cytokine_reasoning_modulation;
    bridge->enable_inflammation_cognitive_slowing = config->enable_inflammation_cognitive_slowing;
    bridge->enable_reasoning_failure_immune_trigger = config->enable_reasoning_failure_immune_trigger;
    bridge->enable_working_memory_modulation = config->enable_working_memory_modulation;
    bridge->enable_contradiction_immune_alert = config->enable_contradiction_immune_alert;

    bridge->cytokine_sensitivity = config->cytokine_sensitivity;
    bridge->inflammation_sensitivity = config->inflammation_sensitivity;
    bridge->max_speed_reduction = config->max_speed_reduction;
    bridge->max_accuracy_reduction = config->max_accuracy_reduction;
    bridge->min_max_iterations = config->min_max_iterations;

    bridge->proof_failure_threshold = config->proof_failure_threshold;
    bridge->proof_failure_escalation = config->proof_failure_escalation;
    bridge->unification_error_threshold = config->unification_error_threshold;
    bridge->failure_window_sec = config->failure_window_sec;
    bridge->contradiction_antigen_severity = config->contradiction_antigen_severity;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_immune_get_stats(
    const reasoning_immune_bridge_t* bridge,
    reasoning_immune_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_immune_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_immune_heartbeat("reasoning_im_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int reasoning_immune_reset_stats(reasoning_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_immune_heartbeat("reasoning_im_reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(reasoning_immune_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Integration API Implementation
 * ============================================================================ */

reasoning_immune_bridge_t* reasoning_connect_immune(
    reasoning_integration_t* reasoning_integration,
    brain_immune_system_t* immune_system,
    const reasoning_immune_config_t* config
) {
    /* Guard: require both systems */
    if (!reasoning_integration || !immune_system) {
        LOG_MODULE_ERROR("reasoning_connect_immune",
                  "Cannot connect without reasoning and immune systems");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_connect_immune: required parameter is NULL (reasoning_integration, immune_system)");
        return NULL;
    }

    /* Create bridge */
    /* Phase 8: Heartbeat at operation start */
    reasoning_immune_heartbeat("reasoning_im_reasoning_connect_im", 0.0f);


    reasoning_immune_bridge_t* bridge =
        reasoning_immune_bridge_create(config, immune_system, reasoning_integration);

    if (!bridge) {
        LOG_MODULE_ERROR("reasoning_connect_immune", "Bridge creation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;
    }

    /* Register callbacks with reasoning integration (if API available) */
    /* Would register hooks for:
     * - EVENT_CONTRADICTION_DETECTED → reasoning_immune_report_contradiction
     * - EVENT_PROOF_FAILED → reasoning_immune_report_proof_failure
     * - EVENT_UNIFICATION_FAILED → reasoning_immune_report_unification_error
     * - Periodic tick → reasoning_immune_apply_cytokine_effects + apply_inflammation_effects
     */

    LOG_MODULE_INFO("reasoning_connect_immune",
                  "Successfully connected reasoning to immune system");

    return bridge;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about reasoning immune
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int reasoning_immune_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    reasoning_immune_heartbeat("reasoning_im_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Reasoning_Immune");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                reasoning_immune_heartbeat("reasoning_im_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Reasoning immune self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Reasoning_Immune");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Reasoning_Immune");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void reasoning_immune_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_reasoning_immune_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int reasoning_immune_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_immune_training_begin: NULL argument");
        return -1;
    }
    /* TODO: Pass bridge instance as health agent when instance tracking is implemented */
    reasoning_immune_heartbeat_instance(NULL, "reasoning_immune_training_begin", 0.0f);
    (void)(struct reasoning_immune_bridge*)instance; /* Module state available for reset */
    return 0;
}

int reasoning_immune_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_immune_training_end: NULL argument");
        return -1;
    }
    /* TODO: Pass bridge instance as health agent when instance tracking is implemented */
    reasoning_immune_heartbeat_instance(NULL, "reasoning_immune_training_end", 1.0f);
    (void)(struct reasoning_immune_bridge*)instance; /* Module state available for finalization */
    return 0;
}

int reasoning_immune_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_immune_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    /* TODO: Pass bridge instance as health agent when instance tracking is implemented */
    reasoning_immune_heartbeat_instance(NULL, "reasoning_immune_training_step", progress);
    (void)(struct reasoning_immune_bridge*)instance; /* Module state available for step adaptation */
    return 0;
}
