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
#include <stdbool.h>
#include <time.h>
#include <ctype.h>

#define LOG_MODULE "EMERGENT_BRIDGE"
#define RING 16
#define LABEL_LEN 48
#define MAX_LABELS 512
#define SAVE_EVERY 5000
#define SAVE_SECS_DEFAULT 300        /* the daemon is SIGKILLed on restart: save on a timer too */
#define SEEN_BITS 16                 /* 65536-slot set of input hashes already observed */
#define SEEN_SLOTS (1u << SEEN_BITS)
#define SIM_BUCKETS 11               /* [none] + cosine deciles <0.1 .. >=0.9 */
#define SIM_LOG_EVERY 1000
#define MAX_LABEL_WORDS 8            /* content words voted per label */

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
    uint64_t decide_observations, learn_observations;

    /* Generalisation: labelled observations whose input was never observed
     * before (not a replay of a seen item). */
    uint64_t* seen;                  /* open-addressed input-hash set; 0 = empty */
    uint32_t  n_seen;
    uint64_t  novel_labeled, novel_predictions, novel_hits, novel_majority_hits;

    /* Nearest-token cosine at observe time (bucket 0: no token yet). */
    uint64_t  sim_hist[SIM_BUCKETS];
    uint64_t  new_tokens;            /* observations that created a token */

    uint64_t  last_save_ms;
    uint32_t  save_secs;
    float     discovery_threshold;   /* 0 = module default */
    bool      label_words;           /* vote each content word, not the whole label */
    char path[512];
};

static uint64_t _now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

/* Insert h; returns true if it was already present. Lock held. When the
 * set is 3/4 full it is cleared (novelty becomes "not seen recently"). */
static bool _seen_before(emergent_bridge_t* b, uint64_t h) {
    if (!b->seen) return false;
    if (h == 0) h = 1;
    if (b->n_seen >= SEEN_SLOTS / 4 * 3) {
        memset(b->seen, 0, SEEN_SLOTS * sizeof(uint64_t));
        b->n_seen = 0;
    }
    uint32_t i = (uint32_t)(h ^ (h >> 32)) & (SEEN_SLOTS - 1);
    while (b->seen[i]) {
        if (b->seen[i] == h) return true;
        i = (i + 1) & (SEEN_SLOTS - 1);
    }
    b->seen[i] = h;
    b->n_seen++;
    return false;
}

static uint64_t _hash(const float* v, uint32_t n) {
    const unsigned char* p = (const unsigned char*)v;
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < (size_t)n * sizeof(float); i++) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h ^ n;
}

/* Function words never make a gloss. */
static bool _stopword(const char* w) {
    static const char* const stop[] = {
        "the", "and", "for", "with", "from", "into", "onto", "over", "under",
        "that", "this", "its", "are", "was", "were", "has", "have", "had",
        "not", "but", "you", "his", "her", "their", "they", "them", "who",
        "what", "when", "where", "which", "while", "then", "than", "there",
        "some", "any", "all", "each", "very", "out", "off", "about", NULL };
    for (int i = 0; stop[i]; i++)
        if (strcmp(w, stop[i]) == 0) return true;
    return false;
}

/* Split a label into its distinct content words: lowercase runs of letters
 * (labels are identifiers like "spotted_cocoon" or plain phrases), at least
 * 3 letters, no digits in the run ("s1" in "video_s1" is dropped), no
 * function words, no repeats ("sensor_sensor_imu" -> sensor, imu is too
 * short). A label with no content word is used whole (lowercased). */
static uint32_t _label_words(const char* label, char out[][LABEL_LEN], uint32_t max) {
    uint32_t n = 0;
    const char* p = label;
    while (*p && n < max) {
        while (*p && !isalnum((unsigned char)*p)) p++;
        const char* start = p;
        bool digit = false;
        while (*p && isalnum((unsigned char)*p)) { if (isdigit((unsigned char)*p)) digit = true; p++; }
        size_t len = (size_t)(p - start);
        if (len < 3 || digit) continue;
        if (len >= LABEL_LEN) len = LABEL_LEN - 1;
        char w[LABEL_LEN];
        for (size_t i = 0; i < len; i++) w[i] = (char)tolower((unsigned char)start[i]);
        w[len] = '\0';
        if (_stopword(w)) continue;
        bool dup = false;
        for (uint32_t i = 0; i < n; i++) if (strcmp(out[i], w) == 0) { dup = true; break; }
        if (!dup) memcpy(out[n++], w, len + 1);
    }
    if (n == 0 && max > 0) {
        size_t i = 0;
        for (; label[i] && i < LABEL_LEN - 1; i++) out[0][i] = (char)tolower((unsigned char)label[i]);
        out[0][i] = '\0';
        n = out[0][0] ? 1 : 0;
    }
    return n;
}

