//=============================================================================
// nimcp_source_memory.h - Source Memory System for Memory Origin Tracking
//=============================================================================
/**
 * @file nimcp_source_memory.h
 * @brief Source memory for tracking where, when, and how information was acquired
 *
 * WHAT: Episodic-like memory for information origin and context
 * WHY:  Accurate recall requires knowing not just WHAT but WHERE/WHEN/HOW learned
 * HOW:  Bind source attributes to memory nodes, support reality monitoring and
 *       false memory detection through perceptual/cognitive feature analysis
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Source Memory in the Brain:
 *   +-----------------------------------------------------------------------+
 *   |  Source memory = memory for the origin and context of learned info   |
 *   |                                                                       |
 *   |  Neural Basis:                                                        |
 *   |  - Hippocampus: Binds content with spatiotemporal context             |
 *   |  - Prefrontal Cortex: Source monitoring and reality checks            |
 *   |  - Medial Temporal Lobe: Recollective vs familiarity signals          |
 *   |  - Posterior Parietal: Retrieval orientation and attention            |
 *   |                                                                       |
 *   |  Source Types:                                                        |
 *   |  - Perceived: Directly experienced through senses                     |
 *   |  - Told: Information received from others                             |
 *   |  - Read: Acquired from text/media                                     |
 *   |  - Inferred: Derived through reasoning                                |
 *   |  - Imagined: Generated internally, may be confused with real          |
 *   +-----------------------------------------------------------------------+
 *
 *   Reality Monitoring Model (Johnson & Raye, 1981):
 *   +-----------------------------------------------------------------------+
 *   |  Distinguishing real from imagined memories based on:                 |
 *   |                                                                       |
 *   |  PERCEPTUAL DETAIL:                                                   |
 *   |  - Real memories: More sensory detail (visual, auditory, etc.)        |
 *   |  - Imagined: Less vivid, more schema-consistent                       |
 *   |                                                                       |
 *   |  CONTEXTUAL DETAIL:                                                   |
 *   |  - Real memories: More spatiotemporal context                         |
 *   |  - Imagined: Less specific context, more floating                     |
 *   |                                                                       |
 *   |  COGNITIVE OPERATIONS:                                                |
 *   |  - Real memories: Less cognitive elaboration needed                   |
 *   |  - Imagined: More reasoning/effort to generate                        |
 *   |                                                                       |
 *   |  SEMANTIC DETAIL:                                                     |
 *   |  - Real memories: Balanced perceptual and semantic                    |
 *   |  - Imagined: Often more semantic/meaningful, less sensory             |
 *   +-----------------------------------------------------------------------+
 *
 *   False Memory Formation:
 *   +-----------------------------------------------------------------------+
 *   |  False memories can arise from:                                       |
 *   |                                                                       |
 *   |  1. SCHEMA CONSISTENCY:                                               |
 *   |     - Memory fits expectations too perfectly                          |
 *   |     - High schema match without source encoding                       |
 *   |                                                                       |
 *   |  2. MISINFORMATION EFFECT:                                            |
 *   |     - Post-event suggestions alter memory                             |
 *   |     - Source confusion between original and suggested                 |
 *   |                                                                       |
 *   |  3. IMAGINATION INFLATION:                                            |
 *   |     - Repeatedly imagining increases false belief                     |
 *   |     - Familiarity misattributed to occurrence                         |
 *   |                                                                       |
 *   |  4. SOURCE AMNESIA:                                                   |
 *   |     - Content preserved but source forgotten                          |
 *   |     - Leads to misattribution and false confidence                    |
 *   |                                                                       |
 *   |  5. EMOTIONAL DISTORTION:                                             |
 *   |     - High emotion enhances gist but may distort details              |
 *   |     - "Flashbulb" memories often less accurate than believed          |
 *   +-----------------------------------------------------------------------+
 *
 *   Source Forgetting Rate:
 *   +-----------------------------------------------------------------------+
 *   |  Source memory decays FASTER than content memory:                     |
 *   |                                                                       |
 *   |  Time ->     |  Content  |  Source  |                                 |
 *   |  ------------|-----------|----------|                                 |
 *   |  Immediate   |   100%    |   100%   |                                 |
 *   |  1 day       |    85%    |    60%   |                                 |
 *   |  1 week      |    70%    |    35%   |                                 |
 *   |  1 month     |    60%    |    20%   |                                 |
 *   |                                                                       |
 *   |  This asymmetry can lead to "knowing" without "remembering how"       |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Source bind: O(1) average (hash table)
 * - Source lookup: O(1) average
 * - Reality monitoring: ~50ns (weighted feature combination)
 * - False memory risk: ~100ns (multiple feature analysis)
 * - Query by source: O(N) where N = entries (or O(log N) with index)
 *
 * MEMORY:
 * - source_attribute_t: ~128 bytes
 * - source_memory_entry_t: ~192 bytes
 * - source_memory_t: ~64 bytes + entries
 *
 * INTEGRATION:
 * - Core: PR Memory Node, Entanglement Graph
 * - Middleware: Metamemory monitoring, reconsolidation
 * - API: Memory query with source filtering
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_SOURCE_MEMORY_H
#define NIMCP_SOURCE_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

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

/** Maximum length for source agent name */
#define SOURCE_MAX_AGENT_NAME_LEN       128

/** Default source decay rate (faster than content decay) */
#define SOURCE_DEFAULT_DECAY_RATE       0.002f

/** Default perceptual threshold for reality monitoring */
#define SOURCE_DEFAULT_PERCEPTUAL_THRESHOLD   0.6f

