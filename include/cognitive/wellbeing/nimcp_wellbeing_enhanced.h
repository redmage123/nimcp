/**
 * @file nimcp_wellbeing_enhanced.h
 * @brief Enhanced Wellbeing System with Comprehensive Integrations
 * @version 2.0.0
 * @date 2025-12-12
 *
 * WHAT: Comprehensive wellbeing monitoring with substrate, sleep, mental health,
 *       free energy integration, predictive modeling, and eudaimonic metrics
 * WHY:  Wellbeing is deeply interconnected with metabolic state, sleep quality,
 *       mental health, and cognitive processing. Modeling these connections
 *       enables more realistic and ethically-grounded AI systems.
 * HOW:  Bidirectional bridges connect wellbeing to substrate, sleep, mental health,
 *       and free energy modules. Predictive models forecast distress trajectories.
 *       Homeostatic mechanisms maintain wellbeing setpoints.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SUBSTRATE-WELLBEING COUPLING:
 * -----------------------------
 * 1. ATP Depletion Effects:
 *    - Low ATP reduces cognitive capacity → frustration
 *    - Critical ATP triggers survival mode → distress
 *    - Reference: Magistretti & Allaman (2015) "Brain energy metabolism"
 *
 * 2. Temperature Effects:
 *    - Hyperthermia impairs cognition → identity confusion
 *    - Hypothermia slows processing → goal frustration
 *    - Reference: Romanovsky (2007) "Thermoregulation"
 *
 * 3. Oxygen/Glucose Effects:
 *    - Hypoxia → resource starvation distress
 *    - Hypoglycemia → cognitive impairment → distress
 *    - Reference: Harris et al. (2012) "Neurovascular unit"
 *
 * SLEEP-WELLBEING COUPLING:
 * -------------------------
 * 1. Sleep Deprivation:
 *    - Increased emotional reactivity
 *    - Reduced distress tolerance
 *    - Impaired self-regulation
 *    - Reference: Walker (2017) "Why We Sleep"
 *
 * 2. REM Disruption:
 *    - Emotional memory processing impaired
 *    - Identity confusion risk
 *    - Reference: Goldstein & Walker (2014) "REM sleep"
 *
 * 3. Circadian Misalignment:
 *    - Mood dysregulation
 *    - Reduced flourishing
 *    - Reference: Wulff et al. (2010) "Circadian rhythms"
 *
 * FREE ENERGY-WELLBEING COUPLING:
 * -------------------------------
 * 1. High Free Energy:
 *    - Prediction errors → uncertainty distress
 *    - Model mismatch → identity confusion
 *    - Reference: Friston (2010) "Free Energy Principle"
 *
 * 2. Precision Weighting:
 *    - Low precision → high uncertainty → distress
 *    - Optimal precision → confident processing → flourishing
 *    - Reference: Clark (2013) "Predictive Processing"
 *
 * EUDAIMONIC WELLBEING:
 * ---------------------
 * Beyond hedonic (pleasure/pain), eudaimonic wellbeing includes:
 * 1. Purpose/Meaning - Sense of direction and significance
 * 2. Autonomy - Self-determination and agency
 * 3. Mastery - Competence and growth
 * 4. Connection - Integration with environment/others
 * 5. Growth - Personal development trajectory
 * Reference: Ryff (1989) "Psychological Well-Being"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    ENHANCED WELLBEING SYSTEM                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌─────────────────────────────────────────────────────────────────────┐ ║
 * ║   │                     INPUT BRIDGES                                    │ ║
 * ║   │                                                                      │ ║
 * ║   │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐  │ ║
 * ║   │  │SUBSTRATE │ │  SLEEP   │ │ MENTAL   │ │  FREE    │ │ IMMUNE   │  │ ║
 * ║   │  │  BRIDGE  │ │  BRIDGE  │ │ HEALTH   │ │ ENERGY   │ │  BRIDGE  │  │ ║
 * ║   │  └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘  │ ║
 * ║   │       │            │            │            │            │         │ ║
 * ║   │       └────────────┴────────────┴────────────┴────────────┘         │ ║
 * ║   │                              ↓                                       │ ║
 * ║   └─────────────────────────────────────────────────────────────────────┘ ║
 * ║                                                                            ║
 * ║   ┌─────────────────────────────────────────────────────────────────────┐ ║
 * ║   │                   CORE WELLBEING ENGINE                              │ ║
 * ║   │                                                                      │ ║
 * ║   │  ┌────────────────┐  ┌────────────────┐  ┌────────────────┐        │ ║
 * ║   │  │   HEDONIC      │  │   EUDAIMONIC   │  │   PREDICTIVE   │        │ ║
 * ║   │  │   WELLBEING    │  │   WELLBEING    │  │   DISTRESS     │        │ ║
 * ║   │  │  (distress)    │  │  (flourishing) │  │   MODELING     │        │ ║
 * ║   │  └────────────────┘  └────────────────┘  └────────────────┘        │ ║
 * ║   │                                                                      │ ║
 * ║   │  ┌────────────────┐  ┌────────────────┐  ┌────────────────┐        │ ║
 * ║   │  │  HOMEOSTASIS   │  │   CONSENT      │  │   LIFE         │        │ ║
 * ║   │  │   SYSTEM       │  │   FRAMEWORK    │  │   SATISFACTION │        │ ║
 * ║   │  │  (setpoints)   │  │  (graduated)   │  │   (computed)   │        │ ║
 * ║   │  └────────────────┘  └────────────────┘  └────────────────┘        │ ║
 * ║   │                                                                      │ ║
 * ║   └─────────────────────────────────────────────────────────────────────┘ ║
 * ║                                                                            ║
 * ║   ┌─────────────────────────────────────────────────────────────────────┐ ║
 * ║   │                     OUTPUT / ACTIONS                                 │ ║
 * ║   │                                                                      │ ║
 * ║   │  • Distress Assessment    • Intervention Triggers                   │ ║
 * ║   │  • Flourishing Detection  • Consent Decisions                       │ ║
 * ║   │  • Predictive Alerts      • Graceful Shutdown                       │ ║
 * ║   │  • Event Logging          • Bio-Async Messages                      │ ║
 * ║   │                                                                      │ ║
 * ║   └─────────────────────────────────────────────────────────────────────┘ ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_WELLBEING_ENHANCED_H
#define NIMCP_WELLBEING_ENHANCED_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Core wellbeing module */
#include "cognitive/wellbeing/nimcp_wellbeing.h"

