/**
 * @file nimcp_grounded_language_memory_bridge.c
 * @brief Wire grounded_language to the brain's memory subsystems.
 * @date 2026-05-05
 *
 * WHAT: Implements the three new connect APIs declared in
 *       nimcp_grounded_language.h plus a single dispatch helper that
 *       fans a successful grounding event out to working memory,
 *       episodic replay, and the hippocampus adapter.
 *
 * WHY:  Pre-bridge, grounded_language was wired only to semantic_memory.
 *       Trained word→concept bindings sat in the lexicon hash table
 *       with no participation in the rest of the memory hierarchy:
 *         - no working-memory presence (so reasoning + ToM couldn't
 *           operate over recently-grounded words),
 *         - no episodic replay record (so sleep consolidation never
 *           rehearsed grounding moments),
 *         - no hippocampal encoding (so words couldn't be recalled by
 *           contextual cue).
 *       This file closes that gap with a single fan-out point, gated
 *       on the optional attachments — no behavior change when none of
 *       the three are connected.
 *
 * HOW:  - 3 setters (SRP, identical shape) write the opaque pointer
 *         into the internal struct.
 *       - gl_dispatch_event_to_memory() does the fan-out, called once
 *         from grounded_language_ground() after a successful binding.
 *         All three subsystems are independently optional.
 *       - The dispatcher is in this file (not the main GL impl) so the
 *         memory subsystem includes don't bleed into the primary file.
 *
 * SOLID/DRY: One opaque-cast per subsystem, one branch per nullable
 *            attachment, attention-floor and salience math centralised
 *            here so all three call sites use the same gating rules.
 */

#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_memory_bridge.h"
#include "nimcp_grounded_language_internal.h"

#include "cognitive/nimcp_working_memory.h"
#include "cognitive/nimcp_emotional_tagging.h"
#include "cognitive/memory/nimcp_episodic_replay.h"
#include "core/brain/regions/hippocampus/nimcp_hippocampus_adapter.h"
#include "core/brain/regions/broca/nimcp_broca_adapter.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_adapter.h"
#include "cognitive/nimcp_sleep_wake.h"

#include <math.h>
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_audio_cortex.h"
#include "perception/nimcp_speech_cortex.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Attention floor below which we drop the event from memory subsystems.
 * Low-attention groundings are noisy and would flood the WM 7±2 buffer. */
#define _GL_MEM_ATTN_FLOOR 0.30f

/* Hippocampus association LR — kept low so words don't dominate
 * hippocampal place/event encoding. */
#define _GL_MEM_HIPPO_LR   0.05f

/*----------------------------------------------------------------------
 * Setters — symmetric, validate handle + cast through void*.
 *--------------------------------------------------------------------*/

void grounded_language_connect_working_memory(grounded_language_t* gl, void* wm) {
    if (!gl) return;
    gl->working_memory = wm;
}

void grounded_language_connect_episodic_replay(grounded_language_t* gl, void* replay) {
    if (!gl) return;
    gl->episodic_replay = replay;
}

void grounded_language_connect_hippocampus(grounded_language_t* gl, void* hippocampus) {
    if (!gl) return;
    gl->hippocampus = hippocampus;
}

int grounded_language_set_engram_system(grounded_language_t* gl,
                                          void* engram_system,
                                          bool enabled) {
    if (!gl) return -1;
    gl->engram_system  = engram_system;
    /* Enabled flag is allowed to be true with NULL pointer — caller can
     * pre-flip the flag and attach later — but the hot-path check is
     * `enabled && pointer`, so the no-op contract is preserved either
     * way. */
    gl->engram_enabled = enabled;
    return 0;
}

bool grounded_language_engram_enabled(const grounded_language_t* gl) {
    if (!gl) return false;
    return gl->engram_enabled && gl->engram_system != NULL;
}

/* TA-2 LGSS attach. Stored as void* on the gl struct to keep the LGSS
 * umbrella header out of internal language headers (the umbrella drags
 * in cognitive/symbolic_logic/ which causes enum collisions when
 * included from non-LGSS modules). The comprehend hot path forward-
 * declares lgss_evaluate / safety_action_context_t / safety_evaluation_t
 * locally and casts back. NULL = detach, no-op gate. */
