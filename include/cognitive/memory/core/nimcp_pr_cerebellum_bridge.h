//=============================================================================
// nimcp_pr_cerebellum_bridge.h - Prime Resonant Cerebellum Bridge
//=============================================================================
/**
 * @file nimcp_pr_cerebellum_bridge.h
 * @brief Procedural memory and cerebellar timing integration for PR Memory
 *
 * WHAT: Integration between Prime Resonant memory and cerebellar-like timing,
 *       procedural memory, motor sequences, and error-based learning
 * WHY:  The cerebellum is critical for procedural memory, precise timing,
 *       motor sequence learning, and error-based adaptation
 * HOW:  Models cerebellar timing circuits, procedural memory encoding,
 *       motor sequence representation, and error signal integration
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Cerebellum and Memory:
 *   +-----------------------------------------------------------------------+
 *   |  The cerebellum contains more neurons than the rest of the brain     |
 *   |  combined and plays critical roles in:                                |
 *   |                                                                       |
 *   |  1. PROCEDURAL MEMORY: Skills, habits, motor sequences               |
 *   |  2. TIMING: Precise temporal processing (10-100ms resolution)        |
 *   |  3. ERROR CORRECTION: Climbing fiber error signals                   |
 *   |  4. PREDICTION: Forward models for action outcomes                   |
 *   |  5. SEQUENCE LEARNING: Temporal pattern acquisition                  |
 *   +-----------------------------------------------------------------------+
 *
 *   Cerebellar Circuit Architecture:
 *   +-----------------------------------------------------------------------+
 *   |                                                                       |
 *   |  Mossy Fibers -----> Granule Cells -----> Parallel Fibers            |
 *   |  (context input)    (expansion layer)    (sparse distributed code)   |
 *   |                            |                        |                |
 *   |                            v                        v                |
 *   |                     Purkinje Cells <---------- Climbing Fibers       |
 *   |                     (integration)              (error signals)       |
 *   |                            |                                         |
 *   |                            v                                         |
 *   |                     Deep Nuclei -----> Motor Output                  |
 *   |                     (output)                                         |
 *   |                                                                       |
 *   +-----------------------------------------------------------------------+
 *
 *   PR Memory - Cerebellum Mapping:
 *   +-----------------------------------------------------------------------+
 *   |  PR Memory Concept    | Cerebellar Analog           | Function       |
 *   |-----------------------|-----------------------------|----------------|
 *   |  Memory Node          | Purkinje cell pattern       | Storage unit   |
 *   |  Quaternion.w         | Synaptic weight strength    | Consolidation  |
 *   |  Quaternion.z         | Timing precision            | Accessibility  |
 *   |  Z-Ladder tier        | Automatization level        | Skill mastery  |
 *   |  Entanglement         | Sequence connections        | Chaining       |
 *   |  Prime signature      | Sparse granule code         | Content ID     |
 *   +-----------------------------------------------------------------------+
 *
 *   Timing Memory Model:
 *   +-----------------------------------------------------------------------+
 *   |  The cerebellum maintains precise temporal representations:          |
 *   |                                                                       |
 *   |  +------------------+                                                |
 *   |  | Timing Memory    |  Stores duration information (ms precision)   |
 *   |  | - Interval       |  Time between events                          |
 *   |  | - Phase          |  Position within rhythmic cycle               |
 *   |  | - Sequence       |  Order within motor sequence                  |
 *   |  +------------------+                                                |
 *   |                                                                       |
 *   |  Timing affects memory retrieval:                                    |
 *   |  - Correct timing: Enhanced accessibility (z boost)                  |
 *   |  - Incorrect timing: Reduced accessibility + error signal            |
 *   +-----------------------------------------------------------------------+
 *
 *   Error-Based Learning:
 *   +-----------------------------------------------------------------------+
 *   |  Climbing Fiber Error Signal:                                        |
 *   |                                                                       |
 *   |  error = expected_outcome - actual_outcome                           |
 *   |                                                                       |
 *   |  Effects:                                                             |
 *   |  - Error > 0: Increase consolidation (need more practice)            |
 *   |  - Error < 0: Unexpected success (reassess timing)                   |
 *   |  - Error ~ 0: Maintain current state (skill mastered)                |
 *   |                                                                       |
 *   |  LTD (Long-Term Depression): Climbing fiber + parallel fiber         |
 *   |  coincidence weakens Purkinje synapse (error correction)             |
 *   +-----------------------------------------------------------------------+
 *
 *   Motor Sequence Memory:
 *   +-----------------------------------------------------------------------+
 *   |  Sequences are stored as chains of timing-linked memories:           |
 *   |                                                                       |
 *   |  [M1] --dt1--> [M2] --dt2--> [M3] --dt3--> [M4]                      |
 *   |                                                                       |
 *   |  Each link stores:                                                   |
 *   |  - Inter-element interval (dt)                                       |
 *   |  - Transition probability                                            |
 *   |  - Error history                                                     |
 *   |  - Execution count                                                   |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Timing sync: O(1)
 * - Error signal processing: O(1)
 * - Sequence retrieval: O(sequence_length)
 * - Procedural memory sync: O(entanglement_count)
 *
 * MEMORY:
 * - pr_cerebellum_bridge_t: ~2KB base structure
 * - Sequence buffer: configurable
 * - Timing history: configurable
 *
 * THREAD SAFETY:
 * - All public functions are thread-safe via internal mutex
 *
 * INTEGRATION:
 * - Core: PR memory nodes, entanglement graph
 * - Z-Ladder: Automatization/skill mastery levels
 * - Bio-Async: Motor output coordination
 *
 * REFERENCES:
 * - Ito (2008): Cerebellar circuitry as a neuronal machine
 * - Ivry & Spencer (2004): Cerebellar timing
 * - Medina & Lisberger (2008): Cerebellar motor learning
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_PR_CEREBELLUM_BRIDGE_H
#define NIMCP_PR_CEREBELLUM_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Dependencies
#include "cognitive/memory/core/nimcp_quaternion.h"
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_entanglement.h"

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

/** Maximum sequence length for motor sequences */
#define PR_CEREB_MAX_SEQUENCE_LENGTH        64

