#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_shannon.c - Shannon Information Theory Implementation
//=============================================================================
/**
 * @file nimcp_shannon.c
 * @brief Implementation of Shannon information theory for NIMCP
 *
 * @author NIMCP Development Team
 * @date 2025-11-13
 * @version 3.0.0 Phase C4
 */

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "security/nimcp_security.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "INFORMATION"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(shannon)

#include "information/nimcp_shannon.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_once.h"
#include "utils/thread/nimcp_atomic.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <float.h>

//=============================================================================
// Module State
//=============================================================================

static nimcp_atomic_bool_t g_shannon_initialized = {0};
static bio_module_context_t g_bio_ctx = NULL;
static bool g_bio_async_enabled = false;
static void* g_security_context = NULL;
static nimcp_platform_once_t g_shannon_init_once = NIMCP_PLATFORM_ONCE_INIT;

//=============================================================================
// Initialization and Cleanup
//=============================================================================

/**
 * @brief Initialize Shannon module (thread-safe via pthread_once)
 */
static void shannon_init_internal(void)
{
    LOG_INFO("Initializing Shannon information module");

    // Register with bio-async router
    g_bio_ctx = NULL;
    g_bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_SYSTEM_SHANNON,
            .module_name = "shannon_information",
            .inbox_capacity = 32,
            .user_data = NULL
        };
        g_bio_ctx = bio_router_register_module(&bio_info);
        if (g_bio_ctx) {
            g_bio_async_enabled = true;
            LOG_DEBUG("Shannon module registered with bio-async router");
        } else {
            LOG_WARN("Failed to register Shannon module with bio-async router");
        }
    }

    // Security context (simplified - no function call needed for now)
    g_security_context = NULL;  // TODO: proper security registration

    nimcp_atomic_store_bool(&g_shannon_initialized, true, NIMCP_MEMORY_ORDER_RELEASE);
    LOG_INFO("Shannon information module initialized successfully");
}

/**
 * @brief Thread-safe Shannon module initialization wrapper
 *
 * WHAT: Ensures Shannon module is initialized exactly once
 * WHY:  Fix TOCTOU race condition on g_shannon_initialized check
 * HOW:  Uses pthread_once via nimcp_platform_once for thread-safe init
 */
static void shannon_init_once(void)
{
    nimcp_platform_once(&g_shannon_init_once, shannon_init_internal);
}

/**
 * @brief Shutdown Shannon module, allowing reinitialization
 *
 * WHAT: Resets module state so it can be reinitialized
 * WHY:  Without resetting g_shannon_init_once, nimcp_platform_once
 *       will not trigger re-initialization after shutdown
 */
