/**
 * @file ann_gradcheck.c
 * @brief Manual GPU test: finite-difference check of the clip-mode ANN
 *        backward (NIMCP_ANN_GPU_GRAD=clip, 2026-11-12).
 *
 * Loads the production ANN from a brain checkpoint, runs one GPU forward on a
 * random (input, target) pair, accumulates the clip-mode gradient into
 * buffers this test owns, and compares it with central differences of the
 * GPU loss L = sum_i (o_i - t_i)^2. Parameters are nudged directly in GPU
 * memory (biases[l]->data, csr.values), so nothing else changes.
 *
 * The accumulate stores delta = 2(t - o) = -dL/dz, so the analytic dL/dp is
 * minus the accumulated value.
 *
 * Checked per layer: the NB biases and NW weights with the largest |grad|.
 * PASS when every checked parameter agrees within 5% (relative) at the
 * better of two step sizes.
 *
 * Usage: ann_gradcheck <brain.bin> [NB=5] [NW=3] [seed=7]
 * Build: gcc -O2 -std=gnu11 -Iinclude -I<cuda>/include tests/manual/ann_gradcheck.c
 *            -L<build>/lib -lnimcp -L<cudart dir> -lcudart -lm
 */
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "gpu/training/nimcp_training_bridge.h"
#include "gpu/training/nimcp_training_gpu.h"

#include <cuda_runtime_api.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct {
    nimcp_gpu_weight_cache_t* c;
    const float* in;
    const float* tgt;
    float* out;
    uint32_t nin, nout;
} fwd_t;

static double loss_of(fwd_t* f)
{
    if (!nimcp_gpu_forward_pass(f->c, f->in, f->nin, f->out, f->nout)) {
        fprintf(stderr, "forward failed\n");
        exit(1);
    }
    double L = 0;
    for (uint32_t i = 0; i < f->nout; i++) {
        double d = (double)f->out[i] - f->tgt[i];
        L += d * d;
    }
    return L;
}

/* Central difference of L in the device float at p */
static double fd_at(fwd_t* f, float* p_dev, double eps)
{
    float v0, v;
    cudaMemcpy(&v0, p_dev, sizeof v0, cudaMemcpyDeviceToHost);
    v = v0 + (float)eps;
    double e_up = (double)v - v0;   /* the step actually representable */
    cudaMemcpy(p_dev, &v, sizeof v, cudaMemcpyHostToDevice);
    double Lp = loss_of(f);
    v = v0 - (float)eps;
    double e_dn = (double)v0 - v;
    cudaMemcpy(p_dev, &v, sizeof v, cudaMemcpyHostToDevice);
    double Lm = loss_of(f);
    cudaMemcpy(p_dev, &v0, sizeof v0, cudaMemcpyHostToDevice);
    return (Lp - Lm) / (e_up + e_dn);
}

static double rel_err(double a, double b)
{
    double m = fmax(fabs(a), fabs(b));
    return m > 1e-12 ? fabs(a - b) / m : 0.0;
}

/* Indices of the k largest |g| (k small; simple selection) */
static void top_k(const float* g, uint64_t n, uint32_t k, uint64_t* idx)
{
    for (uint32_t j = 0; j < k; j++) idx[j] = UINT64_MAX;
    for (uint64_t i = 0; i < n; i++) {
        float a = fabsf(g[i]);
        if (!isfinite(a)) continue;
        for (uint32_t j = 0; j < k; j++) {
            if (idx[j] == UINT64_MAX || a > fabsf(g[idx[j]])) {
                for (uint32_t m = k - 1; m > j; m--) idx[m] = idx[m - 1];
                idx[j] = i;
                break;
            }
        }
    }
}

