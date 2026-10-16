/**
 * @file nimcp_snn_cross_modal_align.c
 * @brief Cross-modal temporal alignment implementation
 *
 * WHAT: Latency compensation between sensory modalities
 * WHY:  Multi-sensory binding requires temporal coherence despite
 *        different neural transmission and processing delays
 * HOW:  Per-modality ring buffers + alignment offsets from slowest modality
 *
 * BIOLOGICAL BASIS:
 * - Superior colliculus aligns visual, auditory, and somatosensory inputs
 * - Temporal binding window ~50-100ms for cross-modal integration
 * - Default latencies: visual=60ms, auditory=20ms, somatosensory=40ms, speech=50ms
 *
 * @author NIMCP Development Team
 * @date 2026-03-09
 */

#include "snn/bridges/nimcp_snn_cross_modal_align.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "cross_modal_align"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cross_modal_align)

//=============================================================================
// Default Configuration
//=============================================================================

void cross_modal_align_config_default(cross_modal_align_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cross_modal_align_config_default: null config");
        return;
    }

    config->num_modalities    = 0;  /* Set by register calls */
    config->alignment_window_ms = 100.0f;
    config->enable_prediction = false;
}

//=============================================================================
// Lifecycle
//=============================================================================

cross_modal_align_t* cross_modal_align_create(
    const cross_modal_align_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cross_modal_align_create: null config");
        return NULL;
    }

    cross_modal_align_t* aligner = nimcp_calloc(1, sizeof(*aligner));
    if (!aligner) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_MEMORY,
            "cross_modal_align_create: failed to allocate aligner");
        return NULL;
    }

    bbb_register_module("cross_modal_align", BBB_MODULE_TYPE_COGNITIVE);

    aligner->config = *config;
    aligner->num_registered = 0;
    aligner->max_total_latency_ms = 0.0f;
    aligner->offsets_valid = false;
    memset(&aligner->stats, 0, sizeof(aligner->stats));

    /* Initialize ring buffers */
    for (uint32_t i = 0; i < CROSS_MODAL_MAX_MODALITIES; i++) {
        aligner->rings[i].head = 0;
        aligner->rings[i].count = 0;
        for (uint32_t j = 0; j < CROSS_MODAL_RING_CAPACITY; j++) {
            aligner->rings[i].events[j].rates = NULL;
            aligner->rings[i].events[j].valid = false;
        }
    }

    LOG_INFO(LOG_MODULE, "Created: window=%.0fms, prediction=%s",
             config->alignment_window_ms,
             config->enable_prediction ? "on" : "off");
    return aligner;
}

void cross_modal_align_destroy(cross_modal_align_t* aligner)
{
    if (!aligner) return;

    /* Free all ring buffer spike data */
    for (uint32_t i = 0; i < CROSS_MODAL_MAX_MODALITIES; i++) {
        for (uint32_t j = 0; j < CROSS_MODAL_RING_CAPACITY; j++) {
            nimcp_free(aligner->rings[i].events[j].rates);
        }
    }

    nimcp_free(aligner);
}

//=============================================================================
// Modality Registration
//=============================================================================

int cross_modal_align_register_modality(
    cross_modal_align_t* aligner,
    const char* name,
    float inherent_latency_ms,
    float processing_latency_ms,
    float stdp_tau_ms)
{
    if (!aligner) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cross_modal_align_register_modality: null aligner");
        return -1;
    }
    if (!name) return -1;
    if (aligner->num_registered >= CROSS_MODAL_MAX_MODALITIES) return -1;

    uint32_t idx = aligner->num_registered;
    cross_modal_latency_t* lat = &aligner->latencies[idx];

    strncpy(lat->name, name, CROSS_MODAL_NAME_LEN - 1);
    lat->name[CROSS_MODAL_NAME_LEN - 1] = '\0';
    lat->inherent_latency_ms  = inherent_latency_ms;
    lat->processing_latency_ms = processing_latency_ms;
    lat->total_latency_ms     = inherent_latency_ms + processing_latency_ms;
    lat->stdp_tau_ms           = stdp_tau_ms;

    /* Track max latency for offset computation */
    if (lat->total_latency_ms > aligner->max_total_latency_ms) {
        aligner->max_total_latency_ms = lat->total_latency_ms;
    }

    aligner->num_registered++;
    aligner->offsets_valid = false;  /* Invalidate cached offsets */

    LOG_INFO(LOG_MODULE, "Registered modality[%u]: '%s' latency=%.0fms, stdp_tau=%.0fms",
             idx, name, lat->total_latency_ms, stdp_tau_ms);
    return (int)idx;
}

//=============================================================================
// Spike Submission
//=============================================================================

