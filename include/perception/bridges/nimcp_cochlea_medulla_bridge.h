/**
 * @file nimcp_cochlea_medulla_bridge.h
 * @brief Cochlea-Medulla brainstem integration bridge
 *
 * WHAT: Connect cochlear processing to brainstem nuclei and protective reflexes
 * WHY:  Enable biological auditory pathway and protective mechanisms
 * HOW:  Cochlear nucleus → Superior olive → Inferior colliculus pathway
 *
 * BIOLOGICAL BASIS:
 * - Cochlear Nucleus (CN): First brainstem relay, timing preservation
 * - Superior Olivary Complex (SOC): Binaural processing, ITD/ILD computation
 * - Inferior Colliculus (IC): Multimodal integration, frequency mapping
 * - Acoustic Reflex: Stapedius muscle contraction for loud sound protection
 *
 * BIDIRECTIONAL DATA FLOWS:
 * - OUTBOUND: Cochlea → Medulla: ANF spikes, sound levels, emergency signals
 * - INBOUND:  Medulla → Cochlea: Arousal modulation, protective attenuation
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#ifndef NIMCP_COCHLEA_MEDULLA_BRIDGE_H
#define NIMCP_COCHLEA_MEDULLA_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"
#include "perception/nimcp_cochlea.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct medulla medulla_t;
// arousal_state_t defined in core/medulla/nimcp_arousal_state.h
struct arousal_state_struct;
typedef struct circadian_clock circadian_clock_t;

//=============================================================================
// Constants
//=============================================================================

/** Acoustic reflex parameters */
#define COCHLEA_MEDULLA_REFLEX_THRESHOLD_DB     85.0f   /**< Reflex trigger (dB SPL) */
#define COCHLEA_MEDULLA_REFLEX_LATENCY_MIN_MS   25.0f   /**< Minimum latency */
#define COCHLEA_MEDULLA_REFLEX_LATENCY_MAX_MS   150.0f  /**< Maximum latency */
#define COCHLEA_MEDULLA_REFLEX_MAX_ATTEN_DB     14.0f   /**< Maximum attenuation */

/** Brainstem nuclei channels */
#define COCHLEA_MEDULLA_CN_CHANNELS             128     /**< Cochlear nucleus */
#define COCHLEA_MEDULLA_SOC_CHANNELS            64      /**< Superior olive */
#define COCHLEA_MEDULLA_IC_CHANNELS             64      /**< Inferior colliculus */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Arousal level affecting cochlear sensitivity
 */
typedef enum {
    AROUSAL_LEVEL_SLEEP,            /**< Deep sleep - reduced sensitivity */
    AROUSAL_LEVEL_DROWSY,           /**< Drowsy - slightly reduced */
    AROUSAL_LEVEL_RELAXED,          /**< Relaxed wakefulness */
    AROUSAL_LEVEL_ALERT,            /**< Alert - normal sensitivity */
    AROUSAL_LEVEL_VIGILANT,         /**< High alert - enhanced sensitivity */
    AROUSAL_LEVEL_STARTLE           /**< Startle response - maximum */
} arousal_level_t;

/**
 * @brief Protection level for loud sounds
 */
typedef enum {
    PROTECTION_NONE,                /**< No protection active */
    PROTECTION_MILD,                /**< Mild attenuation */
    PROTECTION_MODERATE,            /**< Moderate attenuation */
    PROTECTION_SEVERE,              /**< Strong attenuation */
    PROTECTION_EMERGENCY            /**< Emergency cutoff */
} protection_level_t;

/**
 * @brief Circadian phase affecting auditory sensitivity
 */
typedef enum {
    CIRCADIAN_NIGHT,                /**< Nighttime - heightened alertness */
    CIRCADIAN_DAWN,                 /**< Dawn transition */
    CIRCADIAN_MORNING,              /**< Morning - increasing sensitivity */
    CIRCADIAN_MIDDAY,               /**< Midday - peak sensitivity */
    CIRCADIAN_AFTERNOON,            /**< Afternoon - stable */
    CIRCADIAN_EVENING,              /**< Evening - decreasing */
    CIRCADIAN_DUSK                  /**< Dusk transition */
} circadian_phase_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Cochlear nucleus output
 *
 * BIOLOGICAL: Preserves timing, adds onset/offset enhancement
 */
typedef struct {
    float* bushy_cell_output;       /**< Timing preserving cells [num_channels] */
    float* stellate_cell_output;    /**< Onset detectors [num_channels] */
    float* octopus_cell_output;     /**< Broadband onset [num_channels] */
    uint32_t num_channels;
} cn_output_t;

