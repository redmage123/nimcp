//=============================================================================
// nimcp_prospective.h - Prospective Memory System for Future Intentions
//=============================================================================
/**
 * @file nimcp_prospective.h
 * @brief "Remembering to Remember" - Memory for Future Intentions
 *
 * WHAT: Prospective memory system for storing and monitoring future intentions
 * WHY:  Enable planning, goal-directed behavior, and delayed action execution
 * HOW:  Intention nodes with trigger conditions, monitored via theta-gamma cycles
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Prospective Memory Types:
 *   +-----------------------------------------------------------------------+
 *   |  The brain maintains several types of future-oriented memory:        |
 *   |                                                                       |
 *   |  TIME-BASED:                                                          |
 *   |  - "At 3pm, call the doctor"                                         |
 *   |  - Relies on time monitoring (striatum, prefrontal cortex)           |
 *   |  - Clock-checking behavior increases near target time                |
 *   |                                                                       |
 *   |  EVENT-BASED:                                                         |
 *   |  - "When I see John, give him the book"                              |
 *   |  - Cue detection (hippocampus) triggers intention retrieval          |
 *   |  - Prospective component: detect cue                                 |
 *   |  - Retrospective component: remember what to do                      |
 *   |                                                                       |
 *   |  ACTIVITY-BASED:                                                      |
 *   |  - "After finishing this task, start the backup"                     |
 *   |  - Requires task boundary detection (executive function)             |
 *   |  - Prefrontal cortex monitors activity state                         |
 *   |                                                                       |
 *   |  LOCATION-BASED (OPTIONAL):                                           |
 *   |  - "When I get home, water the plants"                               |
 *   |  - Spatial context as trigger (hippocampal place cells)              |
 *   +-----------------------------------------------------------------------+
 *
 *   Neural Mechanisms:
 *   +-----------------------------------------------------------------------+
 *   |  Prefrontal Cortex:                                                   |
 *   |  - Maintains intention in delayed response tasks                     |
 *   |  - Rostral PFC (BA10) particularly important for PM                  |
 *   |  - Cost: Occupies limited WM resources during monitoring             |
 *   |                                                                       |
 *   |  Hippocampus:                                                         |
 *   |  - Encodes intention-cue associations                                |
 *   |  - Pattern completion: partial cue -> full intention                 |
 *   |  - Theta rhythm coordinates encoding/retrieval                       |
 *   |                                                                       |
 *   |  Striatum:                                                            |
 *   |  - Time interval estimation and monitoring                           |
 *   |  - Dopaminergic reward prediction at target time                     |
 *   |                                                                       |
 *   |  Anterior Cingulate:                                                  |
 *   |  - Conflict monitoring when intention competes with ongoing task     |
 *   |  - Error detection for missed intentions                             |
 *   +-----------------------------------------------------------------------+
 *
 *   Intention Lifecycle:
 *   +-----------------------------------------------------------------------+
 *   |  1. FORMATION: Create intention with trigger and action              |
 *   |     -> Encode in episodic memory (hippocampus)                       |
 *   |     -> Establish intention-cue association                           |
 *   |                                                                       |
 *   |  2. RETENTION: Maintain during delay interval                        |
 *   |     -> Periodic reactivation (theta rhythm)                          |
 *   |     -> Background monitoring consumes resources                      |
 *   |                                                                       |
 *   |  3. INITIATION: Trigger condition detected                           |
 *   |     -> Cue activates intention via pattern completion                |
 *   |     -> Interrupt current task if necessary                           |
 *   |                                                                       |
 *   |  4. EXECUTION: Perform intended action                               |
 *   |     -> Retrieved intention -> action execution                       |
 *   |     -> Mark as completed in memory                                   |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Intention creation: O(1) + signature computation
 * - Update cycle: O(n) where n = active intentions
 * - Trigger check: O(1) per intention (time) or O(signature) (event)
 * - Memory: ~256 bytes per intention + action description
 *
 * INTEGRATION:
 * - Core: PR Memory nodes for intention storage
 * - Theta-Gamma: Periodic monitoring during encoding phase
 * - Entanglement: Link intentions to related memories
 * - Resonance: Event trigger detection via signature similarity
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_PROSPECTIVE_H
#define NIMCP_PROSPECTIVE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Dependencies
#include "cognitive/memory/core/nimcp_quaternion.h"
#include "cognitive/memory/core/nimcp_prime_signature.h"
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_theta_gamma.h"
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

/** Maximum number of intentions per prospective memory system */
#define PROSP_MAX_INTENTIONS                1024

/** Maximum length of action description string */
#define PROSP_MAX_ACTION_DESCRIPTION        512

/** Maximum active monitors (currently being watched) */
#define PROSP_MAX_ACTIVE_MONITORS           128

/** Default importance for new intentions */
#define PROSP_DEFAULT_IMPORTANCE            5.0f

/** Default urgency for new intentions */
#define PROSP_DEFAULT_URGENCY               5.0f

/** Default similarity threshold for event matching */
#define PROSP_DEFAULT_SIMILARITY_THRESHOLD  0.7f

/** Default time window before trigger (seconds) for alerts */
#define PROSP_DEFAULT_WINDOW_BEFORE         300.0f  // 5 minutes

/** Default grace period after trigger (seconds) */
#define PROSP_DEFAULT_WINDOW_AFTER          60.0f   // 1 minute

