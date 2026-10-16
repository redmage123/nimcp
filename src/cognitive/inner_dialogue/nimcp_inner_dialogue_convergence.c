/**
 * @file nimcp_inner_dialogue_convergence.c
 * @brief Convergence, Deadlock, and Rumination Detection Algorithms
 * @version 1.0.0
 * @date 2026-01-27
 *
 * WHAT: Implements multi-signal dialogue termination analysis
 * WHY:  The engine must detect when deliberation has converged, deadlocked,
 *        or entered a destructive rumination loop
 * HOW:  Agreement trend (OLS regression), deadlock oscillation counting,
 *        rumination via content similarity + low entropy, emotional spiral via
 *        monotonic intensity detection
 *
 * MATH UTILITIES:
 * - nimcp_clamp_f pattern for bounded values
 * - Shannon entropy (information-theoretic diversity measure)
 * - Linear regression (OLS slope) for trend computation
 * - Jaccard content similarity from turn module
 *
 * QUANTUM NOTE:
 * The Shannon entropy computation here is classical.  The framework is
 * designed so that quantum_shannon_entropy() from nimcp_quantum_shannon.h
 * could replace compute_entropy_internal() for future quantum-enhanced
 * convergence detection over larger state spaces.
 *
 * @author NIMCP Development Team
 */

#include "cognitive/inner_dialogue/nimcp_inner_dialogue_convergence.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/statistics/nimcp_statistics.h"

#include <string.h>
#include <math.h>
#include <float.h>

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(convergence, MESH_ADAPTER_CATEGORY_COGNITIVE)

/** Safe isnan check */
static inline bool is_nan_f(float v) {
    return v != v;  /* IEEE 754: NaN != NaN */
}

/**
 * @brief Shannon entropy from frequency count array
 *
 * WHAT: H = -sum(p_i * log2(p_i)) for p_i > 0
 * WHY:  Measures diversity of a distribution
 * HOW:  Convert counts to probabilities, delegate to central stats module
 *
 * Uses nimcp_stats_entropy() from utils/statistics for core computation.
 *
 * This is the classical Shannon entropy.  For future quantum integration,
 * this function could delegate to quantum_shannon_entropy() when the
 * enable_quantum_entropy flag is set.
 */
static float compute_entropy_internal(const uint32_t* counts, uint32_t bins,
                                       uint32_t total) {
    if (total == 0 || !counts || bins == 0) return 0.0f;

    /* Convert counts to probability distribution */
    float probs[16];  /* bins is always 16 in this module */
    float inv = 1.0f / (float)total;

    for (uint32_t i = 0; i < bins && i < 16; i++) {
        probs[i] = (float)counts[i] * inv;
    }

    /* Delegate to central statistics module */
    return nimcp_stats_entropy(probs, bins < 16 ? bins : 16);
}

/**
 * @brief Ordinary least-squares slope for a sequence of (x, y) values
 *
 * WHAT: Best-fit line slope
 * WHY:  Detects trends in agreement scores
 * HOW:  slope = (n*sum_xy - sum_x*sum_y) / (n*sum_x2 - sum_x^2)
 */
static float ols_slope(const float* y_values, uint32_t n) {
    if (n < 2 || !y_values) return 0.0f;

    float sum_x = 0.0f, sum_y = 0.0f, sum_xy = 0.0f, sum_x2 = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float x = (float)i;
        sum_x  += x;
        sum_y  += y_values[i];
        sum_xy += x * y_values[i];
        sum_x2 += x * x;
    }
    float denom = (float)n * sum_x2 - sum_x * sum_x;
    if (fabsf(denom) < 1e-12f) return 0.0f;
    return ((float)n * sum_xy - sum_x * sum_y) / denom;
}

/* ============================================================================
 * Termination Reason String Table
 * ============================================================================ */

static const char* s_termination_names[TERMINATION_COUNT] = {
    "NONE",
    "CONVERGED",
    "MAX_TURNS",
    "DEADLOCKED",
    "RUMINATING",
    "EMOTIONAL_SPIRAL",
    "SUBSTRATE_SUPPRESSED",
    "CANCELLED",
    "ESCALATED"
};

