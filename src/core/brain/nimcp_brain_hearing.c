/**
 * @file nimcp_brain_hearing.c
 * @brief The brain's hearing: cochlea -> auditory nerve -> Wernicke predictor.
 *        See include/core/brain/nimcp_brain_hearing.h.
 */
#include "core/brain/nimcp_brain_hearing.h"

/* brain_internal first so its typedefs win; nimcp_cochlea.h declares
 * medulla_t / thalamus_t tags that collide (same trick as
 * nimcp_brain_init_cochlea.c). */
#include "core/brain/nimcp_brain_internal.h"
#define medulla_t cochlea_internal_medulla_t
#define thalamus_t cochlea_internal_thalamus_t
#include "perception/nimcp_cochlea.h"
#undef medulla_t
#undef thalamus_t
#include "perception/nimcp_auditory_nerve.h"
#include "utils/thread/nimcp_thread.h"
#include "core/brain/nimcp_brain_sensory_hub.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEARING_BLOCK_MS 40u
#define HEARING_MAX_BLOCK 1024u   /* capacity of hearing's private cochlea output */

typedef struct {
    nimcp_mutex_t* mu;
    auditory_predictor_t* pred;
    uint32_t block;               /* samples per 40 ms frame */
    uint32_t channels;
    float carry[HEARING_MAX_BLOCK];
    uint32_t n_carry;
    uint16_t* counts;
    /* Private cochlea output. feed runs lock-free (outside the daemon's brain
     * lock), so writing the shared brain->cochlea_output_buffer here raced its
     * lock-free readers (decide, learn_vector, the COCHLEA_BRIDGES driver). */
    cochlea_output_t* out;
} brain_hearing_t;

int brain_hearing_create(struct brain_struct* brain) {
    if (!brain || !brain->cochlea) return -1;
    if (brain->hearing) return 0;
    /* The brain's cochlea runs at BRAIN_HEARING_SAMPLE_RATE (created in
     * nimcp_brain_init_cochlea.c; the cochlea has no sample-rate getter). */
    uint32_t channels = cochlea_get_num_channels((const cochlea_t*)brain->cochlea);
    uint32_t block = BRAIN_HEARING_SAMPLE_RATE * HEARING_BLOCK_MS / 1000u;
    if (block == 0 || block > HEARING_MAX_BLOCK || channels == 0) return -1;

    brain_hearing_t* h = (brain_hearing_t*)calloc(1, sizeof(*h));
    if (!h) return -1;
    h->block = block;
    h->channels = channels;
    h->counts = (uint16_t*)calloc(channels, sizeof(uint16_t));
    h->mu = nimcp_mutex_create(NULL);
    h->pred = auditory_predictor_create(channels, NULL);
    h->out = cochlea_output_create((cochlea_t*)brain->cochlea, HEARING_MAX_BLOCK);
    if (!h->counts || !h->mu || !h->pred || !h->out) {
        auditory_predictor_destroy(h->pred);
        if (h->out) cochlea_output_destroy(h->out);
        if (h->mu) nimcp_mutex_free(h->mu);
        free(h->counts);
        free(h);
        return -1;
    }
    brain->hearing = h;
    return 0;
}

void brain_hearing_destroy(struct brain_struct* brain) {
    if (!brain || !brain->hearing) return;
    brain_hearing_t* h = (brain_hearing_t*)brain->hearing;
    brain->hearing = NULL;
    auditory_predictor_destroy(h->pred);
    cochlea_output_destroy(h->out);
    nimcp_mutex_free(h->mu);
    free(h->counts);
    free(h);
}

/* One 40 ms block: cochlea -> per-channel spike counts -> predictor. */
static int hear_block(struct brain_struct* brain, brain_hearing_t* h, const float* block) {
    cochlea_output_t* out = h->out;   /* private: see brain_hearing_t.out */
    if (cochlea_process((cochlea_t*)brain->cochlea, block, h->block, out) != NIMCP_SUCCESS) return -1;
    if (!out->anf_output || !out->anf_output->spike_trains) return -1;
    for (uint32_t ch = 0; ch < h->channels; ch++) {
        const uint8_t* tr = out->anf_output->spike_trains[ch];
        uint32_t s = 0;
        if (tr)
            for (uint32_t k = 0; k < h->block; k++) s += tr[k];
        h->counts[ch] = (uint16_t)(s > 65535u ? 65535u : s);
    }
    if (auditory_predictor_step_counts(h->pred, h->counts) != 0) return -1;
    if (brain->sensory_hub) {               /* the rest of the brain hears it on its next tick */
        float mse = 0.0f, copy = 0.0f;
        auditory_predictor_last_mse(h->pred, &mse, &copy);
        brain_sensory_hub_post_audio(brain, h->counts, h->channels,
                                     auditory_predictor_last_error(h->pred), mse, copy,
                                     auditory_predictor_hidden(h->pred),
                                     auditory_predictor_n_rec(h->pred));
    }
    return 0;
}

