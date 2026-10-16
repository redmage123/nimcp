//=============================================================================
// nimcp_cortical_column.h - Cortical Column Architecture
//=============================================================================
/**
 * @file nimcp_cortical_column.h
 * @brief Core cortical column module for NIMCP - minicolumns and hypercolumns
 * @version 1.0.0
 * @date 2025-11-25
 *
 * WHAT: Hierarchical cortical column architecture with minicolumns and hypercolumns
 * WHY:  Implement biologically-inspired columnar organization found in mammalian cortex
 * HOW:  Minicolumns (~80-100 neurons) grouped into hypercolumns with lateral inhibition
 *       and competitive dynamics for feature detection and representation
 *
 * ARCHITECTURE:
 *
 *   Hypercolumn (100K neurons):
 *   ┌────────────────────────────────────────────────────────┐
 *   │  Minicolumn 0      Minicolumn 1      Minicolumn N     │
 *   │  (θ=0°, ~80n)     (θ=15°, ~80n)     (θ=180°, ~80n)   │
 *   │  ┌─────────┐       ┌─────────┐       ┌─────────┐     │
 *   │  │ Layer 2/3│       │ Layer 2/3│       │ Layer 2/3│     │
 *   │  │ Layer 4  │←──────│ Layer 4  │←──────│ Layer 4  │     │
 *   │  │ Layer 5/6│       │ Layer 5/6│       │ Layer 5/6│     │
 *   │  └─────────┘       └─────────┘       └─────────┘     │
 *   │       ↕ Lateral inhibition (Mexican hat)             │
 *   │       ↕ Winner-take-all / Soft-max competition       │
 *   └────────────────────────────────────────────────────────┘
 *
 * MATHEMATICAL FOUNDATION:
 *
 *   Gaussian Receptive Field:
 *     w(d) = exp(-d²/2σ²)
 *     where d = distance from receptive field center
 *
 *   Mexican Hat Lateral Inhibition:
 *     I(d) = (1 - d²/σ₁²) * exp(-d²/2σ₁²) - A * exp(-d²/2σ₂²)
 *     where σ₁ = narrow excitation, σ₂ = wide inhibition, A = amplitude
 *
 *   Soft-max Competition:
 *     p_i = exp(a_i/T) / Σexp(a_j/T)
 *     where T = temperature parameter
 *
 * PERFORMANCE:
 * - Minicolumn compute: O(N) where N = number of neurons in column
 * - Hypercolumn competition: O(M) where M = number of minicolumns
 * - Lateral inhibition: O(M²) for full connectivity (can be sparsified)
 * - Memory pool allocation: O(1) acquire/release
 *
 * INTEGRATION:
 * - Compatible with nimcp_brain_internal.h
 * - Uses nimcp_memory_pool for hot-path allocations
 * - Thread-safe with nimcp_platform_mutex
 * - Logging via nimcp_logging.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_COLUMN_H
#define NIMCP_CORTICAL_COLUMN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct minicolumn minicolumn_t;
typedef struct hypercolumn hypercolumn_t;
typedef struct cortical_column_pool cortical_column_pool_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Layer distribution for minicolumn neurons
 *
 * WHAT: Specifies how neurons are distributed across cortical layers
 * WHY:  Different layers have different functions (input, integration, output)
 */
typedef struct {
    uint32_t layer_2_3_count;  /**< Supragranular - output to other cortical areas */
    uint32_t layer_4_count;    /**< Granular - primary input from thalamus */
    uint32_t layer_5_6_count;  /**< Infragranular - output to subcortical structures */
} layer_distribution_t;

/**
 * @brief 3D receptive field definition
 *
 * WHAT: Defines the spatial region in feature space that activates the column
 * WHY:  Columns respond preferentially to stimuli in their receptive field
 */
typedef struct {
    float center_x;     /**< X coordinate of receptive field center */
    float center_y;     /**< Y coordinate of receptive field center */
    float center_z;     /**< Z coordinate of receptive field center */
    float radius;       /**< Receptive field radius (σ in Gaussian) */
} receptive_field_t;

/**
 * @brief Minicolumn configuration
 *
 * WHAT: Configuration parameters for creating a minicolumn
 * WHY:  Specify column properties at creation time
 */
