#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_synapse_embeddings.c - CPU-Staged Semantic Embeddings for Synapses
//=============================================================================
/**
 * @file nimcp_synapse_embeddings.c
 * @brief ENHANCEMENT 1: CPU-staged semantic embeddings for intelligent synaptic routing
 *
 * WHAT: Per-synapse semantic vectors stored in a contiguous CPU pool (pinned memory)
 * WHY: Biological synapses are functionally specialized - this models that
 * HOW: Network-level pool of embedding vectors, indexed per synapse, GPU-staged on demand
 *
 * ARCHITECTURE (NIMCP 2.6.4):
 *   - Full 2048-dim embeddings live in CPU RAM (pinned for fast DMA)
 *   - Forward pass reads only cached semantic_relevance (4 bytes, always CPU)
 *   - Relevance recomputed via GPU batch kernel on context change
 *   - Training updates full embeddings on CPU, marks relevance dirty
 *
 * @author NIMCP Development Team
 * @date 2025-11-11 (pooled architecture: 2026-03-10)
 */

#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuralnet_internal.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "constants/nimcp_dimension_constants.h"
#include "gpu/embedding/nimcp_embedding_relevance_gpu.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_memory.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

// === BIO-ASYNC + LOGGING + UNIFIED MEMORY INTEGRATION ===
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "synapse_embeddings"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(synapse_embeddings)

#define BIO_MODULE_ID 0x013C

#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#endif


//=============================================================================
// Embedding Pool Lifecycle
//=============================================================================

bool embedding_pool_create(neural_network_t net, uint32_t capacity, uint16_t dim) {
    if (!net || capacity == 0 || dim == 0) {
        return false;
    }

    size_t pool_bytes = (size_t)capacity * dim * sizeof(float);
    bool pinned = false;

#ifdef NIMCP_ENABLE_CUDA
    // Try pinned memory for fast DMA transfers
    cudaError_t err = cudaHostAlloc((void**)&net->embedding_pool, pool_bytes, cudaHostAllocDefault);
    if (err == cudaSuccess) {
        pinned = true;
        LOG_MODULE_DEBUG(LOG_MODULE, "Allocated pinned embedding pool: %u slots × %uD = %.1f MB",
                         capacity, dim, (float)pool_bytes / (1024.0f * 1024.0f));
    } else {
        // Fall back to regular allocation
        cudaGetLastError();  // Clear error
        net->embedding_pool = (float*)nimcp_calloc(capacity, dim * sizeof(float));
        LOG_MODULE_DEBUG(LOG_MODULE, "Allocated regular embedding pool (pinned failed): %u slots × %uD",
                         capacity, dim);
    }
#else
    net->embedding_pool = (float*)nimcp_calloc(capacity, dim * sizeof(float));
#endif

    if (!net->embedding_pool) {
        LOG_ERROR("Failed to allocate embedding pool: %zu bytes", pool_bytes);
        return false;
    }

    // Allocate free list (for recycling freed slots)
    net->embedding_free_list = (uint32_t*)nimcp_malloc(capacity * sizeof(uint32_t));
    if (!net->embedding_free_list) {
        LOG_ERROR("Failed to allocate embedding free list");
#ifdef NIMCP_ENABLE_CUDA
        if (pinned) {
            cudaFreeHost(net->embedding_pool);
        } else {
            nimcp_free(net->embedding_pool);
        }
#else
        nimcp_free(net->embedding_pool);
#endif
        net->embedding_pool = NULL;
        return false;
    }

    net->embedding_pool_capacity = capacity;
    net->embedding_pool_used = 0;
    net->embedding_free_count = 0;
    net->embedding_dim = dim;
    net->embedding_pool_pinned = pinned;

    LOG_MODULE_DEBUG(LOG_MODULE, "Embedding pool created: %u slots × %uD (%.1f MB, %s)",
                     capacity, dim, (float)pool_bytes / (1024.0f * 1024.0f),
                     pinned ? "pinned" : "regular");
    return true;
}