/** Default cognitive threshold for reality monitoring */
#define SOURCE_DEFAULT_COGNITIVE_THRESHOLD    0.4f

/** Default false memory risk threshold */
#define SOURCE_DEFAULT_FALSE_MEMORY_THRESHOLD 0.7f

/** Maximum entries per source memory system */
#define SOURCE_MAX_ENTRIES              (1ULL << 20)  // ~1M entries

/** Maximum tracked agents */
#define SOURCE_MAX_AGENTS               1024

/** Default initial entry capacity */
#define SOURCE_DEFAULT_ENTRY_CAPACITY   256

/** Epsilon for floating-point comparisons */
#define SOURCE_EPSILON                  1e-6f

/** Invalid agent ID sentinel */
#define SOURCE_INVALID_AGENT_ID         UINT64_MAX

/** Source memory serialization magic */
#define SOURCE_MEMORY_MAGIC             0x534F5243  // "SORC"

/** Source memory serialization version */
#define SOURCE_MEMORY_VERSION           1

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Source type enumeration
 *
 * WHAT: How the information was originally acquired
 * WHY:  Different sources have different reliability profiles
 * HOW:  Enum stored with each memory's source attributes
 */
typedef enum {
    SOURCE_PERCEIVED = 0,   /**< Directly experienced through senses */
    SOURCE_TOLD,            /**< Told by another person/agent */
    SOURCE_READ,            /**< Read from text/media */
    SOURCE_INFERRED,        /**< Derived through reasoning */
    SOURCE_IMAGINED,        /**< Imagined or simulated internally */
    SOURCE_UNKNOWN,         /**< Source has been forgotten */
    SOURCE_TYPE_COUNT       /**< Number of source types */
} source_type_t;

/**
 * @brief Reality status enumeration
 *
 * WHAT: Confidence in whether memory represents real vs imagined event
 * WHY:  Reality monitoring is crucial for accurate recall
 * HOW:  Computed from perceptual, contextual, cognitive features
 */
typedef enum {
    REALITY_CERTAIN_REAL = 0,    /**< Definitely happened (high perceptual, low cognitive) */
    REALITY_PROBABLY_REAL,       /**< Likely happened */
    REALITY_UNCERTAIN,           /**< Unknown if real or imagined */
    REALITY_PROBABLY_IMAGINED,   /**< Likely imagined/simulated */
    REALITY_CERTAIN_IMAGINED,    /**< Definitely imagined */
    REALITY_STATUS_COUNT         /**< Number of reality statuses */
} reality_status_t;

/**
 * @brief Modality flags for how information was acquired
 */
typedef enum {
    MODALITY_NONE       = 0x00, /**< No modality specified */
    MODALITY_VISUAL     = 0x01, /**< Seen visually */
    MODALITY_AUDITORY   = 0x02, /**< Heard auditorily */
    MODALITY_READ       = 0x04, /**< Read from text */
    MODALITY_SPOKEN     = 0x08, /**< Spoken by self or other */
    MODALITY_TACTILE    = 0x10, /**< Felt through touch */
    MODALITY_INTERNAL   = 0x20  /**< Generated internally (thought/imagined) */
} source_modality_t;

/**
 * @brief Error codes for source memory operations
 */
typedef enum {
    SOURCE_SUCCESS = 0,                     /**< Operation succeeded */
    SOURCE_ERROR_NULL_POINTER = -1,         /**< NULL pointer argument */
    SOURCE_ERROR_NOT_FOUND = -2,            /**< Entry not found */
    SOURCE_ERROR_ALREADY_EXISTS = -3,       /**< Entry already exists */
    SOURCE_ERROR_NO_MEMORY = -4,            /**< Memory allocation failed */
    SOURCE_ERROR_FULL = -5,                 /**< Maximum entries reached */
    SOURCE_ERROR_INVALID_PARAM = -6,        /**< Invalid parameter value */
    SOURCE_ERROR_INVALID_AGENT = -7,        /**< Invalid agent ID */
    SOURCE_ERROR_SERIALIZE = -8,            /**< Serialization failed */
    SOURCE_ERROR_DESERIALIZE = -9,          /**< Deserialization failed */
    SOURCE_ERROR_VERSION_MISMATCH = -10     /**< Incompatible version */
} source_error_t;

/**
 * @brief Source attributes for a memory
 *
 * WHAT: Detailed information about where, when, how memory was acquired
 * WHY:  Enable source monitoring, verification, and reality checks
 * HOW:  Comprehensive metadata bound to each memory node
 *
 * Memory layout: ~128 bytes
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Source Type
    //-------------------------------------------------------------------------
    source_type_t type;             /**< How information was acquired */

    //-------------------------------------------------------------------------
    // Source Agent Information
    //-------------------------------------------------------------------------
    uint64_t source_agent_id;       /**< ID of person/entity who provided info (0 if self) */
    char* source_agent_name;        /**< Name of source agent (may be NULL) */
    float source_credibility;       /**< Trust in this source [0-1] */

    //-------------------------------------------------------------------------
    // Temporal Context
    //-------------------------------------------------------------------------
    float acquisition_time;         /**< When info was acquired (timestamp or relative) */
    float time_confidence;          /**< Confidence in timing [0-1] */

    //-------------------------------------------------------------------------
    // Spatial Context
    //-------------------------------------------------------------------------
    prime_signature_t location_context;  /**< Location signature for spatial context */
    float location_confidence;      /**< Confidence in location [0-1] */

    //-------------------------------------------------------------------------
    // Modality Information
    //-------------------------------------------------------------------------
    uint32_t modality_flags;        /**< Combination of source_modality_t flags */

    //-------------------------------------------------------------------------
    // Reality Monitoring Features (Johnson & Raye model)
    //-------------------------------------------------------------------------
    float perceptual_vividness;     /**< Sensory detail richness [0-1] (real > imagined) */
    float contextual_detail;        /**< Spatiotemporal context amount [0-1] */
    float cognitive_operations;     /**< Reasoning/effort involved [0-1] (imagined > real) */
    float semantic_detail;          /**< Meaning vs perception balance [0-1] */

} source_attribute_t;

