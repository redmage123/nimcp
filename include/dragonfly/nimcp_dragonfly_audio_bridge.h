/**
 * @file nimcp_dragonfly_audio_bridge.h
 * @brief Audio Cortex Bridge for Dragonfly Module
 *
 * WHAT: Connects dragonfly interception system to audio cortex perception
 * WHY:  Sound localization provides directional cues for multi-modal tracking
 * HOW:  Extracts direction and intensity from audio cortex spatial analysis
 *
 * INTEGRATION PIPELINE:
 * Audio Cortex → Sound Localization → Dragonfly TSDN → Tracking → Interception
 *
 * BIOLOGICAL REFERENCE:
 * - Some predatory insects use auditory cues for prey detection
 * - Sound localization via interaural time/level difference
 * - Multi-modal integration enhances tracking reliability
 *
 * KEY FEATURES:
 * - Sound localization (azimuth, elevation, distance estimate)
 * - Directional cueing to alert visual attention
 * - Multi-modal fusion with visual tracking
 * - Audio-visual correlation scoring
 *
 * @author NIMCP Team
 * @date 2024-12-28
 */

#ifndef NIMCP_DRAGONFLY_AUDIO_BRIDGE_H
#define NIMCP_DRAGONFLY_AUDIO_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "dragonfly/nimcp_dragonfly.h"

/* Forward declarations */
typedef struct audio_cortex_struct audio_cortex_t;
typedef struct dragonfly_audio_bridge_s dragonfly_audio_bridge_t;

//=============================================================================
// Constants
//=============================================================================

#define AUDIO_BRIDGE_MAX_SOURCES 8      /**< Max simultaneous sound sources */
#define AUDIO_BRIDGE_MAX_HISTORY 32     /**< History buffer for correlation */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Sound source detected via audio
 */
typedef struct {
    float azimuth;            /**< Horizontal direction (radians, 0=forward) */
    float elevation;          /**< Vertical direction (radians, 0=horizontal) */
    float intensity_db;       /**< Sound intensity (dB) */
    float distance_est;       /**< Estimated distance (meters, from attenuation) */
    float confidence;         /**< Localization confidence [0,1] */
    float frequency_hz;       /**< Dominant frequency (Hz) */
    float bandwidth_hz;       /**< Frequency bandwidth (Hz) */
    uint32_t source_id;       /**< Tracking source ID */
    uint64_t timestamp_us;    /**< Detection timestamp */
} audio_source_t;

/**
 * @brief Audio detection result
 */
typedef struct {
    audio_source_t sources[AUDIO_BRIDGE_MAX_SOURCES];
    uint32_t num_sources;
    float ambient_level_db;   /**< Background noise level */
    float peak_azimuth;       /**< Direction of loudest source */
    float peak_elevation;     /**< Elevation of loudest source */
    uint64_t timestamp_us;    /**< Processing timestamp */
} audio_detection_result_t;

/**
 * @brief Audio-visual correlation result
 */
typedef struct {
    uint32_t audio_source_id;     /**< Audio source being correlated */
    uint32_t visual_target_id;    /**< Matched visual target */
    float correlation_score;      /**< Correlation confidence [0,1] */
    float angular_difference;     /**< Angle between audio/visual (radians) */
    float temporal_offset_ms;     /**< Time offset between detections */
    bool is_matched;              /**< True if correlation exceeds threshold */
} audio_visual_correlation_t;

/**
 * @brief Audio localization mode
 */
