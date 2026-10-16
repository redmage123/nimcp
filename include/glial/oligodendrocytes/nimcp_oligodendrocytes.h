/**
 * @file nimcp_oligodendrocytes.h
 * @brief Enhanced Oligodendrocyte Module - Myelination, Saltatory Conduction & Metabolic Support
 *
 * BIOLOGICAL BASIS:
 * - Oligodendrocytes produce myelin sheaths that wrap around axons
 * - Each oligodendrocyte can myelinate 10-50 axons in the CNS
 * - Myelin increases conduction velocity from 0.5-2 m/s to 50-100 m/s (10-100x faster)
 * - Adaptive myelination: high-activity axons receive more myelin (NRG1/BDNF signaling)
 * - Myelin remodeling occurs over hours to days in response to activity patterns
 * - Myelination is metabolically expensive (ATP cost)
 * - G-ratio optimization: optimal inner/outer diameter ratio = 0.6-0.8
 * - Lactate shuttle: oligodendrocytes provide metabolic support to axons
 * - Saltatory conduction: action potentials jump between nodes of Ranvier
 *
 * MATHEMATICAL ENHANCEMENTS:
 * - KD-tree spatial indexing: O(log n) axon queries vs O(n) linear
 * - RK4 ODE integration: Accurate myelination dynamics
 * - G-ratio optimization: Thermodynamic efficiency maximization
 * - Signal filtering: Low-pass filtered activity for stable decisions
 * - Saltatory conduction: Rushton's law for velocity optimization
 * - Reaction-diffusion: NRG1/BDNF concentration dynamics
 *
 * DESIGN PRINCIPLES (SOLID):
 * - Single Responsibility: Modular components for each function
 * - Open/Closed: Extends neural network without modifying neuron code
 * - Interface Segregation: Focused API for myelination/conduction/metabolic
 * - Dependency Inversion: Uses NIMCP utils abstractions
 *
 * INTEGRATION POINTS:
 * - nimcp_neuralnet.c: Signal propagation delays based on myelination
 * - nimcp_brain.c: Assign oligodendrocytes to axons during construction
 * - nimcp_kdtree.h: Spatial indexing for O(log n) queries
 * - nimcp_integration.h: RK4 ODE solver for myelination dynamics
 * - nimcp_centrality.h: Network importance for myelination priority
 *
 * PERFORMANCE CHARACTERISTICS:
 * - Spatial query: O(log N) with KD-tree
 * - Activity update: O(N) where N = myelinated axons
 * - State dynamics: O(1) per oligodendrocyte (RK4 step)
 * - G-ratio optimization: O(N) per oligodendrocyte
 * - Network step: O(M×N) parallelizable
 *
 * @author NIMCP Development Team
 * @date 2025-11-24
 * @version 2.0.0 (Enhanced with mathematical algorithms)
 */

#ifndef NIMCP_OLIGODENDROCYTES_H
#define NIMCP_OLIGODENDROCYTES_H

#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/spatial/nimcp_kdtree.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// CONSTANTS & BIOLOGICAL PARAMETERS
//=============================================================================

/** @brief Maximum axons one oligodendrocyte can myelinate */
#define NIMCP_OLIGO_MAX_AXONS 50

//-----------------------------------------------------------------------------
// Memory Pool Parameters (Phase 1.5+)
//-----------------------------------------------------------------------------

/** @brief Pool size for myelinated axon structures */
#define NIMCP_OLIGO_AXON_POOL_SIZE 1024

/** @brief Pool size for internode segment structures */
#define NIMCP_OLIGO_INTERNODE_POOL_SIZE 4096

/** @brief Pool block size (64 entries per block for bitmap allocation) */
#define NIMCP_OLIGO_POOL_BLOCK_SIZE 64

/** @brief Baseline unmyelinated conduction velocity (m/s) */
#define NIMCP_OLIGO_BASE_VELOCITY_MS 1.0f

/** @brief Myelin velocity multiplier (fully myelinated) */
#define NIMCP_OLIGO_MYELIN_MULTIPLIER 50.0f

/** @brief Activity threshold for triggering myelination (Hz) */
#define NIMCP_OLIGO_ACTIVITY_THRESHOLD_HZ 1.0f

/** @brief ATP cost per unit myelination per second */
#define NIMCP_OLIGO_ATP_COST_PER_MYELIN 0.01f

/** @brief ATP regeneration rate (per second) */
#define NIMCP_OLIGO_ATP_REGEN_RATE 0.1f

/** @brief Myelination remodeling time constant (seconds)
 *
 * BIOLOGICAL NOTE: Real myelination remodeling occurs over hours to days.
 * We use a faster time constant (1s) for demonstration and testing purposes.
 * In production with real-time simulation, consider using 3600s (1 hour) or longer.
 */
#define NIMCP_OLIGO_REMODEL_TAU_S 1.0f // 1 second (accelerated for demos/tests)

/** @brief Activity history window (number of samples) */
#define NIMCP_OLIGO_ACTIVITY_WINDOW 100

//-----------------------------------------------------------------------------
// G-Ratio Optimization Parameters
//-----------------------------------------------------------------------------

/** @brief Optimal G-ratio (inner/outer diameter) */
#define NIMCP_OLIGO_OPTIMAL_G_RATIO 0.7f

/** @brief G-ratio optimization tolerance */
#define NIMCP_OLIGO_G_RATIO_TOLERANCE 0.1f

/** @brief Minimum G-ratio (heavily myelinated) */
#define NIMCP_OLIGO_G_RATIO_MIN 0.5f

/** @brief Maximum G-ratio (lightly myelinated) */
#define NIMCP_OLIGO_G_RATIO_MAX 0.9f

/** @brief G-ratio optimization time constant (seconds) */
#define NIMCP_OLIGO_G_RATIO_TAU_S 10.0f

//-----------------------------------------------------------------------------
// Internode Spacing Parameters (Saltatory Conduction)
//-----------------------------------------------------------------------------

/** @brief Minimum internode length (µm) */
#define NIMCP_OLIGO_INTERNODE_MIN_UM 100.0f

/** @brief Maximum internode length (µm) */
#define NIMCP_OLIGO_INTERNODE_MAX_UM 1500.0f

/** @brief Optimal internode-to-diameter ratio */
#define NIMCP_OLIGO_INTERNODE_DIAMETER_RATIO 100.0f

/** @brief Node of Ranvier length (µm) */
#define NIMCP_OLIGO_NODE_LENGTH_UM 1.0f

//-----------------------------------------------------------------------------
// NRG1/BDNF Signaling Parameters
//-----------------------------------------------------------------------------

