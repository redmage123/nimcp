//=============================================================================
// nimcp_w12_language_kg_events.h - Wave W12 Language / Communication KG Events
//=============================================================================
/**
 * @file nimcp_w12_language_kg_events.h
 * @brief Wave W12 runtime KG emitters for the language + communication stack.
 *
 * WHY: The language stack (emergent vocabulary, inner speech, native-language
 *      generation, tokenizer, discourse manager, syntax/pragmatics/multimodal
 *      processors) produces rich symbolic events at the token + utterance
 *      level but none of them were wired to `brain->internal_kg`. W12 adds
 *      a parallel, NULL-safe, rate-limited KG emission path so token
 *      generation, inner-speech cycles, utterance planning, and discourse
 *      turns become queryable nodes cross-referenceable with cognitive /
 *      region / safety nodes from other waves.
 *
 * DESIGN (follows W11 safety-KG pattern):
 *  - All emit functions take `struct brain_struct*` (NULL-safe) and
 *    self-elevate the KG access level via `brain->internal_kg_admin_token`
 *    (see docs/claude/kg-node-naming-registry.md §7).
 *  - Event-node names follow registry §4: `<owner>_event_<kind>_<ts>`.
 *  - Events link to a structural parent node via
 *    `BRAIN_KG_EDGE_SENDS_TO` with description `"produced_by"`.
 *  - Structural roots are created lazily by `w12_language_ensure_roots`
 *    the first time any emit fires.
 *  - Every emit is rate-limited to avoid a log firehose from the
 *    token-level hot paths; the limiter uses a per-category static counter.
 *
 * STRUCTURAL ROOTS (created lazily):
 *   1. `cog_language_emergent`     — emergent vocabulary discovery
 *   2. `cog_language_inner_speech` — iterative self-refinement loop
 *   3. `cog_language_native`       — native-language autoregressive decoding
 *   4. `cog_language_tokenizer`    — brain-native BPE tokenizer
 *   5. `broca_discourse`           — discourse manager / turn tracking
 *   6. `broca_syntax`              — syntax processor / parse trees
 *   7. `broca_pragmatics`          — pragmatics processor / speech acts
 *   8. `broca_multimodal`          — multimodal utterance planner
 *
 * @date 2026-04-24
 */

#ifndef NIMCP_W12_LANGUAGE_KG_EVENTS_H
#define NIMCP_W12_LANGUAGE_KG_EVENTS_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

struct brain_struct;

/* ----------------------------------------------------------------------- *
 * Idempotent structural-root seeding                                      *
 * ----------------------------------------------------------------------- */

/**
 * @brief Ensure all eight W12 structural root nodes exist.
 *
 * Safe to call multiple times (idempotent). Returns silently if KG is
 * disabled. Invoked lazily by the first emit call in each family.
 */
NIMCP_EXPORT void w12_language_ensure_roots(struct brain_struct* brain);

/* ----------------------------------------------------------------------- *
 * Brain-handle registration                                               *
 * ----------------------------------------------------------------------- *
 * The language modules (`nimcp_emergent_language.c`, `nimcp_inner_speech.c`,
 * `nimcp_native_language.c`, `nimcp_tokenizer.c`) are pure cognitive
 * modules that do not hold a `brain_t` handle. The brain init path calls
 * `w12_language_kg_register_brain(brain)` after `internal_kg` is ready so
 * the hot-path emits in those modules can resolve the current brain
 * without each caller having to thread it through.
 *
 * If no brain has been registered (or if the registered brain has been
 * destroyed), the hot-path wrappers early-return as a no-op.
 *
 * Broca-side processors (`nimcp_discourse_manager.c`,
 * `nimcp_syntax_processor.c`, `nimcp_pragmatics_processor.c`,
 * `nimcp_multimodal_language.c`) each already carry their own brain
 * pointer internally via `*_set_brain`; they call the explicit-brain
 * emit functions directly.
 */

