/**
 * @file nimcp_hopfield_memory.c
 * @brief Modern Hopfield Memory Implementation
 * @version 1.0.0
 * @date 2025-01-04
 */

#include "cognitive/memory/nimcp_hopfield_memory.h"
#include "cognitive/memory/nimcp_memory_kg_events.h"  /* W6: KG event emitters */
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(hopfield_memory, MESH_ADAPTER_CATEGORY_MEMORY)



/* Logging macros - wrap LOG_* for consistent usage */
#define NIMCP_LOG_INFO(...)  LOG_INFO(__VA_ARGS__)
#define NIMCP_LOG_WARN(...)  LOG_WARN(__VA_ARGS__)
#define NIMCP_LOG_ERROR(...) LOG_ERROR(__VA_ARGS__)

/* ============================================================================
 * Private Helpers
 * ============================================================================ */

/**
 * @brief Compute dot product of two vectors
 */
static float dot_product(const float* a, const float* b, uint32_t dim) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            hopfield_memory_heartbeat("hopfield_mem_loop",
                             (float)(i + 1) / (float)dim);
        }

        sum += a[i] * b[i];
    }
    return sum;
}

/**
 * @brief Compute L2 norm of vector
 */
static float l2_norm(const float* v, uint32_t dim) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            hopfield_memory_heartbeat("hopfield_mem_loop",
                             (float)(i + 1) / (float)dim);
        }

        sum += v[i] * v[i];
    }
    return sqrtf(sum);
}

/**
 * @brief Normalize vector in place
 */
static void normalize_vector(float* v, uint32_t dim) {
    float norm = l2_norm(v, dim);
    if (norm > 1e-8f) {
        float inv_norm = 1.0f / (fabsf(norm) > 1e-7f ? norm : 1e-7f);
        for (uint32_t i = 0; i < dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && dim > 256) {
                hopfield_memory_heartbeat("hopfield_mem_loop",
                                 (float)(i + 1) / (float)dim);
            }

            v[i] *= inv_norm;
        }
    }
}

/**
 * @brief Compute softmax over similarities
 */
static void compute_softmax(const float* similarities, float* attention,
                            uint32_t n, float beta) {
    float max_sim = -FLT_MAX;
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            hopfield_memory_heartbeat("hopfield_mem_loop",
                             (float)(i + 1) / (float)n);
        }

        if (similarities[i] > max_sim) {
            max_sim = similarities[i];
        }
    }

    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            hopfield_memory_heartbeat("hopfield_mem_loop",
                             (float)(i + 1) / (float)n);
        }

        attention[i] = expf(beta * (similarities[i] - max_sim));
        sum += attention[i];
    }

    float inv_sum = 1.0f / (sum + 1e-8f);
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            hopfield_memory_heartbeat("hopfield_mem_loop",
                             (float)(i + 1) / (float)n);
        }

        attention[i] *= inv_sum;
    }
}

/**
 * @brief Find pattern index by ID
 */
static int32_t find_pattern_index(const hopfield_memory_t* memory, uint32_t pattern_id) {
    if (!memory->metadata) {
        return -1;
    }
    for (uint32_t i = 0; i < memory->pattern_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && memory->pattern_count > 256) {
            hopfield_memory_heartbeat("hopfield_mem_loop",
                             (float)(i + 1) / (float)memory->pattern_count);
        }

        if (memory->metadata[i].pattern_id == pattern_id) {
            return (int32_t)i;
        }
    }
    /* Normal "not found" path - no throw needed */
    return -1;
}

/**
 * @brief Get pattern pointer by index
 */
static inline float* get_pattern(hopfield_memory_t* memory, uint32_t index) {
    return memory->patterns + (size_t)index * memory->config.pattern_dim;
}

/**
 * @brief Get const pattern pointer by index
 */
static inline const float* get_pattern_const(const hopfield_memory_t* memory, uint32_t index) {
    return memory->patterns + (size_t)index * memory->config.pattern_dim;
}

/**
 * @brief Should use GPU?
 */