typedef struct {
    uint32_t* neuron_ids;              /**< Array of neuron indices into brain's neuron array */
    uint32_t num_neurons;              /**< Number of neurons (typically 80-100) */
    receptive_field_t receptive_field; /**< Spatial receptive field */
    float tuning_preference;           /**< Feature this column detects (e.g., orientation angle) */
    layer_distribution_t layers;       /**< Neuron distribution across layers */
} minicolumn_config_t;

/**
 * @brief Competition mode for hypercolumn dynamics
 *
 * WHAT: Specifies how minicolumns compete for activation
 * WHY:  Different tasks benefit from different competition strategies
 * NOTE: Prefixed with CC_ to avoid conflicts with global_workspace.h
 */
typedef enum {
    CC_COMPETITION_WINNER_TAKE_ALL,    /**< Only strongest minicolumn active */
    CC_COMPETITION_K_WINNERS,          /**< Top K minicolumns active */
    CC_COMPETITION_SOFTMAX,            /**< Soft probabilistic distribution */
    CC_COMPETITION_NONE                /**< No competition, all active */
} cc_competition_mode_t;

/**
 * @brief Hypercolumn configuration
 *
 * WHAT: Configuration for creating a hypercolumn
 * WHY:  Organize minicolumns covering feature space
 */
typedef struct {
    uint32_t num_minicolumns;          /**< Number of minicolumns in hypercolumn */
    minicolumn_config_t* minicolumn_configs; /**< Array of minicolumn configurations */
    float feature_space_min;           /**< Minimum feature value (e.g., 0° orientation) */
    float feature_space_max;           /**< Maximum feature value (e.g., 180° orientation) */
    float topographic_x;               /**< X position in cortical map */
    float topographic_y;               /**< Y position in cortical map */
    cc_competition_mode_t competition; /**< Competition mode */
    uint32_t k_winners;                /**< Number of winners (if K_WINNERS mode) */
    float temperature;                 /**< Temperature for softmax (default: 1.0) */
    float lateral_inhibition_strength; /**< Lateral inhibition amplitude (default: 0.5) */
    float lateral_inhibition_sigma1;   /**< Narrow excitation σ (default: 1.0) */
    float lateral_inhibition_sigma2;   /**< Wide inhibition σ (default: 3.0) */
} hypercolumn_config_t;

/**
 * @brief Pool configuration
 *
 * WHAT: Configuration for memory pool
 * WHY:  Pre-allocate memory for fast O(1) allocation
 */
typedef struct {
    uint32_t max_minicolumns;          /**< Maximum number of minicolumns */
    uint32_t max_hypercolumns;         /**< Maximum number of hypercolumns */
    uint32_t max_neurons_per_minicolumn; /**< Max neurons per minicolumn (for sizing pools) */
    bool enable_cow_support;           /**< Enable copy-on-write support */
} cortical_column_pool_config_t;

//=============================================================================
// Statistics Structures
//=============================================================================

/**
 * @brief Minicolumn statistics
 *
 * WHAT: Runtime statistics for minicolumn
 * WHY:  Monitor performance and activity
 */
typedef struct {
    float activation_level;            /**< Current activation level [0.0, 1.0] */
    float inhibition_level;            /**< Current inhibition level [0.0, 1.0] */
    uint32_t total_activations;        /**< Total times activated */
    float average_activation;          /**< Average activation over time */
    float tuning_preference;           /**< Feature preference */
    uint32_t num_neurons;              /**< Number of neurons */
    uint64_t last_activation_time_us;  /**< Last activation timestamp */
} minicolumn_stats_t;

/**
 * @brief Hypercolumn statistics
 *
 * WHAT: Runtime statistics for hypercolumn
 * WHY:  Monitor competition dynamics and activity
 * NOTE: Prefixed with cc_ to avoid conflicts with orientation_columns.h
 */
typedef struct {
    uint32_t num_minicolumns;          /**< Number of minicolumns */
    uint32_t winner_index;             /**< Index of winning minicolumn */
    float winner_activation;           /**< Activation of winner */
    float total_activation;            /**< Sum of all minicolumn activations */
    float entropy;                     /**< Shannon entropy of distribution */
    uint32_t total_computations;       /**< Total compute calls */
    cc_competition_mode_t competition_mode; /**< Current competition mode */
} cc_hypercolumn_stats_t;

