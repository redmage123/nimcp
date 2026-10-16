/**
 * @file nimcp_heterosynaptic.h
 * @brief Heterosynaptic Plasticity with Spatial Competition
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Heterosynaptic plasticity where potentiation of one synapse weakens neighbors
 * WHY:  Critical for synaptic competition, input selectivity, and preventing runaway potentiation
 * HOW:  When synapse i is potentiated, neighbors j get: Δw_j = -η × exp(-dist/λ) × Δw_i
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * HETEROSYNAPTIC DEPRESSION:
 * -------------------------
 * 1. Local Competition (Lynch et al., 1977; Royer & Paré, 2003):
 *    - LTP at one synapse induces LTD at neighboring synapses
 *    - Distance-dependent: closer neighbors depressed more
 *    - Time window: ~5-30 minutes after potentiation
 *    - Mechanism: Retrograde signaling (endocannabinoids, NO)
 *    - Reference: Lynch et al. (1977) "Intracellular injections of EGTA block LTP"
 *
 * 2. Winner-Take-All Dynamics (Chistiakova & Volgushev, 2009):
 *    - Strong inputs suppress weaker inputs on same dendrite
 *    - Implements input selectivity and feature detection
 *    - Prevents all synapses from saturating at w_max
 *    - Critical for sparse coding and efficient representations
 *    - Reference: Chistiakova et al. (2014) "Heterosynaptic plasticity"
 *
 * 3. Spatial Organization (Bhatt et al., 2016):
 *    - Synapses organized by dendritic location
 *    - Competition radius: 5-20 μm on dendrite
 *    - Exponential distance decay: exp(-dist/λ), λ = 10 μm
 *    - Preserves synaptic clustering vs. competition balance
 *    - Reference: Bhatt et al. (2016) "Dendritic spine dynamics"
 *
 * 4. Homeostatic Function (Turrigiano & Nelson, 2004):
 *    - Prevents runaway LTP from homosynaptic STDP
 *    - Maintains total synaptic strength homeostasis
 *    - Complements synaptic scaling mechanisms
 *    - Essential for stable learning without saturation
 *
 * MATHEMATICAL FORMULATION:
 * ------------------------
 * For synapse i receiving LTP Δw_i, neighbor synapse j experiences:
 *
 *   Δw_j = -η × exp(-d_ij / λ) × Δw_i
 *
 * Where:
 *   η = depression factor (0-1, typically 0.3-0.5)
 *   d_ij = Euclidean distance between synapses i and j
 *   λ = spatial decay constant (length scale)
 *   Δw_i = weight change at potentiated synapse
 *
 * Winner-Take-All Extension:
 *   Only strongest synapse in competition radius gets full LTP
 *   All others receive heterosynaptic depression
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    HETEROSYNAPTIC PLASTICITY                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SYNAPSE POTENTIATION:                                                    ║
 * ║   ┌─────────────┐                                                          ║
 * ║   │ Synapse i   │  ──────┐                                                 ║
 * ║   │ LTP: +Δw_i  │        │                                                 ║
 * ║   └─────────────┘        ▼                                                 ║
 * ║                    ┌──────────────────────────┐                            ║
 * ║                    │  SPATIAL INDEXING        │                            ║
 * ║                    │  Find neighbors within   │                            ║
 * ║                    │  competition radius      │                            ║
 * ║                    └──────────────────────────┘                            ║
 * ║                              │                                             ║
 * ║   ┌──────────────────────────┴────────────────────────────┐               ║
 * ║   │                                                        │               ║
 * ║   ▼                        ▼                              ▼               ║
 * ║ ┌─────────────┐      ┌─────────────┐              ┌─────────────┐        ║
 * ║ │ Neighbor 1  │      │ Neighbor 2  │              │ Neighbor N  │        ║
 * ║ │ dist = 5μm  │      │ dist = 12μm │              │ dist = 18μm │        ║
 * ║ │ factor=0.61 │      │ factor=0.30 │              │ factor=0.16 │        ║
 * ║ │ LTD: -0.30Δw│      │ LTD: -0.15Δw│              │ LTD: -0.08Δw│        ║
 * ║ └─────────────┘      └─────────────┘              └─────────────┘        ║
 * ║                                                                            ║
 * ║   WINNER-TAKE-ALL (Optional):                                             ║
 * ║   ┌──────────────────────────────────────────────────────┐                ║
 * ║   │ Competition Radius = 15μm                            │                ║
 * ║   │ Synapse A: w=0.8, input=0.9  ← WINNER (full LTP)    │                ║
 * ║   │ Synapse B: w=0.6, input=0.7  ← LTD (suppressed)     │                ║
 * ║   │ Synapse C: w=0.5, input=0.6  ← LTD (suppressed)     │                ║
 * ║   └──────────────────────────────────────────────────────┘                ║
 * ║                                                                            ║
 * ║   INTEGRATION WITH OTHER PLASTICITY:                                      ║
 * ║   - STDP provides homosynaptic LTP/LTD                                    ║
 * ║   - Heterosynaptic adds lateral competition                               ║
 * ║   - BCM provides threshold-based modulation                               ║
 * ║   - Homeostatic scaling maintains global stability                        ║
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

#ifndef NIMCP_HETEROSYNAPTIC_H
#define NIMCP_HETEROSYNAPTIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* NIMCP dependencies */
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/memory/nimcp_memory.h"

