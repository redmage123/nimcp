/*=============================================================================
 * nimcp_grounded_language_tf.c — TF (tier-feedback) implementation.
 *
 * TF-1: LCS-aligned token diff between raw and corrected utterances. See
 * nimcp_grounded_language_tf.h for the contract and the campaign motivation.
 *===========================================================================*/

#include "language/nimcp_grounded_language_tf.h"
#include "nimcp_grounded_language_internal.h"  /* gl->stats access for record_diff */

#include <math.h>     /* sqrtf for TF-4 L2-normalize */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Internal tokenizer — whitespace split into a bounded fixed-stride array.
 * Returns the number of tokens parsed (clamped to `cap`). Tokens past
 * GL_TF_TOKLEN-1 chars are truncated (the long-word case is rare and the
 * truncated lemma is still useful for the trigram/distrib downstream). */
static uint32_t tf_tokenize(const char* s,
                            char tok[][GL_TF_TOKLEN], uint32_t cap) {
    if (!s) return 0;
    uint32_t n = 0;
    size_t i = 0, L = strlen(s);
    while (i < L && n < cap) {
        while (i < L && (s[i]==' '||s[i]=='\t'||s[i]=='\n'||s[i]=='\r')) i++;
        if (i >= L) break;
        uint32_t k = 0;
        while (i < L && !(s[i]==' '||s[i]=='\t'||s[i]=='\n'||s[i]=='\r')
               && k < GL_TF_TOKLEN-1) tok[n][k++] = s[i++];
        tok[n][k] = '\0';
        n++;
        while (i < L && !(s[i]==' '||s[i]=='\t'||s[i]=='\n'||s[i]=='\r')) i++;
    }
    return n;
}

/* The DP table is sized (N+1) * (N+1). N = GL_TF_DIFF_MAX_TOKENS = 96 gives
 * 97*97 = 9409 ints = ~37KB on the stack per call — acceptable for the
 * cascade hot path (one call per produce). Static would be smaller but
 * NOT thread-safe, and the cascade is touched by multiple RW workers. */

