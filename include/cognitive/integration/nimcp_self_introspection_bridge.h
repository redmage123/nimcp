/**
 * @file nimcp_self_introspection_bridge.h
 * @brief Bridge between Self-Model and Introspection systems
 *
 * WHAT: Bidirectional integration where the self-model guides introspective queries
 *       and introspection results update the self-model.
 *
 * WHY: Self-awareness emerges from the interplay between a stable self-representation
 *      (self-model) and active self-examination (introspection). The self-model
 *      provides context for introspection; introspection refines the self-model.
 *
 * HOW: Self-model state influences what introspection examines and how results are
 *      interpreted. Introspection results trigger self-model updates and can
 *      initiate reflective processes when discrepancies are detected.
 *
 * BIOLOGICAL BASIS:
 * - Self-model relies on medial prefrontal cortex (mPFC) for self-referential processing
 * - Introspection engages anterior cingulate cortex (ACC) for monitoring
 * - Posterior cingulate cortex (PCC) integrates self-awareness
 * - Insula provides interoceptive self-knowledge
 * - Bidirectional connectivity enables self-model to guide metacognitive access
 *   and metacognitive insights to update self-representation
 *
 * Integration Pattern:
 * Self-Model -> Introspection:
 *   - Self-model state guides introspective query focus
 *   - Expectations from self-model shape introspection interpretation
 *   - Self-concept provides context for self-examination
 *
 * Introspection -> Self-Model:
 *   - Introspective insights update self-knowledge
 *   - Discrepancies trigger self-model revision
 *   - Metacognitive confidence calibrates self-model certainty
 *
 * @author NIMCP Development Team
 * @date 2025-01
 */

#ifndef NIMCP_SELF_INTROSPECTION_BRIDGE_H
#define NIMCP_SELF_INTROSPECTION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Opaque Type
 * ============================================================================ */

/**
 * @brief Opaque Self-Introspection bridge structure
 *
 * WHAT: Forward declaration for Self-Introspection bridge
 * WHY: Encapsulates implementation details
 * HOW: Full definition in implementation file
 */
typedef struct self_introspection_bridge self_introspection_bridge_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/**
 * @brief Default introspection depth level
 */
#define SELF_INTROSPECTION_DEFAULT_DEPTH 3

/**
 * @brief Default self-model update rate (updates per second)
 */
#define SELF_INTROSPECTION_DEFAULT_UPDATE_RATE 10.0f

/**
 * @brief Introspection query type identifiers
 */
typedef enum {
    SELF_INTROSPECTION_QUERY_STATE = 0,      /**< Query current mental state */
    SELF_INTROSPECTION_QUERY_CAPABILITY,     /**< Query own capabilities */
    SELF_INTROSPECTION_QUERY_KNOWLEDGE,      /**< Query knowledge boundaries */
    SELF_INTROSPECTION_QUERY_CONFIDENCE,     /**< Query confidence levels */
    SELF_INTROSPECTION_QUERY_EMOTION,        /**< Query emotional state */
    SELF_INTROSPECTION_QUERY_INTENTION,      /**< Query current intentions */
    SELF_INTROSPECTION_QUERY_BELIEF,         /**< Query belief states */
    SELF_INTROSPECTION_QUERY_MEMORY          /**< Query memory access */
} self_introspection_query_type_t;

/**
 * @brief Reflection trigger type identifiers
 */
typedef enum {
    SELF_INTROSPECTION_TRIGGER_DISCREPANCY = 0, /**< Self-model/observation mismatch */
    SELF_INTROSPECTION_TRIGGER_UNCERTAINTY,     /**< High uncertainty detected */
    SELF_INTROSPECTION_TRIGGER_ERROR,           /**< Error or failure detected */
    SELF_INTROSPECTION_TRIGGER_NOVELTY,         /**< Novel situation encountered */
    SELF_INTROSPECTION_TRIGGER_SCHEDULED,       /**< Periodic self-reflection */
    SELF_INTROSPECTION_TRIGGER_EXTERNAL         /**< External request for reflection */
} self_introspection_trigger_type_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Configuration for Self-Introspection bridge
 *
 * WHAT: Parameters controlling self-model/introspection integration
 *
 * WHY: Different scenarios require different balances between self-model
 *      stability and introspective updating
 *
 * HOW: Configure depth, update rate, and reflection thresholds
 */