/** Activation decay rate per second */
#define PROSP_ACTIVATION_DECAY_RATE         0.1f

/** Minimum activation to remain in active monitoring */
#define PROSP_MIN_ACTIVE_ACTIVATION         0.1f

/** Maximum activation level */
#define PROSP_MAX_ACTIVATION                10.0f

/** Activation boost from retrieval check */
#define PROSP_CHECK_ACTIVATION_BOOST        0.05f

/** Activation boost when approaching trigger time */
#define PROSP_APPROACH_ACTIVATION_BOOST     0.2f

/** Invalid intention ID sentinel */
#define PROSP_INVALID_ID                    UINT64_MAX

/** Epsilon for floating-point comparisons */
#define PROSP_EPSILON                       1e-6f

//=============================================================================
// Type Definitions - Enumerations
//=============================================================================

/**
 * @brief Prospective memory trigger types
 *
 * WHAT: Category of event that triggers intention execution
 * WHY:  Different triggers require different monitoring strategies
 * HOW:  Enum stored with intention, determines check method
 */
typedef enum {
    PROSP_TIME_BASED = 0,      /**< Triggered by reaching target time */
    PROSP_EVENT_BASED,         /**< Triggered by detecting cue/event */
    PROSP_ACTIVITY_BASED,      /**< Triggered by completing prerequisite */
    PROSP_LOCATION_BASED       /**< Triggered by location context */
} prospective_type_t;

/**
 * @brief Prospective memory intention status
 *
 * WHAT: Current state in intention lifecycle
 * WHY:  Track progress from creation to completion/failure
 * HOW:  Updated as intention moves through states
 */
typedef enum {
    PROSP_PENDING = 0,         /**< Waiting for trigger condition */
    PROSP_TRIGGERED,           /**< Trigger fired, awaiting execution */
    PROSP_EXECUTED,            /**< Successfully completed */
    PROSP_MISSED,              /**< Missed (time passed without execution) */
    PROSP_CANCELLED            /**< Explicitly cancelled by user */
} prospective_status_t;

/**
 * @brief Priority level for intentions
 *
 * WHAT: How important/urgent the intention is
 * WHY:  Affects monitoring frequency and interrupt behavior
 * HOW:  Combined from importance and urgency scores
 */
typedef enum {
    PROSP_PRIORITY_LOW = 0,    /**< Background, check infrequently */
    PROSP_PRIORITY_MEDIUM,     /**< Standard monitoring */
    PROSP_PRIORITY_HIGH,       /**< Frequent checks, can interrupt */
    PROSP_PRIORITY_CRITICAL    /**< Immediate attention required */
} prospective_priority_t;

/**
 * @brief Error codes for prospective memory operations
 */
typedef enum {
    PROSP_SUCCESS = 0,                  /**< Operation succeeded */
    PROSP_ERROR_NULL_POINTER = -1,      /**< NULL argument provided */
    PROSP_ERROR_INVALID_TYPE = -2,      /**< Invalid intention type */
    PROSP_ERROR_INVALID_STATUS = -3,    /**< Invalid status transition */
    PROSP_ERROR_NO_MEMORY = -4,         /**< Memory allocation failed */
    PROSP_ERROR_CAPACITY = -5,          /**< Maximum intentions reached */
    PROSP_ERROR_NOT_FOUND = -6,         /**< Intention ID not found */
    PROSP_ERROR_INVALID_TIME = -7,      /**< Invalid time specification */
    PROSP_ERROR_INVALID_TRIGGER = -8,   /**< Invalid trigger configuration */
    PROSP_ERROR_ALREADY_EXECUTED = -9,  /**< Intention already executed */
    PROSP_ERROR_INTERNAL = -10          /**< Internal error */
} prospective_error_t;

//=============================================================================
// Type Definitions - Structures
//=============================================================================

/**
 * @brief Time-based trigger configuration
 *
 * WHAT: Parameters for time-triggered intentions
 * WHY:  Specify when and how to trigger based on time
 * HOW:  Target time with alert/grace windows
 */
typedef struct {
    float target_time;           /**< Absolute target time (seconds since epoch or relative) */
    float window_before;         /**< Alert window before target (seconds) */
    float window_after;          /**< Grace period after target (seconds) */
    bool is_relative;            /**< True if target_time is relative to creation */
    bool repeat_daily;           /**< True if this repeats every day */
    float repeat_interval;       /**< Repeat interval in seconds (0 = no repeat) */
} prosp_time_trigger_t;

/**
 * @brief Event-based trigger configuration
 *
 * WHAT: Parameters for event/cue-triggered intentions
 * WHY:  Specify what cue to watch for
 * HOW:  Signature to match against incoming context
 */
typedef struct {
    prime_signature_t cue_signature;   /**< Signature of triggering cue/event */
    float similarity_threshold;        /**< Minimum similarity to trigger [0,1] */
    nimcp_quaternion_t cue_state;      /**< Optional state to match */
    bool match_state;                  /**< Whether to also match quaternion state */
    float state_weight;                /**< Weight for state matching [0,1] */
} prosp_event_trigger_t;

/**
 * @brief Activity-based trigger configuration
 *
 * WHAT: Parameters for activity-completion-triggered intentions
 * WHY:  Specify which activity must complete first
 * HOW:  Reference to prerequisite intention/activity
 */
