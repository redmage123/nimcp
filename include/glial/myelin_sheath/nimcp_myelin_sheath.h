//=============================================================================
// nimcp_myelin_sheath.h - Myelin Sheath Module
//=============================================================================
/**
 * @file nimcp_myelin_sheath.h
 * @brief Myelin sheath structural modeling and dynamics
 *
 * WHAT: Dedicated module for myelin sheath properties and health tracking
 * WHY:  Separates myelin structure concerns from oligodendrocyte cell biology
 * HOW:  Models lamellae, compaction, integrity, and pathological states
 *
 * BIOLOGICAL BASIS:
 * - Myelin is a multilayered membrane wrapping around axons
 * - Formed by oligodendrocytes in CNS, Schwann cells in PNS
 * - Enables saltatory conduction (50-100x faster than unmyelinated)
 * - G-ratio optimization: optimal ~0.77 for CNS axons
 * - Paranodal junctions seal myelin to axon membrane
 *
 * KEY FEATURES:
 * - Lamellae tracking (number of myelin membrane wraps)
 * - Compaction modeling (MDL and intraperiod lines)
 * - Integrity/health scoring with damage accumulation
 * - Demyelination/remyelination dynamics
 * - Paranodal junction modeling
 * - Memory pools for O(1) allocation
 * - Copy-on-Write support for efficient cloning
 *
 * INTEGRATION:
 * - Works with oligodendrocytes (myelin producers)
 * - Works with axons (myelin consumers)
 * - Integrates with glial_integration layer
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-11-25
 */

#ifndef NIMCP_MYELIN_SHEATH_H
#define NIMCP_MYELIN_SHEATH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"

//=============================================================================
// Constants
//=============================================================================

/** @name Myelin Structural Constants */
///@{
#define NIMCP_MYELIN_MIN_LAMELLAE          1      /**< Minimum lamellae for myelination */
#define NIMCP_MYELIN_MAX_LAMELLAE          160    /**< Maximum lamellae (thick myelin) */
#define NIMCP_MYELIN_OPTIMAL_LAMELLAE      40     /**< Typical CNS lamellae count */
#define NIMCP_MYELIN_LAMELLA_THICKNESS_NM  12.0f  /**< Single lamella thickness (nm) */
#define NIMCP_MYELIN_MDL_PERIOD_NM         3.2f   /**< Major dense line period (nm) */
#define NIMCP_MYELIN_IPL_PERIOD_NM         2.5f   /**< Intraperiod line period (nm) */
///@}

/** @name G-Ratio Constants */
///@{
#define NIMCP_MYELIN_G_RATIO_MIN           0.5f   /**< Minimum g-ratio (very thick myelin) */
#define NIMCP_MYELIN_G_RATIO_MAX           0.95f  /**< Maximum g-ratio (very thin myelin) */
#define NIMCP_MYELIN_G_RATIO_OPTIMAL       0.77f  /**< Optimal CNS g-ratio (Rushton) */
#define NIMCP_MYELIN_G_RATIO_TOLERANCE     0.05f  /**< Acceptable deviation from optimal */
///@}

/** @name Conduction Constants */
///@{
#define NIMCP_MYELIN_BASE_VELOCITY_MS      1.0f   /**< Unmyelinated velocity (m/s) */
#define NIMCP_MYELIN_MAX_VELOCITY_MS       120.0f /**< Maximum myelinated velocity (m/s) */
#define NIMCP_MYELIN_VELOCITY_COEFF        6.0f   /**< Hursh's law coefficient (m/s per um) */
#define NIMCP_MYELIN_VELOCITY_MULTIPLIER   50.0f  /**< Max speedup factor with full myelin */
///@}

/** @name Integrity Constants */
///@{
#define NIMCP_MYELIN_INTEGRITY_HEALTHY     0.9f   /**< Threshold for healthy myelin */
#define NIMCP_MYELIN_INTEGRITY_DAMAGED     0.5f   /**< Threshold for damaged myelin */
#define NIMCP_MYELIN_INTEGRITY_CRITICAL    0.2f   /**< Critical damage threshold */
#define NIMCP_MYELIN_REPAIR_RATE_BASE      0.001f /**< Base repair rate per second */
#define NIMCP_MYELIN_DAMAGE_DECAY_RATE     0.0005f/**< Spontaneous damage rate per second */
///@}

/** @name Pool Constants */
///@{
#define NIMCP_MYELIN_SHEATH_POOL_SIZE      4096   /**< Pre-allocated sheaths */
#define NIMCP_MYELIN_SEGMENT_POOL_SIZE     16384  /**< Pre-allocated segments */
#define NIMCP_MYELIN_POOL_BLOCK_SIZE       64     /**< Bitmap block size */
///@}

/** @name Internode Constants */
///@{
#define NIMCP_MYELIN_NODE_LENGTH_UM        1.0f   /**< Node of Ranvier length (um) */
#define NIMCP_MYELIN_MIN_INTERNODE_UM      100.0f /**< Minimum internode length (um) */
#define NIMCP_MYELIN_MAX_INTERNODE_UM      1500.0f/**< Maximum internode length (um) */
#define NIMCP_MYELIN_INTERNODE_RATIO       100.0f /**< Internode length / axon diameter */
///@}

/** @name Metabolic Constants */
///@{
#define NIMCP_MYELIN_ATP_MAINTENANCE       0.001f /**< ATP cost per second per segment */
#define NIMCP_MYELIN_LACTATE_TRANSFER_RATE 0.8f   /**< Lactate transfer efficiency */
#define NIMCP_MYELIN_TROPHIC_THRESHOLD     0.1f   /**< Min trophic support for health */
///@}

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Myelin sheath health state
 *
 * WHAT: Represents the current health/pathological state of myelin
 * WHY:  Enable modeling of demyelination and remyelination
 * HOW:  State machine driven by integrity score
 */
