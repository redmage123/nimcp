//=============================================================================
// nimcp_brain_pr_memory.h - Prime Resonant Memory Brain Integration
//=============================================================================
/**
 * @file nimcp_brain_pr_memory.h
 * @brief Brain integration for Prime Resonant memory system
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Integration layer connecting PR memory system to brain lifecycle
 * WHY:  PR memory provides biologically-inspired consolidation and retrieval
 * HOW:  Initializes Z-Ladder, theta-gamma, and entanglement during brain creation
 *
 * ARCHITECTURE:
 *
 *   Brain ←→ PR Memory Integration:
 *   +-----------------------------------------------------------------------+
 *   |  Brain Creation                                                        |
 *   |  ┌─────────────────────┐                                               |
 *   |  │ brain_create_*()    │                                               |
 *   |  │                     │                                               |
 *   |  │  ┌───────────────┐  │    ┌─────────────────────────────────────┐   |
 *   |  │  │ PR Memory Init│──│───→│ Z-Ladder: 4-tier consolidation       │   |
 *   |  │  │               │  │    │ Theta-Gamma: Phase-gated encoding    │   |
 *   |  │  │               │  │    │ Entanglement: Associative graph      │   |
 *   |  │  └───────────────┘  │    └─────────────────────────────────────┘   |
 *   |  │                     │                                               |
 *   |  │  ┌───────────────┐  │    ┌─────────────────────────────────────┐   |
 *   |  │  │ Brain Update  │──│───→│ Consolidation tick (every 100ms)     │   |
 *   |  │  │               │  │    │ Theta-gamma phase advance            │   |
 *   |  │  └───────────────┘  │    └─────────────────────────────────────┘   |
 *   |  │                     │                                               |
 *   |  │  ┌───────────────┐  │    ┌─────────────────────────────────────┐   |
 *   |  │  │ Brain Destroy │──│───→│ Cleanup all PR memory resources      │   |
 *   |  │  └───────────────┘  │    └─────────────────────────────────────┘   |
 *   |  └─────────────────────┘                                               |
 *   +-----------------------------------------------------------------------+
 *
 * INTEGRATION POINTS:
 * - Hippocampus: Z-Ladder maps to hippocampal memory consolidation stages
 * - Working Memory: Z0 tier corresponds to prefrontal working memory buffer
 * - Sleep System: Consolidation accelerated during NREM/REM sleep stages
 * - Emotional System: Salience quaternion component affects promotion rates
 * - Training System: PR bridges connect to plasticity mechanisms
 * - Perception: Visual/audio/speech bridges encode sensory memories
 *
 * THREAD SAFETY:
 * - All public functions are thread-safe
 * - PR memory components have internal mutexes
 * - Brain-level mutex coordinates subsystem access
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAIN_PR_MEMORY_H
#define NIMCP_BRAIN_PR_MEMORY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

struct brain_struct;  // From nimcp_brain_internal.h

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief PR memory brain integration configuration
 */
typedef struct {
    /* Z-Ladder configuration */
    uint32_t z0_capacity;              /**< Working memory capacity (default: 9) */
    uint32_t z1_capacity;              /**< Short-term capacity (default: 100) */
    uint32_t z2_capacity;              /**< Long-term capacity (default: 10000) */
    uint32_t z3_capacity;              /**< Permanent capacity (default: 100000) */

    /* Theta-gamma configuration */
    float theta_freq_hz;               /**< Theta frequency (default: 6.0 Hz) */
    float gamma_freq_hz;               /**< Gamma frequency (default: 40.0 Hz) */
    bool enable_phase_gating;          /**< Enable encoding/retrieval windows (default: true) */

    /* Entanglement configuration */
    uint32_t max_entangle_nodes;       /**< Maximum entangled nodes (default: 50000) */
    uint32_t max_entangle_edges;       /**< Maximum entanglement edges (default: 200000) */
    float auto_link_threshold;         /**< Resonance threshold for auto-linking (default: 0.6) */

    /* Consolidation timing */
    uint64_t consolidation_interval_us; /**< Consolidation tick interval (default: 100000 = 100ms) */
    bool enable_sleep_boost;           /**< Boost consolidation during sleep (default: true) */

} brain_pr_memory_config_t;

