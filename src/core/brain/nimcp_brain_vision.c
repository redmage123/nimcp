/**
 * @file nimcp_brain_vision.c
 * @brief The brain's vision: retina -> optic nerve -> sensory hub.
 *        See include/core/brain/nimcp_brain_vision.h.
 */
#include "core/brain/nimcp_brain_vision.h"

#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_sensory_hub.h"
#include "perception/nimcp_retina_circuit.h"
#include "perception/nimcp_optic_nerve.h"
#include "utils/thread/nimcp_thread.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VISION_DEFAULT_MS 100.0f
#define VISION_RATE_SCALE_HZ 100.0f    /* ganglion-cell rate -> [0,1] drive */

typedef struct {
    nimcp_mutex_t* mu;
    retina_circuit_t* retina;
    float prev_lgn[SENSORY_HUB_VISUAL_MAX];
    bool have_prev;
    brain_vision_stats_t st;
} brain_vision_t;

int brain_vision_create(struct brain_struct* b) {
    if (!b) return -1;
    if (b->vision) return 0;
    brain_vision_t* v = (brain_vision_t*)calloc(1, sizeof(*v));
    if (!v) return -1;
    retina_circuit_config_t cfg = retina_circuit_config_default();
    v->mu = nimcp_mutex_create(NULL);
    v->retina = retina_circuit_create(&cfg);
    if (!v->mu || !v->retina) {
        if (v->retina) retina_circuit_destroy(v->retina);
        if (v->mu) nimcp_mutex_free(v->mu);
        free(v);
        return -1;
    }
    if (b->health_agent) retina_circuit_set_health_agent((struct nimcp_health_agent*)b->health_agent);
    v->st.enabled = true;
    b->vision = v;
    return 0;
}

void brain_vision_destroy(struct brain_struct* b) {
    if (!b || !b->vision) return;
    brain_vision_t* v = (brain_vision_t*)b->vision;
    b->vision = NULL;
    retina_circuit_destroy(v->retina);
    nimcp_mutex_free(v->mu);
    free(v);
}

int brain_vision_see(struct brain_struct* b, const uint8_t* pixels, uint32_t w, uint32_t h,
                     uint32_t ch, float duration_ms) {
    if (!b || !b->vision || !pixels || !w || !h || !ch) return -1;
    brain_vision_t* v = (brain_vision_t*)b->vision;
    if (duration_ms <= 0.0f) duration_ms = VISION_DEFAULT_MS;
    brain_sensory_visual_frame_t f;
    memset(&f, 0, sizeof(f));
    retina_output_t out;
    memset(&out, 0, sizeof(out));

    nimcp_mutex_lock(v->mu);
    int rc = retina_circuit_see(v->retina, pixels, w, h, ch, duration_ms, &out);
    if (rc != 0) {
        nimcp_mutex_unlock(v->mu);
        return -1;
    }
    /* What reaches the LGN: per ganglion-cell axon, its arrival rate. */
    const float secs = out.duration_ms / 1000.0f;
    uint32_t n = out.n_rgc < SENSORY_HUB_VISUAL_MAX ? out.n_rgc : SENSORY_HUB_VISUAL_MAX;
    double rgc_sum = 0.0, lgn_sum = 0.0, change = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        float hz = secs > 0 ? out.lgn_spikes[i] / secs : 0.0f;
        float x = fminf(hz / VISION_RATE_SCALE_HZ, 1.0f);
        f.lgn[i] = x;
        if (v->have_prev) change += fabsf(x - v->prev_lgn[i]);
        v->prev_lgn[i] = x;
        lgn_sum += hz;
        rgc_sum += secs > 0 ? out.rgc_spikes[i] / secs : 0.0f;
    }
    f.n = n;
    f.change = (v->have_prev && n) ? (float)(change / n) : 0.0f;
    v->have_prev = true;
    f.luminance = out.cone_activation > out.rod_activation ? out.cone_activation : out.rod_activation;
    f.map_w = out.map_w;
    f.map_h = out.map_h;
    uint32_t cells = out.map_w * out.map_h;
    if (cells > 256) cells = 256;
    double contrast = 0.0;
    for (uint32_t i = 0; i < cells; i++) {
        f.on_map[i] = fminf(out.on_map[i] / VISION_RATE_SCALE_HZ, 1.0f);
        f.off_map[i] = fminf(out.off_map[i] / VISION_RATE_SCALE_HZ, 1.0f);
        contrast += fabsf(f.on_map[i] - f.off_map[i]);
    }
    f.on_off_contrast = cells ? (float)(contrast / cells) : 0.0f;

    v->st.frames++;
    v->st.seconds_seen += secs;
    v->st.rgc_rate_hz = n ? (float)(rgc_sum / n) : 0.0f;
    v->st.lgn_rate_hz = n ? (float)(lgn_sum / n) : 0.0f;
    v->st.latency_ms = out.lgn_mean_latency_ms;
    v->st.rod_activation = out.rod_activation;
    v->st.cone_activation = out.cone_activation;
    v->st.wall_ms = (float)out.wall_ms;
    optic_nerve_t* on = retina_circuit_get_optic_nerve(v->retina);
    optic_nerve_stats_t ons;
    if (on && optic_nerve_get_stats(on, &ons) == 0) v->st.mean_myelination = ons.mean_myelination;
    nimcp_mutex_unlock(v->mu);

    brain_sensory_hub_post_visual(b, &f);
    {   /* echolocation / eagle vision take the retinotopic ON/OFF maps */
        extern void brain_nonhuman_post_visual(struct brain_struct*, const float*, const float*,
                                               uint32_t, uint32_t);
        brain_nonhuman_post_visual(b, f.on_map, f.off_map, f.map_w, f.map_h);
    }   /* the brain sees it on its next tick */
    return 0;
}

bool brain_vision_get_stats(struct brain_struct* b, brain_vision_stats_t* out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!b || !b->vision) return false;
    brain_vision_t* v = (brain_vision_t*)b->vision;
    nimcp_mutex_lock(v->mu);
    *out = v->st;
    nimcp_mutex_unlock(v->mu);
    return true;
}

int brain_vision_save(struct brain_struct* b, const char* checkpoint_path) {
    if (!b || !b->vision || !checkpoint_path) return -1;
    brain_vision_t* v = (brain_vision_t*)b->vision;
    char path[4096];
    snprintf(path, sizeof(path), "%s.retina", checkpoint_path);
    nimcp_mutex_lock(v->mu);
    int rc = retina_circuit_save(v->retina, path);
    nimcp_mutex_unlock(v->mu);
    return rc;
}

int brain_vision_load(struct brain_struct* b, const char* checkpoint_path) {
    if (!b || !b->vision || !checkpoint_path) return -1;
    brain_vision_t* v = (brain_vision_t*)b->vision;
    char path[4096];
    snprintf(path, sizeof(path), "%s.retina", checkpoint_path);
    FILE* f = fopen(path, "rb");
    if (!f) return 1;
    fclose(f);
    nimcp_mutex_lock(v->mu);
    int rc = retina_circuit_load(v->retina, path);
    v->have_prev = false;
    nimcp_mutex_unlock(v->mu);
    return rc;
}
