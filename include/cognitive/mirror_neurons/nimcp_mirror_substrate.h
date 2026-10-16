//=============================================================================
// nimcp_mirror_substrate.h - Mirror Neuron Substrate Integration
//=============================================================================
/**
 * @file nimcp_mirror_substrate.h
 * @brief Substrate-level integration for mirror neurons
 *
 * WHAT: Bridges cognitive mirror neurons with biological substrate
 * WHY:  Enable biologically-realistic mirror neuron behavior including:
 *       - Myelination-dependent recognition speed
 *       - Dendritic spine plasticity for association learning
 *       - Glial cell modulation (astrocytes, oligodendrocytes, microglia)
 *       - Axon-based signal propagation timing
 * HOW:  Provides substrate backing for mirror_neuron_unit_t with optional
 *       integration to axon, dendrite, myelin sheath, and glial networks
 *
 * BIOLOGICAL BASIS:
 * - Mirror neurons located in F5 premotor cortex and inferior parietal lobule
 * - Dense myelination in cortico-cortical connections for fast signaling
 * - Dendritic spines encode observation-execution associations
 * - Astrocytes modulate plasticity via Ca2+ signaling
 * - Oligodendrocytes provide metabolic support and conduction speed
 * - Microglia prune weak/unused associations
 *
 * ARCHITECTURE:
 * - SRP: Substrate concerns separated from cognitive function
 * - Factory Pattern: Create substrate backing on demand
 * - Strategy Pattern: Optional substrate modes (abstract, backed, full)
 * - Observer Pattern: Activity callbacks for plasticity
 *
 * PERFORMANCE:
 * - Memory pool: O(1) allocation for substrate data
 * - CoW support: Efficient cloning for branching scenarios
 * - Lazy binding: Substrate only created when enabled
 *
 * INTEGRATION POINTS:
 * - nimcp_mirror_neurons.c: Main mirror neuron system
 * - nimcp_axon.h: Action potential propagation
 * - nimcp_dendrite.h: Synaptic input integration
 * - nimcp_myelin_sheath.h: Myelination structural modeling
 * - nimcp_glial_integration.h: Glial cell coordination
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-11-25
 */

#ifndef NIMCP_MIRROR_SUBSTRATE_H
#define NIMCP_MIRROR_SUBSTRATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/validation/nimcp_common.h"
#include "utils/thread/nimcp_thread.h"

//=============================================================================
// Forward Declarations (opaque pointers for external types)
//=============================================================================

/**
 * WHAT: Opaque handle types for substrate integration
 * WHY:  Avoid type conflicts with actual header definitions
 * HOW:  Use void* internally, cast to proper types in implementation
 *
 * NOTE: The actual header files (nimcp_axon.h, nimcp_dendrite.h,
 *       nimcp_myelin_sheath.h, nimcp_glial_integration.h) are included
 *       only in the .c implementation file to prevent type conflicts.
 */

/* Mirror neuron system handle (forward declaration) */
typedef struct mirror_neurons_system* mirror_neurons_t;

/* Substrate module types (our own types - fully defined here) */
typedef struct mirror_substrate_backing_struct mirror_substrate_backing_t;
typedef struct mirror_substrate_pool_struct mirror_substrate_pool_t;
typedef struct mirror_substrate_config_struct mirror_substrate_config_t;
typedef struct mirror_substrate_stats_struct mirror_substrate_stats_t;

//=============================================================================
// Constants
//=============================================================================

/** @name Pool Constants */
///@{
#define NIMCP_MIRROR_SUBSTRATE_POOL_SIZE       4096   /**< Pre-allocated backings */
#define NIMCP_MIRROR_SUBSTRATE_POOL_BLOCK_SIZE 64     /**< Bitmap block size */
#define NIMCP_MIRROR_SUBSTRATE_MAX_SPINES      32     /**< Max spines per mirror unit */
#define NIMCP_MIRROR_SUBSTRATE_MAX_AXONS       4      /**< Max axons per mirror unit */
///@}

/** @name Timing Constants */
///@{
#define NIMCP_MIRROR_BASE_DELAY_MS             10.0f  /**< Base recognition delay (ms) */
#define NIMCP_MIRROR_MYELIN_SPEEDUP_MAX        10.0f  /**< Max myelination speedup factor */
#define NIMCP_MIRROR_DENDRITE_INTEGRATION_MS   5.0f   /**< Dendritic integration time (ms) */
///@}

