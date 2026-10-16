/**
 * @file nimcp_echolocation.h
 * @brief 3D Spatial Mapping from Echoes - Superhuman Echolocation Module
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bio-inspired echolocation for 3D environment reconstruction
 * WHY:  Enable spatial perception without visual input
 * HOW:  Echo processing, delay analysis, 3D reconstruction from reflections
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * BAT ECHOLOCATION:
 * ----------------
 * 1. Temporal Resolution:
 *    - Bats resolve time delays down to ~10 microseconds
 *    - Enables millimeter-scale distance discrimination
 *    - Doppler shift detection for velocity estimation
 *    - Reference: Moss & Surlykke (2010) "Probing the natural scene by echolocation"
 *
 * 2. Neural Processing:
 *    - Inferior colliculus: delay-tuned neurons
 *    - Auditory cortex: topographic delay maps
 *    - Combination-sensitive neurons: respond to call-echo pairs
 *    - Reference: Suga (1990) "Biosonar and neural computation"
 *
 * 3. Spatial Reconstruction:
 *    - Range: time delay between call and echo
 *    - Azimuth: interaural intensity/time differences
 *    - Elevation: spectral notches from pinna filtering
 *    - Reference: Simmons (2012) "Target image representation in bat sonar"
 *
 * DOLPHIN ECHOLOCATION:
 * --------------------
 * 1. Click Trains:
 *    - Broadband clicks (10-150 kHz)
 *    - Adjustable click rate based on target distance
 *    - Melon focusing for directional transmission
 *
 * 2. Object Recognition:
 *    - Material discrimination via echo texture
 *    - Shape recognition from echo envelope
 *    - Size estimation from echo amplitude
 *
 * HUMAN ECHOLOCATION:
 * ------------------
 * 1. Tongue Clicks:
 *    - Trained humans achieve 1-3m ranging accuracy
 *    - Primary cue: echo-to-background loudness ratio
 *    - Secondary: spectral coloration from surface properties
 *    - Reference: Thaler et al. (2011) "Neural correlates of motion processing"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ECHOLOCATION_H
#define NIMCP_ECHOLOCATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define ECHOLOCATION_SPEED_OF_SOUND     343.0f  /**< Speed of sound (m/s) at 20C */
#define ECHOLOCATION_MAX_RANGE          100.0f  /**< Maximum detection range (m) */
#define ECHOLOCATION_MIN_RANGE          0.1f    /**< Minimum detection range (m) */
#define ECHOLOCATION_MAX_ECHOES         256     /**< Maximum echoes per pulse */
#define ECHOLOCATION_MAX_OBJECTS        128     /**< Maximum detected objects */
#define ECHOLOCATION_SAMPLE_RATE        192000  /**< Audio sample rate (Hz) */
#define ECHOLOCATION_PULSE_HISTORY      32      /**< Pulses for tracking */
#define ECHOLOCATION_MAP_RESOLUTION     0.1f    /**< Map grid resolution (m) */
#define ECHOLOCATION_MAX_MAP_CELLS      10000   /**< Maximum map cells */

/* ============================================================================
 * Error Codes
 * ============================================================================ */

typedef enum {
    ECHOLOCATION_SUCCESS                 = 0,
    ECHOLOCATION_ERROR_NULL_POINTER      = -1,
    ECHOLOCATION_ERROR_INVALID_PARAM     = -2,
    ECHOLOCATION_ERROR_NO_MEMORY         = -3,
    ECHOLOCATION_ERROR_NOT_INITIALIZED   = -4,
    ECHOLOCATION_ERROR_INVALID_STATE     = -5,
    ECHOLOCATION_ERROR_BUFFER_TOO_SMALL  = -6,
    ECHOLOCATION_ERROR_NO_SIGNAL         = -7,
    ECHOLOCATION_ERROR_PROCESSING_FAILED = -8,
    ECHOLOCATION_ERROR_TIMEOUT           = -9
} echolocation_error_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Pulse type for echolocation signal
 */
