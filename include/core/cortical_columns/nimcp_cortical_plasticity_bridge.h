/**
 * @file nimcp_cortical_plasticity_bridge.h
 * @brief Cortical Column Plasticity Bridge - Integration with Plasticity Coordinator
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between cortical columns and plasticity coordinator,
 *       managing STDP, BCM, homeostatic scaling, and critical period plasticity
 * WHY:  Cortical columns exhibit multiple plasticity mechanisms operating at different
 *       timescales - STDP for intracolumnar connections, BCM for competitive learning,
 *       homeostatic scaling for network stability, and critical period plasticity for
 *       developmental tuning (e.g., ocular dominance shifts)
 * HOW:  Bridge pattern connects cortical column synapses with plasticity coordinator,
 *       registers mechanisms, coordinates updates, and applies modulations
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * CORTICAL COLUMN PLASTICITY MECHANISMS:
 * --------------------------------------
 * Cortical columns exhibit layered plasticity operating at multiple timescales:
 *
 * 1. STDP (Spike-Timing-Dependent Plasticity):
 *    - Intracolumnar synapses: Layer 4→2/3, Layer 2/3→5/6 connections
 *    - Timing window: ±20ms (narrower in mature cortex)
 *    - Critical for precise feature tuning within minicolumns
 *    - Reference: Feldman (2012) "The spike-timing dependence of plasticity"
 *
 * 2. BCM (Bienenstock-Cooper-Munro):
 *    - Rate-based threshold adaptation for competitive learning
 *    - Winner-take-all dynamics between minicolumns in hypercolumn
 *    - Sliding threshold θ prevents runaway excitation
 *    - Critical for orientation/feature selectivity
 *    - Reference: Bienenstock et al. (1982) "Theory for the development of neuron selectivity"
 *
 * 3. Homeostatic Scaling:
 *    - Prevents runaway excitation/depression in columnar networks
 *    - Target firing rate: 4-8 Hz in cortical pyramidal cells
 *    - Slow timescale: hours-days
 *    - Multiplies all synaptic weights to maintain activity
 *    - Reference: Turrigiano & Nelson (2004) "Homeostatic plasticity"
 *
 * 4. Critical Period Plasticity:
 *    - Developmental window for rapid columnar reorganization
 *    - Ocular dominance shifts in V1 (monocular deprivation)
 *    - Barrel cortex plasticity in S1 (whisker trimming)
 *    - Plasticity boost: 2-5x normal learning rate
 *    - Duration: P20-P35 in rodents, years in humans
 *    - Reference: Hensch (2004) "Critical period regulation"
 *
 * 5. Eligibility Traces:
 *    - Synaptic "memory" for reward-modulated plasticity
 *    - Enables reinforcement learning in cortical circuits
 *    - Tau ~1-2 seconds for cortical synapses
 *    - Combined with dopamine/neuromodulator signals
 *    - Reference: Izhikevich (2007) "Solving the distal reward problem"
 *
 * PLASTICITY TIMESCALES IN CORTEX:
 * --------------------------------
 * - Fast (ms-100ms): STDP, spike-timing windows
 * - Medium (100ms-1s): BCM threshold sliding, eligibility traces
 * - Slow (minutes-hours): Homeostatic scaling, synaptic scaling
 * - Very slow (hours-days): Critical period plasticity, consolidation
 *
 * LAYER-SPECIFIC PLASTICITY:
 * -------------------------
 * - Layer 4: Strong STDP for thalamic input refinement
 * - Layer 2/3: BCM competition for feature selectivity
 * - Layer 5/6: Homeostatic scaling for output stabilization
 *
 * CRITICAL PERIOD BIOLOGY:
 * ------------------------
 * During critical periods, plasticity is dramatically enhanced to enable
 * rapid circuit reorganization. Classical examples:
 * - Ocular dominance plasticity: 4 days of monocular deprivation shifts
 *   cortical columns from deprived eye to open eye
 * - Barrel cortex: Whisker trimming expands neighboring barrel representations
 * - Molecular mechanisms: GABAergic inhibition maturation, myelin regulation,
 *   perineuronal net formation
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              CORTICAL COLUMN PLASTICITY BRIDGE                             ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                CORTICAL COLUMN STRUCTURE                            │  ║
 * ║   │                                                                     │  ║
 * ║   │   Hypercolumn                                                       │  ║
 * ║   │   ┌─────────────────────────────────────────────────────────────┐  │  ║
 * ║   │   │  Minicolumn 0   Minicolumn 1   ...   Minicolumn N          │  │  ║
 * ║   │   │  (θ=0°)         (θ=15°)              (θ=180°)              │  │  ║
 * ║   │   │                                                             │  │  ║
 * ║   │   │  Intracolumnar synapses (STDP)                             │  │  ║
 * ║   │   │  ────────────────────────────                              │  │  ║
 * ║   │   │  Layer 4  →  Layer 2/3   ──────→ STDP coordination        │  │  ║
 * ║   │   │  Layer 2/3 → Layer 5/6   ──────→ Timing windows           │  │  ║
 * ║   │   │                                                             │  │  ║
 * ║   │   │  Lateral inhibition (BCM)                                  │  │  ║
 * ║   │   │  ─────────────────────────                                 │  │  ║
 * ║   │   │  Mexican hat ←───────────────→ BCM threshold sliding       │  │  ║
 * ║   │   │  Competition ←───────────────→ Winner-take-all             │  │  ║
 * ║   │   │                                                             │  │  ║
 * ║   │   │  Network stability (Homeostatic)                           │  │  ║
 * ║   │   │  ────────────────────────────────                          │  │  ║
 * ║   │   │  Target rate ←───────────────→ Scaling factor computation  │  │  ║
 * ║   │   │  All synapses ←──────────────→ Global weight scaling       │  │  ║
 * ║   │   └─────────────────────────────────────────────────────────────┘  │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                           ║
 * ║                                ▼                                           ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              PLASTICITY COORDINATOR INTEGRATION                     │  ║
 * ║   │                                                                     │  ║
 * ║   │   STDP Mechanism Registration                                      │  ║
 * ║   │   ────────────────────────                                         │  ║
 * ║   │   • Register intracolumnar synapses                                │  ║
 * ║   │   • Update interval: 10ms                                          │  ║
 * ║   │   • Priority: 0.9 (high precision)                                 │  ║
 * ║   │   • Critical period boost: 2-5x                                    │  ║
 * ║   │                                                                     │  ║
 * ║   │   BCM Mechanism Registration                                       │  ║
 * ║   │   ───────────────────────────                                      │  ║
 * ║   │   • Register lateral connections                                   │  ║
 * ║   │   • Update interval: 50ms                                          │  ║
 * ║   │   • Priority: 0.8 (competitive learning)                           │  ║
 * ║   │   • Theta sliding: minutes timescale                               │  ║
 * ║   │                                                                     │  ║
 * ║   │   Homeostatic Mechanism Registration                               │  ║
 * ║   │   ───────────────────────────────────                              │  ║
 * ║   │   • Register all column synapses                                   │  ║
 * ║   │   • Update interval: 1000ms                                        │  ║
 * ║   │   • Priority: 0.7 (slow stabilization)                             │  ║
 * ║   │   • Target rate: 4-8 Hz                                            │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                           ║
 * ║                                ▼                                           ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  CRITICAL PERIOD MODULATION                         │  ║
 * ║   │                                                                     │  ║
 * ║   │   in_critical_period = true                                        │  ║
 * ║   │   ────────────────────────────                                     │  ║
 * ║   │   STDP learning rate ×= 3.0   (rapid tuning)                       │  ║
 * ║   │   BCM threshold ÷= 2.0        (easier competition)                 │  ║
 * ║   │   Homeostatic tau ÷= 2.0      (faster stabilization)               │  ║
 * ║   │                                                                     │  ║
 * ║   │   Examples:                                                         │  ║
 * ║   │   • Ocular dominance shift: 4-day deprivation → column shift       │  ║
 * ║   │   • Barrel cortex plasticity: whisker trim → representation expand │  ║
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
 * REFERENCES:
 * - Feldman (2012) "The spike-timing dependence of plasticity"
 * - Bienenstock et al. (1982) "Theory for the development of neuron selectivity"
 * - Turrigiano & Nelson (2004) "Homeostatic plasticity in the developing nervous system"
 * - Hensch (2004) "Critical period regulation"
 * - Izhikevich (2007) "Solving the distal reward problem"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_PLASTICITY_BRIDGE_H
#define NIMCP_CORTICAL_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Cortical column structures */
#include "core/cortical_columns/nimcp_cortical_column.h"

