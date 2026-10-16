/**
 * @file nimcp_cross_modal.c
 * @brief Cross-modal information flow tracking implementation
 *
 * WHAT: Shannon information theory for cross-modal pathways
 * WHY:  Detect bottlenecks, optimize multi-sensory integration
 * HOW:  Channel capacity, mutual information, routing optimization
 *
 * @author NIMCP Development Team
 * @date 2025-11-14
 * @version 2.11 (Phase C4.7)
 */

#include "async/nimcp_bio_async.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "INFORMATION"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cross_modal)

#include "information/nimcp_cross_modal.h"
#include "utils/memory/nimcp_memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

/* Use project's standard logging macros (nimcp_logging.h included above) */

/* P3-CM-1/2/3: Named constants for previously magic numbers */
#define CROSS_MODAL_DEFAULT_SNR           10.0f  /**< Default SNR for capacity estimate */
#define CROSS_MODAL_SAMPLE_RATE_HZ        1000.0f /**< Assumed sample rate for info rate */
#define CROSS_MODAL_INTEGRATION_THRESHOLD 0.7f   /**< Efficiency threshold for "integrating well" */

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

/**
 * WHAT: Compute entropy from feature distribution
 * WHY:  Need entropy for channel analysis
 * HOW:  Discretize features, compute H = -Σ p(x) log₂ p(x)
 */
static float compute_feature_entropy(
    const float* features,
    uint32_t dim,
    uint32_t num_samples,
    const shannon_config_t* config)
{
    // Guard: Validate inputs
    if (!features || dim == 0 || num_samples == 0 || !config) {
        return 0.0F;
    }

    // Guard: Check minimum samples
    if (num_samples < 2) {
        return 0.0F;
    }

    // Compute mean and std for each dimension
    float entropy_sum = 0.0F;

    for (uint32_t d = 0; d < dim; d++) {
        float mean = 0.0F;
        for (uint32_t i = 0; i < num_samples; i++) {
            mean += features[i * dim + d];
        }
        mean /= (float)num_samples;

        float std = 0.0F;
        for (uint32_t i = 0; i < num_samples; i++) {
            float diff = features[i * dim + d] - mean;
            std += diff * diff;
        }
        std = sqrtf(std / (float)num_samples);

        // Entropy of Gaussian: H = 0.5 * log2(2πeσ²)
        // Guard: Ensure positive entropy by clamping variance to minimum
        if (std > 1e-6F) {
            float variance = std * std;
            // Clamp variance to ensure positive entropy
            // For entropy > 0, need 2πeσ² > 1, so σ² > 1/(2πe) ≈ 0.0585
            float min_variance = 0.1F;  // Conservative minimum for positive entropy
            if (variance < min_variance) {
                variance = min_variance;
            }
            float h = 0.5F * log2f(2.0F * M_PI * M_E * variance);
            // Ensure non-negative entropy
            if (h > 0.0F) {
                entropy_sum += h;
            }
        } else {
            // Very low variance - use fixed small entropy
            entropy_sum += 0.1F;
        }
    }

    return entropy_sum;
}

/**
 * WHAT: Compute mutual information between two feature sets
 * WHY:  Measure information transfer
 * HOW:  I(X;Y) = H(X) + H(Y) - H(X,Y)
 */
static float compute_mutual_information(
    const float* features_x,
    uint32_t dim_x,
    const float* features_y,
    uint32_t dim_y,
    uint32_t num_samples,
    float entropy_x,
    float entropy_y,
    const shannon_config_t* config)
{
    // Guard: Validate inputs
    if (!features_x || !features_y || num_samples == 0 || !config) {
        return 0.0F;
    }

    // Compute joint entropy H(X,Y)
    uint32_t joint_dim = dim_x + dim_y;
    /* P1-CM-1: Check for integer overflow in allocation */
    size_t joint_alloc_size = (size_t)num_samples * (size_t)joint_dim * sizeof(float);
    if (joint_dim < dim_x || joint_alloc_size / sizeof(float) / num_samples != joint_dim) {
        LOG_ERROR("compute_mutual_information: allocation overflow");
        return 0.0F;
    }
    float* joint_features = (float*)nimcp_malloc(joint_alloc_size);
    if (!joint_features) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_samples * joint_dim * sizeof(float),
                          "compute_mutual_information: Failed to allocate joint features");
        return 0.0F;
    }

    // Concatenate features
    for (uint32_t i = 0; i < num_samples; i++) {
        memcpy(&joint_features[i * joint_dim],
               &features_x[i * dim_x],
               dim_x * sizeof(float));
        memcpy(&joint_features[i * joint_dim + dim_x],
               &features_y[i * dim_y],
               dim_y * sizeof(float));
    }

    float joint_entropy = compute_feature_entropy(
        joint_features, joint_dim, num_samples, config
    );

    nimcp_free(joint_features);

    // I(X;Y) = H(X) + H(Y) - H(X,Y)
    float mi = entropy_x + entropy_y - joint_entropy;

    // Guard: Bound to [0, min(H(X), H(Y))]
    mi = fmaxf(0.0F, fminf(mi, fminf(entropy_x, entropy_y)));

    return mi;
}

