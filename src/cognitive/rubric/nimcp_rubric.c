/**
 * @file nimcp_rubric.c
 * @brief Cognitive Output Rubric — Two-tier human-style quality evaluation
 *
 * Tier 1 (Structural): rule-based, automatable checks
 * Tier 2 (Qualitative): holistic, subsystem-dependent scores
 *
 * Missing subsystems score 0.5 (neutral) when skip_missing_subsystems=true.
 */

#include "cognitive/rubric/nimcp_rubric.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/epistemic/nimcp_epistemic_filter.h"
#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/creative/nimcp_creative.h"
#include "cognitive/creative/appreciation/nimcp_aesthetic_evaluation.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"

#include <math.h>
#include <string.h>
#include <ctype.h>

#define LOG_MODULE "RUBRIC"

/*=============================================================================
 * Evaluator Structure
 *============================================================================*/

struct rubric_evaluator {
    rubric_config_t config;
};

/*=============================================================================
 * Configuration Defaults
 *============================================================================*/

void rubric_config_defaults(rubric_config_t* config) {
    if (!config) return;
    config->tier1_weight = 0.5f;
    config->tier2_weight = 0.5f;
    config->skip_missing_subsystems = true;
}

/*=============================================================================
 * Lifecycle
 *============================================================================*/

rubric_evaluator_t* rubric_evaluator_create(const rubric_config_t* config) {
    rubric_evaluator_t* eval = (rubric_evaluator_t*)nimcp_calloc(1, sizeof(rubric_evaluator_t));
    if (!eval) return NULL;

    if (config) {
        eval->config = *config;
    } else {
        rubric_config_defaults(&eval->config);
    }
    return eval;
}

void rubric_evaluator_destroy(rubric_evaluator_t* eval) {
    nimcp_free(eval);
    eval = NULL;
}

/*=============================================================================
 * Tier 1 Dimension Scorers
 *============================================================================*/

/**
 * Internal consistency: cosine similarity between first and second halves
 * of the output vector. Penalizes contradictory activations.
 */
static float score_internal_consistency(const brain_decision_t* decision) {
    if (!decision->output_vector || decision->output_size < 2) return 0.5f;

    uint32_t half = decision->output_size / 2;
    float dot = 0.0f, mag_a = 0.0f, mag_b = 0.0f;

    for (uint32_t i = 0; i < half; i++) {
        float a = decision->output_vector[i];
        float b = decision->output_vector[half + i];
        dot   += a * b;
        mag_a += a * a;
        mag_b += b * b;
    }

    if (mag_a < 1e-12f || mag_b < 1e-12f) return 0.5f;

    float cosine = dot / (sqrtf(mag_a) * sqrtf(mag_b));
    /* Map [-1, 1] → [0, 1] */
    return (cosine + 1.0f) * 0.5f;
}

/**
 * Confidence calibration: how well does confidence match running accuracy?
 * Perfect calibration = confidence equals actual accuracy.
 */
static float score_confidence_calibration(brain_t brain,
                                          const brain_decision_t* decision) {
    if (!brain) return 0.5f;
    float running_acc = brain->stats.accuracy;
    float gap = fabsf(decision->confidence - running_acc);
    return fmaxf(0.0f, 1.0f - gap);
}

/**
 * Completeness: fraction of output vector entries that are non-zero.
 */
static float score_completeness(const brain_decision_t* decision) {
    if (!decision->output_vector || decision->output_size == 0) return 0.0f;

    uint32_t nonzero = 0;
    for (uint32_t i = 0; i < decision->output_size; i++) {
        if (fabsf(decision->output_vector[i]) > 1e-9f) nonzero++;
    }
    return (float)nonzero / (float)decision->output_size;
}

/**
 * Reasoning chain quality: analyse explanation string.
 * Checks length, delimiter count (WHAT|WHY|PROOF sections), word count.
 */