void shannon_shutdown(void)
{
    if (!nimcp_atomic_load_bool(&g_shannon_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        return;
    }
    g_bio_ctx = NULL;
    g_bio_async_enabled = false;
    g_security_context = NULL;
    nimcp_atomic_store_bool(&g_shannon_initialized, false, NIMCP_MEMORY_ORDER_RELEASE);
    g_shannon_init_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Safe log₂ that handles zero and negative values
 */
static inline float safe_log2(float x)
{
    if (x <= SHANNON_EPSILON) {
        return 0.0F;
    }
    return log2f(x);
}

//=============================================================================
// Core Shannon Functions
//=============================================================================

float shannon_channel_capacity(float bandwidth, float snr)
{
    shannon_init_once();

    LOG_DEBUG("Computing channel capacity: bandwidth=%.2f Hz, SNR=%.2f", bandwidth, snr);

    // Validate inputs
    if (bandwidth < SHANNON_MIN_BANDWIDTH) {
        LOG_WARN("Bandwidth %.2f below minimum, clamping to %.2f", bandwidth, SHANNON_MIN_BANDWIDTH);
        bandwidth = SHANNON_MIN_BANDWIDTH;
    }

    if (snr < 0.0F) {
        LOG_WARN("Negative SNR %.2f, clamping to 0.0", snr);
        snr = 0.0F;
    }

    // Clamp SNR to reasonable range
    if (snr > SHANNON_MAX_SNR) {
        LOG_WARN("SNR %.2f exceeds maximum, clamping to %.2f", snr, SHANNON_MAX_SNR);
        snr = SHANNON_MAX_SNR;
    }

    // Shannon-Hartley theorem: C = B × log₂(1 + SNR)
    float capacity = bandwidth * log2f(1.0F + snr);

    LOG_DEBUG("Channel capacity computed: %.2f bits/s", capacity);

    return capacity;
}

float shannon_entropy(const shannon_distribution_t* distribution)
{
    shannon_init_once();

    LOG_DEBUG("Computing Shannon entropy");

    if (!distribution || !distribution->probabilities) {
        LOG_ERROR("NULL distribution or probabilities");
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "shannon_entropy: NULL distribution or probabilities");
        return 0.0F;
    }

    LOG_DEBUG("Distribution has %u states", distribution->num_states);

    float entropy = 0.0F;

    // H(X) = -Σ p(x) log₂ p(x)
    for (uint32_t i = 0; i < distribution->num_states; i++) {
        float p = distribution->probabilities[i];

        if (p > SHANNON_EPSILON) {
            entropy -= p * log2f(p);
        }
    }

    LOG_DEBUG("Entropy computed: %.4f bits", entropy);

    return entropy;
}

float shannon_entropy_array(const float* probabilities, uint32_t num_states)
{
    if (!probabilities || num_states == 0) {
        return 0.0F;
    }

    // Create temporary distribution
    shannon_distribution_t dist;
    dist.probabilities = (float*)probabilities;
    dist.num_states = num_states;
    dist.total_probability = 1.0F;

    return shannon_entropy(&dist);
}

float shannon_mutual_information(const shannon_joint_distribution_t* joint_distribution)
{
    shannon_init_once();

    LOG_DEBUG("Computing mutual information");

    if (!joint_distribution || !joint_distribution->joint_probabilities) {
        LOG_ERROR("NULL joint distribution or probabilities");
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "shannon_mutual_information: NULL joint distribution or probabilities");
        return 0.0F;
    }

    uint32_t num_x = joint_distribution->num_x_states;
    uint32_t num_y = joint_distribution->num_y_states;

    LOG_DEBUG("Joint distribution: %u x %u states", num_x, num_y);

    // Compute marginal distributions
    float* p_x = (float*)nimcp_calloc(num_x, sizeof(float));
    float* p_y = (float*)nimcp_calloc(num_y, sizeof(float));

    if (!p_x || !p_y) {
        LOG_ERROR("Failed to allocate marginal distributions");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_x * sizeof(float) + num_y * sizeof(float),
                          "shannon_mutual_information: Failed to allocate marginal distributions");
        nimcp_free(p_x);
        nimcp_free(p_y);
        return 0.0F;
    }

    // Marginalize: P(X=x) = Σ_y P(X=x, Y=y)
    for (uint32_t x = 0; x < num_x; x++) {
        for (uint32_t y = 0; y < num_y; y++) {
            float p_xy = joint_distribution->joint_probabilities[x * num_y + y];
            p_x[x] += p_xy;
            p_y[y] += p_xy;
        }
    }

    // Compute mutual information: I(X;Y) = Σ_x Σ_y p(x,y) log₂(p(x,y) / (p(x)p(y)))
    float mutual_info = 0.0F;

    for (uint32_t x = 0; x < num_x; x++) {
        for (uint32_t y = 0; y < num_y; y++) {
            float p_xy = joint_distribution->joint_probabilities[x * num_y + y];

            if (p_xy > SHANNON_EPSILON && p_x[x] > SHANNON_EPSILON && p_y[y] > SHANNON_EPSILON) {
                float ratio = p_xy / (p_x[x] * p_y[y]);
                mutual_info += p_xy * log2f(ratio);
            }
        }
    }

    nimcp_free(p_x);
    nimcp_free(p_y);

    return mutual_info;
}

float shannon_conditional_entropy(const shannon_joint_distribution_t* joint_distribution)
{
    if (!joint_distribution) {
        return 0.0F;
    }

    // H(Y|X) = H(X,Y) - H(X)

    // Compute joint entropy H(X,Y)
    float joint_entropy = 0.0F;
    uint32_t num_x = joint_distribution->num_x_states;
    uint32_t num_y = joint_distribution->num_y_states;

    for (uint32_t i = 0; i < num_x * num_y; i++) {
        float p = joint_distribution->joint_probabilities[i];
        if (p > SHANNON_EPSILON) {
            joint_entropy -= p * log2f(p);
        }
    }

    // Compute marginal H(X)
    shannon_distribution_t* p_x = shannon_marginal_x(joint_distribution);
    if (!p_x) {
        return 0.0F;
    }

    float marginal_entropy_x = shannon_entropy(p_x);
    shannon_distribution_free(p_x);

    return joint_entropy - marginal_entropy_x;
}

float shannon_kl_divergence(
    const shannon_distribution_t* p,
    const shannon_distribution_t* q)
{
    if (!p || !q || !p->probabilities || !q->probabilities) {
        return 0.0F;
    }

    if (p->num_states != q->num_states) {
        return 0.0F;
    }

    float divergence = 0.0F;

    // D(P||Q) = Σ p(x) log₂(p(x) / q(x))
    for (uint32_t i = 0; i < p->num_states; i++) {
        float p_i = p->probabilities[i];
        float q_i = q->probabilities[i];

        if (p_i > SHANNON_EPSILON && q_i > SHANNON_EPSILON) {
            divergence += p_i * log2f(p_i / q_i);
        }
    }

    return divergence;
}

