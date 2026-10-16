#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_pattern_library.c - Pattern Template Storage and Matching
//=============================================================================

#include "middleware/patterns/nimcp_pattern_library.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "middleware/patterns/nimcp_pattern_cow.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"



#define LOG_MODULE "nimcp_pattern_library"
#define LOG_MODULE_ID 0x0526
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pattern_library)

// ============================================================================
// INTERNAL STRUCTURES
// ============================================================================

typedef struct pattern_node {
    pattern_template_t pattern;
    pattern_cow_t* pattern_data_cow;  // CoW wrapper for shared pattern data
    struct pattern_node* next;
} pattern_node_t;

#define HASH_TABLE_SIZE 256

struct pattern_library {
    // Configuration
    pattern_library_config_t config;

    // Pattern storage (hash table for O(1) lookup)
    pattern_node_t* hash_table[HASH_TABLE_SIZE];

    // Pattern list (for iteration)
    pattern_node_t** patterns;     // Array of pattern pointers
    uint32_t num_patterns;
    uint32_t next_pattern_id;

    // Memory pool for KNN similarity computations (Phase 1.4)
    memory_pool_t knn_temp_pool;

    // Statistics
    uint64_t total_matches;
    uint64_t total_adds;
    uint64_t total_updates;
    uint64_t total_prunes;
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static uint32_t hash_pattern_id(uint32_t id) {
    return id % HASH_TABLE_SIZE;
}

static float compute_l2_distance(const float* a, const float* b, uint32_t dim) {
    float sum = 0.0F;
    for (uint32_t i = 0; i < dim; i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrtf(sum);
}

static float compute_cosine_similarity(const float* a, const float* b, uint32_t dim) {
    float dot = 0.0F;
    float norm_a = 0.0F;
    float norm_b = 0.0F;

    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    norm_a = sqrtf(norm_a);
    norm_b = sqrtf(norm_b);

    if (norm_a < 1e-10F || norm_b < 1e-10F) {
        return 0.0F;
    }

    return dot / (norm_a * norm_b);
}

static float compute_correlation(const float* a, const float* b, uint32_t dim) {
    // Pearson correlation
    float mean_a = 0.0F;
    float mean_b = 0.0F;

    for (uint32_t i = 0; i < dim; i++) {
        mean_a += a[i];
        mean_b += b[i];
    }
    mean_a /= (float)dim;
    mean_b /= (float)dim;

    float cov = 0.0F;
    float var_a = 0.0F;
    float var_b = 0.0F;

    for (uint32_t i = 0; i < dim; i++) {
        float da = a[i] - mean_a;
        float db = b[i] - mean_b;
        cov += da * db;
        var_a += da * da;
        var_b += db * db;
    }

    float std_a = sqrtf(var_a / (float)dim);
    float std_b = sqrtf(var_b / (float)dim);

    if (std_a < 1e-10F || std_b < 1e-10F) {
        return 0.0F;
    }

    return cov / (std_a * std_b * (float)dim);
}

static float compute_jaccard_similarity(const float* a, const float* b, uint32_t dim) {
    // For sparse patterns (treat as sets)
    uint32_t intersection = 0;
    uint32_t union_count = 0;

    for (uint32_t i = 0; i < dim; i++) {
        bool a_active = (a[i] > 0.5F);
        bool b_active = (b[i] > 0.5F);

        if (a_active && b_active) intersection++;
        if (a_active || b_active) union_count++;
    }

    if (union_count == 0) return 1.0F;

    return (float)intersection / (float)union_count;
}

static float compute_similarity(const pattern_library_t* library,
                               const float* a, const float* b, uint32_t dim) {
    switch (library->config.metric) {
        case SIMILARITY_L2_DISTANCE: {
            float dist = compute_l2_distance(a, b, dim);
            // Convert distance to similarity [0, 1]
            return 1.0F / (1.0F + dist);
        }
        case SIMILARITY_COSINE:
            return compute_cosine_similarity(a, b, dim);
        case SIMILARITY_CORRELATION:
            return (compute_correlation(a, b, dim) + 1.0F) / 2.0F;  // Map [-1,1] to [0,1]
        case SIMILARITY_JACCARD:
            return compute_jaccard_similarity(a, b, dim);
        default:
            return compute_cosine_similarity(a, b, dim);
    }
}

static pattern_node_t* find_pattern_node(const pattern_library_t* library,
                                        uint32_t pattern_id) {
    uint32_t hash = hash_pattern_id(pattern_id);
    pattern_node_t* node = library->hash_table[hash];

    while (node) {
        if (node->pattern.pattern_id == pattern_id) {
            return node;
        }
        node = node->next;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "compute_jaccard_similarity: validation failed");
    return NULL;
}

static void free_pattern_node(pattern_node_t* node) {
    if (!node) return;

    // Release CoW reference (will free data if last reference)
    pattern_cow_release(node->pattern_data_cow);

    nimcp_free(node->pattern.metadata);
    nimcp_free(node);
}

// ============================================================================
// PUBLIC API
// ============================================================================

pattern_library_config_t pattern_library_default_config(void) {
    pattern_library_config_t config;
    config.max_capacity = PATTERN_LIB_MAX_CAPACITY;
    config.max_dimension = PATTERN_MAX_DIMENSION;
    config.similarity_threshold = PATTERN_MIN_SIMILARITY;
    config.metric = SIMILARITY_COSINE;
    config.enable_pruning = true;
    config.pruning_threshold = PATTERN_PRUNING_THRESHOLD;
    config.enable_incremental_learning = true;
    config.learning_rate = 0.1F;
    return config;
}

pattern_library_t* pattern_library_create(const pattern_library_config_t* config) {
    // Allow NULL config - use defaults
    pattern_library_config_t default_config;
    if (!config) {
        default_config = pattern_library_default_config();
        config = &default_config;
    }

    if (config->max_capacity == 0 || config->max_dimension == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pattern_library_create: max_capacity or max_dimension is 0");
        return NULL;
    }

    pattern_library_t* library = (pattern_library_t*)nimcp_calloc(1, sizeof(pattern_library_t));
    if (!library) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pattern_library_create: failed to allocate library");
        return NULL;
    }

    library->config = *config;

    // Initialize hash table
    memset(library->hash_table, 0, sizeof(library->hash_table));

    // Allocate pattern list
    library->patterns = (pattern_node_t**)nimcp_calloc(config->max_capacity,
                                                 sizeof(pattern_node_t*));
    if (!library->patterns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pattern_library_create: failed to allocate patterns");
        pattern_library_destroy(library);
        return NULL;
    }

    library->num_patterns = 0;
    library->next_pattern_id = 1;

    // Initialize memory pool for KNN temp arrays (Phase 1.4)
    memory_pool_config_t pool_config = {
        .block_size = config->max_capacity * sizeof(void*) * 2,  // Max patterns × similarity pair size
        .num_blocks = 2,       // Double buffer
        .alignment = 16,
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    library->knn_temp_pool = memory_pool_create(&pool_config);
    if (!library->knn_temp_pool) {
        pattern_library_destroy(library);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pattern_library_create: library->knn_temp_pool is NULL");
        return NULL;
    }

    // Initialize statistics
    library->total_matches = 0;
    library->total_adds = 0;
    library->total_updates = 0;
    library->total_prunes = 0;

    return library;
}

void pattern_library_destroy(pattern_library_t* library) {
    if (!library) return;

    // Free all patterns
    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        pattern_node_t* node = library->hash_table[i];
        while (node) {
            pattern_node_t* next = node->next;
            free_pattern_node(node);
            node = next;
        }
    }

    // Destroy memory pool (Phase 1.4)
    memory_pool_destroy(library->knn_temp_pool);

    nimcp_free(library->patterns);
    nimcp_free(library);
}

bool pattern_library_add(pattern_library_t* library,
                         const float* data,
                         uint32_t dimension,
                         const void* metadata,
                         uint32_t metadata_size,
                         uint32_t* pattern_id) {
    if (!library || !data || dimension == 0 ||
        dimension > library->config.max_dimension ||
        library->num_patterns >= library->config.max_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pattern_library_destroy: operation failed");
        return false;
    }

    // Allocate pattern node
    pattern_node_t* node = (pattern_node_t*)nimcp_calloc(1, sizeof(pattern_node_t));
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pattern_library_destroy: node is NULL");
        return false;
    }

