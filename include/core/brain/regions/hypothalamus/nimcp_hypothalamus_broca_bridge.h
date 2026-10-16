/**
 * @file nimcp_hypothalamus_broca_bridge.h
 * @brief Hypothalamus-Broca Bridge for Speech Modulation
 *
 * WHAT: Integration between hypothalamus drives/stress and Broca's speech production
 * WHY:  Stress and arousal significantly affect speech - stuttering, rate, volume
 * HOW:  HPA cortisol → fluency; arousal → rate/volume; social drive → initiation
 *
 * BIOLOGICAL BASIS:
 * The hypothalamus affects speech production through multiple pathways:
 *
 * HPA AXIS → SPEECH (Stress Effects):
 * - Acute cortisol → Enhanced speech fluency (eustress)
 * - Chronic cortisol → Impaired fluency, word-finding difficulty
 * - High stress → Stuttering, speech blocks, decreased prosody
 *
 * AROUSAL → SPEECH (Activation Effects):
 * - High arousal → Faster speech rate, increased volume
 * - Low arousal → Slower speech, reduced volume (monotone)
 * - Optimal arousal → Normal speech production
 *
 * SOCIAL DRIVE → SPEECH (Motivation Effects):
 * - High social drive → Increased speech initiation
 * - Low social drive → Reduced verbal output
 * - Social anxiety → Avoidance of speech situations
 *
 * SURVIVAL → SPEECH (Emergency Effects):
 * - Threat → Vocalization for alarm/help
 * - Extreme stress → Speech suppression or screaming
 *
 * @version Phase 17: Perception & Speech Modulation
 * @date 2026-01-04
 */

#ifndef NIMCP_HYPOTHALAMUS_BROCA_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_BROCA_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "nimcp_hypothalamus_drives.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define HYPO_BROCA_BRIDGE_MODULE_ID  0x1185

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Speech state based on hypothalamic modulation
 */
typedef enum {
    HYPO_SPEECH_NORMAL = 0,       /**< Normal speech production */
    HYPO_SPEECH_ENHANCED,         /**< Enhanced fluency (mild arousal) */
    HYPO_SPEECH_STRESSED,         /**< Stress-affected (mild impairment) */
    HYPO_SPEECH_IMPAIRED,         /**< Significant impairment */
    HYPO_SPEECH_BLOCKED,          /**< Speech blocking (high stress) */
    HYPO_SPEECH_EMERGENCY         /**< Emergency vocalization mode */
} hypo_speech_state_t;

/**
 * @brief Speech initiation mode
 */
typedef enum {
    HYPO_INIT_NORMAL = 0,         /**< Normal initiation threshold */
    HYPO_INIT_EAGER,              /**< High social drive = eager to speak */
    HYPO_INIT_RELUCTANT,          /**< Low drive = reluctant to speak */
    HYPO_INIT_AVOIDANT,           /**< Social anxiety = avoids speaking */
    HYPO_INIT_COMPULSIVE          /**< Alarm = compulsive vocalization */
} hypo_speech_initiation_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Speech modulation parameters for Broca's area
 */
typedef struct {
    /* State */
    hypo_speech_state_t state;
    hypo_speech_initiation_t initiation_mode;

    /* Rate and volume modulation */
    float rate_multiplier;        /**< Speech rate [0.5, 2.0], 1.0 = normal */
    float volume_multiplier;      /**< Volume [0.5, 2.0], 1.0 = normal */

    /* Fluency parameters */
    float fluency_level;          /**< Overall fluency [0, 1] */
    float hesitation_probability; /**< Probability of hesitation [0, 1] */
    float word_finding_delay;     /**< Word retrieval delay factor [0, 1] */

    /* Prosody */
    float prosody_variation;      /**< Emotional prosody range [0, 1] */
    float pitch_baseline;         /**< Pitch shift from baseline [-1, 1] */

    /* Initiation */
    float initiation_threshold;   /**< Threshold to start speaking [0, 1] */
    float urgency_to_speak;       /**< Urgency/motivation to speak [0, 1] */

    /* Alarm vocalization */
    bool alarm_mode;              /**< Emergency vocalization active */
    float alarm_intensity;        /**< Alarm vocalization intensity [0, 1] */

    uint64_t timestamp_us;
} hypo_speech_modulation_t;

/**
 * @brief Stress state input (from emotion/HPA bridge)
 */
