/**
 * @file nimcp_hypo_salience_fep_bridge.h
 * @brief Free Energy Principle bridge for Hypothalamus Salience Integration
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for hypothalamic salience-based prioritization
 * WHY:  Drive urgency modulates attention allocation through salience weights;
 *       fatigue and homeostatic state affect precision in the predictive framework
 * HOW:  Map drive urgency to salience weights, fatigue to precision reduction,
 *       and use active inference for resource allocation
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * HYPOTHALAMIC SALIENCE AS ATTENTION ALLOCATION:
 * ----------------------------------------------
 * The hypothalamus generates salience signals that prioritize processing based on
 * homeostatic needs. High-urgency drives (hunger, thirst, threat) capture attention:
 *
 * 1. Low drive urgency = low salience (background processing)
 * 2. High drive urgency = high salience (attention capture)
 * 3. Fatigue = reduced precision (decreased attention capacity)
 * 4. Competing drives = salience competition (winner-take-all allocation)
 *
 * NEUROBIOLOGICAL CONNECTIONS:
 * - Orexin neurons: Arousal and attention modulation
 * - Dopamine system: Salience tagging and prediction error
 * - Norepinephrine: Precision modulation (LC-NE system)
 * - Cortisol (HPA axis): Stress-induced attention narrowing
 *
 * FEP INTEGRATION:
 * ```
 * Drive State (d) -> Salience Computation
 *         |
 * Expected Priority Pattern mu (baseline salience allocation)
 *         |
 * Prediction Error: epsilon = d - g(mu)
 *         |
 * Free Energy F = Complexity + Inaccuracy
 *         |
 * Salience Weight = softmax(-F / temperature)
 *         |
 * Precision = f(fatigue, arousal, attention_capacity)
 * ```
 *
 * FEP MAPPINGS:
 * - Drive urgency -> Salience weights (higher urgency = higher salience)
 * - Fatigue -> Precision reduction (reduced attention capacity)
 * - Drive conflict -> Free energy (competing priorities)
 * - Resolution -> Belief update (priority reallocation)
 *
 * SALIENCE MAPPING:
 * - Low salience (<0.2)  -> Background/maintenance processing
 * - Medium salience (0.2-0.5) -> Elevated attention
 * - High salience (0.5-0.8) -> Priority processing
 * - Very high salience (>0.8) -> Emergency response
 *
 * @see nimcp_hypothalamus_drives.h
 * @see nimcp_salience.h
 * @see nimcp_free_energy.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HYPO_SALIENCE_FEP_BRIDGE_H
#define NIMCP_HYPO_SALIENCE_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
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

/** Bio-async module ID for hypothalamus salience FEP bridge */
#define BIO_MODULE_HYPO_SALIENCE_FEP    0x0B04

/** Free energy thresholds for salience urgency levels */
#define HYPO_SALIENCE_FEP_LOW_THRESHOLD       2.0f   /**< Low urgency */
#define HYPO_SALIENCE_FEP_MEDIUM_THRESHOLD    5.0f   /**< Medium urgency */
#define HYPO_SALIENCE_FEP_HIGH_THRESHOLD      10.0f  /**< High urgency */
#define HYPO_SALIENCE_FEP_CRITICAL_THRESHOLD  20.0f  /**< Critical/emergency */

/** Precision bounds */
#define HYPO_SALIENCE_FEP_MIN_PRECISION       0.1f   /**< Minimum precision */
#define HYPO_SALIENCE_FEP_MAX_PRECISION       10.0f  /**< Maximum precision */
#define HYPO_SALIENCE_FEP_DEFAULT_PRECISION   1.0f   /**< Default precision */

/** Salience weight bounds */
#define HYPO_SALIENCE_FEP_MIN_WEIGHT          0.01f  /**< Minimum salience weight */
#define HYPO_SALIENCE_FEP_MAX_WEIGHT          1.0f   /**< Maximum salience weight */

/** Softmax temperature for salience computation */
#define HYPO_SALIENCE_FEP_DEFAULT_TEMPERATURE 1.0f   /**< Default softmax temp */

/** Fatigue impact constants */
#define HYPO_SALIENCE_FEP_FATIGUE_PRECISION_SCALE 0.5f  /**< Max precision reduction */
#define HYPO_SALIENCE_FEP_FATIGUE_SALIENCE_SCALE  0.3f  /**< Max salience reduction */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Salience urgency levels based on FEP metrics
 *
 * WHAT: Categorization of overall salience urgency
 * WHY:  Enable graded attention allocation
 */