emergent_bridge_t* emergent_bridge_create_from_env(void) {
    const char* on = getenv("NIMCP_EMERGENT_LANGUAGE");
    if (!on || on[0] != '1') return NULL;
    emergent_bridge_t* b = nimcp_calloc(1, sizeof(*b));
    if (!b) return NULL;
    b->lock = nimcp_mutex_create(NULL);
    if (!b->lock) { nimcp_free(b); return NULL; }
    b->seen = nimcp_calloc(SEEN_SLOTS, sizeof(uint64_t));   /* optional: NULL disables novelty */
    const char* path = getenv("NIMCP_EMERGENT_LANGUAGE_PATH");
    if (path && path[0]) snprintf(b->path, sizeof(b->path), "%s", path);
    const char* ss = getenv("NIMCP_EMERGENT_SAVE_SECS");
    b->save_secs = (ss && ss[0]) ? (uint32_t)strtoul(ss, NULL, 10) : SAVE_SECS_DEFAULT;
    const char* dt = getenv("NIMCP_EMERGENT_DISCOVERY");
    if (dt && dt[0]) {
        float v = strtof(dt, NULL);
        if (v > 0.0f && v < 2.0f) b->discovery_threshold = v;
    }
    const char* lw = getenv("NIMCP_EMERGENT_LABEL_WORDS");
    b->label_words = !(lw && lw[0] == '0');
    b->last_save_ms = _now_ms();
    LOG_INFO("[%s] enabled (persistence: %s, timed save %us, discovery %s, labels %s)", LOG_MODULE,
             b->path[0] ? b->path : "off", b->save_secs,
             b->discovery_threshold > 0.0f ? dt : "default",
             b->label_words ? "per content word" : "whole");
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
    b->last_save_ms = _now_ms();
}

