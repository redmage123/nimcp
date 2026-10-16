/**
 * @file nimcp_hippocampus_utils_bridge.h
 * @brief Hippocampus Utils Integration - Ring Buffer, Hash Table, Memory Pools, Metrics, Math
 * @version 1.0.0
 * @date 2026-01-12
 *
 * Comprehensive integration of NIMCP utilities into Hippocampus module:
 * - Ring buffer for replay buffer management (auto wraparound)
 * - Hash table for O(1) episode lookup by ID
 * - Memory pools for cell arrays (DG, CA3, CA1, Subiculum)
 * - Metrics collection for consolidation analytics
 * - FFT/Signal filtering for oscillation analysis
 * - K-D Tree for spatial pattern queries
 * - Numerical integration for place cell dynamics
 */

#ifndef NIMCP_HIPPOCAMPUS_UTILS_BRIDGE_H
#define NIMCP_HIPPOCAMPUS_UTILS_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/hippocampus/nimcp_hippocampus.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define HIPPO_EPISODE_HASH_BUCKETS      1024    /* Hash table buckets */
#define HIPPO_REPLAY_RING_SIZE          128     /* Replay buffer size */
#define HIPPO_CELL_POOL_DG              2048    /* DG cell pool */
#define HIPPO_CELL_POOL_CA3             1024    /* CA3 cell pool */
#define HIPPO_CELL_POOL_CA1             1024    /* CA1 cell pool */
#define HIPPO_METRICS_BUFFER_SIZE       10000   /* Metrics buffer */
#define HIPPO_FFT_WINDOW_SIZE           1024    /* FFT window for oscillations */
#define HIPPO_KDTREE_MAX_POINTS         10000   /* K-D tree spatial points */

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

typedef struct hippo_utils_ctx_internal* hippo_utils_ctx_t;

/*=============================================================================
 * METRICS STRUCTURES
 *===========================================================================*/

/**
 * @brief Hippocampus metrics for memory analytics
 */
typedef struct {
    /* Counters */
    uint64_t episodes_encoded;          /**< Total episodes encoded */
    uint64_t episodes_retrieved;        /**< Total retrievals */
    uint64_t pattern_separations;       /**< DG pattern separations */
    uint64_t pattern_completions;       /**< CA3 pattern completions */
    uint64_t replay_events;             /**< Sharp-wave ripple replays */
    uint64_t consolidations;            /**< Memory consolidations */

    /* Z-Ladder stats (from PR Memory) */
    uint32_t z0_count;                  /**< Working memory count */
    uint32_t z1_count;                  /**< Short-term count */
    uint32_t z2_count;                  /**< Long-term count */
    uint32_t z3_count;                  /**< Permanent count */
    uint64_t total_promotions;          /**< Z-ladder promotions */

    /* Gauges */
    float mean_encoding_strength;       /**< Average encoding strength */
    float mean_retrieval_accuracy;      /**< Average retrieval confidence */
    float consolidation_progress;       /**< Overall consolidation % */
    float theta_power;                  /**< Theta band power */
    float gamma_power;                  /**< Gamma band power */
    float ripple_power;                 /**< Ripple band power (100-250 Hz) */
    float theta_gamma_coupling;         /**< Phase-amplitude coupling index */

    /* Histograms */
    uint32_t encoding_strength_hist[20];    /**< Encoding strength distribution */
    uint32_t retrieval_accuracy_hist[20];   /**< Retrieval accuracy distribution */
    uint32_t consolidation_latency_hist[20];/**< Z0→Z1 latency distribution */

    /* Timers */
    double avg_encoding_time_ms;        /**< Average encoding latency */
    double avg_retrieval_time_ms;       /**< Average retrieval latency */
    double avg_consolidation_time_ms;   /**< Average consolidation time */
    double avg_replay_duration_ms;      /**< Average replay duration */

    /* Spatial */
    uint32_t active_place_cells;        /**< Currently active place cells */
    float spatial_coherence;            /**< Place field stability */

    /* Timestamp */
    uint64_t last_update_ms;
} hippo_metrics_t;

/**
 * @brief Oscillation analysis result
 */
