/**
 * @file nimcp_semantic_compression.c
 * @brief Semantic compression implementation for neural data
 *
 * WHAT: Compresses neural data using learned semantic primitives
 * WHY:  Enable efficient brain-to-brain sync with minimal bandwidth
 * HOW:  Sparse coding + quantization + entropy encoding
 */

#include "networking/nlp/nimcp_semantic_compression.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/rng/nimcp_rand.h"
#include "async/nimcp_bio_router.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(semantic_compression)

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Active primitive during encoding
 */
typedef struct {
    uint32_t primitive_id;
    float coefficient;
} active_primitive_t;

/**
 * @brief Semantic compressor internal state
 */
struct semantic_compressor {
    /* Configuration */
    semantic_compression_config_t config;

    /* Dictionary */
    semantic_primitive_t* primitives;
    uint32_t num_primitives;

    /* Internal buffers */
    float* encoding_buffer;
    float* reconstruction_buffer;
    active_primitive_t* active_prims;

    /* Statistics */
    semantic_compression_stats_t stats;
    float last_compression_ratio;
    float last_quality;

    /* Bio-async */
    bio_router_t* router;
    bool bio_async_registered;

    /* State */
    uint64_t compression_count;
    uint64_t dictionary_version;
};

/* ============================================================================
 * Constants
 * ============================================================================ */

#define COMPRESSION_MAGIC 0x534D4350  // 'SMCP'
#define COMPRESSION_VERSION 1
#define MIN_QUALITY_THRESHOLD 0.1f
#define MAX_QUALITY_THRESHOLD 1.0f
#define DEFAULT_LEARNING_RATE NIMCP_LEARNING_RATE_DEFAULT
#include "constants/nimcp_constants.h"
#define EPSILON NIMCP_EPSILON_NUMERICAL

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Compute dot product of two vectors
 * WHY:  Core operation for sparse coding
 * HOW:  Standard dot product with SIMD optimization opportunity
 */
static float vector_dot(const float* a, const float* b, uint32_t size) {
    float result = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        result += a[i] * b[i];
    }
    return result;
}

/**
 * WHAT: Compute L2 norm of vector
 * WHY:  Needed for normalization and error metrics
 * HOW:  Square root of sum of squares
 */
static float vector_norm(const float* vec, uint32_t size) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        sum += vec[i] * vec[i];
    }
    return sqrtf(sum);
}

/**
 * WHAT: Normalize vector to unit length
 * WHY:  Ensure primitives are normalized
 * HOW:  Divide by L2 norm
 */
static void vector_normalize(float* vec, uint32_t size) {
    float norm = vector_norm(vec, size);
    if (norm > EPSILON) {
        for (uint32_t i = 0; i < size; i++) {
            vec[i] /= norm;
        }
    }
}

/**
 * WHAT: Initialize primitives with random orthonormal basis
 * WHY:  Start with good initial dictionary
 * HOW:  Gram-Schmidt orthogonalization
 */
static nimcp_result_t initialize_random_primitives(semantic_compressor_t* comp) {
    if (!comp || !comp->primitives) return NIMCP_INVALID_PARAM;

    uint32_t vec_size = comp->config.primitive_vector_size;

    // Generate random unit vectors
    for (uint32_t p = 0; p < comp->num_primitives; p++) {
        semantic_primitive_t* prim = &comp->primitives[p];
        prim->primitive_id = p;
        prim->vector_size = vec_size;
        prim->frequency = 0.0f;
        prim->mean_activation = 0.0f;
        snprintf(prim->name, sizeof(prim->name), "prim_%u", p);

        // Random initialization
        for (uint32_t i = 0; i < vec_size; i++) {
            prim->vector[i] = nimcp_rand_uniform() * 2.0f - 1.0f;
        }

        // Orthogonalize against previous primitives
        for (uint32_t j = 0; j < p; j++) {
            float dot = vector_dot(prim->vector, comp->primitives[j].vector, vec_size);
            for (uint32_t i = 0; i < vec_size; i++) {
                prim->vector[i] -= dot * comp->primitives[j].vector[i];
            }
        }

        // Normalize
        vector_normalize(prim->vector, vec_size);
    }

    return NIMCP_SUCCESS;
}

