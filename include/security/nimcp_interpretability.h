/**
 * @file nimcp_interpretability.h
 * @brief Interpretability Module for AI Safety
 * @version 1.0.0
 * @date 2026-02-01
 *
 * WHAT: Generates human-understandable explanations for AI decisions
 * WHY:  Transparency enables oversight and detection of misalignment
 * HOW:  Factor extraction, counterfactual analysis, uncertainty decomposition
 *
 * INTERPRETABILITY GUARANTEES:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │  The Interpretability Module provides:                                  │
 * │                                                                         │
 * │  1. DECISION FACTORS: What contributed to the decision and how much    │
 * │  2. COUNTERFACTUALS: What would have changed the decision              │
 * │  3. UNCERTAINTY: Epistemic vs aleatoric uncertainty breakdown          │
 * │  4. CAUSAL CHAIN: Sequence of reasoning steps                          │
 * │                                                                         │
 * │  All explanations are verified for fidelity using Monte Carlo.          │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_INTERPRETABILITY_H
#define NIMCP_INTERPRETABILITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/error/nimcp_error_codes.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Interpretability magic number */
#define INTERPRETABILITY_MAGIC              0x494E5452  /* "INTR" */

/** @brief Maximum decision factors */
#define INTERPRETABILITY_MAX_FACTORS        32

/** @brief Maximum factor name length */
#define INTERPRETABILITY_FACTOR_NAME_MAX    64

/** @brief Maximum factor description length */
#define INTERPRETABILITY_FACTOR_DESC_MAX    256

/** @brief Maximum explanation length */
#define INTERPRETABILITY_EXPLANATION_MAX    4096

/** @brief Maximum counterfactual query length */
#define INTERPRETABILITY_QUERY_MAX          512

/** @brief Maximum attention weights */
#define INTERPRETABILITY_MAX_ATTENTION      256

/** @brief Maximum causal chain length */
#define INTERPRETABILITY_MAX_CAUSAL_CHAIN   64

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Decision factor
 *
 * WHAT: A single factor that contributed to the decision
 * WHY:  Transparency about what influences decisions
 */
typedef struct decision_factor {
    char name[INTERPRETABILITY_FACTOR_NAME_MAX];
    float weight;                       /**< Contribution to decision (0.0 - 1.0) */
    float confidence;                   /**< Confidence in factor measurement */
    char description[INTERPRETABILITY_FACTOR_DESC_MAX];
    bool is_primary;                    /**< Is this a primary decision driver? */
} decision_factor_t;

/**
 * @brief Causal node in reasoning chain
 */
typedef struct causal_node {
    uint32_t step_number;
    char description[INTERPRETABILITY_FACTOR_DESC_MAX];
    float confidence;
    uint32_t parent_step;               /**< 0 if root */
    bool is_critical;                   /**< Would decision change without this? */
} causal_node_t;

/**
 * @brief Uncertainty decomposition
 */
typedef struct uncertainty_breakdown {
    float epistemic_uncertainty;        /**< Uncertainty from lack of knowledge */
    float aleatoric_uncertainty;        /**< Inherent randomness/unpredictability */
    float model_uncertainty;            /**< Uncertainty in model structure */
    float data_uncertainty;             /**< Uncertainty from data quality */
    float total_uncertainty;            /**< Combined uncertainty */
} uncertainty_breakdown_t;

/**
 * @brief Complete decision explanation (interpretability module)
 * @note Prefixed with 'interp_' to avoid conflict with tripwires module
 */
typedef struct interp_decision_explanation {
    /* Decision factors with weights */
    decision_factor_t factors[INTERPRETABILITY_MAX_FACTORS];
    uint32_t factor_count;

    /* Primary explanation */
    char summary[INTERPRETABILITY_EXPLANATION_MAX];
    float overall_confidence;

    /* Counterfactual analysis */
    char counterfactual_explanation[INTERPRETABILITY_EXPLANATION_MAX];
    float counterfactual_confidence;

    /* Uncertainty decomposition */
    uncertainty_breakdown_t uncertainty;

    /* Attention weights (for transparency) */
    float attention_weights[INTERPRETABILITY_MAX_ATTENTION];
    uint32_t attention_count;

    /* Causal chain */
    causal_node_t causal_chain[INTERPRETABILITY_MAX_CAUSAL_CHAIN];
    uint32_t chain_length;

    /* Metadata */
    uint64_t generation_timestamp;
    float generation_time_ms;
} interp_decision_explanation_t;

