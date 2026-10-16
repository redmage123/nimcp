/**
 * @file nimcp_columnar_connectivity.h
 * @brief Cortical columnar connectivity with canonical microcircuit implementation
 *
 * WHAT: Biologically-realistic connectivity patterns for cortical columns
 * WHY:  Cortical columns are fundamental processing units with stereotypical
 *       connectivity patterns (Douglas & Martin canonical microcircuit)
 * HOW:  Distance-dependent, layer-specific, feature-similarity connectivity
 *       with patchy horizontal connections and hierarchical pathways
 *
 * DESIGN PHILOSOPHY:
 * - Canonical microcircuit: Douglas & Martin (1991) circuit motif
 * - Layer-specific: Different layers have distinct connectivity patterns
 * - Distance-dependent: P(d) = P₀ × exp(-d/λ) for lateral connections
 * - Feature similarity: Columns with similar features connect preferentially
 * - Patchy connectivity: Clustered connections at specific distances
 * - Small-world: High clustering, short path lengths (σ > 1)
 *
 * CONNECTIVITY TYPES:
 * 1. Intracolumnar (Vertical): Dense connections within minicolumn
 *    - Layer IV → II/III: Feed sensory info to association layers
 *    - Layer II/III → V: Association to output
 *    - Layer V → VI: Output to feedback
 *
 * 2. Intercolumnar (Horizontal): Sparse patchy connections
 *    - Distance-dependent decay
 *    - Feature-similarity modulation
 *    - Patchy clusters at ~0.5mm, ~1.5mm, ~3mm
 *
 * 3. Long-range (Association): Cortico-cortical pathways
 *    - Feedforward: Layer II/III → Layer IV
 *    - Feedback: Layer VI → Layer I
 *
 * MATHEMATICAL MODELS:
 * - Connection probability: P(d, θ) = P₀ × exp(-d/λ) × S(θ₁, θ₂)
 * - Feature similarity: S(θ₁, θ₂) = 0.5 × (1 + cos(2(θ₁ - θ₂)))
 * - Patchy connectivity: P_patch(d) = Σ_k α_k × G(d - d_k, σ_k)
 * - Small-world metrics: σ = (C/C_rand) / (L/L_rand)
 * - Conduction delay: delay(d) = d/v + τ_syn
 *
 * REFERENCES:
 * - Douglas & Martin (1991) "A functional microcircuit for cat visual cortex"
 * - Binzegger et al. (2004) "A quantitative map of the circuit of cat primary visual cortex"
 * - Bosking et al. (1997) "Orientation selectivity and the arrangement of horizontal connections"
 * - Watts & Strogatz (1998) "Collective dynamics of small-world networks"
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_COLUMNAR_CONNECTIVITY_H
#define NIMCP_COLUMNAR_CONNECTIVITY_H

#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/validation/nimcp_common.h"
#include "core/cortical_columns/nimcp_cortical_layers.h"  // For cc_cortical_layer_t and laminar_structure_t
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// CONNECTIVITY TYPES AND ENUMERATIONS
//=============================================================================

/**
 * WHAT: Types of cortical connectivity
 * WHY:  Different connection types have different properties and rules
 */
typedef enum {
    CONNECTIVITY_INTRACOLUMNAR = 0,  /**< Within column (vertical) - dense */
    CONNECTIVITY_INTERCOLUMNAR,      /**< Between nearby columns (horizontal) - sparse/patchy */
    CONNECTIVITY_LONG_RANGE,         /**< Between distant areas - sparse */
    CONNECTIVITY_FEEDBACK,           /**< Higher to lower areas (top-down) */
    CONNECTIVITY_FEEDFORWARD,        /**< Lower to higher areas (bottom-up) */
    CONNECTIVITY_TYPE_COUNT
} connectivity_type_t;

// Note: laminar_structure_t is now defined in nimcp_cortical_layers.h
// with CC_LAYER_COUNT layers (5-layer condensed model)

//=============================================================================
// CONNECTION STRUCTURES
//=============================================================================

