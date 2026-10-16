/**
 * @file nimcp_insula_adapter.h
 * @brief Brain adapter for Insula region integration
 *
 * WHAT: Unified adapter connecting Insula region sub-modules to the brain system
 * WHY:  Enable interoception, emotional awareness, and social emotion processing
 * HOW:  Orchestrates interoception, emotional awareness, and disgust/social processors
 *
 * ARCHITECTURE:
 * - Wraps all Insula sub-modules (interoception, emotional awareness, social emotions)
 * - Provides high-level API for body-brain integration pipeline
 * - Integrates with limbic system for emotional processing
 * - Connects to somatosensory cortex for body state awareness
 * - Supports training through backpropagation adapters
 *
 * BIOLOGICAL BASIS:
 * - Models anterior insula (interoception, emotional awareness)
 * - Models posterior insula (somatosensory integration)
 * - Mid-insula transitions between visceral and cognitive processing
 * - Key hub connecting bodily states to emotional experience
 *
 * FUNCTIONAL DOMAINS:
 * - Interoception: Awareness of internal body states (heartbeat, breathing, hunger)
 * - Emotional Awareness: Subjective feeling states, emotional salience
 * - Disgust Processing: Physical and moral disgust responses
 * - Social Emotions: Empathy, trust, fairness, social rejection
 *
 * @version Phase I1: Insula Integration
 * @date 2025-12-30
 */

#ifndef NIMCP_INSULA_ADAPTER_H
#define NIMCP_INSULA_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bio-async communication system */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

/* Logging system */
#include "utils/logging/nimcp_logging.h"

/* Forward declaration for opaque adapter type */
typedef struct insula_adapter insula_adapter_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Default configuration values
 */
#define INSULA_DEFAULT_INTEROCEPTION_CHANNELS    16    /**< Body signal channels */
#define INSULA_DEFAULT_EMOTION_DIMENSIONS         8    /**< Valence/arousal dimensions */
#define INSULA_DEFAULT_SOCIAL_EMOTION_TYPES      12    /**< Social emotion categories */
#define INSULA_DEFAULT_BODY_MAP_RESOLUTION       64    /**< Body map grid resolution */
#define INSULA_DEFAULT_UPDATE_RATE_HZ           10.0f  /**< Interoceptive update rate */
#define INSULA_DEFAULT_INTEGRATION_WINDOW_MS   500.0f  /**< Temporal integration window */

/**
 * @brief Interoceptive channel types
 */
typedef enum {
    INTERO_CHANNEL_CARDIAC = 0,       /**< Heart rate, HRV */
    INTERO_CHANNEL_RESPIRATORY,       /**< Breathing rate, depth */
    INTERO_CHANNEL_GASTRIC,           /**< Stomach/gut sensations */
    INTERO_CHANNEL_THERMAL,           /**< Body temperature */
    INTERO_CHANNEL_PAIN,              /**< Nociceptive signals */
    INTERO_CHANNEL_PROPRIOCEPTIVE,    /**< Body position sense */
    INTERO_CHANNEL_VESTIBULAR,        /**< Balance/motion */
    INTERO_CHANNEL_HUNGER,            /**< Metabolic needs */
    INTERO_CHANNEL_THIRST,            /**< Hydration status */
    INTERO_CHANNEL_FATIGUE,           /**< Energy/tiredness */
    INTERO_CHANNEL_AROUSAL,           /**< Physiological arousal */
    INTERO_CHANNEL_STRESS,            /**< Stress response markers */
    INTERO_CHANNEL_IMMUNE,            /**< Immune system signals */
    INTERO_CHANNEL_HORMONAL,          /**< Endocrine state */
    INTERO_CHANNEL_PLEASURE,          /**< Hedonic signals */
    INTERO_CHANNEL_DISCOMFORT,        /**< Aversive signals */
    INTERO_CHANNEL_COUNT              /**< Total channel count */
} insula_intero_channel_t;

/**
 * @brief Social emotion types processed by insula
 */