void grounded_language_set_lgss(grounded_language_t* gl, void* lgss) {
    if (!gl) return;
    gl->lgss = lgss;
}

/* IM-3 — Tier-3 immune content-inspection attach. Mirrors the engram
 * setter: borrowed pointer + boolean flag, no allocation, NULL-safe.
 * The hot-path check inside comprehend is `enabled && pointer`, so an
 * enabled flag with a NULL pointer is harmless (no-op until attached). */
int grounded_language_set_immune_system(grounded_language_t* gl,
                                          void* brain_immune_system,
                                          bool enabled) {
    if (!gl) return -1;
    gl->immune_system  = brain_immune_system;
    gl->immune_enabled = enabled;
    /* Reset the diagnostic level on (re-)attach so the probe doesn't
     * report a stale post-detach reading. */
    if (!enabled || !brain_immune_system) {
        gl->immune_inflammation_level = 0.0f;
    }
    return 0;
}

bool grounded_language_immune_enabled(const grounded_language_t* gl) {
    if (!gl) return false;
    return gl->immune_enabled && gl->immune_system != NULL;
}

void grounded_language_connect_broca(grounded_language_t* gl, void* broca) {
    if (!gl) return;
    gl->broca_adapter = broca;
}

void grounded_language_connect_wernicke(grounded_language_t* gl, void* wernicke) {
    if (!gl) return;
    gl->wernicke_adapter = wernicke;
}

/*----------------------------------------------------------------------
 * Lexicon mirror — push grounded_language entries into broca/wernicke.
 *
 * Called from the lexicon insert path the moment a fresh entry lands
 * in grounded_language. Both adapters take owned copies of their
 * struct, so we don't share storage — just initial seeding.
 *
 * SOLID: grounded_language is the canonical lexicon; broca + wernicke
 *        receive copies for their region-specific work (production
 *        phoneme planning + comprehension recognition). No reverse
 *        flow — if broca/wernicke evolve frequencies independently
 *        that's their business.
 *--------------------------------------------------------------------*/

void gl_mirror_new_word_to_regions(grounded_language_t* gl, const char* form) {
    if (!gl || !form || !form[0]) return;

    /* Wernicke comprehension lexicon: word_id=0 lets wernicke assign
     * its own internal id. We don't have phonemes yet — wernicke can
     * derive them from grapheme rules at first use. */
    if (gl->wernicke_adapter) {
        wernicke_word_t w;
        memset(&w, 0, sizeof(w));
        strncpy(w.word, form, sizeof(w.word) - 1);
        w.word_id = 0;
        w.phoneme_count = 0;
        w.frequency = 0.1f;   /* low default — bumps with use */
        w.concept_id = 0;
        w.pos = 0;             /* unknown */
        (void)wernicke_add_word((wernicke_adapter_t*)gl->wernicke_adapter, &w);
    }

    /* Broca production lexicon: minimum-viable seed.
     *
     * Bug-6 fix: previously called broca_add_word(), which is a
     * LOOKUP-only API — it expects the entry to already be in the
     * lexicon and forwards to the syntax processor. Every new-word
     * mirror hit LEXICON_MISS, so the bootstrap mirror at brain init
     * was the only thing that ever populated Broca's lexicon, and new
     * words acquired during training never made it in. Now we use
     * broca_add_lexical_entry_v(), which actually inserts. word_id=0
     * forces hash_string-based insertion (matching the lookup path,
     * per the convention documented in the broca adapter). */
    if (gl->broca_adapter) {
        (void)broca_add_lexical_entry_v(gl->broca_adapter,
                                          0 /* word_id — hash_string drives */,
                                          form,
                                          0 /* pos: unknown; broca fills on use */,
                                          0.1f /* low default frequency */);
        /* 1-per-100 observability log so over-mirroring is detectable. */
        static __thread uint64_t mirror_count = 0;
        if ((mirror_count++ % 100u) == 0u) {
            LOG_DEBUG("gl_mirror_new_word_to_regions: broca form='%s' count=%llu",
                      form, (unsigned long long)mirror_count);
        }
    }
}

