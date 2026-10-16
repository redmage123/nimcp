//=============================================================================
// nimcp_shannon.h - Shannon Information Theory for Neural Networks
//=============================================================================
/**
 * @file nimcp_shannon.h
 * @brief Shannon information theory metrics and optimization for NIMCP
 *
 * WHAT: Implements Shannon channel capacity, entropy, and mutual information
 * WHY:  Quantify and optimize information flow in neural networks
 * HOW:  Apply information theory to synapses, neurons, and networks
 *
 * MATHEMATICAL FOUNDATION:
 *
 * SHANNON CHANNEL CAPACITY:
 * C = B × log₂(1 + SNR)
 * Where:
 * - C = capacity (bits/second)
 * - B = bandwidth (Hz)
 * - SNR = signal-to-noise ratio
 *
 * SHANNON ENTROPY:
 * H(X) = -Σ p(x) log₂ p(x)
 * Where:
 * - H(X) = entropy in bits
 * - p(x) = probability of state x
 *
 * MUTUAL INFORMATION:
 * I(X;Y) = H(X) + H(Y) - H(X,Y)
 * Where:
 * - I(X;Y) = shared information between X and Y
 * - H(X,Y) = joint entropy
 *
 * BIOLOGICAL INTERPRETATION:
 * - Channel capacity = maximum information a synapse can transmit
 * - Entropy = uncertainty/information content of neural state
 * - Mutual information = how much knowing X tells us about Y
 *
 * APPLICATIONS IN NIMCP:
 * 1. Synapse optimization: Maximize channel capacity
 * 2. Network analysis: Identify information bottlenecks
 * 3. Learning: Information-theoretic plasticity rules
 * 4. Attention: Allocate resources to high-information regions
 * 5. Compression: Preserve information while reducing parameters
 *
 * PERFORMANCE:
 * - Single synapse capacity: O(1)
 * - Network entropy: O(N) where N = number of neurons
 * - Mutual information: O(N²) for pairwise, O(N log N) with approximations
 *
 * REFERENCES:
 * - Shannon, C.E. (1948). "A Mathematical Theory of Communication"
 * - Cover & Thomas (2006). "Elements of Information Theory"
 * - Barlow, H.B. (1961). "Possible Principles Underlying Sensory Messages"
 *
 * @author NIMCP Development Team
 * @date 2025-11-13
 * @version 3.0.0 Phase C4
 */

#ifndef NIMCP_SHANNON_H
#define NIMCP_SHANNON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Minimum probability to avoid log(0) */
#define SHANNON_EPSILON (1e-10f)

/** Natural base for conversion (ln(2) for log₂) */
#define SHANNON_LOG2_E (1.4426950408889634f)

/** Maximum reasonable SNR (60 dB) */
#define SHANNON_MAX_SNR (1e6f)

/** Minimum bandwidth (Hz) */
#define SHANNON_MIN_BANDWIDTH (0.1f)

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Shannon metrics for a single synapse
 *
 * WHAT: Information-theoretic characterization of synaptic transmission
 * WHY:  Quantify how much information the synapse can transmit
 * HOW:  Compute from weight, noise, firing rate
 *
 * BIOLOGICAL MEANING:
 * - High capacity → reliable information transmission
 * - High entropy → uncertain/variable state
 * - High mutual info → strong correlation with presynaptic activity
 */
typedef struct {
    float channel_capacity;      ///< C bits/second (Shannon capacity)
    float shannon_entropy;       ///< H(X) bits (uncertainty)
    float mutual_information;    ///< I(pre;post) bits (correlation)
    float information_rate;      ///< dI/dt bits/second (throughput)
    float coding_efficiency;     ///< H/C_max ratio 0-1 (how well capacity used)
    float signal_power;          ///< Signal strength
    float noise_power;           ///< Noise level
    float snr;                   ///< Signal-to-noise ratio
    float bandwidth;             ///< Effective bandwidth (Hz)
} shannon_synapse_metrics_t;

