/**
 * @file nimcp_lnn_bptt.c
 * @brief Exact truncated BPTT through recorded LNN training steps (2026-11-07).
 *
 * See include/lnn/nimcp_lnn_bptt.h for the forward map being differentiated
 * and the recording rules. Everything here runs on the CPU: the production
 * LNN is small (4 layers, ≤128 neurons), so a 100-step backward is a few ms.
 */

#include "lnn/nimcp_lnn_bptt.h"
#include "lnn/nimcp_lnn_types.h"
#include "lnn/nimcp_lnn_config.h"
#include "lnn/nimcp_lnn_training.h"          /* lnn_tune_get_gpu_forward */
#include "core/substrate/nimcp_substrate_effects.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/logging/nimcp_logging.h"

#include <math.h>
#include <string.h>
#include <time.h>

/* Per-step ceiling on each carried dL/dx (matches the legacy adjoint's
 * per-step guard) and per-element clamp on accumulated parameter gradients
 * (matches accumulate_parameter_gradients). */
#define LNN_BPTT_CARRY_CEILING 100.0f
#define LNN_BPTT_GRAD_CLAMP    10.0f
#define LNN_BPTT_TAU_FLOOR     0.01f
#define LNN_BPTT_STATE_CLAMP   10.0f

typedef struct {
    bool   valid;
    bool   ltc;          /* every layer took the CPU LTC path */
    bool   expo;         /* exponential-Euler step (lnn_tune cpu_integrator) */
    float  dt;
    float  gate;         /* thalamic input gain applied to the network input */
    float* buf;          /* per layer: u[m_i] x0[n_i] x1[n_i] */
    float* mod;          /* per layer substrate tau multiplier */
} bptt_entry_t;

typedef struct {
    uint32_t      capacity;
    uint32_t      len;
    uint32_t      head;        /* index of the next write */
    uint32_t      n_layers;
    uint32_t*     m;           /* layer input widths */
    uint32_t*     n;           /* layer widths */
    size_t*       off;         /* per-layer offset of u inside an entry buf */
    size_t        floats;      /* floats per entry buf */
    bptt_entry_t* entries;
    bptt_entry_t  pending;     /* record being filled by the current forward */
    float*        input_grad;  /* [n_inputs] from the last backward */
    uint32_t      input_grad_n;
} bptt_trace_t;

static double bptt_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}

static void bptt_trace_destroy(bptt_trace_t* t) {
    if (!t) return;
    if (t->entries) {
        for (uint32_t k = 0; k < t->capacity; k++) {
            nimcp_free(t->entries[k].buf);
            nimcp_free(t->entries[k].mod);
        }
        nimcp_free(t->entries);
    }
    nimcp_free(t->pending.buf);
    nimcp_free(t->pending.mod);
    nimcp_free(t->m);
    nimcp_free(t->n);
    nimcp_free(t->off);
    nimcp_free(t->input_grad);
    nimcp_free(t);
}

static bool bptt_entry_alloc(bptt_entry_t* e, size_t floats, uint32_t n_layers) {
    e->buf = (float*)nimcp_calloc(floats, sizeof(float));
    e->mod = (float*)nimcp_calloc(n_layers, sizeof(float));
    return e->buf && e->mod;
}