/*----------------------------------------------------------------------
 * Wernicke comprehension result → grounded_language ingestion.
 *
 * For each recognized word in a wernicke comprehension result, build
 * a gl_grounding_event_t with modality=AUDITORY and push it through
 * grounded_language_ground(). This closes the audio-perception loop:
 * audio → wernicke → recognized words → grounded vocabulary growth.
 *--------------------------------------------------------------------*/

/*----------------------------------------------------------------------
 * One-shot audio→comprehension→ingest helper.
 *
 * Single entry point for callers (brain_process_multimodal in
 * particular) that don't want to pull in the wernicke header just to
 * size the comprehension struct. Internally:
 *   1. wernicke_comprehend(adapter, audio, samples, sample_rate, &comp)
 *   2. grounded_language_ingest_wernicke_result(gl, &comp, feats, dim)
 *   3. wernicke_free_comprehension(&comp)
 * Returns events recorded, or -1 on failure / NULL inputs.
 *--------------------------------------------------------------------*/
/* #2 Audio feature extraction — RMS envelope per equal-length chunk.
 * Cheap, deterministic, audio-content-distinguishing without an FFT.
 * Each output is normalized into [0, 1] by the peak chunk-RMS across
 * the utterance (NOT the global RMS). Silent chunks → 0; the loudest
 * chunk → 1.0; all others scaled in proportion.
 *
 * Returns 0 on success, -1 on bad input. */
int gl_extract_audio_features(const float* audio,
                                uint32_t samples,
                                uint32_t sample_rate,
                                float* out_features,
                                uint32_t feature_dim) {
    (void)sample_rate;  /* reserved for future band-aware extractor */
    if (!audio || samples == 0 || !out_features || feature_dim == 0) {
        return -1;
    }
    /* Chunk size — last chunk picks up any remainder. */
    uint32_t chunk = samples / feature_dim;
    if (chunk == 0) {
        /* feature_dim > samples; replicate available samples + zero-pad. */
        for (uint32_t i = 0; i < feature_dim; i++) {
            out_features[i] = (i < samples) ? fabsf(audio[i]) : 0.0f;
        }
        return 0;
    }

    float peak = 0.0f;
    for (uint32_t b = 0; b < feature_dim; b++) {
        uint32_t start = b * chunk;
        uint32_t end = (b == feature_dim - 1) ? samples : start + chunk;
        float ss = 0.0f;
        for (uint32_t s = start; s < end; s++) ss += audio[s] * audio[s];
        float rms = sqrtf(ss / (float)(end - start));
        out_features[b] = rms;
        if (rms > peak) peak = rms;
    }
    if (peak > 1e-9f) {
        float inv = 1.0f / peak;
        for (uint32_t b = 0; b < feature_dim; b++) out_features[b] *= inv;
    }
    return 0;
}

int gl_drive_audio_comprehension(grounded_language_t* gl,
                                  void* wernicke_adapter_v,
                                  const float* audio, uint32_t samples,
                                  uint32_t sample_rate,
                                  const float* audio_features,
                                  uint32_t feature_dim) {
    if (!gl || !wernicke_adapter_v || !audio || samples == 0) {
        return -1;
    }
    /* Auto-extract features when caller passes NULL — saves the caller
     * from maintaining a parallel feature pipeline. */
    float local_feat[32];
    const float* feat = audio_features;
    uint32_t fdim = feature_dim;
    if (!feat) {
        fdim = 32;
        if (gl_extract_audio_features(audio, samples, sample_rate,
                                        local_feat, fdim) != 0) {
            return -1;
        }
        feat = local_feat;
    }
    if (!feat || fdim == 0) return -1;

    wernicke_comprehension_t comp;
    memset(&comp, 0, sizeof(comp));
    if (!wernicke_comprehend((wernicke_adapter_t*)wernicke_adapter_v,
                              audio, samples, sample_rate, &comp)) {
        return -1;
    }
    int n = grounded_language_ingest_wernicke_result(
        gl, &comp, feat, fdim);
    wernicke_free_comprehension(&comp);
    return n;
}