typedef enum {
    ECHO_PULSE_CLICK,       /**< Short broadband click (dolphin-like) */
    ECHO_PULSE_CHIRP,       /**< Frequency-modulated sweep (bat-like) */
    ECHO_PULSE_CONSTANT_FREQ, /**< Constant frequency pulse (CF bat) */
    ECHO_PULSE_TONGUE_CLICK /**< Human-style tongue click */
} echo_pulse_type_t;

/**
 * @brief Surface material classification
 */
typedef enum {
    ECHO_MATERIAL_UNKNOWN,  /**< Unclassified material */
    ECHO_MATERIAL_HARD,     /**< Hard reflective surface */
    ECHO_MATERIAL_SOFT,     /**< Sound-absorbing surface */
    ECHO_MATERIAL_METAL,    /**< Metallic surface */
    ECHO_MATERIAL_GLASS,    /**< Glass/smooth surface */
    ECHO_MATERIAL_FABRIC,   /**< Fabric/cloth surface */
    ECHO_MATERIAL_ORGANIC,  /**< Organic material (vegetation) */
    ECHO_MATERIAL_LIQUID    /**< Water/liquid surface */
} echo_material_t;

/**
 * @brief Object shape classification
 */
typedef enum {
    ECHO_SHAPE_UNKNOWN,     /**< Unknown shape */
    ECHO_SHAPE_POINT,       /**< Point-like reflector */
    ECHO_SHAPE_PLANE,       /**< Flat surface */
    ECHO_SHAPE_CORNER,      /**< Corner reflector (strong return) */
    ECHO_SHAPE_SPHERE,      /**< Spherical object */
    ECHO_SHAPE_CYLINDER,    /**< Cylindrical object */
    ECHO_SHAPE_COMPLEX      /**< Complex geometry */
} echo_shape_t;

/**
 * @brief Processing mode
 */
typedef enum {
    ECHO_MODE_RANGING,      /**< Simple range detection */
    ECHO_MODE_IMAGING,      /**< Full 3D imaging */
    ECHO_MODE_TRACKING,     /**< Moving object tracking */
    ECHO_MODE_NAVIGATION    /**< Environment mapping */
} echo_processing_mode_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief 3D point in space
 */
typedef struct {
    float x;            /**< X coordinate (meters) */
    float y;            /**< Y coordinate (meters) */
    float z;            /**< Z coordinate (meters) */
} echo_point3d_t;

/**
 * @brief Spherical coordinates
 */
typedef struct {
    float range;        /**< Distance (meters) */
    float azimuth;      /**< Horizontal angle (radians) */
    float elevation;    /**< Vertical angle (radians) */
} echo_spherical_t;

/**
 * @brief Raw echo detection
 */
typedef struct {
    uint32_t echo_id;           /**< Unique echo identifier */
    float time_delay_us;        /**< Round-trip time (microseconds) */
    float range;                /**< Calculated range (meters) */
    float amplitude;            /**< Echo amplitude [0-1] */
    float doppler_shift_hz;     /**< Doppler frequency shift */
    float radial_velocity;      /**< Radial velocity (m/s) */
    echo_spherical_t direction; /**< Echo arrival direction */
    float confidence;           /**< Detection confidence [0-1] */
    float spectral_centroid;    /**< Spectral centroid (Hz) */
    float spectral_spread;      /**< Spectral spread (Hz) */
} echo_detection_t;

/**
 * @brief Detected object from multiple echoes
 */
typedef struct {
    uint32_t object_id;         /**< Unique object identifier */
    echo_point3d_t position;    /**< 3D position */
    echo_point3d_t velocity;    /**< 3D velocity (if moving) */
    float size_estimate;        /**< Estimated size (m) */
    echo_material_t material;   /**< Classified material */
    echo_shape_t shape;         /**< Classified shape */
    float reflectivity;         /**< Surface reflectivity [0-1] */
    float confidence;           /**< Object confidence [0-1] */
    uint32_t echo_count;        /**< Number of contributing echoes */
    bool is_moving;             /**< Object in motion */
    uint32_t frames_tracked;    /**< Tracking duration */
} echo_object_t;