/**
 * @brief Shannon metrics for a neuron
 *
 * WHAT: Information content and flow for single neuron
 * WHY:  Understand neuron's role in information processing
 * HOW:  Aggregate from input/output synapses
 */
typedef struct {
    uint64_t neuron_id;          ///< Neuron identifier
    float state_entropy;         ///< H(state) bits
    float spike_entropy;         ///< H(spikes) bits (temporal)
    float input_information;     ///< Total input info (bits/second)
    float output_information;    ///< Total output info (bits/second)
    float information_gain;      ///< Output - Input (bits/second)
    float total_input_capacity;  ///< Sum of input synapse capacities
    float total_output_capacity; ///< Sum of output synapse capacities
    uint32_t num_inputs;         ///< Number of input synapses
    uint32_t num_outputs;        ///< Number of output synapses
} shannon_neuron_metrics_t;

/**
 * @brief Shannon metrics for entire network
 *
 * WHAT: Global information-theoretic statistics
 * WHY:  Assess overall network information processing
 * HOW:  Aggregate from all neurons and synapses
 */
typedef struct {
    float total_capacity;        ///< Sum of all synapse capacities (bits/s)
    float total_entropy;         ///< Network state entropy (bits)
    float mutual_information;    ///< Network-level I(input;output)
    float information_rate;      ///< Current throughput (bits/s)
    float average_efficiency;    ///< Mean coding efficiency 0-1
    float bottleneck_score;      ///< 0-1: 1=no bottlenecks, 0=severe
    uint32_t num_bottlenecks;    ///< Count of low-capacity synapses
    uint32_t num_neurons;        ///< Total neurons analyzed
    uint32_t num_synapses;       ///< Total synapses analyzed
} shannon_network_metrics_t;

/**
 * @brief Configuration for Shannon analysis
 *
 * WHAT: Control parameters for information-theoretic computations
 * WHY:  Tune accuracy vs performance
 * HOW:  Adjust thresholds and sampling parameters
 */
typedef struct {
    float min_probability;       ///< Threshold for p(x) in entropy (default 1e-10)
    float min_capacity;          ///< Minimum capacity to consider (bits/s)
    float bottleneck_threshold;  ///< Capacity ratio for bottleneck detection
    bool use_log_approximation;  ///< Use fast log approximation
    bool normalize_entropy;      ///< Normalize by max possible entropy
    uint32_t sampling_window_ms; ///< Time window for rate estimation
} shannon_config_t;

/**
 * @brief Probability distribution for entropy calculation
 *
 * WHAT: Discrete probability distribution p(x)
 * WHY:  Generic container for computing entropy
 * HOW:  Array of probabilities summing to 1.0
 */
typedef struct {
    float* probabilities;        ///< p(x) for each state [num_states]
    uint32_t num_states;         ///< Number of discrete states
    float total_probability;     ///< Should be 1.0 (for validation)
} shannon_distribution_t;

/**
 * @brief Joint probability distribution for mutual information
 *
 * WHAT: P(X,Y) for two variables
 * WHY:  Compute I(X;Y) = H(X) + H(Y) - H(X,Y)
 * HOW:  2D array of joint probabilities
 */
typedef struct {
    float* joint_probabilities;  ///< p(x,y) [num_x_states × num_y_states]
    uint32_t num_x_states;       ///< Number of X states
    uint32_t num_y_states;       ///< Number of Y states
    float total_probability;     ///< Should be 1.0
} shannon_joint_distribution_t;

/**
 * @brief Information bottleneck detected in network
 *
 * WHAT: Synapse or neuron with low capacity relative to demand
 * WHY:  Identify performance bottlenecks
 * HOW:  Capacity < threshold × average capacity
 */
typedef struct {
    uint64_t synapse_id;         ///< ID of bottleneck synapse
    float capacity;              ///< Actual capacity (bits/s)
    float demand;                ///< Desired information flow (bits/s)
    float bottleneck_ratio;      ///< demand / capacity (>1 = bottleneck)
    float suggested_weight;      ///< Recommended weight to fix bottleneck
} shannon_bottleneck_t;