//=============================================================================
// CROSS-MODAL CHANNEL ANALYSIS
//=============================================================================

cross_modal_channel_t cross_modal_analyze_channel(
    const char* source_modality,
    const char* dest_modality,
    const float* source_features,
    uint32_t source_dim,
    const float* dest_features,
    uint32_t dest_dim,
    uint32_t num_samples,
    const shannon_config_t* config)
{
    // WHAT: Analyze information transfer between modalities
    // WHY:  Detect bottlenecks, measure efficiency
    // HOW:  Compute H(source), H(dest), I(source;dest)

    cross_modal_channel_t channel;
    memset(&channel, 0, sizeof(channel));

    // Guard: Validate inputs
    if (!source_modality || !dest_modality || !source_features || !dest_features || !config) {
        LOG_ERROR("cross_modal_analyze_channel: NULL parameters");
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "cross_modal_analyze_channel: NULL parameters");
        return channel;
    }

    // Guard: Validate dimensions
    if (source_dim == 0 || dest_dim == 0 || num_samples == 0) {
        LOG_ERROR("cross_modal_analyze_channel: Invalid dimensions");
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM,
                    "cross_modal_analyze_channel: Invalid dimensions");
        return channel;
    }

    // Guard: Check sample limit
    if (num_samples > CROSS_MODAL_MAX_SAMPLES) {
        LOG_WARN("cross_modal_analyze_channel: Truncating to %u samples",
                         CROSS_MODAL_MAX_SAMPLES);
        num_samples = CROSS_MODAL_MAX_SAMPLES;
    }

    // Copy modality names
    strncpy(channel.source_modality, source_modality, CROSS_MODAL_MAX_MODALITY_NAME - 1);
    strncpy(channel.dest_modality, dest_modality, CROSS_MODAL_MAX_MODALITY_NAME - 1);

    // Compute source entropy
    channel.source_entropy = compute_feature_entropy(
        source_features, source_dim, num_samples, config
    );

    // Compute destination entropy
    channel.dest_entropy = compute_feature_entropy(
        dest_features, dest_dim, num_samples, config
    );

    // Compute mutual information
    channel.mutual_information = compute_mutual_information(
        source_features, source_dim,
        dest_features, dest_dim,
        num_samples,
        channel.source_entropy,
        channel.dest_entropy,
        config
    );

    // Compute transfer efficiency
    if (channel.source_entropy > 1e-6F) {
        channel.transfer_efficiency = channel.mutual_information / channel.source_entropy;
        channel.transfer_efficiency = fminf(1.0F, fmaxf(0.0F, channel.transfer_efficiency));
    }

    /* P3-CM-1/2: Use named constants for SNR and sample rate */
    // Estimate channel capacity (Shannon-Hartley)
    // C = B * log2(1 + SNR), assume bandwidth ~ feature dim
    float snr = CROSS_MODAL_DEFAULT_SNR;
    channel.channel_capacity = (float)dest_dim * log2f(1.0F + snr);

    // Information rate (bits/sec)
    channel.information_rate = channel.mutual_information * CROSS_MODAL_SAMPLE_RATE_HZ / (float)num_samples;

    // Metadata
    channel.sample_count = num_samples;
    channel.timestamp_ms = 0;  // Caller should set this

    return channel;
}

/* P2-CM-1: Parameter changed from const to non-const since we mutate the channel */
bool cross_modal_is_bottleneck(
    cross_modal_channel_t* channel,
    float efficiency_threshold)
{
    // WHAT: Check if channel is a bottleneck
    // WHY:  Identify problematic pathways
    // HOW:  Compare efficiency against threshold

    // Guard: Validate channel
    if (!channel) {
        LOG_ERROR("cross_modal_is_bottleneck: NULL channel");
        return false;
    }

    // Guard: Validate threshold
    if (efficiency_threshold < 0.0F || efficiency_threshold > 1.0F) {
        LOG_WARN("cross_modal_is_bottleneck: Invalid threshold %.2f, using 0.5",
                         efficiency_threshold);
        efficiency_threshold = CROSS_MODAL_DEFAULT_BOTTLENECK_THRESHOLD;
    }

    // Check if efficiency is below threshold
    bool is_bottleneck = (channel->transfer_efficiency < efficiency_threshold);

    // Compute severity (0 = no bottleneck, 1 = complete bottleneck)
    float severity = 0.0F;
    if (is_bottleneck) {
        severity = 1.0F - (channel->transfer_efficiency / efficiency_threshold);
        severity = fminf(1.0F, fmaxf(0.0F, severity));
    }

    /* P2-CM-1: Now that parameter is non-const, update directly */
    channel->is_bottleneck = is_bottleneck;
    channel->bottleneck_severity = severity;

    return is_bottleneck;
}