static bptt_trace_t* bptt_trace_create(const lnn_network_t* net) {
    if (!net || net->n_layers == 0 || !net->layers) return NULL;
    uint32_t cap = (net->config && net->config->bptt_truncation > 0)
                 ? net->config->bptt_truncation : 100;
    bptt_trace_t* t = (bptt_trace_t*)nimcp_calloc(1, sizeof(bptt_trace_t));
    if (!t) return NULL;
    t->capacity = cap;
    t->n_layers = net->n_layers;
    t->m   = (uint32_t*)nimcp_calloc(net->n_layers, sizeof(uint32_t));
    t->n   = (uint32_t*)nimcp_calloc(net->n_layers, sizeof(uint32_t));
    t->off = (size_t*)nimcp_calloc(net->n_layers, sizeof(size_t));
    if (!t->m || !t->n || !t->off) { bptt_trace_destroy(t); return NULL; }
    size_t floats = 0;
    for (uint32_t i = 0; i < net->n_layers; i++) {
        const lnn_layer_t* L = net->layers[i];
        if (!L || !L->W_in) { bptt_trace_destroy(t); return NULL; }
        t->n[i] = L->n_neurons;
        t->m[i] = nimcp_tensor_shape(L->W_in)->dims[1];
        t->off[i] = floats;
        floats += (size_t)t->m[i] + 2u * (size_t)t->n[i];
    }
    t->floats = floats;
    t->entries = (bptt_entry_t*)nimcp_calloc(cap, sizeof(bptt_entry_t));
    if (!t->entries) { bptt_trace_destroy(t); return NULL; }
    for (uint32_t k = 0; k < cap; k++) {
        if (!bptt_entry_alloc(&t->entries[k], floats, net->n_layers)) {
            bptt_trace_destroy(t);
            return NULL;
        }
    }
    if (!bptt_entry_alloc(&t->pending, floats, net->n_layers)) {
        bptt_trace_destroy(t);
        return NULL;
    }
    t->input_grad_n = t->m[0];
    t->input_grad = (float*)nimcp_calloc(t->input_grad_n, sizeof(float));
    if (!t->input_grad) { bptt_trace_destroy(t); return NULL; }
    return t;
}

/* Entry k-th oldest (0 = oldest, len-1 = newest). */
static bptt_entry_t* bptt_at(bptt_trace_t* t, uint32_t k) {
    uint32_t idx = (t->head + t->capacity - t->len + k) % t->capacity;
    return &t->entries[idx];
}

/*============================================================================
 * Public: recording control
 *==========================================================================*/

void lnn_network_set_bptt_record(lnn_network_t* network, bool on) {
    if (!network) return;
    network->bptt_record = on;
}

uint32_t lnn_bptt_trace_len(const lnn_network_t* network) {
    if (!network || !network->bptt_trace) return 0;
    return ((const bptt_trace_t*)network->bptt_trace)->len;
}

void lnn_bptt_clear(lnn_network_t* network) {
    if (!network || !network->bptt_trace) return;
    bptt_trace_t* t = (bptt_trace_t*)network->bptt_trace;
    for (uint32_t k = 0; k < t->capacity; k++) t->entries[k].valid = false;
    t->len = 0;
    t->head = 0;
}

void lnn_bptt_free(lnn_network_t* network) {
    if (!network) return;
    bptt_trace_destroy((bptt_trace_t*)network->bptt_trace);
    network->bptt_trace = NULL;
}

const float* lnn_bptt_last_input_grad(const lnn_network_t* network, uint32_t* n) {
    if (n) *n = 0;
    if (!network || !network->bptt_trace) return NULL;
    const bptt_trace_t* t = (const bptt_trace_t*)network->bptt_trace;
    if (n) *n = t->input_grad_n;
    return t->input_grad;
}

/*============================================================================
 * Recording hooks (called from lnn_network_forward_step, mutex held)
 *==========================================================================*/

void* lnn_bptt_record_begin(lnn_network_t* network, float dt, float input_gate) {
    if (!network || !network->bptt_record) return NULL;
    if (!network->bptt_trace) {
        network->bptt_trace = bptt_trace_create(network);
        if (!network->bptt_trace) {
            NIMCP_LOGGING_WARN("LNN BPTT: trace allocation failed — recording off");
            network->bptt_record = false;
            return NULL;
        }
    }
    bptt_trace_t* t = (bptt_trace_t*)network->bptt_trace;
    if (t->n_layers != network->n_layers) return NULL;   /* topology changed */
    t->pending.valid = false;
    t->pending.ltc = true;
    t->pending.expo = lnn_tune_get_cpu_integrator() != 0.0f;
    t->pending.dt = dt;
    t->pending.gate = input_gate;
    return &t->pending;
}