typedef enum {
    MYELIN_HEALTH_INTACT = 0,       /**< Healthy, fully functional myelin */
    MYELIN_HEALTH_STRESSED,         /**< Under metabolic stress, may degrade */
    MYELIN_HEALTH_DAMAGED,          /**< Partial damage, reduced function */
    MYELIN_HEALTH_DEMYELINATING,    /**< Active demyelination in progress */
    MYELIN_HEALTH_DEMYELINATED,     /**< Complete loss of myelin */
    MYELIN_HEALTH_REMYELINATING,    /**< Active remyelination in progress */
    MYELIN_HEALTH_COUNT             /**< Number of health states */
} myelin_health_state_t;

/**
 * @brief Myelin compaction state
 *
 * WHAT: Degree of myelin membrane compaction
 * WHY:  Compaction affects insulation quality and conduction
 * HOW:  MDL and IPL formation determines compaction
 */
typedef enum {
    MYELIN_COMPACT_NONE = 0,        /**< No compaction (immature) */
    MYELIN_COMPACT_PARTIAL,         /**< Partial compaction */
    MYELIN_COMPACT_FULL,            /**< Full compaction (mature) */
    MYELIN_COMPACT_COUNT            /**< Number of compaction states */
} myelin_compaction_t;

/**
 * @brief Paranodal junction state
 *
 * WHAT: State of axon-myelin junction at paranodes
 * WHY:  Paranodal junctions seal myelin and cluster ion channels
 * HOW:  Caspr/Contactin/NF155 complex formation
 */
typedef enum {
    PARANODE_ABSENT = 0,            /**< No paranodal junction */
    PARANODE_FORMING,               /**< Junction forming */
    PARANODE_MATURE,                /**< Fully formed, functional junction */
    PARANODE_DISRUPTED,             /**< Junction disrupted (pathological) */
    PARANODE_COUNT                  /**< Number of paranode states */
} paranode_state_t;

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct myelin_sheath myelin_sheath_t;
typedef struct myelin_segment myelin_segment_t;
typedef struct myelin_sheath_network myelin_sheath_network_t;
typedef struct myelin_sheath_pool myelin_sheath_pool_t;
typedef struct myelin_segment_pool myelin_segment_pool_t;

//=============================================================================
// Core Data Structures
//=============================================================================

/**
 * @brief Myelin segment (internode) structure
 *
 * WHAT: Single myelinated segment between nodes of Ranvier
 * WHY:  Enable per-segment tracking of myelin properties
 * HOW:  Contains structural, health, and metabolic state
 *
 * BIOLOGICAL BASIS:
 * - Internode = myelinated segment between nodes
 * - Optimal length: 100x axon diameter
 * - Contains paranodal junctions at each end
 */
typedef struct myelin_segment {
    // === IDENTIFICATION ===
    uint32_t id;                        /**< Unique segment ID */
    uint32_t sheath_id;                 /**< Parent sheath ID */
    uint32_t axon_id;                   /**< Associated axon ID */

    // === POSITION ===
    float start_position_um;            /**< Start position along axon (um) */
    float length_um;                    /**< Segment length (um) */
    float position[3];                  /**< 3D centroid position */

    // === STRUCTURAL PROPERTIES ===
    uint32_t num_lamellae;              /**< Number of myelin wraps */
    float thickness_um;                 /**< Total myelin thickness (um) */
    float inner_diameter_um;            /**< Axon diameter at segment (um) */
    float outer_diameter_um;            /**< Total fiber diameter (um) */
    float g_ratio;                      /**< Inner/outer diameter ratio */

    // === COMPACTION STATE ===
    myelin_compaction_t compaction;     /**< Compaction state */
    float compaction_score;             /**< Compaction completeness (0-1) */
    float mdl_formation;                /**< Major dense line formation (0-1) */
    float ipl_formation;                /**< Intraperiod line formation (0-1) */

    // === PARANODAL JUNCTIONS ===
    paranode_state_t proximal_paranode; /**< Proximal paranode state */
    paranode_state_t distal_paranode;   /**< Distal paranode state */
    float paranode_integrity;           /**< Junction integrity (0-1) */

    // === HEALTH/INTEGRITY ===
    myelin_health_state_t health;       /**< Current health state */
    float integrity;                    /**< Structural integrity (0-1) */
    float damage_accumulated;           /**< Accumulated damage */
    uint64_t damage_onset_time;         /**< When damage began (us) */

    // === CONDUCTION PROPERTIES ===
    float local_velocity_ms;            /**< Local conduction velocity (m/s) */
    float propagation_delay_ms;         /**< Delay through this segment (ms) */
    float block_probability;            /**< Conduction block probability (0-1) */
    bool is_conducting;                 /**< True if signal can propagate */

    // === ENHANCED BIOPHYSICS (from nimcp_myelin_math.h) ===
    nimcp_cable_params_t cable_params;  /**< Cable theory parameters */
    nimcp_saltatory_result_t saltatory; /**< Detailed velocity breakdown */
    float optimal_g_ratio;              /**< Diameter-dependent optimal g-ratio */
    float optimal_internode_um;         /**< Optimal internode for this diameter */
    float internode_efficiency;         /**< Current vs optimal internode efficiency */

    // === METABOLIC STATE ===
    float atp_level;                    /**< Local ATP availability (0-1) */
    float lactate_received;             /**< Lactate from oligodendrocyte (mM) */
    float trophic_support;              /**< Trophic factor level (0-1) */

    // === TIMESTAMPS ===
    uint64_t creation_time;             /**< When segment was created (us) */
    uint64_t last_update_time;          /**< Last update timestamp (us) */
} myelin_segment_t;