/**
 * @brief Source memory entry binding attributes to a memory node
 *
 * WHAT: Complete source information for a single memory
 * WHY:  Links PR memory nodes with their source metadata
 * HOW:  Contains memory reference, source attributes, and reliability indicators
 *
 * Memory layout: ~192 bytes (excluding pointed data)
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Memory Reference
    //-------------------------------------------------------------------------
    pr_memory_node_t* memory;       /**< Associated memory node */
    uint64_t memory_id;             /**< Memory node ID (cached for lookup) */

    //-------------------------------------------------------------------------
    // Source Information
    //-------------------------------------------------------------------------
    source_attribute_t source;      /**< Detailed source attributes */
    reality_status_t reality_status; /**< Current reality assessment */

    //-------------------------------------------------------------------------
    // False Memory Risk Indicators
    //-------------------------------------------------------------------------
    float schema_consistency;       /**< How well fits expectations [0-1] (high = risk) */
    float suggestion_exposure;      /**< Post-event suggestive info exposure [0-1] */
    float repetition_count;         /**< Retrieval repetitions (imagination inflation) */
    float emotional_intensity;      /**< Emotional arousal at encoding [0-1] */
    float time_since_encoding;      /**< Elapsed time in seconds (older = riskier) */

    //-------------------------------------------------------------------------
    // Confidence Metrics
    //-------------------------------------------------------------------------
    float source_confidence;        /**< Confidence in source attribution [0-1] */
    float content_confidence;       /**< Confidence in content accuracy [0-1] */

    //-------------------------------------------------------------------------
    // Verification State
    //-------------------------------------------------------------------------
    bool verified;                  /**< Whether source has been verified */
    bool marked_suspicious;         /**< Flagged for potential inaccuracy */
    uint64_t last_verified_time;    /**< Timestamp of last verification */

} source_memory_entry_t;

/**
 * @brief Agent credibility record
 *
 * WHAT: Tracks reliability of information sources
 * WHY:  Some sources are more reliable than others
 * HOW:  Updated based on verification outcomes
 */
typedef struct {
    uint64_t agent_id;              /**< Unique agent identifier */
    char name[SOURCE_MAX_AGENT_NAME_LEN]; /**< Agent name */
    float credibility;              /**< Current credibility score [0-1] */
    uint32_t total_memories;        /**< Total memories from this source */
    uint32_t verified_correct;      /**< Verified as accurate */
    uint32_t verified_incorrect;    /**< Verified as inaccurate */
    float credibility_trend;        /**< Recent change in credibility [-1, +1] */
} source_agent_record_t;

/**
 * @brief Configuration for source memory system
 */
typedef struct {
    size_t initial_capacity;        /**< Initial entry array size */
    size_t max_entries;             /**< Maximum entries allowed */
    float perceptual_threshold;     /**< Above = likely real memory */
    float cognitive_threshold;      /**< Above = likely imagined memory */
    float false_memory_threshold;   /**< Risk above this triggers warning */
    float source_decay_rate;        /**< Source memory decay rate (per second) */
    bool track_agents;              /**< Whether to track agent credibility */
    bool auto_reality_check;        /**< Auto-update reality status on access */
} source_memory_config_t;

/**
 * @brief Query parameters for source memory searches
 */
typedef struct {
    source_type_t* source_types;    /**< Filter by source types (NULL = all) */
    size_t num_source_types;        /**< Number of types in filter */
    uint64_t source_agent_id;       /**< Filter by agent (SOURCE_INVALID_AGENT_ID = any) */
    float min_credibility;          /**< Minimum source credibility */
    float min_time;                 /**< Earliest acquisition time */
    float max_time;                 /**< Latest acquisition time */
    reality_status_t* reality_statuses; /**< Filter by reality status (NULL = all) */
    size_t num_reality_statuses;    /**< Number of statuses in filter */
    float max_false_memory_risk;    /**< Maximum false memory risk */
    bool only_verified;             /**< Only return verified entries */
    bool exclude_suspicious;        /**< Exclude marked suspicious entries */
} source_query_t;

/**
 * @brief Reality monitoring result
 */
typedef struct {
    reality_status_t status;        /**< Computed reality status */
    float reality_score;            /**< Reality score [0=imagined, 1=real] */
    float perceptual_score;         /**< Perceptual feature score */
    float contextual_score;         /**< Contextual detail score */
    float cognitive_score;          /**< Cognitive operations score */
    float semantic_score;           /**< Semantic detail score */
    float confidence;               /**< Confidence in assessment */
} reality_monitor_result_t;

/**
 * @brief False memory risk assessment result
 */
typedef struct {
    float total_risk;               /**< Combined risk score [0-1] */
    float schema_risk;              /**< Risk from schema consistency */
    float suggestion_risk;          /**< Risk from suggestive exposure */
    float repetition_risk;          /**< Risk from imagination inflation */
    float emotional_risk;           /**< Risk from emotional distortion */
    float temporal_risk;            /**< Risk from time decay */
    float source_amnesia_risk;      /**< Risk from forgotten source */
    bool is_high_risk;              /**< Whether above threshold */
    const char* primary_concern;    /**< Main risk factor description */
} false_memory_risk_t;

