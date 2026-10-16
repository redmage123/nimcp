//=============================================================================
// nimcp_metamemory.h - Metamemory System for Prime Resonant Architecture
//=============================================================================
/**
 * @file nimcp_metamemory.h
 * @brief Metacognitive awareness of memory contents and capabilities
 *
 * WHAT: Implements "knowing what you know" - metacognitive monitoring of memory
 *       system capabilities, contents, and retrieval predictions
 * WHY:  Human cognition includes awareness of memory states (FOK, TOT, JOL)
 *       enabling adaptive retrieval strategies and confidence calibration
 * HOW:  Integrates with PR memory components to detect partial matches,
 *       compute familiarity signals, and calibrate confidence against accuracy
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Metamemory in Human Cognition:
 *   +-----------------------------------------------------------------------+
 *   |  Metamemory is "cognition about cognition" specifically for memory:   |
 *   |                                                                       |
 *   |  Brain Regions Involved:                                              |
 *   |  - Prefrontal cortex: Monitors retrieval success/failure              |
 *   |  - Anterior cingulate: Conflict detection, error monitoring           |
 *   |  - Hippocampus: Provides familiarity signals                          |
 *   |  - Retrosplenial cortex: Links familiarity to context                 |
 *   |                                                                       |
 *   |  Key Metamemory Phenomena:                                            |
 *   |  1. Feeling of Knowing (FOK): Sense that you know something           |
 *   |  2. Tip of Tongue (TOT): Near-retrieval with partial information      |
 *   |  3. Judgments of Learning (JOL): Predicting future recall success     |
 *   |  4. Confidence-Accuracy Calibration: Matching certainty to truth      |
 *   +-----------------------------------------------------------------------+
 *
 *   Metamemory State Space:
 *   +-----------------------------------------------------------------------+
 *   |  State         | Description              | Neural Correlate          |
 *   |----------------|--------------------------|---------------------------|
 *   |  UNKNOWN       | No metacognitive signal  | Low prefrontal activity   |
 *   |  KNOWN         | Confident retrieval      | Strong pattern completion |
 *   |  FOK           | Know it's there, can't   | Familiarity without       |
 *   |                | retrieve                 | recollection              |
 *   |  TOT           | Partial retrieval,       | Partial hippocampal       |
 *   |                | blocking                 | activation                |
 *   |  UNKNOWN_KNOWN | Know that you don't know | Metacognitive certainty   |
 *   +-----------------------------------------------------------------------+
 *
 *   FOK Detection Algorithm:
 *   +-----------------------------------------------------------------------+
 *   |  1. Query prime signature against memory store                        |
 *   |  2. If no direct match but:                                           |
 *   |     - Resonance > familiarity_threshold (0.4-0.6 typical)             |
 *   |     - Multiple related memories activated (partial activation > 0.3) |
 *   |     - Quaternion similarity high but prime match low                  |
 *   |  3. Then: FOK state detected                                          |
 *   |                                                                       |
 *   |  Biological: Perirhinal cortex familiarity + hippocampal mismatch     |
 *   +-----------------------------------------------------------------------+
 *
 *   TOT Detection Algorithm:
 *   +-----------------------------------------------------------------------+
 *   |  1. Compute partial prime signature match (50-90% overlap)            |
 *   |  2. Check for related features accessible:                            |
 *   |     - First letter, syllable count, semantic category                 |
 *   |  3. Verify blocking: Strong related memories inhibiting target        |
 *   |  4. If partial match + partial features + blocking: TOT state         |
 *   |                                                                       |
 *   |  Resolution strategies:                                               |
 *   |     - Alphabetic cueing (try different first letters)                 |
 *   |     - Context reinstatement (similar emotional state)                 |
 *   |     - Incubation (let spreading activation resolve)                   |
 *   +-----------------------------------------------------------------------+
 *
 *   Confidence Calibration:
 *   +-----------------------------------------------------------------------+
 *   |  Confidence Curve: Maps subjective confidence to actual accuracy      |
 *   |                                                                       |
 *   |  Ideal:    /                 Overconfident:  ---/                     |
 *   |           /                                    /                      |
 *   |          /                  Underconfident:   /                       |
 *   |         /                                  --/                        |
 *   |        /                                                              |
 *   |  0---+-----------1          0---+-----------1                         |
 *   |    Confidence                 Confidence                              |
 *   |                                                                       |
 *   |  Calibration Error = mean(|confidence - accuracy|) across history     |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - FOK detection: O(M) where M = memories to scan (~100-500us typical)
 * - TOT detection: O(M * D) where D = average degree (~1-5ms)
 * - JOL computation: O(1) per memory (~50ns)
 * - Confidence update: O(1) (~10ns)
 * - Full evaluation: O(M + K) where K = top-K neighbors (~1-10ms)
 *
 * MEMORY:
 * - metamemory_t: ~2KB base structure
 * - History buffers: history_size * 8 bytes (default 1KB)
 * - Calibration curve: 40 bytes fixed
 * - Related memory cache: K * sizeof(pr_memory_node_t*) per query
 *
 * THREAD SAFETY:
 * - All public functions are thread-safe via internal mutex
 * - State queries return snapshot (may be stale by return)
 * - Update operations are atomic
 *
 * INTEGRATION:
 * - Depends on: nimcp_pr_memory_node.h for memory node access
 * - Depends on: nimcp_prime_signature.h for partial matching
 * - Depends on: nimcp_resonance.h for familiarity computation
 * - Depends on: nimcp_entanglement.h for related memory search
 * - Depends on: nimcp_quaternion.h for semantic state comparison
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_METAMEMORY_H
#define NIMCP_METAMEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Dependencies
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_prime_signature.h"
#include "cognitive/memory/core/nimcp_resonance.h"
#include "cognitive/memory/core/nimcp_entanglement.h"
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

/** Default FOK familiarity threshold (resonance score) */
#define METAMEM_FOK_FAMILIARITY_THRESHOLD       0.5f

