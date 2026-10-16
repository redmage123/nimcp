#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_language_gpu_bridge.c - Language-GPU Acceleration Bridge Implementation
//=============================================================================
/**
 * @file nimcp_language_gpu_bridge.c
 * @brief Implementation of GPU acceleration bridge for language processing
 *
 * WHAT: Bridge enabling GPU-accelerated language processing operations
 * WHY:  Accelerate computationally intensive language operations
 * HOW:  Batch processing, parallel embedding lookups, GPU semantic spreading
 *
 * @version 1.0.0 - Phase L4: Advanced Language Bridges
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#include "language/bridges/nimcp_language_gpu_bridge.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LOG_MODULE "LANG_GPU_BRIDGE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(language_gpu_bridge)

//=============================================================================
// String Conversion Utilities
//=============================================================================

const char* gpu_operation_type_to_string(gpu_operation_type_t type) {
    static const char* names[] = {
        "PHONEME_RECOGNIZE",
        "LEXICAL_LOOKUP",
        "SEMANTIC_SPREAD",
        "EMBEDDING_SIMILARITY",
        "ATTENTION_COMPUTE",
        "EMBEDDING_BATCH",
        "SOFTMAX",
        "MATRIX_MULTIPLY"
    };
    if (type >= GPU_OP_COUNT) return "UNKNOWN";
    return names[type];
}

const char* gpu_status_to_string(gpu_status_t status) {
    static const char* names[] = {
        "IDLE",
        "BATCHING",
        "EXECUTING",
        "TRANSFERRING",
        "COMPLETE",
        "ERROR"
    };
    if (status >= GPU_STATUS_COUNT) return "UNKNOWN";
    return names[status];
}

const char* gpu_backend_to_string(gpu_backend_t backend) {
    static const char* names[] = {
        "NONE",
        "CUDA",
        "OPENCL",
        "METAL",
        "VULKAN"
    };
    if (backend >= GPU_BACKEND_COUNT) return "UNKNOWN";
    return names[backend];
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

language_gpu_bridge_t* language_gpu_bridge_create(
    const language_gpu_config_t* config)
{
    language_gpu_bridge_t* bridge = (language_gpu_bridge_t*)
        nimcp_calloc(1, sizeof(language_gpu_bridge_t));
    if (!bridge) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate GPU bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    if (config) {
        memcpy(&bridge->config, config, sizeof(language_gpu_config_t));
    } else {
        /* Default configuration */
        bridge->config.enable_gpu = true;
        bridge->config.enable_phoneme_gpu = true;
        bridge->config.enable_lexical_gpu = true;
        bridge->config.enable_semantic_gpu = true;
        bridge->config.enable_embedding_gpu = true;
        bridge->config.device_id = LANGUAGE_GPU_DEFAULT_DEVICE_ID;
        bridge->config.batch_size = LANGUAGE_GPU_DEFAULT_BATCH_SIZE;
        bridge->config.batch_timeout_ms = LANGUAGE_GPU_DEFAULT_BATCH_TIMEOUT_MS;
        bridge->config.max_gpu_memory_mb = LANGUAGE_GPU_DEFAULT_MAX_MEMORY_MB;
        bridge->config.enable_async_transfer = true;
        bridge->config.enable_memory_pool = true;
        bridge->config.phoneme_batch_threshold = LANGUAGE_GPU_PHONEME_BATCH_THRESHOLD;
        bridge->config.word_batch_threshold = LANGUAGE_GPU_WORD_BATCH_THRESHOLD;
        bridge->config.enable_bio_async = false;
    }

    bridge->orchestrator = NULL;
    bridge->gpu_ctx = NULL;
    bridge->gpu_available = false;  /* Will be set during init */
    bridge->backend = GPU_BACKEND_NONE;
    bridge->status = GPU_STATUS_IDLE;

    /* Initialize device info */
    memset(&bridge->device_info, 0, sizeof(gpu_device_info_t));

    /* Initialize batch queues */
    memset(&bridge->batch_queue, 0, sizeof(batch_queue_state_t));

    /* Allocate pending ops array */
    bridge->max_pending = LANGUAGE_GPU_MAX_PENDING_OPS;
    bridge->pending_ops = (gpu_pending_op_t*)nimcp_calloc(
        bridge->max_pending, sizeof(gpu_pending_op_t));
    if (!bridge->pending_ops) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate pending ops array");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_gpu_bridge_create: bridge->pending_ops is NULL");
        return NULL;
    }
    bridge->num_pending = 0;

    /* Initialize memory pool state */
    memset(&bridge->memory_pool, 0, sizeof(memory_pool_state_t));
    bridge->memory_pool.pool_size = bridge->config.max_gpu_memory_mb * 1024 * 1024;

    /* GPU-resident data (NULL until uploaded) */
    bridge->word_embeddings_gpu = NULL;
    bridge->word_embedding_count = 0;
    bridge->embedding_dim = 0;
    bridge->concept_embeddings_gpu = NULL;
    bridge->concept_count = 0;
    bridge->adjacency_gpu = NULL;
    bridge->edge_weights_gpu = NULL;
    bridge->graph_nodes = 0;

    /* Bio-async */
    bridge->bio_router = NULL;
    bridge->bio_async_registered = false;

    memset(&bridge->stats, 0, sizeof(language_gpu_stats_t));
    bridge->initialized = false;
    bridge->active = false;

    LOG_INFO(LOG_MODULE, "GPU bridge created (device_id=%u)",
             bridge->config.device_id);
    return bridge;
}