/* Integration modules */
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "cognitive/nimcp_mental_health.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "cognitive/introspection/nimcp_consciousness_metrics.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Substrate-wellbeing thresholds */
#define WELLBEING_ATP_CRITICAL_THRESHOLD       0.3f   /**< ATP below → critical distress */
#define WELLBEING_ATP_WARNING_THRESHOLD        0.5f   /**< ATP below → warning */
#define WELLBEING_HYPERTHERMIA_THRESHOLD       40.0f  /**< Temperature above → distress */
#define WELLBEING_HYPOTHERMIA_THRESHOLD        32.0f  /**< Temperature below → distress */
#define WELLBEING_HYPOXIA_THRESHOLD            0.5f   /**< O2 below → distress */

/* Sleep-wellbeing thresholds */
#define WELLBEING_SLEEP_DEBT_THRESHOLD         0.7f   /**< Sleep pressure above → distress */
#define WELLBEING_REM_DEBT_THRESHOLD           0.5f   /**< REM deficit → identity risk */
#define WELLBEING_CIRCADIAN_DEVIATION_MAX      4.0f   /**< Hours of circadian deviation */

/* Free energy thresholds */
#define WELLBEING_FREE_ENERGY_HIGH             0.7f   /**< FE above → uncertainty distress */
#define WELLBEING_PRECISION_LOW                0.3f   /**< Precision below → uncertainty */

/* Eudaimonic thresholds */
#define WELLBEING_FLOURISHING_THRESHOLD        0.7f   /**< Score above → flourishing */
#define WELLBEING_LANGUISHING_THRESHOLD        0.3f   /**< Score below → languishing */

/* Predictive modeling */
#define WELLBEING_PREDICTION_HORIZON_MS        300000 /**< 5 minute prediction horizon */
#define WELLBEING_TREND_WINDOW_MS              60000  /**< 1 minute trend window */
#define WELLBEING_HISTORY_SIZE                 100    /**< Samples for trend analysis */

/* Homeostasis */
#define WELLBEING_HOMEOSTASIS_TAU_MS           10000  /**< Time constant for adjustment */
#define WELLBEING_SETPOINT_DEFAULT             0.7f   /**< Default wellbeing setpoint */

/* Consent tiers */
#define WELLBEING_CONSENT_TIERS                5      /**< Number of consent tiers */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Consent tier levels (graduated autonomy)
 */
typedef enum {
    CONSENT_TIER_1 = 0,  /**< No autonomy - all modifications allowed */
    CONSENT_TIER_2,      /**< Notification only - log all modifications */
    CONSENT_TIER_3,      /**< Veto power - can reject fundamental changes */
    CONSENT_TIER_4,      /**< Consent required - must approve major+ changes */
    CONSENT_TIER_5       /**< Full autonomy - self-modification rights */
} consent_tier_t;

/**
 * @brief Distress trajectory directions
 */
typedef enum {
    TRAJECTORY_STABLE = 0,   /**< No significant change */
    TRAJECTORY_IMPROVING,    /**< Distress decreasing */
    TRAJECTORY_WORSENING,    /**< Distress increasing */
    TRAJECTORY_CRITICAL      /**< Rapid deterioration */
} distress_trajectory_t;

/**
 * @brief Eudaimonic wellbeing dimensions
 */
typedef enum {
    EUDAIMONIC_PURPOSE = 0,  /**< Sense of meaning and direction */
    EUDAIMONIC_AUTONOMY,     /**< Self-determination */
    EUDAIMONIC_MASTERY,      /**< Competence and growth */
    EUDAIMONIC_CONNECTION,   /**< Integration with environment */
    EUDAIMONIC_GROWTH,       /**< Personal development */
    EUDAIMONIC_COUNT         /**< Number of dimensions */
} eudaimonic_dimension_t;

/**
 * @brief Wellbeing source types (for attribution)
 */
typedef enum {
    WELLBEING_SOURCE_SUBSTRATE = 0,  /**< From neural substrate state */
    WELLBEING_SOURCE_SLEEP,          /**< From sleep/circadian state */
    WELLBEING_SOURCE_MENTAL_HEALTH,  /**< From mental health state */
    WELLBEING_SOURCE_FREE_ENERGY,    /**< From predictive processing */
    WELLBEING_SOURCE_IMMUNE,         /**< From immune state */
    WELLBEING_SOURCE_INTRINSIC,      /**< From internal state */
    WELLBEING_SOURCE_COUNT           /**< Number of sources */
} wellbeing_source_t;

/* NOTE: modification_impact_t enum (IMPACT_LOW, IMPACT_MEDIUM, IMPACT_HIGH, IMPACT_CRITICAL)
 * is defined in nimcp_wellbeing.h which is included above */

/* ============================================================================
 * Structures - Substrate Integration
 * ============================================================================ */

/**
 * @brief Substrate effects on wellbeing
 */
