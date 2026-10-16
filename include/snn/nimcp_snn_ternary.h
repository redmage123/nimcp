//=============================================================================
// nimcp_snn_ternary.h - Ternary Weights for Spiking Neural Networks
//=============================================================================
/**
 * @file nimcp_snn_ternary.h
 * @brief Ternary synaptic weight support for SNN module
 *
 * WHAT: Ternary {-1, 0, +1} weight representation for SNN synapses
 * WHY:  20x memory reduction for large-scale SNN simulations
 * HOW:  Pack 5 ternary weights per byte using base-243 encoding
 *
 * BIOLOGICAL BASIS:
 * - Biological synapses are effectively ternary:
 *   - Inhibitory (-1): GABAergic synapses, suppress postsynaptic activity
 *   - Silent (0): Pruned or dormant synapses, no effect
 *   - Excitatory (+1): Glutamatergic synapses, increase activity
 * - Weight magnitudes can be modeled separately as learnable scales
 *
 * MEMORY COMPARISON (1M synapses):
 * | Representation | Size   | Access Speed |
 * |----------------|--------|--------------|
 * | float32        | 4 MB   | Fastest      |
 * | float16        | 2 MB   | Fast         |
 * | int8           | 1 MB   | Fast         |
 * | Ternary packed | 200 KB | Moderate     |
 *
 * STDP INTEGRATION:
 * - Standard STDP produces continuous weight changes
 * - Ternary STDP uses discrete state transitions:
 *   - 0 + LTP → +1 (above threshold)
 *   - 0 + LTD → -1 (below threshold)
 *   - +1 + LTD → 0 (weakening)
 *   - -1 + LTP → 0 (strengthening)
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#ifndef NIMCP_SNN_TERNARY_H
#define NIMCP_SNN_TERNARY_H

#include "nimcp_snn_types.h"
#include "utils/ternary/nimcp_ternary.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default LTP threshold for ternary weight transitions */
#define SNN_TERNARY_LTP_THRESHOLD 0.3f

/** Default LTD threshold for ternary weight transitions */
#define SNN_TERNARY_LTD_THRESHOLD -0.3f

/** Default positive weight scale (effective weight for +1) */
#define SNN_TERNARY_WEIGHT_POSITIVE_SCALE 1.0f

/** Default negative weight scale (effective weight for -1) */
#define SNN_TERNARY_WEIGHT_NEGATIVE_SCALE -1.0f

/** Magic number for ternary SNN validation */
#define SNN_TERNARY_MAGIC 0x534E5433  /* "SNT3" */

//=============================================================================
// Ternary Synapse Types
//=============================================================================

/**
 * @brief Compact ternary synapse (5 bytes)
 *
 * WHAT: Single synapse with ternary weight
 * WHY:  58% smaller than standard 12-byte synapse
 * HOW:  Store only target ID and ternary weight
 */
typedef struct {
    uint32_t target_id;     /**< Target neuron ID */
    int8_t weight;          /**< Ternary weight: -1, 0, or +1 */
} snn_ternary_synapse_t;

/**
 * @brief Ternary weight matrix for population connectivity
 *
 * WHAT: Packed ternary weight matrix between populations
 * WHY:  20x memory savings for dense connectivity
 * HOW:  Base-243 encoding: 5 weights per byte
 */
typedef struct {
    uint32_t magic;             /**< Validation magic */
    uint32_t pre_size;          /**< Number of presynaptic neurons */
    uint32_t post_size;         /**< Number of postsynaptic neurons */
    ternary_pack_mode_t mode;   /**< Packing mode */
    trit_matrix_t* weights;     /**< Packed weight matrix */

    /* Learnable scales (optional) */
    float positive_scale;       /**< Scale for +1 weights */
    float negative_scale;       /**< Scale for -1 weights */

    /* STDP state (accumulated before discretization) */
    float* accumulated_delta;   /**< Continuous delta accumulator */
    float ltp_threshold;        /**< Threshold for 0→+1 or -1→0 */
    float ltd_threshold;        /**< Threshold for 0→-1 or +1→0 */

    /* Statistics */
    uint64_t n_ltp_events;      /**< Count of LTP transitions */
    uint64_t n_ltd_events;      /**< Count of LTD transitions */
    uint64_t n_updates;         /**< Total update count */
} snn_ternary_weight_matrix_t;

/**
 * @brief Configuration for ternary weight training
 */
