/**
 * @file nimcp_semantic_memory.h
 * @brief Phase M4: Semantic Memory Network
 *
 * WHAT: Network of concepts and relations for general knowledge storage
 * WHY:  Enable abstract reasoning, inference, and knowledge retrieval
 * HOW:  Spreading activation through concept network with relations
 *
 * BIOLOGICAL BASIS:
 * - Semantic memory: Context-independent knowledge (Tulving, 1972)
 * - Semantic networks: Concepts connected by relations (Collins & Quillian, 1969)
 * - Spreading activation: Activation propagates through network (Collins & Loftus, 1975)
 * - Conceptual abstraction: Prototypes from exemplars (Rosch, 1975)
 *
 * INTEGRATION:
 * - Phase M2 Systems Consolidation: Source of semantic memories
 * - Extracts concepts from consolidated cortical nodes
 * - Infers relations from co-occurrence and similarity
 * - Supports inference and reasoning through activation

 *
 * @version Phase M4 Semantic Memory
 * @date 2025-11-13
 */

#ifndef NIMCP_SEMANTIC_MEMORY_H
#define NIMCP_SEMANTIC_MEMORY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @enum concept_category_t
 * @brief Category types for semantic concepts
 */
typedef enum {
    CONCEPT_OBJECT,       // Physical objects
    CONCEPT_ACTION,       // Actions/verbs
    CONCEPT_PROPERTY,     // Properties/adjectives
    CONCEPT_EVENT,        // Events
    CONCEPT_ABSTRACT,     // Abstract concepts
    CONCEPT_CATEGORY      // Category labels
} concept_category_t;

/**
 * @enum relation_type_t
 * @brief Types of relations between concepts
 */
typedef enum {
    RELATION_IS_A,        // Hierarchical (dog is-a animal)
    RELATION_HAS_A,       // Part-whole (car has-a wheel)
    RELATION_PROPERTY_OF, // Property attribution (red property-of apple)
    RELATION_CAUSES,      // Causal (rain causes wet)
    RELATION_SIMILAR_TO,  // Similarity
    RELATION_ASSOCIATED   // General association
} relation_type_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @struct semantic_concept_t
 * @brief Represents a concept in semantic memory
 */
typedef struct {
    uint64_t id;                      // Unique concept ID
    char* label;                      // Human-readable label
    concept_category_t category;      // Concept category

    float* features;                  // Feature vector (32-dim)
    uint32_t feature_dim;

    float activation;                 // Current activation level (0.0-1.0)
    float base_activation;            // Baseline activation (frequency)

    uint64_t* source_memory_ids;      // IDs of source memories (Phase M2)
    uint32_t source_count;

    uint64_t creation_time_ms;        // When concept was formed
    uint32_t access_count;            // How many times accessed

    // Phase 1.5: Copy-on-Write support for efficient feature sharing
    uint32_t* _cow_refcount;          /**< Shared reference count (NULL = owned) */
    bool _cow_is_shallow;             /**< True if features pointer is shared */

} semantic_concept_t;

/**
 * @struct semantic_relation_t
 * @brief Represents a relation between concepts
 */
typedef struct {
    uint64_t id;                      // Unique relation ID
    uint64_t source_concept_id;       // Source concept
    uint64_t target_concept_id;       // Target concept
    relation_type_t type;             // Relation type

    float strength;                   // Relation strength (0.0-1.0)
    uint32_t co_occurrence_count;     // How often seen together

    uint64_t creation_time_ms;        // When relation formed

} semantic_relation_t;

/**
 * @struct spreading_activation_params_t
 * @brief Parameters for spreading activation algorithm
 */
typedef struct {
    float decay_rate;                 // Activation decay per hop (0.8)
    float threshold;                  // Activation threshold (0.3)
    uint32_t max_hops;                // Maximum spread distance (3)
    float min_activation;             // Minimum to continue (0.1)
} spreading_activation_params_t;

/**
 * @struct semantic_memory_stats_t
 * @brief Statistics for semantic memory network
 */
typedef struct {
    uint32_t concept_count;           // Current number of concepts
    uint32_t relation_count;          // Current number of relations
    uint64_t total_retrievals;        // Total retrieval operations
    uint64_t total_concepts_formed;   // Total concepts ever created
    uint64_t total_relations_formed;  // Total relations ever created
    float average_activation;         // Average activation level
} semantic_memory_stats_t;

/**
 * @struct semantic_memory_system_t
 * @brief Semantic memory network system
 */