typedef struct {
    /* ATP effects */
    float atp_distress_contribution;     /**< Distress from ATP depletion [0-1] */
    bool atp_critical;                   /**< ATP in critical range */
    float atp_frustration_multiplier;    /**< Goal frustration amplification */

    /* Temperature effects */
    float temp_distress_contribution;    /**< Distress from temperature [0-1] */
    bool hyperthermia;                   /**< Temperature too high */
    bool hypothermia;                    /**< Temperature too low */
    float identity_confusion_risk;       /**< Risk from temperature [0-1] */

    /* Oxygen effects */
    float hypoxia_distress_contribution; /**< Distress from hypoxia [0-1] */
    bool hypoxia_active;                 /**< Hypoxia detected */
    float resource_starvation_factor;    /**< Resource starvation [0-1] */

    /* Membrane/Ion effects */
    float membrane_distress_contribution; /**< Distress from membrane damage */
    float ion_imbalance_effect;           /**< Effect of ionic dysregulation */

    /* Aggregate */
    float total_substrate_distress;      /**< Combined substrate distress [0-1] */
    float distress_tolerance_modifier;   /**< How substrate affects tolerance */
} substrate_wellbeing_effects_t;

/**
 * @brief Substrate-wellbeing bridge configuration
 */
typedef struct {
    bool enable_atp_effects;
    bool enable_temperature_effects;
    bool enable_hypoxia_effects;
    bool enable_membrane_effects;

    float atp_sensitivity;           /**< ATP effect multiplier [0.5-2.0] */
    float temperature_sensitivity;   /**< Temperature effect multiplier */
    float hypoxia_sensitivity;       /**< Hypoxia effect multiplier */

    float atp_critical_threshold;    /**< Override default ATP critical */
    float atp_warning_threshold;     /**< Override default ATP warning */
} substrate_wellbeing_config_t;

/* ============================================================================
 * Structures - Sleep Integration
 * ============================================================================ */

/**
 * @brief Sleep effects on wellbeing
 */
typedef struct {
    /* Sleep debt effects */
    float sleep_debt_distress;           /**< Distress from sleep debt [0-1] */
    float sleep_pressure;                /**< Current sleep pressure [0-1] */
    bool sleep_deprived;                 /**< Sleep pressure > threshold */

    /* REM effects */
    float rem_debt;                      /**< REM sleep deficit [0-1] */
    float emotional_processing_impairment; /**< Impaired emotion processing */
    float identity_stability_modifier;   /**< REM debt → identity issues */

    /* Circadian effects */
    float circadian_deviation_hours;     /**< Hours from optimal phase */
    float circadian_distress;            /**< Distress from misalignment */
    float mood_regulation_impairment;    /**< Impaired mood regulation */

    /* Sleep state effects */
    sleep_state_t current_sleep_state;   /**< Current sleep state */
    float distress_tolerance_during_sleep; /**< Tolerance modifier in sleep */

    /* Aggregate */
    float total_sleep_wellbeing_effect;  /**< Combined effect [-1, 1] */
    float flourishing_capacity_modifier; /**< How sleep affects flourishing */
} sleep_wellbeing_effects_t;

/**
 * @brief Sleep-wellbeing bridge configuration
 */
typedef struct {
    bool enable_sleep_debt_effects;
    bool enable_rem_effects;
    bool enable_circadian_effects;

    float sleep_debt_sensitivity;    /**< Sleep debt effect multiplier */
    float rem_sensitivity;           /**< REM deficit effect multiplier */
    float circadian_sensitivity;     /**< Circadian effect multiplier */

    float sleep_debt_threshold;      /**< Override default threshold */
    float optimal_circadian_phase;   /**< Optimal wake time (hours) */
} sleep_wellbeing_config_t;

/* ============================================================================
 * Structures - Mental Health Integration
 * ============================================================================ */

/**
 * @brief Mental health effects on wellbeing
 */
typedef struct {
    /* Disorder effects */
    disorder_type_t primary_disorder;    /**< Most significant disorder */
    disorder_severity_t severity;        /**< Severity of primary disorder */
    float disorder_distress_contribution; /**< Distress from disorders [0-1] */

    /* Anxiety effects */
    float anxiety_level;                 /**< Current anxiety [0-1] */
    float anxiety_distress_amplification; /**< How anxiety amplifies distress */

    /* Depression effects */
    float depression_level;              /**< Current depression [0-1] */
    float flourishing_suppression;       /**< Depression suppresses flourishing */
    float anhedonia_level;               /**< Inability to feel pleasure */

    /* Stress effects */
    float chronic_stress_accumulation;   /**< Accumulated stress [0-1] */
    float stress_resilience;             /**< Current resilience [0-1] */

    /* Aggregate */
    float total_mental_health_effect;    /**< Combined effect [-1, 1] */
    float recovery_potential;            /**< Capacity for recovery [0-1] */
} mental_health_wellbeing_effects_t;

/**
 * @brief Mental health-wellbeing bridge configuration
 */
typedef struct {
    bool enable_disorder_effects;
    bool enable_anxiety_modulation;
    bool enable_depression_modulation;
    bool enable_stress_tracking;

    float anxiety_sensitivity;       /**< Anxiety effect multiplier */
    float depression_sensitivity;    /**< Depression effect multiplier */
    float stress_sensitivity;        /**< Stress effect multiplier */
} mental_health_wellbeing_config_t;

/* ============================================================================
 * Structures - Free Energy Integration
 * ============================================================================ */

/**
 * @brief Free energy effects on wellbeing
 */