static inline bool should_use_gpu(const hopfield_memory_t* memory, uint32_t batch_size) {
#ifdef NIMCP_ENABLE_CUDA
    /* P2-COG-04: Returning false is normal behavior, not an error */
    if (!memory || !memory->gpu_initialized) {
        return false;
    }
    if (memory->config.gpu_mode == HOPFIELD_GPU_DISABLED) {
        return false;
    }
    if (memory->config.gpu_mode == HOPFIELD_GPU_REQUIRED) {
        return true;
    }
    if (memory->config.gpu_mode == HOPFIELD_GPU_PREFERRED) {
        return true;
    }
    if (memory->config.gpu_mode == HOPFIELD_GPU_AUTO) {
        return batch_size >= memory->config.min_batch_for_gpu;
    }
    return false;
#else
    (void)memory;
    (void)batch_size;
    /* P2-COG-04: No GPU support is normal, not an error */
    return false;
#endif
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int hopfield_default_config(hopfield_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_hopfield_default_con", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");

    memset(config, 0, sizeof(*config));

    config->pattern_dim = HOPFIELD_DEFAULT_DIM;
    config->capacity = HOPFIELD_DEFAULT_CAPACITY;
    config->mode = HOPFIELD_MODE_SOFTMAX;
    config->store_mode = HOPFIELD_STORE_OVERWRITE;

    config->beta = HOPFIELD_DEFAULT_BETA;
    config->max_iterations = HOPFIELD_DEFAULT_ITERATIONS;
    config->convergence_threshold = HOPFIELD_CONVERGENCE_THRESHOLD;
    config->similarity_threshold = 0.9f;

    config->gpu_mode = HOPFIELD_GPU_AUTO;
    config->min_batch_for_gpu = 16;

    config->enable_metadata = true;
    config->normalize_patterns = true;

    config->enable_bio_async = false;

    return NIMCP_SUCCESS;
}

int hopfield_validate_config(const hopfield_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_hopfield_validate_co", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");
    NIMCP_CHECK_THROW(config->pattern_dim > 0 && config->pattern_dim <= 65536,
                      NIMCP_ERROR_INVALID_PARAM, "pattern_dim must be in range (0, 65536]");
    NIMCP_CHECK_THROW(config->capacity > 0 && config->capacity <= HOPFIELD_MAX_CAPACITY,
                      NIMCP_ERROR_INVALID_PARAM, "capacity must be in range (0, HOPFIELD_MAX_CAPACITY]");
    NIMCP_CHECK_THROW(config->beta > 0.0f && config->beta <= HOPFIELD_MAX_BETA,
                      NIMCP_ERROR_INVALID_PARAM, "beta must be in range (0.0, HOPFIELD_MAX_BETA]");
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

hopfield_memory_t* hopfield_memory_create(const hopfield_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_create", 0.0f);


    hopfield_config_t default_config;
    if (!config) {
        hopfield_default_config(&default_config);
        config = &default_config;
    }

    if (hopfield_validate_config(config) != NIMCP_SUCCESS) {
        NIMCP_LOG_ERROR("Invalid configuration");
        /* P2-COG-05: Use NIMCP_ERROR_INVALID_PARAM for config validation failure */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hopfield_memory_create: invalid configuration");
        return NULL;
    }

    hopfield_memory_t* memory = nimcp_calloc(1, sizeof(hopfield_memory_t));
    if (!memory) {
        NIMCP_LOG_ERROR("Failed to allocate Hopfield memory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hopfield_memory_create: memory is NULL");
        return NULL;
    }

    memcpy(&memory->config, config, sizeof(hopfield_config_t));

    mutex_attr_t attr = {.type = MUTEX_TYPE_NORMAL};
    memory->mutex = nimcp_mutex_create(&attr);
    if (!memory->mutex) {
        NIMCP_LOG_ERROR("Failed to create mutex");
        nimcp_free(memory);
        memory = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hopfield_memory_create: memory->mutex is NULL");
        return NULL;
    }

    size_t patterns_size = (size_t)config->capacity * config->pattern_dim * sizeof(float);
    memory->patterns = nimcp_calloc(1, patterns_size);
    if (!memory->patterns) {
        NIMCP_LOG_ERROR("Failed to allocate pattern storage");
        goto error;
    }

    if (config->enable_metadata) {
        memory->metadata = nimcp_calloc(config->capacity, sizeof(hopfield_pattern_meta_t));
        if (!memory->metadata) {
            NIMCP_LOG_ERROR("Failed to allocate metadata");
            goto error;
        }
    }

    memory->query_buffer = nimcp_calloc(config->pattern_dim, sizeof(float));
    memory->similarity_buffer = nimcp_calloc(config->capacity, sizeof(float));
    if (!memory->similarity_buffer) return NULL;
    memory->attention_buffer = nimcp_calloc(config->capacity, sizeof(float));

    if (!memory->query_buffer || !memory->similarity_buffer || !memory->attention_buffer) {
        NIMCP_LOG_ERROR("Failed to allocate work buffers");
        goto error;
    }

    memory->pattern_count = 0;
    memory->next_pattern_id = 1;

#ifdef NIMCP_ENABLE_CUDA
    memory->gpu_ctx = NULL;
    memory->patterns_device = NULL;
    memory->query_device = NULL;
    memory->similarity_device = NULL;
    memory->gpu_initialized = false;
    memory->patterns_synced = false;

    if (config->gpu_mode != HOPFIELD_GPU_DISABLED) {
        if (hopfield_memory_init_gpu(memory, NULL) != NIMCP_SUCCESS) {
            if (config->gpu_mode == HOPFIELD_GPU_REQUIRED) {
                NIMCP_LOG_ERROR("GPU required but init failed");
                goto error;
            }
            NIMCP_LOG_WARN("GPU init failed, using CPU");
        }
    }
#endif

    NIMCP_LOG_INFO("Created Hopfield memory: capacity=%u, dim=%u",
                   config->capacity, config->pattern_dim);
    return memory;

error:
    hopfield_memory_destroy(memory);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hopfield_memory_create: operation failed");
    return NULL;
}