void language_gpu_bridge_destroy(language_gpu_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "language_gpu");

    /* Unregister from bio-async if registered */
    if (bridge->bio_async_registered) {
        language_gpu_bridge_bio_async_unregister(bridge);
    }

    /* Free GPU-resident data */
    if (bridge->word_embeddings_gpu) {
        nimcp_free(bridge->word_embeddings_gpu);
    }
    if (bridge->concept_embeddings_gpu) {
        nimcp_free(bridge->concept_embeddings_gpu);
    }
    if (bridge->adjacency_gpu) {
        nimcp_free(bridge->adjacency_gpu);
    }
    if (bridge->edge_weights_gpu) {
        nimcp_free(bridge->edge_weights_gpu);
    }

    /* Free batch queues */
    if (bridge->batch_queue.phoneme_queue) {
        nimcp_free(bridge->batch_queue.phoneme_queue);
    }
    if (bridge->batch_queue.lexical_queue) {
        nimcp_free(bridge->batch_queue.lexical_queue);
    }
    if (bridge->batch_queue.embedding_queue) {
        nimcp_free(bridge->batch_queue.embedding_queue);
    }

    /* Free pending ops */
    if (bridge->pending_ops) {
        nimcp_free(bridge->pending_ops);
    }

    nimcp_free(bridge);
    LOG_INFO(LOG_MODULE, "GPU bridge destroyed");
}

int language_gpu_bridge_init(language_gpu_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Check for GPU availability - in a real implementation,
       this would probe the system for CUDA/OpenCL/etc. */
    bridge->gpu_available = false;  /* No GPU by default */
    bridge->backend = GPU_BACKEND_NONE;

    /* Initialize device info with default values */
    bridge->device_info.device_id = bridge->config.device_id;
    strncpy(bridge->device_info.name, "CPU Fallback",
            sizeof(bridge->device_info.name) - 1);
    bridge->device_info.backend = GPU_BACKEND_NONE;
    bridge->device_info.total_memory = 0;
    bridge->device_info.available_memory = 0;

    bridge->initialized = true;
    LOG_INFO(LOG_MODULE, "GPU bridge initialized (gpu_available=%s)",
             bridge->gpu_available ? "true" : "false");
    return 0;
}

int language_gpu_bridge_start(language_gpu_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_gpu_bridge_start: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    bridge->active = true;
    bridge->status = GPU_STATUS_IDLE;

    LOG_INFO(LOG_MODULE, "GPU bridge started");
    return 0;
}

int language_gpu_bridge_stop(language_gpu_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->active = false;
    bridge->status = GPU_STATUS_IDLE;

    LOG_INFO(LOG_MODULE, "GPU bridge stopped");
    return 0;
}

//=============================================================================
// Connection API Implementation
//=============================================================================

int language_gpu_bridge_connect_orchestrator(
    language_gpu_bridge_t* bridge,
    language_orchestrator_t* orchestrator)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->orchestrator = orchestrator;
    return 0;
}

