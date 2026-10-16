//=============================================================================
// nimcp_financial_jepa_bridge.h - Financial JEPA Masking Bridge
//=============================================================================
/**
 * @file nimcp_financial_jepa_bridge.h
 * @brief Joint Embedding Predictive Architecture (JEPA) for financial factor prediction
 *
 * WHAT: Implements JEPA-style masked prediction for financial factor analysis.
 *       Given partially observed market factors, predicts the missing (masked) factors
 *       using learned joint embeddings of market relationships.
 *
 * WHY:  Financial markets have complex interdependencies between factors (sentiment,
 *       volatility, volume, price momentum, etc.). JEPA provides a principled approach
 *       to learn these relationships in embedding space, enabling:
 *       - Missing data imputation when some market signals are unavailable
 *       - Cross-modal prediction (e.g., predict price changes from sentiment)
 *       - Robust inference under partial observation
 *       - Self-supervised learning of market factor relationships
 *
 * HOW:  The bridge implements JEPA's core principle:
 *       1. Encode visible (unmasked) factors into a joint embedding space
 *       2. Use a predictor network to predict embeddings of masked factors
 *       3. Decode predicted embeddings back to factor values
 *       4. Learning minimizes embedding-space prediction error (not reconstruction)
 *
 * ARCHITECTURE:
 * ```
 * +------------------------------------------------------------------+
 * |              FINANCIAL JEPA MASKING BRIDGE                        |
 * +------------------------------------------------------------------+
 * |                                                                   |
 * |   +-------------------+         +-------------------+             |
 * |   | VISIBLE FACTORS   |         | MASKED FACTORS    |             |
 * |   | [sentiment,       |         | [price_momentum,  |             |
 * |   |  volatility,      |         |  liquidity]       |             |
 * |   |  volume]          |         |                   |             |
 * |   +--------+----------+         +--------+----------+             |
 * |            |                             ^                        |
 * |            v                             |                        |
 * |   +-------------------+         +-------------------+             |
 * |   | CONTEXT ENCODER   |         | PREDICTOR NETWORK |             |
 * |   | (encodes visible  |-------->| (predicts masked  |             |
 * |   |  to embeddings)   |         |  embeddings)      |             |
 * |   +-------------------+         +--------+----------+             |
 * |                                          |                        |
 * |                                          v                        |
 * |                                 +-------------------+             |
 * |                                 | DECODER           |             |
 * |                                 | (embeddings to    |             |
 * |                                 |  factor values)   |             |
 * |                                 +-------------------+             |
 * |                                                                   |
 * |   CROSS-MODAL PREDICTION:                                         |
 * |   +-------------------+         +-------------------+             |
 * |   | SENTIMENT MODALITY|-------->| PRICE MODALITY    |             |
 * |   | (news, social)    | predict | (returns, vola)   |             |
 * |   +-------------------+         +-------------------+             |
 * |                                                                   |
 * +------------------------------------------------------------------+
 * ```
 *
 * THEORETICAL FOUNDATION:
 * =======================
 * JEPA minimizes prediction error in embedding space:
 *   L = ||f(z_visible) - z_masked||^2
 *
 * Where:
 * - z_visible = encoder(visible_factors)
 * - f(z_visible) = predictor(z_visible) predicts masked embeddings
 * - z_masked = target_encoder(masked_factors) (target network)
 *
 * Benefits over reconstruction-based approaches:
 * - Learns semantic relationships, not surface correlations
 * - More robust to noise in factor values
 * - Can predict across modalities with shared embedding space
 *
 * @author NIMCP Development Team
 * @date 2026-01-29
 */

#ifndef NIMCP_FINANCIAL_JEPA_BRIDGE_H
#define NIMCP_FINANCIAL_JEPA_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Bio-async module identifier for financial JEPA bridge */
#define BIO_MODULE_FINANCIAL_JEPA           0x03A2

/** Maximum number of factors for JEPA prediction */
#define FIN_JEPA_MAX_FACTORS                256

/** Maximum embedding dimension */
#define FIN_JEPA_MAX_EMBED_DIM              128

/** Maximum mask ratio (fraction of factors to mask) */
#define FIN_JEPA_MAX_MASK_RATIO             0.9f

/** Default embedding dimension */
#define FIN_JEPA_DEFAULT_EMBED_DIM          64