/**
 * @brief Map cell for environment reconstruction
 */
typedef struct {
    echo_point3d_t center;      /**< Cell center position */
    float occupancy;            /**< Occupancy probability [0-1] */
    echo_material_t material;   /**< Dominant material */
    uint32_t observation_count; /**< Times observed */
    float last_update_time;     /**< Last observation time */
} echo_map_cell_t;

/**
 * @brief Transmitted pulse parameters
 */
typedef struct {
    echo_pulse_type_t type;     /**< Pulse type */
    float frequency_start;      /**< Start frequency (Hz) */
    float frequency_end;        /**< End frequency (Hz, for chirp) */
    float duration_us;          /**< Pulse duration (microseconds) */
    float amplitude;            /**< Transmission amplitude [0-1] */
    echo_spherical_t direction; /**< Transmission direction */
    float beam_width;           /**< Beam width (degrees) */
} echo_pulse_params_t;

/**
 * @brief Audio signal buffer
 */
typedef struct {
    const float* samples;       /**< Audio samples */
    uint32_t num_samples;       /**< Number of samples */
    uint32_t sample_rate;       /**< Sample rate (Hz) */
    uint32_t num_channels;      /**< Number of channels (1=mono, 2=binaural) */
    float timestamp_ms;         /**< Buffer timestamp */
} echo_audio_buffer_t;

/**
 * @brief Echolocation system configuration
 */
typedef struct {
    /* Pulse settings */
    echo_pulse_type_t pulse_type;       /**< Default pulse type */
    float pulse_frequency;              /**< Center/start frequency (Hz) */
    float pulse_bandwidth;              /**< Bandwidth for chirp (Hz) */
    float pulse_duration_us;            /**< Pulse duration */
    float pulse_interval_ms;            /**< Time between pulses */

    /* Detection settings */
    float detection_threshold;          /**< Minimum echo amplitude */
    float max_range;                    /**< Maximum detection range (m) */
    float min_range;                    /**< Minimum detection range (m) */
    float range_resolution;             /**< Range resolution (m) */
    float angular_resolution;           /**< Angular resolution (degrees) */

    /* Processing settings */
    echo_processing_mode_t mode;        /**< Processing mode */
    bool enable_doppler;                /**< Enable Doppler processing */
    bool enable_spectral_analysis;      /**< Enable spectral classification */
    bool enable_material_classification; /**< Enable material classification */
    bool enable_object_tracking;        /**< Enable object tracking */
    bool enable_mapping;                /**< Enable environment mapping */

    /* Binaural settings */
    bool binaural_mode;                 /**< Use binaural processing */
    float ear_separation;               /**< Distance between ears (m) */

    /* Performance */
    uint32_t fft_size;                  /**< FFT size for spectral analysis */
    uint32_t max_echoes;                /**< Maximum echoes per pulse */
    uint32_t max_objects;               /**< Maximum tracked objects */
} echolocation_config_t;

/**
 * @brief Processing state
 */
typedef struct {
    /* Current pulse state */
    uint64_t pulse_count;               /**< Total pulses emitted */
    float last_pulse_time_ms;           /**< Last pulse timestamp */
    bool pulse_in_flight;               /**< Waiting for echoes */

    /* Detection state */
    uint32_t echoes_detected;           /**< Echoes this pulse */
    uint32_t objects_detected;          /**< Objects detected */
    float nearest_range;                /**< Nearest echo range */
    float farthest_range;               /**< Farthest echo range */

    /* Environment state */
    float ambient_noise_level;          /**< Background noise level */
    float signal_to_noise;              /**< Current SNR (dB) */
    uint32_t map_cells_occupied;        /**< Occupied map cells */

    /* Processing state */
    bool is_initialized;                /**< System initialized */
    float processing_load;              /**< Processing load [0-1] */
} echolocation_state_t;

/**
 * @brief Accumulated statistics
 */