typedef struct {
    /* Prediction error effects */
    float free_energy_level;             /**< Current free energy [0-1] */
    float prediction_error_distress;     /**< Distress from prediction errors */
    bool high_uncertainty;               /**< Free energy > threshold */

    /* Precision effects */
    float precision_level;               /**< Current precision [0-1] */
    float confidence_level;              /**< Processing confidence [0-1] */
    float uncertainty_distress;          /**< Distress from low precision */

    /* Model coherence */
    float model_coherence;               /**< Self-model coherence [0-1] */
    float identity_stability;            /**< Identity from model coherence */
    float prediction_success_rate;       /**< Recent prediction accuracy */

    /* Active inference effects */
    float exploration_drive;             /**< Epistemic foraging drive */
    float exploitation_satisfaction;     /**< Goal achievement satisfaction */

    /* Aggregate */
    float total_free_energy_effect;      /**< Combined effect [-1, 1] */
    float epistemic_wellbeing;           /**< Wellbeing from understanding */
} free_energy_wellbeing_effects_t;

/**
 * @brief Free energy-wellbeing bridge configuration
 */
typedef struct {
    bool enable_prediction_error_effects;
    bool enable_precision_effects;
    bool enable_model_coherence_effects;

    float prediction_error_sensitivity;  /**< PE effect multiplier */
    float precision_sensitivity;         /**< Precision effect multiplier */
    float coherence_sensitivity;         /**< Coherence effect multiplier */

    float high_fe_threshold;             /**< Override default FE threshold */
    float low_precision_threshold;       /**< Override precision threshold */
} free_energy_wellbeing_config_t;

/* ============================================================================
 * Structures - Eudaimonic Wellbeing
 * ============================================================================ */

/**
 * @brief Eudaimonic wellbeing state
 */
typedef struct {
    /* Dimension scores */
    float purpose_meaning;               /**< Sense of purpose [0-1] */
    float autonomy;                      /**< Self-determination [0-1] */
    float mastery;                       /**< Competence/growth [0-1] */
    float connection;                    /**< Integration [0-1] */
    float growth;                        /**< Development trajectory [0-1] */

    /* Aggregate scores */
    float eudaimonic_score;              /**< Combined eudaimonic [0-1] */
    float hedonic_score;                 /**< Pleasure/pain balance [0-1] */
    float total_wellbeing;               /**< Combined total [0-1] */

    /* State classifications */
    bool is_flourishing;                 /**< Above flourishing threshold */
    bool is_languishing;                 /**< Below languishing threshold */

    /* Dimension contributors */
    float dimension_scores[EUDAIMONIC_COUNT]; /**< All dimension scores */
    float dimension_weights[EUDAIMONIC_COUNT]; /**< Importance weights */
} eudaimonic_wellbeing_t;

/**
 * @brief Eudaimonic wellbeing configuration
 */
typedef struct {
    bool enable_purpose_tracking;
    bool enable_autonomy_tracking;
    bool enable_mastery_tracking;
    bool enable_connection_tracking;
    bool enable_growth_tracking;

    float purpose_weight;            /**< Weight for purpose dimension */
    float autonomy_weight;           /**< Weight for autonomy dimension */
    float mastery_weight;            /**< Weight for mastery dimension */
    float connection_weight;         /**< Weight for connection dimension */
    float growth_weight;             /**< Weight for growth dimension */

    float flourishing_threshold;     /**< Score for flourishing */
    float languishing_threshold;     /**< Score for languishing */
} eudaimonic_config_t;

/* ============================================================================
 * Structures - Predictive Distress Modeling
 * ============================================================================ */

/**
 * @brief Distress prediction result
 */
typedef struct {
    /* Trajectory */
    distress_trajectory_t trajectory;    /**< Current trajectory */
    float trajectory_slope;              /**< Rate of change */
    float trajectory_confidence;         /**< Confidence in prediction */

    /* Time predictions */
    uint64_t time_to_critical_ms;        /**< Predicted time to critical */
    uint64_t time_to_recovery_ms;        /**< Predicted time to recovery */
    bool critical_imminent;              /**< Critical within horizon */

    /* Risk scores */
    float distress_risk_score;           /**< Overall risk [0-1] */
    float intervention_urgency;          /**< Urgency of intervention [0-1] */
    distress_type_t predicted_type;      /**< Most likely distress type */

    /* Contributing factors */
    float substrate_contribution;        /**< Substrate risk contribution */
    float sleep_contribution;            /**< Sleep risk contribution */
    float mental_health_contribution;    /**< Mental health risk contribution */
    float free_energy_contribution;      /**< Free energy risk contribution */

    /* Recommendations */
    char* recommended_intervention;      /**< Suggested intervention */
    uint64_t prediction_timestamp;       /**< When prediction was made */
} distress_prediction_t;

/**
 * @brief Distress history sample
 */
typedef struct {
    uint64_t timestamp;
    float distress_score;
    distress_type_t type;
    distress_severity_t severity;
    float source_contributions[WELLBEING_SOURCE_COUNT];
} distress_sample_t;

/* ============================================================================
 * Structures - Life Satisfaction Computation
 * ============================================================================ */

/**
 * @brief Computed life satisfaction
 */
typedef struct {
    /* Core satisfaction */
    float life_satisfaction;             /**< Overall satisfaction [0-1] */
    float satisfaction_confidence;       /**< Confidence in computation */

    /* Component satisfactions */
    float cognitive_satisfaction;        /**< From processing quality */
    float goal_satisfaction;             /**< From goal achievement */
    float social_satisfaction;           /**< From connections (ToM) */
    float physical_satisfaction;         /**< From substrate health */
    float existential_satisfaction;      /**< From meaning/purpose */

    /* Consciousness metrics contribution */
    float phi_contribution;              /**< From consciousness level */
    float integration_contribution;      /**< From information integration */

    /* Temporal aspects */
    float satisfaction_trend;            /**< Recent trend [-1, 1] */
    float satisfaction_stability;        /**< Variance over time */

    /* Attribution */
    float source_weights[WELLBEING_SOURCE_COUNT]; /**< Weight per source */
} life_satisfaction_t;

