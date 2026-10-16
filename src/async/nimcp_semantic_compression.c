/**
 * @file nimcp_semantic_compression.c
 * @brief Implementation of semantic compression for neural signals
 *
 * This module implements biologically-inspired compression that preserves
 * semantic meaning rather than exact values. Uses vector quantization,
 * delta coding, and adaptive primitive learning.
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include "async/nimcp_semantic_compression.h"
#include "api/nimcp_api_exception.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <float.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(semantic_compression)

/* ============================================================================
 * Constants and Macros
 * ============================================================================ */

#define COMPRESSION_MODULE "SemanticCompression"
#define HASH_TABLE_SIZE 1024
#include "constants/nimcp_constants.h"
#define EPSILON NIMCP_EPSILON_NUMERICAL
#define DEFAULT_LEARNING_RATE NIMCP_LEARNING_RATE_DEFAULT

/* Bio-async message types */
#define BIOMSG_COMPRESS_REQUEST 0x6000
#define BIOMSG_COMPRESS_RESPONSE 0x6001
#define BIOMSG_DECOMPRESS_REQUEST 0x6002
#define BIOMSG_DECOMPRESS_RESPONSE 0x6003

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Primitive dictionary entry
 */
typedef struct primitive_entry_t {
    nimcp_semantic_primitive_t primitive;
    struct primitive_entry_t* next;
} primitive_entry_t;

/**
 * @brief Semantic compressor state
 */
struct nimcp_semantic_compressor_struct {
    nimcp_compression_config_t config;

    /* Primitive dictionary (hash table) */
    primitive_entry_t** primitive_hash;
    size_t hash_size;
    uint32_t next_primitive_id;
    uint32_t active_primitive_count;

    /* Statistics */
    nimcp_compression_stats_t stats;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_registered;

    /* Synchronization */
    nimcp_platform_mutex_t mutex;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Compute hash of vector for dictionary lookup
 * WHY:  Fast primitive matching
 * HOW:  Simple hash of vector components
 */
static uint32_t hash_vector(const float* vector, uint32_t dim) {
    if (!vector) return 0;

    uint32_t hash = 0;
    for (uint32_t i = 0; i < dim; i++) {
        /* Simple but effective hash */
        union {
            float f;
            uint32_t u;
        } converter;
        converter.f = vector[i];
        hash = hash * 31 + converter.u;
    }
    return hash;
}

/**
 * WHAT: Compute Euclidean distance between vectors
 * WHY:  Measure similarity for primitive matching
 * HOW:  Standard L2 distance calculation
 */
static float vector_distance(const float* v1, const float* v2, uint32_t dim) {
    if (!v1 || !v2) return FLT_MAX;

    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float diff = v1[i] - v2[i];
        sum += diff * diff;
    }
    return sqrtf(sum);
}

/**
 * WHAT: Compute cosine similarity between vectors
 * WHY:  Better measure of semantic similarity
 * HOW:  Dot product divided by magnitudes
 */
static float vector_cosine_similarity(const float* v1, const float* v2, uint32_t dim) {
    if (!v1 || !v2) return 0.0f;

    float dot = 0.0f, mag1 = 0.0f, mag2 = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        dot += v1[i] * v2[i];
        mag1 += v1[i] * v1[i];
        mag2 += v2[i] * v2[i];
    }

    float denom = sqrtf(mag1) * sqrtf(mag2);
    if (denom < EPSILON) return 0.0f;

    return dot / denom;
}

/**
 * WHAT: Find best matching primitive for signal segment
 * WHY:  Core of semantic compression
 * HOW:  Search dictionary for nearest neighbor
 */