typedef struct {
    uint64_t prerequisite_id;    /**< ID of activity that must complete */
    bool any_activity;           /**< True to trigger on any activity completion */
    char activity_tag[64];       /**< Optional tag to match activity type */
} prosp_activity_trigger_t;

/**
 * @brief Location-based trigger configuration
 *
 * WHAT: Parameters for location-triggered intentions
 * WHY:  Specify spatial context as trigger
 * HOW:  Location signature or coordinates
 */
typedef struct {
    prime_signature_t location_signature;  /**< Signature of location context */
    float similarity_threshold;            /**< Minimum similarity to trigger */
    float x, y, z;                         /**< Optional spatial coordinates */
    float radius;                          /**< Trigger radius around coordinates */
    bool use_coordinates;                  /**< True to use coordinates instead of signature */
} prosp_location_trigger_t;

/**
 * @brief Union of all trigger types
 *
 * WHAT: Container for trigger-type-specific data
 * WHY:  Space-efficient storage of varied trigger types
 * HOW:  Union selected by prospective_type_t
 */
typedef union {
    prosp_time_trigger_t time_trigger;
    prosp_event_trigger_t event_trigger;
    prosp_activity_trigger_t activity_trigger;
    prosp_location_trigger_t location_trigger;
} prosp_trigger_union_t;

/**
 * @brief Prospective intention structure
 *
 * WHAT: Complete representation of a future intention
 * WHY:  Store all information needed to monitor and execute intention
 * HOW:  Identity, content, trigger, state, and PR integration
 *
 * Memory layout: ~320 bytes (excluding action_description allocation)
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Identity
    //-------------------------------------------------------------------------
    uint64_t intention_id;           /**< Unique intention identifier */
    prospective_type_t type;         /**< Trigger type (time/event/activity/location) */
    prospective_status_t status;     /**< Current status in lifecycle */

    //-------------------------------------------------------------------------
    // Content: What to do
    //-------------------------------------------------------------------------
    char* action_description;        /**< Human-readable action description */
    prime_signature_t action_signature;  /**< Content signature of action */
    nimcp_quaternion_t action_quaternion; /**< Semantic state of action */

    //-------------------------------------------------------------------------
    // Trigger Conditions
    //-------------------------------------------------------------------------
    prosp_trigger_union_t trigger;   /**< Type-specific trigger configuration */

    //-------------------------------------------------------------------------
    // Importance and Urgency
    //-------------------------------------------------------------------------
    float importance;                /**< How important (1-10) */
    float urgency;                   /**< How time-sensitive (1-10) */
    prospective_priority_t priority; /**< Computed priority level */

    //-------------------------------------------------------------------------
    // Monitoring State
    //-------------------------------------------------------------------------
    float current_activation;        /**< Current activation level [0, MAX] */
    size_t retrieval_count;          /**< How many times checked/retrieved */
    float last_check_time;           /**< Time of last monitoring check */
    float trigger_time;              /**< When trigger actually fired */

    //-------------------------------------------------------------------------
    // Temporal Information
    //-------------------------------------------------------------------------
    float creation_time;             /**< When intention was created */
    float expected_execution_time;   /**< Estimated execution time */

    //-------------------------------------------------------------------------
    // PR Memory Integration
    //-------------------------------------------------------------------------
    pr_memory_node_t* memory_node;   /**< Underlying PR memory storage */
    uint64_t related_memory_ids[8];  /**< IDs of related memories (entangled) */
    size_t num_related;              /**< Number of related memories */

} prospective_intention_t;

/**
 * @brief Configuration for prospective memory system
 *
 * WHAT: Parameters controlling system behavior
 * WHY:  Allow tuning for different use cases
 * HOW:  Set at creation, some modifiable at runtime
 */
typedef struct {
    size_t max_intentions;           /**< Maximum intentions to store */
    size_t max_active_monitors;      /**< Maximum simultaneously monitored */
    float default_importance;        /**< Default importance for new intentions */
    float default_urgency;           /**< Default urgency for new intentions */
    float activation_decay_rate;     /**< Activation decay per second */
    float check_interval;            /**< Minimum time between checks (seconds) */
    float similarity_threshold;      /**< Default event similarity threshold */
    bool auto_prune_executed;        /**< Automatically remove executed intentions */
    bool auto_prune_missed;          /**< Automatically remove missed intentions */
    float prune_delay;               /**< Delay before pruning (seconds) */
} prospective_config_t;

/**
 * @brief Statistics for prospective memory system
 *
 * WHAT: Operational metrics and performance data
 * WHY:  Monitoring, debugging, analysis
 */
typedef struct {
    size_t total_created;            /**< Total intentions created */
    size_t total_executed;           /**< Successfully executed */
    size_t total_missed;             /**< Missed (not executed in time) */
    size_t total_cancelled;          /**< Explicitly cancelled */
    size_t current_pending;          /**< Currently pending */
    size_t current_triggered;        /**< Currently triggered (awaiting execution) */
    size_t current_active_monitors;  /**< Currently in active monitoring */
    float mean_response_time;        /**< Mean time from trigger to execution */
    float mean_activation;           /**< Mean activation of pending intentions */
    uint64_t total_checks;           /**< Total monitoring checks performed */
    uint64_t time_triggers_fired;    /**< Time-based triggers fired */
    uint64_t event_triggers_fired;   /**< Event-based triggers fired */
    uint64_t activity_triggers_fired; /**< Activity-based triggers fired */
} prospective_stats_t;

