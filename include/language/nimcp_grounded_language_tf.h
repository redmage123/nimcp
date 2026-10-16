/*=============================================================================
 * nimcp_grounded_language_tf.h — TF (tier-feedback) plasticity loop.
 *
 * The Tier 1-3 surface correctors (F3 agreement, F4 fluency, T2-1
 * pronominalize, T3-1 givenness, T3-2 conjunction) currently produce
 * deterministic post-readout polish. TF captures the DELTA between the raw
 * produce output and the cascade-corrected output and feeds those deltas back
 * into plasticity at a small learning rate — so the network eventually
 * internalizes the corrected form and the correctors become scaffolding the
 * network graduates out of.
 *
 * TF-1 (this file): the diff machinery — LCS-aligned token-level Wagner-Fischer
 * edit script that emits gl_corrector_delta_t records. Observation-only; the
 * plasticity wiring lands in TF-2..TF-5.
 *
 * Per-position delta, never whole-output imitation — the NN learns "prefer
 * 'the' in *this context*", not "prefer 'the' always". Curriculum learning
 * stays the dominant teacher (~10x higher lr); TF is a secondary nudge.
 *===========================================================================*/
#ifndef NIMCP_GROUNDED_LANGUAGE_TF_H
#define NIMCP_GROUNDED_LANGUAGE_TF_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    /* raw[i] != corrected[i]; both nonempty. The most common case
     * (F3 agreement verb-form swap; T2-1 noun -> pronoun; T3-1 a -> the). */
    GL_TF_OP_SUBSTITUTE = 0,
    /* corrected has an extra token at this position that raw did not.
     * T3-2 conjunction insertion is the canonical case. */
    GL_TF_OP_INSERT     = 1,
    /* raw had a token that corrected dropped. T2-1 drops the determiner
     * before pronominalizing the noun ("the cat" -> "it"). */
    GL_TF_OP_DELETE     = 2,
} gl_tf_op_t;

typedef enum {
    /* Diffed over the entire cascade_apply_surface_correctors block.
     * Used in TF-1; finer-grained attribution per-corrector lands in TF-5. */
    GL_TF_SRC_AGGREGATE = 0,
    GL_TF_SRC_F3_AGREEMENT       = 1,
    GL_TF_SRC_F4_FLUENCY         = 2,
    GL_TF_SRC_T2_PRONOMINALIZE   = 3,
    GL_TF_SRC_T3_GIVENNESS       = 4,
    GL_TF_SRC_T3_CONJUNCTION     = 5,
} gl_tf_source_t;

/* Bounded — tokens past this length get truncated. Aligned with the gl_pron
 * lemma helpers' TOKLEN. */
#define GL_TF_TOKLEN  64

typedef struct {
    gl_tf_op_t      op;
    gl_tf_source_t  source;
    /* Position in the CORRECTED token stream. For SUBSTITUTE this is the
     * matching position in both raw and corrected. For INSERT it's where
     * the extra token sits in corrected. For DELETE it's the gap position
     * in corrected (= the index where the dropped raw token would have
     * been if it had survived). */
    uint32_t        position;
    char            raw_token[GL_TF_TOKLEN];
    char            corrected_token[GL_TF_TOKLEN];
} gl_corrector_delta_t;

/* Diff cap — anything past this is dropped. 96 covers ~3x the produce cap
 * (32 default) which gives headroom for clause-frame multi-clause output. */
#define GL_TF_DIFF_MAX_TOKENS  96

/**
 * @brief LCS-aligned token-level diff between two whitespace-tokenized
 *        utterances. O(n*m) Wagner-Fischer; n,m clamped to GL_TF_DIFF_MAX_TOKENS.
 *
 *        Emits oldest-first SUBSTITUTE / INSERT / DELETE ops into the caller's
 *        buffer. Excess deltas past `cap` are dropped (return value reflects
 *        what fit).
 *
 *        Whitespace-only inputs and NULLs return 0 deltas without writing.
 *        Identical inputs return 0 deltas.
 *
 * @param raw         Utterance before the correctors ran.
 * @param corrected   Utterance after the correctors ran.
 * @param deltas      Caller-owned buffer of at least `cap` slots.
 * @param cap         Capacity of `deltas`. 0 returns 0.
 * @param source      Attribution stamped on every emitted delta.
 * @return            Number of deltas written (<= cap).
 */
uint32_t gl_tf_diff_correctors(const char* raw, const char* corrected,
                               gl_corrector_delta_t* deltas, uint32_t cap,
                               gl_tf_source_t source);

/* Forward decl — gl is opaque at this level. */
struct grounded_language;

