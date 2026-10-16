//=============================================================================
// nimcp_pr_plasticity_bridge.h - Prime Resonant Plasticity Bridge
//=============================================================================
/**
 * @file nimcp_pr_plasticity_bridge.h
 * @brief Bidirectional integration between Prime Resonant memory and plasticity
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Bridge connecting Prime Resonant memory (entanglement, resonance, quaternion)
 *       with plasticity mechanisms (STDP, BCM, homeostatic, metaplasticity, structural)
 * WHY:  Memory strength should be modulated by plasticity rules, and plasticity
 *       parameters should be influenced by memory resonance and consolidation state
 * HOW:  Bidirectional integration where:
 *       - Memory resonance affects STDP learning rates
 *       - Memory consolidation (quaternion.w) gates plasticity
 *       - Entanglement edge weights are updated by plasticity rules
 *       - Memory tier (Z0-Z3) determines homeostatic targets
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Plasticity <-> Memory Integration:
 *   +-----------------------------------------------------------------------+
 *   |                                                                        |
 *   |  STDP (Spike-Timing Dependent Plasticity):                            |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  Pre before Post (+Delta_t) -> LTP -> Strengthen Entanglement  |   |
 *   |  |  Post before Pre (-Delta_t) -> LTD -> Weaken Entanglement      |   |
 *   |  |                                                                 |   |
 *   |  |  High resonance nodes -> Higher STDP learning rate              |   |
 *   |  |  Low resonance nodes  -> Lower STDP learning rate               |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  BCM (Bienenstock-Cooper-Munro):                                      |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  Sliding threshold theta based on resonance history             |   |
 *   |  |  High activity -> raise theta -> harder to strengthen           |   |
 *   |  |  Low activity  -> lower theta -> easier to strengthen           |   |
 *   |  |                                                                 |   |
 *   |  |  theta_m (memory threshold) = f(mean_resonance_score)          |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  Homeostatic:                                                          |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  Target activity level for memory tier                          |   |
 *   |  |  Z0: High activity target (working memory)                      |   |
 *   |  |  Z1: Medium-high activity (short-term)                          |   |
 *   |  |  Z2: Medium-low activity (long-term)                            |   |
 *   |  |  Z3: Low activity target (long-term storage)                    |   |
 *   |  |                                                                 |   |
 *   |  |  Scale entanglement weights to maintain target                  |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  Metaplasticity (Plasticity of Plasticity):                           |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  Consolidated memories (high quat.w) -> reduced plasticity      |   |
 *   |  |  Fresh memories (low quat.w) -> enhanced plasticity             |   |
 *   |  |  Prior activity history modulates future learning               |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  Structural Plasticity:                                               |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  High resonance between nodes -> create new edge                |   |
 *   |  |  Very weak edges -> prune (synapse elimination)                 |   |
 *   |  |  Remodel graph topology based on activity patterns              |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   +-----------------------------------------------------------------------+
 *
 *   Tier-Specific Plasticity Rules:
 *   +-----------------------------------------------------------------------+
 *   |  Tier | Target Activity | STDP Rate | BCM tau | Homeostatic         |
 *   |-------|-----------------|-----------|---------|---------------------|
 *   |  Z0   | 0.8 (high)      | 1.0x      | Fast    | Strong scaling      |
 *   |  Z1   | 0.5 (medium)    | 0.7x      | Medium  | Moderate scaling    |
 *   |  Z2   | 0.3 (low)       | 0.4x      | Slow    | Gentle scaling      |
 *   |  Z3   | 0.1 (minimal)   | 0.2x      | V.Slow  | Minimal scaling     |
 *   +-----------------------------------------------------------------------+
 *
 *   Consolidation Gate:
 *   +-----------------------------------------------------------------------+
 *   |  Quaternion.w (consolidation) controls plasticity permission:         |
 *   |                                                                        |
 *   |  w < 0.3: "Fragile" -> High plasticity, can be easily modified        |
 *   |  0.3 <= w < 0.6: "Stabilizing" -> Moderate plasticity                 |
 *   |  0.6 <= w < 0.9: "Consolidated" -> Low plasticity, protected          |
 *   |  w >= 0.9: "Permanent" -> Very low plasticity, highly resistant       |
 *   |                                                                        |
 *   |  This prevents overwriting consolidated memories (catastrophic        |
 *   |  forgetting protection).                                              |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - STDP update: ~50ns per edge
 * - BCM threshold update: ~20ns per node
 * - Homeostatic scaling: ~100ns per tier
 * - Full bridge update: ~1ms for 10K edges
 *
 * MEMORY:
 * - pr_plasticity_bridge_t: ~2KB base + event buffer
 * - Per-edge overhead: ~8 bytes (weight delta tracking)
 *
 * INTEGRATION:
 * - Core: Entanglement graph, resonance engine, quaternion state
 * - SNN: STDP bridge, BCM bridge, homeostatic bridge
 * - Plasticity: Plasticity coordinator
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PR_PLASTICITY_BRIDGE_H
#define NIMCP_PR_PLASTICITY_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Prime Resonant core dependencies */
#include "nimcp_entanglement.h"
#include "nimcp_resonance.h"
#include "nimcp_quaternion.h"
#include "nimcp_pr_memory_node.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Export macro (for shared library builds) */
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of plasticity events to track */
#define PR_PLASTICITY_MAX_EVENTS           4096

