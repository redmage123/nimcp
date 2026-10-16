/**
 * @file nimcp_cross_modal.h
 * @brief Cross-modal information flow tracking and optimization
 *
 * WHAT: Track Shannon information flow between sensory modalities
 * WHY:  Detect bottlenecks, optimize multi-sensory integration
 * HOW:  Channel capacity analysis, mutual information, routing optimization
 *
 * BIOLOGICAL BASIS:
 * - Superior Colliculus: Visual + auditory integration for orienting
 * - Superior Temporal Sulcus (STS): Audiovisual speech integration
 * - Posterior Parietal Cortex: Visuospatial + motor integration
 * - Prefrontal Cortex: All modalities → decision making
 *
 * CLINICAL EXAMPLES:
 * - McGurk Effect: Visual /ga/ + Audio /ba/ → Perceived /da/
 * - Audiovisual Speech: Seeing lips improves speech comprehension 30%
 * - Synesthesia: Cross-modal leakage (see sounds, hear colors)
 * - Autism: Reduced cross-modal integration → impaired social communication
 *
 * REFERENCES:
 * - Stein & Stanford (2008) "Multisensory integration: current issues"
 * - Calvert & Thesen (2004) "Multisensory integration: methodological approaches"
 * - Driver & Noesselt (2008) "Multisensory interplay reveals crossmodal influences"
 *
 * @author NIMCP Development Team
 * @date 2025-11-14
 * @version 2.11 (Phase C4.7)
 */

#ifndef NIMCP_CROSS_MODAL_H
#define NIMCP_CROSS_MODAL_H

#include <stdint.h>
#include <stdbool.h>
#include "information/nimcp_shannon.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// CONSTANTS
//=============================================================================

#define CROSS_MODAL_MAX_MODALITIES 10
#define CROSS_MODAL_MAX_MODALITY_NAME 32
#define CROSS_MODAL_MAX_SAMPLES 1000
#define CROSS_MODAL_DEFAULT_BOTTLENECK_THRESHOLD 0.5f  // 50% efficiency

//=============================================================================
// CROSS-MODAL CHANNEL
//=============================================================================

/**
 * @brief Cross-modal information flow channel
 *
 * WHAT: Track information flow from one modality to another
 * WHY:  Detect bottlenecks, measure transfer efficiency
 * HOW:  Compare source/destination entropy, compute mutual information
 *
 * BIOLOGY: Models pathways like V1 → STS or A1 → Wernicke's area
 */
typedef struct {
    char source_modality[CROSS_MODAL_MAX_MODALITY_NAME];  ///< Source cortex
    char dest_modality[CROSS_MODAL_MAX_MODALITY_NAME];    ///< Dest cortex

    // Shannon metrics
    float source_entropy;             ///< H(source) in bits
    float dest_entropy;               ///< H(dest) in bits
    float mutual_information;         ///< I(source;dest) in bits
    float transfer_efficiency;        ///< I(source;dest) / H(source)
    float channel_capacity;           ///< Maximum bits/sec
    float information_rate;           ///< Actual bits/sec

    // Bottleneck detection
    bool is_bottleneck;               ///< True if efficiency < threshold
    float bottleneck_severity;        ///< 0-1, higher = more severe

    // Timing
    uint64_t timestamp_ms;            ///< When metrics were computed
    uint32_t sample_count;            ///< Number of samples analyzed
} cross_modal_channel_t;

/**
 * WHAT: Analyze cross-modal information transfer
 * WHY:  Detect bottlenecks between cortices
 * HOW:  Sample features from both, compute H(source), H(dest), I(source;dest)
 *
 * @param source_modality Source name (e.g., "visual")
 * @param dest_modality Dest name (e.g., "audio")
 * @param source_features Source feature vectors [num_samples × source_dim]
 * @param source_dim Source feature dimensionality
 * @param dest_features Dest feature vectors [num_samples × dest_dim]
 * @param dest_dim Dest feature dimensionality
 * @param num_samples Number of samples to analyze
 * @param config Shannon configuration
 * @return Cross-modal channel metrics
 *
 * COMPLEXITY: O(N × D) where N=samples, D=max(source_dim, dest_dim)
 */