/**
 * @brief Statistics for source memory system
 */
typedef struct {
    size_t total_entries;           /**< Total entries in system */
    size_t verified_entries;        /**< Entries that have been verified */
    size_t suspicious_entries;      /**< Entries marked suspicious */
    size_t entries_by_type[SOURCE_TYPE_COUNT]; /**< Count per source type */
    size_t entries_by_reality[REALITY_STATUS_COUNT]; /**< Count per reality status */
    float avg_source_confidence;    /**< Average source confidence */
    float avg_content_confidence;   /**< Average content confidence */
    float avg_false_memory_risk;    /**< Average false memory risk */
    size_t num_agents;              /**< Number of tracked agents */
    float avg_agent_credibility;    /**< Average agent credibility */
    uint64_t memory_bytes;          /**< Approximate memory usage */
} source_memory_stats_t;

/**
 * @brief Source memory system handle (opaque)
 */
typedef struct source_memory_struct* source_memory_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default source memory configuration
 *
 * @return Default configuration with:
 *         - initial_capacity: 256
 *         - max_entries: 1M
 *         - perceptual_threshold: 0.6
 *         - cognitive_threshold: 0.4
 *         - false_memory_threshold: 0.7
 *         - source_decay_rate: 0.002
 *         - track_agents: true
 *         - auto_reality_check: true
 *
 * Performance: ~5ns
 */
NIMCP_EXPORT source_memory_config_t source_memory_config_default(void);

/**
 * @brief Validate source memory configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT bool source_memory_config_validate(const source_memory_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create a source memory system
 *
 * WHAT: Creates source memory system with PR integration
 * WHY:  Track memory origins and enable source monitoring
 * HOW:  Allocates entry storage, initializes agent tracking
 *
 * @param entanglement Entanglement graph for memory associations
 * @param node_manager PR node manager for memory access
 * @param config Configuration (NULL for defaults)
 * @return Source memory handle, or NULL on failure
 *
 * Performance: O(initial_capacity)
 * Memory: ~64 bytes + initial_capacity * sizeof(entry)
 *
 * Example:
 *   source_memory_config_t cfg = source_memory_config_default();
 *   source_memory_t sm = source_memory_create(entanglement, node_mgr, &cfg);
 */
NIMCP_EXPORT source_memory_t source_memory_create(
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    const source_memory_config_t* config
);

/**
 * @brief Destroy source memory system
 *
 * @param sm Source memory to destroy (NULL safe)
 *
 * Performance: O(N) where N = entries
 */
NIMCP_EXPORT void source_memory_destroy(source_memory_t sm);

/**
 * @brief Clear all entries from source memory
 *
 * @param sm Source memory to clear
 * @return source_error_t result code
 *
 * Performance: O(N)
 */
NIMCP_EXPORT source_error_t source_memory_clear(source_memory_t sm);

//=============================================================================
// Source Binding Functions
//=============================================================================

/**
 * @brief Bind source attributes to a memory node
 *
 * WHAT: Associates source information with a memory
 * WHY:  Enable source monitoring and reality checking
 * HOW:  Creates entry linking memory ID to source attributes
 *
 * @param sm Source memory system
 * @param memory Memory node to bind source to
 * @param source Source attributes to bind
 * @return source_error_t result code
 *
 * Performance: O(1) average
 *
 * Example:
 *   source_attribute_t src = {
 *       .type = SOURCE_TOLD,
 *       .source_agent_id = teacher_id,
 *       .source_credibility = 0.9f,
 *       .perceptual_vividness = 0.6f,
 *       .cognitive_operations = 0.3f
 *   };
 *   source_memory_bind_source(sm, memory, &src);
 */
NIMCP_EXPORT source_error_t source_memory_bind_source(
    source_memory_t sm,
    pr_memory_node_t* memory,
    const source_attribute_t* source
);

/**
 * @brief Bind source to memory by ID
 *
 * @param sm Source memory system
 * @param memory_id Memory node ID
 * @param source Source attributes to bind
 * @return source_error_t result code
 *
 * Performance: O(1) average
 */
NIMCP_EXPORT source_error_t source_memory_bind_source_by_id(
    source_memory_t sm,
    uint64_t memory_id,
    const source_attribute_t* source
);

/**
 * @brief Get source attributes for a memory
 *
 * WHAT: Retrieves source information for a memory
 * WHY:  Query memory origin and context
 * HOW:  Hash table lookup by memory ID
 *
 * @param sm Source memory system
 * @param memory Memory node to query
 * @param source Output source attributes
 * @return source_error_t (SOURCE_ERROR_NOT_FOUND if no source bound)
 *
 * Performance: O(1) average
 */
NIMCP_EXPORT source_error_t source_memory_get_source(
    source_memory_t sm,
    const pr_memory_node_t* memory,
    source_attribute_t* source
);

/**
 * @brief Get source by memory ID
 *
 * @param sm Source memory system
 * @param memory_id Memory node ID
 * @param source Output source attributes
 * @return source_error_t result code
 */
NIMCP_EXPORT source_error_t source_memory_get_source_by_id(
    source_memory_t sm,
    uint64_t memory_id,
    source_attribute_t* source
);

/**
 * @brief Get full entry for a memory
 *
 * @param sm Source memory system
 * @param memory_id Memory node ID
 * @param entry Output entry (pointer to internal structure, do not free)
 * @return source_error_t result code
 *
 * Performance: O(1) average
 *
 * NOTE: Returned pointer is valid until entry is removed or sm destroyed
 */
