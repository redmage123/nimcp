/**
 * @file nimcp_autobio_plasticity_bridge.h
 * @brief Autobiographical Memory - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between autobiographical memory and synaptic plasticity
 * WHY:  Enable learning of memory consolidation patterns from experience and feedback
 * HOW:  STDP for episodic associations, BCM for stabilization, reward
 *       modulation for emotionally significant memory strengthening
 *
 * THEORETICAL FOUNDATIONS:
 * - Nadel & Moscovitch (1997): Memory consolidation and hippocampal contribution
 * - McGaugh (2000): Emotional arousal and memory consolidation
 * - Frankland & Bontempi (2005): Systems consolidation theory
 *
 * BIOLOGICAL BASIS:
 * - Hippocampal replay during sleep strengthens episodic memories
 * - Amygdala modulation enhances emotional memory consolidation
 * - Neocortical-hippocampal dialogue for systems consolidation
 * - Repeated retrieval strengthens memory traces
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of memory-context pairs
 * - BCM: Stabilize core autobiographical memories
 * - Homeostatic: Maintain balanced memory accessibility
 * - Reward-modulated: Strengthen emotionally significant memories
 *
 * @see nimcp_autobiographical_memory.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_AUTOBIO_PLASTICITY_BRIDGE_H
#define NIMCP_AUTOBIO_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum memory synapses */
#define AUTOBIO_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define AUTOBIO_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_AUTOBIO_PLASTICITY     0x0D61

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Autobiographical memory synapse types
 */
typedef enum {
    AUTOBIO_SYNAPSE_EPISODIC = 0,        /**< Episodic memory trace */
    AUTOBIO_SYNAPSE_TEMPORAL,             /**< Temporal context (PROTECTED) */
    AUTOBIO_SYNAPSE_EMOTIONAL,            /**< Emotional consolidation */
    AUTOBIO_SYNAPSE_SELF_REFERENCE,       /**< Self-referential processing */
    AUTOBIO_SYNAPSE_CONSOLIDATION,        /**< Memory consolidation (PROTECTED) */
    AUTOBIO_SYNAPSE_RETRIEVAL             /**< Retrieval pathway */
} autobio_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    AUTOBIO_LEARN_ENCODING_SUCCESS = 0,   /**< Memory successfully encoded */
    AUTOBIO_LEARN_ENCODING_WEAK,          /**< Weak encoding (needs strengthening) */
    AUTOBIO_LEARN_RETRIEVAL_SUCCESS,      /**< Successful memory retrieval */
    AUTOBIO_LEARN_RETRIEVAL_FAILURE,      /**< Failed retrieval attempt */
    AUTOBIO_LEARN_EMOTIONAL_BOOST,        /**< Emotional event boosts consolidation */
    AUTOBIO_LEARN_CONSOLIDATION_COMPLETE, /**< Memory consolidation completed */
    AUTOBIO_LEARN_SELF_RELEVANCE_HIGH,    /**< High self-relevance detected */
    AUTOBIO_LEARN_TEMPORAL_LINK,          /**< Temporal context established */
    AUTOBIO_LEARN_CORE_MEMORY,            /**< Core memory status assigned */
    AUTOBIO_LEARN_DECAY_PREVENTED         /**< Memory decay prevented by retrieval */
} autobio_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    AUTOBIO_PLASTICITY_STATE_IDLE = 0,
    AUTOBIO_PLASTICITY_STATE_LEARNING,
    AUTOBIO_PLASTICITY_STATE_CONSOLIDATING,
    AUTOBIO_PLASTICITY_STATE_UPDATING,
    AUTOBIO_PLASTICITY_STATE_ERROR
} autobio_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Autobiographical Memory-Plasticity bridge configuration
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
    float target_memory_strength;        /**< Target memory strength level */

    /* Consolidation modulation */
    float emotional_learning_boost;      /**< Boost for emotional memories */
    float retrieval_learning_boost;      /**< Boost for successful retrieval */
    float self_relevance_modulation;     /**< Self-relevance learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core memory protection */
    bool protect_temporal_context;       /**< Protect temporal context weights */
    bool protect_consolidation;          /**< Protect consolidation weights */
    float protection_strength;           /**< How strongly to protect core memories */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} autobio_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Autobiographical memory synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    autobio_synapse_type_t type;         /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} autobio_plasticity_synapse_t;