/** Default FOK partial activation threshold */
#define METAMEM_FOK_PARTIAL_THRESHOLD           0.3f

/** Default TOT partial match threshold (50% signature match) */
#define METAMEM_TOT_PARTIAL_MATCH_THRESHOLD     0.5f

/** Default TOT minimum related memories for state */
#define METAMEM_TOT_MIN_RELATED                 3

/** Default confidence history size */
#define METAMEM_DEFAULT_HISTORY_SIZE            128

/** Number of calibration curve deciles */
#define METAMEM_CALIBRATION_BINS                10

/** Default JOL prediction decay rate (per hour) */
#define METAMEM_JOL_DECAY_RATE                  0.1f

/** Minimum confidence value */
#define METAMEM_MIN_CONFIDENCE                  0.0f

/** Maximum confidence value */
#define METAMEM_MAX_CONFIDENCE                  1.0f

/** Maximum related memories to track in TOT state */
#define METAMEM_MAX_RELATED_MEMORIES            32

/** Epsilon for floating-point comparisons */
#define METAMEM_EPSILON                         1e-6f

/** Default partial features to check in TOT */
#define METAMEM_TOT_FEATURES_CHECKED            8

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Metamemory state enumeration
 *
 * WHAT: Categorical state of metacognitive awareness about a memory query
 * WHY:  Different states warrant different retrieval strategies
 * HOW:  Determined by combination of resonance scores and partial match metrics
 */
typedef enum {
    META_STATE_UNKNOWN = 0,      /**< No metacognitive signal - no information */
    META_STATE_KNOWN,            /**< Confident knowledge - full retrieval available */
    META_STATE_FOK,              /**< Feeling of knowing - can't recall but know it's there */
    META_STATE_TOT,              /**< Tip of tongue - partial retrieval, near access */
    META_STATE_UNKNOWN_KNOWN,    /**< Know that we don't know - confident non-knowledge */
    META_STATE_COUNT             /**< Number of states (for array sizing) */
} meta_state_t;

/**
 * @brief TOT resolution strategy enumeration
 *
 * WHAT: Strategies to help resolve tip-of-tongue states
 * WHY:  Different approaches work for different blocking mechanisms
 * HOW:  Selected based on partial information available
 */
typedef enum {
    TOT_STRATEGY_NONE = 0,          /**< No specific strategy */
    TOT_STRATEGY_ALPHABETIC,        /**< Try different first letters */
    TOT_STRATEGY_SEMANTIC,          /**< Explore semantic category */
    TOT_STRATEGY_CONTEXT,           /**< Reinstate encoding context */
    TOT_STRATEGY_INCUBATION,        /**< Wait for spreading activation */
    TOT_STRATEGY_PHONEMIC,          /**< Sound/syllable cues */
    TOT_STRATEGY_ASSOCIATIVE,       /**< Follow association chains */
    TOT_STRATEGY_COUNT              /**< Number of strategies */
} tot_strategy_t;