/**
 * @brief Result from trigger check operations
 *
 * WHAT: Information about triggered intentions
 * WHY:  Return triggered intentions for execution
 */
typedef struct {
    uint64_t intention_id;           /**< ID of triggered intention */
    prospective_type_t type;         /**< Type of trigger that fired */
    float trigger_strength;          /**< How strongly triggered (similarity, etc.) */
    float time_to_deadline;          /**< Time remaining (negative if past) */
    const char* action_description;  /**< What to do */
} prospective_trigger_result_t;

/**
 * @brief Prospective memory system manager (opaque)
 *
 * Internal structure containing:
 * - Intention storage array
 * - Active monitoring list
 * - PR memory integration handles
 * - Thread synchronization primitives
 */
typedef struct prospective_memory_internal* prospective_memory_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default prospective memory configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Starting point for most use cases
 *
 * @return Default configuration
 *
 * Defaults:
 * - max_intentions: 1024
 * - max_active_monitors: 128
 * - default_importance: 5.0
 * - default_urgency: 5.0
 * - activation_decay_rate: 0.1/sec
 * - check_interval: 1.0 sec
 * - similarity_threshold: 0.7
 * - auto_prune_executed: true
 * - prune_delay: 300.0 sec (5 min)
 */
NIMCP_EXPORT prospective_config_t prospective_config_default(void);

/**
 * @brief Validate prospective configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
NIMCP_EXPORT bool prospective_config_validate(const prospective_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create prospective memory system
 *
 * WHAT: Allocates and initializes prospective memory manager
 * WHY:  Entry point for prospective memory functionality
 * HOW:  Creates storage, initializes monitoring, links to PR system
 *
 * @param entanglement Entanglement graph for memory associations (can be NULL)
 * @param node_manager PR memory node manager (can be NULL)
 * @param theta_gamma Theta-gamma manager for timing (can be NULL)
 * @param config Configuration (NULL for defaults)
 * @return Prospective memory handle or NULL on failure
 *
 * Memory: ~100KB for default configuration
 * Thread safety: Returned handle is thread-safe
 *
 * EXAMPLE:
 * ```c
 * prospective_memory_t pm = prospective_create(
 *     entangle_graph, node_mgr, theta_gamma, NULL);
 * if (!pm) {
 *     fprintf(stderr, "Failed: %s\n", prospective_get_last_error());
 * }
 * ```
 */
NIMCP_EXPORT prospective_memory_t prospective_create(
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    theta_gamma_manager_t theta_gamma,
    const prospective_config_t* config
);

/**
 * @brief Destroy prospective memory system
 *
 * WHAT: Releases all resources
 * WHY:  Clean shutdown
 * HOW:  Frees intentions, monitoring state, handles
 *
 * @param pm Prospective memory to destroy (NULL safe)
 */
NIMCP_EXPORT void prospective_destroy(prospective_memory_t pm);

/**
 * @brief Reset prospective memory to initial state
 *
 * WHAT: Clears all intentions, resets statistics
 * WHY:  Fresh start without reallocation
 *
 * @param pm Prospective memory
 * @return PROSP_SUCCESS or error code
 */
NIMCP_EXPORT prospective_error_t prospective_reset(prospective_memory_t pm);

//=============================================================================
// Intention Creation Functions
//=============================================================================

/**
 * @brief Create a time-based prospective intention
 *
 * WHAT: "At time X, do Y"
 * WHY:  Schedule future actions based on time
 * HOW:  Creates intention with time trigger, starts monitoring
 *
 * @param pm Prospective memory
 * @param target_time Target time (seconds since epoch or relative)
 * @param is_relative True if target_time is relative to now
 * @param action Action description (copied)
 * @param importance Importance score 1-10 (0 for default)
 * @param urgency Urgency score 1-10 (0 for default)
 * @param intention_id_out Output: created intention ID
 * @return PROSP_SUCCESS or error code
 *
 * EXAMPLE:
 * ```c
 * uint64_t id;
 * prospective_error_t err = prospective_create_time_intention(
 *     pm, 3600.0f, true,  // In 1 hour (relative)
 *     "Call the doctor",
 *     8.0f, 7.0f,  // Important and urgent
 *     &id);
 * ```
 */
NIMCP_EXPORT prospective_error_t prospective_create_time_intention(
    prospective_memory_t pm,
    float target_time,
    bool is_relative,
    const char* action,
    float importance,
    float urgency,
    uint64_t* intention_id_out
);

/**
 * @brief Create time intention with detailed trigger configuration
 *
 * @param pm Prospective memory
 * @param trigger Detailed time trigger configuration
 * @param action Action description
 * @param importance Importance score
 * @param urgency Urgency score
 * @param intention_id_out Output: created intention ID
 * @return PROSP_SUCCESS or error code
 */
NIMCP_EXPORT prospective_error_t prospective_create_time_intention_ex(
    prospective_memory_t pm,
    const prosp_time_trigger_t* trigger,
    const char* action,
    float importance,
    float urgency,
    uint64_t* intention_id_out
);