uint32_t gl_tf_diff_correctors(const char* raw, const char* corrected,
                               gl_corrector_delta_t* deltas, uint32_t cap,
                               gl_tf_source_t source) {
    if (!deltas || cap == 0) return 0;
    if (!raw || !corrected) return 0;

    char a[GL_TF_DIFF_MAX_TOKENS][GL_TF_TOKLEN];
    char b[GL_TF_DIFF_MAX_TOKENS][GL_TF_TOKLEN];
    uint32_t na = tf_tokenize(raw,       a, GL_TF_DIFF_MAX_TOKENS);
    uint32_t nb = tf_tokenize(corrected, b, GL_TF_DIFF_MAX_TOKENS);

    /* Fast paths: both empty, one empty, identical. Skip the DP. */
    if (na == 0 && nb == 0) return 0;
    if (na == nb) {
        /* Compare position-by-position — if identical, 0 deltas. The diff
         * loop below would yield the same answer but in O(n^2). */
        uint32_t mismatch = 0;
        for (uint32_t i = 0; i < na; i++) {
            if (strcmp(a[i], b[i]) != 0) { mismatch = 1; break; }
        }
        if (mismatch == 0) return 0;
    }

    /* Wagner-Fischer edit distance with substitute=1, insert=1, delete=1. */
    int dp[GL_TF_DIFF_MAX_TOKENS+1][GL_TF_DIFF_MAX_TOKENS+1];
    for (uint32_t i = 0; i <= na; i++) dp[i][0] = (int)i;
    for (uint32_t j = 0; j <= nb; j++) dp[0][j] = (int)j;
    for (uint32_t i = 1; i <= na; i++) {
        for (uint32_t j = 1; j <= nb; j++) {
            if (strcmp(a[i-1], b[j-1]) == 0) {
                dp[i][j] = dp[i-1][j-1];
            } else {
                int sub = dp[i-1][j-1] + 1;
                int ins = dp[i][j-1]   + 1;
                int del = dp[i-1][j]   + 1;
                int m = sub < ins ? sub : ins;
                if (del < m) m = del;
                dp[i][j] = m;
            }
        }
    }

    /* Walk back from (na, nb), emitting deltas newest-first into scratch.
     * Then reverse + copy up to `cap` into the caller's buffer.
     *
     * Trace-back precedence at a non-matching cell:
     *   - prefer SUBSTITUTE (dp[i-1][j-1] + 1) when both i,j>0
     *   - then INSERT     (dp[i][j-1] + 1) when j>0
     *   - else DELETE     (dp[i-1][j] + 1) when i>0
     * This biases toward "same position, different word" interpretation,
     * which matches the typical Tier 1-3 corrector behavior. */
    gl_corrector_delta_t scratch[GL_TF_DIFF_MAX_TOKENS * 2];
    uint32_t n_emitted = 0;
    int i = (int)na, j = (int)nb;
    while ((i > 0 || j > 0) && n_emitted < GL_TF_DIFF_MAX_TOKENS * 2) {
        if (i > 0 && j > 0 && strcmp(a[i-1], b[j-1]) == 0) {
            /* match — no delta */
            i--; j--;
            continue;
        }
        gl_corrector_delta_t* d = &scratch[n_emitted];
        d->source = source;
        d->raw_token[0] = '\0';
        d->corrected_token[0] = '\0';

        if (i > 0 && j > 0 && dp[i][j] == dp[i-1][j-1] + 1) {
            d->op = GL_TF_OP_SUBSTITUTE;
            d->position = (uint32_t)(j - 1);
            strncpy(d->raw_token,       a[i-1], GL_TF_TOKLEN - 1);
            d->raw_token[GL_TF_TOKLEN-1]       = '\0';
            strncpy(d->corrected_token, b[j-1], GL_TF_TOKLEN - 1);
            d->corrected_token[GL_TF_TOKLEN-1] = '\0';
            i--; j--;
        } else if (j > 0 && (i == 0 || dp[i][j] == dp[i][j-1] + 1)) {
            d->op = GL_TF_OP_INSERT;
            d->position = (uint32_t)(j - 1);
            strncpy(d->corrected_token, b[j-1], GL_TF_TOKLEN - 1);
            d->corrected_token[GL_TF_TOKLEN-1] = '\0';
            j--;
        } else if (i > 0) {
            d->op = GL_TF_OP_DELETE;
            /* DELETE position = where the dropped token would have lived
             * in the corrected stream (= current j, the "gap before next
             * b" point). */
            d->position = (uint32_t)j;
            strncpy(d->raw_token, a[i-1], GL_TF_TOKLEN - 1);
            d->raw_token[GL_TF_TOKLEN-1] = '\0';
            i--;
        } else {
            break;  /* defensive — should be unreachable given the while cond */
        }
        n_emitted++;
    }

    /* Reverse to oldest-first, copy to caller up to cap. */
    uint32_t out_n = n_emitted < cap ? n_emitted : cap;
    for (uint32_t k = 0; k < out_n; k++) {
        deltas[k] = scratch[n_emitted - 1 - k];
    }
    return out_n;
}

uint32_t grounded_language_tf_record_diff(struct grounded_language* gl,
                                          const char* raw,
                                          const char* corrected,
                                          gl_corrector_delta_t* deltas,
                                          uint32_t cap) {
    if (!gl) return 0;
    if (!raw || !corrected) return 0;
    /* Static scratch when caller didn't supply a buffer — TF-1 telemetry
     * only needs the count; plasticity (TF-3..5) will pass its own caller-
     * owned buffer. Sized to GL_TF_DIFF_MAX_TOKENS so it never overflows
     * regardless of utterance length. */
    gl_corrector_delta_t local_scratch[GL_TF_DIFF_MAX_TOKENS];
    gl_corrector_delta_t* dst = deltas ? deltas : local_scratch;
    uint32_t dst_cap = deltas ? cap : (uint32_t)GL_TF_DIFF_MAX_TOKENS;
    if (dst_cap == 0) dst_cap = GL_TF_DIFF_MAX_TOKENS; /* zero-cap caller -> use scratch */

    uint32_t n = gl_tf_diff_correctors(raw, corrected, dst, dst_cap,
                                       GL_TF_SRC_AGGREGATE);

    /* Bump telemetry. tf_calls increments every invocation (including
     * zero-delta passes) so operators can compute "fraction of cascade
     * calls that produced a correction" = tf_deltas_captured / tf_calls. */
    gl->stats.tf_calls++;
    gl->stats.tf_deltas_captured += n;
    return n;
}

