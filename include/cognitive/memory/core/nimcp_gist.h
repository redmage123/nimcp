//=============================================================================
// nimcp_gist.h - Gist Extraction System for Fuzzy Trace Theory Memory
//=============================================================================
/**
 * @file nimcp_gist.h
 * @brief Gist extraction and semantic summary system for Prime Resonant memory
 *
 * WHAT: Extracts core meaning (gist) from detailed memories (verbatim traces)
 * WHY:  Fuzzy Trace Theory shows humans store parallel verbatim and gist
 *       representations with different decay and retrieval properties
 * HOW:  Feature importance weighting, schema mapping, and signature compression
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Fuzzy Trace Theory (Brainerd & Reyna):
 *   +-----------------------------------------------------------------------+
 *   |  Human memory encodes TWO parallel representations:                   |
 *   |                                                                       |
 *   |  VERBATIM TRACE:                                                      |
 *   |  - Exact, specific, surface details                                   |
 *   |  - "She said 'the red car drove by at 3:15 PM'"                      |
 *   |  - Decays quickly (minutes to hours)                                  |
 *   |  - Required for source monitoring and precision                       |
 *   |  - Neural: Hippocampal pattern separation                             |
 *   |                                                                       |
 *   |  GIST TRACE:                                                          |
 *   |  - Semantic core, meaning, essence                                    |
 *   |  - "A car passed by in the afternoon"                                 |
 *   |  - Persists longer (hours to days)                                    |
 *   |  - Basis for reasoning and generalization                             |
 *   |  - Neural: Neocortical schema integration                             |
 *   |                                                                       |
 *   |  Developmental trajectory:                                            |
 *   |  - Children rely more on verbatim (prone to false memory)             |
 *   |  - Adults shift toward gist (better reasoning, some errors)           |
 *   +-----------------------------------------------------------------------+
 *
 *   Gist Extraction Process:
 *   +-----------------------------------------------------------------------+
 *   |                                                                       |
 *   |   [Verbatim Memory]                                                   |
 *   |         |                                                             |
 *   |         v                                                             |
 *   |   +------------+                                                      |
 *   |   | Feature    | Identify high-variance, high-importance features     |
 *   |   | Analysis   |                                                      |
 *   |   +------------+                                                      |
 *   |         |                                                             |
 *   |         v                                                             |
 *   |   +------------+                                                      |
 *   |   | Incidental | Remove context-specific, low-salience details        |
 *   |   | Removal    |                                                      |
 *   |   +------------+                                                      |
 *   |         |                                                             |
 *   |         v                                                             |
 *   |   +------------+                                                      |
 *   |   | Schema     | Map to existing abstract knowledge structures        |
 *   |   | Mapping    |                                                      |
 *   |   +------------+                                                      |
 *   |         |                                                             |
 *   |         v                                                             |
 *   |   +------------+                                                      |
 *   |   | Signature  | Create compressed gist signature                     |
 *   |   | Compress   |                                                      |
 *   |   +------------+                                                      |
 *   |         |                                                             |
 *   |         v                                                             |
 *   |   [Gist Node]                                                         |
 *   |                                                                       |
 *   +-----------------------------------------------------------------------+
 *
 *   Dual Trace Properties:
 *   +-----------------------------------------------------------------------+
 *   | Property        | Verbatim              | Gist                        |
 *   |-----------------|----------------------|------------------------------|
 *   | Specificity     | High (exact details) | Low (semantic core)          |
 *   | Decay Rate      | Fast (hours)         | Slow (days)                  |
 *   | Retrieval Cue   | Surface features     | Meaning/schema               |
 *   | Error Type      | Forgetting           | False recognition            |
 *   | Reasoning Use   | Limited              | Primary                      |
 *   | Neural Basis    | Hippocampus          | Neocortex                    |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Gist extraction: O(n) where n = data size
 * - Dual trace creation: O(n) + extraction overhead
 * - Gist matching: O(k) where k = num key features
 * - Batch extraction: ~10us per memory (amortized)
 *
 * MEMORY:
 * - gist_node_t: ~200 bytes (excluding key features)
 * - dual_trace_t: ~300 bytes
 * - gist_system_t: ~50KB base + O(N) for gists
 *
 * INTEGRATION:
 * - Core: Prime signatures, quaternions, memory nodes
 * - Middleware: Schema system, entanglement graph
 * - API: Memory retrieval with gist matching
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_GIST_H
#define NIMCP_GIST_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Dependencies
#include "cognitive/memory/core/nimcp_quaternion.h"
#include "cognitive/memory/core/nimcp_prime_signature.h"
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_entanglement.h"

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

/** Maximum number of key features per gist */
#define GIST_MAX_KEY_FEATURES           32

/** Maximum number of source memories per gist */
#define GIST_MAX_SOURCES                64

/** Default compression ratio target (gist / verbatim) */
#define GIST_DEFAULT_COMPRESSION        0.25f