static int check_one(fwd_t* f, float* p_dev, double analytic, const char* what,
                     uint32_t layer, uint64_t i, double* worst)
{
    double fd1 = fd_at(f, p_dev, 1e-2);
    double fd2 = fd_at(f, p_dev, 1e-3);
    double e1 = rel_err(fd1, analytic), e2 = rel_err(fd2, analytic);
    double e = fmin(e1, e2);
    if (e > *worst) *worst = e;
    printf("  L%u %s[%8lu]  analytic %+.6e  fd(1e-2) %+.6e (%.2f%%)  fd(1e-3) %+.6e (%.2f%%)%s\n",
           layer, what, (unsigned long)i, analytic, fd1, 100 * e1, fd2, 100 * e2,
           e > 0.05 ? "  <-- FAIL" : "");
    return e > 0.05 ? 1 : 0;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <brain.bin> [NB=5] [NW=3] [seed=7]\n", argv[0]);
        return 2;
    }
    uint32_t NB = argc > 2 ? (uint32_t)atoi(argv[2]) : 5;
    uint32_t NW = argc > 3 ? (uint32_t)atoi(argv[3]) : 3;
    unsigned seed = argc > 4 ? (unsigned)atoi(argv[4]) : 7;
    setenv("NIMCP_ANN_GPU_GRAD", "clip", 1);   /* forward records LayerNorm state */

    adaptive_network_t net = adaptive_network_load(argv[1]);
    if (!net) {
        fprintf(stderr, "load failed\n");
        return 1;
    }
    const network_config_t* bc = &adaptive_network_get_config(net)->base_config;
    uint32_t nin = bc->layer_sizes[0], nout = bc->layer_sizes[bc->num_layers - 1];

    srand(seed);
    float* in = malloc(nin * sizeof(float));
    float* tgt = malloc(nout * sizeof(float));
    float* out = malloc(nout * sizeof(float));
    gauss(in, nin, 1.0f);
    gauss(tgt, nout, 2.0f);

    /* One readonly forward makes sure the GPU cache exists and is uploaded */
    adaptive_network_forward_readonly(net, in, nin, out, nout, 0);
    nimcp_gpu_weight_cache_t* c = adaptive_network_get_gpu_weight_cache(net);
    if (!c) {
        fprintf(stderr, "no GPU weight cache (GPU off?)\n");
        return 1;
    }
    uint32_t L = c->num_layers;
    printf("== gradcheck layers=%u residual-capable=%s NB=%u NW=%u seed=%u\n", L,
           L > 3 ? "yes" : "no", NB, NW, seed);

    fwd_t f = { c, in, tgt, out, nin, nout };
    double L0 = loss_of(&f);
    if (!c->clip_ln_valid) {
        fprintf(stderr, "forward did not record LayerNorm state\n");
        return 1;
    }
    double on = 0;
    for (uint32_t i = 0; i < nout; i++) on += (double)out[i] * out[i];
    printf("loss %.6e  |out| %.4f  residual %d\n", L0, sqrt(on), (int)c->clip_residual);

    /* Gradient buffers owned by the test, zeroed (accumulate adds) */
    float** dW = calloc(L - 1, sizeof(float*));
    float** dB = calloc(L - 1, sizeof(float*));
    for (uint32_t l = 1; l < L; l++) {
        nimcp_sparse_tensor_t* W = c->sparse_weights[l - 1];
        if (W && W->format == SPARSE_FORMAT_CSR && W->data.csr.nnz > 0) {
            size_t nb = (size_t)W->data.csr.nnz * sizeof(float);
            if (cudaMalloc((void**)&dW[l - 1], nb) != cudaSuccess) return 1;
            cudaMemset(dW[l - 1], 0, nb);
        }
        size_t bb = (size_t)c->layer_sizes[l] * sizeof(float);
        if (cudaMalloc((void**)&dB[l - 1], bb) != cudaSuccess) return 1;
        cudaMemset(dB[l - 1], 0, bb);
    }
    int* codes = malloc(L * sizeof(int));
    for (uint32_t l = 0; l < L; l++) {
        switch (c->layer_activations[l]) {
            case ACTIVATION_RELU:       codes[l] = 0; break;
            case ACTIVATION_LEAKY_RELU: codes[l] = 1; break;
            case ACTIVATION_TANH:       codes[l] = 2; break;
            case ACTIVATION_SIGMOID:    codes[l] = 3; break;
            case ACTIVATION_LINEAR:     codes[l] = 4; break;
            case ACTIVATION_ADAPTIVE:   codes[l] = 5; break;
            default:                    codes[l] = 2; break;
        }
        printf("  layer %u size %u act %d code %d\n", l, c->layer_sizes[l],
               (int)c->layer_activations[l], codes[l]);
    }
    codes[L - 1] = 4;

    if (!nimcp_gpu_clip_backward_accumulate(c->ctx, c->sparse_weights, c->activations, dW, dB,
                                            codes, L, c->layer_sizes, tgt, out, nout,
                                            c->clip_pre_h, c->clip_ln_y, c->clip_ln_inv_std,
                                            c->clip_ln_valid, c->clip_residual)) {
        fprintf(stderr, "clip backward failed\n");
        return 1;
    }

    int fails = 0, checked = 0;
    double worst = 0;
    uint64_t* idx = malloc((NB > NW ? NB : NW) * sizeof(uint64_t));
    for (uint32_t l = L - 1; l >= 1; l--) {
        uint32_t n = c->layer_sizes[l];
        float* gb = malloc(n * sizeof(float));
        cudaMemcpy(gb, dB[l - 1], n * sizeof(float), cudaMemcpyDeviceToHost);
        double gn = 0;
        for (uint32_t i = 0; i < n; i++) gn += (double)gb[i] * gb[i];
        printf("layer %u: |bias grad| %.4e\n", l, sqrt(gn));
        top_k(gb, n, NB, idx);
        for (uint32_t j = 0; j < NB && idx[j] != UINT64_MAX; j++) {
            float* p = (float*)c->biases[l - 1]->data + idx[j];
            fails += check_one(&f, p, -(double)gb[idx[j]], "b", l, idx[j], &worst);
            checked++;
        }
        free(gb);

        nimcp_sparse_tensor_t* W = c->sparse_weights[l - 1];
        if (NW > 0 && dW[l - 1]) {
            uint64_t nnz = (uint64_t)W->data.csr.nnz;
            float* gw = malloc(nnz * sizeof(float));
            cudaMemcpy(gw, dW[l - 1], nnz * sizeof(float), cudaMemcpyDeviceToHost);
            top_k(gw, nnz, NW, idx);
            for (uint32_t j = 0; j < NW && idx[j] != UINT64_MAX; j++) {
                float* p = (float*)W->data.csr.values + idx[j];
                fails += check_one(&f, p, -(double)gw[idx[j]], "w", l, idx[j], &worst);
                checked++;
            }
            free(gw);
        }
        fflush(stdout);
    }
    printf("RESULT: %s  (%d of %d parameters outside 5%%, worst %.2f%%)\n",
           fails ? "FAIL" : "PASS", fails, checked, 100 * worst);
    return fails ? 1 : 0;
}