typedef enum {
    SOCIAL_EMOTION_DISGUST = 0,       /**< Physical/moral disgust */
    SOCIAL_EMOTION_EMPATHY,           /**< Shared emotional experience */
    SOCIAL_EMOTION_TRUST,             /**< Trust/distrust */
    SOCIAL_EMOTION_FAIRNESS,          /**< Fairness/inequity aversion */
    SOCIAL_EMOTION_REJECTION,         /**< Social rejection/exclusion */
    SOCIAL_EMOTION_EMBARRASSMENT,     /**< Self-conscious embarrassment */
    SOCIAL_EMOTION_SHAME,             /**< Deep shame response */
    SOCIAL_EMOTION_GUILT,             /**< Guilt/remorse */
    SOCIAL_EMOTION_CONTEMPT,          /**< Contempt toward others */
    SOCIAL_EMOTION_ADMIRATION,        /**< Admiration/respect */
    SOCIAL_EMOTION_GRATITUDE,         /**< Gratitude response */
    SOCIAL_EMOTION_COMPASSION,        /**< Compassionate concern */
    SOCIAL_EMOTION_COUNT              /**< Total social emotion types */
} insula_social_emotion_t;

/**
 * @brief Disgust subtypes
 */
typedef enum {
    DISGUST_CORE = 0,         /**< Core disgust (food, contamination) */
    DISGUST_ANIMAL_REMINDER,  /**< Body products, death, envelope violations */
    DISGUST_INTERPERSONAL,    /**< Contact with strangers, undesirables */
    DISGUST_MORAL,            /**< Moral transgressions */
    DISGUST_SOCIO_MORAL,      /**< Social norm violations */
    DISGUST_COUNT
} insula_disgust_type_t;

/**
 * @brief Insula adapter configuration
 */
typedef struct {
    /* Interoception settings */
    uint32_t interoception_channels;      /**< Number of body signal channels */
    float interoception_sensitivity;      /**< Sensitivity to body signals [0, 1] */
    float interoception_update_hz;        /**< Update frequency */
    bool enable_cardiac_awareness;        /**< Enable heartbeat detection */
    bool enable_respiratory_awareness;    /**< Enable breath awareness */
    bool enable_gastric_awareness;        /**< Enable gut feelings */

    /* Emotional awareness settings */
    uint32_t emotion_dimensions;          /**< Emotional space dimensions */
    float emotional_sensitivity;          /**< Emotional signal sensitivity */
    float emotional_integration_ms;       /**< Integration time window */
    bool enable_valence_tracking;         /**< Track positive/negative */
    bool enable_arousal_tracking;         /**< Track activation level */

    /* Social emotion settings */
    uint32_t social_emotion_types;        /**< Number of social emotions */
    float social_sensitivity;             /**< Sensitivity to social cues */
    bool enable_disgust_processing;       /**< Enable disgust responses */
    bool enable_empathy_processing;       /**< Enable empathic responses */
    bool enable_trust_processing;         /**< Enable trust/distrust */

    /* Body mapping */
    uint32_t body_map_resolution;         /**< Body map grid size */
    bool enable_body_ownership;           /**< Track body ownership */
    bool enable_agency_sense;             /**< Track sense of agency */

    /* Integration */
    bool enable_limbic_integration;       /**< Connect to limbic system */
    bool enable_somatosensory_integration; /**< Connect to S1 */
    bool enable_prefrontal_integration;   /**< Connect to PFC */

    /* Event system */
    bool enable_events;                   /**< Enable event bus integration */

    /* Training */
    bool enable_training;                 /**< Enable learning capabilities */
    float learning_rate;                  /**< Base learning rate */

    /* Bio-async communication */
    bool enable_bio_async;                /**< Enable bio-async messaging */
    nimcp_bio_channel_type_t default_channel; /**< Default neuromodulator channel */
} insula_config_t;

/*=============================================================================
 * STATUS AND STATE
 *===========================================================================*/

/**
 * @brief Processing status of the adapter
 */