/** Maximum concurrent sequences tracked */
#define PR_CEREB_MAX_SEQUENCES              256

/** Default timing precision (milliseconds) */
#define PR_CEREB_DEFAULT_TIMING_PRECISION   10.0f

/** Maximum timing error before memory weakening */
#define PR_CEREB_MAX_TIMING_ERROR_MS        100.0f

/** Default error decay rate per second */
#define PR_CEREB_ERROR_DECAY_RATE           0.2f

/** Error learning rate (how much error affects consolidation) */
#define PR_CEREB_ERROR_LEARNING_RATE        0.1f

/** Default timing history buffer size */
#define PR_CEREB_DEFAULT_HISTORY_SIZE       512

/** Timing window for coincidence detection (ms) */
#define PR_CEREB_COINCIDENCE_WINDOW_MS      20.0f

/** Automatization threshold (above this, skill is "automatic") */
#define PR_CEREB_AUTOMATIZATION_THRESHOLD   0.8f

/** LTD factor for error-coincidence learning */
#define PR_CEREB_LTD_FACTOR                 0.05f

/** LTP factor for correct execution */
#define PR_CEREB_LTP_FACTOR                 0.02f

/** Epsilon for floating-point comparisons */
#define PR_CEREB_EPSILON                    1e-6f

//=============================================================================
// Type Definitions - Enumerations
//=============================================================================

/**
 * @brief Procedural memory types
 *
 * WHAT: Categories of procedural/skill memory
 * WHY:  Different procedural memories have different timing requirements
 */
typedef enum {
    PR_PROC_MOTOR_SEQUENCE = 0,     /**< Motor action sequence */
    PR_PROC_TIMING_PATTERN,         /**< Rhythmic/timing pattern */
    PR_PROC_COGNITIVE_SKILL,        /**< Cognitive procedure */
    PR_PROC_PERCEPTUAL_SKILL,       /**< Perceptual discrimination */
    PR_PROC_HABIT,                  /**< Automatized behavior */
    PR_PROC_TYPE_COUNT              /**< Number of procedural types */
} pr_procedural_type_t;

/**
 * @brief Sequence execution state
 *
 * WHAT: Current state of sequence execution
 * WHY:  Track progress through motor sequences
 */
