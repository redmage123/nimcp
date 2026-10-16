/**
 * @file nimcp_snn_hippocampus_bridge.h
 * @brief SNN integration bridge for hippocampus memory system
 *
 * WHAT: Bidirectional integration between SNN and hippocampus
 * WHY:  Enable spike-based episodic memory formation and retrieval
 * HOW:  Place cell spike patterns, theta rhythm synchronization, ripple generation
 *
 * BIOLOGICAL BASIS:
 * - Place cells: Spike when specific location visited (spatial coding)
 * - Theta rhythm: 4-12 Hz oscillation organizing spike timing
 * - Phase precession: Spike phase shifts relative to theta
 * - Sharp-wave ripples: 100-250 Hz bursts for memory consolidation
 * - CA1/CA3: Recurrent networks for pattern completion
 * - Dentate gyrus: Pattern separation via sparse coding
 * - Entorhinal cortex: Grid cells and temporal coding
 *
 * INTEGRATION:
 * - Connects to hippocampus region (REGION_HIPPOCAMPUS)
 * - Maps SNN populations to hippocampal subregions
 * - Implements place cell spike encoding
 * - Generates theta-modulated spike sequences
 * - Creates ripples for memory replay
 * - Uses bio-async for hippocampal-cortical communication
 *
 * @author NIMCP Team
 * @date 2024
 */

#ifndef NIMCP_SNN_HIPPOCAMPUS_BRIDGE_H
#define NIMCP_SNN_HIPPOCAMPUS_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "core/brain_regions/nimcp_brain_regions.h"

//=============================================================================
// Configuration Types
//=============================================================================

/**
 * @brief SNN hippocampus bridge configuration
 *
 * WHAT: Parameters for SNN-hippocampus integration
 * WHY:  Control theta rhythm, place fields, and consolidation
 * HOW:  Rhythm parameters, place cell tuning, ripple generation
 */
typedef struct snn_hippocampus_config_s {
    /* Theta rhythm parameters */
    float theta_frequency;          /**< Theta oscillation frequency (Hz) [4-12] */
    float theta_amplitude;          /**< Theta modulation amplitude [0, 1] */
    bool enable_phase_precession;   /**< Enable spike phase precession */
    float precession_slope;         /**< Phase precession slope (deg/cm) */

    /* Place cell parameters */
    uint32_t num_place_cells;       /**< Number of place cells */
    float place_field_size;         /**< Place field width (spatial units) */
    float place_field_peak_rate;    /**< Peak firing rate in field (Hz) */
    float background_rate;          /**< Background firing rate (Hz) */
    float spatial_resolution;       /**< Spatial binning resolution */

    /* Ripple parameters */
    float ripple_frequency;         /**< Ripple frequency (Hz) [100-250] */
    float ripple_duration;          /**< Duration of ripple event (ms) */
    float ripple_threshold;         /**< Activity threshold for ripple */
    uint32_t ripple_participant_ratio; /**< % of neurons in ripple */

    /* Episodic encoding */
    float encoding_spike_threshold; /**< Threshold for memory encoding */
    uint32_t min_spikes_for_episode; /**< Minimum spikes to form episode */
    float episode_temporal_window;  /**< Time window for episode (ms) */

    /* Pattern separation (Dentate Gyrus) */
    float dg_sparsity;              /**< DG sparse coding ratio [0, 1] */
    float dg_decorrelation_strength; /**< Pattern separation strength */

    /* Pattern completion (CA3) */
    float ca3_recurrence_strength;  /**< CA3 recurrent weight */
    float ca3_completion_threshold; /**< Min similarity for completion */

    /* Bio-async */
    bool enable_bio_async;          /**< Enable bio-async messaging */
} snn_hippocampus_config_t;

/**
 * @brief Place cell spike pattern
 *
 * WHAT: Spike representation of place cell activity
 * WHY:  Encode spatial location via spike timing
 * HOW:  Rate and phase coding in theta rhythm
 */
typedef struct place_cell_pattern_s {
    uint32_t cell_id;               /**< Place cell identifier */
    float field_center_x;           /**< X coordinate of place field center */
    float field_center_y;           /**< Y coordinate of place field center */
    float field_radius;             /**< Place field radius */
    float current_rate;             /**< Current firing rate (Hz) */
    float theta_phase;              /**< Current theta phase (radians) */
    bool is_active;                 /**< Currently in place field */
    uint64_t last_spike_time;       /**< Last spike timestamp (us) */
} place_cell_pattern_t;

/**
 * @brief Episodic memory representation
 *
 * WHAT: Spike sequence encoding episodic memory
 * WHY:  Store temporal sequences of activity
 * HOW:  Ordered list of spike events with context
 */
