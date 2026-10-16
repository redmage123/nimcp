/**
 * @file nimcp_audio_cortical_bridge.h
 * @brief Audio-Cortical Bridge - Connects audio cortex with cortical columns
 *
 * WHAT: Integration layer that connects audio_cortex perception module with
 *       cortical column processing (frequency hypercolumns, tonotopic maps,
 *       cortical immune system).
 * WHY:  Provides biologically-realistic A1 processing by replacing audio_cortex's
 *       internal frequency filtering with proper cortical column organization.
 * HOW:  Routes audio input through tonotopic mapping to frequency hypercolumns,
 *       applies cortical immune modulation, and uses bio-async messaging.
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                    AUDIO-CORTICAL BRIDGE                                │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │  audio_cortex_t                 feature_hypercolumn_t                   │
 * │  ┌─────────────┐               ┌───────────────────────┐               │
 * │  │ Audio Input │──────────────▶│ Frequency Filter Bank │               │
 * │  │ A1 Features │    Tono-      │ Frequency Columns     │               │
 * │  │ Attention   │    topic      │ Constant-Q Organization│               │
 * │  └─────────────┘    Mapping    └───────────────────────┘               │
 * │        │                               │                                │
 * │        │                               ▼                                │
 * │        │           ┌───────────────────────────────┐                   │
 * │        └──────────▶│ topographic_map_t (Tonotopic) │                   │
 * │                    │ Logarithmic frequency mapping │                   │
 * │                    │ Low-frequency magnification   │                   │
 * │                    └───────────────────────────────┘                   │
 * │                                │                                        │
 * │                                ▼                                        │
 * │                    ┌───────────────────────────────┐                   │
 * │                    │ cortical_immune_system_t       │                   │
 * │                    │ Microglial surveillance        │                   │
 * │                    │ Inflammation modulation        │                   │
 * │                    └───────────────────────────────┘                   │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * BIOLOGICAL BASIS:
 * - A1 auditory cortex has columnar organization for frequency selectivity
 * - Tonotopic mapping provides frequency correspondence with cochlea
 * - Cortical hypercolumns contain neurons tuned to all frequencies
 * - Microglial surveillance monitors cortical health and modulates processing
 *
 * @version 1.0.0
 * @date 2025-12-19
 * @author NIMCP Development Team
 */

#ifndef NIMCP_AUDIO_CORTICAL_BRIDGE_H
#define NIMCP_AUDIO_CORTICAL_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Perception modules */
#include "perception/nimcp_audio_cortex.h"

/* Cortical column modules */
#include "core/cortical_columns/nimcp_feature_hypercolumns.h"
#include "core/cortical_columns/nimcp_topographic_maps.h"
#include "core/cortical_columns/nimcp_cortical_immune.h"

/* Bio-async communication */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Memory management */
#include "utils/memory/nimcp_unified_memory.h"

/* Logging */
#include "utils/logging/nimcp_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum number of frequency hypercolumns */
#define AUDIO_CORTICAL_MAX_HYPERCOLUMNS 256

/** Maximum number of tonotopic positions */
#define AUDIO_CORTICAL_MAX_POSITIONS 1024

/** Default number of frequency bands per hypercolumn */
#define AUDIO_CORTICAL_DEFAULT_FREQ_BANDS 8

/** Default center frequency for auditory filters (Hz) */
#define AUDIO_CORTICAL_DEFAULT_CENTER_FREQ 1000.0f

/** Default Q factor for constant-Q filters */
#define AUDIO_CORTICAL_DEFAULT_Q_FACTOR 2.0f

/** Default tuning width in octaves */
#define AUDIO_CORTICAL_DEFAULT_TUNING_WIDTH 0.5f

/** Default immune inflammation modulation factor */
#define AUDIO_CORTICAL_DEFAULT_IMMUNE_FACTOR 1.0f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Processing mode for the audio-cortical bridge
 */
typedef enum {
    AUDIO_CORTICAL_MODE_DIRECT = 0,      /**< Direct filtering (faster) */
    AUDIO_CORTICAL_MODE_HYPERCOLUMN,     /**< Full hypercolumn processing (more accurate) */
    AUDIO_CORTICAL_MODE_TONOTOPIC,       /**< Include tonotopic mapping */
    AUDIO_CORTICAL_MODE_FULL             /**< All features enabled */
} audio_cortical_mode_t;

/**
 * @brief State of the audio-cortical bridge
 */
typedef enum {
    AUDIO_CORTICAL_STATE_UNINITIALIZED = 0,
    AUDIO_CORTICAL_STATE_READY,
    AUDIO_CORTICAL_STATE_PROCESSING,
    AUDIO_CORTICAL_STATE_ERROR
} audio_cortical_state_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Configuration for audio-cortical bridge
 */