/**
 * @brief Myelin sheath structure (collection of segments)
 *
 * WHAT: Complete myelin sheath wrapping an axon
 * WHY:  Aggregate view of all myelin segments on one axon
 * HOW:  Contains array of segments plus aggregate statistics
 *
 * BIOLOGICAL BASIS:
 * - One oligodendrocyte can form 10-50 sheaths
 * - Each sheath contains multiple segments (internodes)
 * - Sheath health affects overall axon function
 */
typedef struct myelin_sheath {
    // === IDENTIFICATION ===
    uint32_t id;                        /**< Unique sheath ID */
    uint32_t axon_id;                   /**< Target axon ID */
    uint32_t oligodendrocyte_id;        /**< Producing oligodendrocyte ID */

    // === SEGMENTS ===
    uint32_t num_segments;              /**< Number of segments */
    uint32_t max_segments;              /**< Maximum segment capacity */
    myelin_segment_t** segments;        /**< Array of segment pointers */

    // === AGGREGATE PROPERTIES ===
    float total_length_um;              /**< Total myelinated length (um) */
    float mean_g_ratio;                 /**< Average g-ratio */
    float mean_lamellae;                /**< Average lamellae count */
    float coverage_fraction;            /**< Fraction of axon covered */

    // === AGGREGATE HEALTH ===
    myelin_health_state_t overall_health; /**< Aggregate health state */
    float mean_integrity;               /**< Average segment integrity */
    float min_integrity;                /**< Minimum segment integrity */
    uint32_t damaged_segment_count;     /**< Number of damaged segments */

    // === CONDUCTION ===
    float effective_velocity_ms;        /**< Effective conduction velocity (m/s) */
    float total_delay_ms;               /**< Total propagation delay (ms) */
    float velocity_ratio;               /**< Ratio vs unmyelinated */

    // === METABOLIC ===
    float total_atp_consumption;        /**< Total ATP usage */
    float mean_trophic_support;         /**< Average trophic support */

    // === ENHANCED BIOPHYSICS (from nimcp_myelin_math.h) ===
    nimcp_myelin_biophysics_t* biophysics; /**< Comprehensive biophysics state */
    nimcp_metabolic_efficiency_t metabolic_efficiency; /**< Energy efficiency metrics */
    float activity_ema;                 /**< Activity exponential moving average */
    float current_temperature_c;        /**< Temperature for block modeling (°C) */

    // === DYNAMICS ===
    float myelination_rate;             /**< Current myelination rate */
    float demyelination_rate;           /**< Current demyelination rate */
    float repair_rate;                  /**< Current repair rate */

    // === STATE TRACKING ===
    uint64_t creation_time;             /**< Sheath creation time (us) */
    uint64_t last_update_time;          /**< Last update timestamp (us) */
    uint64_t maturation_time;           /**< When sheath became mature (us) */
    bool is_mature;                     /**< True if fully matured */

    // === COPY-ON-WRITE ===
    uint32_t cow_ref_count;             /**< Reference count for CoW */
    bool cow_modified;                  /**< True if modified since copy */
    myelin_sheath_t* cow_original;      /**< Original if this is a copy */

    // === THREAD SAFETY ===
    nimcp_spinlock_t lock;              /**< Per-sheath lock */
} myelin_sheath_t;

/**
 * @brief Myelin sheath network configuration
 */
typedef struct {
    uint32_t max_sheaths;               /**< Maximum number of sheaths */
    uint32_t max_segments_per_sheath;   /**< Max segments per sheath */
    float target_g_ratio;               /**< Target g-ratio (default 0.77) */
    float myelination_threshold;        /**< Activity threshold for myelination */
    float damage_threshold;             /**< Integrity threshold for damage state */
    float repair_rate_multiplier;       /**< Multiplier for repair rate */
    bool enable_paranodes;              /**< Enable paranode modeling */
    bool enable_metabolic_coupling;     /**< Enable metabolic interactions */
    bool enable_activity_dependence;    /**< Enable activity-dependent plasticity */
    bool use_memory_pools;              /**< Use pre-allocated memory pools */
    bool enable_bio_async;              /**< Enable bio-async messaging */
} myelin_network_config_t;

/**
 * @brief Myelin sheath network structure
 *
 * WHAT: Network-level management of all myelin sheaths
 * WHY:  Enable efficient queries and batch operations
 * HOW:  Hash tables for O(1) lookup, spatial index, memory pools
 */
typedef struct myelin_sheath_network {
    // === SHEATHS ===
    uint32_t num_sheaths;               /**< Current sheath count */
    uint32_t capacity;                  /**< Maximum capacity */
    myelin_sheath_t** sheaths;          /**< Array of sheath pointers */

    // === INDEXING ===
    void* axon_to_sheath_map;           /**< axon_id -> sheath_id hash table */
    void* oligo_to_sheaths_map;         /**< oligo_id -> sheath_ids[] hash table */
    void* spatial_index;                /**< KD-tree for spatial queries */
    bool spatial_index_valid;           /**< True if spatial index is current */

    // === MEMORY POOLS ===
    myelin_sheath_pool_t* sheath_pool;  /**< Pre-allocated sheaths */
    myelin_segment_pool_t* segment_pool;/**< Pre-allocated segments */

    // === CONFIGURATION ===
    myelin_network_config_t config;     /**< Network configuration */

    // === BIO-ASYNC ===
    void* bio_ctx;                      /**< Bio-async module context */
    bool bio_async_enabled;             /**< Bio-async integration enabled */

    // === STATISTICS ===
    uint64_t total_segments;            /**< Total segment count */
    float mean_network_integrity;       /**< Network-wide mean integrity */
    float mean_network_g_ratio;         /**< Network-wide mean g-ratio */
    uint32_t demyelinating_count;       /**< Sheaths currently demyelinating */
    uint32_t remyelinating_count;       /**< Sheaths currently remyelinating */

    // === TIMING ===
    uint64_t current_time;              /**< Current simulation time (us) */
    uint64_t last_step_time;            /**< Last step timestamp (us) */

    // === THREAD SAFETY ===
    nimcp_mutex_t lock;                 /**< Network-level lock */
} myelin_sheath_network_t;