typedef enum {
    PR_SEQ_IDLE = 0,                /**< No sequence active */
    PR_SEQ_INITIATING,              /**< Starting sequence */
    PR_SEQ_EXECUTING,               /**< Mid-execution */
    PR_SEQ_COMPLETING,              /**< Finishing sequence */
    PR_SEQ_ERROR,                   /**< Error occurred */
    PR_SEQ_ABORTED                  /**< Sequence aborted */
} pr_sequence_state_t;

/**
 * @brief Error signal types
 *
 * WHAT: Types of error signals (climbing fiber analogs)
 * WHY:  Different errors require different corrections
 */
typedef enum {
    PR_ERROR_TIMING = 0,            /**< Timing error (too early/late) */
    PR_ERROR_SEQUENCE,              /**< Wrong sequence element */
    PR_ERROR_AMPLITUDE,             /**< Wrong movement magnitude */
    PR_ERROR_COORDINATION,          /**< Inter-limb coordination error */
    PR_ERROR_PREDICTION,            /**< Outcome prediction error */
    PR_ERROR_TYPE_COUNT             /**< Number of error types */
} pr_error_type_t;

/**
 * @brief Automatization level
 *
 * WHAT: Level of skill automatization
 * WHY:  Skills progress from effortful to automatic
 */
typedef enum {
    PR_AUTO_NOVICE = 0,             /**< Requires conscious attention */
    PR_AUTO_ADVANCED,               /**< Reduced attention needed */
    PR_AUTO_PROFICIENT,             /**< Mostly automatic */
    PR_AUTO_EXPERT,                 /**< Fully automatic */
    PR_AUTO_LEVEL_COUNT             /**< Number of levels */
} pr_automatization_level_t;

/**
 * @brief Cerebellum bridge error codes
 */
typedef enum {
    PR_CEREB_SUCCESS = 0,                   /**< Operation succeeded */
    PR_CEREB_ERROR_NULL_POINTER = -1,       /**< NULL pointer argument */
    PR_CEREB_ERROR_NO_MEMORY = -2,          /**< Memory allocation failed */
    PR_CEREB_ERROR_NOT_INITIALIZED = -3,    /**< Bridge not initialized */
    PR_CEREB_ERROR_INVALID_CONFIG = -4,     /**< Invalid configuration */
    PR_CEREB_ERROR_SEQUENCE_FULL = -5,      /**< Sequence buffer full */
    PR_CEREB_ERROR_SEQUENCE_NOT_FOUND = -6, /**< Sequence not found */
    PR_CEREB_ERROR_INVALID_TIMING = -7,     /**< Invalid timing value */
    PR_CEREB_ERROR_NO_ENTANGLEMENT = -8     /**< Entanglement graph not set */
} pr_cerebellum_error_t;

//=============================================================================
// Type Definitions - Structures
//=============================================================================

/**
 * @brief Timing information for a memory element
 *
 * WHAT: Temporal parameters associated with a procedural memory
 * WHY:  Cerebellum maintains precise timing for each learned element
 */
typedef struct {
    float expected_interval_ms;     /**< Expected time to this element */
    float actual_interval_ms;       /**< Last actual interval */
    float variance_ms;              /**< Timing variance (learning history) */
    float phase;                    /**< Phase within rhythmic cycle [0, 2pi] */
    uint32_t execution_count;       /**< Times this element executed */
    float timing_accuracy;          /**< Historical accuracy [0, 1] */
} pr_timing_info_t;

/**
 * @brief Sequence element
 *
 * WHAT: Single element in a motor/procedural sequence
 * WHY:  Sequences are chains of timed elements
 */
typedef struct {
    uint64_t memory_id;             /**< Associated memory node */
    pr_timing_info_t timing;        /**< Timing for this element */
    float transition_prob;          /**< Probability of this transition */
    uint32_t position;              /**< Position in sequence (0-indexed) */
    bool is_branch_point;           /**< Multiple possible next elements */
} pr_sequence_element_t;

/**
 * @brief Motor/procedural sequence
 *
 * WHAT: A complete sequence of procedural memory elements
 * WHY:  Skills are often sequences of actions with precise timing
 */
