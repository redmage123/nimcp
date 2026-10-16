/**
 * @file nimcp_epistemic_fep_bridge.h
 * @brief Free Energy Principle - Epistemic Filter Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between FEP and epistemic filtering
 * WHY:  Epistemic value computation guides information-seeking; uncertainty estimation
 *       enables bias detection; FEP precision weights evidence quality.
 * HOW:  FEP uncertainty → epistemic value; epistemic filtering → precision updates;
 *       bias detection → model revision.
 *
 * BIOLOGICAL BASIS:
 * - Epistemic foraging = active inference minimizing expected uncertainty
 * - Curiosity = expected free energy reduction through information gain
 * - Bias detection = mismatch between prior precision and evidence precision
 * - Reference: Friston et al. (2017) "Active inference and epistemic value"
 *
 * FEP → EPISTEMIC PATHWAYS:
 * - High uncertainty → Seek information (epistemic value)
 * - Precision mismatch → Flag potential bias
 * - Expected information gain → Guide evidence collection
 *
 * EPISTEMIC → FEP PATHWAYS:
 * - Evidence quality → Update precision
 * - Bias detection → Revise priors
 * - Source reliability → Weight observations
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EPISTEMIC_FEP_BRIDGE_H
#define NIMCP_EPISTEMIC_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/epistemic/nimcp_epistemic_filter.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EPISTEMIC_FEP_HIGH_UNCERTAINTY_THRESHOLD  0.7f
#define EPISTEMIC_FEP_BIAS_PRECISION_PENALTY      0.5f

typedef struct epistemic_fep_bridge epistemic_fep_bridge_t;

typedef struct {
    float uncertainty_epistemic_threshold;
    float bias_precision_penalty;
    float evidence_quality_weight;
    bool enable_uncertainty_seeking;
    bool enable_bias_detection;
    bool enable_evidence_weighting;
    float information_gain_sensitivity;
    float source_reliability_weight;
    bool enable_source_precision;
    bool enable_quality_updates;
    float fe_sensitivity;
    float epistemic_sensitivity;
} epistemic_fep_config_t;

typedef struct {
    float current_uncertainty;
    float epistemic_value;
    bool information_seeking_active;
    float expected_information_gain;
    float bias_detected_magnitude;
} epistemic_fep_effects_t;

typedef struct {
    float evidence_precision_update;
    float source_reliability_weight;
    bool precision_updated;
    float prior_revision_magnitude;
} fep_epistemic_effects_t;

typedef struct {
    float current_uncertainty;
    float current_epistemic_value;
    bool seeking_information;
    uint32_t biases_detected;
    uint64_t last_information_seek_time;
} epistemic_fep_state_t;

typedef struct {
    uint64_t information_seeking_events;
    uint64_t bias_detections;
    uint64_t precision_updates;
    float avg_epistemic_value;
    float avg_uncertainty;
} epistemic_fep_stats_t;

struct epistemic_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    epistemic_fep_config_t config;
    fep_system_t* fep_system;
    epistemic_filter_t epistemic_filter;
    epistemic_fep_effects_t fep_effects;
    fep_epistemic_effects_t epistemic_effects;
    epistemic_fep_state_t state;
    epistemic_fep_stats_t stats;
};

int epistemic_fep_bridge_default_config(epistemic_fep_config_t* config);
epistemic_fep_bridge_t* epistemic_fep_bridge_create(const epistemic_fep_config_t* config);
void epistemic_fep_bridge_destroy(epistemic_fep_bridge_t* bridge);

int epistemic_fep_bridge_connect_fep(epistemic_fep_bridge_t* bridge, fep_system_t* fep);
int epistemic_fep_bridge_connect_epistemic(epistemic_fep_bridge_t* bridge, epistemic_filter_t filter);
int epistemic_fep_bridge_disconnect(epistemic_fep_bridge_t* bridge);

int epistemic_fep_compute_epistemic_value(epistemic_fep_bridge_t* bridge);
int epistemic_fep_detect_bias_from_precision(epistemic_fep_bridge_t* bridge);
int epistemic_fep_trigger_information_seeking(epistemic_fep_bridge_t* bridge);

int epistemic_fep_update_evidence_precision(epistemic_fep_bridge_t* bridge, float evidence_quality);
int epistemic_fep_revise_priors_from_bias(epistemic_fep_bridge_t* bridge);
int epistemic_fep_weight_by_source_reliability(epistemic_fep_bridge_t* bridge, float reliability);

int epistemic_fep_bridge_update(epistemic_fep_bridge_t* bridge, uint64_t delta_ms);

int epistemic_fep_bridge_get_state(const epistemic_fep_bridge_t* bridge, epistemic_fep_state_t* state);
int epistemic_fep_bridge_get_stats(const epistemic_fep_bridge_t* bridge, epistemic_fep_stats_t* stats);

int epistemic_fep_bridge_connect_bio_async(epistemic_fep_bridge_t* bridge);
int epistemic_fep_bridge_disconnect_bio_async(epistemic_fep_bridge_t* bridge);
bool epistemic_fep_bridge_is_bio_async_connected(const epistemic_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EPISTEMIC_FEP_BRIDGE_H */