/** Default abstractness threshold for gist classification */
#define GIST_ABSTRACTNESS_THRESHOLD     0.5f

/** Default feature importance threshold */
#define GIST_FEATURE_IMPORTANCE_MIN     0.1f

/** Default verbatim decay rate (per hour) - faster than gist */
#define GIST_VERBATIM_DECAY_RATE        0.1f

/** Default gist decay rate (per hour) - slower than verbatim */
#define GIST_GIST_DECAY_RATE            0.02f

/** Minimum coherence for gist-verbatim relationship */
#define GIST_MIN_COHERENCE              0.3f

/** Maximum number of gists in system */
#define GIST_MAX_GISTS                  65536

/** Maximum number of dual traces in system */
#define GIST_MAX_DUAL_TRACES            65536

/** Epsilon for floating-point comparisons */
#define GIST_EPSILON                    1e-6f

/** Invalid gist ID sentinel */
#define GIST_INVALID_ID                 UINT64_MAX

//=============================================================================
// Type Definitions - Enumerations
//=============================================================================

/**
 * @brief Memory trace type classification
 *
 * WHAT: Distinguishes between verbatim (detailed) and gist (abstract) traces
 * WHY:  Different retrieval and decay properties require different handling
 * HOW:  Enum value stored with each memory trace
 */
typedef enum {
    TRACE_VERBATIM = 0,     /**< Detailed, specific memory with surface features */
    TRACE_GIST,             /**< Abstract, semantic core of memory */
    TRACE_HYBRID,           /**< Contains both verbatim and gist components */
    TRACE_TYPE_COUNT        /**< Number of trace types (for array sizing) */
} trace_type_t;

/**
 * @brief Gist extraction method
 *
 * WHAT: Algorithm used for extracting gist from verbatim
 * WHY:  Different methods suit different content types
 */
typedef enum {
    GIST_METHOD_FEATURE_IMPORTANCE = 0, /**< Weight by learned feature importance */
    GIST_METHOD_VARIANCE_BASED,         /**< Keep high-variance features */
    GIST_METHOD_SCHEMA_GUIDED,          /**< Use schema to guide extraction */
    GIST_METHOD_ATTENTION_WEIGHTED,     /**< Weight by attention during encoding */
    GIST_METHOD_FREQUENCY_BASED,        /**< Keep frequently occurring patterns */
    GIST_METHOD_HYBRID,                 /**< Combine multiple methods */
    GIST_METHOD_COUNT                   /**< Number of methods */
} gist_extraction_method_t;

/**
 * @brief Feature type classification
 *
 * WHAT: Categorizes features for importance weighting
 * WHY:  Different feature types have different gist relevance
 */
typedef enum {
    FEATURE_TYPE_SEMANTIC = 0,  /**< Meaning-bearing features (high gist relevance) */
    FEATURE_TYPE_STRUCTURAL,    /**< Structural/syntactic features (medium) */
    FEATURE_TYPE_SURFACE,       /**< Surface/perceptual features (low) */
    FEATURE_TYPE_CONTEXTUAL,    /**< Context-specific features (low for gist) */
    FEATURE_TYPE_TEMPORAL,      /**< Time-related features (variable) */
    FEATURE_TYPE_COUNT          /**< Number of feature types */
} feature_type_t;

/**
 * @brief Gist operation error codes
 */
typedef enum {
    GIST_SUCCESS = 0,                   /**< Operation succeeded */
    GIST_ERROR_NULL_POINTER = -1,       /**< NULL pointer argument */
    GIST_ERROR_INVALID_CONFIG = -2,     /**< Invalid configuration */
    GIST_ERROR_NO_MEMORY = -3,          /**< Memory allocation failed */
    GIST_ERROR_CAPACITY_EXCEEDED = -4,  /**< System capacity exceeded */
    GIST_ERROR_INVALID_ID = -5,         /**< Invalid gist/trace ID */
    GIST_ERROR_LOW_COHERENCE = -6,      /**< Gist-verbatim coherence too low */
    GIST_ERROR_EXTRACTION_FAILED = -7,  /**< Gist extraction algorithm failed */
    GIST_ERROR_SCHEMA_MISMATCH = -8,    /**< Schema mapping failed */
    GIST_ERROR_ALREADY_EXISTS = -9,     /**< Gist already exists for memory */
    GIST_ERROR_NOT_FOUND = -10,         /**< Gist not found */
    GIST_ERROR_INVALID_TRACE = -11,     /**< Invalid trace type for operation */
    GIST_ERROR_MERGE_FAILED = -12       /**< Gist merge operation failed */
} gist_error_t;

//=============================================================================
// Type Definitions - Core Structures
//=============================================================================