typedef enum {
    INSULA_STATUS_IDLE = 0,           /**< Ready for input */
    INSULA_STATUS_INTEROCEPTION,      /**< Processing body signals */
    INSULA_STATUS_EMOTIONAL,          /**< Processing emotions */
    INSULA_STATUS_SOCIAL,             /**< Processing social emotions */
    INSULA_STATUS_INTEGRATION,        /**< Integrating signals */
    INSULA_STATUS_READY,              /**< Output ready */
    INSULA_STATUS_ERROR               /**< Error state */
} insula_status_t;

/**
 * @brief Error codes for Insula operations
 */
typedef enum {
    INSULA_ERROR_NONE = 0,
    INSULA_ERROR_INVALID_INPUT,
    INSULA_ERROR_INTEROCEPTION_FAILURE,
    INSULA_ERROR_EMOTIONAL_FAILURE,
    INSULA_ERROR_SOCIAL_FAILURE,
    INSULA_ERROR_INTEGRATION_FAILURE,
    INSULA_ERROR_BUFFER_OVERFLOW,
    INSULA_ERROR_INTERNAL
} insula_error_t;

/*=============================================================================
 * INPUT/OUTPUT STRUCTURES
 *===========================================================================*/

/**
 * @brief Interoceptive signal input
 */
typedef struct {
    insula_intero_channel_t channel;  /**< Signal channel type */
    float intensity;                  /**< Signal intensity [0, 1] */
    float rate_of_change;             /**< Temporal derivative */
    float reliability;                /**< Signal reliability [0, 1] */
    double timestamp_ms;              /**< Signal timestamp */
} insula_intero_signal_t;

/**
 * @brief Body state representation
 */
typedef struct {
    /* Vital signals */
    float heart_rate;                 /**< BPM */
    float heart_rate_variability;     /**< HRV (ms) */
    float respiratory_rate;           /**< Breaths per minute */
    float respiratory_depth;          /**< Tidal volume [0, 1] */
    float body_temperature;           /**< Celsius */

    /* Metabolic state */
    float hunger_level;               /**< [0, 1] */
    float thirst_level;               /**< [0, 1] */
    float fatigue_level;              /**< [0, 1] */
    float energy_level;               /**< [0, 1] */

    /* Arousal state */
    float physiological_arousal;      /**< [0, 1] */
    float stress_level;               /**< [0, 1] */
    float pain_level;                 /**< [0, 1] */
    float comfort_level;              /**< [0, 1] */

    /* Homeostatic deviation */
    float homeostatic_error;          /**< Distance from setpoint [0, 1] */
    float allostatic_load;            /**< Cumulative stress burden */

    /* Temporal */
    double timestamp_ms;              /**< State timestamp */
} insula_body_state_t;

/**
 * @brief Emotional state representation
 */
typedef struct {
    /* Core dimensions (Russell's circumplex) */
    float valence;                    /**< Negative to positive [-1, 1] */
    float arousal;                    /**< Low to high activation [-1, 1] */
    float dominance;                  /**< Submissive to dominant [-1, 1] */

    /* Felt emotion intensities */
    float joy;                        /**< [0, 1] */
    float sadness;                    /**< [0, 1] */
    float fear;                       /**< [0, 1] */
    float anger;                      /**< [0, 1] */
    float disgust;                    /**< [0, 1] */
    float surprise;                   /**< [0, 1] */
    float contempt;                   /**< [0, 1] */

    /* Meta-emotional awareness */
    float emotional_clarity;          /**< How clear is the feeling [0, 1] */
    float emotional_intensity;        /**< Overall intensity [0, 1] */
    float emotional_stability;        /**< Rate of change (inverse) [0, 1] */

    /* Somatic markers */
    bool has_somatic_marker;          /**< Gut feeling present */
    float somatic_valence;            /**< Body-based good/bad [-1, 1] */

    double timestamp_ms;              /**< State timestamp */
} insula_emotional_state_t;

/**
 * @brief Social emotion state
 */
