//=============================================================================
// nimcp_cortical_temporal.h - Cortical Column Temporal Dynamics
//=============================================================================
/**
 * @file nimcp_cortical_temporal.h
 * @brief Temporal dynamics and sequence processing for cortical columns
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Intrinsic timescales, temporal receptive fields, and sequence detection
 * WHY:  Implement biologically-inspired temporal hierarchies (Murray et al. 2014)
 * HOW:  Layer-specific time constants, history integration, adaptation dynamics
 *
 * ARCHITECTURE:
 *
 *   Temporal Processing Hierarchy:
 *   ┌──────────────────────────────────────────────────────────┐
 *   │  Layer 2/3 (τ ~100ms) - Integration, prediction         │
 *   │  ┌────────────────────────────────────────────────────┐  │
 *   │  │  Temporal Receptive Field (weighted history)       │  │
 *   │  │  Adaptation (fatigue to repeated stimuli)          │  │
 *   │  │  Habituation (long-term response reduction)        │  │
 *   │  └────────────────────────────────────────────────────┘  │
 *   │                                                           │
 *   │  Layer 4 (τ ~10ms) - Fast sensory input processing      │
 *   │  ┌────────────────────────────────────────────────────┐  │
 *   │  │  Rapid response to thalamic input                  │  │
 *   │  │  Minimal temporal integration                       │  │
 *   │  └────────────────────────────────────────────────────┘  │
 *   │                                                           │
 *   │  Layer 5/6 (τ ~200ms) - Output, contextual modulation   │
 *   │  ┌────────────────────────────────────────────────────┐  │
 *   │  │  Sustained activity, motor preparation              │  │
 *   │  │  Long temporal receptive fields                     │  │
 *   │  └────────────────────────────────────────────────────┘  │
 *   └──────────────────────────────────────────────────────────┘
 *
 * MATHEMATICAL FOUNDATION:
 *
 *   Adaptation Dynamics:
 *     a(t) = a(t-1) * exp(-Δt/τ_adapt) + strength * r(t)
 *     where a = adaptation state, r = neural response
 *
 *   Habituation Dynamics:
 *     h(t) = h(t-1) * exp(-Δt/τ_recover) + rate * r(t)
 *     response_modulated = r(t) * (1 - h(t))
 *
 *   Temporal Receptive Field:
 *     TRF(t) = Σ w(i) * r(t - i*Δt)
 *     where w = temporal weights, i = history bin
 *
 *   Layer Timescales (Murray et al. 2014):
 *     L4:   τ ~ 10ms  (sensory gateway)
 *     L2/3: τ ~ 100ms (integration/prediction)
 *     L5/6: τ ~ 200ms (output/context)
 *
 * BIOLOGICAL BASIS:
 * - Murray et al. (2014): Hierarchy of timescales in primate cortex
 * - Carandini & Heeger (2011): Adaptation in visual cortex
 * - Thompson & Spencer (1966): Habituation mechanisms
 * - Hasson et al. (2008): Temporal receptive windows
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_TEMPORAL_H
#define NIMCP_CORTICAL_TEMPORAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct cortical_temporal_system cortical_temporal_system_t;
typedef struct sequence_detector sequence_detector_t;

//=============================================================================
// Constants
//=============================================================================

/** Number of cortical layers tracked */
#define CORTICAL_TEMPORAL_NUM_LAYERS 5

/** Default timescales (ms) per layer */
#define TEMPORAL_TAU_L1      50.0f   /**< Layer 1 (not typically used) */
#define TEMPORAL_TAU_L23     100.0f  /**< Layer 2/3 (integration) */
#define TEMPORAL_TAU_L4      10.0f   /**< Layer 4 (fast input) */
#define TEMPORAL_TAU_L5      150.0f  /**< Layer 5 (output) */
#define TEMPORAL_TAU_L6      200.0f  /**< Layer 6 (feedback/context) */

/** Default history settings */
#define TEMPORAL_DEFAULT_HISTORY_WINDOW_MS 500.0f  /**< 500ms history */
#define TEMPORAL_DEFAULT_HISTORY_BINS      50      /**< 10ms bins */

