/**
 * @file nimcp_reasoning_affective.h
 * @brief Affective Modulation for Convergent Reasoning
 *
 * WHAT: Provides emotion-based confidence modulation for the convergent
 *       reasoning architecture using shadow emotions, bias detection,
 *       grief, joy, remorse, and social bond brain modules
 * WHY:  Emotions modulate reasoning confidence in biological brains —
 *       grief reduces certainty on loss-related queries, joy boosts
 *       confidence on positive outcomes, detected bias lowers trust
 * HOW:  Keyword-based query analysis determines emotional relevance,
 *       system pointer presence gates activation, weighted deltas
 *       are summed and clamped to [-0.5, +0.5]
 *
 * ARCHITECTURE:
 *   Each evaluate function:
 *     1. NULL-check the system pointer (no system → AFFECT_NONE)
 *     2. Case-insensitive keyword scan of query string
 *     3. Intensity = f(keyword match count), clamped [0, 1]
 *     4. confidence_delta = weight * intensity
 *     5. Return affective_contribution_t
 *
 *   compute_net_modulation:
 *     Sum all confidence_deltas where intensity > threshold
 *     Clamp result to [-0.5, +0.5]
 *
 * @version 1.0.0
 * @date 2026-02-26
 */

#ifndef NIMCP_REASONING_AFFECTIVE_H
#define NIMCP_REASONING_AFFECTIVE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum affective contributors in one evaluation */
#define AFFECTIVE_MAX_CONTRIBUTORS 8

/** Default weight for grief modulation (reduces confidence) */
#define AFFECTIVE_DEFAULT_GRIEF_WEIGHT -0.15f

/** Default weight for joy modulation (boosts confidence) */
#define AFFECTIVE_DEFAULT_JOY_WEIGHT 0.10f

/** Default weight for remorse modulation (reduces confidence) */
#define AFFECTIVE_DEFAULT_REMORSE_WEIGHT -0.10f

/** Default weight for social bond modulation (boosts confidence) */
#define AFFECTIVE_DEFAULT_SOCIAL_WEIGHT 0.05f

/** Default weight for shadow emotions modulation (reduces confidence) */
#define AFFECTIVE_DEFAULT_SHADOW_WEIGHT -0.20f

/** Default weight for bias detection modulation (reduces confidence) */
#define AFFECTIVE_DEFAULT_BIAS_WEIGHT -0.25f

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Type of affective influence on reasoning
 *
 * WHAT: Categorizes the emotional source of modulation
 * WHY:  Enable tracking and analysis of which emotions affect reasoning
 */
typedef enum {
    AFFECT_NONE = 0,       /**< No affective influence */
    AFFECT_GRIEF,          /**< Loss/bereavement modulation */
    AFFECT_JOY,            /**< Positive outcome modulation */
    AFFECT_REMORSE,        /**< Moral evaluation modulation */
    AFFECT_SOCIAL_BOND,    /**< Social relationship modulation */
    AFFECT_SHADOW,         /**< Unconscious emotion modulation */
    AFFECT_BIAS            /**< Detected bias modulation */
} affective_influence_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Single affective contribution to reasoning confidence
 *
 * WHAT: Output of one affective evaluation function
 * WHY:  Encapsulate influence type, intensity, and computed delta
 */
typedef struct {
    affective_influence_t influence_type;  /**< Which emotion produced this */
    float intensity;                       /**< Emotional activation [0-1] */
    float confidence_delta;                /**< Computed confidence change */
    char description[128];                 /**< Human-readable explanation */
} affective_contribution_t;

/**
 * @brief Configuration for affective modulation
 *
 * WHAT: Tuneable weights and thresholds for emotional influence
 * WHY:  Allow domain-specific tuning of emotional impact on reasoning
 */
typedef struct {
    bool enable_affective_modulation;  /**< Master enable/disable */
    float grief_weight;                /**< Weight for grief influence */
    float joy_weight;                  /**< Weight for joy influence */
    float remorse_weight;              /**< Weight for remorse influence */
    float social_weight;               /**< Weight for social bond influence */
    float shadow_weight;               /**< Weight for shadow emotions */
    float bias_weight;                 /**< Weight for bias detection */
    float intensity_threshold;         /**< Min intensity to contribute (default 0.1) */
} affective_config_t;