typedef struct {
    float theta_power;                  /**< 4-8 Hz power */
    float gamma_power;                  /**< 30-80 Hz power */
    float ripple_power;                 /**< 100-250 Hz power */
    float theta_phase;                  /**< Instantaneous theta phase (degrees) */
    float gamma_amplitude;              /**< Gamma envelope amplitude */
    float pac_index;                    /**< Phase-amplitude coupling index */
    bool is_encoding_window;            /**< True if 0-90° theta */
    bool is_retrieval_window;           /**< True if 180-270° theta */
    bool ripple_detected;               /**< Sharp-wave ripple present */
} hippo_oscillation_state_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Hippocampus utils bridge configuration
 */
typedef struct {
    /* Hash table for episodes */
    bool enable_episode_hash;
    uint32_t episode_hash_buckets;

    /* Ring buffer for replays */
    bool enable_replay_ring;
    uint32_t replay_ring_size;

    /* Memory pools */
    bool enable_cell_pools;
    uint32_t dg_pool_size;
    uint32_t ca3_pool_size;
    uint32_t ca1_pool_size;
    uint32_t subiculum_pool_size;

    /* Metrics */
    bool enable_metrics;
    uint32_t metrics_flush_interval_ms;
    char metrics_output_dir[256];

    /* FFT/Signal processing */
    bool enable_oscillation_analysis;
    uint32_t fft_window_size;
    float sample_rate_hz;

    /* K-D Tree for spatial */
    bool enable_spatial_index;
    uint32_t kdtree_max_points;

    /* Numerical integration */
    bool use_rk4_place_cells;
    float place_cell_integration_dt;
} hippo_utils_config_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
hippo_utils_config_t hippo_utils_default_config(void);

/**
 * @brief Create hippocampus utils context
 */
hippo_utils_ctx_t hippo_utils_create(const hippo_utils_config_t* config);

/**
 * @brief Destroy hippocampus utils context
 */
void hippo_utils_destroy(hippo_utils_ctx_t ctx);

/**
 * @brief Reset utils context
 */
void hippo_utils_reset(hippo_utils_ctx_t ctx);

/**
 * @brief Attach utils context to hippocampus
 */
bool hippo_utils_attach(hippo_utils_ctx_t ctx, nimcp_hippocampus_t* hippo);

/*=============================================================================
 * EPISODE HASH TABLE API (O(1) Lookup)
 *===========================================================================*/

/**
 * @brief Index episode in hash table
 *
 * @param ctx Utils context
 * @param episode Episode to index
 * @return true on success
 */
bool hippo_utils_index_episode(hippo_utils_ctx_t ctx, const nimcp_episode_t* episode);

/**
 * @brief Lookup episode by ID (O(1))
 *
 * @param ctx Utils context
 * @param episode_id Episode ID to find
 * @return Pointer to episode or NULL if not found
 */
nimcp_episode_t* hippo_utils_lookup_episode(hippo_utils_ctx_t ctx, uint32_t episode_id);

/**
 * @brief Remove episode from hash table
 *
 * @param ctx Utils context
 * @param episode_id Episode ID to remove
 * @return true if found and removed
 */
bool hippo_utils_remove_episode(hippo_utils_ctx_t ctx, uint32_t episode_id);

/**
 * @brief Get hash table statistics
 */
void hippo_utils_hash_stats(
    hippo_utils_ctx_t ctx,
    uint32_t* num_episodes,
    uint32_t* num_buckets,
    float* avg_chain_length,
    uint32_t* max_chain_length
);

/*=============================================================================
 * REPLAY RING BUFFER API
 *===========================================================================*/

/**
 * @brief Push ripple event to replay buffer
 *
 * Automatically overwrites oldest if full (ring buffer semantics).
 *
 * @param ctx Utils context
 * @param event Ripple event
 * @return true on success
 */
bool hippo_utils_push_replay(hippo_utils_ctx_t ctx, const nimcp_ripple_event_t* event);

/**
 * @brief Get most recent N replay events
 *
 * @param ctx Utils context
 * @param n Number of events
 * @param out_events Output array
 * @param num_found Actual count
 * @return true on success
 */