typedef struct {
    uint64_t sequence_id;           /**< Unique sequence identifier */
    char name[64];                  /**< Human-readable name */
    pr_procedural_type_t type;      /**< Type of procedural memory */

    /* Sequence elements */
    pr_sequence_element_t* elements;/**< Array of sequence elements */
    size_t length;                  /**< Number of elements */
    size_t capacity;                /**< Allocated capacity */

    /* Execution state */
    pr_sequence_state_t state;      /**< Current execution state */
    size_t current_position;        /**< Current position in sequence */
    uint64_t execution_start_ms;    /**< When current execution started */
    uint64_t last_element_time_ms;  /**< Time of last element execution */

    /* Learning state */
    float consolidation;            /**< Overall sequence consolidation [0, 1] */
    float automatization;           /**< Automatization level [0, 1] */
    uint32_t total_executions;      /**< Total times sequence executed */
    uint32_t successful_executions; /**< Error-free executions */
    float avg_error;                /**< Running average error */

    /* Metadata */
    uint64_t created_time_ms;       /**< When sequence was created */
    uint64_t last_executed_ms;      /**< Last successful execution */
} pr_sequence_t;

/**
 * @brief Error signal
 *
 * WHAT: A cerebellar error signal (climbing fiber analog)
 * WHY:  Errors drive learning and memory modification
 */
typedef struct {
    pr_error_type_t type;           /**< Error type */
    float magnitude;                /**< Error magnitude [-1, +1] */
    float expected_value;           /**< What was expected */
    float actual_value;             /**< What actually occurred */
    uint64_t memory_id;             /**< Associated memory (if any) */
    uint64_t sequence_id;           /**< Associated sequence (if any) */
    uint32_t sequence_position;     /**< Position in sequence */
    uint64_t timestamp_ms;          /**< When error occurred */
} pr_error_signal_t;

/**
 * @brief Timing history entry
 *
 * WHAT: Record of timing event for analysis
 * WHY:  Track timing patterns over time
 */
typedef struct {
    uint64_t memory_id;             /**< Memory that was accessed */
    uint64_t sequence_id;           /**< Sequence (if part of sequence) */
    float expected_interval_ms;     /**< Expected timing */
    float actual_interval_ms;       /**< Actual timing */
    float error_ms;                 /**< Timing error */
    uint64_t timestamp_ms;          /**< Event timestamp */
} pr_timing_history_t;

/**
 * @brief Bridge configuration
 *
 * WHAT: Parameters controlling cerebellum bridge behavior
 * WHY:  Allow customization for different applications
 */
typedef struct {
    /* Sequence parameters */
    size_t max_sequences;           /**< Maximum concurrent sequences */
    size_t max_sequence_length;     /**< Maximum elements per sequence */

    /* Timing parameters */
    float timing_precision_ms;      /**< Timing precision threshold */
    float max_timing_error_ms;      /**< Maximum tolerated timing error */
    float coincidence_window_ms;    /**< Window for coincidence detection */

    /* Learning parameters */
    float error_learning_rate;      /**< How much errors affect learning */
    float error_decay_rate;         /**< Error signal decay rate */
    float ltd_factor;               /**< Long-term depression factor */
    float ltp_factor;               /**< Long-term potentiation factor */

    /* Automatization parameters */
    float automatization_threshold; /**< Threshold for "automatic" skill */
    uint32_t min_executions_for_auto; /**< Min executions before automatic */

    /* History tracking */
    size_t timing_history_size;     /**< Timing history buffer size */
    bool track_timing_history;      /**< Enable timing history */

    /* Integration */
    bool enable_z_ladder_sync;      /**< Sync automatization with Z-ladder */
    bool enable_entanglement_update;/**< Update entanglement on sequence exec */
} pr_cerebellum_config_t;

/**
 * @brief Bridge statistics
 *
 * WHAT: Operational metrics for the cerebellum bridge
 * WHY:  Monitor procedural memory and timing performance
 */
