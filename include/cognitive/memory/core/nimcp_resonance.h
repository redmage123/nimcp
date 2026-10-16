//=============================================================================
// nimcp_resonance.h - Resonance Scoring Engine for Prime Resonant Memory
//=============================================================================
/**
 * @file nimcp_resonance.h
 * @brief Four-component resonance scoring for memory similarity and retrieval
 *
 * WHAT: Computes multi-dimensional similarity/relevance between memories
 * WHY:  Memory retrieval requires combining content, semantic state, phase, and
 *       oscillator coherence for biologically realistic matching
 * HOW:  Weighted combination of Jaccard, Phase, Quaternion, and Kuramoto scores
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Resonance Scoring Model:
 *   +-----------------------------------------------------------------------+
 *   |  Memory similarity is multi-dimensional, combining:                   |
 *   |                                                                       |
 *   |  1. CONTENT SIMILARITY (Jaccard on Prime Signatures)                  |
 *   |     - What: Overlap of prime number signatures                        |
 *   |     - Biology: Pattern completion in CA3 hippocampus                  |
 *   |     - Formula: |A intersect B| / |A union B|                          |
 *   |                                                                       |
 *   |  2. PHASE COHERENCE (Phase Locking Value)                             |
 *   |     - What: How synchronized are memory oscillation phases            |
 *   |     - Biology: Theta-gamma coupling for binding                       |
 *   |     - Formula: cos(phase1 - phase2), normalized to [0,1]              |
 *   |                                                                       |
 *   |  3. QUATERNION SIMILARITY (Geodesic Distance)                         |
 *   |     - What: Semantic state similarity on hypersphere                  |
 *   |     - Biology: Similar memories share consolidation/emotion/salience  |
 *   |     - Formula: 1 - arccos(|q1.q2|) / pi                               |
 *   |                                                                       |
 *   |  4. KURAMOTO COHERENCE (Oscillator Synchronization)                   |
 *   |     - What: How synchronized are source modules                       |
 *   |     - Biology: Cross-regional binding via gamma synchrony             |
 *   |     - Formula: r = |mean(exp(i*theta_j))|                             |
 *   +-----------------------------------------------------------------------+
 *
 *   Combined Resonance Formula:
 *   +-----------------------------------------------------------------------+
 *   |  R = w1*Jaccard + w2*Phase + w3*Quat + w4*Kuramoto                    |
 *   |                                                                       |
 *   |  Default weights: [0.3, 0.2, 0.3, 0.2]                                |
 *   |  - Higher Jaccard: Content-based retrieval                            |
 *   |  - Higher Quat: Emotional/state-based retrieval                       |
 *   |  - Higher Phase: Temporal binding retrieval                           |
 *   |  - Higher Kuramoto: Cross-modal binding retrieval                     |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Single resonance: ~100ns (no Kuramoto), ~200ns (with Kuramoto)
 * - Batch N=1000: ~100us (vectorized where possible)
 * - Top-K extraction: O(N log K) via partial sort
 *
 * MEMORY:
 * - resonance_config_t: 24 bytes
 * - resonance_result_t: 24 bytes
 * - resonance_query_t: ~40 bytes (excluding signature data)
 *
 * INTEGRATION:
 * - Core: Prime signatures, quaternions, entanglement graph
 * - Middleware: Z-Ladder retrieval, spreading activation
 * - API: Memory query interface
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_RESONANCE_H
#define NIMCP_RESONANCE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "nimcp_quaternion.h"
#include "nimcp_prime_signature.h"
#include "utils/memory/nimcp_memory.h"
#include "constants/nimcp_math_constants.h"

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

/** Pi constant (if not already defined) */

/** Two times Pi */
#ifndef NIMCP_TWO_PI_F
#endif

/** Default weight for Jaccard (prime signature) component */
#define RESONANCE_DEFAULT_WEIGHT_JACCARD    0.3f

/** Default weight for phase coherence component */
#define RESONANCE_DEFAULT_WEIGHT_PHASE      0.2f

/** Default weight for quaternion similarity component */
#define RESONANCE_DEFAULT_WEIGHT_QUATERNION 0.3f

/** Default weight for Kuramoto coherence component */
#define RESONANCE_DEFAULT_WEIGHT_KURAMOTO   0.2f

/** Default minimum resonance threshold */
#define RESONANCE_DEFAULT_THRESHOLD         0.5f