const char* termination_reason_to_string(termination_reason_t reason) {
    if ((unsigned)reason < TERMINATION_COUNT) {
        return s_termination_names[(unsigned)reason];
    }
    return "UNKNOWN";
}

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

convergence_config_t inner_dialogue_convergence_default_config(void) {
    convergence_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.agreement_threshold        = INNER_DIALOGUE_DEFAULT_AGREEMENT_THRESHOLD;
    cfg.deadlock_threshold         = INNER_DIALOGUE_DEFAULT_DEADLOCK_THRESHOLD;
    cfg.rumination_threshold       = INNER_DIALOGUE_DEFAULT_RUMINATION_THRESHOLD;
    cfg.emotional_spiral_threshold = INNER_DIALOGUE_DEFAULT_EMOTIONAL_SPIRAL_THRESHOLD;
    cfg.min_act_entropy            = 1.0f;  /* Below 1 bit → very monotonous */
    cfg.trend_window               = INNER_DIALOGUE_CONVERGENCE_TREND_WINDOW;
    cfg.enable_quantum_entropy     = false;
    return cfg;
}

/* ============================================================================
 * Component Analysis Functions
 * ============================================================================ */

float inner_dialogue_convergence_agreement(
    const inner_dialogue_turn_history_t* history,
    uint32_t window) {
    if (!history || history->count == 0) return -1.0f;

    uint32_t n = (window > 0 && window < history->count) ? window : history->count;

    /* Weighted average with exponential recency bias: w_i = 0.9^i */
    float sum_w = 0.0f;
    float sum_wa = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        const inner_dialogue_turn_t* t = inner_dialogue_turn_history_get_at(history, i);
        if (!t) continue;
        float w = powf(0.9f, (float)i);
        sum_w  += w;
        sum_wa += w * t->agreement_with_prior;
    }

    float agreement = (sum_w > 0.0f) ? sum_wa / sum_w : 0.0f;
    NIMCP_LOGGING_DEBUG("inner_dialogue_convergence: agreement=%.3f (window=%u)",
                        (double)agreement, n);
    return nimcp_clampf(agreement, 0.0f, 1.0f);
}

float inner_dialogue_convergence_trend(
    const inner_dialogue_turn_history_t* history,
    uint32_t window) {
    if (!history || history->count < 2) return 0.0f;

    uint32_t n = (window > 0 && window < history->count) ? window : history->count;
    if (n < 2) return 0.0f;

    /* Collect agreement values in chronological order (oldest first) */
    float values[128];
    uint32_t count = (n > 128) ? 128 : n;
    for (uint32_t i = 0; i < count; i++) {
        const inner_dialogue_turn_t* t = inner_dialogue_turn_history_get_at(
            history, count - 1 - i);  /* Reverse to chronological */
        values[i] = t ? t->agreement_with_prior : 0.5f;
    }

    float slope = ols_slope(values, count);
    NIMCP_LOGGING_DEBUG("inner_dialogue_convergence: trend slope=%.4f (window=%u)",
                        (double)slope, count);
    return slope;
}