//=============================================================================
// Pool Management
//=============================================================================

/**
 * @brief Create cortical column memory pool
 *
 * WHAT: Pre-allocate memory for minicolumns and hypercolumns
 * WHY:  Avoid malloc overhead during runtime, enable O(1) allocation
 * HOW:  Use nimcp_memory_pool internally for efficient management
 *
 * @param config Pool configuration (NULL for defaults)
 * @return Pool handle or NULL on failure
 *
 * COMPLEXITY: O(N+M) where N=max_minicolumns, M=max_hypercolumns
 * THREAD-SAFE: Yes
 *
 * EXAMPLE:
 * ```c
 * cortical_column_pool_config_t config = {
 *     .max_minicolumns = 1000,
 *     .max_hypercolumns = 100,
 *     .max_neurons_per_minicolumn = 100,
 *     .enable_cow_support = true
 * };
 * cortical_column_pool_t* pool = cortical_column_pool_create(&config);
 * ```
 */
cortical_column_pool_t* cortical_column_pool_create(
    const cortical_column_pool_config_t* config
);

/**
 * @brief Destroy cortical column pool
 *
 * WHAT: Free all pool memory
 * WHY:  Clean shutdown
 * HOW:  Validate all columns freed, then destroy pools
 *
 * @param pool Pool handle
 *
 * WARNING: All minicolumns and hypercolumns must be destroyed before pool
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (must not have active allocations)
 */
void cortical_column_pool_destroy(cortical_column_pool_t* pool);

//=============================================================================
// Minicolumn Lifecycle
//=============================================================================

/**
 * @brief Create minicolumn
 *
 * WHAT: Create and initialize a minicolumn
 * WHY:  Represent ~80-100 neurons with shared tuning preference
 * HOW:  Allocate from pool, initialize receptive field and layer distribution
 *
 * @param pool Memory pool
 * @param config Minicolumn configuration
 * @return Minicolumn handle or NULL on failure
 *
 * COMPLEXITY: O(N) where N = num_neurons
 * THREAD-SAFE: Yes (pool is thread-safe)
 *
 * ERROR CONDITIONS:
 * - NULL pool or config
 * - Invalid neuron count (0 or > max)
 * - Pool exhausted
 * - Invalid layer distribution (sum != num_neurons)
 */
minicolumn_t* minicolumn_create(
    cortical_column_pool_t* pool,
    const minicolumn_config_t* config
);

/**
 * @brief Destroy minicolumn
 *
 * WHAT: Free minicolumn resources back to pool
 * WHY:  Enable memory reuse
 * HOW:  Return to pool's free list
 *
 * @param col Minicolumn handle
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void minicolumn_destroy(minicolumn_t* col);

//=============================================================================
// Hypercolumn Lifecycle
//=============================================================================

/**
 * @brief Create hypercolumn
 *
 * WHAT: Create hypercolumn containing multiple minicolumns
 * WHY:  Organize minicolumns covering feature space with competition
 * HOW:  Allocate from pool, create constituent minicolumns
 *
 * @param pool Memory pool
 * @param config Hypercolumn configuration
 * @return Hypercolumn handle or NULL on failure
 *
 * COMPLEXITY: O(M*N) where M=num_minicolumns, N=avg neurons per minicolumn
 * THREAD-SAFE: Yes
 *
 * ERROR CONDITIONS:
 * - NULL pool or config
 * - Invalid minicolumn count (0 or > max)
 * - Pool exhausted
 * - Invalid competition parameters
 */
hypercolumn_t* hypercolumn_create(
    cortical_column_pool_t* pool,
    const hypercolumn_config_t* config
);

/**
 * @brief Destroy hypercolumn
 *
 * WHAT: Free hypercolumn and all constituent minicolumns
 * WHY:  Complete cleanup of column hierarchy
 * HOW:  Destroy all minicolumns, then hypercolumn
 *
 * @param hcol Hypercolumn handle
 *
 * COMPLEXITY: O(M) where M = num_minicolumns
 * THREAD-SAFE: Yes
 */
void hypercolumn_destroy(hypercolumn_t* hcol);

//=============================================================================
// Processing and Computation
//=============================================================================