/** Maximum number of targets for batch operations */
#define RESONANCE_MAX_BATCH_SIZE            65536

/** Epsilon for floating-point comparisons */
#define RESONANCE_EPSILON                   1e-6f

//=============================================================================
// Forward Declarations
//=============================================================================

/* Forward declare Kuramoto state from nimcp_kuramoto.h (optional dependency) */
typedef struct kuramoto_state_struct kuramoto_state_t;

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Resonance computation configuration
 *
 * WHAT: Parameters controlling how resonance is computed
 * WHY:  Different retrieval contexts need different component weightings
 * HOW:  Four weights (summing to 1.0 if normalize_weights=true) plus threshold
 *
 * PRESET CONFIGURATIONS:
 * - Content-focused: [0.6, 0.1, 0.2, 0.1] - Emphasize prime signature match
 * - Emotional: [0.2, 0.1, 0.6, 0.1] - Emphasize quaternion (emotion) similarity
 * - Temporal: [0.2, 0.5, 0.2, 0.1] - Emphasize phase coherence
 * - Cross-modal: [0.2, 0.1, 0.2, 0.5] - Emphasize Kuramoto sync
 */
typedef struct {
    float weight_jaccard;      /**< Weight for prime signature similarity [0,1] */
    float weight_phase;        /**< Weight for phase coherence [0,1] */
    float weight_quaternion;   /**< Weight for quaternion similarity [0,1] */
    float weight_kuramoto;     /**< Weight for oscillator sync [0,1] */
    float threshold;           /**< Minimum resonance to consider (0-1) */
    bool normalize_weights;    /**< Auto-normalize weights to sum to 1.0 */
} resonance_config_t;

/**
 * @brief Resonance result with component breakdown
 *
 * WHAT: Complete resonance score with individual component contributions
 * WHY:  Enables analysis of why memories resonate, not just that they do
 * HOW:  Total score plus four component values
 */
typedef struct {
    float total;               /**< Combined resonance score (0-1) */
    float jaccard_component;   /**< Prime signature contribution [0, w_jaccard] */
    float phase_component;     /**< Phase coherence contribution [0, w_phase] */
    float quat_component;      /**< Quaternion similarity contribution [0, w_quat] */
    float kuramoto_component;  /**< Oscillator sync contribution [0, w_kuramoto] */
    bool above_threshold;      /**< Whether total >= configured threshold */
} resonance_result_t;

/**
 * @brief Query structure for resonance computation
 *
 * WHAT: The "needle" - what we're searching for
 * WHY:  Encapsulates all query parameters in one structure
 * HOW:  Contains prime signature, quaternion state, phase, and module ID
 */
typedef struct {
    prime_signature_t* signature;   /**< Content signature (can be NULL for partial query) */
    nimcp_quaternion_t quaternion;  /**< Semantic state (consolidation, emotion, salience, access) */
    float phase;                    /**< Current oscillation phase [0, 2*pi] */
    uint32_t module_id;             /**< Source module ID for Kuramoto coherence */
} resonance_query_t;

/**
 * @brief Memory target for resonance computation
 *
 * WHAT: The "haystack element" - a memory to compare against
 * WHY:  Encapsulates all memory parameters for resonance scoring
 * HOW:  Same fields as query plus memory_id for result tracking
 */
typedef struct {
    prime_signature_t* signature;   /**< Content signature */
    nimcp_quaternion_t quaternion;  /**< Semantic state */
    float phase;                    /**< Memory's current phase [0, 2*pi] */
    uint32_t module_id;             /**< Module where memory resides */
    uint64_t memory_id;             /**< Unique ID for result tracking */
} resonance_target_t;

/**
 * @brief Result entry for batch operations
 *
 * WHAT: Pairs a memory ID with its resonance result
 * WHY:  Batch operations need to track which result belongs to which memory
 */
typedef struct {
    uint64_t memory_id;         /**< Memory identifier */
    resonance_result_t result;  /**< Full resonance result */
} resonance_batch_result_t;

/**
 * @brief Statistics for resonance operations
 *
 * WHAT: Operational metrics for monitoring and debugging
 * WHY:  Track computation costs and result distributions
 */