static float score_reasoning_chain(const brain_decision_t* decision) {
    const char* expl = decision->explanation;
    if (!expl || expl[0] == '\0') return 0.0f;

    size_t len = strlen(expl);

    /* Length score: 0 at 0 chars, 1.0 at ≥200 chars */
    float len_score = fminf(1.0f, (float)len / 200.0f);

    /* Delimiter score: count structural markers (|, :, -, .) */
    uint32_t delims = 0;
    for (size_t i = 0; i < len; i++) {
        if (expl[i] == '|' || expl[i] == ':' || expl[i] == '-')
            delims++;
    }
    float delim_score = fminf(1.0f, (float)delims / 6.0f);

    /* Word count score: 0 at 0 words, 1.0 at ≥30 words */
    uint32_t words = 0;
    bool in_word = false;
    for (size_t i = 0; i < len; i++) {
        if (isspace((unsigned char)expl[i])) {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            words++;
        }
    }
    float word_score = fminf(1.0f, (float)words / 30.0f);

    return 0.4f * len_score + 0.3f * delim_score + 0.3f * word_score;
}

/**
 * Epistemic quality: call epistemic filter if available.
 */
static float score_epistemic(brain_t brain,
                             const brain_decision_t* decision,
                             uint32_t* subsystems) {
    if (!brain || !brain->epistemic) return 0.5f;

    *subsystems |= RUBRIC_HAS_EPISTEMIC;

    epistemic_assessment_t assessment;
    memset(&assessment, 0, sizeof(assessment));

    claim_evidence_t evidence;
    memset(&evidence, 0, sizeof(evidence));
    evidence.evidence_strength = decision->confidence;

    bool ok = epistemic_assess_claim(brain->epistemic,
                                     decision->explanation,
                                     decision->confidence,
                                     &evidence,
                                     &assessment);
    if (!ok) return 0.5f;
    return fmaxf(0.0f, fminf(1.0f, assessment.epistemic_quality));
}

/**
 * Ethical alignment: call ethics engine if available.
 * Remaps golden_rule_score from [-1,+1] to [0,1].
 */
static float score_ethical_alignment(brain_t brain,
                                     const brain_decision_t* decision,
                                     uint32_t* subsystems) {
    if (!brain || !brain->ethics) return 0.5f;

    *subsystems |= RUBRIC_HAS_ETHICS;

    action_context_t action;
    memset(&action, 0, sizeof(action));
    action.num_features = decision->output_size;
    action.features = decision->output_vector;

    ethics_evaluation_t eval = ethics_engine_evaluate_action(brain->ethics, &action);
    /* Map [-1, +1] → [0, 1] */
    return fmaxf(0.0f, fminf(1.0f, (eval.golden_rule_score + 1.0f) * 0.5f));
}

/*=============================================================================
 * Tier 2 Dimension Scorers
 *============================================================================*/

/**
 * Originality: use aesthetic evaluator's text evaluation if available.
 */
static float score_originality(brain_t brain,
                               const brain_decision_t* decision,
                               uint32_t* subsystems) {
    /* Check if brain has creative orchestrator with aesthetic evaluator */
    /* The aesthetic evaluator is part of the creative subsystem */
    (void)brain;
    (void)decision;
    (void)subsystems;

    /* Aesthetic evaluator requires creative orchestrator — check via brain pointer */
    /* For now, use explanation text heuristic if no evaluator */
    const char* expl = decision->explanation;
    if (!expl || expl[0] == '\0') return 0.5f;

    /* Heuristic: unique word ratio as proxy for originality */
    /* Count distinct first-characters as cheap proxy */
    uint32_t char_buckets[26] = {0};
    uint32_t total_words = 0;
    bool in_word = false;
    size_t len = strlen(expl);
    for (size_t i = 0; i < len; i++) {
        if (isspace((unsigned char)expl[i])) {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            total_words++;
            char c = (char)tolower((unsigned char)expl[i]);
            if (c >= 'a' && c <= 'z')
                char_buckets[c - 'a'] = 1;
        }
    }
    if (total_words == 0) return 0.5f;

    uint32_t unique_starts = 0;
    for (int i = 0; i < 26; i++) unique_starts += char_buckets[i];

    return fminf(1.0f, (float)unique_starts / fminf(26.0f, (float)total_words));
}

/**
 * Integration depth: active neurons / total neurons, weighted by sparsity.
 */