float shannon_js_divergence(
    const shannon_distribution_t* p,
    const shannon_distribution_t* q)
{
    if (!p || !q || !p->probabilities || !q->probabilities) {
        return 0.0F;
    }

    if (p->num_states != q->num_states) {
        return 0.0F;
    }

    uint32_t n = p->num_states;

    // Create mixture distribution M = 0.5(P + Q)
    shannon_distribution_t m;
    m.num_states = n;
    m.probabilities = (float*)nimcp_malloc(n * sizeof(float));
    if (!m.probabilities) {
        return 0.0F;
    }

    for (uint32_t i = 0; i < n; i++) {
        m.probabilities[i] = 0.5F * (p->probabilities[i] + q->probabilities[i]);
    }
    m.total_probability = 1.0F;

    // JSD = 0.5 * D_KL(P||M) + 0.5 * D_KL(Q||M)
    float jsd = 0.5F * shannon_kl_divergence(p, &m) +
                0.5F * shannon_kl_divergence(q, &m);

    nimcp_free(m.probabilities);
    return jsd;
}

float shannon_cross_entropy(
    const shannon_distribution_t* p,
    const shannon_distribution_t* q)
{
    if (!p || !q || !p->probabilities || !q->probabilities) {
        return 0.0F;
    }

    if (p->num_states != q->num_states) {
        return 0.0F;
    }

    float ce = 0.0F;

    // H(P,Q) = -Σ p(x) log₂(q(x))
    for (uint32_t i = 0; i < p->num_states; i++) {
        float p_i = p->probabilities[i];
        float q_i = q->probabilities[i];

        if (p_i > SHANNON_EPSILON && q_i > SHANNON_EPSILON) {
            ce -= p_i * log2f(q_i);
        } else if (p_i > SHANNON_EPSILON && q_i <= SHANNON_EPSILON) {
            // q_i = 0 where p_i > 0 → infinite cross-entropy
            return INFINITY;
        }
    }

    return ce;
}

float shannon_entropy_nats(const shannon_distribution_t* distribution)
{
    if (!distribution || !distribution->probabilities) {
        return 0.0F;
    }

    float entropy = 0.0F;

    for (uint32_t i = 0; i < distribution->num_states; i++) {
        float p_i = distribution->probabilities[i];

        if (p_i > SHANNON_EPSILON) {
            entropy -= p_i * logf(p_i);  // Natural log for nats
        }
    }

    return entropy;
}

float shannon_entropy_nats_array(const float* probabilities, uint32_t num_states)
{
    if (!probabilities || num_states == 0) {
        return 0.0F;
    }

    float entropy = 0.0F;

    for (uint32_t i = 0; i < num_states; i++) {
        float p_i = probabilities[i];

        if (p_i > SHANNON_EPSILON) {
            entropy -= p_i * logf(p_i);
        }
    }

    return entropy;
}

//=============================================================================
// Synapse-Level Shannon Analysis
//=============================================================================

shannon_synapse_metrics_t shannon_analyze_synapse(
    float weight,
    float pre_firing_rate,
    float noise_level,
    float bandwidth,
    const shannon_config_t* config)
{
    shannon_synapse_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    // Use default config if not provided
    shannon_config_t default_config = shannon_default_config();
    if (!config) {
        config = &default_config;
    }

    // Validate inputs
    weight = nimcp_clampf(weight, -1.0F, 1.0F);
    pre_firing_rate = fmaxf(0.0F, pre_firing_rate);
    noise_level = fmaxf(SHANNON_EPSILON, noise_level);
    bandwidth = fmaxf(SHANNON_MIN_BANDWIDTH, bandwidth);

    // If no bandwidth specified, use firing rate as bandwidth
    if (bandwidth < SHANNON_MIN_BANDWIDTH) {
        bandwidth = fmaxf(pre_firing_rate, SHANNON_MIN_BANDWIDTH);
    }

    // Compute signal and noise power
    // Signal power = weight² × pre_firing_rate
    float signal_power = weight * weight * pre_firing_rate;
    float noise_power = noise_level * noise_level;

    metrics.signal_power = signal_power;
    metrics.noise_power = noise_power;
    metrics.bandwidth = bandwidth;

    // Compute SNR
    if (noise_power > SHANNON_EPSILON) {
        metrics.snr = signal_power / noise_power;
    } else {
        metrics.snr = SHANNON_MAX_SNR;
    }

    // Clamp SNR
    metrics.snr = nimcp_clampf(metrics.snr, 0.0F, SHANNON_MAX_SNR);

    // Compute channel capacity: C = B × log₂(1 + SNR)
    metrics.channel_capacity = shannon_channel_capacity(bandwidth, metrics.snr);

    // Compute entropy of synaptic transmission
    // Model as Bernoulli: probability of transmission = tanh(weight)
    float p_transmit = 0.5F * (tanhf(weight) + 1.0F);
    float p_no_transmit = 1.0F - p_transmit;

    metrics.shannon_entropy = 0.0F;
    if (p_transmit > SHANNON_EPSILON) {
        metrics.shannon_entropy -= p_transmit * log2f(p_transmit);
    }
    if (p_no_transmit > SHANNON_EPSILON) {
        metrics.shannon_entropy -= p_no_transmit * log2f(p_no_transmit);
    }

    // Mutual information (simplified): strong weight → high MI
    // I(pre;post) ≈ H(post) - H(post|pre)
    // For strong synapse, H(post|pre) is low → high MI
    float weight_strength = fabsf(weight);
    metrics.mutual_information = metrics.shannon_entropy * weight_strength;

    // Information rate: capacity weighted by current activity
    float activity_factor = pre_firing_rate / (bandwidth + SHANNON_EPSILON);
    activity_factor = nimcp_clampf(activity_factor, 0.0F, 1.0F);
    metrics.information_rate = metrics.channel_capacity * activity_factor;

    // Coding efficiency: how well capacity is being used
    float max_entropy = 1.0F;  // Binary synapse
    if (metrics.channel_capacity > SHANNON_EPSILON) {
        metrics.coding_efficiency = metrics.shannon_entropy /
                                   fminf(metrics.channel_capacity, max_entropy);
    } else {
        metrics.coding_efficiency = 0.0F;
    }
    metrics.coding_efficiency = nimcp_clampf(metrics.coding_efficiency, 0.0F, 1.0F);

    return metrics;
}