//=============================================================================
// Core Shannon Functions
//=============================================================================

/**
 * @brief Compute Shannon channel capacity
 *
 * WHAT: C = B × log₂(1 + SNR)
 * WHY:  Fundamental limit on information transmission
 * HOW:  Apply Shannon-Hartley theorem
 *
 * INTERPRETATION:
 * - C = maximum bits/second that can be reliably transmitted
 * - Doubling bandwidth doubles capacity (linear)
 * - Doubling SNR increases capacity logarithmically
 *
 * EXAMPLE:
 * - bandwidth = 100 Hz, SNR = 10 → C ≈ 346 bits/s
 * - bandwidth = 100 Hz, SNR = 100 → C ≈ 665 bits/s
 *
 * @param bandwidth Bandwidth in Hz (must be > 0)
 * @param snr Signal-to-noise ratio (linear, not dB)
 * @return Channel capacity in bits/second
 *
 * COMPLEXITY: O(1)
 */
float shannon_channel_capacity(float bandwidth, float snr);

/**
 * @brief Compute Shannon entropy of probability distribution
 *
 * WHAT: H(X) = -Σ p(x) log₂ p(x)
 * WHY:  Measure uncertainty/information content
 * HOW:  Sum over all possible states
 *
 * INTERPRETATION:
 * - H = average number of bits needed to encode outcome
 * - H = 0 → deterministic (no uncertainty)
 * - H = log₂(N) → uniform distribution (maximum uncertainty)
 *
 * EXAMPLE:
 * - Fair coin: p(H)=0.5, p(T)=0.5 → H = 1 bit
 * - Biased coin: p(H)=0.9, p(T)=0.1 → H ≈ 0.47 bits
 * - Deterministic: p(H)=1, p(T)=0 → H = 0 bits
 *
 * @param distribution Probability distribution
 * @return Entropy in bits
 *
 * COMPLEXITY: O(N) where N = number of states
 */
float shannon_entropy(const shannon_distribution_t* distribution);

/**
 * @brief Compute Shannon entropy from probability array
 *
 * WHAT: Convenience wrapper for shannon_entropy()
 * WHY:  Avoid creating distribution_t for simple cases
 * HOW:  Wrap array in distribution_t internally
 *
 * @param probabilities Array of probabilities [num_states]
 * @param num_states Number of states
 * @return Entropy in bits
 *
 * COMPLEXITY: O(N)
 */
float shannon_entropy_array(const float* probabilities, uint32_t num_states);

/**
 * @brief Compute mutual information I(X;Y)
 *
 * WHAT: I(X;Y) = H(X) + H(Y) - H(X,Y)
 * WHY:  Measure how much knowing X reduces uncertainty about Y
 * HOW:  Compute from joint distribution
 *
 * INTERPRETATION:
 * - I(X;Y) = 0 → X and Y are independent
 * - I(X;Y) = H(X) = H(Y) → X and Y are perfectly correlated
 * - I(X;Y) > 0 → X and Y share information
 *
 * EXAMPLE:
 * - X = input spike, Y = output spike
 * - I(X;Y) = how much input spike tells us about output
 * - High I → strong synaptic connection
 *
 * @param joint_distribution Joint probability P(X,Y)
 * @return Mutual information in bits
 *
 * COMPLEXITY: O(N × M) where N, M are number of states
 */
float shannon_mutual_information(const shannon_joint_distribution_t* joint_distribution);

/**
 * @brief Compute conditional entropy H(Y|X)
 *
 * WHAT: H(Y|X) = H(X,Y) - H(X)
 * WHY:  Uncertainty about Y given we know X
 * HOW:  Derived from joint and marginal entropies
 *
 * INTERPRETATION:
 * - H(Y|X) = 0 → Y is deterministic given X
 * - H(Y|X) = H(Y) → X tells us nothing about Y (independent)
 *
 * @param joint_distribution Joint probability P(X,Y)
 * @return Conditional entropy in bits
 *
 * COMPLEXITY: O(N × M)
 */
