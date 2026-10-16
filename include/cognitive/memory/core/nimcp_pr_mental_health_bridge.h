//=============================================================================
// nimcp_pr_mental_health_bridge.h - Prime Resonant Mental Health Bridge
//=============================================================================
/**
 * @file nimcp_pr_mental_health_bridge.h
 * @brief Integration bridge between PR memory and mental health monitoring
 *
 * WHAT: Bidirectional bridge connecting PR memory system with mental health
 *       monitoring for trauma handling, rumination detection, and mood tracking
 * WHY:  Memory patterns are key indicators of mental health - rumination in
 *       depression, intrusive memories in PTSD, and mood-congruent recall bias
 * HOW:  Monitors memory retrieval patterns, emotional valence distributions,
 *       and provides therapeutic intervention hooks
 *
 * ARCHITECTURE:
 *
 *   Prime Resonant Mental Health Bridge:
 *   +-----------------------------------------------------------------------+
 *   |                     PR Mental Health Bridge                          |
 *   |                                                                       |
 *   |  +------------------+       +------------------------+                |
 *   |  | Rumination       |<----->| Repeated negative      |                |
 *   |  | Detector         |       | retrieval patterns     |                |
 *   |  +------------------+       +------------------------+                |
 *   |                                                                       |
 *   |  +------------------+       +------------------------+                |
 *   |  | Intrusion        |<----->| Unwanted spontaneous   |                |
 *   |  | Tracker          |       | memory recall          |                |
 *   |  +------------------+       +------------------------+                |
 *   |                                                                       |
 *   |  +------------------+       +------------------------+                |
 *   |  | Valence Bias     |<----->| Mood-congruent recall  |                |
 *   |  | Analyzer         |       | bias assessment        |                |
 *   |  +------------------+       +------------------------+                |
 *   |                                                                       |
 *   |  +------------------+       +------------------------+                |
 *   |  | Trauma Load      |<----->| Flashbulb/trauma       |                |
 *   |  | Assessor         |       | memory burden          |                |
 *   |  +------------------+       +------------------------+                |
 *   |                                                                       |
 *   |  +------------------+       +------------------------+                |
 *   |  | Intervention     |<----->| Therapeutic action     |                |
 *   |  | Hooks            |       | triggers               |                |
 *   |  +------------------+       +------------------------+                |
 *   +-----------------------------------------------------------------------+
 *
 * PSYCHOLOGICAL FOUNDATION:
 *
 *   Rumination Model (Nolen-Hoeksema, 1991):
 *   +-----------------------------------------------------------------------+
 *   |  Rumination: Repetitive focus on negative content without resolution |
 *   |                                                                       |
 *   |  Detection criteria:                                                  |
 *   |  - Same negative memory retrieved multiple times                     |
 *   |  - Short inter-retrieval intervals (obsessive pattern)               |
 *   |  - Negative valence memories dominate recent retrievals              |
 *   |  - Lack of new information or resolution in retrievals               |
 *   +-----------------------------------------------------------------------+
 *
 *   Intrusive Memory Model (Brewin et al., 2010):
 *   +-----------------------------------------------------------------------+
 *   |  Intrusive memories: Involuntary, distressing recollections          |
 *   |                                                                       |
 *   |  Characteristics:                                                     |
 *   |  - Spontaneous (not cue-driven) retrieval                            |
 *   |  - High emotional intensity                                           |
 *   |  - Often trauma-related or highly negative                           |
 *   |  - Associated with sensory vividness                                 |
 *   +-----------------------------------------------------------------------+
 *
 *   Mood-Congruent Memory Bias (Beck, 1967):
 *   +-----------------------------------------------------------------------+
 *   |  Depression: Bias toward retrieving negative memories                |
 *   |  Mania: Bias toward retrieving positive memories                     |
 *   |  Anxiety: Bias toward threat-related memories                        |
 *   |                                                                       |
 *   |  Valence bias = (positive - negative) / total                        |
 *   |  Healthy range: approximately balanced (-0.2 to +0.2)               |
 *   |  Depressive bias: strongly negative (< -0.3)                        |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Rumination check: O(R) where R = retrieval history window
 * - Intrusion track: O(1) for single event, O(I) for batch
 * - Valence analysis: O(N) where N = recent memory count
 * - Mood inference: O(N) where N = retrieval window
 *
 * MEMORY:
 * - pr_mental_health_bridge_t: ~2KB base + retrieval history
 * - Retrieval history: configurable window (default 1000 entries)
 *
 * THREAD SAFETY:
 * - All public functions are thread-safe via internal mutex
 * - Callbacks are invoked with mutex released
 *
 * INTEGRATION:
 * - nimcp_pr_memory_node.h: Memory nodes with quaternion state (valence)
 * - nimcp_flashbulb.h: Trauma/flashbulb memory system
 * - nimcp_reconsolidation.h: Memory modification during therapy
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_PR_MENTAL_HEALTH_BRIDGE_H
#define NIMCP_PR_MENTAL_HEALTH_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Core dependencies
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_flashbulb.h"
#include "cognitive/memory/core/nimcp_quaternion.h"

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

/** Default retrieval history window size */
#define PR_MH_DEFAULT_HISTORY_SIZE          1000

