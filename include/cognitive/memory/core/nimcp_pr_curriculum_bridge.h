//=============================================================================
// nimcp_pr_curriculum_bridge.h - Prime Resonant Curriculum Learning Bridge
//=============================================================================
/**
 * @file nimcp_pr_curriculum_bridge.h
 * @brief Bridge between Prime Resonant memory system and curriculum learning
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Bridge connecting Prime Resonant memory (resonance, entanglement, quaternion,
 *       Z-ladder) with curriculum learning strategies for optimized training
 * WHY:  Memory consolidation patterns should guide curriculum design, and
 *       curriculum difficulty should adapt based on memory resonance signals
 * HOW:  Bidirectional integration where:
 *       - Memory resonance guides sample ordering (resonate = easier)
 *       - Entanglement density informs difficulty estimation
 *       - Consolidation rates pace curriculum progression
 *       - Memory tier distribution shapes scheduling strategy
 *       - Novelty/curiosity drives exploration of low-resonance samples
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Curriculum Learning <-> Memory Integration:
 *   +-----------------------------------------------------------------------+
 *   |                                                                        |
 *   |  RESONANCE-GUIDED ORDERING:                                           |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  High resonance samples -> Familiar/Easier -> Present first    |   |
 *   |  |  Low resonance samples  -> Novel/Harder   -> Present later     |   |
 *   |  |                                                                 |   |
 *   |  |  Scaffolding: Build on existing knowledge structures            |   |
 *   |  |  Transfer: Leverage prior memories for new learning             |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  DIFFICULTY FROM ENTANGLEMENT:                                        |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  Dense entanglement   -> Well-connected concept -> Easier      |   |
 *   |  |  Sparse entanglement  -> Isolated concept      -> Harder       |   |
 *   |  |                                                                 |   |
 *   |  |  Difficulty = f(1 / entanglement_degree)                       |   |
 *   |  |  Highly connected memories are easier to retrieve/apply        |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  CONSOLIDATION-BASED PACING:                                          |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  High consolidation rate -> Learning is happening -> Speed up  |   |
 *   |  |  Low consolidation rate  -> Struggling           -> Slow down  |   |
 *   |  |                                                                 |   |
 *   |  |  Monitor Z-ladder promotions to gauge learning velocity        |   |
 *   |  |  Pace = f(recent_promotions / recent_demotions)                |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  TIER-AWARE SCHEDULING:                                               |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  Z0 heavy -> Too much new info    -> Review more               |   |
 *   |  |  Z1 heavy -> Good short-term      -> Introduce new moderately  |   |
 *   |  |  Z2 heavy -> Strong consolidation -> Can challenge more        |   |
 *   |  |  Z3 heavy -> Rich knowledge base  -> Focus on transfer         |   |
 *   |  |                                                                 |   |
 *   |  |  Balance: Mix new material with review based on distribution   |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  CURIOSITY-DRIVEN SELECTION:                                          |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  Curiosity = 1 - max_resonance_with_any_memory                 |   |
 *   |  |  High curiosity -> Novel sample  -> Explore                    |   |
 *   |  |  Low curiosity  -> Familiar     -> Exploit                     |   |
 *   |  |                                                                 |   |
 *   |  |  Balance exploration/exploitation for optimal learning         |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   +-----------------------------------------------------------------------+
 *
 *   Curriculum Strategies:
 *   +-----------------------------------------------------------------------+
 *   | Strategy     | Description                        | When to Use       |
 *   |--------------|------------------------------------|--------------------|
 *   | RESONANCE    | Order by memory resonance          | Scaffolding        |
 *   | ENTANGLEMENT | Order by connection density        | Concept building   |
 *   | CONSOLIDATION| Pace by consolidation rate         | Adaptive learning  |
 *   | CURIOSITY    | Select novel/surprising samples    | Exploration        |
 *   | HYBRID       | Weighted combination of all        | General purpose    |
 *   +-----------------------------------------------------------------------+
 *
 *   Curriculum Pipeline:
 *   +-----------------------------------------------------------------------+
 *   |  Sample Pool -> Difficulty Assessment -> Ordering -> Batching -> Out  |
 *   |       |               |                    |            |              |
 *   |       v               v                    v            v              |
 *   |  [Features]     [Resonance +         [Sort by      [Select N    ]     |
 *   |                  Entangle +           difficulty    with tier    ]     |
 *   |                  Curiosity]           or strategy   balance     ]     |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Difficulty computation: ~100ns per sample (with cached resonance)
 * - Batch ordering: O(n log n) sorting
 * - Batch selection: O(n) linear scan with tier balancing
 * - Full update cycle: ~10ms for 1000 samples
 *
 * MEMORY:
 * - pr_curriculum_bridge_t: ~4KB base + sample cache
 * - Per-sample overhead: ~32 bytes (difficulty cache)
 *
 * INTEGRATION:
 * - Core: Resonance engine, entanglement graph, Z-ladder
 * - Plasticity: Learning rate derived from curriculum difficulty
 * - Training: Sample ordering, batch composition
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PR_CURRICULUM_BRIDGE_H
#define NIMCP_PR_CURRICULUM_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Prime Resonant core dependencies */
#include "nimcp_resonance.h"
#include "nimcp_quaternion.h"
#include "nimcp_entanglement.h"
#include "nimcp_z_ladder.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Export macro (for shared library builds) */
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of samples to track in curriculum cache */
#define PR_CURRICULUM_MAX_SAMPLES              65536

