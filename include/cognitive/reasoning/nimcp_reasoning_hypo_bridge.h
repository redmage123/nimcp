/**
 * @file nimcp_reasoning_hypo_bridge.h
 * @brief Hypothalamus-Reasoning Bridge — motivational modulation of reasoning depth
 *
 * WHAT: Queries brain's hypothalamus for circadian, stress, and autonomic state,
 *       computes a reasoning modulation that scales depth and capacity
 * WHY:  Portia controls reasoning based on HARDWARE constraints;
 *       this controls reasoning based on MOTIVATIONAL/COGNITIVE state
 * HOW:  Queries hypothalamus_get_state(), computes modulation, applies alongside Portia budget
 *
 * INTEGRATION:
 * hypothalamus state → compute_modulation() → reasoning_hypo_modulation_t → apply to config
 *
 * @version 1.0.0
 * @date 2026-02-25
 */

#ifndef NIMCP_REASONING_HYPO_BRIDGE_H
#define NIMCP_REASONING_HYPO_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declare to avoid header cycles */
struct brain_struct;
typedef struct brain_struct* brain_t;

struct hypothalamus_adapter;
typedef struct hypothalamus_adapter hypothalamus_adapter_t;

/* Need the reasoning config for apply */
#include "cognitive/reasoning/nimcp_reasoning_chain.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Cognitive urgency mode derived from autonomic state
 */
typedef enum {
    REASONING_URGENCY_RELAXED = 0,    /**< Rest-and-digest: deep, thorough reasoning */
    REASONING_URGENCY_NORMAL = 1,     /**< Balanced autonomic state */
    REASONING_URGENCY_ALERT = 2,      /**< Elevated sympathetic tone */
    REASONING_URGENCY_FIGHT_OR_FLIGHT = 3  /**< Acute stress: fast heuristic reasoning */
} reasoning_urgency_mode_t;

/**
 * @brief Hypothalamus reasoning modulation
 *
 * WHAT: Motivational/cognitive state that modulates reasoning
 * WHY:  Decouple hypothalamus query from reasoning config mutation
 * HOW:  Populated by compute_hypo_modulation(), consumed by apply
 */
typedef struct {
    /* Cognitive capacity [0,1] — product of alertness and inverse fatigue */
    float cognitive_capacity;

    /* Urgency mode from autonomic state */
    reasoning_urgency_mode_t urgency_mode;

    /* Stress level [0,1] — from cortisol/HPA axis */
    float stress_level;

    /* Circadian alertness [0,1] — from SCN */
    float alertness;

    /* Sleep pressure [0,1] — homeostatic sleep drive */
    float sleep_pressure;

    /* Recommended max_steps cap based on urgency */
    uint32_t recommended_max_steps;

    /* Whether to force sequential (high stress → avoid thread contention) */
    bool force_sequential;

    /* Convergent reasoning override (added v2.6.4) */
    bool force_wave_pipeline;     /**< Force wave pipeline (disable convergent) */

    /* Source: was hypothalamus available */
    bool hypothalamus_available;
} reasoning_hypo_modulation_t;

/*=============================================================================
 * API FUNCTIONS
 *===========================================================================*/

/**
 * @brief Return a default (neutral) modulation
 *
 * WHAT: Modulation that doesn't change anything
 * WHY:  Fallback when hypothalamus is unavailable
 *
 * @return Neutral modulation
 */
reasoning_hypo_modulation_t reasoning_hypo_neutral_modulation(void);

/**
 * @brief Compute hypothalamus modulation from brain state
 *
 * WHAT: Query hypothalamus adapter, map state to reasoning modulation
 * WHY:  Motivational state should influence reasoning depth
 *
 * @param brain Brain instance (may be NULL)
 * @return Computed modulation (neutral if brain/hypothalamus unavailable)
 */
reasoning_hypo_modulation_t reasoning_hypo_compute_modulation(brain_t brain);

/**
 * @brief Apply hypothalamus modulation to reasoning config
 *
 * WHAT: Adjust config max_steps, concurrent, etc. based on modulation
 * WHY:  Translate motivational state into engine-level config
 *
 * Rules:
 * - FIGHT_OR_FLIGHT: cap max_steps to recommended, force sequential
 * - ALERT: cap max_steps to recommended
 * - cognitive_capacity < 0.3: force sequential
 * - cognitive_capacity scales max_steps: max_steps *= cognitive_capacity
 *
 * @param config Engine config to modify (non-NULL)
 * @param mod Modulation to apply (non-NULL)
 * @return 0 on success, -1 on error
 */
int reasoning_hypo_apply_modulation(reasoning_engine_config_t* config,
                                     const reasoning_hypo_modulation_t* mod);

/**
 * @brief Format modulation as human-readable summary
 *
 * @param mod Modulation to summarize (non-NULL)
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Characters written, or -1 on error
 */
int reasoning_hypo_modulation_summary(const reasoning_hypo_modulation_t* mod,
                                       char* buffer, uint32_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REASONING_HYPO_BRIDGE_H */