float shannon_conditional_entropy(const shannon_joint_distribution_t* joint_distribution);

/**
 * @brief Compute relative entropy (Kullback-Leibler divergence)
 *
 * WHAT: D(P||Q) = Σ p(x) log₂(p(x)/q(x))
 * WHY:  Measure how different two distributions are
 * HOW:  Sum of log probability ratios
 *
 * INTERPRETATION:
 * - D(P||Q) = 0 → P and Q are identical
 * - D(P||Q) > 0 → P differs from Q
 * - NOT symmetric: D(P||Q) ≠ D(Q||P)
 *
 * @param p First distribution
 * @param q Second distribution
 * @return KL divergence in bits (≥ 0)
 *
 * COMPLEXITY: O(N)
 */
float shannon_kl_divergence(
    const shannon_distribution_t* p,
    const shannon_distribution_t* q
);

/**
 * @brief Compute Jensen-Shannon divergence (symmetric KL)
 *
 * WHAT: JSD(P||Q) = 0.5 × D_KL(P||M) + 0.5 × D_KL(Q||M) where M = 0.5(P+Q)
 * WHY:  Symmetric, bounded measure of distribution difference
 * HOW:  Average of KL divergences to mixture
 *
 * INTERPRETATION:
 * - JSD(P||Q) = 0 → P and Q are identical
 * - JSD(P||Q) ≤ 1 bit (bounded unlike KL)
 * - JSD(P||Q) = JSD(Q||P) (symmetric)
 *
 * COMPLEXITY: O(N)
 */
float shannon_js_divergence(
    const shannon_distribution_t* p,
    const shannon_distribution_t* q
);

/**
 * @brief Compute cross-entropy H(P,Q)
 *
 * WHAT: H(P,Q) = -Σ p(x) log₂ q(x)
 * WHY:  Loss function for classification, relates to KL divergence
 * HOW:  Expected code length using Q to encode P
 *
 * INTERPRETATION:
 * - H(P,Q) = H(P) + D_KL(P||Q)
 * - Minimizing cross-entropy = minimizing KL divergence
 * - Common loss function in neural network training
 *
 * @param p True distribution
 * @param q Predicted/model distribution
 * @return Cross-entropy in bits
 *
 * COMPLEXITY: O(N)
 */
float shannon_cross_entropy(
    const shannon_distribution_t* p,
    const shannon_distribution_t* q
);

/**
 * @brief Compute Shannon entropy in natural units (nats)
 *
 * WHAT: H(X) = -Σ p(x) ln(p(x))
 * WHY:  Natural unit preferred in neural networks (gradient-friendly)
 * HOW:  Use natural log instead of log base 2
 *
 * CONVERSION: H_nats = H_bits × ln(2) ≈ H_bits × 0.693
 *
 * @param distribution Probability distribution
 * @return Entropy in nats (natural units)
 *
 * COMPLEXITY: O(N)
 */
float shannon_entropy_nats(const shannon_distribution_t* distribution);

/**
 * @brief Convenience: compute entropy in nats from array
 */
float shannon_entropy_nats_array(const float* probabilities, uint32_t num_states);

//=============================================================================
// Synapse-Level Shannon Analysis
//=============================================================================

/**
 * @brief Compute Shannon metrics for single synapse
 *
 * WHAT: Analyze information-theoretic properties of synapse
 * WHY:  Understand synapse's contribution to information processing
 * HOW:  Compute capacity, entropy, mutual info from synapse parameters
 *
 * ALGORITHM:
 * 1. Signal power = weight² × pre_firing_rate
 * 2. Noise power = noise_level²
 * 3. SNR = signal_power / noise_power
 * 4. Capacity = bandwidth × log₂(1 + SNR)
 * 5. Entropy = H(transmission state)
 * 6. Mutual info = I(pre_activity; post_activity)
 *
 * @param weight Synaptic weight (0-1 or -1 to 1)
 * @param pre_firing_rate Presynaptic firing rate (Hz)
 * @param noise_level Noise standard deviation
 * @param bandwidth Effective bandwidth (Hz), use firing rate if unknown
 * @param config Configuration parameters (can be NULL for defaults)
 * @return Shannon metrics for the synapse
 *
 * COMPLEXITY: O(1)
 */