typedef struct {
    /* Pulse statistics */
    uint64_t total_pulses;              /**< All pulses emitted */
    uint64_t total_echoes;              /**< All echoes detected */
    float avg_echoes_per_pulse;         /**< Average echoes/pulse */

    /* Detection statistics */
    uint64_t total_objects_detected;    /**< All objects detected */
    float avg_detection_range;          /**< Average detection range */
    float max_detection_range;          /**< Maximum range achieved */
    float avg_detection_confidence;     /**< Average confidence */

    /* Accuracy statistics */
    float range_accuracy;               /**< Range estimation accuracy */
    float angular_accuracy;             /**< Angular estimation accuracy */
    float velocity_accuracy;            /**< Velocity estimation accuracy */

    /* Performance statistics */
    float avg_processing_time_ms;       /**< Average processing time */
    float max_processing_time_ms;       /**< Maximum processing time */
    uint64_t classification_count;      /**< Materials classified */
} echolocation_stats_t;

/**
 * @brief Environment map for navigation
 */
typedef struct {
    echo_map_cell_t* cells;             /**< Map cells array */
    uint32_t num_cells;                 /**< Number of cells */
    uint32_t max_cells;                 /**< Array capacity */
    echo_point3d_t bounds_min;          /**< Map minimum bounds */
    echo_point3d_t bounds_max;          /**< Map maximum bounds */
    float resolution;                   /**< Grid resolution */
    uint64_t last_update;               /**< Last update timestamp */
} echo_environment_map_t;

/**
 * @brief Processing output
 */
typedef struct {
    /* Raw echoes */
    echo_detection_t* echoes;           /**< Detected echoes array */
    uint32_t num_echoes;                /**< Number of echoes */
    uint32_t max_echoes;                /**< Array capacity */

    /* Detected objects */
    echo_object_t* objects;             /**< Detected objects array */
    uint32_t num_objects;               /**< Number of objects */
    uint32_t max_objects;               /**< Array capacity */

    /* Processing metadata */
    float processing_time_ms;           /**< Processing time */
    uint64_t pulse_number;              /**< Pulse sequence number */
    float signal_to_noise_db;           /**< Achieved SNR */
} echolocation_output_t;

/**
 * @brief Opaque echolocation system handle
 */
typedef struct echolocation_system echolocation_system_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Initialize config with biologically-plausible defaults
 * WHY:  Provide sensible starting point for echolocation
 * HOW:  Set all fields to validated default values
 *
 * @param config Output configuration structure
 * @return ECHOLOCATION_SUCCESS or error code
 */
int echolocation_default_config(echolocation_config_t* config);

/**
 * @brief Create echolocation processing system
 *
 * WHAT: Allocate and initialize echolocation processor
 * WHY:  Enable 3D spatial mapping from echoes
 * HOW:  Allocate buffers, initialize FFT, setup tracking
 *
 * @param config Configuration (NULL for defaults)
 * @return New echolocation system or NULL on failure
 */
echolocation_system_t* echolocation_create(const echolocation_config_t* config);

/**
 * @brief Destroy echolocation system
 *
 * WHAT: Release all resources
 * WHY:  Clean shutdown and memory management
 * HOW:  Free FFT, buffers, map, tracking state
 *
 * @param system System to destroy (NULL-safe)
 */
void echolocation_destroy(echolocation_system_t* system);

/**
 * @brief Reset system to initial state
 *
 * WHAT: Clear all tracking and mapping without reallocation
 * WHY:  Prepare for new environment
 * HOW:  Zero state, clear map, reset statistics
 *
 * @param system System to reset
 * @return ECHOLOCATION_SUCCESS or error code
 */
int echolocation_reset(echolocation_system_t* system);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Update system configuration
 *
 * @param system Active system
 * @param config New configuration
 * @return ECHOLOCATION_SUCCESS or error code
 */
int echolocation_set_config(echolocation_system_t* system,
                            const echolocation_config_t* config);

/**
 * @brief Get current configuration
 *
 * @param system Active system
 * @param config Output configuration
 * @return ECHOLOCATION_SUCCESS or error code
 */
