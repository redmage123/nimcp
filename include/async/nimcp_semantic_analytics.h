/**
 * @file nimcp_semantic_analytics.h
 * @brief Analytics for semantic primitive usage
 *
 * WHAT: Track and analyze semantic primitive usage patterns
 * WHY:  Optimize primitive dictionary, identify common patterns
 * HOW:  Usage tracking -> pattern analysis -> recommendations
 *
 * ARCHITECTURE:
 * ┌───────────────────────────────────────────────────────────────┐
 * │              SEMANTIC PRIMITIVE ANALYTICS                      │
 * ├───────────────────────────────────────────────────────────────┤
 * │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐        │
 * │  │   Usage      │  │ Co-occurrence│  │   Pattern    │        │
 * │  │   Tracking   │  │   Matrix     │  │  Detection   │        │
 * │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘        │
 * │         │                 │                 │                 │
 * │         └─────────────────┴─────────────────┘                 │
 * │                           │                                   │
 * │                 ┌─────────▼─────────┐                         │
 * │                 │  Pattern Analysis │                         │
 * │                 │  (frequent itemsets)                        │
 * │                 └─────────┬─────────┘                         │
 * │                           │                                   │
 * │                 ┌─────────▼─────────┐                         │
 * │                 │  Recommendations  │                         │
 * │                 │  (optimize dict)  │                         │
 * │                 └───────────────────┘                         │
 * └───────────────────────────────────────────────────────────────┘
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#ifndef NIMCP_SEMANTIC_ANALYTICS_H
#define NIMCP_SEMANTIC_ANALYTICS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct semantic_analytics_struct* semantic_analytics_t;

//=============================================================================
// Constants
//=============================================================================

#define MAX_PRIMITIVE_NAME 64
#define MAX_CONTEXT_LEN 256
#define MAX_PATTERN_SIZE 8
#define MAX_RECOMMENDATIONS 32

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Primitive usage statistics
 */
typedef struct {
    uint32_t primitive_id;            /**< Primitive identifier */
    char primitive_name[MAX_PRIMITIVE_NAME]; /**< Primitive name */
    uint64_t usage_count;             /**< Total times used */
    uint64_t last_used_ms;            /**< Last usage timestamp */
    float avg_confidence;             /**< Average confidence when used */
    char* contexts[32];               /**< Recent contexts (circular buffer) */
    uint32_t context_count;           /**< Number of stored contexts */
    uint32_t context_index;           /**< Current context index */
    float* co_occurrence_scores;      /**< Scores with other primitives */
    uint32_t co_occurrence_count;     /**< Number of co-occurrences tracked */
} primitive_usage_t;

/**
 * @brief Usage pattern (frequent itemset)
 */
typedef struct {
    uint32_t primitives[MAX_PATTERN_SIZE]; /**< Primitive IDs in pattern */
    uint32_t primitive_count;         /**< Number of primitives */
    uint64_t frequency;               /**< How often this pattern occurs */
    float confidence;                 /**< Pattern confidence (0-1) */
    char typical_context[MAX_CONTEXT_LEN]; /**< Representative context */
    uint64_t last_seen_ms;            /**< Last occurrence timestamp */
} usage_pattern_t;

/**
 * @brief Primitive recommendation
 */
typedef struct {
    uint32_t primitive_id;            /**< Recommended primitive */
    char primitive_name[MAX_PRIMITIVE_NAME];
    float score;                      /**< Recommendation score (0-1) */
    char reason[128];                 /**< Why recommended */
    usage_pattern_t* supporting_patterns; /**< Patterns supporting this */
    uint32_t pattern_count;           /**< Number of supporting patterns */
} primitive_recommendation_t;

/**
 * @brief Analytics report
 */
typedef struct {
    uint64_t total_primitives_tracked; /**< Total primitives */
    uint64_t total_usage_events;      /**< Total usage events */
    uint64_t patterns_detected;       /**< Number of patterns found */
    primitive_usage_t* top_primitives; /**< Most used primitives */
    uint32_t top_primitive_count;     /**< Number in top list */
    primitive_usage_t* rare_primitives; /**< Rarely used primitives */
    uint32_t rare_primitive_count;    /**< Number in rare list */
    usage_pattern_t* common_patterns; /**< Most common patterns */
    uint32_t common_pattern_count;    /**< Number of common patterns */
    float avg_primitives_per_message; /**< Average primitives per message */
    float dictionary_efficiency;      /**< Usage efficiency (0-1) */
} analytics_report_t;

