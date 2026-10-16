/**
 * @file nimcp_hypothalamus_perception_bridge.h
 * @brief Hypothalamus-Perception Bridge for Sensory Modulation
 *
 * WHAT: Bidirectional integration between hypothalamus drives and sensory cortices
 * WHY:  Arousal and drives modulate sensory processing - hungry animals see food more
 * HOW:  Arousal → sensory gain; drive urgency → stimulus salience boost
 *
 * BIOLOGICAL BASIS:
 * The hypothalamus modulates sensory processing through multiple pathways:
 *
 * HYPOTHALAMUS → SENSORY CORTICES (via thalamus and neuromodulators):
 * - Arousal level → Global sensory gain (norepinephrine from LC)
 * - Drive urgency → Category-specific salience (dopamine from VTA)
 * - Threat detection → Fear-relevant stimuli bypass attention filter
 *
 * DRIVE-BIASED PERCEPTION:
 * - Hunger → Food-related stimuli more salient
 * - Thirst → Water/liquid stimuli more salient
 * - Safety threat → Threat-related stimuli prioritized
 * - Social drive → Face/voice stimuli enhanced
 * - Curiosity → Novel stimuli boosted
 *
 * SENSORY CORTICES → HYPOTHALAMUS:
 * - Detection of drive-relevant stimuli → Drive satisfaction anticipation
 * - Threat detection → Safety drive activation
 * - Food/water detection → Hunger/thirst anticipation
 *
 * ENHANCED FEATURES (Phase 17.5):
 * 1. INTEROCEPTION: Internal body state sensing (hunger pangs, heart rate, etc.)
 * 2. OLFACTORY/GUSTATORY: Smell/taste integration for food/water detection
 * 3. PREDICTIVE CODING: Top-down predictions to perception for active inference
 * 4. PAIN MODULATION: Stress-induced analgesia and hyperalgesia
 * 5. SLEEP GATING: Sleep state modulates perception sensitivity
 * 6. THERMAL SALIENCE: Temperature drive enhances thermal stimulus processing
 *
 * @version Phase 17.5: Enhanced Perception Integration
 * @date 2026-01-10
 */

#ifndef NIMCP_HYPOTHALAMUS_PERCEPTION_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_PERCEPTION_BRIDGE_H

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

#define HYPO_PERCEPTION_BRIDGE_MODULE_ID  0x1184
#define HYPO_PERCEPTION_MAX_CATEGORIES    16

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Sensory modality types
 */
typedef enum {
    HYPO_SENSE_VISUAL = 0,        /**< Visual cortex (V1-V4, IT) */
    HYPO_SENSE_AUDITORY,          /**< Auditory cortex (A1, belt, parabelt) */
    HYPO_SENSE_SOMATOSENSORY,     /**< Somatosensory cortex (S1, S2) */
    HYPO_SENSE_OLFACTORY,         /**< Olfactory cortex (piriform) */
    HYPO_SENSE_GUSTATORY,         /**< Gustatory cortex (insula) */
    HYPO_SENSE_COUNT
} hypo_sense_modality_t;

/**
 * @brief Stimulus category types for drive-biased salience
 */
typedef enum {
    HYPO_STIM_FOOD = 0,           /**< Food-related stimuli */
    HYPO_STIM_WATER,              /**< Water/liquid stimuli */
    HYPO_STIM_THREAT,             /**< Threat-related stimuli */
    HYPO_STIM_SOCIAL,             /**< Social stimuli (faces, voices) */
    HYPO_STIM_NOVEL,              /**< Novel/unexpected stimuli */
    HYPO_STIM_THERMAL,            /**< Temperature-related stimuli */
    HYPO_STIM_PAIN,               /**< Nociceptive stimuli */
    HYPO_STIM_REWARD,             /**< Reward-associated stimuli */
    HYPO_STIM_NEUTRAL,            /**< Neutral/background stimuli */
    HYPO_STIM_COUNT
} hypo_stim_category_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Sensory modulation output to cortices
 */
typedef struct {
    /* Global arousal modulation */
    float global_gain;            /**< Overall sensory gain [0.5, 2.0] */
    float arousal_level;          /**< Current arousal [0, 1] */

    /* Per-modality gain */
    float modality_gains[HYPO_SENSE_COUNT];

    /* Category-specific salience boosts */
    float category_salience[HYPO_STIM_COUNT];

    /* Attention override flags */
    bool threat_priority;         /**< Threat stimuli bypass attention gate */
    bool survival_mode;           /**< Critical drive = max sensory gain */

    /* Timestamp */
    uint64_t timestamp_us;
} hypo_perception_modulation_t;

