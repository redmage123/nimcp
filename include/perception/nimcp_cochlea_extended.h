/**
 * @file nimcp_cochlea_extended.h
 * @brief Dog and Bat auditory extensions for cochlear processing
 *
 * WHAT: Non-human auditory capabilities (ultrasonic, echolocation)
 * WHY:  Enable extended hearing modes beyond human range
 * HOW:  Species-specific filterbanks, pinnae models, echolocation
 *
 * DOG AUDITORY SYSTEM (Canis lupus familiaris):
 * - Frequency range: 67 Hz - 45-65 kHz (vs human 20-20 kHz)
 * - Pinnae mobility: 18+ muscles per ear for directional hearing
 * - Sound localization: ~0.5° accuracy (vs human ~1°)
 * - Breed variations: Some breeds optimized for specific ranges
 *
 * BAT AUDITORY SYSTEM (Chiroptera):
 * - Frequency range: 1 kHz - 200 kHz
 * - Echolocation: FM sweeps, CF calls, CF-FM combinations
 * - Temporal resolution: 10-50 μs (vs human ~2 ms)
 * - Doppler processing: Velocity detection from frequency shift
 * - Range resolution: ~1 cm using time-of-flight
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#ifndef NIMCP_COCHLEA_EXTENDED_H
#define NIMCP_COCHLEA_EXTENDED_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"
#include "perception/nimcp_basilar_membrane.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Dog Auditory System Constants
//=============================================================================

/** Dog frequency range */
#define DOG_MIN_FREQ_HZ             67.0f
#define DOG_MAX_FREQ_HZ             65000.0f
#define DOG_ULTRASONIC_START_HZ     20000.0f

/** Dog directional hearing */
#define DOG_ITD_RESOLUTION_US       5.0f    /**< Interaural time diff resolution */
#define DOG_ILD_SENSITIVITY_DB      0.5f    /**< Interaural level diff sensitivity */
#define DOG_LOCALIZATION_ACCURACY   0.5f    /**< Degrees accuracy */

/** Dog pinnae parameters */
#define DOG_PINNAE_RANGE_DEG        180.0f  /**< Ear rotation range */
#define DOG_PINNAE_MUSCLES          18      /**< Muscles per ear */

//=============================================================================
// Bat Auditory System Constants
//=============================================================================

/** Bat frequency range */
#define BAT_MIN_FREQ_HZ             1000.0f
#define BAT_MAX_FREQ_HZ             200000.0f
#define BAT_ECHOLOCATION_MIN_HZ     20000.0f

/** Bat temporal precision */
#define BAT_TEMPORAL_RES_US         10.0f   /**< Temporal resolution */
#define BAT_RANGE_RES_CM            1.0f    /**< Range resolution */

/** Bat echolocation parameters */
#define BAT_CALL_DURATION_MIN_MS    0.5f
#define BAT_CALL_DURATION_MAX_MS    20.0f
#define BAT_PULSE_INTERVAL_MIN_MS   20.0f
#define BAT_PULSE_INTERVAL_MAX_MS   200.0f

/** Speed of sound for range calculation */
#define SPEED_OF_SOUND_MPS          343.0f

//=============================================================================
// Dog Enumerations
//=============================================================================

/**
 * @brief Dog breed hearing profiles
 *
 * Different breeds have evolved different hearing capabilities
 */
typedef enum {
    DOG_BREED_GENERIC,              /**< Generic dog profile */
    DOG_BREED_GERMAN_SHEPHERD,      /**< Excellent all-around hearing */
    DOG_BREED_BEAGLE,               /**< Good hearing, scent-focused */
    DOG_BREED_DALMATIAN,            /**< Prone to hearing issues */
    DOG_BREED_BORDER_COLLIE,        /**< High-frequency sensitive */
    DOG_BREED_COCKER_SPANIEL,       /**< Prone to ear infections */
    DOG_BREED_POODLE,               /**< Good hearing */
    DOG_BREED_LABRADOR              /**< Excellent water hearing */
} dog_breed_t;

/**
 * @brief Pinnae (ear) state
 */
typedef enum {
    DOG_PINNAE_NEUTRAL,             /**< Relaxed, forward-facing */
    DOG_PINNAE_ALERT,               /**< Erect, maximizing input */
    DOG_PINNAE_TRACKING,            /**< Actively following sound */
    DOG_PINNAE_FLAT                 /**< Laid back (fear/submission) */
} dog_pinnae_state_t;