NIMCP_EXPORT source_error_t source_memory_get_entry(
    source_memory_t sm,
    uint64_t memory_id,
    source_memory_entry_t** entry
);

/**
 * @brief Update source attributes for a memory
 *
 * @param sm Source memory system
 * @param memory_id Memory node ID
 * @param source New source attributes
 * @return source_error_t result code
 *
 * Performance: O(1) average
 */
NIMCP_EXPORT source_error_t source_memory_update_source(
    source_memory_t sm,
    uint64_t memory_id,
    const source_attribute_t* source
);

/**
 * @brief Remove source binding for a memory
 *
 * @param sm Source memory system
 * @param memory_id Memory node ID
 * @return source_error_t result code
 *
 * Performance: O(1) average
 */
NIMCP_EXPORT source_error_t source_memory_unbind_source(
    source_memory_t sm,
    uint64_t memory_id
);

//=============================================================================
// Reality Monitoring Functions
//=============================================================================

/**
 * @brief Perform reality monitoring on a memory
 *
 * WHAT: Assess whether memory represents real or imagined event
 * WHY:  Distinguish actual experiences from imagination/dreams
 * HOW:  Weighted combination of perceptual, contextual, cognitive features
 *
 * Algorithm (Johnson & Raye model):
 *   1. Perceptual score: Higher for real memories (vivid sensory detail)
 *   2. Contextual score: Higher for real memories (spatiotemporal context)
 *   3. Cognitive score: Higher for imagined (required more thinking)
 *   4. Semantic score: Balance between meaning and perception
 *   5. Reality = w1*perceptual + w2*contextual - w3*cognitive + w4*semantic
 *
 * @param sm Source memory system
 * @param memory_id Memory node ID
 * @param result Output reality monitoring result
 * @return source_error_t result code
 *
 * Performance: ~50ns
 *
 * Example:
 *   reality_monitor_result_t result;
 *   source_memory_reality_monitor(sm, memory_id, &result);
 *   if (result.status == REALITY_CERTAIN_REAL) {
 *       // High confidence this actually happened
 *   }
 */
NIMCP_EXPORT source_error_t source_memory_reality_monitor(
    source_memory_t sm,
    uint64_t memory_id,
    reality_monitor_result_t* result
);

/**
 * @brief Update reality monitoring features for a memory
 *
 * WHAT: Set perceptual, contextual, cognitive, semantic scores
 * WHY:  Allow external systems to provide feature assessments
 * HOW:  Updates entry and recomputes reality status
 *
 * @param sm Source memory system
 * @param memory_id Memory node ID
 * @param perceptual Perceptual vividness [0-1]
 * @param contextual Contextual detail [0-1]
 * @param cognitive Cognitive operations [0-1]
 * @param semantic Semantic detail [0-1]
 * @return source_error_t result code
 */
NIMCP_EXPORT source_error_t source_memory_update_reality_features(
    source_memory_t sm,
    uint64_t memory_id,
    float perceptual,
    float contextual,
    float cognitive,
    float semantic
);

/**
 * @brief Get current reality status for a memory
 *
 * @param sm Source memory system
 * @param memory_id Memory node ID
 * @return reality_status_t (REALITY_UNCERTAIN if not found)
 */
NIMCP_EXPORT reality_status_t source_memory_get_reality_status(
    source_memory_t sm,
    uint64_t memory_id
);

//=============================================================================
// False Memory Detection Functions
//=============================================================================

/**
 * @brief Compute false memory risk for a memory
 *
 * WHAT: Assess likelihood that memory is false/distorted
 * WHY:  Flag potentially unreliable memories
 * HOW:  Analyze multiple risk factors
 *
 * Risk Factors:
 *   1. Schema consistency: Fits expectations too well
 *   2. Suggestion exposure: Post-event suggestive information
 *   3. Repetition: Imagination inflation from repeated retrieval
 *   4. Emotional intensity: High emotion can distort details
 *   5. Temporal decay: Older memories have higher source forgetting
 *   6. Source amnesia: Source forgotten but content "remembered"
 *
 * @param sm Source memory system
 * @param memory_id Memory node ID
 * @param risk Output false memory risk assessment
 * @return source_error_t result code
 *
 * Performance: ~100ns
 *
 * Example:
 *   false_memory_risk_t risk;
 *   source_memory_compute_false_memory_risk(sm, memory_id, &risk);
 *   if (risk.is_high_risk) {
 *       printf("Warning: %s\n", risk.primary_concern);
 *   }
 */
NIMCP_EXPORT source_error_t source_memory_compute_false_memory_risk(
    source_memory_t sm,
    uint64_t memory_id,
    false_memory_risk_t* risk
);

/**
 * @brief Update false memory risk indicators
 *
 * @param sm Source memory system
 * @param memory_id Memory node ID
 * @param schema_consistency How well memory fits schemas [0-1]
 * @param suggestion_exposure Suggestive information exposure [0-1]
 * @param emotional_intensity Emotional intensity [0-1]
 * @return source_error_t result code
 */
NIMCP_EXPORT source_error_t source_memory_update_risk_indicators(
    source_memory_t sm,
    uint64_t memory_id,
    float schema_consistency,
    float suggestion_exposure,
    float emotional_intensity
);

/**
 * @brief Record a retrieval event (for imagination inflation tracking)
 *
 * WHAT: Increment retrieval counter for a memory
 * WHY:  Repeated retrieval without verification increases false memory risk
 * HOW:  Updates repetition_count in entry
 *
 * @param sm Source memory system
 * @param memory_id Memory node ID
 * @return New retrieval count, or 0 on error
 */
