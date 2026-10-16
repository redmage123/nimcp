/**
 * @file nimcp_brainstem_adapter.h
 * @brief Brain adapter for Brainstem region integration
 *
 * WHAT: Unified adapter connecting brainstem sub-modules to the brain system
 * WHY:  Enable seamless integration with cognitive layers, autonomic control, and event system
 * HOW:  Orchestrates midbrain, pons, medulla, and reticular formation as a cohesive unit
 *
 * ARCHITECTURE:
 * - Wraps all four brainstem sub-modules (midbrain, pons, medulla, reticular formation)
 * - Provides high-level API for vital function control
 * - Integrates with neuromodulators for arousal control
 * - Connects to event bus for inter-module communication
 * - Supports training through adaptation mechanisms
 *
 * BIOLOGICAL BASIS:
 * - Midbrain: Superior/inferior colliculus (visual/auditory processing)
 * - Pons: Relay nuclei, respiratory control, sleep-wake transitions
 * - Medulla: Vital functions (heart rate, breathing, blood pressure)
 * - Reticular Formation: Arousal, attention, sleep-wake regulation
 *
 * @version Phase BS-1: Brainstem Brain Integration
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAINSTEM_ADAPTER_H
#define NIMCP_BRAINSTEM_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bio-async communication system */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Logging system */
#include "utils/logging/nimcp_logging.h"

/* Unified memory system */
#include "utils/memory/nimcp_unified_memory.h"

/* Forward declarations for sub-modules */
typedef struct midbrain_processor midbrain_processor_t;
typedef struct pons_processor pons_processor_t;
typedef struct reticular_formation reticular_formation_t;

/* Forward declaration for medulla (already exists) */
typedef struct medulla_struct* medulla_t;

/* Forward declaration for opaque adapter type */
typedef struct brainstem_adapter brainstem_adapter_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Default configuration values
 */
#define BRAINSTEM_DEFAULT_MAX_REFLEXES        32
#define BRAINSTEM_DEFAULT_MAX_PATHWAYS        64
#define BRAINSTEM_DEFAULT_AROUSAL_BASELINE    0.5f
#define BRAINSTEM_DEFAULT_UPDATE_INTERVAL_MS  50.0f

/**
 * @brief Midbrain configuration
 */
typedef struct {
    /* Superior Colliculus (visual) */
    bool enable_superior_colliculus;     /**< Enable visual orienting */
    float saccade_threshold;             /**< Threshold for saccade generation */
    uint32_t max_saccade_targets;        /**< Max simultaneous targets */

    /* Inferior Colliculus (auditory) */
    bool enable_inferior_colliculus;     /**< Enable auditory processing */
    float sound_localization_precision;  /**< Spatial resolution [0-1] */

    /* Periaqueductal Gray (PAG) */
    bool enable_pag;                     /**< Enable pain/fear modulation */
    float defensive_threshold;           /**< Threshold for defensive behaviors */
} midbrain_config_t;

/**
 * @brief Pons configuration
 */
typedef struct {
    /* Relay functions */
    bool enable_corticopontine_relay;    /**< Cortex-cerebellum relay */
    float relay_latency_ms;              /**< Signal relay delay */

    /* Respiratory center */
    bool enable_pneumotaxic_center;      /**< Breathing rate modulation */
    float base_respiratory_rate;         /**< Baseline breaths per minute analog */

    /* Sleep-wake transition */
    bool enable_locus_coeruleus;         /**< Norepinephrine arousal */
    bool enable_raphe_nuclei;            /**< Serotonin regulation */
} pons_config_t;

/**
 * @brief Reticular formation configuration
 */
typedef struct {
    /* Ascending Reticular Activating System (ARAS) */
    bool enable_aras;                    /**< Enable arousal system */
    float baseline_arousal;              /**< Baseline arousal level [0-1] */
    float arousal_decay_rate;            /**< Decay rate per second */

    /* Descending modulation */
    bool enable_descending_modulation;   /**< Motor tone control */
    float motor_tone_baseline;           /**< Baseline muscle tone [0-1] */

    /* Attention filtering */
    bool enable_attention_filter;        /**< Sensory filtering */
    float habituation_rate;              /**< Rate of stimulus habituation */
} reticular_config_t;

/**
 * @brief Brainstem adapter configuration
 */