//=============================================================================
// Bat Enumerations
//=============================================================================

/**
 * @brief Bat species echolocation profiles
 */
typedef enum {
    BAT_SPECIES_GENERIC,            /**< Generic FM bat */
    BAT_SPECIES_HORSESHOE,          /**< CF-FM specialists (Rhinolophidae) */
    BAT_SPECIES_VESPERTILIONID,     /**< FM sweepers (common bats) */
    BAT_SPECIES_PHYLLOSTOMID,       /**< Leaf-nosed, short broadband */
    BAT_SPECIES_PTEROPODID,         /**< Fruit bats, limited echolocation */
    BAT_SPECIES_MOLOSSID            /**< Free-tailed, low frequency calls */
} bat_species_t;

/**
 * @brief Echolocation call types
 */
typedef enum {
    ECHO_CALL_FM,                   /**< Frequency modulated sweep */
    ECHO_CALL_CF,                   /**< Constant frequency */
    ECHO_CALL_CF_FM,                /**< CF followed by FM sweep */
    ECHO_CALL_FM_CF_FM,             /**< FM-CF-FM sandwich */
    ECHO_CALL_CLICK                 /**< Broadband click */
} echolocation_call_type_t;

/**
 * @brief Echolocation processing mode
 */
typedef enum {
    ECHO_MODE_SEARCHING,            /**< Long-range search (long calls) */
    ECHO_MODE_APPROACH,             /**< Closing on target */
    ECHO_MODE_TERMINAL              /**< Final pursuit (rapid short calls) */
} echolocation_mode_t;

//=============================================================================
// Dog Structures
//=============================================================================

/**
 * @brief Dog pinnae (ear) model
 *
 * BIOLOGICAL: Dogs can rotate ears independently to localize sounds
 */
typedef struct {
    /* Ear orientation */
    float left_azimuth_deg;         /**< Left ear azimuth (-90 to +90) */
    float left_elevation_deg;       /**< Left ear elevation (-45 to +45) */
    float right_azimuth_deg;        /**< Right ear azimuth */
    float right_elevation_deg;      /**< Right ear elevation */

    /* Ear state */
    dog_pinnae_state_t left_state;  /**< Left ear state */
    dog_pinnae_state_t right_state; /**< Right ear state */

    /* Gain modulation from ear position */
    float left_gain_factor;         /**< Directional gain, left */
    float right_gain_factor;        /**< Directional gain, right */

    /* Head-related transfer function (simplified) */
    float* left_hrtf;               /**< Left ear HRTF [num_freqs] */
    float* right_hrtf;              /**< Right ear HRTF [num_freqs] */
    uint32_t num_hrtf_freqs;        /**< Number of HRTF frequencies */

} dog_pinnae_t;

/**
 * @brief Dog auditory configuration
 */
typedef struct {
    /* Frequency range */
    float max_freq_hz;              /**< Maximum frequency (45-65 kHz) */
    float ultrasonic_sensitivity;   /**< Sensitivity above 20 kHz (0-1) */

    /* Breed profile */
    dog_breed_t breed;              /**< Breed-specific profile */

    /* Directional hearing */
    bool enable_pinnae;             /**< Enable pinnae simulation */
    bool enable_localization;       /**< Enable ITD/ILD processing */
    float itd_resolution_us;        /**< ITD resolution */
    float ild_sensitivity_db;       /**< ILD sensitivity */

    /* Filterbank extension */
    uint32_t ultrasonic_channels;   /**< Additional ultrasonic channels */

} dog_auditory_config_t;

/**
 * @brief Dog sound localization result
 */
typedef struct {
    float azimuth_deg;              /**< Sound source azimuth */
    float elevation_deg;            /**< Sound source elevation */
    float distance_estimate_m;      /**< Estimated distance (if available) */
    float confidence;               /**< Localization confidence (0-1) */

    /* Binaural cues */
    float itd_us;                   /**< Interaural time difference */
    float ild_db;                   /**< Interaural level difference */

} dog_localization_t;

/**
 * @brief Dog extended auditory processor
 */
typedef struct dog_auditory dog_auditory_t;

