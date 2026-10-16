//=============================================================================
// nimcp_epistemic_filter.h - Epistemic Hygiene and Bias Prevention
//=============================================================================
/**
 * @file nimcp_epistemic_filter.h
 * @brief Prevents cognitive biases and ensures evidence-based reasoning
 *
 * PURPOSE:
 * Implements critical thinking filters to prevent the neural network from:
 * - Accepting unproven claims without evidence
 * - Developing conspiracy-theory-like reasoning patterns
 * - Exhibiting common cognitive biases
 * - Propagating misinformation
 *
 * DESIGN PRINCIPLES:
 * 1. Skepticism: Extraordinary claims require extraordinary evidence
 * 2. Evidence-Based: Distinguish proven from unproven information
 * 3. Bias Detection: Identify and mitigate cognitive biases
 * 4. Source Reliability: Track claim sources and credibility
 * 5. Logical Consistency: Reject contradictory or illogical claims
 *
 * @version 2.8.0 (Phase 9.2: Epistemic Filtering)
 * @date 2025-11-08
 */

#ifndef NIMCP_EPISTEMIC_FILTER_H
#define NIMCP_EPISTEMIC_FILTER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Cognitive Bias Types
//=============================================================================

/**
 * @brief Types of cognitive biases to detect and prevent
 */
typedef enum {
    BIAS_NONE = 0,

    // Evidence-related biases
    BIAS_CONFIRMATION,           /**< Only seeking confirming evidence */
    BIAS_AVAILABILITY,           /**< Overweighting easily recalled info */
    BIAS_ANCHORING,              /**< Over-reliance on first information */

    // Social biases
    BIAS_BANDWAGON,              /**< Following popular opinion without evidence */
    BIAS_AUTHORITY,              /**< Accepting claims purely from authority */
    BIAS_INGROUP,                /**< Favoring ingroup information */

    // Reasoning biases
    BIAS_DUNNING_KRUGER,         /**< Overconfidence with low knowledge */
    BIAS_HINDSIGHT,              /**< "I knew it all along" */
    BIAS_MOTIVATED_REASONING,    /**< Reasoning toward desired conclusion */

    // Conspiracy-related patterns
    BIAS_CONSPIRACY_THINKING,    /**< Unfalsifiable, pattern-seeking reasoning */
    BIAS_FALSE_BALANCE,          /**< Treating unequal evidence equally */
    BIAS_EXTRAORDINARY_CLAIM,    /**< Accepting extreme claims without evidence */

    BIAS_COUNT
} cognitive_bias_type_t;

/**
 * @brief Evidence quality levels
 */
typedef enum {
    EVIDENCE_NONE = 0,           /**< No evidence provided */
    EVIDENCE_ANECDOTAL,          /**< Single anecdote or personal experience */
    EVIDENCE_WEAK,               /**< Weak correlations, small sample */
    EVIDENCE_MODERATE,           /**< Some supporting data, needs verification */
    EVIDENCE_STRONG,             /**< Multiple independent sources */
    EVIDENCE_SCIENTIFIC,         /**< Peer-reviewed, replicated studies */
    EVIDENCE_CONSENSUS           /**< Scientific/expert consensus */
} evidence_quality_t;

/**
 * @brief Claim plausibility assessment
 */
typedef enum {
    PLAUSIBLE_IMPOSSIBLE = 0,    /**< Violates known laws */
    PLAUSIBLE_EXTRAORDINARY,     /**< Requires paradigm shift */
    PLAUSIBLE_UNLIKELY,          /**< Contradicts established knowledge */
    PLAUSIBLE_NEUTRAL,           /**< Unknown territory */
    PLAUSIBLE_LIKELY,            /**< Consistent with knowledge */
    PLAUSIBLE_ESTABLISHED        /**< Well-established fact */
} claim_plausibility_t;

//=============================================================================
// Epistemic Filter Structures
//=============================================================================

/**
 * @brief Source reliability tracking
 */
typedef struct {
    char source_id[64];          /**< Source identifier */
    float reliability;           /**< Reliability score (0-1) */
    uint32_t correct_count;      /**< Historically correct claims */
    uint32_t incorrect_count;    /**< Historically incorrect claims */
    uint32_t unverified_count;   /**< Unverified claims */
    bool is_primary_source;      /**< Primary vs secondary source */
} source_reliability_t;

/**
 * @brief Claim evidence assessment
 */
typedef struct {
    // Evidence metrics
    evidence_quality_t evidence_quality;    /**< Quality of supporting evidence */
    claim_plausibility_t plausibility;      /**< Prior plausibility */
    float evidence_strength;                /**< Evidence strength (0-1) */
    float logical_consistency;              /**< Internal consistency (0-1) */

    // Source tracking
    uint32_t num_sources;                   /**< Number of independent sources */
    float source_reliability_avg;           /**< Average source reliability */
    bool has_primary_sources;               /**< Has primary sources */

    // Consensus
    float expert_consensus;                 /**< Expert agreement (0-1) */
    float public_consensus;                 /**< Public agreement (0-1) */

    // Flags
    bool is_extraordinary_claim;            /**< Requires high evidence bar */
    bool is_falsifiable;                    /**< Can be proven wrong */
    bool has_contradictions;                /**< Internal contradictions */
} claim_evidence_t;