/**
 * @brief Myelin sheath pool (bitmap-based O(1) allocation)
 */
typedef struct myelin_sheath_pool {
    myelin_sheath_t* buffer;            /**< Pre-allocated array */
    uint64_t* bitmap;                   /**< 1=free, 0=allocated */
    uint32_t capacity;                  /**< Total slots */
    uint32_t num_bitmap_words;          /**< 64-bit words in bitmap */
    uint32_t allocated_count;           /**< Currently allocated */
    nimcp_spinlock_t lock;              /**< Thread-safe allocation */
} myelin_sheath_pool_t;

/**
 * @brief Myelin segment pool (bitmap-based O(1) allocation)
 */
typedef struct myelin_segment_pool {
    myelin_segment_t* buffer;           /**< Pre-allocated array */
    uint64_t* bitmap;                   /**< 1=free, 0=allocated */
    uint32_t capacity;                  /**< Total slots */
    uint32_t num_bitmap_words;          /**< 64-bit words in bitmap */
    uint32_t allocated_count;           /**< Currently allocated */
    nimcp_spinlock_t lock;              /**< Thread-safe allocation */
} myelin_segment_pool_t;

/**
 * @brief Network statistics structure
 */
typedef struct {
    uint32_t total_sheaths;             /**< Total sheaths */
    uint32_t total_segments;            /**< Total segments */
    uint32_t healthy_sheaths;           /**< Healthy sheath count */
    uint32_t damaged_sheaths;           /**< Damaged sheath count */
    uint32_t demyelinating_sheaths;     /**< Demyelinating count */
    uint32_t remyelinating_sheaths;     /**< Remyelinating count */
    float mean_integrity;               /**< Mean integrity */
    float mean_g_ratio;                 /**< Mean g-ratio */
    float mean_velocity_ratio;          /**< Mean velocity ratio */
    float total_myelinated_length_um;   /**< Total myelinated length */
    uint32_t pool_sheaths_allocated;    /**< Pool allocation count */
    uint32_t pool_segments_allocated;   /**< Segment pool count */
} myelin_network_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default network configuration
 *
 * WHAT: Returns sensible defaults for myelin network
 * WHY:  Simplify initialization with biological defaults
 * HOW:  Pre-filled configuration structure
 *
 * @return Default configuration
 */
myelin_network_config_t myelin_network_default_config(void);

//=============================================================================
// Memory Pool Functions
//=============================================================================

/**
 * @brief Create sheath memory pool
 *
 * WHAT: Pre-allocate sheath structures for O(1) allocation
 * WHY:  Avoid malloc overhead in hot paths
 * HOW:  Bitmap-based allocation tracking
 *
 * COMPLEXITY: O(capacity) for creation
 *
 * @param capacity Number of sheaths to pre-allocate
 * @return Pool pointer or NULL on failure
 */
myelin_sheath_pool_t* myelin_sheath_pool_create(uint32_t capacity);

/**
 * @brief Destroy sheath memory pool
 * @param pool Pool to destroy
 */
void myelin_sheath_pool_destroy(myelin_sheath_pool_t* pool);

/**
 * @brief Allocate sheath from pool
 *
 * COMPLEXITY: O(1) average case
 *
 * @param pool Pool to allocate from
 * @return Sheath pointer or NULL if pool exhausted
 */
myelin_sheath_t* myelin_sheath_pool_alloc(myelin_sheath_pool_t* pool);

/**
 * @brief Free sheath back to pool
 * @param pool Pool to return to
 * @param sheath Sheath to free
 */
void myelin_sheath_pool_free(myelin_sheath_pool_t* pool, myelin_sheath_t* sheath);

/**
 * @brief Create segment memory pool
 * @param capacity Number of segments to pre-allocate
 * @return Pool pointer or NULL on failure
 */
myelin_segment_pool_t* myelin_segment_pool_create(uint32_t capacity);

/**
 * @brief Destroy segment memory pool
 */
void myelin_segment_pool_destroy(myelin_segment_pool_t* pool);

/**
 * @brief Allocate segment from pool
 * @param pool Pool to allocate from
 * @return Segment pointer or NULL if exhausted
 */
myelin_segment_t* myelin_segment_pool_alloc(myelin_segment_pool_t* pool);

/**
 * @brief Free segment back to pool
 */
void myelin_segment_pool_free(myelin_segment_pool_t* pool, myelin_segment_t* segment);

//=============================================================================
// Sheath Creation and Destruction
//=============================================================================

/**
 * @brief Create myelin sheath for an axon
 *
 * WHAT: Initialize new myelin sheath structure
 * WHY:  Begin myelination of an axon
 * HOW:  Allocate and initialize with default values
 *
 * @param id Unique sheath ID
 * @param axon_id Target axon ID
 * @param oligo_id Oligodendrocyte ID (producer)
 * @param max_segments Maximum segments capacity
 * @return Sheath pointer or NULL on failure
 */
myelin_sheath_t* myelin_sheath_create(uint32_t id, uint32_t axon_id,
                                       uint32_t oligo_id, uint32_t max_segments);

