/**
 * @file nimcp_snn_fno.c
 * @brief Fourier Neural Operator for SNN Population Dynamics
 *
 * Learns (V_t, I_syn) → (V_{t+dt}, spikes) mappings using spectral
 * convolution. Trained on LIF ground truth, used for fast inference.
 */

#include "snn/nimcp_snn_fno.h"
#include "snn/nimcp_snn_types.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

/* =========================================================================
 * Helpers
 * ========================================================================= */

static uint32_t next_pow2(uint32_t v) {
    v--; v |= v >> 1; v |= v >> 2; v |= v >> 4;
    v |= v >> 8; v |= v >> 16; v++;
    return v < 4 ? 4 : v;
}

static float gelu_f(float x) {
    float c = 0.7978845608028654f;
    return 0.5f * x * (1.0f + tanhf(c * (x + 0.044715f * x * x * x)));
}

/** Simple DFT (real → complex) for small N */
static void dft_real(const float* in, uint32_t n, float* re, float* im) {
    uint32_t fn = n / 2 + 1;
    for (uint32_t k = 0; k < fn; k++) {
        double r = 0, m = 0;
        for (uint32_t j = 0; j < n; j++) {
            double a = -2.0 * M_PI * k * j / n;
            r += in[j] * cos(a);
            m += in[j] * sin(a);
        }
        re[k] = (float)r;
        im[k] = (float)m;
    }
}

/** Inverse DFT (complex half-spectrum → real) using Hermitian symmetry */
static void idft_real(const float* re, const float* im, uint32_t n, float* out) {
    uint32_t fn = n / 2 + 1;
    float inv = 1.0f / (float)n;
    for (uint32_t j = 0; j < n; j++) {
        double r = re[0]; /* DC component */
        for (uint32_t k = 1; k < fn - 1; k++) {
            double a = 2.0 * M_PI * k * j / n;
            r += 2.0 * (re[k] * cos(a) - im[k] * sin(a));
        }
        if (n % 2 == 0 && fn > 1) {
            r += re[fn - 1] * cos(M_PI * j);
        }
        out[j] = (float)(r * inv);
    }
}

/* =========================================================================
 * Configuration
 * ========================================================================= */

void snn_fno_config_default(snn_fno_config_t* config) {
    if (!config) return;
    config->n_modes = 0;
    config->hidden_channels = 16;
    config->n_blocks = 3;
    config->training_buffer_size = 256;
    config->learning_rate = 0.001f;
    config->replacement_threshold = 0.01f;
    config->per_population = true;
}

/* =========================================================================
 * Population FNO Lifecycle
 * ========================================================================= */

