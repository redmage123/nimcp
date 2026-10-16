/**
 * @file nimcp_snn_synapse.c
 * @brief CSR synapse storage implementation for lightweight SNN populations
 *
 * Build flow:
 * 1. snn_csr_create() — allocate storage
 * 2. snn_csr_add_entry() × N — append entries in any order (COO mode)
 * 3. snn_csr_finalize() — sort by dst neuron, build row_ptr (CSR mode)
 * 4. snn_csr_get_incoming() — O(1) lookup per neuron
 */

#include "snn/nimcp_snn_synapse.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>
#include <stdlib.h>

#define LOG_MODULE "SNN_CSR"

/* COO entry with destination tag for sorting */
typedef struct {
    uint32_t dst_neuron;
    snn_csr_synapse_t syn;
} coo_tagged_entry_t;

/* ========================================================================= */
/* Lifecycle                                                                  */
/* ========================================================================= */

snn_csr_storage_t* snn_csr_create(uint32_t n_neurons, uint32_t estimated_synapses)
{
    if (n_neurons == 0) return NULL;
    if (estimated_synapses == 0) estimated_synapses = n_neurons * 10;

    snn_csr_storage_t* csr = nimcp_calloc(1, sizeof(snn_csr_storage_t));
    if (!csr) return NULL;

    csr->n_neurons = n_neurons;
    csr->n_synapses = 0;
    csr->capacity = estimated_synapses;
    csr->finalized = false;

    csr->row_ptr = nimcp_calloc(n_neurons + 1, sizeof(uint32_t));

    /* During COO phase, entries stores coo_tagged_entry_t (16 bytes each).
     * After finalize, entries stores snn_csr_synapse_t (12 bytes each).
     * Allocate for the larger of the two. */
    size_t entry_size = sizeof(coo_tagged_entry_t);  /* 16 bytes */
    csr->entries = nimcp_malloc(estimated_synapses * entry_size);

    if (!csr->row_ptr || !csr->entries) {
        snn_csr_destroy(csr);
        return NULL;
    }

    return csr;
}

void snn_csr_destroy(snn_csr_storage_t* csr)
{
    if (!csr) return;
    snn_csr_release_gpu(csr);  /* Free device memory first */
    nimcp_free(csr->row_ptr);
    nimcp_free(csr->entries);
    nimcp_free(csr->weights);
    nimcp_free(csr->flat_col_idx);
    nimcp_free(csr->src_pop_idx);
    nimcp_free(csr);
}

/* ========================================================================= */
/* Build phase (COO mode — entries are coo_tagged_entry_t)                    */
/* ========================================================================= */

int snn_csr_add_entry(snn_csr_storage_t* csr,
                      uint32_t dst_neuron,
                      uint32_t src_pop,
                      uint32_t src_neuron,
                      float weight)
{
    if (!csr || csr->finalized) return -1;
    if (dst_neuron >= csr->n_neurons) return -1;

    /* Grow if needed */
    if (csr->n_synapses >= csr->capacity) {
        uint32_t new_cap = csr->capacity * 2;
        if (new_cap < csr->capacity + 4096) new_cap = csr->capacity + 4096;
        void* new_entries = nimcp_realloc(
            csr->entries, (size_t)new_cap * sizeof(coo_tagged_entry_t));
        if (!new_entries) return -1;
        csr->entries = new_entries;
        csr->capacity = new_cap;
    }

    /* Store as tagged COO entry */
    coo_tagged_entry_t* tagged = (coo_tagged_entry_t*)csr->entries;
    tagged[csr->n_synapses].dst_neuron = dst_neuron;
    tagged[csr->n_synapses].syn.src_pop = src_pop;
    tagged[csr->n_synapses].syn.src_neuron = src_neuron;
    tagged[csr->n_synapses].syn.weight = weight;
    csr->n_synapses++;

    return 0;
}

/* qsort comparator: sort by dst_neuron */
static int coo_compare(const void* a, const void* b)
{
    const coo_tagged_entry_t* ea = (const coo_tagged_entry_t*)a;
    const coo_tagged_entry_t* eb = (const coo_tagged_entry_t*)b;
    if (ea->dst_neuron < eb->dst_neuron) return -1;
    if (ea->dst_neuron > eb->dst_neuron) return 1;
    return 0;
}