typedef struct {
    /* Sequence statistics */
    uint64_t sequences_created;     /**< Total sequences created */
    uint64_t sequences_executed;    /**< Total sequence executions */
    uint64_t sequences_completed;   /**< Successful completions */
    uint64_t sequences_aborted;     /**< Aborted sequences */
    float avg_sequence_length;      /**< Average sequence length */

    /* Timing statistics */
    uint64_t timing_events;         /**< Total timing events */
    float avg_timing_error_ms;      /**< Average timing error */
    float max_timing_error_ms;      /**< Maximum timing error seen */
    float timing_accuracy;          /**< Overall timing accuracy [0, 1] */

    /* Error statistics */
    uint64_t error_signals;         /**< Total error signals */
    uint64_t errors_by_type[PR_ERROR_TYPE_COUNT]; /**< Per-type counts */
    float avg_error_magnitude;      /**< Average error magnitude */
    uint64_t corrections_applied;   /**< Memory corrections from errors */

    /* Learning statistics */
    uint64_t ltd_events;            /**< Long-term depression events */
    uint64_t ltp_events;            /**< Long-term potentiation events */
    float avg_consolidation_change; /**< Average consolidation change */
    uint64_t automatizations;       /**< Skills reaching automatic level */

    /* Memory impact */
    uint64_t memories_modified;     /**< Memories modified by timing/error */
    float avg_accessibility_change; /**< Average z-component change */

    /* Timing */
    uint64_t last_update_ms;        /**< Last activity timestamp */
} pr_cerebellum_stats_t;

/**
 * @brief Callback for sequence events
 *
 * @param sequence The sequence
 * @param event Event type (started, completed, error, etc.)
 * @param user_data User-provided context
 */
typedef void (*pr_sequence_callback_t)(
    const pr_sequence_t* sequence,
    pr_sequence_state_t event,
    void* user_data
);

/**
 * @brief Callback for error signals
 *
 * @param error The error signal
 * @param user_data User-provided context
 */
typedef void (*pr_error_callback_t)(
    const pr_error_signal_t* error,
    void* user_data
);

/**
 * @brief Opaque cerebellum bridge handle
 */
typedef struct pr_cerebellum_bridge_struct* pr_cerebellum_bridge_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible defaults based on cerebellar timing literature
 * WHY:  Provides starting point for typical procedural memory
 *
 * @return Default configuration structure
 */
NIMCP_EXPORT pr_cerebellum_config_t pr_cerebellum_config_default(void);

/**
 * @brief Validate bridge configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
NIMCP_EXPORT bool pr_cerebellum_config_validate(const pr_cerebellum_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create cerebellum bridge
 *
 * WHAT: Allocates and initializes cerebellum bridge
 * WHY:  Entry point for procedural memory integration
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 *
 * COMPLEXITY: O(max_sequences)
 * MEMORY: ~2KB + sequence buffers + history
 */
NIMCP_EXPORT pr_cerebellum_bridge_t pr_cerebellum_bridge_create(
    const pr_cerebellum_config_t* config
);

/**
 * @brief Destroy cerebellum bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
NIMCP_EXPORT void pr_cerebellum_bridge_destroy(pr_cerebellum_bridge_t bridge);

/**
 * @brief Set entanglement graph for sequence connectivity
 *
 * WHAT: Associates entanglement graph with bridge
 * WHY:  Sequences use entanglement for element connectivity
 *
 * @param bridge Cerebellum bridge
 * @param graph Entanglement graph to use (can be NULL)
 * @return PR_CEREB_SUCCESS or error code
 */
NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_set_entanglement(
    pr_cerebellum_bridge_t bridge,
    entangle_graph_t graph
);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Cerebellum bridge
 * @return PR_CEREB_SUCCESS or error code
 */
NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_reset(
    pr_cerebellum_bridge_t bridge
);

//=============================================================================
// Sequence Management Functions
//=============================================================================

/**
 * @brief Create a new sequence
 *
 * WHAT: Create a new motor/procedural sequence
 * WHY:  Entry point for defining skills
 *
 * @param bridge Cerebellum bridge
 * @param name Human-readable name (max 63 chars)
 * @param type Procedural memory type
 * @param sequence_id Output: assigned sequence ID
 * @return PR_CEREB_SUCCESS or error code
 *
 * EXAMPLE:
 * ```c
 * uint64_t seq_id;
 * pr_cerebellum_bridge_create_sequence(bridge, "typing_hello",
 *     PR_PROC_MOTOR_SEQUENCE, &seq_id);
 * ```
 */
NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_create_sequence(
    pr_cerebellum_bridge_t bridge,
    const char* name,
    pr_procedural_type_t type,
    uint64_t* sequence_id
);