/** @brief Number of growth factor types modeled */
#define NIMCP_GROWTH_FACTOR_COUNT 4

/** @brief Growth factor decay rate (per second) */
#define NIMCP_GROWTH_FACTOR_DECAY_RATE 0.1f

/** @brief Growth factor diffusion coefficient (µm²/s) */
#define NIMCP_GROWTH_FACTOR_DIFFUSION_COEFF 50.0f

/** @brief Maximum growth factor concentration */
#define NIMCP_GROWTH_FACTOR_MAX_CONCENTRATION 10.0f

/** @brief NRG1 myelination promotion coefficient */
#define NIMCP_NRG1_MYELIN_COEFFICIENT 0.5f

/** @brief BDNF myelination promotion coefficient */
#define NIMCP_BDNF_MYELIN_COEFFICIENT 0.3f

//-----------------------------------------------------------------------------
// Lactate Shuttle Parameters
//-----------------------------------------------------------------------------

/** @brief Maximum lactate production rate (mM/s) */
#define NIMCP_OLIGO_LACTATE_MAX_PRODUCTION 1.0f

/** @brief Lactate transfer efficiency */
#define NIMCP_OLIGO_LACTATE_TRANSFER_EFFICIENCY 0.8f

/** @brief Lactate decay rate (per second) */
#define NIMCP_OLIGO_LACTATE_DECAY_RATE 0.2f

/** @brief Critical lactate level for axon support */
#define NIMCP_OLIGO_LACTATE_CRITICAL 0.1f

//-----------------------------------------------------------------------------
// Metabolic Parameters
//-----------------------------------------------------------------------------

/** @brief Maximum ATP capacity */
#define NIMCP_OLIGO_ATP_MAX 1.0f

/** @brief ATP threshold for myelination */
#define NIMCP_OLIGO_ATP_MYELIN_THRESHOLD 0.2f

/** @brief Glucose uptake rate (per second) */
#define NIMCP_OLIGO_GLUCOSE_UPTAKE_RATE 0.15f

/** @brief Myelin synthesis ATP cost factor */
#define NIMCP_OLIGO_MYELIN_SYNTHESIS_COST 0.02f

//-----------------------------------------------------------------------------
// RK4 State Dynamics Parameters
//-----------------------------------------------------------------------------

/** @brief Myelination state time constant (seconds) */
#define NIMCP_OLIGO_STATE_TAU_S 5.0f

/** @brief Activity integration time constant (seconds) */
#define NIMCP_OLIGO_ACTIVITY_TAU_S 2.0f

/** @brief Maximum myelination rate (per second) */
#define NIMCP_OLIGO_MAX_MYELIN_RATE 0.1f

//-----------------------------------------------------------------------------
// Centrality-Based Prioritization Parameters
//-----------------------------------------------------------------------------

/** @brief Centrality priority factor for myelination */
#define NIMCP_OLIGO_CENTRALITY_PRIORITY_FACTOR 2.0f

/** @brief Minimum centrality for priority myelination */
#define NIMCP_OLIGO_CENTRALITY_MIN_PRIORITY 0.1f

//=============================================================================
// ENUMERATIONS
//=============================================================================

/**
 * @brief Myelination state for an axon
 *
 * BIOLOGICAL MODEL:
 * - UNMYELINATED: No myelin sheath, slow conduction
 * - INITIATING: Early myelination, forming wraps
 * - PARTIAL: Incomplete myelination, moderate velocity boost
 * - MATURE: Full myelin sheath, optimal conduction
 * - DEGENERATING: Myelin breakdown (pathological)
 */
typedef enum {
    MYELIN_STATE_UNMYELINATED = 0,   /**< No myelin present */
    MYELIN_STATE_INITIATING = 1,     /**< Beginning myelination process */
    MYELIN_STATE_PARTIAL = 2,        /**< Partial myelination */
    MYELIN_STATE_MATURE = 3,         /**< Fully myelinated */
    MYELIN_STATE_DEGENERATING = 4    /**< Myelin breakdown (demyelination) */
} myelin_state_t;

/**
 * @brief Growth factor types for myelination signaling
 *
 * BIOLOGICAL MODEL:
 * - NRG1: Neuregulin-1, primary myelination signal from neurons
 * - BDNF: Brain-Derived Neurotrophic Factor, activity-dependent signal
 * - IGF1: Insulin-like Growth Factor-1, promotes myelin synthesis
 * - NT3: Neurotrophin-3, supports oligodendrocyte survival
 */
typedef enum {
    GROWTH_FACTOR_NRG1 = 0,   /**< Neuregulin-1 (pro-myelination) */
    GROWTH_FACTOR_BDNF = 1,   /**< BDNF (activity-dependent) */
    GROWTH_FACTOR_IGF1 = 2,   /**< IGF-1 (myelin synthesis) */
    GROWTH_FACTOR_NT3 = 3     /**< NT-3 (survival support) */
} growth_factor_type_t;

/**
 * @brief Oligodendrocyte maturation state
 *
 * BIOLOGICAL MODEL:
 * - OPC: Oligodendrocyte Precursor Cell
 * - PRE_OL: Pre-oligodendrocyte, extending processes
 * - IMMATURE: Immature, beginning myelination
 * - MATURE: Fully mature, maintaining myelin sheaths
 */
typedef enum {
    OLIGO_STATE_OPC = 0,         /**< Oligodendrocyte Precursor Cell */
    OLIGO_STATE_PRE_OL = 1,      /**< Pre-oligodendrocyte */
    OLIGO_STATE_IMMATURE = 2,    /**< Immature oligodendrocyte */
    OLIGO_STATE_MATURE = 3       /**< Mature oligodendrocyte */
} oligo_maturation_state_t;

//=============================================================================
// DATA STRUCTURES
//=============================================================================

/**
 * @brief Growth factor state for myelination signaling
 *
 * Models local growth factor concentrations affecting myelination.
 * Used for activity-dependent and neuronal signaling.
 */
typedef struct {
    float concentrations[NIMCP_GROWTH_FACTOR_COUNT];   /**< Current concentrations */
    float production_rates[NIMCP_GROWTH_FACTOR_COUNT]; /**< Production rates */
    float reception_rates[NIMCP_GROWTH_FACTOR_COUNT];  /**< Reception sensitivity */
    uint64_t last_update_time;                          /**< Last update timestamp */
} oligo_growth_factor_state_t;

/**
 * @brief Lactate shuttle state for metabolic support
 *
 * Models oligodendrocyte-to-axon metabolic support via lactate.
 */