static primitive_entry_t* find_best_primitive(
    nimcp_semantic_compressor_t* compressor,
    const float* segment,
    uint32_t segment_len,
    float* out_distance) {

    if (!compressor || !segment) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_best_primitive: required parameter is NULL (compressor, segment)");
        return NULL;
    }

    primitive_entry_t* best = NULL;
    float best_dist = FLT_MAX;

    /* Search all primitives */
    for (size_t i = 0; i < compressor->hash_size; i++) {
        primitive_entry_t* entry = compressor->primitive_hash[i];
        while (entry) {
            /* Match segment length to primitive dimension */
            if (entry->primitive.vector_dim == segment_len) {
                float dist = vector_distance(
                    segment,
                    entry->primitive.meaning_vector,
                    segment_len);

                if (dist < best_dist) {
                    best_dist = dist;
                    best = entry;
                }
            }
            entry = entry->next;
        }
    }

    if (out_distance) {
        *out_distance = best_dist;
    }

    return best;
}

/**
 * WHAT: Extract signal segment at offset
 * WHY:  Prepare segment for primitive matching
 * HOW:  Copy window of signal into temporary buffer
 */
static float* extract_segment(
    const float* signal,
    size_t signal_len,
    size_t offset,
    size_t segment_len) {

    /* P3-62 fix: Check for size_t addition overflow before comparison */
    if (!signal || segment_len > signal_len || offset > signal_len - segment_len) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "extract_segment: signal is NULL or out of bounds");
        return NULL;
    }

    float* segment = (float*)nimcp_malloc(segment_len * sizeof(float));
    if (!segment) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "segment is NULL");

        return NULL;

    }

    memcpy(segment, signal + offset, segment_len * sizeof(float));
    return segment;
}

/**
 * WHAT: Compute delta values for temporal compression
 * WHY:  Exploit temporal correlation in signals
 * HOW:  Store first-order differences
 */
static float* compute_deltas(const float* signal, size_t len, size_t* out_delta_count) {
    if (!signal) {
        if (out_delta_count) *out_delta_count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "compute_deltas: signal is NULL");
        return NULL;
    }
    // P2 fix: len < 2 is normal (first sample has no delta). Not an error.
    if (len < 2) {
        if (out_delta_count) *out_delta_count = 0;
        return NULL;
    }

    size_t delta_count = len - 1;
    float* deltas = (float*)nimcp_malloc(delta_count * sizeof(float));
    if (!deltas) {
        if (out_delta_count) *out_delta_count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "compute_deltas: validation failed");
        return NULL;
    }

    for (size_t i = 0; i < delta_count; i++) {
        deltas[i] = signal[i + 1] - signal[i];
    }

    if (out_delta_count) *out_delta_count = delta_count;
    return deltas;
}

/**
 * WHAT: Apply deltas to reconstruct signal
 * WHY:  Reverse delta coding during decompression
 * HOW:  Integrate deltas starting from initial value
 */
static void apply_deltas(float* signal, size_t len, const float* deltas, size_t delta_count) {
    if (!signal || !deltas || len < 2) return;

    size_t max_apply = (delta_count < len - 1) ? delta_count : (len - 1);
    for (size_t i = 0; i < max_apply; i++) {
        signal[i + 1] = signal[i] + deltas[i];
    }
}

/**
 * WHAT: Calculate reconstruction error (MSE)
 * WHY:  Measure compression quality
 * HOW:  Mean squared error between original and reconstructed
 */
static float calculate_reconstruction_error(
    const float* original,
    const float* reconstructed,
    size_t len) {

    if (!original || !reconstructed || len == 0) return FLT_MAX;

    float sum_squared_error = 0.0f;
    for (size_t i = 0; i < len; i++) {
        float error = original[i] - reconstructed[i];
        sum_squared_error += error * error;
    }

    return sum_squared_error / (float)len;
}

/* ============================================================================
 * Internal API Implementation (lock-free variants)
 * ============================================================================ */

/**
 * @brief Internal helper to add primitive without acquiring lock
 * WHAT: Add primitive to dictionary without locking
 * WHY:  Avoid recursive lock when called from compress (which already holds lock)
 * HOW:  Same logic as nimcp_semantic_compressor_add_primitive but expects caller holds mutex
 *
 * @param compressor The compressor (caller must hold mutex)
 * @param meaning_vector Vector data
 * @param vector_dim Dimension of vector
 * @return Primitive ID or 0 on failure
 */