void embedding_pool_destroy(neural_network_t net) {
    if (!net) return;

    // Destroy GPU embedding state if initialized
    if (net->embedding_gpu_initialized) {
        nimcp_embedding_gpu_destroy(&net->embedding_gpu_state);
        net->embedding_gpu_initialized = false;
    }

    if (net->embedding_pool) {
#ifdef NIMCP_ENABLE_CUDA
        if (net->embedding_pool_pinned) {
            cudaFreeHost(net->embedding_pool);
        } else {
            nimcp_free(net->embedding_pool);
        }
#else
        nimcp_free(net->embedding_pool);
#endif
        net->embedding_pool = NULL;
    }

    if (net->embedding_free_list) {
        nimcp_free(net->embedding_free_list);
        net->embedding_free_list = NULL;
    }

    net->embedding_pool_capacity = 0;
    net->embedding_pool_used = 0;
    net->embedding_free_count = 0;
    net->embedding_dim = 0;
    net->embedding_pool_pinned = false;
}


//=============================================================================
// Pool Slot Management
//=============================================================================

uint32_t embedding_pool_allocate(neural_network_t net) {
    if (!net || !net->embedding_pool) {
        return NIMCP_EMBEDDING_POOL_NONE;
    }

    // Prefer recycled slots
    if (net->embedding_free_count > 0) {
        uint32_t idx = net->embedding_free_list[--net->embedding_free_count];
        return idx;
    }

    // Bump allocate
    if (net->embedding_pool_used < net->embedding_pool_capacity) {
        return net->embedding_pool_used++;
    }

    // Pool exhausted — no growth for now (predictable memory)
    LOG_WARN("Embedding pool exhausted: %u/%u slots used",
             net->embedding_pool_used, net->embedding_pool_capacity);
    return NIMCP_EMBEDDING_POOL_NONE;
}


void embedding_pool_free_slot(neural_network_t net, uint32_t index) {
    if (!net || !net->embedding_pool || index >= net->embedding_pool_capacity) {
        return;
    }

    // Zero the embedding
    memset(&net->embedding_pool[(size_t)index * net->embedding_dim], 0,
           net->embedding_dim * sizeof(float));

    // Push to free list
    if (net->embedding_free_count < net->embedding_pool_capacity) {
        net->embedding_free_list[net->embedding_free_count++] = index;
    }
}


float* embedding_pool_get(neural_network_t net, uint32_t index) {
    if (!net || !net->embedding_pool || index >= net->embedding_pool_capacity) {
        return NULL;
    }
    return &net->embedding_pool[(size_t)index * net->embedding_dim];
}


//=============================================================================
// Synapse Embedding API (Pool-Based)
//=============================================================================

bool synapse_init_embedding(synapse_t *synapse, uint16_t dim) {
    // Legacy signature — requires network handle for pool access.
    // Use synapse_init_embedding_pooled() instead.
    (void)dim;
    if (synapse) {
        // embedding_pool_index and embedding_dim are now in cold storage
        // Cannot set without network handle — just clear semantic_relevance (hot)
        synapse->semantic_relevance = 0.5f;
    }
    return false;  // Cannot allocate without network handle
}


bool synapse_init_embedding_pooled(neural_network_t net, synapse_t *synapse) {
    if (!net || !synapse || !net->embedding_pool) {
        return false;
    }

    uint16_t dim = net->embedding_dim;
    synapse_cold_t* cold = SYNAPSE_ENSURE_COLD(net, synapse);
    if (!cold) {
        synapse->semantic_relevance = 0.5f;
        return false;
    }

    // Free existing if present
    if (cold->embedding_pool_index != NIMCP_EMBEDDING_POOL_NONE) {
        embedding_pool_free_slot(net, cold->embedding_pool_index);
    }

    // Allocate pool slot
    uint32_t idx = embedding_pool_allocate(net);
    if (idx == NIMCP_EMBEDDING_POOL_NONE) {
        cold->embedding_dim = 0;
        synapse->semantic_relevance = 0.5f;
        return false;
    }

    cold->embedding_pool_index = idx;
    cold->embedding_dim = dim;

    // Initialize with Xavier/Glorot
    float* emb = embedding_pool_get(net, idx);
    float scale = sqrtf(2.0f / dim);
    for (uint16_t i = 0; i < dim; i++) {
        emb[i] = ((float)nimcp_tl_rand() / RAND_MAX - 0.5f) * 2.0f * scale;
    }

    // Normalize to unit length
    float norm = 0.0f;
    for (uint16_t i = 0; i < dim; i++) {
        norm += emb[i] * emb[i];
    }
    norm = sqrtf(norm);
    if (norm > 1e-6f) {
        for (uint16_t i = 0; i < dim; i++) {
            emb[i] /= norm;
        }
    }

    synapse->semantic_relevance = 0.5f;
    return true;
}


//=============================================================================
// Bulk Embedding Initialization
//=============================================================================

