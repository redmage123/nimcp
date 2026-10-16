//=============================================================================
// nimcp_language_motor_bridge.h - Language-Motor Articulatory Bridge
//=============================================================================
/**
 * @file nimcp_language_motor_bridge.h
 * @brief Bidirectional bridge integrating Language Layer with Motor Cortex
 *
 * WHAT: Bridge connecting speech motor planning (Broca) with Motor Cortex
 * WHY:  Enable articulatory motor control, proprioceptive feedback, and
 *       coordinated speech production with accurate timing
 * HOW:  Broca sends motor commands for articulation; Motor Cortex provides
 *       proprioceptive feedback and movement status
 *
 * BIOLOGICAL BASIS:
 * - Primary Motor Cortex (M1, BA4): Direct control of articulators
 * - Broca's Area (BA44): Speech motor planning
 * - Face region of motor homunculus: Lips, tongue, jaw, larynx
 * - Corticobulbar tract: Motor commands to speech muscles
 * - Proprioceptive feedback: Muscle spindles, mechanoreceptors
 *
 * KEY CONNECTIONS:
 * - Broca → Motor: Motor programs for phonemes, timing specifications
 * - Motor → Broca: Proprioceptive feedback, execution status, errors
 * - Supplementary Motor Area: Sequencing of speech movements
 * - Cerebellum: Timing coordination (via separate bridge)
 *
 * ARTICULATORY FUNCTIONS:
 * - Phoneme execution: Convert phonological plan to motor commands
 * - Coarticulation: Smooth transitions between phonemes
 * - Timing control: Speech rate, rhythm, prosody
 * - Feedback monitoring: Compare intended vs actual articulation
 * - Error correction: Adjust ongoing speech production
 *
 * @version 1.0.0 - Phase LM1: Language-Motor Integration
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifndef NIMCP_LANGUAGE_MOTOR_BRIDGE_H
#define NIMCP_LANGUAGE_MOTOR_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

/* Include motor adapter first to avoid include order issues */
#include "core/brain/regions/motor/nimcp_motor_adapter.h"

/* Language types after bio_messages */
#include "language/nimcp_language_types.h"
#include "language/nimcp_language_config.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct language_motor_bridge language_motor_bridge_t;
typedef struct language_orchestrator language_orchestrator_t;
typedef struct broca_adapter broca_adapter_t;

#ifndef NIMCP_BIO_ROUTER_H
typedef void* bio_router_t;
#endif

//=============================================================================
// Constants
//=============================================================================

#define LANGUAGE_MOTOR_MODULE_NAME      "language_motor_bridge"
#define LANGUAGE_MOTOR_MODULE_VERSION   "1.0.0"
#define LANGUAGE_MOTOR_BIO_MODULE_ID    0x0822

/* Default configuration values */
#define LM_DEFAULT_UPDATE_INTERVAL_MS        10
#define LM_DEFAULT_MAX_ARTICULATORS          16
#define LM_DEFAULT_MAX_COMMAND_QUEUE         64
#define LM_DEFAULT_FEEDBACK_WINDOW_MS        50.0f
#define LM_DEFAULT_TIMING_TOLERANCE_MS       20.0f
#define LM_DEFAULT_POSITION_TOLERANCE        0.05f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Speech articulator identifiers
 */
typedef enum {
    ARTICULATOR_LIPS = 0,             /**< Lip aperture/rounding */
    ARTICULATOR_TONGUE_TIP,           /**< Tongue tip position */
    ARTICULATOR_TONGUE_BODY,          /**< Tongue body position */
    ARTICULATOR_TONGUE_ROOT,          /**< Tongue root position */
    ARTICULATOR_JAW,                  /**< Jaw opening */
    ARTICULATOR_VELUM,                /**< Velum (soft palate) */
    ARTICULATOR_LARYNX,               /**< Larynx (voicing) */
    ARTICULATOR_GLOTTIS,              /**< Glottal aperture */
    ARTICULATOR_COUNT
} articulator_type_t;

