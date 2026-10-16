/**
 * @file nimcp_snn_population_bridge.h
 * @brief SNN-Population Coding bridge for spike pattern to population code conversion
 *
 * WHAT: Bidirectional integration between SNN and population coding system
 * WHY:  Enable robust distributed representations via population codes
 * HOW:  Convert spike patterns to population vectors and vice versa
 *
 * BIOLOGICAL BASIS:
 * - Motor cortex uses population vectors for reach direction
 * - Visual cortex represents orientation via population tuning
 * - Hippocampal place cells encode spatial location via population
 * - Population codes provide noise robustness through redundancy
 *
 * INTEGRATION:
 * - Connects to snn_network_t for spike activity
 * - Connects to population_coding_encoder_t for vector encoding
 * - Uses bio-async for population state messaging
 * - Enables neural assembly coordination
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_POPULATION_BRIDGE_H
#define NIMCP_SNN_POPULATION_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "middleware/encoding/nimcp_population_coding.h"
#include "async/nimcp_bio_async.h"

//=============================================================================
// Configuration Types
//=============================================================================

/**
 * @brief SNN population coding bridge configuration
 *
 * WHAT: Parameters for SNN-population coding integration
 * WHY:  Control encoding/decoding behavior
 * HOW:  Configure tuning curves, vector sum, synchrony
 */
typedef struct snn_population_config_s {
    /* Population coding parameters */
    float tuning_width_rad;          /**< Tuning curve width (radians) */
    float max_firing_rate_hz;        /**< Maximum rate for normalization */
    uint32_t n_pca_components;       /**< PCA components to extract */
    float synchrony_threshold;       /**< Threshold for synchrony detection */

    /* Encoding/Decoding modes */
    bool enable_vector_sum;          /**< Encode as population vector */
    bool enable_center_of_mass;      /**< Encode as center of mass */
    bool enable_pca;                 /**< Enable PCA encoding */
    bool enable_synchrony_analysis;  /**< Analyze population synchrony */

    /* Tuning curve configuration */
    bool auto_generate_tuning;       /**< Auto-generate tuning curves */
    float tuning_diversity;          /**< Diversity of preferred directions */

    /* Temporal integration */
    float temporal_window_ms;        /**< Time window for rate calculation */
    float rate_decay_tau_ms;         /**< Exponential decay for rate */

    /* Integration flags */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    float update_interval_ms;        /**< How often to update encoding */
} snn_population_config_t;

/**
 * @brief Population coding statistics
 *
 * WHAT: Metrics for population coding performance
 * WHY:  Monitor encoding quality and neural assembly coherence
 * HOW:  Track synchrony, vector magnitude, PCA variance
 */
typedef struct snn_population_stats_s {
    uint64_t vectors_encoded;        /**< Total vectors encoded */
    uint64_t vectors_decoded;        /**< Total vectors decoded */
    float avg_vector_magnitude;      /**< Average population vector magnitude */
    float avg_synchrony;             /**< Average synchrony index */
    float pca_variance_explained;    /**< Variance explained by PCA */
    uint32_t active_assemblies;      /**< Number of active neural assemblies */
} snn_population_stats_t;

/**
 * @brief SNN-Population bridge structure
 *
 * WHAT: Context for SNN-population coding integration
 * WHY:  Maintain state of bidirectional bridge
 * HOW:  Store references and encoding state
 */
typedef struct snn_population_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* network;              /**< SNN network being encoded */
    population_coding_encoder_t encoder; /**< Population coding encoder */
    snn_population_config_t config;      /**< Bridge configuration */
    snn_population_stats_t stats;        /**< Population statistics */

    /* Encoding state */
    bool connected;                      /**< Bridge active */
    float last_update_time;              /**< Last update timestamp (ms) */
    tuning_curve_t* tuning_curves;       /**< Tuning curves per neuron */
    uint32_t n_tuning_curves;            /**< Number of tuning curves */

    /* Population vectors */
    vector3d_t* current_vectors;         /**< Current population vectors per pop */
    float* firing_rates;                 /**< Current firing rates per neuron */
    uint32_t max_neurons;                /**< Max neurons across populations */

    /* PCA state */
    pca_result_t* pca_result;            /**< Cached PCA result */
    bool pca_valid;                      /**< PCA result is current */

    /* Synchrony tracking */
    synchrony_result_t* synchrony_results; /**< Per-population synchrony */

    /* Bio-async */
    bool bio_async_enabled;              /**< Bio-async connected */
    bio_module_context_t bio_ctx;        /**< Bio-async context */

    /* Mutex for thread safety */
    void* mutex;
} snn_population_bridge_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Initialize population config with defaults
 *
 * WHAT: Set biologically-plausible defaults
 * WHY:  Convenient initialization
 * HOW:  Values from population coding literature
 *
 * @param config Config to initialize
 */
