/* ============================================================================
 * [TOMBSTONE] DEPRECATED — proposed design, never implemented.
 *
 * This header declares a bridge API whose .c implementation was never written.
 * Any code that #includes this file and calls its functions will fail at link.
 * Preserved as a design record only; do NOT add new uses.
 *
 * Status: FULL-STATUE in the 2026-04-24 consumer-bridge audit. Ghost-typedef
 * bridges like this describe cross-module couplings that were sketched but
 * never implemented.
 *
 * To revive: write the backing .c file, add it to the appropriate CMakeLists,
 * then remove this banner and validate with the `_update`/`_create` caller
 * chain ending somewhere in a hot path. See
 *   docs/claude/consumer-bridge-inventory-2026-04-24.md
 * for the full inventory + the middle-path rationale for why this is
 * tombstoned rather than deleted or implemented.
 * ========================================================================= */

//=============================================================================
// nimcp_neurogenesis_hub_bridge.h - Neurogenesis to Cognitive Hub Bridge
//=============================================================================
/**
 * @file nimcp_neurogenesis_hub_bridge.h
 * @brief Bridge between neurogenesis and cognitive hub systems
 *
 * WHAT: Connects neurogenesis with the cognitive hub (global workspace),
 *       enabling new neurons to participate in conscious processing and
 *       higher cognitive functions.
 *
 * WHY:  Bridges the gap between:
 *       - Neurogenesis (new neuron development in hippocampus)
 *       - Cognitive hub (global workspace, memory consolidation)
 *       - Pattern separation and completion in memory systems
 *
 * HOW:  Bidirectional integration:
 *       1. Neurogenesis -> Hub: New neurons add pattern separation capacity
 *       2. Hub -> Neurogenesis: Cognitive demands modulate proliferation
 *       3. Memory encoding recruits new neurons
 *       4. Pattern separation quality guides survival
 *
 * BIOLOGICAL BASIS:
 * ```
 * NEUROGENESIS                          COGNITIVE HUB
 * -----------------------------------------------------------------
 * Hippocampal DG neurogenesis        -> Enhanced pattern separation
 * New neuron integration             -> Memory encoding support
 * Activity-dependent survival        <- Task engagement signals
 * Sparse coding by new neurons       -> Orthogonal representations
 * Temporal context sensitivity       -> Episode differentiation
 * Learning-driven proliferation      <- Cognitive challenge signals
 * ```
 *
 * COGNITIVE FUNCTIONS OF NEW NEURONS:
 * - Pattern separation: Distinguish similar experiences
 * - Temporal tagging: Mark memories with temporal context
 * - Forgetting: Enable memory turnover and updating
 * - Flexibility: Support cognitive flexibility
 * - Contextual binding: Link features to contexts
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_NEUROGENESIS_HUB_BRIDGE_H
#define NIMCP_NEUROGENESIS_HUB_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define NEUROGENESIS_HUB_MODULE_NAME            "neurogenesis_hub_bridge"

/** Maximum tracked memories */
#define NEUROGENESIS_HUB_MAX_MEMORIES           512

/** Maximum pattern vectors */
#define NEUROGENESIS_HUB_MAX_PATTERNS           256

/** Default pattern separation threshold */
#define NEUROGENESIS_HUB_SEPARATION_THRESH      0.3f

/** Default cognitive demand threshold for proliferation */
#define NEUROGENESIS_HUB_COGNITIVE_THRESH       0.5f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Cognitive task type affecting neurogenesis
 */
typedef enum {
    NG_HUB_TASK_NONE = 0,            /**< No active task */
    NG_HUB_TASK_ENCODING,            /**< Memory encoding */
    NG_HUB_TASK_RETRIEVAL,           /**< Memory retrieval */
    NG_HUB_TASK_DISCRIMINATION,      /**< Pattern discrimination */
    NG_HUB_TASK_NAVIGATION,          /**< Spatial navigation */
    NG_HUB_TASK_CONTEXTUAL,          /**< Contextual learning */
    NG_HUB_TASK_FLEXIBILITY          /**< Cognitive flexibility */
} ng_hub_task_t;

/**
 * @brief New neuron cognitive role
 */
typedef enum {
    NG_HUB_ROLE_SEPARATOR = 0,       /**< Pattern separation */
    NG_HUB_ROLE_TEMPORAL,            /**< Temporal tagging */
    NG_HUB_ROLE_CONTEXTUAL,          /**< Contextual binding */
    NG_HUB_ROLE_INTEGRATOR,          /**< Multi-modal integration */
    NG_HUB_ROLE_UNASSIGNED           /**< Not yet assigned */
} ng_hub_neuron_role_t;

/**
 * @brief Memory encoding quality
 */