typedef struct {
    /** Maximum depth of introspective analysis (default: 3)
     *  Higher values enable more recursive self-examination */
    uint32_t introspection_depth;

    /** Rate at which self-model accepts introspective updates [0-1] (default: 0.3)
     *  Lower values provide more stable self-model */
    float self_model_update_rate;

    /** Threshold for triggering reflection [0-1] (default: 0.7)
     *  Lower values trigger reflection more easily */
    float reflection_threshold;

    /** Enable automatic reflection on discrepancy detection */
    bool enable_auto_reflection;

    /** Enable self-model guidance of introspection focus */
    bool enable_guided_introspection;

    /** Minimum confidence for self-model updates [0-1] */
    float min_update_confidence;

    /** Maximum reflection depth (prevents infinite recursion) */
    uint32_t max_reflection_depth;
} self_introspection_config_t;

/**
 * @brief Self-model guidance for introspection
 *
 * WHAT: Information from self-model that guides introspective queries
 *
 * WHY: Introspection should be guided by current self-understanding
 *
 * HOW: Contains focus areas, expectations, and context for introspection
 */
typedef struct {
    /** Suggested focus area for introspection */
    self_introspection_query_type_t suggested_focus;

    /** Expected value/state (for discrepancy detection) */
    float expected_value;

    /** Confidence in expectation [0-1] */
    float expectation_confidence;

    /** Current self-model stability [0-1] */
    float self_model_stability;

    /** Priority of this introspection [0-1] */
    float priority;

    /** Context identifier for this guidance */
    uint32_t context_id;
} self_introspection_guidance_t;

/**
 * @brief Introspection result data
 *
 * WHAT: Result from an introspective query
 *
 * WHY: Introspection results need to be communicated to self-model
 *
 * HOW: Contains query result, confidence, and metadata
 */
typedef struct {
    /** Query ID this result corresponds to */
    uint32_t query_id;

    /** Type of query that was performed */
    self_introspection_query_type_t query_type;

    /** Result value (interpretation depends on query_type) */
    float result_value;

    /** Confidence in result [0-1] */
    float confidence;

    /** Discrepancy from expectation (if applicable) */
    float discrepancy;

    /** Time taken for introspection (ms) */
    uint32_t processing_time_ms;

    /** Whether result suggests self-model update */
    bool suggests_update;
} self_introspection_result_t;

/**
 * @brief Current self-model state
 *
 * WHAT: Summary of current self-model state
 *
 * WHY: Enables querying overall self-model status
 *
 * HOW: Aggregates key self-model metrics
 */
typedef struct {
    /** Overall self-model coherence [0-1] */
    float coherence;

    /** Self-model stability over time [0-1] */
    float stability;

    /** Confidence in self-knowledge [0-1] */
    float confidence;

    /** Number of active self-beliefs */
    uint32_t belief_count;

    /** Number of pending updates */
    uint32_t pending_updates;

    /** Time since last major revision (ms) */
    uint64_t time_since_revision;

    /** Is self-model in stable state */
    bool is_stable;

    /** Is reflection currently active */
    bool reflection_active;
} self_introspection_self_state_t;

/**
 * @brief Statistics for Self-Introspection bridge
 *
 * WHAT: Performance and activity metrics for the bridge
 *
 * WHY: Monitor integration health and self-model dynamics
 *
 * HOW: Accumulates counts during bridge operation
 */
