/**
 * @file nimcp_sleep_wake_fep_bridge.h
 * @brief Free Energy Principle - Sleep-Wake Integration Bridge
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between Free Energy Principle and sleep-wake system
 * WHY:  Sleep optimizes generative models and minimizes long-term free energy through
 *       memory consolidation and synaptic homeostasis.
 * HOW:  FEP free energy accumulation triggers sleep need; sleep consolidation
 *       reduces free energy by improving generative model accuracy.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP AS FREE ENERGY MINIMIZATION:
 * -----------------------------------
 * - Hobson & Friston (2012): Sleep optimizes generative models offline
 * - Tononi & Cirelli (2014): Synaptic homeostasis maintains prediction precision
 * - Free energy accumulates during wake → sleep reduces it via consolidation
 *
 * FEP → SLEEP-WAKE PATHWAYS:
 * ---------------------------
 * 1. Free Energy Accumulation Triggers Sleep Pressure:
 *    - High cumulative free energy → Increased adenosine/sleep pressure
 *    - Prediction errors during wake build up metabolic cost
 *    - Sleep becomes necessary when FE exceeds threshold
 *
 * 2. Model Complexity Drives Deep Sleep Need:
 *    - High model complexity → More deep NREM needed
 *    - Complexity = number of active patterns to consolidate
 *    - Deep sleep strengthens important patterns
 *
 * 3. Prediction Error Patterns Guide REM:
 *    - Novel/surprising patterns → More REM sleep
 *    - REM recombines patterns creatively
 *    - Reduces future surprise via model expansion
 *
 * SLEEP-WAKE → FEP PATHWAYS:
 * ---------------------------
 * 1. Consolidation Improves Model Accuracy:
 *    - Deep NREM replay → Reduced prediction errors
 *    - Synaptic scaling → Improved precision
 *    - Better generative model → Lower free energy
 *
 * 2. Sleep State Modulates Learning Rate:
 *    - Awake → High learning rate (fast adaptation)
 *    - Drowsy → Moderate LR (transition)
 *    - NREM → Low LR (consolidation only)
 *    - REM → High LR with noise (exploration)
 *
 * 3. Synaptic Homeostasis Maintains Precision Bounds:
 *    - Pruning weak connections → Reduces model complexity
 *    - Prevents precision degradation
 *    - Maintains optimal free energy range
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SLEEP_WAKE_FEP_BRIDGE_H
#define NIMCP_SLEEP_WAKE_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Free energy thresholds for sleep pressure */
#define SLEEP_FEP_FE_PRESSURE_SCALING      0.1f    /**< FE to pressure conversion */
#define SLEEP_FEP_HIGH_FE_THRESHOLD        20.0f   /**< High FE triggers sleep need */
#define SLEEP_FEP_COMPLEXITY_THRESHOLD     100.0f  /**< High complexity needs deep sleep */

/* Sleep state learning rate modulation */
#define SLEEP_FEP_AWAKE_LR_MULT           1.0f    /**< Normal LR when awake */
#define SLEEP_FEP_DROWSY_LR_MULT          0.7f    /**< Reduced LR when drowsy */
#define SLEEP_FEP_NREM_LR_MULT            0.3f    /**< Low LR during NREM */
#define SLEEP_FEP_REM_LR_MULT             1.2f    /**< High LR during REM */

/* Consolidation effects on free energy */
#define SLEEP_FEP_CONSOLIDATION_FE_REDUCTION  0.15f  /**< FE reduction per cycle */
#define SLEEP_FEP_PRUNING_FE_REDUCTION        0.05f  /**< FE reduction from pruning */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct sleep_wake_fep_bridge sleep_wake_fep_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for Sleep-Wake-FEP bridge
 */
