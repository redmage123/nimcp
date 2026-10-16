/**
 * @file nimcp_reasoning_portia_bridge.h
 * @brief Portia-Reasoning Bridge — resource-aware reasoning budget system
 *
 * WHAT: Bridges Portia's resource monitoring with the reasoning chain engine
 * WHY:  Reasoning is expensive; under degradation we must shed phases gracefully
 * HOW:  Queries Portia status, computes a reasoning_budget_t, applies it to config
 *
 * INTEGRATION:
 * Portia status → compute_budget() → apply_budget(config) → engine runs with reduced phases
 *
 * @version 1.0.0
 * @date 2026-02-25
 */

#ifndef NIMCP_REASONING_PORTIA_BRIDGE_H
#define NIMCP_REASONING_PORTIA_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "portia/nimcp_portia.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Reasoning budget computed from Portia degradation state
 *
 * WHAT: Per-phase allow/deny flags plus global overrides
 * WHY:  Decouple Portia monitoring from reasoning config mutation
 * HOW:  Populated by compute_budget(), consumed by apply_budget()
 */
typedef struct {
    /* Per-phase allow flags (true = enabled, false = shed) */
    bool allow_recall;
    bool allow_knowledge;
    bool allow_world_model;
    bool allow_jepa;
    bool allow_symbolic_inference;
    bool allow_symbolic_query;
    bool allow_verification;
    bool allow_epistemic;

    /* Global overrides */
    bool allow_concurrent;          /**< Allow concurrent pipeline */
    uint32_t max_steps_override;    /**< 0 = use config default */
    float confidence_boost;         /**< Added to final confidence to compensate for fewer steps */

    /* Convergent reasoning overrides (added v2.6.4) */
    bool allow_convergent_mode;          /**< Allow convergent evidence accumulation */
    uint32_t max_convergent_contributors; /**< 0 = use config default */

    /* Source information */
    portia_degradation_level_t source_degradation; /**< Degradation level that produced this budget */
} reasoning_budget_t;

/*=============================================================================
 * API FUNCTIONS
 *===========================================================================*/

/**
 * @brief Return a full (everything-enabled) budget
 *
 * WHAT: Default budget with all phases enabled and no overrides
 * WHY:  Fallback when Portia is unavailable or healthy
 *
 * @return Full budget
 */
reasoning_budget_t reasoning_portia_full_budget(void);

/**
 * @brief Compute a reasoning budget from current Portia state
 *
 * WHAT: Query Portia, map degradation/thermal/power/CPU to phase flags
 * WHY:  Dynamically adapt reasoning depth to available resources
 *
 * @return Computed budget (full budget if Portia unavailable)
 */
reasoning_budget_t reasoning_portia_compute_budget(void);

/**
 * @brief Apply a budget to a reasoning engine config
 *
 * WHAT: Disable config flags based on budget allow flags
 * WHY:  Translate budget into engine-level configuration
 *
 * @param config Engine config to modify (non-NULL)
 * @param budget Budget to apply (non-NULL)
 * @return Number of phases disabled, or -1 on error
 */
int reasoning_portia_apply_budget(reasoning_engine_config_t* config,
                                  const reasoning_budget_t* budget);

/**
 * @brief Format budget as human-readable summary string
 *
 * @param budget Budget to summarize (non-NULL)
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Characters written, or -1 on error
 */
int reasoning_portia_budget_summary(const reasoning_budget_t* budget,
                                    char* buffer, uint32_t buffer_size);

/**
 * @brief Check if reasoning should be skipped entirely
 *
 * WHAT: Returns true if system is in combined EMERGENCY + CRITICAL thermal state
 * WHY:  Under extreme stress, even minimal reasoning wastes resources
 *
 * @return true if reasoning should be skipped
 */
bool reasoning_portia_should_skip(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REASONING_PORTIA_BRIDGE_H */
