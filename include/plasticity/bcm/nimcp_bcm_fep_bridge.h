/**
 * @file nimcp_bcm_fep_bridge.h
 * @brief Free Energy Principle - BCM Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and BCM plasticity
 * WHY:  FEP complexity regularization controls BCM threshold; BCM sliding threshold
 *       implements FEP model selection via plasticity gating.
 * HOW:  FEP complexity cost modulates BCM threshold; BCM weight distribution
 *       minimizes free energy through optimal feature selectivity.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * FEP → BCM PATHWAYS:
 * -------------------
 * 1. Complexity Regularization via BCM Threshold:
 *    - High FEP complexity → increase BCM threshold θ
 *    - Low complexity → decrease threshold (more plastic)
 *    - Implements Occam's razor (simpler models preferred)
 *    - Reference: Friston (2010) "Free-energy principle: model selection"
 *
 * 2. Precision Controls BCM Selectivity:
 *    - High precision → sharper BCM tuning (winner-take-all)
 *    - Low precision → broader BCM tuning (distributed)
 *    - Attention modulates cortical selectivity
 *
 * 3. Prediction Error Gates BCM Learning:
 *    - High PE → active BCM plasticity
 *    - Low PE → minimal BCM changes (converged)
 *    - Learning driven by surprise
 *
 * BCM → FEP PATHWAYS:
 * -------------------
 * 1. BCM Threshold as Model Complexity Indicator:
 *    - Sliding threshold θ reflects model capacity
 *    - High θ → complex model (many features)
 *    - Low θ → simple model (few features)
 *
 * 2. Weight Distribution Minimizes Free Energy:
 *    - BCM naturally creates sparse, selective representations
 *    - Selectivity reduces complexity cost
 *    - Efficient coding = minimal free energy
 *
 * INTEGRATION MECHANISMS:
 * -----------------------
 * - Complexity → threshold: θ_effective = θ_base × (1 + complexity)
 * - Precision → selectivity: BCM tuning width ∝ 1/precision
 * - PE → plasticity: BCM learning rate ∝ |PE|
 * - Sparse coding: BCM sparsity minimizes FEP complexity term
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BCM_FEP_BRIDGE_H
#define NIMCP_BCM_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "plasticity/bcm/nimcp_bcm.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Complexity-threshold scaling */
#define BCM_FEP_COMPLEXITY_MIN            0.0f     /**< Min complexity */
#define BCM_FEP_COMPLEXITY_MAX           10.0f     /**< Max complexity */
#define BCM_FEP_THRESHOLD_SCALING         1.5f     /**< Complexity → threshold */

/* Precision-selectivity scaling */
#define BCM_FEP_PRECISION_MIN             0.1f     /**< Min precision */
#define BCM_FEP_PRECISION_MAX             2.0f     /**< Max precision */
#define BCM_FEP_SELECTIVITY_GAIN          1.0f     /**< Precision → selectivity */

/* Learning rate modulation */
#define BCM_FEP_LR_MIN_FACTOR             0.1f     /**< Min LR scaling */
#define BCM_FEP_LR_MAX_FACTOR             3.0f     /**< Max LR scaling */

/* ============================================================================
 * Structures
 * ============================================================================ */

typedef struct bcm_fep_bridge bcm_fep_bridge_t;

/**
 * @brief Configuration for BCM-FEP bridge
 */
typedef struct {
    float complexity_threshold_gain;     /**< Complexity → threshold scaling */
    float precision_selectivity_gain;    /**< Precision → selectivity scaling */
    float pe_lr_sensitivity;             /**< PE → learning rate scaling */

    bool enable_complexity_regularization;
    bool enable_precision_modulation;
    bool enable_pe_gating;
    bool enable_sparsity_tracking;

    float lr_min_factor;
    float lr_max_factor;
} bcm_fep_config_t;

/**
 * @brief FEP effects on BCM
 */
typedef struct {
    float complexity_value;              /**< Current FEP complexity */
    float complexity_threshold_scaling;  /**< Complexity → threshold */
    float precision_value;               /**< Current precision */
    float precision_selectivity_scaling; /**< Precision → selectivity */
    float pe_magnitude;                  /**< Prediction error */
    float pe_lr_scaling;                 /**< PE → learning rate */
    float total_threshold_modulation;    /**< Combined threshold effect */
    float total_lr_modulation;           /**< Combined LR effect */
} bcm_fep_effects_t;

/**
 * @brief Current state of BCM-FEP interaction
 */
typedef struct {
    float current_complexity;
    float current_precision;
    float current_pe;
    float threshold_modulation;
    float lr_modulation;
    uint32_t sparsity_level;             /**< Active synapses */
    bool converged;
    uint64_t last_update_time;
} bcm_fep_state_t;

/**
 * @brief Statistics for BCM-FEP bridge
 */
typedef struct {
    uint64_t total_updates;
    uint64_t complexity_adjustments;
    uint64_t precision_modulations;
    float avg_complexity;
    float avg_precision;
    float avg_threshold_modulation;
    float avg_sparsity;
    float avg_free_energy;
} bcm_fep_stats_t;

/**
 * @brief BCM-FEP bridge state
 */
struct bcm_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    bcm_fep_config_t config;
    fep_system_t* fep_system;
    bcm_synapse_t* bcm_system;
    uint32_t num_synapses;
    bcm_fep_effects_t effects;
    bcm_fep_state_t state;
    bcm_fep_stats_t stats;
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int bcm_fep_bridge_default_config(bcm_fep_config_t* config);
bcm_fep_bridge_t* bcm_fep_bridge_create(const bcm_fep_config_t* config);
void bcm_fep_bridge_destroy(bcm_fep_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

int bcm_fep_bridge_connect_fep(bcm_fep_bridge_t* bridge, fep_system_t* fep);
int bcm_fep_bridge_connect_bcm(bcm_fep_bridge_t* bridge, bcm_synapse_t* bcm, uint32_t num);
int bcm_fep_bridge_disconnect(bcm_fep_bridge_t* bridge);

/* ============================================================================
 * FEP → BCM Direction
 * ============================================================================ */

float bcm_fep_apply_complexity_regularization(bcm_fep_bridge_t* bridge, float complexity);
float bcm_fep_apply_precision_modulation(bcm_fep_bridge_t* bridge, float precision);
float bcm_fep_apply_pe_gating(bcm_fep_bridge_t* bridge, float pe);
float bcm_fep_get_effective_threshold(const bcm_fep_bridge_t* bridge, float base_threshold);
float bcm_fep_get_effective_lr(const bcm_fep_bridge_t* bridge, float base_lr);

/* ============================================================================
 * BCM → FEP Direction
 * ============================================================================ */

int bcm_fep_report_threshold_changes(bcm_fep_bridge_t* bridge, float threshold_delta);
float bcm_fep_compute_sparsity(const bcm_fep_bridge_t* bridge);
int bcm_fep_report_sparsity(bcm_fep_bridge_t* bridge, uint32_t active_count);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

int bcm_fep_bridge_update(bcm_fep_bridge_t* bridge, uint64_t delta_ms);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

int bcm_fep_bridge_get_state(const bcm_fep_bridge_t* bridge, bcm_fep_state_t* state);
int bcm_fep_bridge_get_stats(const bcm_fep_bridge_t* bridge, bcm_fep_stats_t* stats);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int bcm_fep_bridge_connect_bio_async(bcm_fep_bridge_t* bridge);
int bcm_fep_bridge_disconnect_bio_async(bcm_fep_bridge_t* bridge);
bool bcm_fep_bridge_is_bio_async_connected(const bcm_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BCM_FEP_BRIDGE_H */