/**
 * @brief Partial information structure for TOT states
 *
 * WHAT: Information fragments accessible during TOT state
 * WHY:  Captures what partial information can guide retrieval
 * HOW:  Extracted from partial signature matches and related memories
 */
typedef struct {
    uint8_t num_matched_primes;     /**< How many prime factors matched */
    uint8_t total_primes;           /**< Total prime factors in query */
    float match_percentage;          /**< Fraction of signature matched */

    // Feature fragments (if extractable)
    bool has_first_letter;          /**< First letter known */
    char first_letter;              /**< Estimated first letter */
    bool has_syllable_count;        /**< Syllable count known */
    uint8_t syllable_count;         /**< Estimated syllables */
    bool has_category;              /**< Semantic category known */
    uint32_t category_id;           /**< Category identifier */

    // Emotional/state fragments
    bool has_emotional_valence;     /**< Emotional tone known */
    float emotional_valence;        /**< Estimated emotion [-1, +1] */
    bool has_temporal_context;      /**< Time period known */
    uint64_t temporal_hint_ms;      /**< Approximate time context */
} partial_info_t;

/**
 * @brief Metamemory state structure
 *
 * WHAT: Complete metacognitive state for a memory query
 * WHY:  Encapsulates all metamemory information for decision making
 * HOW:  Populated by metamemory_evaluate_query()
 */
typedef struct {
    meta_state_t state;             /**< Current metacognitive state */
    float confidence;               /**< Confidence in state [0, 1] */
    float calibration_error;        /**< Historical confidence vs accuracy gap */

    //-------------------------------------------------------------------------
    // FOK Indicators
    //-------------------------------------------------------------------------
    float familiarity_signal;       /**< From resonance scores (high = familiar) */
    float partial_activation;       /**< Sum of related memory activations */
    uint32_t num_activated;         /**< Count of activated related memories */

    //-------------------------------------------------------------------------
    // TOT Indicators
    //-------------------------------------------------------------------------
    prime_signature_t partial_signature;  /**< Partial signature match */
    size_t partial_features;              /**< How many features matched */
    pr_memory_node_t** related_memories;  /**< Related but not target memories */
    size_t num_related;                   /**< Count of related memories */
    partial_info_t partial_info;          /**< Extractable partial information */
    tot_strategy_t suggested_strategy;    /**< Recommended resolution approach */

    //-------------------------------------------------------------------------
    // JOL Indicators
    //-------------------------------------------------------------------------
    float encoding_strength;        /**< How well encoded initially */
    float retrieval_prediction;     /**< Predicted future retrieval success */
    float predicted_decay_time_hr;  /**< Hours until below retrieval threshold */

    //-------------------------------------------------------------------------
    // Match Information
    //-------------------------------------------------------------------------
    uint64_t best_match_id;         /**< ID of best matching memory (if any) */
    float best_match_score;         /**< Score of best match */
    bool exact_match_found;         /**< Whether exact match exists */

} metamemory_state_t;

/**
 * @brief Metamemory configuration structure
 *
 * WHAT: Parameters controlling metamemory behavior
 * WHY:  Different applications may need different sensitivity
 * HOW:  Set at creation time, some modifiable afterward
 */
typedef struct {
    //-------------------------------------------------------------------------
    // FOK Configuration
    //-------------------------------------------------------------------------
    float fok_familiarity_threshold;    /**< Min resonance for FOK [0, 1] */
    float fok_partial_threshold;        /**< Min partial activation [0, 1] */
    uint32_t fok_min_activated;         /**< Min activated memories for FOK */

    //-------------------------------------------------------------------------
    // TOT Configuration
    //-------------------------------------------------------------------------
    float tot_partial_match_threshold;  /**< Min signature overlap for TOT [0, 1] */
    size_t tot_min_related;             /**< Min related memories for TOT */
    float tot_blocking_threshold;       /**< Activation level considered blocking */

    //-------------------------------------------------------------------------
    // Confidence Configuration
    //-------------------------------------------------------------------------
    size_t history_size;                /**< Size of accuracy history buffer */
    float initial_calibration;          /**< Starting calibration error estimate */
    float calibration_learning_rate;    /**< How fast to update calibration */

    //-------------------------------------------------------------------------
    // JOL Configuration
    //-------------------------------------------------------------------------
    float jol_decay_rate;               /**< Decay rate for JOL predictions */
    float jol_encoding_weight;          /**< Weight for encoding strength in JOL */
    float jol_accessibility_weight;     /**< Weight for quaternion accessibility */

    //-------------------------------------------------------------------------
    // Search Configuration
    //-------------------------------------------------------------------------
    size_t max_related_search;          /**< Max memories to search for relatives */
    float related_threshold;            /**< Resonance threshold for "related" */
    uint32_t spreading_hops;            /**< Hops for spreading activation search */

} metamemory_config_t;

