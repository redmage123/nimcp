/**
 * @file nimcp_lnn_wiring.h
 * @brief Sparse wiring patterns for Liquid Neural Networks
 *
 * WHAT: Defines connectivity patterns for LNN layers using CSR sparse format
 * WHY:  Sparse wiring reduces parameters and improves generalization in LNNs
 * HOW:  Implements various graph algorithms (Erdos-Renyi, Watts-Strogatz,
 *       Barabasi-Albert) and NCP-specific connectivity
 *
 * BIOLOGICAL GROUNDING:
 * - Biological neural networks are sparse (~1-3% connectivity)
 * - Small-world properties enable efficient information transfer
 * - Scale-free networks exhibit robust dynamics
 * - NCP (Neural Circuit Policy) mimics C. elegans motor circuits
 *
 * PATTERN OVERVIEW:
 * - Full: Dense all-to-all connectivity (baseline)
 * - Random: Erdos-Renyi random graph with target sparsity
 * - Small-World: Watts-Strogatz with ring lattice + rewiring
 * - Scale-Free: Barabasi-Albert preferential attachment
 * - NCP: Sensory → Inter ↔ Inter → Command → Motor hierarchy
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#ifndef NIMCP_LNN_WIRING_H
#define NIMCP_LNN_WIRING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "lnn/nimcp_lnn_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Types lnn_wiring_t and lnn_wiring_type_t are defined in nimcp_lnn_types.h */

/*=============================================================================
 * Wiring Creation Functions
 *===========================================================================*/

/**
 * @brief Create full (dense) wiring
 *
 * WHAT: All-to-all connectivity between neurons
 * WHY:  Baseline for comparison, maximum capacity
 * HOW:  Creates n_neurons * n_neurons edges in CSR format
 *
 * COMPLEXITY: O(n^2) space and time
 * SPARSITY: 0.0 (fully connected)
 *
 * @param n_neurons Number of neurons
 * @return Wiring structure or NULL on failure
 */
lnn_wiring_t* lnn_wiring_create_full(uint32_t n_neurons);

/**
 * @brief Create random sparse wiring (Erdos-Renyi)
 *
 * WHAT: Each potential edge exists with probability (1 - sparsity)
 * WHY:  Random graphs serve as null model for network analysis
 * HOW:  Iterate all pairs, add edge with probability p = 1 - sparsity
 *
 * COMPLEXITY: O(n^2) time, O(n * p * n) space
 * EDGE COUNT: Expected E = n * (n-1) * (1-sparsity)
 *
 * BIOLOGICAL: Matches random wiring in early development before pruning
 *
 * @param n_neurons Number of neurons
 * @param sparsity Fraction of potential edges to omit [0.0, 1.0)
 * @param seed Random seed for reproducibility (0 = use time)
 * @return Wiring structure or NULL on failure
 */
lnn_wiring_t* lnn_wiring_create_random(uint32_t n_neurons, float sparsity, uint64_t seed);

/**
 * @brief Create small-world wiring (Watts-Strogatz)
 *
 * WHAT: Ring lattice with k neighbors, then rewire with probability p
 * WHY:  Small-world networks balance local clustering and global connectivity
 * HOW:  1. Create ring with k/2 neighbors on each side
 *       2. Rewire each edge with probability p to random target
 *
 * COMPLEXITY: O(n * k) space and time
 * EDGE COUNT: n * k (before rewiring)
 *
 * BIOLOGICAL: Models local connectivity with long-range shortcuts
 * - High clustering: Local processing modules
 * - Short path length: Fast global communication
 *
 * PARAMETERS:
 * - k = 4-8: Typical local connectivity
 * - p = 0.01-0.1: Small rewiring probability for shortcuts
 *
 * @param n_neurons Number of neurons
 * @param k Number of neighbors in ring lattice (must be even)
 * @param p Rewiring probability [0.0, 1.0]
 * @param seed Random seed for reproducibility
 * @return Wiring structure or NULL on failure
 */
lnn_wiring_t* lnn_wiring_create_small_world(uint32_t n_neurons, uint32_t k, float p, uint64_t seed);