typedef struct {
    // Concept storage
    semantic_concept_t** concepts;    // Array of concept pointers
    uint32_t concept_count;
    uint32_t concept_capacity;        // Max concepts (2048)

    // Relation storage
    semantic_relation_t** relations;  // Array of relation pointers
    uint32_t relation_count;
    uint32_t relation_capacity;       // Max relations (8192)

    // Activation state
    float* activation_map;            // Current activation per concept
    uint32_t activation_map_size;

    // Spreading activation parameters
    spreading_activation_params_t spread_params;

    // Statistics
    semantic_memory_stats_t stats;

    // System references (not owned)
    void* systems_consolidation;      // Phase M2 (source of semantic memories)

    // Internal state
    uint64_t last_update_time_ms;
    uint64_t next_concept_id;
    uint64_t next_relation_id;

    // Phase 1.5: Memory pools for hot-path allocations
    void* concept_pool;               /**< Pool for concept structs */
    void* relation_pool;              /**< Pool for relation structs */
    void* feature_pool;               /**< Pool for feature vectors */

    // Unified memory integration (CoW support for brain cloning)
    void* mem_manager;                /**< unified_mem_manager_t */
    void* concepts_handle;            /**< CoW handle for concepts array */
    void* activation_handle;          /**< CoW handle for activation map */

    // Bio-async integration
    void* bio_ctx;                    /**< bio_module_context_t pointer */
    bool bio_async_enabled;           /**< Bio-async registration status */

    // Quantum acceleration
    void* quantum_bridge;             /**< Quantum semantic retrieval bridge */
    uint64_t quantum_retrievals;      /**< Number of quantum-accelerated retrievals */
    bool enable_quantum;              /**< Whether quantum retrieval is enabled */

    // P2-COG-07: Thread safety mutex for concept/relation operations
    void* mutex;                      /**< nimcp_mutex_t* for thread safety */

} semantic_memory_system_t;

/**
 * @struct semantic_query_result_t
 * @brief Result of semantic memory query
 */
typedef struct {
    uint64_t* concept_ids;            // Activated concept IDs
    float* activation_levels;         // Activation level per concept
    uint32_t count;                   // Number of activated concepts
} semantic_query_result_t;

//=============================================================================
// System Management API
//=============================================================================

/**
 * @brief Create semantic memory network
 * @return New system, or NULL on failure
 *
 * WHAT: Initialize semantic memory with concept and relation pools
 * WHY:  Prepare system for knowledge storage and retrieval
 * HOW:  Allocate pools, set defaults, initialize stats
 */
semantic_memory_system_t* semantic_memory_create(void);

/**
 * @brief Destroy semantic memory network
 * @param system System to destroy (can be NULL)
 *
 * WHAT: Clean up semantic memory and free resources
 * WHY:  Prevent memory leaks when brain is destroyed
 * HOW:  Free concepts, relations, activation map, system struct
 */
void semantic_memory_destroy(semantic_memory_system_t* system);

/**
 * @brief Reset semantic memory (clear concepts and relations)
 * @param system System to reset
 *
 * WHAT: Clear all concepts and relations
 * WHY:  Allow fresh start while keeping configuration
 * HOW:  Free concepts/relations, reset stats, keep params
 */
void semantic_memory_reset(semantic_memory_system_t* system);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Connect to systems consolidation system
 * @param system Semantic memory system
 * @param consolidation Systems consolidation (Phase M2)
 *
 * WHAT: Link to Phase M2 for concept extraction
 * WHY:  Semantic concepts extracted from consolidated memories
 * HOW:  Store pointer (not owned) to M2 system
 */
void semantic_memory_set_consolidation(
    semantic_memory_system_t* system,
    void* consolidation);

//=============================================================================
// Concept Operations API
//=============================================================================

/**
 * @brief Create new concept from features
 * @param system Semantic memory system
 * @param features Feature vector
 * @param feature_dim Feature dimension
 * @param label Human-readable label (optional, can be NULL)
 * @param category Concept category
 * @return Concept ID, or 0 on failure
 *
 * WHAT: Create concept with features and metadata
 * WHY:  Store semantic knowledge as concepts
 * HOW:  Allocate concept, copy features, assign ID
 */
uint64_t semantic_memory_create_concept(
    semantic_memory_system_t* system,
    const float* features,
    uint32_t feature_dim,
    const char* label,
    concept_category_t category);

/**
 * @brief Get concept by ID
 * @param system Semantic memory system
 * @param concept_id Concept ID
 * @return Concept, or NULL if not found
 *
 * WHAT: Retrieve concept by ID
 * WHY:  Access concept data for queries
 * HOW:  Linear search through concept array
 */
const semantic_concept_t* semantic_memory_get_concept(
    const semantic_memory_system_t* system,
    uint64_t concept_id);

