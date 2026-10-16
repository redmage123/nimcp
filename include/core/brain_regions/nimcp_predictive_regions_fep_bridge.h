/**
 * @file nimcp_predictive_regions_fep_bridge.h
 * @brief Free Energy Principle - Predictive Regions Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and Predictive Regions
 * WHY:  Predictive regions are the native implementation of FEP hierarchy; direct
 *       integration enables hierarchical belief propagation and precision-weighted error
 *       correction across cortical levels.
 * HOW:  Map FEP hierarchy levels to predictive region hierarchy; synchronize predictions,
 *       errors, and precision weights; enable active inference for region dynamics.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * PREDICTIVE REGIONS AS FEP IMPLEMENTATION:
 * -----------------------------------------
 * 1. Cortical Hierarchy Levels:
 *    - V1 (primary visual) = FEP Level 0 (sensory observations)
 *    - V2/V4 (intermediate) = FEP Level 1-2 (feature beliefs)
 *    - IT (inferotemporal) = FEP Level 3+ (abstract beliefs)
 *    - Reference: Bastos et al. (2012) "Canonical microcircuits for predictive coding"
 *
 * 2. Precision Weighting in Cortex:
 *    - Precision = synaptic gain modulation
 *    - High precision → stronger error propagation
 *    - Attention modulates precision via neuromodulators
 *    - Reference: Feldman & Friston (2010) "Attention, uncertainty, and free-energy"
 *
 * 3. Hierarchical Message Passing:
 *    - Superficial layers (2/3): Encode prediction errors (bottom-up)
 *    - Deep layers (5/6): Encode predictions (top-down)
 *    - Layer 4: Receives top-down predictions from higher regions
 *    - Reference: Shipp (2016) "Neural elements for predictive coding"
 *
 * 4. Active Inference in Predictive Regions:
 *    - Regions actively sample inputs to minimize prediction error
 *    - Saccades, attention shifts = active inference policies
 *    - Expected free energy guides region-level action selection
 *    - Reference: Parr & Friston (2017) "Working memory, attention, and salience"
 *
 * FEP → PREDICTIVE REGIONS PATHWAYS:
 * ----------------------------------
 * 1. Belief Synchronization:
 *    - FEP beliefs (μ) → Region representations
 *    - Hierarchical belief propagation
 *    - Precision-weighted updates
 *
 * 2. Prediction Flow:
 *    - FEP top-down predictions → Region predictions
 *    - Higher regions predict lower region activity
 *    - Predictions modulate lower region gain
 *
 * PREDICTIVE REGIONS → FEP PATHWAYS:
 * ----------------------------------
 * 1. Prediction Error Feedback:
 *    - Region prediction errors → FEP error signals
 *    - Bottom-up error propagation
 *    - Drives FEP belief updates
 *
 * 2. Precision Modulation:
 *    - Region precision weights → FEP precision
 *    - Attention-based precision adaptation
 *    - Context-dependent gain control
 *
 * 3. Free Energy Computation:
 *    - Region errors contribute to total free energy
 *    - Hierarchical free energy minimization
 *    - Convergence monitoring
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                FEP-PREDICTIVE REGIONS BRIDGE                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  FEP → PREDICTIVE REGIONS                           │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │  FEP HIERARCHY   │                                             │  ║
 * ║   │   │ ──────────────── │                                             │  ║
 * ║   │   │ Level 0: μ₀      │ ───→ Sensory Region Beliefs                 │  ║
 * ║   │   │ Level 1: μ₁      │ ───→ Feature Region Beliefs                 │  ║
 * ║   │   │ Level 2: μ₂      │ ───→ Abstract Region Beliefs                │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │  PRECISION Π     │ ───→ Region Precision Weights               │  ║
 * ║   │   │  PREDICTIONS μ̂   │ ───→ Top-Down Predictions                   │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                PREDICTIVE REGIONS → FEP                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ REGION HIERARCHY │                                             │  ║
 * ║   │   │ ──────────────── │                                             │  ║
 * ║   │   │ V1: ε₀           │ ───→ FEP Level 0 Prediction Errors          │  ║
 * ║   │   │ V2: ε₁           │ ───→ FEP Level 1 Prediction Errors          │  ║
 * ║   │   │ V4: ε₂           │ ───→ FEP Level 2 Prediction Errors          │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ FREE ENERGY F    │ ───→ FEP Free Energy Accumulation           │  ║
 * ║   │   │ CONVERGENCE      │ ───→ Belief Update Termination              │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
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

#ifndef NIMCP_PREDICTIVE_REGIONS_FEP_BRIDGE_H
#define NIMCP_PREDICTIVE_REGIONS_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "core/brain_regions/nimcp_brain_region_predictive.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Hierarchy mapping limits */
#define PREDICTIVE_FEP_MAX_REGIONS     16    /**< Max regions in hierarchy */
#define PREDICTIVE_FEP_MAX_LEVELS      8     /**< Max FEP hierarchy levels */

