/**
 * @file nimcp_neuromodulatory_security_bridge.h
 * @brief Unified Neuromodulatory-Security System Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Bidirectional bridge connecting all neuromodulatory centers (LC, VTA, Raphe, Habenula)
 *       to the security system (BBB, anomaly detection, rate limiting, policy engine).
 *
 * WHY: Neuromodulatory systems influence threat detection and security responses:
 *      - LC (NE): High arousal increases threat sensitivity and vigilance
 *      - VTA (DA): Reward signals modulate security learning and adaptation
 *      - Raphe (5-HT): Mood affects impulse control and patience with false positives
 *      - Habenula: Aversive signals trigger defensive security measures
 *
 * HOW: Each neuromodulatory center modulates security parameters; security events
 *      trigger appropriate neuromodulatory responses (arousal, stress, etc.).
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * NEUROIMMUNE SECURITY MAPPING:
 * The brain's neuromodulatory systems coordinate with immune and defensive systems:
 *
 * 1. Norepinephrine (LC) and Threat Detection:
 *    - Arousal amplifies sensory processing and threat detection
 *    - Phasic NE bursts signal novelty/uncertainty requiring attention
 *    - High tonic NE = hypervigilant state = lower security thresholds
 *    - Reference: Aston-Jones & Cohen, "Adaptive gain and role of LC-NE system"
 *
 * 2. Dopamine (VTA) and Security Learning:
 *    - RPE signals help learn threat patterns
 *    - Positive RPE = expected safety confirmed
 *    - Negative RPE = unexpected threat = increase vigilance
 *    - Reference: Schultz, "Dopamine reward prediction error coding"
 *
 * 3. Serotonin (Raphe) and Patience/Inhibition:
 *    - High 5-HT = patience with ambiguous signals (reduce false positives)
 *    - Low 5-HT = impulsive responses to potential threats
 *    - Modulates rate limiting patience and tolerance windows
 *    - Reference: Cools et al., "Serotonin and behavioral inhibition"
 *
 * 4. Habenula and Aversive Learning:
 *    - Signals punishment and negative outcomes
 *    - Triggers avoidance responses in security
 *    - Inhibits DA/5-HT to promote defensive mode
 *    - Reference: Hikosaka, "Habenula: crossroad between basal ganglia and limbic"
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |          NEUROMODULATORY-SECURITY UNIFIED BRIDGE                          |
 * +===========================================================================+
 * |                                                                           |
 * |   NEUROMODULATORY CENTERS              SECURITY SYSTEMS                   |
 * |   +-------------------+                +-------------------+              |
 * |   | Locus Coeruleus   |<--Arousal---->| BBB Thresholds    |              |
 * |   | (NE: Arousal)     |<--Vigilance-->| Anomaly Sensitivity|             |
 * |   +-------------------+                +-------------------+              |
 * |   +-------------------+                +-------------------+              |
 * |   | VTA               |<--Learning--->| Pattern Database  |              |
 * |   | (DA: Reward)      |<--Adaptation->| Policy Learning   |              |
 * |   +-------------------+                +-------------------+              |
 * |   +-------------------+                +-------------------+              |
 * |   | Raphe Nuclei      |<--Patience--->| Rate Limiter      |              |
 * |   | (5-HT: Mood)      |<--Tolerance-->| False Positive    |              |
 * |   +-------------------+                +-------------------+              |
 * |   +-------------------+                +-------------------+              |
 * |   | Habenula          |<--Aversive--->| Threat Blocking   |              |
 * |   | (Punishment)      |<--Defense---->| Quarantine Mode   |              |
 * |   +-------------------+                +-------------------+              |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROMODULATORY_SECURITY_BRIDGE_H
#define NIMCP_NEUROMODULATORY_SECURITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NEUROMOD_SEC_MAX_EVENT_BUFFER       128
#define NEUROMOD_SEC_DEFAULT_UPDATE_MS      50
#define NEUROMOD_SEC_MAX_SUBSCRIPTIONS      32

/* Magic number for validation */
#define NEUROMOD_SECURITY_BRIDGE_MAGIC      0x4E534252  /* "NSBR" */

