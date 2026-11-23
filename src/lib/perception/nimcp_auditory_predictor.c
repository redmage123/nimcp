/**
 * @file nimcp_auditory_predictor.c
 * @brief Wernicke predictor: ALIF recurrent spiking network, e-prop learning,
 *        predicting the next 40 ms of auditory-nerve activity.
 *
 * Port of athena_core/athena/bio/wernicke.py (WernickePredictor), core
 * configuration only (no short-term plasticity, delays or Dale's law: those
 * were not yet measured to help). Matrices are row-major with the
 * presynaptic index first: W_in[i*n + j] is input channel i -> neuron j,
 * W_rec[i*n + j] is neuron i -> neuron j, W_out[j*dim + k] is neuron j ->
 * output k, B[k*n + j] is the fixed random feedback from output k to
 * neuron j.
 *
 * Per simulation step (dt = 40/sub ms), with z_prev the previous step's spikes:
 *   b = rho b + z_prev;  A = thr + beta b
 *   v = a v + I_in + z_prev W_rec - z_prev thr;  z = v > A
 *   y = k y + z W_out
 *   epa_ij = psi_prev_j eps_i + (rho_j - psi_prev_j beta_j) epa_ij   (old eps)
 *   eps = a eps + pre;  psi_j = gamma/thr max(0, 1 - |v_j - A_j|/thr)
 *   ebar_ij = k ebar_ij + psi_j (eps_i - beta_j epa_ij);  zbar = k zbar + z
 * Per frame, with err = prediction - (x - x_prev):
 *   L_j = sum_k err_k B[k,j] + reg (rate_j - r_step)
 *   W -= lr L_j ebar_ij;  W_out -= lr_out zbar_j err_k / (1 + |zbar|^2)
 *   W_out *= 1 - out_decay;  h += ip_lr (r_step - rate)
 */
#include "perception/nimcp_auditory_predictor.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct auditory_predictor {
    auditory_predictor_config_t cfg;
    uint32_t dim, n;
    float a, k, r_step, ffi;            /* ffi < 0: off */
    /* parameters */
    float *W_in, *W_rec, *W_out, *B, *h, *rho, *beta;
    /* listening state */
    float *v, *z, *b, *y, *psi, *zbar, *zsum;
    float *eps_in, *eps_rec, *epa_in, *epa_rec, *ebar_in, *ebar_rec;
    float *xbar, *x_prev, *pending;
    bool has_xbar, has_pending;
    /* scratch */
    float *z_prev, *I_in, *xd, *err, *L, *x_tmp;
    /* statistics */
    uint64_t frames, scored;
    double sum_mse, sum_copy;
    float ema_mse, ema_copy;
    float last_mse, last_copy;
};

/* ------------------------------------------------------------------ rng */

static uint64_t splitmix64(uint64_t* s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

static double gaussian(uint64_t* s) {
    double u1 = ((splitmix64(s) >> 11) + 1.0) * (1.0 / 9007199254740993.0);
    double u2 = (splitmix64(s) >> 11) * (1.0 / 9007199254740992.0);
    return sqrt(-2.0 * log(u1)) * cos(6.283185307179586 * u2);
}

/* ------------------------------------------------------------ lifecycle */

auditory_predictor_config_t auditory_predictor_config_default(void) {
    auditory_predictor_config_t c;
    memset(&c, 0, sizeof(c));
    c.n_rec = 512;
    c.sub = 8;
    c.tau_m = 20.0f;
    c.tau_out = 20.0f;
    c.tau_a[0] = 200.0f;
    c.tau_a[1] = 2000.0f;
    c.frac_adaptive = 0.5f;
    c.adapt_rise = 0.5f;
    c.thr = 1.0f;
    c.gamma = 0.3f;
    c.target_rate = 10.0f;
    c.reg = 0.1f;
    c.lr = 1e-3f;
    c.lr_out = 0.005f;
    c.out_decay = 1e-5f;
    c.in_gain = 4.0f;
    c.ip_lr = 0.1f;
    c.ffi_tau = 2000.0f;
    c.input_scale = 320.0f;
    c.stats_window_frames = 9000.0f;
    c.seed = 0;
    return c;
}

static float* fvec(size_t n) { return (float*)calloc(n ? n : 1, sizeof(float)); }

void auditory_predictor_destroy(auditory_predictor_t* p) {
    if (!p) return;
    float** all[] = {&p->W_in, &p->W_rec, &p->W_out, &p->B, &p->h, &p->rho, &p->beta,
                     &p->v, &p->z, &p->b, &p->y, &p->psi, &p->zbar, &p->zsum,
                     &p->eps_in, &p->eps_rec, &p->epa_in, &p->epa_rec, &p->ebar_in,
                     &p->ebar_rec, &p->xbar, &p->x_prev, &p->pending,
                     &p->z_prev, &p->I_in, &p->xd, &p->err, &p->L, &p->x_tmp};
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) free(*all[i]);
    free(p);
}

