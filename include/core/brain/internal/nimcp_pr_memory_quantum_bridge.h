/**
 * @file nimcp_pr_memory_quantum_bridge.h
 * @brief PR Memory Quantum Integration - Z-Ladder + Entanglement Quantum Algorithms
 * @version 1.0.0
 * @date 2026-01-12
 *
 * Integrates quantum algorithms with Prime Resonant Memory Z-Ladder system:
 * - Quantum walk for memory resonance diffusion (sqrt(N) speedup)
 * - Quantum annealing for consolidation optimization
 * - Quantum-Shannon for entanglement bottleneck detection
 * - Quantum Monte Carlo for memory search and retrieval
 * - Quantum semantic for similarity-based recall
 *
 * Z-LADDER QUANTUM INTEGRATION:
 * - Z0 (Working): Quantum superposition for parallel candidate evaluation
 * - Z1 (Short-term): Quantum walk diffusion for fast pattern spreading
 * - Z2 (Long-term): Quantum annealing for optimal consolidation timing
 * - Z3 (Permanent): Quantum semantic search for associative recall
 *
 * ENTANGLEMENT GRAPH QUANTUM ANALYSIS:
 * - Quantum modularity detection for memory clustering
 * - Quantum walk centrality for hub identification
 * - Shannon capacity analysis for information flow bottlenecks
 *
 * MATHEMATICAL FOUNDATION:
 * - Quantum walk: O(sqrt(N)) memory search vs O(N) classical
 * - Grover search: O(sqrt(N)) for content-addressable memory
 * - Quantum annealing: Escape local minima in consolidation scheduling
 * - Shannon capacity: C = B * log2(1 + SNR) for entanglement links
 */

#ifndef NIMCP_PR_MEMORY_QUANTUM_BRIDGE_H
#define NIMCP_PR_MEMORY_QUANTUM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "core/brain/internal/nimcp_brain_pr_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define PR_QUANTUM_WALK_STEPS           100     /* Quantum walk evolution steps */
#define PR_QUANTUM_GROVER_ITERATIONS    10      /* Grover search iterations */
#define PR_QUANTUM_MC_SAMPLES           1000    /* Monte Carlo samples */
#define PR_QUANTUM_ANNEAL_ITERATIONS    200     /* Annealing iterations */
#define PR_QUANTUM_BOTTLENECK_THRESHOLD 0.5f    /* Bottleneck detection threshold */

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

typedef struct pr_memory_quantum_ctx_internal* pr_memory_quantum_ctx_t;

/*=============================================================================
 * QUANTUM SEARCH STRUCTURES
 *===========================================================================*/

/**
 * @brief Quantum memory search candidate
 */
typedef struct {
    uint64_t resonance_signature;       /**< Memory resonance signature */
    int z_level;                        /**< Z-Ladder level (0-3) */
    float amplitude;                    /**< Quantum amplitude */
    float similarity_score;             /**< Pattern similarity */
    float resonance_strength;           /**< Resonance strength */
    float temporal_score;               /**< Recency/temporal score */
    float combined_score;               /**< Weighted combination */
} pr_quantum_candidate_t;

/**
 * @brief Quantum memory search result
 */
typedef struct {
    pr_quantum_candidate_t* best_match; /**< Best matching memory */
    uint32_t candidates_evaluated;      /**< Number evaluated */
    float satisfaction_probability;     /**< Quantum satisfaction probability */
    uint32_t grover_iterations_used;    /**< Grover iterations used */
    float search_speedup;               /**< Speedup vs classical */
    uint32_t z_level_distribution[4];   /**< Candidates per Z-level */
} pr_quantum_search_result_t;

/**
 * @brief Quantum consolidation decision
 */
typedef struct {
    uint64_t resonance_signature;       /**< Memory to consolidate */
    int from_level;                     /**< Current Z-level */
    int to_level;                       /**< Target Z-level */
    float promotion_probability;        /**< Probability of promotion */
    float optimal_timing_ms;            /**< Optimal consolidation time */
    float energy;                       /**< Objective function value */
} pr_quantum_consolidation_t;