uint32_t embedding_pool_init_all_synapses(neural_network_t net) {
    if (!net || !net->embedding_pool || !net->neurons) return 0;

    uint32_t initialized = 0;
    for (uint32_t n = 0; n < net->num_neurons; n++) {
        neuron_t* neuron = &net->neurons[n];
        uint32_t num_in = sparse_synapse_count(&neuron->incoming);
        for (uint32_t s = 0; s < num_in; s++) {
            synapse_t* meta = NEURON_IN_META(net, neuron, s);
            if (!meta) continue;
            // Skip if already has an embedding (check cold storage)
            synapse_cold_t* sc = SYNAPSE_COLD(net, meta);
            if (sc && sc->embedding_pool_index != NIMCP_EMBEDDING_POOL_NONE) continue;
            if (synapse_init_embedding_pooled(net, meta)) {
                initialized++;
            } else {
                // Pool exhausted
                goto done;
            }
        }
    }
done:
    if (initialized > 0) {
        LOG_MODULE_DEBUG(LOG_MODULE, "Bulk-initialized %u synapse embeddings (%u/%u pool slots used)",
                         initialized, net->embedding_pool_used, net->embedding_pool_capacity);
    }
    return initialized;
}


//=============================================================================
// Similarity Computation
//=============================================================================

float synapse_semantic_similarity(const synapse_t *syn1, const synapse_t *syn2) {
    // Cannot compute without network handle for pool access.
    // Use synapse_semantic_similarity_pooled() instead.
    (void)syn1; (void)syn2;
    return 0.0f;
}


float synapse_semantic_similarity_pooled(neural_network_t net,
                                          const synapse_t *syn1,
                                          const synapse_t *syn2) {
    if (!net || !syn1 || !syn2) return 0.0f;
    const synapse_cold_t* cold1 = SYNAPSE_COLD(net, (synapse_t*)syn1);
    const synapse_cold_t* cold2 = SYNAPSE_COLD(net, (synapse_t*)syn2);
    if (!cold1 || !cold2) return 0.0f;
    if (cold1->embedding_pool_index == NIMCP_EMBEDDING_POOL_NONE ||
        cold2->embedding_pool_index == NIMCP_EMBEDDING_POOL_NONE) return 0.0f;
    if (cold1->embedding_dim != cold2->embedding_dim) return 0.0f;

    const float* emb1 = embedding_pool_get(net, cold1->embedding_pool_index);
    const float* emb2 = embedding_pool_get(net, cold2->embedding_pool_index);
    if (!emb1 || !emb2) return 0.0f;

    float dot = 0.0f;
    for (uint16_t i = 0; i < cold1->embedding_dim; i++) {
        dot += emb1[i] * emb2[i];
    }
    return dot;  // Already normalized → cosine similarity
}


//=============================================================================
// Embedding Update
//=============================================================================

bool synapse_update_embedding(synapse_t *synapse, const float *target_embedding, float learning_rate) {
    // Legacy signature — cannot access pool. Returns false.
    (void)synapse; (void)target_embedding; (void)learning_rate;
    return false;
}


bool synapse_update_embedding_pooled(neural_network_t net, synapse_t *synapse,
                                      const float *target_embedding, float learning_rate) {
    if (!net || !synapse || !target_embedding) return false;
    synapse_cold_t* cold = SYNAPSE_COLD(net, synapse);
    if (!cold || cold->embedding_pool_index == NIMCP_EMBEDDING_POOL_NONE) return false;

    float* emb = embedding_pool_get(net, cold->embedding_pool_index);
    if (!emb) return false;

    uint16_t dim = cold->embedding_dim;

    // Gradient descent: emb += lr * (target - emb)
    for (uint16_t i = 0; i < dim; i++) {
        emb[i] += learning_rate * (target_embedding[i] - emb[i]);
    }

    // Renormalize to unit length
    float norm = 0.0f;
    for (uint16_t i = 0; i < dim; i++) {
        norm += emb[i] * emb[i];
    }
    norm = sqrtf(norm);
    if (norm > 1e-6f) {
        for (uint16_t i = 0; i < dim; i++) {
            emb[i] /= norm;
        }
    }

    return true;
}


//=============================================================================
// Relevance Computation
//=============================================================================

float synapse_compute_relevance(synapse_t *synapse, const float *context_embedding, uint16_t context_dim) {
    // Legacy signature — cannot access pool. Returns cached relevance.
    if (!synapse) return 0.0f;
    return synapse->semantic_relevance;
}


