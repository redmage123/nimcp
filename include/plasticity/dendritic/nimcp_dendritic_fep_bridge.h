/**
 * @file nimcp_dendritic_fep_bridge.h
 * @brief Free Energy Principle - Dendritic Computation Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between FEP and dendritic computation
 * WHY:  Dendritic predictions enable hierarchical FEP; local prediction errors
 *       drive dendritic plasticity and NMDA dynamics.
 * HOW:  FEP hierarchical predictions map to dendritic compartments; dendritic
 *       prediction errors modulate NMDA conductance and calcium signaling.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * FEP → DENDRITIC PATHWAYS:
 * -------------------------
 * 1. Hierarchical Predictions in Dendrites:
 *    - Apical dendrites = top-down predictions (higher-level FEP)
 *    - Basal dendrites = bottom-up sensory input
 *    - Dendritic integration = hierarchical prediction error
 *    - Reference: Larkum (2013) "Dendritic mechanisms underlying cortical computations"
 *
 * 2. Prediction Error Modulates NMDA:
 *    - High PE → enhanced NMDA conductance
 *    - NMDA voltage dependence implements precision weighting
 *    - Coincidence detection = prediction confirmation
 *
 * 3. Precision Controls Dendritic Gain:
 *    - High precision → high dendritic excitability
 *    - Low precision → reduced dendritic amplification
 *    - Attention modulates dendritic integration
 *
 * DENDRITIC → FEP PATHWAYS:
 * -------------------------
 * 1. Dendritic Spikes as Prediction Errors:
 *    - Dendritic spike = local prediction error signal
 *    - Amplitude encodes PE magnitude
 *    - Calcium influx drives belief updates
 *
 * 2. Local Computation Implements Hierarchical FEP:
 *    - Each compartment = local generative model
 *    - Dendritic nonlinearities = hierarchical processing
 *    - Efficient hierarchical inference
 *
 * 3. Calcium Signals Model Updates:
 *    - NMDA-mediated Ca²⁺ = learning signal
 *    - Calcium concentration ∝ belief update magnitude
 *    - Local plasticity implements local FEP learning
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_DENDRITIC_FEP_BRIDGE_H
#define NIMCP_DENDRITIC_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "plasticity/dendritic/nimcp_dendritic.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DENDRITIC_FEP_PE_NMDA_SCALING       1.0f
#define DENDRITIC_FEP_PRECISION_GAIN_MIN    0.5f
#define DENDRITIC_FEP_PRECISION_GAIN_MAX    2.0f
#define DENDRITIC_FEP_CALCIUM_PE_FACTOR     1.0f

typedef struct dendritic_fep_bridge dendritic_fep_bridge_t;

typedef struct {
    float pe_nmda_gain;
    float precision_excitability_gain;
    float calcium_belief_sensitivity;
    bool enable_pe_nmda_modulation;
    bool enable_precision_gain_control;
    bool enable_calcium_belief_updates;
    bool enable_hierarchical_predictions;
} dendritic_fep_config_t;

typedef struct {
    float pe_magnitude;
    float pe_nmda_scaling;
    float precision_value;
    float precision_gain_modulation;
    float calcium_concentration;
    float calcium_belief_update;
    float total_nmda_modulation;
    float total_gain_modulation;
} dendritic_fep_effects_t;

typedef struct {
    float current_pe;
    float current_precision;
    float current_calcium;
    float nmda_modulation;
    float gain_modulation;
    uint32_t dendritic_spikes;
    bool hierarchical_prediction_active;
    uint64_t last_update_time;
} dendritic_fep_state_t;

typedef struct {
    uint64_t total_updates;
    uint64_t pe_nmda_events;
    uint64_t dendritic_spike_events;
    float avg_pe;
    float avg_precision;
    float avg_calcium;
    float avg_nmda_modulation;
} dendritic_fep_stats_t;

struct dendritic_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    dendritic_fep_config_t config;
    fep_system_t* fep_system;
    dendritic_tree_t dendritic_system;
    dendritic_fep_effects_t effects;
    dendritic_fep_state_t state;
    dendritic_fep_stats_t stats;
};

int dendritic_fep_bridge_default_config(dendritic_fep_config_t* config);
dendritic_fep_bridge_t* dendritic_fep_bridge_create(const dendritic_fep_config_t* config);
void dendritic_fep_bridge_destroy(dendritic_fep_bridge_t* bridge);

int dendritic_fep_bridge_connect_fep(dendritic_fep_bridge_t* bridge, fep_system_t* fep);
int dendritic_fep_bridge_connect_dendritic(dendritic_fep_bridge_t* bridge, dendritic_tree_t dendritic);
int dendritic_fep_bridge_disconnect(dendritic_fep_bridge_t* bridge);

float dendritic_fep_apply_pe_nmda_modulation(dendritic_fep_bridge_t* bridge, float pe);
float dendritic_fep_apply_precision_gain_control(dendritic_fep_bridge_t* bridge, float precision);
float dendritic_fep_compute_calcium_belief_update(const dendritic_fep_bridge_t* bridge);
float dendritic_fep_get_effective_nmda_conductance(const dendritic_fep_bridge_t* bridge, float base_conductance);

int dendritic_fep_report_dendritic_spike(dendritic_fep_bridge_t* bridge, float spike_amplitude);
int dendritic_fep_bridge_update(dendritic_fep_bridge_t* bridge, uint64_t delta_ms);

int dendritic_fep_bridge_get_state(const dendritic_fep_bridge_t* bridge, dendritic_fep_state_t* state);
int dendritic_fep_bridge_get_stats(const dendritic_fep_bridge_t* bridge, dendritic_fep_stats_t* stats);

int dendritic_fep_bridge_connect_bio_async(dendritic_fep_bridge_t* bridge);
int dendritic_fep_bridge_disconnect_bio_async(dendritic_fep_bridge_t* bridge);
bool dendritic_fep_bridge_is_bio_async_connected(const dendritic_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DENDRITIC_FEP_BRIDGE_H */