void hopfield_memory_destroy(hopfield_memory_t* memory) {
    if (!memory) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_destroy", 0.0f);


#ifdef NIMCP_ENABLE_CUDA
    if (memory->patterns_device) {
        cudaFree(memory->patterns_device);
    }
    if (memory->query_device) {
        cudaFree(memory->query_device);
    }
    if (memory->similarity_device) {
        cudaFree(memory->similarity_device);
    }
    if (memory->gpu_ctx) {
        nimcp_gpu_context_destroy(memory->gpu_ctx);
    }
#endif

    if (memory->patterns) {
        nimcp_free(memory->patterns);
    }
    if (memory->metadata) {
        nimcp_free(memory->metadata);
    }
    if (memory->query_buffer) {
        nimcp_free(memory->query_buffer);
    }
    if (memory->similarity_buffer) {
        nimcp_free(memory->similarity_buffer);
    }
    if (memory->attention_buffer) {
        nimcp_free(memory->attention_buffer);
    }
    if (memory->mutex) {
        nimcp_mutex_destroy(memory->mutex);
    }

    nimcp_free(memory);
    memory = NULL;
}

int hopfield_memory_clear(hopfield_memory_t* memory) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_clear", 0.0f);


    NIMCP_CHECK_THROW(memory, NIMCP_ERROR_INVALID_PARAM, "memory is NULL");

    nimcp_mutex_lock(memory->mutex);

    memory->pattern_count = 0;
    memory->next_pattern_id = 1;

    size_t patterns_size = (size_t)memory->config.capacity *
                           memory->config.pattern_dim * sizeof(float);
    memset(memory->patterns, 0, patterns_size);

    if (memory->metadata) {
        memset(memory->metadata, 0,
               memory->config.capacity * sizeof(hopfield_pattern_meta_t));
    }

#ifdef NIMCP_ENABLE_CUDA
    memory->patterns_synced = false;
#endif

    nimcp_mutex_unlock(memory->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Pattern Storage API
 * ============================================================================ */

int hopfield_memory_store(hopfield_memory_t* memory,
                           const float* pattern,
                           uint32_t* pattern_id) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_store", 0.0f);


    return hopfield_memory_store_with_meta(memory, pattern, 1.0f, NULL, pattern_id);
}

int hopfield_memory_store_with_meta(hopfield_memory_t* memory,
                                     const float* pattern,
                                     float strength,
                                     void* user_data,
                                     uint32_t* pattern_id) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_store_with_meta", 0.0f);


    NIMCP_CHECK_THROW(memory, NIMCP_ERROR_INVALID_PARAM, "memory is NULL");
    NIMCP_CHECK_THROW(pattern, NIMCP_ERROR_INVALID_PARAM, "pattern is NULL");

    nimcp_mutex_lock(memory->mutex);

    uint32_t store_idx = 0;
    if (memory->pattern_count < memory->config.capacity) {
        store_idx = memory->pattern_count;
        memory->pattern_count++;
    } else {
        switch (memory->config.store_mode) {
            case HOPFIELD_STORE_OVERWRITE:
                store_idx = 0;
                for (uint32_t i = 1; i < memory->pattern_count; i++) {
                    if (memory->metadata &&
                        memory->metadata[i].access_count <
                        memory->metadata[store_idx].access_count) {
                        store_idx = i;
                    }
                }
                break;
            case HOPFIELD_STORE_REJECT:
                nimcp_mutex_unlock(memory->mutex);
                return NIMCP_ERROR_OUT_OF_RANGE;
            case HOPFIELD_STORE_MERGE:
                nimcp_mutex_unlock(memory->mutex);
                return NIMCP_ERROR_NOT_IMPLEMENTED;
            default:
                store_idx = 0;
                break;
        }
    }

    float* dest = get_pattern(memory, store_idx);
    memcpy(dest, pattern, memory->config.pattern_dim * sizeof(float));

    if (memory->config.normalize_patterns) {
        normalize_vector(dest, memory->config.pattern_dim);
    }

    uint32_t id = memory->next_pattern_id++;
    if (memory->metadata) {
        memory->metadata[store_idx].pattern_id = id;
        memory->metadata[store_idx].store_timestamp = 0;
        memory->metadata[store_idx].access_count = 0;
        memory->metadata[store_idx].last_access = 0;
        memory->metadata[store_idx].strength = strength;
        memory->metadata[store_idx].user_data = user_data;
    }

    if (pattern_id) {
        *pattern_id = id;
    }

#ifdef NIMCP_ENABLE_CUDA
    memory->patterns_synced = false;