/** Default batch size for curriculum learning */
#define PR_CURRICULUM_DEFAULT_BATCH_SIZE       32

/** Default novelty weight in hybrid strategy */
#define PR_CURRICULUM_DEFAULT_NOVELTY_WEIGHT   0.3f

/** Default difficulty scaling factor */
#define PR_CURRICULUM_DEFAULT_DIFFICULTY_SCALE 1.0f

/** Default consolidation threshold for pacing */
#define PR_CURRICULUM_DEFAULT_CONSOL_THRESHOLD 0.5f

/** Maximum curriculum difficulty value */
#define PR_CURRICULUM_MAX_DIFFICULTY           1.0f

/** Minimum curriculum difficulty value */
#define PR_CURRICULUM_MIN_DIFFICULTY           0.0f

/** Default pacing multiplier for fast consolidation */
#define PR_CURRICULUM_FAST_PACING              1.5f

/** Default pacing multiplier for slow consolidation */
#define PR_CURRICULUM_SLOW_PACING              0.5f

/** Curiosity score threshold for exploration */
#define PR_CURRICULUM_CURIOSITY_THRESHOLD      0.7f

/** Number of memory tiers (matching Z-ladder) */
#define PR_CURRICULUM_NUM_TIERS                4

/** Maximum history for consolidation rate tracking */
#define PR_CURRICULUM_MAX_HISTORY              1000

/** Epsilon for floating-point comparisons */
#define PR_CURRICULUM_EPSILON                  1e-6f

/** Default tier balance weights */
#define PR_CURRICULUM_TIER_WEIGHT_Z0           0.2f
#define PR_CURRICULUM_TIER_WEIGHT_Z1           0.3f
#define PR_CURRICULUM_TIER_WEIGHT_Z2           0.3f
#define PR_CURRICULUM_TIER_WEIGHT_Z3           0.2f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Curriculum learning strategy types
 *
 * WHAT: Strategy for ordering and selecting samples
 * WHY:  Different strategies optimize for different learning goals
 */
typedef enum {
    PR_CURRICULUM_RESONANCE = 0,    /**< Order by resonance with memory (easy first) */
    PR_CURRICULUM_ENTANGLEMENT,     /**< Order by entanglement density */
    PR_CURRICULUM_CONSOLIDATION,    /**< Pace by consolidation rate */
    PR_CURRICULUM_CURIOSITY,        /**< Select novel/low-resonance samples */
    PR_CURRICULUM_HYBRID,           /**< Weighted combination of all strategies */
    PR_CURRICULUM_TYPE_COUNT        /**< Number of curriculum types */
} pr_curriculum_type_t;

/**
 * @brief Difficulty estimation method
 *
 * WHAT: How to compute sample difficulty
 * WHY:  Different difficulty metrics suit different domains
 */
typedef enum {
    PR_DIFFICULTY_RESONANCE = 0,    /**< Difficulty = 1 - max_resonance */
    PR_DIFFICULTY_ENTANGLEMENT,     /**< Difficulty = 1 / (1 + entangle_degree) */
    PR_DIFFICULTY_QUATERNION,       /**< Difficulty from semantic distance */
    PR_DIFFICULTY_COMPOSITE,        /**< Weighted combination */
    PR_DIFFICULTY_EXTERNAL          /**< Use externally provided difficulty */
} pr_difficulty_method_t;

/**
 * @brief Sample representation for curriculum
 *
 * WHAT: A training sample with its curriculum metadata
 * WHY:  Track samples through curriculum pipeline
 *
 * Memory: ~64 bytes per sample
 */
typedef struct {
    uint64_t sample_id;             /**< Unique sample identifier */
    prime_signature_t* signature;   /**< Content signature for resonance */
    nimcp_quaternion_t quaternion;  /**< Semantic state */
    float phase;                    /**< Current oscillation phase */
    void* data;                     /**< Pointer to actual sample data */
    size_t data_size;               /**< Size of sample data */
    float external_difficulty;      /**< Externally assigned difficulty [0,1] */
    uint32_t presentation_count;    /**< Times this sample has been presented */
    uint64_t last_presented_ms;     /**< Last presentation timestamp */
} pr_curriculum_sample_t;

/**
 * @brief Computed difficulty for a sample
 *
 * WHAT: Difficulty breakdown by component
 * WHY:  Enable analysis of why samples are considered easy/hard
 */
typedef struct {
    uint64_t sample_id;             /**< Sample identifier */
    float total_difficulty;         /**< Combined difficulty score [0,1] */
    float resonance_difficulty;     /**< From resonance (1 - max_res) */
    float entanglement_difficulty;  /**< From entanglement density */
    float quaternion_difficulty;    /**< From semantic distance */
    float curiosity_score;          /**< Novelty/curiosity score [0,1] */
    float confidence;               /**< Confidence in difficulty estimate */
    uint32_t resonance_count;       /**< Number of memories resonated with */
} pr_difficulty_result_t;

/**
 * @brief Curriculum pacing information
 *
 * WHAT: Current learning velocity and recommended pacing
 * WHY:  Adapt curriculum speed to learner performance
 */