//=============================================================================
// Bat Structures
//=============================================================================

/**
 * @brief Echolocation call parameters
 */
typedef struct {
    echolocation_call_type_t type;  /**< Call type */

    /* Frequency parameters */
    float start_freq_hz;            /**< Starting frequency (FM) or CF freq */
    float end_freq_hz;              /**< Ending frequency (FM) */
    float cf_freq_hz;               /**< Constant frequency component */

    /* Temporal parameters */
    float duration_ms;              /**< Call duration */
    float sweep_rate;               /**< Frequency change rate (Hz/ms) */

    /* Intensity */
    float intensity_db;             /**< Call intensity (dB SPL) */

    /* Harmonic structure */
    uint32_t num_harmonics;         /**< Number of harmonics */
    float* harmonic_levels;         /**< Relative harmonic levels */

} echolocation_call_t;

/**
 * @brief Echolocation target detection
 */
typedef struct {
    /* Target position */
    float range_m;                  /**< Distance to target (meters) */
    float azimuth_deg;              /**< Horizontal angle */
    float elevation_deg;            /**< Vertical angle */

    /* Target properties */
    float velocity_mps;             /**< Target velocity (from Doppler) */
    float target_strength;          /**< Echo strength (0-1) */
    float size_estimate_cm;         /**< Estimated target size */

    /* Confidence */
    float detection_confidence;     /**< Detection confidence (0-1) */
    float range_confidence;         /**< Range accuracy confidence */

} echolocation_target_t;

/**
 * @brief Echolocation result (multiple targets)
 */
typedef struct {
    echolocation_target_t* targets; /**< Detected targets array */
    uint32_t num_targets;           /**< Number of detected targets */
    uint32_t max_targets;           /**< Maximum target capacity */

    /* Scene summary */
    float nearest_target_m;         /**< Nearest target distance */
    float clutter_level;            /**< Background clutter (0-1) */
    echolocation_mode_t suggested_mode; /**< Suggested processing mode */

} echolocation_result_t;

/**
 * @brief Doppler processing state
 */
typedef struct {
    float reference_freq_hz;        /**< Reference/resting frequency */
    float current_shift_hz;         /**< Current Doppler shift */
    float shift_rate_hz_per_s;      /**< Shift rate */
    float velocity_estimate_mps;    /**< Estimated relative velocity */

    /* Doppler-shift compensation (horseshoe bats) */
    bool enable_dsc;                /**< Enable Doppler shift compensation */
    float compensation_amount;      /**< Current compensation */

} doppler_state_t;

/**
 * @brief Bat auditory configuration
 */
typedef struct {
    /* Frequency range */
    float max_freq_hz;              /**< Maximum frequency (up to 200 kHz) */
    float echolocation_band_min;    /**< Echolocation band start */
    float echolocation_band_max;    /**< Echolocation band end */

    /* Species profile */
    bat_species_t species;          /**< Species-specific profile */

    /* Echolocation settings */
    bool enable_echolocation;       /**< Enable echolocation processing */
    echolocation_call_type_t call_type; /**< Call type preference */

    /* Temporal processing */
    float temporal_resolution_us;   /**< Temporal resolution (10-50 μs) */
    float range_resolution_cm;      /**< Range resolution (cm) */

    /* Doppler processing */
    bool enable_doppler;            /**< Enable Doppler processing */
    float doppler_sensitivity;      /**< Doppler sensitivity (Hz per m/s) */

    /* Filterbank extension */
    uint32_t ultrasonic_channels;   /**< Ultrasonic frequency channels */

} bat_auditory_config_t;

/**
 * @brief Bat extended auditory processor
 */
typedef struct bat_auditory bat_auditory_t;

//=============================================================================
// Unified Extended Hearing
//=============================================================================

/**
 * @brief Extended hearing mode
 */
typedef enum {
    EXT_HEARING_HUMAN,              /**< Standard human hearing */
    EXT_HEARING_DOG,                /**< Dog mode */
    EXT_HEARING_BAT,                /**< Bat mode */
    EXT_HEARING_HYBRID,             /**< Combined capabilities */
    EXT_HEARING_CUSTOM              /**< Custom configuration */
} ext_hearing_mode_t;

/**
 * @brief Extended hearing configuration
 */
