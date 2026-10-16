/**
 * @file nimcp_omni_cortical_columns_bridge.h
 * @brief Omnidirectional Inference to Cortical Columns Bridge
 * @version 1.0.0
 * @date 2025-01-04
 *
 * WHAT: Bridge integrating omnidirectional inference with cortical columns
 * WHY:  Enable prediction-driven columnar dynamics and winner-take-all
 * HOW:  Top-down predictions bias minicolumn competition, bottom-up features
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * COLUMNAR PREDICTIVE PROCESSING:
 * -------------------------------
 * Cortical columns as prediction units:
 *
 *   1. MINICOLUMN AS HYPOTHESIS:
 *      - Each minicolumn represents a hypothesis about input
 *      - Top-down prediction biases which minicolumn wins
 *      - Winner = best match between prediction and input
 *
 *   2. HYPERCOLUMN COMPETITION:
 *      - Hypercolumn hosts competing hypotheses
 *      - Mexican hat inhibition implements competition
 *      - Winner-take-all selects single hypothesis
 *      - OR K-winners for distributed representations
 *
 *   3. HIERARCHICAL PREDICTION:
 *      - Higher columns predict lower column activations
 *      - Lower columns send prediction errors upward
 *      - Bidirectional flow through hierarchy
 *
 * INFERENCE DIRECTION MAPPING:
 * ----------------------------
 *   Direction        Columnar Operation
 *   ─────────────────────────────────────────────
 *   Forward          Bottom-up feature propagation
 *   Backward         Top-down prediction biasing
 *   Lateral          Within-hypercolumn competition
 *   Hierarchical     Cross-level prediction flow
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OMNI_CORTICAL_COLUMNS_BRIDGE_H
#define NIMCP_OMNI_CORTICAL_COLUMNS_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct omni_cortical_columns_bridge omni_cortical_columns_bridge_t;
typedef struct jepa_bidirectional jepa_bidirectional_t;
typedef struct predictive_hierarchy predictive_hierarchy_t;
typedef struct minicolumn minicolumn_t;
typedef struct hypercolumn hypercolumn_t;
typedef struct cortical_column_pool cortical_column_pool_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Bio-async module ID for omni-cortical columns bridge */
#define BIO_MODULE_OMNI_CORTICAL_COLUMNS_BRIDGE    0x0E56

/** @brief Default prediction bias strength */
#define OMNI_CC_DEFAULT_BIAS_STRENGTH              0.5f

/** @brief Default competition temperature */
#define OMNI_CC_DEFAULT_TEMPERATURE                1.0f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Competition mode influenced by omni inference
 */
typedef enum {
    OMNI_CC_COMP_WTA = 0,        /**< Winner-take-all */
    OMNI_CC_COMP_K_WINNERS,      /**< K-winners survive */
    OMNI_CC_COMP_SOFTMAX,        /**< Soft competition (softmax) */
    OMNI_CC_COMP_HEBBIAN         /**< Hebbian learning competition */
} omni_cc_competition_mode_t;

/**
 * @brief Prediction bias mode
 */
typedef enum {
    OMNI_CC_BIAS_NONE = 0,       /**< No prediction bias */
    OMNI_CC_BIAS_ADDITIVE,       /**< Add bias to activation */
    OMNI_CC_BIAS_MULTIPLICATIVE, /**< Multiply activation by bias */
    OMNI_CC_BIAS_GATING          /**< Gate activation by bias */
} omni_cc_bias_mode_t;

/**
 * @brief Layer-specific processing mode
 */
typedef enum {
    OMNI_CC_LAYER_2_3 = 0,       /**< Superficial: Forward connections */
    OMNI_CC_LAYER_4,             /**< Granular: Input reception */
    OMNI_CC_LAYER_5,             /**< Deep: Motor/output predictions */
    OMNI_CC_LAYER_6              /**< Deep: Feedback predictions */
} omni_cc_layer_mode_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Omni effects on cortical columns
 */