/**
 * @brief Get default PR memory brain integration configuration
 * @return Default configuration values
 */
brain_pr_memory_config_t brain_pr_memory_config_default(void);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Initialize PR memory subsystem for a brain
 *
 * WHAT: Creates and configures Z-Ladder, theta-gamma, and entanglement
 * WHY:  Enables biologically-inspired memory consolidation and retrieval
 * HOW:  Allocates PR memory components and links them to brain structure
 *
 * COMPLEXITY: O(1) for initialization (component creation is O(capacity))
 *
 * @param brain Brain structure to initialize PR memory for
 * @param config Configuration (NULL for defaults)
 * @return true on success, false on failure
 *
 * @note Called automatically by brain_create_custom() if PR memory is enabled
 * @note Safe to call multiple times (subsequent calls are no-ops if already init)
 */
bool nimcp_brain_pr_memory_init(struct brain_struct* brain, const brain_pr_memory_config_t* config);

/**
 * @brief Destroy PR memory subsystem for a brain
 *
 * WHAT: Releases all PR memory resources
 * WHY:  Cleanup during brain destruction
 * HOW:  Destroys Z-Ladder, theta-gamma, and entanglement components
 *
 * COMPLEXITY: O(n) where n = total memories stored
 *
 * @param brain Brain structure to cleanup PR memory for
 *
 * @note Called automatically by brain_destroy()
 * @note Safe to call on uninitialized or already-destroyed brain
 */
void nimcp_brain_pr_memory_destroy(struct brain_struct* brain);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Tick PR memory consolidation
 *
 * WHAT: Advances theta-gamma phase, triggers consolidation if interval elapsed
 * WHY:  Memory consolidation should happen continuously in background
 * HOW:  Checks elapsed time, advances oscillations, triggers Z-Ladder consolidation
 *
 * COMPLEXITY: O(n) worst case during consolidation, O(1) between consolidations
 *
 * @param brain Brain structure
 * @param current_time_us Current simulation time in microseconds
 * @return true if consolidation was triggered, false otherwise
 *
 * @note Should be called from brain_step() or inference loop
 */
bool nimcp_brain_pr_memory_tick(struct brain_struct* brain, uint64_t current_time_us);

/**
 * @brief Check if PR memory system is initialized
 *
 * @param brain Brain structure to check
 * @return true if PR memory is initialized and ready
 */
bool nimcp_brain_pr_memory_is_initialized(const struct brain_struct* brain);

//=============================================================================
// Accessor Functions
//=============================================================================

/**
 * @brief Get Z-Ladder handle for direct memory operations
 *
 * @param brain Brain structure
 * @return Z-Ladder handle or NULL if not initialized
 */
struct z_ladder_struct* nimcp_brain_get_z_ladder(struct brain_struct* brain);

/**
 * @brief Get theta-gamma manager for phase queries
 *
 * @param brain Brain structure
 * @return Theta-gamma manager or NULL if not initialized
 */
struct theta_gamma_manager_internal* nimcp_brain_get_theta_gamma(struct brain_struct* brain);

/**
 * @brief Get entanglement graph for association queries
 *
 * @param brain Brain structure
 * @return Entanglement graph or NULL if not initialized
 */
struct entangle_graph_struct* nimcp_brain_get_entanglement(struct brain_struct* brain);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief PR memory statistics
 */