/** Adaptation/habituation defaults */
#define TEMPORAL_DEFAULT_ADAPTATION_STRENGTH 0.5f
#define TEMPORAL_DEFAULT_HABITUATION_RATE    0.1f

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Layer-specific timescale configuration
 *
 * WHAT: Time constants governing neural dynamics in each layer
 * WHY:  Different layers process information at different temporal scales
 * HOW:  Exponential decay with layer-specific tau values
 */
typedef struct {
    float tau;                   /**< Time constant (ms) */
    float adaptation_rate;       /**< How fast adaptation occurs [0-1] */
    float recovery_rate;         /**< Recovery from adaptation [0-1] */
} layer_timescale_t;

/**
 * @brief Temporal system configuration
 *
 * WHAT: Configuration for cortical temporal dynamics
 * WHY:  Parameterize temporal processing for different brain regions
 * HOW:  Per-layer timescales, history windows, adaptation parameters
 */
typedef struct {
    /** Per-layer timescales */
    layer_timescale_t layer_timescales[CORTICAL_TEMPORAL_NUM_LAYERS];

    /** History integration */
    float history_window_ms;     /**< How much history to integrate (ms) */
    uint32_t history_bins;       /**< Temporal discretization bins */

    /** Adaptation dynamics */
    float adaptation_strength;   /**< Adaptation accumulation rate [0-1] */
    float adaptation_decay;      /**< Decay time constant (ms) */

    /** Habituation dynamics */
    float habituation_rate;      /**< Habituation accumulation rate [0-1] */
    float habituation_recovery;  /**< Recovery time constant (ms) */

    /** Sequence detection */
    bool enable_sequence_detection;  /**< Enable sequence matching */
    float sequence_threshold;        /**< Sequence detection threshold [0-1] */
    uint32_t max_sequence_length;    /**< Maximum sequence to track */

    /** Column configuration */
    uint32_t num_columns;        /**< Number of cortical columns */
    uint32_t neurons_per_column; /**< Neurons per column */
} temporal_config_t;

/**
 * @brief Temporal receptive field (weighted history)
 *
 * WHAT: Weights over time defining how past activity influences present
 * WHY:  Neurons integrate information across different time windows
 * HOW:  Weighted sum of past activity bins
 */
typedef struct {
    float* history_weights;      /**< Weights over time (size = num_bins) */
    uint32_t num_bins;           /**< Number of temporal bins */
    float bin_width_ms;          /**< Width of each bin (ms) */
    float total_window_ms;       /**< Total window (ms) */
} temporal_receptive_field_t;

/**
 * @brief Temporal state (history and adaptation)
 *
 * WHAT: Current temporal state including history and adaptation
 * WHY:  Track how activity evolves over time
 * HOW:  Circular buffers for history, scalar state for adaptation
 */
typedef struct {
    /** Per-layer activity history */
    float** layer_activities;    /**< [layer][history_bins] */

    /** Per-column adaptation state */
    float* adaptation_states;    /**< Current adaptation per column */

    /** Per-column habituation state */
    float* habituation_states;   /**< Current habituation per column */

    /** Temporal indexing */
    uint32_t history_index;      /**< Current position in circular buffer */
    uint64_t current_time_ms;    /**< Current simulation time (ms) */
    uint64_t last_update_ms;     /**< Last update time (ms) */
} temporal_state_t;

/**
 * @brief Sequence detector for temporal patterns
 *
 * WHAT: Detects predefined temporal sequences in column activity
 * WHY:  Recognize learned patterns and trajectories
 * HOW:  Template matching with dynamic time warping
 */
typedef struct sequence_detector {
    float* sequence_template;    /**< Expected sequence pattern */
    uint32_t sequence_length;    /**< Length of sequence */
    float* match_scores;         /**< Current match at each position */
    float detection_threshold;   /**< Threshold for detection [0-1] */
    bool detected;               /**< Whether sequence is detected */
    uint32_t detection_count;    /**< Number of times detected */
} sequence_detector_t;

/**
 * @brief Temporal statistics
 *
 * WHAT: Runtime statistics for temporal system
 * WHY:  Monitor adaptation, habituation, sequence detection
 */