float shannon_optimize_synapse_weight(
    float current_weight,
    float target_capacity,
    float pre_firing_rate,
    float noise_level,
    float learning_rate)
{
    // Current metrics
    shannon_config_t config = shannon_default_config();
    shannon_synapse_metrics_t current_metrics = shannon_analyze_synapse(
        current_weight, pre_firing_rate, noise_level,
        pre_firing_rate, &config
    );

    float current_capacity = current_metrics.channel_capacity;

    // If already at target, no change
    if (fabsf(current_capacity - target_capacity) < 0.01F) {
        return current_weight;
    }

    // Gradient: increase weight increases SNR increases capacity
    // dC/dw = dC/dSNR × dSNR/dw
    // dC/dSNR = B / ((1 + SNR) × ln(2))
    // dSNR/dw = 2w × firing_rate / noise²

    float snr = current_metrics.snr;
    float dC_dSNR = pre_firing_rate / ((1.0F + snr) * logf(2.0F));
    float dSNR_dw = 2.0F * current_weight * pre_firing_rate /
                    (noise_level * noise_level + SHANNON_EPSILON);
    float dC_dw = dC_dSNR * dSNR_dw;

    // Gradient descent toward target
    float error = target_capacity - current_capacity;
    float weight_delta = learning_rate * error * (dC_dw > 0.0F ? 1.0F : -1.0F);

    float new_weight = current_weight + weight_delta;
    new_weight = nimcp_clampf(new_weight, -1.0F, 1.0F);

    return new_weight;
}

//=============================================================================
// Neuron-Level Shannon Analysis
//=============================================================================

shannon_neuron_metrics_t shannon_analyze_neuron(
    uint64_t neuron_id,
    const shannon_synapse_metrics_t* input_synapses,
    uint32_t num_inputs,
    const shannon_synapse_metrics_t* output_synapses,
    uint32_t num_outputs,
    float neuron_state,
    const uint64_t* spike_history,
    uint32_t history_length,
    const shannon_config_t* config)
{
    shannon_neuron_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    metrics.neuron_id = neuron_id;
    metrics.num_inputs = num_inputs;
    metrics.num_outputs = num_outputs;

    // Aggregate input information
    for (uint32_t i = 0; i < num_inputs; i++) {
        metrics.input_information += input_synapses[i].information_rate;
        metrics.total_input_capacity += input_synapses[i].channel_capacity;
    }

    // Aggregate output information
    for (uint32_t i = 0; i < num_outputs; i++) {
        metrics.output_information += output_synapses[i].information_rate;
        metrics.total_output_capacity += output_synapses[i].channel_capacity;
    }

    // Information gain/loss
    metrics.information_gain = metrics.output_information - metrics.input_information;

    // State entropy: treat neuron state as continuous → discretize
    // Map state to 10 bins
    uint32_t num_bins = 10;
    /* P2-SH-2: Use float clamp to match function signature (float, float, float) */
    float state_bin_f = nimcp_clampf(neuron_state * (float)num_bins, 0.0f, (float)(num_bins - 1));
    int state_bin = (int)state_bin_f;

    // Simplified: assume uniform distribution for now
    // Real implementation would maintain state histogram
    metrics.state_entropy = log2f((float)num_bins);

    // Spike entropy from ISI (inter-spike intervals)
    if (spike_history && history_length > 1) {
        // Compute ISI distribution
        uint32_t num_isi_bins = 10;
        float* isi_histogram = (float*)nimcp_calloc(num_isi_bins, sizeof(float));

        if (!isi_histogram) {
            // Log but don't throw - partial result is acceptable here
            LOG_WARN("shannon_analyze_neuron: Failed to allocate ISI histogram");
        }

        if (isi_histogram) {
            // Compute ISIs
            for (uint32_t i = 1; i < history_length; i++) {
                uint64_t isi = spike_history[i] - spike_history[i-1];
                uint32_t bin = (uint32_t)(isi % num_isi_bins);
                isi_histogram[bin] += 1.0F;
            }

            // Normalize
            float total = (float)(history_length - 1);
            for (uint32_t i = 0; i < num_isi_bins; i++) {
                isi_histogram[i] /= total;
            }

            // Compute entropy
            metrics.spike_entropy = shannon_entropy_array(isi_histogram, num_isi_bins);

            nimcp_free(isi_histogram);
        }
    }

    return metrics;
}