typedef struct {
    float ltp_threshold;        /**< LTP threshold for weight increase */
    float ltd_threshold;        /**< LTD threshold for weight decrease */
    float positive_scale;       /**< Effective weight for +1 */
    float negative_scale;       /**< Effective weight for -1 */
    float accumulation_decay;   /**< Decay rate for accumulated deltas */
    bool use_stochastic_round;  /**< Stochastic vs deterministic rounding */
    uint32_t update_interval;   /**< Steps between discretization updates */
    ternary_pack_mode_t pack_mode; /**< Packing mode for storage */
} snn_ternary_config_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create ternary weight matrix
 *
 * @param pre_size Number of presynaptic neurons
 * @param post_size Number of postsynaptic neurons
 * @param config Ternary configuration (NULL for defaults)
 * @return New ternary weight matrix, or NULL on failure
 */
snn_ternary_weight_matrix_t* snn_ternary_create(
    uint32_t pre_size,
    uint32_t post_size,
    const snn_ternary_config_t* config
);

/**
 * @brief Destroy ternary weight matrix
 *
 * @param weights Matrix to destroy
 */
void snn_ternary_destroy(snn_ternary_weight_matrix_t* weights);

/**
 * @brief Initialize default ternary configuration
 *
 * @param config Configuration to initialize
 */
void snn_ternary_default_config(snn_ternary_config_t* config);

/**
 * @brief Convert continuous weight matrix to ternary
 *
 * @param float_weights Continuous weight matrix (row-major)
 * @param pre_size Number of presynaptic neurons
 * @param post_size Number of postsynaptic neurons
 * @param threshold Quantization threshold
 * @param config Ternary configuration (NULL for defaults)
 * @return New ternary weight matrix, or NULL on failure
 */
snn_ternary_weight_matrix_t* snn_ternary_from_floats(
    const float* float_weights,
    uint32_t pre_size,
    uint32_t post_size,
    float threshold,
    const snn_ternary_config_t* config
);

/**
 * @brief Convert ternary weight matrix to continuous
 *
 * @param weights Ternary weight matrix
 * @param output Output float array (caller allocated, pre_size * post_size)
 * @return 0 on success, error code on failure
 */
int snn_ternary_to_floats(
    const snn_ternary_weight_matrix_t* weights,
    float* output
);

//=============================================================================
// Weight Access
//=============================================================================

/**
 * @brief Get ternary weight value
 *
 * @param weights Ternary weight matrix
 * @param pre_idx Presynaptic neuron index
 * @param post_idx Postsynaptic neuron index
 * @return Trit weight value (-1, 0, or +1)
 */
static inline trit_t snn_ternary_get_weight(
    const snn_ternary_weight_matrix_t* weights,
    uint32_t pre_idx,
    uint32_t post_idx
) {
    if (!weights || !weights->weights) return TRIT_UNKNOWN;
    if (pre_idx >= weights->pre_size || post_idx >= weights->post_size) {
        return TRIT_UNKNOWN;
    }
    return trit_matrix_get(weights->weights, pre_idx, post_idx);
}

/**
 * @brief Set ternary weight value
 *
 * @param weights Ternary weight matrix
 * @param pre_idx Presynaptic neuron index
 * @param post_idx Postsynaptic neuron index
 * @param value New ternary weight (-1, 0, or +1)
 * @return 0 on success, error code on failure
 */
static inline int snn_ternary_set_weight(
    snn_ternary_weight_matrix_t* weights,
    uint32_t pre_idx,
    uint32_t post_idx,
    trit_t value
) {
    if (!weights || !weights->weights) return -1;
    if (pre_idx >= weights->pre_size || post_idx >= weights->post_size) {
        return -1;
    }
    return trit_matrix_set(weights->weights, pre_idx, post_idx, value);
}

/**
 * @brief Get effective (scaled) weight value
 *
 * @param weights Ternary weight matrix
 * @param pre_idx Presynaptic neuron index
 * @param post_idx Postsynaptic neuron index
 * @return Scaled float weight
 */
static inline float snn_ternary_get_effective_weight(
    const snn_ternary_weight_matrix_t* weights,
    uint32_t pre_idx,
    uint32_t post_idx
) {
    if (!weights) return 0.0f;
    trit_t w = snn_ternary_get_weight(weights, pre_idx, post_idx);
    if (w == TRIT_POSITIVE) return weights->positive_scale;
    if (w == TRIT_NEGATIVE) return weights->negative_scale;
    return 0.0f;
}

//=============================================================================
// Forward Pass
//=============================================================================

/**
 * @brief Compute weighted sum of input spikes (ternary)
 *
 * WHAT: Matrix-vector multiply with ternary weights
 * WHY:  Efficient spike integration for postsynaptic neurons
 * HOW:  y = W @ x where W is ternary, x is spike vector
 *
 * @param weights Ternary weight matrix
 * @param input_spikes Input spike vector (0 or 1 for each neuron)
 * @param output Output weighted sum (post_size elements)
 * @return 0 on success, error code on failure
 */
