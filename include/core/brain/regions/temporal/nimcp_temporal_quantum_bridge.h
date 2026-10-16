/**
 * @file nimcp_temporal_quantum_bridge.h
 * @brief Quantum-inspired temporal cortex optimization
 *
 * WHAT: Integrates quantum algorithms with temporal cortex processing
 * WHY: Accelerate object recognition and semantic memory search
 * HOW: Quantum reasoning for object matching, superposition for concept retrieval
 *
 * BIOLOGICAL INSPIRATION:
 * - Temporal cortex explores multiple object interpretations in parallel
 * - Semantic memory maintains superposition of activated concepts
 * - Object recognition resembles quantum collapse to best match
 * - Concept retrieval benefits from parallel path evaluation
 *
 * QUANTUM CONCEPTS:
 * - Superposition: Explore multiple object interpretations simultaneously
 * - Grover search: Find matching object in O(sqrt(N)) from prototype library
 * - Interference: Cancel low-probability object hypotheses
 * - Amplitude amplification: Boost high-confidence recognitions
 *
 * APPLICATIONS:
 * - Object recognition: Quantum search through visual prototypes
 * - Semantic retrieval: Grover search for concept by embedding similarity
 * - Multimodal binding: Quantum interference for audio-visual integration
 * - Category learning: Quantum-accelerated prototype formation
 *
 * @author NIMCP Team
 * @date 2025-12-30
 */

#ifndef NIMCP_TEMPORAL_QUANTUM_BRIDGE_H
#define NIMCP_TEMPORAL_QUANTUM_BRIDGE_H

#include "cognitive/reasoning/nimcp_quantum_reasoning.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Types
//=============================================================================

typedef struct temporal_quantum_bridge temporal_quantum_bridge_t;

/**
 * @brief Quantum temporal configuration
 */
typedef struct {
    bool enabled;                        /**< Enable quantum optimization */
    uint32_t prototype_search_depth;     /**< Max prototype search depth (default: 1000) */
    uint32_t concept_search_depth;       /**< Max concept search depth (default: 2000) */
    uint32_t max_grover_iterations;      /**< Max Grover iterations (default: 10) */
    float min_recognition_confidence;    /**< Min confidence for recognition (default: 0.5) */
    float min_concept_similarity;        /**< Min similarity for concept match (default: 0.6) */
    bool enable_interference;            /**< Enable quantum interference (default: true) */
    bool use_superposition;              /**< Use superposition for alternatives (default: true) */
    bool enable_multimodal_binding;      /**< Enable audio-visual quantum binding (default: true) */
    uint32_t seed;                       /**< Random seed (default: 42) */
} temporal_quantum_config_t;

/**
 * @brief Object recognition candidate from quantum search
 */
typedef struct {
    uint32_t object_id;                  /**< Object identifier */
    char object_name[64];                /**< Object name */
    float amplitude;                     /**< Quantum amplitude [0, 1] */
    float feature_match;                 /**< Feature similarity [0, 1] */
    float prototype_distance;            /**< Distance to prototype */
    float combined_score;                /**< Combined recognition score */
} quantum_object_candidate_t;

/**
 * @brief Object search result
 */
typedef struct {
    quantum_object_candidate_t* best_candidate; /**< Best object found */
    uint32_t candidates_evaluated;              /**< Total candidates */
    float satisfaction_probability;              /**< Search success probability */
    uint32_t grover_iterations_used;            /**< Grover iterations */
    float search_speedup;                        /**< Speedup vs linear search */
} quantum_object_result_t;

/**
 * @brief Semantic concept candidate
 */
typedef struct {
    uint32_t concept_id;                 /**< Concept identifier */
    char concept_name[64];               /**< Concept name */
    float amplitude;                     /**< Quantum amplitude [0, 1] */
    float embedding_similarity;          /**< Embedding cosine similarity [0, 1] */
    float activation_level;              /**< Pre-existing activation [0, 1] */
    float combined_score;                /**< Combined retrieval score */
} quantum_concept_candidate_t;

/**
 * @brief Concept search result
 */
typedef struct {
    quantum_concept_candidate_t* best_concept;  /**< Best concept found */
    uint32_t concepts_evaluated;                /**< Total concepts searched */
    float satisfaction_probability;              /**< Search success probability */
    uint32_t grover_iterations_used;            /**< Grover iterations */
    float search_speedup;                        /**< Speedup vs linear search */
} quantum_concept_result_t;

/**
 * @brief Multimodal binding result
 */
typedef struct {
    uint32_t visual_object_id;           /**< Matched visual object */
    uint32_t auditory_source_id;         /**< Matched auditory source */
    uint32_t concept_id;                 /**< Bound concept ID */
    float binding_strength;              /**< Binding coherence [0, 1] */
    float interference_pattern;          /**< Quantum interference effect */
    bool is_coherent;                    /**< Coherent multimodal binding */
} quantum_multimodal_binding_t;

/**
 * @brief Statistics for quantum temporal operations
 */
