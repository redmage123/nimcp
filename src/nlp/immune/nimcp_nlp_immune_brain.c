/**
 * @file nimcp_nlp_immune_brain.c
 * @brief Live-brain driver for the NLP-immune bridge. See the header.
 */
#include "nlp/immune/nimcp_nlp_immune_brain.h"
#include "nlp/immune/nimcp_nlp_immune_bridge.h"
#include "nlp/nimcp_multimodal_nlp_bridge.h"
#include "cognitive/language/nimcp_emergent_language_bridge.h"
#include "language/nimcp_grounded_language.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct nlp_immune_brain {
    nlp_immune_bridge_t* bridge;
    nimcp_mutex_t* lock;
    uint64_t calls, ticks;
    uint64_t seen_em_pred, seen_em_hits, seen_sp_pred, seen_sp_hits;
    uint64_t last_ms;
    float    capacity;
};

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static bool enabled(void) {
    static int on = -1;
    if (on < 0) { const char* v = getenv("NIMCP_NLP_IMMUNE"); on = (v && v[0] == '1'); }
    return on == 1;
}

nlp_immune_brain_t* nlp_immune_brain_get(void* const* slot) {
    return slot ? (nlp_immune_brain_t*)__atomic_load_n(slot, __ATOMIC_ACQUIRE) : NULL;
}

nlp_immune_brain_t* nlp_immune_brain_for(void** slot, struct brain_struct* brain) {
    if (!enabled() || !slot || !brain || !brain->immune_system) return NULL;
    nlp_immune_brain_t* nb = nlp_immune_brain_get(slot);
    if (nb) return nb;
    nlp_immune_brain_t* fresh = nimcp_calloc(1, sizeof(*fresh));
    if (!fresh) return NULL;
    fresh->bridge = nlp_immune_bridge_create(NULL, brain->immune_system, brain->nlp_network);
    fresh->lock = nimcp_mutex_create(NULL);
    fresh->capacity = 1.0f;
    if (!fresh->bridge || !fresh->lock) { nlp_immune_brain_destroy(fresh); return NULL; }
    nlp_immune_brain_t* expected = NULL;
    if (__atomic_compare_exchange_n((nlp_immune_brain_t**)slot, &expected, fresh, false,
                                    __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        LOG_INFO("[NLP_IMMUNE] enabled (cytokine clearance %s)",
                 brain_immune_cytokine_clearance_enabled() ? "on" : "OFF: capacity held at 1.0");
        return fresh;
    }
    nlp_immune_brain_destroy(fresh);
    return expected;
}

void nlp_immune_brain_destroy(nlp_immune_brain_t* nb) {
    if (!nb) return;
    if (nb->bridge) nlp_immune_bridge_destroy(nb->bridge);
    if (nb->lock) nimcp_mutex_free(nb->lock);
    nimcp_free(nb);
}

/* New (successes, failures) since the last poll from one counter pair. */
static void delta(uint64_t pred, uint64_t hits, uint64_t* seen_pred, uint64_t* seen_hits,
                  uint32_t* succ, uint32_t* fail) {
    if (pred < *seen_pred || hits < *seen_hits) {    /* source reset: resync */
        *seen_pred = pred; *seen_hits = hits;
        return;
    }
    uint64_t dp = pred - *seen_pred, dh = hits - *seen_hits;
    if (dh > dp) dh = dp;
    *succ += (uint32_t)dh;
    *fail += (uint32_t)(dp - dh);
    *seen_pred = pred; *seen_hits = hits;
}

void nlp_immune_brain_tick(nlp_immune_brain_t* nb, struct brain_struct* brain) {
    if (!nb || !brain) return;
    if (__atomic_add_fetch(&nb->calls, 1, __ATOMIC_RELAXED) % NLP_IMMUNE_BRAIN_PERIOD) return;
    nimcp_mutex_lock(nb->lock);
    uint32_t succ = 0, fail = 0;

    emergent_bridge_t* eb = emergent_bridge_get(&brain->emergent_bridge);
    emergent_bridge_stats_t es;
    if (eb && emergent_bridge_get_stats(eb, &es) == 0)
        delta(es.predictions, es.hits, &nb->seen_em_pred, &nb->seen_em_hits, &succ, &fail);

    multimodal_nlp_brain_t* mb = multimodal_nlp_brain_get(&brain->multimodal_nlp_state);
    multimodal_nlp_brain_stats_t ms;
    if (mb && multimodal_nlp_brain_get_stats(mb, brain->speech_cortex, &ms) == 0)
        delta(ms.predictions, ms.hits, &nb->seen_sp_pred, &nb->seen_sp_hits, &succ, &fail);

    nlp_immune_record_outcomes(nb->bridge, succ, fail);

    uint64_t t = now_ms();
    uint64_t dt = nb->last_ms ? t - nb->last_ms : 0;
    nb->last_ms = t;
    nlp_immune_bridge_update(nb->bridge, dt);
    nb->capacity = nlp_immune_compute_capacity(nb->bridge);
    if (brain->grounded_lang)
        grounded_language_set_language_capacity(brain->grounded_lang, nb->capacity);
    nb->ticks++;
    nimcp_mutex_unlock(nb->lock);
}

int nlp_immune_brain_get_stats(nlp_immune_brain_t* nb, nlp_immune_brain_stats_t* out) {
    if (!nb || !out) return -1;
    memset(out, 0, sizeof(*out));
    nimcp_mutex_lock(nb->lock);
    const nlp_immune_bridge_t* b = nb->bridge;
    out->clearance = brain_immune_cytokine_clearance_enabled();
    out->capacity = nb->capacity;
    out->inflammation_level = (uint32_t)b->inflammation_state.current_level;
    out->outcomes = b->nlp_modulation.total_processed;
    out->successes = b->nlp_modulation.successful_parses;
    out->last_window_error = b->last_window_error;
    out->baseline_error = b->baseline_error;
    out->il1_releases = b->error_triggers;
    out->il10_releases = b->success_boosts;
    out->ticks = nb->ticks;
    nimcp_mutex_unlock(nb->lock);
    return 0;
}
