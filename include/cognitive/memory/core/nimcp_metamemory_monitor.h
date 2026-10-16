//=============================================================================
// nimcp_metamemory_monitor.h - Metamemory Monitoring System
//=============================================================================
/**
 * @file nimcp_metamemory_monitor.h
 * @brief Continuous monitoring and inventory of memory system contents
 *
 * WHAT: Metamemory monitoring provides "memory about memory" - tracking what
 *       knowledge domains are stored, memory system health, and identifying
 *       memories at risk of being forgotten
 * WHY:  Conscious memory systems need self-awareness of their contents to
 *       enable strategic rehearsal, identify knowledge gaps, and maintain
 *       memory health over time
 * HOW:  Integrates with entanglement graph, Z-ladder, and PR memory nodes
 *       to provide continuous monitoring, domain clustering, and predictive
 *       forgetting analysis
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Metamemory in Human Cognition:
 *   +-----------------------------------------------------------------------+
 *   |  Metamemory = "Thinking about memory"                                |
 *   |                                                                       |
 *   |  Key metamemory processes modeled:                                    |
 *   |  - Feeling of Knowing (FOK): "I know something about this topic"     |
 *   |  - Tip-of-Tongue (TOT): High accessibility but retrieval blocked     |
 *   |  - Judgments of Learning (JOL): "How well did I learn this?"         |
 *   |  - Source Monitoring: "Where did this memory come from?"             |
 *   |                                                                       |
 *   |  Neural basis:                                                        |
 *   |  - Prefrontal cortex: Monitoring, strategy selection                 |
 *   |  - Anterior cingulate: Error detection, conflict monitoring          |
 *   |  - Hippocampus: Pattern completion strength estimation               |
 *   |  - Parietal cortex: Attention to internal states                     |
 *   +-----------------------------------------------------------------------+
 *
 *   Knowledge Domain Clustering:
 *   +-----------------------------------------------------------------------+
 *   |  Memories cluster into semantic domains based on:                     |
 *   |  - Prime signature similarity (content overlap)                       |
 *   |  - Entanglement graph communities (associative structure)            |
 *   |  - Quaternion state similarity (emotional/contextual grouping)       |
 *   |                                                                       |
 *   |  Domain metrics:                                                      |
 *   |  - Coverage: How well is this domain represented?                    |
 *   |  - Consolidation: How firmly encoded are domain memories?            |
 *   |  - Accessibility: How easily retrieved are domain memories?          |
 *   |  - Interconnection: How linked is this domain to others?             |
 *   +-----------------------------------------------------------------------+
 *
 *   Forgetting Risk Model:
 *   +-----------------------------------------------------------------------+
 *   |  Risk factors for memory loss:                                        |
 *   |  1. Low consolidation (quaternion w component)                        |
 *   |  2. Long time since last access                                       |
 *   |  3. Low tier (Z0/Z1) with active decay                               |
 *   |  4. Weak entanglement (few associative connections)                  |
 *   |  5. Low emotional salience (low quaternion y component)              |
 *   |                                                                       |
 *   |  Risk score = weighted combination of these factors                   |
 *   |  Urgency levels: NONE, LOW, MEDIUM, HIGH, CRITICAL                   |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Full inventory: O(N) where N = total memories
 * - Domain analysis: O(N * D) where D = domains
 * - At-risk detection: O(N)
 * - Coverage computation: O(D)
 *
 * MEMORY:
 * - metamemory_monitor_t: ~500 bytes base
 * - Per domain: ~256 bytes
 * - Per at-risk entry: ~48 bytes
 * - History buffer: ~400 bytes (100 samples * 4 bytes)
 *
 * INTEGRATION:
 * - Depends on: nimcp_entanglement.h, nimcp_pr_memory_node.h, nimcp_z_ladder.h
 * - Used by: Memory system management, rehearsal scheduling, adaptive learning
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_METAMEMORY_MONITOR_H
#define NIMCP_METAMEMORY_MONITOR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Dependencies
#include "cognitive/memory/core/nimcp_entanglement.h"
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_z_ladder.h"
#include "cognitive/memory/core/nimcp_prime_signature.h"
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

/** Maximum domain name length */
#define METAMEM_MAX_DOMAIN_NAME         128

