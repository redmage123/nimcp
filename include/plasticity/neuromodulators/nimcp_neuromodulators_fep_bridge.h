/**
 * @file nimcp_neuromodulators_fep_bridge.h
 * @brief Free Energy Principle - Neuromodulators Integration Bridge
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between Free Energy Principle and neuromodulator systems
 * WHY:  Neuromodulators encode FEP signals: DA=reward PE, ACh=precision, NE=uncertainty, 5-HT=complexity.
 *       Essential for biochemical implementation of active inference and precision optimization.
 * HOW:  FEP prediction errors drive DA release; FEP precision modulates ACh; FEP uncertainty drives NE;
 *       Neuromodulator levels feedback to modulate FEP learning rates and precision estimates.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * NEUROMODULATORS AS FEP SIGNALS (Friston et al., 2012):
 * -------------------------------------------------------
 * 1. Dopamine = Reward Prediction Error:
 *    - DA phasic bursts = positive PE (better than expected)
 *    - DA dips = negative PE (worse than expected)
 *    - δ = r - V(s) in RL = prediction error in FEP
 *    - Reference: Friston et al. (2012) "Dopamine, affordance and active inference"
 *
 * 2. Acetylcholine = Precision/Salience:
 *    - ACh increases = high precision (attend to this)
 *    - ACh levels gate sensory vs prior influence
 *    - Precision-weighting of prediction errors
 *    - Reference: Yu & Dayan (2005) "Uncertainty, neuromodulation, and attention"
 *
 * 3. Norepinephrine = Expected Uncertainty:
 *    - NE tracks volatility/changepoint probability
 *    - Modulates learning rate (meta-learning)
 *    - Exploration vs exploitation balance
 *    - Reference: Doya (2002) "Metalearning and neuromodulation"
 *
 * 4. Serotonin = Temporal Horizon/Complexity:
 *    - 5-HT promotes long-term planning (low discount)
 *    - Inhibits impulsive responses to immediate PE
 *    - Complexity cost modulation
 *    - Reference: Cools et al. (2011) "Serotonin and cognitive flexibility"
 *
 * FEP → NEUROMODULATOR PATHWAYS:
 * -------------------------------
 * - Prediction error → dopamine release
 * - Precision estimates → acetylcholine levels
 * - Expected free energy → norepinephrine (exploration)
 * - Complexity cost → serotonin modulation
 *
 * NEUROMODULATOR → FEP PATHWAYS:
 * -------------------------------
 * - Dopamine levels → learning rate scaling
 * - Acetylcholine levels → precision estimates
 * - Norepinephrine levels → temperature/exploration
 * - Serotonin levels → temporal discount factor
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROMODULATORS_FEP_BRIDGE_H
#define NIMCP_NEUROMODULATORS_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NEUROMOD_FEP_DA_PE_GAIN         1.0f    /**< DA response to PE */
#define NEUROMOD_FEP_ACH_PRECISION_GAIN 0.8f    /**< ACh response to precision */
#define NEUROMOD_FEP_NE_UNCERTAINTY_GAIN 0.7f   /**< NE response to uncertainty */
#define NEUROMOD_FEP_5HT_COMPLEXITY_GAIN 0.5f   /**< 5-HT response to complexity */

/* ============================================================================
 * Structures
 * ============================================================================ */

typedef struct neuromod_fep_bridge neuromod_fep_bridge_t;

typedef struct {
    float da_pe_sensitivity;
    float ach_precision_sensitivity;
    float ne_uncertainty_sensitivity;
    float sht_complexity_sensitivity;
    bool enable_da_pe_coupling;
    bool enable_ach_precision_coupling;
    bool enable_ne_uncertainty_coupling;
    bool enable_sht_complexity_coupling;
} neuromod_fep_config_t;

typedef struct {
    float pe_magnitude;
    float da_release;
    float precision_value;
    float ach_release;
    float uncertainty_value;
    float ne_release;
    float complexity_value;
    float sht_release;
} neuromod_fep_effects_t;

typedef struct {
    float da_level;
    float ach_level;
    float ne_level;
    float sht_level;
    float learning_rate_modulation;
    float precision_modulation;
} neuromod_fep_feedback_t;

typedef struct {
    uint64_t total_updates;
    uint64_t da_releases;
    uint64_t ach_releases;
    uint64_t ne_releases;
    float avg_da_level;
    float avg_ach_level;
} neuromod_fep_stats_t;

struct neuromod_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    neuromod_fep_config_t config;
    fep_system_t* fep_system;
    neuromodulator_system_t neuromod_system;
    neuromod_fep_effects_t fep_effects;
    neuromod_fep_feedback_t neuromod_effects;
    neuromod_fep_stats_t stats;
};

/* ============================================================================
 * API
 * ============================================================================ */

int neuromod_fep_bridge_default_config(neuromod_fep_config_t* config);
neuromod_fep_bridge_t* neuromod_fep_bridge_create(const neuromod_fep_config_t* config);
void neuromod_fep_bridge_destroy(neuromod_fep_bridge_t* bridge);

int neuromod_fep_bridge_connect_fep(neuromod_fep_bridge_t* bridge, fep_system_t* fep);
int neuromod_fep_bridge_connect_neuromod(neuromod_fep_bridge_t* bridge, neuromodulator_system_t neuromod);
int neuromod_fep_bridge_disconnect(neuromod_fep_bridge_t* bridge);

float neuromod_fep_compute_da_from_pe(neuromod_fep_bridge_t* bridge, float pe);
float neuromod_fep_compute_ach_from_precision(neuromod_fep_bridge_t* bridge, float precision);
float neuromod_fep_compute_ne_from_uncertainty(neuromod_fep_bridge_t* bridge, float uncertainty);
float neuromod_fep_get_learning_rate_modulation(const neuromod_fep_bridge_t* bridge);

int neuromod_fep_bridge_update(neuromod_fep_bridge_t* bridge, uint64_t delta_ms);
int neuromod_fep_bridge_get_stats(const neuromod_fep_bridge_t* bridge, neuromod_fep_stats_t* stats);

int neuromod_fep_bridge_connect_bio_async(neuromod_fep_bridge_t* bridge);
int neuromod_fep_bridge_disconnect_bio_async(neuromod_fep_bridge_t* bridge);
bool neuromod_fep_bridge_is_bio_async_connected(const neuromod_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROMODULATORS_FEP_BRIDGE_H */