/** Default STDP LTP amplitude */
#define PR_STDP_DEFAULT_A_PLUS             0.1f

/** Default STDP LTD amplitude */
#define PR_STDP_DEFAULT_A_MINUS            0.12f

/** Default STDP LTP time constant (ms) */
#define PR_STDP_DEFAULT_TAU_PLUS           20.0f

/** Default STDP LTD time constant (ms) */
#define PR_STDP_DEFAULT_TAU_MINUS          20.0f

/** Default resonance modulation factor */
#define PR_STDP_DEFAULT_RESONANCE_MOD      0.5f

/** Default BCM initial threshold */
#define PR_BCM_DEFAULT_THETA_INITIAL       0.5f

/** Default BCM threshold time constant (hours) */
#define PR_BCM_DEFAULT_THETA_TAU           2.0f

/** Default BCM resonance weight */
#define PR_BCM_DEFAULT_RESONANCE_WEIGHT    0.3f

/** Homeostatic target rate for Z0 (working memory) */
#define PR_HOMEOSTATIC_TARGET_Z0           0.8f

/** Homeostatic target rate for Z1 (short-term) */
#define PR_HOMEOSTATIC_TARGET_Z1           0.5f

/** Homeostatic target rate for Z2 (long-term) */
#define PR_HOMEOSTATIC_TARGET_Z2           0.3f

/** Homeostatic target rate for Z3 (deep storage) */
#define PR_HOMEOSTATIC_TARGET_Z3           0.1f

/** Default homeostatic scaling time constant */
#define PR_HOMEOSTATIC_DEFAULT_SCALING_TAU 1000.0f

/** Default minimum scaling factor */
#define PR_HOMEOSTATIC_MIN_SCALE           0.5f

/** Default maximum scaling factor */
#define PR_HOMEOSTATIC_MAX_SCALE           2.0f

/** Default consolidation gate threshold */
#define PR_CONSOLIDATION_GATE_DEFAULT      0.3f

/** Minimum weight for edge pruning */
#define PR_STRUCTURAL_PRUNE_THRESHOLD      0.1f

/** Minimum resonance for edge creation */
#define PR_STRUCTURAL_CREATE_THRESHOLD     0.7f

/** Maximum edges to create per remodel pass */
#define PR_STRUCTURAL_MAX_NEW_EDGES        100

/** Maximum edges to prune per remodel pass */
#define PR_STRUCTURAL_MAX_PRUNE_EDGES      100

/** Number of memory tiers */
#define PR_PLASTICITY_NUM_TIERS            4

/** Numerical epsilon */
#define PR_PLASTICITY_EPSILON              1e-6f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Plasticity mechanism types
 *
 * WHAT: Types of plasticity mechanisms the bridge can apply
 * WHY:  Different mechanisms operate on different timescales and rules
 */
typedef enum {
    PR_PLASTICITY_STDP = 0,       /**< Spike-timing dependent plasticity */
    PR_PLASTICITY_BCM,            /**< Bienenstock-Cooper-Munro rate-based */
    PR_PLASTICITY_HOMEOSTATIC,    /**< Activity normalization */
    PR_PLASTICITY_METAPLASTICITY, /**< Plasticity of plasticity */
    PR_PLASTICITY_STRUCTURAL,     /**< Synapse creation/removal */
    PR_PLASTICITY_TYPE_COUNT      /**< Number of plasticity types */
} pr_plasticity_type_t;

/**
 * @brief Memory tier indices (matching Z-Ladder)
 * Uses pr_memory_tier_t from nimcp_pr_memory_node.h
 * These aliases provided for backwards compatibility
 */
#define PR_TIER_Z0 PR_MEMORY_TIER_Z0
#define PR_TIER_Z1 PR_MEMORY_TIER_Z1
#define PR_TIER_Z2 PR_MEMORY_TIER_Z2
#define PR_TIER_Z3 PR_MEMORY_TIER_Z3

/**
 * @brief STDP parameters for memory plasticity
 *
 * WHAT: Parameters controlling spike-timing dependent plasticity
 * WHY:  STDP governs how temporal correlations strengthen/weaken edges
 *
 * BIOLOGICAL: Based on Bi & Poo (1998) timing windows
 */
typedef struct {
    float A_plus;                 /**< LTP amplitude (default 0.1) */
    float A_minus;                /**< LTD amplitude (default 0.12) */
    float tau_plus;               /**< LTP time constant (ms, default 20) */
    float tau_minus;              /**< LTD time constant (ms, default 20) */
    float resonance_modulation;   /**< Resonance -> learning rate scale [0-1] */
} pr_stdp_params_t;

/**
 * @brief BCM parameters for memory plasticity
 *
 * WHAT: Parameters controlling BCM sliding threshold plasticity
 * WHY:  BCM provides rate-based self-stabilizing learning
 *
 * BIOLOGICAL: Based on Bienenstock et al. (1982) visual cortex model
 */
typedef struct {
    float theta_initial;          /**< Initial modification threshold */
    float theta_tau;              /**< Threshold time constant (hours) */
    float resonance_weight;       /**< Weight of resonance in theta calculation */
} pr_bcm_params_t;

