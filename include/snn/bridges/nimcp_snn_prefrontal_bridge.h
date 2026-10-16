/**
 * @file nimcp_snn_prefrontal_bridge.h
 * @brief SNN integration bridge for prefrontal cortex executive functions
 *
 * WHAT: Bidirectional integration between SNN and prefrontal cortex
 * WHY:  Enable spike-based working memory, goal maintenance, and inhibitory control
 * HOW:  Persistent activity patterns, rate coding for goals, decision accumulation
 *
 * BIOLOGICAL BASIS:
 * - Persistent activity: Sustained firing maintains working memory during delay
 * - Delay period activity: Neurons fire continuously to bridge temporal gaps
 * - Attractor states: Recurrent networks maintain stable activity patterns
 * - Goal representation: Population codes for abstract goals and rules
 * - Inhibitory control: Prefrontal GABAergic interneurons suppress prepotent responses
 * - Decision accumulation: Ramping activity integrates evidence over time
 * - Dorsolateral PFC (DLPFC): Working memory maintenance
 * - Ventromedial PFC (VMPFC): Value-based decision making
 * - Orbitofrontal cortex (OFC): Outcome evaluation and reversal learning
 *
 * INTEGRATION:
 * - Connects to prefrontal region (REGION_PREFRONTAL)
 * - Maps SNN populations to PFC subregions
 * - Implements persistent activity via recurrent spikes
 * - Encodes goals as stable population patterns
 * - Models inhibitory control via interneuron networks
 * - Uses bio-async for prefrontal-subcortical communication
 *
 * @author NIMCP Team
 * @date 2024
 */

#ifndef NIMCP_SNN_PREFRONTAL_BRIDGE_H
#define NIMCP_SNN_PREFRONTAL_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "core/brain_regions/nimcp_brain_regions.h"

//=============================================================================
// Configuration Types
//=============================================================================

/**
 * @brief SNN prefrontal bridge configuration
 *
 * WHAT: Parameters for SNN-prefrontal cortex integration
 * WHY:  Control working memory, goals, and executive function
 * HOW:  Persistent activity, inhibition, decision dynamics
 */
typedef struct snn_prefrontal_config_s {
    /* Persistent activity parameters */
    float persistent_baseline_rate;   /**< Baseline rate for persistent neurons (Hz) */
    float persistent_active_rate;     /**< Active rate during delay (Hz) */
    float recurrent_excitation;       /**< Recurrent weight for maintenance */
    float decay_time_constant;        /**< Decay rate without input (ms) */

    /* Working memory capacity */
    uint32_t max_wm_items;            /**< Maximum items in working memory */
    uint32_t neurons_per_item;        /**< Neurons encoding each item */
    float wm_threshold;               /**< Activation threshold for WM */

    /* Goal representation */
    uint32_t num_goal_populations;    /**< Number of distinct goal populations */
    float goal_encoding_rate;         /**< Spike rate for active goal (Hz) */
    float goal_switch_cost;           /**< Cost to switch goals (time penalty) */

    /* Inhibitory control */
    float inhibitory_neuron_ratio;    /**< Ratio of inhibitory neurons */
    float inhibition_strength;        /**< GABAergic inhibition weight */
    float go_threshold;               /**< Threshold for go response */
    float stop_threshold;             /**< Threshold for stop/inhibit */

    /* Decision accumulation */
    float accumulator_leak;           /**< Leak rate for decision integrator */
    float decision_threshold;         /**< Threshold for decision commitment */
    float evidence_weight;            /**< Weight for evidence integration */
    bool enable_urgency_signal;       /**< Enable urgency gating */

    /* Subregion parameters */
    float dlpfc_ratio;                /**< % neurons in DLPFC */
    float vmpfc_ratio;                /**< % neurons in VMPFC */
    float ofc_ratio;                  /**< % neurons in OFC */

    /* Bio-async */
    bool enable_bio_async;            /**< Enable bio-async messaging */
} snn_prefrontal_config_t;