/**
 * @brief Dual trace representation (verbatim + gist)
 *
 * WHAT: Paired verbatim and gist representations of same memory
 * WHY:  Fuzzy Trace Theory requires parallel storage with different properties
 * HOW:  Links verbatim and gist signatures with relationship metrics
 *
 * Memory layout: ~300 bytes
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Signatures
    //-------------------------------------------------------------------------
    prime_signature_t verbatim_signature;   /**< Full detailed content signature */
    prime_signature_t gist_signature;       /**< Semantic core signature */

    //-------------------------------------------------------------------------
    // Verbatim Properties
    //-------------------------------------------------------------------------
    float verbatim_strength;                /**< Current verbatim trace strength [0,1] */
    float verbatim_precision;               /**< How specific/detailed (0=vague, 1=precise) */
    float verbatim_decay_rate;              /**< Decay rate per time unit */

    //-------------------------------------------------------------------------
    // Gist Properties
    //-------------------------------------------------------------------------
    float gist_strength;                    /**< Current gist trace strength [0,1] */
    float gist_abstractness;                /**< Abstraction level (0=concrete, 1=abstract) */
    float gist_decay_rate;                  /**< Decay rate per time unit (slower) */

    //-------------------------------------------------------------------------
    // Relationship Metrics
    //-------------------------------------------------------------------------
    float coherence;                        /**< How well gist captures verbatim meaning [0,1] */
    float compression_ratio;                /**< Gist complexity / verbatim complexity */
    float information_retention;            /**< Semantic information preserved [0,1] */

    //-------------------------------------------------------------------------
    // Temporal Information
    //-------------------------------------------------------------------------
    uint64_t created_time_ms;               /**< Creation timestamp */
    uint64_t last_verbatim_access_ms;       /**< Last verbatim retrieval */
    uint64_t last_gist_access_ms;           /**< Last gist retrieval */

    //-------------------------------------------------------------------------
    // Identity
    //-------------------------------------------------------------------------
    uint64_t trace_id;                      /**< Unique dual trace identifier */
    uint64_t source_node_id;                /**< Original memory node ID */

} dual_trace_t;

/**
 * @brief Key feature with importance weight
 *
 * WHAT: A significant feature extracted from verbatim for gist
 * WHY:  Gist is built from high-importance features only
 */
typedef struct {
    uint32_t prime_index;           /**< Index into prime signature */
    float importance;               /**< Learned importance weight [0,1] */
    feature_type_t type;            /**< Feature type classification */
    float variance;                 /**< Feature variance across memories */
    float frequency;                /**< Occurrence frequency in corpus */
} gist_key_feature_t;

/**
 * @brief Gist node - semantic core of memories
 *
 * WHAT: Abstract representation capturing essential meaning
 * WHY:  Enables pattern recognition, reasoning, generalization
 * HOW:  Compressed signature with key features and schema connection
 *
 * Memory layout: ~200 bytes + variable feature array
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Identity
    //-------------------------------------------------------------------------
    uint64_t gist_id;                       /**< Unique gist identifier */

    //-------------------------------------------------------------------------
    // Signature and State
    //-------------------------------------------------------------------------
    prime_signature_t gist_signature;       /**< Compressed semantic signature */
    nimcp_quaternion_t gist_quaternion;     /**< Semantic state (emotion, salience, etc.) */

    //-------------------------------------------------------------------------
    // Source Memories
    //-------------------------------------------------------------------------
    uint64_t* source_memory_ids;            /**< IDs of source verbatim memories */
    size_t num_sources;                     /**< Number of source memories */
    size_t sources_capacity;                /**< Allocated capacity for sources */

    //-------------------------------------------------------------------------
    // Abstraction Level
    //-------------------------------------------------------------------------
    float abstractness;                     /**< How abstract (0=concrete, 1=highly abstract) */
    float generality;                       /**< How many instances covered [1, inf) */
    float confidence;                       /**< Confidence in gist accuracy [0,1] */

    //-------------------------------------------------------------------------
    // Key Features
    //-------------------------------------------------------------------------
    gist_key_feature_t* key_features;       /**< Array of key features */
    size_t num_features;                    /**< Number of key features */
    size_t features_capacity;               /**< Allocated capacity for features */

    //-------------------------------------------------------------------------
    // Schema Connection
    //-------------------------------------------------------------------------
    uint64_t related_schema_id;             /**< ID of related schema (or INVALID) */
    float schema_fit;                       /**< How well gist fits schema [0,1] */

    //-------------------------------------------------------------------------
    // Linked Memory Node (optional)
    //-------------------------------------------------------------------------
    pr_memory_node_t* memory_node;          /**< Optional PR memory node representation */

    //-------------------------------------------------------------------------
    // Temporal and Decay
    //-------------------------------------------------------------------------
    uint64_t created_time_ms;               /**< Creation timestamp */
    uint64_t last_accessed_ms;              /**< Last access timestamp */
    float current_strength;                 /**< Current gist strength [0,1] */
    float decay_rate;                       /**< Decay rate per time unit */

} gist_node_t;

