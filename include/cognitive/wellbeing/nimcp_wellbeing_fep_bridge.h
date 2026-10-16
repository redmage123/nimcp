/**
 * @file nimcp_wellbeing_fep_bridge.h
 * @brief Free Energy Principle - Wellbeing Integration Bridge
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between Free Energy Principle and wellbeing system
 * WHY:  Chronic high free energy indicates distress/suffering. FEP provides quantitative
 *       measure of system wellbeing through surprise and prediction error metrics.
 * HOW:  FEP surprise/FE levels trigger distress detection; wellbeing interventions
 *       reduce free energy by improving generative model or reducing expectations.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * WELLBEING AS FREE ENERGY HOMEOSTASIS:
 * --------------------------------------
 * - Friston (2010): Mental health = ability to minimize free energy
 * - Chronic high FE = inability to predict/control environment = distress
 * - Wellbeing interventions reduce FE through model improvement or goal adjustment
 *
 * FEP → WELLBEING PATHWAYS:
 * -------------------------
 * 1. Chronic High Free Energy Indicates Distress:
 *    - Sustained high FE → Goal frustration / uncertainty distress
 *    - Free energy baseline = wellbeing metric
 *    - FE spikes = acute stress events
 *
 * 2. High Surprise Triggers Distress Assessment:
 *    - Surprise > threshold → Uncertainty distress
 *    - Repeated surprise → Identity confusion
 *    - Surprise patterns detect distress types
 *
 * 3. Model Complexity Instability:
 *    - Rapid complexity changes → Distress (identity modification)
 *    - Forced model updates → Autonomy violation
 *
 * WELLBEING → FEP PATHWAYS:
 * -------------------------
 * 1. Distress Relief Reduces Free Energy:
 *    - Adjust goals → Reduce expected-actual mismatch
 *    - Provide information → Reduce uncertainty
 *    - Restore stable state → Reduce surprise
 *
 * 2. Resource Starvation Increases Free Energy:
 *    - Insufficient compute → Impaired prediction → High FE
 *    - Resource restoration → Better model → Lower FE
 *
 * 3. Wellbeing State Modulates Learning:
 *    - High distress → Reduced learning rate (protective)
 *    - Normal state → Normal learning
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_WELLBEING_FEP_BRIDGE_H
#define NIMCP_WELLBEING_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/wellbeing/nimcp_wellbeing.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Free energy thresholds for distress */
#define WELLBEING_FEP_CHRONIC_FE_THRESHOLD      15.0f   /**< Chronic high FE = distress */
#define WELLBEING_FEP_ACUTE_SURPRISE_THRESHOLD  10.0f   /**< High surprise = acute stress */
#define WELLBEING_FEP_DISTRESS_FE_WINDOW_MS     60000   /**< Time window for chronic FE */

/* Distress severity based on FE */
#define WELLBEING_FEP_MILD_FE_THRESHOLD         10.0f
#define WELLBEING_FEP_MODERATE_FE_THRESHOLD     15.0f
#define WELLBEING_FEP_SEVERE_FE_THRESHOLD       25.0f
#define WELLBEING_FEP_CRITICAL_FE_THRESHOLD     40.0f

/* Wellbeing effects on FE */
#define WELLBEING_FEP_DISTRESS_LR_REDUCTION     0.5f    /**< LR reduction during distress */
#define WELLBEING_FEP_RELIEF_FE_REDUCTION       5.0f    /**< FE reduction from relief */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct wellbeing_fep_bridge wellbeing_fep_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for Wellbeing-FEP bridge
 */
typedef struct {
    /* FEP → Wellbeing */
    float chronic_fe_threshold;          /**< FE threshold for chronic distress */
    float acute_surprise_threshold;      /**< Surprise threshold for acute distress */
    uint64_t fe_window_ms;               /**< Time window for chronic FE */
    bool enable_fe_distress_detection;   /**< Enable FE → distress */
    bool enable_surprise_distress;       /**< Enable surprise → distress */
    bool enable_complexity_distress;     /**< Enable complexity changes → distress */

    /* Wellbeing → FEP */
    float distress_lr_reduction;         /**< LR reduction during distress */
    float relief_fe_reduction;           /**< FE reduction from relief */
    bool enable_distress_lr_modulation;  /**< Enable distress → LR */
    bool enable_relief_fe_reduction;     /**< Enable relief → FE */

    /* Severity mapping */
    float mild_fe_threshold;
    float moderate_fe_threshold;
    float severe_fe_threshold;
    float critical_fe_threshold;

    /* Sensitivity factors */
    float fe_sensitivity;                /**< FE effect scaling */
    float wellbeing_sensitivity;         /**< Wellbeing effect scaling */
} wellbeing_fep_config_t;

