// nimcp_neuralnet.h

#ifndef NIMCP_NEURALNET_H
#define NIMCP_NEURALNET_H

#include "common/nimcp_export.h"
#include "constants/nimcp_dimension_constants.h"
#include "core/neuron_models/nimcp_neuron_model.h"
#include "core/neuron_types/nimcp_neuron_types.h"  // Phase 8.7: Specialized neuron types
#include "core/neuralnet/nimcp_sparse_synapse.h"   // NIMCP 2.11: Sparse synapse storage
#include "plasticity/stp/nimcp_stp.h"
#include "plasticity/bcm/nimcp_bcm.h"  // Phase 11: BCM homeostatic plasticity
#include "utils/ternary/nimcp_ternary.h"  // Ternary weight support

// Forward declare neuron_t so we can use it in function pointers before it's fully defined
// NOTE: neuron_t is defined below as typedef struct { ... } neuron_t;
typedef struct neuron_struct neuron_t;

// Forward declarations for programmable synapse types (NIMCP 2.7)
// WHY: Avoid circular dependency with synapse_compute.h
// PATTERN: Use incomplete types, full definitions in synapse_compute.h
// HOW: Pointers to these types are valid, but struct contents are opaque here
struct synapse_compute_context_t;
struct synapse_compute_state_t;

// Forward declaration for eligibility traces (Phase 11)
// WHY: synapse_t uses eligibility_trace_t*, need forward declaration before synapse_t
// Full definition included after synapse_t to avoid circular dependency
typedef struct eligibility_trace_t eligibility_trace_t;

// Phase 8.7: Include synapse type system
// CRITICAL: Must come AFTER neuron_t forward declaration
#include "core/synapse_types/nimcp_synapse_types.h"

// Phase 11: BCM plasticity types
// NOTE: Already included above (line 10)

// Function pointer types for synapse computation (NIMCP 2.7)
// DESIGN: Define types here so synapse_t can use them without including synapse_compute.h
// NOTE: We use neuron_t* (typedef) not struct neuron_t* (which doesn't exist yet)
typedef float (*synapse_compute_fn)(
    struct synapse_t* syn,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_activity,
    struct synapse_compute_context_t* context
);
typedef void (*synapse_learn_fn)(
    struct synapse_t* syn,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike_time,
    float post_spike_time,
    float reward_signal,
    struct synapse_compute_context_t* context
);

/**
 * @file nimcp_neuralnet.h
 * @brief Neural network implementation with biological learning mechanisms
 *
 * This module implements a biologically-inspired neural network with:
 * - Hebbian learning (STDP)
 * - Oja's learning rule for PCA
 * - Synaptic plasticity and scaling
 * - Homeostatic plasticity
 * - Excitatory/Inhibitory balance
 * - Activity-dependent plasticity
 * - Adaptive thresholding
 *
 * @author NIMCP Development Team
 * @date 2025-02-04
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Network size and capacity constants
 *
 * WHAT: Maximum network capacity and per-neuron limits
 * WHY: Support BRAIN_SIZE_LARGE (100K neurons) while preventing unbounded allocation
 * HOW: Neurons array is dynamically allocated up to MAX_NEURONS
 */
#define MAX_NEURONS 3000000  // Support up to 3M neurons (2.5M target + headroom)
#define MAX_SYNAPSES_PER_NEURON 256
#define SPIKE_HISTORY_LENGTH 1000
#define SPIKE_HISTORY_DEFAULT_CAPACITY 128
#define ACTIVITY_HISTORY_DEFAULT_CAPACITY 32
#define HISTORY_WINDOW 100
#define WEIGHT_UPDATE_THRESHOLD 1e-6f
#define ACTIVITY_THRESHOLD 1e-5f
#define NORM_THRESHOLD 1e-7f
#define NORMALIZATION_INTERVAL 1000  // ms

// Phase 8.7: neuron_type_t now defined in nimcp_neuron_types.h
// This provides backward compatibility with NEURON_EXCITATORY/INHIBITORY
// plus new specialized types (V1 simple/complex, A1 frequency-tuned, etc.)

/**
 * @brief Learning rule types
 */
typedef enum {
    LEARNING_HEBBIAN,         /**< Basic Hebbian learning */
    LEARNING_OJA,             /**< Oja's learning rule */
    LEARNING_GENERALIZED_OJA, /**< Generalized Oja's rule */
    LEARNING_STDP,            /**< Spike-timing dependent plasticity */
    LEARNING_HYBRID           /**< Combined learning rules */
} learning_rule_t;

/**
 * @brief Activation function types
 */
typedef enum {
    ACTIVATION_SIGMOID,    /**< Sigmoid activation function */
    ACTIVATION_TANH,       /**< Hyperbolic tangent activation */
    ACTIVATION_RELU,       /**< Rectified Linear Unit */
    ACTIVATION_LEAKY_RELU, /**< Leaky Rectified Linear Unit */
    ACTIVATION_ADAPTIVE,   /**< Adaptive threshold function */
    ACTIVATION_LINEAR      /**< Identity/linear — no transform (regression output) */
} activation_type_t;

/**
 * @brief Network statistics structure
 */