static uint32_t add_primitive_unlocked(
    nimcp_semantic_compressor_t* compressor,
    const float* meaning_vector,
    uint32_t vector_dim) {

    /* Validate inputs */
    if (!compressor || !meaning_vector) {
        LOG_ERROR(COMPRESSION_MODULE, "NULL parameter in add_primitive_unlocked");
        return 0;
    }

    if (vector_dim > compressor->config.vector_dimension) {
        LOG_ERROR(COMPRESSION_MODULE, "Vector dimension mismatch: %u > %u",
                  vector_dim, compressor->config.vector_dimension);
        return 0;
    }

    /* Check primitive limit */
    if (compressor->active_primitive_count >= compressor->config.max_primitives) {
        LOG_WARN(COMPRESSION_MODULE, "Primitive limit reached: %u",
                 compressor->config.max_primitives);
        return 0;
    }

    /* Allocate new primitive entry */
    primitive_entry_t* entry = (primitive_entry_t*)nimcp_malloc(sizeof(primitive_entry_t));
    if (!entry) {
        LOG_ERROR(COMPRESSION_MODULE, "Failed to allocate primitive entry");
        return 0;
    }

    memset(entry, 0, sizeof(primitive_entry_t));

    /* Copy meaning vector */
    entry->primitive.meaning_vector = (float*)nimcp_malloc(vector_dim * sizeof(float));
    if (!entry->primitive.meaning_vector) {
        LOG_ERROR(COMPRESSION_MODULE, "Failed to allocate meaning vector");
        nimcp_free(entry);
        return 0;
    }

    memcpy(entry->primitive.meaning_vector, meaning_vector, vector_dim * sizeof(float));

    /* Initialize primitive */
    entry->primitive.primitive_id = compressor->next_primitive_id++;
    entry->primitive.vector_dim = vector_dim;
    entry->primitive.confidence = 1.0f;
    entry->primitive.usage_count = 0;
    entry->primitive.last_used = nimcp_time_get_current_time_ns();

    /* Add to hash table */
    uint32_t hash_idx = hash_vector(meaning_vector, vector_dim) % compressor->hash_size;
    entry->next = compressor->primitive_hash[hash_idx];
    compressor->primitive_hash[hash_idx] = entry;

    compressor->active_primitive_count++;
    compressor->stats.total_primitives_created++;

    uint32_t primitive_id = entry->primitive.primitive_id;

    LOG_DEBUG(COMPRESSION_MODULE, "Added primitive %u (dim=%u)", primitive_id, vector_dim);

    return primitive_id;
}

/* ============================================================================
 * Core API Implementation
 * ============================================================================ */

nimcp_compression_config_t nimcp_semantic_compressor_default_config(void) {
    nimcp_compression_config_t config = {
        .max_primitives = 256,
        .vector_dimension = 64,
        .quality_level = 0.95f,
        .enable_delta = true,
        .enable_residuals = true,
        .primitive_learning_rate = DEFAULT_LEARNING_RATE,
        .min_primitive_usage = 5,
        .bio_async_enabled = false,
        .bio_channel = BIO_CHANNEL_DOPAMINE
    };
    return config;
}