typedef enum {
    NG_HUB_ENCODE_FAILED = 0,        /**< Encoding failed */
    NG_HUB_ENCODE_WEAK,              /**< Weak encoding */
    NG_HUB_ENCODE_MODERATE,          /**< Moderate encoding */
    NG_HUB_ENCODE_STRONG,            /**< Strong encoding */
    NG_HUB_ENCODE_EXCEPTIONAL        /**< Exceptional (flashbulb) */
} ng_hub_encode_quality_t;

/**
 * @brief Workspace access status
 */
typedef enum {
    NG_HUB_ACCESS_NONE = 0,          /**< No workspace access */
    NG_HUB_ACCESS_PERIPHERAL,        /**< Peripheral access */
    NG_HUB_ACCESS_BROADCAST,         /**< Full broadcast access */
    NG_HUB_ACCESS_INTEGRATED         /**< Fully integrated */
} ng_hub_access_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for neurogenesis-hub bridge
 */
typedef struct {
    /** Pattern separation parameters */
    float separation_threshold;              /**< Min separation distance */
    float separation_contribution_weight;    /**< New neuron contribution */
    uint32_t max_patterns_per_neuron;        /**< Max patterns per new neuron */
    bool enable_sparse_coding;               /**< Enforce sparse activation */

    /** Cognitive demand coupling */
    float cognitive_demand_threshold;        /**< Demand for proliferation */
    float task_engagement_weight;            /**< Task engagement importance */
    float novelty_weight;                    /**< Novelty importance */
    float challenge_weight;                  /**< Difficulty importance */

    /** Memory encoding */
    float encoding_recruitment_threshold;    /**< When to recruit new neurons */
    float temporal_context_window;           /**< Temporal context duration */
    bool enable_temporal_tagging;            /**< Use temporal context */

    /** Survival coupling */
    float separation_survival_weight;        /**< Separation affects survival */
    float encoding_survival_weight;          /**< Encoding affects survival */
    float cognitive_utility_threshold;       /**< Min utility for survival */

    /** Workspace integration */
    float workspace_access_threshold;        /**< Threshold for workspace */
    float integration_rate;                  /**< Rate of hub integration */
    bool enable_consciousness_gating;        /**< Conscious access affects survival */

    /** Update parameters */
    float update_interval_ms;                /**< Bridge update interval */
    bool enable_logging;
    bool enable_metrics;
} ng_hub_config_t;

/**
 * @brief Pattern representation for separation
 */
typedef struct {
    uint32_t pattern_id;                     /**< Pattern identifier */
    float* features;                         /**< Feature vector */
    uint32_t feature_dim;                    /**< Vector dimension */
    float sparsity;                          /**< Activation sparsity */
    uint64_t encoding_time;                  /**< When encoded */
    float temporal_context;                  /**< Temporal context value */
} ng_hub_pattern_t;

/**
 * @brief Cognitive task state
 */
typedef struct {
    ng_hub_task_t task_type;                 /**< Current task */
    float engagement_level;                  /**< Task engagement (0-1) */
    float novelty_level;                     /**< Task novelty (0-1) */
    float difficulty_level;                  /**< Task difficulty (0-1) */
    float cognitive_demand;                  /**< Overall cognitive demand */
    uint64_t task_start_time;                /**< When task started */
    bool is_learning_phase;                  /**< In learning vs retrieval */
} ng_hub_task_state_t;

/**
 * @brief New neuron cognitive contribution
 */
typedef struct {
    uint32_t neuron_id;                      /**< Neuron identifier */
    ng_hub_neuron_role_t role;               /**< Assigned cognitive role */
    ng_hub_access_t workspace_access;        /**< Workspace access level */
    float separation_contribution;           /**< Pattern separation contrib */
    float encoding_participation;            /**< Memory encoding participation */
    float temporal_specificity;              /**< Temporal context specificity */
    float cognitive_utility;                 /**< Overall cognitive utility */
    uint32_t patterns_encoded;               /**< Patterns this neuron encoded */
    float integration_progress;              /**< Hub integration progress */
} ng_hub_neuron_state_t;

/**
 * @brief Memory encoding event involving new neurons
 */
typedef struct {
    uint32_t memory_id;                      /**< Memory identifier */
    uint32_t* participating_neurons;         /**< New neurons involved */
    uint32_t neuron_count;                   /**< Number of new neurons */
    ng_hub_encode_quality_t quality;         /**< Encoding quality */
    float pattern_separation;                /**< Separation from similar */
    float temporal_context;                  /**< Temporal context value */
    uint64_t encoding_time;                  /**< When encoded */
    bool is_novel;                           /**< Novel vs familiar */
} ng_hub_encoding_event_t;

/**
 * @brief Pattern separation result
 */