typedef struct episodic_memory_s {
    uint32_t episode_id;            /**< Unique episode identifier */
    uint64_t start_time;            /**< Episode start timestamp (us) */
    uint64_t end_time;              /**< Episode end timestamp (us) */
    float spatial_context[3];       /**< Spatial context (x, y, z) */
    uint32_t* spike_sequence;       /**< Neuron IDs in temporal order */
    uint32_t sequence_length;       /**< Length of spike sequence */
    float* spike_times_relative;    /**< Spike times relative to start (ms) */
    bool consolidated;              /**< Whether consolidated via ripple */
} episodic_memory_t;

/**
 * @brief Sharp-wave ripple event
 *
 * WHAT: High-frequency burst for memory replay
 * WHY:  Consolidate episodic memories during rest/sleep
 * HOW:  100-250 Hz population burst with sequence replay
 */
typedef struct ripple_event_s {
    uint64_t start_time;            /**< Ripple start timestamp (us) */
    uint64_t end_time;              /**< Ripple end timestamp (us) */
    float peak_frequency;           /**< Peak ripple frequency (Hz) */
    float peak_amplitude;           /**< Peak population activity */
    uint32_t participating_neurons; /**< Number of neurons in ripple */
    episodic_memory_t* replayed_episode; /**< Episode being replayed */
} ripple_event_t;

/**
 * @brief SNN-hippocampus bridge structure
 *
 * WHAT: Context for SNN-hippocampus integration
 * WHY:  Maintain state of memory formation and retrieval
 * HOW:  Store place cells, episodes, theta state
 */
typedef struct snn_hippocampus_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* network;             /**< SNN network */
    brain_region_t* hippocampus_region; /**< Hippocampus brain region */
    snn_hippocampus_config_t config;    /**< Bridge configuration */

    /* Subregion populations */
    snn_population_t* ca1_pop;          /**< CA1 population */
    snn_population_t* ca3_pop;          /**< CA3 population */
    snn_population_t* dg_pop;           /**< Dentate gyrus population */
    snn_population_t* ec_pop;           /**< Entorhinal cortex population */

    /* Place cell representations */
    place_cell_pattern_t** place_cells; /**< Array of place cells */
    uint32_t n_place_cells;             /**< Number of place cells */

    /* Episodic memory storage */
    episodic_memory_t** episodes;       /**< Stored episodic memories */
    uint32_t n_episodes;                /**< Number of stored episodes */
    uint32_t max_episodes;              /**< Maximum episode capacity */
    episodic_memory_t* current_episode; /**< Currently forming episode */

    /* Ripple event history */
    ripple_event_t* recent_ripples;     /**< Recent ripple events */
    uint32_t ripple_count;              /**< Number of recent ripples */
    uint32_t max_ripples;               /**< Max ripples to track */

    /* Theta rhythm state */
    float theta_phase;                  /**< Current theta phase (radians) */
    float theta_time;                   /**< Time in theta cycle (ms) */
    bool theta_active;                  /**< Theta rhythm currently active */

    /* Spatial state */
    float current_position[3];          /**< Current spatial position */
    float velocity[3];                  /**< Current velocity */

    /* State */
    bool connected;                     /**< Bridge active */
    float last_update_time;             /**< Last update timestamp (ms) */
    uint32_t update_count;              /**< Number of updates */

    /* Bio-async */
    bool bio_async_enabled;             /**< Bio-async connected */
    bio_module_context_t bio_ctx;       /**< Bio-async context */

    /* Mutex for thread safety */
    void* mutex;
} snn_hippocampus_bridge_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Initialize hippocampus config with defaults
 *
 * WHAT: Set biologically-plausible defaults
 * WHY:  Convenient initialization
 * HOW:  Values from hippocampal neuroscience literature
 *
 * @param config Config to initialize
 */