/**
 * @brief Cascade-boundary hook. Runs the diff between `raw` and `corrected`
 *        and bumps gl_stats_t telemetry counters (tf_calls + 1,
 *        tf_deltas_captured += n_deltas). Plasticity hooks land in TF-3..TF-5
 *        and will reuse the same delta buffer.
 *
 *        NULL gl / NULL strings / 0 deltas are silent no-ops. Called once
 *        per cascade_apply_surface_correctors invocation; the bump is
 *        thread-safe via the gl stats mutex used by other counter updates.
 *
 * @param gl          Grounded language handle (opaque).
 * @param raw         Utterance text before correctors.
 * @param corrected   Utterance text after correctors.
 * @param deltas      Optional caller-owned buffer to receive deltas. May be
 *                    NULL — telemetry still bumps with the diff count.
 * @param cap         Capacity of `deltas` (0 if NULL).
 * @return            Number of deltas computed (>= number written to buffer).
 */
uint32_t grounded_language_tf_record_diff(struct grounded_language* gl,
                                          const char* raw,
                                          const char* corrected,
                                          gl_corrector_delta_t* deltas,
                                          uint32_t cap);

/* Forward decl — caller passes through a void* to keep brain_internal
 * out of this header. Cast inside the impl. */
struct brain_internal;

/* Outcome-gate fact bundle. All fields are READ by gl_tf_outcome_ok; the
 * caller is responsible for populating them from the cascade state +
 * brain context. Designed as a plain struct (not a closure) so the gate
 * is unit-testable without spinning up a full brain. */
typedef struct {
    /* gl->current_stage at the time of the cascade. The gate requires >= 2. */
    int       stage;
    /* Master flag mirror — gl->produce_corrector_feedback_enabled. The
     * gate ANDs this; callers can read it via the gl getter. */
    bool      master_enabled;
    /* Latest external reward (DA proxy). Negative means punishment — the
     * gate blocks on reward < 0. Zero or positive passes. */
    float     last_external_reward;
    /* Time-since-last-reward in microseconds. Gate blocks if > reward_ttl_us
     * because stale rewards may have been overtaken by punishment. */
    uint64_t  reward_age_us;
    /* Staleness cutoff. 0 disables the freshness check (= "any past reward
     * counts"). */
    uint64_t  reward_ttl_us;
    /* speech_repair retry count from production_cascade_state_t.repair_attempts.
     * Non-zero means the first cascade pass failed and got retried — those
     * passes are noisy and we don't propagate their deltas. */
    uint32_t  repair_attempts;
} gl_tf_outcome_facts_t;

/* Reason codes returned by gl_tf_outcome_ok_reason; useful for telemetry
 * counters that distinguish WHICH gate blocked the apply. */
typedef enum {
    GL_TF_OUTCOME_OK             = 0,
    GL_TF_OUTCOME_BLOCKED_STAGE  = 1,
    GL_TF_OUTCOME_BLOCKED_MASTER = 2,
    GL_TF_OUTCOME_BLOCKED_DA     = 3,  /* reward < 0 */
    GL_TF_OUTCOME_BLOCKED_STALE  = 4,  /* reward older than ttl */
    GL_TF_OUTCOME_BLOCKED_RETRY  = 5,  /* speech-repair fired */
} gl_tf_outcome_t;

/**
 * @brief Outcome gate. Returns GL_TF_OUTCOME_OK iff every safety check
 *        passes; otherwise the specific reason for the block. The gate is
 *        composable (caller can request which checks to skip via
 *        TTL=0 / stage=2 / etc), but the default is conservative: all
 *        five checks active.
 */
gl_tf_outcome_t gl_tf_outcome_ok_reason(const gl_tf_outcome_facts_t* f);

/* Boolean convenience: true iff GL_TF_OUTCOME_OK. */
bool gl_tf_outcome_ok(const gl_tf_outcome_facts_t* f);

/*============================================================================
 * TF-3 — Trigram feedback path.
 *
 * For each SUBSTITUTE or INSERT delta, train the SNN bridge to prefer the
 * CORRECTED token in the context preceding it. Reuses the existing
 * grounded_language_learn_next_token_pair / _triple machinery — TF is just
 * a small-lr drive of the same plasticity rule that the curriculum text
 * loop uses, gated on outcome_ok + master + per-corrector bit + lr > 0.
 *
 * The "context preceding it" comes from the CORRECTED token stream so
 * subsequent positions stay self-consistent (e.g. after a T2-1
 * pronominalization, downstream trigrams see "it" not "the cat"). For a
 * delta at corrected position i:
 *   i >= 2 -> triple(corrected[i-2], corrected[i-1], corrected[i], lr)
 *   i == 1 -> pair(corrected[0], corrected[1], lr)
 *   i == 0 -> skip (no context)
 *
 * DELETE deltas are skipped — there's no token at the corrected position
 * to teach the network about. The natural curriculum signal erodes the
 * removed pattern over time without our help.
 *==========================================================================*/

/**
 * @brief Apply trigram-feedback plasticity on a batch of cascade deltas.
 *        Reads the gl trigram learning rate (gl->tf_lr_trigram). Bumps the
 *        gl_stats_t.tf_trigram_updates counter for every applied update.
 *        No-op when gl/text/deltas is NULL, count==0, or lr <= 0.
 *
 *        Caller is responsible for outcome gating + master/mask checks
 *        BEFORE calling — this function unconditionally trains on every
 *        delta in the batch. That separation keeps the apply function
 *        unit-testable without spinning up a full brain.
 *
 * @param gl              Grounded language handle.
 * @param corrected_text  The CORRECTED utterance the deltas refer to. Used
 *                        to look up (prev1, prev2) tokens by position.
 * @param deltas          Caller-owned delta buffer (see gl_tf_diff_correctors).
 * @param n_deltas        Number of deltas in `deltas`.
 * @return                Number of plasticity updates applied (>= 0).
 */