typedef struct {
    uint64_t total_computations;     /**< Total resonance computations */
    uint64_t batch_computations;     /**< Batch operation count */
    uint64_t above_threshold_count;  /**< How many exceeded threshold */
    float mean_resonance;            /**< Running mean of resonance scores */
    float max_resonance;             /**< Maximum resonance seen */
    float min_resonance;             /**< Minimum resonance seen (excluding zeros) */
    uint64_t computation_time_ns;    /**< Cumulative computation time */
} resonance_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default resonance configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for most use cases
 * HOW:  Sets balanced weights and standard threshold
 *
 * @return Default configuration with:
 *         - weight_jaccard: 0.3
 *         - weight_phase: 0.2
 *         - weight_quaternion: 0.3
 *         - weight_kuramoto: 0.2
 *         - threshold: 0.5
 *         - normalize_weights: true
 *
 * Performance: ~5ns
 *
 * Example:
 *   resonance_config_t config = resonance_config_default();
 *   config.threshold = 0.7f;  // More selective retrieval
 */
NIMCP_EXPORT resonance_config_t resonance_config_default(void);

/**
 * @brief Get content-focused configuration
 *
 * WHAT: Configuration emphasizing prime signature similarity
 * WHY:  For "find similar content" queries
 *
 * @return Config with weight_jaccard=0.6, others reduced proportionally
 */
NIMCP_EXPORT resonance_config_t resonance_config_content_focused(void);

/**
 * @brief Get emotion-focused configuration
 *
 * WHAT: Configuration emphasizing quaternion (emotional) similarity
 * WHY:  For "find memories that felt like this" queries
 *
 * @return Config with weight_quaternion=0.6, others reduced proportionally
 */
NIMCP_EXPORT resonance_config_t resonance_config_emotion_focused(void);

/**
 * @brief Get temporal-focused configuration
 *
 * WHAT: Configuration emphasizing phase coherence
 * WHY:  For temporal binding and sequence retrieval
 *
 * @return Config with weight_phase=0.5, others reduced proportionally
 */
NIMCP_EXPORT resonance_config_t resonance_config_temporal_focused(void);

/**
 * @brief Validate and optionally normalize configuration
 *
 * WHAT: Ensures configuration is valid, normalizes weights if requested
 * WHY:  Prevent invalid configs causing incorrect resonance scores
 * HOW:  Checks weight ranges, normalizes if flag set
 *
 * @param config Configuration to validate (modified in place if normalizing)
 * @return true if valid (and normalized if requested), false if invalid
 *
 * Validation rules:
 * - All weights must be >= 0
 * - At least one weight must be > 0
 * - Threshold must be in [0, 1]
 *
 * Performance: ~10ns
 *
 * Example:
 *   resonance_config_t config = resonance_config_default();
 *   config.weight_jaccard = 0.5f;
 *   config.weight_phase = 0.5f;
 *   config.normalize_weights = true;
 *   if (!resonance_config_validate(&config)) {
 *       // Handle error
 *   }
 *   // Now all weights sum to 1.0
 */
NIMCP_EXPORT bool resonance_config_validate(resonance_config_t* config);

//=============================================================================
// Single Computation Functions
//=============================================================================

/**
 * @brief Compute full resonance between query and target
 *
 * WHAT: Calculates four-component resonance score
 * WHY:  Core function for memory similarity/relevance
 * HOW:  Computes Jaccard + Phase + Quat + Kuramoto, weighted
 *
 * ALGORITHM:
 *   1. Jaccard = prime_sig_jaccard(query.signature, target.signature)
 *   2. Phase = (cos(query.phase - target.phase) + 1) / 2
 *   3. Quat = 1 - geodesic_distance(query.quat, target.quat) / pi
 *   4. Kuramoto = kuramoto_coherence(query.module, target.module)
 *   5. R = w1*Jaccard + w2*Phase + w3*Quat + w4*Kuramoto
 *
 * @param query Query parameters (needle)
 * @param target Target memory (haystack element)
 * @param config Resonance configuration
 * @param kuramoto_state Kuramoto oscillator state (can be NULL to skip)
 * @param result Output resonance result
 * @return true on success, false on error (NULL pointers, invalid config)
 *
 * Performance: ~100ns without Kuramoto, ~200ns with Kuramoto
 *
 * Example:
 *   resonance_query_t query = { .signature = &sig, .quaternion = q1, .phase = 0.5f };
 *   resonance_target_t target = { .signature = &sig2, .quaternion = q2, .phase = 1.0f };
 *   resonance_config_t config = resonance_config_default();
 *   resonance_result_t result;
 *
 *   if (resonance_compute(&query, &target, &config, NULL, &result)) {
 *       printf("Resonance: %.3f\n", result.total);
 *   }
 */