/** @name Plasticity Constants */
///@{
#define NIMCP_MIRROR_SPINE_LTP_THRESHOLD       0.7f   /**< LTP threshold for spine growth */
#define NIMCP_MIRROR_SPINE_LTD_THRESHOLD       0.3f   /**< LTD threshold for spine shrinkage */
#define NIMCP_MIRROR_ASTROCYTE_MOD_MIN         0.5f   /**< Min astrocyte modulation factor */
#define NIMCP_MIRROR_ASTROCYTE_MOD_MAX         2.0f   /**< Max astrocyte modulation factor */
#define NIMCP_MIRROR_MICROGLIA_PRUNE_THRESHOLD 0.1f   /**< Activity threshold for pruning */
///@}

/** @name Biological Region IDs */
///@{
#define NIMCP_MIRROR_REGION_F5_PREMOTOR        0x0501 /**< F5 premotor cortex */
#define NIMCP_MIRROR_REGION_PARIETAL_IPL       0x0702 /**< Inferior parietal lobule */
#define NIMCP_MIRROR_REGION_STS                0x0603 /**< Superior temporal sulcus */
///@}

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Substrate integration mode
 *
 * WHAT: Determines level of substrate integration for mirror neurons
 * WHY:  Allow performance/realism tradeoff selection
 */
typedef enum {
    MIRROR_SUBSTRATE_MODE_ABSTRACT = 0,  /**< No substrate (fastest, cognitive only) */
    MIRROR_SUBSTRATE_MODE_TIMING   = 1,  /**< Timing only (delays from myelination) */
    MIRROR_SUBSTRATE_MODE_PARTIAL  = 2,  /**< Timing + spine plasticity */
    MIRROR_SUBSTRATE_MODE_FULL     = 3   /**< Full substrate (neurons, synapses, glial) */
} mirror_substrate_mode_t;

/**
 * @brief Spine plasticity state for observation-execution associations
 *
 * WHAT: Tracks plasticity state of dendritic spines encoding associations
 * WHY:  Model activity-dependent structural plasticity
 */
typedef enum {
    MIRROR_SPINE_STATE_THIN     = 0,  /**< Learning spine (small, highly plastic) */
    MIRROR_SPINE_STATE_STUBBY   = 1,  /**< Intermediate state */
    MIRROR_SPINE_STATE_MUSHROOM = 2,  /**< Memory spine (large, stable) */
    MIRROR_SPINE_STATE_PRUNED   = 3   /**< Marked for removal by microglia */
} mirror_spine_state_t;

/**
 * @brief Glial modulation type
 *
 * WHAT: Type of glial cell modulating mirror neuron activity
 * WHY:  Track different modulation sources
 */
typedef enum {
    MIRROR_GLIAL_NONE          = 0,  /**< No glial modulation */
    MIRROR_GLIAL_ASTROCYTE     = 1,  /**< Astrocyte Ca2+ modulation */
    MIRROR_GLIAL_OLIGODENDRO   = 2,  /**< Oligodendrocyte myelination */
    MIRROR_GLIAL_MICROGLIA     = 3   /**< Microglia pruning/surveillance */
} mirror_glial_type_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Configuration for substrate integration
 *
 * WHAT: Configuration parameters for mirror neuron substrate
 * WHY:  Allow customization of substrate behavior
 */
struct mirror_substrate_config_struct {
    /* Mode selection */
    mirror_substrate_mode_t mode;          /**< Integration mode */

    /* Feature flags */
    bool enable_myelination;               /**< Use myelin sheath for timing */
    bool enable_dendrites;                 /**< Use dendritic spine plasticity */
    bool enable_axons;                     /**< Use axon propagation delays */
    bool enable_astrocytes;                /**< Astrocyte plasticity modulation */
    bool enable_oligodendrocytes;          /**< Oligodendrocyte metabolic support */
    bool enable_microglia;                 /**< Microglia pruning */
    bool enable_memory_pool;               /**< Use memory pool for allocations */
    bool enable_cow;                       /**< Enable copy-on-write */

    /* Timing parameters */
    float base_recognition_delay_ms;       /**< Base delay without substrate */
    float myelin_speedup_factor;           /**< Max speedup from myelination */
    float dendrite_integration_time_ms;    /**< Dendritic integration window */

    /* Plasticity parameters */
    float spine_ltp_threshold;             /**< Threshold for spine growth */
    float spine_ltd_threshold;             /**< Threshold for spine shrinkage */
    float spine_maturation_rate;           /**< Rate of spine type transitions */
    float pruning_activity_threshold;      /**< Min activity to avoid pruning */