//=============================================================================
// MULTI-MODAL INTEGRATION
//=============================================================================

multi_modal_integration_t cross_modal_analyze_integration(
    const float** features,
    const uint32_t* dims,
    uint32_t num_modalities,
    uint32_t num_samples,
    const char** modality_names,
    const shannon_config_t* config)
{
    // WHAT: Analyze how well multiple modalities integrate
    // WHY:  Optimize multi-sensory perception
    // HOW:  Compute joint entropy, redundancy, synergy

    multi_modal_integration_t integration;
    memset(&integration, 0, sizeof(integration));

    // Guard: Validate inputs
    if (!features || !dims || !modality_names || !config) {
        LOG_ERROR("cross_modal_analyze_integration: NULL parameters");
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "cross_modal_analyze_integration: NULL parameters");
        return integration;
    }

    /* P2-CM-2: Use defined constant instead of magic number 4 */
    // Guard: Validate modality count
    if (num_modalities < 2 || num_modalities > CROSS_MODAL_MAX_MODALITIES) {
        LOG_ERROR("cross_modal_analyze_integration: Invalid num_modalities %u (must be 2-4)",
                       num_modalities);
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM,
                    "cross_modal_analyze_integration: Invalid num_modalities (must be 2-4)");
        return integration;
    }

    // Guard: Validate samples
    if (num_samples == 0) {
        LOG_ERROR("cross_modal_analyze_integration: num_samples is 0");
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM,
                    "cross_modal_analyze_integration: num_samples is 0");
        return integration;
    }

    integration.num_modalities = num_modalities;

    // Copy modality names
    for (uint32_t i = 0; i < num_modalities; i++) {
        strncpy(integration.modality_names[i], modality_names[i],
                CROSS_MODAL_MAX_MODALITY_NAME - 1);
    }

    // Compute individual entropies
    float sum_entropy = 0.0F;
    for (uint32_t i = 0; i < num_modalities; i++) {
        integration.individual_entropy[i] = compute_feature_entropy(
            features[i], dims[i], num_samples, config
        );
        sum_entropy += integration.individual_entropy[i];
    }

    // Compute joint entropy (concatenate all features)
    uint32_t total_dim = 0;
    for (uint32_t i = 0; i < num_modalities; i++) {
        total_dim += dims[i];
    }

    /* P1-CM-1: Check for integer overflow in allocation */
    size_t joint_alloc_size = (size_t)num_samples * (size_t)total_dim * sizeof(float);
    if (total_dim == 0 || joint_alloc_size / sizeof(float) / num_samples != total_dim) {
        LOG_ERROR("cross_modal_analyze_integration: allocation overflow");
        return integration;
    }
    float* joint_features = (float*)nimcp_malloc(joint_alloc_size);
    if (!joint_features) {
        LOG_ERROR("cross_modal_analyze_integration: malloc failed");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_samples * total_dim * sizeof(float),
                          "cross_modal_analyze_integration: Failed to allocate joint features");
        return integration;
    }

    // Concatenate all modality features
    for (uint32_t sample = 0; sample < num_samples; sample++) {
        uint32_t offset = 0;
        for (uint32_t mod = 0; mod < num_modalities; mod++) {
            memcpy(&joint_features[sample * total_dim + offset],
                   &features[mod][sample * dims[mod]],
                   dims[mod] * sizeof(float));
            offset += dims[mod];
        }
    }

    integration.joint_entropy = compute_feature_entropy(
        joint_features, total_dim, num_samples, config
    );

    nimcp_free(joint_features);

    // Compute pairwise mutual information (total)
    integration.total_mutual_info = 0.0F;
    for (uint32_t i = 0; i < num_modalities; i++) {
        for (uint32_t j = i + 1; j < num_modalities; j++) {
            float mi = compute_mutual_information(
                features[i], dims[i],
                features[j], dims[j],
                num_samples,
                integration.individual_entropy[i],
                integration.individual_entropy[j],
                config
            );
            integration.total_mutual_info += mi;
        }
    }

    // Compute redundancy (how much information is shared)
    integration.redundancy = sum_entropy - integration.joint_entropy;
    integration.redundancy = fmaxf(0.0F, integration.redundancy);

    // Integration efficiency
    if (sum_entropy > 1e-6F) {
        integration.integration_efficiency = integration.joint_entropy / sum_entropy;
        integration.integration_efficiency = fminf(1.0F, fmaxf(0.0F,
                                                    integration.integration_efficiency));
    }

    /* P3-CM-3: Use named constant for integration threshold */
    integration.is_integrating_well = (integration.integration_efficiency > CROSS_MODAL_INTEGRATION_THRESHOLD);

    return integration;
}

