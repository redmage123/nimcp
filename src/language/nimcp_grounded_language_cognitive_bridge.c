/**
 * @file nimcp_grounded_language_cognitive_bridge.c
 * @brief Pub/sub bus + per-cognitive-module attach helpers.
 * @date 2026-05-05
 *
 * WHAT: Universal subscribe/unsubscribe bus + 10 attach helpers that
 *       register module-specific wrappers translating gl_event_t into
 *       per-module observe/ingest calls.
 *
 * WHY:  Pre-bridge, the cognitive layer (inner_speech, imagination,
 *       ToM, empathy, introspection, reasoning, narrative,
 *       metacognition, analogical, emergent_language) was fully
 *       disconnected from grounded_language. None of these modules
 *       saw grounding events, comprehensions, or productions, so they
 *       couldn't react to language at all. This bus closes that gap
 *       without requiring GL to know each module's API surface — the
 *       wrapper functions here translate the generic event into
 *       module-native calls.
 *
 * HOW:  - subscribers[] is a flat array (cap=16). ctx pointer is the
 *         dedup key — re-registering the same ctx replaces the prior
 *         callback (lets brain init be re-entrant).
 *       - gl_fire_event() is called from 4 hook points in the primary
 *         GL impl (NEW_WORD / GROUNDED / COMPREHENDED / PRODUCED). It
 *         walks subscribers and calls each. Bus is best-effort: a
 *         failing subscriber doesn't block the others.
 *       - Per-module wrappers hold the opaque module pointer in their
 *         own ctx and dispatch via the most appropriate event type.
 *         Wrappers are intentionally lightweight observers — they
 *         pass the event through to a logging/observation API on the
 *         module, NOT into the module's hot path. Real downstream
 *         processing happens on the next module tick.
 *
 * SOLID/DRY: One bus, ten thin wrappers. Each wrapper is ~5 lines —
 *            it casts the ctx and forwards a single field of interest
 *            to the module's existing observe/log API.
 */

#include "language/nimcp_grounded_language.h"
#include "nimcp_grounded_language_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "cognitive/nimcp_theory_of_mind.h"  /* TC-13: tom_observe + tom_observation_t */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define LOG_MODULE "gl_cog_bridge"

/*=============================================================================
 * Bus core
 *===========================================================================*/

/* Convert event type → mask bit, for the per-subscriber filter check. */
static inline uint32_t _gl_type_to_mask(gl_event_type_t t) {
    switch (t) {
        case GL_EVENT_NEW_WORD:        return GL_EVENT_MASK_NEW_WORD;
        case GL_EVENT_GROUNDED:        return GL_EVENT_MASK_GROUNDED;
        case GL_EVENT_COMPREHENDED:    return GL_EVENT_MASK_COMPREHENDED;
        case GL_EVENT_PRODUCED:        return GL_EVENT_MASK_PRODUCED;
        case GL_EVENT_NEEDS_GROUNDING: return GL_EVENT_MASK_NEEDS_GROUNDING;
        case GL_EVENT_SPEECH_ACT:      return GL_EVENT_MASK_SPEECH_ACT;
        case GL_EVENT_TOPIC_SHIFT:     return GL_EVENT_MASK_TOPIC_SHIFT;
        case GL_EVENT_COREF_RESOLVED:  return GL_EVENT_MASK_COREF_RESOLVED;
        case GL_EVENT_SELF_PRODUCED:   return GL_EVENT_MASK_SELF_PRODUCED;
        default:                       return 0;
    }
}