    /* Brain region assignment */
    uint32_t primary_region_id;            /**< Primary brain region (F5/parietal) */
    uint32_t secondary_region_id;          /**< Secondary region for projections */

    /* Pool sizing */
    uint32_t pool_capacity;                /**< Substrate backing pool size */
};

/**
 * @brief Substrate backing for a single mirror neuron unit
 *
 * WHAT: Biological substrate data for one mirror neuron
 * WHY:  Enable biologically-realistic behavior per neuron
 *
 * DESIGN: Separates substrate concerns from cognitive mirror_neuron_unit_t
 */
struct mirror_substrate_backing_struct {
    /* Identity */
    uint32_t mirror_unit_id;               /**< ID of associated mirror_neuron_unit */
    uint32_t substrate_id;                 /**< Unique substrate backing ID */

    /* Substrate neuron reference (optional) */
    void* substrate_neuron;                /**< Backing neural substrate (NULL if abstract) */
    uint32_t neuron_id;                    /**< Substrate neuron ID (0 if abstract) */

    /* Axon integration (observation pathway) */
    uint32_t observation_axon_id;          /**< Axon for observation input */
    void* observation_axon;                /**< Observation axon pointer */
    float observation_delay_ms;            /**< Cached observation delay */

    /* Axon integration (execution pathway) */
    uint32_t execution_axon_id;            /**< Axon for execution output */
    void* execution_axon;                  /**< Execution axon pointer */
    float execution_delay_ms;              /**< Cached execution delay */

    /* Myelin sheath integration */
    uint32_t myelin_sheath_id;             /**< Myelin sheath ID (0 if unmyelinated) */
    void* myelin_sheath;                   /**< Myelin sheath pointer */
    float myelination_level;               /**< Current myelination (0-1) */
    float conduction_velocity_ms;          /**< Conduction velocity (m/s) */

    /* Dendrite integration */
    uint32_t dendrite_id;                  /**< Primary dendrite ID */
    void* dendrite;                        /**< Dendrite pointer */

    /* Dendritic spines (observation-execution associations) */
    uint32_t num_spines;                   /**< Number of active spines */
    uint32_t spine_ids[NIMCP_MIRROR_SUBSTRATE_MAX_SPINES];    /**< Spine IDs */
    void* spines[NIMCP_MIRROR_SUBSTRATE_MAX_SPINES];          /**< Spine pointers */
    mirror_spine_state_t spine_states[NIMCP_MIRROR_SUBSTRATE_MAX_SPINES]; /**< Spine states */
    float spine_weights[NIMCP_MIRROR_SUBSTRATE_MAX_SPINES];   /**< Spine synaptic weights */

    /* Glial cell assignments */
    uint32_t astrocyte_id;                 /**< Assigned astrocyte (0 = none) */
    void* astrocyte;                       /**< Astrocyte pointer */
    float astrocyte_modulation;            /**< Current modulation factor (0.5-2.0) */

    uint32_t oligodendrocyte_id;           /**< Assigned oligodendrocyte (0 = none) */
    void* oligodendrocyte;                 /**< Oligodendrocyte pointer */
    float lactate_received;                /**< Metabolic support level */

    uint32_t microglia_id;                 /**< Assigned microglia (0 = none) */
    void* microglia;                       /**< Microglia pointer */
    float surveillance_score;              /**< Microglia activity monitoring */
    bool marked_for_pruning;               /**< True if low activity detected */

    /* Brain region placement */
    uint32_t brain_region_id;              /**< Brain region containing this unit */
    float position[3];                     /**< 3D position in brain (um) */

    /* Activity tracking for plasticity */
    float observation_activity_ema;        /**< Observation activity (EMA) */
    float execution_activity_ema;          /**< Execution activity (EMA) */
    float coactivation_score;              /**< Obs-exec coactivation strength */
    uint64_t last_observation_time;        /**< Last observation timestamp (us) */
    uint64_t last_execution_time;          /**< Last execution timestamp (us) */
    uint64_t last_coactivation_time;       /**< Last coactivation timestamp (us) */

    /* Copy-on-Write support */
    uint32_t cow_ref_count;                /**< Reference count for CoW */
    bool cow_modified;                     /**< True if modified since copy */
    mirror_substrate_backing_t* cow_original; /**< Original if this is a copy */

    /* Thread safety */
    nimcp_spinlock_t lock;                 /**< Per-backing spinlock */
};

/**
 * @brief Memory pool for substrate backings
 *
 * WHAT: Pre-allocated pool of substrate backing slots
 * WHY:  O(1) allocation during mirror neuron operations
 * HOW:  Bitmap-based allocation with thread-safe access
 */