void lnn_bptt_record_layer_pre(lnn_network_t* network, void* rec, uint32_t i,
                               const nimcp_tensor_t* layer_input) {
    if (!network || !rec || !network->bptt_trace) return;
    bptt_trace_t* t = (bptt_trace_t*)network->bptt_trace;
    bptt_entry_t* e = (bptt_entry_t*)rec;
    if (i >= t->n_layers) return;
    lnn_layer_t* L = network->layers[i];
    float* u  = e->buf + t->off[i];
    float* x0 = u + t->m[i];
    const uint32_t m = t->m[i], n = t->n[i];

    size_t un = layer_input ? nimcp_tensor_numel(layer_input) : 0;
    const float* ud = layer_input ? (const float*)nimcp_tensor_data_const(layer_input) : NULL;
    size_t cu = (un < m) ? un : m;
    if (ud && cu) memcpy(u, ud, cu * sizeof(float));
    if (cu < m) memset(u + cu, 0, (m - cu) * sizeof(float));

    if (L && L->x && nimcp_tensor_numel(L->x) >= n) {
        memcpy(x0, nimcp_tensor_data_const(L->x), n * sizeof(float));
    } else {
        memset(x0, 0, n * sizeof(float));
        e->ltc = false;
    }
    float mod = 1.0f;
    if (L && L->substrate_dend_effects && L->tau) {
        mod = L->substrate_dend_effects->membrane_time_constant_mod;
    }
    e->mod[i] = mod;

    /* Paths this backward does not model: Hamiltonian layer, GPU forward. */
    if (L && L->use_hamiltonian && L->H_net && L->p) e->ltc = false;
    if (L && L->gpu_lnn_layer && L->gpu_ctx && lnn_tune_get_gpu_forward() != 0.0f) e->ltc = false;
    if (!L || !L->W_in || !L->W_rec || !L->W_tau || !L->b_in || !L->b_tau || !L->tau_base) {
        e->ltc = false;
    }
}

void lnn_bptt_record_layer_post(lnn_network_t* network, void* rec, uint32_t i) {
    if (!network || !rec || !network->bptt_trace) return;
    bptt_trace_t* t = (bptt_trace_t*)network->bptt_trace;
    bptt_entry_t* e = (bptt_entry_t*)rec;
    if (i >= t->n_layers) return;
    lnn_layer_t* L = network->layers[i];
    float* x1 = e->buf + t->off[i] + t->m[i] + t->n[i];
    const uint32_t n = t->n[i];
    if (L && L->x && nimcp_tensor_numel(L->x) >= n) {
        memcpy(x1, nimcp_tensor_data_const(L->x), n * sizeof(float));
    } else {
        memset(x1, 0, n * sizeof(float));
        e->ltc = false;
    }
}

void lnn_bptt_record_commit(lnn_network_t* network, void* rec, bool ok) {
    if (!network || !rec || !network->bptt_trace || !ok) return;
    bptt_trace_t* t = (bptt_trace_t*)network->bptt_trace;
    bptt_entry_t* e = (bptt_entry_t*)rec;
    /* Swap the pending buffers into the ring slot (no copy). */
    bptt_entry_t* slot = &t->entries[t->head];
    float* b = slot->buf; float* md = slot->mod;
    slot->buf = e->buf;   slot->mod = e->mod;
    slot->dt = e->dt;     slot->gate = e->gate;
    slot->ltc = e->ltc;   slot->valid = true;
    slot->expo = e->expo;
    e->buf = b;           e->mod = md;
    e->valid = false;
    t->head = (t->head + 1) % t->capacity;
    if (t->len < t->capacity) t->len++;
}

/*============================================================================
 * Backward
 *==========================================================================*/