shannon_synapse_metrics_t shannon_analyze_synapse(
    float weight,
    float pre_firing_rate,
    float noise_level,
    float bandwidth,
    const shannon_config_t* config
);

/**
 * @brief Optimize synapse weight to maximize channel capacity
 *
 * WHAT: Find weight that maximizes information transmission
 * WHY:  Information-theoretic plasticity rule
 * HOW:  Increase weight if capacity < demand, decrease if excess noise
 *
 * ALGORITHM:
 * - Current capacity: C_current = B × log₂(1 + SNR_current)
 * - Target capacity: C_target (provided)
 * - Solve for weight: w_new such that C(w_new) = C_target
 * - Apply learning rate: w = w_old + η × (w_new - w_old)
 *
 * @param current_weight Current synaptic weight
 * @param target_capacity Desired capacity (bits/second)
 * @param pre_firing_rate Presynaptic firing rate (Hz)
 * @param noise_level Noise standard deviation
 * @param learning_rate Step size (0-1)
 * @return Optimized weight
 *
 * COMPLEXITY: O(1)
 */
float shannon_optimize_synapse_weight(
    float current_weight,
    float target_capacity,
    float pre_firing_rate,
    float noise_level,
    float learning_rate
);

//=============================================================================
// Neuron-Level Shannon Analysis
//=============================================================================

/**
 * @brief Compute Shannon metrics for single neuron
 *
 * WHAT: Aggregate information flow through neuron
 * WHY:  Identify high-information neurons
 * HOW:  Sum input/output capacities, compute state entropy
 *
 * @param neuron_id Neuron identifier
 * @param input_synapses Array of input synapse metrics [num_inputs]
 * @param num_inputs Number of input synapses
 * @param output_synapses Array of output synapse metrics [num_outputs]
 * @param num_outputs Number of output synapses
 * @param neuron_state Current neuron state (for entropy)
 * @param spike_history Recent spike times [history_length]
 * @param history_length Number of recent spikes
 * @param config Configuration (can be NULL)
 * @return Shannon metrics for the neuron
 *
 * COMPLEXITY: O(N + M) where N=inputs, M=outputs
 */
shannon_neuron_metrics_t shannon_analyze_neuron(
    uint64_t neuron_id,
    const shannon_synapse_metrics_t* input_synapses,
    uint32_t num_inputs,
    const shannon_synapse_metrics_t* output_synapses,
    uint32_t num_outputs,
    float neuron_state,
    const uint64_t* spike_history,
    uint32_t history_length,
    const shannon_config_t* config
);

//=============================================================================
// Network-Level Shannon Analysis
//=============================================================================

/**
 * @brief Compute Shannon metrics for entire network
 *
 * WHAT: Global information-theoretic characterization
 * WHY:  Assess overall network information processing capability
 * HOW:  Aggregate from all synapses and neurons
 *
 * @param synapse_metrics Array of per-synapse metrics [num_synapses]
 * @param num_synapses Number of synapses
 * @param neuron_metrics Array of per-neuron metrics [num_neurons]
 * @param num_neurons Number of neurons
 * @param config Configuration (can be NULL)
 * @return Network-level Shannon metrics
 *
 * COMPLEXITY: O(S + N) where S=synapses, N=neurons
 */
shannon_network_metrics_t shannon_analyze_network(
    const shannon_synapse_metrics_t* synapse_metrics,
    uint32_t num_synapses,
    const shannon_neuron_metrics_t* neuron_metrics,
    uint32_t num_neurons,
    const shannon_config_t* config
);

