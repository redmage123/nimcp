/**
 * @file nimcp_knowledge_plasticity_bridge.h
 * @brief Knowledge - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between knowledge system and synaptic plasticity
 * WHY:  Enable learning and strengthening of semantic associations from experience
 * HOW:  STDP for concept-association timing, BCM for knowledge stabilization,
 *       reward modulation for retrieval success learning
 *
 * THEORETICAL FOUNDATIONS:
 * - McClelland et al. (1995): Complementary learning systems
 * - Kumaran & McClelland (2012): Semantic memory consolidation
 * - Rogers & McClelland (2004): Semantic cognition learning
 *
 * BIOLOGICAL BASIS:
 * - Medial temporal lobe for knowledge encoding plasticity
 * - Neocortical consolidation for long-term semantic storage
 * - Hippocampal-cortical replay strengthens semantic connections
 * - Retrieval practice enhances knowledge retention
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of concept-relation pairs
 * - BCM: Stabilize core semantic knowledge
 * - Homeostatic: Maintain balanced knowledge activation
 * - Reward-modulated: Learn from retrieval success
 *
 * @see nimcp_knowledge.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_KNOWLEDGE_PLASTICITY_BRIDGE_H
#define NIMCP_KNOWLEDGE_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum knowledge synapses */
#define KNOWLEDGE_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define KNOWLEDGE_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_KNOWLEDGE_PLASTICITY     0x0D61

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Knowledge synapse types
 */
typedef enum {
    KNOWLEDGE_SYNAPSE_SEMANTIC = 0,      /**< Semantic encoding */
    KNOWLEDGE_SYNAPSE_RETRIEVAL,          /**< Retrieval pathway (PROTECTED) */
    KNOWLEDGE_SYNAPSE_ASSOCIATION,        /**< Association links */
    KNOWLEDGE_SYNAPSE_CATEGORICAL,        /**< Categorical organization */
    KNOWLEDGE_SYNAPSE_HIERARCHICAL,       /**< Hierarchical structure */
    KNOWLEDGE_SYNAPSE_CONFIDENCE          /**< Confidence pathway (PROTECTED) */
} knowledge_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    KNOWLEDGE_LEARN_RETRIEVAL_SUCCESS = 0, /**< Successful knowledge retrieval */
    KNOWLEDGE_LEARN_RETRIEVAL_FAILURE,     /**< Failed retrieval attempt */
    KNOWLEDGE_LEARN_ASSOCIATION_FORMED,    /**< New association formed */
    KNOWLEDGE_LEARN_ASSOCIATION_WEAK,      /**< Weak association detected */
    KNOWLEDGE_LEARN_ENCODING_STRONG,       /**< Strong semantic encoding */
    KNOWLEDGE_LEARN_ENCODING_WEAK,         /**< Weak semantic encoding */
    KNOWLEDGE_LEARN_CATEGORY_MATCHED,      /**< Category prediction matched */
    KNOWLEDGE_LEARN_CONSOLIDATION,         /**< Knowledge consolidation event */
    KNOWLEDGE_LEARN_INTERFERENCE,          /**< Knowledge interference detected */
    KNOWLEDGE_LEARN_REINFORCEMENT          /**< Knowledge reinforcement */
} knowledge_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    KNOWLEDGE_PLASTICITY_STATE_IDLE = 0,
    KNOWLEDGE_PLASTICITY_STATE_LEARNING,
    KNOWLEDGE_PLASTICITY_STATE_CONSOLIDATING,
    KNOWLEDGE_PLASTICITY_STATE_UPDATING,
    KNOWLEDGE_PLASTICITY_STATE_ERROR
} knowledge_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Knowledge-Plasticity bridge configuration
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
    float target_activation;             /**< Target knowledge activation level */

    /* Reward modulation */
    float retrieval_success_boost;       /**< Boost for successful retrieval */
    float association_learning_boost;    /**< Boost for association formation */
    float semantic_modulation;           /**< Semantic encoding strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_retrieval_pathway;      /**< Protect retrieval pathway weights */
    bool protect_confidence_pathway;     /**< Protect confidence pathway weights */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} knowledge_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Knowledge synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    knowledge_synapse_type_t type;       /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} knowledge_plasticity_synapse_t;

