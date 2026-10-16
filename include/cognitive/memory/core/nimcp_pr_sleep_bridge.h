//=============================================================================
// nimcp_pr_sleep_bridge.h - Prime Resonant Sleep-Wake Bridge
//=============================================================================
/**
 * @file nimcp_pr_sleep_bridge.h
 * @brief Sleep stage-based memory consolidation for Prime Resonant memory
 *
 * WHAT: Sleep-wake cycle integration with PR memory for biologically-inspired
 *       memory consolidation, including NREM slow-wave and REM processing
 * WHY:  Sleep is critical for memory consolidation in biological systems;
 *       different sleep stages serve distinct memory processing functions
 * HOW:  Models sleep stages (Wake, N1, N2, N3/SWS, REM), implements memory
 *       replay mechanisms, Z-ladder promotions, and emotional processing
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Sleep Stages and Memory:
 *   +-----------------------------------------------------------------------+
 *   |  Sleep architecture follows a predictable cycle:                      |
 *   |                                                                       |
 *   |  WAKE -----> N1 -----> N2 -----> N3/SWS -----> REM -----> (repeat)   |
 *   |   ^                                            |                      |
 *   |   +--------------------------------------------+                      |
 *   |                                                                       |
 *   |  Memory Consolidation by Stage:                                       |
 *   |  +----------------------------------------------------------------+   |
 *   |  | Stage | Dominant Waves | Memory Function                       |   |
 *   |  |-------|----------------|---------------------------------------|   |
 *   |  | N1    | Theta (4-8Hz)  | Transition, light processing          |   |
 *   |  | N2    | Sleep spindles | Memory stabilization, gating          |   |
 *   |  |       | (12-14Hz)      | hippocampal-neocortical transfer      |   |
 *   |  | N3/SWS| Delta (<4Hz)   | Declarative memory consolidation      |   |
 *   |  |       | Slow waves     | Hippocampal replay, Z-ladder promotion|   |
 *   |  | REM   | Theta + PGO    | Procedural, emotional processing      |   |
 *   |  |       | waves          | Memory integration, pruning           |   |
 *   |  +----------------------------------------------------------------+   |
 *   +-----------------------------------------------------------------------+
 *
 *   Hippocampal-Neocortical Dialogue:
 *   +-----------------------------------------------------------------------+
 *   |  During NREM slow-wave sleep:                                         |
 *   |                                                                       |
 *   |  1. Slow oscillations (0.5-1Hz) modulate neocortical excitability    |
 *   |  2. During UP states, hippocampal sharp-wave ripples (SWR) occur     |
 *   |  3. SWRs contain compressed replay of recent experiences             |
 *   |  4. Sleep spindles (thalamic) gate information transfer              |
 *   |  5. Replayed sequences strengthen neocortical representations        |
 *   |                                                                       |
 *   |  +--------------------+      SWR replay      +---------------------+  |
 *   |  |   Hippocampus      | ------------------> |    Neocortex        |  |
 *   |  |   (Z0-Z1 memories) |    during spindles  |    (Z2-Z3 storage)  |  |
 *   |  +--------------------+                     +---------------------+  |
 *   +-----------------------------------------------------------------------+
 *
 *   Memory Replay Mechanism:
 *   +-----------------------------------------------------------------------+
 *   |  1. Select replay candidates based on:                                |
 *   |     - Recent encoding (Z0, Z1 tiers)                                  |
 *   |     - Emotional salience (quaternion x-component)                     |
 *   |     - High entanglement count (well-connected memories)               |
 *   |     - Partially consolidated (strength > threshold)                   |
 *   |                                                                       |
 *   |  2. Replay sequence generation:                                       |
 *   |     - Forward replay: Temporal sequence preservation                  |
 *   |     - Reverse replay: Prediction error minimization                   |
 *   |     - Compressed: 5-20x faster than real-time                        |
 *   |                                                                       |
 *   |  3. Consolidation effects:                                            |
 *   |     - Strengthen replayed memory (reinforce)                          |
 *   |     - Update entanglement weights (associative strengthening)         |
 *   |     - Promote eligible memories up Z-ladder                           |
 *   +-----------------------------------------------------------------------+
 *
 *   REM Sleep Functions:
 *   +-----------------------------------------------------------------------+
 *   |  - Procedural memory consolidation (motor sequences, skills)          |
 *   |  - Emotional memory processing (amygdala-hippocampal interaction)     |
 *   |  - Memory integration (schema integration, insight)                   |
 *   |  - Weak association pruning (forgetting less relevant connections)    |
 *   |  - Dreams as memory "defragmentation" and creative recombination      |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Stage transition: O(1)
 * - Memory replay cycle: O(K log K) where K = replay candidates
 * - Full consolidation pass: O(N) where N = total memories
 * - Emotional processing: O(E) where E = emotional memories
 *
 * MEMORY:
 * - pr_sleep_bridge_t internal: ~2KB
 * - Replay buffer: configurable (default 1024 entries)
 * - Per-stage state: ~256 bytes
 *
 * THREAD SAFETY:
 * - All public functions are thread-safe via internal mutex
 * - Callbacks invoked without lock held
 *
 * INTEGRATION:
 * - Core: Z-Ladder for tier management, PR memory nodes
 * - Middleware: Resonance for replay selection, Entanglement for association
 * - Optional: Theta-gamma for phase synchronization
 *
 * REFERENCES:
 * - Diekelmann & Born (2010): Memory function of sleep
 * - Walker & Stickgold (2006): Sleep-dependent memory consolidation
 * - Rasch & Born (2013): About sleep's role in memory
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_PR_SLEEP_BRIDGE_H
#define NIMCP_PR_SLEEP_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Dependencies
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_z_ladder.h"
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

/** Number of sleep stages */
#define PR_SLEEP_STAGE_COUNT            5

