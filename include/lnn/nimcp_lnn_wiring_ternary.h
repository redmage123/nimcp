//=============================================================================
// nimcp_lnn_wiring_ternary.h - Ternary Wiring Patterns for LNN
//=============================================================================
/**
 * @file nimcp_lnn_wiring_ternary.h
 * @brief Ternary connection strength wiring for Liquid Neural Networks
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Ternary wiring with connection strengths {STRONG=+1, WEAK=0, ABSENT=-1}
 * WHY:  Capture bidirectional connection semantics in neural wiring
 * HOW:  Extend wiring patterns with ternary adjacency representation
 *
 * BIOLOGICAL BASIS:
 * - Neural connections can be excitatory (+1), inhibitory (-1), or absent (0)
 * - Wiring patterns encode connectivity strength semantics
 * - Ternary encoding captures sign and presence in single value
 *
 * CONNECTION SEMANTICS:
 * | Value | Name   | Meaning                                    |
 * |-------|--------|--------------------------------------------|
 * | +1    | STRONG | Strong excitatory connection (potentiated) |
 * |  0    | WEAK   | Weak or neutral connection                 |
 * | -1    | ABSENT | No connection (can mean inhibitory)        |
 *
 * USAGE:
 * ```c
 * // Create ternary wiring pattern
 * lnn_ternary_wiring_config_t config = {
 *     .type = LNN_WIRING_RANDOM,
 *     .n_neurons = 100,
 *     .sparsity = 0.8f,
 *     .excitatory_ratio = 0.8f  // 80% excitatory, 20% inhibitory
 * };
 *
 * lnn_ternary_wiring_t* wiring = lnn_ternary_wiring_create(&config);
 *
 * // Query connection
 * trit_t conn = lnn_ternary_wiring_get(wiring, 0, 5);
 * if (conn == LNN_CONN_STRONG) {
 *     // Strong excitatory connection from 0 to 5
 * }
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_LNN_WIRING_TERNARY_H
#define NIMCP_LNN_WIRING_TERNARY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "lnn/nimcp_lnn_types.h"
#include "utils/ternary/nimcp_ternary.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Connection Strength Constants
//=============================================================================

/**
 * @brief Ternary connection strength values
 *
 * WHAT: Named constants for ternary connection semantics
 * WHY:  Clear semantic meaning for wiring patterns
 */
#define LNN_CONN_STRONG   TRIT_POSITIVE   /**< Strong/excitatory (+1) */
#define LNN_CONN_WEAK     TRIT_UNKNOWN    /**< Weak/neutral (0) */
#define LNN_CONN_ABSENT   TRIT_NEGATIVE   /**< Absent/inhibitory (-1) */

/** Alternative naming for E/I balance context */
#define LNN_CONN_EXCITATORY  TRIT_POSITIVE
#define LNN_CONN_NEUTRAL     TRIT_UNKNOWN
#define LNN_CONN_INHIBITORY  TRIT_NEGATIVE

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for ternary wiring generation
 *
 * WHAT: Parameters for creating ternary wiring patterns
 * WHY:  Control topology and E/I balance
 * HOW:  Specify pattern type, density, and excitatory ratio
 */
typedef struct {
    lnn_wiring_type_t type;          /**< Base wiring pattern type */
    uint32_t n_neurons;              /**< Number of neurons */
    float sparsity;                  /**< Target sparsity [0, 1) */
    float excitatory_ratio;          /**< Fraction of excitatory connections [0, 1] */

    /* NCP-specific */
    uint32_t n_sensory;              /**< Sensory neurons (NCP) */
    uint32_t n_inter;                /**< Interneurons (NCP) */
    uint32_t n_command;              /**< Command neurons (NCP) */
    uint32_t n_motor;                /**< Motor neurons (NCP) */

    /* Small-world (Watts-Strogatz) */
    float rewire_prob;               /**< Rewiring probability */
    uint32_t k_neighbors;            /**< Initial neighbors */

    /* Scale-free (Barabasi-Albert) */
    uint32_t m_edges;                /**< Edges per new node */

    /* Random seed */
    uint64_t seed;                   /**< Random seed (0 = time-based) */

    /* Ternary-specific */
    bool use_packed_storage;         /**< Use packed ternary storage */
    ternary_pack_mode_t pack_mode;   /**< Packing mode */
    bool dale_law_compliant;         /**< Enforce Dale's law (neurons are either E or I) */
} lnn_ternary_wiring_config_t;

//=============================================================================
// Ternary Wiring Structure
//=============================================================================

/**
 * @brief Ternary wiring pattern for LNN
 *
 * WHAT: Adjacency structure with ternary connection strengths
 * WHY:  Encode connectivity with semantic meaning
 * HOW:  Dense or sparse ternary matrix storage
 *
 * SEMANTIC MAPPING:
 * - adjacency[i][j] = +1: Strong connection from i to j
 * - adjacency[i][j] =  0: Weak/absent connection
 * - adjacency[i][j] = -1: Inhibitory connection or no connection
 */