float synapse_compute_relevance_pooled(neural_network_t net, synapse_t *synapse,
                                        const float *context_embedding, uint16_t context_dim) {
    if (!net || !synapse || !context_embedding) return 0.0f;
    synapse_cold_t* cold = SYNAPSE_COLD(net, synapse);
    if (!cold || cold->embedding_pool_index == NIMCP_EMBEDDING_POOL_NONE) return 0.0f;
    if (cold->embedding_dim != context_dim) return 0.0f;

    const float* emb = embedding_pool_get(net, cold->embedding_pool_index);
    if (!emb) return 0.0f;

    float dot = 0.0f;
    float ctx_norm = 0.0f;
    for (uint16_t i = 0; i < context_dim; i++) {
        dot += emb[i] * context_embedding[i];
        ctx_norm += context_embedding[i] * context_embedding[i];
    }

    ctx_norm = sqrtf(ctx_norm);
    if (ctx_norm < 1e-6f) return 0.0f;

    float similarity = dot / ctx_norm;
    float relevance = (similarity + 1.0f) / 2.0f;

    synapse->semantic_relevance = relevance;
    return relevance;
}


//=============================================================================
// Batch Relevance Recomputation (CPU fallback)
//=============================================================================

uint32_t embedding_pool_recompute_relevance_cpu(neural_network_t net,
                                                 const float *context_embedding,
                                                 uint16_t context_dim) {
    if (!net || !context_embedding || !net->embedding_pool) return 0;
    if (context_dim != net->embedding_dim) return 0;

    // Pre-compute context norm
    float ctx_norm = 0.0f;
    for (uint16_t i = 0; i < context_dim; i++) {
        ctx_norm += context_embedding[i] * context_embedding[i];
    }
    ctx_norm = sqrtf(ctx_norm);
    if (ctx_norm < 1e-6f) return 0;

    float inv_ctx_norm = 1.0f / ctx_norm;
    uint32_t updated = 0;

    // Iterate all neurons and their incoming synapses
    if (!net->neurons) return 0;
    for (uint32_t n = 0; n < net->num_neurons; n++) {
        neuron_t* neuron = &net->neurons[n];
        uint32_t num_in = sparse_synapse_count(&neuron->incoming);
        for (uint32_t s = 0; s < num_in; s++) {
            synapse_t* meta = NEURON_IN_META(net, neuron, s);
            if (!meta) continue;
            synapse_cold_t* mc = SYNAPSE_COLD(net, meta);
            if (!mc || mc->embedding_pool_index == NIMCP_EMBEDDING_POOL_NONE) continue;

            const float* emb = embedding_pool_get(net, mc->embedding_pool_index);
            if (!emb) continue;

            float dot = 0.0f;
            for (uint16_t i = 0; i < context_dim; i++) {
                dot += emb[i] * context_embedding[i];
            }

            meta->semantic_relevance = (dot * inv_ctx_norm + 1.0f) / 2.0f;
            updated++;
        }
    }

    return updated;
}


//=============================================================================
// Cleanup
//=============================================================================

void synapse_destroy_embedding(synapse_t *synapse) {
    if (!synapse) return;

    // Pool-based: just reset the cached relevance on hot struct.
    // Cold fields (embedding_pool_index, embedding_dim) freed with cold pool.
    synapse->semantic_relevance = 0.0f;
}


void synapse_destroy_embedding_pooled(neural_network_t net, synapse_t *synapse) {
    if (!net || !synapse) return;

    synapse_cold_t* cold = SYNAPSE_COLD(net, synapse);
    if (cold && cold->embedding_pool_index != NIMCP_EMBEDDING_POOL_NONE) {
        embedding_pool_free_slot(net, cold->embedding_pool_index);
        cold->embedding_pool_index = NIMCP_EMBEDDING_POOL_NONE;
        cold->embedding_dim = 0;
    }

    synapse->semantic_relevance = 0.0f;
}


//=============================================================================
// GPU-Accelerated Relevance Recomputation (Orchestrator)
//=============================================================================

