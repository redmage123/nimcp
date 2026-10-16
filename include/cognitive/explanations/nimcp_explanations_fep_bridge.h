/**
 * @file nimcp_explanations_fep_bridge.h
 * @brief Free Energy Principle - Explanations Integration Bridge
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between Free Energy Principle and natural explanations
 * WHY:  Explanations communicate generative model structure to reduce observer uncertainty;
 *       FEP prediction errors guide what needs explaining.
 * HOW:  FEP uncertainty determines explanation detail; explanations expose model structure;
 *       counterfactuals derive from FEP generative model.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * EXPLANATIONS AS FREE ENERGY MINIMIZATION:
 * -----------------------------------------
 * - Explanations minimize listener's free energy by providing model structure
 * - Clarity = high precision in communicated beliefs
 * - Reference: Friston & Frith (2015) "A duet for one" (communication as active inference)
 *
 * FEP → EXPLANATIONS PATHWAYS:
 * ----------------------------
 * 1. Uncertainty Guides Explanation Detail:
 *    - High free energy → detailed explanations needed
 *    - Low confidence → emphasize uncertainty
 *    - Prediction error → explain what went wrong
 *
 * 2. Generative Model Structure → Causal Chains:
 *    - FEP hierarchy → explanation levels
 *    - Beliefs → why-explanations
 *    - Prediction paths → how-explanations
 *
 * EXPLANATIONS → FEP PATHWAYS:
 * -----------------------------
 * 1. Explanations Expose Model Structure:
 *    - Causal chains reveal generative model
 *    - Counterfactuals test belief strength
 *    - Symbolic proofs formalize FEP inference
 *
 * 2. Explanation Quality Indicates Model Health:
 *    - Clear explanations → coherent model
 *    - Confused explanations → dysregulated FEP
 *    - Counterfactual success → robust beliefs
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EXPLANATIONS_FEP_BRIDGE_H
#define NIMCP_EXPLANATIONS_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_explanations.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EXPLANATIONS_FEP_HIGH_UNCERTAINTY_THRESHOLD  3.5f
#define EXPLANATIONS_FEP_DETAIL_BOOST_FACTOR         2.0f

typedef struct explanations_fep_bridge explanations_fep_bridge_t;

typedef struct {
    float uncertainty_threshold;
    float detail_boost_factor;
    bool enable_uncertainty_modulation;
    bool enable_causal_extraction;
    bool enable_counterfactual_testing;
    bool enable_model_exposure;
    float fe_sensitivity;
    float explanation_sensitivity;
} explanations_fep_config_t;

typedef struct {
    float current_uncertainty;
    bool detailed_explanation_needed;
    uint32_t explanation_depth_level;
    bool counterfactual_mode_active;
} explanations_fep_effects_t;

typedef struct {
    bool model_structure_exposed;
    float model_coherence;
    bool belief_testing_active;
} fep_explanations_effects_t;

typedef struct {
    float current_uncertainty;
    uint32_t explanation_depth;
    bool counterfactual_active;
    uint64_t last_explanation_time;
} explanations_fep_state_t;

typedef struct {
    uint64_t explanations_generated;
    uint64_t counterfactuals_tested;
    float avg_uncertainty;
    uint64_t model_exposures;
    float avg_free_energy;
} explanations_fep_stats_t;

struct explanations_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    explanations_fep_config_t config;
    fep_system_t* fep_system;
    explanation_generator_t explanation_gen;
    explanations_fep_effects_t fep_effects;
    fep_explanations_effects_t explanation_effects;
    explanations_fep_state_t state;
    explanations_fep_stats_t stats;
};

int explanations_fep_bridge_default_config(explanations_fep_config_t* config);
explanations_fep_bridge_t* explanations_fep_bridge_create(const explanations_fep_config_t* config);
void explanations_fep_bridge_destroy(explanations_fep_bridge_t* bridge);

int explanations_fep_bridge_connect_fep(explanations_fep_bridge_t* bridge, fep_system_t* fep);
int explanations_fep_bridge_connect_generator(explanations_fep_bridge_t* bridge, explanation_generator_t gen);
int explanations_fep_bridge_disconnect(explanations_fep_bridge_t* bridge);

int explanations_fep_modulate_detail(explanations_fep_bridge_t* bridge, float uncertainty);
int explanations_fep_extract_causal_chain(explanations_fep_bridge_t* bridge);
int explanations_fep_test_counterfactual(explanations_fep_bridge_t* bridge);

int explanations_fep_bridge_update(explanations_fep_bridge_t* bridge, uint64_t delta_ms);
int explanations_fep_bridge_get_state(explanations_fep_bridge_t* bridge, explanations_fep_state_t* state);
int explanations_fep_bridge_get_stats(explanations_fep_bridge_t* bridge, explanations_fep_stats_t* stats);

int explanations_fep_bridge_connect_bio_async(explanations_fep_bridge_t* bridge);
int explanations_fep_bridge_disconnect_bio_async(explanations_fep_bridge_t* bridge);
bool explanations_fep_bridge_is_bio_async_connected(const explanations_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EXPLANATIONS_FEP_BRIDGE_H */