/**
 * @brief Sensory detection feedback from cortices
 */
typedef struct {
    hypo_stim_category_t detected_category;  /**< What category was detected */
    hypo_sense_modality_t modality;          /**< Which modality detected it */
    float confidence;                        /**< Detection confidence [0, 1] */
    float intensity;                         /**< Stimulus intensity [0, 1] */
    bool is_threat;                          /**< Threat flag for fast path */
    uint64_t timestamp_us;
} hypo_sensory_detection_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Arousal → gain mapping */
    float arousal_gain_min;       /**< Gain at zero arousal (default 0.7) */
    float arousal_gain_max;       /**< Gain at max arousal (default 1.5) */

    /* Drive → salience mapping */
    float drive_salience_weight;  /**< How much drives boost salience (default 0.8) */
    float threat_priority_threshold; /**< Safety urgency for threat priority (default 0.5) */

    /* Modality-specific weights */
    float visual_weight;          /**< Visual modality sensitivity */
    float auditory_weight;        /**< Auditory modality sensitivity */

    /* Anticipation */
    bool enable_anticipation;     /**< Enable drive anticipation from detection */
    float anticipation_decay;     /**< How fast anticipation decays */
} hypo_perception_config_t;

typedef struct hypo_perception_bridge hypo_perception_bridge_t;

/*=============================================================================
 * LIFECYCLE
 *==============================================================================*/

/**
 * @brief Create perception bridge
 *
 * @param drives Drive system handle
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
hypo_perception_bridge_t* hypo_perception_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_perception_config_t* config);

/**
 * @brief Destroy perception bridge
 */
void hypo_perception_bridge_destroy(hypo_perception_bridge_t* bridge);

/**
 * @brief Get default configuration
 */
void hypo_perception_bridge_default_config(hypo_perception_config_t* config);

/*=============================================================================
 * DRIVE → PERCEPTION MODULATION
 *===========================================================================*/

/**
 * @brief Compute perception modulation from current drive state
 *
 * Maps arousal and drive urgencies to sensory modulation parameters:
 * - Global gain from arousal level
 * - Category salience from corresponding drive urgencies
 * - Threat priority from safety drive
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_compute_modulation(hypo_perception_bridge_t* bridge);

/**
 * @brief Get current perception modulation
 *
 * @param bridge Bridge handle
 * @param modulation Output modulation parameters
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_get_modulation(
    const hypo_perception_bridge_t* bridge,
    hypo_perception_modulation_t* modulation);

/**
 * @brief Set arousal level (from brainstem/LC)
 *
 * @param bridge Bridge handle
 * @param arousal Arousal level [0, 1]
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_set_arousal(
    hypo_perception_bridge_t* bridge,
    float arousal);

/*=============================================================================
 * PERCEPTION → DRIVE FEEDBACK
 *===========================================================================*/

/**
 * @brief Process sensory detection
 *
 * When sensory cortices detect drive-relevant stimuli, this informs
 * the drive system for anticipation effects.
 *
 * @param bridge Bridge handle
 * @param detection Detection event
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_process_detection(
    hypo_perception_bridge_t* bridge,
    const hypo_sensory_detection_t* detection);

/**
 * @brief Get anticipation level for a drive
 *
 * Returns how much the drive is being anticipated based on
 * recent sensory detections (seeing food increases hunger anticipation).
 *
 * @param bridge Bridge handle
 * @param drive_type Which drive
 * @return Anticipation level [0, 1]
 */
float hypo_perception_bridge_get_anticipation(
    const hypo_perception_bridge_t* bridge,
    hypo_drive_type_t drive_type);

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
bool hypo_perception_bridge_register_bio(
    hypo_perception_bridge_t* bridge,
    bool use_kg_wiring);

/**
 * @brief Unregister from bio-async router
 */
void hypo_perception_bridge_unregister_bio(hypo_perception_bridge_t* bridge);

