/**
 * @file nimcp_swarm_brain_fep_bridge.h
 * @brief FEP Bridge for Swarm Brain Coordinator
 *
 * WHAT: Free Energy Principle integration for swarm brain coordination system
 * WHY:  Enable multi-agent free energy minimization and collective inference
 * HOW:  Bidirectional modulation between swarm brain state and FEP beliefs
 *
 * BIOLOGICAL BASIS:
 * - Swarm as multi-agent generative model
 * - Collective perception minimizes collective prediction error
 * - Drone coordination as coordinated precision-weighting
 * - Emergence tiers as hierarchical model depth
 * - Coherence as model agreement (minimize collective surprise)
 *
 * FEP INTERPRETATION:
 * - Observations: Aggregated swarm sensor data
 * - Hidden states: Swarm formation, goals, threats
 * - Actions: Formation changes, movement commands
 * - Prediction errors: Mismatch between expected and actual swarm state
 * - Free energy: Collective uncertainty about environment
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 */

#ifndef NIMCP_SWARM_BRAIN_FEP_BRIDGE_H
#define NIMCP_SWARM_BRAIN_FEP_BRIDGE_H

#include "swarm/nimcp_swarm_brain.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief FEP bridge configuration for swarm brain
 */
typedef struct {
    float collective_pe_weight;      /**< Weight for collective prediction error */
    float coherence_precision_gain;  /**< Precision boost from high coherence */
    float emergence_lr_scale;        /**< Learning rate scaling by emergence tier */
    float formation_prior_strength;  /**< Prior strength for formation states */
    bool enable_collective_inference; /**< Enable multi-agent inference */
    bool enable_emergence_scaling;   /**< Scale FEP by emergence tier */
} swarm_brain_fep_config_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief FEP effects on swarm brain
 *
 * WHAT: How FEP modulates swarm coordination
 * WHY:  Prediction error drives formation/behavior changes
 * HOW:  High free energy → adapt swarm strategy
 */
typedef struct {
    float coordination_adjustment;    /**< Formation tightness adjustment [-1,1] */
    float communication_urgency;      /**< Message priority boost [0,1] */
    float exploration_bias;           /**< Exploration vs exploitation [0,1] */
    float tier_advancement_threshold; /**< Threshold for tier advancement */
} swarm_brain_fep_effects_t;

/**
 * @brief Swarm brain effects on FEP
 *
 * WHAT: How swarm state modulates FEP processing
 * WHY:  Swarm coherence affects belief precision
 * HOW:  High coherence → high precision beliefs
 */
typedef struct {
    float precision_modulation;       /**< Precision scaling [0.5, 2.0] */
    float learning_rate_modulation;   /**< Learning rate scaling [0.5, 2.0] */
    uint32_t hierarchy_depth;         /**< Effective FEP hierarchy depth */
    float collective_confidence;      /**< Collective belief confidence [0,1] */
} fep_swarm_brain_effects_t;

/* ============================================================================
 * State Tracking
 * ============================================================================ */

/**
 * @brief Swarm brain FEP bridge state
 */
typedef struct {
    float last_collective_free_energy; /**< Last collective FE */
    float last_coherence;              /**< Last swarm coherence */
    swarm_emergence_tier_t last_tier;  /**< Last emergence tier */
    uint32_t update_count;             /**< Number of updates */
    uint64_t last_update_time;         /**< Last update timestamp */
} swarm_brain_fep_state_t;

/**
 * @brief Swarm brain FEP statistics
 */
typedef struct {
    uint64_t total_updates;            /**< Total FEP updates */
    float avg_collective_fe;           /**< Average collective free energy */
    float min_collective_fe;           /**< Minimum collective FE */
    float max_collective_fe;           /**< Maximum collective FE */
    uint32_t tier_changes;             /**< Number of tier changes */
    uint32_t formation_adaptations;    /**< Formation changes driven by FEP */
} swarm_brain_fep_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Swarm brain FEP bridge
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    swarm_brain_fep_config_t config;
    fep_system_t* fep_system;
    swarm_brain_t* swarm_brain;
    swarm_brain_fep_effects_t fep_effects;
    fep_swarm_brain_effects_t swarm_effects;
    swarm_brain_fep_state_t state;
    swarm_brain_fep_stats_t stats;} swarm_brain_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
void swarm_brain_fep_default_config(swarm_brain_fep_config_t* config);

/**
 * @brief Create swarm brain FEP bridge
 */
swarm_brain_fep_bridge_t* swarm_brain_fep_create(
    const swarm_brain_fep_config_t* config,
    swarm_brain_t* swarm_brain,
    fep_system_t* fep_system
);

/**
 * @brief Destroy swarm brain FEP bridge
 */
void swarm_brain_fep_destroy(swarm_brain_fep_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update FEP bridge
 *
 * WHAT: Compute bidirectional FEP-swarm modulation
 * WHY:  Synchronize free energy and swarm coordination
 * HOW:  Read swarm state, update FEP, compute effects
 */
int swarm_brain_fep_update(swarm_brain_fep_bridge_t* bridge);

/**
 * @brief Apply FEP modulation to swarm brain
 */
int swarm_brain_fep_apply_modulation(swarm_brain_fep_bridge_t* bridge);

/**
 * @brief Compute collective free energy across swarm
 *
 * WHAT: Aggregate free energy from all drones
 * WHY:  Measure collective uncertainty
 * HOW:  Weighted sum of individual FE values
 */
float swarm_brain_fep_compute_collective_fe(swarm_brain_fep_bridge_t* bridge);

/**
 * @brief Process swarm observation through FEP
 *
 * WHAT: Update FEP beliefs given swarm sensor data
 * WHY:  Collective perception as inference
 * HOW:  Aggregate observations, compute prediction error
 */
int swarm_brain_fep_process_collective_observation(
    swarm_brain_fep_bridge_t* bridge,
    const float* observation,
    uint32_t observation_dim
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get FEP effects on swarm brain
 */
int swarm_brain_fep_get_effects(
    const swarm_brain_fep_bridge_t* bridge,
    swarm_brain_fep_effects_t* effects
);

/**
 * @brief Get swarm effects on FEP
 */
int swarm_brain_fep_get_swarm_effects(
    const swarm_brain_fep_bridge_t* bridge,
    fep_swarm_brain_effects_t* effects
);

/**
 * @brief Get bridge statistics
 */
int swarm_brain_fep_get_stats(
    const swarm_brain_fep_bridge_t* bridge,
    swarm_brain_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 */
int swarm_brain_fep_connect_bio_async(swarm_brain_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 */
int swarm_brain_fep_disconnect_bio_async(swarm_brain_fep_bridge_t* bridge);

/**
 * @brief Check if connected to bio-async
 */
bool swarm_brain_fep_is_bio_async_connected(const swarm_brain_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_BRAIN_FEP_BRIDGE_H */
