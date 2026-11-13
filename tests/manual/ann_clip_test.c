/**
 * @file ann_clip_test.c
 * @brief Manual GPU test: does the production ANN's GPU backward train every
 *        layer? Compare default vs NIMCP_ANN_GPU_GRAD=clip (2026-11-12).
 *
 * Loads the ANN from a brain checkpoint (.bin; adaptive_network_load enables
 * the GPU itself), memorises K random (input, target) pairs through the real
 * learn paths, and reports per epoch: mean cos(output, own target), held-out
 * cos, and each layer's relative weight/bias change since the start.
 *
 * Usage: ann_clip_test <brain.bin> <lr> <epochs> <single|batch> [K] [random|cluster|real:<file>] [fuse]
 *
 * random (default): K random pairs, 4 held out (memorisation only).
 * cluster: 8 clusters, each an (input centre, target centre) pair; an item is
 * its centre plus noise in both input and target. K items train (default
 * 48), 16 more from the same clusters are held out, so held_cos measures
 * generalisation; held_cent is cos(output, cluster target centre), the best
 * a model can do for an unseen item.
 * real:<file>: pairs from the trainer's embedding cache (written by
 * tests/manual/make_real_pairs.py:uint32 N, in, out; N inputs; N targets, float32). The
 * first K train, up to 256 of the rest are held out. fuse (default 0) adds one
 * fixed random vector to every input at fuse x the input's RMS, modelling the
 * cortex CNN blend in brain_learn_vector (alpha 2.0 live). Extra metrics,
 * per set (train / held): spec = cos(own target) - mean cos(other targets in
 * the set), top1 = fraction whose nearest target in the set is their own,
 * xsim = mean pairwise output cos. A model that outputs the mean target
 * scores spec 0 and top1 ~1/N however high own_cos is.
 * Scores come from the GPU forward (nimcp_gpu_forward_pass on the weight
 * cache), the function training optimises. adaptive_network_forward_readonly
 * is NOT used for scoring: every gradient flush flags the weights dirty on
 * the CPU, after which it falls back to the CPU forward, a different function
 * (cos ~0.65 to the GPU output on the production ANN). cpu_own is that
 * fallback's own_cos, for comparison.
 * The mode is chosen by the environment (NIMCP_ANN_GPU_GRAD=clip or unset);
 * it is read once per process, so run one process per mode.
 */
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "gpu/training/nimcp_training_bridge.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void gauss(float* v, uint32_t n, float norm)
{
    double s = 0;
    for (uint32_t i = 0; i < n; i++) {
        double u1 = (rand() + 1.0) / (RAND_MAX + 2.0), u2 = (rand() + 1.0) / (RAND_MAX + 2.0);
        v[i] = (float)(sqrt(-2 * log(u1)) * cos(2 * M_PI * u2));
        s += (double)v[i] * v[i];
    }
    float k = (float)(norm / sqrt(s));
    for (uint32_t i = 0; i < n; i++) v[i] *= k;
}

/* Item-specific fit over a set of n outputs O and targets Tg (row-major, dim d):
 * spec, top1, xsim as described in the header. */
static void set_metrics(const float* O, const float* Tg, uint32_t n, uint32_t d,
                        double* spec, double* top1, double* xsim)
{
    double* on = malloc(n * sizeof(double));
    double* tn = malloc(n * sizeof(double));
    for (uint32_t a = 0; a < n; a++) {
        double so = 0, st = 0;
        for (uint32_t i = 0; i < d; i++) {
            so += (double)O[(size_t)a * d + i] * O[(size_t)a * d + i];
            st += (double)Tg[(size_t)a * d + i] * Tg[(size_t)a * d + i];
        }
        on[a] = sqrt(so) + 1e-30;
        tn[a] = sqrt(st) + 1e-30;
    }
    double sp = 0, hit = 0, xs = 0;
    for (uint32_t a = 0; a < n; a++) {
        const float* o = &O[(size_t)a * d];
        double own = 0, others = 0, best = -2;
        uint32_t arg = 0;
        for (uint32_t b = 0; b < n; b++) {
            const float* t = &Tg[(size_t)b * d];
            double dot = 0;
            for (uint32_t i = 0; i < d; i++) dot += (double)o[i] * t[i];
            double c = dot / (on[a] * tn[b]);
            if (b == a) own = c; else others += c;
            if (c > best) { best = c; arg = b; }
        }
        sp += own - others / (n - 1);
        hit += arg == a;
        for (uint32_t b = a + 1; b < n; b++) {
            const float* o2 = &O[(size_t)b * d];
            double dot = 0;
            for (uint32_t i = 0; i < d; i++) dot += (double)o[i] * o2[i];
            xs += dot / (on[a] * on[b]);
        }
    }
    *spec = sp / n;
    *top1 = hit / n;
    *xsim = xs / ((double)n * (n - 1) / 2);
    free(on);
    free(tn);
}

