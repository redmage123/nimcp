//=============================================================================
// nimcp_cortical_column_ternary.h - Ternary Inter-Column Connectivity
//=============================================================================
/**
 * @file nimcp_cortical_column_ternary.h
 * @brief Ternary weight support for cortical column connectivity
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Ternary inter-column weights with winner-take-all ternary dynamics
 * WHY:  Memory-efficient column connectivity with discrete competition
 * HOW:  Ternary adjacency matrices between hypercolumns/minicolumns
 *
 * BIOLOGICAL BASIS:
 * - Cortical columns compete via lateral inhibition
 * - Connections are effectively excitatory (+1), inhibitory (-1), or absent (0)
 * - Winner-take-all dynamics naturally discretize activation
 *
 * ARCHITECTURE:
 * ```
 *   Hypercolumn A               Hypercolumn B
 *   ┌─────────────┐            ┌─────────────┐
 *   │ Mini 0      │───(+1)────>│ Mini 0      │
 *   │ Mini 1      │───(-1)────>│ Mini 1      │  (inhibition)
 *   │ Mini 2      │───( 0)     │ Mini 2      │  (no connection)
 *   └─────────────┘            └─────────────┘
 * ```
 *
 * TERNARY WTA DYNAMICS:
 * - Minicolumn activations converted to ternary: active(+1), neutral(0), suppressed(-1)
 * - Ternary lateral inhibition matrix modulates competition
 * - Final output is ternary state for each minicolumn
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_COLUMN_TERNARY_H
#define NIMCP_CORTICAL_COLUMN_TERNARY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "core/cortical_columns/nimcp_cortical_column.h"
#include "utils/ternary/nimcp_ternary.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default threshold for activation to ternary conversion */
#define CC_TERNARY_ACTIVE_THRESHOLD    0.7f
#define CC_TERNARY_SUPPRESS_THRESHOLD  0.3f

/** Magic number for validation */
#define CC_TERNARY_MAGIC 0x43435454  /* "CCTT" */

//=============================================================================
// Ternary Competition Configuration
//=============================================================================

/**
 * @brief Ternary winner-take-all configuration
 *
 * WHAT: Parameters for ternary competition dynamics
 * WHY:  Control how activations are discretized and compete
 * HOW:  Threshold-based quantization with ternary lateral inhibition
 */
typedef struct {
    float active_threshold;           /**< Activation >= this becomes +1 */
    float suppress_threshold;         /**< Activation <= this becomes -1 */
    bool use_soft_ternary;            /**< Allow intermediate states during competition */
    uint32_t max_iterations;          /**< Max iterations for convergence */
    float convergence_epsilon;        /**< Convergence threshold */
    float inhibition_strength;        /**< Strength of lateral inhibition */
} cc_ternary_wta_config_t;

//=============================================================================
// Inter-Column Connectivity Structure
//=============================================================================

/**
 * @brief Ternary inter-column connectivity matrix
 *
 * WHAT: Ternary weights between cortical columns
 * WHY:  Memory-efficient representation of excitatory/inhibitory connections
 * HOW:  Ternary matrix where +1=excitatory, 0=absent, -1=inhibitory
 *
 * USE CASES:
 * - Lateral inhibition within hypercolumn (typically all -1 except self)
 * - Cross-hypercolumn connectivity (mixed +1/-1 patterns)
 * - Feature binding between column populations
 */
typedef struct {
    uint32_t magic;                   /**< Validation: CC_TERNARY_MAGIC */
    uint32_t n_source;                /**< Number of source columns */
    uint32_t n_target;                /**< Number of target columns */

    /* Ternary connectivity */
    trit_matrix_t* weights;           /**< Ternary weight matrix [n_source x n_target] */

    /* Scale factors for dequantization */
    float excitatory_scale;           /**< Scale for +1 weights */
    float inhibitory_scale;           /**< Scale for -1 weights */

    /* Statistics */
    uint32_t n_excitatory;            /**< Count of +1 connections */
    uint32_t n_inhibitory;            /**< Count of -1 connections */
    uint32_t n_absent;                /**< Count of 0 connections */
    float ei_ratio;                   /**< Excitatory/Inhibitory ratio */
} cc_ternary_connectivity_t;

/**
 * @brief Ternary minicolumn state
 *
 * WHAT: Ternary representation of minicolumn activation
 * WHY:  Discrete state for WTA dynamics
 * HOW:  Map float activation to {-1, 0, +1}
 */
typedef struct {
    trit_t state;                     /**< Current ternary state */
    float raw_activation;             /**< Underlying float activation */
    float confidence;                 /**< Confidence in ternary state [0, 1] */
    uint32_t win_count;               /**< Times won WTA competition */
} cc_ternary_state_t;