typedef struct {
    ext_hearing_mode_t mode;        /**< Active hearing mode */

    /* Mode-specific configs */
    dog_auditory_config_t dog;      /**< Dog configuration */
    bat_auditory_config_t bat;      /**< Bat configuration */

    /* Mode switching */
    bool enable_mode_switching;     /**< Allow dynamic mode changes */
    float mode_transition_ms;       /**< Transition time (ms) */

    /* Hybrid settings */
    float dog_weight;               /**< Dog mode contribution (0-1) */
    float bat_weight;               /**< Bat mode contribution (0-1) */

} ext_hearing_config_t;

/**
 * @brief Extended hearing processor
 */
typedef struct ext_hearing ext_hearing_t;

//=============================================================================
// Dog Auditory API
//=============================================================================

/**
 * @brief Get default dog auditory configuration
 *
 * @param breed Dog breed profile
 * @return Default configuration for breed
 */
dog_auditory_config_t dog_auditory_config_default(dog_breed_t breed);

/**
 * @brief Create dog auditory processor
 *
 * @param config Dog auditory configuration
 * @param sample_rate Audio sample rate (Hz)
 * @return Dog auditory processor or NULL
 */
dog_auditory_t* dog_auditory_create(
    const dog_auditory_config_t* config,
    uint32_t sample_rate
);

/**
 * @brief Destroy dog auditory processor
 *
 * @param dog Dog auditory processor
 */
void dog_auditory_destroy(dog_auditory_t* dog);

/**
 * @brief Process stereo audio through dog hearing
 *
 * @param dog Dog auditory processor
 * @param audio_left Left channel
 * @param audio_right Right channel
 * @param num_samples Number of samples
 * @param output Output channels [num_channels]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t dog_auditory_process(
    dog_auditory_t* dog,
    const float* audio_left,
    const float* audio_right,
    uint32_t num_samples,
    float* output
);

/**
 * @brief Set pinnae orientation
 *
 * @param dog Dog auditory processor
 * @param left_azimuth Left ear azimuth (degrees)
 * @param left_elevation Left ear elevation (degrees)
 * @param right_azimuth Right ear azimuth
 * @param right_elevation Right ear elevation
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t dog_auditory_set_pinnae(
    dog_auditory_t* dog,
    float left_azimuth,
    float left_elevation,
    float right_azimuth,
    float right_elevation
);

/**
 * @brief Localize sound source
 *
 * @param dog Dog auditory processor
 * @param result Output localization result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t dog_auditory_localize(
    dog_auditory_t* dog,
    dog_localization_t* result
);

/**
 * @brief Get ultrasonic detection
 *
 * @param dog Dog auditory processor
 * @param freq_hz Output: detected ultrasonic frequency
 * @param level Output: signal level
 * @return true if ultrasonic detected
 */
bool dog_auditory_detect_ultrasonic(
    dog_auditory_t* dog,
    float* freq_hz,
    float* level
);

//=============================================================================
// Bat Auditory API
//=============================================================================

/**
 * @brief Get default bat auditory configuration
 *
 * @param species Bat species profile
 * @return Default configuration for species
 */
bat_auditory_config_t bat_auditory_config_default(bat_species_t species);

/**
 * @brief Create bat auditory processor
 *
 * @param config Bat auditory configuration
 * @param sample_rate Audio sample rate (Hz, should be >= 400 kHz)
 * @return Bat auditory processor or NULL
 */
bat_auditory_t* bat_auditory_create(
    const bat_auditory_config_t* config,
    uint32_t sample_rate
);

/**
 * @brief Destroy bat auditory processor
 *
 * @param bat Bat auditory processor
 */
void bat_auditory_destroy(bat_auditory_t* bat);