/** Default rumination time window (ms) */
#define PR_MH_DEFAULT_RUMINATION_WINDOW_MS  3600000   // 1 hour

/** Default rumination threshold (repeat count) */
#define PR_MH_DEFAULT_RUMINATION_THRESHOLD  5

/** Default intrusion intensity threshold */
#define PR_MH_DEFAULT_INTRUSION_THRESHOLD   0.7f

/** Default valence bias healthy range */
#define PR_MH_VALENCE_BIAS_HEALTHY_MIN      -0.2f
#define PR_MH_VALENCE_BIAS_HEALTHY_MAX      0.2f

/** Default valence bias depressive threshold */
#define PR_MH_VALENCE_BIAS_DEPRESSIVE       -0.3f

/** Default trauma load threshold for intervention */
#define PR_MH_TRAUMA_LOAD_THRESHOLD         0.7f

/** Maximum tracked intrusive memories */
#define PR_MH_MAX_INTRUSIVE_MEMORIES        256

/** Maximum intervention callbacks */
#define PR_MH_MAX_INTERVENTION_CALLBACKS    16

/** Minimum time between interventions (ms) */
#define PR_MH_MIN_INTERVENTION_INTERVAL_MS  60000     // 1 minute

/** Numerical epsilon */
#define PR_MH_EPSILON                       1e-6f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Mental health indicator types
 */
typedef enum {
    PR_MH_INDICATOR_RUMINATION = 0,     /**< Rumination detected */
    PR_MH_INDICATOR_INTRUSION,          /**< Intrusive memory detected */
    PR_MH_INDICATOR_NEGATIVE_BIAS,      /**< Negative valence bias */
    PR_MH_INDICATOR_POSITIVE_BIAS,      /**< Excessive positive bias (mania) */
    PR_MH_INDICATOR_TRAUMA_LOAD,        /**< High trauma memory burden */
    PR_MH_INDICATOR_AVOIDANCE,          /**< Memory avoidance pattern */
    PR_MH_INDICATOR_FRAGMENTATION,      /**< Fragmented memory patterns */
    PR_MH_INDICATOR_COUNT               /**< Number of indicator types */
} pr_mh_indicator_t;

/**
 * @brief Intervention types for therapeutic hooks
 */
typedef enum {
    PR_MH_INTERVENTION_NONE = 0,        /**< No intervention */
    PR_MH_INTERVENTION_DISTRACTION,     /**< Redirect attention */
    PR_MH_INTERVENTION_RECONSOLIDATION, /**< Trigger reconsolidation */
    PR_MH_INTERVENTION_DAMPENING,       /**< Emotional dampening */
    PR_MH_INTERVENTION_EXPOSURE,        /**< Controlled exposure */
    PR_MH_INTERVENTION_COGNITIVE,       /**< Cognitive reframing */
    PR_MH_INTERVENTION_MINDFULNESS,     /**< Mindfulness interrupt */
    PR_MH_INTERVENTION_COUNT            /**< Number of intervention types */
} pr_mh_intervention_type_t;

/**
 * @brief Mood state enumeration
 */
typedef enum {
    PR_MH_MOOD_UNKNOWN = 0,             /**< Insufficient data */
    PR_MH_MOOD_DEPRESSED,               /**< Depressed mood indicated */
    PR_MH_MOOD_ANXIOUS,                 /**< Anxious mood indicated */
    PR_MH_MOOD_NEUTRAL,                 /**< Neutral/balanced mood */
    PR_MH_MOOD_POSITIVE,                /**< Positive mood indicated */
    PR_MH_MOOD_ELEVATED,                /**< Elevated/manic mood */
    PR_MH_MOOD_MIXED,                   /**< Mixed indicators */
    PR_MH_MOOD_COUNT                    /**< Number of mood states */
} pr_mh_mood_state_t;

/**
 * @brief Error codes for mental health bridge operations
 */