/* Allocate everything and derive the constants; weights left at zero. */
static auditory_predictor_t* alloc_predictor(uint32_t dim, const auditory_predictor_config_t* cfg) {
    if (!cfg || dim == 0 || cfg->n_rec == 0 || cfg->sub == 0 || cfg->tau_m <= 0 ||
        cfg->tau_out <= 0 || cfg->thr <= 0 || cfg->input_scale <= 0) {
        return NULL;
    }
    auditory_predictor_t* p = (auditory_predictor_t*)calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->cfg = *cfg;
    uint32_t n = p->n = cfg->n_rec;
    p->dim = dim;
    size_t dn = (size_t)dim * n, nn = (size_t)n * n;
    p->W_in = fvec(dn); p->W_rec = fvec(nn); p->W_out = fvec(dn); p->B = fvec(dn);
    p->h = fvec(n); p->rho = fvec(n); p->beta = fvec(n);
    p->v = fvec(n); p->z = fvec(n); p->b = fvec(n); p->psi = fvec(n);
    p->zbar = fvec(n); p->zsum = fvec(n); p->eps_rec = fvec(n);
    p->y = fvec(dim); p->eps_in = fvec(dim);
    p->epa_in = fvec(dn); p->epa_rec = fvec(nn); p->ebar_in = fvec(dn); p->ebar_rec = fvec(nn);
    p->xbar = fvec(dim); p->x_prev = fvec(dim); p->pending = fvec(dim);
    p->z_prev = fvec(n); p->I_in = fvec(n); p->xd = fvec(dim); p->err = fvec(dim);
    p->L = fvec(n); p->x_tmp = fvec(dim);
    if (!p->W_in || !p->W_rec || !p->W_out || !p->B || !p->h || !p->rho || !p->beta ||
        !p->v || !p->z || !p->b || !p->psi || !p->zbar || !p->zsum || !p->eps_rec ||
        !p->y || !p->eps_in || !p->epa_in || !p->epa_rec || !p->ebar_in || !p->ebar_rec ||
        !p->xbar || !p->x_prev || !p->pending || !p->z_prev || !p->I_in || !p->xd ||
        !p->err || !p->L || !p->x_tmp) {
        auditory_predictor_destroy(p);
        return NULL;
    }

    double dt = 40.0 / cfg->sub;
    p->a = (float)exp(-dt / cfg->tau_m);
    p->k = (float)exp(-dt / cfg->tau_out);
    p->r_step = (float)(cfg->target_rate * dt / 1000.0);
    p->ffi = cfg->ffi_tau > 0 ? (float)exp(-40.0 / cfg->ffi_tau) : -1.0f;

    /* adaptation: the first frac_adaptive of the neurons, split evenly over
     * the two time constants; the rest are plain LIF (rho = beta = 0) */
    uint32_t n_ad = (uint32_t)lround(cfg->frac_adaptive * n);
    for (uint32_t g = 0; g < 2; g++) {
        uint32_t lo = g * n_ad / 2, hi = (g + 1) * n_ad / 2;
        if (cfg->tau_a[g] <= 0) continue;
        float rho = (float)exp(-dt / cfg->tau_a[g]);
        float steady_b = p->r_step / fmaxf(1.0f - rho, 1e-6f);
        for (uint32_t j = lo; j < hi; j++) {
            p->rho[j] = rho;
            p->beta[j] = cfg->adapt_rise * cfg->thr / steady_b;
        }
    }
    return p;
}