/** Maximum number of domains to track */
#define METAMEM_MAX_DOMAINS             256

/** Maximum top memories per domain */
#define METAMEM_MAX_TOP_MEMORIES        16

/** Maximum at-risk memories to track */
#define METAMEM_MAX_AT_RISK             1024

/** Default history buffer length */
#define METAMEM_HISTORY_LENGTH          100

/** Default monitoring interval (seconds) */
#define METAMEM_DEFAULT_INTERVAL        10.0f

/** High risk threshold for forgetting */
#define METAMEM_HIGH_RISK_THRESHOLD     0.75f

/** Critical risk threshold for forgetting */
#define METAMEM_CRITICAL_RISK_THRESHOLD 0.9f

/** Domain similarity threshold for clustering */
#define METAMEM_DOMAIN_SIMILARITY       0.5f

/** Minimum memories to form a domain */
#define METAMEM_MIN_DOMAIN_SIZE         3

/** Weight for consolidation in risk score */
#define METAMEM_RISK_WEIGHT_CONSOL      0.3f

/** Weight for time since access in risk score */
#define METAMEM_RISK_WEIGHT_TIME        0.25f

/** Weight for tier decay in risk score */
#define METAMEM_RISK_WEIGHT_TIER        0.2f

/** Weight for entanglement in risk score */
#define METAMEM_RISK_WEIGHT_ENTANGLE    0.15f

/** Weight for salience in risk score */
#define METAMEM_RISK_WEIGHT_SALIENCE    0.1f

/** Numerical epsilon for floating-point comparisons */
#define METAMEM_EPSILON                 1e-6f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Urgency level for at-risk memories
 *
 * Indicates how urgently a memory needs reinforcement to avoid forgetting.
 */
typedef enum {
    META_URGENCY_NONE = 0,      /**< No risk of forgetting */
    META_URGENCY_LOW,           /**< Low risk, monitor only */
    META_URGENCY_MEDIUM,        /**< Medium risk, consider review */
    META_URGENCY_HIGH,          /**< High risk, recommend immediate review */
    META_URGENCY_CRITICAL       /**< Critical risk, memory may be lost soon */
} meta_urgency_t;

/**
 * @brief Error codes for metamemory operations
 */
typedef enum {
    METAMEM_SUCCESS = 0,                /**< Operation succeeded */
    METAMEM_ERROR_NULL_POINTER = -1,    /**< NULL pointer argument */
    METAMEM_ERROR_NOT_INITIALIZED = -2, /**< Monitor not initialized */
    METAMEM_ERROR_NO_MEMORY = -3,       /**< Memory allocation failed */
    METAMEM_ERROR_INVALID_CONFIG = -4,  /**< Invalid configuration */
    METAMEM_ERROR_DOMAIN_NOT_FOUND = -5,/**< Domain not found */
    METAMEM_ERROR_CAPACITY = -6,        /**< Capacity exceeded */
    METAMEM_ERROR_INVALID_STATE = -7    /**< Invalid monitor state */
} metamem_error_t;

/**
 * @brief Alert type for metamemory callbacks
 */
typedef enum {
    METAMEM_ALERT_FORGETTING_RISK = 0,  /**< Memory at risk of being forgotten */
    METAMEM_ALERT_COVERAGE_GAP,         /**< Knowledge domain has poor coverage */
    METAMEM_ALERT_HEALTH_DEGRADED,      /**< Overall memory health declining */
    METAMEM_ALERT_TIER_OVERFLOW,        /**< Memory tier approaching capacity */
    METAMEM_ALERT_DOMAIN_FRAGMENTED     /**< Domain memories becoming disconnected */
} metamem_alert_type_t;

/**
 * @brief Knowledge domain tracking structure
 *
 * Represents a semantic cluster of related memories identified by
 * prime signature similarity and entanglement patterns.
 *
 * Memory layout: ~256 bytes + variable (top_memories pointers)
 */