typedef struct {
    uint64_t object_searches;            /**< Total object searches */
    uint64_t concept_searches;           /**< Total concept searches */
    uint64_t multimodal_bindings;        /**< Total multimodal bindings */
    float avg_object_speedup;            /**< Average object search speedup */
    float avg_concept_speedup;           /**< Average concept search speedup */
    float avg_satisfaction_prob;         /**< Average success probability */
    uint64_t successful_searches;        /**< Searches with high confidence */
    uint64_t failed_searches;            /**< Searches with low confidence */
} temporal_quantum_stats_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default quantum temporal configuration
 * @return Default configuration
 */
temporal_quantum_config_t temporal_quantum_default_config(void);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create quantum temporal bridge
 * @param temporal Temporal adapter handle
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
temporal_quantum_bridge_t* temporal_quantum_bridge_create(
    void* temporal,
    const temporal_quantum_config_t* config
);

/**
 * @brief Destroy quantum temporal bridge
 * @param bridge Bridge to destroy
 */
void temporal_quantum_bridge_destroy(temporal_quantum_bridge_t* bridge);

/**
 * @brief Check if quantum optimization is enabled
 * @param bridge Quantum bridge
 * @return true if enabled
 */
bool temporal_quantum_bridge_is_enabled(const temporal_quantum_bridge_t* bridge);

/**
 * @brief Enable or disable quantum optimization
 * @param bridge Quantum bridge
 * @param enabled Enable flag
 */
void temporal_quantum_bridge_set_enabled(temporal_quantum_bridge_t* bridge, bool enabled);

//=============================================================================
// Object Recognition API
//=============================================================================

/**
 * @brief Search object prototypes using quantum Grover algorithm
 * @param bridge Quantum bridge
 * @param query_features Query feature vector
 * @param feature_dim Feature dimension
 * @param prototype_count Number of prototypes to search
 * @param result Output: search result
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(sqrt(N)) vs O(N) linear search
 */
int temporal_quantum_search_objects(
    temporal_quantum_bridge_t* bridge,
    const float* query_features,
    uint32_t feature_dim,
    uint32_t prototype_count,
    quantum_object_result_t* result
);

/**
 * @brief Recognize object with quantum-accelerated matching
 * @param bridge Quantum bridge
 * @param features Object feature vector
 * @param feature_dim Feature dimension
 * @param top_k Number of top matches to return
 * @param candidates Output: array of candidates (must be pre-allocated)
 * @param num_candidates Output: number of candidates returned
 * @return 0 on success, -1 on error
 */
int temporal_quantum_recognize_topk(
    temporal_quantum_bridge_t* bridge,
    const float* features,
    uint32_t feature_dim,
    uint32_t top_k,
    quantum_object_candidate_t* candidates,
    uint32_t* num_candidates
);

//=============================================================================
// Semantic Memory API
//=============================================================================

/**
 * @brief Search semantic memory using quantum Grover algorithm
 * @param bridge Quantum bridge
 * @param query_embedding Query concept embedding
 * @param embedding_dim Embedding dimension
 * @param concept_count Number of concepts to search
 * @param result Output: search result
 * @return 0 on success, -1 on error
 */
int temporal_quantum_search_concepts(
    temporal_quantum_bridge_t* bridge,
    const float* query_embedding,
    uint32_t embedding_dim,
    uint32_t concept_count,
    quantum_concept_result_t* result
);

/**
 * @brief Retrieve related concepts using quantum spreading activation
 * @param bridge Quantum bridge
 * @param seed_concept_id Starting concept ID
 * @param max_depth Maximum spreading depth
 * @param candidates Output: array of activated concepts
 * @param max_candidates Maximum candidates to return
 * @param num_candidates Output: number of candidates returned
 * @return 0 on success, -1 on error
 */
int temporal_quantum_spread_activation(
    temporal_quantum_bridge_t* bridge,
    uint32_t seed_concept_id,
    uint32_t max_depth,
    quantum_concept_candidate_t* candidates,
    uint32_t max_candidates,
    uint32_t* num_candidates
);

//=============================================================================
// Multimodal Binding API
//=============================================================================

/**
 * @brief Bind auditory and visual features using quantum interference
 * @param bridge Quantum bridge
 * @param visual_features Visual feature vector
 * @param visual_dim Visual feature dimension
 * @param auditory_features Auditory feature vector
 * @param auditory_dim Auditory feature dimension
 * @param binding Output: multimodal binding result
 * @return 0 on success, -1 on error
 *
 * Uses quantum interference patterns to find coherent audio-visual bindings
 */
int temporal_quantum_bind_multimodal(
    temporal_quantum_bridge_t* bridge,
    const float* visual_features,
    uint32_t visual_dim,
    const float* auditory_features,
    uint32_t auditory_dim,
    quantum_multimodal_binding_t* binding
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get quantum temporal statistics
 * @param bridge Quantum bridge
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int temporal_quantum_get_stats(
    const temporal_quantum_bridge_t* bridge,
    temporal_quantum_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Quantum bridge
 */
void temporal_quantum_reset_stats(temporal_quantum_bridge_t* bridge);

/**
 * @brief Get current configuration
 * @param bridge Quantum bridge
 * @param config Output: configuration
 * @return 0 on success, -1 on error
 */
int temporal_quantum_get_config(
    const temporal_quantum_bridge_t* bridge,
    temporal_quantum_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TEMPORAL_QUANTUM_BRIDGE_H */