/**
 * @brief Superior olivary complex output
 *
 * BIOLOGICAL: Binaural processing for localization
 */
typedef struct {
    float* mso_output;              /**< Medial SO - ITD processing */
    float* lso_output;              /**< Lateral SO - ILD processing */
    float itd_estimate_us;          /**< Computed ITD */
    float ild_estimate_db;          /**< Computed ILD */
    float azimuth_deg;              /**< Estimated azimuth */
    uint32_t num_channels;
} soc_output_t;

/**
 * @brief Inferior colliculus output
 *
 * BIOLOGICAL: Multimodal integration, frequency mapping
 */
typedef struct {
    float* frequency_map;           /**< Tonotopic activation [num_channels] */
    float* amplitude_map;           /**< Intensity mapping [num_channels] */
    float dominant_frequency_hz;    /**< Peak frequency */
    float overall_intensity_db;     /**< Overall level */
    uint32_t num_channels;
} ic_output_t;

/**
 * @brief Acoustic reflex state
 */
typedef struct {
    bool reflex_triggered;          /**< Reflex currently active */
    float trigger_level_db;         /**< Level that triggered reflex */
    float current_attenuation_db;   /**< Current attenuation amount */
    float reflex_onset_time_ms;     /**< Time since trigger */
    float reflex_duration_ms;       /**< How long reflex has been active */
    float decay_time_constant_ms;   /**< Decay after loud sound stops */
} acoustic_reflex_state_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Reflex parameters */
    float reflex_threshold_db;      /**< Threshold for acoustic reflex */
    float reflex_latency_ms;        /**< Reflex latency */
    float max_attenuation_db;       /**< Maximum attenuation */

    /* Arousal coupling */
    bool enable_arousal_coupling;   /**< Enable arousal modulation */
    float arousal_gain_range_db;    /**< Range of arousal modulation */

    /* Circadian coupling */
    bool enable_circadian_coupling; /**< Enable circadian modulation */
    float circadian_sensitivity[24];/**< Per-hour sensitivity curve */

    /* Processing options */
    bool enable_cn_simulation;      /**< Simulate cochlear nucleus */
    bool enable_soc_binaural;       /**< Enable SOC binaural processing */
    bool enable_ic_integration;     /**< Enable IC integration */

} cochlea_medulla_config_t;

/**
 * @brief Cochlea-Medulla bridge instance
 */
typedef struct cochlea_medulla_bridge cochlea_medulla_bridge_t;

//=============================================================================
// Configuration Helpers
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @return Default configuration
 */
cochlea_medulla_config_t cochlea_medulla_config_default(void);

//=============================================================================
// Core API
//=============================================================================

/**
 * @brief Create cochlea-medulla bridge
 *
 * WHAT: Initialize brainstem pathway integration
 * WHY:  Connect cochlea to protective and modulatory systems
 * HOW:  Create CN, SOC, IC processing chains
 *
 * @param cochlea Cochlea instance
 * @param medulla Medulla instance
 * @param config Bridge configuration
 * @return Bridge instance or NULL
 */
cochlea_medulla_bridge_t* cochlea_medulla_bridge_create(
    cochlea_t* cochlea,
    medulla_t* medulla,
    const cochlea_medulla_config_t* config
);

/**
 * @brief Destroy bridge
 *
 * @param bridge Bridge to destroy
 */
void cochlea_medulla_bridge_destroy(cochlea_medulla_bridge_t* bridge);

/**
 * @brief Update bridge processing
 *
 * WHAT: Process cochlear output through brainstem pathway
 * WHY:  Maintain real-time brainstem simulation
 * HOW:  CN → SOC → IC cascade with arousal modulation
 *
 * @param bridge Bridge instance
 * @param cochlea_output Current cochlear output
 * @param dt_ms Time step (milliseconds)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_medulla_bridge_update(
    cochlea_medulla_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_medulla_bridge_reset(cochlea_medulla_bridge_t* bridge);

//=============================================================================
// Protective Functions
//=============================================================================

/**
 * @brief Trigger protective cutoff
 *
 * WHAT: Activate acoustic reflex for loud sound protection
 * WHY:  Prevent cochlear damage from excessive sound levels
 * HOW:  Stapedius reflex simulation with appropriate latency
 *
 * BIOLOGICAL: Real reflex has 25-150 ms latency, provides 10-14 dB attenuation
 *
 * @param bridge Bridge instance
 * @param sound_level_db Triggering sound level
 * @param level Protection level
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_medulla_trigger_protective_cutoff(
    cochlea_medulla_bridge_t* bridge,
    float sound_level_db,
    protection_level_t level
);

/**
 * @brief Check if protection is active
 *
 * @param bridge Bridge instance
 * @return true if protection currently active
 */
