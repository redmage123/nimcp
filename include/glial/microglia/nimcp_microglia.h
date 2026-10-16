/**
 * @file nimcp_microglia.h
 * @brief Enhanced Microglia Module - Synaptic Surveillance, Pruning & Immune Response
 *
 * BIOLOGICAL BASIS:
 * - Microglia are immune cells that continuously survey the brain
 * - Prune weak/inactive synapses during development and learning
 * - Activity-dependent refinement: preserve active, remove inactive
 * - State transitions: Ramified (surveillance) → Activated → Phagocytic
 * - Complement cascade: C1q/C3 tagging marks synapses for elimination
 * - Cytokine signaling: IL-1β, TNF-α, IL-6, IL-10, TGF-β communication
 *
 * MATHEMATICAL ENHANCEMENTS:
 * - KD-tree spatial indexing: O(log n) synapse queries vs O(n) linear
 * - RK4 ODE integration: Accurate state transition dynamics
 * - Centrality-protected pruning: Preserve network-critical synapses
 * - Signal filtering: Low-pass filtered activity for stable decisions
 * - Reaction-diffusion: Cytokine concentration dynamics
 *
 * DESIGN PRINCIPLES (SOLID):
 * - Single Responsibility: Modular components for each function
 * - Open/Closed: Extends neural network without modifying synapse code
 * - Interface Segregation: Focused API for surveillance/pruning/immune
 * - Dependency Inversion: Uses NIMCP utils abstractions
 *
 * INTEGRATION POINTS:
 * - nimcp_neuralnet.c: Synapse removal/pruning
 * - nimcp_brain.c: Assign microglia to spatial regions
 * - nimcp_kdtree.h: Spatial indexing for O(log n) queries
 * - nimcp_integration.h: RK4 ODE solver for state dynamics
 * - nimcp_centrality.h: Network importance for pruning protection
 *
 * PERFORMANCE CHARACTERISTICS:
 * - Spatial query: O(log N) with KD-tree
 * - Activity update: O(N) where N = monitored synapses
 * - State dynamics: O(1) per microglia (RK4 step)
 * - Centrality check: O(1) lookup (precomputed)
 * - Network step: O(M×N) parallelizable
 *
 * @author NIMCP Development Team
 * @date 2025-11-24
 * @version 2.0.0 (Enhanced with mathematical algorithms)
 */

#ifndef NIMCP_MICROGLIA_H
#define NIMCP_MICROGLIA_H

#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/spatial/nimcp_kdtree.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// CONSTANTS & BIOLOGICAL PARAMETERS
//=============================================================================

/** @brief Default surveillance radius (µm) */
#define NIMCP_MICROGLIA_SURVEILLANCE_RADIUS_UM 100.0f

/** @brief Default pruning threshold (activity score) */
#define NIMCP_MICROGLIA_PRUNING_THRESHOLD 0.1f

/** @brief Default pruning rate (max synapses per step) */
#define NIMCP_MICROGLIA_PRUNING_RATE 5.0f

/** @brief Activity score decay time constant (seconds) */
#define NIMCP_MICROGLIA_ACTIVITY_DECAY_TAU_S 10.0f

/** @brief Minimum activity window for assessment (milliseconds) */
#define NIMCP_MICROGLIA_MIN_ACTIVITY_WINDOW_MS 1000.0f

/** @brief Default capacity for monitored synapses */
#define NIMCP_MICROGLIA_DEFAULT_CAPACITY 1000

//-----------------------------------------------------------------------------
// Memory Pool Parameters (Phase 1.5+)
//-----------------------------------------------------------------------------

/** @brief Pool size for monitored synapse structures */
#define NIMCP_MICROGLIA_SYNAPSE_POOL_SIZE 2048

/** @brief Pool block size (64 entries per block for bitmap allocation) */
#define NIMCP_MICROGLIA_POOL_BLOCK_SIZE 64

//-----------------------------------------------------------------------------
// State Transition Parameters (RK4 ODE)
//-----------------------------------------------------------------------------

/** @brief Activation threshold (inflammation level) */
#define NIMCP_MICROGLIA_ACTIVATION_THRESHOLD 0.3f

