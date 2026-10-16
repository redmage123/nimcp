//=============================================================================
// nimcp_community_detection_ternary.h - Ternary Community Detection
//=============================================================================
/**
 * @file nimcp_community_detection_ternary.h
 * @brief Ternary adjacency matrix support for community detection
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Community detection on ternary adjacency graphs
 * WHY:  Detect functional modules in networks with signed connections
 * HOW:  Extended Louvain algorithm for ternary adjacency matrices
 *
 * BIOLOGICAL BASIS:
 * - Neural networks have both excitatory (+1) and inhibitory (-1) connections
 * - Community structure considers connection sign in modularity
 * - Signed modularity captures E/I balance within communities
 *
 * TERNARY MODULARITY:
 * Extended Newman's Q for signed networks:
 *   Q_signed = (1/2m) * Σ [A_ij - γ*(k_i_+ * k_j_+ - k_i_- * k_j_-)/2m] * δ(c_i, c_j)
 *
 * Where:
 * - A_ij ∈ {-1, 0, +1} is ternary adjacency
 * - k_i_+ = sum of positive edges for node i
 * - k_i_- = sum of negative edges for node i
 * - γ = resolution parameter
 *
 * USAGE:
 * ```c
 * // Create ternary adjacency from neural network
 * trit_matrix_t* adj = community_ternary_create_adjacency(network);
 *
 * // Detect communities
 * community_ternary_config_t config;
 * community_ternary_config_default(&config);
 *
 * community_ternary_result_t* result = community_ternary_detect(adj, &config);
 *
 * printf("Signed modularity Q: %.3f\n", result->modularity_signed);
 * printf("Communities: %u\n", result->num_communities);
 *
 * community_ternary_result_free(result);
 * trit_matrix_destroy(adj);
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_COMMUNITY_DETECTION_TERNARY_H
#define NIMCP_COMMUNITY_DETECTION_TERNARY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "core/topology/nimcp_community_detection.h"
#include "utils/ternary/nimcp_ternary.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default resolution parameter for ternary modularity */
#define COMMUNITY_TERNARY_DEFAULT_RESOLUTION 1.0f

/** Magic number for validation */
#define COMMUNITY_TERNARY_MAGIC 0x434D5454  /* "CMTT" */

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for ternary community detection
 *
 * WHAT: Parameters controlling ternary Louvain algorithm
 * WHY:  Tune detection for different network types
 * HOW:  Resolution, iteration limits, and signed modularity options
 */
typedef struct {
    /* Algorithm parameters */
    uint32_t max_iterations;          /**< Max iterations per phase */
    float min_modularity_gain;        /**< Stop if gain < threshold */
    uint32_t max_communities;         /**< Limit number of communities (0 = no limit) */
    float resolution;                 /**< Resolution parameter (>1 = more communities) */
    uint32_t random_seed;             /**< Seed for randomization (0 = time-based) */

    /* Ternary-specific */
    bool use_signed_modularity;       /**< Use signed modularity formula */
    float negative_weight;            /**< Weight for negative edges (default 1.0) */
    bool separate_pos_neg;            /**< Compute separate pos/neg modularity */
} community_ternary_config_t;

//=============================================================================
// Result Structures
//=============================================================================

/**
 * @brief Ternary community detection result
 *
 * WHAT: Complete result from ternary community detection
 * WHY:  Include signed modularity and E/I statistics
 * HOW:  Extended community_structure_t with ternary metrics
 */
typedef struct {
    uint32_t magic;                   /**< Validation: COMMUNITY_TERNARY_MAGIC */
    uint32_t num_nodes;               /**< Total nodes in network */
    uint32_t num_communities;         /**< Number of detected communities */
    uint32_t* community_ids;          /**< Community ID for each node [num_nodes] */
    uint32_t* community_sizes;        /**< Size of each community [num_communities] */

    /* Standard modularity */
    float modularity;                 /**< Unsigned modularity Q */

    /* Signed modularity */
    float modularity_signed;          /**< Signed modularity Q_signed */
    float modularity_positive;        /**< Modularity of positive edges only */
    float modularity_negative;        /**< Modularity of negative edges only */

    /* Per-community E/I statistics */
    float* ei_ratio;                  /**< E/I ratio per community [num_communities] */
    uint32_t* internal_positive;      /**< Internal +1 edges per community */
    uint32_t* internal_negative;      /**< Internal -1 edges per community */
    uint32_t* external_positive;      /**< External +1 edges per community */
    uint32_t* external_negative;      /**< External -1 edges per community */

    /* Global statistics */
    uint32_t total_positive;          /**< Total +1 edges */
    uint32_t total_negative;          /**< Total -1 edges */
    uint32_t total_absent;            /**< Total 0 edges */
    float network_ei_ratio;           /**< Overall network E/I ratio */
} community_ternary_result_t;

//=============================================================================
// Ternary Adjacency Creation
//=============================================================================

/**
 * @brief Create ternary adjacency from neural network
 *
 * WHAT: Extract ternary adjacency matrix from neural network
 * WHY:  Prepare network for community detection
 * HOW:  Quantize weights: |w| < threshold => 0, else sign(w)
 *
 * @param network Neural network
 * @param threshold Weight threshold for quantization
 * @param pack_mode Ternary packing mode
 * @return Ternary adjacency matrix or NULL on failure
 */
trit_matrix_t* community_ternary_create_adjacency(
    neural_network_t network,
    float threshold,
    ternary_pack_mode_t pack_mode
);

/**
 * @brief Create ternary adjacency from existing float adjacency
 *
 * WHAT: Quantize float adjacency matrix to ternary
 * WHY:  Convert existing adjacency for ternary community detection
 * HOW:  Apply threshold to each edge
 *
 * @param float_adj Float adjacency matrix (2D tensor)
 * @param threshold Quantization threshold
 * @param pack_mode Ternary packing mode
 * @return Ternary adjacency matrix or NULL on failure
 */