    // Create CoW wrapper for pattern data (Phase 1.4)
    node->pattern_data_cow = pattern_cow_create(data, dimension);
    if (!node->pattern_data_cow) {
        nimcp_free(node);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_library_destroy: node->pattern_data_cow is NULL");
        return false;
    }

    // Point pattern.data to CoW data (read-only access)
    node->pattern.data = (float*)pattern_cow_data(node->pattern_data_cow);
    node->pattern.dimension = dimension;
    node->pattern.pattern_id = library->next_pattern_id++;
    node->pattern.usage_count = 0;
    node->pattern.last_used_ms = 0;
    node->pattern.avg_similarity = 0.0F;

    // Copy metadata if provided
    if (metadata && metadata_size > 0) {
        node->pattern.metadata = nimcp_malloc(metadata_size);
        if (node->pattern.metadata) {
            memcpy(node->pattern.metadata, metadata, metadata_size);
            node->pattern.metadata_size = metadata_size;
        }
    } else {
        node->pattern.metadata = NULL;
        node->pattern.metadata_size = 0;
    }

    // Add to hash table
    uint32_t hash = hash_pattern_id(node->pattern.pattern_id);
    node->next = library->hash_table[hash];
    library->hash_table[hash] = node;

    // Add to pattern list
    library->patterns[library->num_patterns] = node;
    library->num_patterns++;