typedef struct {
    /* Disgust processing */
    float disgust_intensity;          /**< Overall disgust [0, 1] */
    insula_disgust_type_t disgust_type; /**< Type of disgust */

    /* Empathy state */
    float empathic_resonance;         /**< Shared feeling [0, 1] */
    float empathic_concern;           /**< Concern for other [0, 1] */
    float perspective_taking;         /**< Cognitive empathy [0, 1] */

    /* Trust state */
    float trust_level;                /**< Trust in other [0, 1] */
    float trustworthiness_estimate;   /**< Estimated trustworthiness */
    bool betrayal_detected;           /**< Betrayal violation */

    /* Fairness state */
    float fairness_assessment;        /**< Fair/unfair [-1, 1] */
    float inequity_aversion;          /**< Reaction to unfairness */
    bool social_norm_violation;       /**< Norm was violated */

    /* Social rejection */
    float rejection_sensitivity;      /**< Pain from rejection [0, 1] */
    float social_pain;                /**< Social exclusion pain [0, 1] */
    float belonging_need;             /**< Need for inclusion [0, 1] */

    double timestamp_ms;
} insula_social_state_t;

/**
 * @brief Integrated insula output
 */
typedef struct {
    /* Body state summary */
    insula_body_state_t body_state;

    /* Emotional state summary */
    insula_emotional_state_t emotional_state;

    /* Social state summary */
    insula_social_state_t social_state;

    /* Integration metrics */
    float interoceptive_accuracy;     /**< How well body state tracked */
    float emotional_awareness;        /**< Clarity of emotional experience */
    float social_sensitivity;         /**< Responsiveness to social cues */

    /* Decision guidance */
    float approach_motivation;        /**< Drive toward [0, 1] */
    float avoidance_motivation;       /**< Drive away [0, 1] */
    float risk_assessment;            /**< Gut-feel risk level [0, 1] */

    /* Flags */
    bool urgent_signal;               /**< Requires immediate attention */
    bool homeostatic_alarm;           /**< Body needs intervention */
    bool emotional_alarm;             /**< Emotional state needs attention */
    bool social_alarm;                /**< Social threat detected */

    double timestamp_ms;
} insula_output_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Adapter statistics
 */
typedef struct {
    /* Processing counts */
    uint64_t intero_signals_processed;    /**< Body signals processed */
    uint64_t emotional_updates;           /**< Emotional state updates */
    uint64_t social_assessments;          /**< Social emotion assessments */
    uint64_t integration_cycles;          /**< Full integration cycles */

    /* Accuracy metrics */
    float avg_interoceptive_accuracy;     /**< Average body awareness */
    float avg_emotional_clarity;          /**< Average emotional clarity */
    float avg_social_sensitivity;         /**< Average social sensitivity */

    /* Event counts */
    uint64_t disgust_responses;           /**< Disgust events triggered */
    uint64_t empathy_responses;           /**< Empathic responses */
    uint64_t trust_violations;            /**< Betrayal detections */
    uint64_t homeostatic_alarms;          /**< Body alarm events */

    /* Timing */
    float avg_processing_time_ms;         /**< Average processing time */
    float max_processing_time_ms;         /**< Maximum processing time */

    /* Training */
    uint64_t training_iterations;         /**< Training updates */
    float training_loss;                  /**< Current loss */
} insula_stats_t;

/*=============================================================================
 * CALLBACK TYPES
 *===========================================================================*/

/**
 * @brief Callback for body state changes
 */
typedef void (*insula_body_callback_t)(
    const insula_body_state_t* state,
    void* user_data
);

/**
 * @brief Callback for emotional state changes
 */
typedef void (*insula_emotion_callback_t)(
    const insula_emotional_state_t* state,
    void* user_data
);

/**
 * @brief Callback for social emotion events
 */
typedef void (*insula_social_callback_t)(
    insula_social_emotion_t emotion_type,
    float intensity,
    void* user_data
);

/**
 * @brief Callback for alarm events
 */