/*============================================================================
 * TF-2 — Outcome gate.
 *
 * Composable safety check: TF plasticity (TF-3..TF-5) only fires when this
 * returns GL_TF_OUTCOME_OK. Each rejection reason is enumerated so the
 * caller can bump a per-reason counter for operator-visible telemetry —
 * "TF blocked: 240 stage / 12 retry / 0 DA" tells you whether the gate is
 * too restrictive vs whether plasticity is genuinely firing.
 *==========================================================================*/
gl_tf_outcome_t gl_tf_outcome_ok_reason(const gl_tf_outcome_facts_t* f) {
    if (!f) return GL_TF_OUTCOME_BLOCKED_MASTER;
    /* Stage gate: language correctors only meaningful at >=2; before that
     * the lexicon is too sparse to drive useful plasticity. */
    if (f->stage < 2) return GL_TF_OUTCOME_BLOCKED_STAGE;
    /* Master switch — gl->produce_corrector_feedback_enabled. */
    if (!f->master_enabled) return GL_TF_OUTCOME_BLOCKED_MASTER;
    /* DA gate: negative reward (punishment) means the cascade output was
     * judged bad — do NOT propagate its delta as a teaching signal. Zero
     * passes (neutral outcome — still worth small learning). */
    if (!(f->last_external_reward >= 0.0f)) return GL_TF_OUTCOME_BLOCKED_DA;
    /* Freshness: if ttl == 0 the caller has opted out of staleness checks.
     * Otherwise reward_age_us must be <= ttl, else the reward is from a
     * different conversational moment and shouldn't gate THIS delta. */
    if (f->reward_ttl_us > 0 && f->reward_age_us > f->reward_ttl_us) {
        return GL_TF_OUTCOME_BLOCKED_STALE;
    }
    /* Speech-repair: any non-zero retry count means the first cascade pass
     * was rejected and re-run. Those passes are noisy and we don't trust
     * their deltas as a teaching signal. */
    if (f->repair_attempts > 0) return GL_TF_OUTCOME_BLOCKED_RETRY;
    return GL_TF_OUTCOME_OK;
}

bool gl_tf_outcome_ok(const gl_tf_outcome_facts_t* f) {
    return gl_tf_outcome_ok_reason(f) == GL_TF_OUTCOME_OK;
}

/*============================================================================
 * TF-3 — Trigram feedback path.
 *==========================================================================*/
uint32_t gl_tf_apply_trigram(struct grounded_language* gl,
                             const char* corrected_text,
                             const gl_corrector_delta_t* deltas,
                             uint32_t n_deltas) {
    if (!gl || !corrected_text || !deltas || n_deltas == 0) return 0;
    const float lr = gl->tf_lr_trigram;
    if (!(lr > 0.0f)) return 0;  /* NaN guard included by the > test */

    /* Tokenize the corrected stream once — the deltas all refer to
     * positions in this stream. We hand the resulting tokens to the
     * existing learn_next_token helpers so TF rides the same plasticity
     * path the curriculum text loop uses. */
    char tok[GL_TF_DIFF_MAX_TOKENS][GL_TF_TOKLEN];
    uint32_t n_tok = 0;
    {
        size_t i = 0, L = strlen(corrected_text);
        while (i < L && n_tok < GL_TF_DIFF_MAX_TOKENS) {
            while (i < L && (corrected_text[i]==' '||corrected_text[i]=='\t'||
                             corrected_text[i]=='\n'||corrected_text[i]=='\r')) i++;
            if (i >= L) break;
            uint32_t k = 0;
            while (i < L && !(corrected_text[i]==' '||corrected_text[i]=='\t'||
                              corrected_text[i]=='\n'||corrected_text[i]=='\r')
                   && k < GL_TF_TOKLEN-1) tok[n_tok][k++] = corrected_text[i++];
            tok[n_tok][k] = '\0';
            n_tok++;
            while (i < L && !(corrected_text[i]==' '||corrected_text[i]=='\t'||
                              corrected_text[i]=='\n'||corrected_text[i]=='\r')) i++;
        }
    }

    uint32_t applied = 0;
    for (uint32_t k = 0; k < n_deltas; k++) {
        const gl_corrector_delta_t* d = &deltas[k];
        /* DELETE: no corrected token to teach. Skip. */
        if (d->op == GL_TF_OP_DELETE) continue;
        /* Sanity: position must be in range of the tokenized stream. */
        if (d->position >= n_tok) continue;
        /* The target token comes from the delta record (already the
         * corrected form), but we cross-check against the live token at
         * that position — if they mismatch (shouldn't happen for clean
         * diffs but defensive), we trust the delta record over the
         * recomputed token. */
        const char* target = d->corrected_token[0] ? d->corrected_token
                                                   : tok[d->position];
        if (!target[0]) continue;

        int rc = -1;
        if (d->position >= 2) {
            /* Full trigram update: (prev2, prev1, target). */
            rc = grounded_language_learn_next_token_triple(
                gl, tok[d->position - 2], tok[d->position - 1], target, lr);
        } else if (d->position == 1) {
            /* Single-token context — fall back to the bigram update. */
            rc = grounded_language_learn_next_token_pair(
                gl, tok[d->position - 1], target, lr);
        }
        /* position == 0: no context, skip silently. */

        if (rc == 0) {
            gl->stats.tf_trigram_updates++;
            applied++;
        }
    }
    return applied;
}

