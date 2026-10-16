//=============================================================================
// nimcp_reconsolidation.h - Memory Reconsolidation System for Prime Resonant Memory
//=============================================================================
/**
 * @file nimcp_reconsolidation.h
 * @brief Memory updating during retrieval-triggered labile states
 *
 * WHAT: Implements memory reconsolidation - the process by which retrieved
 *       memories become temporarily labile and can be modified or strengthened
 * WHY:  Memories are not static; retrieval makes them modifiable, enabling
 *       learning from new experiences to update existing memories
 * HOW:  Tracks retrieval events, manages lability windows, and coordinates
 *       memory updates through the Prime Resonant memory infrastructure
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Memory Reconsolidation Biology:
 *   +-----------------------------------------------------------------------+
 *   |  When a memory is retrieved, it enters a "labile" state where:       |
 *   |                                                                       |
 *   |  1. REACTIVATION: Retrieval triggers memory trace destabilization    |
 *   |     - Neural: NMDA receptor activation in hippocampus/amygdala       |
 *   |     - Molecular: Protein degradation of synaptic scaffolds           |
 *   |                                                                       |
 *   |  2. LABILITY WINDOW: Memory is modifiable for ~6 hours               |
 *   |     - Blocking protein synthesis during this window erases memory    |
 *   |     - New information presented during window can modify memory      |
 *   |                                                                       |
 *   |  3. RESTABILIZATION: Memory re-consolidated with potential changes   |
 *   |     - Neural: New protein synthesis required                         |
 *   |     - Result: Updated memory trace with modified content/strength    |
 *   +-----------------------------------------------------------------------+
 *
 *   Reconsolidation Outcomes:
 *   +-----------------------------------------------------------------------+
 *   |  STRENGTHENING: Retrieval without new info reinforces memory         |
 *   |     - Increases consolidation (quaternion.w)                         |
 *   |     - Increases accessibility (quaternion.z)                         |
 *   |     - May promote to higher Z-ladder tier                           |
 *   |                                                                       |
 *   |  UPDATE: Compatible new information modifies memory                  |
 *   |     - Prime signature updated to incorporate new content             |
 *   |     - Quaternion state adjusted for new context                      |
 *   |     - Entanglement graph edges may be added/modified                |
 *   |                                                                       |
 *   |  INTERFERENCE: Incompatible new learning disrupts reconsolidation   |
 *   |     - Memory may become weaker or corrupted                          |
 *   |     - Can lead to "forgetting" of original memory                   |
 *   |                                                                       |
 *   |  BLOCKED: Protein synthesis inhibition prevents restabilization      |
 *   |     - Simulates PSI (protein synthesis inhibitor) effects            |
 *   |     - Memory strength reduced, may be lost                          |
 *   +-----------------------------------------------------------------------+
 *
 *   State Machine:
 *   +-----------------------------------------------------------------------+
 *   |                                                                       |
 *   |   STABLE -----(retrieval)-----> LABILE                               |
 *   |      ^                           |                                   |
 *   |      |                           v                                   |
 *   |      |                     +------------+                            |
 *   |      |                     | no change  |---(timeout)---> RESTABILIZING
 *   |      |                     | new info   |---+                |       |
 *   |      |                     +------------+   |                v       |
 *   |      |                           |          |          UPDATING      |
 *   |      |                           |          |             |          |
 *   |      +---(restabilized)----------+----------+-------------+          |
 *   |                                                                       |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - on_retrieval: O(1) for window creation
 * - update cycle: O(W) where W = active windows
 * - propose_update: O(1) signature/quaternion copy
 * - commit_update: O(1) with optional O(N) entanglement update
 * - interference check: O(W * R) where R = recent encodings
 *
 * MEMORY:
 * - reconsolidation_window_t: ~256 bytes per active window
 * - reconsolidation_system_t: ~1KB base + windows
 * - Maximum windows configurable (default 1024)
 *
 * THREAD SAFETY:
 * - System-level operations protected by mutex
 * - Individual window operations require system lock
 * - Statistics use atomic operations where possible
 *
 * INTEGRATION:
 * - PR Memory Node: Triggers on retrieval, updates signature/quaternion
 * - Entanglement Graph: May modify edges during update
 * - Resonance Engine: Used for interference detection
 * - Z-Ladder: Strengthening may promote tier
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_RECONSOLIDATION_H
#define NIMCP_RECONSOLIDATION_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Dependencies
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_prime_signature.h"
#include "cognitive/memory/core/nimcp_quaternion.h"
#include "cognitive/memory/core/nimcp_entanglement.h"
#include "cognitive/memory/core/nimcp_resonance.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default lability window duration in seconds (~6 hours biological) */
#define RECON_DEFAULT_LABILITY_DURATION     21600.0f