auditory_predictor_t* auditory_predictor_create(uint32_t dim, const auditory_predictor_config_t* cfg) {
    auditory_predictor_config_t def = auditory_predictor_config_default();
    if (!cfg) cfg = &def;
    auditory_predictor_t* p = alloc_predictor(dim, cfg);
    if (!p) return NULL;
    uint32_t n = p->n;
    uint64_t s = cfg->seed * 0x2545F4914F6CDD1Dull + 1;
    double g_in = cfg->in_gain / sqrt((double)dim), g_rec = 0.5 / sqrt((double)n),
           g_b = 1.0 / sqrt((double)dim);
    for (size_t i = 0; i < (size_t)dim * n; i++) p->W_in[i] = (float)(gaussian(&s) * g_in);
    for (uint32_t i = 0; i < n; i++)
        for (uint32_t j = 0; j < n; j++)
            p->W_rec[(size_t)i * n + j] = i == j ? 0.0f : (float)(gaussian(&s) * g_rec);
    for (size_t i = 0; i < (size_t)dim * n; i++) p->B[i] = (float)(gaussian(&s) * g_b);
    return p;
}

uint32_t auditory_predictor_dim(const auditory_predictor_t* p) { return p ? p->dim : 0; }
uint32_t auditory_predictor_n_rec(const auditory_predictor_t* p) { return p ? p->n : 0; }

const float* auditory_predictor_last_error(const auditory_predictor_t* p) {
    return p && p->scored > 0 ? p->err : NULL;
}

const float* auditory_predictor_hidden(const auditory_predictor_t* p) { return p ? p->zbar : NULL; }

void auditory_predictor_last_mse(const auditory_predictor_t* p, float* mse, float* copy_mse) {
    if (mse) *mse = p ? p->last_mse : 0.0f;
    if (copy_mse) *copy_mse = p ? p->last_copy : 0.0f;
}

const float* auditory_predictor_prediction(const auditory_predictor_t* p) {
    return p && p->has_pending ? p->pending : NULL;
}

/* ------------------------------------------------------------- learning */