/**
 * @brief Broadcast perception modulation
 *
 * @param bridge Bridge handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t hypo_perception_bridge_broadcast_modulation(
    hypo_perception_bridge_t* bridge);

/*=============================================================================
 * ENHANCEMENT 1: INTEROCEPTION (Internal Body State Sensing)
 *===========================================================================*/

/**
 * @brief Interoceptive signal types
 */
typedef enum {
    HYPO_INTERO_HUNGER_PANGS = 0,  /**< Stomach contractions */
    HYPO_INTERO_HEART_RATE,        /**< Cardiac awareness */
    HYPO_INTERO_BREATHING,         /**< Respiratory awareness */
    HYPO_INTERO_TEMPERATURE,       /**< Core temperature sensing */
    HYPO_INTERO_FATIGUE,           /**< Physical tiredness */
    HYPO_INTERO_PAIN,              /**< Nociceptive signals */
    HYPO_INTERO_THIRST,            /**< Osmoreceptor signals */
    HYPO_INTERO_BLADDER,           /**< Bladder fullness */
    HYPO_INTERO_COUNT
} hypo_interoceptive_type_t;

/**
 * @brief Interoceptive state for body awareness
 */
typedef struct {
    float signals[HYPO_INTERO_COUNT];  /**< Signal intensity [0, 1] */
    float prediction_errors[HYPO_INTERO_COUNT];  /**< Deviation from expected */
    float salience[HYPO_INTERO_COUNT];  /**< How attention-grabbing */
    float global_interoceptive_accuracy;  /**< Overall body awareness [0, 1] */
    uint64_t timestamp_us;
} hypo_interoceptive_state_t;

/**
 * @brief Process interoceptive signal
 *
 * @param bridge Bridge handle
 * @param signal_type Type of interoceptive signal
 * @param intensity Signal intensity [0, 1]
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_process_interoceptive(
    hypo_perception_bridge_t* bridge,
    hypo_interoceptive_type_t signal_type,
    float intensity);

/**
 * @brief Get interoceptive state
 *
 * @param bridge Bridge handle
 * @param state Output state
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_get_interoceptive_state(
    const hypo_perception_bridge_t* bridge,
    hypo_interoceptive_state_t* state);

/**
 * @brief Set interoceptive accuracy (body awareness level)
 *
 * @param bridge Bridge handle
 * @param accuracy Accuracy level [0, 1]
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_set_interoceptive_accuracy(
    hypo_perception_bridge_t* bridge,
    float accuracy);

/*=============================================================================
 * ENHANCEMENT 2: OLFACTORY/GUSTATORY INTEGRATION
 *===========================================================================*/

/**
 * @brief Olfactory stimulus types
 */
typedef enum {
    HYPO_OLFACT_FOOD_PLEASANT = 0,  /**< Appetizing food smell */
    HYPO_OLFACT_FOOD_UNPLEASANT,    /**< Spoiled/toxic smell */
    HYPO_OLFACT_PHEROMONE_SOCIAL,   /**< Social pheromones */
    HYPO_OLFACT_DANGER,             /**< Smoke, predator scent */
    HYPO_OLFACT_NEUTRAL,            /**< Background odors */
    HYPO_OLFACT_COUNT
} hypo_olfactory_type_t;

/**
 * @brief Gustatory stimulus types
 */
typedef enum {
    HYPO_GUST_SWEET = 0,    /**< Sugar/carbohydrate */
    HYPO_GUST_SALTY,        /**< Sodium detection */
    HYPO_GUST_SOUR,         /**< Acid detection */
    HYPO_GUST_BITTER,       /**< Toxin warning */
    HYPO_GUST_UMAMI,        /**< Protein/amino acids */
    HYPO_GUST_FAT,          /**< Lipid detection */
    HYPO_GUST_COUNT
} hypo_gustatory_type_t;

/**
 * @brief Chemosensory state (smell + taste)
 */
typedef struct {
    float olfactory_intensity[HYPO_OLFACT_COUNT];
    float gustatory_intensity[HYPO_GUST_COUNT];
    float hunger_modulation;   /**< How much hunger boosts food smell */
    float thirst_modulation;   /**< How much thirst boosts salt taste */
    bool food_detected;
    bool toxin_detected;
    uint64_t timestamp_us;
} hypo_chemosensory_state_t;