/**
 * @brief Confidence history entry
 *
 * WHAT: Single historical record of confidence vs outcome
 * WHY:  Build calibration curve from past predictions
 */
typedef struct {
    float confidence;               /**< Stated confidence */
    bool was_correct;               /**< Whether retrieval succeeded */
    uint64_t timestamp_ms;          /**< When recorded */
} confidence_record_t;

/**
 * @brief Metamemory statistics
 *
 * WHAT: Operational metrics for metamemory system
 * WHY:  Monitoring, debugging, and calibration analysis
 */
typedef struct {
    uint64_t total_evaluations;     /**< Total query evaluations */
    uint64_t fok_detections;        /**< Times FOK state detected */
    uint64_t tot_detections;        /**< Times TOT state detected */
    uint64_t known_detections;      /**< Times KNOWN state detected */
    uint64_t unknown_detections;    /**< Times UNKNOWN state detected */
    uint64_t unknown_known_detections; /**< Times UNKNOWN_KNOWN detected */

    float mean_confidence;          /**< Average confidence across queries */
    float mean_accuracy;            /**< Actual accuracy rate */
    float current_calibration_error; /**< Current calibration error */

    uint64_t tot_resolutions;       /**< Successful TOT resolutions */
    uint64_t fok_confirmations;     /**< FOK followed by successful recall */

    float calibration_curve[METAMEM_CALIBRATION_BINS]; /**< Current curve */
    uint32_t calibration_counts[METAMEM_CALIBRATION_BINS]; /**< Samples per bin */

} metamemory_stats_t;

/**
 * @brief Opaque metamemory system handle
 *
 * Internal implementation manages:
 * - Integration with PR memory components
 * - Confidence history tracking
 * - Calibration curve maintenance
 * - Query evaluation state
 */
typedef struct metamemory_struct* metamemory_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default metamemory configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for most applications
 *
 * @return Default configuration with:
 *         - fok_familiarity_threshold: 0.5
 *         - fok_partial_threshold: 0.3
 *         - tot_partial_match_threshold: 0.5
 *         - tot_min_related: 3
 *         - history_size: 128
 *
 * Performance: ~5ns
 *
 * Example:
 *   metamemory_config_t config = metamemory_config_default();
 *   config.fok_familiarity_threshold = 0.6f;  // More conservative FOK
 */
NIMCP_EXPORT metamemory_config_t metamemory_config_default(void);

/**
 * @brief Validate metamemory configuration
 *
 * WHAT: Checks configuration parameters are valid
 * WHY:  Prevent invalid configs causing runtime errors
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Validation rules:
 * - All thresholds must be in [0, 1]
 * - history_size must be > 0
 * - tot_min_related must be > 0
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT bool metamemory_config_validate(const metamemory_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create a new metamemory system
 *
 * WHAT: Allocates and initializes metamemory with PR memory integration
 * WHY:  Entry point for metacognitive memory monitoring
 * HOW:  Connects to entanglement graph, node manager, and resonance engine
 *
 * @param entanglement Entanglement graph for related memory search
 * @param node_manager Memory node manager for node access
 * @param resonance Resonance engine for similarity computation
 * @param config Configuration (NULL for defaults)
 * @return Metamemory handle, or NULL on failure
 *
 * Performance: O(history_size) for allocation
 * Memory: ~2KB + history buffers
 *
 * Thread safety: Returned handle is thread-safe
 *
 * Example:
 *   metamemory_t meta = metamemory_create(
 *       entangle_graph, node_manager, resonance, NULL);
 *   if (!meta) {
 *       fprintf(stderr, "Metamemory creation failed: %s\n",
 *               metamemory_get_last_error());
 *   }
 */
NIMCP_EXPORT metamemory_t metamemory_create(
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    resonance_config_t* resonance_config,
    const metamemory_config_t* config
);

/**
 * @brief Destroy metamemory system and free resources
 *
 * WHAT: Deallocates metamemory and all internal buffers
 * WHY:  Resource cleanup
 * HOW:  Frees history buffers, clears state
 *
 * @param meta Metamemory handle to destroy (NULL safe)
 *
 * Performance: O(history_size)
 *
 * Warning: Does not destroy connected components (graph, manager, etc.)
 *
 * Example:
 *   metamemory_destroy(meta);
 *   meta = NULL;
 */