typedef struct {
    /* FEP → Sleep-Wake */
    float fe_pressure_scaling;           /**< How much FE affects sleep pressure */
    float fe_sleep_threshold;            /**< FE level triggering sleep */
    float complexity_deep_sleep_scaling; /**< Complexity effect on deep sleep */
    bool enable_fe_sleep_pressure;       /**< Enable FE → sleep pressure */
    bool enable_complexity_deep_sleep;   /**< Enable complexity → deep sleep */
    bool enable_surprise_rem;            /**< Enable surprise → REM */

    /* Sleep-Wake → FEP */
    float consolidation_fe_reduction;    /**< FE reduction from consolidation */
    float pruning_fe_reduction;          /**< FE reduction from pruning */
    float awake_lr_multiplier;           /**< LR multiplier when awake */
    float drowsy_lr_multiplier;          /**< LR multiplier when drowsy */
    float nrem_lr_multiplier;            /**< LR multiplier during NREM */
    float rem_lr_multiplier;             /**< LR multiplier during REM */
    bool enable_state_lr_modulation;     /**< Enable sleep state → LR */
    bool enable_consolidation_fe_reduction; /**< Enable consolidation → FE */
    bool enable_homeostasis_precision;   /**< Enable homeostasis → precision */

    /* Sensitivity factors */
    float fe_sensitivity;                /**< FE effect scaling */
    float sleep_sensitivity;             /**< Sleep effect scaling */
} sleep_wake_fep_config_t;

/**
 * @brief FEP effects on sleep-wake
 */
typedef struct {
    /* Free energy effects */
    float current_free_energy;           /**< Current FEP free energy */
    float cumulative_free_energy;        /**< Accumulated FE during wake */
    float fe_sleep_pressure;             /**< Sleep pressure from FE */

    /* Model complexity effects */
    float model_complexity;              /**< Current model complexity */
    float deep_sleep_need;               /**< Deep sleep duration needed */

    /* Surprise effects */
    float current_surprise;              /**< Current surprise level */
    float rem_sleep_need;                /**< REM sleep duration needed */

    /* Combined effects */
    float total_sleep_pressure_boost;   /**< Total pressure boost from FEP */
} sleep_wake_fep_effects_t;

/**
 * @brief Sleep-Wake effects on FEP
 */
typedef struct {
    /* Sleep state */
    sleep_state_t current_state;         /**< Current sleep state */
    float sleep_pressure;                /**< Current sleep pressure */

    /* Learning rate modulation */
    float state_lr_modifier;             /**< LR modifier from sleep state */

    /* Consolidation effects */
    float consolidation_fe_reduction;    /**< FE reduction from consolidation */
    uint32_t patterns_consolidated;      /**< Patterns consolidated this cycle */

    /* Homeostasis effects */
    float pruning_fe_reduction;          /**< FE reduction from pruning */
    float precision_maintenance;         /**< Precision from homeostasis */
} fep_sleep_wake_effects_t;

/**
 * @brief Current state of Sleep-Wake-FEP interaction
 */
typedef struct {
    /* Current values */
    float current_free_energy;           /**< Current FEP free energy */
    float current_sleep_pressure;        /**< Current sleep pressure */
    sleep_state_t current_sleep_state;   /**< Current sleep state */

    /* Applied modifiers */
    float lr_modulation;                 /**< Applied LR modulation */
    float sleep_pressure_boost;          /**< Applied pressure boost */
    float fe_reduction;                  /**< Applied FE reduction */

    /* State flags */
    bool fe_sleep_triggered;             /**< FE triggered sleep */
    bool consolidation_active;           /**< Consolidation in progress */
    bool homeostasis_active;             /**< Homeostasis in progress */

    /* Timestamps */
    uint64_t last_sleep_time;            /**< Last sleep cycle timestamp */
    uint64_t last_consolidation_time;    /**< Last consolidation timestamp */
} sleep_wake_fep_state_t;

/**
 * @brief Statistics for Sleep-Wake-FEP bridge
 */
typedef struct {
    /* FEP → Sleep-Wake */
    uint64_t fe_sleep_triggers;          /**< Times FE triggered sleep */
    uint64_t complexity_deep_sleep_boosts; /**< Complexity-driven deep sleep */
    uint64_t surprise_rem_boosts;        /**< Surprise-driven REM */
    float avg_fe_at_sleep;               /**< Average FE when entering sleep */
    float avg_sleep_pressure_boost;      /**< Average pressure boost */

    /* Sleep-Wake → FEP */
    uint64_t lr_modulation_events;       /**< LR modulation events */
    uint64_t consolidation_cycles;       /**< Consolidation cycles completed */
    uint64_t homeostasis_events;         /**< Homeostasis events */
    float total_fe_reduced;              /**< Total FE reduced by sleep */
    float avg_lr_modifier;               /**< Average LR modifier */

    /* Performance */
    float avg_free_energy;               /**< Average free energy */
    float avg_sleep_quality;             /**< Sleep quality metric */
} sleep_wake_fep_stats_t;