uint32_t embedding_pool_recompute_relevance(
    neural_network_t net,
    nimcp_gpu_context_t* gpu_ctx,
    const float* context_embedding,
    uint16_t context_dim)
{
    if (!net || !context_embedding || !net->embedding_pool) return 0;
    if (context_dim != net->embedding_dim) return 0;
    if (!net->neurons) return 0;

    // Lazy-init GPU state on first call (if GPU context available)
    nimcp_embedding_gpu_state_t* gpu_state = NULL;
    if (gpu_ctx && !net->embedding_gpu_initialized) {
        if (nimcp_embedding_gpu_init(gpu_ctx, &net->embedding_gpu_state,
                                      net->embedding_pool_capacity,
                                      net->embedding_dim,
                                      NIMCP_EMBEDDING_BATCH_SIZE)) {
            net->embedding_gpu_initialized = true;
            LOG_MODULE_DEBUG(LOG_MODULE, "Lazy-initialized embedding GPU state");
        }
    }
    if (net->embedding_gpu_initialized) {
        gpu_state = &net->embedding_gpu_state;
    }

    // Phase 1: Gather active embedding indices and their synapse_t pointers
    uint32_t max_active = net->embedding_pool_used;
    if (max_active == 0) return 0;

    uint32_t* active_indices = (uint32_t*)nimcp_malloc(max_active * sizeof(uint32_t));
    synapse_t** active_synapses = (synapse_t**)nimcp_malloc(max_active * sizeof(synapse_t*));
    if (!active_indices || !active_synapses) {
        if (active_indices) nimcp_free(active_indices);
        if (active_synapses) nimcp_free(active_synapses);
        return 0;
    }

    uint32_t num_active = 0;
    for (uint32_t n = 0; n < net->num_neurons; n++) {
        neuron_t* neuron = &net->neurons[n];
        uint32_t num_in = sparse_synapse_count(&neuron->incoming);
        for (uint32_t s = 0; s < num_in; s++) {
            synapse_t* meta = NEURON_IN_META(net, neuron, s);
            if (!meta) continue;
            synapse_cold_t* gc = SYNAPSE_COLD(net, meta);
            if (!gc || gc->embedding_pool_index == NIMCP_EMBEDDING_POOL_NONE) continue;
            if (num_active >= max_active) goto gather_done;
            active_indices[num_active] = gc->embedding_pool_index;
            active_synapses[num_active] = meta;
            num_active++;
        }
    }
gather_done:

    if (num_active == 0) {
        nimcp_free(active_indices);
        nimcp_free(active_synapses);
        return 0;
    }

    // Phase 2: Compute relevance (GPU or CPU)
    float* relevance_out = (float*)nimcp_malloc(num_active * sizeof(float));
    if (!relevance_out) {
        nimcp_free(active_indices);
        nimcp_free(active_synapses);
        return 0;
    }

    uint32_t processed = 0;

    if (gpu_state && gpu_state->initialized) {
        processed = nimcp_embedding_gpu_batch_relevance(
            gpu_ctx, gpu_state,
            net->embedding_pool,
            context_embedding,
            active_indices,
            num_active,
            relevance_out);

        if (processed == 0) {
            LOG_WARN("GPU relevance compute failed, falling back to CPU");
        }
    }

    if (processed == 0) {
        // CPU fallback
        float ctx_norm = 0.0f;
        for (uint16_t i = 0; i < context_dim; i++) {
            ctx_norm += context_embedding[i] * context_embedding[i];
        }
        ctx_norm = sqrtf(ctx_norm);
        if (ctx_norm < 1e-6f) {
            nimcp_free(active_indices);
            nimcp_free(active_synapses);
            nimcp_free(relevance_out);
            return 0;
        }
        float inv_norm = 1.0f / ctx_norm;

        for (uint32_t i = 0; i < num_active; i++) {
            const float* emb = embedding_pool_get(net, active_indices[i]);
            if (!emb) { relevance_out[i] = 0.5f; continue; }

            float dot = 0.0f;
            for (uint16_t d = 0; d < context_dim; d++) {
                dot += emb[d] * context_embedding[d];
            }
            relevance_out[i] = (dot * inv_norm + 1.0f) * 0.5f;
        }
        processed = num_active;
    }

    // Phase 3: Scatter results to synapse_t.semantic_relevance
    for (uint32_t i = 0; i < processed; i++) {
        active_synapses[i]->semantic_relevance = relevance_out[i];
    }

    LOG_MODULE_DEBUG(LOG_MODULE, "Relevance recomputed for %u/%u embeddings (%s)",
                     processed, num_active,
                     gpu_state ? "GPU" : "CPU");

    nimcp_free(active_indices);
    nimcp_free(active_synapses);
    nimcp_free(relevance_out);

    return processed;
}