/* One frame of drive x: sub simulation steps with eligibility traces. */
static void present(auditory_predictor_t* p, const float* x_in) {
    const uint32_t n = p->n, dim = p->dim;
    const float a = p->a, k = p->k, thr = p->cfg.thr, gam = p->cfg.gamma / thr;
    float* x = p->xd;
    memset(p->zsum, 0, n * sizeof(float));

    if (p->ffi >= 0) {   /* feedforward inhibition: cortex gets the change */
        if (!p->has_xbar) {
            memcpy(p->xbar, x_in, dim * sizeof(float));
            p->has_xbar = true;
        } else {
            for (uint32_t i = 0; i < dim; i++)
                p->xbar[i] = p->ffi * p->xbar[i] + (1.0f - p->ffi) * x_in[i];
        }
        for (uint32_t i = 0; i < dim; i++) x[i] = x_in[i] - p->xbar[i];
    } else {
        memcpy(x, x_in, dim * sizeof(float));
    }

    memcpy(p->I_in, p->h, n * sizeof(float));
    for (uint32_t i = 0; i < dim; i++) {
        const float xi = x[i];
        const float* w = p->W_in + (size_t)i * n;
        for (uint32_t j = 0; j < n; j++) p->I_in[j] += xi * w[j];
    }

    for (uint32_t step = 0; step < p->cfg.sub; step++) {
        memcpy(p->z_prev, p->z, n * sizeof(float));
        const float* zp = p->z_prev;

        /* membrane: leak + input + recurrent spikes - reset */
        for (uint32_t j = 0; j < n; j++) {
            p->b[j] = p->rho[j] * p->b[j] + zp[j];
            p->v[j] = a * p->v[j] + p->I_in[j] - zp[j] * thr;
        }
        for (uint32_t i = 0; i < n; i++) {
            if (zp[i] == 0.0f) continue;
            const float* w = p->W_rec + (size_t)i * n;
            for (uint32_t j = 0; j < n; j++) p->v[j] += w[j];
        }
        for (uint32_t kk = 0; kk < dim; kk++) p->y[kk] *= k;
        for (uint32_t j = 0; j < n; j++) {
            float A = thr + p->beta[j] * p->b[j];
            float zj = p->v[j] > A ? 1.0f : 0.0f;
            p->z[j] = zj;
            p->zsum[j] += zj;
            if (zj != 0.0f) {
                const float* w = p->W_out + (size_t)j * dim;
                for (uint32_t kk = 0; kk < dim; kk++) p->y[kk] += w[kk];
            }
        }

        /* eligibility traces: adaptation traces use the previous step's psi
         * and presynaptic traces; L holds rho - psi_prev beta per neuron */
        float* decay_a = p->L;
        for (uint32_t j = 0; j < n; j++) decay_a[j] = p->rho[j] - p->psi[j] * p->beta[j];
        for (uint32_t i = 0; i < dim; i++) {
            const float e = p->eps_in[i];
            float* epa = p->epa_in + (size_t)i * n;
            for (uint32_t j = 0; j < n; j++) epa[j] = p->psi[j] * e + decay_a[j] * epa[j];
        }
        for (uint32_t i = 0; i < n; i++) {
            const float e = p->eps_rec[i];
            float* epa = p->epa_rec + (size_t)i * n;
            for (uint32_t j = 0; j < n; j++) epa[j] = p->psi[j] * e + decay_a[j] * epa[j];
        }
        for (uint32_t i = 0; i < dim; i++) p->eps_in[i] = a * p->eps_in[i] + x[i];
        for (uint32_t i = 0; i < n; i++) p->eps_rec[i] = a * p->eps_rec[i] + zp[i];
        for (uint32_t j = 0; j < n; j++) {
            float A = thr + p->beta[j] * p->b[j];
            float d = 1.0f - fabsf(p->v[j] - A) / thr;
            p->psi[j] = d > 0.0f ? gam * d : 0.0f;
        }
        for (uint32_t i = 0; i < dim; i++) {
            const float e = p->eps_in[i];
            const float* epa = p->epa_in + (size_t)i * n;
            float* eb = p->ebar_in + (size_t)i * n;
            for (uint32_t j = 0; j < n; j++)
                eb[j] = k * eb[j] + p->psi[j] * (e - p->beta[j] * epa[j]);
        }
        for (uint32_t i = 0; i < n; i++) {
            const float e = p->eps_rec[i];
            const float* epa = p->epa_rec + (size_t)i * n;
            float* eb = p->ebar_rec + (size_t)i * n;
            for (uint32_t j = 0; j < n; j++)
                eb[j] = k * eb[j] + p->psi[j] * (e - p->beta[j] * epa[j]);
        }
        for (uint32_t j = 0; j < n; j++) p->zbar[j] = k * p->zbar[j] + p->z[j];
    }
}

static void learn(auditory_predictor_t* p) {
    const uint32_t n = p->n, dim = p->dim;
    const auditory_predictor_config_t* c = &p->cfg;
    for (uint32_t j = 0; j < n; j++) {
        float rate = p->zsum[j] / c->sub;
        p->L[j] = c->reg * (rate - p->r_step);
    }
    for (uint32_t kk = 0; kk < dim; kk++) {
        const float e = p->err[kk];
        const float* fb = p->B + (size_t)kk * n;
        for (uint32_t j = 0; j < n; j++) p->L[j] += e * fb[j];
    }
    for (uint32_t i = 0; i < dim; i++) {
        float* w = p->W_in + (size_t)i * n;
        const float* eb = p->ebar_in + (size_t)i * n;
        for (uint32_t j = 0; j < n; j++) w[j] -= c->lr * p->L[j] * eb[j];
    }
    for (uint32_t i = 0; i < n; i++) {
        float* w = p->W_rec + (size_t)i * n;
        const float* eb = p->ebar_rec + (size_t)i * n;
        for (uint32_t j = 0; j < n; j++) w[j] -= c->lr * p->L[j] * eb[j];
        w[i] = 0.0f;
    }
    /* readout, normalised by presynaptic activity (NLMS) */
    double zz = 0.0;
    for (uint32_t j = 0; j < n; j++) zz += (double)p->zbar[j] * p->zbar[j];
    const float g = (float)(c->lr_out / (1.0 + zz));
    const float keep = 1.0f - c->out_decay;
    for (uint32_t j = 0; j < n; j++) {
        float* w = p->W_out + (size_t)j * dim;
        const float zj = p->zbar[j] * g;
        for (uint32_t kk = 0; kk < dim; kk++) w[kk] = (w[kk] - zj * p->err[kk]) * keep;
    }
}