typedef void (*insula_alarm_callback_t)(
    const char* alarm_type,
    float urgency,
    void* user_data
);

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * WHAT: Returns default configuration for Insula adapter
 * WHY:  Provide sensible defaults for common use cases
 * HOW:  Initialize all fields with biologically-motivated values
 *
 * @return Default configuration structure
 */
insula_config_t insula_default_config(void);

/**
 * @brief Create Insula adapter
 *
 * WHAT: Allocate and initialize the adapter with all sub-modules
 * WHY:  Central point for interoceptive and emotional processing
 * HOW:  Create processors, initialize body maps and emotional state
 *
 * @param config Configuration (NULL for defaults)
 * @return New adapter instance, or NULL on failure
 */
insula_adapter_t* insula_create(const insula_config_t* config);

/**
 * @brief Destroy Insula adapter
 *
 * WHAT: Free all resources associated with the adapter
 * WHY:  Prevent memory leaks
 * HOW:  Destroy sub-modules, free buffers
 *
 * @param adapter Adapter to destroy
 */
void insula_destroy(insula_adapter_t* adapter);

/**
 * @brief Reset adapter state
 *
 * WHAT: Clear buffers and reset to idle state
 * WHY:  Prepare for fresh processing
 * HOW:  Reset all processors, clear state
 *
 * @param adapter Adapter instance
 * @return true on success, false on failure
 */
bool insula_reset(insula_adapter_t* adapter);

/*=============================================================================
 * INTEROCEPTION API
 *===========================================================================*/

/**
 * @brief Update interoceptive channel
 *
 * WHAT: Feed body signal to insula
 * WHY:  Build awareness of internal body state
 * HOW:  Update channel state with temporal integration
 *
 * @param adapter Adapter instance
 * @param signal Interoceptive signal
 * @return true on success
 */
bool insula_update_interoception(insula_adapter_t* adapter,
                                   const insula_intero_signal_t* signal);

/**
 * @brief Update multiple interoceptive channels
 *
 * WHAT: Batch update body signals
 * WHY:  Efficient multi-channel processing
 * HOW:  Process all signals with shared timestamp
 *
 * @param adapter Adapter instance
 * @param signals Array of signals
 * @param count Number of signals
 * @return true on success
 */
bool insula_update_interoception_batch(insula_adapter_t* adapter,
                                         const insula_intero_signal_t* signals,
                                         uint32_t count);

/**
 * @brief Get current body state
 *
 * WHAT: Retrieve integrated body state
 * WHY:  Access body awareness for decision-making
 * HOW:  Return current body state estimate
 *
 * @param adapter Adapter instance
 * @param state Output state structure
 * @return true on success
 */
bool insula_get_body_state(const insula_adapter_t* adapter,
                            insula_body_state_t* state);

/**
 * @brief Set interoceptive sensitivity
 *
 * WHAT: Adjust sensitivity to body signals
 * WHY:  Allow dynamic attention to body
 * HOW:  Scale signal processing gain
 *
 * @param adapter Adapter instance
 * @param channel Channel to adjust (or -1 for all)
 * @param sensitivity New sensitivity [0, 1]
 * @return true on success
 */
bool insula_set_interoceptive_sensitivity(insula_adapter_t* adapter,
                                            int channel,
                                            float sensitivity);

/*=============================================================================
 * EMOTIONAL AWARENESS API
 *===========================================================================*/

/**
 * @brief Process emotional input
 *
 * WHAT: Integrate emotional signals
 * WHY:  Build subjective emotional experience
 * HOW:  Combine body state with emotional tags
 *
 * @param adapter Adapter instance
 * @param valence Emotional valence [-1, 1]
 * @param arousal Emotional arousal [-1, 1]
 * @param source Source of emotional signal
 * @return true on success
 */
bool insula_process_emotion(insula_adapter_t* adapter,
                             float valence,
                             float arousal,
                             const char* source);