typedef struct {
    /** Number of frequency hypercolumns to create */
    uint32_t num_hypercolumns;

    /** Number of frequency bands per hypercolumn (typically 8 or 16) */
    uint32_t freq_bands_per_hypercolumn;

    /** Minimum frequency for tonotopic mapping (Hz) */
    float min_frequency;

    /** Maximum frequency for tonotopic mapping (Hz) */
    float max_frequency;

    /** Q factor for constant-Q filters */
    float q_factor;

    /** Frequency tuning width in octaves */
    float tuning_width;

    /** Processing mode */
    audio_cortical_mode_t mode;

    /** Enable tonotopic mapping */
    bool enable_tonotopic_mapping;

    /** Enable cortical immune integration */
    bool enable_cortical_immune;

    /** Enable bio-async messaging */
    bool enable_bio_async;

    /** Frequency range coverage in octaves */
    float frequency_range_octaves;

    /** Low-frequency emphasis region (Hz) */
    float low_freq_emphasis;

    /** Cortical magnification factor */
    float cortical_magnification;

    /** Immune modulation factor (0.0 = no effect, 1.0 = full effect) */
    float immune_modulation_factor;

    /** Use unified memory manager for allocations */
    bool use_umm;
} audio_cortical_config_t;

/**
 * @brief Statistics for audio-cortical bridge
 */
typedef struct {
    /** Total audio frames processed */
    uint64_t frames_processed;

    /** Total hypercolumn activations */
    uint64_t hypercolumn_activations;

    /** Total bio-async messages sent */
    uint64_t bio_messages_sent;

    /** Total bio-async messages received */
    uint64_t bio_messages_received;

    /** Average processing time in ms */
    float avg_processing_time_ms;

    /** Peak frequency response */
    float peak_frequency_response;

    /** Current dominant frequency (Hz) */
    float current_dominant_frequency;

    /** Current immune modulation level */
    float current_immune_modulation;

    /** Number of active hypercolumns */
    uint32_t active_hypercolumns;
} audio_cortical_stats_t;

/**
 * @brief Result of frequency analysis
 */
typedef struct {
    /** Dominant frequency in Hz */
    float dominant_frequency;

    /** Frequency selectivity index (0-1) */
    float selectivity_index;

    /** Frequency distribution [num_freq_bands] */
    float* frequency_responses;

    /** Number of frequency bands */
    uint32_t num_freq_bands;

    /** Confidence of frequency detection (0-1) */
    float confidence;

    /** Tonotopic position x (if mapped) */
    float tono_x;

    /** Tonotopic position y (if mapped) */
    float tono_y;
} audio_cortical_frequency_result_t;

/**
 * @brief Opaque handle to audio-cortical bridge instance
 */
typedef struct audio_cortical_bridge audio_cortical_bridge_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Initialize default configuration
 *
 * WHAT: Sets up default configuration for audio-cortical bridge.
 * WHY:  Provide sensible defaults for typical A1 processing.
 * HOW:  Fills config structure with biologically-plausible values.
 *
 * @param config Configuration structure to initialize
 */
void audio_cortical_default_config(audio_cortical_config_t* config);

/**
 * @brief Create audio-cortical bridge
 *
 * WHAT: Allocates and initializes the audio-cortical bridge.
 * WHY:  Connects auditory perception with cortical column processing.
 * HOW:  Creates hypercolumns, tonotopic map, and immune connections.
 *
 * @param config Configuration (NULL for defaults)
 * @param audio_cortex Audio cortex instance to connect (may be NULL)
 * @return Bridge instance, or NULL on failure
 *
 * @note Caller must free with audio_cortical_bridge_destroy()
 */
audio_cortical_bridge_t* audio_cortical_bridge_create(
    const audio_cortical_config_t* config,
    audio_cortex_t* audio_cortex
);

/**
 * @brief Destroy audio-cortical bridge
 *
 * WHAT: Frees all resources associated with the bridge.
 * WHY:  Clean up memory and connections.
 * HOW:  Destroys hypercolumns, map, and disconnects immune.
 *
 * @param bridge Bridge to destroy (may be NULL)
 */
void audio_cortical_bridge_destroy(audio_cortical_bridge_t* bridge);

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

/**
 * @brief Connect to audio cortex
 *
 * WHAT: Associates an audio cortex instance with this bridge.
 * WHY:  Enable bidirectional communication between perception and columns.
 * HOW:  Stores reference and sets up callbacks.
 *
 * @param bridge Audio-cortical bridge
 * @param audio_cortex Audio cortex to connect
 * @return 0 on success, non-zero on failure
 */
int audio_cortical_connect_audio_cortex(
    audio_cortical_bridge_t* bridge,
    audio_cortex_t* audio_cortex
);