/* Plasticity coordinator integration */
#include "plasticity/nimcp_plasticity_coordinator.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Utilities */
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define CORTICAL_PLASTICITY_MODULE_NAME "cortical_plasticity_bridge"
#define CORTICAL_PLASTICITY_MAX_COLUMNS 256    /**< Max cortical columns tracked */

/* STDP parameters */
#define CORTICAL_STDP_A_PLUS_DEFAULT           0.01f    /**< LTP amplitude */
#define CORTICAL_STDP_A_MINUS_DEFAULT          0.0105f  /**< LTD amplitude (slightly > LTP) */
#define CORTICAL_STDP_TAU_PLUS_DEFAULT         20.0f    /**< LTP time constant (ms) */
#define CORTICAL_STDP_TAU_MINUS_DEFAULT        20.0f    /**< LTD time constant (ms) */

/* BCM parameters */
#define CORTICAL_BCM_THETA_INIT_DEFAULT        1.0f     /**< Initial threshold */
#define CORTICAL_BCM_TAU_THETA_DEFAULT         1000.0f  /**< Threshold adaptation (ms) */

/* Homeostatic parameters */
#define CORTICAL_TARGET_FIRING_RATE_DEFAULT    6.0f     /**< Target Hz for pyramidal cells */
#define CORTICAL_HOMEOSTATIC_TAU_DEFAULT       3600000.0f /**< 1 hour in ms */

