//=============================================================================
// nimcp_pattern_library.h - Pattern Template Storage and Matching
//=============================================================================

#ifndef NIMCP_PATTERN_LIBRARY_H
#define NIMCP_PATTERN_LIBRARY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_pattern_library.h
 * @brief Store and match learned neural patterns
 *
 * WHAT: Template-based pattern recognition library with similarity matching
 * WHY:  Maintain reusable pattern knowledge for rapid recognition
 * HOW:  Hash-indexed template storage with fast similarity search
 *
 * BIOLOGICAL BASIS:
 * - Cortical columns store prototypical patterns (canonical circuits)
 * - Pattern completion from partial cues (attractor dynamics)
 * - Incremental learning strengthens existing patterns
 * - Synaptic homeostasis prunes unused patterns
 *
 * ALGORITHMS:
 * - Template matching with L2 distance or cosine similarity
 * - K-nearest neighbor search for partial matches
 * - Incremental averaging for pattern refinement
 * - LRU eviction for capacity management
 * - Hash indexing for O(1) lookup by ID
 */

// ============================================================================
// CONSTANTS
// ============================================================================

#define PATTERN_LIB_MAX_CAPACITY 1000        // Maximum stored patterns
#define PATTERN_MAX_DIMENSION 10000          // Maximum pattern size
#define PATTERN_MIN_SIMILARITY 0.7f          // Minimum match threshold
#define PATTERN_PRUNING_THRESHOLD 10         // Min usage before pruning

// ============================================================================
// TYPES
// ============================================================================

/**
 * @brief Similarity metric for pattern matching
 */
typedef enum {
    SIMILARITY_L2_DISTANCE,      // Euclidean distance
    SIMILARITY_COSINE,           // Cosine similarity
    SIMILARITY_CORRELATION,      // Pearson correlation
    SIMILARITY_JACCARD          // Jaccard index (for sparse patterns)
} similarity_metric_t;

/**
 * @brief Stored pattern template
 */
typedef struct {
    uint32_t pattern_id;         // Unique identifier
    float* data;                 // Pattern vector
    uint32_t dimension;          // Vector dimension
    uint32_t usage_count;        // Times matched
    uint64_t last_used_ms;       // Last access time
    float avg_similarity;        // Average match similarity
    void* metadata;              // Optional metadata
    uint32_t metadata_size;      // Metadata size in bytes
} pattern_template_t;

/**
 * @brief Pattern match result
 */
#ifndef NIMCP_PATTERN_MATCH_T_DEFINED
#define NIMCP_PATTERN_MATCH_T_DEFINED
typedef struct pattern_match_t {
    uint32_t pattern_id;         // Matched pattern ID
    float similarity;            // Match similarity [0.0, 1.0]
    uint32_t num_mismatches;     // Elements below threshold
    bool is_exact;               // Perfect match (similarity = 1.0)
} pattern_match_t;
#endif

/**
 * @brief Pattern library configuration
 */
typedef struct {
    uint32_t max_capacity;           // Maximum patterns to store
    uint32_t max_dimension;          // Maximum pattern dimension
    float similarity_threshold;      // Minimum similarity for match
    similarity_metric_t metric;      // Similarity metric
    bool enable_pruning;             // Auto-prune unused patterns
    uint32_t pruning_threshold;      // Min usage before pruning
    bool enable_incremental_learning; // Update patterns on match
    float learning_rate;             // Incremental learning rate
} pattern_library_config_t;

/**
 * @brief Opaque pattern library handle
 */
typedef struct pattern_library pattern_library_t;

// ============================================================================
// API
// ============================================================================

/**
 * @brief Create pattern library with configuration
 *
 * WHAT: Initialize pattern storage and matching system
 * WHY:  Set up indexed storage for efficient retrieval
 * HOW:  Allocate hash table and pattern slots
 *
 * @param config Library configuration (NULL for defaults)
 * @return Library handle or NULL on failure
 */
pattern_library_t* pattern_library_create(const pattern_library_config_t* config);

/**
 * @brief Destroy pattern library and free resources
 */
void pattern_library_destroy(pattern_library_t* library);

/**
 * @brief Add new pattern to library
 *
 * WHAT: Store pattern template for future matching
 * WHY:  Build pattern knowledge base
 * HOW:  Allocate storage, assign ID, index by hash
 *
 * @param library Library handle
 * @param data Pattern vector
 * @param dimension Vector dimension
 * @param metadata Optional metadata (can be NULL)
 * @param metadata_size Metadata size in bytes
 * @param pattern_id Output: assigned pattern ID
 * @return true on success, false on error
 */