/**
 * @brief Add element to sequence
 *
 * WHAT: Add a memory element to a sequence
 * WHY:  Build up sequences element by element
 *
 * @param bridge Cerebellum bridge
 * @param sequence_id Sequence to add to
 * @param memory_id Memory node for this element
 * @param interval_ms Expected interval from previous element
 * @return PR_CEREB_SUCCESS or error code
 *
 * EXAMPLE:
 * ```c
 * // Build typing sequence
 * pr_cerebellum_bridge_add_element(bridge, seq_id, mem_h, 0);    // First element
 * pr_cerebellum_bridge_add_element(bridge, seq_id, mem_e, 100);  // 100ms later
 * pr_cerebellum_bridge_add_element(bridge, seq_id, mem_l, 120);  // 120ms later
 * pr_cerebellum_bridge_add_element(bridge, seq_id, mem_l, 110);  // 110ms later
 * pr_cerebellum_bridge_add_element(bridge, seq_id, mem_o, 150);  // 150ms later
 * ```
 */
NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_add_element(
    pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id,
    uint64_t memory_id,
    float interval_ms
);

/**
 * @brief Get sequence by ID
 *
 * @param bridge Cerebellum bridge
 * @param sequence_id Sequence ID
 * @param sequence Output: copy of sequence (caller should not modify elements)
 * @return PR_CEREB_SUCCESS or error code
 */
NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_get_sequence(
    const pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id,
    pr_sequence_t* sequence
);

/**
 * @brief Delete sequence
 *
 * @param bridge Cerebellum bridge
 * @param sequence_id Sequence to delete
 * @return PR_CEREB_SUCCESS or error code
 */
NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_delete_sequence(
    pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id
);

/**
 * @brief Get sequence count
 *
 * @param bridge Cerebellum bridge
 * @return Number of sequences
 */
NIMCP_EXPORT size_t pr_cerebellum_bridge_get_sequence_count(
    const pr_cerebellum_bridge_t bridge
);

//=============================================================================
// Procedural Memory Sync Functions
//=============================================================================

/**
 * @brief Sync procedural memory with timing
 *
 * WHAT: Apply cerebellar timing information to memory state
 * WHY:  Procedural memories have timing components affecting retrieval
 *
 * @param bridge Cerebellum bridge
 * @param node Memory node to sync
 * @return PR_CEREB_SUCCESS or error code
 *
 * Effects on quaternion:
 * - z (accessibility): Boosted if timing is correct
 * - w (consolidation): Affected by practice/error history
 */
NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_sync_procedural(
    pr_cerebellum_bridge_t bridge,
    pr_memory_node_t* node
);

/**
 * @brief Sync sequence timing with PR memory
 *
 * WHAT: Update PR memory nodes based on sequence timing
 * WHY:  Keep memory states consistent with sequence performance
 *
 * @param bridge Cerebellum bridge
 * @param sequence_id Sequence to sync
 * @return Number of memories synced, or -1 on error
 */
NIMCP_EXPORT int pr_cerebellum_bridge_sync_sequence(
    pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id
);

/**
 * @brief Get automatization level for a skill
 *
 * WHAT: Determine how automatic a skill has become
 * WHY:  Skills progress from conscious to automatic
 *
 * @param bridge Cerebellum bridge
 * @param sequence_id Sequence to check
 * @return Automatization level
 */
NIMCP_EXPORT pr_automatization_level_t pr_cerebellum_bridge_get_automatization(
    const pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id
);

//=============================================================================
// Timing Memory Functions
//=============================================================================

/**
 * @brief Record timing event for memory
 *
 * WHAT: Record that a memory was accessed with specific timing
 * WHY:  Build timing model for procedural memory
 *
 * @param bridge Cerebellum bridge
 * @param memory_id Memory that was accessed
 * @param interval_ms Actual interval from previous event
 * @return PR_CEREB_SUCCESS or error code
 */
NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_timing_memory(
    pr_cerebellum_bridge_t bridge,
    uint64_t memory_id,
    float interval_ms
);

/**
 * @brief Record timing event in sequence context
 *
 * WHAT: Record timing for a memory within a sequence
 * WHY:  Sequence context affects timing expectations
 *
 * @param bridge Cerebellum bridge
 * @param sequence_id Sequence context
 * @param memory_id Memory accessed
 * @param interval_ms Actual interval
 * @return Timing error (ms), or NaN on error
 */
NIMCP_EXPORT float pr_cerebellum_bridge_timing_in_sequence(
    pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id,
    uint64_t memory_id,
    float interval_ms
);

