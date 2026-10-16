# Phase M4: Semantic Memory Network Specification

**Version:** 1.0
**Date:** 2025-11-13
**Status:** Implementation Ready
**Depends On:** Phase M2 (Systems Consolidation), Phase M1 (Engrams)

## Executive Summary

Phase M4 implements a semantic memory network that organizes consolidated knowledge into concepts and relations. This bridges the gap between individual memories and structured knowledge, enabling abstract reasoning, inference, and knowledge retrieval through spreading activation.

## Biological Foundation

### Neuroscience Background

**Semantic Memory (Tulving, 1972):**
- General knowledge storage (facts, concepts, meanings)
- Context-independent (vs episodic: context-dependent)
- Extracted from repeated episodic experiences
- Organized in conceptual networks
- Supports abstract reasoning and inference

**Semantic Network Theory (Collins & Quillian, 1969):**
- Knowledge organized as network of concepts
- Concepts connected by relations (is-a, has-a, part-of)
- Hierarchical organization
- Inheritance of properties
- Distance affects retrieval time

**Spreading Activation (Collins & Loftus, 1975):**
- Activation spreads through network
- Nearby concepts become primed
- Decay over distance
- Explains priming effects
- Parallel constraint satisfaction

**Conceptual Abstraction (Rosch, 1975):**
- Prototypes represent categories
- Features define concepts
- Typicality gradients
- Basic-level categories most accessible
- Abstraction from exemplars

### Key Mechanisms

1. **Concept Formation**
   - Extract features from semantic memories (Phase M2)
   - Cluster similar patterns
   - Create prototype representations
   - Tag with labels/categories

2. **Relation Learning**
   - Co-occurrence detection
   - Temporal association
   - Similarity-based linking
   - Hierarchical organization

3. **Spreading Activation**
   - Activation from cue concepts
   - Propagation through relations
   - Decay over distance
   - Summation at nodes
   - Threshold-based retrieval

4. **Knowledge Retrieval**
   - Query with concept(s)
   - Activate related concepts
   - Return activated network region
   - Support inference and reasoning

## Architecture Design

### System Components

```
┌─────────────────────────────────────────────────────────────┐
│                   SEMANTIC MEMORY NETWORK                    │
│                                                               │
│  ┌─────────┐      ┌─────────┐      ┌─────────┐             │
│  │Concept A│─────→│Concept B│─────→│Concept C│             │
│  │features │ is-a │features │ has-a│features │             │
│  │activation│      │activation│      │activation│             │
│  └─────────┘      └─────────┘      └─────────┘             │
│       ↓                 ↓                 ↓                   │
│  [Spreading]      [Relations]       [Inference]             │
│                                                               │
│  ┌──────────────────────────────────────────────────┐       │
│  │ Concept Pool: 2048 concepts                      │       │
│  │ Relation Pool: 8192 relations                    │       │
│  │ Activation Map: Current activation levels        │       │
│  └──────────────────────────────────────────────────┘       │
└─────────────────────────────────────────────────────────────┘
                            ↑
                  ┌─────────────────┐
                  │ KNOWLEDGE        │
                  │ EXTRACTION       │
                  │ (from Phase M2)  │
                  └─────────────────┘
```

### Data Structures

```c
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
```

## Core API

### System Management

```c
/**
 * @brief Create semantic memory network
 * @return New system, or NULL on failure
 */
semantic_memory_system_t* semantic_memory_create(void);

/**
 * @brief Destroy semantic memory network
 * @param system System to destroy
 */
void semantic_memory_destroy(semantic_memory_system_t* system);

/**
 * @brief Reset semantic memory (clear concepts and relations)
 * @param system System to reset
 */
void semantic_memory_reset(semantic_memory_system_t* system);
```

### Integration API

```c
/**
 * @brief Connect to systems consolidation system
 * @param system Semantic memory system
 * @param consolidation Systems consolidation (Phase M2)
 */
void semantic_memory_set_consolidation(
    semantic_memory_system_t* system,
    void* consolidation);
```