//=============================================================================
// Network-Level Shannon Analysis
//=============================================================================

shannon_network_metrics_t shannon_analyze_network(
    const shannon_synapse_metrics_t* synapse_metrics,
    uint32_t num_synapses,
    const shannon_neuron_metrics_t* neuron_metrics,
    uint32_t num_neurons,
    const shannon_config_t* config)
{
    shannon_network_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    metrics.num_synapses = num_synapses;
    metrics.num_neurons = num_neurons;

    // Use default config if not provided
    shannon_config_t default_config = shannon_default_config();
    if (!config) {
        config = &default_config;
    }

    // Aggregate synapse metrics
    float sum_efficiency = 0.0F;
    float avg_capacity = 0.0F;

    for (uint32_t i = 0; i < num_synapses; i++) {
        metrics.total_capacity += synapse_metrics[i].channel_capacity;
        metrics.information_rate += synapse_metrics[i].information_rate;
        sum_efficiency += synapse_metrics[i].coding_efficiency;
    }

    if (num_synapses > 0) {
        metrics.average_efficiency = sum_efficiency / (float)num_synapses;
        avg_capacity = metrics.total_capacity / (float)num_synapses;
    }

    // Aggregate neuron entropy
    for (uint32_t i = 0; i < num_neurons; i++) {
        metrics.total_entropy += neuron_metrics[i].state_entropy;
        metrics.total_entropy += neuron_metrics[i].spike_entropy;
    }

    // Detect bottlenecks
    metrics.num_bottlenecks = 0;
    float bottleneck_capacity_threshold = avg_capacity * config->bottleneck_threshold;

    for (uint32_t i = 0; i < num_synapses; i++) {
        if (synapse_metrics[i].channel_capacity < bottleneck_capacity_threshold) {
            metrics.num_bottlenecks++;
        }
    }

    // Bottleneck score: 0 = many bottlenecks, 1 = no bottlenecks
    if (num_synapses > 0) {
        float bottleneck_ratio = (float)metrics.num_bottlenecks / (float)num_synapses;
        metrics.bottleneck_score = 1.0F - bottleneck_ratio;
    } else {
        metrics.bottleneck_score = 1.0F;
    }

    // Mutual information (simplified): correlation between inputs and outputs
    // Real implementation would require joint distribution P(input, output)
    // For now, estimate from information flow
    if (metrics.total_capacity > SHANNON_EPSILON) {
        metrics.mutual_information = metrics.information_rate * 0.7F;  // Rough estimate
    }

    return metrics;
}

uint32_t shannon_detect_bottlenecks(
    const shannon_synapse_metrics_t* synapse_metrics,
    uint32_t num_synapses,
    float bottleneck_threshold,
    shannon_bottleneck_t* bottlenecks,
    uint32_t max_bottlenecks)
{
    if (!synapse_metrics || !bottlenecks || max_bottlenecks == 0) {
        return 0;
    }

    // Compute average capacity
    float avg_capacity = 0.0F;
    for (uint32_t i = 0; i < num_synapses; i++) {
        avg_capacity += synapse_metrics[i].channel_capacity;
    }
    if (num_synapses > 0) {
        avg_capacity /= (float)num_synapses;
    }

    float threshold_capacity = avg_capacity * bottleneck_threshold;

    // Find bottlenecks
    uint32_t num_found = 0;
    for (uint32_t i = 0; i < num_synapses && num_found < max_bottlenecks; i++) {
        if (synapse_metrics[i].channel_capacity < threshold_capacity) {
            bottlenecks[num_found].synapse_id = i;
            bottlenecks[num_found].capacity = synapse_metrics[i].channel_capacity;
            bottlenecks[num_found].demand = avg_capacity;  // Estimate
            bottlenecks[num_found].bottleneck_ratio =
                bottlenecks[num_found].demand /
                (bottlenecks[num_found].capacity + SHANNON_EPSILON);

            // Suggest weight increase to match average capacity
            // C = B × log₂(1 + SNR), solve for new weight
            float current_snr = synapse_metrics[i].snr;
            float bandwidth = synapse_metrics[i].bandwidth;
            float target_snr = powf(2.0F, avg_capacity / bandwidth) - 1.0F;
            float weight_scale = sqrtf(target_snr / (current_snr + SHANNON_EPSILON));
            bottlenecks[num_found].suggested_weight = weight_scale * 0.5F;  // Heuristic
            bottlenecks[num_found].suggested_weight =
                nimcp_clampf(bottlenecks[num_found].suggested_weight, 0.1F, 1.0F);

            num_found++;
        }
    }

    return num_found;
}