/**
 * @brief Get current emotional state
 *
 * WHAT: Retrieve integrated emotional state
 * WHY:  Access emotional awareness
 * HOW:  Return current emotional state estimate
 *
 * @param adapter Adapter instance
 * @param state Output state structure
 * @return true on success
 */
bool insula_get_emotional_state(const insula_adapter_t* adapter,
                                  insula_emotional_state_t* state);

/**
 * @brief Generate somatic marker
 *
 * WHAT: Create body-based emotional memory
 * WHY:  Enable gut-feeling decision guidance
 * HOW:  Associate body state with decision context
 *
 * @param adapter Adapter instance
 * @param context Decision context identifier
 * @param valence Good/bad outcome [-1, 1]
 * @return true on success
 */
bool insula_create_somatic_marker(insula_adapter_t* adapter,
                                    uint32_t context,
                                    float valence);

/**
 * @brief Query somatic marker
 *
 * WHAT: Retrieve gut feeling for context
 * WHY:  Use body wisdom in decisions
 * HOW:  Look up associated body state
 *
 * @param adapter Adapter instance
 * @param context Decision context identifier
 * @param valence Output: stored valence
 * @param confidence Output: marker confidence
 * @return true if marker exists
 */
bool insula_query_somatic_marker(const insula_adapter_t* adapter,
                                   uint32_t context,
                                   float* valence,
                                   float* confidence);

/*=============================================================================
 * DISGUST AND SOCIAL EMOTION API
 *===========================================================================*/

/**
 * @brief Process disgust stimulus
 *
 * WHAT: Evaluate disgust-eliciting input
 * WHY:  Detect contamination and moral violations
 * HOW:  Process through disgust circuitry
 *
 * @param adapter Adapter instance
 * @param stimulus_type Type of disgusting stimulus
 * @param intensity Stimulus intensity [0, 1]
 * @param is_moral Is this moral (vs physical) disgust
 * @return Disgust response intensity [0, 1]
 */
float insula_process_disgust(insula_adapter_t* adapter,
                              insula_disgust_type_t stimulus_type,
                              float intensity,
                              bool is_moral);

/**
 * @brief Process empathy signal
 *
 * WHAT: Share emotional state of observed other
 * WHY:  Build empathic resonance
 * HOW:  Mirror emotional state with perspective adjustment
 *
 * @param adapter Adapter instance
 * @param other_valence Other's emotional valence
 * @param other_arousal Other's emotional arousal
 * @param similarity Self-other similarity [0, 1]
 * @return Empathic resonance [0, 1]
 */
float insula_process_empathy(insula_adapter_t* adapter,
                              float other_valence,
                              float other_arousal,
                              float similarity);

/**
 * @brief Assess trust/trustworthiness
 *
 * WHAT: Evaluate trustworthiness of other
 * WHY:  Guide social decision-making
 * HOW:  Integrate facial, behavioral, and history cues
 *
 * @param adapter Adapter instance
 * @param face_trustworthiness Facial cue [0, 1]
 * @param behavior_reliability Past behavior reliability
 * @param reciprocity Reciprocal behavior score
 * @return Trust assessment [0, 1]
 */
float insula_assess_trust(insula_adapter_t* adapter,
                           float face_trustworthiness,
                           float behavior_reliability,
                           float reciprocity);

/**
 * @brief Process fairness violation
 *
 * WHAT: React to unfair treatment
 * WHY:  Enforce social norms
 * HOW:  Generate inequity aversion response
 *
 * @param adapter Adapter instance
 * @param own_outcome Own outcome value
 * @param other_outcome Other's outcome value
 * @param is_self_disadvantaged Self got less
 * @return Fairness response (negative = unfair)
 */
float insula_process_fairness(insula_adapter_t* adapter,
                               float own_outcome,
                               float other_outcome,
                               bool is_self_disadvantaged);

/**
 * @brief Process social rejection
 *
 * WHAT: React to social exclusion
 * WHY:  Signal social pain
 * HOW:  Activate rejection sensitivity
 *
 * @param adapter Adapter instance
 * @param rejection_intensity How strong the rejection [0, 1]
 * @param source_importance How important is the source [0, 1]
 * @return Social pain intensity [0, 1]
 */
