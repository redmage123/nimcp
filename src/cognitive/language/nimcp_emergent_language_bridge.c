/**
 * @file nimcp_emergent_language_bridge.c
 * @brief Brain <-> emergent-language wiring. See the header for the design.
 *
 * Cost per decide: one FNV-1a hash of the input, one ring scan (16
 * entries), and observe_ex = vocab x output_size cosine terms (at most
 * 1024 x output_size). Merge/prune run only when the vocabulary is full.
 */
#include "cognitive/language/nimcp_emergent_language_bridge.h"
#include "cognitive/language/nimcp_emergent_language.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define LOG_MODULE "EMERGENT_BRIDGE"
#define RING 16
#define LABEL_LEN 48
#define MAX_LABELS 512
#define SAVE_EVERY 5000

typedef struct {
    uint64_t hash;
    char     label[LABEL_LEN];
} _recent_t;

typedef struct {
    char     label[LABEL_LEN];
    uint64_t count;
} _label_count_t;

struct emergent_bridge {
    nimcp_emergent_language_t* el;   /* created on first decide (needs output size) */
    uint32_t output_size;
    nimcp_mutex_t* lock;             /* guards everything below + lazy create */

    _recent_t recent[RING];
    uint32_t  recent_next;

    _label_count_t labels[MAX_LABELS];
    uint32_t  n_labels;
    uint32_t  majority;              /* index into labels */

    uint64_t observations, labeled_decides, predictions, hits, majority_hits;
    char path[512];
};

static uint64_t _hash(const float* v, uint32_t n) {
    const unsigned char* p = (const unsigned char*)v;
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < (size_t)n * sizeof(float); i++) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h ^ n;
}

emergent_bridge_t* emergent_bridge_create_from_env(void) {
    const char* on = getenv("NIMCP_EMERGENT_LANGUAGE");
    if (!on || on[0] != '1') return NULL;
    emergent_bridge_t* b = nimcp_calloc(1, sizeof(*b));
    if (!b) return NULL;
    b->lock = nimcp_mutex_create(NULL);
    if (!b->lock) { nimcp_free(b); return NULL; }
    const char* path = getenv("NIMCP_EMERGENT_LANGUAGE_PATH");
    if (path && path[0]) snprintf(b->path, sizeof(b->path), "%s", path);
    LOG_INFO("[%s] enabled (persistence: %s)", LOG_MODULE, b->path[0] ? b->path : "off");
    return b;
}

