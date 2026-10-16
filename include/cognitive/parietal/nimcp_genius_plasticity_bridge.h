/**
 * @file nimcp_genius_plasticity_bridge.h
 * @brief Mathematical Genius - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-24
 *
 * WHAT: Bidirectional bridge between mathematical genius module and synaptic plasticity
 * WHY:  Enable learning of mathematical patterns, theorem proving strategies, and
 *       conjecture generation through STDP/BCM plasticity mechanisms
 * HOW:  STDP for associating mathematical concepts, BCM for stabilizing proven
 *       insights, reward modulation for successful proof discovery
 *
 * THEORETICAL FOUNDATIONS:
 * - Dehaene (2011): Learning and plasticity in mathematical cognition
 * - Butterworth (2005): Developmental dyscalculia and mathematical learning
 * - Nieder & Dehaene (2009): Representation of number in brain and behavioral training
 *
 * BIOLOGICAL BASIS:
 * - Parietal cortex plasticity for mathematical concept learning
 * - Prefrontal-parietal synaptic strengthening for proof strategies
 * - Hippocampal-cortical plasticity for mathematical memory
 * - Angular gyrus adaptation for symbolic processing
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of proof steps
 * - BCM: Stabilize proven theorems and valid patterns
 * - Reward-modulated: Learn from proof success
 * - Homeostatic: Maintain balanced mathematical activity
 *
 * @see nimcp_mathematical_genius.h
 * @see nimcp_plasticity.h
 * @see nimcp_stdp.h
 */

#ifndef NIMCP_GENIUS_PLASTICITY_BRIDGE_H
#define NIMCP_GENIUS_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "cognitive/parietal/nimcp_genius_modes.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum genius synapses */
#define GENIUS_PLASTICITY_MAX_SYNAPSES     512

/** @brief Default learning rate */
#define GENIUS_PLASTICITY_DEFAULT_LR       0.005f

/** @brief Bio-async module ID */
#define BIO_MODULE_GENIUS_PLASTICITY       0x0399

//=============================================================================
// KG Wiring Constants
//=============================================================================

/** @brief KG module name */
#define KG_GENIUS_PLASTICITY_MODULE_NAME   "genius_plasticity_bridge"

/** @brief KG module type */
#define KG_GENIUS_PLASTICITY_MODULE_TYPE   "MATHEMATICAL_LEARNING"

/* Input message types */
#define KG_MSG_PLASTICITY_PROOF_RESULT     "PROOF_RESULT"
#define KG_MSG_PLASTICITY_INSIGHT_EVENT    "INSIGHT_EVENT"
#define KG_MSG_PLASTICITY_REWARD_SIGNAL    "REWARD_SIGNAL"
#define KG_MSG_PLASTICITY_MODE_FEEDBACK    "MODE_FEEDBACK"

/* Output message types */
#define KG_MSG_PLASTICITY_WEIGHT_UPDATE    "WEIGHT_UPDATE"
#define KG_MSG_PLASTICITY_SKILL_CHANGE     "SKILL_CHANGE"
#define KG_MSG_PLASTICITY_CONSOLIDATION    "CONSOLIDATION_EVENT"
#define KG_MSG_PLASTICITY_HOMEOSTASIS      "HOMEOSTASIS_EVENT"

/* Handler message types */
#define KG_MSG_PLASTICITY_LEARN_EVENT      "LEARN_EVENT"
#define KG_MSG_PLASTICITY_STDP_UPDATE      "STDP_UPDATE"
#define KG_MSG_PLASTICITY_BCM_UPDATE       "BCM_UPDATE"

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Genius synapse types
 */