int snn_ternary_forward(
    const snn_ternary_weight_matrix_t* weights,
    const uint8_t* input_spikes,
    float* output
);

/**
 * @brief Compute weighted sum with continuous input
 *
 * @param weights Ternary weight matrix
 * @param input Continuous input vector (pre_size elements)
 * @param output Output vector (post_size elements)
 * @return 0 on success, error code on failure
 */
int snn_ternary_forward_float(
    const snn_ternary_weight_matrix_t* weights,
    const float* input,
    float* output
);

//=============================================================================
// STDP Learning
//=============================================================================

/**
 * @brief Apply STDP update to ternary weights
 *
 * WHAT: Accumulate STDP delta and apply discrete transitions
 * WHY:  Learning with ternary weights requires state machine
 * HOW:  Accumulate continuous delta, discretize when threshold crossed
 *
 * STATE TRANSITIONS:
 * - 0 → +1: accumulated_delta >= ltp_threshold
 * - 0 → -1: accumulated_delta <= ltd_threshold
 * - +1 → 0: accumulated_delta <= ltd_threshold
 * - -1 → 0: accumulated_delta >= ltp_threshold
 *
 * @param weights Ternary weight matrix
 * @param pre_idx Presynaptic neuron index
 * @param post_idx Postsynaptic neuron index
 * @param delta STDP delta value (positive for LTP, negative for LTD)
 * @return 1 if weight changed, 0 if no change, negative on error
 */
int snn_ternary_stdp_update(
    snn_ternary_weight_matrix_t* weights,
    uint32_t pre_idx,
    uint32_t post_idx,
    float delta
);

/**
 * @brief Apply batch STDP updates
 *
 * @param weights Ternary weight matrix
 * @param deltas Delta matrix (pre_size x post_size, row-major)
 * @return Number of weight changes applied
 */
int snn_ternary_stdp_batch(
    snn_ternary_weight_matrix_t* weights,
    const float* deltas
);

/**
 * @brief Decay accumulated deltas
 *
 * WHAT: Apply exponential decay to accumulated deltas
 * WHY:  Prevent runaway accumulation, implement eligibility traces
 * HOW:  accumulated_delta *= decay_factor
 *
 * @param weights Ternary weight matrix
 * @param decay_factor Decay multiplier [0, 1]
 */
void snn_ternary_decay(
    snn_ternary_weight_matrix_t* weights,
    float decay_factor
);

/**
 * @brief Force discretization of all accumulated deltas
 *
 * WHAT: Apply thresholds to all accumulated deltas
 * WHY:  Periodic cleanup, end-of-epoch processing
 * HOW:  Apply state transitions for all weights that crossed thresholds
 *
 * @param weights Ternary weight matrix
 * @return Number of weight changes applied
 */
int snn_ternary_discretize(snn_ternary_weight_matrix_t* weights);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Ternary weight statistics
 */
typedef struct {
    uint32_t n_positive;        /**< Count of +1 weights */
    uint32_t n_unknown;         /**< Count of 0 weights (silent) */
    uint32_t n_negative;        /**< Count of -1 weights */
    float sparsity;             /**< Fraction of silent weights */
    float balance;              /**< Ratio of positive to negative */
    uint64_t n_ltp_total;       /**< Total LTP events */
    uint64_t n_ltd_total;       /**< Total LTD events */
    size_t memory_bytes;        /**< Memory usage in bytes */
    float compression_ratio;    /**< Compared to float32 */
} snn_ternary_stats_t;

/**
 * @brief Compute ternary weight statistics
 *
 * @param weights Ternary weight matrix
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int snn_ternary_get_stats(
    const snn_ternary_weight_matrix_t* weights,
    snn_ternary_stats_t* stats
);

/**
 * @brief Print ternary weight statistics
 *
 * @param weights Ternary weight matrix
 */
void snn_ternary_print_stats(const snn_ternary_weight_matrix_t* weights);

//=============================================================================
// Serialization
//=============================================================================

/**
 * @brief Serialize ternary weights to buffer
 *
 * @param weights Ternary weight matrix
 * @param buffer Output buffer (NULL to query size)
 * @param buffer_size Buffer size
 * @return Bytes written, or required size if buffer is NULL
 */
size_t snn_ternary_serialize(
    const snn_ternary_weight_matrix_t* weights,
    uint8_t* buffer,
    size_t buffer_size
);

/**
 * @brief Deserialize ternary weights from buffer
 *
 * @param buffer Input buffer
 * @param buffer_size Buffer size
 * @return New ternary weight matrix, or NULL on failure
 */
snn_ternary_weight_matrix_t* snn_ternary_deserialize(
    const uint8_t* buffer,
    size_t buffer_size
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_TERNARY_H */