#endif

    memory->stats.total_stores++;
    memory->stats.patterns_stored = memory->pattern_count;
    memory->stats.capacity_used = (float)memory->pattern_count /
                                   (float)memory->config.capacity;

    nimcp_mutex_unlock(memory->mutex);

    /* W6: emit KG event for hopfield pattern stored */
    memory_kg_emit_pattern_stored(
        memory_kg_events_get_registered_brain(), id, strength);

    return NIMCP_SUCCESS;
}

int hopfield_memory_store_batch(hopfield_memory_t* memory,
                                 const float* patterns,
                                 uint32_t num_patterns,
                                 uint32_t* pattern_ids) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_store_batch", 0.0f);


    NIMCP_CHECK_THROW(memory, NIMCP_ERROR_INVALID_PARAM, "memory is NULL");
    NIMCP_CHECK_THROW(patterns, NIMCP_ERROR_INVALID_PARAM, "patterns is NULL");
    NIMCP_CHECK_THROW(num_patterns > 0, NIMCP_ERROR_INVALID_PARAM, "num_patterns must be > 0");

    for (uint32_t i = 0; i < num_patterns; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_patterns > 256) {
            hopfield_memory_heartbeat("hopfield_mem_loop",
                             (float)(i + 1) / (float)num_patterns);
        }

        const float* pattern = patterns + (size_t)i * memory->config.pattern_dim;
        uint32_t id = 0;
        int ret = hopfield_memory_store(memory, pattern, &id);
        if (ret != NIMCP_SUCCESS) {
            return ret;
        }
        if (pattern_ids) {
            pattern_ids[i] = id;
        }
    }

    return NIMCP_SUCCESS;
}

int hopfield_memory_update_pattern(hopfield_memory_t* memory,
                                    uint32_t pattern_id,
                                    const float* pattern) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_update_pattern", 0.0f);


    NIMCP_CHECK_THROW(memory, NIMCP_ERROR_INVALID_PARAM, "memory is NULL");
    NIMCP_CHECK_THROW(pattern, NIMCP_ERROR_INVALID_PARAM, "pattern is NULL");

    nimcp_mutex_lock(memory->mutex);

    int32_t idx = find_pattern_index(memory, pattern_id);
    if (idx < 0) {
        nimcp_mutex_unlock(memory->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "pattern not found");
    }

    float* dest = get_pattern(memory, (uint32_t)idx);
    memcpy(dest, pattern, memory->config.pattern_dim * sizeof(float));

    if (memory->config.normalize_patterns) {
        normalize_vector(dest, memory->config.pattern_dim);
    }

#ifdef NIMCP_ENABLE_CUDA
    memory->patterns_synced = false;
#endif

    nimcp_mutex_unlock(memory->mutex);
    return NIMCP_SUCCESS;
}

int hopfield_memory_remove_pattern(hopfield_memory_t* memory,
                                    uint32_t pattern_id) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_remove_pattern", 0.0f);


    NIMCP_CHECK_THROW(memory, NIMCP_ERROR_INVALID_PARAM, "memory is NULL");

    nimcp_mutex_lock(memory->mutex);

    int32_t idx = find_pattern_index(memory, pattern_id);
    if (idx < 0) {
        nimcp_mutex_unlock(memory->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "pattern not found");
    }

    uint32_t last_idx = memory->pattern_count - 1;
    if ((uint32_t)idx != last_idx) {
        float* dest = get_pattern(memory, (uint32_t)idx);
        const float* src = get_pattern_const(memory, last_idx);
        memcpy(dest, src, memory->config.pattern_dim * sizeof(float));

        if (memory->metadata) {
            memory->metadata[idx] = memory->metadata[last_idx];
        }
    }

    memory->pattern_count--;
    memory->stats.patterns_stored = memory->pattern_count;
    memory->stats.capacity_used = (float)memory->pattern_count /
                                   (float)memory->config.capacity;

#ifdef NIMCP_ENABLE_CUDA
    memory->patterns_synced = false;
#endif

    nimcp_mutex_unlock(memory->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Pattern Retrieval API
 * ============================================================================ */

int hopfield_memory_retrieve(hopfield_memory_t* memory,
                              const float* query,
                              hopfield_retrieval_result_t* result) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_retrieve", 0.0f);


    return hopfield_memory_retrieve_iter(memory, query,
                                          memory->config.max_iterations, result);
}

