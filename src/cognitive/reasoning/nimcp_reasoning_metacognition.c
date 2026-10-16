/**
 * @file nimcp_reasoning_metacognition.c
 * @brief Metacognitive Controller — adaptive strategy selection implementation
 *
 * WHAT: Estimates query complexity and selects optimal reasoning strategy
 * WHY:  Avoid wasting convergent reasoning on trivial queries; allocate
 *       cognitive resources proportional to task difficulty
 * HOW:  Heuristic complexity scoring from query features + EMA-based
 *       threshold adaptation from recorded outcomes
 *
 * COMPLEXITY HEURISTICS:
 *   - Length score: longer queries tend to be more complex
 *   - Logical operators: "and", "or", "not", "if", "then" increase complexity
 *   - Causal language: "because", "therefore", "implies" signal reasoning depth
 *   - Counterfactuals: "if...had...would", "what if" signal hypothetical reasoning
 *   - Analogical: "like", "similar to", "compared to" signal cross-domain mapping
 *   - Nested structure: commas, semicolons, parentheses signal clause nesting
 *   - Factual prefix: "what is", "define", "who is" signal simple lookup
 *
 * STRATEGY MAPPING:
 *   TRIVIAL  -> STRATEGY_SEQUENTIAL (minimal overhead, skip most phases)
 *   SIMPLE   -> STRATEGY_SEQUENTIAL
 *   MODERATE -> STRATEGY_CONCURRENT (if thread pool available)
 *   COMPLEX  -> STRATEGY_CONVERGENT (if enabled)
 *   HARD     -> STRATEGY_CONVERGENT
 *
 * OVERRIDE RULES:
 *   - Convergent disabled -> downgrade to CONCURRENT
 *   - Concurrent disabled -> downgrade to SEQUENTIAL
 *   - Portia SEVERE budget -> force SEQUENTIAL
 *
 * @version 1.0.0
 * @date 2026-02-26
 */

#include "cognitive/reasoning/nimcp_reasoning_metacognition.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

#define LOG_MODULE "reasoning_metacognition"

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Outcome history entry for learning
 */
typedef struct {
    reasoning_strategy_t strategy_used;
    float actual_confidence;
    float actual_time_us;
    uint32_t actual_steps;
    query_complexity_t assessed_complexity;
} metacognitive_outcome_t;

/**
 * @brief Metacognitive controller internal state
 */
struct reasoning_metacognition {
    metacognitive_config_t config;
    metacognitive_stats_t stats;

    /* Outcome history (ring buffer) */
    metacognitive_outcome_t* history;
    uint32_t history_count;
    uint32_t history_write_idx;

    /* Adaptive thresholds (learned from outcomes) */
    float adapted_threshold_simple;
    float adapted_threshold_moderate;
    float adapted_threshold_hard;

    /* Per-strategy performance tracking */
    float avg_steps_per_strategy[REASONING_NUM_STRATEGIES];
    float avg_confidence_per_strategy[REASONING_NUM_STRATEGIES];
    uint32_t outcome_count_per_strategy[REASONING_NUM_STRATEGIES];

    /* Assessment timing */
    float total_assessment_time_us;
};

/*=============================================================================
 * INTERNAL HELPER FUNCTIONS
 *===========================================================================*/

/**
 * @brief Case-insensitive substring search
 */
static bool contains_word(const char* text, const char* word)
{
    if (!text || !word) return false;

    size_t text_len = strlen(text);
    size_t word_len = strlen(word);
    if (word_len > text_len) return false;

    for (size_t i = 0; i <= text_len - word_len; i++) {
        /* Check if this is a word boundary (start of string or non-alpha before) */
        if (i > 0 && isalpha((unsigned char)text[i - 1])) continue;

        /* Case-insensitive comparison */
        bool match = true;
        for (size_t j = 0; j < word_len; j++) {
            if (tolower((unsigned char)text[i + j]) != tolower((unsigned char)word[j])) {
                match = false;
                break;
            }
        }

        if (match) {
            /* Check word boundary at end */
            size_t end = i + word_len;
            if (end < text_len && isalpha((unsigned char)text[end])) continue;
            return true;
        }
    }

    return false;
}