typedef struct {
    uint32_t num_neurons;
    uint32_t num_inhibitory;
    uint32_t num_excitatory;
    uint32_t total_synapses;
    float avg_activity;
    float avg_weight;
    float avg_strength;
    float avg_plasticity;
    float avg_calcium;
    float network_stability;
    uint64_t network_time;
} network_stats_t;

/**
 * @brief Spike timing record structure
 */
typedef struct {
    uint64_t timestamp; /**< Spike timestamp */
    float magnitude;    /**< Spike magnitude */
} spike_record_t;

/**
 * @brief Oja learning parameters
 */
typedef struct {
    float alpha;         /**< Learning rate parameter */
    float forgetting;    /**< Forgetting/normalization factor */
    float stabilization; /**< Stabilization parameter */
    float target_norm;   /**< Target norm for weights */
} oja_params_t;

/**
 * @brief Union of neuron type parameters for inline storage
 *
 * WHAT: Replaces void* type_params with inline union (no heap allocation)
 * WHY:  Eliminates pointer indirection and malloc/free for type params
 * HOW:  Union of all known model parameter types + raw fallback
 */
typedef union {
    lif_params_t lif;
    uint8_t raw[64]; /**< Reserve space for any model params */
} neuron_type_params_union_t;

/**
 * @brief Cold (rarely-accessed) neuron data
 *
 * WHAT: Struct for neuron fields that are NOT on the hot path
 * WHY:  Cache line optimization - keep hot fields (state, bias, threshold,
 *       activation_type, synapses) packed in neuron_struct, push cold
 *       fields (oja_params, creation_time, model_type, type_params) here.
 * HOW:  Allocated once per neuron alongside the neuron array.
 *       Access via neuron->cold->field.
 *
 * NOTE: For backward compatibility, the original fields remain in neuron_struct.
 *       New code should prefer neuron->cold->field for cache-friendly access.
 *       The cold struct fields are authoritative; the neuron_struct copies
 *       will be deprecated in a future release.
 */
typedef struct {
    oja_params_t oja_params;              /**< Oja learning parameters */
    uint64_t creation_time;               /**< Neuron creation timestamp */
    neuron_model_type_t model_type;       /**< Type of model being used */
    neuron_type_params_union_t type_params; /**< Type-specific parameters (inline, no heap) */
} neuron_cold_data_t;

/**
 * @brief Create cold data struct for a neuron
 * @return Heap-allocated cold data initialized to zero, or NULL on failure
 */
neuron_cold_data_t* neuron_cold_data_create(void);

/**
 * @brief Destroy cold data struct
 * @param cold Pointer to cold data (NULL-safe)
 */
void neuron_cold_data_destroy(neuron_cold_data_t* cold);

/**
 * @brief STDP parameters
 */
typedef struct {
    float learning_rate;   /**< STDP learning rate */
    float time_window;     /**< Time window for STDP (ms) */
    float positive_factor; /**< Factor for potentiation */
    float negative_factor; /**< Factor for depression */
} stdp_params_t;

/**
 * @brief Homeostatic plasticity parameters
 */
typedef struct {
    float target_activity; /**< Target average activity */
    float time_scale;      /**< Time scale for averaging */
    float strength;        /**< Strength of homeostatic adjustment */

    // Immune system integration
    float baseline_target_activity;  /**< Original target activity (before immune modulation) */
    float inflammation_modulation;   /**< Inflammation effect on set point (0.0-1.0) */
    float cytokine_scaling_factor;   /**< Cytokine effect on scaling rate (-1.0 to 1.0) */
    float metabolic_load;            /**< Current metabolic load from immune response (0.0-1.0) */
    float allostatic_load;           /**< Accumulated allostatic burden (0.0-inf, 0=healthy) */
    uint64_t inflammation_start;     /**< When inflammation began (0 = no inflammation) */
} homeostatic_params_t;

/**
 * @brief Synapse cold data — rarely-accessed fields, allocated on demand
 *
 * NIMCP 2.11: Hot/cold split for memory efficiency.
 * Only ~5% of synapses need cold data (STP, BCM, eligibility, compute fns,
 * typed synapse state, embeddings, ternary weights).
 * For 2M neurons: cold pool ~1 GB vs 23 GB if stored inline.
 */
#define SYNAPSE_COLD_NONE UINT32_MAX