bool hippo_utils_get_recent_replays(
    hippo_utils_ctx_t ctx,
    uint32_t n,
    nimcp_ripple_event_t* out_events,
    uint32_t* num_found
);

/**
 * @brief Iterate over replay buffer
 *
 * @param ctx Utils context
 * @param callback Function called for each event
 * @param user_data User context
 */
typedef bool (*replay_iterator_fn)(const nimcp_ripple_event_t* event, void* user_data);
void hippo_utils_iterate_replays(
    hippo_utils_ctx_t ctx,
    replay_iterator_fn callback,
    void* user_data
);

/**
 * @brief Get replay buffer size
 */
uint32_t hippo_utils_replay_count(hippo_utils_ctx_t ctx);

/*=============================================================================
 * MEMORY POOL API
 *===========================================================================*/

/**
 * @brief Allocate DG cell from pool
 */
nimcp_dg_cell_t* hippo_utils_alloc_dg_cell(hippo_utils_ctx_t ctx);

/**
 * @brief Allocate CA3 cell from pool
 */
nimcp_ca3_cell_t* hippo_utils_alloc_ca3_cell(hippo_utils_ctx_t ctx);

/**
 * @brief Allocate CA1 cell from pool
 */
nimcp_ca1_cell_t* hippo_utils_alloc_ca1_cell(hippo_utils_ctx_t ctx);

/**
 * @brief Allocate subiculum cell from pool
 */
nimcp_subiculum_cell_t* hippo_utils_alloc_subiculum_cell(hippo_utils_ctx_t ctx);

/**
 * @brief Free cell back to pool
 */
void hippo_utils_free_dg_cell(hippo_utils_ctx_t ctx, nimcp_dg_cell_t* cell);
void hippo_utils_free_ca3_cell(hippo_utils_ctx_t ctx, nimcp_ca3_cell_t* cell);
void hippo_utils_free_ca1_cell(hippo_utils_ctx_t ctx, nimcp_ca1_cell_t* cell);
void hippo_utils_free_subiculum_cell(hippo_utils_ctx_t ctx, nimcp_subiculum_cell_t* cell);

/**
 * @brief Batch allocate cells
 */
uint32_t hippo_utils_alloc_dg_batch(hippo_utils_ctx_t ctx, uint32_t count, nimcp_dg_cell_t** cells);
uint32_t hippo_utils_alloc_ca3_batch(hippo_utils_ctx_t ctx, uint32_t count, nimcp_ca3_cell_t** cells);
uint32_t hippo_utils_alloc_ca1_batch(hippo_utils_ctx_t ctx, uint32_t count, nimcp_ca1_cell_t** cells);

/**
 * @brief Get pool statistics for each region
 */
void hippo_utils_pool_stats(
    hippo_utils_ctx_t ctx,
    hippo_region_t region,
    uint32_t* total,
    uint32_t* used,
    uint32_t* free
);

/*=============================================================================
 * METRICS API
 *===========================================================================*/

/**
 * @brief Record encoding event
 */
void hippo_utils_record_encoding(
    hippo_utils_ctx_t ctx,
    uint32_t episode_id,
    float encoding_strength,
    double latency_ms
);

/**
 * @brief Record retrieval event
 */
void hippo_utils_record_retrieval(
    hippo_utils_ctx_t ctx,
    uint32_t episode_id,
    float accuracy,
    double latency_ms
);

/**
 * @brief Record consolidation event
 */
void hippo_utils_record_consolidation(
    hippo_utils_ctx_t ctx,
    uint32_t num_episodes,
    double latency_ms
);

/**
 * @brief Record replay event
 */
void hippo_utils_record_replay(
    hippo_utils_ctx_t ctx,
    const nimcp_ripple_event_t* event
);

/**
 * @brief Update Z-ladder statistics
 */
void hippo_utils_update_z_stats(
    hippo_utils_ctx_t ctx,
    uint32_t z0, uint32_t z1, uint32_t z2, uint32_t z3,
    uint64_t promotions
);

/**
 * @brief Get current metrics snapshot
 */
bool hippo_utils_get_metrics(hippo_utils_ctx_t ctx, hippo_metrics_t* metrics);

/**
 * @brief Flush metrics to disk
 */