int language_gpu_bridge_connect_gpu_context(
    language_gpu_bridge_t* bridge,
    gpu_execution_context_t* gpu_ctx)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->gpu_ctx = gpu_ctx;

    if (gpu_ctx) {
        bridge->gpu_available = true;
        /* Would query actual device info from context */
    }

    return 0;
}

//=============================================================================
// GPU Availability API Implementation
//=============================================================================

bool language_gpu_bridge_is_available(const language_gpu_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->gpu_available && bridge->active;
}

int language_gpu_bridge_get_device_info(
    const language_gpu_bridge_t* bridge,
    gpu_device_info_t* info)
{
    if (!bridge || !info) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_gpu_bridge_get_device_info: required parameter is NULL (bridge, info)");
        return -1;
    }
    memcpy(info, &bridge->device_info, sizeof(gpu_device_info_t));
    return 0;
}

gpu_status_t language_gpu_bridge_get_status(
    const language_gpu_bridge_t* bridge)
{
    if (!bridge) return GPU_STATUS_ERROR;
    return bridge->status;
}

//=============================================================================
// Data Upload API Implementation
//=============================================================================

int language_gpu_bridge_upload_word_embeddings(
    language_gpu_bridge_t* bridge,
    const float* embeddings,
    uint32_t count,
    uint32_t dim)
{
    if (!bridge || !embeddings || count == 0 || dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_gpu_bridge_upload_word_embeddings: required parameter is NULL (bridge, embeddings)");
        return -1;
    }

    /* Free existing embeddings and adjust memory tracking */
    if (bridge->word_embeddings_gpu) {
        size_t old_size = (size_t)bridge->word_embedding_count * bridge->embedding_dim * sizeof(float);
        bridge->memory_pool.used_memory -= old_size;
        nimcp_free(bridge->word_embeddings_gpu);
    }

    /* Allocate and copy (CPU fallback - real impl would use GPU memory) */
    size_t size = (size_t)count * dim * sizeof(float);
    bridge->word_embeddings_gpu = (float*)nimcp_malloc(size);
    if (!bridge->word_embeddings_gpu) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate word embeddings");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_gpu_bridge_upload_word_embeddings: bridge->word_embeddings_gpu is NULL");
        return -1;
    }

    memcpy(bridge->word_embeddings_gpu, embeddings, size);
    bridge->word_embedding_count = count;
    bridge->embedding_dim = dim;

    bridge->memory_pool.used_memory += size;
    if (bridge->memory_pool.used_memory > bridge->memory_pool.peak_usage) {
        bridge->memory_pool.peak_usage = bridge->memory_pool.used_memory;
    }
    bridge->memory_pool.num_allocations++;

    LOG_DEBUG(LOG_MODULE, "Uploaded %u word embeddings (dim=%u)", count, dim);
    return 0;
}

int language_gpu_bridge_upload_concept_embeddings(
    language_gpu_bridge_t* bridge,
    const float* embeddings,
    uint32_t count,
    uint32_t dim)
{
    if (!bridge || !embeddings || count == 0 || dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_gpu_bridge_upload_concept_embeddings: required parameter is NULL (bridge, embeddings)");
        return -1;
    }

    /* Free existing embeddings and adjust memory tracking */
    if (bridge->concept_embeddings_gpu) {
        size_t old_size = (size_t)bridge->concept_count * bridge->embedding_dim * sizeof(float);
        bridge->memory_pool.used_memory -= old_size;
        nimcp_free(bridge->concept_embeddings_gpu);
    }

    size_t size = (size_t)count * dim * sizeof(float);
    bridge->concept_embeddings_gpu = (float*)nimcp_malloc(size);
    if (!bridge->concept_embeddings_gpu) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate concept embeddings");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_gpu_bridge_upload_concept_embeddings: bridge->concept_embeddings_gpu is NULL");
        return -1;
    }

    memcpy(bridge->concept_embeddings_gpu, embeddings, size);
    bridge->concept_count = count;

    bridge->memory_pool.used_memory += size;
    if (bridge->memory_pool.used_memory > bridge->memory_pool.peak_usage) {
        bridge->memory_pool.peak_usage = bridge->memory_pool.used_memory;
    }
    bridge->memory_pool.num_allocations++;

    LOG_DEBUG(LOG_MODULE, "Uploaded %u concept embeddings", count);
    return 0;
}