/* Bio-async integration */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Heterosynaptic plasticity defaults */
#define HETERO_DEFAULT_NEIGHBOR_RADIUS      15.0f   /**< Competition radius (μm) */
#define HETERO_DEFAULT_DEPRESSION_FACTOR    0.4f    /**< Strength of depression (0-1) */
#define HETERO_DEFAULT_DECAY_LAMBDA         10.0f   /**< Spatial decay constant (μm) */
#define HETERO_DEFAULT_DELAY_MS             500.0f  /**< Delay after LTP (ms) */

/* Winner-take-all parameters */
#define HETERO_WTA_THRESHOLD                0.7f    /**< Minimum strength to compete */
#define HETERO_WTA_SUPPRESSION_FACTOR       0.8f    /**< Suppression for non-winners */

/* Spatial indexing constants */
#define HETERO_SPATIAL_GRID_CELLS           32      /**< Spatial hash grid size */
#define HETERO_MAX_NEIGHBORS_PER_SYNAPSE    50      /**< Max neighbors to check */

/* Callback event types */
#define HETERO_EVENT_COMPETITION            1       /**< Competition event occurred */
#define HETERO_EVENT_WINNER_SELECTED        2       /**< WTA winner selected */
#define HETERO_EVENT_NEIGHBOR_DEPRESSED     3       /**< Neighbor was depressed */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief 3D spatial coordinates for synapse location
 *
 * WHAT: Physical position on dendrite or neuron
 * WHY:  Distance computation for competition
 */
typedef struct {
    float x;                /**< X coordinate (μm) */
    float y;                /**< Y coordinate (μm) */
    float z;                /**< Z coordinate (μm) */
} hetero_spatial_coords_t;

/**
 * @brief Heterosynaptic synapse state
 *
 * WHAT: Per-synapse heterosynaptic plasticity state
 * WHY:  Track position, competition events, and modulation
 *
 * THREAD SAFETY NOTE:
 * The lock field uses pointer indirection (nimcp_platform_mutex_t*) instead
 * of embedding the mutex directly. This is REQUIRED because:
 *   1. The synapse array may be reallocated (via nimcp_realloc)
 *   2. On some platforms, mutexes contain self-referential pointers
 *   3. realloc would invalidate those internal pointers, causing UB
 * By using a pointer to a separately-allocated mutex, the mutex memory
 * stays fixed even when the synapse struct moves during realloc.
 */