/**
 * @brief Create scale-free wiring (Barabasi-Albert)
 *
 * WHAT: Preferential attachment model with m edges per new node
 * WHY:  Scale-free networks are robust to random failures, hubs enable integration
 * HOW:  1. Start with m0 fully connected nodes
 *       2. Add nodes sequentially, attach to m existing nodes
 *       3. Probability of connection proportional to existing degree
 *
 * COMPLEXITY: O(n * m) space and time
 * EDGE COUNT: m0 * (m0-1)/2 + (n - m0) * m
 * DEGREE DISTRIBUTION: P(k) ~ k^(-3) power law
 *
 * BIOLOGICAL: Models hub neurons in cortex (pyramidal cells)
 * - Hub neurons integrate information from many sources
 * - Robust to random lesions, vulnerable to targeted hub damage
 *
 * PARAMETERS:
 * - m = 2-5: Edges per new node
 * - Produces hubs with degree >> average
 *
 * @param n_neurons Total number of neurons
 * @param m Edges to attach for each new node (must be < n_neurons)
 * @param seed Random seed for reproducibility
 * @return Wiring structure or NULL on failure
 */
lnn_wiring_t* lnn_wiring_create_scale_free(uint32_t n_neurons, uint32_t m, uint64_t seed);

/**
 * @brief Create NCP (Neural Circuit Policy) wiring
 *
 * WHAT: Hierarchical connectivity inspired by C. elegans motor circuits
 * WHY:  Biologically-realistic architecture for sensorimotor control
 * HOW:  Four neuron types with structured connectivity:
 *       - Sensory: Input neurons, no incoming connections
 *       - Inter: Hidden layer, receives from sensory and inter (recurrent)
 *       - Command: Decision layer, receives from inter
 *       - Motor: Output neurons, receives from command
 *
 * CONNECTIVITY RULES (sparse):
 * - Sensory → Inter: ~50% connectivity
 * - Inter ↔ Inter: ~30% recurrent (enables dynamics)
 * - Inter → Command: ~50% connectivity
 * - Command → Motor: ~80% connectivity (dense for precise control)
 *
 * BIOLOGICAL GROUNDING:
 * - C. elegans has 302 neurons with similar architecture
 * - Sparse connectivity enables efficient learning
 * - Recurrent inter-neurons maintain state
 *
 * COMPLEXITY: O(n_total * avg_sparsity) space
 * TOTAL NEURONS: n_sensory + n_inter + n_command + n_motor
 *
 * @param n_sensory Number of sensory (input) neurons
 * @param n_inter Number of interneurons (hidden)
 * @param n_command Number of command neurons (decision)
 * @param n_motor Number of motor (output) neurons
 * @return Wiring structure or NULL on failure
 */
lnn_wiring_t* lnn_wiring_create_ncp(uint32_t n_sensory, uint32_t n_inter,
                                     uint32_t n_command, uint32_t n_motor);

/**
 * @brief Create wiring from adjacency matrix
 *
 * WHAT: Convert dense adjacency matrix to sparse CSR format
 * WHY:  Allow user-defined custom connectivity patterns
 * HOW:  Scan dense matrix row-by-row, extract non-zero entries to CSR
 *
 * COMPLEXITY: O(n^2) time, O(E) space where E = number of edges
 *
 * @param adj Adjacency matrix [n_neurons x n_neurons] (row-major)
 *            adj[i*n + j] = 1 if edge from i to j, 0 otherwise
 * @param n_neurons Number of neurons (matrix dimension)
 * @return Wiring structure or NULL on failure
 */
lnn_wiring_t* lnn_wiring_create_from_adjacency(const uint8_t* adj, uint32_t n_neurons);

/**
 * @brief Create wiring based on type enum
 *
 * WHAT: Factory function to create wiring by type
 * WHY:  Allows runtime wiring selection from configuration
 * HOW:  Dispatches to appropriate creator based on type
 *
 * @param type Wiring pattern type
 * @param n_neurons Number of neurons
 * @param sparsity Target sparsity for random patterns
 * @return Wiring structure or NULL on failure
 */
lnn_wiring_t* lnn_wiring_create(lnn_wiring_type_t type, uint32_t n_neurons, float sparsity);

/*=============================================================================
 * Wiring Destruction
 *===========================================================================*/

/**
 * @brief Destroy wiring and free all resources
 *
 * WHAT: Free CSR arrays and wiring structure
 * WHY:  Prevent memory leaks
 * HOW:  Free row_ptr, col_idx, edge_weights (if present), then wiring struct
 *
 * @param wiring Wiring to destroy (NULL is safe)
 */
void lnn_wiring_destroy(lnn_wiring_t* wiring);

/*=============================================================================
 * Wiring Queries
 *===========================================================================*/