int language_gpu_bridge_upload_semantic_graph(
    language_gpu_bridge_t* bridge,
    const uint32_t* adjacency,
    const float* weights,
    uint32_t num_nodes,
    uint32_t num_edges)
{
    if (!bridge || !adjacency || num_nodes == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_gpu_bridge_upload_semantic_graph: required parameter is NULL (bridge, adjacency)");
        return -1;
    }

    /* Free existing graph */
    if (bridge->adjacency_gpu) {
        nimcp_free(bridge->adjacency_gpu);
    }
    if (bridge->edge_weights_gpu) {
        nimcp_free(bridge->edge_weights_gpu);
    }

    size_t adj_size = (size_t)num_edges * 2 * sizeof(uint32_t);
    bridge->adjacency_gpu = (uint32_t*)nimcp_malloc(adj_size);
    if (!bridge->adjacency_gpu) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate adjacency list");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_gpu_bridge_upload_semantic_graph: bridge->adjacency_gpu is NULL");
        return -1;
    }
    memcpy(bridge->adjacency_gpu, adjacency, adj_size);

    if (weights) {
        size_t weight_size = (size_t)num_edges * sizeof(float);
        bridge->edge_weights_gpu = (float*)nimcp_malloc(weight_size);
        if (!bridge->edge_weights_gpu) {
            nimcp_free(bridge->adjacency_gpu);
            bridge->adjacency_gpu = NULL;
            LOG_ERROR(LOG_MODULE, "Failed to allocate edge weights");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_gpu_bridge_upload_semantic_graph: bridge->edge_weights_gpu is NULL");
            return -1;
        }
        memcpy(bridge->edge_weights_gpu, weights, weight_size);
    }

    bridge->graph_nodes = num_nodes;

    LOG_DEBUG(LOG_MODULE, "Uploaded semantic graph (%u nodes, %u edges)",
              num_nodes, num_edges);
    return 0;
}

//=============================================================================
// Batch Operation API Implementation
//=============================================================================

int language_gpu_bridge_submit_phoneme_batch(
    language_gpu_bridge_t* bridge,
    const phoneme_batch_op_t* op)
{
    if (!bridge || !op) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_gpu_bridge_submit_phoneme_batch: required parameter is NULL (bridge, op)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_gpu_bridge_submit_phoneme_batch: bridge->active is NULL");
        return -1;
    }

    /* Add to pending operations */
    if (bridge->num_pending >= bridge->max_pending) {
        LOG_WARN(LOG_MODULE, "Pending ops queue full");
        bridge->stats.errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "language_gpu_bridge_submit_phoneme_batch: capacity exceeded");
        return -1;
    }

    gpu_pending_op_t* pending = &bridge->pending_ops[bridge->num_pending++];
    pending->type = GPU_OP_PHONEME_RECOGNIZE;
    pending->input_data = (void*)op->spectral_frames;
    pending->output_data = (void*)op->phoneme_ids;
    pending->input_size = op->num_frames * op->frame_dim * sizeof(float);
    pending->output_size = op->max_output * sizeof(uint32_t);
    pending->batch_count = op->num_frames;

    bridge->status = GPU_STATUS_BATCHING;
    bridge->stats.phoneme_ops++;

    return 0;
}

int language_gpu_bridge_submit_lexical_batch(
    language_gpu_bridge_t* bridge,
    const lexical_batch_op_t* op)
{
    if (!bridge || !op) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_gpu_bridge_submit_lexical_batch: required parameter is NULL (bridge, op)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_gpu_bridge_submit_lexical_batch: bridge->active is NULL");
        return -1;
    }

    if (bridge->num_pending >= bridge->max_pending) {
        LOG_WARN(LOG_MODULE, "Pending ops queue full");
        bridge->stats.errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "language_gpu_bridge_submit_lexical_batch: capacity exceeded");
        return -1;
    }

    gpu_pending_op_t* pending = &bridge->pending_ops[bridge->num_pending++];
    pending->type = GPU_OP_LEXICAL_LOOKUP;
    pending->input_data = (void*)op->phoneme_sequences;
    pending->output_data = (void*)op->word_ids;
    pending->batch_count = op->num_sequences;

    bridge->status = GPU_STATUS_BATCHING;
    bridge->stats.lexical_ops++;

    return 0;
}