typedef enum {
    GENIUS_SYNAPSE_PATTERN_RECOGNITION = 0, /**< Pattern recognition (PROTECTED) */
    GENIUS_SYNAPSE_PROOF_STEP,               /**< Proof step associations */
    GENIUS_SYNAPSE_CONJECTURE,               /**< Conjecture formation */
    GENIUS_SYNAPSE_ANALOGY,                  /**< Cross-domain analogies */
    GENIUS_SYNAPSE_INTUITION,                /**< Mathematical intuition (PROTECTED) */
    GENIUS_SYNAPSE_MODE_SELECTION,           /**< Genius mode selection */
    GENIUS_SYNAPSE_ELEGANCE,                 /**< Elegance perception */
    GENIUS_SYNAPSE_RIGOR                     /**< Proof rigor */
} genius_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    GENIUS_LEARN_PROOF_SUCCESS = 0,        /**< Successful proof completion */
    GENIUS_LEARN_PROOF_FAILURE,            /**< Failed proof attempt */
    GENIUS_LEARN_PATTERN_FOUND,            /**< Pattern successfully found */
    GENIUS_LEARN_PATTERN_MISSED,           /**< Missed obvious pattern */
    GENIUS_LEARN_CONJECTURE_VERIFIED,      /**< Conjecture verified */
    GENIUS_LEARN_CONJECTURE_REFUTED,       /**< Conjecture refuted by counter-example */
    GENIUS_LEARN_INSIGHT_EMERGED,          /**< New insight emerged */
    GENIUS_LEARN_ANALOGY_FOUND,            /**< Cross-domain analogy discovered */
    GENIUS_LEARN_MODE_EFFECTIVE,           /**< Mode choice was effective */
    GENIUS_LEARN_MODE_INEFFECTIVE,         /**< Mode choice was ineffective */
    GENIUS_LEARN_ELEGANCE_ACHIEVED,        /**< Elegant proof achieved */
    GENIUS_LEARN_BREAKTHROUGH              /**< Major mathematical breakthrough */
} genius_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    GENIUS_PLASTICITY_STATE_IDLE = 0,
    GENIUS_PLASTICITY_STATE_LEARNING,
    GENIUS_PLASTICITY_STATE_CONSOLIDATING,
    GENIUS_PLASTICITY_STATE_UPDATING,
    GENIUS_PLASTICITY_STATE_ERROR
} genius_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Genius-Plasticity bridge configuration
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
    float target_insight_rate;           /**< Target insight emergence rate */

    /* Reward modulation */
    float proof_success_boost;           /**< Boost for proof success */
    float pattern_found_boost;           /**< Boost for pattern discovery */
    float insight_modulation;            /**< Insight learning strength */
    float breakthrough_boost;            /**< Major boost for breakthroughs */
    float elegance_bonus;                /**< Bonus for elegant proofs */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_pattern_recognition;    /**< Protect pattern recognition */
    bool protect_intuition;              /**< Protect mathematical intuition */
    float protection_strength;           /**< How strongly to protect */

    /* Mode-specific learning */
    float gauss_learning_rate;           /**< Gauss mode learning rate */
    float newton_learning_rate;          /**< Newton mode learning rate */
    float erdos_learning_rate;           /**< Erdos mode learning rate */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} genius_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Genius synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    genius_synapse_type_t type;          /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
    genius_mode_t associated_mode;       /**< Associated genius mode */
} genius_plasticity_synapse_t;

/**
 * @brief Mathematical learning state
 */
typedef struct {
    float pattern_sensitivity;           /**< Sensitivity to patterns */
    float proof_skill;                   /**< Theorem proving skill */
    float conjecture_quality;            /**< Conjecture generation quality */
    float analogy_strength;              /**< Cross-domain analogy ability */
    float insight_frequency;             /**< Insight emergence frequency */
    float elegance_perception;           /**< Elegance perception */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
    uint64_t breakthroughs_achieved;     /**< Total breakthroughs */
    genius_mode_t strongest_mode;        /**< Mode with highest skill */
} genius_learning_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    genius_plasticity_state_t state;     /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
    float recent_insight_rate;           /**< Recent insight rate */
} genius_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t proof_success_events;       /**< Proof success learning */
    uint64_t proof_failure_events;       /**< Proof failure learning */
    uint64_t pattern_found_events;       /**< Pattern found events */
    uint64_t insight_events;             /**< Insight emergence events */
    uint64_t breakthrough_events;        /**< Breakthrough events */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
    float mode_skill[GENIUS_MODE_COUNT]; /**< Skill level per mode */
} genius_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct genius_plasticity_bridge genius_plasticity_bridge_t;

//=============================================================================
// Forward Declarations
//=============================================================================

