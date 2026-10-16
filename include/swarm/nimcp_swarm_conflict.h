/**
 * @file nimcp_swarm_conflict.h
 * @brief Multi-swarm conflict detection and resolution
 *
 * WHAT: Detect and resolve conflicts between multiple swarm groups
 * WHY: Multiple swarms may compete for resources or have contradictory goals
 * HOW: Conflict detection -> priority evaluation -> negotiation -> resolution
 *
 * BIOLOGICAL INSPIRATION:
 * - Ant colony conflict resolution (priority-based)
 * - Bee waggle dance negotiation (consensus-building)
 * - Slime mold resource allocation (fair-share)
 * - Bird flock conflict avoidance (yield/defer)
 *
 * FEATURES:
 * 1. Conflict Detection - Detect resource, goal, territory, priority conflicts
 * 2. Resolution Strategies - Priority, fair-share, negotiation, arbitration, yield
 * 3. Negotiation Protocol - Multi-round negotiation with convergence detection
 * 4. Resolution Tracking - History, statistics, and analytics
 *
 * @author NIMCP Development Team
 * @version 1.0
 * @date 2025
 */

#ifndef NIMCP_SWARM_CONFLICT_H
#define NIMCP_SWARM_CONFLICT_H

#include "utils/validation/nimcp_common.h"
#include "swarm/nimcp_swarm_multi.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * TYPE DEFINITIONS
 *============================================================================*/

/**
 * @brief Opaque conflict resolver handle
 */
typedef struct swarm_conflict_resolver_struct* swarm_conflict_resolver_t;

/**
 * @brief Maximum number of swarms in a conflict
 */
#define NIMCP_MAX_CONFLICT_SWARMS 8

/**
 * @brief Maximum negotiation rounds
 */
#define NIMCP_MAX_NEGOTIATION_ROUNDS 10

/**
 * @brief Maximum conflict history entries
 */
#define NIMCP_MAX_CONFLICT_HISTORY 1000

/**
 * @brief Conflict type enumeration
 */
typedef enum {
    CONFLICT_TYPE_NONE = 0,
    CONFLICT_TYPE_RESOURCE,        /**< Resource competition */
    CONFLICT_TYPE_GOAL,            /**< Contradictory goals */
    CONFLICT_TYPE_TERRITORY,       /**< Territory overlap */
    CONFLICT_TYPE_PRIORITY,        /**< Priority disputes */
    CONFLICT_TYPE_COMMUNICATION,   /**< Communication conflicts */
    CONFLICT_TYPE_COUNT
} conflict_type_t;

/**
 * @brief Resolution strategy enumeration
 */
typedef enum {
    RESOLUTION_STRATEGY_PRIORITY_WINS,  /**< Higher priority swarm wins */
    RESOLUTION_STRATEGY_FAIR_SHARE,     /**< Split resource proportionally */
    RESOLUTION_STRATEGY_NEGOTIATION,    /**< Multi-round negotiation */
    RESOLUTION_STRATEGY_ARBITRATION,    /**< External arbiter decides */
    RESOLUTION_STRATEGY_YIELD,          /**< Lower priority yields */
    RESOLUTION_STRATEGY_TIME_SHARE,     /**< Time-based sharing */
    RESOLUTION_STRATEGY_MERGE,          /**< Merge conflicting swarms */
    RESOLUTION_STRATEGY_COUNT
} resolution_strategy_t;

/**
 * @brief Conflict configuration
 */
typedef struct {
    uint32_t max_conflicts;             /**< Maximum tracked conflicts */
    uint32_t resolution_timeout_ms;     /**< Resolution timeout */
    bool enable_negotiation;            /**< Enable negotiation protocol */
    bool enable_arbitration;            /**< Enable arbitration */
    bool enable_auto_resolution;        /**< Auto-resolve conflicts */
    float convergence_threshold;        /**< Negotiation convergence threshold */
    uint32_t max_negotiation_rounds;    /**< Max rounds per negotiation */
    resolution_strategy_t default_strategy; /**< Default resolution strategy */
} conflict_config_t;