int32_t hippo_utils_flush_metrics(hippo_utils_ctx_t ctx);

/**
 * @brief Export metrics to CSV
 */
bool hippo_utils_export_csv(hippo_utils_ctx_t ctx, const char* filename);

/**
 * @brief Export metrics to JSON
 */
bool hippo_utils_export_json(hippo_utils_ctx_t ctx, const char* filename);

/*=============================================================================
 * OSCILLATION ANALYSIS API (FFT + Signal Filter)
 *===========================================================================*/

/**
 * @brief Analyze LFP signal for theta/gamma/ripple
 *
 * @param ctx Utils context
 * @param lfp_signal Local field potential signal
 * @param num_samples Number of samples
 * @param state Output oscillation state
 * @return true on success
 */
bool hippo_utils_analyze_oscillations(
    hippo_utils_ctx_t ctx,
    const float* lfp_signal,
    uint32_t num_samples,
    hippo_oscillation_state_t* state
);

/**
 * @brief Extract theta rhythm from signal
 *
 * @param ctx Utils context
 * @param signal Input signal
 * @param num_samples Number of samples
 * @param theta_output Filtered theta (4-8 Hz)
 * @return true on success
 */
bool hippo_utils_extract_theta(
    hippo_utils_ctx_t ctx,
    const float* signal,
    uint32_t num_samples,
    float* theta_output
);

/**
 * @brief Extract gamma envelope from signal
 *
 * @param ctx Utils context
 * @param signal Input signal
 * @param num_samples Number of samples
 * @param gamma_envelope Gamma amplitude envelope
 * @return true on success
 */
bool hippo_utils_extract_gamma_envelope(
    hippo_utils_ctx_t ctx,
    const float* signal,
    uint32_t num_samples,
    float* gamma_envelope
);

/**
 * @brief Detect sharp-wave ripples
 *
 * @param ctx Utils context
 * @param signal Input signal
 * @param num_samples Number of samples
 * @param ripple_times Output timestamps of detected ripples
 * @param max_ripples Maximum ripples to detect
 * @param num_ripples Number detected
 * @return true on success
 */
bool hippo_utils_detect_ripples(
    hippo_utils_ctx_t ctx,
    const float* signal,
    uint32_t num_samples,
    float* ripple_times,
    uint32_t max_ripples,
    uint32_t* num_ripples
);

/**
 * @brief Compute phase-amplitude coupling
 *
 * @param ctx Utils context
 * @param theta_phase Array of theta phases (radians)
 * @param gamma_amplitude Array of gamma amplitudes
 * @param num_samples Number of samples
 * @return Modulation index (0 = no coupling, 1 = perfect)
 */
float hippo_utils_compute_pac(
    hippo_utils_ctx_t ctx,
    const float* theta_phase,
    const float* gamma_amplitude,
    uint32_t num_samples
);

/**
 * @brief Get instantaneous theta phase
 *
 * @param ctx Utils context
 * @param signal Input signal
 * @param num_samples Number of samples
 * @param phases Output phases (radians)
 * @return true on success
 */
bool hippo_utils_get_theta_phase(
    hippo_utils_ctx_t ctx,
    const float* signal,
    uint32_t num_samples,
    float* phases
);

/*=============================================================================
 * SPATIAL INDEX API (K-D Tree)
 *===========================================================================*/

/**
 * @brief Add place cell to spatial index
 *
 * @param ctx Utils context
 * @param cell_id Place cell ID
 * @param position 3D position [x, y, z]
 * @return true on success
 */
bool hippo_utils_index_place_cell(
    hippo_utils_ctx_t ctx,
    uint32_t cell_id,
    const float position[3]
);

/**
 * @brief Find k nearest place cells to position
 *
 * @param ctx Utils context
 * @param position Query position
 * @param k Number of neighbors
 * @param out_cell_ids Output cell IDs
 * @param out_distances Output distances
 * @param num_found Actual count found
 * @return true on success
 */
bool hippo_utils_find_nearest_place_cells(
    hippo_utils_ctx_t ctx,
    const float position[3],
    uint32_t k,
    uint32_t* out_cell_ids,
    float* out_distances,
    uint32_t* num_found
);