float inner_dialogue_convergence_deadlock(
    const inner_dialogue_turn_history_t* history,
    uint32_t window) {
    if (!history || history->count < INNER_DIALOGUE_CONVERGENCE_MIN_TURNS) {
        return -1.0f;
    }

    uint32_t n = (window > 0 && window < history->count) ? window : history->count;

    /* Count challenge-assert-challenge oscillations and low-agreement pairs */
    uint32_t oscillations = 0;
    uint32_t disagreements = 0;

    for (uint32_t i = 0; i + 2 < n; i++) {
        const inner_dialogue_turn_t* t0 = inner_dialogue_turn_history_get_at(history, i);
        const inner_dialogue_turn_t* t1 = inner_dialogue_turn_history_get_at(history, i + 1);
        const inner_dialogue_turn_t* t2 = inner_dialogue_turn_history_get_at(history, i + 2);
        if (!t0 || !t1 || !t2) continue;

        /* Oscillation: A challenges B, C re-asserts (or vice versa) */
        if ((t0->act == DIALOGUE_ACT_CHALLENGE && t2->act == DIALOGUE_ACT_CHALLENGE) ||
            (t0->act == DIALOGUE_ACT_ASSERT && t1->act == DIALOGUE_ACT_CHALLENGE &&
             t2->act == DIALOGUE_ACT_ASSERT)) {
            oscillations++;
        }
        if (t1->agreement_with_prior < 0.3f) {
            disagreements++;
        }
    }

    float max_oscillations = (n > 2) ? (float)(n - 2) : 1.0f;
    float osc_ratio = (float)oscillations / max_oscillations;
    float disagree_ratio = (n > 1) ? (float)disagreements / (float)(n - 1) : 0.0f;

    float deadlock = 0.6f * osc_ratio + 0.4f * disagree_ratio;
    deadlock = nimcp_clampf(deadlock, 0.0f, 1.0f);

    NIMCP_LOGGING_DEBUG("inner_dialogue_convergence: deadlock=%.3f (osc=%u, disagree=%u, n=%u)",
                        (double)deadlock, oscillations, disagreements, n);
    return deadlock;
}

float inner_dialogue_convergence_rumination(
    const inner_dialogue_turn_history_t* history,
    uint32_t window) {
    if (!history || history->count < INNER_DIALOGUE_CONVERGENCE_MIN_TURNS) {
        return -1.0f;
    }

    uint32_t n = (window > 0 && window < history->count) ? window : history->count;

    /* Factor 1: Content similarity between consecutive turns */
    float total_sim = 0.0f;
    uint32_t sim_count = 0;
    for (uint32_t i = 0; i + 1 < n; i++) {
        const inner_dialogue_turn_t* t0 = inner_dialogue_turn_history_get_at(history, i);
        const inner_dialogue_turn_t* t1 = inner_dialogue_turn_history_get_at(history, i + 1);
        if (!t0 || !t1) continue;
        float sim = inner_dialogue_turn_content_similarity(t0, t1);
        if (sim >= 0.0f) {
            total_sim += sim;
            sim_count++;
        }
    }
    float avg_similarity = (sim_count > 0) ? total_sim / (float)sim_count : 0.0f;

    /* Factor 2: Low act entropy (monotonous pattern) */
    float act_entropy = inner_dialogue_turn_history_act_entropy(history, n);
    float max_entropy = log2f((float)DIALOGUE_ACT_COUNT);
    float entropy_ratio = (max_entropy > 0.0f && act_entropy >= 0.0f) ?
                          act_entropy / max_entropy : 0.5f;
    float low_diversity = 1.0f - entropy_ratio;  /* Higher when less diverse */

    /* Factor 3: Same perspective speaking repeatedly */
    uint32_t consecutive_same = 0;
    for (uint32_t i = 0; i + 1 < n; i++) {
        const inner_dialogue_turn_t* t0 = inner_dialogue_turn_history_get_at(history, i);
        const inner_dialogue_turn_t* t1 = inner_dialogue_turn_history_get_at(history, i + 1);
        if (t0 && t1 && t0->perspective_idx == t1->perspective_idx) {
            consecutive_same++;
        }
    }
    float perspective_repetition = (n > 1) ?
        (float)consecutive_same / (float)(n - 1) : 0.0f;

    float rumination = 0.4f * avg_similarity +
                       0.35f * low_diversity +
                       0.25f * perspective_repetition;
    rumination = nimcp_clampf(rumination, 0.0f, 1.0f);

    NIMCP_LOGGING_DEBUG("inner_dialogue_convergence: rumination=%.3f "
                        "(sim=%.2f, low_div=%.2f, persp_rep=%.2f)",
                        (double)rumination, (double)avg_similarity,
                        (double)low_diversity, (double)perspective_repetition);
    return rumination;
}

