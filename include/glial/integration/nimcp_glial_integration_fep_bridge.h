/**
 * @file nimcp_glial_integration_fep_bridge.h
 * @brief Free Energy Principle bridge for glial integration system
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between glial integration and Free Energy Principle
 * WHY:  Glial cells collectively maintain prediction capacity across neural networks
 * HOW:  FEP guides coordinated glial support; glial state informs FEP model
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 * - Tripartite synapse: Astrocyte-neuron-neuron communication
 * - Metabolic support: ATP/lactate provision maintains prediction capacity
 * - Network homeostasis: Glial cells stabilize network dynamics
 * - Coordinated response: Multiple glial cell types work together
 * - Precision regulation: Glial support proportional to computational demands
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_GLIAL_INTEGRATION_FEP_BRIDGE_H
#define NIMCP_GLIAL_INTEGRATION_FEP_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "glial/integration/nimcp_glial_integration.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

#define GLIAL_INTEGRATION_FEP_DEFAULT_METABOLIC_GAIN 0.7f
#define GLIAL_INTEGRATION_FEP_DEFAULT_HOMEOSTASIS_GAIN 0.5f

typedef struct {
    float metabolic_gain;
    float homeostasis_gain;
    float network_precision_factor;
    bool enable_metabolic_prediction;
    bool enable_homeostatic_precision;
    bool enable_coordinated_response;
} glial_integration_fep_config_t;

typedef struct {
    float metabolic_demand_prediction;
    float network_stability_requirement;
    float precision_distribution_pattern;
    float coordinated_support_level;
} fep_glial_integration_effects_t;

typedef struct {
    float metabolic_capacity;
    float network_homeostasis_level;
    float overall_precision_support;
    float glial_prediction_capacity;
} glial_integration_fep_effects_t;

typedef struct {
    uint64_t last_update_time;
    float predicted_metabolic_demand;
    float metabolic_prediction_error;
    uint32_t num_active_glial_cells;
} glial_integration_fep_state_t;

typedef struct {
    uint64_t total_updates;
    uint64_t metabolic_predictions;
    uint64_t homeostatic_adjustments;
    float avg_metabolic_capacity;
    float avg_precision_support;
} glial_integration_fep_stats_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    glial_integration_fep_config_t config;
    fep_system_t* fep_system;
    glial_integration_t* glial_integration;
    fep_glial_integration_effects_t fep_effects;
    glial_integration_fep_effects_t glial_effects;
    glial_integration_fep_state_t state;
    glial_integration_fep_stats_t stats;} glial_integration_fep_bridge_t;

int glial_integration_fep_default_config(glial_integration_fep_config_t* config);
glial_integration_fep_bridge_t* glial_integration_fep_create(
    const glial_integration_fep_config_t* config,
    glial_integration_t* glial_integration,
    fep_system_t* fep_system
);
void glial_integration_fep_destroy(glial_integration_fep_bridge_t* bridge);
int glial_integration_fep_update_fep_to_glial(glial_integration_fep_bridge_t* bridge);
int glial_integration_fep_update_glial_to_fep(glial_integration_fep_bridge_t* bridge);
int glial_integration_fep_update(glial_integration_fep_bridge_t* bridge);
int glial_integration_fep_apply_modulation(glial_integration_fep_bridge_t* bridge);
float glial_integration_fep_get_metabolic_prediction(const glial_integration_fep_bridge_t* bridge);
float glial_integration_fep_get_precision_support(const glial_integration_fep_bridge_t* bridge);
int glial_integration_fep_get_stats(const glial_integration_fep_bridge_t* bridge,
                                    glial_integration_fep_stats_t* stats);
int glial_integration_fep_connect_bio_async(glial_integration_fep_bridge_t* bridge);
int glial_integration_fep_disconnect_bio_async(glial_integration_fep_bridge_t* bridge);
bool glial_integration_fep_is_bio_async_connected(const glial_integration_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GLIAL_INTEGRATION_FEP_BRIDGE_H */