struct mirror_substrate_pool_struct {
    mirror_substrate_backing_t* buffer;    /**< Pre-allocated backing array */
    uint64_t* bitmap;                      /**< Allocation bitmap (1 = free) */
    uint32_t capacity;                     /**< Total pool capacity */
    uint32_t num_bitmap_words;             /**< Number of 64-bit bitmap words */
    uint32_t allocated_count;              /**< Currently allocated slots */
    uint32_t next_search_pos;              /**< Next position to search */
    uint32_t high_water_mark;              /**< Peak allocations */
    nimcp_spinlock_t lock;                 /**< Thread-safe access */
};

/**
 * @brief Statistics for substrate integration
 *
 * WHAT: Runtime statistics for substrate operations
 * WHY:  Monitor performance and integration health
 */
struct mirror_substrate_stats_struct {
    /* Counts */
    uint32_t total_backings;               /**< Total substrate backings created */
    uint32_t active_backings;              /**< Currently active backings */
    uint32_t myelinated_count;             /**< Backings with myelin */
    uint32_t with_dendrites;               /**< Backings with dendrite integration */
    uint32_t with_astrocytes;              /**< Backings with astrocyte modulation */
    uint32_t with_oligodendrocytes;        /**< Backings with oligo support */
    uint32_t with_microglia;               /**< Backings under microglia surveillance */

    /* Pool statistics */
    uint32_t pool_capacity;                /**< Pool total capacity */
    uint32_t pool_allocated;               /**< Pool slots in use */
    uint32_t pool_high_water;              /**< Pool peak usage */
    uint32_t pool_alloc_failures;          /**< Failed allocations */

    /* Timing statistics */
    float avg_recognition_delay_ms;        /**< Average recognition delay */
    float min_recognition_delay_ms;        /**< Minimum delay (best myelination) */
    float max_recognition_delay_ms;        /**< Maximum delay (unmyelinated) */
    float avg_myelination_level;           /**< Average myelination (0-1) */

    /* Plasticity statistics */
    uint32_t total_spines;                 /**< Total dendritic spines */
    uint32_t thin_spines;                  /**< Learning spines */
    uint32_t stubby_spines;                /**< Intermediate spines */
    uint32_t mushroom_spines;              /**< Memory spines */
    uint32_t pruned_spines;                /**< Spines marked for pruning */
    float avg_spine_weight;                /**< Average spine synaptic weight */

    /* Glial statistics */
    float avg_astrocyte_modulation;        /**< Average astrocyte modulation */
    float avg_lactate_support;             /**< Average metabolic support */
    uint32_t microglia_pruning_candidates; /**< Units marked for pruning */

    /* CoW statistics */
    uint32_t cow_copies_created;           /**< CoW copies made */
    uint32_t cow_deep_copies_triggered;    /**< Deep copies on write */
};

//=============================================================================
// Pool Management API
//=============================================================================

/**
 * @brief Create substrate backing memory pool
 *
 * WHAT: Allocate pre-sized pool for substrate backings
 * WHY:  O(1) allocation during mirror neuron operations
 * HOW:  Allocate contiguous buffer with bitmap tracking
 *
 * @param capacity Number of slots to pre-allocate (rounded to 64)
 * @return Pool handle or NULL on failure
 *
 * COMPLEXITY: O(capacity)
 * THREAD-SAFE: Yes (creates new instance)
 */
mirror_substrate_pool_t* mirror_substrate_pool_create(uint32_t capacity);

/**
 * @brief Destroy substrate backing memory pool
 *
 * WHAT: Free pool and all contained backings
 * WHY:  Clean resource management
 * HOW:  Free buffer and bitmap, then pool structure
 *
 * @param pool Pool to destroy (NULL-safe)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must ensure exclusive access)
 */
void mirror_substrate_pool_destroy(mirror_substrate_pool_t* pool);

/**
 * @brief Allocate substrate backing from pool
 *
 * WHAT: Get a substrate backing slot from pool
 * WHY:  Fast O(1) allocation without malloc
 * HOW:  Bitmap scan for free slot
 *
 * @param pool Pool to allocate from
 * @return Pointer to backing or NULL if pool exhausted
 *
 * COMPLEXITY: O(1) average, O(capacity/64) worst case
 * THREAD-SAFE: Yes (internal spinlock)
 */
mirror_substrate_backing_t* mirror_substrate_pool_alloc(mirror_substrate_pool_t* pool);