NIMCP_EXPORT uint32_t source_memory_record_retrieval(
    source_memory_t sm,
    uint64_t memory_id
);

/**
 * @brief Mark memory as suspicious
 *
 * @param sm Source memory system
 * @param memory_id Memory node ID
 * @param reason Optional reason string (can be NULL)
 * @return source_error_t result code
 */
NIMCP_EXPORT source_error_t source_memory_mark_suspicious(
    source_memory_t sm,
    uint64_t memory_id,
    const char* reason
);

/**
 * @brief Clear suspicious flag
 *
 * @param sm Source memory system
 * @param memory_id Memory node ID
 * @return source_error_t result code
 */
NIMCP_EXPORT source_error_t source_memory_clear_suspicious(
    source_memory_t sm,
    uint64_t memory_id
);

//=============================================================================
// Source Credibility Functions
//=============================================================================

/**
 * @brief Update credibility for a source agent
 *
 * WHAT: Adjust trustworthiness of an information source
 * WHY:  Some sources are more reliable than others
 * HOW:  Update credibility based on verification outcomes
 *
 * @param sm Source memory system
 * @param agent_id Agent identifier
 * @param delta Credibility change [-1, +1]
 * @return New credibility score, or -1.0f on error
 *
 * Performance: O(1) average
 */
NIMCP_EXPORT float source_memory_update_credibility(
    source_memory_t sm,
    uint64_t agent_id,
    float delta
);

/**
 * @brief Get credibility for a source agent
 *
 * @param sm Source memory system
 * @param agent_id Agent identifier
 * @return Credibility score [0-1], or -1.0f if not found
 */
NIMCP_EXPORT float source_memory_get_credibility(
    source_memory_t sm,
    uint64_t agent_id
);

/**
 * @brief Register a new source agent
 *
 * @param sm Source memory system
 * @param agent_id Agent identifier
 * @param name Agent name (can be NULL)
 * @param initial_credibility Starting credibility [0-1]
 * @return source_error_t result code
 */
NIMCP_EXPORT source_error_t source_memory_register_agent(
    source_memory_t sm,
    uint64_t agent_id,
    const char* name,
    float initial_credibility
);

/**
 * @brief Get agent record
 *
 * @param sm Source memory system
 * @param agent_id Agent identifier
 * @param record Output agent record
 * @return source_error_t result code
 */
NIMCP_EXPORT source_error_t source_memory_get_agent(
    source_memory_t sm,
    uint64_t agent_id,
    source_agent_record_t* record
);

//=============================================================================
// Source Verification Functions
//=============================================================================

/**
 * @brief Verify a source attribution
 *
 * WHAT: Mark source as verified correct or incorrect
 * WHY:  Track accuracy of source memory
 * HOW:  Updates verification state and agent credibility
 *
 * @param sm Source memory system
 * @param memory_id Memory node ID
 * @param correct Whether source was verified as correct
 * @return source_error_t result code
 *
 * Performance: O(1) average
 *
 * Side effects:
 * - Updates entry's verified flag
 * - Adjusts source agent's credibility
 */
NIMCP_EXPORT source_error_t source_memory_verify_source(
    source_memory_t sm,
    uint64_t memory_id,
    bool correct
);

/**
 * @brief Cross-check source against other memories
 *
 * WHAT: Verify source by comparing with corroborating memories
 * WHY:  Multiple sources increase reliability
 * HOW:  Find memories with similar content, check source consistency
 *
 * @param sm Source memory system
 * @param memory_id Memory to verify
 * @param corroboration_threshold Minimum resonance for corroboration
 * @param corroborating_ids Output array of corroborating memory IDs
 * @param max_ids Maximum IDs to return
 * @param count Output: number of corroborating memories found
 * @return source_error_t result code
 */
NIMCP_EXPORT source_error_t source_memory_cross_check(
    source_memory_t sm,
    uint64_t memory_id,
    float corroboration_threshold,
    uint64_t* corroborating_ids,
    size_t max_ids,
    size_t* count
);

/**
 * @brief Detect source misattribution
 *
 * WHAT: Identify when source may be wrongly attributed
 * WHY:  Source confusion is common source of memory errors
 * HOW:  Compare source features against typical patterns
 *
 * @param sm Source memory system
 * @param memory_id Memory node ID
 * @param misattribution_score Output: likelihood of misattribution [0-1]
 * @param likely_true_source Output: ID of more likely source (0 if unknown)
 * @return source_error_t result code
 */
