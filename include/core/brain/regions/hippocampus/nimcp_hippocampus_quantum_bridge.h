/**
 * @file nimcp_hippocampus_quantum_bridge.h
 * @brief Quantum bridge for Hippocampus integration
 *
 * WHAT: Quantum-accelerated algorithms for hippocampal memory operations
 * WHY:  Grover's algorithm provides O(sqrt(N)) speedup for memory search
 * HOW:  Wraps quantum reasoner for pattern matching and memory retrieval
 *
 * ARCHITECTURE:
 * - Integrates with hippocampus adapter for memory operations
 * - Uses quantum superposition for parallel memory search
 * - Employs Grover's algorithm for content-addressable memory
 * - Provides quantum-enhanced pattern matching
 *
 * QUANTUM APPLICATIONS:
 * - Memory Search: O(sqrt(N)) search over memory store
 * - Pattern Matching: Amplitude encoding for similarity computation
 * - Spatial Search: Quantum walk for navigation optimization
 * - Associative Recall: Superposition of memory candidates
 *
 * @version Phase H1: Hippocampus Brain Integration
 * @date 2025-12-30
 */

#ifndef NIMCP_HIPPOCAMPUS_QUANTUM_BRIDGE_H
#define NIMCP_HIPPOCAMPUS_QUANTUM_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Forward declarations */
typedef struct hippocampus_adapter hippocampus_adapter_t;
typedef struct hippocampus_quantum_bridge hippocampus_quantum_bridge_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Quantum bridge configuration
 */
typedef struct {
    bool enabled;                    /**< Enable quantum acceleration */
    uint32_t memory_search_depth;    /**< Max memories for quantum search */
    uint32_t pattern_alternatives;   /**< Candidate patterns to evaluate */
    uint32_t max_grover_iterations;  /**< Max Grover iterations */
    float min_match_confidence;      /**< Minimum match confidence */
    bool enable_interference;        /**< Enable quantum interference */
    bool use_superposition;          /**< Use superposition for parallel search */
    uint32_t seed;                   /**< RNG seed for quantum simulation */
} hippocampus_quantum_config_t;

/*=============================================================================
 * QUANTUM SEARCH RESULTS
 *===========================================================================*/

/**
 * @brief Quantum memory search candidate
 */
typedef struct {
    uint32_t memory_id;              /**< Memory identifier */
    float amplitude;                 /**< Quantum amplitude */
    float similarity_score;          /**< Feature similarity */
    float spatial_score;             /**< Spatial proximity score */
    float temporal_score;            /**< Temporal recency score */
    float combined_score;            /**< Weighted combination */
} quantum_memory_candidate_t;

/**
 * @brief Quantum memory search result
 */
typedef struct {
    quantum_memory_candidate_t* best_candidate;  /**< Best matching memory */
    uint32_t candidates_evaluated;   /**< Number evaluated */
    float satisfaction_probability;  /**< Quantum satisfaction prob */
    uint32_t grover_iterations_used; /**< Grover iterations used */
    float search_speedup;            /**< Speedup over classical */
} quantum_memory_result_t;

/**
 * @brief Quantum pattern matching candidate
 */
typedef struct {
    uint32_t pattern_id;             /**< Pattern identifier */
    float* pattern;                  /**< Pattern vector */
    uint32_t pattern_size;           /**< Pattern dimension */
    float amplitude;                 /**< Quantum amplitude */
    float completion_score;          /**< Pattern completion quality */
    bool is_match;                   /**< Above threshold? */
} quantum_pattern_candidate_t;

/**
 * @brief Quantum pattern matching result
 */
typedef struct {
    quantum_pattern_candidate_t* best_pattern;   /**< Best matching pattern */
    uint32_t patterns_evaluated;     /**< Patterns evaluated */
    float satisfaction_probability;  /**< Quantum satisfaction prob */
    uint32_t grover_iterations_used; /**< Grover iterations used */
} quantum_pattern_result_t;

/**
 * @brief Quantum spatial search candidate
 */
typedef struct {
    uint32_t location_id;            /**< Location identifier */
    float x;                         /**< X coordinate */
    float y;                         /**< Y coordinate */
    float amplitude;                 /**< Quantum amplitude */
    float path_cost;                 /**< Path cost estimate */
    float reachability;              /**< Reachability score */
} quantum_spatial_candidate_t;

/**
 * @brief Quantum spatial search result
 */
typedef struct {
    quantum_spatial_candidate_t* best_location;  /**< Best target location */
    uint32_t locations_evaluated;    /**< Locations evaluated */
    float optimization_score;        /**< Path optimization quality */
    uint32_t quantum_walk_steps;     /**< Quantum walk steps */
} quantum_spatial_result_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Quantum bridge statistics
 */
typedef struct {
    /* Search operations */
    uint64_t memory_searches;        /**< Total memory searches */
    uint64_t pattern_matches;        /**< Total pattern matches */
    uint64_t spatial_searches;       /**< Total spatial searches */

    /* Success rates */
    uint64_t successful_searches;    /**< Successful searches */
    uint64_t failed_searches;        /**< Failed searches */

    /* Performance */
    float avg_memory_speedup;        /**< Avg memory search speedup */
    float avg_pattern_speedup;       /**< Avg pattern match speedup */
    float avg_spatial_speedup;       /**< Avg spatial search speedup */
    float avg_satisfaction_prob;     /**< Avg satisfaction probability */

    /* Quantum metrics */
    uint64_t total_grover_iterations;/**< Total Grover iterations */
    float avg_grover_iterations;     /**< Avg iterations per search */
} hippocampus_quantum_stats_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default quantum bridge configuration
 *
 * @return Default configuration
 */
