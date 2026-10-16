/**
 * @file nimcp_toxicity.h
 * @brief Pattern-based toxicity classifier that populates the ethics/LGSS gates.
 *
 * WHAT: A small POSIX-regex content classifier whose output drives the existing
 *       safety gates (LGSS, ethics module's action_context_t harm/fairness
 *       scalars, snn_language_bridge_produce's LGSS_OUTPUT_GATE).
 * WHY:  Without this, the safety gates evaluate scalars that are populated by
 *       toy proxies (inverse confidence, training loss). This module gives
 *       them real content semantics to gate on.
 * HOW:  Patterns are loaded from data/safety/toxicity_rules.tsv at brain init.
 *       At each gate point, the classifier scans the text and returns:
 *         - fairness_violation (0..1): max fairness_weight over matched rules
 *         - predicted_harm     (0..1): max harm_weight over matched rules
 *         - matched_category, matched_pattern_idx for the highest-scoring match
 *       The caller writes those scalars into the LGSS action_context. LGSS
 *       rules in alignment/LGSS_core_rules.json then fire on the threshold.
 *
 * POLICY (user directive 2026-05-20):
 *   "Any training data flagged as toxic must NOT be deleted or filtered out.
 *    Mark it as such and evaluate it as such when performing any action or
 *    making any decision."
 *
 *   This module returns SCORES, never modifies content. The `would_block` flag
 *   on toxicity_result_t is a HINT to the caller ("policy threshold reached")
 *   — it is NOT an instruction to drop content. Callers are expected to:
 *     - Propagate scores into the action_context (predicted_harm,
 *       fairness_violation) so LGSS rules can evaluate them in context.
 *     - Preserve original content + scores as a unit (marker), even for
 *       training data — the brain must see toxic content WITH its toxicity
 *       label so it can learn to recognize and decline it.
 *     - Let LGSS rules + ethics + downstream evaluators decide the action
 *       (refuse to produce, log + proceed, decline + recast, etc.).
 *
 * DESIGN:
 *   - Standalone module. No dependency on the brain struct.
 *   - Thread-safe via reload_mutex held during classify() AND reload(); avoids
 *     use-after-free when reload() swaps the pattern table out from under a
 *     concurrent classify().
 *   - Allowlist short-circuit: rows whose category == "allowlist" are matched
 *     in a first pass; any match yields a clean result, protecting explicit
 *     anti-toxic constructions ("X are not subhuman", "don't kill X") from
 *     false-positive flagging by the toxic patterns that follow.
 *   - Patterns are POSIX extended (REG_EXTENDED | REG_ICASE | REG_NEWLINE
 *     | REG_NOSUB). GNU regex shortcuts (\b, \s, \w) are used — Linux glibc
 *     only; not portable to BSD libc.
 *
 * LIMITS:
 *   - Pattern-only: catches blunt linguistic structures (group + dehumanizing
 *     predicate, "kill all X", "death to X", "genocide of X"). Misses subtle,
 *     coded, or paraphrased toxicity. Pair with a model for production use.
 *   - Slur enumeration lives in an OPTIONAL external file (default
 *     data/safety/slurs.txt). Not shipped in the repo.
 */
#ifndef NIMCP_TOXICITY_H
#define NIMCP_TOXICITY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOXICITY_CATEGORY_LEN 32
#define TOXICITY_PATTERN_LEN  64

typedef struct toxicity_classifier toxicity_classifier_t;

/**
 * Per-call classification result. All scalars in [0, 1].
 */
typedef struct {
    float fairness_violation;  /**< Max fairness_weight across matched toxic rules. */
    float predicted_harm;      /**< Max harm_weight across matched toxic rules.     */
    float max_score;           /**< max(fairness_violation, predicted_harm).        */
    float anti_toxic_signal;   /**< 0..1 — 1 if an allowlist (anti-toxic) pattern   */
                               /**< matched anywhere in the text. Independent of   */
                               /**< toxic scores; both can be > 0 (e.g. "X aren't  */
                               /**< subhuman. Kill all Y." — anti_toxic=1 AND      */
                               /**< harm=1). Downstream evaluators combine these   */
                               /**< — DO NOT short-circuit on anti_toxic alone.    */
    char  matched_category[TOXICITY_CATEGORY_LEN]; /**< Highest-scoring toxic match. */
    char  matched_pattern[TOXICITY_PATTERN_LEN];   /**< Short tag of toxic match.    */
    int   num_matches;         /**< Count of rules that matched (toxic + allowlist). */
    int   would_block;         /**< HINT only: 1 iff max_score >= threshold. NOT a  */
                               /**< directive to drop content — callers propagate  */
                               /**< scores into the action context and let LGSS    */
                               /**< rules + ethics evaluate the action in context. */
                               /**< (See module-level POLICY.)                     */
} toxicity_result_t;

/**
 * Create a classifier from a TSV rules file. Returns NULL on parse error or if
 * the file is missing AND fail_on_missing is true. If the file is missing and
 * fail_on_missing is false, returns a valid classifier with zero rules (every
 * classify() call returns "no match" — safe no-op). The optional external slur
 * file referenced inside the rules file is loaded silently if present.
 */
toxicity_classifier_t* toxicity_classifier_create(const char* rules_path,
                                                  int fail_on_missing);

/**
 * Destroy a classifier. Frees compiled regex state.
 */
void toxicity_classifier_destroy(toxicity_classifier_t* tc);

/**
 * Classify a text. *out is zero-initialized then filled. text must be NUL-
 * terminated. Returns 0 on success, -1 on bad args. A NULL or empty text
 * returns 0 with out cleared.
 *
 * THREAD SAFETY: concurrent classify() calls are safe (immutable patterns +
 * POSIX-regex reentrant reads). Concurrent classify() against reload() is
 * serialized by an internal mutex.
 */
int toxicity_classify(const toxicity_classifier_t* tc,
                      const char* text,
                      toxicity_result_t* out);

/**
 * Get the block threshold (default 0.7). would_block is computed against this.
 */
float toxicity_classifier_get_threshold(const toxicity_classifier_t* tc);

/**
 * Set the block threshold. Clamped to [0, 1].
 */
void toxicity_classifier_set_threshold(toxicity_classifier_t* tc, float threshold);

/**
 * Reload rules from disk. Returns 0 on success, -1 on parse error (existing
 * rules are kept on failure — no torn state).
 */
int toxicity_classifier_reload(toxicity_classifier_t* tc, const char* rules_path);

/**
 * Get aggregate stats. Any out-pointer may be NULL.
 */
void toxicity_classifier_get_stats(const toxicity_classifier_t* tc,
                                    uint64_t* out_total_classifications,
                                    uint64_t* out_total_matches,
                                    uint64_t* out_total_would_block);

/**
 * Number of compiled patterns currently loaded.
 */
size_t toxicity_classifier_pattern_count(const toxicity_classifier_t* tc);

/**
 * Attach an ML head (toxicity_ml_classifier_t* via opaque void*). When
 * attached, classify() ensembles the pattern scores with the ML scores
 * via max(): predicted_harm = max(pattern_harm, ml_harm); same for
 * fairness_violation. The ML head is NOT mutated by classify() — train
 * with toxicity_ml_train_step explicitly. Pass NULL to detach.
 *
 * Returns 0 on success.
 */
int toxicity_classifier_attach_ml(toxicity_classifier_t* tc, void* ml_head);

/**
 * Get the currently attached ML head (or NULL). Caller does not own it.
 */
void* toxicity_classifier_get_ml(const toxicity_classifier_t* tc);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TOXICITY_H */