typedef struct {
    /** Adaptation stats */
    float avg_adaptation;        /**< Average adaptation across columns */
    float max_adaptation;        /**< Maximum adaptation value */
    uint32_t adapted_columns;    /**< Number of adapted columns */

    /** Habituation stats */
    float avg_habituation;       /**< Average habituation */
    float max_habituation;       /**< Maximum habituation */
    uint32_t habituated_columns; /**< Number of habituated columns */

    /** Sequence stats */
    uint32_t sequences_detected; /**< Total sequences detected */
    float avg_sequence_match;    /**< Average sequence match score */

    /** Timing */
    uint64_t total_updates;      /**< Total update calls */
    uint64_t elapsed_time_ms;    /**< Total elapsed time */
} temporal_stats_t;

//=============================================================================
// Core API
//=============================================================================

/**
 * WHAT: Create temporal dynamics system
 * WHY:  Initialize temporal processing for cortical columns
 * HOW:  Allocate state, configure timescales, initialize history buffers
 *
 * @param config Temporal configuration
 * @return Temporal system handle or NULL on failure
 */
cortical_temporal_system_t* cortical_temporal_create(
    const temporal_config_t* config
);

/**
 * WHAT: Destroy temporal system
 * WHY:  Free resources
 * HOW:  Deallocate history buffers and state
 *
 * @param system Temporal system to destroy
 */
void cortical_temporal_destroy(cortical_temporal_system_t* system);

/**
 * WHAT: Get default temporal configuration
 * WHY:  Provide sensible defaults based on Murray et al. (2014)
 * HOW:  Layer timescales from biological measurements
 *
 * @param num_columns Number of cortical columns
 * @param neurons_per_column Neurons per column
 * @return Default configuration
 */
temporal_config_t cortical_temporal_default_config(
    uint32_t num_columns,
    uint32_t neurons_per_column
);

//=============================================================================
// Timescale Configuration API
//=============================================================================

/**
 * WHAT: Set timescale for specific layer
 * WHY:  Customize temporal dynamics per layer
 * HOW:  Update layer timescale configuration
 *
 * @param system Temporal system
 * @param layer Layer index (0-4)
 * @param timescale Layer timescale configuration
 * @return 0 on success, -1 on error
 */
int cortical_temporal_set_layer_timescale(
    cortical_temporal_system_t* system,
    uint32_t layer,
    const layer_timescale_t* timescale
);

/**
 * WHAT: Get effective timescale for layer
 * WHY:  Query current temporal dynamics
 * HOW:  Return configured tau value
 *
 * @param system Temporal system
 * @param layer Layer index (0-4)
 * @return Effective tau (ms) or -1.0 on error
 */
float cortical_temporal_get_effective_timescale(
    const cortical_temporal_system_t* system,
    uint32_t layer
);

//=============================================================================
// Temporal Dynamics API
//=============================================================================

/**
 * WHAT: Update temporal state
 * WHY:  Advance time, update adaptation, process history
 * HOW:  Exponential decay of adaptation, circular buffer updates
 *
 * @param system Temporal system
 * @param delta_time_ms Time step (ms)
 * @return 0 on success, -1 on error
 */
int cortical_temporal_update(
    cortical_temporal_system_t* system,
    float delta_time_ms
);

/**
 * WHAT: Apply adaptation to column activity
 * WHY:  Reduce response to repeated stimuli
 * HOW:  a(t) = a(t-1) * exp(-dt/τ) + strength * activity
 *       response_out = response_in * (1 - a(t))
 *
 * @param system Temporal system
 * @param column_id Column index
 * @param activity Column activity [0-1]
 * @return Adapted activity or -1.0 on error
 */
float cortical_temporal_apply_adaptation(
    cortical_temporal_system_t* system,
    uint32_t column_id,
    float activity
);

/**
 * WHAT: Apply habituation to column activity
 * WHY:  Long-term reduction in responsiveness
 * HOW:  h(t) = h(t-1) * exp(-dt/τ_recover) + rate * activity
 *       response_out = response_in * (1 - h(t))
 *
 * @param system Temporal system
 * @param column_id Column index
 * @param activity Column activity [0-1]
 * @return Habituated activity or -1.0 on error
 */
float cortical_temporal_apply_habituation(
    cortical_temporal_system_t* system,
    uint32_t column_id,
    float activity
);