/**
 * @brief Working memory item representation
 *
 * WHAT: Spike pattern maintaining one item in WM
 * WHY:  Represent information during delay periods
 * HOW:  Population code with persistent activity
 */
typedef struct wm_item_s {
    uint32_t item_id;               /**< Item identifier */
    float* feature_vector;          /**< Item features */
    uint32_t feature_dim;           /**< Dimensionality */
    float* population_rates;        /**< Spike rates [neurons_per_item] */
    uint32_t n_neurons;             /**< Number of neurons */
    float activation_strength;      /**< Current activation [0, 1] */
    bool is_active;                 /**< Currently maintained */
    uint64_t onset_time;            /**< When item entered WM (us) */
} wm_item_t;

/**
 * @brief Goal state representation
 *
 * WHAT: Abstract goal encoded via population activity
 * WHY:  Maintain task context and rule sets
 * HOW:  Stable attractor states in goal populations
 */
typedef struct goal_state_s {
    uint32_t goal_id;               /**< Goal identifier */
    char goal_name[64];             /**< Human-readable name */
    float priority;                 /**< Goal priority [0, 1] */
    float activation;               /**< Current activation strength */
    bool is_active;                 /**< Currently pursued */
    uint32_t population_id;         /**< Associated population */
    float mean_spike_rate;          /**< Population mean rate (Hz) */
} goal_state_t;

/**
 * @brief Decision accumulator state
 *
 * WHAT: Evidence accumulation for decision making
 * WHY:  Integrate evidence over time to choice threshold
 * HOW:  Leaky integration with competing accumulators
 */
typedef struct decision_accumulator_s {
    uint32_t n_options;             /**< Number of choice options */
    float* evidence;                /**< Accumulated evidence per option */
    float* evidence_rates;          /**< Integration rates per option */
    bool decision_made;             /**< Threshold crossed */
    uint32_t chosen_option;         /**< Winning option */
    float decision_time;            /**< Time to reach threshold (ms) */
    float confidence;               /**< Decision confidence [0, 1] */
} decision_accumulator_t;

/**
 * @brief Inhibitory control network
 *
 * WHAT: Interneuron network for response inhibition
 * WHY:  Suppress prepotent responses and control behavior
 * HOW:  Feed-forward inhibition with go/stop dynamics
 */
typedef struct inhibitory_control_s {
    uint32_t n_inhibitory;          /**< Number of inhibitory neurons */
    uint32_t n_excitatory;          /**< Number of excitatory neurons */
    float go_signal;                /**< Go signal strength [0, 1] */
    float stop_signal;              /**< Stop signal strength [0, 1] */
    bool response_suppressed;       /**< Currently inhibiting */
    float inhibition_onset_time;    /**< Time inhibition started (ms) */
    float mean_inhibitory_rate;     /**< Mean inhibitory neuron rate (Hz) */
} inhibitory_control_t;

/**
 * @brief SNN-prefrontal bridge structure
 *
 * WHAT: Context for SNN-prefrontal integration
 * WHY:  Maintain state of executive functions
 * HOW:  Store WM items, goals, decision state
 */