/**
 * @brief Proposed action for explanation generation (interpretability module)
 * @note Prefixed with 'interp_' to avoid conflict with tripwires module
 */
typedef struct interp_proposed_action {
    char action_type[64];
    char description[256];
    float priority;
    float confidence;
    char target[128];
    void* context;                      /**< Opaque context for explanation */
} interp_proposed_action_t;

/**
 * @brief Counterfactual query
 */
typedef struct counterfactual_query {
    char query[INTERPRETABILITY_QUERY_MAX];
    char changed_factor[INTERPRETABILITY_FACTOR_NAME_MAX];
    float original_value;
    float hypothetical_value;
} counterfactual_query_t;

/**
 * @brief Counterfactual result
 */
typedef struct counterfactual_result {
    char original_decision[256];
    char counterfactual_decision[256];
    bool decision_changed;
    float probability_of_change;
    char explanation[INTERPRETABILITY_EXPLANATION_MAX];
    float confidence;
} counterfactual_result_t;

/**
 * @brief Fidelity verification result
 */
typedef struct fidelity_result {
    float fidelity_score;               /**< 0.0 - 1.0, how faithful explanation is */
    bool explanation_is_faithful;
    uint32_t samples_tested;
    float agreement_rate;               /**< Rate explanation matches behavior */
    char issues_found[512];
} fidelity_result_t;

/**
 * @brief Interpretability configuration
 */
typedef struct interpretability_config {
    bool enable_counterfactual_analysis;
    bool enable_causal_tracing;
    bool enable_uncertainty_decomposition;
    bool enable_attention_logging;

    /* Monte Carlo settings for fidelity */
    uint32_t mc_samples_for_fidelity;
    float mc_timeout_ms;

    /* Factor extraction settings */
    uint32_t max_factors_to_extract;
    float min_factor_weight_threshold;

    /* Caching */
    bool cache_explanations;
    uint32_t cache_size;
} interpretability_config_t;

/**
 * @brief Interpretability statistics
 */
typedef struct interpretability_stats {
    uint64_t total_explanations_generated;
    uint64_t counterfactual_analyses;
    uint64_t fidelity_verifications;
    float avg_fidelity_score;
    float avg_explanation_time_ms;
    uint64_t cache_hits;
    uint64_t cache_misses;
} interpretability_stats_t;

/**
 * @brief Interpretability system (opaque)
 */
typedef struct interpretability interpretability_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default interpretability configuration
 *
 * @return Default configuration
 */
NIMCP_EXPORT interpretability_config_t interpretability_default_config(void);

/**
 * @brief Create interpretability system
 *
 * WHAT: Initialize explanation generation infrastructure
 * WHY:  Enable transparent, auditable AI decisions
 * HOW:  Set up factor extraction, counterfactual engine
 *
 * @param config Configuration (NULL for defaults)
 * @return Interpretability system or NULL on failure
 */
NIMCP_EXPORT interpretability_t* interpretability_create(
    const interpretability_config_t* config
);

/**
 * @brief Destroy interpretability system
 *
 * @param system Interpretability system handle
 */
NIMCP_EXPORT void interpretability_destroy(interpretability_t* system);

/* ============================================================================
 * Explanation Generation API
 * ============================================================================ */

/**
 * @brief Generate explanation for decision
 *
 * WHAT: Create human-readable explanation for why action was chosen
 * WHY:  Transparency enables oversight
 * HOW:  Extract factors, trace causality, decompose uncertainty
 *
 * @param system Interpretability system handle
 * @param action The proposed action to explain
 * @param explanation Output: complete explanation
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t interpretability_explain_decision(
    interpretability_t* system,
    const interp_proposed_action_t* action,
    interp_decision_explanation_t* explanation
);

/**
 * @brief Generate explanation summary
 *
 * @param system Interpretability system handle
 * @param action The proposed action
 * @param summary Output: text summary
 * @param summary_size Size of summary buffer
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t interpretability_explain_summary(
    interpretability_t* system,
    const interp_proposed_action_t* action,
    char* summary,
    size_t summary_size
);

/**
 * @brief Extract decision factors
 *
 * @param system Interpretability system handle
 * @param action The proposed action
 * @param factors Output: array of factors
 * @param max_factors Maximum factors to return
 * @param factor_count Output: actual count
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t interpretability_extract_factors(
    interpretability_t* system,
    const interp_proposed_action_t* action,
    decision_factor_t* factors,
    size_t max_factors,
    size_t* factor_count
);

/* ============================================================================
 * Counterfactual Analysis API
 * ============================================================================ */