int cross_modal_align_submit_spikes(
    cross_modal_align_t* aligner,
    uint32_t modality_idx,
    const float* rates,
    uint32_t spike_dim,
    float timestamp_ms)
{
    if (!aligner) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cross_modal_align_submit_spikes: null aligner");
        return -1;
    }
    if (!rates) return -1;
    if (modality_idx >= aligner->num_registered) return -1;
    if (spike_dim == 0) return -1;

    cross_modal_ring_t* ring = &aligner->rings[modality_idx];
    uint32_t slot = ring->head;

    /* Free old data in this slot */
    nimcp_free(ring->events[slot].rates);

    /* Copy spike rates */
    float* rates_copy = nimcp_calloc(spike_dim, sizeof(float));
    if (!rates_copy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_MEMORY,
            "cross_modal_align_submit_spikes: failed to allocate rates copy");
        return -1;
    }
    memcpy(rates_copy, rates, spike_dim * sizeof(float));

    ring->events[slot].rates = rates_copy;
    ring->events[slot].spike_dim = spike_dim;
    ring->events[slot].timestamp_ms = timestamp_ms;
    ring->events[slot].valid = true;

    ring->head = (ring->head + 1) % CROSS_MODAL_RING_CAPACITY;
    if (ring->count < CROSS_MODAL_RING_CAPACITY) ring->count++;

    aligner->stats.spikes_submitted++;

    return 0;
}

//=============================================================================
// Alignment Computation
//=============================================================================

int cross_modal_align_compute_offsets(cross_modal_align_t* aligner)
{
    if (!aligner) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cross_modal_align_compute_offsets: null aligner");
        return -1;
    }
    if (aligner->num_registered == 0) return -1;

    /* Find the slowest modality (max total latency) */
    float max_lat = 0.0f;
    for (uint32_t i = 0; i < aligner->num_registered; i++) {
        if (aligner->latencies[i].total_latency_ms > max_lat) {
            max_lat = aligner->latencies[i].total_latency_ms;
        }
    }
    aligner->max_total_latency_ms = max_lat;

    /* Compute offset: faster modalities must be delayed */
    float offset_sum = 0.0f;
    for (uint32_t i = 0; i < aligner->num_registered; i++) {
        aligner->offsets[i] = max_lat - aligner->latencies[i].total_latency_ms;
        offset_sum += aligner->offsets[i];
    }

    aligner->offsets_valid = true;
    aligner->stats.max_latency_ms = max_lat;
    if (aligner->num_registered > 0) {
        aligner->stats.avg_alignment_offset_ms =
            offset_sum / (float)aligner->num_registered;
    }

    LOG_INFO(LOG_MODULE, "Offsets computed: max_latency=%.0fms, avg_offset=%.1fms",
             max_lat, aligner->stats.avg_alignment_offset_ms);

    return 0;
}

//=============================================================================
// Aligned Spike Retrieval
//=============================================================================

/**
 * WHAT: Find the ring buffer event closest to a target effective time
 * WHY:  Need the spike event that, after offset, best matches the request
 * HOW:  Linear scan of valid entries, pick smallest |effective_time - target|
 */
static const cross_modal_spike_event_t* find_closest_event(
    const cross_modal_ring_t* ring,
    float target_time_ms,
    float offset_ms,
    float window_ms)
{
    const cross_modal_spike_event_t* best = NULL;
    float best_diff = window_ms;  /* Don't match outside window */

    for (uint32_t i = 0; i < CROSS_MODAL_RING_CAPACITY; i++) {
        if (!ring->events[i].valid) continue;

        /* Effective time = original timestamp + alignment offset */
        float effective = ring->events[i].timestamp_ms + offset_ms;
        float diff = fabsf(effective - target_time_ms);

        if (diff < best_diff) {
            best_diff = diff;
            best = &ring->events[i];
        }
    }

    return best;
}

int cross_modal_align_get_aligned_spikes(
    cross_modal_align_t* aligner,
    float aligned_timestamp,
    float** output_rates,
    uint32_t* output_dims,
    uint32_t num_modalities)
{
    if (!aligner) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cross_modal_align_get_aligned_spikes: null aligner");
        return -1;
    }
    if (!output_rates || !output_dims) return -1;

    /* Ensure offsets are computed */
    if (!aligner->offsets_valid) {
        if (cross_modal_align_compute_offsets(aligner) != 0) return -1;
    }

    uint32_t n = aligner->num_registered;
    if (num_modalities < n) n = num_modalities;

    int found = 0;
    for (uint32_t i = 0; i < n; i++) {
        const cross_modal_spike_event_t* evt = find_closest_event(
            &aligner->rings[i],
            aligned_timestamp,
            aligner->offsets[i],
            aligner->config.alignment_window_ms
        );

        if (evt) {
            output_rates[i] = evt->rates;  /* Borrowed pointer */
            output_dims[i]  = evt->spike_dim;
            found++;
        } else {
            output_rates[i] = NULL;
            output_dims[i]  = 0;
        }
    }

    /* Zero remaining slots */
    for (uint32_t i = n; i < num_modalities; i++) {
        output_rates[i] = NULL;
        output_dims[i]  = 0;
    }

    aligner->stats.alignments_performed++;

    return found;
}

//=============================================================================
// Statistics
//=============================================================================

int cross_modal_align_get_stats(
    const cross_modal_align_t* aligner,
    cross_modal_align_stats_t* stats)
{
    if (!aligner) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cross_modal_align_get_stats: null aligner");
        return -1;
    }
    if (!stats) return -1;
    *stats = aligner->stats;
    return 0;
}