/* Precision adaptation rates */
#define PREDICTIVE_FEP_PRECISION_ADAPTATION_RATE  0.05f  /**< Precision learning rate */
#define PREDICTIVE_FEP_MIN_PRECISION              0.01f  /**< Minimum precision */
#define PREDICTIVE_FEP_MAX_PRECISION              10.0f  /**< Maximum precision */

/* Convergence thresholds */
#define PREDICTIVE_FEP_CONVERGENCE_THRESHOLD      0.001f /**< Belief convergence threshold */
#define PREDICTIVE_FEP_ERROR_THRESHOLD            0.1f   /**< Low error threshold */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct predictive_regions_fep_bridge predictive_regions_fep_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for Predictive Regions-FEP bridge
 */
typedef struct {
    /* Hierarchy mapping */
    uint32_t num_hierarchy_levels;         /**< Number of hierarchy levels */
    bool enable_precision_adaptation;      /**< Adapt precision from errors */
    bool enable_belief_sync;               /**< Sync FEP beliefs to regions */
    bool enable_error_propagation;         /**< Propagate region errors to FEP */

    /* Learning rates */
    float precision_learning_rate;         /**< Precision adaptation rate */
    float belief_learning_rate;            /**< Belief update rate */
    float prediction_learning_rate;        /**< Prediction weight learning rate */

    /* Thresholds */
    float convergence_threshold;           /**< Convergence threshold */
    float error_threshold;                 /**< Low error threshold */

    /* Active inference */
    bool enable_active_inference;          /**< Enable active region selection */
    float exploration_rate;                /**< Exploration vs exploitation */
} predictive_regions_fep_config_t;

/**
 * @brief Hierarchy level mapping between FEP and regions
 */
typedef struct {
    uint32_t fep_level;                    /**< FEP hierarchy level */
    brain_region_t* region;                /**< Mapped brain region */
    uint32_t region_id;                    /**< Region identifier */
    float* belief_buffer;                  /**< Cached beliefs [region neurons] */
    float* prediction_buffer;              /**< Cached predictions [region neurons] */
    float* error_buffer;                   /**< Cached errors [region neurons] */
    uint32_t buffer_size;                  /**< Size of buffers */
} predictive_fep_level_mapping_t;

/**
 * @brief Effects of FEP on predictive regions
 */
typedef struct {
    /* Precision effects */
    float precision_modulation;            /**< FEP precision → region gain */
    float attention_weight;                /**< Attention-based precision */

    /* Prediction effects */
    float prediction_strength;             /**< Top-down prediction strength */
    float error_correction_rate;           /**< Error-driven correction rate */

    /* Active inference */
    float expected_free_energy;            /**< Expected free energy */
    uint32_t selected_region;              /**< Active inference region selection */
} predictive_regions_fep_effects_t;

/**
 * @brief Current state of Predictive Regions-FEP interaction
 */
typedef struct {
    /* Hierarchy state */
    uint32_t num_mapped_levels;            /**< Number of mapped levels */
    float total_free_energy;               /**< Total hierarchical free energy */
    float mean_prediction_error;           /**< Mean prediction error */

    /* Convergence state */
    bool converged;                        /**< Has hierarchy converged? */
    uint32_t convergence_iterations;       /**< Iterations to convergence */
    float convergence_delta;               /**< Last free energy change */

    /* Active inference state */
    uint32_t active_region_index;          /**< Currently active region */
    float exploration_probability;         /**< Current exploration rate */
} predictive_regions_fep_state_t;

/**
 * @brief Statistics for Predictive Regions-FEP bridge
 */