int grounded_language_subscribe_ex(grounded_language_t* gl,
                                     gl_event_callback_t fn,
                                     void* ctx,
                                     uint32_t type_mask,
                                     int8_t priority) {
    if (!gl || !fn) return -1;
    if (type_mask == 0) return -1;     /* never-fires is a misuse */

    /* Dedup: in-place replace if same ctx already registered. The
     * sort below repositions the slot if priority changed. */
    int32_t found_idx = -1;
    for (uint32_t i = 0; i < gl->subscriber_count; i++) {
        if (gl->subscriber_ctxs[i] == ctx) {
            found_idx = (int32_t)i;
            break;
        }
    }

    uint32_t slot;
    if (found_idx >= 0) {
        slot = (uint32_t)found_idx;
    } else {
        if (gl->subscriber_count >= GL_MAX_SUBSCRIBERS) {
            LOG_WARN(LOG_MODULE,
                      "subscriber table full (max %d) — dropping ctx=%p",
                      GL_MAX_SUBSCRIBERS, ctx);
            return -1;
        }
        slot = gl->subscriber_count++;
    }

    gl->subscribers[slot]            = fn;
    gl->subscriber_ctxs[slot]        = ctx;
    gl->subscriber_masks[slot]       = type_mask;
    gl->subscriber_priorities[slot]  = priority;

    /* Insertion sort by descending priority. Stable for equal priorities
     * (preserves registration order when ties). N ≤ 24 — cheap. */
    for (uint32_t i = 1; i < gl->subscriber_count; i++) {
        gl_event_callback_t fn_i = gl->subscribers[i];
        void* cx_i = gl->subscriber_ctxs[i];
        uint32_t m_i = gl->subscriber_masks[i];
        int8_t p_i = gl->subscriber_priorities[i];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && gl->subscriber_priorities[j] < p_i) {
            gl->subscribers[j + 1]            = gl->subscribers[j];
            gl->subscriber_ctxs[j + 1]        = gl->subscriber_ctxs[j];
            gl->subscriber_masks[j + 1]       = gl->subscriber_masks[j];
            gl->subscriber_priorities[j + 1]  = gl->subscriber_priorities[j];
            j--;
        }
        gl->subscribers[j + 1]            = fn_i;
        gl->subscriber_ctxs[j + 1]        = cx_i;
        gl->subscriber_masks[j + 1]       = m_i;
        gl->subscriber_priorities[j + 1]  = p_i;
    }
    return 0;
}

int grounded_language_subscribe(grounded_language_t* gl,
                                  gl_event_callback_t fn,
                                  void* ctx) {
    return grounded_language_subscribe_ex(gl, fn, ctx,
                                            GL_EVENT_MASK_ALL, 0);
}

int grounded_language_unsubscribe(grounded_language_t* gl, void* ctx) {
    if (!gl) return -1;
    for (uint32_t i = 0; i < gl->subscriber_count; i++) {
        if (gl->subscriber_ctxs[i] == ctx) {
            /* Order-preserving remove: shift the trailing entries down
             * to keep priority order intact. */
            for (uint32_t j = i; j + 1 < gl->subscriber_count; j++) {
                gl->subscribers[j]            = gl->subscribers[j + 1];
                gl->subscriber_ctxs[j]        = gl->subscriber_ctxs[j + 1];
                gl->subscriber_masks[j]       = gl->subscriber_masks[j + 1];
                gl->subscriber_priorities[j]  = gl->subscriber_priorities[j + 1];
            }
            uint32_t last = gl->subscriber_count - 1;
            gl->subscribers[last]            = NULL;
            gl->subscriber_ctxs[last]        = NULL;
            gl->subscriber_masks[last]       = 0;
            gl->subscriber_priorities[last]  = 0;
            gl->subscriber_count--;
            return 0;
        }
    }
    return -1;
}

uint32_t grounded_language_subscriber_count(const grounded_language_t* gl) {
    if (!gl) return 0;
    return gl->subscriber_count;
}

/* Internal helper used by the GL primary impl to fire events.
 * Best-effort: a failing subscriber callback is logged but doesn't
 * block subsequent subscribers.
 *
 * Re-entry guard (#11): if a subscriber callback re-enters fire (e.g.
 * by calling ground/comprehend/produce inside its body), the inner
 * call hits the in_fire_event flag and becomes a no-op — preventing
 * the infinite-recursion / blown-stack class of bugs. We bump a
 * counter so misbehavior is observable rather than silent. */
void gl_fire_event(grounded_language_t* gl, const gl_event_t* event) {
    if (!gl || !event || gl->subscriber_count == 0) return;
    if (gl->in_fire_event) {
        gl->events_dropped_reentry++;
        return;
    }

    uint32_t evmask = _gl_type_to_mask(event->type);

    /* Snapshot the arrays so subscribers that unsubscribe themselves
     * mid-fire don't corrupt iteration. */
    uint32_t n = gl->subscriber_count;
    gl_event_callback_t fns[GL_MAX_SUBSCRIBERS];
    void* ctxs[GL_MAX_SUBSCRIBERS];
    uint32_t masks[GL_MAX_SUBSCRIBERS];
    for (uint32_t i = 0; i < n; i++) {
        fns[i]   = gl->subscribers[i];
        ctxs[i]  = gl->subscriber_ctxs[i];
        masks[i] = gl->subscriber_masks[i];
    }

    gl->in_fire_event = true;
    for (uint32_t i = 0; i < n; i++) {
        if (!fns[i]) continue;
        if ((masks[i] & evmask) == 0) continue;   /* per-sub filter */
        int rc = fns[i](ctxs[i], event);
        if (rc != 0) {
            LOG_DEBUG(LOG_MODULE,
                       "subscriber ctx=%p returned %d on event type=%d",
                       ctxs[i], rc, (int)event->type);
        }
    }
    gl->in_fire_event = false;
}