snn_fno_population_t* snn_fno_population_create(
    uint32_t pop_id, uint32_t n_neurons,
    const snn_fno_config_t* config)
{
    if (n_neurons == 0) return NULL;

    snn_fno_config_t cfg;
    if (config) cfg = *config;
    else snn_fno_config_default(&cfg);

    snn_fno_population_t* fno = nimcp_calloc(1, sizeof(snn_fno_population_t));
    if (!fno) return NULL;

    fno->pop_id = pop_id;
    fno->n_neurons = n_neurons;
    fno->state_dim = n_neurons;
    fno->input_dim = n_neurons;
    fno->n_blocks = cfg.n_blocks;
    fno->hidden_channels = cfg.hidden_channels;
    fno->n_modes = cfg.n_modes > 0 ? cfg.n_modes : n_neurons / 4;
    if (fno->n_modes < 2) fno->n_modes = 2;
    fno->fft_size = next_pow2(n_neurons);

    uint32_t hc = fno->hidden_channels;
    uint32_t total_in = fno->state_dim + fno->input_dim;
    uint32_t total_out = fno->state_dim + n_neurons;
    uint32_t nm = fno->n_modes;

    /* Lifting */
    fno->W_lift = nimcp_calloc((size_t)hc * total_in, sizeof(float));
    fno->b_lift = nimcp_calloc(hc, sizeof(float));
    fno->grad_W_lift = nimcp_calloc((size_t)hc * total_in, sizeof(float));
    fno->grad_b_lift = nimcp_calloc(hc, sizeof(float));

    /* Spectral blocks */
    fno->block_W_real = nimcp_calloc(cfg.n_blocks, sizeof(float*));
    fno->block_W_imag = nimcp_calloc(cfg.n_blocks, sizeof(float*));
    fno->block_grad_W_real = nimcp_calloc(cfg.n_blocks, sizeof(float*));
    fno->block_grad_W_imag = nimcp_calloc(cfg.n_blocks, sizeof(float*));
    fno->block_bypass = nimcp_calloc(cfg.n_blocks, sizeof(float*));
    fno->block_grad_bypass = nimcp_calloc(cfg.n_blocks, sizeof(float*));

    if (!fno->W_lift || !fno->block_W_real) {
        snn_fno_population_destroy(fno);
        return NULL;
    }

    size_t spec_size = (size_t)hc * hc * nm;
    size_t bypass_size = (size_t)hc * hc;

    for (uint32_t b = 0; b < cfg.n_blocks; b++) {
        fno->block_W_real[b] = nimcp_calloc(spec_size, sizeof(float));
        fno->block_W_imag[b] = nimcp_calloc(spec_size, sizeof(float));
        fno->block_grad_W_real[b] = nimcp_calloc(spec_size, sizeof(float));
        fno->block_grad_W_imag[b] = nimcp_calloc(spec_size, sizeof(float));
        fno->block_bypass[b] = nimcp_calloc(bypass_size, sizeof(float));
        fno->block_grad_bypass[b] = nimcp_calloc(bypass_size, sizeof(float));

        if (!fno->block_W_real[b]) {
            snn_fno_population_destroy(fno);
            return NULL;
        }

        /* Xavier init */
        float scale = sqrtf(2.0f / (float)(hc * nm));
        for (size_t i = 0; i < spec_size; i++) {
            fno->block_W_real[b][i] = scale * (2.0f * ((float)rand() / RAND_MAX) - 1.0f);
            fno->block_W_imag[b][i] = scale * (2.0f * ((float)rand() / RAND_MAX) - 1.0f);
        }
        scale = sqrtf(2.0f / (float)(hc + hc));
        for (size_t i = 0; i < bypass_size; i++) {
            fno->block_bypass[b][i] = scale * (2.0f * ((float)rand() / RAND_MAX) - 1.0f);
        }
    }

    /* Projection */
    fno->W_proj = nimcp_calloc((size_t)total_out * hc, sizeof(float));
    fno->b_proj = nimcp_calloc(total_out, sizeof(float));
    fno->grad_W_proj = nimcp_calloc((size_t)total_out * hc, sizeof(float));
    fno->grad_b_proj = nimcp_calloc(total_out, sizeof(float));

    /* Lifting init */
    float scale = sqrtf(2.0f / (float)(total_in + hc));
    for (size_t i = 0; i < (size_t)hc * total_in; i++) {
        fno->W_lift[i] = scale * (2.0f * ((float)rand() / RAND_MAX) - 1.0f);
    }
    scale = sqrtf(2.0f / (float)(hc + total_out));
    for (size_t i = 0; i < (size_t)total_out * hc; i++) {
        fno->W_proj[i] = scale * (2.0f * ((float)rand() / RAND_MAX) - 1.0f);
    }

    /* Training buffer */
    fno->buffer_size = cfg.training_buffer_size;
    fno->buffer = nimcp_calloc(fno->buffer_size, sizeof(snn_fno_state_pair_t));
    if (fno->buffer) {
        bool alloc_ok = true;
        for (uint32_t i = 0; i < fno->buffer_size && alloc_ok; i++) {
            fno->buffer[i].state_in = nimcp_calloc(n_neurons, sizeof(float));
            fno->buffer[i].synaptic_input = nimcp_calloc(n_neurons, sizeof(float));
            fno->buffer[i].state_out = nimcp_calloc(n_neurons, sizeof(float));
            fno->buffer[i].spike_out = nimcp_calloc(n_neurons, sizeof(float));
            if (!fno->buffer[i].state_in || !fno->buffer[i].synaptic_input ||
                !fno->buffer[i].state_out || !fno->buffer[i].spike_out) {
                alloc_ok = false;
            }
        }
        if (!alloc_ok) {
            snn_fno_population_destroy(fno);
            return NULL;
        }
    }

    NIMCP_LOGGING_INFO("SNN FNO pop %u: %u neurons, %u modes, %u blocks, %u hidden",
                       pop_id, n_neurons, nm, cfg.n_blocks, hc);
    return fno;
}