typedef struct {
    float* prediction_bias;          /**< Prediction bias per minicolumn */
    uint32_t num_minicolumns;        /**< Number of minicolumns biased */
    float bias_strength;             /**< Overall bias strength */
    omni_cc_bias_mode_t bias_mode;   /**< Bias application mode */
    float competition_temp;          /**< Competition temperature */
    omni_cc_competition_mode_t comp_mode; /**< Competition mode */
    float* layer_modulation;         /**< Per-layer modulation [6] */
} omni_to_columns_effects_t;

/**
 * @brief Cortical columns effects on omni
 */
typedef struct {
    float* winner_activations;       /**< Winner minicolumn activations */
    uint32_t* winner_indices;        /**< Winner minicolumn indices */
    uint32_t num_winners;            /**< Number of winners */
    float* activation_distribution;  /**< Full activation distribution */
    uint32_t distribution_size;      /**< Distribution size */
    float competition_entropy;       /**< Entropy of competition */
    float sparsity;                  /**< Activation sparsity */
    float* prediction_errors;        /**< Column-wise PEs */
    uint32_t num_columns;            /**< Number of columns */
} columns_to_omni_effects_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Bias control */
    float default_bias_strength;     /**< Default prediction bias */
    omni_cc_bias_mode_t bias_mode;   /**< Bias application mode */
    bool enable_adaptive_bias;       /**< Adapt bias based on PE */

    /* Competition control */
    omni_cc_competition_mode_t competition_mode; /**< Competition mode */
    float competition_temperature;   /**< Softmax temperature */
    uint32_t k_winners;              /**< K for K-winners mode */

    /* Layer-specific */
    bool enable_layer_modulation;    /**< Layer-specific modulation */
    float layer_2_3_weight;          /**< Layer 2/3 weight */
    float layer_4_weight;            /**< Layer 4 weight */
    float layer_5_weight;            /**< Layer 5 weight */
    float layer_6_weight;            /**< Layer 6 weight */

    /* Sparsity control */
    float target_sparsity;           /**< Target activation sparsity */
    bool enable_homeostatic;         /**< Homeostatic sparsity control */

    /* Integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    bool enable_logging;             /**< Enable logging */
} omni_cc_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;          /**< Total bridge updates */
    uint64_t competition_events;     /**< Competition events */
    uint64_t bias_applications;      /**< Bias application count */
    float avg_winners_per_hypercolumn; /**< Avg winners */
    float avg_sparsity;              /**< Average sparsity */
    float avg_competition_entropy;   /**< Avg entropy */
    float avg_bias_strength;         /**< Avg applied bias */
} omni_cc_stats_t;

/**
 * @brief Omni-cortical columns bridge structure
 */