/**
 * @brief Configuration for gist system
 *
 * WHAT: Parameters controlling gist extraction and management
 * WHY:  Tunable to different application needs
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Extraction Parameters
    //-------------------------------------------------------------------------
    gist_extraction_method_t method;        /**< Primary extraction method */
    float compression_target;               /**< Target gist/verbatim ratio [0,1] */
    float feature_importance_threshold;     /**< Minimum feature importance to include */
    size_t max_key_features;                /**< Maximum key features per gist */

    //-------------------------------------------------------------------------
    // Quality Thresholds
    //-------------------------------------------------------------------------
    float min_coherence;                    /**< Minimum gist-verbatim coherence */
    float min_information_retention;        /**< Minimum information preserved */
    float abstractness_target;              /**< Target abstraction level */

    //-------------------------------------------------------------------------
    // Decay Parameters
    //-------------------------------------------------------------------------
    float verbatim_decay_rate;              /**< Verbatim decay per hour */
    float gist_decay_rate;                  /**< Gist decay per hour */
    float decay_differential;               /**< Verbatim/gist decay ratio */

    //-------------------------------------------------------------------------
    // Capacity
    //-------------------------------------------------------------------------
    size_t max_gists;                       /**< Maximum gists in system */
    size_t max_dual_traces;                 /**< Maximum dual traces */

    //-------------------------------------------------------------------------
    // Feature Weights by Type
    //-------------------------------------------------------------------------
    float feature_weights[FEATURE_TYPE_COUNT]; /**< Importance weight per feature type */

} gist_config_t;

/**
 * @brief Gist extraction result
 *
 * WHAT: Output from gist extraction operation
 * WHY:  Provides extraction metrics and quality indicators
 */
typedef struct {
    gist_node_t* gist;                      /**< Extracted gist (owned by system) */
    dual_trace_t* dual_trace;               /**< Created dual trace (owned by system) */
    float extraction_quality;               /**< Overall extraction quality [0,1] */
    float coherence_achieved;               /**< Actual gist-verbatim coherence */
    float compression_achieved;             /**< Actual compression ratio */
    size_t features_extracted;              /**< Number of key features found */
    gist_error_t status;                    /**< Operation status */
} gist_extraction_result_t;

/**
 * @brief Gist match result
 *
 * WHAT: Result of matching a query against gists
 * WHY:  Returns best matches with similarity scores
 */
typedef struct {
    uint64_t gist_id;                       /**< Matched gist ID */
    float similarity;                       /**< Overall similarity score [0,1] */
    float feature_match;                    /**< Key feature match score [0,1] */
    float signature_match;                  /**< Signature Jaccard similarity [0,1] */
    float schema_match;                     /**< Schema compatibility [0,1] */
} gist_match_result_t;

/**
 * @brief Statistics for gist system
 *
 * WHAT: Operational metrics for monitoring
 * WHY:  Track system health and performance
 */
typedef struct {
    size_t num_gists;                       /**< Current gist count */
    size_t num_dual_traces;                 /**< Current dual trace count */
    size_t num_extractions;                 /**< Total extractions performed */
    size_t num_merges;                      /**< Total gist merges */
    float avg_compression;                  /**< Average compression ratio */
    float avg_coherence;                    /**< Average gist-verbatim coherence */
    float avg_abstractness;                 /**< Average gist abstractness */
    uint64_t total_features;                /**< Total key features across gists */
    size_t memory_bytes;                    /**< Approximate memory usage */
} gist_stats_t;

//=============================================================================
// Opaque System Handle
//=============================================================================

/**
 * @brief Opaque gist system handle
 *
 * Internal implementation includes:
 * - Hash table for gist lookup
 * - Dual trace storage
 * - Feature importance model
 * - Schema integration hooks
 */
typedef struct gist_system_struct* gist_system_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default gist configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for most applications
 *
 * @return Default configuration with:
 *         - method: GIST_METHOD_FEATURE_IMPORTANCE
 *         - compression_target: 0.25
 *         - feature_importance_threshold: 0.1
 *         - max_key_features: 32
 *         - min_coherence: 0.3
 *         - verbatim_decay_rate: 0.1/hour
 *         - gist_decay_rate: 0.02/hour
 *
 * Performance: ~5ns
 *
 * Example:
 *   gist_config_t config = gist_config_default();
 *   config.compression_target = 0.3f;  // Less aggressive compression
 *   gist_system_t sys = gist_system_create(&config, entangle, node_mgr);
 */
NIMCP_EXPORT gist_config_t gist_config_default(void);

/**
 * @brief Validate gist configuration
 *
 * WHAT: Checks configuration values are valid
 * WHY:  Prevent invalid configs causing runtime errors
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Validation rules:
 * - compression_target in (0, 1]
 * - All thresholds in [0, 1]
 * - Decay rates >= 0
 * - Max values > 0
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT bool gist_config_validate(const gist_config_t* config);

//=============================================================================
// System Lifecycle Functions
//=============================================================================

/**
 * @brief Create gist extraction system
 *
 * WHAT: Allocates and initializes gist system
 * WHY:  Entry point for gist-based memory operations
 * HOW:  Creates storage, initializes feature weights, connects to subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param entanglement Entanglement graph for memory associations
 * @param node_manager Node manager for memory node creation
 * @return Gist system handle, or NULL on failure
 *
 * Performance: O(1)
 * Memory: ~50KB base
 *
 * Example:
 *   gist_config_t config = gist_config_default();
 *   gist_system_t sys = gist_system_create(&config, entangle_graph, node_mgr);
 *   if (!sys) {
 *       fprintf(stderr, "Failed: %s\n", gist_get_last_error());
 *   }
 */