typedef struct synapse_cold_t {
    // Short-term plasticity (NIMCP 2.6)
    stp_state_t stp;       /**< Short-term plasticity state */
    bool enable_stp;       /**< Enable STP for this synapse */

    // BCM homeostatic plasticity (Phase 11: Plasticity Wiring)
    bcm_synapse_t* bcm;  /**< BCM sliding threshold state (NULL = disabled) */
    bool enable_bcm;     /**< Enable BCM for this synapse */

    // Eligibility traces (Phase 11: Plasticity Wiring)
    eligibility_trace_t* eligibility;  /**< Eligibility trace for RL (NULL = disabled) */
    bool enable_eligibility;           /**< Enable eligibility traces for this synapse */

    // Programmable computation (NIMCP 2.7)
    synapse_compute_fn compute_function;  /**< Custom computation (NULL = default) */
    synapse_learn_fn learn_function;      /**< Custom learning (NULL = default) */
    struct synapse_compute_state_t* compute_state; /**< Function-specific state (NULL = none) */

    // Synapse type system (NIMCP 2.8.7 / Phase 8.7)
    synapse_type_t type;           /**< Synapse type (AMPA, NMDA, GABA-A, etc) */
    synapse_type_state_t type_state; /**< Type-specific state (conductance, modulation, etc) */

    // Semantic Embeddings (NIMCP 2.9, CPU-staged in 2.6.4)
    uint32_t embedding_pool_index; /**< Index into network's CPU embedding pool (NIMCP_EMBEDDING_POOL_NONE = none) */
    uint16_t embedding_dim;        /**< Embedding dimension (0 = no embedding, typically 2048) */

    // Ternary Weight Support (NIMCP 2.10)
    trit_t ternary_weight;         /**< Ternary weight {-1, 0, +1} */
    bool use_ternary_weight;       /**< Use ternary weight instead of float weight */
    float ternary_scale;           /**< Scale factor for ternary dequantization (default 1.0) */
} synapse_cold_t;

/**
 * @brief Synapse structure — HOT path only (~56 bytes)
 *
 * DESIGN EVOLUTION:
 * - NIMCP 2.0-2.5: Synapse = weight (monolithic)
 * - NIMCP 2.6-2.10: Synapse = weight + STP + type + compute (~200 bytes)
 * - NIMCP 2.11: Hot/cold split — hot = learning-critical fields (~56 bytes),
 *   cold = STP/BCM/eligibility/compute/type/embeddings (on-demand, ~128 bytes)
 *
 * HOT struct stored in metadata pool (every outgoing synapse with metadata).
 * COLD struct stored in separate cold pool (only synapses that need advanced features).
 * Reduces metadata pool from ~32 GB to ~8 GB for 2M-neuron networks.
 */
typedef struct synapse_t {
    uint32_t target_id;        /**< Target neuron ID */
    float weight;              /**< Synaptic weight (synced with handle) */
    float plasticity;          /**< Plasticity coefficient */
    float last_change;         /**< Last weight change (for momentum) */
    uint64_t last_active;      /**< Last activation timestamp */
    float strength;            /**< Synaptic strength (separate from weight) */
    float meta_plasticity;     /**< Meta-plasticity factor */
    float trace;               /**< Synaptic trace for STDP */

    // Axon integration
    uint32_t source_neuron_id; /**< Pre-synaptic neuron ID (0 = unset/legacy) */
    uint32_t axon_id;          /**< Axon delivering spikes (0 = no axon) */

    // Cached hot-path field from cold (avoids cold lookup in forward pass)
    float semantic_relevance;  /**< Cached relevance score (0-1) for current context */

    // Cold data link
    uint32_t cold_index;       /**< Index into cold pool (SYNAPSE_COLD_NONE = no cold data) */
} synapse_t;

// Phase 11: Eligibility traces for temporal credit assignment
// NOTE: Must come AFTER synapse_t definition (eligibility functions modify synapse->weight)
#include "plasticity/eligibility/nimcp_eligibility_trace.h"

/**
 * @brief Neuron structure
 *
 * NIMCP 2.7: Changed from anonymous struct to named struct (neuron_struct)
 * WHY: Enables forward declaration for use in synapse compute function pointers
 */