int snn_csr_finalize(snn_csr_storage_t* csr)
{
    if (!csr || csr->finalized) return -1;

    uint32_t nnz = csr->n_synapses;

    if (nnz == 0) {
        /* Empty — just zero row_ptr */
        memset(csr->row_ptr, 0, (csr->n_neurons + 1) * sizeof(uint32_t));
        csr->finalized = true;
        return 0;
    }

    /* Step 1: Sort COO entries by dst_neuron */
    coo_tagged_entry_t* tagged = (coo_tagged_entry_t*)csr->entries;
    qsort(tagged, nnz, sizeof(coo_tagged_entry_t), coo_compare);

    /* Step 2: Build row_ptr from sorted entries */
    memset(csr->row_ptr, 0, (csr->n_neurons + 1) * sizeof(uint32_t));
    for (uint32_t i = 0; i < nnz; i++) {
        csr->row_ptr[tagged[i].dst_neuron + 1]++;
    }
    for (uint32_t i = 0; i < csr->n_neurons; i++) {
        csr->row_ptr[i + 1] += csr->row_ptr[i];
    }

    /* Step 3: Extract synapse data from tagged entries into compact format.
     * We can do this in-place since snn_csr_synapse_t (12 bytes) is smaller
     * than coo_tagged_entry_t (16 bytes), and entries are already sorted. */
    snn_csr_synapse_t* compact = (snn_csr_synapse_t*)csr->entries;
    for (uint32_t i = 0; i < nnz; i++) {
        compact[i] = tagged[i].syn;  /* Copy 12 bytes from 16-byte slot */
    }

    /* Step 4: Shrink allocation if significantly oversized */
    if (csr->capacity > nnz * 2 && nnz > 1024) {
        snn_csr_synapse_t* shrunk = nimcp_realloc(
            csr->entries, nnz * sizeof(snn_csr_synapse_t));
        if (shrunk) {
            csr->entries = shrunk;
            csr->capacity = nnz;
        }
    }

    csr->finalized = true;

    LOG_INFO("CSR finalized: %u neurons, %u synapses (%.1f avg/neuron)",
             csr->n_neurons, nnz, (float)nnz / csr->n_neurons);

    return 0;
}

/* Re-open a finalized CSR so more snn_csr_add_entry calls can append, then the
 * caller re-finalizes (and re-prepares/uploads). This is the core of runtime
 * synaptogenesis. finalize() compacts entries[] IN PLACE from 16-byte COO
 * (coo_tagged_entry_t) to 12-byte CSR (snn_csr_synapse_t), and add_entry assumes
 * 16-byte COO slots — so we must RE-EXPAND back to COO first, recovering each
 * synapse's dst_neuron from row_ptr. We also invalidate the derived host arrays
 * (weights/flat_col_idx/src_pop_idx — rebuilt by snn_csr_prepare_gpu after
 * re-finalize) and release the GPU mirror (re-uploaded lazily on the next step).
 *
 * CONCURRENCY: this mutates topology + frees the GPU mirror. The caller MUST
 * ensure no SNN step is in flight (hold the step lock / run in the write-locked,
 * between-steps window — same CB-GPU-7 invariant as connect_populations).
 * Returns 0, or -1 on error. */
int snn_csr_reopen(snn_csr_storage_t* csr)
{
    if (!csr) return -1;
    if (!csr->finalized) return 0;   /* already in COO/build mode */

    const uint32_t nnz = csr->n_synapses;
    if (nnz > 0) {
        /* Re-expand compact 12-byte entries → 16-byte COO. A fresh buffer is
         * required: an in-place 12→16 grow would overwrite source bytes not yet
         * read. dst_neuron is recovered from row_ptr (entries are sorted by dst
         * post-finalize, so row n owns indices [row_ptr[n], row_ptr[n+1])). */
        coo_tagged_entry_t* coo = (coo_tagged_entry_t*)
            nimcp_malloc((size_t)nnz * sizeof(coo_tagged_entry_t));
        if (!coo) return -1;
        const snn_csr_synapse_t* compact = (const snn_csr_synapse_t*)csr->entries;
        uint32_t idx = 0;
        for (uint32_t n = 0; n < csr->n_neurons && idx < nnz; n++) {
            const uint32_t s = csr->row_ptr[n];
            const uint32_t e = csr->row_ptr[n + 1];
            for (uint32_t i = s; i < e && idx < nnz; i++) {
                coo[idx].dst_neuron = n;
                coo[idx].syn = compact[i];
                idx++;
            }
        }
        nimcp_free(csr->entries);
        csr->entries  = coo;
        csr->capacity = nnz;   /* now counted in coo_tagged_entry_t units */
    }

    /* Derived host arrays are stale — free + NULL so prepare_gpu rebuilds them. */
    nimcp_free(csr->weights);      csr->weights = NULL;
    nimcp_free(csr->flat_col_idx); csr->flat_col_idx = NULL;
    nimcp_free(csr->src_pop_idx);  csr->src_pop_idx = NULL;

    /* GPU mirror is stale — free device buffers so the next step re-uploads. */
    snn_csr_release_gpu(csr);

    csr->finalized = false;
    return 0;
}