static inline float bptt_act(lnn_activation_t a, float h) {
    switch (a) {
        case LNN_ACTIVATION_SIGMOID: return 1.0f / (1.0f + expf(-h));
        case LNN_ACTIVATION_RELU:    return h > 0.0f ? h : 0.0f;
        case LNN_ACTIVATION_TANH:
        default:                     return tanhf(h);
    }
}

static inline float bptt_act_deriv(lnn_activation_t a, float h, float y) {
    switch (a) {
        case LNN_ACTIVATION_SIGMOID: return y * (1.0f - y);
        case LNN_ACTIVATION_RELU:    return h > 0.0f ? 1.0f : 0.0f;
        case LNN_ACTIVATION_TANH:
        default:                     return 1.0f - y * y;
    }
}

/* Scratch for one layer's local backward. */
typedef struct {
    float* s;  float* sg; float* tau; uint8_t* fl1; uint8_t* fl2;
    float* h;  float* a;  float* y;   float* c;     uint8_t* cm;
    float* xh; float* gy; float* gs;  float* gh;
} bptt_scratch_t;

static void bptt_scratch_free(bptt_scratch_t* s) {
    nimcp_free(s->s); nimcp_free(s->sg); nimcp_free(s->tau);
    nimcp_free(s->fl1); nimcp_free(s->fl2); nimcp_free(s->h); nimcp_free(s->a);
    nimcp_free(s->y); nimcp_free(s->c); nimcp_free(s->cm); nimcp_free(s->xh);
    nimcp_free(s->gy); nimcp_free(s->gs); nimcp_free(s->gh);
}

static bool bptt_scratch_alloc(bptt_scratch_t* s, uint32_t n) {
    memset(s, 0, sizeof(*s));
    s->s = nimcp_calloc(n, sizeof(float));  s->sg = nimcp_calloc(n, sizeof(float));
    s->tau = nimcp_calloc(n, sizeof(float));
    s->fl1 = nimcp_calloc(n, 1);            s->fl2 = nimcp_calloc(n, 1);
    s->h = nimcp_calloc(n, sizeof(float));  s->a = nimcp_calloc(n, sizeof(float));
    s->y = nimcp_calloc(n, sizeof(float));  s->c = nimcp_calloc(n, sizeof(float));
    s->cm = nimcp_calloc(n, 1);             s->xh = nimcp_calloc(n, sizeof(float));
    s->gy = nimcp_calloc(n, sizeof(float)); s->gs = nimcp_calloc(n, sizeof(float));
    s->gh = nimcp_calloc(n, sizeof(float));
    return s->s && s->sg && s->tau && s->fl1 && s->fl2 && s->h && s->a && s->y
        && s->c && s->cm && s->xh && s->gy && s->gs && s->gh;
}

/**
 * One layer, one step. g = dL/dx1 (in place, consumed). Writes dL/dx0 into
 * dx0 and ADDS dL/du into du (both zeroed by the caller as needed).
 * Accumulates parameter gradients into the layer's grad tensors and their
 * squared norm into *sumsq.
 */
