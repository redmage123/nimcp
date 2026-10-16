/**
 * @file nimcp_hypothalamus_amygdala_bridge.h
 * @brief Hypothalamus <-> Amygdala Bridge for Emotion-Drive Integration
 *
 * WHAT: Bidirectional bridge between hypothalamus drives and amygdala emotions
 * WHY:  Drives modulate fear sensitivity; fear/threat boosts safety drive
 * HOW:  Maps drive states to emotional modulation and vice versa
 *
 * BYRNES MODEL CONTEXT:
 * The steering subsystem (hypothalamus) must integrate with emotional
 * processing (amygdala) for proper fear/threat responses. The amygdala
 * outputs to the hypothalamus for autonomic fear responses (CeA->hypothalamus),
 * while the hypothalamus provides state context that modulates fear sensitivity.
 *
 * HYPOTHALAMUS -> AMYGDALA:
 * - Drive urgency -> threat sensitivity (stressed/depleted = hypervigilant)
 * - Arousal level -> fear response scaling
 * - Stress hormones (CRH/cortisol) -> amygdala sensitization
 * - Safety satisfaction -> fear dampening
 *
 * AMYGDALA -> HYPOTHALAMUS:
 * - Fear level -> safety drive boost (threat = prioritize safety)
 * - Anxiety level -> chronic stress signaling (PVN activation)
 * - Threat detection -> acute stress response
 * - Emotional valence -> drive modulation (fear suppresses feeding)
 *
 * BIO-ASYNC MESSAGES:
 * - Sends: BIO_MSG_AMYGDALA_STRESS_MODULATION, BIO_MSG_AMYGDALA_DRIVE_CONTEXT
 * - Receives: BIO_MSG_AMYGDALA_FEAR_LEVEL, BIO_MSG_AMYGDALA_THREAT_DETECTED
 *
 * @version Phase 12: Cognitive Layer Integration
 * @date 2026-01-04
 */

#ifndef NIMCP_HYPOTHALAMUS_AMYGDALA_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_AMYGDALA_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "core/brain/subcortical/nimcp_amygdala.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/bridge/nimcp_bridge_base.h"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Fear-to-safety drive scale */
#define HYPO_AMYG_FEAR_SAFETY_SCALE      0.6f

/** Stress modulation scale */
#define HYPO_AMYG_STRESS_SCALE           0.4f

/** Anxiety threshold for chronic stress */
#define HYPO_AMYG_ANXIETY_THRESHOLD      0.5f

/** Threat response boost factor */
#define HYPO_AMYG_THREAT_BOOST           1.5f

/** Fear-feeding inhibition weight */
#define HYPO_AMYG_FEAR_FEEDING_INHIBIT   0.3f

/*=============================================================================
 * STRESS MODULATION TYPES
 *===========================================================================*/

/**
 * @brief Stress modulation signal to amygdala
 *
 * Hypothalamus sends stress context to modulate amygdala sensitivity
 */
typedef struct {
    float cortisol_level;                  /**< Current cortisol [0, 1] */
    float crh_level;                       /**< CRH (corticotropin-releasing hormone) [0, 1] */
    float norepinephrine_level;            /**< NE from LC [0, 1] */
    float arousal_level;                   /**< Current arousal [0, 1] */
    float stress_chronicity;               /**< Chronic vs acute stress [0=acute, 1=chronic] */
    hypo_drive_type_t primary_stressor;    /**< Drive causing most stress */
    uint64_t timestamp_us;
} hypo_amyg_stress_modulation_t;

/**
 * @brief Drive context signal to amygdala
 *
 * Current drive states affect fear sensitivity
 */
typedef struct {
    float total_drive_urgency;             /**< Sum of all drive urgencies */
    float safety_drive_level;              /**< Current safety drive [0, 1] */
    float social_drive_level;              /**< Current social drive [0, 1] */
    float physiological_stress;            /**< Stress from physiological drives */
    float psychological_stress;            /**< Stress from psychological drives */
    bool any_drive_critical;               /**< Any drive at critical urgency */
    uint64_t timestamp_us;
} hypo_amyg_drive_context_t;

/*=============================================================================
 * AMYGDALA FEEDBACK TYPES
 *===========================================================================*/

/**
 * @brief Fear level feedback from amygdala
 */