NIMCP_EXPORT bool resonance_compute(
    const resonance_query_t* query,
    const resonance_target_t* target,
    const resonance_config_t* config,
    const kuramoto_state_t* kuramoto_state,
    resonance_result_t* result);

/**
 * @brief Compute resonance without Kuramoto component (faster)
 *
 * WHAT: Three-component resonance (Jaccard + Phase + Quat)
 * WHY:  Faster when oscillator coherence isn't needed
 * HOW:  Skips Kuramoto computation, redistributes weight
 *
 * @param query Query parameters
 * @param target Target memory
 * @param config Configuration (Kuramoto weight is redistributed)
 * @param result Output resonance result (kuramoto_component will be 0)
 * @return true on success, false on error
 *
 * Performance: ~80ns
 *
 * Example:
 *   resonance_result_t result;
 *   resonance_compute_fast(&query, &target, &config, &result);
 */
NIMCP_EXPORT bool resonance_compute_fast(
    const resonance_query_t* query,
    const resonance_target_t* target,
    const resonance_config_t* config,
    resonance_result_t* result);

/**
 * @brief Compute only prime signature similarity
 *
 * WHAT: Single-component resonance (Jaccard only)
 * WHY:  Fastest option for content-only matching
 * HOW:  Only computes prime signature Jaccard coefficient
 *
 * @param query Query parameters (only signature is used)
 * @param target Target memory (only signature is used)
 * @return Jaccard similarity [0, 1], or -1.0f on error
 *
 * Performance: ~30ns
 */
NIMCP_EXPORT float resonance_compute_jaccard_only(
    const resonance_query_t* query,
    const resonance_target_t* target);

//=============================================================================
// Batch Computation Functions
//=============================================================================

/**
 * @brief Compute resonance for multiple targets
 *
 * WHAT: Batch resonance computation for N targets
 * WHY:  More efficient than N individual calls
 * HOW:  Vectorized where possible, shared computation
 *
 * @param query Single query
 * @param targets Array of targets
 * @param count Number of targets
 * @param config Resonance configuration
 * @param kuramoto_state Kuramoto state (can be NULL)
 * @param results Output array of results (must be pre-allocated)
 * @return Number of successful computations, or -1 on error
 *
 * Performance: ~80ns per target (with amortized overhead)
 *
 * Example:
 *   resonance_target_t targets[100];
 *   resonance_result_t results[100];
 *   // ... fill targets ...
 *   int count = resonance_compute_batch(&query, targets, 100, &config, NULL, results);
 */
NIMCP_EXPORT int resonance_compute_batch(
    const resonance_query_t* query,
    const resonance_target_t* targets,
    size_t count,
    const resonance_config_t* config,
    const kuramoto_state_t* kuramoto_state,
    resonance_result_t* results);

/**
 * @brief Get top-K matching targets by resonance
 *
 * WHAT: Finds K targets with highest resonance scores
 * WHY:  Memory retrieval typically wants top matches, not all
 * HOW:  Partial sort / heap selection for O(N log K) complexity
 *
 * @param query Single query
 * @param targets Array of targets
 * @param count Number of targets
 * @param k Number of top matches to return
 * @param config Resonance configuration
 * @param kuramoto_state Kuramoto state (can be NULL)
 * @param results Output array of K results (pre-allocated, size >= k)
 * @return Actual number of results returned (<= k), or -1 on error
 *
 * Performance: O(N + N log K) where N = count
 *
 * Example:
 *   resonance_batch_result_t top10[10];
 *   int found = resonance_compute_top_k(&query, targets, 1000, 10, &config, NULL, top10);
 *   for (int i = 0; i < found; i++) {
 *       printf("Memory %lu: %.3f\n", top10[i].memory_id, top10[i].result.total);
 *   }
 */
NIMCP_EXPORT int resonance_compute_top_k(
    const resonance_query_t* query,
    const resonance_target_t* targets,
    size_t count,
    size_t k,
    const resonance_config_t* config,
    const kuramoto_state_t* kuramoto_state,
    resonance_batch_result_t* results);