static double cosd(const float* a, const float* b, uint32_t n)
{
    double d = 0, na = 0, nb = 0;
    for (uint32_t i = 0; i < n; i++) { d += (double)a[i] * b[i]; na += (double)a[i] * a[i]; nb += (double)b[i] * b[i]; }
    return (na > 0 && nb > 0) ? d / sqrt(na * nb) : 0.0;
}

/* Snapshot of every layer's incoming weights and biases, in a fixed order */
typedef struct {
    float* w;
    float* b;
    uint64_t nw, nb;
} snap_t;

static void take_snap(neural_network_t base, const network_config_t* bc, snap_t* s, bool alloc)
{
    uint32_t off = bc->layer_sizes[0];
    uint64_t nw = 0, nb = 0;
    if (alloc) {
        for (uint32_t l = 1; l < bc->num_layers; l++) {
            for (uint32_t i = 0; i < bc->layer_sizes[l]; i++) {
                nw += NEURON_IN_COUNT(neural_network_get_neuron(base, off + i));
            }
            nb += bc->layer_sizes[l];
            off += bc->layer_sizes[l];
        }
        s->w = malloc(nw * sizeof(float));
        s->b = malloc(nb * sizeof(float));
        s->nw = nw;
        s->nb = nb;
        off = bc->layer_sizes[0];
        nw = nb = 0;
    } else {
        off = bc->layer_sizes[0];
    }
    for (uint32_t l = 1; l < bc->num_layers; l++) {
        for (uint32_t i = 0; i < bc->layer_sizes[l]; i++) {
            neuron_t* n = neural_network_get_neuron(base, off + i);
            for (uint32_t k = 0; k < NEURON_IN_COUNT(n) && nw < s->nw; k++) {
                s->w[nw++] = NEURON_IN_HANDLE(n, k)->weight;
            }
            s->b[nb++] = n->bias;
        }
        off += bc->layer_sizes[l];
    }
}

/* Per-layer |current - start| / |start| for weights and biases */
static void report_change(neural_network_t base, const network_config_t* bc, const snap_t* s0)
{
    uint32_t off = bc->layer_sizes[0];
    uint64_t iw = 0, ib = 0;
    printf("    change:");
    for (uint32_t l = 1; l < bc->num_layers; l++) {
        double dw = 0, w0 = 0, db = 0, b0 = 0;
        for (uint32_t i = 0; i < bc->layer_sizes[l]; i++) {
            neuron_t* n = neural_network_get_neuron(base, off + i);
            for (uint32_t k = 0; k < NEURON_IN_COUNT(n) && iw < s0->nw; k++, iw++) {
                double d = NEURON_IN_HANDLE(n, k)->weight - s0->w[iw];
                dw += d * d;
                w0 += (double)s0->w[iw] * s0->w[iw];
            }
            double d = n->bias - s0->b[ib];
            db += d * d;
            b0 += (double)s0->b[ib] * s0->b[ib];
            ib++;
        }
        printf("  L%u w%.1e b%.1e", l, sqrt(dw / (w0 + 1e-30)), sqrt(db / (b0 + 1e-30)));
        off += bc->layer_sizes[l];
    }
    printf("\n");
}