/**
 * WHAT: Individual connection between columns
 * WHY:  Store all properties needed for signal propagation and plasticity
 * HOW:  Lightweight struct (32 bytes) for efficient storage
 */
typedef struct {
    uint32_t source_column_id;       /**< Source column ID */
    uint32_t target_column_id;       /**< Target column ID */
    cc_cortical_layer_t source_layer;   /**< Source layer */
    cc_cortical_layer_t target_layer;   /**< Target layer */
    float weight;                    /**< Synaptic weight (0-1) */
    float delay_ms;                  /**< Conduction delay in milliseconds */
    connectivity_type_t type;        /**< Connection type */
    uint8_t _padding[3];             /**< Padding for alignment */
} columnar_connection_t;

/**
 * WHAT: Rule for generating connections
 * WHY:  Specify biological connectivity patterns declaratively
 * HOW:  Parameters for mathematical models (exponential decay, feature similarity)
 */
typedef struct {
    connectivity_type_t type;           /**< Type of connectivity rule */
    float base_probability;             /**< Base connection probability P₀ (0-1) */
    float distance_decay_lambda;        /**< Length constant λ (mm) for exp decay */
    float feature_similarity_weight;    /**< Weight for feature similarity (0-1) */
    bool layer_specific;                /**< Apply to specific layers only */
    cc_cortical_layer_t source_layer;      /**< Source layer (if layer_specific) */
    cc_cortical_layer_t target_layer;      /**< Target layer (if layer_specific) */
    float min_delay_ms;                 /**< Minimum conduction delay */
    float conduction_velocity_m_s;      /**< Axonal conduction velocity (m/s) */
} connectivity_rule_t;

/**
 * WHAT: Statistics about connectivity
 * WHY:  Monitor connectivity patterns and verify biological realism
 */
typedef struct {
    uint32_t total_connections;          /**< Total number of connections */
    uint32_t intracolumnar_count;        /**< Vertical connections */
    uint32_t intercolumnar_count;        /**< Horizontal connections */
    uint32_t long_range_count;           /**< Long-range connections */
    float avg_weight;                    /**< Average synaptic weight */
    float avg_delay_ms;                  /**< Average conduction delay */
    float clustering_coefficient;        /**< C (for small-world metric) */
    float characteristic_path_length;    /**< L (for small-world metric) */
    float small_world_sigma;             /**< σ = (C/C_rand)/(L/L_rand) */
    uint32_t layer_connection_counts[CC_LAYER_COUNT][CC_LAYER_COUNT]; /**< [src][tgt] */
} connectivity_stats_t;

//=============================================================================
// OPAQUE HANDLE
//=============================================================================

/**
 * WHAT: Opaque handle to columnar connectivity manager
 * WHY:  Encapsulation - hide implementation details
 */
typedef struct columnar_connectivity columnar_connectivity_t;

//=============================================================================
// LIFECYCLE MANAGEMENT
//=============================================================================

/**
 * WHAT: Create columnar connectivity manager
 * WHY:  Initialize data structures for storing and managing connections
 * HOW:  Allocates connection pool, hash tables, and mutex
 *
 * @param max_connections Maximum number of connections to support
 * @return Connectivity manager handle or NULL on error
 *
 * COMPLEXITY: O(max_connections) time, space
 * THREAD-SAFE: Yes (each instance is independent)
 */
columnar_connectivity_t* columnar_connectivity_create(uint32_t max_connections);

/**
 * WHAT: Destroy columnar connectivity manager
 * WHY:  Free all allocated memory and cleanup resources
 * HOW:  Frees connection pool, hash tables, destroys mutex
 *
 * @param conn Connectivity manager (NULL-safe)
 *
 * COMPLEXITY: O(max_connections) time
 * THREAD-SAFE: No (caller must ensure no concurrent access)
 */
void columnar_connectivity_destroy(columnar_connectivity_t* conn);

//=============================================================================
// CONNECTIVITY RULES
//=============================================================================

