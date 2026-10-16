/**
 * @file nimcp_snn_cortical_bridge.h
 * @brief SNN integration bridge for cortical column structures
 *
 * WHAT: Bidirectional integration between SNN and cortical columns
 * WHY:  Enable spike-based processing in hierarchical cortical architecture
 * HOW:  Layer-specific spike routing, minicolumn population coding, lateral inhibition
 *
 * BIOLOGICAL BASIS:
 * - Layer 2/3: Integrate spikes from local minicolumns, output to other cortical areas
 * - Layer 4: Process thalamic spike inputs (sensory relay)
 * - Layer 5: Generate output spike bursts for subcortical targets
 * - Layer 6: Feedback spike modulation to thalamus and layer 4
 * - Minicolumns: ~80-100 neurons with shared tuning preference
 * - Lateral inhibition: Mexican hat profile via interneuron spike timing
 *
 * INTEGRATION:
 * - Connects to cortical_column_pool_t and hypercolumn_t
 * - Maps SNN populations to cortical layers
 * - Converts minicolumn activations to spike patterns
 * - Implements competitive dynamics via spike-based WTA
 * - Uses bio-async for inter-column communication
 *
 * @author NIMCP Team
 * @date 2024
 */

#ifndef NIMCP_SNN_CORTICAL_BRIDGE_H
#define NIMCP_SNN_CORTICAL_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "core/cortical_columns/nimcp_cortical_column.h"
#include "core/brain_regions/nimcp_brain_regions.h"

//=============================================================================
// Configuration Types
//=============================================================================

/**
 * @brief SNN cortical bridge configuration
 *
 * WHAT: Parameters for SNN-cortical column integration
 * WHY:  Control how spikes map to cortical layers and minicolumns
 * HOW:  Layer-specific rates, competitive dynamics, lateral inhibition
 */
typedef struct snn_cortical_config_s {
    /* Layer-specific spike rates */
    float layer_2_3_base_rate;      /**< Base firing rate for L2/3 (Hz) */
    float layer_4_base_rate;        /**< Base rate for L4 input layer (Hz) */
    float layer_5_base_rate;        /**< Base rate for L5 output layer (Hz) */
    float layer_6_base_rate;        /**< Base rate for L6 feedback layer (Hz) */

    /* Spike burst parameters */
    float burst_threshold;          /**< Activation threshold for burst generation */
    uint32_t burst_spike_count;     /**< Number of spikes per burst */
    float burst_isi;                /**< Inter-spike interval in burst (ms) */

    /* Minicolumn population coding */
    uint32_t neurons_per_minicolumn; /**< Neurons representing each minicolumn */
    float tuning_curve_width;       /**< Gaussian tuning curve sigma */
    float population_code_threshold; /**< Min activation for spike generation */

    /* Lateral inhibition via spikes */
    float inhibitory_spike_weight;  /**< Weight of inhibitory spike (negative) */
    float inhibitory_delay;         /**< Delay for lateral inhibition (ms) */
    float mexican_hat_radius;       /**< Spatial radius for lateral inhibition */

    /* Competition dynamics */
    cc_competition_mode_t competition_mode; /**< WTA/K-winners/softmax */
    uint32_t k_winners;             /**< Number of winners (if K_WINNERS) */
    float competition_timescale;    /**< Competition update interval (ms) */

    /* Bio-async */
    bool enable_bio_async;          /**< Enable bio-async messaging */
} snn_cortical_config_t;

/**
 * @brief Layer-specific spike activity
 *
 * WHAT: Spike statistics per cortical layer
 * WHY:  Monitor layer-specific processing
 * HOW:  Track rates, burst patterns, and synchrony
 */
typedef struct cortical_layer_activity_s {
    float spike_rate;               /**< Current layer firing rate (Hz) */
    float burst_ratio;              /**< Fraction of spikes in bursts */
    float mean_isi;                 /**< Mean inter-spike interval (ms) */
    float synchrony_index;          /**< Layer synchrony [0, 1] */
    uint32_t active_neurons;        /**< Number of active neurons */
    uint64_t total_spikes;          /**< Total spikes since reset */
} cortical_layer_activity_t;