/**
 * @brief Create sheath with position information
 *
 * @param id Unique sheath ID
 * @param axon_id Target axon ID
 * @param oligo_id Oligodendrocyte ID
 * @param axon_length Total axon length (um)
 * @param axon_diameter Axon diameter (um)
 * @param max_segments Maximum segments
 * @return Sheath pointer or NULL on failure
 */
myelin_sheath_t* myelin_sheath_create_for_axon(uint32_t id, uint32_t axon_id,
                                                uint32_t oligo_id,
                                                float axon_length,
                                                float axon_diameter,
                                                uint32_t max_segments);

/**
 * @brief Destroy myelin sheath
 * @param sheath Sheath to destroy
 */
void myelin_sheath_destroy(myelin_sheath_t* sheath);

//=============================================================================
// Segment Management
//=============================================================================

/**
 * @brief Add segment to sheath
 *
 * WHAT: Create new myelinated segment
 * WHY:  Build up myelin coverage along axon
 * HOW:  Allocate segment and add to sheath array
 *
 * @param sheath Parent sheath
 * @param start_position_um Start position along axon (um)
 * @param length_um Segment length (um)
 * @param axon_diameter Axon diameter at segment (um)
 * @return Segment pointer or NULL on failure
 */
myelin_segment_t* myelin_sheath_add_segment(myelin_sheath_t* sheath,
                                             float start_position_um,
                                             float length_um,
                                             float axon_diameter);

/**
 * @brief Remove segment from sheath
 * @param sheath Parent sheath
 * @param segment_id Segment to remove
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t myelin_sheath_remove_segment(myelin_sheath_t* sheath,
                                             uint32_t segment_id);

/**
 * @brief Get segment by index
 * @param sheath Parent sheath
 * @param index Segment index
 * @return Segment pointer or NULL if invalid
 */
myelin_segment_t* myelin_sheath_get_segment(myelin_sheath_t* sheath, uint32_t index);

/**
 * @brief Find segment containing position
 * @param sheath Sheath to search
 * @param position_um Position along axon (um)
 * @return Segment pointer or NULL if not found
 */
myelin_segment_t* myelin_sheath_find_segment_at(myelin_sheath_t* sheath,
                                                 float position_um);

//=============================================================================
// Structural Properties
//=============================================================================

/**
 * @brief Set lamellae count for segment
 *
 * WHAT: Set number of myelin membrane wraps
 * WHY:  Control myelin thickness
 * HOW:  Update lamellae and recalculate properties
 *
 * @param segment Target segment
 * @param num_lamellae Number of lamellae (1-160)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t myelin_segment_set_lamellae(myelin_segment_t* segment,
                                            uint32_t num_lamellae);

/**
 * @brief Set compaction state
 * @param segment Target segment
 * @param compaction Compaction state
 * @param score Compaction score (0-1)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t myelin_segment_set_compaction(myelin_segment_t* segment,
                                              myelin_compaction_t compaction,
                                              float score);

/**
 * @brief Compute optimal lamellae for g-ratio
 *
 * WHAT: Calculate lamellae count for target g-ratio
 * WHY:  Optimize myelin thickness for conduction
 * HOW:  Inverse of g-ratio formula
 *
 * @param axon_diameter Axon diameter (um)
 * @param target_g_ratio Target g-ratio (0.5-0.95)
 * @return Optimal lamellae count
 */
uint32_t myelin_compute_optimal_lamellae(float axon_diameter, float target_g_ratio);

/**
 * @brief Compute g-ratio from structural properties
 * @param inner_diameter Axon diameter (um)
 * @param num_lamellae Number of lamellae
 * @return G-ratio (inner/outer diameter)
 */
float myelin_compute_g_ratio(float inner_diameter, uint32_t num_lamellae);

/**
 * @brief Compute myelin thickness from lamellae
 * @param num_lamellae Number of lamellae
 * @return Thickness in micrometers
 */
float myelin_compute_thickness(uint32_t num_lamellae);

//=============================================================================
// Conduction Properties
//=============================================================================

/**
 * @brief Compute conduction velocity for segment
 *
 * WHAT: Calculate local conduction velocity
 * WHY:  Determine signal propagation speed
 * HOW:  Hursh's law with g-ratio efficiency
 *
 * BIOLOGICAL BASIS:
 * v = k * d * g_efficiency * myelination_factor
 * k = 6.0 m/s per um (myelinated)
 * k = 1.0 m/s per sqrt(um) (unmyelinated)
 *
 * @param segment Target segment
 * @return Velocity in m/s
 */
float myelin_segment_compute_velocity(const myelin_segment_t* segment);

/**
 * @brief Compute propagation delay for segment
 * @param segment Target segment
 * @return Delay in milliseconds
 */
float myelin_segment_compute_delay(const myelin_segment_t* segment);

/**
 * @brief Update sheath conduction properties
 *
 * Recalculates effective velocity and total delay
 *
 * @param sheath Sheath to update
 */
void myelin_sheath_update_conduction(myelin_sheath_t* sheath);

/**
 * @brief Get velocity ratio vs unmyelinated
 * @param sheath Sheath to query
 * @return Velocity ratio (1.0 = no benefit, 50.0 = max benefit)
 */
float myelin_sheath_get_velocity_ratio(const myelin_sheath_t* sheath);

//=============================================================================
// Health and Integrity
//=============================================================================

/**
 * @brief Apply damage to segment
 *
 * WHAT: Accumulate damage on segment
 * WHY:  Model demyelination and pathology
 * HOW:  Reduce integrity, transition health states
 *
 * @param segment Target segment
 * @param damage_amount Damage to apply (0-1)
 * @param current_time Current simulation time (us)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t myelin_segment_apply_damage(myelin_segment_t* segment,
                                            float damage_amount,
                                            uint64_t current_time);

/**
 * @brief Repair segment damage
 *
 * @param segment Target segment
 * @param repair_amount Repair amount (0-1)
 * @param current_time Current time (us)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t myelin_segment_repair(myelin_segment_t* segment,
                                      float repair_amount,
                                      uint64_t current_time);

/**
 * @brief Update segment health state
 *
 * Transitions health state based on integrity
 *
 * @param segment Target segment
 * @param current_time Current time (us)
 */