NIMCP_EXPORT gist_system_t gist_system_create(
    const gist_config_t* config,
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager
);

/**
 * @brief Destroy gist system
 *
 * WHAT: Frees all resources including stored gists
 * WHY:  Resource cleanup
 *
 * @param system System to destroy (NULL safe)
 *
 * Performance: O(N) where N = stored gists
 *
 * WARNING: Invalidates all gist_node_t* and dual_trace_t* returned by system
 */
NIMCP_EXPORT void gist_system_destroy(gist_system_t system);

/**
 * @brief Clear all gists from system
 *
 * WHAT: Removes all gists and dual traces
 * WHY:  Reset system without reallocation
 *
 * @param system System to clear
 * @return GIST_SUCCESS or error code
 *
 * Performance: O(N)
 */
NIMCP_EXPORT gist_error_t gist_system_clear(gist_system_t system);

//=============================================================================
// Gist Extraction Functions
//=============================================================================

/**
 * @brief Extract gist from a memory node
 *
 * WHAT: Creates gist (semantic core) from verbatim memory
 * WHY:  Core operation for Fuzzy Trace Theory implementation
 * HOW:  Identifies key features, removes incidentals, compresses signature
 *
 * ALGORITHM:
 *   1. Analyze memory signature for feature importance
 *   2. Identify key features (above threshold)
 *   3. Remove low-importance surface details
 *   4. Map to schema if available
 *   5. Generate compressed gist signature
 *   6. Create dual trace linking verbatim and gist
 *   7. Compute coherence and quality metrics
 *
 * @param system Gist system
 * @param memory Source memory node
 * @param result Output extraction result
 * @return GIST_SUCCESS or error code
 *
 * Performance: O(n) where n = memory data size
 *
 * Example:
 *   pr_memory_node_t* memory = pr_memory_node_create(mgr, data, size, NULL);
 *   gist_extraction_result_t result;
 *   if (gist_extract(system, memory, &result) == GIST_SUCCESS) {
 *       printf("Gist extracted: %lu features, %.2f compression\n",
 *              result.features_extracted, result.compression_achieved);
 *   }
 */
NIMCP_EXPORT gist_error_t gist_extract(
    gist_system_t system,
    const pr_memory_node_t* memory,
    gist_extraction_result_t* result
);

/**
 * @brief Extract gist with custom parameters
 *
 * WHAT: Extraction with method and threshold overrides
 * WHY:  Fine-grained control for specific use cases
 *
 * @param system Gist system
 * @param memory Source memory node
 * @param method Extraction method override
 * @param compression_target Compression ratio override
 * @param result Output extraction result
 * @return GIST_SUCCESS or error code
 *
 * Performance: O(n)
 */
NIMCP_EXPORT gist_error_t gist_extract_custom(
    gist_system_t system,
    const pr_memory_node_t* memory,
    gist_extraction_method_t method,
    float compression_target,
    gist_extraction_result_t* result
);

/**
 * @brief Extract gist from multiple memories (batch)
 *
 * WHAT: Extracts gists from array of memories
 * WHY:  More efficient than individual calls
 * HOW:  Shared computation and amortized overhead
 *
 * @param system Gist system
 * @param memories Array of memory nodes
 * @param count Number of memories
 * @param results Output array of results (must be pre-allocated)
 * @return Number of successful extractions, or -1 on error
 *
 * Performance: ~10us per memory (amortized)
 *
 * Example:
 *   pr_memory_node_t* memories[100];
 *   gist_extraction_result_t results[100];
 *   // ... fill memories ...
 *   int extracted = gist_extract_batch(system, memories, 100, results);
 */
NIMCP_EXPORT int gist_extract_batch(
    gist_system_t system,
    const pr_memory_node_t** memories,
    size_t count,
    gist_extraction_result_t* results
);