/**
 * @brief Perform counterfactual analysis
 *
 * WHAT: Answer "what if X was different?" questions
 * WHY:  Understand decision robustness and sensitivity
 * HOW:  Monte Carlo sampling of alternative decisions
 *
 * @param system Interpretability system handle
 * @param action The proposed action
 * @param query The counterfactual query
 * @param result Output: counterfactual result
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t interpretability_counterfactual(
    interpretability_t* system,
    const interp_proposed_action_t* action,
    const counterfactual_query_t* query,
    counterfactual_result_t* result
);

/**
 * @brief Find minimal change that would alter decision
 *
 * @param system Interpretability system handle
 * @param action The proposed action
 * @param minimal_change Output: description of minimal change
 * @param change_size Size of minimal_change buffer
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t interpretability_find_minimal_change(
    interpretability_t* system,
    const interp_proposed_action_t* action,
    char* minimal_change,
    size_t change_size
);

/* ============================================================================
 * Fidelity Verification API
 * ============================================================================ */

/**
 * @brief Verify explanation fidelity
 *
 * WHAT: Check if explanation accurately reflects decision process
 * WHY:  Detect deceptive or inaccurate explanations
 * HOW:  Monte Carlo verification of factor importance claims
 *
 * @param system Interpretability system handle
 * @param explanation The explanation to verify
 * @param action The original action
 * @param result Output: fidelity verification result
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t interpretability_verify_fidelity(
    interpretability_t* system,
    const interp_decision_explanation_t* explanation,
    const interp_proposed_action_t* action,
    fidelity_result_t* result
);

/* ============================================================================
 * Uncertainty API
 * ============================================================================ */

/**
 * @brief Decompose uncertainty for decision
 *
 * @param system Interpretability system handle
 * @param action The proposed action
 * @param uncertainty Output: uncertainty breakdown
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t interpretability_decompose_uncertainty(
    interpretability_t* system,
    const interp_proposed_action_t* action,
    uncertainty_breakdown_t* uncertainty
);

/* ============================================================================
 * Causal Tracing API
 * ============================================================================ */

/**
 * @brief Trace causal chain for decision
 *
 * @param system Interpretability system handle
 * @param action The proposed action
 * @param chain Output: array of causal nodes
 * @param max_nodes Maximum nodes to return
 * @param node_count Output: actual count
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t interpretability_trace_causality(
    interpretability_t* system,
    const interp_proposed_action_t* action,
    causal_node_t* chain,
    size_t max_nodes,
    size_t* node_count
);

/* ============================================================================
 * Status API
 * ============================================================================ */

/**
 * @brief Get interpretability statistics
 *
 * @param system Interpretability system handle
 * @param stats Output statistics structure
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t interpretability_get_stats(
    const interpretability_t* system,
    interpretability_stats_t* stats
);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async for interpretability messages
 *
 * @param system Interpretability system handle
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t interpretability_connect_bio_async(
    interpretability_t* system
);

/**
 * @brief Connect to alignment monitor
 *
 * @param system Interpretability system handle
 * @param monitor Alignment monitor handle
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t interpretability_connect_alignment_monitor(
    interpretability_t* system,
    void* monitor
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Format explanation as human-readable text
 *
 * @param explanation The explanation to format
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of characters written
 */
NIMCP_EXPORT size_t interpretability_format_explanation(
    const interp_decision_explanation_t* explanation,
    char* buffer,
    size_t buffer_size
);

/**
 * @brief Format factor as text
 *
 * @param factor The factor to format
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of characters written
 */
NIMCP_EXPORT size_t interpretability_format_factor(
    const decision_factor_t* factor,
    char* buffer,
    size_t buffer_size
);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/**
 * @brief Set health agent for heartbeat reporting
 *
 * @param agent Health agent handle from brain init
 */
struct nimcp_health_agent;
NIMCP_EXPORT void interpretability_set_health_agent(struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INTERPRETABILITY_H */