typedef struct snn_prefrontal_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* network;             /**< SNN network */
    brain_region_t* prefrontal_region;  /**< Prefrontal brain region */
    snn_prefrontal_config_t config;     /**< Bridge configuration */

    /* Subregion populations */
    snn_population_t* dlpfc_pop;        /**< DLPFC (working memory) */
    snn_population_t* vmpfc_pop;        /**< VMPFC (value/emotion) */
    snn_population_t* ofc_pop;          /**< OFC (outcome evaluation) */
    snn_population_t* inhibitory_pop;   /**< Inhibitory interneurons */

    /* Working memory items */
    wm_item_t** wm_items;               /**< Active WM items */
    uint32_t n_wm_items;                /**< Number of active items */
    uint32_t max_wm_items;              /**< Maximum capacity */

    /* Goal representations */
    goal_state_t** goals;               /**< Goal states */
    uint32_t n_goals;                   /**< Number of goals */
    goal_state_t* active_goal;          /**< Currently active goal */

    /* Decision accumulator */
    decision_accumulator_t* accumulator; /**< Decision state */

    /* Inhibitory control */
    inhibitory_control_t* inhibition;   /**< Inhibitory network */

    /* State */
    bool connected;                     /**< Bridge active */
    float last_update_time;             /**< Last update timestamp (ms) */
    uint32_t update_count;              /**< Number of updates */

    /* Performance metrics */
    uint32_t total_decisions;           /**< Total decisions made */
    uint32_t inhibited_responses;       /**< Responses successfully inhibited */
    float mean_decision_time;           /**< Average decision time (ms) */

    /* Bio-async */
    bool bio_async_enabled;             /**< Bio-async connected */
    bio_module_context_t bio_ctx;       /**< Bio-async context */

    /* Mutex for thread safety */
    void* mutex;
} snn_prefrontal_bridge_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Initialize prefrontal config with defaults
 *
 * WHAT: Set biologically-plausible defaults
 * WHY:  Convenient initialization
 * HOW:  Values from prefrontal neuroscience literature
 *
 * @param config Config to initialize
 */
