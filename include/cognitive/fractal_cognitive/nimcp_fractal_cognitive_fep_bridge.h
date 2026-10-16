/**
 * @file nimcp_fractal_cognitive_fep_bridge.h
 * @brief Free Energy Principle - Fractal Cognitive Integration Bridge
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between Free Energy Principle and fractal cognitive topology
 * WHY:  Scale-free network structure minimizes complexity in generative models;
 *       FEP prediction errors guide hub neuron discovery and hierarchical structure detection.
 * HOW:  FEP uncertainty drives fractal exploration; hub neurons constrain
 *       generative model precision; hierarchical levels map to FEP levels.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * FRACTAL TOPOLOGY AS FREE ENERGY MINIMIZATION:
 * ---------------------------------------------
 * - Scale-free networks minimize wiring cost (complexity) while maximizing
 *   information flow efficiency (accuracy)
 * - Hub neurons = high precision nodes in FEP framework
 * - Reference: Sporns & Honey (2006) "Small worlds inside big brains"
 *
 * FEP → FRACTAL COGNITIVE PATHWAYS:
 * -------------------------------
 * 1. High Prediction Error → Hub Discovery:
 *    - High PE suggests new information bottlenecks
 *    - Trigger hub neuron re-identification
 *    - Update centrality scores
 *
 * 2. Precision Weights Hub Neurons:
 *    - High precision regions = hub candidates
 *    - Precision-weighted centrality
 *    - Hub strength from FEP confidence
 *
 * 3. Surprise-Driven Hierarchical Reorganization:
 *    - Novel patterns trigger hierarchy updates
 *    - FEP surprise → fractal structure evolution
 *
 * FRACTAL COGNITIVE → FEP PATHWAYS:
 * --------------------------------
 * 1. Hub Neurons Constrain Generative Model:
 *    - Hubs → High precision nodes in FEP
 *    - Centrality → Precision magnitude
 *    - Degree → Belief connectivity
 *
 * 2. Hierarchical Levels Map to FEP Levels:
 *    - Fractal hierarchy → FEP hierarchy
 *    - Top-level hubs → Abstract beliefs
 *    - Low-level nodes → Sensory observations
 *
 * 3. Scale-Free Structure Informs Priors:
 *    - Power-law degree distribution → Prior structure
 *    - Clustering → Local belief coherence
 *    - Small-world → Efficient inference
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FRACTAL_COGNITIVE_FEP_BRIDGE_H
#define NIMCP_FRACTAL_COGNITIVE_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_fractal_cognitive.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FRACTAL_FEP_HIGH_PE_THRESHOLD       6.0f    /**< PE threshold for hub discovery */
#define FRACTAL_FEP_HUB_PRECISION_FACTOR    3.0f    /**< Precision boost for hubs */
#define FRACTAL_FEP_CENTRALITY_PRIOR        0.4f    /**< Centrality prior strength */
#define FRACTAL_FEP_MAX_HUBS                64      /**< Maximum tracked hub neurons */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

typedef struct fractal_cognitive_fep_bridge fractal_cognitive_fep_bridge_t;

/**
 * @brief Configuration for fractal-FEP bridge
 */
typedef struct {
    float pe_exploration_threshold;     /**< PE threshold for hub discovery */
    float precision_hub_factor;         /**< Precision boost for hubs */
    float centrality_prior_strength;    /**< Hub centrality prior weight */
    bool enable_pe_exploration;         /**< Enable PE-driven hub discovery */
    bool enable_hub_precision;          /**< Enable hub precision weighting */
    bool enable_hierarchy_mapping;      /**< Map fractal levels to FEP levels */
    float hierarchy_sensitivity;        /**< Sensitivity to hierarchy changes */
    float hub_belief_strength;          /**< Hub influence on beliefs */
    bool enable_hub_beliefs;            /**< Enable hub-constrained beliefs */
    bool enable_structure_updates;      /**< Enable fractal structure updates */
    float fe_sensitivity;               /**< FEP sensitivity */
    float fractal_sensitivity;          /**< Fractal sensitivity */
} fractal_cognitive_fep_config_t;

/**
 * @brief FEP effects on fractal cognitive system
 */