int main(int argc, char** argv)
{
    if (argc < 5) {
        fprintf(stderr, "usage: %s <brain.bin> <lr> <epochs> <single|batch> [K] "
                "[random|cluster|real:<file>] [fuse]\n", argv[0]);
        return 2;
    }
    float lr = strtof(argv[2], NULL);
    int epochs = atoi(argv[3]);
    bool batch = strcmp(argv[4], "batch") == 0;
    bool cluster = argc > 6 && strcmp(argv[6], "cluster") == 0;
    const char* real_path = (argc > 6 && strncmp(argv[6], "real:", 5) == 0) ? argv[6] + 5 : NULL;
    float fuse = argc > 7 ? strtof(argv[7], NULL) : 0.0f;
    uint32_t K = argc > 5 ? (uint32_t)atoi(argv[5]) : (cluster ? 48 : real_path ? 512 : 16);
    uint32_t H = cluster ? 16 : 4;   /* held-out items (real: set from the file) */
    const uint32_t C = 8;            /* clusters; item k belongs to k % C */
    const char* mode = getenv("NIMCP_ANN_GPU_GRAD");
    printf("== mode=%s lr=%g epochs=%d path=%s K=%u task=%s fuse=%g\n", mode ? mode : "default",
           lr, epochs, batch ? "batch8" : "single", K,
           cluster ? "cluster" : real_path ? "real" : "random", fuse);

    adaptive_network_t net = adaptive_network_load(argv[1]);
    if (!net) {
        fprintf(stderr, "load failed\n");
        return 1;
    }
    const network_config_t* bc = &adaptive_network_get_config(net)->base_config;
    neural_network_t base = adaptive_network_get_base_network(net);
    uint32_t in = bc->layer_sizes[0], out = bc->layer_sizes[bc->num_layers - 1];

    srand(12345);   /* same items in every run */
    float* cin = malloc((size_t)C * in * sizeof(float));
    float* ctg = malloc((size_t)C * out * sizeof(float));
    float* noise = malloc((in > out ? in : out) * sizeof(float));
    for (uint32_t c = 0; c < C; c++) {
        gauss(&cin[(size_t)c * in], in, 1.0f);
        gauss(&ctg[(size_t)c * out], out, 2.0f);
    }
    float* rin = NULL;
    float* rtg = NULL;
    if (real_path) {
        FILE* f = fopen(real_path, "rb");
        uint32_t hdr[3];
        if (!f || fread(hdr, sizeof hdr, 1, f) != 1 || hdr[1] != in || hdr[2] != out || hdr[0] <= K) {
            fprintf(stderr, "bad real file %s (need N > K=%u, in=%u, out=%u)\n", real_path, K, in, out);
            return 1;
        }
        H = hdr[0] - K < 256 ? hdr[0] - K : 256;
        rin = malloc((size_t)hdr[0] * in * sizeof(float));
        rtg = malloc((size_t)hdr[0] * out * sizeof(float));
        if (fread(rin, sizeof(float), (size_t)hdr[0] * in, f) != (size_t)hdr[0] * in ||
            fread(rtg, sizeof(float), (size_t)hdr[0] * out, f) != (size_t)hdr[0] * out) {
            fprintf(stderr, "short read %s\n", real_path);
            return 1;
        }
        fclose(f);
        printf("real: %u items, %u train, %u held out\n", hdr[0], K, H);
    }
    float* fvec = NULL;
    if (fuse > 0) {
        fvec = malloc(in * sizeof(float));
        gauss(fvec, in, 1.0f);   /* rms = 1/sqrt(in) */
    }
    training_example_t* ex = calloc(K + H, sizeof(training_example_t));
    for (uint32_t k = 0; k < K + H; k++) {
        ex[k].input = malloc(in * sizeof(float));
        ex[k].target = malloc(out * sizeof(float));
        if (real_path) {
            memcpy(ex[k].input, &rin[(size_t)k * in], in * sizeof(float));
            memcpy(ex[k].target, &rtg[(size_t)k * out], out * sizeof(float));
        } else if (cluster) {
            /* centre + noise at 0.6x the centre's norm (item cos to centre ~0.86),
             * target noise at 0.5x (cos ~0.89); renormalised */
            uint32_t c = k % C;
            gauss(noise, in, 0.6f);
            for (uint32_t i = 0; i < in; i++) ex[k].input[i] = cin[(size_t)c * in + i] + noise[i];
            gauss(noise, out, 1.0f);
            for (uint32_t i = 0; i < out; i++) ex[k].target[i] = ctg[(size_t)c * out + i] + noise[i];
            double a = 0, b = 0;
            for (uint32_t i = 0; i < in; i++) a += (double)ex[k].input[i] * ex[k].input[i];
            for (uint32_t i = 0; i < out; i++) b += (double)ex[k].target[i] * ex[k].target[i];
            for (uint32_t i = 0; i < in; i++) ex[k].input[i] = (float)(ex[k].input[i] / sqrt(a));
            for (uint32_t i = 0; i < out; i++) ex[k].target[i] = (float)(2.0 * ex[k].target[i] / sqrt(b));
        } else {
            gauss(ex[k].input, in, 1.0f);
            gauss(ex[k].target, out, 2.0f);
        }
        if (fvec) {
            /* cortex_cnn_fusion_scale: scale = fuse * rms(input) / rms(fused) */
            double s = 0;
            for (uint32_t i = 0; i < in; i++) s += (double)ex[k].input[i] * ex[k].input[i];
            float sc = (float)(fuse * sqrt(s / in) * sqrt((double)in));
            for (uint32_t i = 0; i < in; i++) ex[k].input[i] += sc * fvec[i];
        }
        ex[k].input_size = in;
        ex[k].target_size = out;
        ex[k].confidence = 1.0f;
        snprintf(ex[k].label, sizeof ex[k].label, "item%u", k);
    }

    snap_t s0;
    take_snap(base, bc, &s0, true);
    float* o = malloc(out * sizeof(float));
    float* O = real_path ? malloc((size_t)(K + H) * out * sizeof(float)) : NULL;
    float* TG = NULL;
    if (real_path) {
        TG = malloc((size_t)(K + H) * out * sizeof(float));
        for (uint32_t k = 0; k < K + H; k++) memcpy(&TG[(size_t)k * out], ex[k].target, out * sizeof(float));
    }

    for (int e = 0; e <= epochs; e++) {
        double t0 = now_s(), loss = 0;
        int steps = 0;
        if (e > 0) {
            if (batch) {
                for (uint32_t k = 0; k < K; k += 8) {
                    uint32_t nb = (K - k < 8) ? K - k : 8;
                    loss += adaptive_network_learn_batch(net, &ex[k], nb, LEARN_MODE_DISTILLATION, lr);
                    steps++;
                }
            } else {
                for (uint32_t k = 0; k < K; k++) {
                    loss += adaptive_network_learn(net, &ex[k], LEARN_MODE_DISTILLATION, lr);
                    steps++;
                }
            }
        }
        double dt = now_s() - t0;
        double own = 0, held = 0, cent = 0, on = 0, cpu_own = 0;
        bool finite = true;
        nimcp_gpu_weight_cache_t* cache = adaptive_network_get_gpu_weight_cache(net);
        for (uint32_t k = 0; k < K; k++) {
            adaptive_network_forward_readonly(net, ex[k].input, in, o, out, 0);
            cpu_own += cosd(o, ex[k].target, out) / K;
        }
        for (uint32_t k = 0; k < K + H; k++) {
            if (!cache || !nimcp_gpu_forward_pass(cache, ex[k].input, in, o, out)) {
                fprintf(stderr, "GPU forward unavailable\n");
                return 1;
            }
            if (O) memcpy(&O[(size_t)k * out], o, out * sizeof(float));
            double c = cosd(o, ex[k].target, out);
            if (!isfinite(c)) finite = false;
            if (k < K) {
                own += c / K;
                double s = 0;
                for (uint32_t i = 0; i < out; i++) s += (double)o[i] * o[i];
                on += sqrt(s) / K;
            } else {
                held += c / H;
                if (cluster) cent += cosd(o, &ctg[(size_t)(k % C) * out], out) / H;
            }
        }
        printf("  epoch %2d: own_cos %+.4f held_cos %+.4f held_cent %+.4f |out| %8.3f cpu_own %+.4f "
               "loss %.4g grad_norm %.3g  %.0f ms/step%s\n",
               e, own, held, cent, on, cpu_own, steps ? loss / steps : 0.0,
               (double)adaptive_network_get_last_grad_norm(net),
               steps ? 1000.0 * dt / steps : 0.0, finite ? "" : "  NON-FINITE");
        if (O) {
            double sp, t1, xs, hsp, ht1, hxs;
            set_metrics(O, TG, K, out, &sp, &t1, &xs);
            set_metrics(&O[(size_t)K * out], &TG[(size_t)K * out], H, out, &hsp, &ht1, &hxs);
            printf("    train: spec %+.4f top1 %.3f xsim %.4f | held: spec %+.4f top1 %.3f xsim %.4f"
                   "  (chance top1 %.4f / %.4f)\n", sp, t1, xs, hsp, ht1, hxs, 1.0 / K, 1.0 / H);
        }
        if (e == 1 || e == epochs || (e > 0 && e % 10 == 0)) {
            report_change(base, bc, &s0);
        }
        fflush(stdout);
    }
    return 0;
}