/**
 * @brief Process olfactory stimulus
 *
 * @param bridge Bridge handle
 * @param odor_type Type of odor
 * @param intensity Intensity [0, 1]
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_process_olfactory(
    hypo_perception_bridge_t* bridge,
    hypo_olfactory_type_t odor_type,
    float intensity);

/**
 * @brief Process gustatory stimulus
 *
 * @param bridge Bridge handle
 * @param taste_type Type of taste
 * @param intensity Intensity [0, 1]
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_process_gustatory(
    hypo_perception_bridge_t* bridge,
    hypo_gustatory_type_t taste_type,
    float intensity);

/**
 * @brief Get chemosensory state
 *
 * @param bridge Bridge handle
 * @param state Output state
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_get_chemosensory_state(
    const hypo_perception_bridge_t* bridge,
    hypo_chemosensory_state_t* state);

/*=============================================================================
 * ENHANCEMENT 3: PREDICTIVE CODING (Top-Down Predictions)
 *===========================================================================*/

/**
 * @brief Prediction types for active inference
 */
typedef enum {
    HYPO_PRED_FOOD_PRESENCE = 0,   /**< Expect food in environment */
    HYPO_PRED_THREAT_PRESENCE,     /**< Expect danger */
    HYPO_PRED_SOCIAL_INTERACTION,  /**< Expect social contact */
    HYPO_PRED_TEMPERATURE_CHANGE,  /**< Expect temp change */
    HYPO_PRED_REWARD_AVAILABLE,    /**< Expect reward */
    HYPO_PRED_COUNT
} hypo_prediction_type_t;

/**
 * @brief Predictive state for perception
 */
typedef struct {
    float predictions[HYPO_PRED_COUNT];     /**< Expected probability [0, 1] */
    float prediction_errors[HYPO_PRED_COUNT]; /**< Actual - Expected */
    float precision[HYPO_PRED_COUNT];       /**< Confidence in prediction */
    float free_energy;                      /**< Overall prediction error */
    bool active_inference_enabled;          /**< Act to fulfill predictions */
    uint64_t timestamp_us;
} hypo_predictive_state_t;

/**
 * @brief Generate prediction for perception
 *
 * @param bridge Bridge handle
 * @param pred_type Prediction type
 * @param probability Expected probability [0, 1]
 * @param precision Confidence [0, 1]
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_generate_prediction(
    hypo_perception_bridge_t* bridge,
    hypo_prediction_type_t pred_type,
    float probability,
    float precision);

/**
 * @brief Update prediction error from perception
 *
 * @param bridge Bridge handle
 * @param pred_type Prediction type
 * @param actual_value What was actually observed [0, 1]
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_update_prediction_error(
    hypo_perception_bridge_t* bridge,
    hypo_prediction_type_t pred_type,
    float actual_value);

/**
 * @brief Get predictive state
 *
 * @param bridge Bridge handle
 * @param state Output state
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_get_predictive_state(
    const hypo_perception_bridge_t* bridge,
    hypo_predictive_state_t* state);

/**
 * @brief Compute free energy from prediction errors
 *
 * @param bridge Bridge handle
 * @param free_energy Output free energy value
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_compute_free_energy(
    hypo_perception_bridge_t* bridge,
    float* free_energy);

/*=============================================================================
 * ENHANCEMENT 4: PAIN MODULATION (Stress-Induced Changes)
 *===========================================================================*/

/**
 * @brief Pain modulation state
 */
typedef struct {
    float pain_sensitivity;     /**< Current pain sensitivity [0.5, 2.0] */
    float stress_level;         /**< Current stress [0, 1] */
    float analgesia_level;      /**< Stress-induced analgesia [0, 1] */
    float hyperalgesia_level;   /**< Stress-induced hyperalgesia [0, 1] */
    bool in_acute_stress;       /**< Acute stress = analgesia */
    bool in_chronic_stress;     /**< Chronic stress = hyperalgesia */
    float endorphin_level;      /**< Endogenous opioid level [0, 1] */
    uint64_t timestamp_us;
} hypo_pain_modulation_state_t;

