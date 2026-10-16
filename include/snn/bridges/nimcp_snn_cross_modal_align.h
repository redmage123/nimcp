/**
 * @file nimcp_snn_cross_modal_align.h
 * @brief Cross-modal temporal alignment for multi-sensory spike synchronization
 *
 * WHAT: Latency compensation between sensory modalities
 * WHY:  Visual (~60ms), auditory (~20ms), somatosensory (~40ms), and speech
 *        (~50ms) signals arrive at different times; alignment is needed for
 *        coherent multi-sensory binding
 * HOW:  Ring buffer per modality with timestamps; alignment offsets computed
 *        from slowest modality; spikes delivered at unified timestamp
 *
 * BIOLOGICAL BASIS:
 * - Superior colliculus performs multi-sensory alignment in real time
 * - Temporal binding window ~50-100ms for audiovisual integration
 * - Inverse effectiveness: weak unimodal -> stronger cross-modal enhancement
 *
 * @author NIMCP Development Team
 * @date 2026-03-09
 * @version 1.0.0
 */

#ifndef NIMCP_SNN_CROSS_MODAL_ALIGN_H
#define NIMCP_SNN_CROSS_MODAL_ALIGN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

//=============================================================================
// Constants
//=============================================================================

#ifndef CROSS_MODAL_MAX_MODALITIES
#define CROSS_MODAL_MAX_MODALITIES   8
#endif
#define CROSS_MODAL_RING_CAPACITY    64
#define CROSS_MODAL_NAME_LEN         32

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Per-modality latency descriptor
 *
 * WHAT: Latency characteristics for one sensory modality
 * WHY:  Each modality has inherent + processing delays
 * HOW:  Total = inherent + processing; alignment offset derived from max
 */
typedef struct cross_modal_latency_s {
    char     name[CROSS_MODAL_NAME_LEN];  /**< Modality name (e.g., "visual") */
    float    inherent_latency_ms;         /**< Neural transmission latency (ms) */
    float    processing_latency_ms;       /**< Cortical processing latency (ms) */
    float    total_latency_ms;            /**< inherent + processing (computed) */
    float    stdp_tau_ms;                 /**< Modality-specific STDP time constant */
} cross_modal_latency_t;

/**
 * @brief Single spike event in the ring buffer
 */
typedef struct cross_modal_spike_event_s {
    float    timestamp_ms;   /**< Submission timestamp */
    float*   rates;          /**< Spike rates (heap-allocated, dim = spike_dim) */
    uint32_t spike_dim;      /**< Dimension of spike rate vector */
    bool     valid;          /**< Entry contains data */
} cross_modal_spike_event_t;

/**
 * @brief Per-modality ring buffer
 */
typedef struct cross_modal_ring_s {
    cross_modal_spike_event_t events[CROSS_MODAL_RING_CAPACITY];
    uint32_t head;           /**< Next write index */
    uint32_t count;          /**< Number of valid entries */
} cross_modal_ring_t;

/**
 * @brief Alignment statistics
 */
typedef struct cross_modal_align_stats_s {
    uint64_t spikes_submitted;       /**< Total submissions across modalities */
    uint64_t alignments_performed;   /**< Total alignment queries */
    float    avg_alignment_offset_ms;/**< Average offset applied */
    float    max_latency_ms;         /**< Maximum latency encountered */
} cross_modal_align_stats_t;

/**
 * @brief Cross-modal alignment configuration
 *
 * WHAT: Parameters for multi-sensory temporal alignment
 * WHY:  Control alignment window and prediction behavior
 */
typedef struct cross_modal_align_config_s {
    uint32_t num_modalities;         /**< Number of modalities (set at init) */
    float    alignment_window_ms;    /**< Max window for buffered alignment */
    bool     enable_prediction;      /**< Extrapolate missing modalities */
} cross_modal_align_config_t;

/**
 * @brief Cross-modal temporal alignment context
 *
 * WHAT: Manages spike buffers across modalities for temporal alignment
 * WHY:  Enable coherent multi-sensory binding despite latency differences
 * HOW:  Per-modality ring buffers + computed alignment offsets
 */