typedef struct {
    float fear_level;                      /**< Current fear [0, 1] */
    float anxiety_level;                   /**< Background anxiety [0, 1] */
    amyg_threat_level_t threat_level;      /**< Categorical threat assessment */
    amyg_valence_t emotional_valence;      /**< Current emotional valence */
    bool is_acute_fear;                    /**< Acute vs chronic/anticipatory */
    hypo_drive_type_t fear_relevant_drive; /**< Drive most affected by fear */
    uint64_t timestamp_us;
} hypo_amyg_fear_feedback_t;

/**
 * @brief Threat detection event from amygdala
 */
typedef struct {
    float threat_intensity;                /**< Threat intensity [0, 1] */
    amyg_threat_level_t threat_category;   /**< Threat category */
    uint32_t triggering_memory_id;         /**< Fear memory that triggered (if any) */
    float urgency;                         /**< How urgent the response should be */
    bool requires_freeze;                  /**< Freeze response indicated */
    bool requires_flight;                  /**< Flight response indicated */
    bool requires_fight;                   /**< Fight response indicated */
    uint64_t timestamp_us;
} hypo_amyg_threat_event_t;

/**
 * @brief Fear output signals for hypothalamic response
 */
typedef struct {
    float autonomic_activation;            /**< Autonomic fear output [0, 1] */
    float hormonal_activation;             /**< Hormonal (HPA axis) output [0, 1] */
    float attention_bias;                  /**< Threat attention bias [0, 1] */
    float freezing_signal;                 /**< Freezing behavior signal [0, 1] */
    float startle_potentiation;            /**< Startle reflex potentiation [0, 1] */
    uint64_t timestamp_us;
} hypo_amyg_fear_output_t;

/*=============================================================================
 * BRIDGE CONFIGURATION
 *===========================================================================*/

/**
 * @brief Amygdala bridge configuration
 */
typedef struct {
    /* Stress modulation */
    float stress_scale;                    /**< Stress->amygdala scale */
    float cortisol_sensitivity;            /**< Cortisol effect on fear */
    float crh_sensitivity;                 /**< CRH effect on fear */

    /* Fear-to-drive mapping */
    float fear_safety_scale;               /**< Fear->safety drive scale */
    float anxiety_stress_scale;            /**< Anxiety->chronic stress scale */
    float threat_boost_factor;             /**< Threat detection amplification */

    /* Drive modulation by fear */
    bool enable_fear_drive_modulation;     /**< Fear affects other drives */
    float fear_feeding_inhibition;         /**< Fear suppresses hunger */
    float fear_curiosity_inhibition;       /**< Fear suppresses exploration */
    float fear_social_modulation;          /**< Fear affects social drive (+/-) */

    /* Thresholds */
    float anxiety_chronic_threshold;       /**< Threshold for chronic stress mode */
    float fear_acute_threshold;            /**< Threshold for acute fear response */
    float threat_response_threshold;       /**< Threshold for threat event */

    /* Bio-async */
    bool broadcast_enabled;                /**< Enable bio-async broadcasts */
} hypo_amyg_bridge_config_t;

/**
 * @brief Amygdala bridge context
 */
typedef struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    hypo_amyg_bridge_config_t config;

    /* Connected modules */
    hypo_drive_system_handle_t* drives;    /**< Hypothalamus drives */
    amygdala_t* amygdala;                  /**< Amygdala module (optional) */

    /* Current state - outgoing */
    hypo_amyg_stress_modulation_t current_stress;
    hypo_amyg_drive_context_t current_drive_context;

    /* Current state - incoming */
    hypo_amyg_fear_feedback_t current_fear;
    hypo_amyg_threat_event_t last_threat;
    hypo_amyg_fear_output_t current_fear_output;

    /* Integrated state */
    float integrated_stress_level;         /**< Combined stress [0, 1] */
    float chronic_stress_accumulator;      /**< Chronic stress buildup */
    bool in_threat_response;               /**< Currently responding to threat */

    /* Drive modulation from fear */
    float fear_drive_modulation[HYPO_DRIVE_COUNT]; /**< Fear effects on drives */

    /* Timing */
    uint64_t last_update_us;
    uint64_t last_threat_us;
    uint64_t chronic_stress_start_us;

    /* Bio-async context */
    bio_module_context_t bio_ctx;

    /* Statistics */
    uint64_t stress_signals_sent;
    uint64_t fear_updates_received;
    uint64_t threat_events_processed;
    uint64_t chronic_stress_episodes;
    uint64_t safety_drive_boosts;

} hypo_amyg_bridge_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default amygdala bridge configuration
 *
 * @return Default configuration
 */