/**
 * @brief Ternary hypercolumn for WTA competition
 *
 * WHAT: Hypercolumn with ternary inter-minicolumn weights
 * WHY:  Discrete competition dynamics between minicolumns
 * HOW:  Ternary lateral inhibition matrix with iterative WTA
 */
typedef struct {
    uint32_t magic;                   /**< Validation */
    hypercolumn_t* base;              /**< Base hypercolumn (owned elsewhere) */

    /* Ternary state per minicolumn */
    cc_ternary_state_t* states;       /**< Ternary states [num_minicolumns] */
    uint32_t num_minicolumns;         /**< Number of minicolumns */

    /* Lateral inhibition (ternary) */
    cc_ternary_connectivity_t* lateral; /**< Lateral inhibition matrix */

    /* WTA configuration */
    cc_ternary_wta_config_t wta_config;

    /* Statistics */
    uint32_t total_competitions;       /**< Total WTA competitions run */
    uint32_t convergence_failures;     /**< Competitions that didn't converge */
    float avg_iterations;              /**< Average iterations to converge */
} cc_ternary_hypercolumn_t;

//=============================================================================
// Connectivity Lifecycle
//=============================================================================

/**
 * @brief Create ternary inter-column connectivity
 *
 * WHAT: Allocate and initialize ternary connectivity matrix
 * WHY:  Set up connection structure between column populations
 * HOW:  Create ternary matrix with given dimensions
 *
 * @param n_source Number of source columns
 * @param n_target Number of target columns
 * @param pack_mode Ternary packing mode
 * @return Connectivity structure or NULL on failure
 */
cc_ternary_connectivity_t* cc_ternary_connectivity_create(
    uint32_t n_source,
    uint32_t n_target,
    ternary_pack_mode_t pack_mode
);

/**
 * @brief Create lateral inhibition connectivity
 *
 * WHAT: Create ternary matrix for lateral inhibition
 * WHY:  Standard pattern: self-excitation, neighbor inhibition
 * HOW:  Diagonal +1, off-diagonal -1 (or 0 for distant)
 *
 * PATTERN TYPES:
 * - FULL_INHIBITION: All off-diagonal are -1
 * - NEIGHBOR_INHIBITION: Only adjacent columns are -1
 * - MEXICAN_HAT: +1 for self, -1 for near, 0 for far
 *
 * @param n_columns Number of columns
 * @param pattern Inhibition pattern type
 * @param neighborhood_size Size of inhibitory neighborhood (for neighbor patterns)
 * @return Connectivity structure or NULL on failure
 */
cc_ternary_connectivity_t* cc_ternary_connectivity_create_lateral(
    uint32_t n_columns,
    int pattern,  /* 0=full, 1=neighbor, 2=mexican_hat */
    uint32_t neighborhood_size
);

/**
 * @brief Destroy ternary connectivity
 *
 * @param conn Connectivity to destroy
 */
void cc_ternary_connectivity_destroy(cc_ternary_connectivity_t* conn);

/**
 * @brief Clone ternary connectivity
 *
 * @param src Source connectivity
 * @return Cloned connectivity or NULL on failure
 */
cc_ternary_connectivity_t* cc_ternary_connectivity_clone(
    const cc_ternary_connectivity_t* src
);

//=============================================================================
// Connectivity Access
//=============================================================================

/**
 * @brief Get connection weight between columns
 *
 * @param conn Connectivity structure
 * @param source Source column index
 * @param target Target column index
 * @return Connection weight {-1, 0, +1}
 */
trit_t cc_ternary_connectivity_get(
    const cc_ternary_connectivity_t* conn,
    uint32_t source,
    uint32_t target
);

/**
 * @brief Set connection weight between columns
 *
 * @param conn Connectivity structure
 * @param source Source column index
 * @param target Target column index
 * @param weight Connection weight {-1, 0, +1}
 * @return 0 on success, negative on error
 */
int cc_ternary_connectivity_set(
    cc_ternary_connectivity_t* conn,
    uint32_t source,
    uint32_t target,
    trit_t weight
);

/**
 * @brief Apply ternary connectivity to activation vector
 *
 * WHAT: Compute output = W * input using ternary weights
 * WHY:  Propagate activations through ternary connections
 * HOW:  Sparse ternary matmul
 *
 * @param conn Connectivity structure
 * @param input Source activations [n_source]
 * @param output Target activations [n_target] (pre-allocated)
 * @return 0 on success, negative on error
 */
int cc_ternary_connectivity_apply(
    const cc_ternary_connectivity_t* conn,
    const float* input,
    float* output
);

//=============================================================================
// Ternary Hypercolumn Lifecycle
//=============================================================================