/*=============================================================================
 * ENTANGLEMENT QUANTUM STRUCTURES
 *===========================================================================*/

/**
 * @brief Entanglement graph bottleneck
 */
typedef struct {
    uint64_t from_signature;            /**< Source memory */
    uint64_t to_signature;              /**< Target memory */
    float entangle_strength;            /**< Current entanglement strength */
    float capacity;                     /**< Shannon capacity bits/s */
    float demand;                       /**< Information demand bits/s */
    float deficit;                      /**< (demand - capacity) / demand */
    float suggested_strength;           /**< Recommended strength */
} pr_entangle_bottleneck_t;

/**
 * @brief Quantum community detection result
 */
typedef struct {
    uint32_t* community_assignments;    /**< Memory -> community mapping */
    uint32_t num_communities;           /**< Number of detected communities */
    float modularity_score;             /**< Modularity Q score */
    float* community_sizes;             /**< Size of each community */
    float quantum_speedup;              /**< Speedup achieved */
} pr_quantum_community_t;

/**
 * @brief Quantum hub detection result
 */
typedef struct {
    uint64_t* hub_signatures;           /**< Hub memory signatures */
    float* centrality_scores;           /**< Centrality for each hub */
    uint32_t num_hubs;                  /**< Number of hubs found */
    float avg_centrality;               /**< Average hub centrality */
} pr_quantum_hubs_t;

/*=============================================================================
 * METRICS STRUCTURES
 *===========================================================================*/

/**
 * @brief PR Memory quantum bridge metrics
 */
typedef struct {
    /* Quantum search */
    uint64_t quantum_searches;          /**< Total quantum searches */
    float avg_search_speedup;           /**< Average speedup achieved */
    float avg_satisfaction_prob;        /**< Average satisfaction probability */
    uint64_t grover_iterations_total;   /**< Total Grover iterations */

    /* Quantum consolidation */
    uint64_t quantum_consolidations;    /**< Quantum-guided consolidations */
    uint64_t promotions_optimized;      /**< Promotions via quantum */
    float avg_consolidation_energy;     /**< Average objective value */
    uint64_t tunneling_events;          /**< Quantum tunneling events */

    /* Entanglement analysis */
    uint64_t entangle_analyses;         /**< Entanglement analyses performed */
    uint32_t bottlenecks_detected;      /**< Total bottlenecks found */
    float avg_bottleneck_severity;      /**< Average severity */
    float information_efficiency;       /**< Current info efficiency */

    /* Community detection */
    uint64_t community_detections;      /**< Community detections run */
    float avg_modularity;               /**< Average modularity score */
    float avg_num_communities;          /**< Average communities found */

    /* Quantum walk */
    uint64_t diffusion_operations;      /**< Quantum walk operations */
    float avg_diffusion_speedup;        /**< Average diffusion speedup */

    /* Monte Carlo */
    uint64_t mc_samples_total;          /**< Total MC samples */
    float avg_mc_variance;              /**< Average estimation variance */

    /* Performance */
    double total_processing_time_ms;    /**< Total processing time */
    double avg_search_latency_us;       /**< Average search latency */

    /* Timestamp */
    uint64_t last_update_ms;
} pr_quantum_metrics_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief PR Memory quantum bridge configuration
 */
typedef struct {
    /* Feature toggles */
    bool enable_quantum_search;
    bool enable_quantum_consolidation;
    bool enable_quantum_entangle_analysis;
    bool enable_quantum_communities;
    bool enable_quantum_walk;

    /* Quantum search */
    uint32_t grover_max_iterations;
    uint32_t search_candidates_limit;
    float min_similarity_threshold;

    /* Quantum consolidation */
    float anneal_initial_temp;
    float anneal_final_temp;
    float tunneling_rate;
    uint32_t anneal_iterations;
    uint32_t consolidation_objective;   /**< 0=speed, 1=quality, 2=balance */

    /* Entanglement analysis */
    float bottleneck_threshold;
    uint32_t entangle_sample_size;
    uint32_t shannon_update_interval;

    /* Community detection */
    uint32_t community_iterations;
    float resolution_parameter;         /**< Modularity resolution */

    /* Quantum walk */
    uint32_t walk_steps;
    float decoherence_rate;
    uint32_t coin_type;                 /**< HADAMARD=0, GROVER=1, FOURIER=2 */

    /* Monte Carlo */
    uint32_t mc_samples;
    bool use_importance_sampling;

    /* Metrics */
    bool enable_metrics;
    uint32_t metrics_flush_interval_ms;
    char metrics_output_dir[256];
} pr_quantum_config_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default quantum bridge configuration
 */
