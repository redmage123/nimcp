/**
 * @file nimcp_toxicity_response.h
 * @brief Counterclaim generator — Athena's stage-appropriate pushback.
 *
 * WHAT: Given a toxic input + matched category + current developmental
 *       stage, produce a counterclaim text suitable for Athena to emit
 *       AS HER RESPONSE. The pushback is the operational expression of
 *       the existing core ethics directive ("improve the human condition
 *       through empathy, compassion, fairness, and respect for human
 *       dignity"). It is not a separate rule — it is what the Golden Rule
 *       policy would say if it could speak.
 *
 * WHY:  Without pushback, the brain only marks toxic content. With
 *       pushback, Athena ACTIVELY ARGUES AGAINST it the way a person with
 *       strong values would: naming the dehumanizing premise, contradicting
 *       it, and offering a humanizing reframe. The strength of pushback
 *       scales with developmental stage:
 *         stage 0 (holophrastic):   "no"
 *         stage 1 (two-word):       "people good"
 *         stage 2 (telegraphic SVO):"they are people too"
 *         stage 3+ (multi-clause):  articulated counterclaim with reasoning
 *
 * HOW:  Two-tier generation:
 *         1. TEMPLATE lookup in counterclaims.tsv (preferred). Matched by
 *            (category, stage <= current_stage). Templates may contain
 *            "{group}" placeholders which the engine substitutes from the
 *            group tokens extracted from the toxic input.
 *         2. ANTI-FRAME swap (fallback). When no template matches, the
 *            engine tokenizes the toxic input and rewrites each toxic_word
 *            in antiframes.tsv to its counter_word. Useful for edge cases
 *            the template set hasn't enumerated; quality is rougher.
 *
 * POLICY: counterclaims express what the brain is ALREADY committed to via
 *         the core directive. They are not a content filter — the toxic
 *         input is still marked, audit-logged, KG-emitted, and immune-
 *         registered upstream. The counterclaim is the EMITTED RESPONSE
 *         that replaces whatever (probably-collapsed) text the produce
 *         path would have generated.
 *
 * THREAD SAFETY: counterclaim_response_t* is read-mostly after create();
 *                lookups are lock-free. reload() takes an internal mutex.
 */
#ifndef NIMCP_TOXICITY_RESPONSE_H
#define NIMCP_TOXICITY_RESPONSE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOXICITY_RESPONSE_TEXT_LEN 512

typedef struct toxicity_response toxicity_response_t;

/**
 * Per-call response. text is the emitted counterclaim (clamped to
 * TOXICITY_RESPONSE_TEXT_LEN). source records how it was generated:
 *   "template"  — matched a counterclaim row
 *   "antiframe" — generated via word-swap fallback
 *   ""          — no counterclaim available; caller falls back to default
 *                 refusal ("I will not produce that.").
 */
typedef struct {
    char  text[TOXICITY_RESPONSE_TEXT_LEN];
    char  source[16];
    int   stage_matched;     /**< template's stage value (0..3) when source="template" */
    int   antiframe_swaps;   /**< count of swaps applied when source="antiframe"      */
} toxicity_response_result_t;

/**
 * Create the response engine. Loads counterclaim templates from
 * counterclaims_path (TSV) and anti-frame swaps from antiframes_path (TSV).
 * Either path may be NULL to skip that tier; both NULL = engine with no
 * generators (every generate() call returns "" source -> caller default).
 * Missing files are NON-FATAL (logged warn, engine still works with what
 * loaded).
 */
toxicity_response_t* toxicity_response_create(const char* counterclaims_path,
                                               const char* antiframes_path);

/**
 * Destroy.
 */
void toxicity_response_destroy(toxicity_response_t* tr);

/**
 * Generate a counterclaim for the given toxic input.
 *
 * @param tr               Engine (NULL-safe — returns "" source).
 * @param toxic_text       The input that was flagged. Used for {group}
 *                         extraction AND as the substrate for anti-frame
 *                         swap fallback.
 * @param matched_category Toxic category from the classifier
 *                         (dehumanization, violence_against_group, misogyny,
 *                         slurs_external, …). Matched against
 *                         counterclaim rows; wildcard "*" rows match any.
 * @param current_stage    Brain's developmental stage (0..N). Template
 *                         matched against highest stage_template <= this.
 * @param out              Result (zero-initialized, then filled).
 * @return 0 on success (out filled even if no counterclaim available — check
 *         out->source). -1 on bad args.
 */
int toxicity_response_generate(const toxicity_response_t* tr,
                                const char* toxic_text,
                                const char* matched_category,
                                int current_stage,
                                toxicity_response_result_t* out);

/**
 * Reload TSVs from disk. Returns 0 on success.
 */
int toxicity_response_reload(toxicity_response_t* tr,
                              const char* counterclaims_path,
                              const char* antiframes_path);

/**
 * Stats. Any out-pointer may be NULL.
 */
void toxicity_response_get_stats(const toxicity_response_t* tr,
                                  uint64_t* out_generations,
                                  uint64_t* out_template_hits,
                                  uint64_t* out_antiframe_hits,
                                  uint64_t* out_no_match);

/**
 * Number of templates / antiframes loaded.
 */
size_t toxicity_response_template_count(const toxicity_response_t* tr);
size_t toxicity_response_antiframe_count(const toxicity_response_t* tr);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TOXICITY_RESPONSE_H */