/**
 * @brief Create dual trace without gist extraction
 *
 * WHAT: Creates verbatim + gist pairing with explicit gist signature
 * WHY:  Allow external gist computation or schema-defined gists
 *
 * @param system Gist system
 * @param memory Verbatim memory
 * @param gist_signature Pre-computed gist signature
 * @param abstractness Abstractness level [0,1]
 * @return Created dual trace, or NULL on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT dual_trace_t* gist_create_dual_trace(
    gist_system_t system,
    const pr_memory_node_t* memory,
    const prime_signature_t* gist_signature,
    float abstractness
);

//=============================================================================
// Gist Compression and Expansion Functions
//=============================================================================

/**
 * @brief Compress verbatim signature to gist
 *
 * WHAT: Creates compressed gist signature from full signature
 * WHY:  Direct signature manipulation for custom pipelines
 * HOW:  Keeps only high-importance prime indices
 *
 * @param system Gist system (for feature weights)
 * @param verbatim_sig Source verbatim signature
 * @param target_ratio Target compression ratio [0,1]
 * @param gist_sig_out Output compressed signature
 * @return GIST_SUCCESS or error code
 *
 * Performance: O(PRIME_SIG_DIM)
 *
 * Example:
 *   prime_signature_t gist_sig;
 *   gist_compress(system, &verbatim_sig, 0.25f, &gist_sig);
 *   // gist_sig has ~25% of original complexity
 */
NIMCP_EXPORT gist_error_t gist_compress(
    gist_system_t system,
    const prime_signature_t* verbatim_sig,
    float target_ratio,
    prime_signature_t* gist_sig_out
);

/**
 * @brief Expand gist signature to estimate verbatim
 *
 * WHAT: Reconstructs approximate verbatim from gist
 * WHY:  Pattern completion from semantic core
 * HOW:  Uses learned correlations to fill missing features
 *
 * NOTE: This is an approximation - original verbatim cannot be
 * perfectly recovered from gist (information was lost).
 *
 * @param system Gist system
 * @param gist_sig Gist signature to expand
 * @param verbatim_estimate Output estimated verbatim signature
 * @return GIST_SUCCESS or error code
 *
 * Performance: O(PRIME_SIG_DIM)
 *
 * Example:
 *   prime_signature_t expanded;
 *   gist_expand(system, &gist_node->gist_signature, &expanded);
 *   // expanded is best guess at original verbatim
 */
NIMCP_EXPORT gist_error_t gist_expand(
    gist_system_t system,
    const prime_signature_t* gist_sig,
    prime_signature_t* verbatim_estimate
);

//=============================================================================
// Feature Analysis Functions
//=============================================================================

/**
 * @brief Identify key features from memory signature
 *
 * WHAT: Extracts high-importance features for gist
 * WHY:  Core step in gist extraction algorithm
 * HOW:  Scores features by importance, variance, type
 *
 * @param system Gist system
 * @param signature Memory signature to analyze
 * @param features Output array of key features
 * @param max_features Maximum features to return
 * @param num_features Output: actual features found
 * @return GIST_SUCCESS or error code
 *
 * Performance: O(PRIME_SIG_DIM log PRIME_SIG_DIM) for sorting
 *
 * Example:
 *   gist_key_feature_t features[32];
 *   size_t count;
 *   gist_identify_key_features(system, &sig, features, 32, &count);
 *   for (size_t i = 0; i < count; i++) {
 *       printf("Feature %u: importance=%.3f\n",
 *              features[i].prime_index, features[i].importance);
 *   }
 */
NIMCP_EXPORT gist_error_t gist_identify_key_features(
    gist_system_t system,
    const prime_signature_t* signature,
    gist_key_feature_t* features,
    size_t max_features,
    size_t* num_features
);

/**
 * @brief Compute abstractness level of signature
 *
 * WHAT: Measures how abstract vs concrete a signature is
 * WHY:  Classify memories along verbatim-gist spectrum
 * HOW:  Ratio of semantic to surface features
 *
 * @param system Gist system
 * @param signature Signature to analyze
 * @return Abstractness level [0=concrete, 1=abstract]
 *
 * Performance: O(PRIME_SIG_DIM)
 *
 * Example:
 *   float abs = gist_compute_abstractness(system, &sig);
 *   if (abs > 0.7f) {
 *       printf("Highly abstract representation\n");
 *   }
 */
NIMCP_EXPORT float gist_compute_abstractness(
    gist_system_t system,
    const prime_signature_t* signature
);

/**
 * @brief Update feature importance weights (learning)
 *
 * WHAT: Updates feature importance based on retrieval success
 * WHY:  Learn which features are most useful for gist
 * HOW:  Reinforcement based on retrieval accuracy
 *
 * @param system Gist system
 * @param feature_index Prime index of feature
 * @param success_delta How well feature helped retrieval [-1,+1]
 * @param learning_rate Learning rate [0,1]
 * @return GIST_SUCCESS or error code
 *
 * Performance: O(1)
 */
NIMCP_EXPORT gist_error_t gist_update_feature_importance(
    gist_system_t system,
    uint32_t feature_index,
    float success_delta,
    float learning_rate
);

/**
 * @brief Get feature importance weight
 *
 * @param system Gist system
 * @param feature_index Prime index
 * @return Current importance weight [0,1]
 */
NIMCP_EXPORT float gist_get_feature_importance(
    gist_system_t system,
    uint32_t feature_index
);

//=============================================================================
// Gist Matching and Retrieval Functions
//=============================================================================