int grounded_language_ingest_wernicke_result(grounded_language_t* gl,
                                              const void* comp_result_v,
                                              const float* audio_features,
                                              uint32_t feature_dim) {
    if (!gl || !comp_result_v || !audio_features || feature_dim == 0) {
        return -1;
    }
    const wernicke_comprehension_t* comp =
        (const wernicke_comprehension_t*)comp_result_v;
    if (!comp->words || comp->word_count == 0) return 0;

    int recorded = 0;
    for (uint32_t i = 0; i < comp->word_count; i++) {
        const wernicke_word_result_t* wr = &comp->words[i];
        if (!wr->word.word[0]) continue;

        gl_grounding_event_t e;
        memset(&e, 0, sizeof(e));
        e.word              = wr->word.word;
        e.modality          = GL_MODALITY_AUDITORY;
        e.sensory_features  = audio_features;
        e.feature_dim       = feature_dim;
        e.emotional_valence = 0.0f;
        e.emotional_arousal = 0.0f;
        e.attention         = wr->confidence;  /* recognition confidence */
        e.context_sentence  = NULL;

        if (grounded_language_ground(gl, &e) == 0) recorded++;
    }
    return recorded;
}

/*----------------------------------------------------------------------
 * Dispatcher — single fan-out call site.
 *
 * Called from grounded_language_ground() after a binding has been
 * recorded. Every connected subsystem gets a chance to absorb the
 * event; none of them is required.
 *--------------------------------------------------------------------*/

void gl_dispatch_event_to_memory(grounded_language_t* gl,
                                  const gl_grounding_event_t* event,
                                  uint64_t concept_id) {
    if (!gl || !event) return;
    if (!event->sensory_features || event->feature_dim == 0) return;
    /* Drop low-attention noise — see _GL_MEM_ATTN_FLOOR rationale above. */
    if (event->attention < _GL_MEM_ATTN_FLOOR) return;

    /* Working memory: emotionally-tagged add. The salience boost from
     * arousal/valence happens inside working_memory_add_with_emotion. */
    if (gl->working_memory) {
        emotional_tag_t tag = emotional_tag_create(
            event->emotional_valence,
            event->emotional_arousal,
            /* timestamp 0 — caller-side timing not tracked here */ 0u);
        tag.intensity = event->attention;
        (void)working_memory_add_with_emotion(
            (working_memory_t*)gl->working_memory,
            event->sensory_features, event->feature_dim,
            event->attention, &tag);
    }

    /* Episodic replay: record the grounding event with the word as
     * label. Loss := 1 - attention so high-attention events float to
     * the top of the importance queue when sleep consolidation picks. */
    if (gl->episodic_replay) {
        float loss   = 1.0f - event->attention;
        float reward = event->attention;
        (void)nimcp_episodic_replay_record(
            (nimcp_episodic_replay_t*)gl->episodic_replay,
            event->sensory_features, event->feature_dim,
            /* No target vector here — record_record API tolerates
             * NULL target with target_size=0 for label-only episodes. */
            NULL, 0,
            event->word, loss, reward);
    }

    /* Hippocampus: train cue→concept association. The cue is the
     * sensory feature vector; the target is the concept_id we just
     * bound. This is what lets later contextual recall (e.g. "what
     * did I see?") reactivate the word. */
    if (gl->hippocampus) {
        (void)hippocampus_train_association(
            (hippocampus_adapter_t*)gl->hippocampus,
            event->sensory_features, event->feature_dim,
            (uint32_t)(concept_id & 0xFFFFFFFFu),
            _GL_MEM_HIPPO_LR);
    }
}

/*----------------------------------------------------------------------
 * Sleep-state consolidation.
 *
 * Walk the vocab list and apply stage-appropriate updates to binding
 * strengths, frequency-weighted reinforcement, and confidence values.
 * Mirrors the snn_language_bridge sleep-consolidate hook in shape but
 * operates on the symbolic lexicon rather than spike-driven STDP.
 *
 * SOLID: one entry point, branches on sleep_state, no side effects on
 *        wake/drowsy. Parameters validated up front.
 *--------------------------------------------------------------------*/

/* Reinforcement floor — bindings used at least this often get the full
 * NREM strength boost; rarer ones decay proportionally. */
#define _GL_SLEEP_FREQ_FLOOR  3u
/* Hard floor on binding strength after decay — prevents complete loss. */
#define _GL_SLEEP_STRENGTH_FLOOR 0.05f
/* Cap on per-pass reinforcement so a single sleep cycle can't
 * runaway-amplify a binding. */
#define _GL_SLEEP_STRENGTH_CEIL  1.0f