typedef struct {
    /* Synchronization events */
    uint64_t belief_syncs;                 /**< Times beliefs synced FEP → regions */
    uint64_t error_propagations;           /**< Times errors sent regions → FEP */
    uint64_t precision_updates;            /**< Precision adaptation events */

    /* Performance metrics */
    float avg_free_energy;                 /**< Average free energy */
    float avg_prediction_error;            /**< Average prediction error */
    float avg_convergence_iterations;      /**< Average iterations to convergence */

    /* Active inference */
    uint64_t active_inference_actions;     /**< Active inference selections */
    float avg_expected_free_energy;        /**< Average EFE */
} predictive_regions_fep_stats_t;

/**
 * @brief Predictive Regions-FEP bridge state
 */
struct predictive_regions_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    predictive_regions_fep_config_t config;

    /* Connected systems */
    fep_system_t* fep_system;              /**< FEP system */
    predictive_fep_level_mapping_t* level_mappings; /**< Hierarchy level mappings */
    uint32_t num_mappings;                 /**< Number of level mappings */

    /* Current effects */
    predictive_regions_fep_effects_t effects;
    predictive_regions_fep_state_t state;

    /* Statistics */
    predictive_regions_fep_stats_t stats;

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default Predictive Regions-FEP configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard hierarchical parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int predictive_regions_fep_bridge_default_config(
    predictive_regions_fep_config_t* config
);

/**
 * @brief Create Predictive Regions-FEP bridge
 *
 * WHAT: Initialize Predictive Regions-FEP integration bridge
 * WHY:  Enable bidirectional FEP-regions interaction
 * HOW:  Allocate bridge, initialize hierarchy mappings
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
predictive_regions_fep_bridge_t* predictive_regions_fep_bridge_create(
    const predictive_regions_fep_config_t* config
);

/**
 * @brief Destroy Predictive Regions-FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free level mappings, buffers, mutex
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void predictive_regions_fep_bridge_destroy(
    predictive_regions_fep_bridge_t* bridge
);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect FEP system
 *
 * WHAT: Link bridge to FEP system
 * WHY:  Enable FEP hierarchy access
 * HOW:  Store FEP system pointer
 *
 * @param bridge Predictive Regions-FEP bridge
 * @param fep FEP system
 * @return 0 on success
 */
int predictive_regions_fep_bridge_connect_fep(
    predictive_regions_fep_bridge_t* bridge,
    fep_system_t* fep
);

/**
 * @brief Map brain region to FEP hierarchy level
 *
 * WHAT: Establish correspondence between region and FEP level
 * WHY:  Enable hierarchical prediction flow
 * HOW:  Create level mapping with buffers
 *
 * @param bridge Predictive Regions-FEP bridge
 * @param region Brain region
 * @param fep_level FEP hierarchy level (0 = sensory)
 * @return 0 on success
 */
int predictive_regions_fep_bridge_map_region(
    predictive_regions_fep_bridge_t* bridge,
    brain_region_t* region,
    uint32_t fep_level
);

/**
 * @brief Disconnect all systems
 *
 * WHAT: Unlink FEP and regions
 * WHY:  Safe shutdown
 * HOW:  Clear system pointers, free mappings
 *
 * @param bridge Predictive Regions-FEP bridge
 * @return 0 on success
 */
int predictive_regions_fep_bridge_disconnect(
    predictive_regions_fep_bridge_t* bridge
);

/* ============================================================================
 * FEP → Predictive Regions Direction
 * ============================================================================ */

/**
 * @brief Synchronize FEP beliefs to regions
 *
 * WHAT: Transfer FEP beliefs (μ) to region representations
 * WHY:  Ensure regions reflect FEP posterior beliefs
 * HOW:  Copy FEP level beliefs to mapped region buffers
 *
 * @param bridge Predictive Regions-FEP bridge
 * @return 0 on success
 */
int predictive_regions_fep_sync_beliefs_to_regions(
    predictive_regions_fep_bridge_t* bridge
);

/**
 * @brief Apply FEP precision to region gain
 *
 * WHAT: Modulate region precision weights from FEP precision
 * WHY:  Attention-based gain control via precision
 * HOW:  Scale region precision by FEP precision values
 *
 * @param bridge Predictive Regions-FEP bridge
 * @return 0 on success
 */
int predictive_regions_fep_apply_precision_modulation(
    predictive_regions_fep_bridge_t* bridge
);

/**
 * @brief Generate top-down predictions from FEP
 *
 * WHAT: Compute predictions from higher FEP levels to lower regions
 * WHY:  Hierarchical predictive coding
 * HOW:  Use FEP generative model to predict lower level activity
 *
 * @param bridge Predictive Regions-FEP bridge
 * @return 0 on success
 */