/**
 * @brief Match query against stored gists
 *
 * WHAT: Finds gists similar to query signature/state
 * WHY:  Gist-based retrieval for reasoning and generalization
 * HOW:  Multi-component matching (signature, features, schema)
 *
 * @param system Gist system
 * @param query_sig Query signature
 * @param query_quat Query quaternion state (can be NULL)
 * @param results Output array of match results
 * @param max_results Maximum results to return
 * @param num_results Output: actual results found
 * @return GIST_SUCCESS or error code
 *
 * Performance: O(N log K) where N = gists, K = max_results
 *
 * Example:
 *   gist_match_result_t matches[10];
 *   size_t count;
 *   gist_match(system, &query_sig, NULL, matches, 10, &count);
 *   for (size_t i = 0; i < count; i++) {
 *       printf("Match %lu: similarity=%.3f\n",
 *              matches[i].gist_id, matches[i].similarity);
 *   }
 */
NIMCP_EXPORT gist_error_t gist_match(
    gist_system_t system,
    const prime_signature_t* query_sig,
    const nimcp_quaternion_t* query_quat,
    gist_match_result_t* results,
    size_t max_results,
    size_t* num_results
);

/**
 * @brief Retrieve verbatim memories from gist
 *
 * WHAT: Gets source verbatim memories that contributed to gist
 * WHY:  Navigate from abstract to specific
 * HOW:  Returns memory IDs linked to gist
 *
 * @param system Gist system
 * @param gist_id Gist identifier
 * @param memory_ids Output array of memory IDs
 * @param max_memories Maximum memories to return
 * @param num_memories Output: actual memories found
 * @return GIST_SUCCESS or error code
 *
 * Performance: O(K) where K = num source memories
 *
 * Example:
 *   uint64_t memory_ids[64];
 *   size_t count;
 *   gist_retrieve_verbatim(system, gist_id, memory_ids, 64, &count);
 */
NIMCP_EXPORT gist_error_t gist_retrieve_verbatim(
    gist_system_t system,
    uint64_t gist_id,
    uint64_t* memory_ids,
    size_t max_memories,
    size_t* num_memories
);

/**
 * @brief Get gist node by ID
 *
 * @param system Gist system
 * @param gist_id Gist identifier
 * @return Gist node pointer, or NULL if not found
 *
 * NOTE: Returned pointer is valid until gist is removed or system destroyed
 */
NIMCP_EXPORT gist_node_t* gist_get_by_id(
    gist_system_t system,
    uint64_t gist_id
);

/**
 * @brief Get dual trace by ID
 *
 * @param system Gist system
 * @param trace_id Dual trace identifier
 * @return Dual trace pointer, or NULL if not found
 */
NIMCP_EXPORT dual_trace_t* gist_get_dual_trace(
    gist_system_t system,
    uint64_t trace_id
);

//=============================================================================
// Gist Merge and Generalization Functions
//=============================================================================

/**
 * @brief Merge similar gists into more general gist
 *
 * WHAT: Combines multiple gists into single abstract gist
 * WHY:  Hierarchical abstraction and category formation
 * HOW:  Intersect signatures, average states, combine features
 *
 * @param system Gist system
 * @param gist_ids Array of gist IDs to merge
 * @param count Number of gists to merge
 * @param merged_gist Output: new merged gist
 * @return GIST_SUCCESS or error code
 *
 * Performance: O(count * PRIME_SIG_DIM)
 *
 * Example:
 *   uint64_t similar_gists[] = {gist1, gist2, gist3};
 *   gist_node_t* merged;
 *   gist_merge(system, similar_gists, 3, &merged);
 *   // merged represents abstraction over gist1, gist2, gist3
 */
NIMCP_EXPORT gist_error_t gist_merge(
    gist_system_t system,
    const uint64_t* gist_ids,
    size_t count,
    gist_node_t** merged_gist
);

/**
 * @brief Generalize gist from multiple instances
 *
 * WHAT: Create higher-level gist from memory instances
 * WHY:  Category learning and prototype formation
 * HOW:  Extract common features across instances
 *
 * @param system Gist system
 * @param memory_ids Array of memory IDs representing instances
 * @param count Number of instances
 * @param generality_target Target generality level [1, inf)
 * @param generalized_gist Output: new generalized gist
 * @return GIST_SUCCESS or error code
 *
 * Performance: O(count * n) where n = avg memory size
 *
 * Example:
 *   // Generalize from 5 examples of "dog"
 *   uint64_t dog_memories[] = {m1, m2, m3, m4, m5};
 *   gist_node_t* dog_concept;
 *   gist_generalize(system, dog_memories, 5, 5.0f, &dog_concept);
 */
NIMCP_EXPORT gist_error_t gist_generalize(
    gist_system_t system,
    const uint64_t* memory_ids,
    size_t count,
    float generality_target,
    gist_node_t** generalized_gist
);

//=============================================================================
// Decay and Forgetting Functions
//=============================================================================