//=============================================================================
// Error Codes
//=============================================================================

#define FIN_JEPA_ERROR_BASE                 35100
#define FIN_JEPA_ERR_OK                     0
#define FIN_JEPA_ERR_NULL                   (FIN_JEPA_ERROR_BASE + 1)
#define FIN_JEPA_ERR_INVALID_PARAM          (FIN_JEPA_ERROR_BASE + 2)
#define FIN_JEPA_ERR_NO_MEMORY              (FIN_JEPA_ERROR_BASE + 3)
#define FIN_JEPA_ERR_NOT_INITIALIZED        (FIN_JEPA_ERROR_BASE + 4)
#define FIN_JEPA_ERR_PREDICTION             (FIN_JEPA_ERROR_BASE + 5)
#define FIN_JEPA_ERR_ENCODING               (FIN_JEPA_ERROR_BASE + 6)
#define FIN_JEPA_ERR_MASK                   (FIN_JEPA_ERROR_BASE + 7)
#define FIN_JEPA_ERR_SUBSYSTEM              (FIN_JEPA_ERROR_BASE + 8)
#define FIN_JEPA_ERR_VALIDATION             (FIN_JEPA_ERROR_BASE + 9)
#define FIN_JEPA_ERR_CROSS_MODAL            (FIN_JEPA_ERROR_BASE + 10)

//=============================================================================
// Enumerations
//=============================================================================

/** Bridge operational state */
typedef enum {
    FIN_JEPA_STATE_UNINITIALIZED = 0,
    FIN_JEPA_STATE_IDLE,
    FIN_JEPA_STATE_ENCODING,
    FIN_JEPA_STATE_PREDICTING,
    FIN_JEPA_STATE_DECODING,
    FIN_JEPA_STATE_CROSS_MODAL,
    FIN_JEPA_STATE_ERROR
} fin_jepa_op_state_t;

/** Factor modality types for cross-modal prediction */
typedef enum {
    FIN_MODALITY_PRICE = 0,         /**< Price-based factors (returns, momentum) */
    FIN_MODALITY_VOLUME,            /**< Volume-based factors */
    FIN_MODALITY_VOLATILITY,        /**< Volatility factors (realized, implied) */
    FIN_MODALITY_SENTIMENT,         /**< Sentiment factors (news, social) */
    FIN_MODALITY_FUNDAMENTAL,       /**< Fundamental factors (P/E, book value) */
    FIN_MODALITY_MACRO,             /**< Macroeconomic factors (rates, inflation) */
    FIN_MODALITY_TECHNICAL,         /**< Technical indicators (RSI, MACD) */
    FIN_MODALITY_FLOW,              /**< Fund flows, institutional activity */
    FIN_MODALITY_COUNT
} fin_factor_modality_t;

/** Mask strategy for selecting which factors to mask */
typedef enum {
    FIN_MASK_RANDOM = 0,            /**< Random masking */
    FIN_MASK_BLOCK,                 /**< Block masking (contiguous) */
    FIN_MASK_MODALITY,              /**< Mask entire modality */
    FIN_MASK_IMPORTANCE_WEIGHTED,   /**< Mask based on factor importance */
    FIN_MASK_MISSING_DATA           /**< Mask naturally missing data */
} fin_mask_strategy_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief JEPA input with visible factors and mask
 *
 * Contains the observed (visible) market factors and a mask indicating
 * which factors should be predicted.
 */
typedef struct {
    float* visible_factors;         /**< Observed market factors [num_factors] */
    bool* mask;                     /**< Which factors are masked (true = masked) */
    uint32_t num_factors;           /**< Total number of factors */
} fin_jepa_input_t;

/**
 * @brief JEPA output with predicted factors and confidence
 */
typedef struct {
    float* predicted_factors;       /**< Predicted masked factors [num_predicted] */
    float* confidence;              /**< Prediction confidence for each factor [num_predicted] */
    uint32_t num_predicted;         /**< Number of predicted factors */
} fin_jepa_output_t;

/**
 * @brief Bridge statistics for monitoring and diagnostics
 */