/** Default replay buffer capacity */
#define PR_SLEEP_DEFAULT_REPLAY_BUFFER  1024

/** Default consolidation cycles per sleep stage */
#define PR_SLEEP_DEFAULT_CYCLES         10

/** Default promotion boost during SWS */
#define PR_SLEEP_SWS_PROMOTION_BOOST    0.3f

/** Default emotional processing intensity in REM */
#define PR_SLEEP_REM_EMOTIONAL_FACTOR   0.5f

/** Default replay compression factor (real-time multiplier) */
#define PR_SLEEP_REPLAY_COMPRESSION     10.0f

/** Minimum memory strength for replay candidacy */
#define PR_SLEEP_MIN_REPLAY_STRENGTH    0.2f

/** Maximum replay candidates per cycle */
#define PR_SLEEP_MAX_REPLAY_PER_CYCLE   100

/** Minimum emotional magnitude for emotional processing */
#define PR_SLEEP_MIN_EMOTIONAL_MAG      0.3f

/** Sleep spindle frequency (Hz) for N2 */
#define PR_SLEEP_SPINDLE_FREQ           12.0f

/** Slow wave frequency (Hz) for SWS */
#define PR_SLEEP_SLOW_WAVE_FREQ         0.75f

/** Default wake maintenance decay rate */
#define PR_SLEEP_WAKE_DECAY_RATE        0.001f

/** Default consolidation strength per replay */
#define PR_SLEEP_CONSOLIDATION_DELTA    0.05f

/** Default association strengthening per co-replay */
#define PR_SLEEP_ASSOC_STRENGTHEN_DELTA 0.1f

/** Default weak association prune threshold in REM */
#define PR_SLEEP_REM_PRUNE_THRESHOLD    0.2f

//=============================================================================
// Type Definitions - Enumerations
//=============================================================================

/**
 * @brief Sleep-wake stage enumeration
 *
 * WHAT: Models the five primary states of sleep-wake cycle
 * WHY:  Each stage has distinct neural activity and memory processing functions
 * HOW:  Enum values map to stage-specific consolidation behaviors
 */
typedef enum {
    PR_SLEEP_STAGE_WAKE = 0,    /**< Waking state - encoding and retrieval active */
    PR_SLEEP_STAGE_N1,          /**< NREM Stage 1 - light sleep, theta waves */
    PR_SLEEP_STAGE_N2,          /**< NREM Stage 2 - sleep spindles, K-complexes */
    PR_SLEEP_STAGE_N3,          /**< NREM Stage 3/SWS - slow-wave sleep, delta */
    PR_SLEEP_STAGE_REM          /**< REM sleep - rapid eye movement, dreaming */
} pr_sleep_stage_t;

/**
 * @brief Memory replay direction
 *
 * WHAT: Direction of memory sequence replay during sleep
 * WHY:  Forward and reverse replay serve different functions
 * HOW:  Determines replay order in replay_sequence()
 */
typedef enum {
    PR_REPLAY_FORWARD = 0,      /**< Forward temporal replay (encoding order) */
    PR_REPLAY_REVERSE,          /**< Reverse replay (prediction learning) */
    PR_REPLAY_RANDOM            /**< Random order (creative recombination) */
} pr_replay_direction_t;

/**
 * @brief Memory type for consolidation targeting
 *
 * WHAT: Category of memory for stage-appropriate processing
 * WHY:  Different memory types consolidate in different stages
 * HOW:  Affects which memories are selected for each stage
 */
typedef enum {
    PR_MEMORY_TYPE_DECLARATIVE = 0, /**< Facts, events (hippocampal) - SWS */
    PR_MEMORY_TYPE_PROCEDURAL,      /**< Skills, motor sequences - REM */
    PR_MEMORY_TYPE_EMOTIONAL,       /**< Emotionally charged memories - REM */
    PR_MEMORY_TYPE_SEMANTIC,        /**< General knowledge - SWS/REM */
    PR_MEMORY_TYPE_EPISODIC         /**< Specific experiences - SWS */
} pr_memory_type_t;

/**
 * @brief Sleep bridge error codes
 */