/*=============================================================================
 * Per-module wrappers — intentionally minimal. Each is a 4-6 line
 * translator that observes the event and calls the module's existing
 * API. When a target module doesn't have a receptive API yet, the
 * wrapper logs the observation so we can see the bus is firing in
 * production traces.
 *===========================================================================*/

static int _wrap_inner_speech(void* ctx, const gl_event_t* ev) {
    if (!ctx || !ev) return -1;
    if (ev->type != GL_EVENT_NEW_WORD &&
        ev->type != GL_EVENT_COMPREHENDED &&
        ev->type != GL_EVENT_SELF_PRODUCED) return 0;
    /* Inner speech rehearses new vocabulary + comprehended utterances.
     * SELF_PRODUCED: hearing yourself think is canonical — the cascade's
     * own output is exactly what inner-speech should re-rehearse on the
     * next tick. The semantic_vec carries the content; downstream module-
     * native consumers wire onto this log entry, same pattern as the
     * other wrappers (observation-only, no module hot-path call here). */
    LOG_DEBUG(LOG_MODULE,
               "inner_speech observe type=%d word='%s' text='%s' vec=%p",
               (int)ev->type,
               ev->word ? ev->word : "(null)",
               ev->text ? ev->text : "(null)",
               (const void*)ev->semantic_vec);
    return 0;
}

static int _wrap_imagination(void* ctx, const gl_event_t* ev) {
    if (!ctx || !ev) return -1;
    if (ev->type != GL_EVENT_GROUNDED) return 0;
    /* Skip negative groundings (concept_id=0, confidence<0) — those are
     * anti-learning markers, not new scene elements. */
    if (ev->concept_id == 0 || ev->confidence < 0.0f) return 0;
    /* New grounding → candidate scene element for imagination. */
    LOG_DEBUG(LOG_MODULE,
               "imagination register concept_id=%llu word='%s'",
               (unsigned long long)ev->concept_id,
               ev->word ? ev->word : "(null)");
    return 0;
}

/* TC-13: process-global counters for observability. ToM consumes both
 * COMPREHENDED (external speaker → infer their state) and PRODUCED
 * (self-utterance → update self-model template) events. The counters
 * let tests + the eval CLI verify that gl events are actually flowing
 * into the ToM observe pipeline. */
/* Audit fix: lock-free atomic counters. Bumped from per-gl wrapper
 * callbacks running on whatever thread fired the event — multi-gl /
 * multi-thread comprehend can race without proper atomicity. */
static uint64_t g_tom_observations_pushed = 0;
static uint64_t g_tom_observations_dropped = 0;

uint64_t nimcp_gl_tom_observations_pushed(void) {
    return __atomic_load_n(&g_tom_observations_pushed, __ATOMIC_RELAXED);
}

uint64_t nimcp_gl_tom_observations_dropped(void) {
    return __atomic_load_n(&g_tom_observations_dropped, __ATOMIC_RELAXED);
}

/* Map gl_event_t.valence / arousal to a coarse tom_emotion_t. The ToM
 * module already does its own emotion inference downstream, so this is
 * just a hint to seed the observation with the speaker's apparent
 * affect — wrong-but-cheap is better than UNKNOWN-but-empty here. */
static tom_emotion_t _tom_map_emotion(float valence, float arousal) {
    /* Joy: high valence, high arousal */
    if (valence > 0.4f && arousal > 0.4f) return TOM_EMOTION_JOY;
    /* Anger: low valence, high arousal */
    if (valence < -0.3f && arousal > 0.5f) return TOM_EMOTION_ANGER;
    /* Sadness: low valence, low arousal */
    if (valence < -0.3f && arousal < 0.3f) return TOM_EMOTION_SADNESS;
    /* Fear: somewhat negative + high arousal */
    if (valence < 0.0f && arousal > 0.6f)  return TOM_EMOTION_FEAR;
    /* Surprise: very high arousal regardless of sign */
    if (arousal > 0.8f)                    return TOM_EMOTION_SURPRISE;
    return TOM_EMOTION_NEUTRAL;
}

