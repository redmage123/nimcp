/**
 * @file nimcp_parietal_plasticity_bridge.h
 * @brief Parietal - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between parietal lobe and synaptic plasticity
 * WHY:  Enable learning of spatial representations, numerical processing,
 *       and visuospatial skills from experience and feedback
 * HOW:  STDP for spatial-temporal associations, BCM for stabilization, reward
 *       modulation for skill learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Dehaene & Cohen (2007): Cultural recycling of cortical maps
 * - Buonomano & Merzenich (1998): Cortical plasticity in parietal cortex
 * - Colby & Duhamel (1996): Spatial representations in parietal cortex
 *
 * BIOLOGICAL BASIS:
 * - Posterior parietal cortex plasticity for spatial learning
 * - Intraparietal sulcus adaptation for numerical processing
 * - Superior parietal lobule plasticity for visuomotor integration
 * - Cross-modal plasticity in multi-sensory integration
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of spatial-motor pairs
 * - BCM: Stabilize core spatial representations
 * - Homeostatic: Maintain balanced parietal activity
 * - Reward-modulated: Learn from task success
 *
 * @see nimcp_parietal.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_PARIETAL_PLASTICITY_BRIDGE_H
#define NIMCP_PARIETAL_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum parietal synapses */
#define PARIETAL_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define PARIETAL_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_PARIETAL_PLASTICITY     0x0D61

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Parietal synapse types
 */
typedef enum {
    PARIETAL_SYNAPSE_SPATIAL_ATTENTION = 0, /**< Spatial attention (PROTECTED) */
    PARIETAL_SYNAPSE_NUMERICAL,              /**< Numerical magnitude */
    PARIETAL_SYNAPSE_MULTISENSORY,           /**< Multi-sensory integration */
    PARIETAL_SYNAPSE_BODY_SCHEMA,            /**< Body schema (PROTECTED) */
    PARIETAL_SYNAPSE_VISUOSPATIAL,           /**< Visuospatial processing */
    PARIETAL_SYNAPSE_COORDINATE_TRANSFORM    /**< Coordinate transformation */
} parietal_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    PARIETAL_LEARN_ATTENTION_SHIFT = 0,    /**< Successful attention shift */
    PARIETAL_LEARN_ATTENTION_ERROR,         /**< Attention prediction error */
    PARIETAL_LEARN_MAGNITUDE_ACCURATE,      /**< Accurate numerical judgment */
    PARIETAL_LEARN_MAGNITUDE_ERROR,         /**< Numerical magnitude error */
    PARIETAL_LEARN_SPATIAL_SUCCESS,         /**< Successful spatial task */
    PARIETAL_LEARN_SPATIAL_FAILURE,         /**< Failed spatial task */
    PARIETAL_LEARN_INTEGRATION_MATCHED,     /**< Multi-sensory integration matched */
    PARIETAL_LEARN_ROTATION_SUCCESS,        /**< Mental rotation success */
    PARIETAL_LEARN_ROTATION_FAILURE,        /**< Mental rotation failure */
    PARIETAL_LEARN_TRANSFORM_COMPLETE       /**< Coordinate transform completed */
} parietal_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    PARIETAL_PLASTICITY_STATE_IDLE = 0,
    PARIETAL_PLASTICITY_STATE_LEARNING,
    PARIETAL_PLASTICITY_STATE_CONSOLIDATING,
    PARIETAL_PLASTICITY_STATE_UPDATING,
    PARIETAL_PLASTICITY_STATE_ERROR
} parietal_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Parietal-Plasticity bridge configuration
 */
typedef struct {
    /* Learning parameters */
    float base_learning_rate;            /**< Base learning rate */
    float stdp_tau_plus_ms;              /**< STDP potentiation time constant */
    float stdp_tau_minus_ms;             /**< STDP depression time constant */
    float stdp_a_plus;                   /**< STDP potentiation magnitude */
    float stdp_a_minus;                  /**< STDP depression magnitude */

    /* BCM parameters */
    float bcm_tau_ms;                    /**< BCM threshold time constant */
    float bcm_target_rate;               /**< BCM target activity */

    /* Homeostatic parameters */
    float homeostatic_tau_ms;            /**< Homeostatic time constant */
    float target_spatial_activity;       /**< Target spatial activity level */

    /* Reward modulation */
    float spatial_learning_boost;        /**< Boost for spatial task success */
    float numerical_learning_boost;      /**< Boost for numerical accuracy */
    float attention_modulation;          /**< Attention learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_spatial_attention;      /**< Protect spatial attention weights */
    bool protect_body_schema;            /**< Protect body schema weights */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} parietal_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Parietal synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    parietal_synapse_type_t type;        /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} parietal_plasticity_synapse_t;