nimcp_semantic_compressor_t* nimcp_semantic_compressor_create(
    const nimcp_compression_config_t* config) {

    /* Validate input */
    if (!config) {
        LOG_ERROR(COMPRESSION_MODULE, "NULL configuration provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_semantic_compressor_create: config is NULL");
        return NULL;
    }

    if (config->max_primitives > SEMANTIC_MAX_PRIMITIVES) {
        LOG_ERROR(COMPRESSION_MODULE, "max_primitives exceeds limit: %u > %u",
                  config->max_primitives, SEMANTIC_MAX_PRIMITIVES);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_semantic_compressor_create: max_primitives exceeds limit");
        return NULL;
    }

    if (config->vector_dimension > SEMANTIC_MAX_VECTOR_DIM) {
        LOG_ERROR(COMPRESSION_MODULE, "vector_dimension exceeds limit: %u > %u",
                  config->vector_dimension, SEMANTIC_MAX_VECTOR_DIM);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_semantic_compressor_create: vector_dimension exceeds limit");
        return NULL;
    }

    if (config->quality_level < SEMANTIC_MIN_QUALITY ||
        config->quality_level > SEMANTIC_MAX_QUALITY) {
        LOG_ERROR(COMPRESSION_MODULE, "quality_level out of range: %.2f",
                  config->quality_level);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_semantic_compressor_create: operation failed");
        return NULL;
    }

    /* Allocate compressor - memset zeroes all pointers for safe cleanup */
    bool mutex_initialized = false;
    nimcp_semantic_compressor_t* compressor =
        (nimcp_semantic_compressor_t*)nimcp_malloc(sizeof(nimcp_semantic_compressor_t));
    if (!compressor) {
        LOG_ERROR(COMPRESSION_MODULE, "Failed to allocate compressor");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_semantic_compressor_create: compressor is NULL");
        return NULL;
    }

    memset(compressor, 0, sizeof(nimcp_semantic_compressor_t));

    /* Copy configuration */
    compressor->config = *config;

    /* Initialize primitive hash table */
    compressor->hash_size = HASH_TABLE_SIZE;
    compressor->primitive_hash = (primitive_entry_t**)nimcp_malloc(
        compressor->hash_size * sizeof(primitive_entry_t*));
    if (!compressor->primitive_hash) goto cleanup;

    memset(compressor->primitive_hash, 0, compressor->hash_size * sizeof(primitive_entry_t*));

    compressor->next_primitive_id = 1;
    compressor->active_primitive_count = 0;

    /* Initialize mutex (non-recursive) */
    if (nimcp_platform_mutex_init(&compressor->mutex, false) != 0) goto cleanup;
    mutex_initialized = true;

    /* Initialize statistics */
    memset(&compressor->stats, 0, sizeof(nimcp_compression_stats_t));

    /* Register with bio-async if enabled */
    if (config->bio_async_enabled) {
        if (nimcp_semantic_compressor_register_bio_async(compressor) != NIMCP_SUCCESS) {
            LOG_WARN(COMPRESSION_MODULE, "Failed to register with bio-async");
        }
    }

    LOG_INFO(COMPRESSION_MODULE,
             "Created compressor: max_primitives=%u, vector_dim=%u, quality=%.2f",
             config->max_primitives, config->vector_dimension, config->quality_level);

    return compressor;

cleanup:
    LOG_ERROR(COMPRESSION_MODULE, "Failed to allocate compressor resources");
    if (mutex_initialized) {
        nimcp_platform_mutex_destroy(&compressor->mutex);
    }
    nimcp_free(compressor->primitive_hash);
    nimcp_free(compressor);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_semantic_compressor_create: allocation failed");
    return NULL;
}

void nimcp_semantic_compressor_destroy(nimcp_semantic_compressor_t* compressor) {
    if (!compressor) return;

    LOG_DEBUG(COMPRESSION_MODULE, "Destroying compressor");

    /* Free all primitives */
    if (compressor->primitive_hash) {
        for (size_t i = 0; i < compressor->hash_size; i++) {
            primitive_entry_t* entry = compressor->primitive_hash[i];
            while (entry) {
                primitive_entry_t* next = entry->next;
                if (entry->primitive.meaning_vector) {
                    nimcp_free(entry->primitive.meaning_vector);
                }
                nimcp_free(entry);
                entry = next;
            }
        }
        nimcp_free(compressor->primitive_hash);
    }

    /* Destroy mutex */
    nimcp_platform_mutex_destroy(&compressor->mutex);

    /* Free compressor */
    nimcp_free(compressor);
}

uint32_t nimcp_semantic_compressor_add_primitive(
    nimcp_semantic_compressor_t* compressor,
    const float* meaning_vector,
    uint32_t vector_dim) {

    /* Validate inputs (before locking) */
    if (!compressor || !meaning_vector) {
        LOG_ERROR(COMPRESSION_MODULE, "NULL parameter in add_primitive");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_semantic_compressor_add_primitive: NULL parameter (compressor or meaning_vector)");
        return 0;
    }

    nimcp_platform_mutex_lock(&compressor->mutex);
    uint32_t primitive_id = add_primitive_unlocked(compressor, meaning_vector, vector_dim);
    nimcp_platform_mutex_unlock(&compressor->mutex);

    return primitive_id;
}