typedef struct {
    /* Z-Ladder stats */
    uint32_t z0_count;                 /**< Current Z0 (working) memory count */
    uint32_t z1_count;                 /**< Current Z1 (short-term) memory count */
    uint32_t z2_count;                 /**< Current Z2 (long-term) memory count */
    uint32_t z3_count;                 /**< Current Z3 (permanent) memory count */
    uint64_t total_promotions;         /**< Total promotion events */
    uint64_t total_demotions;          /**< Total demotion events */
    uint64_t total_evictions;          /**< Total eviction events */

    /* Theta-gamma stats */
    float current_theta_phase;         /**< Current theta phase (0-360 degrees) */
    float current_gamma_amplitude;     /**< Current gamma amplitude envelope */
    bool is_encoding_window;           /**< True if in encoding window (0-90°) */
    bool is_retrieval_window;          /**< True if in retrieval window (180-270°) */

    /* Entanglement stats */
    uint32_t entangle_node_count;      /**< Current entanglement nodes */
    uint32_t entangle_edge_count;      /**< Current entanglement edges */
    float avg_node_degree;             /**< Average connections per node */

    /* Timing stats */
    uint64_t total_consolidation_ticks; /**< Total consolidation ticks processed */
    uint64_t last_consolidation_us;    /**< Timestamp of last consolidation */

} brain_pr_memory_stats_t;

/**
 * @brief Get PR memory statistics
 *
 * @param brain Brain structure
 * @param stats Output statistics structure
 * @return true on success, false if PR memory not initialized
 */
bool nimcp_brain_pr_memory_get_stats(const struct brain_struct* brain, brain_pr_memory_stats_t* stats);

/*============================================================================
 * Phase E3: auto-insertion hook from training tick chain.
 *==========================================================================*/
void nimcp_brain_pr_memory_auto_insert(struct brain_struct* brain,
                                        const float* features,
                                        uint32_t num_features,
                                        float confidence);

/*============================================================================
 * Phase E4 #2: Landmark checkpoint save / load.
 *
 * save: iterates live landmark set, writes a compact binary record per
 *       landmark containing {reason, prime_signature, data_size, data}
 *       to `path`. Atomic (temp + rename).
 * load: re-hydrates landmarks by reconstructing pr_memory_node
 *       instances from saved data+signature, inserting at Z0, then
 *       marking as landmark. Silently skips corrupted records.
 *
 * Returns true on success (including zero-landmarks-to-save), false on
 * I/O failure or pr_memory not initialized.
 *==========================================================================*/
bool nimcp_brain_pr_memory_save_landmarks(struct brain_struct* brain,
                                           const char* path);
bool nimcp_brain_pr_memory_load_landmarks(struct brain_struct* brain,
                                           const char* path);

/*============================================================================
 * Phase E4 #4: Event-triggered landmark promotion.
 *
 * Combines pr_memory_node_create + z_ladder_insert + z_ladder_mark_landmark
 * into one call. For use by reward/amygdala/engram paths to mark salient
 * events as permanent landmarks without going through auto-insert.
 *
 * Returns the assigned node_id (non-zero) on success, 0 on failure.
 *==========================================================================*/
uint64_t nimcp_brain_pr_memory_promote_event(struct brain_struct* brain,
                                              const float* features,
                                              uint32_t num_features,
                                              const char* reason);

/*============================================================================
 * Phase E6: autonomous driver lifecycle.
 *
 * start: prefers cycle-coordinator-driven mode (register_driven); falls
 *        back to a local pthread at 100ms cadence if no coordinator is
 *        available. Idempotent — returns true if already running.
 * stop:  unregisters from the coordinator (which stops + joins the
 *        coordinator-owned thread) OR signals + joins the fallback
 *        thread. Safe to call when not started.
 *
 * Normally invoked from pr_memory_init/destroy; exposed here for tests
 * and future callers that may need to restart the driver.
 *==========================================================================*/
bool nimcp_brain_pr_memory_driver_start(struct brain_struct* brain);
void nimcp_brain_pr_memory_driver_stop(struct brain_struct* brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_PR_MEMORY_H