/**
 * @brief Knowledge consolidation state
 */
typedef struct {
    float semantic_sensitivity;          /**< Sensitivity to semantic encoding */
    float retrieval_calibration;         /**< Retrieval calibration level */
    float association_sensitivity;       /**< Sensitivity to associations */
    float categorical_strength;          /**< Categorical organization strength */
    float hierarchical_strength;         /**< Hierarchical structure strength */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} knowledge_consolidation_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    knowledge_plasticity_state_t state;  /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} knowledge_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t retrieval_success_events;   /**< Retrieval success events */
    uint64_t retrieval_failure_events;   /**< Retrieval failure events */
    uint64_t association_formed_events;  /**< Association formation events */
    uint64_t consolidation_events;       /**< Consolidation events */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} knowledge_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct knowledge_plasticity_bridge knowledge_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*knowledge_plasticity_learn_callback_t)(
    knowledge_plasticity_bridge_t* bridge,
    knowledge_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Consolidation update callback */
typedef void (*knowledge_plasticity_consolidation_callback_t)(
    knowledge_plasticity_bridge_t* bridge,
    float old_activation,
    float new_activation,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
knowledge_plasticity_config_t knowledge_plasticity_config_default(void);

/**
 * @brief Create knowledge plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
knowledge_plasticity_bridge_t* knowledge_plasticity_create(
    const knowledge_plasticity_config_t* config
);

/**
 * @brief Destroy knowledge plasticity bridge
 * @param bridge Bridge to destroy
 */
void knowledge_plasticity_destroy(knowledge_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int knowledge_plasticity_reset(knowledge_plasticity_bridge_t* bridge);

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
int knowledge_plasticity_register_synapse(
    knowledge_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    knowledge_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int knowledge_plasticity_unregister_synapse(
    knowledge_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int knowledge_plasticity_get_synapse(
    knowledge_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    knowledge_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int knowledge_plasticity_protect_synapse(
    knowledge_plasticity_bridge_t* bridge,
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
int knowledge_plasticity_learn(
    knowledge_plasticity_bridge_t* bridge,
    knowledge_learn_event_t event,
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
float knowledge_plasticity_apply_stdp(
    knowledge_plasticity_bridge_t* bridge,
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
int knowledge_plasticity_apply_reward(
    knowledge_plasticity_bridge_t* bridge,
    float reward
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int knowledge_plasticity_update_bcm(
    knowledge_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int knowledge_plasticity_homeostatic_update(
    knowledge_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int knowledge_plasticity_update_traces(
    knowledge_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int knowledge_plasticity_consolidate(knowledge_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get consolidation state
 * @param bridge Bridge handle
 * @param state Output consolidation state
 * @return 0 on success, -1 on failure
 */
int knowledge_plasticity_get_consolidation_state(
    knowledge_plasticity_bridge_t* bridge,
    knowledge_consolidation_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int knowledge_plasticity_get_state(
    knowledge_plasticity_bridge_t* bridge,
    knowledge_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int knowledge_plasticity_get_stats(
    knowledge_plasticity_bridge_t* bridge,
    knowledge_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int knowledge_plasticity_reset_stats(knowledge_plasticity_bridge_t* bridge);

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
int knowledge_plasticity_register_learn_callback(
    knowledge_plasticity_bridge_t* bridge,
    knowledge_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register consolidation update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int knowledge_plasticity_register_consolidation_callback(
    knowledge_plasticity_bridge_t* bridge,
    knowledge_plasticity_consolidation_callback_t callback,
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
int knowledge_plasticity_bio_async_connect(knowledge_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int knowledge_plasticity_bio_async_disconnect(knowledge_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool knowledge_plasticity_is_bio_async_connected(knowledge_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KNOWLEDGE_PLASTICITY_BRIDGE_H */