/**
 * @brief Analytics configuration
 */
typedef struct {
    uint32_t max_tracked_primitives;  /**< Maximum primitives to track */
    uint32_t context_history_size;    /**< Contexts to remember per primitive */
    float min_pattern_support;        /**< Minimum support for patterns (0-1) */
    float min_pattern_confidence;     /**< Minimum confidence for patterns */
    uint32_t max_patterns;            /**< Maximum patterns to detect */
    bool enable_co_occurrence;        /**< Track co-occurrence matrix */
    bool enable_recommendations;      /**< Generate recommendations */
    uint32_t cleanup_interval_ms;     /**< How often to cleanup old data */
} analytics_config_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create semantic analytics instance
 *
 * WHAT: Initialize analytics tracking system
 * WHY:  Monitor primitive usage patterns
 * HOW:  Allocate storage, setup tracking
 *
 * @param config Configuration (NULL for defaults)
 * @return Analytics handle or NULL on failure
 */
semantic_analytics_t semantic_analytics_create(const analytics_config_t* config);

/**
 * @brief Destroy analytics instance
 *
 * WHAT: Clean up analytics resources
 * WHY:  Prevent memory leaks
 * HOW:  Free all tracking data
 *
 * @param analytics Analytics handle
 */
void semantic_analytics_destroy(semantic_analytics_t analytics);

/**
 * @brief Get default analytics configuration
 *
 * @return Default configuration
 */
analytics_config_t semantic_analytics_default_config(void);

//=============================================================================
// Recording API
//=============================================================================

/**
 * @brief Record primitive usage
 *
 * WHAT: Record that primitive was used in context
 * WHY:  Track usage patterns
 * HOW:  Update usage stats, context history
 *
 * @param analytics Analytics handle
 * @param primitive_id Primitive identifier
 * @param primitive_name Primitive name (for lookup)
 * @param context Context string where used
 * @param confidence Usage confidence (0-1)
 * @return true on success, false on failure
 */
bool semantic_analytics_record(
    semantic_analytics_t analytics,
    uint32_t primitive_id,
    const char* primitive_name,
    const char* context,
    float confidence
);

/**
 * @brief Record co-occurrence of primitives
 *
 * WHAT: Record that primitives appeared together
 * WHY:  Build co-occurrence matrix
 * HOW:  Update pairwise scores
 *
 * @param analytics Analytics handle
 * @param primitive_ids Array of primitive IDs
 * @param primitive_count Number of primitives
 * @param context Shared context
 * @return true on success, false on failure
 */