NIMCP_EXPORT source_error_t source_memory_detect_misattribution(
    source_memory_t sm,
    uint64_t memory_id,
    float* misattribution_score,
    uint64_t* likely_true_source
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Query memories by source criteria
 *
 * WHAT: Find memories matching source query parameters
 * WHY:  "What did X tell me?" or "What did I read last week?"
 * HOW:  Filter entries by source attributes
 *
 * @param sm Source memory system
 * @param query Query parameters
 * @param results Output array of memory IDs
 * @param max_results Maximum results to return
 * @param count Output: number of results
 * @return source_error_t result code
 *
 * Performance: O(N) where N = total entries
 *
 * Example:
 *   source_query_t query = {
 *       .source_agent_id = teacher_id,
 *       .min_credibility = 0.8f
 *   };
 *   uint64_t results[100];
 *   size_t count;
 *   source_memory_query_by_source(sm, &query, results, 100, &count);
 */
NIMCP_EXPORT source_error_t source_memory_query_by_source(
    source_memory_t sm,
    const source_query_t* query,
    uint64_t* results,
    size_t max_results,
    size_t* count
);

/**
 * @brief Query memories by time period
 *
 * @param sm Source memory system
 * @param min_time Earliest acquisition time
 * @param max_time Latest acquisition time
 * @param results Output array of memory IDs
 * @param max_results Maximum results
 * @param count Output: number of results
 * @return source_error_t result code
 */
NIMCP_EXPORT source_error_t source_memory_query_by_time(
    source_memory_t sm,
    float min_time,
    float max_time,
    uint64_t* results,
    size_t max_results,
    size_t* count
);

/**
 * @brief Query memories by reality status
 *
 * @param sm Source memory system
 * @param status Reality status to filter by
 * @param results Output array of memory IDs
 * @param max_results Maximum results
 * @param count Output: number of results
 * @return source_error_t result code
 */
NIMCP_EXPORT source_error_t source_memory_query_by_reality(
    source_memory_t sm,
    reality_status_t status,
    uint64_t* results,
    size_t max_results,
    size_t* count
);

/**
 * @brief Get memories with high false memory risk
 *
 * @param sm Source memory system
 * @param risk_threshold Minimum risk to include
 * @param results Output array of memory IDs
 * @param max_results Maximum results
 * @param count Output: number of results
 * @return source_error_t result code
 */
NIMCP_EXPORT source_error_t source_memory_query_high_risk(
    source_memory_t sm,
    float risk_threshold,
    uint64_t* results,
    size_t max_results,
    size_t* count
);

//=============================================================================
// Source Decay Functions
//=============================================================================

/**
 * @brief Apply source forgetting (decay) to all entries
 *
 * WHAT: Model faster decay of source vs content memory
 * WHY:  Source memory decays faster than content memory
 * HOW:  Reduce source confidence, may transition to SOURCE_UNKNOWN
 *
 * @param sm Source memory system
 * @param elapsed_seconds Time since last decay application
 * @return Number of entries affected
 *
 * Performance: O(N)
 *
 * Algorithm:
 *   source_confidence *= exp(-source_decay_rate * elapsed)
 *   If source_confidence < threshold, mark as SOURCE_UNKNOWN
 */
NIMCP_EXPORT size_t source_memory_source_forgetting(
    source_memory_t sm,
    float elapsed_seconds
);

/**
 * @brief Consolidate source binding (strengthen against decay)
 *
 * WHAT: Strengthen source memory through rehearsal
 * WHY:  Active retrieval of source strengthens binding
 * HOW:  Increase source confidence, reduce decay susceptibility
 *
 * @param sm Source memory system
 * @param memory_id Memory node ID
 * @param reinforcement Strength increase [0-1]
 * @return New source confidence, or -1.0f on error
 */
NIMCP_EXPORT float source_memory_consolidate_source(
    source_memory_t sm,
    uint64_t memory_id,
    float reinforcement
);

//=============================================================================
// Context Binding Functions
//=============================================================================

/**
 * @brief Bind contextual details to source
 *
 * WHAT: Associate additional contextual information with source
 * WHY:  Rich context improves source memory accuracy
 * HOW:  Store context signature with source attributes
 *
 * @param sm Source memory system
 * @param memory_id Memory node ID
 * @param context_signature Prime signature of context
 * @param context_confidence Confidence in context [0-1]
 * @return source_error_t result code
 */
NIMCP_EXPORT source_error_t source_memory_bind_context(
    source_memory_t sm,
    uint64_t memory_id,
    const prime_signature_t* context_signature,
    float context_confidence
);

/**
 * @brief Query memories by context similarity
 *
 * @param sm Source memory system
 * @param context_signature Context to match
 * @param similarity_threshold Minimum context similarity
 * @param results Output array of memory IDs
 * @param max_results Maximum results
 * @param count Output: number of results
 * @return source_error_t result code
 */
NIMCP_EXPORT source_error_t source_memory_query_by_context(
    source_memory_t sm,
    const prime_signature_t* context_signature,
    float similarity_threshold,
    uint64_t* results,
    size_t max_results,
    size_t* count
);

//=============================================================================
// Statistics and Utility Functions
//=============================================================================

/**
 * @brief Get source memory statistics
 *
 * @param sm Source memory system
 * @param stats Output statistics structure
 * @return source_error_t result code
 */
NIMCP_EXPORT source_error_t source_memory_get_stats(
    source_memory_t sm,
    source_memory_stats_t* stats
);

/**
 * @brief Get error string for error code
 *
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* source_error_string(source_error_t error);

/**
 * @brief Get source type name as string
 *
 * @param type Source type
 * @return Human-readable type name
 */
NIMCP_EXPORT const char* source_type_name(source_type_t type);

/**
 * @brief Get reality status name as string
 *
 * @param status Reality status
 * @return Human-readable status name
 */
NIMCP_EXPORT const char* reality_status_name(reality_status_t status);

/**
 * @brief Initialize source attribute structure with defaults
 *
 * @param attr Attribute structure to initialize
 */
NIMCP_EXPORT void source_attribute_init(source_attribute_t* attr);

/**
 * @brief Initialize source query structure with defaults
 *
 * @param query Query structure to initialize
 */
NIMCP_EXPORT void source_query_init(source_query_t* query);

/**
 * @brief Print source entry for debugging
 *
 * @param entry Entry to print
 */
NIMCP_EXPORT void source_entry_print(const source_memory_entry_t* entry);

//=============================================================================
// Serialization Functions
//=============================================================================

/**
 * @brief Serialize source memory to buffer
 *
 * @param sm Source memory system
 * @param buffer Output buffer (NULL to query size)
 * @param buffer_size Buffer size
 * @param written_size Output: bytes written or required
 * @return source_error_t result code
 */
NIMCP_EXPORT source_error_t source_memory_serialize(
    source_memory_t sm,
    void* buffer,
    size_t buffer_size,
    size_t* written_size
);

/**
 * @brief Deserialize source memory from buffer
 *
 * @param buffer Input buffer
 * @param buffer_size Buffer size
 * @param entanglement Entanglement graph for integration
 * @param node_manager Node manager for integration
 * @param bytes_read Output: bytes consumed
 * @return Deserialized source memory, or NULL on error
 */
NIMCP_EXPORT source_memory_t source_memory_deserialize(
    const void* buffer,
    size_t buffer_size,
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    size_t* bytes_read
);

//=============================================================================
// Inline Helper Functions
//=============================================================================

/**
 * @brief Create source attribute for perceived (directly experienced) memory
 *
 * @param perceptual_vividness Sensory detail vividness [0-1]
 * @param contextual_detail Context detail amount [0-1]
 * @return Initialized source attribute
 */
static inline source_attribute_t source_attr_perceived(
    float perceptual_vividness,
    float contextual_detail
) {
    source_attribute_t attr;
    source_attribute_init(&attr);
    attr.type = SOURCE_PERCEIVED;
    attr.source_agent_id = 0;  // Self
    attr.source_credibility = 1.0f;  // Direct experience
    attr.perceptual_vividness = perceptual_vividness;
    attr.contextual_detail = contextual_detail;
    attr.cognitive_operations = 0.1f;  // Low cognitive effort for perception
    attr.modality_flags = MODALITY_VISUAL | MODALITY_AUDITORY;
    return attr;
}

/**
 * @brief Create source attribute for told (from another person) memory
 *
 * @param agent_id Source agent ID
 * @param credibility Agent credibility [0-1]
 * @return Initialized source attribute
 */
static inline source_attribute_t source_attr_told(
    uint64_t agent_id,
    float credibility
) {
    source_attribute_t attr;
    source_attribute_init(&attr);
    attr.type = SOURCE_TOLD;
    attr.source_agent_id = agent_id;
    attr.source_credibility = credibility;
    attr.perceptual_vividness = 0.4f;  // Moderate (secondhand)
    attr.contextual_detail = 0.3f;
    attr.cognitive_operations = 0.3f;
    attr.modality_flags = MODALITY_AUDITORY | MODALITY_SPOKEN;
    return attr;
}

/**
 * @brief Create source attribute for read memory
 *
 * @return Initialized source attribute
 */
static inline source_attribute_t source_attr_read(void) {
    source_attribute_t attr;
    source_attribute_init(&attr);
    attr.type = SOURCE_READ;
    attr.source_agent_id = 0;
    attr.source_credibility = 0.8f;  // Written sources generally reliable
    attr.perceptual_vividness = 0.3f;  // Low visual, text-based
    attr.contextual_detail = 0.2f;
    attr.cognitive_operations = 0.5f;  // More processing for reading
    attr.modality_flags = MODALITY_READ | MODALITY_VISUAL;
    return attr;
}

/**
 * @brief Create source attribute for inferred memory
 *
 * @return Initialized source attribute
 */
static inline source_attribute_t source_attr_inferred(void) {
    source_attribute_t attr;
    source_attribute_init(&attr);
    attr.type = SOURCE_INFERRED;
    attr.source_agent_id = 0;
    attr.source_credibility = 0.6f;  // Inferences can be wrong
    attr.perceptual_vividness = 0.2f;  // Low perceptual
    attr.contextual_detail = 0.1f;
    attr.cognitive_operations = 0.8f;  // High cognitive effort
    attr.semantic_detail = 0.8f;  // High semantic content
    attr.modality_flags = MODALITY_INTERNAL;
    return attr;
}

/**
 * @brief Create source attribute for imagined memory
 *
 * @return Initialized source attribute
 */
static inline source_attribute_t source_attr_imagined(void) {
    source_attribute_t attr;
    source_attribute_init(&attr);
    attr.type = SOURCE_IMAGINED;
    attr.source_agent_id = 0;
    attr.source_credibility = 0.0f;  // Not a real source
    attr.perceptual_vividness = 0.5f;  // Can be vivid
    attr.contextual_detail = 0.2f;  // Usually less context
    attr.cognitive_operations = 0.9f;  // High effort to generate
    attr.semantic_detail = 0.7f;
    attr.modality_flags = MODALITY_INTERNAL;
    return attr;
}

/**
 * @brief Check if source type indicates external origin
 *
 * @param type Source type
 * @return true if external (told, read), false if internal (perceived, inferred, imagined)
 */
static inline bool source_is_external(source_type_t type) {
    return type == SOURCE_TOLD || type == SOURCE_READ;
}

/**
 * @brief Check if reality status indicates real memory
 *
 * @param status Reality status
 * @return true if probably or certainly real
 */
static inline bool reality_is_real(reality_status_t status) {
    return status == REALITY_CERTAIN_REAL || status == REALITY_PROBABLY_REAL;
}

/**
 * @brief Check if reality status indicates imagined memory
 *
 * @param status Reality status
 * @return true if probably or certainly imagined
 */
static inline bool reality_is_imagined(reality_status_t status) {
    return status == REALITY_CERTAIN_IMAGINED || status == REALITY_PROBABLY_IMAGINED;
}

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SOURCE_MEMORY_H