/**
 * @brief Articulator command type
 */
typedef enum {
    ARTIC_CMD_POSITION = 0,           /**< Set target position */
    ARTIC_CMD_VELOCITY,               /**< Set target velocity */
    ARTIC_CMD_FORCE,                  /**< Set applied force */
    ARTIC_CMD_HOLD,                   /**< Hold current position */
    ARTIC_CMD_RELEASE,                /**< Release to rest position */
    ARTIC_CMD_COUNT
} articulator_command_type_t;

/**
 * @brief Speech execution state
 */
typedef enum {
    SPEECH_EXEC_IDLE = 0,             /**< No speech in progress */
    SPEECH_EXEC_PREPARING,            /**< Preparing articulators */
    SPEECH_EXEC_PRODUCING,            /**< Active speech production */
    SPEECH_EXEC_PAUSED,               /**< Production paused */
    SPEECH_EXEC_COMPLETING,           /**< Finishing current utterance */
    SPEECH_EXEC_ERROR,                /**< Execution error */
    SPEECH_EXEC_COUNT
} speech_execution_state_t;

/**
 * @brief Feedback type from motor cortex
 */
typedef enum {
    FEEDBACK_PROPRIOCEPTIVE = 0,      /**< Muscle position/velocity */
    FEEDBACK_TACTILE,                 /**< Touch/contact sensors */
    FEEDBACK_TIMING,                  /**< Movement timing */
    FEEDBACK_ERROR,                   /**< Execution error signal */
    FEEDBACK_COMPLETION,              /**< Movement complete */
    FEEDBACK_COUNT
} motor_feedback_type_t;

/**
 * @brief Bridge operating state
 */
typedef enum {
    LM_STATE_IDLE = 0,                /**< No active processing */
    LM_STATE_COMMANDING,              /**< Sending motor commands */
    LM_STATE_MONITORING,              /**< Monitoring execution */
    LM_STATE_CORRECTING,              /**< Applying corrections */
    LM_STATE_ERROR,                   /**< Error state */
    LM_STATE_COUNT
} lm_bridge_state_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Configuration for Language-Motor bridge
 */
typedef struct {
    /* Operating parameters */
    uint32_t update_interval_ms;          /**< Update cycle interval */
    uint32_t max_articulators;            /**< Maximum tracked articulators */
    uint32_t max_command_queue;           /**< Command queue size */

    /* Timing control */
    float feedback_window_ms;             /**< Feedback integration window */
    float timing_tolerance_ms;            /**< Acceptable timing error */
    float position_tolerance;             /**< Acceptable position error [0-1] */

    /* Features */
    bool enable_proprioceptive_feedback;  /**< Enable proprioception */
    bool enable_coarticulation;           /**< Enable coarticulation planning */
    bool enable_timing_correction;        /**< Enable timing adjustments */
    bool enable_error_correction;         /**< Enable error-based corrections */
    bool enable_predictive_control;       /**< Enable feedforward control */

    /* Bio-async */
    bool enable_bio_async;                /**< Enable bio-async messaging */
} language_motor_config_t;

/**
 * @brief Articulator state
 */
typedef struct {
    articulator_type_t type;              /**< Articulator type */
    float position;                       /**< Current position [0-1] */
    float velocity;                       /**< Current velocity */
    float target_position;                /**< Target position [0-1] */
    float target_velocity;                /**< Target velocity */
    float force;                          /**< Applied force [0-1] */
    bool is_active;                       /**< Currently controlled */
    uint64_t last_update_ms;              /**< Last update timestamp */
} articulator_state_t;

/**
 * @brief Articulator command from Broca to Motor
 */
typedef struct {
    uint32_t command_id;                  /**< Command identifier */
    articulator_type_t articulator;       /**< Target articulator */
    articulator_command_type_t type;      /**< Command type */
    float target_value;                   /**< Target value [0-1] */
    float duration_ms;                    /**< Movement duration */
    uint64_t execution_time_ms;           /**< When to execute */
    uint8_t phoneme_id;                   /**< Associated phoneme */
    float priority;                       /**< Command priority [0-1] */
} articulator_command_t;