/**
 * @brief Sleep-Wake-FEP bridge state
 */
struct sleep_wake_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    sleep_wake_fep_config_t config;

    /* Connected systems */
    fep_system_t* fep_system;            /**< FEP system */
    sleep_system_t sleep_system;         /**< Sleep-wake system */

    /* Current effects */
    sleep_wake_fep_effects_t fep_effects;     /**< FEP → Sleep-Wake */
    fep_sleep_wake_effects_t sleep_effects;   /**< Sleep-Wake → FEP */
    sleep_wake_fep_state_t state;

    /* Statistics */
    sleep_wake_fep_stats_t stats;

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default Sleep-Wake-FEP configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard thresholds and enable all features
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int sleep_wake_fep_bridge_default_config(sleep_wake_fep_config_t* config);

/**
 * @brief Create Sleep-Wake-FEP bridge
 *
 * WHAT: Initialize Sleep-Wake-FEP integration bridge
 * WHY:  Enable bidirectional Sleep-Wake-FEP interaction
 * HOW:  Allocate bridge, link systems, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
sleep_wake_fep_bridge_t* sleep_wake_fep_bridge_create(
    const sleep_wake_fep_config_t* config
);

/**
 * @brief Destroy Sleep-Wake-FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void sleep_wake_fep_bridge_destroy(sleep_wake_fep_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect FEP system
 *
 * WHAT: Link bridge to FEP system
 * WHY:  Enable FEP state monitoring and modulation
 * HOW:  Store FEP system pointer
 *
 * @param bridge Sleep-Wake-FEP bridge
 * @param fep FEP system
 * @return 0 on success
 */
int sleep_wake_fep_bridge_connect_fep(
    sleep_wake_fep_bridge_t* bridge,
    fep_system_t* fep
);

/**
 * @brief Connect sleep-wake system
 *
 * WHAT: Link bridge to sleep-wake system
 * WHY:  Enable sleep state monitoring and modulation
 * HOW:  Store sleep system handle
 *
 * @param bridge Sleep-Wake-FEP bridge
 * @param sleep Sleep-wake system
 * @return 0 on success
 */
int sleep_wake_fep_bridge_connect_sleep_wake(
    sleep_wake_fep_bridge_t* bridge,
    sleep_system_t sleep
);

/**
 * @brief Disconnect all systems
 *
 * WHAT: Unlink FEP and sleep-wake systems
 * WHY:  Safe shutdown
 * HOW:  Clear system pointers
 *
 * @param bridge Sleep-Wake-FEP bridge
 * @return 0 on success
 */
int sleep_wake_fep_bridge_disconnect(sleep_wake_fep_bridge_t* bridge);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update Sleep-Wake-FEP bridge state
 *
 * WHAT: Main update loop for bidirectional integration
 * WHY:  Keep Sleep-Wake and FEP systems synchronized
 * HOW:  Update effects, apply modulations, check thresholds
 *
 * @param bridge Sleep-Wake-FEP bridge
 * @return 0 on success
 */
int sleep_wake_fep_bridge_update(sleep_wake_fep_bridge_t* bridge);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge Sleep-Wake-FEP bridge
 * @param state Output state
 * @return 0 on success
 */
int sleep_wake_fep_bridge_get_state(
    const sleep_wake_fep_bridge_t* bridge,
    sleep_wake_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Sleep-Wake-FEP bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int sleep_wake_fep_bridge_get_stats(
    const sleep_wake_fep_bridge_t* bridge,
    sleep_wake_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for Sleep-Wake-FEP coordination
 * WHY:  Distributed sleep-FE signaling
 * HOW:  Register module, set up handlers
 *
 * @param bridge Sleep-Wake-FEP bridge
 * @return 0 on success
 */
int sleep_wake_fep_bridge_connect_bio_async(sleep_wake_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Sleep-Wake-FEP bridge
 * @return 0 on success
 */
int sleep_wake_fep_bridge_disconnect_bio_async(sleep_wake_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Sleep-Wake-FEP bridge
 * @return true if bio-async enabled
 */
bool sleep_wake_fep_bridge_is_bio_async_connected(
    const sleep_wake_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SLEEP_WAKE_FEP_BRIDGE_H */