typedef struct {
    char domain_name[METAMEM_MAX_DOMAIN_NAME];  /**< Human-readable domain name */
    uint64_t domain_hash;                        /**< Hash for fast lookup */
    prime_signature_t domain_signature;          /**< Representative signature */

    // Domain statistics
    size_t memory_count;                /**< Total memories in domain */
    float mean_consolidation;           /**< Average quat.w across domain */
    float mean_accessibility;           /**< Average quat.z across domain */
    float mean_salience;                /**< Average quat.y across domain */
    float coverage_score;               /**< How well covered [0, 1] */
    float interconnection_score;        /**< Internal entanglement density */
    float external_connections;         /**< Connections to other domains */

    // Tier distribution
    size_t tier_counts[PR_MEMORY_TIER_COUNT];   /**< Memories per Z-tier */

    // Top memories in domain (by consolidation)
    pr_memory_node_t** top_memories;    /**< Most consolidated memories */
    size_t num_top;                     /**< Count of top memories */
    size_t max_top;                     /**< Capacity for top memories */

    // Temporal information
    uint64_t last_update_ms;            /**< Last domain update time */
    uint64_t domain_age_ms;             /**< Time since domain identified */
} knowledge_domain_t;

/**
 * @brief Memory at risk of being forgotten
 *
 * Tracks individual memories that may be lost without reinforcement.
 *
 * Memory layout: ~48 bytes
 */
typedef struct {
    pr_memory_node_t* memory;           /**< Pointer to at-risk memory */
    uint64_t memory_id;                 /**< Memory node ID */
    float risk_score;                   /**< Overall risk [0, 1] */
    float time_since_access;            /**< Time since last retrieval (sec) */
    float decay_rate;                   /**< Estimated decay rate */
    float consolidation;                /**< Current consolidation level */
    float entanglement_strength;        /**< Average entanglement weight */
    meta_urgency_t urgency;             /**< Urgency classification */
    pr_memory_tier_t tier;              /**< Current Z-ladder tier */
} at_risk_memory_t;

/**
 * @brief Health report for the memory system
 *
 * Comprehensive health metrics for the entire memory system.
 */
typedef struct {
    // Overall metrics
    float overall_health;               /**< Combined health score [0, 1] */
    float overall_coverage;             /**< Knowledge coverage [0, 1] */
    float mean_consolidation;           /**< Average consolidation */
    float retrieval_success_rate;       /**< Recent retrieval success rate */
    float encoding_rate;                /**< Recent encoding rate (mem/sec) */

    // Tier health
    size_t tier_counts[PR_MEMORY_TIER_COUNT];   /**< Memories per tier */
    float tier_utilization[PR_MEMORY_TIER_COUNT]; /**< Capacity usage per tier */
    float tier_avg_strength[PR_MEMORY_TIER_COUNT]; /**< Average strength per tier */

    // Risk summary
    size_t at_risk_count;               /**< Total at-risk memories */
    size_t critical_count;              /**< Critical urgency memories */
    size_t high_risk_count;             /**< High urgency memories */

    // Domain summary
    size_t domain_count;                /**< Number of knowledge domains */
    size_t weak_domains;                /**< Domains with low coverage */

    // Trend indicators
    float consolidation_trend;          /**< Positive = improving, negative = declining */
    float accessibility_trend;          /**< Trend in average accessibility */
    float retrieval_trend;              /**< Trend in retrieval success */

    // Timestamp
    uint64_t report_time_ms;            /**< When report was generated */
} metamem_health_report_t;

/**
 * @brief Configuration for metamemory monitor
 */
