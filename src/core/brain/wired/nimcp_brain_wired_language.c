/**
 * @file nimcp_brain_wired_language.c
 * @brief Wired-module group BRAIN_WIRED_LANGUAGE — language <-> cognitive bridges.
 *
 * See include/core/brain/nimcp_brain_wired.h (lifecycle contract) and
 * include/language/bridges/nimcp_language_cognitive_wired.h (bridge vtables).
 *
 * Lifecycle:
 *   create  — one bridge per module whose subsystem exists on this brain
 *             (NULL subsystem -> bridge skipped). Requires brain->grounded_lang.
 *   tick    — every LW_UPDATE_EVERY training ticks: snapshot the language
 *             state (newest + previous discourse turn, copied under the gl
 *             mutate_lock by grounded_language_copy_recent_turn_vector;
 *             comprehension confidence; caregiver external reward) and run
 *             every bridge enabled in gl->bridge_enable_flags. Each bridge
 *             reads its real module and returns a bounded signal; results are
 *             committed under the group lock.
 *   cascade — brain_wired_language_floor_adjust() / _apply_cascade() expose
 *             the aggregate to the production cascade (RPC threads).
 *   save/load — sidecar <checkpoint>.wired_language with the bridges' small
 *             learned blobs (BG value, Hamiltonian energy mean, JEPA / world
 *             model surprise baselines).
 *
 * Lock discipline: the group lock is a leaf lock. Nothing that can take a
 * foreign lock is called while it is held, except prime_resonance update /
 * recall which only call the pure prime_sig_* functions and nimcp_malloc.
 * The gl copy happens BEFORE the group lock is taken, so a caller holding the
 * gl mutate_lock that calls into the cascade entry points cannot deadlock
 * against tick.
 */
#include "core/brain/nimcp_brain_wired.h"
#include "core/brain/nimcp_brain_internal.h"
#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_internal.h"
#include "language/nimcp_communication_cascade.h"
#include "language/bridges/nimcp_language_cognitive_wired.h"
#include "utils/memory/nimcp_memory.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LW_UPDATE_EVERY   32u        /* ~0.5 s of training at 16.6 ms/tick */
#define LW_SIDECAR_MAGIC  0x4C57474Cu /* "LWGL" */
#define LW_SIDECAR_VER    1u
#define LW_FLOOR_MIN     (-0.30f)
#define LW_FLOOR_MAX      (0.30f)
#define LW_GAIN_MIN       (0.80f)
#define LW_GAIN_MAX       (1.20f)

static const lang_cog_bridge_vt_t* const k_bridges[] = {
    &lang_acc_bridge_vt,
    &lang_amygdala_bridge_vt,
    &lang_basal_ganglia_bridge_vt,
    &lang_consciousness_bridge_vt,
    &lang_episodic_replay_bridge_vt,
    &lang_hamiltonian_bridge_vt,
    &lang_hypothalamus_bridge_vt,
    &lang_imagination_bridge_vt,
    &lang_introspection_bridge_vt,
    &lang_jepa_bridge_vt,
    &lang_mirror_bridge_vt,
    &lang_prime_resonance_bridge_vt,
    &lang_tom_bridge_vt,
    &lang_world_model_bridge_vt,
};
#define LW_NBRIDGES ((uint32_t)(sizeof(k_bridges) / sizeof(k_bridges[0])))

typedef struct {
    void*             st[LW_NBRIDGES];     /* NULL = subsystem absent */
    lang_cog_signal_t sig[LW_NBRIDGES];    /* last committed signal */
    uint64_t          updates[LW_NBRIDGES];
    uint64_t          failures[LW_NBRIDGES];

    pthread_mutex_t   lock;                /* leaf lock: sig[], aggregates, recall */
    uint32_t          dim;                 /* gl semantic_dim */
    float*            cur;                 /* newest turn copy */
    float*            prev;                /* previous turn copy */
    float*            last_cur;            /* cur seen at the last update */
    float*            recall_buf;          /* scratch for apply_cascade (under lock) */

    uint64_t          ticks;
    uint64_t          update_index;
    uint64_t          new_turns;
    uint64_t          cascade_applies;
    float             floor_adjust;        /* aggregate, clamped */
    float             intent_gain;         /* aggregate, clamped */
    float             last_surprise;
    uint32_t          n_live;
} lang_wired_t;