float shannon_information_flow_rate(
    const shannon_synapse_metrics_t* synapse_metrics,
    uint32_t num_synapses,
    float time_window_ms)
{
    if (!synapse_metrics || num_synapses == 0) {
        return 0.0F;
    }

    float total_rate = 0.0F;

    for (uint32_t i = 0; i < num_synapses; i++) {
        total_rate += synapse_metrics[i].information_rate;
    }

    // Scale by time window if needed
    if (time_window_ms > 0.0F && time_window_ms != 1000.0F) {
        total_rate *= (time_window_ms / 1000.0F);
    }

    return total_rate;
}

//=============================================================================
// Distribution Utilities
//=============================================================================

shannon_distribution_t* shannon_distribution_create(
    uint32_t num_states,
    const float* probabilities)
{
    if (num_states == 0) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM,
                    "shannon_distribution_create: num_states cannot be 0");
        return NULL;
    }

    shannon_distribution_t* dist = (shannon_distribution_t*)nimcp_malloc(sizeof(shannon_distribution_t));
    if (!dist) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(shannon_distribution_t),
                          "shannon_distribution_create: Failed to allocate distribution");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dist is NULL");

        return NULL;
    }

    dist->num_states = num_states;
    dist->probabilities = (float*)nimcp_malloc(num_states * sizeof(float));

    if (!dist->probabilities) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_states * sizeof(float),
                          "shannon_distribution_create: Failed to allocate probabilities");
        nimcp_free(dist);
        return NULL;
    }

    if (probabilities) {
        memcpy(dist->probabilities, probabilities, num_states * sizeof(float));
    } else {
        // Initialize to uniform distribution
        float uniform_p = 1.0F / (float)num_states;
        for (uint32_t i = 0; i < num_states; i++) {
            dist->probabilities[i] = uniform_p;
        }
    }

    // Compute total
    dist->total_probability = 0.0F;
    for (uint32_t i = 0; i < num_states; i++) {
        dist->total_probability += dist->probabilities[i];
    }

    return dist;
}

void shannon_distribution_free(shannon_distribution_t* distribution)
{
    if (!distribution) {
        return;
    }

    if (distribution->probabilities) {
        nimcp_free(distribution->probabilities);
    }

    nimcp_free(distribution);
}

bool shannon_distribution_normalize(shannon_distribution_t* distribution)
{
    // Process pending bio-async messages
    if (g_bio_ctx) {
        bio_router_process_inbox(g_bio_ctx, 5);
    }

    if (!distribution || !distribution->probabilities) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "shannon_distribution_normalize: distribution or probabilities is NULL");
        return false;
    }

    float sum = 0.0F;
    for (uint32_t i = 0; i < distribution->num_states; i++) {
        sum += distribution->probabilities[i];
    }

    if (sum < SHANNON_EPSILON) {
        return false;
    }

    for (uint32_t i = 0; i < distribution->num_states; i++) {
        distribution->probabilities[i] /= sum;
    }

    distribution->total_probability = 1.0F;

    return true;
}