typedef struct {
    float cortisol_level;         /**< Current cortisol [0, 1] */
    float cortisol_chronic;       /**< Chronic cortisol accumulation [0, 1] */
    float acute_stress;           /**< Acute stress level [0, 1] */
    bool chronic_stress_active;   /**< Chronic stress flag */
} hypo_stress_input_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Stress → fluency mapping */
    float optimal_cortisol;       /**< Cortisol for peak performance (default 0.3) */
    float stress_fluency_weight;  /**< How much stress affects fluency (default 0.6) */
    float chronic_impairment;     /**< Chronic stress impairment factor (default 0.4) */

    /* Arousal → rate/volume mapping */
    float arousal_rate_weight;    /**< Arousal effect on speech rate (default 0.5) */
    float arousal_volume_weight;  /**< Arousal effect on volume (default 0.3) */

    /* Social drive → initiation */
    float social_initiation_weight; /**< Social drive effect on initiation (default 0.7) */

    /* Safety → alarm */
    float alarm_threshold;        /**< Safety urgency for alarm mode (default 0.8) */
    bool enable_alarm_vocalization; /**< Allow emergency vocalizations */
} hypo_broca_config_t;

typedef struct hypo_broca_bridge hypo_broca_bridge_t;

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Create Broca bridge
 *
 * @param drives Drive system handle
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
hypo_broca_bridge_t* hypo_broca_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_broca_config_t* config);

/**
 * @brief Destroy Broca bridge
 */
void hypo_broca_bridge_destroy(hypo_broca_bridge_t* bridge);

/**
 * @brief Get default configuration
 */
void hypo_broca_bridge_default_config(hypo_broca_config_t* config);

/*=============================================================================
 * HYPOTHALAMUS → SPEECH MODULATION
 *===========================================================================*/

/**
 * @brief Update stress state from HPA/emotion system
 *
 * @param bridge Bridge handle
 * @param stress Stress state input
 * @return 0 on success, -1 on error
 */
int hypo_broca_bridge_update_stress(
    hypo_broca_bridge_t* bridge,
    const hypo_stress_input_t* stress);

/**
 * @brief Set arousal level (from brainstem)
 *
 * @param bridge Bridge handle
 * @param arousal Arousal level [0, 1]
 * @return 0 on success, -1 on error
 */
int hypo_broca_bridge_set_arousal(
    hypo_broca_bridge_t* bridge,
    float arousal);

/**
 * @brief Compute speech modulation from current state
 *
 * Integrates stress, arousal, and drive states to compute
 * speech production parameters.
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_broca_bridge_compute_modulation(hypo_broca_bridge_t* bridge);

/**
 * @brief Get current speech modulation
 *
 * @param bridge Bridge handle
 * @param modulation Output modulation parameters
 * @return 0 on success, -1 on error
 */
int hypo_broca_bridge_get_modulation(
    const hypo_broca_bridge_t* bridge,
    hypo_speech_modulation_t* modulation);

/*=============================================================================
 * SPEECH STATE QUERIES
 *===========================================================================*/

/**
 * @brief Get current speech state
 *
 * @param bridge Bridge handle
 * @return Current speech state
 */
hypo_speech_state_t hypo_broca_bridge_get_state(
    const hypo_broca_bridge_t* bridge);

/**
 * @brief Check if speech is impaired
 *
 * @param bridge Bridge handle
 * @return true if speech production is impaired
 */
bool hypo_broca_bridge_is_impaired(const hypo_broca_bridge_t* bridge);

/**
 * @brief Check if alarm vocalization is active
 *
 * @param bridge Bridge handle
 * @return true if alarm mode active
 */
bool hypo_broca_bridge_is_alarm_active(const hypo_broca_bridge_t* bridge);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/**
 * @brief Register with bio-async router
 *
 * @param bridge Bridge handle
 * @param use_kg_wiring Use knowledge graph wiring
 * @return true on success
 */
bool hypo_broca_bridge_register_bio(
    hypo_broca_bridge_t* bridge,
    bool use_kg_wiring);

/**
 * @brief Unregister from bio-async router
 */
void hypo_broca_bridge_unregister_bio(hypo_broca_bridge_t* bridge);

/**
 * @brief Broadcast speech modulation
 *
 * @param bridge Bridge handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t hypo_broca_bridge_broadcast_modulation(
    hypo_broca_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_BROCA_BRIDGE_H */