/** @brief Phagocytic threshold (inflammation level) */
#define NIMCP_MICROGLIA_PHAGOCYTIC_THRESHOLD 0.7f

/** @brief State transition time constant (seconds) */
#define NIMCP_MICROGLIA_STATE_TAU_S 5.0f

/** @brief Base process extension rate (µm/s) */
#define NIMCP_MICROGLIA_PROCESS_EXTENSION_RATE 2.0f

//-----------------------------------------------------------------------------
// Cytokine Parameters
//-----------------------------------------------------------------------------

/** @brief Number of cytokine types modeled */
#define NIMCP_CYTOKINE_COUNT 5

/** @brief Cytokine decay rate (per second) */
#define NIMCP_CYTOKINE_DECAY_RATE 0.1f

/** @brief Cytokine diffusion coefficient (µm²/s) */
#define NIMCP_CYTOKINE_DIFFUSION_COEFF 100.0f

/** @brief Maximum cytokine concentration */
#define NIMCP_CYTOKINE_MAX_CONCENTRATION 10.0f

//-----------------------------------------------------------------------------
// Complement Cascade Parameters
//-----------------------------------------------------------------------------

/** @brief C1q tagging threshold (activity below this gets tagged) */
#define NIMCP_COMPLEMENT_C1Q_THRESHOLD 0.2f

/** @brief C3 conversion rate (C1q → C3 opsonization) */
#define NIMCP_COMPLEMENT_C3_CONVERSION_RATE 0.5f

/** @brief Complement decay rate (per second) */
#define NIMCP_COMPLEMENT_DECAY_RATE 0.05f

//-----------------------------------------------------------------------------
// Centrality Protection Parameters
//-----------------------------------------------------------------------------

/** @brief Centrality protection factor (multiplier for threshold) */
#define NIMCP_CENTRALITY_PROTECTION_FACTOR 2.0f

/** @brief Minimum centrality to enable protection */
#define NIMCP_CENTRALITY_PROTECTION_MIN 0.1f

//=============================================================================
// ENUMERATIONS
//=============================================================================

/**
 * @brief Microglia activation state
 *
 * BIOLOGICAL MODEL:
 * - RAMIFIED: Resting surveillance mode, highly branched processes
 * - ACTIVATED: Intermediate state, retracted processes, cytokine release
 * - PHAGOCYTIC: Fully activated, engulfing debris and synapses
 *
 * STATE TRANSITIONS (ODE-driven):
 * - inflammation < 0.3 → RAMIFIED
 * - 0.3 ≤ inflammation < 0.7 → ACTIVATED
 * - inflammation ≥ 0.7 → PHAGOCYTIC
 */
typedef enum {
    MICROGLIA_STATE_RAMIFIED = 0,   /**< Resting/surveillance mode */
    MICROGLIA_STATE_ACTIVATED = 1,  /**< Intermediate activation */
    MICROGLIA_STATE_PHAGOCYTIC = 2  /**< Full phagocytic mode */
} microglia_state_t;

/**
 * @brief Cytokine types
 *
 * BIOLOGICAL MODEL:
 * - Pro-inflammatory: IL1B, TNFA, IL6 (promote activation)
 * - Anti-inflammatory: IL10, TGFB (promote resolution)
 */
typedef enum {
    CYTOKINE_IL1B = 0,   /**< Interleukin-1β (pro-inflammatory) */
    CYTOKINE_TNFA = 1,   /**< Tumor Necrosis Factor-α (pro-inflammatory) */
    CYTOKINE_IL6 = 2,    /**< Interleukin-6 (pro-inflammatory) */
    CYTOKINE_IL10 = 3,   /**< Interleukin-10 (anti-inflammatory) */
    CYTOKINE_TGFB = 4    /**< Transforming Growth Factor-β (anti-inflammatory) */
} cytokine_type_t;

/* Backward compatibility aliases for base cytokine_type_t */
#define CYTOKINE_IL1       CYTOKINE_IL1B

/**
 * @brief Complement tag state
 *
 * BIOLOGICAL MODEL:
 * - NONE: No complement tagging
 * - C1Q: Initial complement binding (early mark)
 * - C3: Opsonization complete (ready for phagocytosis)
 */
