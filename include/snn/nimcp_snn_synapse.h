/**
 * @file nimcp_snn_synapse.h
 * @brief Lightweight CSR (Compressed Sparse Row) synapse storage for SNN
 *
 * WHAT: Cache-friendly synapse storage that replaces the heavy neuron_t
 *       sparse_synapse_storage_t (12KB per neuron) with a compact CSR
 *       format (~12 bytes per synapse).
 * WHY:  neuron_t costs 14KB each; 1.8M SNN neurons = 25 GB. CSR reduces
 *       this to ~1 GB for the same connectivity.
 * HOW:  Per-population CSR stores incoming synapses. For neuron i,
 *       synapses are at entries[row_ptr[i]..row_ptr[i+1]).
 *       Built in COO (coordinate) mode, then finalized to CSR.
 */

#ifndef NIMCP_SNN_SYNAPSE_H
#define NIMCP_SNN_SYNAPSE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Single incoming synapse in CSR format (12 bytes)
 */
typedef struct {
    uint32_t src_pop;       /**< Source population index in network */
    uint32_t src_neuron;    /**< Neuron index within source population */
    float    weight;        /**< Synaptic weight (mutable for learning) */
} snn_csr_synapse_t;

/**
 * @brief CSR incoming synapse storage for one population.
 *
 * Two modes:
 * - COO (build) mode: entries appended unsorted, row_ptr unused.
 *   Call snn_csr_finalize() to sort and build row_ptr.
 * - CSR (query) mode: entries sorted by dst neuron, row_ptr valid.
 *   snn_csr_get_incoming() returns O(1) per neuron.
 */
typedef struct snn_csr_storage_s {
    uint32_t*          row_ptr;    /**< [n_neurons + 1] row start indices */
    snn_csr_synapse_t* entries;    /**< [n_synapses] flat synapse array */
    uint32_t           n_neurons;  /**< Number of rows (neurons in this pop) */
    uint32_t           n_synapses; /**< Number of entries used */
    uint32_t           capacity;   /**< Allocated capacity of entries[] */
    bool               finalized;  /**< true after snn_csr_finalize() */

    /* GPU-ready flat arrays (populated by snn_csr_prepare_gpu) */
    float*             weights;         /**< [n_synapses] synapse weights (flat) */
    uint32_t*          flat_col_idx;    /**< [n_synapses] flat global neuron index */
    uint32_t*          src_pop_idx;     /**< [n_synapses] source pop index (CB-GPU-7) */
    bool               gpu_ready;       /**< true after snn_csr_prepare_gpu() */

    /* === V2: Persistent GPU-resident copies of CSR data ===
     * These are populated once by snn_csr_upload_to_gpu() and live on
     * the device for the population's lifetime. The isyn kernel reads
     * directly from them without per-timestep PCIe transfers.
     *
     * Lifecycle:
     *   - NULL initially
     *   - Allocated by snn_csr_upload_to_gpu() after finalize
     *   - Freed by snn_csr_release_gpu() or snn_csr_destroy()
     *   - Weight updates during training sync H→D via snn_csr_sync_weights_to_gpu()
     *
     * NOTE: d_gpu_ctx stores the context used at upload time, so release
     * and sync can free/copy without needing the caller to re-pass it.
     * The brain's gpu_ctx lifetime is guaranteed to outlive the CSR.
     */
    void*              d_weights;       /**< Device pointer for weights[] (float*) */
    void*              d_flat_col_idx;  /**< Device pointer for flat_col_idx[] (uint32_t*) */
    void*              d_row_ptr;       /**< Device pointer for row_ptr[] (uint32_t*) */
    void*              d_src_pop_idx;   /**< Device pointer for src_pop_idx[] (uint32_t*); CB-GPU-7 deposit kernel routing */
    void*              d_gpu_ctx;       /**< nimcp_gpu_context_t* used for upload */
    bool               gpu_resident;    /**< true after snn_csr_upload_to_gpu() */
} snn_csr_storage_t;

/* ========================================================================= */
/* GPU residency API (V2)                                                     */
/* ========================================================================= */

/**
 * @brief Upload CSR arrays to GPU and keep them resident.
 *
 * Eliminates per-timestep ~12 GB PCIe transfer for populations with
 * ~1.45B synapses. After this call, d_weights/d_flat_col_idx/d_row_ptr
 * are valid device pointers that persist until release or destroy.
 *
 * @param csr Storage (must be finalized)
 * @param gpu_ctx GPU context (opaque pointer to nimcp_gpu_context_t)
 * @return 0 on success, -1 on error
 */
int snn_csr_upload_to_gpu(snn_csr_storage_t* csr, void* gpu_ctx);

