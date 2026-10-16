//=============================================================================
// nimcp_training_bridge.c - GPU Weight Cache & Forward Pass Bridge
//=============================================================================
/**
 * @file nimcp_training_bridge.c
 * @brief Bridges neuron/synapse AoS layout to GPU contiguous tensors
 *
 * WHAT: Extracts weight matrices from neuron_t.incoming_synapses[] into
 *       row-major GPU tensors, runs GEMV forward pass + MSE loss on GPU,
 *       and syncs activations/weights back to CPU structs.
 *
 * KEY ALGORITHM (weight extraction):
 *   For layer transition l -> l+1:
 *     For each neuron i in layer l+1:
 *       For each incoming_synapse j:
 *         src_local = source_neuron_id - layer_offset[l]
 *         W[i][src_local] = syn->weight * syn->strength
 *
 * PERFORMANCE:
 *   Upload/download are O(N*S) where N=neurons, S=avg synapses per neuron.
 *
 *   Forward pass is GPU-bound: O(sum(layer_sizes[l+1] * layer_sizes[l])).
 *   Loss computation is O(output_size) on GPU.
 */

#include "gpu/training/nimcp_training_bridge.h"
#include "training/nimcp_unified_training.h"
#include "gpu/sparse/nimcp_sparse_gpu.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "gpu/training/nimcp_training_gpu.h"
#include "gpu/tensor/nimcp_amp_autocast.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>

#define LOG_MODULE "gpu_training_bridge"

//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Compute layer neuron offsets from layer_sizes
 *
 * layer_offsets[0] = 0 (input layer starts at neuron 0)
 * layer_offsets[1] = layer_sizes[0] (first hidden layer starts after input)
 * etc.
 */
static uint32_t* compute_layer_offsets(const uint32_t* layer_sizes, uint32_t num_layers)
{
    uint32_t* offsets = nimcp_calloc(num_layers, sizeof(uint32_t));
    if (!offsets) return NULL;

    offsets[0] = 0;
    for (uint32_t l = 1; l < num_layers; l++) {
        uint32_t new_offset = offsets[l - 1] + layer_sizes[l - 1];
        if (new_offset < offsets[l - 1]) {
            /* uint32_t overflow in layer offset computation */
            NIMCP_LOG_ERROR("Layer offset overflow at layer %u: %u + %u wraps",
                            l, offsets[l - 1], layer_sizes[l - 1]);
            nimcp_free(offsets);
            return NULL;
        }
        offsets[l] = new_offset;
    }
    return offsets;
}

/**
 * @brief Find the maximum layer sizes for scratch buffer allocation
 *
 * Only computes max_bias and max_activation — weights are now sparse
 * with dynamically-grown COO/CSR scratch buffers.
 */
static void find_max_sizes(const uint32_t* layer_sizes, uint32_t num_layers,
                           size_t* max_bias, size_t* max_activation)
{
    *max_bias = 0;
    *max_activation = 0;

    for (uint32_t l = 0; l < num_layers; l++) {
        if (layer_sizes[l] > *max_activation) {
            *max_activation = layer_sizes[l];
        }
    }

    for (uint32_t l = 0; l < num_layers - 1; l++) {
        if (layer_sizes[l + 1] > *max_bias) *max_bias = layer_sizes[l + 1];
    }
}

/**
 * @brief Apply activation function to host buffer (for GPU clamp step)
 *
 * Used only as fallback when GPU activation returns false.
 * The GPU path applies activation via nimcp_gpu_sigmoid/tanh/relu.
 */
static void clamp_host_buffer(float* buf, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++) {
        if (buf[i] > 1.0f) buf[i] = 1.0f;
        else if (buf[i] < -1.0f) buf[i] = -1.0f;
    }
}

/**
 * @brief Ensure COO/CSR scratch buffers can hold the given sizes
 *
 * Uses 2x doubling strategy to amortize reallocation cost.
 *
 * @param cache Weight cache owning the scratch buffers
 * @param nnz Required COO entry count
 * @param rows Required row count (for CSR row_ptrs = rows+1)
 * @return true on success, false on allocation failure
 */
static bool ensure_coo_capacity(nimcp_gpu_weight_cache_t* cache, size_t nnz, size_t rows)
{
    // Grow COO arrays (values, row_idx, col_idx) if needed
    if (nnz > cache->host_coo_capacity) {
        size_t new_cap = cache->host_coo_capacity ? cache->host_coo_capacity : 1024;
        while (new_cap < nnz) new_cap *= 2;

        /* Realloc each COO array independently. On failure, realloc preserves
         * the original pointer, so we always update on success. */
        float* new_vals = nimcp_realloc(cache->host_coo_values, new_cap * sizeof(float));
        if (!new_vals) return false;
        cache->host_coo_values = new_vals;

        int* new_rows = nimcp_realloc(cache->host_coo_row_idx, new_cap * sizeof(int));
        if (!new_rows) return false;
        cache->host_coo_row_idx = new_rows;

        int* new_cols = nimcp_realloc(cache->host_coo_col_idx, new_cap * sizeof(int));
        if (!new_cols) return false;
        cache->host_coo_col_idx = new_cols;
        cache->host_coo_capacity = new_cap;
    }

    // Grow CSR row_ptrs if needed (rows + 1 entries)
    size_t row_ptrs_needed = rows + 1;
    if (row_ptrs_needed > cache->host_csr_row_ptrs_capacity) {
        size_t new_cap = cache->host_csr_row_ptrs_capacity ? cache->host_csr_row_ptrs_capacity : 256;
        while (new_cap < row_ptrs_needed) new_cap *= 2;

        int* new_ptrs = nimcp_realloc(cache->host_csr_row_ptrs, new_cap * sizeof(int));
        if (!new_ptrs) return false;
        cache->host_csr_row_ptrs = new_ptrs;
        cache->host_csr_row_ptrs_capacity = new_cap;
    }

    return true;
}

/**
 * @brief Clone a GPU tensor by downloading to host and re-uploading
 *
 * Used by gradient checkpointing to save activation snapshots.
 *
 * @param ctx GPU context
 * @param src Source tensor to clone
 * @return New tensor with same data, or NULL on failure
 */
static nimcp_gpu_tensor_t* clone_gpu_tensor(nimcp_gpu_context_t* ctx,
                                             const nimcp_gpu_tensor_t* src)
{
    if (!ctx || !src || !src->numel) return NULL;

    // Allocate host buffer, download, re-upload
    size_t bytes = src->numel * sizeof(float);
    float* host_buf = nimcp_malloc(bytes);
    if (!host_buf) return NULL;

    if (!nimcp_gpu_tensor_to_host(src, host_buf)) {
        nimcp_free(host_buf);
        return NULL;
    }

    nimcp_gpu_tensor_t* dst = nimcp_gpu_tensor_from_host(
        ctx, host_buf, src->dims, src->ndim, src->precision);
    nimcp_free(host_buf);
    return dst;
}

//=============================================================================
// Lifecycle
//=============================================================================