float insula_process_rejection(insula_adapter_t* adapter,
                                float rejection_intensity,
                                float source_importance);

/**
 * @brief Get current social emotional state
 *
 * @param adapter Adapter instance
 * @param state Output state structure
 * @return true on success
 */
bool insula_get_social_state(const insula_adapter_t* adapter,
                               insula_social_state_t* state);

/*=============================================================================
 * INTEGRATION API
 *===========================================================================*/

/**
 * @brief Perform full integration cycle
 *
 * WHAT: Integrate all insula processing streams
 * WHY:  Generate unified body-emotion-social output
 * HOW:  Combine interoception, emotion, and social processing
 *
 * @param adapter Adapter instance
 * @param output Output structure (optional)
 * @return true on success
 */
bool insula_integrate(insula_adapter_t* adapter, insula_output_t* output);

/**
 * @brief Update simulation time
 *
 * WHAT: Advance insula to new time
 * WHY:  Enable temporal dynamics
 * HOW:  Update all time-dependent processes
 *
 * @param adapter Adapter instance
 * @param time_ms New simulation time
 * @return true on success
 */
bool insula_step(insula_adapter_t* adapter, double time_ms);

/**
 * @brief Get full integrated output
 *
 * @param adapter Adapter instance
 * @param output Output structure
 * @return true on success
 */
bool insula_get_output(const insula_adapter_t* adapter, insula_output_t* output);

/*=============================================================================
 * CALLBACK REGISTRATION
 *===========================================================================*/

/**
 * @brief Set body state change callback
 */
bool insula_set_body_callback(insula_adapter_t* adapter,
                                insula_body_callback_t callback,
                                void* user_data);

/**
 * @brief Set emotional state change callback
 */
bool insula_set_emotion_callback(insula_adapter_t* adapter,
                                   insula_emotion_callback_t callback,
                                   void* user_data);

/**
 * @brief Set social emotion callback
 */
bool insula_set_social_callback(insula_adapter_t* adapter,
                                  insula_social_callback_t callback,
                                  void* user_data);

/**
 * @brief Set alarm callback
 */
bool insula_set_alarm_callback(insula_adapter_t* adapter,
                                 insula_alarm_callback_t callback,
                                 void* user_data);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get current processing status
 */
insula_status_t insula_get_status(const insula_adapter_t* adapter);

/**
 * @brief Get last error code
 */
insula_error_t insula_get_last_error(const insula_adapter_t* adapter);

/**
 * @brief Get error description string
 */
const char* insula_error_string(insula_error_t error);

/**
 * @brief Get status description string
 */
const char* insula_status_string(insula_status_t status);

/**
 * @brief Get adapter statistics
 */
bool insula_get_stats(const insula_adapter_t* adapter, insula_stats_t* stats);

/**
 * @brief Get adapter configuration
 */
bool insula_get_config(const insula_adapter_t* adapter, insula_config_t* config);

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

/**
 * @brief Get bio-async module context
 */
bio_module_context_t insula_get_bio_context(insula_adapter_t* adapter);

/**
 * @brief Process pending bio-async messages
 */
uint32_t insula_process_bio_messages(insula_adapter_t* adapter, uint32_t max_messages);

/**
 * @brief Broadcast body state change
 */
nimcp_error_t insula_broadcast_body_state(insula_adapter_t* adapter,
                                            const insula_body_state_t* state);

/**
 * @brief Broadcast emotional state change
 */
nimcp_error_t insula_broadcast_emotional_state(insula_adapter_t* adapter,
                                                 const insula_emotional_state_t* state);

/**
 * @brief Broadcast social alarm
 */
nimcp_error_t insula_broadcast_social_alarm(insula_adapter_t* adapter,
                                              insula_social_emotion_t emotion,
                                              float intensity);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INSULA_ADAPTER_H */