NIMCP_EXPORT void metamemory_destroy(metamemory_t meta);

//=============================================================================
// Query Evaluation Functions
//=============================================================================

/**
 * @brief Evaluate a query for metacognitive state
 *
 * WHAT: Main function - determines metamemory state for a query
 * WHY:  Core functionality for "knowing what you know"
 * HOW:  Searches memories, computes FOK/TOT indicators, returns state
 *
 * ALGORITHM:
 *   1. Search for exact signature match
 *   2. If found: state = KNOWN, confidence from resonance
 *   3. Else compute:
 *      a. Familiarity signal from top-K resonance scores
 *      b. Partial activation from spreading search
 *      c. Partial signature matches
 *   4. Determine state based on thresholds:
 *      - High familiarity + no match = FOK
 *      - Partial match + related active = TOT
 *      - No signals = UNKNOWN or UNKNOWN_KNOWN
 *   5. Compute confidence and calibrated confidence
 *
 * @param meta Metamemory system
 * @param query_signature Prime signature of query
 * @param query_state Quaternion state of query context (can be identity)
 * @param state Output metamemory state (caller-allocated)
 * @return true on success, false on error
 *
 * Performance: O(M + K) typical, where M = memories, K = neighbors
 *
 * Thread safety: Thread-safe, state is consistent snapshot
 *
 * Example:
 *   prime_signature_t* query_sig = prime_sig_from_text("capital of France");
 *   nimcp_quaternion_t context = quat_identity();
 *   metamemory_state_t state;
 *
 *   if (metamemory_evaluate_query(meta, query_sig, context, &state)) {
 *       printf("State: %s, Confidence: %.2f\n",
 *              metamemory_state_name(state.state), state.confidence);
 *       if (state.state == META_STATE_TOT) {
 *           printf("Partial match: %.1f%%\n",
 *                  state.partial_info.match_percentage * 100);
 *       }
 *   }
 */
NIMCP_EXPORT bool metamemory_evaluate_query(
    metamemory_t meta,
    const prime_signature_t* query_signature,
    nimcp_quaternion_t query_state,
    metamemory_state_t* state
);

/**
 * @brief Quick check if memory likely exists (FOK indicator only)
 *
 * WHAT: Fast familiarity check without full TOT analysis
 * WHY:  Quick "do I know this?" decision
 * HOW:  Computes familiarity signal only, skips partial analysis
 *
 * @param meta Metamemory system
 * @param query_signature Query signature
 * @param familiarity_out Output familiarity score [0, 1]
 * @return true if familiarity above FOK threshold, false otherwise
 *
 * Performance: O(log M) for best match search
 *
 * Example:
 *   float familiarity;
 *   if (metamemory_check_familiarity(meta, sig, &familiarity)) {
 *       printf("Probably know this (familiarity: %.2f)\n", familiarity);
 *   }
 */
NIMCP_EXPORT bool metamemory_check_familiarity(
    metamemory_t meta,
    const prime_signature_t* query_signature,
    float* familiarity_out
);

//=============================================================================
// FOK (Feeling of Knowing) Functions
//=============================================================================

/**
 * @brief Compute feeling-of-knowing for a query
 *
 * WHAT: Determines FOK strength for a query signature
 * WHY:  FOK indicates memory exists but isn't accessible
 * HOW:  Combines resonance familiarity with partial activation signals
 *
 * @param meta Metamemory system
 * @param query_signature Query signature
 * @param fok_strength Output FOK strength [0, 1]
 * @return true if FOK detected (above threshold), false otherwise
 *
 * Performance: O(M) for memory scan
 *
 * Example:
 *   float fok;
 *   if (metamemory_compute_fok(meta, sig, &fok)) {
 *       printf("FOK detected with strength %.2f\n", fok);
 *       // Consider providing cues or waiting
 *   }
 */
NIMCP_EXPORT bool metamemory_compute_fok(
    metamemory_t meta,
    const prime_signature_t* query_signature,
    float* fok_strength
);

/**
 * @brief Get memories contributing to FOK signal
 *
 * WHAT: Returns memories that activated during FOK evaluation
 * WHY:  Understanding what's "almost" being retrieved
 * HOW:  Returns top-K memories by partial resonance
 *
 * @param meta Metamemory system
 * @param query_signature Query signature
 * @param memories Output array of memory pointers (caller-allocated)
 * @param max_memories Maximum memories to return
 * @param count Output actual count
 * @return true on success, false on error
 *
 * Performance: O(M + K log K)
 */