/**
 * @brief Return substrate backing to pool
 *
 * WHAT: Release backing slot back to pool
 * WHY:  Enable slot reuse
 * HOW:  Clear backing, set bitmap bit
 *
 * @param pool Pool to return to
 * @param backing Backing to return
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (internal spinlock)
 */
void mirror_substrate_pool_free(mirror_substrate_pool_t* pool,
                                 mirror_substrate_backing_t* backing);

/**
 * @brief Get pool statistics
 *
 * WHAT: Query pool usage statistics
 * WHY:  Monitor pool health and sizing
 *
 * @param pool Pool to query
 * @param allocated Output: allocated slots
 * @param capacity Output: total capacity
 * @param high_water Output: peak usage
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (atomic reads)
 */
void mirror_substrate_pool_stats(const mirror_substrate_pool_t* pool,
                                  uint32_t* allocated,
                                  uint32_t* capacity,
                                  uint32_t* high_water);

//=============================================================================
// Substrate Backing Lifecycle API
//=============================================================================

/**
 * @brief Create substrate backing for mirror neuron unit
 *
 * WHAT: Create biological substrate data for a mirror neuron
 * WHY:  Enable substrate-level behavior for the unit
 * HOW:  Allocate from pool or heap, initialize fields
 *
 * @param mirror_unit_id ID of the mirror_neuron_unit
 * @param config Substrate configuration
 * @param pool Memory pool (NULL = use heap)
 * @return Substrate backing or NULL on failure
 *
 * COMPLEXITY: O(1) with pool, O(1) heap
 * THREAD-SAFE: Yes with pool, Yes for heap
 */
mirror_substrate_backing_t* mirror_substrate_backing_create(
    uint32_t mirror_unit_id,
    const mirror_substrate_config_t* config,
    mirror_substrate_pool_t* pool);

/**
 * @brief Destroy substrate backing
 *
 * WHAT: Release substrate backing resources
 * WHY:  Clean resource management
 * HOW:  Return to pool or free to heap
 *
 * @param backing Backing to destroy (NULL-safe)
 * @param pool Pool backing came from (NULL = heap allocated)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void mirror_substrate_backing_destroy(mirror_substrate_backing_t* backing,
                                       mirror_substrate_pool_t* pool);

/**
 * @brief Initialize substrate backing with defaults
 *
 * WHAT: Set default values for substrate backing
 * WHY:  Ensure consistent initialization
 * HOW:  Zero-init then set biological defaults
 *
 * @param backing Backing to initialize
 * @param config Configuration for defaults
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller owns backing)
 */
void mirror_substrate_backing_init(mirror_substrate_backing_t* backing,
                                    const mirror_substrate_config_t* config);

//=============================================================================
// Copy-on-Write API
//=============================================================================

/**
 * @brief Create CoW copy of substrate backing
 *
 * WHAT: Create shallow copy that shares data until modification
 * WHY:  Efficient cloning for branching/analysis
 * HOW:  Increment ref count, defer deep copy until write
 *
 * @param backing Backing to copy
 * @param pool Pool for new backing (NULL = heap)
 * @return CoW copy or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (atomic ref count)
 */
mirror_substrate_backing_t* mirror_substrate_cow_copy(
    mirror_substrate_backing_t* backing,
    mirror_substrate_pool_t* pool);

/**
 * @brief Prepare backing for modification (CoW)
 *
 * WHAT: Ensure backing has own copy before write
 * WHY:  Implement copy-on-write semantics
 * HOW:  Deep copy if ref count > 1
 *
 * @param backing Backing to prepare
 * @return NIMCP_SUCCESS if ready for write
 *
 * COMPLEXITY: O(1) if sole owner, O(n) if copy needed
 * THREAD-SAFE: Yes
 */
nimcp_result_t mirror_substrate_cow_prepare_write(mirror_substrate_backing_t* backing);

/**
 * @brief Release CoW reference
 *
 * WHAT: Decrement ref count and free if last reference
 * WHY:  Proper cleanup of CoW copies
 * HOW:  Atomic decrement, free when count reaches 0
 *
 * @param backing Backing to release
 * @param pool Pool for deallocation
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (atomic operations)
 */
void mirror_substrate_cow_release(mirror_substrate_backing_t* backing,
                                   mirror_substrate_pool_t* pool);

/**
 * @brief Check if backing is a CoW copy
 *
 * @param backing Backing to check
 * @return true if this is a CoW copy
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (atomic read)
 */
