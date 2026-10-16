/**
 * @file nimcp_fno_layer.h
 * @brief Fourier Neural Operator — spectral convolution layer
 *
 * WHAT: Learns mappings between functions using spectral convolutions
 * WHY:  Global receptive field in one layer, resolution-independent,
 *       naturally captures multi-scale structure (audio, population dynamics)
 * HOW:  FFT → pointwise multiply with learned complex weights → IFFT
 *       Plus bypass (1x1 conv) residual connection
 *
 * Reusable by both FNO Audio Cortex and FNO Population Dynamics.
 *
 * MATHEMATICAL BASIS:
 *   Output = IFFT(W_spectral ⊙ FFT(input)) + W_bypass @ input
 *   Where W_spectral ∈ C^{out_ch × in_ch × n_modes} are learned
 *   complex weights truncated to the first n_modes Fourier modes.
 */

#ifndef NIMCP_FNO_LAYER_H
#define NIMCP_FNO_LAYER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Spectral Convolution Layer
 * ========================================================================= */

typedef struct fno_spectral_conv_s {
    uint32_t in_channels;
    uint32_t out_channels;
    uint32_t n_modes;           /**< Fourier modes to keep (truncation) */
    uint32_t spatial_size;      /**< Original spatial dimension */
    uint32_t fft_size;          /**< Padded to power of 2 */
    uint32_t freq_size;         /**< fft_size/2 + 1 (real FFT output) */

    /* Learned complex weights [out_ch * in_ch * n_modes] */
    float* W_real;
    float* W_imag;
    float* grad_W_real;
    float* grad_W_imag;

    /* Bypass (residual) 1x1 conv: [out_ch * in_ch] */
    float* W_bypass;
    float* grad_W_bypass;

    /* Bias per output channel */
    float* bias;
    float* grad_bias;

    /* Cached for backward pass */
    float* input_cache;         /**< [in_ch * spatial_size] */
    float* input_hat_real;      /**< FFT of input, real [in_ch * freq_size] */
    float* input_hat_imag;      /**< FFT of input, imag [in_ch * freq_size] */
    float* pre_activation;      /**< Output before GELU [out_ch * spatial_size] */
} fno_spectral_conv_t;

/** Create spectral convolution layer */
fno_spectral_conv_t* fno_spectral_conv_create(
    uint32_t in_ch, uint32_t out_ch, uint32_t n_modes, uint32_t spatial_size);

/** Destroy spectral convolution layer */
void fno_spectral_conv_destroy(fno_spectral_conv_t* layer);

/** Forward pass: input [in_ch * spatial_size] → output [out_ch * spatial_size] */
int fno_spectral_conv_forward(fno_spectral_conv_t* layer,
    const float* input, float* output);

/** Backward pass: dl_dout [out_ch * spatial_size] → dl_din [in_ch * spatial_size] */
int fno_spectral_conv_backward(fno_spectral_conv_t* layer,
    const float* dl_dout, float* dl_din);

/** Zero all gradients */
void fno_spectral_conv_zero_grad(fno_spectral_conv_t* layer);

/** Apply gradients with learning rate */
void fno_spectral_conv_step(fno_spectral_conv_t* layer, float lr);

/** Get parameter count */
uint32_t fno_spectral_conv_param_count(const fno_spectral_conv_t* layer);

/* =========================================================================
 * FNO Audio Processor (full pipeline for audio cortex)
 * ========================================================================= */

typedef struct fno_audio_processor_s {
    /* Lifting: project 1-channel mel to hidden_ch */
    float* W_lift;              /**< [hidden_ch] (1-channel input) */
    float* b_lift;              /**< [hidden_ch] */
    float* grad_W_lift;
    float* grad_b_lift;

    /* FNO blocks */
    fno_spectral_conv_t** blocks;
    uint32_t n_blocks;
    uint32_t hidden_channels;
    uint32_t n_modes;

    /* Projection: hidden_ch → embed_dim */
    float* W_proj;              /**< [embed_dim * hidden_ch] */
    float* b_proj;              /**< [embed_dim] */
    float* grad_W_proj;
    float* grad_b_proj;

    /* Configuration */
    uint32_t input_size;        /**< mel_size */
    uint32_t embed_dim;         /**< Output embedding dimension */

    /* Cached for backward */
    float* lifted;              /**< [hidden_ch * input_size] */
    float** block_outputs;      /**< [n_blocks][hidden_ch * input_size] */
    float* pooled;              /**< [hidden_ch] after global avg pool */

    /* Training metrics */
    float last_loss;            /**< Most recent forward loss */
    float ema_loss;             /**< EMA of loss (alpha=0.01) */
    uint64_t forward_steps;     /**< Total forward passes */
} fno_audio_processor_t;

/** Create FNO audio processor */
fno_audio_processor_t* fno_audio_create(
    uint32_t mel_size, uint32_t embed_dim,
    uint32_t hidden_ch, uint32_t n_modes, uint32_t n_blocks);

/** Destroy FNO audio processor */
void fno_audio_destroy(fno_audio_processor_t* proc);

/** Forward: mel [mel_size] → embedding [embed_dim] */
int fno_audio_forward(fno_audio_processor_t* proc,
    const float* mel, uint32_t mel_size, float* embedding);

/** Backward: dl_dembed [embed_dim] → dl_dinput [mel_size] (can be NULL) */
int fno_audio_backward(fno_audio_processor_t* proc,
    const float* dl_dembed, float* dl_dinput);

/** Apply gradients */
void fno_audio_step(fno_audio_processor_t* proc, float lr);

/** Zero all gradients */
void fno_audio_zero_grad(fno_audio_processor_t* proc);

/** Get total parameter count */
uint32_t fno_audio_param_count(const fno_audio_processor_t* proc);

/** Record loss for metrics tracking (called by cortex_cnn_backward).
 *  Updates last_loss + ema_loss (alpha=0.01). */
void fno_audio_record_loss(fno_audio_processor_t* proc, float loss);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FNO_LAYER_H */