/**
 * @brief Set stress level for pain modulation
 *
 * Acute stress → stress-induced analgesia (reduced pain)
 * Chronic stress → hyperalgesia (increased pain sensitivity)
 *
 * @param bridge Bridge handle
 * @param stress_level Stress level [0, 1]
 * @param is_chronic True if chronic stress
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_set_stress_for_pain(
    hypo_perception_bridge_t* bridge,
    float stress_level,
    bool is_chronic);

/**
 * @brief Process pain stimulus
 *
 * @param bridge Bridge handle
 * @param raw_pain Raw pain intensity [0, 1]
 * @param modulated_pain Output: Pain after modulation
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_modulate_pain(
    hypo_perception_bridge_t* bridge,
    float raw_pain,
    float* modulated_pain);

/**
 * @brief Get pain modulation state
 *
 * @param bridge Bridge handle
 * @param state Output state
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_get_pain_state(
    const hypo_perception_bridge_t* bridge,
    hypo_pain_modulation_state_t* state);

/**
 * @brief Trigger endorphin release (e.g., from exercise)
 *
 * @param bridge Bridge handle
 * @param amount Release amount [0, 1]
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_release_endorphins(
    hypo_perception_bridge_t* bridge,
    float amount);

/*=============================================================================
 * ENHANCEMENT 5: SLEEP-DEPENDENT PERCEPTION GATING
 *===========================================================================*/

/**
 * @brief Sleep stages
 */
typedef enum {
    HYPO_SLEEP_AWAKE = 0,      /**< Fully awake */
    HYPO_SLEEP_DROWSY,         /**< Drowsy, reduced vigilance */
    HYPO_SLEEP_LIGHT,          /**< Light sleep (N1/N2) */
    HYPO_SLEEP_DEEP,           /**< Deep sleep (N3/SWS) */
    HYPO_SLEEP_REM,            /**< REM sleep (dreaming) */
    HYPO_SLEEP_COUNT
} hypo_sleep_stage_t;

/**
 * @brief Sleep gating state
 */
typedef struct {
    hypo_sleep_stage_t current_stage;
    float perception_gate;     /**< How open is perception [0, 1] */
    float arousal_threshold;   /**< Stimulus strength to wake [0, 1] */
    float threat_bypass;       /**< Threat stimuli bypass gating [0, 1] */
    float name_bypass;         /**< Own name can bypass gating [0, 1] */
    bool gating_enabled;
    uint64_t sleep_onset_us;   /**< When sleep started */
    uint64_t timestamp_us;
} hypo_sleep_gating_state_t;

/**
 * @brief Set sleep stage for perception gating
 *
 * @param bridge Bridge handle
 * @param stage Sleep stage
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_set_sleep_stage(
    hypo_perception_bridge_t* bridge,
    hypo_sleep_stage_t stage);

/**
 * @brief Check if stimulus can pass sleep gate
 *
 * @param bridge Bridge handle
 * @param stimulus_intensity Intensity [0, 1]
 * @param is_threat Is this a threat stimulus
 * @param is_name Is this the agent's name
 * @param passes Output: true if passes gate
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_check_sleep_gate(
    hypo_perception_bridge_t* bridge,
    float stimulus_intensity,
    bool is_threat,
    bool is_name,
    bool* passes);

/**
 * @brief Get sleep gating state
 *
 * @param bridge Bridge handle
 * @param state Output state
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_get_sleep_gating_state(
    const hypo_perception_bridge_t* bridge,
    hypo_sleep_gating_state_t* state);

/*=============================================================================
 * ENHANCEMENT 6: THERMAL SALIENCE
 *===========================================================================*/

/**
 * @brief Thermal perception state
 */
typedef struct {
    float core_temperature;       /**< Core temp in relative units [0, 1] */
    float ambient_temperature;    /**< Ambient temp [0, 1] */
    float thermal_discomfort;     /**< Deviation from setpoint [0, 1] */
    float thermal_salience_boost; /**< Boost to thermal stimuli [1.0, 3.0] */
    float warm_seeking;           /**< Motivation to seek warmth [0, 1] */
    float cool_seeking;           /**< Motivation to seek cooling [0, 1] */
    bool hypothermic_alert;       /**< Too cold warning */
    bool hyperthermic_alert;      /**< Too hot warning */
    uint64_t timestamp_us;
} hypo_thermal_state_t;