/* Critical period parameters */
#define CORTICAL_CRITICAL_PERIOD_PLASTICITY_BOOST 3.0f  /**< Learning rate multiplier */
#define CORTICAL_CRITICAL_PERIOD_BCM_REDUCTION    0.5f  /**< BCM threshold reduction */
#define CORTICAL_CRITICAL_PERIOD_HOMEO_SPEEDUP    0.5f  /**< Homeostatic tau reduction */

/* Layer-specific learning rates */
#define CORTICAL_LAYER_4_LR_BOOST              1.2f     /**< Layer 4 STDP boost */
#define CORTICAL_LAYER_23_LR_BOOST             1.0f     /**< Layer 2/3 baseline */
#define CORTICAL_LAYER_56_LR_BOOST             0.8f     /**< Layer 5/6 reduced */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct cortical_plasticity_bridge cortical_plasticity_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for cortical plasticity bridge
 *
 * WHAT: Parameters controlling cortical column plasticity mechanisms
 * WHY:  Allow tuning of STDP, BCM, homeostatic, and critical period parameters
 */
typedef struct {
    /* STDP parameters */
    float stdp_a_plus;              /**< LTP amplitude */
    float stdp_a_minus;             /**< LTD amplitude */
    float stdp_tau_plus;            /**< LTP time constant (ms) */
    float stdp_tau_minus;           /**< LTD time constant (ms) */

    /* BCM parameters */
    float bcm_theta_init;           /**< Initial BCM threshold */
    float bcm_tau_theta;            /**< Threshold adaptation time constant (ms) */

    /* Homeostatic parameters */
    float target_firing_rate;       /**< Target firing rate (Hz) */
    float homeostatic_tau;          /**< Homeostatic adaptation time constant (ms) */

    /* Critical period parameters */
    bool in_critical_period;        /**< Whether in critical period */
    float critical_period_plasticity_boost;  /**< Learning rate multiplier */
    float critical_period_bcm_reduction;     /**< BCM threshold reduction */
    float critical_period_homeo_speedup;     /**< Homeostatic speedup factor */

    /* Layer-specific modulation */
    float layer_4_lr_boost;         /**< Layer 4 learning rate boost */
    float layer_23_lr_boost;        /**< Layer 2/3 learning rate boost */
    float layer_56_lr_boost;        /**< Layer 5/6 learning rate boost */

    /* Feature enables */
    bool enable_stdp;               /**< Enable STDP */
    bool enable_bcm;                /**< Enable BCM */
    bool enable_homeostatic;        /**< Enable homeostatic scaling */
    bool enable_eligibility;        /**< Enable eligibility traces */
    bool enable_bio_async;          /**< Enable bio-async integration */
} cortical_plasticity_config_t;