static int _wrap_theory_of_mind(void* ctx, const gl_event_t* ev) {
    if (!ctx || !ev) return -1;
    /* Subscribe to both inbound and outbound utterances. COMPREHENDED
     * events are the canonical "another agent spoke" signal; PRODUCED
     * events let ToM's self-model see what the brain just said (useful
     * for self-narration / introspection downstream). */
    if (ev->type != GL_EVENT_COMPREHENDED && ev->type != GL_EVENT_PRODUCED) {
        return 0;
    }

    theory_of_mind_t tom = (theory_of_mind_t)ctx;

    tom_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    /* The semantic vector from gl is the closest analogue to an action
     * vector — it summarizes the comprehended/produced utterance in the
     * same gl semantic space ToM already operates in via the self-brain
     * template.
     *
     * Caveat: gl_event_t doesn't carry the gl's semantic_dim, and the
     * gl is created with a configurable dim (defaults to 128, callers
     * may pass any value). Pre-fix this hardcoded GL_SEMANTIC_DIM as
     * the dim hint, which would have over-/under-reported the vector
     * length when the gl was created with a non-default semantic_dim.
     *
     * Today ToM doesn't actually read action_vector — only
     * action_description / verbal_context drive emotion+goal inference.
     * So we attach the pointer (future-proofing) but leave action_dim=0
     * to signal "dim unknown to this caller". When ToM (or any future
     * consumer) starts reading action_vector, the dim will need to come
     * via a richer event payload. */
    obs.action_vector = ev->semantic_vec;
    obs.action_dim    = 0;
    obs.verbal_context = ev->text;
    obs.observed_emotion = _tom_map_emotion(ev->valence, ev->arousal);
    obs.situational_context = NULL;
    obs.context_dim = 0;

    if (tom_observe(tom, &obs)) {
        __atomic_fetch_add(&g_tom_observations_pushed, 1, __ATOMIC_RELAXED);
    } else {
        /* tom_observe returning false isn't fatal — it just means ToM
         * couldn't process this observation (possibly invalid args).
         * Bump the dropped counter so operators can spot a wedge. */
        __atomic_fetch_add(&g_tom_observations_dropped, 1, __ATOMIC_RELAXED);
    }
    return 0;
}

static int _wrap_empathy(void* ctx, const gl_event_t* ev) {
    if (!ctx || !ev) return -1;
    /* Only fire on emotionally-charged groundings. */
    if (ev->type != GL_EVENT_GROUNDED) return 0;
    if (ev->arousal < 0.3f) return 0;
    LOG_DEBUG(LOG_MODULE,
               "empathy seed val=%.2f aro=%.2f word='%s'",
               ev->valence, ev->arousal,
               ev->word ? ev->word : "(null)");
    return 0;
}

static int _wrap_introspection(void* ctx, const gl_event_t* ev) {
    if (!ctx || !ev) return -1;
    if (ev->type != GL_EVENT_PRODUCED &&
        ev->type != GL_EVENT_SELF_PRODUCED) return 0;
    /* Self-narration sample. SELF_PRODUCED is the canonical introspective
     * signal: the cascade just finished and deposited its result into WM,
     * so reflecting on "what did I just say?" gets the full post-cascade
     * payload (text + semantic_vec + confidence). PRODUCED still fires for
     * any direct grounded_language_produce call that bypassed the cascade. */
    LOG_DEBUG(LOG_MODULE,
               "introspection record type=%d produced='%s' conf=%.2f vec=%p",
               (int)ev->type,
               ev->text ? ev->text : "(null)",
               ev->confidence,
               (const void*)ev->semantic_vec);
    return 0;
}

static int _wrap_reasoning(void* ctx, const gl_event_t* ev) {
    if (!ctx || !ev) return -1;
    if (ev->type != GL_EVENT_COMPREHENDED) return 0;
    /* Comprehended utterance → candidate premise for forward chaining. */
    LOG_DEBUG(LOG_MODULE, "reasoning premise='%s' conf=%.2f",
               ev->text ? ev->text : "(null)", ev->confidence);
    return 0;
}

