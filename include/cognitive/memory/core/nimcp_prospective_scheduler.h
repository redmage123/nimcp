//=============================================================================
// nimcp_prospective_scheduler.h - Prime Resonant Prospective Memory Scheduler
//=============================================================================
/**
 * @file nimcp_prospective_scheduler.h
 * @brief Scheduler for prospective memory intentions with priority management
 *
 * WHAT: Manages scheduling, prioritization, and execution of prospective memories
 * WHY:  Prospective memory requires remembering to do things at the right time
 * HOW:  Priority queue with urgency/importance scoring, conflict resolution,
 *       and graduated reminder system
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Prospective Memory Overview:
 *   +-----------------------------------------------------------------------+
 *   |  Prospective memory = "remembering to remember"                       |
 *   |                                                                       |
 *   |  EVENT-BASED: Triggered by external cues                              |
 *   |  - "When I see John, give him the book"                               |
 *   |  - Requires monitoring environment for trigger                        |
 *   |                                                                       |
 *   |  TIME-BASED: Triggered by time                                        |
 *   |  - "Take medicine at 3pm"                                             |
 *   |  - Requires internal time monitoring                                  |
 *   |                                                                       |
 *   |  ACTIVITY-BASED: Triggered by completing an activity                  |
 *   |  - "After lunch, call the dentist"                                    |
 *   |  - Requires activity completion detection                             |
 *   +-----------------------------------------------------------------------+
 *
 *   Neural Basis of Prospective Memory:
 *   +-----------------------------------------------------------------------+
 *   |  ROSTRAL PREFRONTAL CORTEX (BA10):                                    |
 *   |  - Maintains intentions during ongoing tasks                          |
 *   |  - "Gateway" for switching attention to intentions                    |
 *   |                                                                       |
 *   |  ANTERIOR CINGULATE CORTEX (ACC):                                     |
 *   |  - Conflict monitoring between current task and intention             |
 *   |  - Urgency detection as deadline approaches                           |
 *   |                                                                       |
 *   |  HIPPOCAMPUS:                                                         |
 *   |  - Encodes intention-context associations                             |
 *   |  - Pattern completion triggers intention retrieval                    |
 *   |                                                                       |
 *   |  PARIETAL CORTEX:                                                     |
 *   |  - Time estimation for time-based intentions                          |
 *   |  - Deadline monitoring                                                |
 *   +-----------------------------------------------------------------------+
 *
 *   Priority Computation Model:
 *   +-----------------------------------------------------------------------+
 *   |  Priority = Urgency x Importance x Decay_Factor x Recency_Boost       |
 *   |                                                                       |
 *   |  URGENCY: f(time_until_trigger)                                       |
 *   |  - Exponential increase as deadline approaches                        |
 *   |  - urgency = 1 - exp(-lambda * (deadline - now))                      |
 *   |                                                                       |
 *   |  IMPORTANCE: User-assigned [0, 1]                                     |
 *   |  - Critical tasks: 0.9-1.0                                            |
 *   |  - Normal tasks: 0.5-0.7                                              |
 *   |  - Low priority: 0.1-0.3                                              |
 *   |                                                                       |
 *   |  DECAY_FACTOR: Freshness of intention                                 |
 *   |  - Recently added intentions get boost                                |
 *   |  - decay = exp(-age / tau)                                            |
 *   |                                                                       |
 *   |  CONFLICT_PENALTY: If conflicts exist                                 |
 *   |  - Reduces priority when conflicting intentions present               |
 *   +-----------------------------------------------------------------------+
 *
 *   Reminder System (Graduated Alerts):
 *   +-----------------------------------------------------------------------+
 *   |  Level          | Default Timing    | Description                     |
 *   |-----------------|-------------------|---------------------------------|
 *   |  FAR            | 1 day before      | "Tomorrow: doctor appointment"  |
 *   |  MODERATE       | 1 hour before     | "In an hour: meeting"           |
 *   |  SOON           | 10 min before     | "Soon: call starting"           |
 *   |  IMMINENT       | 1 min before      | "NOW: take medicine!"           |
 *   |  OVERDUE        | Past deadline     | "MISSED: was due 5 min ago"     |
 *   +-----------------------------------------------------------------------+
 *
 *   Conflict Resolution Strategies:
 *   +-----------------------------------------------------------------------+
 *   |  HIGHEST_PRIORITY: Execute only highest priority intention            |
 *   |  SEQUENTIAL: Queue conflicting intentions in priority order           |
 *   |  BATCH: Group compatible intentions for batch execution               |
 *   |  USER_CHOICE: Fire callback to let user decide                        |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Add intention: O(log n) heap insertion
 * - Get next: O(1) heap peek
 * - Remove: O(log n) heap delete
 * - Update all: O(n log n) re-heapify
 * - Check reminders: O(n)
 *
 * MEMORY:
 * - Scheduler: ~256 bytes base
 * - Per intention: ~128 bytes scheduled_intention_t
 * - Heap: capacity * sizeof(priority_heap_node_t) = capacity * 16 bytes
 *
 * THREAD SAFETY:
 * - All public functions are thread-safe via internal mutex
 * - Callback invocation holds no locks (callback can call scheduler)
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_PR_PROSPECTIVE_SCHEDULER_H
#define NIMCP_PR_PROSPECTIVE_SCHEDULER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_prime_signature.h"

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

/** Default maximum number of scheduled intentions */
#define PR_SCHED_DEFAULT_MAX_INTENTIONS     256

/** Default reminder timing: FAR (1 day = 86400 seconds) */
#define PR_SCHED_DEFAULT_REMINDER_FAR       86400.0f

/** Default reminder timing: MODERATE (1 hour = 3600 seconds) */
#define PR_SCHED_DEFAULT_REMINDER_MODERATE  3600.0f

/** Default reminder timing: SOON (10 minutes = 600 seconds) */
#define PR_SCHED_DEFAULT_REMINDER_SOON      600.0f

/** Default reminder timing: IMMINENT (1 minute = 60 seconds) */
#define PR_SCHED_DEFAULT_REMINDER_IMMINENT  60.0f