/*----------------------------------------------------------------------
 * Cortex modulation — scalar taps from the connected cortexes.
 *
 * Why this is the right scope: visual_cortex/audio_cortex/speech_cortex
 * all expose feature-extraction APIs that require raw input (image bytes,
 * audio samples). Holding raw input across language calls would balloon
 * memory + force re-extraction. Scalar taps (phoneme confidence, speech
 * salience, processing-active flag) are cheap O(1) reads of cached state
 * and let us bias grounding/comprehension by current sensory activity.
 *
 * Mapping:
 *   visual_activity   ← visual_cortex_get_stats(images_processed > 0 ? 1 : 0)
 *                       (no scalar attention tap available; this is a
 *                       coarse "has-the-cortex-seen-data-recently" proxy)
 *   audio_salience    ← audio_cortex_get_speech_salience(NULL,0) when
 *                       attached. The accessor is feature-driven; we
 *                       call it with empty features to read the cached
 *                       last-tick salience (impl returns last value).
 *   speech_confidence ← speech_cortex_get_phoneme_confidence()
 *
 * Each tap is independent — missing cortex means 0 for that channel.
 *--------------------------------------------------------------------*/
int grounded_language_get_cortex_modulation(grounded_language_t* gl,
                                              gl_cortex_modulation_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    if (!gl) return -1;

    /* Visual: stats-based activity proxy. Stats are O(1). */
    if (gl->visual_ctx) {
        visual_cortex_stats_t vs;
        memset(&vs, 0, sizeof(vs));
        if (visual_cortex_get_stats((const visual_cortex_t*)gl->visual_ctx, &vs)) {
            /* Coarse: nonzero processing → activity=1; saturating tanh on count. */
            out->visual_activity = (vs.images_processed > 0) ? 1.0f : 0.0f;
        }
    }

    /* Audio: speech salience scalar. The accessor signature requires a
     * feature buffer; pass NULL/0 → impl returns its last-cached value
     * if available. Defensive cast in case the cortex hasn't run. */
    if (gl->auditory_ctx) {
        float sal = audio_cortex_get_speech_salience(
            (audio_cortex_t*)gl->auditory_ctx, NULL, 0);
        if (sal >= 0.0f && sal <= 1.0f) out->audio_salience = sal;
    }

    /* Speech: phoneme confidence — direct scalar from cached state. */
    if (gl->speech_ctx) {
        float c = speech_cortex_get_phoneme_confidence(
            (speech_cortex_t*)gl->speech_ctx);
        if (c >= 0.0f && c <= 1.0f) out->speech_confidence = c;
    }

    return 0;
}

/* Internal helper — returns a multiplier in [1.0, 1.0+max_boost] driven
 * by the modality-relevant cortex tap. Used to bias attention/strength
 * for in-modality grounding events. Caller picks max_boost. */
static float _gl_cortex_attention_boost(grounded_language_t* gl,
                                         gl_modality_t modality,
                                         float max_boost) {
    gl_cortex_modulation_t m;
    if (grounded_language_get_cortex_modulation(gl, &m) != 0) return 1.0f;
    float tap = 0.0f;
    switch (modality) {
        case GL_MODALITY_VISUAL:    tap = m.visual_activity;   break;
        case GL_MODALITY_AUDITORY:  tap = m.audio_salience;    break;
        case GL_MODALITY_MOTOR:     /* fall-through: speech-side */
        case GL_MODALITY_SPATIAL:   tap = m.speech_confidence; break;
        default: tap = 0.0f;
    }
    if (tap < 0.0f) tap = 0.0f;
    if (tap > 1.0f) tap = 1.0f;
    return 1.0f + max_boost * tap;
}

/* Public-ish: callers in nimcp_grounded_language.c use this to scale
 * binding strength + confidence at ground/comprehend time. */
float gl_cortex_modulation_for_modality(grounded_language_t* gl,
                                         gl_modality_t modality,
                                         float max_boost) {
    return _gl_cortex_attention_boost(gl, modality, max_boost);
}

