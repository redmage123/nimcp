/**
 * @file nimcp_hypo_immune_fep_bridge.h
 * @brief Free Energy Principle bridge for Hypothalamus Immune System Integration
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for hypothalamic immune system coordination
 * WHY:  Cytokine levels and immune responses represent high-surprise deviations
 *       from expected homeostatic states in the predictive processing framework
 * HOW:  Map cytokine levels to free energy, immune response to prediction error,
 *       and use precision modulation based on inflammation state
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * HYPOTHALAMIC-IMMUNE AXIS AS PREDICTIVE PROCESSING:
 * --------------------------------------------------
 * The hypothalamus coordinates immune responses through neuroendocrine signaling.
 * Normal immune homeostasis has predictable cytokine patterns - deviations generate:
 *
 * 1. Normal cytokine levels = low free energy (homeostatic equilibrium)
 * 2. Elevated cytokines = high free energy (inflammatory surprise)
 * 3. Immune activation = prediction error (threat detected)
 * 4. Cytokine storms = belief violation (extreme deviation)
 *
 * KEY CYTOKINES TRACKED:
 * - IL-1: Pro-inflammatory, induces fever (hypothalamic thermostat)
 * - IL-6: Acute phase response, fatigue signal
 * - TNF-alpha: Inflammation amplifier
 * - IL-10: Anti-inflammatory, resolution signal
 * - IFN-gamma: Adaptive immunity activation
 *
 * FEP INTEGRATION:
 * ```
 * Cytokine State (c) -> Inflammatory Assessment
 *         |
 * Expected Cytokine Pattern mu (baseline immune state)
 *         |
 * Prediction Error: epsilon = c - g(mu)
 *         |
 * Free Energy F = Complexity + Inaccuracy
 *         |
 * Surprise = -ln p(c) <= F
 *         |
 * Immune Threat Level = F / F_threshold
 * ```
 *
 * FEP MAPPINGS:
 * - Cytokine levels -> Free energy (deviation from baseline)
 * - Immune response -> Prediction error (unexpected activation)
 * - Inflammation -> Precision reduction (uncertainty increases)
 * - Resolution -> Belief update (learning from immune events)
 *
 * DETECTION MAPPING:
 * - Low FE (<2.0)  -> Normal immune function
 * - Medium FE (2-5) -> Immune activation (monitoring)
 * - High FE (5-10)  -> Significant inflammation
 * - Very high FE (>10) -> Cytokine storm / critical state
 *
 * @see nimcp_hypothalamus_drives.h
 * @see nimcp_brain_immune.h
 * @see nimcp_free_energy.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HYPO_IMMUNE_FEP_BRIDGE_H
#define NIMCP_HYPO_IMMUNE_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_time.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bio-async module ID for hypothalamus immune FEP bridge */
#define BIO_MODULE_HYPO_IMMUNE_FEP      0x0B02

/** Free energy thresholds for immune state levels */
#define HYPO_IMMUNE_FEP_NORMAL_THRESHOLD      2.0f   /**< Normal immune function */
#define HYPO_IMMUNE_FEP_ACTIVATION_THRESHOLD  5.0f   /**< Immune activation */
#define HYPO_IMMUNE_FEP_INFLAMMATION_THRESHOLD 10.0f /**< Significant inflammation */
#define HYPO_IMMUNE_FEP_STORM_THRESHOLD       20.0f  /**< Cytokine storm threshold */

/** Precision bounds */
#define HYPO_IMMUNE_FEP_MIN_PRECISION         0.1f   /**< Minimum precision */
#define HYPO_IMMUNE_FEP_MAX_PRECISION         10.0f  /**< Maximum precision */
#define HYPO_IMMUNE_FEP_DEFAULT_PRECISION     1.0f   /**< Default precision */

/** Cytokine baseline levels (normalized 0-1) */
#define HYPO_IMMUNE_FEP_IL1_BASELINE          0.1f   /**< IL-1 baseline */
#define HYPO_IMMUNE_FEP_IL6_BASELINE          0.1f   /**< IL-6 baseline */
#define HYPO_IMMUNE_FEP_TNF_BASELINE          0.05f  /**< TNF-alpha baseline */
#define HYPO_IMMUNE_FEP_IL10_BASELINE         0.2f   /**< IL-10 baseline */
#define HYPO_IMMUNE_FEP_IFN_BASELINE          0.1f   /**< IFN-gamma baseline */

/** Cytokine weights for FE computation */
#define HYPO_IMMUNE_FEP_IL1_WEIGHT            3.0f   /**< IL-1 FE weight */
#define HYPO_IMMUNE_FEP_IL6_WEIGHT            3.5f   /**< IL-6 FE weight */
#define HYPO_IMMUNE_FEP_TNF_WEIGHT            4.0f   /**< TNF-alpha FE weight */
#define HYPO_IMMUNE_FEP_IL10_WEIGHT           -2.0f  /**< IL-10 FE weight (negative = resolution) */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Immune state levels based on FEP metrics
 *
 * WHAT: Categorization of immune system state
 * WHY:  Enable graded response to immune activation
 */