int hopfield_memory_retrieve_iter(hopfield_memory_t* memory,
                                   const float* query,
                                   uint32_t max_iterations,
                                   hopfield_retrieval_result_t* result) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_retrieve_iter", 0.0f);


    NIMCP_CHECK_THROW(memory, NIMCP_ERROR_INVALID_PARAM, "memory is NULL");
    NIMCP_CHECK_THROW(query, NIMCP_ERROR_INVALID_PARAM, "query is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_INVALID_PARAM, "result is NULL");
    NIMCP_CHECK_THROW(result->pattern, NIMCP_ERROR_INVALID_PARAM, "result->pattern is NULL");

    nimcp_mutex_lock(memory->mutex);

    if (memory->pattern_count == 0) {
        nimcp_mutex_unlock(memory->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "no patterns stored");
    }

    uint32_t dim = memory->config.pattern_dim;
    float beta = memory->config.beta;

    memcpy(memory->query_buffer, query, dim * sizeof(float));
    if (memory->config.normalize_patterns) {
        normalize_vector(memory->query_buffer, dim);
    }

    bool converged = false;
    float prev_energy = FLT_MAX;
    uint32_t iter = 0;

    for (iter = 0; iter < max_iterations; iter++) {
        for (uint32_t i = 0; i < memory->pattern_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && memory->pattern_count > 256) {
                hopfield_memory_heartbeat("hopfield_mem_loop",
                                 (float)(i + 1) / (float)memory->pattern_count);
            }

            const float* pattern = get_pattern_const(memory, i);
            memory->similarity_buffer[i] = dot_product(memory->query_buffer,
                                                        pattern, dim);
        }

        compute_softmax(memory->similarity_buffer, memory->attention_buffer,
                        memory->pattern_count, beta);

        float* new_state = result->pattern;
        memset(new_state, 0, dim * sizeof(float));
        for (uint32_t i = 0; i < memory->pattern_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && memory->pattern_count > 256) {
                hopfield_memory_heartbeat("hopfield_mem_loop",
                                 (float)(i + 1) / (float)memory->pattern_count);
            }

            float weight = memory->attention_buffer[i];
            const float* pattern = get_pattern_const(memory, i);
            for (uint32_t j = 0; j < dim; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && dim > 256) {
                    hopfield_memory_heartbeat("hopfield_mem_loop",
                                     (float)(j + 1) / (float)dim);
                }

                new_state[j] += weight * pattern[j];
            }
        }

        if (memory->config.normalize_patterns) {
            normalize_vector(new_state, dim);
        }

        float energy = hopfield_memory_compute_energy(memory, new_state);
        float delta = fabsf(prev_energy - energy);

        if (delta < memory->config.convergence_threshold) {
            converged = true;
            break;
        }

        memcpy(memory->query_buffer, new_state, dim * sizeof(float));
        prev_energy = energy;
    }

    float best_sim = -FLT_MAX;
    uint32_t best_idx = 0;
    for (uint32_t i = 0; i < memory->pattern_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && memory->pattern_count > 256) {
            hopfield_memory_heartbeat("hopfield_mem_loop",
                             (float)(i + 1) / (float)memory->pattern_count);
        }

        if (memory->similarity_buffer[i] > best_sim) {
            best_sim = memory->similarity_buffer[i];
            best_idx = i;
        }
    }

    if (memory->metadata) {
        result->pattern_id = memory->metadata[best_idx].pattern_id;
        memory->metadata[best_idx].access_count++;
    } else {
        result->pattern_id = best_idx;
    }

    result->similarity = best_sim;
    result->energy = hopfield_memory_compute_energy(memory, result->pattern);
    result->iterations = iter + 1;
    result->converged = converged;

    memory->stats.total_retrievals++;
    if (converged) {
        memory->stats.successful_retrievals++;
    }
    if (isfinite(best_sim)) {
        memory->stats.avg_similarity = (memory->stats.avg_similarity * 0.99f) +
                                       (best_sim * 0.01f);
    }
    memory->stats.avg_iterations = (memory->stats.avg_iterations * 0.99f) +
                                   ((float)(iter + 1) * 0.01f);
    if (isfinite(result->energy)) {
        memory->stats.avg_energy = (memory->stats.avg_energy * 0.99f) +
                                   (result->energy * 0.01f);
    }

    if (should_use_gpu(memory, 1)) {
        memory->stats.gpu_retrievals++;
    } else {
        memory->stats.cpu_retrievals++;
    }

    uint32_t emitted_pattern_id = result->pattern_id;
    float emitted_sim = best_sim;
    bool emitted_converged = converged;

    nimcp_mutex_unlock(memory->mutex);

    /* W6: emit KG event for hopfield pattern completion */
    memory_kg_emit_pattern_completed(
        memory_kg_events_get_registered_brain(),
        emitted_pattern_id, emitted_sim, emitted_converged);

    return NIMCP_SUCCESS;
}