static void bptt_layer_step(lnn_layer_t* L, uint32_t m, uint32_t n,
                            const float* u, const float* x0, float dt, float mod, bool expo,
                            const float* g, float* dx0, float* du,
                            bptt_scratch_t* S, double* sumsq) {
    const float* W_in  = (const float*)nimcp_tensor_data_const(L->W_in);
    const float* W_rec = (const float*)nimcp_tensor_data_const(L->W_rec);
    const float* W_tau = (const float*)nimcp_tensor_data_const(L->W_tau);
    const float* b_in  = (const float*)nimcp_tensor_data_const(L->b_in);
    const float* b_tau = (const float*)nimcp_tensor_data_const(L->b_tau);
    const float* tbase = (const float*)nimcp_tensor_data_const(L->tau_base);
    float* gW_in  = L->grad_W_in     ? (float*)nimcp_tensor_data(L->grad_W_in)     : NULL;
    float* gW_rec = L->grad_W_rec    ? (float*)nimcp_tensor_data(L->grad_W_rec)    : NULL;
    float* gW_tau = L->grad_W_tau    ? (float*)nimcp_tensor_data(L->grad_W_tau)    : NULL;
    float* gb_in  = L->grad_b_in     ? (float*)nimcp_tensor_data(L->grad_b_in)     : NULL;
    float* gb_tau = L->grad_b_tau    ? (float*)nimcp_tensor_data(L->grad_b_tau)    : NULL;
    float* gtb    = L->grad_tau_base ? (float*)nimcp_tensor_data(L->grad_tau_base) : NULL;
    const lnn_activation_t act = L->neurons ? L->neurons[0].activation : LNN_ACTIVATION_TANH;
    const uint32_t tw = m + n;   /* W_tau row width */

    /* ---- Recompute the step's local forward ---- */
    for (uint32_t j = 0; j < n; j++) {
        const float* wt = W_tau + (size_t)j * tw;
        float s = b_tau[j];
        for (uint32_t k = 0; k < m; k++) s += wt[k] * u[k];
        for (uint32_t k = 0; k < n; k++) s += wt[m + k] * x0[k];
        float sg = 1.0f / (1.0f + expf(-s));
        float tr = tbase[j] * sg;
        uint8_t f1 = !(isfinite(tr) && tr >= LNN_BPTT_TAU_FLOOR);
        float tau = f1 ? LNN_BPTT_TAU_FLOOR : tr;
        uint8_t f2 = 0;
        if (mod != 1.0f) {
            float v = tau * mod;
            f2 = !(isfinite(v) && v >= LNN_BPTT_TAU_FLOOR);
            tau = f2 ? LNN_BPTT_TAU_FLOOR : v;
        }
        S->s[j] = s; S->sg[j] = sg; S->tau[j] = tau; S->fl1[j] = f1; S->fl2[j] = f2;

        const float* wi = W_in + (size_t)j * m;
        const float* wr = W_rec + (size_t)j * n;
        float h = b_in[j];
        for (uint32_t k = 0; k < m; k++) h += wi[k] * u[k];
        for (uint32_t k = 0; k < n; k++) h += wr[k] * x0[k];
        float a = bptt_act(act, h);
        float y = expo ? x0[j] * expf(-dt / tau) + tau * (1.0f - expf(-dt / tau)) * a
                       : x0[j] + dt * (-x0[j] / tau + a);
        float c;
        uint8_t cm;
        if (!isfinite(y))                    { c = 0.0f;                   cm = 0; }
        else if (y >  LNN_BPTT_STATE_CLAMP)  { c =  LNN_BPTT_STATE_CLAMP;  cm = 0; }
        else if (y < -LNN_BPTT_STATE_CLAMP)  { c = -LNN_BPTT_STATE_CLAMP;  cm = 0; }
        else                                 { c = y;                      cm = 1; }
        S->h[j] = h; S->a[j] = a; S->y[j] = y; S->c[j] = c; S->cm[j] = cm;
    }

    /* ---- dL/dc (through layer norm when the forward used it) ---- */
    if (L->use_layer_norm && n > 1) {
        double mean = 0.0;
        for (uint32_t j = 0; j < n; j++) mean += S->c[j];
        mean /= (double)n;
        double var = 0.0;
        for (uint32_t j = 0; j < n; j++) { double d = S->c[j] - mean; var += d * d; }
        var /= (double)n;
        float r = 1.0f / sqrtf((float)var + L->layer_norm_eps);
        double mg = 0.0, mgx = 0.0;
        for (uint32_t j = 0; j < n; j++) {
            S->xh[j] = (float)((S->c[j] - mean) * r);
            mg  += g[j];
            mgx += (double)g[j] * S->xh[j];
        }
        mg /= (double)n; mgx /= (double)n;
        for (uint32_t j = 0; j < n; j++) {
            S->gy[j] = r * (g[j] - (float)mg - S->xh[j] * (float)mgx);
        }
    } else {
        for (uint32_t j = 0; j < n; j++) S->gy[j] = g[j];
    }

    /* ---- Local backward ---- */
    for (uint32_t j = 0; j < n; j++) {
        float gy = S->cm[j] ? S->gy[j] : 0.0f;
        float tau = S->tau[j];
        float gtau, ga;
        if (expo) {
            /* y = x0·E + τ(1-E)·a, E = e^(-dt/τ), dE/dτ = E·dt/τ² */
            const float E = expf(-dt / tau);
            dx0[j] = gy * E;
            gtau = gy * (E * dt / (tau * tau) * x0[j] + S->a[j] * (1.0f - E)
                         - S->a[j] * E * dt / tau);
            ga = gy * tau * (1.0f - E);
        } else {
            /* y = x0 + dt·(-x0/τ + a) */
            dx0[j] = gy * (1.0f - dt / tau);
            gtau = gy * dt * x0[j] / (tau * tau);
            ga = gy * dt;
        }
        float gtr = S->fl2[j] ? 0.0f : ((mod != 1.0f) ? gtau * mod : gtau);
        if (S->fl1[j]) gtr = 0.0f;
        float sg = S->sg[j];
        float gtbase = gtr * sg;
        S->gs[j] = gtr * tbase[j] * sg * (1.0f - sg);
        /* activation path */
        S->gh[j] = ga * bptt_act_deriv(act, S->h[j], S->a[j]);
        if (gtb)    { gtb[j]    += gtbase;   *sumsq += (double)gtbase * gtbase; }
        if (gb_tau) { gb_tau[j] += S->gs[j]; *sumsq += (double)S->gs[j] * S->gs[j]; }
        if (gb_in)  { gb_in[j]  += S->gh[j]; *sumsq += (double)S->gh[j] * S->gh[j]; }
    }
    for (uint32_t j = 0; j < n; j++) {
        const float gs = S->gs[j], gh = S->gh[j];
        const float* wt = W_tau + (size_t)j * tw;
        const float* wi = W_in + (size_t)j * m;
        const float* wr = W_rec + (size_t)j * n;
        if (gs != 0.0f) {
            float* gwt = gW_tau ? gW_tau + (size_t)j * tw : NULL;
            for (uint32_t k = 0; k < m; k++) {
                if (gwt) { float d = gs * u[k]; gwt[k] += d; *sumsq += (double)d * d; }
                du[k] += wt[k] * gs;
            }
            for (uint32_t k = 0; k < n; k++) {
                if (gwt) { float d = gs * x0[k]; gwt[m + k] += d; *sumsq += (double)d * d; }
                dx0[k] += wt[m + k] * gs;
            }
        }
        if (gh != 0.0f) {
            float* gwi = gW_in ? gW_in + (size_t)j * m : NULL;
            float* gwr = gW_rec ? gW_rec + (size_t)j * n : NULL;
            for (uint32_t k = 0; k < m; k++) {
                if (gwi) { float d = gh * u[k]; gwi[k] += d; *sumsq += (double)d * d; }
                du[k] += wi[k] * gh;
            }
            for (uint32_t k = 0; k < n; k++) {
                if (gwr) { float d = gh * x0[k]; gwr[k] += d; *sumsq += (double)d * d; }
                dx0[k] += wr[k] * gh;
            }
        }
    }
}