typedef struct {
    float lactate_pool;              /**< Current lactate available (mM) */
    float production_rate;           /**< Lactate production rate (mM/s) */
    float transfer_rate;             /**< Transfer to axons (mM/s) */
    float glucose_uptake;            /**< Glucose uptake rate */
    uint32_t supported_axon_count;   /**< Number of axons receiving support */
    float* axon_lactate_delivery;    /**< Per-axon lactate delivery rates */
} lactate_shuttle_state_t;

/**
 * @brief Internode segment data for saltatory conduction
 *
 * Models a single myelin segment between nodes of Ranvier.
 */
typedef struct {
    float start_position;            /**< Start position along axon (µm) */
    float length;                    /**< Internode length (µm) */
    float myelin_thickness;          /**< Myelin sheath thickness (µm) */
    float g_ratio;                   /**< Inner/outer diameter ratio (0.5-0.9) */
    uint32_t wrap_count;             /**< Number of myelin wraps */
    float compaction;                /**< Myelin compaction (0-1) */
} internode_segment_t;

/**
 * @brief Enhanced myelinated axon data
 *
 * Extended per-axon data including G-ratio, internodes,
 * growth factor sensitivity, and saltatory conduction parameters.
 */
typedef struct {
    uint32_t axon_id;                     /**< Axon/neuron identifier */
    float position[3];                    /**< Axon position (µm) */

    //-------------------------------------------------------------------------
    // Myelination State
    //-------------------------------------------------------------------------
    myelin_state_t myelin_state;          /**< Current myelination state */
    float myelination_level;              /**< Overall myelination 0-1 */
    float target_myelination;             /**< Target based on activity */

    //-------------------------------------------------------------------------
    // G-Ratio Optimization
    //-------------------------------------------------------------------------
    float axon_diameter;                  /**< Axon inner diameter (µm) */
    float fiber_diameter;                 /**< Total fiber diameter (µm) */
    float g_ratio;                        /**< Current G-ratio */
    float optimal_g_ratio;                /**< Optimal G-ratio for this axon */

    //-------------------------------------------------------------------------
    // Internode Segments
    //-------------------------------------------------------------------------
    uint32_t num_internodes;              /**< Number of internode segments */
    uint32_t max_internodes;              /**< Maximum internode capacity */
    internode_segment_t* internodes;      /**< Array of internode segments */
    float total_myelin_length;            /**< Total myelinated length (µm) */
    float axon_length;                    /**< Total axon length (µm) */

    //-------------------------------------------------------------------------
    // Activity Tracking
    //-------------------------------------------------------------------------
    float activity_score;                 /**< Current activity (EMA) */
    float filtered_activity;              /**< Low-pass filtered activity */
    uint64_t last_activity_time;          /**< Last spike timestamp */
    float activity_integral;              /**< Integrated activity over time */

    //-------------------------------------------------------------------------
    // Growth Factor Sensitivity
    //-------------------------------------------------------------------------
    float nrg1_sensitivity;               /**< NRG1 receptor expression (0-1) */
    float bdnf_sensitivity;               /**< BDNF receptor expression (0-1) */

    //-------------------------------------------------------------------------
    // Centrality for Prioritization
    //-------------------------------------------------------------------------
    float centrality_score;               /**< Network importance (0-1) */
    bool priority_myelination;            /**< High-priority flag */

    //-------------------------------------------------------------------------
    // Conduction Properties
    //-------------------------------------------------------------------------
    float conduction_velocity;            /**< Current conduction velocity (m/s) */
    float conduction_delay;               /**< Signal propagation delay (ms) */

    //-------------------------------------------------------------------------
    // Metabolic Support
    //-------------------------------------------------------------------------
    float lactate_received;               /**< Lactate from oligodendrocyte (mM) */
    float metabolic_demand;               /**< Axon metabolic demand */

    //-------------------------------------------------------------------------
    // Enhanced Biophysics (from nimcp_myelin_math.h)
    //-------------------------------------------------------------------------
    nimcp_cable_params_t cable_params;    /**< Cable theory parameters */
    nimcp_saltatory_result_t saltatory;   /**< Saltatory conduction result */
    float optimal_g_ratio_math;           /**< Diameter-dependent optimal g-ratio */
    float optimal_internode_um;           /**< Optimal internode length */
    float block_probability;              /**< Conduction block probability (0-1) */
    bool is_conducting;                   /**< True if signal can propagate */
    nimcp_metabolic_efficiency_t metabolic; /**< Energy efficiency metrics */
} myelinated_axon_t;

//-----------------------------------------------------------------------------
// Memory Pool Structures (Phase 1.5+)
//-----------------------------------------------------------------------------

/**
 * @brief Memory pool for myelinated axon structures
 *
 * Provides O(1) allocation for myelinated_axon_t using bitmap-based tracking.
 * Reduces malloc/free overhead in hot paths during myelination operations.
 */
typedef struct {
    myelinated_axon_t* buffer;           /**< Pre-allocated axon array */
    uint64_t* bitmap;                    /**< Bitmap for free/allocated tracking (1=free) */
    uint32_t capacity;                   /**< Total slots in pool */
    uint32_t num_bitmap_words;           /**< Number of 64-bit bitmap words */
    uint32_t allocated_count;            /**< Number of currently allocated slots */
    nimcp_spinlock_t lock;               /**< Thread-safe access */
} oligo_axon_pool_t;

/**
 * @brief Memory pool for internode segment structures
 *
 * Provides O(1) allocation for internode_segment_t using bitmap-based tracking.
 * Each myelinated axon can have multiple internode segments.
 */
typedef struct {
    internode_segment_t* buffer;         /**< Pre-allocated internode array */
    uint64_t* bitmap;                    /**< Bitmap for free/allocated tracking (1=free) */
    uint32_t capacity;                   /**< Total slots in pool */
    uint32_t num_bitmap_words;           /**< Number of 64-bit bitmap words */
    uint32_t allocated_count;            /**< Number of currently allocated slots */
    nimcp_spinlock_t lock;               /**< Thread-safe access */
} oligo_internode_pool_t;

/**
 * @brief Oligodendrocyte cell state (Enhanced)
 *
 * Models a single oligodendrocyte with full biological accuracy:
 * - State dynamics via RK4 ODE integration
 * - NRG1/BDNF growth factor signaling
 * - G-ratio optimization for each axon
 * - Lactate shuttle metabolic support
 * - KD-tree indexed axon management
 * - Saltatory conduction modeling
 */