void snn_hippocampus_config_default(snn_hippocampus_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create SNN-hippocampus bridge
 *
 * WHAT: Initialize bidirectional bridge
 * WHY:  Enable spike-based episodic memory
 * HOW:  Allocate context, create place cells, initialize theta
 *
 * @param config Bridge configuration
 * @param network SNN network
 * @param hippocampus_region Hippocampus brain region
 * @return Bridge instance or NULL on failure
 */
snn_hippocampus_bridge_t* snn_hippocampus_bridge_create(
    const snn_hippocampus_config_t* config,
    snn_network_t* network,
    brain_region_t* hippocampus_region
);

/**
 * @brief Destroy SNN-hippocampus bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper cleanup
 * HOW:  Free episodes, place cells, disconnect
 *
 * @param bridge Bridge to destroy
 */
void snn_hippocampus_bridge_destroy(snn_hippocampus_bridge_t* bridge);

/**
 * @brief Connect bridge to bio-async
 *
 * @param bridge Bridge to connect
 * @return 0 on success, error code on failure
 */
int snn_hippocampus_bridge_connect_bio_async(snn_hippocampus_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success
 */
int snn_hippocampus_bridge_disconnect_bio_async(snn_hippocampus_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool snn_hippocampus_bridge_is_bio_async_connected(const snn_hippocampus_bridge_t* bridge);

//=============================================================================
// Processing Functions
//=============================================================================

/**
 * @brief Process spatial input and update place cells
 *
 * WHAT: Update place cell activity based on position
 * WHY:  Encode spatial location via spike patterns
 * HOW:  Compute place field overlap, generate theta-modulated spikes
 *
 * @param bridge Bridge context
 * @param position Current position (x, y, z)
 * @param velocity Current velocity (vx, vy, vz)
 * @param output Output spike patterns (pre-allocated)
 * @param output_size Size of output buffer
 * @return 0 on success, error code on failure
 */
int snn_hippocampus_bridge_process(
    snn_hippocampus_bridge_t* bridge,
    const float* position,
    const float* velocity,
    float* output,
    uint32_t output_size
);

/**
 * @brief Update bridge state
 *
 * WHAT: Single timestep update of hippocampal dynamics
 * WHY:  Advance theta rhythm, update place cells, check for ripples
 * HOW:  Update theta phase, process spikes, consolidate memories
 *
 * @param bridge Bridge to update
 * @param dt Timestep (ms)
 * @return 0 on success, error code on failure
 */
int snn_hippocampus_bridge_update(snn_hippocampus_bridge_t* bridge, float dt);

/**
 * @brief Generate sharp-wave ripple event
 *
 * WHAT: Create high-frequency burst for memory consolidation
 * WHY:  Replay episodic sequences for long-term storage
 * HOW:  Generate 100-250 Hz population burst with episode replay
 *
 * @param bridge Bridge context
 * @param episode Episode to replay (NULL for spontaneous)
 * @return Ripple event or NULL on failure
 */
ripple_event_t* snn_hippocampus_generate_ripple(
    snn_hippocampus_bridge_t* bridge,
    episodic_memory_t* episode
);

/**
 * @brief Encode episodic memory from recent activity
 *
 * WHAT: Form episodic memory from recent spike sequence
 * WHY:  Store temporal context for later retrieval
 * HOW:  Extract spike sequence, spatial context, temporal order
 *
 * @param bridge Bridge context
 * @param time_window Time window to include (ms)
 * @return Episodic memory or NULL on failure
 */
episodic_memory_t* snn_hippocampus_encode_episode(
    snn_hippocampus_bridge_t* bridge,
    float time_window
);

/**
 * @brief Retrieve episodic memory by similarity
 *
 * WHAT: Pattern completion - retrieve episode from partial cue
 * WHY:  Recall memories from partial information
 * HOW:  CA3 recurrence completes patterns
 *
 * @param bridge Bridge context
 * @param cue_spikes Partial spike pattern (cue)
 * @param cue_length Length of cue
 * @return Most similar episode or NULL if no match
 */
episodic_memory_t* snn_hippocampus_retrieve_episode(
    snn_hippocampus_bridge_t* bridge,
    const uint32_t* cue_spikes,
    uint32_t cue_length
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current theta phase
 *
 * @param bridge Bridge to query
 * @return Current theta phase (radians) or -1.0 on error
 */
float snn_hippocampus_get_theta_phase(const snn_hippocampus_bridge_t* bridge);

/**
 * @brief Get place cell pattern
 *
 * @param bridge Bridge to query
 * @param cell_idx Place cell index
 * @param pattern Output for pattern (copied)
 * @return 0 on success
 */
int snn_hippocampus_get_place_cell(
    const snn_hippocampus_bridge_t* bridge,
    uint32_t cell_idx,
    place_cell_pattern_t* pattern
);

/**
 * @brief Get number of stored episodes
 *
 * @param bridge Bridge to query
 * @return Number of episodes
 */
uint32_t snn_hippocampus_get_episode_count(const snn_hippocampus_bridge_t* bridge);

/**
 * @brief Get overall bridge activity level
 *
 * WHAT: Compute average hippocampal activity
 * WHY:  Single metric for memory system activity
 * HOW:  Average place cell firing rates
 *
 * @param bridge Bridge to query
 * @return Activity level [0, 1] or -1.0 on error
 */
float snn_hippocampus_bridge_get_activity(const snn_hippocampus_bridge_t* bridge);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge to query
 * @param total_ripples Output: total ripples generated
 * @param episodes_stored Output: episodes in memory
 * @param updates Output: update cycles completed
 * @return 0 on success
 */
int snn_hippocampus_get_stats(
    const snn_hippocampus_bridge_t* bridge,
    uint32_t* total_ripples,
    uint32_t* episodes_stored,
    uint32_t* updates
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge to reset
 */
void snn_hippocampus_reset_stats(snn_hippocampus_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_HIPPOCAMPUS_BRIDGE_H */