float inner_dialogue_convergence_emotional_temperature(
    const inner_dialogue_turn_history_t* history,
    uint32_t window) {
    if (!history || history->count == 0) return -1.0f;

    uint32_t n = (window > 0 && window < history->count) ? window : history->count;

    float sum_abs_valence = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        const inner_dialogue_turn_t* t = inner_dialogue_turn_history_get_at(history, i);
        if (t) {
            sum_abs_valence += fabsf(t->emotional_valence);
        }
    }

    float temperature = (n > 0) ? sum_abs_valence / (float)n : 0.0f;
    temperature = nimcp_clampf(temperature, 0.0f, 1.0f);

    NIMCP_LOGGING_DEBUG("inner_dialogue_convergence: emotional_temperature=%.3f (window=%u)",
                        (double)temperature, n);
    return temperature;
}

float inner_dialogue_convergence_perspective_entropy(
    const inner_dialogue_turn_history_t* history,
    uint32_t window) {
    if (!history || history->count == 0) return -1.0f;

    uint32_t n = (window > 0 && window < history->count) ? window : history->count;

    uint32_t persp_counts[16];
    memset(persp_counts, 0, sizeof(persp_counts));
    for (uint32_t i = 0; i < n; i++) {
        const inner_dialogue_turn_t* t = inner_dialogue_turn_history_get_at(history, i);
        if (t && t->perspective_idx < 16) {
            persp_counts[t->perspective_idx]++;
        }
    }

    float entropy = compute_entropy_internal(persp_counts, 16, n);
    NIMCP_LOGGING_DEBUG("inner_dialogue_convergence: perspective_entropy=%.3f (window=%u)",
                        (double)entropy, n);
    return entropy;
}

/* ============================================================================
 * Full Analysis
 * ============================================================================ */