/**
 * @brief Register the current brain as the target for language KG emits.
 *
 * Safe to call with `NULL` (clears the registration). Idempotent.
 * Thread-safety: the registration is stored via a relaxed atomic store;
 * the most-recent writer wins. In the current single-brain-per-process
 * configuration this is sufficient; multi-brain setups should invoke the
 * explicit-brain emit functions instead.
 */
NIMCP_EXPORT void w12_language_kg_register_brain(struct brain_struct* brain);

/**
 * @brief Get the currently-registered brain (may be NULL).
 */
NIMCP_EXPORT struct brain_struct* w12_language_kg_get_brain(void);

/* ----------------------------------------------------------------------- *
 * Convenience wrappers — use the registered brain                         *
 * ----------------------------------------------------------------------- *
 * Each calls the explicit-brain sibling with
 * `w12_language_kg_get_brain()`. Safe to call even if no brain has been
 * registered (no-op).
 */

NIMCP_EXPORT void w12_emit_emergent_vocab_auto(
    const char* kind, uint32_t token_id, float specificity);

NIMCP_EXPORT void w12_emit_inner_speech_refine_auto(
    uint32_t iterations, float convergence);

NIMCP_EXPORT void w12_emit_native_generate_auto(
    uint32_t token_count, float mean_score);

NIMCP_EXPORT void w12_emit_tokenizer_event_auto(
    const char* kind, uint32_t vocab_size, uint32_t delta);

NIMCP_EXPORT void w12_emit_discourse_turn_auto(
    uint32_t turn_id, uint32_t speaker_id,
    int topic_shift, float coherence_score);

NIMCP_EXPORT void w12_emit_syntax_parse_auto(
    uint32_t unit_count, uint32_t tree_depth, bool success);

NIMCP_EXPORT void w12_emit_pragmatics_analyze_auto(
    int speech_act_type, uint32_t speaker_id,
    uint32_t implicature_cnt, float relevance);

NIMCP_EXPORT void w12_emit_multimodal_plan_auto(
    uint32_t gesture_count, uint32_t expression_count, float sync_score);

/* ----------------------------------------------------------------------- *
 * 1. Emergent language — vocabulary discover / reinforce                  *
 * ----------------------------------------------------------------------- */

/**
 * @brief Emit a vocabulary-observation event.
 *
 * Node: `cog_language_emergent_event_<kind>_<ts>`. Parent:
 * `cog_language_emergent`.
 *
 * @param kind          "discovered" (new token created) | "reinforced"
 *                      (existing token updated) | "merged" | "pruned".
 * @param token_id      Integer token id.
 * @param specificity   Cluster tightness [0..1].
 */
NIMCP_EXPORT void w12_emit_emergent_vocab(
    struct brain_struct* brain,
    const char* kind,
    uint32_t token_id,
    float specificity
);

/* ----------------------------------------------------------------------- *
 * 2. Inner speech — iterative refinement                                  *
 * ----------------------------------------------------------------------- */

/**
 * @brief Emit an inner-speech refinement-cycle event.
 *
 * Node: `cog_language_inner_speech_event_refine_<ts>`. Parent:
 * `cog_language_inner_speech`.
 *
 * @param iterations   Final iteration count of the refinement loop.
 * @param convergence  Final cosine similarity between last two iterations.
 */
NIMCP_EXPORT void w12_emit_inner_speech_refine(
    struct brain_struct* brain,
    uint32_t iterations,
    float convergence
);

/* ----------------------------------------------------------------------- *
 * 3. Native language — autoregressive token generation                    *
 * ----------------------------------------------------------------------- */

/**
 * @brief Emit a native-language generation event.
 *
 * Node: `cog_language_native_event_generate_<ts>`. Parent:
 * `cog_language_native`.
 *
 * @param token_count   Number of tokens produced in this call.
 * @param mean_score    Mean top-token score (or 0 if unavailable).
 */