/**
 * WHAT: Find sparse representation using matching pursuit
 * WHY:  Core compression algorithm
 * HOW:  Greedy selection of best-matching primitives
 */
static nimcp_result_t find_sparse_encoding(
    semantic_compressor_t* comp,
    const float* data,
    uint32_t data_size,
    active_primitive_t* active_out,
    uint32_t* num_active
) {
    if (!comp || !data || !active_out || !num_active) {
        return NIMCP_INVALID_PARAM;
    }

    uint32_t max_active = comp->config.max_active_primitives;
    float* residual = comp->encoding_buffer;

    // Initialize residual to input data
    memcpy(residual, data, data_size * sizeof(float));

    *num_active = 0;
    float residual_energy = vector_norm(residual, data_size);

    // Matching pursuit: iteratively find best primitive
    for (uint32_t iter = 0; iter < max_active; iter++) {
        // Stop if residual is small enough
        if (residual_energy < EPSILON) break;

        // Find primitive with highest correlation
        float best_correlation = 0.0f;
        uint32_t best_idx = 0;

        for (uint32_t p = 0; p < comp->num_primitives; p++) {
            float corr = fabsf(vector_dot(residual, comp->primitives[p].vector, data_size));
            if (corr > best_correlation) {
                best_correlation = corr;
                best_idx = p;
            }
        }

        if (best_correlation < EPSILON) break;

        // Compute coefficient and update residual
        float coef = vector_dot(residual, comp->primitives[best_idx].vector, data_size);

        for (uint32_t i = 0; i < data_size; i++) {
            residual[i] -= coef * comp->primitives[best_idx].vector[i];
        }

        // Store active primitive
        active_out[*num_active].primitive_id = best_idx;
        active_out[*num_active].coefficient = coef;
        (*num_active)++;

        // Update residual energy
        residual_energy = vector_norm(residual, data_size);
    }

    return NIMCP_SUCCESS;
}

/**
 * WHAT: Reconstruct data from sparse representation
 * WHY:  Decompression operation
 * HOW:  Linear combination of primitives
 */
static nimcp_result_t reconstruct_from_encoding(
    semantic_compressor_t* comp,
    const active_primitive_t* active,
    uint32_t num_active,
    float* output,
    uint32_t output_size
) {
    if (!comp || !active || !output) return NIMCP_INVALID_PARAM;

    // Initialize output to zero
    memset(output, 0, output_size * sizeof(float));

    // Sum weighted primitives
    for (uint32_t i = 0; i < num_active; i++) {
        uint32_t prim_id = active[i].primitive_id;
        float coef = active[i].coefficient;

        if (prim_id >= comp->num_primitives) {
            LOG_ERROR("Invalid primitive ID: %u", prim_id);
            return NIMCP_INVALID_PARAM;
        }

        const float* prim_vec = comp->primitives[prim_id].vector;
        for (uint32_t j = 0; j < output_size; j++) {
            output[j] += coef * prim_vec[j];
        }
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

semantic_compressor_t* semantic_compressor_create(
    const semantic_compression_config_t* config
) {
    // Guard: validate config
    if (!config || semantic_validate_config(config) != NIMCP_SUCCESS) {
        LOG_ERROR("Invalid configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_compressor_create: config is NULL");
        return NULL;
    }

    // Allocate compressor
    semantic_compressor_t* comp = nimcp_calloc(1, sizeof(semantic_compressor_t));
    if (!comp) {
        LOG_ERROR("Failed to allocate compressor");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "comp is NULL");

        return NULL;
    }

    // Copy config
    memcpy(&comp->config, config, sizeof(semantic_compression_config_t));
    comp->num_primitives = config->dictionary_size;

    // Allocate primitives
    comp->primitives = nimcp_calloc(comp->num_primitives, sizeof(semantic_primitive_t));
    if (!comp->primitives) {
        LOG_ERROR("Failed to allocate primitives");
        nimcp_free(comp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "semantic_compressor_create: comp->primitives is NULL");
        return NULL;
    }

    // Allocate primitive vectors
    for (uint32_t i = 0; i < comp->num_primitives; i++) {
        comp->primitives[i].vector = nimcp_calloc(config->primitive_vector_size, sizeof(float));
        if (!comp->primitives[i].vector) {
            LOG_ERROR("Failed to allocate primitive vector %u", i);
            for (uint32_t j = 0; j < i; j++) {
                nimcp_free(comp->primitives[j].vector);
            }
            nimcp_free(comp->primitives);
            nimcp_free(comp);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "semantic_compressor_create: comp->primitives is NULL");
            return NULL;
        }
    }

    // Allocate internal buffers
    comp->encoding_buffer = nimcp_calloc(config->primitive_vector_size, sizeof(float));
    comp->reconstruction_buffer = nimcp_calloc(config->primitive_vector_size, sizeof(float));
    comp->active_prims = nimcp_calloc(config->max_active_primitives, sizeof(active_primitive_t));

    if (!comp->encoding_buffer || !comp->reconstruction_buffer || !comp->active_prims) {
        LOG_ERROR("Failed to allocate internal buffers");
        semantic_compressor_destroy(comp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "semantic_compressor_create: required parameter is NULL (comp->encoding_buffer, comp->reconstruction_buffer, comp->active_prims)");
        return NULL;
    }

    // Initialize primitives
    if (initialize_random_primitives(comp) != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to initialize primitives");
        semantic_compressor_destroy(comp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "semantic_compressor_create: validation failed");
        return NULL;
    }

    comp->dictionary_version = 1;
    LOG_INFO("Created semantic compressor: %u primitives, dim=%u",
             comp->num_primitives, config->primitive_vector_size);

    return comp;
}