    library->total_adds++;

    if (pattern_id) *pattern_id = node->pattern.pattern_id;

    return true;
}

bool pattern_library_match(pattern_library_t* library,
                           const float* data,
                           uint32_t dimension,
                           pattern_match_t* match) {
    if (!library || !data || !match || dimension == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_library_destroy: required parameter is NULL (library, data, match)");
        return false;
    }

    float best_similarity = -1.0F;
    uint32_t best_id = 0;

    // Search all patterns
    for (uint32_t i = 0; i < library->num_patterns; i++) {
        pattern_node_t* node = library->patterns[i];
        if (node->pattern.dimension != dimension) continue;

        float similarity = compute_similarity(library, data,
                                             node->pattern.data, dimension);

        if (similarity > best_similarity) {
            best_similarity = similarity;
            best_id = node->pattern.pattern_id;
        }
    }

    // Check threshold
    if (best_similarity < library->config.similarity_threshold) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pattern_library_destroy: validation failed");
        return false;
    }

    // Update pattern statistics
    pattern_node_t* best_node = find_pattern_node(library, best_id);
    if (best_node) {
        best_node->pattern.usage_count++;
        best_node->pattern.last_used_ms++;  // Placeholder timestamp

        float alpha = 0.1F;
        best_node->pattern.avg_similarity =
            (1.0F - alpha) * best_node->pattern.avg_similarity + alpha * best_similarity;

        // Incremental learning
        if (library->config.enable_incremental_learning) {
            float lr = library->config.learning_rate;
            for (uint32_t i = 0; i < dimension; i++) {
                best_node->pattern.data[i] =
                    (1.0F - lr) * best_node->pattern.data[i] + lr * data[i];
            }
            library->total_updates++;
        }
    }

    match->pattern_id = best_id;
    match->similarity = best_similarity;
    match->num_mismatches = 0;  // Would need element-wise comparison
    match->is_exact = (best_similarity > 0.99F);

    library->total_matches++;

    return true;
}

bool pattern_library_knn(pattern_library_t* library,
                         const float* data,
                         uint32_t dimension,
                         uint32_t k,
                         pattern_match_t* matches,
                         uint32_t* num_found) {
    if (!library || !data || !matches || !num_found || k == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_library_destroy: required parameter is NULL (library, data, matches, num_found)");
        return false;
    }

    *num_found = 0;

    // Compute all similarities
    typedef struct {
        uint32_t pattern_id;
        float similarity;
    } sim_pair_t;

    // Use memory pool for temp array (Phase 1.4 - 1.13x faster than malloc)
    sim_pair_t* all_sims = (sim_pair_t*)memory_pool_acquire(library->knn_temp_pool);
    if (!all_sims) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pattern_library_destroy: all_sims is NULL");
        return false;
    }

    uint32_t valid_count = 0;

    for (uint32_t i = 0; i < library->num_patterns; i++) {
        pattern_node_t* node = library->patterns[i];
        if (node->pattern.dimension != dimension) continue;

        float similarity = compute_similarity(library, data,
                                             node->pattern.data, dimension);

        if (similarity >= library->config.similarity_threshold) {
            all_sims[valid_count].pattern_id = node->pattern.pattern_id;
            all_sims[valid_count].similarity = similarity;
            valid_count++;
        }
    }

    // Sort by similarity (bubble sort for simplicity)
    for (uint32_t i = 0; i < valid_count; i++) {
        for (uint32_t j = i + 1; j < valid_count; j++) {
            if (all_sims[j].similarity > all_sims[i].similarity) {
                sim_pair_t temp = all_sims[i];
                all_sims[i] = all_sims[j];
                all_sims[j] = temp;
            }
        }
    }

    // Return top K
    uint32_t num_to_return = (k < valid_count) ? k : valid_count;

    for (uint32_t i = 0; i < num_to_return; i++) {
        matches[i].pattern_id = all_sims[i].pattern_id;
        matches[i].similarity = all_sims[i].similarity;
        matches[i].num_mismatches = 0;
        matches[i].is_exact = (all_sims[i].similarity > 0.99F);
    }

    *num_found = num_to_return;

    // Release back to pool (Phase 1.4)
    memory_pool_release(library->knn_temp_pool, all_sims);

    return true;
}