void* lang_cog_brain_imagination(struct brain_struct* brain) {
    return brain ? (void*)brain->imagination : NULL;
}

static lang_wired_t* lw_get(struct brain_struct* brain) {
    return brain ? (lang_wired_t*)brain->wired[BRAIN_WIRED_LANGUAGE] : NULL;
}

static void lw_free(lang_wired_t* lw) {
    if (!lw) { return; }
    for (uint32_t i = 0; i < LW_NBRIDGES; i++) {
        if (lw->st[i]) { k_bridges[i]->destroy(lw->st[i]); lw->st[i] = NULL; }
    }
    nimcp_free(lw->cur);
    nimcp_free(lw->prev);
    nimcp_free(lw->last_cur);
    nimcp_free(lw->recall_buf);
    pthread_mutex_destroy(&lw->lock);
    nimcp_free(lw);
}

int brain_wired_language_create(struct brain_struct* brain) {
    if (!brain || !brain->grounded_lang) { return -1; }
    if (brain->wired[BRAIN_WIRED_LANGUAGE]) { return 0; }
    uint32_t dim = grounded_language_get_semantic_dim(brain->grounded_lang);
    if (dim == 0 || dim > 65536u) { return -1; }

    lang_wired_t* lw = (lang_wired_t*)nimcp_calloc(1, sizeof(*lw));
    if (!lw) { return -1; }
    if (pthread_mutex_init(&lw->lock, NULL) != 0) { nimcp_free(lw); return -1; }
    lw->dim        = dim;
    lw->cur        = (float*)nimcp_calloc(dim, sizeof(float));
    lw->prev       = (float*)nimcp_calloc(dim, sizeof(float));
    lw->last_cur   = (float*)nimcp_calloc(dim, sizeof(float));
    lw->recall_buf = (float*)nimcp_calloc(dim, sizeof(float));
    if (!lw->cur || !lw->prev || !lw->last_cur || !lw->recall_buf) { lw_free(lw); return -1; }
    lw->intent_gain = 1.0f;

    for (uint32_t i = 0; i < LW_NBRIDGES; i++) {
        lang_cog_signal_neutral(&lw->sig[i]);
        lw->st[i] = k_bridges[i]->create(brain);
        if (lw->st[i]) { lw->n_live++; }
    }
    brain->wired[BRAIN_WIRED_LANGUAGE] = lw;
    fprintf(stderr, "[WIRED] LANGUAGE: %u/%u cognitive bridges live (dim=%u):",
            lw->n_live, LW_NBRIDGES, dim);
    for (uint32_t i = 0; i < LW_NBRIDGES; i++) {
        if (lw->st[i]) { fprintf(stderr, " %s", k_bridges[i]->name); }
    }
    fprintf(stderr, "\n");
    return 0;
}

static float lw_cosine(const float* a, const float* b, uint32_t n) {
    double dot = 0.0, na = 0.0, nb = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        dot += (double)a[i] * b[i];
        na  += (double)a[i] * a[i];
        nb  += (double)b[i] * b[i];
    }
    if (na < 1e-12 || nb < 1e-12) { return 0.0f; }
    double c = dot / (sqrt(na) * sqrt(nb));
    return isfinite(c) ? (float)c : 0.0f;
}

static bool lw_all_finite(const float* v, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        if (!isfinite(v[i])) { return false; }
    }
    return true;
}