/**
 * @brief Per-column plasticity state
 *
 * WHAT: Tracks plasticity state for individual cortical columns
 * WHY:  Each column has independent BCM threshold and homeostatic scaling
 */
typedef struct {
    uint32_t column_id;             /**< Column identifier */

    /* BCM state */
    float bcm_threshold;            /**< Current BCM threshold */
    float avg_activity;             /**< Running average activity */

    /* Homeostatic state */
    float homeostatic_scale;        /**< Current scaling factor */
    float current_firing_rate;      /**< Current firing rate (Hz) */

    /* Eligibility traces */
    float* eligibility_traces;      /**< Per-synapse eligibility */
    uint32_t num_synapses;          /**< Number of synapses */

    /* Statistics */
    uint64_t last_update_time;      /**< Last update timestamp (ms) */
    uint64_t total_updates;         /**< Total update count */
} cortical_column_plasticity_state_t;

/**
 * @brief Cortical plasticity bridge state
 */
struct cortical_plasticity_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    cortical_plasticity_config_t config;

    /* Cortical columns */
    hypercolumn_t** columns;        /**< Array of hypercolumn pointers */
    uint32_t num_columns;           /**< Number of columns */
    uint32_t column_capacity;       /**< Capacity of columns array */

    /* Per-column state */
    cortical_column_plasticity_state_t* column_states;

    /* Plasticity coordinator integration */
    plasticity_coordinator_t* coordinator;
    uint32_t stdp_mechanism_id;     /**< Registered STDP mechanism ID */
    uint32_t bcm_mechanism_id;      /**< Registered BCM mechanism ID */
    uint32_t homeostatic_mechanism_id; /**< Registered homeostatic mechanism ID */
    uint32_t eligibility_mechanism_id; /**< Registered eligibility mechanism ID */

    /* Statistics */
    uint64_t total_stdp_updates;
    uint64_t total_bcm_updates;
    uint64_t total_homeostatic_updates;
    uint64_t total_eligibility_updates;

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default cortical plasticity configuration
 *
 * WHAT: Provide biologically-plausible default parameters
 * WHY:  Easy initialization with cortical-realistic values
 * HOW:  Set evidence-based STDP, BCM, homeostatic parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int cortical_plasticity_default_config(cortical_plasticity_config_t* config);

/**
 * @brief Create cortical plasticity bridge
 *
 * WHAT: Initialize cortical column plasticity bridge
 * WHY:  Enable coordinated plasticity across cortical columns
 * HOW:  Allocate structures, initialize per-column state
 *
 * @param config Configuration (NULL for defaults)
 * @param coordinator Plasticity coordinator (can be NULL, connected later)
 * @return New bridge or NULL on failure
 */
cortical_plasticity_bridge_t* cortical_plasticity_bridge_create(
    const cortical_plasticity_config_t* config,
    plasticity_coordinator_t* coordinator
);

/**
 * @brief Destroy cortical plasticity bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Unregister mechanisms, free memory
 *
 * NOTE: Does NOT destroy coordinator or columns (caller responsibility)
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void cortical_plasticity_bridge_destroy(cortical_plasticity_bridge_t* bridge);

/* ============================================================================
 * Coordinator Integration API
 * ============================================================================ */

/**
 * @brief Connect to plasticity coordinator
 *
 * WHAT: Register cortical plasticity mechanisms with coordinator
 * WHY:  Enable coordinated multi-mechanism plasticity
 * HOW:  Register STDP, BCM, homeostatic mechanisms with coordinator
 *
 * @param bridge Cortical plasticity bridge
 * @param coordinator Plasticity coordinator
 * @return 0 on success, -1 on error
 */
int cortical_plasticity_connect_coordinator(
    cortical_plasticity_bridge_t* bridge,
    plasticity_coordinator_t* coordinator
);

/**
 * @brief Disconnect from plasticity coordinator
 *
 * WHAT: Unregister mechanisms from coordinator
 * WHY:  Clean disconnection
 * HOW:  Unregister all mechanism IDs
 *
 * @param bridge Cortical plasticity bridge
 * @return 0 on success
 */
int cortical_plasticity_disconnect_coordinator(
    cortical_plasticity_bridge_t* bridge
);