typedef struct {
    /* Sub-module configurations */
    midbrain_config_t midbrain;
    pons_config_t pons;
    reticular_config_t reticular;

    /* Use external medulla (if already created) */
    bool use_external_medulla;           /**< Use provided medulla instance */

    /* Processing options */
    bool enable_reflexes;                /**< Enable reflex pathways */
    bool enable_vital_monitoring;        /**< Enable vital function monitoring */
    bool enable_arousal_control;         /**< Enable arousal modulation */

    /* Event system */
    bool enable_events;                  /**< Enable event bus integration */

    /* Timing */
    float update_interval_ms;            /**< Update interval */

    /* Bio-async communication */
    bool enable_bio_async;               /**< Enable bio-async messaging */
    nimcp_bio_channel_type_t default_channel; /**< Default neuromodulator channel */
} brainstem_config_t;

/*=============================================================================
 * STATUS AND STATE
 *===========================================================================*/

/**
 * @brief Processing status of the adapter
 */
typedef enum {
    BRAINSTEM_STATUS_OFFLINE = 0,        /**< Not initialized */
    BRAINSTEM_STATUS_INITIALIZING,       /**< Starting up */
    BRAINSTEM_STATUS_ACTIVE,             /**< Normal operation */
    BRAINSTEM_STATUS_ALERT,              /**< Heightened arousal */
    BRAINSTEM_STATUS_PROTECTIVE,         /**< Protective mode */
    BRAINSTEM_STATUS_SLEEP,              /**< Sleep mode */
    BRAINSTEM_STATUS_ERROR               /**< Error state */
} brainstem_status_t;

/**
 * @brief Error codes for brainstem operations
 */
typedef enum {
    BRAINSTEM_ERROR_NONE = 0,
    BRAINSTEM_ERROR_INVALID_INPUT,
    BRAINSTEM_ERROR_MIDBRAIN_FAILURE,
    BRAINSTEM_ERROR_PONS_FAILURE,
    BRAINSTEM_ERROR_MEDULLA_FAILURE,
    BRAINSTEM_ERROR_RETICULAR_FAILURE,
    BRAINSTEM_ERROR_REFLEX_FAILURE,
    BRAINSTEM_ERROR_AROUSAL_FAILURE,
    BRAINSTEM_ERROR_INTERNAL
} brainstem_error_t;

/**
 * @brief Arousal level categories (maps to reticular formation)
 */
typedef enum {
    BRAINSTEM_AROUSAL_COMA = 0,          /**< Minimal activity */
    BRAINSTEM_AROUSAL_DEEP_SLEEP,        /**< Deep sleep */
    BRAINSTEM_AROUSAL_LIGHT_SLEEP,       /**< Light sleep */
    BRAINSTEM_AROUSAL_DROWSY,            /**< Drowsy/inattentive */
    BRAINSTEM_AROUSAL_AWAKE,             /**< Normal wakefulness */
    BRAINSTEM_AROUSAL_ALERT,             /**< High alertness */
    BRAINSTEM_AROUSAL_HYPERAROUSED       /**< Fight-or-flight */
} brainstem_arousal_level_t;

/*=============================================================================
 * INPUT/OUTPUT STRUCTURES
 *===========================================================================*/

/**
 * @brief Sensory input for midbrain processing
 */
typedef struct {
    /* Visual input (superior colliculus) */
    float visual_target_x;               /**< Target X position [-1, 1] */
    float visual_target_y;               /**< Target Y position [-1, 1] */
    float visual_salience;               /**< Target salience [0, 1] */
    bool visual_motion_detected;         /**< Motion detection flag */

    /* Auditory input (inferior colliculus) */
    float sound_azimuth;                 /**< Sound direction [0, 360] degrees */
    float sound_elevation;               /**< Sound elevation [-90, 90] degrees */
    float sound_intensity;               /**< Sound intensity [0, 1] */
    bool sudden_sound;                   /**< Startle stimulus flag */
} brainstem_sensory_input_t;

/**
 * @brief Reflex specification
 */
typedef struct {
    uint32_t reflex_id;                  /**< Unique reflex identifier */
    char name[32];                       /**< Reflex name */
    float threshold;                     /**< Activation threshold */
    float latency_ms;                    /**< Response latency */
    float gain;                          /**< Response magnitude multiplier */
    bool is_active;                      /**< Currently active */
} brainstem_reflex_t;

/**
 * @brief Motor output command (to spinal pathways)
 */
typedef struct {
    uint32_t pathway_id;                 /**< Target motor pathway */
    float activation;                    /**< Activation level [0, 1] */
    float urgency;                       /**< Response urgency [0, 1] */
    double timestamp_ms;                 /**< Execution time */
    bool is_reflex;                      /**< Reflex-driven (bypass cortex) */
} brainstem_motor_output_t;

/**
 * @brief Orienting response (from midbrain)
 */