int hopfield_memory_retrieve_batch(hopfield_memory_t* memory,
                                    const float* queries,
                                    uint32_t num_queries,
                                    hopfield_batch_result_t* result) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_retrieve_batch", 0.0f);


    NIMCP_CHECK_THROW(memory, NIMCP_ERROR_INVALID_PARAM, "memory is NULL");
    NIMCP_CHECK_THROW(queries, NIMCP_ERROR_INVALID_PARAM, "queries is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_INVALID_PARAM, "result is NULL");
    NIMCP_CHECK_THROW(num_queries > 0, NIMCP_ERROR_INVALID_PARAM, "num_queries must be > 0");

    uint64_t start_time = 0;
    float total_sim = 0.0f;
    float total_energy = 0.0f;

    for (uint32_t i = 0; i < num_queries; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_queries > 256) {
            hopfield_memory_heartbeat("hopfield_mem_loop",
                             (float)(i + 1) / (float)num_queries);
        }

        const float* query = queries + (size_t)i * memory->config.pattern_dim;
        int ret = hopfield_memory_retrieve(memory, query, &result->results[i]);
        if (ret != NIMCP_SUCCESS) {
            return ret;
        }
        total_sim += result->results[i].similarity;
        total_energy += result->results[i].energy;
    }

    result->num_results = num_queries;
    result->avg_similarity = total_sim / (float)num_queries;
    result->avg_energy = total_energy / (float)num_queries;
    result->total_time_us = 0;

    return NIMCP_SUCCESS;
}

int hopfield_memory_top_k(hopfield_memory_t* memory,
                           const float* query,
                           uint32_t k,
                           uint32_t* pattern_ids,
                           float* similarities) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_top_k", 0.0f);


    NIMCP_CHECK_THROW(memory, NIMCP_ERROR_INVALID_PARAM, "memory is NULL");
    NIMCP_CHECK_THROW(query, NIMCP_ERROR_INVALID_PARAM, "query is NULL");
    NIMCP_CHECK_THROW(pattern_ids, NIMCP_ERROR_INVALID_PARAM, "pattern_ids is NULL");
    NIMCP_CHECK_THROW(similarities, NIMCP_ERROR_INVALID_PARAM, "similarities is NULL");
    NIMCP_CHECK_THROW(k > 0, NIMCP_ERROR_INVALID_PARAM, "k must be > 0");

    nimcp_mutex_lock(memory->mutex);

    if (memory->pattern_count == 0) {
        nimcp_mutex_unlock(memory->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "no patterns stored");
    }

    uint32_t actual_k = (k > memory->pattern_count) ? memory->pattern_count : k;

    float* query_norm = memory->query_buffer;
    memcpy(query_norm, query, memory->config.pattern_dim * sizeof(float));
    if (memory->config.normalize_patterns) {
        normalize_vector(query_norm, memory->config.pattern_dim);
    }

    for (uint32_t i = 0; i < memory->pattern_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && memory->pattern_count > 256) {
            hopfield_memory_heartbeat("hopfield_mem_loop",
                             (float)(i + 1) / (float)memory->pattern_count);
        }

        const float* pattern = get_pattern_const(memory, i);
        memory->similarity_buffer[i] = dot_product(query_norm, pattern,
                                                    memory->config.pattern_dim);
    }

    for (uint32_t i = 0; i < actual_k; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && actual_k > 256) {
            hopfield_memory_heartbeat("hopfield_mem_loop",
                             (float)(i + 1) / (float)actual_k);
        }

        float best_sim = -FLT_MAX;
        uint32_t best_idx = 0;
        for (uint32_t j = 0; j < memory->pattern_count; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && memory->pattern_count > 256) {
                hopfield_memory_heartbeat("hopfield_mem_loop",
                                 (float)(j + 1) / (float)memory->pattern_count);
            }

            if (memory->similarity_buffer[j] > best_sim) {
                best_sim = memory->similarity_buffer[j];
                best_idx = j;
            }
        }

        if (memory->metadata) {
            pattern_ids[i] = memory->metadata[best_idx].pattern_id;
        } else {
            pattern_ids[i] = best_idx;
        }
        similarities[i] = best_sim;
        memory->similarity_buffer[best_idx] = -FLT_MAX;
    }

    nimcp_mutex_unlock(memory->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Energy API
 * ============================================================================ */

float hopfield_memory_compute_energy(hopfield_memory_t* memory,
                                      const float* state) {
    if (!memory || !state || memory->pattern_count == 0) {
        return NAN;
    }

    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_compute_energy", 0.0f);


    float beta = memory->config.beta;
    float max_sim = -FLT_MAX;

    for (uint32_t i = 0; i < memory->pattern_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && memory->pattern_count > 256) {
            hopfield_memory_heartbeat("hopfield_mem_loop",
                             (float)(i + 1) / (float)memory->pattern_count);
        }

        const float* pattern = get_pattern_const(memory, i);
        float sim = dot_product(state, pattern, memory->config.pattern_dim);
        if (sim > max_sim) {
            max_sim = sim;
        }
    }

    float log_sum_exp = 0.0f;
    for (uint32_t i = 0; i < memory->pattern_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && memory->pattern_count > 256) {
            hopfield_memory_heartbeat("hopfield_mem_loop",
                             (float)(i + 1) / (float)memory->pattern_count);
        }

        const float* pattern = get_pattern_const(memory, i);
        float sim = dot_product(state, pattern, memory->config.pattern_dim);
        log_sum_exp += expf(beta * (sim - max_sim));
    }
    log_sum_exp = logf(log_sum_exp + 1e-8f) + beta * max_sim;

    float state_norm_sq = 0.0f;
    for (uint32_t i = 0; i < memory->config.pattern_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && memory->config.pattern_dim > 256) {
            hopfield_memory_heartbeat("hopfield_mem_loop",
                             (float)(i + 1) / (float)memory->config.pattern_dim);
        }

        state_norm_sq += state[i] * state[i];
    }

    float energy = -log_sum_exp + (beta / 2.0f) * state_norm_sq;
    return energy;
}