bool mirror_substrate_is_cow_copy(const mirror_substrate_backing_t* backing);

//=============================================================================
// Axon Integration API
//=============================================================================

/**
 * @brief Bind observation axon to substrate backing
 *
 * WHAT: Connect axon for observation input pathway
 * WHY:  Enable myelination-dependent recognition speed
 * HOW:  Store axon reference, compute initial delay
 *
 * @param backing Substrate backing
 * @param axon Observation pathway axon
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller owns backing)
 */
nimcp_result_t mirror_substrate_bind_observation_axon(
    mirror_substrate_backing_t* backing,
    void* axon);

/**
 * @brief Bind execution axon to substrate backing
 *
 * WHAT: Connect axon for execution output pathway
 * WHY:  Enable motor output timing
 * HOW:  Store axon reference, compute initial delay
 *
 * @param backing Substrate backing
 * @param axon Execution pathway axon
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller owns backing)
 */
nimcp_result_t mirror_substrate_bind_execution_axon(
    mirror_substrate_backing_t* backing,
    void* axon);

/**
 * @brief Get observation recognition delay
 *
 * WHAT: Calculate delay for observation recognition
 * WHY:  Myelination affects recognition speed
 * HOW:  Combine axon delay + myelin speedup + dendrite integration
 *
 * @param backing Substrate backing
 * @return Recognition delay in milliseconds
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
float mirror_substrate_get_observation_delay(const mirror_substrate_backing_t* backing);

/**
 * @brief Get execution output delay
 *
 * WHAT: Calculate delay for execution signal output
 * WHY:  Timing affects motor coordination
 * HOW:  Combine axon propagation + myelination
 *
 * @param backing Substrate backing
 * @return Execution delay in milliseconds
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
float mirror_substrate_get_execution_delay(const mirror_substrate_backing_t* backing);

//=============================================================================
// Myelin Sheath Integration API
//=============================================================================

/**
 * @brief Bind myelin sheath to substrate backing
 *
 * WHAT: Connect myelin sheath for myelination effects
 * WHY:  Enable detailed myelination modeling
 * HOW:  Store sheath reference, sync myelination level
 *
 * @param backing Substrate backing
 * @param sheath Myelin sheath
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller owns backing)
 */
nimcp_result_t mirror_substrate_bind_myelin_sheath(
    mirror_substrate_backing_t* backing,
    void* sheath);

/**
 * @brief Update myelination level from sheath
 *
 * WHAT: Sync myelination level with myelin sheath state
 * WHY:  Track myelination changes over time
 * HOW:  Query sheath for current level, update velocity
 *
 * @param backing Substrate backing
 * @return New myelination level (0-1)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must synchronize)
 */
float mirror_substrate_update_myelination(mirror_substrate_backing_t* backing);

/**
 * @brief Apply activity to myelin (activity-dependent myelination)
 *
 * WHAT: Signal activity to myelin sheath for adaptive myelination
 * WHY:  High-activity pathways get more myelin
 * HOW:  Forward activity to sheath, trigger remodeling
 *
 * @param backing Substrate backing
 * @param activity_level Activity level (firing rate proxy)
 * @param dt_seconds Time step in seconds
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must synchronize)
 */
void mirror_substrate_apply_activity_to_myelin(
    mirror_substrate_backing_t* backing,
    float activity_level,
    float dt_seconds);

//=============================================================================
// Dendrite Integration API
//=============================================================================

/**
 * @brief Bind dendrite to substrate backing
 *
 * WHAT: Connect dendrite for synaptic integration
 * WHY:  Enable dendritic spine plasticity
 * HOW:  Store dendrite reference
 *
 * @param backing Substrate backing
 * @param dendrite Dendrite structure
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller owns backing)
 */
nimcp_result_t mirror_substrate_bind_dendrite(
    mirror_substrate_backing_t* backing,
    void* dendrite);

/**
 * @brief Add dendritic spine for association
 *
 * WHAT: Add spine encoding observation-execution association
 * WHY:  Spines are substrate for associative learning
 * HOW:  Allocate spine slot, initialize with thin type
 *
 * @param backing Substrate backing
 * @param spine Dendritic spine (from dendrite module)
 * @param initial_weight Initial synaptic weight (0-1)
 * @return Spine index or -1 on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller owns backing)
 */
int32_t mirror_substrate_add_spine(
    mirror_substrate_backing_t* backing,
    void* spine,
    float initial_weight);