typedef struct neuron_struct {
    uint32_t id;             /**< Unique neuron identifier */
    neuron_type_t type;      /**< Excitatory or Inhibitory */
    float state;             /**< Current activation state */
    float rest_potential;    /**< Resting membrane potential */
    float threshold;         /**< Firing threshold */
    float adaptation;        /**< Adaptive threshold parameter */
    float refractory_period; /**< Refractory period (ms) */
    float bias;              /**< Neuron bias value */
    float external_current;  /**< External input current (e.g., from spike encoding) */

    // Learning parameters
    learning_rule_t learning_rule;     /**< Active learning rule */
    activation_type_t activation_type; /**< Activation function type */
    oja_params_t oja_params;           /**< Oja learning parameters */
    stdp_params_t stdp_params;         /**< STDP parameters */
    homeostatic_params_t homeostatic;  /**< Homeostatic parameters */

    // NIMCP 2.11: Sparse synapse storage (replaces fixed arrays)
    // Memory: ~6,164 bytes per neuron (128 embedded synapses × 24 bytes + metadata)
    sparse_synapse_storage_t outgoing;   /**< Outgoing synapses (embedded[128] + overflow) */
    sparse_synapse_storage_t incoming;   /**< Incoming synapses (for O(S) forward pass) */

    // Plasticity parameters
    float plasticity_rate;       /**< Learning rate for plasticity */
    float homeostatic_factor;    /**< Homeostatic plasticity factor */
    float calcium_concentration; /**< Calcium concentration (for STDP) */
    float weight_norm;           /**< Current norm of weights */

    // Activity tracking
    spike_record_t* spike_history;       /**< Dynamic ring buffer (heap-allocated) */
    uint32_t spike_history_capacity;     /**< Ring buffer capacity */
    uint32_t spike_history_index;        /**< Current write position */
    uint32_t spike_history_count;        /**< Valid entries, saturates at capacity */
    float* activity_history;             /**< Dynamic activity history buffer (heap-allocated) */
    uint32_t activity_history_capacity;  /**< Activity history buffer capacity */
    float avg_activity;     /**< Average activity level */
    float ema_activity;     /**< Exponential moving average of activity (alpha=0.05) */
    uint64_t last_spike;    /**< Last spike timestamp */
    uint64_t last_update;   /**< Last state update timestamp */
    uint64_t creation_time; /**< Neuron creation timestamp */

    // Neuron model - Plugin architecture for LIF/Izhikevich/etc
    neuron_model_state_t model;  /**< Neuron dynamics model (NULL = legacy LIF) */
    neuron_model_type_t model_type; /**< Type of model being used */

    // Phase 8.7: Type-specific parameters for specialized neurons
    // Forward declaration from nimcp_neuron_types.h
    void* type_params;  /**< Type-specific parameters (neuron_type_params_t*) */

    // Hot/cold split: rarely-accessed fields mirrored in cold data struct
    // New code should prefer neuron->cold->field for cache-friendly access paths
    neuron_cold_data_t* cold;  /**< Cold data: oja_params, creation_time, model_type, type_params */

    // Axon integration - Signal propagation with realistic conduction delays
    uint32_t axon_id;  /**< Axon ID for this neuron's output (0 = no axon, direct connection) */

    // Dendrite integration - Spatiotemporal signal integration (Phase 1.5.7)
    uint32_t* dendrite_ids;   /**< Array of dendrite IDs for input integration */
    uint32_t num_dendrites;   /**< Number of dendrites (typically 1-5, 0 = direct input) */
} neuron_t;

/**
 * @brief Network configuration parameters
 */
typedef struct {
    // Original NIMCP 2.0 fields
    uint32_t num_neurons;     /**< Initial number of neurons */
    float ei_ratio;           /**< Excitatory/Inhibitory ratio */
    float learning_rate;      /**< Base learning rate */
    float hebbian_rate;       /**< Hebbian learning rate */
    float stdp_window;        /**< STDP time window (ms) */
    float homeostatic_rate;   /**< Homeostatic plasticity rate */
    float target_activity;    /**< Target activity level */
    float adaptation_rate;    /**< Threshold adaptation rate */
    float refractory_period;  /**< Base refractory period (ms) */
    float min_weight;         /**< Minimum synaptic weight */
    float max_weight;         /**< Maximum synaptic weight */
    uint32_t update_interval; /**< Minimum update interval (ms) */

    // NIMCP 2.5 Brain API extensions
    uint32_t input_size;     /**< Number of input neurons */
    uint32_t output_size;    /**< Number of output neurons */
    uint32_t num_layers;     /**< Number of layers */
    uint32_t* layer_sizes;   /**< Size of each layer */
    bool enable_stdp;        /**< Enable STDP learning */
    bool enable_hebbian;     /**< Enable Hebbian learning */
    bool enable_oja;         /**< Enable Oja's rule */
    bool enable_homeostasis; /**< Enable homeostatic plasticity */

    // NIMCP 2.6 Neuron Model Extensions
    neuron_model_type_t neuron_model;  /**< Neuron dynamics model (LIF, Izhikevich, etc) */
    const void* model_params;          /**< Model-specific parameters (izhikevich_params_t*, etc) */

    // Part A1.1: ODE Integration Method (RK4 Support)
    ode_integration_method_t integration_method;  /**< ODE integration algorithm (Euler, RK4) */

    // Advanced Plasticity Controls (conditional allocation for scalability)
    bool enable_bcm;                   /**< Enable BCM plasticity allocation */
    bool enable_eligibility;           /**< Enable eligibility trace allocation */

    // Bio-async integration
    bool enable_bio_async;             /**< Enable bio-async communication */

    // NIMCP 2.10: Ternary Weight Configuration
    // WHAT: Enable ternary weight representation for synapses
    // WHY:  Memory efficiency for large networks (20x savings)
    // HOW:  Store ternary weights {-1, 0, +1} with optional scale factor
    bool enable_ternary_weights;       /**< Enable ternary weight mode for new synapses */
    float ternary_threshold;           /**< Threshold for float->ternary conversion (default 0.3) */
    float ternary_positive_scale;      /**< Scale for +1 weights (default 1.0) */
    float ternary_negative_scale;      /**< Scale for -1 weights (default -1.0) */
    ternary_pack_mode_t ternary_pack_mode;  /**< Packing mode for ternary weight matrices */
    uint32_t spike_history_capacity;     /**< 0 = use SPIKE_HISTORY_DEFAULT_CAPACITY */
    uint32_t activity_history_capacity;  /**< 0 = use ACTIVITY_HISTORY_DEFAULT_CAPACITY */
    bool skip_layer_wiring;              /**< Skip dense layer wiring during create (for resize) */
    uint32_t wiring_threads;             /**< Thread pool size for parallel backbone wiring (0 = auto/4) */
    bool enable_residual;                /**< Enable residual/skip connections (layer L -> L+2) */
} network_config_t;