/**
 * @brief Get all targets with resonance above threshold
 *
 * WHAT: Filters targets by resonance threshold
 * WHY:  Retrieve all relevant memories, not just top-K
 * HOW:  Linear scan with threshold comparison
 *
 * @param query Single query
 * @param targets Array of targets
 * @param count Number of targets
 * @param config Resonance configuration (threshold is used)
 * @param kuramoto_state Kuramoto state (can be NULL)
 * @param results Output array (pre-allocated, size >= count for safety)
 * @param result_count Output: number of results returned
 * @return true on success, false on error
 *
 * Performance: O(N) where N = count
 *
 * Example:
 *   resonance_batch_result_t* results = nimcp_malloc(count * sizeof(*results));
 *   size_t found;
 *   resonance_compute_above_threshold(&query, targets, count, &config, NULL, results, &found);
 *   printf("Found %zu memories above threshold %.2f\n", found, config.threshold);
 */
NIMCP_EXPORT bool resonance_compute_above_threshold(
    const resonance_query_t* query,
    const resonance_target_t* targets,
    size_t count,
    const resonance_config_t* config,
    const kuramoto_state_t* kuramoto_state,
    resonance_batch_result_t* results,
    size_t* result_count);

//=============================================================================
// Component Functions (Internal API, also exposed for flexibility)
//=============================================================================

/**
 * @brief Compute Jaccard similarity between two prime signatures
 *
 * WHAT: Set intersection / set union for prime signatures
 * WHY:  Core content similarity metric
 * HOW:  Count overlapping vs total unique primes weighted by exponents
 *
 * Formula: J(A,B) = sum(min(exp_a[i], exp_b[i])) / sum(max(exp_a[i], exp_b[i]))
 *
 * @param sig1 First prime signature
 * @param sig2 Second prime signature
 * @return Jaccard coefficient [0, 1], or -1.0f on error
 *
 * Performance: ~25ns
 *
 * Example:
 *   float similarity = resonance_jaccard(&sig_a, &sig_b);
 *   if (similarity > 0.8f) {
 *       // Very similar content
 *   }
 */
NIMCP_EXPORT float resonance_jaccard(
    const prime_signature_t* sig1,
    const prime_signature_t* sig2);

/**
 * @brief Compute phase coherence (Phase Locking Value)
 *
 * WHAT: How synchronized are two oscillation phases
 * WHY:  Temporal binding metric for memory association
 * HOW:  PLV = (cos(phase1 - phase2) + 1) / 2
 *
 * @param phase1 First phase [0, 2*pi] (wrapped internally if outside)
 * @param phase2 Second phase [0, 2*pi]
 * @return Phase coherence [0, 1], where 1 = perfectly in-phase
 *
 * Performance: ~5ns
 *
 * Example:
 *   float coherence = resonance_phase_coherence(0.0f, 0.1f);  // ~0.99
 *   float anti = resonance_phase_coherence(0.0f, M_PI);       // ~0.0
 */
NIMCP_EXPORT float resonance_phase_coherence(float phase1, float phase2);

/**
 * @brief Compute quaternion similarity
 *
 * WHAT: 1 minus normalized geodesic distance on unit hypersphere
 * WHY:  Semantic state similarity metric
 * HOW:  1 - arccos(|q1.q2|) / pi
 *
 * @param q1 First quaternion (will be normalized)
 * @param q2 Second quaternion (will be normalized)
 * @return Similarity [0, 1], where 1 = identical states
 *
 * Performance: ~20ns
 *
 * Example:
 *   nimcp_quaternion_t q1 = quat_create(0.9f, 0.1f, 0.8f, 0.7f);
 *   nimcp_quaternion_t q2 = quat_create(0.85f, 0.15f, 0.75f, 0.65f);
 *   float sim = resonance_quaternion_similarity(q1, q2);  // ~0.95
 */
NIMCP_EXPORT float resonance_quaternion_similarity(
    nimcp_quaternion_t q1,
    nimcp_quaternion_t q2);

/**
 * @brief Compute Kuramoto oscillator coherence between modules
 *
 * WHAT: How synchronized are oscillators in two modules
 * WHY:  Cross-regional binding metric
 * HOW:  Uses Kuramoto order parameter between module oscillators
 *
 * @param module1 First module ID
 * @param module2 Second module ID
 * @param kuramoto_state Kuramoto oscillator system state
 * @return Coherence [0, 1], where 1 = perfectly synchronized
 *
 * NOTE: Returns 0.5 (neutral) if kuramoto_state is NULL
 *
 * Performance: ~50ns (depends on oscillator count)
 */