typedef struct {
    uint64_t predictions;           /**< Total predictions made */
    uint64_t cross_modal_predictions; /**< Cross-modal predictions */
    uint64_t immune_checks;         /**< Immune system validations */
    uint64_t bbb_validations;       /**< Blood-brain barrier validations */
    uint64_t kg_messages_sent;      /**< Knowledge graph messages sent */
    uint64_t health_heartbeats;     /**< Health agent heartbeats */
} fin_jepa_bridge_stats_t;

/**
 * @brief Cross-modal prediction input
 *
 * Used for predicting one modality from another (e.g., price from sentiment).
 */
typedef struct {
    float* source_factors;          /**< Source modality factors */
    uint32_t num_source;            /**< Number of source factors */
    fin_factor_modality_t source_modality;  /**< Source modality type */
    fin_factor_modality_t target_modality;  /**< Target modality type */
} fin_cross_modal_input_t;

/**
 * @brief Cross-modal prediction output
 */
typedef struct {
    float* predicted_factors;       /**< Predicted target modality factors */
    float* confidence;              /**< Prediction confidence per factor */
    uint32_t num_predicted;         /**< Number of predicted factors */
    float cross_modal_coherence;    /**< Coherence score between modalities [0-1] */
} fin_cross_modal_output_t;

/**
 * @brief Embedding state for internal representation
 */
typedef struct {
    float* embeddings;              /**< Current factor embeddings [num_factors * embed_dim] */
    uint32_t num_factors;           /**< Number of factors */
    uint32_t embed_dim;             /**< Embedding dimension */
} fin_jepa_embedding_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Bridge configuration structure
 */
typedef struct {
    /* Model settings */
    uint32_t num_factors;           /**< Number of factors to track */
    uint32_t embed_dim;             /**< Embedding dimension */

    /* Masking settings */
    fin_mask_strategy_t mask_strategy; /**< Default mask strategy */
    float mask_ratio;               /**< Default ratio of factors to mask [0-0.9] */

    /* Predictor settings */
    uint32_t predictor_hidden_dim;  /**< Predictor hidden layer dimension */
    float predictor_dropout;        /**< Dropout rate for predictor */

    /* EMA settings for target encoder */
    float target_ema_decay;         /**< EMA decay for target encoder (0.99 typical) */

    /* Confidence settings */
    float min_confidence;           /**< Minimum confidence threshold */
    float confidence_decay;         /**< Confidence decay for distant predictions */

    /* Modulation sensitivity */
    float inflammation_sensitivity; /**< Sensitivity to inflammation [0-2] */
    float fatigue_sensitivity;      /**< Sensitivity to fatigue [0-2] */

    /* Security settings */
    bool enable_bbb_validation;     /**< Enable blood-brain barrier checks */
    bool enable_immune_validation;  /**< Enable immune system checks */
} fin_jepa_config_t;

//=============================================================================
// Opaque Handle
//=============================================================================

/** Opaque bridge handle */
typedef struct financial_jepa_bridge financial_jepa_bridge_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default configuration
 * @return Default configuration with sensible values
 */
fin_jepa_config_t financial_jepa_bridge_default_config(void);

/**
 * @brief Create financial JEPA bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
financial_jepa_bridge_t* financial_jepa_bridge_create(
    const fin_jepa_config_t* config);

/**
 * @brief Destroy bridge and free resources
 * @param bridge Bridge handle
 */
void financial_jepa_bridge_destroy(financial_jepa_bridge_t* bridge);

/**
 * @brief Get current bridge state
 * @param bridge Bridge handle
 * @return Current operational state
 */