trit_matrix_t* community_ternary_quantize_adjacency(
    const float* float_adj,
    uint32_t num_nodes,
    float threshold,
    ternary_pack_mode_t pack_mode
);

//=============================================================================
// Community Detection
//=============================================================================

/**
 * @brief Detect communities in ternary adjacency graph
 *
 * WHAT: Run modified Louvain algorithm on ternary graph
 * WHY:  Find functional modules considering edge signs
 * HOW:  Optimize signed modularity via greedy local moves
 *
 * ALGORITHM:
 * 1. Initialize each node in own community
 * 2. For each node:
 *    a. Compute signed modularity gain for moving to each neighbor's community
 *    b. Move to community with max positive gain
 * 3. Repeat until no improvements
 * 4. Aggregate communities and repeat
 *
 * @param adjacency Ternary adjacency matrix [n x n]
 * @param config Detection configuration (NULL for defaults)
 * @return Detection result or NULL on failure
 */
community_ternary_result_t* community_ternary_detect(
    const trit_matrix_t* adjacency,
    const community_ternary_config_t* config
);

/**
 * @brief Free community detection result
 *
 * @param result Result to free
 */
void community_ternary_result_free(community_ternary_result_t* result);

//=============================================================================
// Modularity Computation
//=============================================================================

/**
 * @brief Compute signed modularity for given partition
 *
 * WHAT: Calculate Q_signed for specific community assignment
 * WHY:  Evaluate quality of community structure in signed network
 * HOW:  Extended Newman's formula for signed edges
 *
 * @param adjacency Ternary adjacency matrix
 * @param community_ids Community assignment for each node
 * @param num_communities Number of distinct communities
 * @param resolution Resolution parameter
 * @return Signed modularity Q_signed
 */
float community_ternary_modularity_signed(
    const trit_matrix_t* adjacency,
    const uint32_t* community_ids,
    uint32_t num_communities,
    float resolution
);

/**
 * @brief Compute unsigned modularity (treating all edges as positive)
 *
 * WHAT: Standard Newman's modularity ignoring signs
 * WHY:  Comparison with signed modularity
 * HOW:  Count edges regardless of sign
 *
 * @param adjacency Ternary adjacency matrix
 * @param community_ids Community assignment for each node
 * @param num_communities Number of distinct communities
 * @return Unsigned modularity Q
 */
float community_ternary_modularity_unsigned(
    const trit_matrix_t* adjacency,
    const uint32_t* community_ids,
    uint32_t num_communities
);

/**
 * @brief Compute modularity gain for moving node to community
 *
 * WHAT: Delta Q if node moves from current to target community
 * WHY:  Core operation for Louvain algorithm
 * HOW:  Difference in signed modularity
 *
 * @param adjacency Ternary adjacency matrix
 * @param community_ids Current community assignments
 * @param node Node to potentially move
 * @param target_community Target community
 * @param resolution Resolution parameter
 * @return Modularity gain (negative if move decreases Q)
 */
float community_ternary_modularity_gain(
    const trit_matrix_t* adjacency,
    const uint32_t* community_ids,
    uint32_t node,
    uint32_t target_community,
    float resolution
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Compute E/I ratio for a community
 *
 * WHAT: Calculate excitatory/inhibitory edge ratio within community
 * WHY:  Monitor balance of E/I connections
 * HOW:  Count positive vs negative internal edges
 *
 * @param adjacency Ternary adjacency matrix
 * @param community_ids Community assignments
 * @param community Target community ID
 * @return E/I ratio (INFINITY if no inhibitory edges)
 */
float community_ternary_ei_ratio(
    const trit_matrix_t* adjacency,
    const uint32_t* community_ids,
    uint32_t community
);

/**
 * @brief Count edge types within and between communities
 *
 * WHAT: Count +1/-1/0 edges for community analysis
 * WHY:  Detailed edge statistics
 * HOW:  Iterate adjacency and categorize by community membership
 *
 * @param adjacency Ternary adjacency matrix
 * @param community_ids Community assignments
 * @param community Target community ID
 * @param internal_pos Output: internal +1 edges
 * @param internal_neg Output: internal -1 edges
 * @param external_pos Output: external +1 edges
 * @param external_neg Output: external -1 edges
 */
void community_ternary_edge_counts(
    const trit_matrix_t* adjacency,
    const uint32_t* community_ids,
    uint32_t community,
    uint32_t* internal_pos,
    uint32_t* internal_neg,
    uint32_t* external_pos,
    uint32_t* external_neg
);

//=============================================================================
// Configuration Helpers
//=============================================================================

/**
 * @brief Get default ternary community detection configuration
 *
 * @param config Configuration to initialize
 */
void community_ternary_config_default(community_ternary_config_t* config);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @return 0 if valid, negative on error
 */
int community_ternary_config_validate(const community_ternary_config_t* config);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Print ternary community statistics
 *
 * @param result Detection result
 */
void community_ternary_print_stats(const community_ternary_result_t* result);

/**
 * @brief Get community with best E/I balance
 *
 * WHAT: Find community closest to target E/I ratio
 * WHY:  Identify well-balanced functional modules
 * HOW:  Compare E/I ratios to target
 *
 * @param result Detection result
 * @param target_ratio Target E/I ratio (e.g., 4.0 for 80% E / 20% I)
 * @return Community ID with closest E/I ratio
 */
uint32_t community_ternary_best_balanced(
    const community_ternary_result_t* result,
    float target_ratio
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COMMUNITY_DETECTION_TERNARY_H */