/*============================================================================
 * TF-4 — Distributional EMA feedback.
 *==========================================================================*/
uint32_t gl_tf_apply_distributional(struct grounded_language* gl,
                                    const char* corrected_text,
                                    const gl_corrector_delta_t* deltas,
                                    uint32_t n_deltas) {
    if (!gl || !corrected_text || !deltas || n_deltas == 0) return 0;
    const float lr = gl->tf_lr_distrib;
    if (!(lr > 0.0f)) return 0;
    const uint32_t D = gl->semantic_dim;
    if (D == 0) return 0;

    /* Tokenize the corrected stream into a bounded fixed-stride array
     * (same shape as gl_tf_apply_trigram). Distrib needs to look up
     * lexicon entries by token so we need the tokens, not just positions. */
    char tok[GL_TF_DIFF_MAX_TOKENS][GL_TF_TOKLEN];
    uint32_t n_tok = 0;
    {
        size_t i = 0, L = strlen(corrected_text);
        while (i < L && n_tok < GL_TF_DIFF_MAX_TOKENS) {
            while (i < L && (corrected_text[i]==' '||corrected_text[i]=='\t'||
                             corrected_text[i]=='\n'||corrected_text[i]=='\r')) i++;
            if (i >= L) break;
            uint32_t k = 0;
            while (i < L && !(corrected_text[i]==' '||corrected_text[i]=='\t'||
                              corrected_text[i]=='\n'||corrected_text[i]=='\r')
                   && k < GL_TF_TOKLEN-1) tok[n_tok][k++] = corrected_text[i++];
            tok[n_tok][k] = '\0';
            n_tok++;
            while (i < L && !(corrected_text[i]==' '||corrected_text[i]=='\t'||
                              corrected_text[i]=='\n'||corrected_text[i]=='\r')) i++;
        }
    }
    if (n_tok == 0) return 0;

    /* Per-call scratch for the context-mean vector. Stack-resident — D is
     * bounded by GL_SEMANTIC_DIM (configured per-deployment, typically
     * 64-256). 256 floats × 4 bytes = 1KB on the stack is fine. */
    float context_mean[GL_SEMANTIC_DIM];

    uint32_t applied = 0;
    for (uint32_t k = 0; k < n_deltas; k++) {
        const gl_corrector_delta_t* d = &deltas[k];
        if (d->op == GL_TF_OP_DELETE) continue;
        if (d->position >= n_tok) continue;
        const char* target = d->corrected_token[0] ? d->corrected_token
                                                   : tok[d->position];
        if (!target[0]) continue;

        /* Compute context mean = average of context_vectors of tokens in
         * [pos-W, pos-1] ∪ [pos+1, pos+W], skipping the delta position
         * itself and any token whose entry isn't context_initialized.
         * Empty window (no initialized neighbors) skips the update — that's
         * the cold-start case and we don't want to nudge toward zeros. */
        for (uint32_t dd = 0; dd < D; dd++) context_mean[dd] = 0.0f;
        uint32_t ctx_n = 0;
        int32_t  pos = (int32_t)d->position;
        int32_t  lo  = pos - (int32_t)GL_TF_DISTRIB_WINDOW;
        int32_t  hi  = pos + (int32_t)GL_TF_DISTRIB_WINDOW;
        if (lo < 0) lo = 0;
        if (hi >= (int32_t)n_tok) hi = (int32_t)n_tok - 1;
        for (int32_t j = lo; j <= hi; j++) {
            if (j == pos) continue;
            if (!tok[j][0]) continue;
            gl_lexicon_entry_t* e = gl_internal_lexicon_find_or_create(gl, tok[j]);
            if (!e || !e->context_initialized || !e->context_vector) continue;
            for (uint32_t dd = 0; dd < D; dd++) {
                context_mean[dd] += e->context_vector[dd];
            }
            ctx_n++;
        }
        if (ctx_n == 0) continue;
        float inv = 1.0f / (float)ctx_n;
        for (uint32_t dd = 0; dd < D; dd++) context_mean[dd] *= inv;

        /* Find / create target entry. EMA-nudge its context_vector toward
         * the computed mean. If the entry was un-initialized, the first
         * nudge effectively sets it (vec=0 → vec += lr * mean). Mark
         * context_initialized to make the entry visible to subsequent
         * comprehend / distributional reads. */
        gl_lexicon_entry_t* te = gl_internal_lexicon_find_or_create(gl, target);
        if (!te || !te->context_vector) continue;
        float norm2 = 0.0f;
        for (uint32_t dd = 0; dd < D; dd++) {
            te->context_vector[dd] += lr * (context_mean[dd] - te->context_vector[dd]);
            norm2 += te->context_vector[dd] * te->context_vector[dd];
        }
        /* L2-normalize to keep the embedding bounded — same invariant the
         * curriculum loop maintains. Skip if the vector is collapsed (the
         * EMA can't recover from a zero state without a non-zero context). */
        if (norm2 > 1e-12f) {
            float inv_norm = 1.0f / sqrtf(norm2);
            for (uint32_t dd = 0; dd < D; dd++) {
                te->context_vector[dd] *= inv_norm;
            }
        }
        te->context_initialized = true;

        gl->stats.tf_distrib_updates++;
        applied++;
    }
    return applied;
}

