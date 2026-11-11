/**
 * @file nimcp_brain_fusion_reliability.c
 * @brief Reliability-weighted ANN/SNN output fusion (see header).
 */
#include "core/brain/nimcp_brain_fusion_reliability.h"

#include "core/brain/nimcp_brain_internal.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "snn/nimcp_snn_network.h"
#include "utils/memory/nimcp_memory.h"

#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* EMA rate; the first 1/FUSION_REL_ALPHA samples use a running mean instead
 * so the estimate is not biased toward its zero start. */
#define FUSION_REL_ALPHA 0.02f

static pthread_once_t s_once = PTHREAD_ONCE_INIT;
static bool s_enabled = false;

static void fusion_rel_read_env(void)
{
    const char* e = getenv("NIMCP_FUSION_RELIABILITY");
    s_enabled = (e && strcmp(e, "1") == 0);
}

bool brain_fusion_rel_enabled(void)
{
    pthread_once(&s_once, fusion_rel_read_env);
    return s_enabled;
}

static void fusion_rel_update(float* ema, uint32_t* samples, float value)
{
    if (!isfinite(value)) {
        return;
    }
    (*samples)++;
    float a = 1.0f / (float)(*samples);
    if (a < FUSION_REL_ALPHA) {
        a = FUSION_REL_ALPHA;
    }
    *ema += a * (value - *ema);
}

void brain_fusion_rel_observe_ann(struct brain_struct* brain)
{
    if (!brain_fusion_rel_enabled() || !brain || !brain->network) {
        return;
    }
    float c = 0.0f;
    if (adaptive_network_get_last_learn_cos(brain->network, &c)) {
        fusion_rel_update(&brain->fusion_rel_ann, &brain->fusion_rel_ann_samples, c);
    }
}

/* cos(v - mean(v), t) over the first n entries */
static float centred_cos(const float* v, const float* t, uint32_t n)
{
    double mean = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        mean += isfinite(v[i]) ? v[i] : 0.0f;
    }
    mean /= (double)n;
    double dot = 0.0, nv = 0.0, nt = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        double c = (isfinite(v[i]) ? v[i] : 0.0f) - mean;
        dot += c * t[i];
        nv += c * c;
        nt += (double)t[i] * t[i];
    }
    if (nv < 1e-16 || nt < 1e-16) {
        return 0.0f;   /* silent or uniform output carries no target signal */
    }
    return (float)(dot / sqrt(nv * nt));
}

void brain_fusion_rel_observe_snn(struct brain_struct* brain, const float* target,
                                  uint32_t target_size)
{
    if (!brain_fusion_rel_enabled() || !brain || !brain->snn_network || !target) {
        return;
    }
    snn_network_t* snn = brain->snn_network;
    if (!snn->output_pop || snn->output_pop->n_neurons == 0) {
        return;
    }
    uint32_t n_out = snn->output_pop->n_neurons;
    float* rates = nimcp_calloc(n_out, sizeof(float));
    if (!rates) {
        return;
    }
    /* Rates over the decoder window: after the training step this reflects
     * the SNN's recent response, which is what the decide path reads too. */
    if (snn_network_get_outputs(snn, rates, n_out) == SNN_SUCCESS) {
        uint32_t n = (n_out < target_size) ? n_out : target_size;
        if (n > 0) {
            fusion_rel_update(&brain->fusion_rel_snn, &brain->fusion_rel_snn_samples,
                              centred_cos(rates, target, n));
        }
    }
    nimcp_free(rates);
}

bool brain_fusion_rel_blend(struct brain_struct* brain, float* out, const float* snn,
                            uint32_t n, float* w_out)
{
    if (!brain_fusion_rel_enabled() || !brain || !out || !snn || n == 0 ||
        brain->fusion_rel_ann_samples == 0) {
        return false;
    }
    float w_a = fmaxf(brain->fusion_rel_ann, 0.0f);
    float w_s = (brain->fusion_rel_snn_samples > 0) ? fmaxf(brain->fusion_rel_snn, 0.0f) : 0.0f;
    if (w_a + w_s < 1e-3f) {
        /* Neither pathway predicts its targets yet: keep the trained ANN */
        w_a = 1.0f;
        w_s = 0.0f;
    }
    float ws = w_a + w_s;
    w_a /= ws;
    w_s /= ws;
    if (w_out) {
        w_out[0] = w_a;
        w_out[1] = w_s;
    }

    /* Centre the SNN rates and scale them to the ANN's norm */
    double mean = 0.0, n_ann = 0.0, n_snn = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        mean += isfinite(snn[i]) ? snn[i] : 0.0f;
        n_ann += (double)out[i] * out[i];
    }
    mean /= (double)n;
    for (uint32_t i = 0; i < n; i++) {
        double c = (isfinite(snn[i]) ? snn[i] : 0.0f) - mean;
        n_snn += c * c;
    }
    float scale = (n_snn > 1e-16) ? (float)sqrt(n_ann / n_snn) : 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float c = (float)((isfinite(snn[i]) ? snn[i] : 0.0f) - mean);
        out[i] = w_a * out[i] + w_s * scale * c;
    }
    return true;
}