typedef struct oligodendrocyte {
    uint32_t id;                          /**< Unique oligodendrocyte ID */

    //-------------------------------------------------------------------------
    // Spatial Properties
    //-------------------------------------------------------------------------
    float position[3];                    /**< x, y, z coordinates (µm) */
    float process_reach;                  /**< Maximum process extension (µm) */
    float territory_radius;               /**< Myelination territory radius (µm) */

    //-------------------------------------------------------------------------
    // Maturation State
    //-------------------------------------------------------------------------
    oligo_maturation_state_t maturation;  /**< Current maturation state */
    float maturation_progress;            /**< Progress to next state (0-1) */
    uint64_t maturation_time;             /**< Time at current state (µs) */

    //-------------------------------------------------------------------------
    // State Dynamics (RK4 ODE)
    //-------------------------------------------------------------------------
    float state_variables[4];             /**< ODE state: [myelin_rate, activity, energy, maturation] */
    float myelination_rate;               /**< Current myelination rate */

    //-------------------------------------------------------------------------
    // Growth Factor Signaling
    //-------------------------------------------------------------------------
    oligo_growth_factor_state_t growth_factors;  /**< Growth factor state */

    //-------------------------------------------------------------------------
    // Myelinated Axons (Enhanced)
    //-------------------------------------------------------------------------
    uint32_t num_myelinated_axons;        /**< Number of axons myelinated */
    uint32_t max_axons;                   /**< Capacity (typically 10-50) */
    myelinated_axon_t* axons;             /**< Enhanced axon data array */

    //-------------------------------------------------------------------------
    // Legacy Arrays (For backward compatibility)
    //-------------------------------------------------------------------------
    uint32_t* myelinated_neuron_ids;      /**< Array of neuron IDs */
    float* myelination_levels;            /**< Per-neuron myelination 0-1 */
    float* neuron_activity_history;       /**< Rolling average activity */
    uint64_t* last_spike_times;           /**< Last spike timestamp */

    //-------------------------------------------------------------------------
    // Metabolic State
    //-------------------------------------------------------------------------
    float atp_level;                      /**< Energy available (0-1) */
    float metabolic_cost;                 /**< Current ATP consumption rate */
    float max_myelination_capacity;       /**< Total myelin capacity */
    float glucose_level;                  /**< Glucose availability */

    //-------------------------------------------------------------------------
    // Lactate Shuttle
    //-------------------------------------------------------------------------
    lactate_shuttle_state_t lactate_shuttle;  /**< Metabolic support state */

    //-------------------------------------------------------------------------
    // Remodeling State
    //-------------------------------------------------------------------------
    uint64_t last_remodeling_time;        /**< When last adjusted myelination (µs) */
    float remodeling_interval_ms;         /**< How often to remodel */
    float g_ratio_optimization_rate;      /**< G-ratio adjustment rate */

    //-------------------------------------------------------------------------
    // Statistics
    //-------------------------------------------------------------------------
    uint32_t total_myelin_segments;       /**< Total internode segments produced */
    float total_myelin_volume;            /**< Total myelin volume (µm³) */
    float avg_g_ratio;                    /**< Average G-ratio across axons */
    float avg_conduction_velocity;        /**< Average conduction velocity (m/s) */
    uint32_t demyelination_events;        /**< Count of demyelination events */
    float total_lactate_delivered;        /**< Cumulative lactate delivered */

    //-------------------------------------------------------------------------
    // Thread Safety
    //-------------------------------------------------------------------------
    nimcp_spinlock_t lock;                /**< Lock for concurrent access */

    //-------------------------------------------------------------------------
    // Enhanced Biophysics (from nimcp_myelin_math.h)
    //-------------------------------------------------------------------------
    nimcp_myelin_biophysics_t* biophysics; /**< Comprehensive biophysics state */
    nimcp_myelination_kinetics_t kinetics; /**< Myelination kinetics parameters */
    float current_temperature_c;           /**< Temperature for block modeling (°C) */

    //-------------------------------------------------------------------------
    // Copy-on-Write Support (Phase 1.5+)
    //-------------------------------------------------------------------------
    uint32_t cow_ref_count;               /**< Reference count for CoW */
    bool cow_modified;                    /**< True if modified since copy */
    void* cow_original;                   /**< Pointer to original if this is a copy */
} oligodendrocyte_t;

/**
 * @brief Network of oligodendrocytes (Enhanced)
 *
 * Manages multiple oligodendrocytes with:
 * - KD-tree spatial indexing for O(log n) queries
 * - Network-wide growth factor diffusion
 * - Centrality computation integration
 * - Global myelination statistics
 */
typedef struct {
    uint32_t num_oligodendrocytes;        /**< Current number of oligodendrocytes */
    uint32_t capacity;                    /**< Max oligodendrocytes */
    oligodendrocyte_t** oligodendrocytes; /**< Array of oligodendrocyte pointers */

    //-------------------------------------------------------------------------
    // Spatial Indexing
    //-------------------------------------------------------------------------
    kdtree_t* oligo_tree;                 /**< KD-tree for oligodendrocyte positions */
    kdtree_t* axon_tree;                  /**< KD-tree for axon positions */
    bool spatial_index_valid;             /**< True if KD-trees are up to date */

    //-------------------------------------------------------------------------
    // Global Growth Factor Field
    //-------------------------------------------------------------------------
    float* global_growth_factor_field;    /**< Network-wide growth factor concentrations */
    uint32_t growth_factor_field_size;    /**< Size of growth factor field */

    //-------------------------------------------------------------------------
    // Centrality Scores (Precomputed)
    //-------------------------------------------------------------------------
    float* axon_centrality;               /**< Per-axon centrality scores */
    uint32_t num_centrality_scores;       /**< Number of centrality scores */
    bool centrality_valid;                /**< True if centrality is up to date */

    //-------------------------------------------------------------------------
    // Global Parameters
    //-------------------------------------------------------------------------
    float base_conduction_velocity;       /**< Unmyelinated velocity (m/s) */
    float myelinated_velocity_multiplier; /**< Myelin boost factor (10-100x) */
    float activity_threshold;             /**< Activity level to trigger myelination (Hz) */
    float global_g_ratio_target;          /**< Target G-ratio for network */

    //-------------------------------------------------------------------------
    // Activity Filter State
    //-------------------------------------------------------------------------
    float filter_cutoff_hz;               /**< Low-pass filter cutoff (Hz) */
    float filter_alpha;                   /**< Filter smoothing coefficient */

    //-------------------------------------------------------------------------
    // Thread Safety
    //-------------------------------------------------------------------------
    nimcp_mutex_t lock;                   /**< Network-level lock */

    //-------------------------------------------------------------------------
    // Memory Pools (Phase 1.5+)
    //-------------------------------------------------------------------------
    oligo_axon_pool_t* axon_pool;         /**< Shared pool for myelinated axon data */
    oligo_internode_pool_t* internode_pool; /**< Shared pool for internode segments */
} oligodendrocyte_network_t;