struct mathematical_genius;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*genius_plasticity_learn_callback_t)(
    genius_plasticity_bridge_t* bridge,
    genius_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Skill improvement callback */
typedef void (*genius_plasticity_skill_callback_t)(
    genius_plasticity_bridge_t* bridge,
    genius_mode_t mode,
    float old_skill,
    float new_skill,
    void* user_data
);

/** @brief Breakthrough callback */
typedef void (*genius_plasticity_breakthrough_callback_t)(
    genius_plasticity_bridge_t* bridge,
    const char* description,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
genius_plasticity_config_t genius_plasticity_config_default(void);

/**
 * @brief Create genius plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
genius_plasticity_bridge_t* genius_plasticity_create(
    const genius_plasticity_config_t* config
);

/**
 * @brief Destroy genius plasticity bridge
 * @param bridge Bridge to destroy
 */
void genius_plasticity_destroy(genius_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int genius_plasticity_reset(genius_plasticity_bridge_t* bridge);

/**
 * @brief Link to mathematical genius module
 * @param bridge Bridge handle
 * @param genius Mathematical genius handle
 * @return 0 on success, -1 on failure
 */
int genius_plasticity_link_genius(
    genius_plasticity_bridge_t* bridge,
    struct mathematical_genius* genius
);

//=============================================================================
// Synapse Management
//=============================================================================

/**
 * @brief Register a synapse for plasticity tracking
 * @param bridge Bridge handle
 * @param synapse_id Unique synapse ID
 * @param type Synapse type
 * @param initial_weight Initial weight
 * @param mode Associated genius mode
 * @return 0 on success, -1 on failure
 */
int genius_plasticity_register_synapse(
    genius_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    genius_synapse_type_t type,
    float initial_weight,
    genius_mode_t mode
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int genius_plasticity_unregister_synapse(
    genius_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int genius_plasticity_get_synapse(
    genius_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    genius_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int genius_plasticity_protect_synapse(
    genius_plasticity_bridge_t* bridge,
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
int genius_plasticity_learn(
    genius_plasticity_bridge_t* bridge,
    genius_learn_event_t event,
    float magnitude,
    uint32_t synapse_id,
    float context
);

/**
 * @brief Apply STDP to synapse (proof step association)
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param pre_time Pre-synaptic spike time (ms)
 * @param post_time Post-synaptic spike time (ms)
 * @return Weight change, NAN on failure
 */
float genius_plasticity_apply_stdp(
    genius_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
);

/**
 * @brief Apply proof outcome reward modulation
 * @param bridge Bridge handle
 * @param reward Reward signal [-1, 1]
 * @param elegance Elegance bonus [0, 1]
 * @return 0 on success, -1 on failure
 */
int genius_plasticity_apply_proof_reward(
    genius_plasticity_bridge_t* bridge,
    float reward,
    float elegance
);

/**
 * @brief Apply insight emergence reward
 * @param bridge Bridge handle
 * @param insight_strength Insight strength [0, 1]
 * @param mode Mode that produced insight
 * @return 0 on success, -1 on failure
 */
int genius_plasticity_apply_insight_reward(
    genius_plasticity_bridge_t* bridge,
    float insight_strength,
    genius_mode_t mode
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int genius_plasticity_update_bcm(
    genius_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int genius_plasticity_homeostatic_update(
    genius_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int genius_plasticity_update_traces(
    genius_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning (e.g., after sleep)
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int genius_plasticity_consolidate(genius_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get mathematical learning state
 * @param bridge Bridge handle
 * @param state Output learning state
 * @return 0 on success, -1 on failure
 */
int genius_plasticity_get_learning_state(
    genius_plasticity_bridge_t* bridge,
    genius_learning_state_t* state
);

/**
 * @brief Get skill level for specific mode
 * @param bridge Bridge handle
 * @param mode Genius mode
 * @return Skill level [0-1], -1 on error
 */
float genius_plasticity_get_mode_skill(
    genius_plasticity_bridge_t* bridge,
    genius_mode_t mode
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int genius_plasticity_get_state(
    genius_plasticity_bridge_t* bridge,
    genius_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int genius_plasticity_get_stats(
    genius_plasticity_bridge_t* bridge,
    genius_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int genius_plasticity_reset_stats(genius_plasticity_bridge_t* bridge);

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
int genius_plasticity_register_learn_callback(
    genius_plasticity_bridge_t* bridge,
    genius_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register skill improvement callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int genius_plasticity_register_skill_callback(
    genius_plasticity_bridge_t* bridge,
    genius_plasticity_skill_callback_t callback,
    void* user_data
);

/**
 * @brief Register breakthrough callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int genius_plasticity_register_breakthrough_callback(
    genius_plasticity_bridge_t* bridge,
    genius_plasticity_breakthrough_callback_t callback,
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
int genius_plasticity_bio_async_connect(genius_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int genius_plasticity_bio_async_disconnect(genius_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool genius_plasticity_is_bio_async_connected(genius_plasticity_bridge_t* bridge);

//=============================================================================
// Heartbeat and State Serialization (Phase 8)
//=============================================================================

/** @brief Default heartbeat interval in milliseconds */
#define GENIUS_PLASTICITY_HEARTBEAT_INTERVAL_MS  1000

/** @brief Heartbeat timeout multiplier */
#define GENIUS_PLASTICITY_HEARTBEAT_TIMEOUT_MULT 3.0f

/**
 * @brief Serialized state structure for persistence/recovery
 */
typedef struct {
    uint32_t version;                        /**< Serialization version */
    uint32_t num_synapses;                   /**< Number of synapses */
    genius_plasticity_bridge_state_t state;  /**< Bridge state snapshot */
    genius_learning_state_t learning;        /**< Learning state snapshot */
    genius_plasticity_stats_t stats;         /**< Statistics snapshot */
    uint64_t timestamp_us;                   /**< Serialization timestamp */
    uint32_t checksum;                       /**< Data integrity checksum */
} genius_plasticity_serialized_t;

/**
 * @brief Send heartbeat signal
 *
 * WHAT: Broadcast heartbeat to indicate bridge is alive
 * WHY:  Enable health monitoring and fault detection
 * HOW:  Send bio-async heartbeat message with current state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int genius_plasticity_send_heartbeat(genius_plasticity_bridge_t* bridge);

/**
 * @brief Get last heartbeat timestamp
 *
 * @param bridge Bridge handle
 * @return Last heartbeat timestamp in microseconds, 0 on error
 */
uint64_t genius_plasticity_get_last_heartbeat(
    const genius_plasticity_bridge_t* bridge
);

/**
 * @brief Check if heartbeat is stale
 *
 * @param bridge Bridge handle
 * @param timeout_ms Timeout threshold in milliseconds
 * @return true if stale (timeout exceeded), false otherwise
 */
bool genius_plasticity_is_heartbeat_stale(
    const genius_plasticity_bridge_t* bridge,
    uint32_t timeout_ms
);

/**
 * @brief Serialize bridge state for persistence
 *
 * WHAT: Capture full bridge state for recovery
 * WHY:  Enable fault tolerance and state migration
 * HOW:  Serialize synapse weights, learning state, statistics
 *
 * @param bridge Bridge handle
 * @param serialized Output serialized state
 * @return 0 on success, -1 on failure
 */
int genius_plasticity_serialize_state(
    genius_plasticity_bridge_t* bridge,
    genius_plasticity_serialized_t* serialized
);

/**
 * @brief Deserialize and restore bridge state
 *
 * @param bridge Bridge handle
 * @param serialized Serialized state to restore
 * @return 0 on success, -1 on failure
 */
int genius_plasticity_deserialize_state(
    genius_plasticity_bridge_t* bridge,
    const genius_plasticity_serialized_t* serialized
);

/**
 * @brief Compute checksum for state verification
 *
 * @param serialized Serialized state
 * @return Computed checksum
 */
uint32_t genius_plasticity_compute_checksum(
    const genius_plasticity_serialized_t* serialized
);

/**
 * @brief Verify serialized state integrity
 *
 * @param serialized Serialized state to verify
 * @return true if checksum matches, false otherwise
 */
bool genius_plasticity_verify_checksum(
    const genius_plasticity_serialized_t* serialized
);

//=============================================================================
// KG Wiring Integration
//=============================================================================

/* Forward declaration */
struct kg_module_wiring;

/**
 * @brief Create KG wiring descriptor for this bridge
 *
 * Creates a module wiring descriptor that enables brain self-awareness
 * of this bridge's topology, connections, inputs/outputs, and weights.
 *
 * @return Wiring descriptor or NULL on failure (caller owns memory)
 */
struct kg_module_wiring* genius_plasticity_create_kg_wiring(void);

/**
 * @brief Get KG wiring from bridge instance
 * @param bridge Bridge handle
 * @return Wiring descriptor or NULL if not initialized
 */
struct kg_module_wiring* genius_plasticity_get_kg_wiring(genius_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GENIUS_PLASTICITY_BRIDGE_H */
