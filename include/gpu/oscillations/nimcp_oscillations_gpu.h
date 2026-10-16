/**
 * @file nimcp_oscillations_gpu.h
 * @brief GPU-accelerated Brain Oscillation operations using CUDA
 *
 * WHAT: CUDA kernels for brain oscillation computations
 * WHY:  Enable massive parallel computation of neural oscillations
 * HOW:  Custom kernels for phase sync, PAC, FFT-based power, coherence
 *
 * ARCHITECTURE:
 * - Phase synchronization across neural populations
 * - Cross-frequency coupling (phase-amplitude coupling)
 * - Oscillatory power computation (FFT-based)
 * - Coherence computation between brain regions
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_OSCILLATIONS_GPU_H
#define NIMCP_OSCILLATIONS_GPU_H

#include "gpu/context/nimcp_gpu_context.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// Oscillation Band Definitions
//=============================================================================

typedef enum {
    NIMCP_BAND_DELTA = 0,      /**< Delta band (0.5-4 Hz) */
    NIMCP_BAND_THETA = 1,      /**< Theta band (4-8 Hz) */
    NIMCP_BAND_ALPHA = 2,      /**< Alpha band (8-13 Hz) */
    NIMCP_BAND_BETA = 3,       /**< Beta band (13-30 Hz) */
    NIMCP_BAND_GAMMA = 4,      /**< Gamma band (30-100 Hz) */
    NIMCP_BAND_HIGH_GAMMA = 5  /**< High gamma (100-200 Hz) */
} nimcp_oscillation_band_t;

//=============================================================================
// Oscillation Parameters
//=============================================================================

/**
 * @brief Parameters for oscillation analysis
 */
typedef struct {
    float sampling_rate;        /**< Sampling rate in Hz */
    float dt;                   /**< Time step in seconds */
    size_t n_samples;           /**< Number of time samples */
    size_t n_channels;          /**< Number of channels/regions */
    float freq_low;             /**< Low frequency bound (Hz) */
    float freq_high;            /**< High frequency bound (Hz) */
    size_t n_fft;               /**< FFT window size */
    size_t hop_length;          /**< Hop length for STFT */
} nimcp_oscillation_params_t;

/**
 * @brief Phase-Amplitude Coupling parameters
 */
typedef struct {
    float phase_freq_low;       /**< Phase frequency low bound (Hz) */
    float phase_freq_high;      /**< Phase frequency high bound (Hz) */
    float amp_freq_low;         /**< Amplitude frequency low bound (Hz) */
    float amp_freq_high;        /**< Amplitude frequency high bound (Hz) */
    size_t n_phase_bins;        /**< Number of phase bins for MI calculation */
    bool use_hilbert;           /**< Use Hilbert transform for instantaneous phase */
} nimcp_pac_params_t;

/**
 * @brief Coherence computation parameters
 */
typedef struct {
    size_t n_fft;               /**< FFT size for coherence */
    size_t n_overlap;           /**< Overlap between segments */
    float freq_low;             /**< Low frequency for band coherence */
    float freq_high;            /**< High frequency for band coherence */
    bool imaginary_coherence;   /**< Compute imaginary coherence (iCoh) */
} nimcp_coherence_params_t;

//=============================================================================
// Oscillation State Structures
//=============================================================================

/**
 * @brief Oscillation analysis state
 */
typedef struct {
    nimcp_gpu_tensor_t* signal;         /**< Input signal buffer */
    nimcp_gpu_tensor_t* phase;          /**< Instantaneous phase */
    nimcp_gpu_tensor_t* amplitude;      /**< Instantaneous amplitude */
    nimcp_gpu_tensor_t* power;          /**< Band power */
    nimcp_gpu_tensor_t* fft_buffer;     /**< FFT work buffer */
    nimcp_oscillation_params_t params;  /**< Analysis parameters */
} nimcp_oscillation_state_t;

/**
 * @brief Phase synchronization state
 */
typedef struct {
    nimcp_gpu_tensor_t* phase_diff;     /**< Phase differences between channels */
    nimcp_gpu_tensor_t* plv;            /**< Phase Locking Value */
    nimcp_gpu_tensor_t* pli;            /**< Phase Lag Index */
    nimcp_gpu_tensor_t* sync_matrix;    /**< Synchronization matrix */
    size_t n_channels;                  /**< Number of channels */
    size_t window_size;                 /**< Window for PLV computation */
} nimcp_phase_sync_state_t;

//=============================================================================
// Oscillation State Lifecycle
//=============================================================================

/**
 * @brief Create oscillation analysis state
 *
 * @param ctx GPU context
 * @param params Oscillation parameters
 * @return State on success, NULL on failure
 */
NIMCP_EXPORT nimcp_oscillation_state_t* nimcp_oscillation_state_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_oscillation_params_t* params
);