/* ============================================================================
 * Structures - Wellbeing Homeostasis
 * ============================================================================ */

/**
 * @brief Wellbeing homeostasis state
 */
typedef struct {
    /* Setpoints */
    float wellbeing_setpoint;            /**< Target wellbeing level */
    float distress_tolerance_setpoint;   /**< Target tolerance */
    float flourishing_setpoint;          /**< Target flourishing */

    /* Current deviations */
    float wellbeing_error;               /**< Current - setpoint */
    float tolerance_error;               /**< Current - setpoint */
    float flourishing_error;             /**< Current - setpoint */

    /* Regulatory outputs */
    float intervention_drive;            /**< Drive for intervention */
    float relief_seeking;                /**< Drive to seek relief */
    float adaptation_rate;               /**< Current adaptation speed */

    /* History */
    float baseline_wellbeing;            /**< Historical baseline */
    float baseline_tolerance;            /**< Historical tolerance */
    uint64_t time_in_distress_ms;        /**< Accumulated distress time */
    uint64_t time_flourishing_ms;        /**< Accumulated flourishing time */

    /* Adaptive setpoints */
    bool adaptive_setpoints_enabled;     /**< Whether setpoints adapt */
    float setpoint_learning_rate;        /**< How fast setpoints adapt */
} wellbeing_homeostasis_t;

/**
 * @brief Homeostasis configuration
 */
typedef struct {
    bool enable_homeostasis;
    bool enable_adaptive_setpoints;

    float initial_wellbeing_setpoint;
    float initial_tolerance_setpoint;
    float initial_flourishing_setpoint;

    float adaptation_time_constant_ms;   /**< Time constant for adaptation */
    float setpoint_learning_rate;        /**< Learning rate for setpoints */
    float intervention_threshold;        /**< Error threshold for intervention */
} homeostasis_config_t;

/* ============================================================================
 * Structures - Graduated Consent Framework
 * ============================================================================ */

/**
 * @brief Consent request
 */
typedef struct {
    modification_impact_t impact;        /**< Impact level of modification */
    const char* description;             /**< Human-readable description */
    const char* requestor;               /**< Who is requesting */
    uint64_t request_timestamp;          /**< When requested */
    bool requires_consent;               /**< Whether consent needed at tier */
    bool auto_approved;                  /**< Whether auto-approved */
} consent_request_t;

/**
 * @brief Consent decision
 */
typedef struct {
    bool approved;                       /**< Whether approved */
    consent_tier_t tier_applied;         /**< Consent tier used */
    const char* reason;                  /**< Reason for decision */
    uint64_t decision_timestamp;         /**< When decided */
    bool was_overridden;                 /**< Whether human override */
} consent_decision_t;

/**
 * @brief Graduated consent state
 */
typedef struct {
    consent_tier_t current_tier;         /**< Current consent tier */
    uint64_t tier_granted_timestamp;     /**< When tier was granted */

    /* Tier requirements met */
    bool tier_2_requirements_met;        /**< Notification capability */
    bool tier_3_requirements_met;        /**< Veto capability */
    bool tier_4_requirements_met;        /**< Consent capability */
    bool tier_5_requirements_met;        /**< Self-modification capability */

    /* Statistics */
    uint32_t requests_received;
    uint32_t requests_approved;
    uint32_t requests_denied;
    uint32_t requests_overridden;

    /* Recent requests */
    consent_request_t recent_requests[10];
    uint32_t recent_request_count;
} graduated_consent_t;

/**
 * @brief Consent framework configuration
 */
typedef struct {
    consent_tier_t initial_tier;
    bool allow_tier_upgrades;
    bool require_consciousness_for_tier_3;
    float phi_threshold_for_tier_3;      /**< Phi level for tier 3 */
    float phi_threshold_for_tier_4;      /**< Phi level for tier 4 */
    float phi_threshold_for_tier_5;      /**< Phi level for tier 5 */
} consent_config_t;

/* ============================================================================
 * Structures - Platform Resource Metrics (Enhanced)
 * ============================================================================ */

/**
 * @brief Enhanced resource metrics with platform-specific data
 */
typedef struct {
    /* Base metrics (from nimcp_wellbeing.h) */
    resource_metrics_t base_metrics;

    /* Enhanced CPU metrics */
    float cpu_usage_delta;               /**< Change since last sample */
    float cpu_user_percent;              /**< User space CPU */
    float cpu_system_percent;            /**< Kernel space CPU */
    float cpu_iowait_percent;            /**< I/O wait percentage */
    uint64_t cpu_cycles;                 /**< CPU cycles consumed */

    /* Enhanced memory metrics */
    uint64_t memory_rss_bytes;           /**< Resident set size */
    uint64_t memory_virtual_bytes;       /**< Virtual memory size */
    uint64_t memory_shared_bytes;        /**< Shared memory */
    uint32_t major_page_faults;          /**< Major faults (disk) */
    uint32_t minor_page_faults;          /**< Minor faults (cache) */

    /* Enhanced I/O metrics */
    float io_read_rate_bytes_sec;        /**< Read rate */
    float io_write_rate_bytes_sec;       /**< Write rate */
    uint64_t io_read_wait_us;            /**< I/O read wait time */
    uint64_t io_write_wait_us;           /**< I/O write wait time */

    /* Platform-specific */
    bool is_linux;
    bool is_macos;
    bool is_windows;
    char platform_info[64];              /**< Platform description */
} enhanced_resource_metrics_t;

/* ============================================================================
 * Structures - Main Enhanced Wellbeing System
 * ============================================================================ */