hypo_amyg_bridge_config_t hypo_amyg_bridge_default_config(void);

/**
 * @brief Create amygdala bridge
 *
 * @param drives Hypothalamus drive system handle
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge context, or NULL on failure
 */
hypo_amyg_bridge_t* hypo_amyg_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_amyg_bridge_config_t* config);

/**
 * @brief Destroy amygdala bridge
 *
 * @param bridge Bridge to destroy
 */
void hypo_amyg_bridge_destroy(hypo_amyg_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge to reset
 */
void hypo_amyg_bridge_reset(hypo_amyg_bridge_t* bridge);

/*=============================================================================
 * CORE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Update bridge bidirectionally
 *
 * WHAT: Update stress modulation and process fear feedback
 * WHY:  Maintain bidirectional hypothalamus-amygdala integration
 * HOW:  Compute stress signals, apply fear-to-drive modulation
 *
 * @param bridge Bridge context
 * @param dt_ms Time delta in milliseconds
 * @return 0 on success, -1 on error
 */
int hypo_amyg_bridge_update(hypo_amyg_bridge_t* bridge, float dt_ms);

/**
 * @brief Compute stress modulation signal
 *
 * @param bridge Bridge context
 * @return Stress modulation signal for amygdala
 */
hypo_amyg_stress_modulation_t hypo_amyg_bridge_compute_stress(
    hypo_amyg_bridge_t* bridge);

/**
 * @brief Compute drive context signal
 *
 * @param bridge Bridge context
 * @return Drive context for amygdala
 */
hypo_amyg_drive_context_t hypo_amyg_bridge_compute_drive_context(
    hypo_amyg_bridge_t* bridge);

/**
 * @brief Get current stress level
 *
 * @param bridge Bridge context
 * @return Integrated stress level [0, 1]
 */
float hypo_amyg_bridge_get_stress_level(const hypo_amyg_bridge_t* bridge);

/**
 * @brief Check if in chronic stress state
 *
 * @param bridge Bridge context
 * @return true if chronically stressed
 */
bool hypo_amyg_bridge_is_chronic_stress(const hypo_amyg_bridge_t* bridge);

/*=============================================================================
 * FEAR FEEDBACK PROCESSING
 *===========================================================================*/

/**
 * @brief Process fear level update from amygdala
 *
 * WHAT: Handle fear/anxiety feedback from amygdala
 * WHY:  Fear should boost safety drive and affect other drives
 * HOW:  Update safety drive, modulate feeding/exploration
 *
 * @param bridge Bridge context
 * @param fear Fear feedback from amygdala
 * @return Safety drive boost generated
 */
float hypo_amyg_bridge_process_fear(
    hypo_amyg_bridge_t* bridge,
    const hypo_amyg_fear_feedback_t* fear);

/**
 * @brief Process threat detection event
 *
 * WHAT: Handle acute threat detection from amygdala
 * WHY:  Triggers acute stress response in hypothalamus
 * HOW:  Activate PVN for CRH release, boost safety drive
 *
 * @param bridge Bridge context
 * @param threat Threat event from amygdala
 * @return Stress response magnitude
 */
float hypo_amyg_bridge_process_threat(
    hypo_amyg_bridge_t* bridge,
    const hypo_amyg_threat_event_t* threat);

/**
 * @brief Process fear output signals
 *
 * @param bridge Bridge context
 * @param output Fear output from amygdala CeA
 */
void hypo_amyg_bridge_process_fear_output(
    hypo_amyg_bridge_t* bridge,
    const hypo_amyg_fear_output_t* output);

/**
 * @brief Get current fear feedback state
 *
 * @param bridge Bridge context
 * @param fear Output: current fear state
 * @return true if valid
 */
bool hypo_amyg_bridge_get_fear_state(
    const hypo_amyg_bridge_t* bridge,
    hypo_amyg_fear_feedback_t* fear);

/**
 * @brief Get fear-induced drive modulation
 *
 * @param bridge Bridge context
 * @param modulation Output array (size HYPO_DRIVE_COUNT)
 * @return true on success
 */
bool hypo_amyg_bridge_get_fear_drive_modulation(
    const hypo_amyg_bridge_t* bridge,
    float* modulation);

/*=============================================================================
 * AMYGDALA CONNECTION
 *===========================================================================*/

/**
 * @brief Connect to amygdala module directly
 *
 * @param bridge Bridge context
 * @param amygdala Amygdala module handle
 * @return true on success
 */
bool hypo_amyg_bridge_connect(
    hypo_amyg_bridge_t* bridge,
    amygdala_t* amygdala);

/**
 * @brief Send stress modulation to amygdala
 *
 * @param bridge Bridge context
 * @param stress Stress modulation signal
 * @return 0 on success, -1 on error
 */
int hypo_amyg_bridge_send_stress(
    hypo_amyg_bridge_t* bridge,
    const hypo_amyg_stress_modulation_t* stress);

/**
 * @brief Send drive context to amygdala
 *
 * @param bridge Bridge context
 * @param context Drive context signal
 * @return 0 on success, -1 on error
 */
int hypo_amyg_bridge_send_drive_context(
    hypo_amyg_bridge_t* bridge,
    const hypo_amyg_drive_context_t* context);

/**
 * @brief Query fear level from amygdala
 *
 * @param bridge Bridge context
 * @param fear Output: fear feedback
 * @return 0 on success, -1 on error
 */
int hypo_amyg_bridge_query_fear(
    hypo_amyg_bridge_t* bridge,
    hypo_amyg_fear_feedback_t* fear);

/*=============================================================================
 * STRESS RESPONSE
 *===========================================================================*/

/**
 * @brief Trigger acute stress response
 *
 * WHAT: Initiate acute stress response from threat
 * WHY:  Hypothalamus must coordinate acute stress (fight/flight)
 * HOW:  Activate PVN, boost arousal, prioritize safety
 *
 * @param bridge Bridge context
 * @param intensity Stress intensity [0, 1]
 * @return 0 on success, -1 on error
 */
int hypo_amyg_bridge_trigger_acute_stress(
    hypo_amyg_bridge_t* bridge,
    float intensity);

/**
 * @brief Enter chronic stress state
 *
 * @param bridge Bridge context
 */
void hypo_amyg_bridge_enter_chronic_stress(hypo_amyg_bridge_t* bridge);

/**
 * @brief Exit chronic stress state
 *
 * @param bridge Bridge context
 */
void hypo_amyg_bridge_exit_chronic_stress(hypo_amyg_bridge_t* bridge);

/**
 * @brief Get time in threat response
 *
 * @param bridge Bridge context
 * @return Time since threat response started (ms), 0 if not in response
 */
uint64_t hypo_amyg_bridge_get_threat_response_duration(
    const hypo_amyg_bridge_t* bridge);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/**
 * @brief Register with bio-async router
 *
 * @param bridge Bridge context
 * @param use_kg_wiring Use KG-driven wiring (true) or legacy (false)
 * @return true on success
 */
bool hypo_amyg_bridge_register_bio(
    hypo_amyg_bridge_t* bridge,
    bool use_kg_wiring);

/**
 * @brief Process incoming bio-async messages
 *
 * @param bridge Bridge context
 * @param max_messages Maximum messages to process
 * @return Number of messages processed
 */
uint32_t hypo_amyg_bridge_process_bio(
    hypo_amyg_bridge_t* bridge,
    uint32_t max_messages);

/**
 * @brief Broadcast stress modulation
 *
 * @param bridge Bridge context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t hypo_amyg_bridge_broadcast_stress(
    hypo_amyg_bridge_t* bridge);

/**
 * @brief Broadcast drive context
 *
 * @param bridge Bridge context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t hypo_amyg_bridge_broadcast_drive_context(
    hypo_amyg_bridge_t* bridge);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge context
 * @param stress_signals Output: stress signals sent
 * @param fear_updates Output: fear updates received
 * @param threat_events Output: threat events processed
 * @param chronic_episodes Output: chronic stress episodes
 */
void hypo_amyg_bridge_get_stats(
    const hypo_amyg_bridge_t* bridge,
    uint64_t* stress_signals,
    uint64_t* fear_updates,
    uint64_t* threat_events,
    uint64_t* chronic_episodes);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_AMYGDALA_BRIDGE_H */