typedef struct {
    /** Number of introspective queries guided by self-model */
    uint64_t queries_guided;

    /** Number of introspection results integrated into self-model */
    uint64_t results_integrated;

    /** Number of self-reflections triggered */
    uint64_t reflections_triggered;

    /** Number of self-model updates performed */
    uint64_t self_model_updates;

    /** Number of discrepancies detected */
    uint64_t discrepancies_detected;

    /** Average introspection confidence [0-1] */
    float avg_introspection_confidence;

    /** Average discrepancy magnitude */
    float avg_discrepancy;

    /** Number of reflection failures */
    uint64_t reflection_failures;
} self_introspection_stats_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Initialize default Self-Introspection configuration
 *
 * WHAT: Sets default parameters for Self-Introspection bridge
 * WHY: Provides sensible defaults for typical use cases
 * HOW: Initializes config with balanced parameters
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int self_introspection_default_config(self_introspection_config_t* config);

/**
 * @brief Create Self-Introspection bridge
 *
 * WHAT: Allocates and initializes Self-Introspection integration bridge
 * WHY: Establishes bidirectional link between self-model and introspection
 * HOW: Creates bridge, initializes state tracking, sets up reflection pipeline
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Pointer to created bridge, NULL on failure
 */
self_introspection_bridge_t* self_introspection_bridge_create(
    const self_introspection_config_t* config
);

/**
 * @brief Destroy Self-Introspection bridge
 *
 * WHAT: Cleans up and deallocates bridge
 * WHY: Prevents memory leaks and releases resources
 * HOW: Clears state, frees memory, deallocates bridge
 *
 * @param bridge Bridge to destroy
 */
void self_introspection_bridge_destroy(self_introspection_bridge_t* bridge);

/**
 * @brief Get self-model guidance for introspection
 *
 * WHAT: Self-model provides guidance for introspective query
 * WHY: Introspection should be informed by current self-understanding
 * HOW: Queries self-model for focus areas and expectations
 *
 * @param bridge Bridge instance
 * @param query_type Type of introspective query being planned
 * @param guidance_out Output buffer for self-model guidance
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL BASIS: mPFC self-representation provides top-down guidance
 *                   for ACC metacognitive monitoring processes.
 */
int self_introspection_guide_query(
    self_introspection_bridge_t* bridge,
    self_introspection_query_type_t query_type,
    self_introspection_guidance_t* guidance_out
);

/**
 * @brief Process introspection result to update self-model
 *
 * WHAT: Introspection result is integrated into self-model
 * WHY: Introspective insights should update self-knowledge
 * HOW: Evaluates result, detects discrepancies, updates self-model
 *
 * @param bridge Bridge instance
 * @param query_id ID of the completed query
 * @param result_data Introspection result to integrate
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL BASIS: ACC monitoring outputs update mPFC self-representation
 *                   through PCC integration processes.
 */
int self_introspection_on_result(
    self_introspection_bridge_t* bridge,
    uint32_t query_id,
    const self_introspection_result_t* result_data
);

/**
 * @brief Trigger self-reflection process
 *
 * WHAT: Initiates a self-reflection episode
 * WHY: Certain conditions require deeper self-examination
 * HOW: Activates reflection pipeline with specified trigger type
 *
 * @param bridge Bridge instance
 * @param trigger_type Type of trigger initiating reflection
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL BASIS: Discrepancies or uncertainty activate ACC/PCC
 *                   reflection networks for self-model revision.
 */
int self_introspection_trigger_reflection(
    self_introspection_bridge_t* bridge,
    self_introspection_trigger_type_t trigger_type
);

/**
 * @brief Get current self-model state
 *
 * WHAT: Retrieves summary of current self-model state
 * WHY: Enables monitoring of self-model health and stability
 * HOW: Copies current state metrics to output buffer
 *
 * @param bridge Bridge instance
 * @param state_out Output buffer for self-model state
 * @return 0 on success, -1 on error
 */
int self_introspection_get_self_state(
    self_introspection_bridge_t* bridge,
    self_introspection_self_state_t* state_out
);

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieves performance and activity metrics
 * WHY: Monitor bridge health and self-model dynamics
 * HOW: Copies current statistics to output buffer
 *
 * @param bridge Bridge instance
 * @param stats_out Output buffer for statistics
 * @return 0 on success, -1 on error
 */
int self_introspection_get_stats(
    const self_introspection_bridge_t* bridge,
    self_introspection_stats_t* stats_out
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SELF_INTROSPECTION_BRIDGE_H */