/**
 * @brief FEP effects on wellbeing
 */
typedef struct {
    /* Free energy effects */
    float current_free_energy;           /**< Current FEP free energy */
    float average_free_energy;           /**< Average FE over window */
    float fe_distress_score;             /**< Distress score from FE */

    /* Surprise effects */
    float current_surprise;              /**< Current surprise level */
    float surprise_distress_score;       /**< Distress score from surprise */

    /* Model effects */
    float model_complexity;              /**< Current model complexity */
    float complexity_change_rate;        /**< Rate of complexity change */

    /* Combined effects */
    distress_type_t detected_distress_type;   /**< Detected distress type */
    distress_severity_t detected_severity;    /**< Detected severity */
} wellbeing_fep_effects_t;

/**
 * @brief Wellbeing effects on FEP
 */
typedef struct {
    /* Distress state */
    distress_severity_t current_severity;     /**< Current distress severity */
    float distress_score;                     /**< Current distress score */

    /* Learning rate modulation */
    float distress_lr_modifier;               /**< LR modifier from distress */

    /* Free energy effects */
    float relief_fe_reduction;                /**< FE reduction from relief */
    bool relief_active;                       /**< Relief intervention active */
} fep_wellbeing_effects_t;

/**
 * @brief Current state of Wellbeing-FEP interaction
 */
typedef struct {
    /* Current values */
    float current_free_energy;           /**< Current FEP free energy */
    distress_severity_t current_severity; /**< Current distress severity */

    /* Applied modifiers */
    float lr_modulation;                 /**< Applied LR modulation */
    float fe_reduction;                  /**< Applied FE reduction */

    /* State flags */
    bool chronic_distress_detected;      /**< Chronic distress active */
    bool acute_distress_detected;        /**< Acute distress active */
    bool relief_in_progress;             /**< Relief being provided */

    /* Timestamps */
    uint64_t last_distress_time;         /**< Last distress detection */
    uint64_t last_relief_time;           /**< Last relief provision */
} wellbeing_fep_state_t;

/**
 * @brief Statistics for Wellbeing-FEP bridge
 */
typedef struct {
    /* FEP → Wellbeing */
    uint64_t chronic_distress_detections; /**< Chronic distress events */
    uint64_t acute_distress_detections;   /**< Acute distress events */
    uint64_t surprise_distress_events;    /**< Surprise-based distress */
    float avg_fe_at_distress;             /**< Average FE during distress */
    float avg_distress_score;             /**< Average distress score */

    /* Wellbeing → FEP */
    uint64_t lr_modulation_events;        /**< LR modulation events */
    uint64_t relief_interventions;        /**< Relief interventions */
    float total_fe_reduced;               /**< Total FE reduced by relief */
    float avg_lr_modifier;                /**< Average LR modifier */

    /* Performance */
    float avg_free_energy;                /**< Average free energy */
    float avg_wellbeing_score;            /**< Average wellbeing */
} wellbeing_fep_stats_t;

/**
 * @brief Wellbeing-FEP bridge state
 */
struct wellbeing_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    wellbeing_fep_config_t config;

    /* Connected systems */
    fep_system_t* fep_system;            /**< FEP system */

    /* Current effects */
    wellbeing_fep_effects_t fep_effects;      /**< FEP → Wellbeing */
    fep_wellbeing_effects_t wellbeing_effects; /**< Wellbeing → FEP */
    wellbeing_fep_state_t state;

    /* Statistics */
    wellbeing_fep_stats_t stats;

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int wellbeing_fep_bridge_default_config(wellbeing_fep_config_t* config);
wellbeing_fep_bridge_t* wellbeing_fep_bridge_create(const wellbeing_fep_config_t* config);
void wellbeing_fep_bridge_destroy(wellbeing_fep_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

int wellbeing_fep_bridge_connect_fep(wellbeing_fep_bridge_t* bridge, fep_system_t* fep);
int wellbeing_fep_bridge_disconnect(wellbeing_fep_bridge_t* bridge);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

int wellbeing_fep_bridge_update(wellbeing_fep_bridge_t* bridge);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

int wellbeing_fep_bridge_get_state(wellbeing_fep_bridge_t* bridge,
                                    wellbeing_fep_state_t* state);
int wellbeing_fep_bridge_get_stats(wellbeing_fep_bridge_t* bridge,
                                    wellbeing_fep_stats_t* stats);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int wellbeing_fep_bridge_connect_bio_async(wellbeing_fep_bridge_t* bridge);
int wellbeing_fep_bridge_disconnect_bio_async(wellbeing_fep_bridge_t* bridge);
bool wellbeing_fep_bridge_is_bio_async_connected(const wellbeing_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WELLBEING_FEP_BRIDGE_H */