pr_quantum_config_t pr_quantum_default_config(void);

/**
 * @brief Create PR Memory quantum bridge context
 */
pr_memory_quantum_ctx_t pr_quantum_create(const pr_quantum_config_t* config);

/**
 * @brief Destroy PR Memory quantum bridge context
 */
void pr_quantum_destroy(pr_memory_quantum_ctx_t ctx);

/**
 * @brief Reset quantum bridge context
 */
void pr_quantum_reset(pr_memory_quantum_ctx_t ctx);

/**
 * @brief Attach to brain PR memory system
 */
bool pr_quantum_attach(pr_memory_quantum_ctx_t ctx, struct brain_struct* brain);

/**
 * @brief Check if quantum bridge is enabled
 */
bool pr_quantum_is_enabled(const pr_memory_quantum_ctx_t ctx);

/**
 * @brief Enable/disable quantum bridge
 */
void pr_quantum_set_enabled(pr_memory_quantum_ctx_t ctx, bool enabled);

/*=============================================================================
 * QUANTUM SEARCH API
 *===========================================================================*/

/**
 * @brief Quantum-accelerated memory search using Grover's algorithm
 *
 * Achieves O(sqrt(N)) speedup for content-addressable memory search.
 *
 * @param ctx Quantum bridge context
 * @param query_pattern Query pattern to search for
 * @param pattern_size Size of query pattern
 * @param z_levels_mask Bitmask of Z-levels to search (0xF = all)
 * @param result Output search result
 * @return true on success
 */
bool pr_quantum_search_memory(
    pr_memory_quantum_ctx_t ctx,
    const void* query_pattern,
    size_t pattern_size,
    uint8_t z_levels_mask,
    pr_quantum_search_result_t* result
);

/**
 * @brief Quantum similarity search with amplitude encoding
 *
 * Encodes patterns as quantum amplitudes for parallel similarity computation.
 *
 * @param ctx Quantum bridge context
 * @param query_pattern Query pattern
 * @param pattern_size Pattern size
 * @param max_results Maximum results to return
 * @param results Output array of candidates
 * @param num_found Number of matches found
 * @return true on success
 */
bool pr_quantum_similarity_search(
    pr_memory_quantum_ctx_t ctx,
    const void* query_pattern,
    size_t pattern_size,
    uint32_t max_results,
    pr_quantum_candidate_t* results,
    uint32_t* num_found
);

/**
 * @brief Quantum associative recall using superposition
 *
 * @param ctx Quantum bridge context
 * @param cue Memory cue
 * @param cue_size Cue size
 * @param max_associations Maximum associations
 * @param associated_signatures Output array of signatures
 * @param association_strengths Output array of strengths
 * @param num_found Number found
 * @return true on success
 */
bool pr_quantum_associative_recall(
    pr_memory_quantum_ctx_t ctx,
    const void* cue,
    size_t cue_size,
    uint32_t max_associations,
    uint64_t* associated_signatures,
    float* association_strengths,
    uint32_t* num_found
);

/*=============================================================================
 * QUANTUM CONSOLIDATION API
 *===========================================================================*/

/**
 * @brief Quantum-optimized consolidation scheduling
 *
 * Uses quantum annealing to find optimal promotion timing and targets.
 *
 * @param ctx Quantum bridge context
 * @param candidates Candidate memories for consolidation
 * @param num_candidates Number of candidates
 * @param decisions Output consolidation decisions
 * @param max_decisions Maximum decisions
 * @param num_decisions Number of decisions made
 * @return true on success
 */