typedef enum {
    PR_MH_SUCCESS = 0,                      /**< Operation succeeded */
    PR_MH_ERROR_NULL_POINTER = -1,          /**< NULL pointer argument */
    PR_MH_ERROR_NOT_INITIALIZED = -2,       /**< Bridge not initialized */
    PR_MH_ERROR_INVALID_CONFIG = -3,        /**< Invalid configuration */
    PR_MH_ERROR_NO_MEMORY = -4,             /**< Memory allocation failed */
    PR_MH_ERROR_CAPACITY = -5,              /**< Capacity limit reached */
    PR_MH_ERROR_NOT_FOUND = -6,             /**< Memory/entry not found */
    PR_MH_ERROR_INSUFFICIENT_DATA = -7,     /**< Not enough data for analysis */
    PR_MH_ERROR_INTERVENTION_BLOCKED = -8,  /**< Intervention rate limited */
    PR_MH_ERROR_INVALID_STATE = -9          /**< Invalid state for operation */
} pr_mh_error_t;

/**
 * @brief Retrieval event record for history tracking
 */
typedef struct {
    uint64_t node_id;                   /**< Memory node retrieved */
    uint64_t timestamp_ms;              /**< When retrieved */
    float valence;                      /**< Emotional valence at retrieval */
    float arousal;                      /**< Arousal level at retrieval */
    float intensity;                    /**< Retrieval intensity/strength */
    bool was_voluntary;                 /**< Voluntary vs involuntary recall */
    bool is_trauma_related;             /**< Related to trauma memory */
    uint32_t retrieval_count;           /**< Times this memory retrieved */
} pr_mh_retrieval_event_t;

/**
 * @brief Rumination pattern detection state
 */
typedef struct {
    uint64_t target_node_id;            /**< Memory being ruminated on */
    uint32_t repetition_count;          /**< Times retrieved in window */
    uint64_t first_retrieval_ms;        /**< First retrieval timestamp */
    uint64_t last_retrieval_ms;         /**< Most recent retrieval */
    float mean_interval_ms;             /**< Mean inter-retrieval interval */
    float pattern_strength;             /**< Rumination pattern strength [0-1] */
    bool is_active;                     /**< Currently active pattern */
} pr_mh_rumination_pattern_t;

/**
 * @brief Intrusive memory tracking record
 */
typedef struct {
    uint64_t node_id;                   /**< Intrusive memory node */
    uint32_t intrusion_count;           /**< Total intrusion events */
    uint64_t first_intrusion_ms;        /**< First intrusion timestamp */
    uint64_t last_intrusion_ms;         /**< Most recent intrusion */
    float average_intensity;            /**< Average intrusion intensity */
    float distress_level;               /**< Associated distress [0-1] */
    bool is_flashbulb;                  /**< Associated with flashbulb memory */
    bool is_trauma;                     /**< Associated with trauma */
} pr_mh_intrusion_record_t;

/**
 * @brief Valence bias analysis result
 */
typedef struct {
    float bias_score;                   /**< Bias score [-1 to +1] */
    float positive_ratio;               /**< Ratio of positive retrievals */
    float negative_ratio;               /**< Ratio of negative retrievals */
    float neutral_ratio;                /**< Ratio of neutral retrievals */
    uint32_t total_retrievals;          /**< Total retrievals analyzed */
    uint64_t analysis_window_ms;        /**< Time window analyzed */
    bool indicates_depression;          /**< Strong negative bias */
    bool indicates_mania;               /**< Strong positive bias */
    float confidence;                   /**< Confidence in analysis [0-1] */
} pr_mh_valence_bias_t;

/**
 * @brief Trauma load assessment result
 */
typedef struct {
    float trauma_load;                  /**< Overall trauma burden [0-1] */
    uint32_t trauma_memory_count;       /**< Number of trauma memories */
    uint32_t active_intrusions;         /**< Currently active intrusions */
    float mean_intrusion_frequency;     /**< Average intrusion rate */
    float mean_avoidance_level;         /**< Average avoidance behavior */
    float mean_distress;                /**< Average distress level */
    float hyperarousal_index;           /**< Hyperarousal indicator */
    bool needs_intervention;            /**< Recommends intervention */
    pr_mh_intervention_type_t suggested_intervention;
} pr_mh_trauma_assessment_t;

/**
 * @brief Mood inference result
 */
typedef struct {
    pr_mh_mood_state_t primary_mood;    /**< Primary mood indication */
    pr_mh_mood_state_t secondary_mood;  /**< Secondary mood (if mixed) */
    float confidence;                   /**< Confidence in inference [0-1] */
    float valence_trend;                /**< Valence trend over time */
    float arousal_mean;                 /**< Mean arousal level */
    float variability;                  /**< Mood variability score */
    uint32_t samples_used;              /**< Memory samples used */
    uint64_t inference_time_ms;         /**< When inference was made */
} pr_mh_mood_inference_t;

/**
 * @brief Intervention request structure
 */