/**
 * @brief Homeostatic plasticity parameters
 *
 * WHAT: Parameters controlling synaptic scaling and activity normalization
 * WHY:  Homeostatic plasticity prevents runaway excitation/inhibition
 *
 * BIOLOGICAL: Based on Turrigiano & Nelson (2004) scaling rules
 */
typedef struct {
    float target_rate[PR_PLASTICITY_NUM_TIERS];  /**< Target activity per tier */
    float scaling_tau;                           /**< Scaling time constant (ms) */
    float min_scale;                             /**< Minimum scaling factor */
    float max_scale;                             /**< Maximum scaling factor */
} pr_homeostatic_params_t;

/**
 * @brief Metaplasticity parameters
 *
 * WHAT: Parameters controlling plasticity of plasticity
 * WHY:  Prior history should affect future learning capacity
 */
typedef struct {
    float history_decay;          /**< Decay rate for activity history */
    float min_lr_scale;           /**< Minimum learning rate scale */
    float max_lr_scale;           /**< Maximum learning rate scale */
    float consolidation_protection; /**< Protection factor for consolidated memories */
} pr_metaplasticity_params_t;

/**
 * @brief Structural plasticity parameters
 *
 * WHAT: Parameters controlling synapse creation and pruning
 * WHY:  Graph topology should adapt to activity patterns
 */
typedef struct {
    float create_threshold;       /**< Min resonance for edge creation */
    float prune_threshold;        /**< Max weight for edge pruning */
    uint32_t max_new_edges;       /**< Max edges to create per pass */
    uint32_t max_prune_edges;     /**< Max edges to prune per pass */
    float remodel_interval_ms;    /**< Interval between remodel passes */
} pr_structural_params_t;

/**
 * @brief Plasticity event record
 *
 * WHAT: Record of a single plasticity event for tracking
 * WHY:  Track what changes occurred for debugging and analysis
 */
typedef struct {
    uint64_t from_node;           /**< Source node of affected edge */
    uint64_t to_node;             /**< Target node of affected edge */
    pr_plasticity_type_t type;    /**< Type of plasticity applied */
    float delta_weight;           /**< Change in weight */
    float pre_weight;             /**< Weight before change */
    float post_weight;            /**< Weight after change */
    float resonance_at_event;     /**< Resonance score when event occurred */
    uint64_t timestamp_ms;        /**< When event occurred */
} pr_plasticity_event_t;

/**
 * @brief Per-tier plasticity parameters
 *
 * WHAT: Tier-specific plasticity configuration
 * WHY:  Different memory tiers have different plasticity needs
 */
typedef struct {
    float stdp_rate_scale;        /**< STDP learning rate multiplier */
    float bcm_tau_scale;          /**< BCM threshold tau multiplier */
    float homeostatic_strength;   /**< Homeostatic scaling strength */
    float target_activity;        /**< Target activity level */
    float metaplasticity_gate;    /**< Metaplasticity gating factor */
} pr_tier_plasticity_params_t;

/**
 * @brief Bridge configuration
 *
 * WHAT: Complete configuration for plasticity bridge
 * WHY:  Centralize all configuration options
 */
typedef struct {
    pr_stdp_params_t stdp;                /**< STDP parameters */
    pr_bcm_params_t bcm;                  /**< BCM parameters */
    pr_homeostatic_params_t homeostatic;  /**< Homeostatic parameters */
    pr_metaplasticity_params_t meta;      /**< Metaplasticity parameters */
    pr_structural_params_t structural;    /**< Structural plasticity params */
    pr_tier_plasticity_params_t tier[PR_PLASTICITY_NUM_TIERS]; /**< Per-tier params */

    bool enable_stdp;             /**< Enable STDP plasticity */
    bool enable_bcm;              /**< Enable BCM plasticity */
    bool enable_homeostatic;      /**< Enable homeostatic plasticity */
    bool enable_metaplasticity;   /**< Enable metaplasticity */
    bool enable_structural;       /**< Enable structural plasticity */

    float consolidation_gate;     /**< Min quaternion.w for plasticity */
    bool enable_event_logging;    /**< Track plasticity events */
    uint32_t max_events;          /**< Maximum events to store */

    bool enable_bio_async;        /**< Enable bio-async messaging */
    bool enable_coordinator_sync; /**< Sync with plasticity coordinator */
} pr_plasticity_bridge_config_t;

/**
 * @brief Bridge statistics
 *
 * WHAT: Operational metrics for the plasticity bridge
 * WHY:  Track bridge health and performance
 */
typedef struct {
    /* Event counts */
    uint64_t stdp_ltp_events;          /**< STDP LTP event count */
    uint64_t stdp_ltd_events;          /**< STDP LTD event count */
    uint64_t bcm_updates;              /**< BCM threshold updates */
    uint64_t homeostatic_scalings;     /**< Homeostatic scaling events */
    uint64_t metaplasticity_events;    /**< Metaplasticity adjustments */
    uint64_t edges_created;            /**< Structural: edges created */
    uint64_t edges_pruned;             /**< Structural: edges pruned */

    /* Weight statistics */
    float total_weight_change;         /**< Sum of |delta_weight| */
    float avg_weight_change;           /**< Average |delta_weight| */
    float max_weight_change;           /**< Maximum |delta_weight| */

    /* Resonance modulation */
    float avg_resonance_modulation;    /**< Average resonance at events */
    float avg_consolidation_gate;      /**< Average quaternion.w at events */

    /* Tier statistics */
    uint64_t events_per_tier[PR_PLASTICITY_NUM_TIERS]; /**< Events per tier */
    float avg_activity_per_tier[PR_PLASTICITY_NUM_TIERS]; /**< Avg activity */

    /* Blocked events */
    uint64_t blocked_by_consolidation; /**< Events blocked by consolidation */

    /* Timing */
    uint64_t total_updates;            /**< Total update cycles */
    float avg_update_time_us;          /**< Average update time */
} pr_plasticity_bridge_stats_t;