/**
 * @brief Detect information bottlenecks in network
 *
 * WHAT: Find synapses with capacity < demand
 * WHY:  Identify performance-limiting connections
 * HOW:  Compare actual capacity to average network capacity
 *
 * ALGORITHM:
 * 1. Compute average synapse capacity
 * 2. For each synapse: if capacity < threshold × average, it's a bottleneck
 * 3. Estimate demand from pre/post activity correlation
 * 4. Return bottlenecks sorted by severity
 *
 * @param synapse_metrics Array of synapse metrics [num_synapses]
 * @param num_synapses Number of synapses
 * @param bottleneck_threshold Ratio (e.g., 0.5 = capacity < 50% of average)
 * @param bottlenecks Output array (allocated by caller) [max_bottlenecks]
 * @param max_bottlenecks Maximum number to return
 * @return Number of bottlenecks found
 *
 * COMPLEXITY: O(S log S) where S=synapses (includes sorting)
 */
uint32_t shannon_detect_bottlenecks(
    const shannon_synapse_metrics_t* synapse_metrics,
    uint32_t num_synapses,
    float bottleneck_threshold,
    shannon_bottleneck_t* bottlenecks,
    uint32_t max_bottlenecks
);

/**
 * @brief Compute information flow rate through network
 *
 * WHAT: dI/dt = rate of information transmission (bits/second)
 * WHY:  Real-time measure of network activity
 * HOW:  Sum of all active synapse information rates
 *
 * @param synapse_metrics Array of synapse metrics [num_synapses]
 * @param num_synapses Number of synapses
 * @param time_window_ms Time window for rate estimation
 * @return Information flow rate in bits/second
 *
 * COMPLEXITY: O(S)
 */
float shannon_information_flow_rate(
    const shannon_synapse_metrics_t* synapse_metrics,
    uint32_t num_synapses,
    float time_window_ms
);

//=============================================================================
// Distribution Utilities
//=============================================================================

/**
 * @brief Create probability distribution
 *
 * WHAT: Allocate and initialize distribution
 * WHY:  Prepare for entropy calculation
 * HOW:  Allocate array, set to uniform or provided values
 *
 * @param num_states Number of discrete states
 * @param probabilities Initial probabilities (can be NULL for uniform)
 * @return Allocated distribution, or NULL on failure
 *
 * COMPLEXITY: O(N)
 * MEMORY: sizeof(shannon_distribution_t) + N × sizeof(float)
 */
shannon_distribution_t* shannon_distribution_create(
    uint32_t num_states,
    const float* probabilities
);

/**
 * @brief Free probability distribution
 *
 * @param distribution Distribution to free (NULL-safe)
 *
 * COMPLEXITY: O(1)
 */
void shannon_distribution_free(shannon_distribution_t* distribution);

/**
 * @brief Normalize distribution to sum to 1.0
 *
 * WHAT: Ensure Σ p(x) = 1.0
 * WHY:  Required for valid probability distribution
 * HOW:  Divide all probabilities by sum
 *
 * @param distribution Distribution to normalize (modified in-place)
 * @return true if successful, false if sum = 0
 *
 * COMPLEXITY: O(N)
 */
bool shannon_distribution_normalize(shannon_distribution_t* distribution);

/**
 * @brief Create joint probability distribution
 *
 * @param num_x_states Number of X states
 * @param num_y_states Number of Y states
 * @param joint_probabilities P(X,Y) [num_x × num_y] (can be NULL for uniform)
 * @return Allocated joint distribution, or NULL on failure
 *
 * COMPLEXITY: O(N × M)
 * MEMORY: sizeof(shannon_joint_distribution_t) + N × M × sizeof(float)
 */
shannon_joint_distribution_t* shannon_joint_distribution_create(
    uint32_t num_x_states,
    uint32_t num_y_states,
    const float* joint_probabilities
);

/**
 * @brief Free joint distribution
 *
 * @param joint_distribution Joint distribution to free (NULL-safe)
 *
 * COMPLEXITY: O(1)
 */
void shannon_joint_distribution_free(shannon_joint_distribution_t* joint_distribution);