/**
 * @brief Spatial processing learning state
 */
typedef struct {
    float attention_sensitivity;         /**< Sensitivity to attention shifts */
    float spatial_calibration;           /**< Spatial processing calibration */
    float numerical_sensitivity;         /**< Sensitivity to numerical magnitude */
    float integration_strength;          /**< Multi-sensory integration strength */
    float rotation_strength;             /**< Mental rotation strength */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} parietal_spatial_learning_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    parietal_plasticity_state_t state;   /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} parietal_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t attention_shift_events;     /**< Attention shift learning events */
    uint64_t attention_error_events;     /**< Attention error corrections */
    uint64_t magnitude_accurate_events;  /**< Accurate magnitude events */
    uint64_t spatial_success_events;     /**< Spatial task success learning */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} parietal_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct parietal_plasticity_bridge parietal_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*parietal_plasticity_learn_callback_t)(
    parietal_plasticity_bridge_t* bridge,
    parietal_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Spatial update callback */
typedef void (*parietal_plasticity_spatial_callback_t)(
    parietal_plasticity_bridge_t* bridge,
    float old_spatial,
    float new_spatial,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
parietal_plasticity_config_t parietal_plasticity_config_default(void);

/**
 * @brief Create parietal plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
parietal_plasticity_bridge_t* parietal_plasticity_create(
    const parietal_plasticity_config_t* config
);

/**
 * @brief Destroy parietal plasticity bridge
 * @param bridge Bridge to destroy
 */
void parietal_plasticity_destroy(parietal_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int parietal_plasticity_reset(parietal_plasticity_bridge_t* bridge);

//=============================================================================
// Synapse Management
//=============================================================================

/**
 * @brief Register a synapse for plasticity tracking
 * @param bridge Bridge handle
 * @param synapse_id Unique synapse ID
 * @param type Synapse type
 * @param initial_weight Initial weight
 * @return 0 on success, -1 on failure
 */
int parietal_plasticity_register_synapse(
    parietal_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    parietal_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int parietal_plasticity_unregister_synapse(
    parietal_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int parietal_plasticity_get_synapse(
    parietal_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    parietal_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int parietal_plasticity_protect_synapse(
    parietal_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    bool protect
);

//=============================================================================
// Learning Functions
//=============================================================================

/**
 * @brief Apply learning event
 * @param bridge Bridge handle
 * @param event Event type
 * @param magnitude Event magnitude [0-1]
 * @param synapse_id Target synapse
 * @param context Context strength
 * @return 0 on success, -1 on failure
 */
int parietal_plasticity_learn(
    parietal_plasticity_bridge_t* bridge,
    parietal_learn_event_t event,
    float magnitude,
    uint32_t synapse_id,
    float context
);

/**
 * @brief Apply STDP to synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param pre_time Pre-synaptic spike time (ms)
 * @param post_time Post-synaptic spike time (ms)
 * @return Weight change, NAN on failure
 */
float parietal_plasticity_apply_stdp(
    parietal_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
);

/**
 * @brief Apply reward modulation
 * @param bridge Bridge handle
 * @param reward Reward signal [-1, 1]
 * @return 0 on success, -1 on failure
 */
int parietal_plasticity_apply_reward(
    parietal_plasticity_bridge_t* bridge,
    float reward
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int parietal_plasticity_update_bcm(
    parietal_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int parietal_plasticity_homeostatic_update(
    parietal_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int parietal_plasticity_update_traces(
    parietal_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int parietal_plasticity_consolidate(parietal_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get spatial learning state
 * @param bridge Bridge handle
 * @param state Output spatial learning state
 * @return 0 on success, -1 on failure
 */
int parietal_plasticity_get_spatial_state(
    parietal_plasticity_bridge_t* bridge,
    parietal_spatial_learning_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int parietal_plasticity_get_state(
    parietal_plasticity_bridge_t* bridge,
    parietal_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int parietal_plasticity_get_stats(
    parietal_plasticity_bridge_t* bridge,
    parietal_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int parietal_plasticity_reset_stats(parietal_plasticity_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register learning event callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int parietal_plasticity_register_learn_callback(
    parietal_plasticity_bridge_t* bridge,
    parietal_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register spatial update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int parietal_plasticity_register_spatial_callback(
    parietal_plasticity_bridge_t* bridge,
    parietal_plasticity_spatial_callback_t callback,
    void* user_data
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect to bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int parietal_plasticity_bio_async_connect(parietal_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int parietal_plasticity_bio_async_disconnect(parietal_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool parietal_plasticity_is_bio_async_connected(parietal_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PARIETAL_PLASTICITY_BRIDGE_H */