/**
 * @brief Configuration for oligodendrocyte network
 */
typedef struct {
    uint32_t capacity;                    /**< Maximum oligodendrocyte count */
    uint32_t max_axons_per_oligo;         /**< Max axons per oligodendrocyte */
    float activity_threshold;             /**< Activity threshold for myelination */
    float territory_radius;               /**< Default territory radius */
    float target_g_ratio;                 /**< Target G-ratio */
    bool enable_g_ratio_optimization;     /**< Use G-ratio optimization */
    bool enable_growth_factor_signaling;  /**< Use NRG1/BDNF signaling */
    bool enable_lactate_shuttle;          /**< Use lactate metabolic support */
    bool enable_state_dynamics;           /**< Use RK4 state transitions */
    bool enable_centrality_priority;      /**< Use centrality for prioritization */
    float filter_cutoff_hz;               /**< Activity filter cutoff */
} oligodendrocyte_network_config_t;

/**
 * @brief Statistics for oligodendrocyte network
 */
typedef struct {
    uint32_t total_oligodendrocytes;
    uint32_t total_myelinated_axons;
    uint32_t total_internode_segments;
    float total_myelin_volume;
    float avg_myelination_level;
    float avg_g_ratio;
    float avg_conduction_velocity;
    float min_conduction_velocity;
    float max_conduction_velocity;
    uint32_t opc_count;
    uint32_t pre_ol_count;
    uint32_t immature_count;
    uint32_t mature_count;
    float total_nrg1;
    float total_bdnf;
    float total_lactate_delivered;
    float network_myelination_efficiency;
} oligodendrocyte_network_stats_t;

//=============================================================================
// CREATION & DESTRUCTION
//=============================================================================

/**
 * @brief Create a new enhanced oligodendrocyte
 *
 * @param id Unique identifier
 * @param x X coordinate (µm)
 * @param y Y coordinate (µm)
 * @param z Z coordinate (µm)
 * @param max_axons Maximum axons to myelinate (typically 10-50)
 *
 * @return Pointer to oligodendrocyte or NULL on failure
 *
 * INITIAL STATE:
 * - Maturation: OPC (precursor cell)
 * - No axons assigned
 * - ATP level = 1.0 (fully energized)
 * - All myelination levels = 0.0
 * - Growth factors at baseline
 */
oligodendrocyte_t* oligodendrocyte_create(uint32_t id, float x, float y, float z,
                                           uint32_t max_axons);

/**
 * @brief Create oligodendrocyte with basic parameters (backward compatible)
 *
 * @param id Unique identifier
 * @param max_axons Maximum axons to myelinate
 *
 * @return Pointer to oligodendrocyte or NULL on failure
 */
oligodendrocyte_t* oligodendrocyte_create_basic(uint32_t id, uint32_t max_axons);

/**
 * @brief Destroy oligodendrocyte and free resources
 *
 * @param oligo Oligodendrocyte to destroy (NULL safe)
 */
void oligodendrocyte_destroy(oligodendrocyte_t* oligo);

/**
 * @brief Get default network configuration
 *
 * @return Default configuration with all features enabled
 */
oligodendrocyte_network_config_t oligodendrocyte_network_default_config(void);

/**
 * @brief Create enhanced oligodendrocyte network
 *
 * @param config Configuration parameters
 *
 * @return Network handle or NULL on failure
 */
oligodendrocyte_network_t* oligodendrocyte_network_create_enhanced(
    const oligodendrocyte_network_config_t* config);

/**
 * @brief Create basic oligodendrocyte network (backward compatible)
 *
 * @param capacity Maximum number of oligodendrocytes
 *
 * @return Network handle or NULL on failure
 */
oligodendrocyte_network_t* oligodendrocyte_network_create(uint32_t capacity);

/**
 * @brief Destroy oligodendrocyte network
 *
 * @param network Network to destroy (NULL safe)
 */
void oligodendrocyte_network_destroy(oligodendrocyte_network_t* network);

//=============================================================================
// AXON ASSIGNMENT & MYELINATION
//=============================================================================

/**
 * @brief Assign an axon to this oligodendrocyte with position
 *
 * @param oligo Oligodendrocyte
 * @param axon_id ID of axon to myelinate
 * @param x Axon X position (µm)
 * @param y Axon Y position (µm)
 * @param z Axon Z position (µm)
 * @param axon_diameter Axon diameter (µm)
 * @param axon_length Axon length (µm)
 *
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t oligodendrocyte_assign_axon_at(oligodendrocyte_t* oligo,
                                               uint32_t axon_id,
                                               float x, float y, float z,
                                               float axon_diameter,
                                               float axon_length);

/**
 * @brief Assign a neuron/axon to this oligodendrocyte (legacy)
 *
 * @param oligo Oligodendrocyte
 * @param neuron_id ID of neuron/axon to myelinate
 *
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t oligodendrocyte_assign_neuron(oligodendrocyte_t* oligo, uint32_t neuron_id);

/**
 * @brief Get current myelination level for an axon
 *
 * @param oligo Oligodendrocyte
 * @param axon_id ID of axon to query
 *
 * @return Myelination level 0-1, or 0.0 if not found
 */
float oligodendrocyte_get_myelination_level(oligodendrocyte_t* oligo, uint32_t axon_id);

/**
 * @brief Set myelination level for an axon
 *
 * @param oligo Oligodendrocyte
 * @param axon_id ID of axon
 * @param level Myelination level (0-1)
 *
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t oligodendrocyte_set_myelination_level(oligodendrocyte_t* oligo,
                                                      uint32_t axon_id,
                                                      float level);

/**
 * @brief Get myelin state for an axon
 *
 * @param oligo Oligodendrocyte
 * @param axon_id ID of axon
 *
 * @return Myelin state
 */
myelin_state_t oligodendrocyte_get_myelin_state(oligodendrocyte_t* oligo, uint32_t axon_id);

//=============================================================================
// G-RATIO OPTIMIZATION
//=============================================================================

/**
 * @brief Get G-ratio for an axon
 *
 * @param oligo Oligodendrocyte
 * @param axon_id ID of axon
 *
 * @return G-ratio (0.5-0.9), or -1 if not found
 *
 * BIOLOGICAL NOTE:
 * G-ratio = axon_diameter / fiber_diameter
 * Optimal G-ratio for CNS axons is 0.6-0.8
 */
float oligodendrocyte_get_g_ratio(oligodendrocyte_t* oligo, uint32_t axon_id);