typedef struct {
    float consolidation_rate;       /**< Recent consolidation rate (promotions/time) */
    float retention_rate;           /**< Recent retention rate (1 - demotion_rate) */
    float pacing_multiplier;        /**< Recommended speed multiplier */
    uint32_t recent_promotions;     /**< Promotions in recent window */
    uint32_t recent_demotions;      /**< Demotions in recent window */
    float z0_pressure;              /**< Working memory pressure (0-1) */
    float z1_throughput;            /**< Short-term throughput */
    uint64_t window_ms;             /**< Time window for measurements */
} pr_curriculum_pacing_t;

/**
 * @brief Tier distribution in memory
 *
 * WHAT: Current distribution of memories across Z-ladder tiers
 * WHY:  Guide curriculum strategy based on knowledge state
 */
typedef struct {
    uint32_t counts[PR_CURRICULUM_NUM_TIERS];  /**< Node count per tier */
    float proportions[PR_CURRICULUM_NUM_TIERS]; /**< Normalized proportions */
    float avg_strength[PR_CURRICULUM_NUM_TIERS]; /**< Average strength per tier */
    uint32_t total_nodes;                       /**< Total nodes across all tiers */
    float balance_score;                        /**< How balanced the distribution is */
} pr_tier_distribution_t;

/**
 * @brief Batch selection result
 *
 * WHAT: Result of selecting a batch of samples
 * WHY:  Package batch with metadata for training loop
 */
typedef struct {
    uint64_t* sample_ids;           /**< Selected sample IDs (caller-allocated) */
    uint32_t batch_size;            /**< Number of samples selected */
    float avg_difficulty;           /**< Average difficulty of batch */
    float min_difficulty;           /**< Minimum difficulty in batch */
    float max_difficulty;           /**< Maximum difficulty in batch */
    float exploration_ratio;        /**< Fraction that are novel/curious */
    uint32_t tier_representation[PR_CURRICULUM_NUM_TIERS]; /**< Samples per tier affinity */
} pr_batch_result_t;

/**
 * @brief Step result for curriculum update
 *
 * WHAT: Feedback from a training step to update curriculum
 * WHY:  Enable curriculum to adapt based on learning outcomes
 */
typedef struct {
    uint64_t sample_id;             /**< Sample that was processed */
    float loss;                     /**< Training loss for this sample */
    float gradient_norm;            /**< Gradient magnitude (difficulty proxy) */
    bool correct;                   /**< For classification: was prediction correct */
    float confidence;               /**< Model confidence in prediction */
    uint64_t processing_time_us;    /**< Time to process sample */
} pr_step_result_t;

/**
 * @brief Curriculum bridge configuration
 *
 * WHAT: Complete configuration for curriculum bridge
 * WHY:  Centralize all configuration options
 */
typedef struct {
    pr_curriculum_type_t type;      /**< Primary curriculum strategy */
    pr_difficulty_method_t difficulty_method; /**< How to compute difficulty */

    /* Difficulty parameters */
    float difficulty_scale;         /**< Scale factor for difficulty scores */
    float novelty_weight;           /**< Weight of novelty in hybrid strategy */
    float resonance_weight;         /**< Weight of resonance in difficulty */
    float entanglement_weight;      /**< Weight of entanglement in difficulty */
    float quaternion_weight;        /**< Weight of quaternion distance */

    /* Pacing parameters */
    float consolidation_threshold;  /**< Consolidation rate for "good" learning */
    float fast_pace_multiplier;     /**< Speed multiplier when learning well */
    float slow_pace_multiplier;     /**< Speed multiplier when struggling */
    uint64_t pacing_window_ms;      /**< Time window for pacing measurements */

    /* Batch parameters */
    uint32_t default_batch_size;    /**< Default batch size */
    float exploration_epsilon;      /**< Probability of exploration selection */
    float tier_balance_weights[PR_CURRICULUM_NUM_TIERS]; /**< Tier representation goals */

    /* Ordering parameters */
    bool ascending_difficulty;      /**< True = easy first, False = hard first */
    float difficulty_jitter;        /**< Random jitter to prevent overfitting order */

    /* Caching */
    bool enable_difficulty_cache;   /**< Cache difficulty computations */
    uint64_t cache_ttl_ms;          /**< Cache time-to-live */

    /* Integration */
    bool enable_plasticity_feedback; /**< Feed difficulty to plasticity bridge */
    bool enable_event_logging;      /**< Log curriculum events */
    uint32_t max_events;            /**< Maximum events to store */
} pr_curriculum_config_t;

/**
 * @brief Curriculum event types
 *
 * WHAT: Types of events logged by curriculum bridge
 * WHY:  Track curriculum decisions for analysis
 */
typedef enum {
    PR_CURRICULUM_EVENT_BATCH_SELECTED = 0, /**< Batch was selected */
    PR_CURRICULUM_EVENT_SAMPLE_PRESENTED,   /**< Sample was presented */
    PR_CURRICULUM_EVENT_DIFFICULTY_COMPUTED, /**< Difficulty was computed */
    PR_CURRICULUM_EVENT_PACING_ADJUSTED,    /**< Pacing was adjusted */
    PR_CURRICULUM_EVENT_STRATEGY_CHANGED,   /**< Strategy was changed */
    PR_CURRICULUM_EVENT_TYPE_COUNT
} pr_curriculum_event_type_t;