/** Default urgency weight in priority computation */
#define PR_SCHED_DEFAULT_URGENCY_WEIGHT     0.4f

/** Default importance weight in priority computation */
#define PR_SCHED_DEFAULT_IMPORTANCE_WEIGHT  0.4f

/** Default recency weight in priority computation */
#define PR_SCHED_DEFAULT_RECENCY_WEIGHT     0.2f

/** Urgency time constant (seconds) - how fast urgency grows */
#define PR_SCHED_URGENCY_TAU                3600.0f

/** Recency decay time constant (seconds) */
#define PR_SCHED_RECENCY_TAU                86400.0f

/** Maximum number of conflict groups */
#define PR_SCHED_MAX_CONFLICT_GROUPS        64

/** Maximum intentions per conflict group */
#define PR_SCHED_MAX_CONFLICTS_PER_GROUP    16

/** Epsilon for floating-point comparisons */
#define PR_SCHED_EPSILON                    1e-6f

/** Invalid intention ID sentinel */
#define PR_SCHED_INVALID_ID                 UINT64_MAX

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Intention trigger type
 */
typedef enum {
    PR_INTENT_TRIGGER_TIME = 0,      /**< Time-based trigger */
    PR_INTENT_TRIGGER_EVENT,          /**< Event-based trigger */
    PR_INTENT_TRIGGER_ACTIVITY,       /**< Activity completion trigger */
    PR_INTENT_TRIGGER_LOCATION,       /**< Location-based trigger */
    PR_INTENT_TRIGGER_CONTEXT         /**< Context-based trigger */
} pr_intent_trigger_type_t;

/**
 * @brief Intention state
 */
typedef enum {
    PR_INTENT_STATE_PENDING = 0,     /**< Not yet triggered */
    PR_INTENT_STATE_ACTIVE,          /**< Currently active/executing */
    PR_INTENT_STATE_COMPLETED,       /**< Successfully completed */
    PR_INTENT_STATE_FAILED,          /**< Execution failed */
    PR_INTENT_STATE_CANCELLED,       /**< User cancelled */
    PR_INTENT_STATE_EXPIRED          /**< Past deadline without execution */
} pr_intent_state_t;

/**
 * @brief Reminder urgency level
 *
 * Graduated reminder system with increasing urgency as deadline approaches.
 */
typedef enum {
    REMINDER_NONE = 0,               /**< No reminder needed yet */
    REMINDER_FAR,                    /**< Way ahead (e.g., 1 day before) */
    REMINDER_MODERATE,               /**< Getting closer (e.g., 1 hour before) */
    REMINDER_SOON,                   /**< Almost time (e.g., 10 min before) */
    REMINDER_IMMINENT,               /**< Right now (e.g., 1 min before) */
    REMINDER_OVERDUE                 /**< Past deadline */
} reminder_level_t;

/**
 * @brief Conflict resolution strategy
 */
typedef enum {
    PR_CONFLICT_HIGHEST_PRIORITY = 0, /**< Execute highest priority only */
    PR_CONFLICT_SEQUENTIAL,           /**< Queue in priority order */
    PR_CONFLICT_BATCH,                /**< Group compatible intentions */
    PR_CONFLICT_USER_CHOICE           /**< Fire callback for user decision */
} pr_conflict_strategy_t;

/**
 * @brief Error codes for scheduler operations
 */
typedef enum {
    PR_SCHED_SUCCESS = 0,                   /**< Operation succeeded */
    PR_SCHED_ERROR_NULL_POINTER = -1,       /**< NULL pointer argument */
    PR_SCHED_ERROR_INVALID_CONFIG = -2,     /**< Invalid configuration */
    PR_SCHED_ERROR_NO_MEMORY = -3,          /**< Memory allocation failed */
    PR_SCHED_ERROR_CAPACITY_EXCEEDED = -4,  /**< Maximum capacity reached */
    PR_SCHED_ERROR_NOT_FOUND = -5,          /**< Intention not found */
    PR_SCHED_ERROR_INVALID_STATE = -6,      /**< Invalid intention state */
    PR_SCHED_ERROR_CONFLICT = -7,           /**< Conflict detected */
    PR_SCHED_ERROR_ALREADY_EXISTS = -8,     /**< Intention already scheduled */
    PR_SCHED_ERROR_INVALID_TIME = -9,       /**< Invalid time specification */
    PR_SCHED_ERROR_EMPTY = -10,             /**< Scheduler is empty */
    PR_SCHED_ERROR_CALLBACK_FAILED = -11    /**< Callback execution failed */
} pr_sched_error_t;

//=============================================================================
// Core Structures
//=============================================================================

/**
 * @brief Time window for intention execution
 */
typedef struct {
    float start_time;                /**< Window start (seconds, relative/absolute) */
    float end_time;                  /**< Window end (deadline) */
    float optimal_time;              /**< Preferred execution time within window */
    bool is_absolute;                /**< true=absolute timestamp, false=relative */
} pr_time_window_t;

/**
 * @brief Event trigger specification
 */
typedef struct {
    char event_type[64];             /**< Event type identifier */
    prime_signature_t* cue_signature; /**< Signature of triggering cue (optional) */
    float similarity_threshold;      /**< Required similarity for cue match */
    void* event_data;                /**< Additional event data */
    size_t event_data_size;          /**< Size of event data */
} pr_event_trigger_t;

/**
 * @brief Activity trigger specification
 */
typedef struct {
    char activity_name[64];          /**< Name of preceding activity */
    uint64_t activity_id;            /**< ID of activity to wait for */
    bool require_success;            /**< Only trigger if activity succeeds */
} pr_activity_trigger_t;

/**
 * @brief Prospective memory intention
 *
 * Core data structure representing a future intention to be executed.
 */
