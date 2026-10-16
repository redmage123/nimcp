/**
 * @file nimcp_reasoning_portia_bridge.c
 * @brief Portia-Reasoning Bridge — resource-aware reasoning budget system
 *
 * WHAT: Bridges Portia's resource monitoring with the reasoning chain engine
 * WHY:  Reasoning is computationally expensive; under resource pressure we must
 *        gracefully shed phases to keep the system responsive
 * HOW:  Queries portia_get_status(), maps degradation/thermal/power/CPU to a
 *        reasoning_budget_t, then apply_budget() mutates the engine config
 *
 * DEGRADATION LADDER:
 *   NONE      → all phases enabled, concurrent allowed
 *   MINOR     → disable JEPA + epistemic (non-critical verification)
 *   MODERATE  → also disable world_model + verification + symbolic_inference,
 *               force sequential, cap at 20 steps
 *   SEVERE    → only recall + knowledge + symbolic_query + inference, sequential,
 *               cap at 10 steps, +0.05 confidence boost
 *   EMERGENCY → only recall, sequential, cap at 5 steps, +0.1 confidence boost
 *
 * Thermal/power/CPU overrides can escalate the budget further.
 *
 * @version 1.0.0
 * @date 2026-02-25
 */

#include "cognitive/reasoning/nimcp_reasoning_portia_bridge.h"
#include "portia/nimcp_portia.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>
#include <stdio.h>

#define LOG_MODULE "reasoning_portia"

/*=============================================================================
 * FULL BUDGET (everything enabled)
 *===========================================================================*/

reasoning_budget_t reasoning_portia_full_budget(void) {
    reasoning_budget_t budget;
    memset(&budget, 0, sizeof(budget));

    budget.allow_recall             = true;
    budget.allow_knowledge          = true;
    budget.allow_world_model        = true;
    budget.allow_jepa               = true;
    budget.allow_symbolic_inference = true;
    budget.allow_symbolic_query     = true;
    budget.allow_verification       = true;
    budget.allow_epistemic          = true;
    budget.allow_concurrent         = true;
    budget.max_steps_override       = 0;
    budget.confidence_boost         = 0.0f;
    budget.allow_convergent_mode    = true;
    budget.max_convergent_contributors = 0;
    budget.source_degradation       = PORTIA_DEGRADATION_NONE;

    return budget;
}

/*=============================================================================
 * BUDGET HELPERS (static)
 *===========================================================================*/

/**
 * @brief Apply SEVERE degradation rules to a budget
 *
 * Only recall + knowledge + symbolic_query + inference remain enabled.
 * Sequential, max 10 steps, confidence boost 0.05.
 */
static void apply_severe_budget(reasoning_budget_t* budget) {
    budget->allow_world_model        = false;
    budget->allow_jepa               = false;
    budget->allow_symbolic_inference = false;
    budget->allow_verification       = false;
    budget->allow_epistemic          = false;
    budget->allow_concurrent         = false;
    budget->allow_convergent_mode    = false;  /* Force wave pipeline */
    budget->max_steps_override       = 10;
    budget->confidence_boost         = 0.05f;
}

/*=============================================================================
 * COMPUTE BUDGET
 *===========================================================================*/