int auditory_predictor_step(auditory_predictor_t* p, const float* x) {
    if (!p || !x) return -1;
    const uint32_t n = p->n, dim = p->dim;
    if (p->has_pending) {
        double mse = 0.0, copy = 0.0;
        for (uint32_t kk = 0; kk < dim; kk++) {
            float target = x[kk] - p->x_prev[kk];
            p->err[kk] = p->pending[kk] - target;
            mse += (double)p->err[kk] * p->err[kk];
            copy += (double)target * target;
        }
        mse /= dim;
        copy /= dim;
        if (!isfinite(mse)) return -1;   /* never learn from a non-finite error */
        learn(p);
        for (uint32_t j = 0; j < n; j++)
            p->h[j] += p->cfg.ip_lr * (p->r_step - p->zsum[j] / p->cfg.sub);
        /* statistics: the prediction was made before this frame was heard */
        float al = p->cfg.stats_window_frames > 1 ? 1.0f / p->cfg.stats_window_frames : 1.0f;
        if (p->scored == 0) {
            p->ema_mse = (float)mse;
            p->ema_copy = (float)copy;
        } else {
            p->ema_mse += al * ((float)mse - p->ema_mse);
            p->ema_copy += al * ((float)copy - p->ema_copy);
        }
        p->last_mse = (float)mse;
        p->last_copy = (float)copy;
        p->sum_mse += mse;
        p->sum_copy += copy;
        p->scored++;
    }
    present(p, x);
    memcpy(p->pending, p->y, dim * sizeof(float));
    memcpy(p->x_prev, x, dim * sizeof(float));
    p->has_pending = true;
    p->frames++;
    return 0;
}

int auditory_predictor_step_counts(auditory_predictor_t* p, const uint16_t* counts) {
    if (!p || !counts) return -1;
    for (uint32_t i = 0; i < p->dim; i++) p->x_tmp[i] = counts[i] / p->cfg.input_scale;
    return auditory_predictor_step(p, p->x_tmp);
}

void auditory_predictor_get_stats(const auditory_predictor_t* p, auditory_predictor_stats_t* out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!p) return;
    out->frames = p->frames;
    out->scored = p->scored;
    out->hours = p->frames * 0.04 / 3600.0;
    double zs = 0.0;
    for (uint32_t j = 0; j < p->n; j++) zs += p->zsum[j];
    out->rate_hz = (float)(zs / p->n / p->cfg.sub / (0.04 / p->cfg.sub));
    out->mse_recent = p->ema_mse;
    out->copy_mse_recent = p->ema_copy;
    out->skill_recent = p->ema_copy > 0 ? 1.0f - p->ema_mse / p->ema_copy : 0.0f;
    out->skill_total = p->sum_copy > 0 ? (float)(1.0 - p->sum_mse / p->sum_copy) : 0.0f;
}

/* ---------------------------------------------------------- persistence */

static const char MAGIC[8] = {'N', 'A', 'U', 'D', 'P', 'R', 'D', '1'};

/* Config fields in a fixed order (not the raw struct) so other writers,
 * e.g. a Python exporter, can produce the file. */
#define CFG_FLOATS(c) { &(c)->tau_m, &(c)->tau_out, &(c)->tau_a[0], &(c)->tau_a[1], \
    &(c)->frac_adaptive, &(c)->adapt_rise, &(c)->thr, &(c)->gamma, &(c)->target_rate, \
    &(c)->reg, &(c)->lr, &(c)->lr_out, &(c)->out_decay, &(c)->in_gain, &(c)->ip_lr, \
    &(c)->ffi_tau, &(c)->input_scale, &(c)->stats_window_frames }
#define N_CFG_FLOATS 18

static int wr(FILE* f, const void* d, size_t sz, size_t cnt) { return fwrite(d, sz, cnt, f) == cnt ? 0 : -1; }
static int rd(FILE* f, void* d, size_t sz, size_t cnt) { return fread(d, sz, cnt, f) == cnt ? 0 : -1; }