/* ============================================================================
 * Column Management API
 * ============================================================================ */

/**
 * @brief Add cortical column to bridge
 *
 * WHAT: Register hypercolumn for plasticity management
 * WHY:  Bridge needs to track columns for per-column state
 * HOW:  Add to columns array, initialize per-column state
 *
 * @param bridge Cortical plasticity bridge
 * @param column Hypercolumn to add
 * @param column_id_out Output: assigned column ID
 * @return 0 on success, -1 on error
 */
int cortical_plasticity_add_column(
    cortical_plasticity_bridge_t* bridge,
    hypercolumn_t* column,
    uint32_t* column_id_out
);

/**
 * @brief Remove cortical column from bridge
 *
 * WHAT: Unregister hypercolumn from plasticity management
 * WHY:  Dynamic column management
 * HOW:  Remove from array, free per-column state
 *
 * @param bridge Cortical plasticity bridge
 * @param column_id Column ID to remove
 * @return 0 on success, -1 if not found
 */
int cortical_plasticity_remove_column(
    cortical_plasticity_bridge_t* bridge,
    uint32_t column_id
);

/* ============================================================================
 * STDP API
 * ============================================================================ */

/**
 * @brief Apply STDP to intracolumnar synapses
 *
 * WHAT: Update synaptic weights based on spike timing
 * WHY:  Refine feature selectivity within minicolumns
 * HOW:  Calculate spike timing differences, apply STDP rule
 *
 * BIOLOGICAL: Layer 4→2/3 and 2/3→5/6 connections use STDP for
 *             precise timing-based tuning
 *
 * @param bridge Cortical plasticity bridge
 * @param column_id Column ID
 * @param pre_spike_time Pre-synaptic spike time (ms)
 * @param post_spike_time Post-synaptic spike time (ms)
 * @param synapse_id Synapse identifier
 * @return 0 on success, -1 on error
 */
int cortical_plasticity_apply_stdp(
    cortical_plasticity_bridge_t* bridge,
    uint32_t column_id,
    float pre_spike_time,
    float post_spike_time,
    uint32_t synapse_id
);

/* ============================================================================
 * BCM API
 * ============================================================================ */

/**
 * @brief Update BCM threshold for column
 *
 * WHAT: Slide BCM threshold based on average activity
 * WHY:  Enable competitive learning between minicolumns
 * HOW:  θ(t+1) = θ(t) + (1/τ) * (E[a²] - θ(t))
 *
 * BIOLOGICAL: Competition in hypercolumns for orientation selectivity
 *
 * @param bridge Cortical plasticity bridge
 * @param column_id Column ID
 * @param current_activity Current activation level
 * @param dt Time delta (ms)
 * @return 0 on success, -1 on error
 */
int cortical_plasticity_update_bcm_threshold(
    cortical_plasticity_bridge_t* bridge,
    uint32_t column_id,
    float current_activity,
    float dt
);

/**
 * @brief Get BCM threshold for column
 *
 * @param bridge Cortical plasticity bridge
 * @param column_id Column ID
 * @return BCM threshold or -1.0f on error
 */
float cortical_plasticity_get_bcm_threshold(
    const cortical_plasticity_bridge_t* bridge,
    uint32_t column_id
);

/* ============================================================================
 * Homeostatic Scaling API
 * ============================================================================ */

/**
 * @brief Apply homeostatic scaling to column
 *
 * WHAT: Scale all synaptic weights to maintain target firing rate
 * WHY:  Prevent runaway excitation/depression in columnar network
 * HOW:  Multiply all weights by scaling factor based on rate deviation
 *
 * BIOLOGICAL: Synaptic scaling operates on hours-days timescale to
 *             maintain network stability
 *
 * @param bridge Cortical plasticity bridge
 * @param column_id Column ID
 * @param current_rate Current firing rate (Hz)
 * @param dt Time delta (ms)
 * @return 0 on success, -1 on error
 */
int cortical_plasticity_apply_homeostatic_scaling(
    cortical_plasticity_bridge_t* bridge,
    uint32_t column_id,
    float current_rate,
    float dt
);

/**
 * @brief Get homeostatic scaling factor for column
 *
 * @param bridge Cortical plasticity bridge
 * @param column_id Column ID
 * @return Scaling factor or -1.0f on error
 */