/**
 * @brief Get expected timing for memory in sequence
 *
 * @param bridge Cerebellum bridge
 * @param sequence_id Sequence context
 * @param position Position in sequence
 * @return Expected interval in ms, or -1.0f on error
 */
NIMCP_EXPORT float pr_cerebellum_bridge_get_expected_timing(
    const pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id,
    size_t position
);

/**
 * @brief Get timing variance for memory
 *
 * @param bridge Cerebellum bridge
 * @param sequence_id Sequence context
 * @param position Position in sequence
 * @return Timing variance in ms, or -1.0f on error
 */
NIMCP_EXPORT float pr_cerebellum_bridge_get_timing_variance(
    const pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id,
    size_t position
);

//=============================================================================
// Error Signal Functions
//=============================================================================

/**
 * @brief Process error signal (climbing fiber analog)
 *
 * WHAT: Process error signal for learning
 * WHY:  Errors drive cerebellar learning
 *
 * @param bridge Cerebellum bridge
 * @param type Error type
 * @param expected Expected value
 * @param actual Actual value
 * @param memory_id Associated memory (0 if none)
 * @return PR_CEREB_SUCCESS or error code
 *
 * Effects:
 * - Timing error: Adjusts timing expectations
 * - Sequence error: May trigger LTD
 * - Positive learning signal (actual better than expected): May trigger LTP
 *
 * EXAMPLE:
 * ```c
 * // Keystroke was 50ms late
 * pr_cerebellum_bridge_error_signal(bridge,
 *     PR_ERROR_TIMING, 100.0f, 150.0f, memory_id);
 * ```
 */
NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_error_signal(
    pr_cerebellum_bridge_t bridge,
    pr_error_type_t type,
    float expected,
    float actual,
    uint64_t memory_id
);

/**
 * @brief Process error signal in sequence context
 *
 * @param bridge Cerebellum bridge
 * @param type Error type
 * @param expected Expected value
 * @param actual Actual value
 * @param sequence_id Sequence context
 * @param position Position in sequence where error occurred
 * @return PR_CEREB_SUCCESS or error code
 */
NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_error_in_sequence(
    pr_cerebellum_bridge_t bridge,
    pr_error_type_t type,
    float expected,
    float actual,
    uint64_t sequence_id,
    size_t position
);

/**
 * @brief Apply LTD (Long-Term Depression) to memory
 *
 * WHAT: Weaken memory based on error signal
 * WHY:  Error + coincidence should reduce incorrect response
 *
 * @param bridge Cerebellum bridge
 * @param memory_id Memory to weaken
 * @param ltd_amount Amount of weakening [0, 1]
 * @return New consolidation value, or -1.0f on error
 */
NIMCP_EXPORT float pr_cerebellum_bridge_apply_ltd(
    pr_cerebellum_bridge_t bridge,
    uint64_t memory_id,
    float ltd_amount
);

/**
 * @brief Apply LTP (Long-Term Potentiation) to memory
 *
 * WHAT: Strengthen memory based on correct execution
 * WHY:  Correct execution should strengthen response
 *
 * @param bridge Cerebellum bridge
 * @param memory_id Memory to strengthen
 * @param ltp_amount Amount of strengthening [0, 1]
 * @return New consolidation value, or -1.0f on error
 */
NIMCP_EXPORT float pr_cerebellum_bridge_apply_ltp(
    pr_cerebellum_bridge_t bridge,
    uint64_t memory_id,
    float ltp_amount
);

//=============================================================================
// Sequence Execution Functions
//=============================================================================

/**
 * @brief Start executing a sequence
 *
 * @param bridge Cerebellum bridge
 * @param sequence_id Sequence to execute
 * @return PR_CEREB_SUCCESS or error code
 */
NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_start_sequence(
    pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id
);

/**
 * @brief Execute next element in sequence
 *
 * @param bridge Cerebellum bridge
 * @param sequence_id Sequence being executed
 * @param interval_ms Actual interval from last element
 * @param element_id Output: memory ID of current element
 * @return PR_CEREB_SUCCESS, PR_CEREB_ERROR_SEQUENCE_NOT_FOUND if done, or error
 */
NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_execute_next(
    pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id,
    float interval_ms,
    uint64_t* element_id
);