static void bptt_guard(float* v, uint32_t n) {
    double sq = 0.0;
    bool bad = false;
    for (uint32_t k = 0; k < n; k++) {
        if (!isfinite(v[k])) { bad = true; break; }
        sq += (double)v[k] * v[k];
    }
    if (bad) { memset(v, 0, n * sizeof(float)); return; }
    float norm = (float)sqrt(sq);
    if (norm > LNN_BPTT_CARRY_CEILING) {
        float sc = LNN_BPTT_CARRY_CEILING / norm;
        for (uint32_t k = 0; k < n; k++) v[k] *= sc;
    }
}

static float bptt_clamp_and_norm(lnn_network_t* net) {
    double sq = 0.0;
    for (uint32_t i = 0; i < net->n_layers; i++) {
        lnn_layer_t* L = net->layers[i];
        if (!L) continue;
        nimcp_tensor_t* gs[6] = { L->grad_W_in, L->grad_W_rec, L->grad_W_tau,
                                  L->grad_b_in, L->grad_b_tau, L->grad_tau_base };
        for (int g = 0; g < 6; g++) {
            if (!gs[g]) continue;
            float* d = (float*)nimcp_tensor_data(gs[g]);
            size_t ne = nimcp_tensor_numel(gs[g]);
            for (size_t k = 0; k < ne; k++) {
                float v = d[k];
                if (!isfinite(v)) v = 0.0f;
                else if (v >  LNN_BPTT_GRAD_CLAMP) v =  LNN_BPTT_GRAD_CLAMP;
                else if (v < -LNN_BPTT_GRAD_CLAMP) v = -LNN_BPTT_GRAD_CLAMP;
                d[k] = v;
                sq += (double)v * v;
            }
        }
    }
    return (float)sqrt(sq);
}