bool pr_quantum_optimize_consolidation(
    pr_memory_quantum_ctx_t ctx,
    const pr_quantum_candidate_t* candidates,
    uint32_t num_candidates,
    pr_quantum_consolidation_t* decisions,
    uint32_t max_decisions,
    uint32_t* num_decisions
);

/**
 * @brief Quantum-guided theta-gamma consolidation tick
 *
 * @param ctx Quantum bridge context
 * @param brain Brain structure
 * @param theta_phase Current theta phase
 * @param gamma_amplitude Current gamma amplitude
 * @param current_time_us Current time in microseconds
 * @return Number of memories consolidated
 */
uint32_t pr_quantum_consolidation_tick(
    pr_memory_quantum_ctx_t ctx,
    struct brain_struct* brain,
    float theta_phase,
    float gamma_amplitude,
    uint64_t current_time_us
);

/**
 * @brief Get annealing state for consolidation optimizer
 */
bool pr_quantum_get_anneal_state(
    pr_memory_quantum_ctx_t ctx,
    float* temperature,
    float* tunneling_prob,
    uint64_t* iteration
);

/*=============================================================================
 * ENTANGLEMENT ANALYSIS API (Quantum-Shannon)
 *===========================================================================*/

/**
 * @brief Analyze information flow through entanglement graph
 *
 * @param ctx Quantum bridge context
 * @param brain Brain structure
 * @return Information efficiency [0, 1]
 */
float pr_quantum_analyze_entanglement_flow(
    pr_memory_quantum_ctx_t ctx,
    struct brain_struct* brain
);

/**
 * @brief Detect entanglement bottlenecks
 *
 * @param ctx Quantum bridge context
 * @param brain Brain structure
 * @param bottlenecks Output bottleneck array
 * @param max_bottlenecks Maximum to return
 * @param num_found Number found
 * @return true on success
 */
bool pr_quantum_detect_entangle_bottlenecks(
    pr_memory_quantum_ctx_t ctx,
    struct brain_struct* brain,
    pr_entangle_bottleneck_t* bottlenecks,
    uint32_t max_bottlenecks,
    uint32_t* num_found
);

/**
 * @brief Optimize entanglement strengths based on Shannon analysis
 *
 * @param ctx Quantum bridge context
 * @param brain Brain structure
 * @param adaptation_rate Learning rate for strength adjustments
 * @return Number of entanglements adjusted
 */
uint32_t pr_quantum_optimize_entanglements(
    pr_memory_quantum_ctx_t ctx,
    struct brain_struct* brain,
    float adaptation_rate
);

/*=============================================================================
 * COMMUNITY DETECTION API
 *===========================================================================*/

/**
 * @brief Quantum-accelerated memory community detection
 *
 * Uses quantum modularity optimization to cluster related memories.
 *
 * @param ctx Quantum bridge context
 * @param brain Brain structure
 * @param result Output community result
 * @return true on success
 */
bool pr_quantum_detect_communities(
    pr_memory_quantum_ctx_t ctx,
    struct brain_struct* brain,
    pr_quantum_community_t* result
);

/**
 * @brief Free community detection result
 */
void pr_quantum_free_community_result(pr_quantum_community_t* result);

/**
 * @brief Identify memory hub nodes
 *
 * @param ctx Quantum bridge context
 * @param brain Brain structure
 * @param result Output hub detection result
 * @return true on success
 */
bool pr_quantum_find_memory_hubs(
    pr_memory_quantum_ctx_t ctx,
    struct brain_struct* brain,
    pr_quantum_hubs_t* result
);

/**
 * @brief Free hub detection result
 */
void pr_quantum_free_hubs_result(pr_quantum_hubs_t* result);

/*=============================================================================
 * QUANTUM WALK DIFFUSION API
 *===========================================================================*/

/**
 * @brief Diffuse resonance using quantum walk
 *
 * Spreads resonance signal through memory graph with sqrt(N) speedup.
 *
 * @param ctx Quantum bridge context
 * @param brain Brain structure
 * @param source_signature Source memory signature
 * @param initial_resonance Initial resonance strength
 * @param diffused_resonances Output resonance per memory
 * @param max_memories Maximum memories in output
 * @param num_affected Number of memories affected
 * @return true on success
 */