fin_jepa_op_state_t financial_jepa_bridge_get_state(
    const financial_jepa_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_jepa_bridge_reset(financial_jepa_bridge_t* bridge);

//=============================================================================
// Subsystem Setters
//=============================================================================

/**
 * @brief Set immune system for validation
 * @param bridge Bridge handle
 * @param immune Immune system handle (NULL to disable)
 * @return 0 on success
 */
int financial_jepa_bridge_set_immune(financial_jepa_bridge_t* bridge,
                                      void* immune);

/**
 * @brief Set blood-brain barrier for data validation
 * @param bridge Bridge handle
 * @param bbb BBB handle (NULL to disable)
 * @return 0 on success
 */
int financial_jepa_bridge_set_bbb(financial_jepa_bridge_t* bridge,
                                   void* bbb);

/**
 * @brief Enable/disable BBB validation
 * @param bridge Bridge handle
 * @param enable True to enable validation
 * @return 0 on success
 */
int financial_jepa_bridge_enable_bbb_validation(
    financial_jepa_bridge_t* bridge, bool enable);

/**
 * @brief Enable/disable immune validation
 * @param bridge Bridge handle
 * @param enable True to enable validation
 * @return 0 on success
 */
int financial_jepa_bridge_enable_immune_validation(
    financial_jepa_bridge_t* bridge, bool enable);

/**
 * @brief Set KG wiring for inter-module communication
 * @param bridge Bridge handle
 * @param kg KG wiring handle
 * @return 0 on success
 */
int financial_jepa_bridge_set_kg_wiring(financial_jepa_bridge_t* bridge,
                                         void* kg);

/**
 * @brief Set health agent for heartbeat monitoring
 * @param bridge Bridge handle
 * @param health_agent Health agent handle
 * @return 0 on success
 */
int financial_jepa_bridge_set_health_agent(financial_jepa_bridge_t* bridge,
                                            void* health_agent);

/**
 * @brief Set logger for debug/trace output
 * @param bridge Bridge handle
 * @param logger Logger handle
 * @return 0 on success
 */
int financial_jepa_bridge_set_logger(financial_jepa_bridge_t* bridge,
                                      void* logger);

//=============================================================================
// Core JEPA API
//=============================================================================

/**
 * @brief Predict missing (masked) factors using JEPA
 *
 * Takes partially observed market factors with a mask indicating which
 * factors are missing/masked, and predicts the masked factor values
 * using the learned JEPA model.
 *
 * @param bridge Bridge handle
 * @param input Input with visible factors and mask
 * @param output Output with predicted factors (caller allocates)
 * @return 0 on success, error code on failure
 *
 * Example:
 * ```c
 * fin_jepa_input_t input = {
 *     .visible_factors = factors,
 *     .mask = mask,  // true for factors to predict
 *     .num_factors = 10
 * };
 * fin_jepa_output_t output = {
 *     .predicted_factors = nimcp_malloc(10 * sizeof(float)),
 *     .confidence = nimcp_malloc(10 * sizeof(float)),
 *     .num_predicted = 0
 * };
 * int rc = financial_jepa_bridge_predict_missing(bridge, &input, &output);
 * ```
 */
int financial_jepa_bridge_predict_missing(
    financial_jepa_bridge_t* bridge,
    const fin_jepa_input_t* input,
    fin_jepa_output_t* output);

/**
 * @brief Predict one modality from another (cross-modal prediction)
 *
 * Predicts target modality factors from source modality factors.
 * For example, predict price momentum from sentiment indicators.
 *
 * @param bridge Bridge handle
 * @param input Cross-modal input with source factors
 * @param output Cross-modal output with predictions (caller allocates)
 * @return 0 on success, error code on failure
 *
 * Example:
 * ```c
 * fin_cross_modal_input_t input = {
 *     .source_factors = sentiment_data,
 *     .num_source = 5,
 *     .source_modality = FIN_MODALITY_SENTIMENT,
 *     .target_modality = FIN_MODALITY_PRICE
 * };
 * fin_cross_modal_output_t output = {
 *     .predicted_factors = nimcp_malloc(10 * sizeof(float)),
 *     .confidence = nimcp_malloc(10 * sizeof(float)),
 *     .num_predicted = 0
 * };
 * int rc = financial_jepa_bridge_cross_modal_predict(bridge, &input, &output);
 * ```
 */
int financial_jepa_bridge_cross_modal_predict(
    financial_jepa_bridge_t* bridge,
    const fin_cross_modal_input_t* input,
    fin_cross_modal_output_t* output);

//=============================================================================
// Embedding API
//=============================================================================

/**
 * @brief Get current embeddings for factors
 * @param bridge Bridge handle
 * @param embeddings Output embedding state (caller allocates internal arrays)
 * @return 0 on success, error code on failure
 */
int financial_jepa_bridge_get_embeddings(
    const financial_jepa_bridge_t* bridge,
    fin_jepa_embedding_t* embeddings);

/**
 * @brief Compute similarity between factor embeddings
 * @param bridge Bridge handle
 * @param factor_a Index of first factor
 * @param factor_b Index of second factor
 * @param similarity Output similarity score [-1, 1]
 * @return 0 on success, error code on failure
 */
int financial_jepa_bridge_embedding_similarity(
    const financial_jepa_bridge_t* bridge,
    uint32_t factor_a,
    uint32_t factor_b,
    float* similarity);

//=============================================================================
// Mask Generation API
//=============================================================================

/**
 * @brief Generate a mask based on configured strategy
 * @param bridge Bridge handle
 * @param num_factors Number of factors
 * @param mask Output mask array (caller allocates, size num_factors)
 * @param num_masked Output number of masked factors
 * @return 0 on success, error code on failure
 */
int financial_jepa_bridge_generate_mask(
    financial_jepa_bridge_t* bridge,
    uint32_t num_factors,
    bool* mask,
    uint32_t* num_masked);

/**
 * @brief Generate a modality-specific mask
 * @param bridge Bridge handle
 * @param modality Modality to mask
 * @param num_factors Number of factors
 * @param factor_modalities Array indicating modality of each factor
 * @param mask Output mask array (caller allocates)
 * @param num_masked Output number of masked factors
 * @return 0 on success, error code on failure
 */
int financial_jepa_bridge_generate_modality_mask(
    financial_jepa_bridge_t* bridge,
    fin_factor_modality_t modality,
    uint32_t num_factors,
    const fin_factor_modality_t* factor_modalities,
    bool* mask,
    uint32_t* num_masked);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Allocate JEPA input structure
 * @param num_factors Number of factors
 * @return Allocated input or NULL on error
 */
fin_jepa_input_t* financial_jepa_input_create(uint32_t num_factors);

/**
 * @brief Free JEPA input structure
 * @param input Input to free
 */
void financial_jepa_input_destroy(fin_jepa_input_t* input);

/**
 * @brief Allocate JEPA output structure
 * @param num_factors Maximum number of factors
 * @return Allocated output or NULL on error
 */
fin_jepa_output_t* financial_jepa_output_create(uint32_t num_factors);

/**
 * @brief Free JEPA output structure
 * @param output Output to free
 */
void financial_jepa_output_destroy(fin_jepa_output_t* output);

/**
 * @brief Allocate cross-modal output structure
 * @param num_factors Maximum number of target factors
 * @return Allocated output or NULL on error
 */
fin_cross_modal_output_t* financial_jepa_cross_modal_output_create(uint32_t num_factors);

/**
 * @brief Free cross-modal output structure
 * @param output Output to free
 */
void financial_jepa_cross_modal_output_destroy(fin_cross_modal_output_t* output);

//=============================================================================
// Modulation API
//=============================================================================

/**
 * @brief Set inflammation level (reduces prediction confidence)
 * @param bridge Bridge handle
 * @param level Inflammation level [0-1]
 * @return 0 on success
 */
int financial_jepa_bridge_set_inflammation(
    financial_jepa_bridge_t* bridge, float level);

/**
 * @brief Set fatigue level (reduces prediction confidence)
 * @param bridge Bridge handle
 * @param level Fatigue level [0-1]
 * @return 0 on success
 */
int financial_jepa_bridge_set_fatigue(
    financial_jepa_bridge_t* bridge, float level);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics (caller allocates)
 * @return 0 on success
 */
int financial_jepa_bridge_get_stats(
    const financial_jepa_bridge_t* bridge,
    fin_jepa_bridge_stats_t* stats);

/**
 * @brief Reset statistics counters
 * @param bridge Bridge handle
 */
void financial_jepa_bridge_reset_stats(financial_jepa_bridge_t* bridge);

/**
 * @brief Get last error message
 * @return Thread-local error message string
 */
const char* financial_jepa_bridge_get_last_error(void);

//=============================================================================
// Global Subsystem Setters (for integration)
//=============================================================================

/**
 * @brief Set global health agent for all JEPA bridge instances
 * @param agent Health agent handle
 */
void financial_jepa_bridge_set_global_health_agent(void* agent);

/**
 * @brief Set global immune system for all JEPA bridge instances
 * @param immune Immune system handle
 */
void financial_jepa_bridge_set_global_immune(void* immune);

/**
 * @brief Set global BBB for all JEPA bridge instances
 * @param bbb BBB handle
 */
void financial_jepa_bridge_set_global_bbb(void* bbb);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_JEPA_BRIDGE_H */