typedef enum {
    HYPO_SALIENCE_FEP_LEVEL_LOW = 0,          /**< Low urgency */
    HYPO_SALIENCE_FEP_LEVEL_MODERATE,         /**< Moderate urgency */
    HYPO_SALIENCE_FEP_LEVEL_ELEVATED,         /**< Elevated urgency */
    HYPO_SALIENCE_FEP_LEVEL_HIGH,             /**< High urgency */
    HYPO_SALIENCE_FEP_LEVEL_CRITICAL          /**< Critical - emergency response */
} hypo_salience_fep_level_t;

/**
 * @brief Active inference response types for salience
 *
 * WHAT: Types of attention allocation via active inference
 * WHY:  Different urgency levels require different allocations
 */
typedef enum {
    HYPO_SALIENCE_FEP_RESPONSE_MAINTAIN = 0,  /**< Maintain current allocation */
    HYPO_SALIENCE_FEP_RESPONSE_SHIFT,         /**< Shift attention */
    HYPO_SALIENCE_FEP_RESPONSE_FOCUS,         /**< Focus on priority */
    HYPO_SALIENCE_FEP_RESPONSE_NARROW,        /**< Narrow focus (stress) */
    HYPO_SALIENCE_FEP_RESPONSE_EMERGENCY      /**< Emergency tunneling */
} hypo_salience_fep_response_t;

/**
 * @brief Salience conflict types
 *
 * WHAT: Classification of salience conflict types
 * WHY:  Different conflicts require different resolution strategies
 */
typedef enum {
    HYPO_SALIENCE_CONFLICT_NONE = 0,          /**< No conflict */
    HYPO_SALIENCE_CONFLICT_MILD,              /**< Mild competition */
    HYPO_SALIENCE_CONFLICT_MODERATE,          /**< Moderate conflict */
    HYPO_SALIENCE_CONFLICT_SEVERE             /**< Severe conflict */
} hypo_salience_conflict_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus salience FEP configuration
 *
 * WHAT: Configuration for FEP-salience integration
 * WHY:  Control attention allocation and fatigue effects
 */
typedef struct {
    /* FEP parameters */
    float drive_fe_weight;                    /**< Weight of drives in free energy */
    float prediction_error_gain;              /**< PE gain from urgency deviation */
    float precision_modulation;               /**< Precision based on fatigue */
    bool enable_active_inference;             /**< Allow salience-based actions */
    bool enable_bio_async;                    /**< Bio-async integration enabled */

    /* Salience computation */
    float softmax_temperature;                /**< Temperature for salience softmax */
    float urgency_to_salience_scale;          /**< Urgency to salience conversion */
    float salience_decay_rate;                /**< Salience decay over time */

    /* Fatigue effects */
    float fatigue_precision_scale;            /**< Fatigue impact on precision */
    float fatigue_salience_scale;             /**< Fatigue impact on salience */
    bool enable_fatigue_adaptation;           /**< Adapt to sustained fatigue */

    /* Conflict resolution */
    float conflict_threshold;                 /**< Threshold for conflict detection */
    float conflict_resolution_tau;            /**< Time constant for resolution */
    bool enable_winner_take_all;              /**< Use WTA for conflict */

    /* Detection parameters */
    float free_energy_threshold;              /**< FE threshold for detection */
    float surprise_threshold;                 /**< Surprise threshold */
    float precision_learning_rate;            /**< Precision adaptation rate */

    /* Learning */
    bool enable_online_learning;              /**< Update FEP from events */
    float learning_rate;                      /**< Belief update rate */
} hypo_salience_fep_config_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief FEP effects output
 *
 * WHAT: How FEP analysis affects salience allocation
 * WHY:  Free energy provides urgency-based attention signal
 */
typedef struct {
    float free_energy;                        /**< Current FE from drives */
    float prediction_error;                   /**< PE from urgency deviation */
    float precision;                          /**< Current precision */
    float active_inference_strength;          /**< Action strength */

    hypo_salience_fep_level_t urgency_level;  /**< Urgency classification */
    float urgency_confidence;                 /**< Detection confidence [0-1] */

    hypo_salience_fep_response_t recommended_response; /**< Recommended action */
    float response_urgency;                   /**< Response urgency [0-1] */

    hypo_salience_conflict_t conflict_level;  /**< Current conflict level */
    float conflict_intensity;                 /**< Conflict intensity [0-1] */

    /* Salience outputs */
    float salience_weights[HYPO_DRIVE_COUNT]; /**< Salience weight per drive */
    float total_salience;                     /**< Sum of all salience */
    hypo_drive_type_t dominant_drive;         /**< Most salient drive */
    float dominant_salience;                  /**< Salience of dominant drive */

    /* Attention metrics */
    float attention_capacity;                 /**< Available attention [0-1] */
    float attention_focus;                    /**< Focus intensity [0-1] */
} hypo_salience_fep_effects_t;