/**
 * @brief Create an event-based prospective intention
 *
 * WHAT: "When I see X, do Y"
 * WHY:  Schedule actions triggered by environmental cues
 * HOW:  Creates intention with event signature trigger
 *
 * @param pm Prospective memory
 * @param cue_signature Signature of triggering cue (can be NULL to set later)
 * @param similarity_threshold Minimum similarity to trigger (0 for default)
 * @param action Action description
 * @param importance Importance score
 * @param urgency Urgency score
 * @param intention_id_out Output: created intention ID
 * @return PROSP_SUCCESS or error code
 *
 * EXAMPLE:
 * ```c
 * // Create signature for "seeing John"
 * prime_signature_t* john_sig = prime_sig_from_text("John Smith face recognition");
 *
 * uint64_t id;
 * prospective_create_event_intention(
 *     pm, john_sig, 0.8f,
 *     "Give John the book",
 *     6.0f, 5.0f, &id);
 * ```
 */
NIMCP_EXPORT prospective_error_t prospective_create_event_intention(
    prospective_memory_t pm,
    const prime_signature_t* cue_signature,
    float similarity_threshold,
    const char* action,
    float importance,
    float urgency,
    uint64_t* intention_id_out
);

/**
 * @brief Create event intention with detailed trigger configuration
 *
 * @param pm Prospective memory
 * @param trigger Detailed event trigger configuration
 * @param action Action description
 * @param importance Importance score
 * @param urgency Urgency score
 * @param intention_id_out Output: created intention ID
 * @return PROSP_SUCCESS or error code
 */
NIMCP_EXPORT prospective_error_t prospective_create_event_intention_ex(
    prospective_memory_t pm,
    const prosp_event_trigger_t* trigger,
    const char* action,
    float importance,
    float urgency,
    uint64_t* intention_id_out
);

/**
 * @brief Create an activity-based prospective intention
 *
 * WHAT: "After finishing X, do Y"
 * WHY:  Chain actions to task completion
 * HOW:  Creates intention triggered by activity completion
 *
 * @param pm Prospective memory
 * @param prerequisite_id ID of activity/intention that must complete first
 * @param action Action description
 * @param importance Importance score
 * @param urgency Urgency score
 * @param intention_id_out Output: created intention ID
 * @return PROSP_SUCCESS or error code
 *
 * EXAMPLE:
 * ```c
 * uint64_t id;
 * prospective_create_activity_intention(
 *     pm, task_id,  // After this task completes
 *     "Run the test suite",
 *     5.0f, 5.0f, &id);
 * ```
 */
NIMCP_EXPORT prospective_error_t prospective_create_activity_intention(
    prospective_memory_t pm,
    uint64_t prerequisite_id,
    const char* action,
    float importance,
    float urgency,
    uint64_t* intention_id_out
);

/**
 * @brief Create activity intention with detailed configuration
 *
 * @param pm Prospective memory
 * @param trigger Activity trigger configuration
 * @param action Action description
 * @param importance Importance score
 * @param urgency Urgency score
 * @param intention_id_out Output: created intention ID
 * @return PROSP_SUCCESS or error code
 */
NIMCP_EXPORT prospective_error_t prospective_create_activity_intention_ex(
    prospective_memory_t pm,
    const prosp_activity_trigger_t* trigger,
    const char* action,
    float importance,
    float urgency,
    uint64_t* intention_id_out
);

/**
 * @brief Create a location-based prospective intention
 *
 * WHAT: "When at location X, do Y"
 * WHY:  Trigger actions based on spatial context
 *
 * @param pm Prospective memory
 * @param location_signature Signature of location context
 * @param similarity_threshold Minimum similarity to trigger
 * @param action Action description
 * @param importance Importance score
 * @param urgency Urgency score
 * @param intention_id_out Output: created intention ID
 * @return PROSP_SUCCESS or error code
 */
NIMCP_EXPORT prospective_error_t prospective_create_location_intention(
    prospective_memory_t pm,
    const prime_signature_t* location_signature,
    float similarity_threshold,
    const char* action,
    float importance,
    float urgency,
    uint64_t* intention_id_out
);

//=============================================================================
// Update and Monitoring Functions
//=============================================================================

/**
 * @brief Main update function - check all triggers
 *
 * WHAT: Updates time, checks all pending intentions for triggers
 * WHY:  Core monitoring loop
 * HOW:  Advances time, checks each intention type, returns triggered
 *
 * @param pm Prospective memory
 * @param current_time Current simulation/wall time
 * @param triggered_out Output array for triggered intentions
 * @param max_triggered Maximum results to return
 * @param num_triggered_out Output: number of triggered intentions
 * @return PROSP_SUCCESS or error code
 *
 * Call this periodically (e.g., every frame or theta cycle).
 *
 * EXAMPLE:
 * ```c
 * prospective_trigger_result_t triggered[16];
 * size_t num_triggered;
 * prospective_update(pm, get_current_time(), triggered, 16, &num_triggered);
 * for (size_t i = 0; i < num_triggered; i++) {
 *     printf("Triggered: %s\n", triggered[i].action_description);
 * }
 * ```
 */
NIMCP_EXPORT prospective_error_t prospective_update(
    prospective_memory_t pm,
    float current_time,
    prospective_trigger_result_t* triggered_out,
    size_t max_triggered,
    size_t* num_triggered_out
);