### Concept Operations

```c
/**
 * @brief Create new concept from features
 * @param system Semantic memory system
 * @param features Feature vector
 * @param feature_dim Feature dimension
 * @param label Human-readable label (optional, can be NULL)
 * @param category Concept category
 * @return Concept ID, or 0 on failure
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
 */
semantic_query_result_t* semantic_memory_find_similar(
    semantic_memory_system_t* system,
    const float* features,
    uint32_t feature_dim,
    uint32_t max_results,
    float threshold);
```

### Relation Operations

```c
/**
 * @brief Create relation between concepts
 * @param system Semantic memory system
 * @param source_id Source concept ID
 * @param target_id Target concept ID
 * @param type Relation type
 * @param strength Initial strength (0.0-1.0)
 * @return Relation ID, or 0 on failure
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
 */
uint32_t semantic_memory_get_relations(
    const semantic_memory_system_t* system,
    uint64_t concept_id,
    uint64_t* relation_ids,
    uint32_t max_relations);
```

### Spreading Activation

```c
/**
 * @brief Activate concept and spread activation
 * @param system Semantic memory system
 * @param concept_id Concept to activate
 * @param initial_activation Initial activation level (0.0-1.0)
 * @return Query result with activated concepts
 */
semantic_query_result_t* semantic_memory_activate(
    semantic_memory_system_t* system,
    uint64_t concept_id,
    float initial_activation);

/**
 * @brief Query semantic memory with features (activate + spread)
 * @param system Semantic memory system
 * @param features Query features
 * @param feature_dim Feature dimension
 * @return Query result with activated concepts
 */
semantic_query_result_t* semantic_memory_query(
    semantic_memory_system_t* system,
    const float* features,
    uint32_t feature_dim);

/**
 * @brief Free query result
 * @param result Query result to free
 */
void semantic_memory_free_result(semantic_query_result_t* result);
```

### Knowledge Extraction

```c
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
```

### Configuration API

```c
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
```

### Statistics API

```c
/**
 * @brief Get semantic memory statistics
 * @param system Semantic memory system
 * @param stats_out Output for statistics
 */
void semantic_memory_get_statistics(
    const semantic_memory_system_t* system,
    semantic_memory_stats_t* stats_out);
```

## Integration Points

### Brain Cognitive Pipeline

**Location:** `brain_decide()` after Phase M2 consolidation

**Integration:**
1. After consolidation update, extract new semantic memories
2. Create concepts from consolidated cortical nodes
3. Infer relations from co-occurrence
4. Update semantic network

**Pseudocode:**
```c
brain_decision_t* brain_decide(...) {
    // Existing pipeline...

    // Phase M4: Semantic Memory Network Update
    if (brain->semantic_memory && brain->systems_consolidation) {
        // Extract concepts from newly consolidated memories
        uint32_t concepts_extracted = semantic_memory_extract_from_consolidation(
            brain->semantic_memory
        );

        // Optional: Use semantic memory for inference enhancement
        if (concepts_extracted > 0) {
            // Query semantic network with input features
            semantic_query_result_t* result = semantic_memory_query(
                brain->semantic_memory,
                features,
                num_features
            );

            // Result contains activated concepts for reasoning
            if (result) {
                // Could use for decision enhancement
                semantic_memory_free_result(result);
            }
        }
    }
}
```

## Default Configuration

```c
spreading_activation_params_t DEFAULT_SPREAD_PARAMS = {
    .decay_rate = 0.8f,        // 20% decay per hop
    .threshold = 0.3f,         // 30% activation threshold
    .max_hops = 3,             // Spread up to 3 hops
    .min_activation = 0.1f     // Stop below 10% activation
};

const uint32_t DEFAULT_CONCEPT_CAPACITY = 2048;
const uint32_t DEFAULT_RELATION_CAPACITY = 8192;
```

## Performance Characteristics