/**
 * @brief Per-node BCM state
 *
 * WHAT: BCM sliding threshold state for a memory node
 * WHY:  Track activity history for BCM threshold computation
 */
typedef struct {
    uint64_t node_id;             /**< Node identifier */
    float theta;                  /**< Current BCM threshold */
    float activity_avg;           /**< Running average activity */
    float activity_squared_avg;   /**< Running average of squared activity */
    float resonance_avg;          /**< Running average resonance */
    uint64_t last_update_ms;      /**< Last update timestamp */
} pr_bcm_node_state_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct pr_plasticity_bridge_struct* pr_plasticity_bridge_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides biologically-plausible starting point
 *
 * @return Default configuration with:
 *         - STDP: A+=0.1, A-=0.12, tau=20ms
 *         - BCM: theta_init=0.5, tau=2h
 *         - Homeostatic: tier-based targets
 *         - Consolidation gate: 0.3
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT pr_plasticity_bridge_config_t pr_plasticity_config_default(void);

/**
 * @brief Validate bridge configuration
 *
 * WHAT: Check configuration for validity
 * WHY:  Prevent invalid parameters causing runtime errors
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Validation rules:
 * - STDP amplitudes must be positive
 * - Time constants must be positive
 * - Thresholds must be in valid ranges
 *
 * Performance: ~50ns
 */