typedef struct {
    // Monitoring intervals
    float monitor_interval_sec;         /**< Time between updates (default: 10s) */
    float deep_scan_interval_sec;       /**< Time between full scans (default: 60s) */

    // Risk thresholds
    float risk_threshold_low;           /**< Threshold for LOW urgency */
    float risk_threshold_medium;        /**< Threshold for MEDIUM urgency */
    float risk_threshold_high;          /**< Threshold for HIGH urgency */
    float risk_threshold_critical;      /**< Threshold for CRITICAL urgency */

    // Risk weights
    float risk_weight_consolidation;    /**< Weight for consolidation factor */
    float risk_weight_time;             /**< Weight for time since access */
    float risk_weight_tier;             /**< Weight for tier decay */
    float risk_weight_entanglement;     /**< Weight for entanglement */
    float risk_weight_salience;         /**< Weight for salience */

    // Domain detection
    float domain_similarity_threshold;  /**< Threshold for domain clustering */
    size_t min_domain_size;             /**< Minimum memories to form domain */
    size_t max_domains;                 /**< Maximum domains to track */
    size_t max_top_per_domain;          /**< Max top memories per domain */

    // History tracking
    size_t history_length;              /**< Length of trend history */

    // Callbacks
    bool enable_alerts;                 /**< Enable alert callbacks */

    // Memory limits
    size_t max_at_risk;                 /**< Maximum at-risk entries to track */
} metamem_config_t;

/**
 * @brief Alert callback function type
 *
 * @param alert_type Type of alert
 * @param data Alert-specific data (depends on type)
 * @param user_data User-provided context
 */
typedef void (*metamem_alert_callback_t)(
    metamem_alert_type_t alert_type,
    void* data,
    void* user_data
);

/**
 * @brief Metamemory monitor structure (opaque handle)
 */
typedef struct metamem_monitor_struct* metamem_monitor_t;

/**
 * @brief Trend data point for historical analysis
 */
typedef struct {
    float consolidation;                /**< Consolidation at this point */
    float accessibility;                /**< Accessibility at this point */
    float retrieval_success;            /**< Retrieval success rate */
    uint64_t timestamp_ms;              /**< Timestamp of measurement */
} metamem_trend_point_t;

/**
 * @brief Domain summary for API output
 */
typedef struct {
    char name[METAMEM_MAX_DOMAIN_NAME]; /**< Domain name */
    uint64_t hash;                      /**< Domain hash */
    size_t memory_count;                /**< Memories in domain */
    float coverage;                     /**< Coverage score */
    float mean_consolidation;           /**< Average consolidation */
    float mean_accessibility;           /**< Average accessibility */
    float health_score;                 /**< Overall domain health */
} metamem_domain_summary_t;

/**
 * @brief Review recommendation for memory maintenance
 */
typedef struct {
    pr_memory_node_t* memory;           /**< Memory to review */
    uint64_t memory_id;                 /**< Memory ID */
    float priority;                     /**< Review priority [0, 1] */
    float risk_score;                   /**< Current risk score */
    const char* reason;                 /**< Why review is recommended */
    meta_urgency_t urgency;             /**< Urgency level */
} metamem_review_rec_t;

/**
 * @brief Forgetting prediction result
 */
typedef struct {
    pr_memory_node_t* memory;           /**< Memory being predicted */
    uint64_t memory_id;                 /**< Memory ID */
    float probability_24h;              /**< P(forget) within 24 hours */
    float probability_7d;               /**< P(forget) within 7 days */
    float probability_30d;              /**< P(forget) within 30 days */
    float estimated_half_life_sec;      /**< Estimated half-life in seconds */
} metamem_forgetting_pred_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default metamemory monitor configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for most use cases
 *
 * @return Default configuration with standard parameters
 *
 * Performance: ~10ns
 *
 * Example:
 *   metamem_config_t config = metamem_config_default();
 *   config.monitor_interval_sec = 5.0f;  // More frequent monitoring
 */
NIMCP_EXPORT metamem_config_t metamem_config_default(void);

/**
 * @brief Validate metamemory configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Performance: ~20ns
 */
NIMCP_EXPORT bool metamem_config_validate(const metamem_config_t* config);

//=============================================================================
// Monitor Lifecycle
//=============================================================================

/**
 * @brief Create a metamemory monitor
 *
 * WHAT: Allocates and initializes metamemory monitoring system
 * WHY:  Central manager for memory system awareness
 * HOW:  Creates domain tracking, risk analysis, and trend history
 *
 * @param entanglement Entanglement graph to monitor
 * @param node_manager PR memory node manager
 * @param z_ladder Z-Ladder tier manager
 * @param config Configuration (NULL for defaults)
 * @return Monitor handle or NULL on failure
 *
 * Performance: O(1)
 * Memory: ~2KB base + domains + history
 *
 * Example:
 *   metamem_monitor_t monitor = metamem_monitor_create(
 *       entangle_graph, node_manager, z_ladder, NULL);
 */