/**
 * WHAT: Add connectivity rule to the manager
 * WHY:  Define how connections should be generated
 * HOW:  Stores rule in internal array for later application
 *
 * @param conn Connectivity manager
 * @param rule Connectivity rule to add
 * @return NIMCP_SUCCESS on success, error code otherwise
 *
 * COMPLEXITY: O(1) time
 * THREAD-SAFE: Yes (acquires lock)
 */
nimcp_result_t connectivity_add_rule(columnar_connectivity_t* conn,
                                     const connectivity_rule_t* rule);

/**
 * WHAT: Apply canonical microcircuit connectivity rules
 * WHY:  Use biologically-realistic default connectivity (Douglas & Martin)
 * HOW:  Adds standard intracolumnar and intercolumnar rules
 *
 * CANONICAL RULES (Binzegger et al. 2004):
 * - Layer IV → II/III: P = 0.3, λ = 0.2mm
 * - Layer II/III → V: P = 0.2, λ = 0.3mm
 * - Layer V → VI: P = 0.15, λ = 0.25mm
 * - Horizontal (same layer): P = 0.1, λ = 1.5mm (patchy)
 *
 * @param conn Connectivity manager
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(1) time
 * THREAD-SAFE: Yes (calls connectivity_add_rule)
 */
nimcp_result_t connectivity_apply_canonical_rules(columnar_connectivity_t* conn);

//=============================================================================
// CONNECTION GENERATION
//=============================================================================

/**
 * WHAT: Generate intracolumnar (vertical) connections
 * WHY:  Connect neurons within a single minicolumn across layers
 * HOW:  Apply layer-to-layer connectivity rules (IV→II/III, II/III→V, V→VI)
 *
 * @param conn Connectivity manager
 * @param column_id Column ID to wire internally
 * @param layers Laminar structure with neuron IDs per layer
 * @return Number of connections created
 *
 * COMPLEXITY: O(N²) where N = neurons per layer
 * THREAD-SAFE: Yes (acquires lock)
 */
uint32_t connectivity_generate_intracolumnar(
    columnar_connectivity_t* conn,
    uint32_t column_id,
    const laminar_structure_t* layers);

/**
 * WHAT: Generate intercolumnar (horizontal) connections between nearby columns
 * WHY:  Create lateral connections for contextual modulation
 * HOW:  Distance-dependent probability with patchy clusters
 *
 * ALGORITHM:
 * For each column pair (i, j):
 *   distance = sqrt((x_i - x_j)² + (y_i - y_j)²)
 *   P(connect) = P₀ × exp(-distance/λ) × S(feature_i, feature_j)
 *   if rand() < P(connect): create connection
 *
 * @param conn Connectivity manager
 * @param column_ids Array of column IDs
 * @param num_columns Number of columns
 * @param positions Spatial positions [x0, y0, x1, y1, ...] or NULL for grid
 * @param dims Number of spatial dimensions (2 or 3)
 * @return Number of connections created
 *
 * COMPLEXITY: O(N²) where N = num_columns
 * THREAD-SAFE: Yes (acquires lock)
 */
uint32_t connectivity_generate_intercolumnar(
    columnar_connectivity_t* conn,
    const uint32_t* column_ids,
    uint32_t num_columns,
    const float* positions,
    uint32_t dims);

/**
 * WHAT: Generate long-range connections between distant column groups
 * WHY:  Connect different cortical areas (feedforward/feedback pathways)
 * HOW:  Create sparse connections with layer-specific targeting
 *
 * TARGETING:
 * - Feedforward: Layer II/III (source) → Layer IV (target)
 * - Feedback: Layer VI (source) → Layer I (target)
 *
 * @param conn Connectivity manager
 * @param source_columns Array of source column IDs
 * @param num_sources Number of source columns
 * @param target_columns Array of target column IDs
 * @param num_targets Number of target columns
 * @return Number of connections created
 *
 * COMPLEXITY: O(N × M) where N = num_sources, M = num_targets
 * THREAD-SAFE: Yes (acquires lock)
 */
uint32_t connectivity_generate_long_range(
    columnar_connectivity_t* conn,
    const uint32_t* source_columns,
    uint32_t num_sources,
    const uint32_t* target_columns,
    uint32_t num_targets);