typedef struct {
    pr_mh_intervention_type_t type;     /**< Intervention type */
    pr_mh_indicator_t trigger;          /**< What triggered intervention */
    uint64_t target_node_id;            /**< Target memory (if applicable) */
    float severity;                     /**< Severity of trigger [0-1] */
    float urgency;                      /**< How urgent [0-1] */
    uint64_t timestamp_ms;              /**< When triggered */
    void* context_data;                 /**< Additional context */
} pr_mh_intervention_request_t;

/**
 * @brief Intervention callback function type
 *
 * @param request Intervention request details
 * @param user_data User-provided context
 * @return 0 if handled, negative on error
 */
typedef int (*pr_mh_intervention_callback_t)(
    const pr_mh_intervention_request_t* request,
    void* user_data
);

/**
 * @brief Bridge configuration
 */
typedef struct {
    // History configuration
    size_t retrieval_history_size;          /**< Max retrieval events to track */
    uint64_t analysis_window_ms;            /**< Time window for analysis */

    // Rumination detection
    bool enable_rumination_detection;       /**< Enable rumination detector */
    uint32_t rumination_threshold;          /**< Repeat count threshold */
    uint64_t rumination_window_ms;          /**< Time window for rumination */
    float rumination_valence_threshold;     /**< Valence threshold (negative) */

    // Intrusion tracking
    bool enable_intrusion_tracking;         /**< Enable intrusion tracker */
    float intrusion_intensity_threshold;    /**< Intensity threshold */
    float intrusion_distress_threshold;     /**< Distress threshold */

    // Valence bias analysis
    bool enable_valence_analysis;           /**< Enable valence bias analysis */
    float valence_bias_healthy_min;         /**< Healthy range minimum */
    float valence_bias_healthy_max;         /**< Healthy range maximum */
    float valence_bias_depressive;          /**< Depression threshold */
    float valence_bias_manic;               /**< Mania threshold */

    // Trauma assessment
    bool enable_trauma_assessment;          /**< Enable trauma load assessment */
    float trauma_load_threshold;            /**< Intervention threshold */

    // Intervention configuration
    bool enable_auto_intervention;          /**< Enable automatic interventions */
    uint64_t min_intervention_interval_ms;  /**< Minimum time between interventions */

    // Flashbulb integration
    flashbulb_system_t* flashbulb_system;   /**< Optional flashbulb system */
} pr_mh_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    // Event counts
    uint64_t total_retrievals_tracked;      /**< Total retrievals tracked */
    uint64_t voluntary_retrievals;          /**< Voluntary recall count */
    uint64_t involuntary_retrievals;        /**< Involuntary recall count */
    uint64_t trauma_related_retrievals;     /**< Trauma-related retrievals */

    // Detection counts
    uint64_t rumination_episodes;           /**< Rumination episodes detected */
    uint64_t intrusion_events;              /**< Intrusion events detected */
    uint64_t bias_alerts;                   /**< Valence bias alerts */
    uint64_t trauma_alerts;                 /**< Trauma load alerts */

    // Intervention counts
    uint64_t interventions_triggered;       /**< Interventions triggered */
    uint64_t interventions_blocked;         /**< Rate-limited interventions */

    // Current state
    pr_mh_mood_state_t current_mood;        /**< Most recent mood inference */
    float current_valence_bias;             /**< Current valence bias */
    float current_trauma_load;              /**< Current trauma load */
    uint32_t active_ruminations;            /**< Active rumination patterns */
    uint32_t tracked_intrusions;            /**< Currently tracked intrusions */

    // Timing
    uint64_t last_analysis_ms;              /**< Last analysis timestamp */
    uint64_t uptime_ms;                     /**< Bridge uptime */
} pr_mh_stats_t;

/**
 * @brief Opaque mental health bridge handle
 */
typedef struct pr_mental_health_bridge_struct* pr_mental_health_bridge_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default mental health bridge configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for most use cases
 *
 * @return Default configuration with:
 *         - retrieval_history_size: 1000
 *         - analysis_window_ms: 3600000 (1 hour)
 *         - rumination_threshold: 5
 *         - intrusion_intensity_threshold: 0.7
 *         - All detection features enabled
 *
 * Performance: O(1)
 */
NIMCP_EXPORT pr_mh_config_t pr_mental_health_bridge_default_config(void);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Performance: O(1)
 */