void myelin_segment_update_health(myelin_segment_t* segment, uint64_t current_time);

/**
 * @brief Update sheath aggregate health
 * @param sheath Sheath to update
 */
void myelin_sheath_update_health(myelin_sheath_t* sheath);

/**
 * @brief Get health state string
 * @param state Health state
 * @return String representation
 */
const char* myelin_health_state_to_string(myelin_health_state_t state);

//=============================================================================
// Paranodal Junctions
//=============================================================================

/**
 * @brief Set paranode state
 *
 * WHAT: Update paranodal junction state
 * WHY:  Paranodes seal myelin and cluster channels
 * HOW:  State machine for junction formation/disruption
 *
 * @param segment Target segment
 * @param proximal Proximal paranode state
 * @param distal Distal paranode state
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t myelin_segment_set_paranodes(myelin_segment_t* segment,
                                             paranode_state_t proximal,
                                             paranode_state_t distal);

/**
 * @brief Update paranode integrity
 * @param segment Target segment
 * @param integrity Integrity score (0-1)
 */
void myelin_segment_update_paranode_integrity(myelin_segment_t* segment,
                                               float integrity);

/**
 * @brief Check if paranodes are functional
 * @param segment Segment to check
 * @return true if both paranodes are mature
 */
bool myelin_segment_paranodes_functional(const myelin_segment_t* segment);

//=============================================================================
// Metabolic Support
//=============================================================================

/**
 * @brief Update segment metabolic state
 *
 * WHAT: Update ATP and trophic support
 * WHY:  Metabolic health affects myelin integrity
 * HOW:  Consume ATP, receive lactate
 *
 * @param segment Target segment
 * @param dt Time step (seconds)
 */
void myelin_segment_update_metabolism(myelin_segment_t* segment, float dt);

/**
 * @brief Receive lactate from oligodendrocyte
 * @param segment Target segment
 * @param lactate_amount Lactate amount (mM)
 */
void myelin_segment_receive_lactate(myelin_segment_t* segment, float lactate_amount);

/**
 * @brief Set trophic support level
 * @param segment Target segment
 * @param trophic_level Support level (0-1)
 */
void myelin_segment_set_trophic_support(myelin_segment_t* segment, float trophic_level);

/**
 * @brief Check if segment is metabolically healthy
 * @param segment Segment to check
 * @return true if ATP and trophic support adequate
 */
bool myelin_segment_metabolically_healthy(const myelin_segment_t* segment);

//=============================================================================
// Dynamics and Simulation
//=============================================================================

/**
 * @brief Step segment dynamics forward
 *
 * WHAT: Update segment for one time step
 * WHY:  Simulate myelin dynamics
 * HOW:  Update metabolism, health, compaction
 *
 * @param segment Target segment
 * @param dt Time step (seconds)
 * @param current_time Current time (us)
 */
void myelin_segment_step(myelin_segment_t* segment, float dt, uint64_t current_time);

/**
 * @brief Step sheath dynamics forward
 *
 * Updates all segments and aggregate properties
 *
 * @param sheath Target sheath
 * @param dt Time step (seconds)
 * @param current_time Current time (us)
 */
void myelin_sheath_step(myelin_sheath_t* sheath, float dt, uint64_t current_time);

/**
 * @brief Trigger myelination process
 *
 * WHAT: Increase lamellae count over time
 * WHY:  Model active myelination
 * HOW:  Gradual lamellae addition based on rate
 *
 * @param sheath Target sheath
 * @param rate Lamellae addition rate per second
 * @param dt Time step (seconds)
 */
void myelin_sheath_myelinate(myelin_sheath_t* sheath, float rate, float dt);

/**
 * @brief Trigger demyelination process
 *
 * @param sheath Target sheath
 * @param rate Lamellae removal rate per second
 * @param dt Time step (seconds)
 */
void myelin_sheath_demyelinate(myelin_sheath_t* sheath, float rate, float dt);

//=============================================================================
// Copy-on-Write Support
//=============================================================================

/**
 * @brief Create shallow copy of sheath (CoW)
 *
 * WHAT: Create reference-counted copy
 * WHY:  Efficient cloning without immediate duplication
 * HOW:  Share data until modification
 *
 * @param sheath Sheath to copy
 * @return New sheath reference or NULL on failure
 */
myelin_sheath_t* myelin_sheath_cow_copy(myelin_sheath_t* sheath);

/**
 * @brief Prepare sheath for write (deep copy if shared)
 *
 * @param sheath Sheath to prepare
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t myelin_sheath_cow_prepare_write(myelin_sheath_t* sheath);

/**
 * @brief Release CoW reference
 * @param sheath Sheath to release
 */
void myelin_sheath_cow_release(myelin_sheath_t* sheath);

/**
 * @brief Check if sheath is a CoW copy
 * @param sheath Sheath to check
 * @return true if this is a shallow copy
 */
bool myelin_sheath_is_cow_copy(const myelin_sheath_t* sheath);

/**
 * @brief Get CoW reference count
 * @param sheath Sheath to query
 * @return Reference count
 */
uint32_t myelin_sheath_cow_ref_count(const myelin_sheath_t* sheath);

//=============================================================================
// Network Management
//=============================================================================

/**
 * @brief Create myelin sheath network
 *
 * WHAT: Initialize network-level myelin management
 * WHY:  Efficient queries and batch operations
 * HOW:  Hash tables, spatial index, memory pools
 *
 * @param config Network configuration
 * @return Network pointer or NULL on failure
 */