//=============================================================================
// CONNECTION ACCESS
//=============================================================================

/**
 * WHAT: Get all outgoing connections from a column
 * WHY:  Needed for forward propagation of activity
 * HOW:  Hash table lookup by source column ID
 *
 * @param conn Connectivity manager
 * @param column_id Source column ID
 * @param out_connections Output buffer (pre-allocated)
 * @param max_connections Buffer capacity
 * @return Number of connections returned (may be less than actual if buffer too small)
 *
 * COMPLEXITY: O(K) where K = connections from this column
 * THREAD-SAFE: Yes (acquires read lock)
 */
uint32_t connectivity_get_connections_from(
    columnar_connectivity_t* conn,
    uint32_t column_id,
    columnar_connection_t* out_connections,
    uint32_t max_connections);

/**
 * WHAT: Get all incoming connections to a column
 * WHY:  Needed for computing total input and backpropagation
 * HOW:  Hash table lookup by target column ID
 *
 * @param conn Connectivity manager
 * @param column_id Target column ID
 * @param out_connections Output buffer (pre-allocated)
 * @param max_connections Buffer capacity
 * @return Number of connections returned
 *
 * COMPLEXITY: O(K) where K = connections to this column
 * THREAD-SAFE: Yes (acquires read lock)
 */
uint32_t connectivity_get_connections_to(
    columnar_connectivity_t* conn,
    uint32_t column_id,
    columnar_connection_t* out_connections,
    uint32_t max_connections);

//=============================================================================
// SIGNAL PROPAGATION
//=============================================================================

/**
 * WHAT: Propagate activity through connections (instantaneous)
 * WHY:  Compute weighted sum of inputs to each column
 * HOW:  For each connection: target_inputs[j] += weight × source_activations[i]
 *
 * @param conn Connectivity manager
 * @param source_activations Activity level per column [num_columns]
 * @param target_inputs Output: summed inputs per column [num_columns]
 * @param num_columns Number of columns
 *
 * COMPLEXITY: O(E) where E = total connections
 * THREAD-SAFE: No (caller must synchronize if multi-threaded)
 */
void connectivity_propagate(
    columnar_connectivity_t* conn,
    const float* source_activations,
    float* target_inputs,
    uint32_t num_columns);

/**
 * WHAT: Propagate activity with axonal conduction delays
 * WHY:  Biologically realistic timing (delays affect synchronization)
 * HOW:  Use delay line buffer to store spikes, deliver after delay period
 *
 * NOTE: Requires delay buffer to be pre-allocated and managed by caller
 *
 * @param conn Connectivity manager
 * @param source_activations Activity level per column
 * @param target_inputs Output: summed inputs per column
 * @param num_columns Number of columns
 * @param dt_ms Time step in milliseconds
 *
 * COMPLEXITY: O(E) where E = total connections
 * THREAD-SAFE: No (caller must synchronize)
 */
void connectivity_propagate_with_delay(
    columnar_connectivity_t* conn,
    const float* source_activations,
    float* target_inputs,
    uint32_t num_columns,
    float dt_ms);

//=============================================================================
// PLASTICITY
//=============================================================================

/**
 * WHAT: Apply Hebbian learning rule to connections
 * WHY:  "Neurons that fire together, wire together"
 * HOW:  Δw = η × pre × post (for correlations)
 *
 * ALGORITHM:
 * For each connection (i → j):
 *   Δw_ij = learning_rate × pre_activations[i] × post_activations[j]
 *   w_ij = clip(w_ij + Δw_ij, 0, 1)
 *
 * @param conn Connectivity manager
 * @param pre_activations Pre-synaptic activations [num_columns]
 * @param post_activations Post-synaptic activations [num_columns]
 * @param learning_rate Learning rate η (typically 0.001-0.01)
 *
 * COMPLEXITY: O(E) where E = total connections
 * THREAD-SAFE: No (modifies weights)
 */