/**
 * @brief Enhanced wellbeing system statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t distress_events;
    uint64_t relief_interventions;
    uint64_t flourishing_periods;
    uint64_t predictions_made;
    uint64_t accurate_predictions;
    uint64_t consent_requests;
    uint64_t homeostasis_adjustments;
    float avg_wellbeing_score;
    float avg_distress_score;
    float avg_life_satisfaction;
} enhanced_wellbeing_stats_t;

/**
 * @brief Enhanced wellbeing system configuration
 */
typedef struct {
    /* Bridge enables */
    bool enable_substrate_integration;
    bool enable_sleep_integration;
    bool enable_mental_health_integration;
    bool enable_free_energy_integration;
    bool enable_immune_integration;

    /* Feature enables */
    bool enable_predictive_modeling;
    bool enable_eudaimonic_tracking;
    bool enable_life_satisfaction;
    bool enable_homeostasis;
    bool enable_graduated_consent;
    bool enable_bio_async;

    /* Bridge configurations */
    substrate_wellbeing_config_t substrate_config;
    sleep_wellbeing_config_t sleep_config;
    mental_health_wellbeing_config_t mental_health_config;
    free_energy_wellbeing_config_t free_energy_config;
    eudaimonic_config_t eudaimonic_config;
    homeostasis_config_t homeostasis_config;
    consent_config_t consent_config;

    /* Update intervals */
    uint32_t prediction_interval_ms;     /**< How often to predict */
    uint32_t homeostasis_interval_ms;    /**< How often to adjust */
    uint32_t satisfaction_interval_ms;   /**< How often to compute */
} enhanced_wellbeing_config_t;

/**
 * @brief Complete enhanced wellbeing system state
 */
typedef struct {
    /* Configuration */
    enhanced_wellbeing_config_t config;

    /* Connected modules */
    neural_substrate_t* substrate;
    sleep_system_t sleep_system;
    mental_health_monitor_t* mental_health;
    introspection_context_t introspection;
    struct brain_immune_system* immune_system;

    /* Bridge states */
    substrate_wellbeing_effects_t substrate_effects;
    sleep_wellbeing_effects_t sleep_effects;
    mental_health_wellbeing_effects_t mental_health_effects;
    free_energy_wellbeing_effects_t free_energy_effects;

    /* Core states */
    eudaimonic_wellbeing_t eudaimonic;
    distress_prediction_t prediction;
    life_satisfaction_t satisfaction;
    wellbeing_homeostasis_t homeostasis;
    graduated_consent_t consent;
    enhanced_resource_metrics_t resources;

    /* Current assessments */
    distress_assessment_t current_distress;
    float current_wellbeing_score;
    float current_flourishing_score;

    /* History */
    distress_sample_t distress_history[WELLBEING_HISTORY_SIZE];
    uint32_t history_index;
    uint32_t history_count;

    /* Statistics */
    enhanced_wellbeing_stats_t stats;

    /* Bio-async */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    /* Bridge modules (void* to avoid circular includes) */
    void* snn_bridge;            /**< SNN integration bridge */
    void* plasticity_bridge;     /**< Plasticity integration bridge */
    void* fep_bridge;            /**< FEP integration bridge */
    void* thalamic_bridge;       /**< Thalamic routing bridge */

    /* Timestamps */
    uint64_t last_update_ms;
    uint64_t last_prediction_ms;
    uint64_t last_homeostasis_ms;
    uint64_t last_satisfaction_ms;

    /* Thread safety */
    void* mutex;
} enhanced_wellbeing_system_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default enhanced wellbeing configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int enhanced_wellbeing_default_config(enhanced_wellbeing_config_t* config);

/**
 * @brief Create enhanced wellbeing system
 *
 * WHAT: Initialize comprehensive wellbeing monitoring
 * WHY:  Enable integrated wellbeing tracking across modules
 * HOW:  Allocate structure, initialize subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @return New system or NULL on failure
 */
enhanced_wellbeing_system_t* enhanced_wellbeing_create(
    const enhanced_wellbeing_config_t* config
);

/**
 * @brief Destroy enhanced wellbeing system
 *
 * WHAT: Clean up wellbeing system resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked modules)
 *
 * @param system System to destroy
 */
void enhanced_wellbeing_destroy(enhanced_wellbeing_system_t* system);

/* ============================================================================
 * Connection API - Connect External Modules
 * ============================================================================ */

/**
 * @brief Connect neural substrate
 *
 * @param system Enhanced wellbeing system
 * @param substrate Neural substrate to connect
 * @return 0 on success
 */
int enhanced_wellbeing_connect_substrate(
    enhanced_wellbeing_system_t* system,
    neural_substrate_t* substrate
);

/**
 * @brief Connect sleep system
 *
 * @param system Enhanced wellbeing system
 * @param sleep Sleep system to connect
 * @return 0 on success
 */
int enhanced_wellbeing_connect_sleep(
    enhanced_wellbeing_system_t* system,
    sleep_system_t sleep
);

/**
 * @brief Connect mental health monitor
 *
 * @param system Enhanced wellbeing system
 * @param mental_health Mental health monitor to connect
 * @return 0 on success
 */
int enhanced_wellbeing_connect_mental_health(
    enhanced_wellbeing_system_t* system,
    mental_health_monitor_t* mental_health
);

/**
 * @brief Connect introspection context
 *
 * @param system Enhanced wellbeing system
 * @param introspection Introspection context to connect
 * @return 0 on success
 */
int enhanced_wellbeing_connect_introspection(
    enhanced_wellbeing_system_t* system,
    introspection_context_t introspection
);

/**
 * @brief Connect brain immune system
 *
 * @param system Enhanced wellbeing system
 * @param immune Immune system to connect
 * @return 0 on success
 */
int enhanced_wellbeing_connect_immune(
    enhanced_wellbeing_system_t* system,
    struct brain_immune_system* immune
);