int language_gpu_bridge_submit_semantic_spread(
    language_gpu_bridge_t* bridge,
    const semantic_spread_op_t* op)
{
    if (!bridge || !op) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_gpu_bridge_submit_semantic_spread: required parameter is NULL (bridge, op)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_gpu_bridge_submit_semantic_spread: bridge->active is NULL");
        return -1;
    }

    if (bridge->num_pending >= bridge->max_pending) {
        bridge->stats.errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "language_gpu_bridge_submit_semantic_spread: capacity exceeded");
        return -1;
    }

    gpu_pending_op_t* pending = &bridge->pending_ops[bridge->num_pending++];
    pending->type = GPU_OP_SEMANTIC_SPREAD;
    pending->input_data = (void*)op->source_concept_ids;
    pending->output_data = (void*)op->activated_concepts;
    pending->batch_count = op->num_sources;

    bridge->stats.semantic_ops++;

    return 0;
}

int language_gpu_bridge_submit_embedding_batch(
    language_gpu_bridge_t* bridge,
    const embedding_batch_op_t* op)
{
    if (!bridge || !op) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_gpu_bridge_submit_embedding_batch: required parameter is NULL (bridge, op)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_gpu_bridge_submit_embedding_batch: bridge->active is NULL");
        return -1;
    }

    if (bridge->num_pending >= bridge->max_pending) {
        bridge->stats.errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "language_gpu_bridge_submit_embedding_batch: capacity exceeded");
        return -1;
    }

    gpu_pending_op_t* pending = &bridge->pending_ops[bridge->num_pending++];
    pending->type = GPU_OP_EMBEDDING_SIMILARITY;
    pending->input_data = (void*)op->query_embeddings;
    pending->output_data = (void*)op->top_k_indices;
    pending->batch_count = op->num_queries;

    bridge->stats.embedding_ops++;

    return 0;
}

int language_gpu_bridge_submit_attention_batch(
    language_gpu_bridge_t* bridge,
    const attention_batch_op_t* op)
{
    if (!bridge || !op) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_gpu_bridge_submit_attention_batch: required parameter is NULL (bridge, op)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_gpu_bridge_submit_attention_batch: bridge->active is NULL");
        return -1;
    }

    if (bridge->num_pending >= bridge->max_pending) {
        bridge->stats.errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "language_gpu_bridge_submit_attention_batch: capacity exceeded");
        return -1;
    }

    gpu_pending_op_t* pending = &bridge->pending_ops[bridge->num_pending++];
    pending->type = GPU_OP_ATTENTION_COMPUTE;
    pending->input_data = (void*)op->queries;
    pending->output_data = (void*)op->attention_output;
    pending->batch_count = op->batch_size;

    bridge->stats.attention_ops++;

    return 0;
}

int language_gpu_bridge_flush(language_gpu_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Execute all pending operations */
    bridge->status = GPU_STATUS_EXECUTING;

    /* In CPU fallback mode, just clear the queue */
    /* A real implementation would dispatch to GPU */
    bridge->num_pending = 0;
    bridge->stats.batches_executed++;

    bridge->status = GPU_STATUS_COMPLETE;

    return 0;
}

//=============================================================================
// Synchronous Operation API Implementation
//=============================================================================