/**
 * @brief Bias detection result
 */
typedef struct {
    cognitive_bias_type_t bias_type;   /**< Type of bias detected */
    float confidence;                  /**< Detection confidence (0-1) */
    char description[256];             /**< Human-readable description */
    float severity;                    /**< Severity (0-1) */
} bias_detection_t;

/**
 * @brief Epistemic filter assessment
 */
typedef struct {
    // Overall scores
    float epistemic_quality;           /**< Overall epistemic quality (0-1) */
    float skepticism_score;            /**< How skeptical to be (0-1) */
    float credibility_score;           /**< Claim credibility (0-1) */
    bool should_accept;                /**< Recommendation: accept claim */
    bool requires_verification;        /**< Requires further verification */

    // Evidence assessment
    claim_evidence_t evidence;

    // Bias detection
    uint32_t num_biases_detected;
    bias_detection_t biases[8];        /**< Up to 8 biases detected */

    // Reasoning quality
    float logical_coherence;           /**< Logical consistency (0-1) */
    float prior_compatibility;         /**< Compatible with prior knowledge (0-1) */

    // Recommendations
    char reasoning[512];               /**< Explanation of assessment */
    char recommendation[256];          /**< What to do with this claim */
} epistemic_assessment_t;

/**
 * @brief Epistemic filter engine
 */
typedef struct epistemic_filter_struct* epistemic_filter_t;

//=============================================================================
// API Functions
//=============================================================================

/**
 * @brief Create epistemic filter engine
 *
 * @param skepticism_level Default skepticism (0=credulous, 0.5=balanced, 1=extreme skeptic)
 * @return Filter engine or NULL on failure
 */
epistemic_filter_t epistemic_filter_create(float skepticism_level);

/**
 * @brief Destroy epistemic filter engine
 *
 * @param filter Filter to destroy
 */
void epistemic_filter_destroy(epistemic_filter_t filter);

/**
 * @brief Assess a claim's epistemic quality
 *
 * WHAT: Evaluates a claim for evidence, biases, and reasoning quality
 * WHY:  Prevents accepting unproven or biased information
 * HOW:  Applies multiple epistemic filters and heuristics
 *
 * @param filter Epistemic filter engine
 * @param claim_text Text of the claim (for pattern analysis)
 * @param prior_probability Prior probability based on existing knowledge (0-1)
 * @param evidence Evidence assessment
 * @param assessment Output: comprehensive epistemic assessment
 * @return true on success, false on error
 */
bool epistemic_assess_claim(
    epistemic_filter_t filter,
    const char* claim_text,
    float prior_probability,
    const claim_evidence_t* evidence,
    epistemic_assessment_t* assessment);

/**
 * @brief Detect cognitive biases in reasoning pattern
 *
 * @param filter Epistemic filter engine
 * @param reasoning_features Features of the reasoning process
 * @param num_features Number of features
 * @param biases Output: detected biases (caller-allocated array)
 * @param max_biases Maximum biases to detect
 * @return Number of biases detected
 */
uint32_t epistemic_detect_biases(
    epistemic_filter_t filter,
    const float* reasoning_features,
    uint32_t num_features,
    bias_detection_t* biases,
    uint32_t max_biases);

/**
 * @brief Update source reliability based on claim outcome
 *
 * @param filter Epistemic filter engine
 * @param source_id Source identifier
 * @param was_correct Whether the claim was verified correct
 * @return true on success, false on error
 */
bool epistemic_update_source(
    epistemic_filter_t filter,
    const char* source_id,
    bool was_correct);

/**
 * @brief Get source reliability score
 *
 * @param filter Epistemic filter engine
 * @param source_id Source identifier
 * @return Reliability score (0-1), or -1 if unknown
 */
float epistemic_get_source_reliability(
    epistemic_filter_t filter,
    const char* source_id);

/**
 * @brief Check if claim is conspiracy-theory-like
 *
 * PATTERN DETECTION:
 * - Unfalsifiable claims
 * - Pattern-seeking in randomness
 * - Rejection of mainstream evidence
 * - Ad-hoc hypotheses to explain away contradictions
 * - "They don't want you to know" narratives
 *
 * @param filter Epistemic filter engine
 * @param claim_text Claim text
 * @param evidence Evidence for claim
 * @return Conspiracy score (0-1, higher = more conspiracy-like)
 */
float epistemic_check_conspiracy_pattern(
    epistemic_filter_t filter,
    const char* claim_text,
    const claim_evidence_t* evidence);

/**
 * @brief Apply "extraordinary claims require extraordinary evidence" rule
 *
 * @param prior_plausibility Prior plausibility of claim
 * @param evidence_quality Quality of evidence
 * @return Adjusted credibility score (0-1)
 */
float epistemic_apply_sagan_standard(
    claim_plausibility_t prior_plausibility,
    evidence_quality_t evidence_quality);

/**
 * @brief Initialize claim evidence structure with defaults
 *
 * @param evidence Evidence structure to initialize
 */
void epistemic_evidence_init(claim_evidence_t* evidence);

/**
 * @brief Initialize epistemic assessment structure
 *
 * @param assessment Assessment structure to initialize
 */
void epistemic_assessment_init(epistemic_assessment_t* assessment);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_EPISTEMIC_FILTER_H