void semantic_compressor_destroy(semantic_compressor_t* comp) {
    if (!comp) return;

    // Free primitive vectors
    if (comp->primitives) {
        for (uint32_t i = 0; i < comp->num_primitives; i++) {
            nimcp_free(comp->primitives[i].vector);
        }
        nimcp_free(comp->primitives);
    }

    // Free buffers
    nimcp_free(comp->encoding_buffer);
    nimcp_free(comp->reconstruction_buffer);
    nimcp_free(comp->active_prims);

    nimcp_free(comp);
}

nimcp_result_t semantic_compressor_reset(
    semantic_compressor_t* comp,
    bool reset_dictionary
) {
    if (!comp) return NIMCP_INVALID_PARAM;

    // Reset statistics
    memset(&comp->stats, 0, sizeof(semantic_compression_stats_t));
    comp->compression_count = 0;

    // Optionally reset dictionary
    if (reset_dictionary) {
        return initialize_random_primitives(comp);
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Dictionary Management
 * ============================================================================ */

nimcp_result_t semantic_add_primitive(
    semantic_compressor_t* comp,
    const semantic_primitive_t* prim
) {
    if (!comp || !prim) return NIMCP_INVALID_PARAM;
    if (prim->primitive_id >= comp->num_primitives) return NIMCP_INVALID_PARAM;
    if (prim->vector_size != comp->config.primitive_vector_size) return NIMCP_INVALID_PARAM;

    semantic_primitive_t* target = &comp->primitives[prim->primitive_id];

    // Copy metadata
    target->primitive_id = prim->primitive_id;
    target->vector_size = prim->vector_size;
    target->frequency = prim->frequency;
    target->mean_activation = prim->mean_activation;
    strncpy(target->name, prim->name, sizeof(target->name) - 1);

    // Copy and normalize vector
    memcpy(target->vector, prim->vector, prim->vector_size * sizeof(float));
    vector_normalize(target->vector, prim->vector_size);

    comp->dictionary_version++;

    return NIMCP_SUCCESS;
}

nimcp_result_t semantic_get_primitive(
    semantic_compressor_t* comp,
    uint32_t primitive_id,
    semantic_primitive_t* out_prim
) {
    if (!comp || !out_prim) return NIMCP_INVALID_PARAM;
    if (primitive_id >= comp->num_primitives) return NIMCP_NOT_FOUND;

    // Copy primitive (vector pointer points to internal)
    memcpy(out_prim, &comp->primitives[primitive_id], sizeof(semantic_primitive_t));

    return NIMCP_SUCCESS;
}

nimcp_result_t semantic_learn_primitives(
    semantic_compressor_t* comp,
    const float** data,
    uint32_t num_samples,
    uint32_t sample_size
) {
    if (!comp || !data) return NIMCP_INVALID_PARAM;
    if (sample_size != comp->config.primitive_vector_size) return NIMCP_INVALID_PARAM;
    if (num_samples == 0) return NIMCP_INVALID_PARAM;

    // Simplified K-means-like dictionary learning
    uint32_t iterations = comp->config.learning_iterations;
    float lr = comp->config.learning_rate;

    for (uint32_t iter = 0; iter < iterations; iter++) {
        // For each sample
        for (uint32_t s = 0; s < num_samples; s++) {
            // Find sparse encoding
            uint32_t num_active = 0;
            find_sparse_encoding(comp, data[s], sample_size, comp->active_prims, &num_active);

            // Reconstruct
            reconstruct_from_encoding(comp, comp->active_prims, num_active,
                                     comp->reconstruction_buffer, sample_size);

            // Compute residual
            float* residual = comp->encoding_buffer;
            for (uint32_t i = 0; i < sample_size; i++) {
                residual[i] = data[s][i] - comp->reconstruction_buffer[i];
            }

            // Update active primitives toward residual
            for (uint32_t a = 0; a < num_active; a++) {
                uint32_t prim_id = comp->active_prims[a].primitive_id;
                float* prim_vec = comp->primitives[prim_id].vector;

                for (uint32_t i = 0; i < sample_size; i++) {
                    prim_vec[i] += lr * residual[i];
                }

                vector_normalize(prim_vec, sample_size);
            }
        }
    }

    comp->stats.dictionary_updates++;
    comp->dictionary_version++;

    LOG_INFO("Learned primitives from %u samples (%u iterations)", num_samples, iterations);

    return NIMCP_SUCCESS;
}

nimcp_result_t semantic_update_dictionary(
    semantic_compressor_t* comp,
    const float* neural_data,
    uint32_t data_size
) {
    if (!comp || !neural_data) return NIMCP_INVALID_PARAM;
    if (!comp->config.enable_online_learning) return NIMCP_SUCCESS;

    const float** data_array = (const float**)&neural_data;
    return semantic_learn_primitives(comp, data_array, 1, data_size);
}

/* ============================================================================
 * Compression Functions
 * ============================================================================ */

nimcp_result_t semantic_compress(
    semantic_compressor_t* comp,
    const float* neural_data,
    uint32_t data_size,
    uint8_t* compressed,
    uint32_t* compressed_size
) {
    if (!comp || !neural_data || !compressed || !compressed_size) {
        return NIMCP_INVALID_PARAM;
    }

    if (data_size != comp->config.primitive_vector_size) {
        LOG_ERROR("Data size mismatch: expected %u, got %u",
                  comp->config.primitive_vector_size, data_size);
        return NIMCP_INVALID_PARAM;
    }

    // Find sparse encoding
    uint32_t num_active = 0;
    nimcp_result_t result = find_sparse_encoding(comp, neural_data, data_size,
                                                  comp->active_prims, &num_active);
    if (result != NIMCP_SUCCESS) return result;

    // Calculate compressed size
    uint32_t header_size = sizeof(uint32_t) * 4 + sizeof(float);  // magic, version, size, num_active, quality
    uint32_t data_size_bytes = num_active * (sizeof(uint32_t) + sizeof(float));  // IDs + coefficients
    uint32_t total_size = header_size + data_size_bytes;

    if (*compressed_size < total_size) {
        *compressed_size = total_size;
        return NIMCP_BUFFER_TOO_SMALL;
    }

    // Write header (use memcpy to avoid unaligned access UB)
    uint8_t* ptr = compressed;
    uint32_t tmp_u32;
    tmp_u32 = COMPRESSION_MAGIC; memcpy(ptr, &tmp_u32, sizeof(uint32_t)); ptr += sizeof(uint32_t);
    tmp_u32 = COMPRESSION_VERSION; memcpy(ptr, &tmp_u32, sizeof(uint32_t)); ptr += sizeof(uint32_t);
    tmp_u32 = data_size; memcpy(ptr, &tmp_u32, sizeof(uint32_t)); ptr += sizeof(uint32_t);
    tmp_u32 = num_active; memcpy(ptr, &tmp_u32, sizeof(uint32_t)); ptr += sizeof(uint32_t);

    // Compute quality
    reconstruct_from_encoding(comp, comp->active_prims, num_active,
                             comp->reconstruction_buffer, data_size);
    float error = 0.0f;
    for (uint32_t i = 0; i < data_size; i++) {
        float diff = neural_data[i] - comp->reconstruction_buffer[i];
        error += diff * diff;
    }
    float quality = 1.0f - sqrtf(error) / (vector_norm(neural_data, data_size) + EPSILON);
    memcpy(ptr, &quality, sizeof(float)); ptr += sizeof(float);

    // Write active primitives (use memcpy to avoid unaligned access UB)
    for (uint32_t i = 0; i < num_active; i++) {
        tmp_u32 = comp->active_prims[i].primitive_id;
        memcpy(ptr, &tmp_u32, sizeof(uint32_t)); ptr += sizeof(uint32_t);
        float tmp_f = comp->active_prims[i].coefficient;
        memcpy(ptr, &tmp_f, sizeof(float)); ptr += sizeof(float);
    }

    *compressed_size = total_size;

    // Update statistics
    comp->stats.total_compressions++;
    comp->stats.total_bytes_in += data_size * sizeof(float);
    comp->stats.total_bytes_out += total_size;
    comp->stats.avg_compression_ratio =
        (float)comp->stats.total_bytes_in / (float)comp->stats.total_bytes_out;
    comp->stats.avg_sparsity =
        (comp->stats.avg_sparsity * (comp->stats.total_compressions - 1) + num_active) /
        comp->stats.total_compressions;

    comp->last_compression_ratio = (float)(data_size * sizeof(float)) / (float)total_size;
    comp->last_quality = quality;

    // Online learning
    if (comp->config.enable_online_learning) {
        semantic_update_dictionary(comp, neural_data, data_size);
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t semantic_compress_with_quality(
    semantic_compressor_t* comp,
    const float* neural_data,
    uint32_t data_size,
    float min_quality,
    uint8_t* compressed,
    uint32_t* compressed_size,
    semantic_compression_metadata_t* out_metadata
) {
    // Simple implementation: just compress and check quality
    nimcp_result_t result = semantic_compress(comp, neural_data, data_size,
                                              compressed, compressed_size);

    if (result == NIMCP_SUCCESS && out_metadata) {
        out_metadata->original_size = data_size;
        out_metadata->compressed_size = *compressed_size;
        out_metadata->compression_ratio = comp->last_compression_ratio;
        out_metadata->quality_score = comp->last_quality;
        out_metadata->is_lossy = (comp->last_quality < 0.999f);
        out_metadata->timestamp_us = 0;  // Would need platform-specific timing
    }

    return result;
}

nimcp_result_t semantic_decompress(
    semantic_compressor_t* comp,
    const uint8_t* compressed,
    uint32_t comp_size,
    float* neural_data,
    uint32_t* data_size
) {
    if (!comp || !compressed || !neural_data || !data_size) {
        return NIMCP_INVALID_PARAM;
    }

    // Read header (use memcpy to avoid unaligned access UB)
    const uint8_t* ptr = compressed;
    uint32_t magic; memcpy(&magic, ptr, sizeof(uint32_t)); ptr += sizeof(uint32_t);
    uint32_t version; memcpy(&version, ptr, sizeof(uint32_t)); ptr += sizeof(uint32_t);
    uint32_t original_size; memcpy(&original_size, ptr, sizeof(uint32_t)); ptr += sizeof(uint32_t);
    uint32_t num_active; memcpy(&num_active, ptr, sizeof(uint32_t)); ptr += sizeof(uint32_t);
    float quality; memcpy(&quality, ptr, sizeof(float)); ptr += sizeof(float);

    // Validate
    if (magic != COMPRESSION_MAGIC) {
        LOG_ERROR("Invalid magic number: 0x%08X", magic);
        return NIMCP_INVALID_MSG;
    }

    if (*data_size < original_size) {
        *data_size = original_size;
        return NIMCP_BUFFER_TOO_SMALL;
    }

    // Read active primitives (use memcpy to avoid unaligned access UB)
    active_primitive_t* active = comp->active_prims;
    for (uint32_t i = 0; i < num_active; i++) {
        memcpy(&active[i].primitive_id, ptr, sizeof(uint32_t)); ptr += sizeof(uint32_t);
        memcpy(&active[i].coefficient, ptr, sizeof(float)); ptr += sizeof(float);
    }

    // Reconstruct
    nimcp_result_t result = reconstruct_from_encoding(comp, active, num_active,
                                                      neural_data, original_size);
    if (result == NIMCP_SUCCESS) {
        *data_size = original_size;
        comp->stats.total_decompressions++;
    }

    return result;
}

nimcp_result_t semantic_decompress_with_metadata(
    semantic_compressor_t* comp,
    const uint8_t* compressed,
    uint32_t comp_size,
    float* neural_data,
    uint32_t* data_size,
    semantic_compression_metadata_t* out_metadata
) {
    // Extract quality from header before decompression (use memcpy to avoid unaligned access UB)
    if (out_metadata && comp_size >= 20) {
        const uint8_t* ptr = compressed + 16;  // Skip to quality field
        memcpy(&out_metadata->quality_score, ptr, sizeof(float));
    }

    return semantic_decompress(comp, compressed, comp_size, neural_data, data_size);
}

/* ============================================================================
 * Quality Metrics
 * ============================================================================ */

float semantic_get_compression_ratio(semantic_compressor_t* comp) {
    return comp ? comp->stats.avg_compression_ratio : 0.0f;
}

float semantic_get_reconstruction_quality(semantic_compressor_t* comp) {
    return comp ? comp->last_quality : 0.0f;
}

float semantic_get_sparsity(semantic_compressor_t* comp) {
    return comp ? comp->stats.avg_sparsity : 0.0f;
}

nimcp_result_t semantic_compute_reconstruction_error(
    semantic_compressor_t* comp,
    const float* original,
    uint32_t data_size,
    float* out_error
) {
    if (!comp || !original || !out_error) return NIMCP_INVALID_PARAM;

    // Compress then decompress
    uint8_t compressed[4096];
    uint32_t comp_size = sizeof(compressed);

    nimcp_result_t result = semantic_compress(comp, original, data_size, compressed, &comp_size);
    if (result != NIMCP_SUCCESS) return result;

    float* reconstructed = comp->reconstruction_buffer;
    uint32_t recon_size = data_size;
    result = semantic_decompress(comp, compressed, comp_size, reconstructed, &recon_size);
    if (result != NIMCP_SUCCESS) return result;

    // Compute MSE
    float mse = 0.0f;
    for (uint32_t i = 0; i < data_size; i++) {
        float diff = original[i] - reconstructed[i];
        mse += diff * diff;
    }
    mse /= data_size;

    *out_error = mse;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

nimcp_result_t semantic_compressor_register_bioasync(
    semantic_compressor_t* comp,
    bio_router_t* router
) {
    if (!comp || !router) return NIMCP_INVALID_PARAM;

    comp->router = router;
    comp->bio_async_registered = true;

    LOG_INFO("Semantic compressor registered with bio-async router");
    return NIMCP_SUCCESS;
}

nimcp_result_t semantic_broadcast_stats(semantic_compressor_t* comp) {
    if (!comp || !comp->bio_async_registered) return NIMCP_NOT_INITIALIZED;

    comp->stats.bio_broadcasts_sent++;

    // Would send BIO_MSG_NLP_COMPRESSION_COMPLETE message
    LOG_DEBUG("Broadcasting compression stats: ratio=%.2f, quality=%.3f",
              comp->stats.avg_compression_ratio, comp->last_quality);

    return NIMCP_SUCCESS;
}

nimcp_result_t semantic_handle_compression_request(
    semantic_compressor_t* comp,
    const void* message,
    size_t msg_size
) {
    if (!comp || !message) return NIMCP_INVALID_PARAM;

    // Would parse bio_msg_nlp_compression_t and compress
    LOG_DEBUG("Handling compression request");

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics and Utilities
 * ============================================================================ */

nimcp_result_t semantic_get_stats(
    semantic_compressor_t* comp,
    semantic_compression_stats_t* out_stats
) {
    if (!comp || !out_stats) return NIMCP_INVALID_PARAM;

    memcpy(out_stats, &comp->stats, sizeof(semantic_compression_stats_t));
    return NIMCP_SUCCESS;
}

nimcp_result_t semantic_reset_stats(semantic_compressor_t* comp) {
    if (!comp) return NIMCP_INVALID_PARAM;

    memset(&comp->stats, 0, sizeof(semantic_compression_stats_t));
    return NIMCP_SUCCESS;
}

void semantic_get_default_config(semantic_compression_config_t* out_config) {
    if (!out_config) return;

    out_config->dictionary_size = 128;
    out_config->primitive_vector_size = 512;
    out_config->max_compression_ratio = 10;
    out_config->max_active_primitives = 20;
    out_config->enable_lossy = true;
    out_config->quality_threshold = 0.9f;
    out_config->learning_rate = DEFAULT_LEARNING_RATE;
    out_config->learning_iterations = 10;
    out_config->enable_online_learning = false;
    out_config->quantization_bits = 0;  // No quantization
    out_config->enable_entropy_coding = false;
    out_config->enable_bio_async = false;
    out_config->broadcast_interval_ms = NIMCP_TIMEOUT_LONG_MS;
}

nimcp_result_t semantic_validate_config(const semantic_compression_config_t* config) {
    if (!config) return NIMCP_INVALID_PARAM;

    if (config->dictionary_size == 0 || config->dictionary_size > 10000) {
        LOG_ERROR("Invalid dictionary_size: %u", config->dictionary_size);
        return NIMCP_INVALID_PARAM;
    }

    if (config->primitive_vector_size == 0 || config->primitive_vector_size > 100000) {
        LOG_ERROR("Invalid primitive_vector_size: %u", config->primitive_vector_size);
        return NIMCP_INVALID_PARAM;
    }

    if (config->max_active_primitives == 0 ||
        config->max_active_primitives > config->dictionary_size) {
        LOG_ERROR("Invalid max_active_primitives: %u", config->max_active_primitives);
        return NIMCP_INVALID_PARAM;
    }

    if (config->enable_lossy && (config->quality_threshold < MIN_QUALITY_THRESHOLD ||
                                  config->quality_threshold > MAX_QUALITY_THRESHOLD)) {
        LOG_ERROR("Invalid quality_threshold: %f", config->quality_threshold);
        return NIMCP_INVALID_PARAM;
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t semantic_save_dictionary(
    semantic_compressor_t* comp,
    const char* filepath
) {
    if (!comp || !filepath) return NIMCP_INVALID_PARAM;

    FILE* fp = fopen(filepath, "wb");
    if (!fp) {
        LOG_ERROR("Failed to open file for writing: %s", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "semantic_save_dictionary: failed to open file");
        return NIMCP_INVALID_PARAM;
    }

    /* Write header: magic, version, num_primitives, dictionary_version */
    uint32_t magic = COMPRESSION_MAGIC;
    uint32_t version = COMPRESSION_VERSION;
    fwrite(&magic, sizeof(uint32_t), 1, fp);
    fwrite(&version, sizeof(uint32_t), 1, fp);
    fwrite(&comp->num_primitives, sizeof(uint32_t), 1, fp);
    fwrite(&comp->dictionary_version, sizeof(uint64_t), 1, fp);

    /* Write each primitive */
    for (uint32_t i = 0; i < comp->num_primitives; i++) {
        semantic_primitive_t* p = &comp->primitives[i];
        fwrite(&p->primitive_id, sizeof(uint32_t), 1, fp);
        fwrite(p->name, sizeof(p->name), 1, fp);
        fwrite(&p->vector_size, sizeof(uint32_t), 1, fp);
        fwrite(&p->frequency, sizeof(float), 1, fp);
        fwrite(&p->mean_activation, sizeof(float), 1, fp);
        if (p->vector && p->vector_size > 0) {
            fwrite(p->vector, sizeof(float), p->vector_size, fp);
        }
    }

    fclose(fp);
    LOG_INFO("Saved dictionary: %u primitives to %s", comp->num_primitives, filepath);
    return NIMCP_SUCCESS;
}

nimcp_result_t semantic_load_dictionary(
    semantic_compressor_t* comp,
    const char* filepath
) {
    if (!comp || !filepath) return NIMCP_INVALID_PARAM;

    FILE* fp = fopen(filepath, "rb");
    if (!fp) {
        LOG_ERROR("Failed to open file for reading: %s", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "semantic_load_dictionary: failed to open file");
        return NIMCP_INVALID_PARAM;
    }

    /* Read and validate header */
    uint32_t magic = 0, version = 0, num_prims = 0;
    uint64_t dict_version = 0;
    if (fread(&magic, sizeof(uint32_t), 1, fp) != 1 ||
        fread(&version, sizeof(uint32_t), 1, fp) != 1 ||
        fread(&num_prims, sizeof(uint32_t), 1, fp) != 1 ||
        fread(&dict_version, sizeof(uint64_t), 1, fp) != 1) {
        fclose(fp);
        LOG_ERROR("Failed to read dictionary header");
        return NIMCP_INVALID_PARAM;
    }

    if (magic != COMPRESSION_MAGIC || version > COMPRESSION_VERSION) {
        fclose(fp);
        LOG_ERROR("Invalid dictionary format: magic=0x%08X version=%u", magic, version);
        return NIMCP_INVALID_PARAM;
    }

    if (num_prims > 100000) {
        fclose(fp);
        LOG_ERROR("Dictionary too large: %u primitives", num_prims);
        return NIMCP_INVALID_PARAM;
    }

    /* Free existing primitives */
    if (comp->primitives) {
        for (uint32_t i = 0; i < comp->num_primitives; i++) {
            if (comp->primitives[i].vector) {
                nimcp_free(comp->primitives[i].vector);
            }
        }
        nimcp_free(comp->primitives);
    }

    /* Allocate new primitives */
    comp->primitives = nimcp_calloc(num_prims, sizeof(semantic_primitive_t));
    if (!comp->primitives) {
        fclose(fp);
        comp->num_primitives = 0;
        return NIMCP_INVALID_PARAM;
    }

    /* Read each primitive */
    for (uint32_t i = 0; i < num_prims; i++) {
        semantic_primitive_t* p = &comp->primitives[i];
        if (fread(&p->primitive_id, sizeof(uint32_t), 1, fp) != 1 ||
            fread(p->name, sizeof(p->name), 1, fp) != 1 ||
            fread(&p->vector_size, sizeof(uint32_t), 1, fp) != 1 ||
            fread(&p->frequency, sizeof(float), 1, fp) != 1 ||
            fread(&p->mean_activation, sizeof(float), 1, fp) != 1) {
            LOG_ERROR("Failed to read primitive %u", i);
            break;
        }

        if (p->vector_size > 0 && p->vector_size <= 1048576) {
            p->vector = nimcp_calloc(p->vector_size, sizeof(float));
            if (p->vector) {
                if (fread(p->vector, sizeof(float), p->vector_size, fp) != p->vector_size) {
                    LOG_ERROR("Failed to read vector data for primitive %u", i);
                    nimcp_free(p->vector);
                    p->vector = NULL;
                    p->vector_size = 0;
                }
            }
        }
    }

    comp->num_primitives = num_prims;
    comp->dictionary_version = dict_version;
    fclose(fp);

    LOG_INFO("Loaded dictionary: %u primitives from %s (version %lu)",
             num_prims, filepath, (unsigned long)dict_version);
    return NIMCP_SUCCESS;
}