int predictive_regions_fep_generate_predictions(
    predictive_regions_fep_bridge_t* bridge
);

/* ============================================================================
 * Predictive Regions → FEP Direction
 * ============================================================================ */

/**
 * @brief Propagate region prediction errors to FEP
 *
 * WHAT: Transfer region prediction errors to FEP hierarchy
 * WHY:  Drive FEP belief updates from bottom-up errors
 * HOW:  Copy region errors to FEP error signals
 *
 * @param bridge Predictive Regions-FEP bridge
 * @return 0 on success
 */
int predictive_regions_fep_propagate_errors_to_fep(
    predictive_regions_fep_bridge_t* bridge
);

/**
 * @brief Compute hierarchical free energy
 *
 * WHAT: Calculate total free energy across hierarchy
 * WHY:  Monitor overall prediction quality
 * HOW:  Sum precision-weighted squared errors across levels
 *
 * @param bridge Predictive Regions-FEP bridge
 * @param free_energy Output free energy
 * @return 0 on success
 */
int predictive_regions_fep_compute_free_energy(
    predictive_regions_fep_bridge_t* bridge,
    float* free_energy
);

/**
 * @brief Adapt FEP precision from region errors
 *
 * WHAT: Learn FEP precision based on region error statistics
 * WHY:  Automatic attention allocation
 * HOW:  Precision ∝ 1/error_variance
 *
 * @param bridge Predictive Regions-FEP bridge
 * @return 0 on success
 */
int predictive_regions_fep_adapt_precision(
    predictive_regions_fep_bridge_t* bridge
);

/* ============================================================================
 * Active Inference
 * ============================================================================ */

/**
 * @brief Select region via active inference
 *
 * WHAT: Choose which region to sample based on expected free energy
 * WHY:  Active sampling minimizes expected uncertainty
 * HOW:  Compute EFE per region, select via softmax
 *
 * @param bridge Predictive Regions-FEP bridge
 * @param selected_region Output selected region index
 * @return 0 on success
 */
int predictive_regions_fep_active_inference_select(
    predictive_regions_fep_bridge_t* bridge,
    uint32_t* selected_region
);

/**
 * @brief Compute expected free energy for region
 *
 * WHAT: Calculate EFE for sampling a specific region
 * WHY:  EFE quantifies value of information
 * HOW:  EFE = Risk + Ambiguity
 *
 * @param bridge Predictive Regions-FEP bridge
 * @param region_index Region index
 * @param efe Output expected free energy
 * @return 0 on success
 */
int predictive_regions_fep_compute_efe(
    predictive_regions_fep_bridge_t* bridge,
    uint32_t region_index,
    float* efe
);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update Predictive Regions-FEP bridge state
 *
 * WHAT: Main update loop for bidirectional integration
 * WHY:  Keep FEP and regions synchronized
 * HOW:  Sync beliefs, propagate errors, update precision
 *
 * @param bridge Predictive Regions-FEP bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int predictive_regions_fep_bridge_update(
    predictive_regions_fep_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge Predictive Regions-FEP bridge
 * @param state Output state
 * @return 0 on success
 */
int predictive_regions_fep_bridge_get_state(
    const predictive_regions_fep_bridge_t* bridge,
    predictive_regions_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Predictive Regions-FEP bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int predictive_regions_fep_bridge_get_stats(
    const predictive_regions_fep_bridge_t* bridge,
    predictive_regions_fep_stats_t* stats
);

/**
 * @brief Check if hierarchy has converged
 *
 * @param bridge Predictive Regions-FEP bridge
 * @return true if converged
 */
bool predictive_regions_fep_is_converged(
    const predictive_regions_fep_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for FEP-regions coordination
 * WHY:  Distributed hierarchical signaling
 * HOW:  Register module, set up handlers
 *
 * @param bridge Predictive Regions-FEP bridge
 * @return 0 on success
 */
int predictive_regions_fep_bridge_connect_bio_async(
    predictive_regions_fep_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Predictive Regions-FEP bridge
 * @return 0 on success
 */
int predictive_regions_fep_bridge_disconnect_bio_async(
    predictive_regions_fep_bridge_t* bridge
);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Predictive Regions-FEP bridge
 * @return true if bio-async enabled
 */
bool predictive_regions_fep_bridge_is_bio_async_connected(
    const predictive_regions_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREDICTIVE_REGIONS_FEP_BRIDGE_H */