typedef enum {
    AUDIO_LOC_ITD,           /**< Interaural Time Difference */
    AUDIO_LOC_ILD,           /**< Interaural Level Difference */
    AUDIO_LOC_COMBINED,      /**< ITD + ILD combined */
    AUDIO_LOC_SPECTRAL       /**< Spectral cues (HRTF-based) */
} audio_localization_mode_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Sound detection */
    float min_intensity_db;       /**< Minimum intensity to detect (dB) */
    float min_frequency_hz;       /**< Minimum frequency of interest (Hz) */
    float max_frequency_hz;       /**< Maximum frequency of interest (Hz) */
    float detection_threshold;    /**< Detection confidence threshold */

    /* Localization */
    audio_localization_mode_t loc_mode;  /**< Localization method */
    float ear_separation_m;       /**< Distance between microphones (m) */
    float speed_of_sound_mps;     /**< Speed of sound (m/s), ~343 */

    /* Distance estimation */
    bool estimate_distance;       /**< Enable distance estimation */
    float reference_db_at_1m;     /**< Reference level at 1 meter */
    float attenuation_exp;        /**< Distance attenuation exponent */

    /* Multi-modal fusion */
    bool enable_visual_fusion;    /**< Enable audio-visual correlation */
    float correlation_threshold;  /**< Min correlation to consider match */
    float max_angular_diff;       /**< Max angle difference for matching (radians) */
    float max_temporal_diff_ms;   /**< Max time difference for matching (ms) */

    /* Cueing */
    bool enable_attention_cue;    /**< Generate attention cues from audio */
    float cue_priority_boost;     /**< Priority boost for audio-cued targets */

    /* Filtering */
    float persistence_ms;         /**< Source persistence time (ms) */
    float smoothing_alpha;        /**< Direction smoothing (0-1) */
} audio_bridge_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t frames_processed;
    uint64_t sources_detected;
    uint64_t cues_generated;
    uint64_t correlations_found;
    float avg_process_time_us;
    float avg_sources_per_frame;
    float correlation_success_rate;
} audio_bridge_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default configuration
 */
audio_bridge_config_t audio_bridge_default_config(void);

/**
 * @brief Validate configuration
 */