typedef struct {
    /* Synaptic state */
    float weight;                       /**< Current weight [0-1] */
    float w_max;                        /**< Maximum weight */
    float w_min;                        /**< Minimum weight */

    /* Spatial organization */
    hetero_spatial_coords_t position;   /**< 3D position on dendrite */
    uint32_t synapse_id;                /**< Unique synapse identifier */
    uint32_t postsynaptic_neuron_id;    /**< Which neuron this synapses onto */

    /* Competition state */
    float last_potentiation;            /**< Last LTP amount received */
    float last_depression;              /**< Last LTD amount received */
    uint64_t last_potentiation_time_ms; /**< When last potentiated */
    bool is_eligible_for_competition;   /**< Can participate in WTA */

    /* Sleep state modulation */
    sleep_state_t current_sleep_state;  /**< Current sleep/wake state */

    /* Statistics */
    uint64_t num_competitions;          /**< Times participated in competition */
    uint64_t num_wins;                  /**< Times won competition */
    uint64_t num_neighbor_depressions;  /**< Times depressed by neighbor */
    float total_hetero_ltd;             /**< Cumulative heterosynaptic LTD */

    /* Thread safety - pointer indirection for realloc safety */
    nimcp_platform_mutex_t* lock;       /**< Mutex for concurrent access (heap-allocated) */
} hetero_synapse_t;

/**
 * @brief Spatial index for efficient neighbor lookup
 *
 * WHAT: Accelerates finding synapses within competition radius
 * WHY:  O(1) average case vs O(n) linear search
 * HOW:  Spatial hashing into 3D grid cells
 */
typedef struct {
    hetero_synapse_t*** grid;           /**< 3D grid of synapse lists */
    uint32_t* cell_counts;              /**< Synapses per cell */
    uint32_t* cell_capacities;          /**< Capacity per cell */
    uint32_t grid_size;                 /**< Cells per dimension */
    float cell_width;                   /**< Width of each cell (μm) */
    float world_min[3];                 /**< Minimum world coordinates */
    float world_max[3];                 /**< Maximum world coordinates */
} hetero_spatial_index_t;

/**
 * @brief Heterosynaptic plasticity configuration
 */
typedef struct {
    /* Core parameters */
    float neighbor_radius;              /**< Competition radius (μm) */
    float depression_factor;            /**< Strength of depression (0-1) */
    float decay_lambda;                 /**< Spatial decay constant (μm) */
    float delay_ms;                     /**< Delay after LTP (ms) */

    /* Winner-take-all */
    bool enable_competition;            /**< Enable WTA dynamics */
    float wta_threshold;                /**< Minimum strength to compete */
    float wta_suppression_factor;       /**< Non-winner suppression */

    /* Integration */
    bool enable_sleep_modulation;       /**< Sleep state modulates competition */
    bool enable_immune_modulation;      /**< Immune system modulates competition */
    bool enable_bio_async;              /**< Enable bio-async messaging */

    /* Spatial indexing */
    bool enable_spatial_index;          /**< Use spatial hashing (recommended) */
    uint32_t spatial_grid_size;         /**< Grid cells per dimension */

    /* Callbacks */
    void (*on_competition_event)(uint32_t winner_id, uint32_t num_competitors, void* user_data);
    void* callback_user_data;           /**< User data for callbacks */
} hetero_config_t;

/**
 * @brief Heterosynaptic plasticity system
 *
 * WHAT: Manages all heterosynaptic synapses and competition
 * WHY:  Centralized coordination of lateral competition
 */
typedef struct {
    /* Synapse management */
    hetero_synapse_t* synapses;         /**< Array of synapses */
    size_t num_synapses;                /**< Current number of synapses */
    size_t synapse_capacity;            /**< Allocated capacity */

    /* Spatial organization */
    hetero_spatial_index_t* spatial_index; /**< Fast neighbor lookup */

    /* Configuration */
    hetero_config_t config;             /**< System configuration */

    /* Global statistics */
    uint64_t total_competitions;        /**< Total competition events */
    uint64_t total_depressions;         /**< Total heterosynaptic LTD events */
    float avg_neighbors_per_competition; /**< Average competitors per event */

    /* Bio-async integration */
    bio_module_context_t bio_ctx;       /**< Bio-async module context */
    bool bio_async_enabled;             /**< Bio-async active */

    /* Thread safety */
    nimcp_mutex_t* mutex;               /**< System-wide mutex */
} hetero_system_t;

/**
 * @brief Competition event result
 *
 * MEMORY OWNERSHIP: The depressed_ids array is heap-allocated and MUST be
 * freed by the caller when no longer needed. Use hetero_competition_result_free()
 * for proper cleanup, or manually call nimcp_free(result.depressed_ids).
 *
 * Example usage:
 *   hetero_competition_result_t result = {0};
 *   hetero_run_competition(system, center, &result);
 *   // ... use result ...
 *   hetero_competition_result_free(&result);  // Required to avoid memory leak
 */