void brain_wired_language_tick(struct brain_struct* brain, const float* f, uint32_t n, float dt_ms) {
    (void)f; (void)n; (void)dt_ms;
    lang_wired_t* lw = lw_get(brain);
    if (!lw || !brain->grounded_lang) { return; }
    if ((lw->ticks++ % LW_UPDATE_EVERY) != 0u) { return; }

    grounded_language_t* gl = brain->grounded_lang;

    /* --- language snapshot (gl mutate_lock taken inside the copy; we hold
     *     no lock here) --- */
    uint32_t got_cur  = grounded_language_copy_recent_turn_vector(gl, 1, lw->cur, lw->dim);
    uint32_t got_prev = grounded_language_copy_recent_turn_vector(gl, 2, lw->prev, lw->dim);
    bool have_cur  = (got_cur == lw->dim)  && lw_all_finite(lw->cur, lw->dim);
    bool have_prev = (got_prev == lw->dim) && lw_all_finite(lw->prev, lw->dim);

    lang_cog_input_t in;
    memset(&in, 0, sizeof(in));
    in.dim       = lw->dim;
    in.lang_vec  = have_cur ? lw->cur : NULL;
    in.lang_prev = have_prev ? lw->prev : NULL;
    if (have_cur && memcmp(lw->cur, lw->last_cur, (size_t)lw->dim * sizeof(float)) != 0) {
        in.new_turn = true;
        memcpy(lw->last_cur, lw->cur, (size_t)lw->dim * sizeof(float));
        lw->new_turns++;
    }
    if (have_cur && have_prev) {
        float c = lw_cosine(lw->cur, lw->prev, lw->dim);
        in.semantic_surprise = lang_cog_clampf(0.5f * (1.0f - c), 0.0f, 1.0f);
    }
    /* Racy single-float reads of gl / brain counters are benign. */
    in.comprehension_conf = lang_cog_clampf(
        lang_cog_finite_or(gl->stats.avg_comprehension_confidence, 0.0f), 0.0f, 1.0f);
    in.external_reward    = lang_cog_finite_or(brain->last_external_reward, 0.0f);
    in.external_reward_us = brain->last_external_reward_us;
    in.update_index       = ++lw->update_index;

    /* --- run bridges (module getters outside the group lock) --- */
    lang_cog_signal_t sig[LW_NBRIDGES];
    bool ran[LW_NBRIDGES];
    for (uint32_t i = 0; i < LW_NBRIDGES; i++) {
        lang_cog_signal_neutral(&sig[i]);
        ran[i] = false;
        if (!lw->st[i]) { continue; }
        if (!grounded_language_is_bridge_enabled(gl, k_bridges[i]->gl_bit)) { continue; }
        int rc;
        if (k_bridges[i]->recall) {
            /* Bridges with recall state mutate it on update: serialize with
             * apply_cascade. Their update only calls lock-free helpers. */
            pthread_mutex_lock(&lw->lock);
            rc = k_bridges[i]->update(lw->st[i], brain, &in, &sig[i]);
            pthread_mutex_unlock(&lw->lock);
        } else {
            rc = k_bridges[i]->update(lw->st[i], brain, &in, &sig[i]);
        }
        if (rc != 0) { lw->failures[i]++; lang_cog_signal_neutral(&sig[i]); continue; }
        ran[i] = true;
    }

    /* --- aggregate + commit --- */
    float floor_sum = 0.0f, gain_prod = 1.0f;
    for (uint32_t i = 0; i < LW_NBRIDGES; i++) {
        if (!ran[i] || !sig[i].valid) { continue; }
        float fa = lang_cog_finite_or(sig[i].floor_adjust, 0.0f);
        float g  = lang_cog_finite_or(sig[i].intent_gain, 1.0f);
        sig[i].floor_adjust = lang_cog_clampf(fa, -0.15f, 0.15f);
        sig[i].intent_gain  = lang_cog_clampf(g, 0.7f, 1.3f);
        sig[i].primary      = lang_cog_finite_or(sig[i].primary, 0.0f);
        sig[i].secondary    = lang_cog_finite_or(sig[i].secondary, 0.0f);
        floor_sum += sig[i].floor_adjust;
        gain_prod *= sig[i].intent_gain;
    }
    pthread_mutex_lock(&lw->lock);
    for (uint32_t i = 0; i < LW_NBRIDGES; i++) {
        if (!lw->st[i]) { continue; }
        if (ran[i]) { lw->updates[i]++; lw->sig[i] = sig[i]; }
        else        { lang_cog_signal_neutral(&lw->sig[i]); }
    }
    lw->floor_adjust  = lang_cog_clampf(floor_sum, LW_FLOOR_MIN, LW_FLOOR_MAX);
    lw->intent_gain   = lang_cog_clampf(gain_prod, LW_GAIN_MIN, LW_GAIN_MAX);
    lw->last_surprise = in.semantic_surprise;
    pthread_mutex_unlock(&lw->lock);
}

float brain_wired_language_floor_adjust(struct brain_struct* brain) {
    lang_wired_t* lw = lw_get(brain);
    if (!lw) { return 0.0f; }
    pthread_mutex_lock(&lw->lock);
    float v = lw->floor_adjust;
    pthread_mutex_unlock(&lw->lock);
    return lang_cog_clampf(lang_cog_finite_or(v, 0.0f), LW_FLOOR_MIN, LW_FLOOR_MAX);
}