typedef enum {
    PR_SLEEP_SUCCESS = 0,               /**< Operation succeeded */
    PR_SLEEP_ERROR_NULL_POINTER = -1,   /**< NULL pointer argument */
    PR_SLEEP_ERROR_INVALID_STAGE = -2,  /**< Invalid sleep stage */
    PR_SLEEP_ERROR_NO_MEMORY = -3,      /**< Memory allocation failed */
    PR_SLEEP_ERROR_NOT_INITIALIZED = -4,/**< Bridge not initialized */
    PR_SLEEP_ERROR_INVALID_CONFIG = -5, /**< Invalid configuration */
    PR_SLEEP_ERROR_NO_LADDER = -6,      /**< Z-ladder not set */
    PR_SLEEP_ERROR_REPLAY_FAILED = -7,  /**< Replay operation failed */
    PR_SLEEP_ERROR_STAGE_LOCKED = -8    /**< Stage transition blocked */
} pr_sleep_error_t;

//=============================================================================
// Type Definitions - Structures
//=============================================================================

/**
 * @brief Replay candidate with selection criteria
 *
 * WHAT: Memory selected for replay during sleep
 * WHY:  Track why memory was selected and replay parameters
 * HOW:  Populated by get_replay_candidates()
 */
typedef struct {
    uint64_t node_id;               /**< Memory node identifier */
    pr_memory_tier_t tier;          /**< Current Z-ladder tier */
    float strength;                 /**< Current memory strength */
    float emotional_magnitude;      /**< |emotion| from quaternion.x */
    float salience;                 /**< Salience from quaternion.y */
    uint32_t entanglement_count;    /**< Number of associations */
    uint64_t age_ms;                /**< Time since encoding */
    float replay_priority;          /**< Computed replay priority score */
    pr_memory_type_t type;          /**< Inferred memory type */
    bool is_novel;                  /**< Encoded since last sleep */
} pr_replay_candidate_t;

/**
 * @brief Replay event record
 *
 * WHAT: Record of a completed replay event
 * WHY:  Track replay history for analysis and debugging
 * HOW:  Created after each replay during sleep consolidation
 */
typedef struct {
    uint64_t node_id;               /**< Replayed memory ID */
    pr_sleep_stage_t stage;         /**< Stage during replay */
    pr_replay_direction_t direction;/**< Replay direction */
    float strength_before;          /**< Strength before replay */
    float strength_after;           /**< Strength after replay */
    float consolidation_gained;     /**< Consolidation increase */
    bool was_promoted;              /**< True if tier promoted */
    pr_memory_tier_t tier_before;   /**< Tier before replay */
    pr_memory_tier_t tier_after;    /**< Tier after replay */
    uint64_t timestamp_ms;          /**< Replay timestamp */
    uint32_t sequence_position;     /**< Position in replay sequence */
} pr_replay_event_t;

/**
 * @brief Sleep stage state information
 *
 * WHAT: Current state of a sleep stage
 * WHY:  Track stage-specific metrics and progress
 * HOW:  Updated during consolidation, queried for monitoring
 */
typedef struct {
    pr_sleep_stage_t stage;         /**< Which stage this represents */
    uint64_t entry_time_ms;         /**< When entered this stage */
    uint64_t total_time_ms;         /**< Cumulative time in stage */
    uint32_t consolidation_cycles;  /**< Cycles completed this entry */
    uint32_t memories_replayed;     /**< Memories replayed this entry */
    uint32_t promotions;            /**< Z-ladder promotions this entry */
    float dominant_frequency;       /**< Current dominant EEG frequency */
    float consolidation_efficiency; /**< Ratio of successful consolidations */
} pr_sleep_stage_state_t;

/**
 * @brief Sleep bridge configuration
 *
 * WHAT: Parameters controlling sleep-wake bridge behavior
 * WHY:  Allow tuning for different applications and experiments
 * HOW:  Passed to pr_sleep_bridge_create()
 */
typedef struct {
    /** Replay configuration */
    size_t replay_buffer_capacity;      /**< Max replay candidates to track */
    size_t max_replay_per_cycle;        /**< Max replays per consolidation cycle */
    float replay_compression;           /**< Compression factor vs real-time */
    float min_replay_strength;          /**< Min strength for replay eligibility */

    /** Stage-specific parameters */
    float sws_promotion_boost;          /**< Extra promotion eligibility in SWS */
    float sws_consolidation_factor;     /**< Consolidation multiplier in SWS */
    float rem_emotional_factor;         /**< Emotional processing intensity in REM */
    float rem_prune_threshold;          /**< Weak association prune threshold */
    uint32_t cycles_per_stage[PR_SLEEP_STAGE_COUNT]; /**< Cycles per stage */

    /** Consolidation parameters */
    float consolidation_delta;          /**< Strength gain per replay */
    float association_strengthen_delta; /**< Entanglement weight gain per co-replay */
    float wake_decay_rate;              /**< Decay rate during wake (per second) */
    bool enable_reverse_replay;         /**< Allow reverse replay */
    bool enable_random_replay;          /**< Allow random-order replay in REM */

    /** Integration parameters */
    bool enable_theta_gamma_sync;       /**< Sync with theta-gamma manager */
    bool enable_entanglement_update;    /**< Update entanglement graph */
    bool track_replay_history;          /**< Record replay events */
    size_t max_replay_history;          /**< Max replay events to track */
} pr_sleep_config_t;

