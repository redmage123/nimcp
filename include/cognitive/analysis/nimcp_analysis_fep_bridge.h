/**
 * @file nimcp_analysis_fep_bridge.h
 * @brief Free Energy Principle - Network Analysis Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and network analysis
 * WHY:  Network topology decomposition minimizes complexity in generative models;
 *       FEP prediction errors guide community detection and structure discovery.
 * HOW:  FEP uncertainty drives topological exploration; network structure constrains
 *       generative model architecture; community changes trigger belief updates.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * NETWORK ANALYSIS AS FREE ENERGY MINIMIZATION:
 * ---------------------------------------------
 * - Network modules minimize free energy by reducing wiring cost (complexity)
 *   while maximizing functional efficiency (accuracy)
 * - Community detection = finding optimal partition that minimizes F
 * - Reference: Bullmore & Sporns (2012) "The economy of brain network organization"
 *
 * FEP → NETWORK ANALYSIS PATHWAYS:
 * -------------------------------
 * 1. High Prediction Error → Topological Exploration:
 *    - High PE suggests current network model is wrong
 *    - Trigger community re-detection
 *    - Search for new modular structure
 *
 * 2. Precision Weights Analysis:
 *    - High precision regions = important hubs
 *    - Low precision = ignore in topology
 *    - Precision-weighted centrality
 *
 * 3. Surprise-Driven Reorganization:
 *    - Novel patterns trigger network plasticity
 *    - FEP surprise → structural reorganization
 *
 * NETWORK ANALYSIS → FEP PATHWAYS:
 * --------------------------------
 * 1. Network Structure Constrains Generative Model:
 *    - Community structure → Hierarchical FEP levels
 *    - Hub neurons → High precision nodes
 *    - Modularity Q → Prior complexity penalty
 *
 * 2. Topology Metrics Inform Beliefs:
 *    - Small-worldness → Efficient inference
 *    - Clustering → Local prediction accuracy
 *    - Path length → Inference speed
 *
 * 3. Community Changes Update Beliefs:
 *    - New community = new hidden state
 *    - Community merge = state space reduction
 *    - Community split = increased complexity
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ANALYSIS_FEP_BRIDGE_H
#define NIMCP_ANALYSIS_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/analysis/nimcp_network_analysis.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ANALYSIS_FEP_HIGH_PE_THRESHOLD       5.0f
#define ANALYSIS_FEP_MODULARITY_PRIOR        0.3f
#define ANALYSIS_FEP_HUB_PRECISION_BOOST     2.0f

typedef struct analysis_fep_bridge analysis_fep_bridge_t;

typedef struct {
    float pe_exploration_threshold;
    float precision_hub_weight;
    float modularity_prior_strength;
    bool enable_pe_exploration;
    bool enable_precision_weighting;
    bool enable_topology_priors;
    float community_change_sensitivity;
    float topology_belief_strength;
    bool enable_community_beliefs;
    bool enable_topology_updates;
    float fe_sensitivity;
    float analysis_sensitivity;
} analysis_fep_config_t;

typedef struct {
    float current_prediction_error;
    bool exploration_triggered;
    float hub_precision_weights[64];
    uint32_t num_hubs_weighted;
    float current_surprise;
    bool topology_exploration_active;
} analysis_fep_effects_t;

typedef struct {
    float modularity_prior_bias;
    bool topology_constraining_model;
    uint32_t num_communities_detected;
    float community_hierarchy_depth;
    float small_worldness_factor;
    bool model_structure_updated;
} fep_analysis_effects_t;

typedef struct {
    float current_prediction_error;
    float current_modularity;
    float current_small_worldness;
    bool exploration_active;
    uint32_t num_communities;
    uint64_t last_exploration_time;
    uint64_t last_topology_update_time;
} analysis_fep_state_t;

typedef struct {
    uint64_t exploration_events;
    uint64_t topology_updates;
    uint64_t community_detections;
    float avg_prediction_error;
    float avg_modularity;
    uint64_t hub_weighting_applications;
    uint64_t topology_constraint_updates;
    float avg_free_energy;
} analysis_fep_stats_t;

struct analysis_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    analysis_fep_config_t config;
    fep_system_t* fep_system;
    network_analyzer_t* analyzer;
    analysis_fep_effects_t fep_effects;
    fep_analysis_effects_t analysis_effects;
    analysis_fep_state_t state;
    analysis_fep_stats_t stats;
};

int analysis_fep_bridge_default_config(analysis_fep_config_t* config);
analysis_fep_bridge_t* analysis_fep_bridge_create(const analysis_fep_config_t* config);
void analysis_fep_bridge_destroy(analysis_fep_bridge_t* bridge);

int analysis_fep_bridge_connect_fep(analysis_fep_bridge_t* bridge, fep_system_t* fep);
int analysis_fep_bridge_connect_analysis(analysis_fep_bridge_t* bridge, network_analyzer_t* analyzer);
int analysis_fep_bridge_disconnect(analysis_fep_bridge_t* bridge);

int analysis_fep_trigger_exploration(analysis_fep_bridge_t* bridge, float pe_magnitude);
int analysis_fep_weight_hubs_by_precision(analysis_fep_bridge_t* bridge);
int analysis_fep_trigger_reorganization(analysis_fep_bridge_t* bridge);

int analysis_fep_apply_topology_priors(analysis_fep_bridge_t* bridge);
int analysis_fep_apply_community_beliefs(analysis_fep_bridge_t* bridge);
int analysis_fep_update_model_structure(analysis_fep_bridge_t* bridge);

int analysis_fep_bridge_update(analysis_fep_bridge_t* bridge, uint64_t delta_ms);

int analysis_fep_bridge_get_state(analysis_fep_bridge_t* bridge, analysis_fep_state_t* state);
int analysis_fep_bridge_get_stats(analysis_fep_bridge_t* bridge, analysis_fep_stats_t* stats);

int analysis_fep_bridge_connect_bio_async(analysis_fep_bridge_t* bridge);
int analysis_fep_bridge_disconnect_bio_async(analysis_fep_bridge_t* bridge);
bool analysis_fep_bridge_is_bio_async_connected(const analysis_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ANALYSIS_FEP_BRIDGE_H */