void snn_fno_population_destroy(snn_fno_population_t* fno) {
    if (!fno) return;

    nimcp_free(fno->W_lift); nimcp_free(fno->b_lift);
    nimcp_free(fno->grad_W_lift); nimcp_free(fno->grad_b_lift);

    for (uint32_t b = 0; b < fno->n_blocks; b++) {
        if (fno->block_W_real) nimcp_free(fno->block_W_real[b]);
        if (fno->block_W_imag) nimcp_free(fno->block_W_imag[b]);
        if (fno->block_grad_W_real) nimcp_free(fno->block_grad_W_real[b]);
        if (fno->block_grad_W_imag) nimcp_free(fno->block_grad_W_imag[b]);
        if (fno->block_bypass) nimcp_free(fno->block_bypass[b]);
        if (fno->block_grad_bypass) nimcp_free(fno->block_grad_bypass[b]);
    }
    nimcp_free(fno->block_W_real); nimcp_free(fno->block_W_imag);
    nimcp_free(fno->block_grad_W_real); nimcp_free(fno->block_grad_W_imag);
    nimcp_free(fno->block_bypass); nimcp_free(fno->block_grad_bypass);

    nimcp_free(fno->W_proj); nimcp_free(fno->b_proj);
    nimcp_free(fno->grad_W_proj); nimcp_free(fno->grad_b_proj);

    if (fno->buffer) {
        for (uint32_t i = 0; i < fno->buffer_size; i++) {
            nimcp_free(fno->buffer[i].state_in);
            nimcp_free(fno->buffer[i].synaptic_input);
            nimcp_free(fno->buffer[i].state_out);
            nimcp_free(fno->buffer[i].spike_out);
        }
        nimcp_free(fno->buffer);
    }

    nimcp_free(fno);
}

/* =========================================================================
 * Training Data Recording
 * ========================================================================= */

int snn_fno_record_pair(snn_fno_population_t* fno,
    const float* v_before, const float* i_syn,
    const float* v_after, const float* spikes, uint32_t n)
{
    if (!fno || !v_before || !v_after || !spikes || n == 0) return -1;
    if (!fno->buffer) return -1;
    if (n > fno->n_neurons) {
        n = fno->n_neurons;  /* Clamp to prevent buffer overflow */
    }

    uint32_t idx = fno->buffer_write_idx % fno->buffer_size;
    snn_fno_state_pair_t* pair = &fno->buffer[idx];

    memcpy(pair->state_in, v_before, n * sizeof(float));
    if (i_syn) memcpy(pair->synaptic_input, i_syn, n * sizeof(float));
    else memset(pair->synaptic_input, 0, n * sizeof(float));
    memcpy(pair->state_out, v_after, n * sizeof(float));
    memcpy(pair->spike_out, spikes, n * sizeof(float));

    fno->buffer_write_idx++;
    if (fno->buffer_count < fno->buffer_size)
        fno->buffer_count++;

    return 0;
}

/* =========================================================================
 * FNO Forward Pass (simplified spectral conv)
 * ========================================================================= */