int echolocation_get_config(const echolocation_system_t* system,
                            echolocation_config_t* config);

/**
 * @brief Set pulse parameters
 *
 * @param system Active system
 * @param params New pulse parameters
 * @return ECHOLOCATION_SUCCESS or error code
 */
int echolocation_set_pulse_params(echolocation_system_t* system,
                                  const echo_pulse_params_t* params);

/**
 * @brief Set processing mode
 *
 * @param system Active system
 * @param mode New processing mode
 * @return ECHOLOCATION_SUCCESS or error code
 */
int echolocation_set_mode(echolocation_system_t* system,
                          echo_processing_mode_t mode);

/* ============================================================================
 * Processing API
 * ============================================================================ */

/**
 * @brief Generate echolocation pulse
 *
 * WHAT: Create outgoing echolocation signal
 * WHY:  Initiate echo detection cycle
 * HOW:  Generate pulse waveform based on configuration
 *
 * @param system Active system
 * @param output_samples Output buffer for pulse samples
 * @param buffer_size Buffer size in samples
 * @param num_samples Output: samples generated
 * @return ECHOLOCATION_SUCCESS or error code
 */
int echolocation_generate_pulse(echolocation_system_t* system,
                                float* output_samples,
                                uint32_t buffer_size,
                                uint32_t* num_samples);

/**
 * @brief Process received audio for echoes
 *
 * WHAT: Analyze audio for echo detections
 * WHY:  Main entry point for echolocation processing
 * HOW:  Matched filter, peak detection, classification
 *
 * @param system Active system
 * @param audio Input audio buffer
 * @param output Processing output
 * @return ECHOLOCATION_SUCCESS or error code
 */
int echolocation_process_audio(echolocation_system_t* system,
                               const echo_audio_buffer_t* audio,
                               echolocation_output_t* output);

/**
 * @brief Detect echoes in audio
 *
 * @param system Active system
 * @param audio Input audio
 * @param echoes Output echoes array
 * @param max_echoes Array capacity
 * @param num_echoes Output: echoes detected
 * @return ECHOLOCATION_SUCCESS or error code
 */
int echolocation_detect_echoes(echolocation_system_t* system,
                               const echo_audio_buffer_t* audio,
                               echo_detection_t* echoes,
                               uint32_t max_echoes,
                               uint32_t* num_echoes);

/**
 * @brief Analyze echo for Doppler shift
 *
 * @param system Active system
 * @param echo Echo to analyze
 * @param velocity Output radial velocity (m/s)
 * @return ECHOLOCATION_SUCCESS or error code
 */
int echolocation_analyze_doppler(echolocation_system_t* system,
                                 echo_detection_t* echo,
                                 float* velocity);

/**
 * @brief Classify echo material
 *
 * WHAT: Determine surface material from echo properties
 * WHY:  Enable environment understanding
 * HOW:  Spectral analysis of echo signature
 *
 * @param system Active system
 * @param echo Echo to classify
 * @param material Output material classification
 * @param confidence Output classification confidence
 * @return ECHOLOCATION_SUCCESS or error code
 */
int echolocation_classify_material(echolocation_system_t* system,
                                   const echo_detection_t* echo,
                                   echo_material_t* material,
                                   float* confidence);

/**
 * @brief Cluster echoes into objects
 *
 * @param system Active system
 * @param echoes Input echoes
 * @param num_echoes Number of echoes
 * @param objects Output objects array
 * @param max_objects Array capacity
 * @param num_objects Output: objects formed
 * @return ECHOLOCATION_SUCCESS or error code
 */
int echolocation_cluster_objects(echolocation_system_t* system,
                                 const echo_detection_t* echoes,
                                 uint32_t num_echoes,
                                 echo_object_t* objects,
                                 uint32_t max_objects,
                                 uint32_t* num_objects);

/**
 * @brief Track objects across pulses
 *
 * @param system Active system
 * @param objects Current objects
 * @param num_objects Number of objects
 * @return ECHOLOCATION_SUCCESS or error code
 */