### Time Complexity
- **Create Concept:** O(1)
- **Find Similar:** O(n) where n = concept count
- **Spreading Activation:** O(h × r) where h = hops, r = relations per concept
- **Extract from M2:** O(m) where m = semantic memories in M2

### Space Complexity
- **Concepts:** O(2048) × O(32 features) = ~256KB
- **Relations:** O(8192) × O(24 bytes) = ~192KB
- **Activation Map:** O(2048 floats) = ~8KB
- **Total:** ~456KB per brain

### Expected Performance
- **Concept creation:** <50μs
- **Similarity search:** <500μs (2048 concepts)
- **Spreading activation:** <200μs (3 hops)
- **Extraction from M2:** <1ms per cycle

## Test Plan

### Unit Tests (Target: 20+ tests)

1. **System Management** (3 tests)
   - Create system
   - Destroy system
   - Reset system

2. **Concept Operations** (5 tests)
   - Create concept
   - Get concept
   - Find similar concepts
   - Concept with label
   - Concept categories

3. **Relation Operations** (4 tests)
   - Create relation
   - Get relations for concept
   - Relation types
   - Relation strength

4. **Spreading Activation** (4 tests)
   - Activate single concept
   - Spreading with decay
   - Multi-hop spreading
   - Threshold cutoff

5. **Query Operations** (2 tests)
   - Query with features
   - Free query result

6. **Statistics** (2 tests)
   - Get statistics
   - Statistics update correctly

### Integration Tests (Target: 10+ tests)

1. **Brain Integration** (3 tests)
   - Brain creation with semantic memory
   - Learning creates concepts (via M2)
   - Inference queries semantic memory

2. **Phase M2 Integration** (3 tests)
   - Extract concepts from M2
   - Relations inferred from M2
   - Semantic abstraction

3. **Knowledge Retrieval** (2 tests)
   - Query activates related concepts
   - Spreading activation works

4. **Performance** (2 tests)
   - No excessive overhead
   - Scales with concept count

### Regression Tests (Target: 15+ tests)

1. **Backward Compatibility** (5 tests)
   - Brain creation still works
   - Learning unaffected
   - Inference unaffected
   - Phase M1/M2 still work
   - Performance acceptable

2. **Stability** (5 tests)
   - Extended use stable
   - No memory leaks
   - Multi-pattern learning
   - Concept/relation limits
   - Edge cases handled

3. **API Compatibility** (5 tests)
   - All brain APIs work
   - Phase M2 unaffected
   - Phase M3 unaffected
   - Performance acceptable
   - Zero breaking changes

## Success Criteria

- ✅ All unit tests passing (20+)
- ✅ All integration tests passing (10+)
- ✅ All regression tests passing (15+)
- ✅ Total: 45+ tests, 100% passing
- ✅ Performance: <1ms overhead per cycle
- ✅ Memory overhead: <500KB per brain
- ✅ Zero breaking changes
- ✅ NIMCP standards: <50 lines per function
- ✅ Comprehensive documentation

## References

1. **Tulving, E. (1972).** "Episodic and semantic memory." *Organization of Memory*, 381-403.

2. **Collins, A.M. & Quillian, M.R. (1969).** "Retrieval time from semantic memory." *Journal of Verbal Learning and Verbal Behavior, 8*(2), 240-247.

3. **Collins, A.M. & Loftus, E.F. (1975).** "A spreading-activation theory of semantic processing." *Psychological Review, 82*(6), 407-428.

4. **Rosch, E. (1975).** "Cognitive representations of semantic categories." *Journal of Experimental Psychology: General, 104*(3), 192-233.

5. **McClelland, J.L. & Rogers, T.T. (2003).** "The parallel distributed processing approach to semantic cognition." *Nature Reviews Neuroscience, 4*(4), 310-322.

## Implementation Timeline

**Day 1:** Core system (header + source + unit tests)
**Day 2:** Brain integration + integration tests
**Day 3:** Regression tests + documentation

**Estimated Total:** 1-2 days for full implementation with 100% test coverage