int language_gpu_bridge_word_similarity_sync(
    language_gpu_bridge_t* bridge,
    const float* query_embedding,
    uint32_t dim,
    uint32_t top_k,
    uint32_t* result_ids,
    float* result_scores)
{
    if (!bridge || !query_embedding || !result_ids || !result_scores) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_gpu_bridge_word_similarity_sync: required parameter is NULL (bridge, query_embedding, result_ids, result_scores)");
        return -1;
    }
    if (!bridge->word_embeddings_gpu || bridge->word_embedding_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_gpu_bridge_word_similarity_sync: bridge->word_embeddings_gpu is NULL");
        return -1;
    }
    if (dim != bridge->embedding_dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "language_gpu_bridge_word_similarity_sync: validation failed");
        return -1;
    }

    /* CPU fallback: compute dot products */
    /* For each word, compute similarity */
    for (uint32_t k = 0; k < top_k; k++) {
        result_ids[k] = 0;
        result_scores[k] = -1.0f;
    }

    for (uint32_t i = 0; i < bridge->word_embedding_count; i++) {
        float* word_emb = &bridge->word_embeddings_gpu[i * dim];
        float score = 0.0f;

        for (uint32_t d = 0; d < dim; d++) {
            score += query_embedding[d] * word_emb[d];
        }

        /* Insert into top-k if better */
        for (uint32_t k = 0; k < top_k; k++) {
            if (score > result_scores[k]) {
                /* Shift down */
                for (uint32_t j = top_k - 1; j > k; j--) {
                    result_ids[j] = result_ids[j - 1];
                    result_scores[j] = result_scores[j - 1];
                }
                result_ids[k] = i;
                result_scores[k] = score;
                break;
            }
        }
    }

    bridge->stats.embedding_ops++;
    return 0;
}

int language_gpu_bridge_semantic_spread_sync(
    language_gpu_bridge_t* bridge,
    const uint32_t* source_concepts,
    const float* source_activations,
    uint32_t num_sources,
    uint32_t max_depth,
    uint32_t* result_concepts,
    float* result_activations,
    uint32_t max_results)
{
    if (!bridge || !source_concepts || !source_activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_gpu_bridge_semantic_spread_sync: required parameter is NULL (bridge, source_concepts, source_activations)");
        return -1;
    }
    if (!result_concepts || !result_activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_gpu_bridge_semantic_spread_sync: required parameter is NULL (result_concepts, result_activations)");
        return -1;
    }

    (void)max_depth;  /* Would be used for graph traversal depth */

    /* Simplified spreading activation (CPU fallback) */
    uint32_t num_activated = 0;

    /* Copy source concepts as initial activations */
    for (uint32_t i = 0; i < num_sources && num_activated < max_results; i++) {
        result_concepts[num_activated] = source_concepts[i];
        result_activations[num_activated] = source_activations[i];
        num_activated++;
    }

    bridge->stats.semantic_ops++;
    return (int)num_activated;
}

//=============================================================================
// Memory Management API Implementation
//=============================================================================

int language_gpu_bridge_get_memory_state(
    const language_gpu_bridge_t* bridge,
    memory_pool_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_gpu_bridge_get_memory_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    memcpy(state, &bridge->memory_pool, sizeof(memory_pool_state_t));
    return 0;
}

size_t language_gpu_bridge_free_unused_memory(language_gpu_bridge_t* bridge) {
    if (!bridge) return 0;

    /* In CPU fallback mode, nothing to free dynamically */
    return 0;
}

//=============================================================================
// Update and Statistics API Implementation
//=============================================================================

int language_gpu_bridge_update(
    language_gpu_bridge_t* bridge,
    uint64_t current_time_ms)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->active) return 0;

    /* Check batch timeouts and flush if needed */
    if (bridge->num_pending > 0 && bridge->status == GPU_STATUS_BATCHING) {
        /* Would check timeout and flush */
        language_gpu_bridge_flush(bridge);
    }

    bridge->stats.last_update_time_ms = current_time_ms;
    return 0;
}

int language_gpu_bridge_get_stats(
    const language_gpu_bridge_t* bridge,
    language_gpu_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_gpu_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    memcpy(stats, &bridge->stats, sizeof(language_gpu_stats_t));
    return 0;
}

//=============================================================================
// Bio-Async Integration Implementation
//=============================================================================

int language_gpu_bridge_bio_async_register(
    language_gpu_bridge_t* bridge,
    bio_router_t* router)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->bio_router = router;
    bridge->bio_async_registered = (router != NULL);

    LOG_DEBUG(LOG_MODULE, "Bio-async registration: %s",
              bridge->bio_async_registered ? "registered" : "unregistered");
    return 0;
}

int language_gpu_bridge_bio_async_unregister(language_gpu_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->bio_router = NULL;
    bridge->bio_async_registered = false;

    return 0;
}