typedef struct {
    float current_prediction_error;     /**< Current PE magnitude */
    bool hub_discovery_triggered;       /**< Hub re-identification triggered */
    float hub_precision_weights[FRACTAL_FEP_MAX_HUBS]; /**< Precision per hub */
    uint32_t num_hubs_weighted;         /**< Count of weighted hubs */
    float current_surprise;             /**< Current surprise level */
    bool hierarchy_update_active;       /**< Hierarchy reorganization active */
} fractal_cognitive_fep_effects_t;

/**
 * @brief Fractal cognitive effects on FEP
 */
typedef struct {
    float hub_precision_bias;           /**< Hub-based precision bias */
    bool hubs_constraining_model;       /**< Hubs constraining generative model */
    uint32_t num_hierarchy_levels;      /**< Detected hierarchy depth */
    float centrality_prior_strength;    /**< Centrality-based prior */
    float scale_free_factor;            /**< Power-law distribution factor */
    bool model_structure_updated;       /**< FEP structure updated from fractal */
} fep_fractal_cognitive_effects_t;

/**
 * @brief Bridge state
 */
typedef struct {
    float current_prediction_error;     /**< Current PE */
    float current_centrality_mean;      /**< Mean centrality */
    float current_hub_count;            /**< Number of hubs */
    bool hub_discovery_active;          /**< Hub discovery in progress */
    uint32_t num_hierarchy_levels;      /**< Current hierarchy depth */
    uint64_t last_discovery_time;       /**< Last hub discovery time */
    uint64_t last_structure_update_time; /**< Last structure update time */
} fractal_cognitive_fep_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t hub_discovery_events;      /**< Hub discovery events */
    uint64_t structure_updates;         /**< Structure update events */
    uint64_t hierarchy_mappings;        /**< Hierarchy mapping events */
    float avg_prediction_error;         /**< Average PE */
    float avg_centrality;               /**< Average centrality */
    uint64_t precision_applications;    /**< Hub precision applications */
    uint64_t constraint_updates;        /**< FEP constraint updates */
    float avg_free_energy;              /**< Average free energy */
} fractal_cognitive_fep_stats_t;

/**
 * @brief Complete bridge structure
 */
struct fractal_cognitive_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    fractal_cognitive_fep_config_t config;
    fep_system_t* fep_system;
    fractal_cognitive_cache_t* fractal_cache;
    fractal_cognitive_fep_effects_t fep_effects;
    fep_fractal_cognitive_effects_t fractal_effects;
    fractal_cognitive_fep_state_t state;
    fractal_cognitive_fep_stats_t stats;
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Initialize default fractal-FEP bridge configuration
 * WHY:  Provide sensible defaults for scale-free network integration
 * HOW:  Set biologically-plausible parameters
 *
 * @param config Output configuration
 * @return 0 on success, NIMCP_ERROR_NULL_POINTER on error
 */
int fractal_cognitive_fep_bridge_default_config(fractal_cognitive_fep_config_t* config);

/**
 * @brief Create fractal-FEP bridge
 *
 * WHAT: Initialize bidirectional integration infrastructure
 * WHY:  Enable FEP-fractal cognitive coupling
 * HOW:  Allocate structure, create mutex, set config
 *
 * @param config Configuration (NULL = defaults)
 * @return Bridge instance or NULL on failure
 */
fractal_cognitive_fep_bridge_t* fractal_cognitive_fep_bridge_create(
    const fractal_cognitive_fep_config_t* config
);

/**
 * @brief Destroy fractal-FEP bridge
 *
 * WHAT: Clean up resources
 * WHY:  Prevent memory leaks
 * HOW:  Disconnect bio-async, destroy mutex, free structure
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void fractal_cognitive_fep_bridge_destroy(fractal_cognitive_fep_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect FEP system to bridge
 *
 * @param bridge Bridge instance
 * @param fep FEP system
 * @return 0 on success
 */
int fractal_cognitive_fep_bridge_connect_fep(
    fractal_cognitive_fep_bridge_t* bridge,
    fep_system_t* fep
);

/**
 * @brief Connect fractal cognitive cache to bridge
 *
 * @param bridge Bridge instance
 * @param fractal_cache Fractal cognitive cache
 * @return 0 on success
 */