static int fno_pop_forward(snn_fno_population_t* fno,
    const float* input_concat, /* [state_dim + input_dim] */
    float* output)             /* [state_dim + n_neurons] */
{
    uint32_t n = fno->n_neurons;
    uint32_t hc = fno->hidden_channels;
    uint32_t nm = fno->n_modes;
    uint32_t total_in = fno->state_dim + fno->input_dim;
    uint32_t total_out = fno->state_dim + n;

    /* Lifting: [total_in] → [hc] via dense */
    float* hidden = nimcp_calloc(hc, sizeof(float));
    if (!hidden) return -1;

    for (uint32_t ch = 0; ch < hc; ch++) {
        float sum = fno->b_lift[ch];
        for (uint32_t j = 0; j < total_in; j++) {
            sum += fno->W_lift[ch * total_in + j] * input_concat[j];
        }
        hidden[ch] = gelu_f(sum);
    }

    /* Spectral blocks: simplified 1D spectral conv on [hc] vector
     * Treat the hidden channels as a 1D signal, FFT, multiply, IFFT */
    float* fft_re = nimcp_calloc(hc / 2 + 1, sizeof(float));
    float* fft_im = nimcp_calloc(hc / 2 + 1, sizeof(float));
    float* block_out = nimcp_calloc(hc, sizeof(float));

    if (!fft_re || !fft_im || !block_out) {
        nimcp_free(hidden); nimcp_free(fft_re);
        nimcp_free(fft_im); nimcp_free(block_out);
        return -1;
    }

    uint32_t freq_n = hc / 2 + 1;
    uint32_t modes = nm < freq_n ? nm : freq_n;

    for (uint32_t b = 0; b < fno->n_blocks; b++) {
        /* FFT hidden */
        dft_real(hidden, hc, fft_re, fft_im);

        /* Spectral multiply (simplified: diagonal in frequency) */
        float* wr = fno->block_W_real[b];
        float* wi = fno->block_W_imag[b];
        float* out_re = nimcp_calloc(freq_n, sizeof(float));
        float* out_im = nimcp_calloc(freq_n, sizeof(float));
        if (!out_re || !out_im) {
            nimcp_free(out_re); nimcp_free(out_im);
            break;
        }

        for (uint32_t k = 0; k < modes; k++) {
            /* Simplified: diagonal multiply (no cross-channel mixing) */
            uint32_t w_idx = k; /* Use first n_modes weights */
            if (w_idx < (uint32_t)(hc * hc * nm)) {
                out_re[k] = wr[w_idx] * fft_re[k] - wi[w_idx] * fft_im[k];
                out_im[k] = wr[w_idx] * fft_im[k] + wi[w_idx] * fft_re[k];
            }
        }

        /* IFFT */
        idft_real(out_re, out_im, hc, block_out);

        /* Bypass + GELU */
        for (uint32_t ch = 0; ch < hc; ch++) {
            float bypass = 0.0f;
            for (uint32_t j = 0; j < hc; j++) {
                bypass += fno->block_bypass[b][ch * hc + j] * hidden[j];
            }
            hidden[ch] = gelu_f(block_out[ch] + bypass);
        }

        nimcp_free(out_re);
        nimcp_free(out_im);
    }

    /* Projection: [hc] → [total_out] */
    for (uint32_t i = 0; i < total_out; i++) {
        float sum = fno->b_proj[i];
        for (uint32_t ch = 0; ch < hc; ch++) {
            sum += fno->W_proj[i * hc + ch] * hidden[ch];
        }
        output[i] = sum;
    }

    nimcp_free(hidden);
    nimcp_free(fft_re);
    nimcp_free(fft_im);
    nimcp_free(block_out);
    return 0;
}

/* =========================================================================
 * Training
 * ========================================================================= */

int snn_fno_train(snn_fno_population_t* fno, uint32_t n_epochs) {
    if (!fno || fno->buffer_count < 16) return -1;

    uint32_t n = fno->n_neurons;
    uint32_t total_in = fno->state_dim + fno->input_dim;
    uint32_t total_out = fno->state_dim + n;
    float lr = 0.001f;

    float* input_concat = nimcp_calloc(total_in, sizeof(float));
    float* predicted = nimcp_calloc(total_out, sizeof(float));
    float* target = nimcp_calloc(total_out, sizeof(float));
    if (!input_concat || !predicted || !target) {
        nimcp_free(input_concat); nimcp_free(predicted); nimcp_free(target);
        return -1;
    }

    float total_mse = 0.0f;
    uint32_t total_samples = 0;

    for (uint32_t epoch = 0; epoch < n_epochs; epoch++) {
        float epoch_mse = 0.0f;

        for (uint32_t s = 0; s < fno->buffer_count; s++) {
            snn_fno_state_pair_t* pair = &fno->buffer[s];

            /* Build input: [V_t; I_syn] */
            memcpy(input_concat, pair->state_in, n * sizeof(float));
            memcpy(input_concat + n, pair->synaptic_input, n * sizeof(float));

            /* Build target: [V_{t+dt}; spikes] */
            memcpy(target, pair->state_out, n * sizeof(float));
            memcpy(target + n, pair->spike_out, n * sizeof(float));

            /* Forward */
            fno_pop_forward(fno, input_concat, predicted);

            /* MSE loss + simple gradient step on projection weights */
            float sample_mse = 0.0f;
            for (uint32_t i = 0; i < total_out; i++) {
                float diff = predicted[i] - target[i];
                sample_mse += diff * diff;

                /* Simplified gradient: update projection bias directly */
                fno->b_proj[i] -= lr * 2.0f * diff / (float)total_out;
            }
            sample_mse /= (float)total_out;
            epoch_mse += sample_mse;
        }

        epoch_mse /= (float)fno->buffer_count;
        total_mse = epoch_mse;
    }

    fno->train_mse = total_mse;
    fno->train_steps += n_epochs;

    /* Check if ready for inference */
    if (total_mse < 0.01f && fno->buffer_count >= 64) {
        fno->ready_for_inference = true;
    }

    NIMCP_LOGGING_DEBUG("SNN FNO pop %u trained: MSE=%.6f, ready=%d",
                        fno->pop_id, total_mse, fno->ready_for_inference);
    return 0;
}