int hopfield_memory_get_similarities(hopfield_memory_t* memory,
                                      const float* query,
                                      float* similarities) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_get_similarities", 0.0f);


    NIMCP_CHECK_THROW(memory, NIMCP_ERROR_INVALID_PARAM, "memory is NULL");
    NIMCP_CHECK_THROW(query, NIMCP_ERROR_INVALID_PARAM, "query is NULL");
    NIMCP_CHECK_THROW(similarities, NIMCP_ERROR_INVALID_PARAM, "similarities output is NULL");

    nimcp_mutex_lock(memory->mutex);

    float* query_norm = memory->query_buffer;
    memcpy(query_norm, query, memory->config.pattern_dim * sizeof(float));
    if (memory->config.normalize_patterns) {
        normalize_vector(query_norm, memory->config.pattern_dim);
    }

    for (uint32_t i = 0; i < memory->pattern_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && memory->pattern_count > 256) {
            hopfield_memory_heartbeat("hopfield_mem_loop",
                             (float)(i + 1) / (float)memory->pattern_count);
        }

        const float* pattern = get_pattern_const(memory, i);
        similarities[i] = dot_product(query_norm, pattern,
                                       memory->config.pattern_dim);
    }

    nimcp_mutex_unlock(memory->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * GPU API
 * ============================================================================ */

#ifdef NIMCP_ENABLE_CUDA
int hopfield_memory_init_gpu(hopfield_memory_t* memory,
                              struct nimcp_gpu_context_s* gpu_ctx) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_init_gpu", 0.0f);


    NIMCP_CHECK_THROW(memory, NIMCP_ERROR_INVALID_PARAM, "memory is NULL");

    if (gpu_ctx) {
        memory->gpu_ctx = gpu_ctx;
    } else {
        memory->gpu_ctx = nimcp_gpu_context_create_auto();
        NIMCP_CHECK_THROW(memory->gpu_ctx, NIMCP_ERROR_GPU_NOT_AVAILABLE, "failed to create GPU context");
    }

    memory->gpu_initialized = true;
    memory->patterns_synced = false;
    NIMCP_LOG_INFO("GPU initialized for Hopfield memory");
    return NIMCP_SUCCESS;
}

int hopfield_memory_sync_to_gpu(hopfield_memory_t* memory) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_sync_to_gpu", 0.0f);


    NIMCP_CHECK_THROW(memory, NIMCP_ERROR_INVALID_PARAM, "memory is NULL");
    NIMCP_CHECK_THROW(memory->gpu_initialized, NIMCP_ERROR_INVALID_PARAM, "GPU not initialized");
    memory->patterns_synced = true;
    return NIMCP_SUCCESS;
}

bool hopfield_memory_has_gpu(const hopfield_memory_t* memory) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_has_gpu", 0.0f);


    return memory && memory->gpu_initialized;
}
#endif

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int hopfield_memory_get_stats(const hopfield_memory_t* memory,
                               hopfield_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_get_stats", 0.0f);


    NIMCP_CHECK_THROW(memory, NIMCP_ERROR_INVALID_PARAM, "memory is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_INVALID_PARAM, "stats output is NULL");
    memcpy(stats, &memory->stats, sizeof(hopfield_stats_t));
    return NIMCP_SUCCESS;
}

int hopfield_memory_reset_stats(hopfield_memory_t* memory) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_reset_stats", 0.0f);


    NIMCP_CHECK_THROW(memory, NIMCP_ERROR_INVALID_PARAM, "memory is NULL");

    nimcp_mutex_lock(memory->mutex);
    uint32_t stored = memory->stats.patterns_stored;
    float used = memory->stats.capacity_used;
    memset(&memory->stats, 0, sizeof(hopfield_stats_t));
    memory->stats.patterns_stored = stored;
    memory->stats.capacity_used = used;
    nimcp_mutex_unlock(memory->mutex);

    return NIMCP_SUCCESS;
}

uint32_t hopfield_memory_pattern_count(const hopfield_memory_t* memory) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_pattern_count", 0.0f);


    return memory ? memory->pattern_count : 0;
}