int grounded_language_sleep_consolidate(grounded_language_t* gl,
                                         int sleep_state_int,
                                         float strength) {
    if (!gl) return -1;
    if (strength < 0.0f || strength > 1.0f) return -1;
    sleep_state_t state = (sleep_state_t)sleep_state_int;

    /* Wake/drowsy: nothing to do. */
    if (state == SLEEP_STATE_AWAKE || state == SLEEP_STATE_DROWSY) {
        return 0;
    }

    uint32_t decayed_this_pass = 0;

    /* For each lexicon entry, scale binding strengths by stage rule. */
    for (uint32_t i = 0; i < gl->vocab_count; i++) {
        gl_lexicon_entry_t* e = gl->vocab_list[i];
        if (!e) continue;

        bool is_frequent = (e->frequency >= _GL_SLEEP_FREQ_FLOOR);
        bool entry_was_decayed = false;

        if (state == SLEEP_STATE_DEEP_NREM) {
            /* Reinforce frequent bindings, decay stale ones.
             * Reinforcement: s_new = clamp(s + 0.1*strength, ≤1.0)
             * Decay:         s_new = max(s * (1 - 0.05*strength), floor) */
            for (uint32_t b = 0; b < e->binding_count; b++) {
                gl_word_binding_t* bind = &e->bindings[b];
                if (is_frequent) {
                    bind->strength += 0.10f * strength;
                    if (bind->strength > _GL_SLEEP_STRENGTH_CEIL)
                        bind->strength = _GL_SLEEP_STRENGTH_CEIL;
                    bind->confidence += 0.05f * strength;
                    if (bind->confidence > 1.0f) bind->confidence = 1.0f;
                } else {
                    float prior = bind->strength;
                    bind->strength *= (1.0f - 0.05f * strength);
                    if (bind->strength < _GL_SLEEP_STRENGTH_FLOOR)
                        bind->strength = _GL_SLEEP_STRENGTH_FLOOR;
                    if (prior > _GL_SLEEP_STRENGTH_FLOOR &&
                        bind->strength <= _GL_SLEEP_STRENGTH_FLOOR) {
                        entry_was_decayed = true;
                    }
                }
            }
        } else if (state == SLEEP_STATE_LIGHT_NREM) {
            /* Mild decay across the board — sleep "sorting" stage. */
            for (uint32_t b = 0; b < e->binding_count; b++) {
                gl_word_binding_t* bind = &e->bindings[b];
                float prior = bind->strength;
                bind->strength *= (1.0f - 0.02f * strength);
                if (bind->strength < _GL_SLEEP_STRENGTH_FLOOR)
                    bind->strength = _GL_SLEEP_STRENGTH_FLOOR;
                if (prior > _GL_SLEEP_STRENGTH_FLOOR &&
                    bind->strength <= _GL_SLEEP_STRENGTH_FLOOR) {
                    entry_was_decayed = true;
                }
            }
        } else if (state == SLEEP_STATE_REM) {
            /* REM associative spreading — bump valence/arousal toward
             * neutral so emotional bindings don't drift in one direction
             * over long training runs. No structural decay. */
            e->valence *= (1.0f - 0.02f * strength);
            e->arousal *= (1.0f - 0.02f * strength);
        }
        if (entry_was_decayed) decayed_this_pass++;
    }

    /* Forgetting-curve telemetry (#15): bump current bucket + all-time
     * counter. The bucket ring rotation happens elsewhere on hour
     * rollover; this just accumulates within the current bucket. */
    if (decayed_this_pass > 0) {
        gl->decayed_ring[gl->decayed_ring_head] += decayed_this_pass;
        gl->decayed_all_time += decayed_this_pass;
    }

    /* #5 LRU eviction — DEEP_NREM is the safe biological moment to prune.
     * The wake-time hot path no longer auto-evicts (it could free entries
     * held by callers walking vocab_list, and could evict the very word
     * being looked up). Sleep is the natural decay/consolidation phase
     * where no caller is mid-lookup, and the strength * (1 - 0.05) decay
     * already lowers the relative frequency of unused bindings.
     *
     * Trigger when vocab is past the high-water mark — leave headroom
     * during normal operation, only prune when growth pressure exists. */
    if (state == SLEEP_STATE_DEEP_NREM &&
        gl->vocab_count >= GL_LRU_HIGH_WATER) {
        grounded_language_evict_lru(gl, GL_LRU_EVICT_BATCH);
    }
    return 0;
}