/**
 * @brief Compute minicolumn activation
 *
 * WHAT: Compute minicolumn response to input using receptive field weighting
 * WHY:  Determine how strongly this column responds to current input
 * HOW:  Apply Gaussian receptive field weighting: w(d) = exp(-d²/2σ²)
 *
 * @param col Minicolumn handle
 * @param input Input feature vector
 * @param input_size Size of input vector
 * @return Activation level [0.0, 1.0] or -1.0 on error
 *
 * COMPLEXITY: O(N) where N = input_size
 * THREAD-SAFE: Yes (read-only on input, updates internal state with mutex)
 *
 * ALGORITHM:
 * 1. Compute distance from input to receptive field center
 * 2. Apply Gaussian weighting: activation = exp(-distance²/2σ²)
 * 3. Combine with neuron activations
 * 4. Apply lateral inhibition
 */
float minicolumn_compute(
    minicolumn_t* col,
    const float* input,
    uint32_t input_size
);

/**
 * @brief Compute hypercolumn response
 *
 * WHAT: Compute all minicolumn activations and run competition
 * WHY:  Determine feature representation across entire hypercolumn
 * HOW:  Compute each minicolumn, apply lateral inhibition, run competition
 *
 * @param hcol Hypercolumn handle
 * @param input Input feature vector
 * @param input_size Size of input vector
 *
 * COMPLEXITY: O(M*N) where M=num_minicolumns, N=input_size
 * THREAD-SAFE: Yes
 *
 * ALGORITHM:
 * 1. Compute activation for each minicolumn
 * 2. Apply lateral inhibition (Mexican hat)
 * 3. Run competition (winner-take-all / softmax)
 * 4. Update statistics
 */
void hypercolumn_compute(
    hypercolumn_t* hcol,
    const float* input,
    uint32_t input_size
);

/**
 * @brief Get winning minicolumn index
 *
 * WHAT: Return index of most active minicolumn after competition
 * WHY:  Determine which feature was detected
 * HOW:  Return cached winner from last compute
 *
 * @param hcol Hypercolumn handle
 * @return Winner index or UINT32_MAX on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
uint32_t hypercolumn_get_winner(hypercolumn_t* hcol);

/**
 * @brief Get activation distribution across minicolumns
 *
 * WHAT: Return activation levels for all minicolumns
 * WHY:  Access full population code (not just winner)
 * HOW:  Copy activation array to output buffer
 *
 * @param hcol Hypercolumn handle
 * @param out_distribution Output array (must be size >= num_minicolumns)
 * @param size Size of output array
 *
 * COMPLEXITY: O(M) where M = num_minicolumns
 * THREAD-SAFE: Yes
 *
 * ERROR CONDITIONS:
 * - NULL hcol or out_distribution
 * - size < num_minicolumns
 */
void hypercolumn_get_distribution(
    hypercolumn_t* hcol,
    float* out_distribution,
    uint32_t size
);

//=============================================================================
// Lateral Inhibition and Competition
//=============================================================================

/**
 * @brief Apply lateral inhibition to minicolumn
 *
 * WHAT: Reduce minicolumn activation based on neighbor activity
 * WHY:  Implement competitive dynamics and contrast enhancement
 * HOW:  Subtract inhibition value from current activation
 *
 * @param col Minicolumn handle
 * @param inhibition Inhibition strength [0.0, 1.0]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * NOTE: Activation is clamped to [0.0, 1.0] after inhibition
 */
void minicolumn_apply_lateral_inhibition(minicolumn_t* col, float inhibition);

/**
 * @brief Run competition in hypercolumn
 *
 * WHAT: Apply competition dynamics to minicolumn activations
 * WHY:  Implement winner-take-all or softmax competition
 * HOW:  Apply selected competition mode with given parameters
 *
 * @param hcol Hypercolumn handle
 * @param mode Competition mode
 * @param temperature Temperature parameter for softmax (ignored for WTA)
 *
 * COMPLEXITY: O(M) for WTA, O(M log M) for K-winners
 * THREAD-SAFE: Yes
 *
 * ALGORITHMS:
 * - WINNER_TAKE_ALL: Set winner=1.0, others=0.0
 * - K_WINNERS: Set top K=1.0, others=0.0
 * - SOFTMAX: p_i = exp(a_i/T) / Σexp(a_j/T)
 * - NONE: No change to activations
 */