typedef struct {
    uint32_t magic;                  /**< Validation magic */
    lnn_wiring_type_t type;          /**< Wiring pattern type */
    uint32_t n_neurons;              /**< Number of neurons */

    /* Dense ternary adjacency matrix */
    trit_matrix_t* adjacency;        /**< Ternary adjacency [n_neurons x n_neurons] */

    /* Statistics */
    uint32_t n_excitatory;           /**< Count of excitatory connections */
    uint32_t n_inhibitory;           /**< Count of inhibitory connections */
    uint32_t n_absent;               /**< Count of absent connections */
    float actual_sparsity;           /**< Actual sparsity (n_absent / total) */
    float actual_ei_ratio;           /**< Actual E/I ratio */

    /* Dale's law compliance (optional) */
    bool dale_law_compliant;         /**< Whether Dale's law enforced */
    bool* neuron_type;               /**< Per-neuron type: true=excitatory, false=inhibitory */

    /* NCP role assignments */
    uint32_t n_sensory;
    uint32_t n_inter;
    uint32_t n_command;
    uint32_t n_motor;
    lnn_neuron_role_t* roles;        /**< Role for each neuron [n_neurons] */
} lnn_ternary_wiring_t;

/** Magic number for validation */
#define LNN_TERNARY_WIRING_MAGIC 0x4C4E5457  /* "LNTW" */

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create ternary wiring pattern
 *
 * WHAT: Generate ternary adjacency based on configuration
 * WHY:  Create connectivity with semantic connection strengths
 * HOW:  Generate base pattern, assign ternary values
 *
 * GENERATION PROCESS:
 * 1. Generate base topology (random, small-world, etc.)
 * 2. For each edge:
 *    - If Dale's law: inherit neuron's excitatory/inhibitory type
 *    - Else: randomly assign based on excitatory_ratio
 * 3. Non-edges become ABSENT or WEAK based on context
 *
 * @param config Wiring configuration
 * @return Ternary wiring or NULL on failure
 */
lnn_ternary_wiring_t* lnn_ternary_wiring_create(
    const lnn_ternary_wiring_config_t* config
);

/**
 * @brief Create ternary wiring from existing binary wiring
 *
 * WHAT: Convert binary adjacency to ternary
 * WHY:  Upgrade existing wiring patterns
 * HOW:  Map edges to ternary, optionally assign E/I types
 *
 * @param wiring Existing wiring structure
 * @param excitatory_ratio Fraction excitatory [0, 1]
 * @param seed Random seed for E/I assignment
 * @return Ternary wiring or NULL on failure
 */
lnn_ternary_wiring_t* lnn_ternary_wiring_from_binary(
    const lnn_wiring_t* wiring,
    float excitatory_ratio,
    uint64_t seed
);

/**
 * @brief Destroy ternary wiring
 *
 * WHAT: Free all wiring memory
 * WHY:  Clean resource release
 * HOW:  Free adjacency matrix and metadata
 *
 * @param wiring Wiring to destroy
 */
void lnn_ternary_wiring_destroy(lnn_ternary_wiring_t* wiring);

/**
 * @brief Clone ternary wiring
 *
 * WHAT: Deep copy of wiring pattern
 * WHY:  Needed for checkpointing
 * HOW:  Duplicate all storage
 *
 * @param src Source wiring
 * @return Cloned wiring or NULL on failure
 */
lnn_ternary_wiring_t* lnn_ternary_wiring_clone(
    const lnn_ternary_wiring_t* src
);

//=============================================================================
// Access Functions
//=============================================================================

/**
 * @brief Get connection strength between neurons
 *
 * WHAT: Query ternary connection value
 * WHY:  Access individual connection semantics
 * HOW:  Look up in adjacency matrix
 *
 * @param wiring Ternary wiring
 * @param from Source neuron index
 * @param to Target neuron index
 * @return Connection strength {+1, 0, -1}
 */
trit_t lnn_ternary_wiring_get(
    const lnn_ternary_wiring_t* wiring,
    uint32_t from,
    uint32_t to
);

/**
 * @brief Set connection strength between neurons
 *
 * WHAT: Modify ternary connection value
 * WHY:  Plasticity and manual wiring modification
 * HOW:  Update adjacency matrix
 *
 * @param wiring Ternary wiring
 * @param from Source neuron index
 * @param to Target neuron index
 * @param strength New connection strength {+1, 0, -1}
 * @return 0 on success, negative on error
 */
int lnn_ternary_wiring_set(
    lnn_ternary_wiring_t* wiring,
    uint32_t from,
    uint32_t to,
    trit_t strength
);

/**
 * @brief Check if connection exists (non-WEAK)
 *
 * WHAT: Binary connectivity query
 * WHY:  Topology inspection without strength semantics
 * HOW:  Check if value != WEAK
 *
 * @param wiring Ternary wiring
 * @param from Source neuron
 * @param to Target neuron
 * @return true if connection exists
 */