typedef enum {
    HYPO_IMMUNE_FEP_STATE_NORMAL = 0,         /**< Normal immune function */
    HYPO_IMMUNE_FEP_STATE_VIGILANT,           /**< Elevated vigilance */
    HYPO_IMMUNE_FEP_STATE_ACTIVATED,          /**< Active immune response */
    HYPO_IMMUNE_FEP_STATE_INFLAMED,           /**< Significant inflammation */
    HYPO_IMMUNE_FEP_STATE_STORM               /**< Cytokine storm / critical */
} hypo_immune_fep_state_t;

/**
 * @brief Active inference response types for immune modulation
 *
 * WHAT: Types of immune modulation via active inference
 * WHY:  Different immune states require different interventions
 */
typedef enum {
    HYPO_IMMUNE_FEP_RESPONSE_NONE = 0,        /**< No response needed */
    HYPO_IMMUNE_FEP_RESPONSE_MONITOR,         /**< Increase monitoring */
    HYPO_IMMUNE_FEP_RESPONSE_MODULATE,        /**< Modulate immune response */
    HYPO_IMMUNE_FEP_RESPONSE_SUPPRESS,        /**< Suppress inflammation */
    HYPO_IMMUNE_FEP_RESPONSE_EMERGENCY        /**< Emergency intervention */
} hypo_immune_fep_response_t;

/**
 * @brief Immune event types detected by FEP
 *
 * WHAT: Classification of immune event types
 * WHY:  Different events require different handling
 */
typedef enum {
    HYPO_IMMUNE_EVENT_NONE = 0,               /**< No event */
    HYPO_IMMUNE_EVENT_ACTIVATION,             /**< Immune activation */
    HYPO_IMMUNE_EVENT_INFLAMMATION,           /**< Inflammatory response */
    HYPO_IMMUNE_EVENT_RESOLUTION,             /**< Resolution phase */
    HYPO_IMMUNE_EVENT_STORM                   /**< Cytokine storm */
} hypo_immune_event_type_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus immune FEP configuration
 *
 * WHAT: Configuration for FEP-immune integration
 * WHY:  Control detection sensitivity and response parameters
 */
typedef struct {
    /* FEP parameters */
    float drive_fe_weight;                    /**< Weight of drives in free energy */
    float prediction_error_gain;              /**< PE gain from cytokine deviation */
    float precision_modulation;               /**< Precision based on fatigue */
    bool enable_active_inference;             /**< Allow immune-based actions */
    bool enable_bio_async;                    /**< Bio-async integration enabled */

    /* Cytokine weights */
    float il1_weight;                         /**< IL-1 contribution to FE */
    float il6_weight;                         /**< IL-6 contribution to FE */
    float tnf_weight;                         /**< TNF-alpha contribution to FE */
    float il10_weight;                        /**< IL-10 contribution (negative) */
    float ifn_weight;                         /**< IFN-gamma contribution to FE */

    /* Baseline levels */
    float il1_baseline;                       /**< IL-1 expected baseline */
    float il6_baseline;                       /**< IL-6 expected baseline */
    float tnf_baseline;                       /**< TNF-alpha expected baseline */
    float il10_baseline;                      /**< IL-10 expected baseline */
    float ifn_baseline;                       /**< IFN-gamma expected baseline */

    /* Detection parameters */
    float free_energy_threshold;              /**< FE threshold for detection */
    float surprise_threshold;                 /**< Surprise threshold */
    float precision_learning_rate;            /**< Precision adaptation rate */

    /* Inflammation response */
    bool enable_fever_response;               /**< Enable hypothalamic fever */
    bool enable_sickness_behavior;            /**< Enable sickness behavior */
    float inflammation_precision_reduction;   /**< Precision reduction during inflammation */

    /* Learning */
    bool enable_online_learning;              /**< Update FEP from events */
    float learning_rate;                      /**< Belief update rate */
} hypo_immune_fep_config_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief FEP effects output
 *
 * WHAT: How FEP analysis affects immune coordination
 * WHY:  Free energy provides immune threat signal
 */