int snn_csr_prepare_gpu(snn_csr_storage_t* csr,
                        const uint32_t* pop_offsets,
                        uint32_t n_populations)
{
    if (!csr || !csr->finalized || !pop_offsets) return -1;
    if (csr->gpu_ready) return 0;  /* Already prepared */

    uint32_t nnz = csr->n_synapses;
    if (nnz == 0) {
        csr->gpu_ready = true;
        return 0;
    }

    csr->weights = nimcp_malloc(nnz * sizeof(float));
    csr->flat_col_idx = nimcp_malloc(nnz * sizeof(uint32_t));
    csr->src_pop_idx = nimcp_malloc(nnz * sizeof(uint32_t));
    if (!csr->weights || !csr->flat_col_idx || !csr->src_pop_idx) {
        nimcp_free(csr->weights);
        nimcp_free(csr->flat_col_idx);
        nimcp_free(csr->src_pop_idx);
        csr->weights = NULL;
        csr->flat_col_idx = NULL;
        csr->src_pop_idx = NULL;
        return -1;
    }

    for (uint32_t i = 0; i < nnz; i++) {
        csr->weights[i] = csr->entries[i].weight;
        uint32_t sp = csr->entries[i].src_pop;
        uint32_t sn = csr->entries[i].src_neuron;
        /* Both arrays must clamp invalid src_pop to 0 — the CB-GPU-7
         * deposit kernel uses src_pop_idx[i] as a direct index into the
         * synapse_type table (size = n_populations). An out-of-range
         * value would OOB-read in the kernel. The flat_col_idx clamp
         * already handles the spike vector side; mirror it here. */
        if (sp < n_populations) {
            csr->flat_col_idx[i] = pop_offsets[sp] + sn;
            csr->src_pop_idx[i]  = sp;
        } else {
            csr->flat_col_idx[i] = 0;
            csr->src_pop_idx[i]  = 0;
        }
    }

    csr->gpu_ready = true;
    LOG_INFO("CSR GPU prepared: %u synapses, flat column indices built", nnz);
    return 0;
}

/* =========================================================================
 * GPU-resident memory API (V2 — eliminates per-timestep PCIe transfer)
 *
 * The ctx used for upload is stored in csr->d_gpu_ctx so that sync and
 * release paths can use it without the caller re-passing it. The brain
 * guarantees the ctx outlives the CSR.
 * ========================================================================= */

#include "gpu/context/nimcp_gpu_context.h"