typedef enum {
    COMPLEMENT_NONE = 0,  /**< No complement tag */
    COMPLEMENT_C1Q = 1,   /**< C1q bound (weak tag) */
    COMPLEMENT_C3 = 2     /**< C3 opsonized (strong tag, prune-ready) */
} complement_tag_t;

//=============================================================================
// DATA STRUCTURES
//=============================================================================

/**
 * @brief Cytokine state for a microglia
 *
 * Models local cytokine concentrations around this microglia.
 * Used for communication and state modulation.
 */
typedef struct {
    float concentrations[NIMCP_CYTOKINE_COUNT];  /**< Current concentrations */
    float production_rates[NIMCP_CYTOKINE_COUNT]; /**< Production rates */
    uint64_t last_update_time;                    /**< Last update timestamp */
} microglia_cytokine_state_t;

/**
 * @brief Complement tag info for a synapse
 */
typedef struct {
    complement_tag_t tag;      /**< Current complement state */
    float tag_strength;        /**< Tag strength (0-1) */
    uint64_t tag_time;         /**< When tag was applied */
} synapse_complement_t;

/**
 * @brief Enhanced synapse monitoring data
 *
 * Extended per-synapse data including complement tags,
 * centrality scores, and filtered activity.
 */
typedef struct {
    uint64_t synapse_id;           /**< Synapse identifier (G7: uint64 packs pre/post neuron IDs bijectively) */
    float position[3];             /**< Synapse 3D position (µm) */
    float activity_score;          /**< Current activity (EMA) */
    float filtered_activity;       /**< Low-pass filtered activity */
    uint64_t last_activity_time;   /**< Last spike timestamp */
    synapse_complement_t complement; /**< Complement cascade state */
    float centrality_score;        /**< Network importance (0-1) */
    bool protected_by_centrality;  /**< If true, pruning protection active */
} monitored_synapse_t;

/**
 * @brief Memory pool for monitored synapse structures (Phase 1.5+)
 *
 * Pre-allocated pool with O(1) bitmap-based allocation to avoid
 * malloc/free in hot paths during synapse monitoring operations.
 */
typedef struct {
    monitored_synapse_t* buffer;     /**< Pre-allocated synapse array */
    uint64_t* bitmap;                /**< Bitmap for free/allocated tracking (1=free) */
    uint32_t capacity;               /**< Total slots in pool */
    uint32_t num_bitmap_words;       /**< Number of 64-bit bitmap words */
    uint32_t allocated_count;        /**< Number of currently allocated slots */
    nimcp_spinlock_t lock;           /**< Thread-safe access */
} microglia_synapse_pool_t;

/**
 * @brief Microglia cell state (Enhanced)
 *
 * Models a single microglia with full biological accuracy:
 * - State dynamics via RK4 ODE integration
 * - Cytokine signaling system
 * - Complement cascade tagging
 * - Centrality-aware pruning
 * - KD-tree indexed synapse monitoring
 */