/**
 * @brief Check only time-based triggers
 *
 * WHAT: Checks time triggers against current time
 * WHY:  Separate time checking for efficiency
 *
 * @param pm Prospective memory
 * @param current_time Current time
 * @param triggered_out Output array
 * @param max_triggered Maximum results
 * @param num_triggered_out Output: number triggered
 * @return PROSP_SUCCESS or error code
 */
NIMCP_EXPORT prospective_error_t prospective_check_time_triggers(
    prospective_memory_t pm,
    float current_time,
    prospective_trigger_result_t* triggered_out,
    size_t max_triggered,
    size_t* num_triggered_out
);

/**
 * @brief Check event-based triggers against current context
 *
 * WHAT: Matches context signature against event triggers
 * WHY:  Detect environmental cues
 *
 * @param pm Prospective memory
 * @param context_signature Current environmental context signature
 * @param triggered_out Output array
 * @param max_triggered Maximum results
 * @param num_triggered_out Output: number triggered
 * @return PROSP_SUCCESS or error code
 */
NIMCP_EXPORT prospective_error_t prospective_check_event_triggers(
    prospective_memory_t pm,
    const prime_signature_t* context_signature,
    prospective_trigger_result_t* triggered_out,
    size_t max_triggered,
    size_t* num_triggered_out
);

/**
 * @brief Check activity triggers after activity completion
 *
 * WHAT: Fires triggers waiting on completed activity
 * WHY:  Chain actions to task completion
 *
 * @param pm Prospective memory
 * @param completed_activity_id ID of completed activity
 * @param triggered_out Output array
 * @param max_triggered Maximum results
 * @param num_triggered_out Output: number triggered
 * @return PROSP_SUCCESS or error code
 */
NIMCP_EXPORT prospective_error_t prospective_check_activity_triggers(
    prospective_memory_t pm,
    uint64_t completed_activity_id,
    prospective_trigger_result_t* triggered_out,
    size_t max_triggered,
    size_t* num_triggered_out
);

/**
 * @brief Signal current context for event matching
 *
 * WHAT: Provides current sensory/cognitive context
 * WHY:  Enable continuous event-based monitoring
 * HOW:  Updates internal context, triggers matching check
 *
 * @param pm Prospective memory
 * @param context Current context signature
 * @param context_state Optional quaternion state of context
 * @return Number of intentions triggered, or -1 on error
 *
 * EXAMPLE:
 * ```c
 * // Visual system detected a face
 * prime_signature_t* face_sig = compute_face_signature(image);
 * int triggered = prospective_signal_context(pm, face_sig, NULL);
 * ```
 */
NIMCP_EXPORT int prospective_signal_context(
    prospective_memory_t pm,
    const prime_signature_t* context,
    const nimcp_quaternion_t* context_state
);

/**
 * @brief Signal activity completion
 *
 * WHAT: Notifies system that an activity completed
 * WHY:  Fire activity-based triggers
 *
 * @param pm Prospective memory
 * @param activity_id ID of completed activity
 * @param activity_tag Optional tag describing activity type
 * @return Number of intentions triggered, or -1 on error
 *
 * EXAMPLE:
 * ```c
 * // Task finished
 * int triggered = prospective_signal_activity_complete(pm, task_id, "build");
 * ```
 */
NIMCP_EXPORT int prospective_signal_activity_complete(
    prospective_memory_t pm,
    uint64_t activity_id,
    const char* activity_tag
);

/**
 * @brief Signal location change
 *
 * WHAT: Updates current location context
 * WHY:  Fire location-based triggers
 *
 * @param pm Prospective memory
 * @param location_signature Location signature
 * @return Number of intentions triggered, or -1 on error
 */
NIMCP_EXPORT int prospective_signal_location(
    prospective_memory_t pm,
    const prime_signature_t* location_signature
);

//=============================================================================
// Intention Management Functions
//=============================================================================

/**
 * @brief Mark intention as executed
 *
 * WHAT: Transitions intention to EXECUTED status
 * WHY:  Record successful completion
 *
 * @param pm Prospective memory
 * @param intention_id Intention to mark executed
 * @return PROSP_SUCCESS or error code
 *
 * EXAMPLE:
 * ```c
 * // User confirmed they called the doctor
 * prospective_execute_intention(pm, intention_id);
 * ```
 */
NIMCP_EXPORT prospective_error_t prospective_execute_intention(
    prospective_memory_t pm,
    uint64_t intention_id
);

/**
 * @brief Cancel an intention
 *
 * WHAT: Transitions intention to CANCELLED status
 * WHY:  User no longer wants to perform action
 *
 * @param pm Prospective memory
 * @param intention_id Intention to cancel
 * @return PROSP_SUCCESS or error code
 */
NIMCP_EXPORT prospective_error_t prospective_cancel_intention(
    prospective_memory_t pm,
    uint64_t intention_id
);

/**
 * @brief Mark intention as missed
 *
 * WHAT: Transitions to MISSED status (time-based that weren't executed)
 * WHY:  Track failures for analysis
 *
 * @param pm Prospective memory
 * @param intention_id Intention to mark missed
 * @return PROSP_SUCCESS or error code
 */
NIMCP_EXPORT prospective_error_t prospective_miss_intention(
    prospective_memory_t pm,
    uint64_t intention_id
);

/**
 * @brief Reschedule a time-based intention
 *
 * WHAT: Updates target time for a time-based intention
 * WHY:  Postpone or adjust scheduled actions
 *
 * @param pm Prospective memory
 * @param intention_id Intention to reschedule
 * @param new_target_time New target time
 * @param is_relative True if new time is relative to now
 * @return PROSP_SUCCESS or error code
 */