/**
 * @brief Complete sequence execution
 *
 * @param bridge Cerebellum bridge
 * @param sequence_id Sequence to complete
 * @param success Whether execution was successful
 * @return PR_CEREB_SUCCESS or error code
 */
NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_complete_sequence(
    pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id,
    bool success
);

/**
 * @brief Abort sequence execution
 *
 * @param bridge Cerebellum bridge
 * @param sequence_id Sequence to abort
 * @return PR_CEREB_SUCCESS or error code
 */
NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_abort_sequence(
    pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id
);

/**
 * @brief Get current sequence position
 *
 * @param bridge Cerebellum bridge
 * @param sequence_id Sequence to query
 * @return Current position, or -1 if not executing
 */
NIMCP_EXPORT int pr_cerebellum_bridge_get_position(
    const pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id
);

//=============================================================================
// Callback Functions
//=============================================================================

/**
 * @brief Set callback for sequence events
 *
 * @param bridge Cerebellum bridge
 * @param callback Function to call (NULL to disable)
 * @param user_data Context passed to callback
 * @return PR_CEREB_SUCCESS or error code
 */
NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_set_sequence_callback(
    pr_cerebellum_bridge_t bridge,
    pr_sequence_callback_t callback,
    void* user_data
);

/**
 * @brief Set callback for error signals
 *
 * @param bridge Cerebellum bridge
 * @param callback Function to call (NULL to disable)
 * @param user_data Context passed to callback
 * @return PR_CEREB_SUCCESS or error code
 */
NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_set_error_callback(
    pr_cerebellum_bridge_t bridge,
    pr_error_callback_t callback,
    void* user_data
);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Cerebellum bridge
 * @param stats Output statistics structure
 * @return PR_CEREB_SUCCESS or error code
 */
NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_get_stats(
    const pr_cerebellum_bridge_t bridge,
    pr_cerebellum_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * @param bridge Cerebellum bridge
 * @return PR_CEREB_SUCCESS or error code
 */
NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_reset_stats(
    pr_cerebellum_bridge_t bridge
);

/**
 * @brief Get timing history
 *
 * @param bridge Cerebellum bridge
 * @param entries Output array
 * @param max_entries Maximum entries to return
 * @param count Output: actual count returned
 * @return PR_CEREB_SUCCESS or error code
 */
NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_get_timing_history(
    const pr_cerebellum_bridge_t bridge,
    pr_timing_history_t* entries,
    size_t max_entries,
    size_t* count
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error string for error code
 *
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* pr_cerebellum_error_string(pr_cerebellum_error_t error);

/**
 * @brief Get procedural type name
 *
 * @param type Procedural type
 * @return Human-readable type name
 */
NIMCP_EXPORT const char* pr_procedural_type_name(pr_procedural_type_t type);

/**
 * @brief Get sequence state name
 *
 * @param state Sequence state
 * @return Human-readable state name
 */
NIMCP_EXPORT const char* pr_sequence_state_name(pr_sequence_state_t state);

/**
 * @brief Get error type name
 *
 * @param type Error type
 * @return Human-readable type name
 */
NIMCP_EXPORT const char* pr_error_type_name(pr_error_type_t type);

/**
 * @brief Get automatization level name
 *
 * @param level Automatization level
 * @return Human-readable level name
 */
NIMCP_EXPORT const char* pr_automatization_level_name(pr_automatization_level_t level);

/**
 * @brief Print sequence summary
 *
 * @param sequence Sequence to print
 */
NIMCP_EXPORT void pr_cerebellum_print_sequence(const pr_sequence_t* sequence);

/**
 * @brief Print bridge summary
 *
 * @param bridge Cerebellum bridge
 */
NIMCP_EXPORT void pr_cerebellum_bridge_print_summary(const pr_cerebellum_bridge_t bridge);

/**
 * @brief Get current time in milliseconds
 *
 * @return Milliseconds since epoch
 */
NIMCP_EXPORT uint64_t pr_cerebellum_current_time_ms(void);

/**
 * @brief Validate bridge internal consistency
 *
 * @param bridge Cerebellum bridge
 * @return true if consistent, false if corruption detected
 */
NIMCP_EXPORT bool pr_cerebellum_bridge_validate(const pr_cerebellum_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PR_CEREBELLUM_BRIDGE_H