NIMCP_EXPORT metamem_monitor_t metamem_monitor_create(
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    z_ladder_t z_ladder,
    const metamem_config_t* config
);

/**
 * @brief Destroy metamemory monitor
 *
 * WHAT: Releases all monitor resources
 * WHY:  Clean shutdown
 * HOW:  Frees domains, at-risk list, history buffers
 *
 * @param monitor Monitor to destroy (NULL safe)
 *
 * Performance: O(D + R) where D = domains, R = at-risk entries
 */
NIMCP_EXPORT void metamem_monitor_destroy(metamem_monitor_t monitor);

//=============================================================================
// Update and Monitoring
//=============================================================================

/**
 * @brief Main update cycle for metamemory monitor
 *
 * WHAT: Performs periodic monitoring tasks
 * WHY:  Keep metamemory state current
 * HOW:  Updates at-risk list, domain stats, trends based on elapsed time
 *
 * @param monitor Monitor to update
 * @param current_time_ms Current timestamp
 * @return METAMEM_SUCCESS or error code
 *
 * Performance: O(N) where N = total memories (for quick updates)
 *              O(N * D) for full scans
 *
 * Example:
 *   // Call periodically
 *   metamem_monitor_update(monitor, current_time_ms());
 */
NIMCP_EXPORT metamem_error_t metamem_monitor_update(
    metamem_monitor_t monitor,
    uint64_t current_time_ms
);

/**
 * @brief Force full inventory scan
 *
 * WHAT: Complete scan of all memories and domains
 * WHY:  Rebuild complete picture after major changes
 * HOW:  Iterates all memories, rebuilds domain clusters, updates all stats
 *
 * @param monitor Monitor to update
 * @return METAMEM_SUCCESS or error code
 *
 * Performance: O(N * D) where N = memories, D = domains
 */
NIMCP_EXPORT metamem_error_t metamem_monitor_full_scan(metamem_monitor_t monitor);

//=============================================================================
// Knowledge Domain API
//=============================================================================

/**
 * @brief Build/rebuild knowledge domain inventory
 *
 * WHAT: Clusters memories into semantic domains
 * WHY:  Understand what topics the memory system knows about
 * HOW:  Clusters by prime signature similarity and entanglement patterns
 *
 * @param monitor Monitor to update
 * @return Number of domains identified, or -1 on error
 *
 * Performance: O(N^2) for clustering (can be optimized with LSH)
 *
 * Algorithm:
 * 1. For each memory, compute signature similarity to existing domain centers
 * 2. Assign to closest domain if similarity > threshold
 * 3. Create new domain if no match found
 * 4. Update domain statistics
 */
NIMCP_EXPORT int metamem_monitor_inventory_domains(metamem_monitor_t monitor);

/**
 * @brief Get summary for a specific domain
 *
 * WHAT: Returns statistics for a knowledge domain
 * WHY:  Query domain health and coverage
 *
 * @param monitor Monitor to query
 * @param domain_hash Hash of domain to query
 * @param summary Output summary structure
 * @return METAMEM_SUCCESS or error code
 *
 * Performance: O(1) lookup + O(D_size) for stats
 */
NIMCP_EXPORT metamem_error_t metamem_monitor_get_domain_summary(
    metamem_monitor_t monitor,
    uint64_t domain_hash,
    metamem_domain_summary_t* summary
);

/**
 * @brief Get all domain summaries
 *
 * @param monitor Monitor to query
 * @param summaries Output array (caller-allocated)
 * @param max_summaries Maximum summaries to return
 * @param count Output: actual count returned
 * @return METAMEM_SUCCESS or error code
 *
 * Performance: O(D) where D = domains
 */
NIMCP_EXPORT metamem_error_t metamem_monitor_get_all_domains(
    metamem_monitor_t monitor,
    metamem_domain_summary_t* summaries,
    size_t max_summaries,
    size_t* count
);