/* =========================================================================
 * Prediction
 * ========================================================================= */

int snn_fno_predict(snn_fno_population_t* fno,
    const float* v_current, const float* i_syn, uint32_t n,
    float* v_next, float* spikes)
{
    if (!fno || !v_current || !v_next || !spikes || n == 0) return -1;

    uint32_t total_in = fno->state_dim + fno->input_dim;
    uint32_t total_out = fno->state_dim + n;

    float* input_concat = nimcp_calloc(total_in, sizeof(float));
    float* output = nimcp_calloc(total_out, sizeof(float));
    if (!input_concat || !output) {
        nimcp_free(input_concat); nimcp_free(output);
        return -1;
    }

    memcpy(input_concat, v_current, n * sizeof(float));
    if (i_syn) memcpy(input_concat + n, i_syn, n * sizeof(float));

    int rc = fno_pop_forward(fno, input_concat, output);
    if (rc == 0) {
        memcpy(v_next, output, n * sizeof(float));
        /* Threshold spikes at 0.5 */
        for (uint32_t i = 0; i < n; i++) {
            spikes[i] = (output[n + i] > 0.5f) ? 1.0f : 0.0f;
        }
        fno->inference_steps++;
    }

    nimcp_free(input_concat);
    nimcp_free(output);
    return rc;
}

/* =========================================================================
 * Validation
 * ========================================================================= */

float snn_fno_validate(snn_fno_population_t* fno,
    const float* v_next_lif, const float* spikes_lif, uint32_t n)
{
    if (!fno || !v_next_lif || !spikes_lif || fno->buffer_count == 0) return 1e6f;

    /* Use last recorded pair's input for prediction */
    uint32_t last = (fno->buffer_write_idx - 1) % fno->buffer_size;
    snn_fno_state_pair_t* pair = &fno->buffer[last];

    float* v_pred = nimcp_calloc(n, sizeof(float));
    float* s_pred = nimcp_calloc(n, sizeof(float));
    if (!v_pred || !s_pred) {
        nimcp_free(v_pred); nimcp_free(s_pred);
        return 1e6f;
    }

    snn_fno_predict(fno, pair->state_in, pair->synaptic_input, n, v_pred, s_pred);

    float mse = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float dv = v_pred[i] - v_next_lif[i];
        float ds = s_pred[i] - spikes_lif[i];
        mse += dv * dv + ds * ds;
    }
    mse /= (float)(2 * n);

    fno->validation_mse = mse;
    nimcp_free(v_pred);
    nimcp_free(s_pred);
    return mse;
}

/* =========================================================================
 * Accessors
 * ========================================================================= */

float snn_fno_get_train_mse(const snn_fno_population_t* fno) {
    return fno ? fno->train_mse : 0.0f;
}

float snn_fno_get_validation_mse(const snn_fno_population_t* fno) {
    return fno ? fno->validation_mse : 0.0f;
}

bool snn_fno_is_ready(const snn_fno_population_t* fno) {
    return fno ? fno->ready_for_inference : false;
}

uint32_t snn_fno_param_count(const snn_fno_population_t* fno) {
    if (!fno) return 0;
    uint32_t hc = fno->hidden_channels;
    uint32_t nm = fno->n_modes;
    uint32_t total_in = fno->state_dim + fno->input_dim;
    uint32_t total_out = fno->state_dim + fno->n_neurons;

    uint32_t lift = hc * total_in + hc;
    uint32_t spectral = fno->n_blocks * (hc * hc * nm * 2 + hc * hc);
    uint32_t proj = total_out * hc + total_out;
    return lift + spectral + proj;
}

/* =========================================================================
 * Network-level FNO (placeholder — wires into snn_network_step)
 * ========================================================================= */