int echolocation_track_objects(echolocation_system_t* system,
                               echo_object_t* objects,
                               uint32_t num_objects);

/* ============================================================================
 * 3D Reconstruction API
 * ============================================================================ */

/**
 * @brief Convert echo to 3D position
 *
 * WHAT: Calculate 3D position from echo parameters
 * WHY:  Spatial localization of reflector
 * HOW:  Range, azimuth, elevation to Cartesian
 *
 * @param system Active system
 * @param echo Echo detection
 * @param position Output 3D position
 * @return ECHOLOCATION_SUCCESS or error code
 */
int echolocation_echo_to_position(echolocation_system_t* system,
                                  const echo_detection_t* echo,
                                  echo_point3d_t* position);

/**
 * @brief Update environment map
 *
 * @param system Active system
 * @param objects Detected objects
 * @param num_objects Number of objects
 * @return ECHOLOCATION_SUCCESS or error code
 */
int echolocation_update_map(echolocation_system_t* system,
                            const echo_object_t* objects,
                            uint32_t num_objects);

/**
 * @brief Get environment map
 *
 * @param system Active system
 * @param map Output map structure
 * @return ECHOLOCATION_SUCCESS or error code
 */
int echolocation_get_map(const echolocation_system_t* system,
                         echo_environment_map_t** map);

/**
 * @brief Clear environment map
 *
 * @param system Active system
 * @return ECHOLOCATION_SUCCESS or error code
 */
int echolocation_clear_map(echolocation_system_t* system);

/**
 * @brief Query map at position
 *
 * @param system Active system
 * @param position Query position
 * @param cell Output map cell (NULL if unoccupied)
 * @return ECHOLOCATION_SUCCESS or error code
 */
int echolocation_query_map(const echolocation_system_t* system,
                           echo_point3d_t position,
                           echo_map_cell_t* cell);

/* ============================================================================
 * State and Statistics API
 * ============================================================================ */

/**
 * @brief Get current processing state
 *
 * @param system Active system
 * @param state Output state structure
 * @return ECHOLOCATION_SUCCESS or error code
 */
int echolocation_get_state(const echolocation_system_t* system,
                           echolocation_state_t* state);

/**
 * @brief Get accumulated statistics
 *
 * @param system Active system
 * @param stats Output statistics structure
 * @return ECHOLOCATION_SUCCESS or error code
 */
int echolocation_get_stats(const echolocation_system_t* system,
                           echolocation_stats_t* stats);

/**
 * @brief Reset statistics counters
 *
 * @param system Active system
 * @return ECHOLOCATION_SUCCESS or error code
 */
int echolocation_reset_stats(echolocation_system_t* system);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Allocate output structure
 *
 * @param max_echoes Maximum echoes capacity
 * @param max_objects Maximum objects capacity
 * @return Allocated output or NULL
 */
echolocation_output_t* echolocation_output_create(uint32_t max_echoes,
                                                  uint32_t max_objects);

/**
 * @brief Free output structure
 *
 * @param output Output to free (NULL-safe)
 */
void echolocation_output_destroy(echolocation_output_t* output);

/**
 * @brief Convert time delay to range
 *
 * @param delay_us Time delay in microseconds
 * @return Range in meters
 */
float echolocation_delay_to_range(float delay_us);

/**
 * @brief Convert range to time delay
 *
 * @param range Range in meters
 * @return Time delay in microseconds
 */
float echolocation_range_to_delay(float range);

/**
 * @brief Convert Doppler shift to velocity
 *
 * @param doppler_hz Doppler shift in Hz
 * @param carrier_hz Carrier frequency in Hz
 * @return Radial velocity in m/s
 */
float echolocation_doppler_to_velocity(float doppler_hz, float carrier_hz);

/**
 * @brief Get error description string
 *
 * @param error Error code
 * @return Human-readable error description
 */
const char* echolocation_error_string(echolocation_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ECHOLOCATION_H */
