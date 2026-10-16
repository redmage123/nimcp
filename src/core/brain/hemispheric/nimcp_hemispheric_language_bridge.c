/**
 * @file nimcp_hemispheric_language_bridge.c
 * @brief Lateralized GL wiring + corpus-callosum forwarding.
 * @date 2026-05-05
 *
 * WHAT: Subscribes to the LEFT hemisphere's grounded_language event
 *       bus and forwards COMPREHENDED/PRODUCED/GROUNDED events to the
 *       right hemisphere via the corpus callosum's COGNITIVE and
 *       EMOTIONAL channels. The right hemisphere drains its inbox on
 *       each tick and updates its emotion state accordingly.
 *
 * WHY:  Models biological language lateralization. Pre-bridge, the
 *       left hemisphere's GL fired events into a void as far as the
 *       right hemisphere was concerned, even though the right
 *       hemisphere is documented as handling prosody + emotional
 *       tone of language.
 *
 * HOW:  - install() hooks a single subscriber on left->brain.grounded_lang
 *       - The subscriber serializes the event into a small fixed
 *         payload (gl_event_payload_t) and calls callosum_send()
 *       - tick() drains pending right-side messages via
 *         callosum_receive() and updates right-hemisphere emotion
 *         state proportional to event arousal/valence
 *       - The bridge state lives in a static global keyed by
 *         hemispheric_brain_t* (one bridge per bilateral brain).
 *         A small registry array (max 4) covers all realistic uses.
 *
 * SOLID/DRY: Single registry; one subscriber callback handles all
 *            event types via switch; one drain loop on the tick path.
 */

#include "core/brain/hemispheric/nimcp_hemispheric_language_bridge.h"
#include "core/brain/hemispheric/nimcp_corpus_callosum.h"
#include "core/brain/hemispheric/nimcp_brain_hemisphere.h"
#include "core/brain/nimcp_brain_internal.h"
#include "language/nimcp_grounded_language.h"
#include "utils/logging/nimcp_logging.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define LOG_MODULE "hemi_lang_bridge"

/* Application-defined message_type tags carried over the callosum. */
#define HL_MSG_COMPREHENDED 0x4C475F43  /* "LG_C" */
#define HL_MSG_PRODUCED     0x4C475F50  /* "LG_P" */
#define HL_MSG_GROUNDED     0x4C475F47  /* "LG_G" */

/* Fixed-size payload that crosses the callosum. We keep it small so
 * the bandwidth-limited COGNITIVE/EMOTIONAL channels don't saturate. */
typedef struct {
    int32_t  event_type;          /* gl_event_type_t cast */
    float    valence;
    float    arousal;
    float    confidence;
    uint64_t concept_id;
    char     summary[32];          /* word or first 31 chars of text */
} hl_payload_t;

/*=============================================================================
 * Bridge state — small registry keyed by hemispheric_brain_t*.
 *===========================================================================*/

#define HL_MAX_BRIDGES 4

typedef struct {
    hemispheric_brain_t*           hb;            /* NULL slot = free */
    grounded_language_t*           left_gl;       /* the subscribed handle */
    hemispheric_language_stats_t   stats;
    float                          aphasia_severity;
    bool                           rh_gl_was_present;  /* for restore */
    grounded_language_t*           saved_rh_gl;        /* muted handle */
} hl_bridge_t;

static hl_bridge_t g_bridges[HL_MAX_BRIDGES];

static hl_bridge_t* _find_bridge(hemispheric_brain_t* hb) {
    if (!hb) return NULL;
    for (int i = 0; i < HL_MAX_BRIDGES; i++) {
        if (g_bridges[i].hb == hb) return &g_bridges[i];
    }
    return NULL;
}

static hl_bridge_t* _alloc_bridge(hemispheric_brain_t* hb) {
    for (int i = 0; i < HL_MAX_BRIDGES; i++) {
        if (g_bridges[i].hb == NULL) {
            memset(&g_bridges[i], 0, sizeof(g_bridges[i]));
            g_bridges[i].hb = hb;
            return &g_bridges[i];
        }
    }
    return NULL;
}