/**
 * @brief Curriculum event record
 *
 * WHAT: Record of a single curriculum event
 * WHY:  Track decisions for debugging and analysis
 */
typedef struct {
    pr_curriculum_event_type_t type; /**< Event type */
    uint64_t timestamp_ms;          /**< When event occurred */
    uint64_t sample_id;             /**< Relevant sample ID (if applicable) */
    float difficulty;               /**< Difficulty at event time */
    float pacing;                   /**< Pacing multiplier at event time */
    pr_curriculum_type_t strategy;  /**< Active strategy */
} pr_curriculum_event_t;

/**
 * @brief Curriculum bridge statistics
 *
 * WHAT: Operational metrics for curriculum bridge
 * WHY:  Monitor bridge health and effectiveness
 */
typedef struct {
    /* Sample statistics */
    uint64_t samples_presented;     /**< Total samples presented */
    uint64_t batches_selected;      /**< Total batches selected */
    uint64_t difficulty_computations; /**< Difficulty computations performed */
    uint64_t cache_hits;            /**< Difficulty cache hits */
    uint64_t cache_misses;          /**< Difficulty cache misses */

    /* Difficulty statistics */
    float avg_difficulty;           /**< Average presented difficulty */
    float min_difficulty;           /**< Minimum presented difficulty */
    float max_difficulty;           /**< Maximum presented difficulty */
    float difficulty_variance;      /**< Variance of presented difficulties */

    /* Pacing statistics */
    float avg_pacing;               /**< Average pacing multiplier */
    uint32_t pace_increases;        /**< Times pacing increased */
    uint32_t pace_decreases;        /**< Times pacing decreased */

    /* Curriculum effectiveness */
    float curriculum_efficiency;    /**< Samples to mastery metric */
    float exploration_ratio;        /**< Actual exploration ratio */
    float tier_balance_achieved;    /**< How well tier goals are met */

    /* Performance */
    float avg_batch_select_time_us; /**< Average batch selection time */
    float avg_difficulty_time_us;   /**< Average difficulty computation time */
    uint64_t total_update_time_us;  /**< Total time in updates */
} pr_curriculum_stats_t;

/**
 * @brief Opaque curriculum bridge handle
 */
typedef struct pr_curriculum_bridge_struct* pr_curriculum_bridge_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default curriculum bridge configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides biologically-plausible starting point
 *
 * @return Default configuration with:
 *         - Type: HYBRID
 *         - Difficulty method: COMPOSITE
 *         - Batch size: 32
 *         - Novelty weight: 0.3
 *         - Ascending difficulty: true
 *
 * Performance: ~10ns
 *
 * Example:
 *   pr_curriculum_config_t config = pr_curriculum_config_default();
 *   config.type = PR_CURRICULUM_CURIOSITY;
 *   pr_curriculum_bridge_t bridge = pr_curriculum_bridge_create(&config);
 */
NIMCP_EXPORT pr_curriculum_config_t pr_curriculum_config_default(void);

/**
 * @brief Get resonance-focused configuration
 *
 * WHAT: Configuration emphasizing resonance-based ordering
 * WHY:  For scaffolded learning building on existing knowledge
 *
 * @return Config optimized for resonance-guided curriculum
 */
NIMCP_EXPORT pr_curriculum_config_t pr_curriculum_config_resonance(void);

/**
 * @brief Get curiosity-driven configuration
 *
 * WHAT: Configuration emphasizing novelty exploration
 * WHY:  For exploration-heavy learning phases
 *
 * @return Config optimized for curiosity-driven selection
 */
NIMCP_EXPORT pr_curriculum_config_t pr_curriculum_config_curiosity(void);

/**
 * @brief Get adaptive pacing configuration
 *
 * WHAT: Configuration emphasizing consolidation-based pacing
 * WHY:  For self-paced adaptive learning
 *
 * @return Config optimized for adaptive pacing
 */
NIMCP_EXPORT pr_curriculum_config_t pr_curriculum_config_adaptive(void);

/**
 * @brief Validate curriculum bridge configuration
 *
 * WHAT: Check configuration for validity
 * WHY:  Prevent invalid parameters causing runtime errors
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Validation rules:
 * - Weights must be non-negative
 * - Batch size must be > 0
 * - Thresholds must be in valid ranges
 *
 * Performance: ~50ns
 */