bool semantic_analytics_record_group(
    semantic_analytics_t analytics,
    const uint32_t* primitive_ids,
    uint32_t primitive_count,
    const char* context
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get usage statistics for primitive
 *
 * WHAT: Retrieve usage stats for specific primitive
 * WHY:  Analyze individual primitive usage
 * HOW:  Hash table lookup
 *
 * @param analytics Analytics handle
 * @param primitive_id Primitive ID
 * @param out_usage Output usage structure
 * @return true if found, false otherwise
 */
bool semantic_analytics_get_usage(
    semantic_analytics_t analytics,
    uint32_t primitive_id,
    primitive_usage_t* out_usage
);

/**
 * @brief Find common usage patterns
 *
 * WHAT: Detect frequent primitive combinations
 * WHY:  Identify common communication patterns
 * HOW:  Frequent itemset mining (Apriori-like)
 *
 * @param analytics Analytics handle
 * @param out_patterns Output pattern array
 * @param max_patterns Maximum patterns to return
 * @param out_count Number of patterns found
 * @return true on success, false on failure
 */
bool semantic_analytics_find_patterns(
    semantic_analytics_t analytics,
    usage_pattern_t* out_patterns,
    uint32_t max_patterns,
    uint32_t* out_count
);

/**
 * @brief Generate analytics report
 *
 * WHAT: Generate comprehensive analytics report
 * WHY:  Overview of primitive usage
 * HOW:  Aggregate all statistics
 *
 * @param analytics Analytics handle
 * @param out_report Output report structure
 * @return true on success, false on failure
 */
bool semantic_analytics_generate_report(
    semantic_analytics_t analytics,
    analytics_report_t* out_report
);

/**
 * @brief Recommend primitives to add/remove
 *
 * WHAT: Generate recommendations for dictionary optimization
 * WHY:  Improve dictionary efficiency
 * HOW:  Analyze usage patterns, find gaps and redundancies
 *
 * @param analytics Analytics handle
 * @param out_recommendations Output recommendations array
 * @param max_recommendations Maximum recommendations
 * @param out_count Number of recommendations
 * @return true on success, false on failure
 */
bool semantic_analytics_recommend_primitives(
    semantic_analytics_t analytics,
    primitive_recommendation_t* out_recommendations,
    uint32_t max_recommendations,
    uint32_t* out_count
);

//=============================================================================
// Analysis API
//=============================================================================

/**
 * @brief Get co-occurrence score between primitives
 *
 * WHAT: Get how often primitives appear together
 * WHY:  Understand primitive relationships
 * HOW:  Lookup co-occurrence matrix
 *
 * @param analytics Analytics handle
 * @param primitive_id1 First primitive
 * @param primitive_id2 Second primitive
 * @param out_score Output score (0-1, higher = more correlated)
 * @return true if data available, false otherwise
 */
bool semantic_analytics_get_co_occurrence(
    semantic_analytics_t analytics,
    uint32_t primitive_id1,
    uint32_t primitive_id2,
    float* out_score
);

/**
 * @brief Find similar primitives
 *
 * WHAT: Find primitives with similar usage patterns
 * WHY:  Identify redundancies
 * HOW:  Compare co-occurrence vectors
 *
 * @param analytics Analytics handle
 * @param primitive_id Target primitive
 * @param out_similar_ids Output array of similar primitive IDs
 * @param max_similar Maximum to return
 * @param out_count Number found
 * @return true on success, false on failure
 */
bool semantic_analytics_find_similar(
    semantic_analytics_t analytics,
    uint32_t primitive_id,
    uint32_t* out_similar_ids,
    uint32_t max_similar,
    uint32_t* out_count
);

/**
 * @brief Predict next primitive in sequence
 *
 * WHAT: Predict likely next primitive given history
 * WHY:  Enable auto-completion, compression
 * HOW:  Pattern matching on recent context
 *
 * @param analytics Analytics handle
 * @param recent_primitives Recent primitive sequence
 * @param recent_count Length of sequence
 * @param out_predictions Output predictions (sorted by probability)
 * @param max_predictions Maximum predictions
 * @param out_count Number of predictions
 * @return true on success, false on failure
 */
bool semantic_analytics_predict_next(
    semantic_analytics_t analytics,
    const uint32_t* recent_primitives,
    uint32_t recent_count,
    uint32_t* out_predictions,
    uint32_t max_predictions,
    uint32_t* out_count
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Reset all analytics data
 *
 * WHAT: Clear all collected data
 * WHY:  Start fresh analysis
 * HOW:  Reset all counters and structures
 *
 * @param analytics Analytics handle
 * @return true on success, false on failure
 */
bool semantic_analytics_reset(semantic_analytics_t analytics);

/**
 * @brief Export analytics to JSON
 *
 * WHAT: Export analytics data as JSON
 * WHY:  Integration with external tools
 * HOW:  Format all data as JSON
 *
 * @param analytics Analytics handle
 * @param out_buffer Output buffer
 * @param buffer_size Buffer size
 * @param out_bytes Bytes written
 * @return true on success, false on failure
 */
bool semantic_analytics_export_json(
    semantic_analytics_t analytics,
    char* out_buffer,
    size_t buffer_size,
    size_t* out_bytes
);

/**
 * @brief Cleanup old analytics data
 *
 * WHAT: Remove old, unused data
 * WHY:  Manage memory usage
 * HOW:  Age-based pruning
 *
 * @param analytics Analytics handle
 * @return Number of entries cleaned up
 */
uint32_t semantic_analytics_cleanup(semantic_analytics_t analytics);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SEMANTIC_ANALYTICS_H */