int lnn_bptt_backward(lnn_network_t* network, const nimcp_tensor_t* loss_grad,
                      lnn_bptt_stats_t* stats) {
    if (stats) memset(stats, 0, sizeof(*stats));
    if (!network || !loss_grad) return LNN_ERROR_NULL_POINTER;
    bptt_trace_t* t = (bptt_trace_t*)network->bptt_trace;
    if (!t || t->len == 0) return LNN_ERROR_INVALID_STATE;
    const double t0 = bptt_now_ms();
    if (network->mutex) nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)network->mutex);

    const uint32_t L = t->n_layers;
    uint32_t nmax = 0, mmax = 0;
    for (uint32_t i = 0; i < L; i++) {
        if (t->n[i] > nmax) nmax = t->n[i];
        if (t->m[i] > mmax) mmax = t->m[i];
    }
    /* carry[i]: dL/dx1 of layer i at the step being processed (from the
     * step after it); up: dL/du handed down from the layer above. */
    float** carry = (float**)nimcp_calloc(L, sizeof(float*));
    float* g   = (float*)nimcp_calloc(nmax, sizeof(float));
    float* dx0 = (float*)nimcp_calloc(nmax, sizeof(float));
    float* up  = (float*)nimcp_calloc(mmax > nmax ? mmax : nmax, sizeof(float));
    float* du  = (float*)nimcp_calloc(mmax > nmax ? mmax : nmax, sizeof(float));
    bptt_scratch_t S;
    memset(&S, 0, sizeof(S));
    bool ok = carry && g && dx0 && up && du && bptt_scratch_alloc(&S, nmax);
    for (uint32_t i = 0; ok && i < L; i++) {
        carry[i] = (float*)nimcp_calloc(t->n[i], sizeof(float));
        ok = carry[i] != NULL;
    }
    int rc = LNN_SUCCESS;
    double sumsq = 0.0;
    uint32_t used = 0, breaks = 0;
    if (!ok) {
        rc = LNN_ERROR_OUT_OF_MEMORY;
        goto done;
    }
    memset(t->input_grad, 0, t->input_grad_n * sizeof(float));

    const float* lg = (const float*)nimcp_tensor_data_const(loss_grad);
    size_t lgn = nimcp_tensor_numel(loss_grad);
    const uint32_t top = L - 1;
    {
        uint32_t c = (lgn < t->n[top]) ? (uint32_t)lgn : t->n[top];
        memcpy(carry[top], lg, c * sizeof(float));
    }

    const uint32_t horizon = (uint32_t)lnn_tune_get_bptt_steps();
    for (int k = (int)t->len - 1; k >= 0; k--) {
        if (horizon > 0 && used >= horizon) break;
        bptt_entry_t* e = bptt_at(t, (uint32_t)k);
        if (!e->valid || !e->ltc) { breaks++; break; }
        /* Anything left to propagate? */
        bool any = false;
        for (uint32_t i = 0; i < L && !any; i++) {
            for (uint32_t j = 0; j < t->n[i]; j++) if (carry[i][j] != 0.0f) { any = true; break; }
        }
        if (!any) break;
        used++;

        memset(up, 0, (mmax > nmax ? mmax : nmax) * sizeof(float));
        for (int i = (int)top; i >= 0; i--) {
            lnn_layer_t* Ly = network->layers[i];
            const uint32_t m = t->m[i], n = t->n[i];
            const float* u  = e->buf + t->off[i];
            const float* x0 = u + m;
            for (uint32_t j = 0; j < n; j++) g[j] = carry[i][j] + (i == (int)top ? 0.0f : up[j]);
            memset(du, 0, m * sizeof(float));
            bptt_layer_step(Ly, m, n, u, x0, e->dt, e->mod[i], e->expo, g, dx0, du, &S, &sumsq);
            /* dL/dx0 → carry to the previous step (continuity checked below) */
            memcpy(carry[i], dx0, n * sizeof(float));
            bptt_guard(carry[i], n);
            /* dL/du → the layer below at this same step */
            if (i > 0) {
                memcpy(up, du, m * sizeof(float));
                bptt_guard(up, m);
            } else if (k == (int)t->len - 1) {
                uint32_t c = (m < t->input_grad_n) ? m : t->input_grad_n;
                for (uint32_t q = 0; q < c; q++) t->input_grad[q] = du[q] * e->gate;
            }
        }

        /* Continuity into the previous step: its x1 must be this x0. */
        if (k > 0 && !(horizon > 0 && used >= horizon)) {
            bptt_entry_t* p = bptt_at(t, (uint32_t)(k - 1));
            bool cont = p->valid && p->ltc;
            for (uint32_t i = 0; cont && i < L; i++) {
                const float* x0 = e->buf + t->off[i] + t->m[i];
                const float* x1p = p->buf + t->off[i] + t->m[i] + t->n[i];
                if (memcmp(x0, x1p, t->n[i] * sizeof(float)) != 0) cont = false;
            }
            if (!cont) { breaks++; break; }
        }
    }