NIMCP_EXPORT bool pr_plasticity_config_validate(const pr_plasticity_bridge_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create plasticity bridge
 *
 * WHAT: Initialize plasticity bridge for Prime Resonant memory
 * WHY:  Entry point for bidirectional plasticity integration
 * HOW:  Allocate state, initialize parameters, connect to entanglement graph
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge handle, or NULL on failure
 *
 * Performance: O(1)
 * Memory: ~2KB base + event buffer
 *
 * Thread safety: The returned bridge is thread-safe for concurrent use
 *
 * Example:
 *   pr_plasticity_bridge_config_t config = pr_plasticity_config_default();
 *   config.enable_structural = false;  // Disable structural plasticity
 *   pr_plasticity_bridge_t bridge = pr_plasticity_bridge_create(&config);
 */
NIMCP_EXPORT pr_plasticity_bridge_t pr_plasticity_bridge_create(
    const pr_plasticity_bridge_config_t* config);

/**
 * @brief Destroy plasticity bridge
 *
 * WHAT: Free all bridge resources
 * WHY:  Proper resource cleanup
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * Performance: O(n) where n = tracked nodes
 */
NIMCP_EXPORT void pr_plasticity_bridge_destroy(pr_plasticity_bridge_t bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset statistics and event buffer
 * WHY:  Start fresh measurement period
 *
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 *
 * Performance: O(n) where n = tracked nodes
 */
NIMCP_EXPORT int pr_plasticity_bridge_reset(pr_plasticity_bridge_t bridge);

//=============================================================================
// STDP Integration Functions
//=============================================================================

/**
 * @brief Apply STDP to entanglement edge
 *
 * WHAT: Update edge weight based on pre/post spike timing
 * WHY:  Temporal correlation learning for memory association
 * HOW:  Compute STDP delta, modulate by resonance, apply to edge
 *
 * @param bridge Plasticity bridge
 * @param graph Entanglement graph containing edge
 * @param from_id Pre-synaptic node ID
 * @param to_id Post-synaptic node ID
 * @param pre_time_ms Pre-synaptic spike time (ms)
 * @param post_time_ms Post-synaptic spike time (ms)
 * @param resonance Current resonance score for edge (0-1)
 * @return New edge weight, or -1.0f on error
 *
 * STDP Rule:
 *   dt = post_time - pre_time
 *   if dt > 0: dw = A+ * exp(-dt/tau+) * (1 + res_mod * resonance)
 *   if dt < 0: dw = -A- * exp(dt/tau-) * (1 + res_mod * resonance)
 *
 * Performance: ~50ns
 */
NIMCP_EXPORT float pr_stdp_apply_to_entanglement(
    pr_plasticity_bridge_t bridge,
    entangle_graph_t graph,
    uint64_t from_id,
    uint64_t to_id,
    float pre_time_ms,
    float post_time_ms,
    float resonance);

/**
 * @brief Compute STDP weight change
 *
 * WHAT: Calculate weight delta without applying
 * WHY:  Preview change for conflict resolution
 *
 * @param bridge Plasticity bridge
 * @param pre_time_ms Pre-synaptic spike time
 * @param post_time_ms Post-synaptic spike time
 * @param resonance Resonance score (0-1)
 * @return Weight change delta
 *
 * Performance: ~20ns
 */
NIMCP_EXPORT float pr_stdp_compute_delta(
    pr_plasticity_bridge_t bridge,
    float pre_time_ms,
    float post_time_ms,
    float resonance);

/**
 * @brief Batch STDP update for multiple edges
 *
 * WHAT: Apply STDP to multiple edges efficiently
 * WHY:  Batch processing for performance
 *
 * @param bridge Plasticity bridge
 * @param graph Entanglement graph
 * @param from_ids Array of pre-synaptic node IDs
 * @param to_ids Array of post-synaptic node IDs
 * @param pre_times Array of pre spike times
 * @param post_times Array of post spike times
 * @param resonances Array of resonance scores
 * @param count Number of edges to process
 * @return Number of edges successfully updated
 *
 * Performance: ~40ns per edge (amortized)
 */
NIMCP_EXPORT uint32_t pr_stdp_batch_update(
    pr_plasticity_bridge_t bridge,
    entangle_graph_t graph,
    const uint64_t* from_ids,
    const uint64_t* to_ids,
    const float* pre_times,
    const float* post_times,
    const float* resonances,
    uint32_t count);

/**
 * @brief Modulate STDP delta by resonance
 *
 * WHAT: Scale base STDP delta by resonance score
 * WHY:  High-resonance edges should learn faster
 *
 * @param bridge Plasticity bridge
 * @param base_delta Base STDP weight change
 * @param resonance Resonance score (0-1)
 * @return Modulated delta
 *
 * Formula: modulated = base_delta * (1 + resonance_mod * resonance)
 *
 * Performance: ~5ns
 */
NIMCP_EXPORT float pr_stdp_modulate_by_resonance(
    pr_plasticity_bridge_t bridge,
    float base_delta,
    float resonance);

//=============================================================================
// BCM Integration Functions
//=============================================================================

/**
 * @brief Compute BCM threshold for node
 *
 * WHAT: Calculate sliding modification threshold for a node
 * WHY:  BCM threshold determines LTP/LTD boundary
 *
 * @param bridge Plasticity bridge
 * @param node_id Node to compute threshold for
 * @return Current BCM threshold, or -1.0f on error
 *
 * Formula: theta = <activity^2> weighted by resonance history
 *
 * Performance: ~20ns
 */
NIMCP_EXPORT float pr_bcm_compute_threshold(
    pr_plasticity_bridge_t bridge,
    uint64_t node_id);

/**
 * @brief Apply BCM plasticity to node
 *
 * WHAT: Update node's edges based on BCM rule
 * WHY:  Rate-based plasticity for selectivity
 *
 * @param bridge Plasticity bridge
 * @param graph Entanglement graph
 * @param node_id Node to update
 * @param activity Current activity level (0-1)
 * @return Number of edges updated
 *
 * BCM Rule:
 *   phi(activity, theta) = activity * (activity - theta)
 *   if phi > 0: LTP (strengthen edges)
 *   if phi < 0: LTD (weaken edges)
 *
 * Performance: O(degree) where degree = node's edge count
 */
NIMCP_EXPORT int pr_bcm_apply_to_node(
    pr_plasticity_bridge_t bridge,
    entangle_graph_t graph,
    uint64_t node_id,
    float activity);

/**
 * @brief Update BCM activity history
 *
 * WHAT: Update running average activity for BCM threshold
 * WHY:  Threshold adapts to activity history
 *
 * @param bridge Plasticity bridge
 * @param node_id Node to update
 * @param activity Current activity level
 * @return 0 on success, -1 on error
 *
 * Performance: ~15ns
 */
NIMCP_EXPORT int pr_bcm_update_history(
    pr_plasticity_bridge_t bridge,
    uint64_t node_id,
    float activity);

/**
 * @brief Get BCM phi function value
 *
 * WHAT: Compute phi(activity, theta) for LTP/LTD decision
 * WHY:  Preview BCM outcome without applying
 *
 * @param activity Post-synaptic activity level
 * @param theta BCM modification threshold
 * @return phi value: positive = LTP, negative = LTD
 *
 * Formula: phi(y) = y * (y - theta)
 *
 * Performance: ~3ns
 */
NIMCP_EXPORT float pr_bcm_get_phi(float activity, float theta);

//=============================================================================
// Homeostatic Integration Functions
//=============================================================================

/**
 * @brief Scale all edges in a memory tier
 *
 * WHAT: Apply homeostatic scaling to maintain target activity
 * WHY:  Prevent runaway excitation/inhibition in tier
 *
 * @param bridge Plasticity bridge
 * @param graph Entanglement graph
 * @param tier Memory tier (Z0-Z3)
 * @param node_ids Array of node IDs in tier
 * @param node_count Number of nodes
 * @return Number of edges scaled
 *
 * Scaling:
 *   scale = 1 + (target - current) / tau
 *   for each edge: weight *= clamp(scale, min_scale, max_scale)
 *
 * Performance: O(total edges in tier)
 */
NIMCP_EXPORT uint32_t pr_homeostatic_scale_tier(
    pr_plasticity_bridge_t bridge,
    entangle_graph_t graph,
    pr_memory_tier_t tier,
    const uint64_t* node_ids,
    uint32_t node_count);

/**
 * @brief Get homeostatic scaling factor for node
 *
 * WHAT: Compute scaling factor based on deviation from target
 * WHY:  Preview scaling without applying
 *
 * @param bridge Plasticity bridge
 * @param node_id Node to query
 * @param current_activity Current activity level
 * @param tier Memory tier
 * @return Scaling factor (clamped to [min_scale, max_scale])
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT float pr_homeostatic_get_scaling(
    pr_plasticity_bridge_t bridge,
    uint64_t node_id,
    float current_activity,
    pr_memory_tier_t tier);

/**
 * @brief Apply homeostatic scaling to single edge
 *
 * WHAT: Scale individual edge weight
 * WHY:  Fine-grained homeostatic control
 *
 * @param bridge Plasticity bridge
 * @param graph Entanglement graph
 * @param from_id Source node
 * @param to_id Target node
 * @param scale Scaling factor to apply
 * @return New edge weight, or -1.0f on error
 *
 * Performance: ~30ns
 */
NIMCP_EXPORT float pr_homeostatic_apply_to_edge(
    pr_plasticity_bridge_t bridge,
    entangle_graph_t graph,
    uint64_t from_id,
    uint64_t to_id,
    float scale);

/**
 * @brief Update target activity levels
 *
 * WHAT: Recalculate target activities based on current state
 * WHY:  Targets may need adjustment over time
 *
 * @param bridge Plasticity bridge
 * @return 0 on success, -1 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_homeostatic_update_targets(pr_plasticity_bridge_t bridge);

//=============================================================================
// Metaplasticity Functions
//=============================================================================

/**
 * @brief Adjust STDP parameters based on history
 *
 * WHAT: Modify STDP learning rates based on prior activity
 * WHY:  Prevent saturation, allow recovery after intense learning
 *
 * @param bridge Plasticity bridge
 * @param node_id Node to adjust
 * @return Adjusted STDP rate multiplier
 *
 * Rule:
 *   High prior activity -> reduce learning rate
 *   Low prior activity -> increase learning rate
 *
 * Performance: ~15ns
 */
NIMCP_EXPORT float pr_metaplasticity_adjust_stdp(
    pr_plasticity_bridge_t bridge,
    uint64_t node_id);

/**
 * @brief Derive metaplasticity from consolidation state
 *
 * WHAT: Use quaternion consolidation to modulate plasticity
 * WHY:  Consolidated memories should be protected
 *
 * @param bridge Plasticity bridge
 * @param quat Memory quaternion state
 * @return Plasticity rate multiplier (0-1)
 *
 * Rule:
 *   High quat.w (consolidated) -> low plasticity
 *   Low quat.w (fragile) -> high plasticity
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT float pr_metaplasticity_from_consolidation(
    pr_plasticity_bridge_t bridge,
    nimcp_quaternion_t quat);

//=============================================================================
// Structural Plasticity Functions
//=============================================================================

/**
 * @brief Create new entanglement edge based on resonance
 *
 * WHAT: Add edge between nodes with high resonance
 * WHY:  Synaptogenesis - new connections for strong associations
 *
 * @param bridge Plasticity bridge
 * @param graph Entanglement graph
 * @param from_id Source node
 * @param to_id Target node
 * @param resonance Resonance score (should be > create_threshold)
 * @return true if edge created, false otherwise
 *
 * Conditions:
 * - Resonance must exceed create_threshold
 * - Edge must not already exist
 * - Both nodes must exist in graph
 *
 * Performance: O(1) average
 */
NIMCP_EXPORT bool pr_structural_create_edge(
    pr_plasticity_bridge_t bridge,
    entangle_graph_t graph,
    uint64_t from_id,
    uint64_t to_id,
    float resonance);

/**
 * @brief Prune weak entanglement edge
 *
 * WHAT: Remove edge with weight below threshold
 * WHY:  Synapse elimination - remove unused connections
 *
 * @param bridge Plasticity bridge
 * @param graph Entanglement graph
 * @param from_id Source node
 * @param to_id Target node
 * @return true if edge pruned, false otherwise
 *
 * Conditions:
 * - Edge weight must be below prune_threshold
 * - Edge must exist
 *
 * Performance: O(1) average
 */
NIMCP_EXPORT bool pr_structural_prune_edge(
    pr_plasticity_bridge_t bridge,
    entangle_graph_t graph,
    uint64_t from_id,
    uint64_t to_id);

/**
 * @brief Full structural remodeling pass
 *
 * WHAT: Scan graph for edges to create/prune
 * WHY:  Periodic graph topology optimization
 *
 * @param bridge Plasticity bridge
 * @param graph Entanglement graph
 * @param node_ids Array of candidate nodes
 * @param node_count Number of candidate nodes
 * @param edges_created Output: number of edges created
 * @param edges_pruned Output: number of edges pruned
 * @return 0 on success, -1 on error
 *
 * Algorithm:
 * 1. For each node, check edges below prune_threshold
 * 2. Prune up to max_prune_edges weak edges
 * 3. For high-resonance node pairs without edges, create up to max_new_edges
 *
 * Performance: O(nodes * avg_degree)
 */
NIMCP_EXPORT int pr_structural_remodel(
    pr_plasticity_bridge_t bridge,
    entangle_graph_t graph,
    const uint64_t* node_ids,
    uint32_t node_count,
    uint32_t* edges_created,
    uint32_t* edges_pruned);

//=============================================================================
// Quaternion <-> Plasticity Functions
//=============================================================================

/**
 * @brief Derive plasticity rates from quaternion state
 *
 * WHAT: Convert quaternion memory state to plasticity parameters
 * WHY:  Memory state should influence learning dynamics
 *
 * @param bridge Plasticity bridge
 * @param quat Memory quaternion
 * @param stdp_rate Output: STDP rate multiplier
 * @param bcm_rate Output: BCM update rate
 * @param homeostatic_rate Output: Homeostatic scaling rate
 * @return 0 on success, -1 on error
 *
 * Mapping:
 * - w (consolidation) -> protection level (lower plasticity)
 * - x (emotion) -> modulation (extreme emotions enhance)
 * - y (salience) -> priority (salient memories learn faster)
 * - z (accessibility) -> exposure (accessible = more plastic)
 *
 * Performance: ~15ns
 */
NIMCP_EXPORT int pr_plasticity_from_quaternion(
    pr_plasticity_bridge_t bridge,
    nimcp_quaternion_t quat,
    float* stdp_rate,
    float* bcm_rate,
    float* homeostatic_rate);

/**
 * @brief Update quaternion from plasticity events
 *
 * WHAT: Modify quaternion state based on plasticity history
 * WHY:  Learning activity affects memory consolidation
 *
 * @param bridge Plasticity bridge
 * @param quat_in Input quaternion
 * @param events Array of plasticity events
 * @param event_count Number of events
 * @param quat_out Output updated quaternion
 * @return 0 on success, -1 on error
 *
 * Updates:
 * - Strong LTP -> increase consolidation (w)
 * - Repeated access -> increase accessibility (z)
 *
 * Performance: O(event_count)
 */
NIMCP_EXPORT int pr_quaternion_from_plasticity(
    pr_plasticity_bridge_t bridge,
    nimcp_quaternion_t quat_in,
    const pr_plasticity_event_t* events,
    uint32_t event_count,
    nimcp_quaternion_t* quat_out);

/**
 * @brief Check if plasticity is allowed by consolidation gate
 *
 * WHAT: Determine if quaternion permits plasticity
 * WHY:  Protect consolidated memories from modification
 *
 * @param bridge Plasticity bridge
 * @param quat Memory quaternion
 * @return true if plasticity allowed, false if blocked
 *
 * Rule:
 *   quat.w < consolidation_gate -> plasticity allowed
 *   quat.w >= consolidation_gate -> check protection level
 *
 * Performance: ~5ns
 */
NIMCP_EXPORT bool pr_consolidation_gate(
    pr_plasticity_bridge_t bridge,
    nimcp_quaternion_t quat);

//=============================================================================
// Resonance <-> Plasticity Functions
//=============================================================================

/**
 * @brief Convert resonance to STDP learning rate
 *
 * WHAT: Map resonance score to learning rate multiplier
 * WHY:  High-resonance edges should learn faster
 *
 * @param bridge Plasticity bridge
 * @param resonance Resonance score (0-1)
 * @return Learning rate multiplier (1.0 at resonance=0, up to 1+mod at 1)
 *
 * Formula: rate = 1 + resonance_modulation * resonance
 *
 * Performance: ~3ns
 */
NIMCP_EXPORT float pr_resonance_to_learning_rate(
    pr_plasticity_bridge_t bridge,
    float resonance);

/**
 * @brief Convert resonance to BCM weight contribution
 *
 * WHAT: Map resonance to BCM threshold contribution
 * WHY:  Resonance history affects BCM threshold
 *
 * @param bridge Plasticity bridge
 * @param resonance Resonance score (0-1)
 * @return BCM weight contribution
 *
 * Performance: ~3ns
 */
NIMCP_EXPORT float pr_resonance_to_bcm_weight(
    pr_plasticity_bridge_t bridge,
    float resonance);

/**
 * @brief Update resonance score from plasticity event
 *
 * WHAT: Modify edge resonance based on plasticity outcome
 * WHY:  Plasticity affects future resonance
 *
 * @param bridge Plasticity bridge
 * @param graph Entanglement graph
 * @param from_id Source node
 * @param to_id Target node
 * @param delta_weight Weight change that occurred
 * @return New resonance score, or -1.0f on error
 *
 * Rule:
 *   LTP (positive delta) -> increase resonance slightly
 *   LTD (negative delta) -> decrease resonance slightly
 *
 * Performance: ~40ns
 */
NIMCP_EXPORT float pr_plasticity_update_resonance(
    pr_plasticity_bridge_t bridge,
    entangle_graph_t graph,
    uint64_t from_id,
    uint64_t to_id,
    float delta_weight);

//=============================================================================
// Tier-Specific Functions
//=============================================================================

/**
 * @brief Get plasticity parameters for memory tier
 *
 * WHAT: Retrieve tier-specific plasticity configuration
 * WHY:  Different tiers have different plasticity needs
 *
 * @param bridge Plasticity bridge
 * @param tier Memory tier (Z0-Z3)
 * @param params Output tier parameters
 * @return 0 on success, -1 on error
 *
 * Performance: ~5ns
 */
NIMCP_EXPORT int pr_plasticity_get_tier_params(
    pr_plasticity_bridge_t bridge,
    pr_memory_tier_t tier,
    pr_tier_plasticity_params_t* params);

/**
 * @brief Apply tier-specific plasticity rules
 *
 * WHAT: Apply all plasticity rules appropriate for tier
 * WHY:  Centralized tier-based plasticity application
 *
 * @param bridge Plasticity bridge
 * @param graph Entanglement graph
 * @param node_id Node to process
 * @param tier Memory tier
 * @param activity Current activity level
 * @return Number of modifications applied
 *
 * Performance: O(node degree)
 */
NIMCP_EXPORT int pr_plasticity_apply_tier_rules(
    pr_plasticity_bridge_t bridge,
    entangle_graph_t graph,
    uint64_t node_id,
    pr_memory_tier_t tier,
    float activity);

//=============================================================================
// Event Tracking Functions
//=============================================================================

/**
 * @brief Log a plasticity event
 *
 * WHAT: Record plasticity event in buffer
 * WHY:  Track history for analysis and quaternion updates
 *
 * @param bridge Plasticity bridge
 * @param event Event to log
 * @return 0 on success, -1 on error (buffer full)
 *
 * Performance: ~20ns
 */
NIMCP_EXPORT int pr_plasticity_log_event(
    pr_plasticity_bridge_t bridge,
    const pr_plasticity_event_t* event);

/**
 * @brief Get recent plasticity events
 *
 * WHAT: Retrieve logged events from buffer
 * WHY:  Analysis and quaternion update computation
 *
 * @param bridge Plasticity bridge
 * @param events Output event array (caller-allocated)
 * @param max_events Maximum events to return
 * @param event_count Output: actual events returned
 * @return 0 on success, -1 on error
 *
 * Performance: O(min(buffer_size, max_events))
 */
NIMCP_EXPORT int pr_plasticity_get_events(
    pr_plasticity_bridge_t bridge,
    pr_plasticity_event_t* events,
    uint32_t max_events,
    uint32_t* event_count);

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieve operational metrics
 * WHY:  Monitoring and debugging
 *
 * @param bridge Plasticity bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_plasticity_get_stats(
    pr_plasticity_bridge_t bridge,
    pr_plasticity_bridge_stats_t* stats);

/**
 * @brief Clear event buffer
 *
 * WHAT: Remove all logged events
 * WHY:  Reset after processing
 *
 * @param bridge Plasticity bridge
 * @return 0 on success, -1 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_plasticity_clear_events(pr_plasticity_bridge_t bridge);

//=============================================================================
// Integration Functions
//=============================================================================

/**
 * @brief Synchronize with plasticity coordinator
 *
 * WHAT: Sync state with global plasticity coordinator
 * WHY:  Coordinate with other plasticity modules
 *
 * @param bridge Plasticity bridge
 * @return 0 on success, -1 on error
 *
 * Syncs:
 * - Current learning rates
 * - Conflict resolution decisions
 * - Global plasticity state
 *
 * Performance: ~100ns
 */
NIMCP_EXPORT int pr_plasticity_sync_with_coordinator(pr_plasticity_bridge_t bridge);

/**
 * @brief Connect to bio-async messaging
 *
 * WHAT: Enable bio-async integration
 * WHY:  Cross-system plasticity coordination
 *
 * @param bridge Plasticity bridge
 * @return 0 on success, -1 on error
 *
 * Performance: ~500ns
 */
NIMCP_EXPORT int pr_plasticity_connect_bio_async(pr_plasticity_bridge_t bridge);

/**
 * @brief Disconnect from bio-async
 *
 * @param bridge Plasticity bridge
 * @return 0 on success
 */
NIMCP_EXPORT int pr_plasticity_disconnect_bio_async(pr_plasticity_bridge_t bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Plasticity bridge
 * @return true if connected
 */
NIMCP_EXPORT bool pr_plasticity_is_bio_async_connected(pr_plasticity_bridge_t bridge);

//=============================================================================
// Main Update Function
//=============================================================================

/**
 * @brief Main bridge update
 *
 * WHAT: Perform all enabled plasticity updates
 * WHY:  Single entry point for regular updates
 * HOW:  Update BCM thresholds, apply homeostatic scaling, process events
 *
 * @param bridge Plasticity bridge
 * @param graph Entanglement graph
 * @param dt_ms Time delta since last update (milliseconds)
 * @return 0 on success, -1 on error
 *
 * Update sequence:
 * 1. Decay eligibility traces
 * 2. Update BCM thresholds
 * 3. Apply homeostatic scaling (if interval elapsed)
 * 4. Process pending structural changes
 * 5. Update statistics
 *
 * Performance: ~1ms for 10K edges typical
 */
NIMCP_EXPORT int pr_plasticity_bridge_update(
    pr_plasticity_bridge_t bridge,
    entangle_graph_t graph,
    float dt_ms);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get plasticity type name as string
 *
 * @param type Plasticity type
 * @return Human-readable string
 */
NIMCP_EXPORT const char* pr_plasticity_type_name(pr_plasticity_type_t type);

/**
 * @brief Get tier name as string
 *
 * @param tier Memory tier
 * @return Human-readable string
 */
NIMCP_EXPORT const char* pr_tier_name(pr_memory_tier_t tier);

/**
 * @brief Print plasticity event to stdout
 *
 * @param event Event to print
 */
NIMCP_EXPORT void pr_plasticity_event_print(const pr_plasticity_event_t* event);

/**
 * @brief Print bridge statistics to stdout
 *
 * @param bridge Bridge to print stats for
 */
NIMCP_EXPORT void pr_plasticity_print_stats(pr_plasticity_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PR_PLASTICITY_BRIDGE_H */