/** Simulated lability duration for testing (5 seconds) */
#define RECON_TEST_LABILITY_DURATION        5.0f

/** Default exponential decay rate for lability strength */
#define RECON_DEFAULT_DECAY_RATE            0.0002f

/** Default threshold for update vs strengthen decision */
#define RECON_DEFAULT_UPDATE_THRESHOLD      0.3f

/** Default interference threshold */
#define RECON_DEFAULT_INTERFERENCE_THRESHOLD 0.7f

/** Default strengthening boost per retrieval */
#define RECON_DEFAULT_STRENGTHEN_BOOST      0.05f

/** Maximum labile memories per interference check */
#define RECON_MAX_INTERFERENCE_CHECK        32

/** Maximum simultaneous reconsolidation windows */
#define RECON_DEFAULT_MAX_WINDOWS           1024

/** Minimum lability strength to maintain window */
#define RECON_LABILITY_EPSILON              0.01f

/** Numerical epsilon for floating point comparisons */
#define RECON_EPSILON                       1e-6f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Reconsolidation state enumeration
 *
 * WHAT: Current state of a memory in the reconsolidation process
 * WHY:  Different states require different handling
 */
typedef enum {
    RECON_STABLE = 0,      /**< Memory is stable, not recently retrieved */
    RECON_LABILE,          /**< Memory is labile, can be modified */
    RECON_UPDATING,        /**< Currently being updated with new info */
    RECON_RESTABILIZING    /**< Restabilizing after update/timeout */
} reconsolidation_state_t;

/**
 * @brief Reconsolidation outcome enumeration
 *
 * WHAT: Result of a reconsolidation cycle
 * WHY:  Tracking outcomes for analysis and debugging
 */
typedef enum {
    RECON_OUTCOME_NONE = 0,        /**< No outcome yet (still in progress) */
    RECON_OUTCOME_STRENGTHENED,    /**< Memory was strengthened */
    RECON_OUTCOME_UPDATED,         /**< Memory was updated with new info */
    RECON_OUTCOME_INTERFERENCE,    /**< Reconsolidation blocked by interference */
    RECON_OUTCOME_BLOCKED,         /**< Blocked by protein synthesis inhibition */
    RECON_OUTCOME_EXPIRED,         /**< Window expired without restabilization */
    RECON_OUTCOME_ROLLBACK         /**< Update was rolled back */
} reconsolidation_outcome_t;

/**
 * @brief Error codes for reconsolidation operations
 */
typedef enum {
    RECON_SUCCESS = 0,                     /**< Operation succeeded */
    RECON_ERROR_NULL_POINTER = -1,         /**< NULL pointer argument */
    RECON_ERROR_NOT_FOUND = -2,            /**< Memory/window not found */
    RECON_ERROR_NOT_LABILE = -3,           /**< Memory is not in labile state */
    RECON_ERROR_ALREADY_UPDATING = -4,     /**< Memory is already being updated */
    RECON_ERROR_NO_PROPOSED_UPDATE = -5,   /**< No update has been proposed */
    RECON_ERROR_INTERFERENCE = -6,         /**< Interference blocked operation */
    RECON_ERROR_BLOCKED = -7,              /**< Protein synthesis blocked */
    RECON_ERROR_CAPACITY = -8,             /**< Maximum windows reached */
    RECON_ERROR_NO_MEMORY = -9,            /**< Memory allocation failed */
    RECON_ERROR_INVALID_STATE = -10,       /**< Invalid state for operation */
    RECON_ERROR_INVALID_CONFIG = -11       /**< Invalid configuration */
} reconsolidation_error_t;