/**
 * @brief Neural network structure
 */
typedef struct neural_network_struct* neural_network_t;

// Core network functions
neural_network_t neural_network_create(const network_config_t* config);
void neural_network_destroy(neural_network_t network);
bool neural_network_update_neuron(neural_network_t network, uint32_t neuron_id, float input_current,
                                  uint64_t timestamp);
bool neural_network_get_neuron_state(neural_network_t network, uint32_t neuron_id, float* state);

/**
 * @brief Get number of neurons in network
 *
 * WHAT: Return total neuron count
 * WHY: Allow external code to iterate neurons without accessing internals
 * HOW: Return num_neurons field
 *
 * @param network Network instance
 * @return Number of neurons, or 0 if network is NULL
 */
uint32_t neural_network_get_num_neurons(neural_network_t network);

/**
 * @brief Get pointer to neuron structure
 *
 * WHAT: Access neuron internals for serialization/introspection
 * WHY: Enable save/load of synaptic weights without duplicating access logic
 * HOW: Return pointer to neuron in neurons array
 *
 * WARNING: Returns internal pointer - use carefully!
 *
 * @param network Network instance
 * @param neuron_id Neuron ID
 * @return Pointer to neuron, or NULL if invalid
 */
neuron_t* neural_network_get_neuron(neural_network_t network, uint32_t neuron_id);

/**
 * @brief Get outgoing synapse metadata via network pool
 * @param network Network instance (owns the metadata pool)
 * @param neuron Neuron pointer
 * @param index Synapse index within neuron's outgoing storage
 * @return synapse_t* metadata pointer, or NULL
 */
synapse_t* neural_network_get_out_meta(neural_network_t network, neuron_t* neuron, uint32_t index);

/**
 * @brief Get incoming synapse metadata via network pool
 */
synapse_t* neural_network_get_in_meta(neural_network_t network, neuron_t* neuron, uint32_t index);

/**
 * @brief Rebuild all incoming synapses from outgoing data
 * Call after deserialization to populate incoming handles for the forward pass.
 */
bool neural_network_rebuild_incoming(neural_network_t network);

// NIMCP 2.5 - Forward pass for inference
bool neural_network_forward(neural_network_t network, const float* inputs, uint32_t input_size,
                            float* outputs, uint32_t output_size);
uint32_t neural_network_add_neuron(neural_network_t network, activation_type_t activation);
bool neural_network_add_connection(neural_network_t network, uint32_t from_id, uint32_t to_id,
                                   float weight);

/**
 * @brief Add connection with specific synapse type (Phase 8.7)
 *
 * WHAT: Create synapse with biological type (AMPA, NMDA, GABA-A, etc)
 * WHY: Enable biologically realistic synapse diversity
 * HOW: Initialize synapse with type-specific parameters
 *
 * COMPLEXITY: O(1)
 *
 * @param network Neural network
 * @param from_id Source neuron ID
 * @param to_id Target neuron ID
 * @param weight Initial synaptic weight
 * @param type Synapse type (SYNAPSE_AMPA, SYNAPSE_NMDA, etc)
 * @return true if successful, false on error
 */
bool neural_network_add_connection_typed(neural_network_t network, uint32_t from_id, uint32_t to_id,
                                          float weight, synapse_type_t type);
uint32_t neural_network_compute_step(neural_network_t network, uint64_t timestamp);

/** Get active neuron count from last compute step */
uint32_t neural_network_get_active_count(neural_network_t network);

/** Get active neuron IDs array (read-only). Returns NULL if not available. */
const uint32_t* neural_network_get_active_ids(neural_network_t network);

/** Get active neuron ratio (active/total) */
float neural_network_get_sparsity_ratio(neural_network_t network);

// Timing functions
void neural_network_set_time(neural_network_t network, uint64_t timestamp);

// Learning and plasticity functions
uint32_t neural_network_apply_oja(neural_network_t network, uint32_t neuron_id, uint64_t timestamp);
uint32_t neural_network_apply_generalized_oja(neural_network_t network, uint32_t neuron_id,
                                              uint64_t timestamp);
uint32_t neural_network_apply_stdp(neural_network_t network, uint32_t neuron_id,
                                   uint64_t timestamp);
bool neural_network_apply_homeostasis(neural_network_t network, uint32_t neuron_id,
                                      uint64_t timestamp);
uint32_t neural_network_update_plasticity(neural_network_t network, uint32_t neuron_id,
                                          uint64_t timestamp);
bool neural_network_normalize_weights(neural_network_t network, uint32_t neuron_id);

// Activity and state functions
bool neural_network_record_spike(neural_network_t network, uint32_t neuron_id, float magnitude,
                                 uint64_t timestamp);
bool neural_network_adapt_threshold(neural_network_t network, uint32_t neuron_id,
                                    uint64_t timestamp);
float neural_network_compute_activation(neuron_t* neuron, float input);
void neural_network_update_traces(neural_network_t network, uint32_t neuron_id, uint64_t timestamp);