int brain_wired_language_apply_cascade(struct brain_struct* brain, void* production_cascade_state) {
    production_cascade_state_t* cs = (production_cascade_state_t*)production_cascade_state;
    if (!cs) { return -1; }
    lang_wired_t* lw = lw_get(brain);
    if (!lw || !cs->content_intent || cs->content_dim == 0) { return 0; }

    float* intent = cs->content_intent;
    const uint32_t d = cs->content_dim;
    if (!lw_all_finite(intent, d)) { return 0; }   /* never amplify a bad vector */

    pthread_mutex_lock(&lw->lock);
    float gain = lw->intent_gain;
    float w = 0.0f;
    if (d == lw->dim) {
        for (uint32_t i = 0; i < LW_NBRIDGES; i++) {
            if (!lw->st[i] || !k_bridges[i]->recall || !lw->sig[i].valid) { continue; }
            float wi = k_bridges[i]->recall(lw->st[i], lw->recall_buf, lw->dim);
            if (wi > 0.0f && isfinite(wi)) { w = lang_cog_clampf(wi, 0.0f, 0.2f); break; }
        }
    }
    int modified = 0;
    double norm = 0.0;
    for (uint32_t k = 0; k < d; k++) { norm += (double)intent[k] * intent[k]; }
    norm = sqrt(norm);
    if (w > 0.0f && norm > 1e-6 && lw_all_finite(lw->recall_buf, d)) {
        double rn = 0.0;
        for (uint32_t k = 0; k < d; k++) { rn += (double)lw->recall_buf[k] * lw->recall_buf[k]; }
        rn = sqrt(rn);
        if (rn > 1e-6) {
            /* Scale-matched blend: recall contributes direction, not magnitude. */
            float s = (float)(norm / rn);
            for (uint32_t k = 0; k < d; k++) {
                float v = (1.0f - w) * intent[k] + w * s * lw->recall_buf[k];
                if (isfinite(v)) { intent[k] = v; }
            }
            modified = 1;
        }
    }
    lw->cascade_applies++;
    pthread_mutex_unlock(&lw->lock);

    gain = lang_cog_clampf(lang_cog_finite_or(gain, 1.0f), LW_GAIN_MIN, LW_GAIN_MAX);
    if (fabsf(gain - 1.0f) > 1e-4f) {
        for (uint32_t k = 0; k < d; k++) {
            float v = intent[k] * gain;
            if (isfinite(v)) { intent[k] = v; }
        }
        modified = 1;
    }
    return modified;
}

static void lw_sidecar_path(char* out, size_t cap, const char* path) {
    snprintf(out, cap, "%s.wired_language", path);
}

int brain_wired_language_save(struct brain_struct* brain, const char* path) {
    lang_wired_t* lw = lw_get(brain);
    if (!lw || !path) { return 0; }
    char sp[4096], tmp[4200];
    lw_sidecar_path(sp, sizeof(sp), path);
    snprintf(tmp, sizeof(tmp), "%s.tmp", sp);
    FILE* fp = fopen(tmp, "wb");
    if (!fp) { return -1; }
    uint32_t hdr[3] = { LW_SIDECAR_MAGIC, LW_SIDECAR_VER, 0 };
    for (uint32_t i = 0; i < LW_NBRIDGES; i++) {
        if (lw->st[i] && k_bridges[i]->save) { hdr[2]++; }
    }
    int rc = (fwrite(hdr, sizeof(hdr), 1, fp) == 1) ? 0 : -1;
    /* Blob state is a few floats updated by tick; a save racing a tick can
     * at worst persist one-update-stale values (each float is read whole). */
    for (uint32_t i = 0; i < LW_NBRIDGES && rc == 0; i++) {
        if (!lw->st[i] || !k_bridges[i]->save) { continue; }
        char name[16];
        memset(name, 0, sizeof(name));
        strncpy(name, k_bridges[i]->name, sizeof(name) - 1);
        if (fwrite(name, sizeof(name), 1, fp) != 1) { rc = -1; break; }
        rc = k_bridges[i]->save(lw->st[i], fp);
    }
    if (fclose(fp) != 0) { rc = -1; }
    if (rc != 0 || rename(tmp, sp) != 0) { remove(tmp); return -1; }
    return 0;
}