uint32_t hopfield_memory_capacity(const hopfield_memory_t* memory) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_capacity", 0.0f);


    return memory ? memory->config.capacity : 0;
}

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int hopfield_memory_connect_bio_async(hopfield_memory_t* memory) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_connect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(memory, NIMCP_ERROR_INVALID_PARAM, "memory is NULL");
    NIMCP_LOG_INFO("Bio-async connection (stub)");
    return NIMCP_SUCCESS;
}

int hopfield_memory_disconnect_bio_async(hopfield_memory_t* memory) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_disconnect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(memory, NIMCP_ERROR_INVALID_PARAM, "memory is NULL");
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Result Management API
 * ============================================================================ */

hopfield_retrieval_result_t* hopfield_result_create(uint32_t dim) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_hopfield_result_crea", 0.0f);


    hopfield_retrieval_result_t* result = nimcp_calloc(1, sizeof(hopfield_retrieval_result_t));
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate result");

        return NULL;
    }

    result->pattern = nimcp_calloc(dim, sizeof(float));
    if (!result->pattern) {
        nimcp_free(result);
        result = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hopfield_result_create: result->pattern is NULL");
        return NULL;
    }

    return result;
}

void hopfield_result_destroy(hopfield_retrieval_result_t* result) {
    if (!result) {
        return;
    }
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_hopfield_result_dest", 0.0f);


    if (result->pattern) {
        nimcp_free(result->pattern);
    }
    nimcp_free(result);
    result = NULL;
}

hopfield_batch_result_t* hopfield_batch_result_create(uint32_t num_queries,
                                                       uint32_t dim) {
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_hopfield_batch_resul", 0.0f);


    hopfield_batch_result_t* result = nimcp_calloc(1, sizeof(hopfield_batch_result_t));
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate result");

        return NULL;
    }

    result->results = nimcp_calloc(num_queries, sizeof(hopfield_retrieval_result_t));
    if (!result->results) {
        nimcp_free(result);
        result = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hopfield_result_destroy: result->results is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < num_queries; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_queries > 256) {
            hopfield_memory_heartbeat("hopfield_mem_loop",
                             (float)(i + 1) / (float)num_queries);
        }

        result->results[i].pattern = nimcp_calloc(dim, sizeof(float));
        if (!result->results[i].pattern) {
            for (uint32_t j = 0; j < i; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && i > 256) {
                    hopfield_memory_heartbeat("hopfield_mem_loop",
                                     (float)(j + 1) / (float)i);
                }

                nimcp_free(result->results[j].pattern);
            }
            nimcp_free(result->results);
            nimcp_free(result);
            result = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hopfield_result_destroy: validation failed");
            return NULL;
        }
    }

    result->num_results = num_queries;
    return result;
}

void hopfield_batch_result_destroy(hopfield_batch_result_t* result) {
    if (!result) {
        return;
    }
    /* Phase 8: Heartbeat at operation start */
    hopfield_memory_heartbeat("hopfield_mem_hopfield_batch_resul", 0.0f);


    if (result->results) {
        for (uint32_t i = 0; i < result->num_results; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && result->num_results > 256) {
                hopfield_memory_heartbeat("hopfield_mem_loop",
                                 (float)(i + 1) / (float)result->num_results);
            }

            if (result->results[i].pattern) {
                nimcp_free(result->results[i].pattern);
            }
        }
        nimcp_free(result->results);
    }
    nimcp_free(result);
    result = NULL;
}

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* hopfield_mode_to_string(hopfield_mode_t mode) {
    switch (mode) {
        case HOPFIELD_MODE_SOFTMAX: return "SOFTMAX";
        case HOPFIELD_MODE_EXPONENTIAL: return "EXPONENTIAL";
        case HOPFIELD_MODE_POLYNOMIAL: return "POLYNOMIAL";
        case HOPFIELD_MODE_SPARSE: return "SPARSE";
        default: return "UNKNOWN";
    }
}

const char* hopfield_store_mode_to_string(hopfield_store_mode_t mode) {
    switch (mode) {
        case HOPFIELD_STORE_OVERWRITE: return "OVERWRITE";
        case HOPFIELD_STORE_REJECT: return "REJECT";
        case HOPFIELD_STORE_MERGE: return "MERGE";
        default: return "UNKNOWN";
    }
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void hopfield_memory_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_hopfield_memory_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int hopfield_memory_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hopfield_memory_training_begin: NULL argument");
        return -1;
    }
    hopfield_memory_heartbeat_instance(NULL, "hopfield_memory_training_begin", 0.0f);
    return 0;
}

int hopfield_memory_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hopfield_memory_training_end: NULL argument");
        return -1;
    }
    hopfield_memory_heartbeat_instance(NULL, "hopfield_memory_training_end", 1.0f);
    return 0;
}

int hopfield_memory_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hopfield_memory_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    hopfield_memory_heartbeat_instance(NULL, "hopfield_memory_training_step", progress);
    return 0;
}