typedef struct {
    /* Identity */
    uint64_t intention_id;           /**< Unique intention identifier */
    char name[128];                  /**< Human-readable name */
    char description[256];           /**< Detailed description */

    /* Trigger specification */
    pr_intent_trigger_type_t trigger_type; /**< How this intention triggers */
    pr_time_window_t time_window;    /**< When this can execute */
    pr_event_trigger_t event_trigger; /**< Event-based trigger (if applicable) */
    pr_activity_trigger_t activity_trigger; /**< Activity trigger (if applicable) */

    /* Priority factors */
    float importance;                /**< User-assigned importance [0, 1] */
    float urgency_base;              /**< Base urgency level [0, 1] */
    float flexibility;               /**< How flexible is timing [0, 1] */

    /* Associated memory */
    pr_memory_node_t* associated_memory; /**< Linked memory node (optional) */
    void* action_data;               /**< Data for action execution */
    size_t action_data_size;         /**< Size of action data */

    /* State */
    pr_intent_state_t state;         /**< Current state */
    uint64_t created_time_ms;        /**< When intention was created */
    uint64_t last_updated_ms;        /**< Last update timestamp */
    uint32_t reminder_count;         /**< Number of reminders sent */
    uint32_t snooze_count;           /**< Times user snoozed */

    /* Execution tracking */
    uint64_t triggered_time_ms;      /**< When intention was triggered */
    uint64_t completed_time_ms;      /**< When intention completed */
    float completion_quality;        /**< Quality of execution [0, 1] */

    /* User data */
    void* user_data;                 /**< Application-specific data */
} prospective_intention_t;

/**
 * @brief Scheduled intention entry
 *
 * Wraps a prospective intention with scheduling metadata.
 */
typedef struct {
    prospective_intention_t* intention; /**< The intention being scheduled */
    float priority;                  /**< Computed priority (urgency x importance x decay) */
    float time_until_trigger;        /**< Time until trigger (negative = overdue) */
    float urgency_factor;            /**< Current urgency factor */
    float recency_factor;            /**< Recency boost factor */
    reminder_level_t reminder_level; /**< Current reminder level */
    bool reminded_at_level[6];       /**< Track which levels have been reminded */
    size_t heap_index;               /**< Index in priority heap */
    uint32_t conflict_group_id;      /**< ID of conflict group (0 = none) */
} scheduled_intention_t;

/**
 * @brief Priority heap node
 */
typedef struct {
    scheduled_intention_t* item;     /**< Scheduled intention */
    float priority;                  /**< Priority value for heap ordering */
} priority_heap_node_t;

/**
 * @brief Conflict group
 *
 * Intentions that cannot execute simultaneously.
 */
typedef struct {
    uint32_t group_id;               /**< Unique group identifier */
    uint64_t* intention_ids;         /**< Array of conflicting intention IDs */
    size_t num_intentions;           /**< Number of intentions in group */
    size_t capacity;                 /**< Array capacity */
    pr_conflict_strategy_t strategy; /**< Resolution strategy for this group */
} pr_conflict_group_t;

/**
 * @brief Timeline entry for visualization
 */
typedef struct {
    uint64_t intention_id;           /**< Intention identifier */
    char name[128];                  /**< Intention name */
    float start_time;                /**< Start of time window */
    float end_time;                  /**< End of time window */
    float priority;                  /**< Current priority */
    reminder_level_t reminder_level; /**< Current reminder level */
    bool has_conflict;               /**< Whether conflicts exist */
} pr_timeline_entry_t;

/**
 * @brief Cognitive load estimate
 */
typedef struct {
    float total_load;                /**< Total estimated load [0, 1] */
    size_t num_pending;              /**< Number of pending intentions */
    size_t num_imminent;             /**< Number of imminent intentions */
    size_t num_conflicts;            /**< Number of conflict situations */
    float peak_load_time;            /**< When peak load is expected */
    float avg_priority;              /**< Average priority of pending */
} pr_cognitive_load_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Reminder callback function type
 *
 * Called when an intention reaches a new reminder level.
 *
 * @param intention The intention triggering the reminder
 * @param level The reminder level reached
 * @param user_data User-provided context
 */
typedef void (*pr_reminder_callback_t)(
    prospective_intention_t* intention,
    reminder_level_t level,
    void* user_data
);

/**
 * @brief Conflict resolution callback type
 *
 * Called when conflicts need user resolution.
 *
 * @param intentions Array of conflicting intentions
 * @param num_intentions Number of conflicting intentions
 * @param user_data User-provided context
 * @return Index of chosen intention, or -1 to defer
 */
typedef int (*pr_conflict_callback_t)(
    prospective_intention_t** intentions,
    size_t num_intentions,
    void* user_data
);

/**
 * @brief Intention triggered callback type
 *
 * Called when an intention is triggered for execution.
 *
 * @param intention The triggered intention
 * @param user_data User-provided context
 * @return 0 on success, non-zero on failure
 */
typedef int (*pr_trigger_callback_t)(
    prospective_intention_t* intention,
    void* user_data
);

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Scheduler configuration
 */
typedef struct {
    /* Capacity */
    size_t max_intentions;           /**< Maximum scheduled intentions */
    size_t max_conflict_groups;      /**< Maximum conflict groups */

    /* Reminder timing (seconds before trigger) */
    float reminder_far;              /**< FAR reminder threshold */
    float reminder_moderate;         /**< MODERATE reminder threshold */
    float reminder_soon;             /**< SOON reminder threshold */
    float reminder_imminent;         /**< IMMINENT reminder threshold */

    /* Priority computation weights */
    float urgency_weight;            /**< Weight for urgency factor */
    float importance_weight;         /**< Weight for importance factor */
    float recency_weight;            /**< Weight for recency factor */

    /* Time constants */
    float urgency_tau;               /**< Urgency growth time constant */
    float recency_tau;               /**< Recency decay time constant */

    /* Conflict resolution */
    pr_conflict_strategy_t default_conflict_strategy; /**< Default strategy */
    bool allow_concurrent;           /**< Allow multiple simultaneous executions */
    size_t max_concurrent;           /**< Maximum concurrent executions */

    /* Behavior flags */
    bool auto_remove_completed;      /**< Remove completed intentions automatically */
    bool auto_remove_expired;        /**< Remove expired intentions automatically */
    bool enable_snooze;              /**< Allow snoozing reminders */
    float default_snooze_duration;   /**< Default snooze duration (seconds) */

    /* Statistics */
    bool track_statistics;           /**< Enable statistics tracking */
} prospective_scheduler_config_t;