NIMCP_EXPORT bool metamemory_get_fok_contributors(
    metamemory_t meta,
    const prime_signature_t* query_signature,
    pr_memory_node_t** memories,
    size_t max_memories,
    size_t* count
);

//=============================================================================
// TOT (Tip of Tongue) Functions
//=============================================================================

/**
 * @brief Detect tip-of-tongue state for a query
 *
 * WHAT: Determines if query is in TOT state
 * WHY:  TOT has specific resolution strategies
 * HOW:  Checks partial signature match + blocking + partial features
 *
 * @param meta Metamemory system
 * @param query_signature Query signature
 * @param partial_info Output partial information (caller-allocated)
 * @return true if TOT detected, false otherwise
 *
 * Performance: O(M * D) for partial matching + spreading
 *
 * Example:
 *   partial_info_t partial;
 *   if (metamemory_detect_tot(meta, sig, &partial)) {
 *       printf("TOT detected! Matched %.1f%%\n",
 *              partial.match_percentage * 100);
 *       if (partial.has_first_letter) {
 *           printf("First letter might be: %c\n", partial.first_letter);
 *       }
 *   }
 */
NIMCP_EXPORT bool metamemory_detect_tot(
    metamemory_t meta,
    const prime_signature_t* query_signature,
    partial_info_t* partial_info
);

/**
 * @brief Get partial information available in TOT state
 *
 * WHAT: Extracts accessible fragments from partial match
 * WHY:  Partial info helps guide retrieval strategies
 * HOW:  Analyzes partial signature and related memory features
 *
 * @param meta Metamemory system
 * @param query_signature Query signature
 * @param info Output partial info structure
 * @return true on success, false on error
 *
 * Performance: O(K) where K = partial matches
 */
NIMCP_EXPORT bool metamemory_get_partial_information(
    metamemory_t meta,
    const prime_signature_t* query_signature,
    partial_info_t* info
);

/**
 * @brief Search for memories related to a TOT query
 *
 * WHAT: Finds memories associated with blocked target
 * WHY:  Related memories may help resolve TOT or reveal blocking
 * HOW:  Spreading activation from partial matches
 *
 * @param meta Metamemory system
 * @param query_signature Query signature
 * @param related Output array of related memory pointers
 * @param max_related Maximum to return
 * @param count Output actual count
 * @return true on success, false on error
 *
 * Performance: O(M + K * D)
 */
NIMCP_EXPORT bool metamemory_search_related(
    metamemory_t meta,
    const prime_signature_t* query_signature,
    pr_memory_node_t** related,
    size_t max_related,
    size_t* count
);

/**
 * @brief Get suggested strategy for resolving TOT state
 *
 * WHAT: Recommends resolution approach based on partial info
 * WHY:  Different blockages need different strategies
 * HOW:  Analyzes partial info to determine best approach
 *
 * Strategy selection:
 * - Has first letter: ALPHABETIC (try variations)
 * - Has category: SEMANTIC (explore category)
 * - Has emotional context: CONTEXT (reinstate mood)
 * - Strong related blocking: INCUBATION (wait for resolution)
 *
 * @param meta Metamemory system
 * @param query_signature Query signature
 * @param partial_info Partial information from detect_tot
 * @return Recommended strategy
 *
 * Performance: O(1)
 */
NIMCP_EXPORT tot_strategy_t metamemory_suggest_tot_strategy(
    metamemory_t meta,
    const prime_signature_t* query_signature,
    const partial_info_t* partial_info
);

/**
 * @brief Attempt to resolve TOT using specified strategy
 *
 * WHAT: Applies resolution strategy to find target
 * WHY:  Actively helps retrieve blocked memory
 * HOW:  Strategy-specific search guided by partial info
 *
 * @param meta Metamemory system
 * @param query_signature Query signature
 * @param strategy Strategy to apply
 * @param partial_info Partial information guiding search
 * @param candidates Output candidate memories
 * @param max_candidates Maximum candidates to return
 * @param count Output actual count
 * @return true if candidates found, false if resolution failed
 *
 * Performance: O(M * strategy_factor)
 */
NIMCP_EXPORT bool metamemory_resolve_tot(
    metamemory_t meta,
    const prime_signature_t* query_signature,
    tot_strategy_t strategy,
    const partial_info_t* partial_info,
    pr_memory_node_t** candidates,
    size_t max_candidates,
    size_t* count
);

//=============================================================================
// JOL (Judgments of Learning) Functions
//=============================================================================