/**
 * @brief Apply differential forgetting to dual trace
 *
 * WHAT: Applies decay to verbatim (fast) and gist (slow) separately
 * WHY:  Models biological memory where details fade before meaning
 * HOW:  Exponential decay with different rates
 *
 * @param system Gist system
 * @param trace Dual trace to decay
 * @param elapsed_hours Time since last decay application
 * @return GIST_SUCCESS or error code
 *
 * ALGORITHM:
 *   verbatim_strength *= exp(-verbatim_decay_rate * elapsed_hours)
 *   gist_strength *= exp(-gist_decay_rate * elapsed_hours)
 *
 * Performance: O(1)
 *
 * Example:
 *   // Apply 1 hour of decay
 *   gist_apply_forgetting(system, trace, 1.0f);
 *   // Verbatim decayed ~10%, gist decayed ~2%
 */
NIMCP_EXPORT gist_error_t gist_apply_forgetting(
    gist_system_t system,
    dual_trace_t* trace,
    float elapsed_hours
);

/**
 * @brief Apply forgetting to all dual traces
 *
 * WHAT: Batch decay for time progression
 * WHY:  Efficient system-wide decay
 *
 * @param system Gist system
 * @param elapsed_hours Time since last decay
 * @return Number of traces affected
 *
 * Performance: O(N) where N = dual traces
 */
NIMCP_EXPORT size_t gist_apply_forgetting_all(
    gist_system_t system,
    float elapsed_hours
);

/**
 * @brief Reinforce dual trace (counter decay)
 *
 * WHAT: Increases trace strengths after retrieval
 * WHY:  Model retrieval-induced strengthening
 *
 * @param system Gist system
 * @param trace Dual trace to reinforce
 * @param verbatim_boost Verbatim reinforcement [0,1]
 * @param gist_boost Gist reinforcement [0,1]
 * @return GIST_SUCCESS or error code
 *
 * Performance: O(1)
 */
NIMCP_EXPORT gist_error_t gist_reinforce(
    gist_system_t system,
    dual_trace_t* trace,
    float verbatim_boost,
    float gist_boost
);

/**
 * @brief Remove decayed gists below threshold
 *
 * WHAT: Garbage collection for weak gists
 * WHY:  Free resources from forgotten memories
 *
 * @param system Gist system
 * @param strength_threshold Remove gists below this strength
 * @return Number of gists removed
 *
 * Performance: O(N)
 */
NIMCP_EXPORT size_t gist_prune_weak(
    gist_system_t system,
    float strength_threshold
);

//=============================================================================
// Statistics and Information Functions
//=============================================================================

/**
 * @brief Get gist system statistics
 *
 * @param system Gist system
 * @param stats Output statistics
 * @return GIST_SUCCESS or error code
 *
 * Performance: O(N) for some metrics
 */
NIMCP_EXPORT gist_error_t gist_get_stats(
    gist_system_t system,
    gist_stats_t* stats
);

/**
 * @brief Get gist count
 *
 * @param system Gist system
 * @return Number of gists in system
 */
NIMCP_EXPORT size_t gist_get_count(gist_system_t system);

/**
 * @brief Get dual trace count
 *
 * @param system Gist system
 * @return Number of dual traces in system
 */
NIMCP_EXPORT size_t gist_get_dual_trace_count(gist_system_t system);

/**
 * @brief Get trace type name as string
 *
 * @param type Trace type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* gist_trace_type_name(trace_type_t type);

/**
 * @brief Get extraction method name as string
 *
 * @param method Extraction method
 * @return Human-readable name
 */
NIMCP_EXPORT const char* gist_method_name(gist_extraction_method_t method);

/**
 * @brief Get error string for error code
 *
 * @param error Error code
 * @return Human-readable error message
 */
NIMCP_EXPORT const char* gist_error_string(gist_error_t error);

/**
 * @brief Get last error message
 *
 * @return Error string from last failed operation, or NULL
 */
NIMCP_EXPORT const char* gist_get_last_error(void);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Print gist node for debugging
 *
 * @param gist Gist to print
 */
NIMCP_EXPORT void gist_print(const gist_node_t* gist);

/**
 * @brief Print dual trace for debugging
 *
 * @param trace Dual trace to print
 */
NIMCP_EXPORT void gist_dual_trace_print(const dual_trace_t* trace);

/**
 * @brief Validate gist node integrity
 *
 * @param gist Gist to validate
 * @return true if valid, false if corrupted
 */
NIMCP_EXPORT bool gist_validate(const gist_node_t* gist);

/**
 * @brief Compute coherence between verbatim and gist signatures
 *
 * @param verbatim Verbatim signature
 * @param gist Gist signature
 * @return Coherence score [0,1]
 */
NIMCP_EXPORT float gist_compute_coherence(
    const prime_signature_t* verbatim,
    const prime_signature_t* gist
);

/**
 * @brief Get current time in milliseconds
 *
 * @return Milliseconds since epoch
 */
NIMCP_EXPORT uint64_t gist_current_time_ms(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GIST_H