/**
 * @brief Reconsolidation window for tracking labile memory
 *
 * WHAT: Tracks a single memory's reconsolidation state and potential updates
 * WHY:  Need to maintain state for each actively reconsolidating memory
 * HOW:  Stores original state for rollback, proposed updates, and timing info
 *
 * Memory layout: ~256 bytes per window
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Memory Reference
    //-------------------------------------------------------------------------
    pr_memory_node_t* memory;              /**< Reference to the labile memory */
    uint64_t memory_id;                    /**< Memory node ID (for validation) */

    //-------------------------------------------------------------------------
    // State Information
    //-------------------------------------------------------------------------
    reconsolidation_state_t state;         /**< Current reconsolidation state */
    reconsolidation_outcome_t outcome;     /**< Outcome (when completed) */

    //-------------------------------------------------------------------------
    // Timing Information
    //-------------------------------------------------------------------------
    float retrieval_time;                  /**< When was it retrieved (seconds) */
    float lability_remaining;              /**< Time remaining in labile window */
    float lability_strength;               /**< How labile (0-1), decays over time */
    float state_enter_time;                /**< When current state was entered */

    //-------------------------------------------------------------------------
    // Original State (for rollback if needed)
    //-------------------------------------------------------------------------
    prime_signature_t original_signature;  /**< Original content signature */
    nimcp_quaternion_t original_quaternion; /**< Original semantic state */
    float original_strength;               /**< Original memory strength */

    //-------------------------------------------------------------------------
    // Proposed Updates
    //-------------------------------------------------------------------------
    prime_signature_t proposed_signature;  /**< Proposed new signature */
    nimcp_quaternion_t proposed_quaternion; /**< Proposed new state */
    bool has_proposed_update;              /**< Whether an update has been proposed */
    float update_magnitude;                /**< How much the update would change */

    //-------------------------------------------------------------------------
    // Interference Tracking
    //-------------------------------------------------------------------------
    pr_memory_node_t** interfering_memories; /**< Memories causing interference */
    size_t num_interfering;                /**< Number of interfering memories */
    size_t max_interfering;                /**< Capacity of interference array */
    float interference_strength;           /**< Total interference level (0-1) */

    //-------------------------------------------------------------------------
    // Retrieval Context
    //-------------------------------------------------------------------------
    uint32_t retrieval_count;              /**< Times retrieved during window */
    float cumulative_activation;           /**< Sum of activations during window */

} reconsolidation_window_t;

/**
 * @brief Configuration for reconsolidation system
 *
 * WHAT: Parameters controlling reconsolidation behavior
 * WHY:  Different applications may need different timing and thresholds
 */
typedef struct {
    float lability_duration;               /**< How long memories stay labile (seconds) */
    float lability_decay_rate;             /**< Exponential decay rate for lability */
    float update_threshold;                /**< Change magnitude triggering update vs strengthen */
    float interference_threshold;          /**< Resonance level that causes interference */
    float strengthen_boost;                /**< Consolidation boost per retrieval */
    size_t max_windows;                    /**< Maximum simultaneous windows */
    size_t max_interfering_per_window;     /**< Max interference memories to track per window */
    bool enable_interference_detection;    /**< Whether to detect interference */
    bool enable_auto_strengthen;           /**< Auto-strengthen on retrieval */
    bool enable_auto_commit;               /**< Auto-commit updates on timeout */
    float restabilization_time;            /**< Time to spend in restabilizing state */
} reconsolidation_config_t;

/**
 * @brief Statistics for reconsolidation system
 *
 * WHAT: Operational metrics for monitoring and debugging
 * WHY:  Track system health and behavior patterns
 */
typedef struct {
    uint64_t total_retrievals;             /**< Total memory retrievals tracked */
    uint64_t total_windows_created;        /**< Windows created over lifetime */
    uint64_t total_updates;                /**< Successful memory updates */
    uint64_t total_strengthenings;         /**< Memories strengthened */
    uint64_t total_interference_blocks;    /**< Updates blocked by interference */
    uint64_t total_synthesis_blocks;       /**< Updates blocked by PSI */
    uint64_t total_rollbacks;              /**< Updates rolled back */
    uint64_t total_expired;                /**< Windows that expired */
    uint64_t current_active_windows;       /**< Currently active windows */
    uint64_t peak_active_windows;          /**< Peak concurrent windows */
    float mean_lability_duration;          /**< Average actual lability duration */
    float mean_update_magnitude;           /**< Average update magnitude */
    float mean_interference_strength;      /**< Average interference when blocked */
} reconsolidation_stats_t;

/**
 * @brief Labile memory query result
 *
 * WHAT: Information about a labile memory for queries
 * WHY:  External code may need to know what memories are currently labile
 */
typedef struct {
    uint64_t memory_id;                    /**< Memory node ID */
    reconsolidation_state_t state;         /**< Current state */
    float lability_remaining;              /**< Seconds remaining */
    float lability_strength;               /**< Current lability (0-1) */
    bool has_proposed_update;              /**< Whether update is proposed */
    float interference_strength;           /**< Current interference level */
} labile_memory_info_t;

/**
 * @brief Resonance configuration for interference detection (forward declare)
 *
 * When resonance engine is available, this is used to configure
 * how interference is computed between memories.
 */
typedef resonance_config_t reconsolidation_resonance_config_t;

/**
 * @brief Opaque reconsolidation system handle
 *
 * Internal structure manages:
 * - Active reconsolidation windows
 * - Connection to PR memory infrastructure
 * - Statistics and configuration
 * - Thread synchronization
 */