typedef struct {
    float saccade_x;                     /**< Eye movement X component */
    float saccade_y;                     /**< Eye movement Y component */
    float head_turn;                     /**< Head rotation angle */
    float attention_shift;               /**< Attention allocation [0, 1] */
    bool reflex_triggered;               /**< Was this reflex-driven */
} brainstem_orienting_response_t;

/**
 * @brief Vital signs (from medulla)
 */
typedef struct {
    float heart_rate_analog;             /**< System activity rate [0, 1] */
    float respiratory_rate;              /**< Processing cycle rate [0, 1] */
    float blood_pressure_analog;         /**< Resource pressure [0, 1] */
    float temperature_analog;            /**< Thermal state [0, 1] */
    bool vital_alarm;                    /**< Vital function alarm */
} brainstem_vitals_t;

/**
 * @brief Complete brainstem state
 */
typedef struct {
    /* Status */
    brainstem_status_t status;
    brainstem_arousal_level_t arousal_level;
    float arousal_value;                 /**< Raw arousal [0, 1] */

    /* Vital signs */
    brainstem_vitals_t vitals;

    /* Orienting */
    brainstem_orienting_response_t orienting;

    /* Motor tone */
    float motor_tone;                    /**< Overall motor readiness [0, 1] */

    /* Sleep-wake */
    bool sleep_pressure_high;            /**< High homeostatic sleep pressure */
    float circadian_phase;               /**< Circadian phase [0, 24] hours */
} brainstem_state_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Adapter statistics
 */
typedef struct {
    /* Processing counts */
    uint64_t updates_processed;          /**< Total update cycles */
    uint64_t reflexes_triggered;         /**< Reflex activations */
    uint64_t saccades_generated;         /**< Saccadic movements */
    uint64_t arousal_modulations;        /**< Arousal level changes */

    /* Sub-module activity */
    uint64_t midbrain_activations;       /**< Midbrain processing events */
    uint64_t pons_relays;                /**< Pons relay events */
    uint64_t medulla_updates;            /**< Medulla vital updates */
    uint64_t reticular_updates;          /**< Reticular formation updates */

    /* Timing */
    float avg_latency_ms;                /**< Average processing latency */
    float max_latency_ms;                /**< Maximum latency observed */

    /* Arousal */
    float avg_arousal;                   /**< Average arousal level */
    uint32_t sleep_episodes;             /**< Sleep periods */
    uint32_t alert_episodes;             /**< High-alert periods */
} brainstem_stats_t;

/*=============================================================================
 * CALLBACK TYPES
 *===========================================================================*/

/**
 * @brief Callback for reflex activation
 */
typedef void (*brainstem_reflex_callback_t)(
    const brainstem_reflex_t* reflex,
    const brainstem_motor_output_t* response,
    void* user_data
);

/**
 * @brief Callback for arousal change
 */
typedef void (*brainstem_arousal_callback_t)(
    brainstem_arousal_level_t old_level,
    brainstem_arousal_level_t new_level,
    float arousal_value,
    void* user_data
);

/**
 * @brief Callback for vital alarm
 */
typedef void (*brainstem_vital_callback_t)(
    const brainstem_vitals_t* vitals,
    void* user_data
);

/**
 * @brief Callback for orienting response
 */
typedef void (*brainstem_orienting_callback_t)(
    const brainstem_orienting_response_t* response,
    void* user_data
);

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * @return Default configuration structure
 */
brainstem_config_t brainstem_default_config(void);

/**
 * @brief Create brainstem adapter
 *
 * @param config Configuration (NULL for defaults)
 * @param external_medulla Optional external medulla instance (can be NULL)
 * @return New adapter instance, or NULL on failure
 */
brainstem_adapter_t* brainstem_create(const brainstem_config_t* config,
                                       medulla_t external_medulla);

/**
 * @brief Destroy brainstem adapter
 *
 * @param adapter Adapter to destroy
 */
void brainstem_destroy(brainstem_adapter_t* adapter);

/**
 * @brief Reset adapter state
 *
 * @param adapter Adapter instance
 * @return true on success, false on failure
 */
bool brainstem_reset(brainstem_adapter_t* adapter);

/*=============================================================================
 * REFLEX MANAGEMENT
 *===========================================================================*/

/**
 * @brief Register a reflex pathway
 *
 * @param adapter Adapter instance
 * @param reflex Reflex specification
 * @return true on success, false if registry full
 */
bool brainstem_register_reflex(brainstem_adapter_t* adapter,
                                const brainstem_reflex_t* reflex);