void snn_population_config_default(snn_population_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create SNN-population coding bridge
 *
 * WHAT: Initialize bidirectional bridge between SNN and population coding
 * WHY:  Enable population code representations
 * HOW:  Allocate context, create encoder, generate tuning curves
 *
 * @param config Bridge configuration
 * @param network SNN network to encode
 * @return Bridge instance or NULL on failure
 */
snn_population_bridge_t* snn_population_bridge_create(
    const snn_population_config_t* config,
    snn_network_t* network
);

/**
 * @brief Destroy SNN-population bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper cleanup
 * HOW:  Disconnect and free encoder
 *
 * @param bridge Bridge to destroy
 */
void snn_population_bridge_destroy(snn_population_bridge_t* bridge);

/**
 * @brief Connect bridge to bio-async
 *
 * WHAT: Enable bio-async population messaging
 * WHY:  Distributed population coordination
 * HOW:  Register with bio-router
 *
 * @param bridge Bridge to connect
 * @return 0 on success, error code on failure
 */
int snn_population_bridge_connect_bio_async(snn_population_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success
 */
int snn_population_bridge_disconnect_bio_async(snn_population_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool snn_population_bridge_is_bio_async_connected(const snn_population_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Process population encoding from spike patterns
 *
 * WHAT: Convert spike activity to population code
 * WHY:  Extract distributed representation from neural activity
 * HOW:  Compute firing rates, apply vector sum or center of mass
 *
 * ALGORITHM:
 * 1. For each population:
 *    a. Compute instantaneous firing rates from spike trains
 *    b. If vector_sum enabled: weighted sum of tuning curves
 *    c. If center_of_mass enabled: activity centroid
 *    d. If PCA enabled: project to principal components
 *    e. If synchrony enabled: compute correlation matrix
 * 2. Update statistics
 *
 * @param bridge Bridge for encoding
 * @param spikes_in Input spike array [n_spikes]
 * @param n_spikes Number of input spikes
 * @param spikes_out Output spike array (decoded) [n_out capacity]
 * @param n_out_capacity Maximum output spikes
 * @param n_out_actual Actual number of output spikes
 * @return 0 on success, error code on failure
 */
int snn_population_bridge_process(
    snn_population_bridge_t* bridge,
    const snn_spike_t* spikes_in,
    uint32_t n_spikes,
    snn_spike_t* spikes_out,
    uint32_t n_out_capacity,
    uint32_t* n_out_actual
);

/**
 * @brief Update population encoding state
 *
 * WHAT: Update firing rates, vectors, synchrony
 * WHY:  Keep population code current
 * HOW:  Recalculate rates, update vectors, analyze synchrony
 *
 * @param bridge Bridge to update
 * @param dt Timestep (ms)
 * @return 0 on success, error code on failure
 */
int snn_population_bridge_update(snn_population_bridge_t* bridge, float dt);

/**
 * @brief Encode population vector from firing rates
 *
 * WHAT: Compute population vector sum
 * WHY:  Extract directional information
 * HOW:  Weighted sum of tuning curves
 *
 * @param bridge Bridge with tuning curves
 * @param pop_id Population ID
 * @param vector_out Output population vector
 * @return 0 on success, error code on failure
 */
int snn_population_bridge_encode_vector(
    snn_population_bridge_t* bridge,
    uint32_t pop_id,
    vector3d_t* vector_out
);

/**
 * @brief Decode firing rates from population vector
 *
 * WHAT: Convert population vector to firing rates
 * WHY:  Generate spike activity from desired direction
 * HOW:  Rate[i] = max_rate × cos(angle(vector, preferred[i]))
 *
 * @param bridge Bridge with tuning curves
 * @param pop_id Population ID
 * @param vector Input population vector
 * @return 0 on success, error code on failure
 */
int snn_population_bridge_decode_vector(
    snn_population_bridge_t* bridge,
    uint32_t pop_id,
    const vector3d_t* vector
);

//=============================================================================
// Tuning Curve Management
//=============================================================================

/**
 * @brief Set tuning curve for neuron
 *
 * WHAT: Configure neuron's preferred direction
 * WHY:  Define population coding space
 * HOW:  Store tuning parameters
 *
 * @param bridge Bridge to configure
 * @param pop_id Population ID
 * @param neuron_idx Neuron index within population
 * @param tuning Tuning curve parameters
 * @return 0 on success, error code on failure
 */
int snn_population_bridge_set_tuning(
    snn_population_bridge_t* bridge,
    uint32_t pop_id,
    uint32_t neuron_idx,
    const tuning_curve_t* tuning
);

/**
 * @brief Get tuning curve for neuron
 *
 * @param bridge Bridge to query
 * @param pop_id Population ID
 * @param neuron_idx Neuron index
 * @param tuning Output: tuning curve
 * @return 0 on success, error code on failure
 */
int snn_population_bridge_get_tuning(
    const snn_population_bridge_t* bridge,
    uint32_t pop_id,
    uint32_t neuron_idx,
    tuning_curve_t* tuning
);

/**
 * @brief Auto-generate tuning curves for population
 *
 * WHAT: Create diverse tuning curves
 * WHY:  Cover directional space uniformly
 * HOW:  Distribute preferred directions evenly
 *
 * BIOLOGICAL BASIS:
 * - Visual cortex has neurons tuned to all orientations
 * - Motor cortex covers all reach directions
 * - Place cells tile spatial environment
 *
 * @param bridge Bridge to configure
 * @param pop_id Population ID
 * @return 0 on success, error code on failure
 */
int snn_population_bridge_generate_tuning(
    snn_population_bridge_t* bridge,
    uint32_t pop_id
);

//=============================================================================
// Synchrony Analysis
//=============================================================================

/**
 * @brief Compute population synchrony
 *
 * WHAT: Measure coordinated firing in population
 * WHY:  Detect neural assemblies and binding
 * HOW:  Average pairwise cross-correlation
 *
 * @param bridge Bridge with spike data
 * @param pop_id Population ID
 * @param synchrony_out Output: synchrony metrics
 * @return 0 on success, error code on failure
 */
int snn_population_bridge_compute_synchrony(
    snn_population_bridge_t* bridge,
    uint32_t pop_id,
    synchrony_result_t* synchrony_out
);

/**
 * @brief Get current population vector
 *
 * WHAT: Retrieve current population vector
 * WHY:  Query population state
 * HOW:  Return cached vector
 *
 * @param bridge Bridge to query
 * @param pop_id Population ID
 * @param vector Output: current vector
 * @return 0 on success, error code on failure
 */
int snn_population_bridge_get_current_vector(
    const snn_population_bridge_t* bridge,
    uint32_t pop_id,
    vector3d_t* vector
);

/**
 * @brief Get firing rates for population
 *
 * @param bridge Bridge to query
 * @param pop_id Population ID
 * @param rates Output: firing rates [n_neurons]
 * @param n_neurons Size of rates array
 * @return 0 on success, error code on failure
 */
int snn_population_bridge_get_rates(
    const snn_population_bridge_t* bridge,
    uint32_t pop_id,
    float* rates,
    uint32_t n_neurons
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get population coding statistics
 *
 * @param bridge Bridge to query
 * @param stats Output: statistics (copied)
 * @return 0 on success
 */
int snn_population_bridge_get_stats(
    const snn_population_bridge_t* bridge,
    snn_population_stats_t* stats
);

/**
 * @brief Reset population statistics
 *
 * @param bridge Bridge to reset
 */
void snn_population_bridge_reset_stats(snn_population_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_POPULATION_BRIDGE_H */
