/**
 * @file nimcp_predictive_coding_fep_bridge.h
 * @brief Free Energy Principle - Predictive Coding Integration Bridge
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between Free Energy Principle and predictive coding
 * WHY:  Predictive coding IS an implementation of FEP - this bridge provides explicit coordination.
 *       Essential for hierarchical error minimization and belief propagation.
 * HOW:  FEP provides global free energy objective; predictive coding implements hierarchical inference;
 *       prediction errors from PC update FEP beliefs; FEP precision weights PC prediction errors.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * THEORETICAL UNITY:
 * ------------------
 * Predictive coding and FEP are mathematically equivalent frameworks (Friston, 2005):
 * - PC prediction errors = FEP variational gradients
 * - PC hierarchical updates = FEP belief propagation
 * - PC precision weighting = FEP attention
 * - PC learning = FEP model optimization
 *
 * FEP → PC PATHWAYS:
 * ------------------
 * 1. Global Free Energy Monitors Hierarchy:
 *    - FEP computes total F across all levels
 *    - PC hierarchy minimizes this global objective
 *    - Convergence detection via F stabilization
 *
 * 2. Precision Controls Error Weighting:
 *    - FEP precision estimates → PC error weights
 *    - High precision errors dominate belief updates
 *    - Precision learning via FEP uncertainty estimates
 *
 * 3. Expected Free Energy Guides Learning:
 *    - FEP EFE → PC learning rate modulation
 *    - Active inference constrains weight updates
 *
 * PC → FEP PATHWAYS:
 * ------------------
 * 1. Hierarchical Errors Decompose Free Energy:
 *    - PC errors at each level contribute to total F
 *    - Level-specific complexity and accuracy costs
 *    - Hierarchical free energy decomposition
 *
 * 2. Belief Updates Minimize Free Energy:
 *    - PC μ updates implement variational inference
 *    - Converged beliefs = FEP posterior approximation
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PREDICTIVE_CODING_FEP_BRIDGE_H
#define NIMCP_PREDICTIVE_CODING_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "plasticity/predictive/nimcp_predictive_coding.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define PC_FEP_MAX_LEVELS               8       /**< Max hierarchy levels */
#define PC_FEP_CONVERGENCE_THRESHOLD    0.001f  /**< F convergence threshold */
#define PC_FEP_PRECISION_MIN            0.01f   /**< Min precision */
#define PC_FEP_PRECISION_MAX            10.0f   /**< Max precision */

/* ============================================================================
 * Structures
 * ============================================================================ */

typedef struct predictive_coding_fep_bridge predictive_coding_fep_bridge_t;

typedef struct {
    float precision_sensitivity;
    float error_scaling;
    float learning_rate_modulation;
    bool enable_precision_weighting;
    bool enable_hierarchical_fe;
    bool enable_convergence_detection;
} predictive_coding_fep_config_t;

typedef struct {
    float total_free_energy;
    float* level_free_energies;
    uint32_t num_levels;
    float precision_scaling;
    float error_weight;
} predictive_coding_fep_effects_t;

typedef struct {
    float* prediction_errors;
    uint32_t num_levels;
    float mean_error;
    float hierarchy_precision;
} predictive_coding_fep_feedback_t;

typedef struct {
    uint64_t total_updates;
    uint64_t convergence_events;
    float avg_free_energy;
    float avg_prediction_error;
} predictive_coding_fep_stats_t;

struct predictive_coding_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    predictive_coding_fep_config_t config;
    fep_system_t* fep_system;
    pc_hierarchy_t pc_hierarchy;
    predictive_coding_fep_effects_t fep_effects;
    predictive_coding_fep_feedback_t pc_effects;
    predictive_coding_fep_stats_t stats;
};

/* ============================================================================
 * API
 * ============================================================================ */

int predictive_coding_fep_bridge_default_config(predictive_coding_fep_config_t* config);
predictive_coding_fep_bridge_t* predictive_coding_fep_bridge_create(const predictive_coding_fep_config_t* config);
void predictive_coding_fep_bridge_destroy(predictive_coding_fep_bridge_t* bridge);

int predictive_coding_fep_bridge_connect_fep(predictive_coding_fep_bridge_t* bridge, fep_system_t* fep);
int predictive_coding_fep_bridge_connect_pc(predictive_coding_fep_bridge_t* bridge, pc_hierarchy_t hierarchy);
int predictive_coding_fep_bridge_disconnect(predictive_coding_fep_bridge_t* bridge);

float predictive_coding_fep_apply_precision_weighting(predictive_coding_fep_bridge_t* bridge, uint32_t level, float error);
float predictive_coding_fep_compute_hierarchical_free_energy(const predictive_coding_fep_bridge_t* bridge);
int predictive_coding_fep_report_errors(predictive_coding_fep_bridge_t* bridge, const float* errors, uint32_t num_levels);

int predictive_coding_fep_bridge_update(predictive_coding_fep_bridge_t* bridge, uint64_t delta_ms);
int predictive_coding_fep_bridge_get_stats(const predictive_coding_fep_bridge_t* bridge, predictive_coding_fep_stats_t* stats);

int predictive_coding_fep_bridge_connect_bio_async(predictive_coding_fep_bridge_t* bridge);
int predictive_coding_fep_bridge_disconnect_bio_async(predictive_coding_fep_bridge_t* bridge);
bool predictive_coding_fep_bridge_is_bio_async_connected(const predictive_coding_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREDICTIVE_CODING_FEP_BRIDGE_H */