NIMCP_EXPORT prospective_error_t prospective_reschedule(
    prospective_memory_t pm,
    uint64_t intention_id,
    float new_target_time,
    bool is_relative
);

/**
 * @brief Remove intention from system
 *
 * WHAT: Deletes intention entirely
 * WHY:  Clean up old intentions
 *
 * @param pm Prospective memory
 * @param intention_id Intention to remove
 * @return PROSP_SUCCESS or error code
 */
NIMCP_EXPORT prospective_error_t prospective_remove_intention(
    prospective_memory_t pm,
    uint64_t intention_id
);

/**
 * @brief Prune old executed/missed/cancelled intentions
 *
 * WHAT: Removes intentions in terminal states
 * WHY:  Free memory, keep system clean
 *
 * @param pm Prospective memory
 * @param min_age Minimum age in seconds (0 to prune all terminal)
 * @return Number of intentions removed
 */
NIMCP_EXPORT size_t prospective_prune(prospective_memory_t pm, float min_age);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get intention by ID
 *
 * WHAT: Retrieves intention details
 * WHY:  Inspect intention state
 *
 * @param pm Prospective memory
 * @param intention_id ID to look up
 * @param intention_out Output: intention copy (caller-allocated)
 * @return PROSP_SUCCESS or error code
 *
 * Note: action_description in output is pointer to internal storage
 */
NIMCP_EXPORT prospective_error_t prospective_get_intention(
    prospective_memory_t pm,
    uint64_t intention_id,
    prospective_intention_t* intention_out
);

/**
 * @brief Get all pending intentions
 *
 * WHAT: Returns intentions in PENDING status
 * WHY:  Overview of future actions
 *
 * @param pm Prospective memory
 * @param intentions_out Output array
 * @param max_intentions Maximum to return
 * @param count_out Output: actual count
 * @return PROSP_SUCCESS or error code
 */
NIMCP_EXPORT prospective_error_t prospective_get_pending(
    prospective_memory_t pm,
    prospective_intention_t* intentions_out,
    size_t max_intentions,
    size_t* count_out
);

/**
 * @brief Get triggered intentions awaiting execution
 *
 * WHAT: Returns intentions in TRIGGERED status
 * WHY:  List of actions ready to perform
 *
 * @param pm Prospective memory
 * @param intentions_out Output array
 * @param max_intentions Maximum to return
 * @param count_out Output: actual count
 * @return PROSP_SUCCESS or error code
 */
NIMCP_EXPORT prospective_error_t prospective_get_triggered(
    prospective_memory_t pm,
    prospective_intention_t* intentions_out,
    size_t max_intentions,
    size_t* count_out
);

/**
 * @brief Get most urgent pending intentions
 *
 * WHAT: Returns top-K urgent intentions
 * WHY:  Prioritize attention to important deadlines
 *
 * @param pm Prospective memory
 * @param k Number of intentions to return
 * @param intentions_out Output array (size >= k)
 * @param count_out Output: actual count (<= k)
 * @return PROSP_SUCCESS or error code
 */
NIMCP_EXPORT prospective_error_t prospective_get_urgent(
    prospective_memory_t pm,
    size_t k,
    prospective_intention_t* intentions_out,
    size_t* count_out
);

/**
 * @brief Get intentions by type
 *
 * WHAT: Filter intentions by trigger type
 * WHY:  Analyze specific categories
 *
 * @param pm Prospective memory
 * @param type Trigger type to filter by
 * @param intentions_out Output array
 * @param max_intentions Maximum to return
 * @param count_out Output: actual count
 * @return PROSP_SUCCESS or error code
 */
NIMCP_EXPORT prospective_error_t prospective_get_by_type(
    prospective_memory_t pm,
    prospective_type_t type,
    prospective_intention_t* intentions_out,
    size_t max_intentions,
    size_t* count_out
);

/**
 * @brief Search intentions by action description
 *
 * WHAT: Find intentions matching text query
 * WHY:  Search for specific future actions
 *
 * @param pm Prospective memory
 * @param query Text to search for (substring match)
 * @param intentions_out Output array
 * @param max_intentions Maximum to return
 * @param count_out Output: actual count
 * @return PROSP_SUCCESS or error code
 */
NIMCP_EXPORT prospective_error_t prospective_search(
    prospective_memory_t pm,
    const char* query,
    prospective_intention_t* intentions_out,
    size_t max_intentions,
    size_t* count_out
);

//=============================================================================
// Activation Functions
//=============================================================================

/**
 * @brief Compute activation level for an intention
 *
 * WHAT: Calculate how "active" an intention is in memory
 * WHY:  Activation affects monitoring frequency and accessibility
 * HOW:  Based on importance, urgency, time to trigger, retrieval count
 *
 * @param pm Prospective memory
 * @param intention_id Intention to check
 * @param current_time Current time for time-based calculations
 * @return Activation level [0, MAX] or -1 on error
 *
 * Factors increasing activation:
 * - Higher importance/urgency
 * - Approaching trigger time (time-based)
 * - Recent retrieval/checks
 * - High similarity to current context (event-based)
 */
NIMCP_EXPORT float prospective_compute_activation(
    prospective_memory_t pm,
    uint64_t intention_id,
    float current_time
);