/**
 * @brief Conflict descriptor
 */
typedef struct {
    uint64_t conflict_id;                       /**< Unique conflict ID */
    conflict_type_t type;                       /**< Type of conflict */
    uint64_t swarm_ids[NIMCP_MAX_CONFLICT_SWARMS]; /**< Involved swarms */
    uint32_t swarm_count;                       /**< Number of swarms */
    uint64_t resource_id;                       /**< Contested resource ID */
    float severity;                             /**< Severity 0.0-1.0 */
    uint64_t timestamp_us;                      /**< Detection timestamp */
    uint64_t resolution_timestamp_us;           /**< Resolution timestamp */
    bool is_resolved;                           /**< Resolution status */
    char description[256];                      /**< Human-readable description */
    void* context_data;                         /**< Type-specific context */
    uint32_t context_size;                      /**< Context data size */
} conflict_t;

/**
 * @brief Resolution result
 */
typedef struct {
    uint64_t conflict_id;               /**< Conflict ID */
    resolution_strategy_t strategy_used; /**< Strategy applied */
    uint64_t winner_id;                 /**< Winner swarm (if applicable) */
    float terms[NIMCP_MAX_CONFLICT_SWARMS]; /**< Resolution terms per swarm */
    uint32_t term_count;                /**< Number of terms */
    bool success;                       /**< Resolution successful */
    uint32_t negotiation_rounds;        /**< Rounds taken */
    float resolution_time_ms;           /**< Time to resolve */
    char outcome_description[512];      /**< Outcome description */
} resolution_result_t;

/**
 * @brief Negotiation offer
 */
typedef struct {
    uint64_t conflict_id;               /**< Associated conflict */
    uint64_t proposer_swarm_id;         /**< Proposing swarm */
    uint32_t round;                     /**< Negotiation round */
    float proposal[NIMCP_MAX_CONFLICT_SWARMS]; /**< Proposed allocation */
    uint32_t proposal_size;             /**< Number of values */
    float acceptance_score;             /**< How acceptable (0-1) */
    uint64_t timestamp_us;              /**< Proposal timestamp */
} negotiation_offer_t;

/**
 * @brief Conflict resolution statistics
 */
typedef struct {
    uint32_t total_conflicts;           /**< Total conflicts detected */
    uint32_t conflicts_resolved;        /**< Successfully resolved */
    uint32_t conflicts_pending;         /**< Currently pending */
    uint32_t conflicts_failed;          /**< Failed to resolve */
    float avg_resolution_time_ms;       /**< Average resolution time */
    float max_resolution_time_ms;       /**< Maximum resolution time */
    float min_resolution_time_ms;       /**< Minimum resolution time */
    uint32_t strategy_usage[RESOLUTION_STRATEGY_COUNT]; /**< Usage per strategy */
    uint32_t type_counts[CONFLICT_TYPE_COUNT]; /**< Conflicts per type */
    float avg_severity;                 /**< Average conflict severity */
    uint32_t negotiations_started;      /**< Negotiations initiated */
    uint32_t negotiations_succeeded;    /**< Negotiations that converged */
    uint32_t arbitrations;              /**< Arbitrations performed */
    uint32_t merges;                    /**< Swarm merges performed */
} conflict_stats_t;

/**
 * @brief Swarm state for conflict detection
 */
typedef struct {
    uint64_t swarm_id;                  /**< Swarm identifier */
    nimcp_territory_bounds_t territory; /**< Operational territory */
    uint64_t resource_ids[32];          /**< Resources in use */
    uint32_t resource_count;            /**< Number of resources */
    uint64_t goal_ids[16];              /**< Active goals */
    uint32_t goal_count;                /**< Number of goals */
    float priority;                     /**< Overall priority */
    nimcp_mission_priority_t mission_priority; /**< Mission priority */
    bool is_active;                     /**< Currently active */
} swarm_state_t;