/**
 * @brief Apply reward-modulated learning to all synapses (Phase 11)
 *
 * WHAT: Apply biological plasticity (STDP/BCM/eligibility) with reward signal
 * WHY:  Enable biological learning in supervised/RL contexts
 * HOW:  Iterate all neurons → Apply STDP → Apply eligibility traces → Apply BCM
 *
 * COMPLEXITY: O(N×S) where N = neurons, S = avg synapses per neuron
 *
 * @param network Neural network
 * @param reward Reward signal [0, 1]
 * @param learning_rate Learning rate for weight updates
 * @param current_time Current timestamp
 * @return Number of synapses modified
 */
uint32_t neural_network_apply_reward_learning(neural_network_t network, float reward,
                                              float learning_rate, uint64_t current_time);

/**
 * @brief Apply lateral inhibition (winner-take-all) to output layer
 *
 * @param network Neural network
 * @param output_start First output neuron index
 * @param output_count Number of output neurons
 * @param inhibition_strength How much to suppress losers (0.0-1.0)
 * @return Number of neurons modified
 */
uint32_t neural_network_apply_lateral_inhibition(
    neural_network_t network,
    uint32_t output_start,
    uint32_t output_count,
    float inhibition_strength);

// Analysis and monitoring functions
float neural_network_get_average_activity(neural_network_t network, uint32_t neuron_id);
float neural_network_get_weight_norm(neural_network_t network, uint32_t neuron_id);
void neural_network_get_weight_statistics(neural_network_t network, uint32_t neuron_id, float* mean,
                                          float* std_dev);

// NIMCP 2.6: Neuron model configuration
/**
 * @brief Set neuron model type for a specific neuron
 *
 * WHAT: Changes the dynamics model of an existing neuron
 * WHY: Enables heterogeneous networks with mixed neuron types
 * HOW: Cleans up old model, creates new model with specified type
 *
 * COMPLEXITY: O(1)
 *
 * @param network Neural network
 * @param neuron_id Neuron to modify
 * @param model_type Desired model type (NEURON_MODEL_LIF, NEURON_MODEL_IZHIKEVICH, etc)
 * @param params Model-specific parameters (NULL = use defaults for that model)
 * @return true if successful, false on error
 */
bool neural_network_set_neuron_model(neural_network_t network, uint32_t neuron_id,
                                     neuron_model_type_t model_type, const void* params);

/**
 * @brief Get current network statistics
 *
 * @param network Pointer to neural network
 * @param stats Pointer to stats structure to fill
 * @return true if successful, false otherwise
 */
bool neural_network_get_stats(neural_network_t network, network_stats_t* stats);

// Maintenance and utility functions
void neural_network_maintain(neural_network_t network, uint64_t timestamp);
void neural_network_maintain_homeostasis(neural_network_t network, uint64_t timestamp);
void neural_network_reset(neural_network_t network);
void neural_network_reinit_weights(neural_network_t network);
void neural_network_set_output_activation(neural_network_t network, activation_type_t activation);
uint32_t neural_network_prune_synapses(neural_network_t network, float threshold);
void neural_network_dump_neuron(neural_network_t network, uint32_t neuron_id);

// NIMCP 2.7: NLP Integration - Accessor functions for synapse compute context
/**
 * @brief Set global state for synapse computation (e.g., attention output)
 *
 * WHAT: Attach global state buffer to network for synapse compute functions
 * WHY: Enable synapses to access shared context (attention, etc)
 * HOW: Stores pointer and size, passed to synapses via compute context
 * WHEN: Called by NLP layer after attention computation
 *
 * DESIGN PATTERN: Dependency Injection - external state injected into network
 * COMPLEXITY: O(1)
 *
 * @param network Neural network
 * @param global_state Pointer to global state buffer (e.g., attention output)
 * @param size Size of global state buffer (floats)
 * @return true on success
 */
bool neural_network_set_global_state(neural_network_t network, float* global_state, uint32_t size);

/**
 * @brief Set neuromodulator system for synapse computation
 *
 * WHAT: Attach neuromodulator system to network
 * WHY: Enable synapses to access neuromodulator levels (dopamine, etc)
 * HOW: Stores opaque pointer, queried during synapse computation
 * WHEN: Called by NLP layer during initialization
 *
 * DESIGN PATTERN: Dependency Injection - external system injected
 * COMPLEXITY: O(1)
 *
 * @param network Neural network
 * @param neuromod_system Opaque pointer to neuromodulator system
 * @return true on success
 */
bool neural_network_set_neuromodulator_system(neural_network_t network, void* neuromod_system);

/**
 * @brief Set glial integration system for neuro-glial signaling (Phase 6)
 *
 * WHAT: Attach glial integration system to network
 * WHY: Enable bidirectional neuro-glial communication
 * HOW: Stores opaque pointer, notified on neuron/synapse events
 * WHEN: Called after glial cells are assigned to neurons/synapses
 *
 * DESIGN PATTERN: Observer - glial system observes neural events
 * COMPLEXITY: O(1)
 *
 * @param network Neural network
 * @param glial_system Opaque pointer to glial integration system
 * @return true on success
 */
bool neural_network_set_glial_integration(neural_network_t network, void* glial_system);

