/**
 * @file nimcp_symbolic_logic_fep_bridge.h
 * @brief Free Energy Principle - Symbolic Logic Integration Bridge
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between Free Energy Principle and symbolic logic
 * WHY:  Logical inference formalizes FEP belief updates; FEP provides probabilistic
 *       weighting to symbolic rules; hybrid neuro-symbolic reasoning emerges.
 * HOW:  FEP uncertainty weights logical rules; symbolic proofs validate FEP inferences;
 *       fact salience guides precision allocation.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SYMBOLIC LOGIC AS FEP FORMALIZATION:
 * -------------------------------------
 * - Logical rules = structured generative model
 * - Inference = belief propagation
 * - Reference: Pearl (1988) "Probabilistic Reasoning in Intelligent Systems"
 *
 * FEP → SYMBOLIC LOGIC PATHWAYS:
 * ------------------------------
 * 1. Uncertainty Weights Rules:
 *    - High confidence facts → high salience
 *    - Low precision → exploratory inference
 *    - FEP beliefs → logic priors
 *
 * 2. Prediction Errors Guide Exploration:
 *    - High PE → trigger curiosity-driven inference
 *    - Novelty → logical exploration
 *    - Surprise → hypothesis generation
 *
 * SYMBOLIC LOGIC → FEP PATHWAYS:
 * --------------------------------
 * 1. Logical Proofs Validate Beliefs:
 *    - Proven facts → high precision
 *    - Uncertain conclusions → low precision
 *    - Rule confidence → belief strength
 *
 * 2. Fact Novelty Drives FEP:
 *    - Novel logical facts → high free energy
 *    - Contradictions → belief revision
 *    - Salience → attention allocation
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SYMBOLIC_LOGIC_FEP_BRIDGE_H
#define NIMCP_SYMBOLIC_LOGIC_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOGIC_FEP_HIGH_PE_THRESHOLD       4.5f
#define LOGIC_FEP_PROOF_PRECISION_FACTOR  3.0f

typedef struct symbolic_logic_fep_bridge symbolic_logic_fep_bridge_t;

typedef struct {
    float pe_exploration_threshold;
    float proof_precision_factor;
    bool enable_pe_exploration;
    bool enable_proof_validation;
    bool enable_novelty_weighting;
    bool enable_salience_precision;
    float fe_sensitivity;
    float logic_sensitivity;
} symbolic_logic_fep_config_t;

typedef struct {
    float current_prediction_error;
    bool logical_exploration_triggered;
    float fact_salience_weights[256];
    uint32_t num_salient_facts;
    bool hypothesis_generation_active;
} symbolic_logic_fep_effects_t;

typedef struct {
    float proof_precision_boost;
    bool logic_validating_beliefs;
    float fact_novelty_score;
    bool belief_revision_triggered;
} fep_symbolic_logic_effects_t;

typedef struct {
    float current_prediction_error;
    bool exploration_active;
    uint32_t num_salient_facts;
    uint64_t last_exploration_time;
} symbolic_logic_fep_state_t;

typedef struct {
    uint64_t exploration_events;
    uint64_t proof_validations;
    uint64_t novelty_detections;
    float avg_prediction_error;
    uint64_t belief_revisions;
    float avg_free_energy;
} symbolic_logic_fep_stats_t;

struct symbolic_logic_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    symbolic_logic_fep_config_t config;
    fep_system_t* fep_system;
    symbolic_logic_t* logic_system;
    symbolic_logic_fep_effects_t fep_effects;
    fep_symbolic_logic_effects_t logic_effects;
    symbolic_logic_fep_state_t state;
    symbolic_logic_fep_stats_t stats;
};

int symbolic_logic_fep_bridge_default_config(symbolic_logic_fep_config_t* config);
symbolic_logic_fep_bridge_t* symbolic_logic_fep_bridge_create(const symbolic_logic_fep_config_t* config);
void symbolic_logic_fep_bridge_destroy(symbolic_logic_fep_bridge_t* bridge);

int symbolic_logic_fep_bridge_connect_fep(symbolic_logic_fep_bridge_t* bridge, fep_system_t* fep);
int symbolic_logic_fep_bridge_connect_logic(symbolic_logic_fep_bridge_t* bridge, symbolic_logic_t* logic);
int symbolic_logic_fep_bridge_disconnect(symbolic_logic_fep_bridge_t* bridge);

int symbolic_logic_fep_trigger_exploration(symbolic_logic_fep_bridge_t* bridge, float pe_magnitude);
int symbolic_logic_fep_weight_facts_by_confidence(symbolic_logic_fep_bridge_t* bridge);

int symbolic_logic_fep_validate_beliefs_by_proof(symbolic_logic_fep_bridge_t* bridge);
int symbolic_logic_fep_trigger_revision_from_contradiction(symbolic_logic_fep_bridge_t* bridge);

int symbolic_logic_fep_bridge_update(symbolic_logic_fep_bridge_t* bridge, uint64_t delta_ms);
int symbolic_logic_fep_bridge_get_state(symbolic_logic_fep_bridge_t* bridge, symbolic_logic_fep_state_t* state);
int symbolic_logic_fep_bridge_get_stats(symbolic_logic_fep_bridge_t* bridge, symbolic_logic_fep_stats_t* stats);

int symbolic_logic_fep_bridge_connect_bio_async(symbolic_logic_fep_bridge_t* bridge);
int symbolic_logic_fep_bridge_disconnect_bio_async(symbolic_logic_fep_bridge_t* bridge);
bool symbolic_logic_fep_bridge_is_bio_async_connected(const symbolic_logic_fep_bridge_t* bridge);

/* FEP Orchestrator Integration */
struct fep_orchestrator;
int symbolic_logic_fep_bridge_register_with_orchestrator(
    symbolic_logic_fep_bridge_t* bridge,
    struct fep_orchestrator* orchestrator,
    uint32_t* bridge_id_out);
int symbolic_logic_fep_bridge_unregister_from_orchestrator(
    symbolic_logic_fep_bridge_t* bridge,
    struct fep_orchestrator* orchestrator);

/* Update wrapper for FEP orchestrator callback */
int symbolic_logic_fep_bridge_update_wrapper(void* bridge_handle);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SYMBOLIC_LOGIC_FEP_BRIDGE_H */