/**
 * @brief Minicolumn spike pattern
 *
 * WHAT: Spike representation of minicolumn state
 * WHY:  Convert activation to population code
 * HOW:  Gaussian-weighted spike rates across neuron population
 */
typedef struct minicolumn_spike_pattern_s {
    uint32_t minicolumn_id;         /**< Minicolumn this pattern represents */
    float activation_level;         /**< Current activation [0, 1] */
    float* neuron_rates;            /**< Spike rates per neuron [neurons_per_minicolumn] */
    uint32_t n_neurons;             /**< Number of neurons */
    bool is_winner;                 /**< Winner in competition */
    float lateral_inhibition;       /**< Current inhibition level */
} minicolumn_spike_pattern_t;

/**
 * @brief SNN-cortical bridge structure
 *
 * WHAT: Context for SNN-cortical integration
 * WHY:  Maintain state of bidirectional bridge
 * HOW:  Store references and spike patterns
 */
typedef struct snn_cortical_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* network;                 /**< SNN network */
    cortical_column_pool_t* pool;           /**< Cortical column pool */
    hypercolumn_t* hypercolumn;             /**< Hypercolumn being bridged */
    snn_cortical_config_t config;           /**< Bridge configuration */

    /* Population mappings (one per layer) */
    snn_population_t* layer_2_3_pop;        /**< L2/3 population */
    snn_population_t* layer_4_pop;          /**< L4 population */
    snn_population_t* layer_5_pop;          /**< L5 population */
    snn_population_t* layer_6_pop;          /**< L6 population */

    /* Minicolumn spike patterns */
    minicolumn_spike_pattern_t** minicolumn_patterns; /**< Patterns per minicolumn */
    uint32_t n_minicolumns;                 /**< Number of minicolumns */

    /* Layer activity tracking */
    cortical_layer_activity_t layer_activity[LAYER_COUNT]; /**< Activity per layer */

    /* State */
    bool connected;                         /**< Bridge active */
    float last_update_time;                 /**< Last update timestamp (ms) */
    uint32_t update_count;                  /**< Number of updates */

    /* Bio-async */
    bool bio_async_enabled;                 /**< Bio-async connected */
    bio_module_context_t bio_ctx;           /**< Bio-async context */

    /* Mutex for thread safety */
    void* mutex;
} snn_cortical_bridge_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Initialize cortical config with defaults
 *
 * WHAT: Set biologically-plausible defaults for cortical layers
 * WHY:  Convenient initialization
 * HOW:  Values from cortical neuroscience literature
 *
 * @param config Config to initialize
 */