typedef struct {
    float free_energy;                        /**< Current FE from cytokines */
    float prediction_error;                   /**< PE from immune state */
    float precision;                          /**< Current precision */
    float active_inference_strength;          /**< Action strength */

    hypo_immune_fep_state_t immune_state;     /**< Immune state classification */
    float state_confidence;                   /**< Detection confidence [0-1] */

    hypo_immune_fep_response_t recommended_response; /**< Recommended action */
    float response_urgency;                   /**< Response urgency [0-1] */

    hypo_immune_event_type_t detected_event;  /**< Most significant event */
    float immune_health;                      /**< Immune health estimate [0-1] */

    /* Cytokine-derived metrics */
    float inflammation_level;                 /**< Overall inflammation [0-1] */
    float resolution_progress;                /**< Resolution progress [0-1] */
    float fever_signal;                       /**< Hypothalamic fever signal [0-1] */
} hypo_immune_fep_effects_t;

/**
 * @brief Immune effects on FEP
 *
 * WHAT: How immune events affect FEP beliefs
 * WHY:  Immune patterns update the generative model
 */
typedef struct {
    /* Cytokine levels (normalized 0-1) */
    float cytokine_il1;                       /**< Current IL-1 level */
    float cytokine_il6;                       /**< Current IL-6 level */
    float cytokine_tnf;                       /**< Current TNF-alpha level */
    float cytokine_il10;                      /**< Current IL-10 level */
    float cytokine_ifn;                       /**< Current IFN-gamma level */

    /* Event counts */
    uint64_t activations_detected;            /**< Immune activations */
    uint64_t inflammations_detected;          /**< Inflammation events */
    uint64_t resolutions_detected;            /**< Resolution events */
    uint64_t storms_detected;                 /**< Cytokine storms */

    /* State metrics */
    brain_inflammation_level_t inflammation_level; /**< Current inflammation */
    float inflammation_duration_sec;          /**< Duration of inflammation */
    bool cytokine_storm_active;               /**< Storm condition active */
    bool sickness_behavior_active;            /**< Sickness behavior active */

    /* Drive integration */
    float fatigue_drive_impact;               /**< Impact on fatigue drive */
    float social_drive_impact;                /**< Impact on social drive */
} immune_to_fep_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief FEP bridge state
 *
 * WHAT: Current operational state of the bridge
 * WHY:  Track real-time status for monitoring
 */
typedef struct {
    bool active;                              /**< Whether bridge is active */
    uint64_t update_count;                    /**< Number of updates */
    uint64_t detection_count;                 /**< Detections processed */

    float current_precision;                  /**< Current precision level */
    float avg_surprise;                       /**< Running average surprise */
    float avg_prediction_error;               /**< Running average PE */

    hypo_immune_fep_state_t last_state;       /**< Last immune state */
    uint64_t last_detection_time_ms;          /**< Timestamp of last detection */

    /* Cytokine tracking */
    float cytokine_history[5][16];            /**< Recent cytokine history */
    uint32_t history_idx;                     /**< Current history index */
} hypo_immune_fep_state_internal_t;

/**
 * @brief FEP bridge statistics
 *
 * WHAT: Cumulative statistics for the bridge
 * WHY:  Performance monitoring and tuning
 */
typedef struct {
    uint64_t total_updates;                   /**< Total updates performed */
    uint64_t fep_detections;                  /**< FEP-based detections */
    uint64_t immune_events_detected;          /**< Immune events found */
    uint64_t responses_triggered;             /**< Responses triggered */
    uint64_t precision_adaptations;           /**< Precision updates */

    /* By event type */
    uint64_t activations;                     /**< Activation events */
    uint64_t inflammations;                   /**< Inflammation events */
    uint64_t resolutions;                     /**< Resolution events */
    uint64_t storms;                          /**< Storm events */

    float avg_free_energy;                    /**< Average free energy */
    float avg_surprise;                       /**< Average surprise */
    float avg_prediction_error;               /**< Average prediction error */
    float current_precision;                  /**< Current precision */

    float max_free_energy;                    /**< Maximum FE observed */
    float max_inflammation;                   /**< Maximum inflammation level */
} hypo_immune_fep_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus Immune FEP Bridge
 *
 * WHAT: Main bridge connecting hypothalamus immune to FEP
 * WHY:  Centralized integration of immune system with free energy principle
 * HOW:  Contains configuration, connections, effects, and state
 */
typedef struct {
    bridge_base_t base;                       /**< MUST be first: base infrastructure */

    hypo_immune_fep_config_t config;          /**< Configuration */

    /* System connections */
    hypo_drive_system_handle_t* drive_system; /**< Connected drive system */
    brain_immune_system_t* immune_system;     /**< Connected immune system */
    fep_system_t* fep_system;                 /**< Connected FEP system */

    /* Bidirectional effects */
    hypo_immune_fep_effects_t fep_effects;    /**< FEP -> Immune effects */
    immune_to_fep_effects_t immune_effects;   /**< Immune -> FEP effects */

    /* State and statistics */
    hypo_immune_fep_state_internal_t state;   /**< Current state */
    hypo_immune_fep_stats_t stats;            /**< Statistics */
} hypo_immune_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for immune FEP integration
 * WHY:  Simplify initialization with biologically-plausible settings
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int hypo_immune_fep_default_config(hypo_immune_fep_config_t* config);