/**
 * @brief Proprioceptive feedback from Motor to Broca
 */
typedef struct {
    articulator_type_t articulator;       /**< Source articulator */
    motor_feedback_type_t feedback_type;  /**< Feedback type */
    float current_position;               /**< Current position [0-1] */
    float current_velocity;               /**< Current velocity */
    float position_error;                 /**< Error from target */
    float timing_error_ms;                /**< Timing error (ms) */
    uint64_t timestamp_ms;                /**< Feedback timestamp */
    uint8_t phoneme_id;                   /**< Associated phoneme */
} proprioceptive_feedback_t;

/**
 * @brief Phoneme motor program
 */
typedef struct {
    uint8_t phoneme_id;                   /**< Phoneme identifier */
    uint32_t num_commands;                /**< Number of commands */
    articulator_command_t commands[8];    /**< Articulator commands */
    float total_duration_ms;              /**< Total execution time */
    bool requires_voicing;                /**< Requires laryngeal voicing */
    float onset_time_ms;                  /**< Onset relative to start */
} phoneme_motor_program_t;

/**
 * @brief Speech production request
 */
typedef struct {
    uint32_t request_id;                  /**< Request identifier */
    uint8_t* phoneme_sequence;            /**< Phonemes to produce */
    uint32_t phoneme_count;               /**< Number of phonemes */
    float speech_rate;                    /**< Rate multiplier (1.0 = normal) */
    float volume;                         /**< Volume [0-1] */
    float pitch_base_hz;                  /**< Base pitch frequency */
    bool allow_coarticulation;            /**< Enable coarticulation */
} speech_production_request_t;

/**
 * @brief Speech production status
 */
typedef struct {
    uint32_t request_id;                  /**< Associated request */
    speech_execution_state_t state;       /**< Current state */
    uint32_t phonemes_completed;          /**< Phonemes produced */
    uint32_t phonemes_total;              /**< Total phonemes */
    uint8_t current_phoneme;              /**< Currently producing */
    float progress;                       /**< Overall progress [0-1] */
    float timing_accuracy;                /**< Timing accuracy [0-1] */
    float position_accuracy;              /**< Position accuracy [0-1] */
    bool has_errors;                      /**< Errors encountered */
} speech_production_status_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Counts */
    uint64_t commands_sent;               /**< Commands sent to motor */
    uint64_t feedback_received;           /**< Feedback messages received */
    uint64_t phonemes_executed;           /**< Phonemes executed */
    uint64_t corrections_applied;         /**< Error corrections applied */
    uint64_t timing_adjustments;          /**< Timing adjustments made */

    /* Timing */
    float avg_command_latency_ms;         /**< Average command latency */
    float avg_feedback_latency_ms;        /**< Average feedback latency */
    float avg_timing_error_ms;            /**< Average timing error */

    /* Quality */
    float avg_position_accuracy;          /**< Average position accuracy */
    float avg_timing_accuracy;            /**< Average timing accuracy */

    /* Current state */
    speech_execution_state_t exec_state;  /**< Current execution state */
    lm_bridge_state_t bridge_state;       /**< Current bridge state */
} language_motor_stats_t;

//=============================================================================
// Callback Types
//=============================================================================

typedef void (*lm_command_callback_t)(const articulator_command_t* cmd, void* user_data);
typedef void (*lm_feedback_callback_t)(const proprioceptive_feedback_t* feedback, void* user_data);
typedef void (*lm_status_callback_t)(const speech_production_status_t* status, void* user_data);

//=============================================================================
// Lifecycle Functions
//=============================================================================

language_motor_config_t language_motor_default_config(void);

language_motor_bridge_t* language_motor_bridge_create(
    language_orchestrator_t* language,
    motor_adapter_t* motor,
    const language_motor_config_t* config
);