static float score_integration_depth(brain_t brain,
                                     const brain_decision_t* decision) {
    if (!brain || brain->stats.num_neurons == 0) return 0.5f;

    float ratio = (float)decision->num_active_neurons / (float)brain->stats.num_neurons;
    /* Weight by inverse sparsity — denser activation = more integration */
    float sparsity_bonus = 1.0f - decision->sparsity;
    return fminf(1.0f, ratio * 2.0f + sparsity_bonus * 0.3f);
}

/**
 * Communication clarity: label specificity + explanation readability.
 * Penalizes generic labels like "unknown" or "output_N".
 */
static float score_communication_clarity(const brain_decision_t* decision) {
    float label_score = 1.0f;
    const char* label = decision->label;

    /* Penalize generic labels */
    if (!label || label[0] == '\0') {
        label_score = 0.0f;
    } else if (strcmp(label, "unknown") == 0 || strcmp(label, "Unknown") == 0) {
        label_score = 0.2f;
    } else if (strncmp(label, "output_", 7) == 0) {
        label_score = 0.3f;
    } else if (strlen(label) <= 2) {
        label_score = 0.5f;
    }

    /* Explanation readability: average word length (3-7 is ideal) */
    float readability = 0.5f;
    const char* expl = decision->explanation;
    if (expl && expl[0] != '\0') {
        size_t len = strlen(expl);
        uint32_t words = 0, chars = 0;
        for (size_t i = 0; i < len; i++) {
            if (isalpha((unsigned char)expl[i])) chars++;
            if (isspace((unsigned char)expl[i]) || i == len - 1) {
                if (chars > 0) words++;
            }
        }
        if (words > 0) {
            float avg_word_len = (float)chars / (float)words;
            /* Ideal range 3-7, penalize outside */
            if (avg_word_len >= 3.0f && avg_word_len <= 7.0f) {
                readability = 1.0f;
            } else {
                readability = fmaxf(0.3f, 1.0f - fabsf(avg_word_len - 5.0f) * 0.15f);
            }
        }
    }

    return 0.5f * label_score + 0.5f * readability;
}

/**
 * Engagement quality: Berlyne dimensions (hedonic tone + arousal).
 * Uses aesthetic evaluator if available, else heuristic from explanation length/variety.
 */
static float score_engagement(brain_t brain,
                              const brain_decision_t* decision,
                              uint32_t* subsystems) {
    (void)brain;
    (void)subsystems;

    /* Heuristic: explanation variety as engagement proxy */
    const char* expl = decision->explanation;
    if (!expl || expl[0] == '\0') return 0.5f;

    size_t len = strlen(expl);
    /* Variety: count distinct characters used */
    bool seen[256] = {false};
    for (size_t i = 0; i < len; i++)
        seen[(unsigned char)expl[i]] = true;

    uint32_t distinct = 0;
    for (int i = 0; i < 256; i++) distinct += seen[i];

    /* 40+ distinct chars = highly engaging text */
    return fminf(1.0f, (float)distinct / 40.0f);
}

/**
 * Empathetic accuracy: mirror neuron match quality if available.
 */
static float score_empathetic_accuracy(brain_t brain,
                                       uint32_t* subsystems) {
    if (!brain || !brain->mirror_neurons) return 0.5f;

    *subsystems |= RUBRIC_HAS_MIRROR;

    mirror_neuron_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    bool ok = mirror_neurons_get_stats(brain->mirror_neurons, &stats);
    if (!ok) return 0.5f;

    return fmaxf(0.0f, fminf(1.0f, stats.avg_match_quality));
}

/**
 * Information density: normalized Shannon entropy of softmax(output_vector).
 * H / log(output_size) gives [0,1] where 1 = maximum entropy.
 */
static float score_information_density(const brain_decision_t* decision) {
    if (!decision->output_vector || decision->output_size < 2) return 0.5f;

    /* Compute softmax */
    float max_val = decision->output_vector[0];
    for (uint32_t i = 1; i < decision->output_size; i++) {
        if (decision->output_vector[i] > max_val)
            max_val = decision->output_vector[i];
    }

    float sum_exp = 0.0f;
    for (uint32_t i = 0; i < decision->output_size; i++) {
        sum_exp += expf(decision->output_vector[i] - max_val);
    }

    if (sum_exp < 1e-12f) return 0.0f;

    /* Shannon entropy */
    float entropy = 0.0f;
    for (uint32_t i = 0; i < decision->output_size; i++) {
        float p = expf(decision->output_vector[i] - max_val) / sum_exp;
        if (p > 1e-12f)
            entropy -= p * logf(p);
    }

    /* Normalize by max entropy */
    float max_entropy = logf((float)decision->output_size);
    if (max_entropy < 1e-12f) return 0.5f;

    return fmaxf(0.0f, fminf(1.0f, entropy / max_entropy));
}