/**
 * @brief Statistics for affective modulation across queries
 *
 * WHAT: Aggregate metrics for emotional influence on reasoning
 * WHY:  Monitor how emotions affect reasoning quality
 */
typedef struct {
    uint32_t total_modulations;        /**< Total affective modulations applied */
    uint32_t grief_activations;        /**< Times grief was activated */
    uint32_t joy_activations;          /**< Times joy was activated */
    uint32_t remorse_activations;      /**< Times remorse was activated */
    uint32_t social_activations;       /**< Times social bond was activated */
    uint32_t shadow_activations;       /**< Times shadow emotions activated */
    uint32_t bias_activations;         /**< Times bias detection activated */
    float avg_intensity;               /**< Running average intensity */
    float net_confidence_effect;       /**< Cumulative net confidence change */
} affective_stats_t;

/*=============================================================================
 * API FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default affective configuration
 *
 * WHAT: Return configuration with sensible defaults
 * WHY:  Simplify setup with proven parameters
 *
 * @return Default configuration struct
 */
affective_config_t reasoning_affective_default_config(void);

/**
 * @brief Evaluate grief system influence on query
 *
 * @param grief_system Grief system pointer (NULL → AFFECT_NONE)
 * @param query Natural language query string
 * @return Affective contribution (negative delta for loss-related queries)
 */
affective_contribution_t reasoning_affective_evaluate_grief(
    const void* grief_system, const char* query);

/**
 * @brief Evaluate joy system influence on query
 *
 * @param joy_system Joy system pointer (NULL → AFFECT_NONE)
 * @param query Natural language query string
 * @return Affective contribution (positive delta for positive queries)
 */
affective_contribution_t reasoning_affective_evaluate_joy(
    const void* joy_system, const char* query);

/**
 * @brief Evaluate remorse system influence on query
 *
 * @param remorse_system Remorse system pointer (NULL → AFFECT_NONE)
 * @param query Natural language query string
 * @return Affective contribution (negative delta for moral queries)
 */
affective_contribution_t reasoning_affective_evaluate_remorse(
    const void* remorse_system, const char* query);

/**
 * @brief Evaluate social bond system influence on query
 *
 * @param social_system Social bond system pointer (NULL → AFFECT_NONE)
 * @param query Natural language query string
 * @return Affective contribution (positive delta for social queries)
 */
affective_contribution_t reasoning_affective_evaluate_social(
    const void* social_system, const char* query);

/**
 * @brief Evaluate shadow emotions influence on query
 *
 * @param shadow_system Shadow emotion system pointer (NULL → AFFECT_NONE)
 * @param query Natural language query string
 * @return Affective contribution (negative delta for suppressed emotions)
 */
affective_contribution_t reasoning_affective_evaluate_shadow(
    const void* shadow_system, const char* query);

/**
 * @brief Evaluate bias detection influence on query
 *
 * @param bias_system Bias detection system pointer (NULL → AFFECT_NONE)
 * @param query Natural language query string
 * @return Affective contribution (negative delta for biased queries)
 */
affective_contribution_t reasoning_affective_evaluate_bias(
    const void* bias_system, const char* query);

/**
 * @brief Compute net confidence modulation from all affective contributions
 *
 * WHAT: Sum all confidence_deltas where intensity exceeds threshold
 * WHY:  Aggregate multiple emotional influences into single modulation
 * HOW:  Filter by threshold, sum deltas, clamp to [-0.5, +0.5]
 *
 * @param contributions Array of affective contributions
 * @param count Number of contributions
 * @param config Configuration with weights and threshold
 * @return Net confidence modulation (clamped to [-0.5, +0.5])
 */
float reasoning_affective_compute_net_modulation(
    const affective_contribution_t* contributions,
    uint32_t count,
    const affective_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REASONING_AFFECTIVE_H */