/**
 * @brief Compute optimal G-ratio for an axon
 *
 * @param axon_diameter Axon inner diameter (µm)
 * @param activity_level Activity level (Hz)
 *
 * @return Optimal G-ratio
 *
 * FORMULA (Rushton's optimization):
 * G_opt = 0.6 for small axons, 0.77 for large axons
 * Activity increases optimal G-ratio (thinner myelin for faster remodeling)
 */
float oligodendrocyte_compute_optimal_g_ratio(float axon_diameter, float activity_level);

/**
 * @brief Optimize G-ratios for all axons
 *
 * @param oligo Oligodendrocyte
 * @param dt Time step (seconds)
 *
 * ALGORITHM:
 * 1. Compute optimal G-ratio for each axon based on diameter and activity
 * 2. Adjust current G-ratio toward optimal using first-order dynamics
 * 3. Rate-limited by ATP availability and time constant
 */
void oligodendrocyte_optimize_g_ratios(oligodendrocyte_t* oligo, float dt);

/**
 * @brief Get average G-ratio deviation from optimal
 *
 * @param oligo Oligodendrocyte
 *
 * @return Mean absolute deviation from optimal G-ratio
 */
float oligodendrocyte_get_g_ratio_deviation(oligodendrocyte_t* oligo);

//=============================================================================
// SALTATORY CONDUCTION
//=============================================================================

/**
 * @brief Compute conduction velocity for an axon
 *
 * @param oligo Oligodendrocyte
 * @param axon_id ID of axon
 * @param base_velocity Unmyelinated conduction velocity (m/s)
 *
 * @return Effective velocity with myelin (m/s)
 *
 * FORMULA (Rushton's law for saltatory conduction):
 * v = k × d × G-ratio factor
 * where k ≈ 5.5 m/s per µm for myelinated axons
 *
 * FACTORS:
 * - Axon diameter (larger = faster)
 * - Myelin thickness (optimal G-ratio = fastest)
 * - Internode spacing (longer = fewer delays)
 */
float oligodendrocyte_compute_conduction_velocity(oligodendrocyte_t* oligo,
                                                   uint32_t axon_id,
                                                   float base_velocity);

/**
 * @brief Compute saltatory conduction velocity using detailed model
 *
 * @param axon Myelinated axon data
 *
 * @return Conduction velocity (m/s)
 *
 * MODEL:
 * Uses full saltatory conduction model with:
 * - Node of Ranvier delays
 * - Internode capacitance
 * - G-ratio efficiency factor
 */
float oligodendrocyte_compute_saltatory_velocity(const myelinated_axon_t* axon);

/**
 * @brief Compute signal propagation delay
 *
 * @param oligo Oligodendrocyte
 * @param axon_id ID of axon
 *
 * @return Propagation delay (ms)
 */
float oligodendrocyte_compute_propagation_delay(oligodendrocyte_t* oligo, uint32_t axon_id);

/**
 * @brief Optimize internode spacing for conduction
 *
 * @param oligo Oligodendrocyte
 * @param axon_id ID of axon
 *
 * ALGORITHM:
 * Optimal internode length ≈ 100 × axon diameter
 * Adjusts existing internodes toward optimal spacing
 */
void oligodendrocyte_optimize_internode_spacing(oligodendrocyte_t* oligo, uint32_t axon_id);

//=============================================================================
// ACTIVITY TRACKING & ADAPTIVE MYELINATION
//=============================================================================

/**
 * @brief Track activity for an axon
 *
 * @param oligo Oligodendrocyte
 * @param axon_id ID of active axon
 * @param activity Activity level (e.g., spike count, firing rate)
 * @param timestamp Timestamp of this activity sample (µs)
 */
void oligodendrocyte_track_activity(oligodendrocyte_t* oligo,
                                     uint32_t axon_id,
                                     float activity,
                                     uint64_t timestamp);

/**
 * @brief Update all activity scores (decay + filtering)
 *
 * @param oligo Oligodendrocyte
 * @param current_time Current timestamp (µs)
 */
void oligodendrocyte_update_activity_scores(oligodendrocyte_t* oligo, uint64_t current_time);

/**
 * @brief Remodel myelination based on activity history
 *
 * @param oligo Oligodendrocyte
 * @param dt Time step (seconds)
 *
 * ADAPTIVE MYELINATION ALGORITHM:
 * 1. Compute target myelination for each axon based on:
 *    - Activity level (high activity → more myelin)
 *    - NRG1/BDNF concentrations (growth factor signals)
 *    - Centrality score (important axons prioritized)
 * 2. Adjust myelination toward target using RK4 dynamics
 * 3. Optimize G-ratio for each axon
 * 4. Rate-limited by ATP availability
 */
void oligodendrocyte_remodel_myelination(oligodendrocyte_t* oligo, float dt);

/**
 * @brief Set centrality score for an axon (prioritization)
 *
 * @param oligo Oligodendrocyte
 * @param axon_id ID of axon
 * @param centrality Centrality score (0-1)
 */
void oligodendrocyte_set_axon_centrality(oligodendrocyte_t* oligo,
                                          uint32_t axon_id,
                                          float centrality);

//=============================================================================
// NRG1/BDNF GROWTH FACTOR SIGNALING
//=============================================================================

/**
 * @brief Update growth factor concentrations
 *
 * @param oligo Oligodendrocyte
 * @param dt Time step (seconds)
 *
 * DYNAMICS:
 * - Production based on local activity
 * - Decay over time
 * - Diffusion handled at network level
 */
void oligodendrocyte_update_growth_factors(oligodendrocyte_t* oligo, float dt);

/**
 * @brief Get growth factor concentration
 *
 * @param oligo Oligodendrocyte
 * @param type Growth factor type
 *
 * @return Concentration (0 - MAX_CONCENTRATION)
 */
float oligodendrocyte_get_growth_factor(const oligodendrocyte_t* oligo,
                                         growth_factor_type_t type);

/**
 * @brief Add external growth factor input (from neurons)
 *
 * @param oligo Oligodendrocyte
 * @param type Growth factor type
 * @param amount Amount to add
 */
void oligodendrocyte_add_growth_factor(oligodendrocyte_t* oligo,
                                        growth_factor_type_t type,
                                        float amount);

/**
 * @brief Compute myelination signal from growth factors
 *
 * @param oligo Oligodendrocyte
 * @param axon_id ID of axon
 *
 * @return Myelination signal strength (0-1)
 *
 * FORMULA:
 * signal = NRG1 × nrg1_coeff + BDNF × bdnf_coeff + IGF1 × igf1_coeff
 */
float oligodendrocyte_compute_myelin_signal(const oligodendrocyte_t* oligo, uint32_t axon_id);