/**
 * @brief Sleep bridge statistics
 *
 * WHAT: Operational metrics for sleep-wake cycle
 * WHY:  Monitor consolidation effectiveness, debug issues
 * HOW:  Updated during operations, queried via get_stats()
 */
typedef struct {
    /** Stage statistics */
    uint64_t time_per_stage_ms[PR_SLEEP_STAGE_COUNT]; /**< Time in each stage */
    uint32_t entries_per_stage[PR_SLEEP_STAGE_COUNT]; /**< Entry count per stage */
    uint32_t cycles_per_stage[PR_SLEEP_STAGE_COUNT];  /**< Total cycles per stage */

    /** Consolidation statistics */
    uint64_t total_replays;             /**< Total memories replayed */
    uint64_t total_promotions;          /**< Total Z-ladder promotions */
    uint64_t total_consolidation_time_ms; /**< Total consolidation time */
    float avg_strength_gain;            /**< Average strength gain per replay */
    float avg_consolidation_efficiency; /**< Consolidation success rate */

    /** Replay statistics */
    uint64_t forward_replays;           /**< Forward direction replays */
    uint64_t reverse_replays;           /**< Reverse direction replays */
    uint64_t random_replays;            /**< Random order replays */

    /** Emotional processing (REM) */
    uint64_t emotional_memories_processed; /**< Emotional memories in REM */
    float avg_emotional_reduction;      /**< Mean emotional magnitude reduction */

    /** Association statistics */
    uint64_t associations_strengthened; /**< Entanglement edges strengthened */
    uint64_t associations_pruned;       /**< Weak associations removed */

    /** Overall */
    uint64_t total_sleep_cycles;        /**< Complete sleep cycles */
    uint64_t first_sleep_time_ms;       /**< Timestamp of first sleep */
    uint64_t last_wake_time_ms;         /**< Timestamp of last wake */
} pr_sleep_stats_t;

/**
 * @brief Callback for replay events
 *
 * @param event The replay event that occurred
 * @param user_data User-provided context
 */
typedef void (*pr_replay_callback_t)(
    const pr_replay_event_t* event,
    void* user_data
);

/**
 * @brief Callback for stage transitions
 *
 * @param from_stage Previous stage
 * @param to_stage New stage
 * @param user_data User-provided context
 */
typedef void (*pr_stage_callback_t)(
    pr_sleep_stage_t from_stage,
    pr_sleep_stage_t to_stage,
    void* user_data
);

/**
 * @brief Callback for memory promotion during sleep
 *
 * @param node_id Promoted memory ID
 * @param from_tier Source tier
 * @param to_tier Destination tier
 * @param stage Sleep stage during promotion
 * @param user_data User-provided context
 */
typedef void (*pr_sleep_promotion_callback_t)(
    uint64_t node_id,
    pr_memory_tier_t from_tier,
    pr_memory_tier_t to_tier,
    pr_sleep_stage_t stage,
    void* user_data
);

/**
 * @brief Opaque sleep bridge handle
 */
typedef struct pr_sleep_bridge_struct* pr_sleep_bridge_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default sleep bridge configuration
 *
 * WHAT: Returns sensible default configuration based on sleep research
 * WHY:  Provides starting point for typical memory consolidation scenarios
 * HOW:  Sets replay, consolidation, and stage parameters to research-based values
 *
 * @return Default configuration structure
 *
 * Default values:
 * - replay_buffer_capacity: 1024
 * - max_replay_per_cycle: 100
 * - replay_compression: 10x
 * - sws_promotion_boost: 0.3
 * - rem_emotional_factor: 0.5
 *
 * EXAMPLE:
 * ```c
 * pr_sleep_config_t config = pr_sleep_config_default();
 * config.max_replay_per_cycle = 50;  // More selective replay
 * pr_sleep_bridge_t bridge = pr_sleep_bridge_create(&config);
 * ```
 */
NIMCP_EXPORT pr_sleep_config_t pr_sleep_config_default(void);

/**
 * @brief Validate sleep bridge configuration
 *
 * WHAT: Checks configuration values are valid
 * WHY:  Prevent invalid configs causing runtime errors
 * HOW:  Range checks, constraint enforcement
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Validation rules:
 * - replay_buffer_capacity > 0
 * - max_replay_per_cycle > 0
 * - Factors in range [0, 1]
 * - Delta values > 0
 */