/**
 * @brief Update spine plasticity based on activity
 *
 * WHAT: Apply plasticity rules to association spines
 * WHY:  Activity patterns drive structural changes
 * HOW:  Hebbian-like: co-activation strengthens, low activity weakens
 *
 * @param backing Substrate backing
 * @param observation_active True if observation pathway active
 * @param execution_active True if execution pathway active
 * @param dt_seconds Time step in seconds
 *
 * COMPLEXITY: O(num_spines)
 * THREAD-SAFE: No (caller owns backing)
 */
void mirror_substrate_update_spine_plasticity(
    mirror_substrate_backing_t* backing,
    bool observation_active,
    bool execution_active,
    float dt_seconds);

/**
 * @brief Get total spine weight (association strength)
 *
 * WHAT: Sum spine weights for overall association strength
 * WHY:  Measure observation-execution association
 * HOW:  Sum weights of non-pruned spines
 *
 * @param backing Substrate backing
 * @return Total spine weight (0 to num_spines)
 *
 * COMPLEXITY: O(num_spines)
 * THREAD-SAFE: Yes (read-only)
 */
float mirror_substrate_get_total_spine_weight(const mirror_substrate_backing_t* backing);

//=============================================================================
// Glial Cell Integration API
//=============================================================================

/**
 * @brief Bind astrocyte to substrate backing
 *
 * WHAT: Assign astrocyte for plasticity modulation
 * WHY:  Astrocytes modulate synaptic strength via Ca2+
 * HOW:  Store reference, initialize modulation factor
 *
 * @param backing Substrate backing
 * @param astrocyte Astrocyte cell
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller owns backing)
 */
nimcp_result_t mirror_substrate_bind_astrocyte(
    mirror_substrate_backing_t* backing,
    void* astrocyte);

/**
 * @brief Bind oligodendrocyte to substrate backing
 *
 * WHAT: Assign oligodendrocyte for metabolic support
 * WHY:  Oligodendrocytes provide lactate and maintain myelin
 * HOW:  Store reference, track metabolic support
 *
 * @param backing Substrate backing
 * @param oligo Oligodendrocyte cell
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller owns backing)
 */
nimcp_result_t mirror_substrate_bind_oligodendrocyte(
    mirror_substrate_backing_t* backing,
    void* oligo);

/**
 * @brief Bind microglia to substrate backing
 *
 * WHAT: Assign microglia for synaptic surveillance
 * WHY:  Microglia prune weak/unused associations
 * HOW:  Store reference, begin activity monitoring
 *
 * @param backing Substrate backing
 * @param microglia Microglia cell
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller owns backing)
 */
nimcp_result_t mirror_substrate_bind_microglia(
    mirror_substrate_backing_t* backing,
    void* microglia);

/**
 * @brief Get astrocyte modulation factor
 *
 * WHAT: Query current astrocyte modulation of plasticity
 * WHY:  Use for scaling learning rate
 * HOW:  Return cached modulation factor
 *
 * @param backing Substrate backing
 * @return Modulation factor (0.5 to 2.0), 1.0 if no astrocyte
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
float mirror_substrate_get_astrocyte_modulation(const mirror_substrate_backing_t* backing);

/**
 * @brief Update glial cell states
 *
 * WHAT: Step all glial cell integrations forward
 * WHY:  Keep glial state synchronized with activity
 * HOW:  Update astrocyte Ca2+, oligo lactate, microglia surveillance
 *
 * @param backing Substrate backing
 * @param activity_level Current activity level
 * @param dt_seconds Time step in seconds
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must synchronize)
 */
void mirror_substrate_update_glial(
    mirror_substrate_backing_t* backing,
    float activity_level,
    float dt_seconds);

/**
 * @brief Check if backing is marked for pruning
 *
 * WHAT: Query microglia pruning decision
 * WHY:  Low-activity associations should be removed
 * HOW:  Check surveillance score against threshold
 *
 * @param backing Substrate backing
 * @return true if marked for pruning
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
bool mirror_substrate_is_marked_for_pruning(const mirror_substrate_backing_t* backing);

//=============================================================================
// System Integration API
//=============================================================================

/**
 * @brief Integrate substrate with mirror neuron system
 *
 * WHAT: Connect substrate layer to mirror neuron cognitive system
 * WHY:  Enable substrate-backed mirror neuron behavior
 * HOW:  Store references, configure integration mode
 *
 * @param mirror Mirror neuron system
 * @param config Substrate configuration
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(num_neurons) for full integration
 * THREAD-SAFE: No (requires exclusive access to mirror)
 */