/**
 * @brief Create hypothalamus immune FEP bridge
 *
 * WHAT: Initialize FEP integration for immune system
 * WHY:  Enable surprise-based immune monitoring
 *
 * @param config Configuration (NULL for defaults)
 * @param drive_system Drive system handle
 * @param immune_system Immune system handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
hypo_immune_fep_bridge_t* hypo_immune_fep_create(
    const hypo_immune_fep_config_t* config,
    hypo_drive_system_handle_t* drive_system,
    brain_immune_system_t* immune_system,
    fep_system_t* fep_system
);

/**
 * @brief Destroy hypothalamus immune FEP bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void hypo_immune_fep_destroy(hypo_immune_fep_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_immune_fep_reset(hypo_immune_fep_bridge_t* bridge);

/**
 * @brief Update bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_immune_fep_update(hypo_immune_fep_bridge_t* bridge);

/* ============================================================================
 * Core Operations API
 * ============================================================================ */

/**
 * @brief Compute free energy from drive state
 *
 * WHAT: Calculate FE from current drive and cytokine state
 * WHY:  Core FEP computation for immune monitoring
 *
 * @param bridge Bridge handle
 * @param drives Drive state input
 * @return 0 on success, -1 on error
 */
int hypo_immune_fep_compute_fe(
    hypo_immune_fep_bridge_t* bridge,
    const hypo_drive_system_t* drives
);

/**
 * @brief Modulate precision based on fatigue
 *
 * WHAT: Adjust detection precision based on fatigue and inflammation
 * WHY:  Precision represents confidence; illness reduces confidence
 *
 * @param bridge Bridge handle
 * @param fatigue Fatigue level [0-1]
 * @return 0 on success, -1 on error
 */
int hypo_immune_fep_modulate_precision(
    hypo_immune_fep_bridge_t* bridge,
    float fatigue
);

/**
 * @brief Get current FEP effects
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int hypo_immune_fep_get_effects(
    const hypo_immune_fep_bridge_t* bridge,
    hypo_immune_fep_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int hypo_immune_fep_get_stats(
    const hypo_immune_fep_bridge_t* bridge,
    hypo_immune_fep_stats_t* stats
);

/* ============================================================================
 * Cytokine Integration API
 * ============================================================================ */

/**
 * @brief Update cytokine levels
 *
 * WHAT: Feed current cytokine levels to bridge
 * WHY:  Cytokines are primary immune signals
 *
 * @param bridge Bridge handle
 * @param il1 IL-1 level [0-1]
 * @param il6 IL-6 level [0-1]
 * @param tnf TNF-alpha level [0-1]
 * @param il10 IL-10 level [0-1]
 * @param ifn IFN-gamma level [0-1]
 * @return 0 on success, -1 on error
 */
int hypo_immune_fep_update_cytokines(
    hypo_immune_fep_bridge_t* bridge,
    float il1,
    float il6,
    float tnf,
    float il10,
    float ifn
);

/**
 * @brief Get fever signal
 *
 * WHAT: Get hypothalamic fever signal based on cytokines
 * WHY:  IL-1 and prostaglandins trigger fever via hypothalamus
 *
 * @param bridge Bridge handle
 * @return Fever signal [0-1] or -1.0f on error
 */
float hypo_immune_fep_get_fever_signal(const hypo_immune_fep_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge handle
 * @param router Bio-router handle
 * @return 0 on success, -1 on error
 */
int hypo_immune_fep_connect_bio_async(
    hypo_immune_fep_bridge_t* bridge,
    bio_router_t* router
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_immune_fep_disconnect_bio_async(hypo_immune_fep_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
int hypo_immune_fep_process_messages(
    hypo_immune_fep_bridge_t* bridge,
    uint32_t max_messages
);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Get immune state name
 *
 * @param state Immune state
 * @return Human-readable name
 */
const char* hypo_immune_fep_state_name(hypo_immune_fep_state_t state);

/**
 * @brief Get response type name
 *
 * @param response Response type
 * @return Human-readable name
 */
const char* hypo_immune_fep_response_name(hypo_immune_fep_response_t response);

/**
 * @brief Get event type name
 *
 * @param event Event type
 * @return Human-readable name
 */
const char* hypo_immune_fep_event_name(hypo_immune_event_type_t event);

/**
 * @brief Print bridge summary
 *
 * @param bridge Bridge handle
 */
void hypo_immune_fep_print_summary(const hypo_immune_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPO_IMMUNE_FEP_BRIDGE_H */
