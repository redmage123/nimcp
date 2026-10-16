//=============================================================================
// nimcp_neuron_synapse_access.h - Accessor macros for sparse synapse storage
//=============================================================================
/**
 * @file nimcp_neuron_synapse_access.h
 * @brief Accessor macros and inline functions for neuron synapse access
 *
 * WHAT: Unified interface for accessing neuron synapses via sparse storage
 * WHY:  Decouple callers from storage layout (fixed array vs sparse)
 * HOW:  Macros delegate to sparse_synapse_* API functions
 *
 * THREE TIERS:
 * - Tier 1 (handle-level): No network context needed. Count, handle access.
 * - Tier 2 (hot metadata): Needs network for metadata pool lookup.
 * - Tier 3 (cold metadata): Needs network for cold pool lookup. Lazy allocation.
 */

#ifndef NIMCP_NEURON_SYNAPSE_ACCESS_H
#define NIMCP_NEURON_SYNAPSE_ACCESS_H

#include "core/neuralnet/nimcp_sparse_synapse.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Tier 1: Handle-level access (no network context needed)
//=============================================================================

/** Get outgoing synapse count */
#define NEURON_OUT_COUNT(n) \
    sparse_synapse_count(&(n)->outgoing)

/** Get incoming synapse count */
#define NEURON_IN_COUNT(n) \
    sparse_synapse_count(&(n)->incoming)

/** Get outgoing synapse handle by index */
#define NEURON_OUT_HANDLE(n, i) \
    sparse_synapse_get(&(n)->outgoing, (i))

/** Get incoming synapse handle by index */
#define NEURON_IN_HANDLE(n, i) \
    sparse_synapse_get(&(n)->incoming, (i))

//=============================================================================
// Tier 2: Hot synapse_t metadata access (needs network for pool)
//=============================================================================

/**
 * Macro for hot metadata access — requires network->synapse_metadata_pool
 * Usage: synapse_t* syn = NEURON_OUT_META(network, neuron, i);
 */
#define NEURON_OUT_META(net, n, i) \
    _neuron_meta_from_pool((net)->synapse_metadata_pool, &(n)->outgoing, (i))

#define NEURON_IN_META(net, n, i) \
    _neuron_meta_from_pool((net)->synapse_metadata_pool, &(n)->incoming, (i))

/**
 * @brief Internal helper: get hot metadata from pool via storage index
 */
static inline synapse_t* _neuron_meta_from_pool(
    synapse_metadata_pool_t pool,
    sparse_synapse_storage_t* storage,
    uint32_t i
) {
    synapse_handle_t* h = sparse_synapse_get(storage, i);
    if (!h || h->metadata_index == SPARSE_SYNAPSE_NO_METADATA) return NULL;
    return synapse_metadata_pool_get(pool, h->metadata_index);
}

//=============================================================================
// Tier 3: Cold synapse_cold_t access (lazy allocation)
//=============================================================================

/**
 * Get cold data for a synapse (NULL if none allocated)
 * Usage: synapse_cold_t* cold = SYNAPSE_COLD(network, syn);
 */
#define SYNAPSE_COLD(net, syn) \
    _synapse_get_cold((net)->synapse_cold_pool, (syn))

/**
 * Get or allocate cold data for a synapse (lazy init)
 * Usage: synapse_cold_t* cold = SYNAPSE_ENSURE_COLD(network, syn);
 */
#define SYNAPSE_ENSURE_COLD(net, syn) \
    _synapse_ensure_cold((net)->synapse_cold_pool, (syn))

static inline synapse_cold_t* _synapse_get_cold(
    synapse_cold_pool_t cold_pool,
    synapse_t* hot
) {
    if (!hot || hot->cold_index == SYNAPSE_COLD_NONE) return NULL;
    return synapse_cold_pool_get(cold_pool, hot->cold_index);
}

static inline synapse_cold_t* _synapse_ensure_cold(
    synapse_cold_pool_t cold_pool,
    synapse_t* hot
) {
    if (!hot) return NULL;
    if (hot->cold_index == SYNAPSE_COLD_NONE) {
        hot->cold_index = synapse_cold_pool_allocate(cold_pool);
        if (hot->cold_index == SYNAPSE_COLD_NONE) return NULL;
        synapse_cold_t* cold = synapse_cold_pool_get(cold_pool, hot->cold_index);
        if (cold) memset(cold, 0, sizeof(synapse_cold_t));
        return cold;
    }
    return synapse_cold_pool_get(cold_pool, hot->cold_index);
}

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NEURON_SYNAPSE_ACCESS_H