nimcp_compressed_signal_t* nimcp_semantic_compressor_compress(
    nimcp_semantic_compressor_t* compressor,
    const float* signal,
    size_t len) {

    /* Validate inputs */
    if (!compressor || !signal || len == 0) {
        LOG_ERROR(COMPRESSION_MODULE, "Invalid parameters for compress");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_semantic_compressor_compress: required parameter is NULL (compressor, signal)");
        return NULL;
    }

    nimcp_platform_mutex_lock(&compressor->mutex);

    LOG_DEBUG(COMPRESSION_MODULE, "Compressing signal of length %zu", len);

    /* Allocate compressed signal */
    nimcp_compressed_signal_t* compressed =
        (nimcp_compressed_signal_t*)nimcp_malloc(sizeof(nimcp_compressed_signal_t));
    if (!compressed) {
        LOG_ERROR(COMPRESSION_MODULE, "Failed to allocate compressed signal");
        nimcp_platform_mutex_unlock(&compressor->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_semantic_compressor_compress: compressed is NULL");
        return NULL;
    }

    memset(compressed, 0, sizeof(nimcp_compressed_signal_t));

    /* Estimate number of primitives needed (one per vector_dim samples) */
    uint32_t segment_size = compressor->config.vector_dimension;
    uint32_t max_primitives = (uint32_t)((len + segment_size - 1) / segment_size);

    /* Allocate primitive ID array */
    compressed->primitive_ids = (uint32_t*)nimcp_malloc(max_primitives * sizeof(uint32_t));
    if (!compressed->primitive_ids) {
        LOG_ERROR(COMPRESSION_MODULE, "Failed to allocate primitive IDs");
        nimcp_free(compressed);
        nimcp_platform_mutex_unlock(&compressor->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_semantic_compressor_compress: compressed->primitive_ids is NULL");
        return NULL;
    }

    compressed->num_primitives = 0;

    /* Compress signal using primitives */
    size_t offset = 0;
    while (offset < len) {
        size_t remaining = len - offset;
        size_t seg_len = (remaining < segment_size) ? remaining : segment_size;

        float* segment = extract_segment(signal, len, offset, seg_len);
        if (!segment) break;

        /* Find or create primitive for this segment */
        float distance;
        primitive_entry_t* best = find_best_primitive(compressor, segment, seg_len, &distance);

        if (best) {
            /* Use existing primitive - bounds check before storing */
            if (compressed->num_primitives < max_primitives) {
                compressed->primitive_ids[compressed->num_primitives++] = best->primitive.primitive_id;
                best->primitive.usage_count++;
                best->primitive.last_used = nimcp_time_get_current_time_ns();
            }
        } else {
            /* Create new primitive if space available
             * Use unlocked version since we already hold the mutex */
            uint32_t new_id = add_primitive_unlocked(
                compressor, segment, (uint32_t)seg_len);
            if (new_id > 0 && compressed->num_primitives < max_primitives) {
                compressed->primitive_ids[compressed->num_primitives++] = new_id;
            }
        }

        nimcp_free(segment);
        offset += seg_len;
    }

    /* Apply delta coding if enabled */
    if (compressor->config.enable_delta) {
        compressed->deltas = compute_deltas(signal, len, &compressed->num_deltas);
    }

    /* Calculate sizes */
    compressed->original_size = len * sizeof(float);
    compressed->compressed_size = compressed->num_primitives * sizeof(uint32_t) +
                                   compressed->num_deltas * sizeof(float);

    compressed->timestamp = nimcp_time_get_current_time_ns();

    /* Update statistics */
    compressor->stats.total_compressions++;
    compressor->stats.total_bytes_in += compressed->original_size;
    compressor->stats.total_bytes_out += compressed->compressed_size;

    if (compressed->original_size > 0) {
        double ratio = (double)compressed->original_size / (double)compressed->compressed_size;
        /* Use double precision for intermediate calculation to avoid overflow
         * with rolling average when total_compressions is large */
        double prev_avg = (double)compressor->stats.avg_compression_ratio;
        double n = (double)compressor->stats.total_compressions;
        double new_avg = (prev_avg * (n - 1.0) + ratio) / n;
        compressor->stats.avg_compression_ratio = (float)new_avg;
    }

    compressor->stats.active_primitives = compressor->active_primitive_count;

    nimcp_platform_mutex_unlock(&compressor->mutex);

    LOG_INFO(COMPRESSION_MODULE,
             "Compressed %zu bytes -> %zu bytes (ratio: %.2fx)",
             compressed->original_size,
             compressed->compressed_size,
             (float)compressed->original_size / (float)compressed->compressed_size);

    return compressed;
}

nimcp_decompressed_signal_t* nimcp_semantic_compressor_decompress(
    nimcp_semantic_compressor_t* compressor,
    const nimcp_compressed_signal_t* compressed) {

    /* Validate inputs */
    if (!compressor || !compressed) {
        LOG_ERROR(COMPRESSION_MODULE, "Invalid parameters for decompress");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_semantic_compressor_decompress: required parameter is NULL (compressor, compressed)");
        return NULL;
    }

    nimcp_platform_mutex_lock(&compressor->mutex);

    LOG_DEBUG(COMPRESSION_MODULE, "Decompressing signal with %u primitives",
              compressed->num_primitives);

    /* Allocate decompressed signal */
    nimcp_decompressed_signal_t* decompressed =
        (nimcp_decompressed_signal_t*)nimcp_malloc(sizeof(nimcp_decompressed_signal_t));
    if (!decompressed) {
        LOG_ERROR(COMPRESSION_MODULE, "Failed to allocate decompressed signal");
        nimcp_platform_mutex_unlock(&compressor->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_semantic_compressor_decompress: decompressed is NULL");
        return NULL;
    }

    memset(decompressed, 0, sizeof(nimcp_decompressed_signal_t));

    /* Estimate signal length */
    size_t estimated_len = compressed->num_primitives * compressor->config.vector_dimension;

    /* Allocate signal array */
    decompressed->signal = (float*)nimcp_malloc(estimated_len * sizeof(float));
    if (!decompressed->signal) {
        LOG_ERROR(COMPRESSION_MODULE, "Failed to allocate signal array");
        nimcp_free(decompressed);
        nimcp_platform_mutex_unlock(&compressor->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_semantic_compressor_decompress: decompressed->signal is NULL");
        return NULL;
    }

    decompressed->len = 0;

    /* Guard: check primitive_ids array before accessing */
    if (!compressed->primitive_ids && compressed->num_primitives > 0) {
        LOG_ERROR(COMPRESSION_MODULE, "Corrupted compressed signal: NULL primitive_ids");
        nimcp_free(decompressed->signal);
        nimcp_free(decompressed);
        nimcp_platform_mutex_unlock(&compressor->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_semantic_compressor_decompress: compressed->primitive_ids is NULL");
        return NULL;
    }

    /* Reconstruct signal from primitives */
    for (uint32_t i = 0; i < compressed->num_primitives; i++) {
        uint32_t primitive_id = compressed->primitive_ids[i];

        /* Find primitive in hash table */
        primitive_entry_t* found = NULL;
        for (size_t h = 0; h < compressor->hash_size && !found; h++) {
            primitive_entry_t* entry = compressor->primitive_hash[h];
            while (entry) {
                if (entry->primitive.primitive_id == primitive_id) {
                    found = entry;
                    break;
                }
                entry = entry->next;
            }
        }

        if (found) {
            /* Copy primitive vector to signal */
            size_t copy_len = found->primitive.vector_dim;
            if (decompressed->len + copy_len <= estimated_len) {
                memcpy(decompressed->signal + decompressed->len,
                       found->primitive.meaning_vector,
                       copy_len * sizeof(float));
                decompressed->len += copy_len;
            }
        } else {
            LOG_WARN(COMPRESSION_MODULE, "Primitive %u not found", primitive_id);
        }
    }

    /* Apply deltas if available */
    if (compressed->deltas && compressed->num_deltas > 0) {
        apply_deltas(decompressed->signal, decompressed->len,
                     compressed->deltas, compressed->num_deltas);
    }

    decompressed->timestamp = nimcp_time_get_current_time_ns();
    decompressed->semantic_loss = 0.0f;  /* Would need original signal to calculate */
    decompressed->reconstruction_error = 0.0f;

    /* Update statistics */
    compressor->stats.total_decompressions++;

    nimcp_platform_mutex_unlock(&compressor->mutex);

    LOG_INFO(COMPRESSION_MODULE, "Decompressed to %zu samples", decompressed->len);

    return decompressed;
}

float nimcp_semantic_compressor_get_ratio(const nimcp_semantic_compressor_t* compressor) {
    if (!compressor) return 0.0f;
    return compressor->stats.avg_compression_ratio;
}

nimcp_result_t nimcp_semantic_compressor_get_stats(
    const nimcp_semantic_compressor_t* compressor,
    nimcp_compression_stats_t* stats) {

    NIMCP_CHECK_THROW(compressor && stats, NIMCP_ERROR_NULL_POINTER,
                      "nimcp_semantic_compressor_get_stats: compressor or stats is NULL");

    /* P2-61 fix: Document intentional lock-free stats read.
     * We do NOT lock here because:
     * 1. The compressor parameter is const, and casting away const for mutex
     *    would be undefined behavior if the original object is truly const.
     * 2. Stats are only used for monitoring/display - slightly stale data
     *    is acceptable.
     * 3. The struct copy on aligned data is safe on x86/x64 (no torn reads
     *    for aligned 32/64-bit values). On ARM/RISC-V, individual fields may
     *    be slightly inconsistent but the overall stats are still useful.
     * Trade-off: Accepts potentially stale/inconsistent stats for monitoring
     * in exchange for avoiding UB and const-correctness. */
    *stats = compressor->stats;

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_semantic_compressor_reset_primitives(
    nimcp_semantic_compressor_t* compressor) {

    NIMCP_CHECK_THROW(compressor != NULL, NIMCP_ERROR_NULL_POINTER,
                      "nimcp_semantic_compressor_reset_primitives: compressor is NULL");

    nimcp_platform_mutex_lock(&compressor->mutex);

    LOG_INFO(COMPRESSION_MODULE, "Resetting primitive dictionary");

    /* Free all primitives */
    for (size_t i = 0; i < compressor->hash_size; i++) {
        primitive_entry_t* entry = compressor->primitive_hash[i];
        while (entry) {
            primitive_entry_t* next = entry->next;
            if (entry->primitive.meaning_vector) {
                nimcp_free(entry->primitive.meaning_vector);
            }
            nimcp_free(entry);
            entry = next;
        }
        compressor->primitive_hash[i] = NULL;
    }

    compressor->active_primitive_count = 0;
    compressor->next_primitive_id = 1;

    nimcp_platform_mutex_unlock(&compressor->mutex);

    return NIMCP_SUCCESS;
}

uint32_t nimcp_semantic_compressor_prune_primitives(
    nimcp_semantic_compressor_t* compressor,
    uint32_t min_usage) {

    if (!compressor) return 0;

    nimcp_platform_mutex_lock(&compressor->mutex);

    uint32_t pruned = 0;

    /* Prune primitives with low usage */
    for (size_t i = 0; i < compressor->hash_size; i++) {
        primitive_entry_t** entry_ptr = &compressor->primitive_hash[i];
        while (*entry_ptr) {
            primitive_entry_t* entry = *entry_ptr;
            if (entry->primitive.usage_count < min_usage) {
                /* Remove this entry */
                *entry_ptr = entry->next;
                if (entry->primitive.meaning_vector) {
                    nimcp_free(entry->primitive.meaning_vector);
                }
                nimcp_free(entry);
                pruned++;
                compressor->active_primitive_count--;
            } else {
                entry_ptr = &entry->next;
            }
        }
    }

    compressor->stats.primitives_pruned += pruned;

    nimcp_platform_mutex_unlock(&compressor->mutex);

    LOG_INFO(COMPRESSION_MODULE, "Pruned %u primitives with usage < %u", pruned, min_usage);

    return pruned;
}

void nimcp_compressed_signal_destroy(nimcp_compressed_signal_t* compressed) {
    if (!compressed) return;

    if (compressed->primitive_ids) {
        nimcp_free(compressed->primitive_ids);
    }
    if (compressed->deltas) {
        nimcp_free(compressed->deltas);
    }
    if (compressed->residuals) {
        nimcp_free(compressed->residuals);
    }
    nimcp_free(compressed);
}

void nimcp_decompressed_signal_destroy(nimcp_decompressed_signal_t* decompressed) {
    if (!decompressed) return;

    if (decompressed->signal) {
        nimcp_free(decompressed->signal);
    }
    nimcp_free(decompressed);
}

float nimcp_semantic_similarity(
    const float* signal1, size_t len1,
    const float* signal2, size_t len2) {

    if (!signal1 || !signal2 || len1 == 0 || len2 == 0) {
        return 0.0f;
    }

    /* Use shorter length */
    size_t min_len = (len1 < len2) ? len1 : len2;

    /* Compute cosine similarity */
    return vector_cosine_similarity(signal1, signal2, (uint32_t)min_len);
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

nimcp_result_t nimcp_semantic_compressor_register_bio_async(
    nimcp_semantic_compressor_t* compressor) {

    NIMCP_CHECK_THROW(compressor != NULL, NIMCP_ERROR_NULL_POINTER,
                      "nimcp_semantic_compressor_register_bio_async: compressor is NULL");

    LOG_INFO(COMPRESSION_MODULE, "Registering with bio-async system");

    /* Initialize bio context */
    compressor->bio_ctx.module_name = COMPRESSION_MODULE;
    compressor->bio_ctx.channel = compressor->config.bio_channel;
    compressor->bio_ctx.module_id = 0;  /* Will be assigned by router */

    /* Register with bio-router */
    nimcp_result_t result = nimcp_bio_router_register_module(&compressor->bio_ctx);
    if (result != NIMCP_SUCCESS) {
        LOG_ERROR(COMPRESSION_MODULE, "Failed to register with bio-router: %d", result);
        return result;
    }

    compressor->bio_async_registered = true;

    LOG_INFO(COMPRESSION_MODULE, "Successfully registered with bio-async");

    return NIMCP_SUCCESS;
}

uint32_t nimcp_semantic_compressor_process_inbox(
    nimcp_semantic_compressor_t* compressor) {

    if (!compressor || !compressor->bio_async_registered) {
        return 0;
    }

    uint32_t processed = 0;

    /* Process messages from bio-async inbox */
    bio_message_t msg;
    while (nimcp_bio_router_poll_message(&compressor->bio_ctx, &msg) == NIMCP_SUCCESS) {
        /* Handle different message types */
        switch (msg.header.msg_type) {
            case BIOMSG_COMPRESS_REQUEST:
                LOG_DEBUG(COMPRESSION_MODULE, "Received compress request");
                /* Would handle async compression here */
                break;

            case BIOMSG_DECOMPRESS_REQUEST:
                LOG_DEBUG(COMPRESSION_MODULE, "Received decompress request");
                /* Would handle async decompression here */
                break;

            default:
                LOG_WARN(COMPRESSION_MODULE, "Unknown message type: 0x%04x",
                         msg.header.msg_type);
                break;
        }

        processed++;
    }

    return processed;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from the knowledge graph
 *
 * WHAT: Retrieves structural self-knowledge about the Semantic_Compression module
 * WHY:  Enables runtime introspection and self-awareness capabilities
 * HOW:  Queries KG for Semantic_Compression entity and logs observations/relations
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge was found, 0 otherwise
 */
int semantic_compression_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Semantic_Compression");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            LOG_DEBUG(COMPRESSION_MODULE, "Semantic_Compression self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Semantic_Compression");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Semantic_Compression");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