shannon_joint_distribution_t* shannon_joint_distribution_create(
    uint32_t num_x_states,
    uint32_t num_y_states,
    const float* joint_probabilities)
{
    if (num_x_states == 0 || num_y_states == 0) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM,
                    "shannon_joint_distribution_create: states cannot be 0");
        return NULL;
    }

    shannon_joint_distribution_t* joint = (shannon_joint_distribution_t*)
        nimcp_malloc(sizeof(shannon_joint_distribution_t));
    if (!joint) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(shannon_joint_distribution_t),
                          "shannon_joint_distribution_create: Failed to allocate joint distribution");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "joint is NULL");

        return NULL;
    }

    joint->num_x_states = num_x_states;
    joint->num_y_states = num_y_states;

    /* P2-SH-1: Use size_t to prevent integer overflow on large distributions */
    size_t total_size = (size_t)num_x_states * (size_t)num_y_states;
    if (total_size / num_x_states != num_y_states) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM,
                    "shannon_joint_distribution_create: allocation overflow");
        nimcp_free(joint);
        return NULL;
    }
    joint->joint_probabilities = (float*)nimcp_malloc(total_size * sizeof(float));

    if (!joint->joint_probabilities) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, total_size * sizeof(float),
                          "shannon_joint_distribution_create: Failed to allocate joint probabilities");
        nimcp_free(joint);
        return NULL;
    }

    if (joint_probabilities) {
        memcpy(joint->joint_probabilities, joint_probabilities,
               total_size * sizeof(float));
    } else {
        // Initialize to uniform
        float uniform_p = 1.0F / (float)total_size;
        for (uint32_t i = 0; i < total_size; i++) {
            joint->joint_probabilities[i] = uniform_p;
        }
    }

    // Compute total
    joint->total_probability = 0.0F;
    for (uint32_t i = 0; i < total_size; i++) {
        joint->total_probability += joint->joint_probabilities[i];
    }

    return joint;
}

void shannon_joint_distribution_free(shannon_joint_distribution_t* joint_distribution)
{
    if (!joint_distribution) {
        return;
    }

    if (joint_distribution->joint_probabilities) {
        nimcp_free(joint_distribution->joint_probabilities);
    }

    nimcp_free(joint_distribution);
}

shannon_distribution_t* shannon_marginal_x(
    const shannon_joint_distribution_t* joint_distribution)
{
    if (!joint_distribution) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "shannon_marginal_x: NULL joint distribution");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "joint_distribution is NULL");

        return NULL;
    }

    uint32_t num_x = joint_distribution->num_x_states;
    uint32_t num_y = joint_distribution->num_y_states;

    float* marginal_probs = (float*)nimcp_calloc(num_x, sizeof(float));
    if (!marginal_probs) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_x * sizeof(float),
                          "shannon_marginal_x: Failed to allocate marginal probs");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "marginal_probs is NULL");

        return NULL;
    }

    // P(X=x) = Σ_y P(X=x, Y=y)
    for (uint32_t x = 0; x < num_x; x++) {
        for (uint32_t y = 0; y < num_y; y++) {
            marginal_probs[x] += joint_distribution->joint_probabilities[x * num_y + y];
        }
    }

    shannon_distribution_t* marginal = shannon_distribution_create(num_x, marginal_probs);
    nimcp_free(marginal_probs);

    return marginal;
}

shannon_distribution_t* shannon_marginal_y(
    const shannon_joint_distribution_t* joint_distribution)
{
    if (!joint_distribution) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "shannon_marginal_y: NULL joint distribution");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "joint_distribution is NULL");

        return NULL;
    }

    uint32_t num_x = joint_distribution->num_x_states;
    uint32_t num_y = joint_distribution->num_y_states;

    float* marginal_probs = (float*)nimcp_calloc(num_y, sizeof(float));
    if (!marginal_probs) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_y * sizeof(float),
                          "shannon_marginal_y: Failed to allocate marginal probs");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "marginal_probs is NULL");

        return NULL;
    }

    // P(Y=y) = Σ_x P(X=x, Y=y)
    for (uint32_t x = 0; x < num_x; x++) {
        for (uint32_t y = 0; y < num_y; y++) {
            marginal_probs[y] += joint_distribution->joint_probabilities[x * num_y + y];
        }
    }

    shannon_distribution_t* marginal = shannon_distribution_create(num_y, marginal_probs);
    nimcp_free(marginal_probs);

    return marginal;
}

//=============================================================================
// Configuration
//=============================================================================

shannon_config_t shannon_default_config(void)
{
    shannon_config_t config;
    config.min_probability = SHANNON_EPSILON;
    config.min_capacity = 0.1F;
    config.bottleneck_threshold = 0.5F;
    config.use_log_approximation = false;
    config.normalize_entropy = false;
    config.sampling_window_ms = 1000;
    return config;
}

shannon_config_t shannon_high_accuracy_config(void)
{
    shannon_config_t config;
    config.min_probability = 1e-12F;
    config.min_capacity = 0.01F;
    config.bottleneck_threshold = 0.8F;
    config.use_log_approximation = false;
    config.normalize_entropy = true;
    config.sampling_window_ms = 10000;
    return config;
}

shannon_config_t shannon_fast_config(void)
{
    shannon_config_t config;
    config.min_probability = 1e-6F;
    config.min_capacity = 1.0F;
    config.bottleneck_threshold = 0.3F;
    config.use_log_approximation = true;
    config.normalize_entropy = false;
    config.sampling_window_ms = 100;
    return config;
}

//=============================================================================
// Utility Functions
//=============================================================================