void hypercolumn_run_competition(
    hypercolumn_t* hcol,
    cc_competition_mode_t mode,
    float temperature
);

//=============================================================================
// Receptive Field Operations
//=============================================================================

/**
 * @brief Set minicolumn receptive field
 *
 * WHAT: Update receptive field center and radius
 * WHY:  Configure or adapt receptive field properties
 * HOW:  Update internal receptive field structure
 *
 * @param col Minicolumn handle
 * @param cx Center X coordinate
 * @param cy Center Y coordinate
 * @param cz Center Z coordinate
 * @param radius Receptive field radius (σ)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void minicolumn_set_receptive_field(
    minicolumn_t* col,
    float cx, float cy, float cz,
    float radius
);

/**
 * @brief Compute receptive field weight for a point
 *
 * WHAT: Calculate Gaussian weight for point in feature space
 * WHY:  Determine how strongly point activates this minicolumn
 * HOW:  Gaussian: w(d) = exp(-d²/2σ²) where d = Euclidean distance
 *
 * @param col Minicolumn handle
 * @param x X coordinate in feature space
 * @param y Y coordinate in feature space
 * @param z Z coordinate in feature space
 * @return Weight [0.0, 1.0] or -1.0 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float minicolumn_compute_receptive_weight(
    minicolumn_t* col,
    float x, float y, float z
);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get minicolumn statistics
 *
 * WHAT: Retrieve current statistics for minicolumn
 * WHY:  Monitor activity and performance
 * HOW:  Copy internal stats to output structure
 *
 * @param col Minicolumn handle
 * @param stats Output statistics structure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * ERROR CONDITIONS:
 * - NULL col or stats
 */
void minicolumn_get_stats(minicolumn_t* col, minicolumn_stats_t* stats);

/**
 * @brief Get hypercolumn statistics
 *
 * WHAT: Retrieve current statistics for hypercolumn
 * WHY:  Monitor competition dynamics
 * HOW:  Copy internal stats to output structure
 *
 * @param hcol Hypercolumn handle
 * @param stats Output statistics structure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * ERROR CONDITIONS:
 * - NULL hcol or stats
 */
void hypercolumn_get_stats(hypercolumn_t* hcol, cc_hypercolumn_stats_t* stats);

//=============================================================================
// Plasticity Integration API
//=============================================================================

/* Forward declaration for plasticity bridge */
struct cortical_plasticity_bridge;
typedef struct cortical_plasticity_bridge cortical_plasticity_bridge_t;

/**
 * @brief Set plasticity bridge for cortical columns
 *
 * WHAT: Connect cortical columns to plasticity system
 * WHY:  Enable STDP-based weight updates in column processing
 * HOW:  Store bridge reference for use during computation
 *
 * @param bridge Plasticity bridge (can be NULL to disable)
 *
 * THREAD-SAFE: Yes
 */
void cortical_column_set_plasticity_bridge(cortical_plasticity_bridge_t* bridge);

/**
 * @brief Get current plasticity bridge
 *
 * @return Current plasticity bridge or NULL if not set
 */
cortical_plasticity_bridge_t* cortical_column_get_plasticity_bridge(void);

/**
 * @brief Apply STDP weight update for minicolumn synapses
 *
 * WHAT: Update synaptic weights based on spike timing
 * WHY:  Enable learning within cortical columns
 * HOW:  Calculate timing difference, apply STDP rule via bridge
 *
 * @param col Minicolumn
 * @param pre_spike_time Pre-synaptic spike time (us)
 * @param post_spike_time Post-synaptic spike time (us)
 * @param synapse_id Synapse identifier
 * @return Weight change applied, or 0 if no plasticity bridge
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float minicolumn_apply_stdp(
    minicolumn_t* col,
    uint64_t pre_spike_time,
    uint64_t post_spike_time,
    uint32_t synapse_id
);

/**
 * @brief Notify plasticity system of spike event
 *
 * WHAT: Report spike activity for plasticity processing
 * WHY:  Enable timing-dependent plasticity across modules
 * HOW:  Send spike event via bio-async if connected
 *
 * @param col Minicolumn that spiked
 * @param spike_time Spike timestamp (us)
 * @param is_pre_spike true=pre-synaptic, false=post-synaptic
 *
 * THREAD-SAFE: Yes
 */