nimcp_result_t mirror_substrate_integrate_system(
    mirror_neurons_t mirror,
    const mirror_substrate_config_t* config);

/**
 * @brief Connect to glial integration layer
 *
 * WHAT: Wire substrate to central glial integration
 * WHY:  Coordinate with other glial-neural integrations
 * HOW:  Store glial integration reference, register callbacks
 *
 * @param mirror Mirror neuron system
 * @param glial_integration Glial integration layer
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
nimcp_result_t mirror_substrate_connect_glial_integration(
    mirror_neurons_t mirror,
    void* glial_integration);

/**
 * @brief Connect to myelin sheath network
 *
 * WHAT: Wire substrate to myelin sheath network
 * WHY:  Enable network-level myelination coordination
 * HOW:  Store network reference, create sheaths for active units
 *
 * @param mirror Mirror neuron system
 * @param myelin_network Myelin sheath network
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(num_neurons)
 * THREAD-SAFE: No
 */
nimcp_result_t mirror_substrate_connect_myelin_network(
    mirror_neurons_t mirror,
    void* myelin_network);

/**
 * @brief Connect to axon network
 *
 * WHAT: Wire substrate to axon network
 * WHY:  Enable axon-based propagation delays
 * HOW:  Store network reference, create axons for pathways
 *
 * @param mirror Mirror neuron system
 * @param axon_network Axon network
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(num_neurons)
 * THREAD-SAFE: No
 */
nimcp_result_t mirror_substrate_connect_axon_network(
    mirror_neurons_t mirror,
    void* axon_network);

/**
 * @brief Connect to dendrite network
 *
 * WHAT: Wire substrate to dendrite network
 * WHY:  Enable dendritic spine plasticity
 * HOW:  Store network reference, create dendrites for units
 *
 * @param mirror Mirror neuron system
 * @param dendrite_network Dendrite network
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(num_neurons)
 * THREAD-SAFE: No
 */
nimcp_result_t mirror_substrate_connect_dendrite_network(
    mirror_neurons_t mirror,
    void* dendrite_network);

//=============================================================================
// Simulation Step API
//=============================================================================

/**
 * @brief Step substrate forward in time
 *
 * WHAT: Advance all substrate states by one timestep
 * WHY:  Keep substrate synchronized with simulation
 * HOW:  Update myelination, spine plasticity, glial states
 *
 * @param backing Substrate backing
 * @param current_time Current simulation time (microseconds)
 * @param dt_seconds Time step in seconds
 *
 * COMPLEXITY: O(num_spines)
 * THREAD-SAFE: No (caller must synchronize)
 */
void mirror_substrate_step(
    mirror_substrate_backing_t* backing,
    uint64_t current_time,
    float dt_seconds);

/**
 * @brief Record observation activity
 *
 * WHAT: Update activity tracking for observation event
 * WHY:  Activity drives plasticity and myelination
 * HOW:  Update EMA, timestamp, trigger plasticity
 *
 * @param backing Substrate backing
 * @param strength Activation strength (0-1)
 * @param timestamp Event timestamp (microseconds)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must synchronize)
 */
void mirror_substrate_record_observation(
    mirror_substrate_backing_t* backing,
    float strength,
    uint64_t timestamp);

/**
 * @brief Record execution activity
 *
 * WHAT: Update activity tracking for execution event
 * WHY:  Activity drives plasticity and myelination
 * HOW:  Update EMA, timestamp, check coactivation
 *
 * @param backing Substrate backing
 * @param strength Activation strength (0-1)
 * @param timestamp Event timestamp (microseconds)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must synchronize)
 */
void mirror_substrate_record_execution(
    mirror_substrate_backing_t* backing,
    float strength,
    uint64_t timestamp);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get substrate statistics for mirror system
 *
 * WHAT: Aggregate statistics across all substrate backings
 * WHY:  Monitor substrate health and performance
 * HOW:  Iterate backings, compute aggregates
 *
 * @param mirror Mirror neuron system
 * @param stats Output statistics structure
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(num_neurons)
 * THREAD-SAFE: Yes (read-only aggregation)
 */
nimcp_result_t mirror_substrate_get_stats(
    mirror_neurons_t mirror,
    mirror_substrate_stats_t* stats);

/**
 * @brief Get default substrate configuration
 *
 * WHAT: Return sensible default configuration
 * WHY:  Provide good starting point for substrate integration
 * HOW:  Return pre-configured struct
 *
 * @return Default configuration
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (returns stack value)
 */
mirror_substrate_config_t mirror_substrate_get_default_config(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_SUBSTRATE_H */