/* Modulation strengths (derived from biological literature) */
#define NE_THREAT_SENSITIVITY_BOOST         0.15f   /* +15% detection sensitivity per unit NE */
#define DA_LEARNING_RATE_SCALE              0.20f   /* +20% pattern learning rate per unit RPE */
#define HT_PATIENCE_FACTOR                  0.25f   /* +25% tolerance window per unit 5-HT */
#define HAB_DEFENSIVE_BOOST                 0.30f   /* +30% blocking aggression per unit hab */

/* ============================================================================
 * Neuromodulatory Security Event Types
 * ============================================================================ */

typedef enum {
    /* LC -> Security events */
    NEUROMOD_SEC_EVENT_AROUSAL_BOOST = 0,       /**< Increase threat sensitivity */
    NEUROMOD_SEC_EVENT_VIGILANCE_INCREASE,       /**< Lower detection thresholds */
    NEUROMOD_SEC_EVENT_PHASIC_ALERT,             /**< Urgent attention signal */

    /* VTA -> Security events */
    NEUROMOD_SEC_EVENT_PATTERN_LEARN,            /**< Learn new threat pattern */
    NEUROMOD_SEC_EVENT_ADAPTATION_SIGNAL,        /**< Adapt policy weights */
    NEUROMOD_SEC_EVENT_REWARD_SAFETY,            /**< Safe state confirmed */

    /* Raphe -> Security events */
    NEUROMOD_SEC_EVENT_PATIENCE_INCREASE,        /**< Increase tolerance window */
    NEUROMOD_SEC_EVENT_IMPULSE_CONTROL,          /**< Slow reaction to ambiguous */
    NEUROMOD_SEC_EVENT_FP_REDUCTION,             /**< Reduce false positive rate */

    /* Habenula -> Security events */
    NEUROMOD_SEC_EVENT_AVERSIVE_TRIGGER,         /**< Activate defensive mode */
    NEUROMOD_SEC_EVENT_PUNISHMENT_SIGNAL,        /**< Signal negative outcome */
    NEUROMOD_SEC_EVENT_QUARANTINE_REQUEST,       /**< Request isolation */

    /* Security -> Neuromodulatory events (feedback) */
    NEUROMOD_SEC_EVENT_THREAT_DETECTED,          /**< Triggers LC arousal */
    NEUROMOD_SEC_EVENT_PATTERN_MATCHED,          /**< Triggers VTA RPE */
    NEUROMOD_SEC_EVENT_RATE_LIMIT_HIT,           /**< Affects Raphe patience */
    NEUROMOD_SEC_EVENT_ATTACK_BLOCKED,           /**< Triggers Habenula */

    NEUROMOD_SEC_EVENT_COUNT
} neuromod_sec_event_t;

/* ============================================================================
 * Security Modulation State
 * ============================================================================ */

/**
 * @brief How neuromodulators affect security parameters
 */
typedef struct {
    /* From LC (arousal) */
    float threat_sensitivity_boost;     /**< BBB/anomaly detection boost */
    float vigilance_level;              /**< Overall alertness [0-1] */
    float gain_modulation;              /**< Signal amplification */
    bool phasic_alert_active;           /**< Urgent attention mode */

    /* From VTA (reward/learning) */
    float pattern_learning_rate;        /**< How fast to learn patterns */
    float policy_adaptation_rate;       /**< Policy update speed */
    float last_rpe;                     /**< Last reward prediction error */
    bool safety_confirmed;              /**< Expected safety achieved */

    /* From Raphe (mood/patience) */
    float tolerance_window;             /**< Window for false positive */
    float impulse_inhibition;           /**< How much to delay response */
    float patience_level;               /**< Patience with ambiguity */
    float false_positive_threshold;     /**< FP reduction factor */

    /* From Habenula (aversive) */
    float defensive_boost;              /**< Blocking/quarantine boost */
    float avoidance_drive;              /**< How much to avoid */
    float punishment_signal;            /**< Negative outcome strength */
    bool quarantine_mode;               /**< Isolation requested */

    uint64_t timestamp_us;
} neuromod_security_modulation_t;