bool audio_bridge_validate_config(const audio_bridge_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create audio bridge
 *
 * @param dragonfly Dragonfly system to cue
 * @param audio_cortex Audio cortex for processing (can be NULL for testing)
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
dragonfly_audio_bridge_t* dragonfly_audio_bridge_create(
    dragonfly_system_t* dragonfly,
    audio_cortex_t* audio_cortex,
    const audio_bridge_config_t* config
);

/**
 * @brief Destroy bridge
 */
void dragonfly_audio_bridge_destroy(dragonfly_audio_bridge_t* bridge);

/**
 * @brief Reset bridge state
 */
int dragonfly_audio_bridge_reset(dragonfly_audio_bridge_t* bridge);

//=============================================================================
// Processing Functions
//=============================================================================

/**
 * @brief Process audio frame
 *
 * WHAT: Extract sound sources and localize them
 * WHY:  Main entry point for audio input
 * HOW:  Uses audio cortex for frequency analysis, then localization
 *
 * @param bridge Audio bridge
 * @param samples Audio samples (interleaved if stereo)
 * @param num_samples Number of samples per channel
 * @param num_channels Number of channels (1 or 2)
 * @param sample_rate Sample rate (Hz)
 * @return 0 on success, -1 on error
 */
int dragonfly_audio_bridge_process_frame(
    dragonfly_audio_bridge_t* bridge,
    const float* samples,
    uint32_t num_samples,
    uint32_t num_channels,
    uint32_t sample_rate
);

/**
 * @brief Process pre-computed spectral features
 *
 * @param bridge Audio bridge
 * @param left_spectrum Left channel spectrum (magnitude)
 * @param right_spectrum Right channel spectrum (magnitude)
 * @param left_phase Left channel phase
 * @param right_phase Right channel phase
 * @param num_bins Number of frequency bins
 * @param sample_rate Sample rate (Hz)
 * @return 0 on success, -1 on error
 */
int dragonfly_audio_bridge_process_spectrum(
    dragonfly_audio_bridge_t* bridge,
    const float* left_spectrum,
    const float* right_spectrum,
    const float* left_phase,
    const float* right_phase,
    uint32_t num_bins,
    uint32_t sample_rate
);

/**
 * @brief Inject synthetic audio source (for testing)
 *
 * @param bridge Audio bridge
 * @param source Audio source to inject
 * @return 0 on success, -1 on error
 */
int dragonfly_audio_bridge_inject_source(
    dragonfly_audio_bridge_t* bridge,
    const audio_source_t* source
);

/**
 * @brief Get latest detection result
 */
int dragonfly_audio_bridge_get_result(
    const dragonfly_audio_bridge_t* bridge,
    audio_detection_result_t* result
);

//=============================================================================
// Multi-Modal Fusion Functions
//=============================================================================

/**
 * @brief Correlate audio with visual targets
 *
 * WHAT: Match audio sources to visual targets
 * WHY:  Multi-modal tracking improves reliability
 * HOW:  Compare directions and timing, score correlation
 *
 * @param bridge Audio bridge
 * @param visual_result Visual detection result (from visual bridge)
 * @param correlations Output: correlation results
 * @param max_correlations Maximum correlations to return
 * @return Number of correlations found, or -1 on error
 */
int dragonfly_audio_bridge_correlate_visual(
    dragonfly_audio_bridge_t* bridge,
    const void* visual_result,
    audio_visual_correlation_t* correlations,
    uint32_t max_correlations
);

/**
 * @brief Generate attention cue from audio
 *
 * WHAT: Create directional attention cue from loudest sound
 * WHY:  Sound alerts visual system to look in a direction
 *
 * @param bridge Audio bridge
 * @param cue_direction Output: suggested look direction (azimuth, elevation)
 * @param cue_priority Output: priority of cue [0,1]
 * @return 0 on success (cue generated), 1 if no cue, -1 on error
 */
int dragonfly_audio_bridge_get_attention_cue(
    const dragonfly_audio_bridge_t* bridge,
    float cue_direction[2],
    float* cue_priority
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Convert azimuth/elevation to 3D direction vector
 *
 * @param azimuth Horizontal angle (radians, 0=forward)
 * @param elevation Vertical angle (radians, 0=horizontal)
 * @param direction Output: unit direction vector [x, y, z]
 */
void dragonfly_audio_bridge_angles_to_vector(
    float azimuth,
    float elevation,
    float direction[3]
);

/**
 * @brief Estimate distance from sound intensity
 *
 * @param bridge Audio bridge (for reference level)
 * @param intensity_db Sound intensity (dB)
 * @return Estimated distance (meters)
 */
float dragonfly_audio_bridge_estimate_distance(
    const dragonfly_audio_bridge_t* bridge,
    float intensity_db
);

/**
 * @brief Calculate ITD from azimuth
 *
 * @param bridge Audio bridge (for ear separation)
 * @param azimuth Horizontal angle (radians)
 * @return Interaural time difference (seconds)
 */
float dragonfly_audio_bridge_azimuth_to_itd(
    const dragonfly_audio_bridge_t* bridge,
    float azimuth
);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get bridge statistics
 */
int dragonfly_audio_bridge_get_stats(
    const dragonfly_audio_bridge_t* bridge,
    audio_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 */
int dragonfly_audio_bridge_reset_stats(dragonfly_audio_bridge_t* bridge);

//=============================================================================
// Configuration Update
//=============================================================================

/**
 * @brief Update configuration
 */
int dragonfly_audio_bridge_set_config(
    dragonfly_audio_bridge_t* bridge,
    const audio_bridge_config_t* config
);

/**
 * @brief Get current configuration
 */
int dragonfly_audio_bridge_get_config(
    const dragonfly_audio_bridge_t* bridge,
    audio_bridge_config_t* config
);

/**
 * @brief Get localization mode name
 */
const char* dragonfly_audio_localization_mode_name(audio_localization_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_AUDIO_BRIDGE_H */