typedef struct reconsolidation_system_struct reconsolidation_system_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default reconsolidation configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for most use cases
 *
 * @return Default configuration with:
 *         - lability_duration: 21600.0f (6 hours)
 *         - lability_decay_rate: 0.0002f
 *         - update_threshold: 0.3f
 *         - interference_threshold: 0.7f
 *         - strengthen_boost: 0.05f
 *         - max_windows: 1024
 *         - enable_interference_detection: true
 *         - enable_auto_strengthen: true
 *         - enable_auto_commit: false
 *
 * Performance: ~5ns
 *
 * Example:
 *   reconsolidation_config_t config = reconsolidation_config_default();
 *   config.lability_duration = RECON_TEST_LABILITY_DURATION;  // For testing
 */
NIMCP_EXPORT reconsolidation_config_t reconsolidation_config_default(void);

/**
 * @brief Get fast/testing configuration with short timings
 *
 * WHAT: Configuration with accelerated timings for testing
 * WHY:  Testing can't wait 6 hours for reconsolidation
 *
 * @return Configuration with 5-second lability window
 */
NIMCP_EXPORT reconsolidation_config_t reconsolidation_config_test(void);

/**
 * @brief Validate reconsolidation configuration
 *
 * WHAT: Checks configuration values are valid
 * WHY:  Prevent invalid configs causing runtime errors
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Validation rules:
 * - lability_duration > 0
 * - lability_decay_rate >= 0
 * - update_threshold in [0, 1]
 * - interference_threshold in [0, 1]
 * - max_windows > 0
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT bool reconsolidation_config_validate(const reconsolidation_config_t* config);

//=============================================================================
// System Lifecycle Functions
//=============================================================================

/**
 * @brief Create reconsolidation system
 *
 * WHAT: Creates and initializes the reconsolidation system
 * WHY:  Entry point for memory reconsolidation functionality
 * HOW:  Allocates system, initializes data structures, connects to PR infra
 *
 * @param entanglement Entanglement graph for association management
 * @param node_manager PR memory node manager
 * @param resonance_config Resonance configuration for interference detection (can be NULL)
 * @param config Configuration (NULL for defaults)
 * @return Opaque system handle, or NULL on failure
 *
 * Performance: O(max_windows) for initialization
 * Memory: ~1KB base + ~256 bytes per max_window capacity
 *
 * Thread safety: The returned system is thread-safe
 *
 * Example:
 *   reconsolidation_system_t* recon = reconsolidation_create(
 *       entangle_graph, node_mgr, res_engine, NULL);
 *   if (!recon) {
 *       fprintf(stderr, "Failed to create reconsolidation system\n");
 *   }
 */
NIMCP_EXPORT reconsolidation_system_t* reconsolidation_create(
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    const reconsolidation_resonance_config_t* resonance_config,
    const reconsolidation_config_t* config
);

/**
 * @brief Destroy reconsolidation system
 *
 * WHAT: Frees all resources associated with the system
 * WHY:  Resource cleanup
 * HOW:  Closes all windows, frees memory, releases locks
 *
 * @param system System to destroy (NULL safe)
 *
 * Performance: O(active_windows)
 *
 * Warning: Any active reconsolidation processes are aborted
 *
 * Example:
 *   reconsolidation_destroy(recon);
 *   recon = NULL;  // Good practice
 */
NIMCP_EXPORT void reconsolidation_destroy(reconsolidation_system_t* system);

//=============================================================================
// Core Reconsolidation Functions
//=============================================================================

/**
 * @brief Notify system that a memory was retrieved
 *
 * WHAT: Called when a memory is accessed, may trigger lability
 * WHY:  Retrieval is what initiates the reconsolidation process
 * HOW:  Creates or updates reconsolidation window for the memory
 *
 * @param system Reconsolidation system
 * @param memory Retrieved memory node
 * @param activation_strength How strongly the memory was activated (0-1)
 * @param current_time Current simulation time in seconds
 * @return RECON_SUCCESS or error code
 *
 * Performance: O(1) average
 *
 * Behavior:
 * - If memory is stable: Creates new reconsolidation window, enters LABILE
 * - If memory is already labile: Resets lability timer, accumulates activation
 * - If memory is updating/restabilizing: Accumulates activation only
 *
 * Example:
 *   // When memory is retrieved during query
 *   reconsolidation_error_t err = reconsolidation_on_retrieval(
 *       recon, retrieved_node, 0.8f, current_sim_time);
 */
NIMCP_EXPORT reconsolidation_error_t reconsolidation_on_retrieval(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory,
    float activation_strength,
    float current_time
);