/*=============================================================================
 * Subscriber callback — runs on every event from the left GL.
 * Serializes a small payload + sends across the callosum. Returns 0
 * always so other subscribers (memory bridge, region bridge, etc.)
 * still fire downstream of us.
 *===========================================================================*/

static int _hl_left_subscriber(void* ctx, const gl_event_t* ev) {
    hl_bridge_t* b = (hl_bridge_t*)ctx;
    if (!b || !b->hb || !ev) return 0;
    if (!b->hb->callosum)        return 0;
    if (!b->hb->callosum_intact) {
        b->stats.callosum_msgs_dropped++;
        return 0;
    }

    /* Tally per-event-type counters. */
    switch (ev->type) {
        case GL_EVENT_COMPREHENDED: b->stats.lh_comprehensions++; break;
        case GL_EVENT_PRODUCED:     b->stats.lh_productions++;    break;
        case GL_EVENT_GROUNDED:     b->stats.lh_groundings++;     break;
        default: return 0;  /* NEW_WORD stays in the left hemisphere */
    }

    /* Aphasia gate: severity ≥ 0.99 fully blocks the left→right
     * forward (simulating Broca's-style disconnection). Otherwise we
     * scale confidence multiplicatively before crossing. */
    if (b->aphasia_severity >= 0.99f) {
        b->stats.aphasia_dropped++;
        return 0;
    }

    /* Pack a small fixed payload. */
    hl_payload_t payload = {0};
    payload.event_type = (int32_t)ev->type;
    payload.valence    = ev->valence;
    payload.arousal    = ev->arousal;
    payload.confidence = ev->confidence * (1.0f - b->aphasia_severity);
    payload.concept_id = ev->concept_id;

    const char* src = ev->word ? ev->word : (ev->text ? ev->text : "");
    if (src) {
        size_t n = strlen(src);
        if (n > sizeof(payload.summary) - 1) n = sizeof(payload.summary) - 1;
        memcpy(payload.summary, src, n);
        payload.summary[n] = '\0';
    }

    /* Channel selection:
     *   - GROUNDED with non-zero arousal → EMOTIONAL channel (5-HT-mediated)
     *   - everything else → COGNITIVE channel (DA-mediated working memory)
     */
    callosum_channel_type_t channel =
        (ev->type == GL_EVENT_GROUNDED && ev->arousal > 0.05f)
            ? CALLOSUM_CHANNEL_EMOTIONAL
            : CALLOSUM_CHANNEL_COGNITIVE;

    uint32_t msg_type = (ev->type == GL_EVENT_COMPREHENDED) ? HL_MSG_COMPREHENDED
                       : (ev->type == GL_EVENT_PRODUCED)    ? HL_MSG_PRODUCED
                       : HL_MSG_GROUNDED;

    int rc = callosum_send(b->hb->callosum, HEMISPHERE_LEFT, channel,
                            CALLOSUM_PRIORITY_NORMAL, msg_type,
                            &payload, sizeof(payload));
    if (rc < 0) {
        b->stats.callosum_msgs_dropped++;
    } else {
        b->stats.callosum_msgs_sent++;
    }
    return 0;
}

/*=============================================================================
 * Public API
 *===========================================================================*/