/**
 * @brief Security feedback to neuromodulatory systems
 */
typedef struct {
    /* Threat information */
    float current_threat_level;         /**< Overall threat [0-1] */
    uint32_t threats_detected;          /**< Count of active threats */
    uint32_t patterns_matched;          /**< Known threat patterns matched */

    /* Rate limiting state */
    float rate_limit_utilization;       /**< How close to limits [0-1] */
    uint32_t rate_violations;           /**< Count of rate violations */

    /* Defensive state */
    bool under_attack;                  /**< Active attack detected */
    bool quarantine_active;             /**< Currently in quarantine */
    float defensive_mode_level;         /**< Defensive posture [0-1] */

    /* Learning outcomes */
    float pattern_match_rate;           /**< Success rate of pattern DB */
    float false_positive_rate;          /**< Current FP rate */

    uint64_t last_update_us;
} neuromod_security_feedback_t;

/* ============================================================================
 * Event Payloads
 * ============================================================================ */

typedef struct {
    float arousal_level;
    float vigilance;
    float sensitivity_boost;
    bool phasic_burst;
    uint64_t timestamp;
} neuromod_sec_arousal_payload_t;

typedef struct {
    float rpe;
    float learning_rate;
    float adaptation_rate;
    bool positive_outcome;
    uint32_t pattern_id;
    uint64_t timestamp;
} neuromod_sec_learning_payload_t;

typedef struct {
    float patience;
    float impulse_inhibition;
    float tolerance_boost;
    uint64_t timestamp;
} neuromod_sec_patience_payload_t;

typedef struct {
    float punishment_strength;
    float avoidance_drive;
    bool quarantine_request;
    uint32_t threat_id;
    uint64_t timestamp;
} neuromod_sec_aversive_payload_t;

typedef struct {
    float threat_level;
    uint32_t threat_count;
    uint32_t threat_type;
    bool urgent;
    uint64_t timestamp;
} neuromod_sec_threat_payload_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

typedef struct {
    /* Enable flags per center */
    bool enable_lc_security_modulation;
    bool enable_vta_security_modulation;
    bool enable_raphe_security_modulation;
    bool enable_habenula_security_modulation;

    /* Enable security feedback */
    bool enable_threat_feedback;
    bool enable_rate_limit_feedback;
    bool enable_pattern_feedback;

    /* Modulation strengths */
    float ne_sensitivity_weight;        /**< How much NE affects sensitivity */
    float da_learning_weight;           /**< How much DA affects learning */
    float ht_patience_weight;           /**< How much 5-HT affects patience */
    float hab_defensive_weight;         /**< How much hab affects defense */

    /* Timing */
    float update_interval_ms;
    bool broadcast_on_change;

    /* Event buffer */
    uint32_t event_buffer_size;
} neuromod_security_bridge_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    /* Outbound events per center */
    uint32_t lc_modulations_sent;
    uint32_t vta_modulations_sent;
    uint32_t raphe_modulations_sent;
    uint32_t habenula_modulations_sent;

    /* Inbound security feedback */
    uint32_t threat_events_received;
    uint32_t pattern_events_received;
    uint32_t rate_limit_events_received;

    /* Effectiveness */
    uint32_t threats_detected_during_high_arousal;
    uint32_t false_positives_prevented;
    float avg_response_latency_us;

    /* Overall */
    uint32_t total_events_sent;
    uint32_t total_events_received;
    uint64_t last_activity_us;
} neuromod_security_bridge_stats_t;