/**
 * @brief Memory consolidation state
 */
typedef struct {
    float episodic_strength;             /**< Episodic memory strength */
    float temporal_coherence;            /**< Temporal context coherence */
    float emotional_consolidation;       /**< Emotional consolidation level */
    float self_relevance_strength;       /**< Self-relevance strength */
    float retrieval_ease;                /**< How easily retrieved */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_consolidation_us;      /**< Last consolidation event */
} autobio_consolidation_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    autobio_plasticity_state_t state;    /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} autobio_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t encoding_success_events;    /**< Encoding success events */
    uint64_t retrieval_success_events;   /**< Retrieval success events */
    uint64_t emotional_boost_events;     /**< Emotional boost events */
    uint64_t consolidation_events;       /**< Consolidation events */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} autobio_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct autobio_plasticity_bridge autobio_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*autobio_plasticity_learn_callback_t)(
    autobio_plasticity_bridge_t* bridge,
    autobio_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Consolidation update callback */
typedef void (*autobio_plasticity_consolidation_callback_t)(
    autobio_plasticity_bridge_t* bridge,
    float old_strength,
    float new_strength,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
autobio_plasticity_config_t autobio_plasticity_config_default(void);

/**
 * @brief Create autobiographical memory plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
autobio_plasticity_bridge_t* autobio_plasticity_create(
    const autobio_plasticity_config_t* config
);

/**
 * @brief Destroy autobiographical memory plasticity bridge
 * @param bridge Bridge to destroy
 */
void autobio_plasticity_destroy(autobio_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int autobio_plasticity_reset(autobio_plasticity_bridge_t* bridge);

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
int autobio_plasticity_register_synapse(
    autobio_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    autobio_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int autobio_plasticity_unregister_synapse(
    autobio_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int autobio_plasticity_get_synapse(
    autobio_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    autobio_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int autobio_plasticity_protect_synapse(
    autobio_plasticity_bridge_t* bridge,
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
int autobio_plasticity_learn(
    autobio_plasticity_bridge_t* bridge,
    autobio_learn_event_t event,
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
float autobio_plasticity_apply_stdp(
    autobio_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
);

/**
 * @brief Apply emotional modulation
 * @param bridge Bridge handle
 * @param emotional_intensity Emotional intensity [0, 1]
 * @return 0 on success, -1 on failure
 */
int autobio_plasticity_apply_emotional_boost(
    autobio_plasticity_bridge_t* bridge,
    float emotional_intensity
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int autobio_plasticity_update_bcm(
    autobio_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int autobio_plasticity_homeostatic_update(
    autobio_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int autobio_plasticity_update_traces(
    autobio_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate memory learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int autobio_plasticity_consolidate(autobio_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get consolidation state
 * @param bridge Bridge handle
 * @param state Output consolidation state
 * @return 0 on success, -1 on failure
 */
int autobio_plasticity_get_consolidation_state(
    autobio_plasticity_bridge_t* bridge,
    autobio_consolidation_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int autobio_plasticity_get_state(
    autobio_plasticity_bridge_t* bridge,
    autobio_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int autobio_plasticity_get_stats(
    autobio_plasticity_bridge_t* bridge,
    autobio_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int autobio_plasticity_reset_stats(autobio_plasticity_bridge_t* bridge);

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
int autobio_plasticity_register_learn_callback(
    autobio_plasticity_bridge_t* bridge,
    autobio_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register consolidation update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int autobio_plasticity_register_consolidation_callback(
    autobio_plasticity_bridge_t* bridge,
    autobio_plasticity_consolidation_callback_t callback,
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
int autobio_plasticity_bio_async_connect(autobio_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int autobio_plasticity_bio_async_disconnect(autobio_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool autobio_plasticity_is_bio_async_connected(autobio_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AUTOBIO_PLASTICITY_BRIDGE_H */