bool cochlea_medulla_is_protection_active(const cochlea_medulla_bridge_t* bridge);

/**
 * @brief Get current protection attenuation
 *
 * @param bridge Bridge instance
 * @return Attenuation in dB (0 if no protection)
 */
float cochlea_medulla_get_attenuation(const cochlea_medulla_bridge_t* bridge);

/**
 * @brief Get acoustic reflex state
 *
 * @param bridge Bridge instance
 * @param state Output reflex state
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_medulla_get_reflex_state(
    const cochlea_medulla_bridge_t* bridge,
    acoustic_reflex_state_t* state
);

//=============================================================================
// Arousal Modulation
//=============================================================================

/**
 * @brief Set arousal level
 *
 * WHAT: Modulate cochlear sensitivity based on arousal state
 * WHY:  Biological coupling between alertness and hearing sensitivity
 * HOW:  Scale cochlear gain based on arousal level
 *
 * BIOLOGICAL: Locus coeruleus → cochlear nucleus pathway
 *
 * @param bridge Bridge instance
 * @param level Arousal level
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_medulla_set_arousal(
    cochlea_medulla_bridge_t* bridge,
    arousal_level_t level
);

/**
 * @brief Get current arousal level
 *
 * @param bridge Bridge instance
 * @return Current arousal level
 */
arousal_level_t cochlea_medulla_get_arousal(
    const cochlea_medulla_bridge_t* bridge
);

/**
 * @brief Get arousal-based gain modulation
 *
 * @param bridge Bridge instance
 * @return Gain modulation in dB
 */
float cochlea_medulla_get_arousal_gain(
    const cochlea_medulla_bridge_t* bridge
);

//=============================================================================
// Circadian Modulation
//=============================================================================

/**
 * @brief Set circadian phase
 *
 * WHAT: Modulate sensitivity based on time of day
 * WHY:  Auditory sensitivity varies with circadian rhythm
 * HOW:  Apply phase-appropriate sensitivity scaling
 *
 * @param bridge Bridge instance
 * @param phase Circadian phase
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_medulla_set_circadian_phase(
    cochlea_medulla_bridge_t* bridge,
    circadian_phase_t phase
);

/**
 * @brief Set circadian phase by hour
 *
 * @param bridge Bridge instance
 * @param hour Hour of day (0-23)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_medulla_set_circadian_hour(
    cochlea_medulla_bridge_t* bridge,
    uint32_t hour
);

/**
 * @brief Get circadian sensitivity modifier
 *
 * @param bridge Bridge instance
 * @return Sensitivity multiplier (0.5-1.5 typical)
 */
float cochlea_medulla_get_circadian_sensitivity(
    const cochlea_medulla_bridge_t* bridge
);

//=============================================================================
// Brainstem Nucleus Access
//=============================================================================

/**
 * @brief Get cochlear nucleus output
 *
 * @param bridge Bridge instance
 * @param output Output structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_medulla_get_cn_output(
    const cochlea_medulla_bridge_t* bridge,
    cn_output_t* output
);

/**
 * @brief Get superior olive output
 *
 * @param bridge Bridge instance
 * @param output Output structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_medulla_get_soc_output(
    const cochlea_medulla_bridge_t* bridge,
    soc_output_t* output
);

/**
 * @brief Get inferior colliculus output
 *
 * @param bridge Bridge instance
 * @param output Output structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_medulla_get_ic_output(
    const cochlea_medulla_bridge_t* bridge,
    ic_output_t* output
);

//=============================================================================
// Bidirectional Verification
//=============================================================================

/**
 * @brief Verify bidirectional data flow
 *
 * @param bridge Bridge instance
 * @return true if both directions have recent activity
 */
bool cochlea_medulla_verify_bidirectional(
    const cochlea_medulla_bridge_t* bridge
);

/**
 * @brief Get last outbound timestamp (cochlea → medulla)
 *
 * @param bridge Bridge instance
 * @return Timestamp in milliseconds
 */
uint64_t cochlea_medulla_get_last_outbound(
    const cochlea_medulla_bridge_t* bridge
);

/**
 * @brief Get last inbound timestamp (medulla → cochlea)
 *
 * @param bridge Bridge instance
 * @return Timestamp in milliseconds
 */
uint64_t cochlea_medulla_get_last_inbound(
    const cochlea_medulla_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COCHLEA_MEDULLA_BRIDGE_H */
