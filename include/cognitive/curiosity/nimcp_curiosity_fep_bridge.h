/**
 * @file nimcp_curiosity_fep_bridge.h
 * @brief Free Energy Principle - Curiosity Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and curiosity system
 * WHY:  Curiosity is epistemic value under FEP (information gain reduces uncertainty);
 *       FEP prediction errors and expected free energy drive exploration
 * HOW:  FEP epistemic value drives curiosity; curiosity-driven exploration provides
 *       observations that reduce FEP uncertainty
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * CURIOSITY AS EPISTEMIC VALUE:
 * ----------------------------
 * 1. Information Gain as Curiosity:
 *    - Curiosity maximizes expected information gain
 *    - Information gain = reduction in uncertainty (entropy)
 *    - E[IG] = H[p(s)] - E[H[p(s|o)]]
 *    - Reference: Friston et al. (2017) "Active inference and epistemic value"
 *
 * 2. Expected Free Energy and Exploration:
 *    - Expected free energy G(π) = pragmatic value + epistemic value
 *    - Epistemic value = ambiguity = expected conditional entropy
 *    - Curiosity-driven policies minimize G through exploration
 *    - Reference: Friston et al. (2015) "Active inference and epistemic value"
 *
 * 3. Intrinsic Motivation from Prediction Errors:
 *    - Prediction errors signal novelty
 *    - Novelty triggers curiosity
 *    - Curiosity drives learning to reduce future surprise
 *    - Reference: Schmidhuber (2010) "Formal theory of creativity"
 *
 * FEP → CURIOSITY PATHWAYS:
 * ------------------------
 * 1. Expected Information Gain → Curiosity Intensity
 * 2. Epistemic Value → Exploration Motivation
 * 3. Model Uncertainty → Knowledge Gaps
 * 4. Ambiguity → Question Generation
 *
 * CURIOSITY → FEP PATHWAYS:
 * ------------------------
 * 1. Exploration Actions → Active Inference Policies
 * 2. Acquired Knowledge → Generative Model Updates
 * 3. Learning Progress → Belief Precision Increases
 * 4. Answered Questions → Reduced Uncertainty
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CURIOSITY_FEP_BRIDGE_H
#define NIMCP_CURIOSITY_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/curiosity/nimcp_curiosity.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define EPISTEMIC_VALUE_MAX               1.0f
#define CURIOSITY_INTENSITY_MAX           1.0f
#define KNOWLEDGE_GAP_THRESHOLD           0.5f
#define INFORMATION_GAIN_BASELINE         0.1f

/* ============================================================================
 * Structures
 * ============================================================================ */

typedef struct curiosity_fep_bridge curiosity_fep_bridge_t;

typedef struct {
    float epistemic_value_weight;
    float uncertainty_sensitivity;
    float information_gain_rate;
    float exploration_boost;
    bool enable_epistemic_curiosity;
    bool enable_knowledge_gap_detection;
    bool enable_exploration_feedback;
    bool enable_learning_updates;
} curiosity_fep_config_t;

typedef struct {
    float curiosity_boost;
    float exploration_motivation;
    float epistemic_value;
    float knowledge_gap_size;
} curiosity_fep_effects_t;

typedef struct {
    float current_epistemic_value;
    float current_uncertainty;
    float current_curiosity_level;
    float current_information_gain;
    uint32_t knowledge_gaps_detected;
    uint32_t explorations_triggered;
    uint32_t learning_updates_made;
} curiosity_fep_state_t;

typedef struct {
    uint64_t total_epistemic_triggers;
    uint64_t total_explorations;
    uint64_t total_learning_updates;
    float avg_information_gain;
    float avg_curiosity_level;
    float total_uncertainty_reduction;
} curiosity_fep_stats_t;

struct curiosity_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    curiosity_fep_config_t config;
    fep_system_t* fep_system;
    curiosity_engine_t curiosity_engine;
    curiosity_fep_effects_t effects;
    curiosity_fep_state_t state;
    curiosity_fep_stats_t stats;
};

/* ============================================================================
 * API
 * ============================================================================ */

int curiosity_fep_bridge_default_config(curiosity_fep_config_t* config);
curiosity_fep_bridge_t* curiosity_fep_bridge_create(const curiosity_fep_config_t* config);
void curiosity_fep_bridge_destroy(curiosity_fep_bridge_t* bridge);

int curiosity_fep_bridge_connect_fep(curiosity_fep_bridge_t* bridge, fep_system_t* fep);
int curiosity_fep_bridge_connect_curiosity(curiosity_fep_bridge_t* bridge, curiosity_engine_t curiosity);

int curiosity_fep_compute_epistemic_value(curiosity_fep_bridge_t* bridge);
int curiosity_fep_detect_knowledge_gaps(curiosity_fep_bridge_t* bridge);
int curiosity_fep_trigger_exploration(curiosity_fep_bridge_t* bridge);
int curiosity_fep_update_model_from_learning(curiosity_fep_bridge_t* bridge);

int curiosity_fep_bridge_update(curiosity_fep_bridge_t* bridge, uint64_t delta_ms);
int curiosity_fep_bridge_get_state(const curiosity_fep_bridge_t* bridge, curiosity_fep_state_t* state);
int curiosity_fep_bridge_get_stats(const curiosity_fep_bridge_t* bridge, curiosity_fep_stats_t* stats);

int curiosity_fep_bridge_connect_bio_async(curiosity_fep_bridge_t* bridge);
int curiosity_fep_bridge_disconnect_bio_async(curiosity_fep_bridge_t* bridge);
bool curiosity_fep_bridge_is_bio_async_connected(const curiosity_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CURIOSITY_FEP_BRIDGE_H */