NIMCP_EXPORT bool pr_sleep_config_validate(const pr_sleep_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create sleep bridge
 *
 * WHAT: Allocates and initializes sleep-wake bridge
 * WHY:  Entry point for sleep-based memory consolidation
 * HOW:  Creates internal structures, initializes to WAKE state
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 *
 * COMPLEXITY: O(replay_buffer_capacity) for allocation
 * MEMORY: ~2KB + replay buffer + history buffer
 *
 * EXAMPLE:
 * ```c
 * pr_sleep_config_t config = pr_sleep_config_default();
 * pr_sleep_bridge_t bridge = pr_sleep_bridge_create(&config);
 * if (!bridge) {
 *     fprintf(stderr, "Failed to create bridge: %s\n",
 *             pr_sleep_error_string(PR_SLEEP_ERROR_NO_MEMORY));
 * }
 * ```
 */
NIMCP_EXPORT pr_sleep_bridge_t pr_sleep_bridge_create(const pr_sleep_config_t* config);

/**
 * @brief Destroy sleep bridge and free resources
 *
 * WHAT: Deallocates bridge and all internal buffers
 * WHY:  Resource cleanup
 * HOW:  Frees replay buffer, history, state structures
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
NIMCP_EXPORT void pr_sleep_bridge_destroy(pr_sleep_bridge_t bridge);

/**
 * @brief Set the Z-ladder for consolidation operations
 *
 * WHAT: Associates Z-ladder with sleep bridge
 * WHY:  Z-ladder required for tier promotions and memory access
 * HOW:  Stores reference to Z-ladder for consolidation operations
 *
 * @param bridge Sleep bridge
 * @param ladder Z-ladder to use
 * @return PR_SLEEP_SUCCESS or error code
 *
 * NOTE: Z-ladder must remain valid for lifetime of bridge
 */
NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_set_ladder(
    pr_sleep_bridge_t bridge,
    z_ladder_t ladder
);

/**
 * @brief Set the entanglement graph for association updates
 *
 * WHAT: Associates entanglement graph with sleep bridge
 * WHY:  Graph needed for association strengthening/pruning
 * HOW:  Stores reference to graph for replay operations
 *
 * @param bridge Sleep bridge
 * @param graph Entanglement graph to use (can be NULL to disable)
 * @return PR_SLEEP_SUCCESS or error code
 */
NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_set_entanglement(
    pr_sleep_bridge_t bridge,
    entangle_graph_t graph
);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Clears all state, returns to WAKE
 * WHY:  Start fresh for new simulation
 * HOW:  Resets stage, clears buffers and stats
 *
 * @param bridge Sleep bridge
 * @return PR_SLEEP_SUCCESS or error code
 */
NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_reset(pr_sleep_bridge_t bridge);

//=============================================================================
// Stage Management Functions
//=============================================================================

/**
 * @brief Set current sleep stage
 *
 * WHAT: Transitions to specified sleep stage
 * WHY:  Model sleep-wake cycle progression
 * HOW:  Updates internal state, triggers stage-specific initialization
 *
 * @param bridge Sleep bridge
 * @param stage New sleep stage
 * @return PR_SLEEP_SUCCESS or error code
 *
 * Side effects:
 * - Invokes stage transition callback if set
 * - Resets stage-specific cycle counters
 * - Updates stage entry timestamp
 *
 * EXAMPLE:
 * ```c
 * // Progress through sleep cycle
 * pr_sleep_bridge_set_stage(bridge, PR_SLEEP_STAGE_N1);
 * pr_sleep_bridge_consolidate(bridge);
 * pr_sleep_bridge_set_stage(bridge, PR_SLEEP_STAGE_N2);
 * pr_sleep_bridge_consolidate(bridge);
 * // ... continue through stages
 * ```
 */
NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_set_stage(
    pr_sleep_bridge_t bridge,
    pr_sleep_stage_t stage
);

/**
 * @brief Get current sleep stage
 *
 * @param bridge Sleep bridge
 * @return Current stage or PR_SLEEP_STAGE_WAKE on error
 */
NIMCP_EXPORT pr_sleep_stage_t pr_sleep_bridge_get_stage(
    const pr_sleep_bridge_t bridge
);

/**
 * @brief Get state information for current stage
 *
 * @param bridge Sleep bridge
 * @param state Output state structure
 * @return PR_SLEEP_SUCCESS or error code
 */
NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_get_stage_state(
    const pr_sleep_bridge_t bridge,
    pr_sleep_stage_state_t* state
);

/**
 * @brief Advance to next natural sleep stage
 *
 * WHAT: Automatically transitions to next stage in cycle
 * WHY:  Convenience for simulating natural sleep progression
 * HOW:  WAKE->N1->N2->N3->REM->N2->N3->REM... (typical cycle)
 *
 * @param bridge Sleep bridge
 * @return New stage, or current stage on error
 *
 * Stage progression:
 * - From WAKE: -> N1
 * - From N1: -> N2
 * - From N2: -> N3
 * - From N3: -> REM (first time) or N2 (subsequent)
 * - From REM: -> N2 (cycles) or WAKE (after ~90 min equivalent)
 */
NIMCP_EXPORT pr_sleep_stage_t pr_sleep_bridge_advance_stage(
    pr_sleep_bridge_t bridge
);

/**
 * @brief Check if currently in sleep (any non-WAKE stage)
 *
 * @param bridge Sleep bridge
 * @return true if in any sleep stage, false if WAKE or error
 */
NIMCP_EXPORT bool pr_sleep_bridge_is_sleeping(const pr_sleep_bridge_t bridge);

/**
 * @brief Check if currently in deep sleep (N3/SWS)
 *
 * @param bridge Sleep bridge
 * @return true if in N3/SWS, false otherwise
 */
NIMCP_EXPORT bool pr_sleep_bridge_is_deep_sleep(const pr_sleep_bridge_t bridge);

//=============================================================================
// Consolidation Functions
//=============================================================================

/**
 * @brief Run consolidation for current sleep stage
 *
 * WHAT: Executes stage-appropriate memory consolidation
 * WHY:  Main entry point for sleep-based memory processing
 * HOW:  Selects candidates, replays, promotes based on stage
 *
 * @param bridge Sleep bridge
 * @return Number of memories processed, or -1 on error
 *
 * Stage-specific behavior:
 * - WAKE: Light decay, no consolidation
 * - N1: Mild stabilization, few replays
 * - N2: Spindle-gated transfers, moderate replay
 * - N3/SWS: Intensive declarative consolidation, max replay
 * - REM: Procedural/emotional processing, creative replay
 *
 * COMPLEXITY: O(N log K) where N=total memories, K=replay count
 *
 * EXAMPLE:
 * ```c
 * pr_sleep_bridge_set_stage(bridge, PR_SLEEP_STAGE_N3);
 * int processed = pr_sleep_bridge_consolidate(bridge);
 * printf("Consolidated %d memories in SWS\n", processed);
 * ```
 */
NIMCP_EXPORT int pr_sleep_bridge_consolidate(pr_sleep_bridge_t bridge);

/**
 * @brief Run multiple consolidation cycles for current stage
 *
 * WHAT: Executes multiple consolidation passes
 * WHY:  Full stage-worth of consolidation in one call
 * HOW:  Calls consolidate() num_cycles times
 *
 * @param bridge Sleep bridge
 * @param num_cycles Number of cycles (0 = use config default for stage)
 * @return Total memories processed across all cycles
 */
NIMCP_EXPORT int pr_sleep_bridge_consolidate_cycles(
    pr_sleep_bridge_t bridge,
    uint32_t num_cycles
);

/**
 * @brief Run full sleep cycle (N1->N2->N3->REM->...)
 *
 * WHAT: Simulates complete ~90 minute sleep cycle
 * WHY:  Convenience for full sleep simulation
 * HOW:  Progresses through stages with appropriate timing
 *
 * @param bridge Sleep bridge
 * @param num_cycles Number of NREM-REM cycles to run
 * @return Total memories processed
 *
 * EXAMPLE:
 * ```c
 * // Run 4 sleep cycles (~6 hours equivalent)
 * int total = pr_sleep_bridge_run_sleep_cycles(bridge, 4);
 * printf("Processed %d memories during sleep\n", total);
 * ```
 */
NIMCP_EXPORT int pr_sleep_bridge_run_sleep_cycles(
    pr_sleep_bridge_t bridge,
    uint32_t num_cycles
);

//=============================================================================
// Memory Replay Functions
//=============================================================================

/**
 * @brief Get replay candidates for current stage
 *
 * WHAT: Selects memories eligible for replay
 * WHY:  Not all memories replay; selection criteria vary by stage
 * HOW:  Filters by tier, strength, age, emotional salience, etc.
 *
 * @param bridge Sleep bridge
 * @param candidates Output array (caller-allocated)
 * @param max_candidates Maximum candidates to return
 * @param count Output: actual count returned
 * @return PR_SLEEP_SUCCESS or error code
 *
 * Selection criteria (stage-dependent):
 * - SWS: Z0/Z1 tier, moderate strength, recent encoding
 * - REM: Emotional memories (|quat.x| > threshold), procedural
 * - N2: Transitional, mixed criteria
 *
 * COMPLEXITY: O(N log K) for top-K selection
 *
 * EXAMPLE:
 * ```c
 * pr_replay_candidate_t candidates[100];
 * size_t count;
 * pr_sleep_bridge_get_replay_candidates(bridge, candidates, 100, &count);
 * for (size_t i = 0; i < count; i++) {
 *     printf("Candidate %lu: priority=%.3f\n",
 *            candidates[i].node_id, candidates[i].replay_priority);
 * }
 * ```
 */
NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_get_replay_candidates(
    pr_sleep_bridge_t bridge,
    pr_replay_candidate_t* candidates,
    size_t max_candidates,
    size_t* count
);

/**
 * @brief Replay specific memory
 *
 * WHAT: Execute replay for a single memory
 * WHY:  Fine-grained control over replay sequence
 * HOW:  Reinforce memory, update associations, check promotion
 *
 * @param bridge Sleep bridge
 * @param node_id Memory to replay
 * @param direction Replay direction
 * @param event Output: replay event record (can be NULL)
 * @return PR_SLEEP_SUCCESS or error code
 *
 * Replay effects:
 * - Increase memory strength by consolidation_delta
 * - Update quaternion consolidation component
 * - Check and execute Z-ladder promotion if eligible
 * - Strengthen entanglement with co-active memories
 *
 * COMPLEXITY: O(E) where E = entangled memories
 */
NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_replay(
    pr_sleep_bridge_t bridge,
    uint64_t node_id,
    pr_replay_direction_t direction,
    pr_replay_event_t* event
);

/**
 * @brief Replay sequence of memories
 *
 * WHAT: Execute replay for a sequence in specified direction
 * WHY:  Sequences preserve temporal relationships
 * HOW:  Replays in order (forward/reverse/random)
 *
 * @param bridge Sleep bridge
 * @param node_ids Array of memory IDs to replay
 * @param count Number of memories
 * @param direction Replay direction
 * @return Number of successful replays
 *
 * Sequence replay adds:
 * - Temporal association strengthening between adjacent items
 * - Order-dependent consolidation bonuses
 */
NIMCP_EXPORT int pr_sleep_bridge_replay_sequence(
    pr_sleep_bridge_t bridge,
    const uint64_t* node_ids,
    size_t count,
    pr_replay_direction_t direction
);

//=============================================================================
// Z-Ladder Promotion Functions
//=============================================================================

/**
 * @brief Promote eligible memories during sleep consolidation
 *
 * WHAT: Check and promote memories meeting tier advancement criteria
 * WHY:  Sleep accelerates memory consolidation to higher tiers
 * HOW:  Enhanced promotion criteria during sleep (especially SWS)
 *
 * @param bridge Sleep bridge
 * @return Number of memories promoted
 *
 * Sleep promotion differences:
 * - Lower strength threshold (promotion_threshold - sws_boost)
 * - Age requirements relaxed
 * - Replayed memories get extra eligibility
 *
 * EXAMPLE:
 * ```c
 * // During SWS, promote eligible memories
 * pr_sleep_bridge_set_stage(bridge, PR_SLEEP_STAGE_N3);
 * int promoted = pr_sleep_bridge_promote_z_ladder(bridge);
 * printf("Promoted %d memories during SWS\n", promoted);
 * ```
 */
NIMCP_EXPORT int pr_sleep_bridge_promote_z_ladder(pr_sleep_bridge_t bridge);

/**
 * @brief Promote specific memory with sleep boost
 *
 * WHAT: Attempt to promote a specific memory
 * WHY:  Target important memories for accelerated consolidation
 * HOW:  Apply sleep-specific promotion criteria
 *
 * @param bridge Sleep bridge
 * @param node_id Memory to promote
 * @return true if promoted, false if not eligible or error
 */
NIMCP_EXPORT bool pr_sleep_bridge_promote_memory(
    pr_sleep_bridge_t bridge,
    uint64_t node_id
);

/**
 * @brief Get promotion eligibility for memory in current sleep state
 *
 * WHAT: Calculate adjusted promotion eligibility
 * WHY:  Check before attempting promotion
 * HOW:  Combines base eligibility with sleep-stage bonuses
 *
 * @param bridge Sleep bridge
 * @param node_id Memory to check
 * @return Eligibility score [0, 1], or -1.0f on error
 */
NIMCP_EXPORT float pr_sleep_bridge_get_promotion_eligibility(
    pr_sleep_bridge_t bridge,
    uint64_t node_id
);

//=============================================================================
// Emotional Processing Functions (REM)
//=============================================================================

/**
 * @brief Process emotional memories during REM
 *
 * WHAT: Execute REM-specific emotional memory processing
 * WHY:  REM sleep reduces emotional intensity while preserving content
 * HOW:  Attenuates quaternion emotional component, integrates with schema
 *
 * @param bridge Sleep bridge
 * @return Number of emotional memories processed
 *
 * Emotional processing effects:
 * - Reduce |quaternion.x| by rem_emotional_factor
 * - Strengthen connections to similar-valence memories
 * - Integrate into broader memory schemas
 *
 * NEUROSCIENCE: Supports overnight therapy hypothesis
 *
 * EXAMPLE:
 * ```c
 * pr_sleep_bridge_set_stage(bridge, PR_SLEEP_STAGE_REM);
 * int processed = pr_sleep_bridge_emotional_process(bridge);
 * printf("Processed %d emotional memories\n", processed);
 * ```
 */
NIMCP_EXPORT int pr_sleep_bridge_emotional_process(pr_sleep_bridge_t bridge);

/**
 * @brief Process specific emotional memory
 *
 * WHAT: Apply emotional processing to single memory
 * WHY:  Fine-grained control over emotional processing
 *
 * @param bridge Sleep bridge
 * @param node_id Memory to process
 * @return New emotional magnitude, or -1.0f on error
 */
NIMCP_EXPORT float pr_sleep_bridge_process_emotional_memory(
    pr_sleep_bridge_t bridge,
    uint64_t node_id
);

/**
 * @brief Get emotional memories for REM processing
 *
 * WHAT: Select memories with high emotional magnitude
 * WHY:  REM preferentially processes emotional content
 * HOW:  Filter by |quaternion.x| > threshold
 *
 * @param bridge Sleep bridge
 * @param candidates Output array
 * @param max_candidates Maximum to return
 * @param count Output: actual count
 * @return PR_SLEEP_SUCCESS or error code
 */
NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_get_emotional_memories(
    pr_sleep_bridge_t bridge,
    pr_replay_candidate_t* candidates,
    size_t max_candidates,
    size_t* count
);

//=============================================================================
// Association Management Functions
//=============================================================================

/**
 * @brief Strengthen associations between co-replayed memories
 *
 * WHAT: Increase entanglement weights for memories replayed together
 * WHY:  Co-replay indicates association (Hebbian principle)
 * HOW:  Updates entanglement graph edge weights
 *
 * @param bridge Sleep bridge
 * @param node_ids Array of co-replayed memory IDs
 * @param count Number of memories
 * @return Number of associations strengthened
 */
NIMCP_EXPORT int pr_sleep_bridge_strengthen_associations(
    pr_sleep_bridge_t bridge,
    const uint64_t* node_ids,
    size_t count
);

/**
 * @brief Prune weak associations during REM
 *
 * WHAT: Remove low-weight entanglement edges
 * WHY:  REM sleep involves forgetting weak associations
 * HOW:  Removes edges below rem_prune_threshold
 *
 * @param bridge Sleep bridge
 * @return Number of associations pruned
 *
 * NOTE: Only active during REM stage
 */
NIMCP_EXPORT int pr_sleep_bridge_prune_weak_associations(
    pr_sleep_bridge_t bridge
);

//=============================================================================
// Callback Functions
//=============================================================================

/**
 * @brief Set callback for replay events
 *
 * @param bridge Sleep bridge
 * @param callback Function to call on replay (NULL to disable)
 * @param user_data Context passed to callback
 * @return PR_SLEEP_SUCCESS or error code
 */
NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_set_replay_callback(
    pr_sleep_bridge_t bridge,
    pr_replay_callback_t callback,
    void* user_data
);

/**
 * @brief Set callback for stage transitions
 *
 * @param bridge Sleep bridge
 * @param callback Function to call on transition (NULL to disable)
 * @param user_data Context passed to callback
 * @return PR_SLEEP_SUCCESS or error code
 */
NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_set_stage_callback(
    pr_sleep_bridge_t bridge,
    pr_stage_callback_t callback,
    void* user_data
);

/**
 * @brief Set callback for sleep promotions
 *
 * @param bridge Sleep bridge
 * @param callback Function to call on promotion (NULL to disable)
 * @param user_data Context passed to callback
 * @return PR_SLEEP_SUCCESS or error code
 */
NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_set_promotion_callback(
    pr_sleep_bridge_t bridge,
    pr_sleep_promotion_callback_t callback,
    void* user_data
);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get sleep bridge statistics
 *
 * @param bridge Sleep bridge
 * @param stats Output statistics structure
 * @return PR_SLEEP_SUCCESS or error code
 */
NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_get_stats(
    const pr_sleep_bridge_t bridge,
    pr_sleep_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * @param bridge Sleep bridge
 * @return PR_SLEEP_SUCCESS or error code
 */
NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_reset_stats(
    pr_sleep_bridge_t bridge
);

/**
 * @brief Get replay history
 *
 * WHAT: Retrieve recent replay events
 * WHY:  Analysis and debugging of consolidation
 * HOW:  Copies from internal history buffer
 *
 * @param bridge Sleep bridge
 * @param events Output array
 * @param max_events Maximum events to return
 * @param count Output: actual count
 * @return PR_SLEEP_SUCCESS or error code
 *
 * NOTE: Requires track_replay_history=true in config
 */
NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_get_replay_history(
    const pr_sleep_bridge_t bridge,
    pr_replay_event_t* events,
    size_t max_events,
    size_t* count
);

/**
 * @brief Clear replay history
 *
 * @param bridge Sleep bridge
 * @return PR_SLEEP_SUCCESS or error code
 */
NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_clear_replay_history(
    pr_sleep_bridge_t bridge
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error string for sleep bridge error code
 *
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* pr_sleep_error_string(pr_sleep_error_t error);

/**
 * @brief Get sleep stage name as string
 *
 * @param stage Sleep stage
 * @return Human-readable stage name
 */
NIMCP_EXPORT const char* pr_sleep_stage_name(pr_sleep_stage_t stage);

/**
 * @brief Get replay direction name as string
 *
 * @param direction Replay direction
 * @return Human-readable direction name
 */
NIMCP_EXPORT const char* pr_replay_direction_name(pr_replay_direction_t direction);

/**
 * @brief Get memory type name as string
 *
 * @param type Memory type
 * @return Human-readable type name
 */
NIMCP_EXPORT const char* pr_memory_type_name(pr_memory_type_t type);

/**
 * @brief Print sleep bridge state summary
 *
 * @param bridge Sleep bridge
 */
NIMCP_EXPORT void pr_sleep_bridge_print_summary(const pr_sleep_bridge_t bridge);

/**
 * @brief Get current time in milliseconds
 *
 * @return Milliseconds since epoch
 */
NIMCP_EXPORT uint64_t pr_sleep_current_time_ms(void);

/**
 * @brief Validate bridge internal consistency
 *
 * WHAT: Check internal data structure consistency
 * WHY:  Debug/test tool for corruption detection
 *
 * @param bridge Sleep bridge
 * @return true if consistent, false if corruption detected
 */
NIMCP_EXPORT bool pr_sleep_bridge_validate(const pr_sleep_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PR_SLEEP_BRIDGE_H