typedef struct {
    uint32_t id;                         /**< Unique microglia ID */

    //-------------------------------------------------------------------------
    // Spatial Properties
    //-------------------------------------------------------------------------
    float position[3];                   /**< x, y, z coordinates (µm) */
    float surveillance_radius;           /**< Monitoring radius (µm) */
    float process_extension;             /**< Current process length (0-1) */

    //-------------------------------------------------------------------------
    // State Dynamics (RK4 ODE)
    //-------------------------------------------------------------------------
    microglia_state_t state;             /**< Current activation state */
    float inflammation_level;            /**< Inflammation driver (0-1) */
    float state_variables[4];            /**< ODE state: [inflammation, activation, process, energy] */

    //-------------------------------------------------------------------------
    // Cytokine Signaling
    //-------------------------------------------------------------------------
    microglia_cytokine_state_t cytokines; /**< Cytokine concentrations */

    //-------------------------------------------------------------------------
    // Monitored Synapses (Enhanced)
    //-------------------------------------------------------------------------
    uint32_t num_monitored_synapses;     /**< Number of synapses monitored */
    uint32_t max_monitored_synapses;     /**< Capacity */
    monitored_synapse_t* synapses;       /**< Enhanced synapse data array */

    //-------------------------------------------------------------------------
    // Legacy Arrays (For backward compatibility)
    //-------------------------------------------------------------------------
    uint64_t* monitored_synapse_ids;     /**< Array of synapse IDs (G7: uint64) */
    float* synapse_activity_scores;      /**< Activity level per synapse */
    uint64_t* last_activity_times;       /**< Last active timestamp */

    //-------------------------------------------------------------------------
    // Pruning Parameters
    //-------------------------------------------------------------------------
    float pruning_threshold;             /**< Activity below this → candidate */
    float pruning_rate;                  /**< Max synapses to prune per step */
    uint64_t last_pruning_time;          /**< When last pruned (µs) */

    //-------------------------------------------------------------------------
    // Statistics
    //-------------------------------------------------------------------------
    uint32_t total_synapses_pruned;      /**< Cumulative pruned count */
    uint32_t total_c1q_tags;             /**< Total C1q tags applied */
    uint32_t total_c3_conversions;       /**< Total C3 conversions */
    uint32_t protected_from_pruning;     /**< Synapses protected by centrality */

    //-------------------------------------------------------------------------
    // Thread Safety
    //-------------------------------------------------------------------------
    nimcp_spinlock_t lock;               /**< Lock for concurrent access */

    //-------------------------------------------------------------------------
    // Copy-on-Write Support (Phase 1.5+)
    //-------------------------------------------------------------------------
    uint32_t cow_ref_count;              /**< Reference count for CoW */
    bool cow_modified;                   /**< True if modified since copy */
    void* cow_original;                  /**< Pointer to original if this is a copy */
} microglia_t;

/**
 * @brief Network of microglia cells (Enhanced)
 *
 * Manages multiple microglia with:
 * - KD-tree spatial indexing for O(log n) queries
 * - Network-wide cytokine diffusion
 * - Centrality computation integration
 */
typedef struct {
    uint32_t num_microglia;              /**< Current number of microglia */
    uint32_t capacity;                   /**< Max microglia */
    microglia_t** microglia;             /**< Array of microglia pointers */

    //-------------------------------------------------------------------------
    // Spatial Indexing
    //-------------------------------------------------------------------------
    kdtree_t* microglia_tree;            /**< KD-tree for microglia positions */
    kdtree_t* synapse_tree;              /**< KD-tree for synapse positions */
    bool spatial_index_valid;            /**< True if KD-trees are up to date */
    uint32_t synapse_changes_since_rebuild; /**< Synapse add/remove count since last KD-tree rebuild */
    uint32_t total_monitored_synapse_count; /**< Total synapses across all microglia (for 10% threshold) */

    //-------------------------------------------------------------------------
    // Global Cytokine Field
    //-------------------------------------------------------------------------
    float* global_cytokine_field;        /**< Network-wide cytokine concentrations */
    uint32_t cytokine_field_size;        /**< Size of cytokine field */

    //-------------------------------------------------------------------------
    // Centrality Scores (Precomputed)
    //-------------------------------------------------------------------------
    float* synapse_centrality;           /**< Per-synapse centrality scores */
    uint32_t num_centrality_scores;      /**< Number of centrality scores */
    bool centrality_valid;               /**< True if centrality is up to date */

    //-------------------------------------------------------------------------
    // Global Parameters
    //-------------------------------------------------------------------------
    float global_pruning_threshold;      /**< Default pruning threshold */
    float min_activity_window_ms;        /**< Min time window for assessment */
    float global_inflammation;           /**< Network-wide inflammation level */

    //-------------------------------------------------------------------------
    // Activity Filter State
    //-------------------------------------------------------------------------
    float filter_cutoff_hz;              /**< Low-pass filter cutoff (Hz) */
    float filter_alpha;                  /**< Filter smoothing coefficient */

    //-------------------------------------------------------------------------
    // Thread Safety
    //-------------------------------------------------------------------------
    nimcp_mutex_t lock;                  /**< Network-level lock */

    //-------------------------------------------------------------------------
    // Timing State
    //-------------------------------------------------------------------------
    uint64_t last_step_time;             /**< Last step timestamp for dt calculation */

    //-------------------------------------------------------------------------
    // Memory Pool (Phase 1.5+)
    //-------------------------------------------------------------------------
    microglia_synapse_pool_t* synapse_pool;  /**< Shared pool for synapse data */
} microglia_network_t;