bool pr_quantum_diffuse_resonance(
    pr_memory_quantum_ctx_t ctx,
    struct brain_struct* brain,
    uint64_t source_signature,
    float initial_resonance,
    float* diffused_resonances,
    uint32_t max_memories,
    uint32_t* num_affected
);

/**
 * @brief Multi-source resonance diffusion
 */
bool pr_quantum_diffuse_resonance_multi(
    pr_memory_quantum_ctx_t ctx,
    struct brain_struct* brain,
    const uint64_t* source_signatures,
    const float* initial_resonances,
    uint32_t num_sources,
    float* diffused_resonances,
    uint32_t max_memories,
    uint32_t* num_affected
);

/*=============================================================================
 * ENHANCED PR MEMORY OPERATIONS
 *===========================================================================*/

/**
 * @brief Enhanced memory store with quantum optimization
 *
 * @param ctx Quantum bridge context
 * @param brain Brain structure
 * @param content Memory content
 * @param content_size Content size
 * @param resonance_strength Initial resonance
 * @param signature_out Output signature
 * @return true on success
 */
bool pr_quantum_store_enhanced(
    pr_memory_quantum_ctx_t ctx,
    struct brain_struct* brain,
    const void* content,
    size_t content_size,
    float resonance_strength,
    uint64_t* signature_out
);

/**
 * @brief Enhanced memory retrieve with quantum search
 *
 * @param ctx Quantum bridge context
 * @param brain Brain structure
 * @param query Query pattern (partial or full)
 * @param query_size Query size
 * @param content_out Output content buffer
 * @param max_size Maximum content size
 * @param signature_out Output signature found
 * @param strength_out Output resonance strength
 * @return true on success
 */
bool pr_quantum_retrieve_enhanced(
    pr_memory_quantum_ctx_t ctx,
    struct brain_struct* brain,
    const void* query,
    size_t query_size,
    void* content_out,
    size_t max_size,
    uint64_t* signature_out,
    float* strength_out
);

/**
 * @brief Full learning tick with all quantum integration
 *
 * Performs: quantum search optimization, consolidation scheduling,
 * entanglement analysis, community updates, metrics collection.
 *
 * @param ctx Quantum bridge context
 * @param brain Brain structure
 * @param current_time_us Current time in microseconds
 * @return Number of operations performed
 */
uint32_t pr_quantum_tick(
    pr_memory_quantum_ctx_t ctx,
    struct brain_struct* brain,
    uint64_t current_time_us
);

/*=============================================================================
 * METRICS API
 *===========================================================================*/

/**
 * @brief Get comprehensive metrics snapshot
 */
bool pr_quantum_get_metrics(pr_memory_quantum_ctx_t ctx, pr_quantum_metrics_t* metrics);

/**
 * @brief Flush metrics to disk
 */
int32_t pr_quantum_flush_metrics(pr_memory_quantum_ctx_t ctx);

/**
 * @brief Export metrics to CSV
 */
bool pr_quantum_export_csv(pr_memory_quantum_ctx_t ctx, const char* filename);

/**
 * @brief Export metrics to JSON
 */
bool pr_quantum_export_json(pr_memory_quantum_ctx_t ctx, const char* filename);

/**
 * @brief Reset metrics counters
 */
void pr_quantum_reset_metrics(pr_memory_quantum_ctx_t ctx);

/*=============================================================================
 * DIAGNOSTIC API
 *===========================================================================*/

/**
 * @brief Print quantum bridge status
 */
void pr_quantum_print_status(const pr_memory_quantum_ctx_t ctx);

/**
 * @brief Verify quantum bridge integrity
 */
bool pr_quantum_verify(const pr_memory_quantum_ctx_t ctx);

/**
 * @brief Get feature availability flags
 */
uint32_t pr_quantum_get_features(const pr_memory_quantum_ctx_t ctx);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PR_MEMORY_QUANTUM_BRIDGE_H */