NIMCP_EXPORT float resonance_kuramoto_coherence(
    uint32_t module1,
    uint32_t module2,
    const kuramoto_state_t* kuramoto_state);

//=============================================================================
// Pink Noise Modulation Functions
//=============================================================================

/**
 * @brief Modulate resonance score with pink noise sample
 *
 * WHAT: Adds 1/f fluctuation to resonance score
 * WHY:  Real memory retrieval has stochastic variability
 * HOW:  modulated = base * (1 + depth * pink_sample)
 *
 * @param base_score Base resonance score [0, 1]
 * @param pink_sample Pink noise sample [-1, +1]
 * @param depth Modulation depth [0, 1]
 * @return Modulated score, clamped to [0, 1]
 *
 * Performance: ~3ns
 *
 * Example:
 *   float modulated = resonance_modulate(0.8f, 0.2f, 0.1f);
 *   // Result: 0.8 * (1 + 0.1 * 0.2) = 0.816
 */
NIMCP_EXPORT float resonance_modulate(
    float base_score,
    float pink_sample,
    float depth);

/**
 * @brief Batch modulate resonance scores with pink noise
 *
 * WHAT: Apply pink noise modulation to array of scores
 * WHY:  Efficient batch processing
 * HOW:  Vectorized modulation with buffer of pink samples
 *
 * @param scores Array of resonance scores (modified in place)
 * @param count Number of scores
 * @param pink_buffer Array of pink noise samples (must be >= count)
 * @param depth Modulation depth
 * @return true on success, false on error
 *
 * Performance: ~2ns per score
 */
NIMCP_EXPORT bool resonance_modulate_batch(
    float* scores,
    size_t count,
    const float* pink_buffer,
    float depth);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Print resonance result to stdout (debug)
 *
 * WHAT: Human-readable output of resonance result
 * WHY:  Debugging and inspection
 *
 * @param result Result to print
 *
 * Output format:
 *   Resonance: 0.750 [Jaccard: 0.250, Phase: 0.150, Quat: 0.200, Kuramoto: 0.150] ABOVE
 */
NIMCP_EXPORT void resonance_result_print(const resonance_result_t* result);

/**
 * @brief Generate human-readable explanation of resonance result
 *
 * WHAT: Text explanation of why memories resonate
 * WHY:  Explainable AI - understand memory retrieval
 * HOW:  Describes each component's contribution
 *
 * @param result Result to explain
 * @param buf Output buffer for explanation string
 * @param size Size of output buffer
 * @return Number of characters written (excluding null terminator)
 *
 * Example output:
 *   "Resonance 0.75: Strong content match (0.25), good semantic alignment (0.20),
 *    moderate phase coherence (0.15), good module sync (0.15). Above threshold."
 */
NIMCP_EXPORT size_t resonance_explain(
    const resonance_result_t* result,
    char* buf,
    size_t size);

/**
 * @brief Initialize a resonance query structure
 *
 * WHAT: Zero-initializes query and sets defaults
 * WHY:  Convenience for creating queries
 *
 * @param query Query to initialize
 */
NIMCP_EXPORT void resonance_query_init(resonance_query_t* query);

/**
 * @brief Initialize a resonance target structure
 *
 * WHAT: Zero-initializes target and sets defaults
 * WHY:  Convenience for creating targets
 *
 * @param target Target to initialize
 */
NIMCP_EXPORT void resonance_target_init(resonance_target_t* target);

/**
 * @brief Get resonance statistics
 *
 * WHAT: Returns operational metrics
 * WHY:  Monitoring and performance analysis
 *
 * @param stats Output statistics structure
 */
NIMCP_EXPORT void resonance_get_stats(resonance_stats_t* stats);

/**
 * @brief Reset resonance statistics
 *
 * WHAT: Clears all counters and metrics
 * WHY:  Start fresh measurement period
 */
NIMCP_EXPORT void resonance_reset_stats(void);

/**
 * @brief Get last error message
 *
 * WHAT: Returns description of last error
 * WHY:  Debugging failed operations
 *
 * @return Error string or NULL if no error
 */
NIMCP_EXPORT const char* resonance_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_RESONANCE_H