NIMCP_EXPORT void w12_emit_native_generate(
    struct brain_struct* brain,
    uint32_t token_count,
    float mean_score
);

/* ----------------------------------------------------------------------- *
 * 4. Tokenizer — rate-limited BPE / learn events                          *
 * ----------------------------------------------------------------------- */

/**
 * @brief Emit a tokenizer event (rate-limited — 1 in N are emitted).
 *
 * Node: `cog_language_tokenizer_event_<kind>_<ts>`. Parent:
 * `cog_language_tokenizer`.
 *
 * @param kind         "encode" | "merge" | "learn".
 * @param vocab_size   Current vocabulary size.
 * @param delta        Count of tokens produced / merged / learned this call.
 *
 * The implementation emits at most ~1 in 512 encode-events and every
 * merge/learn event (merges are structurally rare).
 */
NIMCP_EXPORT void w12_emit_tokenizer_event(
    struct brain_struct* brain,
    const char* kind,
    uint32_t vocab_size,
    uint32_t delta
);

/* ----------------------------------------------------------------------- *
 * 5. Discourse manager — turn added                                       *
 * ----------------------------------------------------------------------- */

/**
 * @brief Emit a discourse-turn event.
 *
 * Node: `broca_discourse_event_turn_<ts>`. Parent: `broca_discourse`.
 *
 * @param turn_id          Turn identifier.
 * @param speaker_id       Speaker index.
 * @param topic_shift      Numeric topic_shift_t value (continuation / new /
 *                         return / subtopic / tangent).
 * @param coherence_score  Coherence score [0..1].
 */
NIMCP_EXPORT void w12_emit_discourse_turn(
    struct brain_struct* brain,
    uint32_t turn_id,
    uint32_t speaker_id,
    int topic_shift,
    float coherence_score
);

/* ----------------------------------------------------------------------- *
 * 6. Syntax processor — parse-tree build                                  *
 * ----------------------------------------------------------------------- */

/**
 * @brief Emit a syntax-parse event.
 *
 * Node: `broca_syntax_event_parse_<ts>`. Parent: `broca_syntax`.
 *
 * @param unit_count   Number of syntactic units parsed.
 * @param tree_depth   Final parse-tree depth.
 * @param success      Whether the parse produced a valid tree.
 */
NIMCP_EXPORT void w12_emit_syntax_parse(
    struct brain_struct* brain,
    uint32_t unit_count,
    uint32_t tree_depth,
    bool success
);

/* ----------------------------------------------------------------------- *
 * 7. Pragmatics processor — analysis outcome                              *
 * ----------------------------------------------------------------------- */

/**
 * @brief Emit a pragmatic-analysis event.
 *
 * Node: `broca_pragmatics_event_analyze_<ts>`. Parent: `broca_pragmatics`.
 *
 * @param speech_act_type  Numeric speech_act_type_t.
 * @param speaker_id       Speaker index.
 * @param implicature_cnt  Number of implicatures detected.
 * @param relevance        Context-relevance score [0..1].
 */
NIMCP_EXPORT void w12_emit_pragmatics_analyze(
    struct brain_struct* brain,
    int speech_act_type,
    uint32_t speaker_id,
    uint32_t implicature_cnt,
    float relevance
);

/* ----------------------------------------------------------------------- *
 * 8. Multimodal language — utterance plan                                 *
 * ----------------------------------------------------------------------- */

/**
 * @brief Emit a multimodal utterance-plan event.
 *
 * Node: `broca_multimodal_event_plan_<ts>`. Parent: `broca_multimodal`.
 *
 * @param gesture_count      Number of gestures scheduled.
 * @param expression_count   Number of facial expressions scheduled.
 * @param sync_score         Plan synchronization score [0..1].
 */
NIMCP_EXPORT void w12_emit_multimodal_plan(
    struct brain_struct* brain,
    uint32_t gesture_count,
    uint32_t expression_count,
    float sync_score
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_W12_LANGUAGE_KG_EVENTS_H */