/**
 * @brief Find concepts similar to features
 * @param system Semantic memory system
 * @param features Query features
 * @param feature_dim Feature dimension
 * @param max_results Maximum results to return
 * @param threshold Minimum similarity (0.0-1.0)
 * @return Query result with similar concepts
 *
 * WHAT: Find concepts similar to query features
 * WHY:  Retrieve related concepts for reasoning
 * HOW:  Compute cosine similarity, return top matches
 */
semantic_query_result_t* semantic_memory_find_similar(
    semantic_memory_system_t* system,
    const float* features,
    uint32_t feature_dim,
    uint32_t max_results,
    float threshold);

//=============================================================================
// Relation Operations API
//=============================================================================

/**
 * @brief Create relation between concepts
 * @param system Semantic memory system
 * @param source_id Source concept ID
 * @param target_id Target concept ID
 * @param type Relation type
 * @param strength Initial strength (0.0-1.0)
 * @return Relation ID, or 0 on failure
 *
 * WHAT: Create directed relation between concepts
 * WHY:  Encode relationships for inference
 * HOW:  Allocate relation, store IDs and metadata
 */
uint64_t semantic_memory_create_relation(
    semantic_memory_system_t* system,
    uint64_t source_id,
    uint64_t target_id,
    relation_type_t type,
    float strength);

/**
 * @brief Get relations for concept
 * @param system Semantic memory system
 * @param concept_id Concept ID
 * @param relation_ids Output array for relation IDs
 * @param max_relations Maximum relations to return
 * @return Number of relations found
 *
 * WHAT: Find all relations involving concept
 * WHY:  Navigate network for spreading activation
 * HOW:  Search relations where concept is source or target
 */
uint32_t semantic_memory_get_relations(
    const semantic_memory_system_t* system,
    uint64_t concept_id,
    uint64_t* relation_ids,
    uint32_t max_relations);

//=============================================================================
// Spreading Activation API
//=============================================================================

/**
 * @brief Activate concept and spread activation
 * @param system Semantic memory system
 * @param concept_id Concept to activate
 * @param initial_activation Initial activation level (0.0-1.0)
 * @return Query result with activated concepts
 *
 * WHAT: Activate concept and propagate through relations
 * WHY:  Retrieve related concepts via spreading
 * HOW:  BFS with decay, return activated concepts
 */
semantic_query_result_t* semantic_memory_activate(
    semantic_memory_system_t* system,
    uint64_t concept_id,
    float initial_activation);

/**
 * @brief Query semantic memory with features
 * @param system Semantic memory system
 * @param features Query features
 * @param feature_dim Feature dimension
 * @return Query result with activated concepts
 *
 * WHAT: Find similar concepts, then spread activation
 * WHY:  Combined similarity + spreading for retrieval
 * HOW:  Find similar, activate best match, spread
 */
semantic_query_result_t* semantic_memory_query(
    semantic_memory_system_t* system,
    const float* features,
    uint32_t feature_dim);

/**
 * @brief Free query result
 * @param result Query result to free
 *
 * WHAT: Free memory allocated for query result
 * WHY:  Prevent memory leaks
 * HOW:  Free arrays, then struct
 */
void semantic_memory_free_result(semantic_query_result_t* result);

//=============================================================================
// Knowledge Extraction API
//=============================================================================

/**
 * @brief Extract concepts from consolidated memories
 * @param system Semantic memory system
 * @return Number of concepts extracted
 *
 * WHAT: Extract semantic concepts from Phase M2 cortical nodes
 * WHY:  Build semantic network from consolidated memories
 * HOW:  Query M2 for semantic memories, create concepts, infer relations
 */
uint32_t semantic_memory_extract_from_consolidation(
    semantic_memory_system_t* system);

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Set spreading activation parameters
 * @param system Semantic memory system
 * @param params New parameters
 */
void semantic_memory_set_spread_params(
    semantic_memory_system_t* system,
    const spreading_activation_params_t* params);

/**
 * @brief Get spreading activation parameters
 * @param system Semantic memory system
 * @param params_out Output for parameters
 */
void semantic_memory_get_spread_params(
    const semantic_memory_system_t* system,
    spreading_activation_params_t* params_out);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get semantic memory statistics
 * @param system Semantic memory system
 * @param stats_out Output for statistics
 */
void semantic_memory_get_statistics(
    const semantic_memory_system_t* system,
    semantic_memory_stats_t* stats_out);

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Get default spreading activation parameters
 * @return Default parameters based on neuroscience literature
 */
spreading_activation_params_t semantic_memory_get_default_spread_params(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SEMANTIC_MEMORY_H
