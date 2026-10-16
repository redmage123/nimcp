/**
 * @file nimcp_hierarchical_fep_bridge.h
 * @brief Free Energy Principle - Hierarchical Brain Integration Bridge
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between Free Energy Principle and hierarchical brain regions
 * WHY:  FEP is inherently hierarchical - higher levels predict lower levels, prediction errors
 *       propagate up the hierarchy.
 * HOW:  Each brain region has its own FEP level; hierarchical predictive coding coordinates
 *       multi-level free energy minimization.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * HIERARCHICAL PREDICTIVE PROCESSING:
 * ------------------------------------
 * - Friston (2008): Brain hierarchy implements hierarchical FEP
 * - Higher cortical areas = abstract predictions
 * - Lower cortical areas = concrete predictions
 * - Prediction errors flow bottom-up, predictions flow top-down
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HIERARCHICAL_FEP_BRIDGE_H
#define NIMCP_HIERARCHICAL_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_hierarchical.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hierarchical_fep_bridge hierarchical_fep_bridge_t;

typedef struct {
    bool enable_hierarchical_prediction;
    bool enable_pe_propagation;
    bool enable_layer_specific_lr;
    float hierarchy_sensitivity;
    float fep_sensitivity;
} hierarchical_fep_config_t;

typedef struct {
    uint32_t num_levels;
    float* level_free_energies;
    float* level_prediction_errors;
    float total_hierarchical_fe;
} hierarchical_fep_effects_t;

typedef struct {
    float* level_lr_modifiers;
    float* level_precisions;
    bool prediction_active;
} fep_hierarchical_effects_t;

typedef struct {
    float current_free_energy;
    uint32_t num_levels;
    float* level_states;
} hierarchical_fep_state_t;

typedef struct {
    uint64_t prediction_events;
    float avg_free_energy;
    float avg_hierarchical_consistency;
} hierarchical_fep_stats_t;

struct hierarchical_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    hierarchical_fep_config_t config;
    fep_system_t* fep_system;
    hierarchical_brain_t hierarchical_brain;
    hierarchical_fep_effects_t fep_effects;
    fep_hierarchical_effects_t hierarchical_effects;
    hierarchical_fep_state_t state;
    hierarchical_fep_stats_t stats;
};

int hierarchical_fep_bridge_default_config(hierarchical_fep_config_t* config);
hierarchical_fep_bridge_t* hierarchical_fep_bridge_create(const hierarchical_fep_config_t* config);
void hierarchical_fep_bridge_destroy(hierarchical_fep_bridge_t* bridge);
int hierarchical_fep_bridge_connect_fep(hierarchical_fep_bridge_t* bridge, fep_system_t* fep);
int hierarchical_fep_bridge_connect_hierarchical(hierarchical_fep_bridge_t* bridge,
                                                  hierarchical_brain_t hbrain);
int hierarchical_fep_bridge_disconnect(hierarchical_fep_bridge_t* bridge);
int hierarchical_fep_bridge_update(hierarchical_fep_bridge_t* bridge);
int hierarchical_fep_bridge_get_state(hierarchical_fep_bridge_t* bridge,
                                       hierarchical_fep_state_t* state);
int hierarchical_fep_bridge_get_stats(hierarchical_fep_bridge_t* bridge,
                                       hierarchical_fep_stats_t* stats);
int hierarchical_fep_bridge_connect_bio_async(hierarchical_fep_bridge_t* bridge);
int hierarchical_fep_bridge_disconnect_bio_async(hierarchical_fep_bridge_t* bridge);
bool hierarchical_fep_bridge_is_bio_async_connected(const hierarchical_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HIERARCHICAL_FEP_BRIDGE_H */