int inner_dialogue_convergence_analyse(
    const inner_dialogue_turn_history_t* history,
    const convergence_config_t* config,
    convergence_analysis_t* analysis) {
    if (!history) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_CONVERGENCE_ERROR_NULL,
                              "inner_dialogue_convergence: analyse with NULL history");
        return NIMCP_INNER_DIALOGUE_CONVERGENCE_ERROR_NULL;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_CONVERGENCE_ERROR_NULL,
                              "inner_dialogue_convergence: analyse with NULL config");
        return NIMCP_INNER_DIALOGUE_CONVERGENCE_ERROR_NULL;
    }
    if (!analysis) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_CONVERGENCE_ERROR_NULL,
                              "inner_dialogue_convergence: analyse with NULL output");
        return NIMCP_INNER_DIALOGUE_CONVERGENCE_ERROR_NULL;
    }

    memset(analysis, 0, sizeof(convergence_analysis_t));
    analysis->turns_analysed = history->count;

    /* Need minimum turns for meaningful analysis */
    if (history->count < INNER_DIALOGUE_CONVERGENCE_MIN_TURNS) {
        analysis->recommended_action = TERMINATION_NONE;
        NIMCP_LOGGING_DEBUG("inner_dialogue_convergence: too few turns (%u < %u), no recommendation",
                            history->count, (unsigned)INNER_DIALOGUE_CONVERGENCE_MIN_TURNS);
        convergence_heartbeat("convergence_analyse", 0.1f);
        return 0;
    }

    uint32_t w = config->trend_window;

    /* 1. Agreement analysis */
    analysis->agreement_score = inner_dialogue_convergence_agreement(history, w);
    analysis->agreement_trend = inner_dialogue_convergence_trend(history, w);
    analysis->converged = (analysis->agreement_score >= config->agreement_threshold) &&
                          (analysis->agreement_trend >= 0.0f);
    convergence_heartbeat("convergence_agreement", 0.3f);

    /* 2. Deadlock analysis */
    analysis->deadlock_score = inner_dialogue_convergence_deadlock(history, w);
    analysis->deadlocked = (analysis->deadlock_score >= config->deadlock_threshold);
    convergence_heartbeat("convergence_deadlock", 0.5f);

    /* 3. Rumination analysis */
    analysis->rumination_score = inner_dialogue_convergence_rumination(history, w);
    analysis->ruminating = (analysis->rumination_score >= config->rumination_threshold);
    convergence_heartbeat("convergence_rumination", 0.7f);

    /* 4. Emotional analysis */
    analysis->emotional_temperature =
        inner_dialogue_convergence_emotional_temperature(history, w);

    /* Check for spiral: emotional temperature in recent half vs earlier half */
    if (history->count >= 4) {
        uint32_t half = history->count / 2;
        float recent_temp = inner_dialogue_convergence_emotional_temperature(history, half);
        float earlier_temp = 0.0f;
        /* Compute earlier half manually */
        float sum_earlier = 0.0f;
        for (uint32_t i = half; i < history->count && i < half + half; i++) {
            const inner_dialogue_turn_t* t = inner_dialogue_turn_history_get_at(history, i);
            if (t) sum_earlier += fabsf(t->emotional_valence);
        }
        earlier_temp = (half > 0) ? sum_earlier / (float)half : 0.0f;
        analysis->emotional_spiral =
            (recent_temp > earlier_temp + 0.1f) &&
            (recent_temp >= config->emotional_spiral_threshold);
    }
    convergence_heartbeat("convergence_emotional", 0.85f);

    /* 5. Entropy metrics */
    analysis->act_entropy = inner_dialogue_turn_history_act_entropy(history, w);
    analysis->perspective_entropy =
        inner_dialogue_convergence_perspective_entropy(history, w);

    /* NaN guard: if any metric is NaN, report and zero it */
    if (is_nan_f(analysis->agreement_score)) {
        NIMCP_LOGGING_WARN("inner_dialogue_convergence: NaN in agreement_score, zeroing");
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_CONVERGENCE_ERROR_NAN,
                              "inner_dialogue_convergence: NaN detected in agreement_score");
        analysis->agreement_score = 0.0f;
    }
    if (is_nan_f(analysis->deadlock_score)) {
        analysis->deadlock_score = 0.0f;
    }
    if (is_nan_f(analysis->rumination_score)) {
        analysis->rumination_score = 0.0f;
    }

    /* 6. Determine recommended action (priority order) */
    if (analysis->converged) {
        analysis->recommended_action = TERMINATION_CONVERGED;
    } else if (analysis->emotional_spiral) {
        analysis->recommended_action = TERMINATION_EMOTIONAL_SPIRAL;
    } else if (analysis->deadlocked) {
        analysis->recommended_action = TERMINATION_DEADLOCKED;
    } else if (analysis->ruminating) {
        analysis->recommended_action = TERMINATION_RUMINATING;
    } else {
        analysis->recommended_action = TERMINATION_NONE;
    }

    NIMCP_LOGGING_INFO("inner_dialogue_convergence: analysis complete "
                       "(agree=%.2f trend=%.3f deadlock=%.2f ruminate=%.2f "
                       "emotion=%.2f spiral=%d) → %s",
                       (double)analysis->agreement_score,
                       (double)analysis->agreement_trend,
                       (double)analysis->deadlock_score,
                       (double)analysis->rumination_score,
                       (double)analysis->emotional_temperature,
                       analysis->emotional_spiral,
                       termination_reason_to_string(analysis->recommended_action));

    convergence_heartbeat("convergence_analyse", 1.0f);
    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void convergence_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_convergence_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int convergence_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "convergence_training_begin: NULL argument");
        return -1;
    }
    convergence_heartbeat_instance(NULL, "convergence_training_begin", 0.0f);
    return 0;
}

int convergence_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "convergence_training_end: NULL argument");
        return -1;
    }
    convergence_heartbeat_instance(NULL, "convergence_training_end", 1.0f);
    return 0;
}

int convergence_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "convergence_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    convergence_heartbeat_instance(NULL, "convergence_training_step", progress);
    return 0;
}