NIMCP_EXPORT bool pr_curriculum_config_validate(const pr_curriculum_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create curriculum bridge
 *
 * WHAT: Initialize curriculum bridge for Prime Resonant memory
 * WHY:  Entry point for curriculum learning integration
 * HOW:  Allocate state, initialize caches, connect to memory systems
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge handle, or NULL on failure
 *
 * Performance: O(1)
 * Memory: ~4KB base + sample cache
 *
 * Thread safety: The returned bridge is thread-safe for concurrent use
 *
 * Example:
 *   pr_curriculum_config_t config = pr_curriculum_config_default();
 *   config.exploration_epsilon = 0.2f;  // 20% exploration
 *   pr_curriculum_bridge_t bridge = pr_curriculum_bridge_create(&config);
 */
NIMCP_EXPORT pr_curriculum_bridge_t pr_curriculum_bridge_create(
    const pr_curriculum_config_t* config);

/**
 * @brief Destroy curriculum bridge
 *
 * WHAT: Free all bridge resources
 * WHY:  Proper resource cleanup
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * Performance: O(n) where n = cached samples
 */
NIMCP_EXPORT void pr_curriculum_bridge_destroy(pr_curriculum_bridge_t bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset statistics, caches, and history
 * WHY:  Start fresh curriculum learning session
 *
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 *
 * Performance: O(n) where n = cached samples
 */
NIMCP_EXPORT int pr_curriculum_bridge_reset(pr_curriculum_bridge_t bridge);

//=============================================================================
// Difficulty Computation Functions
//=============================================================================

/**
 * @brief Compute difficulty for a single sample
 *
 * WHAT: Estimate learning difficulty for a sample
 * WHY:  Core function for curriculum ordering
 * HOW:  Combine resonance, entanglement, and novelty signals
 *
 * @param bridge Curriculum bridge
 * @param graph Entanglement graph for context
 * @param sample Sample to evaluate
 * @param result Output difficulty result
 * @return 0 on success, -1 on error
 *
 * Difficulty formula (COMPOSITE method):
 *   D = w1*(1 - max_resonance) + w2*(1/(1+entangle)) + w3*quat_dist
 *
 * Performance: ~100ns with cached resonance, ~500ns without
 *
 * Example:
 *   pr_difficulty_result_t result;
 *   pr_curriculum_compute_difficulty(bridge, graph, &sample, &result);
 *   printf("Difficulty: %.3f (curiosity: %.3f)\n",
 *          result.total_difficulty, result.curiosity_score);
 */
NIMCP_EXPORT int pr_curriculum_compute_difficulty(
    pr_curriculum_bridge_t bridge,
    entangle_graph_t graph,
    const pr_curriculum_sample_t* sample,
    pr_difficulty_result_t* result);

/**
 * @brief Batch compute difficulties for multiple samples
 *
 * WHAT: Efficiently compute difficulty for many samples
 * WHY:  Performance optimization for large curricula
 *
 * @param bridge Curriculum bridge
 * @param graph Entanglement graph
 * @param samples Array of samples
 * @param results Output array of results (caller-allocated)
 * @param count Number of samples
 * @return Number of successfully computed, -1 on error
 *
 * Performance: ~80ns per sample (amortized)
 */
NIMCP_EXPORT int pr_curriculum_compute_difficulty_batch(
    pr_curriculum_bridge_t bridge,
    entangle_graph_t graph,
    const pr_curriculum_sample_t* samples,
    pr_difficulty_result_t* results,
    uint32_t count);

/**
 * @brief Get cached difficulty for sample
 *
 * WHAT: Retrieve cached difficulty without recomputation
 * WHY:  Fast lookup when difficulty is already known
 *
 * @param bridge Curriculum bridge
 * @param sample_id Sample identifier
 * @param result Output difficulty result (if found)
 * @return true if found in cache, false otherwise
 *
 * Performance: O(1) hash lookup
 */
NIMCP_EXPORT bool pr_curriculum_get_cached_difficulty(
    pr_curriculum_bridge_t bridge,
    uint64_t sample_id,
    pr_difficulty_result_t* result);

/**
 * @brief Invalidate cached difficulty for sample
 *
 * WHAT: Mark cached difficulty as stale
 * WHY:  Force recomputation after memory changes
 *
 * @param bridge Curriculum bridge
 * @param sample_id Sample identifier
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pr_curriculum_invalidate_difficulty(
    pr_curriculum_bridge_t bridge,
    uint64_t sample_id);

//=============================================================================
// Ordering Functions
//=============================================================================

/**
 * @brief Order samples by resonance with memory
 *
 * WHAT: Sort samples by their resonance with existing memories
 * WHY:  Present familiar samples first (scaffolding)
 * HOW:  Compute max resonance for each, sort by descending resonance
 *
 * @param bridge Curriculum bridge
 * @param graph Entanglement graph for resonance context
 * @param samples Array of samples (reordered in place)
 * @param count Number of samples
 * @return 0 on success, -1 on error
 *
 * Algorithm:
 *   1. For each sample, find max resonance with any memory node
 *   2. Sort samples by resonance (descending = easy first)
 *   3. Optionally add jitter to prevent deterministic ordering
 *
 * Performance: O(n * m) resonance + O(n log n) sort
 * where n = samples, m = memory nodes
 *
 * Example:
 *   pr_curriculum_sample_t samples[100];
 *   // ... fill samples ...
 *   pr_curriculum_order_by_resonance(bridge, graph, samples, 100);
 *   // Now samples[0] has highest resonance (easiest)
 */
NIMCP_EXPORT int pr_curriculum_order_by_resonance(
    pr_curriculum_bridge_t bridge,
    entangle_graph_t graph,
    pr_curriculum_sample_t* samples,
    uint32_t count);

/**
 * @brief Order samples by difficulty
 *
 * WHAT: Sort samples by computed difficulty
 * WHY:  Present easy samples first (curriculum learning)
 * HOW:  Compute difficulty for each, sort accordingly
 *
 * @param bridge Curriculum bridge
 * @param graph Entanglement graph for difficulty context
 * @param samples Array of samples (reordered in place)
 * @param count Number of samples
 * @return 0 on success, -1 on error
 *
 * Performance: O(n) difficulty + O(n log n) sort
 *
 * Note: Uses bridge config's ascending_difficulty flag
 */
NIMCP_EXPORT int pr_curriculum_order_by_difficulty(
    pr_curriculum_bridge_t bridge,
    entangle_graph_t graph,
    pr_curriculum_sample_t* samples,
    uint32_t count);

/**
 * @brief Order samples by curiosity score
 *
 * WHAT: Sort samples by novelty/curiosity
 * WHY:  Prioritize novel samples for exploration
 * HOW:  Curiosity = 1 - max_resonance, sort descending
 *
 * @param bridge Curriculum bridge
 * @param graph Entanglement graph
 * @param samples Array of samples (reordered in place)
 * @param count Number of samples
 * @return 0 on success, -1 on error
 *
 * Performance: O(n * m) resonance + O(n log n) sort
 */
NIMCP_EXPORT int pr_curriculum_order_by_curiosity(
    pr_curriculum_bridge_t bridge,
    entangle_graph_t graph,
    pr_curriculum_sample_t* samples,
    uint32_t count);

//=============================================================================
// Batch Selection Functions
//=============================================================================

/**
 * @brief Select next batch of samples for training
 *
 * WHAT: Choose optimal batch based on curriculum strategy
 * WHY:  Core curriculum learning API for training loops
 * HOW:  Apply strategy, balance tiers, mix exploration
 *
 * @param bridge Curriculum bridge
 * @param graph Entanglement graph
 * @param samples Pool of available samples
 * @param sample_count Number of available samples
 * @param batch_size Desired batch size
 * @param result Output batch result (sample_ids must be pre-allocated)
 * @return 0 on success, -1 on error
 *
 * Algorithm:
 *   1. Compute/retrieve difficulties for all samples
 *   2. Apply curriculum strategy (resonance, curiosity, hybrid)
 *   3. Balance tier representation per config
 *   4. Apply exploration epsilon for random selection
 *   5. Return selected sample IDs
 *
 * Performance: O(n) difficulty + O(n) selection
 *
 * Example:
 *   pr_curriculum_sample_t samples[1000];
 *   uint64_t selected_ids[32];
 *   pr_batch_result_t result = { .sample_ids = selected_ids };
 *   pr_curriculum_select_next_batch(bridge, graph, samples, 1000, 32, &result);
 *   // Process selected_ids[0..result.batch_size-1]
 */
NIMCP_EXPORT int pr_curriculum_select_next_batch(
    pr_curriculum_bridge_t bridge,
    entangle_graph_t graph,
    const pr_curriculum_sample_t* samples,
    uint32_t sample_count,
    uint32_t batch_size,
    pr_batch_result_t* result);

/**
 * @brief Select batch with tier balancing
 *
 * WHAT: Select batch with explicit tier distribution targets
 * WHY:  Control balance of new vs. review material
 *
 * @param bridge Curriculum bridge
 * @param graph Entanglement graph
 * @param samples Sample pool
 * @param sample_count Number of samples
 * @param batch_size Desired batch size
 * @param tier_weights Target proportion per tier [Z0, Z1, Z2, Z3]
 * @param result Output batch result
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pr_curriculum_select_balanced_batch(
    pr_curriculum_bridge_t bridge,
    entangle_graph_t graph,
    const pr_curriculum_sample_t* samples,
    uint32_t sample_count,
    uint32_t batch_size,
    const float tier_weights[PR_CURRICULUM_NUM_TIERS],
    pr_batch_result_t* result);

//=============================================================================
// Curiosity/Novelty Functions
//=============================================================================

/**
 * @brief Compute curiosity score for a sample
 *
 * WHAT: Measure how novel/surprising a sample is
 * WHY:  Drive exploration for curriculum diversity
 * HOW:  Curiosity = 1 - max(resonance with any memory)
 *
 * @param bridge Curriculum bridge
 * @param graph Entanglement graph
 * @param sample Sample to evaluate
 * @return Curiosity score [0,1], or -1.0f on error
 *
 * Interpretation:
 * - 0.0 = Highly familiar (resonates perfectly with existing memory)
 * - 0.5 = Moderately novel (some similarity to existing memories)
 * - 1.0 = Completely novel (no resonance with any memory)
 *
 * Performance: O(m) where m = memory nodes
 *
 * Example:
 *   float curiosity = pr_curriculum_curiosity_score(bridge, graph, &sample);
 *   if (curiosity > 0.7f) {
 *       // High novelty - explore this sample
 *   }
 */
NIMCP_EXPORT float pr_curriculum_curiosity_score(
    pr_curriculum_bridge_t bridge,
    entangle_graph_t graph,
    const pr_curriculum_sample_t* sample);

/**
 * @brief Select most curious samples from pool
 *
 * WHAT: Pick K samples with highest curiosity
 * WHY:  Active learning / exploration selection
 *
 * @param bridge Curriculum bridge
 * @param graph Entanglement graph
 * @param samples Sample pool
 * @param sample_count Number of samples
 * @param k Number of samples to select
 * @param selected_ids Output selected IDs (caller-allocated, size >= k)
 * @param selected_count Output: actual number selected
 * @return 0 on success, -1 on error
 *
 * Performance: O(n * m + n log k)
 */
NIMCP_EXPORT int pr_curriculum_select_most_curious(
    pr_curriculum_bridge_t bridge,
    entangle_graph_t graph,
    const pr_curriculum_sample_t* samples,
    uint32_t sample_count,
    uint32_t k,
    uint64_t* selected_ids,
    uint32_t* selected_count);

//=============================================================================
// Pacing Functions
//=============================================================================

/**
 * @brief Get recommended pacing based on consolidation
 *
 * WHAT: Compute curriculum pacing from memory consolidation rate
 * WHY:  Speed up when learning well, slow down when struggling
 * HOW:  Monitor Z-ladder promotions/demotions over time window
 *
 * @param bridge Curriculum bridge
 * @param ladder Z-ladder for consolidation signals
 * @param pacing Output pacing information
 * @return 0 on success, -1 on error
 *
 * Pacing formula:
 *   rate = recent_promotions / recent_demotions
 *   if rate > threshold: multiplier = fast_pace (e.g., 1.5)
 *   if rate < 1/threshold: multiplier = slow_pace (e.g., 0.5)
 *   else: multiplier = 1.0
 *
 * Performance: O(1) with cached stats
 *
 * Example:
 *   pr_curriculum_pacing_t pacing;
 *   pr_curriculum_consolidation_pace(bridge, ladder, &pacing);
 *   if (pacing.pacing_multiplier > 1.0f) {
 *       // Increase difficulty or speed
 *   }
 */
NIMCP_EXPORT int pr_curriculum_consolidation_pace(
    pr_curriculum_bridge_t bridge,
    z_ladder_t ladder,
    pr_curriculum_pacing_t* pacing);

/**
 * @brief Compute pacing for current epoch
 *
 * WHAT: Get pacing multiplier for epoch-based training
 * WHY:  Adjust curriculum speed per epoch
 *
 * @param bridge Curriculum bridge
 * @param ladder Z-ladder
 * @param current_epoch Current epoch number
 * @return Pacing multiplier, or 1.0f on error
 */
NIMCP_EXPORT float pr_curriculum_get_epoch_pace(
    pr_curriculum_bridge_t bridge,
    z_ladder_t ladder,
    uint32_t current_epoch);

/**
 * @brief Record consolidation event for pacing
 *
 * WHAT: Track promotion/demotion event for pacing computation
 * WHY:  Keep history for pacing calculations
 *
 * @param bridge Curriculum bridge
 * @param is_promotion true for promotion, false for demotion
 * @param from_tier Source tier
 * @param to_tier Destination tier
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pr_curriculum_record_consolidation(
    pr_curriculum_bridge_t bridge,
    bool is_promotion,
    pr_memory_tier_t from_tier,
    pr_memory_tier_t to_tier);

//=============================================================================
// Tier Distribution Functions
//=============================================================================

/**
 * @brief Get current tier distribution from Z-ladder
 *
 * WHAT: Retrieve memory distribution across tiers
 * WHY:  Guide curriculum strategy based on knowledge state
 *
 * @param bridge Curriculum bridge
 * @param ladder Z-ladder for tier information
 * @param distribution Output distribution
 * @return 0 on success, -1 on error
 *
 * Performance: O(1) with cached stats
 *
 * Example:
 *   pr_tier_distribution_t dist;
 *   pr_curriculum_tier_distribution(bridge, ladder, &dist);
 *   if (dist.proportions[PR_MEMORY_TIER_Z0] > 0.5f) {
 *       // Too much in working memory - need more consolidation time
 *   }
 */
NIMCP_EXPORT int pr_curriculum_tier_distribution(
    pr_curriculum_bridge_t bridge,
    z_ladder_t ladder,
    pr_tier_distribution_t* distribution);

/**
 * @brief Get recommended strategy based on tier distribution
 *
 * WHAT: Suggest curriculum strategy from memory state
 * WHY:  Automatically adapt curriculum to learner state
 *
 * @param bridge Curriculum bridge
 * @param ladder Z-ladder
 * @return Recommended curriculum strategy
 *
 * Recommendations:
 * - Z0 heavy: PR_CURRICULUM_RESONANCE (review/consolidate)
 * - Z1 heavy: PR_CURRICULUM_HYBRID (balance)
 * - Z2 heavy: PR_CURRICULUM_CURIOSITY (challenge)
 * - Z3 heavy: PR_CURRICULUM_CURIOSITY (transfer)
 */
NIMCP_EXPORT pr_curriculum_type_t pr_curriculum_recommend_strategy(
    pr_curriculum_bridge_t bridge,
    z_ladder_t ladder);

//=============================================================================
// Feedback Functions
//=============================================================================

/**
 * @brief Update curriculum state after training step
 *
 * WHAT: Process feedback from training step
 * WHY:  Adapt curriculum based on learning outcomes
 * HOW:  Update difficulty estimates, adjust pacing
 *
 * @param bridge Curriculum bridge
 * @param results Array of step results
 * @param count Number of results
 * @return 0 on success, -1 on error
 *
 * Updates:
 * - Difficulty estimates (if loss suggests miscalibration)
 * - Pacing multiplier
 * - Exploration ratio
 * - Per-sample presentation counts
 *
 * Performance: O(count)
 *
 * Example:
 *   pr_step_result_t results[32];
 *   // ... fill results from training ...
 *   pr_curriculum_update_after_step(bridge, results, 32);
 */
NIMCP_EXPORT int pr_curriculum_update_after_step(
    pr_curriculum_bridge_t bridge,
    const pr_step_result_t* results,
    uint32_t count);

/**
 * @brief Update difficulty estimate from training outcome
 *
 * WHAT: Adjust difficulty based on actual learning performance
 * WHY:  Calibrate difficulty estimates to ground truth
 *
 * @param bridge Curriculum bridge
 * @param sample_id Sample identifier
 * @param loss Training loss (lower = easier)
 * @param was_correct For classification tasks
 * @return New difficulty estimate, or -1.0f on error
 *
 * Formula:
 *   difficulty_new = alpha * difficulty_estimated + (1-alpha) * loss_normalized
 */
NIMCP_EXPORT float pr_curriculum_update_difficulty_from_loss(
    pr_curriculum_bridge_t bridge,
    uint64_t sample_id,
    float loss,
    bool was_correct);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get curriculum bridge statistics
 *
 * WHAT: Retrieve operational metrics
 * WHY:  Monitor bridge health and curriculum effectiveness
 *
 * @param bridge Curriculum bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_curriculum_get_stats(
    pr_curriculum_bridge_t bridge,
    pr_curriculum_stats_t* stats);

/**
 * @brief Reset statistics counters
 *
 * WHAT: Clear all statistics to zero
 * WHY:  Start fresh measurement period
 *
 * @param bridge Curriculum bridge
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pr_curriculum_reset_stats(pr_curriculum_bridge_t bridge);

/**
 * @brief Get recent curriculum events
 *
 * WHAT: Retrieve logged curriculum events
 * WHY:  Debugging and analysis
 *
 * @param bridge Curriculum bridge
 * @param events Output event array (caller-allocated)
 * @param max_events Maximum events to return
 * @param event_count Output: actual events returned
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pr_curriculum_get_events(
    pr_curriculum_bridge_t bridge,
    pr_curriculum_event_t* events,
    uint32_t max_events,
    uint32_t* event_count);

//=============================================================================
// Strategy Control Functions
//=============================================================================

/**
 * @brief Set curriculum strategy
 *
 * WHAT: Change the active curriculum strategy
 * WHY:  Adapt strategy mid-training
 *
 * @param bridge Curriculum bridge
 * @param type New strategy type
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pr_curriculum_set_strategy(
    pr_curriculum_bridge_t bridge,
    pr_curriculum_type_t type);

/**
 * @brief Get current curriculum strategy
 *
 * @param bridge Curriculum bridge
 * @return Current strategy type
 */
NIMCP_EXPORT pr_curriculum_type_t pr_curriculum_get_strategy(
    pr_curriculum_bridge_t bridge);

/**
 * @brief Set exploration epsilon
 *
 * WHAT: Adjust exploration probability
 * WHY:  Control exploration/exploitation balance
 *
 * @param bridge Curriculum bridge
 * @param epsilon New epsilon value [0,1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pr_curriculum_set_exploration(
    pr_curriculum_bridge_t bridge,
    float epsilon);

/**
 * @brief Set difficulty ordering direction
 *
 * WHAT: Set whether to present easy or hard samples first
 * WHY:  Different learning stages may benefit from different orders
 *
 * @param bridge Curriculum bridge
 * @param ascending true = easy first, false = hard first
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pr_curriculum_set_ascending_difficulty(
    pr_curriculum_bridge_t bridge,
    bool ascending);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get strategy name as string
 *
 * @param type Curriculum strategy type
 * @return Human-readable strategy name
 */
NIMCP_EXPORT const char* pr_curriculum_type_name(pr_curriculum_type_t type);

/**
 * @brief Get difficulty method name as string
 *
 * @param method Difficulty method
 * @return Human-readable method name
 */
NIMCP_EXPORT const char* pr_difficulty_method_name(pr_difficulty_method_t method);

/**
 * @brief Get event type name as string
 *
 * @param type Event type
 * @return Human-readable event name
 */
NIMCP_EXPORT const char* pr_curriculum_event_name(pr_curriculum_event_type_t type);

/**
 * @brief Print difficulty result to stdout
 *
 * @param result Difficulty result to print
 */
NIMCP_EXPORT void pr_curriculum_print_difficulty(const pr_difficulty_result_t* result);

/**
 * @brief Print batch result to stdout
 *
 * @param result Batch result to print
 */
NIMCP_EXPORT void pr_curriculum_print_batch(const pr_batch_result_t* result);

/**
 * @brief Print pacing information to stdout
 *
 * @param pacing Pacing information to print
 */
NIMCP_EXPORT void pr_curriculum_print_pacing(const pr_curriculum_pacing_t* pacing);

/**
 * @brief Print curriculum statistics to stdout
 *
 * @param bridge Bridge to print stats for
 */
NIMCP_EXPORT void pr_curriculum_print_stats(pr_curriculum_bridge_t bridge);

/**
 * @brief Initialize a sample structure
 *
 * WHAT: Zero-initialize and set default values
 * WHY:  Convenience for creating samples
 *
 * @param sample Sample to initialize
 */
NIMCP_EXPORT void pr_curriculum_sample_init(pr_curriculum_sample_t* sample);

/**
 * @brief Get current time in milliseconds
 *
 * Utility function for timestamp generation.
 *
 * @return Milliseconds since epoch
 */
NIMCP_EXPORT uint64_t pr_curriculum_current_time_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PR_CURRICULUM_BRIDGE_H */