/*=============================================================================
 * LIFECYCLE API
 *============================================================================*/

/**
 * @brief Create conflict resolver
 *
 * WHAT: Creates a new conflict resolver instance
 * WHY:  Enables conflict detection and resolution for swarms
 * HOW:  Allocates resolver structure with configuration
 *
 * @param coordinator Multi-swarm coordinator
 * @param config Conflict configuration (NULL for defaults)
 * @return Conflict resolver handle or NULL on failure
 */
swarm_conflict_resolver_t conflict_resolver_create(
    nimcp_multi_swarm_coordinator_t* coordinator,
    const conflict_config_t* config
);

/**
 * @brief Destroy conflict resolver
 *
 * WHAT: Destroys conflict resolver and frees resources
 * WHY:  Clean up resolver on shutdown
 * HOW:  Frees all tracked conflicts and internal structures
 *
 * @param resolver Resolver to destroy
 */
void conflict_resolver_destroy(swarm_conflict_resolver_t resolver);

/**
 * @brief Get default conflict configuration
 *
 * WHAT: Returns default configuration values
 * WHY:  Provides sensible defaults for typical scenarios
 * HOW:  Returns pre-configured struct
 *
 * @return Default configuration
 */
conflict_config_t conflict_resolver_default_config(void);

/*=============================================================================
 * FEATURE 1: CONFLICT DETECTION
 *============================================================================*/

/**
 * @brief Detect conflicts between swarms
 *
 * WHAT: Scans swarm states for resource, goal, and territory conflicts
 * WHY:  Proactive detection enables early resolution
 * HOW:  Pairwise comparison of territories, resources, and goals
 *
 * @param resolver Conflict resolver
 * @param swarm_states Array of swarm states
 * @param state_count Number of swarm states
 * @param conflicts Output array for detected conflicts (caller allocates)
 * @param max_conflicts Maximum conflicts to detect
 * @param count Output for number of conflicts detected
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_detect(
    swarm_conflict_resolver_t resolver,
    const swarm_state_t* swarm_states,
    uint32_t state_count,
    conflict_t* conflicts,
    uint32_t max_conflicts,
    uint32_t* count
);

/**
 * @brief Calculate conflict severity
 *
 * WHAT: Computes severity score for a conflict
 * WHY:  Enables prioritization of resolution efforts
 * HOW:  Considers resource importance, swarm priorities, and impact
 *
 * @param resolver Conflict resolver
 * @param conflict Conflict to evaluate
 * @return Severity score (0.0 = minor, 1.0 = critical)
 */
float conflict_resolver_calculate_severity(
    swarm_conflict_resolver_t resolver,
    const conflict_t* conflict
);

/**
 * @brief Detect resource conflicts
 *
 * WHAT: Detects when multiple swarms want the same resource
 * WHY:  Resource contention must be resolved for efficiency
 * HOW:  Checks for overlapping resource requests
 *
 * @param resolver Conflict resolver
 * @param swarm_states Array of swarm states
 * @param state_count Number of states
 * @param conflicts Output conflicts array
 * @param max_conflicts Maximum conflicts
 * @param count Output count
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_detect_resource_conflicts(
    swarm_conflict_resolver_t resolver,
    const swarm_state_t* swarm_states,
    uint32_t state_count,
    conflict_t* conflicts,
    uint32_t max_conflicts,
    uint32_t* count
);

/**
 * @brief Detect territory conflicts
 *
 * WHAT: Detects overlapping operational territories
 * WHY:  Territory conflicts can cause coordination issues
 * HOW:  Checks bounding box intersections
 *
 * @param resolver Conflict resolver
 * @param swarm_states Array of swarm states
 * @param state_count Number of states
 * @param conflicts Output conflicts array
 * @param max_conflicts Maximum conflicts
 * @param count Output count
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_detect_territory_conflicts(
    swarm_conflict_resolver_t resolver,
    const swarm_state_t* swarm_states,
    uint32_t state_count,
    conflict_t* conflicts,
    uint32_t max_conflicts,
    uint32_t* count
);

/**
 * @brief Detect goal conflicts
 *
 * WHAT: Detects contradictory or competing goals
 * WHY:  Goal conflicts lead to inefficient behavior
 * HOW:  Checks for mutually exclusive goals
 *
 * @param resolver Conflict resolver
 * @param swarm_states Array of swarm states
 * @param state_count Number of states
 * @param conflicts Output conflicts array
 * @param max_conflicts Maximum conflicts
 * @param count Output count
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_detect_goal_conflicts(
    swarm_conflict_resolver_t resolver,
    const swarm_state_t* swarm_states,
    uint32_t state_count,
    conflict_t* conflicts,
    uint32_t max_conflicts,
    uint32_t* count
);

/*=============================================================================
 * FEATURE 2: RESOLUTION STRATEGIES
 *============================================================================*/