done:
    {
        float final_norm = 0.0f;
        if (rc == LNN_SUCCESS) {
            final_norm = bptt_clamp_and_norm(network);
            network->stats.backward_steps++;
            network->stats.gradient_norm = final_norm;
            network->stats.ode_evaluations += used;
        }
        if (network->mutex) nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)network->mutex);
        const double ms = bptt_now_ms() - t0;
        const float raw = (float)sqrt(sumsq);
        if (stats) {
            stats->steps_recorded = t->len;
            stats->steps_used = used;
            stats->chain_breaks = breaks;
            stats->raw_norm = raw;
            stats->final_norm = final_norm;
            stats->time_ms = ms;
        }
        if (rc == LNN_SUCCESS) {
            NIMCP_LOGGING_INFO("Adjoint computation complete (BPTT): norm=%f raw_norm=%f "
                               "time=%.3f ms steps=%u/%u breaks=%u",
                               final_norm, raw, ms, used, t->len, breaks);
        }
    }
    if (carry) {
        for (uint32_t i = 0; i < L; i++) nimcp_free(carry[i]);
        nimcp_free(carry);
    }
    nimcp_free(g); nimcp_free(dx0); nimcp_free(up); nimcp_free(du);
    bptt_scratch_free(&S);
    return rc;
}