float cross_modal_compute_synergy(const multi_modal_integration_t* integration)
{
    // WHAT: Compute synergy between modalities
    // WHY:  Measure emergent multi-sensory properties
    // HOW:  Synergy = redundancy - total_mutual_info

    // Guard: Validate integration
    if (!integration) {
        LOG_ERROR("cross_modal_compute_synergy: NULL integration");
        return 0.0F;
    }

    // Guard: Validate modalities
    if (integration->num_modalities < 2) {
        LOG_ERROR("cross_modal_compute_synergy: Need at least 2 modalities");
        return 0.0F;
    }

    // Synergy = Redundancy - Mutual Info
    // Positive synergy: modalities create new information together
    // Negative synergy: modalities are just redundant
    float synergy = integration->redundancy - integration->total_mutual_info;

    return synergy;
}

//=============================================================================
// ROUTING GRAPH
//=============================================================================

cross_modal_routing_graph_t* cross_modal_create_routing_graph(
    const char** modality_names,
    uint32_t num_modalities)
{
    // WHAT: Create cross-modal routing graph
    // WHY:  Track global information flow
    // HOW:  Allocate adjacency matrix

    // Guard: Validate inputs
    if (!modality_names) {
        LOG_ERROR("cross_modal_create_routing_graph: NULL modality_names");
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "cross_modal_create_routing_graph: NULL modality_names");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "modality_names is NULL");

        return NULL;
    }

    // Guard: Validate modality count
    if (num_modalities == 0 || num_modalities > CROSS_MODAL_MAX_MODALITIES) {
        LOG_ERROR("cross_modal_create_routing_graph: Invalid num_modalities %u",
                       num_modalities);
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM,
                    "cross_modal_create_routing_graph: Invalid num_modalities");
        return NULL;
    }

    // Allocate graph
    cross_modal_routing_graph_t* graph = (cross_modal_routing_graph_t*)
        nimcp_calloc(1, sizeof(cross_modal_routing_graph_t));
    if (!graph) {
        LOG_ERROR("cross_modal_create_routing_graph: malloc failed");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(cross_modal_routing_graph_t),
                          "cross_modal_create_routing_graph: Failed to allocate graph");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "graph is NULL");

        return NULL;
    }

    graph->num_modalities = num_modalities;

    // Copy modality names
    for (uint32_t i = 0; i < num_modalities; i++) {
        strncpy(graph->modality_names[i], modality_names[i],
                CROSS_MODAL_MAX_MODALITY_NAME - 1);
    }

    // Initialize adjacency matrix to NULL
    for (uint32_t i = 0; i < CROSS_MODAL_MAX_MODALITIES; i++) {
        for (uint32_t j = 0; j < CROSS_MODAL_MAX_MODALITIES; j++) {
            graph->channels[i][j] = NULL;
        }
    }

    return graph;
}

bool cross_modal_update_routing_graph(
    cross_modal_routing_graph_t* graph,
    uint32_t source_id,
    uint32_t dest_id,
    const cross_modal_channel_t* channel)
{
    // WHAT: Update routing graph with channel data
    // WHY:  Keep graph current
    // HOW:  Store channel in [source][dest]

    // Guard: Validate graph
    if (!graph) {
        LOG_ERROR("cross_modal_update_routing_graph: NULL graph");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cross_modal_update_routing_graph: graph is NULL");
        return false;
    }

    // Guard: Validate channel
    if (!channel) {
        LOG_ERROR("cross_modal_update_routing_graph: NULL channel");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cross_modal_update_routing_graph: channel is NULL");
        return false;
    }

    // Guard: Validate indices
    if (source_id >= graph->num_modalities || dest_id >= graph->num_modalities) {
        LOG_ERROR("cross_modal_update_routing_graph: Invalid indices %u, %u",
                       source_id, dest_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cross_modal_update_routing_graph: capacity exceeded");
        return false;
    }

    // Allocate channel if needed
    if (!graph->channels[source_id][dest_id]) {
        graph->channels[source_id][dest_id] = (cross_modal_channel_t*)
            nimcp_malloc(sizeof(cross_modal_channel_t));
        if (!graph->channels[source_id][dest_id]) {
            LOG_ERROR("cross_modal_update_routing_graph: malloc failed");
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(cross_modal_channel_t),
                              "cross_modal_update_routing_graph: Failed to allocate channel");
            return false;
        }
    }

    // Copy channel data
    memcpy(graph->channels[source_id][dest_id], channel, sizeof(cross_modal_channel_t));

    return true;
}