//=============================================================================
// Statistics Structure
//=============================================================================

/**
 * @brief Scheduler statistics
 */
typedef struct {
    /* Counts */
    uint64_t total_added;            /**< Total intentions added */
    uint64_t total_completed;        /**< Successfully completed */
    uint64_t total_failed;           /**< Failed executions */
    uint64_t total_expired;          /**< Expired without execution */
    uint64_t total_cancelled;        /**< User cancelled */

    /* Reminders */
    uint64_t reminders_sent;         /**< Total reminders sent */
    uint64_t reminders_snoozed;      /**< Reminders that were snoozed */

    /* Timing */
    float avg_response_time_ms;      /**< Average time from trigger to completion */
    float avg_early_completion_ms;   /**< Average time before deadline */
    float avg_late_completion_ms;    /**< Average overdue time */

    /* Conflicts */
    uint64_t conflicts_detected;     /**< Total conflict situations */
    uint64_t conflicts_resolved;     /**< Successfully resolved conflicts */

    /* Current state */
    size_t current_pending;          /**< Currently pending intentions */
    size_t current_active;           /**< Currently active intentions */
    float current_load;              /**< Current cognitive load estimate */

    /* Performance */
    double avg_update_time_us;       /**< Average update cycle time */
    uint64_t last_reset_time_ms;     /**< When stats were last reset */
} pr_sched_stats_t;

//=============================================================================
// Main Scheduler Structure
//=============================================================================

/**
 * @brief Prospective Memory Scheduler
 *
 * Central scheduler for managing prospective memory intentions.
 * Uses priority heap for efficient retrieval of highest-priority items.
 */
typedef struct {
    /* Priority heap (max-heap by priority) */
    priority_heap_node_t* heap;      /**< Heap array */
    size_t heap_size;                /**< Current number of items */
    size_t heap_capacity;            /**< Maximum heap capacity */

    /* Time-ordered list for time-based intentions */
    scheduled_intention_t** time_ordered; /**< Time-sorted array */
    size_t num_time_ordered;         /**< Number of time-ordered items */
    size_t time_ordered_capacity;    /**< Array capacity */

    /* All scheduled intentions (for lookup) */
    scheduled_intention_t** all_intentions; /**< All scheduled intentions */
    size_t num_intentions;           /**< Number of scheduled intentions */

    /* Conflict groups */
    pr_conflict_group_t* conflict_groups; /**< Array of conflict groups */
    size_t num_conflict_groups;      /**< Number of active conflict groups */
    uint32_t next_conflict_group_id; /**< Next group ID to assign */

    /* Callbacks */
    pr_reminder_callback_t reminder_callback; /**< Reminder notification callback */
    void* reminder_user_data;        /**< User data for reminder callback */
    pr_conflict_callback_t conflict_callback; /**< Conflict resolution callback */
    void* conflict_user_data;        /**< User data for conflict callback */
    pr_trigger_callback_t trigger_callback; /**< Trigger notification callback */
    void* trigger_user_data;         /**< User data for trigger callback */

    /* Current time tracking */
    float current_time;              /**< Current scheduler time (seconds) */
    uint64_t last_update_ms;         /**< Last update timestamp */

    /* ID generation */
    uint64_t next_intention_id;      /**< Next intention ID to assign */

    /* Configuration */
    prospective_scheduler_config_t config; /**< Current configuration */

    /* Statistics */
    pr_sched_stats_t stats;          /**< Operational statistics */

    /* Internal state */
    bool initialized;                /**< Scheduler initialized flag */
} prospective_scheduler_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default scheduler configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for most use cases
 * HOW:  Sets biologically realistic parameters
 *
 * @return Default configuration with:
 *         - 256 max intentions
 *         - Standard reminder timings
 *         - Balanced priority weights
 *         - HIGHEST_PRIORITY conflict strategy
 *
 * Performance: ~5ns
 */
NIMCP_EXPORT prospective_scheduler_config_t prospective_scheduler_config_default(void);

/**
 * @brief Validate scheduler configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Validation rules:
 * - max_intentions > 0
 * - Reminder timings: far > moderate > soon > imminent > 0
 * - Weights must sum to <= 1.0 and each >= 0
 * - Time constants > 0
 */