int brain_hearing_feed(struct brain_struct* brain, const float* audio, uint32_t num_samples) {
    if (!brain || !audio || !brain->hearing || !brain->cochlea) return -1;
    brain_hearing_t* h = (brain_hearing_t*)brain->hearing;
    int frames = 0;
    nimcp_mutex_lock(h->mu);
    uint32_t off = 0;
    if (h->n_carry) {                         /* complete the carried block first */
        uint32_t need = h->block - h->n_carry;
        uint32_t take = num_samples < need ? num_samples : need;
        memcpy(h->carry + h->n_carry, audio, take * sizeof(float));
        h->n_carry += take;
        off = take;
        if (h->n_carry == h->block) {
            if (hear_block(brain, h, h->carry) != 0) { frames = -1; goto done; }
            frames++;
            h->n_carry = 0;
        }
    }
    for (; off + h->block <= num_samples; off += h->block) {
        if (hear_block(brain, h, audio + off) != 0) { frames = -1; goto done; }
        frames++;
    }
    if (off < num_samples) {
        memcpy(h->carry + h->n_carry, audio + off, (num_samples - off) * sizeof(float));
        h->n_carry += num_samples - off;
    }
done:
    nimcp_mutex_unlock(h->mu);
    return frames;
}

bool brain_hearing_get_stats(struct brain_struct* brain, auditory_predictor_stats_t* out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!brain || !brain->hearing) return false;
    brain_hearing_t* h = (brain_hearing_t*)brain->hearing;
    nimcp_mutex_lock(h->mu);
    auditory_predictor_get_stats(h->pred, out);
    nimcp_mutex_unlock(h->mu);
    return true;
}

int brain_hearing_reset(struct brain_struct* brain) {
    if (!brain || !brain->hearing) return -1;
    brain_hearing_t* h = (brain_hearing_t*)brain->hearing;
    auditory_predictor_t* p = auditory_predictor_create(h->channels, NULL);
    if (!p) return -1;
    nimcp_mutex_lock(h->mu);
    auditory_predictor_t* old = h->pred;
    h->pred = p;
    h->n_carry = 0;
    nimcp_mutex_unlock(h->mu);
    auditory_predictor_destroy(old);
    return 0;
}

static void sidecar_path(char* buf, size_t n, const char* checkpoint_path) {
    snprintf(buf, n, "%s.hear", checkpoint_path);
}

int brain_hearing_save(struct brain_struct* brain, const char* checkpoint_path) {
    if (!brain || !brain->hearing || !checkpoint_path) return -1;
    brain_hearing_t* h = (brain_hearing_t*)brain->hearing;
    char path[4096];
    sidecar_path(path, sizeof(path), checkpoint_path);
    nimcp_mutex_lock(h->mu);
    int rc = auditory_predictor_save(h->pred, path);
    nimcp_mutex_unlock(h->mu);
    return rc;
}

int brain_hearing_load(struct brain_struct* brain, const char* checkpoint_path) {
    if (!brain || !brain->hearing || !checkpoint_path) return -1;
    brain_hearing_t* h = (brain_hearing_t*)brain->hearing;
    char path[4096];
    sidecar_path(path, sizeof(path), checkpoint_path);
    FILE* f = fopen(path, "rb");
    if (!f) return 1;
    fclose(f);
    auditory_predictor_t* p = auditory_predictor_load(path);
    if (!p) return -1;
    if (auditory_predictor_dim(p) != h->channels) {
        auditory_predictor_destroy(p);
        return -1;
    }
    nimcp_mutex_lock(h->mu);
    auditory_predictor_t* old = h->pred;
    h->pred = p;
    h->n_carry = 0;
    nimcp_mutex_unlock(h->mu);
    auditory_predictor_destroy(old);
    return 0;
}