typedef struct {
    uint32_t pattern_a;                      /**< First pattern ID */
    uint32_t pattern_b;                      /**< Second pattern ID */
    float input_similarity;                  /**< Input similarity */
    float output_separation;                 /**< Output separation achieved */
    float new_neuron_contribution;           /**< New neuron contribution */
    bool discrimination_success;             /**< Successful discrimination */
} ng_hub_separation_result_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t patterns_separated;             /**< Patterns successfully separated */
    uint64_t encoding_events;                /**< Memory encoding events */
    uint64_t retrieval_events;               /**< Retrieval events */
    uint64_t neurons_recruited;              /**< Neurons recruited for encoding */
    uint64_t workspace_broadcasts;           /**< Broadcasts to workspace */
    float avg_separation_quality;            /**< Average separation quality */
    float avg_encoding_quality;              /**< Average encoding quality */
    float avg_cognitive_utility;             /**< Average new neuron utility */
    float current_cognitive_demand;          /**< Current cognitive demand */
    uint32_t neurons_with_access;            /**< Neurons with workspace access */
    uint64_t update_count;                   /**< Total updates */
    float last_update_ms;                    /**< Last update timestamp */
} ng_hub_stats_t;

/** Opaque bridge handle */
typedef struct ng_hub_bridge_struct ng_hub_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_hub_default_config(ng_hub_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create neurogenesis-hub bridge
 *
 * WHAT: Creates bridge for cognitive integration of new neurons
 * WHY:  Enable new neurons to contribute to cognition
 * HOW:  Manages pattern separation, encoding, workspace access
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT ng_hub_bridge_t* ng_hub_bridge_create(
    const ng_hub_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void ng_hub_bridge_destroy(ng_hub_bridge_t* bridge);

//=============================================================================
// Cognitive Task API
//=============================================================================

/**
 * @brief Set current cognitive task
 *
 * WHAT: Updates active cognitive task
 * WHY:  Task type modulates neurogenesis
 * HOW:  Sets task parameters, updates demand
 *
 * @param bridge Bridge handle
 * @param task Task type
 * @param engagement Engagement level (0-1)
 * @param novelty Novelty level (0-1)
 * @param difficulty Difficulty level (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_hub_set_task(
    ng_hub_bridge_t* bridge,
    ng_hub_task_t task,
    float engagement,
    float novelty,
    float difficulty
);

/**
 * @brief Get current task state
 *
 * @param bridge Bridge handle
 * @param state Output state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_hub_get_task_state(
    const ng_hub_bridge_t* bridge,
    ng_hub_task_state_t* state
);

/**
 * @brief Get cognitive demand for proliferation
 *
 * WHAT: Returns current cognitive demand level
 * WHY:  High demand increases neurogenesis
 * HOW:  Aggregates task, novelty, challenge signals
 *
 * @param bridge Bridge handle
 * @return Cognitive demand (0-1)
 */
NIMCP_EXPORT float ng_hub_get_cognitive_demand(
    const ng_hub_bridge_t* bridge
);

/**
 * @brief Check if proliferation is cognitively warranted
 *
 * @param bridge Bridge handle
 * @return true if cognitive state supports proliferation
 */
NIMCP_EXPORT bool ng_hub_should_proliferate(
    const ng_hub_bridge_t* bridge
);

//=============================================================================
// Pattern Separation API
//=============================================================================

/**
 * @brief Register pattern for separation tracking
 *
 * WHAT: Adds pattern to separation system
 * WHY:  Track pattern separation quality
 * HOW:  Stores pattern, assigns new neurons
 *
 * @param bridge Bridge handle
 * @param pattern Pattern data
 * @param pattern_id Output pattern ID
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_hub_register_pattern(
    ng_hub_bridge_t* bridge,
    const ng_hub_pattern_t* pattern,
    uint32_t* pattern_id
);

/**
 * @brief Compute pattern separation
 *
 * WHAT: Calculates separation between two patterns
 * WHY:  Measure pattern discrimination quality
 * HOW:  Compares representations, tracks new neuron contribution
 *
 * @param bridge Bridge handle
 * @param pattern_a First pattern ID
 * @param pattern_b Second pattern ID
 * @param result Output separation result
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_hub_compute_separation(
    ng_hub_bridge_t* bridge,
    uint32_t pattern_a,
    uint32_t pattern_b,
    ng_hub_separation_result_t* result
);

/**
 * @brief Recruit neuron for pattern
 *
 * WHAT: Assigns new neuron to pattern representation
 * WHY:  New neurons enhance pattern separation
 * HOW:  Links neuron to pattern, updates role
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron to recruit
 * @param pattern_id Target pattern
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_hub_recruit_for_pattern(
    ng_hub_bridge_t* bridge,
    uint32_t neuron_id,
    uint32_t pattern_id
);

//=============================================================================
// Memory Encoding API
//=============================================================================

/**
 * @brief Initiate memory encoding with new neurons
 *
 * WHAT: Starts encoding event recruiting new neurons
 * WHY:  New neurons participate in memory formation
 * HOW:  Selects appropriate new neurons, begins encoding
 *
 * @param bridge Bridge handle
 * @param pattern_id Pattern to encode
 * @param is_novel Whether pattern is novel
 * @param event Output encoding event
 * @return Number of neurons recruited
 */
NIMCP_EXPORT int ng_hub_encode_memory(
    ng_hub_bridge_t* bridge,
    uint32_t pattern_id,
    bool is_novel,
    ng_hub_encoding_event_t* event
);

/**
 * @brief Report encoding outcome
 *
 * WHAT: Reports quality of memory encoding
 * WHY:  Encoding success affects neuron survival
 * HOW:  Updates neuron contribution metrics
 *
 * @param bridge Bridge handle
 * @param memory_id Memory identifier
 * @param quality Encoding quality achieved
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_hub_report_encoding(
    ng_hub_bridge_t* bridge,
    uint32_t memory_id,
    ng_hub_encode_quality_t quality
);

/**
 * @brief Set temporal context
 *
 * WHAT: Updates temporal context for encoding
 * WHY:  New neurons tag memories with time
 * HOW:  Sets context value for temporal tagging
 *
 * @param bridge Bridge handle
 * @param context Temporal context value
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_hub_set_temporal_context(
    ng_hub_bridge_t* bridge,
    float context
);

//=============================================================================
// Neuron State API
//=============================================================================

/**
 * @brief Register neuron for cognitive hub
 *
 * WHAT: Adds new neuron to hub system
 * WHY:  Track cognitive contribution
 * HOW:  Creates state entry, assigns initial role
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @param role Initial cognitive role
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_hub_register_neuron(
    ng_hub_bridge_t* bridge,
    uint32_t neuron_id,
    ng_hub_neuron_role_t role
);

/**
 * @brief Get neuron cognitive state
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @param state Output state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_hub_get_neuron_state(
    const ng_hub_bridge_t* bridge,
    uint32_t neuron_id,
    ng_hub_neuron_state_t* state
);

/**
 * @brief Get cognitive utility for survival
 *
 * WHAT: Returns neuron's cognitive utility score
 * WHY:  Cognitive contribution affects survival
 * HOW:  Aggregates separation, encoding, utility metrics
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @return Cognitive utility (0-1)
 */
NIMCP_EXPORT float ng_hub_get_cognitive_utility(
    const ng_hub_bridge_t* bridge,
    uint32_t neuron_id
);

/**
 * @brief Update workspace access for neuron
 *
 * WHAT: Progress neuron toward workspace integration
 * WHY:  Workspace access is a survival criterion
 * HOW:  Based on cognitive contribution and activity
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @param contribution Recent cognitive contribution
 * @return New access level
 */
NIMCP_EXPORT ng_hub_access_t ng_hub_update_access(
    ng_hub_bridge_t* bridge,
    uint32_t neuron_id,
    float contribution
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Update demands, progress integration, compute utilities
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_hub_update(
    ng_hub_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_hub_reset(ng_hub_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_hub_get_stats(
    const ng_hub_bridge_t* bridge,
    ng_hub_stats_t* stats
);

/**
 * @brief Get neurons by cognitive role
 *
 * @param bridge Bridge handle
 * @param role Role to query
 * @param ids Output array
 * @param max_ids Maximum IDs
 * @return Number of neurons returned
 */
NIMCP_EXPORT int ng_hub_get_neurons_by_role(
    const ng_hub_bridge_t* bridge,
    ng_hub_neuron_role_t role,
    uint32_t* ids,
    uint32_t max_ids
);

/**
 * @brief Get neurons with workspace access
 *
 * @param bridge Bridge handle
 * @param access Minimum access level
 * @param ids Output array
 * @param max_ids Maximum IDs
 * @return Number of neurons returned
 */
NIMCP_EXPORT int ng_hub_get_neurons_with_access(
    const ng_hub_bridge_t* bridge,
    ng_hub_access_t access,
    uint32_t* ids,
    uint32_t max_ids
);

/**
 * @brief Get task name string
 *
 * @param task Task type
 * @return Task name
 */
NIMCP_EXPORT const char* ng_hub_task_name(ng_hub_task_t task);

/**
 * @brief Get role name string
 *
 * @param role Neuron role
 * @return Role name
 */
NIMCP_EXPORT const char* ng_hub_role_name(ng_hub_neuron_role_t role);

/**
 * @brief Get access level name string
 *
 * @param access Access level
 * @return Access name
 */
NIMCP_EXPORT const char* ng_hub_access_name(ng_hub_access_t access);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROGENESIS_HUB_BRIDGE_H */