/**
 * @brief Sync host weights[] to GPU device memory.
 *
 * Call after training updates the weights[] array to keep GPU copy fresh.
 * Partial syncs (by range) may be added later; currently copies the full array.
 *
 * @param csr Storage (must be gpu_resident)
 * @return 0 on success, -1 on error
 */
int snn_csr_sync_weights_to_gpu(snn_csr_storage_t* csr);

/**
 * @brief Release GPU-resident arrays.
 *
 * @param csr Storage (safe if gpu_resident=false)
 */
void snn_csr_release_gpu(snn_csr_storage_t* csr);

/* ========================================================================= */
/* Lifecycle                                                                  */
/* ========================================================================= */

/**
 * @brief Create CSR storage for a population
 * @param n_neurons Number of neurons (rows)
 * @param estimated_synapses Initial capacity hint (grows if needed)
 * @return Storage handle, or NULL on error
 */
snn_csr_storage_t* snn_csr_create(uint32_t n_neurons, uint32_t estimated_synapses);

/**
 * @brief Destroy CSR storage and free all memory
 * @param csr Storage to destroy (NULL-safe)
 */
void snn_csr_destroy(snn_csr_storage_t* csr);

/* ========================================================================= */
/* Build phase (COO mode)                                                     */
/* ========================================================================= */

/**
 * @brief Append a synapse entry (COO mode — unsorted)
 * @param csr Storage (must not be finalized)
 * @param dst_neuron Destination neuron index within this population
 * @param src_pop Source population index
 * @param src_neuron Source neuron index within source population
 * @param weight Synaptic weight
 * @return 0 on success, -1 on error
 */
int snn_csr_add_entry(snn_csr_storage_t* csr,
                      uint32_t dst_neuron,
                      uint32_t src_pop,
                      uint32_t src_neuron,
                      float weight);

/**
 * @brief Sort entries by destination neuron and build row_ptr
 *
 * After this call, snn_csr_get_incoming() is valid.
 * No more entries can be added.
 *
 * @param csr Storage to finalize
 * @return 0 on success, -1 on error
 */
int snn_csr_finalize(snn_csr_storage_t* csr);

/**
 * @brief Re-open a finalized CSR so more snn_csr_add_entry calls can append
 *        (runtime synaptogenesis), then the caller re-finalizes.
 *
 * Re-expands the compacted CSR entries back to COO (so add_entry's 16-byte
 * layout assumption holds), invalidates the derived host arrays
 * (weights/flat_col_idx/src_pop_idx) + the GPU mirror. Caller MUST re-finalize
 * (and re-prepare/upload) afterward, and MUST ensure no SNN step is in flight
 * (CB-GPU-7 invariant — same as connect_populations).
 *
 * @param csr Finalized storage to re-open (no-op if already in build mode)
 * @return 0 on success, -1 on error
 */
int snn_csr_reopen(snn_csr_storage_t* csr);

/* ========================================================================= */
/* Query phase (CSR mode — after finalize)                                    */
/* ========================================================================= */

/**
 * @brief Get incoming synapses for a neuron (O(1) lookup)
 * @param csr Finalized CSR storage
 * @param neuron_idx Neuron index within population
 * @param[out] count Number of incoming synapses
 * @return Pointer to first synapse entry, or NULL if none
 */
static inline snn_csr_synapse_t* snn_csr_get_incoming(
    const snn_csr_storage_t* csr,
    uint32_t neuron_idx,
    uint32_t* count)
{
    if (!csr || !csr->finalized || neuron_idx >= csr->n_neurons) {
        if (count) *count = 0;
        return NULL;
    }
    uint32_t start = csr->row_ptr[neuron_idx];
    uint32_t end   = csr->row_ptr[neuron_idx + 1];
    if (count) *count = end - start;
    return (end > start) ? &csr->entries[start] : NULL;
}

/* ========================================================================= */
/* GPU preparation (extract flat weight/index arrays for SpMV)                */
/* ========================================================================= */

/**
 * @brief Prepare GPU-friendly flat arrays from CSR entries.
 *
 * Extracts separate weights[] and flat_col_idx[] arrays from the
 * snn_csr_synapse_t entries. flat_col_idx maps (src_pop, src_neuron)
 * to a global flat index using the provided population offsets.
 *
 * @param csr Finalized CSR storage
 * @param pop_offsets [n_populations] flat offset for each population
 * @param n_populations Number of populations
 * @return 0 on success, -1 on error
 */
int snn_csr_prepare_gpu(snn_csr_storage_t* csr,
                        const uint32_t* pop_offsets,
                        uint32_t n_populations);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_SYNAPSE_H */