/**
 * @brief Trigger a reflex by ID
 *
 * @param adapter Adapter instance
 * @param reflex_id Reflex identifier
 * @param stimulus_intensity Stimulus strength [0, 1]
 * @param output Output motor command (filled on success)
 * @return true if reflex activated, false otherwise
 */
bool brainstem_trigger_reflex(brainstem_adapter_t* adapter,
                               uint32_t reflex_id,
                               float stimulus_intensity,
                               brainstem_motor_output_t* output);

/**
 * @brief Set reflex callback
 *
 * @param adapter Adapter instance
 * @param callback Reflex activation handler
 * @param user_data User context
 * @return true on success
 */
bool brainstem_set_reflex_callback(brainstem_adapter_t* adapter,
                                    brainstem_reflex_callback_t callback,
                                    void* user_data);

/*=============================================================================
 * SENSORY PROCESSING (MIDBRAIN)
 *===========================================================================*/

/**
 * @brief Process sensory input through midbrain
 *
 * @param adapter Adapter instance
 * @param input Sensory input data
 * @param response Output orienting response
 * @return true on success
 */
bool brainstem_process_sensory(brainstem_adapter_t* adapter,
                                const brainstem_sensory_input_t* input,
                                brainstem_orienting_response_t* response);

/**
 * @brief Generate saccade to target
 *
 * @param adapter Adapter instance
 * @param target_x Target X position [-1, 1]
 * @param target_y Target Y position [-1, 1]
 * @param urgency Movement urgency [0, 1]
 * @return true on success
 */
bool brainstem_generate_saccade(brainstem_adapter_t* adapter,
                                 float target_x,
                                 float target_y,
                                 float urgency);

/**
 * @brief Set orienting callback
 *
 * @param adapter Adapter instance
 * @param callback Orienting response handler
 * @param user_data User context
 * @return true on success
 */
bool brainstem_set_orienting_callback(brainstem_adapter_t* adapter,
                                       brainstem_orienting_callback_t callback,
                                       void* user_data);

/*=============================================================================
 * AROUSAL CONTROL (RETICULAR FORMATION)
 *===========================================================================*/

/**
 * @brief Get current arousal level
 *
 * @param adapter Adapter instance
 * @return Current arousal level category
 */
brainstem_arousal_level_t brainstem_get_arousal_level(const brainstem_adapter_t* adapter);

/**
 * @brief Get raw arousal value
 *
 * @param adapter Adapter instance
 * @return Arousal value [0, 1]
 */
float brainstem_get_arousal_value(const brainstem_adapter_t* adapter);

/**
 * @brief Boost arousal (alerting stimulus)
 *
 * @param adapter Adapter instance
 * @param amount Arousal boost amount [0, 1]
 * @return true on success
 */
bool brainstem_boost_arousal(brainstem_adapter_t* adapter, float amount);

/**
 * @brief Reduce arousal (calming)
 *
 * @param adapter Adapter instance
 * @param amount Arousal reduction amount [0, 1]
 * @return true on success
 */
bool brainstem_reduce_arousal(brainstem_adapter_t* adapter, float amount);

/**
 * @brief Set target arousal level
 *
 * @param adapter Adapter instance
 * @param target Target arousal [0, 1]
 * @return true on success
 */
bool brainstem_set_target_arousal(brainstem_adapter_t* adapter, float target);

/**
 * @brief Set arousal callback
 *
 * @param adapter Adapter instance
 * @param callback Arousal change handler
 * @param user_data User context
 * @return true on success
 */
bool brainstem_set_arousal_callback(brainstem_adapter_t* adapter,
                                     brainstem_arousal_callback_t callback,
                                     void* user_data);

/*=============================================================================
 * VITAL FUNCTIONS (MEDULLA)
 *===========================================================================*/

/**
 * @brief Get current vital signs
 *
 * @param adapter Adapter instance
 * @param vitals Output vital signs
 * @return true on success
 */
bool brainstem_get_vitals(const brainstem_adapter_t* adapter,
                           brainstem_vitals_t* vitals);

/**
 * @brief Set vital alarm callback
 *
 * @param adapter Adapter instance
 * @param callback Vital alarm handler
 * @param user_data User context
 * @return true on success
 */
bool brainstem_set_vital_callback(brainstem_adapter_t* adapter,
                                   brainstem_vital_callback_t callback,
                                   void* user_data);

/**
 * @brief Trigger protective response
 *
 * @param adapter Adapter instance
 * @param severity Severity level [0, 1]
 * @return true on success
 */
bool brainstem_trigger_protection(brainstem_adapter_t* adapter, float severity);

/*=============================================================================
 * RELAY FUNCTIONS (PONS)
 *===========================================================================*/