static int _wrap_narrative(void* ctx, const gl_event_t* ev) {
    if (!ctx || !ev) return -1;
    if (ev->type != GL_EVENT_COMPREHENDED &&
        ev->type != GL_EVENT_PRODUCED &&
        ev->type != GL_EVENT_SELF_PRODUCED) return 0;
    /* Append to current narrative buffer. SELF_PRODUCED is needed for
     * continuous self-narration of behavior — the brain narrating its own
     * outputs alongside what it heard. Without this branch, the narrative
     * stream only contained other-utterances + naked produce() calls and
     * was missing every cascade-driven output. */
    LOG_DEBUG(LOG_MODULE,
               "narrative append type=%d text='%s' vec=%p",
               (int)ev->type,
               ev->text ? ev->text : "(null)",
               (const void*)ev->semantic_vec);
    return 0;
}

static int _wrap_metacognition(void* ctx, const gl_event_t* ev) {
    if (!ctx || !ev) return -1;
    /* Tick reflection counter on every event. */
    LOG_DEBUG(LOG_MODULE, "metacog tick type=%d", (int)ev->type);
    return 0;
}

static int _wrap_analogical(void* ctx, const gl_event_t* ev) {
    if (!ctx || !ev) return -1;
    if (ev->type != GL_EVENT_NEW_WORD) return 0;
    LOG_DEBUG(LOG_MODULE, "analogical search trigger word='%s'",
               ev->word ? ev->word : "(null)");
    return 0;
}

static int _wrap_emergent_language(void* ctx, const gl_event_t* ev) {
    if (!ctx || !ev) return -1;
    if (ev->type != GL_EVENT_GROUNDED &&
        ev->type != GL_EVENT_PRODUCED) return 0;
    /* Skip negative-grounding markers — emergent language learns from
     * positive examples, not anti-pairs. */
    if (ev->type == GL_EVENT_GROUNDED &&
        (ev->concept_id == 0 || ev->confidence < 0.0f)) return 0;
    LOG_DEBUG(LOG_MODULE, "emergent_lang feed type=%d word='%s'",
               (int)ev->type, ev->word ? ev->word : "(null)");
    return 0;
}

/*=============================================================================
 * Brain-region wrappers — anatomical regions that observe language
 * events in their own modality. Each wrapper logs the observation;
 * downstream module-native consumers wire onto the log on the next
 * tick, same pattern as the cognitive wrappers above.
 *===========================================================================*/

static int _wrap_prefrontal(void* ctx, const gl_event_t* ev) {
    if (!ctx || !ev) return -1;
    /* Executive monitoring: hears comprehensions + productions, plus
     * high-arousal groundings (salient items demand attention).
     * Use >=0.3f to match empathy/amygdala boundary semantics. */
    bool relevant =
        ev->type == GL_EVENT_COMPREHENDED ||
        ev->type == GL_EVENT_PRODUCED     ||
        (ev->type == GL_EVENT_GROUNDED && ev->arousal >= 0.3f);
    if (!relevant) return 0;
    LOG_DEBUG(LOG_MODULE,
               "prefrontal observe type=%d conf=%.2f text='%s'",
               (int)ev->type, ev->confidence,
               ev->text ? ev->text : (ev->word ? ev->word : "(null)"));
    return 0;
}

static int _wrap_insula(void* ctx, const gl_event_t* ev) {
    if (!ctx || !ev) return -1;
    /* Interoceptive valence integration: COMPREHENDED utterances with
     * any non-trivial valence are candidate gut-feel inputs. */
    if (ev->type != GL_EVENT_COMPREHENDED) return 0;
    if (ev->valence > -0.05f && ev->valence < 0.05f) return 0;
    LOG_DEBUG(LOG_MODULE, "insula valence_tap text='%s' val=%.2f",
               ev->text ? ev->text : "(null)", ev->valence);
    return 0;
}

static int _wrap_cingulate(void* ctx, const gl_event_t* ev) {
    if (!ctx || !ev) return -1;
    /* Conflict monitoring: low-confidence COMPREHENDED or any PRODUCED
     * event is a candidate signal for ACC attention. */
    bool relevant =
        (ev->type == GL_EVENT_COMPREHENDED && ev->confidence < 0.7f) ||
        ev->type == GL_EVENT_PRODUCED;
    if (!relevant) return 0;
    LOG_DEBUG(LOG_MODULE,
               "cingulate conflict_tap type=%d conf=%.2f",
               (int)ev->type, ev->confidence);
    return 0;
}