bool cross_modal_detect_bottlenecks(
    const cross_modal_routing_graph_t* graph,
    float efficiency_threshold,
    cross_modal_channel_t* bottlenecks,
    uint32_t max_bottlenecks,
    uint32_t* num_bottlenecks)
{
    // WHAT: Detect all bottlenecks in graph
    // WHY:  Identify global problems
    // HOW:  Scan adjacency matrix

    // Guard: Validate inputs
    if (!graph || !bottlenecks || !num_bottlenecks) {
        LOG_ERROR("cross_modal_detect_bottlenecks: NULL parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cross_modal_detect_bottlenecks: required parameter is NULL (graph, bottlenecks, num_bottlenecks)");
        return false;
    }

    // Guard: Validate threshold
    if (efficiency_threshold < 0.0F || efficiency_threshold > 1.0F) {
        efficiency_threshold = CROSS_MODAL_DEFAULT_BOTTLENECK_THRESHOLD;
    }

    *num_bottlenecks = 0;

    // Scan all channels
    for (uint32_t i = 0; i < graph->num_modalities; i++) {
        for (uint32_t j = 0; j < graph->num_modalities; j++) {
            if (i == j) continue;  // Skip self-loops

            cross_modal_channel_t* channel = graph->channels[i][j];
            if (!channel) continue;  // Skip missing channels

            if (cross_modal_is_bottleneck(channel, efficiency_threshold)) {
                if (*num_bottlenecks < max_bottlenecks) {
                    memcpy(&bottlenecks[*num_bottlenecks], channel,
                           sizeof(cross_modal_channel_t));
                    (*num_bottlenecks)++;
                }
            }
        }
    }

    return true;
}

float cross_modal_find_optimal_route(
    const cross_modal_routing_graph_t* graph,
    uint32_t source_id,
    uint32_t dest_id,
    uint32_t* path,
    uint32_t max_path_length,
    uint32_t* path_length)
{
    // WHAT: Find optimal route using Dijkstra
    // WHY:  Route around bottlenecks
    // HOW:  Maximize capacity along path

    // Guard: Validate inputs
    if (!graph || !path || !path_length) {
        LOG_ERROR("cross_modal_find_optimal_route: NULL parameters");
        return 0.0F;
    }

    // Guard: Validate indices
    if (source_id >= graph->num_modalities || dest_id >= graph->num_modalities) {
        LOG_ERROR("cross_modal_find_optimal_route: Invalid indices");
        return 0.0F;
    }

    // Guard: Validate path length
    if (max_path_length < 2) {
        LOG_ERROR("cross_modal_find_optimal_route: max_path_length too small");
        return 0.0F;
    }

    // Simple implementation: Direct path only (for now)
    // TODO: Implement full Dijkstra if needed

    *path_length = 0;

    // Check if direct path exists
    cross_modal_channel_t* direct_channel = graph->channels[source_id][dest_id];
    if (direct_channel) {
        path[0] = source_id;
        path[1] = dest_id;
        *path_length = 2;
        return direct_channel->channel_capacity;
    }

    // No path found
    return 0.0F;
}

void cross_modal_destroy_routing_graph(cross_modal_routing_graph_t* graph)
{
    // WHAT: Destroy routing graph
    // WHY:  Free resources
    // HOW:  Free channels and graph

    // Guard: Validate graph
    if (!graph) {
        return;
    }

    // Free all channels
    for (uint32_t i = 0; i < graph->num_modalities; i++) {
        for (uint32_t j = 0; j < graph->num_modalities; j++) {
            if (graph->channels[i][j]) {
                nimcp_free(graph->channels[i][j]);
                graph->channels[i][j] = NULL;
            }
        }
    }

    // Free graph
    nimcp_free(graph);
}

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

shannon_config_t cross_modal_default_config(void)
{
    // WHAT: Get default cross-modal configuration
    // WHY:  Provide sensible defaults
    // HOW:  Return Shannon config with reasonable values

    return shannon_default_config();
}