/**
 * @brief Salience effects on FEP
 *
 * WHAT: How salience state affects FEP beliefs
 * WHY:  Salience patterns update the generative model
 */
typedef struct {
    /* Drive state */
    float drive_urgencies[HYPO_DRIVE_COUNT];  /**< Current drive urgencies */
    hypo_drive_type_t priority_drive;         /**< Current priority drive */
    float priority_urgency;                   /**< Priority drive urgency */

    /* Fatigue state */
    float current_fatigue;                    /**< Current fatigue level */
    float arousal_level;                      /**< Current arousal */

    /* Conflict tracking */
    uint64_t conflicts_detected;              /**< Conflicts detected */
    uint64_t conflicts_resolved;              /**< Conflicts resolved */
    float avg_conflict_duration_ms;           /**< Average resolution time */

    /* Attention metrics */
    float sustained_attention_time_ms;        /**< Time in sustained attention */
    float attention_switches;                 /**< Number of attention switches */
    bool attention_depleted;                  /**< Attention resources depleted */
} salience_to_fep_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Salience tracking state
 *
 * WHAT: Tracks salience evolution over time
 * WHY:  Enables FEP-based salience prediction
 */
typedef struct {
    float salience_history[HYPO_DRIVE_COUNT][16]; /**< Recent salience history */
    uint32_t history_idx;                     /**< Current history index */

    float predicted_salience[HYPO_DRIVE_COUNT]; /**< Predicted salience */
    float salience_velocity[HYPO_DRIVE_COUNT];  /**< Rate of salience change */

    uint64_t last_switch_time_ms;             /**< Last attention switch */
    hypo_drive_type_t last_dominant;          /**< Previous dominant drive */
} hypo_salience_tracking_t;

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

    hypo_salience_fep_level_t last_level;     /**< Last urgency level */
    uint64_t last_detection_time_ms;          /**< Timestamp of last detection */

    hypo_salience_tracking_t salience_tracking; /**< Salience tracking state */
} hypo_salience_fep_state_t;

/**
 * @brief FEP bridge statistics
 *
 * WHAT: Cumulative statistics for the bridge
 * WHY:  Performance monitoring and tuning
 */
typedef struct {
    uint64_t total_updates;                   /**< Total updates performed */
    uint64_t fep_detections;                  /**< FEP-based detections */
    uint64_t salience_shifts;                 /**< Salience shifts detected */
    uint64_t conflicts_detected;              /**< Conflicts detected */
    uint64_t conflicts_resolved;              /**< Conflicts resolved */
    uint64_t precision_adaptations;           /**< Precision updates */

    /* By drive type */
    uint64_t dominance_counts[HYPO_DRIVE_COUNT]; /**< Dominance by drive */

    float avg_free_energy;                    /**< Average free energy */
    float avg_surprise;                       /**< Average surprise */
    float avg_prediction_error;               /**< Average prediction error */
    float current_precision;                  /**< Current precision */

    float max_free_energy;                    /**< Maximum FE observed */
    float max_salience;                       /**< Maximum salience observed */
    float max_conflict_intensity;             /**< Maximum conflict intensity */

    float avg_attention_capacity;             /**< Average attention capacity */
    float avg_switch_interval_ms;             /**< Average attention switch interval */
} hypo_salience_fep_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus Salience FEP Bridge
 *
 * WHAT: Main bridge connecting hypothalamus salience to FEP
 * WHY:  Centralized integration of salience with free energy principle
 * HOW:  Contains configuration, connections, effects, and state
 */
typedef struct {
    bridge_base_t base;                       /**< MUST be first: base infrastructure */

    hypo_salience_fep_config_t config;        /**< Configuration */

    /* System connections */
    hypo_drive_system_handle_t* drive_system; /**< Connected drive system */
    fep_system_t* fep_system;                 /**< Connected FEP system */

    /* Bidirectional effects */
    hypo_salience_fep_effects_t fep_effects;  /**< FEP -> Salience effects */
    salience_to_fep_effects_t sal_effects;    /**< Salience -> FEP effects */

    /* State and statistics */
    hypo_salience_fep_state_t state;          /**< Current state */
    hypo_salience_fep_stats_t stats;          /**< Statistics */
} hypo_salience_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for salience FEP integration
 * WHY:  Simplify initialization with biologically-plausible settings
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int hypo_salience_fep_default_config(hypo_salience_fep_config_t* config);