/**
 * @brief Compute coverage analysis for a domain
 *
 * WHAT: Analyzes how well a domain is covered
 * WHY:  Identify knowledge gaps and areas needing reinforcement
 * HOW:  Evaluates memory count, consolidation distribution, tier spread
 *
 * @param monitor Monitor to query
 * @param domain_hash Domain to analyze
 * @return Coverage score [0, 1], or -1 on error
 *
 * Coverage factors:
 * - Memory count relative to expected
 * - Tier distribution (more in Z2/Z3 = better)
 * - Consolidation distribution
 * - Entanglement density
 */
NIMCP_EXPORT float metamem_monitor_compute_coverage(
    metamem_monitor_t monitor,
    uint64_t domain_hash
);

/**
 * @brief Find domain by name
 *
 * @param monitor Monitor to query
 * @param name Domain name to search
 * @return Domain hash, or 0 if not found
 *
 * Performance: O(D) linear search
 */
NIMCP_EXPORT uint64_t metamem_monitor_find_domain(
    metamem_monitor_t monitor,
    const char* name
);

/**
 * @brief Register a new domain manually
 *
 * WHAT: Creates a domain with specified name and seed signature
 * WHY:  Allow manual domain creation for known topics
 *
 * @param monitor Monitor to update
 * @param name Domain name
 * @param seed_signature Optional seed signature for clustering
 * @return Domain hash, or 0 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT uint64_t metamem_monitor_register_domain(
    metamem_monitor_t monitor,
    const char* name,
    const prime_signature_t* seed_signature
);

//=============================================================================
// At-Risk Detection API
//=============================================================================

/**
 * @brief Detect memories at risk of being forgotten
 *
 * WHAT: Identifies memories that may be lost without reinforcement
 * WHY:  Enable proactive memory maintenance
 * HOW:  Scores each memory based on risk factors, tracks high-risk items
 *
 * @param monitor Monitor to update
 * @return Number of at-risk memories found, or -1 on error
 *
 * Performance: O(N) where N = total memories
 *
 * Risk factors considered:
 * - Low consolidation (quat.w)
 * - Long time since access
 * - Low tier with high decay rate
 * - Weak entanglement (few connections)
 * - Low salience (quat.y)
 */
NIMCP_EXPORT int metamem_monitor_detect_at_risk(metamem_monitor_t monitor);

/**
 * @brief Get list of at-risk memories
 *
 * @param monitor Monitor to query
 * @param at_risk Output array (caller-allocated)
 * @param max_count Maximum entries to return
 * @param count Output: actual count returned
 * @return METAMEM_SUCCESS or error code
 *
 * Performance: O(min(R, max_count))
 */
NIMCP_EXPORT metamem_error_t metamem_monitor_get_at_risk(
    metamem_monitor_t monitor,
    at_risk_memory_t* at_risk,
    size_t max_count,
    size_t* count
);

/**
 * @brief Get at-risk memories filtered by urgency
 *
 * @param monitor Monitor to query
 * @param min_urgency Minimum urgency level to include
 * @param at_risk Output array
 * @param max_count Maximum entries
 * @param count Output: actual count
 * @return METAMEM_SUCCESS or error code
 */
NIMCP_EXPORT metamem_error_t metamem_monitor_get_at_risk_by_urgency(
    metamem_monitor_t monitor,
    meta_urgency_t min_urgency,
    at_risk_memory_t* at_risk,
    size_t max_count,
    size_t* count
);

/**
 * @brief Compute risk score for a specific memory
 *
 * @param monitor Monitor context
 * @param node Memory node to evaluate
 * @return Risk score [0, 1], or -1 on error
 */
NIMCP_EXPORT float metamem_monitor_compute_risk(
    metamem_monitor_t monitor,
    const pr_memory_node_t* node
);

/**
 * @brief Classify urgency from risk score
 *
 * @param monitor Monitor (for threshold configuration)
 * @param risk_score Risk score [0, 1]
 * @return Urgency classification
 */
NIMCP_EXPORT meta_urgency_t metamem_monitor_classify_urgency(
    metamem_monitor_t monitor,
    float risk_score
);

//=============================================================================
// Health Reporting API
//=============================================================================

