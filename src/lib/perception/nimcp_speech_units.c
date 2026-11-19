/**
 * @file nimcp_speech_units.c
 * @brief Learned sound units. See the header for the design.
 *
 * Cost for a 1 s clip at 16 kHz: ~98 frames x (512-point FFT + 24-band mel
 * + K=44 distance terms of 24 dims) -- well under a millisecond of work.
 */
#include "perception/nimcp_speech_units.h"
#include "utils/spectral/nimcp_fft.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define SU_BANDS        24
#define SU_FFT          512
#define SU_FMIN         80.0f
#define SU_FMAX_CAP     7600.0f
#define SU_SILENCE_NAT  5.0f      /* natural-log energy below peak = ~22 dB */
#define SU_ABS_FLOOR    -13.8f    /* ln(1e-6): frames quieter than this are silence
                                     even if the whole clip is that quiet */
#define SU_SEED_DIST    24.0f     /* min squared distance to seed (~1 nat = 4 dB RMS per band) */
#define SU_LR_FLOOR     0.002f
#define SU_MAX_FRAMES   1024      /* ~10 s at 10 ms hop */

struct speech_units {
    uint32_t sample_rate, win, hop, k;
    float*   mel;                 /* [SU_BANDS][SU_FFT/2+1] triangular filters */
    float*   centroids;           /* [k][SU_BANDS] */
    uint64_t* counts;             /* frames assigned per unit */
    uint32_t seeded;
    uint64_t frames_learned;
    fft_plan_t* plan;
    float*   frame;               /* [SU_FFT] */
    fft_complex_t* spec;          /* [SU_FFT/2+1] */
    float*   power;               /* [SU_FFT/2+1] */
    float*   feats;               /* [SU_MAX_FRAMES][SU_BANDS] */
    float*   loge;                /* [SU_MAX_FRAMES] */
    int32_t* assign;              /* [SU_MAX_FRAMES] */
    float*   dist;                /* [SU_MAX_FRAMES] */
    nimcp_mutex_t* lock;
};

static float hz_to_mel(float hz) { return 2595.0f * log10f(1.0f + hz / 700.0f); }
static float mel_to_hz(float m)  { return 700.0f * (powf(10.0f, m / 2595.0f) - 1.0f); }

static void build_mel(speech_units_t* su) {
    const uint32_t nb = SU_FFT / 2 + 1;
    float fmax = fminf(SU_FMAX_CAP, 0.5f * (float)su->sample_rate - 1.0f);
    float mlo = hz_to_mel(SU_FMIN), mhi = hz_to_mel(fmax);
    float edges[SU_BANDS + 2];
    for (int i = 0; i < SU_BANDS + 2; i++)
        edges[i] = mel_to_hz(mlo + (mhi - mlo) * (float)i / (float)(SU_BANDS + 1));
    float bin_hz = (float)su->sample_rate / (float)SU_FFT;
    for (int b = 0; b < SU_BANDS; b++) {
        float lo = edges[b], c = edges[b + 1], hi = edges[b + 2];
        for (uint32_t k = 0; k < nb; k++) {
            float f = (float)k * bin_hz, w = 0.0f;
            if (f > lo && f <= c) w = (f - lo) / (c - lo);
            else if (f > c && f < hi) w = (hi - f) / (hi - c);
            su->mel[b * nb + k] = w;
        }
    }
}

speech_units_t* speech_units_create(uint32_t sample_rate, uint32_t num_units) {
    if (sample_rate < 8000 || sample_rate > 48000 || num_units < 2 || num_units > 256)
        return NULL;
    speech_units_t* su = nimcp_calloc(1, sizeof(*su));
    if (!su) return NULL;
    su->sample_rate = sample_rate;
    su->win = sample_rate / 40;           /* 25 ms */
    su->hop = sample_rate / 100;          /* 10 ms */
    if (su->win > SU_FFT) su->win = SU_FFT;
    su->k = num_units;
    const uint32_t nb = SU_FFT / 2 + 1;
    su->mel = nimcp_calloc((size_t)SU_BANDS * nb, sizeof(float));
    su->centroids = nimcp_calloc((size_t)num_units * SU_BANDS, sizeof(float));
    su->counts = nimcp_calloc(num_units, sizeof(uint64_t));
    su->frame = nimcp_calloc(SU_FFT, sizeof(float));
    su->spec = nimcp_calloc(nb, sizeof(fft_complex_t));
    su->power = nimcp_calloc(nb, sizeof(float));
    su->feats = nimcp_calloc((size_t)SU_MAX_FRAMES * SU_BANDS, sizeof(float));
    su->loge = nimcp_calloc(SU_MAX_FRAMES, sizeof(float));
    su->assign = nimcp_calloc(SU_MAX_FRAMES, sizeof(int32_t));
    su->dist = nimcp_calloc(SU_MAX_FRAMES, sizeof(float));
    su->plan = fft_plan_create(SU_FFT, FFT_REAL);
    su->lock = nimcp_mutex_create(NULL);
    if (!su->mel || !su->centroids || !su->counts || !su->frame || !su->spec ||
        !su->power || !su->feats || !su->loge || !su->assign || !su->dist ||
        !su->plan || !su->lock) {
        speech_units_destroy(su);
        return NULL;
    }
    fft_plan_set_window(su->plan, FFT_WINDOW_HANN);
    build_mel(su);
    return su;
}