/**
 * @brief Configuration for microglia network
 */
typedef struct {
    uint32_t capacity;                   /**< Maximum microglia count */
    float pruning_threshold;             /**< Activity threshold for pruning */
    float pruning_rate;                  /**< Max pruning per step */
    float surveillance_radius;           /**< Default surveillance radius */
    bool enable_centrality_protection;   /**< Use centrality for protection */
    bool enable_complement_cascade;      /**< Use C1q/C3 tagging */
    bool enable_cytokine_signaling;      /**< Use cytokine system */
    bool enable_state_dynamics;          /**< Use RK4 state transitions */
    float filter_cutoff_hz;              /**< Activity filter cutoff */
} microglia_network_config_t;

/**
 * @brief Statistics for microglia network
 */
typedef struct {
    uint32_t total_microglia;
    uint32_t total_monitored_synapses;
    uint32_t total_pruned;
    uint32_t total_c1q_tagged;
    uint32_t total_c3_tagged;
    uint32_t total_protected;
    uint32_t ramified_count;
    uint32_t activated_count;
    uint32_t phagocytic_count;
    float avg_inflammation;
    float avg_activity_score;
    float total_pro_inflammatory;
    float total_anti_inflammatory;
} microglia_network_stats_t;

//=============================================================================
// CREATION & DESTRUCTION
//=============================================================================

/**
 * @brief Create a new enhanced microglia cell
 *
 * @param id Unique identifier
 * @param x X coordinate (µm)
 * @param y Y coordinate (µm)
 * @param z Z coordinate (µm)
 * @param surveillance_radius Monitoring radius (µm)
 *
 * @return Pointer to microglia or NULL on failure
 */
microglia_t* microglia_create(uint32_t id, float x, float y, float z,
                               float surveillance_radius);

/**
 * @brief Destroy microglia and free resources
 *
 * @param mg Microglia to destroy (NULL safe)
 */
void microglia_destroy(microglia_t* mg);

/**
 * @brief Get default network configuration
 *
 * @return Default configuration with all features enabled
 */
microglia_network_config_t microglia_network_default_config(void);

/**
 * @brief Create enhanced microglia network
 *
 * @param config Configuration parameters
 *
 * @return Network handle or NULL on failure
 */
microglia_network_t* microglia_network_create_enhanced(
    const microglia_network_config_t* config);

/**
 * @brief Create basic microglia network (backward compatible)
 *
 * @param capacity Maximum number of microglia
 *
 * @return Network handle or NULL on failure
 */
microglia_network_t* microglia_network_create(uint32_t capacity);

/**
 * @brief Destroy microglia network
 *
 * @param network Network to destroy (NULL safe)
 */
void microglia_network_destroy(microglia_network_t* network);

//=============================================================================
// SYNAPSE MONITORING
//=============================================================================

/**
 * @brief Add synapse to monitoring with position
 *
 * @param mg Microglia
 * @param synapse_id Synapse identifier
 * @param x Synapse X position (µm)
 * @param y Synapse Y position (µm)
 * @param z Synapse Z position (µm)
 *
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t microglia_monitor_synapse_at(microglia_t* mg, uint64_t synapse_id,
                                             float x, float y, float z);

/**
 * @brief Add synapse to monitoring (legacy, no position)
 *
 * @param mg Microglia
 * @param synapse_id Synapse identifier
 *
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t microglia_monitor_synapse(microglia_t* mg, uint64_t synapse_id);

/**
 * @brief Track activity for a monitored synapse
 *
 * @param mg Microglia
 * @param synapse_id Synapse identifier
 * @param activity Activity level (0-10)
 * @param timestamp Current timestamp (µs)
 */
void microglia_track_synapse_activity(microglia_t* mg, uint64_t synapse_id,
                                       float activity, uint64_t timestamp);