int fractal_cognitive_fep_bridge_connect_fractal(
    fractal_cognitive_fep_bridge_t* bridge,
    fractal_cognitive_cache_t* fractal_cache
);

/**
 * @brief Disconnect both systems
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int fractal_cognitive_fep_bridge_disconnect(fractal_cognitive_fep_bridge_t* bridge);

/* ============================================================================
 * FEP → Fractal Cognitive API
 * ============================================================================ */

/**
 * @brief Trigger hub discovery based on prediction error
 *
 * WHAT: Re-identify hub neurons when PE is high
 * WHY:  High PE suggests current hub structure inadequate
 * HOW:  If PE > threshold, trigger hub re-identification
 *
 * @param bridge Bridge instance
 * @param pe_magnitude Prediction error magnitude
 * @return 0 on success
 */
int fractal_cognitive_fep_trigger_hub_discovery(
    fractal_cognitive_fep_bridge_t* bridge,
    float pe_magnitude
);

/**
 * @brief Weight hub neurons by FEP precision
 *
 * WHAT: Apply FEP precision to hub importance
 * WHY:  High precision regions are critical hubs
 * HOW:  Boost hub weights by precision values
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int fractal_cognitive_fep_weight_hubs_by_precision(
    fractal_cognitive_fep_bridge_t* bridge
);

/**
 * @brief Trigger hierarchical reorganization
 *
 * WHAT: Update fractal hierarchy structure
 * WHY:  FEP surprise requires structural adaptation
 * HOW:  Signal hierarchy re-detection
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int fractal_cognitive_fep_trigger_hierarchy_update(
    fractal_cognitive_fep_bridge_t* bridge
);

/* ============================================================================
 * Fractal Cognitive → FEP API
 * ============================================================================ */

/**
 * @brief Apply hub structure as FEP priors
 *
 * WHAT: Use hub centrality as precision priors
 * WHY:  Hub structure constrains generative model
 * HOW:  Map centrality to FEP precision
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int fractal_cognitive_fep_apply_hub_priors(fractal_cognitive_fep_bridge_t* bridge);

/**
 * @brief Map fractal hierarchy to FEP levels
 *
 * WHAT: Align fractal levels with FEP hierarchy
 * WHY:  Hierarchical structure shapes beliefs
 * HOW:  Map fractal levels to FEP state levels
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int fractal_cognitive_fep_map_hierarchy_to_fep(fractal_cognitive_fep_bridge_t* bridge);

/**
 * @brief Update FEP model structure from fractal topology
 *
 * WHAT: Reshape FEP generative model
 * WHY:  Network architecture determines model structure
 * HOW:  Scale-free structure → FEP connectivity
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int fractal_cognitive_fep_update_model_structure(
    fractal_cognitive_fep_bridge_t* bridge
);

/* ============================================================================
 * Update and Query API
 * ============================================================================ */

/**
 * @brief Periodic bridge update
 *
 * WHAT: Continuous synchronization
 * WHY:  Maintain bidirectional coupling
 * HOW:  Apply all effects
 *
 * @param bridge Bridge instance
 * @param delta_ms Time delta in milliseconds
 * @return 0 on success
 */
int fractal_cognitive_fep_bridge_update(
    fractal_cognitive_fep_bridge_t* bridge,
    uint64_t delta_ms
);

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge instance
 * @param state Output state
 * @return 0 on success
 */
int fractal_cognitive_fep_bridge_get_state(
    const fractal_cognitive_fep_bridge_t* bridge,
    fractal_cognitive_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics
 * @return 0 on success
 */
int fractal_cognitive_fep_bridge_get_stats(
    const fractal_cognitive_fep_bridge_t* bridge,
    fractal_cognitive_fep_stats_t* stats
);

/* ============================================================================
 * Bio-async Integration API
 * ============================================================================ */

/**
 * @brief Register with bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int fractal_cognitive_fep_bridge_connect_bio_async(
    fractal_cognitive_fep_bridge_t* bridge
);

/**
 * @brief Unregister from bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int fractal_cognitive_fep_bridge_disconnect_bio_async(
    fractal_cognitive_fep_bridge_t* bridge
);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool fractal_cognitive_fep_bridge_is_bio_async_connected(
    const fractal_cognitive_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FRACTAL_COGNITIVE_FEP_BRIDGE_H */