/* ============================================================================
 * Bridge Handle
 * ============================================================================ */

typedef struct neuromod_security_bridge_struct neuromod_security_bridge_t;

/* Forward declarations for adapters */
#ifndef NIMCP_LC_ADAPTER_H
typedef struct nimcp_lc_adapter_struct* nimcp_lc_adapter_t;
#endif

#ifndef NIMCP_VTA_ADAPTER_H
typedef struct nimcp_vta_adapter_struct* nimcp_vta_adapter_t;
#endif

#ifndef NIMCP_RAPHE_ADAPTER_H
typedef struct nimcp_raphe_adapter_struct* nimcp_raphe_adapter_t;
#endif

#ifndef NIMCP_HABENULA_ADAPTER_H
typedef struct nimcp_habenula_adapter_struct* nimcp_habenula_adapter_t;
#endif

/* Security system handle (opaque) */
typedef struct nimcp_security_context_struct* nimcp_security_context_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/* Lifecycle */
int neuromod_security_bridge_default_config(neuromod_security_bridge_config_t* config);
neuromod_security_bridge_t* neuromod_security_bridge_create(const neuromod_security_bridge_config_t* config);
void neuromod_security_bridge_destroy(neuromod_security_bridge_t* bridge);

/* Connection */
int neuromod_security_bridge_connect_security(neuromod_security_bridge_t* bridge, nimcp_security_context_t security);
int neuromod_security_bridge_disconnect(neuromod_security_bridge_t* bridge);
bool neuromod_security_bridge_is_connected(const neuromod_security_bridge_t* bridge);

/* Adapter registration */
int neuromod_security_bridge_register_lc(neuromod_security_bridge_t* bridge, nimcp_lc_adapter_t adapter);
int neuromod_security_bridge_register_vta(neuromod_security_bridge_t* bridge, nimcp_vta_adapter_t adapter);
int neuromod_security_bridge_register_raphe(neuromod_security_bridge_t* bridge, nimcp_raphe_adapter_t adapter);
int neuromod_security_bridge_register_habenula(neuromod_security_bridge_t* bridge, nimcp_habenula_adapter_t adapter);

/* Update and processing */
int neuromod_security_bridge_update(neuromod_security_bridge_t* bridge, float delta_ms);
int neuromod_security_bridge_process_events(neuromod_security_bridge_t* bridge, uint32_t max_events);

/* Neuromodulatory -> Security modulation */
int neuromod_security_apply_arousal(neuromod_security_bridge_t* bridge, const neuromod_sec_arousal_payload_t* payload);
int neuromod_security_apply_learning(neuromod_security_bridge_t* bridge, const neuromod_sec_learning_payload_t* payload);
int neuromod_security_apply_patience(neuromod_security_bridge_t* bridge, const neuromod_sec_patience_payload_t* payload);
int neuromod_security_apply_aversive(neuromod_security_bridge_t* bridge, const neuromod_sec_aversive_payload_t* payload);

/* Security -> Neuromodulatory feedback */
int neuromod_security_report_threat(neuromod_security_bridge_t* bridge, const neuromod_sec_threat_payload_t* payload);

/* State access */
int neuromod_security_bridge_get_modulation(const neuromod_security_bridge_t* bridge, neuromod_security_modulation_t* modulation);
int neuromod_security_bridge_get_feedback(const neuromod_security_bridge_t* bridge, neuromod_security_feedback_t* feedback);

/* Statistics */
int neuromod_security_bridge_get_stats(const neuromod_security_bridge_t* bridge, neuromod_security_bridge_stats_t* stats);
int neuromod_security_bridge_reset_stats(neuromod_security_bridge_t* bridge);

/* Diagnostics */
const char* neuromod_sec_event_name(neuromod_sec_event_t event);
void neuromod_security_bridge_print_summary(const neuromod_security_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROMODULATORY_SECURITY_BRIDGE_H */