/**
 * @brief Update all activity scores (decay + filtering)
 *
 * @param mg Microglia
 * @param current_time Current timestamp (µs)
 */
void microglia_update_activity_scores(microglia_t* mg, uint64_t current_time);

/**
 * @brief Get activity score for a synapse
 *
 * @param mg Microglia
 * @param synapse_id Synapse identifier
 *
 * @return Activity score (0-1+) or 0.0 if not found
 */
float microglia_get_synapse_activity_score(microglia_t* mg, uint64_t synapse_id);

/**
 * @brief Set centrality score for a synapse
 *
 * @param mg Microglia
 * @param synapse_id Synapse identifier
 * @param centrality Centrality score (0-1)
 */
void microglia_set_synapse_centrality(microglia_t* mg, uint64_t synapse_id,
                                       float centrality);

//=============================================================================
// STATE DYNAMICS (RK4 ODE)
//=============================================================================

/**
 * @brief Update microglia state using RK4 integration
 *
 * @param mg Microglia
 * @param dt Time step (seconds)
 *
 * DYNAMICS:
 * - d(inflammation)/dt = input - decay
 * - d(activation)/dt = f(inflammation) - decay
 * - d(process)/dt = extension_rate × (1 - process) if ramified, -rate if activated
 * - State transitions based on inflammation thresholds
 */
void microglia_update_state_dynamics(microglia_t* mg, float dt);

/**
 * @brief Get current activation state
 *
 * @param mg Microglia
 *
 * @return Current state
 */
microglia_state_t microglia_get_state(const microglia_t* mg);

/**
 * @brief Set inflammation input (external stimulus)
 *
 * @param mg Microglia
 * @param inflammation Inflammation level (0-1)
 */
void microglia_set_inflammation(microglia_t* mg, float inflammation);

/**
 * @brief Get current process extension (0-1)
 *
 * @param mg Microglia
 *
 * @return Process extension (1.0 = fully extended, 0.0 = retracted)
 */
float microglia_get_process_extension(const microglia_t* mg);

//=============================================================================
// COMPLEMENT CASCADE
//=============================================================================

/**
 * @brief Apply complement tagging to weak synapses
 *
 * @param mg Microglia
 * @param current_time Current timestamp (µs)
 *
 * ALGORITHM:
 * 1. For each synapse with activity < C1Q_THRESHOLD:
 *    - If no tag: Apply C1q tag
 *    - If C1q tag and time elapsed > conversion_time: Convert to C3
 * 2. C3-tagged synapses are prioritized for pruning
 *
 * @return Number of synapses newly tagged
 */
uint32_t microglia_apply_complement_tags(microglia_t* mg, uint64_t current_time);

/**
 * @brief Get complement tag for a synapse
 *
 * @param mg Microglia
 * @param synapse_id Synapse identifier
 *
 * @return Complement tag state
 */
complement_tag_t microglia_get_complement_tag(const microglia_t* mg,
                                               uint64_t synapse_id);

/**
 * @brief Decay complement tags over time
 *
 * @param mg Microglia
 * @param dt Time step (seconds)
 */
void microglia_decay_complement_tags(microglia_t* mg, float dt);

//=============================================================================
// CYTOKINE SIGNALING
//=============================================================================

/**
 * @brief Update cytokine production based on state
 *
 * @param mg Microglia
 * @param dt Time step (seconds)
 *
 * PRODUCTION RULES:
 * - RAMIFIED: Low production of all cytokines
 * - ACTIVATED: High IL-1β, TNF-α, IL-6
 * - PHAGOCYTIC: High IL-10, TGF-β (resolution phase)
 */
void microglia_update_cytokines(microglia_t* mg, float dt);

/**
 * @brief Get cytokine concentration
 *
 * @param mg Microglia
 * @param type Cytokine type
 *
 * @return Concentration (0 - MAX_CONCENTRATION)
 */
float microglia_get_cytokine(const microglia_t* mg, cytokine_type_t type);

/**
 * @brief Add external cytokine input (from other cells)
 *
 * @param mg Microglia
 * @param type Cytokine type
 * @param amount Amount to add
 */