bool lnn_ternary_wiring_connected(
    const lnn_ternary_wiring_t* wiring,
    uint32_t from,
    uint32_t to
);

/**
 * @brief Get outgoing connections for a neuron
 *
 * WHAT: List neurons that receive input from given neuron
 * WHY:  Traverse connectivity graph
 * HOW:  Scan row of adjacency matrix
 *
 * @param wiring Ternary wiring
 * @param neuron Source neuron index
 * @param targets Output array for target indices
 * @param strengths Output array for connection strengths (can be NULL)
 * @param max_count Maximum targets to return
 * @return Number of targets found
 */
uint32_t lnn_ternary_wiring_get_targets(
    const lnn_ternary_wiring_t* wiring,
    uint32_t neuron,
    uint32_t* targets,
    trit_t* strengths,
    uint32_t max_count
);

/**
 * @brief Get incoming connections for a neuron
 *
 * WHAT: List neurons that provide input to given neuron
 * WHY:  Traverse connectivity graph
 * HOW:  Scan column of adjacency matrix
 *
 * @param wiring Ternary wiring
 * @param neuron Target neuron index
 * @param sources Output array for source indices
 * @param strengths Output array for connection strengths (can be NULL)
 * @param max_count Maximum sources to return
 * @return Number of sources found
 */
uint32_t lnn_ternary_wiring_get_sources(
    const lnn_ternary_wiring_t* wiring,
    uint32_t neuron,
    uint32_t* sources,
    trit_t* strengths,
    uint32_t max_count
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get wiring statistics
 *
 * WHAT: Compute connectivity statistics
 * WHY:  Monitor network topology properties
 * HOW:  Count connection types and compute ratios
 *
 * @param wiring Ternary wiring
 * @param n_excitatory Output count of excitatory connections
 * @param n_inhibitory Output count of inhibitory connections
 * @param n_absent Output count of absent connections
 * @param sparsity Output sparsity ratio
 * @param ei_ratio Output excitatory/inhibitory ratio
 */
void lnn_ternary_wiring_stats(
    const lnn_ternary_wiring_t* wiring,
    uint32_t* n_excitatory,
    uint32_t* n_inhibitory,
    uint32_t* n_absent,
    float* sparsity,
    float* ei_ratio
);

/**
 * @brief Get out-degree for a neuron
 *
 * WHAT: Count outgoing connections
 * WHY:  Measure neuron connectivity
 * HOW:  Count non-zero/non-WEAK entries in row
 *
 * @param wiring Ternary wiring
 * @param neuron Neuron index
 * @return Out-degree (number of targets)
 */
uint32_t lnn_ternary_wiring_out_degree(
    const lnn_ternary_wiring_t* wiring,
    uint32_t neuron
);

/**
 * @brief Get in-degree for a neuron
 *
 * WHAT: Count incoming connections
 * WHY:  Measure neuron connectivity
 * HOW:  Count non-zero/non-WEAK entries in column
 *
 * @param wiring Ternary wiring
 * @param neuron Neuron index
 * @return In-degree (number of sources)
 */
uint32_t lnn_ternary_wiring_in_degree(
    const lnn_ternary_wiring_t* wiring,
    uint32_t neuron
);

//=============================================================================
// Configuration Helpers
//=============================================================================

/**
 * @brief Get default ternary wiring configuration
 *
 * WHAT: Initialize config with sensible defaults
 * WHY:  Convenient starting point
 * HOW:  Set random type, 50% sparsity, 80% excitatory
 *
 * @param config Configuration to initialize
 */
void lnn_ternary_wiring_config_default(lnn_ternary_wiring_config_t* config);

/**
 * @brief Validate ternary wiring configuration
 *
 * WHAT: Check configuration for valid values
 * WHY:  Catch errors before generation
 * HOW:  Validate ranges and consistency
 *
 * @param config Configuration to validate
 * @return 0 if valid, negative on error
 */
int lnn_ternary_wiring_config_validate(
    const lnn_ternary_wiring_config_t* config
);

//=============================================================================
// Conversion to Weight Matrix
//=============================================================================

/**
 * @brief Convert ternary wiring to ternary weight matrix
 *
 * WHAT: Generate weight matrix from wiring pattern
 * WHY:  Create LNN recurrent weights from topology
 * HOW:  Map ternary adjacency to weight matrix structure
 *
 * The resulting matrix can be used directly with lnn_ternary_matmul().
 *
 * @param wiring Source ternary wiring
 * @param pack_mode Packing mode for output matrix
 * @return Ternary matrix or NULL on failure
 */
lnn_ternary_matrix_t* lnn_ternary_wiring_to_matrix(
    const lnn_ternary_wiring_t* wiring,
    ternary_pack_mode_t pack_mode
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_WIRING_TERNARY_H */