/**
 * @brief Boost intention activation (rehearsal)
 *
 * WHAT: Increase activation through conscious rehearsal
 * WHY:  "Thinking about" an intention strengthens it
 *
 * @param pm Prospective memory
 * @param intention_id Intention to boost
 * @param boost_amount Amount to add to activation
 * @return New activation level or -1 on error
 */
NIMCP_EXPORT float prospective_boost_activation(
    prospective_memory_t pm,
    uint64_t intention_id,
    float boost_amount
);

/**
 * @brief Apply activation decay to all intentions
 *
 * WHAT: Reduce activation over time
 * WHY:  Model forgetting of intentions
 *
 * @param pm Prospective memory
 * @param elapsed_time Time since last decay application
 * @return PROSP_SUCCESS or error code
 */
NIMCP_EXPORT prospective_error_t prospective_apply_decay(
    prospective_memory_t pm,
    float elapsed_time
);

//=============================================================================
// Integration Functions
//=============================================================================

/**
 * @brief Link intention to related memory
 *
 * WHAT: Create entanglement between intention and memory
 * WHY:  Related memories can cue intention retrieval
 *
 * @param pm Prospective memory
 * @param intention_id Intention to link
 * @param memory_id Related memory ID
 * @return PROSP_SUCCESS or error code
 */
NIMCP_EXPORT prospective_error_t prospective_link_memory(
    prospective_memory_t pm,
    uint64_t intention_id,
    uint64_t memory_id
);

/**
 * @brief Get the underlying PR memory node for an intention
 *
 * WHAT: Access intention's memory node directly
 * WHY:  Advanced integration with PR system
 *
 * @param pm Prospective memory
 * @param intention_id Intention to query
 * @return PR memory node or NULL if not found
 */
NIMCP_EXPORT pr_memory_node_t* prospective_get_memory_node(
    prospective_memory_t pm,
    uint64_t intention_id
);

/**
 * @brief Synchronize with theta-gamma for optimal monitoring
 *
 * WHAT: Align monitoring checks with theta encoding phase
 * WHY:  Biologically realistic periodic checking
 *
 * @param pm Prospective memory
 * @return true if a check should be performed now
 */
NIMCP_EXPORT bool prospective_should_check_now(prospective_memory_t pm);

//=============================================================================
// Statistics and Debugging
//=============================================================================

/**
 * @brief Get prospective memory statistics
 *
 * @param pm Prospective memory
 * @param stats_out Output statistics structure
 * @return PROSP_SUCCESS or error code
 */
NIMCP_EXPORT prospective_error_t prospective_get_stats(
    prospective_memory_t pm,
    prospective_stats_t* stats_out
);

/**
 * @brief Reset statistics counters
 *
 * @param pm Prospective memory
 * @return PROSP_SUCCESS or error code
 */
NIMCP_EXPORT prospective_error_t prospective_reset_stats(prospective_memory_t pm);

/**
 * @brief Get error string for error code
 *
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* prospective_error_string(prospective_error_t error);

/**
 * @brief Get last error message
 *
 * @return Error string from last failed operation
 */
NIMCP_EXPORT const char* prospective_get_last_error(void);

/**
 * @brief Get intention type name
 *
 * @param type Intention type
 * @return Human-readable type name
 */
NIMCP_EXPORT const char* prospective_type_name(prospective_type_t type);

/**
 * @brief Get intention status name
 *
 * @param status Intention status
 * @return Human-readable status name
 */
NIMCP_EXPORT const char* prospective_status_name(prospective_status_t status);

/**
 * @brief Print intention details (debug)
 *
 * @param intention Intention to print
 */
NIMCP_EXPORT void prospective_print_intention(const prospective_intention_t* intention);

/**
 * @brief Print system summary (debug)
 *
 * @param pm Prospective memory
 */
NIMCP_EXPORT void prospective_print_summary(prospective_memory_t pm);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get current time (utility)
 *
 * @return Current time in seconds (since epoch)
 */
NIMCP_EXPORT float prospective_current_time(void);

/**
 * @brief Compute priority from importance and urgency
 *
 * @param importance Importance score (1-10)
 * @param urgency Urgency score (1-10)
 * @return Priority level
 */
NIMCP_EXPORT prospective_priority_t prospective_compute_priority(
    float importance,
    float urgency
);

/**
 * @brief Initialize default time trigger
 *
 * @param trigger Trigger to initialize
 * @param target_time Target time
 * @param is_relative Whether target is relative
 */
NIMCP_EXPORT void prospective_init_time_trigger(
    prosp_time_trigger_t* trigger,
    float target_time,
    bool is_relative
);

/**
 * @brief Initialize default event trigger
 *
 * @param trigger Trigger to initialize
 * @param cue_signature Cue signature (copied)
 * @param threshold Similarity threshold
 */
NIMCP_EXPORT void prospective_init_event_trigger(
    prosp_event_trigger_t* trigger,
    const prime_signature_t* cue_signature,
    float threshold
);

/**
 * @brief Initialize default activity trigger
 *
 * @param trigger Trigger to initialize
 * @param prerequisite_id Prerequisite activity ID
 */
NIMCP_EXPORT void prospective_init_activity_trigger(
    prosp_activity_trigger_t* trigger,
    uint64_t prerequisite_id
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PROSPECTIVE_H