void microglia_add_cytokine(microglia_t* mg, cytokine_type_t type, float amount);

/**
 * @brief Compute net inflammatory signal
 *
 * @param mg Microglia
 *
 * @return Net inflammation (positive = pro-inflammatory)
 *
 * FORMULA: (IL1B + TNFA + IL6) - (IL10 + TGFB)
 */
float microglia_get_net_inflammation(const microglia_t* mg);

//=============================================================================
// PRUNING (Enhanced with Centrality Protection)
//=============================================================================

/**
 * @brief Identify weak synapses (centrality-aware)
 *
 * @param mg Microglia
 * @param weak_synapse_ids Output buffer
 * @param max_count Buffer size
 *
 * @return Number of weak synapses identified
 *
 * ALGORITHM:
 * - Base threshold adjusted by centrality: threshold × (1 + centrality × protection_factor)
 * - C3-tagged synapses have lowered threshold
 * - High-centrality synapses are protected
 */
uint32_t microglia_identify_weak_synapses(microglia_t* mg,
                                           uint32_t* weak_synapse_ids,
                                           uint32_t max_count);

/**
 * @brief Prune weak synapses (centrality-aware)
 *
 * @param mg Microglia
 *
 * @return Number of synapses pruned
 */
uint32_t microglia_prune_weak_synapses(microglia_t* mg);

/**
 * @brief Check if synapse should be pruned
 *
 * @param mg Microglia
 * @param synapse_id Synapse identifier
 *
 * @return true if synapse should be pruned
 */
bool microglia_should_prune_synapse(const microglia_t* mg, uint64_t synapse_id);

//=============================================================================
// NETWORK OPERATIONS
//=============================================================================

/**
 * @brief Add microglia to network
 *
 * @param network Network
 * @param mg Microglia to add
 *
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t microglia_network_add(microglia_network_t* network, microglia_t* mg);

/**
 * @brief Rebuild spatial index (KD-trees)
 *
 * @param network Network
 *
 * Call after adding/removing microglia or synapses.
 */
void microglia_network_rebuild_spatial_index(microglia_network_t* network);

/**
 * @brief Update centrality scores for all synapses
 *
 * @param network Network
 * @param synapse_graph Graph representation of synaptic connections
 *
 * Uses betweenness centrality from nimcp_centrality.h
 */
void microglia_network_update_centrality(microglia_network_t* network,
                                          void* synapse_graph);

/**
 * @brief Step network forward (full simulation step)
 *
 * @param network Network
 * @param current_time Current timestamp (µs)
 *
 * OPERATIONS:
 * 1. Update state dynamics (RK4) for all microglia
 * 2. Apply complement tagging
 * 3. Update cytokine concentrations
 * 4. Diffuse cytokines between nearby microglia
 * 5. Update activity scores
 * 6. Prune weak synapses
 */
void microglia_network_step(microglia_network_t* network, uint64_t current_time);

/**
 * @brief Find microglia monitoring a synapse
 *
 * @param network Network
 * @param synapse_id Synapse identifier
 *
 * @return Microglia pointer or NULL
 */
microglia_t* microglia_network_find_by_synapse(microglia_network_t* network,
                                                uint64_t synapse_id);

/**
 * @brief Find nearest microglia to a position
 *
 * @param network Network
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 *
 * @return Nearest microglia or NULL
 *
 * COMPLEXITY: O(log N) with KD-tree
 */
microglia_t* microglia_network_find_nearest(microglia_network_t* network,
                                             float x, float y, float z);

/**
 * @brief Find all microglia within radius
 *
 * @param network Network
 * @param x Center X
 * @param y Center Y
 * @param z Center Z
 * @param radius Search radius
 * @param results Output buffer
 * @param max_results Buffer size
 *
 * @return Number of microglia found
 */
uint32_t microglia_network_find_in_radius(microglia_network_t* network,
                                           float x, float y, float z,
                                           float radius,
                                           microglia_t** results,
                                           uint32_t max_results);

/**
 * @brief Diffuse cytokines between nearby microglia
 *
 * @param network Network
 * @param dt Time step (seconds)
 */
void microglia_network_diffuse_cytokines(microglia_network_t* network, float dt);