myelin_sheath_network_t* myelin_network_create(const myelin_network_config_t* config);

/**
 * @brief Create network with default configuration
 * @param capacity Maximum sheath capacity
 * @return Network pointer or NULL on failure
 */
myelin_sheath_network_t* myelin_network_create_default(uint32_t capacity);

/**
 * @brief Destroy myelin network
 * @param network Network to destroy
 */
void myelin_network_destroy(myelin_sheath_network_t* network);

/**
 * @brief Add sheath to network
 * @param network Target network
 * @param sheath Sheath to add
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t myelin_network_add_sheath(myelin_sheath_network_t* network,
                                          myelin_sheath_t* sheath);

/**
 * @brief Remove sheath from network
 * @param network Target network
 * @param sheath_id Sheath ID to remove
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t myelin_network_remove_sheath(myelin_sheath_network_t* network,
                                             uint32_t sheath_id);

/**
 * @brief Find sheath by ID
 * @param network Network to search
 * @param sheath_id Sheath ID
 * @return Sheath pointer or NULL if not found
 */
myelin_sheath_t* myelin_network_find_sheath(myelin_sheath_network_t* network,
                                             uint32_t sheath_id);

/**
 * @brief Find sheath for axon
 * @param network Network to search
 * @param axon_id Axon ID
 * @return Sheath pointer or NULL if not found
 */
myelin_sheath_t* myelin_network_find_by_axon(myelin_sheath_network_t* network,
                                              uint32_t axon_id);

/**
 * @brief Find sheaths by oligodendrocyte
 * @param network Network to search
 * @param oligo_id Oligodendrocyte ID
 * @param results Output array
 * @param max_results Maximum results
 * @return Number of sheaths found
 */
uint32_t myelin_network_find_by_oligo(myelin_sheath_network_t* network,
                                       uint32_t oligo_id,
                                       myelin_sheath_t** results,
                                       uint32_t max_results);

/**
 * @brief Step network forward
 *
 * Updates all sheaths for one time step
 *
 * @param network Target network
 * @param dt Time step (seconds)
 * @param current_time Current time (us)
 */
void myelin_network_step(myelin_sheath_network_t* network,
                          float dt, uint64_t current_time);

/**
 * @brief Get network statistics
 * @param network Network to query
 * @param stats Output statistics
 */
void myelin_network_get_stats(const myelin_sheath_network_t* network,
                               myelin_network_stats_t* stats);

/**
 * @brief Rebuild spatial index
 * @param network Network to rebuild
 */
void myelin_network_rebuild_spatial_index(myelin_sheath_network_t* network);

//=============================================================================
// Integration Helpers
//=============================================================================

/**
 * @brief Create sheath for axon with automatic segmentation
 *
 * WHAT: Create fully segmented sheath for axon
 * WHY:  Simplify integration with axon module
 * HOW:  Calculate optimal segments based on diameter
 *
 * @param network Target network
 * @param axon_id Axon ID
 * @param oligo_id Oligodendrocyte ID
 * @param axon_length Axon length (um)
 * @param axon_diameter Axon diameter (um)
 * @param start_position Starting position along axon (um)
 * @return Created sheath or NULL on failure
 */
myelin_sheath_t* myelin_network_create_sheath_for_axon(
    myelin_sheath_network_t* network,
    uint32_t axon_id,
    uint32_t oligo_id,
    float axon_length,
    float axon_diameter,
    float start_position);

/**
 * @brief Get myelination factor for axon
 *
 * WHAT: Get overall myelination level for conduction
 * WHY:  Interface with axon/neuron modules
 * HOW:  Weighted average of segment integrity and coverage
 *
 * @param network Network to query
 * @param axon_id Axon ID
 * @return Myelination factor (0-1)
 */
float myelin_network_get_myelination_factor(myelin_sheath_network_t* network,
                                             uint32_t axon_id);

/**
 * @brief Get conduction velocity for axon
 * @param network Network to query
 * @param axon_id Axon ID
 * @return Effective velocity (m/s)
 */
float myelin_network_get_velocity(myelin_sheath_network_t* network,
                                   uint32_t axon_id);

/**
 * @brief Get propagation delay for axon
 * @param network Network to query
 * @param axon_id Axon ID
 * @return Total delay (ms)
 */
float myelin_network_get_delay(myelin_sheath_network_t* network,
                                uint32_t axon_id);

/**
 * @brief Apply activity-dependent plasticity
 *
 * WHAT: Adjust myelination based on axon activity
 * WHY:  Activity-dependent myelination is biological
 * HOW:  Increase lamellae on active axons
 *
 * @param network Target network
 * @param axon_id Axon ID
 * @param activity_level Activity level (Hz normalized)
 * @param dt Time step (seconds)
 */
void myelin_network_apply_activity(myelin_sheath_network_t* network,
                                    uint32_t axon_id,
                                    float activity_level,
                                    float dt);

//=============================================================================
// Enhanced Biophysics Functions (from nimcp_myelin_math.h integration)
//=============================================================================

/**
 * @brief Compute segment velocity with full biophysics
 *
 * WHAT: Calculate velocity using enhanced cable theory and saltatory conduction
 * WHY:  More accurate than simple Hursh's law approximation
 * HOW:  Uses all 8 mathematical models from nimcp_myelin_math.h
 *
 * @param segment Target segment
 * @return Velocity in m/s (0 if blocked)
 */
float myelin_segment_compute_velocity_enhanced(myelin_segment_t* segment);

/**
 * @brief Update segment cable parameters
 *
 * WHAT: Recalculate space constant λ and time constant τ
 * WHY:  Cable parameters affect passive signal propagation
 * HOW:  Uses cable theory from nimcp_myelin_math.h
 *
 * @param segment Target segment
 */