int snn_csr_upload_to_gpu(snn_csr_storage_t* csr, void* gpu_ctx_void)
{
    if (!csr || !csr->finalized) return -1;
    if (!csr->gpu_ready) {
        LOG_ERROR("snn_csr_upload_to_gpu: csr not GPU-prepared yet");
        return -1;
    }
    if (csr->gpu_resident) return 0;  /* Already uploaded */
    if (csr->n_synapses == 0) return 0;

    nimcp_gpu_context_t* gpu_ctx = (nimcp_gpu_context_t*)gpu_ctx_void;
    if (!gpu_ctx) {
        LOG_ERROR("snn_csr_upload_to_gpu: NULL gpu context");
        return -1;
    }

    size_t w_bytes  = (size_t)csr->n_synapses * sizeof(float);
    size_t ci_bytes = (size_t)csr->n_synapses * sizeof(uint32_t);
    size_t sp_bytes = (size_t)csr->n_synapses * sizeof(uint32_t);
    size_t rp_bytes = (size_t)(csr->n_neurons + 1) * sizeof(uint32_t);

    /* Allocate first; only store the ctx if all allocations succeed.
     * Otherwise release_gpu (called on failure) would attempt to free
     * with a ctx that had no successful allocations. */
    void* dw = nimcp_gpu_malloc(gpu_ctx, w_bytes);
    void* dc = nimcp_gpu_malloc(gpu_ctx, ci_bytes);
    void* dr = nimcp_gpu_malloc(gpu_ctx, rp_bytes);
    void* dsp = csr->src_pop_idx ? nimcp_gpu_malloc(gpu_ctx, sp_bytes) : NULL;

    if (!dw || !dc || !dr || (csr->src_pop_idx && !dsp)) {
        LOG_ERROR("snn_csr_upload_to_gpu: GPU allocation failed "
                  "(weights=%p, col=%p, row=%p, src_pop=%p)",
                  dw, dc, dr, dsp);
        if (dw)  nimcp_gpu_free(gpu_ctx, dw);
        if (dc)  nimcp_gpu_free(gpu_ctx, dc);
        if (dr)  nimcp_gpu_free(gpu_ctx, dr);
        if (dsp) nimcp_gpu_free(gpu_ctx, dsp);
        return -1;
    }

    csr->d_weights      = dw;
    csr->d_flat_col_idx = dc;
    csr->d_row_ptr      = dr;
    csr->d_src_pop_idx  = dsp;
    csr->d_gpu_ctx      = gpu_ctx;

    if (nimcp_gpu_memcpy(gpu_ctx, csr->d_weights, csr->weights,
                          w_bytes, GPU_MEMCPY_HOST_TO_DEVICE) != 0) goto fail;
    if (nimcp_gpu_memcpy(gpu_ctx, csr->d_flat_col_idx, csr->flat_col_idx,
                          ci_bytes, GPU_MEMCPY_HOST_TO_DEVICE) != 0) goto fail;
    if (nimcp_gpu_memcpy(gpu_ctx, csr->d_row_ptr, csr->row_ptr,
                          rp_bytes, GPU_MEMCPY_HOST_TO_DEVICE) != 0) goto fail;
    if (csr->src_pop_idx && csr->d_src_pop_idx &&
        nimcp_gpu_memcpy(gpu_ctx, csr->d_src_pop_idx, csr->src_pop_idx,
                          sp_bytes, GPU_MEMCPY_HOST_TO_DEVICE) != 0) goto fail;

    csr->gpu_resident = true;
    LOG_INFO("CSR uploaded to GPU: %u synapses (%.1f MB), resident",
             csr->n_synapses,
             (w_bytes + ci_bytes + rp_bytes + (dsp ? sp_bytes : 0))
             / (1024.0 * 1024.0));
    return 0;

fail:
    LOG_ERROR("snn_csr_upload_to_gpu: GPU memcpy failed");
    snn_csr_release_gpu(csr);
    return -1;
}

int snn_csr_sync_weights_to_gpu(snn_csr_storage_t* csr)
{
    if (!csr || !csr->gpu_resident || !csr->d_weights || !csr->weights)
        return -1;
    if (!csr->d_gpu_ctx) return -1;
    size_t w_bytes = (size_t)csr->n_synapses * sizeof(float);
    return nimcp_gpu_memcpy((nimcp_gpu_context_t*)csr->d_gpu_ctx,
                             csr->d_weights, csr->weights,
                             w_bytes, GPU_MEMCPY_HOST_TO_DEVICE);
}

void snn_csr_release_gpu(snn_csr_storage_t* csr)
{
    if (!csr) return;
    nimcp_gpu_context_t* ctx = (nimcp_gpu_context_t*)csr->d_gpu_ctx;
    /* Without a stored ctx, we can't safely free (unknown backend) — leak
     * would be worse than not freeing on the rare context-loss path. */
    if (csr->d_weights && ctx) {
        nimcp_gpu_free(ctx, csr->d_weights);
    }
    if (csr->d_flat_col_idx && ctx) {
        nimcp_gpu_free(ctx, csr->d_flat_col_idx);
    }
    if (csr->d_row_ptr && ctx) {
        nimcp_gpu_free(ctx, csr->d_row_ptr);
    }
    if (csr->d_src_pop_idx && ctx) {
        nimcp_gpu_free(ctx, csr->d_src_pop_idx);
    }
    csr->d_weights = NULL;
    csr->d_flat_col_idx = NULL;
    csr->d_row_ptr = NULL;
    csr->d_src_pop_idx = NULL;
    csr->d_gpu_ctx = NULL;
    csr->gpu_resident = false;
}