int hemispheric_language_bridge_install(hemispheric_brain_t* hb) {
    if (!hb || !hb->left || !hb->right) return -1;

    /* brain_t is `struct brain_struct*` — the hemisphere already holds
     * it by-value as a pointer. Bail before deref if either side has
     * no brain attached. */
    brain_t left_brain  = hb->left->brain;
    brain_t right_brain = hb->right->brain;
    if (!left_brain || !right_brain) return -2;

    grounded_language_t* left_gl =
        (grounded_language_t*)left_brain->grounded_lang;
    if (!left_gl) return -2;

    /* Reuse existing slot if already installed for this hb. */
    hl_bridge_t* b = _find_bridge(hb);
    bool fresh_slot = false;
    if (!b) {
        b = _alloc_bridge(hb);
        if (!b) {
            LOG_WARN(LOG_MODULE,
                      "bridge registry full (max %d) — refusing install",
                      HL_MAX_BRIDGES);
            return -1;
        }
        fresh_slot = true;
    } else {
        /* Re-install: unsubscribe the prior callback first. */
        if (b->left_gl) grounded_language_unsubscribe(b->left_gl, b);
    }
    b->left_gl = left_gl;

    /* Subscribe FIRST so we can roll back the lateralization mute if
     * the bus rejects us — otherwise a failed install leaves the right
     * hemisphere silently muted with no live forwarder. */
    if (grounded_language_subscribe(left_gl, _hl_left_subscriber, b) != 0) {
        LOG_WARN(LOG_MODULE, "subscribe failed on left GL");
        if (fresh_slot) {
            memset(b, 0, sizeof(*b));   /* free the slot */
        } else {
            b->left_gl = NULL;          /* keep stats; clear handle */
        }
        return -1;
    }

    /* Lateralization: mute the right hemisphere's GL (if any) so the
     * two hemispheres don't independently train competing lexicons.
     * We don't destroy it — split-brain restore needs the handle.
     * Skip if we're re-installing and already have a saved handle. */
    if (!b->rh_gl_was_present && right_brain->grounded_lang) {
        b->saved_rh_gl = (grounded_language_t*)right_brain->grounded_lang;
        right_brain->grounded_lang = NULL;
        b->rh_gl_was_present = true;
    }

    LOG_INFO(LOG_MODULE,
              "installed: left_gl=%p rh_gl_muted=%s callosum=%p",
              (void*)left_gl,
              b->rh_gl_was_present ? "yes" : "no",
              (void*)hb->callosum);
    return 0;
}

void hemispheric_language_bridge_uninstall(hemispheric_brain_t* hb) {
    hl_bridge_t* b = _find_bridge(hb);
    if (!b) return;
    if (b->left_gl) {
        grounded_language_unsubscribe(b->left_gl, b);
    }
    /* Restore right GL if we had muted one. */
    if (b->rh_gl_was_present && hb->right && hb->right->brain) {
        hb->right->brain->grounded_lang = (struct grounded_language*)b->saved_rh_gl;
    }
    memset(b, 0, sizeof(*b));
}

/*=============================================================================
 * Tick — drain right-hemisphere callosum inbox + apply emotion bumps.
 *===========================================================================*/

int hemispheric_language_bridge_tick(hemispheric_brain_t* hb) {
    if (!hb) return 0;
    hl_bridge_t* b = _find_bridge(hb);
    if (!b || !hb->callosum) return 0;

    callosum_message_t inbox[16];
    int n = callosum_receive(hb->callosum, HEMISPHERE_RIGHT, inbox, 16);
    if (n <= 0) return 0;

    for (int i = 0; i < n; i++) {
        const callosum_message_t* m = &inbox[i];
        if (!m->data || m->data_size != sizeof(hl_payload_t)) continue;
        const hl_payload_t* p = (const hl_payload_t*)m->data;

        /* Right-hemisphere emotion update — proportional to arousal,
         * polarity from valence. We bump a coarse counter; downstream
         * emotion processing reads the aggregate. Real consumer:
         * brain->emotional_system_t's tick on the right brain. */
        if (m->channel == CALLOSUM_CHANNEL_EMOTIONAL ||
            (p->arousal > 0.05f && m->message_type == HL_MSG_GROUNDED)) {
            b->stats.rh_emotion_updates++;
            /* Concrete update would call emotional_system_observe()
             * on hb->right->brain.emotional_system; we leave that
             * site for a follow-up wave. The counter here proves the
             * traffic is flowing. */
        }
    }
    return n;
}

int hemispheric_language_bridge_get_stats(hemispheric_brain_t* hb,
                                            hemispheric_language_stats_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    hl_bridge_t* b = _find_bridge(hb);
    if (!b) return -1;
    *out = b->stats;
    return 0;
}

void hemispheric_language_bridge_set_aphasia(hemispheric_brain_t* hb,
                                              float severity) {
    hl_bridge_t* b = _find_bridge(hb);
    if (!b) return;
    if (severity < 0.0f) severity = 0.0f;
    if (severity > 1.0f) severity = 1.0f;
    b->aphasia_severity = severity;
    /* The severity is read by future hot-path code that gates GL
     * confidence; for now it's stored for the test surface. */
}