/**
 * WHAT: Integrate temporal history for column
 * WHY:  Compute temporally-weighted activity
 * HOW:  TRF(t) = Σ w(i) * r(t - i*dt)
 *
 * @param system Temporal system
 * @param column_id Column index
 * @param layer Layer index (0-4)
 * @param current_activity Current activity value
 * @return Integrated activity or -1.0 on error
 */
float cortical_temporal_integrate_history(
    cortical_temporal_system_t* system,
    uint32_t column_id,
    uint32_t layer,
    float current_activity
);

/**
 * WHAT: Reset adaptation state for column
 * WHY:  Clear adaptation after stimulus offset
 * HOW:  Set adaptation state to 0
 *
 * @param system Temporal system
 * @param column_id Column index
 * @return 0 on success, -1 on error
 */
int cortical_temporal_reset_adaptation(
    cortical_temporal_system_t* system,
    uint32_t column_id
);

/**
 * WHAT: Reset habituation state for column
 * WHY:  Clear habituation after recovery period
 * HOW:  Set habituation state to 0
 *
 * @param system Temporal system
 * @param column_id Column index
 * @return 0 on success, -1 on error
 */
int cortical_temporal_reset_habituation(
    cortical_temporal_system_t* system,
    uint32_t column_id
);

//=============================================================================
// Sequence Detection API
//=============================================================================

/**
 * WHAT: Create sequence detector
 * WHY:  Detect predefined temporal patterns
 * HOW:  Allocate detector with template
 *
 * @param sequence_template Expected sequence pattern
 * @param sequence_length Length of sequence
 * @param detection_threshold Detection threshold [0-1]
 * @return Sequence detector or NULL on failure
 */
sequence_detector_t* cortical_temporal_create_sequence_detector(
    const float* sequence_template,
    uint32_t sequence_length,
    float detection_threshold
);

/**
 * WHAT: Destroy sequence detector
 * WHY:  Free resources
 * HOW:  Deallocate detector memory
 *
 * @param detector Sequence detector
 */
void cortical_temporal_destroy_sequence_detector(
    sequence_detector_t* detector
);

/**
 * WHAT: Detect sequence in temporal activity
 * WHY:  Recognize learned patterns
 * HOW:  Template matching with current activity buffer
 *
 * @param system Temporal system
 * @param detector Sequence detector
 * @param column_id Column to check
 * @param layer Layer to check
 * @return true if sequence detected, false otherwise
 */
bool cortical_temporal_detect_sequence(
    cortical_temporal_system_t* system,
    sequence_detector_t* detector,
    uint32_t column_id,
    uint32_t layer
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * WHAT: Get temporal statistics
 * WHY:  Monitor adaptation, habituation, sequence detection
 * HOW:  Compute statistics from current state
 *
 * @param system Temporal system
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int cortical_temporal_get_stats(
    const cortical_temporal_system_t* system,
    temporal_stats_t* stats
);

/**
 * WHAT: Reset temporal statistics
 * WHY:  Clear counters for new measurement period
 * HOW:  Zero out statistics
 *
 * @param system Temporal system
 * @return 0 on success, -1 on error
 */
int cortical_temporal_reset_stats(
    cortical_temporal_system_t* system
);

//=============================================================================
// Bio-async Integration API
//=============================================================================

/**
 * WHAT: Connect temporal system to bio-async router
 * WHY:  Enable inter-module temporal signaling
 * HOW:  Register with BIO_MODULE_CORTICAL_TEMPORAL
 *
 * @param system Temporal system
 * @return 0 on success, -1 on error
 */
int cortical_temporal_connect_bio_async(
    cortical_temporal_system_t* system
);

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown
 * HOW:  Unregister module
 *
 * @param system Temporal system
 * @return 0 on success, -1 on error
 */
int cortical_temporal_disconnect_bio_async(
    cortical_temporal_system_t* system
);

/**
 * WHAT: Check if bio-async connected
 * WHY:  Query connection status
 * HOW:  Check bio_async_enabled flag
 *
 * @param system Temporal system
 * @return true if connected, false otherwise
 */
bool cortical_temporal_is_bio_async_connected(
    const cortical_temporal_system_t* system
);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_CORTICAL_TEMPORAL_H