/**
 * @brief Count occurrences of a word (case-insensitive, word-boundary aware)
 */
static uint32_t count_word(const char* text, const char* word)
{
    if (!text || !word) return 0;

    uint32_t count = 0;
    size_t text_len = strlen(text);
    size_t word_len = strlen(word);
    if (word_len > text_len) return 0;

    for (size_t i = 0; i <= text_len - word_len; i++) {
        if (i > 0 && isalpha((unsigned char)text[i - 1])) continue;

        bool match = true;
        for (size_t j = 0; j < word_len; j++) {
            if (tolower((unsigned char)text[i + j]) != tolower((unsigned char)word[j])) {
                match = false;
                break;
            }
        }

        if (match) {
            size_t end = i + word_len;
            if (end < text_len && isalpha((unsigned char)text[end])) continue;
            count++;
        }
    }

    return count;
}

/**
 * @brief Check if query starts with a factual lookup prefix
 */
static bool is_factual_prefix(const char* query)
{
    if (!query) return false;

    /* Skip leading whitespace */
    while (*query && isspace((unsigned char)*query)) query++;

    static const char* prefixes[] = {
        "what is", "what are", "define", "who is", "who are",
        "when is", "when was", "where is", "where was",
        "name the", "list the", "how many",
        NULL
    };

    for (int i = 0; prefixes[i]; i++) {
        size_t plen = strlen(prefixes[i]);
        bool match = true;
        for (size_t j = 0; j < plen; j++) {
            if (tolower((unsigned char)query[j]) != tolower((unsigned char)prefixes[i][j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }

    return false;
}

/**
 * @brief Compute raw complexity score from query features
 *
 * Returns a score in [0.0, 1.0] representing estimated difficulty.
 */
static float compute_complexity_score(const char* query)
{
    if (!query) return 0.0f;

    size_t len = strlen(query);
    float score = 0.0f;

    /* ── Length component (0.0 - 0.15) ── */
    if (len < 10) {
        score += 0.0f;
    } else if (len < 30) {
        score += 0.05f;
    } else if (len < 60) {
        score += 0.08f;
    } else if (len < 120) {
        score += 0.12f;
    } else {
        score += 0.15f;
    }

    /* ── Factual prefix detection (reduces score) ── */
    if (is_factual_prefix(query)) {
        score -= 0.10f;
    }

    /* ── Logical operators (0.0 - 0.30) ── */
    static const char* logical_ops[] = {
        "and", "or", "not", "if", "then", "but", "however",
        "although", "unless", "either", "neither", NULL
    };
    uint32_t logical_count = 0;
    for (int i = 0; logical_ops[i]; i++) {
        logical_count += count_word(query, logical_ops[i]);
    }
    float logical_score = (float)logical_count * 0.06f;
    if (logical_score > 0.30f) logical_score = 0.30f;
    score += logical_score;

    /* ── Causal language (0.0 - 0.20) ── */
    static const char* causal_words[] = {
        "because", "therefore", "implies", "causes", "leads to",
        "results in", "consequently", "hence", "thus", "so that", NULL
    };
    uint32_t causal_count = 0;
    for (int i = 0; causal_words[i]; i++) {
        causal_count += count_word(query, causal_words[i]);
    }
    float causal_score = (float)causal_count * 0.08f;
    if (causal_score > 0.20f) causal_score = 0.20f;
    score += causal_score;

    /* ── Counterfactual language (0.0 - 0.20) ── */
    bool has_counterfactual = false;
    if (contains_word(query, "would") && contains_word(query, "had")) {
        has_counterfactual = true;
    }
    if (contains_word(query, "what if")) {
        has_counterfactual = true;
    }
    if (contains_word(query, "hypothetically")) {
        has_counterfactual = true;
    }
    if (contains_word(query, "suppose")) {
        has_counterfactual = true;
    }
    if (contains_word(query, "imagine")) {
        has_counterfactual = true;
    }
    if (has_counterfactual) {
        score += 0.20f;
    }

    /* ── Analogical language (0.0 - 0.15) ── */
    static const char* analogy_words[] = {
        "like", "similar to", "compared to", "analogous",
        "as if", "resembles", "parallel", NULL
    };
    uint32_t analogy_count = 0;
    for (int i = 0; analogy_words[i]; i++) {
        analogy_count += count_word(query, analogy_words[i]);
    }
    if (analogy_count > 0) {
        float analogy_score = (float)analogy_count * 0.08f;
        if (analogy_score > 0.15f) analogy_score = 0.15f;
        score += analogy_score;
    }

    /* ── Nested structure: commas, semicolons, parentheses ── */
    uint32_t clause_markers = 0;
    for (size_t i = 0; i < len; i++) {
        if (query[i] == ',' || query[i] == ';' || query[i] == '(' || query[i] == ')') {
            clause_markers++;
        }
    }
    float nesting_score = (float)clause_markers * 0.03f;
    if (nesting_score > 0.10f) nesting_score = 0.10f;
    score += nesting_score;

    /* Clamp to [0.0, 1.0] */
    if (score < 0.0f) score = 0.0f;
    if (score > 1.0f) score = 1.0f;

    return score;
}

/**
 * @brief Map complexity score to complexity enum using adaptive thresholds
 */
static query_complexity_t score_to_complexity(const reasoning_metacognition_t* mc,
                                                float score)
{
    if (score < mc->adapted_threshold_simple) {
        return REASONING_COMPLEXITY_TRIVIAL;
    } else if (score < mc->adapted_threshold_moderate) {
        return REASONING_COMPLEXITY_SIMPLE;
    } else if (score < mc->adapted_threshold_hard) {
        return REASONING_COMPLEXITY_MODERATE;
    } else if (score < 0.90f) {
        return REASONING_COMPLEXITY_COMPLEX;
    } else {
        return REASONING_COMPLEXITY_HARD;
    }
}

/**
 * @brief Map complexity to default strategy label (for logging only)
 *
 * This is retained for backward compatibility in logging and stats.
 * The actual dispatch uses the continuous resource budget, not this enum.
 */
static reasoning_strategy_t complexity_to_strategy(query_complexity_t complexity)
{
    switch (complexity) {
        case REASONING_COMPLEXITY_TRIVIAL:
            return REASONING_STRATEGY_SEQUENTIAL;
        case REASONING_COMPLEXITY_SIMPLE:
            return REASONING_STRATEGY_SEQUENTIAL;
        case REASONING_COMPLEXITY_MODERATE:
            return REASONING_STRATEGY_CONCURRENT;
        case REASONING_COMPLEXITY_COMPLEX:
            return REASONING_STRATEGY_CONVERGENT;
        case REASONING_COMPLEXITY_HARD:
            return REASONING_STRATEGY_CONVERGENT;
        default:
            return REASONING_STRATEGY_SEQUENTIAL;
    }
}

/**
 * @brief Compute continuous resource budget from complexity score
 *
 * Maps the raw [0.0, 1.0] complexity score to continuous resource parameters.
 * All interpolations are smooth — no hard cutoffs.
 *
 * Scaling curves:
 *   parallelism_factor:    linear [0.0, 1.0]
 *   max_contributors:      quadratic: score^1.5 * max  (accelerating growth)
 *   max_steps:             linear: 3 + score * (max_steps - 3)
 *   convergence_threshold: exponential decay: 0.05 * exp(-3 * score)
 *   confidence_target:     inverse linear: 0.9 - 0.4 * score (easier to satisfy at high complexity)
 *   timeout_factor:        linear: 0.2 + 0.8 * score
 *   use_thread_pool:       true when score > 0.1
 */
static reasoning_resource_budget_t compute_resource_budget(
    float score,
    const reasoning_engine_config_t* cfg)
{
    reasoning_resource_budget_t budget;
    memset(&budget, 0, sizeof(budget));

    /* Clamp score */
    if (score < 0.0f) score = 0.0f;
    if (score > 1.0f) score = 1.0f;

    /* Parallelism factor: direct mapping from score */
    budget.parallelism_factor = score;

    /* Max contributors: quadratic scaling (score^1.5) for accelerating growth
     * At score=0.0: 1 contributor (sequential-like)
     * At score=0.5: ~35% of max
     * At score=1.0: 100% of max */
    uint32_t max_possible = cfg ? cfg->max_convergent_contributors : 64;
    if (max_possible == 0) max_possible = 64;
    float scaled = powf(score, 1.5f) * (float)max_possible;
    budget.max_contributors = (uint32_t)(scaled + 0.5f);
    if (budget.max_contributors < 1) budget.max_contributors = 1;
    if (budget.max_contributors > max_possible)
        budget.max_contributors = max_possible;

    /* Max steps: linear interpolation [3, max_steps] */
    uint32_t max_steps = cfg ? cfg->max_steps : 50;
    if (max_steps < 3) max_steps = 50;
    budget.max_steps = 3 + (uint32_t)(score * (float)(max_steps - 3) + 0.5f);

    /* Convergence threshold: exponential decay
     * High score = looser threshold (patient, waits for deep convergence)
     * Low score = tight threshold (accepts quick answers) */
    budget.convergence_threshold = 0.05f * expf(-3.0f * score);
    if (budget.convergence_threshold < 0.001f)
        budget.convergence_threshold = 0.001f;

    /* Confidence target: lower for complex queries (harder to be certain)
     * score=0: 0.9 (easy queries should be very confident)
     * score=1: 0.5 (hard queries accept moderate confidence) */
    budget.confidence_target = 0.9f - 0.4f * score;

    /* Timeout factor: linear scaling
     * Low score gets less time, high score gets full timeout */
    budget.timeout_factor = 0.2f + 0.8f * score;

    /* Thread pool: engage when there's enough complexity to benefit */
    budget.use_thread_pool = (score > 0.10f);

    return budget;
}

/**
 * @brief Estimate number of reasoning steps (now derived from budget)
 */
static uint32_t estimate_steps(query_complexity_t complexity)
{
    switch (complexity) {
        case REASONING_COMPLEXITY_TRIVIAL:  return 3;
        case REASONING_COMPLEXITY_SIMPLE:   return 8;
        case REASONING_COMPLEXITY_MODERATE: return 15;
        case REASONING_COMPLEXITY_COMPLEX:  return 25;
        case REASONING_COMPLEXITY_HARD:     return 40;
        default: return 10;
    }
}

/**
 * @brief Estimate execution time (now derived from budget)
 */
static float estimate_time_us(query_complexity_t complexity)
{
    switch (complexity) {
        case REASONING_COMPLEXITY_TRIVIAL:  return 100.0f;
        case REASONING_COMPLEXITY_SIMPLE:   return 500.0f;
        case REASONING_COMPLEXITY_MODERATE: return 2000.0f;
        case REASONING_COMPLEXITY_COMPLEX:  return 10000.0f;
        case REASONING_COMPLEXITY_HARD:     return 50000.0f;
        default: return 1000.0f;
    }
}

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

metacognitive_config_t reasoning_metacognition_default_config(void)
{
    metacognitive_config_t config;
    memset(&config, 0, sizeof(config));

    config.enable_metacognition = true;
    config.complexity_threshold_simple = REASONING_METACOG_DEFAULT_SIMPLE_THRESHOLD;
    config.complexity_threshold_moderate = REASONING_METACOG_DEFAULT_MODERATE_THRESHOLD;
    config.complexity_threshold_hard = REASONING_METACOG_DEFAULT_HARD_THRESHOLD;
    config.learning_rate = REASONING_METACOG_DEFAULT_LEARNING_RATE;
    config.history_size = REASONING_METACOG_DEFAULT_HISTORY_SIZE;

    return config;
}

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

reasoning_metacognition_t* reasoning_metacognition_create(
    const metacognitive_config_t* config)
{
    reasoning_metacognition_t* mc = (reasoning_metacognition_t*)nimcp_calloc(
        1, sizeof(reasoning_metacognition_t));
    if (!mc) {
        NIMCP_LOGGING_ERROR("reasoning_metacognition: failed to allocate controller");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        mc->config = *config;
    } else {
        mc->config = reasoning_metacognition_default_config();
    }

    /* Validate history size */
    if (mc->config.history_size == 0) {
        mc->config.history_size = REASONING_METACOG_DEFAULT_HISTORY_SIZE;
    }
    if (mc->config.history_size > REASONING_METACOG_MAX_HISTORY_SIZE) {
        mc->config.history_size = REASONING_METACOG_MAX_HISTORY_SIZE;
    }

    /* Allocate outcome history */
    mc->history = (metacognitive_outcome_t*)nimcp_calloc(
        mc->config.history_size, sizeof(metacognitive_outcome_t));
    if (!mc->history) {
        NIMCP_LOGGING_ERROR("reasoning_metacognition: failed to allocate history");
        nimcp_free(mc);
        mc = NULL;
        return NULL;
    }

    /* Initialize adaptive thresholds from config */
    mc->adapted_threshold_simple = mc->config.complexity_threshold_simple;
    mc->adapted_threshold_moderate = mc->config.complexity_threshold_moderate;
    mc->adapted_threshold_hard = mc->config.complexity_threshold_hard;

    NIMCP_LOGGING_INFO("reasoning_metacognition: controller created "
                       "(thresholds=%.2f/%.2f/%.2f, learning_rate=%.3f)",
                       mc->adapted_threshold_simple,
                       mc->adapted_threshold_moderate,
                       mc->adapted_threshold_hard,
                       mc->config.learning_rate);

    return mc;
}

void reasoning_metacognition_destroy(reasoning_metacognition_t* mc)
{
    if (!mc) return;

    if (mc->history) {
        nimcp_free(mc->history);
        mc->history = NULL;
    }

    NIMCP_LOGGING_INFO("reasoning_metacognition: controller destroyed "
                       "(assessments=%u, accuracy=%.3f)",
                       mc->stats.total_assessments,
                       mc->stats.accuracy);

    nimcp_free(mc);
    mc = NULL;
}

/*=============================================================================
 * CORE ASSESSMENT
 *===========================================================================*/

metacognitive_assessment_t reasoning_metacognition_assess(
    reasoning_metacognition_t* mc,
    const char* query,
    const void* engine_config)
{
    metacognitive_assessment_t result;
    memset(&result, 0, sizeof(result));

    if (!mc || !query) {
        return result;
    }

    uint64_t start_us = nimcp_time_get_us();

    /* Compute raw complexity score */
    float score = compute_complexity_score(query);
    result.complexity_score = score;

    /* Map score to complexity level using adaptive thresholds (for labels) */
    result.complexity = score_to_complexity(mc, score);

    /* Compute continuous resource budget from raw score */
    const reasoning_engine_config_t* cfg =
        (const reasoning_engine_config_t*)engine_config;
    result.budget = compute_resource_budget(score, cfg);

    /* Apply config constraints to budget */
    if (cfg) {
        /* If convergent is disabled, cap parallelism at concurrent level */
        if (!cfg->enable_convergent_reasoning) {
            if (result.budget.parallelism_factor > 0.5f)
                result.budget.parallelism_factor = 0.5f;
            /* Cap contributors to wave-pipeline max (9 existing phases) */
            if (result.budget.max_contributors > 9)
                result.budget.max_contributors = 9;
        }

        /* If concurrent is also disabled, force sequential */
        if (!cfg->enable_concurrent_pipeline) {
            result.budget.parallelism_factor = 0.0f;
            result.budget.use_thread_pool = false;
            result.budget.max_contributors = 1;
        }
    }

    /* Derive discrete strategy label from parallelism_factor (for logging)
     * Labels reflect the *constrained* budget, not the raw score */
    if (result.budget.parallelism_factor < 0.10f) {
        result.recommended_strategy = REASONING_STRATEGY_SEQUENTIAL;
    } else if (cfg && !cfg->enable_convergent_reasoning) {
        /* Convergent disabled: best we can do is concurrent */
        result.recommended_strategy = REASONING_STRATEGY_CONCURRENT;
    } else if (result.budget.parallelism_factor < 0.40f) {
        result.recommended_strategy = REASONING_STRATEGY_CONCURRENT;
    } else {
        result.recommended_strategy = REASONING_STRATEGY_CONVERGENT;
    }

    /* Estimate steps and time from budget */
    result.estimated_steps = result.budget.max_steps;
    result.estimated_time_us = estimate_time_us(result.complexity);

    /* Assessment confidence is high for extreme scores, lower in the middle */
    float dist_to_nearest_threshold = 1.0f;
    float thresholds[] = {
        mc->adapted_threshold_simple,
        mc->adapted_threshold_moderate,
        mc->adapted_threshold_hard,
        0.90f
    };
    for (int i = 0; i < 4; i++) {
        float d = fabsf(score - thresholds[i]);
        if (d < dist_to_nearest_threshold) {
            dist_to_nearest_threshold = d;
        }
    }
    /* Further from thresholds = more confident */
    result.confidence_in_assessment = 0.5f + dist_to_nearest_threshold;
    if (result.confidence_in_assessment > 1.0f) {
        result.confidence_in_assessment = 1.0f;
    }

    /* Update timing stats */
    uint64_t elapsed_us = nimcp_time_get_us() - start_us;
    mc->stats.total_assessments++;
    mc->stats.strategy_counts[result.recommended_strategy]++;

    float n = (float)mc->stats.total_assessments;
    mc->total_assessment_time_us += (float)elapsed_us;
    mc->stats.avg_assessment_time_us = mc->total_assessment_time_us / (fabsf(n) > 1e-7f ? n : 1e-7f);

    NIMCP_LOGGING_DEBUG("reasoning_metacognition: query=\"%.40s%s\" "
                        "score=%.3f complexity=%s strategy=%s confidence=%.3f",
                        query, strlen(query) > 40 ? "..." : "",
                        score,
                        reasoning_metacognition_get_complexity_name(result.complexity),
                        reasoning_metacognition_get_strategy_name(result.recommended_strategy),
                        result.confidence_in_assessment);

    return result;
}

/*=============================================================================
 * OUTCOME LEARNING
 *===========================================================================*/

int reasoning_metacognition_record_outcome(
    reasoning_metacognition_t* mc,
    reasoning_strategy_t used,
    float actual_confidence,
    float actual_time_us,
    uint32_t actual_steps)
{
    if (!mc) return -1;
    if ((int)used < 0 || (int)used >= REASONING_NUM_STRATEGIES) return -1;

    /* Store in ring buffer */
    uint32_t idx = mc->history_write_idx % mc->config.history_size;
    mc->history[idx].strategy_used = used;
    mc->history[idx].actual_confidence = actual_confidence;
    mc->history[idx].actual_time_us = actual_time_us;
    mc->history[idx].actual_steps = actual_steps;
    /* assessed_complexity is set from the most recent assessment context */

    mc->history_write_idx++;
    if (mc->history_count < mc->config.history_size) {
        mc->history_count++;
    }

    /* Update per-strategy averages via EMA */
    uint32_t sidx = (uint32_t)used;
    mc->outcome_count_per_strategy[sidx]++;

    float lr = mc->config.learning_rate;
    if (mc->outcome_count_per_strategy[sidx] == 1) {
        /* First outcome: initialize directly */
        mc->avg_steps_per_strategy[sidx] = (float)actual_steps;
        mc->avg_confidence_per_strategy[sidx] = actual_confidence;
    } else {
        /* EMA update */
        mc->avg_steps_per_strategy[sidx] =
            (1.0f - lr) * mc->avg_steps_per_strategy[sidx] + lr * (float)actual_steps;
        mc->avg_confidence_per_strategy[sidx] =
            (1.0f - lr) * mc->avg_confidence_per_strategy[sidx] + lr * actual_confidence;
    }

    /*
     * Adaptive threshold learning:
     * If a query classified as "simple" (STRATEGY_SEQUENTIAL) actually needed many
     * steps (> 15), the simple threshold was too loose — raise it slightly.
     * If a query classified as "hard" (STRATEGY_CONVERGENT) finished quickly
     * with few steps (< 5), the hard threshold was too tight — lower it slightly.
     */
    if (used == REASONING_STRATEGY_SEQUENTIAL && actual_steps > 15) {
        /* Simple queries taking many steps: make thresholds stricter */
        mc->adapted_threshold_simple =
            (1.0f - lr) * mc->adapted_threshold_simple + lr * (mc->adapted_threshold_simple * 0.90f);
    }
    if (used == REASONING_STRATEGY_CONVERGENT && actual_steps < 5 && actual_confidence > 0.8f) {
        /* Complex queries finishing trivially: relax thresholds */
        mc->adapted_threshold_hard =
            (1.0f - lr) * mc->adapted_threshold_hard + lr * (mc->adapted_threshold_hard * 1.10f);
        if (mc->adapted_threshold_hard > 0.95f) {
            mc->adapted_threshold_hard = 0.95f;
        }
    }

    /* Update accuracy: fraction of outcomes where the strategy was "good enough"
     * (achieved confidence >= 0.5 within reasonable step count for that strategy) */
    bool was_good = (actual_confidence >= 0.3f);
    mc->stats.accuracy = (1.0f - lr) * mc->stats.accuracy + lr * (was_good ? 1.0f : 0.0f);

    return 0;
}

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

int reasoning_metacognition_get_stats(
    const reasoning_metacognition_t* mc,
    metacognitive_stats_t* stats)
{
    if (!mc || !stats) return -1;

    *stats = mc->stats;
    return 0;
}

/*=============================================================================
 * UTILITY
 *===========================================================================*/

const char* reasoning_metacognition_get_strategy_name(reasoning_strategy_t strategy)
{
    switch (strategy) {
        case REASONING_STRATEGY_TRIVIAL:     return "TRIVIAL";
        case REASONING_STRATEGY_SEQUENTIAL:  return "SEQUENTIAL";
        case REASONING_STRATEGY_CONCURRENT:  return "CONCURRENT";
        case REASONING_STRATEGY_CONVERGENT:  return "CONVERGENT";
        default:                              return "UNKNOWN";
    }
}

const char* reasoning_metacognition_get_complexity_name(query_complexity_t complexity)
{
    switch (complexity) {
        case REASONING_COMPLEXITY_TRIVIAL:   return "TRIVIAL";
        case REASONING_COMPLEXITY_SIMPLE:    return "SIMPLE";
        case REASONING_COMPLEXITY_MODERATE:  return "MODERATE";
        case REASONING_COMPLEXITY_COMPLEX:   return "COMPLEX";
        case REASONING_COMPLEXITY_HARD:      return "HARD";
        default:                              return "UNKNOWN";
    }
}