int brain_wired_language_load(struct brain_struct* brain, const char* path) {
    lang_wired_t* lw = lw_get(brain);
    if (!lw || !path) { return 1; }
    char sp[4096];
    lw_sidecar_path(sp, sizeof(sp), path);
    FILE* fp = fopen(sp, "rb");
    if (!fp) { return 1; }                       /* no sidecar: fresh state */
    uint32_t hdr[3];
    int rc = 0;
    if (fread(hdr, sizeof(hdr), 1, fp) != 1 || hdr[0] != LW_SIDECAR_MAGIC ||
        hdr[1] != LW_SIDECAR_VER || hdr[2] > LW_NBRIDGES) {
        fclose(fp);
        return -1;
    }
    for (uint32_t r = 0; r < hdr[2] && rc == 0; r++) {
        char name[16];
        if (fread(name, sizeof(name), 1, fp) != 1) { rc = -1; break; }
        name[15] = '\0';
        int found = 0;
        for (uint32_t i = 0; i < LW_NBRIDGES; i++) {
            if (strcmp(name, k_bridges[i]->name) != 0 || !k_bridges[i]->load) { continue; }
            found = 1;
            if (lw->st[i]) {
                rc = k_bridges[i]->load(lw->st[i], fp);
            } else {
                /* Bridge absent on this brain: skip its blob. */
                uint32_t cnt = 0;
                if (fread(&cnt, sizeof(cnt), 1, fp) != 1 || cnt > 1024u ||
                    fseek(fp, (long)cnt * (long)sizeof(float), SEEK_CUR) != 0) { rc = -1; }
            }
            break;
        }
        if (!found) { rc = -1; }
    }
    fclose(fp);
    return rc;
}

void brain_wired_language_destroy(struct brain_struct* brain) {
    if (!brain) { return; }
    lang_wired_t* lw = lw_get(brain);
    brain->wired[BRAIN_WIRED_LANGUAGE] = NULL;
    lw_free(lw);
}

int brain_wired_language_status_json(struct brain_struct* brain, char* buf, uint32_t cap) {
    lang_wired_t* lw = lw_get(brain);
    if (!buf || cap == 0) { return 0; }
    if (!lw) { return snprintf(buf, cap, "null"); }
    pthread_mutex_lock(&lw->lock);
    uint32_t off = 0;
    int w = snprintf(buf, cap,
                     "{\"live\":%u,\"updates\":%llu,\"new_turns\":%llu,\"cascade_applies\":%llu,"
                     "\"floor_adjust\":%.4f,\"intent_gain\":%.4f,\"surprise\":%.4f,\"bridges\":{",
                     lw->n_live, (unsigned long long)lw->update_index,
                     (unsigned long long)lw->new_turns, (unsigned long long)lw->cascade_applies,
                     (double)lw->floor_adjust, (double)lw->intent_gain, (double)lw->last_surprise);
    if (w > 0 && (uint32_t)w < cap) { off = (uint32_t)w; }
    int first = 1;
    for (uint32_t i = 0; i < LW_NBRIDGES && off < cap; i++) {
        if (!lw->st[i]) { continue; }
        const lang_cog_signal_t* s = &lw->sig[i];
        w = snprintf(buf + off, cap - off,
                     "%s\"%s\":{\"valid\":%s,\"floor\":%.4f,\"gain\":%.4f,\"p\":%.4f,\"q\":%.4f,"
                     "\"n\":%llu,\"fail\":%llu}",
                     first ? "" : ",", k_bridges[i]->name, s->valid ? "true" : "false",
                     (double)s->floor_adjust, (double)s->intent_gain,
                     (double)s->primary, (double)s->secondary,
                     (unsigned long long)lw->updates[i], (unsigned long long)lw->failures[i]);
        if (w < 0 || (uint32_t)w >= cap - off) { off = cap; break; }
        off += (uint32_t)w;
        first = 0;
    }
    pthread_mutex_unlock(&lw->lock);
    if (off >= cap) { return (int)cap; }         /* truncated: signal like snprintf */
    w = snprintf(buf + off, cap - off, "}}");
    if (w < 0 || (uint32_t)w >= cap - off) { return (int)cap; }
    return (int)(off + (uint32_t)w);
}