/**
 * @brief Judge learning for a recently encoded memory
 *
 * WHAT: Predicts future retrieval success for a memory
 * WHY:  JOL helps allocate study time and detect weak encodings
 * HOW:  Combines encoding strength, accessibility, and history
 *
 * @param meta Metamemory system
 * @param memory Memory to judge
 * @param prediction_hours Hours in future to predict for
 * @param jol_out Output JOL prediction [0, 1]
 * @return true on success, false on error
 *
 * Performance: O(1)
 *
 * Example:
 *   float jol;
 *   if (metamemory_judge_learning(meta, new_memory, 24.0f, &jol)) {
 *       printf("24h recall probability: %.1f%%\n", jol * 100);
 *       if (jol < 0.5f) {
 *           printf("Consider reinforcing this memory\n");
 *       }
 *   }
 */
NIMCP_EXPORT bool metamemory_judge_learning(
    metamemory_t meta,
    const pr_memory_node_t* memory,
    float prediction_hours,
    float* jol_out
);

/**
 * @brief Predict recall success at specific future time
 *
 * WHAT: Estimates retrieval probability at given time
 * WHY:  Plan rehearsal schedules, anticipate forgetting
 * HOW:  Decay model based on encoding strength and tier
 *
 * @param meta Metamemory system
 * @param memory Memory to predict for
 * @param hours_from_now Hours in future
 * @return Predicted recall probability [0, 1]
 *
 * Performance: O(1)
 */
NIMCP_EXPORT float metamemory_predict_recall(
    metamemory_t meta,
    const pr_memory_node_t* memory,
    float hours_from_now
);

/**
 * @brief Estimate time until memory falls below retrieval threshold
 *
 * WHAT: Predicts when memory will become unretrievable
 * WHY:  Schedule reinforcement before forgetting
 * HOW:  Extrapolates decay curve to threshold crossing
 *
 * @param meta Metamemory system
 * @param memory Memory to analyze
 * @param threshold Retrieval probability threshold
 * @return Hours until below threshold, or INFINITY if never
 *
 * Performance: O(1)
 */
NIMCP_EXPORT float metamemory_estimate_decay_time(
    metamemory_t meta,
    const pr_memory_node_t* memory,
    float threshold
);

//=============================================================================
// Confidence and Calibration Functions
//=============================================================================

/**
 * @brief Get calibrated confidence for a raw confidence value
 *
 * WHAT: Adjusts stated confidence based on historical accuracy
 * WHY:  Raw confidence often miscalibrated; this corrects it
 * HOW:  Maps through learned calibration curve
 *
 * @param meta Metamemory system
 * @param raw_confidence Raw confidence value [0, 1]
 * @return Calibrated confidence [0, 1]
 *
 * Performance: O(1)
 *
 * Example:
 *   float raw = 0.8f;  // I'm 80% sure
 *   float calibrated = metamemory_get_calibrated_confidence(meta, raw);
 *   // If historically overconfident, calibrated might be 0.6
 */
NIMCP_EXPORT float metamemory_get_calibrated_confidence(
    metamemory_t meta,
    float raw_confidence
);

/**
 * @brief Update calibration with feedback
 *
 * WHAT: Records confidence/outcome pair for calibration learning
 * WHY:  Improve future calibration accuracy
 * HOW:  Updates history buffer and recalculates curve
 *
 * @param meta Metamemory system
 * @param confidence Stated confidence [0, 1]
 * @param was_correct Whether retrieval was actually correct
 * @return Current calibration error
 *
 * Performance: O(1) amortized
 *
 * Example:
 *   // After retrieval attempt
 *   float error = metamemory_update_calibration(meta, 0.7f, true);
 *   printf("Calibration error: %.2f\n", error);
 */
NIMCP_EXPORT float metamemory_update_calibration(
    metamemory_t meta,
    float confidence,
    bool was_correct
);

/**
 * @brief Get current calibration error
 *
 * WHAT: Returns mean absolute difference between confidence and accuracy
 * WHY:  Monitor metacognitive accuracy
 * HOW:  Computed from history buffer
 *
 * @param meta Metamemory system
 * @return Calibration error [0, 1], lower is better
 *
 * Performance: O(1)
 */
NIMCP_EXPORT float metamemory_get_calibration_error(metamemory_t meta);