/**
 * @brief Process audio through bat hearing
 *
 * @param bat Bat auditory processor
 * @param audio_in Input audio (high sample rate)
 * @param num_samples Number of samples
 * @param output Frequency channel outputs
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t bat_auditory_process(
    bat_auditory_t* bat,
    const float* audio_in,
    uint32_t num_samples,
    float* output
);

/**
 * @brief Generate echolocation call
 *
 * @param bat Bat auditory processor
 * @param call Call parameters
 * @param audio_out Output audio buffer
 * @param buffer_size Buffer size (samples)
 * @param num_samples_out Actual samples generated
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t bat_auditory_generate_call(
    bat_auditory_t* bat,
    const echolocation_call_t* call,
    float* audio_out,
    uint32_t buffer_size,
    uint32_t* num_samples_out
);

/**
 * @brief Process echo return
 *
 * WHAT: Analyze echo for target detection
 * WHY:  Extract range, velocity, and direction from echoes
 * HOW:  Correlate with outgoing call, detect Doppler shift
 *
 * @param bat Bat auditory processor
 * @param echo_audio Received echo signal
 * @param num_samples Number of samples
 * @param result Echolocation result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t bat_auditory_process_echo(
    bat_auditory_t* bat,
    const float* echo_audio,
    uint32_t num_samples,
    echolocation_result_t* result
);

/**
 * @brief Set echolocation mode
 *
 * @param bat Bat auditory processor
 * @param mode Processing mode (search/approach/terminal)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t bat_auditory_set_mode(
    bat_auditory_t* bat,
    echolocation_mode_t mode
);

/**
 * @brief Get Doppler processing state
 *
 * @param bat Bat auditory processor
 * @param state Output Doppler state
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t bat_auditory_get_doppler(
    const bat_auditory_t* bat,
    doppler_state_t* state
);

/**
 * @brief Apply Doppler shift compensation
 *
 * BIOLOGICAL: Horseshoe bats adjust call frequency to compensate
 * for Doppler shift, keeping echoes in optimal frequency range
 *
 * @param bat Bat auditory processor
 * @param enable Enable/disable DSC
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t bat_auditory_enable_dsc(bat_auditory_t* bat, bool enable);

//=============================================================================
// Echolocation Result Management
//=============================================================================

/**
 * @brief Create echolocation result structure
 *
 * @param max_targets Maximum targets to track
 * @return Result structure or NULL
 */
echolocation_result_t* echolocation_result_create(uint32_t max_targets);

/**
 * @brief Destroy echolocation result
 *
 * @param result Result to destroy
 */
void echolocation_result_destroy(echolocation_result_t* result);

/**
 * @brief Clear echolocation result
 *
 * @param result Result to clear
 */
void echolocation_result_clear(echolocation_result_t* result);

//=============================================================================
// Extended Hearing API
//=============================================================================

/**
 * @brief Get default extended hearing configuration
 *
 * @param mode Hearing mode
 * @return Default configuration
 */
ext_hearing_config_t ext_hearing_config_default(ext_hearing_mode_t mode);

/**
 * @brief Create extended hearing processor
 *
 * @param config Extended hearing configuration
 * @param sample_rate Audio sample rate
 * @return Extended hearing processor or NULL
 */
ext_hearing_t* ext_hearing_create(
    const ext_hearing_config_t* config,
    uint32_t sample_rate
);

/**
 * @brief Destroy extended hearing processor
 *
 * @param ext Extended hearing processor
 */
void ext_hearing_destroy(ext_hearing_t* ext);

/**
 * @brief Process audio through extended hearing
 *
 * @param ext Extended hearing processor
 * @param audio_in Input audio
 * @param num_samples Number of samples
 * @param output Channel outputs
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t ext_hearing_process(
    ext_hearing_t* ext,
    const float* audio_in,
    uint32_t num_samples,
    float* output
);

/**
 * @brief Switch hearing mode
 *
 * @param ext Extended hearing processor
 * @param mode Target mode
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t ext_hearing_switch_mode(
    ext_hearing_t* ext,
    ext_hearing_mode_t mode
);

/**
 * @brief Get current hearing mode
 *
 * @param ext Extended hearing processor
 * @return Current mode
 */
ext_hearing_mode_t ext_hearing_get_mode(const ext_hearing_t* ext);

/**
 * @brief Get dog processor from extended hearing
 *
 * @param ext Extended hearing processor
 * @return Dog processor or NULL if not in dog/hybrid mode
 */
dog_auditory_t* ext_hearing_get_dog(ext_hearing_t* ext);

/**
 * @brief Get bat processor from extended hearing
 *
 * @param ext Extended hearing processor
 * @return Bat processor or NULL if not in bat/hybrid mode
 */
bat_auditory_t* ext_hearing_get_bat(ext_hearing_t* ext);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COCHLEA_EXTENDED_H */