bool pattern_library_get(const pattern_library_t* library,
                         uint32_t pattern_id,
                         pattern_template_t* pattern) {
    if (!library || !pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (library, pattern)");
        return false;
    }

    pattern_node_t* node = find_pattern_node(library, pattern_id);
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: node is NULL");
        return false;
    }

    *pattern = node->pattern;
    return true;
}

bool pattern_library_update(pattern_library_t* library,
                            uint32_t pattern_id,
                            const float* data,
                            uint32_t dimension) {
    if (!library || !data || dimension == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (library, data)");
        return false;
    }

    pattern_node_t* node = find_pattern_node(library, pattern_id);
    if (!node || node->pattern.dimension != dimension) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: node is NULL");
        return false;
    }

    float lr = library->config.learning_rate;
    for (uint32_t i = 0; i < dimension; i++) {
        node->pattern.data[i] = (1.0F - lr) * node->pattern.data[i] + lr * data[i];
    }

    library->total_updates++;

    return true;
}

bool pattern_library_remove(pattern_library_t* library, uint32_t pattern_id) {
    if (!library) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_library_remove: library is NULL");
        return false;
    }

    uint32_t hash = hash_pattern_id(pattern_id);
    pattern_node_t** prev = &library->hash_table[hash];
    pattern_node_t* node = library->hash_table[hash];

    while (node) {
        if (node->pattern.pattern_id == pattern_id) {
            *prev = node->next;

            // Remove from pattern list
            for (uint32_t i = 0; i < library->num_patterns; i++) {
                if (library->patterns[i] == node) {
                    library->patterns[i] = library->patterns[library->num_patterns - 1];
                    library->num_patterns--;
                    break;
                }
            }

            free_pattern_node(node);
            return true;
        }
        prev = &node->next;
        node = node->next;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pattern_library_remove: operation failed");
    return false;
}

bool pattern_library_prune(pattern_library_t* library,
                           uint32_t min_usage,
                           uint32_t* num_removed) {
    if (!library) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_library_remove: library is NULL");
        return false;
    }

    uint32_t removed = 0;

    for (uint32_t i = 0; i < library->num_patterns; ) {
        pattern_node_t* node = library->patterns[i];

        if (node->pattern.usage_count < min_usage) {
            pattern_library_remove(library, node->pattern.pattern_id);
            removed++;
            // Don't increment i, as we removed an element
        } else {
            i++;
        }
    }

    if (num_removed) *num_removed = removed;

    library->total_prunes += removed;

    return true;
}

bool pattern_library_get_stats(const pattern_library_t* library,
                               uint32_t* num_patterns,
                               float* capacity_used,
                               float* avg_dimension,
                               uint64_t* total_matches) {
    if (!library) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_library_remove: library is NULL");
        return false;
    }

    if (num_patterns) *num_patterns = library->num_patterns;

    if (capacity_used) {
        *capacity_used = (float)library->num_patterns /
                        (float)library->config.max_capacity;
    }

    if (avg_dimension) {
        if (library->num_patterns > 0) {
            uint64_t sum_dim = 0;
            for (uint32_t i = 0; i < library->num_patterns; i++) {
                sum_dim += library->patterns[i]->pattern.dimension;
            }
            *avg_dimension = (float)sum_dim / (float)library->num_patterns;
        } else {
            *avg_dimension = 0.0F;
        }
    }

    if (total_matches) *total_matches = library->total_matches;

    return true;
}

void pattern_library_clear(pattern_library_t* library) {
    if (!library) return;

    // Remove all patterns
    while (library->num_patterns > 0) {
        pattern_node_t* node = library->patterns[0];
        pattern_library_remove(library, node->pattern.pattern_id);
    }
}

float pattern_library_compute_similarity(const pattern_library_t* library,
                                         const float* pattern1,
                                         const float* pattern2,
                                         uint32_t dimension) {
    if (!library || !pattern1 || !pattern2 || dimension == 0) {
        return 0.0F;
    }

    return compute_similarity(library, pattern1, pattern2, dimension);
}