cross_modal_channel_t cross_modal_analyze_channel(
    const char* source_modality,
    const char* dest_modality,
    const float* source_features,
    uint32_t source_dim,
    const float* dest_features,
    uint32_t dest_dim,
    uint32_t num_samples,
    const shannon_config_t* config
);

/**
 * WHAT: Check if channel is a bottleneck
 * WHY:  Identify problematic cross-modal pathways
 * HOW:  Compare efficiency against threshold
 *
 * @param channel Cross-modal channel
 * @param efficiency_threshold Minimum acceptable efficiency (default: 0.5)
 * @return true if bottleneck detected
 */
bool cross_modal_is_bottleneck(
    cross_modal_channel_t* channel,
    float efficiency_threshold
);

//=============================================================================
// MULTI-MODAL INTEGRATION
//=============================================================================

/**
 * @brief Multi-modal integration metrics
 *
 * WHAT: Measure how well multiple modalities integrate
 * WHY:  Optimize multi-sensory perception (McGurk effect, etc.)
 * HOW:  Compute joint entropy, redundancy, synergy
 *
 * BIOLOGY: Models STS integration of visual + auditory speech
 */
typedef struct {
    uint32_t num_modalities;          ///< Number of input modalities (2-4)
    char modality_names[4][CROSS_MODAL_MAX_MODALITY_NAME];  ///< Modality names

    // Individual entropies
    float individual_entropy[4];      ///< H(M_i) for each modality

    // Joint metrics
    float joint_entropy;              ///< H(M1, M2, ..., Mn)
    float total_mutual_info;          ///< Sum of pairwise I(M_i;M_j)
    float redundancy;                 ///< Overlap between modalities
    float synergy;                    ///< Information only in combination

    // Integration efficiency
    float integration_efficiency;     ///< joint / sum(individual)
    bool is_integrating_well;         ///< True if efficiency > threshold
} multi_modal_integration_t;

/**
 * WHAT: Analyze multi-modal integration
 * WHY:  Measure how well modalities combine
 * HOW:  Compute joint entropy H(M1,M2,...), redundancy, synergy
 *
 * @param features Array of feature pointers [num_modalities]
 * @param dims Array of feature dimensions [num_modalities]
 * @param num_modalities Number of modalities (2-4)
 * @param num_samples Number of samples per modality
 * @param modality_names Names of modalities
 * @param config Shannon configuration
 * @return Multi-modal integration metrics
 *
 * COMPLEXITY: O(2^M × N × D) where M=modalities, N=samples, D=dimensions
 * NOTE: Exponential in num_modalities, limit to 2-4
 */
multi_modal_integration_t cross_modal_analyze_integration(
    const float** features,
    const uint32_t* dims,
    uint32_t num_modalities,
    uint32_t num_samples,
    const char** modality_names,
    const shannon_config_t* config
);

/**
 * WHAT: Compute synergy between modalities
 * WHY:  Understand emergent multi-sensory properties
 * HOW:  Synergy = I(M1,M2;Target) - I(M1;Target) - I(M2;Target)
 *
 * @param integration Multi-modal integration metrics
 * @return Synergy in bits (positive = emergence, negative = redundancy)
 *
 * BIOLOGY: Synergy explains McGurk effect (visual + audio > sum of parts)
 */
float cross_modal_compute_synergy(const multi_modal_integration_t* integration);

//=============================================================================
// ROUTING GRAPH
//=============================================================================

/**
 * @brief Cross-modal routing graph
 *
 * WHAT: Track all cross-modal pathways in brain
 * WHY:  Find optimal routes, detect global bottlenecks
 * HOW:  Graph of modalities with Shannon metrics on edges
 *
 * BIOLOGY: Models dorsal (where) and ventral (what) streams
 */