/**
 * @brief Read back the currently-attached glial integration pointer.
 * @param network Neural network (NULL-tolerant, returns NULL).
 * @return Opaque pointer set by neural_network_set_glial_integration, or NULL.
 */
void* neural_network_get_glial_integration(neural_network_t network);

/**
 * @brief Get neuromodulation level for synapse computation
 *
 * WHAT: Query current neuromodulation level (dopamine, primarily)
 * WHY: Used by synapse compute context to modulate transmission
 * HOW: Queries attached neuromodulator system
 * WHEN: Called during synapse computation in sum_synaptic_inputs
 *
 * COMPLEXITY: O(1)
 *
 * @param network Neural network
 * @return Current neuromodulation level [0,1] (0 if no system attached)
 */
float neural_network_get_neuromodulation(neural_network_t network);

// Bidirectional synapse access (OPTIMIZATION API)
/**
 * @brief Get count of incoming synapses to a neuron
 *
 * DESIGN PATTERN: Iterator Pattern - access to reverse edges
 * COMPLEXITY: O(1)
 *
 * @param network Neural network
 * @param neuron_id Target neuron
 * @return Number of incoming synapses (0 if invalid neuron or NULL network)
 */
uint32_t neural_network_get_incoming_synapse_count(neural_network_t network, uint32_t neuron_id);

/**
 * @brief Get array of incoming synapses
 *
 * DESIGN PATTERN: Iterator Pattern - provides direct access for O(S) iteration
 * COMPLEXITY: O(1) to get pointer, O(S) to iterate
 *
 * @param network Neural network
 * @param neuron_id Target neuron
 * @param out_synapses Pointer to receive synapse array (read-only)
 * @return Number of incoming synapses (0 if invalid)
 */
uint32_t neural_network_get_incoming_synapses(neural_network_t network, uint32_t neuron_id,
                                               const synapse_t** out_synapses);

//=============================================================================
// Sparse synapse pool accessors (NIMCP 2.11)
//=============================================================================

/**
 * @brief Get synapse handle pool for external code that needs to add/remove synapses
 */
sparse_synapse_pool_t neural_network_get_synapse_handle_pool(neural_network_t network);

/**
 * @brief Get synapse metadata pool for external code that needs full synapse_t access
 */
synapse_metadata_pool_t neural_network_get_synapse_metadata_pool(neural_network_t network);

//=============================================================================
// ENHANCEMENT 1: Synapse Semantic Embeddings API
//=============================================================================

/**
 * @brief Initialize semantic embedding for a synapse
 *
 * WHAT: Allocates and initializes semantic vector for intelligent routing
 * WHY: Enables synapses to route information based on semantic relevance
 * HOW: Random initialization, later refined by learning
 *
 * @param synapse Synapse to initialize
 * @param dim Embedding dimension (128-512 recommended)
 * @return true on success
 */
bool synapse_init_embedding(synapse_t *synapse, uint16_t dim);

/**
 * @brief Compute semantic similarity between two synapses
 *
 * WHAT: Cosine similarity between embedding vectors
 * WHY: Identifies functionally similar synapses
 * HOW: dot(a,b) / (||a|| * ||b||)
 *
 * @param syn1 First synapse
 * @param syn2 Second synapse
 * @return Similarity (-1 to 1, higher = more similar)
 */
float synapse_semantic_similarity(const synapse_t *syn1, const synapse_t *syn2);

/**
 * @brief Update synapse embedding via gradient descent
 *
 * WHAT: Refines embedding based on usage patterns
 * WHY: Learn semantic routing over time
 *
 * @param synapse Synapse to update
 * @param target_embedding Target vector [dim]
 * @param learning_rate Step size
 * @return true on success
 */
bool synapse_update_embedding(synapse_t *synapse, const float *target_embedding, float learning_rate);

/**
 * @brief Compute relevance of synapse to current context
 *
 * WHAT: Similarity between synapse embedding and context vector
 * WHY: Route information through relevant synapses
 *
 * @param synapse Synapse to evaluate
 * @param context_embedding Context vector [dim]
 * @param context_dim Context dimension
 * @return Relevance score (0-1)
 */
float synapse_compute_relevance(synapse_t *synapse, const float *context_embedding, uint16_t context_dim);

/**
 * @brief Free synapse embedding memory
 *
 * @param synapse Synapse to cleanup
 */
void synapse_destroy_embedding(synapse_t *synapse);

//=============================================================================
// ENHANCEMENT 2: Ternary Weight API (NIMCP 2.10)
//=============================================================================

/**
 * @brief Ternary weight matrix for network-level storage
 *
 * WHAT: Efficient storage of ternary weights for entire network
 * WHY:  20x memory savings vs float weight storage
 * HOW:  Packed ternary matrix indexed by source/target neuron IDs
 *
 * MEMORY COMPARISON (10K neurons, 1M synapses):
 * - Float weights: 1M * 4 bytes = 4 MB
 * - Ternary unpacked: 1M * 1 byte = 1 MB
 * - Ternary base-243: 1M * 0.2 bytes = 200 KB
 */