/**
 * @brief Create hypothalamus salience FEP bridge
 *
 * WHAT: Initialize FEP integration for salience
 * WHY:  Enable urgency-based attention allocation
 *
 * @param config Configuration (NULL for defaults)
 * @param drive_system Drive system handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
hypo_salience_fep_bridge_t* hypo_salience_fep_create(
    const hypo_salience_fep_config_t* config,
    hypo_drive_system_handle_t* drive_system,
    fep_system_t* fep_system
);

/**
 * @brief Destroy hypothalamus salience FEP bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void hypo_salience_fep_destroy(hypo_salience_fep_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_salience_fep_reset(hypo_salience_fep_bridge_t* bridge);

/**
 * @brief Update bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_salience_fep_update(hypo_salience_fep_bridge_t* bridge);

/* ============================================================================
 * Core Operations API
 * ============================================================================ */

/**
 * @brief Compute free energy from drive state
 *
 * WHAT: Calculate FE from current drive urgencies
 * WHY:  Core FEP computation for salience allocation
 *
 * @param bridge Bridge handle
 * @param drives Drive state input
 * @return 0 on success, -1 on error
 */
int hypo_salience_fep_compute_fe(
    hypo_salience_fep_bridge_t* bridge,
    const hypo_drive_system_t* drives
);

/**
 * @brief Modulate precision based on fatigue
 *
 * WHAT: Adjust salience precision based on fatigue
 * WHY:  Fatigue reduces attention capacity and salience precision
 *
 * @param bridge Bridge handle
 * @param fatigue Fatigue level [0-1]
 * @return 0 on success, -1 on error
 */
int hypo_salience_fep_modulate_precision(
    hypo_salience_fep_bridge_t* bridge,
    float fatigue
);

/**
 * @brief Get current FEP effects
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int hypo_salience_fep_get_effects(
    const hypo_salience_fep_bridge_t* bridge,
    hypo_salience_fep_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int hypo_salience_fep_get_stats(
    const hypo_salience_fep_bridge_t* bridge,
    hypo_salience_fep_stats_t* stats
);

/* ============================================================================
 * Salience API
 * ============================================================================ */

/**
 * @brief Get salience weight for a drive
 *
 * WHAT: Get the current salience weight for a specific drive
 * WHY:  Enable drive-specific attention queries
 *
 * @param bridge Bridge handle
 * @param drive Drive type
 * @return Salience weight [0-1] or -1.0f on error
 */
float hypo_salience_fep_get_weight(
    const hypo_salience_fep_bridge_t* bridge,
    hypo_drive_type_t drive
);

/**
 * @brief Get all salience weights
 *
 * WHAT: Get salience weights for all drives
 * WHY:  Enable batch attention allocation
 *
 * @param bridge Bridge handle
 * @param weights Output array (size HYPO_DRIVE_COUNT)
 * @return 0 on success, -1 on error
 */
int hypo_salience_fep_get_weights(
    const hypo_salience_fep_bridge_t* bridge,
    float* weights
);

/**
 * @brief Detect salience conflict
 *
 * WHAT: Detect conflict between competing drives
 * WHY:  Conflict requires resolution strategy
 *
 * @param bridge Bridge handle
 * @param conflict_out Output conflict type
 * @param intensity_out Output conflict intensity [0-1]
 * @return 0 on success, -1 on error
 */
int hypo_salience_fep_detect_conflict(
    hypo_salience_fep_bridge_t* bridge,
    hypo_salience_conflict_t* conflict_out,
    float* intensity_out
);

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
int hypo_salience_fep_connect_bio_async(
    hypo_salience_fep_bridge_t* bridge,
    bio_router_t* router
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_salience_fep_disconnect_bio_async(hypo_salience_fep_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
int hypo_salience_fep_process_messages(
    hypo_salience_fep_bridge_t* bridge,
    uint32_t max_messages
);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Get urgency level name
 *
 * @param level Urgency level
 * @return Human-readable name
 */
const char* hypo_salience_fep_level_name(hypo_salience_fep_level_t level);

/**
 * @brief Get response type name
 *
 * @param response Response type
 * @return Human-readable name
 */
const char* hypo_salience_fep_response_name(hypo_salience_fep_response_t response);

/**
 * @brief Get conflict type name
 *
 * @param conflict Conflict type
 * @return Human-readable name
 */
const char* hypo_salience_fep_conflict_name(hypo_salience_conflict_t conflict);

/**
 * @brief Print bridge summary
 *
 * @param bridge Bridge handle
 */
void hypo_salience_fep_print_summary(const hypo_salience_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPO_SALIENCE_FEP_BRIDGE_H */