int auditory_predictor_save(const auditory_predictor_t* p, const char* path) {
    if (!p || !path) return -1;
    size_t len = strlen(path) + 5;
    char* tmp = (char*)malloc(len);
    if (!tmp) return -1;
    snprintf(tmp, len, "%s.tmp", path);
    FILE* f = fopen(tmp, "wb");
    if (!f) { free(tmp); return -1; }
    auditory_predictor_config_t c = p->cfg;
    float* cf[] = CFG_FLOATS(&c);
    uint32_t hdr[3] = {1u, p->dim, p->n};
    uint8_t flags[2] = {p->has_xbar, p->has_pending};
    size_t dn = (size_t)p->dim * p->n, nn = (size_t)p->n * p->n;
    int rc = wr(f, MAGIC, 1, 8) | wr(f, hdr, 4, 3) | wr(f, &c.sub, 4, 1) | wr(f, &c.seed, 8, 1);
    for (int i = 0; i < N_CFG_FLOATS; i++) rc |= wr(f, cf[i], 4, 1);
    rc |= wr(f, &p->frames, 8, 1) | wr(f, &p->scored, 8, 1) | wr(f, &p->sum_mse, 8, 1) |
          wr(f, &p->sum_copy, 8, 1) | wr(f, &p->ema_mse, 4, 1) | wr(f, &p->ema_copy, 4, 1);
    rc |= wr(f, p->W_in, 4, dn) | wr(f, p->W_rec, 4, nn) | wr(f, p->W_out, 4, dn) |
          wr(f, p->B, 4, dn) | wr(f, p->h, 4, p->n);
    rc |= wr(f, p->v, 4, p->n) | wr(f, p->z, 4, p->n) | wr(f, p->b, 4, p->n) |
          wr(f, p->zbar, 4, p->n) | wr(f, p->zsum, 4, p->n) | wr(f, p->y, 4, p->dim);
    rc |= wr(f, flags, 1, 2) | wr(f, p->xbar, 4, p->dim) | wr(f, p->x_prev, 4, p->dim) |
          wr(f, p->pending, 4, p->dim);
    if (fclose(f) != 0) rc = -1;
    if (rc == 0 && rename(tmp, path) != 0) rc = -1;
    if (rc != 0) remove(tmp);
    free(tmp);
    return rc ? -1 : 0;
}

auditory_predictor_t* auditory_predictor_load(const char* path) {
    if (!path) return NULL;
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    char magic[8];
    uint32_t hdr[3];
    auditory_predictor_config_t c = auditory_predictor_config_default();
    float* cf[] = CFG_FLOATS(&c);
    auditory_predictor_t* p = NULL;
    if (rd(f, magic, 1, 8) || memcmp(magic, MAGIC, 8) || rd(f, hdr, 4, 3) || hdr[0] != 1u ||
        rd(f, &c.sub, 4, 1) || rd(f, &c.seed, 8, 1)) goto fail;
    for (int i = 0; i < N_CFG_FLOATS; i++)
        if (rd(f, cf[i], 4, 1)) goto fail;
    c.n_rec = hdr[2];
    p = alloc_predictor(hdr[1], &c);
    if (!p) goto fail;
    size_t dn = (size_t)p->dim * p->n, nn = (size_t)p->n * p->n;
    uint8_t flags[2];
    if (rd(f, &p->frames, 8, 1) || rd(f, &p->scored, 8, 1) || rd(f, &p->sum_mse, 8, 1) ||
        rd(f, &p->sum_copy, 8, 1) || rd(f, &p->ema_mse, 4, 1) || rd(f, &p->ema_copy, 4, 1) ||
        rd(f, p->W_in, 4, dn) || rd(f, p->W_rec, 4, nn) || rd(f, p->W_out, 4, dn) ||
        rd(f, p->B, 4, dn) || rd(f, p->h, 4, p->n) ||
        rd(f, p->v, 4, p->n) || rd(f, p->z, 4, p->n) || rd(f, p->b, 4, p->n) ||
        rd(f, p->zbar, 4, p->n) || rd(f, p->zsum, 4, p->n) || rd(f, p->y, 4, p->dim) ||
        rd(f, flags, 1, 2) || rd(f, p->xbar, 4, p->dim) || rd(f, p->x_prev, 4, p->dim) ||
        rd(f, p->pending, 4, p->dim)) goto fail;
    p->has_xbar = flags[0] != 0;
    p->has_pending = flags[1] != 0;
    fclose(f);
    return p;
fail:
    auditory_predictor_destroy(p);
    fclose(f);
    return NULL;
}