void speech_units_destroy(speech_units_t* su) {
    if (!su) return;
    if (su->plan) fft_plan_destroy(su->plan);
    nimcp_free(su->mel); nimcp_free(su->centroids); nimcp_free(su->counts);
    nimcp_free(su->frame); nimcp_free(su->spec); nimcp_free(su->power);
    nimcp_free(su->feats); nimcp_free(su->loge); nimcp_free(su->assign);
    nimcp_free(su->dist);
    if (su->lock) nimcp_mutex_free(su->lock);
    nimcp_free(su);
}

/* Log-mel features for every frame; returns frame count. Lock held. */
static uint32_t featurize(speech_units_t* su, const float* audio, uint32_t n) {
    const uint32_t nb = SU_FFT / 2 + 1;
    if (n < su->win) return 0;
    uint32_t nf = 1 + (n - su->win) / su->hop;
    if (nf > SU_MAX_FRAMES) nf = SU_MAX_FRAMES;
    for (uint32_t f = 0; f < nf; f++) {
        memset(su->frame, 0, SU_FFT * sizeof(float));
        memcpy(su->frame, audio + (size_t)f * su->hop, su->win * sizeof(float));
        if (!fft_execute_real(su->plan, su->frame, su->spec)) return 0;
        fft_power_spectrum(su->spec, su->power, nb);
        float etot = 1e-10f;
        float* out = su->feats + (size_t)f * SU_BANDS;
        for (int b = 0; b < SU_BANDS; b++) {
            const float* w = su->mel + (size_t)b * nb;
            float e = 0.0f;
            for (uint32_t k = 0; k < nb; k++) e += w[k] * su->power[k];
            out[b] = logf(e + 1e-10f);
            etot += e;
        }
        su->loge[f] = logf(etot);
    }
    /* Per-utterance mean normalisation over voiced frames (removes the
     * channel/loudness offset so categories reflect spectral shape). */
    float peak = -1e30f;
    for (uint32_t f = 0; f < nf; f++) if (su->loge[f] > peak) peak = su->loge[f];
    float floor_e = peak - SU_SILENCE_NAT;
    if (floor_e < SU_ABS_FLOOR) floor_e = SU_ABS_FLOOR;
    float mean[SU_BANDS] = {0};
    uint32_t voiced = 0;
    for (uint32_t f = 0; f < nf; f++) {
        if (su->loge[f] < floor_e) continue;
        for (int b = 0; b < SU_BANDS; b++) mean[b] += su->feats[(size_t)f * SU_BANDS + b];
        voiced++;
    }
    if (voiced == 0) return 0;
    for (int b = 0; b < SU_BANDS; b++) mean[b] /= (float)voiced;
    for (uint32_t f = 0; f < nf; f++) {
        float* v = su->feats + (size_t)f * SU_BANDS;
        for (int b = 0; b < SU_BANDS; b++) v[b] -= mean[b];
        if (su->loge[f] < floor_e) su->loge[f] = -1e30f;  /* mark silence */
    }
    return nf;
}

static int nearest(const speech_units_t* su, const float* v, float* out_d2) {
    int best = -1; float bd = 1e30f;
    for (uint32_t u = 0; u < su->seeded; u++) {
        const float* c = su->centroids + (size_t)u * SU_BANDS;
        float d = 0.0f;
        for (int b = 0; b < SU_BANDS; b++) { float t = v[b] - c[b]; d += t * t; }
        if (d < bd) { bd = d; best = (int)u; }
    }
    if (out_d2) *out_d2 = bd;
    return best;
}