/**
 * @brief Get comprehensive health report
 *
 * WHAT: Returns detailed health metrics for entire memory system
 * WHY:  Enable monitoring dashboards and health alerts
 * HOW:  Aggregates per-tier, per-domain, and overall metrics
 *
 * @param monitor Monitor to query
 * @param report Output health report structure
 * @return METAMEM_SUCCESS or error code
 *
 * Performance: O(N + D) where N = memories, D = domains
 */
NIMCP_EXPORT metamem_error_t metamem_monitor_get_health_report(
    metamem_monitor_t monitor,
    metamem_health_report_t* report
);

/**
 * @brief Get overall memory system health score
 *
 * WHAT: Single metric summarizing memory system health
 * WHY:  Quick status check
 *
 * @param monitor Monitor to query
 * @return Health score [0, 1], or -1 on error
 *
 * Health factors:
 * - Tier utilization balance
 * - Average consolidation levels
 * - At-risk memory proportion
 * - Domain coverage
 * - Recent retrieval success
 */
NIMCP_EXPORT float metamem_monitor_get_overall_health(metamem_monitor_t monitor);

//=============================================================================
// Review and Prediction API
//=============================================================================

/**
 * @brief Get recommended memories to review
 *
 * WHAT: Returns prioritized list of memories to rehearse
 * WHY:  Enable strategic memory maintenance
 * HOW:  Combines risk score with review value to prioritize
 *
 * @param monitor Monitor to query
 * @param recommendations Output array (caller-allocated)
 * @param max_count Maximum recommendations
 * @param count Output: actual count
 * @return METAMEM_SUCCESS or error code
 *
 * Priority considers:
 * - Risk of forgetting
 * - Domain importance
 * - Entanglement centrality (reviewing may strengthen network)
 * - Time efficiency (review memories that reinforce others)
 */
NIMCP_EXPORT metamem_error_t metamem_monitor_recommend_review(
    metamem_monitor_t monitor,
    metamem_review_rec_t* recommendations,
    size_t max_count,
    size_t* count
);

/**
 * @brief Predict which memories will fade
 *
 * WHAT: Probabilistic predictions of memory loss over time horizons
 * WHY:  Enable proactive intervention before memories are lost
 * HOW:  Exponential decay model based on tier and consolidation
 *
 * @param monitor Monitor to query
 * @param predictions Output array (caller-allocated)
 * @param max_count Maximum predictions
 * @param count Output: actual count
 * @return METAMEM_SUCCESS or error code
 *
 * Prediction model:
 * - P(forget) = 1 - exp(-decay_rate * time / (1 + consolidation))
 * - Adjusted for tier, entanglement, and recent access patterns
 */
NIMCP_EXPORT metamem_error_t metamem_monitor_predict_forgetting(
    metamem_monitor_t monitor,
    metamem_forgetting_pred_t* predictions,
    size_t max_count,
    size_t* count
);

/**
 * @brief Predict forgetting for a specific memory
 *
 * @param monitor Monitor context
 * @param node Memory to predict
 * @param prediction Output prediction structure
 * @return METAMEM_SUCCESS or error code
 */
NIMCP_EXPORT metamem_error_t metamem_monitor_predict_memory(
    metamem_monitor_t monitor,
    const pr_memory_node_t* node,
    metamem_forgetting_pred_t* prediction
);

//=============================================================================
// Historical Trends API
//=============================================================================

/**
 * @brief Get historical trend data
 *
 * WHAT: Returns time series of memory system metrics
 * WHY:  Enable trend analysis and visualization
 *
 * @param monitor Monitor to query
 * @param points Output array (caller-allocated)
 * @param max_points Maximum points to return
 * @param count Output: actual count
 * @return METAMEM_SUCCESS or error code
 *
 * Returns most recent points first.
 */
NIMCP_EXPORT metamem_error_t metamem_monitor_get_trends(
    metamem_monitor_t monitor,
    metamem_trend_point_t* points,
    size_t max_points,
    size_t* count
);