typedef struct {
    uint32_t winner_id;                 /**< ID of winning synapse */
    uint32_t num_competitors;           /**< Number of synapses in competition */
    float winner_strength;              /**< Strength of winner */
    float avg_competitor_strength;      /**< Average strength of losers */
    uint32_t* depressed_ids;            /**< IDs of depressed synapses (caller must free) */
    size_t num_depressed;               /**< Number depressed */
} hetero_competition_result_t;

/**
 * WHAT: Free competition result resources
 * WHY:  depressed_ids is heap-allocated and must be freed to avoid leaks
 * HOW:  Free depressed_ids array and reset to NULL
 *
 * @param result Competition result to clean up (may be NULL)
 */
static inline void hetero_competition_result_free(hetero_competition_result_t* result) {
    if (result && result->depressed_ids) {
        nimcp_free(result->depressed_ids);
        result->depressed_ids = NULL;
        result->num_depressed = 0;
    }
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default heterosynaptic configuration
 * WHY:  Provide biologically plausible starting parameters
 * HOW:  Return struct with Lynch/Royer/Paré defaults
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int hetero_default_config(hetero_config_t* config);

/**
 * WHAT: Create heterosynaptic plasticity system
 * WHY:  Initialize competition and spatial indexing
 * HOW:  Allocate structures, build spatial index
 *
 * @param config Configuration (NULL for defaults)
 * @param initial_capacity Initial synapse capacity
 * @return New system or NULL on failure
 */
hetero_system_t* hetero_create(const hetero_config_t* config, size_t initial_capacity);

/**
 * WHAT: Destroy heterosynaptic system
 * WHY:  Clean up all resources
 * HOW:  Free synapses, spatial index, mutex
 *
 * @param system System to destroy
 */
void hetero_destroy(hetero_system_t* system);

/* ============================================================================
 * Synapse Management API
 * ============================================================================ */

/**
 * WHAT: Add synapse to heterosynaptic system
 * WHY:  Register synapse for competition
 * HOW:  Add to array and spatial index
 *
 * @param system Heterosynaptic system
 * @param position 3D coordinates on dendrite
 * @param initial_weight Starting weight
 * @param synapse_id Unique identifier
 * @param neuron_id Postsynaptic neuron ID
 * @return 0 on success, -1 on error
 */
int hetero_add_synapse(
    hetero_system_t* system,
    const hetero_spatial_coords_t* position,
    float initial_weight,
    uint32_t synapse_id,
    uint32_t neuron_id
);

/**
 * WHAT: Remove synapse from system
 * WHY:  Synapse pruning or cleanup
 * HOW:  Remove from array and spatial index
 *
 * @param system Heterosynaptic system
 * @param synapse_id ID of synapse to remove
 * @return 0 on success, -1 if not found
 */
int hetero_remove_synapse(hetero_system_t* system, uint32_t synapse_id);

/**
 * WHAT: Get synapse by ID (DEPRECATED - UNSAFE - see warning)
 * WHY:  Query or modify synapse state
 * HOW:  Linear search (use sparingly, prefer batch operations)
 *
 * @deprecated This function is DEPRECATED for multi-threaded code.
 *             Use hetero_get_synapse_copy() instead for thread-safe access.
 *
 * CRITICAL WARNING: Returns pointer that may be invalidated by concurrent
 * add/remove operations. The pointer can become dangling if another thread
 * calls hetero_add_synapse() (triggering realloc) or hetero_remove_synapse().
 *
 * For thread-safe access, use hetero_get_synapse_copy() instead.
 *
 * @param system Heterosynaptic system
 * @param synapse_id Synapse identifier
 * @return Synapse pointer or NULL if not found (POINTER MAY BE INVALIDATED)
 */
hetero_synapse_t* hetero_get_synapse(hetero_system_t* system, uint32_t synapse_id);

/**
 * WHAT: Get thread-safe copy of synapse data by ID
 * WHY:  hetero_get_synapse() returns pointer that can be invalidated by realloc
 * HOW:  Copy synapse data while holding mutex
 *
 * This is the RECOMMENDED API for multi-threaded access.
 *
 * @param system Heterosynaptic system
 * @param synapse_id Synapse identifier
 * @param out_synapse Output: copy of synapse data (caller-allocated)
 * @return 0 on success, -1 if not found, -2 if NULL parameters
 */
int hetero_get_synapse_copy(hetero_system_t* system, uint32_t synapse_id,
                            hetero_synapse_t* out_synapse);

/* ============================================================================
 * Heterosynaptic Plasticity API
 * ============================================================================ */

/**
 * WHAT: Apply heterosynaptic depression to synapse
 * WHY:  Core heterosynaptic plasticity mechanism
 * HOW:  Compute distance-weighted depression: Δw = -η × exp(-d/λ) × Δw_i
 *
 * BIOLOGICAL: Lynch et al. (1977), Royer & Paré (2003)
 *
 * @param system Heterosynaptic system
 * @param potentiated_id ID of synapse that was potentiated
 * @param ltp_amount Amount of LTP (positive)
 * @param current_time_ms Current simulation time
 * @return 0 on success, -1 on error
 */
int hetero_apply_depression(
    hetero_system_t* system,
    uint32_t potentiated_id,
    float ltp_amount,
    uint64_t current_time_ms
);

/**
 * WHAT: Run winner-take-all competition
 * WHY:  Implement input selectivity and sparse coding
 * HOW:  Find strongest synapse in radius, depress others
 *
 * BIOLOGICAL: Chistiakova & Volgushev (2009)
 *
 * @param system Heterosynaptic system
 * @param center_position Competition center
 * @param radius Competition radius (overrides config if > 0)
 * @param result Output: competition result
 * @return 0 on success, -1 on error
 */
int hetero_winner_take_all(
    hetero_system_t* system,
    const hetero_spatial_coords_t* center_position,
    float radius,
    hetero_competition_result_t* result
);

/**
 * WHAT: Update synapse weight with heterosynaptic modulation
 * WHY:  Apply heterosynaptic LTD from nearby potentiation
 * HOW:  Check for recent neighbor potentiation, apply depression
 *
 * @param system Heterosynaptic system
 * @param synapse_id Synapse to update
 * @param homosynaptic_dw Weight change from STDP/BCM
 * @param current_time_ms Current simulation time
 * @return Effective weight change (includes heterosynaptic)
 */
float hetero_modulate_weight_change(
    hetero_system_t* system,
    uint32_t synapse_id,
    float homosynaptic_dw,
    uint64_t current_time_ms
);

/* ============================================================================
 * Spatial Query API
 * ============================================================================ */

/**
 * WHAT: Find neighbors within radius (DEPRECATED - UNSAFE)
 * WHY:  Identify synapses for competition
 * HOW:  Query spatial index or linear search
 *
 * @deprecated This function is DEPRECATED for multi-threaded code.
 *             Use hetero_find_neighbors_copy() instead for thread-safe access.
 *
 * CRITICAL WARNING: Returns pointers that may be invalidated by concurrent
 * add/remove operations. The pointers can become dangling if another thread
 * calls hetero_add_synapse() (triggering realloc) or hetero_remove_synapse().
 *
 * For thread-safe access, use hetero_find_neighbors_copy() instead.
 *
 * @param system Heterosynaptic system
 * @param center_position Query center
 * @param radius Search radius (um)
 * @param neighbors Output array of POINTERS (caller allocates) - UNSAFE
 * @param max_neighbors Maximum neighbors to return
 * @param num_found Output: number found
 * @return 0 on success, -1 on error
 */
int hetero_find_neighbors(
    hetero_system_t* system,
    const hetero_spatial_coords_t* center_position,
    float radius,
    hetero_synapse_t** neighbors,
    size_t max_neighbors,
    size_t* num_found
);

/**
 * WHAT: Find neighbors within radius and copy their data (THREAD-SAFE)
 * WHY:  hetero_find_neighbors() returns pointers that can be invalidated by realloc
 * HOW:  Copy synapse data to caller-provided array while holding mutex
 *
 * This is the RECOMMENDED API for multi-threaded access.
 *
 * @param system Heterosynaptic system
 * @param center_position Query center
 * @param radius Search radius (um)
 * @param neighbors_out Output array of COPIES (caller allocates) - SAFE
 * @param max_neighbors Maximum neighbors to return
 * @param num_found Output: number found
 * @return 0 on success, NIMCP_ERROR_NULL_POINTER if NULL params
 */
int hetero_find_neighbors_copy(
    hetero_system_t* system,
    const hetero_spatial_coords_t* center_position,
    float radius,
    hetero_synapse_t* neighbors_out,
    size_t max_neighbors,
    size_t* num_found
);

/**
 * WHAT: Compute Euclidean distance between synapses
 * WHY:  Distance-dependent depression
 * HOW:  sqrt((x1-x2)^2 + (y1-y2)^2 + (z1-z2)^2)
 *
 * @param pos1 First position
 * @param pos2 Second position
 * @return Distance in micrometers
 */
float hetero_compute_distance(
    const hetero_spatial_coords_t* pos1,
    const hetero_spatial_coords_t* pos2
);

/**
 * WHAT: Compute depression factor from distance
 * WHY:  Exponential spatial decay
 * HOW:  factor = η × exp(-distance / λ)
 *
 * @param distance Distance between synapses (μm)
 * @param depression_factor Base depression η
 * @param decay_lambda Spatial decay λ
 * @return Depression factor [0-1]
 */
float hetero_compute_depression_factor(
    float distance,
    float depression_factor,
    float decay_lambda
);

/* ============================================================================
 * Sleep Integration API
 * ============================================================================ */

/**
 * WHAT: Set sleep state for all synapses
 * WHY:  Sleep modulates competition strength
 * HOW:  Update current_sleep_state field
 *
 * During sleep, competition may be reduced to allow consolidation
 *
 * @param system Heterosynaptic system
 * @param state New sleep state
 * @return 0 on success
 */
int hetero_set_sleep_state(hetero_system_t* system, sleep_state_t state);

/**
 * WHAT: Get sleep-modulated depression factor
 * WHY:  Competition varies with sleep/wake
 * HOW:  Scale depression by sleep state multiplier
 *
 * AWAKE: Full competition (1.0x)
 * NREM: Reduced competition (0.5x) for consolidation
 * REM: Moderate competition (0.7x)
 *
 * @param system Heterosynaptic system
 * @param base_factor Base depression factor
 * @return Modulated factor
 */
float hetero_get_sleep_modulated_factor(hetero_system_t* system, float base_factor);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * WHAT: Connect to bio-async router
 * WHY:  Enable inter-module messaging for competition events
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_HETEROSYNAPTIC
 *
 * @param system Heterosynaptic system
 * @return 0 on success, -1 on error
 */
int hetero_connect_bio_async(hetero_system_t* system);

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param system Heterosynaptic system
 * @return 0 on success
 */
int hetero_disconnect_bio_async(hetero_system_t* system);

/**
 * WHAT: Check if bio-async is connected
 *
 * @param system Heterosynaptic system
 * @return true if connected
 */
bool hetero_is_bio_async_connected(const hetero_system_t* system);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * WHAT: Get system statistics
 * WHY:  Monitor competition dynamics
 * HOW:  Return aggregate counts and averages
 *
 * @param system Heterosynaptic system
 * @param total_competitions Output: total competitions
 * @param total_depressions Output: total LTD events
 * @param avg_neighbors Output: average competitors per event
 * @return 0 on success
 */
int hetero_get_statistics(
    const hetero_system_t* system,
    uint64_t* total_competitions,
    uint64_t* total_depressions,
    float* avg_neighbors
);

/**
 * WHAT: Reset system statistics
 * WHY:  Start fresh tracking
 * HOW:  Zero all counters
 *
 * @param system Heterosynaptic system
 */
void hetero_reset_statistics(hetero_system_t* system);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * WHAT: Free competition result
 * WHY:  Cleanup dynamically allocated arrays
 * HOW:  Free depressed_ids array
 *
 * @param result Result to free
 */
void hetero_free_competition_result(hetero_competition_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HETEROSYNAPTIC_H */