//=============================================================================
// LACTATE SHUTTLE (METABOLIC SUPPORT)
//=============================================================================

/**
 * @brief Update lactate shuttle state
 *
 * @param oligo Oligodendrocyte
 * @param dt Time step (seconds)
 *
 * DYNAMICS:
 * - Produce lactate from glucose
 * - Transfer lactate to myelinated axons
 * - Distribution based on axon activity and demand
 */
void oligodendrocyte_update_lactate_shuttle(oligodendrocyte_t* oligo, float dt);

/**
 * @brief Get lactate delivered to an axon
 *
 * @param oligo Oligodendrocyte
 * @param axon_id ID of axon
 *
 * @return Lactate delivered (mM)
 */
float oligodendrocyte_get_axon_lactate(const oligodendrocyte_t* oligo, uint32_t axon_id);

/**
 * @brief Set axon metabolic demand
 *
 * @param oligo Oligodendrocyte
 * @param axon_id ID of axon
 * @param demand Metabolic demand (arbitrary units)
 */
void oligodendrocyte_set_axon_demand(oligodendrocyte_t* oligo,
                                      uint32_t axon_id,
                                      float demand);

/**
 * @brief Check if axon is receiving adequate metabolic support
 *
 * @param oligo Oligodendrocyte
 * @param axon_id ID of axon
 *
 * @return true if lactate delivery >= critical level
 */
bool oligodendrocyte_axon_metabolically_supported(const oligodendrocyte_t* oligo,
                                                   uint32_t axon_id);

//=============================================================================
// STATE DYNAMICS (RK4 ODE)
//=============================================================================

/**
 * @brief Update oligodendrocyte state using RK4 integration
 *
 * @param oligo Oligodendrocyte
 * @param dt Time step (seconds)
 *
 * STATE VARIABLES:
 * - state[0]: Myelination rate (driven by signals)
 * - state[1]: Activity integration (EMA of axon activity)
 * - state[2]: Energy state (ATP dynamics)
 * - state[3]: Maturation progress (OPC → mature)
 */
void oligodendrocyte_update_state_dynamics(oligodendrocyte_t* oligo, float dt);

/**
 * @brief Get current maturation state
 *
 * @param oligo Oligodendrocyte
 *
 * @return Current maturation state
 */
oligo_maturation_state_t oligodendrocyte_get_maturation(const oligodendrocyte_t* oligo);

/**
 * @brief Advance maturation state
 *
 * @param oligo Oligodendrocyte
 *
 * @return true if state transitioned
 */
bool oligodendrocyte_advance_maturation(oligodendrocyte_t* oligo);

//=============================================================================
// METABOLIC MANAGEMENT
//=============================================================================

/**
 * @brief Update ATP level based on metabolic activity
 *
 * @param oligo Oligodendrocyte
 * @param dt Time step (seconds)
 *
 * DYNAMICS:
 * - ATP depleted by myelin synthesis and maintenance
 * - ATP regenerated from glucose
 * - Low ATP limits myelination rate
 */
void oligodendrocyte_update_atp(oligodendrocyte_t* oligo, float dt);

/**
 * @brief Get current ATP level
 *
 * @param oligo Oligodendrocyte
 *
 * @return ATP level (0-1)
 */
float oligodendrocyte_get_atp_level(const oligodendrocyte_t* oligo);

/**
 * @brief Add glucose to oligodendrocyte
 *
 * @param oligo Oligodendrocyte
 * @param amount Glucose amount
 */
void oligodendrocyte_add_glucose(oligodendrocyte_t* oligo, float amount);

//=============================================================================
// NETWORK OPERATIONS
//=============================================================================

/**
 * @brief Add oligodendrocyte to network
 *
 * @param network Network
 * @param oligo Oligodendrocyte to add
 *
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t oligodendrocyte_network_add(oligodendrocyte_network_t* network,
                                            oligodendrocyte_t* oligo);

/**
 * @brief Rebuild spatial index (KD-trees)
 *
 * @param network Network
 */
void oligodendrocyte_network_rebuild_spatial_index(oligodendrocyte_network_t* network);

/**
 * @brief Update centrality scores for all axons
 *
 * @param network Network
 * @param axon_graph Graph representation of axonal connections
 */
void oligodendrocyte_network_update_centrality(oligodendrocyte_network_t* network,
                                                 void* axon_graph);

/**
 * @brief Step network forward (full simulation step)
 *
 * @param network Network
 * @param dt Time step (seconds)
 *
 * OPERATIONS:
 * 1. Update state dynamics (RK4) for all oligodendrocytes
 * 2. Update growth factor concentrations
 * 3. Diffuse growth factors between nearby oligodendrocytes
 * 4. Update activity scores
 * 5. Remodel myelination
 * 6. Optimize G-ratios
 * 7. Update lactate shuttle
 * 8. Update ATP levels
 */
void oligodendrocyte_network_step(oligodendrocyte_network_t* network, float dt);

/**
 * @brief Find oligodendrocyte myelinating an axon
 *
 * @param network Network
 * @param axon_id Axon identifier
 *
 * @return Oligodendrocyte pointer or NULL
 */
oligodendrocyte_t* oligodendrocyte_network_find_by_axon(oligodendrocyte_network_t* network,
                                                         uint32_t axon_id);

/**
 * @brief Find oligodendrocyte by neuron ID (legacy)
 *
 * @param network Network
 * @param neuron_id Neuron identifier
 *
 * @return Oligodendrocyte pointer or NULL
 */
oligodendrocyte_t* oligodendrocyte_network_find_by_neuron(oligodendrocyte_network_t* network,
                                                           uint32_t neuron_id);

/**
 * @brief Find nearest oligodendrocyte to a position
 *
 * @param network Network
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 *
 * @return Nearest oligodendrocyte or NULL
 *
 * COMPLEXITY: O(log N) with KD-tree
 */
oligodendrocyte_t* oligodendrocyte_network_find_nearest(oligodendrocyte_network_t* network,
                                                         float x, float y, float z);

/**
 * @brief Find all oligodendrocytes within radius
 *
 * @param network Network
 * @param x Center X
 * @param y Center Y
 * @param z Center Z
 * @param radius Search radius
 * @param results Output buffer
 * @param max_results Buffer size
 *
 * @return Number of oligodendrocytes found
 */
uint32_t oligodendrocyte_network_find_in_radius(oligodendrocyte_network_t* network,
                                                  float x, float y, float z,
                                                  float radius,
                                                  oligodendrocyte_t** results,
                                                  uint32_t max_results);

/**
 * @brief Diffuse growth factors between nearby oligodendrocytes
 *
 * @param network Network
 * @param dt Time step (seconds)
 */