/**
 * @brief Set resolution strategy for conflict type
 *
 * WHAT: Configures which strategy to use for a conflict type
 * WHY:  Different conflict types benefit from different strategies
 * HOW:  Stores strategy mapping in resolver configuration
 *
 * @param resolver Conflict resolver
 * @param conflict_type Type of conflict
 * @param strategy Strategy to use
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_set_strategy(
    swarm_conflict_resolver_t resolver,
    conflict_type_t conflict_type,
    resolution_strategy_t strategy
);

/**
 * @brief Resolve conflict with specified strategy
 *
 * WHAT: Applies resolution strategy to resolve conflict
 * WHY:  Enables flexible conflict resolution
 * HOW:  Dispatches to strategy-specific implementation
 *
 * @param resolver Conflict resolver
 * @param conflict Conflict to resolve
 * @param strategy Strategy to apply (or use default)
 * @param result Output resolution result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_resolve(
    swarm_conflict_resolver_t resolver,
    conflict_t* conflict,
    resolution_strategy_t strategy,
    resolution_result_t* result
);

/**
 * @brief Resolve using priority strategy
 *
 * WHAT: Higher priority swarm wins the conflict
 * WHY:  Simple, decisive resolution for clear priority cases
 * HOW:  Compares swarm priorities, awards to highest
 *
 * @param resolver Conflict resolver
 * @param conflict Conflict to resolve
 * @param result Output result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_priority_wins(
    swarm_conflict_resolver_t resolver,
    conflict_t* conflict,
    resolution_result_t* result
);

/**
 * @brief Resolve using fair-share strategy
 *
 * WHAT: Splits resource proportionally among swarms
 * WHY:  Equitable distribution for shared resources
 * HOW:  Allocates proportional to priority or capacity
 *
 * @param resolver Conflict resolver
 * @param conflict Conflict to resolve
 * @param result Output result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_fair_share(
    swarm_conflict_resolver_t resolver,
    conflict_t* conflict,
    resolution_result_t* result
);

/**
 * @brief Resolve using arbitration strategy
 *
 * WHAT: External arbiter makes final decision
 * WHY:  Neutral third party for difficult conflicts
 * HOW:  Invokes coordinator's arbitration logic
 *
 * @param resolver Conflict resolver
 * @param conflict Conflict to resolve
 * @param result Output result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_arbitration(
    swarm_conflict_resolver_t resolver,
    conflict_t* conflict,
    resolution_result_t* result
);

/**
 * @brief Resolve using yield strategy
 *
 * WHAT: Lower priority swarm yields to higher priority
 * WHY:  Graceful conflict avoidance
 * HOW:  Lower priority swarm backs off
 *
 * @param resolver Conflict resolver
 * @param conflict Conflict to resolve
 * @param result Output result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_yield(
    swarm_conflict_resolver_t resolver,
    conflict_t* conflict,
    resolution_result_t* result
);

/*=============================================================================
 * FEATURE 3: NEGOTIATION PROTOCOL
 *============================================================================*/