void minicolumn_notify_spike(
    minicolumn_t* col,
    uint64_t spike_time,
    bool is_pre_spike
);

/**
 * @brief Apply plasticity to hypercolumn winners
 *
 * WHAT: Update weights for winning minicolumns
 * WHY:  Learning occurs based on competition winners
 * HOW:  Apply STDP to winner's synaptic connections
 *
 * @param hcol Hypercolumn
 * @param current_time Current simulation time (us)
 *
 * THREAD-SAFE: Yes
 */
void hypercolumn_apply_plasticity(
    hypercolumn_t* hcol,
    uint64_t current_time
);

/**
 * @brief Compute hypercolumn with plasticity integration
 *
 * WHAT: Process hypercolumn and apply STDP learning
 * WHY:  Combine computation with online learning
 * HOW:  Call compute, then apply plasticity to winners
 *
 * @param hcol Hypercolumn
 * @param input Input feature vector
 * @param input_size Size of input
 * @param current_time Current simulation time (us)
 *
 * THREAD-SAFE: Yes
 */
void hypercolumn_compute_with_plasticity(
    hypercolumn_t* hcol,
    const float* input,
    uint32_t input_size,
    uint64_t current_time
);

//=============================================================================
// SNN Bridge Integration API
//=============================================================================

/** Forward declaration for SNN network */
struct snn_network_s;
typedef struct snn_network_s cortical_snn_network_t;

/** Forward declaration for SNN population */
struct snn_population_s;
typedef struct snn_population_s cortical_snn_population_t;

/**
 * @brief Set SNN network for spike-based cortical column processing
 * @param network SNN network or NULL to disconnect
 */
void cortical_column_set_snn_network(cortical_snn_network_t* network);

/**
 * @brief Get current SNN network
 * @return Current SNN network or NULL
 */
cortical_snn_network_t* cortical_column_get_snn_network(void);

/**
 * @brief Compute minicolumn activation using SNN spike integration
 * @param col Minicolumn
 * @param spike_times Array of spike timestamps (microseconds)
 * @param num_spikes Number of spikes
 * @param current_time Current simulation time (us)
 * @return Computed activation level [0,1] or -1 on error
 */
float minicolumn_compute_spike_based(
    minicolumn_t* col,
    const uint64_t* spike_times,
    uint32_t num_spikes,
    uint64_t current_time
);

/**
 * @brief Hypercolumn spike-based computation
 * @param hcol Hypercolumn
 * @param spike_times Array of spike time arrays per minicolumn
 * @param spike_counts Number of spikes per minicolumn
 * @param current_time Current simulation time (us)
 */
void hypercolumn_compute_spike_based(
    hypercolumn_t* hcol,
    const uint64_t** spike_times,
    const uint32_t* spike_counts,
    uint64_t current_time
);

/**
 * @brief Generate output spikes from hypercolumn activity
 * @param hcol Hypercolumn
 * @param out_spike_times Output buffer for spike times
 * @param out_neuron_ids Output buffer for neuron IDs
 * @param max_spikes Maximum spikes to generate
 * @param current_time Current simulation time (us)
 * @return Number of spikes generated
 */
uint32_t hypercolumn_generate_spikes(
    hypercolumn_t* hcol,
    uint64_t* out_spike_times,
    uint32_t* out_neuron_ids,
    uint32_t max_spikes,
    uint64_t current_time
);

/**
 * @brief Connect hypercolumn to SNN population
 * @param hcol Hypercolumn
 * @param population SNN population
 * @return 0 on success, -1 on error
 */
int hypercolumn_connect_snn_population(
    hypercolumn_t* hcol,
    cortical_snn_population_t* population
);

/**
 * @brief Disconnect hypercolumn from SNN population
 * @param hcol Hypercolumn
 * @return 0 on success, -1 on error
 */
int hypercolumn_disconnect_snn_population(hypercolumn_t* hcol);

/**
 * @brief Check if hypercolumn is connected to SNN
 * @param hcol Hypercolumn
 * @return true if connected
 */
bool hypercolumn_is_snn_connected(const hypercolumn_t* hcol);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_CORTICAL_COLUMN_H
