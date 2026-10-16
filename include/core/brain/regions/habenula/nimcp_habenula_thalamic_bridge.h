/**
 * @file nimcp_habenula_thalamic_bridge.h
 * @brief Habenula - Thalamic Relay Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bidirectional bridge between Habenula and thalamic systems
 * WHY:  Enable habenula-mediated thalamic modulation for aversive processing
 * HOW:  Habenula affects thalamic processing of aversive stimuli
 *
 * THEORETICAL FOUNDATIONS:
 * - Hikosaka (2010): Habenula and thalamic connections
 * - Herkenham & Nauta (1979): Habenula thalamic projections
 * - Shelton et al. (2012): Habenula in thalamic regulation
 *
 * BIOLOGICAL BASIS:
 * - Habenula receives input from thalamus
 * - Projects to thalamic nuclei via brainstem
 * - Modulates thalamic responses to aversive cues
 * - Part of epithalamus with thalamic connections
 *
 * INTEGRATION FLOWS:
 *
 * Habenula --> Thalamus:
 *   1. Aversive signals modulate thalamic relay
 *   2. Disappointment affects sensory processing
 *   3. Inhibition of positive signals
 *   4. Enhanced aversive cue detection
 *
 * Thalamus --> Habenula:
 *   1. Sensory cues predict aversive outcomes
 *   2. Thalamic state indicates threat level
 *   3. Processing of aversive stimuli
 *   4. Contextual information for habenula
 *
 * @see nimcp_habenula.h
 * @see nimcp_thalamic_relay.h
 */

#ifndef NIMCP_HABENULA_THALAMIC_BRIDGE_H
#define NIMCP_HABENULA_THALAMIC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct nimcp_habenula_adapter_struct;
typedef struct nimcp_habenula_adapter_struct* nimcp_habenula_adapter_t;
struct nimcp_thalamic_relay;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Default aversive relay gain */
#define HAB_THAL_AVERSIVE_GAIN          1.2f

/** @brief Bio-async module ID */
#define BIO_MODULE_HAB_THALAMIC         0x0F20

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Aversive processing mode
 */
typedef enum {
    HAB_THAL_MODE_NEUTRAL = 0,       /**< Neutral processing */
    HAB_THAL_MODE_VIGILANT,          /**< Aversive vigilance */
    HAB_THAL_MODE_AVOIDANT,          /**< Avoidance mode */
    HAB_THAL_MODE_HABITUATED         /**< Habituated to aversive */
} nimcp_hab_thal_mode_t;

/**
 * @brief Threat detection level
 */
typedef enum {
    HAB_THAL_THREAT_NONE = 0,        /**< No threat detected */
    HAB_THAL_THREAT_LOW,             /**< Low threat */
    HAB_THAL_THREAT_MODERATE,        /**< Moderate threat */
    HAB_THAL_THREAT_HIGH             /**< High threat */
} nimcp_hab_thal_threat_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief Habenula-Thalamic bridge configuration
 */
typedef struct {
    /* Aversive processing */
    nimcp_hab_thal_mode_t default_mode;
    float aversive_gain;             /**< Aversive stimulus gain */
    float threat_threshold;          /**< Threshold for threat response */

    /* Relay modulation */
    float positive_suppression;      /**< Suppression of positive signals */
    float aversive_enhancement;      /**< Enhancement of aversive signals */

    /* Habituation */
    bool enable_habituation;         /**< Enable threat habituation */
    float habituation_rate;          /**< Habituation rate */

    /* Feedback */
    float predictive_cue_gain;       /**< Predictive cue effect */

    /* Update */
    float update_interval_ms;
    bool enable_bio_async;
} nimcp_hab_thal_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Thalamic modulation output
 */
typedef struct {
    nimcp_hab_thal_mode_t mode;      /**< Current processing mode */
    float aversive_relay_gain;       /**< Aversive channel gain */
    float positive_suppression;      /**< Positive channel suppression */
    nimcp_hab_thal_threat_t threat;  /**< Detected threat level */
} nimcp_hab_thal_modulation_t;

/**
 * @brief Thalamic feedback to Habenula
 */
typedef struct {
    float aversive_cue_strength;     /**< Strength of aversive cues */
    float threat_signal;             /**< Thalamic threat signal */
    float predictive_cue;            /**< Aversive-predictive cue */
    float sensory_load;              /**< Processing load */
} nimcp_hab_thal_feedback_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_hab_thal_mode_t current_mode;
    nimcp_hab_thal_threat_t threat_level;
    float current_aversive_gain;
    float habituation_level;
    float accumulated_threat;
} nimcp_hab_thal_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t threat_detections;
    uint64_t mode_changes;
    float avg_threat_level;
    float avg_aversive_gain;
    float time_in_vigilant;
} nimcp_hab_thal_stats_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_hab_thal_bridge nimcp_hab_thal_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

nimcp_hab_thal_config_t nimcp_hab_thal_config_default(void);

nimcp_hab_thal_bridge_t* nimcp_hab_thal_create(
    const nimcp_hab_thal_config_t* config
);

void nimcp_hab_thal_destroy(nimcp_hab_thal_bridge_t* bridge);

int nimcp_hab_thal_reset(nimcp_hab_thal_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_hab_thal_connect_habenula(
    nimcp_hab_thal_bridge_t* bridge,
    nimcp_habenula_adapter_t hab_adapter
);

int nimcp_hab_thal_connect_thalamus(
    nimcp_hab_thal_bridge_t* bridge,
    struct nimcp_thalamic_relay* thalamus
);

/*=============================================================================
 * Habenula --> Thalamus API
 *===========================================================================*/

/**
 * @brief Compute thalamic modulation from habenula state
 */
int nimcp_hab_thal_compute_modulation(
    nimcp_hab_thal_bridge_t* bridge,
    nimcp_hab_thal_modulation_t* modulation
);

/**
 * @brief Set processing mode
 */
int nimcp_hab_thal_set_mode(
    nimcp_hab_thal_bridge_t* bridge,
    nimcp_hab_thal_mode_t mode
);

/**
 * @brief Suppress positive signals
 */
int nimcp_hab_thal_suppress_positive(
    nimcp_hab_thal_bridge_t* bridge,
    float suppression_level
);

/*=============================================================================
 * Thalamus --> Habenula API
 *===========================================================================*/

/**
 * @brief Receive thalamic feedback
 */
int nimcp_hab_thal_receive_feedback(
    nimcp_hab_thal_bridge_t* bridge,
    const nimcp_hab_thal_feedback_t* feedback
);

/**
 * @brief Process threat signal
 */
int nimcp_hab_thal_process_threat(
    nimcp_hab_thal_bridge_t* bridge,
    float threat_level
);

/**
 * @brief Get habenula response to thalamic state
 */
float nimcp_hab_thal_get_hab_response(nimcp_hab_thal_bridge_t* bridge);

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_hab_thal_update(nimcp_hab_thal_bridge_t* bridge, float dt_ms);

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_hab_thal_get_state(
    const nimcp_hab_thal_bridge_t* bridge,
    nimcp_hab_thal_bridge_state_t* state
);

int nimcp_hab_thal_get_stats(
    const nimcp_hab_thal_bridge_t* bridge,
    nimcp_hab_thal_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HABENULA_THALAMIC_BRIDGE_H */