/**
 * @brief Update reconsolidation system (call periodically)
 *
 * WHAT: Main update loop for reconsolidation processing
 * WHY:  Advances state machines, applies decay, handles timeouts
 * HOW:  Iterates through active windows and updates each
 *
 * @param system Reconsolidation system
 * @param current_time Current simulation time in seconds
 * @param delta_time Time elapsed since last update in seconds
 * @return Number of windows processed
 *
 * Performance: O(active_windows)
 *
 * Behavior per window:
 * - LABILE: Decay lability, check for timeout
 * - UPDATING: Monitor update progress
 * - RESTABILIZING: Check if restabilization complete
 *
 * Example:
 *   // In main simulation loop
 *   float dt = 1.0f / 60.0f;  // 60 fps
 *   reconsolidation_update(recon, sim_time, dt);
 */
NIMCP_EXPORT size_t reconsolidation_update(
    reconsolidation_system_t* system,
    float current_time,
    float delta_time
);

/**
 * @brief Propose an update to a labile memory
 *
 * WHAT: Propose new signature and/or quaternion for a labile memory
 * WHY:  New information should be able to modify retrieved memories
 * HOW:  Stores proposed changes in window, computes update magnitude
 *
 * @param system Reconsolidation system
 * @param memory Memory to update (must be labile)
 * @param new_signature New prime signature (NULL to keep current)
 * @param new_quaternion New quaternion state (NULL to keep current)
 * @return RECON_SUCCESS or error code
 *
 * Performance: O(1)
 *
 * Notes:
 * - Memory must be in LABILE state
 * - Overwrites any previous proposed update
 * - Does not apply the update until commit
 * - Computes update magnitude for threshold comparison
 *
 * Example:
 *   prime_signature_t* new_sig = prime_sig_from_content(new_data, size);
 *   nimcp_quaternion_t new_q = quat_create(0.9f, 0.2f, 0.8f, 0.7f);
 *   reconsolidation_propose_update(recon, memory, new_sig, &new_q);
 */
NIMCP_EXPORT reconsolidation_error_t reconsolidation_propose_update(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory,
    const prime_signature_t* new_signature,
    const nimcp_quaternion_t* new_quaternion
);

/**
 * @brief Commit proposed update to labile memory
 *
 * WHAT: Apply the proposed update to the memory
 * WHY:  Finalize the reconsolidation with new information
 * HOW:  Transitions to UPDATING, applies changes, then RESTABILIZING
 *
 * @param system Reconsolidation system
 * @param memory Memory to commit update for
 * @param current_time Current simulation time
 * @return RECON_SUCCESS or error code
 *
 * Performance: O(1) memory update + O(degree) for entanglement updates
 *
 * Notes:
 * - Memory must have a proposed update
 * - Checks for interference before committing
 * - Checks for protein synthesis blockade
 * - If update magnitude < threshold, strengthens instead
 *
 * Example:
 *   reconsolidation_error_t err = reconsolidation_commit_update(
 *       recon, memory, current_time);
 *   if (err == RECON_ERROR_INTERFERENCE) {
 *       printf("Update blocked by interference\n");
 *   }
 */
NIMCP_EXPORT reconsolidation_error_t reconsolidation_commit_update(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory,
    float current_time
);

/**
 * @brief Rollback to original state (undo update)
 *
 * WHAT: Restore memory to state before reconsolidation
 * WHY:  Allow undoing updates if they prove incorrect
 * HOW:  Restores original signature and quaternion from window
 *
 * @param system Reconsolidation system
 * @param memory Memory to rollback
 * @return RECON_SUCCESS or error code
 *
 * Performance: O(1)
 *
 * Notes:
 * - Can be called in UPDATING or RESTABILIZING states
 * - Marks outcome as ROLLBACK
 * - Transitions to STABLE
 *
 * Example:
 *   if (update_was_wrong) {
 *       reconsolidation_rollback(recon, memory);
 *   }
 */
NIMCP_EXPORT reconsolidation_error_t reconsolidation_rollback(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory
);

/**
 * @brief Strengthen memory without modification
 *
 * WHAT: Increase memory consolidation through retrieval
 * WHY:  Retrieval without new info should strengthen, not change
 * HOW:  Boosts quaternion.w (consolidation) and optionally promotes tier
 *
 * @param system Reconsolidation system
 * @param memory Memory to strengthen
 * @param boost_amount Additional boost (added to config strengthen_boost)
 * @return RECON_SUCCESS or error code
 *
 * Performance: O(1)
 *
 * Behavior:
 * - Increases quaternion.w by (config.strengthen_boost + boost_amount)
 * - May increase accessibility (quaternion.z)
 * - Updates promotion eligibility
 * - Marks outcome as STRENGTHENED
 * - Transitions to RESTABILIZING
 *
 * Example:
 *   // Memory was retrieved but no new info presented
 *   reconsolidation_strengthen(recon, memory, 0.0f);
 */