NIMCP_EXPORT bool pr_mental_health_bridge_validate_config(
    const pr_mh_config_t* config
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create mental health bridge
 *
 * WHAT: Creates bridge for memory-mental health integration
 * WHY:  Central coordinator for mental health monitoring via memory patterns
 * HOW:  Allocates bridge, initializes trackers, prepares for monitoring
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 *
 * Performance: O(history_size) for allocation
 * Memory: ~2KB base + history_size * sizeof(retrieval_event)
 * Thread safety: Thread-safe
 */
NIMCP_EXPORT pr_mental_health_bridge_t pr_mental_health_bridge_create(
    const pr_mh_config_t* config
);

/**
 * @brief Destroy mental health bridge
 *
 * WHAT: Releases bridge and all resources
 * WHY:  Clean shutdown
 * HOW:  Frees all tracking structures, releases mutex
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * Performance: O(tracked_items)
 * Thread safety: Thread-safe
 */
NIMCP_EXPORT void pr_mental_health_bridge_destroy(
    pr_mental_health_bridge_t bridge
);

/**
 * @brief Reset bridge to initial state
 *
 * Clears all tracking history but keeps configuration.
 *
 * @param bridge Bridge handle
 * @return PR_MH_SUCCESS or error code
 */
NIMCP_EXPORT pr_mh_error_t pr_mental_health_bridge_reset(
    pr_mental_health_bridge_t bridge
);

//=============================================================================
// Retrieval Tracking Functions
//=============================================================================

/**
 * @brief Track a memory retrieval event
 *
 * WHAT: Records memory retrieval for pattern analysis
 * WHY:  Retrieval patterns indicate mental health state
 * HOW:  Adds to history, updates running statistics
 *
 * @param bridge Mental health bridge
 * @param node Memory node that was retrieved
 * @param was_voluntary Whether retrieval was voluntary
 * @param intensity Retrieval intensity/activation strength [0-1]
 * @return PR_MH_SUCCESS or error code
 *
 * Performance: O(1) amortized
 *
 * SIDE EFFECTS:
 * - Updates retrieval history
 * - May trigger rumination detection
 * - May trigger intrusion detection if involuntary
 * - Updates running valence statistics
 */
NIMCP_EXPORT pr_mh_error_t pr_mental_health_bridge_track_retrieval(
    pr_mental_health_bridge_t bridge,
    const pr_memory_node_t* node,
    bool was_voluntary,
    float intensity
);

/**
 * @brief Track a memory retrieval with full event details
 *
 * @param bridge Mental health bridge
 * @param event Complete retrieval event
 * @return PR_MH_SUCCESS or error code
 */
NIMCP_EXPORT pr_mh_error_t pr_mental_health_bridge_track_retrieval_event(
    pr_mental_health_bridge_t bridge,
    const pr_mh_retrieval_event_t* event
);

//=============================================================================
// Rumination Detection Functions
//=============================================================================

/**
 * @brief Detect rumination patterns
 *
 * WHAT: Analyzes retrieval history for rumination indicators
 * WHY:  Rumination is a key marker for depression and anxiety
 * HOW:  Identifies repeated retrieval of negative memories
 *
 * @param bridge Mental health bridge
 * @param patterns Output array for detected patterns
 * @param max_patterns Maximum patterns to return
 * @param count Output: actual pattern count
 * @return PR_MH_SUCCESS or error code
 *
 * Performance: O(H) where H = history size
 *
 * DETECTION CRITERIA:
 * - Same memory retrieved >= rumination_threshold times
 * - Memory has negative valence
 * - Retrievals within rumination_window_ms
 * - Pattern shows obsessive characteristics (short intervals)
 */
NIMCP_EXPORT pr_mh_error_t pr_mental_health_bridge_detect_rumination(
    pr_mental_health_bridge_t bridge,
    pr_mh_rumination_pattern_t* patterns,
    size_t max_patterns,
    size_t* count
);

/**
 * @brief Check if specific memory shows rumination pattern
 *
 * @param bridge Mental health bridge
 * @param node_id Memory node to check
 * @param pattern Output: rumination pattern if detected
 * @return true if rumination detected, false otherwise
 *
 * Performance: O(H) where H = history size
 */
NIMCP_EXPORT bool pr_mental_health_bridge_is_ruminating_on(
    pr_mental_health_bridge_t bridge,
    uint64_t node_id,
    pr_mh_rumination_pattern_t* pattern
);

/**
 * @brief Get current rumination score
 *
 * WHAT: Computes overall rumination severity
 * WHY:  Single metric for rumination intensity
 *
 * @param bridge Mental health bridge
 * @return Rumination score [0-1], or -1 on error
 *
 * Performance: O(active_patterns)
 */
NIMCP_EXPORT float pr_mental_health_bridge_get_rumination_score(
    pr_mental_health_bridge_t bridge
);

//=============================================================================
// Intrusion Tracking Functions
//=============================================================================

/**
 * @brief Track intrusive memories
 *
 * WHAT: Records and analyzes intrusive memory events
 * WHY:  Intrusions are key PTSD and anxiety symptoms
 * HOW:  Maintains records of involuntary high-intensity recalls
 *
 * @param bridge Mental health bridge
 * @param node_id Intrusive memory node ID
 * @param intensity Intrusion intensity [0-1]
 * @param distress Associated distress level [0-1]
 * @return PR_MH_SUCCESS or error code
 *
 * Performance: O(1) for update, O(I) if new record
 *
 * SIDE EFFECTS:
 * - Updates intrusion tracking records
 * - May trigger intervention callback
 */
NIMCP_EXPORT pr_mh_error_t pr_mental_health_bridge_track_intrusion(
    pr_mental_health_bridge_t bridge,
    uint64_t node_id,
    float intensity,
    float distress
);

/**
 * @brief Get intrusion records
 *
 * @param bridge Mental health bridge
 * @param records Output array for intrusion records
 * @param max_records Maximum records to return
 * @param count Output: actual record count
 * @return PR_MH_SUCCESS or error code
 *
 * Performance: O(tracked_intrusions)
 */
NIMCP_EXPORT pr_mh_error_t pr_mental_health_bridge_get_intrusion_records(
    pr_mental_health_bridge_t bridge,
    pr_mh_intrusion_record_t* records,
    size_t max_records,
    size_t* count
);

/**
 * @brief Get intrusion frequency for specific memory
 *
 * @param bridge Mental health bridge
 * @param node_id Memory node ID
 * @return Intrusion frequency (per hour), or -1 if not tracked
 *
 * Performance: O(1) lookup
 */
NIMCP_EXPORT float pr_mental_health_bridge_get_intrusion_frequency(
    pr_mental_health_bridge_t bridge,
    uint64_t node_id
);

//=============================================================================
// Valence Bias Analysis Functions
//=============================================================================

/**
 * @brief Analyze memory valence bias
 *
 * WHAT: Computes valence distribution in recent retrievals
 * WHY:  Valence bias indicates mood state (depression, mania)
 * HOW:  Analyzes quaternion x (valence) components in retrieval history
 *
 * @param bridge Mental health bridge
 * @param bias Output: valence bias analysis result
 * @return PR_MH_SUCCESS or error code
 *
 * Performance: O(H) where H = analysis window entries
 *
 * BIAS INTERPRETATION:
 * - Score < -0.3: Strong negative bias (depression marker)
 * - Score in [-0.2, 0.2]: Healthy balanced retrieval
 * - Score > 0.3: Strong positive bias (possible mania)
 */
NIMCP_EXPORT pr_mh_error_t pr_mental_health_bridge_analyze_valence_bias(
    pr_mental_health_bridge_t bridge,
    pr_mh_valence_bias_t* bias
);

/**
 * @brief Get current valence bias score
 *
 * @param bridge Mental health bridge
 * @return Bias score [-1, +1], or NaN on error
 *
 * Performance: O(1) if recently computed, O(H) otherwise
 */
NIMCP_EXPORT float pr_mental_health_bridge_get_valence_bias(
    pr_mental_health_bridge_t bridge
);

/**
 * @brief Check if valence bias indicates concern
 *
 * @param bridge Mental health bridge
 * @param indicator Output: which indicator triggered (if any)
 * @return true if bias indicates mental health concern
 *
 * Performance: O(1)
 */
NIMCP_EXPORT bool pr_mental_health_bridge_valence_bias_concerning(
    pr_mental_health_bridge_t bridge,
    pr_mh_indicator_t* indicator
);

//=============================================================================
// Trauma Assessment Functions
//=============================================================================

/**
 * @brief Assess trauma memory load
 *
 * WHAT: Evaluates overall trauma memory burden
 * WHY:  High trauma load indicates need for intervention
 * HOW:  Combines trauma memory count, intrusion rates, and distress
 *
 * @param bridge Mental health bridge
 * @param assessment Output: trauma assessment result
 * @return PR_MH_SUCCESS or error code
 *
 * Performance: O(T) where T = trauma memory count
 *
 * ASSESSMENT FACTORS:
 * - Number of trauma/flashbulb memories
 * - Active intrusion frequency
 * - Avoidance behavior indicators
 * - Overall distress levels
 * - Hyperarousal markers
 */
NIMCP_EXPORT pr_mh_error_t pr_mental_health_bridge_assess_trauma_load(
    pr_mental_health_bridge_t bridge,
    pr_mh_trauma_assessment_t* assessment
);

/**
 * @brief Get current trauma load score
 *
 * @param bridge Mental health bridge
 * @return Trauma load [0-1], or -1 on error
 *
 * Performance: O(1) if recently computed
 */
NIMCP_EXPORT float pr_mental_health_bridge_get_trauma_load(
    pr_mental_health_bridge_t bridge
);

/**
 * @brief Mark memory as trauma-related
 *
 * @param bridge Mental health bridge
 * @param node_id Memory node to mark
 * @param distress_level Associated distress [0-1]
 * @return PR_MH_SUCCESS or error code
 */
NIMCP_EXPORT pr_mh_error_t pr_mental_health_bridge_mark_trauma_memory(
    pr_mental_health_bridge_t bridge,
    uint64_t node_id,
    float distress_level
);

//=============================================================================
// Mood Inference Functions
//=============================================================================

/**
 * @brief Infer mood from memory retrieval patterns
 *
 * WHAT: Derives mood state from recent memory patterns
 * WHY:  Memory patterns reflect and indicate mood state
 * HOW:  Combines valence bias, arousal levels, and pattern analysis
 *
 * @param bridge Mental health bridge
 * @param inference Output: mood inference result
 * @return PR_MH_SUCCESS or error code
 *
 * Performance: O(H) for history analysis
 *
 * MOOD INDICATORS:
 * - DEPRESSED: Strong negative bias, low arousal, rumination
 * - ANXIOUS: High arousal, threat-related retrievals, intrusions
 * - NEUTRAL: Balanced valence, moderate arousal
 * - POSITIVE: Positive bias, moderate-high arousal
 * - ELEVATED: Strong positive bias, high arousal (mania warning)
 * - MIXED: Contradictory indicators
 */
NIMCP_EXPORT pr_mh_error_t pr_mental_health_bridge_get_mood_from_memories(
    pr_mental_health_bridge_t bridge,
    pr_mh_mood_inference_t* inference
);

/**
 * @brief Get mood state as string
 *
 * @param mood Mood state
 * @return Human-readable mood name
 */
NIMCP_EXPORT const char* pr_mental_health_bridge_mood_name(
    pr_mh_mood_state_t mood
);

//=============================================================================
// Intervention Functions
//=============================================================================

/**
 * @brief Trigger therapeutic intervention
 *
 * WHAT: Initiates intervention based on detected indicators
 * WHY:  Provides hooks for therapeutic responses
 * HOW:  Calls registered callbacks with intervention request
 *
 * @param bridge Mental health bridge
 * @param type Intervention type to trigger
 * @param indicator What triggered the intervention
 * @param target_node_id Target memory (0 if not memory-specific)
 * @param severity Severity of trigger [0-1]
 * @return PR_MH_SUCCESS or error code
 *
 * Performance: O(callbacks)
 *
 * RATE LIMITING: Interventions are rate-limited by config
 */
NIMCP_EXPORT pr_mh_error_t pr_mental_health_bridge_trigger_intervention(
    pr_mental_health_bridge_t bridge,
    pr_mh_intervention_type_t type,
    pr_mh_indicator_t indicator,
    uint64_t target_node_id,
    float severity
);

/**
 * @brief Register intervention callback
 *
 * @param bridge Mental health bridge
 * @param callback Callback function
 * @param user_data User context
 * @return PR_MH_SUCCESS or error code
 */
NIMCP_EXPORT pr_mh_error_t pr_mental_health_bridge_register_intervention_callback(
    pr_mental_health_bridge_t bridge,
    pr_mh_intervention_callback_t callback,
    void* user_data
);

/**
 * @brief Get suggested intervention for current state
 *
 * WHAT: Recommends intervention based on current indicators
 * WHY:  Automated therapeutic suggestion
 *
 * @param bridge Mental health bridge
 * @param intervention Output: suggested intervention type
 * @param indicator Output: primary indicator driving suggestion
 * @return PR_MH_SUCCESS, PR_MH_ERROR_INSUFFICIENT_DATA, or error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT pr_mh_error_t pr_mental_health_bridge_suggest_intervention(
    pr_mental_health_bridge_t bridge,
    pr_mh_intervention_type_t* intervention,
    pr_mh_indicator_t* indicator
);

/**
 * @brief Get intervention type as string
 *
 * @param type Intervention type
 * @return Human-readable intervention name
 */
NIMCP_EXPORT const char* pr_mental_health_bridge_intervention_name(
    pr_mh_intervention_type_t type
);

//=============================================================================
// Update and Analysis Functions
//=============================================================================

/**
 * @brief Perform periodic update and analysis
 *
 * WHAT: Updates all tracking and analysis components
 * WHY:  Keeps mental health indicators current
 * HOW:  Runs rumination detection, intrusion tracking, bias analysis
 *
 * @param bridge Mental health bridge
 * @param current_time_ms Current timestamp
 * @return PR_MH_SUCCESS or error code
 *
 * Performance: O(H + I + R) where H=history, I=intrusions, R=ruminations
 *
 * Should be called periodically (e.g., every second or on retrieval batch)
 */
NIMCP_EXPORT pr_mh_error_t pr_mental_health_bridge_update(
    pr_mental_health_bridge_t bridge,
    uint64_t current_time_ms
);

/**
 * @brief Prune old history entries
 *
 * @param bridge Mental health bridge
 * @param max_age_ms Maximum age to keep
 * @return Number of entries pruned
 *
 * Performance: O(H)
 */
NIMCP_EXPORT size_t pr_mental_health_bridge_prune_history(
    pr_mental_health_bridge_t bridge,
    uint64_t max_age_ms
);

//=============================================================================
// Statistics and Queries
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Mental health bridge
 * @param stats Output: statistics structure
 * @return PR_MH_SUCCESS or error code
 *
 * Performance: O(1)
 */
NIMCP_EXPORT pr_mh_error_t pr_mental_health_bridge_get_stats(
    pr_mental_health_bridge_t bridge,
    pr_mh_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Mental health bridge
 * @return PR_MH_SUCCESS or error code
 */
NIMCP_EXPORT pr_mh_error_t pr_mental_health_bridge_reset_stats(
    pr_mental_health_bridge_t bridge
);

/**
 * @brief Get retrieval history count
 *
 * @param bridge Mental health bridge
 * @return Current history entry count
 */
NIMCP_EXPORT size_t pr_mental_health_bridge_get_history_count(
    pr_mental_health_bridge_t bridge
);

/**
 * @brief Export retrieval history
 *
 * @param bridge Mental health bridge
 * @param events Output array
 * @param max_events Maximum events to export
 * @param count Output: actual count
 * @return PR_MH_SUCCESS or error code
 */
NIMCP_EXPORT pr_mh_error_t pr_mental_health_bridge_export_history(
    pr_mental_health_bridge_t bridge,
    pr_mh_retrieval_event_t* events,
    size_t max_events,
    size_t* count
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error string
 *
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* pr_mental_health_bridge_error_string(
    pr_mh_error_t error
);

/**
 * @brief Get indicator name
 *
 * @param indicator Indicator type
 * @return Human-readable indicator name
 */
NIMCP_EXPORT const char* pr_mental_health_bridge_indicator_name(
    pr_mh_indicator_t indicator
);

/**
 * @brief Get current time in milliseconds
 *
 * @return Milliseconds since epoch
 */
NIMCP_EXPORT uint64_t pr_mental_health_bridge_current_time_ms(void);

/**
 * @brief Validate bridge internal state
 *
 * @param bridge Bridge to validate
 * @return true if consistent, false if issues detected
 */
NIMCP_EXPORT bool pr_mental_health_bridge_validate(
    pr_mental_health_bridge_t bridge
);

//=============================================================================
// Flashbulb Integration Functions
//=============================================================================

/**
 * @brief Connect flashbulb system for trauma tracking
 *
 * @param bridge Mental health bridge
 * @param flashbulb_system Flashbulb memory system
 * @return PR_MH_SUCCESS or error code
 */
NIMCP_EXPORT pr_mh_error_t pr_mental_health_bridge_connect_flashbulb(
    pr_mental_health_bridge_t bridge,
    flashbulb_system_t* flashbulb_system
);

/**
 * @brief Sync with flashbulb system
 *
 * Updates trauma tracking from flashbulb system state.
 *
 * @param bridge Mental health bridge
 * @return PR_MH_SUCCESS or error code
 */
NIMCP_EXPORT pr_mh_error_t pr_mental_health_bridge_sync_flashbulb(
    pr_mental_health_bridge_t bridge
);

//=============================================================================
// Inline Helper Functions
//=============================================================================

/**
 * @brief Check if mood state indicates concern
 *
 * @param mood Mood state
 * @return true if concerning (depressed, anxious, or elevated)
 */
static inline bool pr_mh_mood_is_concerning(pr_mh_mood_state_t mood) {
    return mood == PR_MH_MOOD_DEPRESSED ||
           mood == PR_MH_MOOD_ANXIOUS ||
           mood == PR_MH_MOOD_ELEVATED;
}

/**
 * @brief Check if intervention type is active (not NONE)
 *
 * @param type Intervention type
 * @return true if active intervention type
 */
static inline bool pr_mh_intervention_is_active(pr_mh_intervention_type_t type) {
    return type != PR_MH_INTERVENTION_NONE;
}

/**
 * @brief Classify valence as positive/negative/neutral
 *
 * @param valence Valence value [-1, +1]
 * @param threshold Threshold for classification (default 0.1)
 * @return -1 for negative, 0 for neutral, +1 for positive
 */
static inline int pr_mh_classify_valence(float valence, float threshold) {
    if (valence < -threshold) return -1;
    if (valence > threshold) return 1;
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PR_MENTAL_HEALTH_BRIDGE_H