/**
 * @brief Compute trend direction for a metric
 *
 * WHAT: Returns slope of recent trend
 * WHY:  Quick indicator of improvement or decline
 *
 * @param monitor Monitor to query
 * @param metric Which metric to analyze (0=consolidation, 1=accessibility, 2=retrieval)
 * @return Trend slope (positive = improving, negative = declining)
 */
NIMCP_EXPORT float metamem_monitor_compute_trend(
    metamem_monitor_t monitor,
    int metric
);

//=============================================================================
// Callback API
//=============================================================================

/**
 * @brief Set alert callback for metamemory events
 *
 * WHAT: Register function to be called on metamemory alerts
 * WHY:  Enable reactive systems to respond to memory issues
 *
 * @param monitor Monitor to configure
 * @param callback Callback function
 * @param user_data Context to pass to callback
 * @return METAMEM_SUCCESS or error code
 *
 * Alert types:
 * - FORGETTING_RISK: Memory at critical risk
 * - COVERAGE_GAP: Domain has poor coverage
 * - HEALTH_DEGRADED: Overall health declining
 * - TIER_OVERFLOW: Tier approaching capacity
 * - DOMAIN_FRAGMENTED: Domain losing coherence
 */
NIMCP_EXPORT metamem_error_t metamem_monitor_set_alert_callback(
    metamem_monitor_t monitor,
    metamem_alert_callback_t callback,
    void* user_data
);

/**
 * @brief Clear alert callback
 *
 * @param monitor Monitor to configure
 * @return METAMEM_SUCCESS or error code
 */
NIMCP_EXPORT metamem_error_t metamem_monitor_clear_alert_callback(
    metamem_monitor_t monitor
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error string for metamemory error code
 *
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* metamem_error_string(metamem_error_t error);

/**
 * @brief Get urgency level name as string
 *
 * @param urgency Urgency level
 * @return Human-readable urgency name
 */
NIMCP_EXPORT const char* metamem_urgency_string(meta_urgency_t urgency);

/**
 * @brief Get alert type name as string
 *
 * @param alert_type Alert type
 * @return Human-readable alert type name
 */
NIMCP_EXPORT const char* metamem_alert_type_string(metamem_alert_type_t alert_type);

/**
 * @brief Get current time in milliseconds
 *
 * @return Milliseconds since epoch
 */
NIMCP_EXPORT uint64_t metamem_current_time_ms(void);

/**
 * @brief Print health report to stdout
 *
 * @param report Report to print
 */
NIMCP_EXPORT void metamem_print_health_report(const metamem_health_report_t* report);

/**
 * @brief Print domain summary to stdout
 *
 * @param summary Summary to print
 */
NIMCP_EXPORT void metamem_print_domain_summary(const metamem_domain_summary_t* summary);

/**
 * @brief Validate monitor internal consistency
 *
 * @param monitor Monitor to validate
 * @return true if consistent, false if corruption detected
 */
NIMCP_EXPORT bool metamem_monitor_validate(metamem_monitor_t monitor);

//=============================================================================
// Inline Helper Functions
//=============================================================================

/**
 * @brief Check if risk score indicates high risk
 *
 * @param risk_score Risk score [0, 1]
 * @return true if high risk (>= 0.75)
 */
static inline bool metamem_is_high_risk(float risk_score) {
    return risk_score >= METAMEM_HIGH_RISK_THRESHOLD;
}

/**
 * @brief Check if risk score indicates critical risk
 *
 * @param risk_score Risk score [0, 1]
 * @return true if critical risk (>= 0.9)
 */
static inline bool metamem_is_critical_risk(float risk_score) {
    return risk_score >= METAMEM_CRITICAL_RISK_THRESHOLD;
}

/**
 * @brief Compute simple hash for domain name
 *
 * @param name Domain name
 * @return 64-bit hash
 */
static inline uint64_t metamem_hash_domain_name(const char* name) {
    if (!name) return 0;
    uint64_t hash = 14695981039346656037ULL; // FNV offset basis
    while (*name) {
        hash ^= (uint64_t)(unsigned char)(*name++);
        hash *= 1099511628211ULL; // FNV prime
    }
    return hash;
}

#ifdef __cplusplus
}
#endif

#endif // NIMCP_METAMEMORY_MONITOR_H