typedef struct {
    uint32_t num_modalities;          ///< Number of cortices
    char modality_names[CROSS_MODAL_MAX_MODALITIES][CROSS_MODAL_MAX_MODALITY_NAME];

    // Adjacency matrix of channels [source][dest]
    cross_modal_channel_t* channels[CROSS_MODAL_MAX_MODALITIES][CROSS_MODAL_MAX_MODALITIES];

    // Global metrics
    float total_capacity;             ///< Sum of all channel capacities
    float total_information_rate;     ///< Actual info flow rate
    float network_efficiency;         ///< rate / capacity
    uint32_t num_bottlenecks;         ///< Count of bottleneck channels

    // Optimization guidance
    uint32_t bottleneck_sources[CROSS_MODAL_MAX_MODALITIES];
    uint32_t bottleneck_dests[CROSS_MODAL_MAX_MODALITIES];
} cross_modal_routing_graph_t;

/**
 * WHAT: Create cross-modal routing graph
 * WHY:  Track global information flow across brain
 * HOW:  Allocate adjacency matrix, initialize metrics
 *
 * @param modality_names Names of cortices
 * @param num_modalities Number of cortices
 * @return Routing graph or NULL on failure
 *
 * COMPLEXITY: O(V²) where V = num_modalities
 */
cross_modal_routing_graph_t* cross_modal_create_routing_graph(
    const char** modality_names,
    uint32_t num_modalities
);

/**
 * WHAT: Update routing graph with new channel data
 * WHY:  Keep routing graph current
 * HOW:  Store channel in adjacency matrix [source][dest]
 *
 * @param graph Routing graph
 * @param source_id Source modality index
 * @param dest_id Destination modality index
 * @param channel Channel metrics
 * @return true on success
 */
bool cross_modal_update_routing_graph(
    cross_modal_routing_graph_t* graph,
    uint32_t source_id,
    uint32_t dest_id,
    const cross_modal_channel_t* channel
);

/**
 * WHAT: Detect all bottlenecks in routing graph
 * WHY:  Identify global cross-modal problems
 * HOW:  Scan adjacency matrix, collect bottlenecks
 *
 * @param graph Routing graph
 * @param efficiency_threshold Minimum acceptable efficiency
 * @param bottlenecks Output array of bottleneck channels
 * @param max_bottlenecks Maximum bottlenecks to return
 * @param num_bottlenecks Number of bottlenecks found
 * @return true on success
 *
 * COMPLEXITY: O(V²) where V = num_modalities
 */
bool cross_modal_detect_bottlenecks(
    const cross_modal_routing_graph_t* graph,
    float efficiency_threshold,
    cross_modal_channel_t* bottlenecks,
    uint32_t max_bottlenecks,
    uint32_t* num_bottlenecks
);

/**
 * WHAT: Find optimal route between modalities
 * WHY:  Route information around bottlenecks
 * HOW:  Dijkstra's algorithm with capacity as weight
 *
 * @param graph Routing graph
 * @param source_id Source modality
 * @param dest_id Destination modality
 * @param path Output path (array of modality IDs)
 * @param max_path_length Maximum path length
 * @param path_length Actual path length
 * @return Total capacity of path, or 0 if no path
 *
 * COMPLEXITY: O(V² log V) where V = num_modalities
 */
float cross_modal_find_optimal_route(
    const cross_modal_routing_graph_t* graph,
    uint32_t source_id,
    uint32_t dest_id,
    uint32_t* path,
    uint32_t max_path_length,
    uint32_t* path_length
);

/**
 * WHAT: Destroy routing graph
 * WHY:  Free allocated resources
 * HOW:  Free channels and graph structure
 *
 * @param graph Routing graph to destroy
 */
void cross_modal_destroy_routing_graph(cross_modal_routing_graph_t* graph);

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/**
 * WHAT: Get default cross-modal configuration
 * WHY:  Provide sensible defaults
 * HOW:  Return config with reasonable thresholds
 *
 * @return Default configuration
 */
shannon_config_t cross_modal_default_config(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_CROSS_MODAL_H