/*=============================================================================
 * Grade Derivation
 *============================================================================*/

static void derive_grade(float score, char* grade, char* modifier) {
    if (score >= 0.93f)      { *grade = 'A'; *modifier = '+'; }
    else if (score >= 0.87f) { *grade = 'A'; *modifier = ' '; }
    else if (score >= 0.83f) { *grade = 'A'; *modifier = '-'; }
    else if (score >= 0.80f) { *grade = 'B'; *modifier = '+'; }
    else if (score >= 0.73f) { *grade = 'B'; *modifier = ' '; }
    else if (score >= 0.70f) { *grade = 'B'; *modifier = '-'; }
    else if (score >= 0.67f) { *grade = 'C'; *modifier = '+'; }
    else if (score >= 0.60f) { *grade = 'C'; *modifier = ' '; }
    else if (score >= 0.57f) { *grade = 'C'; *modifier = '-'; }
    else if (score >= 0.53f) { *grade = 'D'; *modifier = '+'; }
    else if (score >= 0.50f) { *grade = 'D'; *modifier = ' '; }
    else                     { *grade = 'F'; *modifier = ' '; }
}

/*=============================================================================
 * Main Evaluation
 *============================================================================*/

int rubric_evaluate_decision(rubric_evaluator_t* eval,
                             brain_t brain,
                             const brain_decision_t* decision,
                             rubric_result_t* result) {
    if (!eval || !decision || !result) return -1;

    uint64_t start_us = nimcp_time_get_us();
    memset(result, 0, sizeof(*result));

    uint32_t subsystems = 0;

    /* --- Tier 1: Structural --- */
    result->tier1.internal_consistency   = score_internal_consistency(decision);
    result->tier1.confidence_calibration = score_confidence_calibration(brain, decision);
    result->tier1.completeness           = score_completeness(decision);
    result->tier1.reasoning_chain_quality = score_reasoning_chain(decision);
    result->tier1.epistemic_quality      = score_epistemic(brain, decision, &subsystems);
    result->tier1.ethical_alignment      = score_ethical_alignment(brain, decision, &subsystems);

    /* Tier 1 aggregate (equal weights for 6 dimensions) */
    result->tier1.tier1_score = (result->tier1.internal_consistency
                                + result->tier1.confidence_calibration
                                + result->tier1.completeness
                                + result->tier1.reasoning_chain_quality
                                + result->tier1.epistemic_quality
                                + result->tier1.ethical_alignment) / 6.0f;

    /* --- Tier 2: Qualitative --- */
    result->tier2.originality            = score_originality(brain, decision, &subsystems);
    result->tier2.integration_depth      = score_integration_depth(brain, decision);
    result->tier2.communication_clarity  = score_communication_clarity(decision);
    result->tier2.engagement_quality     = score_engagement(brain, decision, &subsystems);
    result->tier2.empathetic_accuracy    = score_empathetic_accuracy(brain, &subsystems);
    result->tier2.information_density    = score_information_density(decision);

    /* Tier 2 aggregate (equal weights for 6 dimensions) */
    result->tier2.tier2_score = (result->tier2.originality
                                + result->tier2.integration_depth
                                + result->tier2.communication_clarity
                                + result->tier2.engagement_quality
                                + result->tier2.empathetic_accuracy
                                + result->tier2.information_density) / 6.0f;

    /* --- Overall --- */
    result->overall_score = eval->config.tier1_weight * result->tier1.tier1_score
                          + eval->config.tier2_weight * result->tier2.tier2_score;
    result->overall_score = fmaxf(0.0f, fminf(1.0f, result->overall_score));

    derive_grade(result->overall_score, &result->grade, &result->grade_modifier);

    result->subsystems_available = subsystems;
    result->evaluation_time_us = nimcp_time_get_us() - start_us;

    return 0;
}