emergent_bridge_t* emergent_bridge_for(void** slot) {
    static int enabled = -1;                 /* env read once; benign race */
    if (enabled < 0) {
        const char* on = getenv("NIMCP_EMERGENT_LANGUAGE");
        enabled = (on && on[0] == '1') ? 1 : 0;
    }
    if (!enabled || !slot) return NULL;
    emergent_bridge_t* b = __atomic_load_n((emergent_bridge_t**)slot, __ATOMIC_ACQUIRE);
    if (b) return b;
    emergent_bridge_t* fresh = emergent_bridge_create_from_env();
    if (!fresh) return NULL;
    emergent_bridge_t* expected = NULL;
    if (__atomic_compare_exchange_n((emergent_bridge_t**)slot, &expected, fresh, false,
                                    __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
        return fresh;
    emergent_bridge_destroy(fresh);          /* another thread won */
    return expected;
}

emergent_bridge_t* emergent_bridge_get(void* const* slot) {
    return slot ? (emergent_bridge_t*)__atomic_load_n(slot, __ATOMIC_ACQUIRE) : NULL;
}

static void _save(emergent_bridge_t* b) {
    if (b->el && b->path[0]) {
        char tmp[600];
        snprintf(tmp, sizeof(tmp), "%s.tmp", b->path);
        if (nimcp_emergent_save(b->el, tmp) == 0 && rename(tmp, b->path) != 0)
            LOG_WARN("[%s] rename %s -> %s failed", LOG_MODULE, tmp, b->path);
    }
}

void emergent_bridge_destroy(emergent_bridge_t* b) {
    if (!b) return;
    _save(b);
    if (b->el) nimcp_emergent_language_destroy(b->el);
    nimcp_mutex_free(b->lock);
    nimcp_free(b);
}

void emergent_bridge_on_learn(emergent_bridge_t* b, const float* features,
                              uint32_t num_features, const char* label) {
    if (!b || !features || num_features == 0 || !label || !label[0]) return;
    uint64_t h = _hash(features, num_features);
    nimcp_mutex_lock(b->lock);
    _recent_t* r = &b->recent[b->recent_next++ % RING];
    r->hash = h;
    snprintf(r->label, LABEL_LEN, "%s", label);
    nimcp_mutex_unlock(b->lock);
}

/* Count a label; keep the running majority. Called with lock held. */
static void _count_label(emergent_bridge_t* b, const char* label) {
    uint32_t i;
    for (i = 0; i < b->n_labels; i++)
        if (strcmp(b->labels[i].label, label) == 0) break;
    if (i == b->n_labels) {
        if (b->n_labels == MAX_LABELS) return;   /* table full: stop tracking new labels */
        snprintf(b->labels[i].label, LABEL_LEN, "%s", label);
        b->n_labels++;
    }
    b->labels[i].count++;
    if (b->labels[i].count > b->labels[b->majority].count) b->majority = i;
}

void emergent_bridge_on_decide(emergent_bridge_t* b, const float* features,
                               uint32_t num_features, const float* output,
                               uint32_t output_size) {
    if (!b || !features || !output || output_size == 0) return;
    uint64_t h = _hash(features, num_features);

    nimcp_mutex_lock(b->lock);
    if (!b->el) {
        nimcp_emergent_config_t cfg = nimcp_emergent_config_default();
        cfg.centroid_dim = output_size;
        cfg.embed_dim = output_size < 256 ? output_size : 256;
        cfg.use_unicode_symbols = true;
        b->el = nimcp_emergent_language_create(&cfg);
        b->output_size = output_size;
        if (b->el && b->path[0]) {
            FILE* f = fopen(b->path, "rb");
            if (f) {
                fclose(f);
                if (nimcp_emergent_load(b->el, b->path) != 0)
                    LOG_WARN("[%s] could not load %s; starting empty", LOG_MODULE, b->path);
            }
        }
        if (!b->el) { nimcp_mutex_unlock(b->lock); return; }
    }
    if (output_size != b->output_size) {     /* a different head; not this vocabulary */
        nimcp_mutex_unlock(b->lock);
        return;
    }
    char label[LABEL_LEN] = {0};
    for (uint32_t i = 0; i < RING; i++) {
        if (b->recent[i].label[0] && b->recent[i].hash == h) {
            snprintf(label, LABEL_LEN, "%s", b->recent[i].label);
            break;
        }
    }

    uint32_t tok = UINT32_MAX;
    nimcp_emergent_observe_ex(b->el, output, output_size, &tok);
    b->observations++;

    if (label[0] && tok != UINT32_MAX) {
        b->labeled_decides++;
        if (b->n_labels > 0 && strcmp(b->labels[b->majority].label, label) == 0)
            b->majority_hits++;
        nimcp_emergent_translation_t tr;
        int had_gloss = (nimcp_emergent_get_translation(b->el, tok, &tr) == 0 &&
                         tr.human_gloss[0] != '\0');
        int m = nimcp_emergent_vote_human(b->el, tok, label);
        if (had_gloss) {
            b->predictions++;
            if (m == 1) b->hits++;
        }
        _count_label(b, label);
    }
    if (b->path[0] && b->observations % SAVE_EVERY == 0) _save(b);
    nimcp_mutex_unlock(b->lock);
}

int emergent_bridge_get_stats(emergent_bridge_t* b, emergent_bridge_stats_t* out) {
    if (!b || !out) return -1;
    memset(out, 0, sizeof(*out));
    nimcp_mutex_lock(b->lock);
    if (b->el) {
        nimcp_emergent_stats_t s;
        if (nimcp_emergent_get_stats(b->el, &s) == 0) {
            out->vocab_size = s.vocab_size;
            out->active_tokens = s.active_tokens;
        }
        nimcp_emergent_get_vote_stats(b->el, &out->labeled_tokens, &out->purity);
    }
    out->observations = b->observations;
    out->labeled_decides = b->labeled_decides;
    out->predictions = b->predictions;
    out->hits = b->hits;
    out->majority_hits = b->majority_hits;
    out->distinct_labels = b->n_labels;
    nimcp_mutex_unlock(b->lock);
    return 0;
}

int emergent_bridge_express(emergent_bridge_t* b, const float* output,
                            uint32_t output_size, char* text, uint32_t max_len) {
    if (!b || !output || !text || max_len == 0) return -1;
    text[0] = '\0';
    nimcp_mutex_lock(b->lock);
    int n = b->el ? nimcp_emergent_express_symbols(b->el, output, output_size, text, max_len) : 0;
    nimcp_mutex_unlock(b->lock);
    return n;
}