/**
 * @brief Get network statistics
 *
 * @param network Network
 * @param stats Output statistics
 */
void microglia_network_get_stats(const microglia_network_t* network,
                                  microglia_network_stats_t* stats);

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/**
 * @brief Get total synapses pruned
 *
 * @param mg Microglia
 *
 * @return Total pruned count
 */
uint32_t microglia_get_total_pruned(microglia_t* mg);

/**
 * @brief Get state name as string
 *
 * @param state Microglia state
 *
 * @return State name string
 */
const char* microglia_state_to_string(microglia_state_t state);

/**
 * @brief Get cytokine name as string
 *
 * @param type Cytokine type
 *
 * @return Cytokine name string
 */
const char* cytokine_type_to_string(cytokine_type_t type);

//=============================================================================
// MEMORY POOL FUNCTIONS (Phase 1.5+)
//=============================================================================

/**
 * @brief Create a synapse memory pool
 *
 * @param capacity Number of synapse slots to pre-allocate
 *
 * @return Pool handle or NULL on failure
 */
microglia_synapse_pool_t* microglia_synapse_pool_create(uint32_t capacity);

/**
 * @brief Destroy a synapse memory pool
 *
 * @param pool Pool to destroy (NULL safe)
 */
void microglia_synapse_pool_destroy(microglia_synapse_pool_t* pool);

/**
 * @brief Allocate a synapse from the pool
 *
 * @param pool Memory pool
 *
 * @return Pointer to synapse or NULL if pool exhausted
 */
monitored_synapse_t* microglia_synapse_pool_alloc(microglia_synapse_pool_t* pool);

/**
 * @brief Return a synapse to the pool
 *
 * @param pool Memory pool
 * @param synapse Synapse to return
 */
void microglia_synapse_pool_free(microglia_synapse_pool_t* pool,
                                  monitored_synapse_t* synapse);

/**
 * @brief Get pool utilization statistics
 *
 * @param pool Memory pool
 * @param allocated Output: number of allocated slots
 * @param capacity Output: total capacity
 */
void microglia_synapse_pool_stats(const microglia_synapse_pool_t* pool,
                                   uint32_t* allocated, uint32_t* capacity);

//=============================================================================
// COPY-ON-WRITE FUNCTIONS (Phase 1.5+)
//=============================================================================

/**
 * @brief Create a shallow copy of a microglia (CoW)
 *
 * @param mg Microglia to copy
 *
 * @return Shallow copy sharing data with original
 */
microglia_t* microglia_cow_copy(microglia_t* mg);

/**
 * @brief Prepare microglia for write (deep copy if shared)
 *
 * @param mg Microglia to prepare
 *
 * @return NIMCP_SUCCESS if ready for writing
 */
nimcp_result_t microglia_cow_prepare_write(microglia_t* mg);

/**
 * @brief Release a CoW reference
 *
 * @param mg Microglia to release
 */
void microglia_cow_release(microglia_t* mg);

/**
 * @brief Check if microglia is a CoW copy
 *
 * @param mg Microglia to check
 *
 * @return true if this is a CoW copy
 */
bool microglia_is_cow_copy(const microglia_t* mg);

//=============================================================================
// Bio-Async Integration API
//=============================================================================

/**
 * @brief Register microglia bio-async message handlers
 *
 * WHAT: Registers handlers for alert, prune request, metabolic, and sync messages
 * WHY:  Enables event-driven microglia coordination via bio-async messaging
 * HOW:  Calls internal bio_init which registers with bio-router
 *
 * This function should be called during glial system initialization.
 * It registers handlers for:
 * - BIO_MSG_MICROGLIA_ALERT
 * - BIO_MSG_MICROGLIA_PRUNE_REQUEST
 * - BIO_MSG_METABOLIC_DEMAND
 * - BIO_MSG_GLIAL_SYNC_REQUEST
 *
 * @return NIMCP_SUCCESS or error code
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (uses internal mutex)
 */
nimcp_result_t microglia_register_bio_handlers(void);

/**
 * @brief Unregister microglia bio-async handlers and cleanup
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
void microglia_unregister_bio_handlers(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MICROGLIA_H