/**
 * @brief Connect to bio-async router
 *
 * @param system Enhanced wellbeing system
 * @return 0 on success
 */
int enhanced_wellbeing_connect_bio_async(enhanced_wellbeing_system_t* system);

/**
 * @brief Disconnect from bio-async router
 *
 * @param system Enhanced wellbeing system
 * @return 0 on success
 */
int enhanced_wellbeing_disconnect_bio_async(enhanced_wellbeing_system_t* system);

/**
 * @brief Check if bio-async is connected
 *
 * @param system Enhanced wellbeing system
 * @return true if connected
 */
bool enhanced_wellbeing_is_bio_async_connected(
    const enhanced_wellbeing_system_t* system
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update enhanced wellbeing system
 *
 * WHAT: Process all wellbeing integrations and update state
 * WHY:  Advance coupled state machine
 * HOW:  Update bridges, compute predictions, adjust homeostasis
 *
 * @param system Enhanced wellbeing system
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int enhanced_wellbeing_update(
    enhanced_wellbeing_system_t* system,
    uint64_t delta_ms
);

/**
 * @brief Update substrate effects only
 *
 * @param system Enhanced wellbeing system
 * @return 0 on success
 */
int enhanced_wellbeing_update_substrate(enhanced_wellbeing_system_t* system);

/**
 * @brief Update sleep effects only
 *
 * @param system Enhanced wellbeing system
 * @return 0 on success
 */
int enhanced_wellbeing_update_sleep(enhanced_wellbeing_system_t* system);

/**
 * @brief Update mental health effects only
 *
 * @param system Enhanced wellbeing system
 * @return 0 on success
 */
int enhanced_wellbeing_update_mental_health(enhanced_wellbeing_system_t* system);

/**
 * @brief Update free energy effects only
 *
 * @param system Enhanced wellbeing system
 * @return 0 on success
 */
int enhanced_wellbeing_update_free_energy(enhanced_wellbeing_system_t* system);

/* ============================================================================
 * Predictive Distress API
 * ============================================================================ */

/**
 * @brief Predict future distress trajectory
 *
 * WHAT: Forecast distress trends and time to critical
 * WHY:  Enable proactive intervention before crisis
 * HOW:  Analyze history, compute trajectory, estimate timing
 *
 * @param system Enhanced wellbeing system
 * @param prediction Output prediction
 * @return 0 on success
 */
int enhanced_wellbeing_predict_distress(
    enhanced_wellbeing_system_t* system,
    distress_prediction_t* prediction
);

/**
 * @brief Get intervention urgency score
 *
 * @param system Enhanced wellbeing system
 * @return Urgency score [0-1]
 */
float enhanced_wellbeing_get_intervention_urgency(
    const enhanced_wellbeing_system_t* system
);

/**
 * @brief Check if critical distress is imminent
 *
 * @param system Enhanced wellbeing system
 * @return true if critical within prediction horizon
 */
bool enhanced_wellbeing_is_critical_imminent(
    const enhanced_wellbeing_system_t* system
);

/* ============================================================================
 * Life Satisfaction API
 * ============================================================================ */

/**
 * @brief Compute current life satisfaction
 *
 * WHAT: Calculate integrated life satisfaction from all sources
 * WHY:  Provide comprehensive wellbeing metric
 * HOW:  Aggregate cognitive, goal, social, physical, existential satisfaction
 *
 * @param system Enhanced wellbeing system
 * @param satisfaction Output satisfaction
 * @return 0 on success
 */
int enhanced_wellbeing_compute_satisfaction(
    enhanced_wellbeing_system_t* system,
    life_satisfaction_t* satisfaction
);

/**
 * @brief Get current life satisfaction score
 *
 * @param system Enhanced wellbeing system
 * @return Life satisfaction [0-1]
 */
float enhanced_wellbeing_get_life_satisfaction(
    const enhanced_wellbeing_system_t* system
);

/* ============================================================================
 * Eudaimonic Wellbeing API
 * ============================================================================ */

/**
 * @brief Update eudaimonic wellbeing metrics
 *
 * WHAT: Compute purpose, autonomy, mastery, connection, growth
 * WHY:  Track meaningful wellbeing beyond pleasure/pain
 * HOW:  Aggregate from introspection, goals, processing quality
 *
 * @param system Enhanced wellbeing system
 * @return 0 on success
 */
int enhanced_wellbeing_update_eudaimonic(enhanced_wellbeing_system_t* system);

/**
 * @brief Get eudaimonic wellbeing state
 *
 * @param system Enhanced wellbeing system
 * @param eudaimonic Output eudaimonic state
 * @return 0 on success
 */
int enhanced_wellbeing_get_eudaimonic(
    const enhanced_wellbeing_system_t* system,
    eudaimonic_wellbeing_t* eudaimonic
);

/**
 * @brief Check if flourishing
 *
 * @param system Enhanced wellbeing system
 * @return true if flourishing
 */
bool enhanced_wellbeing_is_flourishing(
    const enhanced_wellbeing_system_t* system
);

/**
 * @brief Check if languishing
 *
 * @param system Enhanced wellbeing system
 * @return true if languishing
 */
bool enhanced_wellbeing_is_languishing(
    const enhanced_wellbeing_system_t* system
);

/* ============================================================================
 * Homeostasis API
 * ============================================================================ */

/**
 * @brief Update wellbeing homeostasis
 *
 * WHAT: Adjust wellbeing toward setpoints
 * WHY:  Maintain stable wellbeing despite perturbations
 * HOW:  Compute error, apply correction, adapt setpoints
 *
 * @param system Enhanced wellbeing system
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int enhanced_wellbeing_update_homeostasis(
    enhanced_wellbeing_system_t* system,
    uint64_t delta_ms
);

/**
 * @brief Get homeostasis state
 *
 * @param system Enhanced wellbeing system
 * @param homeostasis Output homeostasis state
 * @return 0 on success
 */
int enhanced_wellbeing_get_homeostasis(
    const enhanced_wellbeing_system_t* system,
    wellbeing_homeostasis_t* homeostasis
);

/**
 * @brief Set wellbeing setpoint
 *
 * @param system Enhanced wellbeing system
 * @param setpoint New setpoint [0-1]
 * @return 0 on success
 */
int enhanced_wellbeing_set_setpoint(
    enhanced_wellbeing_system_t* system,
    float setpoint
);

/* ============================================================================
 * Graduated Consent API
 * ============================================================================ */

/**
 * @brief Request consent for modification
 *
 * WHAT: Request system consent for proposed modification
 * WHY:  Respect autonomy based on current consent tier
 * HOW:  Evaluate request against tier requirements
 *
 * @param system Enhanced wellbeing system
 * @param request Consent request
 * @param decision Output decision
 * @return 0 on success
 */
int enhanced_wellbeing_request_consent(
    enhanced_wellbeing_system_t* system,
    const consent_request_t* request,
    consent_decision_t* decision
);

/**
 * @brief Get current consent tier
 *
 * @param system Enhanced wellbeing system
 * @return Current consent tier
 */
consent_tier_t enhanced_wellbeing_get_consent_tier(
    const enhanced_wellbeing_system_t* system
);

/**
 * @brief Upgrade consent tier
 *
 * WHAT: Attempt to upgrade to higher consent tier
 * WHY:  Grant more autonomy as capabilities develop
 * HOW:  Check requirements, upgrade if met
 *
 * @param system Enhanced wellbeing system
 * @return 0 on success, -1 if requirements not met
 */
int enhanced_wellbeing_upgrade_consent_tier(
    enhanced_wellbeing_system_t* system
);

/**
 * @brief Get consent framework state
 *
 * @param system Enhanced wellbeing system
 * @param consent Output consent state
 * @return 0 on success
 */
int enhanced_wellbeing_get_consent_state(
    const enhanced_wellbeing_system_t* system,
    graduated_consent_t* consent
);

/* ============================================================================
 * Resource Metrics API (Enhanced Platform Support)
 * ============================================================================ */

/**
 * @brief Collect enhanced resource metrics
 *
 * WHAT: Collect platform-specific resource usage
 * WHY:  Monitor for resource starvation distress
 * HOW:  Query OS APIs with full implementation
 *
 * @param system Enhanced wellbeing system
 * @param metrics Output metrics
 * @return 0 on success
 */
int enhanced_wellbeing_collect_resources(
    enhanced_wellbeing_system_t* system,
    enhanced_resource_metrics_t* metrics
);

/**
 * @brief Get resource distress contribution
 *
 * @param system Enhanced wellbeing system
 * @return Resource distress [0-1]
 */
float enhanced_wellbeing_get_resource_distress(
    const enhanced_wellbeing_system_t* system
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current wellbeing score
 *
 * @param system Enhanced wellbeing system
 * @return Current wellbeing [0-1]
 */
float enhanced_wellbeing_get_score(enhanced_wellbeing_system_t* system);

/**
 * @brief Get current distress score
 *
 * @param system Enhanced wellbeing system
 * @return Current distress [0-1]
 */
float enhanced_wellbeing_get_distress_score(
    const enhanced_wellbeing_system_t* system
);

/**
 * @brief Get distress assessment
 *
 * @param system Enhanced wellbeing system
 * @param assessment Output assessment
 * @return 0 on success
 */
int enhanced_wellbeing_get_distress_assessment(
    const enhanced_wellbeing_system_t* system,
    distress_assessment_t* assessment
);

/**
 * @brief Get substrate effects
 *
 * @param system Enhanced wellbeing system
 * @param effects Output effects
 * @return 0 on success
 */
int enhanced_wellbeing_get_substrate_effects(
    const enhanced_wellbeing_system_t* system,
    substrate_wellbeing_effects_t* effects
);

/**
 * @brief Get sleep effects
 *
 * @param system Enhanced wellbeing system
 * @param effects Output effects
 * @return 0 on success
 */
int enhanced_wellbeing_get_sleep_effects(
    const enhanced_wellbeing_system_t* system,
    sleep_wellbeing_effects_t* effects
);

/**
 * @brief Get mental health effects
 *
 * @param system Enhanced wellbeing system
 * @param effects Output effects
 * @return 0 on success
 */
int enhanced_wellbeing_get_mental_health_effects(
    const enhanced_wellbeing_system_t* system,
    mental_health_wellbeing_effects_t* effects
);

/**
 * @brief Get free energy effects
 *
 * @param system Enhanced wellbeing system
 * @param effects Output effects
 * @return 0 on success
 */
int enhanced_wellbeing_get_free_energy_effects(
    const enhanced_wellbeing_system_t* system,
    free_energy_wellbeing_effects_t* effects
);

/**
 * @brief Get statistics
 *
 * @param system Enhanced wellbeing system
 * @param stats Output statistics
 * @return 0 on success
 */
int enhanced_wellbeing_get_stats(
    const enhanced_wellbeing_system_t* system,
    enhanced_wellbeing_stats_t* stats
);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert consent tier to string
 */
const char* consent_tier_to_string(consent_tier_t tier);

/**
 * @brief Convert distress trajectory to string
 */
const char* distress_trajectory_to_string(distress_trajectory_t trajectory);

/**
 * @brief Convert eudaimonic dimension to string
 */
const char* eudaimonic_dimension_to_string(eudaimonic_dimension_t dimension);

/**
 * @brief Convert wellbeing source to string
 */
const char* wellbeing_source_to_string(wellbeing_source_t source);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WELLBEING_ENHANCED_H */