NIMCP_EXPORT reconsolidation_error_t reconsolidation_strengthen(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory,
    float boost_amount
);

//=============================================================================
// Interference Functions
//=============================================================================

/**
 * @brief Check for interference with labile memory
 *
 * WHAT: Detect if new learning interferes with reconsolidation
 * WHY:  Incompatible new memories can disrupt reconsolidation
 * HOW:  Compute resonance between labile memory and recent encodings
 *
 * @param system Reconsolidation system
 * @param memory Labile memory to check
 * @param new_memory Recently encoded memory that might interfere
 * @param interference_out Output: interference strength (0-1)
 * @return true if interference detected (above threshold), false otherwise
 *
 * Performance: O(1) resonance computation
 *
 * Interference criteria:
 * - High resonance (similar content) but different signature
 * - Computed as: resonance * (1 - signature_similarity)
 *
 * Example:
 *   float interference;
 *   if (reconsolidation_check_interference(recon, labile, new_mem, &interference)) {
 *       printf("Interference detected: %.2f\n", interference);
 *   }
 */
NIMCP_EXPORT bool reconsolidation_check_interference(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory,
    pr_memory_node_t* new_memory,
    float* interference_out
);

/**
 * @brief Register new memory encoding for interference tracking
 *
 * WHAT: Notify system of new memory encoding
 * WHY:  New encodings during labile windows can cause interference
 * HOW:  Checks interference against all currently labile memories
 *
 * @param system Reconsolidation system
 * @param new_memory Newly encoded memory
 * @param current_time Current simulation time
 * @return Number of labile memories affected by interference
 *
 * Performance: O(active_windows)
 *
 * Example:
 *   // When a new memory is created
 *   size_t affected = reconsolidation_register_encoding(recon, new_mem, time);
 */