typedef struct cross_modal_align_s {
    cross_modal_align_config_t config;

    /* Per-modality data */
    cross_modal_latency_t latencies[CROSS_MODAL_MAX_MODALITIES];
    cross_modal_ring_t    rings[CROSS_MODAL_MAX_MODALITIES];
    float                 offsets[CROSS_MODAL_MAX_MODALITIES];  /**< Delay offsets (ms) */
    uint32_t              num_registered;                        /**< Registered modalities */

    /* State */
    float max_total_latency_ms;   /**< Slowest modality latency */
    bool  offsets_valid;          /**< Offsets have been computed */

    /* Statistics */
    cross_modal_align_stats_t stats;

} cross_modal_align_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Initialize alignment config with defaults
 *
 * WHAT: Sensible defaults for multi-sensory alignment
 * WHY:  Convenient initialization
 * HOW:  100ms window, prediction disabled
 *
 * @param config Config to initialize
 */
void cross_modal_align_config_default(cross_modal_align_config_t* config);

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Create cross-modal alignment context
 *
 * WHAT: Allocate and initialize alignment buffers
 * WHY:  Enable multi-sensory temporal compensation
 * HOW:  Allocate ring buffers per modality, init offsets
 *
 * @param config Alignment configuration
 * @return Alignment context or NULL on failure
 */
cross_modal_align_t* cross_modal_align_create(
    const cross_modal_align_config_t* config);

/**
 * @brief Destroy alignment context
 *
 * @param aligner Context to destroy
 */
void cross_modal_align_destroy(cross_modal_align_t* aligner);

//=============================================================================
// Modality Registration
//=============================================================================

/**
 * @brief Register a sensory modality with its latency
 *
 * WHAT: Add a modality to the alignment system
 * WHY:  Each modality must declare its latency for offset computation
 * HOW:  Store latency, invalidate cached offsets
 *
 * @param aligner Alignment context
 * @param name Modality name (e.g., "visual")
 * @param inherent_latency_ms Neural transmission delay
 * @param processing_latency_ms Cortical processing delay
 * @param stdp_tau_ms Modality-specific STDP tau (ms)
 * @return Modality index (>=0) or -1 on failure
 */
int cross_modal_align_register_modality(
    cross_modal_align_t* aligner,
    const char* name,
    float inherent_latency_ms,
    float processing_latency_ms,
    float stdp_tau_ms
);

//=============================================================================
// Spike Submission
//=============================================================================

/**
 * @brief Submit spike rates for a modality at a given timestamp
 *
 * WHAT: Store spikes in the modality's ring buffer
 * WHY:  Buffer spikes until alignment is requested
 * HOW:  Append to ring buffer with timestamp
 *
 * @param aligner Alignment context
 * @param modality_idx Modality index from register
 * @param rates Spike rate vector (copied)
 * @param spike_dim Dimension of rate vector
 * @param timestamp_ms Submission time
 * @return 0 on success, -1 on failure
 */
int cross_modal_align_submit_spikes(
    cross_modal_align_t* aligner,
    uint32_t modality_idx,
    const float* rates,
    uint32_t spike_dim,
    float timestamp_ms
);

//=============================================================================
// Alignment
//=============================================================================

/**
 * @brief Compute alignment offsets for all modalities
 *
 * WHAT: Calculate delay offsets so all modalities align to slowest
 * WHY:  Fast modalities must wait for slow ones
 * HOW:  offset[i] = max_total_latency - latency[i]
 *
 * @param aligner Alignment context
 * @return 0 on success, -1 on failure
 */
int cross_modal_align_compute_offsets(cross_modal_align_t* aligner);

/**
 * @brief Get aligned spikes at a unified timestamp
 *
 * WHAT: Retrieve spikes from all modalities at the same effective time
 * WHY:  Produce temporally coherent multi-sensory representation
 * HOW:  For each modality, find the spike event whose (timestamp + offset)
 *        best matches the requested aligned_timestamp
 *
 * @param aligner Alignment context
 * @param aligned_timestamp Target time (ms)
 * @param output_rates Array of float* pointers, one per modality (caller-allocated)
 * @param output_dims Array of uint32_t, dimension per modality (filled by callee)
 * @param num_modalities Number of entries in output arrays
 * @return Number of modalities that had aligned data, or -1 on error
 */
int cross_modal_align_get_aligned_spikes(
    cross_modal_align_t* aligner,
    float aligned_timestamp,
    float** output_rates,
    uint32_t* output_dims,
    uint32_t num_modalities
);

//=============================================================================
// Statistics
//=============================================================================

int cross_modal_align_get_stats(
    const cross_modal_align_t* aligner,
    cross_modal_align_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_CROSS_MODAL_ALIGN_H */