/**
 * @brief Check if edge exists from neuron 'from' to neuron 'to'
 *
 * WHAT: Query CSR structure for edge existence
 * WHY:  Enable dynamic connectivity queries
 * HOW:  Binary search in col_idx[row_ptr[from] : row_ptr[from+1]]
 *
 * COMPLEXITY: O(log(degree)) for sorted CSR, O(degree) for unsorted
 *
 * @param wiring Wiring structure
 * @param from Source neuron ID
 * @param to Target neuron ID
 * @return true if edge exists, false otherwise
 */
bool lnn_wiring_has_edge(const lnn_wiring_t* wiring, uint32_t from, uint32_t to);

/**
 * @brief Check if neurons are connected (alias for lnn_wiring_has_edge)
 */
static inline bool lnn_wiring_is_connected(const lnn_wiring_t* wiring, uint32_t from, uint32_t to) {
    return lnn_wiring_has_edge(wiring, from, to);
}

/**
 * @brief Get out-degree of neuron (number of outgoing connections)
 *
 * WHAT: Count edges originating from neuron
 * WHY:  Important for understanding connectivity distribution
 * HOW:  return row_ptr[neuron+1] - row_ptr[neuron]
 *
 * COMPLEXITY: O(1)
 *
 * @param wiring Wiring structure
 * @param neuron Neuron ID
 * @return Number of outgoing edges
 */
uint32_t lnn_wiring_out_degree(const lnn_wiring_t* wiring, uint32_t neuron);

/**
 * @brief Get in-degree of neuron (number of incoming connections)
 *
 * WHAT: Count edges targeting neuron
 * WHY:  Important for understanding information integration
 * HOW:  Scan all col_idx entries for matches (requires full scan)
 *
 * COMPLEXITY: O(E) where E = total edges
 * NOTE: Expensive operation, cache results if querying repeatedly
 *
 * @param wiring Wiring structure
 * @param neuron Neuron ID
 * @return Number of incoming edges
 */
uint32_t lnn_wiring_in_degree(const lnn_wiring_t* wiring, uint32_t neuron);

/**
 * @brief Get neighbors of neuron (outgoing connections)
 *
 * WHAT: Return pointer to neighbor IDs in CSR format
 * WHY:  Efficient iteration over neuron's targets
 * HOW:  Return &col_idx[row_ptr[neuron]], set count to out_degree
 *
 * COMPLEXITY: O(1) to get pointer, O(degree) to iterate
 *
 * USAGE:
 *   uint32_t count;
 *   const uint32_t* neighbors = lnn_wiring_get_neighbors(wiring, 5, &count);
 *   for (uint32_t i = 0; i < count; i++) {
 *       printf("Neighbor: %u\n", neighbors[i]);
 *   }
 *
 * @param wiring Wiring structure
 * @param neuron Neuron ID
 * @param count Output: number of neighbors
 * @return Pointer to neighbor array (valid until wiring destroyed)
 */
const uint32_t* lnn_wiring_get_neighbors(const lnn_wiring_t* wiring, uint32_t neuron, uint32_t* count);

/*=============================================================================
 * Wiring Utilities
 *===========================================================================*/

/**
 * @brief Compute sparsity of wiring
 *
 * WHAT: Fraction of potential edges that are zero
 * WHY:  Quantify connectivity density
 * HOW:  sparsity = 1.0 - (n_edges / (n_neurons^2))
 *
 * COMPLEXITY: O(1)
 *
 * @param wiring Wiring structure
 * @return Sparsity in [0.0, 1.0]
 */
float lnn_wiring_compute_sparsity(const lnn_wiring_t* wiring);

/**
 * @brief Convert CSR wiring to dense adjacency matrix
 *
 * WHAT: Expand sparse CSR to full matrix representation
 * WHY:  Needed for visualization or dense algorithms
 * HOW:  Initialize dense to zeros, then set dense[i][j] = 1 for each edge
 *
 * COMPLEXITY: O(n^2) time and space
 * WARNING: Memory intensive for large n (use with caution)
 *
 * @param wiring Wiring structure
 * @param dense Output: dense matrix [rows x cols] (row-major, pre-allocated)
 * @param rows Number of rows (must be >= n_neurons)
 * @param cols Number of columns (must be >= n_neurons)
 * @return 0 on success, negative on error
 */
int lnn_wiring_to_dense(const lnn_wiring_t* wiring, float* dense, uint32_t rows, uint32_t cols);

/**
 * @brief Convert wiring type enum to string
 *
 * WHAT: Human-readable name for wiring type
 * WHY:  Logging and debugging
 * HOW:  Lookup table
 *
 * @param type Wiring type
 * @return String name (e.g., "Small-World")
 */
const char* lnn_wiring_type_to_string(lnn_wiring_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_WIRING_H */