void language_motor_bridge_destroy(language_motor_bridge_t* bridge);

int language_motor_bridge_reset(language_motor_bridge_t* bridge);

//=============================================================================
// Connection Functions
//=============================================================================

int language_motor_connect_broca(
    language_motor_bridge_t* bridge,
    broca_adapter_t* broca
);

int language_motor_connect_bio_async(
    language_motor_bridge_t* bridge,
    bio_router_t router
);

//=============================================================================
// Update Functions
//=============================================================================

int language_motor_bridge_update(
    language_motor_bridge_t* bridge,
    uint64_t timestamp_ms
);

//=============================================================================
// Command Functions (Broca -> Motor)
//=============================================================================

int language_motor_send_command(
    language_motor_bridge_t* bridge,
    const articulator_command_t* command
);

int language_motor_send_phoneme_program(
    language_motor_bridge_t* bridge,
    const phoneme_motor_program_t* program
);

int language_motor_request_speech_production(
    language_motor_bridge_t* bridge,
    const speech_production_request_t* request
);

int language_motor_pause_production(language_motor_bridge_t* bridge);

int language_motor_resume_production(language_motor_bridge_t* bridge);

int language_motor_stop_production(language_motor_bridge_t* bridge);

//=============================================================================
// Feedback Functions (Motor -> Broca)
//=============================================================================

int language_motor_receive_feedback(
    language_motor_bridge_t* bridge,
    const proprioceptive_feedback_t* feedback
);

int language_motor_get_articulator_state(
    const language_motor_bridge_t* bridge,
    articulator_type_t articulator,
    articulator_state_t* state
);

int language_motor_get_all_articulator_states(
    const language_motor_bridge_t* bridge,
    articulator_state_t* states,
    uint32_t max_states
);

//=============================================================================
// Production Status
//=============================================================================

int language_motor_get_production_status(
    const language_motor_bridge_t* bridge,
    speech_production_status_t* status
);

speech_execution_state_t language_motor_get_execution_state(
    const language_motor_bridge_t* bridge
);

//=============================================================================
// Motor Program Management
//=============================================================================

int language_motor_register_phoneme_program(
    language_motor_bridge_t* bridge,
    const phoneme_motor_program_t* program
);

int language_motor_get_phoneme_program(
    const language_motor_bridge_t* bridge,
    uint8_t phoneme_id,
    phoneme_motor_program_t* program
);

bool language_motor_has_phoneme_program(
    const language_motor_bridge_t* bridge,
    uint8_t phoneme_id
);

//=============================================================================
// Timing Control
//=============================================================================

int language_motor_set_speech_rate(
    language_motor_bridge_t* bridge,
    float rate_multiplier
);

float language_motor_get_speech_rate(const language_motor_bridge_t* bridge);

int language_motor_apply_timing_correction(
    language_motor_bridge_t* bridge,
    float correction_ms
);

//=============================================================================
// Callback Registration
//=============================================================================

int language_motor_set_command_callback(
    language_motor_bridge_t* bridge,
    lm_command_callback_t callback,
    void* user_data
);

int language_motor_set_feedback_callback(
    language_motor_bridge_t* bridge,
    lm_feedback_callback_t callback,
    void* user_data
);

int language_motor_set_status_callback(
    language_motor_bridge_t* bridge,
    lm_status_callback_t callback,
    void* user_data
);

//=============================================================================
// Status and Statistics
//=============================================================================

lm_bridge_state_t language_motor_get_state(
    const language_motor_bridge_t* bridge
);

int language_motor_get_stats(
    const language_motor_bridge_t* bridge,
    language_motor_stats_t* stats
);

void language_motor_reset_stats(language_motor_bridge_t* bridge);

int language_motor_get_config(
    const language_motor_bridge_t* bridge,
    language_motor_config_t* config
);

int language_motor_set_config(
    language_motor_bridge_t* bridge,
    const language_motor_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_MOTOR_BRIDGE_H */