bool pattern_library_add(pattern_library_t* library,
                         const float* data,
                         uint32_t dimension,
                         const void* metadata,
                         uint32_t metadata_size,
                         uint32_t* pattern_id);

/**
 * @brief Match pattern against library
 *
 * WHAT: Find best matching stored pattern
 * WHY:  Recognize learned patterns in new data
 * HOW:  Compute similarity to all patterns, return best match
 *
 * @param library Library handle
 * @param data Query pattern vector
 * @param dimension Vector dimension
 * @param match Output: best match result (required)
 * @return true if match found, false otherwise
 */
bool pattern_library_match(pattern_library_t* library,
                           const float* data,
                           uint32_t dimension,
                           pattern_match_t* match);

/**
 * @brief Find K nearest neighbors in pattern space
 *
 * WHAT: Retrieve K most similar patterns
 * WHY:  Get multiple match candidates for disambiguation
 * HOW:  Heap-based K-best selection during linear scan
 *
 * @param library Library handle
 * @param data Query pattern vector
 * @param dimension Vector dimension
 * @param k Number of neighbors to return
 * @param matches Output array for matches
 * @param num_found Output: number of matches found
 * @return true on success, false on error
 */
bool pattern_library_knn(pattern_library_t* library,
                         const float* data,
                         uint32_t dimension,
                         uint32_t k,
                         pattern_match_t* matches,
                         uint32_t* num_found);

/**
 * @brief Get pattern by ID
 *
 * @param library Library handle
 * @param pattern_id Pattern identifier
 * @param pattern Output: pattern template (required)
 * @return true if found, false otherwise
 */
bool pattern_library_get(const pattern_library_t* library,
                         uint32_t pattern_id,
                         pattern_template_t* pattern);

/**
 * @brief Update pattern with new observation
 *
 * WHAT: Incrementally refine pattern template
 * WHY:  Improve pattern through multiple observations
 * HOW:  Exponential moving average with learning rate
 *
 * @param library Library handle
 * @param pattern_id Pattern to update
 * @param data New observation
 * @param dimension Vector dimension
 * @return true on success, false on error
 */
bool pattern_library_update(pattern_library_t* library,
                            uint32_t pattern_id,
                            const float* data,
                            uint32_t dimension);

/**
 * @brief Remove pattern from library
 *
 * @param library Library handle
 * @param pattern_id Pattern to remove
 * @return true on success, false if not found
 */
bool pattern_library_remove(pattern_library_t* library, uint32_t pattern_id);

/**
 * @brief Prune unused patterns
 *
 * WHAT: Remove patterns below usage threshold
 * WHY:  Free space for new patterns, maintain relevance
 * HOW:  Sort by usage count, remove bottom N patterns
 *
 * @param library Library handle
 * @param min_usage Minimum usage count to keep
 * @param num_removed Output: number of patterns removed
 * @return true on success, false on error
 */
bool pattern_library_prune(pattern_library_t* library,
                           uint32_t min_usage,
                           uint32_t* num_removed);

/**
 * @brief Get library statistics
 *
 * @param library Library handle
 * @param num_patterns Output: current pattern count
 * @param capacity_used Output: fraction of capacity used
 * @param avg_dimension Output: average pattern dimension
 * @param total_matches Output: lifetime matches
 * @return true on success, false on error
 */
bool pattern_library_get_stats(const pattern_library_t* library,
                               uint32_t* num_patterns,
                               float* capacity_used,
                               float* avg_dimension,
                               uint64_t* total_matches);

/**
 * @brief Clear all patterns from library
 *
 * WHAT: Remove all stored patterns
 * WHY:  Start learning from scratch
 * HOW:  Free all pattern storage, reset counters
 */
void pattern_library_clear(pattern_library_t* library);

/**
 * @brief Get default configuration
 */
pattern_library_config_t pattern_library_default_config(void);

/**
 * @brief Compute similarity between two patterns
 *
 * WHAT: Calculate similarity score
 * WHY:  Utility function for custom matching
 * HOW:  Apply configured similarity metric
 *
 * @param library Library handle (for metric)
 * @param pattern1 First pattern
 * @param pattern2 Second pattern
 * @param dimension Pattern dimension
 * @return Similarity score [0.0, 1.0]
 */
float pattern_library_compute_similarity(const pattern_library_t* library,
                                         const float* pattern1,
                                         const float* pattern2,
                                         uint32_t dimension);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PATTERN_LIBRARY_H