hippocampus_quantum_config_t hippocampus_quantum_default_config(void);

/**
 * @brief Create quantum bridge for hippocampus
 *
 * WHAT: Create bridge connecting hippocampus to quantum reasoner
 * WHY:  Enable quantum-accelerated memory operations
 * HOW:  Initialize quantum SAT solver for memory search
 *
 * @param hippocampus Hippocampus adapter handle
 * @param config Configuration (NULL for defaults)
 * @return New bridge instance, or NULL on failure
 */
hippocampus_quantum_bridge_t* hippocampus_quantum_bridge_create(
    void* hippocampus,
    const hippocampus_quantum_config_t* config
);

/**
 * @brief Destroy quantum bridge
 *
 * @param bridge Bridge to destroy
 */
void hippocampus_quantum_bridge_destroy(hippocampus_quantum_bridge_t* bridge);

/**
 * @brief Check if quantum bridge is enabled
 *
 * @param bridge Bridge instance
 * @return true if enabled
 */
bool hippocampus_quantum_bridge_is_enabled(const hippocampus_quantum_bridge_t* bridge);

/**
 * @brief Enable/disable quantum bridge
 *
 * @param bridge Bridge instance
 * @param enabled Enable state
 */
void hippocampus_quantum_bridge_set_enabled(hippocampus_quantum_bridge_t* bridge, bool enabled);

/*=============================================================================
 * QUANTUM MEMORY SEARCH API
 *===========================================================================*/

/**
 * @brief Quantum-accelerated memory search
 *
 * WHAT: Search memory store using Grover's algorithm
 * WHY:  O(sqrt(N)) speedup over classical sequential search
 * HOW:  Encode memories as quantum states, apply Grover iterations
 *
 * @param bridge Bridge instance
 * @param query_features Query feature vector
 * @param query_size Query vector size
 * @param memory_count Number of memories to search
 * @param result Output search result
 * @return 0 on success, -1 on failure
 */
int hippocampus_quantum_search_memory(
    hippocampus_quantum_bridge_t* bridge,
    const float* query_features,
    uint32_t query_size,
    uint32_t memory_count,
    quantum_memory_result_t* result
);

/**
 * @brief Quantum-accelerated pattern matching
 *
 * WHAT: Match partial pattern against stored patterns
 * WHY:  Parallel evaluation of pattern candidates
 * HOW:  Superposition of patterns, amplitude-based scoring
 *
 * @param bridge Bridge instance
 * @param partial_pattern Partial input pattern
 * @param pattern_size Pattern size
 * @param num_patterns Number of candidate patterns
 * @param result Output pattern result
 * @return 0 on success, -1 on failure
 */
int hippocampus_quantum_match_pattern(
    hippocampus_quantum_bridge_t* bridge,
    const float* partial_pattern,
    uint32_t pattern_size,
    uint32_t num_patterns,
    quantum_pattern_result_t* result
);

/**
 * @brief Quantum-accelerated spatial search
 *
 * WHAT: Find optimal location using quantum walk
 * WHY:  Faster exploration of spatial map
 * HOW:  Quantum walk over place cell graph
 *
 * @param bridge Bridge instance
 * @param current_x Current X position
 * @param current_y Current Y position
 * @param goal_x Goal X position
 * @param goal_y Goal Y position
 * @param num_locations Number of candidate locations
 * @param result Output spatial result
 * @return 0 on success, -1 on failure
 */
int hippocampus_quantum_search_spatial(
    hippocampus_quantum_bridge_t* bridge,
    float current_x,
    float current_y,
    float goal_x,
    float goal_y,
    uint32_t num_locations,
    quantum_spatial_result_t* result
);

/**
 * @brief Quantum-accelerated associative recall
 *
 * WHAT: Recall memories associated with cue using superposition
 * WHY:  Parallel activation of associated memories
 * HOW:  Amplitude encoding of association strengths
 *
 * @param bridge Bridge instance
 * @param cue Recall cue
 * @param cue_size Cue size
 * @param max_associations Maximum associations to return
 * @param memory_ids Output array of memory IDs
 * @param association_strengths Output array of strengths
 * @param count Output: number of associations found
 * @return 0 on success, -1 on failure
 */
int hippocampus_quantum_associative_recall(
    hippocampus_quantum_bridge_t* bridge,
    const float* cue,
    uint32_t cue_size,
    uint32_t max_associations,
    uint32_t* memory_ids,
    float* association_strengths,
    uint32_t* count
);

/*=============================================================================
 * STATISTICS API
 *===========================================================================*/

/**
 * @brief Get quantum bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics
 * @return 0 on success, -1 on failure
 */
int hippocampus_quantum_get_stats(
    const hippocampus_quantum_bridge_t* bridge,
    hippocampus_quantum_stats_t* stats
);

/**
 * @brief Reset quantum bridge statistics
 *
 * @param bridge Bridge instance
 */
void hippocampus_quantum_reset_stats(hippocampus_quantum_bridge_t* bridge);

/**
 * @brief Get quantum bridge configuration
 *
 * @param bridge Bridge instance
 * @param config Output configuration
 * @return 0 on success, -1 on failure
 */
int hippocampus_quantum_get_config(
    const hippocampus_quantum_bridge_t* bridge,
    hippocampus_quantum_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HIPPOCAMPUS_QUANTUM_BRIDGE_H */