NIMCP_EXPORT size_t reconsolidation_register_encoding(
    reconsolidation_system_t* system,
    pr_memory_node_t* new_memory,
    float current_time
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Check if a specific memory is currently labile
 *
 * WHAT: Query whether memory is in reconsolidation process
 * WHY:  External code may need to know lability status
 *
 * @param system Reconsolidation system
 * @param memory Memory to check
 * @return true if memory is labile (any non-STABLE state)
 *
 * Performance: O(1) hash lookup
 *
 * Example:
 *   if (reconsolidation_is_labile(recon, memory)) {
 *       // Handle specially
 *   }
 */
NIMCP_EXPORT bool reconsolidation_is_labile(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory
);

/**
 * @brief Get current lability strength of a memory
 *
 * WHAT: Query how labile a memory currently is
 * WHY:  Lability decays over time, may affect update decisions
 *
 * @param system Reconsolidation system
 * @param memory Memory to check
 * @return Lability strength [0, 1], or 0 if not labile
 *
 * Performance: O(1)
 *
 * Example:
 *   float lability = reconsolidation_get_lability_strength(recon, memory);
 *   printf("Memory is %.1f%% labile\n", lability * 100);
 */
NIMCP_EXPORT float reconsolidation_get_lability_strength(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory
);

/**
 * @brief Get current state of a reconsolidating memory
 *
 * WHAT: Query the reconsolidation state machine state
 * WHY:  Different states may require different handling
 *
 * @param system Reconsolidation system
 * @param memory Memory to check
 * @return Current state, or RECON_STABLE if not in system
 *
 * Performance: O(1)
 *
 * Example:
 *   reconsolidation_state_t state = reconsolidation_get_state(recon, memory);
 *   if (state == RECON_UPDATING) {
 *       // Wait for update to complete
 *   }
 */
NIMCP_EXPORT reconsolidation_state_t reconsolidation_get_state(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory
);

/**
 * @brief Get all currently labile memories
 *
 * WHAT: Query for all memories in reconsolidation
 * WHY:  May need to process all labile memories together
 *
 * @param system Reconsolidation system
 * @param out_info Output array for labile memory info
 * @param max_results Maximum results to return
 * @param count_out Output: actual number of results
 * @return RECON_SUCCESS or error code
 *
 * Performance: O(active_windows)
 *
 * Example:
 *   labile_memory_info_t infos[100];
 *   size_t count;
 *   reconsolidation_get_labile_memories(recon, infos, 100, &count);
 *   for (size_t i = 0; i < count; i++) {
 *       printf("Memory %lu: %.2f lability\n",
 *              infos[i].memory_id, infos[i].lability_strength);
 *   }
 */
NIMCP_EXPORT reconsolidation_error_t reconsolidation_get_labile_memories(
    reconsolidation_system_t* system,
    labile_memory_info_t* out_info,
    size_t max_results,
    size_t* count_out
);

/**
 * @brief Compute how much a proposed update would change a memory
 *
 * WHAT: Calculate update magnitude before proposing
 * WHY:  Preview update impact for decision making
 *
 * @param system Reconsolidation system
 * @param memory Memory to potentially update
 * @param new_signature Proposed new signature (NULL to keep)
 * @param new_quaternion Proposed new quaternion (NULL to keep)
 * @return Update magnitude [0, 1], or -1 on error
 *
 * Performance: O(1)
 *
 * Calculation:
 * - Signature component: 1 - Jaccard(old, new)
 * - Quaternion component: geodesic_distance(old, new) / pi
 * - Combined with configurable weights
 *
 * Example:
 *   float magnitude = reconsolidation_compute_update_magnitude(
 *       recon, memory, new_sig, &new_quat);
 *   if (magnitude > 0.5f) {
 *       printf("Warning: large update\n");
 *   }
 */
NIMCP_EXPORT float reconsolidation_compute_update_magnitude(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory,
    const prime_signature_t* new_signature,
    const nimcp_quaternion_t* new_quaternion
);

//=============================================================================
// Protein Synthesis Simulation Functions
//=============================================================================

/**
 * @brief Block protein synthesis (simulates PSI)
 *
 * WHAT: Simulate protein synthesis inhibitor for research
 * WHY:  PSI during reconsolidation prevents restabilization
 * HOW:  Sets flag that blocks all restabilization
 *
 * @param system Reconsolidation system
 * @param blocked Whether synthesis should be blocked
 *
 * Performance: O(1)
 *
 * Notes:
 * - Blocking during lability prevents memory restabilization
 * - Can lead to memory loss (strength reduction)
 * - Used for simulating pharmacological interventions
 *
 * Example:
 *   // Research simulation: what happens with PSI?
 *   reconsolidation_block_synthesis(recon, true);
 *   reconsolidation_on_retrieval(recon, memory, 0.8f, time);
 *   // Memory will not restabilize properly
 */
NIMCP_EXPORT void reconsolidation_block_synthesis(
    reconsolidation_system_t* system,
    bool blocked
);

/**
 * @brief Check if protein synthesis is blocked
 *
 * @param system Reconsolidation system
 * @return true if synthesis is blocked
 *
 * Performance: O(1)
 */
NIMCP_EXPORT bool reconsolidation_is_synthesis_blocked(
    reconsolidation_system_t* system
);

//=============================================================================
// Window Management Functions
//=============================================================================

/**
 * @brief Force close a reconsolidation window
 *
 * WHAT: Immediately close a window without completing reconsolidation
 * WHY:  Emergency cleanup or testing
 * HOW:  Marks window as expired, frees resources
 *
 * @param system Reconsolidation system
 * @param memory Memory whose window to close
 * @param restore_original Whether to restore original state
 * @return RECON_SUCCESS or error code
 *
 * Performance: O(1)
 *
 * Example:
 *   reconsolidation_force_close(recon, memory, true);
 */
NIMCP_EXPORT reconsolidation_error_t reconsolidation_force_close(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory,
    bool restore_original
);

/**
 * @brief Force close all reconsolidation windows
 *
 * WHAT: Close all active windows
 * WHY:  System reset or shutdown
 *
 * @param system Reconsolidation system
 * @param restore_original Whether to restore original states
 * @return Number of windows closed
 *
 * Performance: O(active_windows)
 *
 * Example:
 *   size_t closed = reconsolidation_force_close_all(recon, true);
 */
NIMCP_EXPORT size_t reconsolidation_force_close_all(
    reconsolidation_system_t* system,
    bool restore_original
);

/**
 * @brief Get number of currently active windows
 *
 * @param system Reconsolidation system
 * @return Number of active reconsolidation windows
 *
 * Performance: O(1)
 */
NIMCP_EXPORT size_t reconsolidation_get_active_window_count(
    reconsolidation_system_t* system
);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get reconsolidation system statistics
 *
 * WHAT: Returns operational metrics
 * WHY:  Monitoring, debugging, analysis
 *
 * @param system Reconsolidation system
 * @param stats Output statistics structure
 * @return RECON_SUCCESS or error code
 *
 * Performance: O(1)
 *
 * Example:
 *   reconsolidation_stats_t stats;
 *   reconsolidation_get_stats(recon, &stats);
 *   printf("Updates: %lu, Strengthenings: %lu\n",
 *          stats.total_updates, stats.total_strengthenings);
 */
NIMCP_EXPORT reconsolidation_error_t reconsolidation_get_stats(
    reconsolidation_system_t* system,
    reconsolidation_stats_t* stats
);

/**
 * @brief Reset reconsolidation statistics
 *
 * WHAT: Clear all statistical counters
 * WHY:  Start fresh measurement period
 *
 * @param system Reconsolidation system
 *
 * Performance: O(1)
 */
NIMCP_EXPORT void reconsolidation_reset_stats(reconsolidation_system_t* system);

//=============================================================================
// Configuration Update Functions
//=============================================================================

/**
 * @brief Update system configuration
 *
 * WHAT: Change configuration parameters at runtime
 * WHY:  Adjust behavior without recreating system
 *
 * @param system Reconsolidation system
 * @param config New configuration
 * @return RECON_SUCCESS or error code
 *
 * Performance: O(1)
 *
 * Notes:
 * - max_windows cannot be changed (would require reallocation)
 * - Changes apply to new windows, not existing ones
 *
 * Example:
 *   reconsolidation_config_t config = reconsolidation_config_default();
 *   config.update_threshold = 0.5f;  // More conservative updates
 *   reconsolidation_set_config(recon, &config);
 */
NIMCP_EXPORT reconsolidation_error_t reconsolidation_set_config(
    reconsolidation_system_t* system,
    const reconsolidation_config_t* config
);

/**
 * @brief Get current system configuration
 *
 * @param system Reconsolidation system
 * @param config Output configuration structure
 * @return RECON_SUCCESS or error code
 *
 * Performance: O(1)
 */
NIMCP_EXPORT reconsolidation_error_t reconsolidation_get_config(
    reconsolidation_system_t* system,
    reconsolidation_config_t* config
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error string for reconsolidation error code
 *
 * @param error Error code
 * @return Human-readable error string
 *
 * Performance: O(1)
 */
NIMCP_EXPORT const char* reconsolidation_error_string(reconsolidation_error_t error);

/**
 * @brief Get state name as string
 *
 * @param state Reconsolidation state
 * @return Human-readable state name
 *
 * Performance: O(1)
 */
NIMCP_EXPORT const char* reconsolidation_state_name(reconsolidation_state_t state);

/**
 * @brief Get outcome name as string
 *
 * @param outcome Reconsolidation outcome
 * @return Human-readable outcome name
 *
 * Performance: O(1)
 */
NIMCP_EXPORT const char* reconsolidation_outcome_name(reconsolidation_outcome_t outcome);

/**
 * @brief Print reconsolidation window status for debugging
 *
 * @param system Reconsolidation system
 * @param memory Memory to print status for
 */
NIMCP_EXPORT void reconsolidation_print_window_status(
    reconsolidation_system_t* system,
    pr_memory_node_t* memory
);

/**
 * @brief Print system summary for debugging
 *
 * @param system Reconsolidation system
 */
NIMCP_EXPORT void reconsolidation_print_summary(reconsolidation_system_t* system);

/**
 * @brief Validate system internal consistency
 *
 * WHAT: Debug tool for checking system integrity
 * WHY:  Detect corruption or bugs
 *
 * @param system Reconsolidation system
 * @return true if consistent, false if issues detected
 *
 * Performance: O(active_windows)
 */
NIMCP_EXPORT bool reconsolidation_validate(reconsolidation_system_t* system);

//=============================================================================
// Inline Helper Functions
//=============================================================================

/**
 * @brief Check if state indicates memory is being modified
 *
 * @param state Reconsolidation state
 * @return true if LABILE, UPDATING, or RESTABILIZING
 */
static inline bool reconsolidation_state_is_active(reconsolidation_state_t state) {
    return state != RECON_STABLE;
}

/**
 * @brief Check if state allows proposing updates
 *
 * @param state Reconsolidation state
 * @return true if LABILE
 */
static inline bool reconsolidation_state_allows_update(reconsolidation_state_t state) {
    return state == RECON_LABILE;
}

/**
 * @brief Check if outcome indicates successful reconsolidation
 *
 * @param outcome Reconsolidation outcome
 * @return true if STRENGTHENED or UPDATED
 */
static inline bool reconsolidation_outcome_is_success(reconsolidation_outcome_t outcome) {
    return outcome == RECON_OUTCOME_STRENGTHENED || outcome == RECON_OUTCOME_UPDATED;
}

#ifdef __cplusplus
}
#endif

#endif // NIMCP_RECONSOLIDATION_H