void oligodendrocyte_network_diffuse_growth_factors(oligodendrocyte_network_t* network,
                                                      float dt);

/**
 * @brief Get network statistics
 *
 * @param network Network
 * @param stats Output statistics
 */
void oligodendrocyte_network_get_stats(const oligodendrocyte_network_t* network,
                                         oligodendrocyte_network_stats_t* stats);

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/**
 * @brief Get total myelination for this oligodendrocyte
 *
 * @param oligo Oligodendrocyte
 *
 * @return Sum of myelination levels across all axons
 */
float oligodendrocyte_get_total_myelination(oligodendrocyte_t* oligo);

/**
 * @brief Get average conduction velocity
 *
 * @param oligo Oligodendrocyte
 *
 * @return Average conduction velocity (m/s)
 */
float oligodendrocyte_get_avg_conduction_velocity(oligodendrocyte_t* oligo);

/**
 * @brief Get maturation state name as string
 *
 * @param state Maturation state
 *
 * @return State name string
 */
const char* oligo_maturation_state_to_string(oligo_maturation_state_t state);

/**
 * @brief Get myelin state name as string
 *
 * @param state Myelin state
 *
 * @return State name string
 */
const char* myelin_state_to_string(myelin_state_t state);

/**
 * @brief Get growth factor name as string
 *
 * @param type Growth factor type
 *
 * @return Growth factor name string
 */
const char* growth_factor_type_to_string(growth_factor_type_t type);

//=============================================================================
// MEMORY POOL OPERATIONS (Phase 1.5+)
//=============================================================================

/**
 * @brief Create axon memory pool
 *
 * @param capacity Maximum number of axon slots (rounds up to nearest 64)
 *
 * @return Pool handle or NULL on failure
 */
oligo_axon_pool_t* oligo_axon_pool_create(uint32_t capacity);

/**
 * @brief Destroy axon memory pool
 *
 * @param pool Pool to destroy (NULL safe)
 */
void oligo_axon_pool_destroy(oligo_axon_pool_t* pool);

/**
 * @brief Allocate axon from pool (O(1) bitmap allocation)
 *
 * @param pool Memory pool
 *
 * @return Pointer to axon or NULL if pool exhausted
 */
myelinated_axon_t* oligo_axon_pool_alloc(oligo_axon_pool_t* pool);

/**
 * @brief Return axon to pool
 *
 * @param pool Memory pool
 * @param axon Axon to return
 */
void oligo_axon_pool_free(oligo_axon_pool_t* pool, myelinated_axon_t* axon);

/**
 * @brief Get pool statistics
 *
 * @param pool Memory pool
 * @param allocated Output: number of allocated slots
 * @param capacity Output: total capacity
 */
void oligo_axon_pool_stats(const oligo_axon_pool_t* pool,
                           uint32_t* allocated, uint32_t* capacity);

/**
 * @brief Create internode memory pool
 *
 * @param capacity Maximum number of internode slots (rounds up to nearest 64)
 *
 * @return Pool handle or NULL on failure
 */
oligo_internode_pool_t* oligo_internode_pool_create(uint32_t capacity);

/**
 * @brief Destroy internode memory pool
 *
 * @param pool Pool to destroy (NULL safe)
 */
void oligo_internode_pool_destroy(oligo_internode_pool_t* pool);

/**
 * @brief Allocate internode from pool (O(1) bitmap allocation)
 *
 * @param pool Memory pool
 *
 * @return Pointer to internode or NULL if pool exhausted
 */
internode_segment_t* oligo_internode_pool_alloc(oligo_internode_pool_t* pool);

/**
 * @brief Return internode to pool
 *
 * @param pool Memory pool
 * @param internode Internode to return
 */
void oligo_internode_pool_free(oligo_internode_pool_t* pool, internode_segment_t* internode);

/**
 * @brief Get internode pool statistics
 *
 * @param pool Memory pool
 * @param allocated Output: number of allocated slots
 * @param capacity Output: total capacity
 */
void oligo_internode_pool_stats(const oligo_internode_pool_t* pool,
                                uint32_t* allocated, uint32_t* capacity);

//=============================================================================
// COPY-ON-WRITE OPERATIONS (Phase 1.5+)
//=============================================================================

/**
 * @brief Create a copy-on-write reference to an oligodendrocyte
 *
 * @param oligo Original oligodendrocyte
 *
 * @return CoW reference (shares data until modified)
 */
oligodendrocyte_t* oligodendrocyte_cow_copy(oligodendrocyte_t* oligo);

/**
 * @brief Prepare oligodendrocyte for writing (deep copy if needed)
 *
 * @param oligo Oligodendrocyte to prepare
 *
 * @return NIMCP_SUCCESS if ready for writing
 */
nimcp_result_t oligodendrocyte_cow_prepare_write(oligodendrocyte_t* oligo);

/**
 * @brief Release CoW reference
 *
 * @param oligo Oligodendrocyte to release
 */
void oligodendrocyte_cow_release(oligodendrocyte_t* oligo);

/**
 * @brief Check if oligodendrocyte is a CoW copy
 *
 * @param oligo Oligodendrocyte to check
 *
 * @return true if this is a CoW copy of another oligodendrocyte
 */
bool oligodendrocyte_is_cow_copy(const oligodendrocyte_t* oligo);

//=============================================================================
// Bio-Async Integration API
//=============================================================================

/**
 * @brief Register oligodendrocyte bio-async message handlers
 *
 * WHAT: Registers handlers for myelination, metabolic, and sync messages
 * WHY:  Enables event-driven oligodendrocyte coordination via bio-async messaging
 * HOW:  Calls internal bio_init which registers with bio-router
 *
 * This function should be called during glial system initialization.
 * It registers handlers for:
 * - BIO_MSG_OLIGODENDROCYTE_MYELINATE
 * - BIO_MSG_METABOLIC_DEMAND
 * - BIO_MSG_METABOLIC_SUPPLY
 * - BIO_MSG_GLIAL_SYNC_REQUEST
 *
 * @return NIMCP_SUCCESS or error code
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (uses internal mutex)
 */
nimcp_result_t oligodendrocyte_register_bio_handlers(void);

/**
 * @brief Unregister oligodendrocyte bio-async handlers and cleanup
 *
 * WHAT: Unregisters from bio-router and frees bio-async resources
 * WHY:  Clean shutdown of bio-async integration
 * HOW:  Calls internal bio_shutdown
 *
 * Should be called during glial system shutdown.
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void oligodendrocyte_unregister_bio_handlers(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_OLIGODENDROCYTES_H