/**
 * @brief Find place cells within radius
 *
 * @param ctx Utils context
 * @param position Center position
 * @param radius Search radius
 * @param out_cell_ids Output cell IDs
 * @param max_cells Maximum to return
 * @param num_found Actual count
 * @return true on success
 */
bool hippo_utils_find_place_cells_in_radius(
    hippo_utils_ctx_t ctx,
    const float position[3],
    float radius,
    uint32_t* out_cell_ids,
    uint32_t max_cells,
    uint32_t* num_found
);

/**
 * @brief Link episode to spatial location in index
 */
bool hippo_utils_index_episode_location(
    hippo_utils_ctx_t ctx,
    uint32_t episode_id,
    const float position[3]
);

/**
 * @brief Find episodes near location
 */
bool hippo_utils_find_episodes_near(
    hippo_utils_ctx_t ctx,
    const float position[3],
    float radius,
    uint32_t* episode_ids,
    uint32_t max_episodes,
    uint32_t* num_found
);

/*=============================================================================
 * NUMERICAL INTEGRATION API (Place Cell Dynamics)
 *===========================================================================*/

/**
 * @brief Update place cell field with RK4 integration
 *
 * @param cell Place cell to update
 * @param current_position Current agent position
 * @param dt Time step
 * @param ctx Utils context
 */
void hippo_utils_update_place_field_rk4(
    nimcp_place_cell_t* cell,
    const float current_position[3],
    float dt,
    hippo_utils_ctx_t ctx
);

/**
 * @brief Batch update place cells
 *
 * @param cells Array of place cells
 * @param num_cells Number of cells
 * @param current_position Current position
 * @param dt Time step
 * @param ctx Utils context
 */
void hippo_utils_batch_update_place_cells(
    nimcp_place_cell_t* cells,
    uint32_t num_cells,
    const float current_position[3],
    float dt,
    hippo_utils_ctx_t ctx
);

/**
 * @brief Integrate theta phase precession
 *
 * @param cell CA1 cell with place field
 * @param position_in_field Position within field [0, 1]
 * @param theta_phase Current theta phase
 * @param dt Time step
 * @return Updated phase precession value
 */
float hippo_utils_integrate_phase_precession(
    nimcp_ca1_cell_t* cell,
    float position_in_field,
    float theta_phase,
    float dt
);

/*=============================================================================
 * ENHANCED HIPPOCAMPUS OPERATIONS
 *===========================================================================*/

/**
 * @brief Enhanced episode encoding with full utils integration
 *
 * Uses: hash table indexing, metrics, spatial indexing
 */
int hippo_utils_encode_episode_enhanced(
    hippo_utils_ctx_t ctx,
    nimcp_hippocampus_t* hippo,
    const float* what, uint32_t what_dim,
    const float* where, uint32_t where_dim,
    const float* when, uint32_t when_dim,
    float emotional_valence,
    float emotional_arousal,
    uint32_t* episode_id_out
);

/**
 * @brief Enhanced episode retrieval with O(1) lookup
 *
 * Uses: hash table for fast lookup, metrics
 */
int hippo_utils_retrieve_episode_enhanced(
    hippo_utils_ctx_t ctx,
    nimcp_hippocampus_t* hippo,
    const float* cue, uint32_t cue_dim,
    retrieval_mode_t mode,
    uint32_t* episode_id_out,
    float* match_confidence
);

/**
 * @brief Enhanced consolidation with metrics and analytics
 */
int hippo_utils_consolidate_enhanced(
    hippo_utils_ctx_t ctx,
    nimcp_hippocampus_t* hippo,
    float dt
);

/**
 * @brief Enhanced replay processing with ring buffer
 */
int hippo_utils_process_replay_enhanced(
    hippo_utils_ctx_t ctx,
    nimcp_hippocampus_t* hippo
);

/**
 * @brief Enhanced oscillation update with analysis
 */
int hippo_utils_update_oscillations_enhanced(
    hippo_utils_ctx_t ctx,
    nimcp_hippocampus_t* hippo,
    const float* lfp_signal,
    uint32_t num_samples,
    float dt
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HIPPOCAMPUS_UTILS_BRIDGE_H */
