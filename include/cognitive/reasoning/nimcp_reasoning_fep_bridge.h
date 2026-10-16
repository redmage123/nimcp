/**
 * @file nimcp_reasoning_fep_bridge.h
 * @brief Free Energy Principle - Reasoning Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and reasoning system
 * WHY:  Reasoning implements inference as free energy minimization. Logical inference
 *       chains minimize variational free energy over symbolic representations.
 * HOW:  FEP prediction errors guide inference; reasoning results constrain FEP beliefs;
 *       abductive reasoning minimizes surprise.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * REASONING AS FREE ENERGY MINIMIZATION:
 * ---------------------------------------
 * - Friston (2018): "Am I self-conscious? (Or does self-organization entail
 *   self-consciousness?)" - Reasoning emerges from hierarchical inference
 * - Logical inference = minimizing surprise over symbolic representations
 * - Abduction (hypothesis generation) = minimizing free energy
 * - Deduction (consequence derivation) = predictive processing
 *
 * FEP → REASONING PATHWAYS:
 * -------------------------
 * 1. Prediction Errors Drive Inference:
 *    - High PE → Trigger abductive reasoning
 *    - Generate hypotheses to explain surprise
 *    - Reference: Peirce's abduction, Friston's generalized filtering
 *
 * 2. Free Energy Guides Hypothesis Selection:
 *    - Hypotheses with lower F preferred
 *    - Model evidence = exp(-F)
 *    - Occam's razor emerges from complexity term
 *
 * 3. Precision Weights Inference Steps:
 *    - High precision → Confident deduction
 *    - Low precision → Tentative inference
 *    - Uncertainty propagates through reasoning chains
 *
 * REASONING → FEP PATHWAYS:
 * -------------------------
 * 1. Logical Constraints on Beliefs:
 *    - Reasoning conclusions → FEP belief constraints
 *    - Logical consistency enforced on beliefs
 *    - Hard constraints vs soft probabilistic beliefs
 *
 * 2. Knowledge Base as Generative Model:
 *    - Symbolic rules → Generative model structure
 *    - Forward chaining = predictive processing
 *    - Backward chaining = evidence accumulation
 *
 * 3. Explanations Reduce Free Energy:
 *    - Good explanations reduce surprise
 *    - Explanatory coherence = low F
 *    - Reference: Thagard's coherence theory
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                  FEP-REASONING BRIDGE                                      ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                FEP → REASONING PATHWAYS                             │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ HIGH PE      │ ──→ Abductive Reasoning (generate hypotheses)   │  ║
 * ║   │   │ Surprise     │                                                 │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ FREE ENERGY  │ ──→ Hypothesis Selection (min F)                │  ║
 * ║   │   │ F(h1), F(h2) │                                                 │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ PRECISION    │ ──→ Inference Confidence                        │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              REASONING → FEP PATHWAYS                               │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ LOGICAL RULES    │ ──→ Generative Model Structure              │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ CONCLUSIONS      │ ──→ Belief Constraints                      │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ EXPLANATIONS     │ ──→ Free Energy Reduction                   │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_REASONING_FEP_BRIDGE_H
#define NIMCP_REASONING_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/reasoning/nimcp_reasoning_integration.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

#define REASONING_FEP_MAX_HYPOTHESES         16
#define REASONING_FEP_PE_ABDUCTION_THRESHOLD 5.0f
#define REASONING_FEP_PRECISION_THRESHOLD    1.0f

typedef struct reasoning_fep_bridge reasoning_fep_bridge_t;

typedef struct {
    float pe_abduction_threshold;
    float hypothesis_selection_temperature;
    float inference_precision_threshold;
    bool enable_pe_abduction;
    bool enable_fe_hypothesis_selection;
    bool enable_precision_inference;
    float rule_prior_strength;
    float conclusion_belief_strength;
    float explanation_fe_reduction;
    bool enable_rule_priors;
    bool enable_conclusion_constraints;
    bool enable_explanation_reduction;
    float fe_sensitivity;
    float reasoning_sensitivity;
} reasoning_fep_config_t;

typedef struct {
    float current_prediction_error;
    bool abduction_triggered;
    float hypothesis_free_energies[REASONING_FEP_MAX_HYPOTHESES];
    uint32_t selected_hypothesis;
    float current_precision;
    float inference_confidence;
} reasoning_fep_effects_t;

typedef struct {
    float rule_prior_bias;
    bool rule_priors_active;
    float conclusion_constraint_strength;
    bool conclusions_constraining_beliefs;
    float explanation_quality;
    float fe_reduction;
} fep_reasoning_effects_t;

typedef struct {
    float current_prediction_error;
    float current_precision;
    float current_free_energy;
    bool abduction_active;
    bool inference_active;
    uint64_t last_abduction_time;
    uint64_t last_inference_time;
} reasoning_fep_state_t;

typedef struct {
    uint64_t abduction_events;
    uint64_t hypothesis_selections;
    uint64_t inference_steps;
    float avg_prediction_error;
    float avg_hypothesis_fe;
    uint64_t rule_prior_applications;
    uint64_t conclusion_constraints;
    uint64_t explanation_reductions;
    float avg_explanation_quality;
    float avg_free_energy;
} reasoning_fep_stats_t;

struct reasoning_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    reasoning_fep_config_t config;
    fep_system_t* fep_system;
    reasoning_integration_t* reasoning_system;
    reasoning_fep_effects_t fep_effects;
    fep_reasoning_effects_t reasoning_effects;
    reasoning_fep_state_t state;
    reasoning_fep_stats_t stats;
};

int reasoning_fep_bridge_default_config(reasoning_fep_config_t* config);
reasoning_fep_bridge_t* reasoning_fep_bridge_create(const reasoning_fep_config_t* config);
void reasoning_fep_bridge_destroy(reasoning_fep_bridge_t* bridge);

int reasoning_fep_bridge_connect_fep(reasoning_fep_bridge_t* bridge, fep_system_t* fep);
int reasoning_fep_bridge_connect_reasoning(reasoning_fep_bridge_t* bridge, reasoning_integration_t* reasoning);
int reasoning_fep_bridge_disconnect(reasoning_fep_bridge_t* bridge);

int reasoning_fep_trigger_abduction(reasoning_fep_bridge_t* bridge, float pe_magnitude);
int reasoning_fep_select_hypothesis_by_fe(reasoning_fep_bridge_t* bridge);
int reasoning_fep_modulate_inference_confidence(reasoning_fep_bridge_t* bridge);

int reasoning_fep_apply_rule_priors(reasoning_fep_bridge_t* bridge);
int reasoning_fep_apply_conclusion_constraints(reasoning_fep_bridge_t* bridge);
int reasoning_fep_apply_explanation_reduction(reasoning_fep_bridge_t* bridge);

int reasoning_fep_bridge_update(reasoning_fep_bridge_t* bridge, uint64_t delta_ms);

int reasoning_fep_bridge_get_state(reasoning_fep_bridge_t* bridge, reasoning_fep_state_t* state);
int reasoning_fep_bridge_get_stats(reasoning_fep_bridge_t* bridge, reasoning_fep_stats_t* stats);

int reasoning_fep_bridge_connect_bio_async(reasoning_fep_bridge_t* bridge);
int reasoning_fep_bridge_disconnect_bio_async(reasoning_fep_bridge_t* bridge);
bool reasoning_fep_bridge_is_bio_async_connected(const reasoning_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REASONING_FEP_BRIDGE_H */