/**
 * @brief Connect to cortical immune system
 *
 * WHAT: Associates cortical immune system for modulation.
 * WHY:  Enable inflammation-based processing modulation.
 * HOW:  Stores reference and registers for cytokine updates.
 *
 * @param bridge Audio-cortical bridge
 * @param immune Cortical immune system to connect
 * @return 0 on success, non-zero on failure
 */
int audio_cortical_connect_immune(
    audio_cortical_bridge_t* bridge,
    cortical_immune_system_t* immune
);

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Registers bridge with bio-async messaging system.
 * WHY:  Enable inter-module communication.
 * HOW:  Registers module and sets up message handlers.
 *
 * @param bridge Audio-cortical bridge
 * @return 0 on success, non-zero on failure
 */
int audio_cortical_connect_bio_async(audio_cortical_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregisters bridge from bio-async messaging system.
 * WHY:  Clean disconnection before shutdown.
 * HOW:  Unregisters module ID.
 *
 * @param bridge Audio-cortical bridge
 * @return 0 on success, non-zero on failure
 */
int audio_cortical_disconnect_bio_async(audio_cortical_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Audio-cortical bridge
 * @return true if connected, false otherwise
 */
bool audio_cortical_is_bio_async_connected(const audio_cortical_bridge_t* bridge);

/* ============================================================================
 * Processing Functions
 * ============================================================================ */

/**
 * @brief Process audio through audio-cortical bridge
 *
 * WHAT: Routes audio through tonotopic mapping and frequency hypercolumns.
 * WHY:  Biologically-realistic A1 processing with proper cortical organization.
 * HOW:  Maps audio to cortical positions, processes through hypercolumns.
 *
 * @param bridge Audio-cortical bridge
 * @param audio_data Audio samples (mono, normalized -1 to 1)
 * @param num_samples Number of audio samples
 * @param sample_rate Sample rate in Hz
 * @param result Output frequency analysis result
 * @return 0 on success, non-zero on failure
 *
 * @note Result's frequency_responses is allocated by this function.
 *       Caller must free with audio_cortical_free_result()
 */
int audio_cortical_process(
    audio_cortical_bridge_t* bridge,
    const float* audio_data,
    uint32_t num_samples,
    uint32_t sample_rate,
    audio_cortical_frequency_result_t* result
);

/**
 * @brief Process audio at specific tonotopic position
 *
 * WHAT: Processes audio focused on a specific frequency region.
 * WHY:  Efficient processing for attention-guided audition.
 * HOW:  Maps frequency to hypercolumn and processes.
 *
 * @param bridge Audio-cortical bridge
 * @param audio_data Audio samples
 * @param num_samples Number of samples
 * @param sample_rate Sample rate in Hz
 * @param center_freq Center frequency to focus on (Hz)
 * @param bandwidth Frequency bandwidth (Hz)
 * @param result Output frequency analysis result
 * @return 0 on success, non-zero on failure
 */
int audio_cortical_process_band(
    audio_cortical_bridge_t* bridge,
    const float* audio_data,
    uint32_t num_samples,
    uint32_t sample_rate,
    float center_freq,
    float bandwidth,
    audio_cortical_frequency_result_t* result
);

/**
 * @brief Free frequency result resources
 *
 * WHAT: Frees memory allocated in frequency result.
 * WHY:  Clean up after processing.
 * HOW:  Frees frequency_responses array.
 *
 * @param result Result to free
 */
void audio_cortical_free_result(audio_cortical_frequency_result_t* result);

/**
 * @brief Get frequency map for entire audio frame
 *
 * WHAT: Computes dominant frequency for each time position in audio.
 * WHY:  Generate frequency-based spectrogram.
 * HOW:  Processes all time windows through hypercolumns.
 *
 * @param bridge Audio-cortical bridge
 * @param audio_data Input audio
 * @param num_samples Number of samples
 * @param sample_rate Sample rate in Hz
 * @param frequency_map Output frequency map [num_windows]
 * @param selectivity_map Optional selectivity map [num_windows]
 * @param num_windows Number of time windows (output)
 * @return 0 on success, non-zero on failure
 */
int audio_cortical_get_frequency_map(
    audio_cortical_bridge_t* bridge,
    const float* audio_data,
    uint32_t num_samples,
    uint32_t sample_rate,
    float* frequency_map,
    float* selectivity_map,
    uint32_t* num_windows
);

/* ============================================================================
 * Hypercolumn Functions
 * ============================================================================ */

/**
 * @brief Get hypercolumn at tonotopic position
 *
 * WHAT: Retrieves hypercolumn responsible for given frequency.
 * WHY:  Access individual hypercolumn for analysis.
 * HOW:  Uses tonotopic mapping to find hypercolumn index.
 *
 * @param bridge Audio-cortical bridge
 * @param frequency Frequency in Hz
 * @return Hypercolumn pointer, or NULL if not found
 */
const feature_hypercolumn_t* audio_cortical_get_hypercolumn(
    const audio_cortical_bridge_t* bridge,
    float frequency
);

/**
 * @brief Get hypercolumn by index
 *
 * WHAT: Retrieves hypercolumn by array index.
 * WHY:  Direct access for iteration.
 * HOW:  Returns pointer from internal array.
 *
 * @param bridge Audio-cortical bridge
 * @param index Hypercolumn index
 * @return Hypercolumn pointer, or NULL if out of bounds
 */
const feature_hypercolumn_t* audio_cortical_get_hypercolumn_by_index(
    const audio_cortical_bridge_t* bridge,
    uint32_t index
);

/**
 * @brief Get number of hypercolumns
 *
 * @param bridge Audio-cortical bridge
 * @return Number of hypercolumns, or 0 if bridge is NULL
 */
uint32_t audio_cortical_get_num_hypercolumns(const audio_cortical_bridge_t* bridge);

/* ============================================================================
 * Immune Modulation Functions
 * ============================================================================ */

/**
 * @brief Update immune modulation
 *
 * WHAT: Updates processing gains based on immune state.
 * WHY:  Apply inflammation effects to frequency processing.
 * HOW:  Queries immune system and adjusts gains.
 *
 * @param bridge Audio-cortical bridge
 * @return 0 on success, non-zero on failure
 */
int audio_cortical_update_immune_modulation(audio_cortical_bridge_t* bridge);

/**
 * @brief Set immune modulation factor
 *
 * WHAT: Manually sets immune modulation level.
 * WHY:  Allow direct control for testing or simulation.
 * HOW:  Sets internal modulation factor (0.0-1.0).
 *
 * @param bridge Audio-cortical bridge
 * @param factor Modulation factor (0.0 = no effect, 1.0 = full effect)
 * @return 0 on success, non-zero on failure
 */
int audio_cortical_set_immune_factor(
    audio_cortical_bridge_t* bridge,
    float factor
);

/**
 * @brief Get current immune modulation factor
 *
 * @param bridge Audio-cortical bridge
 * @return Current modulation factor, or 0.0 if bridge is NULL
 */
float audio_cortical_get_immune_factor(const audio_cortical_bridge_t* bridge);

/* ============================================================================
 * Statistics and State Functions
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieves processing and performance statistics.
 * WHY:  Monitoring and debugging.
 * HOW:  Copies internal stats to output structure.
 *
 * @param bridge Audio-cortical bridge
 * @param stats Output statistics structure
 * @return 0 on success, non-zero on failure
 */
int audio_cortical_get_stats(
    const audio_cortical_bridge_t* bridge,
    audio_cortical_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * WHAT: Clears all statistics counters.
 * WHY:  Fresh start for benchmarking.
 * HOW:  Zeros stats structure.
 *
 * @param bridge Audio-cortical bridge
 * @return 0 on success, non-zero on failure
 */
int audio_cortical_reset_stats(audio_cortical_bridge_t* bridge);

/**
 * @brief Get bridge state
 *
 * @param bridge Audio-cortical bridge
 * @return Current state, or UNINITIALIZED if bridge is NULL
 */
audio_cortical_state_t audio_cortical_get_state(
    const audio_cortical_bridge_t* bridge
);

/**
 * @brief Get tonotopic map
 *
 * WHAT: Returns the internal tonotopic map.
 * WHY:  Allow external access for analysis.
 * HOW:  Returns pointer to internal map.
 *
 * @param bridge Audio-cortical bridge
 * @return Tonotopic map, or NULL if not enabled
 */
const topographic_map_t* audio_cortical_get_tonotopic_map(
    const audio_cortical_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Message Handling
 * ============================================================================ */

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Handles incoming messages from other modules.
 * WHY:  Respond to attention shifts, immune updates, etc.
 * HOW:  Processes inbox and invokes handlers.
 *
 * @param bridge Audio-cortical bridge
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t audio_cortical_process_bio_messages(
    audio_cortical_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Broadcast frequency detection result
 *
 * WHAT: Sends frequency detection to interested modules.
 * WHY:  Notify downstream modules of auditory features.
 * HOW:  Sends BIO_MSG_CORTICAL_FREQUENCY_DETECTED.
 *
 * @param bridge Audio-cortical bridge
 * @param result Frequency result to broadcast
 * @return 0 on success, non-zero on failure
 */
int audio_cortical_broadcast_frequency(
    audio_cortical_bridge_t* bridge,
    const audio_cortical_frequency_result_t* result
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AUDIO_CORTICAL_BRIDGE_H */