reasoning_budget_t reasoning_portia_compute_budget(void) {
    /* Fallback: if Portia is not available, assume full resources */
    if (!portia_is_initialized()) {
        NIMCP_LOGGING_DEBUG("Portia not initialized — returning full budget");
        return reasoning_portia_full_budget();
    }

    portia_status_t status;
    memset(&status, 0, sizeof(status));

    nimcp_error_t err = portia_get_status(&status);
    if (err != NIMCP_SUCCESS) {
        NIMCP_LOGGING_WARN("portia_get_status() failed (err=%d) — returning full budget", err);
        return reasoning_portia_full_budget();
    }

    /* Start with everything enabled */
    reasoning_budget_t budget = reasoning_portia_full_budget();

    /*-----------------------------------------------------------------
     * Step 1: Apply degradation level rules
     *-----------------------------------------------------------------*/
    switch (status.degradation_level) {
        case PORTIA_DEGRADATION_NONE:
            /* All phases stay enabled */
            break;

        case PORTIA_DEGRADATION_MINOR:
            /* Disable non-critical verification phases */
            budget.allow_jepa      = false;
            budget.allow_epistemic = false;
            break;

        case PORTIA_DEGRADATION_MODERATE:
            /* Disable JEPA + epistemic (from MINOR) */
            budget.allow_jepa               = false;
            budget.allow_epistemic          = false;
            /* Additionally disable world model, verification, symbolic inference */
            budget.allow_world_model        = false;
            budget.allow_verification       = false;
            budget.allow_symbolic_inference = false;
            /* Force sequential, cap steps */
            budget.allow_concurrent         = false;
            budget.max_steps_override       = 20;
            /* Convergent allowed but with limited contributors */
            budget.allow_convergent_mode    = true;
            budget.max_convergent_contributors = 16;
            break;

        case PORTIA_DEGRADATION_SEVERE:
            /* Only recall + knowledge + symbolic_query + inference */
            apply_severe_budget(&budget);
            break;

        case PORTIA_DEGRADATION_EMERGENCY:
            /* Only recall survives */
            budget.allow_knowledge          = false;
            budget.allow_world_model        = false;
            budget.allow_jepa               = false;
            budget.allow_symbolic_inference = false;
            budget.allow_symbolic_query     = false;
            budget.allow_verification       = false;
            budget.allow_epistemic          = false;
            budget.allow_concurrent         = false;
            budget.max_steps_override       = 5;
            budget.confidence_boost         = 0.1f;
            break;

        default:
            NIMCP_LOGGING_WARN("Unknown degradation level %d — treating as NONE",
                               (int)status.degradation_level);
            break;
    }

    /*-----------------------------------------------------------------
     * Step 2: Thermal overrides (can only tighten, never loosen)
     *-----------------------------------------------------------------*/
    if (status.thermal_state == PORTIA_THERMAL_THROTTLED ||
        status.thermal_state == PORTIA_THERMAL_CRITICAL) {
        budget.allow_concurrent = false;
    }
    if (status.thermal_state == PORTIA_THERMAL_CRITICAL) {
        /* Escalate to at least SEVERE if not already worse */
        if (status.degradation_level < PORTIA_DEGRADATION_SEVERE) {
            NIMCP_LOGGING_WARN("Thermal CRITICAL — escalating to SEVERE budget");
            apply_severe_budget(&budget);
        }
    }

    /*-----------------------------------------------------------------
     * Step 3: Power overrides
     *-----------------------------------------------------------------*/
    if (status.power_state == PORTIA_POWER_BATTERY_LOW) {
        budget.allow_world_model = false;  /* world model is expensive simulation */
    }
    if (status.power_state == PORTIA_POWER_BATTERY_CRITICAL) {
        /* Escalate to at least SEVERE */
        if (status.degradation_level < PORTIA_DEGRADATION_SEVERE) {
            NIMCP_LOGGING_WARN("Battery CRITICAL — escalating to SEVERE budget");
            apply_severe_budget(&budget);
        }
    }

    /*-----------------------------------------------------------------
     * Step 4: CPU pressure override
     *-----------------------------------------------------------------*/
    if (status.cpu_usage > 0.9f) {
        budget.allow_concurrent = false;
    }

    /*-----------------------------------------------------------------
     * Store source degradation level
     *-----------------------------------------------------------------*/
    budget.source_degradation = status.degradation_level;

    NIMCP_LOGGING_DEBUG("Computed budget: degradation=%d thermal=%d power=%d cpu=%.2f",
                        (int)status.degradation_level, (int)status.thermal_state,
                        (int)status.power_state, (double)status.cpu_usage);

    return budget;
}

/*=============================================================================
 * APPLY BUDGET
 *===========================================================================*/