/**
 * @brief Compute marginal distribution P(X) from joint P(X,Y)
 *
 * WHAT: P(X=x) = Σ_y P(X=x, Y=y)
 * WHY:  Needed for mutual information calculation
 * HOW:  Sum over Y dimension
 *
 * @param joint_distribution Joint distribution P(X,Y)
 * @return Marginal distribution P(X), or NULL on failure
 *
 * COMPLEXITY: O(N × M)
 */
shannon_distribution_t* shannon_marginal_x(
    const shannon_joint_distribution_t* joint_distribution
);

/**
 * @brief Compute marginal distribution P(Y) from joint P(X,Y)
 *
 * @param joint_distribution Joint distribution P(X,Y)
 * @return Marginal distribution P(Y), or NULL on failure
 *
 * COMPLEXITY: O(N × M)
 */
shannon_distribution_t* shannon_marginal_y(
    const shannon_joint_distribution_t* joint_distribution
);

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Get default Shannon configuration
 *
 * WHAT: Return sensible default parameters
 * WHY:  Good starting point for most use cases
 * HOW:  Set standard thresholds
 *
 * DEFAULTS:
 * - min_probability: 1e-10
 * - min_capacity: 0.1 bits/s
 * - bottleneck_threshold: 0.5 (50% of average)
 * - use_log_approximation: false
 * - normalize_entropy: false
 * - sampling_window_ms: 1000
 *
 * @return Default configuration
 *
 * COMPLEXITY: O(1)
 */
shannon_config_t shannon_default_config(void);

/**
 * @brief Get high-accuracy configuration
 *
 * WHAT: Maximize precision at cost of performance
 * WHY:  Critical analysis or validation
 * HOW:  Disable approximations, use tight thresholds
 *
 * @return High-accuracy configuration
 *
 * COMPLEXITY: O(1)
 */
shannon_config_t shannon_high_accuracy_config(void);

/**
 * @brief Get fast approximation configuration
 *
 * WHAT: Maximize speed at cost of accuracy
 * WHY:  Real-time analysis of large networks
 * HOW:  Enable approximations, use loose thresholds
 *
 * @return Fast configuration
 *
 * COMPLEXITY: O(1)
 */
shannon_config_t shannon_fast_config(void);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Fast log₂ approximation
 *
 * WHAT: Approximate log₂(x) using bit manipulation
 * WHY:  5-10x faster than standard log2f()
 * HOW:  Extract exponent + polynomial approximation for mantissa
 *
 * ACCURACY: ±0.5% error
 * SPEEDUP: 5-10x vs log2f()
 *
 * @param x Input value (must be > 0)
 * @return Approximate log₂(x)
 *
 * COMPLEXITY: O(1)
 */
float shannon_log2_fast(float x);

/**
 * @brief Convert SNR from linear to dB
 *
 * WHAT: SNR_dB = 10 × log₁₀(SNR_linear)
 * WHY:  Common representation in engineering
 * HOW:  Standard conversion formula
 *
 * @param snr_linear SNR in linear scale
 * @return SNR in decibels (dB)
 *
 * COMPLEXITY: O(1)
 */
float shannon_snr_to_db(float snr_linear);

/**
 * @brief Convert SNR from dB to linear
 *
 * WHAT: SNR_linear = 10^(SNR_dB / 10)
 * WHY:  Shannon formula uses linear SNR
 * HOW:  Standard conversion formula
 *
 * @param snr_db SNR in decibels
 * @return SNR in linear scale
 *
 * COMPLEXITY: O(1)
 */
float shannon_snr_from_db(float snr_db);

/**
 * @brief Print Shannon metrics (debugging)
 *
 * @param metrics Synapse, neuron, or network metrics
 * @param label Optional label (can be NULL)
 *
 * COMPLEXITY: O(1)
 */
void shannon_print_synapse_metrics(
    const shannon_synapse_metrics_t* metrics,
    const char* label
);

void shannon_print_neuron_metrics(
    const shannon_neuron_metrics_t* metrics,
    const char* label
);

void shannon_print_network_metrics(
    const shannon_network_metrics_t* metrics,
    const char* label
);

/**
 * @brief Shutdown Shannon module, allowing reinitialization
 */
void shannon_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SHANNON_H