/**
 * @brief Relay signal through pons
 *
 * @param adapter Adapter instance
 * @param signal Input signal array
 * @param signal_size Signal dimension
 * @param output Output buffer (same size as input)
 * @return true on success
 */
bool brainstem_relay_signal(brainstem_adapter_t* adapter,
                             const float* signal,
                             uint32_t signal_size,
                             float* output);

/**
 * @brief Modulate sleep-wake transition
 *
 * @param adapter Adapter instance
 * @param sleep_pressure Homeostatic sleep pressure [0, 1]
 * @return true on success
 */
bool brainstem_modulate_sleep(brainstem_adapter_t* adapter, float sleep_pressure);

/*=============================================================================
 * UPDATE AND STATE
 *===========================================================================*/

/**
 * @brief Main update tick
 *
 * @param adapter Adapter instance
 * @param dt Time step in seconds
 * @return true on success
 */
bool brainstem_update(brainstem_adapter_t* adapter, float dt);

/**
 * @brief Get complete brainstem state
 *
 * @param adapter Adapter instance
 * @param state Output state structure
 * @return true on success
 */
bool brainstem_get_state(const brainstem_adapter_t* adapter,
                          brainstem_state_t* state);

/**
 * @brief Get current processing status
 *
 * @param adapter Adapter instance
 * @return Current status
 */
brainstem_status_t brainstem_get_status(const brainstem_adapter_t* adapter);

/**
 * @brief Get last error code
 *
 * @param adapter Adapter instance
 * @return Last error, or BRAINSTEM_ERROR_NONE
 */
brainstem_error_t brainstem_get_last_error(const brainstem_adapter_t* adapter);

/**
 * @brief Get error description string
 *
 * @param error Error code
 * @return Human-readable error description
 */
const char* brainstem_error_string(brainstem_error_t error);

/**
 * @brief Get status description string
 *
 * @param status Status code
 * @return Human-readable status description
 */
const char* brainstem_status_string(brainstem_status_t status);

/**
 * @brief Get arousal level description string
 *
 * @param level Arousal level
 * @return Human-readable description
 */
const char* brainstem_arousal_string(brainstem_arousal_level_t level);

/**
 * @brief Get adapter statistics
 *
 * @param adapter Adapter instance
 * @param stats Output statistics structure
 * @return true on success
 */
bool brainstem_get_stats(const brainstem_adapter_t* adapter, brainstem_stats_t* stats);

/**
 * @brief Get adapter configuration
 *
 * @param adapter Adapter instance
 * @param config Output configuration structure
 * @return true on success
 */
bool brainstem_get_config(const brainstem_adapter_t* adapter, brainstem_config_t* config);

/*=============================================================================
 * SUB-MODULE ACCESS (Advanced)
 *===========================================================================*/

/**
 * @brief Get midbrain processor handle
 *
 * @param adapter Adapter instance
 * @return Midbrain processor, or NULL
 */
midbrain_processor_t* brainstem_get_midbrain(brainstem_adapter_t* adapter);

/**
 * @brief Get pons processor handle
 *
 * @param adapter Adapter instance
 * @return Pons processor, or NULL
 */
pons_processor_t* brainstem_get_pons(brainstem_adapter_t* adapter);

/**
 * @brief Get reticular formation handle
 *
 * @param adapter Adapter instance
 * @return Reticular formation, or NULL
 */
reticular_formation_t* brainstem_get_reticular(brainstem_adapter_t* adapter);

/**
 * @brief Get medulla handle
 *
 * @param adapter Adapter instance
 * @return Medulla, or NULL
 */
medulla_t brainstem_get_medulla(brainstem_adapter_t* adapter);

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

/**
 * @brief Get bio-async module context
 *
 * @param adapter Adapter instance
 * @return Bio-async module context, or NULL if not enabled
 */
bio_module_context_t brainstem_get_bio_context(brainstem_adapter_t* adapter);

/**
 * @brief Process pending bio-async messages
 *
 * @param adapter Adapter instance
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t brainstem_process_bio_messages(brainstem_adapter_t* adapter,
                                         uint32_t max_messages);

/**
 * @brief Broadcast arousal state change
 *
 * @param adapter Adapter instance
 * @param new_level New arousal level
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t brainstem_broadcast_arousal_change(brainstem_adapter_t* adapter,
                                                   brainstem_arousal_level_t new_level);

/**
 * @brief Broadcast reflex activation
 *
 * @param adapter Adapter instance
 * @param reflex Activated reflex
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t brainstem_broadcast_reflex(brainstem_adapter_t* adapter,
                                          const brainstem_reflex_t* reflex);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAINSTEM_ADAPTER_H */