/**
 * @brief Start negotiation for conflict
 *
 * WHAT: Initiates multi-round negotiation protocol
 * WHY:  Allows swarms to reach mutually acceptable solution
 * HOW:  Sets up negotiation context and sends initial offers
 *
 * @param resolver Conflict resolver
 * @param conflict Conflict to negotiate
 * @param max_rounds Maximum negotiation rounds
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_negotiate(
    swarm_conflict_resolver_t resolver,
    conflict_t* conflict,
    uint32_t max_rounds
);

/**
 * @brief Make negotiation offer
 *
 * WHAT: Swarm proposes solution to conflict
 * WHY:  Enables collaborative problem-solving
 * HOW:  Stores offer and broadcasts to other swarms
 *
 * @param resolver Conflict resolver
 * @param conflict_id Conflict ID
 * @param swarm_id Proposing swarm
 * @param proposal Proposed allocation
 * @param proposal_size Number of allocation values
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_make_offer(
    swarm_conflict_resolver_t resolver,
    uint64_t conflict_id,
    uint64_t swarm_id,
    const float* proposal,
    uint32_t proposal_size
);

/**
 * @brief Accept negotiation offer
 *
 * WHAT: Swarm accepts current offer
 * WHY:  Finalizes negotiated solution
 * HOW:  Marks conflict as resolved with accepted offer
 *
 * @param resolver Conflict resolver
 * @param conflict_id Conflict ID
 * @param swarm_id Accepting swarm
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_accept_offer(
    swarm_conflict_resolver_t resolver,
    uint64_t conflict_id,
    uint64_t swarm_id
);

/**
 * @brief Reject negotiation offer
 *
 * WHAT: Swarm rejects current offer
 * WHY:  Allows continued negotiation with feedback
 * HOW:  Logs rejection and increments negotiation round
 *
 * @param resolver Conflict resolver
 * @param conflict_id Conflict ID
 * @param swarm_id Rejecting swarm
 * @param reason Rejection reason
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_reject_offer(
    swarm_conflict_resolver_t resolver,
    uint64_t conflict_id,
    uint64_t swarm_id,
    const char* reason
);

/**
 * @brief Check negotiation convergence
 *
 * WHAT: Determines if negotiation has converged
 * WHY:  Detect successful consensus
 * HOW:  Checks if recent offers are within threshold
 *
 * @param resolver Conflict resolver
 * @param conflict_id Conflict ID
 * @param converged Output convergence status
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_check_convergence(
    swarm_conflict_resolver_t resolver,
    uint64_t conflict_id,
    bool* converged
);

/**
 * @brief Handle negotiation timeout
 *
 * WHAT: Handles negotiation timeout scenario
 * WHY:  Prevents indefinite negotiation
 * HOW:  Falls back to arbitration or priority strategy
 *
 * @param resolver Conflict resolver
 * @param conflict_id Conflict ID
 * @param result Output result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_handle_timeout(
    swarm_conflict_resolver_t resolver,
    uint64_t conflict_id,
    resolution_result_t* result
);

/*=============================================================================
 * FEATURE 4: RESOLUTION TRACKING
 *============================================================================*/