struct omni_cortical_columns_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge */

    omni_cc_config_t config;         /**< Configuration */

    /* Connected systems */
    jepa_bidirectional_t* jepa;      /**< Bidirectional JEPA */
    predictive_hierarchy_t* pred_hier; /**< Predictive hierarchy */
    cortical_column_pool_t* column_pool; /**< Cortical column pool */

    /* Computed effects */
    omni_to_columns_effects_t omni_effects;   /**< Omni → columns */
    columns_to_omni_effects_t column_effects; /**< Columns → omni */

    /* Statistics */
    omni_cc_stats_t stats;

    /* Bio-async integration */
    void* bio_context;               /**< Bio-async module context */
    bool bio_async_connected;        /**< Bio-async connection state */

    /* Thread safety */
    nimcp_platform_mutex_t* mutex;
};

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int omni_cc_default_config(omni_cc_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

omni_cortical_columns_bridge_t* omni_cc_bridge_create(
    const omni_cc_config_t* config);

void omni_cc_bridge_destroy(omni_cortical_columns_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

int omni_cc_connect_jepa(omni_cortical_columns_bridge_t* bridge,
                          jepa_bidirectional_t* jepa);

int omni_cc_connect_pred_hier(omni_cortical_columns_bridge_t* bridge,
                               predictive_hierarchy_t* pred_hier);

int omni_cc_connect_column_pool(omni_cortical_columns_bridge_t* bridge,
                                 cortical_column_pool_t* pool);

/* ============================================================================
 * Update API
 * ============================================================================ */

int omni_cc_update(omni_cortical_columns_bridge_t* bridge);

int omni_cc_apply_to_columns(omni_cortical_columns_bridge_t* bridge);

int omni_cc_apply_to_omni(omni_cortical_columns_bridge_t* bridge);

/* ============================================================================
 * Bias API
 * ============================================================================ */

/**
 * @brief Set prediction bias for a hypercolumn
 */
int omni_cc_set_hypercolumn_bias(omni_cortical_columns_bridge_t* bridge,
                                  uint32_t hypercolumn_id,
                                  const float* bias,
                                  uint32_t num_minicolumns);

/**
 * @brief Get prediction bias for a hypercolumn
 */
int omni_cc_get_hypercolumn_bias(const omni_cortical_columns_bridge_t* bridge,
                                  uint32_t hypercolumn_id,
                                  float* bias,
                                  uint32_t* num_minicolumns);

/**
 * @brief Apply bias from prediction to hypercolumn
 */
int omni_cc_apply_prediction_bias(omni_cortical_columns_bridge_t* bridge,
                                   uint32_t hypercolumn_id);

/* ============================================================================
 * Competition API
 * ============================================================================ */

/**
 * @brief Run prediction-biased competition
 */
int omni_cc_run_competition(omni_cortical_columns_bridge_t* bridge,
                             uint32_t hypercolumn_id);

/**
 * @brief Get competition winner(s)
 */
int omni_cc_get_winners(const omni_cortical_columns_bridge_t* bridge,
                         uint32_t hypercolumn_id,
                         uint32_t* winner_indices,
                         float* winner_activations,
                         uint32_t* num_winners);

/**
 * @brief Set competition mode for hypercolumn
 */
int omni_cc_set_competition_mode(omni_cortical_columns_bridge_t* bridge,
                                  uint32_t hypercolumn_id,
                                  omni_cc_competition_mode_t mode);

/* ============================================================================
 * Layer API
 * ============================================================================ */

/**
 * @brief Set layer-specific modulation
 */
int omni_cc_set_layer_modulation(omni_cortical_columns_bridge_t* bridge,
                                  omni_cc_layer_mode_t layer,
                                  float modulation);

/**
 * @brief Get layer-specific prediction error
 */
int omni_cc_get_layer_pe(const omni_cortical_columns_bridge_t* bridge,
                          omni_cc_layer_mode_t layer,
                          float* pe);

/* ============================================================================
 * Query API
 * ============================================================================ */

int omni_cc_get_omni_effects(const omni_cortical_columns_bridge_t* bridge,
                              omni_to_columns_effects_t* effects);

int omni_cc_get_column_effects(const omni_cortical_columns_bridge_t* bridge,
                                columns_to_omni_effects_t* effects);

int omni_cc_get_stats(const omni_cortical_columns_bridge_t* bridge,
                       omni_cc_stats_t* stats);

int omni_cc_reset_stats(omni_cortical_columns_bridge_t* bridge);

/**
 * @brief Get activation sparsity
 */
float omni_cc_get_sparsity(const omni_cortical_columns_bridge_t* bridge);

/**
 * @brief Get competition entropy
 */
float omni_cc_get_entropy(const omni_cortical_columns_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int omni_cc_connect_bio_async(omni_cortical_columns_bridge_t* bridge);
int omni_cc_disconnect_bio_async(omni_cortical_columns_bridge_t* bridge);
bool omni_cc_is_bio_async_connected(const omni_cortical_columns_bridge_t* bridge);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* omni_cc_competition_to_string(omni_cc_competition_mode_t mode);
const char* omni_cc_bias_to_string(omni_cc_bias_mode_t mode);
const char* omni_cc_layer_to_string(omni_cc_layer_mode_t layer);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_CORTICAL_COLUMNS_BRIDGE_H */