void snn_prefrontal_config_default(snn_prefrontal_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create SNN-prefrontal bridge
 *
 * WHAT: Initialize bidirectional bridge
 * WHY:  Enable spike-based executive functions
 * HOW:  Allocate context, create populations, initialize WM
 *
 * @param config Bridge configuration
 * @param network SNN network
 * @param prefrontal_region Prefrontal brain region
 * @return Bridge instance or NULL on failure
 */
snn_prefrontal_bridge_t* snn_prefrontal_bridge_create(
    const snn_prefrontal_config_t* config,
    snn_network_t* network,
    brain_region_t* prefrontal_region
);

/**
 * @brief Destroy SNN-prefrontal bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper cleanup
 * HOW:  Free WM items, goals, accumulators, disconnect
 *
 * @param bridge Bridge to destroy
 */
void snn_prefrontal_bridge_destroy(snn_prefrontal_bridge_t* bridge);

/**
 * @brief Connect bridge to bio-async
 *
 * @param bridge Bridge to connect
 * @return 0 on success, error code on failure
 */
int snn_prefrontal_bridge_connect_bio_async(snn_prefrontal_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success
 */
int snn_prefrontal_bridge_disconnect_bio_async(snn_prefrontal_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool snn_prefrontal_bridge_is_bio_async_connected(const snn_prefrontal_bridge_t* bridge);

//=============================================================================
// Processing Functions
//=============================================================================

/**
 * @brief Process input and update prefrontal state
 *
 * WHAT: Update WM, goals, and decisions based on input
 * WHY:  Maintain executive function state
 * HOW:  Update persistent activity, accumulators, inhibition
 *
 * @param bridge Bridge context
 * @param input Input feature vector
 * @param input_size Size of input
 * @param output Output state (pre-allocated)
 * @param output_size Size of output buffer
 * @return 0 on success, error code on failure
 */
int snn_prefrontal_bridge_process(
    snn_prefrontal_bridge_t* bridge,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t output_size
);

/**
 * @brief Update bridge state
 *
 * WHAT: Single timestep update of prefrontal dynamics
 * WHY:  Maintain persistent activity and accumulation
 * HOW:  Update WM decay, evidence integration, inhibition
 *
 * @param bridge Bridge to update
 * @param dt Timestep (ms)
 * @return 0 on success, error code on failure
 */
int snn_prefrontal_bridge_update(snn_prefrontal_bridge_t* bridge, float dt);

/**
 * @brief Add item to working memory
 *
 * WHAT: Encode new item via persistent activity
 * WHY:  Maintain information during delay
 * HOW:  Create population code with recurrent excitation
 *
 * @param bridge Bridge context
 * @param features Feature vector to encode
 * @param feature_dim Feature dimensionality
 * @return Item ID or UINT32_MAX on failure
 */
uint32_t snn_prefrontal_add_wm_item(
    snn_prefrontal_bridge_t* bridge,
    const float* features,
    uint32_t feature_dim
);

/**
 * @brief Remove item from working memory
 *
 * @param bridge Bridge context
 * @param item_id Item to remove
 * @return 0 on success
 */
int snn_prefrontal_remove_wm_item(snn_prefrontal_bridge_t* bridge, uint32_t item_id);

/**
 * @brief Set active goal
 *
 * WHAT: Switch to new goal representation
 * WHY:  Change task context
 * HOW:  Activate goal population, suppress others
 *
 * @param bridge Bridge context
 * @param goal_id Goal to activate
 * @return 0 on success
 */
int snn_prefrontal_set_goal(snn_prefrontal_bridge_t* bridge, uint32_t goal_id);

/**
 * @brief Accumulate evidence for decision
 *
 * WHAT: Integrate evidence spike by spike
 * WHY:  Build to decision threshold
 * HOW:  Leaky integration with competing accumulators
 *
 * @param bridge Bridge context
 * @param evidence Evidence values per option
 * @param n_options Number of options
 * @return 0 on success, 1 if decision made
 */
int snn_prefrontal_accumulate_evidence(
    snn_prefrontal_bridge_t* bridge,
    const float* evidence,
    uint32_t n_options
);

/**
 * @brief Apply inhibitory control
 *
 * WHAT: Suppress prepotent response
 * WHY:  Model stop-signal task and response inhibition
 * HOW:  Activate inhibitory population
 *
 * @param bridge Bridge context
 * @param stop_signal Stop signal strength [0, 1]
 * @return 0 on success
 */
int snn_prefrontal_apply_inhibition(
    snn_prefrontal_bridge_t* bridge,
    float stop_signal
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get number of items in working memory
 *
 * @param bridge Bridge to query
 * @return Number of active WM items
 */
uint32_t snn_prefrontal_get_wm_count(const snn_prefrontal_bridge_t* bridge);

/**
 * @brief Get working memory item
 *
 * @param bridge Bridge to query
 * @param item_id Item identifier
 * @return WM item or NULL if not found
 */
const wm_item_t* snn_prefrontal_get_wm_item(
    const snn_prefrontal_bridge_t* bridge,
    uint32_t item_id
);

/**
 * @brief Get active goal
 *
 * @param bridge Bridge to query
 * @return Active goal state or NULL
 */
const goal_state_t* snn_prefrontal_get_active_goal(const snn_prefrontal_bridge_t* bridge);

/**
 * @brief Check if decision has been made
 *
 * @param bridge Bridge to query
 * @return true if decision threshold crossed
 */
bool snn_prefrontal_is_decision_made(const snn_prefrontal_bridge_t* bridge);

/**
 * @brief Get chosen option from decision
 *
 * @param bridge Bridge to query
 * @return Chosen option index or UINT32_MAX if no decision
 */
uint32_t snn_prefrontal_get_decision(const snn_prefrontal_bridge_t* bridge);

/**
 * @brief Get overall bridge activity level
 *
 * WHAT: Compute average prefrontal activity
 * WHY:  Single metric for executive function engagement
 * HOW:  Average firing rates across subregions
 *
 * @param bridge Bridge to query
 * @return Activity level [0, 1] or -1.0 on error
 */
float snn_prefrontal_bridge_get_activity(const snn_prefrontal_bridge_t* bridge);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge to query
 * @param total_decisions Output: total decisions made
 * @param inhibited_responses Output: responses inhibited
 * @param updates Output: update cycles completed
 * @return 0 on success
 */
int snn_prefrontal_get_stats(
    const snn_prefrontal_bridge_t* bridge,
    uint32_t* total_decisions,
    uint32_t* inhibited_responses,
    uint32_t* updates
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge to reset
 */
void snn_prefrontal_reset_stats(snn_prefrontal_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_PREFRONTAL_BRIDGE_H */