/**
 * @brief Get conflict history for swarm
 *
 * WHAT: Retrieves past conflicts involving a swarm
 * WHY:  Analyze conflict patterns and learn
 * HOW:  Queries history database filtered by swarm ID
 *
 * @param resolver Conflict resolver
 * @param swarm_id Swarm ID to query
 * @param conflicts Output conflicts array (caller allocates)
 * @param max_conflicts Maximum conflicts to return
 * @param count Output count
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_get_history(
    swarm_conflict_resolver_t resolver,
    uint64_t swarm_id,
    conflict_t* conflicts,
    uint32_t max_conflicts,
    uint32_t* count
);

/**
 * @brief Get conflict resolution statistics
 *
 * WHAT: Returns comprehensive conflict statistics
 * WHY:  Monitor system health and resolution effectiveness
 * HOW:  Aggregates stats from history
 *
 * @param resolver Conflict resolver
 * @param stats Output statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_get_stats(
    swarm_conflict_resolver_t resolver,
    conflict_stats_t* stats
);

/**
 * @brief Get active conflicts
 *
 * WHAT: Returns currently unresolved conflicts
 * WHY:  Monitor pending conflicts
 * HOW:  Returns conflicts with is_resolved = false
 *
 * @param resolver Conflict resolver
 * @param conflicts Output conflicts array
 * @param max_conflicts Maximum conflicts
 * @param count Output count
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_get_active_conflicts(
    swarm_conflict_resolver_t resolver,
    conflict_t* conflicts,
    uint32_t max_conflicts,
    uint32_t* count
);

/**
 * @brief Clear conflict history
 *
 * WHAT: Clears conflict history database
 * WHY:  Reset tracking for long-running systems
 * HOW:  Clears history but retains active conflicts
 *
 * @param resolver Conflict resolver
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_clear_history(
    swarm_conflict_resolver_t resolver
);

/**
 * @brief Get conflict by ID
 *
 * WHAT: Retrieves specific conflict by ID
 * WHY:  Query detailed conflict information
 * HOW:  Searches active and historical conflicts
 *
 * @param resolver Conflict resolver
 * @param conflict_id Conflict ID
 * @param conflict Output conflict structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_get_conflict(
    swarm_conflict_resolver_t resolver,
    uint64_t conflict_id,
    conflict_t* conflict
);

/**
 * @brief Analyze conflict patterns
 *
 * WHAT: Analyzes common conflict patterns
 * WHY:  Identify systemic issues and optimize
 * HOW:  Statistical analysis of conflict history
 *
 * @param resolver Conflict resolver
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_analyze_patterns(
    swarm_conflict_resolver_t resolver
);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *============================================================================*/

/**
 * @brief Process bio-async inbox messages
 *
 * WHAT: Processes conflict-related bio-async messages
 * WHY:  Enables event-driven conflict resolution
 * HOW:  Handles conflict detection, negotiation, and resolution messages
 *
 * @param resolver Conflict resolver
 * @return Number of messages processed
 */
uint32_t conflict_resolver_process_inbox(
    swarm_conflict_resolver_t resolver
);

/**
 * @brief Register resolver with bio-async system
 *
 * WHAT: Registers conflict resolver with bio-async router
 * WHY:  Enables message-based conflict handling
 * HOW:  Registers handlers for conflict message types
 *
 * @param resolver Conflict resolver
 * @param router Bio-async router
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t conflict_resolver_register_bioasync(
    swarm_conflict_resolver_t resolver,
    bio_router_t router
);

/*=============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get conflict type name
 *
 * WHAT: Returns string name for conflict type
 * WHY:  Human-readable logging and debugging
 * HOW:  Returns static string
 *
 * @param type Conflict type
 * @return Type name string
 */
const char* conflict_type_name(conflict_type_t type);

/**
 * @brief Get resolution strategy name
 *
 * WHAT: Returns string name for resolution strategy
 * WHY:  Human-readable logging and debugging
 * HOW:  Returns static string
 *
 * @param strategy Resolution strategy
 * @return Strategy name string
 */
const char* resolution_strategy_name(resolution_strategy_t strategy);

/**
 * @brief Print conflict details
 *
 * WHAT: Prints detailed conflict information
 * WHY:  Debugging and monitoring
 * HOW:  Formats and prints all conflict fields
 *
 * @param conflict Conflict to print
 */
void conflict_print(const conflict_t* conflict);

/**
 * @brief Print resolution result
 *
 * WHAT: Prints resolution result information
 * WHY:  Debugging and monitoring
 * HOW:  Formats and prints result fields
 *
 * @param result Result to print
 */
void resolution_result_print(const resolution_result_t* result);

/**
 * @brief Print conflict statistics
 *
 * WHAT: Prints comprehensive statistics
 * WHY:  System monitoring and analysis
 * HOW:  Formats and prints all statistics
 *
 * @param stats Statistics to print
 */
void conflict_stats_print(const conflict_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_CONFLICT_H */