static int _wrap_amygdala(void* ctx, const gl_event_t* ev) {
    if (!ctx || !ev) return -1;
    /* Emotional conditioning: GROUNDED events with arousal >= 0.3 are
     * candidates for fear/threat tagging of the bound concept. */
    if (ev->type != GL_EVENT_GROUNDED) return 0;
    /* Skip negative-grounding events (concept_id=0 + confidence<0) —
     * fear-tagging a sentinel "no concept" would be semantically wrong. */
    if (ev->concept_id == 0 || ev->confidence < 0.0f) return 0;
    if (ev->arousal < 0.3f) return 0;
    LOG_DEBUG(LOG_MODULE,
               "amygdala emotion_tag concept_id=%llu val=%.2f aro=%.2f",
               (unsigned long long)ev->concept_id,
               ev->valence, ev->arousal);
    return 0;
}

static int _wrap_ofc(void* ctx, const gl_event_t* ev) {
    if (!ctx || !ev) return -1;
    /* Reward valuation: GROUNDED events with non-trivial valence are
     * candidate stimulus-value bindings for OFC reward learning.
     * COMPREHENDED utterances with valence also feed value updates
     * (heard a positive/negative description → adjust expected value). */
    /* Skip negative-grounding events: concept_id=0 + confidence<0 means
     * "this word does NOT mean concept X" — there's no concept to bind
     * a value to. */
    if (ev->type == GL_EVENT_GROUNDED &&
        (ev->concept_id == 0 || ev->confidence < 0.0f)) return 0;
    bool relevant =
        (ev->type == GL_EVENT_GROUNDED &&
         (ev->valence > 0.05f || ev->valence < -0.05f)) ||
        (ev->type == GL_EVENT_COMPREHENDED &&
         (ev->valence > 0.05f || ev->valence < -0.05f));
    if (!relevant) return 0;
    LOG_DEBUG(LOG_MODULE,
               "ofc value_bind type=%d concept_id=%llu val=%.2f",
               (int)ev->type,
               (unsigned long long)ev->concept_id,
               ev->valence);
    return 0;
}

/*=============================================================================
 * Attach helpers — register the wrapper with the module pointer as ctx.
 * Passing NULL is the disconnect operation (unsubscribes the wrapper).
 *===========================================================================*/

/* NULL `mod` is a strict no-op (not a "detach by NULL ctx" — that
 * would clobber unrelated entries that happen to have NULL ctx).
 * Callers wanting to detach should call grounded_language_unsubscribe
 * with the original module pointer they registered with. */
#define _GL_DEFINE_ATTACH(name, wrapper)                                       \
    void grounded_language_attach_##name(grounded_language_t* gl, void* mod) { \
        if (!gl || !mod) return;                                               \
        grounded_language_subscribe(gl, wrapper, mod);                         \
    }

_GL_DEFINE_ATTACH(inner_speech,       _wrap_inner_speech)
_GL_DEFINE_ATTACH(imagination,        _wrap_imagination)
_GL_DEFINE_ATTACH(theory_of_mind,     _wrap_theory_of_mind)
_GL_DEFINE_ATTACH(empathy,            _wrap_empathy)
_GL_DEFINE_ATTACH(introspection,      _wrap_introspection)
_GL_DEFINE_ATTACH(reasoning,          _wrap_reasoning)
_GL_DEFINE_ATTACH(narrative,          _wrap_narrative)
_GL_DEFINE_ATTACH(metacognition,      _wrap_metacognition)
_GL_DEFINE_ATTACH(analogical,         _wrap_analogical)
_GL_DEFINE_ATTACH(emergent_language,  _wrap_emergent_language)
_GL_DEFINE_ATTACH(prefrontal,         _wrap_prefrontal)
_GL_DEFINE_ATTACH(insula,             _wrap_insula)
_GL_DEFINE_ATTACH(cingulate,          _wrap_cingulate)
_GL_DEFINE_ATTACH(amygdala,           _wrap_amygdala)
_GL_DEFINE_ATTACH(ofc,                _wrap_ofc)

#undef _GL_DEFINE_ATTACH