float cortical_plasticity_get_homeostatic_scale(
    const cortical_plasticity_bridge_t* bridge,
    uint32_t column_id
);

/* ============================================================================
 * Critical Period API
 * ============================================================================ */

/**
 * @brief Set critical period state
 *
 * WHAT: Enable/disable critical period plasticity boost
 * WHY:  Developmental windows require enhanced plasticity
 * HOW:  Apply boost factors to STDP, BCM, homeostatic parameters
 *
 * BIOLOGICAL: Critical periods enable rapid reorganization
 *             (ocular dominance, barrel cortex)
 *
 * @param bridge Cortical plasticity bridge
 * @param in_critical_period Whether in critical period
 * @return 0 on success, -1 on error
 */
int cortical_plasticity_set_critical_period(
    cortical_plasticity_bridge_t* bridge,
    bool in_critical_period
);

/**
 * @brief Check if in critical period
 *
 * @param bridge Cortical plasticity bridge
 * @return true if in critical period
 */
bool cortical_plasticity_is_critical_period(
    const cortical_plasticity_bridge_t* bridge
);

/* ============================================================================
 * Eligibility Trace API
 * ============================================================================ */

/**
 * @brief Update eligibility traces for column
 *
 * WHAT: Decay eligibility traces, mark recent spike pairs
 * WHY:  Enable reward-modulated plasticity in cortical circuits
 * HOW:  e(t+1) = e(t) * exp(-dt/τ) + δ(spike_pair)
 *
 * BIOLOGICAL: Eligibility traces bridge spike timing and delayed rewards
 *
 * @param bridge Cortical plasticity bridge
 * @param column_id Column ID
 * @param dt Time delta (ms)
 * @return 0 on success, -1 on error
 */
int cortical_plasticity_update_eligibility(
    cortical_plasticity_bridge_t* bridge,
    uint32_t column_id,
    float dt
);

/**
 * @brief Apply reward signal to eligibility traces
 *
 * WHAT: Convert eligibility traces to weight changes via reward
 * WHY:  Reinforcement learning in cortical columns
 * HOW:  Δw = reward * eligibility
 *
 * @param bridge Cortical plasticity bridge
 * @param column_id Column ID
 * @param reward Reward signal [-1, 1]
 * @return 0 on success, -1 on error
 */
int cortical_plasticity_apply_reward(
    cortical_plasticity_bridge_t* bridge,
    uint32_t column_id,
    float reward
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieve plasticity update counts
 * WHY:  Monitor plasticity activity
 * HOW:  Return copy of statistics structure
 *
 * @param bridge Cortical plasticity bridge
 * @param stdp_updates_out Output: STDP update count
 * @param bcm_updates_out Output: BCM update count
 * @param homeostatic_updates_out Output: Homeostatic update count
 * @param eligibility_updates_out Output: Eligibility update count
 * @return 0 on success, -1 on error
 */
int cortical_plasticity_get_stats(
    const cortical_plasticity_bridge_t* bridge,
    uint64_t* stdp_updates_out,
    uint64_t* bcm_updates_out,
    uint64_t* homeostatic_updates_out,
    uint64_t* eligibility_updates_out
);

/**
 * @brief Get per-column plasticity state
 *
 * @param bridge Cortical plasticity bridge
 * @param column_id Column ID
 * @param state_out Output: column state
 * @return 0 on success, -1 on error
 */
int cortical_plasticity_get_column_state(
    const cortical_plasticity_bridge_t* bridge,
    uint32_t column_id,
    cortical_column_plasticity_state_t* state_out
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async
 * WHY:  Enable inter-module messaging
 * HOW:  Register module with BIO_MODULE_CORTICAL_PLASTICITY
 *
 * @param bridge Cortical plasticity bridge
 * @return 0 on success, -1 on error
 */
int cortical_plasticity_connect_bio_async(
    cortical_plasticity_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async
 *
 * @param bridge Cortical plasticity bridge
 * @return 0 on success
 */
int cortical_plasticity_disconnect_bio_async(
    cortical_plasticity_bridge_t* bridge
);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Cortical plasticity bridge
 * @return true if connected
 */
bool cortical_plasticity_is_bio_async_connected(
    const cortical_plasticity_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORTICAL_PLASTICITY_BRIDGE_H */