/**
 * @brief Get calibration curve
 *
 * WHAT: Returns the learned confidence-to-accuracy mapping
 * WHY:  Analyze calibration characteristics (over/under confidence)
 * HOW:  Returns copy of internal calibration curve
 *
 * @param meta Metamemory system
 * @param curve_out Output array of size METAMEM_CALIBRATION_BINS
 * @return true on success, false on error
 *
 * Performance: O(CALIBRATION_BINS)
 *
 * Interpretation:
 * - curve[i] = actual accuracy when confidence in bin i
 * - Bin i covers confidence range [i/10, (i+1)/10)
 * - Perfect calibration: curve[i] ≈ (i+0.5)/10
 */
NIMCP_EXPORT bool metamemory_get_calibration_curve(
    metamemory_t meta,
    float* curve_out
);

/**
 * @brief Reset calibration to initial state
 *
 * WHAT: Clears calibration history and resets curve
 * WHY:  Start fresh calibration (e.g., after model change)
 * HOW:  Clears history, sets curve to identity
 *
 * @param meta Metamemory system
 *
 * Performance: O(history_size)
 */
NIMCP_EXPORT void metamemory_reset_calibration(metamemory_t meta);

//=============================================================================
// State Management Functions
//=============================================================================

/**
 * @brief Free resources in metamemory state structure
 *
 * WHAT: Releases memory allocated within state structure
 * WHY:  State contains dynamically allocated related_memories array
 * HOW:  Frees related_memories array if allocated
 *
 * @param state State structure to clean up (not the struct itself)
 *
 * Note: Does NOT free the state structure itself, only its contents
 *
 * Performance: O(1)
 */
NIMCP_EXPORT void metamemory_state_cleanup(metamemory_state_t* state);

/**
 * @brief Initialize a metamemory state structure
 *
 * WHAT: Zero-initializes state and sets safe defaults
 * WHY:  Convenience for creating state structures
 *
 * @param state State structure to initialize
 */
NIMCP_EXPORT void metamemory_state_init(metamemory_state_t* state);

/**
 * @brief Get current metamemory state (last evaluation)
 *
 * WHAT: Returns copy of most recent evaluation result
 * WHY:  Access last state without re-evaluation
 *
 * @param meta Metamemory system
 * @param state Output state structure
 * @return true on success, false if no evaluation performed yet
 *
 * Performance: O(1)
 */
NIMCP_EXPORT bool metamemory_get_current_state(
    metamemory_t meta,
    metamemory_state_t* state
);

//=============================================================================
// Statistics and Utility Functions
//=============================================================================

/**
 * @brief Get metamemory statistics
 *
 * WHAT: Returns operational metrics and state distribution
 * WHY:  Monitoring, debugging, analysis
 *
 * @param meta Metamemory system
 * @param stats Output statistics structure
 * @return true on success, false on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT bool metamemory_get_stats(
    metamemory_t meta,
    metamemory_stats_t* stats
);

/**
 * @brief Reset metamemory statistics
 *
 * WHAT: Clears all counters and metrics
 * WHY:  Start fresh measurement period
 *
 * @param meta Metamemory system
 */
NIMCP_EXPORT void metamemory_reset_stats(metamemory_t meta);

/**
 * @brief Get metamemory state name as string
 *
 * WHAT: Converts state enum to human-readable string
 * WHY:  Debugging, logging
 *
 * @param state Metamemory state
 * @return Static string name (e.g., "FOK", "TOT", "KNOWN")
 */
NIMCP_EXPORT const char* metamemory_state_name(meta_state_t state);

/**
 * @brief Get TOT strategy name as string
 *
 * WHAT: Converts strategy enum to human-readable string
 * WHY:  Debugging, logging
 *
 * @param strategy TOT resolution strategy
 * @return Static string name (e.g., "ALPHABETIC", "SEMANTIC")
 */
NIMCP_EXPORT const char* metamemory_strategy_name(tot_strategy_t strategy);

/**
 * @brief Get last error message
 *
 * WHAT: Returns description of last error
 * WHY:  Debugging failed operations
 *
 * @return Error string, or NULL if no error
 *
 * Thread safety: Error messages are thread-local
 */
NIMCP_EXPORT const char* metamemory_get_last_error(void);

/**
 * @brief Print metamemory state to stdout (debug)
 *
 * WHAT: Human-readable output of metamemory state
 * WHY:  Debugging and inspection
 *
 * @param state State to print
 */
NIMCP_EXPORT void metamemory_state_print(const metamemory_state_t* state);

/**
 * @brief Print metamemory statistics to stdout (debug)
 *
 * @param stats Statistics to print
 */
NIMCP_EXPORT void metamemory_stats_print(const metamemory_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_METAMEMORY_H