/*============================================================================
 * TF-5 — Lexicon-binding strength feedback (the "bridge STDP" slot).
 *
 * Slice A removed bridge weights, so the analogous SNN-side mutable state
 * is gl_word_binding.strength. The produce-score concept term reads it via
 * score_word_against_vector — strengthening the corrected token's top
 * binding directly raises its produce score for similar future intents.
 *
 * Asymmetric (positive 1.0×, negative 0.5×) so a corrector misfire doesn't
 * erode the lexicon faster than legitimate corrections build it. Strength
 * is clamped to [0, 1].
 *==========================================================================*/
uint32_t gl_tf_apply_bridge_stdp(struct grounded_language* gl,
                                 const gl_corrector_delta_t* deltas,
                                 uint32_t n_deltas) {
    if (!gl || !deltas || n_deltas == 0) return 0;
    const float lr = gl->tf_lr_bridge_stdp;
    if (!(lr > 0.0f)) return 0;

    uint32_t applied = 0;
    for (uint32_t k = 0; k < n_deltas; k++) {
        const gl_corrector_delta_t* d = &deltas[k];

        /* Positive pass on corrected token (SUBSTITUTE or INSERT). */
        if ((d->op == GL_TF_OP_SUBSTITUTE || d->op == GL_TF_OP_INSERT) &&
            d->corrected_token[0]) {
            gl_lexicon_entry_t* e =
                gl_internal_lexicon_find_or_create(gl, d->corrected_token);
            if (e && e->binding_count > 0 && e->bindings) {
                /* Touch the top binding only — narrow signal that won't
                 * destabilize the entry's whole binding distribution. */
                gl_word_binding_t* top = &e->bindings[0];
                for (uint32_t b = 1; b < e->binding_count; b++) {
                    if (e->bindings[b].strength > top->strength) {
                        top = &e->bindings[b];
                    }
                }
                float ns = top->strength + lr;
                if (ns > 1.0f) ns = 1.0f;
                top->strength = ns;
                gl->stats.tf_stdp_updates_pos++;
                applied++;
            }
        }

        /* Negative pass on raw token (SUBSTITUTE or DELETE). Half lr. */
        if ((d->op == GL_TF_OP_SUBSTITUTE || d->op == GL_TF_OP_DELETE) &&
            d->raw_token[0]) {
            gl_lexicon_entry_t* e =
                gl_internal_lexicon_find_or_create(gl, d->raw_token);
            if (e && e->binding_count > 0 && e->bindings) {
                gl_word_binding_t* top = &e->bindings[0];
                for (uint32_t b = 1; b < e->binding_count; b++) {
                    if (e->bindings[b].strength > top->strength) {
                        top = &e->bindings[b];
                    }
                }
                float ns = top->strength - lr * 0.5f;
                if (ns < 0.0f) ns = 0.0f;
                top->strength = ns;
                gl->stats.tf_stdp_updates_neg++;
                applied++;
            }
        }
    }
    return applied;
}