/**
 * @brief Create ternary hypercolumn wrapper
 *
 * WHAT: Wrap existing hypercolumn with ternary dynamics
 * WHY:  Add ternary WTA competition to standard hypercolumn
 * HOW:  Create ternary state and lateral connectivity
 *
 * @param base Base hypercolumn (not owned, must outlive wrapper)
 * @param wta_config WTA configuration (NULL for defaults)
 * @return Ternary hypercolumn or NULL on failure
 */
cc_ternary_hypercolumn_t* cc_ternary_hypercolumn_create(
    hypercolumn_t* base,
    const cc_ternary_wta_config_t* wta_config
);

/**
 * @brief Destroy ternary hypercolumn wrapper
 *
 * NOTE: Does not destroy the base hypercolumn
 *
 * @param thcol Ternary hypercolumn to destroy
 */
void cc_ternary_hypercolumn_destroy(cc_ternary_hypercolumn_t* thcol);

//=============================================================================
// Ternary WTA Competition
//=============================================================================

/**
 * @brief Run ternary winner-take-all competition
 *
 * WHAT: Execute iterative ternary WTA dynamics
 * WHY:  Discrete competition between minicolumns
 * HOW:  Iterate ternary state updates until convergence
 *
 * ALGORITHM:
 * 1. Convert float activations to initial ternary states
 * 2. For each iteration:
 *    a. Apply lateral inhibition: state_new = state - inhibition * neighbors
 *    b. Re-quantize to ternary
 *    c. Check for convergence (no state changes)
 * 3. Determine winner(s) based on final ternary states
 *
 * @param thcol Ternary hypercolumn
 * @return Index of winning minicolumn, or UINT32_MAX on error
 */
uint32_t cc_ternary_hypercolumn_wta(cc_ternary_hypercolumn_t* thcol);

/**
 * @brief Update ternary states from current activations
 *
 * WHAT: Convert float activations to ternary states
 * WHY:  Synchronize ternary representation with underlying activations
 * HOW:  Apply thresholds to quantize
 *
 * @param thcol Ternary hypercolumn
 * @return 0 on success, negative on error
 */
int cc_ternary_hypercolumn_update_states(cc_ternary_hypercolumn_t* thcol);

/**
 * @brief Get ternary state of minicolumn
 *
 * @param thcol Ternary hypercolumn
 * @param minicolumn Minicolumn index
 * @return Ternary state {-1, 0, +1}
 */
trit_t cc_ternary_hypercolumn_get_state(
    const cc_ternary_hypercolumn_t* thcol,
    uint32_t minicolumn
);

/**
 * @brief Get all ternary states as vector
 *
 * @param thcol Ternary hypercolumn
 * @param out_states Output trit array [num_minicolumns] (pre-allocated)
 * @return 0 on success, negative on error
 */
int cc_ternary_hypercolumn_get_all_states(
    const cc_ternary_hypercolumn_t* thcol,
    trit_t* out_states
);

/**
 * @brief Compute ternary distribution (counts)
 *
 * @param thcol Ternary hypercolumn
 * @param n_active Output count of +1 states
 * @param n_neutral Output count of 0 states
 * @param n_suppressed Output count of -1 states
 */
void cc_ternary_hypercolumn_distribution(
    const cc_ternary_hypercolumn_t* thcol,
    uint32_t* n_active,
    uint32_t* n_neutral,
    uint32_t* n_suppressed
);

//=============================================================================
// Configuration Helpers
//=============================================================================

/**
 * @brief Get default WTA configuration
 *
 * @param config Configuration to initialize
 */
void cc_ternary_wta_config_default(cc_ternary_wta_config_t* config);

/**
 * @brief Validate WTA configuration
 *
 * @param config Configuration to validate
 * @return 0 if valid, negative on error
 */
int cc_ternary_wta_config_validate(const cc_ternary_wta_config_t* config);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Convert float activation to ternary
 *
 * WHAT: Quantize single float to trit
 * WHY:  Basis for state conversion
 * HOW:  Apply thresholds
 *
 * @param activation Float activation [0, 1]
 * @param active_threshold Threshold for +1
 * @param suppress_threshold Threshold for -1
 * @return Trit value {-1, 0, +1}
 */
trit_t cc_activation_to_ternary(
    float activation,
    float active_threshold,
    float suppress_threshold
);

/**
 * @brief Convert ternary state to representative float
 *
 * WHAT: Map trit to float value
 * WHY:  Interface with float-based systems
 * HOW:  -1 -> 0.0, 0 -> 0.5, +1 -> 1.0
 *
 * @param state Ternary state
 * @return Float representation
 */
float cc_ternary_to_activation(trit_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORTICAL_COLUMN_TERNARY_H */