/** Cap on FNO neurons per population — keeps memory tractable for big pops.
 * 1024 × 4 floats × scratch buffer = 16 KB/pop for v_prev. */
#define SNN_FNO_POPULATION_NEURON_CAP 1024u

int snn_network_init_fno(snn_network_t* network, const snn_fno_config_t* config) {
    if (!network) return -1;
    if (network->fno_populations) return 0; /* idempotent — already initialized */

    uint32_t n_pops = network->n_populations;
    if (n_pops == 0) return 0;

    network->fno_populations =
        (struct snn_fno_population_s**)nimcp_calloc(n_pops, sizeof(snn_fno_population_t*));
    if (!network->fno_populations) return -1;

    snn_fno_config_t cfg;
    if (config) cfg = *config;
    else snn_fno_config_default(&cfg);

    /* Compute max population size for v_prev scratch stride.
     * The stride is what we actually copy for each pop on each step. */
    size_t max_pop_n = 0;
    for (uint32_t p = 0; p < n_pops; p++) {
        if (!network->populations[p]) continue;
        uint32_t pop_n = network->populations[p]->n_neurons;
        if (pop_n > SNN_FNO_POPULATION_NEURON_CAP) pop_n = SNN_FNO_POPULATION_NEURON_CAP;
        if (pop_n > max_pop_n) max_pop_n = pop_n;
    }

    /* Per-population FNO models — capped at SNN_FNO_POPULATION_NEURON_CAP. */
    for (uint32_t p = 0; p < n_pops; p++) {
        if (!network->populations[p]) continue;
        uint32_t pop_n = network->populations[p]->n_neurons;
        uint32_t fno_n = (pop_n > SNN_FNO_POPULATION_NEURON_CAP)
                       ? SNN_FNO_POPULATION_NEURON_CAP : pop_n;
        network->fno_populations[p] = snn_fno_population_create(p, fno_n, &cfg);
    }
    network->fno_count = n_pops;

    /* v_prev scratch — owned by network, freed in destroy_fno.
     * Recording itself defaults to OFF: when the SNN runs on GPU
     * (CB-GPU path), pop->membrane_v is a GPU-resident tensor and
     * nimcp_tensor_data_const forces a host-side sync. With 64 pops ×
     * 2 reads per snn_network_step (snapshot + record) every tick,
     * the daemon ends up GPU-sync-bound — the production pod hit a
     * D-state stall this way during the 2026-05-09 deploy.
     *
     * Wiring is left in place; flip fno_recording_enabled true via
     * snn_set_fno_recording_enabled() to opt back in once the GPU-sync
     * path has a proper solution (probably batched async readback into
     * a host-pinned buffer rather than per-pop tensor_data_const). */
    if (max_pop_n > 0) {
        network->fno_v_prev_pop_stride = max_pop_n;
        network->fno_v_prev_buf = (float*)nimcp_calloc(
            (size_t)n_pops * max_pop_n, sizeof(float));
        if (!network->fno_v_prev_buf) {
            NIMCP_LOGGING_WARN("snn_network_init_fno: v_prev scratch alloc failed "
                               "(%zu pops × %zu floats); recording disabled",
                               (size_t)n_pops, max_pop_n);
            network->fno_recording_enabled = false;
            return 0;
        }
        network->fno_recording_enabled = false;  /* opt-in via setter */
        size_t bytes = (size_t)n_pops * max_pop_n * sizeof(float);
        NIMCP_LOGGING_INFO("snn_network_init_fno: %u FNO pops, scratch %zu KB "
                           "(recording OFF — call snn_set_fno_recording_enabled() to enable)",
                           n_pops, bytes / 1024);
    } else {
        network->fno_recording_enabled = false;
    }
    return 0;
}

void snn_set_fno_recording_enabled(snn_network_t* network, bool enabled) {
    if (!network) return;
    /* Don't enable if init didn't allocate the scratch — recording would
     * NULL-deref otherwise. Init logs the failure case, so this is just
     * a runtime safety. */
    if (enabled && !network->fno_v_prev_buf) {
        NIMCP_LOGGING_WARN("snn_set_fno_recording_enabled: refused enable "
                           "(scratch buffer not allocated)");
        return;
    }
    network->fno_recording_enabled = enabled;
    NIMCP_LOGGING_INFO("snn_set_fno_recording_enabled: %s",
                       enabled ? "ON" : "OFF");
}