int reasoning_portia_apply_budget(reasoning_engine_config_t* config,
                                  const reasoning_budget_t* budget) {
    if (!config || !budget) {
        return -1;
    }

    int disabled = 0;

    /* Map budget allow flags to config enable flags */
    if (!budget->allow_recall) {
        config->enable_engram_recall = false;
        disabled++;
    }
    if (!budget->allow_knowledge) {
        config->enable_knowledge_query = false;
        disabled++;
    }
    if (!budget->allow_world_model) {
        config->enable_world_model = false;
        disabled++;
    }
    if (!budget->allow_jepa) {
        config->enable_jepa_prediction = false;
        disabled++;
    }
    if (!budget->allow_verification) {
        config->enable_predictive_verify = false;
        disabled++;
    }
    if (!budget->allow_epistemic) {
        config->enable_epistemic_check = false;
        disabled++;
    }

    /*
     * Symbolic logic subtlety: config->enable_symbolic_logic gates BOTH
     * query and inference phases. We can only disable it if both are denied.
     * If only inference is denied, we count it but leave the config flag
     * enabled — the engine will use the budget directly for finer gating.
     */
    if (!budget->allow_symbolic_query && !budget->allow_symbolic_inference) {
        config->enable_symbolic_logic = false;
        disabled += 2;  /* Both query and inference shed */
    } else if (!budget->allow_symbolic_inference) {
        /* Count inference as disabled but don't touch config flag */
        disabled++;
    } else if (!budget->allow_symbolic_query) {
        /* Count query as disabled but don't touch config flag */
        disabled++;
    }

    /* Concurrency override */
    if (!budget->allow_concurrent) {
        config->enable_concurrent_pipeline = false;
    }

    /* Step cap override */
    if (budget->max_steps_override > 0) {
        config->max_steps = budget->max_steps_override;
    }

    /* Convergent reasoning overrides */
    if (!budget->allow_convergent_mode) {
        config->enable_convergent_reasoning = false;
    }
    if (budget->max_convergent_contributors > 0) {
        config->max_convergent_contributors = budget->max_convergent_contributors;
    }

    return disabled;
}

/*=============================================================================
 * BUDGET SUMMARY
 *===========================================================================*/

int reasoning_portia_budget_summary(const reasoning_budget_t* budget,
                                    char* buffer, uint32_t buffer_size) {
    if (!budget || !buffer || buffer_size == 0) {
        return -1;
    }

    /* Map degradation level to name */
    const char* level_name;
    switch (budget->source_degradation) {
        case PORTIA_DEGRADATION_NONE:      level_name = "NONE";      break;
        case PORTIA_DEGRADATION_MINOR:     level_name = "MINOR";     break;
        case PORTIA_DEGRADATION_MODERATE:  level_name = "MODERATE";  break;
        case PORTIA_DEGRADATION_SEVERE:    level_name = "SEVERE";    break;
        case PORTIA_DEGRADATION_EMERGENCY: level_name = "EMERGENCY"; break;
        default:                           level_name = "UNKNOWN";   break;
    }

    int written = snprintf(buffer, buffer_size,
        "Portia budget [%s]:"
        " concurrent=%s"
        " recall=%s"
        " knowledge=%s"
        " world_model=%s"
        " jepa=%s"
        " sym_query=%s"
        " sym_infer=%s"
        " verify=%s"
        " epistemic=%s"
        " max_steps=%u"
        " conf_boost=%.2f",
        level_name,
        budget->allow_concurrent         ? "yes" : "no",
        budget->allow_recall             ? "yes" : "no",
        budget->allow_knowledge          ? "yes" : "no",
        budget->allow_world_model        ? "yes" : "no",
        budget->allow_jepa               ? "yes" : "no",
        budget->allow_symbolic_query     ? "yes" : "no",
        budget->allow_symbolic_inference ? "yes" : "no",
        budget->allow_verification       ? "yes" : "no",
        budget->allow_epistemic          ? "yes" : "no",
        budget->max_steps_override,
        (double)budget->confidence_boost);

    return written;
}

/*=============================================================================
 * SHOULD SKIP
 *===========================================================================*/

bool reasoning_portia_should_skip(void) {
    if (!portia_is_initialized()) {
        /* Can't determine resource state — don't skip (safe default) */
        return false;
    }

    portia_status_t status;
    memset(&status, 0, sizeof(status));

    nimcp_error_t err = portia_get_status(&status);
    if (err != NIMCP_SUCCESS) {
        /* Can't get status — don't skip */
        return false;
    }

    if (status.degradation_level == PORTIA_DEGRADATION_EMERGENCY &&
        status.thermal_state == PORTIA_THERMAL_CRITICAL) {
        NIMCP_LOGGING_WARN("EMERGENCY + CRITICAL thermal — reasoning should be skipped");
        return true;
    }

    return false;
}