uint32_t gl_tf_apply_trigram(struct grounded_language* gl,
                             const char* corrected_text,
                             const gl_corrector_delta_t* deltas,
                             uint32_t n_deltas);

/*============================================================================
 * TF-4 — Distributional EMA feedback.
 *
 * For each SUBSTITUTE/INSERT delta, compute the context vector around the
 * corrected position (mean of context_vectors of surrounding tokens in a
 * window of GL_TF_DISTRIB_WINDOW on each side) and EMA-nudge the
 * corrected token's lexicon entry->context_vector toward it:
 *
 *   entry->context_vector += lr * (context_mean - entry->context_vector)
 *
 * Final L2-normalize after each touch to keep the embedding bounded —
 * the same invariant the curriculum loop maintains.
 *
 * Reads gl->tf_lr_distrib. Bumps gl_stats_t.tf_distrib_updates per applied
 * EMA touch (one per SUBSTITUTE/INSERT delta that had a non-empty context
 * window). DELETE deltas + tokens with empty windows skipped.
 *==========================================================================*/

/* Window size on each side of the delta position. 5 each side = up to 10
 * neighbors averaged. Tokens past the start/end of the stream simply
 * shrink the window — no padding. */
#define GL_TF_DISTRIB_WINDOW  5u

uint32_t gl_tf_apply_distributional(struct grounded_language* gl,
                                    const char* corrected_text,
                                    const gl_corrector_delta_t* deltas,
                                    uint32_t n_deltas);

/*============================================================================
 * TF-5 — SNN-side binding-strength feedback (the "bridge STDP" slot from
 * the original plan, retargeted post-Slice-A).
 *
 * After the rebuild the SNN-language bridge is transport-only — it owns no
 * mutable weights. The closest analog to "STDP on (context_pop → lex)" in
 * the post-Slice-A world is the lexicon→concept binding strength
 * (gl_word_binding_t.strength), which is what the produce-score concept
 * term reads via score_word_against_vector. TF-5 nudges that strength:
 *
 *   for SUBSTITUTE/INSERT delta(raw -> corrected):
 *     corrected_entry->top_binding.strength += lr      (clamp [0, 1])
 *     raw_entry->top_binding.strength       -= lr * 0.5 (clamp ≥ 0)
 *
 * Asymmetric (positive twice as strong as negative) so a corrector misfire
 * doesn't erode the lexicon faster than legitimate corrections build it.
 * DELETE deltas only apply the negative pass on the raw token.
 *
 * Reads gl->tf_lr_bridge_stdp. Bumps gl_stats_t.tf_stdp_updates per
 * applied strength touch. Default lr is 0.0 — the path is wired but inert
 * until TF-5 walkthrough + soak gives operators confidence to raise it.
 *==========================================================================*/

uint32_t gl_tf_apply_bridge_stdp(struct grounded_language* gl,
                                 const gl_corrector_delta_t* deltas,
                                 uint32_t n_deltas);

/**
 * @brief Cascade-level entry point that composes gate + diff + apply.
 *
 *        - Reads master flag, corrector mask, and lrs from gl.
 *        - If master is OFF or mask is 0 -> immediate no-op (no diff).
 *        - Otherwise runs gl_tf_diff_correctors internally (no telemetry
 *          bump — that's record_diff's job earlier in the cascade).
 *        - Builds outcome facts from the caller-supplied brain/cascade
 *          state; bumps the appropriate tf_outcome_* counter.
 *        - On GL_TF_OUTCOME_OK, dispatches to gl_tf_apply_trigram (TF-3),
 *          gl_tf_apply_distributional (TF-4 — added later), and
 *          gl_tf_apply_bridge_stdp (TF-5 — added later) per path lr.
 *
 *        Returns the number of plasticity updates applied across all paths
 *        (sum of returns from the per-path helpers).
 *
 * @param gl                     Grounded language handle.
 * @param raw                    Pre-corrector utterance.
 * @param corrected              Post-corrector utterance.
 * @param last_external_reward   brain->last_external_reward (DA proxy).
 * @param reward_age_us          now - brain->last_external_reward_us.
 * @param reward_ttl_us          0 disables freshness check.
 * @param repair_attempts        out_state->repair_attempts (0 = clean run).
 * @return                       Total plasticity updates applied.
 */
uint32_t grounded_language_tf_apply_cascade_feedback(
    struct grounded_language* gl,
    const char* raw,
    const char* corrected,
    float last_external_reward,
    uint64_t reward_age_us,
    uint64_t reward_ttl_us,
    uint32_t repair_attempts);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GROUNDED_LANGUAGE_TF_H */