int speech_units_segment(speech_units_t* su, const float* audio, uint32_t num_samples,
                         bool learn, speech_unit_seg_t* out, uint32_t max_out) {
    if (!su || !audio || !out || max_out == 0) return -1;
    nimcp_mutex_lock(su->lock);
    uint32_t nf = featurize(su, audio, num_samples);

    for (uint32_t f = 0; f < nf; f++) {
        su->assign[f] = -1;
        if (su->loge[f] < -1e29f) continue;                  /* silence */
        const float* v = su->feats + (size_t)f * SU_BANDS;
        float d2 = 0.0f;
        int u = nearest(su, v, &d2);
        if (learn && su->seeded < su->k && (u < 0 || d2 > SU_SEED_DIST)) {
            u = (int)su->seeded++;
            memcpy(su->centroids + (size_t)u * SU_BANDS, v, SU_BANDS * sizeof(float));
            d2 = 0.0f;
        }
        if (u < 0) continue;
        su->assign[f] = u;
        su->dist[f] = sqrtf(d2 / (float)SU_BANDS);
        if (learn) {
            su->counts[u]++;
            su->frames_learned++;
            float lr = 1.0f / (float)(su->counts[u] + 1);
            if (lr < SU_LR_FLOOR) lr = SU_LR_FLOOR;
            float* c = su->centroids + (size_t)u * SU_BANDS;
            for (int b = 0; b < SU_BANDS; b++) c[b] += lr * (v[b] - c[b]);
        }
    }

    /* Collapse runs of the same unit (silence breaks a run). */
    uint32_t nseg = 0;
    speech_unit_seg_t tmp[256];
    uint32_t f = 0;
    while (f < nf && nseg < 256) {
        if (su->assign[f] < 0) { f++; continue; }
        int u = su->assign[f];
        uint32_t start = f; float dsum = 0.0f;
        while (f < nf && su->assign[f] == u) { dsum += su->dist[f]; f++; }
        uint32_t len = f - start;
        tmp[nseg].unit = (uint16_t)u;
        tmp[nseg].frames = (uint16_t)(len > 65535 ? 65535 : len);
        tmp[nseg].onset_ms = start * 10u;
        tmp[nseg].confidence = expf(-dsum / (float)len);
        nseg++;
    }
    /* Drop single-frame runs that sit between two other runs (frame noise),
     * then merge neighbours that became equal. */
    uint32_t w = 0;
    for (uint32_t i = 0; i < nseg; i++) {
        bool blip = tmp[i].frames == 1 && i > 0 && i + 1 < nseg;
        if (blip) continue;
        if (w > 0 && out[w - 1].unit == tmp[i].unit) {
            out[w - 1].frames = (uint16_t)(out[w - 1].frames + tmp[i].frames);
            continue;
        }
        if (w == max_out) break;
        out[w++] = tmp[i];
    }
    nimcp_mutex_unlock(su->lock);
    return (int)w;
}

uint32_t speech_units_num_units(const speech_units_t* su) { return su ? su->k : 0; }
uint32_t speech_units_num_seeded(const speech_units_t* su) { return su ? su->seeded : 0; }
uint64_t speech_units_frames_learned(const speech_units_t* su) { return su ? su->frames_learned : 0; }

int speech_units_save(speech_units_t* su, const char* path) {
    if (!su || !path) return -1;
    FILE* fp = fopen(path, "w");
    if (!fp) return -1;
    nimcp_mutex_lock(su->lock);
    fprintf(fp, "SPEECHUNITS 1 %u %u %d %u %llu\n", su->sample_rate, su->k, SU_BANDS,
            su->seeded, (unsigned long long)su->frames_learned);
    for (uint32_t u = 0; u < su->seeded; u++) {
        fprintf(fp, "%llu", (unsigned long long)su->counts[u]);
        for (int b = 0; b < SU_BANDS; b++)
            fprintf(fp, " %.6f", su->centroids[(size_t)u * SU_BANDS + b]);
        fprintf(fp, "\n");
    }
    nimcp_mutex_unlock(su->lock);
    return fclose(fp) == 0 ? 0 : -1;
}

int speech_units_load(speech_units_t* su, const char* path) {
    if (!su || !path) return -1;
    FILE* fp = fopen(path, "r");
    if (!fp) return -1;
    unsigned ver, sr, k, seeded; int bands; unsigned long long frames;
    int rc = -1;
    if (fscanf(fp, "SPEECHUNITS %u %u %u %d %u %llu", &ver, &sr, &k, &bands, &seeded,
               &frames) == 6 && ver == 1 && sr == su->sample_rate && k == su->k &&
        bands == SU_BANDS && seeded <= su->k) {
        nimcp_mutex_lock(su->lock);
        rc = 0;
        for (uint32_t u = 0; u < seeded && rc == 0; u++) {
            unsigned long long cnt;
            if (fscanf(fp, "%llu", &cnt) != 1) { rc = -1; break; }
            su->counts[u] = cnt;
            for (int b = 0; b < SU_BANDS; b++)
                if (fscanf(fp, "%f", &su->centroids[(size_t)u * SU_BANDS + b]) != 1) { rc = -1; break; }
        }
        if (rc == 0) { su->seeded = seeded; su->frames_learned = frames; }
        else { su->seeded = 0; memset(su->counts, 0, su->k * sizeof(uint64_t)); }
        nimcp_mutex_unlock(su->lock);
    }
    fclose(fp);
    return rc;
}