nimcp_gpu_weight_cache_t* nimcp_gpu_weight_cache_create(
    nimcp_gpu_context_t* ctx,
    neural_network_t net,
    const uint32_t* layer_sizes,
    uint32_t num_layers)
{
    if (!ctx || !net || !layer_sizes || num_layers < 2) {
        return NULL;
    }

    nimcp_gpu_weight_cache_t* cache = nimcp_calloc(1, sizeof(nimcp_gpu_weight_cache_t));
    if (!cache) return NULL;

    cache->ctx = ctx;
    cache->num_layers = num_layers;
    cache->weights_dirty_on_cpu = true;
    cache->last_sync_step = 0;

    // Create sparse context for cuSPARSE operations
    cache->sparse_ctx = nimcp_sparse_ctx_create(ctx);
    if (!cache->sparse_ctx) goto fail;

    // Copy layer sizes
    cache->layer_sizes = nimcp_calloc(num_layers, sizeof(uint32_t));
    if (!cache->layer_sizes) goto fail;
    memcpy(cache->layer_sizes, layer_sizes, num_layers * sizeof(uint32_t));

    // Compute layer offsets
    cache->layer_offsets = compute_layer_offsets(layer_sizes, num_layers);
    if (!cache->layer_offsets) goto fail;

    // Determine activation type per layer from first neuron in each layer
    cache->layer_activations = nimcp_calloc(num_layers, sizeof(activation_type_t));
    if (!cache->layer_activations) goto fail;
    for (uint32_t l = 1; l < num_layers; l++) {
        neuron_t* neuron = neural_network_get_neuron(net, cache->layer_offsets[l]);
        if (neuron) {
            cache->layer_activations[l] = neuron->activation_type;
        } else {
            cache->layer_activations[l] = ACTIVATION_TANH;  // safe default
        }
    }
    /* Force output layer to LINEAR activation.
     * The output layer produces a dense embedding (regression target).
     * ADAPTIVE activation zeros outputs below threshold, causing 95% sparsity
     * in the output vector — which prevents the network from using all 4096 dims.
     * LINEAR lets all values through unchanged. */
    if (num_layers > 1) {
        cache->layer_activations[num_layers - 1] = ACTIVATION_LINEAR;
    }

    // Allocate per-transition arrays (num_layers - 1 transitions)
    uint32_t num_transitions = num_layers - 1;

    // Sparse weights: NULL-initialized, populated during upload
    cache->sparse_weights = nimcp_calloc(num_transitions, sizeof(nimcp_sparse_tensor_t*));
    if (!cache->sparse_weights) goto fail;
    cache->bsr_weights = nimcp_calloc(num_transitions, sizeof(nimcp_sparse_tensor_t*));
    /* BSR allocation failure is non-fatal — falls back to CSR-only */

    // Dense bias tensors
    cache->biases = nimcp_calloc(num_transitions, sizeof(nimcp_gpu_tensor_t*));
    if (!cache->biases) goto fail;

    // Allocate per-layer activation arrays
    cache->activations = nimcp_calloc(num_layers, sizeof(nimcp_gpu_tensor_t*));
    if (!cache->activations) goto fail;

    // Create GPU tensors for bias vectors (sparse weights created during upload)
    for (uint32_t l = 0; l < num_transitions; l++) {
        uint32_t rows = layer_sizes[l + 1];  // output neurons
        size_t b_dims[1] = { rows };
        cache->biases[l] = nimcp_gpu_tensor_create(ctx, b_dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (!cache->biases[l]) goto fail;
    }

    // Create activation tensors for each layer
    for (uint32_t l = 0; l < num_layers; l++) {
        size_t a_dims[1] = { layer_sizes[l] };
        cache->activations[l] = nimcp_gpu_tensor_create(ctx, a_dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (!cache->activations[l]) goto fail;
    }

    // Allocate host scratch buffers for bias and activation (reused across calls)
    // COO/CSR scratch buffers are grown dynamically on first upload
    find_max_sizes(layer_sizes, num_layers,
                   &cache->host_bias_buf_size,
                   &cache->host_activation_buf_size);

    cache->host_coo_values  = NULL;
    cache->host_coo_row_idx = NULL;
    cache->host_coo_col_idx = NULL;
    cache->host_csr_row_ptrs = NULL;
    cache->host_coo_capacity = 0;
    cache->host_csr_row_ptrs_capacity = 0;

    cache->host_bias_buf = nimcp_calloc(cache->host_bias_buf_size, sizeof(float));
    cache->host_activation_buf = nimcp_calloc(cache->host_activation_buf_size, sizeof(float));
    if (!cache->host_bias_buf || !cache->host_activation_buf) goto fail;

    // Mixed precision: starts disabled (autocast_ctx = NULL from calloc)
    cache->autocast_ctx = NULL;
    cache->mixed_precision_enabled = false;

    // Gradient checkpointing: starts disabled (all fields NULL/false from calloc)
    cache->gradient_checkpointing = false;
    cache->checkpoint_interval = 0;
    cache->checkpoint_activations = NULL;
    cache->is_checkpoint_layer = NULL;
    cache->num_checkpoint_layers = 0;

    NIMCP_LOG_INFO("GPU weight cache created: %u layers, %u transitions",
                   num_layers, num_transitions);
    return cache;

fail:
    nimcp_gpu_weight_cache_destroy(cache);
    return NULL;
}

void nimcp_gpu_weight_cache_destroy(nimcp_gpu_weight_cache_t* cache)
{
    if (!cache) return;

    uint32_t num_transitions = (cache->num_layers > 1) ? cache->num_layers - 1 : 0;

    // Free sparse weight tensors (CSR)
    if (cache->sparse_weights) {
        for (uint32_t l = 0; l < num_transitions; l++) {
            if (cache->sparse_weights[l]) nimcp_sparse_tensor_destroy(cache->sparse_weights[l]);
        }
        nimcp_free(cache->sparse_weights);
    }
    // Free BSR weight tensors
    if (cache->bsr_weights) {
        for (uint32_t l = 0; l < num_transitions; l++) {
            if (cache->bsr_weights[l]) nimcp_sparse_tensor_destroy(cache->bsr_weights[l]);
        }
        nimcp_free(cache->bsr_weights);
    }

    // Free sparse context
    if (cache->sparse_ctx) {
        nimcp_sparse_ctx_destroy(cache->sparse_ctx);
    }

    // Free GPU bias tensors
    if (cache->biases) {
        for (uint32_t l = 0; l < num_transitions; l++) {
            if (cache->biases[l]) nimcp_gpu_tensor_destroy(cache->biases[l]);
        }
        nimcp_free(cache->biases);
    }

    // Free GPU activation tensors
    if (cache->activations) {
        for (uint32_t l = 0; l < cache->num_layers; l++) {
            if (cache->activations[l]) nimcp_gpu_tensor_destroy(cache->activations[l]);
        }
        nimcp_free(cache->activations);
    }

    // Free connected-neuron index lists
    if (cache->connected_dst) {
        for (uint32_t l = 0; l < num_transitions; l++) {
            nimcp_free(cache->connected_dst[l]);
        }
        nimcp_free(cache->connected_dst);
    }
    nimcp_free(cache->num_connected_dst);

    // Free gradient accumulation buffers
    if (cache->grad_accum_initialized) {
        for (uint32_t l = 0; l < num_transitions; l++) {
            if (cache->d_weight_grad_accum && cache->d_weight_grad_accum[l])
                nimcp_gpu_free(cache->ctx, cache->d_weight_grad_accum[l]);
            if (cache->d_bias_grad_accum && cache->d_bias_grad_accum[l])
                nimcp_gpu_free(cache->ctx, cache->d_bias_grad_accum[l]);
        }
    }
    nimcp_free(cache->d_weight_grad_accum);
    nimcp_free(cache->d_bias_grad_accum);

    // Free autocast context (mixed precision)
    if (cache->autocast_ctx) {
        nimcp_autocast_destroy(cache->autocast_ctx);
        cache->autocast_ctx = NULL;
    }

    // Free gradient checkpointing resources
    if (cache->checkpoint_activations) {
        for (uint32_t l = 0; l < cache->num_layers; l++) {
            if (cache->checkpoint_activations[l]) {
                nimcp_gpu_tensor_destroy(cache->checkpoint_activations[l]);
            }
        }
        nimcp_free(cache->checkpoint_activations);
    }
    nimcp_free(cache->is_checkpoint_layer);

    // Free CPU arrays
    nimcp_free(cache->layer_sizes);
    nimcp_free(cache->layer_offsets);
    nimcp_free(cache->layer_activations);
    nimcp_free(cache->host_coo_values);
    nimcp_free(cache->host_coo_row_idx);
    nimcp_free(cache->host_coo_col_idx);
    nimcp_free(cache->host_csr_row_ptrs);
    nimcp_free(cache->host_bias_buf);
    nimcp_free(cache->host_activation_buf);

    nimcp_free(cache);
}

//=============================================================================
// Weight Synchronization
//=============================================================================

bool nimcp_gpu_weight_cache_upload(nimcp_gpu_weight_cache_t* cache, neural_network_t net)
{
    if (!cache || !net) return false;

    // Validate layer sizes against actual network on first upload
    uint32_t total_neurons = neural_network_get_num_neurons(net);
    uint32_t config_total = 0;
    for (uint32_t l = 0; l < cache->num_layers; l++) {
        config_total += cache->layer_sizes[l];
    }
    if (config_total != total_neurons) {
        NIMCP_LOG_WARN("GPU cache layer_sizes sum (%u) != network neurons (%u) — "
                       "clamping layer sizes to actual neuron count",
                       config_total, total_neurons);
        // Clamp last layer so total matches actual neuron count
        if (config_total > total_neurons && cache->num_layers > 0) {
            uint32_t excess = config_total - total_neurons;
            uint32_t last = cache->num_layers - 1;
            if (cache->layer_sizes[last] > excess) {
                cache->layer_sizes[last] -= excess;
            }
        }
    }

    uint32_t num_transitions = cache->num_layers - 1;

    // First upload: build connected-neuron index lists for sparse networks.
    // Subsequent uploads reuse these lists, skipping millions of disconnected neurons.
    if (!cache->connected_dst_valid) {
        cache->connected_dst = nimcp_calloc(num_transitions, sizeof(uint32_t*));
        cache->num_connected_dst = nimcp_calloc(num_transitions, sizeof(uint32_t));
        if (!cache->connected_dst || !cache->num_connected_dst) {
            nimcp_free(cache->connected_dst);
            nimcp_free(cache->num_connected_dst);
            cache->connected_dst = NULL;
            cache->num_connected_dst = NULL;
            // Fall through — will use full scan below
        }

        if (cache->connected_dst) {
            for (uint32_t l = 0; l < num_transitions; l++) {
                uint32_t dst_layer_size = cache->layer_sizes[l + 1];
                uint32_t dst_offset = cache->layer_offsets[l + 1];

                // Clamp to actual neuron count
                if (dst_offset + dst_layer_size > total_neurons) {
                    dst_layer_size = (dst_offset < total_neurons) ? total_neurons - dst_offset : 0;
                }

                // Count connected neurons
                uint32_t count = 0;
                for (uint32_t i = 0; i < dst_layer_size; i++) {
                    neuron_t* neuron = neural_network_get_neuron(net, dst_offset + i);
                    if (neuron && NEURON_IN_COUNT(neuron) > 0) count++;
                }

                if (count > 0 && count < dst_layer_size) {
                    /* Sparse — build index list.
                     *
                     * RACE HARDENING: NEURON_IN_COUNT can change between the
                     * count pass above and the populate pass below, because
                     * structural plasticity and synaptogenesis can run from
                     * other threads. If a new synapse is added after the
                     * count pass, pass 2 would see MORE qualifying neurons
                     * than pass 1 and overflow the count-sized allocation
                     * (write out-of-bounds → heap corruption → deferred
                     * SEGV).
                     *
                     * Two safeguards:
                     *   1. Pad the allocation by 16 slots so a small number
                     *      of concurrent additions still fit.
                     *   2. Hard-cap writes at `slots` so even a large
                     *      concurrent burst cannot overrun. Any neurons
                     *      missed on this pass are picked up on the next
                     *      invalidation + rebuild.
                     * Also update num_connected_dst[l] to reflect what we
                     * actually wrote, not the stale pass-1 count. */
                    uint32_t slots = count + 16u;  /* +16 safety margin */
                    if (slots > dst_layer_size) slots = dst_layer_size;
                    cache->connected_dst[l] = nimcp_malloc(slots * sizeof(uint32_t));
                    if (cache->connected_dst[l]) {
                        uint32_t idx = 0;
                        for (uint32_t i = 0; i < dst_layer_size && idx < slots; i++) {
                            neuron_t* neuron = neural_network_get_neuron(net, dst_offset + i);
                            if (neuron && NEURON_IN_COUNT(neuron) > 0) {
                                cache->connected_dst[l][idx++] = i;
                            }
                        }
                        cache->num_connected_dst[l] = idx;
                    } else {
                        cache->num_connected_dst[l] = 0;
                    }
                } else {
                    /* count == 0 or == dst_layer_size: no sparse list needed.
                     * For count==dst_layer_size the iterator uses full scan;
                     * for count==0 there's nothing to iterate. */
                    cache->num_connected_dst[l] = count;
                }
                // If count == dst_layer_size (all connected), leave connected_dst[l] = NULL
                // to signal "use full iteration" (no benefit from index list)
            }
            cache->connected_dst_valid = true;
        }
    }

    for (uint32_t l = 0; l < num_transitions; l++) {
        uint32_t dst_layer_size = cache->layer_sizes[l + 1];  // post-synaptic
        uint32_t src_layer_size = cache->layer_sizes[l];       // pre-synaptic
        uint32_t dst_offset = cache->layer_offsets[l + 1];
        uint32_t src_offset = cache->layer_offsets[l];

        memset(cache->host_bias_buf, 0, dst_layer_size * sizeof(float));

        // Use connected-neuron index list if available (sparse fast path)
        bool use_sparse_list = cache->connected_dst_valid &&
                               cache->connected_dst &&
                               cache->connected_dst[l] != NULL;
        uint32_t iter_count = use_sparse_list ? cache->num_connected_dst[l] : dst_layer_size;

        // --- Pass 1: Count nnz and extract biases ---
        size_t nnz = 0;
        for (uint32_t ii = 0; ii < iter_count; ii++) {
            uint32_t i = use_sparse_list ? cache->connected_dst[l][ii] : ii;
            uint32_t neuron_id = dst_offset + i;
            neuron_t* neuron = neural_network_get_neuron(net, neuron_id);
            if (!neuron) continue;

            cache->host_bias_buf[i] = neuron->bias;

            for (uint32_t j = 0; j < NEURON_IN_COUNT(neuron); j++) {
                synapse_handle_t* in_h = NEURON_IN_HANDLE(neuron, j);
                if (!in_h) continue;

                uint32_t source_id = in_h->target_neuron_id;
                if (source_id >= src_offset &&
                    source_id < src_offset + src_layer_size) {
                    nnz++;
                }
            }
        }

        // Ensure COO/CSR scratch buffers are large enough
        if (nnz > 0 && !ensure_coo_capacity(cache, nnz, dst_layer_size)) {
            return false;
        }

        // --- Pass 2: Fill COO arrays ---
        size_t k = 0;
        for (uint32_t ii = 0; ii < iter_count; ii++) {
            uint32_t i = use_sparse_list ? cache->connected_dst[l][ii] : ii;
            uint32_t neuron_id = dst_offset + i;
            neuron_t* neuron = neural_network_get_neuron(net, neuron_id);
            if (!neuron) continue;

            for (uint32_t j = 0; j < NEURON_IN_COUNT(neuron); j++) {
                synapse_handle_t* in_h = NEURON_IN_HANDLE(neuron, j);
                if (!in_h) continue;

                uint32_t source_id = in_h->target_neuron_id;
                if (source_id >= src_offset &&
                    source_id < src_offset + src_layer_size) {
                    uint32_t src_local = source_id - src_offset;
                    float eff_weight = in_h->weight * in_h->strength;

                    cache->host_coo_values[k]  = eff_weight;
                    cache->host_coo_row_idx[k] = (int)i;
                    cache->host_coo_col_idx[k] = (int)src_local;
                    k++;
                }
            }
        }

        // --- Upload sparse weights via COO -> CSR ---
        // Destroy previous sparse tensor for this layer
        if (cache->sparse_weights[l]) {
            nimcp_sparse_tensor_destroy(cache->sparse_weights[l]);
            cache->sparse_weights[l] = NULL;
        }

        if (nnz > 0) {
            /* CSR for backward pass + plasticity, BSR for forward pass (tensor cores) */
            cache->sparse_weights[l] = nimcp_sparse_from_coo(
                cache->sparse_ctx,
                cache->host_coo_values,
                cache->host_coo_row_idx,
                cache->host_coo_col_idx,
                (int)dst_layer_size,
                (int)src_layer_size,
                (int)nnz,
                SPARSE_FORMAT_CSR);
            if (!cache->sparse_weights[l]) return false;

            /* BSR DISABLED: dual CSR+BSR doubles VRAM for 204M weights.
             * Random connectivity → ~6x BSR padding → CUDA OOM → GPU corruption.
             * Infrastructure ready for structured sparsity or larger VRAM. */
        }
        // nnz == 0: sparse_weights[l] stays NULL (valid empty transition)

        // Upload bias vector to GPU
        size_t b_dims[1] = { dst_layer_size };
        nimcp_gpu_tensor_t* b_upload = nimcp_gpu_tensor_from_host(
            cache->ctx, cache->host_bias_buf, b_dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (!b_upload) return false;

        nimcp_gpu_tensor_destroy(cache->biases[l]);
        cache->biases[l] = b_upload;
    }

    cache->weights_dirty_on_cpu = false;
    cache->last_sync_step++;
    return true;
}

bool nimcp_gpu_weight_cache_download(nimcp_gpu_weight_cache_t* cache, neural_network_t net)
{
    if (!cache || !net) return false;

    uint32_t num_transitions = cache->num_layers - 1;

    for (uint32_t l = 0; l < num_transitions; l++) {
        uint32_t dst_layer_size = cache->layer_sizes[l + 1];
        uint32_t src_layer_size = cache->layer_sizes[l];
        uint32_t dst_offset = cache->layer_offsets[l + 1];
        uint32_t src_offset = cache->layer_offsets[l];

        // Skip empty transitions
        if (!cache->sparse_weights[l]) continue;

        int nnz = nimcp_sparse_nnz(cache->sparse_weights[l]);
        if (nnz == 0) continue;

        // Ensure scratch buffers can hold the CSR data
        if (!ensure_coo_capacity(cache, (size_t)nnz, (size_t)dst_layer_size)) {
            return false;
        }

        // Download CSR data from GPU to host
        if (!nimcp_sparse_to_host_csr(cache->sparse_weights[l],
                                       cache->host_coo_values,
                                       cache->host_coo_col_idx,
                                       cache->host_csr_row_ptrs)) {
            return false;
        }

        // Write back to synapse structs using CSR structure
        for (uint32_t i = 0; i < dst_layer_size; i++) {
            uint32_t neuron_id = dst_offset + i;
            neuron_t* neuron = neural_network_get_neuron(net, neuron_id);
            if (!neuron) continue;

            int row_start = cache->host_csr_row_ptrs[i];
            int row_end   = cache->host_csr_row_ptrs[i + 1];

            for (uint32_t j = 0; j < NEURON_IN_COUNT(neuron); j++) {
                synapse_handle_t* in_h = NEURON_IN_HANDLE(neuron, j);
                if (!in_h) continue;

                uint32_t source_id = in_h->target_neuron_id;
                if (source_id >= src_offset &&
                    source_id < src_offset + src_layer_size) {
                    int src_local = (int)(source_id - src_offset);

                    // Binary search CSR row for matching column (columns sorted by cuSPARSE)
                    float eff_weight = 0.0f;
                    {
                        int lo = row_start, hi = row_end - 1;
                        while (lo <= hi) {
                            int mid = lo + (hi - lo) / 2;
                            int col = cache->host_coo_col_idx[mid];
                            if (col == src_local) {
                                eff_weight = cache->host_coo_values[mid];
                                break;
                            } else if (col < src_local) {
                                lo = mid + 1;
                            } else {
                                hi = mid - 1;
                            }
                        }
                    }

                    // Divide by strength to preserve strength separately
                    if (fabsf(in_h->strength) > 1e-8f) {
                        in_h->weight = eff_weight / in_h->strength;
                    } else {
                        in_h->weight = eff_weight;
                    }
                }
            }
        }
    }

    return true;
}

void nimcp_gpu_weight_cache_sync_activations(nimcp_gpu_weight_cache_t* cache, neural_network_t net)
{
    if (!cache || !net) return;

    for (uint32_t l = 0; l < cache->num_layers; l++) {
        uint32_t layer_size = cache->layer_sizes[l];
        uint32_t offset = cache->layer_offsets[l];

        memset(cache->host_activation_buf, 0, layer_size * sizeof(float));

        if (!nimcp_gpu_tensor_to_host(cache->activations[l], cache->host_activation_buf)) {
            continue;
        }

        // Write activation values back to neuron->state
        for (uint32_t i = 0; i < layer_size; i++) {
            neuron_t* neuron = neural_network_get_neuron(net, offset + i);
            if (neuron) {
                neuron->state = cache->host_activation_buf[i];
            }
        }
    }
}

//=============================================================================
// Internal: Forward pass for a single layer transition
//=============================================================================

/**
 * @brief Compute one layer transition: a[l+1] = activation(W[l] @ a[l] + b[l])
 *
 * Factored out of nimcp_gpu_forward_pass so gradient checkpointing can
 * reuse it for recomputation during backward pass.
 *
 * @param cache Weight cache (must have valid sparse_weights, biases, activations)
 * @param l Layer transition index (0-based: computes layer l+1 from layer l)
 * @param num_transitions Total number of transitions
 * @return true on success
 */
static bool forward_one_layer(nimcp_gpu_weight_cache_t* cache, uint32_t l, uint32_t num_transitions)
{
    // y = W[l] @ a[l]  (SpMV: sparse matrix-vector multiply)
    if (cache->sparse_weights[l]) {
        nimcp_gpu_tensor_t* mv_result = nimcp_sparse_mv(
            cache->sparse_ctx,
            cache->sparse_weights[l],
            cache->activations[l],
            1.0f, 0.0f,
            cache->activations[l + 1]);

        if (!mv_result) {
            NIMCP_LOG_ERROR("GPU SpMV failed for layer %u", l);
            return false;
        }
        if (mv_result != cache->activations[l + 1]) {
            nimcp_gpu_tensor_destroy(cache->activations[l + 1]);
            cache->activations[l + 1] = mv_result;
        }
    } else {
        // sparse_weights[l] is NULL (nnz=0): zero the activation tensor
        uint32_t ls = cache->layer_sizes[l + 1];
        memset(cache->host_activation_buf, 0, ls * sizeof(float));
        size_t a_dims[1] = { ls };
        nimcp_gpu_tensor_t* zero_tensor = nimcp_gpu_tensor_from_host(
            cache->ctx, cache->host_activation_buf, a_dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (!zero_tensor) return false;
        nimcp_gpu_tensor_destroy(cache->activations[l + 1]);
        cache->activations[l + 1] = zero_tensor;
    }

    // Add bias: a[l+1] = a[l+1] + b[l]
    if (!nimcp_gpu_add(cache->ctx,
                       cache->activations[l + 1],
                       cache->biases[l],
                       cache->activations[l + 1])) {
        NIMCP_LOG_ERROR("GPU bias add failed for layer %u: "
                        "activations[%u].numel=%zu, biases[%u].numel=%zu, "
                        "expected layer_size=%u",
                        l,
                        l + 1, cache->activations[l + 1] ? cache->activations[l + 1]->numel : 0,
                        l, cache->biases[l] ? cache->biases[l]->numel : 0,
                        cache->layer_sizes[l + 1]);
        return false;
    }

    // Apply activation function based on per-layer type
    // Force output layer to LINEAR (no threshold gate) regardless of stored type
    activation_type_t act = cache->layer_activations[l + 1];
    if (l + 1 == cache->num_layers - 1) {
        act = ACTIVATION_LINEAR;
    }
    bool act_ok = false;
    switch (act) {
        case ACTIVATION_SIGMOID:
            act_ok = nimcp_gpu_sigmoid(cache->ctx,
                cache->activations[l + 1], cache->activations[l + 1]);
            break;
        case ACTIVATION_TANH:
            act_ok = nimcp_gpu_tanh(cache->ctx,
                cache->activations[l + 1], cache->activations[l + 1]);
            break;
        case ACTIVATION_RELU:
            act_ok = nimcp_gpu_relu(cache->ctx,
                cache->activations[l + 1], cache->activations[l + 1]);
            break;
        case ACTIVATION_LEAKY_RELU:
            act_ok = nimcp_gpu_leaky_relu(cache->ctx,
                cache->activations[l + 1], cache->activations[l + 1], 0.01f);
            break;
        case ACTIVATION_ADAPTIVE: {
            /* GPU/CPU alignment fix: match CPU's threshold-gated tanh.
             * CPU does: if (x > threshold) tanh((x - threshold) / 10.0) else 0.
             * Since per-neuron thresholds live on CPU, download pre-activation
             * values, apply the gated activation on host, then upload back.
             * Uses a fixed threshold (0.1) as a proxy for the per-neuron
             * adaptive threshold, which converges to a narrow range. */
            uint32_t layer_size = cache->layer_sizes[l + 1];
            if (!nimcp_gpu_tensor_to_host(cache->activations[l + 1],
                                          cache->host_activation_buf)) {
                return false;
            }
            const float fixed_thresh = 0.1f;
            for (uint32_t ci = 0; ci < layer_size; ci++) {
                float x = cache->host_activation_buf[ci];
                if (x > fixed_thresh) {
                    cache->host_activation_buf[ci] = tanhf((x - fixed_thresh) / 10.0f);
                } else {
                    cache->host_activation_buf[ci] = 0.0f;
                }
            }
            size_t a_dims[1] = { layer_size };
            nimcp_gpu_tensor_t* act_result = nimcp_gpu_tensor_from_host(
                cache->ctx, cache->host_activation_buf, a_dims, 1, NIMCP_GPU_PRECISION_FP32);
            if (!act_result) return false;
            nimcp_gpu_tensor_destroy(cache->activations[l + 1]);
            cache->activations[l + 1] = act_result;
            act_ok = true;
            break;
        }
        case ACTIVATION_LINEAR:
            /* Identity — no activation transform. Output stays as-is. */
            act_ok = true;
            break;
        default:
            act_ok = nimcp_gpu_tanh(cache->ctx,
                cache->activations[l + 1], cache->activations[l + 1]);
            break;
    }

    if (!act_ok) {
        NIMCP_LOG_ERROR("GPU activation failed for layer %u", l);
        return false;
    }

    // Clamp unbounded activations — all use ±100 (weights trained with this regime)
    if (act == ACTIVATION_RELU || act == ACTIVATION_LEAKY_RELU || act == ACTIVATION_LINEAR) {
        float clamp_val = 100.0f;
        uint32_t layer_size = cache->layer_sizes[l + 1];
        if (!nimcp_gpu_tensor_to_host(cache->activations[l + 1],
                                      cache->host_activation_buf)) {
            return false;
        }
        for (uint32_t ci = 0; ci < layer_size; ci++) {
            if (cache->host_activation_buf[ci] > clamp_val)
                cache->host_activation_buf[ci] = clamp_val;
            else if (cache->host_activation_buf[ci] < -clamp_val)
                cache->host_activation_buf[ci] = -clamp_val;
        }
        size_t a_dims[1] = { layer_size };
        nimcp_gpu_tensor_t* clamped = nimcp_gpu_tensor_from_host(
            cache->ctx, cache->host_activation_buf, a_dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (!clamped) return false;
        nimcp_gpu_tensor_destroy(cache->activations[l + 1]);
        cache->activations[l + 1] = clamped;
    }

    // Layer normalization for hidden layers (backprop now accounts for this).
    // Skip output layer (last transition → output is raw LINEAR).
    if (l + 1 < num_transitions) {
        uint32_t layer_size = cache->layer_sizes[l + 1];
        if (layer_size > 1) {
            if (!nimcp_gpu_tensor_to_host(cache->activations[l + 1],
                                          cache->host_activation_buf)) {
                return false;
            }
            double sum = 0.0;
            for (uint32_t ci = 0; ci < layer_size; ci++) {
                sum += (double)cache->host_activation_buf[ci];
            }
            float mean = (float)(sum / layer_size);
            double var_sum = 0.0;
            for (uint32_t ci = 0; ci < layer_size; ci++) {
                float diff = cache->host_activation_buf[ci] - mean;
                var_sum += (double)(diff * diff);
            }
            float inv_std = 1.0f / sqrtf((float)(var_sum / layer_size) + 1e-5f);
            for (uint32_t ci = 0; ci < layer_size; ci++) {
                cache->host_activation_buf[ci] =
                    (cache->host_activation_buf[ci] - mean) * inv_std;
            }
            size_t a_dims[1] = { layer_size };
            nimcp_gpu_tensor_t* normed = nimcp_gpu_tensor_from_host(
                cache->ctx, cache->host_activation_buf, a_dims, 1,
                NIMCP_GPU_PRECISION_FP32);
            if (!normed) return false;
            nimcp_gpu_tensor_destroy(cache->activations[l + 1]);
            cache->activations[l + 1] = normed;
        }
    }

    return true;
}

//=============================================================================
// GPU Forward Pass
//=============================================================================

bool nimcp_gpu_forward_pass(
    nimcp_gpu_weight_cache_t* cache,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t output_size)
{
    if (!cache || !input || !output) return false;
    if (input_size != cache->layer_sizes[0]) {
        static int s_in_warn = 0;
        if (s_in_warn++ < 3)
            fprintf(stderr, "[GPU-FWD] input_size mismatch: %u vs cache %u\n",
                    input_size, cache->layer_sizes[0]);
        return false;
    }
    if (output_size != cache->layer_sizes[cache->num_layers - 1]) {
        static int s_out_warn = 0;
        if (s_out_warn++ < 3)
            fprintf(stderr, "[GPU-FWD] output_size mismatch: %u vs cache %u (layers=%u)\n",
                    output_size, cache->layer_sizes[cache->num_layers - 1], cache->num_layers);
        return false;
    }

    // Fast path: if ALL sparse weight matrices are NULL (no connections yet),
    // the output is just bias-through-activation ~ zeros. Skip GPU entirely to
    // avoid CUDA misaligned-address errors on very large layers (2M+ neurons).
    bool any_weights = false;
    for (uint32_t l = 0; l < cache->num_layers - 1; l++) {
        if (cache->sparse_weights[l]) { any_weights = true; break; }
    }
    if (!any_weights) {
        static int s_nw = 0;
        if (s_nw++ < 3) {
            fprintf(stderr, "[GPU-FWD] ALL sparse_weights NULL (%u layers) — returning zeros\n",
                    cache->num_layers);
            fflush(stderr);
        }
        memset(output, 0, output_size * sizeof(float));
        return true;
    }

    // Step 1: Upload input to layer 0 activation tensor
    size_t in_dims[1] = { input_size };
    nimcp_gpu_tensor_t* input_tensor = nimcp_gpu_tensor_from_host(
        cache->ctx, input, in_dims, 1, NIMCP_GPU_PRECISION_FP32);
    if (!input_tensor) return false;

    // Replace layer 0 activation with input
    nimcp_gpu_tensor_destroy(cache->activations[0]);
    cache->activations[0] = input_tensor;

    // Step 2: Forward pass through each layer transition
    uint32_t num_transitions = cache->num_layers - 1;

    // If gradient checkpointing is enabled, save checkpoint activations
    // and free non-checkpoint intermediate activations after use
    bool use_checkpointing = cache->gradient_checkpointing &&
                             cache->is_checkpoint_layer &&
                             cache->checkpoint_activations;

    if (use_checkpointing) {
        // Save layer 0 (input) — always a checkpoint
        if (cache->checkpoint_activations[0]) {
            nimcp_gpu_tensor_destroy(cache->checkpoint_activations[0]);
        }
        cache->checkpoint_activations[0] = clone_gpu_tensor(cache->ctx, cache->activations[0]);
    }

    /* Saved activations for residual skip connections (L-2 → L).
     * Stored on host to avoid GPU memory overhead. Lazily allocated. */
    float** residual_bufs = NULL;
    if (cache->num_layers > 3) {
        residual_bufs = (float**)nimcp_calloc(cache->num_layers, sizeof(float*));
    }

    for (uint32_t l = 0; l < num_transitions; l++) {
        if (!forward_one_layer(cache, l, num_transitions)) {
            if (residual_bufs) {
                for (uint32_t r = 0; r < cache->num_layers; r++) nimcp_free(residual_bufs[r]);
                nimcp_free(residual_bufs);
            }
            return false;
        }

        /* Residual skip: add activation from layer (l+1-2) to layer (l+1).
         * Truncated identity — copy min(src_dim, dst_dim) elements.
         * Done on host to keep GPU memory usage constant. */
        uint32_t dst_layer = l + 1;
        /* Residual skip L-1 → L (every hidden layer, skip output).
         * Truncated identity: copy min(src, dst) elements. */
        if (residual_bufs && dst_layer >= 2 && dst_layer < cache->num_layers - 1) {
            uint32_t src_layer = dst_layer - 1;
            float* src_buf = residual_bufs[src_layer];
            if (src_buf) {
                uint32_t src_dim = cache->layer_sizes[src_layer];
                uint32_t dst_dim = cache->layer_sizes[dst_layer];
                uint32_t copy_n = (src_dim < dst_dim) ? src_dim : dst_dim;

                /* Download current activation, add residual, upload back */
                if (!nimcp_gpu_tensor_to_host(cache->activations[dst_layer],
                                              cache->host_activation_buf)) {
                    /* Non-fatal: skip residual on download failure */
                } else {
                    for (uint32_t i = 0; i < copy_n; i++) {
                        cache->host_activation_buf[i] += src_buf[i];
                    }
                    size_t dims[1] = { dst_dim };
                    nimcp_gpu_tensor_t* updated = nimcp_gpu_tensor_from_host(
                        cache->ctx, cache->host_activation_buf, dims, 1,
                        NIMCP_GPU_PRECISION_FP32);
                    if (updated) {
                        nimcp_gpu_tensor_destroy(cache->activations[dst_layer]);
                        cache->activations[dst_layer] = updated;
                    }
                }
            }
        }

        /* Save current layer's post-norm activation for future residual use */
        if (residual_bufs && dst_layer < cache->num_layers) {
            uint32_t lsz = cache->layer_sizes[dst_layer];
            if (!residual_bufs[dst_layer]) {
                residual_bufs[dst_layer] = (float*)nimcp_malloc(lsz * sizeof(float));
            }
            if (residual_bufs[dst_layer]) {
                nimcp_gpu_tensor_to_host(cache->activations[dst_layer],
                                         residual_bufs[dst_layer]);
            }
        }

        // Gradient checkpointing: save activation at checkpoint boundaries
        if (use_checkpointing) {
            uint32_t out_layer = l + 1;
            if (cache->is_checkpoint_layer[out_layer]) {
                // Save a copy of this activation for backward recomputation
                if (cache->checkpoint_activations[out_layer]) {
                    nimcp_gpu_tensor_destroy(cache->checkpoint_activations[out_layer]);
                }
                cache->checkpoint_activations[out_layer] =
                    clone_gpu_tensor(cache->ctx, cache->activations[out_layer]);
            }
        }
    }

    /* Free residual buffers */
    if (residual_bufs) {
        for (uint32_t r = 0; r < cache->num_layers; r++) nimcp_free(residual_bufs[r]);
        nimcp_free(residual_bufs);
    }

    // Step 3: Download output from last layer activation
    uint32_t last_layer = cache->num_layers - 1;
    if (!nimcp_gpu_tensor_to_host(cache->activations[last_layer], output)) {
        return false;
    }



    return true;
}

/* Unified anti-collapse state for GPU compute_loss */
static nimcp_anti_collapse_state_t s_gpu_anti_collapse;
static bool s_gpu_anti_collapse_inited = false;

//=============================================================================
// GPU Loss Computation
//=============================================================================

float nimcp_gpu_compute_loss(
    nimcp_gpu_weight_cache_t* cache,
    const float* output,
    const float* target,
    uint32_t size)
{
    if (!cache || !output || !target || size == 0) return -1.0f;

    // Upload output and target to GPU tensors
    size_t dims[1] = { size };

    nimcp_gpu_tensor_t* pred_tensor = nimcp_gpu_tensor_from_host(
        cache->ctx, output, dims, 1, NIMCP_GPU_PRECISION_FP32);
    if (!pred_tensor) return -1.0f;

    nimcp_gpu_tensor_t* target_tensor = nimcp_gpu_tensor_from_host(
        cache->ctx, target, dims, 1, NIMCP_GPU_PRECISION_FP32);
    if (!target_tensor) {
        nimcp_gpu_tensor_destroy(pred_tensor);
        return -1.0f;
    }

    // Compute MSE loss
    float loss = -1.0f;
    bool ok = nimcp_gpu_loss_mse(cache->ctx, pred_tensor, target_tensor, &loss, NULL);

    nimcp_gpu_tensor_destroy(pred_tensor);
    nimcp_gpu_tensor_destroy(target_tensor);

    if (!ok) return -1.0f;

    /* Add diversity loss to prevent mode collapse (unified anti-collapse).
     * Note: When UTM is active, this function should not be called directly
     * for training — UTM handles diversity centrally. This remains for
     * standalone GPU loss evaluation (e.g., monitoring, validation). */
    {
        if (!s_gpu_anti_collapse_inited) {
            nimcp_anti_collapse_init(&s_gpu_anti_collapse, NULL);
            s_gpu_anti_collapse_inited = true;
        }
        float div_loss = nimcp_anti_collapse_diversity_loss(
            &s_gpu_anti_collapse, output, NULL, size);
        loss += div_loss;
    }

    return loss;
}

//=============================================================================
// Batched GPU Forward Pass
//=============================================================================

bool nimcp_gpu_forward_pass_batch(
    nimcp_gpu_weight_cache_t* cache,
    const float* inputs,
    uint32_t batch_size,
    uint32_t input_size,
    float* outputs,
    uint32_t output_size)
{
    if (!cache || !inputs || !outputs || batch_size == 0) return false;
    if (input_size != cache->layer_sizes[0]) return false;
    if (output_size != cache->layer_sizes[cache->num_layers - 1]) return false;

    // For batch_size == 1, delegate to single forward pass
    if (batch_size == 1) {
        return nimcp_gpu_forward_pass(cache, inputs, input_size, outputs, output_size);
    }

    // Upload input batch as 2D tensor [batch_size x input_size]
    size_t in_dims[2] = { batch_size, input_size };
    nimcp_gpu_tensor_t* act = nimcp_gpu_tensor_from_host(
        cache->ctx, inputs, in_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!act) return false;

    uint32_t num_transitions = cache->num_layers - 1;

    // Allocate host scratch for batched clamp
    size_t max_layer = 0;
    for (uint32_t l = 0; l <= num_transitions; l++) {
        if (cache->layer_sizes[l] > max_layer)
            max_layer = cache->layer_sizes[l];
    }
    float* clamp_buf = (float*)nimcp_malloc(batch_size * max_layer * sizeof(float));
    if (!clamp_buf) {
        nimcp_gpu_tensor_destroy(act);
        return false;
    }

    for (uint32_t l = 0; l < num_transitions; l++) {
        uint32_t out_size = cache->layer_sizes[l + 1];

        // Phase 3 optimization: True SpMM per layer instead of per-sample SpMV
        // Kernel launches: O(batch x layers) -> O(layers)
        nimcp_gpu_tensor_t* next_act = NULL;
        if (cache->sparse_weights[l]) {
            next_act = nimcp_sparse_linear_forward(
                cache->sparse_ctx, cache->sparse_weights[l],
                cache->biases[l], act);
        }

        if (!next_act) {
            // Zero fallback: allocate zeroed output tensor
            size_t out_dims[2] = { batch_size, out_size };
            next_act = nimcp_gpu_tensor_create(
                cache->ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);
            if (!next_act) {
                nimcp_gpu_tensor_destroy(act);
                nimcp_free(clamp_buf);
                return false;
            }
        }

        // Activation: download -> host-side apply -> re-upload
        // (kept for functional parity; future: GPU activation kernel)
        activation_type_t atype = cache->layer_activations[l + 1];
        size_t total_elems = (size_t)batch_size * out_size;
        if (!nimcp_gpu_tensor_to_host(next_act, clamp_buf)) {
            LOG_ERROR("Failed to download activation for layer %u", l);
            nimcp_gpu_tensor_destroy(next_act);
            nimcp_gpu_tensor_destroy(act);
            nimcp_free(clamp_buf);
            return false;
        }

        // Apply activation on host
        switch (atype) {
            case ACTIVATION_SIGMOID:
                for (size_t i = 0; i < total_elems; i++) {
                    clamp_buf[i] = 1.0f / (1.0f + expf(-clamp_buf[i]));
                }
                break;
            case ACTIVATION_TANH:
                for (size_t i = 0; i < total_elems; i++) {
                    clamp_buf[i] = tanhf(clamp_buf[i]);
                }
                break;
            case ACTIVATION_RELU:
                for (size_t i = 0; i < total_elems; i++) {
                    if (clamp_buf[i] < 0.0f) clamp_buf[i] = 0.0f;
                    else if (clamp_buf[i] > 100.0f) clamp_buf[i] = 100.0f;
                }
                break;
            case ACTIVATION_LEAKY_RELU:
                for (size_t i = 0; i < total_elems; i++) {
                    if (clamp_buf[i] < 0.0f) clamp_buf[i] *= 0.01f;
                    if (clamp_buf[i] > 100.0f) clamp_buf[i] = 100.0f;
                    else if (clamp_buf[i] < -100.0f) clamp_buf[i] = -100.0f;
                }
                break;
            case ACTIVATION_LINEAR:
                /* Keep ±100 clamp — weights trained with this. Weight decay handles the rest. */
                for (size_t i = 0; i < total_elems; i++) {
                    if (clamp_buf[i] > 100.0f) clamp_buf[i] = 100.0f;
                    else if (clamp_buf[i] < -100.0f) clamp_buf[i] = -100.0f;
                }
                break;
            default:
                for (size_t i = 0; i < total_elems; i++) {
                    clamp_buf[i] = tanhf(clamp_buf[i]);
                }
                break;
        }

        // Re-upload activated batch to GPU
        nimcp_gpu_tensor_destroy(act);
        nimcp_gpu_tensor_destroy(next_act);

        size_t out_dims[2] = { batch_size, out_size };
        act = nimcp_gpu_tensor_from_host(
            cache->ctx, clamp_buf, out_dims, 2, NIMCP_GPU_PRECISION_FP32);
        if (!act) {
            nimcp_free(clamp_buf);
            return false;
        }
    }

    // Download final output
    if (!nimcp_gpu_tensor_to_host(act, outputs)) {
        LOG_ERROR("Failed to download final output from GPU");
        nimcp_gpu_tensor_destroy(act);
        nimcp_free(clamp_buf);
        return false;
    }
    nimcp_gpu_tensor_destroy(act);
    nimcp_free(clamp_buf);
    return true;
}

//=============================================================================
// GPU Backward Pass (uses GPU sparse backprop kernels)
//=============================================================================

/**
 * Build activation type int array from cache's layer_activations enum.
 * Caller must free returned array.
 */
static int* build_act_types(nimcp_gpu_weight_cache_t* cache) {
    int* act_types = nimcp_calloc(cache->num_layers, sizeof(int));
    if (!act_types) return NULL;
    for (uint32_t l = 0; l < cache->num_layers; l++) {
        switch (cache->layer_activations[l]) {
            case ACTIVATION_RELU:       act_types[l] = 0; break;
            case ACTIVATION_LEAKY_RELU: act_types[l] = 1; break;
            case ACTIVATION_TANH:       act_types[l] = 2; break;
            case ACTIVATION_SIGMOID:    act_types[l] = 3; break;
            case ACTIVATION_LINEAR:     act_types[l] = 4; break;
            default:                    act_types[l] = 1; break;
        }
    }
    return act_types;
}

/**
 * @brief Recompute activations for a segment between two checkpoints
 *
 * Used during backward pass when gradient checkpointing is enabled.
 * Restores the activation at checkpoint_start from the saved copy,
 * then re-runs the forward pass for layers [checkpoint_start..target_layer]
 * to reconstruct intermediate activations needed for gradient computation.
 *
 * @param cache Weight cache with checkpoint_activations populated
 * @param checkpoint_start Layer index of the nearest preceding checkpoint
 * @param target_layer Layer index whose activation we need
 * @return true on success
 */
static bool recompute_activations_from_checkpoint(
    nimcp_gpu_weight_cache_t* cache,
    uint32_t checkpoint_start,
    uint32_t target_layer)
{
    if (!cache || !cache->checkpoint_activations) return false;
    if (checkpoint_start >= cache->num_layers) return false;
    if (target_layer >= cache->num_layers) return false;
    if (target_layer <= checkpoint_start) return true; // nothing to recompute

    uint32_t num_transitions = cache->num_layers - 1;

    // Restore activation at checkpoint_start from saved copy
    nimcp_gpu_tensor_t* saved = cache->checkpoint_activations[checkpoint_start];
    if (!saved) {
        NIMCP_LOG_ERROR("Gradient checkpointing: no saved activation at checkpoint layer %u",
                        checkpoint_start);
        return false;
    }

    // Clone saved activation into cache->activations[checkpoint_start]
    nimcp_gpu_tensor_t* restored = clone_gpu_tensor(cache->ctx, saved);
    if (!restored) return false;
    nimcp_gpu_tensor_destroy(cache->activations[checkpoint_start]);
    cache->activations[checkpoint_start] = restored;

    // Re-run forward pass from checkpoint_start to target_layer
    for (uint32_t l = checkpoint_start; l < target_layer && l < num_transitions; l++) {
        if (!forward_one_layer(cache, l, num_transitions)) {
            NIMCP_LOG_ERROR("Gradient checkpointing: recomputation failed at layer %u", l);
            return false;
        }
    }

    return true;
}

bool nimcp_gpu_backward_pass(
    nimcp_gpu_weight_cache_t* cache,
    neural_network_t net,
    const float* target,
    const float* output,
    uint32_t target_size,
    float learning_rate,
    float min_weight, float max_weight,
    float* out_grad_norm)
{
    if (!cache || !net || !target || !output || !out_grad_norm) return false;

    // If gradient checkpointing is active, recompute all activations before
    // the backward pass so the kernel has correct activation data.
    // Strategy: walk backward through checkpoint segments, recomputing each.
    bool use_checkpointing = cache->gradient_checkpointing &&
                             cache->is_checkpoint_layer &&
                             cache->checkpoint_activations;

    if (use_checkpointing && cache->num_layers > 2) {
        uint32_t num_transitions = cache->num_layers - 1;

        // Find the last checkpoint before the output layer and recompute
        // from each checkpoint segment. We walk backward so that by the
        // time the CUDA backward kernels run, all activations[] are valid.
        // The backward kernel processes layers right-to-left; it reads
        // activations[layer-1] for each layer. We need all of them populated.
        //
        // Simple approach: for each segment [ckpt_start .. next_ckpt-1],
        // recompute forward from ckpt_start.
        uint32_t seg_start = 0;
        for (uint32_t l = 1; l < cache->num_layers; l++) {
            if (cache->is_checkpoint_layer[l] || l == cache->num_layers - 1) {
                // Recompute segment [seg_start .. l]
                if (l > seg_start + 1) {
                    if (!recompute_activations_from_checkpoint(cache, seg_start, l)) {
                        NIMCP_LOG_ERROR("Gradient checkpointing: segment recomputation failed "
                                        "[%u..%u]", seg_start, l);
                        return false;
                    }
                }
                seg_start = l;
            }
        }
    }

    int* act_types = build_act_types(cache);
    if (!act_types) {
        NIMCP_LOG_WARN("gpu_backward: build_act_types failed");
        return false;
    }

    {
        static int _bp_bridge = 0;
        if (_bp_bridge++ < 5) {
            NIMCP_LOG_INFO("gpu_backward: layers=%u sparse_ctx=%p",
                           cache->num_layers, (void*)cache->sparse_ctx);
            for (uint32_t _l = 0; _l < cache->num_layers; _l++) {
                uint32_t sz = cache->layer_sizes ? cache->layer_sizes[_l] : 0;
                int nnz = 0;
                if (_l > 0 && cache->sparse_weights && cache->sparse_weights[_l-1] &&
                    cache->sparse_weights[_l-1]->format == SPARSE_FORMAT_CSR) {
                    nnz = cache->sparse_weights[_l-1]->data.csr.nnz;
                }
                NIMCP_LOG_INFO("  layer[%u]: size=%u nnz=%d act=%p",
                               _l, sz, nnz,
                               cache->activations ? (void*)cache->activations[_l] : NULL);
            }
        }
    }

    bool ok = nimcp_gpu_sparse_backward_pass(
        cache->ctx, cache->sparse_ctx,
        cache->sparse_weights, cache->biases, cache->activations,
        act_types, cache->num_layers, cache->layer_sizes,
        target, output, target_size,
        learning_rate, min_weight, max_weight,
        out_grad_norm);

    nimcp_free(act_types);

    if (ok) {
        nimcp_gpu_weight_cache_download(cache, net);
        cache->weights_dirty_on_cpu = true;
    }

    return ok;
}

//=============================================================================
// Gradient Accumulation Bridge
//=============================================================================

/**
 * Lazily initialize gradient accumulation buffers on GPU.
 * Allocates per-transition weight and bias gradient buffers, zeroed.
 */
static bool ensure_grad_accum_buffers(nimcp_gpu_weight_cache_t* cache) {
    if (cache->grad_accum_initialized) return true;

    uint32_t num_transitions = cache->num_layers - 1;
    cache->d_weight_grad_accum = nimcp_calloc(num_transitions, sizeof(float*));
    cache->d_bias_grad_accum = nimcp_calloc(num_transitions, sizeof(float*));
    if (!cache->d_weight_grad_accum || !cache->d_bias_grad_accum) {
        LOG_ERROR("Failed to allocate grad accum pointer arrays");
        nimcp_free(cache->d_weight_grad_accum);
        cache->d_weight_grad_accum = NULL;
        nimcp_free(cache->d_bias_grad_accum);
        cache->d_bias_grad_accum = NULL;
        return false;
    }

    for (uint32_t t = 0; t < num_transitions; t++) {
        nimcp_sparse_tensor_t* W = cache->sparse_weights[t];
        if (!W || W->format != SPARSE_FORMAT_CSR || W->data.csr.nnz == 0) continue;

        // Weight gradient buffer: same nnz as sparse weight matrix
        size_t w_bytes = W->data.csr.nnz * sizeof(float);
        cache->d_weight_grad_accum[t] = (float*)nimcp_gpu_malloc(cache->ctx, w_bytes);
        if (cache->d_weight_grad_accum[t]) {
            nimcp_gpu_memset(cache->ctx, cache->d_weight_grad_accum[t], 0, w_bytes);
        }

        // Bias gradient buffer: layer_sizes[t+1]
        uint32_t bias_size = cache->layer_sizes[t + 1];
        size_t b_bytes = bias_size * sizeof(float);
        cache->d_bias_grad_accum[t] = (float*)nimcp_gpu_malloc(cache->ctx, b_bytes);
        if (cache->d_bias_grad_accum[t]) {
            nimcp_gpu_memset(cache->ctx, cache->d_bias_grad_accum[t], 0, b_bytes);
        }
    }

    cache->grad_accum_count = 0;
    cache->grad_accum_initialized = true;
    LOG_INFO("Gradient accumulation buffers initialized (%u transitions)", num_transitions);
    return true;
}

bool nimcp_gpu_backward_accumulate(
    nimcp_gpu_weight_cache_t* cache,
    const float* target,
    const float* output,
    uint32_t target_size,
    float learning_rate)
{
    if (!cache || !target || !output) return false;

    if (!ensure_grad_accum_buffers(cache)) return false;

    // Enter autocast region if mixed precision is enabled
    bool autocast_active = cache->mixed_precision_enabled && cache->autocast_ctx;
    if (autocast_active) {
        nimcp_autocast_begin(cache->autocast_ctx);
    }

    // If gradient checkpointing is active, recompute all activations before
    // accumulating gradients (same logic as nimcp_gpu_backward_pass)
    bool use_checkpointing = cache->gradient_checkpointing &&
                             cache->is_checkpoint_layer &&
                             cache->checkpoint_activations;

    if (use_checkpointing && cache->num_layers > 2) {
        uint32_t num_transitions = cache->num_layers - 1;
        uint32_t seg_start = 0;
        for (uint32_t l = 1; l < cache->num_layers; l++) {
            if (cache->is_checkpoint_layer[l] || l == cache->num_layers - 1) {
                if (l > seg_start + 1) {
                    if (!recompute_activations_from_checkpoint(cache, seg_start, l)) {
                        NIMCP_LOG_ERROR("Gradient checkpointing: segment recomputation failed "
                                        "[%u..%u] in accumulate", seg_start, l);
                        if (autocast_active) nimcp_autocast_end(cache->autocast_ctx);
                        return false;
                    }
                }
                seg_start = l;
            }
        }
    }

    int* act_types = build_act_types(cache);
    if (!act_types) {
        if (autocast_active) nimcp_autocast_end(cache->autocast_ctx);
        return false;
    }

    // Scale learning rate by loss scale for FP16 gradient stability
    float effective_lr = learning_rate;
    if (autocast_active) {
        float scale = nimcp_autocast_scale_loss(cache->autocast_ctx, 1.0f);
        effective_lr = learning_rate * scale;
    }

    /* CSR backward accumulate — BSR disabled due to NaN issues */
    bool ok = nimcp_gpu_sparse_backward_accumulate(
        cache->ctx, cache->sparse_ctx,
        cache->sparse_weights, cache->biases, cache->activations,
        cache->d_weight_grad_accum, cache->d_bias_grad_accum,
        act_types, cache->num_layers, cache->layer_sizes,
        target, output, target_size, effective_lr);

    nimcp_free(act_types);

    // Exit autocast region
    if (autocast_active) {
        nimcp_autocast_end(cache->autocast_ctx);
    }

    if (ok) {
        cache->grad_accum_count++;
    }
    return ok;
}

bool nimcp_gpu_gradient_flush_and_sync(
    nimcp_gpu_weight_cache_t* cache,
    neural_network_t net,
    float min_weight, float max_weight,
    float* out_grad_norm)
{
    if (!cache || !net || cache->grad_accum_count == 0) return false;
    if (!cache->grad_accum_initialized) return false;

    // Enter autocast region if mixed precision is enabled
    bool autocast_active = cache->mixed_precision_enabled && cache->autocast_ctx;
    if (autocast_active) {
        nimcp_autocast_begin(cache->autocast_ctx);
    }

    bool ok = nimcp_gpu_gradient_flush(
        cache->ctx, cache->sparse_weights, cache->biases,
        cache->d_weight_grad_accum, cache->d_bias_grad_accum,
        cache->num_layers, cache->layer_sizes,
        cache->grad_accum_count, min_weight, max_weight,
        out_grad_norm);

    // If mixed precision, unscale gradient norm and update loss scale
    if (autocast_active && ok && out_grad_norm) {
        // Gradient norm was computed with scaled gradients; report true norm
        float scale = nimcp_autocast_scale_loss(cache->autocast_ctx, 1.0f);
        if (scale > 0.0f) {
            *out_grad_norm /= scale;
        }
        // Update dynamic loss scale based on gradient health
        bool grads_valid = isfinite(*out_grad_norm) && *out_grad_norm < 1e6f;
        nimcp_autocast_update_scale(cache->autocast_ctx, grads_valid);
    }

    // Exit autocast region
    if (autocast_active) {
        nimcp_autocast_end(cache->autocast_ctx);
    }

    if (ok) {
        nimcp_gpu_weight_cache_download(cache, net);
        cache->weights_dirty_on_cpu = true;
        cache->grad_accum_count = 0;
    }

    return ok;
}

//=============================================================================
// Mixed Precision (AMP) Support
//=============================================================================

bool nimcp_gpu_weight_cache_enable_mixed_precision(
    nimcp_gpu_weight_cache_t* cache,
    bool enable)
{
    if (!cache) return false;

    if (enable && !cache->autocast_ctx) {
        // Create autocast context with FP16 mode and loss scaling enabled
        nimcp_autocast_config_t config;
        nimcp_autocast_default_config(&config, AUTOCAST_FP16);
        config.enable_scaler = true;
        config.init_scale = 65536.0f;  // 2^16 initial loss scale

        cache->autocast_ctx = nimcp_autocast_create_with_config(cache->ctx, &config);
        if (!cache->autocast_ctx) {
            NIMCP_LOG_ERROR("Failed to create autocast context for mixed precision");
            return false;
        }
        cache->mixed_precision_enabled = true;
        NIMCP_LOG_INFO("Mixed precision (FP16) training enabled on GPU weight cache");
    } else if (enable && cache->autocast_ctx) {
        // Already created, just enable
        cache->mixed_precision_enabled = true;
        NIMCP_LOG_INFO("Mixed precision (FP16) training re-enabled");
    } else if (!enable) {
        // Disable but keep context for re-enabling
        cache->mixed_precision_enabled = false;
        NIMCP_LOG_INFO("Mixed precision training disabled");
    }

    return true;
}

bool nimcp_gpu_weight_cache_is_mixed_precision(
    const nimcp_gpu_weight_cache_t* cache)
{
    if (!cache) return false;
    return cache->mixed_precision_enabled && cache->autocast_ctx != NULL;
}

//=============================================================================
// Gradient Checkpointing Control
//=============================================================================

bool nimcp_gpu_weight_cache_set_gradient_checkpointing(
    nimcp_gpu_weight_cache_t* cache,
    bool enable,
    uint32_t checkpoint_interval)
{
    if (!cache) return false;

    if (enable) {
        // Default interval: every 2 layers
        uint32_t interval = (checkpoint_interval > 0) ? checkpoint_interval : 2;

        // Clamp interval to at most num_layers - 1 (otherwise no checkpoints)
        if (interval >= cache->num_layers) {
            interval = cache->num_layers - 1;
        }

        cache->checkpoint_interval = interval;

        // Allocate/reallocate checkpoint arrays if needed
        if (!cache->checkpoint_activations) {
            cache->checkpoint_activations = nimcp_calloc(cache->num_layers,
                                                          sizeof(nimcp_gpu_tensor_t*));
            if (!cache->checkpoint_activations) {
                NIMCP_LOG_ERROR("Failed to allocate checkpoint activation array");
                return false;
            }
        }

        if (!cache->is_checkpoint_layer) {
            cache->is_checkpoint_layer = nimcp_calloc(cache->num_layers, sizeof(bool));
            if (!cache->is_checkpoint_layer) {
                NIMCP_LOG_ERROR("Failed to allocate checkpoint layer flags");
                return false;
            }
        }

        // Mark checkpoint boundary layers:
        // - Layer 0 (input) is always a checkpoint
        // - Every interval-th layer is a checkpoint
        // - Last layer (output) is always a checkpoint
        uint32_t count = 0;
        for (uint32_t l = 0; l < cache->num_layers; l++) {
            bool is_ckpt = (l == 0) ||
                           (l == cache->num_layers - 1) ||
                           (l % interval == 0);
            cache->is_checkpoint_layer[l] = is_ckpt;
            if (is_ckpt) count++;
        }
        cache->num_checkpoint_layers = count;
        cache->gradient_checkpointing = true;

        NIMCP_LOG_INFO("Gradient checkpointing enabled: interval=%u, %u/%u layers checkpointed, "
                       "~%.0f%% memory saved",
                       interval, count, cache->num_layers,
                       (1.0 - (double)count / cache->num_layers) * 100.0);
    } else {
        // Disable: free checkpoint activations but keep arrays allocated
        // for potential re-enable
        if (cache->checkpoint_activations) {
            for (uint32_t l = 0; l < cache->num_layers; l++) {
                if (cache->checkpoint_activations[l]) {
                    nimcp_gpu_tensor_destroy(cache->checkpoint_activations[l]);
                    cache->checkpoint_activations[l] = NULL;
                }
            }
        }
        cache->gradient_checkpointing = false;
        cache->num_checkpoint_layers = 0;
        NIMCP_LOG_INFO("Gradient checkpointing disabled");
    }

    return true;
}

bool nimcp_gpu_weight_cache_is_gradient_checkpointing(
    const nimcp_gpu_weight_cache_t* cache)
{
    if (!cache) return false;
    return cache->gradient_checkpointing;
}