void connectivity_apply_hebbian(
    columnar_connectivity_t* conn,
    const float* pre_activations,
    const float* post_activations,
    float learning_rate);

/**
 * WHAT: Apply Spike-Timing-Dependent Plasticity (STDP)
 * WHY:  Timing-based learning: pre-before-post → LTP, post-before-pre → LTD
 * HOW:  Δw = A_+ × exp(-Δt/τ_+) if Δt > 0, else A_- × exp(Δt/τ_-)
 *
 * STDP WINDOW:
 * - LTP: Δt ∈ [0, 20ms], A_+ = 0.01, τ_+ = 20ms
 * - LTD: Δt ∈ [-20ms, 0], A_- = -0.01, τ_- = 20ms
 *
 * @param conn Connectivity manager
 * @param pre_spike_times Last spike time per column (microseconds)
 * @param post_spike_times Last spike time per column (microseconds)
 * @param num_columns Number of columns
 *
 * COMPLEXITY: O(E) where E = total connections
 * THREAD-SAFE: No (modifies weights)
 */
void connectivity_apply_stdp(
    columnar_connectivity_t* conn,
    const uint64_t* pre_spike_times,
    const uint64_t* post_spike_times,
    uint32_t num_columns);

//=============================================================================
// TOPOLOGY ANALYSIS
//=============================================================================

/**
 * WHAT: Compute clustering coefficient of connectivity graph
 * WHY:  Measure local connectivity density (for small-world analysis)
 * HOW:  C = (# triangles) / (# possible triangles)
 *
 * For each node i:
 *   neighbors = {j : connected(i,j)}
 *   triangles = count {j,k : j,k ∈ neighbors AND connected(j,k)}
 *   C_i = triangles / (|neighbors| × (|neighbors| - 1) / 2)
 * C = average(C_i)
 *
 * @param conn Connectivity manager
 * @return Clustering coefficient (0-1), or -1.0 on error
 *
 * COMPLEXITY: O(N × k²) where N = columns, k = avg degree
 * THREAD-SAFE: Yes (read-only)
 */
float connectivity_compute_clustering(columnar_connectivity_t* conn);

/**
 * WHAT: Compute characteristic path length
 * WHY:  Measure global connectivity efficiency (for small-world analysis)
 * HOW:  Average shortest path length between all column pairs
 *
 * Uses Dijkstra's algorithm for all pairs (or Floyd-Warshall if dense)
 *
 * @param conn Connectivity manager
 * @return Characteristic path length L, or -1.0 on error
 *
 * COMPLEXITY: O(N³) for Floyd-Warshall, O(N² log N) for Dijkstra
 * THREAD-SAFE: Yes (read-only)
 */
float connectivity_compute_path_length(columnar_connectivity_t* conn);

/**
 * WHAT: Check if connectivity exhibits small-world properties
 * WHY:  Small-world networks combine local clustering with short paths
 * HOW:  Compute σ = (C/C_rand) / (L/L_rand); small-world if σ > 1
 *
 * CRITERIA:
 * - High clustering: C >> C_rand
 * - Short paths: L ≈ L_rand
 * - Small-world: σ = (C/C_rand) / (L/L_rand) > 1
 *
 * @param conn Connectivity manager
 * @return true if small-world (σ > 1), false otherwise
 *
 * COMPLEXITY: O(N³) - calls compute_clustering and compute_path_length
 * THREAD-SAFE: Yes (read-only)
 */
bool connectivity_is_small_world(columnar_connectivity_t* conn);

//=============================================================================
// STATISTICS
//=============================================================================

/**
 * WHAT: Get connectivity statistics
 * WHY:  Monitor connectivity patterns, verify biological realism
 * HOW:  Aggregate connection counts, weights, delays by type/layer
 *
 * @param conn Connectivity manager
 * @param stats Output statistics structure
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(E) where E = total connections
 * THREAD-SAFE: Yes (acquires read lock)
 */
nimcp_result_t connectivity_get_stats(
    columnar_connectivity_t* conn,
    connectivity_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_COLUMNAR_CONNECTIVITY_H