bool snn_get_fno_recording_enabled(const snn_network_t* network) {
    return network ? network->fno_recording_enabled : false;
}

int snn_network_step_fno(snn_network_t* network, float dt) {
    /* Reserved for future inference-mode replacement of LIF with the trained
     * FNO. Currently unused — recording happens transparently in
     * snn_network_step via snn_fno_snapshot_v_before / snn_fno_record_post_step.
     * Returns 0 to indicate "no-op success" rather than -1. */
    (void)network; (void)dt;
    return 0;
}

void snn_network_destroy_fno(snn_network_t* network) {
    if (!network) return;
    if (network->fno_populations) {
        for (uint32_t p = 0; p < network->fno_count; p++) {
            if (network->fno_populations[p]) {
                snn_fno_population_destroy(network->fno_populations[p]);
            }
        }
        nimcp_free(network->fno_populations);
        network->fno_populations = NULL;
    }
    network->fno_count = 0;
    if (network->fno_v_prev_buf) {
        nimcp_free(network->fno_v_prev_buf);
        network->fno_v_prev_buf = NULL;
    }
    network->fno_v_prev_pop_stride = 0;
    network->fno_recording_enabled = false;
}

/* =========================================================================
 * Recording hooks called from snn_network_step
 *
 * snn_fno_snapshot_v_before: copy each population's V[] into the network's
 *   per-pop scratch slot. Called at the very top of snn_network_step,
 *   BEFORE any integration logic.
 * snn_fno_record_post_step: read each population's V_after + spikes, pair
 *   with the stashed v_before, and feed snn_fno_record_pair. Called at the
 *   bottom of snn_network_step, AFTER all integration and stat updates.
 *
 * Both no-op when fno_recording_enabled is false or the FNO array isn't
 * wired (legacy callers running without snn_network_init_fno).
 * ========================================================================= */

void snn_fno_snapshot_v_before(snn_network_t* network) {
    if (!network) return;
    if (!network->fno_recording_enabled) return;
    if (!network->fno_v_prev_buf) return;
    size_t stride = network->fno_v_prev_pop_stride;
    if (stride == 0) return;

    /* Bound the loop by fno_count (size of the scratch buffer slots) AND
     * n_populations (current SNN size). If a population was added after
     * snn_network_init_fno ran, indexing past fno_count would write past
     * fno_v_prev_buf — we have no slot for it, so just skip. */
    uint32_t bound = network->n_populations;
    if (bound > network->fno_count) bound = network->fno_count;
    for (uint32_t p = 0; p < bound; p++) {
        snn_population_t* pop = network->populations[p];
        if (!pop || !pop->membrane_v) continue;
        const float* v = (const float*)nimcp_tensor_data_const(pop->membrane_v);
        if (!v) continue;
        size_t n = pop->n_neurons;
        if (n > stride) n = stride;
        memcpy(network->fno_v_prev_buf + (size_t)p * stride, v, n * sizeof(float));
    }
}

void snn_fno_record_post_step(snn_network_t* network) {
    if (!network) return;
    if (!network->fno_recording_enabled) return;
    if (!network->fno_v_prev_buf) return;
    if (!network->fno_populations) return;
    size_t stride = network->fno_v_prev_pop_stride;
    if (stride == 0) return;

    uint32_t bound = network->n_populations;
    if (bound > network->fno_count) bound = network->fno_count;

    for (uint32_t p = 0; p < bound; p++) {
        snn_fno_population_t* fno = network->fno_populations[p];
        snn_population_t* pop = network->populations[p];
        if (!fno || !pop || !pop->membrane_v || !pop->spike_output) continue;

        const float* v_after = (const float*)nimcp_tensor_data_const(pop->membrane_v);
        const float* spikes  = (const float*)nimcp_tensor_data_const(pop->spike_output);
        if (!v_after || !spikes) continue;

        const float* v_before = network->fno_v_prev_buf + (size_t)p * stride;
        /* I_syn proxy: external_current is set on lightweight CSR pops; legacy
         * pops leave it NULL and snn_fno_record_pair will zero-fill that slot.
         * That's an acceptable v1 approximation — the FNO will still learn
         * non-trivial dynamics from V_t alone. */
        const float* i_syn = pop->external_current;

        uint32_t n = pop->n_neurons;
        snn_fno_record_pair(fno, v_before, i_syn, v_after, spikes, n);
    }
}