typedef struct {
    trit_matrix_t* weights;        /**< Ternary weight matrix [source x target] */
    float positive_scale;          /**< Scale for +1 weights */
    float negative_scale;          /**< Scale for -1 weights */
    float threshold;               /**< Quantization threshold */
    uint32_t num_neurons;          /**< Matrix dimension */
    bool is_sparse;                /**< Use sparse representation */
} ternary_weight_matrix_t;

/**
 * @brief Create ternary weight matrix for network
 *
 * WHAT: Allocate ternary weight storage for network
 * WHY:  Enable memory-efficient weight representation
 * HOW:  Create packed trit matrix with specified dimensions
 *
 * @param num_neurons Number of neurons (matrix is num_neurons x num_neurons)
 * @param pack_mode Packing mode for storage efficiency
 * @return Ternary weight matrix or NULL on failure
 *
 * COMPLEXITY: O(N^2) allocation
 */
ternary_weight_matrix_t* ternary_weight_matrix_create(
    uint32_t num_neurons,
    ternary_pack_mode_t pack_mode
);

/**
 * @brief Destroy ternary weight matrix
 *
 * @param twm Matrix to destroy
 */
void ternary_weight_matrix_destroy(ternary_weight_matrix_t* twm);

/**
 * @brief Convert float weight to ternary
 *
 * WHAT: Quantize continuous weight to {-1, 0, +1}
 * WHY:  Prepare weight for ternary storage
 * HOW:  Threshold-based quantization
 *
 * @param weight Float weight value
 * @param threshold Quantization threshold (values with |weight| < threshold -> 0)
 * @return Ternary weight
 */
trit_t synapse_weight_to_ternary(float weight, float threshold);

/**
 * @brief Convert ternary weight to float
 *
 * WHAT: Dequantize ternary weight to continuous value
 * WHY:  Enable computation with ternary-stored weights
 * HOW:  Map {-1, 0, +1} to {-scale, 0, +scale}
 *
 * @param ternary_weight Ternary weight value
 * @param positive_scale Scale for +1 weights
 * @param negative_scale Scale for -1 weights
 * @return Float weight value
 */
float synapse_ternary_to_weight(trit_t ternary_weight, float positive_scale, float negative_scale);

/**
 * @brief Enable ternary mode for synapse
 *
 * WHAT: Convert synapse from float to ternary weight mode
 * WHY:  Reduce memory footprint of individual synapse
 * HOW:  Quantize current weight, set use_ternary_weight flag
 *
 * @param synapse Synapse to convert
 * @param threshold Quantization threshold
 * @param scale Scale factor for dequantization
 * @return true on success
 */
bool synapse_enable_ternary_weight(neural_network_t net, synapse_t* synapse, float threshold, float scale);

/**
 * @brief Disable ternary mode for synapse
 *
 * WHAT: Convert synapse back to float weight mode
 * WHY:  Allow fine-grained weight adjustments
 * HOW:  Dequantize ternary weight to float, clear flag
 *
 * @param net Network (for cold pool access)
 * @param synapse Synapse to convert
 * @return true on success
 */
bool synapse_disable_ternary_weight(neural_network_t net, synapse_t* synapse);

/**
 * @brief Get effective weight (handles ternary/float transparently)
 *
 * WHAT: Return weight value regardless of storage mode
 * WHY:  Unified interface for synapse computation
 * HOW:  Check use_ternary_weight in cold, return appropriate value
 *
 * @param net Network (for cold pool access)
 * @param synapse Synapse to query
 * @return Effective weight value
 */
float synapse_get_effective_weight(neural_network_t net, const synapse_t* synapse);

/**
 * @brief Set effective weight (handles ternary/float transparently)
 *
 * WHAT: Set weight value regardless of storage mode
 * WHY:  Unified interface for synapse learning
 * HOW:  If ternary mode, quantize; otherwise set float
 *
 * @param net Network (for cold pool access)
 * @param synapse Synapse to modify
 * @param weight New weight value
 * @param threshold Threshold for ternary quantization (if in ternary mode)
 */
void synapse_set_effective_weight(neural_network_t net, synapse_t* synapse, float weight, float threshold);

/**
 * @brief Export network weights to ternary matrix
 *
 * WHAT: Create ternary weight matrix from network
 * WHY:  Enable efficient storage/transmission of learned weights
 * HOW:  Iterate synapses, quantize to ternary matrix
 *
 * @param network Neural network
 * @param threshold Quantization threshold
 * @param pack_mode Packing mode
 * @return Ternary weight matrix or NULL on failure
 */
ternary_weight_matrix_t* neural_network_export_ternary_weights(
    neural_network_t network,
    float threshold,
    ternary_pack_mode_t pack_mode
);

/**
 * @brief Import ternary weights to network
 *
 * WHAT: Load ternary weights into network synapses
 * WHY:  Restore network from compact representation
 * HOW:  Dequantize ternary values, set synapse weights
 *
 * @param network Neural network
 * @param twm Ternary weight matrix
 * @return Number of weights imported, -1 on error
 */
int neural_network_import_ternary_weights(
    neural_network_t network,
    const ternary_weight_matrix_t* twm
);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_NEURALNET_H