void emergent_bridge_destroy(emergent_bridge_t* b) {
    if (!b) return;
    _save(b);
    if (b->el) nimcp_emergent_language_destroy(b->el);
    nimcp_free(b->seen);
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

/* Lazy model creation (first output fixes the dimension). Lock held.
 * Returns false when this output belongs to a different head. */
static bool _ensure_model_locked(emergent_bridge_t* b, uint32_t output_size) {
    if (!b->el) {
        nimcp_emergent_config_t cfg = nimcp_emergent_config_default();
        cfg.centroid_dim = output_size;
        cfg.embed_dim = output_size < 256 ? output_size : 256;
        cfg.use_unicode_symbols = true;
        if (b->discovery_threshold > 0.0f) cfg.discovery_threshold = b->discovery_threshold;
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
        if (!b->el) return false;
    }
    return output_size == b->output_size;
}

/* Observe one output; when labelled, score the token's gloss against the
 * label, then vote. Lock held. */
static void _log_progress(emergent_bridge_t* b) {
    char h[160]; int n = 0;
    for (int i = 0; i < SIM_BUCKETS && n < (int)sizeof(h); i++)
        n += snprintf(h + n, sizeof(h) - (size_t)n, "%s%llu", i ? " " : "",
                      (unsigned long long)b->sim_hist[i]);
    LOG_INFO("[%s] obs=%llu new_tokens=%llu hit=%llu/%llu maj=%llu/%llu "
             "novel: hit=%llu/%llu maj=%llu/%llu | nearest-cos hist [none,<.1..>=.9]: %s",
             LOG_MODULE, (unsigned long long)b->observations, (unsigned long long)b->new_tokens,
             (unsigned long long)b->hits, (unsigned long long)b->predictions,
             (unsigned long long)b->majority_hits, (unsigned long long)b->labeled_decides,
             (unsigned long long)b->novel_hits, (unsigned long long)b->novel_predictions,
             (unsigned long long)b->novel_majority_hits, (unsigned long long)b->novel_labeled, h);
}

static void _observe_locked(emergent_bridge_t* b, const float* output,
                            uint32_t output_size, const char* label, uint64_t input_hash) {
    uint32_t vocab_before = nimcp_emergent_get_vocab_size(b->el);
    uint32_t tok = UINT32_MAX;
    nimcp_emergent_observe_ex(b->el, output, output_size, &tok);
    if (tok == UINT32_MAX) return;           /* near-zero output: not observed */
    b->observations++;
    float sim = nimcp_emergent_last_best_similarity(b->el);
    int bucket = sim < -1.5f ? 0 : 1 + (sim <= 0.0f ? 0 : sim >= 1.0f ? 9 : (int)(sim * 10.0f));
    b->sim_hist[bucket]++;
    if (nimcp_emergent_get_vocab_size(b->el) > vocab_before) b->new_tokens++;
    bool novel = !_seen_before(b, input_hash);

    if (label && label[0]) {
        /* The label's content words (or the whole label when label_words is
         * off). A prediction hits when the token's gloss before this vote is
         * one of them; the baseline guesses the most-counted word. */
        char words[MAX_LABEL_WORDS][LABEL_LEN];
        uint32_t nw;
        if (b->label_words) {
            nw = _label_words(label, words, MAX_LABEL_WORDS);
        } else {
            snprintf(words[0], LABEL_LEN, "%s", label);
            nw = 1;
        }
        if (nw == 0) goto scored;
        b->labeled_decides++;
        bool maj = false;
        for (uint32_t i = 0; i < nw && b->n_labels > 0; i++)
            if (strcmp(b->labels[b->majority].label, words[i]) == 0) { maj = true; break; }
        if (maj) b->majority_hits++;
        nimcp_emergent_translation_t tr;
        bool had_gloss = (nimcp_emergent_get_translation(b->el, tok, &tr) == 0 &&
                          tr.human_gloss[0] != '\0');
        bool hit = false;
        for (uint32_t i = 0; had_gloss && i < nw; i++)
            if (strncmp(tr.human_gloss, words[i], LABEL_LEN - 1) == 0) { hit = true; break; }
        for (uint32_t i = 0; i < nw; i++) nimcp_emergent_vote_human(b->el, tok, words[i]);
        if (had_gloss) {
            b->predictions++;
            if (hit) b->hits++;
        }
        if (novel && b->seen) {
            b->novel_labeled++;
            if (maj) b->novel_majority_hits++;
            if (had_gloss) {
                b->novel_predictions++;
                if (hit) b->novel_hits++;
            }
        }
        for (uint32_t i = 0; i < nw; i++) _count_label(b, words[i]);
    }
scored:
    if (b->observations % SIM_LOG_EVERY == 0) _log_progress(b);
    if (b->path[0] && (b->observations % SAVE_EVERY == 0 ||
        (b->save_secs > 0 && _now_ms() - b->last_save_ms >= (uint64_t)b->save_secs * 1000u)))
        _save(b);
}

void emergent_bridge_on_decide(emergent_bridge_t* b, const float* features,
                               uint32_t num_features, const float* output,
                               uint32_t output_size) {
    if (!b || !features || !output || output_size == 0) return;
    uint64_t h = _hash(features, num_features);

    nimcp_mutex_lock(b->lock);
    if (!_ensure_model_locked(b, output_size)) { nimcp_mutex_unlock(b->lock); return; }
    char label[LABEL_LEN] = {0};
    for (uint32_t i = 0; i < RING; i++) {
        if (b->recent[i].label[0] && b->recent[i].hash == h) {
            snprintf(label, LABEL_LEN, "%s", b->recent[i].label);
            break;
        }
    }
    uint64_t before = b->observations;
    _observe_locked(b, output, output_size, label, h);
    if (b->observations > before) b->decide_observations++;
    nimcp_mutex_unlock(b->lock);
}

void emergent_bridge_on_learn_output(emergent_bridge_t* b, const float* features,
                                     uint32_t num_features, const float* output,
                                     uint32_t output_size, const char* label) {
    if (!b || !features || !output || output_size == 0 || !label || !label[0]) return;
    uint64_t h = _hash(features, num_features);
    nimcp_mutex_lock(b->lock);
    if (_ensure_model_locked(b, output_size)) {
        uint64_t before = b->observations;
        _observe_locked(b, output, output_size, label, h);
        if (b->observations > before) b->learn_observations++;
    }
    nimcp_mutex_unlock(b->lock);
}

bool emergent_bridge_observe_learn_enabled(void) {
    static int on = -1;                      /* env read once; benign race */
    if (on < 0) {
        const char* v = getenv("NIMCP_EMERGENT_OBSERVE_LEARN");
        on = (v && v[0] == '0') ? 0 : 1;
    }
    return on == 1;
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
    out->decide_observations = b->decide_observations;
    out->learn_observations = b->learn_observations;
    out->novel_labeled = b->novel_labeled;
    out->novel_predictions = b->novel_predictions;
    out->novel_hits = b->novel_hits;
    out->novel_majority_hits = b->novel_majority_hits;
    out->new_tokens = b->new_tokens;
    for (int i = 0; i < SIM_BUCKETS; i++) out->nearest_sim_hist[i] = b->sim_hist[i];
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