void snn_cortical_config_default(snn_cortical_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create SNN-cortical bridge
 *
 * WHAT: Initialize bidirectional bridge
 * WHY:  Enable spike-based cortical processing
 * HOW:  Allocate context, create populations, set up mappings
 *
 * @param config Bridge configuration
 * @param network SNN network
 * @param pool Cortical column pool
 * @param hypercolumn Hypercolumn to bridge
 * @return Bridge instance or NULL on failure
 */
snn_cortical_bridge_t* snn_cortical_bridge_create(
    const snn_cortical_config_t* config,
    snn_network_t* network,
    cortical_column_pool_t* pool,
    hypercolumn_t* hypercolumn
);

/**
 * @brief Destroy SNN-cortical bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper cleanup
 * HOW:  Disconnect and free
 *
 * @param bridge Bridge to destroy
 */
void snn_cortical_bridge_destroy(snn_cortical_bridge_t* bridge);

/**
 * @brief Connect bridge to bio-async
 *
 * WHAT: Enable bio-async messaging
 * WHY:  Distributed cortical coordination
 * HOW:  Register with bio-router
 *
 * @param bridge Bridge to connect
 * @return 0 on success, error code on failure
 */
int snn_cortical_bridge_connect_bio_async(snn_cortical_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success
 */
int snn_cortical_bridge_disconnect_bio_async(snn_cortical_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool snn_cortical_bridge_is_bio_async_connected(const snn_cortical_bridge_t* bridge);

//=============================================================================
// Processing Functions
//=============================================================================

/**
 * @brief Process input through cortical layers
 *
 * WHAT: Convert input to layer 4 spikes, propagate through layers
 * WHY:  Implement cortical feedforward processing
 * HOW:  Encode input as L4 spikes, compute L2/3, L5, L6 responses
 *
 * @param bridge Bridge context
 * @param input Input feature vector
 * @param input_size Size of input
 * @param output Output spike patterns (pre-allocated)
 * @param output_size Size of output buffer
 * @return 0 on success, error code on failure
 */
int snn_cortical_bridge_process(
    snn_cortical_bridge_t* bridge,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t output_size
);

/**
 * @brief Update bridge state
 *
 * WHAT: Single timestep update of cortical-SNN dynamics
 * WHY:  Synchronize cortical and SNN states
 * HOW:  Update spike patterns, competition, lateral inhibition
 *
 * @param bridge Bridge to update
 * @param dt Timestep (ms)
 * @return 0 on success, error code on failure
 */
int snn_cortical_bridge_update(snn_cortical_bridge_t* bridge, float dt);

/**
 * @brief Generate spike bursts for winning minicolumns
 *
 * WHAT: Generate spike bursts for columns above threshold
 * WHY:  Model cortical burst firing patterns
 * HOW:  Generate burst_spike_count spikes at burst_isi intervals
 *
 * @param bridge Bridge context
 * @param layer Target cortical layer
 * @return Number of bursts generated
 */
uint32_t snn_cortical_generate_bursts(
    snn_cortical_bridge_t* bridge,
    cortical_layer_t layer
);

/**
 * @brief Apply lateral inhibition via spike timing
 *
 * WHAT: Implement Mexican hat lateral inhibition with spikes
 * WHY:  Model cortical competitive dynamics
 * HOW:  Generate inhibitory spikes based on neighbor activity
 *
 * @param bridge Bridge context
 * @return 0 on success, error code on failure
 */
int snn_cortical_apply_lateral_inhibition(snn_cortical_bridge_t* bridge);

/**
 * @brief Run spike-based competition in hypercolumn
 *
 * WHAT: Implement WTA/K-winners via spike rates
 * WHY:  Model cortical feature selection
 * HOW:  Modulate spike rates based on relative activations
 *
 * @param bridge Bridge context
 * @return 0 on success, error code on failure
 */
int snn_cortical_run_competition(snn_cortical_bridge_t* bridge);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get layer activity statistics
 *
 * @param bridge Bridge to query
 * @param layer Cortical layer
 * @param activity Output for activity (copied)
 * @return 0 on success
 */
int snn_cortical_get_layer_activity(
    const snn_cortical_bridge_t* bridge,
    cortical_layer_t layer,
    cortical_layer_activity_t* activity
);

/**
 * @brief Get minicolumn spike pattern
 *
 * @param bridge Bridge to query
 * @param minicolumn_idx Minicolumn index
 * @param pattern Output for pattern (copied)
 * @return 0 on success
 */
int snn_cortical_get_minicolumn_pattern(
    const snn_cortical_bridge_t* bridge,
    uint32_t minicolumn_idx,
    minicolumn_spike_pattern_t* pattern
);

/**
 * @brief Get winning minicolumn index
 *
 * @param bridge Bridge to query
 * @return Winner index or UINT32_MAX on error
 */
uint32_t snn_cortical_get_winner(const snn_cortical_bridge_t* bridge);

/**
 * @brief Get overall bridge activity level
 *
 * WHAT: Compute average activity across all layers
 * WHY:  Single metric for cortical activity
 * HOW:  Weighted average of layer spike rates
 *
 * @param bridge Bridge to query
 * @return Activity level [0, 1] or -1.0 on error
 */
float snn_cortical_bridge_get_activity(const snn_cortical_bridge_t* bridge);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge to query
 * @param total_spikes Output: total spikes across all layers
 * @param mean_rate Output: mean firing rate (Hz)
 * @param updates Output: update cycles completed
 * @return 0 on success
 */
int snn_cortical_get_stats(
    const snn_cortical_bridge_t* bridge,
    uint64_t* total_spikes,
    float* mean_rate,
    uint32_t* updates
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge to reset
 */
void snn_cortical_reset_stats(snn_cortical_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_CORTICAL_BRIDGE_H */