NIMCP_EXPORT bool prospective_scheduler_config_validate(
    const prospective_scheduler_config_t* config
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create a new prospective memory scheduler
 *
 * WHAT: Creates and initializes a prospective memory scheduler
 * WHY:  Manage scheduling of future intentions
 * HOW:  Allocates scheduler, initializes heap and data structures
 *
 * @param config Scheduler configuration (NULL for defaults)
 * @return New scheduler or NULL on failure
 *
 * Performance: ~50us
 * Memory: ~256 bytes + max_intentions * 144 bytes
 *
 * Example:
 *   prospective_scheduler_config_t config = prospective_scheduler_config_default();
 *   config.max_intentions = 512;
 *   prospective_scheduler_t* sched = prospective_scheduler_create(&config);
 */
NIMCP_EXPORT prospective_scheduler_t* prospective_scheduler_create(
    const prospective_scheduler_config_t* config
);

/**
 * @brief Destroy a prospective memory scheduler
 *
 * WHAT: Frees all scheduler resources
 * WHY:  Clean shutdown and resource release
 * HOW:  Frees heap, intentions, conflict groups
 *
 * @param scheduler Scheduler to destroy (NULL safe)
 *
 * WARNING: Does not free associated intention data - caller must
 *          iterate and free intention action_data/user_data first.
 *
 * Performance: ~10us
 */
NIMCP_EXPORT void prospective_scheduler_destroy(prospective_scheduler_t* scheduler);

/**
 * @brief Reset scheduler to initial state
 *
 * WHAT: Clears all scheduled intentions and conflicts
 * WHY:  Start fresh scheduling state
 * HOW:  Clears heap, lists, conflict groups, resets statistics
 *
 * @param scheduler Prospective scheduler
 * @return PR_SCHED_SUCCESS or error code
 *
 * WARNING: Does not free intention data - just clears references.
 *
 * Performance: ~20us
 */
NIMCP_EXPORT pr_sched_error_t prospective_scheduler_reset(
    prospective_scheduler_t* scheduler
);

//=============================================================================
// Intention Creation Functions
//=============================================================================

/**
 * @brief Create a new prospective intention
 *
 * WHAT: Creates an intention structure for scheduling
 * WHY:  Intentions must be created before scheduling
 * HOW:  Allocates and initializes intention with given parameters
 *
 * @param name Human-readable name
 * @param description Detailed description
 * @param trigger_type Type of trigger
 * @param importance Importance factor [0, 1]
 * @return New intention or NULL on failure
 *
 * Performance: ~1us
 *
 * Example:
 *   prospective_intention_t* intent = prospective_intention_create(
 *       "Doctor Appointment",
 *       "Annual checkup at Dr. Smith's office",
 *       PR_INTENT_TRIGGER_TIME,
 *       0.9f  // High importance
 *   );
 */
NIMCP_EXPORT prospective_intention_t* prospective_intention_create(
    const char* name,
    const char* description,
    pr_intent_trigger_type_t trigger_type,
    float importance
);

/**
 * @brief Destroy a prospective intention
 *
 * WHAT: Frees intention resources
 * WHY:  Clean up intention memory
 * HOW:  Frees intention and associated data
 *
 * @param intention Intention to destroy (NULL safe)
 *
 * Note: Does NOT free action_data or user_data - caller must free those.
 */
NIMCP_EXPORT void prospective_intention_destroy(prospective_intention_t* intention);

/**
 * @brief Set time window for intention
 *
 * @param intention Prospective intention
 * @param start_time Window start (seconds)
 * @param end_time Window end/deadline (seconds)
 * @param optimal_time Preferred execution time (seconds)
 * @param is_absolute true=absolute timestamp, false=relative to now
 * @return PR_SCHED_SUCCESS or error code
 */
NIMCP_EXPORT pr_sched_error_t prospective_intention_set_time_window(
    prospective_intention_t* intention,
    float start_time,
    float end_time,
    float optimal_time,
    bool is_absolute
);

/**
 * @brief Set event trigger for intention
 *
 * @param intention Prospective intention
 * @param event_type Event type identifier
 * @param cue_signature Optional cue signature for matching
 * @param similarity_threshold Required similarity for match
 * @return PR_SCHED_SUCCESS or error code
 */
NIMCP_EXPORT pr_sched_error_t prospective_intention_set_event_trigger(
    prospective_intention_t* intention,
    const char* event_type,
    const prime_signature_t* cue_signature,
    float similarity_threshold
);

/**
 * @brief Set activity trigger for intention
 *
 * @param intention Prospective intention
 * @param activity_name Name of preceding activity
 * @param activity_id ID of activity to wait for
 * @param require_success Only trigger if activity succeeds
 * @return PR_SCHED_SUCCESS or error code
 */
NIMCP_EXPORT pr_sched_error_t prospective_intention_set_activity_trigger(
    prospective_intention_t* intention,
    const char* activity_name,
    uint64_t activity_id,
    bool require_success
);

//=============================================================================
// Scheduling Functions
//=============================================================================

/**
 * @brief Add intention to scheduler
 *
 * WHAT: Schedules an intention for future execution
 * WHY:  Intention must be scheduled to be tracked and triggered
 * HOW:  Inserts into priority heap and time-ordered list
 *
 * @param scheduler Prospective scheduler
 * @param intention Intention to schedule
 * @return PR_SCHED_SUCCESS or error code
 *
 * Performance: O(log n) heap insertion
 *
 * Example:
 *   prospective_intention_t* intent = prospective_intention_create(...);
 *   prospective_intention_set_time_window(intent, 0, 3600, 3000, false);
 *   pr_sched_error_t err = prospective_scheduler_add(scheduler, intent);
 */
NIMCP_EXPORT pr_sched_error_t prospective_scheduler_add(
    prospective_scheduler_t* scheduler,
    prospective_intention_t* intention
);

/**
 * @brief Remove intention from scheduler
 *
 * WHAT: Removes scheduled intention
 * WHY:  Intention no longer needed or completed
 * HOW:  Removes from heap and lists
 *
 * @param scheduler Prospective scheduler
 * @param intention_id ID of intention to remove
 * @return PR_SCHED_SUCCESS or error code
 *
 * Note: Does NOT destroy the intention - caller retains ownership.
 *
 * Performance: O(log n)
 */
NIMCP_EXPORT pr_sched_error_t prospective_scheduler_remove(
    prospective_scheduler_t* scheduler,
    uint64_t intention_id
);

/**
 * @brief Update scheduler state
 *
 * WHAT: Main update cycle for scheduler
 * WHY:  Recompute priorities, check triggers, fire reminders
 * HOW:  Updates all scheduled intentions based on current time
 *
 * @param scheduler Prospective scheduler
 * @param current_time Current time (seconds, same basis as time windows)
 * @return PR_SCHED_SUCCESS or error code
 *
 * This function:
 * 1. Updates time_until_trigger for all intentions
 * 2. Recomputes priorities based on urgency
 * 3. Checks for newly triggered intentions
 * 4. Updates reminder levels and fires callbacks
 * 5. Handles expired intentions
 *
 * Performance: O(n log n) for full re-heapify
 *
 * Example:
 *   // Update every second
 *   prospective_scheduler_update(scheduler, get_current_time_seconds());
 */
NIMCP_EXPORT pr_sched_error_t prospective_scheduler_update(
    prospective_scheduler_t* scheduler,
    float current_time
);

/**
 * @brief Get highest priority intention
 *
 * WHAT: Returns and removes highest priority intention
 * WHY:  Execute most urgent/important intention first
 * HOW:  Pops from max-heap
 *
 * @param scheduler Prospective scheduler
 * @return Highest priority intention or NULL if empty
 *
 * Note: Intention is removed from scheduler. Caller must manage it.
 *
 * Performance: O(log n)
 */
NIMCP_EXPORT prospective_intention_t* prospective_scheduler_get_next(
    prospective_scheduler_t* scheduler
);

/**
 * @brief Peek at highest priority intention
 *
 * WHAT: Returns highest priority intention without removing
 * WHY:  Inspect next intention without committing to execution
 * HOW:  Reads heap root
 *
 * @param scheduler Prospective scheduler
 * @return Highest priority intention or NULL if empty
 *
 * Performance: O(1)
 */
NIMCP_EXPORT prospective_intention_t* prospective_scheduler_peek(
    const prospective_scheduler_t* scheduler
);

/**
 * @brief Get all due intentions
 *
 * WHAT: Returns all intentions that are currently due
 * WHY:  Handle multiple simultaneous triggers
 * HOW:  Scans for intentions with time_until_trigger <= 0
 *
 * @param scheduler Prospective scheduler
 * @param out_intentions Output array for due intentions
 * @param max_count Maximum intentions to return
 * @return Number of due intentions found, or -1 on error
 *
 * Note: Intentions are NOT removed from scheduler.
 *
 * Performance: O(n)
 */
NIMCP_EXPORT int prospective_scheduler_get_due(
    prospective_scheduler_t* scheduler,
    prospective_intention_t** out_intentions,
    size_t max_count
);

/**
 * @brief Get intentions within time window
 *
 * WHAT: Returns intentions due within specified time range
 * WHY:  Look ahead for upcoming intentions
 * HOW:  Scans time-ordered list
 *
 * @param scheduler Prospective scheduler
 * @param time_start Start of time window (seconds)
 * @param time_end End of time window (seconds)
 * @param out_intentions Output array for intentions
 * @param max_count Maximum intentions to return
 * @return Number of intentions found, or -1 on error
 *
 * Performance: O(n) scan
 */
NIMCP_EXPORT int prospective_scheduler_get_in_window(
    const prospective_scheduler_t* scheduler,
    float time_start,
    float time_end,
    prospective_intention_t** out_intentions,
    size_t max_count
);

//=============================================================================
// Reminder Functions
//=============================================================================

/**
 * @brief Check and fire reminder callbacks
 *
 * WHAT: Checks reminder levels and fires callbacks as needed
 * WHY:  Notify user as deadlines approach
 * HOW:  Compares time_until_trigger to reminder thresholds
 *
 * @param scheduler Prospective scheduler
 * @return Number of reminders fired, or -1 on error
 *
 * Fires reminder callback for each intention that:
 * - Has reached a new reminder level
 * - Has not already been reminded at that level
 *
 * Performance: O(n)
 */
NIMCP_EXPORT int prospective_scheduler_check_reminders(
    prospective_scheduler_t* scheduler
);

/**
 * @brief Set reminder callback function
 *
 * @param scheduler Prospective scheduler
 * @param callback Callback function (NULL to disable)
 * @param user_data User data passed to callback
 * @return PR_SCHED_SUCCESS or error code
 */
NIMCP_EXPORT pr_sched_error_t prospective_scheduler_set_reminder_callback(
    prospective_scheduler_t* scheduler,
    pr_reminder_callback_t callback,
    void* user_data
);

/**
 * @brief Snooze reminder for intention
 *
 * WHAT: Delays reminder by snooze duration
 * WHY:  User acknowledges but not ready to act
 * HOW:  Adjusts reminder thresholds for this intention
 *
 * @param scheduler Prospective scheduler
 * @param intention_id ID of intention to snooze
 * @param snooze_duration Snooze duration in seconds (0 for default)
 * @return PR_SCHED_SUCCESS or error code
 */
NIMCP_EXPORT pr_sched_error_t prospective_scheduler_snooze(
    prospective_scheduler_t* scheduler,
    uint64_t intention_id,
    float snooze_duration
);

/**
 * @brief Get current reminder level for intention
 *
 * @param scheduler Prospective scheduler
 * @param intention_id ID of intention
 * @return Current reminder level or REMINDER_NONE on error
 */
NIMCP_EXPORT reminder_level_t prospective_scheduler_get_reminder_level(
    const prospective_scheduler_t* scheduler,
    uint64_t intention_id
);

//=============================================================================
// Conflict Management Functions
//=============================================================================

/**
 * @brief Add intentions to conflict group
 *
 * WHAT: Marks intentions as conflicting (cannot execute together)
 * WHY:  Resource conflicts, mutually exclusive actions
 * HOW:  Creates or updates conflict group
 *
 * @param scheduler Prospective scheduler
 * @param intention_ids Array of conflicting intention IDs
 * @param num_intentions Number of intentions in group
 * @param strategy Resolution strategy for this group
 * @return Group ID on success, or 0 on error
 *
 * Example:
 *   uint64_t ids[] = {intent1->intention_id, intent2->intention_id};
 *   uint32_t group = prospective_scheduler_add_conflict(
 *       scheduler, ids, 2, PR_CONFLICT_HIGHEST_PRIORITY);
 */
NIMCP_EXPORT uint32_t prospective_scheduler_add_conflict(
    prospective_scheduler_t* scheduler,
    const uint64_t* intention_ids,
    size_t num_intentions,
    pr_conflict_strategy_t strategy
);

/**
 * @brief Remove conflict group
 *
 * @param scheduler Prospective scheduler
 * @param group_id ID of conflict group to remove
 * @return PR_SCHED_SUCCESS or error code
 */
NIMCP_EXPORT pr_sched_error_t prospective_scheduler_remove_conflict(
    prospective_scheduler_t* scheduler,
    uint32_t group_id
);

/**
 * @brief Resolve conflict between intentions
 *
 * WHAT: Chooses which intention to execute when conflicts occur
 * WHY:  Cannot execute conflicting intentions simultaneously
 * HOW:  Applies resolution strategy or fires callback
 *
 * @param scheduler Prospective scheduler
 * @param group_id Conflict group to resolve
 * @return ID of chosen intention, or PR_SCHED_INVALID_ID on error
 *
 * Resolution based on group's strategy:
 * - HIGHEST_PRIORITY: Returns highest priority intention
 * - SEQUENTIAL: Returns first by deadline
 * - BATCH: Returns all compatible (may need multiple calls)
 * - USER_CHOICE: Fires callback and returns chosen
 */
NIMCP_EXPORT uint64_t prospective_scheduler_resolve_conflict(
    prospective_scheduler_t* scheduler,
    uint32_t group_id
);

/**
 * @brief Set conflict resolution callback
 *
 * @param scheduler Prospective scheduler
 * @param callback Callback function (NULL to disable)
 * @param user_data User data passed to callback
 * @return PR_SCHED_SUCCESS or error code
 */
NIMCP_EXPORT pr_sched_error_t prospective_scheduler_set_conflict_callback(
    prospective_scheduler_t* scheduler,
    pr_conflict_callback_t callback,
    void* user_data
);

/**
 * @brief Check if intention has conflicts
 *
 * @param scheduler Prospective scheduler
 * @param intention_id ID of intention to check
 * @return true if conflicts exist, false otherwise
 */
NIMCP_EXPORT bool prospective_scheduler_has_conflict(
    const prospective_scheduler_t* scheduler,
    uint64_t intention_id
);

//=============================================================================
// Rescheduling Functions
//=============================================================================

/**
 * @brief Reschedule an intention
 *
 * WHAT: Changes timing of scheduled intention
 * WHY:  User wants to change when to execute
 * HOW:  Updates time window, recomputes priority
 *
 * @param scheduler Prospective scheduler
 * @param intention_id ID of intention to reschedule
 * @param new_start New start time
 * @param new_end New end time (deadline)
 * @param new_optimal New optimal time
 * @return PR_SCHED_SUCCESS or error code
 */
NIMCP_EXPORT pr_sched_error_t prospective_scheduler_reschedule(
    prospective_scheduler_t* scheduler,
    uint64_t intention_id,
    float new_start,
    float new_end,
    float new_optimal
);

/**
 * @brief Update intention importance
 *
 * @param scheduler Prospective scheduler
 * @param intention_id ID of intention to update
 * @param new_importance New importance value [0, 1]
 * @return PR_SCHED_SUCCESS or error code
 */
NIMCP_EXPORT pr_sched_error_t prospective_scheduler_update_importance(
    prospective_scheduler_t* scheduler,
    uint64_t intention_id,
    float new_importance
);

/**
 * @brief Mark intention as completed
 *
 * @param scheduler Prospective scheduler
 * @param intention_id ID of completed intention
 * @param quality Completion quality [0, 1]
 * @return PR_SCHED_SUCCESS or error code
 */
NIMCP_EXPORT pr_sched_error_t prospective_scheduler_mark_completed(
    prospective_scheduler_t* scheduler,
    uint64_t intention_id,
    float quality
);

/**
 * @brief Mark intention as failed
 *
 * @param scheduler Prospective scheduler
 * @param intention_id ID of failed intention
 * @return PR_SCHED_SUCCESS or error code
 */
NIMCP_EXPORT pr_sched_error_t prospective_scheduler_mark_failed(
    prospective_scheduler_t* scheduler,
    uint64_t intention_id
);

/**
 * @brief Cancel an intention
 *
 * @param scheduler Prospective scheduler
 * @param intention_id ID of intention to cancel
 * @return PR_SCHED_SUCCESS or error code
 */
NIMCP_EXPORT pr_sched_error_t prospective_scheduler_cancel(
    prospective_scheduler_t* scheduler,
    uint64_t intention_id
);

//=============================================================================
// Timeline and Load Functions
//=============================================================================

/**
 * @brief Get upcoming timeline
 *
 * WHAT: Returns timeline of upcoming intentions
 * WHY:  Visualization of scheduled activities
 * HOW:  Generates sorted timeline entries
 *
 * @param scheduler Prospective scheduler
 * @param time_horizon How far ahead to look (seconds)
 * @param out_entries Output array for timeline entries
 * @param max_entries Maximum entries to return
 * @return Number of entries, or -1 on error
 *
 * Performance: O(n log n) for sorting
 */
NIMCP_EXPORT int prospective_scheduler_get_timeline(
    const prospective_scheduler_t* scheduler,
    float time_horizon,
    pr_timeline_entry_t* out_entries,
    size_t max_entries
);

/**
 * @brief Estimate future cognitive load
 *
 * WHAT: Estimates cognitive load from scheduled intentions
 * WHY:  Avoid overloading with too many simultaneous intentions
 * HOW:  Sums weighted priorities in time windows
 *
 * @param scheduler Prospective scheduler
 * @param time_horizon How far ahead to estimate (seconds)
 * @param out_load Output load estimate
 * @return PR_SCHED_SUCCESS or error code
 *
 * Load factors:
 * - Number of pending intentions
 * - Priority sum in near future
 * - Number of conflicts
 * - Deadline clustering
 */
NIMCP_EXPORT pr_sched_error_t prospective_scheduler_estimate_load(
    const prospective_scheduler_t* scheduler,
    float time_horizon,
    pr_cognitive_load_t* out_load
);

/**
 * @brief Check if scheduler can accept more intentions
 *
 * @param scheduler Prospective scheduler
 * @return true if more intentions can be added
 */
NIMCP_EXPORT bool prospective_scheduler_has_capacity(
    const prospective_scheduler_t* scheduler
);

/**
 * @brief Get number of scheduled intentions
 *
 * @param scheduler Prospective scheduler
 * @return Number of scheduled intentions
 */
NIMCP_EXPORT size_t prospective_scheduler_get_count(
    const prospective_scheduler_t* scheduler
);

//=============================================================================
// Event Notification Functions
//=============================================================================

/**
 * @brief Notify scheduler of event occurrence
 *
 * WHAT: Reports an event that may trigger event-based intentions
 * WHY:  Event-based prospective memory needs event detection
 * HOW:  Matches event against event-based intention triggers
 *
 * @param scheduler Prospective scheduler
 * @param event_type Event type that occurred
 * @param event_signature Optional signature of event (for matching)
 * @return Number of intentions triggered, or -1 on error
 *
 * Example:
 *   // "See John" event occurs
 *   prospective_scheduler_notify_event(scheduler, "see_person", &john_sig);
 */
NIMCP_EXPORT int prospective_scheduler_notify_event(
    prospective_scheduler_t* scheduler,
    const char* event_type,
    const prime_signature_t* event_signature
);

/**
 * @brief Notify scheduler of activity completion
 *
 * WHAT: Reports activity completion for activity-based triggers
 * WHY:  Activity-based intentions need completion detection
 * HOW:  Matches activity against activity-based intention triggers
 *
 * @param scheduler Prospective scheduler
 * @param activity_id ID of completed activity
 * @param success Whether activity succeeded
 * @return Number of intentions triggered, or -1 on error
 */
NIMCP_EXPORT int prospective_scheduler_notify_activity(
    prospective_scheduler_t* scheduler,
    uint64_t activity_id,
    bool success
);

/**
 * @brief Set trigger callback
 *
 * Called when an intention is triggered for execution.
 *
 * @param scheduler Prospective scheduler
 * @param callback Callback function (NULL to disable)
 * @param user_data User data passed to callback
 * @return PR_SCHED_SUCCESS or error code
 */
NIMCP_EXPORT pr_sched_error_t prospective_scheduler_set_trigger_callback(
    prospective_scheduler_t* scheduler,
    pr_trigger_callback_t callback,
    void* user_data
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Find intention by ID
 *
 * @param scheduler Prospective scheduler
 * @param intention_id ID to find
 * @return Intention or NULL if not found
 */
NIMCP_EXPORT prospective_intention_t* prospective_scheduler_find(
    const prospective_scheduler_t* scheduler,
    uint64_t intention_id
);

/**
 * @brief Get scheduled intention info
 *
 * @param scheduler Prospective scheduler
 * @param intention_id ID of intention
 * @param out_scheduled Output scheduled intention info
 * @return PR_SCHED_SUCCESS or error code
 */
NIMCP_EXPORT pr_sched_error_t prospective_scheduler_get_scheduled_info(
    const prospective_scheduler_t* scheduler,
    uint64_t intention_id,
    scheduled_intention_t* out_scheduled
);

/**
 * @brief Get priority for intention
 *
 * @param scheduler Prospective scheduler
 * @param intention_id ID of intention
 * @return Current priority or -1.0f if not found
 */
NIMCP_EXPORT float prospective_scheduler_get_priority(
    const prospective_scheduler_t* scheduler,
    uint64_t intention_id
);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get scheduler statistics
 *
 * @param scheduler Prospective scheduler
 * @param out_stats Output statistics structure
 * @return PR_SCHED_SUCCESS or error code
 */
NIMCP_EXPORT pr_sched_error_t prospective_scheduler_get_stats(
    const prospective_scheduler_t* scheduler,
    pr_sched_stats_t* out_stats
);

/**
 * @brief Reset scheduler statistics
 *
 * @param scheduler Prospective scheduler
 * @return PR_SCHED_SUCCESS or error code
 */
NIMCP_EXPORT pr_sched_error_t prospective_scheduler_reset_stats(
    prospective_scheduler_t* scheduler
);

/**
 * @brief Print scheduler state (debug)
 *
 * @param scheduler Prospective scheduler
 */
NIMCP_EXPORT void prospective_scheduler_print_state(
    const prospective_scheduler_t* scheduler
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
NIMCP_EXPORT const char* pr_sched_error_string(pr_sched_error_t error);

/**
 * @brief Get reminder level name as string
 *
 * @param level Reminder level
 * @return Static string name
 */
NIMCP_EXPORT const char* pr_reminder_level_name(reminder_level_t level);

/**
 * @brief Get trigger type name as string
 *
 * @param type Trigger type
 * @return Static string name
 */
NIMCP_EXPORT const char* pr_trigger_type_name(pr_intent_trigger_type_t type);

/**
 * @brief Get intention state name as string
 *
 * @param state Intention state
 * @return Static string name
 */
NIMCP_EXPORT const char* pr_intent_state_name(pr_intent_state_t state);

/**
 * @brief Get conflict strategy name as string
 *
 * @param strategy Conflict strategy
 * @return Static string name
 */
NIMCP_EXPORT const char* pr_conflict_strategy_name(pr_conflict_strategy_t strategy);

/**
 * @brief Get current time in milliseconds
 *
 * @return Milliseconds since epoch
 */
NIMCP_EXPORT uint64_t pr_sched_current_time_ms(void);

/**
 * @brief Compute priority for intention
 *
 * Utility function to compute priority from factors.
 *
 * @param urgency Urgency factor [0, 1]
 * @param importance Importance factor [0, 1]
 * @param recency Recency factor [0, 1]
 * @param config Configuration with weights
 * @return Computed priority
 */
NIMCP_EXPORT float pr_sched_compute_priority(
    float urgency,
    float importance,
    float recency,
    const prospective_scheduler_config_t* config
);

/**
 * @brief Compute urgency from time until trigger
 *
 * @param time_until_trigger Time until trigger (negative = overdue)
 * @param urgency_tau Time constant for urgency growth
 * @return Urgency factor [0, 1+] (can exceed 1 when overdue)
 */
NIMCP_EXPORT float pr_sched_compute_urgency(
    float time_until_trigger,
    float urgency_tau
);

/**
 * @brief Compute reminder level from time until trigger
 *
 * @param time_until_trigger Time until trigger (seconds)
 * @param config Configuration with reminder thresholds
 * @return Appropriate reminder level
 */
NIMCP_EXPORT reminder_level_t pr_sched_compute_reminder_level(
    float time_until_trigger,
    const prospective_scheduler_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PR_PROSPECTIVE_SCHEDULER_H