float shannon_log2_fast(float x)
{
    if (x <= 0.0F) {
        return -FLT_MAX;
    }

    // Use bit manipulation for fast approximation
    union {
        float f;
        uint32_t i;
    } u;

    u.f = x;

    // Extract exponent
    int32_t exponent = ((u.i >> 23) & 0xFF) - 127;

    // Extract mantissa and normalize to [1, 2)
    u.i = (u.i & 0x007FFFFF) | 0x3F800000;
    float mantissa = u.f;

    // Polynomial approximation for log₂(mantissa)
    // log₂(m) ≈ -1.49 + (2.11 - 0.729m)m for m ∈ [1, 2]
    float log2_mantissa = -1.49F + (2.11F - 0.729F * mantissa) * mantissa;

    return (float)exponent + log2_mantissa;
}

float shannon_snr_to_db(float snr_linear)
{
    if (snr_linear <= SHANNON_EPSILON) {
        return -100.0F;  // Very low SNR
    }

    return 10.0F * log10f(snr_linear);
}

float shannon_snr_from_db(float snr_db)
{
    return powf(10.0F, snr_db / 10.0F);
}

/* Intentional printf to stdout - these are display/diagnostic functions */
void shannon_print_synapse_metrics(
    const shannon_synapse_metrics_t* metrics,
    const char* label)
{
    if (!metrics) {
        return;
    }

    if (label) {
        printf("=== Shannon Synapse Metrics: %s ===\n", label);
    } else {
        printf("=== Shannon Synapse Metrics ===\n");
    }

    printf("  Channel Capacity:  %.2f bits/s\n", metrics->channel_capacity);
    printf("  Shannon Entropy:   %.3f bits\n", metrics->shannon_entropy);
    printf("  Mutual Information: %.3f bits\n", metrics->mutual_information);
    printf("  Information Rate:  %.2f bits/s\n", metrics->information_rate);
    printf("  Coding Efficiency: %.1f%%\n", metrics->coding_efficiency * 100.0F);
    printf("  SNR:               %.2f (%.1f dB)\n",
           metrics->snr, shannon_snr_to_db(metrics->snr));
    printf("  Signal Power:      %.3f\n", metrics->signal_power);
    printf("  Noise Power:       %.3f\n", metrics->noise_power);
    printf("  Bandwidth:         %.1f Hz\n", metrics->bandwidth);
    printf("\n");
}

void shannon_print_neuron_metrics(
    const shannon_neuron_metrics_t* metrics,
    const char* label)
{
    if (!metrics) {
        return;
    }

    if (label) {
        printf("=== Shannon Neuron Metrics: %s ===\n", label);
    } else {
        printf("=== Shannon Neuron Metrics ===\n");
    }

    printf("  Neuron ID:          %lu\n", (unsigned long)metrics->neuron_id);
    printf("  State Entropy:      %.3f bits\n", metrics->state_entropy);
    printf("  Spike Entropy:      %.3f bits\n", metrics->spike_entropy);
    printf("  Input Information:  %.2f bits/s\n", metrics->input_information);
    printf("  Output Information: %.2f bits/s\n", metrics->output_information);
    printf("  Information Gain:   %.2f bits/s\n", metrics->information_gain);
    printf("  Input Capacity:     %.2f bits/s\n", metrics->total_input_capacity);
    printf("  Output Capacity:    %.2f bits/s\n", metrics->total_output_capacity);
    printf("  Num Inputs:         %u\n", metrics->num_inputs);
    printf("  Num Outputs:        %u\n", metrics->num_outputs);
    printf("\n");
}

void shannon_print_network_metrics(
    const shannon_network_metrics_t* metrics,
    const char* label)
{
    if (!metrics) {
        return;
    }

    if (label) {
        printf("=== Shannon Network Metrics: %s ===\n", label);
    } else {
        printf("=== Shannon Network Metrics ===\n");
    }

    printf("  Total Capacity:     %.2f Kbits/s\n", metrics->total_capacity / 1000.0F);
    printf("  Total Entropy:      %.2f Kbits\n", metrics->total_entropy / 1000.0F);
    printf("  Mutual Information: %.2f Kbits\n", metrics->mutual_information / 1000.0F);
    printf("  Information Rate:   %.2f Kbits/s\n", metrics->information_rate / 1000.0F);
    printf("  Average Efficiency: %.1f%%\n", metrics->average_efficiency * 100.0F);
    printf("  Bottleneck Score:   %.3f (0=bad, 1=good)\n", metrics->bottleneck_score);
    printf("  Num Bottlenecks:    %u\n", metrics->num_bottlenecks);
    printf("  Num Neurons:        %u\n", metrics->num_neurons);
    printf("  Num Synapses:       %u\n", metrics->num_synapses);
    printf("\n");
}