/**
 * @brief Set core temperature
 *
 * @param bridge Bridge handle
 * @param temperature Core temperature [0, 1] (0.5 = setpoint)
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_set_core_temperature(
    hypo_perception_bridge_t* bridge,
    float temperature);

/**
 * @brief Set ambient temperature
 *
 * @param bridge Bridge handle
 * @param temperature Ambient temperature [0, 1] (0.5 = comfortable)
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_set_ambient_temperature(
    hypo_perception_bridge_t* bridge,
    float temperature);

/**
 * @brief Get thermal state
 *
 * @param bridge Bridge handle
 * @param state Output state
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_get_thermal_state(
    const hypo_perception_bridge_t* bridge,
    hypo_thermal_state_t* state);

/**
 * @brief Compute thermal salience boost
 *
 * @param bridge Bridge handle
 * @param boost Output salience boost [1.0, 3.0]
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_compute_thermal_salience(
    hypo_perception_bridge_t* bridge,
    float* boost);

/*=============================================================================
 * ENHANCED CONFIGURATION
 *===========================================================================*/

/**
 * @brief Enhanced configuration for all features
 */
typedef struct {
    /* Base configuration */
    hypo_perception_config_t base;

    /* Interoception */
    bool enable_interoception;
    float interoceptive_accuracy;

    /* Chemosensory */
    bool enable_chemosensory;
    float hunger_smell_boost;      /**< How much hunger boosts food smell */
    float thirst_salt_boost;       /**< How much thirst boosts salt taste */

    /* Predictive coding */
    bool enable_predictive_coding;
    float prediction_precision_default;
    float free_energy_weight;

    /* Pain modulation */
    bool enable_pain_modulation;
    float acute_stress_analgesia;  /**< Analgesia strength in acute stress */
    float chronic_stress_hyperalgesia; /**< Hyperalgesia in chronic stress */

    /* Sleep gating */
    bool enable_sleep_gating;
    float deep_sleep_gate;         /**< Gate value in deep sleep [0, 0.2] */
    float rem_sleep_gate;          /**< Gate value in REM [0.1, 0.3] */

    /* Thermal */
    bool enable_thermal_salience;
    float thermal_setpoint;        /**< Comfortable temperature [0.5] */
    float thermal_tolerance;       /**< Range of comfort [0.1] */
} hypo_perception_enhanced_config_t;

/**
 * @brief Get default enhanced configuration
 *
 * @param config Output configuration
 */
void hypo_perception_bridge_default_enhanced_config(
    hypo_perception_enhanced_config_t* config);

/**
 * @brief Apply enhanced configuration
 *
 * @param bridge Bridge handle
 * @param config Enhanced configuration
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_apply_enhanced_config(
    hypo_perception_bridge_t* bridge,
    const hypo_perception_enhanced_config_t* config);

/*=============================================================================
 * STATISTICS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Statistics for perception bridge
 */
typedef struct {
    uint32_t modulations_computed;
    uint32_t detections_processed;
    uint32_t interoceptive_signals;
    uint32_t olfactory_stimuli;
    uint32_t gustatory_stimuli;
    uint32_t predictions_generated;
    uint32_t prediction_errors_updated;
    uint32_t pain_stimuli_modulated;
    uint32_t sleep_gate_checks;
    uint32_t sleep_gate_passes;
    uint32_t thermal_updates;
    float avg_free_energy;
    float avg_pain_modulation;
    uint64_t uptime_us;
} hypo_perception_bridge_stats_t;

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_get_stats(
    const hypo_perception_bridge_t* bridge,
    hypo_perception_bridge_stats_t* stats);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_reset_stats(hypo_perception_bridge_t* bridge);

/**
 * @brief Print bridge summary
 *
 * @param bridge Bridge handle
 */
void hypo_perception_bridge_print_summary(const hypo_perception_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_reset(hypo_perception_bridge_t* bridge);

/**
 * @brief Update bridge (called periodically)
 *
 * @param bridge Bridge handle
 * @param delta_us Time since last update in microseconds
 * @return 0 on success, -1 on error
 */
int hypo_perception_bridge_update(
    hypo_perception_bridge_t* bridge,
    uint64_t delta_us);

/*=============================================================================
 * STRING UTILITIES
 *===========================================================================*/

const char* hypo_interoceptive_type_name(hypo_interoceptive_type_t type);
const char* hypo_olfactory_type_name(hypo_olfactory_type_t type);
const char* hypo_gustatory_type_name(hypo_gustatory_type_t type);
const char* hypo_prediction_type_name(hypo_prediction_type_t type);
const char* hypo_sleep_stage_name(hypo_sleep_stage_t stage);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_PERCEPTION_BRIDGE_H */