/**
 * @brief Destroy oscillation state
 *
 * @param state State to destroy
 */
NIMCP_EXPORT void nimcp_oscillation_state_destroy(nimcp_oscillation_state_t* state);

/**
 * @brief Create phase synchronization state
 *
 * @param ctx GPU context
 * @param n_channels Number of channels
 * @param window_size Window size for PLV
 * @return State on success, NULL on failure
 */
NIMCP_EXPORT nimcp_phase_sync_state_t* nimcp_phase_sync_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_channels,
    size_t window_size
);

/**
 * @brief Destroy phase synchronization state
 *
 * @param state State to destroy
 */
NIMCP_EXPORT void nimcp_phase_sync_state_destroy(nimcp_phase_sync_state_t* state);

//=============================================================================
// Phase Synchronization Operations
//=============================================================================

/**
 * @brief Compute Phase Locking Value (PLV) between channels
 *
 * PLV = |mean(exp(i * (phase1 - phase2)))|
 * Measures phase synchronization strength
 *
 * @param ctx GPU context
 * @param phase1 Phase of first signal (n_samples)
 * @param phase2 Phase of second signal (n_samples)
 * @param plv Output PLV value
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_phase_locking_value(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* phase1,
    const nimcp_gpu_tensor_t* phase2,
    float* plv
);

/**
 * @brief Compute PLV matrix for all channel pairs
 *
 * @param ctx GPU context
 * @param phases Phase tensor (n_channels x n_samples)
 * @param plv_matrix Output PLV matrix (n_channels x n_channels)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_plv_matrix(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* phases,
    nimcp_gpu_tensor_t* plv_matrix
);

/**
 * @brief Compute Phase Lag Index (PLI)
 *
 * PLI = |mean(sign(phase_diff))|
 * More robust to volume conduction than PLV
 *
 * @param ctx GPU context
 * @param phase1 Phase of first signal
 * @param phase2 Phase of second signal
 * @param pli Output PLI value
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_phase_lag_index(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* phase1,
    const nimcp_gpu_tensor_t* phase2,
    float* pli
);

/**
 * @brief Compute global phase synchronization index
 *
 * Uses Kuramoto order parameter: R = |mean(exp(i * phases))|
 *
 * @param ctx GPU context
 * @param phases Phase tensor (n_channels x n_samples)
 * @param sync_index Output synchronization index per time point
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_global_sync_index(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* phases,
    nimcp_gpu_tensor_t* sync_index
);

//=============================================================================
// Cross-Frequency Coupling (Phase-Amplitude Coupling)
//=============================================================================

/**
 * @brief Compute Phase-Amplitude Coupling (PAC) using Modulation Index
 *
 * MI = (H_max - H) / H_max where H is entropy of amplitude distribution
 *
 * @param ctx GPU context
 * @param signal Input signal (n_samples or n_channels x n_samples)
 * @param pac_values Output PAC values
 * @param params PAC parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_pac_modulation_index(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal,
    nimcp_gpu_tensor_t* pac_values,
    const nimcp_pac_params_t* params
);

/**
 * @brief Compute comodulogram (PAC across frequency pairs)
 *
 * @param ctx GPU context
 * @param signal Input signal
 * @param comodulogram Output comodulogram (n_phase_freqs x n_amp_freqs)
 * @param phase_freqs Array of phase frequencies
 * @param n_phase_freqs Number of phase frequencies
 * @param amp_freqs Array of amplitude frequencies
 * @param n_amp_freqs Number of amplitude frequencies
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_pac_comodulogram(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal,
    nimcp_gpu_tensor_t* comodulogram,
    const float* phase_freqs,
    size_t n_phase_freqs,
    const float* amp_freqs,
    size_t n_amp_freqs
);

/**
 * @brief Extract instantaneous phase using Hilbert transform
 *
 * @param ctx GPU context
 * @param signal Input signal (bandpass filtered)
 * @param phase Output instantaneous phase
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_hilbert_phase(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal,
    nimcp_gpu_tensor_t* phase
);

/**
 * @brief Extract instantaneous amplitude (envelope)
 *
 * @param ctx GPU context
 * @param signal Input signal (bandpass filtered)
 * @param amplitude Output instantaneous amplitude
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_hilbert_amplitude(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal,
    nimcp_gpu_tensor_t* amplitude
);

//=============================================================================
// Oscillatory Power Computation (FFT-based)
//=============================================================================

/**
 * @brief Compute band power using FFT
 *
 * @param ctx GPU context
 * @param signal Input signal (n_channels x n_samples)
 * @param power Output power per channel (n_channels)
 * @param params Oscillation parameters (with freq_low, freq_high)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_band_power(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal,
    nimcp_gpu_tensor_t* power,
    const nimcp_oscillation_params_t* params
);

/**
 * @brief Compute power spectral density (PSD)
 *
 * Uses Welch's method with overlapping windows
 *
 * @param ctx GPU context
 * @param signal Input signal
 * @param psd Output PSD (n_channels x n_freqs)
 * @param freqs Output frequency array (n_freqs)
 * @param params Oscillation parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_power_spectral_density(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal,
    nimcp_gpu_tensor_t* psd,
    nimcp_gpu_tensor_t* freqs,
    const nimcp_oscillation_params_t* params
);

/**
 * @brief Compute time-frequency representation (spectrogram)
 *
 * @param ctx GPU context
 * @param signal Input signal (n_samples)
 * @param spectrogram Output spectrogram (n_time x n_freqs)
 * @param params Oscillation parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_spectrogram(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal,
    nimcp_gpu_tensor_t* spectrogram,
    const nimcp_oscillation_params_t* params
);

/**
 * @brief Compute relative band power
 *
 * Normalizes band power by total power
 *
 * @param ctx GPU context
 * @param signal Input signal
 * @param relative_power Output relative power per band
 * @param bands Array of oscillation bands
 * @param n_bands Number of bands
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_relative_band_power(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal,
    nimcp_gpu_tensor_t* relative_power,
    const nimcp_oscillation_band_t* bands,
    size_t n_bands
);

//=============================================================================
// Coherence Computation
//=============================================================================

/**
 * @brief Compute coherence between two signals
 *
 * Coh(f) = |Pxy(f)|^2 / (Pxx(f) * Pyy(f))
 *
 * @param ctx GPU context
 * @param signal1 First signal
 * @param signal2 Second signal
 * @param coherence Output coherence spectrum
 * @param params Coherence parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_coherence(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal1,
    const nimcp_gpu_tensor_t* signal2,
    nimcp_gpu_tensor_t* coherence,
    const nimcp_coherence_params_t* params
);

/**
 * @brief Compute coherence matrix for all channel pairs
 *
 * @param ctx GPU context
 * @param signals Multi-channel signal (n_channels x n_samples)
 * @param coh_matrix Output coherence matrix (n_channels x n_channels)
 * @param params Coherence parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_coherence_matrix(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signals,
    nimcp_gpu_tensor_t* coh_matrix,
    const nimcp_coherence_params_t* params
);

/**
 * @brief Compute imaginary coherence (iCoh)
 *
 * iCoh = Im(Cxy) / sqrt(Cxx * Cyy)
 * Robust to volume conduction artifacts
 *
 * @param ctx GPU context
 * @param signal1 First signal
 * @param signal2 Second signal
 * @param icoh Output imaginary coherence
 * @param params Coherence parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_imaginary_coherence(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal1,
    const nimcp_gpu_tensor_t* signal2,
    nimcp_gpu_tensor_t* icoh,
    const nimcp_coherence_params_t* params
);

/**
 * @brief Compute band-averaged coherence
 *
 * @param ctx GPU context
 * @param signals Multi-channel signal
 * @param band_coherence Output coherence per band
 * @param band Frequency band
 * @param params Coherence parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_band_coherence(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signals,
    nimcp_gpu_tensor_t* band_coherence,
    nimcp_oscillation_band_t band,
    const nimcp_coherence_params_t* params
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Apply bandpass filter (butterworth)
 *
 * @param ctx GPU context
 * @param signal Input signal
 * @param filtered Output filtered signal
 * @param low_freq Low cutoff frequency (Hz)
 * @param high_freq High cutoff frequency (Hz)
 * @param order Filter order
 * @param sampling_rate Sampling rate (Hz)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_bandpass_filter(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal,
    nimcp_gpu_tensor_t* filtered,
    float low_freq,
    float high_freq,
    int order,
    float sampling_rate
);

/**
 * @brief Get frequency band limits
 *
 * @param band Oscillation band
 * @param low_out Output low frequency
 * @param high_out Output high frequency
 */
NIMCP_EXPORT void nimcp_get_band_frequencies(
    nimcp_oscillation_band_t band,
    float* low_out,
    float* high_out
);

/**
 * @brief Get default oscillation parameters
 *
 * @param sampling_rate Sampling rate in Hz
 * @return Default parameters
 */
NIMCP_EXPORT nimcp_oscillation_params_t nimcp_oscillation_default_params(float sampling_rate);

/**
 * @brief Get default PAC parameters
 *
 * @return Default PAC parameters (theta-gamma coupling)
 */
NIMCP_EXPORT nimcp_pac_params_t nimcp_pac_default_params(void);

/**
 * @brief Get default coherence parameters
 *
 * @return Default coherence parameters
 */
NIMCP_EXPORT nimcp_coherence_params_t nimcp_coherence_default_params(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_OSCILLATIONS_GPU_H