/*============================================================================
 * TF-3 cascade entry: compose master + mask check, outcome gate, and
 * per-path dispatch. The cascade module calls THIS, never the per-path
 * apply functions directly — so the gate semantics live in one place.
 *==========================================================================*/
uint32_t grounded_language_tf_apply_cascade_feedback(
    struct grounded_language* gl,
    const char* raw,
    const char* corrected,
    float last_external_reward,
    uint64_t reward_age_us,
    uint64_t reward_ttl_us,
    uint32_t repair_attempts)
{
    if (!gl) return 0;
    /* Fast-skip checks BEFORE building outcome facts so the master-OFF
     * common path is essentially free. */
    if (!gl->produce_corrector_feedback_enabled) return 0;
    if (gl->tf_enabled_correctors == 0) return 0;
    if (!raw || !corrected) return 0;

    /* Build outcome facts + check the gate, bumping the corresponding
     * counter regardless of outcome. */
    gl_tf_outcome_facts_t facts;
    facts.stage                = gl->current_stage;
    facts.master_enabled       = gl->produce_corrector_feedback_enabled;
    facts.last_external_reward = last_external_reward;
    facts.reward_age_us        = reward_age_us;
    facts.reward_ttl_us        = reward_ttl_us;
    facts.repair_attempts      = repair_attempts;
    gl_tf_outcome_t outcome = gl_tf_outcome_ok_reason(&facts);

    switch (outcome) {
        case GL_TF_OUTCOME_OK:              gl->stats.tf_outcome_ok++;             break;
        case GL_TF_OUTCOME_BLOCKED_STAGE:   gl->stats.tf_outcome_blocked_stage++;  return 0;
        case GL_TF_OUTCOME_BLOCKED_MASTER:  gl->stats.tf_outcome_blocked_master++; return 0;
        case GL_TF_OUTCOME_BLOCKED_DA:      gl->stats.tf_outcome_blocked_da++;     return 0;
        case GL_TF_OUTCOME_BLOCKED_STALE:   gl->stats.tf_outcome_blocked_stale++;  return 0;
        case GL_TF_OUTCOME_BLOCKED_RETRY:   gl->stats.tf_outcome_blocked_retry++;  return 0;
        default: return 0;
    }

    /* Run the diff. This is a second diff call per cascade pass (record_diff
     * already ran for TF-1 telemetry); the cost is a single Wagner-Fischer
     * over ≤96 tokens, which is fine on the cascade-once-per-produce hot
     * path. Telemetry IS NOT bumped here so the TF-1 invariant holds.
     *
     * Deltas hold corrected-position references so subsequent apply
     * functions can index `corrected` by d->position. */
    gl_corrector_delta_t deltas[GL_TF_DIFF_MAX_TOKENS];
    uint32_t n = gl_tf_diff_correctors(raw, corrected, deltas,
                                       GL_TF_DIFF_MAX_TOKENS,
                                       GL_TF_SRC_AGGREGATE);
    if (n == 0) return 0;

    uint32_t total_applied = 0;
    /* TF-3: trigram-feedback plasticity. */
    if (gl->tf_lr_trigram > 0.0f) {
        total_applied += gl_tf_apply_trigram(gl, corrected, deltas, n);
    }
    /* TF-4: distributional-EMA feedback plasticity. */
    if (gl->tf_lr_distrib > 0.0f) {
        total_applied += gl_tf_apply_distributional(gl, corrected, deltas, n);
    }
    /* TF-5: lexicon-binding strength feedback. Gated on tf_lr_bridge_stdp
     * > 0 — defaults to 0.0 even when master is on, so this path stays
     * inert by default until TF-6 walkthrough + soak gives operators
     * confidence to raise it. */
    if (gl->tf_lr_bridge_stdp > 0.0f) {
        total_applied += gl_tf_apply_bridge_stdp(gl, deltas, n);
    }
    return total_applied;
}