void myelin_segment_update_cable_params(myelin_segment_t* segment);

/**
 * @brief Compute optimal g-ratio for segment
 *
 * WHAT: Calculate diameter-dependent optimal g-ratio
 * WHY:  Smaller axons have different optimal myelination
 * HOW:  Rushton model with exponential diameter correction
 *
 * @param segment Target segment
 * @return Optimal g-ratio for this diameter
 */
float myelin_segment_compute_optimal_g_ratio(myelin_segment_t* segment);

/**
 * @brief Compute optimal internode length for segment
 *
 * WHAT: Calculate ideal segment length for diameter
 * WHY:  Internode length affects velocity and efficiency
 * HOW:  Power-law relationship from biology
 *
 * @param segment Target segment
 * @return Optimal internode length (um)
 */
float myelin_segment_compute_optimal_internode(myelin_segment_t* segment);

/**
 * @brief Check for conduction block
 *
 * WHAT: Determine if signal can propagate through segment
 * WHY:  Model pathological conditions (MS, temperature effects)
 * HOW:  Sigmoid probability with temperature modulation
 *
 * @param segment Target segment
 * @param temperature_c Temperature in Celsius
 * @return true if blocked, false if conducting
 */
bool myelin_segment_check_block(myelin_segment_t* segment, float temperature_c);

/**
 * @brief Get conduction block probability
 *
 * @param segment Target segment
 * @param temperature_c Temperature in Celsius
 * @return Block probability (0-1)
 */
float myelin_segment_get_block_probability(myelin_segment_t* segment, float temperature_c);

/**
 * @brief Apply activity-dependent myelination to segment
 *
 * WHAT: Adjust lamellae based on neural activity
 * WHY:  Activity-dependent plasticity is biological
 * HOW:  Hill-function kinetics with saturation
 *
 * @param segment Target segment
 * @param activity Activity level (0-1 normalized)
 * @param dt Time step (seconds)
 * @return Change in lamellae (can be fractional)
 */
float myelin_segment_apply_activity_plasticity(myelin_segment_t* segment,
                                                float activity,
                                                float dt);

/**
 * @brief Initialize biophysics for sheath
 *
 * WHAT: Create and attach biophysics state
 * WHY:  Enable enhanced mathematical modeling
 * HOW:  Allocate and initialize nimcp_myelin_biophysics_t
 *
 * @param sheath Target sheath
 * @param use_stochastic Enable stochastic variability
 * @param seed Random seed (0 for time-based)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t myelin_sheath_init_biophysics(myelin_sheath_t* sheath,
                                              bool use_stochastic,
                                              uint64_t seed);

/**
 * @brief Set temperature for conduction block modeling
 *
 * @param sheath Target sheath
 * @param temperature_c Temperature in Celsius
 */
void myelin_sheath_set_temperature(myelin_sheath_t* sheath, float temperature_c);

/**
 * @brief Compute metabolic efficiency for sheath
 *
 * WHAT: Calculate energy costs and efficiency ratio
 * WHY:  Quantify metabolic benefit of myelination
 * HOW:  Compare myelinated vs unmyelinated energy costs
 *
 * @param sheath Target sheath
 */
void myelin_sheath_compute_metabolic_efficiency(myelin_sheath_t* sheath);

/**
 * @brief Get ATP cost per action potential
 *
 * @param sheath Sheath to query
 * @return ATP molecules per AP
 */
float myelin_sheath_get_atp_per_ap(const myelin_sheath_t* sheath);

/**
 * @brief Get energy efficiency ratio
 *
 * @param sheath Sheath to query
 * @return Efficiency ratio vs unmyelinated (>1 means more efficient)
 */
float myelin_sheath_get_efficiency_ratio(const myelin_sheath_t* sheath);

/**
 * @brief Update all enhanced biophysics for sheath
 *
 * WHAT: Recalculate all biophysics parameters for all segments
 * WHY:  Keep cached calculations current after changes
 * HOW:  Iterate segments and update cable, velocity, block, etc.
 *
 * @param sheath Target sheath
 */
void myelin_sheath_update_biophysics(myelin_sheath_t* sheath);

/**
 * @brief Apply stochastic variability to sheath
 *
 * WHAT: Add biological variability to structural parameters
 * WHY:  Real myelin has natural variation
 * HOW:  Log-normal and normal distributions
 *
 * @param sheath Target sheath
 */
void myelin_sheath_apply_variability(myelin_sheath_t* sheath);

/**
 * @brief Get frequency-dependent block threshold
 *
 * WHAT: Calculate minimum integrity for given stimulation frequency
 * WHY:  High-frequency conduction fails first
 * HOW:  Inverse of block probability with frequency factor
 *
 * @param sheath Target sheath
 * @param frequency_hz Stimulation frequency (Hz)
 * @return Minimum integrity for reliable conduction
 */
float myelin_sheath_get_frequency_threshold(const myelin_sheath_t* sheath,
                                             float frequency_hz);

/**
 * @brief Get detailed saltatory conduction result
 *
 * WHAT: Get breakdown of velocity calculation
 * WHY:  Debugging and analysis of conduction
 * HOW:  Access cached saltatory result
 *
 * @param segment Target segment
 * @param result Output result structure
 */
void myelin_segment_get_saltatory_result(const myelin_segment_t* segment,
                                          nimcp_saltatory_result_t* result);

/**
 * @brief Get cable theory parameters
 *
 * @param segment Target segment
 * @param params Output cable parameters
 */
void myelin_segment_get_cable_params(const myelin_segment_t* segment,
                                      nimcp_cable_params_t* params);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MYELIN_SHEATH_H
